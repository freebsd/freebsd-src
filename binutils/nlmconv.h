/* nlmconv.h -- header file for NLM conversion program
   Copyright 1993, 2002, 2003 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Ian Lance Taylor <ian@cygnus.com>.

   bfd.h, nlm/common.h and nlm/internal.h must be included before this
   file.  */

/* A linked list of strings.  */

struct string_list
{
  struct string_list *next;
  char *string;
};

/* The NLM header parser in nlmheader.y stores information in the
   following variables.  */

extern Nlm_Internal_Fixed_Header *fixed_hdr;
extern Nlm_Internal_Variable_Header *var_hdr;
extern Nlm_Internal_Version_Header *version_hdr;
extern Nlm_Internal_Copyright_Header *copyright_hdr;
extern Nlm_Internal_Extended_Header *extended_hdr;

/* Procedure named by CHECK.  */
extern char *check_procedure;
/* File named by CUSTOM.  */
extern char *custom_file;
/* Whether to generate debugging information (DEBUG).  */
extern bfd_boolean debug_info;
/* Procedure named by EXIT.  */
extern char *exit_procedure;
/* Exported symbols (EXPORT).  */
extern struct string_list *export_symbols;
/* List of files from INPUT.  */
extern struct string_list *input_files;
/* Map file name (MAP, FULLMAP).  */
extern char *map_file;
/* Whether a full map has been requested (FULLMAP).  */
extern bfd_boolean full_map;
/* File named by HELP.  */
extern char *help_file;
/* Imported symbols (IMPORT).  */
extern struct string_list *import_symbols;
/* File named by MESSAGES.  */
extern char *message_file;
/* Autoload module list (MODULE).  */
extern struct string_list *modules;
/* File named by OUTPUT.  */
extern char *output_file;
/* File named by SHARELIB.  */
extern char *sharelib_file;
/* Start procedure name (START).  */
extern char *start_procedure;
/* VERBOSE.  */
extern bfd_boolean verbose;
/* RPC description file (XDCDATA).  */
extern char *rpc_file;

/* The number of serious parse errors.  */
extern int parse_errors;

/* The parser.  */
extern int yyparse (void);

/* Tell the lexer what file to read.  */
extern bfd_boolean nlmlex_file (const char *);
