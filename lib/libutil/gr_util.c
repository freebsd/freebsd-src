/*-
 * Copyright (c) 2008 Sean C. Farley <scf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <grp.h>
#include <inttypes.h>
#include <libutil.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char GroupLineFormat[] = "%s:%s:%ju:";

/*
 * Compares two struct group's.
 */
int
gr_equal(const struct group *gr1, const struct group *gr2)
{
	int gr1Ndx;
	int gr2Ndx;
	bool equal;
	bool found;

	/* Check that the non-member information is the same. */
	equal = strcmp(gr1->gr_name, gr2->gr_name) == 0 &&
	    strcmp(gr1->gr_passwd, gr2->gr_passwd) == 0 &&
	    gr1->gr_gid == gr2->gr_gid;

	/* Check all members in both groups. */
	if (equal) {
		for (found = false, gr1Ndx = 0; gr1->gr_mem[gr1Ndx] != NULL;
		    gr1Ndx++) {
			for (gr2Ndx = 0; gr2->gr_mem[gr2Ndx] != NULL; gr2Ndx++)
				if (strcmp(gr1->gr_mem[gr1Ndx],
				    gr2->gr_mem[gr2Ndx]) == 0) {
					found = true;
					break;
				}
			if (! found) {
				equal = false;
				break;
			}
		}

		/* Check that group2 does not have more members than group1. */
		if (gr2->gr_mem[gr1Ndx] != NULL)
			equal = false;
	}

	return (equal);
}

/*
 * Make a group line out of a struct group.
 */
char *
gr_make(const struct group *gr)
{
	char *line;
	size_t lineSize;
	int ndx;

	/* Calculate the length of the group line. */
	lineSize = snprintf(NULL, 0, GroupLineFormat, gr->gr_name,
	    gr->gr_passwd, (uintmax_t)gr->gr_gid) + 1;
	for (ndx = 0; gr->gr_mem[ndx] != NULL; ndx++)
		lineSize += strlen(gr->gr_mem[ndx]) + 1;
	if (ndx > 0)
		lineSize--;

	/* Create the group line and fill it. */
	if ((line = malloc(lineSize)) == NULL)
		return (NULL);
	lineSize = snprintf(line, lineSize, GroupLineFormat, gr->gr_name,
	    gr->gr_passwd, (uintmax_t)gr->gr_gid);
	for (ndx = 0; gr->gr_mem[ndx] != NULL; ndx++) {
		strcat(line, gr->gr_mem[ndx]);
		if (gr->gr_mem[ndx + 1] != NULL)
			strcat(line, ",");
	}

	return (line);
}

/*
 * Duplicate a struct group.
 */
struct group *
gr_dup(const struct group *gr)
{
	size_t len;
	struct group *ngr;
	int ndx;
	int numMem;

	/* Calculate size of group. */
	len = sizeof(*gr) +
	    (gr->gr_name != NULL ? strlen(gr->gr_name) + 1 : 0) +
	    (gr->gr_passwd != NULL ? strlen(gr->gr_passwd) + 1 : 0);
	numMem = 0;
	if (gr->gr_mem != NULL) {
		for (; gr->gr_mem[numMem] != NULL; numMem++)
			len += strlen(gr->gr_mem[numMem]) + 1;
		len += (numMem + 1) * sizeof(*gr->gr_mem);
	}

	/* Create new group and copy old group into it. */
	if ((ngr = calloc(1, len)) == NULL)
		return (NULL);
	len = sizeof(*ngr);
	ngr->gr_gid = gr->gr_gid;
	if (gr->gr_name != NULL) {
		ngr->gr_name = (char *)ngr + len;
		len += sprintf(ngr->gr_name, "%s", gr->gr_name) + 1;
	}
	if (gr->gr_passwd != NULL) {
		ngr->gr_passwd = (char *)ngr + len;
		len += sprintf(ngr->gr_passwd, "%s", gr->gr_passwd) + 1;
	}
	if (gr->gr_mem != NULL) {
		ngr->gr_mem = (char **)((char *)ngr + len);
		len += (numMem + 1) * sizeof(*ngr->gr_mem);
		for (ndx = 0; gr->gr_mem[ndx] != NULL; ndx++) {
			ngr->gr_mem[ndx] = (char *)ngr + len;
			len += sprintf(ngr->gr_mem[ndx], "%s",
			    gr->gr_mem[ndx]) + 1;
		}
		ngr->gr_mem[ndx] = NULL;
	}

	return (ngr);
}

/*
 * Scan a line and place it into a group structure.
 */
static bool
__gr_scan(char *line, struct group *gr)
{
	char *loc;
	int ndx;

	/* Assign non-member information to structure. */
	gr->gr_name = line;
	if ((loc = strchr(line, ':')) == NULL)
		return (false);
	*loc = '\0';
	gr->gr_passwd = loc + 1;
	if (*gr->gr_passwd == ':')
		*gr->gr_passwd = '\0';
	else {
		if ((loc = strchr(loc + 1, ':')) == NULL)
			return (false);
		*loc = '\0';
	}
	if (sscanf(loc + 1, "%u", &gr->gr_gid) != 1)
		return (false);

	/* Assign member information to structure. */
	if ((loc = strchr(loc + 1, ':')) == NULL)
		return (false);
	line = loc + 1;
	gr->gr_mem = NULL;
	if (*line != '\0') {
		ndx = 0;
		do {
			if ((gr->gr_mem = reallocf(gr->gr_mem,
			    sizeof(*gr->gr_mem) * (ndx + 1))) == NULL)
				return (false);
			gr->gr_mem[ndx] = strsep(&line, ",");
		} while (gr->gr_mem[ndx++] != NULL);
	}

	return (true);
}

/*
 * Create a struct group from a line.
 */
struct group *
gr_scan(const char *line)
{
	struct group gr;
	char *lineCopy;
	struct group *newGr;

	if ((lineCopy = strdup(line)) == NULL)
		return (NULL);
	if (!__gr_scan(lineCopy, &gr)) {
		free(lineCopy);
		return (NULL);
	}
	newGr = gr_dup(&gr);
	free(lineCopy);
	if (gr.gr_mem != NULL)
		free(gr.gr_mem);

	return (newGr);
}
