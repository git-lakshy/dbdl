#include "scheduler.h"
#include "service_graph.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
void printBootOrder(ServiceGraph& graph, std::ostream& out) {
    std::vector<std::string> order;
    std::vector<std::string> cycle;
    if (graph.topologicalSort(order, cycle)) {
        out << "\nValid Boot Order\n";
        out << "----------------\n";
        for (size_t i = 0; i < order.size(); ++i) {
            out << i + 1 << ". " << order[i] << '\n';
        }
    } else {
        out << "\nCycle detected:\n";
        for (size_t i = 0; i < cycle.size(); ++i) {
            out << cycle[i];
            if (i + 1 < cycle.size()) {
                out << " -> ";
            }
        }
        out << '\n';
    }
}

void runAllDemos(ServiceGraph& normalGraph, ServiceGraph& cycleGraph, PriorityScheduler& scheduler) {
    normalGraph.printServices(std::cout);
    printBootOrder(normalGraph, std::cout);
    normalGraph.simulateBoot(std::cout, "AuthenticationService");

    std::cout << "\nCycle Demo\n";
    std::cout << "----------\n";
    cycleGraph.printServices(std::cout);
    printBootOrder(cycleGraph, std::cout);

    scheduler.printJobs(std::cout);
    scheduler.simulate(std::cout, false);
    scheduler.simulate(std::cout, true);
}

void printMenu() {
    std::cout << "\nHospital Infrastructure Scheduler Simulator\n";
    std::cout << "------------------------------------------\n";
    std::cout << "1. Show normal service dependency graph\n";
    std::cout << "2. Show valid boot order\n";
    std::cout << "3. Simulate critical service failure\n";
    std::cout << "4. Show cycle detection demo\n";
    std::cout << "5. Show bandwidth jobs\n";
    std::cout << "6. Run scheduler without aging\n";
    std::cout << "7. Run scheduler with aging\n";
    std::cout << "8. Run full demo\n";
    std::cout << "0. Exit\n";
    std::cout << "Choose: ";
}
}  // namespace

int main() {
    ServiceGraph normalGraph;
    ServiceGraph cycleGraph;
    PriorityScheduler scheduler;
    std::string error;

    if (!normalGraph.loadFromFile("services.txt", error)) {
        std::cerr << error << '\n';
        return 1;
    }
    if (!cycleGraph.loadFromFile("services_cycle.txt", error)) {
        std::cerr << error << '\n';
        return 1;
    }
    if (!scheduler.loadFromFile("jobs.txt", error)) {
        std::cerr << error << '\n';
        return 1;
    }

    int choice = -1;
    while (choice != 0) {
        printMenu();
        std::cin >> choice;

        switch (choice) {
            case 1:
                normalGraph.printServices(std::cout);
                break;
            case 2:
                printBootOrder(normalGraph, std::cout);
                break;
            case 3:
                normalGraph.simulateBoot(std::cout, "AuthenticationService");
                break;
            case 4:
                cycleGraph.printServices(std::cout);
                printBootOrder(cycleGraph, std::cout);
                break;
            case 5:
                scheduler.printJobs(std::cout);
                break;
            case 6:
                scheduler.simulate(std::cout, false);
                break;
            case 7:
                scheduler.simulate(std::cout, true);
                break;
            case 8:
                runAllDemos(normalGraph, cycleGraph, scheduler);
                break;
            case 0:
                std::cout << "Exiting.\n";
                break;
            default:
                std::cout << "Invalid choice.\n";
                break;
        }
    }

    return 0;
}
