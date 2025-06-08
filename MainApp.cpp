#include "MainApp.h"
#include "Logger.h"
#include "Utils.h"
#include "Constants.h"
#include <d3d12.h>

#include <iostream>

namespace Lunar {

MainApp *g_mainApp = nullptr;

LRESULT WINAPI WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return g_mainApp->MessageProc(hWnd, msg, wParam, lParam);
}

MainApp::MainApp()
{
	g_mainApp = this;
	m_displayWidth = 1280;
	m_displayHeight = 720;
}

MainApp::~MainApp()
{
	g_mainApp = nullptr;

	DestroyWindow(m_mainWindow);
}

LRESULT MainApp::MessageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
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
			LOG_DEBUG("Mouse ", LOWORD(lParam), " ", HIWORD(lParam));
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
	
	m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocator.GetAddressOf()));
	
	m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(m_commandList.GetAddressOf()));

	m_commandList->Close();
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
	swapChainDesc.Width = m_displayWidth;
	swapChainDesc.Height = m_displayHeight;
	swapChainDesc.Format = Lunar::Constants::SWAP_CHAIN_FORMAT;
	swapChainDesc.SampleDesc.Count = Lunar::Constants::SAMPLE_COUNT; // not use MSAA for now
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = Lunar::Constants::BUFFER_COUNT;
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
	rtvHeapDesc.NumDescriptors = Lunar::Constants::BUFFER_COUNT;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 1;
	THROW_IF_FAILED(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf())))
}

void MainApp::CreateRenderTargetView()
{
	LOG_FUNCTION_ENTRY();
	
	m_rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (uint32_t i = 0; i < Lunar::Constants::BUFFER_COUNT; ++i)
	{
		ComPtr<ID3D12Resource> backBuffer;
		m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));
		m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_rtvHandle);

		// TODO
		backBuffer->Release();
	}
}

void MainApp::CreateRootSignature()
{
	/*
	typedef struct D3D12_ROOT_SIGNATURE_DESC
	{
		UINT NumParameters;
		_Field_size_full_(NumParameters)  const D3D12_ROOT_PARAMETER *pParameters;
		UINT NumStaticSamplers;
		_Field_size_full_(NumStaticSamplers)  const D3D12_STATIC_SAMPLER_DESC *pStaticSamplers;
		D3D12_ROOT_SIGNATURE_FLAGS Flags;
	} 	D3D12_ROOT_SIGNATURE_DESC;
	*/

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.NumParameters = 0;
	rootSignatureDesc.pParameters = nullptr;
	rootSignatureDesc.NumStaticSamplers = 0;
	rootSignatureDesc.pStaticSamplers = nullptr;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	
	ComPtr<ID3DBlob> signature = nullptr;
	ComPtr<ID3DBlob> error = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
										   signature.GetAddressOf(), error.GetAddressOf());
	
	if (FAILED(hr))
	{
		if (error != nullptr)
		{
			LOG_ERROR("Failed to serialize root signature: ", 
					static_cast<char*>(error->GetBufferPointer()));
		}
		return;
	}
	
	THROW_IF_FAILED(m_device->CreateRootSignature(0, signature->GetBufferPointer(),
											   signature->GetBufferSize(),
											   IID_PPV_ARGS(m_rootSignature.GetAddressOf())))
}

int MainApp::Run()
{
	LOG_FUNCTION_ENTRY();
	MSG msg = { 0 };
	while (WM_QUIT != msg.message) 
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			Update();
			Render();
		}
	}
	LOG_FUNCTION_EXIT();
	return 0;
}

void MainApp::Update()
{
}


void MainApp::Render()
{

}

void MainApp::Initialize()
{
	LOG_FUNCTION_ENTRY();
	InitMainWindow();
	InitDirect3D();
	InitializeCommandList();
	CreateSwapChain();
	CreateRTVDescriptorHeap();
	CreateRenderTargetView();
	CreateRootSignature();
	Run();
}

bool MainApp::InitDirect3D()
{
	LOG_FUNCTION_ENTRY();

	ComPtr<ID3D12Device> tempDevice;

	UINT createFactoryFlags = 0;
#if defined(_DEBUG) | defined(DEBUG)
	// createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
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

		LOG_DEBUG("Selected GPU: ", std::string(std::begin(desc.Description), std::end(desc.Description)), " (", desc.DedicatedVideoMemory >> 20, " MB)");
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

} // namespace Lunar

