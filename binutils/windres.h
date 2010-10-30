/* windres.h -- header file for windres program.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2005, 2007
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.
   Rewritten by Kai Tietz, Onevision.

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include "ansidecl.h"

/* This is the header file for the windres program.  It defines
   structures and declares functions used within the program.  */

#include "winduni.h"
#include "windint.h"

extern int verbose;

/* Function declarations.  */

extern rc_res_directory *read_rc_file (const char *, const char *, const char *, int, int);
extern rc_res_directory *read_res_file (const char *);
extern rc_res_directory *read_coff_rsrc (const char *, const char *);
extern void write_rc_file (const char *, const rc_res_directory *);
extern void write_res_file (const char *, const rc_res_directory *);
extern void write_coff_file (const char *, const char *, const rc_res_directory *);

extern rc_res_resource *bin_to_res (windres_bfd *, rc_res_id, const bfd_byte *,
				    rc_uint_type);
extern rc_uint_type res_to_bin (windres_bfd *, rc_uint_type, const rc_res_resource *);

extern FILE *open_file_search (const char *, const char *, const char *, char **);

extern void *res_alloc (rc_uint_type);
extern void *reswr_alloc (rc_uint_type);

/* Resource ID handling.  */

extern int res_id_cmp (rc_res_id, rc_res_id);
extern void res_id_print (FILE *, rc_res_id, int);
extern void res_ids_print (FILE *, int, const rc_res_id *);
extern void res_string_to_id (rc_res_id *, const char *);
extern void res_unistring_to_id (rc_res_id *, const unichar *);

/* Manipulation of the resource tree.  */

extern rc_res_resource *define_resource (rc_res_directory **, int, const rc_res_id *,
					 int);
extern rc_res_resource *define_standard_resource (rc_res_directory **, int, rc_res_id,
						  rc_uint_type, int);

extern int extended_dialog (const rc_dialog *);
extern int extended_menu (const rc_menu *);

/* Communication between the rc file support and the parser and lexer.  */

extern int yydebug;
extern char *rc_filename;
extern int rc_lineno;

extern int yyparse (void);
extern int yylex (void);
extern void yyerror (const char *);
extern void rcparse_warning (const char *);
extern void rcparse_set_language (int);
extern void rcparse_discard_strings (void);
extern void rcparse_rcdata (void);
extern void rcparse_normal (void);

extern void define_accelerator (rc_res_id, const rc_res_res_info *, rc_accelerator *);
extern void define_bitmap (rc_res_id, const rc_res_res_info *, const char *);
extern void define_cursor (rc_res_id, const rc_res_res_info *, const char *);
extern void define_dialog (rc_res_id, const rc_res_res_info *, const rc_dialog *);
extern rc_dialog_control *define_control (const rc_res_id, rc_uint_type, rc_uint_type,
					  rc_uint_type, rc_uint_type, rc_uint_type,
					  const rc_res_id, rc_uint_type, rc_uint_type);
extern rc_dialog_control *define_icon_control (rc_res_id, rc_uint_type, rc_uint_type,
					       rc_uint_type, rc_uint_type, rc_uint_type,
					       rc_uint_type, rc_rcdata_item *,
					       rc_dialog_ex *);
extern void define_font (rc_res_id, const rc_res_res_info *, const char *);
extern void define_icon (rc_res_id, const rc_res_res_info *, const char *);
extern void define_menu (rc_res_id, const rc_res_res_info *, rc_menuitem *);
extern rc_menuitem *define_menuitem (const unichar *, rc_uint_type, rc_uint_type,
				     rc_uint_type, rc_uint_type, rc_menuitem *);
extern void define_messagetable (rc_res_id, const rc_res_res_info *, const char *);
extern void define_rcdata (rc_res_id, const rc_res_res_info *, rc_rcdata_item *);
extern void define_rcdata_file  (rc_res_id, const rc_res_res_info *, const char *);
extern rc_rcdata_item *define_rcdata_string (const char *, rc_uint_type);
extern rc_rcdata_item *define_rcdata_unistring (const unichar *, rc_uint_type);
extern rc_rcdata_item *define_rcdata_number (rc_uint_type, int);
extern void define_stringtable (const rc_res_res_info *, rc_uint_type, const unichar *);
extern void define_user_data (rc_res_id, rc_res_id, const rc_res_res_info *, rc_rcdata_item *);
extern void define_toolbar (rc_res_id, rc_res_res_info *, rc_uint_type ,rc_uint_type ,rc_toolbar_item *);
extern void define_user_file (rc_res_id, rc_res_id, const rc_res_res_info *, const char *);
extern void define_versioninfo (rc_res_id, rc_uint_type, rc_fixed_versioninfo *, rc_ver_info *);
extern rc_ver_info *append_ver_stringfileinfo (rc_ver_info *, const char *, rc_ver_stringinfo *);
extern rc_ver_info *append_ver_varfileinfo (rc_ver_info *, const unichar *, rc_ver_varinfo *);
extern rc_ver_stringinfo *append_verval (rc_ver_stringinfo *, const unichar *, const unichar *);
extern rc_ver_varinfo *append_vertrans (rc_ver_varinfo *, rc_uint_type, rc_uint_type);

extern bfd *windres_open_as_binary (const char *, int);

extern int wr_printcomment (FILE *, const char *, ...);
extern int wr_print (FILE *, const char *, ...);
#define wr_print_flush(FP)  wr_print ((FP),NULL)
