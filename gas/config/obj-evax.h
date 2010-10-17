/* This file is obj-evax.h
   Copyright 1996, 2000 Free Software Foundation, Inc.
   Contributed by Klaus Kämpf (kkaempf@progis.de) of
     proGIS Software, Aachen, Germany.

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
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.  */

/*
 * This file is obj-evax.h and is intended to be a template for
 * object format specific header files.
 */

/* define an obj specific macro off which target cpu back ends may key.  */
#define OBJ_EVAX 1

/* include whatever target cpu is appropriate.  */
#include "targ-cpu.h"

#ifdef BFD_ASSEMBLER
#define OUTPUT_FLAVOR bfd_target_evax_flavour
#endif

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

/*  */
typedef struct
  {
    void *nothing;
  }
obj_symbol_type;		/* should be the format's symbol structure */

typedef void *object_headers;

#define DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE (0)	/* your magic number */

#define OBJ_EMIT_LINENO(a,b,c)	/* must be *something*.  This no-op's it out.  */

#define obj_symbol_new_hook(s)        {;}

#define S_SET_OTHER(S,V)
#define S_SET_TYPE(S,T)
#define S_SET_DESC(S,D)
#define S_GET_OTHER(S)	0
#define S_GET_TYPE(S)	0
#define S_GET_DESC(S)	0

#define PDSC_S_K_KIND_FP_STACK 9
#define PDSC_S_K_KIND_FP_REGISTER 10
#define PDSC_S_K_KIND_NULL 8

#define PDSC_S_K_MIN_STACK_SIZE 32
#define PDSC_S_K_MIN_REGISTER_SIZE 24
#define PDSC_S_K_NULL_SIZE 16

#define PDSC_S_M_BASE_REG_IS_FP 0x80	/* low byte */
#define PDSC_S_M_NATIVE 0x10		/* high byte */
#define PDSC_S_M_NO_JACKET 0x20		/* high byte */

#define LKP_S_K_SIZE 16

#define TC_IMPLICIT_LCOMM_ALIGNMENT(SIZE, P2VAR) (P2VAR) = 3

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */
