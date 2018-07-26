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

#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <atlbase.h>

#include <array>
#include <stdint.h>
#include <vector>

#ifdef NDEBUG
#  define ASSERT(x) __assume(x)
#else
#  include <assert.h>
#  define ASSERT(x) assert(x)
#endif

#define DX_VERIFY(x) do { HRESULT res = (x); ASSERT(SUCCEEDED(res)); } while(0)
#define ARRAY_COUNT(a)  (uint32_t)(sizeof(a)/sizeof((a)[0]))

// Like ATL's CComPtr but with just the stuff we need.
//
template <class T>
class ComPtr {
   T *m_ptr;

   inline void attach(T *ptr)
   {
      ASSERT(ptr == nullptr || ptr != m_ptr);

      if (m_ptr) {
         m_ptr->Release();
      }
      m_ptr = ptr;
   }

   inline T *detach()
   {
      T *ptr = m_ptr;
      m_ptr = nullptr;
      return ptr;
   }

public:
   inline ComPtr() : m_ptr(nullptr) {}
   inline ComPtr(T *ptr) : m_ptr(ptr)
   {
      if (ptr) {
         ptr->AddRef();
      }
   }
   inline ComPtr(const ComPtr &ptr) : ComPtr(ptr.m_ptr) { }
   inline ComPtr(ComPtr &&ptr) : m_ptr(ptr.detach()) { }

   inline ~ComPtr()
   {
      attach(nullptr);
   }

   inline T *Get() const
   {
      return m_ptr;
   }

   template <class U>
   inline bool As(U **pp) const
   {
      ASSERT(m_ptr);
      return m_ptr->QueryInterface(IID_PPV_ARGS(pp)) >= 0;
   }

   inline ComPtr &operator=(T *rhs)
   {
      if (rhs) {
         rhs->AddRef();
      }
      attach(rhs);
      return *this;
   }

   inline ComPtr &operator=(const ComPtr &rhs)
   {
      return *this = rhs.m_ptr;
   }

   inline ComPtr &operator=(ComPtr &&rhs)
   {
      attach(rhs.detach());
      return *this;
   }

   inline T **operator&()
   {
      ASSERT(m_ptr == nullptr);  // A Release() could get missed if m_ptr is non-null
      return &m_ptr;
   }

   inline T *operator->() const
   {
      ASSERT(m_ptr);
      return m_ptr;
   }

   inline operator bool() const
   {
      return m_ptr != nullptr;
   }
};

struct Dx12 {
   ComPtr<IDXGIFactory4> factory;

   std::vector<ComPtr<IDXGIAdapter1>> adapters;
   std::vector<DXGI_ADAPTER_DESC1> adapterDescs;
};

struct Dx12Frame {
   ComPtr<ID3D12CommandAllocator> commandAllocator;
   ComPtr<ID3D12Resource> renderTarget;
   D3D12_CPU_DESCRIPTOR_HANDLE rtv;
};

struct Dx12DescriptorHeap {
   ComPtr<ID3D12DescriptorHeap> heap;
   D3D12_DESCRIPTOR_HEAP_TYPE type;
   UINT descriptorCount;

   D3D12_CPU_DESCRIPTOR_HANDLE cpuStart;
   D3D12_GPU_DESCRIPTOR_HANDLE gpuStart;
   UINT increment;
};

struct Dx12Device {
   const Dx12 *dx12;

   uint32_t deviceIdx; // index into dx12->adapters/adapterDescs
   ComPtr<ID3D12Device> device;
   ComPtr<ID3D12CommandQueue> commandQueue;
   ComPtr<ID3D12Fence> fence;
   HANDLE fenceEvent;

   Dx12DescriptorHeap rtvHeap;

   Dx12Frame frames[2];
   ComPtr<IDXGISwapChain3> swapChain;

   uint32_t surfaceWidth;
   uint32_t surfaceHeight;
};