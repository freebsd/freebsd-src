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

static char *welcome[] = {
    "View 'READ ME FIRST' File.",
    "View FreeBSD Copyright Information.",
    "Proceed with installation.",
    "Repair existing installation ('fixit' mode).",
    "Exit to shell.",
};

void 
stage0()
{
    int valid = 0;

	if (!access(README_FILE, R_OK)) {
	    dialog_clear();
	    dialog_textbox("READ ME FIRST", README_FILE, 24, 80);
	}
    return;

    do {
	if (!dialog_menu("Welcome to FreeBSD!",
			 "Please select one of the following options.\n",
			 10, 75, 5, 5, welcome, selection))
	    valid = 1;
	dialog_clear();
    } while (!valid);
    switch (atoi(selection)) {
    case 1:	/* View readme */
	if (!access(README_FILE, R_OK)) {
	    dialog_clear();
	    dialog_textbox("READ ME FIRST", README_FILE, 24, 80);
	}
	break;

    case 2:	/* View copyrights */
	if (!access(COPYRIGHT_FILE, R_OK)) {
	    dialog_clear();
	    dialog_textbox("COPYRIGHT", COPYRIGHT_FILE, 24, 80);
	}
	break;

    case 3:	/* Proceed (do nothing special, really) */
	break;

    case 4:
	dialog_msgbox("Sorry!", "This feature not currently implemented.",
		      6, 75, 1);
	break;

    case 5:
	exit(0);
	break;	/* hope not! :) */
    }
}
