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
 * 3. View package files
 * 4. Install packages files.
 *
 * Installation and deletion of packages should be previewable
 *
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
#include <sys/stat.h>
#include <stdio.h>

#include "pkg_manage.h"

static PKG_info		p_inf;
static int got_info;

/*
 * Local prototypes
 */

void	run_menu(void);
void	get_pkginfo(void);
void 	FreeMnu(unsigned char **mnu, int n);
void    FreeInfo(void);

/*
 * Main
 */

void
main(void)
{
    init_dialog();

    get_pkginfo();
    run_menu();
    
    if (got_info)
	FreeInfo();

    clear();
    dialog_update();
    end_dialog();

    return;
} /* main() */

void FreeInfo(void)
{
	free(p_inf.buf);
	free(p_inf.name);
	free(p_inf.comment);
	free(p_inf.description);
	FreeMnu(p_inf.mnu, 2*p_inf.Nitems);
	got_info = FALSE;
}

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


void
get_pkginfo(void)
{
    FILE	*f;
    char 	*p;
    char 	prog[512];
    struct stat sb;
    int		i, j, n, lsize;
    int		newline;
    int		state;
    char	*tmp_file;
#define	R_NAME 1
#define R_COMMENT 2
#define R_DESC 3

    if (got_info)
	FreeInfo();

    dialog_msgbox("PKG INFO", "Reading info, please wait ...", 4, 35, FALSE);

    tmp_file = tempnam(NULL, "pkg.");

    sprintf(prog, "%s -a > %s", PKG_INFO, tmp_file);
    system(prog);
    dialog_clear_norefresh();
    
    f = fopen(tmp_file, "r");
    if (!f) {
	dialog_notify("Could not open temporary file");
	goto err1;
    }
    
    if (stat(tmp_file, &sb)) {	/* stat file to get filesize */
	dialog_notify("Could not stat temporary file");
	goto err2;
    }
    
    if (sb.st_size == 0) {
	dialog_notify("No packages info available");
	goto err2;
    }

    /* Allocate a buffer with sufficient space to hold the entire file */
    p_inf.buf = (char *) malloc( sb.st_size );
    p_inf.N = sb.st_size;
    if (fread(p_inf.buf, 1, p_inf.N, f) != p_inf.N) {
	dialog_notify("Could not read from temporary file");
	free(p_inf.buf);
err2:
	fclose(f);
err1:
	got_info = FALSE;
	unlink(tmp_file);
	free(tmp_file);
	return;
    }
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
    while (i--) {
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
		    if (*p) *p = '\0';
		    p++;
		    state = R_DESC;
		}
		break;
	    case R_DESC:
		if (strncmp(p, "Description:", 12) == 0) {
		    while (*p && *p != '\n') p++;
		    if (*p) p_inf.description[i] = (char *) p+1;
		    state = R_NAME;
		    i++;
		}
		break;
	    }
	    newline = FALSE;
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
    
    got_info = TRUE;
    return;
} /* get_pkginfo() */

int
get_pkg_index(char *selection)
/*
 * desc: get the index i, for which p_inf.name[i] == selection
 */
{
    int i, found = FALSE, index = -1;

    if (got_info)
	for (i=0; i<p_inf.Nitems && !found; i++) {
	    if (strcmp(selection, p_inf.name[i]) == 0) {
		found = TRUE;
		index = i;
	    }
	}
    
    return(index);
} /* get_pkg_index() */

void
view_installed(void)
/*
 * Desc: View the installed packages
 */
{
    int		i, quit_view, sc=0, ch=0;
    char	selection[1024];

    if (!got_info) {
	use_helpfile(NULL);
	use_helpline(NULL);
	dialog_notify("No packages info available");
	return;
    }
    use_helpfile(VIEW_INST_HLP);
    quit_view = FALSE;
    while (!quit_view) {
	use_helpline("use arrow-keys or character to select option and press enter");
	if (dialog_menu("View installed packages", 
			"Press enter to see the package descriptions",
			LINES, COLS, LINES-5, p_inf.Nitems, 
			p_inf.mnu, selection, &ch, &sc )) {
	    quit_view = TRUE;
	} else {
	    i = get_pkg_index(selection);
	    use_helpline("use PgUp and PgDn and arrow-keys to move throught the text");
	    dialog_mesgbox(p_inf.comment[i], p_inf.description[i], LINES, COLS);
	}
    }
    use_helpfile(NULL);
    use_helpline(NULL);

    return;
} /* view_installed() */

void
delete_installed(void)
/*
 * Desc: Delete an installed package
 */
{
    unsigned char *mnu[] = {
	"1. Simulate delete", "Display commands that are going to be executed",
	"2. Delete", "Execute commands to delete the package",
	"3. Cancel", "Do NOT delete the package"
    };
    char	tmp[512], selection[512], *tmp_file;
    int		quit_view, quit_del;
    int		i, sel, ch=0, sc=0, ch0=0, sc0=0;

    if (!got_info) {
	use_helpfile(NULL);
	use_helpline(NULL);
	dialog_notify("No packages info available");
	return;
    }
    quit_view = FALSE;
    while (!quit_view) {
	use_helpline("use arrow-keys or character to select option and press enter");
	use_helpfile(DEL_INST_HLP);
	if (dialog_menu("DELETE an installed package",
			"Press enter to select a package",
			LINES, COLS, LINES-5, p_inf.Nitems, 
			p_inf.mnu, selection, &ch, &sc)) {
	    quit_view = TRUE;
	} else {
	    quit_del = FALSE;
	    i = get_pkg_index(selection);
	    while (!quit_del) {
		sprintf(tmp, "Delete <%s>", p_inf.name[i]);
		use_helpline("use arrow-keys or digit to select option and press enter");
		if (dialog_menu("Delete a package", tmp, 10, COLS-6, 
				3, 3, mnu, selection, &ch0, &sc0)) {
		    quit_del = TRUE;
		} else {
		    sel = atoi(selection);
		    switch(sel) {
		    case 1:
			tmp_file = tempnam(NULL, "pkg.");
			sprintf(tmp, "%s -n %s > %s", 
				PKG_DELETE, p_inf.name[i], tmp_file);
			system(tmp);
			dialog_textbox("Package deletion commands",
				       tmp_file, LINES, COLS);
			unlink(tmp_file);
			free(tmp_file);
			break;
		    case 2:
			sprintf(tmp, "%s %s", PKG_DELETE, p_inf.name[i]);
			system(tmp);
			get_pkginfo();
			quit_del = TRUE;
			break;
		    case 3:
			quit_del = TRUE;
			break;
		    }
		}
	    }
	}
    }
    use_helpfile(NULL);
    use_helpline(NULL);

    return;
} /* delete_installed() */

void 
preview_pkg(void)
/* 
 * Desc: View the package description and comment before installation
 */
{
    char	*fname;
    char	*tmp_file;
    char	prog[512], title[512];

    use_helpfile(PREVIEW_HLP);
    fname = dialog_fselect(".", "*.tgz");
    while (fname) {
	use_helpfile(PREVIEW_HLP);
	use_helpline("use PgUp and PgDn and arrow-keys to move through the text");
	tmp_file = tempnam(NULL, "pkg.");
	sprintf(prog, "%s zxOf %s +COMMENT +DESC > %s", TAR, fname, tmp_file);
	system(prog);
	sprintf(title, "Preview package <%s>", fname);
	dialog_textbox(title, tmp_file, LINES, COLS);
	unlink(tmp_file);
	free(fname);
	free(tmp_file);
	fname = dialog_fselect(".", "*.tgz");
    }
    if (fname) free(fname);
    use_helpfile(NULL);
    use_helpline(NULL);

    return;
} /* preview_pkg() */

void
install_new(void)
/*
 * Desc: Install a new package
 */
{
    char		*fname;
    char		*tmp_file;
    char		tmp[512], selection[40];
    unsigned char 	*mnu[] = {
	"1. Simulate install", "Display commands that are going to be executed",
	"2. Install", "Execute commands to install the package",
	"3. Cancel", "Do NOT install the package"
    };
    int		sel, quit_inst, ch=0, sc=0;
    
    use_helpfile(INSTALL_HLP);
    fname = dialog_fselect(".", "*.tgz");
    if (!fname) {
	quit_inst = TRUE;
    } else {
	quit_inst = FALSE;
    }
    while (!quit_inst) {
	use_helpfile(INSTALL_HLP);
	use_helpline("use arrow-keys or digit to select option and press enter");
	sprintf(tmp, "Install package <%s>", fname);
	if (dialog_menu("Install a package", tmp, 10, COLS-5, 
			3, 3, mnu, selection, &ch, &sc)) {
	    quit_inst = TRUE;
	} else {
	    sel = atoi(selection);
	    switch(sel) {
	    case 1:
		tmp_file = tempnam(NULL, "*.pkg");
		sprintf(tmp, "%s -n %s 2>&1 | cat > %s", PKG_ADD, fname, tmp_file);
		system(tmp);
		dialog_textbox("Package installation commands",
			       tmp_file, LINES, COLS);
		unlink(tmp_file);
		free(tmp_file);
		break;
	    case 2:
		sprintf(tmp, "%s %s", PKG_ADD, fname);
		system(tmp);
		get_pkginfo();
		quit_inst = TRUE;
		break;
	    case 3:
		quit_inst = TRUE;
		break;
	    }
	}
    }
    if (fname) free(fname);
    use_helpfile(NULL);
    use_helpline(NULL);

    return;
} /* install_new() */

void
run_menu(void)
/*
 * Desc: display/choose from menu
 */
{
    int		quit_pkg, sel, ch=0, sc=0;
    char	selection[30];
    unsigned char *pkg_menu[] = {
	"1. View installed", "Overview of the installed packages",
	"2. Delete installed", "Delete an installed package",
	"3. View pkg files", "Preview about to be installed packages",
	"4. Install pkg files", "Install new packages",
	"5. Quit", "Leave the program",
    };

    quit_pkg = FALSE;
    while (!quit_pkg) {
	use_helpline("use arrow-keys or digit to select option and press enter");
	use_helpfile(MAIN_HLP);
	if (dialog_menu("Package Manager", "Choose one of the options",
		    LINES, COLS, 5, 5, pkg_menu, selection, &ch, &sc)) {
	    sel = 0;
	    quit_pkg = TRUE;
	} else {
	    sel = atoi(selection);
	}
	
	switch(sel) {
	case 0: /* Quit */
	    break;
	case 1:	/* View installed packages */
	    view_installed();
	    break;
	case 2: /* Delete installed package */
	    delete_installed();
	    break;
	case 3: /* Preview new package file */
	    preview_pkg();
	    break;
	case 4: /* Install new package */
	    install_new();
	    break;
	case 5: /* Quit */
	    quit_pkg = TRUE;
	    break;
	}
    }

    return;
} /* run_menu() */
	    
    
