/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *   Author: Juergen Pfeifer <juergen.pfeifer@gmx.net> 1995,1997            *
 ****************************************************************************/

#ifndef ETI_MENU
#define ETI_MENU

#include <curses.h>
#include <eti.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int Menu_Options;
typedef int Item_Options;

/* Menu options: */
#define O_ONEVALUE      (0x01)
#define O_SHOWDESC      (0x02)
#define O_ROWMAJOR      (0x04)
#define O_IGNORECASE    (0x08)
#define O_SHOWMATCH     (0x10)
#define O_NONCYCLIC     (0x20)

/* Item options: */
#define O_SELECTABLE    (0x01)

typedef struct
{
  const char* str;
  unsigned short length;
} TEXT;

typedef struct tagITEM 
{
  TEXT           name;        /* name of menu item                         */
  TEXT           description; /* description of item, optional in display  */ 
  struct tagMENU *imenu;      /* Pointer to parent menu                    */
  void           *userptr;    /* Pointer to user defined per item data     */ 
  Item_Options   opt;         /* Item options                              */ 
  short          index;       /* Item number if connected to a menu        */
  short          y;           /* y and x location of item in menu          */
  short          x;
  bool           value;       /* Selection value                           */
                             
  struct tagITEM *left;       /* neighbour items                           */
  struct tagITEM *right;
  struct tagITEM *up;
  struct tagITEM *down;

} ITEM;

typedef void (*Menu_Hook)(struct tagMENU *);

typedef struct tagMENU 
{
  short          height;                /* Nr. of chars high               */
  short          width;                 /* Nr. of chars wide               */
  short          rows;                  /* Nr. of items high               */
  short          cols;                  /* Nr. of items wide               */
  short          frows;                 /* Nr. of formatted items high     */
  short          fcols;                 /* Nr. of formatted items wide     */
  short          arows;                 /* Nr. of items high (actual)      */
  short          namelen;               /* Max. name length                */
  short          desclen;               /* Max. description length         */
  short          marklen;               /* Length of mark, if any          */
  short          itemlen;               /* Length of one item              */
  short          spc_desc;              /* Spacing for descriptor          */
  short          spc_cols;              /* Spacing for columns             */
  short          spc_rows;              /* Spacing for rows                */ 
  char          *pattern;               /* Buffer to store match chars     */
  short          pindex;                /* Index into pattern buffer       */
  WINDOW        *win;                   /* Window containing menu          */
  WINDOW        *sub;                   /* Subwindow for menu display      */
  WINDOW        *userwin;               /* User's window                   */
  WINDOW        *usersub;               /* User's subwindow                */
  ITEM          **items;                /* array of items                  */ 
  short          nitems;                /* Nr. of items in menu            */
  ITEM          *curitem;               /* Current item                    */
  short          toprow;                /* Top row of menu                 */
  chtype         fore;                  /* Selection attribute             */
  chtype         back;                  /* Nonselection attribute          */
  chtype         grey;                  /* Inactive attribute              */
  unsigned char  pad;                   /* Pad character                   */

  Menu_Hook      menuinit;              /* User hooks                      */
  Menu_Hook      menuterm;
  Menu_Hook      iteminit;
  Menu_Hook      itemterm;

  void          *userptr;               /* Pointer to menus user data      */
  char          *mark;                  /* Pointer to marker string        */

  Menu_Options   opt;                   /* Menu options                    */
  unsigned short status;                /* Internal state of menu          */

} MENU;


/* Define keys */

#define REQ_LEFT_ITEM           (KEY_MAX + 1)
#define REQ_RIGHT_ITEM          (KEY_MAX + 2)
#define REQ_UP_ITEM             (KEY_MAX + 3)
#define REQ_DOWN_ITEM           (KEY_MAX + 4)
#define REQ_SCR_ULINE           (KEY_MAX + 5)
#define REQ_SCR_DLINE           (KEY_MAX + 6)
#define REQ_SCR_DPAGE           (KEY_MAX + 7)
#define REQ_SCR_UPAGE           (KEY_MAX + 8)
#define REQ_FIRST_ITEM          (KEY_MAX + 9)
#define REQ_LAST_ITEM           (KEY_MAX + 10)
#define REQ_NEXT_ITEM           (KEY_MAX + 11)
#define REQ_PREV_ITEM           (KEY_MAX + 12)
#define REQ_TOGGLE_ITEM         (KEY_MAX + 13)
#define REQ_CLEAR_PATTERN       (KEY_MAX + 14)
#define REQ_BACK_PATTERN        (KEY_MAX + 15)
#define REQ_NEXT_MATCH          (KEY_MAX + 16)
#define REQ_PREV_MATCH          (KEY_MAX + 17)

#define MIN_MENU_COMMAND        (KEY_MAX + 1)
#define MAX_MENU_COMMAND        (KEY_MAX + 17)

/*
 * Some AT&T code expects MAX_COMMAND to be out-of-band not
 * just for menu commands but for forms ones as well.
 */
#if defined(MAX_COMMAND)
#  if (MAX_MENU_COMMAND > MAX_COMMAND)
#    error Something is wrong -- MAX_MENU_COMMAND is greater than MAX_COMMAND
#  elif (MAX_COMMAND != (KEY_MAX + 128))
#    error Something is wrong -- MAX_COMMAND is already inconsistently defined.
#  endif
#else
#  define MAX_COMMAND (KEY_MAX + 128)
#endif


/* --------- prototypes for libmenu functions ----------------------------- */

extern ITEM     **menu_items(const MENU *),
                *current_item(const MENU *),
                *new_item(const char *,const char *);

extern MENU     *new_menu(ITEM **);

extern Item_Options  item_opts(const ITEM *);
extern Menu_Options  menu_opts(const MENU *);

Menu_Hook       item_init(const MENU *),
                item_term(const MENU *),
                menu_init(const MENU *),
                menu_term(const MENU *);

extern WINDOW   *menu_sub(const MENU *),
                *menu_win(const MENU *);

extern const char *item_description(const ITEM *),
                  *item_name(const ITEM *),
                  *menu_mark(const MENU *),
                  *menu_request_name(int);

extern char     *menu_pattern(const MENU *);

extern void     *menu_userptr(const MENU *),
                *item_userptr(const ITEM *);

extern chtype   menu_back(const MENU *),
                menu_fore(const MENU *),
                menu_grey(const MENU *);

extern int      free_item(ITEM *),
                free_menu(MENU *),
                item_count(const MENU *),
                item_index(const ITEM *),
                item_opts_off(ITEM *,Item_Options),
                item_opts_on(ITEM *,Item_Options),
                menu_driver(MENU *,int),
                menu_opts_off(MENU *,Menu_Options),
                menu_opts_on(MENU *,Menu_Options),
                menu_pad(const MENU *),
                pos_menu_cursor(const MENU *),
                post_menu(MENU *),
                scale_menu(const MENU *,int *,int *),
                set_current_item(MENU *menu,ITEM *item),
                set_item_init(MENU *,void(*)(MENU *)),
                set_item_opts(ITEM *,Item_Options),
                set_item_term(MENU *,void(*)(MENU *)),
                set_item_userptr(ITEM *, void *),
                set_item_value(ITEM *,bool),
                set_menu_back(MENU *,chtype),
                set_menu_fore(MENU *,chtype),
                set_menu_format(MENU *,int,int),
                set_menu_grey(MENU *,chtype),
                set_menu_init(MENU *,void(*)(MENU *)),
                set_menu_items(MENU *,ITEM **),
                set_menu_mark(MENU *, const char *),
                set_menu_opts(MENU *,Menu_Options),
                set_menu_pad(MENU *,int),
                set_menu_pattern(MENU *,const char *),
                set_menu_sub(MENU *,WINDOW *),
                set_menu_term(MENU *,void(*)(MENU *)),
                set_menu_userptr(MENU *,void *),
                set_menu_win(MENU *,WINDOW *),
                set_top_row(MENU *,int),
                top_row(const MENU *),
                unpost_menu(MENU *),
                menu_request_by_name(const char *),
                set_menu_spacing(MENU *,int,int,int),
                menu_spacing(const MENU *,int *,int *,int *);


extern bool     item_value(const ITEM *),
                item_visible(const ITEM *);

void            menu_format(const MENU *,int *,int *);

#ifdef __cplusplus
  }
#endif

#endif /* ETI_MENU */
