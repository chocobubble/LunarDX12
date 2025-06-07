#include "MainApp.h"
#include "Logger.h"

#include <iostream>

using namespace std;

MainApp *g_mainApp = nullptr;

MainApp::MainApp()
{
	g_mainApp = this;
}

MainApp::~MainApp()
{
	g_mainApp = nullptr;

	DestroyWindow(m_mainWindow);
}

LRESULT WINAPI WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return  g_mainApp->MessageProc(hWnd, msg, wParam, lParam);
}

LRESULT MainApp::MessageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_SIZE:
			// Reset and resize swapchain
			break;
		case WM_SYSCOMMAND:
			if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
				return 0;
			break;
		case WM_MOUSEMOVE:
			LOG_DEBUG("Mouse ", LOWORD(lParam), " ", HIWORD(lParam));
			break;
		case WM_LBUTTONUP:
			LOG_DEBUG("Left mouse button");
			break;
		case WM_RBUTTONUP:
			LOG_DEBUG("Right mouse button");
			break;
		case WM_KEYDOWN:
			LOG_DEBUG("Key ", (int)wParam, " down");
			break;
		case WM_DESTROY:
			::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

int MainApp::Run()
{
	LOG_FUNCTION_ENTRY();
	MSG msg = { 0 };
	while (WM_QUIT != msg.message) 
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			Update();
			Render();
		}
	}
	LOG_FUNCTION_EXIT();
	return 0;
}

void MainApp::Update()
{
}


void MainApp::Render()
{

}

void MainApp::Initialize()
{
	LOG_FUNCTION_ENTRY();
	InitMainWindow();
	Run();
}

bool MainApp::InitDirect3D()
{
	return true;	
}

bool MainApp::InitMainWindow()
{
	LOG_FUNCTION_ENTRY();
	WNDCLASSEX wc = {
		sizeof(WNDCLASSEX),
		CS_CLASSDC,
		WindowProc,
		0,
		0,
		GetModuleHandle(NULL),
		NULL,
		NULL,
		NULL,
		NULL,
		L"MainApp",
		NULL
	};

	if (!RegisterClassEx(&wc))
	{
		LOG_ERROR("RegisterClassEx() Failed");
		return false;
	}

	RECT wr = {0, 0, 1280, 720};
	m_mainWindow = CreateWindow(wc.lpszClassName,
								L"Lunar App",
								WS_OVERLAPPEDWINDOW,
								0,
								0,
								wr.right - wr.left,
								wr.bottom - wr.top,
								NULL,
								NULL,
								wc.hInstance,
								NULL);
	if (!m_mainWindow)
	{
		LOG_ERROR("CreateWindow() Failed");
		return false;
	}

	ShowWindow(m_mainWindow, SW_SHOW);
	UpdateWindow(m_mainWindow);

	return true;
}


