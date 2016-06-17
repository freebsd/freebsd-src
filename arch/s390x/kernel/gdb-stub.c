/*
 *  arch/s390/kernel/gdb-stub.c
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com),
 *
 *  Originally written by Glenn Engel, Lake Stevens Instrument Division
 *
 *  Contributed by HP Systems
 *
 *  Modified for SPARC by Stu Grossman, Cygnus Support.
 *
 *  Modified for Linux/MIPS (and MIPS in general) by Andreas Busse
 *  Send complaints, suggestions etc. to <andy@waldorf-gmbh.de>
 *
 *  Copyright (C) 1995 Andreas Busse
 */

/*
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a BREAK instruction.
 *
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
 */

#include <asm/gdb-stub.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/pgtable.h>
#include <asm/system.h>


/*
 * external low-level support routines
 */

extern int putDebugChar(char c);    /* write a single character      */
extern char getDebugChar(void);     /* read and return a single char */
extern void fltr_set_mem_err(void);
extern void trap_low(void);

/*
 * breakpoint and test functions
 */
extern void breakpoint(void);
extern void breakinst(void);

/*
 * local prototypes
 */

static void getpacket(char *buffer);
static void putpacket(char *buffer);
static int hex(unsigned char ch);
static int hexToInt(char **ptr, int *intValue);
static unsigned char *mem2hex(char *mem, char *buf, int count, int may_fault);


/*
 * BUFMAX defines the maximum number of characters in inbound/outbound buffers
 * at least NUMREGBYTES*2 are needed for register packets
 */
#define BUFMAX 2048

static char input_buffer[BUFMAX];
static char output_buffer[BUFMAX];
int gdb_stub_initialised = FALSE;	
static const char hexchars[]="0123456789abcdef";


/*
 * Convert ch from a hex digit to an int
 */
static int hex(unsigned char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch-'a'+10;
	if (ch >= '0' && ch <= '9')
		return ch-'0';
	if (ch >= 'A' && ch <= 'F')
		return ch-'A'+10;
	return -1;
}

/*
 * scan for the sequence $<data>#<checksum>
 */
static void getpacket(char *buffer)
{
	unsigned char checksum;
	unsigned char xmitcsum;
	int i;
	int count;
	unsigned char ch;

	do {
		/*
		 * wait around for the start character,
		 * ignore all other characters
		 */
		while ((ch = (getDebugChar() & 0x7f)) != '$') ;

		checksum = 0;
		xmitcsum = -1;
		count = 0;
	
		/*
		 * now, read until a # or end of buffer is found
		 */
		while (count < BUFMAX) {
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

		if (ch == '#') {
			xmitcsum = hex(getDebugChar() & 0x7f) << 4;
			xmitcsum |= hex(getDebugChar() & 0x7f);

			if (checksum != xmitcsum)
				putDebugChar('-');	/* failed checksum */
			else {
				putDebugChar('+'); /* successful transfer */

				/*
				 * if a sequence char is present,
				 * reply the sequence ID
				 */
				if (buffer[2] == ':') {
					putDebugChar(buffer[0]);
					putDebugChar(buffer[1]);

					/*
					 * remove sequence chars from buffer
					 */
					count = strlen(buffer);
					for (i=3; i <= count; i++)
						buffer[i-3] = buffer[i];
				}
			}
		}
	}
	while (checksum != xmitcsum);
}

/*
 * send the packet in buffer.
 */
static void putpacket(char *buffer)
{
	unsigned char checksum;
	int count;
	unsigned char ch;

	/*
	 * $<packet info>#<checksum>.
	 */

	do {
		putDebugChar('$');
		checksum = 0;
		count = 0;

		while ((ch = buffer[count]) != 0) {
			if (!(putDebugChar(ch)))
				return;
			checksum += ch;
			count += 1;
		}

		putDebugChar('#');
		putDebugChar(hexchars[checksum >> 4]);
		putDebugChar(hexchars[checksum & 0xf]);

	}
	while ((getDebugChar() & 0x7f) != '+');
}



/*
 * Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null), in case of mem fault,
 * return 0.
 * If MAY_FAULT is non-zero, then we will handle memory faults by returning
 * a 0, else treat a fault like any other fault in the stub.
 */
static unsigned char *mem2hex(char *mem, char *buf, int count, int may_fault)
{
	unsigned char ch;

/*	set_mem_fault_trap(may_fault); */

	while (count-- > 0) {
		ch = *(mem++);
		if (mem_err)
			return 0;
		*buf++ = hexchars[ch >> 4];
		*buf++ = hexchars[ch & 0xf];
	}

	*buf = 0;

/*	set_mem_fault_trap(0); */

	return buf;
}

/*
 * convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written
 */
static char *hex2mem(char *buf, char *mem, int count, int may_fault)
{
	int i;
	unsigned char ch;

/*	set_mem_fault_trap(may_fault); */

	for (i=0; i<count; i++)
	{
		ch = hex(*buf++) << 4;
		ch |= hex(*buf++);
		*(mem++) = ch;
		if (mem_err)
			return 0;
	}

/*	set_mem_fault_trap(0); */

	return mem;
}



/*
 * Set up exception handlers for tracing and breakpoints
 */
void set_debug_traps(void)
{
//	unsigned long flags;
	unsigned char c;

//	save_and_cli(flags);
	/*
	 * In case GDB is started before us, ack any packets
	 * (presumably "$?#xx") sitting there.
	 */
	while((c = getDebugChar()) != '$');
	while((c = getDebugChar()) != '#');
	c = getDebugChar(); /* eat first csum byte */
	c = getDebugChar(); /* eat second csum byte */
	putDebugChar('+'); /* ack it */

	gdb_stub_initialised = TRUE;
//	restore_flags(flags);
}


/*
 * Trap handler for memory errors.  This just sets mem_err to be non-zero.  It
 * assumes that %l1 is non-zero.  This should be safe, as it is doubtful that
 * 0 would ever contain code that could mem fault.  This routine will skip
 * past the faulting instruction after setting mem_err.
 */
extern void fltr_set_mem_err(void)
{
  /* FIXME: Needs to be written... */
}


/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */
static int hexToInt(char **ptr, int *intValue)
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

void gdb_stub_get_non_pt_regs(gdb_pt_regs *regs)
{
	s390_fp_regs *fpregs=&regs->fp_regs;
	int has_ieee=save_fp_regs1(fpregs);

	if(!has_ieee)
	{
		fpregs->fpc=0;
		fpregs->fprs[1].d=
		fpregs->fprs[3].d=
		fpregs->fprs[5].d=
		fpregs->fprs[7].d=0;
		memset(&fpregs->fprs[8].d,0,sizeof(freg_t)*8);
	}
}

void gdb_stub_set_non_pt_regs(gdb_pt_regs *regs)
{
	restore_fp_regs1(&regs->fp_regs);
}

void gdb_stub_send_signal(int sigval)
{
	char *ptr;
	ptr = output_buffer;

	/*
	 * Send trap type (converted to signal)
	 */
	*ptr++ = 'S';
	*ptr++ = hexchars[sigval >> 4];
	*ptr++ = hexchars[sigval & 0xf];
	*ptr++ = 0;
	putpacket(output_buffer);	/* send it off... */
}

/*
 * This function does all command processing for interfacing to gdb.  It
 * returns 1 if you should skip the instruction at the trap address, 0
 * otherwise.
 */
void gdb_stub_handle_exception(gdb_pt_regs *regs,int sigval)
{
	int trap;			/* Trap type */
	int addr;
	int length;
	char *ptr;
	unsigned long *stack;

	
	/*
	 * reply to host that an exception has occurred
	 */
	send_signal(sigval);

	/*
	 * Wait for input from remote GDB
	 */
	while (1) {
		output_buffer[0] = 0;
		getpacket(input_buffer);

		switch (input_buffer[0])
		{
		case '?':
			send_signal(sigval);
			continue;

		case 'd':
			/* toggle debug flag */
			break;

		/*
		 * Return the value of the CPU registers
		 */
		case 'g':
			gdb_stub_get_non_pt_regs(regs);
			ptr = output_buffer;
			ptr=  mem2hex((char *)regs,ptr,sizeof(s390_regs_common),FALSE);
			ptr=  mem2hex((char *)&regs->crs[0],ptr,NUM_CRS*CR_SIZE,FALSE);
			ptr = mem2hex((char *)&regs->fp_regs, ptr,sizeof(s390_fp_regs));
			break;
	  
		/*
		 * set the value of the CPU registers - return OK
		 * FIXME: Needs to be written
		 */
		case 'G':
			ptr=input_buffer;
			hex2mem (ptr, (char *)regs,sizeof(s390_regs_common), FALSE);
			ptr+=sizeof(s390_regs_common)*2;
			hex2mem (ptr, (char *)regs->crs[0],NUM_CRS*CR_SIZE, FALSE);
			ptr+=NUM_CRS*CR_SIZE*2;
			hex2mem (ptr, (char *)regs->fp_regs,sizeof(s390_fp_regs), FALSE);
			gdb_stub_set_non_pt_regs(regs);
			strcpy(output_buffer,"OK");
		break;

		/*
		 * mAA..AA,LLLL  Read LLLL bytes at address AA..AA
		 */
		case 'm':
			ptr = &input_buffer[1];

			if (hexToInt(&ptr, &addr)
				&& *ptr++ == ','
				&& hexToInt(&ptr, &length)) {
				if (mem2hex((char *)addr, output_buffer, length, 1))
					break;
				strcpy (output_buffer, "E03");
			} else
				strcpy(output_buffer,"E01");
			break;

		/*
		 * MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK
		 */
		case 'M': 
			ptr = &input_buffer[1];

			if (hexToInt(&ptr, &addr)
				&& *ptr++ == ','
				&& hexToInt(&ptr, &length)
				&& *ptr++ == ':') {
				if (hex2mem(ptr, (char *)addr, length, 1))
					strcpy(output_buffer, "OK");
				else
					strcpy(output_buffer, "E03");
			}
			else
				strcpy(output_buffer, "E02");
			break;

		/*
		 * cAA..AA    Continue at address AA..AA(optional)
		 */
		case 'c':    
			/* try to read optional parameter, pc unchanged if no parm */

			ptr = &input_buffer[1];
			if (hexToInt(&ptr, &addr))
				regs->cp0_epc = addr;
	  
			/*
			 * Need to flush the instruction cache here, as we may
			 * have deposited a breakpoint, and the icache probably
			 * has no way of knowing that a data ref to some location
			 * may have changed something that is in the instruction
			 * cache.
			 * NB: We flush both caches, just to be sure...
			 */

			flush_cache_all();
			return;
			/* NOTREACHED */
			break;


		/*
		 * kill the program
		 */
		case 'k' :
			break;		/* do nothing */


		/*
		 * Reset the whole machine (FIXME: system dependent)
		 */
		case 'r':
			break;


		/*
		 * Step to next instruction
		 */
		case 's':
			/*
			 * There is no single step insn in the MIPS ISA, so we
			 * use breakpoints and continue, instead.
			 */
			single_step(regs);
			flush_cache_all();
			return;
			/* NOTREACHED */

		}
		break;

		}			/* switch */

		/*
		 * reply to the request
		 */

		putpacket(output_buffer);

	} /* while */
}

/*
 * This function will generate a breakpoint exception.  It is used at the
 * beginning of a program to sync up with a debugger and can be used
 * otherwise as a quick means to stop program execution and "break" into
 * the debugger.
 */
void breakpoint(void)
{
	if (!gdb_stub_initialised)
		return;
	__asm__ __volatile__(
			".globl	breakinst\n"
			"breakinst:\t.word   %0\n\t"
			:
			: "i" (S390_BREAKPOINT_U16)
				:
				);
}







