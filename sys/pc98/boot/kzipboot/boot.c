/*
 * FreeBSD kernel unpacker.
 * 1993 by Serge Vakulenko
 * modified for FreeBSD 2.1 by Gary Jennejohn - 12FEB95
 */

/*
 * FreeBSD(98) port
 * 1995 by KATO T. of Nagoya University
 */

#include <machine/cpufunc.h> /* for inb/outb */
#include <sys/reboot.h> /* for RB_SERIAL */

short *videomem;
int curs;
int cols;
int lines;
unsigned int port;

#ifdef PC98
unsigned char bios[0x200];
#else
unsigned char bios[0x100];
#endif

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
#ifdef PC98
	do {
		 stat = inb (COMCONSOLE+2);
	} while (!(stat & 0x01));
#else
	do {
		 stat = inb (COMCONSOLE+5);
	} while (!(stat & 0x20));
#endif

	outb (COMCONSOLE, c);
}

void putchar (unsigned char c)
{
	switch (c) {
	case '\n':      curs = (curs + cols) / cols * cols;     break;
#ifdef PC98
	default:        videomem[curs++] = (c == 0x5c ? 0xfc : c);   break;
#else
	default:        videomem[curs++] = 0x0700 | c;          break;
#endif
	}
	while (curs >= cols*lines) {
		int col;

		memcpy (videomem, videomem+cols, (lines-1) * cols * 2);
		for (col = 0; col < cols; col++)
#ifdef PC98
			videomem[(lines - 1) * cols + col] = 0x20;
#else
			videomem[(lines - 1) * cols + col] = 0x720;
#endif
		curs -= cols;
	}
	/* set cursor position */
#ifdef PC98
	while ((inb(0x60) & 0x04) == 0) {}
	outb(0x62, 0x49);
	outb(0x60, (curs + 1) & 0xff);
	outb(0x60, (curs + 1) >> 8);
#else
	outb (port, 0x0e); outb (port+1, curs>>8);
	outb (port, 0x0f); outb (port+1, curs);
#endif
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
#ifdef PC98
	unsigned short gdc_curaddr;
	int i;
#endif
	/* clear bss */
	for (p = &edata; p < &end; ++p)
		*p = 0;

	inbuf   = (void *)0x20000;
	outbuf  = (void *)0x30000;
	window  = (void *)0x40000;
	storage = (void *)0x50000;

	if (!(use_serial = (howto & RB_SERIAL))) {
#ifdef PC98
		videomem = (void*)0xa0000;
		cols = 80;
		lines = 25;

		/* Is FIFO buffer empty ? */
		while ((inb(0x60) & 0x04) == 0) {}

		outb(0x62, 0xe0);	/* CSRR command */
		/* T-GDC busy ? */
		while ((inb(0x60) & 0x01) == 0) {}
		/* read cursor address */
		gdc_curaddr = inb(0x62);
		gdc_curaddr += (inb(0x62) << 8);
		/* ignore rest of data */
		for (i = 0; i < 3; i++) {
			(void)inb(0x62);
		}

		l = gdc_curaddr / 80 + 1;
		c = 0;
#else
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
#endif

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
