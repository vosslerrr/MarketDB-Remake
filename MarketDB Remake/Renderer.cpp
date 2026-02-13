#include "Renderer.h"
#include <iostream>

UINT Renderer::m_ResizeWidth = 0;
UINT Renderer::m_ResizeHeight = 0;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI Renderer::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED)
                return 0;
            m_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
            m_ResizeHeight = (UINT)HIWORD(lParam);
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

void Renderer::CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_mainRenderTargetView);
    pBackBuffer->Release();
}

void Renderer::CleanupRenderTarget()
{
    if (m_mainRenderTargetView) { m_mainRenderTargetView->Release(); m_mainRenderTargetView = nullptr; }
}

bool Renderer::CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    // This is a basic setup. Optimally could use e.g. DXGI_SWAP_EFFECT_FLIP_DISCARD and handle fullscreen mode differently. See #8979 for suggestions.
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_pSwapChain, &m_pd3dDevice, &featureLevel, &m_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_pSwapChain, &m_pd3dDevice, &featureLevel, &m_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    // Disable DXGI's default Alt+Enter fullscreen behavior.
    // - You are free to leave this enabled, but it will not work properly with multiple viewports.
    // - This must be done for all windows associated to the device. Our DX11 backend does this automatically for secondary viewports that it creates.
    IDXGIFactory* pSwapChainFactory;
    if (SUCCEEDED(m_pSwapChain->GetParent(IID_PPV_ARGS(&pSwapChainFactory))))
    {
        pSwapChainFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
        pSwapChainFactory->Release();
    }

    CreateRenderTarget();
    return true;
}

void Renderer::CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (m_pSwapChain) { m_pSwapChain->Release(); m_pSwapChain = nullptr; }
    if (m_pd3dDeviceContext) { m_pd3dDeviceContext->Release(); m_pd3dDeviceContext = nullptr; }
    if (m_pd3dDevice) { m_pd3dDevice->Release(); m_pd3dDevice = nullptr; }
}

void Renderer::Setup()
{
    // Make process DPI aware and obtain main monitor scale
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    // Create application window
    m_wc = { sizeof(m_wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"MarketDB", nullptr };
    ::RegisterClassExW(&m_wc);
    m_hwnd = ::CreateWindowW(m_wc.lpszClassName, L"MarketDB", WS_OVERLAPPEDWINDOW, 100, 100, (int)(1280 * main_scale), (int)(800 * main_scale), nullptr, nullptr, m_wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(m_hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
    }

    // Show the window
    ::ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(m_hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dDeviceContext);
}

void Renderer::Render()
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    int numCols = 0;

    bool item = false;
    bool aisle = false;
    bool section = false;
    bool supplier = false;
    bool transaction = false;

    char item_itemID[128] = "";
    char item_itemName[128] = "";
    char item_aisleNo[128] = "";
    char item_sectionID[128] = "";
    char item_itemPrice[128] = "";
    char item_numItems[128] = "";

    char aisle_aisleNo[128] = "";
    char aisle_numSections[128] = "";

    char section_sectionID[128] = "";
    char section_sectionName[128] = "";
    char section_aisleNo[128] = "";

    char supplier_supplierID[128] = "";
    char supplier_itemID[128] = "";
    char supplier_itemCost[128] = "";
    char supplier_supplierName[128] = "";

    char transaction_transactionID[128] = "";
    char transaction_itemID[128] = "";
    char transaction_itemPrice[128] = "";
    char transaction_taxAmount[128] = "";
    char transaction_transactionTotal[128] = "";
    char transaction_trasactionDate[128] = "";

    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window being minimized or screen locked
        if (m_SwapChainOccluded && m_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        m_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (m_ResizeWidth != 0 && m_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            m_pSwapChain->ResizeBuffers(0, m_ResizeWidth, m_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            m_ResizeWidth = m_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        
        ImGui::Begin("Test", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        if (ImGui::BeginChild("##inputs", ImVec2(300,0), ImGuiChildFlags_Borders))
        {
            if (ImGui::BeginTabBar("##TabBar"))
            {
                if (ImGui::BeginTabItem("Item"))
                {
                    numCols = 6;

                    item = true;
                    aisle = false;
                    section = false;
                    supplier = false;
                    transaction = false;

                    ImGui::Text("Item I.D.");
                    ImGui::SameLine(); ImGui::Dummy({ 13.0f, 0 }); ImGui::SameLine();
                    ImGui::InputText("##item_itemID", item_itemID, IM_COUNTOF(item_itemID));

                    ImGui::Text("Item Name");
                    ImGui::SameLine(); ImGui::Dummy({ 13.0f, 0 }); ImGui::SameLine();
                    ImGui::InputText("##item_itemName", item_itemName, IM_COUNTOF(item_itemName));

                    ImGui::Text("Aisle No.");
                    ImGui::SameLine(); ImGui::Dummy({ 13.0f, 0 }); ImGui::SameLine();
                    ImGui::InputText("##item_aisleNo", item_aisleNo, IM_COUNTOF(item_aisleNo));

                    ImGui::Text("Section I.D.");
                    ImGui::SameLine();
                    ImGui::InputText("##item_sectionID", item_sectionID, IM_COUNTOF(item_sectionID));

                    ImGui::Text("Item Price");
                    ImGui::SameLine(); ImGui::Dummy({ 6.0f, 0 }); ImGui::SameLine();
                    ImGui::InputText("##item_itemPrice", item_itemPrice, IM_COUNTOF(item_itemPrice));

                    ImGui::Text("No. of Items");
                    ImGui::SameLine();
                    ImGui::InputText("##item_numItems", item_numItems, IM_COUNTOF(item_numItems));

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Aisle"))
                {
                    numCols = 2;

                    item = false;
                    aisle = true;
                    section = false;
                    supplier = false;
                    transaction = false;

                    ImGui::Text("Aisle No.");
                    ImGui::SameLine(); ImGui::Dummy({ 34.0f, 0 }); ImGui::SameLine();
                    ImGui::InputText("##aisle_aisleNo", aisle_aisleNo, IM_COUNTOF(aisle_aisleNo));

                    ImGui::Text("No. of Sections");
                    ImGui::SameLine();
                    ImGui::InputText("##aisle_numSections", aisle_numSections, IM_COUNTOF(aisle_numSections));

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Section"))
                {
                    numCols = 3;

                    item = false;
                    aisle = false;
                    section = true;
                    supplier = false;
                    transaction = false;

                    ImGui::Text("Section I.D.");
                    ImGui::SameLine();
                    ImGui::InputText("##section_sectionID", section_sectionID, IM_COUNTOF(section_sectionID));

                    ImGui::Text("Section Name");
                    ImGui::SameLine();
                    ImGui::InputText("##section_sectionName", section_sectionName, IM_COUNTOF(section_sectionName));

                    ImGui::Text("Aisle No.");
                    ImGui::SameLine(); ImGui::Dummy({ 13.0f, 0 }); ImGui::SameLine();
                    ImGui::InputText("##section_aisleNo", section_aisleNo, IM_COUNTOF(section_aisleNo));

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Supplier"))
                {
                    numCols = 4;

                    item = false;
                    aisle = false;
                    section = false;
                    supplier = true;
                    transaction = false;

                    ImGui::Text("Supplier I.D.");
                    ImGui::SameLine();
                    ImGui::InputText("##supplier_supplierID", supplier_supplierID, IM_COUNTOF(supplier_supplierID));

                    ImGui::Text("Item I.D.");
                    ImGui::SameLine(); ImGui::Dummy({ 20.0f, 0 }); ImGui::SameLine();
                    ImGui::InputText("##supplier_itemID", supplier_itemID, IM_COUNTOF(supplier_itemID));

                    ImGui::Text("Item Cost");
                    ImGui::SameLine(); ImGui::Dummy({ 20.0f, 0 }); ImGui::SameLine();
                    ImGui::InputText("##supplier_itemCost", supplier_itemCost, IM_COUNTOF(supplier_itemCost));

                    ImGui::Text("Supplier Name");
                    ImGui::SameLine();
                    ImGui::InputText("##supplier_supplierName", supplier_supplierName, IM_COUNTOF(supplier_supplierName));

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Transaction"))
                {
                    numCols = 6;

                    item = false;
                    aisle = false;
                    section = false;
                    supplier = false;
                    transaction = true;

                    ImGui::Text("Transaction I.D.");
                    ImGui::SameLine(); ImGui::Dummy({ 7.0f, 0 }); ImGui::SameLine();
                    ImGui::InputText("##transaction_transactionID", transaction_transactionID, IM_COUNTOF(transaction_transactionID));

                    ImGui::Text("Item I.D.");
                    ImGui::SameLine(); ImGui::Dummy({ 56.0f, 0 }); ImGui::SameLine();
                    ImGui::InputText("##transaction_itemID", transaction_itemID, IM_COUNTOF(transaction_itemID));

                    ImGui::Text("Item Price");
                    ImGui::SameLine(); ImGui::Dummy({ 49.0f, 0 }); ImGui::SameLine();
                    ImGui::InputText("##transaction_itemPrice", transaction_itemPrice, IM_COUNTOF(transaction_itemPrice));

                    ImGui::Text("Tax Amount");
                    ImGui::SameLine(); ImGui::Dummy({ 49.0f, 0 }); ImGui::SameLine();
                    ImGui::InputText("##transaction_taxAmount", transaction_taxAmount, IM_COUNTOF(transaction_taxAmount));

                    ImGui::Text("Transaction Total");
                    ImGui::SameLine(); ImGui::Dummy({ 0, 0 }); ImGui::SameLine();
                    ImGui::InputText("##transaction_transactionTotal", transaction_transactionTotal, IM_COUNTOF(transaction_transactionTotal));

                    ImGui::Text("Transaction Date");
                    ImGui::SameLine(); ImGui::Dummy({ 7.0f, 0 }); ImGui::SameLine();
                    ImGui::InputText("##transaction_trasactionDate", transaction_trasactionDate, IM_COUNTOF(transaction_trasactionDate));

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        
        ImGui::EndChild();
        ImGui::SameLine();

        if (ImGui::BeginTable("##outputs", numCols))
        {
            if (item)
            {
                ImGui::TableSetupColumn("Item I.D.");
                ImGui::TableSetupColumn("Item Name");
                ImGui::TableSetupColumn("Aisle No.");
                ImGui::TableSetupColumn("Section I.D.");
                ImGui::TableSetupColumn("Item Price");
                ImGui::TableSetupColumn("No. of Items");
                ImGui::TableHeadersRow();
            }

            if (aisle)
            {
                ImGui::TableSetupColumn("Aisle No.");
                ImGui::TableSetupColumn("No. of Sections");
                ImGui::TableHeadersRow();
            }

            if (section)
            {
                ImGui::TableSetupColumn("Section I.D.");
                ImGui::TableSetupColumn("Section Name");
                ImGui::TableSetupColumn("Aisle No.");
                ImGui::TableHeadersRow();
            }

            if (supplier)
            {
                ImGui::TableSetupColumn("Supplier I.D.");
                ImGui::TableSetupColumn("Item I.D.");
                ImGui::TableSetupColumn("Item Cost");
                ImGui::TableSetupColumn("Supplier Name");
                ImGui::TableHeadersRow();
            }

            if (transaction)
            {
                ImGui::TableSetupColumn("Transaction I.D.");
                ImGui::TableSetupColumn("Item I.D.");
                ImGui::TableSetupColumn("Item Price");
                ImGui::TableSetupColumn("Tax Amount");
                ImGui::TableSetupColumn("Transaction Total");
                ImGui::TableSetupColumn("Transaction Date");
                ImGui::TableHeadersRow();
            }

            ImGui::EndTable();
        }

        ImGui::End();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0, 0.8f, 0, 1.0f };
        m_pd3dDeviceContext->OMSetRenderTargets(1, &m_mainRenderTargetView, nullptr);
        m_pd3dDeviceContext->ClearRenderTargetView(m_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        // Present
        HRESULT hr = m_pSwapChain->Present(1, 0);   // Present with vsync
        m_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }
}

void Renderer::Cleanup()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(m_hwnd);
    ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
}