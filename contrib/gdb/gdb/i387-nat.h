/* Native-dependent code for the i387.
   Copyright 2000, 2001 Free Software Foundation, Inc.

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

#ifndef I387_NAT_H
#define I387_NAT_H

/* Fill register REGNUM in GDB's register array with the appropriate
   value from *FSAVE.  This function masks off any of the reserved
   bits in *FSAVE.  */

extern void i387_supply_register (int regnum, char *fsave);

/* Fill GDB's register array with the floating-point register values
   in *FSAVE.  This function masks off any of the reserved
   bits in *FSAVE.  */

extern void i387_supply_fsave (char *fsave);

/* Fill register REGNUM (if it is a floating-point register) in *FSAVE
   with the value in GDB's register array.  If REGNUM is -1, do this
   for all registers.  This function doesn't touch any of the reserved
   bits in *FSAVE.  */

extern void i387_fill_fsave (char *fsave, int regnum);

/* Fill GDB's register array with the floating-point and SSE register
   values in *FXSAVE.  This function masks off any of the reserved
   bits in *FXSAVE.  */

extern void i387_supply_fxsave (char *fxsave);

/* Fill register REGNUM (if it is a floating-point or SSE register) in
   *FXSAVE with the value in GDB's register array.  If REGNUM is -1, do
   this for all registers.  This function doesn't touch any of the
   reserved bits in *FXSAVE.  */

extern void i387_fill_fxsave (char *fxsave, int regnum);

#endif /* i387-nat.h */
