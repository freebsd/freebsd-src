/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/* Collect and manage hardlink info associated with a particular file.  */

#include "cvs.h"
#include "hardlink.h"

/* The structure currently used to manage hardlink info is a list.
   Therefore, most of the functions which manipulate hardlink data
   are walklist procedures.  This is not a very efficient implementation;
   if someone decides to use a real hash table (for instance), then
   much of this code can be rewritten to be a little less arcane.

   Each element of `hardlist' represents an inode.  It is keyed on the
   inode number, and points to a list of files.  This is to make it
   easy to find out what files are linked to a given file FOO: find
   FOO's inode, look it up in hardlist, and retrieve the list of files
   associated with that inode.

   Each file node, in turn, is represented by a `hardlink_info' struct,
   which includes `status' and `links' fields.  The `status' field should
   be used by a procedure like commit_fileproc or update_fileproc to
   record each file's status; that way, after all file links have been
   recorded, CVS can check the linkage of files which are in doubt
   (i.e. T_NEEDS_MERGE files).

   TODO: a diagram of an example hardlist would help here. */

/* TODO: change this to something with a marginal degree of
   efficiency, like maybe a hash table.  Yeah. */

List *hardlist;		/* Record hardlink information for working files */
char *working_dir;	/* The top-level working directory, used for
			   constructing full pathnames. */

/* Return a pointer to FILEPATH's node in the hardlist.  This means
   looking up its inode, retrieving the list of files linked to that
   inode, and then looking up FILE in that list.  If the file doesn't
   seem to exist, return NULL. */
Node *
lookup_file_by_inode (filepath)
    const char *filepath;
{
    char *inodestr, *file;
    struct stat sb;
    Node *hp, *p;

    /* Get file's basename, so that we can stat it. */
    file = strrchr (filepath, '/');
    if (file)
	++file;
    else
	file = (char *) filepath;

    /* inodestr contains the hexadecimal representation of an
       inode, so it requires two bytes of text to represent
       each byte of the inode number. */
    inodestr = (char *) xmalloc (2*sizeof(ino_t) + 1);
    if (stat (file, &sb) < 0)
    {
	if (existence_error (errno))
	{
	    /* The file doesn't exist; we may be doing an update on a
	       file that's been removed.  A nonexistent file has no
	       link information, so return without changing hardlist. */
	    free (inodestr);
	    return NULL;
	}
	error (1, errno, "cannot stat %s", file);
    }

    sprintf (inodestr, "%lx", (unsigned long) sb.st_ino);

    /* Find out if this inode is already in the hardlist, adding
       a new entry to the list if not. */
    hp = findnode (hardlist, inodestr);
    if (hp == NULL)
    {
	hp = getnode ();
	hp->type = NT_UNKNOWN;
	hp->key = inodestr;
	hp->data = getlist();
	hp->delproc = dellist;
	(void) addnode (hardlist, hp);
    }
    else
    {
	free (inodestr);
    }

    p = findnode (hp->data, filepath);
    if (p == NULL)
    {
	p = getnode();
	p->type = NT_UNKNOWN;
	p->key = xstrdup (filepath);
	p->data = NULL;
	(void) addnode (hp->data, p);
    }

    return p;
}

/* After a file has been checked out, add a node for it to the hardlist
   (if necessary) and mark it as checked out. */
void
update_hardlink_info (file)
    const char *file;
{
    char *path;
    Node *n;
    struct hardlink_info *hlinfo;

    if (file[0] == '/')
    {
	path = xstrdup (file);
    }
    else
    {
	/* file is a relative pathname; assume it's from the current
	   working directory. */
	char *dir = xgetwd();
	path = xmalloc (strlen(dir) + strlen(file) + 2);
	sprintf (path, "%s/%s", dir, file);
	free (dir);
    }

    n = lookup_file_by_inode (path);
    if (n == NULL)
    {
	/* Something is *really* wrong if the file doesn't exist here;
	   update_hardlink_info should be called only when a file has
	   just been checked out to a working directory. */
	error (1, 0, "lost hardlink info for %s", file);
    }

    if (n->data == NULL)
	n->data = xmalloc (sizeof (struct hardlink_info));
    hlinfo = n->data;
    hlinfo->status = T_UPTODATE;
    hlinfo->checked_out = 1;
}

/* Return a List with all the files known to be linked to FILE in
   the working directory.  Used by special_file_mismatch, to determine
   whether it is safe to merge two files.

   FIXME: What is the memory allocation for the return value?  We seem
   to sometimes allocate a new list (getlist() call below) and sometimes
   return an existing list (where we return n->data).  */
List *
list_linked_files_on_disk (file)
    char *file;
{
    char *inodestr, *path;
    struct stat sb;
    Node *n;

    /* If hardlist is NULL, we have not been doing an operation that
       would permit us to know anything about the file's hardlinks
       (cvs update, cvs commit, etc).  Return an empty list. */
    if (hardlist == NULL)
	return getlist();

    /* Get the full pathname of file (assuming the working directory) */
    if (file[0] == '/')
	path = xstrdup (file);
    else
    {
	char *dir = xgetwd();
	path = (char *) xmalloc (strlen(dir) + strlen(file) + 2);
	sprintf (path, "%s/%s", dir, file);
	free (dir);
    }

    /* We do an extra lookup_file here just to make sure that there
       is a node for `path' in the hardlist.  If that were not so,
       comparing the working directory linkage against the repository
       linkage for a file would always fail. */
    (void) lookup_file_by_inode (path);

    if (stat (path, &sb) < 0)
	error (1, errno, "cannot stat %s", file);
    /* inodestr contains the hexadecimal representation of an
       inode, so it requires two bytes of text to represent
       each byte of the inode number. */
    inodestr = (char *) xmalloc (2*sizeof(ino_t) + 1);
    sprintf (inodestr, "%lx", (unsigned long) sb.st_ino);

    /* Make sure the files linked to this inode are sorted. */
    n = findnode (hardlist, inodestr);
    sortlist (n->data, fsortcmp);

    free (inodestr);
    return n->data;
}

/* Compare the files in the `key' fields of two lists, returning 1 if
   the lists are equivalent and 0 otherwise.

   Only the basenames of each file are compared. This is an awful hack
   that exists because list_linked_files_on_disk returns full paths
   and the `hardlinks' structure of a RCSVers node contains only
   basenames.  That in turn is a result of the awful hack that only
   basenames are stored in the RCS file.  If anyone ever solves the
   problem of correctly managing cross-directory hardlinks, this
   function (along with most functions in this file) must be fixed. */
						      
int
compare_linkage_lists (links1, links2)
    List *links1;
    List *links2;
{
    Node *n1, *n2;
    char *p1, *p2;

    sortlist (links1, fsortcmp);
    sortlist (links2, fsortcmp);

    n1 = links1->list->next;
    n2 = links2->list->next;

    while (n1 != links1->list && n2 != links2->list)
    {
	/* Get the basenames of both files. */
	p1 = strrchr (n1->key, '/');
	if (p1 == NULL)
	    p1 = n1->key;
	else
	    ++p1;

	p2 = strrchr (n2->key, '/');
	if (p2 == NULL)
	    p2 = n2->key;
	else
	    ++p2;

	/* Compare the files' basenames. */
	if (strcmp (p1, p2) != 0)
	    return 0;

	n1 = n1->next;
	n2 = n2->next;
    }

    /* At this point we should be at the end of both lists; if not,
       one file has more links than the other, and return 1. */
    return (n1 == links1->list && n2 == links2->list);
}

/* Find a checked-out file in a list of filenames.  Used by RCS_checkout
   when checking out a new hardlinked file, to decide whether this file
   can be linked to any others that already exist.  The return value
   is not currently used. */

int
find_checkedout_proc (node, data)
    Node *node;
    void *data;
{
    Node **uptodate = (Node **) data;
    Node *link;
    char *dir = xgetwd();
    char *path;
    struct hardlink_info *hlinfo;

    /* If we have already found a file, don't do anything. */
    if (*uptodate != NULL)
	return 0;

    /* Look at this file in the hardlist and see whether the checked_out
       field is 1, meaning that it has been checked out during this CVS run. */
    path = (char *)
	xmalloc (strlen (dir) + strlen (node->key) + 2);
    sprintf (path, "%s/%s", dir, node->key);
    link = lookup_file_by_inode (path);
    free (path);
    free (dir);

    if (link == NULL)
    {
	/* We haven't seen this file -- maybe it hasn't been checked
	   out yet at all. */
	return 0;
    }

    hlinfo = link->data;
    if (hlinfo->checked_out)
    {
	/* This file has been checked out recently, so it's safe to
           link to it. */
	*uptodate = link;
    }

    return 0;
}

