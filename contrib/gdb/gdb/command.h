/* Header file for command-reading library command.c.
   Copyright (C) 1986, 1989, 1990 Free Software Foundation, Inc.

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

#if !defined (COMMAND_H)
#define COMMAND_H 1

/* Not a set/show command.  Note that some commands which begin with
   "set" or "show" might be in this category, if their syntax does
   not fall into one of the following categories.  */
typedef enum cmd_types {
  not_set_cmd,
  set_cmd,
  show_cmd
} cmd_types;

/* Types of "set" or "show" command.  */
typedef enum var_types {
  /* "on" or "off".  *VAR is an integer which is nonzero for on,
     zero for off.  */
  var_boolean,
  /* Unsigned Integer.  *VAR is an unsigned int.  The user can type 0
     to mean "unlimited", which is stored in *VAR as UINT_MAX.  */
  var_uinteger,

  /* Like var_uinteger but signed.  *VAR is an int.  The user can type 0
     to mean "unlimited", which is stored in *VAR as INT_MAX.  */
  var_integer,

  /* String which the user enters with escapes (e.g. the user types \n and
     it is a real newline in the stored string).
     *VAR is a malloc'd string, or NULL if the string is empty.  */
  var_string,
  /* String which stores what the user types verbatim.
     *VAR is a malloc'd string, or NULL if the string is empty.  */
  var_string_noescape,
  /* String which stores a filename.
     *VAR is a malloc'd string, or NULL if the string is empty.  */
  var_filename,
  /* ZeroableInteger.  *VAR is an int.  Like Unsigned Integer except
     that zero really means zero.  */
  var_zinteger,
  /* Enumerated type.  Can only have one of the specified values.  *VAR is a
     char pointer to the name of the element that we find.  */
  var_enum
} var_types;

/* This structure records one command'd definition.  */

struct cmd_list_element
  {
    /* Points to next command in this list.  */
    struct cmd_list_element *next;

    /* Name of this command.  */
    char *name;

    /* Command class; class values are chosen by application program.  */
    enum command_class class;

    /* Function definition of this command.
       NO_FUNCTION for command class names and for help topics that
       are not really commands.  */
    union
      {
	/* If type is not_set_cmd, call it like this:  */
	void (*cfunc) PARAMS ((char *args, int from_tty));

	/* If type is cmd_set or show_cmd, first set the variables, and
	   then call this.  */
	void (*sfunc) PARAMS ((char *args, int from_tty,
			       struct cmd_list_element *c));
      } function;
#   define NO_FUNCTION ((void (*) PARAMS((char *args, int from_tty))) 0)

    /* Documentation of this command (or help topic).
       First line is brief documentation; remaining lines form, with it,
       the full documentation.  First line should end with a period.
       Entire string should also end with a period, not a newline.  */
    char *doc;

    /* Hook for another command to be executed before this command.  */
    struct cmd_list_element *hook;

    /* Nonzero identifies a prefix command.  For them, the address
       of the variable containing the list of subcommands.  */
    struct cmd_list_element **prefixlist;

    /* For prefix commands only:
       String containing prefix commands to get here: this one
       plus any others needed to get to it.  Should end in a space.
       It is used before the word "command" in describing the
       commands reached through this prefix.  */
    char *prefixname;

    /* For prefix commands only:
       nonzero means do not get an error if subcommand is not
       recognized; call the prefix's own function in that case.  */
    char allow_unknown;

    /* Nonzero says this is an abbreviation, and should not
       be mentioned in lists of commands.
       This allows "br<tab>" to complete to "break", which it
       otherwise wouldn't.  */
    char abbrev_flag;

    /* Completion routine for this command.  TEXT is the text beyond
       what was matched for the command itself (leading whitespace is
       skipped).  It stops where we are supposed to stop completing
       (rl_point) and is '\0' terminated.

       Return value is a malloc'd vector of pointers to possible completions
       terminated with NULL.  If there are no completions, returning a pointer
       to a NULL would work but returning NULL itself is also valid.
       WORD points in the same buffer as TEXT, and completions should be
       returned relative to this position.  For example, suppose TEXT is "foo"
       and we want to complete to "foobar".  If WORD is "oo", return
       "oobar"; if WORD is "baz/foo", return "baz/foobar".  */
    char ** (*completer) PARAMS ((char *text, char *word));

    /* Type of "set" or "show" command (or SET_NOT_SET if not "set"
       or "show").  */
    cmd_types type;

    /* Pointer to variable affected by "set" and "show".  Doesn't matter
       if type is not_set.  */
    char *var;

    /* What kind of variable is *VAR?  */
    var_types var_type;

    /* Pointer to NULL terminated list of enumerated values (like argv).  */
    char **enums;

    /* Pointer to command strings of user-defined commands */
    struct command_line *user_commands;

    /* Pointer to command that is hooked by this one,
       so the hook can be removed when this one is deleted.  */
    struct cmd_list_element *hookee;

    /* Pointer to command that is aliased by this one, so the
       aliased command can be located in case it has been hooked.  */
    struct cmd_list_element *cmd_pointer;
  };

/* Forward-declarations of the entry-points of command.c.  */

extern struct cmd_list_element *
add_cmd PARAMS ((char *, enum command_class, void (*fun) (char *, int),
		 char *, struct cmd_list_element **));

extern struct cmd_list_element *
add_alias_cmd PARAMS ((char *, char *, enum command_class, int,
		       struct cmd_list_element **));

extern struct cmd_list_element *
add_prefix_cmd PARAMS ((char *, enum command_class, void (*fun) (char *, int),
			char *, struct cmd_list_element **, char *, int,
			struct cmd_list_element **));

extern struct cmd_list_element *
add_abbrev_prefix_cmd PARAMS ((char *, enum command_class,
			       void (*fun) (char *, int), char *,
			       struct cmd_list_element **, char *, int,
			       struct cmd_list_element **));

extern struct cmd_list_element *
lookup_cmd PARAMS ((char **, struct cmd_list_element *, char *, int, int));

extern struct cmd_list_element *
lookup_cmd_1 PARAMS ((char **, struct cmd_list_element *,
		      struct cmd_list_element **, int));

extern void
add_com PARAMS ((char *, enum command_class, void (*fun)(char *, int),
		 char *));

extern void
add_com_alias PARAMS ((char *, char *, enum command_class, int));

extern void
add_info PARAMS ((char *, void (*fun) (char *, int), char *));

extern void
add_info_alias PARAMS ((char *, char *, int));

extern char **
complete_on_cmdlist PARAMS ((struct cmd_list_element *, char *, char *));

extern char **
complete_on_enum PARAMS ((char **enumlist, char *, char *));

extern void
delete_cmd PARAMS ((char *, struct cmd_list_element **));

extern void
help_cmd PARAMS ((char *, GDB_FILE *));

extern void
help_list PARAMS ((struct cmd_list_element *, char *, enum command_class,
		   GDB_FILE *));

extern void
help_cmd_list PARAMS ((struct cmd_list_element *, enum command_class, char *,
		       int, GDB_FILE *));

extern struct cmd_list_element *
add_set_cmd PARAMS ((char *, enum command_class, var_types, char *, char *,
		     struct cmd_list_element **));

extern struct cmd_list_element *
add_set_enum_cmd PARAMS ((char *name, enum command_class, char *list[],
			  char *var, char *doc, struct cmd_list_element **c));

extern struct cmd_list_element *
add_show_from_set PARAMS ((struct cmd_list_element *,
			   struct cmd_list_element **));

/* Do a "set" or "show" command.  ARG is NULL if no argument, or the text
   of the argument, and FROM_TTY is nonzero if this command is being entered
   directly by the user (i.e. these are just like any other
   command).  C is the command list element for the command.  */

extern void
do_setshow_command PARAMS ((char *, int, struct cmd_list_element *));

/* Do a "show" command for each thing on a command list.  */

extern void
cmd_show_list PARAMS ((struct cmd_list_element *, int, char *));

extern void
error_no_arg PARAMS ((char *));

extern void
dont_repeat PARAMS ((void));

/* Used to mark commands that don't do anything.  If we just leave the
   function field NULL, the command is interpreted as a help topic, or
   as a class of commands.  */

extern void
not_just_help_class_command PARAMS ((char *, int));

#endif /* !defined (COMMAND_H) */
