/* RCS keyword table and match operation */

/* Copyright 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991, 1992, 1993, 1995 Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/

/*
 * Revision 5.4  1995/06/16 06:19:24  eggert
 * Update FSF address.
 *
 * Revision 5.3  1993/11/03 17:42:27  eggert
 * Add Name keyword.
 *
 * Revision 5.2  1991/08/19  03:13:55  eggert
 * Say `T const' instead of `const T'; it's less confusing for pointer types.
 * (This change was made in other source files too.)
 *
 * Revision 5.1  1991/04/21  11:58:25  eggert
 * Don't put , just before } in initializer.
 *
 * Revision 5.0  1990/08/22  08:12:54  eggert
 * Add -k.  Ansify and Posixate.
 *
 * Revision 4.3  89/05/01  15:13:02  narten
 * changed copyright header to reflect current distribution rules
 *
 * Revision 4.2  87/10/18  10:36:33  narten
 * Updating version numbers. Changes relative to 1.1 actuallyt
 * relative to 4.1
 *
 * Revision 1.2  87/09/24  14:00:10  narten
 * Sources now pass through lint (if you ignore printf/sprintf/fprintf
 * warnings)
 *
 * Revision 4.1  83/05/04  10:06:53  wft
 * Initial revision.
 *
 */


#include "rcsbase.h"

libId(keysId, "$FreeBSD$")


char const *Keyword[] = {
    /* This must be in the same order as rcsbase.h's enum markers type. */
	0,
	AUTHOR, DATE, HEADER, IDH,
	LOCKER, LOG, NAME, RCSFILE, REVISION, SOURCE, STATE, CVSHEADER,
	NULL
};

/* Expand all keywords by default */
static int ExpandKeyword[] = {
	false,
	true, true, true, true,
	true, true, true, true, true, true, true, true,
	true
};
enum markers LocalIdMode = Id;

	enum markers
trymatch(string)
	char const *string;
/* function: Checks whether string starts with a keyword followed
 * by a KDELIM or a VDELIM.
 * If successful, returns the appropriate marker, otherwise Nomatch.
 */
{
        register int j;
	register char const *p, *s;
	for (j = sizeof(Keyword)/sizeof(*Keyword);  (--j);  ) {
		if (!ExpandKeyword[j])
			continue;
		/* try next keyword */
		p = Keyword[j];
		if (p == NULL)
			continue;
		s = string;
		while (*p++ == *s++) {
			if (!*p)
			    switch (*s) {
				case KDELIM:
				case VDELIM:
				    return (enum markers)j;
				default:
				    return Nomatch;
			    }
		}
        }
        return(Nomatch);
}

	void
setIncExc(arg)
	char const *arg;
/* Sets up the ExpandKeyword table according to command-line flags */
{
	char *key;
	char *copy, *next;
	int include = 0, j;

	copy = strdup(arg);
	next = copy;
	switch (*next++) {
	    case 'e':
		include = false;
		break;
	    case 'i':
		include = true;
		break;
	    default:
		free(copy);
		return;
	}
	if (include)
		for (j = sizeof(Keyword)/sizeof(*Keyword);  (--j);  )
			ExpandKeyword[j] = false;
	key = strtok(next, ",");
	while (key) {
		for (j = sizeof(Keyword)/sizeof(*Keyword);  (--j);  ) {
			if (Keyword[j] == NULL)
				continue;
			if (!strcmp(key, Keyword[j]))
				ExpandKeyword[j] = include;
		}
		key = strtok(NULL, ",");
	}
	free(copy);
	return;
}

	void
setRCSLocalId(string)
	char const *string;
/* function: sets local RCS id and RCSLOCALID envariable */
{
	static char local_id[keylength+1];
	char *copy, *next, *key;
	int j;

	copy = strdup(string);
	next = copy;
	key = strtok(next, "=");
	if (strlen(key) > keylength)
		error("LocalId is too long");
	VOID strcpy(local_id, key);
	Keyword[LocalId] = local_id;

	/* options? */
	while (key = strtok(NULL, ",")) {
		if (!strcmp(key, Keyword[Id]))
			LocalIdMode=Id;
		else if (!strcmp(key, Keyword[Header]))
			LocalIdMode=Header;
		else if (!strcmp(key, Keyword[CVSHeader]))
			LocalIdMode=CVSHeader;
		else
			error("Unknown LocalId mode");
	}
	free(copy);
}
