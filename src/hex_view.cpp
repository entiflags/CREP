#include "hex_view.h"
#include "hex_engine.h"
#include "mem_buffer.h"
#include <cstdio>

const wchar_t* HexView::CLASS_NAME = L"CREP_HexView";

bool HexView::registerClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_IBEAM);
    wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wcex.lpszClassName = CLASS_NAME;
    return RegisterClassExW(&wcex) != 0;
}

bool HexView::create(HWND hParent, HINSTANCE hInstance, int x, int y, int w, int h)
{
    m_hWnd = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        CLASS_NAME,
        nullptr,
        WS_CHILD | WS_VSCROLL | WS_TABSTOP,
        x, y, w, h,
        hParent,
        nullptr,
        hInstance,
        nullptr);

    if (!m_hWnd)
        return false;

    SetWindowLongPtrW(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);

    // Create monospace font
    m_hFont = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    // Measure character size
    HDC hdc = GetDC(m_hWnd);
    HFONT oldFont = (HFONT)SelectObject(hdc, m_hFont);
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    m_charWidth = tm.tmAveCharWidth;
    m_charHeight = tm.tmHeight + tm.tmExternalLeading;
    SelectObject(hdc, oldFont);
    ReleaseDC(m_hWnd, hdc);

    ShowWindow(m_hWnd, SW_SHOW);
    return true;
}

void HexView::setEngine(HexEngine* engine)
{
    m_engine = engine;
    updateScrollbar();
    InvalidateRect(m_hWnd, nullptr, TRUE);
}

void HexView::refresh()
{
    updateScrollbar();
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void HexView::resize(int x, int y, int w, int h)
{
    if (m_hWnd)
        MoveWindow(m_hWnd, x, y, w, h, TRUE);
    updateScrollbar();
}

LRESULT CALLBACK HexView::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HexView* self = (HexView*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        if (self) self->onPaint(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_VSCROLL:
        if (self) self->onVScroll(wParam);
        return 0;

    case WM_MOUSEWHEEL:
        if (self) self->onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;

    case WM_KEYDOWN:
        if (self) self->onKeyDown(wParam);
        return 0;

    case WM_CHAR:
        if (self) self->onChar((wchar_t)wParam);
        return 0;

    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        if (self) self->onLButtonDown(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_MOUSEMOVE:
        if (self) self->onMouseMove(LOWORD(lParam), HIWORD(lParam), wParam);
        return 0;

    case WM_LBUTTONUP:
        if (self) self->m_selecting = false;
        ReleaseCapture();
        return 0;

    case WM_SIZE:
        if (self) self->updateScrollbar();
        return 0;

    case WM_SETFOCUS:
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_KILLFOCUS:
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int HexView::getOffsetColumnWidth() const
{
    // "00000000  " = 10 chars
    return m_charWidth * 10;
}

int HexView::getHexColumnWidth() const
{
    if (!m_engine) return 0;
    // Each byte = "XX " = 3 chars, plus extra space at midpoint
    size_t bpr = m_engine->getBytesPerRow();
    return m_charWidth * (int)(bpr * 3 + 1);
}

int HexView::getAsciiColumnX() const
{
    return getOffsetColumnWidth() + getHexColumnWidth();
}

void HexView::copySelection()
{
    if (!m_engine || !m_engine->getBuffer())
        return;

    HexSelection sel = m_engine->getSelection();
    if (sel.start >= sel.end)
        return;

    MemBuffer* buf = m_engine->getBuffer();
    size_t count = sel.end - sel.start;

    std::string hex;
    hex.reserve(count * 3);

    for (size_t i = 0; i < count; ++i)
    {
        if (i > 0)
            hex += ' ';
        uint8_t b = buf->readByte(sel.start + i);
        char tmp[4];
        snprintf(tmp, sizeof(tmp), "%02X", b);
        hex += tmp;
    }

    if (!OpenClipboard(m_hWnd))
        return;

    EmptyClipboard();

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, hex.size() + 1);
    if (hMem) {
        char* dest = (char*)GlobalLock(hMem);
        memcpy(dest, hex.c_str(), hex.size() + 1);
        GlobalUnlock(hMem);
        SetClipboardData(CF_TEXT, hMem);
    }

    CloseClipboard();
}

void HexView::onPaint(HDC hdc)
{
    if (!m_engine || !m_engine->getBuffer())
        return;

    MemBuffer* buf = m_engine->getBuffer();
    size_t fileSize = buf->size();
    size_t bpr = m_engine->getBytesPerRow();
    size_t scrollRow = m_engine->getScrollOffset();

    RECT rc;
    GetClientRect(m_hWnd, &rc);
    if (m_charHeight == 0) return;
    int visibleRows = (rc.bottom - rc.top) / m_charHeight;

    // Clear background
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

    HFONT oldFont = (HFONT)SelectObject(hdc, m_hFont);
    SetBkMode(hdc, TRANSPARENT);

    int offsetColW = getOffsetColumnWidth();
    int asciiX = getAsciiColumnX();

    for (int row = 0; row < visibleRows; ++row) {
        size_t rowIndex = scrollRow + row;
        size_t rowOffset = rowIndex * bpr;

        if (rowOffset >= fileSize)
            break;

        int y = row * m_charHeight;

        char offsetStr[16];
        snprintf(offsetStr, sizeof(offsetStr), "%08zX", rowOffset);
        SetTextColor(hdc, RGB(100, 100, 100));
        TextOutA(hdc, 2, y, offsetStr, 8);

        int hexX = offsetColW;
        size_t bytesThisRow = fileSize - rowOffset;
        if (bytesThisRow > bpr) bytesThisRow = bpr;

        for (size_t col = 0; col < bytesThisRow; ++col) {
            size_t off = rowOffset + col;
            uint8_t b = buf->readByte(off);

            char hexStr[4];
            snprintf(hexStr, sizeof(hexStr), "%02X", b);

            // Color: red for dirty, blue for selected, black for normal
            if (m_engine->isDirty(off)) {
                SetTextColor(hdc, RGB(220, 30, 30));
            }
            else if (off >= m_engine->getSelection().start && off < m_engine->getSelection().end) {
                SetTextColor(hdc, RGB(0, 80, 180));
            }
            else {
                SetTextColor(hdc, RGB(0, 0, 0));
            }

            int x = hexX + (int)col * m_charWidth * 3;
            // Add extra space at midpoint
            if (col >= bpr / 2)
                x += m_charWidth;

            TextOutA(hdc, x, y, hexStr, 2);

            // Cursor indicator
            if (off == m_engine->getCursor() && GetFocus() == m_hWnd) {
                RECT cursorRect = { x, y, x + m_charWidth * 2, y + m_charHeight };
                FrameRect(hdc, &cursorRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            }
        }

        int ax = asciiX;
        for (size_t col = 0; col < bytesThisRow; ++col) {
            size_t off = rowOffset + col;
            uint8_t b = buf->readByte(off);
            char ch = (b >= 0x20 && b <= 0x7E) ? (char)b : '.';

            if (m_engine->isDirty(off)) {
                SetTextColor(hdc, RGB(220, 30, 30));
            }
            else if (off >= m_engine->getSelection().start && off < m_engine->getSelection().end) {
                SetTextColor(hdc, RGB(0, 80, 180));
            }
            else {
                SetTextColor(hdc, RGB(80, 80, 80));
            }

            TextOutA(hdc, ax + (int)col * m_charWidth, y, &ch, 1);
        }
    }

    SelectObject(hdc, oldFont);
}

void HexView::updateScrollbar()
{
    if (!m_hWnd || !m_engine || !m_engine->getBuffer() || m_charHeight == 0)
        return;

    RECT rc;
    GetClientRect(m_hWnd, &rc);
    int visibleRows = (rc.bottom - rc.top) / m_charHeight;
    size_t totalRows = m_engine->getTotalRows();

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = (int)totalRows - 1;
    si.nPage = visibleRows;
    si.nPos = (int)m_engine->getScrollOffset();
    SetScrollInfo(m_hWnd, SB_VERT, &si, TRUE);
}

void HexView::onVScroll(WPARAM wParam)
{
    if (!m_engine) return;

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(m_hWnd, SB_VERT, &si);

    int pos = si.nPos;

    switch (LOWORD(wParam)) {
    case SB_LINEUP:        pos -= 1; break;
    case SB_LINEDOWN:      pos += 1; break;
    case SB_PAGEUP:        pos -= si.nPage; break;
    case SB_PAGEDOWN:      pos += si.nPage; break;
    case SB_THUMBTRACK:    pos = si.nTrackPos; break;
    }

    if (pos < 0) pos = 0;
    if (pos > si.nMax - (int)si.nPage + 1)
        pos = si.nMax - (int)si.nPage + 1;

    m_engine->setScrollOffset(pos);
    updateScrollbar();
    InvalidateRect(m_hWnd, nullptr, TRUE);
}

void HexView::onMouseWheel(int delta)
{
    if (!m_engine) return;

    int lines = delta / 120 * 3;
    int newPos = (int)m_engine->getScrollOffset() - lines;
    if (newPos < 0) newPos = 0;

    size_t maxScroll = m_engine->getTotalRows();
    if ((size_t)newPos >= maxScroll && maxScroll > 0)
        newPos = (int)maxScroll - 1;

    m_engine->setScrollOffset(newPos);
    updateScrollbar();
    InvalidateRect(m_hWnd, nullptr, TRUE);
}

void HexView::onKeyDown(WPARAM vk)
{
    if (!m_engine || !m_engine->getBuffer()) return;

    size_t cursor = m_engine->getCursor();
    size_t bpr = m_engine->getBytesPerRow();
    size_t fileSize = m_engine->getBuffer()->size();

    switch (vk) {
    case VK_RIGHT:
        if (cursor + 1 < fileSize)
            m_engine->setCursor(cursor + 1);
        break;
    case VK_LEFT:
        if (cursor > 0)
            m_engine->setCursor(cursor - 1);
        break;
    case VK_DOWN:
        if (cursor + bpr < fileSize)
            m_engine->setCursor(cursor + bpr);
        break;
    case VK_UP:
        if (cursor >= bpr)
            m_engine->setCursor(cursor - bpr);
        break;
    case VK_PRIOR: // Page Up
        if (cursor >= bpr * 16)
            m_engine->setCursor(cursor - bpr * 16);
        else
            m_engine->setCursor(0);
        break;
    case VK_NEXT: // Page Down
        if (cursor + bpr * 16 < fileSize)
            m_engine->setCursor(cursor + bpr * 16);
        else
            m_engine->setCursor(fileSize - 1);
        break;
    case VK_HOME:
        m_engine->setCursor(0);
        break;
    case VK_END:
        m_engine->setCursor(fileSize - 1);
        break;
    case 'Z':
        if (GetKeyState(VK_CONTROL) & 0x8000)
            m_engine->undo();
        break;
    case 'C':
        if (GetKeyState(VK_CONTROL) & 0x8000)
            copySelection();
        break;
    case 'Y':
        if (GetKeyState(VK_CONTROL) & 0x8000)
            m_engine->redo();
        break;
    }

    ensureCursorVisible();
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void HexView::onChar(wchar_t ch)
{
    if (!m_engine || !m_engine->getBuffer()) return;

    // Accept hex digits for editing
    int nibble = -1;
    if (ch >= '0' && ch <= '9') nibble = ch - '0';
    else if (ch >= 'a' && ch <= 'f') nibble = ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F') nibble = ch - 'A' + 10;

    if (nibble < 0)
        return;

    size_t cursor = m_engine->getCursor();
    uint8_t current = m_engine->getBuffer()->readByte(cursor);

    uint8_t newValue = (uint8_t)((nibble << 4) | (current & 0x0F));
    m_engine->editByte(cursor, newValue);

    // Move cursor forward
    if (cursor + 1 < m_engine->getBuffer()->size())
        m_engine->setCursor(cursor + 1);

    ensureCursorVisible();
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void HexView::onLButtonDown(int x, int y)
{
    size_t offset = hitTest(x, y);
    if (offset != SIZE_MAX && m_engine) {
        m_engine->setCursor(offset);
        m_engine->setSelection(offset, offset);
        m_selecting = true;
        SetCapture(m_hWnd);
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

void HexView::onMouseMove(int x, int y, WPARAM keys)
{
    if (!m_selecting || !m_engine || !(keys & MK_LBUTTON))
        return;

    size_t offset = hitTest(x, y);
    if (offset == SIZE_MAX)
        return;

    size_t start = m_engine->getCursor();
    if (offset >= start)
        m_engine->setSelection(start, offset + 1);
    else
        m_engine->setSelection(offset, start + 1);

    InvalidateRect(m_hWnd, nullptr, FALSE);
}

size_t HexView::hitTest(int x, int y) const
{
    if (!m_engine || !m_engine->getBuffer())
        return SIZE_MAX;

    size_t bpr = m_engine->getBytesPerRow();
    size_t scrollRow = m_engine->getScrollOffset();

    int row = y / m_charHeight;
    size_t rowIndex = scrollRow + row;
    size_t rowOffset = rowIndex * bpr;

    if (rowOffset >= m_engine->getBuffer()->size())
        return SIZE_MAX;

    int offsetColW = getOffsetColumnWidth();
    int asciiX = getAsciiColumnX();

    // Check if click is in hex column
    if (x >= offsetColW && x < asciiX) {
        int relX = x - offsetColW;
        int colWidth = m_charWidth * 3;
        int col = relX / colWidth;
        // Account for midpoint gap
        if ((size_t)col >= bpr / 2)
            col = (relX - m_charWidth) / colWidth;
        if (col < 0) col = 0;
        if ((size_t)col >= bpr) col = (int)bpr - 1;

        size_t offset = rowOffset + col;
        if (offset < m_engine->getBuffer()->size())
            return offset;
    }
    // Check if click is in ASCII column
    else if (x >= asciiX) {
        int col = (x - asciiX) / m_charWidth;
        if (col < 0) col = 0;
        if ((size_t)col >= bpr) col = (int)bpr - 1;

        size_t offset = rowOffset + col;
        if (offset < m_engine->getBuffer()->size())
            return offset;
    }

    return SIZE_MAX;
}

void HexView::ensureCursorVisible()
{
    if (!m_engine || !m_engine->getBuffer() || m_charHeight == 0) return;

    size_t cursor = m_engine->getCursor();
    size_t bpr = m_engine->getBytesPerRow();
    size_t cursorRow = cursor / bpr;
    size_t scrollRow = m_engine->getScrollOffset();

    RECT rc;
    GetClientRect(m_hWnd, &rc);
    size_t visibleRows = (rc.bottom - rc.top) / m_charHeight;

    if (cursorRow < scrollRow) {
        m_engine->setScrollOffset(cursorRow);
        updateScrollbar();
    }
    else if (cursorRow >= scrollRow + visibleRows) {
        m_engine->setScrollOffset(cursorRow - visibleRows + 1);
        updateScrollbar();
    }
}
