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
 *    P             set the value of a single CPU register OK or P01 (???)
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
 *    bBB..BB	    Set baud rate to BB..BB		   OK or BNN, then sets
 *							   baud rate
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

#include <string.h>
#include <signal.h>

/************************************************************************
 *
 * external low-level support routines
 */

extern void putDebugChar();	/* write a single character      */
extern int getDebugChar();	/* read and return a single char */

/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 2048

static int initialized = 0;	/* !0 means we've been initialized */
static int remote_debug = 0;	/* turn on verbose debugging */

extern void breakinst();
void _cprint();
static void hw_breakpoint();
static void set_mem_fault_trap();
static void get_in_break_mode();
static unsigned char *mem2hex();

static const char hexchars[]="0123456789abcdef";

#define NUMREGS 121

static unsigned long saved_stack_pointer;

/* Number of bytes of registers.  */
#define NUMREGBYTES (NUMREGS * 4)
enum regnames { G0, G1, G2, G3, G4, G5, G6, G7,
		O0, O1, O2, O3, O4, O5, SP, O7,
		L0, L1, L2, L3, L4, L5, L6, L7,
		I0, I1, I2, I3, I4, I5, FP, I7,

		F0, F1, F2, F3, F4, F5, F6, F7,
		F8, F9, F10, F11, F12, F13, F14, F15,
		F16, F17, F18, F19, F20, F21, F22, F23,
		F24, F25, F26, F27, F28, F29, F30, F31,

		Y, PSR, WIM, TBR, PC, NPC, FPSR, CPSR,
		CCSR, CCPR, CCCRCR, CCOR, CCOBR, CCIBR, CCIR, UNUSED1,

		ASR1, ASR15, ASR17, ASR18, ASR19, ASR20, ASR21, ASR22, 
		/* the following not actually implemented */
		AWR0,  AWR1,  AWR2,  AWR3,  AWR4,  AWR5,  AWR6,  AWR7,  
		AWR8,  AWR9,  AWR10, AWR11, AWR12, AWR13, AWR14, AWR15,  
		AWR16, AWR17, AWR18, AWR19, AWR20, AWR21, AWR22, AWR23,  
		AWR24, AWR25, AWR26, AWR27, AWR28, AWR29, AWR30, AWR31,  
		APSR
};

/***************************  ASSEMBLY CODE MACROS *************************/
/* 									   */

extern void trap_low();

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

	.globl _trap_low
_trap_low:
	mov	%psr, %l0
	mov	%wim, %l3

	srl	%l3, %l0, %l4		! wim >> cwp
	and	%l4, 0xff, %l4		! Mask off windows 28, 29
	cmp	%l4, 1
	bne	window_fine		! Branch if not in the invalid window
	nop

! Handle window overflow

	mov	%g1, %l4		! Save g1, we use it to hold the wim
	srl	%l3, 1, %g1		! Rotate wim right
	and	%g1, 0xff, %g1		! Mask off windows 28, 29
	tst	%g1
	bg	good_wim		! Branch if new wim is non-zero
	nop

! At this point, we need to bring a 1 into the high order bit of the wim.
! Since we don't want to make any assumptions about the number of register
! windows, we figure it out dynamically so as to setup the wim correctly.

	! The normal way doesn't work on the sparclet as register windows
	! 28 and 29 are special purpose windows.
	!not	%g1			! Fill g1 with ones
	!mov	%g1, %wim		! Fill the wim with ones
	!nop
	!nop
	!nop
	!mov	%wim, %g1		! Read back the wim
	!inc	%g1			! Now g1 has 1 just to left of wim
	!srl	%g1, 1, %g1		! Now put 1 at top of wim

	mov	0x80, %g1		! Hack for sparclet

	! This doesn't work on the sparclet.
	!mov	%g0, %wim		! Clear wim so that subsequent save
					!  won't trap
	andn	%l3, 0xff, %l5		! Clear wim but not windows 28, 29
	mov	%l5, %wim
	nop
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
	sub	%sp,(16+1+6+1+88)*4,%sp ! Make room for input & locals
 					! + hidden arg + arg spill
					! + doubleword alignment
					! + registers[121]

	std	%g0, [%sp + (24 + 0) * 4] ! registers[Gx]
	std	%g2, [%sp + (24 + 2) * 4]
	std	%g4, [%sp + (24 + 4) * 4]
	std	%g6, [%sp + (24 + 6) * 4]

	std	%i0, [%sp + (24 + 8) * 4] ! registers[Ox]
	std	%i2, [%sp + (24 + 10) * 4]
	std	%i4, [%sp + (24 + 12) * 4]
	std	%i6, [%sp + (24 + 14) * 4]

	! FP regs (sparclet doesn't have fpu)

	mov	%y, %l4
	mov	%tbr, %l5
	st	%l4, [%sp + (24 + 64) * 4] ! Y
	st	%l0, [%sp + (24 + 65) * 4] ! PSR
	st	%l3, [%sp + (24 + 66) * 4] ! WIM
	st	%l5, [%sp + (24 + 67) * 4] ! TBR
	st	%l1, [%sp + (24 + 68) * 4] ! PC
	st	%l2, [%sp + (24 + 69) * 4] ! NPC
					! CPSR and FPSR not impl
	or	%l0, 0xf20, %l4
	mov	%l4, %psr		! Turn on traps, disable interrupts
	nop
        nop
        nop

! Save coprocessor state.
! See SK/demo/hdlc_demo/ldc_swap_context.S.

	mov	%psr, %l0
	sethi	%hi(0x2000), %l5		! EC bit in PSR
	or	%l5, %l0, %l5
	mov	%l5, %psr			! enable coprocessor
	nop			! 3 nops after write to %psr (needed?)
	nop
	nop
	crdcxt	%ccsr, %l1			! capture CCSR
	mov	0x6, %l2
	cwrcxt	%l2, %ccsr	! set CCP state machine for CCFR
	crdcxt	%ccfr, %l2			! capture CCOR
	cwrcxt	%l2, %ccfr			! tickle  CCFR
	crdcxt	%ccfr, %l3			! capture CCOBR
	cwrcxt	%l3, %ccfr			! tickle  CCFR
	crdcxt	%ccfr, %l4			! capture CCIBR
	cwrcxt	%l4, %ccfr			! tickle  CCFR
	crdcxt	%ccfr, %l5			! capture CCIR
	cwrcxt	%l5, %ccfr			! tickle  CCFR
	crdcxt	%ccpr, %l6			! capture CCPR
	crdcxt	%cccrcr, %l7			! capture CCCRCR
	st	%l1, [%sp + (24 + 72) * 4]	! save CCSR
	st	%l2, [%sp + (24 + 75) * 4]	! save CCOR
	st	%l3, [%sp + (24 + 76) * 4]	! save CCOBR
	st	%l4, [%sp + (24 + 77) * 4]	! save CCIBR
	st	%l5, [%sp + (24 + 78) * 4]	! save CCIR
	st	%l6, [%sp + (24 + 73) * 4]	! save CCPR
	st	%l7, [%sp + (24 + 74) * 4]	! save CCCRCR
	mov	%l0, %psr 			! restore original PSR
	nop			! 3 nops after write to %psr (needed?)
	nop
	nop

! End of saving coprocessor state.
! Save asr regs

! Part of this is silly -- we should not display ASR15 or ASR19 at all.

	sethi	%hi(0x01000000), %l6
	st	%l6, [%sp + (24 + 81) * 4]	! ASR15 == NOP
	sethi	%hi(0xdeadc0de), %l6
	or	%l6, %lo(0xdeadc0de), %l6
	st	%l6, [%sp + (24 + 84) * 4]	! ASR19 == DEADC0DE

	rd	%asr1, %l4
	st	%l4, [%sp + (24 + 80) * 4]
!	rd	%asr15, %l4			! must not read ASR15
!	st	%l4, [%sp + (24 + 81) * 4]	! (illegal instr trap)
	rd	%asr17, %l4
	st	%l4, [%sp + (24 + 82) * 4]
	rd	%asr18, %l4
	st	%l4, [%sp + (24 + 83) * 4]
!	rd	%asr19, %l4			! must not read asr19
!	st	%l4, [%sp + (24 + 84) * 4]	! (halts the CPU)
	rd	%asr20, %l4
	st	%l4, [%sp + (24 + 85) * 4]
	rd	%asr21, %l4
	st	%l4, [%sp + (24 + 86) * 4]
	rd	%asr22, %l4
	st	%l4, [%sp + (24 + 87) * 4]

! End of saving asr regs

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

	! FP regs (sparclet doesn't have fpu)

! Update the coprocessor registers.
! See SK/demo/hdlc_demo/ldc_swap_context.S.

	mov	%psr, %l0
	sethi	%hi(0x2000), %l5		! EC bit in PSR
	or	%l5, %l0, %l5
	mov	%l5, %psr			! enable coprocessor
	nop			! 3 nops after write to %psr (needed?)
	nop
	nop

	mov 0x6, %l2
	cwrcxt	%l2, %ccsr	! set CCP state machine for CCFR

	ld	[%sp + (24 + 72) * 4], %l1	! saved CCSR
	ld	[%sp + (24 + 75) * 4], %l2	! saved CCOR
	ld	[%sp + (24 + 76) * 4], %l3	! saved CCOBR
	ld	[%sp + (24 + 77) * 4], %l4	! saved CCIBR
	ld	[%sp + (24 + 78) * 4], %l5	! saved CCIR
	ld	[%sp + (24 + 73) * 4], %l6	! saved CCPR
	ld	[%sp + (24 + 74) * 4], %l7	! saved CCCRCR

	cwrcxt	%l2, %ccfr			! restore CCOR
	cwrcxt	%l3, %ccfr			! restore CCOBR
	cwrcxt	%l4, %ccfr			! restore CCIBR
	cwrcxt	%l5, %ccfr			! restore CCIR
	cwrcxt	%l6, %ccpr			! restore CCPR
	cwrcxt	%l7, %cccrcr			! restore CCCRCR
	cwrcxt	%l1, %ccsr			! restore CCSR

	mov %l0, %psr				! restore PSR
	nop		! 3 nops after write to %psr (needed?)
	nop
	nop

! End of coprocessor handling stuff.
! Update asr regs

	ld	[%sp + (24 + 80) * 4], %l4
	wr	%l4, %asr1
!	ld	[%sp + (24 + 81) * 4], %l4	! can't write asr15
!	wr	%l4, %asr15
	ld	[%sp + (24 + 82) * 4], %l4
	wr	%l4, %asr17
	ld	[%sp + (24 + 83) * 4], %l4
	wr	%l4, %asr18
!	ld	[%sp + (24 + 84) * 4], %l4	! can't write asr19
!	wr	%l4, %asr19
!	ld	[%sp + (24 + 85) * 4], %l4	! can't write asr20
!	wr	%l4, %asr20
!	ld	[%sp + (24 + 86) * 4], %l4	! can't write asr21
!	wr	%l4, %asr21
	ld	[%sp + (24 + 87) * 4], %l4
	wr	%l4, %asr22

! End of restoring asr regs


	ldd	[%sp + (24 + 64) * 4], %l0 ! Y & PSR
	ldd	[%sp + (24 + 68) * 4], %l2 ! PC & NPC

	restore				! Ensure that previous window is valid
	save	%g0, %g0, %g0		!  by causing a window_underflow trap

	mov	%l0, %y
	mov	%l1, %psr		! Make sure that traps are disabled
					! for rett
	nop	! 3 nops after write to %psr (needed?)
	nop
	nop

	sethi	%hi(in_trap_handler), %l4
	ld	[%lo(in_trap_handler) + %l4], %l5
	dec	%l5
	st	%l5, [%lo(in_trap_handler) + %l4]

	jmpl	%l2, %g0		! Restore old PC
	rett	%l3			! Restore old nPC
");

/* Convert ch from a hex digit to an int */

static int
hex(ch)
     unsigned char ch;
{
  if (ch >= 'a' && ch <= 'f')
    return ch-'a'+10;
  if (ch >= '0' && ch <= '9')
    return ch-'0';
  if (ch >= 'A' && ch <= 'F')
    return ch-'A'+10;
  return -1;
}

/* scan for the sequence $<data>#<checksum>     */

static void
getpacket(buffer)
     char *buffer;
{
  unsigned char checksum;
  unsigned char xmitcsum;
  int i;
  int count;
  unsigned char ch;

  do
    {
      /* wait around for the start character, ignore all other characters */
      while ((ch = (getDebugChar() & 0x7f)) != '$') 
	;

      checksum = 0;
      xmitcsum = -1;

      count = 0;

      /* now, read until a # or end of buffer is found */
      while (count < BUFMAX)
	{
	  ch = getDebugChar() & 0x7f;
	  if (ch == '#')
	    break;
	  checksum = checksum + ch;
	  buffer[count] = ch;
	  count = count + 1;
	}

      if (count >= BUFMAX)
	continue;

      buffer[count] = 0;

      if (ch == '#')
	{
	  xmitcsum = hex(ch = getDebugChar() & 0x7f) << 4;
	  xmitcsum |= hex(ch = getDebugChar() & 0x7f);

	  if (checksum != xmitcsum)
	    putDebugChar('-');	/* failed checksum */
	  else
	    {
	      putDebugChar('+'); /* successful transfer */
	      /* if a sequence char is present, reply the sequence ID */
	      if (buffer[2] == ':')
		{
		  putDebugChar(buffer[0]);
		  putDebugChar(buffer[1]);
		  /* remove sequence chars from buffer */
		  count = strlen(buffer);
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
putpacket(buffer)
     unsigned char *buffer;
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
	  putDebugChar(ch);
	  checksum += ch;
	  count += 1;
	}

      putDebugChar('#');
      putDebugChar(hexchars[checksum >> 4]);
      putDebugChar(hexchars[checksum & 0xf]);

    }
  while ((getDebugChar() & 0x7f) != '+');
}

static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];

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
mem2hex(mem, buf, count, may_fault)
     unsigned char *mem;
     unsigned char *buf;
     int count;
     int may_fault;
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
hex2mem(buf, mem, count, may_fault)
     unsigned char *buf;
     unsigned char *mem;
     int count;
     int may_fault;
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
  {1, SIGSEGV},			/* instruction access exception */
  {0x3b, SIGSEGV},		/* instruction access error */
  {2, SIGILL},			/* illegal    instruction */
  {3, SIGILL},			/* privileged instruction */
  {4, SIGEMT},			/* fp disabled */
  {0x24, SIGEMT},		/* cp disabled */
  {7, SIGBUS},			/* mem address not aligned */
  {0x29, SIGSEGV},		/* data access exception */
  {10, SIGEMT},			/* tag overflow */
  {128+1, SIGTRAP},		/* ta 1 - normal breakpoint instruction */
  {0, 0}			/* Must be last */
};

/* Set up exception handlers for tracing and breakpoints */

void
set_debug_traps()
{
  struct hard_trap_info *ht;

  for (ht = hard_trap_info; ht->tt && ht->signo; ht++)
    exceptionHandler(ht->tt, trap_low);

  /* In case GDB is started before us, ack any packets (presumably
     "$?#xx") sitting there.  */
  putDebugChar ('+');

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
set_mem_fault_trap(enable)
     int enable;
{
  extern void fltr_set_mem_err();
  mem_err = 0;

  if (enable)
    exceptionHandler(0x29, fltr_set_mem_err);
  else
    exceptionHandler(0x29, trap_low);
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
set_hw_breakpoint_trap(enable)
     int enable;
{
  extern void dummy_hw_breakpoint();

  if (enable)
    exceptionHandler(255, dummy_hw_breakpoint);
  else
    exceptionHandler(255, trap_low);
}

static void
get_in_break_mode()
{
#if 0
  int x;
  mesg("get_in_break_mode, sp = ");
  phex(&x);
#endif
  set_hw_breakpoint_trap(1);

  asm("
        sethi   %hi(0xff10), %l4
        or      %l4, %lo(0xff10), %l4
	sta 	%g0, [%l4]0x1	
	nop
	nop
	nop
      ");

  set_hw_breakpoint_trap(0);
}

/* Convert the SPARC hardware trap type code to a unix signal number. */

static int
computeSignal(tt)
     int tt;
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
handle_exception (registers)
     unsigned long *registers;
{
  int tt;			/* Trap type */
  int sigval;
  int addr;
  int length;
  char *ptr;
  unsigned long *sp;
  unsigned long dsr;

/* First, we must force all of the windows to be spilled out */

  asm("
	! Ugh.  sparclet has broken save
	!save %sp, -64, %sp
	save
	add %fp,-64,%sp
	!save %sp, -64, %sp
	save
	add %fp,-64,%sp
	!save %sp, -64, %sp
	save
	add %fp,-64,%sp
	!save %sp, -64, %sp
	save
	add %fp,-64,%sp
	!save %sp, -64, %sp
	save
	add %fp,-64,%sp
	!save %sp, -64, %sp
	save
	add %fp,-64,%sp
	!save %sp, -64, %sp
	save
	add %fp,-64,%sp
	!save %sp, -64, %sp
	save
	add %fp,-64,%sp
	restore
	restore
	restore
	restore
	restore
	restore
	restore
	restore
");

  if (registers[PC] == (unsigned long)breakinst)
    {
      registers[PC] = registers[NPC];
      registers[NPC] += 4;
    }
  sp = (unsigned long *)registers[SP];

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

      getpacket(remcomInBuffer);
      switch (remcomInBuffer[0])
	{
	case '?':
	  remcomOutBuffer[0] = 'S';
	  remcomOutBuffer[1] = hexchars[sigval >> 4];
	  remcomOutBuffer[2] = hexchars[sigval & 0xf];
	  remcomOutBuffer[3] = 0;
	  break;

	case 'd':
	  remote_debug = !(remote_debug);	/* toggle debug flag */
	  break;

	case 'g':		/* return the value of the CPU registers */
	  {
	    ptr = remcomOutBuffer;
	    ptr = mem2hex((char *)registers, ptr, 16 * 4, 0); /* G & O regs */
	    ptr = mem2hex(sp + 0, ptr, 16 * 4, 0); /* L & I regs */
	    memset(ptr, '0', 32 * 8); /* Floating point */
	    ptr = mem2hex((char *)&registers[Y],
		    ptr + 32 * 4 * 2,
		    8 * 4,
		    0); /* Y, PSR, WIM, TBR, PC, NPC, FPSR, CPSR */
	    ptr = mem2hex((char *)&registers[CCSR],
		    ptr,
		    8 * 4,
		    0); /* CCSR, CCPR, CCCRCR, CCOR, CCOBR, CCIBR, CCIR */
	    ptr = mem2hex((char *)&registers[ASR1],
		    ptr,
		    8 * 4,
		    0); /* ASR1,ASR15,ASR17,ASR18,ASR19,ASR20,ASR21,ASR22 */
#if 0 /* not implemented */
	    ptr = mem2hex((char *) &registers[AWR0], 
		    ptr, 
		    32 * 4, 
		    0); /* Alternate Window Registers */
#endif
	  }
	  break;

	case 'G':	/* set value of all the CPU registers - return OK */
	case 'P':	/* set value of one CPU register      - return OK */
	  {
	    unsigned long *newsp, psr;

	    psr = registers[PSR];

	    ptr = &remcomInBuffer[1];

	    if (remcomInBuffer[0] == 'P')	/* do a single register */
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
                    strcpy (remcomOutBuffer, "P01");
                    break;
                  }
	      }
	    else
	      {
		hex2mem(ptr, (char *)registers, 16 * 4, 0); /* G & O regs */
		hex2mem(ptr + 16 * 4 * 2, sp + 0, 16 * 4, 0); /* L & I regs */
		hex2mem(ptr + 64 * 4 * 2, (char *)&registers[Y],
			8 * 4, 0); /* Y,PSR,WIM,TBR,PC,NPC,FPSR,CPSR */
		hex2mem(ptr + 72 * 4 * 2, (char *)&registers[CCSR],
			8 * 4, 0); /* CCSR,CCPR,CCCRCR,CCOR,CCOBR,CCIBR,CCIR */
		hex2mem(ptr + 80 * 4 * 2, (char *)&registers[ASR1],
			8 * 4, 0); /* ASR1 ... ASR22 */
#if 0 /* not implemented */
		hex2mem(ptr + 88 * 4 * 2, (char *)&registers[AWR0],
			8 * 4, 0); /* Alternate Window Registers */
#endif
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

	  ptr = &remcomInBuffer[1];

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

	  ptr = &remcomInBuffer[1];

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

	  ptr = &remcomInBuffer[1];
	  if (hexToInt(&ptr, &addr))
	    {
	      registers[PC] = addr;
	      registers[NPC] = addr + 4;
	    }

/* Need to flush the instruction cache here, as we may have deposited a
   breakpoint, and the icache probably has no way of knowing that a data ref to
   some location may have changed something that is in the instruction cache.
 */

	  flush_i_cache();
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

#if 0
Disabled until we can unscrew this properly

	case 'b':	  /* bBB...  Set baud rate to BB... */
	  {
	    int baudrate;
	    extern void set_timer_3();

	    ptr = &remcomInBuffer[1];
	    if (!hexToInt(&ptr, &baudrate))
	      {
		strcpy(remcomOutBuffer,"B01");
		break;
	      }

	    /* Convert baud rate to uart clock divider */
	    switch (baudrate)
	      {
	      case 38400:
		baudrate = 16;
		break;
	      case 19200:
		baudrate = 33;
		break;
	      case 9600:
		baudrate = 65;
		break;
	      default:
		strcpy(remcomOutBuffer,"B02");
		goto x1;
	      }

	    putpacket("OK");	/* Ack before changing speed */
	    set_timer_3(baudrate); /* Set it */
	  }
x1:	  break;
#endif
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
breakpoint()
{
  if (!initialized)
    return;

  asm("	.globl _breakinst

	_breakinst: ta 1
      ");
}

static void
hw_breakpoint()
{
  asm("
      ta 127
      ");
}

#if 0 /* experimental and never finished, left here for reference */
static void
splet_temp(void)
{
  asm("	sub	%sp,(16+1+6+1+121)*4,%sp ! Make room for input & locals
 					! + hidden arg + arg spill
					! + doubleword alignment
					! + registers[121]

! Leave a trail of breadcrumbs! (save register save area for debugging)
	mov	%sp, %l0
	add	%l0, 24*4, %l0
	sethi	%hi(_debug_registers), %l1
	st	%l0, [%lo(_debug_registers) + %l1]

! Save the Alternate Register Set: (not implemented yet)
!    To save the Alternate Register set, we must:
!    1) Save the current SP in some global location.
!    2) Swap the register sets.
!    3) Save the Alternate SP in the Y register
!    4) Fetch the SP that we saved in step 1.
!    5) Use that to save the rest of the regs (not forgetting ASP in Y)
!    6) Restore the Alternate SP from Y
!    7) Swap the registers back.

! 1) Copy the current stack pointer to global _SAVED_STACK_POINTER:
	sethi	%hi(_saved_stack_pointer), %l0
	st	%sp, [%lo(_saved_stack_pointer) + %l0]

! 2) Swap the register sets:
	mov	%psr, %l1
	sethi	%hi(0x10000), %l2
	xor	%l1, %l2, %l1
	mov	%l1, %psr
	nop			! 3 nops after write to %psr (needed?)
	nop
	nop

! 3) Save Alternate L0 in Y
	wr	%l0, 0, %y

! 4) Load former SP into alternate SP, using L0
	sethi	%hi(_saved_stack_pointer), %l0
	or	%lo(_saved_stack_pointer), %l0, %l0
	swap	[%l0], %sp

! 4.5) Restore alternate L0
	rd	%y, %l0

! 5) Save the Alternate Window Registers
	st	%r0, [%sp + (24 + 88) * 4]	! AWR0
	st	%r1, [%sp + (24 + 89) * 4]	! AWR1
	st	%r2, [%sp + (24 + 90) * 4]	! AWR2
	st	%r3, [%sp + (24 + 91) * 4]	! AWR3
	st	%r4, [%sp + (24 + 92) * 4]	! AWR4
	st	%r5, [%sp + (24 + 93) * 4]	! AWR5
	st	%r6, [%sp + (24 + 94) * 4]	! AWR6
	st	%r7, [%sp + (24 + 95) * 4]	! AWR7
	st	%r8, [%sp + (24 + 96) * 4]	! AWR8
	st	%r9, [%sp + (24 + 97) * 4]	! AWR9
	st	%r10, [%sp + (24 + 98) * 4]	! AWR10
	st	%r11, [%sp + (24 + 99) * 4]	! AWR11
	st	%r12, [%sp + (24 + 100) * 4]	! AWR12
	st	%r13, [%sp + (24 + 101) * 4]	! AWR13
!	st	%r14, [%sp + (24 + 102) * 4]	! AWR14	(SP)
	st	%r15, [%sp + (24 + 103) * 4]	! AWR15
	st	%r16, [%sp + (24 + 104) * 4]	! AWR16
	st	%r17, [%sp + (24 + 105) * 4]	! AWR17
	st	%r18, [%sp + (24 + 106) * 4]	! AWR18
	st	%r19, [%sp + (24 + 107) * 4]	! AWR19
	st	%r20, [%sp + (24 + 108) * 4]	! AWR20
	st	%r21, [%sp + (24 + 109) * 4]	! AWR21
	st	%r22, [%sp + (24 + 110) * 4]	! AWR22
	st	%r23, [%sp + (24 + 111) * 4]	! AWR23
	st	%r24, [%sp + (24 + 112) * 4]	! AWR24
	st	%r25, [%sp + (24 + 113) * 4]	! AWR25
	st	%r26, [%sp + (24 + 114) * 4]	! AWR26
	st	%r27, [%sp + (24 + 115) * 4]	! AWR27
	st	%r28, [%sp + (24 + 116) * 4]	! AWR28
	st	%r29, [%sp + (24 + 117) * 4]	! AWR29
	st	%r30, [%sp + (24 + 118) * 4]	! AWR30
	st	%r31, [%sp + (24 + 119) * 4]	! AWR21

! Get the Alternate PSR (I hope...)

	rd	%psr, %l2
	st	%l2, [%sp + (24 + 120) * 4]	! APSR

! Don't forget the alternate stack pointer

	rd	%y, %l3
	st	%l3, [%sp + (24 + 102) * 4]	! AWR14 (SP)

! 6) Restore the Alternate SP (saved in Y)

	rd	%y, %o6


! 7) Swap the registers back:

	mov	%psr, %l1
	sethi	%hi(0x10000), %l2
	xor	%l1, %l2, %l1
	mov	%l1, %psr
	nop			! 3 nops after write to %psr (needed?)
	nop
	nop
");
}

#endif
