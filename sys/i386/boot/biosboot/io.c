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
 *	$Id: io.c,v 1.19 1996/04/30 23:43:25 bde Exp $
 */

#include "boot.h"
#include <machine/cpufunc.h>
#include <sys/reboot.h>

#define K_RDWR 		0x60		/* keyboard data & cmds (read/write) */
#define K_STATUS 	0x64		/* keyboard status */
#define K_CMD	 	0x64		/* keybd ctlr command (write-only) */

#define K_OBUF_FUL 	0x01		/* output buffer full */
#define K_IBUF_FUL 	0x02		/* input buffer full */

#define KC_CMD_WIN	0xd0		/* read  output port */
#define KC_CMD_WOUT	0xd1		/* write output port */
#define KB_A20		0xdf		/* enable A20,
					   enable output buffer full interrupt
					   enable data line
					   enable clock line */


/*
 * Gate A20 for high memory
 */
void
gateA20(void)
{
#ifdef	IBM_L40
	outb(0x92, 0x2);
#else	IBM_L40
	while (inb(K_STATUS) & K_IBUF_FUL);
	while (inb(K_STATUS) & K_OBUF_FUL)
		(void)inb(K_RDWR);

	outb(K_CMD, KC_CMD_WOUT);
	while (inb(K_STATUS) & K_IBUF_FUL);
	outb(K_RDWR, KB_A20);
	while (inb(K_STATUS) & K_IBUF_FUL);
#endif	IBM_L40
}

/* printf - only handles %d as decimal, %c as char, %s as string */

void
printf(const char *format, ...)
{
	int *dataptr = (int *)&format;
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
	if (c == '\n') {
		if (loadflags & RB_SERIAL)
			serial_putc('\r');
		else
			putc('\r');
	}
	if (loadflags & RB_SERIAL)
		serial_putc(c);
	else
		putc(c);
}

int
getchar(int in_buf)
{
	int c;

loop:
	if ((c = ((loadflags & RB_SERIAL) ? serial_getc() : getc())) == '\r')
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

#ifdef PROBE_KEYBOARD
/*
 * This routine uses an inb to an unused port, the time to execute that
 * inb is approximately 1.25uS.  This value is pretty constant across
 * all CPU's and all buses, with the exception of some PCI implentations
 * that do not forward this I/O adress to the ISA bus as they know it
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
		(void)inb(0x84);
}
#endif /* PROBE_KEYBOARD */

static __inline int
isch(void)
{
	int isc;

	/*
	 * Checking the keyboard has the side effect of enabling clock
	 * interrupts so that bios_tick works.  Check the keyboard to
	 * get this side effect even if we only want the serial status.
	 */
	isc = ischar();

	if (!(loadflags & RB_SERIAL))
		return (isc);
	return (serial_ischar());

}

static __inline unsigned
pword(unsigned physaddr)
{
	unsigned result;

	/*
	 * Give the fs prefix separately because gas omits it for
	 * "movl %fs:0x46c, %eax".
	 */
	__asm __volatile("fs; movl %1, %0" : "=r" (result)
			 : "m" (*(unsigned *)physaddr));
	return (result);
}

int
gets(char *buf)
{
#define bios_tick		pword(0x46c)
#define BIOS_TICK_MS		55
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
bcopy(const char *from, char *to, int len)
{
	while (len-- > 0)
		*to++ = *from++;
}

/* To quote Ken: "You are not expected to understand this." :) */

void
twiddle(void)
{
	putchar((char)tw_chars);
	tw_chars = (tw_chars >> 8) | ((tw_chars & (unsigned long)0xFF) << 24);
	putchar('\b');
}
