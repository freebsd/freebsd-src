/* $FreeBSD: src/sys/alpha/alpha/alpha-gdbstub.c,v 1.7.2.1 2000/08/03 18:46:59 peter Exp $ */
/****************************************************************************

		THIS SOFTWARE IS NOT COPYRIGHTED

   HP offers the following for use in the public domain.  HP makes no
   warranty with regard to the software or its performance and the
   user accepts the software "AS IS" with all faults.

   HP DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD
   TO THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

****************************************************************************/

/****************************************************************************
 *  Header: remcom.c,v 1.34 91/03/09 12:29:49 glenne Exp $
 *
 *  Module name: remcom.c $
 *  Revision: 1.34 $
 *  Date: 91/03/09 12:29:49 $
 *  Contributor:     Lake Stevens Instrument Division$
 *
 *  Description:     low level support for gdb debugger. $
 *
 *  Considerations:  only works on target hardware $
 *
 *  Written by:      Glenn Engel $
 *  ModuleState:     Experimental $
 *
 *  NOTES:           See Below $
 *
 *  Modified for FreeBSD by Stu Grossman.
 *
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a trap #1.
 *
 *  The external function exceptionHandler() is
 *  used to attach a specific handler to a specific 386 vector number.
 *  It should use the same privilege level it runs at.  It should
 *  install it as an interrupt gate so that interrupts are masked
 *  while the handler runs.
 *  Also, need to assign exceptionHook and oldExceptionHook.
 *
 *  Because gdb will sometimes write to the stack area to execute function
 *  calls, this program cannot rely on using the supervisor stack so it
 *  uses its own stack area reserved in the int array remcomStack.
 *
 *************
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *
 *    k             kill
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *
 *    D             detach                                 OK
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ****************************************************************************/

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/cons.h>

#include <machine/reg.h>

#include <ddb/ddb.h>

#include <setjmp.h>

/* #include "sio.h" */
#include "opt_ddb.h"

#include "sio.h"

#if NSIO == 0
void
gdb_handle_exception (db_regs_t *raw_regs, int type, int code)
{
}
#else
/************************************************************************/

void		gdb_handle_exception (db_regs_t *, int, int);

extern jmp_buf	db_jmpbuf;

/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 1500

/* Create private copies of common functions used by the stub.  This prevents
   nasty interactions between app code and the stub (for instance if user steps
   into strlen, etc..) */
/* XXX this is fairly bogus.  strlen() and strcpy() should be reentrant,
   and are reentrant under FreeBSD.  In any case, our versions should not
   be named the same as the standard versions, so that the address `strlen'
   is unambiguous...  */

static int
strlen (const char *s)
{
  const char *s1 = s;

  while (*s1++ != '\000');

  return s1 - s;
}

static char *
strcpy (char *dst, const char *src)
{
  char *retval = dst;

  while ((*dst++ = *src++) != '\000');

  return retval;
}

/* XXX sio always uses its major with minor 0 no matter what we specify.  */
#define	REMOTE_DEV	0

static int
putDebugChar (int c)		/* write a single character      */
{
#if NSIO > 0
  siogdbputc (c);
#endif
  return 1;
}

static int
getDebugChar (void)		/* read and return a single char */
{
#if NSIO > 0
  return siogdbgetc ();
#else
  return 0;
#endif
}

static const char hexchars[]="0123456789abcdef";

static int
hex(char ch)
{
  if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
  if ((ch >= '0') && (ch <= '9')) return (ch-'0');
  if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
  return (-1);
}

/* scan for the sequence $<data>#<checksum>     */
static void
getpacket (char *buffer)
{
  unsigned char checksum;
  unsigned char xmitcsum;
  int i;
  int count;
  unsigned char ch;

  do
    {
      /* wait around for the start character, ignore all other characters */

      while ((ch = (getDebugChar () & 0x7f)) != '$');

      checksum = 0;
      xmitcsum = -1;

      count = 0;

      /* now, read until a # or end of buffer is found */

      while (count < BUFMAX)
	{
	  ch = getDebugChar () & 0x7f;
	  if (ch == '#')
	    break;
	  checksum = checksum + ch;
	  buffer[count] = ch;
	  count = count + 1;
	}
      buffer[count] = 0;

      if (ch == '#')
	{
	  xmitcsum = hex (getDebugChar () & 0x7f) << 4;
	  xmitcsum += hex (getDebugChar () & 0x7f);

	  if (checksum != xmitcsum)
	    putDebugChar ('-');  /* failed checksum */
	  else
	    {
	      putDebugChar ('+'); /* successful transfer */
	      /* if a sequence char is present, reply the sequence ID */
	      if (buffer[2] == ':')
		{
		  putDebugChar (buffer[0]);
		  putDebugChar (buffer[1]);

		  /* remove sequence chars from buffer */

		  count = strlen (buffer);
		  for (i=3; i <= count; i++)
		    buffer[i-3] = buffer[i];
		}
	    }
	}
    }
  while (checksum != xmitcsum);

  if (strlen(buffer) >= BUFMAX)
    panic("kgdb: buffer overflow");
}

/* send the packet in buffer.  */

static void
putpacket (char *buffer)
{
  unsigned char checksum;
  int count;
  unsigned char ch;

  if (strlen(buffer) >= BUFMAX)
    panic("kgdb: buffer overflow");

  /*  $<packet info>#<checksum>. */
  do
    {
/*
 * This is a non-standard hack to allow use of the serial console for
 * operation as well as debugging.  Simply turn on 'remotechat' in gdb.
 *
 * This extension is not part of the Cygnus protocol, is kinda gross,
 * but gets the job done.
 */
#ifdef GDB_REMOTE_CHAT
      putDebugChar ('|');
      putDebugChar ('|');
      putDebugChar ('|');
      putDebugChar ('|');
#endif
      putDebugChar ('$');
      checksum = 0;
      count = 0;

      while ((ch=buffer[count]) != 0)
	{
	  putDebugChar (ch);
	  checksum += ch;
	  count += 1;
	}

      putDebugChar ('#');
      putDebugChar (hexchars[checksum >> 4]);
      putDebugChar (hexchars[checksum & 0xf]);
    }
  while ((getDebugChar () & 0x7f) != '+');
}

static char  remcomInBuffer[BUFMAX];
static char  remcomOutBuffer[BUFMAX];

static int
get_char (vm_offset_t addr)
{
  char data;

  if (setjmp (db_jmpbuf))
    return -1;

  db_read_bytes (addr, 1, &data);

  return data & 0xff;
}

static int
set_char (vm_offset_t addr, int val)
{
  char data;

  if (setjmp (db_jmpbuf))
    return -1;

  data = val;

  db_write_bytes (addr, 1, &data);
  return 0;
}

/* convert the memory pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */

static char *
mem2hex (vm_offset_t mem, char *buf, int count)
{
      int i;
      int ch;

      for (i=0;i<count;i++) {
          ch = get_char (mem++);
	  if (ch == -1)
	    return NULL;
          *buf++ = hexchars[ch >> 4];
          *buf++ = hexchars[ch % 16];
      }
      *buf = 0;
      return(buf);
}

/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
static char *
hex2mem (char *buf, vm_offset_t mem, int count)
{
      int i;
      int ch;
      int rv;

      for (i=0;i<count;i++) {
          ch = hex(*buf++) << 4;
          ch = ch + hex(*buf++);
          rv = set_char (mem++, ch);
	  if (rv == -1)
	    return NULL;
      }
      return(buf);
}

/* this function takes the 386 exception vector and attempts to
   translate this number into a unix compatible signal value */
static int
computeSignal (int entry, int code)
{
    switch (entry) {
    case ALPHA_KENTRY_INT:
    case ALPHA_KENTRY_ARITH:
	return SIGILL;		/* ? can this happen? */
    case ALPHA_KENTRY_MM:
	switch (code) {
	case ALPHA_MMCSR_INVALTRANS:
	    return SIGSEGV;
	case ALPHA_MMCSR_ACCESS:
	case ALPHA_MMCSR_FOR:
	case ALPHA_MMCSR_FOE:
	case ALPHA_MMCSR_FOW:
	    return SIGBUS;
	}
    case ALPHA_KENTRY_IF:
	switch (code) {
	case ALPHA_IF_CODE_BUGCHK:
	case ALPHA_IF_CODE_BPT:
	    return SIGTRAP;
	case ALPHA_IF_CODE_GENTRAP:
	case ALPHA_IF_CODE_FEN:
	case ALPHA_IF_CODE_OPDEC:
	    return SIGILL;
	}
    case ALPHA_KENTRY_UNA:
	return SIGSEGV;
    case ALPHA_KENTRY_SYS:
	return SIGILL;
    }
    return SIGILL;
}

/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */

static int
hexToInt(char **ptr, long *intValue)
{
    int numChars = 0;
    int hexValue;

    *intValue = 0;

    while (**ptr)
    {
        hexValue = hex(**ptr);
        if (hexValue >=0)
        {
            *intValue = (*intValue <<4) | hexValue;
            numChars ++;
        }
        else
            break;

        (*ptr)++;
    }

    return (numChars);
}

static enum {
  NONE,
  NORMAL,
  BRANCH
} ss_mode = NONE;

struct ss_bpt {
  int active;
  vm_offset_t addr;
  u_int32_t contents;
};

static struct ss_bpt ss_bp1, ss_bp2;

static int
set_bpt(struct ss_bpt* bp)
{
  u_int32_t bp_ins = BKPT_INST;
  if (bp->active) return 0;
  if (badaddr((caddr_t)bp->addr, 4))
    return 0;
  db_read_bytes(bp->addr, 4, (caddr_t) &bp->contents);
  db_write_bytes(bp->addr, 4, (caddr_t) &bp_ins);
  bp->active = 1;
  return 1;
}

static void
clear_bpt(struct ss_bpt* bp)
{
  if (!bp->active) return;
  if (badaddr((caddr_t) bp->addr, 4))
    return;
  db_write_bytes(bp->addr, 4, (caddr_t) &bp->contents);
  bp->active = 0;
}

static int
set_single_step(db_regs_t* regs)
{
  u_int32_t ins;
  vm_offset_t pc = regs->tf_regs[FRAME_PC];

  if (ss_mode != NONE) {
    printf("single_step botch\n");
    return 0;
  }

  if (badaddr((caddr_t) pc, 4))
    return 0;
  db_read_bytes(pc, 4, (caddr_t) &ins);
  ss_bp1.addr = pc + 4;
  if (db_inst_branch(ins)) {
    ss_bp2.addr = db_branch_taken(ins, pc, regs);
    if (!set_bpt(&ss_bp1))
      return 0;
    if (!set_bpt(&ss_bp2)) {
      clear_bpt(&ss_bp1);
      return 0;
    }
    ss_mode = BRANCH;
  } else {
    if (!set_bpt(&ss_bp1))
      return 0;
    ss_mode = NORMAL;
  }

  return 1;
}

static void
clear_single_step(db_regs_t* regs)
{
  /* if we hit one of the step breakpoints, adjust pc */
  if (ss_mode == BRANCH)
    {
      /* remove in reverse order in case they are at the same address */
      if (regs->tf_regs[FRAME_PC] == ss_bp1.addr + 4
	  || regs->tf_regs[FRAME_PC] == ss_bp2.addr + 4)
	regs->tf_regs[FRAME_PC] -= 4;
      clear_bpt(&ss_bp2);
      clear_bpt(&ss_bp1);
    }
  else if (ss_mode == NORMAL)
    {
      if (regs->tf_regs[FRAME_PC] == ss_bp1.addr + 4)
	regs->tf_regs[FRAME_PC] -= 4;
      clear_bpt(&ss_bp1);
    }
  ss_mode = NONE;
}

#define NUMREGBYTES (sizeof registers)
#define PC 64
#define SP 30
#define FP 15
#define VFP 65
#define NUM_REGS 66

/*
 * Map trapframe indices into gdb (integer) register indices.
 * Entries not in integer register set are set to -1.
 */
static int tf2gdb[FRAME_SIZE] = {
/*0*/	R_V0,	R_T0,	R_T1,	R_T2,	R_T3,	R_T4,	R_T5,	R_T6,
/*8*/	R_T7,	R_S0,	R_S1,	R_S2,	R_S3,	R_S4,	R_S5,	R_S6,
/*16*/	R_A3,	R_A4,	R_A5,	R_T8,	R_T9,	R_T10,	R_T11,	R_RA,
/*24*/	R_T12,	R_AT,	R_SP,	-1,	-1,	-1,	-1,	-1,
/*32*/	R_GP,	R_A0,	R_A1,	R_A2,
};

/*
 * Map gdb register indices back to trapframe.
 * Entries not in trapframe are set to -1.
 */
static int gdb2tf[NUM_REGS] = {
	/* integer registers */
	FRAME_V0,	FRAME_T0,	FRAME_T1,	FRAME_T2,
	FRAME_T3,	FRAME_T4,	FRAME_T5,	FRAME_T6,
	FRAME_T7,	FRAME_S0,	FRAME_S1,	FRAME_S2,
	FRAME_S3,	FRAME_S4,	FRAME_S5,	FRAME_S6,
	FRAME_A0,	FRAME_A1,	FRAME_A2,	FRAME_A3,
	FRAME_A4,	FRAME_A5,	FRAME_T8,	FRAME_T9,
	FRAME_T10,	FRAME_T11,	FRAME_RA,	FRAME_T12,
	FRAME_AT,	FRAME_GP,	FRAME_SP,	-1,
	/* float registers */
	-1,		-1,		-1,		-1,
	-1,		-1,		-1,		-1,
	-1,		-1,		-1,		-1,
	-1,		-1,		-1,		-1,
	-1,		-1,		-1,		-1,
	-1,		-1,		-1,		-1,
	-1,		-1,		-1,		-1,
	-1,		-1,		-1,		-1,
	/* misc registers */
	FRAME_PC,	-1,
};

/*
 * This function does all command procesing for interfacing to gdb.
 */
void
gdb_handle_exception (db_regs_t *raw_regs, int type, int code)
{
  int    sigval;
  long   addr, length;
  char * ptr;
  struct alpharegs {
    u_int64_t r[32];
    u_int64_t f[32];
    u_int64_t pc, vfp;
  };
  static struct alpharegs registers;
  int i;

  clear_single_step(raw_regs);

  bzero(&registers, sizeof registers);

  /*
   * Map trapframe to registers.
   * Ignore float regs for now.
   */
  for (i = 0; i < FRAME_SIZE; i++)
    if (tf2gdb[i] >= 0)
      registers.r[tf2gdb[i]] = raw_regs->tf_regs[i];
  registers.pc = raw_regs->tf_regs[FRAME_PC];

  /* reply to host that an exception has occurred */
  sigval = computeSignal (type, code);
  ptr = remcomOutBuffer;

  *ptr++ = 'T';
  *ptr++ = hexchars[sigval >> 4];
  *ptr++ = hexchars[sigval & 0xf];

  *ptr++ = hexchars[PC >> 4];
  *ptr++ = hexchars[PC & 0xf];
  *ptr++ = ':';
  ptr = mem2hex ((vm_offset_t)&registers.pc, ptr, 8);
  *ptr++ = ';';

  *ptr++ = hexchars[FP >> 4];
  *ptr++ = hexchars[FP & 0xf];
  *ptr++ = ':';
  ptr = mem2hex ((vm_offset_t)&registers.r[FP], ptr, 8);
  *ptr++ = ';';

  *ptr++ = hexchars[SP >> 4];
  *ptr++ = hexchars[SP & 0xf];
  *ptr++ = ':';
  ptr = mem2hex ((vm_offset_t)&registers.r[SP], ptr, 8);
  *ptr++ = ';';

  *ptr++ = 0;

  putpacket (remcomOutBuffer);

  while (1)
    {
      remcomOutBuffer[0] = 0;

      getpacket (remcomInBuffer);
      switch (remcomInBuffer[0]) 
	{
	case '?':
	  remcomOutBuffer[0] = 'S';
	  remcomOutBuffer[1] = hexchars[sigval >> 4];
	  remcomOutBuffer[2] = hexchars[sigval % 16];
	  remcomOutBuffer[3] = 0;
	  break;

	case 'D':		/* detach; say OK and turn off gdb */
	  putpacket(remcomOutBuffer);
	  boothowto &= ~RB_GDB;
	  return;

	case 'k':
	  prom_halt();
	  /*NOTREACHED*/
	  break;

	case 'g':		/* return the value of the CPU registers */
	  mem2hex ((vm_offset_t)&registers, remcomOutBuffer, NUMREGBYTES);
	  break;

	case 'G':		/* set the value of the CPU registers - return OK */
	  hex2mem (&remcomInBuffer[1], (vm_offset_t)&registers, NUMREGBYTES);
	  strcpy (remcomOutBuffer, "OK");
	  break;

	case 'P':		/* Set the value of one register */
	  {
	    long regno;

	    ptr = &remcomInBuffer[1];

	    if (hexToInt (&ptr, &regno)
		&& *ptr++ == '='
		&& regno < NUM_REGS)
	      {
		hex2mem (ptr, (vm_offset_t)&registers + regno * 8, 8);
		strcpy(remcomOutBuffer,"OK");
	      }
	    else
	      strcpy (remcomOutBuffer, "P01");
	    break;
	  }
	case 'm':	/* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
	  /* Try to read %x,%x.  */

	  ptr = &remcomInBuffer[1];

	  if (hexToInt (&ptr, &addr)
	      && *(ptr++) == ','
	      && hexToInt (&ptr, &length))
	    {
	      if (mem2hex((vm_offset_t) addr, remcomOutBuffer, length) == NULL)
		strcpy (remcomOutBuffer, "E03");
	      break;
	    }
	  else
	    strcpy (remcomOutBuffer, "E01");
	  break;

	case 'M': /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */

	  /* Try to read '%x,%x:'.  */

	  ptr = &remcomInBuffer[1];

	  if (hexToInt(&ptr,&addr)
	      && *(ptr++) == ','
	      && hexToInt(&ptr, &length)
	      && *(ptr++) == ':')
	    {
	      if (hex2mem(ptr, (vm_offset_t) addr, length) == NULL)
		strcpy (remcomOutBuffer, "E03");
	      else
		strcpy (remcomOutBuffer, "OK");
	    }
	  else
	    strcpy (remcomOutBuffer, "E02");
	  break;

	  /* cAA..AA    Continue at address AA..AA(optional) */
	  /* sAA..AA   Step one instruction from AA..AA(optional) */
	case 'c' :
	case 's' :
	  /* try to read optional parameter, pc unchanged if no parm */

	  ptr = &remcomInBuffer[1];
	  if (hexToInt(&ptr,&addr))
	    registers.pc = addr;

	  /*
	   * Map gdb registers back to trapframe (ignoring fp regs).
	   */
	  for (i = 0; i < NUM_REGS; i++)
	    if (gdb2tf[i] >= 0)
	      raw_regs->tf_regs[gdb2tf[i]] = registers.r[i];
	  raw_regs->tf_regs[FRAME_PC] = registers.pc;

	  if (remcomInBuffer[0] == 's')
	    if (!set_single_step(raw_regs))
	      printf("Can't set single step breakpoint\n");

	  return;

	} /* switch */

      /* reply to the request */
      putpacket (remcomOutBuffer);
    }
}
#endif /* NSIO > 0 */
