/* info-utils.c -- miscellanous.
   $Id: info-utils.c,v 1.4 2004/04/11 17:56:45 karl Exp $

   Copyright (C) 1993, 1998, 2003, 2004 Free Software Foundation, Inc.

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

   Originally written by Brian Fox (bfox@ai.mit.edu). */

#include "info.h"
#include "info-utils.h"
#if defined (HANDLE_MAN_PAGES)
#  include "man.h"
#endif /* HANDLE_MAN_PAGES */

/* When non-zero, various display and input functions handle ISO Latin
   character sets correctly. */
int ISO_Latin_p = 1;

/* Variable which holds the most recent filename parsed as a result of
   calling info_parse_xxx (). */
char *info_parsed_filename = (char *)NULL;

/* Variable which holds the most recent nodename parsed as a result of
   calling info_parse_xxx (). */
char *info_parsed_nodename = (char *)NULL;

/* Variable which holds the most recent line number parsed as a result of
   calling info_parse_xxx (). */
int info_parsed_line_number = 0;

/* Functions to remember a filename or nodename for later return. */
static void save_filename (char *filename);
static void saven_filename (char *filename, int len);
static void save_nodename (char *nodename);
static void saven_nodename (char *nodename, int len);

/* How to get a reference (either menu or cross). */
static REFERENCE **info_references_internal (char *label,
    SEARCH_BINDING *binding);

/* Parse the filename and nodename out of STRING.  If STRING doesn't
   contain a filename (i.e., it is NOT (FILENAME)NODENAME) then set
   INFO_PARSED_FILENAME to NULL.  If second argument NEWLINES_OKAY is
   non-zero, it says to allow the nodename specification to cross a
   newline boundary (i.e., only `,', `.', or `TAB' can end the spec). */
void
info_parse_node (char *string, int newlines_okay)
{
  register int i = 0;

  /* Default the answer. */
  save_filename ((char *)NULL);
  save_nodename ((char *)NULL);

  /* Special case of nothing passed.  Return nothing. */
  if (!string || !*string)
    return;

  string += skip_whitespace (string);

  /* Check for (FILENAME)NODENAME. */
  if (*string == '(')
    {
      i = 0;
      /* Advance past the opening paren. */
      string++;

      /* Find the closing paren. */
      while (string[i] && string[i] != ')')
        i++;

      /* Remember parsed filename. */
      saven_filename (string, i);

      /* Point directly at the nodename. */
      string += i;

      if (*string)
        string++;
    }

  /* Parse out nodename. */
  i = skip_node_characters (string, newlines_okay);
  saven_nodename (string, i);
  canonicalize_whitespace (info_parsed_nodename);
  if (info_parsed_nodename && !*info_parsed_nodename)
    {
      free (info_parsed_nodename);
      info_parsed_nodename = (char *)NULL;
    }

  /* Parse ``(line ...)'' part of menus, if any.  */
  {
    char *rest = string + i;

    /* Advance only if it's not already at end of string.  */
    if (*rest)
      rest++;

    /* Skip any whitespace first, and then a newline in case the item
       was so long to contain the ``(line ...)'' string in the same
       physical line.  */
    while (whitespace(*rest))
      rest++;
    if (*rest == '\n')
      {
        rest++;
        while (whitespace(*rest))
          rest++;
      }

    /* Are we looking at an opening parenthesis?  That can only mean
       we have a winner. :)  */
    if (strncmp (rest, "(line ", strlen ("(line ")) == 0)
      {
        rest += strlen ("(line ");
        info_parsed_line_number = strtol (rest, NULL, 0);
      }
    else
      info_parsed_line_number = 0;
  }
}

/* Return the node addressed by LABEL in NODE (usually one of "Prev:",
   "Next:", "Up:", "File:", or "Node:".  After a call to this function,
   the global INFO_PARSED_NODENAME and INFO_PARSED_FILENAME contain
   the information. */
void
info_parse_label (char *label, NODE *node)
{
  register int i;
  char *nodeline;

  /* Default answer to failure. */
  save_nodename ((char *)NULL);
  save_filename ((char *)NULL);

  /* Find the label in the first line of this node. */
  nodeline = node->contents;
  i = string_in_line (label, nodeline);

  if (i == -1)
    return;

  nodeline += i;
  nodeline += skip_whitespace (nodeline);
  info_parse_node (nodeline, DONT_SKIP_NEWLINES);
}

/* **************************************************************** */
/*                                                                  */
/*                  Finding and Building Menus                      */
/*                                                                  */
/* **************************************************************** */

/* Return a NULL terminated array of REFERENCE * which represents the menu
   found in NODE.  If there is no menu in NODE, just return a NULL pointer. */
REFERENCE **
info_menu_of_node (NODE *node)
{
  long position;
  SEARCH_BINDING tmp_search;
  REFERENCE **menu = (REFERENCE **)NULL;

  tmp_search.buffer = node->contents;
  tmp_search.start = 0;
  tmp_search.end = node->nodelen;
  tmp_search.flags = S_FoldCase;

  /* Find the start of the menu. */
  position = search_forward (INFO_MENU_LABEL, &tmp_search);

  if (position == -1)
    return ((REFERENCE **) NULL);

  /* We have the start of the menu now.  Glean menu items from the rest
     of the node. */
  tmp_search.start = position + strlen (INFO_MENU_LABEL);
  tmp_search.start += skip_line (tmp_search.buffer + tmp_search.start);
  tmp_search.start--;
  menu = info_menu_items (&tmp_search);
  return (menu);
}

/* Return a NULL terminated array of REFERENCE * which represents the cross
   refrences found in NODE.  If there are no cross references in NODE, just
   return a NULL pointer. */
REFERENCE **
info_xrefs_of_node (NODE *node)
{
  SEARCH_BINDING tmp_search;

#if defined (HANDLE_MAN_PAGES)
  if (node->flags & N_IsManPage)
    return (xrefs_of_manpage (node));
#endif

  tmp_search.buffer = node->contents;
  tmp_search.start = 0;
  tmp_search.end = node->nodelen;
  tmp_search.flags = S_FoldCase;

  return (info_xrefs (&tmp_search));
}

/* Glean menu entries from BINDING->buffer + BINDING->start until we
   have looked at the entire contents of BINDING.  Return an array
   of REFERENCE * that represents each menu item in this range. */
REFERENCE **
info_menu_items (SEARCH_BINDING *binding)
{
  return (info_references_internal (INFO_MENU_ENTRY_LABEL, binding));
}

/* Glean cross references from BINDING->buffer + BINDING->start until
   BINDING->end.  Return an array of REFERENCE * that represents each
   cross reference in this range. */
REFERENCE **
info_xrefs (SEARCH_BINDING *binding)
{
  return (info_references_internal (INFO_XREF_LABEL, binding));
}

/* Glean cross references or menu items from BINDING.  Return an array
   of REFERENCE * that represents the items found. */
static REFERENCE **
info_references_internal (char *label, SEARCH_BINDING *binding)
{
  SEARCH_BINDING tmp_search;
  REFERENCE **refs = (REFERENCE **)NULL;
  int refs_index = 0, refs_slots = 0;
  int searching_for_menu_items = 0;
  long position;

  tmp_search.buffer = binding->buffer;
  tmp_search.start = binding->start;
  tmp_search.end = binding->end;
  tmp_search.flags = S_FoldCase | S_SkipDest;

  searching_for_menu_items = (strcasecmp (label, INFO_MENU_ENTRY_LABEL) == 0);

  while ((position = search_forward (label, &tmp_search)) != -1)
    {
      int offset, start;
      char *refdef;
      REFERENCE *entry;

      tmp_search.start = position;
      tmp_search.start += skip_whitespace (tmp_search.buffer + tmp_search.start);
      start = tmp_search.start - binding->start;
      refdef = tmp_search.buffer + tmp_search.start;
      offset = string_in_line (":", refdef);

      /* When searching for menu items, if no colon, there is no
         menu item on this line. */
      if (offset == -1)
        {
          if (searching_for_menu_items)
            continue;
          else
            {
              int temp;

              temp = skip_line (refdef);
              offset = string_in_line (":", refdef + temp);
              if (offset == -1)
                continue;       /* Give up? */
              else
                offset += temp;
            }
        }

      entry = (REFERENCE *)xmalloc (sizeof (REFERENCE));
      entry->filename = (char *)NULL;
      entry->nodename = (char *)NULL;
      entry->label = (char *)xmalloc (offset);
      strncpy (entry->label, refdef, offset - 1);
      entry->label[offset - 1] = '\0';
      canonicalize_whitespace (entry->label);

      refdef += offset;
      entry->start = start;
      entry->end = refdef - binding->buffer;

      /* If this reference entry continues with another ':' then the
         nodename is the same as the label. */
      if (*refdef == ':')
        {
          entry->nodename = xstrdup (entry->label);
        }
      else
        {
          /* This entry continues with a specific nodename.  Parse the
             nodename from the specification. */

          refdef += skip_whitespace_and_newlines (refdef);

          if (searching_for_menu_items)
            info_parse_node (refdef, DONT_SKIP_NEWLINES);
          else
            info_parse_node (refdef, SKIP_NEWLINES);

          if (info_parsed_filename)
            entry->filename = xstrdup (info_parsed_filename);

          if (info_parsed_nodename)
            entry->nodename = xstrdup (info_parsed_nodename);

          entry->line_number = info_parsed_line_number;
        }

      add_pointer_to_array
        (entry, refs_index, refs, refs_slots, 50, REFERENCE *);
    }
  return (refs);
}

/* Get the entry associated with LABEL in REFERENCES.  Return a pointer
   to the ENTRY if found, or NULL. */
REFERENCE *
info_get_labeled_reference (char *label, REFERENCE **references)
{
  register int i;
  REFERENCE *entry;

  for (i = 0; references && (entry = references[i]); i++)
    {
      if (strcmp (label, entry->label) == 0)
        return (entry);
    }
  return ((REFERENCE *)NULL);
}

/* A utility function for concatenating REFERENCE **.  Returns a new
   REFERENCE ** which is the concatenation of REF1 and REF2.  The REF1
   and REF2 arrays are freed, but their contents are not. */
REFERENCE **
info_concatenate_references (REFERENCE **ref1, REFERENCE **ref2)
{
  register int i, j;
  REFERENCE **result;
  int size;

  /* With one argument passed as NULL, simply return the other arg. */
  if (!ref1)
    return (ref2);
  else if (!ref2)
    return (ref1);

  /* Get the total size of the slots that we will need. */
  for (i = 0; ref1[i]; i++);
  size = i;
  for (i = 0; ref2[i]; i++);
  size += i;

  result = (REFERENCE **)xmalloc ((1 + size) * sizeof (REFERENCE *));

  /* Copy the contents over. */
  for (i = 0; ref1[i]; i++)
    result[i] = ref1[i];

  j = i;
  for (i = 0; ref2[i]; i++)
    result[j++] = ref2[i];

  result[j] = (REFERENCE *)NULL;
  free (ref1);
  free (ref2);
  return (result);
}



/* Copy a reference structure.  Since we tend to free everything at
   every opportunity, we don't share any points, but copy everything into
   new memory.  */
REFERENCE *
info_copy_reference (REFERENCE *src)
{
  REFERENCE *dest = xmalloc (sizeof (REFERENCE));
  dest->label = src->label ? xstrdup (src->label) : NULL;
  dest->filename = src->filename ? xstrdup (src->filename) : NULL;
  dest->nodename = src->nodename ? xstrdup (src->nodename) : NULL;
  dest->start = src->start;
  dest->end = src->end;
  
  return dest;
}



/* Free the data associated with REFERENCES. */
void
info_free_references (REFERENCE **references)
{
  register int i;
  REFERENCE *entry;

  if (references)
    {
      for (i = 0; references && (entry = references[i]); i++)
        {
          maybe_free (entry->label);
          maybe_free (entry->filename);
          maybe_free (entry->nodename);

          free (entry);
        }

      free (references);
    }
}

/* Search for sequences of whitespace or newlines in STRING, replacing
   all such sequences with just a single space.  Remove whitespace from
   start and end of string. */
void
canonicalize_whitespace (char *string)
{
  register int i, j;
  int len, whitespace_found, whitespace_loc = 0;
  char *temp;

  if (!string)
    return;

  len = strlen (string);
  temp = (char *)xmalloc (1 + len);

  /* Search for sequences of whitespace or newlines.  Replace all such
     sequences in the string with just a single space. */

  whitespace_found = 0;
  for (i = 0, j = 0; string[i]; i++)
    {
      if (whitespace_or_newline (string[i]))
        {
          whitespace_found++;
          whitespace_loc = i;
          continue;
        }
      else
        {
          if (whitespace_found && whitespace_loc)
            {
              whitespace_found = 0;

              /* Suppress whitespace at start of string. */
              if (j)
                temp[j++] = ' ';
            }

          temp[j++] = string[i];
        }
    }

  /* Kill trailing whitespace. */
  if (j && whitespace (temp[j - 1]))
    j--;

  temp[j] = '\0';
  strcpy (string, temp);
  free (temp);
}

/* String representation of a char returned by printed_representation (). */
static char the_rep[10];

/* Return a pointer to a string which is the printed representation
   of CHARACTER if it were printed at HPOS. */
char *
printed_representation (unsigned char character, int hpos)
{
  register int i = 0;
  int printable_limit = ISO_Latin_p ? 255 : 127;

  if (raw_escapes_p && character == '\033')
    the_rep[i++] = character;
  /* Show CTRL-x as ^X.  */
  else if (iscntrl (character) && character < 127)
    {
      switch (character)
        {
        case '\r':
        case '\n':
          the_rep[i++] = character;
          break;

        case '\t':
          {
            int tw;

            tw = ((hpos + 8) & 0xf8) - hpos;
            while (i < tw)
              the_rep[i++] = ' ';
          }
          break;

        default:
          the_rep[i++] = '^';
          the_rep[i++] = (character | 0x40);
        }
    }
  /* Show META-x as 0370.  */
  else if (character > printable_limit)
    {
      sprintf (the_rep + i, "\\%0o", character);
      i = strlen (the_rep);
    }
  else if (character == DEL)
    {
      the_rep[i++] = '^';
      the_rep[i++] = '?';
    }
  else
    the_rep[i++] = character;

  the_rep[i] = 0;

  return the_rep;
}


/* **************************************************************** */
/*                                                                  */
/*                  Functions Static To This File                   */
/*                                                                  */
/* **************************************************************** */

/* Amount of space allocated to INFO_PARSED_FILENAME via xmalloc (). */
static int parsed_filename_size = 0;

/* Amount of space allocated to INFO_PARSED_NODENAME via xmalloc (). */
static int parsed_nodename_size = 0;

static void save_string (char *string, char **string_p, int *string_size_p);
static void saven_string (char *string, int len, char **string_p,
    int *string_size_p);

/* Remember FILENAME in PARSED_FILENAME.  An empty FILENAME is translated
   to a NULL pointer in PARSED_FILENAME. */
static void
save_filename (char *filename)
{
  save_string (filename, &info_parsed_filename, &parsed_filename_size);
}

/* Just like save_filename (), but you pass the length of the string. */
static void
saven_filename (char *filename, int len)
{
  saven_string (filename, len,
                &info_parsed_filename, &parsed_filename_size);
}

/* Remember NODENAME in PARSED_NODENAME.  An empty NODENAME is translated
   to a NULL pointer in PARSED_NODENAME. */
static void
save_nodename (char *nodename)
{
  save_string (nodename, &info_parsed_nodename, &parsed_nodename_size);
}

/* Just like save_nodename (), but you pass the length of the string. */
static void
saven_nodename (char *nodename, int len)
{
  saven_string (nodename, len,
                &info_parsed_nodename, &parsed_nodename_size);
}

/* Remember STRING in STRING_P.  STRING_P should currently have STRING_SIZE_P
   bytes allocated to it.  An empty STRING is translated to a NULL pointer
   in STRING_P. */
static void
save_string (char *string, char **string_p, int *string_size_p)
{
  if (!string || !*string)
    {
      if (*string_p)
        free (*string_p);

      *string_p = (char *)NULL;
      *string_size_p = 0;
    }
  else
    {
      if (strlen (string) >= (unsigned int) *string_size_p)
        *string_p = (char *)xrealloc
          (*string_p, (*string_size_p = 1 + strlen (string)));

      strcpy (*string_p, string);
    }
}

/* Just like save_string (), but you also pass the length of STRING. */
static void
saven_string (char *string, int len, char **string_p, int *string_size_p)
{
  if (!string)
    {
      if (*string_p)
        free (*string_p);

      *string_p = (char *)NULL;
      *string_size_p = 0;
    }
  else
    {
      if (len >= *string_size_p)
        *string_p = (char *)xrealloc (*string_p, (*string_size_p = 1 + len));

      strncpy (*string_p, string, len);
      (*string_p)[len] = '\0';
    }
}

/* Return a pointer to the part of PATHNAME that simply defines the file. */
char *
filename_non_directory (char *pathname)
{
  register char *filename = pathname + strlen (pathname);

  if (HAVE_DRIVE (pathname))
    pathname += 2;

  while (filename > pathname && !IS_SLASH (filename[-1]))
    filename--;

  return (filename);
}

/* Return non-zero if NODE is one especially created by Info. */
int
internal_info_node_p (NODE *node)
{
#if defined (NEVER)
  if (node &&
      (node->filename && !*node->filename) &&
      !node->parent && node->nodename)
    return (1);
  else
    return (0);
#else
  return ((node != (NODE *)NULL) && ((node->flags & N_IsInternal) != 0));
#endif /* !NEVER */
}

/* Make NODE appear to be one especially created by Info. */
void
name_internal_node (NODE *node, char *name)
{
  if (!node)
    return;

  node->filename = "";
  node->parent = (char *)NULL;
  node->nodename = name;
  node->flags |= N_IsInternal;
}

/* Return the window displaying NAME, the name of an internally created
   Info window. */
WINDOW *
get_internal_info_window (char *name)
{
  WINDOW *win;

  for (win = windows; win; win = win->next)
    if (internal_info_node_p (win->node) &&
        (strcmp (win->node->nodename, name) == 0))
      break;

  return (win);
}

/* Return a window displaying the node NODE. */
WINDOW *
get_window_of_node (NODE *node)
{
  WINDOW *win = (WINDOW *)NULL;

  for (win = windows; win; win = win->next)
    if (win->node == node)
      break;

  return (win);
}
