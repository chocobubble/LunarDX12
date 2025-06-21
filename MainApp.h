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

namespace Lunar {
struct Light;
class ConstantBuffer;
class Camera;
class LunarGui;
	
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
	void CreateSRVDescriptorHeap();
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
	void CreateLights();
	
	HWND m_mainWindow;
	
	Microsoft::WRL::ComPtr<ID3D12Device> m_device;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
	Microsoft::WRL::ComPtr<IDXGIFactory4> m_factory;
	Microsoft::WRL::ComPtr<IDXGIAdapter1> m_adapter;
	Microsoft::WRL::ComPtr<IDXGIAdapter1> m_hardwareAdapter;
	Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_lightHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	Microsoft::WRL::ComPtr<ID3DBlob> m_vsByteCode;
	Microsoft::WRL::ComPtr<ID3DBlob> m_psByteCode;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[Lunar::Constants::BUFFER_COUNT];
	Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_textureUploadBuffer;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_imGuiDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;

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
    std::unique_ptr<LunarGui> m_gui;

    bool m_firstMouseMove = true;
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;

    std::unique_ptr<Camera> m_camera;

    std::unique_ptr<ConstantBuffer> m_basicCB;

	std::unique_ptr<Light> m_directionalLight;
	std::unique_ptr<Light> m_pointLight;
	std::unique_ptr<Light> m_spotLight;
	float m_pointLightPosX = 5.0f;
	float m_pointLightPosY = 1.0f;
	float m_pointLightPosZ = 0.0f;

	bool m_mouseMoving = false;
};

} // namespace Lunar
