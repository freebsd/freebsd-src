/*
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 Jordan K. Hubbard
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 *
 * This code is derived from software contributed by the 
 * University of California Berkeley, Jordan K. Hubbard,
 * David Greenman and Naffy, the Wonder Porpoise.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id: userconfig.c,v 1.15 1994/11/14 03:22:28 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <i386/i386/cons.h>

#include <i386/isa/isa_device.h>

#define PARM_DEVSPEC	0x1
#define PARM_INT	0x2
#define PARM_ADDR	0x3

#ifndef TRUE
#define TRUE	(1)
#define FALSE	(0)
#endif

typedef struct _cmdparm {
    int type;
    union {
	struct isa_device *dparm;
	int iparm;
	void *aparm;
    } parm;
} CmdParm;

typedef int (*CmdFunc)(CmdParm *);

typedef struct _cmd {
    char *name;
    CmdFunc handler;
    CmdParm *parms;
} Cmd;

static void lsdevtab(struct isa_device *);
static struct isa_device *find_device(char *, int);
static struct isa_device *search_devtable(struct isa_device *, char *, int);
static void cngets(char *, int);
static Cmd *parse_cmd(char *);
static int parse_args(char *, CmdParm *);
int strncmp(const char *, const char *, size_t);
unsigned long strtoul(const char *, char **, int);

static int list_devices(CmdParm *);
static int set_device_ioaddr(CmdParm *);
static int set_device_irq(CmdParm *);
static int set_device_drq(CmdParm *);
static int set_device_mem(CmdParm *);
static int set_device_flags(CmdParm *);
static int set_device_enable(CmdParm *);
static int set_device_disable(CmdParm *);
static int quitfunc(CmdParm *);
static int helpfunc(CmdParm *);

static int lineno;

static CmdParm addr_parms[] = {
    { PARM_DEVSPEC, {} },
    { PARM_ADDR, {} },
    { -1, {} },
};

static CmdParm int_parms[] = {
    { PARM_DEVSPEC, {} },
    { PARM_INT, {} },
    { -1, {} },
};

static CmdParm dev_parms[] = {
    { PARM_DEVSPEC, {} },
    { -1, {} },
};

static Cmd CmdList[] = {
    { "?", 	helpfunc, 		NULL },		/* ? (help)	*/
    { "di",	set_device_disable,	dev_parms },	/* disable dev	*/
    { "dr",	set_device_drq,		int_parms },	/* drq dev #	*/
    { "en",	set_device_enable,	dev_parms },	/* enable dev	*/
    { "ex", 	quitfunc, 		NULL },		/* exit (quit)	*/
    { "f",	set_device_flags,	int_parms },	/* flags dev mask */
    { "h", 	helpfunc, 		NULL },		/* help		*/
    { "io",	set_device_mem,		addr_parms },	/* iomem dev addr */
    { "ir",	set_device_irq,		int_parms },	/* irq dev #	*/
    { "l",	list_devices,		NULL },		/* ls, list	*/
    { "p",	set_device_ioaddr,	int_parms },	/* port dev addr */
    { "q", 	quitfunc, 		NULL },		/* quit		*/
    { NULL,	NULL,			NULL },
};

void
userconfig(void)
{
    char command[80];
    char input[80];
    int rval;
    struct isa_device *dt;
    Cmd *cmd;

    while (1) {
	printf("config> ");
	cngets(input, 80);
	if (input[0] == '\0')
	    continue;
	cmd = parse_cmd(input);
	if (!cmd) {
	    printf("Invalid command or syntax.  Type `?' for help.\n");
	    continue;
	}
	rval = (*cmd->handler)(cmd->parms);
	if (rval)
	    return;
    }
}

static Cmd *
parse_cmd(char *cmd)
{
    Cmd *cp;

    for (cp = CmdList; cp->name; cp++) {
	int len = strlen(cp->name);

	if (!strncmp(cp->name, cmd, len)) {
	    while (*cmd && *cmd != ' ' && *cmd != '\t')
		++cmd;
	    if (parse_args(cmd, cp->parms))
		return NULL;
	    else
		return cp;
	}
    }
    return NULL;
}

static int
parse_args(char *cmd, CmdParm *parms)
{
    while (1) {
	char *ptr;

	if (*cmd == ' ' || *cmd == '\t') {
	    ++cmd;
	    continue;
	}
	if (parms == NULL || parms->type == -1) {
		if (*cmd == '\0')
			return 0;
		printf("Extra arg(s): %s\n", cmd);
		return 1;
	}
	if (parms->type == PARM_DEVSPEC) {
	    int i = 0;
	    char devname[64];
	    int unit = 0;

	    while (*cmd && !(*cmd == ' ' || *cmd == '\t' ||
	      (*cmd >= '0' && *cmd <= '9')))
		devname[i++] = *(cmd++);
	    devname[i] = '\0';
	    if (*cmd >= '0' && *cmd <= '9') {
		unit = strtoul(cmd, &ptr, 10);
		if (cmd == ptr) {
		    printf("Invalid device number\n");
		    /* XXX should print invalid token here and elsewhere. */
		    return 1;
		}
		/* XXX else should require end of token. */
		cmd = ptr;
	    }
	    if ((parms->parm.dparm = find_device(devname, unit)) == NULL) {
	        printf("No such device: %s%d\n", devname, unit);
		return 1;
	    }
	    ++parms;
	    continue;
	}
	if (parms->type == PARM_INT) {
	    parms->parm.iparm = strtoul(cmd, &ptr, 0);
	    if (cmd == ptr) {
	        printf("Invalid numeric argument\n");
		return 1;
	    }
	    cmd = ptr;
	    ++parms;
	    continue;
	}
	if (parms->type == PARM_ADDR) {
	    parms->parm.aparm = (void *)strtoul(cmd, &ptr, 0);
	    if (cmd == ptr) {
	        printf("Invalid address argument\n");
	        return 1;
	    }
	    cmd = ptr;
	    ++parms;
	    continue;
	}
    }
    return 0;
}

static int
list_devices(CmdParm *parms)
{
    lineno = 0;
    lsdevtab(&isa_devtab_bio[0]);
    lsdevtab(&isa_devtab_tty[0]);
    lsdevtab(&isa_devtab_net[0]);
    lsdevtab(&isa_devtab_null[0]);
    return 0;
}

static int
set_device_ioaddr(CmdParm *parms)
{
    parms[0].parm.dparm->id_iobase = parms[1].parm.iparm;
    return 0;
}

static int
set_device_irq(CmdParm *parms)
{
    unsigned irq;

    irq = parms[1].parm.iparm;
    parms[0].parm.dparm->id_irq = (irq < 16 ? 1 << irq : 0);
    return 0;
}

static int
set_device_drq(CmdParm *parms)
{
    unsigned drq;

    /*
     * The bounds checking is just to ensure that the value can be printed
     * in 5 characters.  32768 gets converted to -32768 and doesn't fit.
     */
    drq = parms[1].parm.iparm;
    parms[0].parm.dparm->id_drq = (drq < 32768 ? drq : -1);
    return 0;
}

static int
set_device_mem(CmdParm *parms)
{
    parms[0].parm.dparm->id_maddr = parms[1].parm.aparm;
    return 0;
}

static int
set_device_flags(CmdParm *parms)
{
    parms[0].parm.dparm->id_flags = parms[1].parm.iparm;
    return 0;
}

static int
set_device_enable(CmdParm *parms)
{
    parms[0].parm.dparm->id_enabled = TRUE;
    return 0;
}

static int
set_device_disable(CmdParm *parms)
{
    parms[0].parm.dparm->id_enabled = FALSE;
    return 0;
}

static int
quitfunc(CmdParm *parms)
{
    return 1;
}

static int
helpfunc(CmdParm *parms)
{
    printf("Command\t\t\tDescription\n");
    printf("-------\t\t\t-----------\n");
    printf("ls\t\t\tList currently configured devices\n");
    printf("port <devname> <addr>\tSet device port (i/o address)\n");
    printf("irq <devname> <number>\tSet device irq\n");
    printf("drq <devname> <number>\tSet device drq\n");
    printf("iomem <devname> <addr>\tSet device maddr (memory address)\n");
    printf("flags <devname> <mask>\tSet device flags\n");
    printf("enable <devname>\tEnable device\n");
    printf("disable <devname>\tDisable device (will not be probed)\n");
    printf("quit\t\t\tExit this configuration utility\n");
    printf("help\t\t\tThis message\n\n");
    printf("Commands may be abbreviated to a unique prefix\n");
    return 0;
}

static void
lsdevtab(struct isa_device *dt)
{
    for (; dt->id_id != 0; dt++) {
	int i;
	char line[80];

	if (lineno >= 23) {
		printf("<More> ");
		(void)cngetc();
		printf("\n");
		lineno = 0;
	}
	if (lineno == 0) {
		printf(
"Device   port       irq   drq   iomem      unit  flags      enabled\n");
		++lineno;
	}
	/*
	 * printf() doesn't support %#, %- or even field widths for strings,
	 * so formatting is not straightforward.
	 */
	bzero(line, sizeof line);
	sprintf(line, "%s%d", dt->id_driver->name, dt->id_unit);
	/* Missing: id_id (don't need it). */
	/* Missing: id_driver (useful if we could show it by name). */
	sprintf(line + 9, "0x%x", dt->id_iobase);
	sprintf(line + 20, "%d", ffs(dt->id_irq) - 1);
	sprintf(line + 26, "%d", dt->id_drq);
	sprintf(line + 32, "0x%x", dt->id_maddr);
	/* Missing: id_msize (0 at start, useful if we can get here later). */
	/* Missing: id_intr (useful if we could show it by name). */
	/* Display only: id_unit. */
	sprintf(line + 43, "%d", dt->id_unit);
	sprintf(line + 49, "0x%x", dt->id_flags);
	/* Missing: id_scsiid, id_alive, id_ri_flags, id_reconfig (0 now...) */
	sprintf(line + 60, "%s", dt->id_enabled ? "Yes" : "No");
	for (i = 0; i < 60; ++i)
		if (line[i] == '\0')
			line[i] = ' ';
	printf("%s\n", line);
	++lineno;
    }
}

static struct isa_device *
find_device(char *devname, int unit)
{
    struct isa_device *ret;

    if ((ret = search_devtable(&isa_devtab_bio[0], devname, unit)) != NULL)
        return ret;
    if ((ret = search_devtable(&isa_devtab_tty[0], devname, unit)) != NULL)
        return ret;
    if ((ret = search_devtable(&isa_devtab_net[0], devname, unit)) != NULL)
        return ret;
    if ((ret = search_devtable(&isa_devtab_null[0], devname, unit)) != NULL)
        return ret;
    return NULL;
}

static struct isa_device *
search_devtable(struct isa_device *dt, char *devname, int unit)
{
    int i;

    for (i = 0; dt->id_id != 0; dt++)
        if (!strcmp(dt->id_driver->name, devname) && dt->id_unit == unit)
	    return dt;
    return NULL;
}

void
cngets(char *input, int maxin)
{
    int c, nchars = 0;

    while (1) {
	c = cngetc();
	/* Treat ^H or ^? as backspace */
	if ((c == '\010' || c == '\177')) {
	    	if (nchars) {
			printf("\010 \010");
			*--input = '\0', --nchars;
		}
		continue;
	}
	/* Treat ^U or ^X as kill line */
	else if ((c == '\025' || c == '\030')) {
		while (nchars) {
			printf("\010 \010");
			*--input = '\0', --nchars;
		}
		continue;
	}
	printf("%c", c);
	if ((++nchars == maxin) || (c == '\n') || (c == '\r')) {
	    *input = '\0';
	    break;
	}
	*input++ = (u_char)c;
    }
}

int
strncmp(const char *s1, const char *s2, size_t n)
{

    if (n == 0)
	return (0);
    do {
	if (*s1 != *s2++)
	    return (*(unsigned char *)s1 - *(unsigned char *)--s2);
	if (*s1++ == 0)
	    break;
    } while (--n != 0);
    return (0);
}

/*
 * Kludges to get the library sources of strtoul.c to work in our
 * environment.  isdigit() and isspace() could be used above too.
 */
#define	isalpha(c)	(((c) >= 'A' && (c) <= 'Z') \
			 || ((c) >= 'a' && (c) <= 'z'))		/* unsafe */
#define	isdigit(c)	((unsigned)((c) - '0') <= '9' - '0')
#define	isspace(c)	((c) == ' ' || (c) == '\t')		/* unsafe */
#define	isupper(c)	((unsigned)((c) - 'A') <= 'Z' - 'A')

static int errno;

/*
 * The following should be identical with the library sources for strtoul.c.
 */

/*
 * Convert a string to an unsigned long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
unsigned long
strtoul(nptr, endptr, base)
	const char *nptr;
	char **endptr;
	register int base;
{
	register const char *s = nptr;
	register unsigned long acc;
	register int c;
	register unsigned long cutoff;
	register int neg = 0, any, cutlim;

	/*
	 * See strtol for comments as to the logic used.
	 */
	do {
		c = *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else if (c == '+')
		c = *s++;
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;
	cutoff = (unsigned long)ULONG_MAX / (unsigned long)base;
	cutlim = (unsigned long)ULONG_MAX % (unsigned long)base;
	for (acc = 0, any = 0;; c = *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || acc == cutoff && c > cutlim)
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0) {
		acc = ULONG_MAX;
		errno = ERANGE;
	} else if (neg)
		acc = -acc;
	if (endptr != 0)
		*endptr = (char *)(any ? s - 1 : nptr);
	return (acc);
}
