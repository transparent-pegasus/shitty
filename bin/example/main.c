/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */
/* The C embedding example: feed a recorded byte stream into a
 * shitty_vt, then print what an embedder can read back - the grid as
 * UTF-8 text, the cursor, the mode bits, the terminal's replies and
 * the scrollback position.
 *
 * Usage: example [columns rows save_lines] [stream-file]
 * With no file the stream is read from stdin. */

#include "lib/embed/shitty_vt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Enough for one grapheme cluster rendered as UTF-8. */
#define CELL_TEXT_CAP 64

struct grid {
    char* text; /* rows * columns slots of CELL_TEXT_CAP bytes */
    uint16_t columns;
    uint16_t rows;
};

static size_t utf8_encode(uint32_t codepoint, char* out) {
    if (codepoint < 0x80) {
        out[0] = (char)codepoint;
        return 1;
    }
    if (codepoint < 0x800) {
        out[0] = (char)(0xc0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3f));
        return 2;
    }
    if (codepoint < 0x10000) {
        out[0] = (char)(0xe0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        out[2] = (char)(0x80 | (codepoint & 0x3f));
        return 3;
    }
    out[0] = (char)(0xf0 | (codepoint >> 18));
    out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
    out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
    out[3] = (char)(0x80 | (codepoint & 0x3f));
    return 4;
}

static void collect_cell(void* user, uint16_t row, uint16_t column, const shitty_vt_cell* cell) {
    struct grid* grid = user;
    char* slot;
    /* The stream may have resized the terminal past our snapshot. */
    if (row >= grid->rows || column >= grid->columns) {
        return;
    }
    slot = grid->text + ((size_t)row * grid->columns + column) * CELL_TEXT_CAP;
    size_t used = 0;
    size_t index;
    for (index = 0; index < cell->grapheme_len && used + 4 < CELL_TEXT_CAP; ++index) {
        used += utf8_encode(cell->grapheme[index], slot + used);
    }
    if (used == 0) {
        slot[used++] = ' ';
    }
    slot[used] = '\0';
}

static void on_title(void* user, const uint8_t* title, size_t len) {
    (void)user;
    printf("title: %.*s\n", (int)len, (const char*)title);
}

static void on_bell(void* user) {
    (void)user;
    printf("bell\n");
}

/* Collects one row's text, ignoring the row number the callback repeats. */
static void collect_row(void* user, uint16_t row, uint16_t column, const shitty_vt_cell* cell) {
    struct grid* const target = (struct grid*)user;
    (void)row;
    collect_cell(user, 0, column, cell);
    (void)target;
}

int main(int argc, char** argv) {
    uint16_t columns = 80;
    uint16_t rows = 24;
    uint16_t save_lines = 0;
    const char* path = NULL;
    int scroll = 0;
    long scroll_to = -1;
    int dump_rows = 0;
    long new_save_lines = -1;
    if (argc >= 4) {
        columns = (uint16_t)atoi(argv[1]);
        rows = (uint16_t)atoi(argv[2]);
        save_lines = (uint16_t)atoi(argv[3]);
        path = argc >= 5 ? argv[4] : NULL;
        /* Rows to scroll up into the scrollback before reading the grid. */
        scroll = argc >= 6 ? atoi(argv[5]) : 0;
        /* An absolute offset to settle on afterwards; negative leaves the
         * view where the relative scroll put it. */
        scroll_to = argc >= 7 ? atol(argv[6]) : -1;
        /* Print every addressable row, history first, after the grid. */
        dump_rows = argc >= 8 ? atoi(argv[7]) : 0;
        /* A history cap to apply after feeding; negative keeps the one
         * the terminal was built with. */
        new_save_lines = argc >= 9 ? atol(argv[8]) : -1;
    } else if (argc == 2) {
        path = argv[1];
    }

    shitty_vt_callbacks callbacks = {0};
    callbacks.title_changed = on_title;
    callbacks.bell = on_bell;

    shitty_vt* vt = shitty_vt_new(columns, rows, save_lines, &callbacks);
    if (vt == NULL) {
        fprintf(stderr, "example: shitty_vt_new failed\n");
        return 1;
    }

    FILE* input = path != NULL ? fopen(path, "rb") : stdin;
    if (input == NULL) {
        fprintf(stderr, "example: can not open %s\n", path);
        shitty_vt_free(vt);
        return 1;
    }
    for (;;) {
        uint8_t chunk[16 * 1024];
        const size_t count = fread(chunk, 1, sizeof(chunk), input);
        if (count == 0) {
            break;
        }
        shitty_vt_feed(vt, chunk, count);
    }
    if (input != stdin) {
        fclose(input);
    }

    if (new_save_lines >= 0) {
        shitty_vt_set_save_lines(vt, (uint16_t)new_save_lines);
    }

    shitty_vt_scroll(vt, scroll);
    if (scroll_to >= 0) {
        shitty_vt_scroll_to(vt, (uint32_t)scroll_to);
    }

    struct grid grid;
    grid.columns = columns;
    grid.rows = rows;
    grid.text = calloc((size_t)columns * rows, CELL_TEXT_CAP);
    if (grid.text == NULL) {
        shitty_vt_free(vt);
        return 1;
    }
    /* Continuations of wide cells are not reported; leave them blank. */
    {
        size_t slot;
        for (slot = 0; slot < (size_t)columns * rows; ++slot) {
            grid.text[slot * CELL_TEXT_CAP] = ' ';
            grid.text[slot * CELL_TEXT_CAP + 1] = '\0';
        }
    }
    shitty_vt_each_cell(vt, collect_cell, &grid);

    {
        uint16_t row;
        uint16_t column;
        for (row = 0; row < rows; ++row) {
            for (column = 0; column < columns; ++column) {
                fputs(grid.text + ((size_t)row * columns + column) * CELL_TEXT_CAP, stdout);
            }
            fputc('\n', stdout);
        }
    }
    free(grid.text);

    {
        const shitty_vt_cursor cursor = shitty_vt_cursor_state(vt);
        printf("cursor: %u %u style=%u visible=%u\n", cursor.column, cursor.row, cursor.style, cursor.visible);
    }
    printf("modes: 0x%x\n", shitty_vt_modes(vt));

    {
        uint8_t replies[4096];
        const size_t count = shitty_vt_take_replies(vt, replies, sizeof(replies));
        size_t index;
        fputs("replies:", stdout);
        for (index = 0; index < count; ++index) {
            printf(" %02x", replies[index]);
        }
        fputc('\n', stdout);
    }

    printf("scrollback: offset=%u history=%u total=%u\n", shitty_vt_scroll_offset(vt), shitty_vt_history_rows(vt), shitty_vt_total_rows(vt));

    {
        shitty_vt_memory memory;
        shitty_vt_memory_usage(vt, &memory);
        printf("memory: allocated_rows=%u capacity_rows=%u columns=%u cell_size=%u cell_bytes=%llu\n", memory.allocated_rows, memory.capacity_rows, memory.columns, memory.cell_size, (unsigned long long)memory.cell_bytes);
    }

    if (dump_rows) {
        const uint32_t total = shitty_vt_total_rows(vt);
        uint32_t index;
        struct grid one;
        one.columns = columns;
        one.rows = 1;
        one.text = calloc(columns, CELL_TEXT_CAP);
        if (one.text == NULL) {
            shitty_vt_free(vt);
            return 1;
        }
        for (index = 0; index < total; ++index) {
            uint16_t column;
            for (column = 0; column < columns; ++column) {
                one.text[column * CELL_TEXT_CAP] = ' ';
                one.text[column * CELL_TEXT_CAP + 1] = '\0';
            }
            shitty_vt_row_cells(vt, index, collect_row, &one);
            printf("row %u:", index);
            for (column = 0; column < columns; ++column) {
                fputs(one.text + column * CELL_TEXT_CAP, stdout);
            }
            fputc('\n', stdout);
        }
        free(one.text);
    }

    shitty_vt_free(vt);
    return 0;
}
