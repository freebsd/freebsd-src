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

#include <machine/cons.h>

#include <ddb/ddb.h>

#include <setjmp.h>

#include "sio.h"
#include "opt_ddb.h"

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
#define BUFMAX 400

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
  siocnputc (REMOTE_DEV, c);
  return 1;
}

static int
getDebugChar (void)		/* read and return a single char */
{
  return siocngetc (REMOTE_DEV);
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
}

/* send the packet in buffer.  */

static void
putpacket (char *buffer)
{
  unsigned char checksum;
  int count;
  unsigned char ch;

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

      while (ch=buffer[count])
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
computeSignal (int exceptionVector)
{
  int sigval;
  switch (exceptionVector & ~T_USER)
    {
    case 0: sigval = 8; break; /* divide by zero */
    case 1: sigval = 5; break; /* debug exception */
    case 3: sigval = 5; break; /* breakpoint */
    case 4: sigval = 16; break; /* into instruction (overflow) */
    case 5: sigval = 16; break; /* bound instruction */
    case 6: sigval = 4; break; /* Invalid opcode */
    case 7: sigval = 8; break; /* coprocessor not available */
    case 8: sigval = 7; break; /* double fault */
    case 9: sigval = 11; break; /* coprocessor segment overrun */
    case 10: sigval = 5; break; /* Invalid TSS (also single-step) */
    case 11: sigval = 11; break; /* Segment not present */
    case 12: sigval = 11; break; /* stack exception */
    case 13: sigval = 11; break; /* general protection */
    case 14: sigval = 11; break; /* page fault */
    case 16: sigval = 7; break; /* coprocessor error */
    default:
      sigval = 7;         /* "software generated"*/
    }
  return (sigval);
}

/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */

static int
hexToInt(char **ptr, int *intValue)
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

#define NUMREGBYTES (sizeof registers)
#define PC 8
#define SP 4
#define FP 5
#define NUM_REGS 14

/*
 * This function does all command procesing for interfacing to gdb.
 */
void
gdb_handle_exception (db_regs_t *raw_regs, int type, int code)
{
  int    sigval;
  int    addr, length;
  char * ptr;
  struct i386regs {
    unsigned int eax;
    unsigned int ecx;
    unsigned int edx;
    unsigned int ebx;
    unsigned int esp;
    unsigned int ebp;
    unsigned int esi;
    unsigned int edi;
    unsigned int eip;
    unsigned int eflags;
    unsigned int cs;
    unsigned int ss;
    unsigned int ds;
    unsigned int es;
  };
  struct i386regs registers;

  registers.eax = raw_regs->tf_eax;
  registers.ebx = raw_regs->tf_ebx;
  registers.ecx = raw_regs->tf_ecx;
  registers.edx = raw_regs->tf_edx;

  registers.esp = raw_regs->tf_esp;
  registers.ebp = raw_regs->tf_ebp;
  registers.esi = raw_regs->tf_esi;
  registers.edi = raw_regs->tf_edi;

  registers.eip = raw_regs->tf_eip;
  registers.eflags = raw_regs->tf_eflags;

  registers.cs = raw_regs->tf_cs;
  registers.ss = raw_regs->tf_ss;
  registers.ds = raw_regs->tf_ds;
  registers.es = raw_regs->tf_es;

  /* reply to host that an exception has occurred */
  sigval = computeSignal (type);
  ptr = remcomOutBuffer;

  *ptr++ = 'T';
  *ptr++ = hexchars[sigval >> 4];
  *ptr++ = hexchars[sigval & 0xf];

  *ptr++ = hexchars[PC >> 4];
  *ptr++ = hexchars[PC & 0xf];
  *ptr++ = ':';
  ptr = mem2hex ((vm_offset_t)&registers.eip, ptr, 4);
  *ptr++ = ';';

  *ptr++ = hexchars[FP >> 4];
  *ptr++ = hexchars[FP & 0xf];
  *ptr++ = ':';
  ptr = mem2hex ((vm_offset_t)&registers.ebp, ptr, 4);
  *ptr++ = ';';

  *ptr++ = hexchars[SP >> 4];
  *ptr++ = hexchars[SP & 0xf];
  *ptr++ = ':';
  ptr = mem2hex ((vm_offset_t)&registers.esp, ptr, 4);
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

	case 'g':		/* return the value of the CPU registers */
	  mem2hex ((vm_offset_t)&registers, remcomOutBuffer, NUMREGBYTES);
	  break;

	case 'G':		/* set the value of the CPU registers - return OK */
	  hex2mem (&remcomInBuffer[1], (vm_offset_t)&registers, NUMREGBYTES);
	  strcpy (remcomOutBuffer, "OK");
	  break;

	case 'P':		/* Set the value of one register */
	  {
	    int regno;

	    ptr = &remcomInBuffer[1];

	    if (hexToInt (&ptr, &regno)
		&& *ptr++ == '='
		&& regno < NUM_REGS)
	      {
		hex2mem (ptr, (vm_offset_t)&registers + regno * 4, 4);
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
	    registers.eip = addr;


	  /* set the trace bit if we're stepping */
	  if (remcomInBuffer[0] == 's')
	    registers.eflags |= PSL_T;
	  else
	    registers.eflags &= ~PSL_T;

	  raw_regs->tf_eax = registers.eax;
	  raw_regs->tf_ebx = registers.ebx;
	  raw_regs->tf_ecx = registers.ecx;
	  raw_regs->tf_edx = registers.edx;

	  raw_regs->tf_esp = registers.esp;
	  raw_regs->tf_ebp = registers.ebp;
	  raw_regs->tf_esi = registers.esi;
	  raw_regs->tf_edi = registers.edi;

	  raw_regs->tf_eip = registers.eip;
	  raw_regs->tf_eflags = registers.eflags;

	  raw_regs->tf_cs = registers.cs;
	  raw_regs->tf_ss = registers.ss;
	  raw_regs->tf_ds = registers.ds;
	  raw_regs->tf_es = registers.es;
	  return;

	} /* switch */

      /* reply to the request */
      putpacket (remcomOutBuffer);
    }
}
#endif /* NSIO > 0 */
