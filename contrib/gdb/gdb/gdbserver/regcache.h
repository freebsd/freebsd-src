/* Register support routines for the remote server for GDB.
   Copyright 2001, 2002 Free Software Foundation, Inc.

   This file is part of GDB.

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

#ifndef REGCACHE_H
#define REGCACHE_H

/* Convert all registers to a string in the currently specified remote
   format.  */

void registers_to_string (char *buf);

/* Convert a string to register values and fill our register cache.  */

void registers_from_string (char *buf);

/* Return the size in bytes of a string-encoded register packet.  */

int registers_length (void);

/* Return a pointer to the description of register ``n''.  */

struct reg *find_register_by_number (int n);

char *register_data (int n);

int register_size (int n);

int find_regno (const char *name);

extern const char **gdbserver_expedite_regs;

#endif /* REGCACHE_H */
