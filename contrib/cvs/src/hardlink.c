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

/* For check_link_proc: list all of the files named in an inode list. */
static int
list_files_proc (node, vstrp)
    Node *node;
    void *vstrp;
{
    char **strp, *file;
    int len;

    /* Get the file's basename.  This is because -- VERY IMPORTANT --
       the `hardlinks' field is presently defined only to include links
       within a directory.  So the hardlinks field might be `foo' or
       `mumble grump flink', but not `foo bar com/baz' or `wham ../bam
       ../thank/you'.  Someday it would be nice to extend this to
       permit cross-directory links, but the issues involved are
       hideous. */

    file = strrchr (node->key, '/');
    if (file)
	++file;
    else
	file = node->key;

    /* Is it safe to cast vstrp to (char **) here, and then play with
       the contents?  I think so, since vstrp will have started out
       a char ** to begin with, so we should not have alignment bugs. */
    strp = (char **) vstrp;
    len = (*strp == NULL ? 0 : strlen (*strp));
    *strp = (char *) xrealloc (*strp, len + strlen (file) + 2);
    if (*strp == NULL)
    {
	error (0, errno, "could not allocate memory");
	return 1;
    }
    if (sprintf (*strp + len, "%s ", file) < 0)
    {
	error (0, errno, "could not compile file list");
	return 1;
    }

    return 0;
}    

/* Set the link field of each hardlink_info node to `data', which is a
   list of linked files. */
static int
set_hardlink_field_proc (node, data)
    Node *node;
    void *data;
{
    struct hardlink_info *hlinfo = (struct hardlink_info *) node->data;
    hlinfo->links = xstrdup ((char *) data);

    return 0;
}

/* For each file being checked in, compile a list of the files linked
   to it, and cache the list in the file's hardlink_info field. */
int
cache_hardlinks_proc (node, data)
    Node *node;
    void *data;
{
    List *inode_links;
    char *p, *linked_files = NULL;
    int err;

    inode_links = (List *) node->data;

    /* inode->data is a list of hardlink_info structures: all the
       files linked to this inode.  We compile a string of each file
       named in this list, in alphabetical order, separated by spaces.
       Then store this string in the `links' field of each
       hardlink_info structure, so that RCS_checkin can easily add
       it to the `hardlinks' field of a new delta node. */

    sortlist (inode_links, fsortcmp);
    err = walklist (inode_links, list_files_proc, &linked_files);
    if (err)
	return err;

    /* Trim trailing whitespace. */
    p = linked_files + strlen(linked_files) - 1;
    while (p > linked_files && isspace (*p))
	*p-- = '\0';

    err = walklist (inode_links, set_hardlink_field_proc, linked_files);
    return err;
}

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
    inodestr = (char *) xmalloc (2*sizeof(ino_t)*sizeof(char) + 1);
    if (stat (file, &sb) < 0)
    {
	if (errno == ENOENT)
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
	hp->type = UNKNOWN;
	hp->key = inodestr;
	hp->data = (char *) getlist();
	hp->delproc = dellist;
	(void) addnode (hardlist, hp);
    }
    else
    {
	free (inodestr);
    }

    p = findnode ((List *) hp->data, filepath);
    if (p == NULL)
    {
	p = getnode();
	p->type = UNKNOWN;
	p->key = xstrdup (filepath);
	p->data = NULL;
	(void) addnode ((List *) hp->data, p);
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
	path = xmalloc (sizeof(char) * (strlen(dir) + strlen(file) + 2));
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
	n->data = (char *) xmalloc (sizeof (struct hardlink_info));
    hlinfo = (struct hardlink_info *) n->data;
    hlinfo->status = T_UPTODATE;
    hlinfo->checked_out = 1;
    hlinfo->links = NULL;
}

/* Return a string listing all the files known to be linked to FILE in
   the working directory.  Used by special_file_mismatch, to determine
   whether it is safe to merge two files. */
char *
list_files_linked_to (file)
    const char *file;
{
    char *inodestr, *filelist, *path;
    struct stat sb;
    Node *n;
    int err;

    /* If hardlist is NULL, we have not been doing an operation that
       would permit us to know anything about the file's hardlinks
       (cvs update, cvs commit, etc).  Return an empty string. */
    if (hardlist == NULL)
	return xstrdup ("");

    /* Get the full pathname of file (assuming the working directory) */
    if (file[0] == '/')
	path = xstrdup (file);
    else
    {
	char *dir = xgetwd();
	path = (char *) xmalloc (sizeof(char) *
				 (strlen(dir) + strlen(file) + 2));
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
    inodestr = (char *) xmalloc (2*sizeof(ino_t)*sizeof(char) + 1);
    sprintf (inodestr, "%lx", (unsigned long) sb.st_ino);

    /* Make sure the files linked to this inode are sorted. */
    n = findnode (hardlist, inodestr);
    sortlist ((List *) n->data, fsortcmp);

    filelist = NULL;
    err = walklist ((List *) n->data, list_files_proc, &filelist);
    if (err)
	error (1, 0, "cannot get list of hardlinks for %s", file);

    free (inodestr);
    return filelist;
}
