#include <d3d12.h>
#include <array>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include "MainApp.h"
#include "Camera.h"
#include "Logger.h"
#include "Utils.h"
#include "LunarConstants.h"
#include "ConstantBuffers.h"
#include "Cube.h"
#include "LunarGui.h"
#include <filesystem>

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
        Lunar::Constants::BUFFER_COUNT,
        Lunar::Constants::SWAP_CHAIN_FORMAT,
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

void MainApp::CreateSRVDescriptorHeap()
{
	LOG_FUNCTION_ENTRY();
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	THROW_IF_FAILED(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(m_srvHeap.GetAddressOf())))
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

	m_renderTargetViewDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
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

		m_renderTargets[i] = backBuffer;
		
		// Increment the handle for the next RTV
		m_rtvHandle.ptr += m_renderTargetViewDescriptorSize;
	}
}

void MainApp::CreateShaderResourceView()
{
	LOG_FUNCTION_ENTRY();
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	/*
	struct D3D12_SHADER_RESOURCE_VIEW_DESC {
		DXGI_FORMAT Format;                    // Pixel format of the resource data
											  // Use DXGI_FORMAT_UNKNOWN to use resource's original format
											  // Can specify different format for type conversion
	
		D3D12_SRV_DIMENSION ViewDimension;    // Specifies the resource type and how shader will access it
											  // Common values:
											  // - D3D12_SRV_DIMENSION_TEXTURE1D: 1D texture
											  // - D3D12_SRV_DIMENSION_TEXTURE2D: 2D texture
											  // - D3D12_SRV_DIMENSION_TEXTURE3D: 3D texture
											  // - D3D12_SRV_DIMENSION_TEXTURECUBE: Cube map
											  // - D3D12_SRV_DIMENSION_BUFFER: Raw buffer
	
		UINT Shader4ComponentMapping;         // Controls how texture components (RGBA) are mapped to shader
											  // Use D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING for standard RGBA mapping
											  // Can rearrange/swizzle components (e.g., BGRA to RGBA)
	
		union {
			D3D12_BUFFER_SRV Buffer;                    // Used when ViewDimension is BUFFER
			D3D12_TEX1D_SRV Texture1D;                 // Used for 1D textures
			D3D12_TEX1D_ARRAY_SRV Texture1DArray;      // Used for 1D texture arrays
			D3D12_TEX2D_SRV Texture2D;                 // Used for 2D textures (most common)
			D3D12_TEX2D_ARRAY_SRV Texture2DArray;      // Used for 2D texture arrays
			D3D12_TEX2DMS_SRV Texture2DMS;             // Used for multisampled 2D textures
			D3D12_TEX2DMS_ARRAY_SRV Texture2DMSArray;  // Used for multisampled 2D texture arrays
			D3D12_TEX3D_SRV Texture3D;                 // Used for 3D textures
			D3D12_TEXCUBE_SRV TextureCube;              // Used for cube map textures
			D3D12_TEXCUBE_ARRAY_SRV TextureCubeArray;  // Used for cube map texture arrays
		};
	};
	
	// D3D12_TEX2D_SRV structure (most commonly used for 2D textures)
	struct D3D12_TEX2D_SRV {
		UINT MostDetailedMip;        // Index of the most detailed mipmap level to use
									 // 0 = highest resolution level
	
		UINT MipLevels;              // Number of mipmap levels to use
									 // Use -1 to use all available mip levels from MostDetailedMip
									 // Use 1 for single mip level (no mipmapping)
	
		UINT PlaneSlice;             // For planar formats, specifies which plane to access
									 // Use 0 for non-planar formats (most common case)
	
		FLOAT ResourceMinLODClamp;   // Minimum LOD (Level of Detail) clamp value
									 // Prevents shader from sampling below this mip level
									 // Use 0.0f for no clamping (most common)
	};
	*/
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	m_srvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
	m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHandle);
}

void MainApp::CreateRootSignature()
{
	/*
	typedef struct D3D12_DESCRIPTOR_RANGE
	{
		D3D12_DESCRIPTOR_RANGE_TYPE RangeType;
		UINT NumDescriptors;
		UINT BaseShaderRegister;
		UINT RegisterSpace;
		UINT OffsetInDescriptorsFromTableStart;
	} 	D3D12_DESCRIPTOR_RANGE;
	*/
	D3D12_DESCRIPTOR_RANGE srvTable = {};
	cbvTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	cbvTable.NumDescriptors = 1;
	cbvTable.BaseShaderRegister = 0;
	cbvTable.RegisterSpace = 0;
	cbvTable.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	
	/*
	typedef struct D3D12_ROOT_PARAMETER
	{
		D3D12_ROOT_PARAMETER_TYPE ParameterType;
		union 
		{
			D3D12_ROOT_DESCRIPTOR_TABLE DescriptorTable;
			D3D12_ROOT_CONSTANTS Constants;
			D3D12_ROOT_DESCRIPTOR Descriptor;
		} 	;
		D3D12_SHADER_VISIBILITY ShaderVisibility;
	} 	D3D12_ROOT_PARAMETER;
	*/
	D3D12_ROOT_PARAMETER rootParameters[4];
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &srvTable;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].Descriptor.RegisterSpace = 0;
	rootParameters[1].Descriptor.ShaderRegister = 0;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].Descriptor.RegisterSpace = 0;
	rootParameters[2].Descriptor.ShaderRegister = 1;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].Descriptor.RegisterSpace = 0;
	rootParameters[3].Descriptor.ShaderRegister = 2;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	/*
	typedef struct D3D12_STATIC_SAMPLER_DESC
	{
		D3D12_FILTER Filter;
		D3D12_TEXTURE_ADDRESS_MODE AddressU;
		D3D12_TEXTURE_ADDRESS_MODE AddressV;
		D3D12_TEXTURE_ADDRESS_MODE AddressW;
		FLOAT MipLODBias;
		UINT MaxAnisotropy;
		D3D12_COMPARISON_FUNC ComparisonFunc;
		D3D12_STATIC_BORDER_COLOR BorderColor;
		FLOAT MinLOD;
		FLOAT MaxLOD;
		UINT ShaderRegister;
		UINT RegisterSpace;
		D3D12_SHADER_VISIBILITY ShaderVisibility;
	} 	D3D12_STATIC_SAMPLER_DESC;
	*/
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 1;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	
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
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignatureDesc.pStaticSamplers = &sampler;
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

// D3D_COMPILE_STANDARD_FILE_INCLUDE can be passed for pInclude in any
// API and indicates that a simple default include handler should be
// used.  The include handler will include files relative to the
// current directory and files relative to the directory of the initial source
// file.  When used with APIs like D3DCompile pSourceName must be a
// file name and the initial relative directory will be derived from it.
#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)

	
void MainApp::BuildShadersAndInputLayout()
{
	UINT compileFlags = 0;
#if defined(_DEBUG) | defined(DEBUG)
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	
	ComPtr<ID3DBlob> errors = nullptr;
	THROW_IF_FAILED(D3DCompileFromFile(
		L"VertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"vs_5_0",
		compileFlags,
		0,
		m_vsByteCode.GetAddressOf(),
		&errors
		))
	errors.Reset();
	THROW_IF_FAILED(D3DCompileFromFile(
		L"PixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"ps_5_0",
		compileFlags,
		0,
		m_psByteCode.GetAddressOf(),
		&errors))

	m_inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
}

void MainApp::BuildPSO()
{
	LOG_FUNCTION_ENTRY();
	/*
	typedef struct D3D12_GRAPHICS_PIPELINE_STATE_DESC
	{
		ID3D12RootSignature *pRootSignature;
		D3D12_SHADER_BYTECODE VS;
		D3D12_SHADER_BYTECODE PS;
		D3D12_SHADER_BYTECODE DS;
		D3D12_SHADER_BYTECODE HS;
		D3D12_SHADER_BYTECODE GS;
		D3D12_STREAM_OUTPUT_DESC StreamOutput;
		D3D12_BLEND_DESC BlendState;
		UINT SampleMask;
		D3D12_RASTERIZER_DESC RasterizerState;
		D3D12_DEPTH_STENCIL_DESC DepthStencilState;
		D3D12_INPUT_LAYOUT_DESC InputLayout;
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
		D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
		UINT NumRenderTargets;
		DXGI_FORMAT RTVFormats[ 8 ];
		DXGI_FORMAT DSVFormat;
		DXGI_SAMPLE_DESC SampleDesc;
		UINT NodeMask;
		D3D12_CACHED_PIPELINE_STATE CachedPSO;
		D3D12_PIPELINE_STATE_FLAGS Flags;
	} 	D3D12_GRAPHICS_PIPELINE_STATE_DESC;
	*/
	
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS.pShaderBytecode = m_vsByteCode->GetBufferPointer();
	psoDesc.VS.BytecodeLength = m_vsByteCode->GetBufferSize();
	psoDesc.PS.pShaderBytecode = m_psByteCode->GetBufferPointer();
	psoDesc.PS.BytecodeLength = m_psByteCode->GetBufferSize();
	psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };

	// Default Blend State
	psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
	psoDesc.BlendState.IndependentBlendEnable = FALSE;
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
	{
		psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
		psoDesc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
		psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
		psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
    
	psoDesc.SampleMask = UINT_MAX;
	/*
	typedef struct D3D12_RASTERIZER_DESC
	{
		D3D12_FILL_MODE FillMode;
		D3D12_CULL_MODE CullMode;
		BOOL FrontCounterClockwise;
		INT DepthBias;
		FLOAT DepthBiasClamp;
		FLOAT SlopeScaledDepthBias;
		BOOL DepthClipEnable;
		BOOL MultisampleEnable;
		BOOL AntialiasedLineEnable;
		UINT ForcedSampleCount;
		D3D12_CONSERVATIVE_RASTERIZATION_MODE ConservativeRaster;
	} 	D3D12_RASTERIZER_DESC;
	*/
	// Default Rasterizer Desc
	D3D12_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc.FrontCounterClockwise = false;
	rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizerDesc.DepthClipEnable = true;
	rasterizerDesc.MultisampleEnable = false;
	rasterizerDesc.AntialiasedLineEnable = false;
	rasterizerDesc.ForcedSampleCount = 0;
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	psoDesc.RasterizerState = rasterizerDesc;

	// Default Depth Stencil Desc
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    
	psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
	psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = Lunar::Constants::SWAP_CHAIN_FORMAT;
	for (UINT i = 1; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		psoDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
    
	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    
	psoDesc.SampleDesc.Count = Lunar::Constants::SAMPLE_COUNT;
	psoDesc.SampleDesc.Quality = 0;
    
	psoDesc.NodeMask = 0;
	psoDesc.CachedPSO.pCachedBlob = nullptr;
	psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;	

	THROW_IF_FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(m_pipelineState.GetAddressOf())))
}

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

	BYTE* pCbvDataBegin; 
	m_uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pCbvDataBegin));

	BasicConstants constants = {};
	XMMATRIX worldMatrix = XMLoadFloat4x4(&m_cube->GetWorldMatrix());
	worldMatrix = XMMatrixTranspose(worldMatrix);
	XMStoreFloat4x4(&constants.model, worldMatrix);
	XMStoreFloat4x4(&constants.view, XMMatrixTranspose(XMLoadFloat4x4(&m_camera->GetViewMatrix())));
	XMStoreFloat4x4(&constants.projection, XMMatrixTranspose(XMLoadFloat4x4(&m_camera->GetProjMatrix())));
	constants.eyePos = m_camera->GetPosition();
	// float t = fmod(m_lunarTimer.GetTotalTime() * 0.3, 1.0f);
	// float angle = fmod(t * XM_2PI, XM_2PI) - XM_PI;
	// XMFLOAT3 rotation = XMFLOAT3(angle, angle * 0.5, angle * 0.2);
	// m_cube->SetRotation(rotation);

	m_pointLight->Position.x = m_pointLightPosX;
	m_pointLight->Position.y = m_pointLightPosY;
	m_pointLight->Position.z = m_pointLightPosZ;
	constants.lights[1] = *m_pointLight;
	
    m_basicCB->CopyData(&constants, sizeof(BasicConstants));
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

	// m_commandAllocator.Reset();
	m_commandAllocator->Reset(); // Cautions!
	m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get());

	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	m_viewport = {};
	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;
	m_viewport.Width = static_cast<float>(m_displayWidth);
	m_viewport.Height = static_cast<float>(m_displayHeight);
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	m_commandList->RSSetViewports(1, &m_viewport);

	m_scissorRect = {};
	m_scissorRect.left = 0;
	m_scissorRect.top = 0;
	m_scissorRect.right = static_cast<LONG>(m_displayWidth);
	m_scissorRect.bottom = static_cast<LONG>(m_displayHeight);
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
	m_commandList->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, nullptr);

	// Set Constant Buffer Descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	m_commandList->SetGraphicsRootDescriptorTable(0, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

    m_commandList->SetGraphicsRootConstantBufferView(
        Lunar::Constants::BASIC_CONSTANTS_ROOT_PARAMETER_INDEX, 
        m_basicCB->GetResource()->GetGPUVirtualAddress());

	// clear
	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_commandList->ClearRenderTargetView(renderTargetViewHandle, clearColor, 0, nullptr);

	m_cube->Draw(m_commandList.Get());

	// resource barrier - transition to the present state
	barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_commandList->ResourceBarrier(1, &barrier);

    m_gui->Render(dt);
	ID3D12DescriptorHeap* heaps[] = { m_imGuiDescriptorHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

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
	InitializeTextures();
	CreateCamera();
	CreateSwapChain();
	CreateSRVDescriptorHeap();
	CreateConstantBuffer();
	CreateRTVDescriptorHeap();
	CreateRenderTargetView();
	CreateShaderResourceView();
	CreateRootSignature();
	BuildShadersAndInputLayout();
	BuildPSO();
	InitializeGeometry();
	CreateLights();
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

float MainApp::GetAspectRatio() const
{
	LOG_FUNCTION_ENTRY();
	return static_cast<float>(m_displayWidth) / m_displayHeight;
}

void MainApp::InitializeGeometry()
{
	LOG_FUNCTION_ENTRY();
	m_cube = std::make_unique<Cube>();
	
	m_commandAllocator->Reset();
	m_commandList->Reset(m_commandAllocator.Get(), nullptr);
	m_cube->Initialize(m_device.Get(), m_commandList.Get());
	m_cube->SetPosition(XMFLOAT3(0.0f, 0.0f, 0.5f));
	m_cube->SetRotation(XMFLOAT3(0.0f, 0.0f, 0.0f));
	
	m_cube->SetScale(0.5f);
	m_cube->SetColor(XMFLOAT4(0.8f, 0.2f, 0.3f, 1.0f));
}

void MainApp::CreateCamera()
{
	LOG_FUNCTION_ENTRY();
	m_camera = std::make_unique<Camera>();
}

void MainApp::InitializeTextures()
{
	LOG_FUNCTION_ENTRY();
	m_commandAllocator->Reset();
	m_commandList->Reset(m_commandAllocator.Get(), nullptr);
    
	// std::string texturePath = "Assets\\Textures\\black_tiling_15-1K\\tiling_15_basecolor-1K.png";
	// std::string texturePath = "Assets\\Textures\\black_tiling_15-1K\\tiling_15_ambientocclusion-1K.png";
	std::string texturePath = "Assets\\Textures\\wall.jpg";
	if (!std::filesystem::exists(texturePath))
	{
		LOG_ERROR("Texture file does not exist: ", texturePath);
        return;
	}
	m_texture = Utils::LoadSimpleTexture(m_device.Get(), m_commandList.Get(), texturePath, m_textureUploadBuffer);
}

void MainApp::CreateConstantBuffer()
{
    m_basicCB = std::make_unique<ConstantBuffer>(m_device.Get(), sizeof(BasicConstants));
}

void MainApp::CreateLights()
{
	m_directionalLight = std::make_unique<Light>();
	m_pointLight = std::make_unique<Light>();
	m_spotLight = std::make_unique<Light>();

	m_pointLight->FalloffEnd = 100.0f;
	m_pointLight->FalloffStart = 0.1f;
	m_pointLight->Strength = {10.0f, 10.0f, 10.0f};

	m_gui->BindSlider("point_light_pos_x", &m_pointLightPosX, -5.0f, 5.0f);
	m_gui->BindSlider<float>("point_light_pos_y", &m_pointLightPosY, -5.0f, 5.0f);
	m_gui->BindSlider<float>("point_light_pos_z", &m_pointLightPosZ, -5.0f, 5.0f);
}
} // namespace Lunar
