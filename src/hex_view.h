#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class HexEngine;

class HexView {
public:
    static bool registerClass(HINSTANCE hInstance);

    bool create(HWND hParent, HINSTANCE hInstance, int x, int y, int w, int h);
    void setEngine(HexEngine* engine);
    void refresh();
    void resize(int x, int y, int w, int h);

    HWND getHwnd() const { return m_hWnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void onPaint(HDC hdc);
    void onVScroll(WPARAM wParam);
    void onMouseWheel(int delta);
    void onMouseMove(int x, int y, WPARAM keys);
    void onKeyDown(WPARAM vk);
    void onLButtonDown(int x, int y);
    void onChar(wchar_t ch);

    void updateScrollbar();
    void ensureCursorVisible();

    size_t hitTest(int x, int y) const;

    void copySelection();

    void doGotoOffset();

    // Metrics
    int  getRowHeight() const { return m_charHeight; }
    int  getOffsetColumnWidth() const;
    int  getHexColumnWidth() const;
    int  getAsciiColumnX() const;
    int  getCharWidth() const { return m_charWidth; }

    HWND        m_hWnd = nullptr;
    HexEngine*  m_engine = nullptr;
    HFONT       m_hFont = nullptr;
    int         m_charWidth = 8;
    int         m_charHeight = 16;
    bool        m_selecting = false;


    static const wchar_t* CLASS_NAME;
};
