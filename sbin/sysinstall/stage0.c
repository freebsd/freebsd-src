/*
 * Copyright (c) 1994, Jordan Hubbard, Paul Richards and Poul-Henning Kamp.
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
 *
 * [Note: This file bears almost no resemblance to what was here in an
 * earlier incarnation].
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dialog.h>

#include "sysinstall.h"

static unsigned char *welcome[] = {
    "1. README",
    "View `READ ME FIRST' File.",
    "2. COPYRIGHT",
    "View FreeBSD Copyright Information.",
    "3. Proceed",
    "Proceed with installation.",
    "4. Fixit",
    "Repair existing installation (`fixit' mode).",
    "5. Exit",
    "Exit to shell.",
};

void bailout(void);

void 
stage0()
{
evil_goto:
    if (dialog_menu("Welcome to FreeBSD!",
		    "Please select one of the following options:",
		    15, 75, 6, 5, welcome, selection)) {
	bailout();
    }
    switch (atoi(selection)) {
    case 1:	/* View readme */
	if (!access(README_FILE, R_OK)) {
	    dialog_clear();
	    dialog_textbox("READ ME FIRST", README_FILE, 24, 80);
	    dialog_clear();
	}
	goto evil_goto;
	break;

    case 2:	/* View copyrights */
	if (!access(COPYRIGHT_FILE, R_OK)) {
	    dialog_clear();
	    dialog_textbox("COPYRIGHT", COPYRIGHT_FILE, 24, 80);
	    dialog_clear();
	}
	goto evil_goto;
	break;

    case 3:	/* Proceed (do nothing special, really) */
	break;

    case 4:
	dialog_msgbox("Sorry!", "This feature not currently implemented.",
		      6, 75, 1);
	goto evil_goto;
	break;

    case 5:
	bailout();
	break;	/* hope not! :) */
    }
}

void
bailout()
{
    dialog_clear();
    end_dialog();
    exit(0);
}
