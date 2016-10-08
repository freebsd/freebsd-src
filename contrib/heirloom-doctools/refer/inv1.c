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

/*	from OpenSolaris "inv1.c	1.5	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)inv1.c	1.3 (gritter) 10/22/05
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <locale.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "refer..c"

int
main(int argc, char **argv)
{
	/* Make inverted file indexes.  Reads a stream from mkey which
	 * gives record pointer items and keys.  Generates set of files
	 *	a. NHASH pointers to file b.
	 *	b. lists of record numbers.
	 *	c. record pointer items.
	 *
	 *  these files are named xxx.ia, xxx.ib, xxx.ic;
	 *  where xxx is taken from arg1.
	 *  If the files exist they are updated.
	 */

	FILE *fa, *fb, *fc, *fta, *ftb;
	FILE *fd = NULL;
	int nhash = 256;
	int appflg = 1;
	int keepkey = 0, pipein = 0;
	char nma[100], nmb[100], nmc[100], com[100], nmd[100];
	char tmpa[20], tmpb[20], tmpc[20];
	char *remove = NULL;
	int chatty = 0, docs, hashes;
	long keys;
	int iflong =0;
	char *sortdir;

	sortdir = (access("/crp/tmp", 06)==0) ? "/crp/tmp" : "/var/tmp";
	while (argc>1 && argv[1][0] == '-')
	{
		switch(argv[1][1])
		{
		case 'h': /* size of hash table */
			nhash = atoi (argv[1]+2); 
			break;
		case 'n': /* new, don't append */
			appflg=0; 
			break;
		case 'a': /* append to old file */
			appflg=1; 
			break;
		case 'v': /* verbose output */
			chatty=1; 
			break;
		case 'd': /* keep keys on file .id for check on searching */
			keepkey=1; 
			break;
		case 'p': /* pipe into sort (saves space, costs time)*/
			pipein = 1; 
			break;
		case 'i': /* input is on file, not stdin */
			close(0);
			if (open(argv[2], O_RDONLY) != 0)
				err("Can't read input %s", argv[2]);
			if (argv[1][2]=='u') /* unlink */
				remove = argv[2];
			argc--; 
			argv++;
			break;
		}
		argc--;
		argv++;
	}
	n_strcpy (nma, argc >= 2 ? argv[1] : "Index", sizeof(nma));
	n_strcpy (nmb, nma, sizeof(nmb));
	n_strcpy (nmc, nma, sizeof(nmc));
	n_strcpy (nmd, nma, sizeof(nmd));
	n_strcat (nma, ".ia", sizeof(nma));
	n_strcat (nmb, ".ib", sizeof(nmb));
	n_strcat (nmc, ".ic", sizeof(nmc));
	n_strcat (nmd, ".id", sizeof(nmd));

	snprintf(tmpa, sizeof tmpa, "junk%di", (int)getpid());
	if (pipein)
	{
		snprintf(com, sizeof com, "sort -T %s -o %s", sortdir, tmpa);
		fta = popen(com, "w");
	}
	else /* use tmp file */
	{
		fta = fopen(tmpa, "w");
		assert (fta != NULL);
	}
	fb = 0;
	if (appflg )
	{
		if ((fb = fopen(nmb, "r")))
		{
			snprintf(tmpb, sizeof tmpb, "junk%dj", (int)getpid());
			ftb = fopen(tmpb, "w");
			if (ftb==NULL)
				err("Can't get scratch file %s",tmpb);
			nhash = recopy(ftb, fb, fopen(nma, "r"), nhash);
			fclose(ftb);
		}
		else
			appflg=0;
	}
	fc = fopen(nmc,  appflg ? "a" : "w");
	if (keepkey)
		fd = keepkey ? fopen(nmd, "w") : 0;
	docs = newkeys(fta, stdin, fc, nhash, fd, &iflong);
	fclose(stdin);
	if (remove != NULL)
		unlink(remove);
	fclose(fta);
	if (pipein)
	{
		pclose(fta);
	}
	else
	{
		snprintf(com, sizeof com,
				"sort -T %s %s -o %s", sortdir, tmpa, tmpa);
		system(com);
	}
	if (appflg)
	{
		snprintf(tmpc, sizeof tmpc, "junk%dk", (int)getpid());
		snprintf(com, sizeof com, "mv %s %s", tmpa, tmpc);
		system(com);
		snprintf(com, sizeof com, "sort -T %s  -m %s %s -o %s", sortdir,
		tmpb, tmpc, tmpa);
		system(com);
	}
	fta = fopen(tmpa, "r");
	fa = fopen(nma, "w");
	fb = fopen(nmb, "w");
	whash(fta, fa, fb, nhash, iflong, &keys, &hashes);
	fclose(fta);
# ifndef D1
	unlink(tmpa);
# endif
	if (appflg)
	{
		unlink(tmpb);
		unlink(tmpc);
	}
	if (chatty)

		printf ("%ld key occurrences,  %d hashes, %d docs\n",
		keys, hashes, docs);

	return 0;
}
