#pragma once

/*
MIT License

Copyright (c) 2014-2024 Stephane Cuillerdier (aka aiekick)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// ezGL is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

#include <memory>

#ifdef _TESTING_
#define ASSERT_THROW(cond, msg) \
    if (!(cond)) throw std::runtime_error(msg)
#else
#define ASSERT_THROW(cond, msg) assert((cond) && msg)
#endif

namespace ez {
namespace gl {

class BufferBlock;
using BufferBlockPtr = ::std::unique_ptr<BufferBlock>;

class FBO;
typedef std::shared_ptr<FBO> FBOPtr;
typedef std::weak_ptr<FBO> FBOWeak;

class FBOPipeLine;
typedef std::shared_ptr<FBOPipeLine> FBOPipeLinePtr;
typedef std::weak_ptr<FBOPipeLine> FBOPipeLineWeak;

class ProcMesh;
typedef std::shared_ptr<ProcMesh> ProcMeshPtr;
typedef std::weak_ptr<ProcMesh> ProcMeshWeak;

class QuadMesh;
typedef std::shared_ptr<QuadMesh> QuadMeshPtr;
typedef std::weak_ptr<QuadMesh> QuadMeshWeak;

class Program;
typedef std::shared_ptr<Program> ProgramPtr;
typedef std::weak_ptr<Program> ProgramWeak;

class ProgramAuto;
typedef std::shared_ptr<ProgramAuto> ProgramAutoPtr;
typedef std::weak_ptr<ProgramAuto> ProgramAutoWeak;

class Shader;
typedef std::shared_ptr<Shader> ShaderPtr;
typedef std::weak_ptr<Shader> ShaderWeak;

class ShaderAuto;
typedef std::shared_ptr<ShaderAuto> ShaderAutoPtr;
typedef std::weak_ptr<ShaderAuto> ShaderAutoWeak;

class Texture;
typedef std::shared_ptr<Texture> TexturePtr;
typedef std::weak_ptr<Texture> TextureWeak;

class CompVfx;
typedef std::shared_ptr<CompVfx> CompVfxPtr;
typedef std::weak_ptr<CompVfx> CompVfxWeak;

class SdfMap;
typedef std::shared_ptr<SdfMap> SdfMapPtr;
typedef std::weak_ptr<SdfMap> SdfMapWeak;

class MeshVfx;
typedef std::shared_ptr<MeshVfx> MeshVfxPtr;
typedef std::weak_ptr<MeshVfx> MeshVfxWeak;

class QuadVfx;
typedef std::shared_ptr<QuadVfx> QuadVfxPtr;
typedef std::weak_ptr<QuadVfx> QuadVfxWeak;

class QuadVfxAuto;
typedef std::shared_ptr<QuadVfxAuto> QuadVfxAutoPtr;
typedef std::weak_ptr<QuadVfxAuto> QuadVfxAutoWeak;

}  // namespace gl
}  // namespace ez
