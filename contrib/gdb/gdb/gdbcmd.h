/* Header file for GDB-specific command-line stuff.
   Copyright 1986, 1989, 1990, 1992 Free Software Foundation, Inc.

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

#if !defined (GDBCMD_H)
#define GDBCMD_H 1

#include "command.h"

/* Chain containing all defined commands.  */

extern struct cmd_list_element *cmdlist;

/* Chain containing all defined info subcommands.  */

extern struct cmd_list_element *infolist;

/* Chain containing all defined enable subcommands. */

extern struct cmd_list_element *enablelist;

/* Chain containing all defined disable subcommands. */

extern struct cmd_list_element *disablelist;

/* Chain containing all defined delete subcommands. */

extern struct cmd_list_element *deletelist;

/* Chain containing all defined "enable breakpoint" subcommands. */

extern struct cmd_list_element *enablebreaklist;

/* Chain containing all defined set subcommands */

extern struct cmd_list_element *setlist;

/* Chain containing all defined unset subcommands */

extern struct cmd_list_element *unsetlist;

/* Chain containing all defined show subcommands.  */

extern struct cmd_list_element *showlist;

/* Chain containing all defined \"set history\".  */

extern struct cmd_list_element *sethistlist;

/* Chain containing all defined \"show history\".  */

extern struct cmd_list_element *showhistlist;

/* Chain containing all defined \"unset history\".  */

extern struct cmd_list_element *unsethistlist;

/* Chain containing all defined maintenance subcommands. */

extern struct cmd_list_element *maintenancelist;

/* Chain containing all defined "maintenance info" subcommands. */

extern struct cmd_list_element *maintenanceinfolist;

/* Chain containing all defined "maintenance print" subcommands. */

extern struct cmd_list_element *maintenanceprintlist;

extern struct cmd_list_element *setprintlist;

extern struct cmd_list_element *showprintlist;

extern struct cmd_list_element *setchecklist;

extern struct cmd_list_element *showchecklist;

extern void
execute_user_command PARAMS ((struct cmd_list_element *, char *));

extern void
execute_command PARAMS ((char *, int));

enum command_control_type
execute_control_command PARAMS ((struct command_line *));

void print_command_line PARAMS ((struct command_line *, unsigned int));

extern char **noop_completer PARAMS ((char *, char *));

extern char **filename_completer PARAMS ((char *, char *));

#endif	/* !defined (GDBCMD_H) */
