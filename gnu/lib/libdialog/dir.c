/****************************************************************************
 *
 *	Program:	dir.c
 *	Author:		Marc van Kempen
 *	desc:		Directory routines, sorting and reading
 *
 * Copyright (c) 1995, Marc van Kempen
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 *
 ****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>		/* XXX for _POSIX_VERSION ifdefs */

#if !defined sgi && !defined _POSIX_VERSION
#include <sys/dir.h>
#endif
#if defined __sun__
#include <sys/dirent.h>
#endif
#if defined sgi || defined _POSIX_VERSION
#include <dirent.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fnmatch.h>
#include <sys/param.h>
#include "dir.h"

/****************************************************************************
 *
 *	local prototypes
 *
 ****************************************************************************/

void toggle_dotfiles(void);
int  show_dotfiles(void);
int  dir_alphasort(const void *d1, const void *d2);
int  dir_sizesort(const void *d1, const void *d2);
int  dir_datesort(const void *d1, const void *d2);
int  dir_extsort(const void *d1, const void *d2);

/****************************************************************************
 *
 *	global variables
 *
 ****************************************************************************/


/* This is user-selectable, I've set them fixed for now however */

void		*_sort_func = dir_alphasort;
static int	_showdotfiles = TRUE;

/****************************************************************************
 *
 *	Functions
 *
 ****************************************************************************/

int
dir_select_nd(
#if defined __linux__
  const struct dirent *d
#else
  struct dirent *d
#endif
)
/*
 *	desc:	allways include a directory entry <d>, except
 *		for the current directory and other dot-files
 *		keep '..' however.
 *	pre:	<d> points to a dirent
 *	post:	returns TRUE if d->d_name != "." else FALSE
 */
{
    if (strcmp(d->d_name, ".")==0 ||
	  (d->d_name[0] == '.' && strlen(d->d_name) > 1 && d->d_name[1] != '.')) {
	return(FALSE);
    } else {
	return(TRUE);
    }
}/* dir_select_nd() */


int
dir_select(
#ifdef __linux__
  const struct dirent *d
#else
  struct dirent *d
#endif
)
/*
 *	desc:	allways include a directory entry <d>, except
 *		for the current directory
 *	pre:	<d> points to a dirent
 *	post:	returns TRUE if d->d_name != "." else FALSE
 */
{
	if (strcmp(d->d_name, ".")==0) {	/* don't include the current directory */
		return(FALSE);
	} else {
	    return(TRUE);
	}
} /* dir_select() */

int
dir_select_root_nd(
#ifdef __linux__
  const struct dirent *d
#else
  struct dirent *d
#endif
)
/*
 *	desc:	allways include a directory entry <d>, except
 *		for the current directory and the parent directory.
 *		Also skip any other dot-files.
 *	pre:	<d> points to a dirent
 *	post:	returns TRUE if d->d_name[0] != "." else FALSE
 */
{
	if (d->d_name[0] == '.') {	/* don't include the current directory */
		return(FALSE);		/* nor the parent directory */
	} else {
	    return(TRUE);
	}
} /* dir_select_root_nd() */


int
dir_select_root(
#ifdef __linux__
  const struct dirent *d
#else
  struct dirent *d
#endif
)
/*
 *	desc:	allways include a directory entry <d>, except
 *		for the current directory and the parent directory
 *	pre:	<d> points to a dirent
 *	post:	returns TRUE if d->d_name[0] != "." else FALSE
 */
{
	if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0) {
		return(FALSE);
	} else {
	    return(TRUE);
	}
}/* dir_select_root() */


#ifdef NO_ALPHA_SORT
int
alphasort(const void *d1, const void *d2)
/*
 *	desc:	a replacement for what should be in the library
 */
{
    return(strcmp(((struct dirent *) d1)->d_name,
		  ((struct dirent *) d2)->d_name));
} /* alphasort() */
#endif

int
dir_alphasort(const void *d1, const void *d2)
/*
 *	desc:	compare d1 and d2, but put directories always first
 *		put '..' always on top
 *
 */
{
    DirList	*f1 = ((DirList *) d1),
		*f2 = ((DirList *) d2);
    struct stat	*s1 = &(f1->filestatus);
    struct stat	*s2 = &(f2->filestatus);

    /* check for '..' */
    if (strcmp(((DirList *) d1)->filename, "..") == 0) {
	return(-1);
    }
    if (strcmp(((DirList *) d2)->filename, "..") == 0) {
	return(1);
    }

    /* put directories first */
    if ((s1->st_mode & S_IFDIR) && (s2->st_mode & S_IFDIR)) {
	return(strcmp(f1->filename, f2->filename));
    };
    if (s1->st_mode & S_IFDIR) {
	return(-1);
    }
    if (s2->st_mode & S_IFDIR) {
	return(1);
    }
    return(strcmp(f1->filename, f2->filename));

} /* dir_alphasort() */


int
dir_sizesort(const void *d1, const void *d2)
/*
 *	desc:	compare d1 and d2, but put directories always first
 *
 */
{
    DirList	*f1 = ((DirList *) d1),
		*f2 = ((DirList *) d2);
    struct stat	*s1 = &(f1->filestatus);
    struct stat	*s2 = &(f2->filestatus);

    /* check for '..' */
    if (strcmp(((DirList *) d1)->filename, "..") == 0) {
	return(-1);
    }
    if (strcmp(((DirList *) d2)->filename, "..") == 0) {
	return(1);
    }

    /* put directories first */
    if ((s1->st_mode & S_IFDIR) && (s2->st_mode & S_IFDIR)) {
	return(s1->st_size < s2->st_size ?
	       -1
	       :
	       s1->st_size >= s2->st_size);
    };
    if (s1->st_mode & S_IFDIR) {
	return(-1);
    }
    if (s2->st_mode & S_IFDIR) {
	return(1);
    }
    return(s1->st_size < s2->st_size ?
	   -1
	   :
	   s1->st_size >= s2->st_size);

} /* dir_sizesort() */

int
dir_datesort(const void *d1, const void *d2)
/*
 *	desc:	compare d1 and d2 on date, but put directories always first
 */
{
    DirList	*f1 = ((DirList *) d1),
		*f2 = ((DirList *) d2);
    struct stat	*s1 = &(f1->filestatus);
    struct stat	*s2 = &(f2->filestatus);


    /* check for '..' */
    if (strcmp(((DirList *) d1)->filename, "..") == 0) {
	return(-1);
    }
    if (strcmp(((DirList *) d2)->filename, "..") == 0) {
	return(1);
    }

    /* put directories first */
    if ((s1->st_mode & S_IFDIR) && (s2->st_mode & S_IFDIR)) {
	return(s1->st_mtime < s2->st_mtime ?
	       -1
	       :
	       s1->st_mtime >= s2->st_mtime);
    };
    if (s1->st_mode & S_IFDIR) {
	return(-1);
    }
    if (s2->st_mode & S_IFDIR) {
	return(1);
    }
    return(s1->st_mtime < s2->st_mtime ?
	   -1
	   :
	   s1->st_mtime >= s2->st_mtime);

} /* dir_datesort() */


int
null_strcmp(char *s1, char *s2)
/*
 *	desc:	compare strings allowing NULL pointers
 */
{
	if ((s1 == NULL) && (s2 == NULL)) {
		return(0);
	}
	if (s1 == NULL) {
		return(-1);
	}
	if (s2 == NULL) {
		return(1);
	}
	return(strcmp(s1, s2));
} /* null_strcmp() */


int
dir_extsort(const void *d1, const void *d2)
/*
 *	desc:	compare d1 and d2 on extension, but put directories always first
 *		extension = "the characters after the last dot in the filename"
 *	pre:	d1 and d2 are pointers to  DirList type records
 *	post:	see code
 */
{
    DirList	*f1 = ((DirList *) d1),
		*f2 = ((DirList *) d2);
    struct stat	*s1 = &(f1->filestatus);
    struct stat	*s2 = &(f2->filestatus);
    char 	*ext1, *ext2;
    int		extf, ret;


    /* check for '..' */
    if (strcmp(((DirList *) d1)->filename, "..") == 0) {
	return(-1);
    }
    if (strcmp(((DirList *) d2)->filename, "..") == 0) {
	return(1);
    }


    /* find the first extension */

    ext1 = f1->filename + strlen(f1->filename);
    extf = FALSE;
    while (!extf && (ext1 > f1->filename)) {
	extf = (*--ext1 == '.');
    }
    if (!extf) {
	ext1 = NULL;
    } else {
	ext1++;
    }
    /* ext1 == NULL if there's no "extension" else ext1 points */
    /* to the first character of the extension string */

    /* find the second extension */

    ext2 = f2->filename + strlen(f2->filename);
    extf = FALSE;
    while (!extf && (ext2 > f2->filename)) {
	extf = (*--ext2 == '.');
    }
    if (!extf) {
	ext2 = NULL;
    } else {
	ext2++;
    }
    /* idem as for ext1 */

    if ((s1->st_mode & S_IFDIR) && (s2->st_mode & S_IFDIR)) {
	ret = null_strcmp(ext1, ext2);
	if (ret == 0) {
	    return(strcmp(f1->filename, f2->filename));
	} else {
	    return(ret);
	}
    };
    if (s1->st_mode & S_IFDIR) {
	return(-1);
    }
    if (s2->st_mode & S_IFDIR) {
	return(1);
    }
    ret = null_strcmp(ext1, ext2);
    if (ret == 0) {
	return(strcmp(f1->filename, f2->filename));
    } else {
	return(ret);
    }

} /* dir_extsort() */


void
get_dir(char *dirname, char *fmask, DirList **dir, int *n)
/*
 *	desc:	get the files in the current directory
 *	pre:	<dir> == NULL
 *	post:	<dir> contains <n> dir-entries
 */
{
	char		cwd[MAXPATHLEN];
	char		buf[256];
	struct dirent	**dire;
	struct stat	status;
	int		i, j, nb;
	long		d;


	getcwd(cwd, MAXPATHLEN);
	if (strcmp(cwd, "/") == 0) {	/* we are in the root directory */
	    if (show_dotfiles()) {
		*n = scandir(dirname, &dire, dir_select_root, alphasort);
	    } else {
		*n = scandir(dirname, &dire, dir_select_root_nd, alphasort);
	    }
	} else {
	    if (show_dotfiles()) {
		*n = scandir(dirname, &dire, dir_select, alphasort);
	    } else {
		*n = scandir(dirname, &dire, dir_select_nd, alphasort);
	    }
	}

	/* There is the possibility that we have entered a directory	*/
	/* which we are not allowed to read, scandir thus returning  	*/
	/* -1 for *n.	 						*/
	/* Actually I should also check for lack of memory, but I'll 	*/
	/* let my application happily crash if this is the case		*/
	/* Solution:							*/
	/*	manually insert the parent directory as the only	*/
	/*	directory entry, and return.				*/

	if (*n == -1) {
	    *n = 1;
	    *dir = (DirList *) malloc(sizeof(DirList));
	    strcpy((*dir)[0].filename, "..");
	    lstat("..", &status);
	    (*dir)[0].filestatus = status;
	    (*dir)[0].link = FALSE;
	    return;
	}

	*dir = (DirList *) malloc( *n * sizeof(DirList) );
	d = 0;
	i = 0;
	j = 0;
	while (j<*n) {
	    lstat(dire[j]->d_name, &status);
	    /* check if this file is to be included */
	    /* always include directories, the rest is subject to fmask */
	    if (S_ISDIR(status.st_mode)
		|| fnmatch(fmask, dire[j]->d_name, FNM_NOESCAPE) != FNM_NOMATCH) {
		strcpy((*dir)[i].filename, dire[j]->d_name);
		(*dir)[i].filestatus = status;
		if ((S_IFMT & status.st_mode) == S_IFLNK) {  /* handle links */
		    (*dir)[i].link = TRUE;
		    stat(dire[j]->d_name, &status);
		    nb = readlink(dire[j]->d_name, buf, 256);
		    if (nb == -1) {
			printf("get_dir(): Error reading link: %s\n", dire[j]->d_name);
			exit(-1);
		    } else {
			(*dir)[i].linkname = malloc(sizeof(char) * nb + 1);
			strncpy((*dir)[i].linkname, buf, nb);
			(*dir)[i].linkname[nb] = 0;
		    }
		    (*dir)[i].filestatus = status;
		} else {
		    (*dir)[i].link = FALSE;
		    (*dir)[i].linkname = NULL;
		}
		i++;
	    } else {
		/* skip this entry */
	    }
	    j++;
	}
	*n = i;

	/* sort the directory with the directory names on top */
	qsort((*dir), *n, sizeof(DirList), _sort_func);

	/* Free the allocated memory */
	for (i=0; i<*n; i++) {
	    free(dire[i]);
	}
	free(dire);

	return;
}/* get_dir() */


void
FreeDir(DirList *d, int n)
/*
 * 	desc:	free the dirlist d
 *	pre:	d != NULL
 *	post:	memory allocated to d has been released
 */
{
    int i;

    if (d) {
	for (i=0; i<n; i++) {
	    if (d[i].linkname) {
		free(d[i].linkname);
	    }
	}
	free(d);
    } else {
	printf("dir.c:FreeDir(): d == NULL\n");
	exit(-1);
    }

    return;
} /* FreeDir() */

void
toggle_dotfiles(void)
/*
 *	desc: toggle visibility of dot-files
 */
{
    _showdotfiles = !_showdotfiles;

    return;
} /* toggle_dotfiles() */

int
show_dotfiles(void)
/*
 *	desc: return the value of _showdotfiles
 */
{
    return(_showdotfiles);
} /* show_dotfiles() */

void
set_dotfiles(int b)
/*
 *	desc: set the value of _showdotfiles
 */
{
    _showdotfiles = b;

    return;
} /* set_dotfiles() */
