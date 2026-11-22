cmake_minimum_required(VERSION 3.10)

project(forceCoverProject LANGUAGES CXX)

# forceCover is built by its own GNU Makefile, whatever the generator of the
# parent build (Ninja, Makefiles...) : CMAKE_MAKE_PROGRAM cannot drive it.
find_program(FORCE_COVER_MAKE NAMES make gmake)
if(NOT FORCE_COVER_MAKE)
    message(FATAL_ERROR "forceCover: GNU make not found, it is required to build the forceCover tool (tests/coverage/forceCover/Makefile).")
endif()

add_custom_target(forceCover ALL
    COMMAND ${FORCE_COVER_MAKE} -C ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
