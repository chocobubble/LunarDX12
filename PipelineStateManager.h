#pragma once
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <wrl/client.h>

namespace Lunar
{
	
class PipelineStateManager
{
public:
	PipelineStateManager();
	~PipelineStateManager() = default;
	void Initialize(ID3D12Device* device);
	Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const std::string& shaderName, const std::string& target) const;
	void CreateRootSignature(ID3D12Device* device);
	void BuildPSOs(ID3D12Device* device);
	ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }
	ID3D12PipelineState* GetPSO(const std::string& psoName) const;
private:
	std::unordered_map<std::string, std::vector<D3D12_INPUT_ELEMENT_DESC>> m_inputLayoutMap;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> m_shaderMap;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_psoMap;

	UINT m_compileFlags = 0;
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
};
	
} // namespace Lunar
