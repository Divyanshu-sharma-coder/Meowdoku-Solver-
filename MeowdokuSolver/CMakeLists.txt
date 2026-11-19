cmake_minimum_required(VERSION 3.21)

project(MeowdokuSolver
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# OpenCV
find_package(OpenCV REQUIRED)

# Application
add_executable(MeowdokuSolver
    src/main.cpp
    src/vision/grid_detector.cpp
    src/vision/region_detector.cpp
    src/vision/cat_detector.cpp
    src/solver/constraint_solver.cpp
    src/solver/move_analyzer.cpp
)
# Header directories
target_include_directories(MeowdokuSolver
    PRIVATE
    include
    ${OpenCV_INCLUDE_DIRS}
)

# OpenCV libraries
target_link_libraries(MeowdokuSolver
    PRIVATE
    ${OpenCV_LIBS}
)