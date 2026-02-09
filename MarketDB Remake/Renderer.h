#pragma once
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <d3d11.h>

class Renderer {
public:
	void Setup();
	void Render();
	void Cleanup();

private:
	ID3D11Device* m_pd3dDevice{ nullptr };
	ID3D11DeviceContext* m_pd3dDeviceContext{ nullptr };
	IDXGISwapChain* m_pSwapChain{ nullptr };
	bool m_SwapChainOccluded{ false };
	static UINT m_ResizeWidth, m_ResizeHeight;
	ID3D11RenderTargetView* m_mainRenderTargetView{ 0 };
	WNDCLASSEXW m_wc;
	HWND m_hwnd;

private:
	bool CreateDeviceD3D(HWND hWnd);
	void CleanupDeviceD3D();
	void CreateRenderTarget();
	void CleanupRenderTarget();
	static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};