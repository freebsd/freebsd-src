/*
 * Program:	objects.c
 * Author:	Marc van Kempen
 * Desc:	Implementation of UI-objects:
 *		- String input fields
 *		- List selection
 *		- Buttons
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
#include <sys/param.h>
#include <ncurses.h>
#include <dialog.h>
#include "dialog.priv.h"
#include "ui_objects.h"

#define ESC 27


/***********************************************************************
 *
 * Obj routines
 *
 ***********************************************************************/

void
AddObj(ComposeObj **Obj, int objtype, void *obj)
/*
 * Desc: Add the object <obj> to the list of objects <Obj>
 */
{
    if (*Obj == NULL) {
	/* Create the root object */
	*Obj = (ComposeObj *) malloc( sizeof(ComposeObj) );
	if (!Obj) {
	    printf("AddObj: Error malloc'ing ComposeObj\n");
	    exit(-1);
	}
	(*Obj)->objtype = objtype;
	(*Obj)->obj = obj;
	(*Obj)->next = NULL;
	(*Obj)->prev = NULL;
    } else {
	ComposeObj	*o = *Obj;

	/* create the next object */
	while (o->next) o = (ComposeObj *) o->next;
	o->next = (struct ComposeObj *) malloc( sizeof(ComposeObj) );
	if (!o->next) {
	    printf("AddObj: Error malloc'ing o->next\n");
	    exit(-1);
	}
	o->next->objtype = objtype;
	o->next->obj = obj;
	o->next->next = NULL;
	o->next->prev = o;
    }

    return;
} /* AddObj() */

void
FreeObj(ComposeObj *Obj)
/*
 * Desc: free the memory occupied by *Obj
 */
{
    ComposeObj	*o = Obj;

    o = Obj;
    while (o) {
	o = Obj->next;
	free(Obj);
	Obj = o;
    }

    return;
} /* FreeObj() */


int
ReadObj(ComposeObj *Obj)
/*
 * Desc: navigate through the different objects calling their
 *	 respective navigation routines as necessary
 * Pre:  Obj != NULL
 */
{
    ComposeObj		*o;
    ComposeObj		*last;	 /* the last object in the list */
    int			ret;	 /* the return value from the selection routine */

    /* find the last object in the list */
    last = Obj;
    while (last->next) last = last->next;

    ret = 0;
    o = Obj;
    while ((ret != SEL_BUTTON) && (ret != SEL_ESC)) {
	switch(o->objtype) {
	case STRINGOBJ:
	    ret = SelectStringObj((StringObj *) o->obj);
	    break;
	case LISTOBJ:
	    ret = SelectListObj((ListObj *) o->obj);
	    break;
	case BUTTONOBJ:
	    ret = SelectButtonObj((ButtonObj *) o->obj);
	    break;
	}
	switch(ret) {
	case KEY_DOWN:
	case SEL_CR:
	case SEL_TAB:	/* move to the next object in the list */
	    if (o->next != NULL) {
		o = o->next;	/* next object */
	    } else {
		o = Obj;	/* beginning of the list */
	    }
	    break;

	case KEY_UP:
	case SEL_BACKTAB: /* move to the previous object in the list */
	    if (o->prev != NULL) {
		o = o->prev;	/* previous object */
	    } else {
		o = last;	/* end of the list */
	    }
	    break;

	case KEY_F(1): /* display help_file */
	case '?':
	    display_helpfile();
	    break;
	}
    }

    return(ret);

} /* ReadObj() */


int
PollObj(ComposeObj **Obj)
{
    ComposeObj		*last;	 /* the last object in the list */
    ComposeObj		*first;  /* the first object in the list */
    int			ret;	 /* the return value from the selection routine */

    /* find the last object in the list */
    last = *Obj;
    while (last->next) last = last->next;

    /* find the first object in the list */
    first = *Obj;
    while (first->prev) first = first->prev;

    ret = 0;
    switch((*Obj)->objtype) {
    case STRINGOBJ:
	ret = SelectStringObj((StringObj *) (*Obj)->obj);
	break;
    case LISTOBJ:
	ret = SelectListObj((ListObj *) (*Obj)->obj);
	break;
    case BUTTONOBJ:
	ret = SelectButtonObj((ButtonObj *) (*Obj)->obj);
	break;
    }
    switch(ret) {
    case KEY_DOWN:
    case SEL_CR:
    case SEL_TAB:		     /* move to the next object in the list */
	if ((*Obj)->next != NULL) {
	    *Obj = (*Obj)->next;     /* next object */
	} else {
	    *Obj = first;	     /* beginning of the list */
	}
	break;

    case KEY_UP:
    case SEL_BACKTAB: 		     /* move to the previous object in the list */
	if ((*Obj)->prev != NULL) {
	    *Obj = (*Obj)->prev;     /* previous object */
	} else {
	    *Obj = last;	     /* end of the list */
	}
	break;
    }

    return(ret);

} /* PollObj() */


void
DelObj(ComposeObj *Obj)
/*
 * Desc: Free all objects
 */
{
    ComposeObj	*o;

    o = Obj;
    while (Obj != NULL) {
	switch(Obj->objtype) {
	case STRINGOBJ:
	    DelStringObj((StringObj *) Obj->obj);
	    break;
	case LISTOBJ:
	    DelListObj((ListObj *) Obj->obj);
	    break;
	case BUTTONOBJ:
	    DelButtonObj((ButtonObj *) Obj->obj);
	    break;
	}
	Obj = Obj->next;
    }

    FreeObj(o);
} /* DelObj() */

/***********************************************************************
 *
 * StringObj routines
 *
 ***********************************************************************/

void
RefreshStringObj(StringObj *so)
/*
 * Desc: redraw the object
 */
{
    char tmp[512];

    wmove(so->win, so->y, so->x+1);
    wattrset(so->win, dialog_attr);
    waddstr(so->win, so->title);

    draw_box(so->win, so->y+1, so->x, 3, so->w, dialog_attr, border_attr);
    wattrset(so->win, item_attr);
    wmove(so->win, so->y+2, so->x+1);
    if (strlen(so->s) > so->w-2) {
	strncpy(tmp, (char *) so->s + strlen(so->s) - so->w + 2, so->w - 1);
	waddstr(so->win, tmp);
    } else {
	waddstr(so->win, so->s);
    }

    return;
} /* RefreshStringObj() */

StringObj *
NewStringObj(WINDOW *win, char *title, char *s, int y, int x, int w, int len)
/*
 * Desc: Initialize a new stringobj and return a pointer to it.
 *	 Draw the object on the screen at the specified coordinates
 */
{
    StringObj	*so;

    /* Initialize a new object */
    so = (StringObj *) malloc( sizeof(StringObj) );
    if (!so) {
	printf("NewStringObj: Error malloc'ing StringObj\n");
	exit(-1);
    }
    so->title = (char *) malloc( strlen(title) + 1);
    if (!so->title) {
	printf("NewStringObj: Error malloc'ing so->title\n");
	exit(-1);
    }
    strcpy(so->title, title);
    so->s = s;
    strcpy(so->s, s);
    so->x = x;
    so->y = y;
    so->w = w;
    so->len = len;
    so->win = win;

    /* Draw it on the screen */
    RefreshStringObj(so);

    return(so);
} /* NewStringObj() */

int
SelectStringObj(StringObj *so)
/*
 * Desc: get input using the info in <so>
 */
{
    int     	key;
    char	tmp[so->len+1];

    strcpy(tmp, so->s);
    key = line_edit(so->win, so->y+2, so->x+1,
		    so->len, so->w-2, inputbox_attr, TRUE, tmp);
    if ((key == '\n') || (key == '\r') || (key == '\t') || key == (KEY_BTAB) ) {
	strcpy(so->s, tmp);
    }
    RefreshStringObj(so);
    if (key == ESC) {
	return(SEL_ESC);
    }
    if (key == '\t') {
	return(SEL_TAB);
    }
    if ( (key == KEY_BTAB) || (key == KEY_F(2)) ) {
	return(SEL_BACKTAB);
    }
    if ((key == '\n') || (key == '\r')) {
	return(SEL_CR);
    }
    return(key);
} /* SelectStringObj() */


void
DelStringObj(StringObj *so)
/*
 * Desc: Free the space occupied by <so>
 */
{
   free(so->title);
   free(so);

   return;
}

/***********************************************************************
 *
 * ListObj routines
 *
 ***********************************************************************/

void
DrawNames(ListObj *lo)
/*
 * Desc: Just refresh the names, not the surrounding box and title
 */
{
    int 	i, j, h, x, y;
    char	tmp[MAXPATHLEN];

    x = lo->x + 1;
    y = lo->y + 2;
    h = lo->h - 2;
    for (i=lo->scroll; i<lo->n && i<lo->scroll+h; i++) {
	wmove(lo->win, y+i-lo->scroll, x);
	if (lo->seld[i]) {
	    wattrset(lo->win, A_BOLD);
	} else {
	    wattrset(lo->win, item_attr);
	}
	if (strlen(lo->name[i]) > lo->w-2) {
	    strncpy(tmp, lo->name[i], lo->w-2);
	    tmp[lo->w - 2] = 0;
	    waddstr(lo->win, tmp);
	} else {
	    waddstr(lo->win, lo->name[i]);
	    for (j=strlen(lo->name[i]); j<lo->w-2; j++) waddstr(lo->win, " ");
	}
    }
    wattrset(lo->win, item_attr);
    while (i<lo->scroll+h) {
	wmove(lo->win, y+i-lo->scroll, x);
	for (j=0; j<lo->w-2; j++) waddstr(lo->win, " ");
	i++;
    }

    return;
} /* DrawNames() */

void
RefreshListObj(ListObj *lo)
/*
 * Desc: redraw the list object
 */
{
    char 	perc[7];

    /* setup the box */
    wmove(lo->win, lo->y, lo->x+1);
    wattrset(lo->win, dialog_attr);
    waddstr(lo->win, lo->title);
    draw_box(lo->win, lo->y+1, lo->x, lo->h, lo->w, dialog_attr, border_attr);

    /* draw the names */
    DrawNames(lo);

    /* Draw % indication */
    sprintf(perc, "(%3d%%)", MIN(100, (int) (100 * (lo->sel+lo->h-2) / MAX(1, lo->n))));
    wmove(lo->win, lo->y + lo->h, lo->x + lo->w - 8);
    wattrset(lo->win, dialog_attr);
    waddstr(lo->win, perc);


    return;
} /* RefreshListObj() */

ListObj *
NewListObj(WINDOW *win, char *title, char **list, char *listelt, int y, int x,
	   int h, int w, int n)
/*
 * Desc: create a listobj, draw it on the screen and return a pointer to it.
 */
{
    ListObj	*lo;
    int		i;

    /* Initialize a new object */
    lo = (ListObj *) malloc( sizeof(ListObj) );
    if (!lo) {
	fprintf(stderr, "NewListObj: Error malloc'ing ListObj\n");
	exit(-1);
    }
    lo->title = (char *) malloc( strlen(title) + 1);
    if (!lo->title) {
	fprintf(stderr, "NewListObj: Error malloc'ing lo->title\n");
	exit(-1);
    }
    strcpy(lo->title, title);
    lo->name = list;
    if (n>0) {
        lo->seld = (int *) malloc( n * sizeof(int) );
        if (!lo->seld) {
            fprintf(stderr, "NewListObj: Error malloc'ing lo->seld\n");
            exit(-1);
        }
        for (i=0; i<n; i++) {
            lo->seld[i] = FALSE;
        }
    } else {
        lo->seld = NULL;
    }
    lo->y = y;
    lo->x = x;
    lo->w = w;
    lo->h = h;
    lo->n = n;
    lo->scroll = 0;
    lo->sel = 0;
    lo->elt = listelt;
    lo->win = win;

    /* Draw the object on the screen */
    RefreshListObj(lo);

    return(lo);
} /* NewListObj() */

void
UpdateListObj(ListObj *lo, char **list, int n)
/*
 * Desc: Update the list in the listobject with the provided list
 * Pre:  lo->name "has been freed"
 *	 "(A i: 0<=i<lo->n: "lo->name[i] has been freed")"
 */
{
    int i;

    if (lo->seld) {
	free(lo->seld);
    }

    /* Rewrite the list in the object */
    lo->name = list;
    if (n>0) {
	lo->seld = (int *) malloc( n * sizeof(int) );
	if (!lo->seld) {
	    fprintf(stderr, "UpdateListObj: Error malloc'ing lo->seld\n");
	    exit(-1);
	}
	for (i=0; i<n; i++) {
	    lo->seld[i] = FALSE;
	}
    } else {
        lo->seld = NULL;
    }
    lo->n = n;
    lo->scroll = 0;
    lo->sel = 0;

    /* Draw the object on the screen */
    RefreshListObj(lo);

    return;
} /* UpdateListObj() */

int
SelectListObj(ListObj *lo)
/*
 * Desc: get a listname (or listnames), TAB to move on, or ESC ESC to exit
 * Pre:	 lo->n >= 1
 */
{
    int 	key, sel_x, sel_y, quit;
    char	tmp[MAXPATHLEN];
    char	perc[4];

    sel_x = lo->x+1;
    sel_y = lo->y + 2 + lo->sel - lo->scroll;

    if (lo->n == 0) return(SEL_TAB);

    keypad(lo->win, TRUE);

    /* Draw current selection in inverse video */
    wmove(lo->win, sel_y, sel_x);
    wattrset(lo->win, item_selected_attr);
    waddstr(lo->win, lo->name[lo->sel]);

    key = wgetch(lo->win);
    quit = FALSE;
    while ((key != '\t') && (key != '\n') && (key != '\r')
	   && (key != ESC) && (key != KEY_F(1)) && (key != '?') && !quit) {
	/* first draw current item in normal video */
	wmove(lo->win, sel_y, sel_x);
	if (lo->seld[lo->sel]) {
	    wattrset(lo->win, A_BOLD);
	} else {
	    wattrset(lo->win, item_attr);
	}
	if (strlen(lo->name[lo->sel]) > lo->w - 2) {
	    strncpy(tmp, lo->name[lo->sel], lo->w - 2);
	    tmp[lo->w - 2] = 0;
	    waddstr(lo->win, tmp);
	} else {
	    waddstr(lo->win, lo->name[lo->sel]);
	}

	switch (key) {
	case KEY_DOWN:
	case ctrl('n'):
	    if (sel_y < lo->y + lo->h-1) {
		if (lo->sel < lo->n-1) {
		    sel_y++;
		    lo->sel++;
		}
	    } else {
		if (lo->sel < lo->n-1) {
		    lo->sel++;
		    lo->scroll++;
		    DrawNames(lo);
		    wrefresh(lo->win);
		}
	    }
	    break;
	case KEY_UP:
	case ctrl('p'):
	    if (sel_y > lo->y+2) {
		if (lo->sel > 0) {
		    sel_y--;
		    lo->sel--;
		}
	    } else {
		if (lo->sel > 0) {
		    lo->sel--;
		    lo->scroll--;
		    DrawNames(lo);
		    wrefresh(lo->win);
		}
	    }
	    break;
	case KEY_HOME:
	case ctrl('a'):
	    lo->sel = 0;
	    lo->scroll = 0;
	    sel_y = lo->y + 2;
	    DrawNames(lo);
	    wrefresh(lo->win);
	    break;
	case KEY_END:
	case ctrl('e'):
	    if (lo->n < lo->h - 3) {
		lo->sel = lo->n-1;
		lo->scroll = 0;
		sel_y = lo->y + 2 + lo->sel - lo->scroll;
	    } else {
		/* more than one page of list */
		lo->sel = lo->n-1;
		lo->scroll = lo->n-1 - (lo->h-3);
		sel_y = lo->y + 2 + lo->sel - lo->scroll;
		DrawNames(lo);
		wrefresh(lo->win);
	    }
	    break;
	case KEY_NPAGE:
	case ctrl('f'):
	    lo->sel += lo->h - 2;
	    if (lo->sel >= lo->n) lo->sel = lo->n - 1;
	    lo->scroll += lo->h - 2;
	    if (lo->scroll >= lo->n - 1) lo->scroll = lo->n - 1;
	    if (lo->scroll < 0) lo->scroll = 0;
	    sel_y = lo->y + 2 + lo->sel - lo->scroll;
	    DrawNames(lo);
	    wrefresh(lo->win);
	    break;
	case KEY_PPAGE:
	case ctrl('b'):
	    lo->sel -= lo->h - 2;
	    if (lo->sel < 0) lo->sel = 0;
	    lo->scroll -= lo->h - 2;
	    if (lo->scroll < 0) lo->scroll = 0;
	    sel_y = lo->y + 2 + lo->sel - lo->scroll;
	    DrawNames(lo);
	    wrefresh(lo->win);
	    break;
	default:
	    quit = TRUE;
	    break;
	}
	/* Draw % indication */
	sprintf(perc, "(%3d%%)", MIN(100, (int)
				     (100 * (lo->sel+lo->h - 2) / MAX(1, lo->n))));
	wmove(lo->win, lo->y + lo->h, lo->x + lo->w - 8);
	wattrset(lo->win, dialog_attr);
	waddstr(lo->win, perc);

	/* draw current item in inverse */
	wmove(lo->win, sel_y, sel_x);
	wattrset(lo->win, item_selected_attr);
	if (strlen(lo->name[lo->sel]) > lo->w - 2) {
	    /* when printing in inverse video show the last characters in the */
	    /* name that will fit in the window */
	    strncpy(tmp,
		    lo->name[lo->sel] + strlen(lo->name[lo->sel]) - (lo->w - 2),
		    lo->w - 2);
	    tmp[lo->w - 2] = 0;
	    waddstr(lo->win, tmp);
	} else {
	    waddstr(lo->win, lo->name[lo->sel]);
	}
	if (!quit) key = wgetch(lo->win);
    }

    if (key == ESC) {
	return(SEL_ESC);
    }
    if (key == '\t') {
	return(SEL_TAB);
    }
    if ((key == KEY_BTAB) || (key == ctrl('b'))) {
	return(SEL_BACKTAB);
    }
    if ((key == '\n') || (key == '\r')) {
	strcpy(lo->elt, lo->name[lo->sel]);
	return(SEL_CR);
    }
    return(key);
} /* SelectListObj() */

void
DelListObj(ListObj *lo)
/*
 * Desc: Free the space occupied by the listobject
 */
{
    free(lo->title);
    if (lo->seld != NULL) free(lo->seld);
    free(lo);

    return;
} /* DelListObj() */

void
MarkCurrentListObj(ListObj *lo)
/*
 * Desc: mark the current item for the selection list
 */
{
    lo->seld[lo->sel] = !(lo->seld[lo->sel]);
    DrawNames(lo);

    return;
} /* MarkCurrentListObj() */

void
MarkAllListObj(ListObj *lo)
/*
 * Desc: mark all items
 */
{
    int i;

    for (i=0; i<lo->n; i++) {
        lo->seld[i] = TRUE;
    }
    DrawNames(lo);

    return;
} /* MarkAllListObj() */

void
UnMarkAllListObj(ListObj *lo)
/*
 * Desc: unmark all items
 */
{
    int i;

    for (i=0; i<lo->n; i++) {
        lo->seld[i] = FALSE;
    }
    DrawNames(lo);

    return;
} /* UnMarkAllListObj() */


/***********************************************************************
 *
 * ButtonObj routines
 *
 ***********************************************************************/


void
RefreshButtonObj(ButtonObj *bo)
/*
 * Desc: redraw the button
 */
{
    draw_box(bo->win, bo->y, bo->x, 3, bo->w, dialog_attr, border_attr);
    print_button(bo->win, bo->title, bo->y+1, bo->x+2, FALSE);

    return;
} /* RefreshButtonObj() */

ButtonObj *
NewButtonObj(WINDOW *win, char *title, int *pushed, int y, int x)
/*
 * Desc: Create a new button object
 */
{
    ButtonObj	*bo;

    bo = (ButtonObj *) malloc( sizeof(ButtonObj) );

    bo->win = win;
    bo->title = (char *) malloc( strlen(title) + 1);
    strcpy(bo->title, title);
    bo->x = x;
    bo->y = y;
    bo->w = strlen(title) + 6;
    bo->h = 3;
    bo->pushed = pushed;

    RefreshButtonObj(bo);

    return(bo);
} /* NewButtonObj() */

int
SelectButtonObj(ButtonObj *bo)
/*
 * Desc: Wait for buttonpresses or TAB's to move on, or ESC ESC
 */
{
    int	key;

    print_button(bo->win, bo->title, bo->y+1, bo->x+2, TRUE);
    wmove(bo->win, bo->y+1, bo->x+(bo->w/2)-1);
    key = wgetch(bo->win);
    print_button(bo->win, bo->title, bo->y+1, bo->x+2, FALSE);
    switch(key) {
    case '\t':
	return(SEL_TAB);
	break;
    case KEY_BTAB:
    case ctrl('b'):
	return(SEL_BACKTAB);
    case '\n':
	*(bo->pushed) = TRUE;
	return(SEL_BUTTON);
	break;
    case ESC:
	return(SEL_ESC);
	break;
    default:
	return(key);
	break;
    }
} /* SelectButtonObj() */

void
DelButtonObj(ButtonObj *bo)
/*
 * Desc: Free the space occupied by <bo>
 */
{
    free(bo->title);
    free(bo);

    return;
} /* DelButtonObj() */
