# Generic LLVM source-based coverage helpers, shared across subsystems
# (ezlibs, ezvision, any future library).
#
# Provides two CMake macros:
#
#   ezCoverage_enableFlags()
#       Apply compile/link flags for source-based coverage (Clang) or
#       gcov-style (GCC). Must be called BEFORE the add_library /
#       add_executable commands of the targets that need to be instrumented.
#
#   ezCoverage_addSubsystem(<aSubsystemName> <aIncludeDir> [aExtraIncludeDirs...])
#       Define two custom targets:
#         forcecover_prep_<aSubsystemName>
#         cov_<aSubsystemName>
#       Output is rooted at ${CMAKE_BINARY_DIR}/Tests/<aSubsystemName>/.
#       The cov target runs the full ctest suite and the HTML / lcov reports
#       are restricted to <aIncludeDir> (other sources are excluded by
#       llvm-cov / genhtml).
#
# Both helpers are macros (not functions) because they call directory-scoped
# CMake commands (add_compile_options, add_link_options, include_directories,
# add_custom_target) whose effects must reach the calling CMakeLists.
#
# The forceCover binary, the helper shell scripts and the python helpers
# live next to this file (ezlibs/Tests/coverage/) and are reused by all
# subsystems through this module — no need to duplicate them anywhere.

# Resolved at include-time so the path is correct regardless of which
# CMakeLists includes this file.
set(_EZ_COVERAGE_SCRIPTS_DIR "${CMAKE_CURRENT_LIST_DIR}")

macro(ezCoverage_enableFlags)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping -O0 -fno-inline -fno-elide-constructors)
        add_link_options(-fprofile-instr-generate -fcoverage-mapping)
    else()
        add_compile_options(-fprofile-arcs -ftest-coverage --coverage -fno-inline -O0)
        add_link_options(-fprofile-arcs -ftest-coverage --coverage -fno-inline -O0)
    endif()
endmacro()

macro(ezCoverage_addSubsystem aSubsystemName aIncludeDir)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "ezCoverage_addSubsystem(${aSubsystemName}): requires Clang for LLVM source-based coverage.")
    endif()

    # ARGN: optional extra_include_dirs (forwarded to forceCover preprocessing).
    set(_ezcov_extraIncludes "${ARGN}")
    set(_ezcov_fcMirrorDir   "${CMAKE_BINARY_DIR}/fc_headers_${aSubsystemName}")
    set(_ezcov_outDir        "${CMAKE_BINARY_DIR}/Tests/${aSubsystemName}")

    file(MAKE_DIRECTORY "${_ezcov_outDir}")
    include_directories(BEFORE "${_ezcov_fcMirrorDir}" "${aIncludeDir}")

    set(_ezcov_shMergeProfraw       "${_EZ_COVERAGE_SCRIPTS_DIR}/merge_profraw.sh")
    set(_ezcov_shCollectShowReport  "${_EZ_COVERAGE_SCRIPTS_DIR}/collect_show_report.sh")
    set(_ezcov_shExportLcovGenhtml  "${_EZ_COVERAGE_SCRIPTS_DIR}/export_lcov_genhtml.sh")
    set(_ezcov_cmRunForceCoverHdrs  "${_EZ_COVERAGE_SCRIPTS_DIR}/run_force_cover_headers.cmake")
    set(_ezcov_pyConvertUtf8        "${_EZ_COVERAGE_SCRIPTS_DIR}/convert_to_utf8.py")
    set(_ezcov_pyPatchInline        "${_EZ_COVERAGE_SCRIPTS_DIR}/patch_genhtml_forcecover_inline.py")

    set(_ezcov_forceCoverExecutable "${_EZ_COVERAGE_SCRIPTS_DIR}/forceCover/forceCover")
    set(_ezcov_fixCoveragePy        "${_EZ_COVERAGE_SCRIPTS_DIR}/forceCover/fixCoverage.py")

    set(_ezcov_coverageTxt        "${_ezcov_outDir}/coverage.txt")
    set(_ezcov_coverageTxtUtf8    "${_ezcov_outDir}/coverage_utf8.txt")
    set(_ezcov_reportTxt          "${_ezcov_outDir}/report.txt")
    set(_ezcov_lcovInfo           "${_ezcov_outDir}/lcov.info")
    set(_ezcov_lcovHtmlDir        "${_ezcov_outDir}/coverage_lcov")
    set(_ezcov_coverageTar        "${_ezcov_outDir}/coverage_bundle.tar.gz")

    add_custom_target(forcecover_prep_${aSubsystemName}
        COMMAND ${CMAKE_COMMAND} -E echo "[force-cover/${aSubsystemName}] preprocess -> ${_ezcov_fcMirrorDir}"
        COMMAND ${CMAKE_COMMAND}
                -DFORCE_COVER_EXE=${_ezcov_forceCoverExecutable}
                -DFC_MIRROR_DIR=${_ezcov_fcMirrorDir}
                -DFORCE_COVER_HEADER_DIRS=${aIncludeDir}
                -DFORCE_COVER_STD=c++17
                -DFORCE_COVER_INCLUDE_DIRS=${_ezcov_extraIncludes}
                -DFORCE_COVER_VERBOSE=2
                -P ${_ezcov_cmRunForceCoverHdrs}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(cov_${aSubsystemName}
        COMMAND ${CMAKE_COMMAND} -E echo "== ${aSubsystemName} instrumented ctest =="
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/profraw_${aSubsystemName}
        COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/profraw_${aSubsystemName}/%p_%m.profraw
                ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --output-on-failure

        COMMAND ${CMAKE_COMMAND} -E echo "== merge profraw =="
        COMMAND bash ${_ezcov_shMergeProfraw} ${CMAKE_BINARY_DIR}/profraw_${aSubsystemName} ${CMAKE_BINARY_DIR}/${aSubsystemName}.profdata

        COMMAND ${CMAKE_COMMAND} -E echo "== llvm-cov show + report =="
        COMMAND bash ${_ezcov_shCollectShowReport} ${CMAKE_BINARY_DIR} ${_ezcov_outDir} ${CMAKE_BINARY_DIR}/${aSubsystemName}.profdata ${CMAKE_BINARY_DIR}/profraw_${aSubsystemName}

        COMMAND ${CMAKE_COMMAND} -E echo "== UTF8 + fix_coverage.py =="
        COMMAND python3 ${_ezcov_pyConvertUtf8} ${_ezcov_coverageTxt} ${_ezcov_coverageTxtUtf8}
        COMMAND ${CMAKE_COMMAND} -E rm -f ${_ezcov_coverageTxt}
        COMMAND python3 ${_ezcov_fixCoveragePy} ${_ezcov_coverageTxtUtf8}

        COMMAND ${CMAKE_COMMAND} -E echo "== export LCOV + genhtml =="
        COMMAND bash ${_ezcov_shExportLcovGenhtml} ${CMAKE_BINARY_DIR} ${_ezcov_lcovInfo} ${_ezcov_lcovHtmlDir} ${CMAKE_BINARY_DIR}/${aSubsystemName}.profdata ${aIncludeDir}

        COMMAND ${CMAKE_COMMAND} -E echo "== patch genhtml (inline, optional) =="
        COMMAND python3 ${_ezcov_pyPatchInline} ${_ezcov_coverageTxtUtf8} ${_ezcov_lcovHtmlDir} || true

        COMMAND ${CMAKE_COMMAND} -E echo "== bundle TAR =="
        COMMAND ${CMAKE_COMMAND} -E tar cfvz ${_ezcov_coverageTar}
                ${_ezcov_lcovInfo}
                ${_ezcov_coverageTxtUtf8}
                ${_ezcov_reportTxt}
                ${_ezcov_lcovHtmlDir}
                ${_ezcov_fcMirrorDir}

        COMMENT "${aSubsystemName} LLVM coverage pipeline"
        USES_TERMINAL
        VERBATIM
    )
endmacro()
