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
 * $FreeBSD: src/gnu/lib/libdialog/TESTS/check2.c,v 1.6 2000/01/10 11:52:02 phantom Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

static int
clearBooks(dialogMenuItem *self)
{
    german_book = italian_book = slang_book = FALSE;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
buyBooks(dialogMenuItem *self)
{
    char foo[256];
    
    if (german_book || italian_book || slang_book) {
	strcpy(foo, "Ok, you're buying books on");
	if (german_book)
	    strcat(foo, " german");
	if (italian_book)
	    strcat(foo, " italian");
	if (slang_book)
	    strcat(foo, " slang");
    }
    else
	strcpy(foo, "You're not buying any books?");
    dialog_mesgbox("This is a direct callback for the `Buy' button", foo, -1, -1);
    return DITEM_SUCCESS;
}

/* menu3 - Look mom!  We can finally use our own OK and Cancel buttons! */
/* prompt	title					checked		fire		sel   data */
static dialogMenuItem menu3[] = {
    { "Buy!",	NULL,					NULL,		buyBooks	}, /* New "OK" button */
    { "No Way!",	NULL,					NULL,		NULL		}, /* New "Cancel" button */
    { "German",	"Buy books on learning German",		getBool,	setBool,	NULL, &german_book },
    { "Italian",	"Buy books on learning Italian",	getBool,	setBool,	NULL, &italian_book },
    { "Slang",	"Buy books on commonly used insults",	getBool,	setBool,	NULL, &slang_book },
    { "Clear",	"Clear book list",			NULL,		clearBooks,	NULL, NULL, ' ', ' ', ' ' },
};

/* End of hook functions */

/* Kick it off, James! */
int
main(int argc, char **argv)
{
    int retval;
    
    init_dialog();
    
    retval = dialog_checklist("this is dialog_checklist() in action, test #2",
			      "Same as before, but now we relabel the buttons and override the OK action.",
			      -1, -1, 4, -4, menu3 + 2, (char *)TRUE);
    dialog_clear();
    fprintf(stderr, "returned value for dialog_checklist was %d\n", retval);
    
    end_dialog();
    return 0;
}
