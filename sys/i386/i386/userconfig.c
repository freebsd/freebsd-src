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
 *      $Id: userconfig.c,v 1.20 1995/03/02 20:07:05 dufault Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <i386/i386/cons.h>

#include <i386/isa/isa_device.h>

#include <scsi/scsiconf.h>
#include "scbus.h"

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

struct isa_device	*isa_devlist = NULL;

#if NSCBUS > 0
static void lsscsi(void);
static int list_scsi(CmdParm *);
#endif

static void lsdevtab(struct isa_device *);
static struct isa_device *find_device(char *, int);
static struct isa_device *search_devtable(struct isa_device *, char *, int);
static void cngets(char *, int);
static Cmd *parse_cmd(char *);
static int parse_args(char *, CmdParm *);
unsigned long strtoul(const char *, char **, int);
static int save_dev(struct isa_device *);

static int list_devices(CmdParm *);
static int set_device_ioaddr(CmdParm *);
static int set_device_irq(CmdParm *);
static int set_device_drq(CmdParm *);
static int set_device_iosize(CmdParm *);
static int set_device_mem(CmdParm *);
static int set_device_flags(CmdParm *);
static int set_device_enable(CmdParm *);
static int set_device_disable(CmdParm *);
static int device_attach(CmdParm *);
static int device_probe(CmdParm *);
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
    { "a",	device_attach,		dev_parms },	/* attach dev */
    { "di",	set_device_disable,	dev_parms },	/* disable dev	*/
    { "dr",	set_device_drq,		int_parms },	/* drq dev #	*/
    { "en",	set_device_enable,	dev_parms },	/* enable dev	*/
    { "ex", 	quitfunc, 		NULL },		/* exit (quit)	*/
    { "f",	set_device_flags,	int_parms },	/* flags dev mask */
    { "h", 	helpfunc, 		NULL },		/* help		*/
    { "iom",	set_device_mem,		addr_parms },	/* iomem dev addr */
    { "ios",	set_device_iosize,	int_parms },	/* iosize dev size */
    { "ir",	set_device_irq,		int_parms },	/* irq dev #	*/
    { "l",	list_devices,		NULL },		/* ls, list	*/
    { "po",	set_device_ioaddr,	int_parms },	/* port dev addr */
    { "pr",	device_probe,		dev_parms },	/* probe dev */
    { "q", 	quitfunc, 		NULL },		/* quit		*/
#if NSCBUS > 0
    { "s",	list_scsi,		NULL },		/* scsi */
#endif
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
    save_dev(parms[0].parm.dparm);
    return 0;
}

static int
set_device_irq(CmdParm *parms)
{
    unsigned irq;

    irq = parms[1].parm.iparm;
    parms[0].parm.dparm->id_irq = (irq < 16 ? 1 << irq : 0);
    save_dev(parms[0].parm.dparm);
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
    save_dev(parms[0].parm.dparm);
    return 0;
}

static int
set_device_iosize(CmdParm *parms)
{
    parms[0].parm.dparm->id_msize = parms[1].parm.iparm;
    save_dev(parms[0].parm.dparm);
    return 0;
}

static int
set_device_mem(CmdParm *parms)
{
    parms[0].parm.dparm->id_maddr = parms[1].parm.aparm;
    save_dev(parms[0].parm.dparm);
    return 0;
}

static int
set_device_flags(CmdParm *parms)
{
    parms[0].parm.dparm->id_flags = parms[1].parm.iparm;
    save_dev(parms[0].parm.dparm);
    return 0;
}

static int
set_device_enable(CmdParm *parms)
{
    parms[0].parm.dparm->id_enabled = TRUE;
    save_dev(parms[0].parm.dparm);
    return 0;
}

static int
set_device_disable(CmdParm *parms)
{
    parms[0].parm.dparm->id_enabled = FALSE;
    save_dev(parms[0].parm.dparm);
    return 0;
}

static int
device_attach(CmdParm *parms)
{
    int status;

    status = (*(parms[0].parm.dparm->id_driver->attach))(parms[0].parm.dparm);
    printf("attach returned status of 0x%x\n", status);
    return 0;
}

static int
device_probe(CmdParm *parms)
{
    int status;

    status = (*(parms[0].parm.dparm->id_driver->probe))(parms[0].parm.dparm);
    printf("probe returned status of 0x%x\n", status);
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
    printf("attach <devname>\tReturn results of device attach\n");
    printf("ls\t\t\tList currently configured devices\n");
    printf("port <devname> <addr>\tSet device port (i/o address)\n");
    printf("irq <devname> <number>\tSet device irq\n");
    printf("drq <devname> <number>\tSet device drq\n");
    printf("iomem <devname> <addr>\tSet device maddr (memory address)\n");
    printf("iosize <devname> <size>\tSet device memory size\n");
    printf("flags <devname> <mask>\tSet device flags\n");
    printf("enable <devname>\tEnable device\n");
    printf("probe <devname>\t\tReturn results of device probe\n");
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
"Device   port       irq   drq   iomem   iosize   unit  flags      enabled\n");
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
	sprintf(line + 40, "%d", dt->id_msize);
	/* Missing: id_msize (0 at start, useful if we can get here later). */
	/* Missing: id_intr (useful if we could show it by name). */
	/* Display only: id_unit. */
	sprintf(line + 49, "%d", dt->id_unit);
	sprintf(line + 55, "0x%x", dt->id_flags);
	/* Missing: id_scsiid, id_alive, id_ri_flags, id_reconfig (0 now...) */
	sprintf(line + 66, "%s", dt->id_enabled ? "Yes" : "No");
	for (i = 0; i < 66; ++i)
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

#if NSCBUS > 0
/* scsi: Support for displaying configured SCSI devices.
 * There is no way to edit them, and this is inconsistent
 * with the ISA method.  This is here as a basis for further work.
 */
static char *
type_text(char *name)	/* XXX: This is bogus */
{
	if (strcmp(name, "sd") == 0)
		return "disk";

	if (strcmp(name, "st") == 0)
		return "tape";

	return "device";
}

static void
id_put(char *desc, int id)
{
    if (id != SCCONF_UNSPEC)
    {
    	if (desc)
	    printf("%s", desc);

    	if (id == SCCONF_ANY)
	    printf("?");
        else
	    printf("%d", id);
    }
}

static void
lsscsi(void)
{
    int i;

    printf("scsi: (can't be edited):\n");

    for (i = 0; scsi_cinit[i].driver; i++)
    {
	id_put("controller scbus", scsi_cinit[i].bus);

	if (scsi_cinit[i].unit != -1)
	{
	    printf(" at ");
	    id_put(scsi_cinit[i].driver, scsi_cinit[i].unit);
	}

	printf("\n");
    }

    for (i = 0; scsi_dinit[i].name; i++)
    {
		printf("%s ", type_text(scsi_dinit[i].name));

		id_put(scsi_dinit[i].name, scsi_dinit[i].unit);
		id_put(" at scbus", scsi_dinit[i].cunit);
		id_put(" target ", scsi_dinit[i].target);
		id_put(" lun ", scsi_dinit[i].lun);

		if (scsi_dinit[i].flags)
	    	printf("flags 0x%x\n", scsi_dinit[i].flags);

		printf("\n");
    }
}

static int
list_scsi(CmdParm *parms)
{
    lineno = 0;
    lsscsi();
    return 0;
}
#endif

static int
save_dev(idev)
struct isa_device 	*idev;
{
	struct isa_device	*id_p,*id_pn;
	
	for (id_p=isa_devlist;
	id_p;
	id_p=id_p->id_next) {
printf("Id=%d\n",id_p->id_id);
		if (id_p->id_id == idev->id_id) {
			id_pn = id_p->id_next;
			bcopy(idev,id_p,sizeof(struct isa_device));
			id_p->id_next = id_pn;
			return 1;
		}
	}
	id_pn = malloc(sizeof(struct isa_device),M_DEVL,M_WAITOK);
	bcopy(idev,id_pn,sizeof(struct isa_device));
	id_pn->id_next = isa_devlist;
	isa_devlist = id_pn;
	return 0;
}

