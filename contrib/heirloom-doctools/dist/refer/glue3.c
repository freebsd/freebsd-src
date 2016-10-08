/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	from OpenSolaris "glue3.c	1.7	05/06/02 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)glue3.c	1.5 (gritter) 9/7/08
 */


#include "refer..c"
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <inttypes.h>
#define move(x, y) close(y); dup(x); close(x);

int
corout(char *in, char *out, char *rprog, char *arg, int outlen)
{
	int pipev[2], fr1, fr2, fw1, fw2, n;
	int	pid;

# if D1
	fprintf(stderr, "in corout, rprog /%s/ in /%s/\n", 
		rprog ? rprog : "", strlen(in) ? in : "");
# endif

	if (strcmp (rprog, "hunt") ==0)
		return(callhunt(in, out, arg, outlen));
	if (strcmp (rprog, "deliv")==0)
		return(dodeliv(in, out, arg, outlen));
	pipe (pipev); 
	fr1= pipev[0]; 
	fw1 = pipev[1];
	pipe (pipev); 
	fr2= pipev[0]; 
	fw2 = pipev[1];
	if ((pid = fork())==0)
	{
		close (fw1); 
		close (fr2);
		move (fr1, 0);
		move (fw2, 1);
		if (rprog[0]!= '/')
			chdir(REFDIR);
		execl(rprog, "deliv", arg, NULL);
		err("Can't run %s", rprog);
	}
	close(fw2); 
	close(fr1);
	if (strlen(in) > 0)
		write (fw1, in , strlen(in));
	close(fw1);
	while (wait(0) != pid);
	n = read (fr2, out, outlen);
	out[n]=0;
	close(fr2);
	return 0;
}

# define ALEN 50

int
callhunt(char *in, char *out, char *arg, int outlen)
{
	char *argv[20], abuff[ALEN];
	int argc;
	extern char one[];
	extern int onelen;
	argv[0] = "hunt";
	argv[1] = "-i";
	argv[2] = in;
	argv[3] = "-t";
	argv[4] = out;
	argv[5] = (char *)(intptr_t)outlen;
	argv[6] = "-T";
	argv[7] = "-F1";
	argv[8] = "-o";
	argv[9] = one;
	argv[10] = (char *)(intptr_t)onelen;
	argv[11] = abuff; 
	if (strlen(arg) > ALEN)
		err("abuff not big enough %d", strlen(arg));
	strcpy (abuff,arg);
	argc = 6;
	huntmain (argc,argv);
	return(0);
}

int
dodeliv(char *in, char *out, char *arg, int outlen)
{
	char *mout;
	int mlen;
# if D1
	fprintf(stderr, "in dodeliv, arg /%s/\n", arg?arg:"");
# endif
	if (arg && arg[0])
		chdir(arg);

	mlen = findline(in, &mout, outlen,0L);

	if (mlen>0)
	{
		strncpy(out, mout, outlen);
		free (mout);
	}
	restodir();
	return 0;
}
