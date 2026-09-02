cmake_minimum_required(VERSION 3.10)

set(PROJECT EzLibs_TestTTF)
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
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezTTF/ezTTF.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezTTF/ttfTypes.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezTTF/ttfStream.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezTTF/ttfTables.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezTTF/ttfPostNames.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezTTF/ttfGlyph.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezTTF/ttfWriter.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezTTF/ttfFont.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezTTF/ttfBuilder.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezTTF/ttfOutline.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezTTF/ttfSynthesizer.hpp
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezMath/ezBezier.hpp)
source_group(TREE ${EZ_LIBS_INCLUDE_DIR}/ezlibs PREFIX Libs FILES ${EZ_LIBS_SOURCE})

add_executable(${PROJECT}
	${EZ_LIBS_SOURCE}
	${PROJECT_TEST_SRC_RECURSE}
)

set_target_properties(${PROJECT} PROPERTIES FOLDER tests/ezlibs)

target_include_directories(${PROJECT} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)

# a REAL font for the local validation : the ogi vendored roboto when
# ezlibs lives inside the ogiFontStudio tree. absent (standalone ezlibs,
# the linux ci) the test skips itself honestly
target_compile_definitions(${PROJECT} PRIVATE
	EZTTF_LOCAL_TTF="${CMAKE_CURRENT_SOURCE_DIR}/../../../ogi/assets/fonts/Roboto_Medium.ttf")

# the COMMITTED fixture (apache 2.0, see res/NOTICE.txt) : a roboto
# subset the builder bootstraps on a dev machine, then serves every ci
target_compile_definitions(${PROJECT} PRIVATE
	EZTTF_RES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/res/")

StageBuildInc()

##########################################################
##### TESTS EzTtfTypes ###################################
##########################################################

AddTest("TestEzTtfTypes_TagComposeAndBack")
AddTest("TestEzTtfTypes_FixedRoundTrip")
AddTest("TestEzTtfTypes_F2Dot14SpecValues")
AddTest("TestEzTtfTypes_F2Dot14SetFloatSaturates")

##########################################################
##### TESTS EzTtfStream ##################################
##########################################################

AddTest("TestEzTtfStream_WritesBigEndianBytes")
AddTest("TestEzTtfStream_RoundTripsEveryType")
AddTest("TestEzTtfStream_OutOfBoundsReadLatchesTheError")
AddTest("TestEzTtfStream_EmptyStreamRefusesEveryRead")
AddTest("TestEzTtfStream_SeekPastEndLatchesTheError")
AddTest("TestEzTtfStream_ReadBytesIsAllOrNothing")
AddTest("TestEzTtfStream_PadToLongAligns")
AddTest("TestEzTtfStream_AppendStreamAndString")
AddTest("TestEzTtfStream_AnEmptyReadAtTheEndIsLegal")

##########################################################
##### TESTS EzTtfTables ##################################
##########################################################

AddTest("TestEzTtfTables_ParsesAValidDirectory")
AddTest("TestEzTtfTables_FindsATableByTag")
AddTest("TestEzTtfTables_RefusesTheOttoFlavour")
AddTest("TestEzTtfTables_RefusesATruncatedDirectory")
AddTest("TestEzTtfTables_RefusesARecordPastTheEnd")
AddTest("TestEzTtfTables_ParsesTheHeadTable")
AddTest("TestEzTtfTables_RefusesABadHeadTable")
AddTest("TestEzTtfTables_ParsesTheMaxpTable")
AddTest("TestEzTtfTables_ParsesTheHheaTable")
AddTest("TestEzTtfTables_ParsesAndResolvesTheHmtxTable")
AddTest("TestEzTtfTables_ParsesBothLocaFlavours")
AddTest("TestEzTtfTables_ParsesACmapFormat4")
AddTest("TestEzTtfTables_PrefersFormat12AndReadsBeyondTheBmp")
AddTest("TestEzTtfTables_RefusesOrBoundsHostileCmaps")
AddTest("TestEzTtfFont_OpensACompleteSyntheticFont")
AddTest("TestEzTtfFont_RefusesAFontMissingARequiredTable")
AddTest("TestEzTtfFont_ReadsASimpleGlyphOutline")
AddTest("TestEzTtfFont_ReadsACompositeEntry")
AddTest("TestEzTtfFont_OpensFromAFile")
AddTest("TestEzTtfTables_ParsesTheNameTable")
AddTest("TestEzTtfTables_ParsesThePostTable")
AddTest("TestEzTtfFont_WriteRoundTripsAndReachesAFixedPoint")
AddTest("TestEzTtfBuilder_SubsetEmbarksTheCompositeClosure")
AddTest("TestEzTtfBuilder_MergesTwoSourcesLastPickWins")
AddTest("TestEzTtfBuilder_ColorLayersReadAndRoundTrip")
AddTest("TestEzTtfBuilder_ValidatesAgainstARealFont")
AddTest("TestEzTtfBuilder_OpensTheCommittedFixture")
AddTest("TestEzTtfBuilder_SubsetEmbarksTheColorLayers")
AddTest("TestEzTtfBuilder_MergeConcatenatesThePalettes")
AddTest("TestEzTtfBuilder_PlainPicksEmitNoColorTables")
AddTest("TestEzTtfBuilder_BuildTellsTheNewIndexOfEveryPick")

##########################################################
##### TESTS EzTtfGlyph (the emitter) #####################
##########################################################

AddTest("TestEzTtfGlyph_EmitParseRoundTripsASimpleGlyph")
AddTest("TestEzTtfGlyph_EmitParseRoundTripsAComposite")
AddTest("TestEzTtfGlyph_AnEmptyGlyphEmitsNothing")
AddTest("TestEzTtfGlyph_RoundTripsEveryGlyphOfARealFont")

##########################################################
##### TESTS EzTtfOutline (the pen) #######################
##########################################################

AddTest("TestEzTtfOutline_ASquareIsOneClosedContour")
AddTest("TestEzTtfOutline_ACircleOfCubicsBecomesQuadratics")
AddTest("TestEzTtfOutline_TheTransformFlipsAndScales")
AddTest("TestEzTtfOutline_DuplicateClosingPointIsDropped")
AddTest("TestEzTtfOutline_QuantizationRoundsAndClamps")

##########################################################
##### TESTS EzTtfSynthesizer #############################
##########################################################

AddTest("TestEzTtfSynthesizer_BuildsAFontThatReopens")
AddTest("TestEzTtfSynthesizer_ColorLayersRoundTrip")
AddTest("TestEzTtfSynthesizer_MergesThroughTheBuilder")
AddTest("TestEzTtfSynthesizer_RefusesUnknownLayers")
AddTest("TestEzTtfSynthesizer_APlainGlyphBeforeTheColoredOne")
