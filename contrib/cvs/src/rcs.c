/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * The routines contained in this file do all the rcs file parsing and
 * manipulation
 */

#include <assert.h>
#include "cvs.h"

static RCSNode *RCS_parsercsfile_i PROTO((FILE * fp, const char *rcsfile));
static char *RCS_getdatebranch PROTO((RCSNode * rcs, char *date, char *branch));
static int getrcskey PROTO((FILE * fp, char **keyp, char **valp));
static int checkmagic_proc PROTO((Node *p, void *closure));
static void do_branches PROTO((List * list, char *val));
static void do_symbols PROTO((List * list, char *val));
static void rcsvers_delproc PROTO((Node * p));

/*
 * We don't want to use isspace() from the C library because:
 *
 * 1. The definition of "whitespace" in RCS files includes ASCII
 *    backspace, but the C locale doesn't.
 * 2. isspace is an very expensive function call in some implementations
 *    due to the addition of wide character support.
 */
static const char spacetab[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0,	/* 0x00 - 0x0f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x10 - 0x1f */
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x20 - 0x2f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x30 - 0x3f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 - 0x4f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 - 0x5f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x60 - 0x8f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x70 - 0x7f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x80 - 0x8f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x90 - 0x9f */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xa0 - 0xaf */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xb0 - 0xbf */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xc0 - 0xcf */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xd0 - 0xdf */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xe0 - 0xef */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  /* 0xf0 - 0xff */
};

#define whitespace(c)	(spacetab[(unsigned char)c] != 0)


/*
 * Parse an rcsfile given a user file name and a repository
 */
RCSNode *
RCS_parse (file, repos)
    const char *file;
    const char *repos;
{
    RCSNode *rcs;
    FILE *fp;
    char rcsfile[PATH_MAX];

    (void) sprintf (rcsfile, "%s/%s%s", repos, file, RCSEXT);
    if ((fp = fopen (rcsfile, FOPEN_BINARY_READ)) != NULL) 
    {
        rcs = RCS_parsercsfile_i(fp, rcsfile);
	if (rcs != NULL) 
	    rcs->flags |= VALID;

	fclose (fp);
	return (rcs);
    }
    else if (! existence_error (errno))
    {
	error (0, errno, "cannot open %s", rcsfile);
	return NULL;
    }

    (void) sprintf (rcsfile, "%s/%s/%s%s", repos, CVSATTIC, file, RCSEXT);
    if ((fp = fopen (rcsfile, FOPEN_BINARY_READ)) != NULL) 
    {
        rcs = RCS_parsercsfile_i(fp, rcsfile);
	if (rcs != NULL)
	{
	    rcs->flags |= INATTIC;
	    rcs->flags |= VALID;
	}

	fclose (fp);
	return (rcs);
    }
    else if (! existence_error (errno))
    {
	error (0, errno, "cannot open %s", rcsfile);
	return NULL;
    }

    return (NULL);
}

/*
 * Parse a specific rcsfile.
 */
RCSNode *
RCS_parsercsfile (rcsfile)
    char *rcsfile;
{
    FILE *fp;
    RCSNode *rcs;

    /* open the rcsfile */
    if ((fp = fopen (rcsfile, FOPEN_BINARY_READ)) == NULL)
    {
	error (0, errno, "Couldn't open rcs file `%s'", rcsfile);
	return (NULL);
    }

    rcs = RCS_parsercsfile_i (fp, rcsfile);

    fclose (fp);
    return (rcs);
}


/*
 */ 
static RCSNode *
RCS_parsercsfile_i (fp, rcsfile)
    FILE *fp;
    const char *rcsfile;
{
    RCSNode *rdata;
    char *key, *value;

    /* make a node */
    rdata = (RCSNode *) xmalloc (sizeof (RCSNode));
    memset ((char *) rdata, 0, sizeof (RCSNode));
    rdata->refcount = 1;
    rdata->path = xstrdup (rcsfile);

    /* Process HEAD and BRANCH keywords from the RCS header.  
     *
     * Most cvs operatations on the main branch don't need any more
     * information.  Those that do call XXX to completely parse the
     * RCS file.  */

    if (getrcskey (fp, &key, &value) == -1 || key == NULL)
	goto l_error;
    if (strcmp (key, RCSDESC) == 0)
	goto l_error;

    if (strcmp (RCSHEAD, key) == 0 && value != NULL)
	rdata->head = xstrdup (value);

    if (getrcskey (fp, &key, &value) == -1 || key == NULL)
	goto l_error;
    if (strcmp (key, RCSDESC) == 0)
	goto l_error;

    if (strcmp (RCSBRANCH, key) == 0 && value != NULL)
    {
	char *cp;

	rdata->branch = xstrdup (value);
	if ((numdots (rdata->branch) & 1) != 0)
	{
	    /* turn it into a branch if it's a revision */
	    cp = strrchr (rdata->branch, '.');
	    *cp = '\0';
	}
    }

    rdata->flags |= PARTIAL;
    return rdata;

l_error:
    if (!really_quiet)
    {
	if (ferror(fp))
	{
	    error (1, 0, "error reading `%s'", rcsfile);
	}
	else
	{
	    error (0, 0, "`%s' does not appear to be a valid rcs file",
		   rcsfile);
	}
    }
    freercsnode (&rdata);
    return (NULL);
}


/* Do the real work of parsing an RCS file.

   On error, die with a fatal error; if it returns at all it was successful.

   If PFP is NULL, close the file when done.  Otherwise, leave it open
   and store the FILE * in *PFP.  */
static void
RCS_reparsercsfile (rdata, pfp)
    RCSNode *rdata;
    FILE **pfp;
{
    FILE *fp;
    char *rcsfile;

    Node *q;
    RCSVers *vnode;
    int n;
    char *cp;
    char *key, *value;

    assert (rdata != NULL);
    rcsfile = rdata->path;

    fp = fopen(rcsfile, FOPEN_BINARY_READ);
    if (fp == NULL)
	error (1, 0, "unable to reopen `%s'", rcsfile);

    /* make a node */
    rdata->versions = getlist ();

    /*
     * process all the special header information, break out when we get to
     * the first revision delta
     */
    for (;;)
    {
	/* get the next key/value pair */

	/* if key is NULL here, then the file is missing some headers
	   or we had trouble reading the file. */
	if (getrcskey (fp, &key, &value) == -1 || key == NULL
	    || strcmp (key, RCSDESC) == 0)
	{
	    if (ferror(fp))
	    {
		error (1, 0, "error reading `%s'", rcsfile);
	    }
	    else
	    {
		error (1, 0, "`%s' does not appear to be a valid rcs file",
		       rcsfile);
	    }
	}

	if (strcmp (RCSSYMBOLS, key) == 0)
	{
	    if (value != NULL)
	    {
		rdata->symbols_data = xstrdup(value);
		continue;
	    }
	}

	if (strcmp (RCSEXPAND, key) == 0)
	{
	    rdata->expand = xstrdup (value);
	    continue;
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

        vnode = (RCSVers *) xmalloc (sizeof (RCSVers));
	memset (vnode, 0, sizeof (RCSVers));

	/* fill in the version before we forget it */
	vnode->version = xstrdup (key);

	/* grab the value of the date from value */
	valp = value + strlen (RCSDATE);/* skip the "date" keyword */
	while (whitespace (*valp))		/* take space off front of value */
	    valp++;

	vnode->date = xstrdup (valp);

	/* Get author field.  */
	(void) getrcskey (fp, &key, &value);
	/* FIXME: should be using errno in case of ferror.  */
	if (key == NULL || strcmp (key, "author") != 0)
	    error (1, 0, "\
unable to parse rcs file; `author' not in the expected place");
	vnode->author = xstrdup (value);

	/* Get state field.  */
	(void) getrcskey (fp, &key, &value);
	/* FIXME: should be using errno in case of ferror.  */
	if (key == NULL || strcmp (key, "state") != 0)
	    error (1, 0, "\
unable to parse rcs file; `state' not in the expected place");
	if (strcmp (value, "dead") == 0)
	{
	    vnode->dead = 1;
	}

	/* fill in the branch list (if any branches exist) */
	(void) getrcskey (fp, &key, &value);
	/* FIXME: should be handling various error conditions better.  */
	if (key != NULL && strcmp (key, RCSDESC) == 0)
	    value = NULL;
	if (value != (char *) NULL)
	{
	    vnode->branches = getlist ();
	    do_branches (vnode->branches, value);
	}

	/* fill in the next field if there is a next revision */
	(void) getrcskey (fp, &key, &value);
	/* FIXME: should be handling various error conditions better.  */
	if (key != NULL && strcmp (key, RCSDESC) == 0)
	    value = NULL;
	if (value != (char *) NULL)
	    vnode->next = xstrdup (value);

	/*
	 * at this point, we skip any user defined fields XXX - this is where
	 * we put the symbolic link stuff???
	 */
	/* FIXME: Does not correctly handle errors, e.g. from stdio.  */
	while ((n = getrcskey (fp, &key, &value)) >= 0)
	{
	    assert (key != NULL);

	    if (strcmp (key, RCSDESC) == 0)
	    {
		n = -1;
		break;
	    }

	    /* Enable use of repositories created by certain obsolete
	       versions of CVS.  This code should remain indefinately;
	       there is no procedure for converting old repositories, and
	       checking for it is harmless.  */
	    if (strcmp(key, RCSDEAD) == 0)
	    {
		vnode->dead = 1;
		continue;
	    }
	    /* if we have a revision, break and do it */
	    for (cp = key; (isdigit (*cp) || *cp == '.') && *cp != '\0'; cp++)
		 /* do nothing */ ;
	    if (*cp == '\0' && strncmp (RCSDATE, value, strlen (RCSDATE)) == 0)
		break;
	}

	/* get the node */
	q = getnode ();
	q->type = RCSVERS;
	q->delproc = rcsvers_delproc;
	q->data = (char *) vnode;
	q->key = vnode->version;

	/* add the nodes to the list */
	if (addnode (rdata->versions, q) != 0)
	{
#if 0
		purify_printf("WARNING: Adding duplicate version: %s (%s)\n",
			 q->key, rcsfile);
		freenode (q);
#endif
	}

	/*
	 * if we left the loop because there were no more keys, we break out
	 * of the revision processing loop
	 */
	if (n < 0)
	    break;
    }

    if (pfp == NULL)
    {
	if (fclose (fp) < 0)
	    error (0, errno, "cannot close %s", rcsfile);
    }
    else
    {
	*pfp = fp;
    }
    rdata->flags &= ~PARTIAL;
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
    if ((*rnodep)->symbols != (List *) NULL)
	dellist (&(*rnodep)->symbols);
    if ((*rnodep)->symbols_data != (char *) NULL)
	free ((*rnodep)->symbols_data);
    if ((*rnodep)->expand != NULL)
	free ((*rnodep)->expand);
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
    if (rnode->date != (char *) NULL)
	free (rnode->date);
    if (rnode->next != (char *) NULL)
	free (rnode->next);
    free ((char *) rnode);
}

/*
 * getrcskey - fill in the key and value from the rcs file the algorithm is
 *             as follows 
 *
 *    o skip whitespace o fill in key with everything up to next white 
 *      space or semicolon 
 *    o if key == "desc" then key and data are NULL and return -1 
 *    o if key wasn't terminated by a semicolon, skip white space and fill 
 *      in value with everything up to a semicolon 
 *    o compress all whitespace down to a single space 
 *    o if a word starts with @, do funky rcs processing
 *    o strip whitespace off end of value or set value to NULL if it empty 
 *    o return 0 since we found something besides "desc"
 *
 * Sets *KEYP and *VALUEP to point to storage managed by the getrcskey
 * function; the contents are only valid until the next call to getrcskey
 * or getrcsrev.
 */

static char *key = NULL;
static char *value = NULL;
static size_t keysize = 0;
static size_t valsize = 0;

#define ALLOCINCR 1024

static int
getrcskey (fp, keyp, valp)
    FILE *fp;
    char **keyp;
    char **valp;
{
    char *cur, *max;
    int c;

    /* skip leading whitespace */
    do
    {
	c = getc (fp);
	if (c == EOF)
	{
	    *keyp = (char *) NULL;
	    *valp = (char *) NULL;
	    return (-1);
	}
    } while (whitespace (c));

    /* fill in key */
    cur = key;
    max = key + keysize;
    while (!whitespace (c) && c != ';')
    {
	if (cur >= max)
	{
	    key = xrealloc (key, keysize + ALLOCINCR);
	    cur = key + keysize;
	    keysize += ALLOCINCR;
	    max = key + keysize;
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
    if (cur >= max)
    {
	key = xrealloc (key, keysize + ALLOCINCR);
	cur = key + keysize;
	keysize += ALLOCINCR;
	max = key + keysize;
    }
    *cur = '\0';

    /* skip whitespace between key and val */
    while (whitespace (c))
    {
	c = getc (fp);
	if (c == EOF)
	{
	    *keyp = (char *) NULL;
	    *valp = (char *) NULL;
	    return (-1);
	}
    } 

    /* if we ended key with a semicolon, there is no value */
    if (c == ';')
    {
	*keyp = key;
	*valp = (char *) NULL;
	return (0);
    }

    /* otherwise, there might be a value, so fill it in */
    cur = value;
    max = value + valsize;

    /* process the value */
    for (;;)
    {
	/* handle RCS "strings" */
	if (c == '@') 
	{
	    for (;;)
	    {
		c = getc (fp);
		if (c == EOF)
		{
		    *keyp = (char *) NULL;
		    *valp = (char *) NULL;
		    return (-1);
		}

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
			break;
		}

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

	/* The syntax for some key-value pairs is different; they
	   don't end with a semicolon.  */
	if (strcmp (key, RCSDESC) == 0
	    || strcmp (key, "text") == 0
	    || strcmp (key, "log") == 0)
	    break;

	/* compress whitespace down to a single space */
	if (whitespace (c))
	{
	    do {
		c = getc (fp);
		if (c == EOF)
		{
		    *keyp = (char *) NULL;
		    *valp = (char *) NULL;
		    return (-1);
		}
	    } while (whitespace (c));

	    if (cur >= max)
	    {
		value = xrealloc (value, valsize + ALLOCINCR);
		cur = value + valsize;
		valsize += ALLOCINCR;
		max = value + valsize;
	    }
	    *cur++ = ' ';
	}

	/* if we got a semi-colon we are done with the entire value */
	if (c == ';')
	    break;

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

    /* terminate the string */
    if (cur >= max)
    {
	value = xrealloc (value, valsize + ALLOCINCR);
	cur = value + valsize;
	valsize += ALLOCINCR;
	max = value + valsize;
    }
    *cur = '\0';

    /* if the string is empty, make it null */
    if (value && *value != '\0')
	*valp = value;
    else
	*valp = NULL;
    *keyp = key;
    return (0);
}

static void getrcsrev PROTO ((FILE *fp, char **revp));

/* Read an RCS revision number from FP.  Put a pointer to it in *REVP;
   it points to space managed by getrcsrev which is only good until
   the next call to getrcskey or getrcsrev.  */
static void
getrcsrev (fp, revp)
    FILE *fp;
    char **revp;
{
    char *cur;
    char *max;
    int c;

    do {
	c = getc (fp);
	if (c == EOF)
	    /* FIXME: should be including filename in error message.  */
	    error (1, errno, "cannot read rcs file");
    } while (whitespace (c));

    if (!(isdigit (c) || c == '.'))
	/* FIXME: should be including filename in error message.  */
	error (1, 0, "error reading rcs file; revision number expected");

    cur = key;
    max = key + keysize;
    while (isdigit (c) || c == '.')
    {
	if (cur >= max)
	{
	    key = xrealloc (key, keysize + ALLOCINCR);
	    cur = key + keysize;
	    keysize += ALLOCINCR;
	    max = key + keysize;
	}
	*cur++ = c;

	c = getc (fp);
	if (c == EOF)
	{
	    /* FIXME: should be including filename in error message.  */
	    error (1, errno, "cannot read rcs file");
	}
    }

    if (cur >= max)
    {
	key = xrealloc (key, keysize + ALLOCINCR);
	cur = key + keysize;
	keysize += ALLOCINCR;
	max = key + keysize;
    }
    *cur = '\0';
    *revp = key;
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
	while (whitespace (*cp))
	    cp++;

	/* if we got to the end, we are done */
	if (*cp == '\0')
	    break;

	/* split it up into tag and rev */
	tag = cp;
	cp = strchr (cp, ':');
	*cp++ = '\0';
	rev = cp;
	while (!whitespace (*cp) && *cp != '\0')
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
	while (whitespace (*cp))
	    cp++;

	/* if we got to the end, we are done */
	if (*cp == '\0')
	    break;

	/* find the end of this branch */
	branch = cp;
	while (!whitespace (*cp) && *cp != '\0')
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
RCS_getversion (rcs, tag, date, force_tag_match, return_both)
    RCSNode *rcs;
    char *tag;
    char *date;
    int force_tag_match;
    int return_both;
{
    /* make sure we have something to look at... */
    assert (rcs != NULL);

    if (tag && date)
    {
	char *cp, *rev, *tagrev;

	/*
	 * first lookup the tag; if that works, turn the revision into
	 * a branch and lookup the date.
	 */
	tagrev = RCS_gettag (rcs, tag, force_tag_match, 0);
	if (tagrev == NULL)
	    return ((char *) NULL);

	if ((cp = strrchr (tagrev, '.')) != NULL)
	    *cp = '\0';
	rev = RCS_getdatebranch (rcs, date, tagrev);
	free (tagrev);
	return (rev);
    }
    else if (tag)
	return (RCS_gettag (rcs, tag, force_tag_match, return_both));
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
RCS_gettag (rcs, symtag, force_tag_match, return_both)
    RCSNode *rcs;
    char *symtag;
    int force_tag_match;
    int return_both;
{
    Node *p;
    char *tag = symtag;

    /* make sure we have something to look at... */
    assert (rcs != NULL);

    /* XXX this is probably not necessary, --jtc */
    if (rcs->flags & PARTIAL) 
	RCS_reparsercsfile (rcs, NULL);

    /* If tag is "HEAD", special case to get head RCS revision */
    if (tag && (strcmp (tag, TAG_HEAD) == 0 || *tag == '\0'))
#if 0 /* This #if 0 is only in the Cygnus code.  Why?  Death support?  */
	if (force_tag_match && (rcs->flags & VALID) && (rcs->flags & INATTIC))
	    return ((char *) NULL);	/* head request for removed file */
	else
#endif
	    return (RCS_head (rcs));

    if (!isdigit (tag[0]))
    {
	/* If we got a symbolic tag, resolve it to a numeric */
	if (rcs == NULL)
	    p = NULL;
	else {
	    p = findnode (RCS_symbols(rcs), tag);
	}
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
		branch = strrchr (tag, '.');
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
	{
	    /*
	     * we have found a numeric revision for the revision tag.
	     * To support expanding the RCS keyword Name, return both
	     * the numeric tag and the supplied tag (which might be
	     * symbolic).  They are separated with a ':' which is not
	     * a valid tag char.  The variable return_both is only set
	     * if this function is called through Version_TS ->
	     * RCS_getversion.
	     */
	    if (return_both)
	    {
		char *both = xmalloc(strlen(tag) + 2 + strlen(symtag));
		sprintf(both, "%s:%s", tag, symtag);
		return both;
	    }
	    else
		return (xstrdup (tag));
	}
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
	if (walklist (RCS_symbols(rcs), checkmagic_proc, NULL) != 0)
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
checkmagic_proc (p, closure)
    Node *p;
    void *closure;
{
    if (strcmp (check_rev, p->data) == 0)
	return (1);
    else
	return (0);
}

/*
 * Given an RCSNode, returns non-zero if the specified revision number 
 * or symbolic tag resolves to a "branch" within the rcs file.
 *
 * FIXME: this is the same as RCS_nodeisbranch except for the special 
 *        case for handling a null rcsnode.
 */
int
RCS_isbranch (rcs, rev)
    RCSNode *rcs;
    const char *rev;
{
    /* numeric revisions are easy -- even number of dots is a branch */
    if (isdigit (*rev))
	return ((numdots (rev) & 1) == 0);

    /* assume a revision if you can't find the RCS info */
    if (rcs == NULL)
	return (0);

    /* now, look for a match in the symbols list */
    return (RCS_nodeisbranch (rcs, rev));
}

/*
 * Given an RCSNode, returns non-zero if the specified revision number
 * or symbolic tag resolves to a "branch" within the rcs file.  We do
 * take into account any magic branches as well.
 */
int
RCS_nodeisbranch (rcs, rev)
    RCSNode *rcs;
    const char *rev;
{
    int dots;
    Node *p;

    /* numeric revisions are easy -- even number of dots is a branch */
    if (isdigit (*rev))
	return ((numdots (rev) & 1) == 0);

    p = findnode (RCS_symbols(rcs), rev);
    if (p == NULL)
	return (0);
    dots = numdots (p->data);
    if ((dots & 1) == 0)
	return (1);

    /* got a symbolic tag match, but it's not a branch; see if it's magic */
    if (dots > 2)
    {
	char *magic;
	char *branch = strrchr (p->data, '.');
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
RCS_whatbranch (rcs, rev)
    RCSNode *rcs;
    const char *rev;
{
    Node *p;
    int dots;

    /* assume no branch if you can't find the RCS info */
    if (rcs == NULL)
	return ((char *) NULL);

    /* now, look for a match in the symbols list */
    p = findnode (RCS_symbols(rcs), rev);
    if (p == NULL)
	return ((char *) NULL);
    dots = numdots (p->data);
    if ((dots & 1) == 0)
	return (xstrdup (p->data));

    /* got a symbolic tag match, but it's not a branch; see if it's magic */
    if (dots > 2)
    {
	char *magic;
	char *branch = strrchr (p->data, '.');
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
char *
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
    assert (rcs != NULL);

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    /* find out if the tag contains a dot, or is on the trunk */
    cp = strrchr (tag, '.');

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
    assert (rcs != NULL);

    /*
     * NOTE: we call getbranch with force_tag_match set to avoid any
     * possibility of recursion
     */
    if (rcs->branch)
	return (RCS_getbranch (rcs, rcs->branch, 1));
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
    assert (rcs != NULL);

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

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
    cp = strrchr (xrev, '.');
    if (cp == NULL)
    {
	free (xrev);
	return (NULL);
    }
    *cp = '\0';				/* turn it into a revision */

    assert (rcs != NULL);

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

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
    assert (rcs != NULL);

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

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

    /* If the year is from 1900 to 1999, RCS files contain only two
       digits, and sscanf gives us a year from 0-99.  If the year is
       2000+, RCS files contain all four digits and we subtract 1900,
       because the tm_year field should contain years since 1900.  */

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

List *
RCS_symbols(rcs)
    RCSNode *rcs;
{
    assert(rcs != NULL);

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    if (rcs->symbols_data) {
	rcs->symbols = getlist ();
	do_symbols (rcs->symbols, rcs->symbols_data);
	free(rcs->symbols_data);
	rcs->symbols_data = NULL;
    }

    return rcs->symbols;
}

/*
 * The argument ARG is the getopt remainder of the -k option specified on the
 * command line.  This function returns malloc'ed space that can be used
 * directly in calls to RCS V5, with the -k flag munged correctly.
 */
char *
RCS_check_kflag (arg)
    const char *arg;
{
    static const char *const kflags[] =
    {"kv", "kvl", "k", "v", "o", "b", (char *) NULL};
    static const char *const  keyword_usage[] =
    {
      "%s %s: invalid RCS keyword expansion mode\n",
      "Valid expansion modes include:\n",
      "   -kkv\tGenerate keywords using the default form.\n",
      "   -kkvl\tLike -kkv, except locker's name inserted.\n",
      "   -kk\tGenerate only keyword names in keyword strings.\n",
      "   -kv\tGenerate only keyword values in keyword strings.\n",
      "   -ko\tGenerate the old keyword string (no changes from checked in file).\n",
      "   -kb\tGenerate binary file unmodified (merges not allowed) (RCS 5.7).\n",
      NULL,
    };
    char karg[10];
    char const *const *cpp = NULL;

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
	usage (keyword_usage);
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
    const char *tag;
{
    char *invalid = "$,.:;@";		/* invalid RCS tag characters */
    const char *cp;

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
	    if (strchr (invalid, *cp))
		error (1, 0, "tag `%s' must not contain the characters `%s'",
		       tag, invalid);
	}
    }
    else
	error (1, 0, "tag `%s' must start with a letter", tag);
}

/*
 * Return true if RCS revision with TAG is a dead revision.
 */
int
RCS_isdead (rcs, tag)
    RCSNode *rcs;
    const char *tag;
{
    Node *p;
    RCSVers *version;

    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);

    p = findnode (rcs->versions, tag);
    if (p == NULL)
	return (0);

    version = (RCSVers *) p->data;
    return (version->dead);
}

/* Return the RCS keyword expansion mode.  For example "b" for binary.
   Returns a pointer into storage which is allocated and freed along with
   the rest of the RCS information; the caller should not modify this
   storage.  Returns NULL if the RCS file does not specify a keyword
   expansion mode; for all other errors, die with a fatal error.  */
char *
RCS_getexpand (rcs)
    RCSNode *rcs;
{
    assert (rcs != NULL);
    if (rcs->flags & PARTIAL)
	RCS_reparsercsfile (rcs, NULL);
    return rcs->expand;
}

/* Stuff related to annotate command.  This should perhaps be split
   into the stuff which knows about the guts of RCS files, and the
   command parsing type stuff.  */

/* Linked list of allocated blocks.  Seems kind of silly to
   reinvent the obstack wheel, and this isn't as nice as obstacks
   in some ways, but obstacks are pretty baroque.  */
struct allocblock
{
    char *text;
    struct allocblock *next;
};
struct allocblock *blocks;

static void *block_alloc PROTO ((size_t));

static void *
block_alloc (n)
    size_t n;
{
    struct allocblock *blk;
    blk = (struct allocblock *) xmalloc (sizeof (struct allocblock));
    blk->text = xmalloc (n);
    blk->next = blocks;
    blocks = blk;
    return blk->text;
}

static void block_free PROTO ((void));

static void
block_free ()
{
    struct allocblock *p;
    struct allocblock *q;

    p = blocks;
    while (p != NULL)
    {
	free (p->text);
	q = p->next;
	free (p);
	p = q;
    }
    blocks = NULL;
}

struct line
{
    /* Text of this line, terminated by \n or \0.  */
    char *text;
    /* Version in which it was introduced.  */
    RCSVers *vers;
    /* Nonzero if this line ends with \n.  This will always be true
       except possibly for the last line.  */
    int has_newline;
};

struct linevector
{
    /* How many lines in use for this linevector?  */
    unsigned int nlines;
    /* How many lines allocated for this linevector?  */
    unsigned int lines_alloced;
    /* Pointer to array containing a pointer to each line.  */
    struct line **vector;
};

static void linevector_init PROTO ((struct linevector *));

/* Initialize *VEC to be a linevector with no lines.  */
static void
linevector_init (vec)
    struct linevector *vec;
{
    vec->lines_alloced = 10;
    vec->nlines = 0;
    vec->vector = (struct line **)
	xmalloc (vec->lines_alloced * sizeof (*vec->vector));
}

static void linevector_add PROTO ((struct linevector *vec, char *text,
				   RCSVers *vers, unsigned int pos));

/* Given some text TEXT, add each of its lines to VEC before line POS
   (where line 0 is the first line).  The last line in TEXT may or may
   not be \n terminated.  All \n in TEXT are changed to \0.  Set the
   version for each of the new lines to VERS.  */
static void
linevector_add (vec, text, vers, pos)
    struct linevector *vec;
    char *text;
    RCSVers *vers;
    unsigned int pos;
{
    unsigned int i;
    unsigned int nnew;
    char *p;
    struct line *lines;

    assert (vec->lines_alloced > 0);

    /* Count the number of lines we will need to add.  */
    nnew = 1;
    for (p = text; *p != '\0'; ++p)
	if (*p == '\n' && p[1] != '\0')
	    ++nnew;
    /* Allocate the struct line's.  */
    lines = block_alloc (nnew * sizeof (struct line));

    /* Expand VEC->VECTOR if needed.  */
    if (vec->nlines + nnew >= vec->lines_alloced)
    {
	while (vec->nlines + nnew >= vec->lines_alloced)
	    vec->lines_alloced *= 2;
	vec->vector = xrealloc (vec->vector,
				vec->lines_alloced * sizeof (*vec->vector));
    }

    /* Make room for the new lines in VEC->VECTOR.  */
    for (i = vec->nlines + nnew - 1; i >= pos + nnew; --i)
	vec->vector[i] = vec->vector[i - nnew];

    if (pos > vec->nlines)
	error (1, 0, "invalid rcs file: line to add out of range");

    /* Actually add the lines, to LINES and VEC->VECTOR.  */
    i = pos;
    lines[0].text = text;
    lines[0].vers = vers;
    lines[0].has_newline = 0;
    vec->vector[i++] = &lines[0];
    for (p = text; *p != '\0'; ++p)
	if (*p == '\n')
	{
	    *p = '\0';
	    lines[i - pos - 1].has_newline = 1;
	    if (p[1] == '\0')
		/* If there are no characters beyond the last newline, we
		   don't consider it another line.  */
		break;
	    lines[i - pos].text = p + 1;
	    lines[i - pos].vers = vers;
	    lines[i - pos].has_newline = 0;
	    vec->vector[i] = &lines[i - pos];
	    ++i;
	}
    vec->nlines += nnew;
}

static void linevector_delete PROTO ((struct linevector *, unsigned int,
				      unsigned int));

/* Remove NLINES lines from VEC at position POS (where line 0 is the
   first line).  */
static void
linevector_delete (vec, pos, nlines)
    struct linevector *vec;
    unsigned int pos;
    unsigned int nlines;
{
    unsigned int i;
    unsigned int last;

    last = vec->nlines - nlines;
    for (i = pos; i < last; ++i)
	vec->vector[i] = vec->vector[i + nlines];
    vec->nlines -= nlines;
}

static void linevector_copy PROTO ((struct linevector *, struct linevector *));

/* Copy FROM to TO, copying the vectors but not the lines pointed to.  */
static void
linevector_copy (to, from)
    struct linevector *to;
    struct linevector *from;
{
    if (from->nlines > to->lines_alloced)
    {
	while (from->nlines > to->lines_alloced)
	    to->lines_alloced *= 2;
	to->vector = (struct line **)
	    xrealloc (to->vector, to->lines_alloced * sizeof (*to->vector));
    }
    memcpy (to->vector, from->vector,
	    from->nlines * sizeof (*to->vector));
    to->nlines = from->nlines;
}

static void linevector_free PROTO ((struct linevector *));

/* Free storage associated with linevector (that is, the vector but
   not the lines pointed to).  */
static void
linevector_free (vec)
    struct linevector *vec;
{
   free (vec->vector);
}

static char *month_printname PROTO ((char *));

/* Given a textual string giving the month (1-12), terminated with any
   character not recognized by atoi, return the 3 character name to
   print it with.  I do not think it is a good idea to change these
   strings based on the locale; they are standard abbreviations (for
   example in rfc822 mail messages) which should be widely understood.
   Returns a pointer into static readonly storage.  */
static char *
month_printname (month)
    char *month;
{
    static const char *const months[] =
      {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int mnum;

    mnum = atoi (month);
    if (mnum < 1 || mnum > 12)
	return "???";
    return (char *)months[mnum - 1];
}

static int annotate_fileproc PROTO ((struct file_info *));

static int
annotate_fileproc (finfo)
    struct file_info *finfo;
{
    FILE *fp;
    char *key;
    char *value;
    RCSVers *vers;
    RCSVers *prev_vers;
    int n;
    int ishead;
    Node *node;
    struct linevector headlines;
    struct linevector curlines;

    if (finfo->rcs == NULL)
        return (1);

    /* Distinguish output for various files if we are processing
       several files.  */
    cvs_outerr ("Annotations for ", 0);
    cvs_outerr (finfo->fullname, 0);
    cvs_outerr ("\n***************\n", 0);

    if (!(finfo->rcs->flags & PARTIAL))
	/* We are leaking memory by calling RCS_reparsefile again.  */
	error (0, 0, "internal warning: non-partial rcs in annotate_fileproc");
    RCS_reparsercsfile (finfo->rcs, &fp);

    ishead = 1;
    vers = NULL;

    do {
	getrcsrev (fp, &key);

	/* Stash the previous version.  */
	prev_vers = vers;

	/* look up the revision */
	node = findnode (finfo->rcs->versions, key);
	if (node == NULL)
	    error (1, 0, "mismatch in rcs file %s between deltas and deltatexts",
		   finfo->rcs->path);
	vers = (RCSVers *) node->data;

	while ((n = getrcskey (fp, &key, &value)) >= 0)
	{
	    if (strcmp (key, "text") == 0)
	    {
		if (ishead)
		{
		    char *p;

		    p = block_alloc (strlen (value) + 1);
		    strcpy (p, value);

		    linevector_init (&headlines);
		    linevector_init (&curlines);
		    linevector_add (&headlines, p, NULL, 0);
		    linevector_copy (&curlines, &headlines);
		    ishead = 0;
		}
		else
		{
		    char *p;
		    char *q;
		    int op;
		    /* The RCS format throws us for a loop in that the
		       deltafrags (if we define a deltafrag as an
		       add or a delete) need to be applied in reverse
		       order.  So we stick them into a linked list.  */
		    struct deltafrag {
			enum {ADD, DELETE} type;
			unsigned long pos;
			unsigned long nlines;
			char *new_lines;
			struct deltafrag *next;
		    };
		    struct deltafrag *dfhead;
		    struct deltafrag *df;

		    dfhead = NULL;
		    for (p = value; p != NULL && *p != '\0'; )
		    {
			op = *p++;
			if (op != 'a' && op != 'd')
			    /* Can't just skip over the deltafrag, because
			       the value of op determines the syntax.  */
			    error (1, 0, "unrecognized operation '%c' in %s",
				   op, finfo->rcs->path);
			df = (struct deltafrag *)
			    xmalloc (sizeof (struct deltafrag));
			df->next = dfhead;
			dfhead = df;
			df->pos = strtoul (p, &q, 10);

			if (p == q)
			    error (1, 0, "number expected in %s",
				   finfo->rcs->path);
			p = q;
			if (*p++ != ' ')
			    error (1, 0, "space expected in %s",
				   finfo->rcs->path);
			df->nlines = strtoul (p, &q, 10);
			if (p == q)
			    error (1, 0, "number expected in %s",
				   finfo->rcs->path);
			p = q;
			if (*p++ != '\012')
			    error (1, 0, "linefeed expected in %s",
				   finfo->rcs->path);

			if (op == 'a')
			{
			    unsigned int i;

			    df->type = ADD;
			    i = df->nlines;
			    /* The text we want is the number of lines
			       specified, or until the end of the value,
			       whichever comes first (it will be the former
			       except in the case where we are adding a line
			       which does not end in newline).  */
			    for (q = p; i != 0; ++q)
				if (*q == '\n')
				    --i;
				else if (*q == '\0')
				{
				    if (i != 1)
					error (1, 0, "\
invalid rcs file %s: premature end of value",
					       finfo->rcs->path);
				    else
					break;
				}

			    /* Copy the text we are adding into allocated
			       space.  */
			    df->new_lines = block_alloc (q - p + 1);
			    strncpy (df->new_lines, p, q - p);
			    df->new_lines[q - p] = '\0';

			    p = q;
			}
			else
			{
			    /* Correct for the fact that line numbers in RCS
			       files start with 1.  */
			    --df->pos;

			    assert (op == 'd');
			    df->type = DELETE;
			}
		    }
		    for (df = dfhead; df != NULL;)
		    {
			unsigned int ln;

			switch (df->type)
			{
			case ADD:
			    linevector_add (&curlines, df->new_lines,
					    NULL, df->pos);
			    break;
			case DELETE:
			    if (df->pos > curlines.nlines
				|| df->pos + df->nlines > curlines.nlines)
				error (1, 0, "\
invalid rcs file %s (`d' operand out of range)",
				       finfo->rcs->path);
			    for (ln = df->pos; ln < df->pos + df->nlines; ++ln)
				curlines.vector[ln]->vers = prev_vers;
			    linevector_delete (&curlines, df->pos, df->nlines);
			    break;
			}
		        df = df->next;
			free (dfhead);
			dfhead = df;
		    }
		}
		break;
	    }
	}
	if (n < 0)
	    goto l_error;
    } while (vers->next != NULL);

    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", finfo->rcs->path);

    /* Now print out the data we have just computed.  */
    {
	unsigned int ln;

	for (ln = 0; ln < headlines.nlines; ++ln)
	{
	    char buf[80];
	    /* Period which separates year from month in date.  */
	    char *ym;
	    /* Period which separates month from day in date.  */
	    char *md;
	    RCSVers *prvers;

	    prvers = headlines.vector[ln]->vers;
	    if (prvers == NULL)
		prvers = vers;

	    sprintf (buf, "%-12s (%-8.8s ",
		     prvers->version,
		     prvers->author);
	    cvs_output (buf, 0);

	    /* Now output the date.  */
	    ym = strchr (prvers->date, '.');
	    if (ym == NULL)
		cvs_output ("??-???-??", 0);
	    else
	    {
		md = strchr (ym + 1, '.');
		if (md == NULL)
		    cvs_output ("??", 0);
		else
		    cvs_output (md + 1, 2);

		cvs_output ("-", 1);
		cvs_output (month_printname (ym + 1), 0);
		cvs_output ("-", 1);
		/* Only output the last two digits of the year.  Our output
		   lines are long enough as it is without printing the
		   century.  */
		cvs_output (ym - 2, 2);
	    }
	    cvs_output ("): ", 0);
	    cvs_output (headlines.vector[ln]->text, 0);
	    cvs_output ("\n", 1);
	}
    }

    if (!ishead)
    {
	linevector_free (&curlines);
	linevector_free (&headlines);
    }
    block_free ();
    return 0;

  l_error:
    if (ferror (fp))
	error (1, errno, "cannot read %s", finfo->rcs->path);
    else
        error (1, 0, "%s does not appear to be a valid rcs file",
	       finfo->rcs->path);
    /* Shut up gcc -Wall.  */
    return 0;
}

static const char *const annotate_usage[] =
{
    "Usage: %s %s [-l] [files...]\n",
    "\t-l\tLocal directory only, no recursion.\n",
    NULL
};

/* Command to show the revision, date, and author where each line of a
   file was modified.  Currently it will only show the trunk, all the
   way to the head, but it would be useful to enhance it to (a) allow
   one to specify a revision, and display only as far as that (easy;
   just have annotate_fileproc set all the ->vers fields to NULL when
   you hit that revision), and (b) handle branches (not as easy, but
   doable).  The user interface for both (a) and (b) could be a -r
   option.  */

int
annotate (argc, argv)
    int argc;
    char **argv;
{
    int local = 0;
    int c;

    if (argc == -1)
	usage (annotate_usage);

    optind = 0;
    while ((c = getopt (argc, argv, "+l")) != -1)
    {
	switch (c)
	{
	    case 'l':
		local = 1;
		break;
	    case '?':
	    default:
		usage (annotate_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

#ifdef CLIENT_SUPPORT
    if (client_active)
    {
	start_server ();
	ign_setup ();

	if (local)
	    send_arg ("-l");
	send_file_names (argc, argv, SEND_EXPAND_WILD);
	/* FIXME:  We shouldn't have to send current files, but I'm not sure
	   whether it works.  So send the files --
	   it's slower but it works.  */
	send_files (argc, argv, local, 0);
	send_to_server ("annotate\012", 0);
	return get_responses_and_close ();
    }
#endif /* CLIENT_SUPPORT */

    return start_recursion (annotate_fileproc, (FILESDONEPROC) NULL,
			    (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL,
			    argc, argv, local, W_LOCAL, 0, 1, (char *)NULL,
			    1, 0);
}
