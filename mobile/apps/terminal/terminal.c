/*
 * Terminal App - VT100 Emulator
 * uOS(m) - User OS Mobile
 */

#include "terminal.h"
#include "../../ui/mobile_ui.h"
#include "../../ui/framebuffer.h"
#include "../../ui/ui_widget.h"
#include "../../ui/window_manager.h"
#include <string.h>
#include <stdio.h>

static terminal_t g_term = {0};
static const uint32_t color_map[] = {
    0xFF000000, 0xFFAA0000, 0xFF00AA00, 0xFFAA5500,
    0xFF0000AA, 0xFFAA00AA, 0xFF00AAAA, 0xFFAAAAAA
};

static void render_glyph(char c, int x, int y, uint32_t fg, uint32_t bg) {
    int font_w = 8, font_h = 16;
    int px, py, bit;
    uint32_t pixel;
    
    for (py = 0; py < font_h; py++) {
        for (px = 0; px < font_w; px++) {
            bit = (c >= 32 && c < 128) ? ((c * font_w + py) % 2) : 0;
            pixel = bit ? fg : bg;
            fb_set_pixel(x + px, y + py, pixel);
        }
    }
}

static void render_text(int row, int col, const char *text, uint32_t fg, uint32_t bg) {
    int x = 20 + col * 8;
    int y = 120 + row * 16;
    int i;
    
    for (i = 0; text[i]; i++) {
        render_glyph(text[i], x + i * 8, y, fg, bg);
    }
}

int terminal_init(void) {
    memset(&g_term, 0, sizeof(g_term));
    g_term.fg_color = COLOR_WHITE;
    g_term.bg_color = COLOR_BLACK;
    g_term.scroll_top = 0;
    g_term.scroll_bottom = TERM_ROWS;
    terminal_clear_screen();
    
    wm_init();
    ui_widget_init();
    return 0;
}

void terminal_deinit(void) {}

void terminal_clear_screen(void) {
    int r, c;
    for (r = 0; r < TERM_ROWS; r++) {
        for (c = 0; c < TERM_COLS; c++) {
            g_term.cells[r][c].fg_color = g_term.fg_color;
            g_term.cells[r][c].bg_color = g_term.bg_color;
            g_term.cells[r][c].text[0] = ' ';
            g_term.cells[r][c].text[1] = '\0';
        }
    }
    g_term.cursor_x = 0;
    g_term.cursor_y = 0;
}

void terminal_scroll(void) {
    int r, c;
    if (g_term.scrollback_count < SCROLLBACK_SIZE) {
        strncpy(g_term.scrollback[g_term.scrollback_count], 
                g_term.cells[g_term.cursor_y].text, TERM_COLS);
        g_term.scrollback[g_term.scrollback_count][TERM_COLS] = '\0';
        g_term.scrollback_count++;
    }
    memmove(&g_term.cells[0], &g_term.cells[1], 
            (TERM_ROWS - 1) * sizeof(term_cell_t) * TERM_COLS);
}

void terminal_put_char(char ch) {
    if (ch == '\n') {
        g_term.cursor_x = 0;
        g_term.cursor_y++;
        if (g_term.cursor_y >= g_term.scroll_bottom) {
            terminal_scroll();
            g_term.cursor_y = g_term.scroll_bottom - 1;
        }
        return;
    }
    if (ch == '\r') {
        g_term.cursor_x = 0;
        return;
    }
    if (ch == '\b') {
        if (g_term.cursor_x > 0) g_term.cursor_x--;
        return;
    }
    
    if (g_term.cursor_x >= TERM_COLS) {
        g_term.cursor_x = 0;
        g_term.cursor_y++;
    }
    
    if (g_term.cursor_y >= g_term.scroll_bottom) {
        terminal_scroll();
        g_term.cursor_y = g_term.scroll_bottom - 1;
    }
    
    if (ch >= 32 && ch < 128) {
        term_cell_t *cell = &g_term.cells[g_term.cursor_y][g_term.cursor_x];
        cell->text[0] = ch;
        cell->text[1] = '\0';
        cell->fg_color = g_term.fg_color;
        cell->bg_color = g_term.bg_color;
        g_term.cursor_x++;
    }
}

void terminal_process_vt100(const char *seq) {
    if (strlen(seq) == 1 && seq[0] == 'H') {
        g_term.cursor_x = 0;
        g_term.cursor_y = 0;
    } else if (strlen(seq) >= 1 && seq[0] == 'J') {
        terminal_clear_screen();
    } else if (strlen(seq) >= 2 && seq[0] == '[') {
        if (seq[1] == 'K') {
            terminal_clear_line();
        } else if (seq[1] >= '0' && seq[1] <= '9' && strlen(seq) >= 3 && seq[2] == ';') {
            int row = seq[1] - '0';
            if (row >= 0 && row < TERM_ROWS) g_term.cursor_y = row;
        } else if (seq[1] >= '0' && seq[1] <= '9') {
            int col = seq[1] - '0';
            if (col >= 0 && col < TERM_COLS) g_term.cursor_x = col;
        }
    }
}

void terminal_clear_line(void) {
    int c;
    for (c = 0; c < TERM_COLS; c++) {
        g_term.cells[g_term.cursor_y][c].text[0] = ' ';
        g_term.cells[g_term.cursor_y][c].text[1] = '\0';
    }
}

void terminal_render(void) {
    int r, c;
    
    fb_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, 0xFF000000);
    fb_fill_rect(0, 0, FB_WIDTH, 80, 0xFF202020);
    fb_draw_text(20, 30, "Terminal", 0xFF00FF00, COLOR_TRANSPARENT);
    
    for (r = 0; r < TERM_ROWS; r++) {
        for (c = 0; c < TERM_COLS; c++) {
            uint32_t fg = color_map[g_term.cells[r][c].fg_color & 7];
            uint32_t bg = color_map[g_term.cells[r][c].bg_color & 7];
            render_text(r, c, g_term.cells[r][c].text, fg, bg);
        }
    }
    
    fb_fill_rect(20 + g_term.cursor_x * 8, 120 + g_term.cursor_y * 16, 
                8, 16, 0xFF00FF00);
}

void terminal_handle_key(int keycode, int pressed) {
    char ch;
    if (!pressed) return;
    
    if (keycode >= 32 && keycode < 127) {
        terminal_put_char(keycode);
    } else if (keycode == '\n') {
        terminal_put_char('\n');
    } else if (keycode == '\b') {
        terminal_put_char('\b');
    }
}

void terminal_input(char ch) {
    terminal_put_char(ch);
}

int main(void) {
    if (terminal_init() != 0) return 1;
    
    mobile_ui_init();
    window_t *win = wm_create_window("Terminal", 0, 0, FB_WIDTH, FB_HEIGHT);
    
    terminal_input('u');
    terminal_input('O');
    terminal_input('S');
    terminal_input(' ');
    terminal_input('(');
    terminal_input('m');
    terminal_input(')');
    terminal_input('\n');
    
    while (1) {
        terminal_render();
        fb_flush();
        mobile_ui_event_loop();
    }
    
    terminal_deinit();
    return 0;
}