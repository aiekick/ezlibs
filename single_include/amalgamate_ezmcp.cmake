# The amalgamation of ezMcp : one self-contained header, single_include/ezMcp.hpp,
# made from include/ezlibs/ezMcp/ezMcp.hpp by inlining every local include
# ("..." includes) once, in dependency order. ezOS.hpp, the platform defines,
# comes FIRST : they hold for the whole file, whatever the order of the
# sources. The system includes stay as they are.
#
#   cmake -DEZ_MCP_MODE=write -P single_include/amalgamate_ezmcp.cmake   writes it
#   cmake -DEZ_MCP_MODE=check -P single_include/amalgamate_ezmcp.cmake   fails when it is stale
#
# A CMake script and not a python one : cmake is there wherever ezLibs builds.

cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED EZ_MCP_MODE)
    set(EZ_MCP_MODE "write")
endif()
if(NOT EZ_MCP_MODE STREQUAL "write" AND NOT EZ_MCP_MODE STREQUAL "check")
    message(FATAL_ERROR "EZ_MCP_MODE is write or check, not ${EZ_MCP_MODE}")
endif()

get_filename_component(EZ_MCP_INCLUDE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../include/ezlibs" ABSOLUTE)
set(EZ_MCP_ENTRY "${EZ_MCP_INCLUDE_ROOT}/ezMcp/ezMcp.hpp")
set(EZ_MCP_OUTPUT "${CMAKE_CURRENT_LIST_DIR}/ezMcp.hpp")
set_property(GLOBAL PROPERTY EZ_MCP_SEEN "")
set_property(GLOBAL PROPERTY EZ_MCP_LICENSE "")

# The content of aPath with its local includes inlined, recursively, each
# file once. The license block and the pragma once of every file go : the
# output carries them once, at its top.
function(ez_mcp_expand aPath aOutVar)
    get_filename_component(absPath "${aPath}" ABSOLUTE)
    get_filename_component(dir "${absPath}" DIRECTORY)
    file(RELATIVE_PATH relPath "${EZ_MCP_INCLUDE_ROOT}" "${absPath}")
    file(READ "${absPath}" content)
    string(REPLACE "\r\n" "\n" content "${content}")
    string(REGEX MATCH "/\\*\nMIT License.*SOFTWARE\\.\n\\*/\n" license "${content}")
    get_property(knownLicense GLOBAL PROPERTY EZ_MCP_LICENSE)
    if(knownLicense STREQUAL "" AND NOT license STREQUAL "")
        set_property(GLOBAL PROPERTY EZ_MCP_LICENSE "${license}")
    endif()
    if(NOT license STREQUAL "")
        string(REPLACE "${license}" "" content "${content}")
    endif()
    string(REPLACE "#pragma once\n" "" content "${content}")
    string(REGEX MATCHALL "#include \"[^\"]+\"" includes "${content}")
    foreach(includeLine IN LISTS includes)
        string(REGEX REPLACE "#include \"([^\"]+)\"" "\\1" includeName "${includeLine}")
        get_filename_component(childPath "${dir}/${includeName}" ABSOLUTE)
        if(NOT EXISTS "${childPath}")
            message(FATAL_ERROR "${relPath} includes ${includeName}, which does not exist")
        endif()
        get_property(seen GLOBAL PROPERTY EZ_MCP_SEEN)
        list(FIND seen "${childPath}" seenIdx)
        if(seenIdx EQUAL -1)
            set_property(GLOBAL APPEND PROPERTY EZ_MCP_SEEN "${childPath}")
            ez_mcp_expand("${childPath}" childContent)
            file(RELATIVE_PATH childRel "${EZ_MCP_INCLUDE_ROOT}" "${childPath}")
            string(REPLACE "${includeLine}" "// ---- begin ${childRel} ----\n${childContent}// ---- end ${childRel} ----" content "${content}")
        else()
            string(REPLACE "${includeLine}" "// ${includeName} : inlined above" content "${content}")
        endif()
    endforeach()
    set(${aOutVar} "${content}" PARENT_SCOPE)
endfunction()

# the platform defines first, then the entry point
set(EZ_MCP_PRELUDE "${EZ_MCP_INCLUDE_ROOT}/ezOS.hpp")
set_property(GLOBAL APPEND PROPERTY EZ_MCP_SEEN "${EZ_MCP_PRELUDE}")
ez_mcp_expand("${EZ_MCP_PRELUDE}" prelude)
set_property(GLOBAL APPEND PROPERTY EZ_MCP_SEEN "${EZ_MCP_ENTRY}")
ez_mcp_expand("${EZ_MCP_ENTRY}" body)
get_property(license GLOBAL PROPERTY EZ_MCP_LICENSE)
set(generated "#pragma once\n\n${license}\n// ezMcp, amalgamated : ONE header, everything ezMcp needs. Generated from\n// include/ezlibs/ezMcp/ by single_include/amalgamate_ezmcp.cmake - never\n// edit it by hand, edit the sources and run the script again.\n// ---- begin ezOS.hpp ----\n${prelude}// ---- end ezOS.hpp ----\n// ---- begin ezMcp/ezMcp.hpp ----\n${body}// ---- end ezMcp/ezMcp.hpp ----\n")

if(EZ_MCP_MODE STREQUAL "write")
    file(WRITE "${EZ_MCP_OUTPUT}" "${generated}")
    message(STATUS "written : ${EZ_MCP_OUTPUT}")
else()
    if(NOT EXISTS "${EZ_MCP_OUTPUT}")
        message(FATAL_ERROR "single_include/ezMcp.hpp is missing : run cmake -DEZ_MCP_MODE=write -P single_include/amalgamate_ezmcp.cmake")
    endif()
    file(READ "${EZ_MCP_OUTPUT}" committed)
    string(REPLACE "\r\n" "\n" committed "${committed}")
    if(NOT committed STREQUAL generated)
        message(FATAL_ERROR "single_include/ezMcp.hpp is stale against include/ezlibs/ezMcp/ : run cmake -DEZ_MCP_MODE=write -P single_include/amalgamate_ezmcp.cmake")
    endif()
    message(STATUS "single_include/ezMcp.hpp is up to date")
endif()
