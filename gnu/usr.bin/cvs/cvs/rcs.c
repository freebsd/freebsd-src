/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * The routines contained in this file do all the rcs file parsing and
 * manipulation
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "@(#)rcs.c 1.28 92/03/31";
#endif

#if __STDC__
static char *RCS_getbranch (RCSNode * rcs, char *tag, int force_tag_match);
static char *RCS_getdatebranch (RCSNode * rcs, char *date, char *branch);
static int getrcskey (FILE * fp, char **keyp, char **valp);
static int parse_rcs_proc (Node * file);
static int checkmagic_proc (Node *p);
static void do_branches (List * list, char *val);
static void do_symbols (List * list, char *val);
static void null_delproc (Node * p);
static void rcsnode_delproc (Node * p);
static void rcsvers_delproc (Node * p);
#else
static int parse_rcs_proc ();
static int checkmagic_proc ();
static void rcsnode_delproc ();
static void rcsvers_delproc ();
static void null_delproc ();
static int getrcskey ();
static void do_symbols ();
static void do_branches ();
static char *RCS_getbranch ();
static char *RCS_getdatebranch ();
#endif				/* __STDC__ */

static List *rcslist;
static char *repository;

/*
 * Parse all the rcs files specified and return a list
 */
List *
RCS_parsefiles (files, xrepos)
    List *files;
    char *xrepos;
{
    /* initialize */
    repository = xrepos;
    rcslist = getlist ();

    /* walk the list parsing files */
    if (walklist (files, parse_rcs_proc) != 0)
    {
	/* free the list and return NULL on error */
	dellist (&rcslist);
	return ((List *) NULL);
    }
    else
	/* return the list we built */
	return (rcslist);
}

/*
 * Parse an rcs file into a node on the rcs list
 */
static int
parse_rcs_proc (file)
    Node *file;
{
    Node *p;
    RCSNode *rdata;

    /* parse the rcs file into rdata */
    rdata = RCS_parse (file->key, repository);

    /* if we got a valid RCSNode back, put it on the list */
    if (rdata != (RCSNode *) NULL)
    {
	p = getnode ();
	p->key = xstrdup (file->key);
	p->delproc = rcsnode_delproc;
	p->type = RCSNODE;
	p->data = (char *) rdata;
	(void) addnode (rcslist, p);
    }
    return (0);
}

/*
 * Parse an rcsfile given a user file name and a repository
 */
RCSNode *
RCS_parse (file, repos)
    char *file;
    char *repos;
{
    RCSNode *rcs;
    char rcsfile[PATH_MAX];

    (void) sprintf (rcsfile, "%s/%s%s", repos, file, RCSEXT);
    if (!isreadable (rcsfile))
    {
	(void) sprintf (rcsfile, "%s/%s/%s%s", repos, CVSATTIC,
			file, RCSEXT);
	if (!isreadable (rcsfile))
	    return (NULL);
	rcs = RCS_parsercsfile (rcsfile);
	if (rcs != NULL)
	{
	    rcs->flags |= INATTIC;
	    rcs->flags |= VALID;
	}
	return (rcs);
    }
    rcs = RCS_parsercsfile (rcsfile);
    if (rcs != NULL)
	rcs->flags |= VALID;
    return (rcs);
}

/*
 * Do the real work of parsing an RCS file
 */
RCSNode *
RCS_parsercsfile (rcsfile)
    char *rcsfile;
{
    Node *q, *r;
    RCSNode *rdata;
    RCSVers *vnode;
    int n;
    char *cp;
    char *key, *value;
    FILE *fp;

    /* open the rcsfile */
    if ((fp = fopen (rcsfile, "r")) == NULL)
    {
	error (0, errno, "Couldn't open rcs file `%s'", rcsfile);
	return (NULL);
    }

    /* make a node */
    rdata = (RCSNode *) xmalloc (sizeof (RCSNode));
    bzero ((char *) rdata, sizeof (RCSNode));
    rdata->refcount = 1;
    rdata->path = xstrdup (rcsfile);
    rdata->versions = getlist ();
    rdata->dates = getlist ();

    /*
     * process all the special header information, break out when we get to
     * the first revision delta
     */
    for (;;)
    {
	/* get the next key/value pair */

	/* if key is NULL here, then the file is missing some headers */
	if (getrcskey (fp, &key, &value) == -1 || key == NULL)
	{
	    if (!really_quiet)
		error (0, 0, "`%s' does not appear to be a valid rcs file",
		       rcsfile);
	    freercsnode (&rdata);
	    (void) fclose (fp);
	    return (NULL);
	}

	/* process it */
	if (strcmp (RCSHEAD, key) == 0 && value != NULL)
	{
	    rdata->head = xstrdup (value);
	    continue;
	}
	if (strcmp (RCSBRANCH, key) == 0 && value != NULL)
	{
	    rdata->branch = xstrdup (value);
	    if ((numdots (rdata->branch) & 1) != 0)
	    {
		/* turn it into a branch if it's a revision */
		cp = rindex (rdata->branch, '.');
		*cp = '\0';
	    }
	    continue;
	}
	if (strcmp (RCSSYMBOLS, key) == 0)
	{
	    if (value != NULL)
	    {
		/* if there are tags, set up the tag list */
		rdata->symbols = getlist ();
		do_symbols (rdata->symbols, value);
		continue;
	    }
	}

	/*
	 * check key for '.''s and digits (probably a rev) if it is a
	 * revision, we are done with the headers and are down to the
	 * revision deltas, so we break out of the loop
	 */
	for (cp = key; (isdigit (*cp) || *cp == '.') && *cp != '\0'; cp++)
	     /* do nothing */ ;
	if (*cp == '\0' && strncmp (RCSDATE, value, strlen (RCSDATE)) == 0)
	    break;

	/* if we haven't grabbed it yet, we didn't want it */
    }

    /*
     * we got out of the loop, so we have the first part of the first
     * revision delta in our hand key=the revision and value=the date key and
     * its value
     */
    for (;;)
    {
	char *valp;
	char date[MAXDATELEN];

	/* grab the value of the date from value */
	valp = value + strlen (RCSDATE);/* skip the "date" keyword */
	while (isspace (*valp))		/* take space off front of value */
	    valp++;
	(void) strcpy (date, valp);

	/* get the nodes (q is by version, r is by date) */
	q = getnode ();
	r = getnode ();
	q->type = RCSVERS;
	r->type = RCSVERS;
	q->delproc = rcsvers_delproc;
	r->delproc = null_delproc;
	q->data = r->data = xmalloc (sizeof (RCSVers));
	bzero (q->data, sizeof (RCSVers));
	vnode = (RCSVers *) q->data;

	/* fill in the version before we forget it */
	q->key = vnode->version = xstrdup (key);

	/* throw away the author field */
	(void) getrcskey (fp, &key, &value);

	/* throw away the state field */
	(void) getrcskey (fp, &key, &value);

	/* fill in the date field */
	r->key = vnode->date = xstrdup (date);

	/* fill in the branch list (if any branches exist) */
	(void) getrcskey (fp, &key, &value);
	if (value != (char *) NULL)
	{
	    vnode->branches = getlist ();
	    do_branches (vnode->branches, value);
	}

	/* fill in the next field if there is a next revision */
	(void) getrcskey (fp, &key, &value);
	if (value != (char *) NULL)
	    vnode->next = xstrdup (value);

	/*
	 * at this point, we skip any user defined fields XXX - this is where
	 * we put the symbolic link stuff???
	 */
	while ((n = getrcskey (fp, &key, &value)) >= 0)
	{
	    /* if we have a revision, break and do it */
	    for (cp = key; (isdigit (*cp) || *cp == '.') && *cp != '\0'; cp++)
		 /* do nothing */ ;
	    if (*cp == '\0' && strncmp (RCSDATE, value, strlen (RCSDATE)) == 0)
		break;
	}

	/* add the nodes to the lists */
	(void) addnode (rdata->versions, q);
	(void) addnode (rdata->dates, r);

	/*
	 * if we left the loop because there were no more keys, we break out
	 * of the revision processing loop
	 */
	if (n < 0)
	    break;
    }

    (void) fclose (fp);
    return (rdata);
}

/*
 * rcsnode_delproc - free up an RCS type node
 */
static void
rcsnode_delproc (p)
    Node *p;
{
    freercsnode ((RCSNode **) & p->data);
}

/*
 * freercsnode - free up the info for an RCSNode
 */
void
freercsnode (rnodep)
    RCSNode **rnodep;
{
    if (rnodep == NULL || *rnodep == NULL)
	return;

    ((*rnodep)->refcount)--;
    if ((*rnodep)->refcount != 0)
    {
	*rnodep = (RCSNode *) NULL;
	return;
    }
    free ((*rnodep)->path);
    dellist (&(*rnodep)->versions);
    dellist (&(*rnodep)->dates);
    if ((*rnodep)->symbols != (List *) NULL)
	dellist (&(*rnodep)->symbols);
    if ((*rnodep)->head != (char *) NULL)
	free ((*rnodep)->head);
    if ((*rnodep)->branch != (char *) NULL)
	free ((*rnodep)->branch);
    free ((char *) *rnodep);
    *rnodep = (RCSNode *) NULL;
}

/*
 * rcsvers_delproc - free up an RCSVers type node
 */
static void
rcsvers_delproc (p)
    Node *p;
{
    RCSVers *rnode;

    rnode = (RCSVers *) p->data;

    if (rnode->branches != (List *) NULL)
	dellist (&rnode->branches);
    if (rnode->next != (char *) NULL)
	free (rnode->next);
    free ((char *) rnode);
}

/*
 * null_delproc - don't free anything since it will be free'd by someone else
 */
/* ARGSUSED */
static void
null_delproc (p)
    Node *p;
{
    /* don't do anything */
}

/*
 * getrcskey - fill in the key and value from the rcs file the algorithm is
 *             as follows 
 *
 *    o skip whitespace o fill in key with everything up to next white 
 *      space or semicolon 
 *    o if key == "desc" then key and data are NULL and return -1 
 *    o if key wasn't terminated by a semicolon, skip white space and fill 
 *      in value with everything up to a semicolon o compress all whitespace
 *      down to a single space 
 *    o if a word starts with @, do funky rcs processing
 *    o strip whitespace off end of value or set value to NULL if it empty 
 *    o return 0 since we found something besides "desc"
 */

static char *key = NULL;
static int keysize = 0;
static char *value = NULL;
static int valsize = 0;

#define ALLOCINCR 1024

static int
getrcskey (fp, keyp, valp)
    FILE *fp;
    char **keyp;
    char **valp;
{
    char *cur, *max;
    int c;
    int funky = 0;
    int white = 1;

    /* skip leading whitespace */
    while (1)
    {
	c = getc (fp);
	if (c == EOF)
	{
	    *keyp = (char *) NULL;
	    *valp = (char *) NULL;
	    return (-1);
	}
	if (!isspace (c))
	    break;
    }

    /* fill in key */
    cur = key;
    max = key + keysize;
    while (!isspace (c) && c != ';')
    {
	if (cur < max)
	    *cur++ = c;
	else
	{
	    key = xrealloc (key, keysize + ALLOCINCR);
	    cur = key + keysize;
	    keysize += ALLOCINCR;
	    max = key + keysize;
	    *cur++ = c;
	}
	c = getc (fp);
	if (c == EOF)
	{
	    *keyp = (char *) NULL;
	    *valp = (char *) NULL;
	    return (-1);
	}
    }
    *cur = '\0';

    /* if we got "desc", we are done with the file */
    if (strcmp (RCSDESC, key) == 0)
    {
	*keyp = (char *) NULL;
	*valp = (char *) NULL;
	return (-1);
    }

    /* if we ended key with a semicolon, there is no value */
    if (c == ';')
    {
	*keyp = key;
	*valp = (char *) NULL;
	return (0);
    }

    /* otherwise, there might be a value, so fill it in */
    (void) ungetc (c, fp);
    cur = value;
    max = value + valsize;

    /* process the value */
    for (;;)
    {
	/* get a character */
	c = getc (fp);
	if (c == EOF)
	{
	    *keyp = (char *) NULL;
	    *valp = (char *) NULL;
	    return (-1);
	}

	/* if we are in funky mode, do the rest of this string */
	if (funky)
	{

	    /*
	     * funky mode processing does the following: o @@ means one @ o
	     * all other characters are literal up to a single @ (including
	     * ';')
	     */
	    for (;;)
	    {
		if (c == '@')
		{
		    c = getc (fp);
		    if (c == EOF)
		    {
			*keyp = (char *) NULL;
			*valp = (char *) NULL;
			return (-1);
		    }
		    if (c != '@')
		    {
			/* @ followed by non @ turns off funky mode */
			funky = 0;
			break;
		    }
		    /* otherwise, we already ate one @ so copy the other one */
		}

		/* put the character on the value (maybe allocating space) */
		if (cur >= max)
		{
		    value = xrealloc (value, valsize + ALLOCINCR);
		    cur = value + valsize;
		    valsize += ALLOCINCR;
		    max = value + valsize;
		}
		*cur++ = c;
		c = getc (fp);
		if (c == EOF)
		{
		    *keyp = (char *) NULL;
		    *valp = (char *) NULL;
		    return (-1);
		}
	    }
	}

	/* if we got the semi-colon we are done with the entire value */
	if (c == ';')
	    break;

	/* process the character we got */
	if (white && c == '@')
	{

	    /*
	     * if we are starting a word with an '@', enable funky processing
	     */
	    white = 0;			/* you can't be funky and white :-) */
	    funky = 1;
	}
	else
	{

	    /*
	     * we put the character on the list, compressing all whitespace
	     * to a single space
	     */

	    /* whitespace with white set means compress it out */
	    if (white && isspace (c))
		continue;

	    if (isspace (c))
	    {
		/* make c a space and set white */
		white = 1;
		c = ' ';
	    }
	    else
		white = 0;

	    /* put the char on the end of value (maybe allocating space) */
	    if (cur >= max)
	    {
		value = xrealloc (value, valsize + ALLOCINCR);
		cur = value + valsize;
		valsize += ALLOCINCR;
		max = value + valsize;
	    }
	    *cur++ = c;
	}
    }

    /* if the last char was white space, take it off */
    if (white && cur != value)
	cur--;

    /* terminate the string */
    if (cur)
	*cur = '\0';

    /* if the string is empty, make it null */
    if (value && *value != '\0')
	*valp = value;
    else
	*valp = NULL;
    *keyp = key;
    return (0);
}

/*
 * process the symbols list of the rcs file
 */
static void
do_symbols (list, val)
    List *list;
    char *val;
{
    Node *p;
    char *cp = val;
    char *tag, *rev;

    for (;;)
    {
	/* skip leading whitespace */
	while (isspace (*cp))
	    cp++;

	/* if we got to the end, we are done */
	if (*cp == '\0')
	    break;

	/* split it up into tag and rev */
	tag = cp;
	cp = index (cp, ':');
	*cp++ = '\0';
	rev = cp;
	while (!isspace (*cp) && *cp != '\0')
	    cp++;
	if (*cp != '\0')
	    *cp++ = '\0';

	/* make a new node and add it to the list */
	p = getnode ();
	p->key = xstrdup (tag);
	p->data = xstrdup (rev);
	(void) addnode (list, p);
    }
}

/*
 * process the branches list of a revision delta
 */
static void
do_branches (list, val)
    List *list;
    char *val;
{
    Node *p;
    char *cp = val;
    char *branch;

    for (;;)
    {
	/* skip leading whitespace */
	while (isspace (*cp))
	    cp++;

	/* if we got to the end, we are done */
	if (*cp == '\0')
	    break;

	/* find the end of this branch */
	branch = cp;
	while (!isspace (*cp) && *cp != '\0')
	    cp++;
	if (*cp != '\0')
	    *cp++ = '\0';

	/* make a new node and add it to the list */
	p = getnode ();
	p->key = xstrdup (branch);
	(void) addnode (list, p);
    }
}

/*
 * Version Number
 * 
 * Returns the requested version number of the RCS file, satisfying tags and/or
 * dates, and walking branches, if necessary.
 * 
 * The result is returned; null-string if error.
 */
char *
RCS_getversion (rcs, tag, date, force_tag_match)
    RCSNode *rcs;
    char *tag;
    char *date;
    int force_tag_match;
{
    /* make sure we have something to look at... */
    if (rcs == NULL)
	return ((char *) NULL);

    if (tag && date)
    {
	char *cp, *rev, *tagrev;

	/*
	 * first lookup the tag; if that works, turn the revision into
	 * a branch and lookup the date.
	 */
	tagrev = RCS_gettag (rcs, tag, force_tag_match);
	if (tagrev == NULL)
	    return ((char *) NULL);

	if ((cp = rindex (tagrev, '.')) != NULL)
	    *cp = '\0';
	rev = RCS_getdatebranch (rcs, date, tagrev);
	free (tagrev);
	return (rev);
    }
    else if (tag)
	return (RCS_gettag (rcs, tag, force_tag_match));
    else if (date)
	return (RCS_getdate (rcs, date, force_tag_match));
    else
	return (RCS_head (rcs));

}

/*
 * Find the revision for a specific tag.
 * If force_tag_match is set, return NULL if an exact match is not
 * possible otherwise return RCS_head ().  We are careful to look for
 * and handle "magic" revisions specially.
 * 
 * If the matched tag is a branch tag, find the head of the branch.
 */
char *
RCS_gettag (rcs, tag, force_tag_match)
    RCSNode *rcs;
    char *tag;
    int force_tag_match;
{
    Node *p;

    /* make sure we have something to look at... */
    if (rcs == NULL)
	return ((char *) NULL);

    /* If tag is "HEAD", special case to get head RCS revision */
    if (tag && (strcmp (tag, TAG_HEAD) == 0 || *tag == '\0'))
	if (force_tag_match && (rcs->flags & VALID) && (rcs->flags & INATTIC))
	    return ((char *) NULL);	/* head request for removed file */
	else
	    return (RCS_head (rcs));

    if (!isdigit (tag[0]))
    {
	/* If we got a symbolic tag, resolve it to a numeric */
	if (rcs == NULL)
	    p = NULL;
	else
	    p = findnode (rcs->symbols, tag);
	if (p != NULL)
	{
	    int dots;
	    char *magic, *branch, *cp;

	    tag = p->data;

	    /*
	     * If this is a magic revision, we turn it into either its
	     * physical branch equivalent (if one exists) or into
	     * its base revision, which we assume exists.
	     */
	    dots = numdots (tag);
	    if (dots > 2 && (dots & 1) != 0)
	    {
		branch = rindex (tag, '.');
		cp = branch++ - 1;
		while (*cp != '.')
		    cp--;

		/* see if we have .magic-branch. (".0.") */
		magic = xmalloc (strlen (tag) + 1);
		(void) sprintf (magic, ".%d.", RCS_MAGIC_BRANCH);
		if (strncmp (magic, cp, strlen (magic)) == 0)
		{
		    char *xtag;

		    /* it's magic.  See if the branch exists */
		    *cp = '\0';		/* turn it into a revision */
		    xtag = xstrdup (tag);
		    *cp = '.';		/* and back again */
		    (void) sprintf (magic, "%s.%s", xtag, branch);
		    branch = RCS_getbranch (rcs, magic, 1);
		    free (magic);
		    if (branch != NULL)
		    {
			free (xtag);
			return (branch);
		    }
		    return (xtag);
		}
		free (magic);
	    }
	}
	else
	{
	    /* The tag wasn't there, so return the head or NULL */
	    if (force_tag_match)
		return (NULL);
	    else
		return (RCS_head (rcs));
	}
    }

    /*
     * numeric tag processing:
     *		1) revision number - just return it
     *		2) branch number   - find head of branch
     */

    /* strip trailing dots */
    while (tag[strlen (tag) - 1] == '.')
	tag[strlen (tag) - 1] = '\0';

    if ((numdots (tag) & 1) == 0)
    {
	/* we have a branch tag, so we need to walk the branch */
	return (RCS_getbranch (rcs, tag, force_tag_match));
    }
    else
    {
	/* we have a revision tag, so make sure it exists */
	if (rcs == NULL)
	    p = NULL;
	else
	    p = findnode (rcs->versions, tag);
	if (p != NULL)
	    return (xstrdup (tag));
	else
	{
	    /* The revision wasn't there, so return the head or NULL */
	    if (force_tag_match)
		return (NULL);
	    else
		return (RCS_head (rcs));
	}
    }
}

/*
 * Return a "magic" revision as a virtual branch off of REV for the RCS file.
 * A "magic" revision is one which is unique in the RCS file.  By unique, I
 * mean we return a revision which:
 *	- has a branch of 0 (see rcs.h RCS_MAGIC_BRANCH)
 *	- has a revision component which is not an existing branch off REV
 *	- has a revision component which is not an existing magic revision
 *	- is an even-numbered revision, to avoid conflicts with vendor branches
 * The first point is what makes it "magic".
 *
 * As an example, if we pass in 1.37 as REV, we will look for an existing
 * branch called 1.37.2.  If it did not exist, we would look for an
 * existing symbolic tag with a numeric part equal to 1.37.0.2.  If that
 * didn't exist, then we know that the 1.37.2 branch can be reserved by
 * creating a symbolic tag with 1.37.0.2 as the numeric part.
 *
 * This allows us to fork development with very little overhead -- just a
 * symbolic tag is used in the RCS file.  When a commit is done, a physical
 * branch is dynamically created to hold the new revision.
 *
 * Note: We assume that REV is an RCS revision and not a branch number.
 */
static char *check_rev;
char *
RCS_magicrev (rcs, rev)
    RCSNode *rcs;
    char *rev;
{
    int rev_num;
    char *xrev, *test_branch;

    xrev = xmalloc (strlen (rev) + 14); /* enough for .0.number */
    check_rev = xrev;

    /* only look at even numbered branches */
    for (rev_num = 2; ; rev_num += 2)
    {
	/* see if the physical branch exists */
	(void) sprintf (xrev, "%s.%d", rev, rev_num);
	test_branch = RCS_getbranch (rcs, xrev, 1);
	if (test_branch != NULL)	/* it did, so keep looking */
	{
	    free (test_branch);
	    continue;
	}

	/* now, create a "magic" revision */
	(void) sprintf (xrev, "%s.%d.%d", rev, RCS_MAGIC_BRANCH, rev_num);

	/* walk the symbols list to see if a magic one already exists */
	if (walklist (rcs->symbols, checkmagic_proc) != 0)
	    continue;

	/* we found a free magic branch.  Claim it as ours */
	return (xrev);
    }
}

/*
 * walklist proc to look for a match in the symbols list.
 * Returns 0 if the symbol does not match, 1 if it does.
 */
static int
checkmagic_proc (p)
    Node *p;
{
    if (strcmp (check_rev, p->data) == 0)
	return (1);
    else
	return (0);
}

/*
 * Returns non-zero if the specified revision number or symbolic tag
 * resolves to a "branch" within the rcs file.  We do take into account
 * any magic branches as well.
 */
int
RCS_isbranch (file, rev, srcfiles)
    char *file;
    char *rev;
    List *srcfiles;
{
    int dots;
    Node *p;
    RCSNode *rcs;

    /* numeric revisions are easy -- even number of dots is a branch */
    if (isdigit (*rev))
	return ((numdots (rev) & 1) == 0);

    /* assume a revision if you can't find the RCS info */
    p = findnode (srcfiles, file);
    if (p == NULL)
	return (0);

    /* now, look for a match in the symbols list */
    rcs = (RCSNode *) p->data;
    p = findnode (rcs->symbols, rev);
    if (p == NULL)
	return (0);
    dots = numdots (p->data);
    if ((dots & 1) == 0)
	return (1);

    /* got a symbolic tag match, but it's not a branch; see if it's magic */
    if (dots > 2)
    {
	char *magic;
	char *branch = rindex (p->data, '.');
	char *cp = branch - 1;
	while (*cp != '.')
	    cp--;

	/* see if we have .magic-branch. (".0.") */
	magic = xmalloc (strlen (p->data) + 1);
	(void) sprintf (magic, ".%d.", RCS_MAGIC_BRANCH);
	if (strncmp (magic, cp, strlen (magic)) == 0)
	{
	    free (magic);
	    return (1);
	}
	free (magic);
    }
    return (0);
}

/*
 * Returns a pointer to malloc'ed memory which contains the branch
 * for the specified *symbolic* tag.  Magic branches are handled correctly.
 */
char *
RCS_whatbranch (file, rev, srcfiles)
    char *file;
    char *rev;
    List *srcfiles;
{
    int dots;
    Node *p;
    RCSNode *rcs;

    /* assume no branch if you can't find the RCS info */
    p = findnode (srcfiles, file);
    if (p == NULL)
	return ((char *) NULL);

    /* now, look for a match in the symbols list */
    rcs = (RCSNode *) p->data;
    p = findnode (rcs->symbols, rev);
    if (p == NULL)
	return ((char *) NULL);
    dots = numdots (p->data);
    if ((dots & 1) == 0)
	return (xstrdup (p->data));

    /* got a symbolic tag match, but it's not a branch; see if it's magic */
    if (dots > 2)
    {
	char *magic;
	char *branch = rindex (p->data, '.');
	char *cp = branch++ - 1;
	while (*cp != '.')
	    cp--;

	/* see if we have .magic-branch. (".0.") */
	magic = xmalloc (strlen (p->data) + 1);
	(void) sprintf (magic, ".%d.", RCS_MAGIC_BRANCH);
	if (strncmp (magic, cp, strlen (magic)) == 0)
	{
	    /* yep.  it's magic.  now, construct the real branch */
	    *cp = '\0';			/* turn it into a revision */
	    (void) sprintf (magic, "%s.%s", p->data, branch);
	    *cp = '.';			/* and turn it back */
	    return (magic);
	}
	free (magic);
    }
    return ((char *) NULL);
}

/*
 * Get the head of the specified branch.  If the branch does not exist,
 * return NULL or RCS_head depending on force_tag_match
 */
static char *
RCS_getbranch (rcs, tag, force_tag_match)
    RCSNode *rcs;
    char *tag;
    int force_tag_match;
{
    Node *p, *head;
    RCSVers *vn;
    char *xtag;
    char *nextvers;
    char *cp;

    /* make sure we have something to look at... */
    if (rcs == NULL)
	return ((char *) NULL);

    /* find out if the tag contains a dot, or is on the trunk */
    cp = rindex (tag, '.');

    /* trunk processing is the special case */
    if (cp == NULL)
    {
	xtag = xmalloc (strlen (tag) + 1 + 1);	/* +1 for an extra . */
	(void) strcpy (xtag, tag);
	(void) strcat (xtag, ".");
	for (cp = rcs->head; cp != NULL;)
	{
	    if (strncmp (xtag, cp, strlen (xtag)) == 0)
		break;
	    p = findnode (rcs->versions, cp);
	    if (p == NULL)
	    {
		free (xtag);
		if (force_tag_match)
		    return (NULL);
		else
		    return (RCS_head (rcs));
	    }
	    vn = (RCSVers *) p->data;
	    cp = vn->next;
	}
	free (xtag);
	if (cp == NULL)
	{
	    if (force_tag_match)
		return (NULL);
	    else
		return (RCS_head (rcs));
	}
	return (xstrdup (cp));
    }

    /* if it had a `.', terminate the string so we have the base revision */
    *cp = '\0';

    /* look up the revision this branch is based on */
    p = findnode (rcs->versions, tag);

    /* put the . back so we have the branch again */
    *cp = '.';

    if (p == NULL)
    {
	/* if the base revision didn't exist, return head or NULL */
	if (force_tag_match)
	    return (NULL);
	else
	    return (RCS_head (rcs));
    }

    /* find the first element of the branch we are looking for */
    vn = (RCSVers *) p->data;
    if (vn->branches == NULL)
	return (NULL);
    xtag = xmalloc (strlen (tag) + 1 + 1);	/* 1 for the extra '.' */
    (void) strcpy (xtag, tag);
    (void) strcat (xtag, ".");
    head = vn->branches->list;
    for (p = head->next; p != head; p = p->next)
	if (strncmp (p->key, xtag, strlen (xtag)) == 0)
	    break;
    free (xtag);

    if (p == head)
    {
	/* we didn't find a match so return head or NULL */
	if (force_tag_match)
	    return (NULL);
	else
	    return (RCS_head (rcs));
    }

    /* now walk the next pointers of the branch */
    nextvers = p->key;
    do
    {
	p = findnode (rcs->versions, nextvers);
	if (p == NULL)
	{
	    /* a link in the chain is missing - return head or NULL */
	    if (force_tag_match)
		return (NULL);
	    else
		return (RCS_head (rcs));
	}
	vn = (RCSVers *) p->data;
	nextvers = vn->next;
    } while (nextvers != NULL);

    /* we have the version in our hand, so go for it */
    return (xstrdup (vn->version));
}

/*
 * Get the head of the RCS file.  If branch is set, this is the head of the
 * branch, otherwise the real head
 */
char *
RCS_head (rcs)
    RCSNode *rcs;
{
    /* make sure we have something to look at... */
    if (rcs == NULL)
	return ((char *) NULL);

    if (rcs->branch)
	return (RCS_getbranch (rcs, rcs->branch, 1));

    /*
     * NOTE: we call getbranch with force_tag_match set to avoid any
     * possibility of recursion
     */
    else
	return (xstrdup (rcs->head));
}

/*
 * Get the most recent revision, based on the supplied date, but use some
 * funky stuff and follow the vendor branch maybe
 */
char *
RCS_getdate (rcs, date, force_tag_match)
    RCSNode *rcs;
    char *date;
    int force_tag_match;
{
    char *cur_rev = NULL;
    char *retval = NULL;
    Node *p;
    RCSVers *vers = NULL;

    /* make sure we have something to look at... */
    if (rcs == NULL)
	return ((char *) NULL);

    /* if the head is on a branch, try the branch first */
    if (rcs->branch != NULL)
	retval = RCS_getdatebranch (rcs, date, rcs->branch);

    /* if we found a match, we are done */
    if (retval != NULL)
	return (retval);

    /* otherwise if we have a trunk, try it */
    if (rcs->head)
    {
	p = findnode (rcs->versions, rcs->head);
	while (p != NULL)
	{
	    /* if the date of this one is before date, take it */
	    vers = (RCSVers *) p->data;
	    if (RCS_datecmp (vers->date, date) <= 0)
	    {
		cur_rev = vers->version;
		break;
	    }

	    /* if there is a next version, find the node */
	    if (vers->next != NULL)
		p = findnode (rcs->versions, vers->next);
	    else
		p = (Node *) NULL;
	}
    }

    /*
     * at this point, either we have the revision we want, or we have the
     * first revision on the trunk (1.1?) in our hands
     */

    /* if we found what we're looking for, and it's not 1.1 return it */
    if (cur_rev != NULL && strcmp (cur_rev, "1.1") != 0)
	return (xstrdup (cur_rev));

    /* look on the vendor branch */
    retval = RCS_getdatebranch (rcs, date, CVSBRANCH);

    /*
     * if we found a match, return it; otherwise, we return the first
     * revision on the trunk or NULL depending on force_tag_match and the
     * date of the first rev
     */
    if (retval != NULL)
	return (retval);

    if (!force_tag_match || RCS_datecmp (vers->date, date) <= 0)
	return (xstrdup (vers->version));
    else
	return (NULL);
}

/*
 * Look up the last element on a branch that was put in before the specified
 * date (return the rev or NULL)
 */
static char *
RCS_getdatebranch (rcs, date, branch)
    RCSNode *rcs;
    char *date;
    char *branch;
{
    char *cur_rev = NULL;
    char *cp;
    char *xbranch, *xrev;
    Node *p;
    RCSVers *vers;

    /* look up the first revision on the branch */
    xrev = xstrdup (branch);
    cp = rindex (xrev, '.');
    if (cp == NULL)
    {
	free (xrev);
	return (NULL);
    }
    *cp = '\0';				/* turn it into a revision */
    p = findnode (rcs->versions, xrev);
    free (xrev);
    if (p == NULL)
	return (NULL);
    vers = (RCSVers *) p->data;

    /* if no branches list, return NULL */
    if (vers->branches == NULL)
	return (NULL);

    /* walk the branches list looking for the branch number */
    xbranch = xmalloc (strlen (branch) + 1 + 1); /* +1 for the extra dot */
    (void) strcpy (xbranch, branch);
    (void) strcat (xbranch, ".");
    for (p = vers->branches->list->next; p != vers->branches->list; p = p->next)
	if (strncmp (p->key, xbranch, strlen (xbranch)) == 0)
	    break;
    free (xbranch);
    if (p == vers->branches->list)
	return (NULL);

    p = findnode (rcs->versions, p->key);

    /* walk the next pointers until you find the end, or the date is too late */
    while (p != NULL)
    {
	vers = (RCSVers *) p->data;
	if (RCS_datecmp (vers->date, date) <= 0)
	    cur_rev = vers->version;
	else
	    break;

	/* if there is a next version, find the node */
	if (vers->next != NULL)
	    p = findnode (rcs->versions, vers->next);
	else
	    p = (Node *) NULL;
    }

    /* if we found something acceptable, return it - otherwise NULL */
    if (cur_rev != NULL)
	return (xstrdup (cur_rev));
    else
	return (NULL);
}

/*
 * Compare two dates in RCS format. Beware the change in format on January 1,
 * 2000, when years go from 2-digit to full format.
 */
int
RCS_datecmp (date1, date2)
    char *date1, *date2;
{
    int length_diff = strlen (date1) - strlen (date2);

    return (length_diff ? length_diff : strcmp (date1, date2));
}

/*
 * Lookup the specified revision in the ,v file and return, in the date
 * argument, the date specified for the revision *minus one second*, so that
 * the logically previous revision will be found later.
 * 
 * Returns zero on failure, RCS revision time as a Unix "time_t" on success.
 */
time_t
RCS_getrevtime (rcs, rev, date, fudge)
    RCSNode *rcs;
    char *rev;
    char *date;
    int fudge;
{
    char tdate[MAXDATELEN];
    struct tm xtm, *ftm;
    time_t revdate = 0;
    Node *p;
    RCSVers *vers;

    /* make sure we have something to look at... */
    if (rcs == NULL)
	return (revdate);

    /* look up the revision */
    p = findnode (rcs->versions, rev);
    if (p == NULL)
	return (-1);
    vers = (RCSVers *) p->data;

    /* split up the date */
    ftm = &xtm;
    (void) sscanf (vers->date, SDATEFORM, &ftm->tm_year, &ftm->tm_mon,
		   &ftm->tm_mday, &ftm->tm_hour, &ftm->tm_min,
		   &ftm->tm_sec);
    if (ftm->tm_year > 1900)
	ftm->tm_year -= 1900;

    /* put the date in a form getdate can grok */
#ifdef HAVE_RCS5
    (void) sprintf (tdate, "%d/%d/%d GMT %d:%d:%d", ftm->tm_mon,
		    ftm->tm_mday, ftm->tm_year, ftm->tm_hour,
		    ftm->tm_min, ftm->tm_sec);
#else
    (void) sprintf (tdate, "%d/%d/%d %d:%d:%d", ftm->tm_mon,
		    ftm->tm_mday, ftm->tm_year, ftm->tm_hour,
		    ftm->tm_min, ftm->tm_sec);
#endif

    /* turn it into seconds since the epoch */
    revdate = get_date (tdate, (struct timeb *) NULL);
    if (revdate != (time_t) -1)
    {
	revdate -= fudge;		/* remove "fudge" seconds */
	if (date)
	{
	    /* put an appropriate string into ``date'' if we were given one */
#ifdef HAVE_RCS5
	    ftm = gmtime (&revdate);
#else
	    ftm = localtime (&revdate);
#endif
	    (void) sprintf (date, DATEFORM,
			    ftm->tm_year + (ftm->tm_year < 100 ? 0 : 1900),
			    ftm->tm_mon + 1, ftm->tm_mday, ftm->tm_hour,
			    ftm->tm_min, ftm->tm_sec);
	}
    }
    return (revdate);
}

/*
 * The argument ARG is the getopt remainder of the -k option specified on the
 * command line.  This function returns malloc'ed space that can be used
 * directly in calls to RCS V5, with the -k flag munged correctly.
 */
char *
RCS_check_kflag (arg)
    char *arg;
{
    static char *kflags[] =
    {"kv", "kvl", "k", "v", "o", (char *) NULL};
    char karg[10];
    char **cpp = NULL;

#ifndef HAVE_RCS5
    error (1, 0, "%s %s: your version of RCS does not support the -k option",
	   program_name, command_name);
#endif

    if (arg)
    {
	for (cpp = kflags; *cpp != NULL; cpp++)
	{
	    if (strcmp (arg, *cpp) == 0)
		break;
	}
    }

    if (arg == NULL || *cpp == NULL)
    {
	(void) fprintf (stderr, "%s %s: invalid -k option\n",
			program_name, command_name);
	(void) fprintf (stderr, "\tvalid options are:\n");
	for (cpp = kflags; *cpp != NULL; cpp++)
	    (void) fprintf (stderr, "\t\t-k%s\n", *cpp);
	error (1, 0, "Please retry with a valid -k option");
    }

    (void) sprintf (karg, "-k%s", *cpp);
    return (xstrdup (karg));
}

/*
 * Do some consistency checks on the symbolic tag... These should equate
 * pretty close to what RCS checks, though I don't know for certain.
 */
void
RCS_check_tag (tag)
    char *tag;
{
    char *invalid = "$,.:;@";		/* invalid RCS tag characters */
    char *cp;

    /*
     * The first character must be an alphabetic letter. The remaining
     * characters cannot be non-visible graphic characters, and must not be
     * in the set of "invalid" RCS identifier characters.
     */
    if (isalpha (*tag))
    {
	for (cp = tag; *cp; cp++)
	{
	    if (!isgraph (*cp))
		error (1, 0, "tag `%s' has non-visible graphic characters",
		       tag);
	    if (index (invalid, *cp))
		error (1, 0, "tag `%s' must not contain the characters `%s'",
		       tag, invalid);
	}
    }
    else
	error (1, 0, "tag `%s' must start with a letter", tag);
}
