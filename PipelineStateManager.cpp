#include "PipelineStateManager.h"

#include <d3dcompiler.h>

#include "Utils/Logger.h"
#include "Utils/Utils.h"

using namespace Microsoft::WRL;
using namespace std;

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
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			},
		},
		{
			"point", {{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }}
		}
	};
}

void PipelineStateManager::Initialize(ID3D12Device* device)
{
	LOG_FUNCTION_ENTRY();

	for (LunarConstants::ShaderInfo shaderInfo : LunarConstants::SHADER_INFO)
	{
		m_shaderMap[shaderInfo.name] = CompileShader(shaderInfo);
		LOG_DEBUG("Shader ", shaderInfo.name, " compiled");
	}
	
	BuildPSOs(device);
}

ComPtr<ID3DBlob> PipelineStateManager::CompileShader(const LunarConstants::ShaderInfo& shaderInfo) const
{
	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors = nullptr;

	string shaderPath = shaderInfo.path;
	THROW_IF_FAILED(D3DCompileFromFile(
		wstring(shaderPath.begin(), shaderPath.end()).c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
        shaderInfo.entryPoint,
		shaderInfo.target,
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
	D3D12_DESCRIPTOR_RANGE textureSrvRange = {};
	textureSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	textureSrvRange.NumDescriptors = LunarConstants::TEXTURE_INFO.size();
	textureSrvRange.BaseShaderRegister = 0;
	textureSrvRange.RegisterSpace = 0;
	textureSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE shadowMapSrvRange = {};
	shadowMapSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	shadowMapSrvRange.NumDescriptors = 1;
	shadowMapSrvRange.BaseShaderRegister = 0;
	shadowMapSrvRange.RegisterSpace = 1;
	shadowMapSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE postProcessSrvRange = {};
	postProcessSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	postProcessSrvRange.NumDescriptors = 2;
	postProcessSrvRange.BaseShaderRegister = 0;
	postProcessSrvRange.RegisterSpace = 3;
	postProcessSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	
	D3D12_DESCRIPTOR_RANGE postProcessUavRange = {};
	postProcessUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	postProcessUavRange.NumDescriptors = 2;
	postProcessUavRange.BaseShaderRegister = 0;
	postProcessUavRange.RegisterSpace = 1;
	postProcessUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
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
	D3D12_ROOT_PARAMETER rootParameters[8];
    D3D12_DESCRIPTOR_RANGE srvRanges[2] = { textureSrvRange, shadowMapSrvRange };
    size_t index = LunarConstants::TEXTURE_SR_ROOT_PARAMETER_INDEX;
	rootParameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[index].DescriptorTable.NumDescriptorRanges = 2;
	rootParameters[index].DescriptorTable.pDescriptorRanges = srvRanges;
	rootParameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    index = LunarConstants::BASIC_CONSTANTS_ROOT_PARAMETER_INDEX;
	rootParameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[index].Descriptor.RegisterSpace = 0;
	rootParameters[index].Descriptor.ShaderRegister = 0;
	rootParameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    index = LunarConstants::OBJECT_CONSTANTS_ROOT_PARAMETER_INDEX;
	rootParameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[index].Descriptor.RegisterSpace = 0;
	rootParameters[index].Descriptor.ShaderRegister = 1;
	rootParameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    index = LunarConstants::MATERIAL_CONSTANTS_ROOT_PARAMETER_INDEX;
	rootParameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[index].Descriptor.RegisterSpace = 0;
	rootParameters[index].Descriptor.ShaderRegister = 2;
	rootParameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    index = LunarConstants::PARTICLE_SRV_ROOT_PARAMETER_INDEX;
    rootParameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParameters[index].Descriptor.RegisterSpace = 2;
	rootParameters[index].Descriptor.ShaderRegister = 0;
    rootParameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    index = LunarConstants::PARTICLE_UAV_ROOT_PARAMETER_INDEX;
    rootParameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
	rootParameters[index].Descriptor.RegisterSpace = 0;
	rootParameters[index].Descriptor.ShaderRegister = 0;
    rootParameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	index = LunarConstants::POST_PROCESS_INPUT_ROOT_PARAMETER_INDEX;
	rootParameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[index].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[index].DescriptorTable.pDescriptorRanges = &postProcessSrvRange;
	rootParameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	
	index = LunarConstants::POST_PROCESS_OUTPUT_ROOT_PARAMETER_INDEX;
	rootParameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[index].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[index].DescriptorTable.pDescriptorRanges = &postProcessUavRange;
	rootParameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
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
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // TODO: Add More sampler
	
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc = {};
	opaquePsoDesc.pRootSignature = m_rootSignature.Get();
	opaquePsoDesc.VS.pShaderBytecode = m_shaderMap["basicVS"]->GetBufferPointer();
	opaquePsoDesc.VS.BytecodeLength = m_shaderMap["basicVS"]->GetBufferSize();
	opaquePsoDesc.PS.pShaderBytecode = m_shaderMap["basicPS"]->GetBufferPointer();
	opaquePsoDesc.PS.BytecodeLength = m_shaderMap["basicPS"]->GetBufferSize();
    
	// Default Blend State
	opaquePsoDesc.BlendState.AlphaToCoverageEnable = FALSE;
	opaquePsoDesc.BlendState.IndependentBlendEnable = FALSE;
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
	{
        opaquePsoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
		opaquePsoDesc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
		opaquePsoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
		opaquePsoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
		opaquePsoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
		opaquePsoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
		opaquePsoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
		opaquePsoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		opaquePsoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
		opaquePsoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
    
    opaquePsoDesc.InputLayout = { m_inputLayout.data(), static_cast<UINT>(m_inputLayout.size()) };
	opaquePsoDesc.SampleMask = UINT_MAX;
	
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
	opaquePsoDesc.RasterizerState = rasterizerDesc;

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
	opaquePsoDesc.DepthStencilState.DepthEnable = true;
	opaquePsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	opaquePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	opaquePsoDesc.DepthStencilState.StencilEnable = FALSE;
    
	opaquePsoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = Lunar::LunarConstants::SWAP_CHAIN_FORMAT;
	for (UINT i = 1; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		opaquePsoDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
    
	opaquePsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    
	opaquePsoDesc.SampleDesc.Count = Lunar::LunarConstants::SAMPLE_COUNT;
	opaquePsoDesc.SampleDesc.Quality = 0;
    
	opaquePsoDesc.NodeMask = 0;
	opaquePsoDesc.CachedPSO.pCachedBlob = nullptr;
	opaquePsoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	opaquePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	
	THROW_IF_FAILED(device->CreateGraphicsPipelineState(&opaquePsoDesc,
		IID_PPV_ARGS(m_psoMap["opaque"].GetAddressOf())))

	// PSO for shadow map
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowMapPsoDesc = opaquePsoDesc;
		shadowMapPsoDesc.VS.pShaderBytecode = m_shaderMap["shadowMapVS"]->GetBufferPointer();
		shadowMapPsoDesc.VS.BytecodeLength = m_shaderMap["shadowMapVS"]->GetBufferSize();
		shadowMapPsoDesc.PS.pShaderBytecode = m_shaderMap["shadowMapPS"]->GetBufferPointer();
		shadowMapPsoDesc.PS.BytecodeLength = m_shaderMap["shadowMapPS"]->GetBufferSize();
		shadowMapPsoDesc.RasterizerState.DepthBias = 100000;
		shadowMapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
		shadowMapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
		shadowMapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN; // render taget not used
		shadowMapPsoDesc.NumRenderTargets = 0;
		THROW_IF_FAILED(device->CreateGraphicsPipelineState(&shadowMapPsoDesc,
			IID_PPV_ARGS(m_psoMap["shadowMap"].GetAddressOf())))
	}

	// PSO for marking stencil mirror
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC mirrorStencilPsoDesc = opaquePsoDesc;
		mirrorStencilPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		mirrorStencilPsoDesc.DepthStencilState.StencilEnable = true;
        mirrorStencilPsoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        mirrorStencilPsoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

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
		mirrorStencilPsoDesc.DepthStencilState.FrontFace = stencilOp;
		mirrorStencilPsoDesc.DepthStencilState.BackFace = stencilOp;
		THROW_IF_FAILED(device->CreateGraphicsPipelineState(&mirrorStencilPsoDesc,
			IID_PPV_ARGS(m_psoMap["mirror"].GetAddressOf())))
	}
	

	// PSO for reflected objects
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC reflectPsoDesc = opaquePsoDesc;
		reflectPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		reflectPsoDesc.RasterizerState.FrontCounterClockwise = true;
		reflectPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		D3D12_DEPTH_STENCILOP_DESC stencilOp = {};
		stencilOp.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		stencilOp.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		stencilOp.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		stencilOp.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
		reflectPsoDesc.DepthStencilState.FrontFace = stencilOp;
		reflectPsoDesc.DepthStencilState.BackFace = stencilOp;
		THROW_IF_FAILED(device->CreateGraphicsPipelineState(&reflectPsoDesc,
			IID_PPV_ARGS(m_psoMap["reflect"].GetAddressOf())))
	}

	// PSO for skybox
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC skyboxPsoDesc = opaquePsoDesc;
		skyboxPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		skyboxPsoDesc.VS.pShaderBytecode = m_shaderMap["skyBoxVS"]->GetBufferPointer();
		skyboxPsoDesc.VS.BytecodeLength = m_shaderMap["skyBoxVS"]->GetBufferSize();
		skyboxPsoDesc.PS.pShaderBytecode = m_shaderMap["skyBoxPS"]->GetBufferPointer();
		skyboxPsoDesc.PS.BytecodeLength = m_shaderMap["skyBoxPS"]->GetBufferSize();
		THROW_IF_FAILED(device->CreateGraphicsPipelineState(&skyboxPsoDesc,
			IID_PPV_ARGS(m_psoMap["background"].GetAddressOf())))
	}

	// PSO for billboard
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC billboardPsoDesc = opaquePsoDesc;
		billboardPsoDesc.VS.pShaderBytecode = m_shaderMap["billboardVS"]->GetBufferPointer();
		billboardPsoDesc.VS.BytecodeLength = m_shaderMap["billboardVS"]->GetBufferSize();
		billboardPsoDesc.GS.pShaderBytecode = m_shaderMap["billboardGS"]->GetBufferPointer();
		billboardPsoDesc.GS.BytecodeLength = m_shaderMap["billboardGS"]->GetBufferSize();
		billboardPsoDesc.PS.pShaderBytecode = m_shaderMap["billboardPS"]->GetBufferPointer();
		billboardPsoDesc.PS.BytecodeLength = m_shaderMap["billboardPS"]->GetBufferSize();
		m_inputLayout = m_inputLayoutMap["point"];
		billboardPsoDesc.InputLayout = { m_inputLayout.data(), static_cast<UINT>(m_inputLayout.size()) };
		billboardPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		THROW_IF_FAILED(device->CreateGraphicsPipelineState(&billboardPsoDesc,
			IID_PPV_ARGS(m_psoMap["billboard"].GetAddressOf())))
	}

	// PSO for normal
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC normalPsoDesc = opaquePsoDesc;
		m_inputLayout = m_inputLayoutMap["default"];
		normalPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		normalPsoDesc.VS.pShaderBytecode = m_shaderMap["normalVS"]->GetBufferPointer();
		normalPsoDesc.VS.BytecodeLength = m_shaderMap["normalVS"]->GetBufferSize();
		normalPsoDesc.GS.pShaderBytecode = m_shaderMap["normalGS"]->GetBufferPointer();
		normalPsoDesc.GS.BytecodeLength = m_shaderMap["normalGS"]->GetBufferSize();
		normalPsoDesc.PS.pShaderBytecode = m_shaderMap["normalPS"]->GetBufferPointer();
		normalPsoDesc.PS.BytecodeLength = m_shaderMap["normalPS"]->GetBufferSize();
		normalPsoDesc.InputLayout = { m_inputLayout.data(), static_cast<UINT>(m_inputLayout.size()) };
		normalPsoDesc.RasterizerState.FrontCounterClockwise = false;
		normalPsoDesc.DepthStencilState.StencilEnable = false;
		normalPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		normalPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		THROW_IF_FAILED(device->CreateGraphicsPipelineState(&normalPsoDesc, IID_PPV_ARGS(m_psoMap["normal"].GetAddressOf())))
	}
	
    // PSO for particle system
    {
    	/*
    	typedef struct D3D12_COMPUTE_PIPELINE_STATE_DESC
    	{
    		ID3D12RootSignature *pRootSignature;
    		D3D12_SHADER_BYTECODE CS;
    		UINT NodeMask;
    		D3D12_CACHED_PIPELINE_STATE CachedPSO;
    		D3D12_PIPELINE_STATE_FLAGS Flags;
    	} 	D3D12_COMPUTE_PIPELINE_STATE_DESC;
		*/

		D3D12_COMPUTE_PIPELINE_STATE_DESC particlesUpdatePsoDesc = {};
    	particlesUpdatePsoDesc.pRootSignature = m_rootSignature.Get();
    	particlesUpdatePsoDesc.NodeMask = 0;
        particlesUpdatePsoDesc.CS.pShaderBytecode = m_shaderMap["particlesUpdateCS"]->GetBufferPointer();
        particlesUpdatePsoDesc.CS.BytecodeLength = m_shaderMap["particlesUpdateCS"]->GetBufferSize();
    	particlesUpdatePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        THROW_IF_FAILED(device->CreateComputePipelineState(&particlesUpdatePsoDesc, 
            IID_PPV_ARGS(m_psoMap["particlesUpdate"].GetAddressOf())))

    	D3D12_GRAPHICS_PIPELINE_STATE_DESC particlesPsoDesc = opaquePsoDesc;
    	particlesPsoDesc.VS.pShaderBytecode = m_shaderMap["particlesVS"]->GetBufferPointer();
    	particlesPsoDesc.VS.BytecodeLength = m_shaderMap["particlesVS"]->GetBufferSize();
    	particlesPsoDesc.GS.pShaderBytecode = m_shaderMap["particlesGS"]->GetBufferPointer();
    	particlesPsoDesc.GS.BytecodeLength = m_shaderMap["particlesGS"]->GetBufferSize();
    	particlesPsoDesc.PS.pShaderBytecode = m_shaderMap["particlesPS"]->GetBufferPointer();
    	particlesPsoDesc.PS.BytecodeLength = m_shaderMap["particlesPS"]->GetBufferSize();
    	particlesPsoDesc.InputLayout = {nullptr, 0};
    	particlesPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    	THROW_IF_FAILED(device->CreateGraphicsPipelineState(&particlesPsoDesc,
			IID_PPV_ARGS(m_psoMap["particles"].GetAddressOf())))
    }

	// PSO for Render Wire Frame 
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC wirePsoDesc = opaquePsoDesc;
		wirePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		THROW_IF_FAILED(device->CreateGraphicsPipelineState(&wirePsoDesc,
			IID_PPV_ARGS(m_psoMap["opaque_wireframe"].GetAddressOf())))
	}

	// PSO for Tessellation
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC tessellationPsoDesc = opaquePsoDesc;
		tessellationPsoDesc.DS.pShaderBytecode = m_shaderMap["basicDS"]->GetBufferPointer();
		tessellationPsoDesc.DS.BytecodeLength = m_shaderMap["basicDS"]->GetBufferSize();
		tessellationPsoDesc.HS.pShaderBytecode = m_shaderMap["basicHS"]->GetBufferPointer();
		tessellationPsoDesc.HS.BytecodeLength = m_shaderMap["basicHS"]->GetBufferSize();
		tessellationPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		THROW_IF_FAILED(device->CreateGraphicsPipelineState(&tessellationPsoDesc,
			IID_PPV_ARGS(m_psoMap["tessellation"].GetAddressOf())))

		tessellationPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		THROW_IF_FAILED(device->CreateGraphicsPipelineState(&tessellationPsoDesc,
			IID_PPV_ARGS(m_psoMap["tessellation_wireframe"].GetAddressOf())))
	}

    // PSO for Blur
    {		
        D3D12_COMPUTE_PIPELINE_STATE_DESC gaussianBlurPsoDesc = {};
    	gaussianBlurPsoDesc.pRootSignature = m_rootSignature.Get();
    	gaussianBlurPsoDesc.NodeMask = 0;
        gaussianBlurPsoDesc.CS.pShaderBytecode = m_shaderMap["gaussianBlurXCS"]->GetBufferPointer();
        gaussianBlurPsoDesc.CS.BytecodeLength = m_shaderMap["gaussianBlurXCS"]->GetBufferSize();
    	gaussianBlurPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        THROW_IF_FAILED(device->CreateComputePipelineState(&gaussianBlurPsoDesc, 
            IID_PPV_ARGS(m_psoMap["gaussianBlurX"].GetAddressOf())))
    	
		gaussianBlurPsoDesc.CS.pShaderBytecode = m_shaderMap["gaussianBlurYCS"]->GetBufferPointer();
		gaussianBlurPsoDesc.CS.BytecodeLength = m_shaderMap["gaussianBlurYCS"]->GetBufferSize();
		THROW_IF_FAILED(device->CreateComputePipelineState(&gaussianBlurPsoDesc, 
			IID_PPV_ARGS(m_psoMap["gaussianBlurY"].GetAddressOf())))
	}
}

ID3D12PipelineState* PipelineStateManager::GetPSO(const string& psoName) const
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