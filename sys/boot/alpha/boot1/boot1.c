/*
 * $Id: boot1.c,v 1.1.1.1 1998/08/21 03:17:41 msmith Exp $
 * From	$NetBSD: bootxx.c,v 1.4 1997/09/06 14:08:29 drochner Exp $ 
 */

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>

#include <machine/prom.h>

#define DEBUGxx

extern end[];
int errno;

char *heap = (char*) end;

void
putchar(int c)
{
    if (c == '\n')
	prom_putchar('\r');
    prom_putchar(c);
}

void
puts(const char *s)
{
    while (*s)
	putchar(*s++);
}

void *
malloc(size_t size)
{
    char *p = heap;
    size = (size + 7) & ~7;
    heap += size;
    return p;
}

void
free(void * p)
{
}

void
panic(const char *message, ...)
{
    puts(message);
    puts("\r\n");
    halt();
}

int prom_fd = 0;

int
devopen()
{
    prom_return_t ret;
    char devname[64];
    
    if (prom_fd)
	return;

    ret.bits = prom_getenv(PROM_E_BOOTED_DEV, devname, sizeof devname);

    ret.bits = prom_open(devname, ret.u.retval + 1);
    if (ret.u.status)
	panic("devopen: open failed\n");

    prom_fd = ret.u.retval;

    /* XXX read disklabel and setup partition offset */

    return 0;
}

#ifdef DEBUG

void
puthex(u_long v)
{
    int digit;
    char hex[] = "0123456789abcdef";

    if (!v) {
	puts("0");
	return;
    }

    for (digit = 0; v >= (0x10L << digit); digit += 4)
	;

    for (; digit >= 0; digit -= 4)
	putchar(hex[(v >> digit) & 0xf]);
}

#endif

void
devread(char *buf, int block, size_t size)
{
#ifdef DEBUG
    puts("devread(");
    puthex((u_long)buf);
    puts(",");
    puthex(block);
    puts(",");
    puthex(size);
    puts(")\n");
#endif

    prom_read(prom_fd, size, buf, block);
}

void
devclose()
{
    if (prom_fd) {
	prom_close(prom_fd);
	prom_fd = 0;
    }
}

void
loadfile(char *name, char *addr)
{
    int n;

    if (openrd(name)) {
	puts("Can't open file ");
	puts(name);
	puts("\n");
	halt();
    }

    do {
	n = readit(addr, 1024);
	addr += n;
	twiddle();
    } while (n > 0);

    devclose();
}

void
main()
{
    char *loadaddr = (char*) SECONDARY_LOAD_ADDRESS;
    char *p;
    void (*entry) __P((void));

    int		i;

    init_prom_calls();
    
    loadfile("/boot/boot2", loadaddr);

    entry = (void (*)())loadaddr;
    (*entry)();
}
