/* This file is listing.h
   Copyright (C) 1987, 1988, 1989, 1990, 1991, 1992 Free Software Foundation, Inc.

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
 * $FreeBSD: src/gnu/usr.bin/as/listing.h,v 1.5 1999/08/27 23:34:18 peter Exp $
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
#else
#define LISTING_NEWLINE() {;}
#endif


/* This structure remembers which .s were used */
typedef struct file_info_struct
{
  char *filename;
  int linenum;
  FILE *file;
  struct file_info_struct *next;
  int end_pending;

}

file_info_type;


/* this structure rememebrs which line from which file goes into which
   frag */
typedef struct list_info_struct
{
  /* Frag which this line of source is nearest to */
  fragS *frag;
  /* The actual line in the source file */
  unsigned int line;
  /* Pointer to the file info struct for the file which this line
     belongs to */
  file_info_type *file;

  /* Next in list */
  struct list_info_struct *next;


  /* Pointer to the file info struct for the high level language
     source line that belongs here */
  file_info_type *hll_file;

  /* High level language source line */
  int hll_line;


  /* Pointer to any error message associated with this line */
  char *message;

  enum
    {
      EDICT_NONE,
      EDICT_SBTTL,
      EDICT_TITLE,
      EDICT_NOLIST,
      EDICT_LIST,
      EDICT_EJECT
    } edict;
  char *edict_arg;

}

list_info_type;

void listing_eject PARAMS ((int));
void listing_error PARAMS ((const char *message));
void listing_file PARAMS ((const char *name));
void listing_flags PARAMS ((int));
void listing_list PARAMS ((int on));
void listing_newline PARAMS ((char *ps));
void listing_prev_line PARAMS ((void));
void listing_print PARAMS ((char *name));
void listing_psize PARAMS ((int));
void listing_source_file PARAMS ((const char *));
void listing_source_line PARAMS ((unsigned int));
void listing_title PARAMS ((int depth));
void listing_warning PARAMS ((const char *message));
void listing_width PARAMS ((unsigned int x));

#endif /* __listing_h__ */

/* end of listing.h */
