/* source.h

   Copyright 2000, 2001, 2002 Free Software Foundation, Inc.

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

#ifndef source_h
#define source_h

typedef struct source_file
  {
    struct source_file *next;
    const char *name;		/* Name of source file.  */
    unsigned long ncalls;	/* # of "calls" to this file.  */
    int num_lines;		/* # of lines in file.  */
    int nalloced;		/* Number of lines allocated.  */
    void **line;		/* Usage-dependent per-line data.  */
  }
Source_File;

/* Options.  */

/* Create annotated output files?  */
extern bfd_boolean create_annotation_files;

/* List of directories to search for source files.  */
extern Search_List src_search_list;

/* Chain of source-file descriptors.  */
extern Source_File *first_src_file;

/* Returns pointer to source file descriptor for PATH/FILENAME.  */
extern Source_File *source_file_lookup_path PARAMS ((const char *));
extern Source_File *source_file_lookup_name PARAMS ((const char *));

/* Read source file SF output annotated source.  The annotation is at
   MAX_WIDTH characters wide and for each source-line an annotation is
   obtained by invoking function ANNOTE.  ARG is an argument passed to
   ANNOTE that is left uninterpreted by annotate_source().

   Returns a pointer to the output file (which maybe stdout) such
   that summary statistics can be printed.  If the returned file
   is not stdout, it should be closed when done with it.  */
extern FILE *annotate_source
  PARAMS ((Source_File *sf, unsigned int max_width,
	   void (*annote) (char *, unsigned int, int, PTR arg),
	   PTR arg));
#endif /* source_h */
