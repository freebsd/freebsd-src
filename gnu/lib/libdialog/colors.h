/*
 *  colors.h -- color attribute definitions
 *
 *  AUTHOR: Savio Lam (lam836@cs.cuhk.hk)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


/*
 *   Default color definitions
 *
 *   *_FG = foreground
 *   *_BG = background
 *   *_HL = highlight?
 */
#define SCREEN_FG                    COLOR_CYAN
#define SCREEN_BG                    COLOR_BLUE
#define SCREEN_HL                    TRUE

#define SHADOW_FG                    COLOR_BLACK
#define SHADOW_BG                    COLOR_BLACK
#define SHADOW_HL                    TRUE

#define DIALOG_FG                    COLOR_BLACK
#define DIALOG_BG                    COLOR_WHITE
#define DIALOG_HL                    FALSE

#define TITLE_FG                     COLOR_YELLOW
#define TITLE_BG                     COLOR_WHITE
#define TITLE_HL                     TRUE

#define BORDER_FG                    COLOR_WHITE
#define BORDER_BG                    COLOR_WHITE
#define BORDER_HL                    TRUE

#define BUTTON_ACTIVE_FG             COLOR_WHITE
#define BUTTON_ACTIVE_BG             COLOR_BLUE
#define BUTTON_ACTIVE_HL             TRUE

#define BUTTON_INACTIVE_FG           COLOR_BLACK
#define BUTTON_INACTIVE_BG           COLOR_WHITE
#define BUTTON_INACTIVE_HL           FALSE

#define BUTTON_KEY_ACTIVE_FG         COLOR_WHITE
#define BUTTON_KEY_ACTIVE_BG         COLOR_BLUE
#define BUTTON_KEY_ACTIVE_HL         TRUE

#define BUTTON_KEY_INACTIVE_FG       COLOR_RED
#define BUTTON_KEY_INACTIVE_BG       COLOR_WHITE
#define BUTTON_KEY_INACTIVE_HL       FALSE

#define BUTTON_LABEL_ACTIVE_FG       COLOR_YELLOW
#define BUTTON_LABEL_ACTIVE_BG       COLOR_BLUE
#define BUTTON_LABEL_ACTIVE_HL       TRUE

#define BUTTON_LABEL_INACTIVE_FG     COLOR_BLACK
#define BUTTON_LABEL_INACTIVE_BG     COLOR_WHITE
#define BUTTON_LABEL_INACTIVE_HL     TRUE

#define INPUTBOX_FG                  COLOR_BLACK
#define INPUTBOX_BG                  COLOR_WHITE
#define INPUTBOX_HL                  FALSE

#define INPUTBOX_BORDER_FG           COLOR_BLACK
#define INPUTBOX_BORDER_BG           COLOR_WHITE
#define INPUTBOX_BORDER_HL           FALSE

#define SEARCHBOX_FG                 COLOR_BLACK
#define SEARCHBOX_BG                 COLOR_WHITE
#define SEARCHBOX_HL                 FALSE

#define SEARCHBOX_TITLE_FG           COLOR_YELLOW
#define SEARCHBOX_TITLE_BG           COLOR_WHITE
#define SEARCHBOX_TITLE_HL           TRUE

#define SEARCHBOX_BORDER_FG          COLOR_WHITE
#define SEARCHBOX_BORDER_BG          COLOR_WHITE
#define SEARCHBOX_BORDER_HL          TRUE

#define POSITION_INDICATOR_FG        COLOR_YELLOW
#define POSITION_INDICATOR_BG        COLOR_WHITE
#define POSITION_INDICATOR_HL        TRUE

#define MENUBOX_FG                   COLOR_BLACK
#define MENUBOX_BG                   COLOR_WHITE
#define MENUBOX_HL                   FALSE

#define MENUBOX_BORDER_FG            COLOR_WHITE
#define MENUBOX_BORDER_BG            COLOR_WHITE
#define MENUBOX_BORDER_HL            TRUE

#define ITEM_FG                      COLOR_BLACK
#define ITEM_BG                      COLOR_WHITE
#define ITEM_HL                      FALSE

#define ITEM_SELECTED_FG             COLOR_WHITE
#define ITEM_SELECTED_BG             COLOR_BLUE
#define ITEM_SELECTED_HL             TRUE

#define TAG_FG                       COLOR_YELLOW
#define TAG_BG                       COLOR_WHITE
#define TAG_HL                       TRUE

#define TAG_SELECTED_FG              COLOR_YELLOW
#define TAG_SELECTED_BG              COLOR_BLUE
#define TAG_SELECTED_HL              TRUE

#define TAG_KEY_FG                   COLOR_RED
#define TAG_KEY_BG                   COLOR_WHITE
#define TAG_KEY_HL                   TRUE

#define TAG_KEY_SELECTED_FG          COLOR_RED
#define TAG_KEY_SELECTED_BG          COLOR_BLUE
#define TAG_KEY_SELECTED_HL          TRUE

#define CHECK_FG                     COLOR_BLACK
#define CHECK_BG                     COLOR_WHITE
#define CHECK_HL                     FALSE

#define CHECK_SELECTED_FG            COLOR_WHITE
#define CHECK_SELECTED_BG            COLOR_BLUE
#define CHECK_SELECTED_HL            TRUE

#define UARROW_FG                    COLOR_GREEN
#define UARROW_BG                    COLOR_WHITE
#define UARROW_HL                    TRUE

#define DARROW_FG                    COLOR_GREEN
#define DARROW_BG                    COLOR_WHITE
#define DARROW_HL                    TRUE

/* End of default color definitions */

#define C_ATTR(x,y)                  ((x ? A_BOLD : 0) | COLOR_PAIR((y)))
#define COLOR_NAME_LEN               10
#define COLOR_COUNT                  8


/*
 * Global variables
 */

typedef struct {
  unsigned char name[COLOR_NAME_LEN];
  int  value;
} color_names_st;


#ifdef __DIALOG_MAIN__

/*
 * For matching color names with color values
 */
color_names_st color_names[] = {
  {"BLACK",   COLOR_BLACK},
  {"RED",     COLOR_RED},
  {"GREEN",   COLOR_GREEN},
  {"YELLOW",  COLOR_YELLOW},
  {"BLUE",    COLOR_BLUE},
  {"MAGENTA", COLOR_MAGENTA},
  {"CYAN",    COLOR_CYAN},
  {"WHITE",   COLOR_WHITE},
};    /* color names */


/*
 * Table of color values
 */
int color_table[][3] = {
  {SCREEN_FG,               SCREEN_BG,               SCREEN_HL               },
  {SHADOW_FG,               SHADOW_BG,               SHADOW_HL               },
  {DIALOG_FG,               DIALOG_BG,               DIALOG_HL               },
  {TITLE_FG,                TITLE_BG,                TITLE_HL                },
  {BORDER_FG,               BORDER_BG,               BORDER_HL               },
  {BUTTON_ACTIVE_FG,        BUTTON_ACTIVE_BG,        BUTTON_ACTIVE_HL        },
  {BUTTON_INACTIVE_FG,      BUTTON_INACTIVE_BG,      BUTTON_INACTIVE_HL      },
  {BUTTON_KEY_ACTIVE_FG,    BUTTON_KEY_ACTIVE_BG,    BUTTON_KEY_ACTIVE_HL    },
  {BUTTON_KEY_INACTIVE_FG,  BUTTON_KEY_INACTIVE_BG,  BUTTON_KEY_INACTIVE_HL  },
  {BUTTON_LABEL_ACTIVE_FG,  BUTTON_LABEL_ACTIVE_BG,  BUTTON_LABEL_ACTIVE_HL  },
  {BUTTON_LABEL_INACTIVE_FG,BUTTON_LABEL_INACTIVE_BG,BUTTON_LABEL_INACTIVE_HL},
  {INPUTBOX_FG,             INPUTBOX_BG,             INPUTBOX_HL             },
  {INPUTBOX_BORDER_FG,      INPUTBOX_BORDER_BG,      INPUTBOX_BORDER_HL      },
  {SEARCHBOX_FG,            SEARCHBOX_BG,            SEARCHBOX_HL            },
  {SEARCHBOX_TITLE_FG,      SEARCHBOX_TITLE_BG,      SEARCHBOX_TITLE_HL      },
  {SEARCHBOX_BORDER_FG,     SEARCHBOX_BORDER_BG,     SEARCHBOX_BORDER_HL     },
  {POSITION_INDICATOR_FG,   POSITION_INDICATOR_BG,   POSITION_INDICATOR_HL   },
  {MENUBOX_FG,              MENUBOX_BG,              MENUBOX_HL              },
  {MENUBOX_BORDER_FG,       MENUBOX_BORDER_BG,       MENUBOX_BORDER_HL       },
  {ITEM_FG,                 ITEM_BG,                 ITEM_HL                 },
  {ITEM_SELECTED_FG,        ITEM_SELECTED_BG,        ITEM_SELECTED_HL        },
  {TAG_FG,                  TAG_BG,                  TAG_HL                  },
  {TAG_SELECTED_FG,         TAG_SELECTED_BG,         TAG_SELECTED_HL         },
  {TAG_KEY_FG,              TAG_KEY_BG,              TAG_KEY_HL              },
  {TAG_KEY_SELECTED_FG,     TAG_KEY_SELECTED_BG,     TAG_KEY_SELECTED_HL     },
  {CHECK_FG,                CHECK_BG,                CHECK_HL                },
  {CHECK_SELECTED_FG,       CHECK_SELECTED_BG,       CHECK_SELECTED_HL       },
  {UARROW_FG,               UARROW_BG,               UARROW_HL               },
  {DARROW_FG,               DARROW_BG,               DARROW_HL               },
};    /* color_table */

#else

extern color_names_st color_names[];
extern int color_table[][3];

#endif    /* __DIALOG_MAIN__ */
