#include <TestEzMath.h>
#include <TestEzVec2.h>
#include <TestEzVec3.h>
#include <TestEzVec4.h>
#include <TestEzVecN.h>
#include <TestEzAABBCC.h>
#include <TestEzMat2.h>
#include <TestEzMatN.h>
#include <TestEzSvd.h>
#include <TestEzEigenSym.h>
#include <TestEzKdTree.h>
#include <TestEzRansac.h>
#include <TestEzEpipolar.h>
#include <TestEzQuat.h>
#include <TestEzAABB.h>
#include <TestEzExpr.h>

#include <limits>
#include <cmath>

#define IfTestCollectionExist(v)             \
    if (vTest.find(#v) != std::string::npos) \
    return v(vTest)

bool TestMisc(const std::string& vTest) {
    IfTestCollectionExist(TestEzMath);
    IfTestCollectionExist(TestEzAABBCC);
    IfTestCollectionExist(TestEzVec2);
    IfTestCollectionExist(TestEzVec3);
    IfTestCollectionExist(TestEzVec4);
    IfTestCollectionExist(TestEzVecN);
    IfTestCollectionExist(TestEzMat2);
    IfTestCollectionExist(TestEzMatN);
    IfTestCollectionExist(TestEzSvd);
    IfTestCollectionExist(TestEzEigenSym);
    IfTestCollectionExist(TestEzKdTree);
    IfTestCollectionExist(TestEzRansac);
    IfTestCollectionExist(TestEzEpipolar);
    IfTestCollectionExist(TestEzQuat);
    IfTestCollectionExist(TestEzAABB);
    IfTestCollectionExist(TestEzExpr);
    return false;
}

////////////////////////////////////////////////////////////////////////////
//// ENTRY POINT ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
    if (argc > 1) {
        printf("Exec test : %s\n", argv[1]);
        return TestMisc(argv[1]) ? 0 : 1;
    }
    // User testing
    return TestMisc("TestEzVec2_GetNormalized<double>") ? 0 : 1;
}
