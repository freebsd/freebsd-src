/*
 *  IBM/3270 Driver -- Copyright (C) 2000 UTS Global LLC
 *
 *  tuball.c -- Initialization, termination, irq lookup
 *
 *
 *
 *
 *
 *  Author:  Richard Hitt
 */
#include <linux/config.h>
#include "tubio.h"
#ifndef MODULE
#include <linux/init.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0))
#include <asm/cpcmd.h>
#include <linux/bootmem.h>
#else
#include "../../../../arch/s390/kernel/cpcmd.h"
#endif
#endif

/* Module parameters */
int tubdebug;
int tubscrolltime = -1;
int tubxcorrect = 1;            /* Do correct ebc<->asc tables */
#ifdef MODULE
MODULE_PARM(tubdebug, "i");
MODULE_PARM(tubscrolltime, "i");
MODULE_PARM(tubxcorrect, "i");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,12))
MODULE_LICENSE ("GPL");
#endif
#endif
/*
 * Values for tubdebug and their effects:
 * 1 - print in hex on console the first 16 bytes received
 * 2 - print address at which array tubminors is allocated
 * 4 - attempt to register tty3270_driver
 */
int tubnummins;
tub_t *(*tubminors)[TUBMAXMINS];
tub_t *(*(*tubirqs)[256])[256];
unsigned char tub_ascebc[256];
unsigned char tub_ebcasc[256];
int tubinitminors(void);
void tubfiniminors(void);
void tubint(int, void *, struct pt_regs *);

/* Lookup-by-irq functions */
int tubaddbyirq(tub_t *, int);
tub_t *tubfindbyirq(int);
void tubdelbyirq(tub_t *, int);
void tubfiniirqs(void);

extern int fs3270_init(void);
extern void fs3270_fini(void);
extern int tty3270_init(void);
extern void tty3270_fini(void);

unsigned char tub_ebcgraf[64] =
	{ 0x40, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	  0xc8, 0xc9, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	  0x50, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	  0xd8, 0xd9, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	  0x60, 0x61, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	  0xe8, 0xe9, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	  0xf8, 0xf9, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f };

int tub3270_init(void);

#ifndef MODULE

/*
 * Can't have this driver a module & support console at the same time
 */
#ifdef CONFIG_TN3270_CONSOLE
static kdev_t tub3270_con_device(struct console *);
static void tub3270_con_unblank(void);
static void tub3270_con_write(struct console *, const char *,
	unsigned int);

static struct console tub3270_con = {
	"tub3270",		/* name */
	tub3270_con_write,	/* write */
	NULL,			/* read */
	tub3270_con_device,	/* device */
	tub3270_con_unblank,	/* unblank */
	NULL,			/* setup */
	CON_PRINTBUFFER,	/* flags */
	0,			/* index */
	0,			/* cflag */
	NULL			/* next */
};

static bcb_t tub3270_con_bcb;		/* Buffer that receives con writes */
static spinlock_t tub3270_con_bcblock;	/* Lock for the buffer */
int tub3270_con_irq = -1;		/* set nonneg by _activate() */
tub_t *tub3270_con_tubp;		/* set nonzero by _activate() */
struct tty_driver tty3270_con_driver;	/* for /dev/console at 4, 64 */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0))
int tub3270_con_devno = -1;		/* set by tub3270_con_setup() */
__initfunc(void tub3270_con_setup(char *str, int *ints))
{
	int vdev;

	vdev = simple_strtoul(str, 0, 16);
	if (vdev >= 0 && vdev < 65536)
		tub3270_con_devno = vdev;
	return;
}

__initfunc (long tub3270_con_init(long kmem_start, long kmem_end))
{
	tub3270_con_bcb.bc_len = 65536;
	if (!MACHINE_IS_VM && !MACHINE_IS_P390)
		return kmem_start;
	tub3270_con_bcb.bc_buf = (void *)kmem_start;
	kmem_start += tub3270_con_bcb.bc_len;
	register_console(&tub3270_con);
	return kmem_start;
}
#else
#define tub3270_con_devno console_device

void __init tub3270_con_init(void)
{
	tub3270_con_bcb.bc_len = 65536;
	if (!CONSOLE_IS_3270)
		return;
	tub3270_con_bcb.bc_buf = (void *)alloc_bootmem_low(
		tub3270_con_bcb.bc_len);
	register_console(&tub3270_con);
}
#endif

static kdev_t
tub3270_con_device(struct console *conp)
{
	return MKDEV(IBM_TTY3270_MAJOR, conp->index + 1);
}

static void
tub3270_con_unblank(void)
{
	/* flush everything:  panic has occurred */
}

int tub3270_con_write_deadlock_ct;
int tub3270_con_write_deadlock_bytes;
static void
tub3270_con_write(struct console *conp,
	const char *buf, unsigned int count)
{
	long flags;
	tub_t *tubp = tub3270_con_tubp;
	void tty3270_sched_bh(tub_t *);
	int rc;
	bcb_t obcb;

	obcb.bc_buf = (char *)buf;
	obcb.bc_len = obcb.bc_cnt = obcb.bc_wr =
		MIN(count, tub3270_con_bcb.bc_len);
	obcb.bc_rd = 0;

	spin_lock_irqsave(&tub3270_con_bcblock, flags);
	rc = tub3270_movedata(&obcb, &tub3270_con_bcb, 0);
	spin_unlock_irqrestore(&tub3270_con_bcblock, flags);

	if (tubp && rc && TUBTRYLOCK(tubp->irq, flags)) {
		tty3270_sched_bh(tubp);
		TUBUNLOCK(tubp->irq, flags);
	}
}
	
int tub3270_con_copy(tub_t *tubp)
{
	long flags;
	int rc;

	spin_lock_irqsave(&tub3270_con_bcblock, flags);
	rc = tub3270_movedata(&tub3270_con_bcb, &tubp->tty_bcb, 0);
	spin_unlock_irqrestore(&tub3270_con_bcblock, flags);
	return rc;
}
#endif /* CONFIG_TN3270_CONSOLE */
#else /* If generated as a MODULE */
/*
 * module init:  find tubes; get a major nbr
 */
int
init_module(void)
{
	if (tubnummins != 0) {
		printk(KERN_ERR "EEEK!!  Tube driver cobbigling!!\n");
		return -1;
	}
	return tub3270_init();
}

/*
 * remove driver:  unregister the major number
 */
void
cleanup_module(void)
{
	fs3270_fini();
	tty3270_fini();
	tubfiniminors();
}
#endif /* Not a MODULE or a MODULE */

void
tub_inc_use_count(void)
{
	MOD_INC_USE_COUNT;
}

void
tub_dec_use_count(void)
{
	MOD_DEC_USE_COUNT;
}

static int
tub3270_is_ours(s390_dev_info_t *dp)
{
	if ((dp->sid_data.cu_type & 0xfff0) == 0x3270)
		return 1;
	if (dp->sid_data.cu_type == 0x3174)
		return 1;
	return 0;
}

/*
 * tub3270_init() called by kernel or module initialization
 */
int
tub3270_init(void)
{
	s390_dev_info_t d;
	int i, rc;

	/*
	 * Copy and correct ebcdic - ascii translate tables
	 */
	memcpy(tub_ascebc, _ascebc, sizeof tub_ascebc);
	memcpy(tub_ebcasc, _ebcasc, sizeof tub_ebcasc);
	if (tubxcorrect) {
		/* correct brackets and circumflex */
		tub_ascebc['['] = 0xad;
		tub_ascebc[']'] = 0xbd;
		tub_ebcasc[0xad] = '[';
		tub_ebcasc[0xbd] = ']';
		tub_ascebc['^'] = 0xb0;
		tub_ebcasc[0x5f] = '^';
	}

	rc = tubinitminors();
	if (rc != 0)
		return rc;

	if (fs3270_init() || tty3270_init()) {
		printk(KERN_ERR "fs3270_init() or tty3270_init() failed\n");
		fs3270_fini();
		tty3270_fini();
		tubfiniminors();
		return -1;
	}

	for (i = get_irq_first(); i >= 0; i = get_irq_next(i)) {
		if ((rc = get_dev_info_by_irq(i, &d)))
			continue;
		if (d.status)
			continue;

#ifdef CONFIG_TN3270_CONSOLE
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0))
		if (d.sid_data.cu_type == 0x3215 && MACHINE_IS_VM) {
			cpcmd("TERM CONMODE 3270", NULL, 0);
			d.sid_data.cu_type = 0x3270;
		}
#else
		if (d.sid_data.cu_type == 0x3215 && CONSOLE_IS_3270) {
			cpcmd("TERM CONMODE 3270", NULL, 0);
			d.sid_data.cu_type = 0x3270;
		}
#endif /* LINUX_VERSION_CODE */
#endif /* CONFIG_TN3270_CONSOLE */
		if (!tub3270_is_ours(&d))
			continue;

		rc = tubmakemin(i, &d);
		if (rc < 0) {
			printk(KERN_WARNING 
			       "3270 tube registration ran out of memory"
			       " after %d devices\n", tubnummins - 1);
			break;
		} else {
			printk(KERN_INFO "3270: %.4x on sch %d, minor %d\n",
				d.devno, d.irq, rc);
		}
	}

	return 0;
}

/*
 * tub3270_movedata(bcb_t *, bcb_t *) -- Move data stream
 */
int
tub3270_movedata(bcb_t *ib, bcb_t *ob, int fromuser)
{
	int count;			/* Total move length */
	int rc;

	rc = count = MIN(ib->bc_cnt, ob->bc_len - ob->bc_cnt);
	while (count > 0) {
		int len1;		/* Contig bytes avail in ib */

		if (ib->bc_wr > ib->bc_rd)
			len1 = ib->bc_wr - ib->bc_rd;
		else
			len1 = ib->bc_len - ib->bc_rd;
		if (len1 > count)
			len1 = count;

		while (len1 > 0) {
			int len2;	/* Contig space avail in ob */

			if (ob->bc_rd > ob->bc_wr)
				len2 = ob->bc_rd - ob->bc_wr;
			else
				len2 = ob->bc_len - ob->bc_wr;
			if (len2 > len1)
				len2 = len1;
			
			if (fromuser) {
				len2 -= copy_from_user(ob->bc_buf + ob->bc_wr,
						       ib->bc_buf + ib->bc_rd,
						       len2);
				if (len2 == 0) {
					if (!rc)
						rc = -EFAULT;
					break;
				}
			} else
				memcpy(ob->bc_buf + ob->bc_wr,
				       ib->bc_buf + ib->bc_rd,
				       len2);
			
			ib->bc_rd += len2;
			if (ib->bc_rd == ib->bc_len)
				ib->bc_rd = 0;
			ib->bc_cnt -= len2;

			ob->bc_wr += len2;
			if (ob->bc_wr == ob->bc_len)
				ob->bc_wr = 0;
			ob->bc_cnt += len2;

			len1 -= len2;
			count -= len2;
		}
	}
	return rc;
}

/*
 * receive an interrupt
 */
void
tubint(int irq, void *ipp, struct pt_regs *prp)
{
	devstat_t *dsp = ipp;
	tub_t *tubp;

	if ((tubp = IRQ2TUB(irq)) && (tubp->intv))
		(tubp->intv)(tubp, dsp);
}

/*
 * Initialize array of pointers to minor structures tub_t.
 * Returns 0 or -ENOMEM.
 */
int
tubinitminors(void)
{
	tubminors = (tub_t *(*)[TUBMAXMINS])kmalloc(sizeof *tubminors,
		GFP_KERNEL);
	if (tubminors == NULL)
		return -ENOMEM;
	memset(tubminors, 0, sizeof *tubminors);
	return 0;
}

/*
 * Add a minor 327x device.  Argument is an irq value.
 *
 * Point elements of two arrays to the newly created tub_t:
 * 1. (*tubminors)[minor]
 * 2. (*(*tubirqs)[irqhi])[irqlo]
 * The first looks up from minor number at context time; the second
 * looks up from irq at interrupt time.
 */
int
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0))
tubmakemin(int irq, dev_info_t *dp)
#else
tubmakemin(int irq, s390_dev_info_t *dp)
#endif
{
	tub_t *tubp;
	int minor;
	long flags;

	if ((minor = ++tubnummins) == TUBMAXMINS)
		return -ENODEV;

	tubp = kmalloc(sizeof(tub_t), GFP_KERNEL);
	if (tubp == NULL) {
		return -ENOMEM;
	}
	if (tubaddbyirq(tubp, irq) != 0) {
		kfree(tubp);
		return -ENOMEM;
	}
	memset(tubp, 0, sizeof(tub_t));
	tubp->minor = minor;
	tubp->irq = irq;
	TUBLOCK(tubp->irq, flags);
	tubp->devno = dp->devno;
	tubp->geom_rows = _GEOM_ROWS;
	tubp->geom_cols = _GEOM_COLS;
	init_waitqueue_head(&tubp->waitq);

	tubp->tty_bcb.bc_len = TTY_OUTPUT_SIZE;
	tubp->tty_bcb.bc_buf = (void *)kmalloc(tubp->tty_bcb.bc_len,
		GFP_KERNEL|GFP_DMA);
	if (tubp->tty_bcb.bc_buf == NULL) {
		TUBUNLOCK(tubp->irq, flags);
		tubdelbyirq(tubp, irq);
		kfree(tubp);
		return -ENOMEM;
	}
	tubp->tty_bcb.bc_cnt = 0;
	tubp->tty_bcb.bc_wr = 0;
	tubp->tty_bcb.bc_rd = 0;
	(*tubminors)[minor] = tubp;

#ifdef CONFIG_TN3270_CONSOLE
	if (CONSOLE_IS_3270) {
		if (tub3270_con_tubp == NULL && 
		    tub3270_con_bcb.bc_buf != NULL &&
		    (tub3270_con_devno == -1 ||
		     tub3270_con_devno == dp->devno)) {
			extern void tty3270_int(tub_t *, devstat_t *);
			
			tub3270_con_devno = dp->devno;
			tubp->cmd = TBC_CONOPEN;
			tubp->flags |= TUB_OPEN_STET | TUB_INPUT_HACK;
			tty3270_size(tubp, &flags);
			tubp->tty_input = kmalloc(GEOM_INPLEN,
				GFP_KERNEL|GFP_DMA);
			tty3270_aid_init(tubp);
			tty3270_scl_init(tubp);
			tub3270_con_irq = tubp->irq;
			tub3270_con_tubp = tubp;
			tubp->intv = tty3270_int;
			tubp->cmd = TBC_UPDSTAT;
			tty3270_build(tubp);
		}
	}
#endif /* CONFIG_TN3270_CONSOLE */

#ifdef CONFIG_DEVFS_FS
	fs3270_devfs_register(tubp);
#endif

	TUBUNLOCK(tubp->irq, flags);
	return minor;
}

/*
 * Release array of pointers to minor structures tub_t, but first
 * release any storage pointed to by them.
 */
void
tubfiniminors(void)
{
	int i;
	tub_t **tubpp, *tubp;

	if (tubminors == NULL)
		return;

	for (i = 0; i < TUBMAXMINS; i++) {
		tubpp = &(*tubminors)[i];
		if ((tubp = *tubpp)) {
#ifdef CONFIG_DEVFS_FS
			fs3270_devfs_unregister(tubp);
#endif
			tubdelbyirq(tubp, tubp->irq);
			tty3270_rcl_fini(tubp);
			kfree(tubp->tty_bcb.bc_buf);
			if (tubp->tty_input) {
				kfree(tubp->tty_input);
				tubp->tty_input = NULL;
			}
			tubp->tty_bcb.bc_buf = NULL;
			tubp->ttyscreen = NULL;
			kfree(tubp);
			*tubpp = NULL;
		}
	}
	kfree(tubminors);
	tubminors = NULL;
	tubfiniirqs();
}

/*
 * tubaddbyirq() -- Add tub_t for irq lookup in tubint()
 */
int
tubaddbyirq(tub_t *tubp, int irq)
{
	int irqhi = (irq >> 8) & 255;
	int irqlo = irq & 255;
	tub_t *(*itubpp)[256];

	/* Allocate array (*tubirqs)[] if first time */
	if (tubirqs == NULL) {
		tubirqs = (tub_t *(*(*)[256])[256])
			kmalloc(sizeof *tubirqs, GFP_KERNEL);
		if (tubirqs == NULL)
			return -ENOMEM;
		memset(tubirqs, 0, sizeof *tubirqs);
	}

	/* Allocate subarray (*(*tubirqs)[])[] if first use */
	if ((itubpp = (*tubirqs)[irqhi]) == NULL) {
		itubpp = (tub_t *(*)[256])
			kmalloc(sizeof(*itubpp), GFP_KERNEL);
		if (itubpp == NULL) {
			if (tubnummins == 1) {  /* if first time */
				kfree(tubirqs);
				tubirqs = NULL;
			}
			return -ENOMEM;
		} else {
			memset(itubpp, 0, sizeof(*itubpp));
			(*tubirqs)[irqhi] = itubpp;
		}
	}

	/* Request interrupt service */
	if ((tubp->irqrc = request_irq(irq, tubint, SA_INTERRUPT,
	    "3270 tube driver", &tubp->devstat)) != 0)
		return tubp->irqrc;

	/* Fill in the proper subarray element */
	(*itubpp)[irqlo] = tubp;
	return 0;
}

/*
 * tubfindbyirq(irq)
 */
tub_t *
tubfindbyirq(int irq)
{
	int irqhi = (irq >> 8) & 255;
	int irqlo = irq & 255;
	tub_t *tubp;

	if (tubirqs == NULL)
		return NULL;
	if ((*tubirqs)[irqhi] == NULL)
		return NULL;
	tubp = (*(*tubirqs)[irqhi])[irqlo];
	if (tubp->irq == irq)
		return tubp;
	return NULL;
}

/*
 * tubdelbyirq(tub_t*, irq)
 */
void
tubdelbyirq(tub_t *tubp, int irq)
{
	int irqhi = (irq >> 8) & 255;
	int irqlo = irq & 255;
	tub_t *(*itubpp)[256], *itubp;

	if (tubirqs == NULL) {
		printk(KERN_ERR "tubirqs is NULL\n");
		return;
	}
	itubpp = (*tubirqs)[irqhi];
	if (itubpp == NULL) {
		printk(KERN_ERR "tubirqs[%d] is NULL\n", irqhi);
		return;
	}
	itubp = (*itubpp)[irqlo];
	if (itubp == NULL) {
		printk(KERN_ERR "tubirqs[%d][%d] is NULL\n", irqhi, irqlo);
		return;
	}
	if (itubp->irqrc == 0)
		free_irq(irq, &itubp->devstat);
	(*itubpp)[irqlo] = NULL;
}

/*
 * tubfiniirqs() -- clean up storage in tub_t *(*(*tubirqs)[256])[256]
 */
void
tubfiniirqs(void)
{
	int i;
	tub_t *(*itubpp)[256];

	if (tubirqs != NULL) {
		for (i = 0; i < 256; i++) {
			if ((itubpp = (*tubirqs)[i])) {
				kfree(itubpp);
				(*tubirqs)[i] = NULL;
			}
		}
		kfree(tubirqs);
		tubirqs = NULL;
	}
}
