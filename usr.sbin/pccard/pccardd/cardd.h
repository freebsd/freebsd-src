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
 * $FreeBSD$
 *
 *	Common include file for PCMCIA daemon
 */
#include <bitstring.h>

#include <pccard/cardinfo.h>
#include <pccard/cis.h>

#include "readcis.h"

#ifndef EXTERN
#define EXTERN extern
#endif

struct cmd {
	struct cmd *next;
	char   *line;		/* Command line */
	int     macro;		/* Contains macros */
};

struct card_config {
	struct card_config *next;
	unsigned char index_type;
	unsigned char index;
	struct driver *driver;
	int     irq;
	int     flags;
	char    inuse;
};

struct ether {
	struct ether *next;
	int	type;
	int	value;
};

#define	ETHTYPE_GENERIC		0
#define	ETHTYPE_ATTR2		1

struct card {
	struct card *next;
	char   *manuf;
	char   *version;
	char   *add_info1;
	char   *add_info2;
	u_char  func_id;
	int     deftype;
	struct ether *ether;		/* For net cards, ether at offset */
	int     reset_time;		/* Reset time */
	int	iosize;			/* I/O window size (ignore location) */
	struct card_config *config;	/* List of configs */
	struct cmd *insert;		/* Insert commands */
	struct cmd *remove;		/* Remove commands */
	char   *logstr;			/* String for logger */
};

struct driver {
	struct driver *next;
	char   *name;
	char   *kernel;			/* Kernel driver base name */
	int     unit;			/* Unit of driver */
	/*
	 * The rest of the structure is allocated dynamically.
	 * Once allocated, it stays allocated.
	 */
	struct card *card;		/* Current card, if any */
	struct card_config *config;	/* Config back ptr */
	unsigned int mem;		/* Allocated host address (if any) */
	int     inuse;
};

/*
 *	Defines one allocation block i.e a starting address
 *	and size. Used for either memory or I/O ports
 */
struct allocblk {
	struct allocblk *next;
	int     addr;			/* Address */
	int     size;			/* Size */
	int     flags;			/* Flags for block */
	int     cardaddr;		/* Card address */
};
/*
 *	Slot structure - data held for each slot.
 */
struct slot {
	struct slot *next;
	int     fd;
	int     mask;
	int     slot;
	char   *name;
	enum cardstate state;
	struct cis *cis;
	struct card *card;		/* Current card */
	struct card_config *config;	/* Current configuration */
	struct cis_config *card_config;
	char    devname[16];
	u_int	manufacturer;
	u_int	product;
	u_int	prodext;
	unsigned char eaddr[6];		/* If any */
	char	manufstr[DEV_MAX_CIS_LEN];
	char	versstr[DEV_MAX_CIS_LEN];
	struct allocblk io;		/* I/O block spec */
	struct allocblk mem;		/* Memory block spec */
	int     irq;			/* Irq value */
	int	flags;			/* Resource assignment flags */
};

/*
 * Slot resource assignment/configuration flags
 */
#define IO_ASSIGNED	0x1
#define MEM_ASSIGNED	0x2
#define IRQ_ASSIGNED	0x4
#define EADDR_CONFIGED	0x8
#define WL_CONFIGED	0x10
#define AFLAGS	(IO_ASSIGNED | MEM_ASSIGNED | IRQ_ASSIGNED)
#define CFLAGS	(EADDR_CONFIGED | WL_CONFIGED)

EXTERN struct slot *slots, *current_slot;
EXTERN int slen;

EXTERN struct allocblk *pool_ioblks;            /* I/O blocks in the pool */
EXTERN struct allocblk *pool_mem;               /* Memory in the pool */
EXTERN int     pool_irq[16];			/* IRQ allocations */
EXTERN int     irq_init[16];			/* initial IRQ allocations */
EXTERN struct driver *drivers;			/* List of drivers */
EXTERN struct card *cards;
EXTERN struct card *last_card;
EXTERN bitstr_t *mem_avail;
EXTERN bitstr_t *mem_init;
EXTERN bitstr_t *io_avail;
EXTERN bitstr_t *io_init;
EXTERN int pccard_init_sleep;			/* Time to sleep on init */
EXTERN int use_kern_irq;
EXTERN int debug_level;

/* cardd.c functions */
void		 dump_config_file(void);
struct slot	*readslots(void);
void		 slot_change(struct slot *);

/* util.c functions */
unsigned long	 alloc_memory(int);
int		 bit_fns(bitstr_t *, int, int, int, int);
void		 die(char *);
void		 execute(struct cmd *, struct slot *);
void		 logmsg(const char *, ...);
void		 log_setup(void);
void		 logerr(char *);
char		*newstr();
void		 reset_slot(struct slot *);
void		*xmalloc(int);

/* file.c functions */
void		 readfile(char *);

/* server.c functions */
void		 set_socket(int);
void		 stat_changed(struct slot *);
void		 process_client(void);

#define	IOPORTS	0x400
#define	MEMUNIT	0x1000
#define	MEMSTART 0xA0000
#define	MEMEND	0x100000
#define	MEMBLKS	((MEMEND-MEMSTART)/MEMUNIT)
#define	MEM2BIT(x) (((x)-MEMSTART)/MEMUNIT)
#define	BIT2MEM(x) (((x)*MEMUNIT)+MEMSTART)

#define MAXINCLUDES	10
#define MAXERRORS	10

/*
 * Config index types
 */
#define NORMAL_INDEX	0
#define DEFAULT_INDEX	1
#define AUTO_INDEX	2

#define DT_VERS 0
#define DT_FUNC 1
