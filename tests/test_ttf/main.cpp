#include <TestEzTtfTypes.h>
#include <TestEzTtfStream.h>
#include <TestEzTtfTables.h>
#include <TestEzTtfFont.h>
#include <TestEzTtfBuilder.h>
#include <TestEzTtfGlyph.h>
#include <TestEzTtfOutline.h>
#include <TestEzTtfSynthesizer.h>

#include <string>
#include <cstdio>

#include <ezlibs/ezCTest.hpp>
#include <ezlibs/ezLog.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestTtf(const std::string& vTest) {
    IfTestCollectionExist(TestEzTtfStream);
    else IfTestCollectionExist(TestEzTtfTables);
    else IfTestCollectionExist(TestEzTtfFont);
    else IfTestCollectionExist(TestEzTtfBuilder);
    else IfTestCollectionExist(TestEzTtfTypes);
    else IfTestCollectionExist(TestEzTtfGlyph);
    else IfTestCollectionExist(TestEzTtfOutline);
    else IfTestCollectionExist(TestEzTtfSynthesizer);
    return false;
}

////////////////////////////////////////////////////////////////////////////
//// ENTRY POINT ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
    ez::Log::initSingleton();
    if (argc > 1) {
        printf("Exec test : %s\n", argv[1]);
        return TestTtf(argv[1]) ? 0 : 1;
    }
    // User testing
    return TestTtf("TestEzTtfStream_RoundTripsEveryType") ? 0 : 1;
}
