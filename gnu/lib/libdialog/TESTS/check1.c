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
 * $FreeBSD: src/gnu/lib/libdialog/TESTS/check1.c,v 1.7 2000/01/10 11:52:02 phantom Exp $
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

/* menu2 - A more advanced way of using checked and fire hooks to manipulate the backing-variables directly */
/* prompt	title					checked		fire		sel   data */
static dialogMenuItem menu2[] = {
    { "German",	"Buy book on learning German",		getBool,	setBool,	NULL, &german_book},
    { "Italian",	"Buy book on learning Italian",		getBool,	setBool,	NULL, &italian_book },
    { "Slang",	"Buy book on commonly used insults",	getBool,	setBool,	NULL, &slang_book },
    { "Clear",	"Clear book list",			NULL,		clearBooks,	NULL, NULL,	' ', ' ', ' ' },
};

/* End of hook functions */

/* Kick it off, James! */
int
main(int argc, char **argv)
{
    int retval;
    
    init_dialog();
    
    retval = dialog_checklist("this is a dialog_checklist() in action, test #1",
			      "this checklist menu shows off some of the straight-forward features\n"
			      "of the new menu system's check & fire dispatch hooks", -1, -1, 4, -4, menu2, NULL);
    dialog_clear();
    fprintf(stderr, "returned value for dialog_checklist was %d (%d %d %d)\n", retval, german_book, italian_book, slang_book);
    
    end_dialog();
    return 0;
}
