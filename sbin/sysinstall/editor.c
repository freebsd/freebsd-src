#include <string.h>
#include <ncurses.h>
#include <dialog.h>

#include "editor.h"

int
disp_fields(WINDOW *window, struct field field[], int no_fields)
{
	int i, j;
	int len;

	wattrset(window, dialog_attr);
	for (i=0; i < no_fields; i++) {
		len=strlen(field[i].field);
		wmove(window, field[i].y, field[i].x);
		for (j=0; j < field[i].width; j++)
			if (j < len)
				waddch(window, field[i].field[j]);
			else
				waddch(window, ' ');
	}
	wrefresh(window);
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
