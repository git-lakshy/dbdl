#ifndef SERVICE_GRAPH_H
#define SERVICE_GRAPH_H

#include <string>
#include <iosfwd>
#include <unordered_map>
#include <vector>

struct ServiceNode {
    std::string name;
    bool critical = false;
    std::vector<std::string> dependencies;
};

enum class BootState {
    Started,
    Failed,
    Skipped
};

struct BootStep {
    std::string serviceName;
    BootState state = BootState::Started;
};

struct BootSimulationResult {
    bool hasCycle = false;
    std::vector<std::string> cycleNodes;
    std::vector<BootStep> steps;
};

class ServiceGraph {
public:
    bool loadFromFile(const std::string& path, std::string& error);
    bool topologicalSort(std::vector<std::string>& order, std::vector<std::string>& cycleNodes) const;
    void printServices(std::ostream& out) const;
    void simulateBoot(std::ostream& out, const std::string& failedService = "") const;
    BootSimulationResult runBootSimulation(const std::string& failedService = "") const;
    const std::unordered_map<std::string, ServiceNode>& services() const;

private:
    enum class VisitState { Unvisited, Visiting, Visited };

    bool dfsTopo(
        const std::string& service,
        std::unordered_map<std::string, VisitState>& state,
        std::vector<std::string>& stack,
        std::vector<std::string>& order,
        std::vector<std::string>& cycleNodes
    ) const;

    void markSkippedDependents(
        const std::string& failedService,
        std::unordered_map<std::string, bool>& skipped
    ) const;

    std::unordered_map<std::string, ServiceNode> services_;
};

#endif
