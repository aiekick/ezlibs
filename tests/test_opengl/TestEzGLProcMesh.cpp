#include "TestEzGLProcMesh.h"
#include "glContext.h"
#include <ezlibs/ezGL/ezGL.hpp>
#include <ezlibs/ezCTest.hpp>

bool TestEzGL_ProcMesh_CreateUVSphere() {
    CTEST_ASSERT(GLContext::initGLContext());

    auto sphere = ez::gl::ProcMesh::createUVSphere(1.0, 10, 10);
    CTEST_ASSERT(sphere != nullptr);
    CTEST_ASSERT(sphere->isValid());

    GLContext::unitGLContext();
    return true;
}

bool TestEzGL_ProcMesh_CreateUVSphere_SmallSubdivs() {
    CTEST_ASSERT(GLContext::initGLContext());

    auto sphere = ez::gl::ProcMesh::createUVSphere(1.0, 3, 3);
    CTEST_ASSERT(sphere != nullptr);
    CTEST_ASSERT(sphere->isValid());

    GLContext::unitGLContext();
    return true;
}

bool TestEzGL_ProcMesh_CreateUVSphere_LargeSubdivs() {
    CTEST_ASSERT(GLContext::initGLContext());

    auto sphere = ez::gl::ProcMesh::createUVSphere(2.0, 20, 20);
    CTEST_ASSERT(sphere != nullptr);
    CTEST_ASSERT(sphere->isValid());

    GLContext::unitGLContext();
    return true;
}

bool TestEzGL_ProcMesh_CreateUVSphere_DifferentRadius() {
    CTEST_ASSERT(GLContext::initGLContext());

    auto sphere1 = ez::gl::ProcMesh::createUVSphere(0.5, 10, 10);
    CTEST_ASSERT(sphere1 != nullptr);
    CTEST_ASSERT(sphere1->isValid());

    auto sphere2 = ez::gl::ProcMesh::createUVSphere(5.0, 10, 10);
    CTEST_ASSERT(sphere2 != nullptr);
    CTEST_ASSERT(sphere2->isValid());

    GLContext::unitGLContext();
    return true;
}

bool TestEzGL_ProcMesh_ProcDatas_DefaultConstructor() {
    ez::gl::ProcDatas data;
    CTEST_ASSERT(data.p[0] == 0.0f);
    CTEST_ASSERT(data.p[1] == 0.0f);
    CTEST_ASSERT(data.p[2] == 0.0f);
    CTEST_ASSERT(data.t[0] == 0.0f);
    CTEST_ASSERT(data.t[1] == 0.0f);
    return true;
}

bool TestEzGL_ProcMesh_ProcDatas_ParameterizedConstructor() {
    ez::gl::ProcDatas data(1.0f, 2.0f, 3.0f, 0.5f, 0.75f);
    CTEST_ASSERT(data.p[0] == 1.0f);
    CTEST_ASSERT(data.p[1] == 2.0f);
    CTEST_ASSERT(data.p[2] == 3.0f);
    CTEST_ASSERT(data.t[0] == 0.5f);
    CTEST_ASSERT(data.t[1] == 0.75f);
    return true;
}

bool TestEzGL_ProcMesh_AccessorsReturnSemanticallyCorrectIds() {
    CTEST_ASSERT(GLContext::initGLContext());
    auto sphere = ez::gl::ProcMesh::createUVSphere(1.0, 10, 10);
    CTEST_ASSERT(sphere != nullptr);
    CTEST_ASSERT(sphere->isValid());

    const auto vaoId = sphere->GetVaoID();
    const auto vboId = sphere->GetVboID();
    const auto iboId = sphere->GetIboID();

    // VAO and buffers live in different GL namespaces, so vaoId may equal
    // vboId/iboId numerically. Only the two buffer IDs MUST be distinct.
    CTEST_ASSERT(vaoId != 0U);
    CTEST_ASSERT(vboId != 0U);
    CTEST_ASSERT(iboId != 0U);
    CTEST_ASSERT(vboId != iboId);

    // Semantic check: GL itself classifies each ID. A circular swap of
    // the accessors would fail at least one of these assertions.
    CTEST_ASSERT(glIsVertexArray(vaoId) == GL_TRUE);
    CTEST_ASSERT(glIsBuffer(vboId) == GL_TRUE);
    CTEST_ASSERT(glIsBuffer(iboId) == GL_TRUE);

    GLContext::unitGLContext();
    return true;
}

////////////////////////////////////////////////////////////////////////////
//// ENTRY POINT ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzGL_ProcMesh(const std::string& vTest) {
    IfTestExist(TestEzGL_ProcMesh_CreateUVSphere);
    IfTestExist(TestEzGL_ProcMesh_CreateUVSphere_SmallSubdivs);
    IfTestExist(TestEzGL_ProcMesh_CreateUVSphere_LargeSubdivs);
    IfTestExist(TestEzGL_ProcMesh_CreateUVSphere_DifferentRadius);
    IfTestExist(TestEzGL_ProcMesh_ProcDatas_DefaultConstructor);
    IfTestExist(TestEzGL_ProcMesh_ProcDatas_ParameterizedConstructor);
    IfTestExist(TestEzGL_ProcMesh_AccessorsReturnSemanticallyCorrectIds);
    return false;
}
