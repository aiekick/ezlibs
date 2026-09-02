#include <TestEzSvgPathParser.h>
#include <TestEzSvgTransform.h>
#include <TestEzSvgColor.h>
#include <TestEzSvgReader.h>
#include <TestEzSvgWriter.h>

#include <string>
#include <cstdio>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezLog.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestSvg(const std::string& vTest) {
    IfTestCollectionExist(TestEzSvgPathParser);
    else IfTestCollectionExist(TestEzSvgTransform);
    else IfTestCollectionExist(TestEzSvgColor);
    else IfTestCollectionExist(TestEzSvgReader);
    else IfTestCollectionExist(TestEzSvgWriter);
    return false;
}

////////////////////////////////////////////////////////////////////////////
//// ENTRY POINT ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
    ez::Log::initSingleton();
    if (argc > 1) {
        printf("Exec test : %s\n", argv[1]);
        return TestSvg(argv[1]) ? 0 : 1;
    }
    // User testing
    return TestSvg("TestEzSvgReader_ASquarePathIsOneShape") ? 0 : 1;
}
