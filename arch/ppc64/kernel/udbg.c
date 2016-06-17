/*
 * NS16550 Serial Port (uart) debugging stuff.
 *
 * c 2001 PPC 64 Team, IBM Corp
 *
 * NOTE: I am trying to make this code avoid any static data references to
 *  simplify debugging early boot.  We'll see how that goes...
 *
 * To use this call udbg_init() first.  It will init the uart to 9600 8N1.
 * You may need to update the COM1 define if your uart is at a different addr.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <stdarg.h>
#define WANT_PPCDBG_TAB /* Only defined here */
#include <asm/ppcdebug.h>
#include <asm/processor.h>
#include <asm/naca.h>
#include <asm/uaccess.h>
#include <asm/machdep.h>

struct NS16550 {
	/* this struct must be packed */
	unsigned char rbr;  /* 0 */
	unsigned char ier;  /* 1 */
	unsigned char fcr;  /* 2 */
	unsigned char lcr;  /* 3 */
	unsigned char mcr;  /* 4 */
	unsigned char lsr;  /* 5 */
	unsigned char msr;  /* 6 */
	unsigned char scr;  /* 7 */
};

#define thr rbr
#define iir fcr
#define dll rbr
#define dlm ier
#define dlab lcr

#define LSR_DR   0x01  /* Data ready */
#define LSR_OE   0x02  /* Overrun */
#define LSR_PE   0x04  /* Parity error */
#define LSR_FE   0x08  /* Framing error */
#define LSR_BI   0x10  /* Break */
#define LSR_THRE 0x20  /* Xmit holding register empty */
#define LSR_TEMT 0x40  /* Xmitter empty */
#define LSR_ERR  0x80  /* Error */

volatile struct NS16550 *udbg_comport;

void
udbg_init_uart(void *comport)
{
	if (comport) {
		udbg_comport = (struct NS16550 *)comport;
		udbg_comport->lcr = 0x00; eieio();
		udbg_comport->ier = 0xFF; eieio();
		udbg_comport->ier = 0x00; eieio();
		udbg_comport->lcr = 0x80; eieio();	/* Access baud rate */
		udbg_comport->dll = 12;   eieio();	/* 1 = 115200,  2 = 57600, 3 = 38400, 12 = 9600 baud */
		udbg_comport->dlm = 0;    eieio();	/* dll >> 8 which should be zero for fast rates; */
		udbg_comport->lcr = 0x03; eieio();	/* 8 data, 1 stop, no parity */
		udbg_comport->mcr = 0x03; eieio();	/* RTS/DTR */
		udbg_comport->fcr = 0x07; eieio();	/* Clear & enable FIFOs */
	}
}

void
udbg_putc(unsigned char c)
{
	if ( udbg_comport ) {
		while ((udbg_comport->lsr & LSR_THRE) == 0)
			/* wait for idle */;
		udbg_comport->thr = c; eieio();
		if (c == '\n') {
			/* Also put a CR.  This is for convenience. */
			while ((udbg_comport->lsr & LSR_THRE) == 0)
				/* wait for idle */;
			udbg_comport->thr = '\r'; eieio();
		}
	} else if (systemcfg->platform == PLATFORM_ISERIES_LPAR) {
		/* ToDo: switch this via ppc_md */
		printk("%c", c);
	}
}

int udbg_getc_poll(void)
{
	if (udbg_comport) {
		if ((udbg_comport->lsr & LSR_DR) != 0)
			return udbg_comport->rbr;
		else
			return -1;
	}
	return -1;
}

unsigned char
udbg_getc(void)
{
	if ( udbg_comport ) {
		while ((udbg_comport->lsr & LSR_DR) == 0)
			/* wait for char */;
		return udbg_comport->rbr;
	}
	return 0;
}

void
udbg_puts(const char *s)
{
	if (ppc_md.udbg_putc) {
		char c;

		if (s && *s != '\0') {
			while ((c = *s++) != '\0')
				ppc_md.udbg_putc(c);
		}
	} else {
		printk("%s", s);
	}
}

int
udbg_write(const char *s, int n)
{
	int remain = n;
	char c;

	if (!ppc_md.udbg_putc)
		return 0;

	if ( s && *s != '\0' ) {
		while ( (( c = *s++ ) != '\0') && (remain-- > 0)) {
			ppc_md.udbg_putc(c);
		}
	}
	return n - remain;
}

int
udbg_read(char *buf, int buflen) {
	char c, *p = buf;
	int i;
	if (!ppc_md.udbg_putc)
		for (;;);	/* stop here for cpuctl */
	for (i = 0; i < buflen; ++i) {
		do {
			c = ppc_md.udbg_getc();
		} while (c == 0x11 || c == 0x13);
		*p++ = c;
	}
	return i;
}

void
udbg_console_write(struct console *con, const char *s, unsigned int n)
{
	udbg_write(s, n);
}

void
udbg_puthex(unsigned long val)
{
	int i, nibbles = sizeof(val)*2;
	unsigned char buf[sizeof(val)*2+1];
	for (i = nibbles-1;  i >= 0;  i--) {
		buf[i] = (val & 0xf) + '0';
		if (buf[i] > '9')
		    buf[i] += ('a'-'0'-10);
		val >>= 4;
	}
	buf[nibbles] = '\0';
	udbg_puts(buf);
}

void
udbg_printSP(const char *s)
{
	if (systemcfg->platform == PLATFORM_PSERIES) {
		unsigned long sp;
		asm("mr %0,1" : "=r" (sp) :);
		if (s)
			udbg_puts(s);
		udbg_puthex(sp);
	}
}

void
udbg_printf(const char *fmt, ...)
{
	unsigned char buf[256];

	va_list args;
	va_start(args, fmt);

	vsprintf(buf, fmt, args);
	udbg_puts(buf);

	va_end(args);
}

/* Special print used by PPCDBG() macro */
void
udbg_ppcdbg(unsigned long debug_flags, const char *fmt, ...)
{
	unsigned long active_debugs = debug_flags & naca->debug_switch;

	if ( active_debugs ) {
		va_list ap;
		unsigned char buf[256];
		unsigned long i, len = 0;

		for(i=0; i < PPCDBG_NUM_FLAGS ;i++) {
			if (((1U << i) & active_debugs) && 
			    trace_names[i]) {
				len += strlen(trace_names[i]); 
				udbg_puts(trace_names[i]);
				break;
			}
		}
		sprintf(buf, " [%s]: ", current->comm);
		len += strlen(buf); 
		udbg_puts(buf);

		while(len < 18) {
			udbg_puts(" ");
			len++;
		}

		va_start(ap, fmt);
		vsprintf(buf, fmt, ap);
		udbg_puts(buf);
		
		va_end(ap);
	}
}

unsigned long
udbg_ifdebug(unsigned long flags)
{
	return (flags & naca->debug_switch);
}
