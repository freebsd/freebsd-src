/*
 * Copyright (C) 1996 Paul Mackerras.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sysrq.h>
#include <linux/fs.h>
#include <asm/machdep.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/prom.h>
#include <asm/processor.h>

/* Transition to udbg isn't quite done yet...but very close. */
#define USE_UDBG
#ifdef USE_UDBG
#include <asm/udbg.h>
#endif

#ifndef USE_UDBG
static volatile unsigned char *sccc, *sccd;
#endif
unsigned long TXRDY, RXRDY;
extern void xmon_printf(const char *fmt, ...);
static int xmon_expect(const char *str, unsigned int timeout);

#ifndef USE_UDBG
static int console = 0;
#endif
static int via_modem = 0;
/* static int xmon_use_sccb = 0;  --Unused */

#define TB_SPEED	25000000

extern void *comport1;
static inline unsigned int readtb(void)
{
	unsigned int ret;

	asm volatile("mftb %0" : "=r" (ret) :);
	return ret;
}

#ifndef USE_UDBG
void buf_access(void)
{
	sccd[3] &= ~0x80;	/* reset DLAB */
}
#endif

static void sysrq_handle_xmon(int key, struct pt_regs *pt_regs, struct kbd_struct *kbd, struct tty_struct *tty) 
{
  xmon(pt_regs);
}
static struct sysrq_key_op sysrq_xmon_op = 
{
	.handler =	sysrq_handle_xmon,
	.help_msg =	"xmon",
	.action_msg =	"Entering xmon\n",
};

void
xmon_map_scc(void)
{
	/* This maybe isn't the best place to register sysrq 'x' */
	__sysrq_put_key_op('x', &sysrq_xmon_op);
#ifndef USE_UDBG
	/* should already be mapped by the kernel boot */
	sccd = (volatile unsigned char *) (((unsigned long)comport1));
	sccc = (volatile unsigned char *) (((unsigned long)comport1)+5);
	TXRDY = 0x20;
	RXRDY = 1;
#endif
}

static int scc_initialized = 0;

void xmon_init_scc(void);
extern void pmu_poll(void);

int
xmon_write(void *handle, void *ptr, int nb)
{
#ifdef USE_UDBG
	return udbg_write(ptr, nb);
#else
	char *p = ptr;
	int i, c, ct;

	if (!scc_initialized)
		xmon_init_scc();
	ct = 0;
	for (i = 0; i < nb; ++i) {
		while ((*sccc & TXRDY) == 0) {
		}
		c = p[i];
		if (c == '\n' && !ct) {
			c = '\r';
			ct = 1;
			--i;
		} else {
			if (console)
				printk("%c", c);
			ct = 0;
		}
		buf_access();
		*sccd = c;
	}
	return i;
#endif
}

int xmon_wants_key;

int
xmon_read(void *handle, void *ptr, int nb)
{
#ifdef USE_UDBG
	return udbg_read(ptr, nb);
#else
	char *p = ptr;
	int i, c;

	if (!scc_initialized)
		xmon_init_scc();
	for (i = 0; i < nb; ++i) {
		do {
			while ((*sccc & RXRDY) == 0)
				;
			buf_access();
			c = *sccd;
		} while (c == 0x11 || c == 0x13);
		*p++ = c;
	}
	return i;
#endif
}

int
xmon_read_poll(void)
{
#ifdef USE_UDBG
	return udbg_getc_poll();
#else
	if ((*sccc & RXRDY) == 0) {
		return -1;
	}
	buf_access();
	return *sccd;
#endif
}
 
void
xmon_init_scc()
{
#ifndef USE_UDBG
	sccd[3] = 0x83; eieio();	/* LCR = 8N1 + DLAB */
	sccd[0] = 12; eieio();		/* DLL = 9600 baud */
	sccd[1] = 0; eieio();
	sccd[2] = 0; eieio();		/* FCR = 0 */
	sccd[3] = 3; eieio();		/* LCR = 8N1 */
	sccd[1] = 0; eieio();		/* IER = 0 */
#endif

	scc_initialized = 1;
	if (via_modem) {
		for (;;) {
			xmon_write(0, "ATE1V1\r", 7);
			if (xmon_expect("OK", 5)) {
				xmon_write(0, "ATA\r", 4);
				if (xmon_expect("CONNECT", 40))
					break;
			}
			xmon_write(0, "+++", 3);
			xmon_expect("OK", 3);
		}
	}
}

void *xmon_stdin;
void *xmon_stdout;
void *xmon_stderr;

void
xmon_init(void)
{
}

int
xmon_putc(int c, void *f)
{
	char ch = c;

	if (c == '\n')
		xmon_putc('\r', f);
	return xmon_write(f, &ch, 1) == 1? c: -1;
}

int
xmon_putchar(int c)
{
	return xmon_putc(c, xmon_stdout);
}

int
xmon_fputs(char *str, void *f)
{
	int n = strlen(str);

	return xmon_write(f, str, n) == n? 0: -1;
}

int
xmon_readchar(void)
{
	char ch;

	for (;;) {
		switch (xmon_read(xmon_stdin, &ch, 1)) {
		case 1:
			return ch;
		case -1:
			xmon_printf("read(stdin) returned -1\r\n", 0, 0);
			return -1;
		}
	}
}

static char line[256];
static char *lineptr;
static int lineleft;

int xmon_expect(const char *str, unsigned int timeout)
{
	int c;
	unsigned int t0;

	timeout *= TB_SPEED;
	t0 = readtb();
	do {
		lineptr = line;
		for (;;) {
			c = xmon_read_poll();
			if (c == -1) {
				if (readtb() - t0 > timeout)
					return 0;
				continue;
			}
			if (c == '\n')
				break;
			if (c != '\r' && lineptr < &line[sizeof(line) - 1])
				*lineptr++ = c;
		}
		*lineptr = 0;
	} while (strstr(line, str) == NULL);
	return 1;
}

int
xmon_getchar(void)
{
	int c;

	if (lineleft == 0) {
		lineptr = line;
		for (;;) {
			c = xmon_readchar();
			if (c == -1 || c == 4)
				break;
			if (c == '\r' || c == '\n') {
				*lineptr++ = '\n';
				xmon_putchar('\n');
				break;
			}
			switch (c) {
			case 0177:
			case '\b':
				if (lineptr > line) {
					xmon_putchar('\b');
					xmon_putchar(' ');
					xmon_putchar('\b');
					--lineptr;
				}
				break;
			case 'U' & 0x1F:
				while (lineptr > line) {
					xmon_putchar('\b');
					xmon_putchar(' ');
					xmon_putchar('\b');
					--lineptr;
				}
				break;
			default:
				if (lineptr >= &line[sizeof(line) - 1])
					xmon_putchar('\a');
				else {
					xmon_putchar(c);
					*lineptr++ = c;
				}
			}
		}
		lineleft = lineptr - line;
		lineptr = line;
	}
	if (lineleft == 0)
		return -1;
	--lineleft;
	return *lineptr++;
}

char *
xmon_fgets(char *str, int nb, void *f)
{
	char *p;
	int c;

	for (p = str; p < str + nb - 1; ) {
		c = xmon_getchar();
		if (c == -1) {
			if (p == str)
				return 0;
			break;
		}
		*p++ = c;
		if (c == '\n')
			break;
	}
	*p = 0;
	return str;
}
