/* Stopgap, until Paul does the right thing */
#define ESC 27
#define TAB 9

#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <string.h>

#include <string.h>
#include <dialog.h>
#include "sysinstall.h"

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

int
AskEm(WINDOW *w,char *prompt, char *answer, int len)
{
    int x,y;
    mvwprintw(w,23,0,prompt);
    getyx(w,y,x);
    addstr("                             ");
    return edit_line(w,y,x,answer,len,len+1);
}

void
ShowFile(char *filename, char *header)
{
    char buf[256];
    if (access(filename, R_OK)) {
	sprintf(buf, "The %s file is not provided on the 1.2MB floppy image.", filename);
	dialog_msgbox("Sorry!", buf, 6, 75, 1);
	return;
    }
    dialog_clear();
    dialog_textbox(header, filename, LINES, COLS);
    dialog_clear();
}
