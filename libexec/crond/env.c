#if !defined(lint) && !defined(LINT)
static char rcsid[] = "$Header: /a/cvs/386BSD/src/libexec/crond/env.c,v 1.1.1.1 1993/06/12 14:55:03 rgrimes Exp $";
#endif

/* Copyright 1988,1990 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie, 329 Noe Street, San Francisco, CA, 94114, (415) 864-7013,
 * paul@vixie.sf.ca.us || {hoptoad,pacbell,decwrl,crash}!vixie!paul
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00131
 * --------------------         -----   ----------------------
 *
 * 06 Apr 93	Adam Glass	Fixes so it compiles quitely
 *
 */


#include "cron.h"


char **
env_init()
{
	extern char	*malloc();
	register char	**p = (char **) malloc(sizeof(char **));

	p[0] = NULL;
	return p;
}


char **
env_set(envp, envstr)
	char	**envp;
	char	*envstr;
{
	extern char	*realloc(), *savestr();
	register int	count, found;
	register char	**p;

	/*
	 * count the number of elements, including the null pointer;
	 * also set 'found' to -1 or index of entry if already in here.
	 */
	found = -1;
	for (count = 0;  envp[count] != NULL;  count++)
	{
		if (!strcmp_until(envp[count], envstr, '='))
			found = count;
	}
	count++;		/* for the null pointer
				 */

	if (found != -1)
	{
		/*
		 * it exists already, so just free the existing setting,
		 * save our new one there, and return the existing array.
		 */
		free(envp[found]);
		envp[found] = savestr(envstr);
		return envp;
	}

	/*
	 * it doesn't exist yet, so resize the array, move null pointer over
	 * one, save our string over the old null pointer, and return resized
	 * array.
	 */
	p = (char **) realloc(
			(char *)   envp,
			(unsigned) ((count+1) * sizeof(char **))
	);
	p[count] = p[count-1];
	p[count-1] = savestr(envstr);
	return p;
}


int
load_env(envstr, f)
	char	*envstr;
	FILE	*f;
{
	/* return	ERR = end of file
	 *		FALSE = not an env setting (file was repositioned)
	 *		TRUE = was an env setting
	 */
	long	filepos;
	int	fileline;
	char	name[MAX_TEMPSTR], val[MAX_ENVSTR];
	int	fields, strdtb();
	void	skip_comments();

	filepos = ftell(f);
	fileline = LineNumber;
	skip_comments(f);
	if (EOF == get_string(envstr, MAX_ENVSTR, f, "\n"))
		return ERR;

	Debug(DPARS, ("load_env, read <%s>\n", envstr))

	name[0] = val[0] = '\0';
	fields = sscanf(envstr, "%[^ =] = %[^\n#]", name, val);
	if (fields != 2)
	{
		Debug(DPARS, ("load_env, not 2 fields (%d)\n", fields))
		fseek(f, filepos, 0);
		Set_LineNum(fileline);
		return FALSE;
	}

	/* 2 fields from scanf; looks like an env setting
	 */

	/*
	 * process value string
	 */
	{
		int	len = strdtb(val);

		if (len >= 2)
			if (val[0] == '\'' || val[0] == '"')
				if (val[len-1] == val[0])
				{
					val[len-1] = '\0';
					(void) strcpy(val, val+1);
				}
	}

	(void) sprintf(envstr, "%s=%s", name, val);
	Debug(DPARS, ("load_env, <%s> <%s> -> <%s>\n", name, val, envstr))
	return TRUE;
}


char *
env_get(name, envp)
	char	*name;
	char	**envp;
{
	char	*index();

	for (;  *envp;  envp++)
		if (!strcmp_until(*envp, name, '='))
			return index(*envp, '=') + 1;
	return NULL;
}
