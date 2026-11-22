# Usage: cmake -DCTEST_EXE=<ctest> -DCTEST_BUILD_DIR=<dir> [-DCTEST_CONFIG=<cfg>] -P run_ctest_nonblocking.cmake
#
# Runs the ctest suite for coverage. A red test is reported but does not stop
# the pipeline : the report of what ran is still worth producing.

if(NOT DEFINED CTEST_EXE)
    set(CTEST_EXE ctest)
endif()
if(NOT DEFINED CTEST_BUILD_DIR)
    message(FATAL_ERROR "run_ctest_nonblocking: CTEST_BUILD_DIR is not defined")
endif()

set(_cmd "${CTEST_EXE}" --test-dir "${CTEST_BUILD_DIR}" --output-on-failure)
if(DEFINED CTEST_CONFIG AND NOT CTEST_CONFIG STREQUAL "")
    list(APPEND _cmd -C "${CTEST_CONFIG}")
endif()

execute_process(COMMAND ${_cmd} RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(WARNING "run_ctest_nonblocking: some tests failed (ctest returned ${_rc}), the coverage report goes on with what ran")
endif()
