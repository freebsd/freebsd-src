/*
 * Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Id: debug.c,v 1.28 2001/09/25 19:57:05 gshapiro Exp $")

/*
**  libsm debugging and tracing
**  For documentation, see debug.html.
*/

#include <ctype.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sm/io.h>
#include <sm/assert.h>
#include <sm/conf.h>
#include <sm/debug.h>
#include <sm/string.h>
#include <sm/varargs.h>
#include <sm/heap.h>

/*
**  Abstractions for printing trace messages.
*/

/*
**  The output file to which trace output is directed.
**  There is a controversy over whether this variable
**  should be process global or thread local.
**  To make the interface more abstract, we've hidden the
**  variable behind access functions.
*/

static SM_FILE_T *SmDebugOutput = smioout;

/*
**  SM_DEBUG_FILE -- Returns current debug file pointer.
**
**	Parameters:
**		none.
**
**	Returns:
**		current debug file pointer.
*/

SM_FILE_T *
sm_debug_file()
{
	return SmDebugOutput;
}

/*
**  SM_DEBUG_SETFILE -- Sets debug file pointer.
**
**	Parameters:
**		fp -- new debug file pointer.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Sets SmDebugOutput.
*/

void
sm_debug_setfile(fp)
	SM_FILE_T *fp;
{
	SmDebugOutput = fp;
}

/*
**  SM_DPRINTF -- printf() for debug output.
**
**	Parameters:
**		fmt -- format for printf()
**
**	Returns:
**		none.
*/

void
#if SM_VA_STD
sm_dprintf(char *fmt, ...)
#else /* SM_VA_STD */
sm_dprintf(fmt, va_alist)
	char *fmt;
	va_dcl
#endif /* SM_VA_STD */
{
	SM_VA_LOCAL_DECL

	if (SmDebugOutput == NULL)
		return;
	SM_VA_START(ap, fmt);
	sm_io_vfprintf(SmDebugOutput, SmDebugOutput->f_timeout, fmt, ap);
	SM_VA_END(ap);
}

/*
**  SM_DFLUSH -- Flush debug output.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
*/

void
sm_dflush()
{
	sm_io_flush(SmDebugOutput, SM_TIME_DEFAULT);
}

/*
**  This is the internal database of debug settings.
**  The semantics of looking up a setting in the settings database
**  are that the *last* setting specified in a -d option on the sendmail
**  command line that matches a given SM_DEBUG structure is the one that is
**  used.  That is necessary to conform to the existing semantics of
**  the sendmail -d option.  We store the settings as a linked list in
**  reverse order, so when we do a lookup, we take the *first* entry
**  that matches.
*/

typedef struct sm_debug_setting SM_DEBUG_SETTING_T;
struct sm_debug_setting
{
	const char		*ds_pattern;
	unsigned int		ds_level;
	SM_DEBUG_SETTING_T	*ds_next;
};
SM_DEBUG_SETTING_T *SmDebugSettings = NULL;

/*
**  We keep a linked list of SM_DEBUG structures that have been initialized,
**  for use by sm_debug_reset.
*/

SM_DEBUG_T *SmDebugInitialized = NULL;

const char SmDebugMagic[] = "sm_debug";

/*
**  SM_DEBUG_RESET -- Reset SM_DEBUG structures.
**
**	Reset all SM_DEBUG structures back to the uninitialized state.
**	This is used by sm_debug_addsetting to ensure that references to
**	SM_DEBUG structures that occur before sendmail processes its -d flags
**	do not cause those structures to be permanently forced to level 0.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
*/

void
sm_debug_reset()
{
	SM_DEBUG_T *debug;

	for (debug = SmDebugInitialized;
	     debug != NULL;
	     debug = debug->debug_next)
	{
		debug->debug_level = SM_DEBUG_UNKNOWN;
	}
	SmDebugInitialized = NULL;
}

/*
**  SM_DEBUG_ADDSETTING_X -- add an entry to the database of debug settings
**
**	Parameters:
**		pattern -- a shell-style glob pattern (see sm_match).
**			WARNING: the storage for 'pattern' will be owned by
**			the debug package, so it should either be a string
**			literal or the result of a call to sm_strdup_x.
**		level -- a non-negative integer.
**
**	Returns:
**		none.
**
**	Exceptions:
**		F:sm_heap -- out of memory
*/

void
sm_debug_addsetting_x(pattern, level)
	const char *pattern;
	int level;
{
	SM_DEBUG_SETTING_T *s;

	SM_REQUIRE(pattern != NULL);
	SM_REQUIRE(level >= 0);
	s = sm_malloc_x(sizeof(SM_DEBUG_SETTING_T));
	s->ds_pattern = pattern;
	s->ds_level = (unsigned int) level;
	s->ds_next = SmDebugSettings;
	SmDebugSettings = s;
	sm_debug_reset();
}

/*
**  PARSE_NAMED_SETTING_X -- process a symbolic debug setting
**
**	Parameters:
**		s -- Points to a non-empty \0 or , terminated string,
**		     of which the initial character is not a digit.
**
**	Returns:
**		pointer to terminating \0 or , character.
**
**	Exceptions:
**		F:sm.heap -- out of memory.
**
**	Side Effects:
**		adds the setting to the database.
*/

static const char *
parse_named_setting_x(s)
	register const char *s;
{
	const char *pat, *endpat;
	int level;

	pat = s;
	while (*s != '\0' && *s != ',' && *s != '.')
		++s;
	endpat = s;
	if (*s == '.')
	{
		++s;
		level = 0;
		while (isascii(*s) && isdigit(*s))
		{
			level = level * 10 + (*s - '0');
			++s;
		}
		if (level < 0)
			level = 0;
	}
	else
		level = 1;

	sm_debug_addsetting_x(sm_strndup_x(pat, endpat - pat), level);

	/* skip trailing junk */
	while (*s != '\0' && *s != ',')
		++s;

	return s;
}

/*
**  SM_DEBUG_ADDSETTINGS_X -- process a list of debug options
**
**	Parameters:
**		s -- a list of debug settings, eg the argument to the
**		     sendmail -d option.
**
**		The syntax of the string s is as follows:
**
**		<settings> ::= <setting> | <settings> "," <setting>
**		<setting> ::= <categories> | <categories> "." <level>
**		<categories> ::= [a-zA-Z_*?][a-zA-Z0-9_*?]*
**
**		However, note that we skip over anything we don't
**		understand, rather than report an error.
**
**	Returns:
**		none.
**
**	Exceptions:
**		F:sm.heap -- out of memory
**
**	Side Effects:
**		updates the database of debug settings.
*/

void
sm_debug_addsettings_x(s)
	register const char *s;
{
	for (;;)
	{
		if (*s == '\0')
			return;
		if (*s == ',')
		{
			++s;
			continue;
		}
		s = parse_named_setting_x(s);
	}
}

/*
**  SM_DEBUG_LOADLEVEL -- Get activation level of the specified debug object.
**
**	Parameters:
**		debug -- debug object.
**
**	Returns:
**		Activation level of the specified debug object.
**
**	Side Effects:
**		Ensures that the debug object is initialized.
*/

int
sm_debug_loadlevel(debug)
	SM_DEBUG_T *debug;
{
	if (debug->debug_level == SM_DEBUG_UNKNOWN)
	{
		SM_DEBUG_SETTING_T *s;

		for (s = SmDebugSettings; s != NULL; s = s->ds_next)
		{
			if (sm_match(debug->debug_name, s->ds_pattern))
			{
				debug->debug_level = s->ds_level;
				goto initialized;
			}
		}
		debug->debug_level = 0;
	initialized:
		debug->debug_next = SmDebugInitialized;
		SmDebugInitialized = debug;
	}
	return (int) debug->debug_level;
}

/*
**  SM_DEBUG_LOADACTIVE -- Activation level reached?
**
**	Parameters:
**		debug -- debug object.
**		level -- level to check.
**
**	Returns:
**		true iff the activation level of the specified debug
**			object >= level.
**
**	Side Effects:
**		Ensures that the debug object is initialized.
*/

bool
sm_debug_loadactive(debug, level)
	SM_DEBUG_T *debug;
	int level;
{
	return sm_debug_loadlevel(debug) >= level;
}
