/* This file is listing.h
   Copyright (C) 1987, 88, 89, 90, 91, 92, 93, 94, 95, 1997
   Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef __listing_h__
#define __listing_h__

#define LISTING_LISTING    1
#define LISTING_SYMBOLS    2
#define LISTING_NOFORM     4
#define LISTING_HLL        8
#define LISTING_NODEBUG   16
#define LISTING_NOCOND	  32
#define LISTING_MACEXP	  64

#define LISTING_DEFAULT    (LISTING_LISTING | LISTING_HLL | LISTING_SYMBOLS)

#ifndef NO_LISTING
#define LISTING_NEWLINE() { if (listing) listing_newline(NULL); }
#else
#define LISTING_NEWLINE() {;}
#endif
#define LISTING_EOF()     LISTING_NEWLINE()

#define LISTING_SKIP_COND() ((listing & LISTING_NOCOND) != 0)

void listing_eject PARAMS ((int));
void listing_error PARAMS ((const char *message));
void listing_file PARAMS ((const char *name));
void listing_flags PARAMS ((int));
void listing_list PARAMS ((int on));
void listing_newline PARAMS ((char *ps));
void listing_prev_line PARAMS ((void));
void listing_print PARAMS ((char *name));
void listing_psize PARAMS ((int));
void listing_nopage PARAMS ((int));
void listing_source_file PARAMS ((const char *));
void listing_source_line PARAMS ((unsigned int));
void listing_title PARAMS ((int depth));
void listing_warning PARAMS ((const char *message));
void listing_width PARAMS ((unsigned int x));

extern int listing_lhs_width;
extern int listing_lhs_width_second;
extern int listing_lhs_cont_lines;
extern int listing_rhs_width;

#endif /* __listing_h__ */

/* end of listing.h */
