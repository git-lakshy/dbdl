# dbdl — Hospital Infrastructure Scheduler

C++17 simulator for hospital service boot order and job scheduling. Built as a systems coursework demo with a CLI and an optional Qt GUI.

## What it models

**Service boot graph**
- Loads services and dependencies from `services.txt`
- DFS topological sort for a valid start order
- Cycle detection (sample graph in `services_cycle.txt`)
- Critical-service failure handling

**Bandwidth / job scheduler**
- Preemptive priority scheduling
- Context-switch logging
- Starvation demo plus priority aging

**Frontends**
- Menu-driven CLI (`hospital_simulator`)
- Qt Widgets GUI (`hospital_simulator_gui`) for walkthrough demos

## Requirements

- `g++` with C++17
- `make`
- Optional: CMake 3.16+
- Optional GUI: Qt5 or Qt6 Widgets (`pkg-config` for Qt5 on Linux)

## Build

```bash
make
```

CLI binary: `./hospital_simulator`  
GUI binary: `./hospital_simulator_gui`

CMake alternative:

```bash
cmake -S . -B build
cmake --build build
```

On Windows, prefer the CMake path and set `CMAKE_PREFIX_PATH` to your Qt install if CMake cannot find Qt.

## Config

**Services** (`services.txt`):

```text
service_name|critical(yes/no)|dependency1,dependency2 or -
```

**Jobs** (`jobs.txt`):

```text
job_name|arrival_time|burst_time|priority
```

Lower priority number means higher priority.

## Suggested GUI flow

1. Show Service Graph  
2. Show Boot Order  
3. Critical Failure Demo  
4. Cycle Detection Demo  
5. Run Without Aging / Run With Aging  
6. Peak Hours Policy / Night Shift Policy  
