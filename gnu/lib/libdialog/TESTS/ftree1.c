/*
 * ftree1.c
 *
 * small test-driver for new dialog functionality
 *
 * Copyright (c) 1998, Anatoly A. Orehovsky
 *
 * file ./ftree1.test with xterm widget tree from 
 * direct editres(1) dump needed !!!
 *
 * $FreeBSD: src/gnu/lib/libdialog/TESTS/ftree1.c,v 1.2 2000/01/10 11:52:04 phantom Exp $
 */

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
