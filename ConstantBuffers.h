#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>

namespace Lunar
{

struct BasicConstants
{
	DirectX::XMFLOAT4X4 model;
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
	DirectX::XMFLOAT3   eyePos;
	float               dummy;
};

struct Light
{
	DirectX::XMFLOAT3 Strength;
	float             FalloffStart;
	DirectX::XMFLOAT3 Direction;
	float             FalloffEnd;
	DirectX::XMFLOAT3 Position;
	float             SpotPower;
};


class ConstantBuffer
{
public:
	ConstantBuffer(ID3D12Device* device, UINT elementByteSize);
	~ConstantBuffer();
	void CopyData(void* data, UINT size);
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
	BYTE* m_mappedData = nullptr;
};
	
} // namespace Lunar
