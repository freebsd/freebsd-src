/*
 * tree.c
 *
 * small test-driver for new dialog functionality
 *
 * Copyright (c) 1998, Anatoly A. Orehovsky
 *
 * $FreeBSD$
 */

#include <stdio.h>
#include <stdlib.h>
#include <dialog.h>

unsigned char *names[] = {
	"/",
	"/dev",
	"/dev/fd",
	"/tmp",
	"/usr",
	"/var",
	"/home",
	"/stand",
	"/stand/etc",
	"/stand/en_US.ISO8859-1",
	"/stand/info",
	"/stand/info/bin",
	"/stand/info/des",
	"/stand/info/games",
	"/stand/info/manpages",
	"/stand/info/proflibs",
	"/stand/info/dict",
	"/stand/info/info",
	"/stand/info/src",
	"/etc",
	"/etc/gnats",
	"/etc/kerberosIV",
	"/etc/mtree",
	"/etc/namedb",
	"/etc/ppp",
	"/etc/uucp",
	"/etc/sliphome",
	"/proc",
	"/lkm",
	"/mnt",
	"/root",
	"/sbin",
	"/bin",
	0
};

unsigned char *names1[] = {
	"a",
	"a:b",
	"a:b:c",
	"a:d"
};

int
main(int argc, char **argv)
{
	int retval;
	unsigned char *tresult;
	char comstr[BUFSIZ];

	init_dialog();
	do {
		use_helpline("Press OK for listing directory");
		retval = dialog_tree(names, 
			sizeof(names)/sizeof(unsigned char *) - 1,
			 '/',
			"tree dialog box example",
			"Typical find -x / -type d output", 
			-1, -1, 15,
                        	    &tresult);
		
		if (retval)
			break;
		
		use_helpline(NULL);
		(void)snprintf(comstr, sizeof(comstr), 
			"ls -CF %s", tresult);

		retval = dialog_prgbox(
			comstr, 
			comstr, 20, 60, TRUE, TRUE);

		dialog_clear();
  		
		retval = dialog_tree(names1, 
			sizeof(names1)/sizeof(unsigned char *),
			 ':',
			"tree dialog box example",
			"Other tree", 
			-1, -1, 5,
                        	    &tresult);
		if (!retval)
		{
	  		dialog_clear();
  		}
	} while (!retval);

	dialog_update();
	
	dialog_clear();
	
	end_dialog();
  	
	exit(retval);
}
