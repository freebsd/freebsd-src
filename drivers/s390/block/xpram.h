
/*
 * xpram.h -- definitions for the char module
 *
 *********/


#include <linux/ioctl.h>
#include <asm/atomic.h>
#include <linux/major.h>

/* version dependencies have been confined to a separate file */

/*
 * Macros to help debugging
 */

#define XPRAM_NAME "xpram"  /* name of device/module */
#define XPRAM_DEVICE_NAME_PREFIX "slram" /* Prefix device name for major 35 */
#define XPRAM_DEVS 1        /* one partition */
#define XPRAM_RAHEAD 8      /* no real read ahead */
#define XPRAM_PGSIZE 4096   /* page size of (expanded) mememory pages
                             * according to S/390 architecture
                             */
#define XPRAM_BLKSIZE XPRAM_PGSIZE  /* must be equalt to page size ! */
#define XPRAM_HARDSECT XPRAM_PGSIZE /* FIXME -- we have to deal with both
                                     * this hard sect size and in some cases
                                     * hard coded 512 bytes which I call
                                     * soft sects:
                                     */
#define XPRAM_SOFTSECT 512
#define XPRAM_MAX_DEVS 32   /* maximal number of devices (partitions) */
#define XPRAM_MAX_DEVS1 33  /* maximal number of devices (partitions) +1 */

/* The following macros depend on the sizes above */

#define XPRAM_KB_IN_PG 4                     /* 4 kBs per page */
#define XPRAM_KB_IN_PG_ORDER 2               /* 2^? kBs per page */

/* Eventhough XPRAM_HARDSECT is set to 4k some data structures use hard
 * coded 512 byte sa sector size
 */
#define XPRAM_SEC2KB(x) ((x >> 1) + (x & 1)) /* modifier used to compute size 
                                                in kB from number of sectors */
#define XPRAM_SEC_IN_PG 8                    /* 8 sectors per page */
#define XPRAM_SEC_IN_PG_ORDER 3              /* 2^? sectors per page */

#define XPRAM_UNUSED 40                     /* unused space between devices, 
                                             * in kB, i.e.
                                             * must be a multiple of 4
                                             */
/*
 * The xpram device is removable: if it is left closed for more than
 * half a minute, it is removed. Thus use a usage count and a
 * kernel timer
 */

typedef struct Xpram_Dev {
   int            size;   /* size in KB not in Byte - RB - */
   atomic_t       usage;
   char *         device_name;   /* device name prefix in devfs */
   devfs_handle_t devfs_entry;   /* handle needed to unregister dev from devfs */
   u8 *data;
}              Xpram_Dev;

/* 2.2: void xpram_setup (char *, int *); */
/* begin 2.3 */
int xpram_setup (char *);
/* end 2.3 */
int xpram_init(void);
