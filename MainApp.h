#pragma once

#include <d3d12.h>
#include <d3dcompiler.h>
#include <vector>
#include <Windows.h>
#include <wrl.h>
#include "dxgi1_6.h"
#include <DirectXMath.h>
#include <memory>
#include "LunarConstants.h"
#include "LunarTimer.h"
using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace Lunar {

struct Vertex
{
	XMFLOAT3 pos;
	XMFLOAT4 color;
	XMFLOAT2 texCoord;
};
	
class MainApp {
public:
	MainApp();
	virtual ~MainApp();
	int Run();
	void Initialize();
	LRESULT MessageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
private:
	void InitGui();
	void InitializeCommandList();
	void CreateSwapChain();
	void CreateCBVDescriptorHeap();
	void CreateConstantBufferView();
	void CreateRTVDescriptorHeap();
	void CreateRenderTargetView();
	void CreateShaderResourceView();
	void CreateRootSignature();
	void BuildShadersAndInputLayout();
	void BuildPSO();
	void CreateFence();
	void Render(double dt);
	void Update(double dt);
    void ProcessInput(double dt);
	bool InitDirect3D();
	bool InitMainWindow();
    void OnMouseMove(float x, float y);
	float GetAspectRatio() const;
	void InitializeGeometry();
	void CreateCamera();
	void InitializeTextures();
	void CreateConstantBuffer();
	
	HWND m_mainWindow;
	
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<IDXGIFactory4> m_factory;
	ComPtr<IDXGIAdapter1> m_adapter;
	ComPtr<IDXGIAdapter1> m_hardwareAdapter;
	ComPtr<IDXGISwapChain1> m_swapChain;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3DBlob> m_vsByteCode;
	ComPtr<ID3DBlob> m_psByteCode;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12Fence> m_fence;
	ComPtr<ID3D12Resource> m_renderTargets[Lunar::Constants::BUFFER_COUNT];
	ComPtr<ID3D12Resource> m_uploadBuffer;
	ComPtr<ID3D12Resource> m_textureUploadBuffer;
	ComPtr<ID3D12DescriptorHeap> m_imGuiDescriptorHeap;
	ComPtr<ID3D12Resource> m_texture;

	D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_cbvHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle;

	UINT m_displayWidth;
	UINT m_displayHeight;
	UINT m_renderTargetViewDescriptorSize;
	UINT m_fenceValue;
	UINT m_frameIndex;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

	HANDLE m_fenceEvent;

	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;

	Lunar::LunarTimer m_lunarTimer;
	
	std::unique_ptr<class Cube> m_cube;
    std::unique_ptr<class LunarGui> m_gui;

    bool m_firstMouseMove = true;
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;

    std::unique_ptr<class Camera> m_camera;

	std::unique_ptr<class ConstantBuffer> m_lightCB;
};

} // namespace Lunar
