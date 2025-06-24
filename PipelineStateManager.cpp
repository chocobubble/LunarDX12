#include "PipelineStateManager.h"

#include <d3dcompiler.h>

#include "Logger.h"
#include "LunarConstants.h"
#include "Utils.h"

using namespace Microsoft::WRL;

namespace Lunar
{
PipelineStateManager::PipelineStateManager()
{
	#if defined(_DEBUG) | defined(DEBUG)
	m_compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	#endif

	m_inputLayoutMap = {
		{
			"default", {{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, 
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }}
		}
	};
}

void PipelineStateManager::Initialize(ID3D12Device* device)
{
	LOG_FUNCTION_ENTRY();

	m_shaderMap["basicVS"] = CompileShader("VertexShader", "vs_5_0");
	m_shaderMap["basicPS"] = CompileShader("PixelShader", "ps_5_0");
	BuildPSOs(device);
}

ComPtr<ID3DBlob> PipelineStateManager::CompileShader(const std::string& shaderName, const std::string& target) const
{
	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors = nullptr;
	THROW_IF_FAILED(D3DCompileFromFile(
		(std::wstring(shaderName.begin(), shaderName.end()) + L".hlsl").c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		target.c_str(),
		m_compileFlags,
		0,
		byteCode.GetAddressOf(),
		&errors
		))
	return byteCode;
}

void PipelineStateManager::CreateRootSignature(ID3D12Device* device)
{
	LOG_FUNCTION_ENTRY();
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
	srvTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvTable.NumDescriptors = 1;
	srvTable.BaseShaderRegister = 0;
	srvTable.RegisterSpace = 0;
	srvTable.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	
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
	
	THROW_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(),
											   signature->GetBufferSize(),
											   IID_PPV_ARGS(m_rootSignature.GetAddressOf())))
}

void PipelineStateManager::BuildPSOs(ID3D12Device* device)
{
	LOG_FUNCTION_ENTRY();

	CreateRootSignature(device);
	
	m_inputLayout = m_inputLayoutMap["default"];

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
	psoDesc.VS.pShaderBytecode = m_shaderMap["basicVS"]->GetBufferPointer();
	psoDesc.VS.BytecodeLength = m_shaderMap["basicVS"]->GetBufferSize();
	psoDesc.PS.pShaderBytecode = m_shaderMap["basicPS"]->GetBufferPointer();
	psoDesc.PS.BytecodeLength = m_shaderMap["basicPS"]->GetBufferSize();
	psoDesc.InputLayout = { m_inputLayout.data(), static_cast<UINT>(m_inputLayout.size()) };

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

	/*
	typedef struct D3D12_DEPTH_STENCIL_DESC
	{
		BOOL DepthEnable;
		D3D12_DEPTH_WRITE_MASK DepthWriteMask;
		D3D12_COMPARISON_FUNC DepthFunc;
		BOOL StencilEnable;
		UINT8 StencilReadMask;
		UINT8 StencilWriteMask;
		D3D12_DEPTH_STENCILOP_DESC FrontFace;
		D3D12_DEPTH_STENCILOP_DESC BackFace;
	} 	D3D12_DEPTH_STENCIL_DESC;
	*/
	
	// Default Depth Stencil Desc
	psoDesc.DepthStencilState.DepthEnable = true;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    
	psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = Lunar::Constants::SWAP_CHAIN_FORMAT;
	for (UINT i = 1; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		psoDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
    
	// psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    
	psoDesc.SampleDesc.Count = Lunar::Constants::SAMPLE_COUNT;
	psoDesc.SampleDesc.Quality = 0;
    
	psoDesc.NodeMask = 0;
	psoDesc.CachedPSO.pCachedBlob = nullptr;
	psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	
	THROW_IF_FAILED(device->CreateGraphicsPipelineState(&psoDesc,
		IID_PPV_ARGS(m_psoMap["opaque"].GetAddressOf())))

	// PSO for marking stencil mirror
	
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DepthStencilState.StencilEnable = true;

		/*
		typedef struct D3D12_DEPTH_STENCILOP_DESC
		{
			D3D12_STENCIL_OP StencilFailOp;
			D3D12_STENCIL_OP StencilDepthFailOp;
			D3D12_STENCIL_OP StencilPassOp;
			D3D12_COMPARISON_FUNC StencilFunc;
		} 	D3D12_DEPTH_STENCILOP_DESC;
		*/

		D3D12_DEPTH_STENCILOP_DESC stencilOp = {};
		stencilOp.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		stencilOp.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		stencilOp.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
		stencilOp.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		psoDesc.DepthStencilState.FrontFace = stencilOp;
		psoDesc.DepthStencilState.BackFace = stencilOp;
		THROW_IF_FAILED(device->CreateGraphicsPipelineState(&psoDesc,
			IID_PPV_ARGS(m_psoMap["mirror"].GetAddressOf())))
	

	// PSO for reflected objects
	
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		psoDesc.RasterizerState.FrontCounterClockwise = false;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		stencilOp.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		stencilOp.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		stencilOp.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		stencilOp.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
		psoDesc.DepthStencilState.FrontFace = stencilOp;
		psoDesc.DepthStencilState.BackFace = stencilOp;
		THROW_IF_FAILED(device->CreateGraphicsPipelineState(&psoDesc,
			IID_PPV_ARGS(m_psoMap["reflect"].GetAddressOf())))
	
}

ID3D12PipelineState* PipelineStateManager::GetPSO(const std::string& psoName) const
{
	if (m_psoMap.find(psoName) != m_psoMap.end())
	{
		return m_psoMap.at(psoName).Get();
	}
	else
	{
		LOG_ERROR("Pipeline state not found: ", psoName);
		return nullptr;
	}
}

} // namespace Lunar