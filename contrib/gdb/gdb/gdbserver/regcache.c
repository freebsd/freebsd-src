/* Register support routines for the remote server for GDB.
   Copyright 2001, 2002
   Free Software Foundation, Inc.

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

#include "server.h"
#include "regdef.h"

#include <stdlib.h>
#include <string.h>

static char *registers;
static int register_bytes;

static struct reg *reg_defs;
static int num_registers;

const char **gdbserver_expedite_regs;

int
registers_length (void)
{
  return 2 * register_bytes;
}

void
set_register_cache (struct reg *regs, int n)
{
  int offset, i;
  
  reg_defs = regs;
  num_registers = n;

  offset = 0;
  for (i = 0; i < n; i++)
    {
      regs[i].offset = offset;
      offset += regs[i].size;
    }

  register_bytes = offset / 8;
  registers = malloc (offset / 8);
  if (!registers)
    fatal ("Could not allocate register cache.");
}

void
registers_to_string (char *buf)
{
  convert_int_to_ascii (registers, buf, register_bytes);
}

void
registers_from_string (char *buf)
{
  int len = strlen (buf);

  if (len != register_bytes * 2)
    {
      warning ("Wrong sized register packet (expected %d bytes, got %d)", 2*register_bytes, len);
      if (len > register_bytes * 2)
	len = register_bytes * 2;
    }
  convert_ascii_to_int (buf, registers, len / 2);
}

struct reg *
find_register_by_name (const char *name)
{
  int i;

  for (i = 0; i < num_registers; i++)
    if (!strcmp (name, reg_defs[i].name))
      return &reg_defs[i];
  fatal ("Unknown register %s requested", name);
  return 0;
}

int
find_regno (const char *name)
{
  int i;

  for (i = 0; i < num_registers; i++)
    if (!strcmp (name, reg_defs[i].name))
      return i;
  fatal ("Unknown register %s requested", name);
  return -1;
}

struct reg *
find_register_by_number (int n)
{
  return &reg_defs[n];
}

int
register_size (int n)
{
  return reg_defs[n].size / 8;
}

char *
register_data (int n)
{
  return registers + (reg_defs[n].offset / 8);
}

void
supply_register (int n, const char *buf)
{
  memcpy (register_data (n), buf, register_size (n));
}

void
supply_register_by_name (const char *name, const char *buf)
{
  supply_register (find_regno (name), buf);
}

void
collect_register (int n, char *buf)
{
  memcpy (buf, register_data (n), register_size (n));
}

void
collect_register_by_name (const char *name, char *buf)
{
  collect_register (find_regno (name), buf);
}
