/*
 *  IBM/3270 Driver -- Copyright (C) 2000 UTS Global LLC
 *
 *  tubttysiz.c -- Linemode screen-size determiner
 *
 *
 *
 *
 *
 *  Author:  Richard Hitt
 */
#include "tubio.h"
static int tty3270_size_io(tub_t *tubp);
static void tty3270_size_int(tub_t *tubp, devstat_t *dsp);
static int tty3270_size_wait(tub_t *tubp, long *flags, int stat);

/*
 * Structure representing Usable Area Query Reply Base
 */
typedef struct {
	short l;                /* Length of this structured field */
	char sfid;              /* 0x81 if Query Reply */
	char qcode;             /* 0x81 if Usable Area */
#define QCODE_UA 0x81
	char flags0;
#define FLAGS0_ADDR 0x0f
#define FLAGS0_ADDR_12_14       1       /* 12/14-bit adrs ok */
#define FLAGS0_ADDR_12_14_16    3       /* 12/14/16-bit adrs ok */
	char flags1;
	short w;                /* Width of usable area */
	short h;                /* Heigth of usavle area */
	char units;             /* 0x00:in; 0x01:mm */
	int xr;
	int yr;
	char aw;
	char ah;
	short buffsz;           /* Character buffer size, bytes */
	char xmin;
	char ymin;
	char xmax;
	char ymax;
} __attribute__ ((packed)) uab_t;

/*
 * Structure representing Alternate Usable Area Self-Defining Parameter
 */
typedef struct {
	char l;                 /* Length of this Self-Defining Parm */
	char sdpid;             /* 0x02 if Alternate Usable Area */
#define SDPID_AUA 0x02
	char res;
	char auaid;             /* 0x01 is Id for the A U A */
	short wauai;            /* Width of AUAi */
	short hauai;            /* Height of AUAi */
	char auaunits;          /* 0x00:in, 0x01:mm */
	int auaxr;
	int auayr;
	char awauai;
	char ahauai;
} __attribute__ ((packed)) aua_t;

/*
 * Structure representing one followed by the other
 */
typedef struct {
	uab_t uab;
	aua_t aua;
} __attribute__ ((packed)) ua_t;

/*
 * Try to determine screen size using Read Partition (Query)
 */
int
tty3270_size(tub_t *tubp, long *flags)
{
	char wbuf[7] = { 0x00, 0x07, 0x01, 0xff, 0x03, 0x00, 0x81 };
	int     rc = 0;
	int     count;
	unsigned char *cp;
	ua_t *uap;
	char miniscreen[256];
	char (*screen)[];
	int screenl;
	int geom_rows, geom_cols, fourteenbitadr;
	void (*oldint)(struct tub_s *, devstat_t *);

	if (tubp->flags & TUB_SIZED)
		return 0;
	fourteenbitadr = 0;
	geom_rows = tubp->geom_rows;
	geom_cols = tubp->geom_cols;

	oldint = tubp->intv;
	tubp->intv = tty3270_size_int;

	if (tubp->cmd == TBC_CONOPEN) {
		tubp->ttyccw.cmd_code = TC_EWRITEA;
		cp = miniscreen;
		*cp++ = TW_KR;
		/* more? */
		tubp->ttyccw.flags = CCW_FLAG_SLI;
		tubp->ttyccw.cda = virt_to_phys(miniscreen);
		tubp->ttyccw.count = (char *)cp - miniscreen;
		rc = tty3270_size_io(tubp);
		rc = tty3270_size_wait(tubp, flags, 0);
	}

	tubp->ttyccw.cmd_code = TC_WRITESF;
	tubp->ttyccw.flags = CCW_FLAG_SLI;
	tubp->ttyccw.cda = virt_to_phys(wbuf);
	tubp->ttyccw.count = sizeof wbuf;

try_again:
	rc = tty3270_size_io(tubp);
	if (rc)
		printk("tty3270_size_io returned %d\n", rc);

	rc = tty3270_size_wait(tubp, flags, 0);
	if (rc != 0) {
		goto do_return;
	}

	/*
	 * Unit-Check Processing:
	 * Expect Command Reject or Intervention Required.
	 * For Command Reject assume old hdwe/software and
	 * set a default size of 80x24.
	 * For Intervention Required, wait for signal pending
	 * or Unsolicited Device End; if the latter, retry.
	 */
	if (tubp->dstat & DEV_STAT_UNIT_CHECK) {
		if (tubp->sense.data[0] & SNS0_CMD_REJECT) {
			goto use_diag210; /* perhaps it's tn3270 */
		} else if (tubp->sense.data[0] & SNS0_INTERVENTION_REQ) {
			if ((rc = tty3270_size_wait(tubp, flags,
			    DEV_STAT_DEV_END)))
				goto do_return;
			goto try_again;
		} else {
			printk("tty3270_size(): unkn sense %.2x\n",
				tubp->sense.data[0]);
			goto do_return;
		}
	}
	if ((rc = tty3270_size_wait(tubp, flags, DEV_STAT_ATTENTION)))
		goto do_return;

	/* Set up a read ccw and issue it */
	tubp->ttyccw.cmd_code = TC_READMOD;
	tubp->ttyccw.flags = CCW_FLAG_SLI;
	tubp->ttyccw.cda = virt_to_phys(miniscreen);
	tubp->ttyccw.count = sizeof miniscreen;
	tty3270_size_io(tubp);
	rc = tty3270_size_wait(tubp, flags, 0);
	if (rc != 0)
		goto do_return;

	count = sizeof miniscreen - tubp->cswl;
	cp = miniscreen;
	if (*cp++ != 0x88)
		goto do_return;
	uap = (void *)cp;
	if (uap->uab.qcode != QCODE_UA)
		goto do_return;
	geom_rows = uap->uab.h;
	geom_cols = uap->uab.w;
	if ((uap->uab.flags0 & FLAGS0_ADDR) == FLAGS0_ADDR_12_14 ||
	    (uap->uab.flags0 & FLAGS0_ADDR) == FLAGS0_ADDR_12_14_16)
		fourteenbitadr = 1;
	if (uap->uab.l <= sizeof uap->uab)
		goto do_return;
	if (uap->aua.sdpid != SDPID_AUA) {
		printk("AUA sdpid was 0x%.2x, expecting 0x%.2x\n",
			uap->aua.sdpid, SDPID_AUA);
		goto do_return;
	}
	geom_rows = uap->aua.hauai;
	geom_cols = uap->aua.wauai;
	goto do_return;

use_diag210:
	if (MACHINE_IS_VM) {
		diag210_t d210;

		d210.vrdcdvno = tubp->devno;
		d210.vrdclen = sizeof d210;
		rc = diag210(&d210);
		if (rc) {
			printk("tty3270_size: diag210 for 0x%.4x "
				"returned %d\n", tubp->devno, rc);
			goto do_return;
		}
		switch(d210.vrdccrmd) {
		case 2:
			geom_rows = 24;
			geom_cols = 80;
			goto do_return;
		case 3:
			geom_rows = 32;
			geom_cols = 80;
			goto do_return;
		case 4:
			geom_rows = 43;
			geom_cols = 80;
			goto do_return;
		case 5:
			geom_rows = 27;
			geom_cols = 132;
			goto do_return;
		default:
			printk("vrdccrmd is 0x%.8x\n", d210.vrdccrmd);
		}
	}

do_return:
	if (geom_rows == 0) {
		geom_rows = _GEOM_ROWS;
		geom_cols = _GEOM_COLS;
	}
	tubp->tubiocb.pf_cnt = 24;
	tubp->tubiocb.re_cnt = 20;
	tubp->tubiocb.map = 0;

	screenl = geom_rows * geom_cols + 100;
	screen = (char (*)[])kmalloc(screenl, GFP_KERNEL);
	if (screen == NULL) {
		printk("ttyscreen size %d unavailable\n", screenl);
	} else {
		if (tubp->ttyscreen)
			kfree(tubp->ttyscreen);
		tubp->tubiocb.line_cnt = tubp->geom_rows = geom_rows;
		tubp->tubiocb.col_cnt = tubp->geom_cols = geom_cols;
		tubp->tty_14bitadr = fourteenbitadr;
		tubp->ttyscreen = screen;
		tubp->ttyscreenl = screenl;
		if (geom_rows == 24 && geom_cols == 80)
			tubp->tubiocb.model = 2;
		else if (geom_rows == 32 && geom_cols == 80)
			tubp->tubiocb.model = 3;
		else if (geom_rows == 43 && geom_cols == 80)
			tubp->tubiocb.model = 4;
		else if (geom_rows == 27 && geom_cols == 132)
			tubp->tubiocb.model = 5;
		else
			tubp->tubiocb.model = 0;
		tubp->flags |= TUB_SIZED;
	}
	if (rc == 0 && tubp->ttyscreen == NULL)
		rc = -ENOMEM;
	tubp->intv = oldint;
	return rc;
}

static int
tty3270_size_io(tub_t *tubp)
{
	tubp->flags |= TUB_WORKING;
	tubp->dstat = 0;

	return do_IO(tubp->irq, &tubp->ttyccw, tubp->irq, 0, 0);
}

static void
tty3270_size_int(tub_t *tubp, devstat_t *dsp)
{
#define DEV_NOT_WORKING \
  (DEV_STAT_ATTENTION | DEV_STAT_DEV_END | DEV_STAT_UNIT_CHECK)

	tubp->dstat = dsp->dstat;
	if (dsp->dstat & DEV_STAT_CHN_END)
		tubp->cswl = dsp->rescnt;
	if (dsp->dstat & DEV_NOT_WORKING)
		tubp->flags &= ~TUB_WORKING;
	if (dsp->dstat & DEV_STAT_UNIT_CHECK)
		tubp->sense = dsp->ii.sense;

	wake_up_interruptible(&tubp->waitq);
}

/*
 * Wait for something.  If the third arg is zero, wait until
 * tty3270_size_int() turns off TUB_WORKING.  If the third arg
 * is not zero, it is a device-status bit; wait until dstat
 * has the bit turned on.  Never wait if signal is pending.
 * Return 0 unless signal pending, in which case -ERESTARTSYS.
 */
static int
tty3270_size_wait(tub_t *tubp, long *flags, int stat)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&tubp->waitq, &wait);
	while (!signal_pending(current) &&
	    (stat? (tubp->dstat & stat) == 0:
	     (tubp->flags & TUB_WORKING) != 0)) {
		current->state = TASK_INTERRUPTIBLE;
		TUBUNLOCK(tubp->irq, *flags);
		schedule();
		current->state = TASK_RUNNING;
		TUBLOCK(tubp->irq, *flags);
	}
	remove_wait_queue(&tubp->waitq, &wait);
	return signal_pending(current)? -ERESTARTSYS: 0;
}
