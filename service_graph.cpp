#include "service_graph.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
std::string trim(const std::string& input) {
    const std::string whitespace = " \t\r\n";
    const size_t start = input.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    const size_t end = input.find_last_not_of(whitespace);
    return input.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& input, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(input);
    std::string item;
    while (std::getline(stream, item, delimiter)) {
        parts.push_back(trim(item));
    }
    return parts;
}
}  // namespace

bool ServiceGraph::loadFromFile(const std::string& path, std::string& error) {
    services_.clear();

    std::ifstream file(path);
    if (!file) {
        error = "Unable to open file: " + path;
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::vector<std::string> fields = split(line, '|');
        if (fields.size() != 3) {
            error = "Invalid service format at line " + std::to_string(lineNumber);
            return false;
        }

        ServiceNode node;
        node.name = fields[0];
        std::string criticalField = fields[1];
        std::transform(criticalField.begin(), criticalField.end(), criticalField.begin(), ::tolower);
        node.critical = (criticalField == "yes" || criticalField == "true" || criticalField == "1");

        if (fields[2] != "-") {
            node.dependencies = split(fields[2], ',');
        }

        services_[node.name] = node;
    }

    for (const auto& [name, node] : services_) {
        for (const std::string& dependency : node.dependencies) {
            if (!services_.count(dependency)) {
                error = "Service '" + name + "' depends on undefined service '" + dependency + "'";
                return false;
            }
        }
    }

    return true;
}

bool ServiceGraph::dfsTopo(
    const std::string& service,
    std::unordered_map<std::string, VisitState>& state,
    std::vector<std::string>& stack,
    std::vector<std::string>& order,
    std::vector<std::string>& cycleNodes
) const {
    state[service] = VisitState::Visiting;
    stack.push_back(service);

    const auto it = services_.find(service);
    for (const std::string& dependency : it->second.dependencies) {
        if (state[dependency] == VisitState::Unvisited) {
            if (!dfsTopo(dependency, state, stack, order, cycleNodes)) {
                return false;
            }
        } else if (state[dependency] == VisitState::Visiting) {
            auto cycleStart = std::find(stack.begin(), stack.end(), dependency);
            cycleNodes.assign(cycleStart, stack.end());
            cycleNodes.push_back(dependency);
            return false;
        }
    }

    stack.pop_back();
    state[service] = VisitState::Visited;
    order.push_back(service);
    return true;
}

bool ServiceGraph::topologicalSort(std::vector<std::string>& order, std::vector<std::string>& cycleNodes) const {
    order.clear();
    cycleNodes.clear();

    std::unordered_map<std::string, VisitState> state;
    for (const auto& [name, _] : services_) {
        state[name] = VisitState::Unvisited;
    }

    std::vector<std::string> stack;
    for (const auto& [name, _] : services_) {
        if (state[name] == VisitState::Unvisited) {
            if (!dfsTopo(name, state, stack, order, cycleNodes)) {
                return false;
            }
        }
    }

    return true;
}

void ServiceGraph::printServices(std::ostream& out) const {
    out << "\nService Dependency Graph\n";
    out << "------------------------\n";
    for (const auto& [name, node] : services_) {
        out << std::left << std::setw(24) << name
            << " critical=" << (node.critical ? "yes" : "no")
            << " depends_on=";
        if (node.dependencies.empty()) {
            out << "-";
        } else {
            for (size_t i = 0; i < node.dependencies.size(); ++i) {
                out << node.dependencies[i];
                if (i + 1 < node.dependencies.size()) {
                    out << ", ";
                }
            }
        }
        out << '\n';
    }
}

void ServiceGraph::markSkippedDependents(
    const std::string& failedService,
    std::unordered_map<std::string, bool>& skipped
) const {
    std::function<void(const std::string&)> dfs = [&](const std::string& parent) {
        for (const auto& [name, node] : services_) {
            if (std::find(node.dependencies.begin(), node.dependencies.end(), parent) != node.dependencies.end()) {
                if (!skipped[name]) {
                    skipped[name] = true;
                    dfs(name);
                }
            }
        }
    };

    skipped[failedService] = true;
    dfs(failedService);
}

void ServiceGraph::simulateBoot(std::ostream& out, const std::string& failedService) const {
    const BootSimulationResult result = runBootSimulation(failedService);
    if (result.hasCycle) {
        out << "\nBoot aborted: circular dependency detected.\nCycle: ";
        for (size_t i = 0; i < result.cycleNodes.size(); ++i) {
            out << result.cycleNodes[i];
            if (i + 1 < result.cycleNodes.size()) {
                out << " -> ";
            }
        }
        out << "\n";
        return;
    }

    out << "\nBoot Simulation\n";
    out << "---------------\n";
    for (size_t i = 0; i < result.steps.size(); ++i) {
        const BootStep& step = result.steps[i];
        out << std::right << std::setw(2) << i + 1 << ". ";
        out << step.serviceName;
        switch (step.state) {
            case BootState::Failed:
                out << " [FAILED - critical service]\n";
                break;
            case BootState::Skipped:
                out << " [SKIPPED - depends on failed critical service]\n";
                break;
            case BootState::Started:
                out << " [STARTED]\n";
                break;
        }
    }
}

BootSimulationResult ServiceGraph::runBootSimulation(const std::string& failedService) const {
    BootSimulationResult result;
    std::vector<std::string> order;
    if (!topologicalSort(order, result.cycleNodes)) {
        result.hasCycle = true;
        return result;
    }

    std::unordered_map<std::string, bool> skipped;
    if (!failedService.empty() && services_.count(failedService) && services_.at(failedService).critical) {
        markSkippedDependents(failedService, skipped);
    }

    for (const std::string& service : order) {
        BootState state = BootState::Started;
        if (!failedService.empty() && service == failedService) {
            state = BootState::Failed;
        } else if (skipped[service]) {
            state = BootState::Skipped;
        }
        result.steps.push_back({service, state});
    }

    return result;
}

const std::unordered_map<std::string, ServiceNode>& ServiceGraph::services() const {
    return services_;
}
