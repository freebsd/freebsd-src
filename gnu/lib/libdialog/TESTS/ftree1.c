/*
 * ftree1.c
 *
 * small test-driver for new dialog functionality
 *
 * Copyright (c) 1998, Anatoly A. Orehovsky
 *
 * file ./ftree1.test with xterm widget tree from 
 * direct editres(1) dump needed !!!
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/gnu/lib/libdialog/TESTS/ftree1.c,v 1.3.28.1 2008/10/02 02:57:24 kensmith Exp $");

#include <stdio.h>
#include <stdlib.h>
#include <dialog.h>

int
main(int argc, char **argv)
{
	int retval;
	unsigned char *tresult;

	init_dialog();
	retval = dialog_ftree("ftree1.test", '\t',
		"ftree dialog box example",
		"xterm widget tree from direct editres(1) dump", 
		-1, -1, 15,
                            &tresult);

	dialog_update();
	
	dialog_clear();
	
	end_dialog();

	if (!retval)
	{
 		puts(tresult);
  		free(tresult);
  	}
  	
	exit(retval);
}
