/* Find a variable's value in memory, for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "frame.h"
#include "value.h"
#include "gdbcore.h"
#include "inferior.h"
#include "target.h"

/* Basic byte-swapping routines.  GDB has needed these for a long time...
   All extract a target-format integer at ADDR which is LEN bytes long.  */

#if TARGET_CHAR_BIT != 8 || HOST_CHAR_BIT != 8
  /* 8 bit characters are a pretty safe assumption these days, so we
     assume it throughout all these swapping routines.  If we had to deal with
     9 bit characters, we would need to make len be in bits and would have
     to re-write these routines...  */
  you lose
#endif

LONGEST
extract_signed_integer (addr, len)
     PTR addr;
     int len;
{
  LONGEST retval;
  unsigned char *p;
  unsigned char *startaddr = (unsigned char *)addr;
  unsigned char *endaddr = startaddr + len;

  if (len > sizeof (LONGEST))
    error ("\
That operation is not available on integers of more than %d bytes.",
	   sizeof (LONGEST));

  /* Start at the most significant end of the integer, and work towards
     the least significant.  */
#if TARGET_BYTE_ORDER == BIG_ENDIAN
  p = startaddr;
#else
  p = endaddr - 1;
#endif
  /* Do the sign extension once at the start.  */
  retval = ((LONGEST)*p ^ 0x80) - 0x80;
#if TARGET_BYTE_ORDER == BIG_ENDIAN
  for (++p; p < endaddr; ++p)
#else
  for (--p; p >= startaddr; --p)
#endif
    {
      retval = (retval << 8) | *p;
    }
  return retval;
}

unsigned LONGEST
extract_unsigned_integer (addr, len)
     PTR addr;
     int len;
{
  unsigned LONGEST retval;
  unsigned char *p;
  unsigned char *startaddr = (unsigned char *)addr;
  unsigned char *endaddr = startaddr + len;

  if (len > sizeof (unsigned LONGEST))
    error ("\
That operation is not available on integers of more than %d bytes.",
	   sizeof (unsigned LONGEST));

  /* Start at the most significant end of the integer, and work towards
     the least significant.  */
  retval = 0;
#if TARGET_BYTE_ORDER == BIG_ENDIAN
  for (p = startaddr; p < endaddr; ++p)
#else
  for (p = endaddr - 1; p >= startaddr; --p)
#endif
    {
      retval = (retval << 8) | *p;
    }
  return retval;
}

CORE_ADDR
extract_address (addr, len)
     PTR addr;
     int len;
{
  /* Assume a CORE_ADDR can fit in a LONGEST (for now).  Not sure
     whether we want this to be true eventually.  */
  return extract_unsigned_integer (addr, len);
}

void
store_signed_integer (addr, len, val)
     PTR addr;
     int len;
     LONGEST val;
{
  unsigned char *p;
  unsigned char *startaddr = (unsigned char *)addr;
  unsigned char *endaddr = startaddr + len;

  /* Start at the least significant end of the integer, and work towards
     the most significant.  */
#if TARGET_BYTE_ORDER == BIG_ENDIAN
  for (p = endaddr - 1; p >= startaddr; --p)
#else
  for (p = startaddr; p < endaddr; ++p)
#endif
    {
      *p = val & 0xff;
      val >>= 8;
    }
}

void
store_unsigned_integer (addr, len, val)
     PTR addr;
     int len;
     unsigned LONGEST val;
{
  unsigned char *p;
  unsigned char *startaddr = (unsigned char *)addr;
  unsigned char *endaddr = startaddr + len;

  /* Start at the least significant end of the integer, and work towards
     the most significant.  */
#if TARGET_BYTE_ORDER == BIG_ENDIAN
  for (p = endaddr - 1; p >= startaddr; --p)
#else
  for (p = startaddr; p < endaddr; ++p)
#endif
    {
      *p = val & 0xff;
      val >>= 8;
    }
}

void
store_address (addr, len, val)
     PTR addr;
     int len;
     CORE_ADDR val;
{
  /* Assume a CORE_ADDR can fit in a LONGEST (for now).  Not sure
     whether we want this to be true eventually.  */
  store_unsigned_integer (addr, len, (LONGEST)val);
}

#if !defined (GET_SAVED_REGISTER)

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

  if (frame == 0)		/* No regs saved if want current frame */
    return 0;

#ifdef HAVE_REGISTER_WINDOWS
  /* We assume that a register in a register window will only be saved
     in one place (since the name changes and/or disappears as you go
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
      return saved_regs.regs[regnum];	/* ... which might be zero */
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

/* Find register number REGNUM relative to FRAME and put its (raw,
   target format) contents in *RAW_BUFFER.  Set *OPTIMIZED if the
   variable was optimized out (and thus can't be fetched).  Set *LVAL
   to lval_memory, lval_register, or not_lval, depending on whether
   the value was fetched from memory, from a register, or in a strange
   and non-modifiable way (e.g. a frame pointer which was calculated
   rather than fetched).  Set *ADDRP to the address, either in memory
   on as a REGISTER_BYTE offset into the registers array.

   Note that this implementation never sets *LVAL to not_lval.  But
   it can be replaced by defining GET_SAVED_REGISTER and supplying
   your own.

   The argument RAW_BUFFER must point to aligned memory.  */

void
get_saved_register (raw_buffer, optimized, addrp, frame, regnum, lval)
     char *raw_buffer;
     int *optimized;
     CORE_ADDR *addrp;
     FRAME frame;
     int regnum;
     enum lval_type *lval;
{
  CORE_ADDR addr;
  /* Normal systems don't optimize out things with register numbers.  */
  if (optimized != NULL)
    *optimized = 0;
  addr = find_saved_register (frame, regnum);
  if (addr != 0)
    {
      if (lval != NULL)
	*lval = lval_memory;
      if (regnum == SP_REGNUM)
	{
	  if (raw_buffer != NULL)
	    {
	      /* Put it back in target format.  */
	      store_address (raw_buffer, REGISTER_RAW_SIZE (regnum), addr);
	    }
	  if (addrp != NULL)
	    *addrp = 0;
	  return;
	}
      if (raw_buffer != NULL)
	read_memory (addr, raw_buffer, REGISTER_RAW_SIZE (regnum));
    }
  else
    {
      if (lval != NULL)
	*lval = lval_register;
      addr = REGISTER_BYTE (regnum);
      if (raw_buffer != NULL)
	read_register_gen (regnum, raw_buffer);
    }
  if (addrp != NULL)
    *addrp = addr;
}
#endif /* GET_SAVED_REGISTER.  */

/* Copy the bytes of register REGNUM, relative to the current stack frame,
   into our memory at MYADDR, in target byte order.
   The number of bytes copied is REGISTER_RAW_SIZE (REGNUM).

   Returns 1 if could not be read, 0 if could.  */

int
read_relative_register_raw_bytes (regnum, myaddr)
     int regnum;
     char *myaddr;
{
  int optim;
  if (regnum == FP_REGNUM && selected_frame)
    {
      /* Put it back in target format.  */
      store_address (myaddr, REGISTER_RAW_SIZE(FP_REGNUM),
		     FRAME_FP(selected_frame));
      return 0;
    }

  get_saved_register (myaddr, &optim, (CORE_ADDR *) NULL, selected_frame,
                      regnum, (enum lval_type *)NULL);
  return optim;
}

/* Return a `value' with the contents of register REGNUM
   in its virtual format, with the type specified by
   REGISTER_VIRTUAL_TYPE.  */

value
value_of_register (regnum)
     int regnum;
{
  CORE_ADDR addr;
  int optim;
  register value val;
  char raw_buffer[MAX_REGISTER_RAW_SIZE];
  char virtual_buffer[MAX_REGISTER_VIRTUAL_SIZE];
  enum lval_type lval;

  get_saved_register (raw_buffer, &optim, &addr,
		      selected_frame, regnum, &lval);

  REGISTER_CONVERT_TO_VIRTUAL (regnum, raw_buffer, virtual_buffer);
  val = allocate_value (REGISTER_VIRTUAL_TYPE (regnum));
  memcpy (VALUE_CONTENTS_RAW (val), virtual_buffer,
	  REGISTER_VIRTUAL_SIZE (regnum));
  VALUE_LVAL (val) = lval;
  VALUE_ADDRESS (val) = addr;
  VALUE_REGNO (val) = regnum;
  VALUE_OPTIMIZED_OUT (val) = optim;
  return val;
}

/* Low level examining and depositing of registers.

   The caller is responsible for making
   sure that the inferior is stopped before calling the fetching routines,
   or it will get garbage.  (a change from GDB version 3, in which
   the caller got the value from the last stop).  */

/* Contents of the registers in target byte order.
   We allocate some extra slop since we do a lot of memcpy's around `registers',
   and failing-soft is better than failing hard.  */
char registers[REGISTER_BYTES + /* SLOP */ 256];

/* Nonzero if that register has been fetched.  */
char register_valid[NUM_REGS];

/* Indicate that registers may have changed, so invalidate the cache.  */
void
registers_changed ()
{
  int i;
  for (i = 0; i < NUM_REGS; i++)
    register_valid[i] = 0;
}

/* Indicate that all registers have been fetched, so mark them all valid.  */
void
registers_fetched ()
{
  int i;
  for (i = 0; i < NUM_REGS; i++)
    register_valid[i] = 1;
}

/* Copy LEN bytes of consecutive data from registers
   starting with the REGBYTE'th byte of register data
   into memory at MYADDR.  */

void
read_register_bytes (regbyte, myaddr, len)
     int regbyte;
     char *myaddr;
     int len;
{
  /* Fetch all registers.  */
  int i;
  for (i = 0; i < NUM_REGS; i++)
    if (!register_valid[i])
      {
	target_fetch_registers (-1);
	break;
      }
  if (myaddr != NULL)
    memcpy (myaddr, &registers[regbyte], len);
}

/* Read register REGNO into memory at MYADDR, which must be large enough
   for REGISTER_RAW_BYTES (REGNO).  Target byte-order.
   If the register is known to be the size of a CORE_ADDR or smaller,
   read_register can be used instead.  */
void
read_register_gen (regno, myaddr)
     int regno;
     char *myaddr;
{
  if (!register_valid[regno])
    target_fetch_registers (regno);
  memcpy (myaddr, &registers[REGISTER_BYTE (regno)],
	  REGISTER_RAW_SIZE (regno));
}

/* Copy LEN bytes of consecutive data from memory at MYADDR
   into registers starting with the REGBYTE'th byte of register data.  */

void
write_register_bytes (regbyte, myaddr, len)
     int regbyte;
     char *myaddr;
     int len;
{
  /* Make sure the entire registers array is valid.  */
  read_register_bytes (0, (char *)NULL, REGISTER_BYTES);
  memcpy (&registers[regbyte], myaddr, len);
  target_store_registers (-1);
}

/* Return the raw contents of register REGNO, regarding it as an integer.  */
/* This probably should be returning LONGEST rather than CORE_ADDR.  */

CORE_ADDR
read_register (regno)
     int regno;
{
  if (!register_valid[regno])
    target_fetch_registers (regno);

  return extract_address (&registers[REGISTER_BYTE (regno)],
			  REGISTER_RAW_SIZE(regno));
}

/* Registers we shouldn't try to store.  */
#if !defined (CANNOT_STORE_REGISTER)
#define CANNOT_STORE_REGISTER(regno) 0
#endif

/* Store VALUE, into the raw contents of register number REGNO.  */
/* FIXME: The val arg should probably be a LONGEST.  */

void
write_register (regno, val)
     int regno;
     LONGEST val;
{
  PTR buf;
  int size;

  /* On the sparc, writing %g0 is a no-op, so we don't even want to change
     the registers array if something writes to this register.  */
  if (CANNOT_STORE_REGISTER (regno))
    return;

  size = REGISTER_RAW_SIZE(regno);
  buf = alloca (size);
  store_signed_integer (buf, size, (LONGEST) val);

  /* If we have a valid copy of the register, and new value == old value,
     then don't bother doing the actual store. */

  if (register_valid [regno]) 
    {
      if (memcmp (&registers[REGISTER_BYTE (regno)], buf, size) == 0)
	return;
    }
  
  target_prepare_to_store ();

  memcpy (&registers[REGISTER_BYTE (regno)], buf, size);

  register_valid [regno] = 1;

  target_store_registers (regno);
}

/* Record that register REGNO contains VAL.
   This is used when the value is obtained from the inferior or core dump,
   so there is no need to store the value there.  */

void
supply_register (regno, val)
     int regno;
     char *val;
{
  register_valid[regno] = 1;
  memcpy (&registers[REGISTER_BYTE (regno)], val, REGISTER_RAW_SIZE (regno));

  /* On some architectures, e.g. HPPA, there are a few stray bits in some
     registers, that the rest of the code would like to ignore.  */
#ifdef CLEAN_UP_REGISTER_VALUE
  CLEAN_UP_REGISTER_VALUE(regno, &registers[REGISTER_BYTE(regno)]);
#endif
}

/* Will calling read_var_value or locate_var_value on SYM end
   up caring what frame it is being evaluated relative to?  SYM must
   be non-NULL.  */
int
symbol_read_needs_frame (sym)
     struct symbol *sym;
{
  switch (SYMBOL_CLASS (sym))
    {
      /* All cases listed explicitly so that gcc -Wall will detect it if
	 we failed to consider one.  */
    case LOC_REGISTER:
    case LOC_ARG:
    case LOC_REF_ARG:
    case LOC_REGPARM:
    case LOC_REGPARM_ADDR:
    case LOC_LOCAL:
    case LOC_LOCAL_ARG:
    case LOC_BASEREG:
    case LOC_BASEREG_ARG:
      return 1;

    case LOC_UNDEF:
    case LOC_CONST:
    case LOC_STATIC:
    case LOC_TYPEDEF:

    case LOC_LABEL:
      /* Getting the address of a label can be done independently of the block,
	 even if some *uses* of that address wouldn't work so well without
	 the right frame.  */

    case LOC_BLOCK:
    case LOC_CONST_BYTES:
    case LOC_OPTIMIZED_OUT:
      return 0;
    }
}

/* Given a struct symbol for a variable,
   and a stack frame id, read the value of the variable
   and return a (pointer to a) struct value containing the value. 
   If the variable cannot be found, return a zero pointer.
   If FRAME is NULL, use the selected_frame.  */

value
read_var_value (var, frame)
     register struct symbol *var;
     FRAME frame;
{
  register value v;
  struct frame_info *fi;
  struct type *type = SYMBOL_TYPE (var);
  CORE_ADDR addr;
  register int len;

  v = allocate_value (type);
  VALUE_LVAL (v) = lval_memory;	/* The most likely possibility.  */
  len = TYPE_LENGTH (type);

  if (frame == 0) frame = selected_frame;

  switch (SYMBOL_CLASS (var))
    {
    case LOC_CONST:
      /* Put the constant back in target format.  */
      store_signed_integer (VALUE_CONTENTS_RAW (v), len,
			    (LONGEST) SYMBOL_VALUE (var));
      VALUE_LVAL (v) = not_lval;
      return v;

    case LOC_LABEL:
      /* Put the constant back in target format.  */
      store_address (VALUE_CONTENTS_RAW (v), len, SYMBOL_VALUE_ADDRESS (var));
      VALUE_LVAL (v) = not_lval;
      return v;

    case LOC_CONST_BYTES:
      {
	char *bytes_addr;
	bytes_addr = SYMBOL_VALUE_BYTES (var);
	memcpy (VALUE_CONTENTS_RAW (v), bytes_addr, len);
	VALUE_LVAL (v) = not_lval;
	return v;
      }

    case LOC_STATIC:
      addr = SYMBOL_VALUE_ADDRESS (var);
      break;

    case LOC_ARG:
      fi = get_frame_info (frame);
      if (fi == NULL)
	return 0;
      addr = FRAME_ARGS_ADDRESS (fi);
      if (!addr)
	{
	  return 0;
	}
      addr += SYMBOL_VALUE (var);
      break;

    case LOC_REF_ARG:
      fi = get_frame_info (frame);
      if (fi == NULL)
	return 0;
      addr = FRAME_ARGS_ADDRESS (fi);
      if (!addr)
	{
	  return 0;
	}
      addr += SYMBOL_VALUE (var);
      addr = read_memory_unsigned_integer
	(addr, TARGET_PTR_BIT / TARGET_CHAR_BIT);
      break;

    case LOC_LOCAL:
    case LOC_LOCAL_ARG:
      fi = get_frame_info (frame);
      if (fi == NULL)
	return 0;
      addr = FRAME_LOCALS_ADDRESS (fi);
      addr += SYMBOL_VALUE (var);
      break;

    case LOC_BASEREG:
    case LOC_BASEREG_ARG:
      {
	char buf[MAX_REGISTER_RAW_SIZE];
	get_saved_register (buf, NULL, NULL, frame, SYMBOL_BASEREG (var),
			    NULL);
	addr = extract_address (buf, REGISTER_RAW_SIZE (SYMBOL_BASEREG (var)));
	addr += SYMBOL_VALUE (var);
	break;
      }
			    
    case LOC_TYPEDEF:
      error ("Cannot look up value of a typedef");
      break;

    case LOC_BLOCK:
      VALUE_ADDRESS (v) = BLOCK_START (SYMBOL_BLOCK_VALUE (var));
      return v;

    case LOC_REGISTER:
    case LOC_REGPARM:
    case LOC_REGPARM_ADDR:
      {
	struct block *b;

	if (frame == NULL)
	  return 0;
	b = get_frame_block (frame);
	
	v = value_from_register (type, SYMBOL_VALUE (var), frame);

	if (SYMBOL_CLASS (var) == LOC_REGPARM_ADDR)
	  {
	    addr = *(CORE_ADDR *)VALUE_CONTENTS (v);
	    VALUE_LVAL (v) = lval_memory;
	  }
	else
	  return v;
      }
      break;

    case LOC_OPTIMIZED_OUT:
      VALUE_LVAL (v) = not_lval;
      VALUE_OPTIMIZED_OUT (v) = 1;
      return v;

    default:
      error ("Cannot look up value of a botched symbol.");
      break;
    }

  VALUE_ADDRESS (v) = addr;
  VALUE_LAZY (v) = 1;
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
  int optim;
  value v = allocate_value (type);
  int len = TYPE_LENGTH (type);
  char *value_bytes = 0;
  int value_bytes_copied = 0;
  int num_storage_locs;
  enum lval_type lval;

  VALUE_REGNO (v) = regnum;

  num_storage_locs = (len > REGISTER_VIRTUAL_SIZE (regnum) ?
		      ((len - 1) / REGISTER_RAW_SIZE (regnum)) + 1 :
		      1);

  if (num_storage_locs > 1
#ifdef GDB_TARGET_IS_H8500
      || TYPE_CODE (type) == TYPE_CODE_PTR
#endif
      )
    {
      /* Value spread across multiple storage locations.  */
      
      int local_regnum;
      int mem_stor = 0, reg_stor = 0;
      int mem_tracking = 1;
      CORE_ADDR last_addr = 0;
      CORE_ADDR first_addr = 0;

      value_bytes = (char *) alloca (len + MAX_REGISTER_RAW_SIZE);

      /* Copy all of the data out, whereever it may be.  */

#ifdef GDB_TARGET_IS_H8500
/* This piece of hideosity is required because the H8500 treats registers
   differently depending upon whether they are used as pointers or not.  As a
   pointer, a register needs to have a page register tacked onto the front.
   An alternate way to do this would be to have gcc output different register
   numbers for the pointer & non-pointer form of the register.  But, it
   doesn't, so we're stuck with this.  */

      if (TYPE_CODE (type) == TYPE_CODE_PTR
	  && len > 2)
	{
	  int page_regnum;

	  switch (regnum)
	    {
	    case R0_REGNUM: case R1_REGNUM: case R2_REGNUM: case R3_REGNUM:
	      page_regnum = SEG_D_REGNUM;
	      break;
	    case R4_REGNUM: case R5_REGNUM:
	      page_regnum = SEG_E_REGNUM;
	      break;
	    case R6_REGNUM: case R7_REGNUM:
	      page_regnum = SEG_T_REGNUM;
	      break;
	    }

	  value_bytes[0] = 0;
	  get_saved_register (value_bytes + 1,
			      &optim,
			      &addr,
			      frame,
			      page_regnum,
			      &lval);

	  if (lval == lval_register)
	    reg_stor++;
	  else
	    mem_stor++;
	  first_addr = addr;
	  last_addr = addr;

	  get_saved_register (value_bytes + 2,
			      &optim,
			      &addr,
			      frame,
			      regnum,
			      &lval);

	  if (lval == lval_register)
	    reg_stor++;
	  else
	    {
	      mem_stor++;
	      mem_tracking = mem_tracking && (addr == last_addr);
	    }
	  last_addr = addr;
	}
      else
#endif				/* GDB_TARGET_IS_H8500 */
	for (local_regnum = regnum;
	     value_bytes_copied < len;
	     (value_bytes_copied += REGISTER_RAW_SIZE (local_regnum),
	      ++local_regnum))
	  {
	    get_saved_register (value_bytes + value_bytes_copied,
				&optim,
				&addr,
				frame,
				local_regnum,
				&lval);

	    if (regnum == local_regnum)
	      first_addr = addr;
	    if (lval == lval_register)
	      reg_stor++;
	    else
	      {
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
	  VALUE_ADDRESS (v) = first_addr;
	}
      else if (reg_stor)
	{
	  VALUE_LVAL (v) = lval_register;
	  VALUE_ADDRESS (v) = first_addr;
	}
      else
	fatal ("value_from_register: Value not stored anywhere!");

      VALUE_OPTIMIZED_OUT (v) = optim;

      /* Any structure stored in more than one register will always be
	 an integral number of registers.  Otherwise, you'd need to do
	 some fiddling with the last register copied here for little
	 endian machines.  */

      /* Copy into the contents section of the value.  */
      memcpy (VALUE_CONTENTS_RAW (v), value_bytes, len);

      /* Finally do any conversion necessary when extracting this
         type from more than one register.  */
#ifdef REGISTER_CONVERT_TO_TYPE
      REGISTER_CONVERT_TO_TYPE(regnum, type, VALUE_CONTENTS_RAW(v));
#endif
      return v;
    }

  /* Data is completely contained within a single register.  Locate the
     register's contents in a real register or in core;
     read the data in raw format.  */

  get_saved_register (raw_buffer, &optim, &addr, frame, regnum, &lval);
  VALUE_OPTIMIZED_OUT (v) = optim;
  VALUE_LVAL (v) = lval;
  VALUE_ADDRESS (v) = addr;
  
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
	  /* FIXME: This value will be not_lval, which means we can't assign
	     to it.  Probably the right fix is to do the cast on a temporary
	     value, and just copy the VALUE_CONTENTS over.  */
	  v = allocate_value (REGISTER_VIRTUAL_TYPE (regnum));
	  memcpy (VALUE_CONTENTS_RAW (v), virtual_buffer,
		  REGISTER_VIRTUAL_SIZE (regnum));
	  v = value_cast (type, v);
	}
      else
	memcpy (VALUE_CONTENTS_RAW (v), virtual_buffer, len);
    }
  else
    {
      /* Raw and virtual formats are the same for this register.  */

#if TARGET_BYTE_ORDER == BIG_ENDIAN
      if (len < REGISTER_RAW_SIZE (regnum))
	{
  	  /* Big-endian, and we want less than full size.  */
	  VALUE_OFFSET (v) = REGISTER_RAW_SIZE (regnum) - len;
	}
#endif

      memcpy (VALUE_CONTENTS_RAW (v), virtual_buffer + VALUE_OFFSET (v), len);
    }
  
  return v;
}

/* Given a struct symbol for a variable or function,
   and a stack frame id, 
   return a (pointer to a) struct value containing the properly typed
   address.  */

value
locate_var_value (var, frame)
     register struct symbol *var;
     FRAME frame;
{
  CORE_ADDR addr = 0;
  struct type *type = SYMBOL_TYPE (var);
  value lazy_value;

  /* Evaluate it first; if the result is a memory address, we're fine.
     Lazy evaluation pays off here. */

  lazy_value = read_var_value (var, frame);
  if (lazy_value == 0)
    error ("Address of \"%s\" is unknown.", SYMBOL_SOURCE_NAME (var));

  if (VALUE_LAZY (lazy_value)
      || TYPE_CODE (type) == TYPE_CODE_FUNC)
    {
      addr = VALUE_ADDRESS (lazy_value);
      return value_from_longest (lookup_pointer_type (type), (LONGEST) addr);
    }

  /* Not a memory address; check what the problem was.  */
  switch (VALUE_LVAL (lazy_value)) 
    {
    case lval_register:
    case lval_reg_frame_relative:
      error ("Address requested for identifier \"%s\" which is in a register.",
	     SYMBOL_SOURCE_NAME (var));
      break;

    default:
      error ("Can't take address of \"%s\" which isn't an lvalue.",
	     SYMBOL_SOURCE_NAME (var));
      break;
    }
  return 0;  /* For lint -- never reached */
}
