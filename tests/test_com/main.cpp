#include <TestEzNamedPipe.h>

#include <limits>
#include <cmath>

#include <ezlibs/ezLog.hpp>

#define IfTestCollectionExist(v)             \
    if (vTest.find(#v) != std::string::npos) \
    return v(vTest)

bool TestCom(const std::string& vTest) {
    IfTestCollectionExist(TestEzNamedPipe);
    return false;
}

////////////////////////////////////////////////////////////////////////////
//// ENTRY POINT ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
    ez::Log::initSingleton();
    if (argc > 1) {
        printf("Exec test : %s\n", argv[1]);
        return TestCom(argv[1]) ? 0 : 1;
    }
    // User testing
    return TestCom("TestEzNamedPipe_Basis") ? 0 : 1;
}
