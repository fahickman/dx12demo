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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "dx12demo.h"

void DrawFrame(const Dx12Device *device, float dt);
bool CreateResources(const Dx12Device *device);
void DestroyResources(const Dx12Device *device);

Dx12 s_dx12;
Dx12Device s_device;

static bool createDescriptorHeap(Dx12DescriptorHeap *heap, ID3D12Device *device,
   D3D12_DESCRIPTOR_HEAP_TYPE type, UINT descriptorCount)
{
   ASSERT(heap);
   ASSERT(device);

   D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
   heapDesc.NumDescriptors = descriptorCount;
   heapDesc.Type = type;
   heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
   heapDesc.NodeMask = 0;
   if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap->heap)))) {
      return false;
   }

   heap->type = type;
   heap->descriptorCount = descriptorCount;
   heap->cpuStart = heap->heap->GetCPUDescriptorHandleForHeapStart();
   heap->gpuStart = heap->heap->GetGPUDescriptorHandleForHeapStart();
   heap->increment = device->GetDescriptorHandleIncrementSize(type);
   return true;
}

static bool createSwapChain(Dx12Device *device, HWND hwnd)
{
   ASSERT(device && device->dx12 && device->device);
   const Dx12 *dx12 = device->dx12;

   RECT rect;
   GetClientRect(hwnd, &rect);
   device->surfaceWidth = rect.right - rect.left;
   device->surfaceHeight = rect.bottom - rect.top;

   DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
   swapChainDesc.BufferCount = (UINT)ARRAY_COUNT(device->frames);
   swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
   swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
   swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
   swapChainDesc.OutputWindow = hwnd;
   swapChainDesc.SampleDesc.Count = 1;
   swapChainDesc.Windowed = TRUE;

   ComPtr<IDXGISwapChain> swapChain;
   if (FAILED(dx12->factory->CreateSwapChain(device->commandQueue.Get(), &swapChainDesc, &swapChain))) {
      return false;
   }

   if (FAILED(dx12->factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER))) {
      return false;
   }

   if (!createDescriptorHeap(&device->rtvHeap, device->device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, swapChainDesc.BufferCount)) {
      return false;
   }

   // create render target views
   {
      D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = device->rtvHeap.cpuStart;
      D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
      rtvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
      rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
      rtvDesc.Texture2D.MipSlice = 0;
      rtvDesc.Texture2D.PlaneSlice = 0;

      for (std::size_t i = 0; i < ARRAY_COUNT(device->frames); ++i) {
         if (FAILED(device->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&device->frames[i].commandAllocator)))) {
            return false;
         }

         if (FAILED(swapChain->GetBuffer((UINT)i, IID_PPV_ARGS(&device->frames[i].renderTarget)))) {
            return false;
         }

         device->device->CreateRenderTargetView(device->frames[i].renderTarget.Get(), &rtvDesc, rtvHandle);
         device->frames[i].rtv = rtvHandle;
         rtvHandle.ptr += device->rtvHeap.increment;
      }
   }

   if (!swapChain.As(&device->swapChain)) {
      return false;
   }

   return CreateResources(device);
}

static void destroySwapChain(Dx12Device *device)
{
   ASSERT(device && device->device);

   DestroyResources(device);
   for (std::size_t i = 0; i < ARRAY_COUNT(device->frames); ++i) {
      device->frames[i].commandAllocator = nullptr;
      device->frames[i].renderTarget = nullptr;
      device->frames[i].rtv.ptr = 0;
   }

   device->rtvHeap.heap = nullptr;
   device->swapChain = nullptr;
}

static bool initD3d(Dx12 *dx12)
{
   UINT dxgiFactoryFlags = 0;

#ifndef NDEBUG
   ComPtr<ID3D12Debug> debugController;
   if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
      debugController->EnableDebugLayer();
      dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
   }
#endif

   ComPtr<IDXGIFactory4> factory;
   if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)))) {
      return false;
   }

   ComPtr<IDXGIAdapter1> adapter;
   dx12->adapters.reserve(8);
   dx12->adapterDescs.reserve(8);
   while (SUCCEEDED(factory->EnumAdapters1((UINT)dx12->adapters.size(), &adapter))) {
      dx12->adapterDescs.resize(dx12->adapterDescs.size() + 1);
      adapter->GetDesc1(&dx12->adapterDescs.back());

      dx12->adapters.emplace_back(std::move(adapter));
   }

   dx12->factory = std::move(factory);
   return true;
}

static void uninitD3d(Dx12 *dx12)
{
   if (dx12) {
      dx12->adapters.clear();
      dx12->adapterDescs.clear();
      dx12->factory = nullptr;
   }
}

static bool createDevice(const Dx12 *dx12, Dx12Device *device)
{
   // look for a hardware adapter
   std::size_t deviceIdx = dx12->adapterDescs.size();
   for (std::size_t i = 0; i < deviceIdx; ++i) {
      if ((dx12->adapterDescs[i].Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
         deviceIdx = i;
      }
   }

   if (deviceIdx == dx12->adapters.size()) {
      return false;
   }

   ComPtr<ID3D12Device> d3dDevice;
   if (FAILED(D3D12CreateDevice(dx12->adapters[deviceIdx].Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3dDevice)))) {
      return false;
   }

   D3D12_COMMAND_QUEUE_DESC queueDesc = {};
   queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
   queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

   ComPtr<ID3D12CommandQueue> commandQueue;
   if (FAILED(d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)))) {
      return false;
   }

   ComPtr<ID3D12Fence> fence;
   if (FAILED(d3dDevice->CreateFence(ARRAY_COUNT(device->frames) - 1, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
      return false;
   }

   device->fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
   device->commandQueue = std::move(commandQueue);
   device->fence = std::move(fence);
   device->device = std::move(d3dDevice);
   device->deviceIdx = (uint32_t) deviceIdx;
   device->dx12 = dx12;
   return true;
}

static void destroyDevice(Dx12Device *device)
{
   if (device) {
      destroySwapChain(device);
      CloseHandle(device->fenceEvent);
      device->fenceEvent = NULL;
      device->fence = nullptr;
      device->commandQueue = nullptr;
      device->device = nullptr;
   }
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   static LARGE_INTEGER s_lastTime, s_freq;

   switch (msg) {
   case WM_CREATE:
      QueryPerformanceFrequency(&s_freq);
      QueryPerformanceCounter(&s_lastTime);
      return 0;
   case WM_CLOSE:
      PostQuitMessage(0);
      return 0;
   case WM_PAINT:
   {
      PAINTSTRUCT ps;
      LARGE_INTEGER curTime;

      QueryPerformanceCounter(&curTime);
      double dt = (curTime.QuadPart - s_lastTime.QuadPart) / (double)s_freq.QuadPart;
      s_lastTime = curTime;

      BeginPaint(hwnd, &ps);
      DrawFrame(&s_device, (float)dt);
      EndPaint(hwnd, &ps);
      return 0;
   }
   case WM_SIZE:
      if (wParam != SIZE_MINIMIZED) {
         destroySwapChain(&s_device);
         createSwapChain(&s_device, hwnd);
      }
      return 0;
   default:
      return DefWindowProc(hwnd, msg, wParam, lParam);
   }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int nShowCmd)
{
   if (!initD3d(&s_dx12)) {
      //FIXME: better error message.
      MessageBox(NULL, L"Could not initialize Direct3D.", L"Error", MB_ICONERROR | MB_OK);
      return -1;
   }
   if (!createDevice(&s_dx12, &s_device)) {
      MessageBox(NULL, L"Could not find suitable Direct3D 12 device.", L"Error", MB_ICONERROR | MB_OK);
      uninitD3d(&s_dx12);
      return -1;
   }

   WNDCLASSEX wcex;
   wcex.cbSize = sizeof(wcex);
   wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
   wcex.lpfnWndProc = wndProc;
   wcex.cbClsExtra = 0;
   wcex.cbWndExtra = 0;
   wcex.hInstance = hInstance;
   wcex.hIcon = NULL;
   wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
   wcex.hbrBackground = NULL;
   wcex.lpszMenuName = NULL;
   wcex.lpszClassName = L"dx12demo.mainWnd";
   wcex.hIconSm = NULL;

   ATOM atom = RegisterClassEx(&wcex);
   if (!atom) {
      destroyDevice(&s_device);
      uninitD3d(&s_dx12);
      return -1;
   }

   HWND hwnd = CreateWindowEx(0, MAKEINTATOM(atom), L"DX12", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
   if (!hwnd) {
      destroyDevice(&s_device);
      uninitD3d(&s_dx12);
      return -1;
   }

   ShowWindow(hwnd, nShowCmd);

   MSG msg = { 0 };
   while (msg.message != WM_QUIT) {
      while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }
      RedrawWindow(hwnd, NULL, NULL, RDW_INTERNALPAINT);
   }

   destroyDevice(&s_device);
   uninitD3d(&s_dx12);
   return (int)msg.wParam;
}