CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
QT_CFLAGS = $(shell pkg-config --cflags Qt5Widgets)
QT_LIBS = $(shell pkg-config --libs Qt5Widgets)
CLI_TARGET = hospital_simulator
GUI_TARGET = hospital_simulator_gui
COMMON_SRCS = service_graph.cpp scheduler.cpp
CLI_SRCS = main.cpp $(COMMON_SRCS)
GUI_SRCS = gui_main.cpp $(COMMON_SRCS)

all: $(CLI_TARGET) $(GUI_TARGET)

$(CLI_TARGET): $(CLI_SRCS)
	$(CXX) $(CXXFLAGS) $(CLI_SRCS) -o $(CLI_TARGET)

$(GUI_TARGET): $(GUI_SRCS)
	$(CXX) $(CXXFLAGS) -fPIC $(QT_CFLAGS) $(GUI_SRCS) -o $(GUI_TARGET) $(QT_LIBS)

clean:
	rm -f $(CLI_TARGET) $(GUI_TARGET)
