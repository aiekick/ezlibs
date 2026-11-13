cmake_minimum_required(VERSION 3.10)

# the plugin the tests of ezPlugin load : a shared library exporting the two
# symbols of the loader, built beside the suite that loads it
set(PROJECT EzLibs_TestPluginFixture)
project(${PROJECT} CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(${PROJECT} SHARED
	${CMAKE_CURRENT_SOURCE_DIR}/ezPluginFixture.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ezPluginFixture.h
)

set_target_properties(${PROJECT} PROPERTIES FOLDER tests/ezlibs)

target_include_directories(${PROJECT} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
