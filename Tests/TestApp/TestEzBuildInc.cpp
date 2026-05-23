#include <TestEzBuildInc.h>
#include <ezlibs/ezFigFont.hpp>
#include <ezlibs/ezBuildInc.hpp>
#include <ezlibs/ezFile.hpp>
#include <ezlibs/ezCTest.hpp>

#include <exception>
#include <iostream>
#include <string>
#include <array>

// Desactivation des warnings de conversion
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)  // Conversion from 'double' to 'float', possible loss of data
#pragma warning(disable : 4305)  // Truncation from 'double' to 'float'
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzBuildInc_base() {
    std::string file = "build.h";
    std::string baseContent = u8R"(
#pragma once

#define Project_Label "project"
#define Project_BuildNumber 3629
#define Project_MinorNumber 3
#define Project_MajorNumber 0
#define Project_BuildId "0.3.3629"
#define Project_BuildIdNum 00033629
)";
    if (!ez::file::saveStringToFile(baseContent, file)) {
        return false;
    }
    auto builder = ez::BuildInc(file);
    CTEST_ASSERT(builder.setFigFontFile(SAMPLES_PATH "/TOTO.flf").isValid() == false);
    auto& figFontBuilder = builder.setFigFontFile(SAMPLES_PATH "/big.flf");
    CTEST_ASSERT(figFontBuilder.isValid() == true);
    CTEST_ASSERT(builder.getBuildNumber() == 3629);
    CTEST_ASSERT(builder.getMinor() == 3);
    CTEST_ASSERT(builder.getMajor() == 0);
    CTEST_ASSERT(builder.getLabel() == "project");
    CTEST_ASSERT(builder.getProject() == "Project");
    std::string expectedContent = u8R"DELIM(#pragma once

#define Project_Label "project"
#define Project_BuildNumber 3630
#define Project_MinorNumber 3
#define Project_MajorNumber 0
#define Project_BuildId "0.3.3630"
#define Project_BuildIdNum 00033630
#define Project_FigFontLabel u8R"(                       _              _             ___      ____  
                      (_)            | |           / _ \    |___ \ 
 _ __   _ __   ___     _   ___   ___ | |_  __   __| | | |     __) |
| '_ \ | '__| / _ \   | | / _ \ / __|| __| \ \ / /| | | |    |__ < 
| |_) || |   | (_) |  | ||  __/| (__ | |_   \ V / | |_| | _  ___) |
| .__/ |_|    \___/   | | \___| \___| \__|   \_/   \___/ (_)|____/ 
| |                  _/ |                                          
|_|                 |__/                                           
)"
)DELIM";

    builder.incBuildNumber().write().printInfos();
    auto newContent = ez::file::loadFileToString(file);
    CTEST_ASSERT(newContent == expectedContent);

    figFontBuilder.useLabel(false);
    expectedContent = u8R"DELIM(#pragma once

#define Project_Label "project"
#define Project_BuildNumber 3630
#define Project_MinorNumber 3
#define Project_MajorNumber 0
#define Project_BuildId "0.3.3630"
#define Project_BuildIdNum 00033630
#define Project_FigFontLabel u8R"(         ___      ____  
        / _ \    |___ \ 
__   __| | | |     __) |
\ \ / /| | | |    |__ < 
 \ V / | |_| | _  ___) |
  \_/   \___/ (_)|____/ 
)"
)DELIM";
    builder.write().printInfos();
    newContent = ez::file::loadFileToString(file);
    CTEST_ASSERT(newContent == expectedContent);

    figFontBuilder.useBuildNumber(true);
    expectedContent = u8R"DELIM(#pragma once

#define Project_Label "project"
#define Project_BuildNumber 3630
#define Project_MinorNumber 3
#define Project_MajorNumber 0
#define Project_BuildId "0.3.3630"
#define Project_BuildIdNum 00033630
#define Project_FigFontLabel u8R"(         ___      ____      ____     __   ____    ___  
        / _ \    |___ \    |___ \   / /  |___ \  / _ \ 
__   __| | | |     __) |     __) | / /_    __) || | | |
\ \ / /| | | |    |__ <     |__ < | '_ \  |__ < | | | |
 \ V / | |_| | _  ___) | _  ___) || (_) | ___) || |_| |
  \_/   \___/ (_)|____/ (_)|____/  \___/ |____/  \___/ 
)"
)DELIM";
    builder.write().printInfos();
    newContent = ez::file::loadFileToString(file);
    CTEST_ASSERT(newContent == expectedContent);

    figFontBuilder.useLabel(true);
    expectedContent = u8R"DELIM(#pragma once

#define Project_Label "project"
#define Project_BuildNumber 3630
#define Project_MinorNumber 3
#define Project_MajorNumber 0
#define Project_BuildId "0.3.3630"
#define Project_BuildIdNum 00033630
#define Project_FigFontLabel u8R"(                       _              _             ___      ____      ____     __   ____    ___  
                      (_)            | |           / _ \    |___ \    |___ \   / /  |___ \  / _ \ 
 _ __   _ __   ___     _   ___   ___ | |_  __   __| | | |     __) |     __) | / /_    __) || | | |
| '_ \ | '__| / _ \   | | / _ \ / __|| __| \ \ / /| | | |    |__ <     |__ < | '_ \  |__ < | | | |
| |_) || |   | (_) |  | ||  __/| (__ | |_   \ V / | |_| | _  ___) | _  ___) || (_) | ___) || |_| |
| .__/ |_|    \___/   | | \___| \___| \__|   \_/   \___/ (_)|____/ (_)|____/  \___/ |____/  \___/ 
| |                  _/ |                                                                         
|_|                 |__/                                                                          
)"
)DELIM";
    figFontBuilder.useBuildNumber(true);
    builder.write().printInfos();
    newContent = ez::file::loadFileToString(file);
    CTEST_ASSERT(newContent == expectedContent);

    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzBuildInc_underscoreProject() {
    /* Regression: a project name that itself contains underscores (e.g.
     * "cdv_lib", "cdv_gui") must be recognised by m_parseDefine so that
     * the BuildNumber is read back across rebuilds and incBuildNumber
     * actually accumulates. Previously the parser split on the FIRST
     * underscore, producing project="cdv" + key="lib_Label" which never
     * matched the known-key list — BuildNumber was perpetually re-read
     * as 0 and the on-disk version stayed at 0.0.1 forever. */
    const std::string file = "build_underscore.h";
    const std::string baseContent = u8R"(#pragma once

#define cdv_lib_Label "cdv-lib"
#define cdv_lib_BuildNumber 42
#define cdv_lib_MinorNumber 1
#define cdv_lib_MajorNumber 0
#define cdv_lib_BuildId "0.1.42"
#define cdv_lib_BuildIdNum 00010042
)";
    if (!ez::file::saveStringToFile(baseContent, file)) {
        return false;
    }
    auto builder = ez::BuildInc(file);
    CTEST_ASSERT(builder.getProject()      == "cdv_lib");
    CTEST_ASSERT(builder.getLabel()        == "cdv-lib");
    CTEST_ASSERT(builder.getMajor()        == 0);
    CTEST_ASSERT(builder.getMinor()        == 1);
    CTEST_ASSERT(builder.getBuildNumber()  == 42);

    builder.incBuildNumber().write();
    CTEST_ASSERT(builder.getBuildNumber()  == 43);

    /* Re-read the file from scratch — the on-disk content must now have
     * BuildNumber 43, not be reset to 1. */
    auto reread = ez::BuildInc(file);
    CTEST_ASSERT(reread.getProject()      == "cdv_lib");
    CTEST_ASSERT(reread.getLabel()        == "cdv-lib");
    CTEST_ASSERT(reread.getBuildNumber()  == 43);
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#define IfTestExist(v)            \
    if (vTest == std::string(#v)) \
    return v()

bool TestEzBuildInc(const std::string& vTest) {
    IfTestExist(TestEzBuildInc_base);
    IfTestExist(TestEzBuildInc_underscoreProject);
    return false;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
