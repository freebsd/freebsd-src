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
 *  Modified for SPARC by Stu Grossman, Cygnus Support.
 *  Based on sparc-stub.c, it's modified for SPARClite Debug Unit hardware
 *  breakpoint support to create sparclite-stub.c, by Kung Hsu, Cygnus Support.
 *
 *  This code has been extensively tested on the Fujitsu SPARClite demo board.
 *
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a trap #1.
 *
 *************
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *    P             set the value of a single CPU register OK or ENN
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

#include <string.h>
#include <signal.h>
#include <sparclite.h>

/************************************************************************
 *
 * external low-level support routines
 */

extern void putDebugChar (int c); /* write a single character      */
extern int getDebugChar (void);	/* read and return a single char */

/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 2048

static int initialized = 0;	/* !0 means we've been initialized */

extern void breakinst ();
static void set_mem_fault_trap (int enable);
static void get_in_break_mode (void);

static const char hexchars[]="0123456789abcdef";

#define NUMREGS 80 

/* Number of bytes of registers.  */
#define NUMREGBYTES (NUMREGS * 4)
enum regnames {G0, G1, G2, G3, G4, G5, G6, G7,
		 O0, O1, O2, O3, O4, O5, SP, O7,
		 L0, L1, L2, L3, L4, L5, L6, L7,
		 I0, I1, I2, I3, I4, I5, FP, I7,

		 F0, F1, F2, F3, F4, F5, F6, F7,
		 F8, F9, F10, F11, F12, F13, F14, F15,
		 F16, F17, F18, F19, F20, F21, F22, F23,
		 F24, F25, F26, F27, F28, F29, F30, F31,
		 Y, PSR, WIM, TBR, PC, NPC, FPSR, CPSR,
		 DIA1, DIA2, DDA1, DDA2, DDV1, DDV2, DCR, DSR };

/***************************  ASSEMBLY CODE MACROS *************************/
/* 									   */

extern void trap_low();

/* Create private copies of common functions used by the stub.  This prevents
   nasty interactions between app code and the stub (for instance if user steps
   into strlen, etc..) */

static char *
strcpy (char *dst, const char *src)
{
  char *retval = dst;

  while ((*dst++ = *src++) != '\000');

  return retval;
}

static void *
memcpy (void *vdst, const void *vsrc, int n)
{
  char *dst = vdst;
  const char *src = vsrc;
  char *retval = dst;

  while (n-- > 0)
    *dst++ = *src++;

  return retval;
}

asm("
	.reserve trapstack, 1000 * 4, \"bss\", 8

	.data
	.align	4

in_trap_handler:
	.word	0

	.text
	.align 4

! This function is called when any SPARC trap (except window overflow or
! underflow) occurs.  It makes sure that the invalid register window is still
! available before jumping into C code.  It will also restore the world if you
! return from handle_exception.
!
! On entry, trap_low expects l1 and l2 to contain pc and npc respectivly.
! Register usage throughout the routine is as follows:
!
!	l0 - psr
!	l1 - pc
!	l2 - npc
!	l3 - wim
!	l4 - scratch and y reg
!	l5 - scratch and tbr
!	l6 - unused
!	l7 - unused

	.globl _trap_low
_trap_low:
	mov	%psr, %l0
	mov	%wim, %l3

	srl	%l3, %l0, %l4		! wim >> cwp
	cmp	%l4, 1
	bne	window_fine		! Branch if not in the invalid window
	nop

! Handle window overflow

	mov	%g1, %l4		! Save g1, we use it to hold the wim
	srl	%l3, 1, %g1		! Rotate wim right
	tst	%g1
	bg	good_wim		! Branch if new wim is non-zero
	nop

! At this point, we need to bring a 1 into the high order bit of the wim.
! Since we don't want to make any assumptions about the number of register
! windows, we figure it out dynamically so as to setup the wim correctly.

	not	%g1			! Fill g1 with ones
	mov	%g1, %wim		! Fill the wim with ones
	nop
	nop
	nop
	mov	%wim, %g1		! Read back the wim
	inc	%g1			! Now g1 has 1 just to left of wim
	srl	%g1, 1, %g1		! Now put 1 at top of wim
	mov	%g0, %wim		! Clear wim so that subsequent save
	nop				!  won't trap
	nop
	nop

good_wim:
	save	%g0, %g0, %g0		! Slip into next window
	mov	%g1, %wim		! Install the new wim

	std	%l0, [%sp + 0 * 4]	! save L & I registers
	std	%l2, [%sp + 2 * 4]
	std	%l4, [%sp + 4 * 4]
	std	%l6, [%sp + 6 * 4]

	std	%i0, [%sp + 8 * 4]
	std	%i2, [%sp + 10 * 4]
	std	%i4, [%sp + 12 * 4]
	std	%i6, [%sp + 14 * 4]

	restore				! Go back to trap window.
	mov	%l4, %g1		! Restore %g1

window_fine:
	sethi	%hi(in_trap_handler), %l4
	ld	[%lo(in_trap_handler) + %l4], %l5
	tst	%l5
	bg	recursive_trap
	inc	%l5

	set	trapstack+1000*4, %sp	! Switch to trap stack

recursive_trap:
	st	%l5, [%lo(in_trap_handler) + %l4]
	sub	%sp,(16+1+6+1+80)*4,%sp	! Make room for input & locals
 					! + hidden arg + arg spill
					! + doubleword alignment
					! + registers[72] local var

	std	%g0, [%sp + (24 + 0) * 4] ! registers[Gx]
	std	%g2, [%sp + (24 + 2) * 4]
	std	%g4, [%sp + (24 + 4) * 4]
	std	%g6, [%sp + (24 + 6) * 4]

	std	%i0, [%sp + (24 + 8) * 4] ! registers[Ox]
	std	%i2, [%sp + (24 + 10) * 4]
	std	%i4, [%sp + (24 + 12) * 4]
	std	%i6, [%sp + (24 + 14) * 4]

	mov	%y, %l4
	mov	%tbr, %l5
	st	%l4, [%sp + (24 + 64) * 4] ! Y
	st	%l0, [%sp + (24 + 65) * 4] ! PSR
	st	%l3, [%sp + (24 + 66) * 4] ! WIM
	st	%l5, [%sp + (24 + 67) * 4] ! TBR
	st	%l1, [%sp + (24 + 68) * 4] ! PC
	st	%l2, [%sp + (24 + 69) * 4] ! NPC

	or	%l0, 0xf20, %l4
	mov	%l4, %psr		! Turn on traps, disable interrupts

	set	0x1000, %l1
	btst	%l1, %l0		! FP enabled?
	be	no_fpstore
	nop

! Must save fsr first, to flush the FQ.  This may cause a deferred fp trap, so
! traps must be enabled to allow the trap handler to clean things up.

	st	%fsr, [%sp + (24 + 70) * 4]

	std	%f0, [%sp + (24 + 32) * 4]
	std	%f2, [%sp + (24 + 34) * 4]
	std	%f4, [%sp + (24 + 36) * 4]
	std	%f6, [%sp + (24 + 38) * 4]
	std	%f8, [%sp + (24 + 40) * 4]
	std	%f10, [%sp + (24 + 42) * 4]
	std	%f12, [%sp + (24 + 44) * 4]
	std	%f14, [%sp + (24 + 46) * 4]
	std	%f16, [%sp + (24 + 48) * 4]
	std	%f18, [%sp + (24 + 50) * 4]
	std	%f20, [%sp + (24 + 52) * 4]
	std	%f22, [%sp + (24 + 54) * 4]
	std	%f24, [%sp + (24 + 56) * 4]
	std	%f26, [%sp + (24 + 58) * 4]
	std	%f28, [%sp + (24 + 60) * 4]
	std	%f30, [%sp + (24 + 62) * 4]
no_fpstore:

	call	_handle_exception
	add	%sp, 24 * 4, %o0	! Pass address of registers

! Reload all of the registers that aren't on the stack

	ld	[%sp + (24 + 1) * 4], %g1 ! registers[Gx]
	ldd	[%sp + (24 + 2) * 4], %g2
	ldd	[%sp + (24 + 4) * 4], %g4
	ldd	[%sp + (24 + 6) * 4], %g6

	ldd	[%sp + (24 + 8) * 4], %i0 ! registers[Ox]
	ldd	[%sp + (24 + 10) * 4], %i2
	ldd	[%sp + (24 + 12) * 4], %i4
	ldd	[%sp + (24 + 14) * 4], %i6


	ldd	[%sp + (24 + 64) * 4], %l0 ! Y & PSR
	ldd	[%sp + (24 + 68) * 4], %l2 ! PC & NPC

	set	0x1000, %l5
	btst	%l5, %l1		! FP enabled?
	be	no_fpreload
	nop

	ldd	[%sp + (24 + 32) * 4], %f0
	ldd	[%sp + (24 + 34) * 4], %f2
	ldd	[%sp + (24 + 36) * 4], %f4
	ldd	[%sp + (24 + 38) * 4], %f6
	ldd	[%sp + (24 + 40) * 4], %f8
	ldd	[%sp + (24 + 42) * 4], %f10
	ldd	[%sp + (24 + 44) * 4], %f12
	ldd	[%sp + (24 + 46) * 4], %f14
	ldd	[%sp + (24 + 48) * 4], %f16
	ldd	[%sp + (24 + 50) * 4], %f18
	ldd	[%sp + (24 + 52) * 4], %f20
	ldd	[%sp + (24 + 54) * 4], %f22
	ldd	[%sp + (24 + 56) * 4], %f24
	ldd	[%sp + (24 + 58) * 4], %f26
	ldd	[%sp + (24 + 60) * 4], %f28
	ldd	[%sp + (24 + 62) * 4], %f30

	ld	[%sp + (24 + 70) * 4], %fsr
no_fpreload:

	restore				! Ensure that previous window is valid
	save	%g0, %g0, %g0		!  by causing a window_underflow trap

	mov	%l0, %y
	mov	%l1, %psr		! Make sure that traps are disabled
					! for rett
	sethi	%hi(in_trap_handler), %l4
	ld	[%lo(in_trap_handler) + %l4], %l5
	dec	%l5
	st	%l5, [%lo(in_trap_handler) + %l4]

	jmpl	%l2, %g0		! Restore old PC
	rett	%l3			! Restore old nPC
");

/* Convert ch from a hex digit to an int */

static int
hex (unsigned char ch)
{
  if (ch >= 'a' && ch <= 'f')
    return ch-'a'+10;
  if (ch >= '0' && ch <= '9')
    return ch-'0';
  if (ch >= 'A' && ch <= 'F')
    return ch-'A'+10;
  return -1;
}

static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];

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
  int count;
  unsigned char ch;

  /*  $<packet info>#<checksum>. */
  do
    {
      putDebugChar('$');
      checksum = 0;
      count = 0;

      while (ch = buffer[count])
	{
	  putDebugChar (ch);
	  checksum += ch;
	  count += 1;
	}

      putDebugChar('#');
      putDebugChar(hexchars[checksum >> 4]);
      putDebugChar(hexchars[checksum & 0xf]);

    }
  while (getDebugChar() != '+');
}

/* Indicate to caller of mem2hex or hex2mem that there has been an
   error.  */
static volatile int mem_err = 0;

/* Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null), in case of mem fault,
 * return 0.
 * If MAY_FAULT is non-zero, then we will handle memory faults by returning
 * a 0, else treat a fault like any other fault in the stub.
 */

static unsigned char *
mem2hex (unsigned char *mem, unsigned char *buf, int count, int may_fault)
{
  unsigned char ch;

  set_mem_fault_trap(may_fault);

  while (count-- > 0)
    {
      ch = *mem++;
      if (mem_err)
	return 0;
      *buf++ = hexchars[ch >> 4];
      *buf++ = hexchars[ch & 0xf];
    }

  *buf = 0;

  set_mem_fault_trap(0);

  return buf;
}

/* convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written */

static char *
hex2mem (unsigned char *buf, unsigned char *mem, int count, int may_fault)
{
  int i;
  unsigned char ch;

  set_mem_fault_trap(may_fault);

  for (i=0; i<count; i++)
    {
      ch = hex(*buf++) << 4;
      ch |= hex(*buf++);
      *mem++ = ch;
      if (mem_err)
	return 0;
    }

  set_mem_fault_trap(0);

  return mem;
}

/* This table contains the mapping between SPARC hardware trap types, and
   signals, which are primarily what GDB understands.  It also indicates
   which hardware traps we need to commandeer when initializing the stub. */

static struct hard_trap_info
{
  unsigned char tt;		/* Trap type code for SPARClite */
  unsigned char signo;		/* Signal that we map this trap into */
} hard_trap_info[] = {
  {0x01, SIGSEGV},		/* instruction access error */
  {0x02, SIGILL},		/* privileged instruction */
  {0x03, SIGILL},		/* illegal instruction */
  {0x04, SIGEMT},		/* fp disabled */
  {0x07, SIGBUS},		/* mem address not aligned */
  {0x09, SIGSEGV},		/* data access exception */
  {0x0a, SIGEMT},		/* tag overflow */
  {0x20, SIGBUS},		/* r register access error */
  {0x21, SIGBUS},		/* instruction access error */
  {0x24, SIGEMT},		/* cp disabled */
  {0x29, SIGBUS},		/* data access error */
  {0x2a, SIGFPE},		/* divide by zero */
  {0x2b, SIGBUS},		/* data store error */
  {0x80+1, SIGTRAP},		/* ta 1 - normal breakpoint instruction */
  {0xff, SIGTRAP},		/* hardware breakpoint */
  {0, 0}			/* Must be last */
};

/* Set up exception handlers for tracing and breakpoints */

void
set_debug_traps (void)
{
  struct hard_trap_info *ht;

/* Only setup fp traps if the FP is disabled.  */

  for (ht = hard_trap_info;
       ht->tt != 0 && ht->signo != 0;
       ht++)
    if (ht->tt != 4 || ! (read_psr () & 0x1000))
      exceptionHandler(ht->tt, trap_low);

  initialized = 1;
}

asm ("
! Trap handler for memory errors.  This just sets mem_err to be non-zero.  It
! assumes that %l1 is non-zero.  This should be safe, as it is doubtful that
! 0 would ever contain code that could mem fault.  This routine will skip
! past the faulting instruction after setting mem_err.

	.text
	.align 4

_fltr_set_mem_err:
	sethi %hi(_mem_err), %l0
	st %l1, [%l0 + %lo(_mem_err)]
	jmpl %l2, %g0
	rett %l2+4
");

static void
set_mem_fault_trap (int enable)
{
  extern void fltr_set_mem_err();
  mem_err = 0;

  if (enable)
    exceptionHandler(9, fltr_set_mem_err);
  else
    exceptionHandler(9, trap_low);
}

asm ("
	.text
	.align 4

_dummy_hw_breakpoint:
	jmpl %l2, %g0
	rett %l2+4
	nop
	nop
");

static void
get_in_break_mode (void)
{
  extern void dummy_hw_breakpoint();

  exceptionHandler (255, dummy_hw_breakpoint);

  asm ("ta 255");

  exceptionHandler (255, trap_low);
}

/* Convert the SPARC hardware trap type code to a unix signal number. */

static int
computeSignal (int tt)
{
  struct hard_trap_info *ht;

  for (ht = hard_trap_info; ht->tt && ht->signo; ht++)
    if (ht->tt == tt)
      return ht->signo;

  return SIGHUP;		/* default for things we don't know about */
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
      if (hexValue < 0)
	break;

      *intValue = (*intValue << 4) | hexValue;
      numChars ++;

      (*ptr)++;
    }

  return (numChars);
}

/*
 * This function does all command procesing for interfacing to gdb.  It
 * returns 1 if you should skip the instruction at the trap address, 0
 * otherwise.
 */

static void
handle_exception (unsigned long *registers)
{
  int tt;			/* Trap type */
  int sigval;
  int addr;
  int length;
  char *ptr;
  unsigned long *sp;
  unsigned long dsr;

/* First, we must force all of the windows to be spilled out */

  asm("	save %sp, -64, %sp
	save %sp, -64, %sp
	save %sp, -64, %sp
	save %sp, -64, %sp
	save %sp, -64, %sp
	save %sp, -64, %sp
	save %sp, -64, %sp
	save %sp, -64, %sp
	restore
	restore
	restore
	restore
	restore
	restore
	restore
	restore
");

  get_in_break_mode ();		/* Enable DSU register writes */

  registers[DIA1] = read_asi (1, 0xff00);
  registers[DIA2] = read_asi (1, 0xff04);
  registers[DDA1] = read_asi (1, 0xff08);
  registers[DDA2] = read_asi (1, 0xff0c);
  registers[DDV1] = read_asi (1, 0xff10);
  registers[DDV2] = read_asi (1, 0xff14);
  registers[DCR] = read_asi (1, 0xff18);
  registers[DSR] = read_asi (1, 0xff1c);

  if (registers[PC] == (unsigned long)breakinst)
    {
      registers[PC] = registers[NPC];
      registers[NPC] += 4;
    }
  sp = (unsigned long *)registers[SP];

  dsr = (unsigned long)registers[DSR];
  if (dsr & 0x3c)
    tt = 255;
  else
    tt = (registers[TBR] >> 4) & 0xff;

  /* reply to host that an exception has occurred */
  sigval = computeSignal(tt);
  ptr = remcomOutBuffer;

  *ptr++ = 'T';
  *ptr++ = hexchars[sigval >> 4];
  *ptr++ = hexchars[sigval & 0xf];

  *ptr++ = hexchars[PC >> 4];
  *ptr++ = hexchars[PC & 0xf];
  *ptr++ = ':';
  ptr = mem2hex((char *)&registers[PC], ptr, 4, 0);
  *ptr++ = ';';

  *ptr++ = hexchars[FP >> 4];
  *ptr++ = hexchars[FP & 0xf];
  *ptr++ = ':';
  ptr = mem2hex(sp + 8 + 6, ptr, 4, 0); /* FP */
  *ptr++ = ';';

  *ptr++ = hexchars[SP >> 4];
  *ptr++ = hexchars[SP & 0xf];
  *ptr++ = ':';
  ptr = mem2hex((char *)&sp, ptr, 4, 0);
  *ptr++ = ';';

  *ptr++ = hexchars[NPC >> 4];
  *ptr++ = hexchars[NPC & 0xf];
  *ptr++ = ':';
  ptr = mem2hex((char *)&registers[NPC], ptr, 4, 0);
  *ptr++ = ';';

  *ptr++ = hexchars[O7 >> 4];
  *ptr++ = hexchars[O7 & 0xf];
  *ptr++ = ':';
  ptr = mem2hex((char *)&registers[O7], ptr, 4, 0);
  *ptr++ = ';';

  *ptr++ = 0;

  putpacket(remcomOutBuffer);

  while (1)
    {
      remcomOutBuffer[0] = 0;

      ptr = getpacket();
      switch (*ptr++)
	{
	case '?':
	  remcomOutBuffer[0] = 'S';
	  remcomOutBuffer[1] = hexchars[sigval >> 4];
	  remcomOutBuffer[2] = hexchars[sigval & 0xf];
	  remcomOutBuffer[3] = 0;
	  break;

	case 'd':
				/* toggle debug flag */
	  break;

	case 'g':		/* return the value of the CPU registers */
	  memcpy (&registers[L0], sp, 16 * 4); /* Copy L & I regs from stack */
	  mem2hex ((char *)registers, remcomOutBuffer, NUMREGBYTES, 0);
	  break;

	case 'G':		/* Set the value of all registers */
	case 'P':		/* Set the value of one register */
	  {
	    unsigned long *newsp, psr;

	    psr = registers[PSR];

	    if (ptr[-1] == 'P')
	      {
		int regno;

		if (hexToInt (&ptr, &regno)
		    && *ptr++ == '=')
		  if (regno >= L0 && regno <= I7)
		    hex2mem (ptr, sp + regno - L0, 4, 0);
		  else
		    hex2mem (ptr, (char *)&registers[regno], 4, 0);
		else
		  {
		    strcpy (remcomOutBuffer, "E01");
		    break;
		  }
	      }
	    else
	      {
		hex2mem (ptr, (char *)registers, NUMREGBYTES, 0);
		memcpy (sp, &registers[L0], 16 * 4); /* Copy L & I regs to stack */
	      }

	    /* See if the stack pointer has moved.  If so, then copy the saved
	       locals and ins to the new location.  This keeps the window
	       overflow and underflow routines happy.  */

	    newsp = (unsigned long *)registers[SP];
	    if (sp != newsp)
	      sp = memcpy(newsp, sp, 16 * 4);

	    /* Don't allow CWP to be modified. */

	    if (psr != registers[PSR])
	      registers[PSR] = (psr & 0x1f) | (registers[PSR] & ~0x1f);

	    strcpy(remcomOutBuffer,"OK");
	  }
	  break;

	case 'm':	  /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
	  /* Try to read %x,%x.  */

	  if (hexToInt(&ptr, &addr)
	      && *ptr++ == ','
	      && hexToInt(&ptr, &length))
	    {
	      if (mem2hex((char *)addr, remcomOutBuffer, length, 1))
		break;

	      strcpy (remcomOutBuffer, "E03");
	    }
	  else
	    strcpy(remcomOutBuffer,"E01");
	  break;

	case 'M': /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
	  /* Try to read '%x,%x:'.  */

	  if (hexToInt(&ptr, &addr)
	      && *ptr++ == ','
	      && hexToInt(&ptr, &length)
	      && *ptr++ == ':')
	    {
	      if (hex2mem(ptr, (char *)addr, length, 1))
		strcpy(remcomOutBuffer, "OK");
	      else
		strcpy(remcomOutBuffer, "E03");
	    }
	  else
	    strcpy(remcomOutBuffer, "E02");
	  break;

	case 'c':    /* cAA..AA    Continue at address AA..AA(optional) */
	  /* try to read optional parameter, pc unchanged if no parm */
	  if (hexToInt(&ptr, &addr))
	    {
	      registers[PC] = addr;
	      registers[NPC] = addr + 4;
	    }

/* Need to flush the instruction cache here, as we may have deposited a
   breakpoint, and the icache probably has no way of knowing that a data ref to
   some location may have changed something that is in the instruction cache.
 */

	  flush_i_cache ();

	  if (!(registers[DSR] & 0x1) /* DSU enabled? */
	      && !(registers[DCR] & 0x200)) /* Are we in break state? */
	    {			/* Yes, set the DSU regs */
	      write_asi (1, 0xff00, registers[DIA1]);
	      write_asi (1, 0xff04, registers[DIA2]);
	      write_asi (1, 0xff08, registers[DDA1]);
	      write_asi (1, 0xff0c, registers[DDA2]);
	      write_asi (1, 0xff10, registers[DDV1]);
	      write_asi (1, 0xff14, registers[DDV2]);
	      write_asi (1, 0xff1c, registers[DSR]);
	      write_asi (1, 0xff18, registers[DCR] | 0x200); /* Clear break */
	    }

	  return;

	  /* kill the program */
	case 'k' :		/* do nothing */
	  break;
#if 0
	case 't':		/* Test feature */
	  asm (" std %f30,[%sp]");
	  break;
#endif
	case 'r':		/* Reset */
	  asm ("call 0
		nop ");
	  break;
	}			/* switch */

      /* reply to the request */
      putpacket(remcomOutBuffer);
    }
}

/* This function will generate a breakpoint exception.  It is used at the
   beginning of a program to sync up with a debugger and can be used
   otherwise as a quick means to stop program execution and "break" into
   the debugger. */

void
breakpoint (void)
{
  if (!initialized)
    return;

  asm("	.globl _breakinst

	_breakinst: ta 1
      ");
}
