# Hospital Infrastructure Scheduler Simulator

This C++ project maps the given problem statement to a hospital-themed simulator.

## Requirements

### Linux

- `g++` with C++17 support
- `make` for the provided `Makefile`
- `cmake` if you want the cross-platform build path
- Qt5 or Qt6 Widgets development libraries for the GUI

The Linux `Makefile` uses `pkg-config` to locate Qt5:

```bash
pkg-config --cflags --libs Qt5Widgets
```

### Windows

- CMake 3.16 or newer
- Qt5 or Qt6 with Widgets support
- a configured compiler:
  - MinGW, or
  - MSVC

## Modules

1. Boot sequencer
   - DFS-based topological sort
   - cycle detection with explicit cycle output
   - 15+ hospital services loaded from config
   - bonus critical-service failure handling

2. Bandwidth scheduler
   - preemptive priority scheduling
   - context switch log
   - starvation demonstration
   - priority aging fix

3. Demo frontends
   - menu-driven CLI
   - simple Qt GUI for presentation

## Build

From the project folder:

```bash
cd /home/jizzlord/Desktop/sumshi
make
```

### Cross-platform CMake Build

```bash
cd /home/jizzlord/Desktop/sumshi
cmake -S . -B build
cmake --build build
```

## Start Commands

### Start CLI

```bash
cd /home/jizzlord/Desktop/sumshi
make
./hospital_simulator
```

### Start CLI with CMake

```bash
cd /home/jizzlord/Desktop/sumshi
cmake -S . -B build
cmake --build build
./build/hospital_simulator
```

### Start GUI

```bash
cd /home/jizzlord/Desktop/sumshi
make
./hospital_simulator_gui
```

## Windows GUI Startup

Use the CMake build path on Windows.

### Requirements on Windows

- CMake 3.16+
- Qt5 or Qt6 with Widgets support
- a configured compiler:
  - MinGW, or
  - MSVC

### Steps

1. Install Qt and CMake.
2. Open a Developer Command Prompt, PowerShell, or terminal with your compiler available.
3. Go to the project folder.
4. Configure CMake:

```powershell
cd path\to\sumshi
cmake -S . -B build
```

5. Build:

```powershell
cmake --build build --config Release
```

6. Run the GUI:

For MinGW or single-config generators:

```powershell
.\build\hospital_simulator_gui.exe
```

For Visual Studio generators:

```powershell
.\build\Release\hospital_simulator_gui.exe
```

### Practical note

If CMake cannot find Qt automatically, provide the Qt install path:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.5.0\mingw_64"
```

or for Qt5:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\5.15.2\mingw81_64"
```

## Demo Buttons

Recommended GUI flow:

1. `Show Service Graph`
2. `Show Boot Order`
3. `Critical Failure Demo`
4. `Cycle Detection Demo`
5. `Run Without Aging`
6. `Run With Aging`
7. `Peak Hours Policy`
8. `Night Shift Policy`

## Config format

### Services

```text
service_name|critical(yes/no)|dependency1,dependency2 or -
```

### Jobs

```text
job_name|arrival_time|burst_time|priority
```

Lower number means higher priority.

## Files To Push To GitHub

Push these source and config files:

- `README.md`
- `Makefile`
- `CMakeLists.txt`
- `main.cpp`
- `gui_main.cpp`
- `service_graph.h`
- `service_graph.cpp`
- `scheduler.h`
- `scheduler.cpp`
- `services.txt`
- `services_cycle.txt`
- `jobs.txt`

Do not push build outputs:

- `hospital_simulator`
- `hospital_simulator_gui`

If you add a `.gitignore`, include at least:

```gitignore
hospital_simulator
hospital_simulator_gui
```
