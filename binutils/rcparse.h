/* A Bison parser, made by GNU Bison 2.1.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     BEG = 258,
     END = 259,
     ACCELERATORS = 260,
     VIRTKEY = 261,
     ASCII = 262,
     NOINVERT = 263,
     SHIFT = 264,
     CONTROL = 265,
     ALT = 266,
     BITMAP = 267,
     CURSOR = 268,
     DIALOG = 269,
     DIALOGEX = 270,
     EXSTYLE = 271,
     CAPTION = 272,
     CLASS = 273,
     STYLE = 274,
     AUTO3STATE = 275,
     AUTOCHECKBOX = 276,
     AUTORADIOBUTTON = 277,
     CHECKBOX = 278,
     COMBOBOX = 279,
     CTEXT = 280,
     DEFPUSHBUTTON = 281,
     EDITTEXT = 282,
     GROUPBOX = 283,
     LISTBOX = 284,
     LTEXT = 285,
     PUSHBOX = 286,
     PUSHBUTTON = 287,
     RADIOBUTTON = 288,
     RTEXT = 289,
     SCROLLBAR = 290,
     STATE3 = 291,
     USERBUTTON = 292,
     BEDIT = 293,
     HEDIT = 294,
     IEDIT = 295,
     FONT = 296,
     ICON = 297,
     LANGUAGE = 298,
     CHARACTERISTICS = 299,
     VERSIONK = 300,
     MENU = 301,
     MENUEX = 302,
     MENUITEM = 303,
     SEPARATOR = 304,
     POPUP = 305,
     CHECKED = 306,
     GRAYED = 307,
     HELP = 308,
     INACTIVE = 309,
     MENUBARBREAK = 310,
     MENUBREAK = 311,
     MESSAGETABLE = 312,
     RCDATA = 313,
     STRINGTABLE = 314,
     VERSIONINFO = 315,
     FILEVERSION = 316,
     PRODUCTVERSION = 317,
     FILEFLAGSMASK = 318,
     FILEFLAGS = 319,
     FILEOS = 320,
     FILETYPE = 321,
     FILESUBTYPE = 322,
     BLOCKSTRINGFILEINFO = 323,
     BLOCKVARFILEINFO = 324,
     VALUE = 325,
     BLOCK = 326,
     MOVEABLE = 327,
     FIXED = 328,
     PURE = 329,
     IMPURE = 330,
     PRELOAD = 331,
     LOADONCALL = 332,
     DISCARDABLE = 333,
     NOT = 334,
     QUOTEDSTRING = 335,
     STRING = 336,
     NUMBER = 337,
     SIZEDSTRING = 338,
     IGNORED_TOKEN = 339,
     NEG = 340
   };
#endif
/* Tokens.  */
#define BEG 258
#define END 259
#define ACCELERATORS 260
#define VIRTKEY 261
#define ASCII 262
#define NOINVERT 263
#define SHIFT 264
#define CONTROL 265
#define ALT 266
#define BITMAP 267
#define CURSOR 268
#define DIALOG 269
#define DIALOGEX 270
#define EXSTYLE 271
#define CAPTION 272
#define CLASS 273
#define STYLE 274
#define AUTO3STATE 275
#define AUTOCHECKBOX 276
#define AUTORADIOBUTTON 277
#define CHECKBOX 278
#define COMBOBOX 279
#define CTEXT 280
#define DEFPUSHBUTTON 281
#define EDITTEXT 282
#define GROUPBOX 283
#define LISTBOX 284
#define LTEXT 285
#define PUSHBOX 286
#define PUSHBUTTON 287
#define RADIOBUTTON 288
#define RTEXT 289
#define SCROLLBAR 290
#define STATE3 291
#define USERBUTTON 292
#define BEDIT 293
#define HEDIT 294
#define IEDIT 295
#define FONT 296
#define ICON 297
#define LANGUAGE 298
#define CHARACTERISTICS 299
#define VERSIONK 300
#define MENU 301
#define MENUEX 302
#define MENUITEM 303
#define SEPARATOR 304
#define POPUP 305
#define CHECKED 306
#define GRAYED 307
#define HELP 308
#define INACTIVE 309
#define MENUBARBREAK 310
#define MENUBREAK 311
#define MESSAGETABLE 312
#define RCDATA 313
#define STRINGTABLE 314
#define VERSIONINFO 315
#define FILEVERSION 316
#define PRODUCTVERSION 317
#define FILEFLAGSMASK 318
#define FILEFLAGS 319
#define FILEOS 320
#define FILETYPE 321
#define FILESUBTYPE 322
#define BLOCKSTRINGFILEINFO 323
#define BLOCKVARFILEINFO 324
#define VALUE 325
#define BLOCK 326
#define MOVEABLE 327
#define FIXED 328
#define PURE 329
#define IMPURE 330
#define PRELOAD 331
#define LOADONCALL 332
#define DISCARDABLE 333
#define NOT 334
#define QUOTEDSTRING 335
#define STRING 336
#define NUMBER 337
#define SIZEDSTRING 338
#define IGNORED_TOKEN 339
#define NEG 340




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 66 "rcparse.y"
typedef union YYSTYPE {
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
} YYSTYPE;
/* Line 1447 of yacc.c.  */
#line 247 "rcparse.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



