/* Sequent Symmetry target interface, for GDB.
   Copyright (C) 1986, 1987, 1989, 1991, 1994 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* many 387-specific items of use taken from i386-dep.c */

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"

#include <signal.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/dir.h>
#include <sys/ioctl.h>
#include "gdb_stat.h"
#include "gdbcore.h"
#include <fcntl.h>

void
symmetry_extract_return_value(type, regbuf, valbuf)
     struct type *type;
     char *regbuf;
     char *valbuf;
{
  union { 
    double	d; 
    int	l[2]; 
  } xd; 
  struct minimal_symbol *msymbol;
  float f;

  if (TYPE_CODE_FLT == TYPE_CODE(type)) { 
    msymbol = lookup_minimal_symbol ("1167_flt", NULL, NULL);
    if (msymbol != NULL) {
      /* found "1167_flt" means 1167, %fp2-%fp3 */ 
      /* float & double; 19= %fp2, 20= %fp3 */
      /* no single precision on 1167 */
      xd.l[1] = *((int *)&regbuf[REGISTER_BYTE(19)]);
      xd.l[0] = *((int *)&regbuf[REGISTER_BYTE(20)]);
      switch (TYPE_LENGTH(type)) {
      case 4:
	/* FIXME: broken for cross-debugging.  */
	f = (float) xd.d;
	memcpy (valbuf, &f, TYPE_LENGTH(type));
	break;
      case 8:
	/* FIXME: broken for cross-debugging.  */
	memcpy (valbuf, &xd.d, TYPE_LENGTH(type)); 
	break;
      default:
	error("Unknown floating point size");
	break;
      }
    } else { 
      /* 387 %st(0), gcc uses this */ 
      i387_to_double(((int *)&regbuf[REGISTER_BYTE(3)]),
		     &xd.d); 
      switch (TYPE_LENGTH(type)) {
      case 4:			/* float */
	f = (float) xd.d;
	/* FIXME: broken for cross-debugging.  */
	memcpy (valbuf, &f, 4); 
	break;
      case 8:			/* double */
	/* FIXME: broken for cross-debugging.  */
	memcpy (valbuf, &xd.d, 8);
	break;
      default:
	error("Unknown floating point size");
	break;
      }
    }
  } else {
    memcpy (valbuf, regbuf, TYPE_LENGTH (type)); 
  }
}
