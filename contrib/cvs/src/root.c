/*
 * Copyright (c) 1992, Mark D. Baushke
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Name of Root
 * 
 * Determine the path to the CVSROOT and set "Root" accordingly.
 * If this looks like of modified clone of Name_Repository() in
 * repos.c, it is... 
 */

#include "cvs.h"

char *
Name_Root(dir, update_dir)
     char *dir;
     char *update_dir;
{
    FILE *fpin;
    char *ret, *xupdate_dir;
    char root[PATH_MAX];
    char tmp[PATH_MAX];
    char cvsadm[PATH_MAX];
    char *cp;

    if (update_dir && *update_dir)
	xupdate_dir = update_dir;
    else
	xupdate_dir = ".";

    if (dir != NULL)
    {
	(void) sprintf (cvsadm, "%s/%s", dir, CVSADM);
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_ROOT);
    }
    else
    {
	(void) strcpy (cvsadm, CVSADM);
	(void) strcpy (tmp, CVSADM_ROOT);
    }

    /*
     * Do not bother looking for a readable file if there is no cvsadm
     * directory present.
     *
     * It is possible that not all repositories will have a CVS/Root
     * file. This is ok, but the user will need to specify -d
     * /path/name or have the environment variable CVSROOT set in
     * order to continue.
     */
    if ((!isdir (cvsadm)) || (!isreadable (tmp)))
    {
	if (CVSroot == NULL)
	{
	    error (0, 0, "in directory %s:", xupdate_dir);
	    error (0, 0, "must set the CVSROOT environment variable");
	    error (0, 0, "or specify the '-d' option to %s.", program_name);
	}
	return (NULL);
    }

    /*
     * The assumption here is that the CVS Root is always contained in the
     * first line of the "Root" file.
     */
    fpin = open_file (tmp, "r");

    if (fgets (root, PATH_MAX, fpin) == NULL)
    {
	error (0, 0, "in directory %s:", xupdate_dir);
	error (0, errno, "cannot read %s", CVSADM_ROOT);
	error (0, 0, "please correct this problem");
	return (NULL);
    }
    (void) fclose (fpin);
    if ((cp = strrchr (root, '\n')) != NULL)
	*cp = '\0';			/* strip the newline */

    /*
     * root now contains a candidate for CVSroot. It must be an
     * absolute pathname
     */

#ifdef CLIENT_SUPPORT
    /* It must specify a server via remote CVS or be an absolute pathname.  */
    if ((strchr (root, ':') == NULL)
    	&& ! isabsolute (root))
#else /* ! CLIENT_SUPPORT */
    if (root[0] != '/')
#endif /* CLIENT_SUPPORT */
    {
	error (0, 0, "in directory %s:", xupdate_dir);
	error (0, 0,
	       "ignoring %s because it does not contain an absolute pathname.",
	       CVSADM_ROOT);
	return (NULL);
    }

#ifdef CLIENT_SUPPORT
    if ((strchr (root, ':') == NULL) && !isdir (root))
#else /* ! CLIENT_SUPPORT */
    if (!isdir (root))
#endif /* CLIENT_SUPPORT */
    {
	error (0, 0, "in directory %s:", xupdate_dir);
	error (0, 0,
	       "ignoring %s because it specifies a non-existent repository %s",
	       CVSADM_ROOT, root);
	return (NULL);
    }

    /* allocate space to return and fill it in */
    strip_path (root);
    ret = xstrdup (root);
    return (ret);
}

/*
 * Returns non-zero if the two directories have the same stat values
 * which indicates that they are really the same directories.
 */
int
same_directories (dir1, dir2)
     char *dir1;
     char *dir2;
{
    struct stat sb1;
    struct stat sb2;
    int ret;

    if (stat (dir1, &sb1) < 0)
        return (0);
    if (stat (dir2, &sb2) < 0)
        return (0);
    
    ret = 0;
    if ( (memcmp( &sb1.st_dev, &sb2.st_dev, sizeof(dev_t) ) == 0) &&
	 (memcmp( &sb1.st_ino, &sb2.st_ino, sizeof(ino_t) ) == 0))
        ret = 1;

    return (ret);
}


/*
 * Write the CVS/Root file so that the environment variable CVSROOT
 * and/or the -d option to cvs will be validated or not necessary for
 * future work.
 */
void
Create_Root (dir, rootdir)
     char *dir;
     char *rootdir;
{
    FILE *fout;
    char tmp[PATH_MAX];

    if (noexec)
	return;

    /* record the current cvs root */

    if (rootdir != NULL)
    {
        if (dir != NULL)
	    (void) sprintf (tmp, "%s/%s", dir, CVSADM_ROOT);
        else
	    (void) strcpy (tmp, CVSADM_ROOT);
        fout = open_file (tmp, "w+");
        if (fprintf (fout, "%s\n", rootdir) < 0)
	    error (1, errno, "write to %s failed", tmp);
        if (fclose (fout) == EOF)
	    error (1, errno, "cannot close %s", tmp);
    }
}
