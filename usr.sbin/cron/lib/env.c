/* Copyright 1988,1990,1993,1994 by Paul Vixie
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
 * Paul Vixie          <paul@vix.com>          uunet!decwrl!vixie!paul
 */

#if !defined(lint) && !defined(LINT)
static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/cron/lib/env.c,v 1.7.2.1 2000/07/01 10:35:07 ps Exp $";
#endif


#include "cron.h"


char **
env_init()
{
	register char	**p = (char **) malloc(sizeof(char *));

	if (p)
		p[0] = NULL;
	return (p);
}


void
env_free(envp)
	char	**envp;
{
	char	**p;

	for (p = envp;  *p;  p++)
		free(*p);
	free(envp);
}


char **
env_copy(envp)
	register char	**envp;
{
	register int	count, i;
	register char	**p;

	for (count = 0;  envp[count] != NULL;  count++)
		;
	p = (char **) malloc((count+1) * sizeof(char *)); /* 1 for the NULL */
	if (p == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	for (i = 0;  i < count;  i++)
		if ((p[i] = strdup(envp[i])) == NULL) {
			while (--i >= 0)
				(void) free(p[i]);
			free(p);
			errno = ENOMEM;
			return NULL;
		}
	p[count] = NULL;
	return (p);
}


char **
env_set(envp, envstr)
	char	**envp;
	char	*envstr;
{
	register int	count, found;
	register char	**p;
	char		*q;

	/*
	 * count the number of elements, including the null pointer;
	 * also set 'found' to -1 or index of entry if already in here.
	 */
	found = -1;
	for (count = 0;  envp[count] != NULL;  count++) {
		if (!strcmp_until(envp[count], envstr, '='))
			found = count;
	}
	count++;	/* for the NULL */

	if (found != -1) {
		/*
		 * it exists already, so just free the existing setting,
		 * save our new one there, and return the existing array.
		 */
		q = envp[found];
		if ((envp[found] = strdup(envstr)) == NULL) {
			envp[found] = q;
			/* XXX env_free(envp); */
			errno = ENOMEM;
			return NULL;
		}
		free(q);
		return (envp);
	}

	/*
	 * it doesn't exist yet, so resize the array, move null pointer over
	 * one, save our string over the old null pointer, and return resized
	 * array.
	 */
	p = (char **) realloc((void *) envp,
			      (unsigned) ((count+1) * sizeof(char *)));
	if (p == NULL) 	{
		/* XXX env_free(envp); */
		errno = ENOMEM;
		return NULL;
	}
	p[count] = p[count-1];
	if ((p[count-1] = strdup(envstr)) == NULL) {
		env_free(p);
		errno = ENOMEM;
		return NULL;
	}
	return (p);
}


/* return	ERR = end of file
 *		FALSE = not an env setting (file was repositioned)
 *		TRUE = was an env setting
 */
int
load_env(envstr, f)
	char	*envstr;
	FILE	*f;
{
	long	filepos;
	int	fileline;
	char	name[MAX_ENVSTR], val[MAX_ENVSTR];
	int	fields;

	filepos = ftell(f);
	fileline = LineNumber;
	skip_comments(f);
	if (EOF == get_string(envstr, MAX_ENVSTR, f, "\n"))
		return (ERR);

	Debug(DPARS, ("load_env, read <%s>\n", envstr))

	name[0] = val[0] = '\0';
	fields = sscanf(envstr, "%[^ =] = %[^\n#]", name, val);
	if (fields != 2) {
		Debug(DPARS, ("load_env, not 2 fields (%d)\n", fields))
		fseek(f, filepos, 0);
		Set_LineNum(fileline);
		return (FALSE);
	}

	/* 2 fields from scanf; looks like an env setting
	 */

	/*
	 * process value string
	 */
	/*local*/{
		int	len = strdtb(val);

		if (len >= 2) {
			if (val[0] == '\'' || val[0] == '"') {
				if (val[len-1] == val[0]) {
					val[len-1] = '\0';
					(void) strcpy(val, val+1);
				}
			}
		}
	}

	if (strlen(name) + 1 + strlen(val) >= MAX_ENVSTR-1)
		return (FALSE);
	(void) sprintf(envstr, "%s=%s", name, val);
	Debug(DPARS, ("load_env, <%s> <%s> -> <%s>\n", name, val, envstr))
	return (TRUE);
}


char *
env_get(name, envp)
	register char	*name;
	register char	**envp;
{
	register int	len = strlen(name);
	register char	*p, *q;

	while ((p = *envp++)) {
		if (!(q = strchr(p, '=')))
			continue;
		if ((q - p) == len && !strncmp(p, name, len))
			return (q+1);
	}
	return (NULL);
}
