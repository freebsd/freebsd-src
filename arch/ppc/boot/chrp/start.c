/*
 * Copyright (C) Paul Mackerras 1997.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <stdarg.h>
#include "of1275.h"

int (*prom)(void *args);

phandle stdin;
phandle stdout;
phandle stderr;

void printk(char *fmt, ...);

extern void chrpboot(int a1, int a2, void *prom);
extern int strlen(const char *s);

void
start(int a1, int a2, void *promptr)
{
    ofinit(promptr);
    if (ofstdio(&stdin, &stdout, &stderr))
	exit();
    chrpboot(a1, a2, promptr);
    for (;;)
	exit();
}

int
putc(int c, void *f)
{
    char ch = c;

    if (c == '\n')
	putc('\r', f);
    return write(f, &ch, 1) == 1? c: -1;
}

int
putchar(int c)
{
    return putc(c, stdout);
}

int
fputs(char *str, void *f)
{
    int n = strlen(str);

    return write(f, str, n) == n? 0: -1;
}

int
readchar(void)
{
    char ch;

    for (;;) {
	switch (read(stdin, &ch, 1)) {
	case 1:
	    return ch;
	case -1:
	    printk("read(stdin) returned -1\r\n");
	    return -1;
	}
    }
}

static char line[256];
static char *lineptr;
static int lineleft;

int
getchar(void)
{
    int c;

    if (lineleft == 0) {
	lineptr = line;
	for (;;) {
	    c = readchar();
	    if (c == -1 || c == 4)
		break;
	    if (c == '\r' || c == '\n') {
		*lineptr++ = '\n';
		putchar('\n');
		break;
	    }
	    switch (c) {
	    case 0177:
	    case '\b':
		if (lineptr > line) {
		    putchar('\b');
		    putchar(' ');
		    putchar('\b');
		    --lineptr;
		}
		break;
	    case 'U' & 0x1F:
		while (lineptr > line) {
		    putchar('\b');
		    putchar(' ');
		    putchar('\b');
		    --lineptr;
		}
		break;
	    default:
		if (lineptr >= &line[sizeof(line) - 1])
		    putchar('\a');
		else {
		    putchar(c);
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

extern int vsprintf(char *buf, const char *fmt, va_list args);
static char sprint_buf[1024];

void
printk(char *fmt, ...)
{
	va_list args;
	int n;

	va_start(args, fmt);
	n = vsprintf(sprint_buf, fmt, args);
	va_end(args);
	write(stdout, sprint_buf, n);
}

int
printf(char *fmt, ...)
{
	va_list args;
	int n;

	va_start(args, fmt);
	n = vsprintf(sprint_buf, fmt, args);
	va_end(args);
	write(stdout, sprint_buf, n);
	return n;
}
