#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <dialog.h>
#include <forms.h>

extern struct form *form;

void
main()
{
	printf("Testing forms code\n");

	if (init_forms("example.frm") == -1)
		exit(1);

	edit_form(form);
}
