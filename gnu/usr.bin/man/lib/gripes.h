/*
 * gripes.h
 *
 * Copyright (c) 1990, 1991, John W. Eaton.
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with the man
 * distribution.  
 *
 * John W. Eaton
 * jwe@che.utexas.edu
 * Department of Chemical Engineering
 * The University of Texas at Austin
 * Austin, Texas  78712
 */

extern void gripe_no_name ();
extern void gripe_converting_name ();
extern void gripe_system_command ();
extern void gripe_reading_man_file ();
extern void gripe_not_found ();
extern void gripe_invalid_section ();
extern void gripe_manpath ();
extern void gripe_alloc ();
extern void gripe_incompatible ();
extern void gripe_getting_mp_config ();
extern void gripe_reading_mp_config ();
extern void gripe_roff_command_from_file ();
extern void gripe_roff_command_from_env ();
extern void gripe_roff_command_from_command_line ();
