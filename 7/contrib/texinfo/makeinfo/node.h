/* node.h -- declarations for Node.
   $Id: node.h,v 1.2 2004/04/11 17:56:47 karl Exp $

   Copyright (C) 1996, 1997, 1998, 1999, 2002 Free Software Foundation, Inc.

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

#ifndef NODE_H
#define NODE_H

#include "xref.h"

/* The various references that we know about. */
/* What we remember for each node. */
typedef struct tentry
{
  struct tentry *next_ent;
  char *node;           /* Name of this node. */
  char *prev;           /* Name of "Prev:" for this node. */
  char *next;           /* Name of "Next:" for this node. */
  char *up;             /* Name of "Up:" for this node.   */
  int position;         /* Output file position of this node. */
  int line_no;          /* Defining line in source file. */
  char *filename;       /* The file that this node was found in. */
  int touched;          /* Nonzero means this node has been referenced. */
  int flags;
  int number;           /* Number for this node, relevant for HTML
                           splitting -- from use+define order, not just
                           define. */
  int order;            /* The order of the tag, starting from zero.  */
  char *html_fname;	/* The HTML file to which this node is written
			   (non-NULL only for HTML splitting).  */
} TAG_ENTRY;

/* If node-a has a "Next" for node-b, but node-b has no "Prev" for node-a,
   we turn on this flag bit in node-b's tag entry.  This means that when
   it is time to validate node-b, we don't report an additional error
   if there was no "Prev" field. */
#define TAG_FLAG_PREV_ERROR  1
#define TAG_FLAG_NEXT_ERROR  2
#define TAG_FLAG_UP_ERROR    4
#define TAG_FLAG_NO_WARN     8
#define TAG_FLAG_IS_TOP     16
#define TAG_FLAG_ANCHOR     32

/* Menu reference, *note reference, and validation hacking. */

/* A structure to remember references with.  A reference to a node is
   either an entry in a menu, or a cross-reference made with [px]ref. */
typedef struct node_ref
{
  struct node_ref *next;
  char *node;                   /* Name of node referred to. */
  char *containing_node;        /* Name of node containing this reference. */
  int line_no;                  /* Line number where the reference occurs. */
  int section;                  /* Section level where the reference occurs. */
  char *filename;               /* Name of file where the reference occurs. */
  enum reftype type;            /* Type of reference, either menu or note. */
  int number;                   /* Number for this node, relevant for
                                   HTML splitting -- from use+define
                                   order, not just define. */
} NODE_REF;

/* The linked list of such structures. */
extern NODE_REF *node_references;

/* A similar list for references occuring in @node next
   and similar references, needed for HTML. */
extern NODE_REF *node_node_references;

/* List of all nodes.  */
extern TAG_ENTRY *tag_table;

/* Counter for setting node_ref.number; zero is Top. */
extern int node_number;

/* Node order counter.  */
extern int node_order;

/* The current node's section level. */
extern int current_section;

/* Nonzero when the next sectioning command should generate an anchor
   corresponding to the current node in HTML mode. */
extern int outstanding_node;

extern TAG_ENTRY *find_node (char *name);

/* A search string which is used to find a line defining a node. */
DECLARE (char *, node_search_string, "\n@node ");

/* Extract node name from a menu item. */
extern char *glean_node_from_menu (int remember_ref, enum reftype ref_type);

/* Remember a node for later validation.  */
extern void remember_node_reference (char *node, int line, enum reftype type);

/* Remember the name of the current output file.  */
extern void set_current_output_filename (const char *fname);

/* Expand macros and commands in the node name and canonicalize
   whitespace in the resulting expansion.  */
extern char *expand_node_name (char *node);

extern int number_of_node (char *node);

extern void init_tag_table (void);
extern void write_tag_table (char *filename);
extern void free_node_references (void);
extern void free_node_node_references (void);
extern void validate_file (TAG_ENTRY *tag_table);
extern void split_file (char *filename, int size);
extern void clean_old_split_files (char *filename);

#endif /* NODE_H */
