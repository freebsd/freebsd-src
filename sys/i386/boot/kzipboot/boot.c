/*
 * FreeBSD kernel unpacker.
 * 1993 by Serge Vakulenko
 * modified for FreeBSD 2.1 by Gary Jennejohn - 12FEB95
 */

#include <sys/types.h>
#include <sys/reboot.h>		/* for RB_SERIAL */

#include <machine/cpufunc.h>	/* for inb/outb */

short *videomem;
int curs;
int cols;
int lines;
unsigned int port;

unsigned char bios[0x100];

extern int end, edata;
void *storage;
void *inbuf;
void *outbuf;
void *window;

void decompress_kernel (void *dest);

int memcmp (const void *arg1, const void *arg2, unsigned len)
{
	unsigned char *a = (unsigned char*) arg1;
	unsigned char *b = (unsigned char*) arg2;

	for (; len-- > 0; ++a, ++b)
		if (*a < *b)
			return (-1);
		else if (*a > *b)
			return (1);
	return (0);
}

void *memcpy (void *to, const void *from, unsigned len)
{
	char *f = (char*) from;
	char *t = (char*) to;

	while (len-- > 0)
		*t++ = *f++;
	return (to);
}

void serial_putchar (unsigned char c)
{
	unsigned char stat;

	if (c == '\n')
		serial_putchar('\r');
	do {
		 stat = inb (COMCONSOLE+5);
	} while (!(stat & 0x20));

	outb (COMCONSOLE, c);
}

void putchar (unsigned char c)
{
	switch (c) {
	case '\n':      curs = (curs + cols) / cols * cols;     break;
	default:        videomem[curs++] = 0x0700 | c;          break;
	}
	while (curs >= cols*lines) {
		int col;

		memcpy (videomem, videomem+cols, (lines-1) * cols * 2);
		for (col = 0; col < cols; col++)
			videomem[(lines - 1) * cols + col] = 0x720;
		curs -= cols;
	}
	/* set cursor position */
	outb (port, 0x0e); outb (port+1, curs>>8);
	outb (port, 0x0f); outb (port+1, curs);
}

int use_serial;

void putstr (char *s)
{
	while (*s) {
		if (use_serial)
			serial_putchar (*s++);
		else
			putchar (*s++);
	}
}

void error (char *s)
{
	putstr ("\n\n");
	putstr (s);
	putstr ("\n\n -- System halted");
	while (1);                      /* Halt */
}

void boot (int howto)
{
	int l, c, *p;

	/* clear bss */
	for (p = &edata; p < &end; ++p)
		*p = 0;

	inbuf   = (void *)0x20000;
	outbuf  = (void *)0x30000;
	window  = (void *)0x40000;
	storage = (void *)0x50000;

	if (!(use_serial = (howto & RB_SERIAL))) {
		/* Test for monochrome video adapter */
		if ((*((unsigned char*) 0x410) & 0x30) == 0x30)
			videomem = (void*) 0xb0000;     /* monochrome */
		else
			videomem = (void*) 0xb8000;     /* color */

		port = *(unsigned short*) 0x463;
		cols = *(unsigned short*) 0x44a;
		lines = 1 + *(unsigned char*) 0x484;
		c = *(unsigned char*) 0x450;
		l = *(unsigned char*) 0x451;

		if (lines < 25)
			lines = 25;
		curs = l*cols + c;
		if (curs > lines*cols)
			curs = (lines-1) * cols;
	}

	putstr ("Uncompressing kernel...");
	decompress_kernel ((void*) KADDR);
	putstr ("done\n");
	putstr ("Booting the kernel\n");
}
