/* Allocate input grammar variables for bison,
   Copyright (C) 1984, 1986, 1989 Free Software Foundation, Inc.

This file is part of Bison, the GNU Compiler Compiler.

Bison is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Bison is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Bison; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */


/* comments for these variables are in gram.h  */

int nitems;
int nrules;
int nsyms;
int ntokens;
int nvars;

short *ritem;
short *rlhs;
short *rrhs;
short *rprec;
short *rprecsym;
short *sprec;
short *rassoc;
short *sassoc;
short *token_translations;
short *rline;

int start_symbol;

int translations;

int max_user_token_number;

int semantic_parser;

int pure_parser;

int error_token_number;

/* This is to avoid linker problems which occur on VMS when using GCC,
   when the file in question contains data definitions only.  */

void
dummy()
{
}
