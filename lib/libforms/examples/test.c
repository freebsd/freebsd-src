/*
 * Copyright (c) 1995 Paul Richards. 
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 */

#include <stdio.h>
#include "../forms.h"

main()
{

	int res;

	char *string0 = "A simple demo of the current forms widgets";
	char *string1 = "Hey, I can flash boldly";
	char *string2 = "This is an input field with default:";
	char *string4 = "This is a labelled input field:";
	char *string6 = "Some options to choose from: ";

	char *options7[] = {"Choose", "one", "of", "these"};

	struct text_field field0   = {string0};
	struct text_field field1   = {string1};
	struct text_field field2   = {string2};
	struct input_field field3  = {0,"Default entry",0};
	struct text_field field4   = {string4};
	struct input_field field5  = {1,"A place filler",0};
	struct text_field field6   = {string6};
	struct menu_field field7   = {sizeof &options7, 0, options7};
	struct action_field field8 = {"EXIT",&exit_form};
	struct action_field field9 = {"CANCEL",&cancel_form};

	struct field field[] = {
		{F_TEXT, 0, 15, 80, 80, A_BOLD, 
			0, 0, 0, 0, 0, (struct text_field *)&field0},
		{F_TEXT, 3, 23, 25, 15, A_BLINK|A_BOLD,
			0, 0, 0, 0, 0, &field1},
		{F_TEXT, 7, 2, 40, 40, F_DEFATTR,
			0, 0, 0, 0, 0, &field2},
		{F_INPUT, 7, 45, 15, 30, F_DEFATTR,
			5, -1, 5, -1, -1, (struct text_field *)&field3},
		{F_TEXT, 11, 2, 40, 40, F_DEFATTR,
			0, 0, 0, 0, 0, &field4},
		{F_INPUT, 11, 45, 15, 30, F_DEFATTR,
			7, 3, 7, -1, -1, (struct text_field *)&field5},
		{F_TEXT, 15, 2, 40, 40, F_DEFATTR,
			0, 0, 0, 0, 0, (struct text_field *)&field6},
		{F_MENU, 15, 45, 10, 10, F_DEFATTR,
			8, 5, 8, -1, -1, (struct text_field *)&field7},
		{F_ACTION, 20, 20, 6, 6, (A_BOLD|A_REVERSE),
			0, 7, -1, -1, 9, (struct text_field *)&field8},
		{F_ACTION, 20, 43, 6, 6, (A_BOLD|A_REVERSE),
			3, 3, 3, 7, 3, (struct text_field *)&field9},
		{F_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	};

	struct form form = {0, 3, field, 24, 80, 0, 0, 0};
	form.field = field;

	initscr();

	initfrm(&form);
	if (!form.window) {
		fprintf(stderr, "\nUnable to initialize forms library.\n");
		endwin();
		exit(1);
	}
	keypad(form.window, TRUE);
	while (!(res = update_form(&form)));


	wclear(form.window);
	wrefresh(form.window);

	if  (res == F_DONE) {
		printf("You're entries were:\n\n");
		printf("%s\n",field[3].field.input->input);
		printf("%s\n",field[5].field.input->input);
		printf("%s\n",field[7].field.menu->options[field[7].field.menu->selected]);
	} else if (res == F_CANCEL)
		printf("You cancelled the form\n");

	endfrm(&form);
	endwin();
}
