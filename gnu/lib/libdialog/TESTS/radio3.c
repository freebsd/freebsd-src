/*
 * small test-driver for new dialog functionality
 *
 * Copyright (c) 1995, Jordan Hubbard
 *
 * All rights reserved.
 *
 * This source code may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of the software nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 *
 * $Id: radio3.c,v 1.3 1996/04/16 12:17:27 jkh Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dialog.h>

/* Hook functions */

static int spending;

static int
check(dialogMenuItem *self)
{
    return ((int)self->data == spending);
}

static int
spend(dialogMenuItem *self)
{
    spending = (int)self->data;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static void
ask(dialogMenuItem *self, int is_selected)
{
    if (is_selected) {
	char *str;
	
	if (!strcmp(self->prompt, "1000"))
	    str = "You'd better ask both your parents first! ";
	else if (!strcmp(self->prompt, "500"))
	    str = "You'd better at least ask your Dad!       ";
	else
	    str = "Yes, being frugal is probably a good idea!";
	DialogX = 15;
	DialogY = 17;
	dialog_msgbox("Free Advice", str, -1, -1, 0);
    }
}

/*
 * menu5 - Show a simple radiolist menu that inherits the radio appearance by default and appears at
 * a different location, leaving room for a msg box below it.  This shows off the DialogX/DialogY extensions.
 */

/* prompt	title			checked		fire		sel	data */
static dialogMenuItem menu5[] = {
    { "1000",	"Spend $1,000",		check,		spend,		ask,	(void *)1000 },
    { "500",	"Spend $500",		check,		spend,		ask,	(void *)500 },
    { "100",	"Spend $100",		check,		spend, 		ask,	(void *)100 },
};

/* End of hook functions */

/* Kick it off, James! */
int
main(int argc, unsigned char *argv[])
{
    int retval;
    
    init_dialog();
    
    
    DialogX = 5;
    DialogY = 1;
    retval = dialog_radiolist("this is dialog_radiolist() in action, test #3",
			      "This radio menu shows off the ability to put dialog menus and other\n"
			      "controls at different locations, as well as the `selected' hook which\n"
			      "lets you follow the traversal of the selection bar as well as what's\n"
			      "selected.",
			      -1, -1, 3, -3, menu5, NULL);
    dialog_clear();
    fprintf(stderr, "returned value for dialog_radiolist was %d (money set to %d)\n", retval, spending);
    
    end_dialog();
    return 0;
}
