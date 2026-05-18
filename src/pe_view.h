#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include "pe_parser.h"
#include "mem_buffer.h"

class PEView {
public:
    bool create(HWND hParent, HINSTANCE hInstance, int x, int y, int w, int h);
    void setPEInfo(const PEInfo& info, const MemBuffer* buffer);
    void clear();
    void resize(int x, int y, int w, int h);
    HWND getHwnd() const { return m_hTreeView; }

private:
    HWND m_hTreeView = nullptr;

    HTREEITEM addItem(HTREEITEM hParent, const wchar_t* text);
    HTREEITEM addItemA(HTREEITEM hParent, const char* text);
};
