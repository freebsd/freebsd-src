/*
 * Copyright (c) 1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * A simple ndbm-emulator for CVS.  It parses a text file of the format:
 * 
 * key	value
 * 
 * at dbm_open time, and loads the entire file into memory.  As such, it is
 * probably only good for fairly small modules files.  Ours is about 30K in
 * size, and this code works fine.
 */

#include "cvs.h"

#ifdef MY_NDBM

#ifndef lint
static char rcsid[] = "@(#)myndbm.c 1.5 92/03/31";
#endif

static void mydbm_load_file ();

/* ARGSUSED */
DBM *
mydbm_open (file, flags, mode)
    char *file;
    int flags;
    int mode;
{
    FILE *fp;
    DBM *db;

    if ((fp = fopen (file, "r")) == NULL)
	return ((DBM *) 0);

    db = (DBM *) xmalloc (sizeof (*db));
    db->dbm_list = getlist ();

    mydbm_load_file (fp, db->dbm_list);
    (void) fclose (fp);
    return (db);
}

void
mydbm_close (db)
    DBM *db;
{
    dellist (&db->dbm_list);
    free ((char *) db);
}

datum
mydbm_fetch (db, key)
    DBM *db;
    datum key;
{
    Node *p;
    char *s;
    datum val;

    /* make sure it's null-terminated */
    s = xmalloc (key.dsize + 1);
    (void) strncpy (s, key.dptr, key.dsize);
    s[key.dsize] = '\0';

    p = findnode (db->dbm_list, s);
    if (p)
    {
	val.dptr = p->data;
	val.dsize = strlen (p->data);
    }
    else
    {
	val.dptr = (char *) NULL;
	val.dsize = 0;
    }
    free (s);
    return (val);
}

datum
mydbm_firstkey (db)
    DBM *db;
{
    Node *head, *p;
    datum key;

    head = db->dbm_list->list;
    p = head->next;
    if (p != head)
    {
	key.dptr = p->key;
	key.dsize = strlen (p->key);
    }
    else
    {
	key.dptr = (char *) NULL;
	key.dsize = 0;
    }
    db->dbm_next = p->next;
    return (key);
}

datum
mydbm_nextkey (db)
    DBM *db;
{
    Node *head, *p;
    datum key;

    head = db->dbm_list->list;
    p = db->dbm_next;
    if (p != head)
    {
	key.dptr = p->key;
	key.dsize = strlen (p->key);
    }
    else
    {
	key.dptr = (char *) NULL;
	key.dsize = 0;
    }
    db->dbm_next = p->next;
    return (key);
}

static void
mydbm_load_file (fp, list)
    FILE *fp;
    List *list;
{
    char line[MAXLINELEN], value[MAXLINELEN];
    char *cp, *vp;
    int len, cont;

    for (cont = 0; fgets (line, sizeof (line), fp) != NULL;)
    {
	if ((cp = rindex (line, '\n')) != NULL)
	    *cp = '\0';			/* strip the newline */

	/*
	 * Add the line to the value, at the end if this is a continuation
	 * line; otherwise at the beginning, but only after any trailing
	 * backslash is removed.
	 */
	vp = value;
	if (cont)
	    vp += strlen (value);

	/*
	 * See if the line we read is a continuation line, and strip the
	 * backslash if so.
	 */
	len = strlen (line);
	if (len > 0)
	    cp = &line[len - 1];
	else
	    cp = line;
	if (*cp == '\\')
	{
	    cont = 1;
	    *cp = '\0';
	}
	else
	{
	    cont = 0;
	}
	(void) strcpy (vp, line);
	if (value[0] == '#')
	    continue;			/* comment line */
	vp = value;
	while (*vp && isspace (*vp))
	    vp++;
	if (*vp == '\0')
	    continue;			/* empty line */

	/*
	 * If this was not a continuation line, add the entry to the database
	 */
	if (!cont)
	{
	    Node *p = getnode ();
	    char *kp;

	    kp = vp;
	    while (*vp && !isspace (*vp))
		vp++;
	    *vp++ = '\0';		/* NULL terminate the key */
	    p->type = NDBMNODE;
	    p->key = xstrdup (kp);
	    while (*vp && isspace (*vp))
		vp++;			/* skip whitespace to value */
	    if (*vp == '\0')
	    {
		error (0, 0, "warning: NULL value for key `%s'", p->key);
		freenode (p);
		continue;
	    }
	    p->data = xstrdup (vp);
	    if (addnode (list, p) == -1)
	    {
		error (0, 0, "duplicate key found for `%s'", p->key);
		freenode (p);
	    }
	}
    }
}

#endif				/* MY_NDBM */
