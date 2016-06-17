/*
 *  IBM/3270 Driver -- Copyright (C) 2000 UTS Global LLC
 *
 *  tubio.h -- All-Purpose header file
 *
 *
 *
 *
 *
 *  Author:  Richard Hitt
 */
#include <linux/config.h>

#include <linux/module.h>
#include <linux/version.h>

#include <linux/major.h>
#ifndef IBM_TTY3270_MAJOR
#  define IBM_TTY3270_MAJOR 212
#endif /* IBM_TTY3270_MAJOR */
#ifndef IBM_FS3270_MAJOR
#  define IBM_FS3270_MAJOR 213
#endif /* IBM_FS3270_MAJOR */


#include <linux/slab.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/idals.h>
#include <linux/console.h>
#include <linux/interrupt.h>
#include <asm/ebcdic.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0))
#include <linux/devfs_fs_kernel.h>
#endif

#define TUB(x) (('3'<<8)|(x))
#define TUBICMD TUB(3)
#define TUBOCMD TUB(4)
#define TUBGETI TUB(7)
#define TUBGETO TUB(8)
#define TUBSETMOD TUB(12)
#define TUBGETMOD TUB(13)
#define TIOPOLL TUB(32)
#define TIOPOKE TUB(33)
#define TIONPOKE TUB(34)
#define TIOTNORM TUB(35)

/* Local Channel Commands */
#define TC_WRITE   0x01
#define TC_EWRITE  0x05
#define TC_READMOD 0x06
#define TC_EWRITEA 0x0d
#define TC_WRITESF 0x11

/* Buffer Control Orders */
#define TO_SF 0x1d
#define TO_SBA 0x11
#define TO_IC 0x13
#define TO_PT 0x05
#define TO_RA 0x3c
#define TO_SFE 0x29
#define TO_EUA 0x12
#define TO_MF 0x2c
#define TO_SA 0x28

/* Field Attribute Bytes */
#define TF_INPUT 0x40           /* Visible input */
#define TF_INPUTN 0x4c          /* Invisible input */
#define TF_INMDT 0xc1           /* Visible, Set-MDT */
#define TF_LOG 0x60
#define TF_STAT 0x60

/* Character Attribute Bytes */
#define TAT_RESET 0x00
#define TAT_FIELD 0xc0
#define TAT_EXTHI 0x41
#define TAT_COLOR 0x42
#define TAT_CHARS 0x43
#define TAT_TRANS 0x46

/* Extended-Highlighting Bytes */
#define TAX_RESET 0x00
#define TAX_BLINK 0xf1
#define TAX_REVER 0xf2
#define TAX_UNDER 0xf4

/* Reset value */
#define TAR_RESET 0x00

/* Color values */
#define TAC_RESET 0x00
#define TAC_BLUE 0xf1
#define TAC_RED 0xf2
#define TAC_PINK 0xf3
#define TAC_GREEN 0xf4
#define TAC_TURQ 0xf5
#define TAC_YELLOW 0xf6
#define TAC_WHITE 0xf7
#define TAC_DEFAULT 0x00

/* Write Control Characters */
#define TW_NONE 0x40            /* No particular action */
#define TW_KR 0xc2              /* Keyboard restore */
#define TW_PLUSALARM 0x04       /* Add this bit for alarm */

/* Attention-ID (AID) Characters */
#define TA_CLEAR 0x6d
#define TA_PA2 0x6e
#define TA_ENTER 0x7d
/* more to come */

#define MIN(a, b) ((a) < (b)? (a): (b))

#define TUB_BUFADR(adr, cpp) \
	tty3270_tub_bufadr(tubp, adr, cpp)

#define TUB_EBCASC(addr, nr) codepage_convert(tub_ebcasc, addr, nr)
#define TUB_ASCEBC(addr, nr) codepage_convert(tub_ascebc, addr, nr)

/*
 *
 * General global values for the tube driver
 *
 */
enum tubmode {
	TBM_LN,                 /* Line mode */
	TBM_FS,                 /* Fullscreen mode */
	TBM_FSLN                /* Line mode shelled out of fullscreen */
};
enum tubstat {              /* normal-mode status */
	TBS_RUNNING,            /* none of the following */
	TBS_MORE,               /* timed "MORE..." in status */
	TBS_HOLD                /* untimed "HOLDING" in status */
};
enum tubcmd {           /* normal-mode actions to do */
	TBC_CONOPEN,		/* Erase-write the console */
	TBC_OPEN,               /* Open the tty screen */
	TBC_UPDATE,             /* Add lines to the log, clear cmdline */
	TBC_UPDLOG,             /* Add lines to log */
	TBC_KRUPDLOG,           /* Add lines to log, reset kbd */
	TBC_CLEAR,              /* Build screen from scratch */
	TBC_CLRUPDLOG,          /* Do log & status, not cmdline */
	TBC_UPDSTAT,            /* Do status update only */
	TBC_CLRINPUT,           /* Clear input area only */
	TBC_UPDINPUT            /* Update input area only */
};
enum tubwhat {          /* echo what= proc actions */
	TW_BOGUS,               /* Nothing at all */
	TW_CONFIG               /* Output configuration info */
};





#define TUBMAXMINS      256
#define TUB_DEV MKDEV(IBM_FS3270_MAJ, 0)        /* Generic /dev/3270/tub */
#define _GEOM_ROWS 24
#define _GEOM_COLS 80
#define GEOM_ROWS (tubp->geom_rows)
#define GEOM_COLS (tubp->geom_cols)
#define GEOM_MAXROWS 127
#define GEOM_MAXCOLS 132
#define GEOM_INPLEN (GEOM_COLS * 2 - 20)
#define GEOM_MAXINPLEN (GEOM_MAXCOLS * 2 - 20)
#define GEOM_INPUT (GEOM_COLS * (GEOM_ROWS - 2) - 1)  /* input atr posn */
#define GEOM_STAT (GEOM_INPUT + 1 + GEOM_INPLEN)
#define GEOM_LOG   (GEOM_COLS * GEOM_ROWS - 1)   /* log atr posn */
#define TS_RUNNING "Linux Running     "
#define TS_MORE    "Linux More...     "
#define DEFAULT_SCROLLTIME 5
#define TS_HOLD    "Linux Holding     "
/* data length used by tty3270_set_status_area: SBA (3), SF (2), data */
#define TS_LENGTH (sizeof TS_RUNNING + 3 + 2)

typedef struct {
	int aid;                        /* What-to-do flags */
	char *string;                   /* Optional input string */
} aid_t;
#define AIDENTRY(ch, tubp)  (&((tubp)->tty_aid[(ch) & 0x3f]))

/* For TUBGETMOD and TUBSETMOD.  Should include. */
typedef struct tubiocb {
	short model;
	short line_cnt;
	short col_cnt;
	short pf_cnt;
	short re_cnt;
	short map;
} tubiocb_t;

/* Flags that go in int aid, above */
#define TA_CLEARKEY     0x01            /* Key does hardware CLEAR */
#define TA_SHORTREAD    0x02            /* Key does hardware shortread */
/* If both are off, key does hardware Read Modified. */
#define TA_DOENTER      0x04            /* Treat key like ENTER */
#define TA_DOSTRING     0x08            /* Use string and ENTER */
#define TA_DOSTRINGD    0x10            /* Display string & set MDT */
#define TA_CLEARLOG     0x20            /* Make key cause clear of log */

/*
 * Tube driver buffer control block
 */
typedef struct bcb_s {
	char	*bc_buf;		/* Pointer to buffer */
	int	bc_len;			/* Length of buffer */
	int	bc_cnt;			/* Count of bytes buffered */
	int	bc_wr;			/* Posn to write next byte into */
	int	bc_rd;			/* Posn to read next byte from */
} bcb_t;

typedef struct tub_s {
	int             minor;
	int             irq;
	int             irqrc;
	int             devno;
	int             geom_rows;
	int             geom_cols;
	tubiocb_t       tubiocb;
	int             lnopen;
	int             fsopen;
	int             icmd;
	int             ocmd;
	devstat_t       devstat;
	ccw1_t          rccw;
	ccw1_t          wccw;
	struct idal_buffer *wbuf;
	int             cswl;
	void            (*intv)(struct tub_s *, devstat_t *);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0))
	struct wait_queue	*waitq;
#else
	wait_queue_head_t waitq;
#endif
	int             dstat;
	sense_t         sense;
	enum tubmode    mode;
	enum tubstat    stat;
	enum tubcmd     cmd;
	int             flags;		/* See below for values */
	struct tq_struct tqueue;

	/* Stuff for fs-driver support */
	pid_t           fs_pid;         /* Pid if TBM_FS */


	/* Stuff for tty-driver support */
	struct tty_struct *tty;
	char *tty_input;		/* tty input area */
	int tty_inattr;         	/* input-area field attribute */
#define TTY_OUTPUT_SIZE 1024
	bcb_t tty_bcb;			/* Output buffer control info */
	int tty_oucol;                  /* Kludge */
	int tty_nextlogx;               /* next screen-log position */
	int tty_savecursor;		/* saved cursor position */
	int tty_scrolltime;             /* scrollforward wait time, sec */
	struct timer_list tty_stimer;   /* timer for scrolltime */
	aid_t tty_aid[64];              /* Aid descriptors */
	int tty_aidinit;                /* Boolean */
	int tty_showaidx;               /* Last aid x to set_aid */
	int tty_14bitadr;               /* 14-bit bufadrs okay */
#define MAX_TTY_ESCA 24			/* Set-Attribute-Order array */
	char tty_esca[MAX_TTY_ESCA];	/* SA array */
	int tty_escx;			/* Current index within it */

	/* For command recall --- */
	char *(*tty_rclbufs)[];         /* Array of ptrs to recall bufs */
	int tty_rclk;                   /* Size of array tty_rclbufs */
	int tty_rclp;                   /* Index for most-recent cmd */
	int tty_rclb;                   /* Index for backscrolling */

	/* Work area to contain the hardware write stream */
	char (*ttyscreen)[];            /* ptr to data stream area */
	int ttyscreenl;			/* its length */
	ccw1_t ttyccw;
} tub_t;

/* values for flags: */
#define	TUB_WORKING	0x0001
#define	TUB_BHPENDING	0x0002
#define	TUB_RDPENDING	0x0004
#define	TUB_ALARM	0x0008
#define	TUB_SCROLLTIMING  0x0010
#define	TUB_ATTN	0x0020
#define	TUB_IACTIVE	0x0040
#define	TUB_SIZED	0x0080
#define	TUB_EXPECT_DE	0x0100
#define	TUB_UNSOL_DE	0x0200
#define	TUB_OPEN_STET	0x0400		/* No screen clear on open */
#define	TUB_UE_BUSY	0x0800
#define	TUB_INPUT_HACK	0x1000		/* Early init of command line */

#ifdef CONFIG_TN3270_CONSOLE
/*
 * Extra stuff for 3270 console support
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0))
#define	S390_CONSOLE_DEV MKDEV(TTY_MAJOR, 64)
#else
#define	S390_CONSOLE_DEV MKDEV(TTYAUX_MAJOR, 1)
#endif
extern int tub3270_con_devno;
extern char (*tub3270_con_output)[];
extern int tub3270_con_outputl;
extern int tub3270_con_ouwr;
extern int tub3270_con_oucount;
extern int tub3270_con_irq;
extern tub_t *tub3270_con_tubp;
extern struct tty_driver tty3270_con_driver;
#endif /* CONFIG_TN3270_CONSOLE */

extern int tubnummins;
extern tub_t *(*tubminors)[TUBMAXMINS];
extern tub_t *(*(*tubirqs)[256])[256];
extern unsigned char tub_ascebc[256];
extern unsigned char tub_ebcasc[256];
extern unsigned char tub_ebcgraf[64];
extern int tubdebug;
extern int fs3270_major;
extern int tty3270_major;
extern int tty3270_proc_misc;
extern enum tubwhat tty3270_proc_what;
extern struct tty_driver tty3270_driver;
#ifdef CONFIG_DEVFS_FS
extern devfs_handle_t fs3270_devfs_dir;
extern void fs3270_devfs_register(tub_t *);
extern void fs3270_devfs_unregister(tub_t *);
#endif

#ifndef spin_trylock_irqsave
#define spin_trylock_irqsave(lock, flags) \
({ \
	int success; \
	__save_flags(flags); \
	__cli(); \
	success = spin_trylock(lock); \
	if (success == 0) \
		__restore_flags(flags); \
	success; \
})
#endif /* if not spin_trylock_irqsave */

#ifndef s390irq_spin_trylock_irqsave
#define s390irq_spin_trylock_irqsave(irq, flags) \
	spin_trylock_irqsave(&(ioinfo[irq]->irq_lock), flags)
#endif /* if not s390irq_spin_trylock_irqsave */

#define TUBLOCK(irq, flags) \
	s390irq_spin_lock_irqsave(irq, flags)

#define TUBTRYLOCK(irq, flags) \
	s390irq_spin_trylock_irqsave(irq, flags)

#define TUBUNLOCK(irq, flags) \
	s390irq_spin_unlock_irqrestore(irq, flags)

/*
 * Find tub_t * given fullscreen device's irq (subchannel number)
 */
extern tub_t *tubfindbyirq(int);
#define IRQ2TUB(irq) tubfindbyirq(irq)
/*
 * Find tub_t * given fullscreen device's inode pointer
 * This algorithm takes into account /dev/3270/tub.
 */
static inline tub_t *INODE2TUB(struct inode *ip)
{
	unsigned int minor = MINOR(ip->i_rdev);
	tub_t *tubp = NULL;
	if (minor == 0 && current->tty != NULL) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0))
#ifdef CONFIG_TN3270_CONSOLE
		if (tub3270_con_tubp != NULL &&
		    current->tty->device == S390_CONSOLE_DEV)
			minor = tub3270_con_tubp->minor;
		else
#endif
#endif
		if (MAJOR(current->tty->device) == IBM_TTY3270_MAJOR)
			minor = MINOR(current->tty->device);
	}
	if (minor <= tubnummins && minor > 0)
		tubp = (*tubminors)[minor];
	return tubp;
}

/*
 * Find tub_t * given non-fullscreen (tty) device's tty_struct pointer
 */
static inline tub_t *TTY2TUB(struct tty_struct *tty)
{
	unsigned int minor = MINOR(tty->device);
	tub_t *tubp = NULL;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0))
#ifdef CONFIG_TN3270_CONSOLE
	if (tty->device == S390_CONSOLE_DEV)
		tubp = tub3270_con_tubp;
	else
#endif
#endif
	if (minor <= tubnummins && minor > 0)
		tubp = (*tubminors)[minor];
	return tubp;
}

extern void tub_inc_use_count(void);
extern void tub_dec_use_count(void);
extern int tub3270_movedata(bcb_t *, bcb_t *, int);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0))
extern int tubmakemin(int, dev_info_t *);
#else
extern int tubmakemin(int, s390_dev_info_t *);
#endif
extern int tub3270_con_copy(tub_t *);
extern int tty3270_rcl_init(tub_t *);
extern int tty3270_rcl_set(tub_t *, char *, int);
extern void tty3270_rcl_fini(tub_t *);
extern int tty3270_rcl_get(tub_t *, char *, int, int);
extern void tty3270_rcl_put(tub_t *, char *, int);
extern void tty3270_rcl_sync(tub_t *);
extern void tty3270_rcl_purge(tub_t *);
extern int tty3270_rcl_resize(tub_t *, int);
extern int tty3270_size(tub_t *, long *);
extern int tty3270_aid_init(tub_t *);
extern void tty3270_aid_fini(tub_t *);
extern void tty3270_aid_reinit(tub_t *);
extern int tty3270_aid_get(tub_t *, int, int *, char **);
extern int tty3270_aid_set(tub_t *, char *, int);
extern int tty3270_build(tub_t *);
extern void tty3270_scl_settimer(tub_t *);
extern void tty3270_scl_resettimer(tub_t *);
extern int tty3270_scl_set(tub_t *, char *, int);
extern int tty3270_scl_init(tub_t *tubp);
extern void tty3270_scl_fini(tub_t *tubp);
