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
    "READ THIS FIRST.",
    "2. Release Notes",
    "Read the 2.0 Release Notes (recommended).",
    "3. Troubleshooting",
    "Read this in case of trouble.",
    "4. Partitions and MBRs",
    "Verbose description of how these work.",
    "5. COPYRIGHT",
    "Read FreeBSD Copyright Information.",
    "6. Install",
    "Proceed with full installation.",
    "7. Fixit",
    "Repair existing installation (`fixit' mode).",
    "8. Quit",
    "Don't do anything, just reboot.",
};

void 
stage0()
{
evil_goto:
    if (dialog_menu("Welcome to FreeBSD!",
		    "Use ALT-F2 and ALT-F1 to toggle between debugging\ninformation screen (ALT-F2) or this dialog screen (ALT-F1)\n\nPlease select one of the following options:", -1, -1, 8, 8, welcome, selection))
	ExitSysinstall();

    switch (atoi(selection)) {
    case 1:	/* View the README */
        ShowFile(README_FILE, "Read Me First");
	goto evil_goto;
	break;

    case 2:	/* View the release notes */
        ShowFile(RELNOTES_FILE, "Release Notes");
	goto evil_goto;
	break;

    case 3:	/* View the troubleshooting file */
        ShowFile(TROUBLE_FILE, "Troubleshooting");
	goto evil_goto;
	break;

    case 4:	/* View DISK FAQ */
        ShowFile(HELPME_FILE, "DISK FAQ");
	goto evil_goto;
	break;

    case 5:	/* View copyrights */
        ShowFile(COPYRIGHT_FILE, "COPYRIGHT");
	goto evil_goto;
	break;

    case 6:	/* Proceed (do nothing special, really) */
	fixit = 0;
	break;

    case 7:
	dialog_clear();
	dialog_update();
	dialog_msgbox("WARNING!", 
"The usual install procedure will be invoked, but with most of the
sanity checks disabled.  The suggested course of action is to:
	1. Go to (F)disk and do a (W)rite, and possibly a (B)oot too
	   if your MBR has been wiped.
	2. Go into (D)isklabel and identify your root (/) and swap
	   partitions.
	3. Select (P)roceed to reboot and load the cpio floppy.
	4. You will now be in the stand-alone shell, where you may
	   conduct further repairs with the tools you'll find in
	   /stand.
	5. Good luck...  You'll probably need it.", -1, -1, 1);
	fixit = 1;
	break;

    case 8:
	/* Be neat.. */
	ExitSysinstall();
	break;	/* hope not! :) */
    }
}
