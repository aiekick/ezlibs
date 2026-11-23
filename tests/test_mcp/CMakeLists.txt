cmake_minimum_required(VERSION 3.10)

set(PROJECT EzLibs_TestMcp)
enable_language(C CXX)
project(${PROJECT} CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_definitions(-DNOMINMAX)
endif()

find_package(Threads REQUIRED)

file(GLOB_RECURSE PROJECT_TEST_SRC_RECURSE 
	${CMAKE_CURRENT_SOURCE_DIR}/*.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/*.h)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX Src FILES ${PROJECT_TEST_SRC_RECURSE})

file(GLOB SRC_RECURSE 
	${EZ_LIBS_INCLUDE_DIR}/ezlibs/ezMcp/*.hpp)
source_group(TREE ${EZ_LIBS_INCLUDE_DIR}/ezlibs PREFIX Libs FILES ${SRC_RECURSE})

add_executable(${PROJECT} 
	${SRC_RECURSE}
	${PROJECT_TEST_SRC_RECURSE}
)

set_target_properties(${PROJECT} PROPERTIES FOLDER tests/ezlibs)

target_include_directories(${PROJECT} PRIVATE 
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT} PRIVATE Threads::Threads)
if(WIN32)
	target_link_libraries(${PROJECT} PRIVATE ws2_32)
endif()

StageBuildInc()

##########################################################
## TESTS MCP JSON ########################################
##########################################################

AddTest(TestEzMcpJson_ReadTypes)
AddTest(TestEzMcpJson_RoundTrip)
AddTest(TestEzMcpJson_KeyOrder)
AddTest(TestEzMcpJson_Escapes)
AddTest(TestEzMcpJson_Unicode)
AddTest(TestEzMcpJson_Numbers)
AddTest(TestEzMcpJson_Refusals)
AddTest(TestEzMcpJson_Depth)
AddTest(TestEzMcpJson_WrongTypeAccess)

##########################################################
## TESTS MCP BASE64 ######################################
##########################################################

AddTest(TestEzMcpBase64_Rfc4648)
AddTest(TestEzMcpBase64_Binary)

##########################################################
## TESTS MCP PNG #########################################
##########################################################

AddTest(TestEzMcpPng_Checksums)
AddTest(TestEzMcpPng_Refusals)
AddTest(TestEzMcpPng_OnePixel)
AddTest(TestEzMcpPng_MultiBlock)

##########################################################
## TESTS MCP JSON-RPC ####################################
##########################################################

AddTest(TestEzMcpRpc_Request)
AddTest(TestEzMcpRpc_Notification)
AddTest(TestEzMcpRpc_Response)
AddTest(TestEzMcpRpc_ParseError)
AddTest(TestEzMcpRpc_InvalidRequest)
AddTest(TestEzMcpRpc_Writers)

##########################################################
## TESTS MCP HTTP ########################################
##########################################################

AddTest(TestEzMcpHttp_Complete)
AddTest(TestEzMcpHttp_HeaderCase)
AddTest(TestEzMcpHttp_Incomplete)
AddTest(TestEzMcpHttp_NoBody)
AddTest(TestEzMcpHttp_Invalid)
AddTest(TestEzMcpHttp_Response)

##########################################################
## TESTS MCP SOCKET ######################################
##########################################################

AddTest(TestEzMcpSocket_LoopbackOnly)
AddTest(TestEzMcpSocket_Exchange)
AddTest(TestEzMcpSocket_Timeouts)
AddTest(TestEzMcpSocket_PeerGone)
AddTest(TestEzMcpSocket_NotOpened)
AddTest(TestEzMcpSocket_PortTaken)

##########################################################
## TESTS MCP SERVER ######################################
##########################################################

AddTest(TestEzMcpServer_Initialize)
AddTest(TestEzMcpServer_Ping)
AddTest(TestEzMcpServer_ToolsList)
AddTest(TestEzMcpServer_AddToolRefusals)
AddTest(TestEzMcpServer_Routing)
AddTest(TestEzMcpServer_CallErrors)
AddTest(TestEzMcpServer_CallTimeout)
AddTest(TestEzMcpServer_EndToEnd)
AddTest(TestEzMcpServer_SlowToolOutlivesTimeout)
AddTest(TestEzMcpServer_StopWhileWaiting)
AddTest(TestEzMcpServer_MalformedHttp)
