/* gmon_out.h

   Copyright 2000, 2001 Free Software Foundation, Inc.

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

/* A gmon.out file consists of a header (defined by gmon_hdr) followed
   by a sequence of records.  Each record starts with a one-byte tag
   identifying the type of records, followed by records specific data.  */
#ifndef gmon_out_h
#define gmon_out_h

#define	GMON_MAGIC	"gmon"	/* magic cookie */
#define GMON_VERSION	1	/* version number */

/* Raw header as it appears on file (without padding).  */
struct gmon_hdr
  {
    char cookie[4];
    char version[4];
    char spare[3 * 4];
  };

/* Types of records in this file.  */
typedef enum
  {
    GMON_TAG_TIME_HIST = 0, GMON_TAG_CG_ARC = 1, GMON_TAG_BB_COUNT = 2
  }
GMON_Record_Tag;

#endif /* gmon_out_h */
