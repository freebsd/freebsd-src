/* info.c -- Display nodes of Info files in multiple windows.
   $Id: info.c,v 1.7 2003/05/19 13:10:59 karl Exp $

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
#include "indices.h"
#include "dribble.h"
#include "getopt.h"
#if defined (HANDLE_MAN_PAGES)
#  include "man.h"
#endif /* HANDLE_MAN_PAGES */

static char *program_name = "info";

/* Non-zero means search all indices for APROPOS_SEARCH_STRING. */
static int apropos_p = 0;

/* Variable containing the string to search for when apropos_p is non-zero. */
static char *apropos_search_string = (char *)NULL;

/* Non-zero means search all indices for INDEX_SEARCH_STRING.  Unlike
   apropos, this puts the user at the node, running info. */
static int index_search_p = 0;

/* Non-zero means look for the node which describes the invocation
   and command-line options of the program, and start the info
   session at that node.  */
static int goto_invocation_p = 0;

/* Variable containing the string to search for when index_search_p is
   non-zero. */
static char *index_search_string = (char *)NULL;

/* Non-zero means print version info only. */
static int print_version_p = 0;

/* Non-zero means print a short description of the options. */
static int print_help_p = 0;

/* Array of the names of nodes that the user specified with "--node" on the
   command line. */
static char **user_nodenames = (char **)NULL;
static int user_nodenames_index = 0;
static int user_nodenames_slots = 0;

/* String specifying the first file to load.  This string can only be set
   by the user specifying "--file" on the command line. */
static char *user_filename = (char *)NULL;

/* String specifying the name of the file to dump nodes to.  This value is
   filled if the user speficies "--output" on the command line. */
static char *user_output_filename = (char *)NULL;

/* Non-zero indicates that when "--output" is specified, all of the menu
   items of the specified nodes (and their subnodes as well) should be
   dumped in the order encountered.  This basically can print a book. */
int dump_subnodes = 0;

/* Non-zero means make default keybindings be loosely modeled on vi(1).  */
int vi_keys_p = 0;

/* Non-zero means don't remove ANSI escape sequences from man pages.  */
int raw_escapes_p = 0;

#ifdef __MSDOS__
/* Non-zero indicates that screen output should be made 'speech-friendly'.
   Since on MSDOS the usual behavior is to write directly to the video
   memory, speech synthesizer software cannot grab the output.  Therefore,
   we provide a user option which tells us to avoid direct screen output
   and use stdout instead (which loses the color output).  */
int speech_friendly = 0;
#endif

/* Structure describing the options that Info accepts.  We pass this structure
   to getopt_long ().  If you add or otherwise change this structure, you must
   also change the string which follows it. */
#define APROPOS_OPTION 1
#define DRIBBLE_OPTION 2
#define RESTORE_OPTION 3
#define IDXSRCH_OPTION 4
static struct option long_options[] = {
  { "apropos", 1, 0, APROPOS_OPTION },
  { "directory", 1, 0, 'd' },
  { "dribble", 1, 0, DRIBBLE_OPTION },
  { "file", 1, 0, 'f' },
  { "help", 0, &print_help_p, 1 },
  { "index-search", 1, 0, IDXSRCH_OPTION },
  { "node", 1, 0, 'n' },
  { "output", 1, 0, 'o' },
  { "raw-escapes", 0, &raw_escapes_p, 1 },
  { "restore", 1, 0, RESTORE_OPTION },
  { "show-options", 0, 0, 'O' },
  { "subnodes", 0, &dump_subnodes, 1 },
  { "usage", 0, 0, 'O' },
  { "version", 0, &print_version_p, 1 },
  { "vi-keys", 0, &vi_keys_p, 1 },
#ifdef __MSDOS__
  { "speech-friendly", 0, &speech_friendly, 1 },
#endif
  {NULL, 0, NULL, 0}
};

/* String describing the shorthand versions of the long options found above. */
#ifdef __MSDOS__
static char *short_options = "d:n:f:ho:ORsb";
#else
static char *short_options = "d:n:f:ho:ORs";
#endif

/* When non-zero, the Info window system has been initialized. */
int info_windows_initialized_p = 0;

/* Some "forward" declarations. */
static void info_short_help ();
static void init_messages ();
extern void add_file_directory_to_path ();


/* **************************************************************** */
/*                                                                  */
/*                Main Entry Point to the Info Program              */
/*                                                                  */
/* **************************************************************** */

int
main (argc, argv)
     int argc;
     char **argv;
{
  int getopt_long_index;        /* Index returned by getopt_long (). */
  NODE *initial_node;           /* First node loaded by Info. */

#ifdef HAVE_SETLOCALE
  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");
#endif

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  init_messages ();

  while (1)
    {
      int option_character;

      option_character = getopt_long
        (argc, argv, short_options, long_options, &getopt_long_index);

      /* getopt_long returns EOF when there are no more long options. */
      if (option_character == EOF)
        break;

      /* If this is a long option, then get the short version of it. */
      if (option_character == 0 && long_options[getopt_long_index].flag == 0)
        option_character = long_options[getopt_long_index].val;

      /* Case on the option that we have received. */
      switch (option_character)
        {
        case 0:
          break;

          /* User wants to add a directory. */
        case 'd':
          info_add_path (optarg, INFOPATH_PREPEND);
          break;

          /* User is specifying a particular node. */
        case 'n':
          add_pointer_to_array (optarg, user_nodenames_index, user_nodenames,
                                user_nodenames_slots, 10, char *);
          break;

          /* User is specifying a particular Info file. */
        case 'f':
          if (user_filename)
            free (user_filename);

          user_filename = xstrdup (optarg);
          break;

          /* Treat -h like --help. */
        case 'h':
          print_help_p = 1;
          break;

          /* User is specifying the name of a file to output to. */
        case 'o':
          if (user_output_filename)
            free (user_output_filename);
          user_output_filename = xstrdup (optarg);
          break;

         /* User has specified that she wants to find the "Options"
             or "Invocation" node for the program.  */
        case 'O':
          goto_invocation_p = 1;
          break;

	  /* User has specified that she wants the escape sequences
	     in man pages to be passed thru unaltered.  */
        case 'R':
          raw_escapes_p = 1;
          break;

          /* User is specifying that she wishes to dump the subnodes of
             the node that she is dumping. */
        case 's':
          dump_subnodes = 1;
          break;

#ifdef __MSDOS__
	  /* User wants speech-friendly output.  */
	case 'b':
	  speech_friendly = 1;
	  break;
#endif /* __MSDOS__ */

          /* User has specified a string to search all indices for. */
        case APROPOS_OPTION:
          apropos_p = 1;
          maybe_free (apropos_search_string);
          apropos_search_string = xstrdup (optarg);
          break;

          /* User has specified a dribble file to receive keystrokes. */
        case DRIBBLE_OPTION:
          close_dribble_file ();
          open_dribble_file (optarg);
          break;

          /* User has specified an alternate input stream. */
        case RESTORE_OPTION:
          info_set_input_from_file (optarg);
          break;

          /* User has specified a string to search all indices for. */
        case IDXSRCH_OPTION:
          index_search_p = 1;
          maybe_free (index_search_string);
          index_search_string = xstrdup (optarg);
          break;

        default:
          fprintf (stderr, _("Try --help for more information.\n"));
          xexit (1);
        }
    }

  /* If the output device is not a terminal, and no output filename has been
     specified, make user_output_filename be "-", so that the info is written
     to stdout, and turn on the dumping of subnodes. */
  if ((!isatty (fileno (stdout))) && (user_output_filename == (char *)NULL))
    {
      user_output_filename = xstrdup ("-");
      dump_subnodes = 1;
    }

  /* If the user specified --version, then show the version and exit. */
  if (print_version_p)
    {
      printf ("%s (GNU %s) %s\n", program_name, PACKAGE, VERSION);
      puts ("");
      printf (_("Copyright (C) %s Free Software Foundation, Inc.\n\
There is NO warranty.  You may redistribute this software\n\
under the terms of the GNU General Public License.\n\
For more information about these matters, see the files named COPYING.\n"),
		  "2003");
      xexit (0);
    }

  /* If the `--help' option was present, show the help and exit. */
  if (print_help_p)
    {
      info_short_help ();
      xexit (0);
    }

  /* If the user hasn't specified a path for Info files, default it.
     Lowest priority is our messy hardwired list in filesys.h.
     Then comes the user's INFODIR from the Makefile.
     Highest priority is the environment variable, if set.  */
  if (!infopath)
    {
      char *path_from_env = getenv ("INFOPATH");

      if (path_from_env)
        {
          unsigned len = strlen (path_from_env);
          /* Trailing : on INFOPATH means insert the default path.  */
          if (len && path_from_env[len - 1] == PATH_SEP[0])
            {
              path_from_env[len - 1] = 0;
              info_add_path (DEFAULT_INFOPATH, INFOPATH_PREPEND);
            }
#ifdef INFODIR /* from the Makefile */
          info_add_path (INFODIR, INFOPATH_PREPEND);
#endif
          info_add_path (path_from_env, INFOPATH_PREPEND);
        }
      else
        {
          info_add_path (DEFAULT_INFOPATH, INFOPATH_PREPEND);
#ifdef INFODIR /* from the Makefile */
         info_add_path (INFODIR, INFOPATH_PREPEND);
#endif
        }
    }

  /* If the user specified a particular filename, add the path of that
     file to the contents of INFOPATH. */
  if (user_filename)
    add_file_directory_to_path (user_filename);

  /* If the user wants to search every known index for a given string,
     do that now, and report the results. */
  if (apropos_p)
    {
      info_apropos (apropos_search_string);
      xexit (0);
    }

  /* Get the initial Info node.  It is either "(dir)Top", or what the user
     specifed with values in user_filename and user_nodenames. */
  initial_node = info_get_node (user_filename,
                                user_nodenames ? user_nodenames[0] : 0);

  /* If we couldn't get the initial node, this user is in trouble. */
  if (!initial_node)
    {
      if (info_recent_file_error)
        info_error (info_recent_file_error);
      else
        info_error (msg_cant_find_node,
                    user_nodenames ? user_nodenames[0] : "Top");
      xexit (1);
    }

  /* Special cases for when the user specifies multiple nodes.  If we
     are dumping to an output file, dump all of the nodes specified.
     Otherwise, attempt to create enough windows to handle the nodes
     that this user wants displayed. */
  if (user_nodenames_index > 1)
    {
      free (initial_node);

      if (user_output_filename)
        dump_nodes_to_file
          (user_filename, user_nodenames, user_output_filename, dump_subnodes);
      else
        begin_multiple_window_info_session (user_filename, user_nodenames);

      xexit (0);
    }

  /* If there are arguments remaining, they are the names of menu items
     in sequential info files starting from the first one loaded.  That
     file name is either "dir", or the contents of user_filename if one
     was specified. */
  {
    char *errstr, *errarg1, *errarg2;
    NODE *new_initial_node = info_follow_menus (initial_node, argv + optind,
                                                &errstr, &errarg1, &errarg2);

    if (new_initial_node && new_initial_node != initial_node)
      initial_node = new_initial_node;

    /* If the user specified that this node should be output, then do that
       now.  Otherwise, start the Info session with this node.  Or act
       accordingly if the initial node was not found.  */
    if (user_output_filename && !goto_invocation_p)
      {
        if (!errstr)
          dump_node_to_file (initial_node, user_output_filename,
                             dump_subnodes);
        else
          info_error (errstr, errarg1, errarg2);
      }
    else
      {

        if (errstr)
          begin_info_session_with_error (initial_node, errstr,
                                         errarg1, errarg2);
        /* If the user specified `--index-search=STRING' or
           --show-options, start the info session in the node
           corresponding to what they want. */
        else if (index_search_p || goto_invocation_p)
          {
            int status = 0;

            initialize_info_session (initial_node, 0);

            if (goto_invocation_p
                || index_entry_exists (windows, index_search_string))
              {
                terminal_prep_terminal ();
                terminal_clear_screen ();
                info_last_executed_command = (VFunction *)NULL;

                if (index_search_p)
                  do_info_index_search (windows, 0, index_search_string);
                else
                  {
                    /* If they said "info --show-options foo bar baz",
                       the last of the arguments is the program whose
                       options they want to see.  */
                    char **p = argv + optind;
                    char *program;

                    if (*p)
                      {
                        while (p[1])
                          p++;
                        program = xstrdup (*p);
                      }
                    else if (user_filename)
		      /* If there's no command-line arguments to
			 supply the program name, use the Info file
			 name (sans extension and leading directories)
			 instead.  */
		      program = program_name_from_file_name (user_filename);
		    else
		      program = xstrdup ("");

                    info_intuit_options_node (windows, initial_node, program);
                    free (program);
                  }

		if (user_output_filename)
		  {
		    dump_node_to_file (windows->node, user_output_filename,
				       dump_subnodes);
		  }
		else
		  info_read_and_dispatch ();

                /* On program exit, leave the cursor at the bottom of the
                   window, and restore the terminal IO. */
                terminal_goto_xy (0, screenheight - 1);
                terminal_clear_to_eol ();
                fflush (stdout);
                terminal_unprep_terminal ();
              }
            else
              {
                fprintf (stderr, _("no index entries found for `%s'\n"),
                         index_search_string);
                status = 2;
              }

            close_dribble_file ();
            xexit (status);
          }
        else
          begin_info_session (initial_node);
      }

    xexit (0);
  }

  return 0; /* Avoid bogus warnings.  */
}

void
add_file_directory_to_path (filename)
     char *filename;
{
  char *directory_name = xstrdup (filename);
  char *temp = filename_non_directory (directory_name);

  if (temp != directory_name)
    {
      if (HAVE_DRIVE (directory_name) && temp == directory_name + 2)
	{
	  /* The directory of "d:foo" is stored as "d:.", to avoid
	     mixing it with "d:/" when a slash is appended.  */
	  *temp = '.';
	  temp += 2;
	}
      temp[-1] = 0;
      info_add_path (directory_name, INFOPATH_PREPEND);
    }

  free (directory_name);
}


/* Error handling.  */

/* Non-zero if an error has been signalled. */
int info_error_was_printed = 0;

/* Non-zero means ring terminal bell on errors. */
int info_error_rings_bell_p = 1;

/* Print FORMAT with ARG1 and ARG2.  If the window system was initialized,
   then the message is printed in the echo area.  Otherwise, a message is
   output to stderr. */
void
info_error (format, arg1, arg2)
     char *format;
     void *arg1, *arg2;
{
  info_error_was_printed = 1;

  if (!info_windows_initialized_p || display_inhibited)
    {
      fprintf (stderr, "%s: ", program_name);
      fprintf (stderr, format, arg1, arg2);
      fprintf (stderr, "\n");
      fflush (stderr);
    }
  else
    {
      if (!echo_area_is_active)
        {
          if (info_error_rings_bell_p)
            terminal_ring_bell ();
          window_message_in_echo_area (format, arg1, arg2);
        }
      else
        {
          NODE *temp;

          temp = build_message_node (format, arg1, arg2);
          if (info_error_rings_bell_p)
            terminal_ring_bell ();
          inform_in_echo_area (temp->contents);
          free (temp->contents);
          free (temp);
        }
    }
}


/* Produce a scaled down description of the available options to Info. */
static void
info_short_help ()
{
#ifdef __MSDOS__
  static const char speech_friendly_string[] = N_("\
  -b, --speech-friendly        be friendly to speech synthesizers.\n");
#else
  static const char speech_friendly_string[] = "";
#endif


  printf (_("\
Usage: %s [OPTION]... [MENU-ITEM...]\n\
\n\
Read documentation in Info format.\n\
\n\
Options:\n\
      --apropos=STRING         look up STRING in all indices of all manuals.\n\
  -d, --directory=DIR          add DIR to INFOPATH.\n\
      --dribble=FILENAME       remember user keystrokes in FILENAME.\n\
  -f, --file=FILENAME          specify Info file to visit.\n\
  -h, --help                   display this help and exit.\n\
      --index-search=STRING    go to node pointed by index entry STRING.\n\
  -n, --node=NODENAME          specify nodes in first visited Info file.\n\
  -o, --output=FILENAME        output selected nodes to FILENAME.\n\
  -R, --raw-escapes            don't remove ANSI escapes from man pages.\n\
      --restore=FILENAME       read initial keystrokes from FILENAME.\n\
  -O, --show-options, --usage  go to command-line options node.\n%s\
      --subnodes               recursively output menu items.\n\
      --vi-keys                use vi-like and less-like key bindings.\n\
      --version                display version information and exit.\n\
\n\
The first non-option argument, if present, is the menu entry to start from;\n\
it is searched for in all `dir' files along INFOPATH.\n\
If it is not present, info merges all `dir' files and shows the result.\n\
Any remaining arguments are treated as the names of menu\n\
items relative to the initial node visited.\n\
\n\
Examples:\n\
  info                       show top-level dir menu\n\
  info emacs                 start at emacs node from top-level dir\n\
  info emacs buffers         start at buffers node within emacs manual\n\
  info --show-options emacs  start at node with emacs' command line options\n\
  info -f ./foo.info         show file ./foo.info, not searching dir\n\
"),
  program_name, speech_friendly_string);

  puts (_("\n\
Email bug reports to bug-texinfo@gnu.org,\n\
general questions and discussion to help-texinfo@gnu.org.\n\
Texinfo home page: http://www.gnu.org/software/texinfo/"));

  xexit (0);
}


/* Initialize strings for gettext.  Because gettext doesn't handle N_ or
   _ within macro definitions, we put shared messages into variables and
   use them that way.  This also has the advantage that there's only one
   copy of the strings.  */

const char *msg_cant_find_node;
const char *msg_cant_file_node;
const char *msg_cant_find_window;
const char *msg_cant_find_point;
const char *msg_cant_kill_last;
const char *msg_no_menu_node;
const char *msg_no_foot_node;
const char *msg_no_xref_node;
const char *msg_no_pointer;
const char *msg_unknown_command;
const char *msg_term_too_dumb;
const char *msg_at_node_bottom;
const char *msg_at_node_top;
const char *msg_one_window;
const char *msg_win_too_small;
const char *msg_cant_make_help;

static void
init_messages ()
{
  msg_cant_find_node   = _("Cannot find node `%s'.");
  msg_cant_file_node   = _("Cannot find node `(%s)%s'.");
  msg_cant_find_window = _("Cannot find a window!");
  msg_cant_find_point  = _("Point doesn't appear within this window's node!");
  msg_cant_kill_last   = _("Cannot delete the last window.");
  msg_no_menu_node     = _("No menu in this node.");
  msg_no_foot_node     = _("No footnotes in this node.");
  msg_no_xref_node     = _("No cross references in this node.");
  msg_no_pointer       = _("No `%s' pointer for this node.");
  msg_unknown_command  = _("Unknown Info command `%c'; try `?' for help.");
  msg_term_too_dumb    = _("Terminal type `%s' is not smart enough to run Info.");
  msg_at_node_bottom   = _("You are already at the last page of this node.");
  msg_at_node_top      = _("You are already at the first page of this node.");
  msg_one_window       = _("Only one window.");
  msg_win_too_small    = _("Resulting window would be too small.");
  msg_cant_make_help   = _("Not enough room for a help window, please delete a window.");
}
