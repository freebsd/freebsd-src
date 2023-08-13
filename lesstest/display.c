#include <termcap.h>
#include "lesstest.h"

extern TermInfo terminfo;

// Set the user's terminal to a given attribute and colors.
static void display_attr_color(Attr attr, Color fg_color, Color bg_color) {
	printf("\33[m");
	if (fg_color != NULL_COLOR)
		printf("\33[%dm", fg_color);
	if (bg_color != NULL_COLOR)
		printf("\33[%dm", bg_color);
	if (attr & ATTR_UNDERLINE)
		printf("%s", terminfo.enter_underline);
	if (attr & ATTR_BOLD)
		printf("%s", terminfo.enter_bold);
	if (attr & ATTR_BLINK)
		printf("%s", terminfo.enter_blink);
	if (attr & ATTR_STANDOUT)
		printf("%s", terminfo.enter_standout);
}

static int hexval(unsigned char ch) {
	if (ch >= '0' && ch <= '9') return ch - '0';
	if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
	if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
	fprintf(stderr, "invalid hex char 0x%x\n", ch);
	abort();
}

static int get_hex(unsigned char const** pp) {
	int v1 = hexval(*(*pp)++);
	int v2 = hexval(*(*pp)++);
	return (v1 << 4) | v2;
}

// Display a given screen image on the user's terminal.
void display_screen(const byte* img, int imglen, int screen_width, int screen_height) {
	int x = 0;
	int y = 0;
	int cursor_x = 0;
	int cursor_y = 0;
	int literal = 0;
	Attr curr_attr = 0;
	Color curr_fg_color = NULL_COLOR;
	Color curr_bg_color = NULL_COLOR;
	while (imglen-- > 0) {
		wchar ch = load_wchar(&img);
		if (!literal) {
			switch (ch) {
			case '\\':
				literal = 1;
				continue;
			case LTS_CHAR_ATTR:
				curr_attr = get_hex(&img);
				display_attr_color(curr_attr, curr_fg_color, curr_bg_color);
				continue;
			case LTS_CHAR_FG_COLOR:
				curr_fg_color = get_hex(&img);
				display_attr_color(curr_attr, curr_fg_color, curr_bg_color);
				continue;
			case LTS_CHAR_BG_COLOR:
				curr_bg_color = get_hex(&img);
				display_attr_color(curr_attr, curr_fg_color, curr_bg_color);
				continue;
			case LTS_CHAR_CURSOR:
				cursor_x = x;
				cursor_y = y;
				continue;
			}
		}
		literal = 0;
		if (ch != 0) {
			byte cbuf[UNICODE_MAX_BYTES];
			byte* cp = cbuf;
			store_wchar(&cp, ch);
			fwrite(cbuf, 1, cp-cbuf, stdout);
		}
		if (++x >= screen_width) {
			printf("\n");
			x = 0;
			if (++y >= screen_height)
				break;
		}
	}
	printf("%s", tgoto(terminfo.cursor_move, cursor_x, cursor_y));
	fflush(stdout);
}

// Print a given screen image on stderr.
// Unlike display_screen which prints escape sequences to change color etc,
// display_screen_debug only prints printable ASCII.
void display_screen_debug(const byte* img, int imglen, int screen_width, int screen_height) {
	int x = 0;
	int y = 0;
	int literal = 0;
	while (imglen-- > 0) {
		wchar ch = load_wchar(&img);
		if (!literal) {
			switch (ch) {
			case '\\':
				literal = 1;
				continue;
			case LTS_CHAR_ATTR:
			case LTS_CHAR_FG_COLOR:
			case LTS_CHAR_BG_COLOR:
				x -= 3; // don't count LTS_CHAR or following 2 bytes
				break;
			case LTS_CHAR_CURSOR:
				x -= 1; // don't count LTS_CHAR
				break;
			}
		}
		literal = 0;
		if (is_ascii(ch))
			fwrite(&ch, 1, 1, stderr);
		else
			fprintf(stderr, "<%lx>", (unsigned long) ch);
		if (++x >= screen_width) {
			fprintf(stderr, "\n");
			x = 0;
			if (++y >= screen_height)
				break;
		}
	}
	fflush(stderr);
}

// Print a list of strings.
void print_strings(const char* title, char* const* strings) {
	if (1) return; ///
	fprintf(stderr, "%s:\n", title);
	char* const* s;
	for (s = strings; *s != NULL; ++s) {
		fprintf(stderr, " ");
		const char* p;
		for (p = *s; *p != '\0'; ++p) {
			if (is_ascii(*p))
				fprintf(stderr, "%c", (char) *p);
			else
				fprintf(stderr, "\\x%04x", *p);
		}
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "%s- end\n", title);
}
