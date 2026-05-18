#include "main_window.h"
#include "resource.h"

#include <commdlg.h>
#include <string>

#pragma comment(lib, "comctl32.lib")

static const wchar_t* WINDOW_CLASS = L"CREP_MainWindow";
static const wchar_t* WINDOW_TITLE = L"C.R.E.P - Cool Reverse Engineering Program";

// --- Public ---

bool MainWindow::create(HINSTANCE hInstance, int nCmdShow)
{
    m_hInstance = hInstance;

    // Register HexView custom control class
    HexView::registerClass(hInstance);

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CREP));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wcex.lpszClassName = WINDOW_CLASS;
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CREP));
    RegisterClassExW(&wcex);

    m_hWnd = CreateWindowExW(
        0,
        WINDOW_CLASS,
        WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 720,
        nullptr, nullptr, hInstance, nullptr);

    if (!m_hWnd)
        return false;

    SetWindowLongPtrW(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);

    onCreate(m_hWnd);

    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);
    return true;
}

// --- WndProc ---

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MainWindow* self = (MainWindow*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_COMMAND:
        if (self) self->onCommand(LOWORD(wParam));
        return 0;

    case WM_SIZE:
        if (self) self->onResize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_CLOSE:
        if (self) self->onClose();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// --- Private handlers ---

void MainWindow::onCreate(HWND hWnd)
{
    createMenuBar();

    RECT rc;
    GetClientRect(hWnd, &rc);

    int hexW = (int)(rc.right * m_splitRatio);
    int peW = rc.right - hexW;

    m_hexView.create(hWnd, m_hInstance, 0, 0, hexW, rc.bottom);
    m_peView.create(hWnd, m_hInstance, hexW, 0, peW, rc.bottom);
}

void MainWindow::onCommand(WORD id)
{
    switch (id)
    {
    case IDM_FILE_OPEN:   doFileOpen();   break;
    case IDM_FILE_SAVE:   doFileSave();   break;
    case IDM_FILE_SAVEAS: doFileSaveAs(); break;
    case IDM_FILE_EXIT:   onClose();      break;
    }
}

void MainWindow::onResize(int cx, int cy)
{
    layoutPanels(cx, cy);
}

void MainWindow::layoutPanels(int cx, int cy)
{
    if (!m_hexView.getHwnd() || !m_peView.getHwnd())
        return;

    int hexW = (int)(cx * m_splitRatio);
    if (hexW < 100) hexW = 100;
    int peW = cx - hexW;
    if (peW < 100) {
        peW = 100;
        hexW = cx - peW;
    }

    m_hexView.resize(0, 0, hexW, cy);
    m_peView.resize(hexW, 0, peW, cy);
}

void MainWindow::onClose()
{
    FileContext* ctx = m_fileManager.getActiveFile();
    if (ctx && ctx->isModified) {
        int result = MessageBoxW(m_hWnd,
            L"You have unsaved changes. Save before closing?",
            L"C.R.E.P",
            MB_YESNOCANCEL | MB_ICONQUESTION);

        if (result == IDCANCEL)
            return;
        if (result == IDYES) {
            if (!m_fileManager.saveFile(ctx))
                return;
        }
    }
    DestroyWindow(m_hWnd);
}

void MainWindow::doFileOpen()
{
    wchar_t szFile[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Executable Files\0*.exe;*.dll;*.sys;*.ocx\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (!GetOpenFileNameW(&ofn))
        return;

    FileContext* ctx = m_fileManager.openFile(szFile);
    if (!ctx) {
        MessageBoxW(m_hWnd, L"Failed to open file.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    updateTitle();

    // Set up hex view
    m_hexEngine.setBuffer(ctx->buffer.get());
    m_hexView.setEngine(&m_hexEngine);

    // Set up PE view
    if (ctx->isPE) {
        m_peView.setPEInfo(ctx->peInfo, ctx->buffer.get());
    }
    else {
        m_peView.clear();
    }
}

void MainWindow::doFileSave()
{
    FileContext* ctx = m_fileManager.getActiveFile();
    if (!ctx) return;

    if (!m_fileManager.saveFile(ctx)) {
        MessageBoxW(m_hWnd, L"Failed to save file.", L"Error", MB_OK | MB_ICONERROR);
    }
    else {
        updateTitle();
    }
}

void MainWindow::doFileSaveAs()
{
    FileContext* ctx = m_fileManager.getActiveFile();
    if (!ctx) return;

    wchar_t szFile[MAX_PATH] = {};
    wcscpy_s(szFile, ctx->filePath.c_str());

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (!GetSaveFileNameW(&ofn))
        return;

    if (!m_fileManager.saveFileAs(ctx, szFile)) {
        MessageBoxW(m_hWnd, L"Failed to save file.", L"Error", MB_OK | MB_ICONERROR);
    }
    else {
        updateTitle();
    }
}

void MainWindow::createMenuBar()
{
    m_hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();

    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open\tCtrl+O");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVEAS, L"Save &As...");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit\tAlt+F4");

    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
    SetMenu(m_hWnd, m_hMenu);

    ACCEL accels[] = {
        { FCONTROL | FVIRTKEY, 'O', IDM_FILE_OPEN },
        { FCONTROL | FVIRTKEY, 'S', IDM_FILE_SAVE },
    };
    m_hAccel = CreateAcceleratorTableW(accels, _countof(accels));
}

void MainWindow::updateTitle()
{
    FileContext* ctx = m_fileManager.getActiveFile();
    std::wstring title = L"C.R.E.P";

    if (ctx) {
        title += L" - ";
        size_t lastSlash = ctx->filePath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos)
            title += ctx->filePath.substr(lastSlash + 1);
        else
            title += ctx->filePath;

        if (ctx->isModified)
            title += L" *";
    }

    SetWindowTextW(m_hWnd, title.c_str());
}
