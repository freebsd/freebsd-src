/*
 * Copyright (c) 1995
 *	Paul Richards.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer, 
 *    verbatim and that no modifications are made prior to this 
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Paul Richards.
 * 4. The name Paul Richards may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL RICHARDS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL RICHARDS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <stdio.h>
#include "../forms.h"

main()
{
	struct Tuple *tuple;
	struct Form *form;
	int res;

	initscr();

	form_bind_tuple("exit_form", FT_FUNC, &exit_form);
	form_bind_tuple("cancel_form", FT_FUNC, &cancel_form);

	if (form_load("example.frm") == FS_ERROR)
		exit(0);;

	form = form_start("example");

	if (!form) {
		err(-1, "No form returned");
		exit(0);
	}

	keypad(form->window, TRUE);
	cbreak();
	noecho();

	tuple = form_get_tuple("example", FT_FORM);
	if (!tuple)
		err(0, "No such form");
	else
		form = (struct Form *)tuple->addr;

	print_status("This is the status line");

	res = form_show("example");

	while (form->status == FS_RUNNING) {
		do_field(form);
		wrefresh(form->window);
	}

	wclear(form->window);
	wrefresh(form->window);

	if  (form->status == FS_EXIT) {
		printf("You're entries were:\n\n");
		tuple = form_get_tuple("input1", FT_FIELD_INST);
		printf("Input 1 = %s\n", ((struct Field *)tuple->addr)->field.input->input);
		tuple = form_get_tuple("input2", FT_FIELD_INST);
		printf("Input 2 = %s\n", ((struct Field *)tuple->addr)->field.input->input);
		tuple = form_get_tuple("menu1", FT_FIELD_INST);
		res = ((struct Field *)tuple->addr)->field.menu->selected;
		printf("Menu selected = %d, %s\n", res,
			 ((struct Field *)tuple->addr)->field.menu->options[res]);
	} else if (form->status == FS_CANCEL)
		printf("You cancelled the form\n");

	endwin();
}
