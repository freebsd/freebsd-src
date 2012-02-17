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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/wait.h>
#include <dialog.h>

/* Hook functions */

static int
getBool(dialogMenuItem *self)
{
    if (self->data && *((int *)self->data))
	return TRUE;
    return FALSE;
}

static int
setBool(dialogMenuItem *self)
{
    if (self->data) {
	*((int *)self->data) = !*((int *)self->data);
	return DITEM_SUCCESS;
    }
    return DITEM_FAILURE;
}

static int german_book, italian_book, slang_book;
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

/* menu4 - Show off a simulated compound menu (group at top is checklist, group at bottom radio) */
/* prompt	title					checked		fire	 sel,  data	     lbra mark rbra */
static dialogMenuItem menu4[] = {
    { "German",	"Buy books on learning German",		getBool,	setBool, NULL, &german_book },
    { "Italian","Buy books on learning Italian",	getBool,	setBool, NULL, &italian_book },
    { "Slang",	"Buy books on commonly used insults",	getBool,	setBool, NULL, &slang_book },
    { "-----",	"----------------------------------",	NULL,		NULL,	 NULL, NULL,	     ' ', ' ', ' ' },
    { "1000",	"Spend $1,000",				check,		spend,	 NULL, (void *)1000, '(', '*', ')' },
    { "500",	"Spend $500",				check,		spend,	 NULL, (void *)500,  '(', '*', ')' },
    { "100",	"Spend $100",				check,		spend,	 NULL, (void *)100,  '(', '*', ')' },
};

/* End of hook functions */

/* Kick it off, James! */
int
main(int argc, char **argv)
{
    int retval;
    
    init_dialog();
    
    
    retval = dialog_checklist("this is dialog_checklist() in action, test #3",
			      "Now we show off some of the button 'styles' one can create.",
			      -1, -1, 7, -7, menu4, NULL);
    dialog_clear();
    fprintf(stderr, "spent $%d on %s%s%s books\n", spending, german_book ? " german" : "",
	    italian_book ? " italian" : "", slang_book ? " slang" : "");
    
    end_dialog();
    return 0;
}
