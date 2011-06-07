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

struct group_storage {
	struct group	 gr;
	char		*members[];
};

static const char group_line_format[] = "%s:%s:%ju:";

/*
 * Compares two struct group's.
 */
int
gr_equal(const struct group *gr1, const struct group *gr2)
{
	int gr1_ndx;
	int gr2_ndx;
	bool found;

	/* Check that the non-member information is the same. */
	if (gr1->gr_name == NULL || gr2->gr_name == NULL) {
		if (gr1->gr_name != gr2->gr_name)
			return (false);
	} else if (strcmp(gr1->gr_name, gr2->gr_name) != 0)
		return (false);
	if (gr1->gr_passwd == NULL || gr2->gr_passwd == NULL) {
		if (gr1->gr_passwd != gr2->gr_passwd)
			return (false);
	} else if (strcmp(gr1->gr_passwd, gr2->gr_passwd) != 0)
		return (false);
	if (gr1->gr_gid != gr2->gr_gid)
		return (false);

	/* Check all members in both groups. */
	if (gr1->gr_mem == NULL || gr2->gr_mem == NULL) {
		if (gr1->gr_mem != gr2->gr_mem)
			return (false);
	} else {
		for (found = false, gr1_ndx = 0; gr1->gr_mem[gr1_ndx] != NULL;
		    gr1_ndx++) {
			for (gr2_ndx = 0; gr2->gr_mem[gr2_ndx] != NULL;
			    gr2_ndx++)
				if (strcmp(gr1->gr_mem[gr1_ndx],
				    gr2->gr_mem[gr2_ndx]) == 0) {
					found = true;
					break;
				}
			if (!found)
				return (false);
		}

		/* Check that group2 does not have more members than group1. */
		if (gr2->gr_mem[gr1_ndx] != NULL)
			return (false);
	}

	return (true);
}

/*
 * Make a group line out of a struct group.
 */
char *
gr_make(const struct group *gr)
{
	char *line;
	size_t line_size;
	int ndx;

	/* Calculate the length of the group line. */
	line_size = snprintf(NULL, 0, group_line_format, gr->gr_name,
	    gr->gr_passwd, (uintmax_t)gr->gr_gid) + 1;
	if (gr->gr_mem != NULL) {
		for (ndx = 0; gr->gr_mem[ndx] != NULL; ndx++)
			line_size += strlen(gr->gr_mem[ndx]) + 1;
		if (ndx > 0)
			line_size--;
	}

	/* Create the group line and fill it. */
	if ((line = malloc(line_size)) == NULL)
		return (NULL);
	snprintf(line, line_size, group_line_format, gr->gr_name, gr->gr_passwd,
	    (uintmax_t)gr->gr_gid);
	if (gr->gr_mem != NULL)
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
	char *dst;
	size_t len;
	struct group_storage *gs;
	int ndx;
	int num_mem;

	/* Calculate size of the group. */
	len = sizeof(*gs);
	if (gr->gr_name != NULL)
		len += strlen(gr->gr_name) + 1;
	if (gr->gr_passwd != NULL)
		len += strlen(gr->gr_passwd) + 1;
	if (gr->gr_mem != NULL) {
		for (num_mem = 0; gr->gr_mem[num_mem] != NULL; num_mem++)
			len += strlen(gr->gr_mem[num_mem]) + 1;
		len += (num_mem + 1) * sizeof(*gr->gr_mem);
	} else
		num_mem = -1;

	/* Create new group and copy old group into it. */
	if ((gs = calloc(1, len)) == NULL)
		return (NULL);
	dst = (char *)&gs->members[num_mem + 1];
	if (gr->gr_name != NULL) {
		gs->gr.gr_name = dst;
		dst = stpcpy(gs->gr.gr_name, gr->gr_name) + 1;
	}
	if (gr->gr_passwd != NULL) {
		gs->gr.gr_passwd = dst;
		dst = stpcpy(gs->gr.gr_passwd, gr->gr_passwd) + 1;
	}
	gs->gr.gr_gid = gr->gr_gid;
	if (gr->gr_mem != NULL) {
		gs->gr.gr_mem = gs->members;
		for (ndx = 0; ndx < num_mem; ndx++) {
			gs->gr.gr_mem[ndx] = dst;
			dst = stpcpy(gs->gr.gr_mem[ndx], gr->gr_mem[ndx]) + 1;
		}
		gs->gr.gr_mem[ndx] = NULL;
	}

	return (&gs->gr);
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
	ndx = 0;
	do {
		gr->gr_mem = reallocf(gr->gr_mem, sizeof(*gr->gr_mem) *
		    (ndx + 1));
		if (gr->gr_mem == NULL)
			return (false);

		/* Skip locations without members (i.e., empty string). */
		do {
			gr->gr_mem[ndx] = strsep(&line, ",");
		} while (gr->gr_mem[ndx] != NULL && *gr->gr_mem[ndx] == '\0');
	} while (gr->gr_mem[ndx++] != NULL);

	return (true);
}

/*
 * Create a struct group from a line.
 */
struct group *
gr_scan(const char *line)
{
	struct group gr;
	char *line_copy;
	struct group *new_gr;

	if ((line_copy = strdup(line)) == NULL)
		return (NULL);
	if (!__gr_scan(line_copy, &gr)) {
		free(line_copy);
		return (NULL);
	}
	new_gr = gr_dup(&gr);
	free(line_copy);
	if (gr.gr_mem != NULL)
		free(gr.gr_mem);

	return (new_gr);
}
