/* Stack unwinding code based on dwarf2 frame info for GDB, the GNU debugger.
   Copyright 2001
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

#ifndef DWARF2CFI_H
#define DWARF2CFI_H

/* Return the frame address.  */
CORE_ADDR cfi_read_fp ();

/* Store the frame address.  */
void cfi_write_fp (CORE_ADDR val);

/* Restore the machine to the state it had before the current frame
   was created.  */
void cfi_pop_frame (struct frame_info *);

/* Determine the address of the calling function's frame.  */
CORE_ADDR cfi_frame_chain (struct frame_info *fi);

/* Sets the pc of the frame.  */
void cfi_init_frame_pc (int fromleaf, struct frame_info *fi);

/* Initialize unwind context informations of the frame.  */
void cfi_init_extra_frame_info (int fromleaf, struct frame_info *fi);

/* Obtain return address of the frame.  */
CORE_ADDR cfi_get_ra (struct frame_info *fi);

/* Find register number REGNUM relative to FRAME and put its
   (raw) contents in *RAW_BUFFER.  Set *OPTIMIZED if the variable
   was optimized out (and thus can't be fetched).  If the variable
   was fetched from memory, set *ADDRP to where it was fetched from,
   otherwise it was fetched from a register.

   The argument RAW_BUFFER must point to aligned memory.  */
void cfi_get_saved_register (char *raw_buffer,
			     int *optimized,
			     CORE_ADDR * addrp,
			     struct frame_info *frame,
			     int regnum, enum lval_type *lval);

/*  Return the register that the function uses for a frame pointer,
    plus any necessary offset to be applied to the register before
    any frame pointer offsets.  */
void cfi_virtual_frame_pointer (CORE_ADDR pc, int *frame_regnum,
				LONGEST * frame_offset);

#endif
