/* infodoc.c -- functions which build documentation nodes.
   $Id: infodoc.c,v 1.6 2003/05/13 16:22:11 karl Exp $

   Copyright (C) 1993, 1997, 1998, 1999, 2001, 2002, 2003 Free Software
   Foundation, Inc.

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

#include "info.h"
#include "funs.h"

/* HELP_NODE_GETS_REGENERATED is always defined now that keys may get
   rebound, or other changes in the help text may occur.  */
#define HELP_NODE_GETS_REGENERATED 1

/* The name of the node used in the help window. */
static char *info_help_nodename = "*Info Help*";

/* A node containing printed key bindings and their documentation. */
static NODE *internal_info_help_node = (NODE *)NULL;

/* A pointer to the contents of the help node. */
static char *internal_info_help_node_contents = (char *)NULL;

/* The (more or less) static text which appears in the internal info
   help node.  The actual key bindings are inserted.  Keep the
   underlines (****, etc.) in the same N_ call as  the text lines they
   refer to, so translations can make the number of *'s or -'s match.  */
#if defined(INFOKEY)

static char *info_internal_help_text[] = {
  N_("Basic Commands in Info Windows\n\
******************************\n"),
  "\n",
  N_("\\%-10[quit-help]  Quit this help.\n"),
  N_("\\%-10[quit]  Quit Info altogether.\n"),
  N_("\\%-10[get-info-help-node]  Invoke the Info tutorial.\n"),
  "\n",
  N_("Selecting other nodes:\n\
----------------------\n"),
  N_("\\%-10[next-node]  Move to the \"next\" node of this node.\n"),
  N_("\\%-10[prev-node]  Move to the \"previous\" node of this node.\n"),
  N_("\\%-10[up-node]  Move \"up\" from this node.\n"),
  N_("\\%-10[menu-item]  Pick menu item specified by name.\n\
              Picking a menu item causes another node to be selected.\n"),
  N_("\\%-10[xref-item]  Follow a cross reference.  Reads name of reference.\n"),
  N_("\\%-10[history-node]  Move to the last node seen in this window.\n"),
  N_("\\%-10[move-to-next-xref]  Skip to next hypertext link within this node.\n"),
  N_("\\%-10[move-to-prev-xref]  Skip to previous hypertext link within this node.\n"),
  N_("\\%-10[select-reference-this-line]  Follow the hypertext link under cursor.\n"),
  N_("\\%-10[dir-node]  Move to the `directory' node.  Equivalent to `\\[goto-node] (DIR)'.\n"),
  N_("\\%-10[top-node]  Move to the Top node.  Equivalent to `\\[goto-node] Top'.\n"),
  "\n",
  N_("Moving within a node:\n\
---------------------\n"),
  N_("\\%-10[beginning-of-node]  Go to the beginning of this node.\n"),
  N_("\\%-10[end-of-node]  Go to the end of this node.\n"),
  N_("\\%-10[next-line]  Scroll forward 1 line.\n"),
  N_("\\%-10[prev-line]  Scroll backward 1 line.\n"),
  N_("\\%-10[scroll-forward]  Scroll forward a page.\n"),
  N_("\\%-10[scroll-backward]  Scroll backward a page.\n"),
  "\n",
  N_("Other commands:\n\
---------------\n"),
  N_("\\%-10[menu-digit]  Pick first ... ninth item in node's menu.\n"),
  N_("\\%-10[last-menu-item]  Pick last item in node's menu.\n"),
  N_("\\%-10[index-search]  Search for a specified string in the index entries of this Info\n\
              file, and select the node referenced by the first entry found.\n"),
  N_("\\%-10[goto-node]  Move to node specified by name.\n\
              You may include a filename as well, as in (FILENAME)NODENAME.\n"),
  N_("\\%-10[search]  Search forward for a specified string\n\
              and select the node in which the next occurrence is found.\n"),
  N_("\\%-10[search-backward]  Search backward for a specified string\n\
              and select the node in which the previous occurrence is found.\n"),
  NULL
};

#else /* !INFOKEY */

static char *info_internal_help_text[] = {
  N_("Basic Commands in Info Windows\n\
******************************\n"),
  "\n",
  N_("  %-10s  Quit this help.\n"),
  N_("  %-10s  Quit Info altogether.\n"),
  N_("  %-10s  Invoke the Info tutorial.\n"),
  "\n",
  N_("Selecting other nodes:\n\
----------------------\n",
  N_("  %-10s  Move to the `next' node of this node.\n"),
  N_("  %-10s  Move to the `previous' node of this node.\n"),
  N_("  %-10s  Move `up' from this node.\n"),
  N_("  %-10s  Pick menu item specified by name.\n"),
  N_("              Picking a menu item causes another node to be selected.\n"),
  N_("  %-10s  Follow a cross reference.  Reads name of reference.\n"),
  N_("  %-10s  Move to the last node seen in this window.\n"),
  N_("  %-10s  Skip to next hypertext link within this node.\n"),
  N_("  %-10s  Follow the hypertext link under cursor.\n"),
  N_("  %-10s  Move to the `directory' node.  Equivalent to `g (DIR)'.\n"),
  N_("  %-10s  Move to the Top node.  Equivalent to `g Top'.\n"),
  "\n",
  N_("Moving within a node:\n\
---------------------\n"),
  N_("  %-10s  Scroll forward a page.\n"),
  N_("  %-10s  Scroll backward a page.\n"),
  N_("  %-10s  Go to the beginning of this node.\n"),
  N_("  %-10s  Go to the end of this node.\n"),
  N_("  %-10s  Scroll forward 1 line.\n"),
  N_("  %-10s  Scroll backward 1 line.\n"),
  "\n",
  N_("Other commands:\n\
---------------\n"),
  N_("  %-10s  Pick first ... ninth item in node's menu.\n"),
  N_("  %-10s  Pick last item in node's menu.\n"),
  N_("  %-10s  Search for a specified string in the index entries of this Info\n"),
  N_("              file, and select the node referenced by the first entry found.\n"),
  N_("  %-10s  Move to node specified by name.\n"),
  N_("              You may include a filename as well, as in (FILENAME)NODENAME.\n"),
  N_("  %-10s  Search forward for a specified string,\n"),
  N_("              and select the node in which the next occurrence is found.\n"),
  N_("  %-10s  Search backward for a specified string\n"),
  N_("              and select the node in which the next occurrence is found.\n"),
  NULL
};

static char *info_help_keys_text[][2] = {
  { "", "" },
  { "", "" },
  { "", "" },
  { "CTRL-x 0", "CTRL-x 0" },
  { "q", "q" },
  { "h", "ESC h" },
  { "", "" },
  { "", "" },
  { "", "" },
  { "SPC", "SPC" },
  { "DEL", "b" },
  { "b", "ESC b" },
  { "e", "ESC e" },
  { "ESC 1 SPC", "RET" },
  { "ESC 1 DEL", "y" },
  { "", "" },
  { "", "" },
  { "", "" },
  { "n", "CTRL-x n" },
  { "p", "CTRL-x p" },
  { "u", "CTRL-x u" },
  { "m", "ESC m" },
  { "", "" },
  { "f", "ESC f" },
  { "l", "l" },
  { "TAB", "TAB" },
  { "RET", "CTRL-x RET" },
  { "d", "ESC d" },
  { "t", "ESC t" },
  { "", "" },
  { "", "" },
  { "", "" },
  { "1-9", "ESC 1-9" },
  { "0", "ESC 0" },
  { "i", "CTRL-x i" },
  { "", "" },
  { "g", "CTRL-x g" },
  { "", "" },
  { "s", "/" },
  { "", "" },
  { "ESC - s", "?" },
  { "", "" },
  NULL
};

#endif /* !INFOKEY */

static char *where_is_internal ();

void
dump_map_to_message_buffer (prefix, map)
     char *prefix;
     Keymap map;
{
  register int i;
  unsigned prefix_len = strlen (prefix);
  char *new_prefix = (char *)xmalloc (prefix_len + 2);

  strncpy (new_prefix, prefix, prefix_len);
  new_prefix[prefix_len + 1] = '\0';

  for (i = 0; i < 256; i++)
    {
      new_prefix[prefix_len] = i;
      if (map[i].type == ISKMAP)
        {
          dump_map_to_message_buffer (new_prefix, (Keymap)map[i].function);
        }
      else if (map[i].function)
        {
          register int last;
          char *doc, *name;

          doc = function_documentation (map[i].function);
          name = function_name (map[i].function);

          if (!*doc)
            continue;

          /* Find out if there is a series of identical functions, as in
             ea_insert (). */
          for (last = i + 1; last < 256; last++)
            if ((map[last].type != ISFUNC) ||
                (map[last].function != map[i].function))
              break;

          if (last - 1 != i)
            {
              printf_to_message_buffer ("%s .. ", pretty_keyseq (new_prefix));
              new_prefix[prefix_len] = last - 1;
              printf_to_message_buffer ("%s\t", pretty_keyseq (new_prefix));
              i = last - 1;
            }
          else
            printf_to_message_buffer ("%s\t", pretty_keyseq (new_prefix));

#if defined (NAMED_FUNCTIONS)
          /* Print the name of the function, and some padding before the
             documentation string is printed. */
          {
            int length_so_far;
            int desired_doc_start = 40; /* Must be multiple of 8. */

            printf_to_message_buffer ("(%s)", name);
            length_so_far = message_buffer_length_this_line ();

            if ((desired_doc_start + strlen (doc)) >= the_screen->width)
              printf_to_message_buffer ("\n     ");
            else
              {
                while (length_so_far < desired_doc_start)
                  {
                    printf_to_message_buffer ("\t");
                    length_so_far += character_width ('\t', length_so_far);
                  }
              }
          }
#endif /* NAMED_FUNCTIONS */
          printf_to_message_buffer ("%s\n", doc);
        }
    }
  free (new_prefix);
}

/* How to create internal_info_help_node.  HELP_IS_ONLY_WINDOW_P says
   whether we're going to end up in a second (or more) window of our
   own, or whether there's only one window and we're going to usurp it.
   This determines how to quit the help window.  Maybe we should just
   make q do the right thing in both cases.  */

static void
create_internal_info_help_node (help_is_only_window_p)
     int help_is_only_window_p;
{
  register int i;
  NODE *node;
  char *contents = NULL;
  char *exec_keys;

#ifndef HELP_NODE_GETS_REGENERATED
  if (internal_info_help_node_contents)
    contents = internal_info_help_node_contents;
#endif /* !HELP_NODE_GETS_REGENERATED */

  if (!contents)
    {
      int printed_one_mx = 0;

      initialize_message_buffer ();

      for (i = 0; info_internal_help_text[i]; i++)
        {
#ifdef INFOKEY
          printf_to_message_buffer (replace_in_documentation (
           _(info_internal_help_text[i]), help_is_only_window_p));
#else
          /* Don't translate blank lines, gettext outputs the po file
             header in that case.  We want a blank line.  */
          char *msg = *(info_internal_help_text[i])
                      ? _(info_internal_help_text[i])
                      : info_internal_help_text[i];
          char *key = info_help_keys_text[i][vi_keys_p];

          /* If we have only one window (because the window size was too
             small to split it), CTRL-x 0 doesn't work to `quit' help.  */
          if (STREQ (key, "CTRL-x 0") && help_is_only_window_p)
            key = "l";

          printf_to_message_buffer (msg, key);
#endif /* !INFOKEY */
        }

      printf_to_message_buffer ("---------------------\n\n");
      printf_to_message_buffer (_("The current search path is:\n"));
      printf_to_message_buffer ("  %s\n", infopath);
      printf_to_message_buffer ("---------------------\n\n");
      printf_to_message_buffer (_("Commands available in Info windows:\n\n"));
      dump_map_to_message_buffer ("", info_keymap);
      printf_to_message_buffer ("---------------------\n\n");
      printf_to_message_buffer (_("Commands available in the echo area:\n\n"));
      dump_map_to_message_buffer ("", echo_area_keymap);

#if defined (NAMED_FUNCTIONS)
      /* Get a list of commands which have no keystroke equivs. */
      exec_keys = where_is (info_keymap, InfoCmd(info_execute_command));
      if (exec_keys)
        exec_keys = xstrdup (exec_keys);
      for (i = 0; function_doc_array[i].func; i++)
        {
          InfoCommand *cmd = DocInfoCmd(&function_doc_array[i]);

          if (InfoFunction(cmd) != info_do_lowercase_version
              && !where_is_internal (info_keymap, cmd)
              && !where_is_internal (echo_area_keymap, cmd))
            {
              if (!printed_one_mx)
                {
                  printf_to_message_buffer ("---------------------\n\n");
                  if (exec_keys && exec_keys[0])
                      printf_to_message_buffer
                        (_("The following commands can only be invoked via %s:\n\n"), exec_keys);
                  else
                      printf_to_message_buffer
                        (_("The following commands cannot be invoked at all:\n\n"));
                  printed_one_mx = 1;
                }

              printf_to_message_buffer
                ("%s %s\n     %s\n",
                 exec_keys,
                 function_doc_array[i].func_name,
                 replace_in_documentation (strlen (function_doc_array[i].doc)
                                           ? _(function_doc_array[i].doc)
                                           : "")
                );

            }
        }

      if (printed_one_mx)
        printf_to_message_buffer ("\n");

      maybe_free (exec_keys);
#endif /* NAMED_FUNCTIONS */

      printf_to_message_buffer
        ("%s", replace_in_documentation
         (_("--- Use `\\[history-node]' or `\\[kill-node]' to exit ---\n")));
      node = message_buffer_to_node ();
      internal_info_help_node_contents = node->contents;
    }
  else
    {
      /* We already had the right contents, so simply use them. */
      node = build_message_node ("", 0, 0);
      free (node->contents);
      node->contents = contents;
      node->nodelen = 1 + strlen (contents);
    }

  internal_info_help_node = node;

  /* Do not GC this node's contents.  It never changes, and we never need
     to delete it once it is made.  If you change some things (such as
     placing information about dynamic variables in the help text) then
     you will need to allow the contents to be gc'd, and you will have to
     arrange to always regenerate the help node. */
#if defined (HELP_NODE_GETS_REGENERATED)
  add_gcable_pointer (internal_info_help_node->contents);
#endif

  name_internal_node (internal_info_help_node, info_help_nodename);

  /* Even though this is an internal node, we don't want the window
     system to treat it specially.  So we turn off the internalness
     of it here. */
  internal_info_help_node->flags &= ~N_IsInternal;
}

/* Return a window which is the window showing help in this Info. */

/* If the eligible window's height is >= this, split it to make the help
   window.  Otherwise display the help window in the current window.  */
#define HELP_SPLIT_SIZE 24

static WINDOW *
info_find_or_create_help_window ()
{
  int help_is_only_window_p;
  WINDOW *eligible = NULL;
  WINDOW *help_window = get_window_of_node (internal_info_help_node);

  /* If we couldn't find the help window, then make it. */
  if (!help_window)
    {
      WINDOW *window;
      int max = 0;

      for (window = windows; window; window = window->next)
        {
          if (window->height > max)
            {
              max = window->height;
              eligible = window;
            }
        }

      if (!eligible)
        return NULL;
    }
#ifndef HELP_NODE_GETS_REGENERATED
  else
    /* help window is static, just return it.  */
    return help_window;
#endif /* not HELP_NODE_GETS_REGENERATED */

  /* Make sure that we have a node containing the help text.  The
     argument is false if help will be the only window (so l must be used
     to quit help), true if help will be one of several visible windows
     (so CTRL-x 0 must be used to quit help).  */
  help_is_only_window_p
     = ((help_window && !windows->next)
        || !help_window && eligible->height < HELP_SPLIT_SIZE);
  create_internal_info_help_node (help_is_only_window_p);

  /* Either use the existing window to display the help node, or create
     a new window if there was no existing help window. */
  if (!help_window)
    { /* Split the largest window into 2 windows, and show the help text
         in that window. */
      if (eligible->height >= HELP_SPLIT_SIZE)
        {
          active_window = eligible;
          help_window = window_make_window (internal_info_help_node);
        }
      else
        {
          set_remembered_pagetop_and_point (active_window);
          window_set_node_of_window (active_window, internal_info_help_node);
          help_window = active_window;
        }
    }
  else
    { /* Case where help node always gets regenerated, and we have an
         existing window in which to place the node. */
      if (active_window != help_window)
        {
          set_remembered_pagetop_and_point (active_window);
          active_window = help_window;
        }
      window_set_node_of_window (active_window, internal_info_help_node);
    }
  remember_window_and_node (help_window, help_window->node);
  return help_window;
}

/* Create or move to the help window. */
DECLARE_INFO_COMMAND (info_get_help_window, _("Display help message"))
{
  WINDOW *help_window;

  help_window = info_find_or_create_help_window ();
  if (help_window)
    {
      active_window = help_window;
      active_window->flags |= W_UpdateWindow;
    }
  else
    {
      info_error (msg_cant_make_help);
    }
}

/* Show the Info help node.  This means that the "info" file is installed
   where it can easily be found on your system. */
DECLARE_INFO_COMMAND (info_get_info_help_node, _("Visit Info node `(info)Help'"))
{
  NODE *node;
  char *nodename;

  /* If there is a window on the screen showing the node "(info)Help" or
     the node "(info)Help-Small-Screen", simply select that window. */
  {
    WINDOW *win;

    for (win = windows; win; win = win->next)
      {
        if (win->node && win->node->filename &&
            (strcasecmp
             (filename_non_directory (win->node->filename), "info") == 0) &&
            ((strcmp (win->node->nodename, "Help") == 0) ||
             (strcmp (win->node->nodename, "Help-Small-Screen") == 0)))
          {
            active_window = win;
            return;
          }
      }
  }

  /* If the current window is small, show the small screen help. */
  if (active_window->height < 24)
    nodename = "Help-Small-Screen";
  else
    nodename = "Help";

  /* Try to get the info file for Info. */
  node = info_get_node ("Info", nodename);

  if (!node)
    {
      if (info_recent_file_error)
        info_error (info_recent_file_error);
      else
        info_error (msg_cant_file_node, "Info", nodename);
    }
  else
    {
      /* If the current window is very large (greater than 45 lines),
         then split it and show the help node in another window.
         Otherwise, use the current window. */

      if (active_window->height > 45)
        active_window = window_make_window (node);
      else
        {
          set_remembered_pagetop_and_point (active_window);
          window_set_node_of_window (active_window, node);
        }

      remember_window_and_node (active_window, node);
    }
}

/* **************************************************************** */
/*                                                                  */
/*                   Groveling Info Keymaps and Docs                */
/*                                                                  */
/* **************************************************************** */

/* Return the documentation associated with the Info command FUNCTION. */
char *
function_documentation (cmd)
     InfoCommand *cmd;
{
  char *doc;

#if defined (INFOKEY)

  doc = cmd->doc;

#else /* !INFOKEY */

  register int i;

  for (i = 0; function_doc_array[i].func; i++)
    if (InfoFunction(cmd) == function_doc_array[i].func)
      break;

  doc = function_doc_array[i].func ? function_doc_array[i].doc : "";

#endif /* !INFOKEY */

  return replace_in_documentation ((strlen (doc) == 0) ? doc : _(doc));
}

#if defined (NAMED_FUNCTIONS)
/* Return the user-visible name of the function associated with the
   Info command FUNCTION. */
char *
function_name (cmd)
     InfoCommand *cmd;
{
#if defined (INFOKEY)

  return cmd->func_name;

#else /* !INFOKEY */

  register int i;

  for (i = 0; function_doc_array[i].func; i++)
    if (InfoFunction(cmd) == function_doc_array[i].func)
      break;

  return (function_doc_array[i].func_name);

#endif /* !INFOKEY */
}

/* Return a pointer to the info command for function NAME. */
InfoCommand *
named_function (name)
     char *name;
{
  register int i;

  for (i = 0; function_doc_array[i].func; i++)
    if (strcmp (function_doc_array[i].func_name, name) == 0)
      break;

  return (DocInfoCmd(&function_doc_array[i]));
}
#endif /* NAMED_FUNCTIONS */

/* Return the documentation associated with KEY in MAP. */
char *
key_documentation (key, map)
     char key;
     Keymap map;
{
  InfoCommand *function = map[key].function;

  if (function)
    return (function_documentation (function));
  else
    return ((char *)NULL);
}

DECLARE_INFO_COMMAND (describe_key, _("Print documentation for KEY"))
{
  char keys[50];
  unsigned char keystroke;
  char *k = keys;
  Keymap map;

  *k = '\0';
  map = window->keymap;

  for (;;)
    {
      message_in_echo_area (_("Describe key: %s"), pretty_keyseq (keys));
      keystroke = info_get_input_char ();
      unmessage_in_echo_area ();

#if !defined (INFOKEY)
      if (Meta_p (keystroke))
        {
          if (map[ESC].type != ISKMAP)
            {
              window_message_in_echo_area
              (_("ESC %s is undefined."), pretty_keyname (UnMeta (keystroke)));
              return;
            }

          *k++ = '\e';
          keystroke = UnMeta (keystroke);
          map = (Keymap)map[ESC].function;
        }
#endif /* !INFOKEY */

      /* Add the KEYSTROKE to our list. */
      *k++ = keystroke;
      *k = '\0';

      if (map[keystroke].function == (InfoCommand *)NULL)
        {
          message_in_echo_area (_("%s is undefined."), pretty_keyseq (keys));
          return;
        }
      else if (map[keystroke].type == ISKMAP)
        {
          map = (Keymap)map[keystroke].function;
          continue;
        }
      else
        {
          char *keyname, *message, *fundoc, *funname = "";

#if defined (INFOKEY)
          /* If the key is bound to do-lowercase-version, but its
             lower-case variant is undefined, say that this key is
             also undefined.  This is especially important for unbound
             edit keys that emit an escape sequence: it's terribly
             confusing to see a message "Home (do-lowercase-version)"
             or some such when Home is unbound.  */
          if (InfoFunction(map[keystroke].function) == info_do_lowercase_version)
            {
              unsigned char lowerkey = Meta_p(keystroke)
                                       ? Meta (tolower (UnMeta (keystroke)))
                                       : tolower (keystroke);

              if (map[lowerkey].function == (InfoCommand *)NULL)
                {
                  message_in_echo_area (_("%s is undefined."),
                                        pretty_keyseq (keys));
                  return;
                }
            }
#endif

          keyname = pretty_keyseq (keys);

#if defined (NAMED_FUNCTIONS)
          funname = function_name (map[keystroke].function);
#endif /* NAMED_FUNCTIONS */

          fundoc = function_documentation (map[keystroke].function);

          message = (char *)xmalloc
            (10 + strlen (keyname) + strlen (fundoc) + strlen (funname));

#if defined (NAMED_FUNCTIONS)
          sprintf (message, "%s (%s): %s.", keyname, funname, fundoc);
#else
          sprintf (message, _("%s is defined to %s."), keyname, fundoc);
#endif /* !NAMED_FUNCTIONS */

          window_message_in_echo_area ("%s", message);
          free (message);
          break;
        }
    }
}

/* Return the pretty printable name of a single character. */
char *
pretty_keyname (key)
     unsigned char key;
{
  static char rep_buffer[30];
  char *rep;

  if (Meta_p (key))
    {
      char temp[20];

      rep = pretty_keyname (UnMeta (key));

#if defined (INFOKEY)
      sprintf (temp, "M-%s", rep);
#else /* !INFOKEY */
      sprintf (temp, "ESC %s", rep);
#endif /* !INFOKEY */
      strcpy (rep_buffer, temp);
      rep = rep_buffer;
    }
  else if (Control_p (key))
    {
      switch (key)
        {
        case '\n': rep = "LFD"; break;
        case '\t': rep = "TAB"; break;
        case '\r': rep = "RET"; break;
        case ESC:  rep = "ESC"; break;

        default:
          sprintf (rep_buffer, "C-%c", UnControl (key));
          rep = rep_buffer;
        }
    }
  else
    {
      switch (key)
        {
        case ' ': rep = "SPC"; break;
        case DEL: rep = "DEL"; break;
        default:
          rep_buffer[0] = key;
          rep_buffer[1] = '\0';
          rep = rep_buffer;
        }
    }
  return (rep);
}

/* Return the pretty printable string which represents KEYSEQ. */

static void pretty_keyseq_internal ();

char *
pretty_keyseq (keyseq)
     char *keyseq;
{
  static char keyseq_rep[200];

  keyseq_rep[0] = '\0';
  if (*keyseq)
    pretty_keyseq_internal (keyseq, keyseq_rep);
  return (keyseq_rep);
}

static void
pretty_keyseq_internal (keyseq, rep)
     char *keyseq, *rep;
{
  if (term_kP && strncmp(keyseq, term_kP, strlen(term_kP)) == 0)
    {
      strcpy(rep, "PgUp");
      keyseq += strlen(term_kP);
    }
  else if (term_kN && strncmp(keyseq, term_kN, strlen(term_kN)) == 0)
    {
      strcpy(rep, "PgDn");
      keyseq += strlen(term_kN);
    }
#if defined(INFOKEY)
  else if (term_kh && strncmp(keyseq, term_kh, strlen(term_kh)) == 0)
    {
      strcpy(rep, "Home");
      keyseq += strlen(term_kh);
    }
  else if (term_ke && strncmp(keyseq, term_ke, strlen(term_ke)) == 0)
    {
      strcpy(rep, "End");
      keyseq += strlen(term_ke);
    }
  else if (term_ki && strncmp(keyseq, term_ki, strlen(term_ki)) == 0)
    {
      strcpy(rep, "INS");
      keyseq += strlen(term_ki);
    }
  else if (term_kx && strncmp(keyseq, term_kx, strlen(term_kx)) == 0)
    {
      strcpy(rep, "DEL");
      keyseq += strlen(term_kx);
    }
#endif /* INFOKEY */
  else if (term_ku && strncmp(keyseq, term_ku, strlen(term_ku)) == 0)
    {
      strcpy(rep, "Up");
      keyseq += strlen(term_ku);
    }
  else if (term_kd && strncmp(keyseq, term_kd, strlen(term_kd)) == 0)
    {
      strcpy(rep, "Down");
      keyseq += strlen(term_kd);
    }
  else if (term_kl && strncmp(keyseq, term_kl, strlen(term_kl)) == 0)
    {
      strcpy(rep, "Left");
      keyseq += strlen(term_kl);
    }
  else if (term_kr && strncmp(keyseq, term_kr, strlen(term_kr)) == 0)
    {
      strcpy(rep, "Right");
      keyseq += strlen(term_kr);
    }
  else
    {
      strcpy (rep, pretty_keyname (keyseq[0]));
      keyseq++;
    }
  if (*keyseq)
    {
      strcat (rep, " ");
      pretty_keyseq_internal (keyseq, rep + strlen(rep));
    }
}

/* Return a pointer to the last character in s that is found in f. */
static char *
strrpbrk (s, f)
     const char *s, *f;
{
  register const char *e = s + strlen(s);
  register const char *t;

  while (e-- != s)
    {
      for (t = f; *t; t++)
        if (*e == *t)
          return (char *)e;
    }
  return NULL;
}

/* Replace the names of functions with the key that invokes them. */
char *
replace_in_documentation (string, help_is_only_window_p)
     char *string;
     int help_is_only_window_p;
{
  unsigned reslen = strlen (string);
  register int i, start, next;
  static char *result = (char *)NULL;

  maybe_free (result);
  result = (char *)xmalloc (1 + reslen);

  i = next = start = 0;

  /* Skip to the beginning of a replaceable function. */
  for (i = start; string[i]; i++)
    {
      int j = i + 1;

      /* Is this the start of a replaceable function name? */
      if (string[i] == '\\')
        {
          char *fmt = NULL;
          unsigned min = 0;
          unsigned max = 0;

          if(string[j] == '%')
            {
              if (string[++j] == '-')
                j++;
              if (isdigit(string[j]))
                {
                  min = atoi(string + j);
                  while (isdigit(string[j]))
                    j++;
                  if (string[j] == '.' && isdigit(string[j + 1]))
                    {
                      j += 1;
                      max = atoi(string + j);
                      while (isdigit(string[j]))
                        j++;
                    }
                  fmt = (char *)xmalloc (j - i + 2);
                  strncpy (fmt, string + i + 1, j - i);
                  fmt[j - i - 1] = 's';
                  fmt[j - i] = '\0';
                }
              else
                j = i + 1;
            }
          if (string[j] == '[')
            {
              unsigned arg = 0;
              char *argstr = NULL;
              char *rep_name, *fun_name, *rep;
              InfoCommand *command;
              char *repstr = NULL;
              unsigned replen;

              /* Copy in the old text. */
              strncpy (result + next, string + start, i - start);
              next += (i - start);
              start = j + 1;

              /* Look for an optional numeric arg. */
              i = start;
              if (isdigit(string[i])
                  || (string[i] == '-' && isdigit(string[i + 1])) )
                {
                  arg = atoi(string + i);
                  if (string[i] == '-')
                    i++;
                  while (isdigit(string[i]))
                    i++;
                }
              start = i;

              /* Move to the end of the function name. */
              for (i = start; string[i] && (string[i] != ']'); i++);

              rep_name = (char *)xmalloc (1 + i - start);
              strncpy (rep_name, string + start, i - start);
              rep_name[i - start] = '\0';

            /* If we have only one window (because the window size was too
               small to split it), we have to quit help by going back one
               noew in the history list, not deleting the window.  */
              if (strcmp (rep_name, "quit-help") == 0)
                fun_name = help_is_only_window_p ? "history-node"
                                                 : "delete-window";
              else
                fun_name = rep_name;

              /* Find a key which invokes this function in the info_keymap. */
              command = named_function (fun_name);

              free (rep_name);

              /* If the internal documentation string fails, there is a
                 serious problem with the associated command's documentation.
                 We croak so that it can be fixed immediately. */
              if (!command)
                abort ();

              if (arg)
                {
                  char *argrep, *p;

                  argrep = where_is (info_keymap, InfoCmd(info_add_digit_to_numeric_arg));
                  p = argrep ? strrpbrk (argrep, "0123456789-") : NULL;
                  if (p)
                    {
                      argstr = (char *)xmalloc (p - argrep + 21);
                      strncpy (argstr, argrep, p - argrep);
                      sprintf (argstr + (p - argrep), "%d", arg);
                    }
                  else
                    command = NULL;
                }
              rep = command ? where_is (info_keymap, command) : NULL;
              if (!rep)
                rep = "N/A";
              replen = (argstr ? strlen (argstr) : 0) + strlen (rep) + 1;
              repstr = (char *)xmalloc (replen);
              repstr[0] = '\0';
              if (argstr)
                {
                  strcat(repstr, argstr);
                  strcat(repstr, " ");
                  free (argstr);
                }
              strcat(repstr, rep);

              if (fmt)
                {
                  if (replen > max)
                    replen = max;
                  if (replen < min)
                    replen = min;
                }
              if (next + replen > reslen)
                {
                  reslen = next + replen + 1;
                  result = (char *)xrealloc (result, reslen + 1);
                }

              if (fmt)
                  sprintf (result + next, fmt, repstr);
              else
                  strcpy (result + next, repstr);

              next = strlen (result);
              free (repstr);

              start = i;
              if (string[i])
                start++;
            }

          maybe_free (fmt);
        }
    }
  strcpy (result + next, string + start);
  return (result);
}

/* Return a string of characters which could be typed from the keymap
   MAP to invoke FUNCTION. */
static char *where_is_rep = (char *)NULL;
static int where_is_rep_index = 0;
static int where_is_rep_size = 0;

char *
where_is (map, cmd)
     Keymap map;
     InfoCommand *cmd;
{
  char *rep;

  if (!where_is_rep_size)
    where_is_rep = (char *)xmalloc (where_is_rep_size = 100);
  where_is_rep_index = 0;

  rep = where_is_internal (map, cmd);

  /* If it couldn't be found, return "M-x Foo" (or equivalent). */
  if (!rep)
    {
      char *name;

      name = function_name (cmd);
      if (!name)
        return NULL; /* no such function */

      rep = where_is_internal (map, InfoCmd(info_execute_command));
      if (!rep)
        return ""; /* function exists but can't be got to by user */

      sprintf (where_is_rep, "%s %s", rep, name);

      rep = where_is_rep;
    }
  return (rep);
}

/* Return the printed rep of the keystrokes that invoke FUNCTION,
   as found in MAP, or NULL. */
static char *
where_is_internal (map, cmd)
     Keymap map;
     InfoCommand *cmd;
{
#if defined(INFOKEY)

  register FUNCTION_KEYSEQ *k;

  for (k = cmd->keys; k; k = k->next)
    if (k->map == map)
      return pretty_keyseq (k->keyseq);

  return NULL;

#else /* !INFOKEY */
  /* There is a bug in that create_internal_info_help_node calls
     where_is_internal without setting where_is_rep_index to zero.  This
     was found by Mandrake and reported by Thierry Vignaud
     <tvignaud@mandrakesoft.com> around April 24, 2002.

     I think the best fix is to make where_is_rep_index another
     parameter to this recursively-called function, instead of a static
     variable.  But this [!INFOKEY] branch of the code is not enabled
     any more, so let's just skip the whole thing.  --karl, 28sep02.  */
  register int i;

  /* If the function is directly invokable in MAP, return the representation
     of that keystroke. */
  for (i = 0; i < 256; i++)
    if ((map[i].type == ISFUNC) && map[i].function == cmd)
      {
        sprintf (where_is_rep + where_is_rep_index, "%s", pretty_keyname (i));
        return (where_is_rep);
      }

  /* Okay, search subsequent maps for this function. */
  for (i = 0; i < 256; i++)
    {
      if (map[i].type == ISKMAP)
        {
          int saved_index = where_is_rep_index;
          char *rep;

          sprintf (where_is_rep + where_is_rep_index, "%s ",
                   pretty_keyname (i));

          where_is_rep_index = strlen (where_is_rep);
          rep = where_is_internal ((Keymap)map[i].function, cmd);

          if (rep)
            return (where_is_rep);

          where_is_rep_index = saved_index;
        }
    }

  return NULL;

#endif /* INFOKEY */
}

extern char *read_function_name ();

DECLARE_INFO_COMMAND (info_where_is,
   _("Show what to type to execute a given command"))
{
  char *command_name;

  command_name = read_function_name (_("Where is command: "), window);

  if (!command_name)
    {
      info_abort_key (active_window, count, key);
      return;
    }

  if (*command_name)
    {
      InfoCommand *command;

      command = named_function (command_name);

      if (command)
        {
          char *location;

          location = where_is (active_window->keymap, command);

          if (!location || !location[0])
            {
              info_error (_("`%s' is not on any keys"), command_name);
            }
          else
            {
              if (strstr (location, function_name (command)))
                window_message_in_echo_area
                  (_("%s can only be invoked via %s."), command_name, location);
              else
                window_message_in_echo_area
                  (_("%s can be invoked via %s."), command_name, location);
            }
        }
      else
        info_error (_("There is no function named `%s'"), command_name);
    }

  free (command_name);
}
