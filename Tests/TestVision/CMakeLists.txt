cmake_minimum_required(VERSION 3.10)

set(PROJECT EzLibs_TestVision)
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
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezMath/ezMatN.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezMath/ezVecN.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezMath/ezSvd.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezVision/ezEpipolar.hpp)
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
##### TESTS EzEpipolar ###################################
##########################################################

AddTest("TestEzEpipolar_DecomposeRejectsNonSquare<float>")
AddTest("TestEzEpipolar_DecomposeRejectsNonSquare<double>")
AddTest("TestEzEpipolar_DecomposePureXTranslation<float>")
AddTest("TestEzEpipolar_DecomposePureXTranslation<double>")
AddTest("TestEzEpipolar_DecomposeProducesProperRotations<float>")
AddTest("TestEzEpipolar_DecomposeProducesProperRotations<double>")
AddTest("TestEzEpipolar_TriangulateRejectsBadDimensions<float>")
AddTest("TestEzEpipolar_TriangulateRejectsBadDimensions<double>")
AddTest("TestEzEpipolar_TriangulateAxisAlignedCameras<float>")
AddTest("TestEzEpipolar_TriangulateAxisAlignedCameras<double>")
AddTest("TestEzEpipolar_CheiralityHandlesEmptySamples<float>")
AddTest("TestEzEpipolar_CheiralityHandlesEmptySamples<double>")
AddTest("TestEzEpipolar_CheiralitySelectsCorrectPose<float>")
AddTest("TestEzEpipolar_CheiralitySelectsCorrectPose<double>")
