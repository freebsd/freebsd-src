/* This file is listing.h
   Copyright (C) 1987-1992 Free Software Foundation, Inc.
   
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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */
/*
 * $Id: listing.h,v 1.1 1993/11/03 00:51:56 paul Exp $
 */



#ifndef __listing_h__
#define __listing_h__

#define LISTING_LISTING    1
#define LISTING_SYMBOLS    2
#define LISTING_NOFORM     4
#define LISTING_HLL        8
#define LISTING_NODEBUG   16

#define LISTING_DEFAULT    (LISTING_LISTING | LISTING_HLL |  LISTING_SYMBOLS)

#ifndef NO_LISTING

#define LISTING_NEWLINE() { if (listing) listing_newline(input_line_pointer); }


#if __STDC__ == 1

void listing_eject(void);
void listing_error(char *message);
void listing_file(char *name);
void listing_flags(void);
void listing_list(unsigned int on);
void listing_newline(char *ps);
void listing_print(char *name);
void listing_psize(void);
void listing_source_file(char *);
void listing_source_line(unsigned int);
void listing_title(unsigned int depth);
void listing_warning(char *message);
void listing_width(unsigned int x);

#else /* not __STDC__ */

void listing_eject();
void listing_error();
void listing_file();
void listing_flags();
void listing_list();
void listing_newline();
void listing_print();
void listing_psize();
void listing_source_file();
void listing_source_line();
void listing_title();
void listing_warning();
void listing_width();

#endif /* not __STDC__ */

#else /* NO_LISTING */

#define LISTING_NEWLINE() {;}

/* Dummy functions for when compiled without listing enabled */

#define listing_flags() {;}
#define listing_list() {;}
#define listing_eject() {;}
#define listing_psize() {;}
#define listing_title(depth) {;}
#define listing_file(name) {;}
#define listing_newline(name) {;}
#define listing_source_line(n) {;}
#define listing_source_file(n) {;}

#endif /* NO_LISTING */

#endif /* __listing_h__ */

/* end of listing.h */
