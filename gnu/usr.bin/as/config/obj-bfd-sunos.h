/*
  Copyright (C) 1987, 1992 Free Software Foundation, Inc.

  This file is part of GAS, the GNU Assembler.

  GAS is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  GAS is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with GAS; see the file COPYING.  If not, write to
  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * This file is obj-bfd-sunos.h.
 */

/* define an obj specific macro off which target cpu back ends may key. */
#define OBJ_BFD
#define OBJ_BFD_SUNOS

#include "bfd.h"

/* include whatever target cpu is appropriate. */
#include "targ-cpu.h"

/*
 * SYMBOLS
 */

/*
 * If your object format needs to reorder symbols, define this.  When
 * defined, symbols are kept on a doubly linked list and functions are
 * made available for push, insert, append, and delete.  If not defined,
 * symbols are kept on a singly linked list, only the append and clear
 * facilities are available, and they are macros.
 */

/* #define SYMBOLS_NEED_PACKPOINTERS */

typedef asymbol obj_symbol_type;
typedef void *object_headers;

#define S_SET_NAME(s, v)		((s)->sy_symbol.name = (v))
#define S_GET_NAME(s)		((s)->sy_symbol.name)
#define S_SET_SEGMENT(s,v)	((s)->sy_symbol.udata = (v))
#define S_GET_SEGMENT(s)	((s)->sy_symbol.udata)
#define S_SET_EXTERNAL(s)	((s)->sy_symbol.flags |= BSF_GLOBAL)
#define S_SET_VALUE(s,v)	((s)->sy_symbol.value  = (v))
#define S_GET_VALUE(s)		((s)->sy_symbol.value)
#define S_IS_DEFINED(s)		(!((s)->sy_symbol.flags & BSF_UNDEFINED))

#define DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE (0) /* your magic number */
#define OBJ_EMIT_LINENO(a,b,c) /* must be *something*.  This no-op's it out. */

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of obj-bfd-sunos.h */
