/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */
/* The C embedding facade over the shitty VT core: one opaque terminal,
 * fed bytes, read back as a grid. Everything the terminal wants from
 * its surroundings arrives through shitty_vt_callbacks; everything it
 * emits toward the child sits in the reply buffer until the embedder
 * drains it into its own pty. */

#pragma once

#include <stddef.h>
#include <stdint.h>

/* shitty_vt_cell.attributes bits */
#define SHITTY_VT_ATTR_BOLD (1u << 0)
#define SHITTY_VT_ATTR_FAINT (1u << 1)
#define SHITTY_VT_ATTR_ITALIC (1u << 2)
#define SHITTY_VT_ATTR_BLINK (1u << 3)
#define SHITTY_VT_ATTR_INVERSE (1u << 4)
#define SHITTY_VT_ATTR_CONCEAL (1u << 5)
#define SHITTY_VT_ATTR_STRIKE (1u << 6)
#define SHITTY_VT_ATTR_OVERLINE (1u << 7)

/* shitty_vt_modes() bits */
#define SHITTY_VT_MODE_ALT_SCREEN (1u << 0)
#define SHITTY_VT_MODE_BRACKETED_PASTE (1u << 1)
#define SHITTY_VT_MODE_APP_CURSOR_KEYS (1u << 2)
#define SHITTY_VT_MODE_APP_KEYPAD (1u << 3)
#define SHITTY_VT_MODE_FOCUS_EVENTS (1u << 4)
#define SHITTY_VT_MODE_AUTO_WRAP (1u << 5)
#define SHITTY_VT_MODE_ORIGIN (1u << 6)
#define SHITTY_VT_MODE_INSERT (1u << 7)
#define SHITTY_VT_MODE_CURSOR_VISIBLE (1u << 8)
#define SHITTY_VT_MODE_SCREEN_REVERSE (1u << 9)
#define SHITTY_VT_MODE_SYNCHRONIZED_OUTPUT (1u << 10)
#define SHITTY_VT_MODE_MOUSE_CLICK (1u << 11)
#define SHITTY_VT_MODE_MOUSE_DRAG (1u << 12)
#define SHITTY_VT_MODE_MOUSE_MOTION (1u << 13)
#define SHITTY_VT_MODE_MOUSE_SGR (1u << 14)
/* DECSET 1007: while the alternate screen is up, wheel input should be
 * sent as arrow keys rather than scrolling a history it does not keep. */
#define SHITTY_VT_MODE_ALTERNATE_SCROLL (1u << 15)

#ifdef __cplusplus
extern "C" {
#endif
    typedef struct shitty_vt shitty_vt;

    /* One readable cell. Colors are resolved through the palette into
     * 0x00BBGGRR - the little-endian view of struct { uint8_t r, g, b; }.
     * The grapheme is the cell's decoded codepoints; an empty cell has
     * grapheme_len 0. The pointer is valid only for the duration of the
     * shitty_vt_each_cell callback. */
    typedef struct shitty_vt_cell {
        const uint32_t* grapheme;
        size_t grapheme_len;
        uint32_t foreground;
        uint32_t background;
        uint32_t underline_color;
        uint16_t attributes;
        /* 0 none, 1 straight, 2 double, 3 curly, 4 dotted, 5 dashed */
        uint8_t underline_style;
        /* 1 or 2; the continuation of a wide cell is not reported */
        uint8_t width;
    } shitty_vt_cell;

    /* row is a row of the current view, not of the live screen: while the
     * view sits in the scrollback the cursor can be at or past rows, which
     * means it is simply not on screen and nothing should be drawn for it. */
    typedef struct shitty_vt_cursor {
        uint16_t column;
        uint16_t row;
        /* 0 hidden, 1 filled block, 2 hollow block, 3 underline, 4 bar */
        uint8_t style;
        uint8_t visible;
    } shitty_vt_cursor;

    typedef void (*shitty_vt_cell_fn)(void* user, uint16_t row, uint16_t column, const shitty_vt_cell* cell);

    /* Everything the terminal may want from its embedder. Every callback
     * may be null; user is passed back verbatim. The terminal keeps the
     * pointer, not a copy - the struct must outlive it. Nothing fires
     * during shitty_vt_new: the first callback an embedder sees is the
     * application's own doing. */
    typedef struct shitty_vt_callbacks {
        void* user;
        /* The application published a new title. */
        void (*title_changed)(void* user, const uint8_t* title, size_t len);
        /* BEL, or an attention request - present it however fits. */
        void (*bell)(void* user);
        /* The presentation moved; re-read the grid when convenient. */
        void (*damaged)(void* user);
        /* A hyperlink wants opening on the embedder's desktop. */
        void (*open_uri)(void* user, const uint8_t* uri, size_t len);
        /* OSC 52: the application replaced a selection. 0 is the primary
         * selection, 1 the clipboard. */
        void (*clipboard_set)(void* user, int clipboard, const uint8_t* bytes, size_t len);
        /* XTWINOPS asked for a grid this large; the embedder decides and
         * answers with shitty_vt_resize if it agrees. */
        void (*resize_request)(void* user, uint16_t columns, uint16_t rows);
    } shitty_vt_callbacks;

    /* callbacks may be null when the embedder wants none. */
    shitty_vt* shitty_vt_new(uint16_t columns, uint16_t rows, uint16_t save_lines, const shitty_vt_callbacks* callbacks);
    void shitty_vt_free(shitty_vt*);
    void shitty_vt_feed(shitty_vt*, const uint8_t* bytes, size_t len);
    void shitty_vt_resize(shitty_vt*, uint16_t columns, uint16_t rows);

    /* Terminal-generated replies (DA, DSR, ...) the embedder must forward
     * to its pty. Drains up to cap bytes into out and returns how many. */
    size_t shitty_vt_take_replies(shitty_vt*, uint8_t* out, size_t cap);

    /* Walks the visible grid row-major; wide-cell continuations are
     * skipped. Reads whatever the view currently shows, so it follows
     * shitty_vt_scroll into the scrollback. */
    void shitty_vt_each_cell(shitty_vt*, shitty_vt_cell_fn, void* user);

    /* Moves the view through the scrollback: positive rows scroll up into
     * history, negative back toward the live bottom. Clamped to the retained
     * history, and inert on the alternate screen, which keeps none. Returns
     * the resulting offset. */
    uint32_t shitty_vt_scroll(shitty_vt*, int32_t rows);

    /* Places the view so that offset rows of history sit above it; 0 is
     * the live bottom. Clamped like shitty_vt_scroll. Returns the resulting
     * offset. */
    uint32_t shitty_vt_scroll_to(shitty_vt*, uint32_t offset);

    /* Rows of history above the live bottom the view currently shows;
     * 0 while it is live. */
    uint32_t shitty_vt_scroll_offset(const shitty_vt*);

    /* Rows of scrollback retained, which is the largest offset
     * shitty_vt_scroll_to will accept. */
    uint32_t shitty_vt_history_rows(const shitty_vt*);

    /* Rows addressable through shitty_vt_row_cells: the retained history
     * followed by the visible grid. Index 0 is the oldest row still kept and
     * the last index is the bottom of the live screen. */
    uint32_t shitty_vt_total_rows(const shitty_vt*);

    /* What the terminal is spending on its grid and history. Cells only:
     * grapheme clusters, hyperlinks and sixel patches live in a separate
     * store this does not count, so treat it as the floor of the real cost
     * rather than the whole of it. */
    typedef struct shitty_vt_memory {
        /* Row slots actually backed by cells. The ring behind them is
         * rounded up to a power of two, so this can exceed capacity_rows:
         * it is what the screen costs, not what it is allowed to hold. */
        uint32_t allocated_rows;
        /* Rows the terminal will keep: rows + save_lines. */
        uint32_t capacity_rows;
        uint32_t columns;
        /* Bytes in one cell, so an embedder can do its own arithmetic. */
        uint32_t cell_size;
        /* allocated_rows * columns * cell_size. */
        uint64_t cell_bytes;
    } shitty_vt_memory;

    void shitty_vt_memory_usage(const shitty_vt*, shitty_vt_memory* out);

    /* Changes how many rows of scrollback the terminal keeps. Lowering it
     * drops the oldest rows that no longer fit, and does so at once rather
     * than as the history is overwritten. The visible grid is untouched. */
    void shitty_vt_set_save_lines(shitty_vt*, uint16_t save_lines);

    /* Walks one row by absolute index, oldest first, leaving the view
     * where it is - the row argument handed to the callback is the index
     * asked for. An index past the last row visits nothing. Use this to read
     * the scrollback without scrolling; use shitty_vt_each_cell to read what
     * the user is looking at. */
    void shitty_vt_row_cells(shitty_vt*, uint32_t index, shitty_vt_cell_fn, void* user);
    shitty_vt_cursor shitty_vt_cursor_state(const shitty_vt*);
    uint32_t shitty_vt_modes(const shitty_vt*);
#ifdef __cplusplus
}
#endif
