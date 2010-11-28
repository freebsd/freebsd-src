/*
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	from: Mach, Revision 2.2  92/04/04  11:35:57  rpd
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "boot.h"
#include <machine/cpufunc.h>
#include <sys/reboot.h>
#include <pc98/pc98/pc98_machdep.h>

static int getchar(int in_buf);

/*
 * Gate A20 for high memory
 */
void
gateA20(void)
{
	outb(0xf2, 0x00);
	outb(0xf6, 0x02);
}

/* printf - only handles %d as decimal, %c as char, %s as string */

void
printf(const char *format, ...)
{
	int *dataptr = (void *)&format;
	char c;

	dataptr++;
	while ((c = *format++))
		if (c != '%')
			putchar(c);
		else
			switch (c = *format++) {
			      case 'd': {
				      int num = *dataptr++;
				      char buf[10], *ptr = buf;
				      if (num<0) {
					      num = -num;
					      putchar('-');
				      }
				      do
					      *ptr++ = '0'+num%10;
				      while (num /= 10);
				      do
					      putchar(*--ptr);
				      while (ptr != buf);
				      break;
			      }
			      case 'x': {
				      unsigned int num = *dataptr++, dig;
				      char buf[8], *ptr = buf;
				      do
					      *ptr++ = (dig=(num&0xf)) > 9?
							'a' + dig - 10 :
							'0' + dig;
				      while (num >>= 4);
				      do
					      putchar(*--ptr);
				      while (ptr != buf);
				      break;
			      }
			      case 'c': putchar((*dataptr++)&0xff); break;
			      case 's': {
				      char *ptr = (char *)*dataptr++;
				      while ((c = *ptr++))
					      putchar(c);
				      break;
			      }
			}
}

void
putchar(int c)
{
	if (c == '\n')
		putchar('\r');
	if (loadflags & RB_DUAL) {
		putc(c);
		serial_putc(c);
	} else if (loadflags & RB_SERIAL)
		serial_putc(c);
	else
		putc(c);
}

static int
getchar(int in_buf)
{
	int c;

loop:
	if (loadflags & RB_DUAL) {
		if (ischar())
			c = getc();
		else if (serial_ischar())
			c = serial_getc();
		else
			goto loop;
	} else if (loadflags & RB_SERIAL)
		c = serial_getc();
	else
		c = getc();
	if (c == '\r')
		c = '\n';
	if (c == '\b') {
		if (in_buf != 0) {
			putchar('\b');
			putchar(' ');
		} else {
			goto loop;
		}
	}
	putchar(c);
	return(c);
}

/*
 * This routine uses an inb to an unused port, the time to execute that
 * inb is approximately 1.25uS.  This value is pretty constant across
 * all CPU's and all buses, with the exception of some PCI implentations
 * that do not forward this I/O address to the ISA bus as they know it
 * is not a valid ISA bus address, those machines execute this inb in
 * 60 nS :-(.
 *
 * XXX this should be converted to use bios_tick.
 */
void
delay1ms(void)
{
	int i = 800;

	while (--i >= 0)
	    (void)outb(0x5f,0);		/* about 600ns */
}

static int
isch(void)
{
	int isc;

	/*
	 * Checking the keyboard has the side effect of enabling clock
	 * interrupts so that bios_tick works.  Check the keyboard to
	 * get this side effect even if we only want the serial status.
	 */
	isc = ischar();

	if (loadflags & RB_DUAL) {
		if (isc != 0)
			return (isc);
	} else if (!(loadflags & RB_SERIAL))
		return (isc);
	return (serial_ischar());
}

static unsigned
pword(unsigned physaddr)
{
	static int counter = 0;
	int i;

	for (i = 0; i < 512; i++)
		(void)outb(0x5f, 0);

	return (counter++);
}

int
gets(char *buf)
{
#define bios_tick		pword(0x46c)
#define BIOS_TICK_MS		1
	unsigned initial_bios_tick;
	char *ptr=buf;

#if BOOTWAIT
	for (initial_bios_tick = bios_tick;
	     bios_tick - initial_bios_tick < BOOTWAIT / BIOS_TICK_MS;)
#endif
		if (isch())
			for (;;) {
				switch(*ptr = getchar(ptr - buf) & 0xff) {
				      case '\n':
				      case '\r':
					*ptr = '\0';
					return 1;
				      case '\b':
					if (ptr > buf) ptr--;
					continue;
				      default:
					ptr++;
				}
#if TIMEOUT + 0
#if !BOOTWAIT
#error "TIMEOUT without BOOTWAIT"
#endif
				for (initial_bios_tick = bios_tick;;) {
					if (isch())
						break;
					if (bios_tick - initial_bios_tick >=
					    TIMEOUT / BIOS_TICK_MS)
					return 0;
				}
#endif
			}
	return 0;
}

int
strcmp(const char *s1, const char *s2)
{
	while (*s1 == *s2) {
		if (!*s1++)
			return 0;
		s2++;
	}
	return 1;
}

void
memcpy(const void *from, void *to, size_t len)
{
	const char *fp = (const char *)from;
	char *tp = (char *)to;

	while (len-- > 0)
		*tp++ = *fp++;
}

/* To quote Ken: "You are not expected to understand this." :) */

void
twiddle(void)
{
	putchar((char)tw_chars);
	tw_chars = (tw_chars >> 8) | ((tw_chars & (unsigned long)0xFF) << 24);
	putchar('\b');
}

static unsigned short *Crtat = (unsigned short *)0;
static int row;
static int col;

void putc(int c)
{
	static unsigned short *crtat;
	unsigned char sys_type;
	unsigned short *cp;
	int i, pos;

	if (Crtat == 0) {
		sys_type = *(unsigned char *)V(0xA1501);
		if (sys_type & 0x08) {
			Crtat = (unsigned short *)V(0xE0000);
			crtat = Crtat;
			row = 31;
			col = 80;
		} else {
			Crtat = (unsigned short *)V(0xA0000);
			crtat = Crtat;
			row = 25;
			col = 80;
		}
	}

	switch(c) {
	case '\t':
		do {
			putc(' ');
		} while ((int)crtat % 16);
		break;
	case '\b':
		crtat--;
		break;
	case '\r':
		crtat -= (crtat - Crtat) % col;
		break;
	case '\n':
		crtat += col;
		break;
	default:
		*crtat = (c == 0x5c ? 0xfc : c);
		*(crtat++ + 0x1000) = 0xe1;
		break;
	}

	if (crtat >= Crtat + col * row) {
		cp = Crtat;
		for (i = 1; i < row; i++) {
			memcpy((void *)(cp+col), (void *)cp, col*2);
			cp += col;
		}
		for (i = 0; i < col; i++) {
			*cp++ = ' ';
		}
		crtat -= col;
	}
	pos = crtat - Crtat;
	while((inb(0x60) & 0x04) == 0) {}
	outb(0x62, 0x49);
	outb(0x60, pos & 0xff);
	outb(0x60, pos >> 8);
}

#ifdef SET_MACHINE_TYPE
void machine_check(void)
{
	int	ret;
	int	i;
	int	data = 0;
	
	/* PC98_SYSTEM_PARAMETER(0x501) */
	ret = ((*(unsigned char*)V(0xA1501)) & 0x08) >> 3;

	/* Wait V-SYNC */
	while (inb(0x60) & 0x20) {}
	while (!(inb(0x60) & 0x20)) {}

	/* ANK 'A' font */
	outb(0xa1, 0x00);
	outb(0xa3, 0x41);

	/* M_NORMAL, use CG window (all NEC OK)  */
	/* sum */
	for (i = 0; i < 4; i++) {
		data += *((unsigned long*)V(0xA4000) + i);/* 0xa4000 */
	}
	if (data == 0x6efc58fc) { /* DA data */
		ret |= M_NEC_PC98;
	} else {
		ret |= M_EPSON_PC98;
	}
	ret |= (inb(0x42) & 0x20) ? M_8M : 0;

	/* PC98_SYSTEM_PARAMETER(0x400) */
	if ((*(unsigned char*)V(0xA1400)) & 0x80) {
		ret |= M_NOTE;
	}
	if (ret & M_NEC_PC98) {
		/* PC98_SYSTEM_PARAMETER(0x458) */
		if ((*(unsigned char*)V(0xA1458)) & 0x80) {
			ret |= M_H98;
		} else {
			ret |= M_NOT_H98;
		}
	} else
		ret |= M_NOT_H98;

	(*(unsigned long *)V(0xA1620)) = ret;
}
#endif
