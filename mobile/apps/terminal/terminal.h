/*
 * Terminal App - VT100 Emulator
 * uOS(m) - User OS Mobile
 */

#ifndef _TERMINAL_H_
#define _TERMINAL_H_

#include <stdint.h>

#define TERM_COLS 80
#define TERM_ROWS 24
#define SCROLLBACK_SIZE 1000

typedef enum {
    COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
    COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
} term_color_t;

typedef struct {
    char text[TERM_COLS + 1];
    uint8_t fg_color;
    uint8_t bg_color;
    uint8_t bold : 1;
    uint8_t underline : 1;
} term_cell_t;

typedef struct {
    term_cell_t cells[TERM_ROWS][TERM_COLS];
    char scrollback[SCROLLBACK_SIZE][TERM_COLS + 1];
    int scrollback_count;
    int cursor_x, cursor_y;
    int scroll_top, scroll_bottom;
    term_color_t fg_color;
    term_color_t bg_color;
    int escaped;
    char escape_seq[32];
    int escape_pos;
} terminal_t;

int terminal_init(void);
void terminal_deinit(void);
void terminal_render(void);
void terminal_handle_key(int keycode, int pressed);
void terminal_input(char ch);
void terminal_process_vt100(const char *seq);
void terminal_put_char(char ch);
void terminal_clear_line(void);
void terminal_clear_screen(void);
void terminal_scroll(void);

#endif /* _TERMINAL_H_ */