#include "ParticleSystemEnhanced.h"
#include "PipelineStateManager.h"

namespace Lunar
{

void ParticleSystemEnhanced::CreateBlendStates()
{
    // 공통 설정
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    
    // 셰이더 설정 (PipelineStateManager에서 가져옴)
    psoDesc.VS = m_psoManager->GetShaderBytecode("ParticleVertexShaderEnhanced");
    psoDesc.GS = m_psoManager->GetShaderBytecode("ParticleGeometryShaderEnhanced");
    psoDesc.PS = m_psoManager->GetShaderBytecode("ParticlePixelShaderEnhanced");
    
    // 입력 레이아웃 (빈 버텍스 - 인스턴싱 사용)
    psoDesc.InputLayout = { nullptr, 0 };
    
    // 루트 시그니처
    psoDesc.pRootSignature = m_psoManager->GetRootSignature();
    
    // 래스터라이저 상태
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // 파티클은 컬링 없음
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = 0;
    psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    
    // 깊이 스텐실 상태 (깊이 쓰기 비활성화, 깊이 테스트 활성화)
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 깊이 쓰기 비활성화
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    
    // 렌더 타겟 포맷
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    
    // 샘플 설정
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    
    // 프리미티브 토폴로지
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    
    // 1. Alpha Blending (투명 블렌딩)
    {
        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        
        psoDesc.BlendState = blendDesc;
        
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
        THROW_IF_FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
        m_blendPSOs[BlendMode::Alpha] = pso;
    }
    
    // 2. Additive Blending (가산 블렌딩)
    {
        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        
        psoDesc.BlendState = blendDesc;
        
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
        THROW_IF_FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
        m_blendPSOs[BlendMode::Additive] = pso;
    }
    
    // 3. Multiplicative Blending (곱셈 블렌딩)
    {
        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_DEST_ALPHA;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        
        psoDesc.BlendState = blendDesc;
        
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
        THROW_IF_FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
        m_blendPSOs[BlendMode::Multiplicative] = pso;
    }
    
    // 4. Screen Blending (스크린 블렌딩)
    {
        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_DEST_ALPHA;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        
        psoDesc.BlendState = blendDesc;
        
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
        THROW_IF_FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
        m_blendPSOs[BlendMode::Screen] = pso;
    }
    
    // 5. Subtractive Blending (감산 블렌딩)
    {
        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT; // 역순 감산
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        
        psoDesc.BlendState = blendDesc;
        
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
        THROW_IF_FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
        m_blendPSOs[BlendMode::Subtractive] = pso;
    }
}

} // namespace Lunar
