/*
 * Copyright (c) 1995
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * reverse netgroup map generator program
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Center for Telecommunications Research
 * Columbia University, New York City
 *
 *	$Id: revnetgroup.c,v 1.1.1.1 1995/10/26 16:25:29 wpaul Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#define LINSIZ 1024

/* Default location of netgroup file. */
char *netgroup = "/etc/netgroup";

/* Stored hash table version of 'forward' netgroup database. */
struct group_entry *gtable[TABLESIZE];

/*
 * Stored hash table of 'reverse' netgroup member database
 * which we will construct.
 */
struct member_entry *mtable[TABLESIZE];

extern void store __P(( struct group_entry ** , char *, char * ));
extern void mstore __P(( struct member_entry ** , char *, char *, char * ));
extern char *lookup __P(( struct group_entry **, char * ));

void usage(prog)
char *prog;
{
	fprintf (stderr,"usage: %s -u|-h [-f netgroup file]\n",prog);
	exit(1);
}

extern char *optarg;

main(argc, argv)
	int argc;
	char *argv[];
{
	FILE *fp;
	char readbuf[LINSIZ];
	struct group_entry *gcur;
	struct member_entry *mcur;
	char *host, *user, *domain;
	int ch;
	char *key = NULL, *data = NULL;
	int hosts = -1, i;

	if (argc < 2)
		usage(argv[0]);

	while ((ch = getopt(argc, argv, "uhf:")) != EOF) {
		switch(ch) {
		case 'u':
			if (hosts != -1) {
				warnx("please use only one of -u or -h");
				usage(argv[0]);
			}
			hosts = 0;
			break;
		case 'h':
			if (hosts != -1) {
				warnx("please use only one of -u or -h");
				usage(argv[0]);
			}
			hosts = 1;
			break;
		case 'f':
			netgroup = optarg;
			break;
		default:
			usage(argv[0]);
			break;
		}
	}

	if (hosts == -1)
		usage(argv[0]);

	if (strcmp(netgroup, "-")) {
		if ((fp = fopen(netgroup, "r")) == NULL) {
			perror(netgroup);
			exit(1);
		}
	} else {
		fp = stdin;
	}

	/* Stuff all the netgroup names and members into a hash table. */
	while (fgets(readbuf, LINSIZ, fp)) {
		if (readbuf[0] == '#')
			continue;
		if ((data = (char *)(strpbrk(readbuf, " \t") + 1)) < (char *)2)
			continue;
		key = (char *)&readbuf;
		*(data - 1) = '\0';
		store(gtable, key, data);
	}

	fclose(fp);

	/*
	 * Find all members of each netgroup and keep track of which
	 * group they belong to.
	 */
	for (i = 0; i < TABLESIZE; i++) {
		gcur = gtable[i];
		while(gcur) {
			__setnetgrent(gcur->key);
			while(__getnetgrent(&host, &user, &domain) != NULL) {
				if (hosts ? host && strcmp(host,"-") : user && strcmp(user, "-"))
					mstore(mtable, hosts ? host : user, gcur->key, domain);
			}
			gcur = gcur->next;
		}
	}

	/* Release resources used by the netgroup parser code. */
	__endnetgrent();

	/* Spew out the results. */
	for (i = 0; i < TABLESIZE; i++) {
		mcur = mtable[i];
		while(mcur) {
			struct grouplist *tmp;
			printf ("%s.%s\t", mcur->key, mcur->domain);
			tmp = mcur->groups;
			while(tmp) {
				printf ("%s", tmp->groupname);
				tmp = tmp->next;
				if (tmp)
					printf(",");
			}
			mcur = mcur->next;
			printf ("\n");
		}
	}

	/* Let the OS free all our resources. */
	exit(0);
}
