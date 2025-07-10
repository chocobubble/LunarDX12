#pragma once

#include <d3d12.h>
#include <Windows.h>
#include <wrl.h>
#include "dxgi1_6.h"
#include <memory>

#include "LunarConstants.h"

namespace Lunar
{
	
class PostProcessManager;
class PipelineStateManager;
class SceneRenderer;
class Camera;
class LunarGui;
class PerformanceProfiler;
class PerformanceViewModel;
	
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
	void CreateRTVDescriptorHeap();
	void CreateRenderTargetView();
	void CreateFence();
	void Render(double dt);
	void Update(double dt);
    void ProcessInput(double dt);
	bool InitDirect3D();
	bool InitMainWindow();
    void OnMouseMove(float x, float y);
	void InitializeGeometry();
	void CreateCamera();
	void InitializeTextures();
	
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
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_lightHeap;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[Lunar::LunarConstants::BUFFER_COUNT];
	Microsoft::WRL::ComPtr<ID3D12Resource> m_textureUploadBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_sceneRenderTarget;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_imGuiDescriptorHeap;

	D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_cbvHandle;

	UINT m_renderTargetViewDescriptorSize;
	UINT m_fenceValue;
	UINT m_frameIndex;

	HANDLE m_fenceEvent;

	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;

	std::unique_ptr<PerformanceProfiler> m_performanceProfiler;
	std::unique_ptr<PerformanceViewModel> m_performanceViewModel;
	
    std::unique_ptr<LunarGui> m_gui;

    bool m_firstMouseMove = true;
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;

    std::unique_ptr<Camera> m_camera;

    std::unique_ptr<SceneRenderer> m_sceneRenderer;
	std::unique_ptr<PipelineStateManager> m_pipelineStateManager;
	std::unique_ptr<PostProcessManager> m_postProcessManager;

	bool m_mouseMoving = false;
};

} // namespace Lunar
