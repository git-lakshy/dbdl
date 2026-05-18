cmake_minimum_required(VERSION 3.16)

project(HospitalInfrastructureSchedulerSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(COMMON_SOURCES
    service_graph.cpp
    scheduler.cpp
)

add_executable(hospital_simulator
    main.cpp
    ${COMMON_SOURCES}
)

find_package(Qt5 COMPONENTS Widgets QUIET)
if(Qt5_FOUND)
    add_executable(hospital_simulator_gui
        gui_main.cpp
        ${COMMON_SOURCES}
    )
    target_link_libraries(hospital_simulator_gui PRIVATE Qt5::Widgets)
else()
    find_package(Qt6 COMPONENTS Widgets REQUIRED)
    add_executable(hospital_simulator_gui
        gui_main.cpp
        ${COMMON_SOURCES}
    )
    target_link_libraries(hospital_simulator_gui PRIVATE Qt6::Widgets)
endif()
