/***************************************************************
 *
 * Program:	pkg_ui.c
 * Author:	Marc van Kempen
 * Desc:	user interface parts of pkg_manage
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
#include <unistd.h>
#include <sys/param.h>
#include "pkg_manage.h"
#include "dir.h"
#include "dialog.priv.h"
#include "ui_objects.h"

extern PKG_info p_inf;

/*
 * Functions
 */


void
view_installed(void)
/*
 * Desc: View the installed packages
 */
{
    int		i, quit_view, sc=0, ch=0;
    char	selection[1024];

    if (p_inf.Nitems == 0) {
	use_helpfile(NULL);
	use_helpline(NULL);
	dialog_notify("No packages installed or no info available");
	return;
    }
    use_helpfile(VIEW_INST_HLP);
    quit_view = FALSE;
    while (!quit_view) {
	use_helpline("F1=help, use arrow-keys or character to select option and press enter");
	if (dialog_menu("View installed packages",
			"Press enter to see the package descriptions",
			LINES, COLS, LINES-5, p_inf.Nitems,
			p_inf.mnu, selection, &ch, &sc )) {
	    quit_view = TRUE;
	} else {
	    i = get_pkg_index(selection);
	    use_helpline("F1=help, use PgUp and PgDn and arrow-keys to move through the text");
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
    char	tmp[512], args[512], selection[512], *tmp_file;
    int		quit_view, quit_del;
    int		i, sel, ch=0, sc=0, ch0=0, sc0=0, ret;

    if (p_inf.Nitems == 0) {
	use_helpfile(NULL);
	use_helpline(NULL);
	dialog_notify("No packages installed or no info available");
	return;
    }
    quit_view = FALSE;
    while (!quit_view) {
	use_helpline("F1=help, use arrow-keys or character to select option and press enter");
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
		use_helpline("F1=help, use arrow-keys or digit to select option and press enter");
		if (dialog_menu("Delete a package", tmp, 10, COLS-6,
				3, 3, mnu, selection, &ch0, &sc0)) {
		    quit_del = TRUE;
		} else {
		    sel = atoi(selection);
		    switch(sel) {
		    case 1:
			tmp_file = tempnam(NULL, "pkg.");
			sprintf(args, "-n %s", p_inf.name[i]);
			ret = exec_catch_errors(PKG_DELETE, args, tmp_file);
			if (!ret) {
			    dialog_textbox("Package deletion commands",
					   tmp_file, LINES, COLS);
			}
			unlink(tmp_file);
			free(tmp_file);
			break;
		    case 2:
			exec_catch_errors(PKG_DELETE, p_inf.name[i], NULL);
			get_pkginfo();
			if (ch >= p_inf.Nitems) { /* adjust pointers */
			    ch = p_inf.Nitems-1;
			    if (sc>ch) sc=ch;
			}
			quit_del = TRUE;
			if (p_inf.Nitems == 0) {
			    /* Quit 'delete-installed' when no packages available */
			    quit_view = TRUE;
			}
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
    char        *fname, *tmp_file;
    char        args[512], title[512];
    int		err;

    use_helpfile(PREVIEW_FS_HLP);
    use_helpline("Select package to preview");
    fname = dialog_fselect(".", "*.tgz");
    while (fname) {
        use_helpfile(PREVIEW_HLP);
        use_helpline("use PgUp and PgDn and arrow-keys to move through the text");
        tmp_file = tempnam(NULL, "pkg.");
	if (!tmp_file) {
	    fprintf(stderr, "preview_pkg: Could not allocate space for tmp_file\n");
	    exit(-1);
	}
        sprintf(args, "-n %s", fname);
	err = exec_catch_errors(PKG_ADD, args, tmp_file);
	if (!err) {
	    sprintf(title, "Preview package <%s>", fname);
	    dialog_textbox(title, tmp_file, LINES, COLS);
	}
	unlink(tmp_file);
        free(fname);
        free(tmp_file);
	use_helpfile(PREVIEW_FS_HLP);
	use_helpline("Select package to preview");
        fname = dialog_fselect(".", "*.tgz");
    }
    if (fname) free(fname);
    use_helpfile(NULL);
    use_helpline(NULL);

    return;
} /* preview_pkg() */

void
install_batch(void)
/*
 * Desc: Get the directory from which to list and install packages
 */
{
    int	quit;

    use_helpfile(DS_INSTALL_HLP);
    quit = FALSE;
    while (!quit) {
	use_helpline("Select directory where the pkg's reside");
	if (dialog_dselect(".", "*.tgz")) {
	    quit = TRUE;
	} else {
	    install_pkgs_indir();
	}
    }

    return;
} /* install_batch() */


void
install_pkgs_indir(void)
/*
 * Desc: install several packages.
 */
{
    WINDOW		*pkg_win, *w, *tmpwin;
    DirList		*d = NULL;
    char		**fnames, o_pkg[MAXPATHLEN], o_pkgi[MAXPATHLEN],
			**comment, **desc, **names, msg[512];
    int			n, nf, i, p, quit, j, *a, recalc;
    struct ComposeObj	*obj = NULL;
    ListObj		*pkg_obj, *pkgi_obj;
    ButtonObj		*installbut_obj, *cancelbut_obj;
    int			o_installbut, o_cancelbut, install;
    long		total_marked = 0, *sizes;
    char		*tmp_file, tmp_dir[MAXPATHLEN];

    /* list all packages in the current directory with a short description of
       the package. Pressing enter should give more info about a specific
       package. */

    /* save current window */
    tmpwin = dupwin(newscr);
    if (tmpwin == NULL) {
	endwin();
	fprintf(stderr, "\ninstall_pkgs_indir: dupwin(newscr) failed\n");
	exit(1);
    }

    use_helpline(NULL);
    pkg_win = newwin(LINES-4, COLS-12, 2, 5);
    if (pkg_win == NULL) {
	endwin();
	fprintf(stderr, "\nnewwin(%d,%d,%d,%d) failed, maybe wrong dims\n",
		LINES-4, COLS-10, 2, 5);
	exit(1);
    }
    draw_box(pkg_win, 0, 0, LINES-4, COLS-12, dialog_attr, border_attr);
    wattrset(pkg_win, dialog_attr);
    mvwaddstr(pkg_win, 0, (COLS-12)/2 - 12, " Install multiple packages ");
    draw_shadow(stdscr, 2, 5, LINES-4, COLS-12);
    use_helpline("Enter,F2=info, Space=mark, *=mark all, -=unmark all, TAB=move");
    display_helpline(pkg_win, LINES-5, COLS-12);
    wrefresh(pkg_win);

    /* now build a list of the packages in the chosen directory */
    /* and display them in a list */

    get_dir(".", "*.tgz", &d, &n);
    get_filenames(d, n, &fnames, &nf);
    FreeDir(d, n); 	/* free the space allocated to d */

    /* now get the description and comment and the name from the packages.  */
    /* If there is  no +COMMENT or +DESC in the package, then it's propably */
    /* not a package 							    */

    if (nf == 0) {
	dialog_notify("No installable packages in this directory");
	return;
    }

    names = (char **) malloc( sizeof(char *) * nf);
    if (!names) {
	fprintf(stderr, "install_batch(): Error mallocing space for names\n");
	exit(-1);
    }
    comment = (char **) malloc( sizeof(char *) * nf );
    if (!comment) {
	fprintf(stderr, "install_batch(): Error malloc'ing space for comment\n");
	exit(-1);
    }
    desc = (char **) malloc( sizeof(char *) * nf );
    if (!desc) {
	fprintf(stderr, "install_batch(): Error malloc'ing space for desc\n");
	exit(-1);
    }
    sizes = (long *) malloc( sizeof(long) * nf );
    if (!sizes) {
	fprintf(stderr, "install_batch(): Error malloc'ing space for desc\n");
	exit(-1);
    }

    /* get_desc extracts the info from the file names[i] and puts the */
    /* comment in comment[i] and the description in desc[i], space is */
    /* malloc'ed as needed, and should be freed when done with it.    */
    /* get_desc() returns FALSE when fnames[i] is not a package       */
    /* the name of the package is extracted from CONTENT and put in   */
    /* names */

    /* create a tmp directory in which the files will be extracted */

    tmp_file = tempnam("", "pkg.");
    if (!tmp_file) {
	fprintf(stderr, "install_batch(): Error malloc'ing space for tmpfile\n");
	exit(1);
    }
    if (getenv("TMPDIR")) {
	sprintf(tmp_dir, "%s/%s", getenv("TMP_DIR"), tmp_file);
    } else {
	sprintf(tmp_dir, "/usr/tmp/%s", tmp_file);
	mkdir("/usr/tmp", S_IRWXU);
    }
    free(tmp_file);
    if (mkdir(tmp_dir, S_IRWXU)) {
	dialog_notify("Could not create temporary directory in /usr/tmp, exiting");
	free(names);
	free(comment);
	free(desc);
	for (i=0; i<nf; i++) free(fnames[i]);
	free(fnames);
	delwin(pkg_win);
	return;
    }


    w = dupwin(curscr);
    if (!w) {
	fprintf(stderr, "install_batch(): Error malloc'ing new window\n");
	exit(-1);
    }
    a = (int *) malloc( nf * sizeof(int) );
    j = 0;
    for (i=0; i<nf; i++) {
	dialog_gauge("Scanning directory:", fnames[i], LINES/2-3, COLS/2-30,
		     7, 60, (int) ((float) (i+1)/nf*100));
	if (get_desc(fnames[i], &(names[i]), &(comment[i]),
		     &(desc[i]), &(sizes[i]), tmp_dir) == FALSE) {
	    a[j] = i;
	    j++;
	}
    }
    wrefresh(w);
    delwin(w);

    /* remove the tmp directory and change to the previous directory */
    if (rmdir(tmp_dir)) {
	dialog_notify("install_batch(): Error removing temporary directory");
	for (i=0; i<nf; i++) {
	    free(fnames[i]);
	    free(names[i]);
	    free(comment[i]);
	    free(desc[i]);
	}
	free(fnames);
	free(names);
	free(comment);
	free(desc);
	free(sizes);
	delwin(pkg_win);
    }

    /* Now we should have an array with indices of filenames that are not */
    /* packages, say a[0..k), 0<=k<=n, and the filenames itself           */
    /* remove the non-packages from the array 				  */

    i=0;
    p=0;
    while (i+p < nf) {
	if ((i+p == a[p]) && (p<j)) {
	    free(fnames[i+p]);
	    free(names[i+p]);
	    free(comment[i+p]);
	    free(desc[i+p]);
	    p++;
	} else {
	    fnames[i] = fnames[i+p];
	    names[i] = names[i+p];
	    comment[i] = comment[i+p];
	    desc[i] = desc[i+p];
	    sizes[i] = sizes[i+p];
	    i++;
	}
    }
    nf = nf - j;
    free(a);

    o_pkg[0] = '0';
    pkg_obj = NewListObj(pkg_win, "To be installed", names, o_pkg,
			 1, 2, LINES-11, COLS-50, nf);
    AddObj(&obj, LISTOBJ, (void *) pkg_obj);

    o_pkgi[0] = '0';
    pkgi_obj = NewListObj(pkg_win, "Already Installed", p_inf.name, o_pkgi,
			  1, COLS-45, LINES-11, COLS-50, p_inf.Nitems);
    AddObj(&obj, LISTOBJ, (void *) pkgi_obj);

    o_installbut = FALSE;
    installbut_obj = NewButtonObj(pkg_win, "Install marked", &o_installbut,
				  LINES-8, (COLS-12)/2 - 22);
    AddObj(&obj, BUTTONOBJ, (void *) installbut_obj);

    o_cancelbut = FALSE;
    cancelbut_obj = NewButtonObj(pkg_win, "Cancel", &o_cancelbut,
				 LINES-8, (COLS-12)/2 + 2);
    AddObj(&obj, BUTTONOBJ, (void *) cancelbut_obj);

    /* print total_marked in window */
    wmove(pkg_win, LINES-9, 2);
    wattrset(pkg_win, dialog_attr);
    waddstr(pkg_win, "Total marked =     0 kB");
    total_marked = 0;

    recalc = FALSE;
    quit = FALSE;
    install = FALSE;
    while (!quit) {
	use_helpfile(INSTALL_HLP);
	switch(PollObj(&obj)) {
	case SEL_CR:
	    /* first move back one list object */
	    if (obj->prev) obj=obj->prev;

	    if ((ListObj *) obj->obj == pkg_obj) {
		dialog_notify(comment[pkg_obj->sel]);
	    }
	    if ((ListObj *) obj->obj == pkgi_obj) {
		dialog_notify(p_inf.comment[pkgi_obj->sel]);
	    }
	    break;
	case SEL_BUTTON:
	    if (o_installbut) {
		install = TRUE;
		quit = TRUE;
	    }
	    if (o_cancelbut) {
		quit = TRUE;
	    }
	    break;
	case SEL_ESC:
	    quit = TRUE;
	    break;
	case ' ':
	    if ((ListObj *) obj->obj == pkg_obj) {
		MarkCurrentListObj(pkg_obj);
	    }
	    recalc = TRUE;
	    wrefresh(pkg_win);
	    break;
	case '*':
	    if ((ListObj *) obj->obj == pkg_obj) {
		MarkAllListObj(pkg_obj);
	    }
	    recalc = TRUE;
	    break;
	case '-':
	    if ((ListObj *) obj->obj == pkg_obj) {
		UnMarkAllListObj(pkg_obj);
	    }
	    recalc = TRUE;
	    break;
	case KEY_F(1):
	    display_helpfile();
	    break;
	case KEY_F(2):
	    if ((ListObj *) obj->obj == pkg_obj) {
		dialog_notify(desc[pkg_obj->sel]);
	    }
	    if ((ListObj *) obj->obj == pkgi_obj) {
		dialog_notify(p_inf.description[pkgi_obj->sel]);
	    }
	    break;
	}
	if (recalc) {
	    total_marked = 0;
	    for (i=0; i<nf; i++) {
		if (pkg_obj->seld[i]) {
		    total_marked += sizes[i];
		}
	    }
	    recalc = FALSE;
	    /* print total_marked in window */
	    wmove(pkg_win, LINES-9, 2);
	    wattrset(pkg_win, dialog_attr);
	    sprintf(msg, "Total marked = %6ld kB", (long) (total_marked / 1024));
	    waddstr(pkg_win, msg);
	    wrefresh(pkg_win);
	}
    }

    if (install) {
	int ninstalled = 0;

	/* check if any of the packages marked for installation are */
	/* already installed */
	i=0;
	n=0;
	while (i < nf) {
	    if ((pkg_obj->seld[i]) && (already_installed(names[i]))) {
		/* popup a warning and remove the package from the */
		/* packages that are going to be installed */
		sprintf(msg, " The following package is already installed:\n\n   %s (%s)\n\n",
			names[i], fnames[i]);
		strcat(msg, " This package will be skipped\n");
		strcat(msg, " If you want to install it anyway, remove it first");
		dialog_notify(msg);
		pkg_obj->seld[i] = FALSE;
	    }
	    if (pkg_obj->seld[i]) n++; /* count selected packages */
	    i++;
	}
	/* now install whatever is left */
	for (i=0; i<nf; i++) {
	    if (pkg_obj->seld[i]) {
		dialog_gauge("Installing packages:", names[i], LINES/2-3,
			     COLS/2-30, 7, 60, (int) ((float) ++ninstalled/n*100));
		install_package(fnames[i]);
	    }
	}
	if (n>0) get_pkginfo();
    }

    /* clean up */
    for (i=0; i<nf; i++) {
	free(fnames[i]);
	free(names[i]);
	free(comment[i]);
	free(desc[i]);
    }
    free(fnames);
    free(names);
    free(comment);
    free(desc);
    free(sizes);
    DelObj(obj);
    use_helpfile(NULL);
    use_helpline(NULL);
    delwin(pkg_win);

    touchwin(tmpwin);
    wrefresh(tmpwin);
    delwin(tmpwin);

    return;
} /* install_batch() */

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
	"3. Preview package", "Preview commands executed on installation",
	"4. Install packages", "Install one or more packages",
	"5. Quit", "Leave the program",
    };

    quit_pkg = FALSE;
    while (!quit_pkg) {
	use_helpline("F1=help, use arrow-keys or digit to select option and press enter");
	use_helpfile(MAIN_HLP);
	if (dialog_menu("Package Manager", "Choose one of the options",
		    LINES, COLS, 5, 5, pkg_menu, selection, &ch, &sc)) {
	    quit_pkg = TRUE;
	} else {
	    sel = atoi(selection);
	    switch(sel) {
	    case 1: /* View installed packages */
		view_installed();
		break;
	    case 2: /* Delete installed package */
		delete_installed();
		break;
	    case 3: /* Preview install */
		preview_pkg();
		break;
	    case 4: /* Install multiple packages */
		install_batch();
		break;
	    case 5: /* Quit */
		quit_pkg = TRUE;
		break;
	    }
	}
    }

    return;
} /* run_menu() */
