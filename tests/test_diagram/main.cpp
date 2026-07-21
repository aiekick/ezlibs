#include <TestEzBilayerCrossings.h>
#include <TestEzSeparation1D.h>

#include <string>

#include <ezlibs/ezCTest.hpp>  // IfTestCollectionExist + CTEST_* macros (do NOT redefine locally)
#include <ezlibs/ezLog.hpp>

bool TestDiagram(const std::string& vTest) {
    IfTestCollectionExist(TestEzBilayerCrossings);
    else IfTestCollectionExist(TestEzSeparation1D);
    return false;
}

////////////////////////////////////////////////////////////////////////////
//// ENTRY POINT ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
    ez::Log::initSingleton();
    if (argc > 1) {
        printf("Exec test : %s\n", argv[1]);
        return TestDiagram(argv[1]) ? 0 : 1;
    }
    // User testing
    return TestDiagram("TestEzBilayerCrossings_K22Crossed") ? 0 : 1;
}
