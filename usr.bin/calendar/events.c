/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1992-2009 Edwin Groothuis <edwin@FreeBSD.org>.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/time.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pathnames.h"
#include "calendar.h"

struct event *
event_add(int year, int month, int day, char *date, int var, char *txt,
    char *extra)
{
	struct event *e;

	/*
	 * Creating a new event:
	 * - Create a new event
	 * - Copy the machine readable day and month
	 * - Copy the human readable and language specific date
	 * - Copy the text of the event
	 */
	e = (struct event *)calloc(1, sizeof(struct event));
	if (e == NULL)
		errx(1, "event_add: cannot allocate memory");
	e->month = month;
	e->day = day;
	e->var = var;
	e->date = strdup(date);
	if (e->date == NULL)
		errx(1, "event_add: cannot allocate memory");
	e->text = strdup(txt);
	if (e->text == NULL)
		errx(1, "event_add: cannot allocate memory");
	e->extra = NULL;
	if (extra != NULL && extra[0] != '\0')
		e->extra = strdup(extra);
	addtodate(e, year, month, day);
	return (e);
}

void
event_continue(struct event *e, char *txt)
{
	char *text;

	/*
	 * Adding text to the event:
	 * - Save a copy of the old text (unknown length, so strdup())
	 * - Allocate enough space for old text + \n + new text + 0
	 * - Store the old text + \n + new text
	 * - Destroy the saved copy.
	 */
	text = strdup(e->text);
	if (text == NULL)
		errx(1, "event_continue: cannot allocate memory");

	free(e->text);
	asprintf(&e->text, "%s\n%s", text, txt);
	if (e->text == NULL)
		errx(1, "event_continue: cannot allocate memory");
	free(text);

	return;
}

void
event_print_all(FILE *fp)
{
	struct event *e;

	while (walkthrough_dates(&e) != 0) {
#ifdef DEBUG
		fprintf(stderr, "event_print_allmonth: %d, day: %d\n",
		    month, day);
#endif

		/*
		 * Go through all events and print the text of the matching
		 * dates
		 */
		while (e != NULL) {
			(void)fprintf(fp, "%s%c%s%s%s%s\n", e->date,
			    e->var ? '*' : ' ', e->text,
			    e->extra != NULL ? " (" : "",
			    e->extra != NULL ? e->extra : "",
			    e->extra != NULL ? ")" : ""
			);

			e = e->next;
		}
	}
}
