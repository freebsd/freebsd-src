
/*
 *	Common include file for PCMCIA daemon
 */
#include <bitstring.h>

#include <pccard/card.h>
#include <pccard/cis.h>

#include "readcis.h"

struct cmd {
	struct cmd *next;
	char   *line;		/* Command line */
	int     macro;		/* Contains macros */
};

struct card_config {
	struct card_config *next;
	unsigned char index;
	struct driver *driver;
	int     irq;
	int     flags;
	char    inuse;
};

struct card {
	struct card *next;
	char   *manuf;
	char   *version;
	int     ether;			/* For net cards, ether at offset */
	int     reset_time;		/* Reset time */
	struct card_config *config;	/* List of configs */
	struct cmd *insert;		/* Insert commands */
	struct cmd *remove;		/* Remove commands */
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
#if 0
	struct device *device;		/* System device info */
#endif
	unsigned int mem;		/* Allocated host address (if any) */
	int     inuse;
};

#if 0
struct device {
	struct device *next;		/* List of devices */
	int     inuse;			/* Driver being used */
	struct cmd *insert;		/* Insert commands */
	struct cmd *remove;		/* Remove commands */
};
#endif

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
	unsigned char eaddr[6];		/* If any */
	struct allocblk io;		/* I/O block spec */
	struct allocblk mem;		/* Memory block spec */
	int     irq;			/* Irq value */
};

struct slot *slots, *current_slot;

struct allocblk *pool_ioblks;		/* I/O blocks in the pool */
struct allocblk *pool_mem;		/* Memory in the pool */
int     pool_irq[16];			/* IRQ allocations */
struct driver *drivers;			/* List of drivers */
struct card *cards;
#if 0
struct device *devlist;
#endif
bitstr_t *mem_avail;
bitstr_t *io_avail;

int     verbose, do_log;

char   *newstr();
void    die(char *);
void   *xmalloc(int);
void    log_1s(char *, char *);
void    logerr(char *);
void    reset_slot(struct slot *);
void    execute(struct cmd *);
unsigned long alloc_memory(int size);
int     bit_fns(bitstr_t * nm, int nbits, int count);
void    readfile(char *name);

#define	IOPORTS	0x400
#define	MEMUNIT	0x1000
#define	MEMSTART 0xA0000
#define	MEMEND	0x100000
#define	MEMBLKS	((MEMEND-MEMSTART)/MEMUNIT)
#define	MEM2BIT(x) (((x)-MEMSTART)/MEMUNIT)
#define	BIT2MEM(x) (((x)*MEMUNIT)+MEMSTART)
