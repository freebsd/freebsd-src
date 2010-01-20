/*
 *  rc.h -- declarations for configuration file processing
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


#define DIALOGRC ".dialogrc"
#define VAR_LEN 30
#define COMMENT_LEN 70

/* Types of values */
#define VAL_INT  0
#define VAL_STR  1
#define VAL_BOOL 2
#define VAL_ATTR 3

/* Type of line in configuration file */
#define LINE_BLANK    2
#define LINE_COMMENT  1
#define LINE_OK       0
#define LINE_ERROR   -1

/* number of configuration variables */
#define VAR_COUNT        (sizeof(vars) / sizeof(vars_st))

/* check if character is white space */
#define whitespace(c)    (c == ' ' || c == '\t')

/* check if character is string quoting characters */
#define isquote(c)       (c == '"' || c == '\'')

/* get last character of string */
#define lastch(str)      str[strlen(str)-1]

/*
 * Configuration variables
 */
typedef struct {
  unsigned char      name[VAR_LEN];  /* name of configuration variable as in DIALOGRC */
  void     *var;            /* address of actually variable to change */
  int       type;           /* type of value */
  unsigned char      comment[COMMENT_LEN];    /* comment to put in "rc" file */
} vars_st;

vars_st vars[] = {
  {  "use_shadow",
     &use_shadow,
     VAL_BOOL,
     "Shadow dialog boxes? This also turns on color."                        },

  {  "use_colors",
     &use_colors,
     VAL_BOOL,
     "Turn color support ON or OFF"                                          },

  {  "screen_color",
     color_table[0],
     VAL_ATTR,
     "Screen color"                                                          },

  {  "shadow_color",
     color_table[1],
     VAL_ATTR,
     "Shadow color"                                                          },

  {  "dialog_color",
     color_table[2],
     VAL_ATTR,
     "Dialog box color"                                                      },

  {  "title_color",
     color_table[3],
     VAL_ATTR,
     "Dialog box title color"                                                },

  {  "border_color",
     color_table[4],
     VAL_ATTR,
     "Dialog box border color"                                               },

  {  "button_active_color",
     color_table[5],
     VAL_ATTR,
     "Active button color"                                                   },

  {  "button_inactive_color",
     color_table[6],
     VAL_ATTR,
     "Inactive button color"                                                 },

  {  "button_key_active_color",
     color_table[7],
     VAL_ATTR,
     "Active button key color"                                               },

  {  "button_key_inactive_color",
     color_table[8],
     VAL_ATTR,
     "Inactive button key color"                                             },

  {  "button_label_active_color",
     color_table[9],
     VAL_ATTR,
     "Active button label color"                                             },

  {  "button_label_inactive_color",
     color_table[10],
     VAL_ATTR,
     "Inactive button label color"                                           },

  {  "inputbox_color",
     color_table[11],
     VAL_ATTR,
     "Input box color"                                                       },

  {  "inputbox_border_color",
     color_table[12],
     VAL_ATTR,
     "Input box border color"                                                },

  {  "searchbox_color",
     color_table[13],
     VAL_ATTR,
     "Search box color"                                                      },

  {  "searchbox_title_color",
     color_table[14],
     VAL_ATTR,
     "Search box title color"                                                },

  {  "searchbox_border_color",
     color_table[15],
     VAL_ATTR,
     "Search box border color"                                               },

  {  "position_indicator_color",
     color_table[16],
     VAL_ATTR,
     "File position indicator color"                                         },

  {  "menubox_color",
     color_table[17],
     VAL_ATTR,
     "Menu box color"                                                        },

  {  "menubox_border_color",
     color_table[18],
     VAL_ATTR,
     "Menu box border color"                                                 },

  {  "item_color",
     color_table[19],
     VAL_ATTR,
     "Item color"                                                            },

  {  "item_selected_color",
     color_table[20],
     VAL_ATTR,
     "Selected item color"                                                   },

  {  "tag_color",
     color_table[21],
     VAL_ATTR,
     "Tag color"                                                             },

  {  "tag_selected_color",
     color_table[22],
     VAL_ATTR,
     "Selected tag color"                                                    },

  {  "tag_key_color",
     color_table[23],
     VAL_ATTR,
     "Tag key color"                                                         },

  {  "tag_key_selected_color",
     color_table[24],
     VAL_ATTR,
     "Selected tag key color"                                                },

  {  "check_color",
     color_table[25],
     VAL_ATTR,
     "Check box color"                                                       },

  {  "check_selected_color",
     color_table[26],
     VAL_ATTR,
     "Selected check box color"                                              },

  {  "uarrow_color",
     color_table[27],
     VAL_ATTR,
     "Up arrow color"                                                        },

  {  "darrow_color",
     color_table[28],
     VAL_ATTR,
     "Down arrow color"                                                      }
};    /* vars */



/*
 * Routines to process configuration file
 */
int parse_rc(void);
