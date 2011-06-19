/*	$NetBSD: sl_init.c,v 1.5 2008/04/29 08:13:38 martin Exp $	*/
/*	from	NetBSD: stringlist.c,v 1.10 2000/01/25 16:24:40 enami Exp	*/

/*-
 * Copyright (c) 1994, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "tnftp.h"

#define _SL_CHUNKSIZE	20

/*
 * sl_init(): Initialize a string list
 */
StringList *
sl_init(void)
{
	StringList *sl;

	sl = malloc(sizeof(StringList));
	if (sl == NULL)
		return (NULL);

	sl->sl_cur = 0;
	sl->sl_max = _SL_CHUNKSIZE;
	sl->sl_str = malloc(sl->sl_max * sizeof(char *));
	if (sl->sl_str == NULL) {
		free(sl);
		sl = NULL;
	}
	return (sl);
}


/*
 * sl_add(): Add an item to the string list
 */
int
sl_add(StringList *sl, char *name)
{
	if (sl->sl_cur == sl->sl_max - 1) {
		char	**new;

		new = (char **)realloc(sl->sl_str,
		    (sl->sl_max + _SL_CHUNKSIZE) * sizeof(char *));
		if (new == NULL)
			return (-1);
		sl->sl_max += _SL_CHUNKSIZE;
		sl->sl_str = new;
	}
	sl->sl_str[sl->sl_cur++] = name;
	return (0);
}


/*
 * sl_free(): Free a stringlist
 */
void
sl_free(StringList *sl, int all)
{
	size_t i;

	if (sl == NULL)
		return;
	if (sl->sl_str) {
		if (all)
			for (i = 0; i < sl->sl_cur; i++)
				free(sl->sl_str[i]);
		free(sl->sl_str);
	}
	free(sl);
}


/*
 * sl_find(): Find a name in the string list
 */
char *
sl_find(StringList *sl, char *name)
{
	size_t i;

	for (i = 0; i < sl->sl_cur; i++)
		if (strcmp(sl->sl_str[i], name) == 0)
			return (sl->sl_str[i]);

	return (NULL);
}
