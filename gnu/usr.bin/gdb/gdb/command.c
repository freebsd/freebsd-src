/* Handle lists of commands, their decoding and documentation, for GDB.
   Copyright 1986, 1989, 1990, 1991 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "gdbcmd.h"
#include "symtab.h"
#include "value.h"
#include <ctype.h>
#include <string.h>

/* Prototypes for local functions */

static void
undef_cmd_error PARAMS ((char *, char *));

static void
show_user PARAMS ((char *, int));

static void
show_user_1 PARAMS ((struct cmd_list_element *, GDB_FILE *));

static void
make_command PARAMS ((char *, int));

static void
shell_escape PARAMS ((char *, int));

static int
parse_binary_operation PARAMS ((char *));

static void
print_doc_line PARAMS ((GDB_FILE *, char *));

/* Add element named NAME.
   CLASS is the top level category into which commands are broken down
   for "help" purposes.
   FUN should be the function to execute the command;
   it will get a character string as argument, with leading
   and trailing blanks already eliminated.

   DOC is a documentation string for the command.
   Its first line should be a complete sentence.
   It should start with ? for a command that is an abbreviation
   or with * for a command that most users don't need to know about.

   Add this command to command list *LIST.  */

struct cmd_list_element *
add_cmd (name, class, fun, doc, list)
     char *name;
     enum command_class class;
     void (*fun) PARAMS ((char *, int));
     char *doc;
     struct cmd_list_element **list;
{
  register struct cmd_list_element *c
    = (struct cmd_list_element *) xmalloc (sizeof (struct cmd_list_element));

  delete_cmd (name, list);
  c->next = *list;
  c->name = name;
  c->class = class;
  c->function.cfunc = fun;
  c->doc = doc;
  c->prefixlist = 0;
  c->prefixname = (char *)NULL;
  c->allow_unknown = 0;
  c->hook = 0;
  c->hookee = 0;
  c->cmd_pointer = 0;
  c->abbrev_flag = 0;
  c->type = not_set_cmd;
  c->completer = make_symbol_completion_list;
  c->var = 0;
  c->var_type = var_boolean;
  c->user_commands = 0;
  *list = c;
  return c;
}

/* Same as above, except that the abbrev_flag is set. */

#if 0	/* Currently unused */

struct cmd_list_element *
add_abbrev_cmd (name, class, fun, doc, list)
     char *name;
     enum command_class class;
     void (*fun) PARAMS ((char *, int));
     char *doc;
     struct cmd_list_element **list;
{
  register struct cmd_list_element *c
    = add_cmd (name, class, fun, doc, list);

  c->abbrev_flag = 1;
  return c;
}

#endif

struct cmd_list_element *
add_alias_cmd (name, oldname, class, abbrev_flag, list)
     char *name;
     char *oldname;
     enum command_class class;
     int abbrev_flag;
     struct cmd_list_element **list;
{
  /* Must do this since lookup_cmd tries to side-effect its first arg */
  char *copied_name;
  register struct cmd_list_element *old;
  register struct cmd_list_element *c;
  copied_name = (char *) alloca (strlen (oldname) + 1);
  strcpy (copied_name, oldname);
  old  = lookup_cmd (&copied_name, *list, "", 1, 1);

  if (old == 0)
    {
      delete_cmd (name, list);
      return 0;
    }

  c = add_cmd (name, class, old->function.cfunc, old->doc, list);
  c->prefixlist = old->prefixlist;
  c->prefixname = old->prefixname;
  c->allow_unknown = old->allow_unknown;
  c->abbrev_flag = abbrev_flag;
  c->cmd_pointer = old;
  return c;
}

/* Like add_cmd but adds an element for a command prefix:
   a name that should be followed by a subcommand to be looked up
   in another command list.  PREFIXLIST should be the address
   of the variable containing that list.  */

struct cmd_list_element *
add_prefix_cmd (name, class, fun, doc, prefixlist, prefixname,
		allow_unknown, list)
     char *name;
     enum command_class class;
     void (*fun) PARAMS ((char *, int));
     char *doc;
     struct cmd_list_element **prefixlist;
     char *prefixname;
     int allow_unknown;
     struct cmd_list_element **list;
{
  register struct cmd_list_element *c = add_cmd (name, class, fun, doc, list);
  c->prefixlist = prefixlist;
  c->prefixname = prefixname;
  c->allow_unknown = allow_unknown;
  return c;
}

/* Like add_prefix_cmd but sets the abbrev_flag on the new command. */
   
struct cmd_list_element *
add_abbrev_prefix_cmd (name, class, fun, doc, prefixlist, prefixname,
		       allow_unknown, list)
     char *name;
     enum command_class class;
     void (*fun) PARAMS ((char *, int));
     char *doc;
     struct cmd_list_element **prefixlist;
     char *prefixname;
     int allow_unknown;
     struct cmd_list_element **list;
{
  register struct cmd_list_element *c = add_cmd (name, class, fun, doc, list);
  c->prefixlist = prefixlist;
  c->prefixname = prefixname;
  c->allow_unknown = allow_unknown;
  c->abbrev_flag = 1;
  return c;
}

/* This is an empty "cfunc".  */
void
not_just_help_class_command (args, from_tty)
     char *args;
     int from_tty;
{
}

/* This is an empty "sfunc".  */
static void empty_sfunc PARAMS ((char *, int, struct cmd_list_element *));

static void
empty_sfunc (args, from_tty, c)
     char *args;
     int from_tty;
     struct cmd_list_element *c;
{
}

/* Add element named NAME to command list LIST (the list for set
   or some sublist thereof).
   CLASS is as in add_cmd.
   VAR_TYPE is the kind of thing we are setting.
   VAR is address of the variable being controlled by this command.
   DOC is the documentation string.  */

struct cmd_list_element *
add_set_cmd (name, class, var_type, var, doc, list)
     char *name;
     enum command_class class;
     var_types var_type;
     char *var;
     char *doc;
     struct cmd_list_element **list;
{
  struct cmd_list_element *c
    = add_cmd (name, class, NO_FUNCTION, doc, list);

  c->type = set_cmd;
  c->var_type = var_type;
  c->var = var;
  /* This needs to be something besides NO_FUNCTION so that this isn't
     treated as a help class.  */
  c->function.sfunc = empty_sfunc;
  return c;
}

/* Where SETCMD has already been added, add the corresponding show
   command to LIST and return a pointer to it.  */
struct cmd_list_element *
add_show_from_set (setcmd, list)
     struct cmd_list_element *setcmd;
     struct cmd_list_element **list;
{
  struct cmd_list_element *showcmd =
    (struct cmd_list_element *) xmalloc (sizeof (struct cmd_list_element));

  memcpy (showcmd, setcmd, sizeof (struct cmd_list_element));
  delete_cmd (showcmd->name, list);
  showcmd->type = show_cmd;
  
  /* Replace "set " at start of docstring with "show ".  */
  if (setcmd->doc[0] == 'S' && setcmd->doc[1] == 'e'
      && setcmd->doc[2] == 't' && setcmd->doc[3] == ' ')
    showcmd->doc = concat ("Show ", setcmd->doc + 4, NULL);
  else
    fprintf_unfiltered (gdb_stderr, "GDB internal error: Bad docstring for set command\n");
  
  showcmd->next = *list;
  *list = showcmd;
  return showcmd;
}

/* Remove the command named NAME from the command list.  */

void
delete_cmd (name, list)
     char *name;
     struct cmd_list_element **list;
{
  register struct cmd_list_element *c;
  struct cmd_list_element *p;

  while (*list && STREQ ((*list)->name, name))
    {
      if ((*list)->hookee)
	(*list)->hookee->hook = 0;	/* Hook slips out of its mouth */
      p = (*list)->next;
      free ((PTR)*list);
      *list = p;
    }

  if (*list)
    for (c = *list; c->next;)
      {
	if (STREQ (c->next->name, name))
	  {
	    if (c->next->hookee)
	      c->next->hookee->hook = 0;  /* hooked cmd gets away.  */
	    p = c->next->next;
	    free ((PTR)c->next);
	    c->next = p;
	  }
	else
	  c = c->next;
      }
}

/* This command really has to deal with two things:
 *     1) I want documentation on *this string* (usually called by
 * "help commandname").
 *     2) I want documentation on *this list* (usually called by
 * giving a command that requires subcommands.  Also called by saying
 * just "help".)
 *
 *   I am going to split this into two seperate comamnds, help_cmd and
 * help_list. 
 */

void
help_cmd (command, stream)
     char *command;
     GDB_FILE *stream;
{
  struct cmd_list_element *c;
  extern struct cmd_list_element *cmdlist;

  if (!command)
    {
      help_list (cmdlist, "", all_classes, stream);
      return;
    }

  c = lookup_cmd (&command, cmdlist, "", 0, 0);

  if (c == 0)
    return;

  /* There are three cases here.
     If c->prefixlist is nonzero, we have a prefix command.
     Print its documentation, then list its subcommands.
     
     If c->function is nonzero, we really have a command.
     Print its documentation and return.
     
     If c->function is zero, we have a class name.
     Print its documentation (as if it were a command)
     and then set class to the number of this class
     so that the commands in the class will be listed.  */

  fputs_filtered (c->doc, stream);
  fputs_filtered ("\n", stream);

  if (c->prefixlist == 0 && c->function.cfunc != NULL)
    return;
  fprintf_filtered (stream, "\n");

  /* If this is a prefix command, print it's subcommands */
  if (c->prefixlist)
    help_list (*c->prefixlist, c->prefixname, all_commands, stream);

  /* If this is a class name, print all of the commands in the class */
  if (c->function.cfunc == NULL)
    help_list (cmdlist, "", c->class, stream);

  if (c->hook)
    fprintf_filtered (stream, "\nThis command has a hook defined: %s\n",
		      c->hook->name);
}

/*
 * Get a specific kind of help on a command list.
 *
 * LIST is the list.
 * CMDTYPE is the prefix to use in the title string.
 * CLASS is the class with which to list the nodes of this list (see
 * documentation for help_cmd_list below),  As usual, ALL_COMMANDS for
 * everything, ALL_CLASSES for just classes, and non-negative for only things
 * in a specific class.
 * and STREAM is the output stream on which to print things.
 * If you call this routine with a class >= 0, it recurses.
 */
void
help_list (list, cmdtype, class, stream)
     struct cmd_list_element *list;
     char *cmdtype;
     enum command_class class;
     GDB_FILE *stream;
{
  int len;
  char *cmdtype1, *cmdtype2;
  
  /* If CMDTYPE is "foo ", CMDTYPE1 gets " foo" and CMDTYPE2 gets "foo sub"  */
  len = strlen (cmdtype);
  cmdtype1 = (char *) alloca (len + 1);
  cmdtype1[0] = 0;
  cmdtype2 = (char *) alloca (len + 4);
  cmdtype2[0] = 0;
  if (len)
    {
      cmdtype1[0] = ' ';
      strncpy (cmdtype1 + 1, cmdtype, len - 1);
      cmdtype1[len] = 0;
      strncpy (cmdtype2, cmdtype, len - 1);
      strcpy (cmdtype2 + len - 1, " sub");
    }

  if (class == all_classes)
    fprintf_filtered (stream, "List of classes of %scommands:\n\n", cmdtype2);
  else
    fprintf_filtered (stream, "List of %scommands:\n\n", cmdtype2);

  help_cmd_list (list, class, cmdtype, (int)class >= 0, stream);

  if (class == all_classes)
    fprintf_filtered (stream, "\n\
Type \"help%s\" followed by a class name for a list of commands in that class.",
	     cmdtype1);

  fprintf_filtered (stream, "\n\
Type \"help%s\" followed by %scommand name for full documentation.\n\
Command name abbreviations are allowed if unambiguous.\n",
	   cmdtype1, cmdtype2);
}
     
/* Print only the first line of STR on STREAM.  */
static void
print_doc_line (stream, str)
     GDB_FILE *stream;
     char *str;
{
  static char *line_buffer = 0;
  static int line_size;
  register char *p;

  if (!line_buffer)
    {
      line_size = 80;
      line_buffer = (char *) xmalloc (line_size);
    }

  p = str;
  while (*p && *p != '\n' && *p != '.' && *p != ',')
    p++;
  if (p - str > line_size - 1)
    {
      line_size = p - str + 1;
      free ((PTR)line_buffer);
      line_buffer = (char *) xmalloc (line_size);
    }
  strncpy (line_buffer, str, p - str);
  line_buffer[p - str] = '\0';
  if (islower (line_buffer[0]))
    line_buffer[0] = toupper (line_buffer[0]);
  fputs_filtered (line_buffer, stream);
}

/*
 * Implement a help command on command list LIST.
 * RECURSE should be non-zero if this should be done recursively on
 * all sublists of LIST.
 * PREFIX is the prefix to print before each command name.
 * STREAM is the stream upon which the output should be written.
 * CLASS should be:
 *	A non-negative class number to list only commands in that
 * class.
 *	ALL_COMMANDS to list all commands in list.
 *	ALL_CLASSES  to list all classes in list.
 *
 *   Note that RECURSE will be active on *all* sublists, not just the
 * ones selected by the criteria above (ie. the selection mechanism
 * is at the low level, not the high-level).
 */
void
help_cmd_list (list, class, prefix, recurse, stream)
     struct cmd_list_element *list;
     enum command_class class;
     char *prefix;
     int recurse;
     GDB_FILE *stream;
{
  register struct cmd_list_element *c;

  for (c = list; c; c = c->next)
    {
      if (c->abbrev_flag == 0 &&
	  (class == all_commands
	  || (class == all_classes && c->function.cfunc == NULL)
	  || (class == c->class && c->function.cfunc != NULL)))
	{
	  fprintf_filtered (stream, "%s%s -- ", prefix, c->name);
	  print_doc_line (stream, c->doc);
	  fputs_filtered ("\n", stream);
	}
      if (recurse
	  && c->prefixlist != 0
	  && c->abbrev_flag == 0)
	help_cmd_list (*c->prefixlist, class, c->prefixname, 1, stream);
    }
}

/* This routine takes a line of TEXT and a CLIST in which to start the
   lookup.  When it returns it will have incremented the text pointer past
   the section of text it matched, set *RESULT_LIST to point to the list in
   which the last word was matched, and will return a pointer to the cmd
   list element which the text matches.  It will return NULL if no match at
   all was possible.  It will return -1 (cast appropriately, ick) if ambigous
   matches are possible; in this case *RESULT_LIST will be set to point to
   the list in which there are ambiguous choices (and *TEXT will be set to
   the ambiguous text string).

   If the located command was an abbreviation, this routine returns the base
   command of the abbreviation.

   It does no error reporting whatsoever; control will always return
   to the superior routine.

   In the case of an ambiguous return (-1), *RESULT_LIST will be set to point
   at the prefix_command (ie. the best match) *or* (special case) will be NULL
   if no prefix command was ever found.  For example, in the case of "info a",
   "info" matches without ambiguity, but "a" could be "args" or "address", so
   *RESULT_LIST is set to the cmd_list_element for "info".  So in this case
   RESULT_LIST should not be interpeted as a pointer to the beginning of a
   list; it simply points to a specific command.  In the case of an ambiguous
   return *TEXT is advanced past the last non-ambiguous prefix (e.g.
   "info t" can be "info types" or "info target"; upon return *TEXT has been
   advanced past "info ").

   If RESULT_LIST is NULL, don't set *RESULT_LIST (but don't otherwise
   affect the operation).

   This routine does *not* modify the text pointed to by TEXT.
   
   If IGNORE_HELP_CLASSES is nonzero, ignore any command list elements which
   are actually help classes rather than commands (i.e. the function field of
   the struct cmd_list_element is NULL).  */

struct cmd_list_element *
lookup_cmd_1 (text, clist, result_list, ignore_help_classes)
     char **text;
     struct cmd_list_element *clist, **result_list;
     int ignore_help_classes;
{
  char *p, *command;
  int len, tmp, nfound;
  struct cmd_list_element *found, *c;

  while (**text == ' ' || **text == '\t')
    (*text)++;

  /* Treating underscores as part of command words is important
     so that "set args_foo()" doesn't get interpreted as
     "set args _foo()".  */
  for (p = *text;
       *p && (isalnum(*p) || *p == '-' || *p == '_');
       p++)
    ;

  /* If nothing but whitespace, return 0.  */
  if (p == *text)
    return 0;
  
  len = p - *text;

  /* *text and p now bracket the first command word to lookup (and
     it's length is len).  We copy this into a local temporary,
     converting to lower case as we go.  */

  command = (char *) alloca (len + 1);
  for (tmp = 0; tmp < len; tmp++)
    {
      char x = (*text)[tmp];
      command[tmp] = isupper(x) ? tolower(x) : x;
    }
  command[len] = '\0';

  /* Look it up.  */
  found = 0;
  nfound = 0;
  for (c = clist; c; c = c->next)
    if (!strncmp (command, c->name, len)
	&& (!ignore_help_classes || c->function.cfunc))
      {
	found = c;
	nfound++;
	if (c->name[len] == '\0')
	  {
	    nfound = 1;
	    break;
	  }
      }

  /* If nothing matches, we have a simple failure.  */
  if (nfound == 0)
    return 0;

  if (nfound > 1)
    {
      if (result_list != NULL)
	/* Will be modified in calling routine
	   if we know what the prefix command is.  */
	*result_list = 0;		
      return (struct cmd_list_element *) -1; /* Ambiguous.  */
    }

  /* We've matched something on this list.  Move text pointer forward. */

  *text = p;

  /* If this was an abbreviation, use the base command instead.  */

  if (found->cmd_pointer)
    found = found->cmd_pointer;

  /* If we found a prefix command, keep looking.  */

  if (found->prefixlist)
    {
      c = lookup_cmd_1 (text, *found->prefixlist, result_list,
			ignore_help_classes);
      if (!c)
	{
	  /* Didn't find anything; this is as far as we got.  */
	  if (result_list != NULL)
	    *result_list = clist;
	  return found;
	}
      else if (c == (struct cmd_list_element *) -1)
	{
	  /* We've gotten this far properley, but the next step
	     is ambiguous.  We need to set the result list to the best
	     we've found (if an inferior hasn't already set it).  */
	  if (result_list != NULL)
	    if (!*result_list)
	      /* This used to say *result_list = *found->prefixlist
		 If that was correct, need to modify the documentation
		 at the top of this function to clarify what is supposed
		 to be going on.  */
	      *result_list = found;
	  return c;
	}
      else
	{
	  /* We matched!  */
	  return c;
	}
    }
  else
    {
      if (result_list != NULL)
	*result_list = clist;
      return found;
    }
}

/* All this hair to move the space to the front of cmdtype */

static void
undef_cmd_error (cmdtype, q)
     char *cmdtype, *q;
{
  error ("Undefined %scommand: \"%s\".  Try \"help%s%.*s\".",
    cmdtype,
    q,
    *cmdtype? " ": "",
    strlen(cmdtype)-1,
    cmdtype);
}

/* Look up the contents of *LINE as a command in the command list LIST.
   LIST is a chain of struct cmd_list_element's.
   If it is found, return the struct cmd_list_element for that command
   and update *LINE to point after the command name, at the first argument.
   If not found, call error if ALLOW_UNKNOWN is zero
   otherwise (or if error returns) return zero.
   Call error if specified command is ambiguous,
   unless ALLOW_UNKNOWN is negative.
   CMDTYPE precedes the word "command" in the error message.

   If INGNORE_HELP_CLASSES is nonzero, ignore any command list
   elements which are actually help classes rather than commands (i.e.
   the function field of the struct cmd_list_element is 0).  */

struct cmd_list_element *
lookup_cmd (line, list, cmdtype, allow_unknown, ignore_help_classes)
     char **line;
     struct cmd_list_element *list;
     char *cmdtype;
     int allow_unknown;
     int ignore_help_classes;
{
  struct cmd_list_element *last_list = 0;
  struct cmd_list_element *c =
    lookup_cmd_1 (line, list, &last_list, ignore_help_classes);
#if 0
  /* This is wrong for complete_command.  */
  char *ptr = (*line) + strlen (*line) - 1;

  /* Clear off trailing whitespace.  */
  while (ptr >= *line && (*ptr == ' ' || *ptr == '\t'))
    ptr--;
  *(ptr + 1) = '\0';
#endif
  
  if (!c)
    {
      if (!allow_unknown)
	{
	  if (!*line)
	    error ("Lack of needed %scommand", cmdtype);
	  else
	    {
	      char *p = *line, *q;

	      while (isalnum(*p) || *p == '-')
		p++;

	      q = (char *) alloca (p - *line + 1);
	      strncpy (q, *line, p - *line);
	      q[p-*line] = '\0';
	      undef_cmd_error (cmdtype, q);
	    }
	}
      else
	return 0;
    }
  else if (c == (struct cmd_list_element *) -1)
    {
      /* Ambigous.  Local values should be off prefixlist or called
	 values.  */
      int local_allow_unknown = (last_list ? last_list->allow_unknown :
				 allow_unknown);
      char *local_cmdtype = last_list ? last_list->prefixname : cmdtype;
      struct cmd_list_element *local_list =
	(last_list ? *(last_list->prefixlist) : list);
      
      if (local_allow_unknown < 0)
	{
	  if (last_list)
	    return last_list;	/* Found something.  */
	  else
	    return 0;		/* Found nothing.  */
	}
      else
	{
	  /* Report as error.  */
	  int amb_len;
	  char ambbuf[100];

	  for (amb_len = 0;
	       ((*line)[amb_len] && (*line)[amb_len] != ' '
		&& (*line)[amb_len] != '\t');
	       amb_len++)
	    ;
	  
	  ambbuf[0] = 0;
	  for (c = local_list; c; c = c->next)
	    if (!strncmp (*line, c->name, amb_len))
	      {
		if (strlen (ambbuf) + strlen (c->name) + 6 < (int)sizeof ambbuf)
		  {
		    if (strlen (ambbuf))
		      strcat (ambbuf, ", ");
		    strcat (ambbuf, c->name);
		  }
		else
		  {
		    strcat (ambbuf, "..");
		    break;
		  }
	      }
	  error ("Ambiguous %scommand \"%s\": %s.", local_cmdtype,
		 *line, ambbuf);
	  return 0;		/* lint */
	}
    }
  else
    {
      /* We've got something.  It may still not be what the caller
         wants (if this command *needs* a subcommand).  */
      while (**line == ' ' || **line == '\t')
	(*line)++;

      if (c->prefixlist && **line && !c->allow_unknown)
	undef_cmd_error (c->prefixname, *line);

      /* Seems to be what he wants.  Return it.  */
      return c;
    }
  return 0;
}
	
#if 0
/* Look up the contents of *LINE as a command in the command list LIST.
   LIST is a chain of struct cmd_list_element's.
   If it is found, return the struct cmd_list_element for that command
   and update *LINE to point after the command name, at the first argument.
   If not found, call error if ALLOW_UNKNOWN is zero
   otherwise (or if error returns) return zero.
   Call error if specified command is ambiguous,
   unless ALLOW_UNKNOWN is negative.
   CMDTYPE precedes the word "command" in the error message.  */

struct cmd_list_element *
lookup_cmd (line, list, cmdtype, allow_unknown)
     char **line;
     struct cmd_list_element *list;
     char *cmdtype;
     int allow_unknown;
{
  register char *p;
  register struct cmd_list_element *c, *found;
  int nfound;
  char ambbuf[100];
  char *processed_cmd;
  int i, cmd_len;

  /* Skip leading whitespace.  */

  while (**line == ' ' || **line == '\t')
    (*line)++;

  /* Clear out trailing whitespace.  */

  p = *line + strlen (*line);
  while (p != *line && (p[-1] == ' ' || p[-1] == '\t'))
    p--;
  *p = 0;

  /* Find end of command name.  */

  p = *line;
  while (*p == '-' || isalnum(*p))
    p++;

  /* Look up the command name.
     If exact match, keep that.
     Otherwise, take command abbreviated, if unique.  Note that (in my
     opinion) a null string does *not* indicate ambiguity; simply the
     end of the argument.  */

  if (p == *line)
    {
      if (!allow_unknown)
	error ("Lack of needed %scommand", cmdtype);
      return 0;
    }
  
  /* Copy over to a local buffer, converting to lowercase on the way.
     This is in case the command being parsed is a subcommand which
     doesn't match anything, and that's ok.  We want the original
     untouched for the routine of the original command.  */
  
  processed_cmd = (char *) alloca (p - *line + 1);
  for (cmd_len = 0; cmd_len < p - *line; cmd_len++)
    {
      char x = (*line)[cmd_len];
      if (isupper(x))
	processed_cmd[cmd_len] = tolower(x);
      else
	processed_cmd[cmd_len] = x;
    }
  processed_cmd[cmd_len] = '\0';

  /* Check all possibilities in the current command list.  */
  found = 0;
  nfound = 0;
  for (c = list; c; c = c->next)
    {
      if (!strncmp (processed_cmd, c->name, cmd_len))
	{
	  found = c;
	  nfound++;
	  if (c->name[cmd_len] == 0)
	    {
	      nfound = 1;
	      break;
	    }
	}
    }

  /* Report error for undefined command name.  */

  if (nfound != 1)
    {
      if (nfound > 1 && allow_unknown >= 0)
	{
	  ambbuf[0] = 0;
	  for (c = list; c; c = c->next)
	    if (!strncmp (processed_cmd, c->name, cmd_len))
	      {
		if (strlen (ambbuf) + strlen (c->name) + 6 < sizeof ambbuf)
		  {
		    if (strlen (ambbuf))
		      strcat (ambbuf, ", ");
		    strcat (ambbuf, c->name);
		  }
		else
		  {
		    strcat (ambbuf, "..");
		    break;
		  }
	      }
	  error ("Ambiguous %scommand \"%s\": %s.", cmdtype,
		 processed_cmd, ambbuf);
	}
      else if (!allow_unknown)
	error ("Undefined %scommand: \"%s\".", cmdtype, processed_cmd);
      return 0;
    }

  /* Skip whitespace before the argument.  */

  while (*p == ' ' || *p == '\t') p++;
  *line = p;

  if (found->prefixlist && *p)
    {
      c = lookup_cmd (line, *found->prefixlist, found->prefixname,
		      found->allow_unknown);
      if (c)
	return c;
    }

  return found;
}
#endif

/* Helper function for SYMBOL_COMPLETION_FUNCTION.  */

/* Return a vector of char pointers which point to the different
   possible completions in LIST of TEXT.  

   WORD points in the same buffer as TEXT, and completions should be
   returned relative to this position.  For example, suppose TEXT is "foo"
   and we want to complete to "foobar".  If WORD is "oo", return
   "oobar"; if WORD is "baz/foo", return "baz/foobar".  */

char **
complete_on_cmdlist (list, text, word)
     struct cmd_list_element *list;
     char *text;
     char *word;
{
  struct cmd_list_element *ptr;
  char **matchlist;
  int sizeof_matchlist;
  int matches;
  int textlen = strlen (text);

  sizeof_matchlist = 10;
  matchlist = (char **) xmalloc (sizeof_matchlist * sizeof (char *));
  matches = 0;

  for (ptr = list; ptr; ptr = ptr->next)
    if (!strncmp (ptr->name, text, textlen)
	&& !ptr->abbrev_flag
	&& (ptr->function.cfunc
	    || ptr->prefixlist))
      {
	if (matches == sizeof_matchlist)
	  {
	    sizeof_matchlist *= 2;
	    matchlist = (char **) xrealloc ((char *)matchlist,
					    (sizeof_matchlist
					     * sizeof (char *)));
	  }

	matchlist[matches] = (char *) 
	  xmalloc (strlen (word) + strlen (ptr->name) + 1);
	if (word == text)
	  strcpy (matchlist[matches], ptr->name);
	else if (word > text)
	  {
	    /* Return some portion of ptr->name.  */
	    strcpy (matchlist[matches], ptr->name + (word - text));
	  }
	else
	  {
	    /* Return some of text plus ptr->name.  */
	    strncpy (matchlist[matches], word, text - word);
	    matchlist[matches][text - word] = '\0';
	    strcat (matchlist[matches], ptr->name);
	  }
	++matches;
      }

  if (matches == 0)
    {
      free ((PTR)matchlist);
      matchlist = 0;
    }
  else
    {
      matchlist = (char **) xrealloc ((char *)matchlist, ((matches + 1)
						* sizeof (char *)));
      matchlist[matches] = (char *) 0;
    }

  return matchlist;
}

static int
parse_binary_operation (arg)
     char *arg;
{
  int length;

  if (!arg || !*arg)
    return 1;

  length = strlen (arg);

  while (arg[length - 1] == ' ' || arg[length - 1] == '\t')
    length--;

  if (!strncmp (arg, "on", length)
      || !strncmp (arg, "1", length)
      || !strncmp (arg, "yes", length))
    return 1;
  else
    if (!strncmp (arg, "off", length)
	|| !strncmp (arg, "0", length)
	|| !strncmp (arg, "no", length))
      return 0;
    else 
      {
	error ("\"on\" or \"off\" expected.");
	return 0;
      }
}

/* Do a "set" or "show" command.  ARG is NULL if no argument, or the text
   of the argument, and FROM_TTY is nonzero if this command is being entered
   directly by the user (i.e. these are just like any other
   command).  C is the command list element for the command.  */
void
do_setshow_command (arg, from_tty, c)
     char *arg;
     int from_tty;
     struct cmd_list_element *c;
{
  if (c->type == set_cmd)
    {
      switch (c->var_type)
	{
	case var_string:
	  {
	    char *new;
	    char *p;
	    char *q;
	    int ch;
	    
	    if (arg == NULL)
	      arg = "";
	    new = (char *) xmalloc (strlen (arg) + 2);
	    p = arg; q = new;
	    while ((ch = *p++) != '\000')
	      {
		if (ch == '\\')
		  {
		    /* \ at end of argument is used after spaces
		       so they won't be lost.  */
		    /* This is obsolete now that we no longer strip
		       trailing whitespace and actually, the backslash
		       didn't get here in my test, readline or
		       something did something funky with a backslash
		       right before a newline.  */
		    if (*p == 0)
		      break;
		    ch = parse_escape (&p);
		    if (ch == 0)
		      break; /* C loses */
		    else if (ch > 0)
		      *q++ = ch;
		  }
		else
		  *q++ = ch;
	      }
#if 0
	    if (*(p - 1) != '\\')
	      *q++ = ' ';
#endif
	    *q++ = '\0';
	    new = (char *) xrealloc (new, q - new);
	    if (*(char **)c->var != NULL)
	      free (*(char **)c->var);
	    *(char **) c->var = new;
	  }
	  break;
	case var_string_noescape:
	  if (arg == NULL)
	    arg = "";
	  if (*(char **)c->var != NULL)
	    free (*(char **)c->var);
	  *(char **) c->var = savestring (arg, strlen (arg));
	  break;
	case var_filename:
	  if (arg == NULL)
	    error_no_arg ("filename to set it to.");
	  if (*(char **)c->var != NULL)
	    free (*(char **)c->var);
	  *(char **)c->var = tilde_expand (arg);
	  break;
	case var_boolean:
	  *(int *) c->var = parse_binary_operation (arg);
	  break;
	case var_uinteger:
	  if (arg == NULL)
	    error_no_arg ("integer to set it to.");
	  *(unsigned int *) c->var = parse_and_eval_address (arg);
	  if (*(unsigned int *) c->var == 0)
	    *(unsigned int *) c->var = UINT_MAX;
	  break;
	case var_integer:
	  {
	    unsigned int val;
	    if (arg == NULL)
	      error_no_arg ("integer to set it to.");
	    val = parse_and_eval_address (arg);
	    if (val == 0)
	      *(int *) c->var = INT_MAX;
	    else if (val >= INT_MAX)
	      error ("integer %u out of range", val);
	    else
	      *(int *) c->var = val;
	    break;
	  }
	case var_zinteger:
	  if (arg == NULL)
	    error_no_arg ("integer to set it to.");
	  *(int *) c->var = parse_and_eval_address (arg);
	  break;
	default:
	  error ("gdb internal error: bad var_type in do_setshow_command");
	}
    }
  else if (c->type == show_cmd)
    {
      /* Print doc minus "show" at start.  */
      print_doc_line (gdb_stdout, c->doc + 5);
      
      fputs_filtered (" is ", gdb_stdout);
      wrap_here ("    ");
      switch (c->var_type)
	{
      case var_string:
	{
	  unsigned char *p;
	  fputs_filtered ("\"", gdb_stdout);
	  for (p = *(unsigned char **) c->var; *p != '\0'; p++)
	    gdb_printchar (*p, gdb_stdout, '"');
	  fputs_filtered ("\"", gdb_stdout);
	}
	break;
      case var_string_noescape:
      case var_filename:
	fputs_filtered ("\"", gdb_stdout);
	fputs_filtered (*(char **) c->var, gdb_stdout);
	fputs_filtered ("\"", gdb_stdout);
	break;
      case var_boolean:
	fputs_filtered (*(int *) c->var ? "on" : "off", gdb_stdout);
	break;
      case var_uinteger:
	if (*(unsigned int *) c->var == UINT_MAX) {
	  fputs_filtered ("unlimited", gdb_stdout);
	  break;
	}
	/* else fall through */
      case var_zinteger:
	fprintf_filtered (gdb_stdout, "%u", *(unsigned int *) c->var);
	break;
      case var_integer:
	if (*(int *) c->var == INT_MAX)
	  {
	    fputs_filtered ("unlimited", gdb_stdout);
	  }
	else
	  fprintf_filtered (gdb_stdout, "%d", *(int *) c->var);
	break;
	    
      default:
	error ("gdb internal error: bad var_type in do_setshow_command");
      }
      fputs_filtered (".\n", gdb_stdout);
    }
  else
    error ("gdb internal error: bad cmd_type in do_setshow_command");
  (*c->function.sfunc) (NULL, from_tty, c);
}

/* Show all the settings in a list of show commands.  */

void
cmd_show_list (list, from_tty, prefix)
     struct cmd_list_element *list;
     int from_tty;
     char *prefix;
{
  for (; list != NULL; list = list->next) {
    /* If we find a prefix, run its list, prefixing our output by its
       prefix (with "show " skipped).  */
    if (list->prefixlist && !list->abbrev_flag)
      cmd_show_list (*list->prefixlist, from_tty, list->prefixname + 5);
    if (list->type == show_cmd)
      {
	fputs_filtered (prefix, gdb_stdout);
	fputs_filtered (list->name, gdb_stdout);
	fputs_filtered (":  ", gdb_stdout);
	do_setshow_command ((char *)NULL, from_tty, list);
      }
  }
}

/* ARGSUSED */
static void
shell_escape (arg, from_tty)
     char *arg;
     int from_tty;
{
#ifdef CANT_FORK
  /* FIXME: what about errors (I don't know how GO32 system() handles
     them)?  */
  system (arg);
#else /* Can fork.  */
  int rc, status, pid;
  char *p, *user_shell;

  if ((user_shell = (char *) getenv ("SHELL")) == NULL)
    user_shell = "/bin/sh";

  /* Get the name of the shell for arg0 */
  if ((p = strrchr (user_shell, '/')) == NULL)
    p = user_shell;
  else
    p++;			/* Get past '/' */

  if ((pid = fork()) == 0)
    {
      if (!arg)
	execl (user_shell, p, 0);
      else
	execl (user_shell, p, "-c", arg, 0);

      fprintf_unfiltered (gdb_stderr, "Cannot execute %s: %s\n", user_shell,
			  safe_strerror (errno));
      gdb_flush (gdb_stderr);
      _exit (0177);
    }

  if (pid != -1)
    while ((rc = wait (&status)) != pid && rc != -1)
      ;
  else
    error ("Fork failed");
#endif /* Can fork.  */
}

static void
make_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  char *p;

  if (arg == 0)
    p = "make";
  else
    {
      p = xmalloc (sizeof("make ") + strlen(arg));
      strcpy (p, "make ");
      strcpy (p + sizeof("make ")-1, arg);
    }
  
  shell_escape (p, from_tty);
}

static void
show_user_1 (c, stream)
     struct cmd_list_element *c;
     GDB_FILE *stream;
{
  register struct command_line *cmdlines;

  cmdlines = c->user_commands;
  if (!cmdlines)
    return;
  fputs_filtered ("User command ", stream);
  fputs_filtered (c->name, stream);
  fputs_filtered (":\n", stream);
  while (cmdlines)
    {
      fputs_filtered (cmdlines->line, stream); 
      fputs_filtered ("\n", stream); 
      cmdlines = cmdlines->next;
    }
  fputs_filtered ("\n", stream);
}

/* ARGSUSED */
static void
show_user (args, from_tty)
     char *args;
     int from_tty;
{
  struct cmd_list_element *c;
  extern struct cmd_list_element *cmdlist;

  if (args)
    {
      c = lookup_cmd (&args, cmdlist, "", 0, 1);
      if (c->class != class_user)
	error ("Not a user command.");
      show_user_1 (c, gdb_stdout);
    }
  else
    {
      for (c = cmdlist; c; c = c->next)
	{
	  if (c->class == class_user)
	    show_user_1 (c, gdb_stdout);
	}
    }
}

void
_initialize_command ()
{
  add_com ("shell", class_support, shell_escape,
	   "Execute the rest of the line as a shell command.  \n\
With no arguments, run an inferior shell.");
  add_com ("make", class_support, make_command,
	   "Run the ``make'' program using the rest of the line as arguments.");
  add_cmd ("user", no_class, show_user, 
	   "Show definitions of user defined commands.\n\
Argument is the name of the user defined command.\n\
With no argument, show definitions of all user defined commands.", &showlist);
}
