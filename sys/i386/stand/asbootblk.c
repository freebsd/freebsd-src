/*
 * sys/i386/stand/asbootblk.c
 *
 * Boot block for Adaptech 1542 SCSI
 *
 * April 10, 1992
 * Pace Willisson
 * pace@blitz.com
 *
 * Placed in the public domain with NO WARRANTIES, not even the
 * implied warranties for MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE.
 *
 * To compile:
 *
 *	cc -O -c -DRELOC=0x70000 asbootblk.c
 *	ld -N -T 7c00 asbootblk.o
 *
 * This should result in a file with 512 bytes of text and no initialized 
 * data.  Strip the 32 bit header and place in block 0.
 *
 * When run, this program copies at least the first 8 blocks of SCSI
 * target 0 to the address specified by RELOC, then jumps to the
 * address RELOC+1024 (skipping the boot block and disk label).  Usually,
 * disks have 512 bytes per block, but I don't think they ever have
 * less, and it wont hurt if they are bigger, as long as RELOC + 8*SIZE
 * is less than 0xa0000.
 *
 * This bootblock does not support fdisk partitions, and can only be used
 * as the master boot block.
 *
 *	from: 386BSD 0.1
 *	$Id: asbootblk.c,v 1.2 1993/10/16 18:49:20 rgrimes Exp $
 */

#include "param.h"
#include "disklabel.h"
#include "i386/isa/asreg.h"

/* RELOC should be defined with a -D flag to cc */

#define SECOND_LEVEL_BOOT_START (RELOC + 0x400)
#define READ_SIZE 8192

#define as_port 0x330
#define target 0


#define NBLOCKS (READ_SIZE / 512) /* how many logical blocks to read */


/* These are the parameters to pass to the second level boot */
#define dev 4 /* major device number of as driver in
		 i386/stand/conf.c and i386/i386/conf.c */
#define unit 0 /* partition number of root file system */
#define off 0 /* block offset of root file system */

/* inline i/o borrowed from Roell X server */
static __inline__ void
outb(port, val)
short port;
char val;
{
   __asm__ volatile("outb %%al, %1" : :"a" (val), "d" (port));
}

static __inline__ unsigned int
inb(port)
short port;
{
   unsigned int ret;
   __asm__ volatile("xorl %%eax, %%eax; inb %1, %%al"
		    : "=a" (ret) : "d" (port));
   return ret;
}

/* this code is linked at 0x7c00 and is loaded there by the BIOS */

asm ("
	/* we're running in 16 real mode, so normal assembly doesn't work */
bootbase:
	/* interrupts off */
	cli

	/* load gdt */
	.byte 0x2e,0x0f,0x01,0x16 /* lgdt %cs:$imm */
	.word _gdtarg + 2

	/* turn on protected mode */
	smsw	%ax
	orb	$1,%al
	lmsw	%ax

	/* flush prefetch queue and reload %cs */
	.byte 0xea /* ljmp $8, flush */
	.word flush
	.word 8

flush:
	/* now running in 32 bit mode */
	movl	$0x10,%eax
	movl	%ax,%ds
	movl	%ax,%es
	movl	%ax,%ss
 	movl	$0x7c00,%esp
	call	_main
"); /* end of asm */

const char gdt[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0xff, 0xff, 0, 0, 0, 0x9f, 0xcf, 0, /* code segment */
	0xff, 0xff, 0, 0, 0, 0x93, 0xcf, 0, /* data segment */
};

const struct {
	short filler;
	short size;
	const char *gdt;
} gdtarg = { 0, sizeof gdt - 1, gdt };

#define CRTBASE ((char *)0xb8000)
#define CHECKPOINT(x) (CRTBASE[0] = x)

volatile struct mailbox_entry mailbox[2];
const char ccb[] = {
	0, /* opcode: normal read/write */
	(target << 5) | 8, /* target num and read flag */
	10, /* scsi cmd len */
	1, /* no automatic request for sense */
	READ_SIZE >> 16, /* data length */
	READ_SIZE >> 8,
	READ_SIZE,
	RELOC >> 16, /* data pointer */
	RELOC >> 8,
	RELOC,
	0, 0, 0, /* link pointer */
	0, /* link id */
	0, /* host status */
	0, /* target status */
	0, 0, /* reserved */

	/* scsi cdb */
	0x28, /* read opcode */
	0, /* logical unit number */
	0, 0, 0, 0, /* logical block address */
	0,	/* reserved */
	0, NBLOCKS, /* transfer length */
	0, /* link control */
};

int (*f)();

main ()
{
	int i;
	extern char edata[], end[];
	char volatile * volatile p, *q;
	int physaddr;

	CHECKPOINT ('a');

	/* clear bss */
	for (p = edata; p < end; p++)
		*p = 0;

	f = (int (*)())SECOND_LEVEL_BOOT_START;

	/* dma setup: see page 5-31 in the Adaptech manual */
	/* this knows we are using drq 5 */
	outb (0xd6, 0xc1);
	outb (0xd4, 0x01);

	outb (as_port + AS_CONTROL, AS_CONTROL_SRST);

	/* delay a little */
	inb (0x84);
	
	while (inb (as_port + AS_STATUS) != (AS_STATUS_INIT | AS_STATUS_IDLE))
		;

	CHECKPOINT ('b');

	as_put_byte (AS_CMD_MAILBOX_INIT);
	as_put_byte (1); /* one mailbox out, one in */
	as_put_byte ((int)mailbox >> 16);
	as_put_byte ((int)mailbox >> 8);
	as_put_byte ((int)mailbox);

	while (inb (as_port + AS_STATUS) & AS_STATUS_INIT)
		;

	CHECKPOINT ('c');

	mailbox[0].msb = (int)ccb >> 16;
	mailbox[0].mid = (int)ccb >> 8;
	mailbox[0].lsb = (int)ccb;
	mailbox[0].cmd = 1;

	as_put_byte (AS_CMD_START_SCSI_COMMAND);

	/* wait for done */
	while (mailbox[1].cmd == 0)
		;

	CHECKPOINT ('d');

	if (mailbox[1].cmd != 1) {
		/* some error */
		CHECKPOINT ('X');
		while (1);
	}

	CHECKPOINT ('e');

	/* the optimazation that gcc uses when it knows we are jumpping
	 * to a constant address is broken, so we have to use a variable 
	 * here
	 */
	(*f)(dev, unit, off);
}

int
as_put_byte (val)
int val;
{
	while (inb (as_port + AS_STATUS) & AS_STATUS_CDF)
		;
	outb (as_port + AS_DATA_OUT, val);
}

asm ("
ebootblkcode:
	. = 510
	.byte 0x55
	.byte 0xaa
ebootblk: 			/* MUST BE EXACTLY 0x200 BIG FOR SURE */
");
