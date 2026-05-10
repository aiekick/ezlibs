#include <TestEzEpipolar.h>

#include <string>
#include <cstdio>

#define IfTestCollectionExist(v)             \
    if (vTest.find(#v) != std::string::npos) \
    return v(vTest)

bool TestVision(const std::string& vTest) {
    IfTestCollectionExist(TestEzEpipolar);
    return false;
}

////////////////////////////////////////////////////////////////////////////
//// ENTRY POINT ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
    if (argc > 1) {
        printf("Exec test : %s\n", argv[1]);
        return TestVision(argv[1]) ? 0 : 1;
    }
    // User testing
    return TestVision("TestEzEpipolar_DecomposeRejectsNonSquare<float>") ? 0 : 1;
}
