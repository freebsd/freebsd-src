/*
 * Copyright (c) 1999 Martin Blapp
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/syslog.h>

#include <rpc/rpc.h>
#include <nfs/rpcv2.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mounttab.h"

int verbose;
struct mtablist *mtabhead;

/*
 * Add an entry to PATH_MOUNTTAB for each mounted NFS filesystem,
 * so the client can notify the NFS server even after reboot.
 */
int
add_mtab(char *hostp, char *dirp) {
	FILE *mtabfile;
	time_t *now;

	now = NULL;
	if ((mtabfile = fopen(PATH_MOUNTTAB, "a")) == NULL)
		return (0);
	else {
		fprintf(mtabfile, "%ld\t%s\t%s\n", time(now), hostp, dirp);
		fclose(mtabfile);
		return (1);
	}
}

/*
 * Read mounttab line for line and return struct mtablist.
 */
int
read_mtab(struct mtablist *mtabp) {
	struct mtablist **mtabpp;
	char *hostp, *dirp, *cp;
	char str[STRSIZ];
	char *timep;
	time_t time;
	FILE *mtabfile;

	if ((mtabfile = fopen(PATH_MOUNTTAB, "r")) == NULL) {
		if (errno == ENOENT)
			return (0);
		else {
			syslog(LOG_ERR, "can't open %s", PATH_MOUNTTAB);
			return (0);
		}
	}
	time = 0;
	mtabpp = &mtabhead;
	while (fgets(str, STRSIZ, mtabfile) != NULL) {
		cp = str;
		errno = 0;
		if (*cp == '#' || *cp == ' ' || *cp == '\n')
			continue;
		timep = strsep(&cp, " \t\n");
		if (timep == NULL || *timep == ' ' || *timep == '\n') {
			badline(timep);
			continue;
		}
		hostp = strsep(&cp, " \t\n");
		if (hostp == NULL || *hostp == ' ' || *hostp == '\n') {
			badline(hostp);
			continue;
		}
		dirp = strsep(&cp, " \t\n");
		if (dirp == NULL || *dirp == ' ' || *dirp == '\n') {
			badline(dirp);
			continue;
		}
		time = strtoul(timep, (char **)NULL, 10);
		if (errno == ERANGE) {
			badline(timep);
			continue;
		}
		if ((mtabp = malloc(sizeof (struct mtablist))) == NULL) {
			syslog(LOG_ERR, "malloc");
			fclose(mtabfile);
			return (0);
		}
		mtabp->mtab_time = time;
		memmove(mtabp->mtab_host, hostp, RPCMNT_NAMELEN);
		mtabp->mtab_host[RPCMNT_NAMELEN - 1] = '\0';
		memmove(mtabp->mtab_dirp, dirp, RPCMNT_PATHLEN);
		mtabp->mtab_dirp[RPCMNT_PATHLEN - 1] = '\0';
		mtabp->mtab_next = (struct mtablist *)NULL;
		*mtabpp = mtabp;
		mtabpp = &mtabp->mtab_next;
	}
	fclose(mtabfile);
	return (1);
}

/*
 * Rewrite PATH_MOUNTTAB from scratch and skip bad entries.
 * Unlink PATH_MOUNTAB if no entry is left.
 */
int
write_mtab() {
	struct mtablist *mtabp;
	FILE *mtabfile;
	int line;

	if ((mtabfile = fopen(PATH_MOUNTTAB, "w")) == NULL) {
		syslog(LOG_ERR, "can't write to %s", PATH_MOUNTTAB);
			return (0);
	}
	line = 0;
	for (mtabp = mtabhead; mtabp != NULL; mtabp = mtabp->mtab_next) {
		if (mtabp->mtab_host != NULL &&
		    strlen(mtabp->mtab_host) > 0) {
			fprintf(mtabfile, "%ld\t%s\t%s\n", mtabp->mtab_time,
			    mtabp->mtab_host, mtabp->mtab_dirp);
			if (verbose) {
				warnx("write entry " "%s:%s",
				    mtabp->mtab_host, mtabp->mtab_dirp);
			}
			clean_mtab(mtabp->mtab_host, mtabp->mtab_dirp);
			line++;
		}		
	}
	fclose(mtabfile);
	if (line == 0) {
		if (unlink(PATH_MOUNTTAB) == -1) {
			syslog(LOG_ERR, "can't remove %s", PATH_MOUNTTAB);
			return (0);
		}
	}
	return (1);
}

/*
 * Mark the entries as clean where RPC calls have been done successfully.
 */
void
clean_mtab(char *hostp, char *dirp) {
	struct mtablist *mtabp;
	char *host;

	host = strdup(hostp);
	for (mtabp = mtabhead; mtabp != NULL; mtabp = mtabp->mtab_next) {
		if (mtabp->mtab_host != NULL &&
		    strcmp(mtabp->mtab_host, host) == 0) {
			if (dirp == NULL) {
				if (verbose) {
					warnx("delete entries "
					    "host %s", host);
				}
				bzero(mtabp->mtab_host, RPCMNT_NAMELEN);
			} else {
				if (strcmp(mtabp->mtab_dirp, dirp) == 0) {
					if (verbose) {
						warnx("delete entry "
						    "%s:%s", host, dirp);
					}
					bzero(mtabp->mtab_host, RPCMNT_NAMELEN);
				}
			}
		}
	}
	free(host);
}

/*
 * Free struct mtablist mtab.
 */
void
free_mtab() {
	struct mtablist *mtabp;
	struct mtablist *mtab_next;

	for (mtabp = mtabhead; mtabp != NULL; mtabp = mtab_next) {
		mtab_next = mtabp->mtab_next;
		free(mtabp);
		mtabp = mtab_next;
	}
}

/*
 * Print bad lines to syslog.
 */
void
badline(char *bad) {

	syslog(LOG_ERR, "skip bad line in mounttab with entry %s", bad);
}
