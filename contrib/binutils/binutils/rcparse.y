%{ /* rcparse.y -- parser for Windows rc files
   Copyright 1997, 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

   This file is part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* This is a parser for Windows rc files.  It is based on the parser
   by Gunther Ebert <gunther.ebert@ixos-leipzig.de>.  */

#include "bfd.h"
#include "bucomm.h"
#include "libiberty.h"
#include "windres.h"
#include "safe-ctype.h"

/* The current language.  */

static unsigned short language;

/* The resource information during a sub statement.  */

static struct res_res_info sub_res_info;

/* Dialog information.  This is built by the nonterminals styles and
   controls.  */

static struct dialog dialog;

/* This is used when building a style.  It is modified by the
   nonterminal styleexpr.  */

static unsigned long style;

/* These are used when building a control.  They are set before using
   control_params.  */

static unsigned long base_style;
static unsigned long default_style;
static unsigned long class;

%}

%union
{
  struct accelerator acc;
  struct accelerator *pacc;
  struct dialog_control *dialog_control;
  struct menuitem *menuitem;
  struct
  {
    struct rcdata_item *first;
    struct rcdata_item *last;
  } rcdata;
  struct rcdata_item *rcdata_item;
  struct stringtable_data *stringtable;
  struct fixed_versioninfo *fixver;
  struct ver_info *verinfo;
  struct ver_stringinfo *verstring;
  struct ver_varinfo *vervar;
  struct res_id id;
  struct res_res_info res_info;
  struct
  {
    unsigned short on;
    unsigned short off;
  } memflags;
  struct
  {
    unsigned long val;
    /* Nonzero if this number was explicitly specified as long.  */
    int dword;
  } i;
  unsigned long il;
  unsigned short is;
  const char *s;
  struct
  {
    unsigned long length;
    const char *s;
  } ss;
};

%token BEG END
%token ACCELERATORS VIRTKEY ASCII NOINVERT SHIFT CONTROL ALT
%token BITMAP
%token CURSOR
%token DIALOG DIALOGEX EXSTYLE CAPTION CLASS STYLE
%token AUTO3STATE AUTOCHECKBOX AUTORADIOBUTTON CHECKBOX COMBOBOX CTEXT
%token DEFPUSHBUTTON EDITTEXT GROUPBOX LISTBOX LTEXT PUSHBOX PUSHBUTTON
%token RADIOBUTTON RTEXT SCROLLBAR STATE3 USERBUTTON
%token BEDIT HEDIT IEDIT
%token FONT
%token ICON
%token LANGUAGE CHARACTERISTICS VERSIONK
%token MENU MENUEX MENUITEM SEPARATOR POPUP CHECKED GRAYED HELP INACTIVE
%token MENUBARBREAK MENUBREAK
%token MESSAGETABLE
%token RCDATA
%token STRINGTABLE
%token VERSIONINFO FILEVERSION PRODUCTVERSION FILEFLAGSMASK FILEFLAGS
%token FILEOS FILETYPE FILESUBTYPE BLOCKSTRINGFILEINFO BLOCKVARFILEINFO
%token VALUE
%token <s> BLOCK
%token MOVEABLE FIXED PURE IMPURE PRELOAD LOADONCALL DISCARDABLE
%token NOT
%token <s> QUOTEDSTRING STRING
%token <i> NUMBER
%token <ss> SIZEDSTRING
%token IGNORED_TOKEN

%type <pacc> acc_entries
%type <acc> acc_entry acc_event
%type <dialog_control> control control_params
%type <menuitem> menuitems menuitem menuexitems menuexitem
%type <rcdata> optrcdata_data optrcdata_data_int rcdata_data
%type <rcdata_item> opt_control_data
%type <fixver> fixedverinfo
%type <verinfo> verblocks
%type <verstring> vervals
%type <vervar> vertrans
%type <res_info> suboptions memflags_move_discard memflags_move
%type <memflags> memflag
%type <id> id resref
%type <il> exstyle parennumber
%type <il> numexpr posnumexpr cnumexpr optcnumexpr cposnumexpr
%type <is> acc_options acc_option menuitem_flags menuitem_flag
%type <s> optstringc file_name resname
%type <i> sizednumexpr sizedposnumexpr

%left '|'
%left '^'
%left '&'
%left '+' '-'
%left '*' '/' '%'
%right '~' NEG

%%

input:
	  /* empty */
	| input accelerator
	| input bitmap
	| input cursor
	| input dialog
	| input font
	| input icon
	| input language
	| input menu
	| input menuex
	| input messagetable
	| input rcdata
	| input stringtable
	| input user
	| input versioninfo
	| input IGNORED_TOKEN
	;

/* Accelerator resources.  */

accelerator:
	  id ACCELERATORS suboptions BEG acc_entries END
	  {
	    define_accelerator ($1, &$3, $5);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	;

acc_entries:
	  /* empty */
	  {
	    $$ = NULL;
	  }
	| acc_entries acc_entry
	  {
	    struct accelerator *a;

	    a = (struct accelerator *) res_alloc (sizeof *a);
	    *a = $2;
	    if ($1 == NULL)
	      $$ = a;
	    else
	      {
		struct accelerator **pp;

		for (pp = &$1->next; *pp != NULL; pp = &(*pp)->next)
		  ;
		*pp = a;
		$$ = $1;
	      }
	  }
	;

acc_entry:
	  acc_event cposnumexpr
	  {
	    $$ = $1;
	    $$.id = $2;
	  }
	| acc_event cposnumexpr ',' acc_options
	  {
	    $$ = $1;
	    $$.id = $2;
	    $$.flags |= $4;
	    if (($$.flags & ACC_VIRTKEY) == 0
		&& ($$.flags & (ACC_SHIFT | ACC_CONTROL | ACC_ALT)) != 0)
	      rcparse_warning (_("inappropriate modifiers for non-VIRTKEY"));
	  }
	;

acc_event:
	  QUOTEDSTRING
	  {
	    const char *s = $1;
	    char ch;

	    $$.next = NULL;
	    $$.id = 0;
	    ch = *s;
	    if (ch != '^')
	      $$.flags = 0;
	    else
	      {
		$$.flags = ACC_CONTROL | ACC_VIRTKEY;
		++s;
		ch = *s;
		ch = TOUPPER (ch);
	      }
	    $$.key = ch;
	    if (s[1] != '\0')
	      rcparse_warning (_("accelerator should only be one character"));
	  }
	| posnumexpr
	  {
	    $$.next = NULL;
	    $$.flags = 0;
	    $$.id = 0;
	    $$.key = $1;
	  }
	;

acc_options:
	  acc_option
	  {
	    $$ = $1;
	  }
	| acc_options ',' acc_option
	  {
	    $$ = $1 | $3;
	  }
	/* I've had one report that the comma is optional.  */
	| acc_options acc_option
	  {
	    $$ = $1 | $2;
	  }
	;

acc_option:
	  VIRTKEY
	  {
	    $$ = ACC_VIRTKEY;
	  }
	| ASCII
	  {
	    /* This is just the absence of VIRTKEY.  */
	    $$ = 0;
	  }
	| NOINVERT
	  {
	    $$ = ACC_NOINVERT;
	  }
	| SHIFT
	  {
	    $$ = ACC_SHIFT;
	  }
	| CONTROL
	  {
	    $$ = ACC_CONTROL;
	  }
	| ALT
	  {
	    $$ = ACC_ALT;
	  }
	;

/* Bitmap resources.  */

bitmap:
	  id BITMAP memflags_move file_name
	  {
	    define_bitmap ($1, &$3, $4);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	;

/* Cursor resources.  */

cursor:
	  id CURSOR memflags_move_discard file_name
	  {
	    define_cursor ($1, &$3, $4);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	;

/* Dialog resources.  */

dialog:
	  id DIALOG memflags_move exstyle posnumexpr cnumexpr cnumexpr
	    cnumexpr
	    {
	      memset (&dialog, 0, sizeof dialog);
	      dialog.x = $5;
	      dialog.y = $6;
	      dialog.width = $7;
	      dialog.height = $8;
	      dialog.style = WS_POPUP | WS_BORDER | WS_SYSMENU;
	      dialog.exstyle = $4;
	      dialog.menu.named = 1;
	      dialog.class.named = 1;
	      dialog.font = NULL;
	      dialog.ex = NULL;
	      dialog.controls = NULL;
	      sub_res_info = $3;
	      style = 0;
	    }
	    styles BEG controls END
	  {
	    define_dialog ($1, &sub_res_info, &dialog);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	| id DIALOGEX memflags_move exstyle posnumexpr cnumexpr cnumexpr
	    cnumexpr
	    {
	      memset (&dialog, 0, sizeof dialog);
	      dialog.x = $5;
	      dialog.y = $6;
	      dialog.width = $7;
	      dialog.height = $8;
	      dialog.style = WS_POPUP | WS_BORDER | WS_SYSMENU;
	      dialog.exstyle = $4;
	      dialog.menu.named = 1;
	      dialog.class.named = 1;
	      dialog.font = NULL;
	      dialog.ex = ((struct dialog_ex *)
			   res_alloc (sizeof (struct dialog_ex)));
	      memset (dialog.ex, 0, sizeof (struct dialog_ex));
	      dialog.controls = NULL;
	      sub_res_info = $3;
	      style = 0;
	    }
	    styles BEG controls END
	  {
	    define_dialog ($1, &sub_res_info, &dialog);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	| id DIALOGEX memflags_move exstyle posnumexpr cnumexpr cnumexpr
	    cnumexpr cnumexpr
	    {
	      memset (&dialog, 0, sizeof dialog);
	      dialog.x = $5;
	      dialog.y = $6;
	      dialog.width = $7;
	      dialog.height = $8;
	      dialog.style = WS_POPUP | WS_BORDER | WS_SYSMENU;
	      dialog.exstyle = $4;
	      dialog.menu.named = 1;
	      dialog.class.named = 1;
	      dialog.font = NULL;
	      dialog.ex = ((struct dialog_ex *)
			   res_alloc (sizeof (struct dialog_ex)));
	      memset (dialog.ex, 0, sizeof (struct dialog_ex));
	      dialog.ex->help = $9;
	      dialog.controls = NULL;
	      sub_res_info = $3;
	      style = 0;
	    }
	    styles BEG controls END
	  {
	    define_dialog ($1, &sub_res_info, &dialog);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	;

exstyle:
	  /* empty */
	  {
	    $$ = 0;
	  }
	| EXSTYLE '=' numexpr
	  {
	    $$ = $3;
	  }
	;

styles:
	  /* empty */
	| styles CAPTION QUOTEDSTRING
	  {
	    dialog.style |= WS_CAPTION;
	    style |= WS_CAPTION;
	    unicode_from_ascii ((int *) NULL, &dialog.caption, $3);
	  }
	| styles CLASS id
	  {
	    dialog.class = $3;
	  }
	| styles STYLE
	    styleexpr
	  {
	    dialog.style = style;
	  }
	| styles EXSTYLE numexpr
	  {
	    dialog.exstyle = $3;
	  }
	| styles CLASS QUOTEDSTRING
	  {
	    res_string_to_id (& dialog.class, $3);
	  }
	| styles FONT numexpr ',' QUOTEDSTRING
	  {
	    dialog.style |= DS_SETFONT;
	    style |= DS_SETFONT;
	    dialog.pointsize = $3;
	    unicode_from_ascii ((int *) NULL, &dialog.font, $5);
	    if (dialog.ex != NULL)
	      {
		dialog.ex->weight = 0;
		dialog.ex->italic = 0;
		dialog.ex->charset = 1;
	      }
	  }
	| styles FONT numexpr ',' QUOTEDSTRING cnumexpr
	  {
	    dialog.style |= DS_SETFONT;
	    style |= DS_SETFONT;
	    dialog.pointsize = $3;
	    unicode_from_ascii ((int *) NULL, &dialog.font, $5);
	    if (dialog.ex == NULL)
	      rcparse_warning (_("extended FONT requires DIALOGEX"));
	    else
	      {
		dialog.ex->weight = $6;
		dialog.ex->italic = 0;
		dialog.ex->charset = 1;
	      }
	  }
	| styles FONT numexpr ',' QUOTEDSTRING cnumexpr cnumexpr
	  {
	    dialog.style |= DS_SETFONT;
	    style |= DS_SETFONT;
	    dialog.pointsize = $3;
	    unicode_from_ascii ((int *) NULL, &dialog.font, $5);
	    if (dialog.ex == NULL)
	      rcparse_warning (_("extended FONT requires DIALOGEX"));
	    else
	      {
		dialog.ex->weight = $6;
		dialog.ex->italic = $7;
		dialog.ex->charset = 1;
	      }
	  }
	| styles FONT numexpr ',' QUOTEDSTRING cnumexpr cnumexpr cnumexpr
	  {
	    dialog.style |= DS_SETFONT;
	    style |= DS_SETFONT;
	    dialog.pointsize = $3;
	    unicode_from_ascii ((int *) NULL, &dialog.font, $5);
	    if (dialog.ex == NULL)
	      rcparse_warning (_("extended FONT requires DIALOGEX"));
	    else
	      {
		dialog.ex->weight = $6;
		dialog.ex->italic = $7;
		dialog.ex->charset = $8;
	      }
	  }
	| styles MENU id
	  {
	    dialog.menu = $3;
	  }
	| styles CHARACTERISTICS numexpr
	  {
	    sub_res_info.characteristics = $3;
	  }
	| styles LANGUAGE numexpr cnumexpr
	  {
	    sub_res_info.language = $3 | ($4 << SUBLANG_SHIFT);
	  }
	| styles VERSIONK numexpr
	  {
	    sub_res_info.version = $3;
	  }
	;

controls:
	  /* empty */
	| controls control
	  {
	    struct dialog_control **pp;

	    for (pp = &dialog.controls; *pp != NULL; pp = &(*pp)->next)
	      ;
	    *pp = $2;
	  }
	;

control:
	  AUTO3STATE
	    {
	      default_style = BS_AUTO3STATE | WS_TABSTOP;
	      base_style = BS_AUTO3STATE;
	      class = CTL_BUTTON;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| AUTOCHECKBOX
	    {
	      default_style = BS_AUTOCHECKBOX | WS_TABSTOP;
	      base_style = BS_AUTOCHECKBOX;
	      class = CTL_BUTTON;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| AUTORADIOBUTTON
	    {
	      default_style = BS_AUTORADIOBUTTON | WS_TABSTOP;
	      base_style = BS_AUTORADIOBUTTON;
	      class = CTL_BUTTON;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| BEDIT
	    {
	      default_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      base_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      class = CTL_EDIT;
	    }
	    control_params
	  {
	    $$ = $3;
	    if (dialog.ex == NULL)
	      rcparse_warning (_("BEDIT requires DIALOGEX"));
	    res_string_to_id (&$$->class, "BEDIT");
	  }
	| CHECKBOX
	    {
	      default_style = BS_CHECKBOX | WS_TABSTOP;
	      base_style = BS_CHECKBOX | WS_TABSTOP;
	      class = CTL_BUTTON;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| COMBOBOX
	    {
	      default_style = CBS_SIMPLE | WS_TABSTOP;
	      base_style = 0;
	      class = CTL_COMBOBOX;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| CONTROL optstringc numexpr cnumexpr control_styleexpr cnumexpr
	    cnumexpr cnumexpr cnumexpr optcnumexpr opt_control_data
	  {
	    $$ = define_control ($2, $3, $6, $7, $8, $9, $4, style, $10);
	    if ($11 != NULL)
	      {
		if (dialog.ex == NULL)
		  rcparse_warning (_("control data requires DIALOGEX"));
		$$->data = $11;
	      }
	  }
	| CONTROL optstringc numexpr cnumexpr control_styleexpr cnumexpr
	    cnumexpr cnumexpr cnumexpr cnumexpr cnumexpr opt_control_data
	  {
	    $$ = define_control ($2, $3, $6, $7, $8, $9, $4, style, $10);
	    if (dialog.ex == NULL)
	      rcparse_warning (_("help ID requires DIALOGEX"));
	    $$->help = $11;
	    $$->data = $12;
	  }
	| CONTROL optstringc numexpr ',' QUOTEDSTRING control_styleexpr
	    cnumexpr cnumexpr cnumexpr cnumexpr optcnumexpr opt_control_data
	  {
	    $$ = define_control ($2, $3, $7, $8, $9, $10, 0, style, $11);
	    if ($12 != NULL)
	      {
		if (dialog.ex == NULL)
		  rcparse_warning ("control data requires DIALOGEX");
		$$->data = $12;
	      }
	    $$->class.named = 1;
  	    unicode_from_ascii (&$$->class.u.n.length, &$$->class.u.n.name, $5);
	  }
	| CONTROL optstringc numexpr ',' QUOTEDSTRING control_styleexpr
	    cnumexpr cnumexpr cnumexpr cnumexpr cnumexpr cnumexpr opt_control_data
	  {
	    $$ = define_control ($2, $3, $7, $8, $9, $10, 0, style, $11);
	    if (dialog.ex == NULL)
	      rcparse_warning ("help ID requires DIALOGEX");
	    $$->help = $12;
	    $$->data = $13;
	    $$->class.named = 1;
  	    unicode_from_ascii (&$$->class.u.n.length, &$$->class.u.n.name, $5);
	  }
	| CTEXT
	    {
	      default_style = SS_CENTER | WS_GROUP;
	      base_style = SS_CENTER;
	      class = CTL_STATIC;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| DEFPUSHBUTTON
	    {
	      default_style = BS_DEFPUSHBUTTON | WS_TABSTOP;
	      base_style = BS_DEFPUSHBUTTON | WS_TABSTOP;
	      class = CTL_BUTTON;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| EDITTEXT
	    {
	      default_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      base_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      class = CTL_EDIT;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| GROUPBOX
	    {
	      default_style = BS_GROUPBOX;
	      base_style = BS_GROUPBOX;
	      class = CTL_BUTTON;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| HEDIT
	    {
	      default_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      base_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      class = CTL_EDIT;
	    }
	    control_params
	  {
	    $$ = $3;
	    if (dialog.ex == NULL)
	      rcparse_warning (_("IEDIT requires DIALOGEX"));
	    res_string_to_id (&$$->class, "HEDIT");
	  }
	| ICON resref numexpr cnumexpr cnumexpr opt_control_data
          {
	    $$ = define_icon_control ($2, $3, $4, $5, 0, 0, 0, $6,
				      dialog.ex);
          }
	| ICON resref numexpr cnumexpr cnumexpr cnumexpr cnumexpr
	    opt_control_data
          {
	    $$ = define_icon_control ($2, $3, $4, $5, 0, 0, 0, $8,
				      dialog.ex);
          }
	| ICON resref numexpr cnumexpr cnumexpr cnumexpr cnumexpr
	    icon_styleexpr optcnumexpr opt_control_data
          {
	    $$ = define_icon_control ($2, $3, $4, $5, style, $9, 0, $10,
				      dialog.ex);
          }
	| ICON resref numexpr cnumexpr cnumexpr cnumexpr cnumexpr
	    icon_styleexpr cnumexpr cnumexpr opt_control_data
          {
	    $$ = define_icon_control ($2, $3, $4, $5, style, $9, $10, $11,
				      dialog.ex);
          }
	| IEDIT
	    {
	      default_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      base_style = ES_LEFT | WS_BORDER | WS_TABSTOP;
	      class = CTL_EDIT;
	    }
	    control_params
	  {
	    $$ = $3;
	    if (dialog.ex == NULL)
	      rcparse_warning (_("IEDIT requires DIALOGEX"));
	    res_string_to_id (&$$->class, "IEDIT");
	  }
	| LISTBOX
	    {
	      default_style = LBS_NOTIFY | WS_BORDER;
	      base_style = LBS_NOTIFY | WS_BORDER;
	      class = CTL_LISTBOX;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| LTEXT
	    {
	      default_style = SS_LEFT | WS_GROUP;
	      base_style = SS_LEFT;
	      class = CTL_STATIC;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| PUSHBOX
	    {
	      default_style = BS_PUSHBOX | WS_TABSTOP;
	      base_style = BS_PUSHBOX;
	      class = CTL_BUTTON;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| PUSHBUTTON
	    {
	      default_style = BS_PUSHBUTTON | WS_TABSTOP;
	      base_style = BS_PUSHBUTTON | WS_TABSTOP;
	      class = CTL_BUTTON;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| RADIOBUTTON
	    {
	      default_style = BS_RADIOBUTTON | WS_TABSTOP;
	      base_style = BS_RADIOBUTTON;
	      class = CTL_BUTTON;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| RTEXT
	    {
	      default_style = SS_RIGHT | WS_GROUP;
	      base_style = SS_RIGHT;
	      class = CTL_STATIC;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| SCROLLBAR
	    {
	      default_style = SBS_HORZ;
	      base_style = 0;
	      class = CTL_SCROLLBAR;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| STATE3
	    {
	      default_style = BS_3STATE | WS_TABSTOP;
	      base_style = BS_3STATE;
	      class = CTL_BUTTON;
	    }
	    control_params
	  {
	    $$ = $3;
	  }
	| USERBUTTON QUOTEDSTRING ',' numexpr ',' numexpr ',' numexpr ','
	    numexpr ',' numexpr ',' 
	    { style = WS_CHILD | WS_VISIBLE; }
	    styleexpr optcnumexpr
	  {
	    $$ = define_control ($2, $4, $6, $8, $10, $12, CTL_BUTTON,
				 style, $16);
	  }
	;

/* Parameters for a control.  The static variables DEFAULT_STYLE,
   BASE_STYLE, and CLASS must be initialized before this nonterminal
   is used.  DEFAULT_STYLE is the style to use if no style expression
   is specified.  BASE_STYLE is the base style to use if a style
   expression is specified; the style expression modifies the base
   style.  CLASS is the class of the control.  */

control_params:
	  optstringc numexpr cnumexpr cnumexpr cnumexpr cnumexpr
	    opt_control_data
	  {
	    $$ = define_control ($1, $2, $3, $4, $5, $6, class,
				 default_style | WS_CHILD | WS_VISIBLE, 0);
	    if ($7 != NULL)
	      {
		if (dialog.ex == NULL)
		  rcparse_warning (_("control data requires DIALOGEX"));
		$$->data = $7;
	      }
	  }
	| optstringc numexpr cnumexpr cnumexpr cnumexpr cnumexpr
	    control_params_styleexpr optcnumexpr opt_control_data
	  {
	    $$ = define_control ($1, $2, $3, $4, $5, $6, class, style, $8);
	    if ($9 != NULL)
	      {
		if (dialog.ex == NULL)
		  rcparse_warning (_("control data requires DIALOGEX"));
		$$->data = $9;
	      }
	  }
	| optstringc numexpr cnumexpr cnumexpr cnumexpr cnumexpr
	    control_params_styleexpr cnumexpr cnumexpr opt_control_data
	  {
	    $$ = define_control ($1, $2, $3, $4, $5, $6, class, style, $8);
	    if (dialog.ex == NULL)
	      rcparse_warning (_("help ID requires DIALOGEX"));
	    $$->help = $9;
	    $$->data = $10;
	  }
	;

optstringc:
	  /* empty */
	  {
	    $$ = NULL;
	  }
	| QUOTEDSTRING
	  {
	    $$ = $1;
	  }
	| QUOTEDSTRING ','
	  {
	    $$ = $1;
	  }
	;

opt_control_data:
	  /* empty */
	  {
	    $$ = NULL;
	  }
	| BEG optrcdata_data END
	  {
	    $$ = $2.first;
	  }
	;

/* These only exist to parse a reduction out of a common case.  */

control_styleexpr:
	  ','
	  { style = WS_CHILD | WS_VISIBLE; }
	  styleexpr
	;

icon_styleexpr:
	  ','
	  { style = SS_ICON | WS_CHILD | WS_VISIBLE; }
	  styleexpr
	;

control_params_styleexpr:
	  ','
	  { style = base_style | WS_CHILD | WS_VISIBLE; }
	  styleexpr
	;

/* Font resources.  */

font:
	  id FONT memflags_move_discard file_name
	  {
	    define_font ($1, &$3, $4);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	;

/* Icon resources.  */

icon:
	  id ICON memflags_move_discard file_name
	  {
	    define_icon ($1, &$3, $4);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	;

/* Language command.  This changes the static variable language, which
   affects all subsequent resources.  */

language:
	  LANGUAGE numexpr cnumexpr
	  {
	    language = $2 | ($3 << SUBLANG_SHIFT);
	  }
	;

/* Menu resources.  */

menu:
	  id MENU suboptions BEG menuitems END
	  {
	    define_menu ($1, &$3, $5);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	;

menuitems:
	  /* empty */
	  {
	    $$ = NULL;
	  }
	| menuitems menuitem
	  {
	    if ($1 == NULL)
	      $$ = $2;
	    else
	      {
		struct menuitem **pp;

		for (pp = &$1->next; *pp != NULL; pp = &(*pp)->next)
		  ;
		*pp = $2;
		$$ = $1;
	      }
	  }
	;

menuitem:
	  MENUITEM QUOTEDSTRING cnumexpr menuitem_flags
	  {
	    $$ = define_menuitem ($2, $3, $4, 0, 0, NULL);
	  }
	| MENUITEM SEPARATOR
	  {
	    $$ = define_menuitem (NULL, 0, 0, 0, 0, NULL);
	  }
	| POPUP QUOTEDSTRING menuitem_flags BEG menuitems END
	  {
	    $$ = define_menuitem ($2, 0, $3, 0, 0, $5);
	  }
	;

menuitem_flags:
	  /* empty */
	  {
	    $$ = 0;
	  }
	| menuitem_flags ',' menuitem_flag
	  {
	    $$ = $1 | $3;
	  }
	| menuitem_flags menuitem_flag
	  {
	    $$ = $1 | $2;
	  }
	;

menuitem_flag:
	  CHECKED
	  {
	    $$ = MENUITEM_CHECKED;
	  }
	| GRAYED
	  {
	    $$ = MENUITEM_GRAYED;
	  }
	| HELP
	  {
	    $$ = MENUITEM_HELP;
	  }
	| INACTIVE
	  {
	    $$ = MENUITEM_INACTIVE;
	  }
	| MENUBARBREAK
	  {
	    $$ = MENUITEM_MENUBARBREAK;
	  }
	| MENUBREAK
	  {
	    $$ = MENUITEM_MENUBREAK;
	  }
	;

/* Menuex resources.  */

menuex:
	  id MENUEX suboptions BEG menuexitems END
	  {
	    define_menu ($1, &$3, $5);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	;

menuexitems:
	  /* empty */
	  {
	    $$ = NULL;
	  }
	| menuexitems menuexitem
	  {
	    if ($1 == NULL)
	      $$ = $2;
	    else
	      {
		struct menuitem **pp;

		for (pp = &$1->next; *pp != NULL; pp = &(*pp)->next)
		  ;
		*pp = $2;
		$$ = $1;
	      }
	  }
	;

menuexitem:
	  MENUITEM QUOTEDSTRING
	  {
	    $$ = define_menuitem ($2, 0, 0, 0, 0, NULL);
	  }
	| MENUITEM QUOTEDSTRING cnumexpr
	  {
	    $$ = define_menuitem ($2, $3, 0, 0, 0, NULL);
	  }
	| MENUITEM QUOTEDSTRING cnumexpr cnumexpr optcnumexpr
	  {
	    $$ = define_menuitem ($2, $3, $4, $5, 0, NULL);
	  }
 	| MENUITEM SEPARATOR
 	  {
 	    $$ = define_menuitem (NULL, 0, 0, 0, 0, NULL);
 	  }
	| POPUP QUOTEDSTRING BEG menuexitems END
	  {
	    $$ = define_menuitem ($2, 0, 0, 0, 0, $4);
	  }
	| POPUP QUOTEDSTRING cnumexpr BEG menuexitems END
	  {
	    $$ = define_menuitem ($2, $3, 0, 0, 0, $5);
	  }
	| POPUP QUOTEDSTRING cnumexpr cnumexpr BEG menuexitems END
	  {
	    $$ = define_menuitem ($2, $3, $4, 0, 0, $6);
	  }
	| POPUP QUOTEDSTRING cnumexpr cnumexpr cnumexpr optcnumexpr
	    BEG menuexitems END
	  {
	    $$ = define_menuitem ($2, $3, $4, $5, $6, $8);
	  }
	;

/* Messagetable resources.  */

messagetable:
	  id MESSAGETABLE memflags_move file_name
	  {
	    define_messagetable ($1, &$3, $4);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	;

/* Rcdata resources.  */

rcdata:
	  id RCDATA suboptions BEG optrcdata_data END
	  {
	    define_rcdata ($1, &$3, $5.first);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	;

/* We use a different lexing algorithm, because rcdata strings may
   contain embedded null bytes, and we need to know the length to use.  */

optrcdata_data:
	  {
	    rcparse_rcdata ();
	  }
	  optrcdata_data_int
	  {
	    rcparse_normal ();
	    $$ = $2;
	  }
	;

optrcdata_data_int:
	  /* empty */
	  {
	    $$.first = NULL;
	    $$.last = NULL;
	  }
	| rcdata_data
	  {
	    $$ = $1;
	  }
	;

rcdata_data:
	  SIZEDSTRING
	  {
	    struct rcdata_item *ri;

	    ri = define_rcdata_string ($1.s, $1.length);
	    $$.first = ri;
	    $$.last = ri;
	  }
	| sizednumexpr
	  {
	    struct rcdata_item *ri;

	    ri = define_rcdata_number ($1.val, $1.dword);
	    $$.first = ri;
	    $$.last = ri;
	  }
	| rcdata_data ',' SIZEDSTRING
	  {
	    struct rcdata_item *ri;

	    ri = define_rcdata_string ($3.s, $3.length);
	    $$.first = $1.first;
	    $1.last->next = ri;
	    $$.last = ri;
	  }
	| rcdata_data ',' sizednumexpr
	  {
	    struct rcdata_item *ri;

	    ri = define_rcdata_number ($3.val, $3.dword);
	    $$.first = $1.first;
	    $1.last->next = ri;
	    $$.last = ri;
	  }
	;

/* Stringtable resources.  */

stringtable:
	  STRINGTABLE suboptions BEG 
	    { sub_res_info = $2; }
	    string_data END
	;

string_data:
	  /* empty */
	| string_data numexpr QUOTEDSTRING
	  {
	    define_stringtable (&sub_res_info, $2, $3);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	| string_data numexpr ',' QUOTEDSTRING
	  {
	    define_stringtable (&sub_res_info, $2, $4);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	;

/* User defined resources.  We accept general suboptions in the
   file_name case to keep the parser happy.  */

user:
	  id id suboptions BEG optrcdata_data END
	  {
	    define_user_data ($1, $2, &$3, $5.first);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	| id id suboptions file_name
	  {
	    define_user_file ($1, $2, &$3, $4);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	;

/* Versioninfo resources.  */

versioninfo:
	  id VERSIONINFO fixedverinfo BEG verblocks END
	  {
	    define_versioninfo ($1, language, $3, $5);
	    if (yychar != YYEMPTY)
	      YYERROR;
	    rcparse_discard_strings ();
	  }
	;

fixedverinfo:
	  /* empty */
	  {
	    $$ = ((struct fixed_versioninfo *)
		  res_alloc (sizeof (struct fixed_versioninfo)));
	    memset ($$, 0, sizeof (struct fixed_versioninfo));
	  }
	| fixedverinfo FILEVERSION numexpr cnumexpr cnumexpr cnumexpr
	  {
	    $1->file_version_ms = ($3 << 16) | $4;
	    $1->file_version_ls = ($5 << 16) | $6;
	    $$ = $1;
	  }
	| fixedverinfo PRODUCTVERSION numexpr cnumexpr cnumexpr cnumexpr
	  {
	    $1->product_version_ms = ($3 << 16) | $4;
	    $1->product_version_ls = ($5 << 16) | $6;
	    $$ = $1;
	  }
	| fixedverinfo FILEFLAGSMASK numexpr
	  {
	    $1->file_flags_mask = $3;
	    $$ = $1;
	  }
	| fixedverinfo FILEFLAGS numexpr
	  {
	    $1->file_flags = $3;
	    $$ = $1;
	  }
	| fixedverinfo FILEOS numexpr
	  {
	    $1->file_os = $3;
	    $$ = $1;
	  }
	| fixedverinfo FILETYPE numexpr
	  {
	    $1->file_type = $3;
	    $$ = $1;
	  }
	| fixedverinfo FILESUBTYPE numexpr
	  {
	    $1->file_subtype = $3;
	    $$ = $1;
	  }
	;

/* To handle verblocks successfully, the lexer handles BLOCK
   specially.  A BLOCK "StringFileInfo" is returned as
   BLOCKSTRINGFILEINFO.  A BLOCK "VarFileInfo" is returned as
   BLOCKVARFILEINFO.  A BLOCK with some other string returns BLOCK
   with the string as the value.  */

verblocks:
	  /* empty */
	  {
	    $$ = NULL;
	  }
	| verblocks BLOCKSTRINGFILEINFO BEG BLOCK BEG vervals END END
	  {
	    $$ = append_ver_stringfileinfo ($1, $4, $6);
	  }
	| verblocks BLOCKVARFILEINFO BEG VALUE QUOTEDSTRING vertrans END
	  {
	    $$ = append_ver_varfileinfo ($1, $5, $6);
	  }
	;

vervals:
	  /* empty */
	  {
	    $$ = NULL;
	  }
	| vervals VALUE QUOTEDSTRING ',' QUOTEDSTRING
	  {
	    $$ = append_verval ($1, $3, $5);
	  }
	;

vertrans:
	  /* empty */
	  {
	    $$ = NULL;
	  }
	| vertrans cnumexpr cnumexpr
	  {
	    $$ = append_vertrans ($1, $2, $3);
	  }
	;

/* A resource ID.  */

id:
	  posnumexpr
	  {
	    $$.named = 0;
	    $$.u.id = $1;
	  }
	| STRING
	  {
	    char *copy, *s;

	    /* It seems that resource ID's are forced to upper case.  */
	    copy = xstrdup ($1);
	    for (s = copy; *s != '\0'; s++)
	      *s = TOUPPER (*s);
	    res_string_to_id (&$$, copy);
	    free (copy);
	  }
	;

/* A resource reference.  */

resname:
	  QUOTEDSTRING
	  {
	    $$ = $1;
	  }
	| QUOTEDSTRING ','
	  {
	    $$ = $1;
	  }
	| STRING ','
	  {
	    $$ = $1;
	  }
	;


resref:
	  posnumexpr ','
	  {
	    $$.named = 0;
	    $$.u.id = $1;
	  }
	| resname
	  {
	    char *copy, *s;

	    /* It seems that resource ID's are forced to upper case.  */
	    copy = xstrdup ($1);
	    for (s = copy; *s != '\0'; s++)
	      *s = TOUPPER (*s);
	    res_string_to_id (&$$, copy);
	    free (copy);
	  }
	;

/* Generic suboptions.  These may appear before the BEGIN in any
   multiline statement.  */

suboptions:
	  /* empty */
	  {
	    memset (&$$, 0, sizeof (struct res_res_info));
	    $$.language = language;
	    /* FIXME: Is this the right default?  */
	    $$.memflags = MEMFLAG_MOVEABLE | MEMFLAG_PURE | MEMFLAG_DISCARDABLE;
	  }
	| suboptions memflag
	  {
	    $$ = $1;
	    $$.memflags |= $2.on;
	    $$.memflags &=~ $2.off;
	  }
	| suboptions CHARACTERISTICS numexpr
	  {
	    $$ = $1;
	    $$.characteristics = $3;
	  }
	| suboptions LANGUAGE numexpr cnumexpr
	  {
	    $$ = $1;
	    $$.language = $3 | ($4 << SUBLANG_SHIFT);
	  }
	| suboptions VERSIONK numexpr
	  {
	    $$ = $1;
	    $$.version = $3;
	  }
	;

/* Memory flags which default to MOVEABLE and DISCARDABLE.  */

memflags_move_discard:
	  /* empty */
	  {
	    memset (&$$, 0, sizeof (struct res_res_info));
	    $$.language = language;
	    $$.memflags = MEMFLAG_MOVEABLE | MEMFLAG_DISCARDABLE;
	  }
	| memflags_move_discard memflag
	  {
	    $$ = $1;
	    $$.memflags |= $2.on;
	    $$.memflags &=~ $2.off;
	  }
	;

/* Memory flags which default to MOVEABLE.  */

memflags_move:
	  /* empty */
	  {
	    memset (&$$, 0, sizeof (struct res_res_info));
	    $$.language = language;
	    $$.memflags = MEMFLAG_MOVEABLE | MEMFLAG_PURE | MEMFLAG_DISCARDABLE;
	  }
	| memflags_move memflag
	  {
	    $$ = $1;
	    $$.memflags |= $2.on;
	    $$.memflags &=~ $2.off;
	  }
	;

/* Memory flags.  This returns a struct with two integers, because we
   sometimes want to set bits and we sometimes want to clear them.  */

memflag:
	  MOVEABLE
	  {
	    $$.on = MEMFLAG_MOVEABLE;
	    $$.off = 0;
	  }
	| FIXED
	  {
	    $$.on = 0;
	    $$.off = MEMFLAG_MOVEABLE;
	  }
	| PURE
	  {
	    $$.on = MEMFLAG_PURE;
	    $$.off = 0;
	  }
	| IMPURE
	  {
	    $$.on = 0;
	    $$.off = MEMFLAG_PURE;
	  }
	| PRELOAD
	  {
	    $$.on = MEMFLAG_PRELOAD;
	    $$.off = 0;
	  }
	| LOADONCALL
	  {
	    $$.on = 0;
	    $$.off = MEMFLAG_PRELOAD;
	  }
	| DISCARDABLE
	  {
	    $$.on = MEMFLAG_DISCARDABLE;
	    $$.off = 0;
	  }
	;

/* A file name.  */

file_name:
	  QUOTEDSTRING
	  {
	    $$ = $1;
	  }
	| STRING
	  {
	    $$ = $1;
	  }
	;

/* A style expression.  This changes the static variable STYLE.  We do
   it this way because rc appears to permit a style to be set to
   something like
       WS_GROUP | NOT WS_TABSTOP
   to mean that a default of WS_TABSTOP should be removed.  Anything
   which wants to accept a style must first set STYLE to the default
   value.  The styleexpr nonterminal will change STYLE as specified by
   the user.  Note that we do not accept arbitrary expressions here,
   just numbers separated by '|'.  */

styleexpr:
	  parennumber
	  {
	    style |= $1;
	  }
	| NOT parennumber
	  {
	    style &=~ $2;
	  }
	| styleexpr '|' parennumber
	  {
	    style |= $3;
	  }
	| styleexpr '|' NOT parennumber
	  {
	    style &=~ $4;
	  }
	;

parennumber:
	  NUMBER
	  {
	    $$ = $1.val;
	  }
	| '(' numexpr ')'
	  {
	    $$ = $2;
	  }
	;

/* An optional expression with a leading comma.  */

optcnumexpr:
	  /* empty */
	  {
	    $$ = 0;
	  }
	| cnumexpr
	  {
	    $$ = $1;
	  }
	;

/* An expression with a leading comma.  */

cnumexpr:
	  ',' numexpr
	  {
	    $$ = $2;
	  }
	;

/* A possibly negated numeric expression.  */

numexpr:
	  sizednumexpr
	  {
	    $$ = $1.val;
	  }
	;

/* A possibly negated expression with a size.  */

sizednumexpr:
	  NUMBER
	  {
	    $$ = $1;
	  }
	| '(' sizednumexpr ')'
	  {
	    $$ = $2;
	  }
	| '~' sizednumexpr %prec '~'
	  {
	    $$.val = ~ $2.val;
	    $$.dword = $2.dword;
	  }
	| '-' sizednumexpr %prec NEG
	  {
	    $$.val = - $2.val;
	    $$.dword = $2.dword;
	  }
	| sizednumexpr '*' sizednumexpr
	  {
	    $$.val = $1.val * $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizednumexpr '/' sizednumexpr
	  {
	    $$.val = $1.val / $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizednumexpr '%' sizednumexpr
	  {
	    $$.val = $1.val % $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizednumexpr '+' sizednumexpr
	  {
	    $$.val = $1.val + $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizednumexpr '-' sizednumexpr
	  {
	    $$.val = $1.val - $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizednumexpr '&' sizednumexpr
	  {
	    $$.val = $1.val & $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizednumexpr '^' sizednumexpr
	  {
	    $$.val = $1.val ^ $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizednumexpr '|' sizednumexpr
	  {
	    $$.val = $1.val | $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	;

/* An expression with a leading comma which does not use unary
   negation.  */

cposnumexpr:
	  ',' posnumexpr
	  {
	    $$ = $2;
	  }
	;

/* An expression which does not use unary negation.  */

posnumexpr:
	  sizedposnumexpr
	  {
	    $$ = $1.val;
	  }
	;

/* An expression which does not use unary negation.  We separate unary
   negation to avoid parsing conflicts when two numeric expressions
   appear consecutively.  */

sizedposnumexpr:
	  NUMBER
	  {
	    $$ = $1;
	  }
	| '(' sizednumexpr ')'
	  {
	    $$ = $2;
	  }
	| '~' sizednumexpr %prec '~'
	  {
	    $$.val = ~ $2.val;
	    $$.dword = $2.dword;
	  }
	| sizedposnumexpr '*' sizednumexpr
	  {
	    $$.val = $1.val * $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizedposnumexpr '/' sizednumexpr
	  {
	    $$.val = $1.val / $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizedposnumexpr '%' sizednumexpr
	  {
	    $$.val = $1.val % $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizedposnumexpr '+' sizednumexpr
	  {
	    $$.val = $1.val + $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizedposnumexpr '-' sizednumexpr
	  {
	    $$.val = $1.val - $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizedposnumexpr '&' sizednumexpr
	  {
	    $$.val = $1.val & $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizedposnumexpr '^' sizednumexpr
	  {
	    $$.val = $1.val ^ $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	| sizedposnumexpr '|' sizednumexpr
	  {
	    $$.val = $1.val | $3.val;
	    $$.dword = $1.dword || $3.dword;
	  }
	;

%%

/* Set the language from the command line.  */

void
rcparse_set_language (lang)
     int lang;
{
  language = lang;
}
