/* Macro definitions for i386 running under the win32 API Unix.
   Copyright 1995, 1996 Free Software Foundation, Inc.

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


#include "i386/tm-i386v.h"

#undef MAX_REGISTER_RAW_SIZE
#undef MAX_REGISTER_VIRTUAL_SIZE
#undef NUM_REGS
#undef REGISTER_BYTE
#undef REGISTER_BYTES
#undef REGISTER_CONVERTIBLE
#undef REGISTER_CONVERT_TO_RAW
#undef REGISTER_CONVERT_TO_VIRTUAL
#undef REGISTER_NAMES
#undef REGISTER_RAW_SIZE
#undef REGISTER_VIRTUAL_SIZE
#undef REGISTER_VIRTUAL_TYPE

/* Number of machine registers */

#define NUM_REGS 24

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */

/* the order of the first 8 registers must match the compiler's 
 * numbering scheme (which is the same as the 386 scheme)
 * also, this table must match regmap in i386-pinsn.c.
 */

#define REGISTER_NAMES { "eax",  "ecx",  "edx",  "ebx",  \
			 "esp",  "ebp",  "esi",  "edi",  \
			 "eip",  "ps",   "cs",   "ss",   \
			 "ds",   "es",   "fs",   "gs",   \
			 "st",   "st(1)","st(2)","st(3)",\
                         "st(4)","st(5)","st(6)","st(7)",}

#define FP0_REGNUM 16

/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.  */

#define REGISTER_BYTES (16 * 4 + 8 * 10)

/* Index within `registers' of the first byte of the space for
   register N.  */

#define REGISTER_BYTE(N) (((N) < 16) ? (N) * 4 : (((N) - 16) * 10) + (16 * 4))

/* Number of bytes of storage in the actual machine representation
   for register N.  */

#define REGISTER_RAW_SIZE(N) (((N) < 16) ? 4 : 10)

/* Number of bytes of storage in the program's representation
   for register N. */

#define REGISTER_VIRTUAL_SIZE(N) (((N) < 16) ? 4 : 10)

/* Largest value REGISTER_RAW_SIZE can have.  */

#define MAX_REGISTER_RAW_SIZE 10

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */

#define MAX_REGISTER_VIRTUAL_SIZE 10

/* Nonzero if register N requires conversion
   from raw format to virtual format.  */

#define REGISTER_CONVERTIBLE(N) \
  ((N < FP0_REGNUM) ? 0 : 1)

/* Convert data from raw format for register REGNUM in buffer FROM
   to virtual format with type TYPE in buffer TO.  */
extern void
i387_to_double PARAMS ((char *, char *));


#define REGISTER_CONVERT_TO_VIRTUAL(REGNUM,TYPE,FROM,TO) \
{ \
  double val; \
  i387_to_double ((FROM), (char *)&val); \
  store_floating ((TO), TYPE_LENGTH (TYPE), val); \
}

extern void
double_to_i387 PARAMS ((char *, char *));

#define REGISTER_CONVERT_TO_RAW(TYPE,REGNUM,FROM,TO) \
{ \
  double val = extract_floating ((FROM), TYPE_LENGTH (TYPE)); \
  double_to_i387((char *)&val, (TO)); \
}

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#define REGISTER_VIRTUAL_TYPE(N) \
  ((N < FP0_REGNUM) ? builtin_type_int : \
   builtin_type_double)

#define NAMES_HAVE_UNDERSCORE

#define IN_SOLIB_CALL_TRAMPOLINE(pc, name) skip_trampoline_code (pc, name)
#define SKIP_TRAMPOLINE_CODE(pc)           skip_trampoline_code (pc, 0)
extern CORE_ADDR skip_trampoline_code PARAMS ((CORE_ADDR pc, char *name));

extern char *cygwin_pid_to_str PARAMS ((int pid));
#define target_pid_to_str(PID) cygwin_pid_to_str (PID)
