#include <string.h>
#include <ncurses.h>
#include <forms.h>

extern FILE *yyin;

struct form *form;
unsigned int keymap[FORM_NO_KEYS] = {
	KEY_BTAB,
	9,
	KEY_UP,
	KEY_DOWN,
	'\r',
	'\033',
	KEY_HOME,
	KEY_END,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_BACKSPACE,
	KEY_DC
};

int
edit_field(WINDOW *window, struct field *field)
{
	int len;
	int key = 0;
	int fpos, dispos, curpos;
	int i;
	int done = 0;

	len = strlen(field->entry.input.field);
	if (len < field->entry.input.field_width) {
		fpos = len;
		curpos = len;
		dispos = 0;
	} else {
		fpos = field->entry.input.field_width;
		curpos = field->entry.input.field_width;
		dispos = len - field->entry.input.field_width;
	};

	field->entry.input.field_attr = FORM_SELECTED_ATTR;
	do {
		wattrset(window, field->entry.input.field_attr);
		wmove(window, field->entry.input.y_field, field->entry.input.x_field);
		for (i=0; i < field->entry.input.field_width; i++)
			if (i < (len - dispos))
				waddch(window, field->entry.input.field[dispos+i]);
			else
				waddch(window, ' ');
		wmove(window, field->entry.input.y_field, field->entry.input.x_field + curpos);
		wrefresh(window);

		key = wgetch(window);
		if (key == keymap[FORM_LEFT] ||
			 key == keymap[FORM_RIGHT] ||
			 key == keymap[FORM_UP] ||
			 key == keymap[FORM_DOWN] ||
			 key == keymap[FORM_EXIT] ||
			 key == '\n' ||
			 key == '\r') {
				done = 1;
		} else if (key == keymap[FORM_FIELD_HOME]) {
			if (len < field->entry.input.field_width) {
				fpos = len;
				curpos = len;
				dispos = 0;
			} else {
				fpos = field->entry.input.field_width;
				curpos = field->entry.input.field_width;
				dispos = len - field->entry.input.field_width;
			};
		} else if (key == keymap[FORM_FIELD_END]) {
			if (len < field->entry.input.field_width) {
				dispos = 0;
				curpos = len - 1;
			} else {
				dispos = len - field->entry.input.field_width - 1;
				curpos = field->entry.input.field_width - 1;
			}
			fpos = len - 1;
		} else if (key == keymap[FORM_FIELD_LEFT]) {
			if ((!curpos) && (!dispos)) {
				beep();
			} else {
				if (--curpos < 0) {
					curpos = 0;
					if (--dispos < 0)
						dispos = 0;
				}
				if (--fpos < 0)
					fpos = 0;
			}
		} else if (key == keymap[FORM_FIELD_RIGHT]) {
			if ((curpos + dispos) == len) {
				beep();
			} else if ((curpos == (field->entry.input.field_width-1)) &&
				(dispos == (field->entry.input.max_field_width - field->entry.input.field_width -1))) {
					beep();
			} else {
				if (++curpos >= field->entry.input.field_width) {
					curpos = field->entry.input.field_width - 1;
					dispos++;
				}
				if (dispos >= len)
					dispos = len - 1;
				if (++fpos >= len) {
					fpos = len;
				}
			}
		} else if (key == keymap[FORM_FIELD_BACKSPACE]) {
			if ((!curpos) && (!dispos)) {
				beep();
			} else if (fpos > 0) {
				memmove(&field->entry.input.field[fpos-1], &field->entry.input.field[fpos], len - fpos);
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
		} else {
			if (len < field->entry.input.max_field_width - 1) {
				memmove(&field->entry.input.field[fpos+1], &field->entry.input.field[fpos], len - fpos);
				field->entry.input.field[fpos] = key;
				len++;
				fpos++;
				if (++curpos == field->entry.input.field_width) {
					--curpos;
					dispos++;
				}
				if (len == (field->entry.input.max_field_width - 1)) {
					dispos = (field->entry.input.max_field_width - field->entry.input.field_width - 1);
				}
			} else
				beep();
		}
	} while (!done);

	field->entry.input.field_attr = FORM_DEFAULT_ATTR;
	wattrset(window, field->entry.input.field_attr);
	wmove(window, field->entry.input.y_field, field->entry.input.x_field);
	for (i=0; i < field->entry.input.field_width; i++)
		if (i < (len - dispos))
			waddch(window, field->entry.input.field[dispos+i]);
		else
			waddch(window, ' ');
	wmove(window, field->entry.input.y_field, field->entry.input.x_field + curpos);
	wrefresh(window);

	field->entry.input.field[len] = 0;
	delwin(window);
	refresh();
	return (key);
}

int
init_forms(char *template)
{
	FILE *fd;
	struct field *link, *next;

	/* Intialise lex input */
	if (!(fd = fopen(template, "r"))) {
		fprintf(stderr, "Couldn't open template file %s\n", template);
		return(-1);
	}

	if (!initscr()) {
		fprintf(stderr, "Failed to initialise curses\n");
		return(-1);
	}

	cbreak();
	noecho();
	nonl();

	yyin = fd;
	yyparse();

	/* Setup up links to/from fields */

	for (next = form->fields; next; next = next->link) {
		/* Ignore the link values of text fields */
		if (next->type == FORM_FTYPE_TEXT)
			continue;
		link = find_link((int)next->next); 
		if (!link) {
			fprintf(stderr, "Bad link (next) from %d to %d\n",
					 next->field_id, (int)next->next);
			next->next = 0;
		} else
			next->next = link;
		link = find_link((int)next->up); 
		if (!link) {
			fprintf(stderr, "Bad link (up) from %d to %d\n",
					 next->field_id, (int)next->up);
			next->up = 0;
		} else
			next->up = link;
		link = find_link((int)next->down); 
		if (!link) {
			fprintf(stderr, "Bad link (down) from %d to %d\n",
					  next->field_id, (int)next->down);
			next->down = 0;
		} else
			next->down = link;
		link = find_link((int)next->left); 
		if (!link) {
			fprintf(stderr, "Bad link (left) from %d to %d\n",
					 next->field_id, (int)next->left);
			next->left = 0;
		} else
			next->left = link;
		link = find_link((int)next->right); 
		if (!link) {
			fprintf(stderr, "Bad link (right) from %d to %d\n",
					 next->field_id, (int)next->right);
			next->right = 0;
		} else
			next->right = link;
	}
}

struct field *
find_link(int id)
{
	struct field *next;

	for (next=form->fields; next; next=next->link)
		/* You can't move into a text field */
		if ((id == next->field_id) && (next->type != FORM_FTYPE_TEXT))
			return (next);
	return(0);
}

void
edit_form(struct form *form)
{
	WINDOW *window;
	struct field *cur_field;
	int key;

	window = newwin(form->height, form->width, form->y, form->x);
	keypad(window, TRUE);

	refresh_form(window, form);

	cur_field = form->fields;

	do {
		/* Just skip over text fields */
		if (cur_field->type == FORM_FTYPE_TEXT) {
			cur_field = cur_field->link;
			continue;
		}
		switch (cur_field->type) {
			case FORM_FTYPE_INPUT:
				key = edit_field(window, cur_field);
				break;
			case FORM_FTYPE_MENU:
			case FORM_FTYPE_BUTTON:
			case FORM_FTYPE_TEXT: /* Should never happen */
			default:
				break;
		}
		if (key == keymap[FORM_UP]) {
			if (cur_field->up)
				cur_field = cur_field->up;
			else
				beep();
		} else if (key == keymap[FORM_DOWN]) {
			if (cur_field->down)
				cur_field = cur_field->down;
			else
				beep();
		} else if (key == keymap[FORM_LEFT]) {
			if (cur_field->left)
				cur_field = cur_field->left;
			else
				beep();
		} else if (key == keymap[FORM_RIGHT]) {
			if (cur_field->right)
				cur_field = cur_field->right;
			else
				beep();
		} else if (key == keymap[FORM_NEXT]) {
			if (cur_field->next)
				cur_field = cur_field->next;
			else
				cur_field = form->fields;
		} else
			beep();
	} while (key != keymap[FORM_EXIT]);
}

void
refresh_form(WINDOW *window, struct form *form)
{
	struct field *cur_field;

	cur_field = form->fields;

	while (cur_field) {
		switch (cur_field->type) {
			case FORM_FTYPE_INPUT:
				wattrset(window, cur_field->entry.input.prompt_attr);
				mvwprintw(window, cur_field->entry.input.y_prompt,
				          cur_field->entry.input.x_prompt,
					       "%s", cur_field->entry.input.prompt);
				wattrset(window, cur_field->entry.input.field_attr);
				mvwprintw(window, cur_field->entry.input.y_field,
							 cur_field->entry.input.x_field,
					       "%s", cur_field->entry.input.field);
				break;
			case FORM_FTYPE_TEXT:
				wattrset(window, cur_field->entry.text.attr);
				mvwprintw(window, cur_field->entry.text.y,
							cur_field->entry.text.x,
							"%s", cur_field->entry.text.text);
				break;
			default:
				break;
		}
		cur_field = cur_field->link;
	}
	wrefresh(window);
}
