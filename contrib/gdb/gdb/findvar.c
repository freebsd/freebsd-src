/* Find a variable's value in memory, for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1994, 1995, 1996 Free Software Foundation, Inc.

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

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "frame.h"
#include "value.h"
#include "gdbcore.h"
#include "inferior.h"
#include "target.h"
#include "gdb_string.h"

/* Registers we shouldn't try to store.  */
#if !defined (CANNOT_STORE_REGISTER)
#define CANNOT_STORE_REGISTER(regno) 0
#endif

static void write_register_pid PARAMS ((int regno, LONGEST val, int pid));

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

  if (len > (int) sizeof (LONGEST))
    error ("\
That operation is not available on integers of more than %d bytes.",
	   sizeof (LONGEST));

  /* Start at the most significant end of the integer, and work towards
     the least significant.  */
  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
    {
      p = startaddr;
      /* Do the sign extension once at the start.  */
      retval = ((LONGEST)*p ^ 0x80) - 0x80;
      for (++p; p < endaddr; ++p)
	retval = (retval << 8) | *p;
    }
  else
    {
      p = endaddr - 1;
      /* Do the sign extension once at the start.  */
      retval = ((LONGEST)*p ^ 0x80) - 0x80;
      for (--p; p >= startaddr; --p)
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

  if (len > (int) sizeof (unsigned LONGEST))
    error ("\
That operation is not available on integers of more than %d bytes.",
	   sizeof (unsigned LONGEST));

  /* Start at the most significant end of the integer, and work towards
     the least significant.  */
  retval = 0;
  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
    {
      for (p = startaddr; p < endaddr; ++p)
	retval = (retval << 8) | *p;
    }
  else
    {
      for (p = endaddr - 1; p >= startaddr; --p)
	retval = (retval << 8) | *p;
    }
  return retval;
}

/* Sometimes a long long unsigned integer can be extracted as a
   LONGEST value.  This is done so that we can print these values
   better.  If this integer can be converted to a LONGEST, this
   function returns 1 and sets *PVAL.  Otherwise it returns 0.  */

int
extract_long_unsigned_integer (addr, orig_len, pval)
     PTR addr;
     int orig_len;
     LONGEST *pval;
{
  char *p, *first_addr;
  int len;

  len = orig_len;
  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
    {
      for (p = (char *) addr;
	   len > (int) sizeof (LONGEST) && p < (char *) addr + orig_len;
	   p++)
	{
	  if (*p == 0)
	    len--;
	  else
	    break;
	}
      first_addr = p;
    }
  else
    {
      first_addr = (char *) addr;
      for (p = (char *) addr + orig_len - 1;
	   len > (int) sizeof (LONGEST) && p >= (char *) addr;
	   p--)
	{
	  if (*p == 0)
	    len--;
	  else
	    break;
	}
    }

  if (len <= (int) sizeof (LONGEST))
    {
      *pval = (LONGEST) extract_unsigned_integer (first_addr,
						  sizeof (LONGEST));
      return 1;
    }

  return 0;
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
  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
    {
      for (p = endaddr - 1; p >= startaddr; --p)
	{
	  *p = val & 0xff;
	  val >>= 8;
	}
    }
  else
    {
      for (p = startaddr; p < endaddr; ++p)
	{
	  *p = val & 0xff;
	  val >>= 8;
	}
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
  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
    {
      for (p = endaddr - 1; p >= startaddr; --p)
	{
	  *p = val & 0xff;
	  val >>= 8;
	}
    }
  else
    {
      for (p = startaddr; p < endaddr; ++p)
	{
	  *p = val & 0xff;
	  val >>= 8;
	}
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

/* Swap LEN bytes at BUFFER between target and host byte-order.  */
#define SWAP_FLOATING(buffer,len) \
  do                                                                    \
    {                                                                   \
      if (TARGET_BYTE_ORDER != HOST_BYTE_ORDER)                         \
        {                                                               \
          char tmp;                                                     \
          char *p = (char *)(buffer);                                   \
          char *q = ((char *)(buffer)) + len - 1;                       \
          for (; p < q; p++, q--)                                       \
            {                                                           \
              tmp = *q;                                                 \
              *q = *p;                                                  \
              *p = tmp;                                                 \
            }                                                           \
        }                                                               \
    }                                                                   \
  while (0)

/* There are various problems with the extract_floating and store_floating
   routines.

   1.  These routines only handle byte-swapping, not conversion of
   formats.  So if host is IEEE floating and target is VAX floating,
   or vice-versa, it loses.  This means that we can't (yet) use these
   routines for extendeds.  Extendeds are handled by
   REGISTER_CONVERTIBLE.  What we want is to use floatformat.h, but that
   doesn't yet handle VAX floating at all.

   2.  We can't deal with it if there is more than one floating point
   format in use.  This has to be fixed at the unpack_double level.

   3.  We probably should have a LONGEST_DOUBLE or DOUBLEST or whatever
   we want to call it which is long double where available.  */

DOUBLEST
extract_floating (addr, len)
     PTR addr;
     int len;
{
  if (len == sizeof (float))
    {
      float retval;
      memcpy (&retval, addr, sizeof (retval));
      SWAP_FLOATING (&retval, sizeof (retval));
      return retval;
    }
  else if (len == sizeof (double))
    {
      double retval;
      memcpy (&retval, addr, sizeof (retval));
      SWAP_FLOATING (&retval, sizeof (retval));
      return retval;
    }
  else if (len == sizeof (DOUBLEST))
    {
      DOUBLEST retval;
      memcpy (&retval, addr, sizeof (retval));
      SWAP_FLOATING (&retval, sizeof (retval));
      return retval;
    }
  else
    {
      error ("Can't deal with a floating point number of %d bytes.", len);
    }
}

void
store_floating (addr, len, val)
     PTR addr;
     int len;
     DOUBLEST val;
{
  if (len == sizeof (float))
    {
      float floatval = val;
      SWAP_FLOATING (&floatval, sizeof (floatval));
      memcpy (addr, &floatval, sizeof (floatval));
    }
  else if (len == sizeof (double))
    {
      double doubleval = val;

      SWAP_FLOATING (&doubleval, sizeof (doubleval));
      memcpy (addr, &doubleval, sizeof (doubleval));
    }
  else if (len == sizeof (DOUBLEST))
    {
      SWAP_FLOATING (&val, sizeof (val));
      memcpy (addr, &val, sizeof (val));
    }
  else
    {
      error ("Can't deal with a floating point number of %d bytes.", len);
    }
}

#if !defined (GET_SAVED_REGISTER)

/* Return the address in which frame FRAME's value of register REGNUM
   has been saved in memory.  Or return zero if it has not been saved.
   If REGNUM specifies the SP, the value we return is actually
   the SP value, not an address where it was saved.  */

CORE_ADDR
find_saved_register (frame, regnum)
     struct frame_info *frame;
     int regnum;
{
  struct frame_saved_regs saved_regs;

  register struct frame_info *frame1 = NULL;
  register CORE_ADDR addr = 0;

  if (frame == NULL)		/* No regs saved if want current frame */
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
      if (!frame1) return 0;	/* Registers of this frame are active.  */
      
      /* Get the SP from the next frame in; it will be this
	 current frame.  */
      if (regnum != SP_REGNUM)
	frame1 = frame;	
	  
      get_frame_saved_regs (frame1, &saved_regs);
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
      get_frame_saved_regs (frame1, &saved_regs);
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
     struct frame_info *frame;
     int regnum;
     enum lval_type *lval;
{
  CORE_ADDR addr;

  if (!target_has_registers)
    error ("No registers.");

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

value_ptr
value_of_register (regnum)
     int regnum;
{
  CORE_ADDR addr;
  int optim;
  register value_ptr reg_val;
  char raw_buffer[MAX_REGISTER_RAW_SIZE];
  enum lval_type lval;

  get_saved_register (raw_buffer, &optim, &addr,
		      selected_frame, regnum, &lval);

  reg_val = allocate_value (REGISTER_VIRTUAL_TYPE (regnum));

  /* Convert raw data to virtual format if necessary.  */

#ifdef REGISTER_CONVERTIBLE
  if (REGISTER_CONVERTIBLE (regnum))
    {
      REGISTER_CONVERT_TO_VIRTUAL (regnum, REGISTER_VIRTUAL_TYPE (regnum),
				   raw_buffer, VALUE_CONTENTS_RAW (reg_val));
    }
  else
#endif
    memcpy (VALUE_CONTENTS_RAW (reg_val), raw_buffer,
	    REGISTER_RAW_SIZE (regnum));
  VALUE_LVAL (reg_val) = lval;
  VALUE_ADDRESS (reg_val) = addr;
  VALUE_REGNO (reg_val) = regnum;
  VALUE_OPTIMIZED_OUT (reg_val) = optim;
  return reg_val;
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

/* The thread/process associated with the current set of registers.  For now,
   -1 is special, and means `no current process'.  */
int registers_pid = -1;

/* Indicate that registers may have changed, so invalidate the cache.  */

void
registers_changed ()
{
  int i;
  int numregs = ARCH_NUM_REGS;

  registers_pid = -1;

  for (i = 0; i < numregs; i++)
    register_valid[i] = 0;

  if (registers_changed_hook)
    registers_changed_hook ();
}

/* Indicate that all registers have been fetched, so mark them all valid.  */
void
registers_fetched ()
{
  int i;
  int numregs = ARCH_NUM_REGS;
  for (i = 0; i < numregs; i++)
    register_valid[i] = 1;
}

/* read_register_bytes and write_register_bytes are generally a *BAD* idea.
   They are inefficient because they need to check for partial updates, which
   can only be done by scanning through all of the registers and seeing if the
   bytes that are being read/written fall inside of an invalid register.  [The
    main reason this is necessary is that register sizes can vary, so a simple
    index won't suffice.]  It is far better to call read_register_gen if you
   want to get at the raw register contents, as it only takes a regno as an
   argument, and therefore can't do a partial register update.  It would also
   be good to have a write_register_gen for similar reasons.

   Prior to the recent fixes to check for partial updates, both read and
   write_register_bytes always checked to see if any registers were stale, and
   then called target_fetch_registers (-1) to update the whole set.  This
   caused really slowed things down for remote targets.  */

/* Copy INLEN bytes of consecutive data from registers
   starting with the INREGBYTE'th byte of register data
   into memory at MYADDR.  */

void
read_register_bytes (inregbyte, myaddr, inlen)
     int inregbyte;
     char *myaddr;
     int inlen;
{
  int inregend = inregbyte + inlen;
  int regno;

  if (registers_pid != inferior_pid)
    {
      registers_changed ();
      registers_pid = inferior_pid;
    }

  /* See if we are trying to read bytes from out-of-date registers.  If so,
     update just those registers.  */

  for (regno = 0; regno < NUM_REGS; regno++)
    {
      int regstart, regend;
      int startin, endin;

      if (register_valid[regno])
	continue;

      regstart = REGISTER_BYTE (regno);
      regend = regstart + REGISTER_RAW_SIZE (regno);

      startin = regstart >= inregbyte && regstart < inregend;
      endin = regend > inregbyte && regend <= inregend;

      if (!startin && !endin)
	continue;

      /* We've found an invalid register where at least one byte will be read.
	 Update it from the target.  */

      target_fetch_registers (regno);

      if (!register_valid[regno])
	error ("read_register_bytes:  Couldn't update register %d.", regno);
    }

  if (myaddr != NULL)
    memcpy (myaddr, &registers[inregbyte], inlen);
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
  if (registers_pid != inferior_pid)
    {
      registers_changed ();
      registers_pid = inferior_pid;
    }

  if (!register_valid[regno])
    target_fetch_registers (regno);
  memcpy (myaddr, &registers[REGISTER_BYTE (regno)],
	  REGISTER_RAW_SIZE (regno));
}

/* Write register REGNO at MYADDR to the target.  MYADDR points at
   REGISTER_RAW_BYTES(REGNO), which must be in target byte-order.  */

void
write_register_gen (regno, myaddr)
     int regno;
     char *myaddr;
{
  int size;

  /* On the sparc, writing %g0 is a no-op, so we don't even want to change
     the registers array if something writes to this register.  */
  if (CANNOT_STORE_REGISTER (regno))
    return;

  if (registers_pid != inferior_pid)
    {
      registers_changed ();
      registers_pid = inferior_pid;
    }

  size = REGISTER_RAW_SIZE(regno);

  /* If we have a valid copy of the register, and new value == old value,
     then don't bother doing the actual store. */

  if (register_valid [regno]
      && memcmp (&registers[REGISTER_BYTE (regno)], myaddr, size) == 0)
    return;
  
  target_prepare_to_store ();

  memcpy (&registers[REGISTER_BYTE (regno)], myaddr, size);

  register_valid [regno] = 1;

  target_store_registers (regno);
}

/* Copy INLEN bytes of consecutive data from memory at MYADDR
   into registers starting with the MYREGSTART'th byte of register data.  */

void
write_register_bytes (myregstart, myaddr, inlen)
     int myregstart;
     char *myaddr;
     int inlen;
{
  int myregend = myregstart + inlen;
  int regno;

  target_prepare_to_store ();

  /* Scan through the registers updating any that are covered by the range
     myregstart<=>myregend using write_register_gen, which does nice things
     like handling threads, and avoiding updates when the new and old contents
     are the same.  */

  for (regno = 0; regno < NUM_REGS; regno++)
    {
      int regstart, regend;
      int startin, endin;
      char regbuf[MAX_REGISTER_RAW_SIZE];

      regstart = REGISTER_BYTE (regno);
      regend = regstart + REGISTER_RAW_SIZE (regno);

      startin = regstart >= myregstart && regstart < myregend;
      endin = regend > myregstart && regend <= myregend;

      if (!startin && !endin)
	continue;		/* Register is completely out of range */

      if (startin && endin)	/* register is completely in range */
	{
	  write_register_gen (regno, myaddr + (regstart - myregstart));
	  continue;
	}

      /* We may be doing a partial update of an invalid register.  Update it
	 from the target before scribbling on it.  */
      read_register_gen (regno, regbuf);

      if (startin)
	memcpy (registers + regstart,
		myaddr + regstart - myregstart,
		myregend - regstart);
      else			/* endin */
	memcpy (registers + myregstart,
		myaddr,
		regend - myregstart);
      target_store_registers (regno);
    }
}

/* Return the raw contents of register REGNO, regarding it as an integer.  */
/* This probably should be returning LONGEST rather than CORE_ADDR.  */

CORE_ADDR
read_register (regno)
     int regno;
{
  if (registers_pid != inferior_pid)
    {
      registers_changed ();
      registers_pid = inferior_pid;
    }

  if (!register_valid[regno])
    target_fetch_registers (regno);

  return extract_address (&registers[REGISTER_BYTE (regno)],
			  REGISTER_RAW_SIZE(regno));
}

CORE_ADDR
read_register_pid (regno, pid)
     int regno, pid;
{
  int save_pid;
  CORE_ADDR retval;

  if (pid == inferior_pid)
    return read_register (regno);

  save_pid = inferior_pid;

  inferior_pid = pid;

  retval = read_register (regno);

  inferior_pid = save_pid;

  return retval;
}

/* Store VALUE, into the raw contents of register number REGNO.  */

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

  if (registers_pid != inferior_pid)
    {
      registers_changed ();
      registers_pid = inferior_pid;
    }

  size = REGISTER_RAW_SIZE(regno);
  buf = alloca (size);
  store_signed_integer (buf, size, (LONGEST) val);

  /* If we have a valid copy of the register, and new value == old value,
     then don't bother doing the actual store. */

  if (register_valid [regno]
      && memcmp (&registers[REGISTER_BYTE (regno)], buf, size) == 0)
    return;
  
  target_prepare_to_store ();

  memcpy (&registers[REGISTER_BYTE (regno)], buf, size);

  register_valid [regno] = 1;

  target_store_registers (regno);
}

static void
write_register_pid (regno, val, pid)
     int regno;
     LONGEST val;
     int pid;
{
  int save_pid;

  if (pid == inferior_pid)
    {
      write_register (regno, val);
      return;
    }

  save_pid = inferior_pid;

  inferior_pid = pid;

  write_register (regno, val);

  inferior_pid = save_pid;
}

/* Record that register REGNO contains VAL.
   This is used when the value is obtained from the inferior or core dump,
   so there is no need to store the value there.  */

void
supply_register (regno, val)
     int regno;
     char *val;
{
  if (registers_pid != inferior_pid)
    {
      registers_changed ();
      registers_pid = inferior_pid;
    }

  register_valid[regno] = 1;
  memcpy (&registers[REGISTER_BYTE (regno)], val, REGISTER_RAW_SIZE (regno));

  /* On some architectures, e.g. HPPA, there are a few stray bits in some
     registers, that the rest of the code would like to ignore.  */
#ifdef CLEAN_UP_REGISTER_VALUE
  CLEAN_UP_REGISTER_VALUE(regno, &registers[REGISTER_BYTE(regno)]);
#endif
}


/* This routine is getting awfully cluttered with #if's.  It's probably
   time to turn this into READ_PC and define it in the tm.h file.
   Ditto for write_pc.  */

CORE_ADDR
read_pc ()
{
#ifdef TARGET_READ_PC
  return TARGET_READ_PC (inferior_pid);
#else
  return ADDR_BITS_REMOVE ((CORE_ADDR) read_register_pid (PC_REGNUM, inferior_pid));
#endif
}

CORE_ADDR
read_pc_pid (pid)
     int pid;
{
#ifdef TARGET_READ_PC
  return TARGET_READ_PC (pid);
#else
  return ADDR_BITS_REMOVE ((CORE_ADDR) read_register_pid (PC_REGNUM, pid));
#endif
}

void
write_pc (val)
     CORE_ADDR val;
{
#ifdef TARGET_WRITE_PC
  TARGET_WRITE_PC (val, inferior_pid);
#else
  write_register_pid (PC_REGNUM, val, inferior_pid);
#ifdef NPC_REGNUM
  write_register_pid (NPC_REGNUM, val + 4, inferior_pid);
#ifdef NNPC_REGNUM
  write_register_pid (NNPC_REGNUM, val + 8, inferior_pid);
#endif
#endif
#endif
}

void
write_pc_pid (val, pid)
     CORE_ADDR val;
     int pid;
{
#ifdef TARGET_WRITE_PC
  TARGET_WRITE_PC (val, pid);
#else
  write_register_pid (PC_REGNUM, val, pid);
#ifdef NPC_REGNUM
  write_register_pid (NPC_REGNUM, val + 4, pid);
#ifdef NNPC_REGNUM
  write_register_pid (NNPC_REGNUM, val + 8, pid);
#endif
#endif
#endif
}

/* Cope with strage ways of getting to the stack and frame pointers */

CORE_ADDR
read_sp ()
{
#ifdef TARGET_READ_SP
  return TARGET_READ_SP ();
#else
  return read_register (SP_REGNUM);
#endif
}

void
write_sp (val)
     CORE_ADDR val;
{
#ifdef TARGET_WRITE_SP
  TARGET_WRITE_SP (val);
#else
  write_register (SP_REGNUM, val);
#endif
}

CORE_ADDR
read_fp ()
{
#ifdef TARGET_READ_FP
  return TARGET_READ_FP ();
#else
  return read_register (FP_REGNUM);
#endif
}

void
write_fp (val)
     CORE_ADDR val;
{
#ifdef TARGET_WRITE_FP
  TARGET_WRITE_FP (val);
#else
  write_register (FP_REGNUM, val);
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
    case LOC_UNRESOLVED:
    case LOC_OPTIMIZED_OUT:
      return 0;
    }
  return 1;
}

/* Given a struct symbol for a variable,
   and a stack frame id, read the value of the variable
   and return a (pointer to a) struct value containing the value. 
   If the variable cannot be found, return a zero pointer.
   If FRAME is NULL, use the selected_frame.  */

value_ptr
read_var_value (var, frame)
     register struct symbol *var;
     struct frame_info *frame;
{
  register value_ptr v;
  struct type *type = SYMBOL_TYPE (var);
  CORE_ADDR addr;
  register int len;

  v = allocate_value (type);
  VALUE_LVAL (v) = lval_memory;	/* The most likely possibility.  */
  len = TYPE_LENGTH (type);

  if (frame == NULL) frame = selected_frame;

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
      if (frame == NULL)
	return 0;
      addr = FRAME_ARGS_ADDRESS (frame);
      if (!addr)
	return 0;
      addr += SYMBOL_VALUE (var);
      break;

    case LOC_REF_ARG:
      if (frame == NULL)
	return 0;
      addr = FRAME_ARGS_ADDRESS (frame);
      if (!addr)
	return 0;
      addr += SYMBOL_VALUE (var);
      addr = read_memory_unsigned_integer
	(addr, TARGET_PTR_BIT / TARGET_CHAR_BIT);
      break;

    case LOC_LOCAL:
    case LOC_LOCAL_ARG:
      if (frame == NULL)
	return 0;
      addr = FRAME_LOCALS_ADDRESS (frame);
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
	

	if (SYMBOL_CLASS (var) == LOC_REGPARM_ADDR)
	  {
	    addr =
	      value_as_pointer (value_from_register (lookup_pointer_type (type),
						     SYMBOL_VALUE (var),
						     frame));
	    VALUE_LVAL (v) = lval_memory;
	  }
	else
	  return value_from_register (type, SYMBOL_VALUE (var), frame);
      }
      break;

    case LOC_UNRESOLVED:
      {
	struct minimal_symbol *msym;

	msym = lookup_minimal_symbol (SYMBOL_NAME (var), NULL, NULL);
	if (msym == NULL)
	  return 0;
	addr = SYMBOL_VALUE_ADDRESS (msym);
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

value_ptr
value_from_register (type, regnum, frame)
     struct type *type;
     int regnum;
     struct frame_info *frame;
{
  char raw_buffer [MAX_REGISTER_RAW_SIZE];
  CORE_ADDR addr;
  int optim;
  value_ptr v = allocate_value (type);
  char *value_bytes = 0;
  int value_bytes_copied = 0;
  int num_storage_locs;
  enum lval_type lval;
  int len;

  CHECK_TYPEDEF (type);
  len = TYPE_LENGTH (type);

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

  /* Convert raw data to virtual format if necessary.  */
  
#ifdef REGISTER_CONVERTIBLE
  if (REGISTER_CONVERTIBLE (regnum))
    {
      REGISTER_CONVERT_TO_VIRTUAL (regnum, type,
				   raw_buffer, VALUE_CONTENTS_RAW (v));
    }
  else
#endif
    {
      /* Raw and virtual formats are the same for this register.  */

      if (TARGET_BYTE_ORDER == BIG_ENDIAN && len < REGISTER_RAW_SIZE (regnum))
	{
  	  /* Big-endian, and we want less than full size.  */
	  VALUE_OFFSET (v) = REGISTER_RAW_SIZE (regnum) - len;
	}

      memcpy (VALUE_CONTENTS_RAW (v), raw_buffer + VALUE_OFFSET (v), len);
    }
  
  return v;
}

/* Given a struct symbol for a variable or function,
   and a stack frame id, 
   return a (pointer to a) struct value containing the properly typed
   address.  */

value_ptr
locate_var_value (var, frame)
     register struct symbol *var;
     struct frame_info *frame;
{
  CORE_ADDR addr = 0;
  struct type *type = SYMBOL_TYPE (var);
  value_ptr lazy_value;

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
