/* dlltool.h -- header file for dlltool
   Copyright 1997, 1998 Free Software Foundation, Inc.

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

#include "ansidecl.h"
#include <stdio.h>

extern void def_code PARAMS ((int));
extern void def_data PARAMS ((int));
extern void def_description PARAMS ((const char *));
extern void def_exports
  PARAMS ((const char *, const char *, int, int, int, int));
extern void def_heapsize PARAMS ((int, int));
extern void def_import
  PARAMS ((const char *, const char *, const char *, const char *, int));
extern void def_library PARAMS ((const char *, int));
extern void def_name PARAMS ((const char *, int));
extern void def_section PARAMS ((const char *, int));
extern void def_stacksize PARAMS ((int, int));
extern void def_version PARAMS ((int, int));
extern int yyparse PARAMS ((void));
extern int yyerror PARAMS ((const char *));
extern int yydebug;
extern int yylex PARAMS ((void));
extern FILE *yyin;
extern int linenumber;
