#include "hex_engine.h"
#include "mem_buffer.h"

void HexEngine::setBuffer(MemBuffer* buffer)
{
    m_buffer = buffer;
    m_cursor = 0;
    m_selection = { 0, 0 };
    m_scrollOffset = 0;
    m_undoStack.clear();
    m_undoPos = 0;
    m_dirtyOffsets.clear();
}

void HexEngine::setCursor(size_t offset)
{
    if (!m_buffer || offset >= m_buffer->size())
        return;
    m_cursor = offset;
}

size_t HexEngine::getCursor() const
{
    return m_cursor;
}

void HexEngine::setSelection(size_t start, size_t end)
{
    if (!m_buffer)
        return;

    // Swap if backwards
    if (start > end) {
        size_t tmp = start;
        start = end;
        end = tmp;
    }

    // Clamp to buffer size
    if (start > m_buffer->size())
        start = m_buffer->size();
    if (end > m_buffer->size())
        end = m_buffer->size();

    m_selection.start = start;
    m_selection.end = end;
}

HexSelection HexEngine::getSelection() const
{
    return m_selection;
}

bool HexEngine::editByte(size_t offset, uint8_t value)
{
    if (!m_buffer || !m_buffer->isValidOffset(offset))
        return false;

    uint8_t oldValue = m_buffer->readByte(offset);

    // No-op if same value
    if (oldValue == value)
        return true;

    // Truncate redo history
    m_undoStack.resize(m_undoPos);

    // Record operation
    m_undoStack.push_back({ offset, oldValue, value });
    m_undoPos = m_undoStack.size();

    // Apply
    m_buffer->writeByte(offset, value);
    m_dirtyOffsets.insert(offset);

    return true;
}

bool HexEngine::undo()
{
    if (m_undoPos == 0)
        return false;

    --m_undoPos;
    const HexEditOp& op = m_undoStack[m_undoPos];
    m_buffer->writeByte(op.offset, op.oldValue);

    return true;
}

bool HexEngine::redo()
{
    if (m_undoPos >= m_undoStack.size())
        return false;

    const HexEditOp& op = m_undoStack[m_undoPos];
    m_buffer->writeByte(op.offset, op.newValue);
    ++m_undoPos;

    return true;
}

bool HexEngine::canUndo() const
{
    return m_undoPos > 0;
}

bool HexEngine::canRedo() const
{
    return m_undoPos < m_undoStack.size();
}

size_t HexEngine::findNext(const uint8_t* pattern, size_t patternLen, size_t startOffset)
{
    if (!m_buffer || !pattern || patternLen == 0)
        return SIZE_MAX;

    if (startOffset >= m_buffer->size())
        return SIZE_MAX;

    if (patternLen > m_buffer->size())
        return SIZE_MAX;

    size_t searchEnd = m_buffer->size() - patternLen;

    for (size_t i = startOffset; i <= searchEnd; ++i) {
        bool match = true;
        for (size_t j = 0; j < patternLen; ++j) {
            if (m_buffer->readByte(i + j) != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match)
            return i;
    }

    return SIZE_MAX;
}

size_t HexEngine::findPrev(const uint8_t* pattern, size_t patternLen, size_t startOffset)
{
    if (!m_buffer || !pattern || patternLen == 0)
        return SIZE_MAX;

    if (m_buffer->size() < patternLen)
        return SIZE_MAX;

    // Start searching backwards from startOffset - 1
    size_t maxStart = m_buffer->size() - patternLen;

    // If startOffset is 0, nothing before it
    if (startOffset == 0)
        return SIZE_MAX;

    size_t begin = startOffset - 1;
    if (begin > maxStart)
        begin = maxStart;

    for (size_t i = begin; ; --i) {
        bool match = true;
        for (size_t j = 0; j < patternLen; ++j) {
            if (m_buffer->readByte(i + j) != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match)
            return i;

        if (i == 0)
            break;
    }

    return SIZE_MAX;
}

bool HexEngine::isDirty(size_t offset) const
{
    return m_dirtyOffsets.count(offset) > 0;
}

void HexEngine::setBytesPerRow(size_t count)
{
    if (count == 8 || count == 16 || count == 32)
        m_bytesPerRow = count;
}

size_t HexEngine::getVisibleRows(size_t viewHeight, size_t rowHeight) const
{
    if (rowHeight == 0)
        return 0;
    return viewHeight / rowHeight;
}

void HexEngine::setScrollOffset(size_t offset)
{
    m_scrollOffset = offset;
}

size_t HexEngine::getTotalRows() const
{
    if (!m_buffer || m_buffer->size() == 0 || m_bytesPerRow == 0)
        return 0;
    return (m_buffer->size() + m_bytesPerRow - 1) / m_bytesPerRow;
}
