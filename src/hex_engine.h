#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <set>

class MemBuffer;

struct HexEditOp {
	size_t offset;
	uint8_t oldValue;
	uint8_t newValue;
};

struct HexSelection {
	size_t start;
	size_t end;
};

class HexEngine {
public:
	void setBuffer(MemBuffer* buffer);
	MemBuffer* getBuffer() const {
		return m_buffer;
	}

	// Navigation
	void setCursor(size_t offset);
	size_t getCursor() const;
	void setSelection(size_t start, size_t end);
	HexSelection getSelection() const;

	// Editing
	bool editByte(size_t offset, uint8_t value);
	bool undo();
	bool redo();
	bool canUndo() const;
	bool canRedo() const;

	// Search
	size_t findNext(const uint8_t* pattern, size_t patternLen, size_t startOffset);
	size_t findPrev(const uint8_t* pattern, size_t patternLen, size_t startOffset);

	// State
	bool isDirty(size_t offset) const;
	bool hasModifications() const {
		return !m_dirtyOffsets.empty();
	}

	// Display helpers
	size_t getBytesPerRow() const {
		return m_bytesPerRow;
	}
	void setBytesPerRow(size_t count);
	size_t getVisibleRows(size_t viewHeight, size_t rowHeight) const;
	size_t getScrollOffset() const {
		return m_scrollOffset;
	}
	void setScrollOffset(size_t offset);
	size_t getTotalRows() const;

private:
	MemBuffer* m_buffer = nullptr;
	size_t m_cursor = 0;
	HexSelection m_selection = { 0, 0 };
	size_t m_bytesPerRow = 16;
	size_t m_scrollOffset = 0;

	std::vector<HexEditOp> m_undoStack;
	size_t m_undoPos = 0;
	std::set<size_t> m_dirtyOffsets;
};
