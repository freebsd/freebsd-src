/*
 * program:	fselect.c
 * author:	Marc van Kempen (wmbfmk@urc.tue.nl)
 * Desc:	File selection routine
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
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <dialog.h>
#include "ui_objects.h"
#include "dir.h"
#include "dialog.priv.h"

/*
 * Local prototypes
 */

char *dialog_dfselect(char *dir, char *fmask, int is_fselect);

/*
 * Functions
 */

void
get_directories(DirList *d, int n, char ***names, int *nd)
/*
 * Desc: return the directorienames in <dir> as an array in
 *	 <names>, the # of entries in <nd>, memory allocated
 *       to *names should be freed when done with it.
 */
{
    int i;

    /* count the directories, which are in front */
    *nd = 0;
    while ((*nd < n) && (S_ISDIR(d[*nd].filestatus.st_mode))) (*nd)++;
    *names = (char **) malloc( *nd * sizeof(char *) );
    for (i=0; i<*nd; i++) {
	(*names)[i] = (char *) malloc( strlen(d[i].filename) + 1);
	strcpy((*names)[i], d[i].filename);
    }

    return;
} /* get_directories() */

void
get_filenames(DirList *d, int n, char ***names, int *nf)
/*
 * Desc: return the filenames in <dir> as an arry in
 *  	 <names>, the # of entries in <nf>, memory allocated
 *	 to *names should be freed when done.
 */
{
    int nd, i;

    /* the # of regular files is the total # of files - # of directories */
    /* count the # of directories */
    nd = 0;
    while ((nd < n) && (S_ISDIR(d[nd].filestatus.st_mode))) nd++;

    *names = (char **) malloc( (n-nd) * sizeof(char *) );
    *nf = n - nd;
    for (i=0; i<*nf; i++) {
	(*names)[i] = (char *) malloc( strlen(d[i+nd].filename) + 1);
	strcpy((*names)[i], d[i+nd].filename);
    }

    return;
} /* get_filenames() */

void
FreeNames(char **names, int n)
/*
 * Desc: free the space occupied by names
 */
{
    int i;

     /* free the space occupied by names */
    for (i=0; i<n; i++) {
	free(names[i]);
    }
    free(names);

    return;
} /* FreeNames() */

int
dialog_dselect_old(void)
/*
 * Desc: starting from the current directory,
 *	 choose a new current directory
 */
{
    DirList		*d = NULL;
    char		**names, old_dir[MAXPATHLEN];
    WINDOW		*ds_win;
    ButtonObj		*okbut, *cancelbut;
    ListObj		*dirs_obj;
    StringObj		*dir_obj;
    char		o_dir[MAXPATHLEN];
    struct ComposeObj	*obj = NULL;
    int			n, nd, okbutton, cancelbutton,
			quit, cancel, ret;

    ds_win = newwin(LINES-8, COLS-30, 4, 15);
    if (ds_win == NULL) {
	fprintf(stderr, "\nnewwin(%d,%d,%d,%d) failed, maybe wrong dims\n",
 		LINES-8, COLS-30, 4, 15);
	exit(1);
    }
    draw_box(ds_win, 0, 0, LINES-8, COLS-30, dialog_attr, border_attr);
    wattrset(ds_win, dialog_attr);
    mvwaddstr(ds_win, 0, (COLS-30)/2 - 9, " Directory Select ");
    draw_shadow(stdscr, 4, 15, LINES-8, COLS-30);
    display_helpline(ds_win, LINES-9, COLS-30);

    /* the Directory string input field */
    getcwd(o_dir, MAXPATHLEN);
    dir_obj = NewStringObj(ds_win, "Directory:", o_dir, 1, 2, COLS-34, MAXPATHLEN-1);
    AddObj(&obj, STRINGOBJ, (void *) dir_obj);

    /* the list of directories */
    get_dir(".", "*", &d, &n);
    get_directories(d, n, &names, &nd);
    dirs_obj = NewListObj(ds_win, "Directories:", names, o_dir, 5, 2,
			  LINES-15, COLS-48, nd);
    AddObj(&obj, LISTOBJ, (void *) dirs_obj);

    /* the Ok-button */
    okbutton = FALSE;
    okbut = NewButtonObj(ds_win, "Continue", &okbutton, 7, COLS-45);
    AddObj(&obj, BUTTONOBJ, (void *) okbut);

    /* the Cancel-button */
    cancelbutton = FALSE;
    cancelbut = NewButtonObj(ds_win, "Return", &cancelbutton, 11, COLS-44);
    AddObj(&obj, BUTTONOBJ, (void *) cancelbut);

    quit = FALSE;
    cancel = FALSE;
    strcpy(old_dir, o_dir);
    while (!quit) {
	ret = PollObj(&obj);
	switch(ret) {
	case SEL_BUTTON:
	    if (okbutton) {
		quit = TRUE;
	    }
	    if (cancelbutton) {
		quit = TRUE;
		cancel = TRUE;
	    }
	    break;
	case SEL_CR:
	    if (strcmp(old_dir, o_dir)) {
		/* the directory was changed, cd into it */
		if (chdir(o_dir)) {
		    dialog_notify("Could not change into directory");
		    strcpy(o_dir, old_dir);
		} else {
		    getcwd(o_dir, MAXPATHLEN);
		    strcpy(old_dir, o_dir);
		}
		RefreshStringObj(dir_obj);
	    }
	    get_dir(".", "*", &d, &n);
	    FreeNames(names, nd);
	    get_directories(d, n, &names, &nd);
	    UpdateListObj(dirs_obj, names, nd);
	    if (((obj->prev)->obj == (void *) dirs_obj)) {
		obj=obj->prev;
	    }
	    break;
	case SEL_ESC:
	    quit = TRUE;
	    cancel = TRUE;
	    break;
	case KEY_F(1):
	    display_helpfile();
	    break;
	}
    }

    FreeNames(names, nd);
    DelObj(obj);
    delwin(ds_win);

    return(cancel);

} /* dialog_dselect() */

int
dialog_dselect(char *dir, char *fmask)
/*
 * Desc: Choose a directory
 */
{
    if (dialog_dfselect(dir, fmask, FALSE)) {
	return(FALSE);	/* esc or cancel was pressed */
    } else {
	return(TRUE);   /* directory was selected */
    }
} /* dialog_dselect() */

char *
dialog_fselect(char *dir, char *fmask)
/*
 * Desc: Choose a file from a directory
 */
{
    return(dialog_dfselect(dir, fmask, TRUE));
} /* dialog_fselect() */

char *
dialog_dfselect(char *dir, char *fmask, int is_fselect)
/*
 * Desc: choose a file from the directory <dir>, which
 *	 initially display files with the mask <filemask>
 * pre:  <dir> is the initial directory
 *	 only files corresponding to the mask <fmask> are displayed
 * post: returns NULL if no file was selected
 *       else returns pointer to filename, space is allocated, should
 *       be freed after use.
 */
{
    DirList 		*d = NULL;
    char		msg[512];
    char		**fnames, **dnames, *ret_name;
    WINDOW		*fs_win;
    int			n, nd, nf, ret;
    StringObj		*fm_obj, *dir_obj, *sel_obj;
    char		o_fm[255], o_dir[MAXPATHLEN], o_sel[MAXPATHLEN];
    char		old_fmask[255], old_dir[MAXPATHLEN];
    ListObj		*dirs_obj,   *files_obj;
    struct ComposeObj	*obj = NULL, *o;
    int 		quit, cancel;
    ButtonObj		*okbut_obj, *canbut_obj;
    int			ok_button, cancel_button;

    if (chdir(dir)) {
	sprintf(msg, "Could not move into specified directory: %s", dir);
	dialog_notify(msg);
	return(NULL);
    }
    getcwd(o_dir, MAXPATHLEN);

    /* setup the fileselect-window and initialize its components */
    fs_win = newwin(LINES-2, COLS-20, 1, 10);
    if (fs_win == NULL) {
	endwin();
	fprintf(stderr, "\nnewwin(%d,%d,%d,%d) failed, maybe wrong dims\n",
		LINES-2, COLS-20, 2, 10);
	exit(1);
    }
    draw_box(fs_win, 0, 0, LINES-2, COLS-20, dialog_attr, border_attr);
    wattrset(fs_win, dialog_attr);
    if (is_fselect) {
	mvwaddstr(fs_win, 0, (COLS-20)/2 - 7, " File Select ");
    } else {
	mvwaddstr(fs_win, 0, (COLS-20)/2 - 9, " Directory Select ");
    }
    draw_shadow(stdscr, 1, 10, LINES-2, COLS-20);
    display_helpline(fs_win, LINES-3, COLS-20);

    /* Filemask entry */
    strcpy(o_fm, fmask);
    fm_obj = NewStringObj(fs_win, "Filemask:", o_fm, 1, 2, 19, 255);
    AddObj(&obj, STRINGOBJ, (void *) fm_obj);

    /* Directory entry */
    dir_obj = NewStringObj(fs_win, "Directory:", o_dir, 1, 22, COLS-44, 255);
    AddObj(&obj, STRINGOBJ, (void *) dir_obj);

    /* Directory list */
    get_dir(".", fmask, &d, &n);	/* read the entire directory */
    get_directories(d, n, &dnames, &nd); /* extract the dir-entries */
    if (is_fselect) {
	dirs_obj = NewListObj(fs_win, "Directories:", dnames, o_dir, 5, 2,
			      LINES-16, (COLS-20)/2-2, nd);
    } else {
	dirs_obj = NewListObj(fs_win, "Directories:", dnames, o_dir, 5, 2,
			      LINES-12, (COLS-20)/2-2, nd);
    }
    AddObj(&obj, LISTOBJ, (void *) dirs_obj);

    /* Filenames list */
    get_filenames(d, n, &fnames, &nf);		/* extract the filenames */
    if (is_fselect) {
	files_obj = NewListObj(fs_win, "Files:", fnames, o_sel, 5, (COLS-20)/2+1,
			       LINES-16, (COLS-20)/2-3, nf);
    } else {
	files_obj = NewListObj(fs_win, "Files:", fnames, o_sel, 5, (COLS-20)/2+1,
			       LINES-12, (COLS-20)/2-3, nf);
    }
    AddObj(&obj, LISTOBJ, (void *) files_obj);

    if (is_fselect) {
	/* Selection entry */
	o_sel[0] = '\0';
	sel_obj = NewStringObj(fs_win, "Selection:", o_sel, LINES-10, 2, COLS-24, 255);
	AddObj(&obj, STRINGOBJ, (void *) sel_obj);
    }

    /* Ok button */
    ok_button = FALSE;
    okbut_obj = NewButtonObj(fs_win, "Ok", &ok_button, LINES-6, 20);
    AddObj(&obj, BUTTONOBJ, (void *) okbut_obj);

    /* Cancel button */
    cancel_button = FALSE;
    canbut_obj = NewButtonObj(fs_win, "Cancel", &cancel_button, LINES-6, 30);
    AddObj(&obj, BUTTONOBJ, (void *) canbut_obj);

    /* Make sure all objects on the window are drawn */
    wrefresh(fs_win);
    keypad(fs_win, TRUE);

    /* Start the reading */
    o = obj;
    strcpy(old_fmask, o_fm);
    strcpy(old_dir, o_dir);
    quit = FALSE;
    cancel = FALSE;
    while (!quit) {
	ret = PollObj(&o);
	switch(ret) {
	case SEL_CR:
	    if (strcmp(old_fmask, o_fm) || strcmp(old_dir, o_dir)) {
		/* reread directory and update the listobjects */
		if (strcmp(old_dir, o_dir)) { /* dir entry was changed */
		    if (chdir(o_dir)) {
			dialog_notify("Could not change into directory");
			strcpy(o_dir, old_dir);
		    } else {
			getcwd(o_dir, MAXPATHLEN);
			strcpy(old_dir, o_dir);
		    }
		    RefreshStringObj(dir_obj);
		} else {		      /* fmask entry was changed */
		    strcpy(old_fmask, o_fm);
		}
		get_dir(".", o_fm, &d, &n);
		FreeNames(dnames, nd);
		get_directories(d, n, &dnames, &nd);
		UpdateListObj(dirs_obj, dnames, nd);
		FreeNames(fnames, nf);
		get_filenames(d, n, &fnames, &nf);
		UpdateListObj(files_obj, fnames, nf);
		if (((o->prev)->obj == (void *) dirs_obj)) {
		    o=o->prev;
		}
	    }
	    break;
	case SEL_BUTTON:
	    /* check which button was pressed */
	    if (ok_button) {
		quit = TRUE;
	    }
	    if (cancel_button) {
		quit = TRUE;
		cancel = TRUE;
	    }
	    break;
	case SEL_ESC:
	    quit = TRUE;
	    cancel = TRUE;
	    break;
	case KEY_F(1):
	case '?':
	    display_helpfile();
	    break;
	}
    }
    DelObj(obj);
    FreeNames(dnames, nd);
    FreeNames(fnames, nf);
    delwin(fs_win);

    if (cancel || (strlen(o_sel) == 0)) {
	return(NULL);
    } else {
	ret_name = (char *) malloc( strlen(o_sel) + 1 );
	strcpy(ret_name, o_sel);
	return(ret_name);
    }
} /* dialog_fselect() */

