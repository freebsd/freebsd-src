/*
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "cardd.h"

static FILE *in;
static int pushc, pusht;
static int lineno;
static char *filename;

static char *keys[] = {
	"io",			/* 1 */
	"irq",			/* 2 */
	"memory",		/* 3 */
	"card",			/* 4 */
	"device",		/* 5 */
	"config",		/* 6 */
	"__EOF__",		/* 7 */
	"reset",		/* 8 */
	"ether",		/* 9 */
	"insert",		/* 10 */
	"remove",		/* 11 */
	"iosize",		/* 12 */
	"memsize",		/* 13 */
	0
};

struct flags {
	char   *name;
	int     mask;
};

void    parsefile(void);
char   *token(void);
char   *getline(void);
char   *next_tok(void);
int     num_tok(void);
void    error(char *);
int     keyword(char *);
struct allocblk *ioblk_tok(int);
struct allocblk *memblk_tok(int);
int     irq_tok(int);
void    setflags(struct flags *, int *);
struct driver *new_driver(char *);

void    addcmd(struct cmd **cp);
void    parse_card(void);

/*
 * Read a file and parse the pcmcia configuration data.
 * After parsing, verify the links.
 */
void
readfile(char *name)
{
	struct card *cp;

	in = fopen(name, "r");
	if (in == 0) {
		perror(name);
		exit(1);
	}
	parsefile();
	for (cp = cards; cp; cp = cp->next) {
		if (cp->config == 0)
			fprintf(stderr, "warning: card %s(%s) has no valid configuration\n",
			    cp->manuf, cp->version);
	}
}

void
parsefile(void)
{
	int     i;
	struct allocblk *bp;

	pushc = 0;
	lineno = 1;
	for (;;)
		switch (keyword(next_tok())) {
		default:
			error("Syntax error");
			pusht = 0;
			break;
		case 7:
			return;
		case 1:
			/* reserved I/O blocks */
			while ((bp = ioblk_tok(0)) != 0) {
				if (bp->size == 0 || bp->addr == 0) {
					free(bp);
					continue;
				}
				bit_nset(io_avail, bp->addr, bp->addr + bp->size - 1);
				bp->next = pool_ioblks;
				pool_ioblks = bp;
			}
			pusht = 1;
			break;
		case 2:
			/* reserved irqs */
			while ((i = irq_tok(0)) > 0)
				pool_irq[i] = 1;
			pusht = 1;
			break;
		case 3:
			/* reserved memory blocks. */
			while ((bp = memblk_tok(0)) != 0) {
				if (bp->size == 0 || bp->addr == 0) {
					free(bp);
					continue;
				}
				bit_nset(mem_avail, MEM2BIT(bp->addr),
				    MEM2BIT(bp->addr + bp->size) - 1);
				bp->next = pool_mem;
				pool_mem = bp;
			}
			pusht = 1;
			break;
		case 4:
			/* Card definition. */
			parse_card();
			break;
#if 0
		case 5:
			/* Device description */
			parse_device();
			break;
#endif
		}
}

/*
 *	Parse a card definition.
 */
void
parse_card(void)
{
	char   *man, *vers;
	struct card *cp;
	int     i;
	struct card_config *confp, *lastp;

	man = newstr(next_tok());
	vers = newstr(next_tok());
	cp = xmalloc(sizeof(*cp));
	cp->manuf = man;
	cp->version = vers;
	cp->reset_time = 50;
	cp->next = cards;
	cards = cp;
	for (;;) {
		switch (keyword(next_tok())) {
		default:
			pusht = 1;
			return;
		case 8:
			i = num_tok();
			if (i == -1) {
				error("Illegal card reset time");
				break;
			}
			cp->reset_time = i;
			break;
		case 6:
			i = num_tok();
			if (i == -1) {
				error("Illegal card config index");
				break;
			}
			confp = xmalloc(sizeof(*confp));
			man = next_tok();
			confp->driver = new_driver(man);
			confp->irq = num_tok();
			confp->flags = num_tok();
			if (confp->flags == -1) {
				pusht = 1;
				confp->flags = 0;
			}
			if (confp->irq < 0 || confp->irq > 15) {
				error("Illegal card IRQ value");
				break;
			}
			confp->index = i & 0x3F;

			/*
			 * If no valid driver for this config, then do not save
			 * this configuration entry.
			 */
			if (confp->driver) {
				if (cp->config == 0)
					cp->config = confp;
				else {
					for (lastp = cp->config; lastp->next;
					    lastp = lastp->next);
					lastp->next = confp;
				}
			} else
				free(confp);
			break;
		case 9:
			cp->ether = num_tok();
			if (cp->ether == -1) {
				error("Illegal ether address offset");
				cp->ether = 0;
			}
			break;
		case 10:
			addcmd(&cp->insert);
			break;
		case 11:
			addcmd(&cp->remove);
			break;
		}
	}
}

/*
 *	Generate a new driver structure. If one exists, use
 *	that one after confirming the correct class.
 */
struct driver *
new_driver(char *name)
{
	struct driver *drvp;
	char   *p;

	for (drvp = drivers; drvp; drvp = drvp->next)
		if (strcmp(drvp->name, name) == 0)
			return (drvp);
	drvp = xmalloc(sizeof(*drvp));
	drvp->next = drivers;
	drivers = drvp;
	drvp->name = newstr(name);
	drvp->kernel = newstr(name);
	p = drvp->kernel;
	while (*p++)
		if (*p >= '0' && *p <= '9') {
			drvp->unit = atoi(p);
			*p = 0;
			break;
		}
#ifdef	DEBUG
	if (verbose)
		printf("Drv %s%d created\n", drvp->kernel, drvp->unit);
#endif
	return (drvp);
}

#if 0
/*
 *	Parse the device description.
 */
parse_device(void)
{
	enum drvclass type = drvclass_tok();
	struct device *dp;
	static struct device *lastp;

	if (type == drv_none) {
		error("Unknown driver class");
		return;
	}
	dp = xmalloc(sizeof(*dp));
	dp->type = type;
	if (devlist == 0)
		devlist = dp;
	else
		lastp->next = dp;
	lastp = dp;
	for (;;)
		switch (keyword(next_tok())) {
		default:
			pusht = 1;
			return;
		case 10:
			addcmd(&dp->insert);
			break;
		case 11:
			addcmd(&dp->remove);
			break;
		}
}

/*
 *	Parse the driver description.
 */
parse_driver(void)
{
	char   *name, *dev, *p;
	struct driver *dp;
	static struct driver *lastp;
	int     i;
	struct allocblk *bp;
	static struct flags io_flags[] = {
		{"ws", 0x01},
		{"16bit", 0x02},
		{"cs16", 0x04},
		{"zerows", 0x08},
		{0, 0}
	};
	static struct flags mem_flags[] = {
		{"16bit", 0x01},
		{"zerows", 0x02},
		{"ws0", 0x04},
		{"ws1", 0x08},
		{0, 0}
	};

	name = newstr(next_tok());
	dev = newstr(next_tok());
	type = drvclass_tok();
	if (type == drv_none) {
		error("Unknown driver class");
		return;
	}
	dp = xmalloc(sizeof(*dp));
	dp->name = name;
	dp->kernel = dev;
	dp->type = type;
	dp->unit = -1;
	dp->irq = -1;

	/* Check for unit number in driver name. */
	p = dev;
	while (*p++)
		if (*p >= '0' && *p <= '9') {
			dp->unit = atoi(p);
			*p = 0;
			break;
		}
	if (dp->unit < 0)
		error("Illegal kernel driver unit");

	/* Place at end of list. */
	if (lastp == 0)
		drivers = dp;
	else
		lastp->next = dp;
	lastp = dp;
	for (;;)
		switch (keyword(next_tok())) {
		default:
			pusht = 1;
			return;
		case 1:
			bp = ioblk_tok(1);
			if (bp) {
				setflags(io_flags, &bp->flags);
				if (dp->io) {
					error("Duplicate I/O spec");
					free(bp);
				} else {
					bit_nclear(io_avail, bp->addr,
					    bp->addr + bp->size - 1);
					dp->io = bp;
				}
			}
			break;
		case 2:
			dp->irq = irq_tok(1);
			if (dp->irq > 0)
				pool_irq[i] = 0;
			break;
		case 3:
			bp = memblk_tok(1);
			if (bp) {
				setflags(mem_flags, &bp->flags);
				if (dp->mem) {
					error("Duplicate memory spec");
					free(bp);
				} else {
					bit_nclear(mem_avail,
					    MEM2BIT(bp->addr),
					    MEM2BIT(bp->addr + bp->size) - 1);
					dp->mem = bp;
				}
			}
			break;
		case 10:
			addcmd(&dp->insert);
			break;
		case 11:
			addcmd(&dp->remove);
			break;
		case 12:
			/*
			 * iosize - Don't allocate an I/O port, but specify
			 * a size for the range of ports. The actual port
			 * number will be allocated dynamically.
			 */
			i = num_tok();
			if (i <= 0 || i > 128)
				error("Illegal iosize");
			else {
				int     flags = 0;
				setflags(io_flags, &flags);
				if (dp->io)
					error("Duplicate I/O spec");
				else {
					dp->io = xmalloc(sizeof(*dp->io));
					dp->io->flags = flags;
					dp->io->size = i;
				}
			}
			break;
		case 13:
			i = num_tok();
			if (i <= 0 || i > 256 * 1024)
				error("Illegal memsize");
			else {
				int     flags = 0;
				setflags(mem_flags, &flags);
				if (dp->mem)
					error("Duplicate memory spec");
				else {
					dp->mem = xmalloc(sizeof(*dp->mem));
					dp->mem->flags = flags;
					dp->mem->size = i;
				}
			}
			break;
		}
}
/*
 *	drvclass_tok - next token is expected to
 *	be a driver class.
 */
enum drvclass
drvclass_tok(void)
{
	char   *s = next_tok();

	if (strcmp(s, "tty") == 0)
		return (drv_tty);
	else
		if (strcmp(s, "net") == 0)
			return (drv_net);
		else
			if (strcmp(s, "bio") == 0)
				return (drv_bio);
			else
				if (strcmp(s, "null") == 0)
					return (drv_null);
	return (drv_none);
}
#endif	/* 0 */

/*
 *	Parse one I/O block.
 */
struct allocblk *
ioblk_tok(int force)
{
	struct allocblk *io;
	int     i, j;

	if ((i = num_tok()) >= 0) {
		if (strcmp("-", next_tok()) || (j = num_tok()) < 0 || j < i) {
			error("I/O block format error");
			return (0);
		}
		io = xmalloc(sizeof(*io));
		io->addr = i;
		io->size = j - i + 1;
		if (j > IOPORTS) {
			error("I/O port out of range");
			if (force) {
				free(io);
				io = 0;
			} else
				io->addr = io->size = 0;
		}
		return (io);
	}
	if (force)
		error("Illegal or missing I/O block spec");
	return (0);
}

/*
 *	Parse a memory block.
 */
struct allocblk *
memblk_tok(int force)
{
	struct allocblk *mem;
	int     i, j;

	if ((i = num_tok()) >= 0)
		if ((j = num_tok()) < 0)
			error("Illegal memory block");
		else {
			mem = xmalloc(sizeof(*mem));
			mem->addr = i & ~(MEMUNIT - 1);
			mem->size = (j + MEMUNIT - 1) & ~(MEMUNIT - 1);
			if (i < MEMSTART || (i + j) > MEMEND) {
				error("Memory address out of range");
				if (force) {
					free(mem);
					mem = 0;
				} else
					mem->addr = mem->size = 0;
			}
			return (mem);
		}
	if (force)
		error("Illegal or missing memory block spec");
	return (0);
}

/*
 *	IRQ token. Must be number > 0 && < 16.
 *	If force is set, IRQ must exist, and can also be '?'.
 */
int
irq_tok(int force)
{
	int     i;

	if (strcmp("?", next_tok()) == 0 && force)
		return (0);
	pusht = 1;
	i = num_tok();
	if (i > 0 && i < 16)
		return (i);
	if (force)
		error("Illegal IRQ value");
	return (-1);
}

/*
 *	search the table for a match.
 */
int
keyword(char *str)
{
	char  **s;
	int     i = 1;

	for (s = keys; *s; s++, i++)
		if (strcmp(*s, str) == 0)
			return (i);
	return (0);
}

/*
 *	Set/clear flags
 */
void
setflags(struct flags *flags, int *value)
{
	char   *s;
	struct flags *fp;
	int     set = 1;

	do {
		s = next_tok();
		if (*s == '!') {
			s++;
			set = 0;
		}
		for (fp = flags; fp->name; fp++)
			if (strcmp(s, fp->name) == 0) {
				if (set)
					*value |= fp->mask;
				else
					*value &= ~fp->mask;
				break;
			}
	} while (fp->name);
	pusht = 1;
}

/*
 *	addcmd - Append the command line to the list of
 *	commands.
 */
void
addcmd(struct cmd **cp)
{
	struct cmd *ncp;
	char   *s = getline();

	if (*s) {
		ncp = xmalloc(sizeof(*ncp));
		ncp->line = s;
		while (*cp)
			cp = &(*cp)->next;
		*cp = ncp;
	}

}
void
error(char *msg)
{
	pusht = 1;
	fprintf(stderr, "%s: %s at line %d, near %s\n",
	    filename, msg, lineno, next_tok());
	pusht = 1;
}

int     last_char;

int
get(void)
{
	int     c;

	if (pushc)
		c = pushc;
	else
		c = getc(in);
	pushc = 0;
	while (c == '\\') {
		c = getc(in);
		switch (c) {
		case '#':
			return (last_char = c);
		case '\n':
			lineno++;
			c = getc(in);
			continue;
		}
		pushc = c;
		return ('\\');
	}
	if (c == '\n')
		lineno++;
	if (c == '#') {
		while (get() != '\n');
		return (last_char = '\n');
	}
	return (last_char = c);
}

/*
 *	num_tok - expecting a number token. If not a number,
 *	return -1.
 *	Handles octal (who uses octal anymore?)
 *		hex
 *		decimal
 *	Looks for a 'k' at the end of decimal numbers
 *	and multiplies by 1024.
 */
int
num_tok(void)
{
	char   *s = next_tok(), c;
	int     val = 0, base;

	base = 10;
	c = *s++;
	if (c == '0') {
		base = 8;
		c = *s++;
		if (c == 'x' || c == 'X') {
			c = *s++;
			base = 16;
		}
	}
	do {
		switch (c) {
		case 'k':
		case 'K':
			if (val && base == 10 && *s == 0)
				return (val * 1024);
			return (-1);
		default:
			return (-1);
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			val = val * base + c - '0';
			break;

		case '8':
		case '9':
			if (base == 8)
				return (-1);
			else
				val = val * base + c - '0';
			break;
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
			if (base == 16)
				val = val * base + c - 'a' + 10;
			else
				return (-1);
			break;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			if (base == 16)
				val = val * base + c - 'A' + 10;
			else
				return (-1);
			break;
		}
	} while ((c = *s++) != 0);
	return (val);
}

char   *_next_tok(void);

char   *
next_tok(void)
{
	char   *s = _next_tok();
#if 0
	printf("Tok = %s\n", s);
#endif
	return (s);
}

/*
 *	get one token. Handles string quoting etc.
 */
char   *
_next_tok(void)
{
	static char buf[1024];
	char   *p = buf, instr = 0;
	int     c;

	if (pusht) {
		pusht = 0;
		return (buf);
	}
	for (;;) {
		c = get();
		switch (c) {
		default:
			*p++ = c;
			break;
		case '"':
			if (instr) {
				*p++ = 0;
				return (buf);
			}
			instr = 1;
			break;
		case '\n':
			if (instr) {
				error("Unterminated string");
				break;
			}
		case ' ':
		case '\t':
			/* Eat whitespace unless in a string. */
			if (!instr) {
				if (p != buf) {
					*p++ = 0;
					return (buf);
				}
			} else
				*p++ = c;
			break;
/*
 *	Special characters that must be tokens on their own.
 */
		case '-':
		case '?':
		case '*':
			if (instr)
				*p++ = c;
			else {
				if (p != buf)
					pushc = c;
				else
					*p++ = c;
				*p++ = 0;
				return (buf);
			}
			break;
		case EOF:
			if (p != buf) {
				*p++ = 0;
				return (buf);
			}
			strcpy(buf, "__EOF__");
			return (buf);
		}
	}
}
/*
 *	get the rest of the line. If the
 *	last character scanned was a newline, then
 *	return an empty line. If this isn't checked, then
 *	a getline may incorrectly return the next line.
 */
char   *
getline(void)
{
	char    buf[1024], *p = buf;
	int     c, i = 0;

	if (last_char == '\n')
		return (newstr(""));
	do {
		c = get();
	} while (c == ' ' || c == '\t');
	for (; c != '\n' && c != EOF; c = get())
		if (i++ < sizeof(buf) - 10)
			*p++ = c;
	*p = 0;
	return (newstr(buf));
}
