/*
 * ftree2.c
 *
 * small test-driver for new dialog functionality
 *
 * Copyright (c) 1998, Anatoly A. Orehovsky
 *
 * file ./ftree2.test with xterm widget tree from 
 * preprocess editres(1) dump needed !!!
 *
 * $FreeBSD$
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
	use_helpfile("ftree2.test");
	use_helpline("Press Arrows, Tab, Enter or F1");
	retval = dialog_ftree("ftree2.test", '\t',
		"ftree dialog box example",
		"xterm widget tree from preprocess editres(1) dump", 
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
