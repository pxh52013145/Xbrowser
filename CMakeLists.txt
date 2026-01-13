cmake_minimum_required(VERSION 3.24)

project(XBrowser VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Gui Qml Quick QuickControls2 Network Test)

qt_standard_project_setup(REQUIRES 6.6)

add_subdirectory(src)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
