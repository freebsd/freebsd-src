/* Cache and manage the values of registers for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1994, 1995, 1996, 1998, 2000, 2001
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

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "gdbarch.h"
#include "gdbcmd.h"
#include "regcache.h"
#include "gdb_assert.h"

/*
 * DATA STRUCTURE
 *
 * Here is the actual register cache.
 */

/* NOTE: this is a write-through cache.  There is no "dirty" bit for
   recording if the register values have been changed (eg. by the
   user).  Therefore all registers must be written back to the
   target when appropriate.  */

/* REGISTERS contains the cached register values (in target byte order). */

char *registers;

/* REGISTER_VALID is 0 if the register needs to be fetched,
                     1 if it has been fetched, and
		    -1 if the register value was not available.  
   "Not available" means don't try to fetch it again.  */

signed char *register_valid;

/* The thread/process associated with the current set of registers. */

static ptid_t registers_ptid;

/*
 * FUNCTIONS:
 */

/* REGISTER_CACHED()

   Returns 0 if the value is not in the cache (needs fetch).
          >0 if the value is in the cache.
	  <0 if the value is permanently unavailable (don't ask again).  */

int
register_cached (int regnum)
{
  return register_valid[regnum];
}

/* Record that REGNUM's value is cached if STATE is >0, uncached but
   fetchable if STATE is 0, and uncached and unfetchable if STATE is <0.  */

void
set_register_cached (int regnum, int state)
{
  register_valid[regnum] = state;
}

/* REGISTER_CHANGED

   invalidate a single register REGNUM in the cache */
void
register_changed (int regnum)
{
  set_register_cached (regnum, 0);
}

/* If REGNUM >= 0, return a pointer to register REGNUM's cache buffer area,
   else return a pointer to the start of the cache buffer.  */

static char *
register_buffer (int regnum)
{
  gdb_assert (regnum >= 0 && regnum < (NUM_REGS + NUM_PSEUDO_REGS));
  return &registers[REGISTER_BYTE (regnum)];
}

/* Return whether register REGNUM is a real register.  */

static int
real_register (int regnum)
{
  return regnum >= 0 && regnum < NUM_REGS;
}

/* Return whether register REGNUM is a pseudo register.  */

static int
pseudo_register (int regnum)
{
  return regnum >= NUM_REGS && regnum < NUM_REGS + NUM_PSEUDO_REGS;
}

/* Fetch register REGNUM into the cache.  */

static void
fetch_register (int regnum)
{
  /* NOTE: cagney/2001-12-04: Legacy targets were using fetch/store
     pseudo-register as a way of handling registers that needed to be
     constructed from one or more raw registers.  New targets instead
     use gdbarch register read/write.  */
  if (FETCH_PSEUDO_REGISTER_P ()
      && pseudo_register (regnum))
    FETCH_PSEUDO_REGISTER (regnum);
  else
    target_fetch_registers (regnum);
}

/* Write register REGNUM cached value to the target.  */

static void
store_register (int regnum)
{
  /* NOTE: cagney/2001-12-04: Legacy targets were using fetch/store
     pseudo-register as a way of handling registers that needed to be
     constructed from one or more raw registers.  New targets instead
     use gdbarch register read/write.  */
  if (STORE_PSEUDO_REGISTER_P ()
      && pseudo_register (regnum))
    STORE_PSEUDO_REGISTER (regnum);
  else
    target_store_registers (regnum);
}

/* Low level examining and depositing of registers.

   The caller is responsible for making sure that the inferior is
   stopped before calling the fetching routines, or it will get
   garbage.  (a change from GDB version 3, in which the caller got the
   value from the last stop).  */

/* REGISTERS_CHANGED ()

   Indicate that registers may have changed, so invalidate the cache.  */

void
registers_changed (void)
{
  int i;

  registers_ptid = pid_to_ptid (-1);

  /* Force cleanup of any alloca areas if using C alloca instead of
     a builtin alloca.  This particular call is used to clean up
     areas allocated by low level target code which may build up
     during lengthy interactions between gdb and the target before
     gdb gives control to the user (ie watchpoints).  */
  alloca (0);

  for (i = 0; i < NUM_REGS + NUM_PSEUDO_REGS; i++)
    set_register_cached (i, 0);

  if (registers_changed_hook)
    registers_changed_hook ();
}

/* REGISTERS_FETCHED ()

   Indicate that all registers have been fetched, so mark them all valid.  */

/* NOTE: cagney/2001-12-04: This function does not set valid on the
   pseudo-register range since pseudo registers are always supplied
   using supply_register().  */
/* FIXME: cagney/2001-12-04: This function is DEPRECATED.  The target
   code was blatting the registers[] array and then calling this.
   Since targets should only be using supply_register() the need for
   this function/hack is eliminated.  */

void
registers_fetched (void)
{
  int i;

  for (i = 0; i < NUM_REGS; i++)
    set_register_cached (i, 1);
  /* Do not assume that the pseudo-regs have also been fetched.
     Fetching all real regs NEVER accounts for pseudo-regs.  */
}

/* read_register_bytes and write_register_bytes are generally a *BAD*
   idea.  They are inefficient because they need to check for partial
   updates, which can only be done by scanning through all of the
   registers and seeing if the bytes that are being read/written fall
   inside of an invalid register.  [The main reason this is necessary
   is that register sizes can vary, so a simple index won't suffice.]
   It is far better to call read_register_gen and write_register_gen
   if you want to get at the raw register contents, as it only takes a
   regnum as an argument, and therefore can't do a partial register
   update.

   Prior to the recent fixes to check for partial updates, both read
   and write_register_bytes always checked to see if any registers
   were stale, and then called target_fetch_registers (-1) to update
   the whole set.  This caused really slowed things down for remote
   targets.  */

/* Copy INLEN bytes of consecutive data from registers
   starting with the INREGBYTE'th byte of register data
   into memory at MYADDR.  */

void
read_register_bytes (int in_start, char *in_buf, int in_len)
{
  int in_end = in_start + in_len;
  int regnum;
  char *reg_buf = alloca (MAX_REGISTER_RAW_SIZE);

  /* See if we are trying to read bytes from out-of-date registers.  If so,
     update just those registers.  */

  for (regnum = 0; regnum < NUM_REGS + NUM_PSEUDO_REGS; regnum++)
    {
      int reg_start;
      int reg_end;
      int reg_len;
      int start;
      int end;
      int byte;

      reg_start = REGISTER_BYTE (regnum);
      reg_len = REGISTER_RAW_SIZE (regnum);
      reg_end = reg_start + reg_len;

      if (reg_end <= in_start || in_end <= reg_start)
	/* The range the user wants to read doesn't overlap with regnum.  */
	continue;

      if (REGISTER_NAME (regnum) != NULL && *REGISTER_NAME (regnum) != '\0')
	/* Force the cache to fetch the entire register.  */
	read_register_gen (regnum, reg_buf);
      else
	/* Legacy note: even though this register is ``invalid'' we
           still need to return something.  It would appear that some
           code relies on apparent gaps in the register array also
           being returned.  */
	/* FIXME: cagney/2001-08-18: This is just silly.  It defeats
           the entire register read/write flow of control.  Must
           resist temptation to return 0xdeadbeef.  */
	memcpy (reg_buf, registers + reg_start, reg_len);

      /* Legacy note: This function, for some reason, allows a NULL
         input buffer.  If the buffer is NULL, the registers are still
         fetched, just the final transfer is skipped. */
      if (in_buf == NULL)
	continue;

      /* start = max (reg_start, in_start) */
      if (reg_start > in_start)
	start = reg_start;
      else
	start = in_start;

      /* end = min (reg_end, in_end) */
      if (reg_end < in_end)
	end = reg_end;
      else
	end = in_end;

      /* Transfer just the bytes common to both IN_BUF and REG_BUF */
      for (byte = start; byte < end; byte++)
	{
	  in_buf[byte - in_start] = reg_buf[byte - reg_start];
	}
    }
}

/* Read register REGNUM into memory at MYADDR, which must be large
   enough for REGISTER_RAW_BYTES (REGNUM).  Target byte-order.  If the
   register is known to be the size of a CORE_ADDR or smaller,
   read_register can be used instead.  */

static void
legacy_read_register_gen (int regnum, char *myaddr)
{
  gdb_assert (regnum >= 0 && regnum < (NUM_REGS + NUM_PSEUDO_REGS));
  if (! ptid_equal (registers_ptid, inferior_ptid))
    {
      registers_changed ();
      registers_ptid = inferior_ptid;
    }

  if (!register_cached (regnum))
    fetch_register (regnum);

  memcpy (myaddr, register_buffer (regnum),
	  REGISTER_RAW_SIZE (regnum));
}

void
regcache_read (int rawnum, char *buf)
{
  gdb_assert (rawnum >= 0 && rawnum < (NUM_REGS + NUM_PSEUDO_REGS));
  /* For moment, just use underlying legacy code. Ulgh!!! */
  legacy_read_register_gen (rawnum, buf);
}

void
read_register_gen (int regnum, char *buf)
{
  if (! gdbarch_register_read_p (current_gdbarch))
    {
      legacy_read_register_gen (regnum, buf);
      return;
    }
  gdbarch_register_read (current_gdbarch, regnum, buf);
}


/* Write register REGNUM at MYADDR to the target.  MYADDR points at
   REGISTER_RAW_BYTES(REGNUM), which must be in target byte-order.  */

static void
legacy_write_register_gen (int regnum, char *myaddr)
{
  int size;
  gdb_assert (regnum >= 0 && regnum < (NUM_REGS + NUM_PSEUDO_REGS));

  /* On the sparc, writing %g0 is a no-op, so we don't even want to
     change the registers array if something writes to this register.  */
  if (CANNOT_STORE_REGISTER (regnum))
    return;

  if (! ptid_equal (registers_ptid, inferior_ptid))
    {
      registers_changed ();
      registers_ptid = inferior_ptid;
    }

  size = REGISTER_RAW_SIZE (regnum);

  if (real_register (regnum))
    {
      /* If we have a valid copy of the register, and new value == old
	 value, then don't bother doing the actual store. */
      if (register_cached (regnum)
	  && memcmp (register_buffer (regnum), myaddr, size) == 0)
	return;
      else
	target_prepare_to_store ();
    }

  memcpy (register_buffer (regnum), myaddr, size);

  set_register_cached (regnum, 1);
  store_register (regnum);
}

void
regcache_write (int rawnum, char *buf)
{
  gdb_assert (rawnum >= 0 && rawnum < (NUM_REGS + NUM_PSEUDO_REGS));
  /* For moment, just use underlying legacy code. Ulgh!!! */
  legacy_write_register_gen (rawnum, buf);
}

void
write_register_gen (int regnum, char *buf)
{
  if (! gdbarch_register_write_p (current_gdbarch))
    {
      legacy_write_register_gen (regnum, buf);
      return;
    }
  gdbarch_register_write (current_gdbarch, regnum, buf);
}

/* Copy INLEN bytes of consecutive data from memory at MYADDR
   into registers starting with the MYREGSTART'th byte of register data.  */

void
write_register_bytes (int myregstart, char *myaddr, int inlen)
{
  int myregend = myregstart + inlen;
  int regnum;

  target_prepare_to_store ();

  /* Scan through the registers updating any that are covered by the
     range myregstart<=>myregend using write_register_gen, which does
     nice things like handling threads, and avoiding updates when the
     new and old contents are the same.  */

  for (regnum = 0; regnum < NUM_REGS + NUM_PSEUDO_REGS; regnum++)
    {
      int regstart, regend;

      regstart = REGISTER_BYTE (regnum);
      regend = regstart + REGISTER_RAW_SIZE (regnum);

      /* Is this register completely outside the range the user is writing?  */
      if (myregend <= regstart || regend <= myregstart)
	/* do nothing */ ;		

      /* Is this register completely within the range the user is writing?  */
      else if (myregstart <= regstart && regend <= myregend)
	write_register_gen (regnum, myaddr + (regstart - myregstart));

      /* The register partially overlaps the range being written.  */
      else
	{
	  char *regbuf = (char*) alloca (MAX_REGISTER_RAW_SIZE);
	  /* What's the overlap between this register's bytes and
             those the caller wants to write?  */
	  int overlapstart = max (regstart, myregstart);
	  int overlapend   = min (regend,   myregend);

	  /* We may be doing a partial update of an invalid register.
	     Update it from the target before scribbling on it.  */
	  read_register_gen (regnum, regbuf);

	  memcpy (registers + overlapstart,
		  myaddr + (overlapstart - myregstart),
		  overlapend - overlapstart);

	  store_register (regnum);
	}
    }
}


/* Return the contents of register REGNUM as an unsigned integer.  */

ULONGEST
read_register (int regnum)
{
  char *buf = alloca (REGISTER_RAW_SIZE (regnum));
  read_register_gen (regnum, buf);
  return (extract_unsigned_integer (buf, REGISTER_RAW_SIZE (regnum)));
}

ULONGEST
read_register_pid (int regnum, ptid_t ptid)
{
  ptid_t save_ptid;
  int save_pid;
  CORE_ADDR retval;

  if (ptid_equal (ptid, inferior_ptid))
    return read_register (regnum);

  save_ptid = inferior_ptid;

  inferior_ptid = ptid;

  retval = read_register (regnum);

  inferior_ptid = save_ptid;

  return retval;
}

/* Return the contents of register REGNUM as a signed integer.  */

LONGEST
read_signed_register (int regnum)
{
  void *buf = alloca (REGISTER_RAW_SIZE (regnum));
  read_register_gen (regnum, buf);
  return (extract_signed_integer (buf, REGISTER_RAW_SIZE (regnum)));
}

LONGEST
read_signed_register_pid (int regnum, ptid_t ptid)
{
  ptid_t save_ptid;
  LONGEST retval;

  if (ptid_equal (ptid, inferior_ptid))
    return read_signed_register (regnum);

  save_ptid = inferior_ptid;

  inferior_ptid = ptid;

  retval = read_signed_register (regnum);

  inferior_ptid = save_ptid;

  return retval;
}

/* Store VALUE into the raw contents of register number REGNUM.  */

void
write_register (int regnum, LONGEST val)
{
  void *buf;
  int size;
  size = REGISTER_RAW_SIZE (regnum);
  buf = alloca (size);
  store_signed_integer (buf, size, (LONGEST) val);
  write_register_gen (regnum, buf);
}

void
write_register_pid (int regnum, CORE_ADDR val, ptid_t ptid)
{
  ptid_t save_ptid;

  if (ptid_equal (ptid, inferior_ptid))
    {
      write_register (regnum, val);
      return;
    }

  save_ptid = inferior_ptid;

  inferior_ptid = ptid;

  write_register (regnum, val);

  inferior_ptid = save_ptid;
}

/* SUPPLY_REGISTER()

   Record that register REGNUM contains VAL.  This is used when the
   value is obtained from the inferior or core dump, so there is no
   need to store the value there.

   If VAL is a NULL pointer, then it's probably an unsupported register.
   We just set its value to all zeros.  We might want to record this
   fact, and report it to the users of read_register and friends.  */

void
supply_register (int regnum, char *val)
{
#if 1
  if (! ptid_equal (registers_ptid, inferior_ptid))
    {
      registers_changed ();
      registers_ptid = inferior_ptid;
    }
#endif

  set_register_cached (regnum, 1);
  if (val)
    memcpy (register_buffer (regnum), val, 
	    REGISTER_RAW_SIZE (regnum));
  else
    memset (register_buffer (regnum), '\000', 
	    REGISTER_RAW_SIZE (regnum));

  /* On some architectures, e.g. HPPA, there are a few stray bits in
     some registers, that the rest of the code would like to ignore.  */

  /* NOTE: cagney/2001-03-16: The macro CLEAN_UP_REGISTER_VALUE is
     going to be deprecated.  Instead architectures will leave the raw
     register value as is and instead clean things up as they pass
     through the method gdbarch_register_read() clean up the
     values. */

#ifdef DEPRECATED_CLEAN_UP_REGISTER_VALUE
  DEPRECATED_CLEAN_UP_REGISTER_VALUE (regnum, register_buffer (regnum));
#endif
}

void
regcache_collect (int regnum, void *buf)
{
  memcpy (buf, register_buffer (regnum), REGISTER_RAW_SIZE (regnum));
}


/* read_pc, write_pc, read_sp, write_sp, read_fp, write_fp, etc.
   Special handling for registers PC, SP, and FP.  */

/* NOTE: cagney/2001-02-18: The functions generic_target_read_pc(),
   read_pc_pid(), read_pc(), generic_target_write_pc(),
   write_pc_pid(), write_pc(), generic_target_read_sp(), read_sp(),
   generic_target_write_sp(), write_sp(), generic_target_read_fp(),
   read_fp(), generic_target_write_fp(), write_fp will eventually be
   moved out of the reg-cache into either frame.[hc] or to the
   multi-arch framework.  The are not part of the raw register cache.  */

/* This routine is getting awfully cluttered with #if's.  It's probably
   time to turn this into READ_PC and define it in the tm.h file.
   Ditto for write_pc.

   1999-06-08: The following were re-written so that it assumes the
   existence of a TARGET_READ_PC et.al. macro.  A default generic
   version of that macro is made available where needed.

   Since the ``TARGET_READ_PC'' et.al. macro is going to be controlled
   by the multi-arch framework, it will eventually be possible to
   eliminate the intermediate read_pc_pid().  The client would call
   TARGET_READ_PC directly. (cagney). */

CORE_ADDR
generic_target_read_pc (ptid_t ptid)
{
#ifdef PC_REGNUM
  if (PC_REGNUM >= 0)
    {
      CORE_ADDR pc_val = ADDR_BITS_REMOVE ((CORE_ADDR) read_register_pid (PC_REGNUM, ptid));
      return pc_val;
    }
#endif
  internal_error (__FILE__, __LINE__,
		  "generic_target_read_pc");
  return 0;
}

CORE_ADDR
read_pc_pid (ptid_t ptid)
{
  ptid_t saved_inferior_ptid;
  CORE_ADDR pc_val;

  /* In case ptid != inferior_ptid. */
  saved_inferior_ptid = inferior_ptid;
  inferior_ptid = ptid;

  pc_val = TARGET_READ_PC (ptid);

  inferior_ptid = saved_inferior_ptid;
  return pc_val;
}

CORE_ADDR
read_pc (void)
{
  return read_pc_pid (inferior_ptid);
}

void
generic_target_write_pc (CORE_ADDR pc, ptid_t ptid)
{
#ifdef PC_REGNUM
  if (PC_REGNUM >= 0)
    write_register_pid (PC_REGNUM, pc, ptid);
  if (NPC_REGNUM >= 0)
    write_register_pid (NPC_REGNUM, pc + 4, ptid);
  if (NNPC_REGNUM >= 0)
    write_register_pid (NNPC_REGNUM, pc + 8, ptid);
#else
  internal_error (__FILE__, __LINE__,
		  "generic_target_write_pc");
#endif
}

void
write_pc_pid (CORE_ADDR pc, ptid_t ptid)
{
  ptid_t saved_inferior_ptid;

  /* In case ptid != inferior_ptid. */
  saved_inferior_ptid = inferior_ptid;
  inferior_ptid = ptid;

  TARGET_WRITE_PC (pc, ptid);

  inferior_ptid = saved_inferior_ptid;
}

void
write_pc (CORE_ADDR pc)
{
  write_pc_pid (pc, inferior_ptid);
}

/* Cope with strage ways of getting to the stack and frame pointers */

CORE_ADDR
generic_target_read_sp (void)
{
#ifdef SP_REGNUM
  if (SP_REGNUM >= 0)
    return read_register (SP_REGNUM);
#endif
  internal_error (__FILE__, __LINE__,
		  "generic_target_read_sp");
}

CORE_ADDR
read_sp (void)
{
  return TARGET_READ_SP ();
}

void
generic_target_write_sp (CORE_ADDR val)
{
#ifdef SP_REGNUM
  if (SP_REGNUM >= 0)
    {
      write_register (SP_REGNUM, val);
      return;
    }
#endif
  internal_error (__FILE__, __LINE__,
		  "generic_target_write_sp");
}

void
write_sp (CORE_ADDR val)
{
  TARGET_WRITE_SP (val);
}

CORE_ADDR
generic_target_read_fp (void)
{
#ifdef FP_REGNUM
  if (FP_REGNUM >= 0)
    return read_register (FP_REGNUM);
#endif
  internal_error (__FILE__, __LINE__,
		  "generic_target_read_fp");
}

CORE_ADDR
read_fp (void)
{
  return TARGET_READ_FP ();
}

void
generic_target_write_fp (CORE_ADDR val)
{
#ifdef FP_REGNUM
  if (FP_REGNUM >= 0)
    {
      write_register (FP_REGNUM, val);
      return;
    }
#endif
  internal_error (__FILE__, __LINE__,
		  "generic_target_write_fp");
}

void
write_fp (CORE_ADDR val)
{
  TARGET_WRITE_FP (val);
}

/* ARGSUSED */
static void
reg_flush_command (char *command, int from_tty)
{
  /* Force-flush the register cache.  */
  registers_changed ();
  if (from_tty)
    printf_filtered ("Register cache flushed.\n");
}

static void
build_regcache (void)
{
  int i;
  int sizeof_register_valid;
  /* Come up with the real size of the registers buffer.  */
  int sizeof_registers = REGISTER_BYTES; /* OK use.  */
  for (i = 0; i < NUM_REGS + NUM_PSEUDO_REGS; i++)
    {
      long regend;
      /* Keep extending the buffer so that there is always enough
         space for all registers.  The comparison is necessary since
         legacy code is free to put registers in random places in the
         buffer separated by holes.  Once REGISTER_BYTE() is killed
         this can be greatly simplified.  */
      /* FIXME: cagney/2001-12-04: This code shouldn't need to use
         REGISTER_BYTE().  Unfortunatly, legacy code likes to lay the
         buffer out so that certain registers just happen to overlap.
         Ulgh!  New targets use gdbarch's register read/write and
         entirely avoid this uglyness.  */
      regend = REGISTER_BYTE (i) + REGISTER_RAW_SIZE (i);
      if (sizeof_registers < regend)
	sizeof_registers = regend;
    }
  registers = xmalloc (sizeof_registers);
  sizeof_register_valid = ((NUM_REGS + NUM_PSEUDO_REGS)
			   * sizeof (*register_valid));
  register_valid = xmalloc (sizeof_register_valid);
  memset (register_valid, 0, sizeof_register_valid);
}

void
_initialize_regcache (void)
{
  build_regcache ();

  register_gdbarch_swap (&registers, sizeof (registers), NULL);
  register_gdbarch_swap (&register_valid, sizeof (register_valid), NULL);
  register_gdbarch_swap (NULL, 0, build_regcache);

  add_com ("flushregs", class_maintenance, reg_flush_command,
	   "Force gdb to flush its register cache (maintainer command)");

   /* Initialize the thread/process associated with the current set of
      registers.  For now, -1 is special, and means `no current process'.  */
  registers_ptid = pid_to_ptid (-1);
}
