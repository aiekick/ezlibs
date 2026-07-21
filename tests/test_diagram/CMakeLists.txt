cmake_minimum_required(VERSION 3.10)

set(PROJECT EzLibs_TestDiagram)
enable_language(C CXX)
project(${PROJECT} CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_definitions(-DNOMINMAX)
endif()

file(GLOB_RECURSE PROJECT_TEST_SRC_RECURSE
	${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/*.h)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX Src FILES ${PROJECT_TEST_SRC_RECURSE})

file(GLOB SRC_RECURSE
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezDiagram/ezBilayerCrossings.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezDiagram/ezSeparation1D.hpp)
source_group(TREE ${EZ_LIBS_INCLUDE_DIR}/ezlibs PREFIX Libs FILES ${SRC_RECURSE})

add_executable(${PROJECT}
	${SRC_RECURSE}
	${PROJECT_TEST_SRC_RECURSE}
)

set_target_properties(${PROJECT} PROPERTIES FOLDER Tests)

target_include_directories(${PROJECT} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)

StageBuildInc()

##########################################################
## TESTS EzBilayerCrossings ##############################
##########################################################

AddTest("TestEzBilayerCrossings_Empty")
AddTest("TestEzBilayerCrossings_SingleEdge")
AddTest("TestEzBilayerCrossings_ParallelNoCross")
AddTest("TestEzBilayerCrossings_K22Nested")
AddTest("TestEzBilayerCrossings_K22Crossed")
AddTest("TestEzBilayerCrossings_FullyReversed")
AddTest("TestEzBilayerCrossings_MultiEdgePerVertex")

##########################################################
## TESTS EzSeparation1D ##################################
##########################################################

AddTest("TestEzSeparation1D_Empty")
AddTest("TestEzSeparation1D_Single")
AddTest("TestEzSeparation1D_TwoAlreadySeparated")
AddTest("TestEzSeparation1D_TwoOverlapForcedApart")
AddTest("TestEzSeparation1D_WeightedGoal")
AddTest("TestEzSeparation1D_OrderPreservedFromInverted")
AddTest("TestEzSeparation1D_ThreeCollapsedCentered")
AddTest("TestEzSeparation1D_ThreeMonotoneUntouched")
