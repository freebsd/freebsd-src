/* Library for reading command lines and decoding commands.
   Copyright (C) 1986, 1989 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "command.h"
#include "defs.h"
#include <stdio.h>
#include <ctype.h>

extern char *xmalloc ();

/* Add element named NAME to command list *LIST.
   FUN should be the function to execute the command;
   it will get a character string as argument, with leading
   and trailing blanks already eliminated.

   DOC is a documentation string for the command.
   Its first line should be a complete sentence.
   It should start with ? for a command that is an abbreviation
   or with * for a command that most users don't need to know about.  */

struct cmd_list_element *
add_cmd (name, class, fun, doc, list)
     char *name;
     int class;
     void (*fun) ();
     char *doc;
     struct cmd_list_element **list;
{
  register struct cmd_list_element *c
    = (struct cmd_list_element *) xmalloc (sizeof (struct cmd_list_element));

  delete_cmd (name, list);
  c->next = *list;
  c->name = savestring (name, strlen (name));
  c->class = class;
  c->function = fun;
  c->doc = doc;
  c->prefixlist = 0;
  c->allow_unknown = 0;
  c->abbrev_flag = 0;
  c->aux = 0;
  *list = c;
  return c;
}

/* Same as above, except that the abbrev_flag is set. */

struct cmd_list_element *
add_abbrev_cmd (name, class, fun, doc, list)
     char *name;
     int class;
     void (*fun) ();
     char *doc;
     struct cmd_list_element **list;
{
  register struct cmd_list_element *c
    = (struct cmd_list_element *) xmalloc (sizeof (struct cmd_list_element));

  delete_cmd (name, list);
  c->next = *list;
  c->name = savestring (name, strlen (name));
  c->class = class;
  c->function = fun;
  c->doc = doc;
  c->prefixlist = 0;
  c->allow_unknown = 0;
  c->abbrev_flag = 1;
  c->aux = 0;
  *list = c;
  return c;
}

struct cmd_list_element *
add_alias_cmd (name, oldname, class, abbrev_flag, list)
     char *name;
     char *oldname;
     int class;
     int abbrev_flag;
     struct cmd_list_element **list;
{
  /* Must do this since lookup_cmd tries to side-effect its first arg */
  char *copied_name;
  register struct cmd_list_element *old;
  register struct cmd_list_element *c;
  copied_name = (char *) alloca (strlen (oldname) + 1);
  strcpy (copied_name, oldname);
  old  = lookup_cmd (&copied_name, *list, 0, 1, 1);

  if (old == 0)
    {
      delete_cmd (name, list);
      return 0;
    }

  c = add_cmd (name, class, old->function, old->doc, list);
  c->prefixlist = old->prefixlist;
  c->prefixname = old->prefixname;
  c->allow_unknown = old->allow_unknown;
  c->abbrev_flag = abbrev_flag;
  c->aux = old->aux;
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
     int class;
     void (*fun) ();
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

/* Like add_prefix_cmd butsets the abbrev_flag on the new command. */
   
struct cmd_list_element *
add_abbrev_prefix_cmd (name, class, fun, doc, prefixlist, prefixname,
		allow_unknown, list)
     char *name;
     int class;
     void (*fun) ();
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

/* Remove the command named NAME from the command list.  */

void
delete_cmd (name, list)
     char *name;
     struct cmd_list_element **list;
{
  register struct cmd_list_element *c;

  while (*list && !strcmp ((*list)->name, name))
    {
      *list = (*list)->next;
    }

  if (*list)
    for (c = *list; c->next;)
      {
	if (!strcmp (c->next->name, name))
	  c->next = c->next->next;
	else
	  c = c->next;
      }
}

void help_cmd (), help_list (), help_cmd_list ();

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
     FILE *stream;
{
  struct cmd_list_element *c;
  extern struct cmd_list_element *cmdlist;

  if (!command)
    {
      help_list (cmdlist, "", -2, stream);
      return;
    }

  c = lookup_cmd (&command, cmdlist, "", 0, 0);

  if (c == 0)
    return;

  /* There are three cases here.
     If c->prefixlist is nonzer, we have a prefix command.
     Print its documentation, then list its subcommands.
     
     If c->function is nonzero, we really have a command.
     Print its documentation and return.
     
     If c->function is zero, we have a class name.
     Print its documentation (as if it were a command)
     and then set class to he number of this class
     so that the commands in the class will be listed.  */

  fputs_filtered (c->doc, stream);
  fputs_filtered ("\n", stream);

  if (c->prefixlist == 0 && c->function != 0)
    return;
  fprintf_filtered (stream, "\n");

  /* If this is a prefix command, print it's subcommands */
  if (c->prefixlist)
    help_list (*c->prefixlist, c->prefixname, -1, stream);

  /* If this is a class name, print all of the commands in the class */
  if (c->function == 0)
    help_list (cmdlist, "", c->class, stream);
}

/*
 * Get a specific kind of help on a command list.
 *
 * LIST is the list.
 * CMDTYPE is the prefix to use in the title string.
 * CLASS is the class with which to list the nodes of this list (see
 * documentation for help_cmd_list below),  As usual, -1 for
 * everything, -2 for just classes, and non-negative for only things
 * in a specific class.
 * and STREAM is the output stream on which to print things.
 * If you call this routine with a class >= 0, it recurses.
 */
void
help_list (list, cmdtype, class, stream)
     struct cmd_list_element *list;
     char *cmdtype;
     int class;
     FILE *stream;
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

  if (class == -2)
    fprintf_filtered (stream, "List of classes of %scommands:\n\n", cmdtype2);
  else
    fprintf_filtered (stream, "List of %scommands:\n\n", cmdtype2);

  help_cmd_list (list, class, cmdtype, (class >= 0), stream);

  if (class == -2)
    fprintf_filtered (stream, "\n\
Type \"help%s\" followed by a class name for a list of commands in that class.",
	     cmdtype1);

  fprintf_filtered (stream, "\n\
Type \"help%s\" followed by %scommand name for full documentation.\n\
Command name abbreviations are allowed if unambiguous.\n",
	   cmdtype1, cmdtype2);
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
 *	-1 to list all commands in list.
 *	-2 to list all classes in list.
 *
 *   Note that RECURSE will be active on *all* sublists, not just the
 * ones seclected by the criteria above (ie. the selection mechanism
 * is at the low level, not the high-level).
 */
void
help_cmd_list (list, class, prefix, recurse, stream)
     struct cmd_list_element *list;
     int class;
     char *prefix;
     int recurse;
     FILE *stream;
{
  register struct cmd_list_element *c;
  register char *p;
  static char *line_buffer = 0;
  static int line_size;

  if (!line_buffer)
    {
      line_size = 80;
      line_buffer = (char *) xmalloc (line_size);
    }

  for (c = list; c; c = c->next)
    {
      if (c->abbrev_flag == 0 &&
	  (class == -1
	  || (class == -2 && c->function == 0)
	  || (class == c->class && c->function != 0)))
	{
	  fprintf_filtered (stream, "%s%s -- ", prefix, c->name);
	  /* Print just the first line */
	  p = c->doc;
	  while (*p && *p != '\n') p++;
	  if (p - c->doc > line_size - 1)
	    {
	      line_size = p - c->doc + 1;
	      free (line_buffer);
	      line_buffer = (char *) xmalloc (line_size);
	    }
	  strncpy (line_buffer, c->doc, p - c->doc);
	  line_buffer[p - c->doc] = '\0';
	  fputs_filtered (line_buffer, stream);
	  fputs_filtered ("\n", stream);
	}
      if (recurse
	  && c->prefixlist != 0
	  && c->abbrev_flag == 0)
	help_cmd_list (*c->prefixlist, class, c->prefixname, 1, stream);
    }
}

/* This routine takes a line of TEXT and a CLIST in which to
   start the lookup.  When it returns it will have incremented the text
   pointer past the section of text it matched, set *RESULT_LIST to
   the list in which the last word was matched, and will return the
   cmd list element which the text matches.  It will return 0 if no
   match at all was possible.  It will return -1 if ambigous matches are
   possible; in this case *RESULT_LIST will be set to the list in which
   there are ambiguous choices (and text will be set to the ambiguous
   text string).

   It does no error reporting whatsoever; control will always return
   to the superior routine.

   In the case of an ambiguous return (-1), *RESULT_LIST will be set to
   point at the prefix_command (ie. the best match) *or* (special
   case) will be 0 if no prefix command was ever found.  For example,
   in the case of "info a", "info" matches without ambiguity, but "a"
   could be "args" or "address", so *RESULT_LIST is set to
   the cmd_list_element for "info".  So in this case
   result list should not be interpeted as a pointer to the beginning
   of a list; it simply points to a specific command.

   This routine does *not* modify the text pointed to by TEXT.
   
   If INGNORE_HELP_CLASSES is nonzero, ignore any command list
   elements which are actually help classes rather than commands (i.e.
   the function field of the struct cmd_list_element is 0).  */

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
      command[tmp] = (x >= 'A' && x <= 'Z') ? x - 'A' + 'a' : x;
    }
  command[len] = '\0';

  /* Look it up.  */
  found = 0;
  nfound = 0;
  for (c = clist; c; c = c->next)
    if (!strncmp (command, c->name, len)
	&& (!ignore_help_classes || c->function))
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
      *result_list = 0;		/* Will be modified in calling routine
				   if we know what the prefix command is.
				   */
      return (struct cmd_list_element *) -1; /* Ambiguous.  */
    }

  /* We've matched something on this list.  Move text pointer forward. */

  *text = p;
  if (found->prefixlist)
    {
      c = lookup_cmd_1 (text, *found->prefixlist, result_list,
			ignore_help_classes);
      if (!c)
	{
	  /* Didn't find anything; this is as far as we got.  */
	  *result_list = clist;
	  return found;
	}
      else if (c == (struct cmd_list_element *) -1)
	{
	  /* We've gotten this far properley, but the next step
	     is ambiguous.  We need to set the result list to the best
	     we've found (if an inferior hasn't already set it).  */
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
      *result_list = clist;
      return found;
    }
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
  char *ptr = (*line) + strlen (*line) - 1;

  /* Clear off trailing whitespace.  */
  while (ptr >= *line && (*ptr == ' ' || *ptr == '\t'))
    ptr--;
  *(ptr + 1) = '\0';
  
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
	      
	      error ("Undefined %scommand: \"%s\".", cmdtype, q);
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
	  error ("Ambiguous %scommand \"%s\": %s.", local_cmdtype,
		 *line, ambbuf);
	}
    }
  else
    {
      /* We've got something.  It may still not be what the caller
         wants (if this command *needs* a subcommand).  */
      while (**line == ' ' || **line == '\t')
	(*line)++;

      if (c->prefixlist && **line && !c->allow_unknown)
	error ("Undefined %scommand: \"%s\".", c->prefixname, *line);

      /* Seems to be what he wants.  Return it.  */
      return c;
    }
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
  while (*p == '-'
	 || (*p >= 'a' && *p <= 'z')
	 || (*p >= 'A' && *p <= 'Z')
	 || (*p >= '0' && *p <= '9'))
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
      if (x >= 'A' && x <= 'Z')
	processed_cmd[cmd_len] = x - 'A' + 'a';
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
   possible completions in LIST of TEXT.  */

char **
complete_on_cmdlist (list, text)
     struct cmd_list_element *list;
     char *text;
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
	&& (ptr->function
	    || ptr->prefixlist))
      {
	if (matches == sizeof_matchlist)
	  {
	    sizeof_matchlist *= 2;
	    matchlist = (char **) xrealloc (matchlist,
					    (sizeof_matchlist
					     * sizeof (char *)));
	  }

	matchlist[matches] = (char *) 
	  xmalloc (strlen (ptr->name) + 1);
	strcpy (matchlist[matches++], ptr->name);
      }

  if (matches == 0)
    {
      free (matchlist);
      matchlist = 0;
    }
  else
    {
      matchlist = (char **) xrealloc (matchlist, ((matches + 1)
						* sizeof (char *)));
      matchlist[matches] = (char *) 0;
    }

  return matchlist;
}

static void
shell_escape (arg, from_tty)
     char *arg;
     int from_tty;
{
  int rc, status, pid;
  char *p, *user_shell;
  extern char *rindex ();

  if ((user_shell = (char *) getenv ("SHELL")) == NULL)
    user_shell = "/bin/sh";

  /* Get the name of the shell for arg0 */
  if ((p = rindex (user_shell, '/')) == NULL)
    p = user_shell;
  else
    p++;			/* Get past '/' */

  if ((pid = fork()) == 0)
    {
      if (!arg)
	execl (user_shell, p, 0);
      else
	execl (user_shell, p, "-c", arg, 0);

      fprintf (stderr, "Exec of shell failed\n");
      exit (0);
    }

  if (pid != -1)
    while ((rc = wait (&status)) != pid && rc != -1)
      ;
  else
    error ("Fork failed");
}

void
_initialize_command ()
{
  add_com ("shell", class_support, shell_escape,
	   "Execute the rest of the line as a shell command.  \n\
With no arguments, run an inferior shell.");
}
