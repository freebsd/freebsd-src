/* Header file for command-reading library command.c.
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

/* This structure records one command'd definition.  */

struct cmd_list_element
  {
    /* Points to next command in this list.  */
    struct cmd_list_element *next;

    /* Name of this command.  */
    char *name;

    /* Command class; class values are chosen by application program.  */
    int class;

    /* Function definition of this command.
       Zero for command class names and for help topics that
       are not really commands.  */
    void (*function) ();

    /* Documentation of this command (or help topic).
       First line is brief documentation; remaining lines form, with it,
       the full documentation.  First line should end with a period.
       Entire string should also end with a period, not a newline.  */
    char *doc;

    /* Auxiliary information.
       It is up to the calling program to decide what this means.  */
    char *aux;

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
  };

/* Forward-declarations of the entry-points of command.c.  */

extern struct cmd_list_element *add_cmd ();
extern struct cmd_list_element *add_alias_cmd ();
extern struct cmd_list_element *add_prefix_cmd ();
extern struct cmd_list_element *lookup_cmd (), *lookup_cmd_1 ();
extern char **complete_on_cmdlist ();
extern void delete_cmd ();
extern void help_cmd ();
