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
#include <sys/reboot.h>
#include <dialog.h>

#include "sysinstall.h"

static unsigned char *welcome[] = {
    "1. README",
    "Read the `READ ME FIRST' File .",
    "2. Release Notes",
     "Read the 2.0 release notes (recommended).",
    "2. COPYRIGHT",
    "Read FreeBSD Copyright Information.",
    "3. Proceed",
    "Proceed with installation.",
    "4. Fixit",
    "Repair existing installation (`fixit' mode).",
    "5. Quit",
    "Don't do anything, just reboot.",
};

void 
stage0()
{
evil_goto:
    if (dialog_menu("Welcome to FreeBSD!",
		    "Please select one of the following options:",
		    15, 75, 6, 5, welcome, selection)) {
	dialog_clear();
	end_dialog();
	reboot(RB_AUTOBOOT);
    }
    switch (atoi(selection)) {
    case 1:	/* View the README */
        ShowFile(README_FILE, "Read Me First");
	goto evil_goto;
	break;

    case 2:	/* View the release notes */
        ShowFile(RELNOTES_FILE, "Release Notes");
	goto evil_goto;
	break;

    case 3:	/* View copyrights */
        ShowFile(COPYRIGHT_FILE, "COPYRIGHT");
	goto evil_goto;
	break;

    case 4:	/* Proceed (do nothing special, really) */
	break;

    case 5:
	dialog_msgbox("Sorry!", "This feature not currently implemented.",
		      6, 75, 1);
	goto evil_goto;
	break;

    case 6:
	/* Be neat.. */
	dialog_clear();
	end_dialog();
	reboot(RB_AUTOBOOT);
	break;	/* hope not! :) */
    }
}
