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
 *  Modified for M32R by Michael Snyder, Cygnus Support.
 *
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a trap #1.
 *
 *  The external function exceptionHandler() is
 *  used to attach a specific handler to a specific M32R vector number.
 *  It should use the same privilege level it runs at.  It should
 *  install it as an interrupt gate so that interrupts are masked
 *  while the handler runs.
 *
 *  Because gdb will sometimes write to the stack area to execute function
 *  calls, this program cannot rely on using the supervisor stack so it
 *  uses it's own stack area reserved in the int array remcomStack.
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
 *    XAA..AA,LLLL: Write LLLL binary bytes at address     OK or ENN
 *                  AA..AA
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
 * <checksum>    :: <two hex digits computed as modulo 256 sum of <packetinfo>>
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


/************************************************************************
 *
 * external low-level support routines
 */
extern void putDebugChar();	/* write a single character      */
extern int getDebugChar();	/* read and return a single char */
extern void exceptionHandler();	/* assign an exception handler   */

/*****************************************************************************
 * BUFMAX defines the maximum number of characters in inbound/outbound buffers
 * at least NUMREGBYTES*2 are needed for register packets 
 */
#define BUFMAX 400

static char initialized;  /* boolean flag. != 0 means we've been initialized */

int     remote_debug;
/*  debug >  0 prints ill-formed commands in valid packets & checksum errors */

static const unsigned char hexchars[]="0123456789abcdef";

#define NUMREGS 24

/* Number of bytes of registers.  */
#define NUMREGBYTES (NUMREGS * 4)
enum regnames { R0,  R1,  R2,  R3,  R4,  R5,  R6,   R7,
		R8,  R9,  R10, R11, R12, R13, R14,  R15,
		PSW, CBR, SPI, SPU, BPC, PC,  ACCL, ACCH };

enum SYS_calls {
  	SYS_null, 
	SYS_exit,
	SYS_open,
	SYS_close,
	SYS_read,
	SYS_write,
	SYS_lseek,
	SYS_unlink,
	SYS_getpid,
	SYS_kill,
	SYS_fstat,
	SYS_sbrk,
	SYS_fork,
	SYS_execve,
	SYS_wait4,
	SYS_link,
	SYS_chdir,
	SYS_stat,
	SYS_utime,
	SYS_chown,
	SYS_chmod,
	SYS_time,
	SYS_pipe };

static int registers[NUMREGS];

#define STACKSIZE 8096
static unsigned char remcomInBuffer[BUFMAX];
static unsigned char remcomOutBuffer[BUFMAX];
static int  remcomStack[STACKSIZE/sizeof(int)];
static int*  stackPtr = &remcomStack[STACKSIZE/sizeof(int) - 1];

static unsigned int save_vectors[18];	/* previous exception vectors */

/* Indicate to caller of mem2hex or hex2mem that there has been an error. */
static volatile int mem_err = 0;

/* Store the vector number here (since GDB only gets the signal
   number through the usual means, and that's not very specific).  */
int gdb_m32r_vector = -1;

#if 0
#include "syscall.h" /* for SYS_exit, SYS_write etc. */
#endif

/* Global entry points:
 */

extern void handle_exception(int);
extern void set_debug_traps(void);
extern void breakpoint(void);

/* Local functions:
 */

static int  computeSignal(int);
static void putpacket(unsigned char *);
static unsigned char *getpacket(void);

static unsigned char *mem2hex(unsigned char *, unsigned char *, int, int);
static unsigned char *hex2mem(unsigned char *, unsigned char *, int, int);
static int  hexToInt(unsigned char **, int *);
static unsigned char *bin2mem(unsigned char *, unsigned char *, int, int);
static void stash_registers(void);
static void restore_registers(void);
static int  prepare_to_step(int);
static int  finish_from_step(void);
static unsigned long crc32 (unsigned char *, int, unsigned long);

static void gdb_error(char *, char *);
static int  gdb_putchar(int), gdb_puts(char *), gdb_write(char *, int);

static unsigned char *strcpy (unsigned char *, const unsigned char *);
static int   strlen (const unsigned char *);

/*
 * This function does all command procesing for interfacing to gdb.
 */

void 
handle_exception(int exceptionVector)
{
  int    sigval, stepping;
  int    addr, length, i;
  unsigned char * ptr;
  unsigned char   buf[16];
  int binary;

  if (!finish_from_step())
    return;		/* "false step": let the target continue */

  gdb_m32r_vector = exceptionVector;

  if (remote_debug)
    {
      mem2hex((unsigned char *) &exceptionVector, buf, 4, 0);
      gdb_error("Handle exception %s, ", buf);
      mem2hex((unsigned char *) &registers[PC], buf, 4, 0);
      gdb_error("PC == 0x%s\n", buf);
    }

  /* reply to host that an exception has occurred */
  sigval = computeSignal( exceptionVector );

  ptr = remcomOutBuffer;
 
  *ptr++ = 'T';         /* notify gdb with signo, PC, FP and SP */
  *ptr++ = hexchars[sigval >> 4];
  *ptr++ = hexchars[sigval & 0xf];
 
  *ptr++ = hexchars[PC >> 4];
  *ptr++ = hexchars[PC & 0xf];
  *ptr++ = ':';
  ptr = mem2hex((unsigned char *)&registers[PC], ptr, 4, 0);     /* PC */
  *ptr++ = ';';
 
  *ptr++ = hexchars[R13 >> 4];
  *ptr++ = hexchars[R13 & 0xf];
  *ptr++ = ':';
  ptr = mem2hex((unsigned char *)&registers[R13], ptr, 4, 0);    /* FP */
  *ptr++ = ';';
 
  *ptr++ = hexchars[R15 >> 4];
  *ptr++ = hexchars[R15 & 0xf];
  *ptr++ = ':';
  ptr = mem2hex((unsigned char *)&registers[R15], ptr, 4, 0);    /* SP */
  *ptr++ = ';';
  *ptr++ = 0;
 
  if (exceptionVector == 0)     /* simulated SYS call stuff */
    {
      mem2hex((unsigned char *) &registers[PC], buf, 4, 0);
      switch (registers[R0]) {
      case SYS_exit:
	gdb_error("Target program has exited at %s\n", buf);
	ptr = remcomOutBuffer;
	*ptr++ = 'W';
	sigval = registers[R1] & 0xff;
	*ptr++ = hexchars[sigval >> 4];
	*ptr++ = hexchars[sigval & 0xf];
	*ptr++ = 0;
	break;
      case SYS_open:
	gdb_error("Target attempts SYS_open call at %s\n", buf);
	break;
      case SYS_close:
	gdb_error("Target attempts SYS_close call at %s\n", buf);
	break;
      case SYS_read:
	gdb_error("Target attempts SYS_read call at %s\n", buf);
	break;
      case SYS_write:
	if (registers[R1] == 1 ||       /* write to stdout  */
	    registers[R1] == 2)		/* write to stderr  */
	  {				/* (we can do that) */
	    registers[R0] = gdb_write((void *) registers[R2], registers[R3]);
	    return;
	  }
	else
	  gdb_error("Target attempts SYS_write call at %s\n", buf);
	break;
      case SYS_lseek:
	gdb_error("Target attempts SYS_lseek call at %s\n", buf);
	break;
      case SYS_unlink:
	gdb_error("Target attempts SYS_unlink call at %s\n", buf);
	break;
      case SYS_getpid:
	gdb_error("Target attempts SYS_getpid call at %s\n", buf);
	break;
      case SYS_kill:
	gdb_error("Target attempts SYS_kill call at %s\n", buf);
	break;
      case SYS_fstat:
	gdb_error("Target attempts SYS_fstat call at %s\n", buf);
	break;
      default:
	gdb_error("Target attempts unknown SYS call at %s\n", buf);
	break;
      }
    }

  putpacket(remcomOutBuffer);

  stepping = 0;

  while (1==1) {
    remcomOutBuffer[0] = 0;
    ptr = getpacket();
    binary = 0;
    switch (*ptr++) {
      default:	/* Unknown code.  Return an empty reply message. */
	break;
      case 'R':
	if (hexToInt (&ptr, &addr))
	  registers[PC] = addr;
	strcpy(remcomOutBuffer, "OK");
	break;
      case '!':
	strcpy(remcomOutBuffer, "OK");
	break;
    case 'X': /* XAA..AA,LLLL:<binary data>#cs */
      binary = 1;
    case 'M': /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
      /* TRY TO READ '%x,%x:'.  IF SUCCEED, SET PTR = 0 */
      {
        if (hexToInt(&ptr,&addr))
          if (*(ptr++) == ',')
            if (hexToInt(&ptr,&length))
              if (*(ptr++) == ':')
                {
                  mem_err = 0;
                  if (binary)
                    bin2mem (ptr, (unsigned char *) addr, length, 1);
                  else
                    hex2mem(ptr, (unsigned char*) addr, length, 1);
                  if (mem_err) {
                    strcpy (remcomOutBuffer, "E03");
                    gdb_error ("memory fault", "");
                  } else {
                    strcpy(remcomOutBuffer,"OK");
                  }
                  ptr = 0;
                }
        if (ptr)
          {
            strcpy(remcomOutBuffer,"E02");
          }
      }
	break;
      case 'm': /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
		/* TRY TO READ %x,%x.  IF SUCCEED, SET PTR = 0 */
	if (hexToInt(&ptr,&addr))
	  if (*(ptr++) == ',')
	    if (hexToInt(&ptr,&length))
	      {
		ptr = 0;
		mem_err = 0;
		mem2hex((unsigned char*) addr, remcomOutBuffer, length, 1);
		if (mem_err) {
		  strcpy (remcomOutBuffer, "E03");
		  gdb_error ("memory fault", "");
		}
	      }
	if (ptr)
	  {
	    strcpy(remcomOutBuffer,"E01");
	  }
	break;
      case '?': 
	remcomOutBuffer[0] = 'S';
	remcomOutBuffer[1] =  hexchars[sigval >> 4];
	remcomOutBuffer[2] =  hexchars[sigval % 16];
	remcomOutBuffer[3] = 0;
	break;
      case 'd': 
	remote_debug = !(remote_debug);  /* toggle debug flag */
	break;
      case 'g': /* return the value of the CPU registers */
	mem2hex((unsigned char*) registers, remcomOutBuffer, NUMREGBYTES, 0);
	break;
      case 'P': /* set the value of a single CPU register - return OK */
	{
	  int regno;

	  if (hexToInt (&ptr, &regno) && *ptr++ == '=')
	    if (regno >= 0 && regno < NUMREGS)
	      {
		int stackmode;

		hex2mem (ptr, (unsigned char *) &registers[regno], 4, 0);
		/*
		 * Since we just changed a single CPU register, let's
		 * make sure to keep the several stack pointers consistant.
		 */
		stackmode = registers[PSW] & 0x80;
		if (regno == R15)	/* stack pointer changed */
		  {			/* need to change SPI or SPU */
		    if (stackmode == 0)
		      registers[SPI] = registers[R15];
		    else
		      registers[SPU] = registers[R15];
		  }
		else if (regno == SPU)	/* "user" stack pointer changed */
		  {
		    if (stackmode != 0)	/* stack in user mode: copy SP */
		      registers[R15] = registers[SPU];
		  }
		else if (regno == SPI)	/* "interrupt" stack pointer changed */
		  {
		    if (stackmode == 0)	/* stack in interrupt mode: copy SP */
		      registers[R15] = registers[SPI];
		  }
		else if (regno == PSW)	/* stack mode may have changed! */
		  {			/* force SP to either SPU or SPI */
		    if (stackmode == 0)	/* stack in user mode */
		      registers[R15] = registers[SPI];
		    else		/* stack in interrupt mode */
		      registers[R15] = registers[SPU];
		  }
		strcpy (remcomOutBuffer, "OK");
		break;
	      }
	  strcpy (remcomOutBuffer, "E01");
	  break;
	}
      case 'G': /* set the value of the CPU registers - return OK */
	hex2mem(ptr, (unsigned char*) registers, NUMREGBYTES, 0);
	strcpy(remcomOutBuffer,"OK");
	break;
      case 's': /* sAA..AA	Step one instruction from AA..AA(optional) */
	stepping = 1;
      case 'c': /* cAA..AA	Continue from address AA..AA(optional) */
		/* try to read optional parameter, pc unchanged if no parm */
	if (hexToInt(&ptr,&addr))
	  registers[ PC ] = addr;
	
	if (stepping)	/* single-stepping */
	  {
	    if (!prepare_to_step(0))	/* set up for single-step */
	      {
		/* prepare_to_step has already emulated the target insn:
		   Send SIGTRAP to gdb, don't resume the target at all.  */
		ptr = remcomOutBuffer;
		*ptr++ = 'T';           /* Simulate stopping with SIGTRAP */
		*ptr++ = '0';
		*ptr++ = '5';

		*ptr++ = hexchars[PC >> 4];     /* send PC */
		*ptr++ = hexchars[PC & 0xf];
		*ptr++ = ':';
		ptr = mem2hex((unsigned char *)&registers[PC], ptr, 4, 0);
		*ptr++ = ';';

		*ptr++ = hexchars[R13 >> 4];    /* send FP */
		*ptr++ = hexchars[R13 & 0xf];
		*ptr++ = ':';
		ptr = mem2hex((unsigned char *)&registers[R13], ptr, 4, 0);
		*ptr++ = ';';

		*ptr++ = hexchars[R15 >> 4];    /* send SP */
		*ptr++ = hexchars[R15 & 0xf];
		*ptr++ = ':';
		ptr = mem2hex((unsigned char *)&registers[R15], ptr, 4, 0);
		*ptr++ = ';';
		*ptr++ = 0;

		break;	
	      }
	  }
	else	/* continuing, not single-stepping */
	  {
	    /* OK, about to do a "continue".  First check to see if the 
	       target pc is on an odd boundary (second instruction in the 
	       word).  If so, we must do a single-step first, because 
	       ya can't jump or return back to an odd boundary!  */
	    if ((registers[PC] & 2) != 0)
	      prepare_to_step(1);
	  }

	return;

      case 'D':	/* Detach */
#if 0
	/* I am interpreting this to mean, release the board from control 
	   by the remote stub.  To do this, I am restoring the original
	   (or at least previous) exception vectors.
	 */
	for (i = 0; i < 18; i++)
	  exceptionHandler (i, save_vectors[i]);
	putpacket ("OK");
	return;		/* continue the inferior */
#else
	strcpy(remcomOutBuffer,"OK");
	break;
#endif
    case 'q':
      if (*ptr++ == 'C' &&
	  *ptr++ == 'R' &&
	  *ptr++ == 'C' &&
	  *ptr++ == ':')
	{
	  unsigned long start, len, our_crc;

	  if (hexToInt (&ptr, (int *) &start) &&
	      *ptr++ == ','                   &&
	      hexToInt (&ptr, (int *) &len))
	    {
	      remcomOutBuffer[0] = 'C';
	      our_crc = crc32 ((unsigned char *) start, len, 0xffffffff);
	      mem2hex ((char *) &our_crc, 
		       &remcomOutBuffer[1], 
		       sizeof (long), 
		       0); 
	    } /* else do nothing */
	} /* else do nothing */
      break;

      case 'k': /* kill the program */
	continue;
      } /* switch */

    /* reply to the request */
    putpacket(remcomOutBuffer);
  }
}

/* qCRC support */

/* Table used by the crc32 function to calcuate the checksum. */
static unsigned long crc32_table[256] = {0, 0};

static unsigned long
crc32 (unsigned char *buf, int len, unsigned long crc)
{
  if (! crc32_table[1])
    {
      /* Initialize the CRC table and the decoding table. */
      int i, j;
      unsigned long c;

      for (i = 0; i < 256; i++)
	{
	  for (c = i << 24, j = 8; j > 0; --j)
	    c = c & 0x80000000 ? (c << 1) ^ 0x04c11db7 : (c << 1);
	  crc32_table[i] = c;
	}
    }

  while (len--)
    {
      crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *buf) & 255];
      buf++;
    }
  return crc;
}

static int 
hex (unsigned char ch)
{
  if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
  if ((ch >= '0') && (ch <= '9')) return (ch-'0');
  if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
  return (-1);
}

/* scan for the sequence $<data>#<checksum>     */

unsigned char *
getpacket (void)
{
  unsigned char *buffer = &remcomInBuffer[0];
  unsigned char checksum;
  unsigned char xmitcsum;
  int count;
  char ch;

  while (1)
    {
      /* wait around for the start character, ignore all other characters */
      while ((ch = getDebugChar ()) != '$')
	;

retry:
      checksum = 0;
      xmitcsum = -1;
      count = 0;

      /* now, read until a # or end of buffer is found */
      while (count < BUFMAX)
	{
	  ch = getDebugChar ();
          if (ch == '$')
	    goto retry;
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
	  xmitcsum = hex (ch) << 4;
	  ch = getDebugChar ();
	  xmitcsum += hex (ch);

	  if (checksum != xmitcsum)
	    {
	      if (remote_debug)
		{
		  unsigned char buf[16];

		  mem2hex((unsigned char *) &checksum, buf, 4, 0);
		  gdb_error("Bad checksum: my count = %s, ", buf);
		  mem2hex((unsigned char *) &xmitcsum, buf, 4, 0);
		  gdb_error("sent count = %s\n", buf);
		  gdb_error(" -- Bad buffer: \"%s\"\n", buffer); 
		}
	      putDebugChar ('-');	/* failed checksum */
	    }
	  else
	    {
	      putDebugChar ('+');	/* successful transfer */

	      /* if a sequence char is present, reply the sequence ID */
	      if (buffer[2] == ':')
		{
		  putDebugChar (buffer[0]);
		  putDebugChar (buffer[1]);

		  return &buffer[3];
		}

	      return &buffer[0];
	    }
	}
    }
}

/* send the packet in buffer.  */

static void 
putpacket (unsigned char *buffer)
{
  unsigned char checksum;
  int  count;
  char ch;

  /*  $<packet info>#<checksum>. */
  do {
    putDebugChar('$');
    checksum = 0;
    count    = 0;

    while (ch=buffer[count]) {
      putDebugChar(ch);
      checksum += ch;
      count += 1;
    }
    putDebugChar('#');
    putDebugChar(hexchars[checksum >> 4]);
    putDebugChar(hexchars[checksum % 16]);
  } while (getDebugChar() != '+');
}

/* Address of a routine to RTE to if we get a memory fault.  */

static void (*volatile mem_fault_routine)() = 0;

static void
set_mem_err (void)
{
  mem_err = 1;
}

/* Check the address for safe access ranges.  As currently defined,
   this routine will reject the "expansion bus" address range(s).
   To make those ranges useable, someone must implement code to detect
   whether there's anything connected to the expansion bus. */

static int
mem_safe (unsigned char *addr)
{
#define BAD_RANGE_ONE_START	((unsigned char *) 0x600000)
#define BAD_RANGE_ONE_END	((unsigned char *) 0xa00000)
#define BAD_RANGE_TWO_START	((unsigned char *) 0xff680000)
#define BAD_RANGE_TWO_END	((unsigned char *) 0xff800000)

  if (addr < BAD_RANGE_ONE_START)	return 1;	/* safe */
  if (addr < BAD_RANGE_ONE_END)		return 0;	/* unsafe */
  if (addr < BAD_RANGE_TWO_START)	return 1;	/* safe */
  if (addr < BAD_RANGE_TWO_END)		return 0;	/* unsafe */
}

/* These are separate functions so that they are so short and sweet
   that the compiler won't save any registers (if there is a fault
   to mem_fault, they won't get restored, so there better not be any
   saved).  */
static int
get_char (unsigned char *addr)
{
#if 1
  if (mem_fault_routine && !mem_safe(addr))
    {
      mem_fault_routine ();
      return 0;
    }
#endif
  return *addr;
}

static void
set_char (unsigned char *addr, unsigned char val)
{
#if 1
  if (mem_fault_routine && !mem_safe (addr))
    {
      mem_fault_routine ();
      return;
    }
#endif
  *addr = val;
}

/* Convert the memory pointed to by mem into hex, placing result in buf.
   Return a pointer to the last char put in buf (null).
   If MAY_FAULT is non-zero, then we should set mem_err in response to
   a fault; if zero treat a fault like any other fault in the stub.  */

static unsigned char *
mem2hex (unsigned char *mem, unsigned char *buf, int count, int may_fault)
{
  int i;
  unsigned char ch;

  if (may_fault)
    mem_fault_routine = set_mem_err;
  for (i=0;i<count;i++) {
    ch = get_char (mem++);
    if (may_fault && mem_err)
      return (buf);
    *buf++ = hexchars[ch >> 4];
    *buf++ = hexchars[ch % 16];
  }
  *buf = 0;
  if (may_fault)
    mem_fault_routine = 0;
  return(buf);
}

/* Convert the hex array pointed to by buf into binary to be placed in mem.
   Return a pointer to the character AFTER the last byte written. */

static unsigned char* 
hex2mem (unsigned char *buf, unsigned char *mem, int count, int may_fault)
{
  int i;
  unsigned char ch;

  if (may_fault)
    mem_fault_routine = set_mem_err;
  for (i=0;i<count;i++) {
    ch = hex(*buf++) << 4;
    ch = ch + hex(*buf++);
    set_char (mem++, ch);
    if (may_fault && mem_err)
      return (mem);
  }
  if (may_fault)
    mem_fault_routine = 0;
  return(mem);
}

/* Convert the binary stream in BUF to memory.

   Gdb will escape $, #, and the escape char (0x7d).
   COUNT is the total number of bytes to write into
   memory. */
static unsigned char *
bin2mem (unsigned char *buf, unsigned char *mem, int count, int may_fault)
{
  int i;
  unsigned char ch;

  if (may_fault)
    mem_fault_routine = set_mem_err;
  for (i = 0; i < count; i++)
    {
      /* Check for any escaped characters. Be paranoid and
         only unescape chars that should be escaped. */
      if (*buf == 0x7d)
        {
          switch (*(buf+1))
            {
            case 0x3:  /* # */
            case 0x4:  /* $ */
            case 0x5d: /* escape char */
              buf++;
              *buf |= 0x20;
              break;
            default:
              /* nothing */
              break;
            }
        }

      set_char (mem++, *buf++);

      if (may_fault && mem_err)
        return mem;
    }

  if (may_fault)
    mem_fault_routine = 0;
  return mem;
}

/* this function takes the m32r exception vector and attempts to
   translate this number into a unix compatible signal value */

static int 
computeSignal (int exceptionVector)
{
  int sigval;
  switch (exceptionVector) {
    case 0  : sigval = 23; break; /* I/O trap                    */
    case 1  : sigval = 5;  break; /* breakpoint                  */
    case 2  : sigval = 5;  break; /* breakpoint                  */
    case 3  : sigval = 5;  break; /* breakpoint                  */
    case 4  : sigval = 5;  break; /* breakpoint                  */
    case 5  : sigval = 5;  break; /* breakpoint                  */
    case 6  : sigval = 5;  break; /* breakpoint                  */
    case 7  : sigval = 5;  break; /* breakpoint                  */
    case 8  : sigval = 5;  break; /* breakpoint                  */
    case 9  : sigval = 5;  break; /* breakpoint                  */
    case 10 : sigval = 5;  break; /* breakpoint                  */
    case 11 : sigval = 5;  break; /* breakpoint                  */
    case 12 : sigval = 5;  break; /* breakpoint                  */
    case 13 : sigval = 5;  break; /* breakpoint                  */
    case 14 : sigval = 5;  break; /* breakpoint                  */
    case 15 : sigval = 5;  break; /* breakpoint                  */
    case 16 : sigval = 10; break; /* BUS ERROR (alignment)       */
    case 17 : sigval = 2;  break; /* INTerrupt                   */
    default : sigval = 7;  break; /* "software generated"        */
  }
  return (sigval);
}

/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD AN INT */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/
static int 
hexToInt (unsigned char **ptr, int *intValue)
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

/*
  Table of branch instructions:
  
  10B6		RTE	return from trap or exception
  1FCr		JMP	jump
  1ECr		JL	jump and link
  7Fxx		BRA	branch
  FFxxxxxx	BRA	branch (long)
  B09rxxxx	BNEZ	branch not-equal-zero
  Br1rxxxx	BNE	branch not-equal
  7Dxx		BNC	branch not-condition
  FDxxxxxx	BNC	branch not-condition (long)
  B0Arxxxx	BLTZ	branch less-than-zero
  B0Crxxxx	BLEZ	branch less-equal-zero
  7Exx		BL	branch and link
  FExxxxxx	BL	branch and link (long)
  B0Drxxxx	BGTZ	branch greater-than-zero
  B0Brxxxx	BGEZ	branch greater-equal-zero
  B08rxxxx	BEQZ	branch equal-zero
  Br0rxxxx	BEQ	branch equal
  7Cxx		BC	branch condition
  FCxxxxxx	BC	branch condition (long)
  */

static int 
isShortBranch (unsigned char *instr)
{
  unsigned char instr0 = instr[0] & 0x7F;		/* mask off high bit */

  if (instr0 == 0x10 && instr[1] == 0xB6)	/* RTE */
    return 1;		/* return from trap or exception */

  if (instr0 == 0x1E || instr0 == 0x1F)		/* JL or JMP */
    if ((instr[1] & 0xF0) == 0xC0)
      return 2;					/* jump thru a register */

  if (instr0 == 0x7C || instr0 == 0x7D || 	/* BC, BNC, BL, BRA */
      instr0 == 0x7E || instr0 == 0x7F)
    return 3;					/* eight bit PC offset */

  return 0;
}

static int
isLongBranch (unsigned char *instr)
{
  if (instr[0] == 0xFC || instr[0] == 0xFD ||	/* BRA, BNC, BL, BC */
      instr[0] == 0xFE || instr[0] == 0xFF)	/* 24 bit relative */
    return 4;
  if ((instr[0] & 0xF0) == 0xB0)		/* 16 bit relative */
    {
      if ((instr[1] & 0xF0) == 0x00 || 		/* BNE, BEQ */
	  (instr[1] & 0xF0) == 0x10)
	return 5;
      if (instr[0] == 0xB0)	/* BNEZ, BLTZ, BLEZ, BGTZ, BGEZ, BEQZ */
	if ((instr[1] & 0xF0) == 0x80 || (instr[1] & 0xF0) == 0x90 || 
	    (instr[1] & 0xF0) == 0xA0 || (instr[1] & 0xF0) == 0xB0 ||
	    (instr[1] & 0xF0) == 0xC0 || (instr[1] & 0xF0) == 0xD0)
	  return 6;
    }
  return 0;
}

/* if address is NOT on a 4-byte boundary, or high-bit of instr is zero, 
   then it's a 2-byte instruction, else it's a 4-byte instruction.  */

#define INSTRUCTION_SIZE(addr) \
    ((((int) addr & 2) || (((unsigned char *) addr)[0] & 0x80) == 0) ? 2 : 4)

static int
isBranch (unsigned char *instr)
{
  if (INSTRUCTION_SIZE(instr) == 2)
    return isShortBranch(instr);
  else
    return isLongBranch(instr);
}

static int
willBranch (unsigned char *instr, int branchCode)
{
  switch (branchCode) 
    {
    case 0:	return 0;	/* not a branch */
    case 1:	return 1;	/* RTE */
    case 2:	return 1;	/* JL or JMP    */
    case 3:			/* BC, BNC, BL, BRA (short) */
    case 4:			/* BC, BNC, BL, BRA (long) */
      switch (instr[0] & 0x0F) 
	{
	case 0xC:		/* Branch if Condition Register */
	  return (registers[CBR] != 0);
	case 0xD:		/* Branch if NOT Condition Register */
	  return (registers[CBR] == 0);
	case 0xE:		/* Branch and Link */
	case 0xF:		/* Branch (unconditional) */
	  return 1;
	default:		/* oops? */
	  return 0;
	}
    case 5: 			/* BNE, BEQ */
      switch (instr[1] & 0xF0) 
	{
	case 0x00:		/* Branch if r1 equal to r2 */
	  return (registers[instr[0] & 0x0F] == registers[instr[1] & 0x0F]);
	case 0x10:		/* Branch if r1 NOT equal to r2 */
	  return (registers[instr[0] & 0x0F] != registers[instr[1] & 0x0F]);
	default:		/* oops? */
	  return 0;
	}
    case 6: 			/* BNEZ, BLTZ, BLEZ, BGTZ, BGEZ ,BEQZ */
      switch (instr[1] & 0xF0) 
	{
	case 0x80:		/* Branch if reg equal to zero */
	  return (registers[instr[1] & 0x0F] == 0);
	case 0x90:		/* Branch if reg NOT equal to zero */
	  return (registers[instr[1] & 0x0F] != 0);
	case 0xA0:		/* Branch if reg less than zero */
	  return (registers[instr[1] & 0x0F] < 0);
	case 0xB0:		/* Branch if reg greater or equal to zero */
	  return (registers[instr[1] & 0x0F] >= 0);
	case 0xC0:		/* Branch if reg less than or equal to zero */
	  return (registers[instr[1] & 0x0F] <= 0);
	case 0xD0:		/* Branch if reg greater than zero */
	  return (registers[instr[1] & 0x0F] > 0);
	default:		/* oops? */
	  return 0;
	}
    default:			/* oops? */
      return 0;
    }
}

static int 
branchDestination (unsigned char *instr, int branchCode)
{ 
  switch (branchCode) { 
  default: 
  case 0:					/* not a branch */ 
    return 0;
  case 1:					/* RTE */ 
    return registers[BPC] & ~3; 		/* pop BPC into PC */
  case 2: 					/* JL or JMP */ 
    return registers[instr[1] & 0x0F] & ~3;	/* jump thru a register */ 
  case 3: 		/* BC, BNC, BL, BRA (short, 8-bit relative offset) */ 
    return (((int) instr) & ~3) + ((char) instr[1] << 2);
  case 4: 		/* BC, BNC, BL, BRA (long, 24-bit relative offset) */ 
    return ((int) instr + 
	    ((((char) instr[1] << 16) | (instr[2] << 8) | (instr[3])) << 2)); 
  case 5: 		/* BNE, BEQ (16-bit relative offset) */ 
  case 6: 		/* BNEZ, BLTZ, BLEZ, BGTZ, BGEZ ,BEQZ (ditto) */ 
    return ((int) instr + ((((char) instr[2] << 8) | (instr[3])) << 2)); 
  }

  /* An explanatory note: in the last three return expressions, I have
     cast the most-significant byte of the return offset to char.
     What this accomplishes is sign extension.  If the other
     less-significant bytes were signed as well, they would get sign
     extended too and, if negative, their leading bits would clobber
     the bits of the more-significant bytes ahead of them.  There are
     other ways I could have done this, but sign extension from
     odd-sized integers is always a pain. */
}

static void
branchSideEffects (unsigned char *instr, int branchCode)
{
  switch (branchCode)
    {
    case 1:			/* RTE */
      return;			/* I <THINK> this is already handled... */
    case 2:			/* JL (or JMP) */
    case 3:			/* BL (or BC, BNC, BRA) */
    case 4:
      if ((instr[0] & 0x0F) == 0x0E)		/* branch/jump and link */
	registers[R14] = (registers[PC] & ~3) + 4;
      return;
    default:			/* any other branch has no side effects */
      return;
    }
}

static struct STEPPING_CONTEXT {
  int stepping;			/* true when we've started a single-step */
  unsigned long  target_addr;	/* the instr we're trying to execute */
  unsigned long  target_size;	/* the size of the target instr */
  unsigned long  noop_addr;	/* where we've inserted a no-op, if any */
  unsigned long  trap1_addr;	/* the trap following the target instr */
  unsigned long  trap2_addr;	/* the trap at a branch destination, if any */
  unsigned short noop_save;	/* instruction overwritten by our no-op */
  unsigned short trap1_save;	/* instruction overwritten by trap1 */
  unsigned short trap2_save;	/* instruction overwritten by trap2 */
  unsigned short continue_p;	/* true if NOT returning to gdb after step */
} stepping;

/* Function: prepare_to_step
   Called from handle_exception to prepare the user program to single-step.
   Places a trap instruction after the target instruction, with special 
   extra handling for branch instructions and for instructions in the 
   second half-word of a word.  

   Returns: True  if we should actually execute the instruction; 
	    False if we are going to emulate executing the instruction,
	    in which case we simply report to GDB that the instruction 
	    has already been executed.  */

#define TRAP1  0x10f1;	/* trap #1 instruction */
#define NOOP   0x7000;  /* noop    instruction */

static unsigned short trap1 = TRAP1;
static unsigned short noop  = NOOP;

static int
prepare_to_step(continue_p)
     int continue_p;	/* if this isn't REALLY a single-step (see below) */
{
  unsigned long pc = registers[PC];
  int branchCode   = isBranch((unsigned char *) pc);
  unsigned char *p;

  /* zero out the stepping context 
     (paranoia -- it should already be zeroed) */
  for (p = (unsigned char *) &stepping;
       p < ((unsigned char *) &stepping) + sizeof(stepping);
       p++)
    *p = 0;

  if (branchCode != 0)			/* next instruction is a branch */
    {
      branchSideEffects((unsigned char *) pc, branchCode);
      if (willBranch((unsigned char *)pc, branchCode))
	registers[PC] = branchDestination((unsigned char *) pc, branchCode);
      else
	registers[PC] = pc + INSTRUCTION_SIZE(pc);
      return 0;			/* branch "executed" -- just notify GDB */
    }
  else if (((int) pc & 2) != 0)		/* "second-slot" instruction */
    {
      /* insert no-op before pc */
      stepping.noop_addr  =  pc - 2;
      stepping.noop_save  = *(unsigned short *) stepping.noop_addr;
      *(unsigned short *) stepping.noop_addr  = noop;
      /* insert trap  after  pc */
      stepping.trap1_addr =  pc + 2;
      stepping.trap1_save = *(unsigned short *) stepping.trap1_addr;
      *(unsigned short *) stepping.trap1_addr = trap1;
    }
  else					/* "first-slot" instruction */
    {
      /* insert trap  after  pc */
      stepping.trap1_addr = pc + INSTRUCTION_SIZE(pc);	
      stepping.trap1_save = *(unsigned short *) stepping.trap1_addr;
      *(unsigned short *) stepping.trap1_addr = trap1;
    }
  /* "continue_p" means that we are actually doing a continue, and not 
     being requested to single-step by GDB.  Sometimes we have to do
     one single-step before continuing, because the PC is on a half-word
     boundary.  There's no way to simply resume at such an address.  */
  stepping.continue_p = continue_p;
  stepping.stepping = 1;		/* starting a single-step */
  return 1;
}

/* Function: finish_from_step
   Called from handle_exception to finish up when the user program 
   returns from a single-step.  Replaces the instructions that had
   been overwritten by traps or no-ops, 

   Returns: True  if we should notify GDB that the target stopped.
	    False if we only single-stepped because we had to before we
	    could continue (ie. we were trying to continue at a 
	    half-word boundary).  In that case don't notify GDB:
	    just "continue continuing".  */

static int
finish_from_step (void)
{
  if (stepping.stepping)	/* anything to do? */
    {
      int continue_p = stepping.continue_p;
      unsigned char *p;

      if (stepping.noop_addr)	/* replace instr "under" our no-op */
	*(unsigned short *) stepping.noop_addr  = stepping.noop_save;
      if (stepping.trap1_addr)	/* replace instr "under" our trap  */
	*(unsigned short *) stepping.trap1_addr = stepping.trap1_save;
      if (stepping.trap2_addr)  /* ditto our other trap, if any    */
	*(unsigned short *) stepping.trap2_addr = stepping.trap2_save;

      for (p = (unsigned char *) &stepping;	/* zero out the stepping context */
	   p < ((unsigned char *) &stepping) + sizeof(stepping);
	   p++)
	*p = 0;

      return !(continue_p);
    }
  else 	/* we didn't single-step, therefore this must be a legitimate stop */
    return 1;
}

struct PSWreg {		/* separate out the bit flags in the PSW register */
  int pad1 : 16;
  int bsm  : 1;
  int bie  : 1;
  int pad2 : 5;
  int bc   : 1;
  int sm   : 1;
  int ie   : 1;
  int pad3 : 5;
  int c    : 1;
} *psw;

/* Upon entry the value for LR to save has been pushed.
   We unpush that so that the value for the stack pointer saved is correct.
   Upon entry, all other registers are assumed to have not been modified
   since the interrupt/trap occured.  */

asm ("
stash_registers:
	push r0
	push r1
	seth r1, #shigh(registers)
	add3 r1, r1, #low(registers)
	pop r0		; r1
	st r0, @(4,r1)
	pop r0		; r0
	st r0, @r1
	addi r1, #4	; only add 4 as subsequent saves are `pre inc'
	st r2, @+r1
	st r3, @+r1
	st r4, @+r1
	st r5, @+r1
	st r6, @+r1
	st r7, @+r1
	st r8, @+r1
	st r9, @+r1
	st r10, @+r1
	st r11, @+r1
	st r12, @+r1
	st r13, @+r1    ; fp
	pop r0		; lr (r14)
	st r0, @+r1
	st sp, @+r1	; sp contains right value at this point
	mvfc r0, cr0
	st r0, @+r1	; cr0 == PSW
	mvfc r0, cr1
	st r0, @+r1	; cr1 == CBR
	mvfc r0, cr2
	st r0, @+r1	; cr2 == SPI
	mvfc r0, cr3
	st r0, @+r1	; cr3 == SPU
	mvfc r0, cr6
	st r0, @+r1	; cr6 == BPC
	st r0, @+r1	; PC  == BPC
	mvfaclo r0
	st r0, @+r1	; ACCL
	mvfachi r0
	st r0, @+r1	; ACCH
	jmp lr");

/* C routine to clean up what stash_registers did.
   It is called after calling stash_registers.
   This is separate from stash_registers as we want to do this in C
   but doing stash_registers in C isn't straightforward.  */

static void
cleanup_stash (void)
{
  psw = (struct PSWreg *) &registers[PSW];	/* fields of PSW register */
  psw->sm = psw->bsm;		/* fix up pre-trap values of psw fields */
  psw->ie = psw->bie;
  psw->c  = psw->bc;
  registers[CBR] = psw->bc;		/* fix up pre-trap "C" register */

#if 0 /* FIXME: Was in previous version.  Necessary?
	 (Remember that we use the "rte" insn to return from the
	 trap/interrupt so the values of bsm, bie, bc are important.  */
  psw->bsm = psw->bie = psw->bc = 0;	/* zero post-trap values */
#endif

  /* FIXME: Copied from previous version.  This can probably be deleted
     since methinks stash_registers has already done this.  */
  registers[PC] = registers[BPC];	/* pre-trap PC */

  /* FIXME: Copied from previous version.  Necessary?  */
  if (psw->sm)			/* copy R15 into (psw->sm ? SPU : SPI) */
    registers[SPU] = registers[R15];
  else
    registers[SPI] = registers[R15];
}

asm ("
restore_and_return:
	seth r0, #shigh(registers+8)
	add3 r0, r0, #low(registers+8)
	ld r2, @r0+	; restore r2
	ld r3, @r0+	; restore r3
	ld r4, @r0+	; restore r4
	ld r5, @r0+	; restore r5
	ld r6, @r0+	; restore r6
	ld r7, @r0+	; restore r7
	ld r8, @r0+	; restore r8
	ld r9, @r0+	; restore r9
	ld r10, @r0+	; restore r10
	ld r11, @r0+	; restore r11
	ld r12, @r0+	; restore r12
	ld r13, @r0+	; restore r13
	ld r14, @r0+	; restore r14
	ld r15, @r0+	; restore r15
	ld r1, @r0+	; restore cr0 == PSW
	mvtc r1, cr0
	ld r1, @r0+	; restore cr1 == CBR (no-op, because it's read only)
	mvtc r1, cr1
	ld r1, @r0+	; restore cr2 == SPI
	mvtc r1, cr2
	ld r1, @r0+	; restore cr3 == SPU
	mvtc r1, cr3
	addi r0, #4	; skip BPC
	ld r1, @r0+	; restore cr6 (BPC) == PC
	mvtc r1, cr6
	ld r1, @r0+	; restore ACCL
	mvtaclo r1
	ld r1, @r0+	; restore ACCH
	mvtachi r1
	seth r0, #shigh(registers)
	add3 r0, r0, #low(registers)
	ld r1, @(4,r0)	; restore r1
	ld r0, @r0	; restore r0
	rte");

/* General trap handler, called after the registers have been stashed.
   NUM is the trap/exception number.  */

static void
process_exception (int num)
{
  cleanup_stash ();
  asm volatile ("
	seth r1, #shigh(stackPtr)
	add3 r1, r1, #low(stackPtr)
	ld r15, @r1		; setup local stack (protect user stack)
	mv r0, %0
	bl handle_exception
	bl restore_and_return"
		: : "r" (num) : "r0", "r1");
}

void _catchException0 ();

asm ("
_catchException0:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #0
	bl process_exception");

void _catchException1 ();

asm ("
_catchException1:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	bl cleanup_stash
	seth r1, #shigh(stackPtr)
	add3 r1, r1, #low(stackPtr)
	ld r15, @r1		; setup local stack (protect user stack)
	seth r1, #shigh(registers + 21*4) ; PC
	add3 r1, r1, #low(registers + 21*4)
	ld r0, @r1
	addi r0, #-4		; back up PC for breakpoint trap.
	st r0, @r1		; FIXME: what about bp in right slot?
	ldi r0, #1
	bl handle_exception
	bl restore_and_return");

void _catchException2 ();

asm ("
_catchException2:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #2
	bl process_exception");

void _catchException3 ();

asm ("
_catchException3:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #3
	bl process_exception");

void _catchException4 ();

asm ("
_catchException4:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #4
	bl process_exception");

void _catchException5 ();

asm ("
_catchException5:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #5
	bl process_exception");

void _catchException6 ();

asm ("
_catchException6:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #6
	bl process_exception");

void _catchException7 ();

asm ("
_catchException7:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #7
	bl process_exception");

void _catchException8 ();

asm ("
_catchException8:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #8
	bl process_exception");

void _catchException9 ();

asm ("
_catchException9:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #9
	bl process_exception");

void _catchException10 ();

asm ("
_catchException10:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #10
	bl process_exception");

void _catchException11 ();

asm ("
_catchException11:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #11
	bl process_exception");

void _catchException12 ();

asm ("
_catchException12:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #12
	bl process_exception");

void _catchException13 ();

asm ("
_catchException13:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #13
	bl process_exception");

void _catchException14 ();

asm ("
_catchException14:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #14
	bl process_exception");

void _catchException15 ();

asm ("
_catchException15:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #15
	bl process_exception");

void _catchException16 ();

asm ("
_catchException16:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #16
	bl process_exception");

void _catchException17 ();

asm ("
_catchException17:
	push lr
	bl stash_registers
	; Note that at this point the pushed value of `lr' has been popped
	ldi r0, #17
	bl process_exception");


/* this function is used to set up exception handlers for tracing and
   breakpoints */
void 
set_debug_traps (void)
{
  /*  extern void remcomHandler(); */
  int i;

  for (i = 0; i < 18; i++)		/* keep a copy of old vectors */
    if (save_vectors[i] == 0)		/* only copy them the first time */
      save_vectors[i] = getExceptionHandler (i);

  stackPtr  = &remcomStack[STACKSIZE/sizeof(int) - 1];

  exceptionHandler (0, _catchException0);
  exceptionHandler (1, _catchException1);
  exceptionHandler (2, _catchException2);
  exceptionHandler (3, _catchException3);
  exceptionHandler (4, _catchException4);
  exceptionHandler (5, _catchException5);
  exceptionHandler (6, _catchException6);
  exceptionHandler (7, _catchException7);
  exceptionHandler (8, _catchException8);
  exceptionHandler (9, _catchException9);
  exceptionHandler (10, _catchException10);
  exceptionHandler (11, _catchException11);
  exceptionHandler (12, _catchException12);
  exceptionHandler (13, _catchException13);
  exceptionHandler (14, _catchException14);
  exceptionHandler (15, _catchException15);
  exceptionHandler (16, _catchException16);
  /*  exceptionHandler (17, _catchException17); */

  initialized = 1;
}

/* This function will generate a breakpoint exception.  It is used at the
   beginning of a program to sync up with a debugger and can be used
   otherwise as a quick means to stop program execution and "break" into
   the debugger. */

#define BREAKPOINT() asm volatile ("	trap #2");

void 
breakpoint (void)
{
  if (initialized)
    BREAKPOINT();
}

/* STDOUT section:
   Stuff pertaining to simulating stdout by sending chars to gdb to be echoed.
   Functions: gdb_putchar(char ch)
              gdb_puts(char *str)
              gdb_write(char *str, int len)
              gdb_error(char *format, char *parm)
	      */
 
/* Function: gdb_putchar(int)
   Make gdb write a char to stdout.
   Returns: the char */
 
static int
gdb_putchar (int ch)
{
  char buf[4];
 
  buf[0] = 'O';
  buf[1] = hexchars[ch >> 4];
  buf[2] = hexchars[ch & 0x0F];
  buf[3] = 0;
  putpacket(buf);
  return ch;
}
 
/* Function: gdb_write(char *, int)
   Make gdb write n bytes to stdout (not assumed to be null-terminated).
   Returns: number of bytes written */
 
static int
gdb_write (char *data, int len)
{
  char *buf, *cpy;
  int i;
 
  buf = remcomOutBuffer;
  buf[0] = 'O';
  i = 0;
  while (i < len)
    {
      for (cpy = buf+1; 
	   i < len && cpy < buf + sizeof(remcomOutBuffer) - 3; 
	   i++)
	{
	  *cpy++ = hexchars[data[i] >> 4];
	  *cpy++ = hexchars[data[i] & 0x0F];
	}
      *cpy = 0;
      putpacket(buf);
    }
  return len;
}

/* Function: gdb_puts(char *)
   Make gdb write a null-terminated string to stdout.
   Returns: the length of the string */
 
static int
gdb_puts (char *str)
{
  return gdb_write(str, strlen(str));
}
 
/* Function: gdb_error(char *, char *)
   Send an error message to gdb's stdout.
   First string may have 1 (one) optional "%s" in it, which
   will cause the optional second string to be inserted.  */
 
static void
gdb_error (char *format, char *parm)
{
  char buf[400], *cpy;
  int len;
 
  if (remote_debug)
    {
      if (format && *format)
	len = strlen(format);
      else
	return;             /* empty input */

      if (parm && *parm)
	len += strlen(parm);
 
      for (cpy = buf; *format; )
	{
	  if (format[0] == '%' && format[1] == 's') /* include second string */
	    {
	      format += 2;          /* advance two chars instead of just one */
	      while (parm && *parm)
		*cpy++ = *parm++;
	    }
	  else
	    *cpy++ = *format++;
	}
      *cpy = '\0';
      gdb_puts(buf);
    }
}
 
static unsigned char *
strcpy (unsigned char *dest, const unsigned char *src)
{
  unsigned char *ret = dest;

  if (dest && src)
    {
      while (*src)
	*dest++ = *src++;
      *dest = 0;
    }
  return ret;
}

static int
strlen (const unsigned char *src)
{
  int ret;

  for (ret = 0; *src; src++)
    ret++;

  return ret;
}

#if 0
void exit (code)
     int code;
{
  _exit (code);
}

int atexit (void *p)
{
  return 0;
}

void abort (void)
{
  _exit (1);
}
#endif
