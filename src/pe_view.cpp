#include "pe_view.h"
#include "resource.h"
#include <cstdio>

bool PEView::create(HWND hParent, HINSTANCE hInstance, int x, int y, int w, int h)
{
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    m_hTreeView = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_TREEVIEWW,
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        x, y, w, h,
        hParent,
        (HMENU)(UINT_PTR)IDC_TREEVIEW,
        hInstance,
        nullptr);

    if (!m_hTreeView)
        return false;

    // Monospace font
    HFONT hFont = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    SendMessage(m_hTreeView, WM_SETFONT, (WPARAM)hFont, TRUE);

    return true;
}

void PEView::clear()
{
    if (m_hTreeView)
        TreeView_DeleteAllItems(m_hTreeView);
}

void PEView::resize(int x, int y, int w, int h)
{
    if (m_hTreeView)
        MoveWindow(m_hTreeView, x, y, w, h, TRUE);
}

HTREEITEM PEView::addItem(HTREEITEM hParent, const wchar_t* text)
{
    TVINSERTSTRUCTW tvis = {};
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT;
    tvis.item.pszText = const_cast<wchar_t*>(text);
    return (HTREEITEM)SendMessageW(m_hTreeView, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
}

HTREEITEM PEView::addItemA(HTREEITEM hParent, const char* text)
{
    wchar_t wbuf[512];
    MultiByteToWideChar(CP_ACP, 0, text, -1, wbuf, 512);
    return addItem(hParent, wbuf);
}

void PEView::setPEInfo(const PEInfo& info, const MemBuffer* buffer)
{
    clear();

    if (!buffer)
        return;

    char buf[256];

    // File info
    HTREEITEM hFile = addItem(TVI_ROOT, L"File Info");
    snprintf(buf, sizeof(buf), "Size: %zu bytes (0x%zX)", buffer->size(), buffer->size());
    addItemA(hFile, buf);
    snprintf(buf, sizeof(buf), "PE Valid: %s", info.isValid ? "Yes" : "No");
    addItemA(hFile, buf);

    if (!info.isValid) {
        snprintf(buf, sizeof(buf), "Error: %s", info.errorMessage.c_str());
        addItemA(hFile, buf);
        TreeView_Expand(m_hTreeView, hFile, TVE_EXPAND);
        return;
    }

    // PE Headers
    HTREEITEM hHeaders = addItem(TVI_ROOT, L"PE Headers");

    snprintf(buf, sizeof(buf), "Type: %s", info.is64Bit ? "PE32+ (64-bit)" : "PE32 (32-bit)");
    addItemA(hHeaders, buf);
    snprintf(buf, sizeof(buf), "Machine: 0x%04X", info.machine);
    addItemA(hHeaders, buf);
    snprintf(buf, sizeof(buf), "Timestamp: 0x%08X", info.timestamp);
    addItemA(hHeaders, buf);
    snprintf(buf, sizeof(buf), "Entry Point RVA: 0x%08X", info.entryPointRVA);
    addItemA(hHeaders, buf);
    snprintf(buf, sizeof(buf), "Image Base: 0x%llX", info.imageBase);
    addItemA(hHeaders, buf);
    snprintf(buf, sizeof(buf), "Section Alignment: 0x%08X", info.sectionAlignment);
    addItemA(hHeaders, buf);
    snprintf(buf, sizeof(buf), "File Alignment: 0x%08X", info.fileAlignment);
    addItemA(hHeaders, buf);

    // Sections
    HTREEITEM hSections = addItem(TVI_ROOT, L"Sections");
    for (const auto& sec : info.sections) {
        snprintf(buf, sizeof(buf), "%-8s  VA=0x%08X  VSize=0x%08X  Raw=0x%08X  RSize=0x%08X",
                 sec.name, sec.virtualAddress, sec.virtualSize,
                 sec.rawDataOffset, sec.rawDataSize);
        addItemA(hSections, buf);
    }

    // Imports
    if (!info.imports.empty()) {
        snprintf(buf, sizeof(buf), "Imports (%zu DLLs)", info.imports.size());
        wchar_t wbuf[256];
        MultiByteToWideChar(CP_ACP, 0, buf, -1, wbuf, 256);
        HTREEITEM hImports = addItem(TVI_ROOT, wbuf);

        for (const auto& imp : info.imports) {
            snprintf(buf, sizeof(buf), "%s (%zu)", imp.dllName.c_str(), imp.functions.size());
            HTREEITEM hDll = addItemA(hImports, buf);

            for (const auto& func : imp.functions) {
                addItemA(hDll, func.c_str());
            }
        }
    }

    // Exports
    if (!info.exports.empty()) {
        snprintf(buf, sizeof(buf), "Exports (%zu)", info.exports.size());
        wchar_t wbuf[256];
        MultiByteToWideChar(CP_ACP, 0, buf, -1, wbuf, 256);
        HTREEITEM hExports = addItem(TVI_ROOT, wbuf);

        for (const auto& exp : info.exports) {
            snprintf(buf, sizeof(buf), "[%u] %s  RVA=0x%08X",
                     exp.ordinal, exp.name.c_str(), exp.rva);
            addItemA(hExports, buf);
        }
    }

    // Expand top-level items
    TreeView_Expand(m_hTreeView, hFile, TVE_EXPAND);
    TreeView_Expand(m_hTreeView, hHeaders, TVE_EXPAND);
    TreeView_Expand(m_hTreeView, hSections, TVE_EXPAND);
}
