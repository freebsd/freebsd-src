#include <string.h>
#include <ncurses.h>
#include <dialog.h>

#include "editor.h"

int
disp_fields(WINDOW *window, struct field field[], int no_fields)
{
	int i, j;
	int len;

	for (i=0; i < no_fields; i++) {
		wmove(window, field[i].y, field[i].x);
		switch (field[i].type) {
			case F_TITLE:
			case F_EDIT:
				if (field[i].type == F_TITLE)
					wattrset(window, title_attr);
				else
					wattrset(window, dialog_attr);
				len=strlen(field[i].field);
				for (j=0; j < field[i].width; j++)
					if (j < len)
						waddch(window, field[i].field[j]);
					else
						waddch(window, ' ');
				break;
			case F_BUTTON:
				print_button(window, field[i].field,
											field[i].y,
											field[i].x,
											FALSE);
				break;
		}
	}
	wrefresh(window);
	return (0);
}

int
change_field(struct field field, int key)
{
	int next;

	switch(key) {
		case KEY_UP:
			next = field.up;
			break;
		case KEY_DOWN:
			next = field.down;
			break;
		case '\t':
			next = field.right;
			break;
		case KEY_BTAB:
			next = field.left;
			break;
		case '\n':
		case '\r':
			next = field.next;
			break;
		default:
			next = -1;
			break;
	}
	return (next);
}
			
int
edit_line(WINDOW *window, int y, int x, char *field, int width, int maxlen)
{
	int len;
	int key = 0;
	int fpos, dispos, curpos;
	int i;
	int done = 0;

	len = strlen(field);
	if (len < width) {
		fpos = len;
		curpos = len;
		dispos = 0;
	} else {
		fpos = width;
		curpos = width;
		dispos = len - width;
	};


	do {
		wattrset(window, item_selected_attr);
		wmove(window, y, x);
		for (i=0; i < width; i++)
			if (i < (len - dispos))
				waddch(window, field[dispos+i]);
			else
				waddch(window, ' ');
		wmove(window, y, x + curpos);
		wrefresh(window);

		key = wgetch(window);
		switch (key) {
			case TAB:
			case KEY_BTAB:
			case KEY_UP:
			case KEY_DOWN:
			case ESC:
			case '\n':
			case '\r':
			case ' ':
				done = 1;
				break;
			case KEY_HOME:
				if (len < width) {
					fpos = len;
					curpos = len;
					dispos = 0;
				} else {
					fpos = width;
					curpos = width;
					dispos = len - width;
				};
				break;
			case KEY_END:
				if (len < width) {
					dispos = 0;
					curpos = len - 1;
				} else {
					dispos = len - width - 1;
					curpos = width - 1;
				}
				fpos = len - 1;
				break;
			case KEY_LEFT:
				if ((!curpos) && (!dispos)) {
					beep();
					break;
				}
				if (--curpos < 0) {
					curpos = 0;
					if (--dispos < 0)
						dispos = 0;
				}
				if (--fpos < 0)
					fpos = 0;
				break;
			case KEY_RIGHT:
				if ((curpos + dispos) == len) {
					beep();
					break;
				}
				if ((curpos == (width-1)) && (dispos == (maxlen - width -1))) {
					beep();
					break;
				}
				if (++curpos >= width) {
					curpos = width - 1;
					dispos++;
				}
				if (dispos >= len)
					dispos = len - 1;
				if (++fpos >= len) {
					fpos = len;
				}
				break;
			case KEY_BACKSPACE:
			case KEY_DC:
				if ((!curpos) && (!dispos)) {
					beep();
					break;
				}
				if (fpos > 0) {
					memmove(field+fpos-1, field+fpos, len - fpos);
					len--;
					fpos--;
					if (curpos > 0)
						--curpos;
					if (!curpos)
						--dispos;
					if (dispos < 0)
						dispos = 0;
				} else
					beep();
				break;
			default:
				if (len < maxlen - 1) {
					memmove(field+fpos+1, field+fpos, len - fpos);
					field[fpos] = key;
					len++;
					fpos++;
					if (++curpos == width) {
						--curpos;
						dispos++;
					}
					if (len == (maxlen - 1)) {
						dispos = (maxlen - width - 1);
					}
				} else
					beep();
				break;
		}
	} while (!done);
	wattrset(window, dialog_attr);
	wmove(window, y, x);
	for (i=0; i < width; i++)
		if (i < (len - dispos))
			waddch(window, field[dispos+i]);
		else
			waddch(window, ' ');
	wmove(window, y, x + curpos);
	wstandend(window);
	field[len] = 0;
	wrefresh(window);
	return (key);
}
