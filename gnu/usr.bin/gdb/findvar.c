/* Find a variable's value in memory, for GDB, the GNU debugger.
   Copyright (C) 1986, 1987, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "param.h"
#include "symtab.h"
#include "frame.h"
#include "value.h"

CORE_ADDR read_register ();

/* Return the address in which frame FRAME's value of register REGNUM
   has been saved in memory.  Or return zero if it has not been saved.
   If REGNUM specifies the SP, the value we return is actually
   the SP value, not an address where it was saved.  */

CORE_ADDR
find_saved_register (frame, regnum)
     FRAME frame;
     int regnum;
{
  struct frame_info *fi;
  struct frame_saved_regs saved_regs;

  register FRAME frame1 = 0;
  register CORE_ADDR addr = 0;

#ifdef HAVE_REGISTER_WINDOWS
  /* We assume that a register in a register window will only be saved
     in one place (since the name changes and disappears as you go
     towards inner frames), so we only call get_frame_saved_regs on
     the current frame.  This is directly in contradiction to the
     usage below, which assumes that registers used in a frame must be
     saved in a lower (more interior) frame.  This change is a result
     of working on a register window machine; get_frame_saved_regs
     always returns the registers saved within a frame, within the
     context (register namespace) of that frame. */

  /* However, note that we don't want this to return anything if
     nothing is saved (if there's a frame inside of this one).  Also,
     callers to this routine asking for the stack pointer want the
     stack pointer saved for *this* frame; this is returned from the
     next frame.  */
     

  if (REGISTER_IN_WINDOW_P(regnum))
    {
      frame1 = get_next_frame (frame);
      if (!frame1) return 0;	/* Registers of this frame are
				   active.  */
      
      /* Get the SP from the next frame in; it will be this
	 current frame.  */
      if (regnum != SP_REGNUM)
	frame1 = frame;	
	  
      fi = get_frame_info (frame1);
      get_frame_saved_regs (fi, &saved_regs);
      return (saved_regs.regs[regnum] ?
	      saved_regs.regs[regnum] : 0);
    }
#endif /* HAVE_REGISTER_WINDOWS */

  /* Note that this next routine assumes that registers used in
     frame x will be saved only in the frame that x calls and
     frames interior to it.  This is not true on the sparc, but the
     above macro takes care of it, so we should be all right. */
  while (1)
    {
      QUIT;
      frame1 = get_prev_frame (frame1);
      if (frame1 == 0 || frame1 == frame)
	break;
      fi = get_frame_info (frame1);
      get_frame_saved_regs (fi, &saved_regs);
      if (saved_regs.regs[regnum])
	addr = saved_regs.regs[regnum];
    }

  return addr;
}

/* Copy the bytes of register REGNUM, relative to the current stack frame,
   into our memory at MYADDR.
   The number of bytes copied is REGISTER_RAW_SIZE (REGNUM).  */

void
read_relative_register_raw_bytes (regnum, myaddr)
     int regnum;
     char *myaddr;
{
  register CORE_ADDR addr;

  if (regnum == FP_REGNUM)
    {
      bcopy (&FRAME_FP(selected_frame), myaddr, sizeof (CORE_ADDR));
      return;
    }

  addr = find_saved_register (selected_frame, regnum);

  if (addr)
    {
      if (regnum == SP_REGNUM)
	{
	  CORE_ADDR buffer = addr;
	  bcopy (&buffer, myaddr, sizeof (CORE_ADDR));
	}
      else
	read_memory (addr, myaddr, REGISTER_RAW_SIZE (regnum));
      return;
    }
  read_register_bytes (REGISTER_BYTE (regnum),
		       myaddr, REGISTER_RAW_SIZE (regnum));
}

/* Return a `value' with the contents of register REGNUM
   in its virtual format, with the type specified by
   REGISTER_VIRTUAL_TYPE.  */

value
value_of_register (regnum)
     int regnum;
{
  register CORE_ADDR addr;
  register value val;
  char raw_buffer[MAX_REGISTER_RAW_SIZE];
  char virtual_buffer[MAX_REGISTER_VIRTUAL_SIZE];

  if (! (have_inferior_p () || have_core_file_p ()))
    error ("Can't get value of register without inferior or core file");
  
  addr =  find_saved_register (selected_frame, regnum);
  if (addr)
    {
      if (regnum == SP_REGNUM)
	return value_from_long (builtin_type_int, (LONGEST) addr);
      read_memory (addr, raw_buffer, REGISTER_RAW_SIZE (regnum));
    }
  else
    read_register_bytes (REGISTER_BYTE (regnum), raw_buffer,
			 REGISTER_RAW_SIZE (regnum));

  REGISTER_CONVERT_TO_VIRTUAL (regnum, raw_buffer, virtual_buffer);
  val = allocate_value (REGISTER_VIRTUAL_TYPE (regnum));
  bcopy (virtual_buffer, VALUE_CONTENTS (val), REGISTER_VIRTUAL_SIZE (regnum));
  VALUE_LVAL (val) = addr ? lval_memory : lval_register;
  VALUE_ADDRESS (val) = addr ? addr : REGISTER_BYTE (regnum);
  VALUE_REGNO (val) = regnum;
  return val;
}

/* Low level examining and depositing of registers.

   Note that you must call `fetch_registers' once
   before examining or depositing any registers.  */

char registers[REGISTER_BYTES];

/* Copy LEN bytes of consecutive data from registers
   starting with the REGBYTE'th byte of register data
   into memory at MYADDR.  */

void
read_register_bytes (regbyte, myaddr, len)
     int regbyte;
     char *myaddr;
     int len;
{
  bcopy (&registers[regbyte], myaddr, len);
}

/* Copy LEN bytes of consecutive data from memory at MYADDR
   into registers starting with the REGBYTE'th byte of register data.  */

void
write_register_bytes (regbyte, myaddr, len)
     int regbyte;
     char *myaddr;
     int len;
{
  bcopy (myaddr, &registers[regbyte], len);
  if (have_inferior_p ())
    store_inferior_registers (-1);
}

/* Return the contents of register REGNO,
   regarding it as an integer.  */

CORE_ADDR
read_register (regno)
     int regno;
{
  /* This loses when REGISTER_RAW_SIZE (regno) != sizeof (int) */
  return *(int *) &registers[REGISTER_BYTE (regno)];
}

/* Store VALUE in the register number REGNO, regarded as an integer.  */

void
write_register (regno, val)
     int regno, val;
{
  /* This loses when REGISTER_RAW_SIZE (regno) != sizeof (int) */
#if defined(sun4)
  /* This is a no-op on a Sun 4.  */
  if (regno == 0)
    return;
#endif

  *(int *) &registers[REGISTER_BYTE (regno)] = val;

  if (have_inferior_p ())
    store_inferior_registers (regno);
}

/* Record that register REGNO contains VAL.
   This is used when the value is obtained from the inferior or core dump,
   so there is no need to store the value there.  */

void
supply_register (regno, val)
     int regno;
     char *val;
{
  bcopy (val, &registers[REGISTER_BYTE (regno)], REGISTER_RAW_SIZE (regno));
}

/* Given a struct symbol for a variable,
   and a stack frame id, read the value of the variable
   and return a (pointer to a) struct value containing the value.  */

value
read_var_value (var, frame)
     register struct symbol *var;
     FRAME frame;
{
  register value v;

  struct frame_info *fi;

  struct type *type = SYMBOL_TYPE (var);
  register CORE_ADDR addr = 0;
  int val = SYMBOL_VALUE (var);
  register int len;

  v = allocate_value (type);
  VALUE_LVAL (v) = lval_memory;	/* The most likely possibility.  */
  len = TYPE_LENGTH (type);

  if (frame == 0) frame = selected_frame;

  switch (SYMBOL_CLASS (var))
    {
    case LOC_CONST:
    case LOC_LABEL:
      bcopy (&val, VALUE_CONTENTS (v), len);
      VALUE_LVAL (v) = not_lval;
      return v;

    case LOC_CONST_BYTES:
      bcopy (val, VALUE_CONTENTS (v), len);
      VALUE_LVAL (v) = not_lval;
      return v;

    case LOC_STATIC:
      addr = val;
      break;

/* Nonzero if a struct which is located in a register or a LOC_ARG
   really contains
   the address of the struct, not the struct itself.  GCC_P is nonzero
   if the function was compiled with GCC.  */
#if !defined (REG_STRUCT_HAS_ADDR)
#define REG_STRUCT_HAS_ADDR(gcc_p) 0
#endif

    case LOC_ARG:
      fi = get_frame_info (frame);
      addr = val + FRAME_ARGS_ADDRESS (fi);
      break;
      
    case LOC_REF_ARG:
      fi = get_frame_info (frame);
      addr = val + FRAME_ARGS_ADDRESS (fi);
      addr = read_memory_integer (addr, sizeof (CORE_ADDR));
      break;
      
    case LOC_LOCAL:
      fi = get_frame_info (frame);
      addr = val + FRAME_LOCALS_ADDRESS (fi);
      break;

    case LOC_TYPEDEF:
      error ("Cannot look up value of a typedef");

    case LOC_BLOCK:
      VALUE_ADDRESS (v) = BLOCK_START (SYMBOL_BLOCK_VALUE (var));
      return v;

    case LOC_REGISTER:
    case LOC_REGPARM:
      {
	struct block *b = get_frame_block (frame);

	v = value_from_register (type, val, frame);

	if (REG_STRUCT_HAS_ADDR(b->gcc_compile_flag)
	    && TYPE_CODE (type) == TYPE_CODE_STRUCT)
	  addr = *(CORE_ADDR *)VALUE_CONTENTS (v);
	else
	  return v;
      }
    }

  read_memory (addr, VALUE_CONTENTS (v), len);
  VALUE_ADDRESS (v) = addr;
  return v;
}

/* Return a value of type TYPE, stored in register REGNUM, in frame
   FRAME. */

value
value_from_register (type, regnum, frame)
     struct type *type;
     int regnum;
     FRAME frame;
{
  char raw_buffer [MAX_REGISTER_RAW_SIZE];
  char virtual_buffer[MAX_REGISTER_VIRTUAL_SIZE];
  CORE_ADDR addr;
  value v = allocate_value (type);
  int len = TYPE_LENGTH (type);
  char *value_bytes = 0;
  int value_bytes_copied = 0;
  int num_storage_locs;

  VALUE_REGNO (v) = regnum;

  num_storage_locs = (len > REGISTER_VIRTUAL_SIZE (regnum) ?
		      ((len - 1) / REGISTER_RAW_SIZE (regnum)) + 1 :
		      1);

  if (num_storage_locs > 1)
    {
      /* Value spread across multiple storage locations.  */
      
      int local_regnum;
      int mem_stor = 0, reg_stor = 0;
      int mem_tracking = 1;
      CORE_ADDR last_addr = 0;

      value_bytes = (char *) alloca (len + MAX_REGISTER_RAW_SIZE);

      /* Copy all of the data out, whereever it may be.  */

      for (local_regnum = regnum;
	   value_bytes_copied < len;
	   (value_bytes_copied += REGISTER_RAW_SIZE (local_regnum),
	    ++local_regnum))
	{
	  int register_index = local_regnum - regnum;
	  addr = find_saved_register (frame, local_regnum);
	  if (addr == 0)
	    {
	      read_register_bytes (REGISTER_BYTE (local_regnum),
				   value_bytes + value_bytes_copied,
				   REGISTER_RAW_SIZE (local_regnum));
	      reg_stor++;
	    }
	  else
	    {
	      read_memory (addr, value_bytes + value_bytes_copied,
			   REGISTER_RAW_SIZE (local_regnum));
	      mem_stor++;
	      mem_tracking =
		(mem_tracking
		 && (regnum == local_regnum
		     || addr == last_addr));
	    }
	  last_addr = addr;
	}

      if ((reg_stor && mem_stor)
	  || (mem_stor && !mem_tracking))
	/* Mixed storage; all of the hassle we just went through was
	   for some good purpose.  */
	{
	  VALUE_LVAL (v) = lval_reg_frame_relative;
	  VALUE_FRAME (v) = FRAME_FP (frame);
	  VALUE_FRAME_REGNUM (v) = regnum;
	}
      else if (mem_stor)
	{
	  VALUE_LVAL (v) = lval_memory;
	  VALUE_ADDRESS (v) = find_saved_register (frame, regnum);
	}
      else if (reg_stor)
	{
	  VALUE_LVAL (v) = lval_register;
	  VALUE_ADDRESS (v) = REGISTER_BYTE (regnum);
	}
      else
	fatal ("value_from_register: Value not stored anywhere!");
      
      /* Any structure stored in more than one register will always be
	 an inegral number of registers.  Otherwise, you'd need to do
	 some fiddling with the last register copied here for little
	 endian machines.  */

      /* Copy into the contents section of the value.  */
      bcopy (value_bytes, VALUE_CONTENTS (v), len);

      return v;
    }

  /* Data is completely contained within a single register.  Locate the
     register's contents in a real register or in core;
     read the data in raw format.  */
  
  addr = find_saved_register (frame, regnum);
  if (addr == 0)
    {
      /* Value is really in a register.  */
      
      VALUE_LVAL (v) = lval_register;
      VALUE_ADDRESS (v) = REGISTER_BYTE (regnum);
      
      read_register_bytes (REGISTER_BYTE (regnum),
			   raw_buffer, REGISTER_RAW_SIZE (regnum));
    }
  else
    {
      /* Value was in a register that has been saved in memory.  */
      
      read_memory (addr, raw_buffer, REGISTER_RAW_SIZE (regnum));
      VALUE_LVAL (v) = lval_memory;
      VALUE_ADDRESS (v) = addr;
    }
  
  /* Convert the raw contents to virtual contents.
     (Just copy them if the formats are the same.)  */
  
  REGISTER_CONVERT_TO_VIRTUAL (regnum, raw_buffer, virtual_buffer);
  
  if (REGISTER_CONVERTIBLE (regnum))
    {
      /* When the raw and virtual formats differ, the virtual format
	 corresponds to a specific data type.  If we want that type,
	 copy the data into the value.
	 Otherwise, do a type-conversion.  */
      
      if (type != REGISTER_VIRTUAL_TYPE (regnum))
	{
	  /* eg a variable of type `float' in a 68881 register
	     with raw type `extended' and virtual type `double'.
	     Fetch it as a `double' and then convert to `float'.  */
	  v = allocate_value (REGISTER_VIRTUAL_TYPE (regnum));
	  bcopy (virtual_buffer, VALUE_CONTENTS (v), len);
	  v = value_cast (type, v);
	}
      else
	bcopy (virtual_buffer, VALUE_CONTENTS (v), len);
    }
  else
    {
      /* Raw and virtual formats are the same for this register.  */

#ifdef BYTES_BIG_ENDIAN
      if (len < REGISTER_RAW_SIZE (regnum))
	{
  	  /* Big-endian, and we want less than full size.  */
	  VALUE_OFFSET (v) = REGISTER_RAW_SIZE (regnum) - len;
	}
#endif

      bcopy (virtual_buffer + VALUE_OFFSET (v),
	     VALUE_CONTENTS (v), len);
    }
  
  return v;
}

/* Given a struct symbol for a variable,
   and a stack frame id, 
   return a (pointer to a) struct value containing the variable's address.  */

value
locate_var_value (var, frame)
     register struct symbol *var;
     FRAME frame;
{
  register CORE_ADDR addr = 0;
  int val = SYMBOL_VALUE (var);
  struct frame_info *fi;
  struct type *type = SYMBOL_TYPE (var);
  struct type *result_type;

  if (frame == 0) frame = selected_frame;

  switch (SYMBOL_CLASS (var))
    {
    case LOC_CONST:
    case LOC_CONST_BYTES:
      error ("Address requested for identifier \"%s\" which is a constant.",
	     SYMBOL_NAME (var));

    case LOC_REGISTER:
    case LOC_REGPARM:
      addr = find_saved_register (frame, val);
      if (addr != 0)
	{
	  int len = TYPE_LENGTH (type);
#ifdef BYTES_BIG_ENDIAN
	  if (len < REGISTER_RAW_SIZE (val))
	    /* Big-endian, and we want less than full size.  */
	    addr += REGISTER_RAW_SIZE (val) - len;
#endif
	  break;
	}
      error ("Address requested for identifier \"%s\" which is in a register.",
	     SYMBOL_NAME (var));

    case LOC_STATIC:
    case LOC_LABEL:
      addr = val;
      break;

    case LOC_ARG:
      fi = get_frame_info (frame);
      addr = val + FRAME_ARGS_ADDRESS (fi);
      break;

    case LOC_REF_ARG:
      fi = get_frame_info (frame);
      addr = val + FRAME_ARGS_ADDRESS (fi);
      addr = read_memory_integer (addr, sizeof (CORE_ADDR));
      break;

    case LOC_LOCAL:
      fi = get_frame_info (frame);
      addr = val + FRAME_LOCALS_ADDRESS (fi);
      break;

    case LOC_TYPEDEF:
      error ("Address requested for identifier \"%s\" which is a typedef.",
	     SYMBOL_NAME (var));

    case LOC_BLOCK:
      addr = BLOCK_START (SYMBOL_BLOCK_VALUE (var));
      break;
    }

  /* Address of an array is of the type of address of it's elements.  */
  result_type =
    lookup_pointer_type (TYPE_CODE (type) == TYPE_CODE_ARRAY ?
			 TYPE_TARGET_TYPE (type) : type);

  return value_cast (result_type,
		     value_from_long (builtin_type_long, (LONGEST) addr));
}

