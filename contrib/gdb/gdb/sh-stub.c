/* sh-stub.c -- debugging stub for the Hitachi-SH. 

 NOTE!! This code has to be compiled with optimization, otherwise the 
 function inlining which generates the exception handlers won't work.
 
*/

/*   This is originally based on an m68k software stub written by Glenn
     Engel at HP, but has changed quite a bit. 

     Modifications for the SH by Ben Lee and Steve Chamberlain

*/

/****************************************************************************

		THIS SOFTWARE IS NOT COPYRIGHTED

   HP offers the following for use in the public domain.  HP makes no
   warranty with regard to the software or it's performance and the
   user accepts the software "AS IS" with all faults.

   HP DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD
   TO THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

****************************************************************************/


/* Remote communication protocol.

   A debug packet whose contents are <data>
   is encapsulated for transmission in the form:

	$ <data> # CSUM1 CSUM2

	<data> must be ASCII alphanumeric and cannot include characters
	'$' or '#'.  If <data> starts with two characters followed by
	':', then the existing stubs interpret this as a sequence number.

	CSUM1 and CSUM2 are ascii hex representation of an 8-bit 
	checksum of <data>, the most significant nibble is sent first.
	the hex digits 0-9,a-f are used.

   Receiver responds with:

	+	- if CSUM is correct and ready for next packet
	-	- if CSUM is incorrect

   <data> is as follows:
   All values are encoded in ascii hex digits.

	Request		Packet

	read registers  g
	reply		XX....X		Each byte of register data
					is described by two hex digits.
					Registers are in the internal order
					for GDB, and the bytes in a register
					are in the same order the machine uses.
			or ENN		for an error.

	write regs	GXX..XX		Each byte of register data
					is described by two hex digits.
	reply		OK		for success
			ENN		for an error

        write reg	Pn...=r...	Write register n... with value r...,
					which contains two hex digits for each
					byte in the register (target byte
					order).
	reply		OK		for success
			ENN		for an error
	(not supported by all stubs).

	read mem	mAA..AA,LLLL	AA..AA is address, LLLL is length.
	reply		XX..XX		XX..XX is mem contents
					Can be fewer bytes than requested
					if able to read only part of the data.
			or ENN		NN is errno

	write mem	MAA..AA,LLLL:XX..XX
					AA..AA is address,
					LLLL is number of bytes,
					XX..XX is data
	reply		OK		for success
			ENN		for an error (this includes the case
					where only part of the data was
					written).

	cont		cAA..AA		AA..AA is address to resume
					If AA..AA is omitted,
					resume at same address.

	step		sAA..AA		AA..AA is address to resume
					If AA..AA is omitted,
					resume at same address.

	last signal     ?               Reply the current reason for stopping.
                                        This is the same reply as is generated
					for step or cont : SAA where AA is the
					signal number.

	There is no immediate reply to step or cont.
	The reply comes when the machine stops.
	It is		SAA		AA is the "signal number"

	or...		TAAn...:r...;n:r...;n...:r...;
					AA = signal number
					n... = register number
					r... = register contents
	or...		WAA		The process exited, and AA is
					the exit status.  This is only
					applicable for certains sorts of
					targets.
	kill request	k

	toggle debug	d		toggle debug flag (see 386 & 68k stubs)
	reset		r		reset -- see sparc stub.
	reserved	<other>		On other requests, the stub should
					ignore the request and send an empty
					response ($#<checksum>).  This way
					we can extend the protocol and GDB
					can tell whether the stub it is
					talking to uses the old or the new.
	search		tAA:PP,MM	Search backwards starting at address
					AA for a match with pattern PP and
					mask MM.  PP and MM are 4 bytes.
					Not supported by all stubs.

	general query	qXXXX		Request info about XXXX.
	general set	QXXXX=yyyy	Set value of XXXX to yyyy.
	query sect offs	qOffsets	Get section offsets.  Reply is
					Text=xxx;Data=yyy;Bss=zzz
	console output	Otext		Send text to stdout.  Only comes from
					remote target.

	Responses can be run-length encoded to save space.  A '*' means that
	the next character is an ASCII encoding giving a repeat count which
	stands for that many repititions of the character preceding the '*'.
	The encoding is n+29, yielding a printable character where n >=3 
	(which is where rle starts to win).  Don't use an n > 126. 

	So 
	"0* " means the same as "0000".  */

#include <string.h>
#include <setjmp.h>



#define COND_BR_MASK	0xff00
#define UCOND_DBR_MASK	0xe000
#define UCOND_RBR_MASK	0xf0df
#define TRAPA_MASK		0xff00

#define COND_DISP		0x00ff
#define UCOND_DISP		0x0fff
#define UCOND_REG		0x0f00

#define BF_INSTR		0x8b00
#define BT_INSTR		0x8900
#define BRA_INSTR		0xa000
#define BSR_INSTR		0xb000
#define JMP_INSTR		0x402b
#define JSR_INSTR		0x400b
#define RTS_INSTR		0x000b
#define RTE_INSTR		0x002b
#define TRAPA_INSTR		0xc300

#define SSTEP_INSTR		0xc3ff

#define T_BIT_MASK		0x0001
/*
 * BUFMAX defines the maximum number of characters in inbound/outbound
 * buffers at least NUMREGBYTES*2 are needed for register packets
 */
#define BUFMAX 1024

/*
 * Number of bytes for registers
 */
#define NUMREGBYTES 112		/* 92 */

/*
 * typedef
 */
typedef void (*Function) ();

/*
 * Forward declarations
 */

static int hex (char);
static char *mem2hex (char *, char *, int);
static char *hex2mem (char *, char *, int);
static int hexToInt (char **, int *);
static void getpacket (char *);
static void putpacket (char *);
static void handle_buserror (void);
static int computeSignal (int exceptionVector);
static void handle_exception (int exceptionVector);
void init_serial();

void putDebugChar (char);
char getDebugChar (void);

/* These are in the file but in asm statements so the compiler can't see them */
void catch_exception_4 (void);
void catch_exception_6 (void);
void catch_exception_9 (void);
void catch_exception_10 (void);
void catch_exception_11 (void);
void catch_exception_32 (void);
void catch_exception_33 (void);
void catch_exception_255 (void);



#define catch_exception_random catch_exception_255 /* Treat all odd ones like 255 */

void breakpoint (void);


#define init_stack_size 8*1024  /* if you change this you should also modify BINIT */
#define stub_stack_size 8*1024

int init_stack[init_stack_size] __attribute__ ((section ("stack"))) = {0};
int stub_stack[stub_stack_size] __attribute__ ((section ("stack"))) = {0};

typedef struct
  {
    void (*func_cold) ();
    int *stack_cold;
    void (*func_warm) ();
    int *stack_warm;
    void (*(handler[256 - 4])) ();
  }
vec_type;


void INIT ();
void BINIT ();

/* When you link take care that this is at address 0 -
   or wherever your vbr points */

#define CPU_BUS_ERROR_VEC  9
#define DMA_BUS_ERROR_VEC 10
#define NMI_VEC           11
#define INVALID_INSN_VEC   4
#define INVALID_SLOT_VEC   6
#define TRAP_VEC          32
#define IO_VEC            33
#define USER_VEC         255


#define BCR  (*(volatile short *)(0x05FFFFA0)) /* Bus control register */
#define BAS  (0x800)				/* Byte access select */
#define WCR1 (*(volatile short *)(0x05ffffA2)) /* Wait state control register */

const vec_type vectable =
{ 
  &BINIT,			/* 0: Power-on reset PC */
  init_stack + init_stack_size, /* 1: Power-on reset SP */
  &BINIT,			/* 2: Manual reset PC */
  init_stack + init_stack_size, /* 3: Manual reset SP */
{
  &catch_exception_4,		/* 4: General invalid instruction */
  &catch_exception_random,	/* 5: Reserved for system */
  &catch_exception_6,		/* 6: Invalid slot instruction */
  &catch_exception_random,	/* 7: Reserved for system */
  &catch_exception_random,	/* 8: Reserved for system */
  &catch_exception_9,		/* 9: CPU bus error */
  &catch_exception_10,		/* 10: DMA bus error */
  &catch_exception_11,		/* 11: NMI */
  &catch_exception_random,	/* 12: User break */
  &catch_exception_random,	/* 13: Reserved for system */
  &catch_exception_random,	/* 14: Reserved for system */
  &catch_exception_random,	/* 15: Reserved for system */
  &catch_exception_random,	/* 16: Reserved for system */
  &catch_exception_random,	/* 17: Reserved for system */
  &catch_exception_random,	/* 18: Reserved for system */
  &catch_exception_random,	/* 19: Reserved for system */
  &catch_exception_random,	/* 20: Reserved for system */
  &catch_exception_random,	/* 21: Reserved for system */
  &catch_exception_random,	/* 22: Reserved for system */
  &catch_exception_random,	/* 23: Reserved for system */
  &catch_exception_random,	/* 24: Reserved for system */
  &catch_exception_random,	/* 25: Reserved for system */
  &catch_exception_random,	/* 26: Reserved for system */
  &catch_exception_random,	/* 27: Reserved for system */
  &catch_exception_random,	/* 28: Reserved for system */
  &catch_exception_random,	/* 29: Reserved for system */
  &catch_exception_random,	/* 30: Reserved for system */
  &catch_exception_random,	/* 31: Reserved for system */
  &catch_exception_32,		/* 32: Trap instr (user vectors) */
  &catch_exception_33,		/* 33: Trap instr (user vectors) */
  &catch_exception_random,	/* 34: Trap instr (user vectors) */
  &catch_exception_random,	/* 35: Trap instr (user vectors) */
  &catch_exception_random,	/* 36: Trap instr (user vectors) */
  &catch_exception_random,	/* 37: Trap instr (user vectors) */
  &catch_exception_random,	/* 38: Trap instr (user vectors) */
  &catch_exception_random,	/* 39: Trap instr (user vectors) */
  &catch_exception_random,	/* 40: Trap instr (user vectors) */
  &catch_exception_random,	/* 41: Trap instr (user vectors) */
  &catch_exception_random,	/* 42: Trap instr (user vectors) */
  &catch_exception_random,	/* 43: Trap instr (user vectors) */
  &catch_exception_random,	/* 44: Trap instr (user vectors) */
  &catch_exception_random,	/* 45: Trap instr (user vectors) */
  &catch_exception_random,	/* 46: Trap instr (user vectors) */
  &catch_exception_random,	/* 47: Trap instr (user vectors) */
  &catch_exception_random,	/* 48: Trap instr (user vectors) */
  &catch_exception_random,	/* 49: Trap instr (user vectors) */
  &catch_exception_random,	/* 50: Trap instr (user vectors) */
  &catch_exception_random,	/* 51: Trap instr (user vectors) */
  &catch_exception_random,	/* 52: Trap instr (user vectors) */
  &catch_exception_random,	/* 53: Trap instr (user vectors) */
  &catch_exception_random,	/* 54: Trap instr (user vectors) */
  &catch_exception_random,	/* 55: Trap instr (user vectors) */
  &catch_exception_random,	/* 56: Trap instr (user vectors) */
  &catch_exception_random,	/* 57: Trap instr (user vectors) */
  &catch_exception_random,	/* 58: Trap instr (user vectors) */
  &catch_exception_random,	/* 59: Trap instr (user vectors) */
  &catch_exception_random,	/* 60: Trap instr (user vectors) */
  &catch_exception_random,	/* 61: Trap instr (user vectors) */
  &catch_exception_random,	/* 62: Trap instr (user vectors) */
  &catch_exception_random,	/* 63: Trap instr (user vectors) */
  &catch_exception_random,	/* 64: IRQ0 */
  &catch_exception_random,	/* 65: IRQ1 */
  &catch_exception_random,	/* 66: IRQ2 */
  &catch_exception_random,	/* 67: IRQ3 */
  &catch_exception_random,	/* 68: IRQ4 */
  &catch_exception_random,	/* 69: IRQ5 */
  &catch_exception_random,	/* 70: IRQ6 */
  &catch_exception_random,	/* 71: IRQ7 */
  &catch_exception_random,
  &catch_exception_random,
  &catch_exception_random,
  &catch_exception_random,
  &catch_exception_random,
  &catch_exception_random,
  &catch_exception_random,
  &catch_exception_random,
  &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_random,
     &catch_exception_255}};


char in_nmi;   /* Set when handling an NMI, so we don't reenter */
int dofault;  /* Non zero, bus errors will raise exception */

int *stub_sp;

/* debug > 0 prints ill-formed commands in valid packets & checksum errors */
int remote_debug;

/* jump buffer used for setjmp/longjmp */
jmp_buf remcomEnv;

enum regnames
  {
    R0, R1, R2, R3, R4, R5, R6, R7,
    R8, R9, R10, R11, R12, R13, R14,
    R15, PC, PR, GBR, VBR, MACH, MACL, SR,
    TICKS, STALLS, CYCLES, INSTS, PLR
  };

typedef struct
  {
    short *memAddr;
    short oldInstr;
  }
stepData;

int registers[NUMREGBYTES / 4];
stepData instrBuffer;
char stepped;
static const char hexchars[] = "0123456789abcdef";
char remcomInBuffer[BUFMAX];
char remcomOutBuffer[BUFMAX];

char highhex(int  x)
{
  return hexchars[(x >> 4) & 0xf];
}

char lowhex(int  x)
{
  return hexchars[x & 0xf];
}

/*
 * Assembly macros
 */

#define BREAKPOINT()   asm("trapa	#0x20"::);


/*
 * Routines to handle hex data
 */

static int
hex (char ch)
{
  if ((ch >= 'a') && (ch <= 'f'))
    return (ch - 'a' + 10);
  if ((ch >= '0') && (ch <= '9'))
    return (ch - '0');
  if ((ch >= 'A') && (ch <= 'F'))
    return (ch - 'A' + 10);
  return (-1);
}

/* convert the memory, pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
static char *
mem2hex (char *mem, char *buf, int count)
{
  int i;
  int ch;
  for (i = 0; i < count; i++)
    {
      ch = *mem++;
      *buf++ = highhex (ch);
      *buf++ = lowhex (ch);
    }
  *buf = 0;
  return (buf);
}

/* convert the hex array pointed to by buf into binary, to be placed in mem */
/* return a pointer to the character after the last byte written */

static char *
hex2mem (char *buf, char *mem, int count)
{
  int i;
  unsigned char ch;
  for (i = 0; i < count; i++)
    {
      ch = hex (*buf++) << 4;
      ch = ch + hex (*buf++);
      *mem++ = ch;
    }
  return (mem);
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
      hexValue = hex (**ptr);
      if (hexValue >= 0)
	{
	  *intValue = (*intValue << 4) | hexValue;
	  numChars++;
	}
      else
	break;

      (*ptr)++;
    }

  return (numChars);
}

/*
 * Routines to get and put packets
 */

/* scan for the sequence $<data>#<checksum>     */

static
void
getpacket (char *buffer)
{
  unsigned char checksum;
  unsigned char xmitcsum;
  int i;
  int count;
  char ch;
  do
    {
      /* wait around for the start character, ignore all other characters */
      while ((ch = getDebugChar ()) != '$');
      checksum = 0;
      xmitcsum = -1;

      count = 0;

      /* now, read until a # or end of buffer is found */
      while (count < BUFMAX)
	{
	  ch = getDebugChar ();
	  if (ch == '#')
	    break;
	  checksum = checksum + ch;
	  buffer[count] = ch;
	  count = count + 1;
	}
      buffer[count] = 0;

      if (ch == '#')
	{
	  xmitcsum = hex (getDebugChar ()) << 4;
	  xmitcsum += hex (getDebugChar ());
	  if (checksum != xmitcsum)
	    putDebugChar ('-');	/* failed checksum */
	  else
	    {
	      putDebugChar ('+');	/* successful transfer */
	      /* if a sequence char is present, reply the sequence ID */
	      if (buffer[2] == ':')
		{
		  putDebugChar (buffer[0]);
		  putDebugChar (buffer[1]);
		  /* remove sequence chars from buffer */
		  count = strlen (buffer);
		  for (i = 3; i <= count; i++)
		    buffer[i - 3] = buffer[i];
		}
	    }
	}
    }
  while (checksum != xmitcsum);

}


/* send the packet in buffer.  The host get's one chance to read it.
   This routine does not wait for a positive acknowledge.  */

static void
putpacket (register char *buffer)
{
  register  int checksum;
  register  int count;

  /*  $<packet info>#<checksum>. */
  do
    {
      char *src = buffer;
      putDebugChar ('$');
      checksum = 0;

      while (*src)
	{
	  int runlen;

	  /* Do run length encoding */
	  for (runlen = 0; runlen < 100; runlen ++) 
	    {
	      if (src[0] != src[runlen]) 
		{
		  if (runlen > 3) 
		    {
		      int encode;
		      /* Got a useful amount */
		      putDebugChar (*src);
		      checksum += *src;
		      putDebugChar ('*');
		      checksum += '*';
		      checksum += (encode = runlen + ' ' - 4);
		      putDebugChar (encode);
		      src += runlen;
		    }
		  else
		    {
		      putDebugChar (*src);
		      checksum += *src;
		      src++;
		    }
		  break;
		}
	    }
	}


      putDebugChar ('#');
      putDebugChar (highhex(checksum));
      putDebugChar (lowhex(checksum));
    }
  while  (getDebugChar() != '+');

}


/* a bus error has occurred, perform a longjmp
   to return execution and allow handling of the error */

void
handle_buserror (void)
{
  longjmp (remcomEnv, 1);
}

/*
 * this function takes the SH-1 exception number and attempts to
 * translate this number into a unix compatible signal value
 */
static int
computeSignal (int exceptionVector)
{
  int sigval;
  switch (exceptionVector)
    {
    case INVALID_INSN_VEC:
      sigval = 4;
      break;			
    case INVALID_SLOT_VEC:
      sigval = 4;
      break;			
    case CPU_BUS_ERROR_VEC:
      sigval = 10;
      break;			
    case DMA_BUS_ERROR_VEC:
      sigval = 10;
      break;	
    case NMI_VEC:
      sigval = 2;
      break;	

    case TRAP_VEC:
    case USER_VEC:
      sigval = 5;
      break;

    default:
      sigval = 7;		/* "software generated"*/
      break;
    }
  return (sigval);
}

void
doSStep (void)
{
  short *instrMem;
  int displacement;
  int reg;
  unsigned short opcode;

  instrMem = (short *) registers[PC];

  opcode = *instrMem;
  stepped = 1;

  if ((opcode & COND_BR_MASK) == BT_INSTR)
    {
      if (registers[SR] & T_BIT_MASK)
	{
	  displacement = (opcode & COND_DISP) << 1;
	  if (displacement & 0x80)
	    displacement |= 0xffffff00;
	  /*
		   * Remember PC points to second instr.
		   * after PC of branch ... so add 4
		   */
	  instrMem = (short *) (registers[PC] + displacement + 4);
	}
      else
	instrMem += 1;
    }
  else if ((opcode & COND_BR_MASK) == BF_INSTR)
    {
      if (registers[SR] & T_BIT_MASK)
	instrMem += 1;
      else
	{
	  displacement = (opcode & COND_DISP) << 1;
	  if (displacement & 0x80)
	    displacement |= 0xffffff00;
	  /*
		   * Remember PC points to second instr.
		   * after PC of branch ... so add 4
		   */
	  instrMem = (short *) (registers[PC] + displacement + 4);
	}
    }
  else if ((opcode & UCOND_DBR_MASK) == BRA_INSTR)
    {
      displacement = (opcode & UCOND_DISP) << 1;
      if (displacement & 0x0800)
	displacement |= 0xfffff000;

      /*
	   * Remember PC points to second instr.
	   * after PC of branch ... so add 4
	   */
      instrMem = (short *) (registers[PC] + displacement + 4);
    }
  else if ((opcode & UCOND_RBR_MASK) == JSR_INSTR)
    {
      reg = (char) ((opcode & UCOND_REG) >> 8);

      instrMem = (short *) registers[reg];
    }
  else if (opcode == RTS_INSTR)
    instrMem = (short *) registers[PR];
  else if (opcode == RTE_INSTR)
    instrMem = (short *) registers[15];
  else if ((opcode & TRAPA_MASK) == TRAPA_INSTR)
    instrMem = (short *) ((opcode & ~TRAPA_MASK) << 2);
  else
    instrMem += 1;

  instrBuffer.memAddr = instrMem;
  instrBuffer.oldInstr = *instrMem;
  *instrMem = SSTEP_INSTR;
}


/* Undo the effect of a previous doSStep.  If we single stepped,
   restore the old instruction. */

void
undoSStep (void)
{
  if (stepped)
    {  short *instrMem;
      instrMem = instrBuffer.memAddr;
      *instrMem = instrBuffer.oldInstr;
    }
  stepped = 0;
}

/*
This function does all exception handling.  It only does two things -
it figures out why it was called and tells gdb, and then it reacts
to gdb's requests.

When in the monitor mode we talk a human on the serial line rather than gdb.

*/


void
gdb_handle_exception (int exceptionVector)
{
  int sigval;
  int addr, length;
  char *ptr;

  /* reply to host that an exception has occurred */
  sigval = computeSignal (exceptionVector);
  remcomOutBuffer[0] = 'S';
  remcomOutBuffer[1] = highhex(sigval);
  remcomOutBuffer[2] = lowhex (sigval);
  remcomOutBuffer[3] = 0;

  putpacket (remcomOutBuffer);

  /*
   * exception 255 indicates a software trap
   * inserted in place of code ... so back up
   * PC by one instruction, since this instruction
   * will later be replaced by its original one!
   */
  if (exceptionVector == 0xff
      || exceptionVector == 0x20)
    registers[PC] -= 2;

  /*
   * Do the thangs needed to undo
   * any stepping we may have done!
   */
  undoSStep ();

  while (1)
    {
      remcomOutBuffer[0] = 0;
      getpacket (remcomInBuffer);

      switch (remcomInBuffer[0])
	{
	case '?':
	  remcomOutBuffer[0] = 'S';
	  remcomOutBuffer[1] = highhex (sigval);
	  remcomOutBuffer[2] = lowhex (sigval);
	  remcomOutBuffer[3] = 0;
	  break;
	case 'd':
	  remote_debug = !(remote_debug);	/* toggle debug flag */
	  break;
	case 'g':		/* return the value of the CPU registers */
	  mem2hex ((char *) registers, remcomOutBuffer, NUMREGBYTES);
	  break;
	case 'G':		/* set the value of the CPU registers - return OK */
	  hex2mem (&remcomInBuffer[1], (char *) registers, NUMREGBYTES);
	  strcpy (remcomOutBuffer, "OK");
	  break;

	  /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
	case 'm':
	  if (setjmp (remcomEnv) == 0)
	    {
	      dofault = 0;
	      /* TRY, TO READ %x,%x.  IF SUCCEED, SET PTR = 0 */
	      ptr = &remcomInBuffer[1];
	      if (hexToInt (&ptr, &addr))
		if (*(ptr++) == ',')
		  if (hexToInt (&ptr, &length))
		    {
		      ptr = 0;
		      mem2hex ((char *) addr, remcomOutBuffer, length);
		    }
	      if (ptr)
		strcpy (remcomOutBuffer, "E01");
	    }
	  else
	    strcpy (remcomOutBuffer, "E03");

	  /* restore handler for bus error */
	  dofault = 1;
	  break;

	  /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
	case 'M':
	  if (setjmp (remcomEnv) == 0)
	    {
	      dofault = 0;

	      /* TRY, TO READ '%x,%x:'.  IF SUCCEED, SET PTR = 0 */
	      ptr = &remcomInBuffer[1];
	      if (hexToInt (&ptr, &addr))
		if (*(ptr++) == ',')
		  if (hexToInt (&ptr, &length))
		    if (*(ptr++) == ':')
		      {
			hex2mem (ptr, (char *) addr, length);
			ptr = 0;
			strcpy (remcomOutBuffer, "OK");
		      }
	      if (ptr)
		strcpy (remcomOutBuffer, "E02");
	    }
	  else
	    strcpy (remcomOutBuffer, "E03");

	  /* restore handler for bus error */
	  dofault = 1;
	  break;

	  /* cAA..AA    Continue at address AA..AA(optional) */
	  /* sAA..AA   Step one instruction from AA..AA(optional) */
	case 'c':
	case 's':
	  {
	    /* tRY, to read optional parameter, pc unchanged if no parm */
	    ptr = &remcomInBuffer[1];
	    if (hexToInt (&ptr, &addr))
	      registers[PC] = addr;

	    if (remcomInBuffer[0] == 's')
	      doSStep ();
	  }
	  return;
	  break;

	  /* kill the program */
	case 'k':		/* do nothing */
	  break;
	}			/* switch */

      /* reply to the request */
      putpacket (remcomOutBuffer);
    }
}


#define GDBCOOKIE 0x5ac 
static int ingdbmode;
/* We've had an exception - choose to go into the monitor or
   the gdb stub */
void handle_exception(int exceptionVector)
{
#ifdef MONITOR
    if (ingdbmode != GDBCOOKIE)
      monitor_handle_exception (exceptionVector);
    else 
#endif
      gdb_handle_exception (exceptionVector);

}

void
gdb_mode()
{
  ingdbmode = GDBCOOKIE;
  breakpoint();
}
/* This function will generate a breakpoint exception.  It is used at the
   beginning of a program to sync up with a debugger and can be used
   otherwise as a quick means to stop program execution and "break" into
   the debugger. */

void
breakpoint (void)
{
      BREAKPOINT ();
}

asm ("_BINIT: mov.l  L1,r15");
asm ("bra _INIT");
asm ("nop");
asm ("L1: .long _init_stack + 8*1024*4");
void
INIT (void)
{
  /* First turn on the ram */
  WCR1  = 0;    /* Never sample wait */
  BCR = BAS;    /* use lowbyte/high byte */

  init_serial();

#ifdef MONITOR
  reset_hook ();
#endif


  in_nmi = 0;
  dofault = 1;
  stepped = 0;

  stub_sp = stub_stack + stub_stack_size;
  breakpoint ();

  while (1)
    ;
}


static void sr()
{


  /* Calling Reset does the same as pressing the button */
  asm (".global _Reset
        .global _WarmReset
_Reset:
_WarmReset:
         mov.l L_sp,r15
         bra   _INIT
         nop
         .align 2
L_sp:    .long _init_stack + 8000");

  asm("saveRegisters:
	mov.l	@(L_reg, pc), r0
	mov.l	@r15+, r1				! pop R0
	mov.l	r2, @(0x08, r0)				! save R2
	mov.l	r1, @r0					! save R0
	mov.l	@r15+, r1				! pop R1
	mov.l	r3, @(0x0c, r0)				! save R3
	mov.l	r1, @(0x04, r0)				! save R1
	mov.l	r4, @(0x10, r0)				! save R4
	mov.l	r5, @(0x14, r0)				! save R5
	mov.l	r6, @(0x18, r0)				! save R6
	mov.l	r7, @(0x1c, r0)				! save R7
	mov.l	r8, @(0x20, r0)				! save R8
	mov.l	r9, @(0x24, r0)				! save R9
	mov.l	r10, @(0x28, r0)			! save R10
	mov.l	r11, @(0x2c, r0)			! save R11
	mov.l	r12, @(0x30, r0)			! save R12
	mov.l	r13, @(0x34, r0)			! save R13
	mov.l	r14, @(0x38, r0)			! save R14
	mov.l	@r15+, r4				! save arg to handleException
	add	#8, r15					! hide PC/SR values on stack
	mov.l	r15, @(0x3c, r0)			! save R15
	add	#-8, r15				! save still needs old SP value
	add	#92, r0					! readjust register pointer
	mov	r15, r2
	add	#4, r2
	mov.l	@r2, r2					! R2 has SR
	mov.l	@r15, r1				! R1 has PC
	mov.l	r2, @-r0				! save SR
	sts.l	macl, @-r0				! save MACL
	sts.l	mach, @-r0				! save MACH
	stc.l	vbr, @-r0				! save VBR
	stc.l	gbr, @-r0				! save GBR
	sts.l	pr, @-r0				! save PR
	mov.l	@(L_stubstack, pc), r2
	mov.l	@(L_hdl_except, pc), r3
	mov.l	@r2, r15
	jsr	@r3
	mov.l	r1, @-r0				! save PC
	mov.l	@(L_stubstack, pc), r0
	mov.l	@(L_reg, pc), r1
	bra	restoreRegisters
	mov.l	r15, @r0				! save __stub_stack
	
	.align 2
L_reg:
	.long	_registers
L_stubstack:
	.long	_stub_sp
L_hdl_except:
	.long	_handle_exception");

}

static void rr()
{
asm("
	.align 2	
        .global _resume
_resume:
	mov	r4,r1
restoreRegisters:
	add	#8, r1						! skip to R2
	mov.l	@r1+, r2					! restore R2
	mov.l	@r1+, r3					! restore R3
	mov.l	@r1+, r4					! restore R4
	mov.l	@r1+, r5					! restore R5
	mov.l	@r1+, r6					! restore R6
	mov.l	@r1+, r7					! restore R7
	mov.l	@r1+, r8					! restore R8
	mov.l	@r1+, r9					! restore R9
	mov.l	@r1+, r10					! restore R10
	mov.l	@r1+, r11					! restore R11
	mov.l	@r1+, r12					! restore R12
	mov.l	@r1+, r13					! restore R13
	mov.l	@r1+, r14					! restore R14
	mov.l	@r1+, r15					! restore programs stack
	mov.l	@r1+, r0
	add	#-8, r15					! uncover PC/SR on stack 
	mov.l	r0, @r15					! restore PC onto stack
	lds.l	@r1+, pr					! restore PR
	ldc.l	@r1+, gbr					! restore GBR		
	ldc.l	@r1+, vbr					! restore VBR
	lds.l	@r1+, mach					! restore MACH
	lds.l	@r1+, macl					! restore MACL
	mov.l	@r1, r0	
	add	#-88, r1					! readjust reg pointer to R1
	mov.l	r0, @(4, r15)					! restore SR onto stack+4
	mov.l	r2, @-r15
	mov.l	L_in_nmi, r0
	mov		#0, r2
	mov.b	r2, @r0
	mov.l	@r15+, r2
	mov.l	@r1+, r0					! restore R0
	rte
	mov.l	@r1, r1						! restore R1

");
}


static __inline__ void code_for_catch_exception(int n) 
{
  asm("		.globl	_catch_exception_%O0" : : "i" (n) 				); 
  asm("	_catch_exception_%O0:" :: "i" (n)      						);

  asm("		add	#-4, r15 				! reserve spot on stack ");
  asm("		mov.l	r1, @-r15				! push R1		");

  if (n == NMI_VEC) 
    {
      /* Special case for NMI - make sure that they don't nest */
      asm("	mov.l	r0, @-r15					! push R0");
      asm("	mov.l	L_in_nmi, r0");
      asm("	tas.b	@r0						! Fend off against addtnl NMIs");
      asm("	bt		noNMI");
      asm("	mov.l	@r15+, r0");
      asm("	mov.l	@r15+, r1");
      asm("	add		#4, r15");
      asm("	rte");
      asm("	nop");
      asm(".align 2");
      asm("L_in_nmi: .long	_in_nmi");
      asm("noNMI:");
    }
  else
    {

      if (n == CPU_BUS_ERROR_VEC)
	{
	  /* Exception 9 (bus errors) are disasbleable - so that you
	     can probe memory and get zero instead of a fault.
	     Because the vector table may be in ROM we don't revector
	     the interrupt like all the other stubs, we check in here
	     */
	  asm("mov.l	L_dofault,r1");
	  asm("mov.l	@r1,r1");
	  asm("tst	r1,r1");
	  asm("bf	faultaway");
	  asm("bsr	_handle_buserror");
	  asm(".align	2");
	  asm("L_dofault: .long _dofault");
	  asm("faultaway:");
	}
      asm("		mov	#15<<4, r1							");
      asm("		ldc	r1, sr					! disable interrupts	");
      asm("		mov.l	r0, @-r15				! push R0		");
    }

  /* Prepare for saving context, we've already pushed r0 and r1, stick exception number
     into the frame */
  asm("		mov	r15, r0								");
  asm("		add	#8, r0								");
  asm("		mov	%0,r1" :: "i" (n)        					);
  asm("		extu.b  r1,r1								");
  asm("		bra	saveRegisters				! save register values	");
  asm("		mov.l	r1, @r0					! save exception # 	");
}


static  void
exceptions()
{
  code_for_catch_exception (CPU_BUS_ERROR_VEC);
  code_for_catch_exception (DMA_BUS_ERROR_VEC);
  code_for_catch_exception (INVALID_INSN_VEC);
  code_for_catch_exception (INVALID_SLOT_VEC);
  code_for_catch_exception (NMI_VEC);
  code_for_catch_exception (TRAP_VEC);
  code_for_catch_exception (USER_VEC);
  code_for_catch_exception (IO_VEC);
}






/* Support for Serial I/O using on chip uart */

#define SMR0 (*(volatile char *)(0x05FFFEC0)) /* Channel 0  serial mode register */
#define BRR0 (*(volatile char *)(0x05FFFEC1)) /* Channel 0  bit rate register */
#define SCR0 (*(volatile char *)(0x05FFFEC2)) /* Channel 0  serial control register */
#define TDR0 (*(volatile char *)(0x05FFFEC3)) /* Channel 0  transmit data register */
#define SSR0 (*(volatile char *)(0x05FFFEC4)) /* Channel 0  serial status register */
#define RDR0 (*(volatile char *)(0x05FFFEC5)) /* Channel 0  receive data register */

#define SMR1 (*(volatile char *)(0x05FFFEC8)) /* Channel 1  serial mode register */
#define BRR1 (*(volatile char *)(0x05FFFEC9)) /* Channel 1  bit rate register */
#define SCR1 (*(volatile char *)(0x05FFFECA)) /* Channel 1  serial control register */
#define TDR1 (*(volatile char *)(0x05FFFECB)) /* Channel 1  transmit data register */
#define SSR1 (*(volatile char *)(0x05FFFECC)) /* Channel 1  serial status register */
#define RDR1 (*(volatile char *)(0x05FFFECD)) /* Channel 1  receive data register */

/*
 * Serial mode register bits
 */

#define SYNC_MODE 		0x80
#define SEVEN_BIT_DATA 		0x40
#define PARITY_ON		0x20
#define ODD_PARITY		0x10
#define STOP_BITS_2		0x08
#define ENABLE_MULTIP		0x04
#define PHI_64			0x03
#define PHI_16			0x02
#define PHI_4			0x01

/*
 * Serial control register bits
 */
#define SCI_TIE				0x80	/* Transmit interrupt enable */
#define SCI_RIE				0x40	/* Receive interrupt enable */
#define SCI_TE				0x20	/* Transmit enable */
#define SCI_RE				0x10	/* Receive enable */
#define SCI_MPIE			0x08	/* Multiprocessor interrupt enable */
#define SCI_TEIE			0x04	/* Transmit end interrupt enable */
#define SCI_CKE1			0x02	/* Clock enable 1 */
#define SCI_CKE0			0x01	/* Clock enable 0 */

/*
 * Serial status register bits
 */
#define SCI_TDRE			0x80	/* Transmit data register empty */
#define SCI_RDRF			0x40	/* Receive data register full */
#define SCI_ORER			0x20	/* Overrun error */
#define SCI_FER				0x10	/* Framing error */
#define SCI_PER				0x08	/* Parity error */
#define SCI_TEND			0x04	/* Transmit end */
#define SCI_MPB				0x02	/* Multiprocessor bit */
#define SCI_MPBT			0x01	/* Multiprocessor bit transfer */


/*
 * Port B IO Register (PBIOR)
 */
#define	PBIOR 		(*(volatile char *)(0x05FFFFC6))
#define	PB15IOR 	0x8000
#define	PB14IOR 	0x4000
#define	PB13IOR 	0x2000
#define	PB12IOR 	0x1000
#define	PB11IOR 	0x0800
#define	PB10IOR 	0x0400
#define	PB9IOR 		0x0200
#define	PB8IOR 		0x0100
#define	PB7IOR 		0x0080
#define	PB6IOR 		0x0040
#define	PB5IOR 		0x0020
#define	PB4IOR 		0x0010
#define	PB3IOR 		0x0008
#define	PB2IOR 		0x0004
#define	PB1IOR 		0x0002
#define	PB0IOR 		0x0001

/*
 * Port B Control Register (PBCR1)
 */
#define	PBCR1 		(*(volatile short *)(0x05FFFFCC))
#define	PB15MD1 	0x8000
#define	PB15MD0 	0x4000
#define	PB14MD1 	0x2000
#define	PB14MD0 	0x1000
#define	PB13MD1 	0x0800
#define	PB13MD0 	0x0400
#define	PB12MD1 	0x0200
#define	PB12MD0 	0x0100
#define	PB11MD1 	0x0080
#define	PB11MD0 	0x0040
#define	PB10MD1 	0x0020
#define	PB10MD0 	0x0010
#define	PB9MD1 		0x0008
#define	PB9MD0 		0x0004
#define	PB8MD1 		0x0002
#define	PB8MD0 		0x0001

#define	PB15MD 		PB15MD1|PB14MD0
#define	PB14MD 		PB14MD1|PB14MD0
#define	PB13MD 		PB13MD1|PB13MD0
#define	PB12MD 		PB12MD1|PB12MD0
#define	PB11MD 		PB11MD1|PB11MD0
#define	PB10MD 		PB10MD1|PB10MD0
#define	PB9MD 		PB9MD1|PB9MD0
#define	PB8MD 		PB8MD1|PB8MD0

#define PB_TXD1 	PB11MD1
#define PB_RXD1 	PB10MD1
#define PB_TXD0 	PB9MD1
#define PB_RXD0 	PB8MD1

/*
 * Port B Control Register (PBCR2)
 */
#define	PBCR2 	0x05FFFFCE
#define	PB7MD1 	0x8000
#define	PB7MD0 	0x4000
#define	PB6MD1 	0x2000
#define	PB6MD0 	0x1000
#define	PB5MD1 	0x0800
#define	PB5MD0 	0x0400
#define	PB4MD1 	0x0200
#define	PB4MD0 	0x0100
#define	PB3MD1 	0x0080
#define	PB3MD0 	0x0040
#define	PB2MD1 	0x0020
#define	PB2MD0 	0x0010
#define	PB1MD1 	0x0008
#define	PB1MD0 	0x0004
#define	PB0MD1 	0x0002
#define	PB0MD0 	0x0001
	
#define	PB7MD 	PB7MD1|PB7MD0
#define	PB6MD 	PB6MD1|PB6MD0
#define	PB5MD 	PB5MD1|PB5MD0
#define	PB4MD 	PB4MD1|PB4MD0
#define	PB3MD 	PB3MD1|PB3MD0
#define	PB2MD 	PB2MD1|PB2MD0
#define	PB1MD 	PB1MD1|PB1MD0
#define	PB0MD 	PB0MD1|PB0MD0


#ifdef MHZ
#define BPS			32 * 9600 * MHZ / ( BAUD * 10)
#else
#define BPS			32	/* 9600 for 10 Mhz */
#endif

void handleError (char theSSR);

void
nop ()
{

}
void 
init_serial()
{
  int i;

  /* Clear TE and RE in Channel 1's SCR   */
  SCR1 &= ~(SCI_TE | SCI_RE);

  /* Set communication to be async, 8-bit data, no parity, 1 stop bit and use internal clock */

  SMR1 = 0;
  BRR1 = BPS;

  SCR1 &= ~(SCI_CKE1 | SCI_CKE0);

  /* let the hardware settle */

  for (i = 0; i < 1000; i++)
    nop ();

  /* Turn on in and out */
  SCR1 |= SCI_RE | SCI_TE;

  /* Set the PFC to make RXD1 (pin PB8) an input pin and TXD1 (pin PB9) an output pin */
  PBCR1 &= ~(PB_TXD1 | PB_RXD1);
  PBCR1 |= PB_TXD1 | PB_RXD1;
}


int
getDebugCharReady (void)
{
  char mySSR;
  mySSR = SSR1 & ( SCI_PER | SCI_FER | SCI_ORER );
  if ( mySSR )
    handleError ( mySSR );
  return SSR1 & SCI_RDRF ;
}

char 
getDebugChar (void)
{
  char ch;
  char mySSR;

  while ( ! getDebugCharReady())
    ;

  ch = RDR1;
  SSR1 &= ~SCI_RDRF;

  mySSR = SSR1 & (SCI_PER | SCI_FER | SCI_ORER);

  if (mySSR)
    handleError (mySSR);

  return ch;
}

int 
putDebugCharReady()
{
  return (SSR1 & SCI_TDRE);
}

void
putDebugChar (char ch)
{
  while (!putDebugCharReady())
    ;

  /*
   * Write data into TDR and clear TDRE
   */
  TDR1 = ch;
  SSR1 &= ~SCI_TDRE;
}

void 
handleError (char theSSR)
{
  SSR1 &= ~(SCI_ORER | SCI_PER | SCI_FER);
}

