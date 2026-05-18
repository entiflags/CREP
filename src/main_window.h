#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "file_manager.h"
#include "hex_engine.h"
#include "hex_view.h"
#include "pe_view.h"

class MainWindow {
public:
    bool create(HINSTANCE hInstance, int nCmdShow);
    HWND getHwnd() const { return m_hWnd; }
    HACCEL getAccel() const { return m_hAccel; }

    FileManager& getFileManager() { return m_fileManager; }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void onCreate(HWND hWnd);
    void onCommand(WORD id);
    void onResize(int cx, int cy);
    void onClose();

    void doFileOpen();
    void doFileSave();
    void doFileSaveAs();

    void createMenuBar();
    void updateTitle();
    void layoutPanels(int cx, int cy);

    HWND        m_hWnd = nullptr;
    HINSTANCE   m_hInstance = nullptr;
    HMENU       m_hMenu = nullptr;
    HACCEL      m_hAccel = nullptr;

    // Child panels
    HexView     m_hexView;
    PEView      m_peView;

    // Core engine
    HexEngine   m_hexEngine;

    // Application logic
    FileManager m_fileManager;

    // Layout: split ratio (hex view gets this fraction of width)
    float m_splitRatio = 0.6f;
};
