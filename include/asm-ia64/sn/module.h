/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_MODULE_H
#define _ASM_IA64_SN_MODULE_H

#ifdef	__cplusplus
extern "C" {
#endif


#include <linux/config.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/ksys/elsc.h>

#define MODULE_MAX			128
#define MODULE_MAX_NODES		2
#define MODULE_HIST_CNT			16
#define MAX_MODULE_LEN			16

/* Well-known module IDs */
#define MODULE_UNKNOWN		(-2) /* initial value of klconfig brd_module */
/* #define INVALID_MODULE	(-1) ** generic invalid moduleid_t (arch.h) */
#define MODULE_NOT_SET		0    /* module ID not set in sys ctlrs. */

/* parameter for format_module_id() */
#define MODULE_FORMAT_BRIEF	1
#define MODULE_FORMAT_LONG	2

/*
 *	Module id format
 *
 *	31-16	Rack ID (encoded class, group, number - 16-bit unsigned int)
 *	 15-8	Brick type (8-bit ascii character)
 *	  7-0	Bay (brick position in rack (0-63) - 8-bit unsigned int)
 *
 */

/*
 * Macros for getting the brick type
 */
#define MODULE_BTYPE_MASK	0xff00
#define MODULE_BTYPE_SHFT	8
#define MODULE_GET_BTYPE(_m)	(((_m) & MODULE_BTYPE_MASK) >> MODULE_BTYPE_SHFT)
#define MODULE_BT_TO_CHAR(_b)	((char)(_b))
#define MODULE_GET_BTCHAR(_m)	(MODULE_BT_TO_CHAR(MODULE_GET_BTYPE(_m)))

/*
 * Macros for getting the rack ID.
 */
#define MODULE_RACK_MASK	0xffff0000
#define MODULE_RACK_SHFT	16
#define MODULE_GET_RACK(_m)	(((_m) & MODULE_RACK_MASK) >> MODULE_RACK_SHFT)

/*
 * Macros for getting the brick position
 */
#define MODULE_BPOS_MASK	0x00ff
#define MODULE_BPOS_SHFT	0
#define MODULE_GET_BPOS(_m)	(((_m) & MODULE_BPOS_MASK) >> MODULE_BPOS_SHFT)

/*
 * Macros for constructing moduleid_t's
 */
#define RBT_TO_MODULE(_r, _b, _t) ((_r) << MODULE_RACK_SHFT | \
				   (_b) << MODULE_BPOS_SHFT | \
				   (_t) << MODULE_BTYPE_SHFT)

/*
 * Macros for encoding and decoding rack IDs
 * A rack number consists of three parts:
 *   class (0==CPU/mixed, 1==I/O), group, number
 *
 * Rack number is stored just as it is displayed on the screen:
 * a 3-decimal-digit number.
 */
#define RACK_CLASS_DVDR		100
#define RACK_GROUP_DVDR		10
#define RACK_NUM_DVDR		1

#define RACK_CREATE_RACKID(_c, _g, _n)	((_c) * RACK_CLASS_DVDR +	\
	(_g) * RACK_GROUP_DVDR + (_n) * RACK_NUM_DVDR)

#define RACK_GET_CLASS(_r)              ((_r) / RACK_CLASS_DVDR)
#define RACK_GET_GROUP(_r)              (((_r) - RACK_GET_CLASS(_r) *   \
            RACK_CLASS_DVDR) / RACK_GROUP_DVDR)
#define RACK_GET_NUM(_r)                (((_r) - RACK_GET_CLASS(_r) *   \
            RACK_CLASS_DVDR - RACK_GET_GROUP(_r) *      \
            RACK_GROUP_DVDR) / RACK_NUM_DVDR)

/*
 * Macros for encoding and decoding rack IDs
 * A rack number consists of three parts:
 *   class	1 bit, 0==CPU/mixed, 1==I/O
 *   group	2 bits for CPU/mixed, 3 bits for I/O
 *   number	3 bits for CPU/mixed, 2 bits for I/O (1 based)
 */
#define RACK_GROUP_BITS(_r)	(RACK_GET_CLASS(_r) ? 3 : 2)
#define RACK_NUM_BITS(_r)	(RACK_GET_CLASS(_r) ? 2 : 3)

#define RACK_CLASS_MASK(_r)	0x20
#define RACK_CLASS_SHFT(_r)	5
#define RACK_ADD_CLASS(_r, _c)	\
	((_r) |= (_c) << RACK_CLASS_SHFT(_r) & RACK_CLASS_MASK(_r))

#define RACK_GROUP_SHFT(_r)	RACK_NUM_BITS(_r)
#define RACK_GROUP_MASK(_r)	\
	( (((unsigned)1<<RACK_GROUP_BITS(_r)) - 1) << RACK_GROUP_SHFT(_r) )
#define RACK_ADD_GROUP(_r, _g)	\
	((_r) |= (_g) << RACK_GROUP_SHFT(_r) & RACK_GROUP_MASK(_r))

#define RACK_NUM_SHFT(_r)	0
#define RACK_NUM_MASK(_r)	\
	( (((unsigned)1<<RACK_NUM_BITS(_r)) - 1) << RACK_NUM_SHFT(_r) )
#define RACK_ADD_NUM(_r, _n)	\
	((_r) |= ((_n) - 1) << RACK_NUM_SHFT(_r) & RACK_NUM_MASK(_r))


/*
 * Brick type definitions
 */
#define MAX_BRICK_TYPES         256 /* brick type is stored as uchar */

extern char brick_types[];

#define MODULE_CBRICK           0
#define MODULE_RBRICK           1
#define MODULE_IBRICK           2
#define MODULE_KBRICK           3
#define MODULE_XBRICK           4
#define MODULE_DBRICK           5
#define MODULE_PBRICK           6
#define MODULE_NBRICK           7
#define MODULE_PEBRICK          8
#define MODULE_PXBRICK          9
#define MODULE_IXBRICK          10
#define MODULE_CGBRICK		11
#define MODULE_OPUSBRICK        12

/*
 * Moduleid_t comparison macros
 */
/* Don't compare the brick type:  only the position is significant */
#define MODULE_CMP(_m1, _m2)    (((_m1)&(MODULE_RACK_MASK|MODULE_BPOS_MASK)) -\
                                 ((_m2)&(MODULE_RACK_MASK|MODULE_BPOS_MASK)))
#define MODULE_MATCH(_m1, _m2)  (MODULE_CMP((_m1),(_m2)) == 0)

typedef struct module_s module_t;

struct module_s {
    moduleid_t		id;		/* Module ID of this module        */

    spinlock_t		lock;		/* Lock for this structure	   */

    /* List of nodes in this module */
    cnodeid_t		nodes[MAX_SLABS + 1];
    geoid_t		geoid[MAX_SLABS + 1];
    struct {
		char	 moduleid[8];
		uint64_t iobrick_type;
    } io[MAX_SLABS + 1];

    /* Fields for Module System Controller */
    int			mesgpend;	/* Message pending                 */
    int			shutdown;	/* Shutdown in progress            */
    struct semaphore	thdcnt;		/* Threads finished counter        */
    time_t		intrhist[MODULE_HIST_CNT];
    int			histptr;

    int			hbt_active;	/* MSC heartbeat monitor active    */
    uint64_t		hbt_last;	/* RTC when last heartbeat sent    */

    /* Module serial number info */
    union {
	char		snum_str[MAX_SERIAL_NUM_SIZE];	 /* used by CONFIG_SGI_IP27    */
	uint64_t	snum_int;			 /* used by speedo */
    } snum;
    int			snum_valid;

    int			disable_alert;
    int			count_down;

    /* System serial number info (used by SN1) */
    char		sys_snum[MAX_SERIAL_NUM_SIZE];
    int			sys_snum_valid;
};

/* module.c */
extern module_t	       *modules[MODULE_MAX];	/* Indexed by cmoduleid_t   */
extern int		nummodules;

extern module_t	       *module_lookup(moduleid_t id);

extern int		get_kmod_sys_snum(cmoduleid_t cmod,
					  char *snum);

extern void		format_module_id(char *buffer, moduleid_t m, int fmt);
extern int		parse_module_id(char *buffer);

#ifdef	__cplusplus
}
#endif

#endif /* _ASM_IA64_SN_MODULE_H */
