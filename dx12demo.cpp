/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include <math.h>
#include <string.h>

#include "dx12demo.h"
#include "D3DCompiler.h"

#define PI 3.14159265f
#define CUBE_SPIN_SPEED    0.5f // turns per second

struct DemoResources {
   ComPtr<ID3D12RootSignature> rootSignature;
   ComPtr<ID3D12PipelineState> pipelineState;
   ComPtr<ID3D12GraphicsCommandList> commandLists[ARRAY_COUNT(Dx12Device::frames)];
};

typedef struct Vec3 {
   float x, y, z;
} Vec3;

typedef struct Vec4 {
   float x, y, z, w;
} Vec4;

typedef struct Mat4 {
   Vec4 m[4];
} Mat4;

typedef struct ShaderMatrices {
   Mat4 clipFromLocal;
} ShaderMatrices;

static DemoResources s_resources;
static uint64_t s_frameNum = ARRAY_COUNT(Dx12Device::frames);
static float s_cubeRot;       // in turns

static inline Vec3 vec3Add(Vec3 a, Vec3 b)
{
   Vec3 r;

   r.x = a.x + b.x;
   r.y = a.y + b.y;
   r.z = a.z + b.z;

   return r;
}

static inline Vec3 vec3Sub(Vec3 a, Vec3 b)
{
   Vec3 r;

   r.x = a.x - b.x;
   r.y = a.y - b.y;
   r.z = a.z - b.z;

   return r;
}

static inline Vec3 vec3Scale(Vec3 a, float b)
{
   Vec3 r;

   r.x = a.x * b;
   r.y = a.y * b;
   r.z = a.z * b;

   return r;
}

static inline float vec3Dot(Vec3 a, Vec3 b)
{
   return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline Vec3 vec3Normalize(Vec3 v)
{
   float lensq = vec3Dot(v, v);
   return vec3Scale(v, 1.0f / sqrtf(lensq));
}

static inline Vec3 vec3Cross(Vec3 a, Vec3 b)
{
   Vec3 r;

   r.x = a.y * b.z - a.z * b.y;
   r.y = a.z * b.x - a.x * b.z;
   r.z = a.x * b.y - a.y * b.x;

   return r;
}

static void mat4RotY(Mat4 *m, float angle)
{
   float c = cosf(angle);
   float s = sinf(angle);

   m->m[0].x = c;
   m->m[0].y = 0.0f;
   m->m[0].z = s;
   m->m[0].w = 0.0f;

   m->m[1].x = 0.0f;
   m->m[1].y = 1.0f;
   m->m[1].z = 0.0f;
   m->m[1].w = 0.0f;

   m->m[2].x = -s;
   m->m[2].y = 0.0f;
   m->m[2].z = c;
   m->m[2].w = 0.0f;

   m->m[3].x = 0.0f;
   m->m[3].y = 0.0f;
   m->m[3].z = 0.0f;
   m->m[3].w = 1.0f;
}

static void mat4Mul(Mat4 *r, const Mat4 *a, const Mat4 *b)
{
   Mat4 tmp;

   tmp.m[0].x = a->m[0].x * b->m[0].x + a->m[1].x * b->m[0].y + a->m[2].x * b->m[0].z + a->m[3].x * b->m[0].w;
   tmp.m[0].y = a->m[0].y * b->m[0].x + a->m[1].y * b->m[0].y + a->m[2].y * b->m[0].z + a->m[3].y * b->m[0].w;
   tmp.m[0].z = a->m[0].z * b->m[0].x + a->m[1].z * b->m[0].y + a->m[2].z * b->m[0].z + a->m[3].z * b->m[0].w;
   tmp.m[0].w = a->m[0].w * b->m[0].x + a->m[1].w * b->m[0].y + a->m[2].w * b->m[0].z + a->m[3].w * b->m[0].w;

   tmp.m[1].x = a->m[0].x * b->m[1].x + a->m[1].x * b->m[1].y + a->m[2].x * b->m[1].z + a->m[3].x * b->m[1].w;
   tmp.m[1].y = a->m[0].y * b->m[1].x + a->m[1].y * b->m[1].y + a->m[2].y * b->m[1].z + a->m[3].y * b->m[1].w;
   tmp.m[1].z = a->m[0].z * b->m[1].x + a->m[1].z * b->m[1].y + a->m[2].z * b->m[1].z + a->m[3].z * b->m[1].w;
   tmp.m[1].w = a->m[0].w * b->m[1].x + a->m[1].w * b->m[1].y + a->m[2].w * b->m[1].z + a->m[3].w * b->m[1].w;

   tmp.m[2].x = a->m[0].x * b->m[2].x + a->m[1].x * b->m[2].y + a->m[2].x * b->m[2].z + a->m[3].x * b->m[2].w;
   tmp.m[2].y = a->m[0].y * b->m[2].x + a->m[1].y * b->m[2].y + a->m[2].y * b->m[2].z + a->m[3].y * b->m[2].w;
   tmp.m[2].z = a->m[0].z * b->m[2].x + a->m[1].z * b->m[2].y + a->m[2].z * b->m[2].z + a->m[3].z * b->m[2].w;
   tmp.m[2].w = a->m[0].w * b->m[2].x + a->m[1].w * b->m[2].y + a->m[2].w * b->m[2].z + a->m[3].w * b->m[2].w;

   tmp.m[3].x = a->m[0].x * b->m[3].x + a->m[1].x * b->m[3].y + a->m[2].x * b->m[3].z + a->m[3].x * b->m[3].w;
   tmp.m[3].y = a->m[0].y * b->m[3].x + a->m[1].y * b->m[3].y + a->m[2].y * b->m[3].z + a->m[3].y * b->m[3].w;
   tmp.m[3].z = a->m[0].z * b->m[3].x + a->m[1].z * b->m[3].y + a->m[2].z * b->m[3].z + a->m[3].z * b->m[3].w;
   tmp.m[3].w = a->m[0].w * b->m[3].x + a->m[1].w * b->m[3].y + a->m[2].w * b->m[3].z + a->m[3].w * b->m[3].w;

   memcpy(r, &tmp, sizeof(tmp));
}

static inline void mat4PerspectiveFov(Mat4 *r, float fovY, float aspect, float nearDist, float farDist)
{
   float y = tanf(fovY * 0.5f);
   float x = aspect * y;

   float hw = x * nearDist;
   float hh = y * nearDist;
   float c, d;

   if (nearDist < farDist) {
      c = nearDist / (nearDist - farDist);
      d = -farDist * nearDist / (nearDist - farDist);
   } else {
      c = 0.0f;
      d = nearDist;
   }

   r->m[0].x = 1.0f / x;
   r->m[0].y = 0.0f;
   r->m[0].z = 0.0f;
   r->m[0].w = 0.0f;

   r->m[1].x = 0.0f;
   r->m[1].y = -1.0f / y;
   r->m[1].z = 0.0f;
   r->m[1].w = 0.0f;

   r->m[2].x = 0.0f;
   r->m[2].y = 0.0f;
   r->m[2].z = c;
   r->m[2].w = 1.0f;

   r->m[3].x = 0.0f;
   r->m[3].y = 0.0f;
   r->m[3].z = d;
   r->m[3].w = 0.0f;
}

static inline void mat4LookAt(Mat4 *r, Vec3 eye, Vec3 target, Vec3 up)
{
   Vec3 mf = vec3Normalize(vec3Sub(target, eye));
   Vec3 mr = vec3Normalize(vec3Cross(up, mf));
   Vec3 mu = vec3Cross(mr, mf);

   r->m[0].x = mr.x;
   r->m[0].y = mr.y;
   r->m[0].z = mr.z;
   r->m[0].w = 0.0f;

   r->m[1].x = mu.x;
   r->m[1].y = mu.y;
   r->m[1].z = mu.z;
   r->m[1].w = 0.0f;

   r->m[2].x = mf.x;
   r->m[2].y = mf.y;
   r->m[2].z = mf.z;
   r->m[2].w = 0.0f;

   r->m[3].x = -eye.x * mr.x - eye.y * mu.x - eye.z * mf.x;
   r->m[3].y = -eye.x * mr.y - eye.y * mu.y - eye.z * mf.y;
   r->m[3].z = -eye.x * mr.z - eye.y * mu.z - eye.z * mf.z;
   r->m[3].w = 1.0f;
}

bool CreateResources(const Dx12Device *device)
{
   ComPtr<ID3DBlob> vertexCode, pixelCode;
   ComPtr<ID3D12RootSignature> rootSignature;
   {
      UINT compileFlags = 0;

#ifndef NDEBUG
      compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

      ComPtr<ID3DBlob> vertexErrors;
      if (FAILED(D3DCompileFromFile(L"cube.vert", nullptr, nullptr, "main", "vs_5_0", compileFlags, 0, &vertexCode, &vertexErrors))) {
#ifndef NDEBUG
         if (vertexErrors) {
            OutputDebugStringA((const char *)vertexErrors->GetBufferPointer());
         }
#endif
         return false;
      }

      ComPtr<ID3DBlob> pixelErrors;
      if (FAILED(D3DCompileFromFile(L"cube.frag", nullptr, nullptr, "main", "ps_5_0", compileFlags, 0, &pixelCode, &pixelErrors))) {
#ifndef NDEBUG
         if (pixelErrors) {
            OutputDebugStringA((const char *)pixelErrors->GetBufferPointer());
         }
#endif
         return false;
      }

      D3D12_ROOT_PARAMETER constantParam;
      constantParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
      constantParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
      constantParam.Constants.ShaderRegister = 0;
      constantParam.Constants.RegisterSpace = 0;
      constantParam.Constants.Num32BitValues = sizeof(ShaderMatrices) / sizeof(UINT);

      D3D12_ROOT_SIGNATURE_DESC rsDesc;
      rsDesc.NumParameters = 1;
      rsDesc.pParameters = &constantParam;
      rsDesc.NumStaticSamplers = 0;
      rsDesc.pStaticSamplers = nullptr;
      rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

      ComPtr<ID3DBlob> rootCode, rootErrors;
      if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootCode, &rootErrors))) {
#ifndef NDEBUG
         if (rootErrors) {
            OutputDebugStringA((const char *)rootErrors->GetBufferPointer());
         }
#endif
         return false;
      }

      if (FAILED(device->device->CreateRootSignature(0, rootCode->GetBufferPointer(), rootCode->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))) {
         return false;
      }
   }

   ComPtr<ID3D12PipelineState> pipelineState;
   {
      D3D12_GRAPHICS_PIPELINE_STATE_DESC psDesc;
      psDesc.pRootSignature = rootSignature.Get();

      psDesc.VS.BytecodeLength = vertexCode->GetBufferSize();
      psDesc.VS.pShaderBytecode = vertexCode->GetBufferPointer();
      psDesc.PS.BytecodeLength = pixelCode->GetBufferSize();
      psDesc.PS.pShaderBytecode = pixelCode->GetBufferPointer();
      psDesc.DS.BytecodeLength = 0;
      psDesc.DS.pShaderBytecode = nullptr;
      psDesc.HS.BytecodeLength = 0;
      psDesc.HS.pShaderBytecode = nullptr;
      psDesc.GS.BytecodeLength = 0;
      psDesc.GS.pShaderBytecode = nullptr;

      psDesc.StreamOutput.pSODeclaration = nullptr;
      psDesc.StreamOutput.NumEntries = 0;
      psDesc.StreamOutput.pBufferStrides = nullptr;
      psDesc.StreamOutput.NumStrides = 0;
      psDesc.StreamOutput.RasterizedStream = 0;

      psDesc.BlendState.AlphaToCoverageEnable = FALSE;
      psDesc.BlendState.IndependentBlendEnable = FALSE;
      psDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
      psDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
      psDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
      psDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
      psDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
      psDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
      psDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
      psDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
      psDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_CLEAR;
      psDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

      psDesc.SampleMask = UINT_MAX;

      psDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
      psDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
      psDesc.RasterizerState.FrontCounterClockwise = FALSE;
      psDesc.RasterizerState.DepthBias = 0;
      psDesc.RasterizerState.DepthBiasClamp = 0;
      psDesc.RasterizerState.SlopeScaledDepthBias = 0.0;
      psDesc.RasterizerState.DepthClipEnable = FALSE;
      psDesc.RasterizerState.MultisampleEnable = FALSE;
      psDesc.RasterizerState.AntialiasedLineEnable = FALSE;
      psDesc.RasterizerState.ForcedSampleCount = 0;
      psDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

      psDesc.DepthStencilState.DepthEnable = FALSE;
      psDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
      psDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
      psDesc.DepthStencilState.StencilEnable = FALSE;
      psDesc.DepthStencilState.StencilReadMask = 0xff;
      psDesc.DepthStencilState.StencilWriteMask = 0xff;
      psDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
      psDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
      psDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
      psDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      psDesc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
      psDesc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
      psDesc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
      psDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

      psDesc.InputLayout.pInputElementDescs = nullptr;
      psDesc.InputLayout.NumElements = 0;
      psDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
      psDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

      psDesc.NumRenderTargets = 1;
      psDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
      psDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
      psDesc.RTVFormats[2] = DXGI_FORMAT_UNKNOWN;
      psDesc.RTVFormats[3] = DXGI_FORMAT_UNKNOWN;
      psDesc.RTVFormats[4] = DXGI_FORMAT_UNKNOWN;
      psDesc.RTVFormats[5] = DXGI_FORMAT_UNKNOWN;
      psDesc.RTVFormats[6] = DXGI_FORMAT_UNKNOWN;
      psDesc.RTVFormats[7] = DXGI_FORMAT_UNKNOWN;
      psDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

      psDesc.NodeMask = 0;
      psDesc.SampleDesc.Count = 1;
      psDesc.SampleDesc.Quality = 0;
      psDesc.CachedPSO.pCachedBlob = nullptr;
      psDesc.CachedPSO.CachedBlobSizeInBytes = 0;

      psDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

      if (FAILED(device->device->CreateGraphicsPipelineState(&psDesc, IID_PPV_ARGS(&pipelineState)))) {
         return false;
      }
   }

   std::array<ComPtr<ID3D12GraphicsCommandList>, ARRAY_COUNT(device->frames)> commandLists;
   for (size_t i = 0; i < ARRAY_COUNT(device->frames); ++i) {
      if (FAILED(device->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
         device->frames[i].commandAllocator.Get(), pipelineState.Get(), IID_PPV_ARGS(&commandLists[i])))) {
         return false;
      }
      commandLists[i]->Close();
   }

   s_resources.rootSignature = std::move(rootSignature);
   s_resources.pipelineState = std::move(pipelineState);
   for (size_t i = 0; i < ARRAY_COUNT(device->frames); ++i) {
      s_resources.commandLists[i] = std::move(commandLists[i]);
   }
   
   return true;
}

void DestroyResources(const Dx12Device *device)
{
   DX_VERIFY(device->fence->SetEventOnCompletion((UINT64)s_frameNum - 1, device->fenceEvent));
   WaitForSingleObject(device->fenceEvent, INFINITE);

   s_resources.pipelineState = nullptr;
   s_resources.rootSignature = nullptr;
   for (size_t i = 0; i < ARRAY_COUNT(device->frames); ++i) {
      s_resources.commandLists[i] = nullptr;
   }
}

void DrawFrame(const Dx12Device *device, float dt)
{
   uint64_t curFrame = s_frameNum++;
   DX_VERIFY(device->fence->SetEventOnCompletion(curFrame - ARRAY_COUNT(device->frames), device->fenceEvent));
   WaitForSingleObject(device->fenceEvent, INFINITE);

   UINT imageIdx = device->swapChain->GetCurrentBackBufferIndex();
   ASSERT(imageIdx < ARRAY_COUNT(s_resources.commandLists));

   DX_VERIFY(device->frames[imageIdx].commandAllocator->Reset());
   ID3D12GraphicsCommandList *commandList = s_resources.commandLists[imageIdx].Get();
   DX_VERIFY(commandList->Reset(device->frames[imageIdx].commandAllocator.Get(), s_resources.pipelineState.Get()));

   commandList->SetGraphicsRootSignature(s_resources.rootSignature.Get());

   D3D12_VIEWPORT viewport;
   viewport.TopLeftX = 0.0f;
   viewport.TopLeftY = 0.0f;
   viewport.Width = (float) device->surfaceWidth;
   viewport.Height = (float) device->surfaceHeight;
   viewport.MinDepth = D3D12_MIN_DEPTH;
   viewport.MaxDepth = D3D12_MAX_DEPTH;

   D3D12_RECT scissor;
   scissor.left = 0;
   scissor.top = 0;
   scissor.right = device->surfaceWidth;
   scissor.bottom = device->surfaceHeight;

   commandList->RSSetViewports(1, &viewport);
   commandList->RSSetScissorRects(1, &scissor);

   D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = device->frames[imageIdx].rtv;
   D3D12_RESOURCE_BARRIER resourceBarrier;
   resourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   resourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
   resourceBarrier.Transition.pResource = device->frames[imageIdx].renderTarget.Get();
   resourceBarrier.Transition.Subresource = 0;
   resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
   resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
   commandList->ResourceBarrier(1, &resourceBarrier);

   commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

   const float clearColor[] = { 0.086f, 0.086f, 0.1137f, 1.0f, };
   commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

   s_cubeRot += dt * CUBE_SPIN_SPEED;
   s_cubeRot -= floorf(s_cubeRot);

   ShaderMatrices matrices;
   Mat4 worldFromLocal, viewFromWorld, viewFromLocal, clipFromView;
   mat4RotY(&worldFromLocal, s_cubeRot * (2.0f * PI));

   Vec3 eye = { 0.0f, 1.5f, -3.0f };
   Vec3 target = { 0.0f, 0.0f, 0.0f };
   Vec3 up = { 0.0f, 1.0f, 0.0f };
   mat4LookAt(&viewFromWorld, eye, target, up);
   mat4Mul(&viewFromLocal, &viewFromWorld, &worldFromLocal);

   mat4PerspectiveFov(&clipFromView, PI / 2.0f, device->surfaceWidth / (float)device->surfaceHeight, 1.0f, 100.0f);
   mat4Mul(&matrices.clipFromLocal, &clipFromView, &viewFromLocal);

   commandList->SetGraphicsRoot32BitConstants(0, sizeof(matrices) / sizeof(UINT), &matrices, 0);
   commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   commandList->DrawInstanced(36, 1, 0, 0);

   resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
   resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
   commandList->ResourceBarrier(1, &resourceBarrier);

   DX_VERIFY(commandList->Close());

   ID3D12CommandList* commandLists[] = { commandList };
   device->commandQueue->ExecuteCommandLists(1, commandLists);

   DX_VERIFY(device->swapChain->Present(1, 0));
   DX_VERIFY(device->commandQueue->Signal(device->fence.Get(), curFrame));
}
