/* Macro defintions for IBM AIX PS/2 (i386).
   Copyright 1986, 1987, 1989, 1992, 1993 Free Software Foundation, Inc.

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

/* Changes for IBM AIX PS/2 by Minh Tran-Le (tranle@intellicorp.com).  */

#ifndef TM_I386AIX_H
#define TM_I386AIX_H 1

#include "i386/tm-i386.h"
#include <sys/reg.h>

#ifndef I386
# define I386 1
#endif
#ifndef I386_AIX_TARGET
# define I386_AIX_TARGET 1
#endif

/* Nonzero if register N requires conversion
   from raw format to virtual format.  */

#undef  REGISTER_CONVERTIBLE
#define REGISTER_CONVERTIBLE(N) \
  ((N < FP0_REGNUM) ? 0 : 1)

/* Convert data from raw format for register REGNUM in buffer FROM
   to virtual format with type TYPE in buffer TO.  */

#undef REGISTER_CONVERT_TO_VIRTUAL
#define REGISTER_CONVERT_TO_VIRTUAL(REGNUM,TYPE,FROM,TO) \
{ \
  double val; \
  i387_to_double ((FROM), (char *)&val); \
  store_floating ((TO), TYPE_LENGTH (TYPE), val); \
}
extern void
i387_to_double PARAMS ((char *, char *));

/* Convert data from virtual format with type TYPE in buffer FROM
   to raw format for register REGNUM in buffer TO.  */

#undef REGISTER_CONVERT_TO_RAW
#define REGISTER_CONVERT_TO_RAW(TYPE,REGNUM,FROM,TO) \
{ \
  double val = extract_floating ((FROM), TYPE_LENGTH (TYPE)); \
  double_to_i387((char *)&val, (TO)); \
}
extern void
double_to_i387 PARAMS ((char *, char *));

#endif /* TM_I386AIX_H */
