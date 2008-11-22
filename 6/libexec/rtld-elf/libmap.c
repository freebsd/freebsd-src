/*
 * $FreeBSD$
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/param.h>

#include "debug.h"
#include "rtld.h"
#include "libmap.h"

#ifndef _PATH_LIBMAP_CONF
#define	_PATH_LIBMAP_CONF	"/etc/libmap.conf"
#endif

#ifdef COMPAT_32BIT
#undef _PATH_LIBMAP_CONF
#define	_PATH_LIBMAP_CONF	"/etc/libmap32.conf"
#endif

TAILQ_HEAD(lm_list, lm);
struct lm {
	char *f;
	char *t;

	TAILQ_ENTRY(lm)	lm_link;
};

TAILQ_HEAD(lmp_list, lmp) lmp_head = TAILQ_HEAD_INITIALIZER(lmp_head);
struct lmp {
	char *p;
	enum { T_EXACT=0, T_BASENAME, T_DIRECTORY } type;
	struct lm_list lml;
	TAILQ_ENTRY(lmp) lmp_link;
};

static int	lm_count;

static void		lmc_parse	(FILE *);
static void		lm_add		(const char *, const char *, const char *);
static void		lm_free		(struct lm_list *);
static char *		lml_find	(struct lm_list *, const char *);
static struct lm_list *	lmp_find	(const char *);
static struct lm_list *	lmp_init	(char *);
static const char * quickbasename	(const char *);
static int	readstrfn	(void * cookie, char *buf, int len);
static int	closestrfn	(void * cookie);

#define	iseol(c)	(((c) == '#') || ((c) == '\0') || \
			 ((c) == '\n') || ((c) == '\r'))

int
lm_init (char *libmap_override)
{
	FILE	*fp;

	dbg("%s(\"%s\")", __func__, libmap_override);

	TAILQ_INIT(&lmp_head);

	fp = fopen(_PATH_LIBMAP_CONF, "r");
	if (fp) {
		lmc_parse(fp);
		fclose(fp);
	}

	if (libmap_override) {
		char	*p;
		/* do some character replacement to make $LIBMAP look like a
		   text file, then "open" it with funopen */
		libmap_override = xstrdup(libmap_override);

		for (p = libmap_override; *p; p++) {
			switch (*p) {
				case '=':
					*p = ' '; break;
				case ',':
					*p = '\n'; break;
			}
		}
		fp = funopen(libmap_override, readstrfn, NULL, NULL, closestrfn);
		if (fp) {
			lmc_parse(fp);
			fclose(fp);
		}
	}

	return (lm_count == 0);
}

static void
lmc_parse (FILE *fp)
{
	char	*cp;
	char	*f, *t, *c, *p;
	char	prog[MAXPATHLEN];
	char	line[MAXPATHLEN + 2];

	dbg("%s(%p)", __func__, fp);
	
	p = NULL;
	while ((cp = fgets(line, MAXPATHLEN + 1, fp)) != NULL) {
		t = f = c = NULL;

		/* Skip over leading space */
		while (isspace(*cp)) cp++;

		/* Found a comment or EOL */
		if (iseol(*cp)) continue;

		/* Found a constraint selector */
		if (*cp == '[') {
			cp++;

			/* Skip leading space */
			while (isspace(*cp)) cp++;

			/* Found comment, EOL or end of selector */
			if  (iseol(*cp) || *cp == ']')
				continue;

			c = cp++;
			/* Skip to end of word */
			while (!isspace(*cp) && !iseol(*cp) && *cp != ']')
				cp++;

			/* Skip and zero out trailing space */
			while (isspace(*cp)) *cp++ = '\0';

			/* Check if there is a closing brace */
			if (*cp != ']') continue;

			/* Terminate string if there was no trailing space */
			*cp++ = '\0';

			/*
			 * There should be nothing except whitespace or comment
			  from this point to the end of the line.
			 */
			while(isspace(*cp)) cp++;
			if (!iseol(*cp)) continue;

			strcpy(prog, c);
			p = prog;
			continue;
		}

		/* Parse the 'from' candidate. */
		f = cp++;
		while (!isspace(*cp) && !iseol(*cp)) cp++;

		/* Skip and zero out the trailing whitespace */
		while (isspace(*cp)) *cp++ = '\0';

		/* Found a comment or EOL */
		if (iseol(*cp)) continue;

		/* Parse 'to' mapping */
		t = cp++;
		while (!isspace(*cp) && !iseol(*cp)) cp++;

		/* Skip and zero out the trailing whitespace */
		while (isspace(*cp)) *cp++ = '\0';

		/* Should be no extra tokens at this point */
		if (!iseol(*cp)) continue;

		*cp = '\0';
		lm_add(p, f, t);
	}
}

static void
lm_free (struct lm_list *lml)
{
	struct lm *lm;

	dbg("%s(%p)", __func__, lml);

	while (!TAILQ_EMPTY(lml)) {
		lm = TAILQ_FIRST(lml);
		TAILQ_REMOVE(lml, lm, lm_link);
		free(lm->f);
		free(lm->t);
		free(lm);
	}
	return;
}

void
lm_fini (void)
{
	struct lmp *lmp;

	dbg("%s()", __func__);

	while (!TAILQ_EMPTY(&lmp_head)) {
		lmp = TAILQ_FIRST(&lmp_head);
		TAILQ_REMOVE(&lmp_head, lmp, lmp_link);
		free(lmp->p);
		lm_free(&lmp->lml);
		free(lmp);
	}
	return;
}

static void
lm_add (const char *p, const char *f, const char *t)
{
	struct lm_list *lml;
	struct lm *lm;

	if (p == NULL)
		p = "$DEFAULT$";

	dbg("%s(\"%s\", \"%s\", \"%s\")", __func__, p, f, t);

	if ((lml = lmp_find(p)) == NULL)
		lml = lmp_init(xstrdup(p));

	lm = xmalloc(sizeof(struct lm));
	lm->f = xstrdup(f);
	lm->t = xstrdup(t);
	TAILQ_INSERT_HEAD(lml, lm, lm_link);
	lm_count++;
}

char *
lm_find (const char *p, const char *f)
{
	struct lm_list *lml;
	char *t;

	dbg("%s(\"%s\", \"%s\")", __func__, p, f);

	if (p != NULL && (lml = lmp_find(p)) != NULL) {
		t = lml_find(lml, f);
		if (t != NULL) {
			/*
			 * Add a global mapping if we have
			 * a successful constrained match.
			 */
			lm_add(NULL, f, t);
			return (t);
		}
	}
	lml = lmp_find("$DEFAULT$");
	if (lml != NULL)
		return (lml_find(lml, f));
	else
		return (NULL);
}

/* Given a libmap translation list and a library name, return the
   replacement library, or NULL */
#ifdef COMPAT_32BIT
char *
lm_findn (const char *p, const char *f, const int n)
{
	char pathbuf[64], *s, *t;

	if (n < sizeof(pathbuf) - 1) {
		memcpy(pathbuf, f, n);
		pathbuf[n] = '\0';
		s = pathbuf;
	} else {
		s = xmalloc(n + 1);
		strcpy(s, f);
	}
	t = lm_find(p, s);
	if (s != pathbuf)
		free(s);
	return (t);
}
#endif

static char *
lml_find (struct lm_list *lmh, const char *f)
{
	struct lm *lm;

	dbg("%s(%p, \"%s\")", __func__, lmh, f);

	TAILQ_FOREACH(lm, lmh, lm_link)
		if (strcmp(f, lm->f) == 0)
			return (lm->t);
	return (NULL);
}

/* Given an executable name, return a pointer to the translation list or
   NULL if no matches */
static struct lm_list *
lmp_find (const char *n)
{
	struct lmp *lmp;

	dbg("%s(\"%s\")", __func__, n);

	TAILQ_FOREACH(lmp, &lmp_head, lmp_link)
		if ((lmp->type == T_EXACT && strcmp(n, lmp->p) == 0) ||
		    (lmp->type == T_DIRECTORY && strncmp(n, lmp->p, strlen(lmp->p)) == 0) ||
		    (lmp->type == T_BASENAME && strcmp(quickbasename(n), lmp->p) == 0))
			return (&lmp->lml);
	return (NULL);
}

static struct lm_list *
lmp_init (char *n)
{
	struct lmp *lmp;

	dbg("%s(\"%s\")", __func__, n);

	lmp = xmalloc(sizeof(struct lmp));
	lmp->p = n;
	if (n[strlen(n)-1] == '/')
		lmp->type = T_DIRECTORY;
	else if (strchr(n,'/') == NULL)
		lmp->type = T_BASENAME;
	else
		lmp->type = T_EXACT;
	TAILQ_INIT(&lmp->lml);
	TAILQ_INSERT_HEAD(&lmp_head, lmp, lmp_link);

	return (&lmp->lml);
}

/* libc basename is overkill.  Return a pointer to the character after the
   last /, or the original string if there are no slashes. */
static const char *
quickbasename (const char *path)
{
	const char *p = path;
	for (; *path; path++) {
		if (*path == '/')
			p = path+1;
	}
	return (p);
}

static int
readstrfn(void * cookie, char *buf, int len)
{
	static char	*current;
	static int	left;
	int 	copied;
	
	copied = 0;
	if (!current) {
		current = cookie;
		left = strlen(cookie);
	}
	while (*current && left && len) {
		*buf++ = *current++;
		left--;
		len--;
		copied++;
	}
	return copied;
}

static int
closestrfn(void * cookie)
{
	free(cookie);
	return 0;
}
