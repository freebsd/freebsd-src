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
 * $Id: menu3.c,v 1.4 1996/04/18 13:21:26 jkh Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dialog.h>

/* Hook functions */

static int
stop(dialogMenuItem *self)
{
    dialog_mesgbox("!", "I'm no idiot!", -1, -1);
    return DITEM_SUCCESS;
}

static int
maybe(dialogMenuItem *self)
{
    dialog_mesgbox("!", "I said don't rush me!  I'm THINKING!", -1, -1);
    return DITEM_SUCCESS | DITEM_RESTORE | DITEM_CONTINUE;
}

/* Dummy menu just to show of the ability */
static char *insurance[] = {
    "1,000,000",	"Mondo insurance policy", "Off",
    "5,000,000",	"Mega insurance policy", "Off",
    "10,000,000",	"Friend!  Most Favored customer!"
};

static void
preinsure(dialogMenuItem *self, int is_selected)
{
    if (is_selected) {
	static WINDOW *w;
	
	/* This has to be here first if you want to see selection traverse properly in the invoking menu */
	refresh();

	w = dupwin(newscr);
	DialogX = 1;
	DialogY = 13;
	dialog_radiolist("How much insurance would you like to take out?",
			 "If you're really going to do this, we recommend some insurance\n"
			 "first!  What kind of life insurance policy would you like?",
			 -1, -1, 3, 3, insurance, NULL);
	touchwin(w);
	wrefresh(w);
	delwin(w);
    }
}

/*
 * Show a simple menu that puts up a sub menu when a certain item is traversed to
 */

/* prompt	title						checked		fire		sel  */
static dialogMenuItem doit[] = {
    { "Stop",	"No, I'm not going to do that!",		NULL,		stop,		NULL	},
    { "Maybe",	"I'm still thinking about it, don't rush me!",	NULL,		maybe,		NULL,	},
    { "Go",	"Yes!  Yes!  I want to do it!",			NULL,		NULL, 		preinsure },
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
    retval = dialog_menu("Do you have the GUTS?",
			 "C'mon, macho man!  Do you have what it takes to do something REALLY\n"
			 "dangerous and stupid?  WHAT ARE YOU WAITING FOR?!",
			 -1, -1, 3, -3, doit, NULL, NULL, NULL);
    dialog_clear();
    fprintf(stderr, "returned value for dialog_menu was %d\n", retval);
    
    end_dialog();
    return 0;
}
