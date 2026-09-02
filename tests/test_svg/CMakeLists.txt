cmake_minimum_required(VERSION 3.10)

set(PROJECT EzLibs_TestSVG)
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

file(GLOB EZ_LIBS_SOURCE
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezSvg/ezSvg.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezSvg/svgTypes.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezSvg/svgScanner.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezSvg/svgStyle.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezSvg/svgColor.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezSvg/svgTransform.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezSvg/svgPathParser.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezSvg/svgReader.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezSvg/svgWriter.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezMath/ezBezier.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezXml.hpp)
source_group(TREE ${EZ_LIBS_INCLUDE_DIR}/ezlibs PREFIX Libs FILES ${EZ_LIBS_SOURCE})

add_executable(${PROJECT}
	${EZ_LIBS_SOURCE}
	${PROJECT_TEST_SRC_RECURSE}
)

set_target_properties(${PROJECT} PROPERTIES FOLDER tests/ezlibs)

target_include_directories(${PROJECT} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)

StageBuildInc()

##########################################################
##### TESTS EzSvgPathParser ##############################
##########################################################

AddTest("TestEzSvgPathParser_ASquareIsThreeLinesAndAClose")
AddTest("TestEzSvgPathParser_RelativeAndGluedNumbers")
AddTest("TestEzSvgPathParser_ImplicitLinetoAfterMoveto")
AddTest("TestEzSvgPathParser_SmoothCurvesReflectTheControl")
AddTest("TestEzSvgPathParser_QuadraticsAreElevated")
AddTest("TestEzSvgPathParser_ArcsBecomeCubics")
AddTest("TestEzSvgPathParser_GluedArcFlags")
AddTest("TestEzSvgPathParser_ClosingReturnsToTheStart")
AddTest("TestEzSvgPathParser_RefusesABrokenPath")

##########################################################
##### TESTS EzSvgTransform ###############################
##########################################################

AddTest("TestEzSvgTransform_TranslateMovesAPoint")
AddTest("TestEzSvgTransform_RotateAroundACenter")
AddTest("TestEzSvgTransform_AListAppliesRightToLeft")
AddTest("TestEzSvgTransform_MatrixAndSkew")
AddTest("TestEzSvgTransform_RefusesGarbage")

##########################################################
##### TESTS EzSvgColor ###################################
##########################################################

AddTest("TestEzSvgColor_HexForms")
AddTest("TestEzSvgColor_FunctionalForms")
AddTest("TestEzSvgColor_KeywordsAndNames")
AddTest("TestEzSvgColor_UnsupportedPaintKeepsItsReference")
AddTest("TestEzSvgColor_RefusesNonsense")

##########################################################
##### TESTS EzSvgReader ##################################
##########################################################

AddTest("TestEzSvgReader_ASquarePathIsOneShape")
AddTest("TestEzSvgReader_ACircleBecomesCubics")
AddTest("TestEzSvgReader_GroupTransformsCompose")
AddTest("TestEzSvgReader_GroupFillIsInherited")
AddTest("TestEzSvgReader_StyleWinsOverTheAttribute")
AddTest("TestEzSvgReader_StrokeOnlyShapesAreCountedNotKept")
AddTest("TestEzSvgReader_HiddenElementsAreSkipped")
AddTest("TestEzSvgReader_UnsupportedElementsAreCounted")
AddTest("TestEzSvgReader_PrologueDoctypeAndCommentsPass")
AddTest("TestEzSvgReader_LengthsWithUnits")
AddTest("TestEzSvgReader_RoundedRectHasCorners")
AddTest("TestEzSvgReader_PolygonAndPolyline")
AddTest("TestEzSvgReader_RefusesADocumentWithoutSvgRoot")
AddTest("TestEzSvgReader_UnsupportedFillKeepsTheShape")
AddTest("TestEzSvgReader_NamespacePrefixesAreStripped")

##########################################################
##### TESTS EzSvgWriter ##################################
##########################################################

AddTest("TestEzSvgWriter_RoundTripsThroughTheReader")
