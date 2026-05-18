#include "scheduler.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
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

bool containsToken(const std::string& text, const std::vector<std::string>& keywords) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const std::string& keyword : keywords) {
        if (lower.find(keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

int adjustedPriorityForPolicy(const Job& job, TimeOfDayPolicy policy) {
    int adjusted = job.priority;
    if (policy == TimeOfDayPolicy::PeakHours) {
        if (containsToken(job.name, {"telemedicine", "alert", "live", "surgery", "mri"})) {
            adjusted = std::max(1, adjusted - 2);
        }
        if (containsToken(job.name, {"backup", "archive", "sync"})) {
            adjusted += 2;
        }
    } else if (policy == TimeOfDayPolicy::NightShift) {
        if (containsToken(job.name, {"backup", "archive", "sync"})) {
            adjusted = std::max(1, adjusted - 3);
        }
        if (containsToken(job.name, {"telemedicine", "alert", "live", "surgery"})) {
            adjusted += 1;
        }
    }
    return adjusted;
}

std::string policyName(TimeOfDayPolicy policy) {
    switch (policy) {
        case TimeOfDayPolicy::PeakHours:
            return "Peak Hours";
        case TimeOfDayPolicy::NightShift:
            return "Night Shift";
        case TimeOfDayPolicy::Standard:
        default:
            return "Standard";
    }
}
}  // namespace

bool PriorityScheduler::loadFromFile(const std::string& path, std::string& error) {
    jobs_.clear();

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
        if (fields.size() != 4) {
            error = "Invalid job format at line " + std::to_string(lineNumber);
            return false;
        }

        Job job;
        job.name = fields[0];
        job.arrivalTime = std::stoi(fields[1]);
        job.burstTime = std::stoi(fields[2]);
        job.priority = std::stoi(fields[3]);
        jobs_.push_back(job);
    }

    return true;
}

void PriorityScheduler::printJobs(std::ostream& out) const {
    out << "\nBandwidth Jobs\n";
    out << "--------------\n";
    out << std::left << std::setw(28) << "Job"
        << std::setw(10) << "Arrival"
        << std::setw(10) << "Burst"
        << std::setw(10) << "Priority" << '\n';

    for (const Job& job : jobs_) {
        out << std::left << std::setw(28) << job.name
            << std::setw(10) << job.arrivalTime
            << std::setw(10) << job.burstTime
            << std::setw(10) << job.priority << '\n';
    }
}

SimulationResult PriorityScheduler::runSimulation(bool useAging, TimeOfDayPolicy policy) const {
    std::vector<RuntimeJob> runtime;
    runtime.reserve(jobs_.size());
    std::vector<int> originalPriorities;
    originalPriorities.reserve(jobs_.size());
    for (const Job& job : jobs_) {
        originalPriorities.push_back(job.priority);
        Job adjustedJob = job;
        adjustedJob.priority = adjustedPriorityForPolicy(job, policy);
        runtime.push_back({adjustedJob, adjustedJob.burstTime, adjustedJob.priority, 0, 0, -1, -1});
    }

    SimulationResult result;
    result.useAging = useAging;
    result.policy = policy;

    int currentTime = 0;
    int finished = 0;
    int previousJob = -1;

    while (finished < static_cast<int>(runtime.size())) {
        std::vector<int> ready;
        for (size_t i = 0; i < runtime.size(); ++i) {
            if (runtime[i].base.arrivalTime <= currentTime && runtime[i].remainingTime > 0) {
                ready.push_back(static_cast<int>(i));
            }
        }

        if (ready.empty()) {
            result.logLines.push_back("t=" + std::to_string(currentTime) + " CPU idle");
            if (!result.slices.empty() && result.slices.back().idle) {
                result.slices.back().duration++;
            } else {
                result.slices.push_back({"IDLE", currentTime, 1, 0, true});
            }
            ++currentTime;
            previousJob = -1;
            continue;
        }

        if (useAging) {
            for (int index : ready) {
                runtime[index].dynamicPriority = runtime[index].base.priority - runtime[index].ageCounter;
            }
        } else {
            for (int index : ready) {
                runtime[index].dynamicPriority = runtime[index].base.priority;
            }
        }

        int selected = ready[0];
        for (int index : ready) {
            const RuntimeJob& candidate = runtime[index];
            const RuntimeJob& chosen = runtime[selected];
            if (candidate.dynamicPriority < chosen.dynamicPriority ||
                (candidate.dynamicPriority == chosen.dynamicPriority && candidate.waitTime > chosen.waitTime) ||
                (candidate.dynamicPriority == chosen.dynamicPriority && candidate.waitTime == chosen.waitTime &&
                 candidate.base.arrivalTime < chosen.base.arrivalTime)) {
                selected = index;
            }
        }

        if (selected != previousJob) {
            if (previousJob == -1) {
                result.logLines.push_back("t=" + std::to_string(currentTime) + " Context switch -> " + runtime[selected].base.name);
            } else {
                result.logLines.push_back(
                    "t=" + std::to_string(currentTime) + " Context switch " + runtime[previousJob].base.name +
                    " -> " + runtime[selected].base.name
                );
            }
        }

        {
            std::ostringstream line;
            line << "t=" << std::setw(2) << currentTime
                 << " Running " << std::left << std::setw(24) << runtime[selected].base.name
                 << " remaining=" << std::setw(3) << runtime[selected].remainingTime
                 << " effective_priority=" << runtime[selected].dynamicPriority;
            result.logLines.push_back(line.str());
        }

        if (!result.slices.empty() && !result.slices.back().idle &&
            result.slices.back().jobName == runtime[selected].base.name &&
            result.slices.back().effectivePriority == runtime[selected].dynamicPriority &&
            result.slices.back().startTime + result.slices.back().duration == currentTime) {
            result.slices.back().duration++;
        } else {
            result.slices.push_back(
                {runtime[selected].base.name, currentTime, 1, runtime[selected].dynamicPriority, false}
            );
        }

        if (runtime[selected].firstStartTime == -1) {
            runtime[selected].firstStartTime = currentTime;
        }
        runtime[selected].remainingTime--;
        runtime[selected].ageCounter = 0;

        for (int index : ready) {
            if (index != selected) {
                runtime[index].waitTime++;
                runtime[index].ageCounter++;
            }
        }

        if (runtime[selected].remainingTime == 0) {
            runtime[selected].completionTime = currentTime + 1;
            ++finished;
        }

        previousJob = selected;
        ++currentTime;
    }

    for (size_t i = 0; i < runtime.size(); ++i) {
        const RuntimeJob& job = runtime[i];
        const int turnaround = job.completionTime - job.base.arrivalTime;
        const int firstRunWait = job.firstStartTime - job.base.arrivalTime;
        const bool starved = firstRunWait > job.base.burstTime;
        result.summaries.push_back({
            job.base.name,
            job.base.arrivalTime,
            job.base.burstTime,
            originalPriorities[i],
            job.base.priority,
            firstRunWait,
            job.waitTime,
            turnaround,
            starved
        });
    }

    int worstWait = std::numeric_limits<int>::min();
    std::string worstJob;
    for (const RuntimeJob& job : runtime) {
        if (job.waitTime > worstWait) {
            worstWait = job.waitTime;
            worstJob = job.base.name;
        }
    }

    result.longestWaitJob = worstJob;
    result.longestWait = worstWait;
    result.totalTime = currentTime;
    return result;
}

void PriorityScheduler::simulate(std::ostream& out, bool useAging) const {
    const SimulationResult result = runSimulation(useAging);
    out << "\nScheduling Mode: " << (useAging ? "Preemptive Priority with Aging" : "Preemptive Priority without Aging") << '\n';
    out << "Policy: " << policyName(result.policy) << '\n';
    out << "--------------------------------------------------------------------------------\n";
    for (const std::string& line : result.logLines) {
        out << line << '\n';
    }

    out << "\nSummary\n";
    out << "-------\n";
    out << std::left << std::setw(28) << "Job"
        << std::setw(10) << "BasePri"
        << std::setw(12) << "PolicyPri"
        << std::setw(14) << "FirstRunWait"
        << std::setw(12) << "TotalWait"
        << std::setw(14) << "Turnaround"
        << std::setw(12) << "Starved?" << '\n';
    for (const JobSummary& summary : result.summaries) {
        out << std::left << std::setw(28) << summary.jobName
            << std::setw(10) << summary.basePriority
            << std::setw(12) << summary.adjustedPriority
            << std::setw(14) << summary.firstRunWait
            << std::setw(12) << summary.totalWait
            << std::setw(14) << summary.turnaround
            << std::setw(12) << (summary.starved ? "yes" : "no") << '\n';
    }

    out << "\nLongest wait: " << result.longestWaitJob << " (" << result.longestWait << " time units)\n";
}

const std::vector<Job>& PriorityScheduler::jobs() const {
    return jobs_;
}
