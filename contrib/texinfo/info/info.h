/* info.h -- Header file which includes all of the other headers.
   $Id: info.h,v 1.16 2002/02/23 19:12:02 karl Exp $

   Copyright (C) 1993, 97, 98, 99, 2001 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

#ifndef INFO_H
#define INFO_H

/* We always want these, so why clutter up the compile command?  */
#define HANDLE_MAN_PAGES
#define NAMED_FUNCTIONS
#define INFOKEY

/* System dependencies.  */
#include "system.h"

/* Some of our other include files use these.  */
typedef int Function ();
typedef void VFunction ();
typedef char *CFunction ();

#include "filesys.h"
#include "doc.h"
#include "display.h"
#include "session.h"
#include "echo-area.h"
#include "footnotes.h"
#include "gc.h"

#define info_toupper(x) (islower (x) ? toupper (x) : x)
#define info_tolower(x) (isupper (x) ? tolower (x) : x)

#if !defined (whitespace)
#  define whitespace(c) ((c == ' ') || (c == '\t'))
#endif /* !whitespace */

#if !defined (whitespace_or_newline)
#  define whitespace_or_newline(c) (whitespace (c) || (c == '\n'))
#endif /* !whitespace_or_newline */

/* Add POINTER to the list of pointers found in ARRAY.  SLOTS is the number
   of slots that have already been allocated.  INDEX is the index into the
   array where POINTER should be added.  GROW is the number of slots to grow
   ARRAY by, in the case that it needs growing.  TYPE is a cast of the type
   of object stored in ARRAY (e.g., NODE_ENTRY *. */
#define add_pointer_to_array(pointer, idx, array, slots, grow, type) \
  do { \
    if (idx + 2 >= slots) \
      array = (type *)(xrealloc (array, (slots += grow) * sizeof (type))); \
    array[idx++] = (type)pointer; \
    array[idx] = (type)NULL; \
  } while (0)

#define maybe_free(x) do { if (x) free (x); } while (0)

#if !defined (zero_mem) && defined (HAVE_MEMSET)
#  define zero_mem(mem, length) memset (mem, 0, length)
#endif /* !zero_mem && HAVE_MEMSET */

#if !defined (zero_mem) && defined (HAVE_BZERO)
#  define zero_mem(mem, length) bzero (mem, length)
#endif /* !zero_mem && HAVE_BZERO */

#if !defined (zero_mem)
#  define zero_mem(mem, length) \
  do {                                  \
        register int zi;                \
        register unsigned char *place;  \
                                        \
        place = (unsigned char *)mem;   \
        for (zi = 0; zi < length; zi++) \
          place[zi] = 0;                \
      } while (0)
#endif /* !zero_mem */


/* A structure associating the nodes visited in a particular window. */
typedef struct {
  WINDOW *window;               /* The window that this list is attached to. */
  NODE **nodes;                 /* Array of nodes visited in this window. */
  int *pagetops;                /* For each node in NODES, the pagetop. */
  long *points;                 /* For each node in NODES, the point. */
  int current;                  /* Index in NODES of the current node. */
  int nodes_index;              /* Index where to add the next node. */
  int nodes_slots;              /* Number of slots allocated to NODES. */
} INFO_WINDOW;

/* Array of structures describing for each window which nodes have been
   visited in that window. */
extern INFO_WINDOW **info_windows;

/* For handling errors.  If you initialize the window system, you should
   also set info_windows_initialized_p to non-zero.  It is used by the
   info_error () function to determine how to format and output errors. */
extern int info_windows_initialized_p;

/* Non-zero if an error message has been printed. */
extern int info_error_was_printed;

/* Non-zero means ring terminal bell on errors. */
extern int info_error_rings_bell_p;

/* Non-zero means default keybindings are loosely modeled on vi(1).  */
extern int vi_keys_p;

/* Non-zero means don't remove ANSI escape sequences from man pages.  */
extern int raw_escapes_p;

/* Print FORMAT with ARG1 and ARG2.  If the window system was initialized,
   then the message is printed in the echo area.  Otherwise, a message is
   output to stderr. */
extern void info_error ();

/* Error message defines. */
extern char *msg_cant_find_node;
extern char *msg_cant_file_node;
extern char *msg_cant_find_window;
extern char *msg_cant_find_point;
extern char *msg_cant_kill_last;
extern char *msg_no_menu_node;
extern char *msg_no_foot_node;
extern char *msg_no_xref_node;
extern char *msg_no_pointer;
extern char *msg_unknown_command;
extern char *msg_term_too_dumb;
extern char *msg_at_node_bottom;
extern char *msg_at_node_top;
extern char *msg_one_window;
extern char *msg_win_too_small;
extern char *msg_cant_make_help;


extern char *filename_non_directory ();	/* Found in info-utils.c. */

#if defined(INFOKEY)
extern void set_variable_to_value ();	/* Found in variables.c. */
#endif /* INFOKEY */

#if !defined (BUILDING_LIBRARY)
extern int info_windows_initialized_p;	/* Found in session.c */
/* Found in window.c. */
extern void message_in_echo_area (), unmessage_in_echo_area ();
#endif /* !BUILDING_LIBRARY */

#endif /* !INFO_H */
