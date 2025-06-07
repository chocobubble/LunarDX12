#pragma once

#include <d3d12.h>
#include <d3dcompiler.h>
#include <Windows.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class MainApp {
public:
	MainApp();
	virtual ~MainApp();
	int Run();
	void Initialize();
	LRESULT MessageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
private:
	void Render();
	void Update();
	bool InitDirect3D();
	bool InitMainWindow();
	HWND m_mainWindow;
};
