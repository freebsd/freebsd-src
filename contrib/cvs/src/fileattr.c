/* Implementation for file attribute munging features.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include "cvs.h"
#include "getline.h"
#include "fileattr.h"
#include <assert.h>

static void fileattr_read PROTO ((void));
static int writeattr_proc PROTO ((Node *, void *));

/* Where to look for CVSREP_FILEATTR.  */
static char *fileattr_stored_repos;

/* The in-memory attributes.  */
static List *attrlist;
static char *fileattr_default_attrs;
/* We have already tried to read attributes and failed in this directory
   (for example, there is no CVSREP_FILEATTR file).  */
static int attr_read_attempted;

/* Have the in-memory attributes been modified since we read them?  */
static int attrs_modified;

/* More in-memory attributes: linked list of unrecognized
   fileattr lines.  We pass these on unchanged.  */
struct unrecog {
    char *line;
    struct unrecog *next;
};
static struct unrecog *unrecog_head;

/* Note that if noone calls fileattr_get, this is very cheap.  No stat(),
   no open(), no nothing.  */
void
fileattr_startdir (repos)
    char *repos;
{
    assert (fileattr_stored_repos == NULL);
    fileattr_stored_repos = xstrdup (repos);
    assert (attrlist == NULL);
    attr_read_attempted = 0;
    assert (unrecog_head == NULL);
}

static void
fileattr_delproc (node)
    Node *node;
{
    assert (node->data != NULL);
    free (node->data);
    node->data = NULL;
}

/* Read all the attributes for the current directory into memory.  */
static void
fileattr_read ()
{
    char *fname;
    FILE *fp;
    char *line = NULL;
    size_t line_len = 0;

    /* If there are no attributes, don't waste time repeatedly looking
       for the CVSREP_FILEATTR file.  */
    if (attr_read_attempted)
	return;

    /* If NULL was passed to fileattr_startdir, then it isn't kosher to look
       at attributes.  */
    assert (fileattr_stored_repos != NULL);

    fname = xmalloc (strlen (fileattr_stored_repos)
		     + 1
		     + sizeof (CVSREP_FILEATTR)
		     + 1);

    strcpy (fname, fileattr_stored_repos);
    strcat (fname, "/");
    strcat (fname, CVSREP_FILEATTR);

    attr_read_attempted = 1;
    fp = CVS_FOPEN (fname, FOPEN_BINARY_READ);
    if (fp == NULL)
    {
	if (!existence_error (errno))
	    error (0, errno, "cannot read %s", fname);
	free (fname);
	return;
    }
    attrlist = getlist ();
    while (1) {
	int nread;
	nread = getline (&line, &line_len, fp);
	if (nread < 0)
	    break;
	/* Remove trailing newline.  */
	line[nread - 1] = '\0';
	if (line[0] == 'F')
	{
	    char *p;
	    Node *newnode;

	    p = strchr (line, '\t');
	    *p++ = '\0';
	    newnode = getnode ();
	    newnode->type = FILEATTR;
	    newnode->delproc = fileattr_delproc;
	    newnode->key = xstrdup (line + 1);
	    newnode->data = xstrdup (p);
	    if (addnode (attrlist, newnode) != 0)
		/* If the same filename appears twice in the file, discard
		   any line other than the first for that filename.  This
		   is the way that CVS has behaved since file attributes
		   were first introduced.  */
		free (newnode);
	}
	else if (line[0] == 'D')
	{
	    char *p;
	    /* Currently nothing to skip here, but for future expansion,
	       ignore anything located here.  */
	    p = strchr (line, '\t');
	    ++p;
	    fileattr_default_attrs = xstrdup (p);
	}
	else
	{
	    /* Unrecognized type, we want to just preserve the line without
	       changing it, for future expansion.  */
	    struct unrecog *new;

	    new = (struct unrecog *) xmalloc (sizeof (struct unrecog));
	    new->line = xstrdup (line);
	    new->next = unrecog_head;
	    unrecog_head = new;
	}
    }
    if (ferror (fp))
	error (0, errno, "cannot read %s", fname);
    if (line != NULL)
	free (line);
    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", fname);
    attrs_modified = 0;
    free (fname);
}

char *
fileattr_get (filename, attrname)
    const char *filename;
    const char *attrname;
{
    Node *node;
    size_t attrname_len = strlen (attrname);
    char *p;

    if (attrlist == NULL)
	fileattr_read ();
    if (attrlist == NULL)
	/* Either nothing has any attributes, or fileattr_read already printed
	   an error message.  */
	return NULL;

    if (filename == NULL)
	p = fileattr_default_attrs;
    else
    {
	node = findnode (attrlist, filename);
	if (node == NULL)
	    /* A file not mentioned has no attributes.  */
	    return NULL;
	p = node->data;
    }
    while (p)
    {
	if (strncmp (attrname, p, attrname_len) == 0
	    && p[attrname_len] == '=')
	{
	    /* Found it.  */
	    return p + attrname_len + 1;
	}
	p = strchr (p, ';');
	if (p == NULL)
	    break;
	++p;
    }
    /* The file doesn't have this attribute.  */
    return NULL;
}

char *
fileattr_get0 (filename, attrname)
    const char *filename;
    const char *attrname;
{
    char *cp;
    char *cpend;
    char *retval;

    cp = fileattr_get (filename, attrname);
    if (cp == NULL)
	return NULL;
    cpend = strchr (cp, ';');
    if (cpend == NULL)
	cpend = cp + strlen (cp);
    retval = xmalloc (cpend - cp + 1);
    strncpy (retval, cp, cpend - cp);
    retval[cpend - cp] = '\0';
    return retval;
}

char *
fileattr_modify (list, attrname, attrval, namevalsep, entsep)
    char *list;
    const char *attrname;
    const char *attrval;
    int namevalsep;
    int entsep;
{
    char *retval;
    char *rp;
    size_t attrname_len = strlen (attrname);

    /* Portion of list before the attribute to be replaced.  */
    char *pre;
    char *preend;
    /* Portion of list after the attribute to be replaced.  */
    char *post;

    char *p;
    char *p2;

    p = list;
    pre = list;
    preend = NULL;
    /* post is NULL unless set otherwise.  */
    post = NULL;
    p2 = NULL;
    if (list != NULL)
    {
	while (1) {
	    p2 = strchr (p, entsep);
	    if (p2 == NULL)
	    {
		p2 = p + strlen (p);
		if (preend == NULL)
		    preend = p2;
	    }
	    else
		++p2;
	    if (strncmp (attrname, p, attrname_len) == 0
		&& p[attrname_len] == namevalsep)
	    {
		/* Found it.  */
		preend = p;
		if (preend > list)
		    /* Don't include the preceding entsep.  */
		    --preend;

		post = p2;
	    }
	    if (p2[0] == '\0')
		break;
	    p = p2;
	}
    }
    if (post == NULL)
	post = p2;

    if (preend == pre && attrval == NULL && post == p2)
	return NULL;

    retval = xmalloc ((preend - pre)
		      + 1
		      + (attrval == NULL ? 0 : (attrname_len + 1
						+ strlen (attrval)))
		      + 1
		      + (p2 - post)
		      + 1);
    if (preend != pre)
    {
	strncpy (retval, pre, preend - pre);
	rp = retval + (preend - pre);
	if (attrval != NULL)
	    *rp++ = entsep;
	*rp = '\0';
    }
    else
	retval[0] = '\0';
    if (attrval != NULL)
    {
	strcat (retval, attrname);
	rp = retval + strlen (retval);
	*rp++ = namevalsep;
	strcpy (rp, attrval);
    }
    if (post != p2)
    {
	rp = retval + strlen (retval);
	if (preend != pre || attrval != NULL)
	    *rp++ = entsep;
	strncpy (rp, post, p2 - post);
	rp += p2 - post;
	*rp = '\0';
    }
    return retval;
}

void
fileattr_set (filename, attrname, attrval)
    const char *filename;
    const char *attrname;
    const char *attrval;
{
    Node *node;
    char *p;

    if (filename == NULL)
    {
	p = fileattr_modify (fileattr_default_attrs, attrname, attrval,
			     '=', ';');
	if (fileattr_default_attrs != NULL)
	    free (fileattr_default_attrs);
	fileattr_default_attrs = p;
	attrs_modified = 1;
	return;
    }
    if (attrlist == NULL)
	fileattr_read ();
    if (attrlist == NULL)
    {
	/* Not sure this is a graceful way to handle things
	   in the case where fileattr_read was unable to read the file.  */
        /* No attributes existed previously.  */
	attrlist = getlist ();
    }

    node = findnode (attrlist, filename);
    if (node == NULL)
    {
	if (attrval == NULL)
	    /* Attempt to remove an attribute which wasn't there.  */
	    return;

	/* First attribute for this file.  */
	node = getnode ();
	node->type = FILEATTR;
	node->delproc = fileattr_delproc;
	node->key = xstrdup (filename);
	node->data = xmalloc (strlen (attrname) + 1 + strlen (attrval) + 1);
	strcpy (node->data, attrname);
	strcat (node->data, "=");
	strcat (node->data, attrval);
	addnode (attrlist, node);
    }

    p = fileattr_modify (node->data, attrname, attrval, '=', ';');
    if (p == NULL)
	delnode (node);
    else
    {
	free (node->data);
	node->data = p;
    }

    attrs_modified = 1;
}

void
fileattr_newfile (filename)
    const char *filename;
{
    Node *node;

    if (attrlist == NULL)
	fileattr_read ();

    if (fileattr_default_attrs == NULL)
	return;

    if (attrlist == NULL)
    {
	/* Not sure this is a graceful way to handle things
	   in the case where fileattr_read was unable to read the file.  */
        /* No attributes existed previously.  */
	attrlist = getlist ();
    }

    node = getnode ();
    node->type = FILEATTR;
    node->delproc = fileattr_delproc;
    node->key = xstrdup (filename);
    node->data = xstrdup (fileattr_default_attrs);
    addnode (attrlist, node);
    attrs_modified = 1;
}

static int
writeattr_proc (node, data)
    Node *node;
    void *data;
{
    FILE *fp = (FILE *)data;
    fputs ("F", fp);
    fputs (node->key, fp);
    fputs ("\t", fp);
    fputs (node->data, fp);
    fputs ("\012", fp);
    return 0;
}

void
fileattr_write ()
{
    FILE *fp;
    char *fname;
    mode_t omask;

    if (!attrs_modified)
	return;

    if (noexec)
	return;

    /* If NULL was passed to fileattr_startdir, then it isn't kosher to set
       attributes.  */
    assert (fileattr_stored_repos != NULL);

    fname = xmalloc (strlen (fileattr_stored_repos)
		     + 1
		     + sizeof (CVSREP_FILEATTR)
		     + 1);

    strcpy (fname, fileattr_stored_repos);
    strcat (fname, "/");
    strcat (fname, CVSREP_FILEATTR);

    if (list_isempty (attrlist)
	&& fileattr_default_attrs == NULL
	&& unrecog_head == NULL)
    {
	/* There are no attributes.  */
	if (unlink_file (fname) < 0)
	{
	    if (!existence_error (errno))
	    {
		error (0, errno, "cannot remove %s", fname);
	    }
	}

	/* Now remove CVSREP directory, if empty.  The main reason we bother
	   is that CVS 1.6 and earlier will choke if a CVSREP directory
	   exists, so provide the user a graceful way to remove it.  */
	strcpy (fname, fileattr_stored_repos);
	strcat (fname, "/");
	strcat (fname, CVSREP);
	if (CVS_RMDIR (fname) < 0)
	{
	    if (errno != ENOTEMPTY

		/* Don't know why we would be here if there is no CVSREP
		   directory, but it seemed to be happening anyway, so
		   check for it.  */
		&& !existence_error (errno))
		error (0, errno, "cannot remove %s", fname);
	}

	free (fname);
	return;
    }

    omask = umask (cvsumask);
    fp = CVS_FOPEN (fname, FOPEN_BINARY_WRITE);
    if (fp == NULL)
    {
	if (existence_error (errno))
	{
	    /* Maybe the CVSREP directory doesn't exist.  Try creating it.  */
	    char *repname;

	    repname = xmalloc (strlen (fileattr_stored_repos)
			       + 1
			       + sizeof (CVSREP)
			       + 1);
	    strcpy (repname, fileattr_stored_repos);
	    strcat (repname, "/");
	    strcat (repname, CVSREP);

	    if (CVS_MKDIR (repname, 0777) < 0 && errno != EEXIST)
	    {
		error (0, errno, "cannot make directory %s", repname);
		(void) umask (omask);
		free (repname);
		return;
	    }
	    free (repname);

	    fp = CVS_FOPEN (fname, FOPEN_BINARY_WRITE);
	}
	if (fp == NULL)
	{
	    error (0, errno, "cannot write %s", fname);
	    (void) umask (omask);
	    return;
	}
    }
    (void) umask (omask);

    /* First write the "F" attributes.  */
    walklist (attrlist, writeattr_proc, fp);

    /* Then the "D" attribute.  */
    if (fileattr_default_attrs != NULL)
    {
	fputs ("D\t", fp);
	fputs (fileattr_default_attrs, fp);
	fputs ("\012", fp);
    }

    /* Then any other attributes.  */
    while (unrecog_head != NULL)
    {
	struct unrecog *p;

	p = unrecog_head;
	fputs (p->line, fp);
	fputs ("\012", fp);

	unrecog_head = p->next;
	free (p->line);
	free (p);
    }

    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", fname);
    attrs_modified = 0;
    free (fname);
}

void
fileattr_free ()
{
    dellist (&attrlist);
    if (fileattr_stored_repos != NULL)
	free (fileattr_stored_repos);
    fileattr_stored_repos = NULL;
    if (fileattr_default_attrs != NULL)
	free (fileattr_default_attrs);
    fileattr_default_attrs = NULL;
}
