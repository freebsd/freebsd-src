/***************************************************************
 * 
 * Program:	pkg_manage
 * Author:	Marc van Kempen
 * Desc:	Add, delete packages with the pkg_* binaries
 *		Get info about installed packages
 *		Review about to be installed packages
 *
 * 1. View installed packages
 * 2. Delete installed packages
 * 3. Install packages
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
 ***************************************************************/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>

#include "pkg_manage.h"
#include "ui_objects.h"
#include "dialog.priv.h"
#include "dir.h"

PKG_info	p_inf = { 0, 0, NULL, NULL, NULL, NULL, NULL };

/*******************************************************************
 *
 * 	Misc Functions
 *
 *******************************************************************/

void 
FreeInfo(void)
/*
 * Desc: free the space allocated to p_inf
 */
{
    free(p_inf.buf);
    free(p_inf.name);
    free(p_inf.comment);
    free(p_inf.description);
    p_inf.Nitems = 0;
    FreeMnu(p_inf.mnu, 2*p_inf.Nitems);

    return;
} /* FreeInfo() */

void
FreeMnu(unsigned char **mnu, int n)
/*
 * Desc: free mnu array
 */
{
    int i;
    
    for (i=0; i<n; i++) {
	free(mnu[i]);
    }   
    free(mnu);

    return;
} /* FreeMnu() */

int
file_exists(char *fname)
/* 
 * Desc: check if the file <fname> exists (and is readable)
 */
{
    FILE *f;
    
    if (strlen(fname) == 0) return(FALSE); /* apparently opening an empty */
					   /* file for reading succeeds always */
    f = fopen(fname, "r");
    if (f) {
	fclose(f);
	return(TRUE);
    } else {
	return(FALSE);
    }
} /* file_exists() */


int
exec_catch_errors(char *prog, char *arg, char *fout)
/*
 * Desc: run the program <prog> with arguments <arg> and catch its output
 * 	 in <fout> and display it in case of an error. specify NULL, 
 *	 if you don't want output.
 */
{
    char	*execstr, *tmp;
    int		ret, yesno, unlink_fout;

    execstr = (char *) malloc( strlen(prog) + strlen(arg) + 30 );
    if (!execstr) {
	fprintf(stderr, "exec_catch_errors: Error while mallocing memory\n");
	exit(-1);
    }

    /* when fout == NULL, allocate a temporary file name and unlink it */
    /* when done */
    if (!fout) {
	fout = tempnam(NULL, "pkg.");
	if (!fout) {
	    fprintf(stderr, "exec_catch_errors: Error allocating temp.name\n");
	    exit(-1);
	}
	unlink_fout = TRUE;
    } else {
	unlink_fout = FALSE;
    }

    sprintf(execstr, "%s %s > %s 2>&1", prog, arg, fout);
    ret = system(execstr);
    if (ret) {	

	yesno = dialog_yesno("Error", "An error occured, view output?", 8, 40);
	if (yesno == 0) {
	    /* disable helpline */
	    tmp = get_helpline();
	    use_helpline("use arrowkeys, PgUp, PgDn to move, press enter when done");
	    dialog_textbox("Error output from pkg_add", fout, LINES-2, COLS-4);
	    restore_helpline(tmp);
	}
    }

    if (unlink_fout) {
	unlink(fout);
	free(fout);
    }
    free(execstr);
    
    return(ret);
} /* exec_catch_errors() */

void
get_pkginfo(void)
/*
 * Desc: get info about installed packages
 */
{
    FILE	*f;
    char 	*p;
    struct stat sb;
    int		i, j, n, lsize;
    int		newline, ret;
    int		state;
    char	*tmp_file;
#define	R_NAME 1
#define R_COMMENT 2
#define R_DESC 3

    if (p_inf.Nitems > 0) {
	FreeInfo();
    }
    /* p_inf.Nitems == 0 */

    dialog_msgbox("PKG INFO", "Reading info, please wait ...", 4, 35, FALSE);

    tmp_file = tempnam(NULL, "pkg.");  
    ret = exec_catch_errors(PKG_INFO, "-a", tmp_file);
    if (ret) {
	dialog_notify("Could not get package info\nexiting!");
	unlink(tmp_file);
	free(tmp_file);
	return;
    }
    dialog_clear_norefresh();

    f = fopen(tmp_file, "r");
    if (!f) {
	dialog_notify("Could not open temporary file");
	unlink(tmp_file);
	free(tmp_file);
	return;
    }
    
    if (stat(tmp_file, &sb)) {	/* stat file to get filesize */
	dialog_notify("Could not stat temporary file");
	fclose(f);
	unlink(tmp_file);
	free(tmp_file);
	return;
    }
    
    if (sb.st_size == 0) {
	dialog_notify("No packages installed or no info available");
	fclose(f);
	unlink(tmp_file);
	free(tmp_file);
	return;
    }

    /* Allocate a buffer with sufficient space to hold the entire file */
    p_inf.buf = (char *) malloc( sb.st_size + 1);
    p_inf.N = sb.st_size;
    if (fread(p_inf.buf, 1, p_inf.N, f) != p_inf.N) {
	dialog_notify("Could not read entire temporary file");
	free(p_inf.buf);
	fclose(f);
	unlink(tmp_file);
	free(tmp_file);
	return;
    }
    p_inf.buf[p_inf.N] = 0;
    fclose(f);
    unlink(tmp_file);
    free(tmp_file);

    /* make one sweep through the buffer to determine the # of entries */
    /* Look for "Information for" in the first column */
    i = p_inf.N - strlen("Information for") - 1;
    p = p_inf.buf;
    if (strncmp(p_inf.buf, "Information for", 15) == 0) {
	n = 1; 
    } else {
	n = 0;
    }
    while (i-- > 0) {
	if (*p == '\n') {
	    if (strncmp(p+1, "Information for", 15) == 0) {
		n++;
	    }
	}
	p++;
    }
    
    /* malloc space for PKG_info */
    p_inf.name = (char **) malloc( n * sizeof(char *) );
    p_inf.comment = (char **) malloc( n * sizeof(char *) );
    p_inf.description = (char **) malloc( n * sizeof(char *) );
    p_inf.Nitems = n;

    /* parse pkg_info output */
    /* use a finite state automate to parse the file */

    i = 0;
    p = p_inf.buf;
    newline = TRUE;
    state = R_NAME;
    while (*p) {
	if (newline) {
	    newline = FALSE;
	    switch(state) {
	    case R_NAME:
		if (strncmp(p, "Information for", 15) == 0) {
		    if (p>p_inf.buf) *(p-1) = '\0';
		    p_inf.name[i] = (char *) p+16;
		    while (*p && *p != ':') p++;
		    if (*p) *p = '\0';
		    state = R_COMMENT;
		}
		break;
	    case R_COMMENT:
		if (strncmp(p, "Comment:", 8) == 0) {
		    while (*p && *p != '\n') p++;
		    if (*p) p_inf.comment[i] = (char *) p+1;
		    p++;
		    while (*p && *p != '\n') p++;
		    if (*p) {
			*p = '\0';
			newline = TRUE;
		    }
		    state = R_DESC;
		}
		break;
	    case R_DESC:
		if (strncmp(p, "Description:", 12) == 0) {
		    while (*p && *p != '\n') p++;
		    if (*p) {
			p_inf.description[i] = (char *) p+1;
			newline = TRUE;
		    }
		    state = R_NAME;
		    i++;
		}
		break;
	    }
	}
	if (*p == '\n') newline = TRUE;
	p++;
    }

    /* build menu entries */
    p_inf.mnu = (unsigned char **) malloc( 2 * p_inf.Nitems * sizeof(char *) );

    lsize = COLS-30;
    j=0;
    for (i=0; i<p_inf.Nitems; i++) {
	/* tag */
	p_inf.mnu[j] = (char *) malloc( lsize );
	strncpy(p_inf.mnu[j], p_inf.name[i], lsize-1); 
	p_inf.mnu[j++][lsize-1] = 0;

	/* description */
	p_inf.mnu[j] = (char *) malloc( lsize );
	strncpy(p_inf.mnu[j], p_inf.comment[i], lsize-1); 
	p_inf.mnu[j++][lsize-1] = 0; 
    }

    return;
} /* get_pkginfo() */

int
get_pkg_index(char *selection)
/*
 * desc: get the index i, for which p_inf.name[i] == selection
 */
{
    int i, found = FALSE, index = -1;

    for (i=0; i<p_inf.Nitems && !found; i++) {
	if (strcmp(selection, p_inf.name[i]) == 0) {
	    found = TRUE;
	    index = i;
	}
    }
    
    return(index);
} /* get_pkg_index() */

void
install_package(char *fname)
/* 
 * Desc: install the package <fname>
 */
{
    char 	*tmp_file;

    tmp_file = tempnam(NULL, "pkg.");
    if (!tmp_file) {
	fprintf(stderr, "install_package(): Error malloc'ing tmp_file\n");
	exit(-1);
    }
    if (!getenv("PKG_PATH"))
	putenv("/usr/ports/packages:/usr/ports/packages/all:.");
    exec_catch_errors(PKG_ADD, fname, tmp_file);

    unlink(tmp_file);
    free(tmp_file);

    return;
} /* install_package() */



int
get_desc(char *fname, char **name, char **comment, 
	 char **desc, long *size, char *tmp_dir)
/*
 * Desc: get the description and comment from the files
 *	 DESC, CONTENT and COMMENT from fname
 * Pre:  the current working directory is a temporary, 
 *	 empty directory.
 * Post: name = the name of the package
 * 	 comment = the comment for the package
 *	 desc = the description for the package
 */
{
    char	msg[80], args[512], *buf, *p, tmp[MAXPATHLEN];
    FILE	*f, *pf;
    struct stat sb;
    int		i, N, ret, found;

    *comment = NULL;
    *desc = NULL;
    *name = NULL;

    sprintf(args, "--fast-read -zxvf %s -C %s %s %s %s", fname, 
	    tmp_dir, CONTENTS, DESC, COMMENT);
    ret = exec_catch_errors(TAR, args, NULL);
    if (ret) {
	sprintf(msg, "Could not get info for <%s>", fname);
	dialog_notify(msg);
	return(FALSE);
    }
    /* Read CONTENTS */
    sprintf(tmp, "%s/%s", tmp_dir, CONTENTS);
    f = fopen(tmp, "r");
    if (f == NULL) {
	/* No contents file in package, propably not a package */
	return(FALSE);
    }
    if (stat(tmp, &sb)) {	/* stat file to get filesize */
	dialog_notify("Could not stat CONTENTS file");
	fclose(f);
	return(FALSE);
    }
    if (sb.st_size == 0) {
	dialog_notify("CONTENTS file has zero length");
	fclose(f);
	return(FALSE);
    }

    /* Allocate a buffer with sufficient space to hold the entire file */
    buf = (char *) malloc( sb.st_size + 1);
    N = sb.st_size;
    if (fread(buf, 1, N, f) != N) {
	sprintf(msg, "Could not read CONTENT file for <%s>", fname);
	dialog_notify(msg);
	free(buf);
	fclose(f);
	return(FALSE);
    }
    fclose(f);
    buf[N] = 0;

    /* look for the name of the package */
    p = buf;
    found = FALSE;
    while (*p && !found) {
	if (strncmp(p, "@name ", 6) == 0) {
	    i=0; 
	    p += 6;
	    while (*p && p[i] != '\n' && p[i] != '\r') i++;
	    *name = (char *) malloc( i+1 );
	    strncpy(*name, p, i);
	    (*name)[i] = 0;
	    found = TRUE;
	} else {
	    p++;
	}
    }
    unlink(tmp);

    /* Read COMMENT file */
    sprintf(tmp, "%s/%s", tmp_dir, COMMENT);
    f = fopen(tmp, "r");
    if (f == NULL) {
	/* No comment file in package, propably not a package */
	return(FALSE);
    }
    if (stat(tmp, &sb)) {	/* stat file to get filesize */
	dialog_notify("Could not stat COMMENT file");
	fclose(f);
	return(FALSE);
    }
    if (sb.st_size == 0) {
	dialog_notify("COMMENT file has zero length");
	fclose(f);
	return(FALSE);
    }

    /* Allocate a buffer with sufficient space to hold the entire file */
    *comment = (char *) malloc( sb.st_size + 1);
    N = sb.st_size;
    if (fread(*comment, 1, N, f) != N) {
	sprintf(msg, "Could not read COMMENT file for <%s>", fname);
	dialog_notify(msg);
	free(*comment);
	fclose(f);
	return(FALSE);
    }
    fclose(f);
    (*comment)[N] = 0;
    unlink(tmp);

    /* Read DESC */
    sprintf(tmp, "%s/%s", tmp_dir, DESC);
    f = fopen(tmp, "r");
    if (f == NULL) {
	/* No description file in package, propably not a package */
	return(FALSE);
    }
    if (stat(tmp, &sb)) {	/* stat file to get filesize */
	dialog_notify("Could not stat DESC file");
	fclose(f);
	return(FALSE);
    }
    if (sb.st_size == 0) {
	dialog_notify("DESC file has zero length");
	fclose(f);
	return(FALSE);
    }

    /* Allocate a buffer with sufficient space to hold the entire file */
    *desc = (char *) malloc( sb.st_size + 1);
    N = sb.st_size;
    if (fread(*desc, 1, N, f) != N) {
	sprintf(msg, "Could not read CONTENT file for <%s>", fname);
	dialog_notify(msg);
	free(*desc);
	fclose(f);
	return(FALSE);
    }
    fclose(f);
    (*desc)[N] = 0;
    unlink(tmp);

    /* get the size from the uncompressed package */
    sprintf(tmp, "%s -l %s", GUNZIP, fname);
    pf = popen(tmp, "r");
    if (!pf) {
	dialog_notify("Could not popen gunzip to get package size");
	*size = 0;
    } else {
	while (!feof(pf)) {
	    fgets(tmp, 80, pf);
	}
	sscanf(tmp, "%*s %ld", size);
	pclose(pf);
    }
	
    if (found) {
	return(TRUE);
    } else {
	return(FALSE);
    }
} /* get_desc() */


int
already_installed(char *name)
/* 
 * Desc: check if <name> is already installed as a package
 */
{
    int	i, found;

    found = FALSE;
    for (i=0; i<p_inf.Nitems && !found; i++) {
	if (strcmp(name, p_inf.name[i]) == 0) {
	    found = TRUE;
	}
    }

    return(found);
} /* already_installed() */
    
 
