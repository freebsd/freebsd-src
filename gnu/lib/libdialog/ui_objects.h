/*
 * Author: Marc van Kempen
 * Desc:   include file for UI-objects
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

#include "dialog.h"
#include <ncurses.h>

/***********************************************************************
 *
 * Defines
 *
 ***********************************************************************/

#define ctrl(a)		((a) - 'a' + 1)

/* the Object types */
#define STRINGOBJ	1
#define LISTOBJ		2
#define BUTTONOBJ	3

/* the return signals from the selection routines */
/* 1000 and higher should avoid conflicts with keys pressed */
#define SEL_CR		1001	/* return was pressed */
#define SEL_ESC		1002	/* ESC pressed */
#define SEL_TAB		1003	/* TAB pressed */
#define SEL_BACKTAB	1004	/* SHIFT-TAB pressed */
#define SEL_BUTTON	1005	/* a button was pressed */

/***********************************************************************
 *
 * Typedefs
 *
 ***********************************************************************/

typedef struct {
    WINDOW	*win;		/* the window it's contained in */
    char	*title;		/* the prompt for the input field */
    char 	*s;		/* initial value of the input field */
    int		x, y, w, len;	/* the (y, x) position of the upperleft */
				/* corner and the width <w> of the display */
				/* and length <len> of the field */
    int		attr_mask;	/* special attributes */
} StringObj;

typedef struct {
    WINDOW	*win;		/* the windows it's contained in */
    char	*title;		/* the title of the list */
    char 	**name;		/* the names of the list */
    int		*seld;		/* the currently selected names */
    char	*elt;		/* the current element in the list list[sel] */
    int		x, y, w, h, n;	/* dimensions of list and # of elements (n) */
    int		scroll, sel;	/* current position in the list */
} ListObj;

typedef struct {
    WINDOW	*win;		/* the window it's contained in */
    char	*title;		/* title for the button */
    int		x, y, w, h; 	/* its dimensions */
    int		*pushed;	/* boolean that determines wether button was pushed */
} ButtonObj;

typedef struct ComposeObj {
    int			objtype;
    void		*obj;
    struct ComposeObj	*next, *prev;
} ComposeObj;

/**********************************************************************
 *
 * Prototypes
 *
 **********************************************************************/

void		RefreshStringObj(StringObj *so);
StringObj 	*NewStringObj(WINDOW *win, char *title, char *s,
			      int y, int x, int w, int len);
int		SelectStringObj(StringObj *so);
void		DelStringObj(StringObj *so);

void		RefreshListObj(ListObj *lo);
ListObj 	*NewListObj(WINDOW *win, char *title, char **list,
			    char *listelt, int y, int x, int h, int w, int n);
void		UpdateListObj(ListObj *lo, char **list, int n);
int		SelectListObj(ListObj *lo);
void		DelListObj(ListObj *obj);
void            MarkCurrentListObj(ListObj *lo);
void            MarkAllListObj(ListObj *lo);
void            UnMarkAllListObj(ListObj *lo);

void		RefreshButtonObj(ButtonObj *bo);
ButtonObj 	*NewButtonObj(WINDOW *win, char *title, int *pushed,
			      int y, int x);
int		SelectButtonObj(ButtonObj *bo);
void		DelButtonObj(ButtonObj *bo);
void		AddObj(ComposeObj **Obj, int objtype, void *obj);
void		FreeObj(ComposeObj *Obj);
int		ReadObj(ComposeObj *Obj);
int		PollObj(ComposeObj **Obj);
void		DelObj(ComposeObj *Obj);

