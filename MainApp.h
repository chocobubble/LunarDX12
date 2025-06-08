#pragma once

#include <d3d12.h>
#include <d3dcompiler.h>
#include <Windows.h>
#include <wrl.h>
#include "dxgi1_6.h"

using Microsoft::WRL::ComPtr;

namespace Lunar {

class MainApp {
public:
	MainApp();
	virtual ~MainApp();
	int Run();
	void Initialize();
	LRESULT MessageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
private:
	void InitializeCommandList();
	void CreateSwapChain();
	void CreateRTVDescriptorHeap();
	void CreateRenderTargetView();
	void CreateRootSignature();
	void Render();
	void Update();
	bool InitDirect3D();
	bool InitMainWindow();
	
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
	ComPtr<ID3D12RootSignature> m_rootSignature;

	D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandle;

	UINT m_displayWidth;
	UINT m_displayHeight;
};

} // namespace Lunar
