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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "cardd.h"

static FILE *in;
static int includes = 0;
static struct {
	FILE	*filep;
	char	*filename;
	int	lineno;
} configfiles[MAXINCLUDES] = {{NULL, NULL, 0}, };

static int pushc, pusht;
static int lineno;
static char *filename;

static char *keys[] = {
	"__EOF__",		/* 1 */
	"io",			/* 2 */
	"irq",			/* 3 */
	"memory",		/* 4 */
	"card",			/* 5 */
	"device",		/* 6 */
	"config",		/* 7 */
	"reset",		/* 8 */
	"ether",		/* 9 */
	"insert",		/* 10 */
	"remove",		/* 11 */
	"iosize",		/* 12 */
	"debuglevel",		/* 13 */
	"include",		/* 14 */
	"function",		/* 15 */
	"logstr",		/* 16 */
	0
};

#define KWD_EOF			1
#define KWD_IO			2
#define KWD_IRQ			3
#define KWD_MEMORY		4
#define KWD_CARD		5
#define KWD_DEVICE		6
#define KWD_CONFIG		7
#define KWD_RESET		8
#define KWD_ETHER		9
#define KWD_INSERT		10
#define KWD_REMOVE		11
#define KWD_IOSIZE		12
#define KWD_DEBUGLEVEL		13
#define KWD_INCLUDE		14
#define KWD_FUNCTION		15
#define KWD_LOGSTR		16

/* for keyword compatibility with PAO/plain FreeBSD */
static struct {
	char	*alias;
	u_int	key;
} key_aliases[] = {
	{"generic", KWD_FUNCTION},
	{0, 0}
};

struct flags {
	char   *name;
	int     mask;
};

extern int	doverbose;

static void    parsefile(void);
static char   *getline(void);
static char   *next_tok(void);
static int     num_tok(void);
static void    error(char *);
static int     keyword(char *);
static int     irq_tok(int);
static int     config_tok(unsigned char *);
static int     func_tok(void);
static int     debuglevel_tok(int);
static struct allocblk *ioblk_tok(int);
static struct allocblk *memblk_tok(int);
static struct driver *new_driver(char *);
static int     iosize_tok(void);
static void    file_include(char *);

static void    addcmd(struct cmd **);
static void    parse_card(int);

static void
delete_card(struct card *cp)
{
	struct ether	*etherp, *ether_next;
	struct card_config *configp, *config_next;
	struct cmd	*cmdp, *cmd_next;

	/* free strings */
	free(cp->manuf);
	free(cp->version);
	free(cp->add_info1);
	free(cp->add_info2);
	free(cp->logstr);

	/* free structures */
	for (etherp = cp->ether; etherp; etherp = ether_next) {
		ether_next = etherp->next;
		free(etherp);
	}
	for (configp = cp->config; configp; configp = config_next) {
		config_next = configp->next;
		free(configp);
	}
	for (cmdp = cp->insert; cmdp; cmdp = cmd_next) {
		cmd_next = cmdp->next;
		free(cmdp->line);
		free(cmdp);
	}
	for (cmdp = cp->remove; cmdp; cmdp = cmd_next) {
		cmd_next = cmdp->next;
		free(cmdp->line);
		free(cmdp);
	}
	free(cp);
}

/*
 * Read a file and parse the pcmcia configuration data.
 * After parsing, verify the links.
 */
void
readfile(char *name)
{
	int i, inuse;
	struct card *cp, *card_next;
	struct card *genericp, *tail_gp;
	struct card_config *configp;

	/* delete all card configuration data before we proceed */
	genericp = 0;
	cp = cards;
	cards = last_card = 0;
	while (cp) {
		card_next = cp->next;

		/* check whether this card is in use */
		inuse = 0;
		for (configp = cp->config; configp; configp = configp->next) {
			if (configp->inuse) {
				inuse = 1;
				break;
			}
		}

		/*
		 * don't delete entry in use for consistency.
		 * leave normal entry in the cards list,
		 * insert generic entry into the list after re-loading config files.
		 */
		if (inuse == 1) {
			cp->next = 0;	/* unchain from the cards list */
			switch (cp->deftype) {
			case DT_VERS:
				/* don't delete this entry for consistency */
				if (debug_level >= 1) {
					logmsg("Card \"%s\"(\"%s\") is in use, "
					    "can't change configuration\n",
					    cp->manuf, cp->version);
				}
				/* add this to the card list */
				if (!last_card) {
					cards = last_card = cp;
				} else {
					last_card->next = cp;
					last_card = cp;
				}
				break;

			case DT_FUNC:
				/* generic entry must be inserted to the list later */
				if (debug_level >= 1) {
					logmsg("Generic entry is in use, "
					    "can't change configuration\n");
				}
				cp->next = genericp;
				genericp = cp;
				break;
			}
		} else {
			delete_card(cp);
		}

		cp = card_next;
	}

	for (i = 0; i < MAXINCLUDES; i++) {
		if (configfiles[i].filep) {
			fclose(configfiles[i].filep);
			configfiles[i].filep = NULL;
			if (i > 0) {
				free(configfiles[i].filename);
			}
		}
	}
	in = fopen(name, "r");
	if (in == 0) {
		logerr(name);
		die("readfile");
	}
	includes = 0;
	configfiles[includes].filep = in;
	filename = configfiles[includes].filename = name;

	parsefile();
	for (cp = cards; cp; cp = cp->next) {
		if (cp->config == 0)
			logmsg("warning: card %s(%s) has no valid configuration\n",
			    cp->manuf, cp->version);
	}

	/* insert generic entries in use into the top of generic entries */
	if (genericp) {
		/* search tail of generic entries in use */
		for (tail_gp = genericp; tail_gp->next; tail_gp = tail_gp->next)
			;

		/*
		 * if the top of cards list is generic entry,
		 * insert generic entries in use before it.
		 */
		if (cards && cards->deftype == DT_FUNC) {
			tail_gp->next = cards;
			cards = genericp;
			goto generic_done;
		}

		/* search top of generic entries */
		for (cp = cards; cp; cp = cp->next) {
			if (cp->next && cp->next->deftype == DT_FUNC) {
				break;
			}
		}

		/*
		 * if we have generic entry in the cards list,
		 * insert generic entries in use into there.
		 */
		if (cp) {
			tail_gp->next = cp->next;
			cp->next = genericp;
			goto generic_done;
		}

		/*
		 * otherwise we don't have generic entries in
		 * cards list, just add them to the list.
		 */
		if (!last_card) {
			cards = genericp;
		} else {
			last_card->next = genericp;
			last_card = tail_gp;
		}
generic_done:
	}

	/* save the initial state of resource pool */
	bcopy(io_avail, io_init, bitstr_size(IOPORTS));
	bcopy(mem_avail, mem_init, bitstr_size(MEMBLKS));
	bcopy(pool_irq, irq_init, sizeof(pool_irq));
}

static void
parsefile(void)
{
	int     i;
	int     errors = 0;
	struct allocblk *bp, *next;
	char	*incl;

	pushc = 0;
	lineno = 1;
	for (;;)
		switch (keyword(next_tok())) {
		case KWD_EOF:
			/* EOF */
			return;
		case KWD_IO:
			/* override reserved I/O blocks */
			bit_nclear(io_avail, 0, IOPORTS-1);
			for (bp = pool_ioblks; bp; bp = next) {
				next = bp->next;
				free(bp);
			}
			pool_ioblks = NULL;

			while ((bp = ioblk_tok(0)) != 0) {
				if (bp->size == 0 || bp->addr == 0) {
					free(bp);
					continue;
				}
				bit_nset(io_avail, bp->addr,
					 bp->addr + bp->size - 1);
				bp->next = pool_ioblks;
				pool_ioblks = bp;
			}
			pusht = 1;
			break;
		case KWD_IRQ:
			/* override reserved irqs */
			bzero(pool_irq, sizeof(pool_irq));
			while ((i = irq_tok(0)) > 0)
				pool_irq[i] = 1;
			pusht = 1;
			break;
		case KWD_MEMORY:
			/* override reserved memory blocks. */
			bit_nclear(mem_avail, 0, MEMBLKS-1);
			for (bp = pool_mem; bp; bp = next) {
				next = bp->next;
				free(bp);
			}
			pool_mem = NULL;

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
		case KWD_CARD:
			/* Card definition. */
			parse_card(DT_VERS);
			break;
		case KWD_FUNCTION:
			/* Function definition. */
			parse_card(DT_FUNC);
			break;
		case KWD_DEBUGLEVEL:
			i = debuglevel_tok(0);
			if (i > 0)
				debug_level = i;
			break;
		case KWD_INCLUDE:
			incl = newstr(next_tok());
			file_include(incl);
			break;
		default:
			error("syntax error");
			pusht = 0;
			if (errors++ >= MAXERRORS) {
				error("too many errors, giving up");
				return;
			}
			break;
		}
}

/*
 *	Parse a card definition.
 */
static void
parse_card(int deftype)
{
	char   *man, *vers, *tmp;
	char   *add_info;
	unsigned char index_type;
	struct card *cp;
	int     i, iosize;
	struct card_config *confp, *lastp;
	struct ether *ether;

	confp = 0;
	cp = xmalloc(sizeof(*cp));
	cp->deftype = deftype;
	switch (deftype) {
	case DT_VERS:
		man = newstr(next_tok());
		vers = newstr(next_tok());
		add_info = newstr(next_tok());
		if (keyword(add_info)) {
			pusht = 1;
			free(add_info);
			cp->add_info1 = NULL;
			cp->add_info2 = NULL;
		} else {
			cp->add_info1 = add_info;
			add_info = newstr(next_tok());
			if (keyword(add_info)) {
				pusht = 1;
				free(add_info);
				cp->add_info2 = NULL;
			} else {
				cp->add_info2 = add_info;
			}
		}
		cp->manuf = man;
		cp->version = vers;
		cp->logstr = NULL;
		asprintf(&cp->logstr, "%s (%s)", man, vers);
		cp->func_id = 0;
		break;
	case DT_FUNC:
		cp->manuf = NULL;
		cp->version = NULL;
		cp->logstr = NULL;
		cp->func_id = (u_char) func_tok();
		break;
	default:
		fprintf(stderr, "parse_card: unknown deftype %d\n", deftype);
		exit(1);
	}
	cp->reset_time = 50;
	cp->next = 0;
	if (!last_card) {
		cards = last_card = cp;
	} else {
		last_card->next = cp;
		last_card = cp;
	}
	for (;;) {
		switch (keyword(next_tok())) {
		case KWD_CONFIG:
			/* config */
			i = config_tok(&index_type);
			if (i == -1) {
				error("illegal card config index");
				break;
			}
			confp = xmalloc(sizeof(*confp));
			man = next_tok();
			confp->driver = new_driver(man);
			confp->irq = irq_tok(1);
			confp->flags = num_tok();
			if (confp->flags == -1) {
				pusht = 1;
				confp->flags = 0;
			}
			if (confp->irq < 0 || confp->irq > 15) {
				error("illegal card IRQ value");
				break;
			}
			confp->index = i & 0x3F;
			confp->index_type = index_type;

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
		case KWD_RESET:
			/* reset */
			i = num_tok();
			if (i == -1) {
				error("illegal card reset time");
				break;
			}
			cp->reset_time = i;
			break;
		case KWD_ETHER:
			/* ether */
			ether = xmalloc(sizeof(*ether));
			ether->type = ETHTYPE_GENERIC;
			tmp = next_tok();
			if (strcmp("attr2", tmp) == 0)
				ether->type = ETHTYPE_ATTR2;
			else {
				pusht = 1;
				ether->value = num_tok();
				if (ether->value == -1) {
					error("illegal ether address offset");
					free(ether);
					break;
				}
			}
			ether->next = cp->ether;
			cp->ether = ether;
			break;
		case KWD_INSERT:
			/* insert */
			addcmd(&cp->insert);
			break;
		case KWD_REMOVE:
			/* remove */
			addcmd(&cp->remove);
			break;
		case KWD_IOSIZE:
			/* iosize */
			iosize = iosize_tok();
			if (!iosize) {
				error("Illegal cardio arguments");
				break;
			}
			if (!confp) {
				error("iosize should be placed after config");
				break;
			}
			cp->iosize = iosize;
			break;
		case KWD_LOGSTR:
			free(cp->logstr);
			cp->logstr = newstr(next_tok());
			break;
		default:
			pusht = 1;
			return;
		}
	}
}

/*
 *	Generate a new driver structure. If one exists, use
 *	that one after confirming the correct class.
 */
static struct driver *
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
	drvp->unit = -1;
	p = drvp->kernel;
	while (*p++)
		if (*p >= '0' && *p <= '9') {
			drvp->unit = atoi(p);
			*p = 0;
			break;
		}
#ifdef	DEBUG
	printf("Drv %s%d created\n", drvp->kernel, drvp->unit);
#endif
	return (drvp);
}


/*
 *	Parse one I/O block.
 */
static struct allocblk *
ioblk_tok(int force)
{
	struct allocblk *io;
	int     i, j;

	/* ignore the keyword to allow separete blocks in multiple lines */
	if (keyword(next_tok()) != KWD_IO) {
		pusht = 1;
	}

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
		error("illegal or missing I/O block spec");
	return (0);
}

/*
 *	Parse a memory block.
 */
static struct allocblk *
memblk_tok(int force)
{
	struct allocblk *mem;
	int     i, j;

	/* ignore the keyword to allow separete blocks in multiple lines */
	if (keyword(next_tok()) != KWD_MEMORY) {
		pusht = 1;
	}

	if ((i = num_tok()) >= 0) {
		if ((j = num_tok()) < 0)
			error("illegal memory block");
		else {
			mem = xmalloc(sizeof(*mem));
			mem->addr = i & ~(MEMUNIT - 1);
			mem->size = (j + MEMUNIT - 1) & ~(MEMUNIT - 1);
			if (i < MEMSTART || (i + j) > MEMEND) {
				error("memory address out of range");
				if (force) {
					free(mem);
					mem = 0;
				} else
					mem->addr = mem->size = 0;
			}
			return (mem);
		}
	}
	if (force)
		error("illegal or missing memory block spec");
	return (0);
}

/*
 *	IRQ token. Must be number > 0 && < 16.
 *	If force is set, IRQ must exist, and can also be '?'.
 */
static int
irq_tok(int force)
{
	int     i;

	/* ignore the keyword to allow separete blocks in multiple lines */
	if (keyword(next_tok()) != KWD_IRQ) {
		pusht = 1;
	}

	if (strcmp("?", next_tok()) == 0 && force)
		return (0);
	pusht = 1;
	i = num_tok();
	if (i > 0 && i < 16)
		return (i);
	if (force)
		error("illegal IRQ value");
	return (-1);
}

/*
 *	Config index token
 */
static int
config_tok(unsigned char *index_type)
{
	if (strcmp("default", next_tok()) == 0)	{
		*index_type = DEFAULT_INDEX;
		return 0;
	}
	pusht = 1;
	if (strcmp("auto", next_tok()) == 0) {
		*index_type = AUTO_INDEX;
		return 0;
	}
	pusht = 1;
	*index_type = NORMAL_INDEX;
	return num_tok();
}
/*
 *	Function ID token
 */
static int
func_tok(void)
{
	if (strcmp("serial", next_tok()) == 0)	
		return 2;
	pusht = 1;
	if (strcmp("fixed_disk", next_tok()) == 0)	
		return 4;
	pusht = 1;
	return num_tok();
}


/*
 *	debuglevel token. Must be between 0 and 9.
 */
static int
debuglevel_tok(int force)
{
	int     i;

	i = num_tok();
	if (i >= 0 && i <= 9)
		return (i);
	return (-1);
}

/*
 *	iosize token
 *	iosize {<size>|auto}
 */
static int
iosize_tok(void)
{
	int iosize = 0;
	if (strcmp("auto", next_tok()) == 0)
		iosize = -1;	/* wildcard */
	else {
		pusht = 1;
		iosize = num_tok();
		if (iosize == -1)
			return 0;
	}
#ifdef DEBUG
	if (doverbose)
		printf("iosize: size=%x\n", iosize);
#endif
	return iosize;
}


/*
 *	search the table for a match.
 */
static int
keyword(char *str)
{
	char  **s;
	int     i = 1;

	for (s = keys; *s; s++, i++)
		if (strcmp(*s, str) == 0)
			return (i);

	/* search keyword aliases too */
	for (i = 0; key_aliases[i].key ; i++)
		if (strcmp(key_aliases[i].alias, str) == 0)
			return (key_aliases[i].key);

	return (0);
}

/*
 *	addcmd - Append the command line to the list of
 *	commands.
 */
static void
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

static void
error(char *msg)
{
	pusht = 1;
	logmsg("%s: %s at line %d, near %s\n",
	    filename, msg, lineno, next_tok());
	pusht = 1;
}

static int     last_char;

static int
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
	if (c == '#') 
		while (((c = get()) != '\n') && (c != EOF));
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
static int
num_tok(void)
{
	char   *s = next_tok(), c;
	int     val = 0, base;

	base = 10;
	c = *s++;
	if (c == '0') {
		base = 8;
		c = *s++;
		if (c == '\0') return 0; 
		else if (c == 'x' || c == 'X') {
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

static char   *_next_tok(void);

static char *
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
static char *
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
				error("unterminated string");
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
		case '-':
		case '?':
		case '*':
			/* Special characters that are tokens on their own. */
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
			if (includes) {
				fclose(in);
				/* go back to previous config file */
				includes--;
				in = configfiles[includes].filep;
				filename = configfiles[includes].filename;
				lineno = configfiles[includes].lineno;
				return _next_tok();	/* recursive */
			}
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
static char *
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

/*
 *	Include configuration file
 */
static void
file_include(char *incl)
{
	int	i, included;
	FILE	*fp;

	/* check nesting overflow */
	if (includes >= MAXINCLUDES) {
		if (debug_level >= 1) {
			logmsg("%s: include nesting overflow "
			    "at line %d, near %s\n", filename, lineno, incl);
		}
		free(incl);
		goto out;
	}

	/* check recursive inclusion */
	for (i = 0, included = 0; i <= includes; i++) {
		if (strcmp(incl, configfiles[i].filename) == 0) {
			included = 1;
			break;
		}
	}
	if (included == 1) {
		if (debug_level >= 1) {
			logmsg("%s: can't include the same file twice "
			    "at line %d, near %s\n", filename, lineno, incl);
		}
		free(incl);
		goto out;
	}

	if (!(fp = fopen(incl, "r"))) {
		if (debug_level >= 1) {
			logmsg("%s: can't open include file "
			    "at line %d, near %s\n", filename, lineno, incl);
		}
		free(incl);
		goto out;
	}

	/* save line number of the current config file */
	configfiles[includes].lineno = lineno;
	lineno = 1;

	/* now we start parsing new config file */
	includes++;
	in = configfiles[includes].filep = fp;
	filename = configfiles[includes].filename = incl;
out:
	return;
}
