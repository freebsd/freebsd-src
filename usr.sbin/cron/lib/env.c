/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * All rights reserved
 */

/*
 * Copyright (c) 1997 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if !defined(lint) && !defined(LINT)
static const char rcsid[] = "$Id: env.c,v 1.3 1998/08/14 00:32:39 vixie Exp $";
#endif


#include "cron.h"


char **
env_init(void)
{
	char	**p = (char **) malloc(sizeof(char **));

	if (p)
		p[0] = NULL;
	return (p);
}


void
env_free(char **envp)
{
	char	**p;

	if ((p = envp))
	    for (;  *p;  p++)
		free(*p);
	free(envp);
}


char **
env_copy(char **envp)
{
	int	count, i;
	char	**p;

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
env_set(char **envp, char *envstr)
{
	int	count, found;
	char	**p;
	char	*q;

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
load_env(char *envstr, FILE *f)
{
	long	filepos;
	int	fileline;
	char	name[MAX_ENVSTR], val[MAX_ENVSTR];
	char	quotechar, *c, *str;
	int	state;

	/* The following states are traversed in order: */
#define NAMEI	0	/* First char of NAME, may be quote */
#define NAME	1	/* Subsequent chars of NAME */
#define EQ1	2	/* After end of name, looking for '=' sign */
#define EQ2	3	/* After '=', skipping whitespace */
#define VALUEI	4	/* First char of VALUE, may be quote */
#define VALUE	5	/* Subsequent chars of VALUE */
#define FINI	6	/* All done, skipping trailing whitespace */
#define ERROR	7	/* Error */

	filepos = ftell(f);
	fileline = LineNumber;
	skip_comments(f);
	if (EOF == get_string(envstr, MAX_ENVSTR, f, "\n"))
		return (ERR);

	Debug(DPARS, ("load_env, read <%s>\n", envstr));

	bzero (name, sizeof name);
	bzero (val, sizeof val);
	str = name;
	state = NAMEI;
	quotechar = '\0';
	c = envstr;
	while (state != ERROR && *c) {
		switch (state) {
		case NAMEI:
		case VALUEI:
			if (*c == '\'' || *c == '"')
				quotechar = *c++;
			++state;
			/* FALLTHROUGH */
		case NAME:
		case VALUE:
			if (quotechar) {
				if (*c == quotechar) {
					state++;
					c++;
					break;
				}
				if (state == NAME && *c == '=') {
					state = ERROR;
					break;
				}
			} else {
				if (state == NAME) {
					if (isspace (*c)) {
						c++;
						state++;
						break;
					}
					if (*c == '=') {
						state++;
						break;
					}
				}
			}
			*str++ = *c++;
			break;

		case EQ1:
			if (*c == '=') {
				state++;
				str = val;
				quotechar = '\0';
			} else {
				if (!isspace (*c))
					state = ERROR;
			}
			c++;
			break;
		case EQ2:
		case FINI:
			if (isspace (*c))
				c++;
			else
				state++;
			break;
		}
	}
	if (state != FINI && !(state == VALUE && !quotechar)) {
		Debug(DPARS, ("load_env, parse error, state = %d\n", state))
		fseek(f, filepos, 0);
		Set_LineNum(fileline);
		return (FALSE);
	}
	if (state == VALUE) {
		/* End of unquoted value: trim trailing whitespace */
		c = val + strlen (val);
		while (c > val && isspace (*(c - 1)))
			*(--c) = '\0';
	}

	/* 2 fields from parser; looks like an env setting */

	if (snprintf(envstr, MAX_ENVSTR, "%s=%s", name, val) >= MAX_ENVSTR)
		return (FALSE);
	Debug(DPARS, ("load_env, <%s> <%s> -> <%s>\n", name, val, envstr))
	return (TRUE);
}


char *
env_get(char *name, char **envp)
{
	int	len = strlen(name);
	char	*p, *q;

	while ((p = *envp++) != NULL) {
		if (!(q = strchr(p, '=')))
			continue;
		if ((q - p) == len && !strncmp(p, name, len))
			return (q+1);
	}
	return (NULL);
}
