// #include <d3d12.h>
#include <array>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <filesystem>

#include "MainApp.h"
#include "Camera.h"
#include "Logger.h"
#include "Utils.h"
#include "LunarConstants.h"
#include "ConstantBuffers.h"
#include "LunarGui.h"
#include "SceneRenderer.h"
#include "PipelineStateManager.h"
#include "Geometry/Transform.h"

using namespace std;
using namespace DirectX;
using namespace Microsoft::WRL;

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Lunar {

MainApp *g_mainApp = nullptr;

LRESULT WINAPI WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return g_mainApp->MessageProc(hWnd, msg, wParam, lParam);
}

MainApp::MainApp()
{
	g_mainApp = this;
	m_sceneRenderer = make_unique<SceneRenderer>();
	m_pipelineStateManager = make_unique<PipelineStateManager>();
}

MainApp::~MainApp()
{
	g_mainApp = nullptr;

	DestroyWindow(m_mainWindow);
}

LRESULT MainApp::MessageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Handle ImGui input first
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return 0;
	
	switch (msg)
	{
		case WM_SIZE:
			// Reset and resize swapchain
			break;
		case WM_SYSCOMMAND:
			if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
				return 0;
			break;
		case WM_MOUSEMOVE:
			// LOG_DEBUG("Mouse ", LOWORD(lParam), " ", HIWORD(lParam));
            if (m_mouseMoving) OnMouseMove(LOWORD(lParam), HIWORD(lParam));
			break;
		case WM_LBUTTONUP:
			LOG_DEBUG("Left mouse button");
			break;
		case WM_RBUTTONUP:
			LOG_DEBUG("Right mouse button");
			break;
		case WM_KEYDOWN:
			LOG_DEBUG("Key ", (int)wParam, " down");
			break;
		case WM_DESTROY:
			::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

void MainApp::OnMouseMove(float x, float y)
{
    if (m_firstMouseMove) 
    {
        m_lastMouseX = x;
        m_lastMouseY = y;
        m_firstMouseMove = false;
        return;
    }

    // TODO : should compare with threshold?
    float dx = x - m_lastMouseX;
    float dy = y - m_lastMouseY;
	m_lastMouseX = x;
	m_lastMouseY = y;
    m_camera->UpdateRotationQuatFromMouse(dx, dy);
}

void MainApp::InitGui()
{
	LOG_FUNCTION_ENTRY();

    m_gui = std::make_unique<LunarGui>();
	
	// Create descriptor heap for ImGui
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	THROW_IF_FAILED(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_imGuiDescriptorHeap.GetAddressOf())));
	
    m_gui->Initialize(
        m_mainWindow,
        m_device.Get(),
        Lunar::LunarConstants::BUFFER_COUNT,
        Lunar::LunarConstants::SWAP_CHAIN_FORMAT,
        m_imGuiDescriptorHeap.Get());
}

void MainApp::InitializeCommandList()
{
	LOG_FUNCTION_ENTRY();
/*	
	typedef struct D3D12_COMMAND_QUEUE_DESC
	{
		D3D12_COMMAND_LIST_TYPE Type;
		INT Priority;
		D3D12_COMMAND_QUEUE_FLAGS Flags;
		UINT NodeMask;
	} 	D3D12_COMMAND_QUEUE_DESC;
	
	typedef 
	enum D3D12_COMMAND_LIST_TYPE
		{
			D3D12_COMMAND_LIST_TYPE_DIRECT	= 0,
			D3D12_COMMAND_LIST_TYPE_BUNDLE	= 1,
			D3D12_COMMAND_LIST_TYPE_COMPUTE	= 2,
			D3D12_COMMAND_LIST_TYPE_COPY	= 3,
			D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE	= 4,
			D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS	= 5,
			D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE	= 6
		} 	D3D12_COMMAND_LIST_TYPE;
	
	typedef 
	enum D3D12_COMMAND_QUEUE_FLAGS
		{
			D3D12_COMMAND_QUEUE_FLAG_NONE	= 0,
			D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT	= 0x1
		} 	D3D12_COMMAND_QUEUE_FLAGS;
	*/
	
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_commandQueue.GetAddressOf()));
	
	THROW_IF_FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocator.GetAddressOf())))
	
	THROW_IF_FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(m_commandList.GetAddressOf())))

	// m_commandList->Close();
}

void MainApp::CreateSwapChain()
{
	LOG_FUNCTION_ENTRY();
	/*
	typedef struct DXGI_SWAP_CHAIN_DESC1
	{
		UINT Width;
		UINT Height;
		DXGI_FORMAT Format; // pixel format
		BOOL Stereo;
		DXGI_SAMPLE_DESC SampleDesc;
		DXGI_USAGE BufferUsage;
		UINT BufferCount;
		DXGI_SCALING Scaling;
		DXGI_SWAP_EFFECT SwapEffect;
		DXGI_ALPHA_MODE AlphaMode;
		UINT Flags;
	} 	DXGI_SWAP_CHAIN_DESC1;
	*/
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = Utils::GetDisplayWidth();
	swapChainDesc.Height = Utils::GetDisplayHeight();
	swapChainDesc.Format = Lunar::LunarConstants::SWAP_CHAIN_FORMAT;
	swapChainDesc.SampleDesc.Count = Lunar::LunarConstants::SAMPLE_COUNT; // not use MSAA for now
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = Lunar::LunarConstants::BUFFER_COUNT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	THROW_IF_FAILED(m_factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),
		m_mainWindow,					// window handle
		&swapChainDesc,                 // swap chain description
		nullptr,						// full screen description
		nullptr,						// restrict to output description
		m_swapChain.GetAddressOf()      // Created Swap Chain
		))
	
}

void MainApp::CreateRTVDescriptorHeap()
{
	LOG_FUNCTION_ENTRY();
	/*
	typedef struct D3D12_DESCRIPTOR_HEAP_DESC
	{
		D3D12_DESCRIPTOR_HEAP_TYPE Type;
		UINT NumDescriptors;
		D3D12_DESCRIPTOR_HEAP_FLAGS Flags;
		UINT NodeMask;
	} 	D3D12_DESCRIPTOR_HEAP_DESC;
	*/
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = Lunar::LunarConstants::BUFFER_COUNT;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 1;
	THROW_IF_FAILED(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf())))

	m_renderTargetViewDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void MainApp::CreateRenderTargetView()
{
	LOG_FUNCTION_ENTRY();
	
	m_rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (uint32_t i = 0; i < Lunar::LunarConstants::BUFFER_COUNT; ++i)
	{
		ComPtr<ID3D12Resource> backBuffer;
		m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));
		m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_rtvHandle);

		m_renderTargets[i] = backBuffer;
		
		// Increment the handle for the next RTV
		m_rtvHandle.ptr += m_renderTargetViewDescriptorSize;
	}
}

// D3D_COMPILE_STANDARD_FILE_INCLUDE can be passed for pInclude in any
// API and indicates that a simple default include handler should be
// used.  The include handler will include files relative to the
// current directory and files relative to the directory of the initial source
// file.  When used with APIs like D3DCompile pSourceName must be a
// file name and the initial relative directory will be derived from it.
#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)

void MainApp::CreateFence()
{
	LOG_FUNCTION_ENTRY();
	m_fenceValue = 0;
	THROW_IF_FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.GetAddressOf())))

	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

int MainApp::Run()
{
	LOG_FUNCTION_ENTRY();
	
	m_lunarTimer.Reset();
	
	MSG msg = { 0 };
	while (WM_QUIT != msg.message) 
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			m_lunarTimer.Tick();
			double deltaTime = m_lunarTimer.GetDeltaTime();
			Update(deltaTime);
			Render(deltaTime);
		}
	}
	LOG_FUNCTION_EXIT();
	return 0;
}

void MainApp::Update(double dt)
{
    ProcessInput(dt);

	BasicConstants& constants = m_sceneRenderer->GetBasicConstants();
	XMStoreFloat4x4(&constants.view, XMMatrixTranspose(XMLoadFloat4x4(&m_camera->GetViewMatrix())));
	XMStoreFloat4x4(&constants.projection, XMMatrixTranspose(XMLoadFloat4x4(&m_camera->GetProjMatrix())));
	constants.eyePos = m_camera->GetPosition();

    m_sceneRenderer->UpdateScene(dt);
}

void MainApp::ProcessInput(double dt)
{
    bool cameraDirty = false;
    float cameraDeltaFront = 0.0;
    float cameraDeltaRight = 0.0;
    if (GetAsyncKeyState('W') & 0x8000) 
    {
        cameraDeltaFront += dt;
        cameraDirty = true;
    }
    if (GetAsyncKeyState('S') & 0x8000)
    {
        cameraDeltaFront -= dt;
        cameraDirty = true;
    }
    if (GetAsyncKeyState('A') & 0x8000)
    {
        cameraDeltaRight -= dt;
        cameraDirty = true;
    }
    if (GetAsyncKeyState('D') & 0x8000)
    {
        cameraDeltaRight += dt;
        cameraDirty = true;
    }
	if (GetAsyncKeyState('F') & 0x8000)
	{
		m_mouseMoving = !m_mouseMoving;
	}

    if (cameraDirty)
    {
        m_camera->UpdatePosition(cameraDeltaRight, 0.0f, cameraDeltaFront);
    }
}

void MainApp::Render(double dt)
{
	ComPtr<IDXGISwapChain3> swapChain3;
	THROW_IF_FAILED(m_swapChain.As(&swapChain3));
	m_frameIndex = swapChain3->GetCurrentBackBufferIndex();

	m_commandAllocator->Reset(); // Cautions!
	m_commandList->Reset(m_commandAllocator.Get(), nullptr);

	m_commandList->SetGraphicsRootSignature(m_pipelineStateManager->GetRootSignature());
	// Set Constant Buffer Descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_sceneRenderer->GetSRVHeap() };
	m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	m_commandList->SetGraphicsRootDescriptorTable(0, m_sceneRenderer->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart());
	
	m_sceneRenderer->RenderShadowMap(m_commandList.Get());

	m_viewport = {};
	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;
	m_viewport.Width = static_cast<float>(Utils::GetDisplayWidth());
	m_viewport.Height = static_cast<float>(Utils::GetDisplayHeight());
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	m_commandList->RSSetViewports(1, &m_viewport);

	m_scissorRect = {};
	m_scissorRect.left = 0;
	m_scissorRect.top = 0;
	m_scissorRect.right = static_cast<LONG>(Utils::GetDisplayWidth());
	m_scissorRect.bottom = static_cast<LONG>(Utils::GetDisplayHeight());
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// barrier
	/*
	typedef struct D3D12_RESOURCE_BARRIER
	{
		D3D12_RESOURCE_BARRIER_TYPE Type;
		D3D12_RESOURCE_BARRIER_FLAGS Flags;
		union 
		{
			D3D12_RESOURCE_TRANSITION_BARRIER Transition;
			D3D12_RESOURCE_ALIASING_BARRIER Aliasing;
			D3D12_RESOURCE_UAV_BARRIER UAV;
		} 	;
	} 	D3D12_RESOURCE_BARRIER;
	*/
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_commandList->ResourceBarrier(1, &barrier);

	// Set Render Target
	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	renderTargetViewHandle.ptr += m_frameIndex * m_renderTargetViewDescriptorSize;
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle = m_sceneRenderer->GetDSVHeap()->GetCPUDescriptorHandleForHeapStart();
	m_commandList->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, &depthStencilViewHandle);

	// clear
	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_commandList->ClearRenderTargetView(renderTargetViewHandle, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(depthStencilViewHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	m_sceneRenderer->RenderScene(m_commandList.Get());
	
    m_gui->Render(dt);
	ID3D12DescriptorHeap* heaps[] = { m_imGuiDescriptorHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

	// resource barrier - transition to the present state
	barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_commandList->ResourceBarrier(1, &barrier);


	m_commandList->Close();

	// Execute Command List
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// present to window
	m_swapChain->Present(1, 0);

	// ready for next frame
	const UINT64 currentFenceValue = m_fenceValue;
	m_commandQueue->Signal(m_fence.Get(), currentFenceValue);
	m_fenceValue++;

	// wait for completing current frame
	if (m_fence->GetCompletedValue() < currentFenceValue)
	{
		THROW_IF_FAILED(m_fence->SetEventOnCompletion(currentFenceValue, m_fenceEvent))
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	// update next frame index
	m_frameIndex = swapChain3->GetCurrentBackBufferIndex();
}

void MainApp::Initialize()
{
	LOG_FUNCTION_ENTRY();
	InitMainWindow();
	InitDirect3D();
	InitializeCommandList();
	InitGui();
	CreateFence();
	CreateCamera();
	CreateSwapChain();
	CreateRTVDescriptorHeap();
	CreateRenderTargetView();
	InitializeGeometry();
	m_pipelineStateManager->Initialize(m_device.Get());
	InitializeTextures(); // for now, should come after InitializeGeometry method
}

bool MainApp::InitDirect3D()
{
	LOG_FUNCTION_ENTRY();

	ComPtr<ID3D12Device> tempDevice;

	UINT createFactoryFlags = 0;
#if defined(_DEBUG) || defined(DEBUG)
{
	ComPtr<ID3D12Debug> debugController;
	THROW_IF_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))
	debugController->EnableDebugLayer();
}
#endif
	
	THROW_IF_FAILED(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(m_factory.GetAddressOf())))

	SIZE_T MaxSize = 0; // for finding the adapter with the most memory
	for (uint32_t adapterIndex = 0; DXGI_ERROR_NOT_FOUND != m_factory->EnumAdapters1(adapterIndex, &m_adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		m_adapter->GetDesc1(&desc);

		// Is a software adapter?
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		// Can create a D3D12 device?
		THROW_IF_FAILED(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&tempDevice)))

		if (desc.DedicatedVideoMemory < MaxSize)
			continue;

		MaxSize = desc.DedicatedVideoMemory;
		if (m_device != nullptr)
			m_device->Release();

		m_device = tempDevice.Detach();

		// LOG_DEBUG("Selected GPU: ", std::string(std::begin(desc.Description), std::end(desc.Description)), " (", desc.DedicatedVideoMemory >> 20, " MB)");
	}
	
	if (m_device == nullptr)
	{
		LOG_ERROR("Failed to find suitable GPU adapter");
		return false;
	}
	
	return true;
}

bool MainApp::InitMainWindow()
{
	LOG_FUNCTION_ENTRY();
	WNDCLASSEX wc = {
		sizeof(WNDCLASSEX),
		CS_CLASSDC,
		WindowProc,
		0,
		0,
		GetModuleHandle(NULL),
		NULL,
		NULL,
		NULL,
		NULL,
		L"MainApp",
		NULL
	};

	if (!RegisterClassEx(&wc))
	{
		LOG_ERROR("RegisterClassEx() Failed");
		return false;
	}

	RECT wr = {0, 0, 1280, 720};
	m_mainWindow = CreateWindow(wc.lpszClassName,
								L"Lunar App",
								WS_OVERLAPPEDWINDOW,
								0,
								0,
								wr.right - wr.left,
								wr.bottom - wr.top,
								NULL,
								NULL,
								wc.hInstance,
								NULL);
	if (!m_mainWindow)
	{
		LOG_ERROR("CreateWindow() Failed");
		return false;
	}

	ShowWindow(m_mainWindow, SW_SHOW);
	UpdateWindow(m_mainWindow);

	return true;
}

void MainApp::InitializeGeometry()
{
	LOG_FUNCTION_ENTRY();
	
    // TODO: Check really need this resetting?
	m_commandAllocator->Reset();
	m_commandList->Reset(m_commandAllocator.Get(), nullptr);
    Transform transform = {};
    transform.Location = XMFLOAT3(0.0f, 1.5f, 0.0f);
    m_sceneRenderer->AddSphere("Sphere0", transform, RenderLayer::World);
    // m_sceneRenderer->AddCube("Cube0", transform, RenderLayer::World);
	transform.Scale = XMFLOAT3(10.0f, 0.1f, 10.0f);
	Transform mirrorTransform = transform;
	mirrorTransform.Location = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mirrorTransform.Rotation = XMFLOAT3(-XM_PIDIV2, 0.0f, 0.0f);
	//m_sceneRenderer->AddPlane("Mirror0", mirrorTransform, 0.1f, 0.2f, RenderLayer::Mirror);
	//m_sceneRenderer->AddTree("Tree0");
	/*m_sceneRenderer->AddCube("Cube1", {{0, 1, 0}, {0, 0, 0}, {1, 1, 1}}, RenderLayer::World);
	m_sceneRenderer->AddCube("Cube2", {{2, 1, 2}, {0, 0, 0}, {1, 1, 1}}, RenderLayer::World);
	m_sceneRenderer->AddCube("Cube3", {{-2, 1, -2}, {0, 0, 0}, {1, 1, 1}}, RenderLayer::World);*/
	m_sceneRenderer->AddPlane("ShadowMapPlane", {{0, 0, 0}, {0, 0, 0}, {3, 3, 1}}, 3.0f, 3.0f, RenderLayer::World);
	transform.Location = XMFLOAT3(0.0f, 0.0f, 0.0f);
	transform.Scale = XMFLOAT3(50.0f, 50.0f, 50.0f);
	m_sceneRenderer->AddSphere("SkyBox0", transform, RenderLayer::Background);

	m_sceneRenderer->InitializeScene(m_device.Get(), m_gui.get(), m_pipelineStateManager.get());
}

void MainApp::CreateCamera()
{
	LOG_FUNCTION_ENTRY();
	m_camera = std::make_unique<Camera>();
}

void MainApp::InitializeTextures()
{
	m_sceneRenderer->InitializeTextures(m_device.Get(), m_commandList.Get());
}

} // namespace Lunar