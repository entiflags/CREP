#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include "pe_parser.h"
#include "mem_buffer.h"

// PE Info panel — displays parsed PE data in a TreeView control
class PEView {
public:
    // Create the TreeView child window inside the parent
    bool create(HWND hParent, HINSTANCE hInstance, int x, int y, int w, int h);

    // Update display with new PE info
    void setPEInfo(const PEInfo& info, const MemBuffer* buffer);

    // Clear all displayed info
    void clear();

    // Resize the control
    void resize(int x, int y, int w, int h);

    // Get the underlying HWND
    HWND getHwnd() const { return m_hTreeView; }

private:
    HWND m_hTreeView = nullptr;

    HTREEITEM addItem(HTREEITEM hParent, const wchar_t* text);
    HTREEITEM addItemA(HTREEITEM hParent, const char* text);
};
