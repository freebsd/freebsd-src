/* ***DEPRECATED***  The gdblib files must not be calling/using things in any
   of the possible command languages.  If necessary, a hook (that may be
   present or not) must be used and set to the appropriate routine by any
   command language that cares about it.  If you are having to include this
   file you are possibly doing things the old way.  This file will disapear.
   2000-12-01 fnasser@redhat.com    */

/* Header file for command-reading library command.c.
   Copyright 1986, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1999, 2000
   Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#if !defined (COMMAND_H)
#define COMMAND_H 1

/* Command classes are top-level categories into which commands are broken
   down for "help" purposes.  
   Notes on classes: class_alias is for alias commands which are not
   abbreviations of the original command.  class-pseudo is for
   commands which are not really commands nor help topics ("stop").  */

enum command_class
{
  /* Special args to help_list */
  class_deprecated, all_classes = -2, all_commands = -1,
  /* Classes of commands */
  no_class = -1, class_run = 0, class_vars, class_stack,
  class_files, class_support, class_info, class_breakpoint, class_trace,
  class_alias, class_obscure, class_user, class_maintenance,
  class_pseudo, class_tui, class_xdb
};

/* Not a set/show command.  Note that some commands which begin with
   "set" or "show" might be in this category, if their syntax does
   not fall into one of the following categories.  */
typedef enum cmd_types
  {
    not_set_cmd,
    set_cmd,
    show_cmd
  }
cmd_types;

/* Reasonable values for an AUTO_BOOLEAN variable. */
enum cmd_auto_boolean
{
  CMD_AUTO_BOOLEAN_TRUE,
  CMD_AUTO_BOOLEAN_FALSE,
  CMD_AUTO_BOOLEAN_AUTO
};

/* Types of "set" or "show" command.  */
typedef enum var_types
  {
    /* "on" or "off".  *VAR is an integer which is nonzero for on,
       zero for off.  */
    var_boolean,

    /* "on" / "true" / "enable" or "off" / "false" / "disable" or
       "auto.  *VAR is an ``enum cmd_auto_boolean''.  NOTE: In general
       a custom show command will need to be implemented - one that
       for "auto" prints both the "auto" and the current auto-selected
       value. */
    var_auto_boolean,

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
  }
var_types;

/* This structure records one command'd definition.  */


/* This flag is used by the code executing commands to warn the user 
   the first time a deprecated command is used, see the 'flags' field in
   the following struct.
*/
#define CMD_DEPRECATED            0x1
#define DEPRECATED_WARN_USER      0x2
#define MALLOCED_REPLACEMENT      0x4

struct cmd_list_element
  {
    /* Points to next command in this list.  */
    struct cmd_list_element *next;

    /* Name of this command.  */
    char *name;

    /* Command class; class values are chosen by application program.  */
    enum command_class class;

    /* Function definition of this command.  NULL for command class
       names and for help topics that are not really commands.  NOTE:
       cagney/2002-02-02: This function signature is evolving.  For
       the moment suggest sticking with either set_cmd_cfunc() or
       set_cmd_sfunc().  */
    void (*func) (struct cmd_list_element *c, char *args, int from_tty);
    /* The command's real callback.  At present func() bounces through
       to one of the below.  */
    union
      {
	/* If type is not_set_cmd, call it like this:  */
	void (*cfunc) (char *args, int from_tty);

	/* If type is set_cmd or show_cmd, first set the variables, and
	   then call this.  */
	void (*sfunc) (char *args, int from_tty, struct cmd_list_element * c);
      }
    function;

    /* Documentation of this command (or help topic).
       First line is brief documentation; remaining lines form, with it,
       the full documentation.  First line should end with a period.
       Entire string should also end with a period, not a newline.  */
    char *doc;

    /* flags : a bitfield 
       
       bit 0: (LSB) CMD_DEPRECATED, when 1 indicated that this command
       is deprecated. It may be removed from gdb's command set in the
       future.

       bit 1: DEPRECATED_WARN_USER, the user needs to be warned that
       this is a deprecated command.  The user should only be warned
       the first time a command is used.
        
       bit 2: MALLOCED_REPLACEMENT, when functions are deprecated at
       compile time (this is the way it should, in general, be done)
       the memory containing the replacement string is statically
       allocated.  In some cases it makes sense to deprecate commands
       at runtime (the testsuite is one example).  In this case the
       memory for replacement is malloc'ed.  When a command is
       undeprecated or re-deprecated at runtime we don't want to risk
       calling free on statically allocated memory, so we check this
       flag.  
     */
    int flags;

    /* if this command is deprecated, this is the replacement name */
    char *replacement;

    /* If this command represents a show command, then this function
       is called before the variable's value is examined.  */
    void (*pre_show_hook) (struct cmd_list_element *c);

    /* Hook for another command to be executed before this command.  */
    struct cmd_list_element *hook_pre;

    /* Hook for another command to be executed after this command.  */
    struct cmd_list_element *hook_post;

    /* Flag that specifies if this command is already running it's hook. */
    /* Prevents the possibility of hook recursion. */
    int hook_in;

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
    char **(*completer) (char *text, char *word);

    /* Type of "set" or "show" command (or SET_NOT_SET if not "set"
       or "show").  */
    cmd_types type;

    /* Pointer to variable affected by "set" and "show".  Doesn't matter
       if type is not_set.  */
    void *var;

    /* What kind of variable is *VAR?  */
    var_types var_type;

    /* Pointer to NULL terminated list of enumerated values (like argv).  */
    const char **enums;

    /* Pointer to command strings of user-defined commands */
    struct command_line *user_commands;

    /* Pointer to command that is hooked by this one, (by hook_pre)
       so the hook can be removed when this one is deleted.  */
    struct cmd_list_element *hookee_pre;

    /* Pointer to command that is hooked by this one, (by hook_post)
       so the hook can be removed when this one is deleted.  */
    struct cmd_list_element *hookee_post;

    /* Pointer to command that is aliased by this one, so the
       aliased command can be located in case it has been hooked.  */
    struct cmd_list_element *cmd_pointer;
  };

/* Forward-declarations of the entry-points of cli/cli-decode.c.  */

extern struct cmd_list_element *add_cmd (char *, enum command_class,
					 void (*fun) (char *, int), char *,
					 struct cmd_list_element **);

extern struct cmd_list_element *add_alias_cmd (char *, char *,
					       enum command_class, int,
					       struct cmd_list_element **);

extern struct cmd_list_element *add_prefix_cmd (char *, enum command_class,
						void (*fun) (char *, int),
						char *,
						struct cmd_list_element **,
						char *, int,
						struct cmd_list_element **);

extern struct cmd_list_element *add_abbrev_prefix_cmd (char *,
						       enum command_class,
						       void (*fun) (char *,
								    int),
						       char *,
						       struct cmd_list_element
						       **, char *, int,
						       struct cmd_list_element
						       **);

/* Set the commands corresponding callback.  */

extern void set_cmd_cfunc (struct cmd_list_element *cmd,
			   void (*cfunc) (char *args, int from_tty));

extern void set_cmd_sfunc (struct cmd_list_element *cmd,
			   void (*sfunc) (char *args, int from_tty,
					  struct cmd_list_element * c));

/* HACK: cagney/2002-02-23: Code, mostly in tracepoints.c, grubs
   around in cmd objects to test the value of the commands sfunc().  */
extern int cmd_cfunc_eq (struct cmd_list_element *cmd,
			 void (*cfunc) (char *args, int from_tty));

extern struct cmd_list_element *lookup_cmd (char **,
					    struct cmd_list_element *, char *,
					    int, int);

extern struct cmd_list_element *lookup_cmd_1 (char **,
					      struct cmd_list_element *,
					      struct cmd_list_element **,
					      int);

extern struct cmd_list_element *
  deprecate_cmd (struct cmd_list_element *, char * );

extern void
  deprecated_cmd_warning (char **);

extern int
  lookup_cmd_composition (char *text,
                        struct cmd_list_element **alias,
                        struct cmd_list_element **prefix_cmd,
                        struct cmd_list_element **cmd);

extern struct cmd_list_element *add_com (char *, enum command_class,
					 void (*fun) (char *, int), char *);

extern struct cmd_list_element *add_com_alias (char *, char *,
					       enum command_class, int);

extern struct cmd_list_element *add_info (char *, void (*fun) (char *, int),
					  char *);

extern struct cmd_list_element *add_info_alias (char *, char *, int);

extern char **complete_on_cmdlist (struct cmd_list_element *, char *, char *);

extern char **complete_on_enum (const char *enumlist[], char *, char *);

extern void delete_cmd (char *, struct cmd_list_element **);

extern void help_cmd (char *, struct ui_file *);

extern void help_list (struct cmd_list_element *, char *,
		       enum command_class, struct ui_file *);

extern void help_cmd_list (struct cmd_list_element *, enum command_class,
			   char *, int, struct ui_file *);

extern struct cmd_list_element *add_set_cmd (char *name, enum
					     command_class class,
					     var_types var_type, void *var,
					     char *doc,
					     struct cmd_list_element **list);

extern struct cmd_list_element *add_set_enum_cmd (char *name,
						  enum command_class class,
						  const char *enumlist[],
						  const char **var,
						  char *doc,
						  struct cmd_list_element **list);

extern struct cmd_list_element *add_set_auto_boolean_cmd (char *name,
							  enum command_class class,
							  enum cmd_auto_boolean *var,
							  char *doc,
							  struct cmd_list_element **list);

extern struct cmd_list_element *add_set_boolean_cmd (char *name,
						     enum command_class class,
						     int *var,
						     char *doc,
						     struct cmd_list_element **list);

extern struct cmd_list_element *add_show_from_set (struct cmd_list_element *,
						   struct cmd_list_element
						   **);

/* Do a "show" command for each thing on a command list.  */

extern void cmd_show_list (struct cmd_list_element *, int, char *);

extern NORETURN void error_no_arg (char *) ATTR_NORETURN;

extern void dont_repeat (void);

/* Used to mark commands that don't do anything.  If we just leave the
   function field NULL, the command is interpreted as a help topic, or
   as a class of commands.  */

extern void not_just_help_class_command (char *, int);

#endif /* !defined (COMMAND_H) */
