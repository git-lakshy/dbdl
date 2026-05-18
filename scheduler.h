#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <iosfwd>
#include <string>
#include <vector>

struct Job {
    std::string name;
    int arrivalTime = 0;
    int burstTime = 0;
    int priority = 0;
};

struct SchedulingSlice {
    std::string jobName;
    int startTime = 0;
    int duration = 0;
    int effectivePriority = 0;
    bool idle = false;
};

struct JobSummary {
    std::string jobName;
    int arrivalTime = 0;
    int burstTime = 0;
    int basePriority = 0;
    int adjustedPriority = 0;
    int firstRunWait = 0;
    int totalWait = 0;
    int turnaround = 0;
    bool starved = false;
};

enum class TimeOfDayPolicy {
    Standard,
    PeakHours,
    NightShift
};

struct SimulationResult {
    bool useAging = false;
    TimeOfDayPolicy policy = TimeOfDayPolicy::Standard;
    int totalTime = 0;
    std::vector<SchedulingSlice> slices;
    std::vector<JobSummary> summaries;
    std::vector<std::string> logLines;
    std::string longestWaitJob;
    int longestWait = 0;
};

class PriorityScheduler {
public:
    bool loadFromFile(const std::string& path, std::string& error);
    void printJobs(std::ostream& out) const;
    void simulate(std::ostream& out, bool useAging) const;
    SimulationResult runSimulation(bool useAging, TimeOfDayPolicy policy = TimeOfDayPolicy::Standard) const;
    const std::vector<Job>& jobs() const;

private:
    struct RuntimeJob {
        Job base;
        int remainingTime = 0;
        int dynamicPriority = 0;
        int waitTime = 0;
        int ageCounter = 0;
        int firstStartTime = -1;
        int completionTime = -1;
    };

    std::vector<Job> jobs_;
};

#endif
