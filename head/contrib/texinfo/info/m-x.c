/* m-x.c -- Meta-x minibuffer reader.
   $Id: m-x.c,v 1.3 2004/04/11 17:56:46 karl Exp $

   Copyright (C) 1993, 1997, 1998, 2001, 2002, 2004 Free Software
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

   Originally written by Brian Fox (bfox@ai.mit.edu). */

#include "info.h"
#include "funs.h"

/* **************************************************************** */
/*                                                                  */
/*                     Reading Named Commands                       */
/*                                                                  */
/* **************************************************************** */

/* Read the name of an Info function in the echo area and return the
   name.  A return value of NULL indicates that no function name could
   be read. */
char *
read_function_name (char *prompt, WINDOW *window)
{
  register int i;
  char *line;
  REFERENCE **array = (REFERENCE **)NULL;
  int array_index = 0, array_slots = 0;

  /* Make an array of REFERENCE which actually contains the names of
     the functions available in Info. */
  for (i = 0; function_doc_array[i].func; i++)
    {
      REFERENCE *entry;

      entry = (REFERENCE *)xmalloc (sizeof (REFERENCE));
      entry->label = xstrdup (function_doc_array[i].func_name);
      entry->nodename = (char *)NULL;
      entry->filename = (char *)NULL;

      add_pointer_to_array
        (entry, array_index, array, array_slots, 200, REFERENCE *);
    }

  line = info_read_completing_in_echo_area (window, prompt, array);

  info_free_references (array);

  if (!echo_area_is_active)
    window_clear_echo_area ();

  return (line);
}

DECLARE_INFO_COMMAND (describe_command,
   _("Read the name of an Info command and describe it"))
{
  char *line;

  line = read_function_name ((char *) _("Describe command: "), window);

  if (!line)
    {
      info_abort_key (active_window, count, key);
      return;
    }

  /* Describe the function named in "LINE". */
  if (*line)
    {
      InfoCommand *cmd = named_function (line);

      if (!cmd)
        return;

      window_message_in_echo_area ("%s: %s.",
                                   line, function_documentation (cmd));
    }
  free (line);
}

DECLARE_INFO_COMMAND (info_execute_command,
   _("Read a command name in the echo area and execute it"))
{
  char *line;
  char *keys;
  char *prompt;

  prompt = (char *)xmalloc (20);

  keys = where_is (info_keymap, InfoCmd(info_execute_command));
  /* If the where_is () function thinks that this command doesn't exist,
     there's something very wrong!  */
  if (!keys)
    abort();

  if (info_explicit_arg || count != 1)
    sprintf (prompt, "%d %s ", count, keys);
  else
    sprintf (prompt, "%s ", keys);

  /* Ask the completer to read a reference for us. */
  line = read_function_name (prompt, window);

  /* User aborted? */
  if (!line)
    {
      info_abort_key (active_window, count, key);
      return;
    }

  /* User accepted "default"?  (There is none.) */
  if (!*line)
    {
      free (line);
      return;
    }

  /* User wants to execute a named command.  Do it. */
  {
    InfoCommand *command;

    if ((active_window != the_echo_area) &&
        (strncmp (line, "echo-area-", 10) == 0))
      {
        free (line);
        info_error ((char *) _("Cannot execute an `echo-area' command here."),
            NULL, NULL);
        return;
      }

    command = named_function (line);
    free (line);

    if (!command)
      return;

    if (InfoFunction(command))
      (*InfoFunction(command)) (active_window, count, 0);
    else
      info_error ((char *) _("Undefined command: %s"), line, NULL);
  }
}

/* Okay, now that we have M-x, let the user set the screen height. */
DECLARE_INFO_COMMAND (set_screen_height,
  _("Set the height of the displayed window"))
{
  int new_height, old_height = screenheight;

  if (info_explicit_arg || count != 1)
    new_height = count;
  else
    {
      char prompt[80];
      char *line;

      new_height = screenheight;

      sprintf (prompt, _("Set screen height to (%d): "), new_height);

      line = info_read_in_echo_area (window, prompt);

      /* If the user aborted, do that now. */
      if (!line)
        {
          info_abort_key (active_window, count, 0);
          return;
        }

      /* Find out what the new height is supposed to be. */
      if (*line)
        new_height = atoi (line);

      /* Clear the echo area if it isn't active. */
      if (!echo_area_is_active)
        window_clear_echo_area ();

      free (line);
    }

  terminal_clear_screen ();
  display_clear_display (the_display);
  screenheight = new_height;
#ifdef SET_SCREEN_SIZE_HELPER
  SET_SCREEN_SIZE_HELPER;
#endif
  if (screenheight == old_height)
    {
      /* Display dimensions didn't actually change, so
	 window_new_screen_size won't do anything, but we've
	 already cleared the display above.  Undo the damage.  */
      window_mark_chain (windows, W_UpdateWindow);
      display_update_display (windows);
    }
  else
    {
      display_initialize_display (screenwidth, screenheight);
      window_new_screen_size (screenwidth, screenheight);
    }
}
