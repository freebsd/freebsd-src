/* Cache and manage the values of registers for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1994, 1995, 1996, 1998, 2000, 2001
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

#ifndef REGCACHE_H
#define REGCACHE_H

/* Transfer a raw register [0..NUM_REGS) between core-gdb and the
   regcache. */

void regcache_read (int rawnum, char *buf);
void regcache_write (int rawnum, char *buf);

/* Transfer a raw register [0..NUM_REGS) between the regcache and the
   target.  These functions are called by the target in response to a
   target_fetch_registers() or target_store_registers().  */

extern void supply_register (int regnum, char *val);
extern void regcache_collect (int regnum, void *buf);


/* DEPRECATED: Character array containing an image of the inferior
   programs' registers for the most recently referenced thread. */

extern char *registers;

/* DEPRECATED: Character array containing the current state of each
   register (unavailable<0, invalid=0, valid>0) for the most recently
   referenced thread. */

extern signed char *register_valid;

extern int register_cached (int regnum);

extern void set_register_cached (int regnum, int state);

extern void register_changed (int regnum);

extern void registers_changed (void);

extern void registers_fetched (void);

extern void read_register_bytes (int regbyte, char *myaddr, int len);

extern void read_register_gen (int regnum, char *myaddr);

extern void write_register_gen (int regnum, char *myaddr);

extern void write_register_bytes (int regbyte, char *myaddr, int len);

/* Rename to read_unsigned_register()? */
extern ULONGEST read_register (int regnum);

/* Rename to read_unsigned_register_pid()? */
extern ULONGEST read_register_pid (int regnum, ptid_t ptid);

extern LONGEST read_signed_register (int regnum);

extern LONGEST read_signed_register_pid (int regnum, ptid_t ptid);

extern void write_register (int regnum, LONGEST val);

extern void write_register_pid (int regnum, CORE_ADDR val, ptid_t ptid);

#endif /* REGCACHE_H */
