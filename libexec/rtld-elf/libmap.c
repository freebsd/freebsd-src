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

#ifndef _PATH_LIBMAP_CONF
#define	_PATH_LIBMAP_CONF	"/etc/libmap.conf"
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
	struct lm_list lml;
	TAILQ_ENTRY(lmp) lmp_link;
};

static void		lm_add		(char *, char *, char *);
static void		lm_free		(struct lm_list *);
static char *		lml_find	(struct lm_list *, const char *);
static struct lm_list *	lmp_find	(const char *);
static struct lm_list *	lmp_init	(char *);

#define	iseol(c)	(((c) == '#') || ((c) == '\0') || \
			 ((c) == '\n') || ((c) == '\r'))

void
lm_init (void)
{
	FILE	*fp;
	char	*cp;
	char	*f, *t, *p;
	char	prog[MAXPATHLEN];
	char	line[MAXPATHLEN + 2];

	TAILQ_INIT(&lmp_head);

	if ((fp = fopen(_PATH_LIBMAP_CONF, "r")) == NULL)
		return;

	p = NULL;
	while ((cp = fgets(line, MAXPATHLEN + 1, fp)) != NULL) {
		t = f = NULL;

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

			p = cp++;
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
			while(isspace(*cp++));
			if (!iseol(*cp)) continue;

			strcpy(prog, p);
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
		lm_add(p, xstrdup(f), xstrdup(t));
	}
	fclose(fp);
	return;
}

static void
lm_free (struct lm_list *lml)
{
	struct lm *lm;

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
lm_add (char *p, char *f, char *t)
{
	struct lm_list *lml;
	struct lm *lm;

	if (p == NULL)
		p = "$DEFAULT$";

	if ((lml = lmp_find(p)) == NULL)
		lml = lmp_init(xstrdup(p));

	lm = xmalloc(sizeof(struct lm));
	lm->f = f;
	lm->t = t;
	TAILQ_INSERT_HEAD(lml, lm, lm_link);
}

char *
lm_find (const char *p, const char *f)
{
	struct lm_list *lml;
	char *t;

	if (p != NULL && (lml = lmp_find(p)) != NULL) {
		t = lml_find(lml, f);
		if (t != NULL) {
			/*
			 * Add a global mapping if we have
			 * a successful constrained match.
			 */
			lm_add(NULL, xstrdup(f), xstrdup(t));
			return (t);
		}
	}
	lml = lmp_find("$DEFAULT$");
	if (lml != NULL)
		return (lml_find(lml, f));
	else
		return (NULL);
}

static char *
lml_find (struct lm_list *lmh, const char *f)
{
	struct lm *lm;

	TAILQ_FOREACH(lm, lmh, lm_link)
		if ((strncmp(f, lm->f, strlen(lm->f)) == 0) &&
		    (strlen(f) == strlen(lm->f)))
			return (lm->t);
	return NULL;
}

static struct lm_list *
lmp_find (const char *n)
{
	struct lmp *lmp;

	TAILQ_FOREACH(lmp, &lmp_head, lmp_link)
		if ((strncmp(n, lmp->p, strlen(lmp->p)) == 0) &&
		    (strlen(n) == strlen(lmp->p)))
			return (&lmp->lml);
	return (NULL);
}

static struct lm_list *
lmp_init (char *n)
{
	struct lmp *lmp;

	lmp = xmalloc(sizeof(struct lmp));
	lmp->p = n;
	TAILQ_INIT(&lmp->lml);
	TAILQ_INSERT_HEAD(&lmp_head, lmp, lmp_link);

	return (&lmp->lml);
}

