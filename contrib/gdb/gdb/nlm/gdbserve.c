/* gdbserve.c -- NLM debugging stub for Novell NetWare.

   This is originally based on an m68k software stub written by Glenn
   Engel at HP, but has changed quite a bit.  It was modified for the
   i386 by Jim Kingdon, Cygnus Support.  It was modified to run under
   NetWare by Ian Lance Taylor, Cygnus Support.

   This code is intended to produce an NLM (a NetWare Loadable Module)
   to run under Novell NetWare.  To create the NLM, compile this code
   into an object file using the NLM SDK on any i386 host, and use the
   nlmconv program (available in the GNU binutils) to transform the
   resulting object file into an NLM.  */

/****************************************************************************

		THIS SOFTWARE IS NOT COPYRIGHTED

   HP offers the following for use in the public domain.  HP makes no
   warranty with regard to the software or it's performance and the
   user accepts the software "AS IS" with all faults.

   HP DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD
   TO THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

****************************************************************************/

/****************************************************************************
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#ifdef __i386__
#include <dfs.h>
#include <conio.h>
#include <advanced.h>
#include <debugapi.h>
#include <process.h>
#else
#include <nwtypes.h>
#include <nwdfs.h>
#include <nwconio.h>
#include <nwadv.h>
#include <nwdbgapi.h>
#include <nwthread.h>
#endif

#include <aio.h>
#include "cpu.h"


/****************************************************/
/* This information is from Novell.  It is not in any of the standard
   NetWare header files.  */

struct DBG_LoadDefinitionStructure
{
	void *reserved1[4];
	LONG reserved5;
	LONG LDCodeImageOffset;
	LONG LDCodeImageLength;
	LONG LDDataImageOffset;
	LONG LDDataImageLength;
	LONG LDUninitializedDataLength;
	LONG LDCustomDataOffset;
	LONG LDCustomDataSize;
	LONG reserved6[2];
	LONG (*LDInitializationProcedure)(void);
};

#define LO_NORMAL		0x0000
#define LO_STARTUP		0x0001
#define LO_PROTECT		0x0002
#define LO_DEBUG		0x0004
#define LO_AUTO_LOAD  		0x0008

/* Loader returned error codes */
#define LOAD_COULD_NOT_FIND_FILE		1
#define LOAD_ERROR_READING_FILE			2
#define LOAD_NOT_NLM_FILE_FORMAT		3
#define LOAD_WRONG_NLM_FILE_VERSION		4
#define LOAD_REENTRANT_INITIALIZE_FAILURE	5
#define LOAD_CAN_NOT_LOAD_MULTIPLE_COPIES	6
#define LOAD_ALREADY_IN_PROGRESS		7
#define LOAD_NOT_ENOUGH_MEMORY			8
#define LOAD_INITIALIZE_FAILURE			9
#define LOAD_INCONSISTENT_FILE_FORMAT		10
#define LOAD_CAN_NOT_LOAD_AT_STARTUP		11
#define LOAD_AUTO_LOAD_MODULES_NOT_LOADED	12
#define LOAD_UNRESOLVED_EXTERNAL		13
#define LOAD_PUBLIC_ALREADY_DEFINED		14
/****************************************************/

/* The main thread ID.  */
static int mainthread;

/* An error message for the main thread to print.  */
static char *error_message;

/* The AIO port handle.  */
static int AIOhandle;

/* BUFMAX defines the maximum number of characters in inbound/outbound
   buffers.  At least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX (REGISTER_BYTES * 2 + 16)

/* remote_debug > 0 prints ill-formed commands in valid packets and
   checksum errors. */
static int remote_debug = 1;

static const char hexchars[] = "0123456789abcdef";

unsigned char breakpoint_insn[] = BREAKPOINT;

char *mem2hex (void *mem, char *buf, int count, int may_fault);
char *hex2mem (char *buf, void *mem, int count, int may_fault);
extern void set_step_traps (struct StackFrame *);
extern void clear_step_traps (struct StackFrame *);

static int __main() {};

/* Read a character from the serial port.  This must busy wait, but
   that's OK because we will be the only thread running anyhow.  */

static int
getDebugChar (void)
{
  int err;
  LONG got;
  unsigned char ret;

  do
    {
      err = AIOReadData (AIOhandle, (char *) &ret, 1, &got);
      if (err != 0)
	{
	  error_message = "AIOReadData failed";
	  ResumeThread (mainthread);
	  return -1;
	}
    }
  while (got == 0);

  return ret;
}

/* Write a character to the serial port.  Returns 0 on failure,
   non-zero on success.  */

static int
putDebugChar (unsigned char c)
{
  int err;
  LONG put;

  put = 0;
  while (put < 1)
    {
      err = AIOWriteData (AIOhandle, (char *) &c, 1, &put);
      if (err != 0)
	ConsolePrintf ("AIOWriteData: err = %d, put = %d\r\n", err, put);
    }
  return 1;
}

/* Turn a hex character into a number.  */

static int
hex (char ch)
{
  if ((ch >= 'a') && (ch <= 'f'))
    return (ch-'a'+10);
  if ((ch >= '0') && (ch <= '9'))
    return (ch-'0');
  if ((ch >= 'A') && (ch <= 'F'))
    return (ch-'A'+10);
  return (-1);
}

/* Scan for the sequence $<data>#<checksum>.  Returns 0 on failure,
   non-zero on success.  */

static int
getpacket (char *buffer)
{
  unsigned char checksum;
  unsigned char xmitcsum;
  int i;
  int count;
  int ch;

  do
    {
      /* wait around for the start character, ignore all other characters */
      while ((ch = getDebugChar()) != '$')
	if (ch == -1)
	  return 0;
      checksum = 0;
      xmitcsum = -1;

      count = 0;

      /* now, read until a # or end of buffer is found */
      while (count < BUFMAX)
	{
	  ch = getDebugChar();
	  if (ch == -1)
	    return 0;
	  if (ch == '#')
	    break;
	  checksum = checksum + ch;
	  buffer[count] = ch;
	  count = count + 1;
	}
      buffer[count] = 0;

      if (ch == '#')
	{
	  ch = getDebugChar ();
	  if (ch == -1)
	    return 0;
	  xmitcsum = hex(ch) << 4;
	  ch = getDebugChar ();
	  if (ch == -1)
	    return 0;
	  xmitcsum += hex(ch);

	  if (checksum != xmitcsum)
	    {
	      if (remote_debug)
		ConsolePrintf ("bad checksum.  My count = 0x%x, sent=0x%x. buf=%s\n",
			       checksum,xmitcsum,buffer);
	      /* failed checksum */
	      if (! putDebugChar('-'))
		return 0;
	      return 1;
	    }
	  else
	    {
	      /* successful transfer */
	      if (! putDebugChar('+'))
		return 0;
	      /* if a sequence char is present, reply the sequence ID */
	      if (buffer[2] == ':')
		{
		  if (! putDebugChar (buffer[0])
		      || ! putDebugChar (buffer[1]))
		    return 0;
		  /* remove sequence chars from buffer */
		  count = strlen(buffer);
		  for (i=3; i <= count; i++)
		    buffer[i-3] = buffer[i];
		}
	    }
	}
    }
  while (checksum != xmitcsum);

  if (remote_debug)
    ConsolePrintf ("Received packet \"%s\"\r\n", buffer);

  return 1;
}

/* Send the packet in buffer.  Returns 0 on failure, non-zero on
   success.  */

static int
putpacket (char *buffer)
{
  unsigned char checksum;
  int count;
  int ch;

  if (remote_debug)
    ConsolePrintf ("Sending packet \"%s\"\r\n", buffer);

  /*  $<packet info>#<checksum>. */
  do
    {
      if (! putDebugChar('$'))
	return 0;
      checksum = 0;
      count = 0;

      while (ch=buffer[count])
	{
	  if (! putDebugChar(ch))
	    return 0;
	  checksum += ch;
	  count += 1;
	}

      if (! putDebugChar('#')
	  || ! putDebugChar(hexchars[checksum >> 4])
	  || ! putDebugChar(hexchars[checksum % 16]))
	return 0;

      ch = getDebugChar ();
      if (ch == -1)
	return 0;
    }
  while (ch != '+');

  return 1;
}

static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];
static short error;

static void
debug_error (char *format, char *parm)
{
  if (remote_debug)
    {
      ConsolePrintf (format, parm);
      ConsolePrintf ("\n");
    }
}

/* This is set if we could get a memory access fault.  */
static int mem_may_fault;

/* Indicate to caller of mem2hex or hex2mem that there has been an
   error.  */
volatile int mem_err = 0;

#ifndef ALTERNATE_MEM_FUNCS
/* These are separate functions so that they are so short and sweet
   that the compiler won't save any registers (if there is a fault
   to mem_fault, they won't get restored, so there better not be any
   saved).  */

int
get_char (char *addr)
{
  return *addr;
}

void
set_char (char *addr, int val)
{
  *addr = val;
}
#endif /* ALTERNATE_MEM_FUNCS */

/* convert the memory pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
/* If MAY_FAULT is non-zero, then we should set mem_err in response to
   a fault; if zero treat a fault like any other fault in the stub.  */

char *
mem2hex (void *mem, char *buf, int count, int may_fault)
{
  int i;
  unsigned char ch;
  char *ptr = mem;

  mem_may_fault = may_fault;
  for (i = 0; i < count; i++)
    {
      ch = get_char (ptr++);
      if (may_fault && mem_err)
	return (buf);
      *buf++ = hexchars[ch >> 4];
      *buf++ = hexchars[ch % 16];
    }
  *buf = 0;
  mem_may_fault = 0;
  return(buf);
}

/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */

char *
hex2mem (char *buf, void *mem, int count, int may_fault)
{
  int i;
  unsigned char ch;
  char *ptr = mem;

  mem_may_fault = may_fault;
  for (i=0;i<count;i++)
    {
      ch = hex(*buf++) << 4;
      ch = ch + hex(*buf++);
      set_char (ptr++, ch);
      if (may_fault && mem_err)
	return (ptr);
    }
  mem_may_fault = 0;
  return(mem);
}

/* This function takes the 386 exception vector and attempts to
   translate this number into a unix compatible signal value.  */

int
computeSignal (int exceptionVector)
{
  int sigval;
  switch (exceptionVector)
    {
    case 0 : sigval = 8; break; /* divide by zero */
    case 1 : sigval = 5; break; /* debug exception */
    case 3 : sigval = 5; break; /* breakpoint */
    case 4 : sigval = 16; break; /* into instruction (overflow) */
    case 5 : sigval = 16; break; /* bound instruction */
    case 6 : sigval = 4; break; /* Invalid opcode */
    case 7 : sigval = 8; break; /* coprocessor not available */
    case 8 : sigval = 7; break; /* double fault */
    case 9 : sigval = 11; break; /* coprocessor segment overrun */
    case 10 : sigval = 11; break; /* Invalid TSS */
    case 11 : sigval = 11; break; /* Segment not present */
    case 12 : sigval = 11; break; /* stack exception */
    case 13 : sigval = 11; break; /* general protection */
    case 14 : sigval = 11; break; /* page fault */
    case 16 : sigval = 7; break; /* coprocessor error */
    default:
      sigval = 7;		/* "software generated"*/
    }
  return (sigval);
}

/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD AN INT */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/
static int
hexToInt (char **ptr, int *intValue)
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

/* This function does all command processing for interfacing to gdb.
   It is called whenever an exception occurs in the module being
   debugged.  */

static LONG
handle_exception (struct StackFrame *frame)
{
  int addr, length;
  char *ptr;
  static struct DBG_LoadDefinitionStructure *ldinfo = 0;
  static unsigned char first_insn[BREAKPOINT_SIZE]; /* The first instruction in the program.  */

#if 0
  /* According to some documentation from Novell, the bell sometimes
     may be ringing at this point.  This can be stopped on Netware 4
     systems by calling the undocumented StopBell() function. */

  StopBell ();
#endif

  if (remote_debug)
    {
      ConsolePrintf ("vector=%d: %s, pc=%08x, thread=%08x\r\n",
		     frame->ExceptionNumber,
		     frame->ExceptionDescription,
		     frame->ExceptionPC,
		     GetThreadID ());
    }

  switch (frame->ExceptionNumber)
    {
    case START_NLM_EVENT:
      /* If the NLM just started, we record the module load information
	 and the thread ID, and set a breakpoint at the first instruction
	 in the program.  */

      ldinfo = ((struct DBG_LoadDefinitionStructure *)
		frame->ExceptionErrorCode);
      memcpy (first_insn, ldinfo->LDInitializationProcedure,
	      BREAKPOINT_SIZE);
      memcpy (ldinfo->LDInitializationProcedure, breakpoint_insn,
	      BREAKPOINT_SIZE);
      flush_i_cache ();
      return RETURN_TO_PROGRAM;

    case ENTER_DEBUGGER_EVENT:
    case KEYBOARD_BREAK_EVENT:
      /* Pass some events on to the next debugger, in case it will handle
	 them.  */
      return RETURN_TO_NEXT_DEBUGGER;

    case 3:			/* Breakpoint */
      /* After we've reached the initial breakpoint, reset it.  */
      if (frame->ExceptionPC - DECR_PC_AFTER_BREAK == (LONG) ldinfo->LDInitializationProcedure
	  && memcmp (ldinfo->LDInitializationProcedure, breakpoint_insn,
		     BREAKPOINT_SIZE) == 0)
	{
	  memcpy (ldinfo->LDInitializationProcedure, first_insn,
		  BREAKPOINT_SIZE);
	  frame->ExceptionPC -= DECR_PC_AFTER_BREAK;
	  flush_i_cache ();
	}
      /* Normal breakpoints end up here */
      do_status (remcomOutBuffer, frame);
      break;

    default:
      /* At the moment, we don't care about most of the unusual NetWare
	 exceptions.  */
      if (frame->ExceptionNumber > 31)
	return RETURN_TO_PROGRAM;

      /* Most machine level exceptions end up here */
      do_status (remcomOutBuffer, frame);
      break;

    case 11:			/* Segment not present */
    case 13:			/* General protection */
    case 14:			/* Page fault */
      /* If we get a GP fault, and mem_may_fault is set, and the
	 instruction pointer is near set_char or get_char, then we caused
	 the fault ourselves accessing an illegal memory location.  */
      if (mem_may_fault
	  && ((frame->ExceptionPC >= (long) &set_char
	       && frame->ExceptionPC < (long) &set_char + 50)
	      || (frame->ExceptionPC >= (long) &get_char
		  && frame->ExceptionPC < (long) &get_char + 50)))
	{
	  mem_err = 1;
	  /* Point the instruction pointer at an assembly language stub
	     which just returns from the function.  */

	  frame->ExceptionPC += 4; /* Skip the load or store */

	  /* Keep going.  This will act as though it returned from
	     set_char or get_char.  The calling routine will check
	     mem_err, and do the right thing.  */
	  return RETURN_TO_PROGRAM;
	}
      /* Random mem fault, report it */
      do_status (remcomOutBuffer, frame);
      break;

    case TERMINATE_NLM_EVENT:
      /* There is no way to get the exit status.  */
      sprintf (remcomOutBuffer, "W%02x", 0);
      break;			/* We generate our own status */
    }

  /* FIXME: How do we know that this exception has anything to do with
     the program we are debugging?  We can check whether the PC is in
     the range of the module we are debugging, but that doesn't help
     much since an error could occur in a library routine.  */

  clear_step_traps (frame);

  if (! putpacket(remcomOutBuffer))
    return RETURN_TO_NEXT_DEBUGGER;

  if (frame->ExceptionNumber == TERMINATE_NLM_EVENT)
    {
      ResumeThread (mainthread);
      return RETURN_TO_PROGRAM;
    }

  while (1)
    {
      error = 0;
      remcomOutBuffer[0] = 0;
      if (! getpacket (remcomInBuffer))
	return RETURN_TO_NEXT_DEBUGGER;
      switch (remcomInBuffer[0])
	{
	case '?':
	  do_status (remcomOutBuffer, frame);
	  break;
	case 'd':
	  remote_debug = !(remote_debug); /* toggle debug flag */
	  break;
	case 'g':
	  /* return the value of the CPU registers */
	  frame_to_registers (frame, remcomOutBuffer);
	  break;
	case 'G':
	  /* set the value of the CPU registers - return OK */
	  registers_to_frame (&remcomInBuffer[1], frame);
	  strcpy(remcomOutBuffer,"OK");
	  break;

	case 'm':
	  /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
	  /* TRY TO READ %x,%x.  IF SUCCEED, SET PTR = 0 */
	  ptr = &remcomInBuffer[1];
	  if (hexToInt(&ptr,&addr))
	    if (*(ptr++) == ',')
	      if (hexToInt(&ptr,&length))
		{
		  ptr = 0;
		  mem_err = 0;
		  mem2hex((char*) addr, remcomOutBuffer, length, 1);
		  if (mem_err)
		    {
		      strcpy (remcomOutBuffer, "E03");
		      debug_error ("memory fault");
		    }
		}

	  if (ptr)
	    {
	      strcpy(remcomOutBuffer,"E01");
	      debug_error("malformed read memory command: %s",remcomInBuffer);
	    }
	  break;

	case 'M':
	  /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
	  /* TRY TO READ '%x,%x:'.  IF SUCCEED, SET PTR = 0 */
	  ptr = &remcomInBuffer[1];
	  if (hexToInt(&ptr,&addr))
	    if (*(ptr++) == ',')
	      if (hexToInt(&ptr,&length))
		if (*(ptr++) == ':')
		  {
		    mem_err = 0;
		    hex2mem(ptr, (char*) addr, length, 1);

		    if (mem_err)
		      {
			strcpy (remcomOutBuffer, "E03");
			debug_error ("memory fault");
		      }
		    else
		      {
			strcpy(remcomOutBuffer,"OK");
		      }

		    ptr = 0;
		  }
	  if (ptr)
	    {
	      strcpy(remcomOutBuffer,"E02");
	      debug_error("malformed write memory command: %s",remcomInBuffer);
	    }
	  break;

	case 'c':
	case 's':
	  /* cAA..AA    Continue at address AA..AA(optional) */
	  /* sAA..AA   Step one instruction from AA..AA(optional) */
	  /* try to read optional parameter, pc unchanged if no parm */
	  ptr = &remcomInBuffer[1];
	  if (hexToInt(&ptr,&addr))
	    {
/*	      registers[PC_REGNUM].lo = addr;*/
	      fprintf (stderr, "Setting PC to 0x%x\n", addr);
	      while (1);
	    }

	  if (remcomInBuffer[0] == 's')
	    set_step_traps (frame);

	  flush_i_cache ();
	  return RETURN_TO_PROGRAM;

	case 'k':
	  /* kill the program */
	  KillMe (ldinfo);
	  ResumeThread (mainthread);
	  return RETURN_TO_PROGRAM;

	case 'q':		/* Query message */
	  if (strcmp (&remcomInBuffer[1], "Offsets") == 0)
	    {
	      sprintf (remcomOutBuffer, "Text=%x;Data=%x;Bss=%x",
		       ldinfo->LDCodeImageOffset,
		       ldinfo->LDDataImageOffset,
		       ldinfo->LDDataImageOffset + ldinfo->LDDataImageLength);
	    }
	  else
	    sprintf (remcomOutBuffer, "E04, Unknown query %s", &remcomInBuffer[1]);
	  break;
	}

      /* reply to the request */
      if (! putpacket(remcomOutBuffer))
	return RETURN_TO_NEXT_DEBUGGER;
    }
}

char *progname;

struct bitRate {
  BYTE bitRate;
  const char *bitRateString;
};

struct bitRate bitRateTable[] = 
{
  { AIO_BAUD_50    ,      "50" },
  { AIO_BAUD_75    ,      "75" },
  { AIO_BAUD_110   ,     "110" },
  { AIO_BAUD_134p5 ,   "134.5" },
  { AIO_BAUD_150   ,     "150" },
  { AIO_BAUD_300   ,     "300" },
  { AIO_BAUD_600   ,     "600" },
  { AIO_BAUD_1200  ,    "1200" },
  { AIO_BAUD_1800  ,    "1800" },
  { AIO_BAUD_2000  ,    "2000" },
  { AIO_BAUD_2400  ,    "2400" },
  { AIO_BAUD_3600  ,    "3600" },
  { AIO_BAUD_4800  ,    "4800" },
  { AIO_BAUD_7200  ,    "7200" },
  { AIO_BAUD_9600  ,    "9600" },
  { AIO_BAUD_19200 ,   "19200" },
  { AIO_BAUD_38400 ,   "38400" },
  { AIO_BAUD_57600 ,   "57600" },
  { AIO_BAUD_115200,  "115200" },
  { -1, NULL }
};

char dataBitsTable[] = "5678";

char *stopBitsTable[] = { "1", "1.5", "2" };

char parity[] = "NOEMS";

/* Start up.  The main thread opens the named serial I/O port, loads
   the named NLM module and then goes to sleep.  The serial I/O port
   is named as a board number and a port number.  It would be more DOS
   like to provide a menu of available serial ports, but I don't want
   to have to figure out how to do that.  */

int
main (int argc, char **argv)
{
  int hardware, board, port;
  BYTE bitRate;
  BYTE dataBits;
  BYTE stopBits;
  BYTE parityMode;
  LONG err;
  struct debuggerStructure s;
  int cmdindx;
  char *cmdlin;
  int i;

  /* set progname */
  progname = "gdbserve";

  /* set default serial line */
  hardware = -1;
  board = 0;
  port = 0;

  /* set default serial line characteristics */
  bitRate  = AIO_BAUD_9600;
  dataBits = AIO_DATA_BITS_8;
  stopBits = AIO_STOP_BITS_1;
  parityMode = AIO_PARITY_NONE;

  cmdindx = 0;
  for (argc--, argv++; *argv; argc--, argv++) 
    {
      char *bp;
      char *ep;

      if (strnicmp(*argv, "BAUD=", 5) == 0) 
	{
	  struct bitRate *brp;

	  bp = *argv + 5;
	  for (brp = bitRateTable; brp->bitRate != (BYTE) -1; brp++) 
	    {
	      if (strcmp(brp->bitRateString, bp) == 0) 
		{
		  bitRate = brp->bitRate;
		  break;
		}
	    }

	  if (brp->bitRateString == NULL) 
	    {
	      fprintf(stderr, "%s: %s: unknown or unsupported bit rate",
		      progname, bp);
	      exit (1);
	    }
	}
      else if (strnicmp(*argv, "BOARD=", 6) == 0) 
        {
	  bp = *argv + 6;
	  board = strtol (bp, &ep, 0);
	  if (ep == bp || *ep != '\0') 
	    {
	      fprintf (stderr, "%s: %s: expected integer argument\n", 
		       progname, bp);
	      exit(1);
	    }
	}
#if 1				/* FIXME: this option has been depricated */
      else if (strnicmp(*argv, "NODE=", 5) == 0)
	{
	  bp = *argv + 5;
	  board = strtol (bp, &ep, 0);
	  if (ep == bp || *ep != '\0') 
	    {
	      fprintf (stderr, "%s: %s: expected integer argument\n", 
		       progname, bp);
	      exit(1);
	    }
	}
#endif
      else if (strnicmp(*argv, "PORT=", 5) == 0)
	{
	  bp = *argv + 5;
	  port = strtol (bp, &ep, 0);
	  if (ep == bp || *ep != '\0')
	    {
	      fprintf (stderr, "%s: %s: expected integer argument\n", 
		       progname, bp);
	      exit(1);
	    }
	}
      else
	{
	  break;
	}

      cmdindx++;
    }

  if (argc == 0)
    {
      fprintf (stderr,
	       "Usage: load %s [options] program [arguments]\n", progname);
      exit (1);
    }

  err = AIOAcquirePort (&hardware, &board, &port, &AIOhandle);
  if (err != AIO_SUCCESS)
    {
      switch (err)
	{
	case AIO_PORT_NOT_AVAILABLE:
	  fprintf (stderr, "Port not available\n");
	  break;

	case AIO_BOARD_NUMBER_INVALID:
	case AIO_PORT_NUMBER_INVALID:
	  fprintf (stderr, "No such port\n");
	  break;

	default:
	  fprintf (stderr, "Could not open port: %d\n", err);
	  break;
	}

      exit (1);
    }

  err = AIOConfigurePort (AIOhandle, bitRate, dataBits, stopBits, parityMode,
			  AIO_HARDWARE_FLOW_CONTROL_OFF);

  if (err == AIO_QUALIFIED_SUCCESS)
    {
      AIOPORTCONFIG portConfig;

      fprintf (stderr, "Port configuration changed!\n");

      portConfig.returnLength = sizeof(portConfig);
      AIOGetPortConfiguration (AIOhandle, &portConfig, NULL);

      fprintf (stderr,
	       "  Bit Rate: %s, Data Bits: %c, Stop Bits: %s, Parity: %c,\
 Flow:%s\n",
	       bitRateTable[portConfig.bitRate].bitRateString,
	       dataBitsTable[portConfig.dataBits],
	       stopBitsTable[portConfig.stopBits],
	       parity[portConfig.parityMode],
	       portConfig.flowCtrlMode ? "ON" : "OFF");
    }
  else if (err != AIO_SUCCESS)
    {
      fprintf (stderr, "Could not configure port: %d\n", err);
      AIOReleasePort (AIOhandle);
      exit (1);
    }

  if (AIOSetExternalControl(AIOhandle, AIO_EXTERNAL_CONTROL,
			    (AIO_EXTCTRL_DTR | AIO_EXTCTRL_RTS))
      != AIO_SUCCESS)
    {
      LONG extStatus, chgdExtStatus;

      fprintf (stderr, "Could not set desired port controls!\n");
      AIOGetExternalStatus (AIOhandle, &extStatus, &chgdExtStatus);
      fprintf (stderr, "Port controls now: %d, %d\n", extStatus, 
	       chgdExtStatus);
    }

  /* Register ourselves as an alternate debugger.  */
  memset (&s, 0, sizeof s);
  s.DDSResourceTag = ((struct ResourceTagStructure *)
		      AllocateResourceTag (GetNLMHandle (),
					   (BYTE *)"gdbserver",
					   DebuggerSignature));
  if (s.DDSResourceTag == 0)
    {
      fprintf (stderr, "AllocateResourceTag failed\n");
      AIOReleasePort (AIOhandle);
      exit (1);
    }
  s.DDSdebuggerEntry = handle_exception;
  s.DDSFlags = TSS_FRAME_BIT;

  err = RegisterDebuggerRTag (&s, AT_FIRST);
  if (err != 0)
    {
      fprintf (stderr, "RegisterDebuggerRTag failed\n");
      AIOReleasePort (AIOhandle);
      exit (1);
    }

  /* Get the command line we were invoked with, and advance it past
     our name and the board and port arguments.  */
  cmdlin = getcmd ((char *) NULL);
  for (i = 0; i < cmdindx; i++)
    {
      while (! isspace (*cmdlin))
	++cmdlin;
      while (isspace (*cmdlin))
	++cmdlin;
    }
  
  /* In case GDB is started before us, ack any packets (presumably
     "$?#xx") sitting there.  */
  if (! putDebugChar ('+'))
    {
      fprintf (stderr, "putDebugChar failed\n");
      UnRegisterDebugger (&s);
      AIOReleasePort (AIOhandle);
      exit (1);
    }

  mainthread = GetThreadID ();

  if (remote_debug > 0)
    ConsolePrintf ("About to call LoadModule with \"%s\" %08x\r\n",
		   cmdlin, __GetScreenID (GetCurrentScreen()));

  /* Start up the module to be debugged.  */
  err = LoadModule ((struct ScreenStruct *) __GetScreenID (GetCurrentScreen()),
		    (BYTE *)cmdlin, LO_DEBUG);
  if (err != 0)
    {
      fprintf (stderr, "LoadModule failed: %d\n", err);
      UnRegisterDebugger (&s);
      AIOReleasePort (AIOhandle);
      exit (1);
    }

  /* Wait for the debugger to wake us up.  */
  if (remote_debug > 0)
    ConsolePrintf ("Suspending main thread (%08x)\r\n", mainthread);
  SuspendThread (mainthread);
  if (remote_debug > 0)
    ConsolePrintf ("Resuming main thread (%08x)\r\n", mainthread);

  /* If we are woken up, print an optional error message, deregister
     ourselves and exit.  */
  if (error_message != NULL)
    fprintf (stderr, "%s\n", error_message);
  UnRegisterDebugger (&s);
  AIOReleasePort (AIOhandle);
  exit (0);
}
