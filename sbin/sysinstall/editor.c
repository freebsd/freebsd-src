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
			next = field.right;
			break;
		default:
			next = -1;
			break;
	}
	return (next);
}

int
button_press(WINDOW *window, struct field field)
{
	int key;

	print_button(window, field.field,
								field.y,
								field.x,
								TRUE);
	key = wgetch(window);

	switch (key) {
		case '\n':
		case '\r':
			return (0);
		case KEY_UP:
		case KEY_DOWN:
		case KEY_BTAB:
		case '\t':
		default:
			return (key);
	}
}

int
toggle_press(WINDOW *window, struct field field)
{
	int key;

	key = wgetch(window);

	switch (key) {
		case ' ':
			field.spare++;
			if (!field.misc[field.spare])
				field.spare = 0;
			sprintf(field.field, "%", field.misc[field.spare]);
			return (key);
			break;
		case '\n':
		case '\r':
		case KEY_UP:
		case KEY_DOWN:
		case KEY_BTAB:
		case '\t':
		default:
			return (key);
	}
}
