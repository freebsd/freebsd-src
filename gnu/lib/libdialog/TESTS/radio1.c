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
 * $Id: radio1.c,v 1.3 1996/04/16 12:17:25 jkh Exp $
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

/* menu5 - Show a simple radiolist menu that inherits the radio appearance by default */
/* prompt	title			checked		fire		sel   data */
static dialogMenuItem menu5[] = {
    { "1000",	"Spend $1,000",		check,		spend,		NULL, (void *)1000 },
    { "500",	"Spend $500",		check,		spend,		NULL, (void *)500 },
    { "100",	"Spend $100",		check,		spend, 		NULL, (void *)100 },
};

/* End of hook functions */

/* Kick it off, James! */
int
main(int argc, unsigned char *argv[])
{
    int retval;
    
    init_dialog();
    
    
    retval = dialog_radiolist("this is dialog_radiolist() in action, test #1",
			      "this radio menu shows off some of the straight-forward features\n"
			      "of the new menu system's check & fire dispatch hooks", -1, -1, 3, -3, menu5, NULL);
    dialog_clear();
    fprintf(stderr, "returned value for dialog_radiolist was %d (money set to %d)\n", retval, spending);
    
    end_dialog();
    return 0;
}
