/* session.c -- user windowing interface to Info.
   $Id: session.c,v 1.8 2003/03/22 17:41:16 karl Exp $

   Copyright (C) 1993, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

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
#include <sys/ioctl.h>

#if defined (HAVE_SYS_TIME_H)
#  include <sys/time.h>
#  define HAVE_STRUCT_TIMEVAL
#endif /* HAVE_SYS_TIME_H */

#if defined (HANDLE_MAN_PAGES)
#  include "man.h"
#endif

#ifdef M_XENIX
/* SCO 3.2v5.0.2 defines but does not correctly declare strncasecmp.
   Since we use it as a symbol, have to get it right.  --gildea, 1jul99.  */
extern int strncasecmp (const char *, const char *, size_t);
#endif

static void info_clear_pending_input (), info_set_pending_input ();
static void info_handle_pointer ();

/* **************************************************************** */
/*                                                                  */
/*                   Running an Info Session                        */
/*                                                                  */
/* **************************************************************** */

/* The place that we are reading input from. */
static FILE *info_input_stream = NULL;

/* The last executed command. */
VFunction *info_last_executed_command = NULL;

/* Becomes non-zero when 'q' is typed to an Info window. */
int quit_info_immediately = 0;

/* Array of structures describing for each window which nodes have been
   visited in that window. */
INFO_WINDOW **info_windows = NULL;

/* Where to add the next window, if we need to add one. */
static int info_windows_index = 0;

/* Number of slots allocated to `info_windows'. */
static int info_windows_slots = 0;

void remember_window_and_node (), forget_window_and_nodes ();
void initialize_info_session (), info_session ();
void display_startup_message_and_start ();

/* Begin an info session finding the nodes specified by FILENAME and NODENAMES.
   For each loaded node, create a new window.  Always split the largest of the
   available windows. */
void
begin_multiple_window_info_session (filename, nodenames)
     char *filename;
     char **nodenames;
{
  register int i;
  WINDOW *window = (WINDOW *)NULL;

  for (i = 0; nodenames[i]; i++)
    {
      NODE *node;

      node = info_get_node (filename, nodenames[i]);

      if (!node)
        break;

      /* If this is the first node, initialize the info session. */
      if (!window)
        {
          initialize_info_session (node, 1);
          window = active_window;
        }
      else
        {
          /* Find the largest window in WINDOWS, and make that be the active
             one.  Then split it and add our window and node to the list
             of remembered windows and nodes.  Then tile the windows. */
          WINDOW *win, *largest = NULL;
          int max_height = 0;

          for (win = windows; win; win = win->next)
            if (win->height > max_height)
              {
                max_height = win->height;
                largest = win;
              }

          if (!largest)
            {
              display_update_display (windows);
              info_error (msg_cant_find_window);
              info_session ();
              xexit (0);
            }

          active_window = largest;
          window = window_make_window (node);
          if (window)
            {
              window_tile_windows (TILE_INTERNALS);
              remember_window_and_node (window, node);
            }
          else
            {
              display_update_display (windows);
              info_error (msg_win_too_small);
              info_session ();
              xexit (0);
            }
        }
    }
  display_startup_message_and_start ();
}

/* Start an info session with INITIAL_NODE, and an error message in the echo
   area made from FORMAT and ARG. */
void
begin_info_session_with_error (initial_node, format, arg1, arg2)
     NODE *initial_node;
     char *format;
     void *arg1;
     void *arg2;
{
  initialize_info_session (initial_node, 1);
  info_error (format, arg1, arg2);
  info_session ();
}

/* Start an info session with INITIAL_NODE. */
void
begin_info_session (initial_node)
     NODE *initial_node;
{
  initialize_info_session (initial_node, 1);
  display_startup_message_and_start ();
}

void
display_startup_message_and_start ()
{
  char *format;

  format = replace_in_documentation
    (_("Welcome to Info version %s. Type \\[get-help-window] for help, \\[menu-item] for menu item."));

  window_message_in_echo_area (format, VERSION);
  info_session ();
}

/* Run an info session with an already initialized window and node. */
void
info_session ()
{
  display_update_display (windows);
  info_last_executed_command = NULL;
  info_read_and_dispatch ();
  /* On program exit, leave the cursor at the bottom of the window, and
     restore the terminal I/O. */
  terminal_goto_xy (0, screenheight - 1);
  terminal_clear_to_eol ();
  fflush (stdout);
  terminal_unprep_terminal ();
  close_dribble_file ();
}

/* Here is a window-location dependent event loop.  Called from the
   functions info_session (), and from read_xxx_in_echo_area (). */
void
info_read_and_dispatch ()
{
  unsigned char key;
  int done;
  done = 0;

  while (!done && !quit_info_immediately)
    {
      int lk;

      /* If we haven't just gone up or down a line, there is no
         goal column for this window. */
      if ((info_last_executed_command != info_next_line) &&
          (info_last_executed_command != info_prev_line))
        active_window->goal_column = -1;

      if (echo_area_is_active)
        {
          lk = echo_area_last_command_was_kill;
          echo_area_prep_read ();
        }

      if (!info_any_buffered_input_p ())
        display_update_display (windows);

      display_cursor_at_point (active_window);
      info_initialize_numeric_arg ();

      initialize_keyseq ();
      key = info_get_input_char ();

      /* No errors yet.  We just read a character, that's all.  Only clear
         the echo_area if it is not currently active. */
      if (!echo_area_is_active)
        window_clear_echo_area ();

      info_error_was_printed = 0;

      /* Do the selected command. */
      info_dispatch_on_key (key, active_window->keymap);

      if (echo_area_is_active)
        {
          /* Echo area commands that do killing increment the value of
             ECHO_AREA_LAST_COMMAND_WAS_KILL.  Thus, if there is no
             change in the value of this variable, the last command
             executed was not a kill command. */
          if (lk == echo_area_last_command_was_kill)
            echo_area_last_command_was_kill = 0;

          if (ea_last_executed_command == ea_newline ||
              info_aborted_echo_area)
            {
              ea_last_executed_command = (VFunction *)NULL;
              done = 1;
            }

          if (info_last_executed_command == info_quit)
            quit_info_immediately = 1;
        }
      else if (info_last_executed_command == info_quit)
        done = 1;
    }
}

/* Found in signals.c */
extern void initialize_info_signal_handler ();

/* Initialize the first info session by starting the terminal, window,
   and display systems.  If CLEAR_SCREEN is 0, don't clear the screen.  */
void
initialize_info_session (node, clear_screen)
     NODE *node;
     int clear_screen;
{
  char *term_name = getenv ("TERM");
  terminal_initialize_terminal (term_name);

  if (terminal_is_dumb_p)
    {
      if (!term_name)
        term_name = "dumb";

      info_error (msg_term_too_dumb, term_name);
      xexit (1);
    }

  if (clear_screen)
    {
      terminal_prep_terminal ();
      terminal_clear_screen ();
    }

  initialize_info_keymaps ();
  window_initialize_windows (screenwidth, screenheight);
  initialize_info_signal_handler ();
  display_initialize_display (screenwidth, screenheight);
  info_set_node_of_window (0, active_window, node);

  /* Tell the window system how to notify us when a window needs to be
     asynchronously deleted (e.g., user resizes window very small). */
  window_deletion_notifier = forget_window_and_nodes;

  /* If input has not been redirected yet, make it come from unbuffered
     standard input. */
  if (!info_input_stream)
    {
      setbuf (stdin, NULL);
      info_input_stream = stdin;
    }

  info_windows_initialized_p = 1;
}

/* Tell Info that input is coming from the file FILENAME. */
void
info_set_input_from_file (filename)
     char *filename;
{
  FILE *stream;

  /* Input may include binary characters.  */
  stream = fopen (filename, FOPEN_RBIN);

  if (!stream)
    return;

  if ((info_input_stream != (FILE *)NULL) &&
      (info_input_stream != stdin))
    fclose (info_input_stream);

  info_input_stream = stream;

  if (stream != stdin)
    display_inhibited = 1;
}

/* Return the INFO_WINDOW containing WINDOW, or NULL if there isn't one. */
static INFO_WINDOW *
get_info_window_of_window (window)
     WINDOW *window;
{
  register int i;
  INFO_WINDOW *info_win = (INFO_WINDOW *)NULL;

  for (i = 0; info_windows && (info_win = info_windows[i]); i++)
    if (info_win->window == window)
      break;

  return (info_win);
}

/* Reset the remembered pagetop and point of WINDOW to WINDOW's current
   values if the window and node are the same as the current one being
   displayed. */
void
set_remembered_pagetop_and_point (window)
     WINDOW *window;
{
  INFO_WINDOW *info_win;

  info_win = get_info_window_of_window (window);

  if (!info_win)
    return;

  if (info_win->nodes_index &&
      (info_win->nodes[info_win->current] == window->node))
    {
      info_win->pagetops[info_win->current] = window->pagetop;
      info_win->points[info_win->current] = window->point;
    }
}

void
remember_window_and_node (window, node)
     WINDOW *window;
     NODE *node;
{
  /* See if we already have this window in our list. */
  INFO_WINDOW *info_win = get_info_window_of_window (window);

  /* If the window wasn't already on our list, then make a new entry. */
  if (!info_win)
    {
      info_win = (INFO_WINDOW *)xmalloc (sizeof (INFO_WINDOW));
      info_win->window = window;
      info_win->nodes = (NODE **)NULL;
      info_win->pagetops = (int *)NULL;
      info_win->points = (long *)NULL;
      info_win->current = 0;
      info_win->nodes_index = 0;
      info_win->nodes_slots = 0;

      add_pointer_to_array (info_win, info_windows_index, info_windows,
                            info_windows_slots, 10, INFO_WINDOW *);
    }

  /* If this node, the current pagetop, and the current point are the
     same as the current saved node and pagetop, don't really add this to
     the list of history nodes.  This may happen only at the very
     beginning of the program, I'm not sure.  --karl  */
  if (info_win->nodes
      && info_win->current >= 0
      && info_win->nodes[info_win->current]->contents == node->contents
      && info_win->pagetops[info_win->current] == window->pagetop
      && info_win->points[info_win->current] == window->point)
  return;

  /* Remember this node, the currently displayed pagetop, and the current
     location of point in this window.  Because we are updating pagetops
     and points as well as nodes, it is more efficient to avoid the
     add_pointer_to_array macro here. */
  if (info_win->nodes_index + 2 >= info_win->nodes_slots)
    {
      info_win->nodes_slots += 20;
      info_win->nodes = (NODE **) xrealloc (info_win->nodes,
                                      info_win->nodes_slots * sizeof (NODE *));
      info_win->pagetops = (int *) xrealloc (info_win->pagetops,
                                      info_win->nodes_slots * sizeof (int));
      info_win->points = (long *) xrealloc (info_win->points,
                                      info_win->nodes_slots * sizeof (long));
    }

  info_win->nodes[info_win->nodes_index] = node;
  info_win->pagetops[info_win->nodes_index] = window->pagetop;
  info_win->points[info_win->nodes_index] = window->point;
  info_win->current = info_win->nodes_index++;
  info_win->nodes[info_win->nodes_index] = NULL;
  info_win->pagetops[info_win->nodes_index] = 0;
  info_win->points[info_win->nodes_index] = 0;
}

#define DEBUG_FORGET_WINDOW_AND_NODES
#if defined (DEBUG_FORGET_WINDOW_AND_NODES)
static void
consistency_check_info_windows ()
{
  register int i;

  for (i = 0; i < info_windows_index; i++)
    {
      WINDOW *win;

      for (win = windows; win; win = win->next)
        if (win == info_windows[i]->window)
          break;

      if (!win)
        abort ();
    }
}
#endif /* DEBUG_FORGET_WINDOW_AND_NODES */

/* Remove WINDOW and its associated list of nodes from INFO_WINDOWS. */
void
forget_window_and_nodes (window)
     WINDOW *window;
{
  register int i;
  INFO_WINDOW *info_win = (INFO_WINDOW *)NULL;

  for (i = 0; info_windows && (info_win = info_windows[i]); i++)
    if (info_win->window == window)
      break;

  /* If we found the window to forget, then do so. */
  if (info_win)
    {
      while (i < info_windows_index)
        {
          info_windows[i] = info_windows[i + 1];
          i++;
        }

      info_windows_index--;
      info_windows[info_windows_index] = (INFO_WINDOW *)NULL;

      if (info_win->nodes)
        {
          /* Free the node structures which held onto internal node contents
             here.  This doesn't free the contents; we have a garbage collector
             which does that. */
          for (i = 0; info_win->nodes[i]; i++)
            if (internal_info_node_p (info_win->nodes[i]))
              free (info_win->nodes[i]);
          free (info_win->nodes);

          maybe_free (info_win->pagetops);
          maybe_free (info_win->points);
        }

      free (info_win);
    }
#if defined (DEBUG_FORGET_WINDOW_AND_NODES)
  consistency_check_info_windows ();
#endif /* DEBUG_FORGET_WINDOW_AND_NODES */
}

/* Set WINDOW to show NODE.  Remember the new window in our list of Info
   windows.  If we are doing automatic footnote display, also try to display
   the footnotes for this window.  If REMEMBER is nonzero, first call
   set_remembered_pagetop_and_point.  */
void
info_set_node_of_window (remember, window, node)
     int remember;
     WINDOW *window;
     NODE *node;
{
  if (remember)
    set_remembered_pagetop_and_point (window);

  /* Put this node into the window. */
  window_set_node_of_window (window, node);

  /* Remember this node and window in our list of info windows. */
  remember_window_and_node (window, node);

  /* If doing auto-footnote display/undisplay, show the footnotes belonging
     to this window's node. */
  if (auto_footnotes_p)
    info_get_or_remove_footnotes (window);
}


/* **************************************************************** */
/*                                                                  */
/*                     Info Movement Commands                       */
/*                                                                  */
/* **************************************************************** */

/* Change the pagetop of WINDOW to DESIRED_TOP, perhaps scrolling the screen
   to do so. */
void
set_window_pagetop (window, desired_top)
     WINDOW *window;
     int desired_top;
{
  int point_line, old_pagetop;

  if (desired_top < 0)
    desired_top = 0;
  else if (desired_top > window->line_count)
    desired_top = window->line_count - 1;

  if (window->pagetop == desired_top)
    return;

  old_pagetop = window->pagetop;
  window->pagetop = desired_top;

  /* Make sure that point appears in this window. */
  point_line = window_line_of_point (window);
  if ((point_line < window->pagetop) ||
      ((point_line - window->pagetop) > window->height - 1))
    window->point =
      window->line_starts[window->pagetop] - window->node->contents;

  window->flags |= W_UpdateWindow;

  /* Find out which direction to scroll, and scroll the window in that
     direction.  Do this only if there would be a savings in redisplay
     time.  This is true if the amount to scroll is less than the height
     of the window, and if the number of lines scrolled would be greater
     than 10 % of the window's height. */
  if (old_pagetop < desired_top)
    {
      int start, end, amount;

      amount = desired_top - old_pagetop;

      if ((amount >= window->height) ||
          (((window->height - amount) * 10) < window->height))
        return;

      start = amount + window->first_row;
      end = window->height + window->first_row;

      display_scroll_display (start, end, -amount);
    }
  else
    {
      int start, end, amount;

      amount = old_pagetop - desired_top;

      if ((amount >= window->height) ||
          (((window->height - amount) * 10) < window->height))
        return;

      start = window->first_row;
      end = (window->first_row + window->height) - amount;
      display_scroll_display (start, end, amount);
    }
}

/* Immediately make WINDOW->point visible on the screen, and move the
   terminal cursor there. */
static void
info_show_point (window)
     WINDOW *window;
{
  int old_pagetop;

  old_pagetop = window->pagetop;
  window_adjust_pagetop (window);
  if (old_pagetop != window->pagetop)
    {
      int new_pagetop;

      new_pagetop = window->pagetop;
      window->pagetop = old_pagetop;
      set_window_pagetop (window, new_pagetop);
    }

  if (window->flags & W_UpdateWindow)
    display_update_one_window (window);

  display_cursor_at_point (window);
}

/* Move WINDOW->point from OLD line index to NEW line index. */
static void
move_to_new_line (old, new, window)
     int old, new;
     WINDOW *window;
{
  if (old == -1)
    {
      info_error (msg_cant_find_point);
    }
  else
    {
      int goal;

      if (new >= window->line_count || new < 0)
        return;

      goal = window_get_goal_column (window);
      window->goal_column = goal;

      window->point = window->line_starts[new] - window->node->contents;
      window->point += window_chars_to_goal (window->line_starts[new], goal);
      info_show_point (window);
    }
}

/* Move WINDOW's point down to the next line if possible. */
DECLARE_INFO_COMMAND (info_next_line, _("Move down to the next line"))
{
  int old_line, new_line;

  if (count < 0)
    info_prev_line (window, -count, key);
  else
    {
      old_line = window_line_of_point (window);
      new_line = old_line + count;
      move_to_new_line (old_line, new_line, window);
    }
}

/* Move WINDOW's point up to the previous line if possible. */
DECLARE_INFO_COMMAND (info_prev_line, _("Move up to the previous line"))
{
  int old_line, new_line;

  if (count < 0)
    info_next_line (window, -count, key);
  else
    {
      old_line = window_line_of_point (window);
      new_line = old_line - count;
      move_to_new_line (old_line, new_line, window);
    }
}

/* Move WINDOW's point to the end of the true line. */
DECLARE_INFO_COMMAND (info_end_of_line, _("Move to the end of the line"))
{
  register int point, len;
  register char *buffer;

  buffer = window->node->contents;
  len = window->node->nodelen;

  for (point = window->point;
       (point < len) && (buffer[point] != '\n');
       point++);

  if (point != window->point)
    {
      window->point = point;
      info_show_point (window);
    }
}

/* Move WINDOW's point to the beginning of the true line. */
DECLARE_INFO_COMMAND (info_beginning_of_line, _("Move to the start of the line"))
{
  register int point;
  register char *buffer;

  buffer = window->node->contents;
  point = window->point;

  for (; (point) && (buffer[point - 1] != '\n'); point--);

  /* If at a line start already, do nothing. */
  if (point != window->point)
    {
      window->point = point;
      info_show_point (window);
    }
}

/* Move point forward in the node. */
DECLARE_INFO_COMMAND (info_forward_char, _("Move forward a character"))
{
  if (count < 0)
    info_backward_char (window, -count, key);
  else
    {
      window->point += count;

      if (window->point >= window->node->nodelen)
        window->point = window->node->nodelen - 1;

      info_show_point (window);
    }
}

/* Move point backward in the node. */
DECLARE_INFO_COMMAND (info_backward_char, _("Move backward a character"))
{
  if (count < 0)
    info_forward_char (window, -count, key);
  else
    {
      window->point -= count;

      if (window->point < 0)
        window->point = 0;

      info_show_point (window);
    }
}

#define alphabetic(c) (islower (c) || isupper (c) || isdigit (c))

/* Move forward a word in this node. */
DECLARE_INFO_COMMAND (info_forward_word, _("Move forward a word"))
{
  long point;
  char *buffer;
  int end, c;

  if (count < 0)
    {
      info_backward_word (window, -count, key);
      return;
    }

  point = window->point;
  buffer = window->node->contents;
  end = window->node->nodelen;

  while (count)
    {
      if (point + 1 >= end)
        return;

      /* If we are not in a word, move forward until we are in one.
         Then, move forward until we hit a non-alphabetic character. */
      c = buffer[point];

      if (!alphabetic (c))
        {
          while (++point < end)
            {
              c = buffer[point];
              if (alphabetic (c))
                break;
            }
        }

      if (point >= end) return;

      while (++point < end)
        {
          c = buffer[point];
          if (!alphabetic (c))
            break;
        }
      --count;
    }
  window->point = point;
  info_show_point (window);
}

DECLARE_INFO_COMMAND (info_backward_word, _("Move backward a word"))
{
  long point;
  char *buffer;
  int c;

  if (count < 0)
    {
      info_forward_word (window, -count, key);
      return;
    }

  buffer = window->node->contents;
  point = window->point;

  while (count)
    {
      if (point == 0)
        break;

      /* Like info_forward_word (), except that we look at the
         characters just before point. */

      c = buffer[point - 1];

      if (!alphabetic (c))
        {
          while (--point)
            {
              c = buffer[point - 1];
              if (alphabetic (c))
                break;
            }
        }

      while (point)
        {
          c = buffer[point - 1];
          if (!alphabetic (c))
            break;
          else
            --point;
        }
      --count;
    }
  window->point = point;
  info_show_point (window);
}

/* Variable controlling the behaviour of default scrolling when you are
   already at the bottom of a node.  Possible values are defined in session.h.
   The meanings are:

   IS_Continuous        Try to get first menu item, or failing that, the
                        "Next:" pointer, or failing that, the "Up:" and
                        "Next:" of the up.
   IS_NextOnly          Try to get "Next:" menu item.
   IS_PageOnly          Simply give up at the bottom of a node. */

int info_scroll_behaviour = IS_Continuous;

/* Choices used by the completer when reading a value for the user-visible
   variable "scroll-behaviour". */
char *info_scroll_choices[] = {
  "Continuous", "Next Only", "Page Only", (char *)NULL
};

/* Default window sizes for scrolling commands.  */
int default_window_size = -1;	/* meaning 1 window-full */
int default_scroll_size = -1;	/* meaning half screen size */

#define INFO_LABEL_FOUND() \
  (info_parsed_nodename || (info_parsed_filename \
                            && !is_dir_name (info_parsed_filename)))

/* Move to 1st menu item, Next, Up/Next, or error in this window. */
static void
forward_move_node_structure (window, behaviour)
     WINDOW *window;
     int behaviour;
{
  switch (behaviour)
    {
    case IS_PageOnly:
      info_error (msg_at_node_bottom);
      break;

    case IS_NextOnly:
      info_next_label_of_node (window->node);
      if (!info_parsed_nodename && !info_parsed_filename)
        info_error (msg_no_pointer, _("Next"));
      else
        {
          window_message_in_echo_area (_("Following Next node..."));
          info_handle_pointer ("Next", window);
        }
      break;

    case IS_Continuous:
      {
        /* First things first.  If this node contains a menu, move down
           into the menu. */
        {
          REFERENCE **menu;

          menu = info_menu_of_node (window->node);

          if (menu)
            {
              info_free_references (menu);
              window_message_in_echo_area (_("Selecting first menu item..."));
              info_menu_digit (window, 1, '1');
              return;
            }
        }

        /* Okay, this node does not contain a menu.  If it contains a
           "Next:" pointer, use that. */
        info_next_label_of_node (window->node);
        if (INFO_LABEL_FOUND ())
          {
            window_message_in_echo_area (_("Selecting Next node..."));
            info_handle_pointer ("Next", window);
            return;
          }

        /* Okay, there wasn't a "Next:" for this node.  Move "Up:" until we
           can move "Next:".  If that isn't possible, complain that there
           are no more nodes. */
        {
          int up_counter, old_current;
          INFO_WINDOW *info_win;

          /* Remember the current node and location. */
          info_win = get_info_window_of_window (window);
          old_current = info_win->current;

          /* Back up through the "Up:" pointers until we have found a "Next:"
             that isn't the same as the first menu item found in that node. */
          up_counter = 0;
          while (!info_error_was_printed)
            {
              info_up_label_of_node (window->node);
              if (INFO_LABEL_FOUND ())
                {
                  info_handle_pointer ("Up", window);
                  if (info_error_was_printed)
                    continue;

                  up_counter++;

                  info_next_label_of_node (window->node);

                  /* If no "Next" pointer, keep backing up. */
                  if (!INFO_LABEL_FOUND ())
                    continue;

                  /* If this node's first menu item is the same as this node's
                     Next pointer, keep backing up. */
                  if (!info_parsed_filename)
                    {
                      REFERENCE **menu;
                      char *next_nodename;

                      /* Remember the name of the Next node, since reading
                         the menu can overwrite the contents of the
                         info_parsed_xxx strings. */
                      next_nodename = xstrdup (info_parsed_nodename);

                      menu = info_menu_of_node (window->node);
                      if (menu &&
                          (strcmp
                           (menu[0]->nodename, next_nodename) == 0))
                        {
                          info_free_references (menu);
                          free (next_nodename);
                          continue;
                        }
                      else
                        {
                          /* Restore the world to where it was before
                             reading the menu contents. */
                          info_free_references (menu);
                          free (next_nodename);
                          info_next_label_of_node (window->node);
                        }
                    }

                  /* This node has a "Next" pointer, and it is not the
                     same as the first menu item found in this node. */
                  window_message_in_echo_area
                    (_("Moving Up %d time(s), then Next."),
                     up_counter);

                  info_handle_pointer ("Next", window);
                  return;
                }
              else
                {
                  /* No more "Up" pointers.  Print an error, and call it
                     quits. */
                  register int i;

                  for (i = 0; i < up_counter; i++)
                    {
                      info_win->nodes_index--;
                      free (info_win->nodes[info_win->nodes_index]);
                      info_win->nodes[info_win->nodes_index] = (NODE *)NULL;
                    }
                  info_win->current = old_current;
                  window->node = info_win->nodes[old_current];
                  window->pagetop = info_win->pagetops[old_current];
                  window->point = info_win->points[old_current];
                  recalculate_line_starts (window);
                  window->flags |= W_UpdateWindow;
                  info_error (_("No more nodes within this document."));
                }
            }
        }
        break;
      }
    }
}

/* Move Prev, Up or error in WINDOW depending on BEHAVIOUR. */
static void
backward_move_node_structure (window, behaviour)
     WINDOW *window;
     int behaviour;
{
  switch (behaviour)
    {
    case IS_PageOnly:
      info_error (msg_at_node_top);
      break;

    case IS_NextOnly:
      info_prev_label_of_node (window->node);
      if (!info_parsed_nodename && !info_parsed_filename)
        info_error (_("No `Prev' for this node."));
      else
        {
          window_message_in_echo_area (_("Moving Prev in this window."));
          info_handle_pointer ("Prev", window);
        }
      break;

    case IS_Continuous:
      info_prev_label_of_node (window->node);

      if (!info_parsed_nodename && (!info_parsed_filename
                                    || is_dir_name (info_parsed_filename)))
        {
          info_up_label_of_node (window->node);
          if (!info_parsed_nodename && (!info_parsed_filename
                                        || is_dir_name (info_parsed_filename)))
            info_error (_("No `Prev' or `Up' for this node within this document."));
          else
            {
              window_message_in_echo_area (_("Moving Up in this window."));
              info_handle_pointer ("Up", window);
            }
        }
      else
        {
          REFERENCE **menu;
          int inhibit_menu_traversing = 0;

          /* Watch out!  If this node's Prev is the same as the Up, then
             move Up.  Otherwise, we could move Prev, and then to the last
             menu item in the Prev.  This would cause the user to loop
             through a subsection of the info file. */
          if (!info_parsed_filename && info_parsed_nodename)
            {
              char *pnode;

              pnode = xstrdup (info_parsed_nodename);
              info_up_label_of_node (window->node);

              if (!info_parsed_filename && info_parsed_nodename &&
                  strcmp (info_parsed_nodename, pnode) == 0)
                {
                  /* The nodes are the same.  Inhibit moving to the last
                     menu item. */
                  free (pnode);
                  inhibit_menu_traversing = 1;
                }
              else
                {
                  free (pnode);
                  info_prev_label_of_node (window->node);
                }
            }

          /* Move to the previous node.  If this node now contains a menu,
             and we have not inhibited movement to it, move to the node
             corresponding to the last menu item. */
          window_message_in_echo_area (_("Moving Prev in this window."));
          info_handle_pointer ("Prev", window);

          if (!inhibit_menu_traversing)
            {
              while (!info_error_was_printed &&
                     (menu = info_menu_of_node (window->node)))
                {
                  info_free_references (menu);
                  window_message_in_echo_area
                    (_("Moving to `Prev's last menu item."));
                  info_menu_digit (window, 1, '0');
                }
            }
        }
      break;
    }
}

/* Move continuously forward through the node structure of this info file. */
DECLARE_INFO_COMMAND (info_global_next_node,
                      _("Move forwards or down through node structure"))
{
  if (count < 0)
    info_global_prev_node (window, -count, key);
  else
    {
      while (count && !info_error_was_printed)
        {
          forward_move_node_structure (window, IS_Continuous);
          count--;
        }
    }
}

/* Move continuously backward through the node structure of this info file. */
DECLARE_INFO_COMMAND (info_global_prev_node,
                      _("Move backwards or up through node structure"))
{
  if (count < 0)
    info_global_next_node (window, -count, key);
  else
    {
      while (count && !info_error_was_printed)
        {
          backward_move_node_structure (window, IS_Continuous);
          count--;
        }
    }
}

static void _scroll_forward();
static void _scroll_backward();

static void
_scroll_forward(window, count, key, behaviour)
  WINDOW *window;
  int count;
  unsigned char key;
  int behaviour;
{
  if (count < 0)
    _scroll_backward (window, -count, key, behaviour);
  else
    {
      int desired_top;

      /* Without an explicit numeric argument, scroll the bottom two
         lines to the top of this window,  Or, if at bottom of window,
         and the chosen behaviour is to scroll through nodes get the
	 "Next" node for this window. */
      if (default_window_size > 0)
        desired_top = window->pagetop + default_window_size;
      else if (!info_explicit_arg && count == 1)
        {
          desired_top = window->pagetop + (window->height - 2);

          /* If there are no more lines to scroll here, error, or get
             another node, depending on BEHAVIOUR. */
          if (desired_top > window->line_count)
            {
              forward_move_node_structure (window, behaviour);
              return;
            }
        }
      else
        desired_top = window->pagetop + count;

      if (desired_top >= window->line_count)
        desired_top = window->line_count - 2;

      if (window->pagetop > desired_top)
        return;
      else
        set_window_pagetop (window, desired_top);
    }
}

static void
_scroll_backward(window, count, key, behaviour)
  WINDOW *window;
  int count;
  unsigned char key;
  int behaviour;
{
  if (count < 0)
    _scroll_forward (window, -count, key, behaviour);
  else
    {
      int desired_top;

      /* Without an explicit numeric argument, scroll the top two lines
         to the bottom of this window, or, depending on the selected
	 behaviour, move to the previous, or Up'th node. */
      if (default_window_size > 0)
        desired_top = window->pagetop - default_window_size;
      else if (!info_explicit_arg && count == 1)
        {
          desired_top = window->pagetop - (window->height - 2);

          if ((desired_top < 0) && (window->pagetop == 0))
            {
              backward_move_node_structure (window, behaviour);
              return;
            }
        }
      else
        desired_top = window->pagetop - count;

      if (desired_top < 0)
        desired_top = 0;

      set_window_pagetop (window, desired_top);
    }
}

/* Show the next screen of WINDOW's node. */
DECLARE_INFO_COMMAND (info_scroll_forward, _("Scroll forward in this window"))
{
  _scroll_forward (window, count, key, info_scroll_behaviour);
}

/* Like info_scroll_forward, but sets default_window_size as a side
   effect.  */
DECLARE_INFO_COMMAND (info_scroll_forward_set_window,
		      _("Scroll forward in this window and set default window size"))
{
  if (info_explicit_arg)
    default_window_size = count;
  _scroll_forward (window, count, key, info_scroll_behaviour);
}

/* Show the next screen of WINDOW's node but never advance to next node. */
DECLARE_INFO_COMMAND (info_scroll_forward_page_only, _("Scroll forward in this window staying within node"))
{
  _scroll_forward (window, count, key, IS_PageOnly);
}

/* Like info_scroll_forward_page_only, but sets default_window_size as a side
   effect.  */
DECLARE_INFO_COMMAND (info_scroll_forward_page_only_set_window,
		      _("Scroll forward in this window staying within node and set default window size"))
{
  if (info_explicit_arg)
    default_window_size = count;
  _scroll_forward (window, count, key, IS_PageOnly);
}

/* Show the previous screen of WINDOW's node. */
DECLARE_INFO_COMMAND (info_scroll_backward, _("Scroll backward in this window"))
{
  _scroll_backward (window, count, key, info_scroll_behaviour);
}

/* Like info_scroll_backward, but sets default_window_size as a side
   effect.  */
DECLARE_INFO_COMMAND (info_scroll_backward_set_window,
		      _("Scroll backward in this window and set default window size"))
{
  if (info_explicit_arg)
    default_window_size = count;
  _scroll_backward (window, count, key, info_scroll_behaviour);
}

/* Show the previous screen of WINDOW's node but never move to previous
   node. */
DECLARE_INFO_COMMAND (info_scroll_backward_page_only, _("Scroll backward in this window staying within node"))
{
  _scroll_backward (window, count, key, IS_PageOnly);
}

/* Like info_scroll_backward_page_only, but sets default_window_size as a side
   effect.  */
DECLARE_INFO_COMMAND (info_scroll_backward_page_only_set_window,
		      _("Scroll backward in this window staying within node and set default window size"))
{
  if (info_explicit_arg)
    default_window_size = count;
  _scroll_backward (window, count, key, IS_PageOnly);
}

/* Move to the beginning of the node. */
DECLARE_INFO_COMMAND (info_beginning_of_node, _("Move to the start of this node"))
{
  window->pagetop = window->point = 0;
  window->flags |= W_UpdateWindow;
}

/* Move to the end of the node. */
DECLARE_INFO_COMMAND (info_end_of_node, _("Move to the end of this node"))
{
  window->point = window->node->nodelen - 1;
  info_show_point (window);
}

/* Scroll the window forward by N lines.  */
DECLARE_INFO_COMMAND (info_down_line, _("Scroll down by lines"))
{
  if (count < 0)
    info_up_line (window, -count, key);
  else
    {
      int desired_top = window->pagetop + count;

      if (desired_top >= window->line_count)
	desired_top = window->line_count - 2;

      if (window->pagetop <= desired_top)
	set_window_pagetop (window, desired_top);
    }
}

/* Scroll the window backward by N lines.  */
DECLARE_INFO_COMMAND (info_up_line, _("Scroll up by lines"))
{
  if (count < 0)
    info_down_line (window, -count, key);
  else
    {
      int desired_top = window->pagetop - count;

      if (desired_top < 0)
	desired_top = 0;

      set_window_pagetop (window, desired_top);
    }
}

/* Scroll the window forward by N lines and remember N as default for
   subsequent commands.  */
DECLARE_INFO_COMMAND (info_scroll_half_screen_down,
		      _("Scroll down by half screen size"))
{
  if (count < 0)
    info_scroll_half_screen_up (window, -count, key);
  else
    {
      int scroll_size = (the_screen->height + 1) / 2;
      int desired_top;

      if (info_explicit_arg)
	default_scroll_size = count;
      if (default_scroll_size > 0)
	scroll_size = default_scroll_size;

      desired_top = window->pagetop + scroll_size;
      if (desired_top >= window->line_count)
	desired_top = window->line_count - 2;

      if (window->pagetop <= desired_top)
	set_window_pagetop (window, desired_top);
    }
}

/* Scroll the window backward by N lines and remember N as default for
   subsequent commands.  */
DECLARE_INFO_COMMAND (info_scroll_half_screen_up,
		      _("Scroll up by half screen size"))
{
  if (count < 0)
    info_scroll_half_screen_down (window, -count, key);
  else
    {
      int scroll_size = (the_screen->height + 1) / 2;
      int desired_top;

      if (info_explicit_arg)
	default_scroll_size = count;
      if (default_scroll_size > 0)
	scroll_size = default_scroll_size;

      desired_top = window->pagetop - scroll_size;
      if (desired_top < 0)
	desired_top = 0;

      set_window_pagetop (window, desired_top);
    }
}

/* **************************************************************** */
/*                                                                  */
/*                 Commands for Manipulating Windows                */
/*                                                                  */
/* **************************************************************** */

/* Make the next window in the chain be the active window. */
DECLARE_INFO_COMMAND (info_next_window, _("Select the next window"))
{
  if (count < 0)
    {
      info_prev_window (window, -count, key);
      return;
    }

  /* If no other window, error now. */
  if (!windows->next && !echo_area_is_active)
    {
      info_error (msg_one_window);
      return;
    }

  while (count--)
    {
      if (window->next)
        window = window->next;
      else
        {
          if (window == the_echo_area || !echo_area_is_active)
            window = windows;
          else
            window = the_echo_area;
        }
    }

  if (active_window != window)
    {
      if (auto_footnotes_p)
        info_get_or_remove_footnotes (window);

      window->flags |= W_UpdateWindow;
      active_window = window;
    }
}

/* Make the previous window in the chain be the active window. */
DECLARE_INFO_COMMAND (info_prev_window, _("Select the previous window"))
{
  if (count < 0)
    {
      info_next_window (window, -count, key);
      return;
    }

  /* Only one window? */

  if (!windows->next && !echo_area_is_active)
    {
      info_error (msg_one_window);
      return;
    }

  while (count--)
    {
      /* If we are in the echo area, or if the echo area isn't active and we
         are in the first window, find the last window in the chain. */
      if (window == the_echo_area ||
          (window == windows && !echo_area_is_active))
        {
          register WINDOW *win, *last;

          for (win = windows; win; win = win->next)
            last = win;

          window = last;
        }
      else
        {
          if (window == windows)
            window = the_echo_area;
          else
            window = window->prev;
        }
    }

  if (active_window != window)
    {
      if (auto_footnotes_p)
        info_get_or_remove_footnotes (window);

      window->flags |= W_UpdateWindow;
      active_window = window;
    }
}

/* Split WINDOW into two windows, both showing the same node.  If we
   are automatically tiling windows, re-tile after the split. */
DECLARE_INFO_COMMAND (info_split_window, _("Split the current window"))
{
  WINDOW *split, *old_active;
  int pagetop;

  /* Remember the current pagetop of the window being split.  If it doesn't
     change, we can scroll its contents around after the split. */
  pagetop = window->pagetop;

  /* Make the new window. */
  old_active = active_window;
  active_window = window;
  split = window_make_window (window->node);
  active_window = old_active;

  if (!split)
    {
      info_error (msg_win_too_small);
    }
  else
    {
#if defined (SPLIT_BEFORE_ACTIVE)
      /* Try to scroll the old window into its new postion. */
      if (pagetop == window->pagetop)
        {
          int start, end, amount;

          start = split->first_row;
          end = start + window->height;
          amount = split->height + 1;
          display_scroll_display (start, end, amount);
        }
#else /* !SPLIT_BEFORE_ACTIVE */
      /* Make sure point still appears in the active window. */
      info_show_point (window);
#endif /* !SPLIT_BEFORE_ACTIVE */

      /* If the window just split was one internal to Info, try to display
         something else in it. */
      if (internal_info_node_p (split->node))
        {
          register int i, j;
          INFO_WINDOW *iw;
          NODE *node = (NODE *)NULL;
          char *filename;

          for (i = 0; (iw = info_windows[i]); i++)
            {
              for (j = 0; j < iw->nodes_index; j++)
                if (!internal_info_node_p (iw->nodes[j]))
                  {
                    if (iw->nodes[j]->parent)
                      filename = iw->nodes[j]->parent;
                    else
                      filename = iw->nodes[j]->filename;

                    node = info_get_node (filename, iw->nodes[j]->nodename);
                    if (node)
                      {
                        window_set_node_of_window (split, node);
                        i = info_windows_index - 1;
                        break;
                      }
                  }
            }
        }
      split->pagetop = window->pagetop;

      if (auto_tiling_p)
        window_tile_windows (DONT_TILE_INTERNALS);
      else
        window_adjust_pagetop (split);

      remember_window_and_node (split, split->node);
    }
}

/* Delete WINDOW, forgetting the list of last visited nodes.  If we are
   automatically displaying footnotes, show or remove the footnotes
   window.  If we are automatically tiling windows, re-tile after the
   deletion. */
DECLARE_INFO_COMMAND (info_delete_window, _("Delete the current window"))
{
  if (!windows->next)
    {
      info_error (msg_cant_kill_last);
    }
  else if (window->flags & W_WindowIsPerm)
    {
      info_error (_("Cannot delete a permanent window"));
    }
  else
    {
      info_delete_window_internal (window);

      if (auto_footnotes_p)
        info_get_or_remove_footnotes (active_window);

      if (auto_tiling_p)
        window_tile_windows (DONT_TILE_INTERNALS);
    }
}

/* Do the physical deletion of WINDOW, and forget this window and
   associated nodes. */
void
info_delete_window_internal (window)
     WINDOW *window;
{
  if (windows->next && ((window->flags & W_WindowIsPerm) == 0))
    {
      /* We not only delete the window from the display, we forget it from
         our list of remembered windows. */
      forget_window_and_nodes (window);
      window_delete_window (window);

      if (echo_area_is_active)
        echo_area_inform_of_deleted_window (window);
    }
}

/* Just keep WINDOW, deleting all others. */
DECLARE_INFO_COMMAND (info_keep_one_window, _("Delete all other windows"))
{
  int num_deleted;              /* The number of windows we deleted. */
  int pagetop, start, end;

  /* Remember a few things about this window.  We may be able to speed up
     redisplay later by scrolling its contents. */
  pagetop = window->pagetop;
  start = window->first_row;
  end = start + window->height;

  num_deleted = 0;

  while (1)
    {
      WINDOW *win;

      /* Find an eligible window and delete it.  If no eligible windows
         are found, we are done.  A window is eligible for deletion if
         is it not permanent, and it is not WINDOW. */
      for (win = windows; win; win = win->next)
        if (win != window && ((win->flags & W_WindowIsPerm) == 0))
          break;

      if (!win)
        break;

      info_delete_window_internal (win);
      num_deleted++;
    }

  /* Scroll the contents of this window into the right place so that the
     user doesn't have to wait any longer than necessary for redisplay. */
  if (num_deleted)
    {
      int amount;

      amount = (window->first_row - start);
      amount -= (window->pagetop - pagetop);
      display_scroll_display (start, end, amount);
    }

  window->flags |= W_UpdateWindow;
}

/* Scroll the "other" window of WINDOW. */
DECLARE_INFO_COMMAND (info_scroll_other_window, _("Scroll the other window"))
{
  WINDOW *other;

  /* If only one window, give up. */
  if (!windows->next)
    {
      info_error (msg_one_window);
      return;
    }

  other = window->next;

  if (!other)
    other = window->prev;

  info_scroll_forward (other, count, key);
}

/* Scroll the "other" window of WINDOW. */
DECLARE_INFO_COMMAND (info_scroll_other_window_backward,
                      _("Scroll the other window backward"))
{
  info_scroll_other_window (window, -count, key);
}

/* Change the size of WINDOW by AMOUNT. */
DECLARE_INFO_COMMAND (info_grow_window, _("Grow (or shrink) this window"))
{
  window_change_window_height (window, count);
}

/* When non-zero, tiling takes place automatically when info_split_window
   is called. */
int auto_tiling_p = 0;

/* Tile all of the visible windows. */
DECLARE_INFO_COMMAND (info_tile_windows,
    _("Divide the available screen space among the visible windows"))
{
  window_tile_windows (TILE_INTERNALS);
}

/* Toggle the state of this window's wrapping of lines. */
DECLARE_INFO_COMMAND (info_toggle_wrap,
              _("Toggle the state of line wrapping in the current window"))
{
  window_toggle_wrap (window);
}

/* **************************************************************** */
/*                                                                  */
/*                      Info Node Commands                          */
/*                                                                  */
/* **************************************************************** */

/* Return (FILENAME)NODENAME for NODE, or just NODENAME if NODE's
   filename is not set. */
char *
node_printed_rep (node)
     NODE *node;
{
  char *rep;

  if (node->filename)
    {
      char *filename
       = filename_non_directory (node->parent ? node->parent : node->filename);
      rep = xmalloc (1 + strlen (filename) + 1 + strlen (node->nodename) + 1);
      sprintf (rep, "(%s)%s", filename, node->nodename);
    }
  else
    rep = node->nodename;

  return rep;
}


/* Using WINDOW for various defaults, select the node referenced by ENTRY
   in it.  If the node is selected, the window and node are remembered. */
void
info_select_reference (window, entry)
     WINDOW *window;
     REFERENCE *entry;
{
  NODE *node;
  char *filename, *nodename, *file_system_error;

  file_system_error = (char *)NULL;

  filename = entry->filename;
  if (!filename)
    filename = window->node->parent;
  if (!filename)
    filename = window->node->filename;

  if (filename)
    filename = xstrdup (filename);

  if (entry->nodename)
    nodename = xstrdup (entry->nodename);
  else
    nodename = xstrdup ("Top");

  node = info_get_node (filename, nodename);

  /* Try something a little weird.  If the node couldn't be found, and the
     reference was of the form "foo::", see if the entry->label can be found
     as a file, with a node of "Top". */
  if (!node)
    {
      if (info_recent_file_error)
        file_system_error = xstrdup (info_recent_file_error);

      if (entry->nodename && (strcmp (entry->nodename, entry->label) == 0))
        {
          node = info_get_node (entry->label, "Top");
          if (!node && info_recent_file_error)
            {
              maybe_free (file_system_error);
              file_system_error = xstrdup (info_recent_file_error);
            }
        }
    }

  if (!node)
    {
      if (file_system_error)
        info_error (file_system_error);
      else
        info_error (msg_cant_find_node, nodename);
    }

  maybe_free (file_system_error);
  maybe_free (filename);
  maybe_free (nodename);

  if (node)
    info_set_node_of_window (1, window, node);
}

/* Parse the node specification in LINE using WINDOW to default the filename.
   Select the parsed node in WINDOW and remember it, or error if the node
   couldn't be found. */
static void
info_parse_and_select (line, window)
     char *line;
     WINDOW *window;
{
  REFERENCE entry;

  info_parse_node (line, DONT_SKIP_NEWLINES);

  entry.nodename = info_parsed_nodename;
  entry.filename = info_parsed_filename;
  entry.label = "*info-parse-and-select*";

  info_select_reference (window, &entry);
}

/* Given that the values of INFO_PARSED_FILENAME and INFO_PARSED_NODENAME
   are previously filled, try to get the node represented by them into
   WINDOW.  The node should have been pointed to by the LABEL pointer of
   WINDOW->node. */
static void
info_handle_pointer (label, window)
     char *label;
     WINDOW *window;
{
  if (info_parsed_filename || info_parsed_nodename)
    {
      char *filename, *nodename;
      NODE *node;

      filename = nodename = (char *)NULL;

      if (info_parsed_filename)
        filename = xstrdup (info_parsed_filename);
      else
        {
          if (window->node->parent)
            filename = xstrdup (window->node->parent);
          else if (window->node->filename)
            filename = xstrdup (window->node->filename);
        }

      if (info_parsed_nodename)
        nodename = xstrdup (info_parsed_nodename);
      else
        nodename = xstrdup ("Top");

      node = info_get_node (filename, nodename);

      if (node)
        {
          INFO_WINDOW *info_win;

          info_win = get_info_window_of_window (window);
          if (info_win)
            {
              info_win->pagetops[info_win->current] = window->pagetop;
              info_win->points[info_win->current] = window->point;
            }
          info_set_node_of_window (1, window, node);
        }
      else
        {
          if (info_recent_file_error)
            info_error (info_recent_file_error);
          else
            info_error (msg_cant_file_node, filename, nodename);
        }

      free (filename);
      free (nodename);
    }
  else
    {
      info_error (msg_no_pointer, label);
    }
}

/* Make WINDOW display the "Next:" node of the node currently being
   displayed. */
DECLARE_INFO_COMMAND (info_next_node, _("Select the Next node"))
{
  info_next_label_of_node (window->node);
  info_handle_pointer ("Next", window);
}

/* Make WINDOW display the "Prev:" node of the node currently being
   displayed. */
DECLARE_INFO_COMMAND (info_prev_node, _("Select the Prev node"))
{
  info_prev_label_of_node (window->node);
  info_handle_pointer ("Prev", window);
}

/* Make WINDOW display the "Up:" node of the node currently being
   displayed. */
DECLARE_INFO_COMMAND (info_up_node, _("Select the Up node"))
{
  info_up_label_of_node (window->node);
  info_handle_pointer ("Up", window);
}

/* Make WINDOW display the last node of this info file. */
DECLARE_INFO_COMMAND (info_last_node, _("Select the last node in this file"))
{
  register int i;
  FILE_BUFFER *fb = file_buffer_of_window (window);
  NODE *node = (NODE *)NULL;

  if (fb && fb->tags)
    {
      int last_node_tag_idx = -1;

      /* If no explicit argument, or argument of zero, default to the
         last node.  */
      if (count == 0 || (count == 1 && !info_explicit_arg))
        count = -1;
      for (i = 0; count && fb->tags[i]; i++)
        if (fb->tags[i]->nodelen != 0) /* don't count anchor tags */
          {
            count--;
            last_node_tag_idx = i;
          }
      if (count > 0)
        i = last_node_tag_idx + 1;
      if (i > 0)
        node = info_get_node (fb->filename, fb->tags[i - 1]->nodename);
    }

  if (!node)
    info_error (_("This window has no additional nodes"));
  else
    info_set_node_of_window (1, window, node);
}

/* Make WINDOW display the first node of this info file. */
DECLARE_INFO_COMMAND (info_first_node, _("Select the first node in this file"))
{
  FILE_BUFFER *fb = file_buffer_of_window (window);
  NODE *node = (NODE *)NULL;

  /* If no explicit argument, or argument of zero, default to the
     first node.  */
  if (count == 0)
    count = 1;
  if (fb && fb->tags)
    {
      register int i;
      int last_node_tag_idx = -1;

      for (i = 0; count && fb->tags[i]; i++)
        if (fb->tags[i]->nodelen != 0) /* don't count anchor tags */
          {
            count--;
            last_node_tag_idx = i;
          }
      if (count > 0)
        i = last_node_tag_idx + 1;
      if (i > 0)
        node = info_get_node (fb->filename, fb->tags[i - 1]->nodename);
    }

  if (!node)
    info_error (_("This window has no additional nodes"));
  else
    info_set_node_of_window (1, window, node);
}

/* Select the last menu item in WINDOW->node. */
DECLARE_INFO_COMMAND (info_last_menu_item,
   _("Select the last item in this node's menu"))
{
  info_menu_digit (window, 1, '0');
}

/* Use KEY (a digit) to select the Nth menu item in WINDOW->node. */
DECLARE_INFO_COMMAND (info_menu_digit, _("Select this menu item"))
{
  register int i, item;
  register REFERENCE *entry, **menu;

  menu = info_menu_of_node (window->node);

  if (!menu)
    {
      info_error (msg_no_menu_node);
      return;
    }

  /* We have the menu.  See if there are this many items in it. */
  item = key - '0';

  /* Special case.  Item "0" is the last item in this menu. */
  if (item == 0)
    for (i = 0; menu[i + 1]; i++);
  else
    {
      for (i = 0; (entry = menu[i]); i++)
        if (i == item - 1)
          break;
    }

  if (menu[i])
    info_select_reference (window, menu[i]);
  else
    info_error (_("There aren't %d items in this menu."), item);

  info_free_references (menu);
  return;
}



/* Return a pointer to the xref in XREF_LIST that is nearest to POS, or
   NULL if XREF_LIST is empty.  That is, if POS is within any of the
   given xrefs, return that one.  Otherwise, return the one with the
   nearest beginning or end.  If there are two that are equidistant,
   prefer the one forward.  The return is in newly-allocated memory,
   since the caller frees it.
   
   This is called from info_menu_or_ref_item with XREF_LIST being all
   the xrefs in the node, and POS being point.  The ui function that
   starts it all off is select-reference-this-line.

   This is not the same logic as in info.el.  Info-get-token prefers
   searching backwards to searching forwards, and has a hardwired search
   limit of 200 chars (in Emacs 21.2).  */

static REFERENCE **
nearest_xref (xref_list, pos)
    REFERENCE **xref_list;
    long pos;
{
  int this_xref;
  int nearest = -1;
  long best_delta = -1;
  
  for (this_xref = 0; xref_list[this_xref]; this_xref++)
    {
      long delta;
      REFERENCE *xref = xref_list[this_xref];
      if (xref->start <= pos && pos <= xref->end)
        { /* POS is within this xref, we're done */
          nearest = this_xref;
          break;
        }
      
      /* See how far POS is from this xref.  Take into account the
         `*Note' that begins the xref, since as far as the user is
         concerned, that's where it starts.  */
      delta = MIN (labs (pos - (xref->start - strlen (INFO_XREF_LABEL))),
                   labs (pos - xref->end));
      
      /* It's the <= instead of < that makes us choose the forward xref
         of POS if two are equidistant.  Of course, because of all the
         punctuation surrounding xrefs, it's not necessarily obvious
         where one ends.  */
      if (delta <= best_delta || best_delta < 0)
        {
          nearest = this_xref;
          best_delta = delta;
        }
    }
  
  /* Maybe there was no list to search through.  */
  if (nearest < 0)
    return NULL;
  
  /* Ok, we have a nearest xref, make a list of it.  */
  {
    REFERENCE **ret = xmalloc (sizeof (REFERENCE *) * 2);
    ret[0] = info_copy_reference (xref_list[nearest]);
    ret[1] = NULL;
    return ret;
  }
}


/* Read a menu or followed reference from the user defaulting to the
   reference found on the current line, and select that node.  The
   reading is done with completion.  BUILDER is the function used
   to build the list of references.  ASK_P is non-zero if the user
   should be prompted, or zero to select the default item. */
static void
info_menu_or_ref_item (window, count, key, builder, ask_p)
     WINDOW *window;
     int count;
     unsigned char key;
     REFERENCE **(*builder) ();
     int ask_p;
{
  char *line;
  REFERENCE *entry;
  REFERENCE *defentry = NULL;
  REFERENCE **menu = (*builder) (window->node);

  if (!menu)
    {
      if (builder == info_menu_of_node)
        info_error (msg_no_menu_node);
      else
        info_error (msg_no_xref_node);
      return;
    }

  /* Default the selected reference to the one which is on the line that
     point is in.  */
  {
    REFERENCE **refs = NULL;
    int point_line = window_line_of_point (window);

    if (point_line != -1)
      {
        SEARCH_BINDING binding;

        binding.buffer = window->node->contents;
        binding.start = window->line_starts[point_line] - binding.buffer;
        if (window->line_starts[point_line + 1])
          binding.end = window->line_starts[point_line + 1] - binding.buffer;
        else
          binding.end = window->node->nodelen;
        binding.flags = 0;

        if (builder == info_menu_of_node)
          {
            if (point_line)
              {
                binding.start--;
                refs = info_menu_items (&binding);
              }
          }
        else
          {
#if defined (HANDLE_MAN_PAGES)
            if (window->node->flags & N_IsManPage)
              refs = manpage_xrefs_in_binding (window->node, &binding);
            else
#endif /* HANDLE_MAN_PAGES */
              refs = nearest_xref (menu, window->point);
          }

        if (refs && refs[0])
          {
            if (strcmp (refs[0]->label, "Menu") != 0
                || builder == info_xrefs_of_node)
              {
                int which = 0;

                /* For xrefs, find the closest reference to point,
                   unless we only have one reference (as we will if
                   we've called nearest_xref above).  It would be better
                   to have only one piece of code, but the conditions
                   when we call this are tangled.  */
                if (builder == info_xrefs_of_node && refs[1])
                  {
                    int closest = -1;

                    for (; refs[which]; which++)
                      {
                        if (window->point >= refs[which]->start
                            && window->point <= refs[which]->end)
                          {
                            closest = which;
                            break;
                          }
                        else if (window->point < refs[which]->start)
                          break;
                      }
                    if (closest == -1)
                      which--;
                    else
                      which = closest;
                  }

                defentry = (REFERENCE *)xmalloc (sizeof (REFERENCE));
                defentry->label = xstrdup (refs[which]->label);
                defentry->filename = refs[which]->filename;
                defentry->nodename = refs[which]->nodename;

                if (defentry->filename)
                  defentry->filename = xstrdup (defentry->filename);
                if (defentry->nodename)
                  defentry->nodename = xstrdup (defentry->nodename);
              }
            info_free_references (refs);
          }
      }
  }

  /* If we are going to ask the user a question, do it now. */
  if (ask_p)
    {
      char *prompt;

      /* Build the prompt string. */
      if (defentry)
        prompt = (char *)xmalloc (99 + strlen (defentry->label));
      else
        prompt = (char *)xmalloc (99);

      if (builder == info_menu_of_node)
        {
          if (defentry)
            sprintf (prompt, _("Menu item (%s): "), defentry->label);
          else
            sprintf (prompt, _("Menu item: "));
        }
      else
        {
          if (defentry)
            sprintf (prompt, _("Follow xref (%s): "), defentry->label);
          else
            sprintf (prompt, _("Follow xref: "));
        }

      line = info_read_completing_in_echo_area (window, prompt, menu);
      free (prompt);

      window = active_window;

      /* User aborts, just quit. */
      if (!line)
        {
          maybe_free (defentry);
          info_free_references (menu);
          info_abort_key (window, 0, 0);
          return;
        }

      /* If we had a default and the user accepted it, use that. */
      if (!*line)
        {
          free (line);
          if (defentry)
            line = xstrdup (defentry->label);
          else
            line = (char *)NULL;
        }
    }
  else
    {
      /* Not going to ask any questions.  If we have a default entry, use
         that, otherwise return. */
      if (!defentry)
        return;
      else
        line = xstrdup (defentry->label);
    }

  if (line)
    {
      /* It is possible that the references have more than a single
         entry with the same label, and also LINE is down-cased, which
         complicates matters even more.  Try to be as accurate as we
         can: if they've chosen the default, use defentry directly. */
      if (defentry && strcmp (line, defentry->label) == 0)
        entry = defentry;
      else
        /* Find the selected label in the references.  If there are
           more than one label which matches, find the one that's
           closest to point.  */
        {
          register int i;
          int best = -1, min_dist = window->node->nodelen;
          REFERENCE *ref;

          for (i = 0; menu && (ref = menu[i]); i++)
            {
              /* Need to use strcasecmp because LINE is downcased
                 inside info_read_completing_in_echo_area.  */
              if (strcasecmp (line, ref->label) == 0)
                {
                  /* ref->end is more accurate estimate of position
                     for menus than ref->start.  Go figure.  */
                  int dist = abs (window->point - ref->end);

                  if (dist < min_dist)
                    {
                      min_dist = dist;
                      best = i;
                    }
                }
            }
          if (best != -1)
            entry = menu[best];
          else
            entry = (REFERENCE *)NULL;
        }

      if (!entry && defentry)
        info_error (_("The reference disappeared! (%s)."), line);
      else
        {
          NODE *orig = window->node;
          info_select_reference (window, entry);
          if (builder == info_xrefs_of_node && window->node != orig
              && !(window->node->flags & N_FromAnchor))
            { /* Search for this reference in the node.  */
              long offset;
              long start;

              if (window->line_count > 0)
                start = window->line_starts[1] - window->node->contents;
              else
                start = 0;

              offset =
                info_target_search_node (window->node, entry->label, start);

              if (offset != -1)
                {
                  window->point = offset;
                  window_adjust_pagetop (window);
                }
            }
        }

      free (line);
      if (defentry)
        {
          free (defentry->label);
          maybe_free (defentry->filename);
          maybe_free (defentry->nodename);
          free (defentry);
        }
    }

  info_free_references (menu);

  if (!info_error_was_printed)
    window_clear_echo_area ();
}

/* Read a line (with completion) which is the name of a menu item,
   and select that item. */
DECLARE_INFO_COMMAND (info_menu_item, _("Read a menu item and select its node"))
{
  info_menu_or_ref_item (window, count, key, info_menu_of_node, 1);
}

/* Read a line (with completion) which is the name of a reference to
   follow, and select the node. */
DECLARE_INFO_COMMAND
  (info_xref_item, _("Read a footnote or cross reference and select its node"))
{
  info_menu_or_ref_item (window, count, key, info_xrefs_of_node, 1);
}

/* Position the cursor at the start of this node's menu. */
DECLARE_INFO_COMMAND (info_find_menu, _("Move to the start of this node's menu"))
{
  SEARCH_BINDING binding;
  long position;

  binding.buffer = window->node->contents;
  binding.start  = 0;
  binding.end = window->node->nodelen;
  binding.flags = S_FoldCase | S_SkipDest;

  position = search (INFO_MENU_LABEL, &binding);

  if (position == -1)
    info_error (msg_no_menu_node);
  else
    {
      window->point = position;
      window_adjust_pagetop (window);
      window->flags |= W_UpdateWindow;
    }
}

/* Visit as many menu items as is possible, each in a separate window. */
DECLARE_INFO_COMMAND (info_visit_menu,
  _("Visit as many menu items at once as possible"))
{
  register int i;
  REFERENCE *entry, **menu;

  menu = info_menu_of_node (window->node);

  if (!menu)
    info_error (msg_no_menu_node);

  for (i = 0; (!info_error_was_printed) && (entry = menu[i]); i++)
    {
      WINDOW *new;

      new = window_make_window (window->node);
      window_tile_windows (TILE_INTERNALS);

      if (!new)
        info_error (msg_win_too_small);
      else
        {
          active_window = new;
          info_select_reference (new, entry);
        }
    }
}

/* Read a line of input which is a node name, and go to that node. */
DECLARE_INFO_COMMAND (info_goto_node, _("Read a node name and select it"))
{
  char *line;

#define GOTO_COMPLETES
#if defined (GOTO_COMPLETES)
  /* Build a completion list of all of the known nodes. */
  {
    register int fbi, i;
    FILE_BUFFER *current;
    REFERENCE **items = (REFERENCE **)NULL;
    int items_index = 0;
    int items_slots = 0;

    current = file_buffer_of_window (window);

    for (fbi = 0; info_loaded_files && info_loaded_files[fbi]; fbi++)
      {
        FILE_BUFFER *fb;
        REFERENCE *entry;
        int this_is_the_current_fb;

        fb = info_loaded_files[fbi];
        this_is_the_current_fb = (current == fb);

        entry = (REFERENCE *)xmalloc (sizeof (REFERENCE));
        entry->filename = entry->nodename = (char *)NULL;
        entry->label = (char *)xmalloc (4 + strlen (fb->filename));
        sprintf (entry->label, "(%s)*", fb->filename);

        add_pointer_to_array
          (entry, items_index, items, items_slots, 10, REFERENCE *);

        if (fb->tags)
          {
            for (i = 0; fb->tags[i]; i++)
              {
                entry = (REFERENCE *)xmalloc (sizeof (REFERENCE));
                entry->filename = entry->nodename = (char *)NULL;
		if (this_is_the_current_fb)
		  entry->label = xstrdup (fb->tags[i]->nodename);
		else
		  {
		    entry->label = (char *) xmalloc
		      (4 + strlen (fb->filename) +
		       strlen (fb->tags[i]->nodename));
		    sprintf (entry->label, "(%s)%s",
			     fb->filename, fb->tags[i]->nodename);
		  }

                add_pointer_to_array
                  (entry, items_index, items, items_slots, 100, REFERENCE *);
              }
          }
      }
    line = info_read_maybe_completing (window, _("Goto node: "), items);
    info_free_references (items);
  }
#else /* !GOTO_COMPLETES */
  line = info_read_in_echo_area (window, _("Goto node: "));
#endif /* !GOTO_COMPLETES */

  /* If the user aborted, quit now. */
  if (!line)
    {
      info_abort_key (window, 0, 0);
      return;
    }

  canonicalize_whitespace (line);

  if (*line)
    info_parse_and_select (line, window);

  free (line);
  if (!info_error_was_printed)
    window_clear_echo_area ();
}

/* Follow the menu list in MENUS (list of strings terminated by a NULL
   entry) from INITIAL_NODE.  If can't continue at any point (no menu or
   no menu entry for the next item), return the node so far -- that
   might be INITIAL_NODE itself.  If error, *ERRSTR and *ERRARG[12] will
   be set to the error message and argument for message, otherwise they
   will be NULL.  */

NODE *
info_follow_menus (initial_node, menus, errstr, errarg1, errarg2)
     NODE *initial_node;
     char **menus;
     const char **errstr;
     char **errarg1, **errarg2;
{
  NODE *node = NULL;
  *errstr = *errarg1 = *errarg2 = NULL;

  for (; *menus; menus++)
    {
      static char *first_arg = NULL;
      REFERENCE **menu;
      REFERENCE *entry;
      char *arg = *menus; /* Remember the name of the menu entry we want. */

      /* A leading space is certainly NOT part of a node name.  Most
	 probably, they typed a space after the separating comma.  The
	 strings in menus[] have their whitespace canonicalized, so
	 there's at most one space to ignore.  */
      if (*arg == ' ')
	arg++;
      if (!first_arg)
        first_arg = arg;

      /* Build and return a list of the menu items in this node. */
      menu = info_menu_of_node (initial_node);

      /* If no menu item in this node, stop here, but let the user
         continue to use Info.  Perhaps they wanted this node and didn't
         realize it. */
      if (!menu)
        {
          if (arg == first_arg)
            {
              node = make_manpage_node (first_arg);
              if (node)
                goto maybe_got_node;
            }
          *errstr = _("No menu in node `%s'.");
          *errarg1 = node_printed_rep (initial_node);
          return initial_node;
        }

      /* Find the specified menu item. */
      entry = info_get_labeled_reference (arg, menu);

      /* If the item wasn't found, search the list sloppily.  Perhaps this
         user typed "buffer" when they really meant "Buffers". */
      if (!entry)
        {
          int i;
          int best_guess = -1;

          for (i = 0; (entry = menu[i]); i++)
            {
              if (strcasecmp (entry->label, arg) == 0)
                break;
              else
                if (strncasecmp (entry->label, arg, strlen (arg)) == 0)
                  best_guess = i;
            }

          if (!entry && best_guess != -1)
            entry = menu[best_guess];
        }

      /* If we still failed to find the reference, start Info with the current
         node anyway.  It is probably a misspelling. */
      if (!entry)
        {
          if (arg == first_arg)
            {
	      /* Maybe they typed "info foo" instead of "info -f foo".  */
	      node = info_get_node (first_arg, 0);
	      if (node)
		add_file_directory_to_path (first_arg);
	      else
		node = make_manpage_node (first_arg);
              if (node)
                goto maybe_got_node;
            }

          info_free_references (menu);
          *errstr = _("No menu item `%s' in node `%s'.");
          *errarg1 = arg;
          *errarg2 = node_printed_rep (initial_node);
          return initial_node;
        }

      /* We have found the reference that the user specified.  If no
         filename in this reference, define it. */
      if (!entry->filename)
        entry->filename = xstrdup (initial_node->parent ? initial_node->parent
                                                     : initial_node->filename);

      /* Try to find this node.  */
      node = info_get_node (entry->filename, entry->nodename);
      if (!node && arg == first_arg)
	{
	  node = make_manpage_node (first_arg);
	  if (node)
	    goto maybe_got_node;
	}

      /* Since we cannot find it, try using the label of the entry as a
         file, i.e., "(LABEL)Top".  */
      if (!node && entry->nodename
          && strcmp (entry->label, entry->nodename) == 0)
        node = info_get_node (entry->label, "Top");

    maybe_got_node:
      if (!node)
        {
          *errstr = _("Unable to find node referenced by `%s' in `%s'.");
          *errarg1 = xstrdup (entry->label);
          *errarg2 = node_printed_rep (initial_node);
          info_free_references (menu);
          return initial_node;
        }

      info_free_references (menu);

      /* Success.  Go round the loop again.  */
      free (initial_node);
      initial_node = node;
    }

  return initial_node;
}

/* Split STR into individual node names by writing null bytes in wherever
   there are commas and constructing a list of the resulting pointers.
   (We can do this since STR has had canonicalize_whitespace called on it.)
   Return array terminated with NULL.  */

static char **
split_list_of_nodenames (str)
     char *str;
{
  unsigned len = 2;
  char **nodes = xmalloc (len * sizeof (char *));

  nodes[len - 2] = str;

  while (*str++)
    {
      if (*str == ',')
        {
          *str++ = 0;		/* get past the null byte */
          len++;
          nodes = xrealloc (nodes, len * sizeof (char *));
          nodes[len - 2] = str;
        }
    }

  nodes[len - 1] = NULL;

  return nodes;
}


/* Read a line of input which is a sequence of menus (starting from
   dir), and follow them.  */
DECLARE_INFO_COMMAND (info_menu_sequence,
   _("Read a list of menus starting from dir and follow them"))
{
  char *line = info_read_in_echo_area (window, _("Follow menus: "));

  /* If the user aborted, quit now. */
  if (!line)
    {
      info_abort_key (window, 0, 0);
      return;
    }

  canonicalize_whitespace (line);

  if (*line)
    {
      char *errstr, *errarg1, *errarg2;
      NODE *dir_node = info_get_node (NULL, NULL);
      char **nodes = split_list_of_nodenames (line);
      NODE *node;

      /* If DIR_NODE is NULL, they might be reading a file directly,
	 like in "info -d . -f ./foo".  Try using "Top" instead.  */
      if (!dir_node)
	{
	  char *file_name = window->node->parent;

	  if (!file_name)
	    file_name = window->node->filename;
	  dir_node = info_get_node (file_name, NULL);
	}

      /* If we still cannot find the starting point, give up.
	 We cannot allow a NULL pointer inside info_follow_menus.  */
      if (!dir_node)
	info_error (msg_cant_find_node, "Top");
      else
	node
	  = info_follow_menus (dir_node, nodes, &errstr, &errarg1, &errarg2);

      free (nodes);
      if (!errstr)
        info_set_node_of_window (1, window, node);
      else
        info_error (errstr, errarg1, errarg2);
    }

  free (line);
  if (!info_error_was_printed)
    window_clear_echo_area ();
}

/* Search the menu MENU for a (possibly mis-spelled) entry ARG.
   Return the menu entry, or the best guess for what they meant by ARG,
   or NULL if there's nothing in this menu seems to fit the bill.
   If EXACT is non-zero, allow only exact matches.  */
static REFERENCE *
entry_in_menu (arg, menu, exact)
     char *arg;
     REFERENCE **menu;
     int exact;
{
  REFERENCE *entry;

  /* First, try to find the specified menu item verbatim.  */
  entry = info_get_labeled_reference (arg, menu);

  /* If the item wasn't found, search the list sloppily.  Perhaps we
     have "Option Summary", but ARG is "option".  */
  if (!entry && !exact)
    {
      int i;
      int best_guess = -1;

      for (i = 0; (entry = menu[i]); i++)
	{
	  if (strcasecmp (entry->label, arg) == 0)
	    break;
	  else
	    if (strncasecmp (entry->label, arg, strlen (arg)) == 0)
	      best_guess = i;
	}

      if (!entry && best_guess != -1)
	entry = menu[best_guess];
    }

  return entry;
}

/* Find the node that is the best candidate to list the PROGRAM's
   invocation info and its command-line options, by looking for menu
   items and chains of menu items with characteristic names.  */
void
info_intuit_options_node (window, initial_node, program)
     WINDOW *window;
     NODE *initial_node;
     char *program;
{
  /* The list of node names typical for GNU manuals where the program
     usage and specifically the command-line arguments are described.
     This is pure heuristics.  I gathered these node names by looking
     at all the Info files I could put my hands on.  If you are
     looking for evidence to complain to the GNU project about
     non-uniform style of documentation, here you have your case!  */
  static const char *invocation_nodes[] = {
    "%s invocation",
    "Invoking %s",
    "Preliminaries",	/* m4 has Invoking under Preliminaries! */
    "Invocation",
    "Command Arguments",/* Emacs */
    "Invoking `%s'",
    "%s options",
    "Options",
    "Option ",		/* e.g. "Option Summary" */
    "Invoking",
    "All options",	/* tar, paxutils */
    "Arguments",
    "%s cmdline",	/* ar */
    "%s",		/* last resort */
    (const char *)0
  };
  NODE *node = NULL;
  REFERENCE **menu;
  const char **try_node;

  /* We keep looking deeper and deeper in the menu structure until
     there are no more menus or no menu items from the above list.
     Some manuals have the invocation node sitting 3 or 4 levels deep
     in the menu hierarchy...  */
  for (node = initial_node; node; initial_node = node)
    {
      REFERENCE *entry;

      /* Build and return a list of the menu items in this node. */
      menu = info_menu_of_node (initial_node);

      /* If no menu item in this node, stop here.  Perhaps this node
	 is the one they need.  */
      if (!menu)
	break;

      /* Look for node names typical for usage nodes in this menu.  */
      for (try_node = invocation_nodes; *try_node; try_node++)
	{
	  char nodename[200];

	  sprintf (nodename, *try_node, program);
	  /* The last resort "%s" is dangerous, so we restrict it
             to exact matches here.  */
	  entry = entry_in_menu (nodename, menu,
				 strcmp (*try_node, "%s") == 0);
	  if (entry)
	    break;
	}

      if (!entry)
	break;

      if (!entry->filename)
	entry->filename = xstrdup (initial_node->parent ? initial_node->parent
				   : initial_node->filename);
      /* Try to find this node.  */
      node = info_get_node (entry->filename, entry->nodename);
      info_free_references (menu);
      if (!node)
	break;
    }

  /* We've got our best shot at the invocation node.  Now select it.  */
  if (initial_node)
    info_set_node_of_window (1, window, initial_node);
  if (!info_error_was_printed)
    window_clear_echo_area ();
}

/* Given a name of an Info file, find the name of the package it
   describes by removing the leading directories and extensions.  */
char *
program_name_from_file_name (file_name)
     char *file_name;
{
  int i;
  char *program_name = xstrdup (filename_non_directory (file_name));

  for (i = strlen (program_name) - 1; i > 0; i--)
    if (program_name[i] == '.'
	&& (FILENAME_CMPN (program_name + i, ".info", 5) == 0
	    || FILENAME_CMPN (program_name + i, ".inf", 4) == 0
#ifdef __MSDOS__
	    || FILENAME_CMPN (program_name + i, ".i", 2) == 0
#endif
	    || isdigit (program_name[i + 1]))) /* a man page foo.1 */
      {
	program_name[i] = 0;
	break;
      }
  return program_name;
}

DECLARE_INFO_COMMAND (info_goto_invocation_node,
		      _("Find the node describing program invocation"))
{
  const char *invocation_prompt = _("Find Invocation node of [%s]: ");
  char *program_name, *line;
  char *default_program_name, *prompt, *file_name;
  NODE *top_node;

  /* Intuit the name of the program they are likely to want.
     We use the file name of the current Info file as a hint.  */
  file_name = window->node->parent ? window->node->parent
				   : window->node->filename;
  default_program_name = program_name_from_file_name (file_name);

  prompt = (char *)xmalloc (strlen (default_program_name) +
			    strlen (invocation_prompt));
  sprintf (prompt, invocation_prompt, default_program_name);
  line = info_read_in_echo_area (window, prompt);
  free (prompt);
  if (!line)
    {
      info_abort_key ();
      return;
    }
  if (*line)
    program_name = line;
  else
    program_name = default_program_name;

  /* In interactive usage they'd probably expect us to begin looking
     from the Top node.  */
  top_node = info_get_node (file_name, NULL);
  if (!top_node)
    info_error (msg_cant_find_node, "Top");

  info_intuit_options_node (window, top_node, program_name);
  free (line);
  free (default_program_name);
}

#if defined (HANDLE_MAN_PAGES)
DECLARE_INFO_COMMAND (info_man, _("Read a manpage reference and select it"))
{
  char *line;

  line = info_read_in_echo_area (window, _("Get Manpage: "));

  if (!line)
    {
      info_abort_key (window, 0, 0);
      return;
    }

  canonicalize_whitespace (line);

  if (*line)
    {
      char *goto_command;

      goto_command = (char *)xmalloc
        (4 + strlen (MANPAGE_FILE_BUFFER_NAME) + strlen (line));

      sprintf (goto_command, "(%s)%s", MANPAGE_FILE_BUFFER_NAME, line);

      info_parse_and_select (goto_command, window);
      free (goto_command);
    }

  free (line);
  if (!info_error_was_printed)
    window_clear_echo_area ();
}
#endif /* HANDLE_MAN_PAGES */

/* Move to the "Top" node in this file. */
DECLARE_INFO_COMMAND (info_top_node, _("Select the node `Top' in this file"))
{
  info_parse_and_select ("Top", window);
}

/* Move to the node "(dir)Top". */
DECLARE_INFO_COMMAND (info_dir_node, _("Select the node `(dir)'"))
{
  info_parse_and_select ("(dir)Top", window);
}


/* Read the name of a node to kill.  The list of available nodes comes
   from the nodes appearing in the current window configuration. */
static char *
read_nodename_to_kill (window)
     WINDOW *window;
{
  int iw;
  char *nodename;
  INFO_WINDOW *info_win;
  REFERENCE **menu = NULL;
  int menu_index = 0, menu_slots = 0;
  char *default_nodename = xstrdup (active_window->node->nodename);
  char *prompt = xmalloc (40 + strlen (default_nodename));

  sprintf (prompt, _("Kill node (%s): "), default_nodename);

  for (iw = 0; (info_win = info_windows[iw]); iw++)
    {
      REFERENCE *entry = (REFERENCE *)xmalloc (sizeof (REFERENCE));
      entry->label = xstrdup (info_win->window->node->nodename);
      entry->filename = entry->nodename = (char *)NULL;

      add_pointer_to_array (entry, menu_index, menu, menu_slots, 10,
                            REFERENCE *);
    }

  nodename = info_read_completing_in_echo_area (window, prompt, menu);
  free (prompt);
  info_free_references (menu);
  if (nodename && !*nodename)
    {
      free (nodename);
      nodename = default_nodename;
    }
  else
    free (default_nodename);

  return nodename;
}


/* Delete NODENAME from this window, showing the most
   recently selected node in this window. */
static void
kill_node (window, nodename)
     WINDOW *window;
     char *nodename;
{
  int iw, i;
  INFO_WINDOW *info_win;
  NODE *temp;

  /* If there is no nodename to kill, quit now. */
  if (!nodename)
    {
      info_abort_key (window, 0, 0);
      return;
    }

  /* If there is a nodename, find it in our window list. */
  for (iw = 0; (info_win = info_windows[iw]); iw++)
    if (strcmp (nodename, info_win->nodes[info_win->current]->nodename) == 0
	&& info_win->window == window)
      break;

  if (!info_win)
    {
      if (*nodename)
        info_error (_("Cannot kill node `%s'"), nodename);
      else
        window_clear_echo_area ();

      return;
    }

  /* If there are no more nodes left anywhere to view, complain and exit. */
  if (info_windows_index == 1 && info_windows[0]->nodes_index == 1)
    {
      info_error (_("Cannot kill the last node"));
      return;
    }

  /* INFO_WIN contains the node that the user wants to stop viewing.  Delete
     this node from the list of nodes previously shown in this window. */
  for (i = info_win->current; i < info_win->nodes_index; i++)
    info_win->nodes[i] = info_win->nodes[i + 1];

  /* There is one less node in this window's history list. */
  info_win->nodes_index--;

  /* Make this window show the most recent history node. */
  info_win->current = info_win->nodes_index - 1;

  /* If there aren't any nodes left in this window, steal one from the
     next window. */
  if (info_win->current < 0)
    {
      INFO_WINDOW *stealer;
      int which, pagetop;
      long point;

      if (info_windows[iw + 1])
        stealer = info_windows[iw + 1];
      else
        stealer = info_windows[0];

      /* If the node being displayed in the next window is not the most
         recently loaded one, get the most recently loaded one. */
      if ((stealer->nodes_index - 1) != stealer->current)
        which = stealer->nodes_index - 1;

      /* Else, if there is another node behind the stealers current node,
         use that one. */
      else if (stealer->current > 0)
        which = stealer->current - 1;

      /* Else, just use the node appearing in STEALER's window. */
      else
        which = stealer->current;

      /* Copy this node. */
      {
        NODE *copy = xmalloc (sizeof (NODE));

        temp = stealer->nodes[which];
        point = stealer->points[which];
        pagetop = stealer->pagetops[which];

        copy->filename = temp->filename;
        copy->parent = temp->parent;
        copy->nodename = temp->nodename;
        copy->contents = temp->contents;
        copy->nodelen = temp->nodelen;
        copy->flags = temp->flags;
        copy->display_pos = temp->display_pos;

        temp = copy;
      }

      window_set_node_of_window (info_win->window, temp);
      window->point = point;
      window->pagetop = pagetop;
      remember_window_and_node (info_win->window, temp);
    }
  else
    {
      temp = info_win->nodes[info_win->current];
      temp->display_pos = info_win->points[info_win->current];
      window_set_node_of_window (info_win->window, temp);
    }

  if (!info_error_was_printed)
    window_clear_echo_area ();

  if (auto_footnotes_p)
    info_get_or_remove_footnotes (window);
}

/* Kill current node, thus going back one in the node history.  I (karl)
   do not think this is completely correct yet, because of the
   window-changing stuff in kill_node, but it's a lot better than the
   previous implementation, which did not account for nodes being
   visited twice at all.  */
DECLARE_INFO_COMMAND (info_history_node,
                      _("Select the most recently selected node"))
{
  kill_node (window, active_window->node->nodename);
}

/* Kill named node.  */
DECLARE_INFO_COMMAND (info_kill_node, _("Kill this node"))
{
  char *nodename = read_nodename_to_kill (window);
  kill_node (window, nodename);
}


/* Read the name of a file and select the entire file. */
DECLARE_INFO_COMMAND (info_view_file, _("Read the name of a file and select it"))
{
  char *line;

  line = info_read_in_echo_area (window, _("Find file: "));
  if (!line)
    {
      info_abort_key (active_window, 1, 0);
      return;
    }

  if (*line)
    {
      NODE *node;

      node = info_get_node (line, "*");
      if (!node)
        {
          if (info_recent_file_error)
            info_error (info_recent_file_error);
          else
            info_error (_("Cannot find `%s'."), line);
        }
      else
        info_set_node_of_window (1, window, node);

      free (line);
    }

  if (!info_error_was_printed)
    window_clear_echo_area ();
}

/* **************************************************************** */
/*                                                                  */
/*                 Dumping and Printing Nodes                       */
/*                                                                  */
/* **************************************************************** */

#define VERBOSE_NODE_DUMPING
static void write_node_to_stream ();
static void dump_node_to_stream ();
static void initialize_dumping ();

/* Dump the nodes specified by FILENAME and NODENAMES to the file named
   in OUTPUT_FILENAME.  If DUMP_SUBNODES is non-zero, recursively dump
   the nodes which appear in the menu of each node dumped. */
void
dump_nodes_to_file (filename, nodenames, output_filename, dump_subnodes)
     char *filename;
     char **nodenames;
     char *output_filename;
     int dump_subnodes;
{
  register int i;
  FILE *output_stream;

  /* Get the stream to print the nodes to.  Special case of an output
     filename of "-" means to dump the nodes to stdout. */
  if (strcmp (output_filename, "-") == 0)
    output_stream = stdout;
  else
    output_stream = fopen (output_filename, "w");

  if (!output_stream)
    {
      info_error (_("Could not create output file `%s'."), output_filename);
      return;
    }

  /* Print each node to stream. */
  initialize_dumping ();
  for (i = 0; nodenames[i]; i++)
    dump_node_to_stream (filename, nodenames[i], output_stream, dump_subnodes);

  if (output_stream != stdout)
    fclose (output_stream);

#if defined (VERBOSE_NODE_DUMPING)
  info_error (_("Done."));
#endif /* VERBOSE_NODE_DUMPING */
}

/* A place to remember already dumped nodes. */
static char **dumped_already = (char **)NULL;
static int dumped_already_index = 0;
static int dumped_already_slots = 0;

static void
initialize_dumping ()
{
  dumped_already_index = 0;
}

/* Get and print the node specified by FILENAME and NODENAME to STREAM.
   If DUMP_SUBNODES is non-zero, recursively dump the nodes which appear
   in the menu of each node dumped. */
static void
dump_node_to_stream (filename, nodename, stream, dump_subnodes)
     char *filename, *nodename;
     FILE *stream;
     int dump_subnodes;
{
  register int i;
  NODE *node;

  node = info_get_node (filename, nodename);

  if (!node)
    {
      if (info_recent_file_error)
        info_error (info_recent_file_error);
      else
        {
          if (filename && *nodename != '(')
            info_error (msg_cant_file_node, filename_non_directory (filename),
                        nodename);
          else
            info_error (msg_cant_find_node, nodename);
        }
      return;
    }

  /* If we have already dumped this node, don't dump it again. */
  for (i = 0; i < dumped_already_index; i++)
    if (strcmp (node->nodename, dumped_already[i]) == 0)
      {
        free (node);
        return;
      }
  add_pointer_to_array (node->nodename, dumped_already_index, dumped_already,
                        dumped_already_slots, 50, char *);

#if defined (VERBOSE_NODE_DUMPING)
  /* Maybe we should print some information about the node being output. */
  info_error (_("Writing node %s..."), node_printed_rep (node));
#endif /* VERBOSE_NODE_DUMPING */

  write_node_to_stream (node, stream);

  /* If we are dumping subnodes, get the list of menu items in this node,
     and dump each one recursively. */
  if (dump_subnodes)
    {
      REFERENCE **menu = (REFERENCE **)NULL;

      /* If this node is an Index, do not dump the menu references. */
      if (string_in_line ("Index", node->nodename) == -1)
        menu = info_menu_of_node (node);

      if (menu)
        {
          for (i = 0; menu[i]; i++)
            {
              /* We don't dump Info files which are different than the
                 current one. */
              if (!menu[i]->filename)
                dump_node_to_stream
                  (filename, menu[i]->nodename, stream, dump_subnodes);
            }
          info_free_references (menu);
        }
    }

  free (node);
}

/* Dump NODE to FILENAME.  If DUMP_SUBNODES is non-zero, recursively dump
   the nodes which appear in the menu of each node dumped. */
void
dump_node_to_file (node, filename, dump_subnodes)
     NODE *node;
     char *filename;
     int dump_subnodes;
{
  FILE *output_stream;
  char *nodes_filename;

  /* Get the stream to print this node to.  Special case of an output
     filename of "-" means to dump the nodes to stdout. */
  if (strcmp (filename, "-") == 0)
    output_stream = stdout;
  else
    output_stream = fopen (filename, "w");

  if (!output_stream)
    {
      info_error (_("Could not create output file `%s'."), filename);
      return;
    }

  if (node->parent)
    nodes_filename = node->parent;
  else
    nodes_filename = node->filename;

  initialize_dumping ();
  dump_node_to_stream
    (nodes_filename, node->nodename, output_stream, dump_subnodes);

  if (output_stream != stdout)
    fclose (output_stream);

#if defined (VERBOSE_NODE_DUMPING)
  info_error (_("Done."));
#endif /* VERBOSE_NODE_DUMPING */
}

#if !defined (DEFAULT_INFO_PRINT_COMMAND)
#  define DEFAULT_INFO_PRINT_COMMAND "lpr"
#endif /* !DEFAULT_INFO_PRINT_COMMAND */

DECLARE_INFO_COMMAND (info_print_node,
 _("Pipe the contents of this node through INFO_PRINT_COMMAND"))
{
  print_node (window->node);
}

/* Print NODE on a printer piping it into INFO_PRINT_COMMAND. */
void
print_node (node)
     NODE *node;
{
  FILE *printer_pipe;
  char *print_command = getenv ("INFO_PRINT_COMMAND");
  int piping = 0;

  if (!print_command || !*print_command)
    print_command = DEFAULT_INFO_PRINT_COMMAND;

  /* Note that on MS-DOS/MS-Windows, this MUST open the pipe in the
     (default) text mode, since the printer drivers there need to see
     DOS-style CRLF pairs at the end of each line.

     FIXME: if we are to support Mac-style text files, we might need
     to convert the text here.  */

  /* INFO_PRINT_COMMAND which says ">file" means write to that file.
     Presumably, the name of the file is the local printer device.  */
  if (*print_command == '>')
    printer_pipe = fopen (++print_command, "w");
  else
    {
      printer_pipe = popen (print_command, "w");
      piping = 1;
    }

  if (!printer_pipe)
    {
      info_error (_("Cannot open pipe to `%s'."), print_command);
      return;
    }

#if defined (VERBOSE_NODE_DUMPING)
  /* Maybe we should print some information about the node being output. */
  info_error (_("Printing node %s..."), node_printed_rep (node));
#endif /* VERBOSE_NODE_DUMPING */

  write_node_to_stream (node, printer_pipe);
  if (piping)
    pclose (printer_pipe);
  else
    fclose (printer_pipe);

#if defined (VERBOSE_NODE_DUMPING)
  info_error (_("Done."));
#endif /* VERBOSE_NODE_DUMPING */
}

static void
write_node_to_stream (node, stream)
     NODE *node;
     FILE *stream;
{
  fwrite (node->contents, 1, node->nodelen, stream);
}

/* **************************************************************** */
/*                                                                  */
/*                    Info Searching Commands                       */
/*                                                                  */
/* **************************************************************** */

/* Variable controlling the garbage collection of files briefly visited
   during searches.  Such files are normally gc'ed, unless they were
   compressed to begin with.  If this variable is non-zero, it says
   to gc even those file buffer contents which had to be uncompressed. */
int gc_compressed_files = 0;

static void info_gc_file_buffers ();
static void info_search_1 ();

static char *search_string = (char *)NULL;
static int search_string_index = 0;
static int search_string_size = 0;
static int isearch_is_active = 0;

static int last_search_direction = 0;
static int last_search_case_sensitive = 0;

/* Return the file buffer which belongs to WINDOW's node. */
FILE_BUFFER *
file_buffer_of_window (window)
     WINDOW *window;
{
  /* If this window has no node, then it has no file buffer. */
  if (!window->node)
    return ((FILE_BUFFER *)NULL);

  if (window->node->parent)
    return (info_find_file (window->node->parent));

  if (window->node->filename)
    return (info_find_file (window->node->filename));

  return ((FILE_BUFFER *)NULL);
}

/* Search for STRING in NODE starting at START.  Return -1 if the string
   was not found, or the location of the string if it was.  If WINDOW is
   passed as non-null, set the window's node to be NODE, its point to be
   the found string, and readjust the window's pagetop.  Final argument
   DIR says which direction to search in.  If it is positive, search
   forward, else backwards. */
long
info_search_in_node (string, node, start, window, dir, case_sensitive)
     char *string;
     NODE *node;
     long start;
     WINDOW *window;
     int dir, case_sensitive;
{
  SEARCH_BINDING binding;
  long offset;

  binding.buffer = node->contents;
  binding.start = start;
  binding.end = node->nodelen;
  binding.flags = 0;
  if (!case_sensitive)
    binding.flags |= S_FoldCase;

  if (dir < 0)
    {
      binding.end = 0;
      binding.flags |= S_SkipDest;
    }

  if (binding.start < 0)
    return (-1);

  /* For incremental searches, we always wish to skip past the string. */
  if (isearch_is_active)
    binding.flags |= S_SkipDest;

  offset = search (string, &binding);

  if (offset != -1 && window)
    {
      set_remembered_pagetop_and_point (window);
      if (window->node != node)
        window_set_node_of_window (window, node);
      window->point = offset;
      window_adjust_pagetop (window);
    }
  return (offset);
}

/* Search NODE, looking for the largest possible match of STRING.  Start the
   search at START.  Return the absolute position of the match, or -1, if
   no part of the string could be found. */
long
info_target_search_node (node, string, start)
     NODE *node;
     char *string;
     long start;
{
  register int i;
  long offset;
  char *target;

  target = xstrdup (string);
  i = strlen (target);

  /* Try repeatedly searching for this string while removing words from
     the end of it. */
  while (i)
    {
      target[i] = '\0';
      offset = info_search_in_node (target, node, start, (WINDOW *)NULL, 1, 0);

      if (offset != -1)
        break;

      /* Delete the last word from TARGET. */
      for (; i && (!whitespace (target[i]) && (target[i] != ',')); i--);
    }
  free (target);
  return (offset);
}

/* Search for STRING starting in WINDOW at point.  If the string is found
   in this node, set point to that position.  Otherwise, get the file buffer
   associated with WINDOW's node, and search through each node in that file.
   If the search fails, return non-zero, else zero.  Side-effect window
   leaving the node and point where the string was found current. */
static int
info_search_internal (string, window, dir, case_sensitive)
     char *string;
     WINDOW *window;
     int dir, case_sensitive;
{
  register int i;
  FILE_BUFFER *file_buffer;
  char *initial_nodename;
  long ret, start = 0;

  file_buffer = file_buffer_of_window (window);
  initial_nodename = window->node->nodename;

  /* This used to begin from window->point, unless this was a repeated
     search command.  But invoking search with an argument loses with
     that logic, since info_last_executed_command is then set to
     info_add_digit_to_numeric_arg.  I think there's no sense in
     ``finding'' a string that is already under the cursor, anyway.  */
  ret = info_search_in_node
        (string, window->node, window->point + dir, window, dir,
         case_sensitive);

  if (ret != -1)
    {
      /* We won! */
      if (!echo_area_is_active && !isearch_is_active)
        window_clear_echo_area ();
      return (0);
    }

  /* The string wasn't found in the current node.  Search through the
     window's file buffer, iff the current node is not "*". */
  if (!file_buffer || (strcmp (initial_nodename, "*") == 0))
    return (-1);

  /* If this file has tags, search through every subfile, starting at
     this node's subfile and node.  Otherwise, search through the
     file's node list. */
  if (file_buffer->tags)
    {
      register int current_tag, number_of_tags;
      char *last_subfile;
      TAG *tag;

      /* Find number of tags and current tag. */
      last_subfile = (char *)NULL;
      for (i = 0; file_buffer->tags[i]; i++)
        if (strcmp (initial_nodename, file_buffer->tags[i]->nodename) == 0)
          {
            current_tag = i;
            last_subfile = file_buffer->tags[i]->filename;
          }

      number_of_tags = i;

      /* If there is no last_subfile, our tag wasn't found. */
      if (!last_subfile)
        return (-1);

      /* Search through subsequent nodes, wrapping around to the top
         of the info file until we find the string or return to this
         window's node and point. */
      while (1)
        {
          NODE *node;

          /* Allow C-g to quit the search, failing it if pressed. */
          return_if_control_g (-1);

          /* Find the next tag that isn't an anchor.  */
          for (i = current_tag + dir; i != current_tag; i += dir)
            {
              if (i < 0)
                i = number_of_tags - 1;
              else if (i == number_of_tags)
                i = 0;

              tag = file_buffer->tags[i];
              if (tag->nodelen != 0)
                break;
            }

          /* If we got past out starting point, bail out.  */
          if (i == current_tag)
            return (-1);
          current_tag = i;

          if (!echo_area_is_active && (last_subfile != tag->filename))
            {
              window_message_in_echo_area
                (_("Searching subfile %s ..."),
                 filename_non_directory (tag->filename));

              last_subfile = tag->filename;
            }

          node = info_get_node (file_buffer->filename, tag->nodename);

          if (!node)
            {
              /* If not doing i-search... */
              if (!echo_area_is_active)
                {
                  if (info_recent_file_error)
                    info_error (info_recent_file_error);
                  else
                    info_error (msg_cant_file_node,
                                filename_non_directory (file_buffer->filename),
                                tag->nodename);
                }
              return (-1);
            }

          if (dir < 0)
            start = tag->nodelen;

          ret =
            info_search_in_node (string, node, start, window, dir,
                                 case_sensitive);

          /* Did we find the string in this node? */
          if (ret != -1)
            {
              /* Yes!  We win. */
              remember_window_and_node (window, node);
              if (!echo_area_is_active)
                window_clear_echo_area ();
              return (0);
            }

          /* No.  Free this node, and make sure that we haven't passed
             our starting point. */
          free (node);

          if (strcmp (initial_nodename, tag->nodename) == 0)
            return (-1);
        }
    }
  return (-1);
}

DECLARE_INFO_COMMAND (info_search_case_sensitively,
                      _("Read a string and search for it case-sensitively"))
{
  last_search_direction = count > 0 ? 1 : -1;
  last_search_case_sensitive = 1;
  info_search_1 (window, count, key, 1, 1);
}

DECLARE_INFO_COMMAND (info_search, _("Read a string and search for it"))
{
  last_search_direction = count > 0 ? 1 : -1;
  last_search_case_sensitive = 0;
  info_search_1 (window, count, key, 0, 1);
}

DECLARE_INFO_COMMAND (info_search_backward,
		      _("Read a string and search backward for it"))
{
  last_search_direction = count > 0 ? -1 : 1;
  last_search_case_sensitive = 0;
  info_search_1 (window, -count, key, 0, 1);
}

static void
info_search_1 (window, count, key, case_sensitive, ask_for_string)
     WINDOW *window;
     int count;
     unsigned char key;
     int case_sensitive;
     int ask_for_string;
{
  char *line, *prompt;
  int result, old_pagetop;
  int direction;

  if (count < 0)
    {
      direction = -1;
      count = -count;
    }
  else
    {
      direction = 1;
      if (count == 0)
        count = 1;	/* for backward compatibility */
    }

  /* Read a string from the user, defaulting the search to SEARCH_STRING. */
  if (!search_string)
    {
      search_string = (char *)xmalloc (search_string_size = 100);
      search_string[0] = '\0';
    }

  if (ask_for_string)
    {
      prompt = (char *)xmalloc (50 + strlen (search_string));

      sprintf (prompt, _("%s%sfor string [%s]: "),
               direction < 0 ? _("Search backward") : _("Search"),
               case_sensitive ? _(" case-sensitively ") : _(" "),
               search_string);

      line = info_read_in_echo_area (window, prompt);
      free (prompt);

      if (!line)
        {
          info_abort_key ();
          return;
        }

      if (*line)
        {
          if (strlen (line) + 1 > search_string_size)
            search_string = (char *) xrealloc
              (search_string, (search_string_size += 50 + strlen (line)));

          strcpy (search_string, line);
          search_string_index = strlen (line);
          free (line);
        }
    }

  /* If the search string includes upper-case letters, make the search
     case-sensitive.  */
  if (case_sensitive == 0)
    for (line = search_string; *line; line++)
      if (isupper (*line))
        {
          case_sensitive = 1;
          break;
        }

  old_pagetop = active_window->pagetop;
  for (result = 0; result == 0 && count--; )
    result = info_search_internal (search_string,
                                   active_window, direction, case_sensitive);

  if (result != 0 && !info_error_was_printed)
    info_error (_("Search failed."));
  else if (old_pagetop != active_window->pagetop)
    {
      int new_pagetop;

      new_pagetop = active_window->pagetop;
      active_window->pagetop = old_pagetop;
      set_window_pagetop (active_window, new_pagetop);
      if (auto_footnotes_p)
        info_get_or_remove_footnotes (active_window);
    }

  /* Perhaps free the unreferenced file buffers that were searched, but
     not retained. */
  info_gc_file_buffers ();
}

DECLARE_INFO_COMMAND (info_search_next,
		      _("Repeat last search in the same direction"))
{
  if (!last_search_direction)
    info_error (_("No previous search string"));
  else
    info_search_1 (window, last_search_direction * count,
		   key, last_search_case_sensitive, 0);
}

DECLARE_INFO_COMMAND (info_search_previous,
		      _("Repeat last search in the reverse direction"))
{
  if (!last_search_direction)
    info_error (_("No previous search string"));
  else
    info_search_1 (window, -last_search_direction * count,
		   key, last_search_case_sensitive, 0);
}

/* **************************************************************** */
/*                                                                  */
/*                      Incremental Searching                       */
/*                                                                  */
/* **************************************************************** */

static void incremental_search ();

DECLARE_INFO_COMMAND (isearch_forward,
                      _("Search interactively for a string as you type it"))
{
  incremental_search (window, count, key);
}

DECLARE_INFO_COMMAND (isearch_backward,
                      _("Search interactively for a string as you type it"))
{
  incremental_search (window, -count, key);
}

/* Incrementally search for a string as it is typed. */
/* The last accepted incremental search string. */
static char *last_isearch_accepted = (char *)NULL;

/* The current incremental search string. */
static char *isearch_string = (char *)NULL;
static int isearch_string_index = 0;
static int isearch_string_size = 0;
static unsigned char isearch_terminate_search_key = ESC;

/* Structure defining the current state of an incremental search. */
typedef struct {
  WINDOW_STATE_DECL;    /* The node, pagetop and point. */
  int search_index;     /* Offset of the last char in the search string. */
  int direction;        /* The direction that this search is heading in. */
  int failing;          /* Whether or not this search failed. */
} SEARCH_STATE;

/* Array of search states. */
static SEARCH_STATE **isearch_states = (SEARCH_STATE **)NULL;
static int isearch_states_index = 0;
static int isearch_states_slots = 0;

/* Push the state of this search. */
static void
push_isearch (window, search_index, direction, failing)
     WINDOW *window;
     int search_index, direction, failing;
{
  SEARCH_STATE *state;

  state = (SEARCH_STATE *)xmalloc (sizeof (SEARCH_STATE));
  window_get_state (window, state);
  state->search_index = search_index;
  state->direction = direction;
  state->failing = failing;

  add_pointer_to_array (state, isearch_states_index, isearch_states,
                        isearch_states_slots, 20, SEARCH_STATE *);
}

/* Pop the state of this search to WINDOW, SEARCH_INDEX, and DIRECTION. */
static void
pop_isearch (window, search_index, direction, failing)
     WINDOW *window;
     int *search_index, *direction, *failing;
{
  SEARCH_STATE *state;

  if (isearch_states_index)
    {
      isearch_states_index--;
      state = isearch_states[isearch_states_index];
      window_set_state (window, state);
      *search_index = state->search_index;
      *direction = state->direction;
      *failing = state->failing;

      free (state);
      isearch_states[isearch_states_index] = (SEARCH_STATE *)NULL;
    }
}

/* Free the memory used by isearch_states. */
static void
free_isearch_states ()
{
  register int i;

  for (i = 0; i < isearch_states_index; i++)
    {
      free (isearch_states[i]);
      isearch_states[i] = (SEARCH_STATE *)NULL;
    }
  isearch_states_index = 0;
}

/* Display the current search in the echo area. */
static void
show_isearch_prompt (dir, string, failing_p)
     int dir;
     unsigned char *string;
     int failing_p;
{
  register int i;
  const char *prefix;
  char *prompt, *p_rep;
  int prompt_len, p_rep_index, p_rep_size;

  if (dir < 0)
    prefix = _("I-search backward: ");
  else
    prefix = _("I-search: ");

  p_rep_index = p_rep_size = 0;
  p_rep = (char *)NULL;
  for (i = 0; string[i]; i++)
    {
      char *rep;

      switch (string[i])
        {
        case ' ': rep = " "; break;
        case LFD: rep = "\\n"; break;
        case TAB: rep = "\\t"; break;
        default:
          rep = pretty_keyname (string[i]);
        }
      if ((p_rep_index + strlen (rep) + 1) >= p_rep_size)
        p_rep = (char *)xrealloc (p_rep, p_rep_size += 100);

      strcpy (p_rep + p_rep_index, rep);
      p_rep_index += strlen (rep);
    }

  prompt_len = strlen (prefix) + p_rep_index + 1;
  if (failing_p)
    prompt_len += strlen (_("Failing "));
  prompt = xmalloc (prompt_len);
  sprintf (prompt, "%s%s%s", failing_p ? _("Failing ") : "", prefix,
           p_rep ? p_rep : "");

  window_message_in_echo_area ("%s", prompt);
  maybe_free (p_rep);
  free (prompt);
  display_cursor_at_point (active_window);
}

static void
incremental_search (window, count, ignore)
     WINDOW *window;
     int count;
     unsigned char ignore;
{
  unsigned char key;
  int last_search_result, search_result, dir;
  SEARCH_STATE mystate, orig_state;
  char *p;
  int case_sensitive = 0;

  if (count < 0)
    dir = -1;
  else
    dir = 1;

  last_search_result = search_result = 0;

  window_get_state (window, &orig_state);

  isearch_string_index = 0;
  if (!isearch_string_size)
    isearch_string = (char *)xmalloc (isearch_string_size = 50);

  /* Show the search string in the echo area. */
  isearch_string[isearch_string_index] = '\0';
  show_isearch_prompt (dir, isearch_string, search_result);

  isearch_is_active = 1;

  while (isearch_is_active)
    {
      VFunction *func = (VFunction *)NULL;
      int quoted = 0;

      /* If a recent display was interrupted, then do the redisplay now if
         it is convenient. */
      if (!info_any_buffered_input_p () && display_was_interrupted_p)
        {
          display_update_one_window (window);
          display_cursor_at_point (active_window);
        }

      /* Read a character and dispatch on it. */
      key = info_get_input_char ();
      window_get_state (window, &mystate);

      if (key == DEL || key == Control ('h'))
        {
          /* User wants to delete one level of search? */
          if (!isearch_states_index)
            {
              terminal_ring_bell ();
              continue;
            }
          else
            {
              pop_isearch
                (window, &isearch_string_index, &dir, &search_result);
              isearch_string[isearch_string_index] = '\0';
              show_isearch_prompt (dir, isearch_string, search_result);
              goto after_search;
            }
        }
      else if (key == Control ('q'))
        {
          key = info_get_input_char ();
          quoted = 1;
        }

      /* We are about to search again, or quit.  Save the current search. */
      push_isearch (window, isearch_string_index, dir, search_result);

      if (quoted)
        goto insert_and_search;

      if (!Meta_p (key) || key > 32)
        {
          /* If this key is not a keymap, get its associated function,
             if any.  If it is a keymap, then it's probably ESC from an
             arrow key, and we handle that case below.  */
          char type = window->keymap[key].type;
          func = type == ISFUNC
                 ? InfoFunction(window->keymap[key].function)
                 : NULL;  /* function member is a Keymap if ISKMAP */

          if (isprint (key) || (type == ISFUNC && func == NULL))
            {
            insert_and_search:

              if (isearch_string_index + 2 >= isearch_string_size)
                isearch_string = (char *)xrealloc
                  (isearch_string, isearch_string_size += 100);

              isearch_string[isearch_string_index++] = key;
              isearch_string[isearch_string_index] = '\0';
              goto search_now;
            }
          else if (func == isearch_forward || func == isearch_backward)
            {
	      /* If this key invokes an incremental search, then this
		 means that we will either search again in the same
		 direction, search again in the reverse direction, or
		 insert the last search string that was accepted through
		 incremental searching. */
              if ((func == isearch_forward && dir > 0) ||
                  (func == isearch_backward && dir < 0))
                {
                  /* If the user has typed no characters, then insert the
                     last successful search into the current search string. */
                  if (isearch_string_index == 0)
                    {
                      /* Of course, there must be something to insert. */
                      if (last_isearch_accepted)
                        {
                          if (strlen (last_isearch_accepted) + 1 >=
                              isearch_string_size)
                            isearch_string = (char *)
                              xrealloc (isearch_string,
                                        isearch_string_size += 10 +
                                        strlen (last_isearch_accepted));
                          strcpy (isearch_string, last_isearch_accepted);
                          isearch_string_index = strlen (isearch_string);
                          goto search_now;
                        }
                      else
                        continue;
                    }
                  else
                    {
                      /* Search again in the same direction.  This means start
                         from a new place if the last search was successful. */
                      if (search_result == 0)
                        window->point += dir;
                    }
                }
              else
                {
                  /* Reverse the direction of the search. */
                  dir = -dir;
                }
            }
          else if (func == info_abort_key)
            {
              /* If C-g pressed, and the search is failing, pop the search
                 stack back to the last unfailed search. */
              if (isearch_states_index && (search_result != 0))
                {
                  terminal_ring_bell ();
                  while (isearch_states_index && (search_result != 0))
                    pop_isearch
                      (window, &isearch_string_index, &dir, &search_result);
                  isearch_string[isearch_string_index] = '\0';
                  show_isearch_prompt (dir, isearch_string, search_result);
                  continue;
                }
              else
                goto exit_search;
            }
          else
            goto exit_search;
        }
      else
        {
        exit_search:
          /* The character is not printable, or it has a function which is
             non-null.  Exit the search, remembering the search string.  If
             the key is not the same as the isearch_terminate_search_key,
             then push it into pending input. */
          if (isearch_string_index && func != info_abort_key)
            {
              maybe_free (last_isearch_accepted);
              last_isearch_accepted = xstrdup (isearch_string);
            }

	  /* If the key is the isearch_terminate_search_key, but some buffered
	     input is pending, it is almost invariably because the ESC key is
	     actually the beginning of an escape sequence, like in case they
	     pressed an arrow key.  So don't gobble the ESC key, push it back
	     into pending input.  */
	  /* FIXME: this seems like a kludge!  We need a more reliable
	     mechanism to know when ESC is a separate key and when it is
	     part of an escape sequence.  */
          if (key != RET  /* Emacs addicts want RET to get lost */
	      && (key != isearch_terminate_search_key
		  || info_any_buffered_input_p ()))
            info_set_pending_input (key);

          if (func == info_abort_key)
            {
              if (isearch_states_index)
                window_set_state (window, &orig_state);
            }

          if (!echo_area_is_active)
            window_clear_echo_area ();

          if (auto_footnotes_p)
            info_get_or_remove_footnotes (active_window);

          isearch_is_active = 0;
          continue;
        }

      /* Search for the contents of isearch_string. */
    search_now:
      show_isearch_prompt (dir, isearch_string, search_result);

      /* If the search string includes upper-case letters, make the
         search case-sensitive.  */
      for (p = isearch_string; *p; p++)
        if (isupper (*p))
          {
            case_sensitive = 1;
            break;
          }
      

      if (search_result == 0)
        {
          /* Check to see if the current search string is right here.  If
             we are looking at it, then don't bother calling the search
             function. */
          if (((dir < 0) &&
	       ((case_sensitive ? strncmp : strncasecmp)
                            (window->node->contents + window->point,
                             isearch_string, isearch_string_index) == 0)) ||
              ((dir > 0) &&
               ((window->point - isearch_string_index) >= 0) &&
	       ((case_sensitive ? strncmp : strncasecmp)
                            (window->node->contents +
                             (window->point - (isearch_string_index - 1)),
                             isearch_string, isearch_string_index) == 0)))
            {
              if (dir > 0)
                window->point++;
            }
          else
            search_result = info_search_internal (isearch_string,
						  window, dir, case_sensitive);
        }

      /* If this search failed, and we didn't already have a failed search,
         then ring the terminal bell. */
      if (search_result != 0 && last_search_result == 0)
        terminal_ring_bell ();

    after_search:
      show_isearch_prompt (dir, isearch_string, search_result);

      if (search_result == 0)
        {
          if ((mystate.node == window->node) &&
              (mystate.pagetop != window->pagetop))
            {
              int newtop = window->pagetop;
              window->pagetop = mystate.pagetop;
              set_window_pagetop (window, newtop);
            }
          display_update_one_window (window);
          display_cursor_at_point (window);
        }

      last_search_result = search_result;
    }

  /* Free the memory used to remember each search state. */
  free_isearch_states ();

  /* Perhaps GC some file buffers. */
  info_gc_file_buffers ();

  /* After searching, leave the window in the correct state. */
  if (!echo_area_is_active)
    window_clear_echo_area ();
}

/* GC some file buffers.  A file buffer can be gc-ed if there we have
   no nodes in INFO_WINDOWS that reference this file buffer's contents.
   Garbage collecting a file buffer means to free the file buffers
   contents. */
static void
info_gc_file_buffers ()
{
  register int fb_index, iw_index, i;
  register FILE_BUFFER *fb;
  register INFO_WINDOW *iw;

  if (!info_loaded_files)
    return;

  for (fb_index = 0; (fb = info_loaded_files[fb_index]); fb_index++)
    {
      int fb_referenced_p = 0;

      /* If already gc-ed, do nothing. */
      if (!fb->contents)
        continue;

      /* If this file had to be uncompressed, check to see if we should
         gc it.  This means that the user-variable "gc-compressed-files"
         is non-zero. */
      if ((fb->flags & N_IsCompressed) && !gc_compressed_files)
        continue;

      /* If this file's contents are not gc-able, move on. */
      if (fb->flags & N_CannotGC)
        continue;

      /* Check each INFO_WINDOW to see if it has any nodes which reference
         this file. */
      for (iw_index = 0; (iw = info_windows[iw_index]); iw_index++)
        {
          for (i = 0; iw->nodes && iw->nodes[i]; i++)
            {
              if ((FILENAME_CMP (fb->fullpath, iw->nodes[i]->filename) == 0) ||
                  (FILENAME_CMP (fb->filename, iw->nodes[i]->filename) == 0))
                {
                  fb_referenced_p = 1;
                  break;
                }
            }
        }

      /* If this file buffer wasn't referenced, free its contents. */
      if (!fb_referenced_p)
        {
          free (fb->contents);
          fb->contents = (char *)NULL;
        }
    }
}

/* **************************************************************** */
/*                                                                  */
/*                Traversing and Selecting References               */
/*                                                                  */
/* **************************************************************** */

/* Move to the next or previous cross reference in this node. */
static void
info_move_to_xref (window, count, key, dir)
     WINDOW *window;
     int count;
     unsigned char key;
     int dir;
{
  long firstmenu, firstxref;
  long nextmenu, nextxref;
  long placement = -1;
  long start = 0;
  NODE *node = window->node;

  if (dir < 0)
    start = node->nodelen;

  /* This search is only allowed to fail if there is no menu or cross
     reference in the current node.  Otherwise, the first menu or xref
     found is moved to. */

  firstmenu = info_search_in_node
    (INFO_MENU_ENTRY_LABEL, node, start, (WINDOW *)NULL, dir, 0);

  /* FIRSTMENU may point directly to the line defining the menu.  Skip that
     and go directly to the first item. */

  if (firstmenu != -1)
    {
      char *text = node->contents + firstmenu;

      if (strncmp (text, INFO_MENU_LABEL, strlen (INFO_MENU_LABEL)) == 0)
        firstmenu = info_search_in_node
          (INFO_MENU_ENTRY_LABEL, node, firstmenu + dir, (WINDOW *)NULL, dir, 0);
    }

  firstxref =
    info_search_in_node (INFO_XREF_LABEL, node, start, (WINDOW *)NULL, dir, 0);

#if defined (HANDLE_MAN_PAGES)
  if ((firstxref == -1) && (node->flags & N_IsManPage))
    {
      firstxref = locate_manpage_xref (node, start, dir);
    }
#endif /* HANDLE_MAN_PAGES */

  if (firstmenu == -1 && firstxref == -1)
    {
      info_error (msg_no_xref_node);
      return;
    }

  /* There is at least one cross reference or menu entry in this node.
     Try hard to find the next available one. */

  nextmenu = info_search_in_node
    (INFO_MENU_ENTRY_LABEL, node, window->point + dir, (WINDOW *)NULL, dir, 0);

  nextxref = info_search_in_node
    (INFO_XREF_LABEL, node, window->point + dir, (WINDOW *)NULL, dir, 0);

#if defined (HANDLE_MAN_PAGES)
  if ((nextxref == -1) && (node->flags & N_IsManPage) && (firstxref != -1))
    nextxref = locate_manpage_xref (node, window->point + dir, dir);
#endif /* HANDLE_MAN_PAGES */

  /* Ignore "Menu:" as a menu item. */
  if (nextmenu != -1)
    {
      char *text = node->contents + nextmenu;

      if (strncmp (text, INFO_MENU_LABEL, strlen (INFO_MENU_LABEL)) == 0)
        nextmenu = info_search_in_node
          (INFO_MENU_ENTRY_LABEL, node, nextmenu + dir, (WINDOW *)NULL, dir, 0);
    }

  /* If there is both a next menu entry, and a next xref entry, choose the
     one which occurs first.  Otherwise, select the one which actually
     appears in this node following point. */
  if (nextmenu != -1 && nextxref != -1)
    {
      if (((dir == 1) && (nextmenu < nextxref)) ||
          ((dir == -1) && (nextmenu > nextxref)))
        placement = nextmenu + 1;
      else
        placement = nextxref;
    }
  else if (nextmenu != -1)
    placement = nextmenu + 1;
  else if (nextxref != -1)
    placement = nextxref;

  /* If there was neither a menu or xref entry appearing in this node after
     point, choose the first menu or xref entry appearing in this node. */
  if (placement == -1)
    {
      if (firstmenu != -1 && firstxref != -1)
        {
          if (((dir == 1) && (firstmenu < firstxref)) ||
              ((dir == -1) && (firstmenu > firstxref)))
            placement = firstmenu + 1;
          else
            placement = firstxref;
        }
      else if (firstmenu != -1)
        placement = firstmenu + 1;
      else
        placement = firstxref;
    }
  window->point = placement;
  window_adjust_pagetop (window);
  window->flags |= W_UpdateWindow;
}

DECLARE_INFO_COMMAND (info_move_to_prev_xref,
                      _("Move to the previous cross reference"))
{
  if (count < 0)
    info_move_to_prev_xref (window, -count, key);
  else
    info_move_to_xref (window, count, key, -1);
}

DECLARE_INFO_COMMAND (info_move_to_next_xref,
                      _("Move to the next cross reference"))
{
  if (count < 0)
    info_move_to_next_xref (window, -count, key);
  else
    info_move_to_xref (window, count, key, 1);
}

/* Select the menu item or reference that appears on this line. */
DECLARE_INFO_COMMAND (info_select_reference_this_line,
                      _("Select reference or menu item appearing on this line"))
{
  char *line;
  NODE *orig;

  line = window->line_starts[window_line_of_point (window)];
  orig = window->node;

  /* If this line contains a menu item, select that one. */
  if (strncmp ("* ", line, 2) == 0)
    info_menu_or_ref_item (window, count, key, info_menu_of_node, 0);
  else
    info_menu_or_ref_item (window, count, key, info_xrefs_of_node, 0);
}

/* **************************************************************** */
/*                                                                  */
/*                  Miscellaneous Info Commands                     */
/*                                                                  */
/* **************************************************************** */

/* What to do when C-g is pressed in a window. */
DECLARE_INFO_COMMAND (info_abort_key, _("Cancel current operation"))
{
  /* If error printing doesn't oridinarily ring the bell, do it now,
     since C-g always rings the bell.  Otherwise, let the error printer
     do it. */
  if (!info_error_rings_bell_p)
    terminal_ring_bell ();
  info_error (_("Quit"));

  info_initialize_numeric_arg ();
  info_clear_pending_input ();
  info_last_executed_command = (VFunction *)NULL;
}

/* Move the cursor to the desired line of the window. */
DECLARE_INFO_COMMAND (info_move_to_window_line,
   _("Move the cursor to a specific line of the window"))
{
  int line;

  /* With no numeric argument of any kind, default to the center line. */
  if (!info_explicit_arg && count == 1)
    line = (window->height / 2) + window->pagetop;
  else
    {
      if (count < 0)
        line = (window->height + count) + window->pagetop;
      else
        line = window->pagetop + count;
    }

  /* If the line doesn't appear in this window, make it do so. */
  if ((line - window->pagetop) >= window->height)
    line = window->pagetop + (window->height - 1);

  /* If the line is too small, make it fit. */
  if (line < window->pagetop)
    line = window->pagetop;

  /* If the selected line is past the bottom of the node, force it back. */
  if (line >= window->line_count)
    line = window->line_count - 1;

  window->point = (window->line_starts[line] - window->node->contents);
}

/* Clear the screen and redraw its contents.  Given a numeric argument,
   move the line the cursor is on to the COUNT'th line of the window. */
DECLARE_INFO_COMMAND (info_redraw_display, _("Redraw the display"))
{
  if ((!info_explicit_arg && count == 1) || echo_area_is_active)
    {
      terminal_clear_screen ();
      display_clear_display (the_display);
      window_mark_chain (windows, W_UpdateWindow);
      display_update_display (windows);
    }
  else
    {
      int desired_line, point_line;
      int new_pagetop;

      point_line = window_line_of_point (window) - window->pagetop;

      if (count < 0)
        desired_line = window->height + count;
      else
        desired_line = count;

      if (desired_line < 0)
        desired_line = 0;

      if (desired_line >= window->height)
        desired_line = window->height - 1;

      if (desired_line == point_line)
        return;

      new_pagetop = window->pagetop + (point_line - desired_line);

      set_window_pagetop (window, new_pagetop);
    }
}
/* This command does nothing.  It is the fact that a key is bound to it
   that has meaning.  See the code at the top of info_session (). */
DECLARE_INFO_COMMAND (info_quit, _("Quit using Info"))
{}


/* **************************************************************** */
/*                                                                  */
/*               Reading Keys and Dispatching on Them               */
/*                                                                  */
/* **************************************************************** */

/* Declaration only.  Special cased in info_dispatch_on_key ().
   Doc string is to avoid ugly results with describe_key etc.  */
DECLARE_INFO_COMMAND (info_do_lowercase_version,
		      _("Run command bound to this key's lowercase variant"))
{}

static void
dispatch_error (keyseq)
     char *keyseq;
{
  char *rep;

  rep = pretty_keyseq (keyseq);

  if (!echo_area_is_active)
    info_error (_("Unknown command (%s)."), rep);
  else
    {
      char *temp = xmalloc (1 + strlen (rep) + strlen (_("\"\" is invalid")));
      sprintf (temp, _("\"%s\" is invalid"), rep);
      terminal_ring_bell ();
      inform_in_echo_area (temp);
      free (temp);
    }
}

/* Keeping track of key sequences. */
static char *info_keyseq = (char *)NULL;
static int info_keyseq_index = 0;
static int info_keyseq_size = 0;
static int info_keyseq_displayed_p = 0;

/* Initialize the length of the current key sequence. */
void
initialize_keyseq ()
{
  info_keyseq_index = 0;
  info_keyseq_displayed_p = 0;
}

/* Add CHARACTER to the current key sequence. */
void
add_char_to_keyseq (character)
     char character;
{
  if (info_keyseq_index + 2 >= info_keyseq_size)
    info_keyseq = (char *)xrealloc (info_keyseq, info_keyseq_size += 10);

  info_keyseq[info_keyseq_index++] = character;
  info_keyseq[info_keyseq_index] = '\0';
}

/* Display the current value of info_keyseq.  If argument EXPECTING is
   non-zero, input is expected to be read after the key sequence is
   displayed, so add an additional prompting character to the sequence. */
void
display_info_keyseq (expecting_future_input)
     int expecting_future_input;
{
  char *rep;

  rep = pretty_keyseq (info_keyseq);
  if (expecting_future_input)
    strcat (rep, "-");

  if (echo_area_is_active)
    inform_in_echo_area (rep);
  else
    {
      window_message_in_echo_area (rep);
      display_cursor_at_point (active_window);
    }
  info_keyseq_displayed_p = 1;
}

/* Called by interactive commands to read a keystroke. */
unsigned char
info_get_another_input_char ()
{
  int ready = !info_keyseq_displayed_p; /* ready if new and pending key */

  /* If there isn't any input currently available, then wait a
     moment looking for input.  If we don't get it fast enough,
     prompt a little bit with the current key sequence. */
  if (!info_keyseq_displayed_p)
    {
      ready = 1;
      if (!info_any_buffered_input_p () &&
          !info_input_pending_p ())
        {
#if defined (FD_SET)
          struct timeval timer;
          fd_set readfds;

          FD_ZERO (&readfds);
          FD_SET (fileno (info_input_stream), &readfds);
          timer.tv_sec = 1;
          timer.tv_usec = 750;
          ready = select (fileno(info_input_stream)+1, &readfds, (fd_set *)NULL, (fd_set *)NULL, &timer);
#else
          ready = 0;
#endif /* FD_SET */
      }
    }

  if (!ready)
    display_info_keyseq (1);

  return (info_get_input_char ());
}

/* Do the command associated with KEY in MAP.  If the associated command is
   really a keymap, then read another key, and dispatch into that map. */
void
info_dispatch_on_key (key, map)
     unsigned char key;
     Keymap map;
{
#if !defined(INFOKEY)
  if (Meta_p (key) && (!ISO_Latin_p || map[key].function != ea_insert))
    {
      if (map[ESC].type == ISKMAP)
        {
          map = (Keymap)map[ESC].function;
          add_char_to_keyseq (ESC);
          key = UnMeta (key);
          info_dispatch_on_key (key, map);
        }
      else
        {
          dispatch_error (info_keyseq);
        }
      return;
    }
#endif /* INFOKEY */

  switch (map[key].type)
    {
    case ISFUNC:
      {
        VFunction *func;

        func = InfoFunction(map[key].function);
        if (func != (VFunction *)NULL)
          {
            /* Special case info_do_lowercase_version (). */
            if (func == info_do_lowercase_version)
              {
#if defined(INFOKEY)
		unsigned char lowerkey;

		lowerkey = Meta_p(key) ? Meta (tolower (UnMeta (key))) : tolower (key);
		if (lowerkey == key)
		  {
		    add_char_to_keyseq (key);
		    dispatch_error (info_keyseq);
		    return;
		  }
                info_dispatch_on_key (lowerkey, map);
#else /* !INFOKEY */
                info_dispatch_on_key (tolower (key), map);
#endif /* INFOKEY */
                return;
              }

            add_char_to_keyseq (key);

            if (info_keyseq_displayed_p)
              display_info_keyseq (0);

            {
              WINDOW *where;

              where = active_window;
              (*InfoFunction(map[key].function))
                (active_window, info_numeric_arg * info_numeric_arg_sign, key);

              /* If we have input pending, then the last command was a prefix
                 command.  Don't change the value of the last function vars.
                 Otherwise, remember the last command executed in the var
                 appropriate to the window in which it was executed. */
              if (!info_input_pending_p ())
                {
                  if (where == the_echo_area)
                    ea_last_executed_command = InfoFunction(map[key].function);
                  else
                    info_last_executed_command = InfoFunction(map[key].function);
                }
            }
          }
        else
          {
            add_char_to_keyseq (key);
            dispatch_error (info_keyseq);
            return;
          }
      }
      break;

    case ISKMAP:
      add_char_to_keyseq (key);
      if (map[key].function != (InfoCommand *)NULL)
        {
          unsigned char newkey;

          newkey = info_get_another_input_char ();
          info_dispatch_on_key (newkey, (Keymap)map[key].function);
        }
      else
        {
          dispatch_error (info_keyseq);
          return;
        }
      break;
    }
}

/* **************************************************************** */
/*                                                                  */
/*                      Numeric Arguments                           */
/*                                                                  */
/* **************************************************************** */

/* Handle C-u style numeric args, as well as M--, and M-digits. */

/* Non-zero means that an explicit argument has been passed to this
   command, as in C-u C-v. */
int info_explicit_arg = 0;

/* The sign of the numeric argument. */
int info_numeric_arg_sign = 1;

/* The value of the argument itself. */
int info_numeric_arg = 1;

/* Add the current digit to the argument in progress. */
DECLARE_INFO_COMMAND (info_add_digit_to_numeric_arg,
                      _("Add this digit to the current numeric argument"))
{
  info_numeric_arg_digit_loop (window, 0, key);
}

/* C-u, universal argument.  Multiply the current argument by 4.
   Read a key.  If the key has nothing to do with arguments, then
   dispatch on it.  If the key is the abort character then abort. */
DECLARE_INFO_COMMAND (info_universal_argument,
                      _("Start (or multiply by 4) the current numeric argument"))
{
  info_numeric_arg *= 4;
  info_numeric_arg_digit_loop (window, 0, 0);
}

/* Create a default argument. */
void
info_initialize_numeric_arg ()
{
  info_numeric_arg = info_numeric_arg_sign = 1;
  info_explicit_arg = 0;
}

DECLARE_INFO_COMMAND (info_numeric_arg_digit_loop,
                      _("Internally used by \\[universal-argument]"))
{
  unsigned char pure_key;
  Keymap keymap = window->keymap;

  while (1)
    {
      if (key)
        pure_key = key;
      else
        {
          if (display_was_interrupted_p && !info_any_buffered_input_p ())
            display_update_display (windows);

          if (active_window != the_echo_area)
            display_cursor_at_point (active_window);

          pure_key = key = info_get_another_input_char ();

#if !defined(INFOKEY)
          if (Meta_p (key))
            add_char_to_keyseq (ESC);

          add_char_to_keyseq (UnMeta (key));
#else /* defined(INFOKEY) */
          add_char_to_keyseq (key);
#endif /* defined(INFOKEY) */
        }

#if !defined(INFOKEY)
      if (Meta_p (key))
        key = UnMeta (key);
#endif /* !defined(INFOKEY) */

      if (keymap[key].type == ISFUNC &&
          InfoFunction(keymap[key].function) == info_universal_argument)
        {
          info_numeric_arg *= 4;
          key = 0;
          continue;
        }

#if defined(INFOKEY)
      if (Meta_p (key))
        key = UnMeta (key);
#endif /* !defined(INFOKEY) */


      if (isdigit (key))
        {
          if (info_explicit_arg)
            info_numeric_arg = (info_numeric_arg * 10) + (key - '0');
          else
            info_numeric_arg = (key - '0');
          info_explicit_arg = 1;
        }
      else
        {
          if (key == '-' && !info_explicit_arg)
            {
              info_numeric_arg_sign = -1;
              info_numeric_arg = 1;
            }
          else
            {
              info_keyseq_index--;
              info_dispatch_on_key (pure_key, keymap);
              return;
            }
        }
      key = 0;
    }
}

/* **************************************************************** */
/*                                                                  */
/*                      Input Character Buffering                   */
/*                                                                  */
/* **************************************************************** */

/* Character waiting to be read next. */
static int pending_input_character = 0;

/* How to make there be no pending input. */
static void
info_clear_pending_input ()
{
  pending_input_character = 0;
}

/* How to set the pending input character. */
static void
info_set_pending_input (key)
     unsigned char key;
{
  pending_input_character = key;
}

/* How to see if there is any pending input. */
unsigned char
info_input_pending_p ()
{
  return (pending_input_character);
}

/* Largest number of characters that we can read in advance. */
#define MAX_INFO_INPUT_BUFFERING 512

static int pop_index = 0, push_index = 0;
static unsigned char info_input_buffer[MAX_INFO_INPUT_BUFFERING];

/* Add KEY to the buffer of characters to be read. */
static void
info_push_typeahead (key)
     unsigned char key;
{
  /* Flush all pending input in the case of C-g pressed. */
  if (key == Control ('g'))
    {
      push_index = pop_index;
      info_set_pending_input (Control ('g'));
    }
  else
    {
      info_input_buffer[push_index++] = key;
      if (push_index >= sizeof (info_input_buffer))
        push_index = 0;
    }
}

/* Return the amount of space available in INFO_INPUT_BUFFER for new chars. */
static int
info_input_buffer_space_available ()
{
  if (pop_index > push_index)
    return (pop_index - push_index);
  else
    return (sizeof (info_input_buffer) - (push_index - pop_index));
}

/* Get a key from the buffer of characters to be read.
   Return the key in KEY.
   Result is non-zero if there was a key, or 0 if there wasn't. */
static int
info_get_key_from_typeahead (key)
     unsigned char *key;
{
  if (push_index == pop_index)
    return (0);

  *key = info_input_buffer[pop_index++];

  if (pop_index >= sizeof (info_input_buffer))
    pop_index = 0;

  return (1);
}

int
info_any_buffered_input_p ()
{
  info_gather_typeahead ();
  return (push_index != pop_index);
}

/* If characters are available to be read, then read them and stuff them into
   info_input_buffer.  Otherwise, do nothing. */
void
info_gather_typeahead ()
{
  register int i = 0;
  int tty, space_avail;
  long chars_avail;
  unsigned char input[MAX_INFO_INPUT_BUFFERING];

  tty = fileno (info_input_stream);
  chars_avail = 0;

  space_avail = info_input_buffer_space_available ();

  /* If we can just find out how many characters there are to read, do so. */
#if defined (FIONREAD)
  {
    ioctl (tty, FIONREAD, &chars_avail);

    if (chars_avail > space_avail)
      chars_avail = space_avail;

    if (chars_avail)
      chars_avail = read (tty, &input[0], chars_avail);
  }
#else /* !FIONREAD */
#  if defined (O_NDELAY)
  {
    int flags;

    flags = fcntl (tty, F_GETFL, 0);

    fcntl (tty, F_SETFL, (flags | O_NDELAY));
      chars_avail = read (tty, &input[0], space_avail);
    fcntl (tty, F_SETFL, flags);

    if (chars_avail == -1)
      chars_avail = 0;
  }
#  else  /* !O_NDELAY */
#   ifdef __DJGPP__
  {
    extern long pc_term_chars_avail (void);

    if (isatty (tty))
      chars_avail = pc_term_chars_avail ();
    else
      {
	/* We could be more accurate by calling ltell, but we have no idea
	   whether tty is buffered by stdio functions, and if so, how many
	   characters are already waiting in the buffer.  So we punt.  */
	struct stat st;

	if (fstat (tty, &st) < 0)
	  chars_avail = 1;
	else
	  chars_avail = st.st_size;
      }
    if (chars_avail > space_avail)
      chars_avail = space_avail;
    if (chars_avail)
      chars_avail = read (tty, &input[0], chars_avail);
  }
#   endif/* __DJGPP__ */
#  endif /* O_NDELAY */
#endif /* !FIONREAD */

  while (i < chars_avail)
    {
      info_push_typeahead (input[i]);
      i++;
    }
}

/* How to read a single character. */
unsigned char
info_get_input_char ()
{
  unsigned char keystroke;

  info_gather_typeahead ();

  if (pending_input_character)
    {
      keystroke = pending_input_character;
      pending_input_character = 0;
    }
  else if (info_get_key_from_typeahead (&keystroke) == 0)
    {
      int rawkey;
      unsigned char c;
      int tty = fileno (info_input_stream);

      /* Using stream I/O causes FIONREAD etc to fail to work
         so unless someone can find a portable way of finding
         out how many characters are currently buffered, we
         should stay with away from stream I/O.
         --Egil Kvaleberg <egilk@sn.no>, January 1997.  */
#ifdef EINTR
      /* Keep reading if we got EINTR, so that we don't just exit.
         --Andreas Schwab <schwab@issan.informatik.uni-dortmund.de>,
         22 Dec 1997.  */
      {
        int n;
        do
	  n = read (tty, &c, 1);
        while (n == -1 && errno == EINTR);
        rawkey = n == 1 ? c : EOF;
      }
#else
      rawkey = (read (tty, &c, 1) == 1) ? c : EOF;
#endif

      keystroke = rawkey;

      if (rawkey == EOF)
        {
          if (info_input_stream != stdin)
            {
              fclose (info_input_stream);
              info_input_stream = stdin;
	      tty = fileno (info_input_stream);
              display_inhibited = 0;
              display_update_display (windows);
              display_cursor_at_point (active_window);
              rawkey = (read (tty, &c, 1) == 1) ? c : EOF;
              keystroke = rawkey;
            }

          if (rawkey == EOF)
            {
              terminal_unprep_terminal ();
              close_dribble_file ();
              xexit (0);
            }
        }
    }

  if (info_dribble_file)
    dribble (keystroke);

  return keystroke;
}
