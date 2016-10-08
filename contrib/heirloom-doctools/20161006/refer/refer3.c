/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * Copyright (c) 1983, 1984 1985, 1986, 1987, 1988, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

/*	from OpenSolaris "refer3.c	1.3	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)refer3.c	1.3 (gritter) 10/22/05
 */

#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "refer..c"
#define move(x, y) close(y); dup(x); close(x);

int
corout(char *in, char *out, char *rprog, char *arg, int outlen)
{
	int pipev[2], fr1, fr2, fw1, fw2, n;
	int pid, status;

	pipe(pipev); 
	fr1 = pipev[0]; 
	fw1 = pipev[1];
	pipe(pipev); 
	fr2 = pipev[0]; 
	fw2 = pipev[1];
	if ((pid = fork()) == 0)
	{
		close(fw1); 
		close(fr2);
		move(fr1, 0);
		move(fw2, 1);
		execl(rprog, "deliv", arg, NULL);
		err("Can't run %s", rprog);
	}
	close(fw2); 
	close(fr1);
	write(fw1, in , strlen(in));
	close(fw1);
	while (wait(&status) != pid);
	n = read(fr2, out, outlen);
	out[n] = 0;
	close(fr2);
	return(n);
}
