/*-
 * Copyright (c) 1999 Timmy M. Vanderhoek
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

/*
 * Expansion of macros.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "less.h"


/*
 * Used to construct tables of macros.  Each macro string expands to command.
 * A number N is associated with each execution of a macro.  The command
 * "set number <N>" will be done before the expansion.  The end of the table is 
 * specified by mactabsize.  A NULL entry for command denotes a macro that
 * has been marked deleted for some reason.  As of this writing, there is no
 * code that actually deletes a macro...
 */
struct macro {
	char *string;    /* characters typed to activate macro */
	char *command;   /* command resulting after the macro is activated */
	long defnumber;  /* default value of the N number */
	int flags;       /* only holds STICKYNUMB for now... */
};
/* (struct macro) ->flags  */
#define NOFLAGS 0
#define STICKYNUMB 1  /* Set defnumber to current number, if current number */

/*
 * The macro table.
 */
struct macro *mactab = NULL;
int mactabsize = 0;

static enum runmacro runmacro_();
static struct macro *matchmac();


/*
 * XXX Everything's really just a macro until resolved as a quantum wave
 * probability distribution.
 */


/*
 * Attempts to run the appropriate macro.  Returns 0, or OK, if the macro
 * was succesfully run.  Returns BADMACRO and sets erreur if something is
 * horribly wrong with the macro.  Returns NOMACRO if the macro has no valid
 * expansion.  BADMACRO and NOMACRO are almost the same.  Returns BADCOMMAND
 * and leaves erreur set (hopefully it was set when runmacro() tried to execute
 * the command associated with the macro) if the command associated with
 * the macro was unsuccessful.  Returns TOOMACRO if the macro appears to be
 * incomplete (ie. the user has not finished typing it in yet).  The erreur
 * is not set in this case.
 *
 * XXX There's no good reason not to just disallow badmacros from within
 *     setmacro()  ...  It's not clear what the author was thinking at the time.
 */
enum runmacro
runmacro(macro, number, anyN)
	const char *macro;  /* the macro string to try and expand */
	long number;  /* the number N associated with this execution */
	int anyN;  /* FALSE is we should use the default N associated with
	            * the macro.  TRUE if we should use the number argument. */
{
	struct macro *cur, *matched;
	int match, yetmatch;
	int s;

	if (!mactab) {
		/* Should only happen with really sucky default rc files... */
		SETERR (E_CANTXPND);
		return NOMACRO;
	}

	match = yetmatch = 0;
	for (cur = mactab, s = mactabsize; s; cur++, s--) {
		if (!cur->command)
			continue;  /* deleted macro */
		if (!strcmp(cur->string, macro))
			matched = cur, match++;
		else if (!strncmp(cur->string, macro, strlen(macro)))
			yetmatch++;
	}

	if (match == 1) {
		if (yetmatch) {
			SETERR (E_AMBIG);
			return BADMACRO;
		}

		/* XXX it's not clear how to handle error when setting
		 * the number N --- this is a deficiency in the style of error-
		 * reporting suggested in command.c and less.h.  Could have
		 * setvar() guarantee success when setting "number".  A failure
		 * must not become fatal or it becomes impossible to do
		 * any commands at all. */
		if (anyN) {
			if (matched->flags & STICKYNUMB)
				matched->defnumber = number;
			(void) setvari("number", number);
		} else
			(void) setvari("number", matched->defnumber);
		clear_error();

		if (command(matched->command))
			return BADCOMMAND;
		return OK;
	}
	if (match > 1) {
		SETERR (E_AMBIG);
		return BADMACRO;
	}
	if (!match && !yetmatch) {
		SETERR (E_CANTXPND);
		return NOMACRO;
	}
	assert(yetmatch);
	return TOOMACRO;
}

/*
 * Associates a macro with a given command.  Returns -1 if it was unable to
 * set the macro.  Errors associated with setting a macro may be caught
 * either in this function, setmacro(), or in runmacro().  Both macro and
 * command are strcpy()'d into their own space.
 */
int
setmacro(macro, command)
	const char *macro;
	const char *command;
{
	struct macro *cur, *new = NULL;
	char *new_mac, *new_com;
	int s;

	assert (macro); assert (command);

	/* First, check for any existing macro matches in the custom table */
	s = mactabsize;
	for (cur = mactab; s; cur++, s--) {
		if (!cur->command) {
			/* Hmm...  A deleted macro in the table */
			new = cur;
			continue;
		}
		if (!strcmp(cur->string, macro)) {
			/*
			 * An exact match to the new macro already exists.
			 * Calling realloc() on cur->string and cur->command
			 * without risking being left in bad state is tricky.
			 * Just do it the slow but sure way...
			 */
			new = cur;
			break;
		}
	}

	/*
	 * Do the allocations here so that we can maintain consistent state
	 * even if realloc() fails when we try to expand the table (suppose
	 * the table gets expanded but the next malloc to get space for the
	 * macro fails).
	 */
	if (!FMALLOC(strlen(macro) + 1, new_mac))
		return -1;
	if (!FMALLOC(strlen(command) + 1, new_com))
		return -1;

	if (!new) {
		/* Extend the command table by one record */
		struct macro *t = realloc(mactab, (mactabsize + 1) *
		    sizeof(struct macro));
		if (!t) {
			/* The old mactab is still valid.  Just back out. */
			free(new_mac), free(new_com);
			SETERR (E_MALLOC);
			return -1;
		} else
			mactab = t;
		new = &mactab[mactabsize];
		mactabsize++;
		new->string = new->command = NULL;
	}

	if (new->string) free(new->string);
	if (new->command) free(new->command);
	new->string = new_mac;
	new->command = new_com;
	strcpy(new->string, macro);
	strcpy(new->command, command);

	return 0;
}

/* 
 * Set the sticky tag on a macro.  Returns -1 on failure, 0 on success.
 */
int
stickymac(macro, state)
	const char *macro;
	int state;  /* set it to TRUE or set it to FALSE */
{
	struct macro *m = matchmac(macro);
	if (!m)
		return -1;

	if (state)
		m->flags |= STICKYNUMB;
	else
		m->flags &= ~STICKYNUMB;

	return 0;
}

/*
 * Set the default number of a macro.  Returns -1 on failure, 0 on success.
 */
int
setmacnumb(macro, N)
	const char *macro;
	long N;  /* The default number */
{
	struct macro *m = matchmac(macro);
	if (!m)
		return -1;

	m->defnumber = N;
	return 0;
}

/*
 * Tries to find a struct macro matching "macro".  Returns NULL if an exact
 * match could not be found (eg. ambiguous macro, no macro, etc).
 */
static struct macro *
matchmac(macro)
	const char *macro;
{
	struct macro *retr, *cur;
	int s;

	retr = NULL;
	for (cur = mactab, s = mactabsize; s; cur++, s--) {
		if (!cur->command)
			continue;
		if (!strcmp(cur->string, macro)) {
			if (retr) {
				SETERR (E_AMBIG);
				return NULL;  /* matched twice! */
			} else
				retr = cur;
		} else if (!strncmp(cur->string, macro, strlen(macro))) {
			SETERR (E_AMBIG);
			return NULL;  /* ambiguous macro! */
		}
	}
	return retr;
}
