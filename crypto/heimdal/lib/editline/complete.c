/*  Copyright 1992 Simmule Turner and Rich Salz.  All rights reserved. 
 *
 *  This software is not subject to any license of the American Telephone 
 *  and Telegraph Company or of the Regents of the University of California. 
 *
 *  Permission is granted to anyone to use this software for any purpose on
 *  any computer system, and to alter it and redistribute it freely, subject
 *  to the following restrictions:
 *  1. The authors are not responsible for the consequences of use of this
 *     software, no matter how awful, even if they arise from flaws in it.
 *  2. The origin of this software must not be misrepresented, either by
 *     explicit claim or by omission.  Since few users ever read sources,
 *     credits must appear in the documentation.
 *  3. Altered versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.  Since few users
 *     ever read sources, credits must appear in the documentation.
 *  4. This notice may not be removed or altered.
 */

/*
**  History and file completion functions for editline library.
*/
#include <config.h>
#include "editline.h"

RCSID("$Id: complete.c,v 1.5 1999/04/10 21:01:16 joda Exp $");

/*
**  strcmp-like sorting predicate for qsort.
*/
static int
compare(const void *p1, const void *p2)
{
    const char	**v1;
    const char	**v2;
    
    v1 = (const char **)p1;
    v2 = (const char **)p2;
    return strcmp(*v1, *v2);
}

/*
**  Fill in *avp with an array of names that match file, up to its length.
**  Ignore . and .. .
*/
static int
FindMatches(char *dir, char *file, char ***avp)
{
    char	**av;
    char	**new;
    char	*p;
    DIR		*dp;
    DIRENTRY	*ep;
    size_t	ac;
    size_t	len;

    if ((dp = opendir(dir)) == NULL)
	return 0;

    av = NULL;
    ac = 0;
    len = strlen(file);
    while ((ep = readdir(dp)) != NULL) {
	p = ep->d_name;
	if (p[0] == '.' && (p[1] == '\0' || (p[1] == '.' && p[2] == '\0')))
	    continue;
	if (len && strncmp(p, file, len) != 0)
	    continue;

	if ((ac % MEM_INC) == 0) {
	    if ((new = malloc(sizeof(char*) * (ac + MEM_INC))) == NULL)
		break;
	    if (ac) {
		memcpy(new, av, ac * sizeof (char **));
		free(av);
	    }
	    *avp = av = new;
	}

	if ((av[ac] = strdup(p)) == NULL) {
	    if (ac == 0)
		free(av);
	    break;
	}
	ac++;
    }

    /* Clean up and return. */
    (void)closedir(dp);
    if (ac)
	qsort(av, ac, sizeof (char **), compare);
    return ac;
}

/*
**  Split a pathname into allocated directory and trailing filename parts.
*/
static int SplitPath(char *path, char **dirpart, char **filepart)
{
    static char	DOT[] = ".";
    char	*dpart;
    char	*fpart;

    if ((fpart = strrchr(path, '/')) == NULL) {
	if ((dpart = strdup(DOT)) == NULL)
	    return -1;
	if ((fpart = strdup(path)) == NULL) {
	    free(dpart);
	    return -1;
	}
    }
    else {
	if ((dpart = strdup(path)) == NULL)
	    return -1;
	dpart[fpart - path] = '\0';
	if ((fpart = strdup(++fpart)) == NULL) {
	    free(dpart);
	    return -1;
	}
    }
    *dirpart = dpart;
    *filepart = fpart;
    return 0;
}

/*
**  Attempt to complete the pathname, returning an allocated copy.
**  Fill in *unique if we completed it, or set it to 0 if ambiguous.
*/

static char *
rl_complete_filename(char *pathname, int *unique)
{
    char	**av;
    char	*new;
    char	*p;
    size_t	ac;
    size_t	end;
    size_t	i;
    size_t	j;
    size_t	len;
    char *s;
    
    ac = rl_list_possib(pathname, &av);
    if(ac == 0)
	return NULL;

    s = strrchr(pathname, '/');
    if(s == NULL)
	len = strlen(pathname);
    else
	len = strlen(s + 1);

    p = NULL;
    if (ac == 1) {
	/* Exactly one match -- finish it off. */
	*unique = 1;
	j = strlen(av[0]) - len + 2;
	if ((p = malloc(j + 1)) != NULL) {
	    memcpy(p, av[0] + len, j);
	    asprintf(&new, "%s%s", pathname, p);
	    if(new != NULL) {
		rl_add_slash(new, p);
		free(new);
	    }
	}
    }
    else {
	*unique = 0;
	if (len) {
	    /* Find largest matching substring. */
	    for (i = len, end = strlen(av[0]); i < end; i++)
		for (j = 1; j < ac; j++)
		    if (av[0][i] != av[j][i])
			goto breakout;
  breakout:
	    if (i > len) {
		j = i - len + 1;
		if ((p = malloc(j)) != NULL) {
		    memcpy(p, av[0] + len, j);
		    p[j - 1] = '\0';
		}
	    }
	}
    }

    /* Clean up and return. */
    for (i = 0; i < ac; i++)
	free(av[i]);
    free(av);
    return p;
}

static rl_complete_func_t complete_func = rl_complete_filename;

char *
rl_complete(char *pathname, int *unique)
{
    return (*complete_func)(pathname, unique);
}

rl_complete_func_t
rl_set_complete_func(rl_complete_func_t func)
{
    rl_complete_func_t old = complete_func;
    complete_func = func;
    return old;
}


/*
**  Return all possible completions.
*/
static int
rl_list_possib_filename(char *pathname, char ***avp)
{
    char	*dir;
    char	*file;
    int		ac;

    if (SplitPath(pathname, &dir, &file) < 0)
	return 0;
    ac = FindMatches(dir, file, avp);
    free(dir);
    free(file);
    return ac;
}

static rl_list_possib_func_t list_possib_func = rl_list_possib_filename;

int
rl_list_possib(char *pathname, char ***avp)
{
    return (*list_possib_func)(pathname, avp);
}

rl_list_possib_func_t
rl_set_list_possib_func(rl_list_possib_func_t func)
{
    rl_list_possib_func_t old = list_possib_func;
    list_possib_func = func;
    return old;
}
