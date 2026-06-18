#include <TestEzGLUniforms.h>
#include <TestEzGLCompVfx.h>
#include <TestEzGLCanvas.h>
#include <TestEzGLMeshVfx.h>
#include <TestEzGLQuadVfx.h>
#include <TestEzGLQuadVfxAuto.h>
#include <TestEzGLBufferBlock.h>
#include <TestEzGLProcMesh.h>
#include <TestEzGLTexture.h>
#include <TestEzGLShader.h>
#include <TestEzGLProgram.h>
#include <TestEzGLCamera.h>

#include <ezlibs/ezLog.hpp>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#define IfTestCollectionExist(v)             \
    if (vTest.find(#v) != std::string::npos) \
    return v(vTest)

bool TestOpengl(const std::string& vTest) {
    IfTestCollectionExist(TestEzGL_Uniforms);
    else IfTestCollectionExist(TestEzGL_Canvas);
    else IfTestCollectionExist(TestEzGL_MeshVfx);
    else IfTestCollectionExist(TestEzGL_QuadVfxAuto);
    else IfTestCollectionExist(TestEzGL_QuadVfx);
    else IfTestCollectionExist(TestEzGL_CompVfx);
    else IfTestCollectionExist(TestEzGL_BufferBlock);
    else IfTestCollectionExist(TestEzGL_ProcMesh);
    else IfTestCollectionExist(TestEzGL_Texture);
    else IfTestCollectionExist(TestEzGL_Shader);
    else IfTestCollectionExist(TestEzGL_Program);
    else IfTestCollectionExist(TestEzGL_Camera);
    return false;
}

////////////////////////////////////////////////////////////////////////////
//// ENTRY POINT ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
    ez::Log::initSingleton();
    if (argc > 1) {
        printf("Exec test : %s\n", argv[1]);
        return TestOpengl(argv[1]) ? 0 : 1;
    }
    // User testing
    return TestOpengl("TestEzCamera_TurntableInitialState") ? 0 : 1;
}
