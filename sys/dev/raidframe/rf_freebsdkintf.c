/*-
 * Copyright (c) 2002 Scott Long <scottl@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*	$NetBSD: rf_netbsdkintf.c,v 1.105 2001/04/05 02:48:51 oster Exp $	*/
/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Greg Oster; Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: cd.c 1.6 90/11/28$
 *
 *      @(#)cd.c        8.2 (Berkeley) 11/16/93
 */




/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Mark Holland, Jim Zelenka
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/***********************************************************
 *
 * rf_kintf.c -- the kernel interface routines for RAIDframe
 *
 ***********************************************************/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/ioccom.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/disk.h>
#include <sys/diskslice.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/reboot.h>
#include <sys/module.h>
#include <sys/devicestat.h>
#include <vm/uma.h>

#include "opt_raid.h"
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_raidframe.h>
#include <dev/raidframe/rf_copyback.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagflags.h>
#include <dev/raidframe/rf_desc.h>
#include <dev/raidframe/rf_diskqueue.h>
#include <dev/raidframe/rf_acctrace.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_debugMem.h>
#include <dev/raidframe/rf_kintf.h>
#include <dev/raidframe/rf_options.h>
#include <dev/raidframe/rf_driver.h>
#include <dev/raidframe/rf_parityscan.h>
#include <dev/raidframe/rf_debugprint.h>
#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_configure.h>

RF_DECLARE_STATIC_MUTEX(rf_sparet_wait_mutex)

static RF_SparetWait_t *rf_sparet_wait_queue;	/* requests to install a
						 * spare table */
static RF_SparetWait_t *rf_sparet_resp_queue;	/* responses from
						 * installation process */

/* prototypes */
static void KernelWakeupFunc(struct bio *);
static void InitBP(struct bio *, struct vnode *, unsigned rw_flag, 
		   dev_t dev, RF_SectorNum_t startSect, 
		   RF_SectorCount_t numSect, caddr_t buf,
		   void (*cbFunc) (struct bio *), void *cbArg, 
		   int logBytesPerSector, struct proc * b_proc);
static dev_t raidinit(RF_Raid_t *);
static void rf_search_label(dev_t, struct disklabel *,
			    RF_AutoConfig_t **) __unused;

static int	raid_modevent(module_t, int, void*);
void		raidattach(void);
d_psize_t	raidsize;
d_open_t	raidopen;
d_close_t	raidclose;
d_ioctl_t	raidioctl;
d_write_t	raidwrite;
d_read_t	raidread;
d_strategy_t	raidstrategy;
#if 0
d_dump_t	raiddump;
#endif

d_open_t	raidctlopen;
d_close_t	raidctlclose;
d_ioctl_t	raidctlioctl;

static struct cdevsw raid_cdevsw = {
	raidopen,
	raidclose,
	raidread,
	raidwrite,
	raidioctl,
	nopoll,
	nommap,
	raidstrategy,
	"raid",
	200,
	nodump,
	nopsize,
	D_DISK,
};

static struct cdevsw raidctl_cdevsw = {
	raidctlopen,
	raidctlclose,
	noread,
	nowrite,
	raidctlioctl,
	nopoll,
	nommap,
	nostrategy,
	"raidctl",
	201,
	nodump,
	nopsize,
	0,
};

static struct cdevsw raiddisk_cdevsw;

/*
 * Pilfered from ccd.c
 */

struct raidbuf {
	struct bio rf_buf;	/* new I/O buf.  MUST BE FIRST!!! */
	struct bio *rf_obp;	/* ptr. to original I/O buf */
	int     rf_flags;	/* misc. flags */
	RF_DiskQueueData_t *req;/* the request that this was part of.. */
};


#define RAIDGETBUF(sc) uma_zalloc((sc)->sc_cbufpool, M_NOWAIT)
#define	RAIDPUTBUF(sc, cbp) uma_zfree((sc)->sc_cbufpool, cbp)

#define RF_MAX_ARRAYS	32

/* Raid control device */
struct raidctl_softc {
	dev_t	sc_dev;		/* Device node */
	int	sc_flags;	/* flags */
	int	sc_numraid;	/* Number of configured raid devices */
	dev_t	sc_raiddevs[RF_MAX_ARRAYS];
};

struct raid_softc {
	dev_t	sc_dev;		/* Our device */
	dev_t	sc_parent_dev;
	int     sc_flags;	/* flags */
	int	sc_busycount;	/* How many times are we opened? */
	size_t  sc_size;	/* size of the raid device */
	dev_t	sc_parent;	/* Parent device */
	struct disk		sc_dkdev;	/* generic disk device info */
 	uma_zone_t		sc_cbufpool;	/* component buffer pool */
	RF_Raid_t		*raidPtr;	/* Raid information struct */
	struct bio_queue_head	bio_queue;	/* used for the device queue */
	struct devstat		device_stats;	/* devstat gathering */
};
/* sc_flags */
#define RAIDF_OPEN	0x01	/* unit has been initialized */
#define RAIDF_WLABEL	0x02	/* label area is writable */
#define RAIDF_LABELLING	0x04	/* unit is currently being labelled */
#define RAIDF_WANTED	0x40	/* someone is waiting to obtain a lock */
#define RAIDF_LOCKED	0x80	/* unit is locked */

/* 
 * Allow RAIDOUTSTANDING number of simultaneous IO's to this RAID device. 
 * Be aware that large numbers can allow the driver to consume a lot of 
 * kernel memory, especially on writes, and in degraded mode reads.
 * 
 * For example: with a stripe width of 64 blocks (32k) and 5 disks, 
 * a single 64K write will typically require 64K for the old data, 
 * 64K for the old parity, and 64K for the new parity, for a total 
 * of 192K (if the parity buffer is not re-used immediately).
 * Even it if is used immedately, that's still 128K, which when multiplied
 * by say 10 requests, is 1280K, *on top* of the 640K of incoming data.
 * 
 * Now in degraded mode, for example, a 64K read on the above setup may
 * require data reconstruction, which will require *all* of the 4 remaining 
 * disks to participate -- 4 * 32K/disk == 128K again.
 */

#ifndef RAIDOUTSTANDING
#define RAIDOUTSTANDING   10
#endif

#define RAIDLABELDEV(dev)	dkmodpart(dev, RAW_PART)
#define DISKPART(dev)	dkpart(dev)

static void raidgetdefaultlabel(RF_Raid_t *, struct raid_softc *, struct disk*);
static int raidlock(struct raid_softc *);
static void raidunlock(struct raid_softc *);

static void rf_markalldirty(RF_Raid_t *);

static dev_t raidctl_dev;

void rf_ReconThread(struct rf_recon_req *);
/* XXX what I want is: */
/*void rf_ReconThread(RF_Raid_t *raidPtr);  */
void rf_RewriteParityThread(RF_Raid_t *raidPtr);
void rf_CopybackThread(RF_Raid_t *raidPtr);
void rf_ReconstructInPlaceThread(struct rf_recon_req *);
void rf_buildroothack(void *, struct raidctl_softc *);

RF_AutoConfig_t *rf_find_raid_components(void);
RF_ConfigSet_t *rf_create_auto_sets(RF_AutoConfig_t *);
static int rf_does_it_fit(RF_ConfigSet_t *,RF_AutoConfig_t *);
static int rf_reasonable_label(RF_ComponentLabel_t *);
void rf_create_configuration(RF_AutoConfig_t *,RF_Config_t *, RF_Raid_t *);
int rf_set_autoconfig(RF_Raid_t *, int);
int rf_set_rootpartition(RF_Raid_t *, int);
void rf_release_all_vps(RF_ConfigSet_t *);
void rf_cleanup_config_set(RF_ConfigSet_t *);
int rf_have_enough_components(RF_ConfigSet_t *);
int rf_auto_config_set(RF_ConfigSet_t *, int *, struct raidctl_softc *);
static int raidgetunit(struct raidctl_softc *, int);
static int raidshutdown(void);

void
raidattach(void)
{
	struct raidctl_softc *parent_sc = NULL;
	RF_AutoConfig_t *ac_list; /* autoconfig list */
	RF_ConfigSet_t *config_sets;
	int autoconfig = 0;

	/* This is where all the initialization stuff gets done. */

	if(rf_mutex_init(&rf_sparet_wait_mutex, __FUNCTION__)) {
		rf_printf(0, "RAIDframe: failed to initialize mutexes\n");
		return;
	}

	rf_sparet_wait_queue = rf_sparet_resp_queue = NULL;

	if (rf_BootRaidframe() != 0) {
		rf_printf(0, "Serious error booting RAIDframe!!\n");
		return;
	}

	rf_printf(0, "Kernelized RAIDframe activated\n");
	MALLOC(parent_sc, struct raidctl_softc *, sizeof(*parent_sc),
	    M_RAIDFRAME, M_NOWAIT|M_ZERO);
	if (parent_sc == NULL) {
		RF_PANIC();
		return;
	}

	parent_sc->sc_dev= make_dev(&raidctl_cdevsw, 0, UID_ROOT, GID_WHEEL,
	    0600, "raidctl");
	parent_sc->sc_dev->si_drv1 = parent_sc;
	raidctl_dev = parent_sc->sc_dev;

#if RAID_AUTOCONFIG
	autoconfig = 1;
#endif

	if (autoconfig) {
		/* 1. locate all RAID components on the system */

		rf_printf(0, "Searching for raid components...\n");
		ac_list = rf_find_raid_components();
		if (ac_list == NULL)
			return;

		/* 2. sort them into their respective sets */

		config_sets = rf_create_auto_sets(ac_list);

		/* 3. evaluate each set and configure the valid ones
		   This gets done in rf_buildroothack() */

		/* schedule the creation of the thread to do the 
		   "/ on RAID" stuff */

		rf_buildroothack(config_sets, parent_sc);
#if 0
		kthread_create(rf_buildroothack,config_sets);

#endif /* RAID_AUTOCONFIG */
	}
}

void
rf_buildroothack(arg, parent_sc)
	void *arg;
	struct raidctl_softc *parent_sc;
{
	RF_ConfigSet_t *config_sets = arg;
	RF_ConfigSet_t *cset;
	RF_ConfigSet_t *next_cset;
	int retcode;
	int raidID;
	int rootID;
	int num_root;

	rootID = 0;
	num_root = 0;
	cset = config_sets;
	while(cset != NULL ) {
		next_cset = cset->next;
		if (rf_have_enough_components(cset) && 
		    cset->ac->clabel->autoconfigure==1) {
			retcode = rf_auto_config_set(cset, &raidID, parent_sc);
			if (!retcode) {
				if (cset->rootable) {
					rootID = raidID;
					num_root++;
				}
			} else {
				/* The autoconfig didn't work :( */
				rf_printf(1, "Autoconfig failed with code %d"
				    "for raid%d\n", retcode, raidID);
				rf_release_all_vps(cset);
			}
		} else {
			/* we're not autoconfiguring this set...  
			   release the associated resources */
			rf_release_all_vps(cset);
		}
		/* cleanup */
		rf_cleanup_config_set(cset);
		cset = next_cset;
	}
	if (boothowto & RB_ASKNAME) {
		/* We don't auto-config... */
	} else {
		/* They didn't ask, and we found something bootable... */

#if 0
		if (num_root == 1) {
			booted_device = &raidrootdev[rootID]; 
		} else if (num_root > 1) {
			/* we can't guess.. require the user to answer... */
			boothowto |= RB_ASKNAME;
		}
#endif
	}
}

int
raidctlopen(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct raidctl_softc *parent_sc;

	parent_sc = dev->si_drv1;

	if ((parent_sc->sc_flags & RAIDF_OPEN) != 0)
		return (EBUSY);

	parent_sc->sc_flags |= RAIDF_OPEN;
	return (0);
}

int
raidctlclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct raidctl_softc *parent_sc;

	parent_sc = dev->si_drv1;

	parent_sc->sc_flags &= ~RAIDF_OPEN;
	return (0);
}

int
raidctlioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{
	struct raidctl_softc *parent_sc;
	struct raid_softc *sc;
	RF_Config_t *u_cfg, *k_cfg;
	RF_Raid_t *raidPtr;
	u_char *specific_buf;
	u_int unit;
	int retcode = 0;

	parent_sc = dev->si_drv1;

	switch (cmd) {
		/* configure the system */
	case RAIDFRAME_CONFIGURE:

		/* copy-in the configuration information */
		/* data points to a pointer to the configuration structure */

		u_cfg = *((RF_Config_t **) data);
		RF_Malloc(k_cfg, sizeof(RF_Config_t), (RF_Config_t *));
		if (k_cfg == NULL) {
			return (ENOMEM);
		}
		retcode = copyin((caddr_t) u_cfg, (caddr_t) k_cfg,
		    sizeof(RF_Config_t));
		if (retcode) {
			RF_Free(k_cfg, sizeof(RF_Config_t));
			rf_printf(2, "raidctlioctl: retcode=%d copyin.1\n",
				retcode);
			return (retcode);
		}
		/* allocate a buffer for the layout-specific data, and copy it
		 * in */
		if (k_cfg->layoutSpecificSize) {
			if (k_cfg->layoutSpecificSize > 10000) {
				/* sanity check */
				RF_Free(k_cfg, sizeof(RF_Config_t));
				return (EINVAL);
			}
			RF_Malloc(specific_buf, k_cfg->layoutSpecificSize,
			    (u_char *));
			if (specific_buf == NULL) {
				RF_Free(k_cfg, sizeof(RF_Config_t));
				return (ENOMEM);
			}
			retcode = copyin(k_cfg->layoutSpecific,
			    (caddr_t) specific_buf,
			    k_cfg->layoutSpecificSize);
			if (retcode) {
				RF_Free(k_cfg, sizeof(RF_Config_t));
				RF_Free(specific_buf, 
					k_cfg->layoutSpecificSize);
				rf_printf(2, "raidctlioctl: retcode=%d "
					"copyin.2\n", retcode);
				return (retcode);
			}
		} else
			specific_buf = NULL;
		k_cfg->layoutSpecific = specific_buf;

		/* should do some kind of sanity check on the configuration.
		 * Store the sum of all the bytes in the last byte? */

		/* configure the system */

		RF_Malloc(raidPtr, sizeof(*raidPtr), (RF_Raid_t *));
		if (raidPtr == NULL) {
			rf_printf(0, "No memory for raid device\n");
			RF_Free(k_cfg, sizeof(RF_Config_t));
			retcode = ENOMEM;
		}
		bzero((char *) raidPtr, sizeof(RF_Raid_t));

		/* Request a unit number for this soon-to-be device. */
		unit = raidgetunit(parent_sc, 0);
		if (unit == -1) {
			rf_printf(0, "Cannot allocate raid unit\n");
			RF_Free(raidPtr, sizeof(*raidPtr));
			goto out;
		}
		raidPtr->raidid = unit;

		if ((retcode = rf_Configure(raidPtr, k_cfg, NULL)) == 0) {

			/* allow this many simultaneous IO's to 
			   this RAID device */
			raidPtr->openings = RAIDOUTSTANDING;

			parent_sc->sc_raiddevs[unit] = raidinit(raidPtr);
			if (parent_sc->sc_raiddevs[unit] == NULL) {
				rf_printf(0, "Could not create raid device\n");
				RF_Free(raidPtr, sizeof(*raidPtr));
				goto out;
			}
			parent_sc->sc_numraid++;
			((struct raid_softc *)raidPtr->sc)->sc_parent_dev = dev;
			rf_markalldirty(raidPtr);
		} else {
			parent_sc->sc_raiddevs[unit] = NULL;
			RF_Free(raidPtr, sizeof(*raidPtr));
		}

out:
		/* free the buffers.  No return code here. */
		if (k_cfg->layoutSpecificSize) {
			RF_Free(specific_buf, k_cfg->layoutSpecificSize);
		}
		RF_Free(k_cfg, sizeof(RF_Config_t));
		break;

	case RAIDFRAME_SHUTDOWN:

		unit = *(u_int *)data;
		if ((unit >= RF_MAX_ARRAYS) ||
		    (parent_sc->sc_raiddevs[unit] == NULL))
			return (EINVAL);

		sc = parent_sc->sc_raiddevs[unit]->si_drv1;
		if ((retcode = raidlock(sc)) != 0)
			return (retcode);

		/*
		 * If somebody has a partition mounted, we shouldn't
		 * shutdown.
		 */

		if ((sc->sc_flags & RAIDF_OPEN) != 0) {
			raidunlock(sc);
			return (EBUSY);
		}

		rf_printf(0, "Shutting down RAIDframe engine\n");
		retcode = rf_Shutdown(sc->raidPtr);
		RF_THREADGROUP_WAIT_STOP(&sc->raidPtr->engine_tg);

		devstat_remove_entry(&sc->device_stats);

		disk_destroy(parent_sc->sc_raiddevs[unit]);
		raidunlock(sc);

		/* XXX Need to be able to destroy the zone */
		uma_zdestroy(sc->sc_cbufpool);

		parent_sc->sc_numraid--;
		parent_sc->sc_raiddevs[unit] = NULL;

		RF_Free(sc->raidPtr, sizeof(*raidPtr));
		RF_Free(sc, sizeof(*sc));

		break;

	default:
		retcode = ENOIOCTL;
	}

	return (retcode);
}

#if 0 /* XXX DUMP!!!! */
int
raiddump(dev)
	dev_t   dev;
{
	/* Not implemented. */
	return ENXIO;
}
#endif

/* ARGSUSED */
int
raidopen(dev, flags, fmt, td)
	dev_t   dev;
	int     flags, fmt;
	struct thread *td;
{
	struct raid_softc *sc;
	struct disk	*dp;
	int     error = 0;

	sc = dev->si_drv1;

	if ((error = raidlock(sc)) != 0)
		return (error);
	dp = &sc->sc_dkdev;

	rf_printf(1, "Opening raid device %s\n", dev->si_name);

	/* Generate overall disklabel */
	raidgetdefaultlabel(sc->raidPtr, sc, dp);

	if (sc->sc_busycount == 0) {
		/* First one... mark things as dirty... Note that we *MUST*
		 have done a configure before this.  I DO NOT WANT TO BE
		 SCRIBBLING TO RANDOM COMPONENTS UNTIL IT'S BEEN DETERMINED
		 THAT THEY BELONG TOGETHER!!!!! */
		/* XXX should check to see if we're only open for reading
		   here... If so, we needn't do this, but then need some
		   other way of keeping track of what's happened.. */

		rf_markalldirty( sc->raidPtr );
		sc->sc_flags |= RAIDF_OPEN;
	}

	/* Prevent this unit from being unconfigured while open. */
	sc->sc_busycount++;

	raidunlock(sc);

	return (error);


}
/* ARGSUSED */
int
raidclose(dev, flags, fmt, td)
	dev_t   dev;
	int     flags, fmt;
	struct thread *td;
{
	struct raid_softc *sc;
	int     error = 0;

	sc = dev->si_drv1;

	if ((error = raidlock(sc)) != 0)
		return (error);

	sc->sc_busycount--;
	if (sc->sc_busycount == 0) {
		sc->sc_flags &= ~RAIDF_OPEN;
		rf_update_component_labels(sc->raidPtr,
		    RF_FINAL_COMPONENT_UPDATE);
	}

	raidunlock(sc);
	return (0);

}

void
raidstrategy(bp)
	struct bio *bp;
{
	RF_Raid_t *raidPtr;
	struct raid_softc *sc = bp->bio_dev->si_drv1;
	int     s;

	raidPtr = sc->raidPtr;
	if (raidPtr == NULL) {
		bp->bio_error = ENODEV;
		bp->bio_flags |= BIO_ERROR;
		bp->bio_resid = bp->bio_bcount;
		biodone(bp);
		return;
	}
	if (!raidPtr->valid) {
		bp->bio_error = ENODEV;
		bp->bio_flags |= BIO_ERROR;
		bp->bio_resid = bp->bio_bcount;
		biodone(bp);
		return;
	}
	if (bp->bio_bcount == 0) {
		rf_printf(2, "b_bcount is zero..\n");
		biodone(bp);
		return;
	}

	s = splbio();

	bp->bio_resid = 0;

	/* stuff it onto our queue. XXX locking? */
	bioq_insert_tail(&sc->bio_queue, bp);

	raidstart(raidPtr);

	splx(s);
}

int
raidread(dev, uio, flags)
	dev_t   dev;
	struct uio *uio;
	int     flags;
{
	struct raid_softc *sc;

	sc = dev->si_drv1;

	return (physio(dev, uio, BIO_READ));

}

int
raidwrite(dev, uio, flags)
	dev_t   dev;
	struct uio *uio;
	int     flags;
{
	struct raid_softc *sc;
	int ret;

	sc = dev->si_drv1;

	rf_printf(3, "raidwrite\n");
	ret = physio(dev, uio, BIO_WRITE);

	return (ret);

}

int
raidioctl(dev, cmd, data, flag, td)
	dev_t   dev;
	u_long  cmd;
	caddr_t data;
	int     flag;
	struct thread *td;
{
	struct raid_softc *sc;
	RF_Raid_t *raidPtr;
	RF_RaidDisk_t *diskPtr;
	RF_AccTotals_t *totals;
	RF_DeviceConfig_t *d_cfg, **ucfgp;
	struct rf_recon_req *rrcopy, *rr;
	RF_ComponentLabel_t *clabel;
	RF_ComponentLabel_t *ci_label;
	RF_SingleComponent_t *sparePtr,*componentPtr;
	RF_SingleComponent_t *hot_spare, *component;
	RF_ProgressInfo_t progressInfo;
	int retcode = 0;
	int row, column;
	int unit;
	int i, j, d;

	sc = dev->si_drv1;
	raidPtr = sc->raidPtr;

	rf_printf(2, "raidioctl: %s %ld\n", dev->si_name, cmd);

	switch (cmd) {

	case RAIDFRAME_GET_COMPONENT_LABEL:
		/* need to read the component label for the disk indicated
		   by row,column in clabel */

		/* For practice, let's get it directly fromdisk, rather 
		   than from the in-core copy */
		RF_Malloc( clabel, sizeof( RF_ComponentLabel_t ),
			   (RF_ComponentLabel_t *));
		if (clabel == NULL)
			return (ENOMEM);

		bzero((char *) clabel, sizeof(RF_ComponentLabel_t));
		
		bcopy(data, clabel, sizeof(RF_ComponentLabel_t));

		row = clabel->row;
		column = clabel->column;

		if ((row < 0) || (row >= raidPtr->numRow) ||
		    (column < 0) || (column >= raidPtr->numCol +
				     raidPtr->numSpare)) {
			RF_Free( clabel, sizeof(RF_ComponentLabel_t));
			return(EINVAL);
		}

		raidread_component_label(raidPtr->Disks[row][column].dev, 
				raidPtr->raid_cinfo[row][column].ci_vp, 
				clabel );

		bcopy(clabel, data, sizeof(RF_ComponentLabel_t));
		RF_Free( clabel, sizeof(RF_ComponentLabel_t));
		return (retcode);

	case RAIDFRAME_SET_COMPONENT_LABEL:
		clabel = (RF_ComponentLabel_t *) data;

		/* XXX check the label for valid stuff... */
		/* Note that some things *should not* get modified --
		   the user should be re-initing the labels instead of 
		   trying to patch things.
		   */

		rf_printf(1, "Got component label:\n");
		rf_printf(1, "Version: %d\n",clabel->version);
		rf_printf(1, "Serial Number: %d\n",clabel->serial_number);
		rf_printf(1, "Mod counter: %d\n",clabel->mod_counter);
		rf_printf(1, "Row: %d\n", clabel->row);
		rf_printf(1, "Column: %d\n", clabel->column);
		rf_printf(1, "Num Rows: %d\n", clabel->num_rows);
		rf_printf(1, "Num Columns: %d\n", clabel->num_columns);
		rf_printf(1, "Clean: %d\n", clabel->clean);
		rf_printf(1, "Status: %d\n", clabel->status);

		row = clabel->row;
		column = clabel->column;

		if ((row < 0) || (row >= raidPtr->numRow) ||
		    (column < 0) || (column >= raidPtr->numCol)) {
			return(EINVAL);
		}

		/* XXX this isn't allowed to do anything for now :-) */

		/* XXX and before it is, we need to fill in the rest
		   of the fields!?!?!?! */
#if 0
		raidwrite_component_label( 
                            raidPtr->Disks[row][column].dev, 
			    raidPtr->raid_cinfo[row][column].ci_vp, 
			    clabel );
#endif
		return (0);

	case RAIDFRAME_INIT_LABELS:
		MALLOC(ci_label, RF_ComponentLabel_t *,
		    sizeof(RF_ComponentLabel_t), M_RAIDFRAME,
		    M_WAITOK | M_ZERO);
		clabel = (RF_ComponentLabel_t *) data;
		/* 
		   we only want the serial number from
		   the above.  We get all the rest of the information
		   from the config that was used to create this RAID
		   set. 
		   */

		raidPtr->serial_number = clabel->serial_number;
		
		raid_init_component_label(raidPtr, ci_label);
		ci_label->serial_number = clabel->serial_number;

		for(row=0;row<raidPtr->numRow;row++) {
			ci_label->row = row;
			for(column=0;column<raidPtr->numCol;column++) {
				diskPtr = &raidPtr->Disks[row][column];
				if (!RF_DEAD_DISK(diskPtr->status)) {
					ci_label->partitionSize =
					    diskPtr->partitionSize;
					ci_label->column = column;
					raidwrite_component_label( 
					    raidPtr->Disks[row][column].dev, 
					    raidPtr->raid_cinfo[row][column].ci_vp, 
					  ci_label );
				}
			}
		}

		FREE(ci_label, M_RAIDFRAME);
		return (retcode);
	case RAIDFRAME_SET_AUTOCONFIG:
		d = rf_set_autoconfig(raidPtr, *(int *) data);
		rf_printf(1, "New autoconfig value is: %d\n", d);
		*(int *) data = d;
		return (retcode);

	case RAIDFRAME_SET_ROOT:
		d = rf_set_rootpartition(raidPtr, *(int *) data);
		rf_printf(1, "New rootpartition value is: %d\n", d);
		*(int *) data = d;
		return (retcode);

		/* initialize all parity */
	case RAIDFRAME_REWRITEPARITY:

		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* Parity for RAID 0 is trivially correct */
			raidPtr->parity_good = RF_RAID_CLEAN;
			return(0);
		}
		
		if (raidPtr->parity_rewrite_in_progress == 1) {
			/* Re-write is already in progress! */
			return(EINVAL);
		}

		retcode = RF_CREATE_THREAD(raidPtr->parity_rewrite_thread,
					   rf_RewriteParityThread,
					   raidPtr,"raid_parity");
		return (retcode);


	case RAIDFRAME_ADD_HOT_SPARE:
		MALLOC(hot_spare, RF_SingleComponent_t *,
		    sizeof(RF_SingleComponent_t), M_RAIDFRAME,
		     M_WAITOK | M_ZERO);
		sparePtr = (RF_SingleComponent_t *) data;
		memcpy( hot_spare, sparePtr, sizeof(RF_SingleComponent_t));
		retcode = rf_add_hot_spare(raidPtr, hot_spare);
		FREE(hot_spare, M_RAIDFRAME);
		return(retcode);

	case RAIDFRAME_REMOVE_HOT_SPARE:
		return(retcode);

	case RAIDFRAME_DELETE_COMPONENT:
		MALLOC(component, RF_SingleComponent_t *,
		    sizeof(RF_SingleComponent_t), M_RAIDFRAME,
		     M_WAITOK | M_ZERO);
		componentPtr = (RF_SingleComponent_t *)data;
		memcpy( component, componentPtr, 
			sizeof(RF_SingleComponent_t));
		retcode = rf_delete_component(raidPtr, component);
		FREE(component, M_RAIDFRAME);
		return(retcode);

	case RAIDFRAME_INCORPORATE_HOT_SPARE:
		MALLOC(component, RF_SingleComponent_t *,
		    sizeof(RF_SingleComponent_t), M_RAIDFRAME,
		     M_WAITOK | M_ZERO);
		componentPtr = (RF_SingleComponent_t *)data;
		memcpy( component, componentPtr, 
			sizeof(RF_SingleComponent_t));
		retcode = rf_incorporate_hot_spare(raidPtr, component);
		FREE(component, M_RAIDFRAME);
		return(retcode);

	case RAIDFRAME_REBUILD_IN_PLACE:

		MALLOC(component, RF_SingleComponent_t *,
		    sizeof(RF_SingleComponent_t), M_RAIDFRAME,
		     M_WAITOK | M_ZERO);
		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* Can't do this on a RAID 0!! */
			FREE(component, M_RAIDFRAME);
			return(EINVAL);
		}

		if (raidPtr->recon_in_progress == 1) {
			/* a reconstruct is already in progress! */
			FREE(component, M_RAIDFRAME);
			return(EINVAL);
		}

		componentPtr = (RF_SingleComponent_t *) data;
		memcpy( component, componentPtr, 
			sizeof(RF_SingleComponent_t));
		row = component->row;
		column = component->column;
		unit = raidPtr->raidid;
		rf_printf(0, "raid%d Rebuild: %d %d\n", unit, row, column);
		if ((row < 0) || (row >= raidPtr->numRow) ||
		    (column < 0) || (column >= raidPtr->numCol)) {
			FREE(component, M_RAIDFRAME);
			return(EINVAL);
		}

		RF_Malloc(rrcopy, sizeof(*rrcopy), (struct rf_recon_req *));
		if (rrcopy == NULL) {
			FREE(component, M_RAIDFRAME);
			return(ENOMEM);
		}

		rrcopy->raidPtr = (void *) raidPtr;
		rrcopy->row = row;
		rrcopy->col = column;

		retcode = RF_CREATE_THREAD(raidPtr->recon_thread,
					   rf_ReconstructInPlaceThread,
					   rrcopy,"raid_reconip");
		FREE(component, M_RAIDFRAME);
		return(retcode);

	case RAIDFRAME_GET_UNIT:

		*(int *)data = raidPtr->raidid;
		return (0);

	case RAIDFRAME_GET_INFO:
		if (!raidPtr->valid)
			return (ENODEV);
		ucfgp = (RF_DeviceConfig_t **) data;
		RF_Malloc(d_cfg, sizeof(RF_DeviceConfig_t),
			  (RF_DeviceConfig_t *));
		if (d_cfg == NULL)
			return (ENOMEM);
		bzero((char *) d_cfg, sizeof(RF_DeviceConfig_t));
		d_cfg->rows = raidPtr->numRow;
		d_cfg->cols = raidPtr->numCol;
		d_cfg->ndevs = raidPtr->numRow * raidPtr->numCol;
		if (d_cfg->ndevs >= RF_MAX_DISKS) {
			RF_Free(d_cfg, sizeof(RF_DeviceConfig_t));
			return (ENOMEM);
		}
		d_cfg->nspares = raidPtr->numSpare;
		if (d_cfg->nspares >= RF_MAX_DISKS) {
			RF_Free(d_cfg, sizeof(RF_DeviceConfig_t));
			return (ENOMEM);
		}
		d_cfg->maxqdepth = raidPtr->maxQueueDepth;
		d = 0;
		for (i = 0; i < d_cfg->rows; i++) {
			for (j = 0; j < d_cfg->cols; j++) {
				d_cfg->devs[d] = raidPtr->Disks[i][j];
				d++;
			}
		}
		for (j = d_cfg->cols, i = 0; i < d_cfg->nspares; i++, j++) {
			d_cfg->spares[i] = raidPtr->Disks[0][j];
		}

		retcode = copyout(d_cfg, *ucfgp, sizeof(RF_DeviceConfig_t));

		RF_Free(d_cfg, sizeof(RF_DeviceConfig_t));

		return (retcode);

	case RAIDFRAME_CHECK_PARITY:
		*(int *) data = raidPtr->parity_good;
		return (0);

	case RAIDFRAME_RESET_ACCTOTALS:
		bzero(&raidPtr->acc_totals, sizeof(raidPtr->acc_totals));
		return (0);

	case RAIDFRAME_GET_ACCTOTALS:
		totals = (RF_AccTotals_t *) data;
		*totals = raidPtr->acc_totals;
		return (0);

	case RAIDFRAME_KEEP_ACCTOTALS:
		raidPtr->keep_acc_totals = *(int *)data;
		return (0);

	case RAIDFRAME_GET_SIZE:
		*(int *) data = raidPtr->totalSectors;
		return (0);

		/* fail a disk & optionally start reconstruction */
	case RAIDFRAME_FAIL_DISK:

		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* Can't do this on a RAID 0!! */
			return(EINVAL);
		}

		rr = (struct rf_recon_req *) data;

		if (rr->row < 0 || rr->row >= raidPtr->numRow
		    || rr->col < 0 || rr->col >= raidPtr->numCol)
			return (EINVAL);

		rf_printf(0, "%s: Failing the disk: row: %d col: %d\n",
		       dev->si_name, rr->row, rr->col);

		/* make a copy of the recon request so that we don't rely on
		 * the user's buffer */
		RF_Malloc(rrcopy, sizeof(*rrcopy), (struct rf_recon_req *));
		if (rrcopy == NULL)
			return(ENOMEM);
		bcopy(rr, rrcopy, sizeof(*rr));
		rrcopy->raidPtr = (void *) raidPtr;

		retcode = RF_CREATE_THREAD(raidPtr->recon_thread,
					   rf_ReconThread,
					   rrcopy,"raid_recon");
		return (0);

		/* invoke a copyback operation after recon on whatever disk
		 * needs it, if any */
	case RAIDFRAME_COPYBACK:

		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0!! */
			return(EINVAL);
		}

		if (raidPtr->copyback_in_progress == 1) {
			/* Copyback is already in progress! */
			return(EINVAL);
		}

		retcode = RF_CREATE_THREAD(raidPtr->copyback_thread,
					   rf_CopybackThread,
					   raidPtr,"raid_copyback");
		return (retcode);

		/* return the percentage completion of reconstruction */
	case RAIDFRAME_CHECK_RECON_STATUS:
		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0, so tell the
			   user it's done. */
			*(int *) data = 100;
			return(0);
		}
		row = 0; /* XXX we only consider a single row... */
		if (raidPtr->status[row] != rf_rs_reconstructing)
			*(int *) data = 100;
		else
			*(int *) data = raidPtr->reconControl[row]->percentComplete;
		return (0);
	case RAIDFRAME_CHECK_RECON_STATUS_EXT:
		row = 0; /* XXX we only consider a single row... */
		if (raidPtr->status[row] != rf_rs_reconstructing) {
			progressInfo.remaining = 0;
			progressInfo.completed = 100;
			progressInfo.total = 100;
		} else {
			progressInfo.total = 
				raidPtr->reconControl[row]->numRUsTotal;
			progressInfo.completed = 
				raidPtr->reconControl[row]->numRUsComplete;
			progressInfo.remaining = progressInfo.total -
				progressInfo.completed;
		}
		bcopy((caddr_t) &progressInfo, data, sizeof(RF_ProgressInfo_t));
		return (retcode);

	case RAIDFRAME_CHECK_PARITYREWRITE_STATUS:
		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0, so tell the
			   user it's done. */
			*(int *) data = 100;
			return(0);
		}
		if (raidPtr->parity_rewrite_in_progress == 1) {
			*(int *) data = 100 * 
				raidPtr->parity_rewrite_stripes_done / 
				raidPtr->Layout.numStripe;
		} else {
			*(int *) data = 100;
		}
		return (0);

	case RAIDFRAME_CHECK_PARITYREWRITE_STATUS_EXT:
		if (raidPtr->parity_rewrite_in_progress == 1) {
			progressInfo.total = raidPtr->Layout.numStripe;
			progressInfo.completed = 
				raidPtr->parity_rewrite_stripes_done;
			progressInfo.remaining = progressInfo.total -
				progressInfo.completed;
		} else {
			progressInfo.remaining = 0;
			progressInfo.completed = 100;
			progressInfo.total = 100;
		}
		bcopy((caddr_t) &progressInfo, data, sizeof(RF_ProgressInfo_t));
		return (retcode);

	case RAIDFRAME_CHECK_COPYBACK_STATUS:
		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0 */
			*(int *) data = 100;
			return(0);
		}
		if (raidPtr->copyback_in_progress == 1) {
			*(int *) data = 100 * raidPtr->copyback_stripes_done /
				raidPtr->Layout.numStripe;
		} else {
			*(int *) data = 100;
		}
		return (0);

	case RAIDFRAME_CHECK_COPYBACK_STATUS_EXT:
		if (raidPtr->copyback_in_progress == 1) {
			progressInfo.total = raidPtr->Layout.numStripe;
			progressInfo.completed = 
				raidPtr->copyback_stripes_done;
			progressInfo.remaining = progressInfo.total -
				progressInfo.completed;
		} else {
			progressInfo.remaining = 0;
			progressInfo.completed = 100;
			progressInfo.total = 100;
		}
		bcopy((caddr_t) &progressInfo, data, sizeof(RF_ProgressInfo_t));
		return (retcode);

		/* the sparetable daemon calls this to wait for the kernel to
		 * need a spare table. this ioctl does not return until a
		 * spare table is needed. XXX -- calling mpsleep here in the
		 * ioctl code is almost certainly wrong and evil. -- XXX XXX
		 * -- I should either compute the spare table in the kernel,
		 * or have a different -- XXX XXX -- interface (a different
		 * character device) for delivering the table     -- XXX */
#if 0
	case RAIDFRAME_SPARET_WAIT:
		RF_LOCK_MUTEX(rf_sparet_wait_mutex);
		while (!rf_sparet_wait_queue)
			mpsleep(&rf_sparet_wait_queue, (PZERO + 1) | PCATCH, "sparet wait", 0, (void *) simple_lock_addr(rf_sparet_wait_mutex), MS_LOCK_SIMPLE);
		waitreq = rf_sparet_wait_queue;
		rf_sparet_wait_queue = rf_sparet_wait_queue->next;
		RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);

		/* structure assignment */
		*((RF_SparetWait_t *) data) = *waitreq;	

		RF_Free(waitreq, sizeof(*waitreq));
		return (0);

		/* wakes up a process waiting on SPARET_WAIT and puts an error
		 * code in it that will cause the dameon to exit */
	case RAIDFRAME_ABORT_SPARET_WAIT:
		RF_Malloc(waitreq, sizeof(*waitreq), (RF_SparetWait_t *));
		waitreq->fcol = -1;
		RF_LOCK_MUTEX(rf_sparet_wait_mutex);
		waitreq->next = rf_sparet_wait_queue;
		rf_sparet_wait_queue = waitreq;
		RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);
		wakeup(&rf_sparet_wait_queue);
		return (0);

		/* used by the spare table daemon to deliver a spare table
		 * into the kernel */
	case RAIDFRAME_SEND_SPARET:

		/* install the spare table */
		retcode = rf_SetSpareTable(raidPtr, *(void **) data);

		/* respond to the requestor.  the return status of the spare
		 * table installation is passed in the "fcol" field */
		RF_Malloc(waitreq, sizeof(*waitreq), (RF_SparetWait_t *));
		waitreq->fcol = retcode;
		RF_LOCK_MUTEX(rf_sparet_wait_mutex);
		waitreq->next = rf_sparet_resp_queue;
		rf_sparet_resp_queue = waitreq;
		wakeup(&rf_sparet_resp_queue);
		RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);

		return (retcode);
#endif

	default:
		retcode = ENOIOCTL;
		break; /* fall through to the os-specific code below */

	}

	return (retcode);

}


/* raidinit -- complete the rest of the initialization for the
   RAIDframe device.  */


static dev_t 
raidinit(raidPtr)
	RF_Raid_t *raidPtr;
{
	struct raid_softc *sc;
	dev_t	diskdev;

	RF_Malloc(sc, sizeof(struct raid_softc), (struct raid_softc *));
	if (sc == NULL) {
		rf_printf(1, "No memory for raid device\n");
		return(NULL);
	}

	sc->raidPtr = raidPtr;

	/* XXX Should check return code here */
	bioq_init(&sc->bio_queue);
	sc->sc_cbufpool = uma_zcreate("raidpl", sizeof(struct raidbuf), NULL,
	    NULL, NULL, NULL, 0, 0); 

	/* XXX There may be a weird interaction here between this, and
	 * protectedSectors, as used in RAIDframe.  */

	sc->sc_size = raidPtr->totalSectors;

	/* Create the disk device */
	diskdev = disk_create(raidPtr->raidid, &sc->sc_dkdev, 0, &raid_cdevsw,
		    &raiddisk_cdevsw);
	if (diskdev == NODEV) {
		rf_printf(1, "disk_create failed\n");
		return (NULL);
	}
	sc->sc_dkdev.d_dev->si_drv1 = sc;
	sc->sc_dev = diskdev;
	raidPtr->sc = sc;

	/* Register with devstat */
	devstat_add_entry(&sc->device_stats, "raid", raidPtr->raidid, 0,
			  DEVSTAT_NO_BLOCKSIZE | DEVSTAT_NO_ORDERED_TAGS,
			  DEVSTAT_TYPE_IF_OTHER, DEVSTAT_PRIORITY_ARRAY);

	return (diskdev);
}

/* wake up the daemon & tell it to get us a spare table
 * XXX
 * the entries in the queues should be tagged with the raidPtr
 * so that in the extremely rare case that two recons happen at once, 
 * we know for which device were requesting a spare table
 * XXX
 * 
 * XXX This code is not currently used. GO
 */
int 
rf_GetSpareTableFromDaemon(req)
	RF_SparetWait_t *req;
{
	int     retcode;

	RF_LOCK_MUTEX(rf_sparet_wait_mutex);
	req->next = rf_sparet_wait_queue;
	rf_sparet_wait_queue = req;
	wakeup(&rf_sparet_wait_queue);

	/* mpsleep unlocks the mutex */
	while (!rf_sparet_resp_queue) {
		tsleep(&rf_sparet_resp_queue, PRIBIO,
		    "raidframe getsparetable", 0);
	}
	req = rf_sparet_resp_queue;
	rf_sparet_resp_queue = req->next;
	RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);

	retcode = req->fcol;
	RF_Free(req, sizeof(*req));	/* this is not the same req as we
					 * alloc'd */
	return (retcode);
}

/* a wrapper around rf_DoAccess that extracts appropriate info from the 
 * bp & passes it down.
 * any calls originating in the kernel must use non-blocking I/O
 * do some extra sanity checking to return "appropriate" error values for
 * certain conditions (to make some standard utilities work)
 * 
 * Formerly known as: rf_DoAccessKernel
 */
void
raidstart(raidPtr)
	RF_Raid_t *raidPtr;
{
	RF_SectorCount_t num_blocks, pb, sum;
	RF_RaidAddr_t raid_addr;
	struct raid_softc *sc;
	struct bio *bp;
	daddr_t blocknum;
	int     unit, retcode, do_async;

	unit = raidPtr->raidid;
	sc = raidPtr->sc;
	
	/* quick check to see if anything has died recently */
	RF_LOCK_MUTEX(raidPtr->mutex);
	if (raidPtr->numNewFailures > 0) {
		raidPtr->numNewFailures--;
		RF_UNLOCK_MUTEX(raidPtr->mutex);
		rf_update_component_labels(raidPtr, 
					   RF_NORMAL_COMPONENT_UPDATE);
	} else
		RF_UNLOCK_MUTEX(raidPtr->mutex);

	/* Check to see if we're at the limit... */
	RF_LOCK_MUTEX(raidPtr->mutex);
	while (raidPtr->openings > 0) {
		RF_UNLOCK_MUTEX(raidPtr->mutex);

		/* get the next item, if any, from the queue */
		if ((bp = bioq_first(&sc->bio_queue)) == NULL) {
			/* nothing more to do */
			return;
		}
		bioq_remove(&sc->bio_queue, bp);

		/* Ok, for the bp we have here, bp->b_blkno is relative to the
		 * partition.. Need to make it absolute to the underlying 
		 * device.. */

		blocknum = bp->bio_blkno;
#if 0 /* XXX Is this needed? */
		if (DISKPART(bp->bio_dev) != RAW_PART) {
			struct partition *pp;
			pp = &sc->sc_dkdev.d_label.d_partitions[DISKPART(
			    bp->bio_dev)];
			blocknum += pp->p_offset;
		}
#endif

		rf_printf(3, "Blocks: %ld, %ld\n", (long)bp->bio_blkno, (long)blocknum);
		
		rf_printf(3, "bp->bio_bcount = %d\n", (int) bp->bio_bcount);
		rf_printf(3, "bp->bio_resid = %d\n", (int) bp->bio_resid);
		
		/* *THIS* is where we adjust what block we're going to... 
		 * but DO NOT TOUCH bp->bio_blkno!!! */
		raid_addr = blocknum;
		
		num_blocks = bp->bio_bcount >> raidPtr->logBytesPerSector;
		pb = (bp->bio_bcount & raidPtr->sectorMask) ? 1 : 0;
		sum = raid_addr + num_blocks + pb;
		if (rf_debugKernelAccess) {
			rf_printf(0, "raid_addr=0x%x sum=%d num_blocks=%d(+%d) "
				    "(%d)\n", (int)raid_addr, (int)sum, 
				    (int)num_blocks, (int)pb,
				    (int)bp->bio_resid);
		}
		if ((sum > raidPtr->totalSectors) || (sum < raid_addr)
		    || (sum < num_blocks) || (sum < pb)) {
			bp->bio_error = ENOSPC;
			bp->bio_flags |= BIO_ERROR;
			bp->bio_resid = bp->bio_bcount;
			biodone(bp);
			RF_LOCK_MUTEX(raidPtr->mutex);
			continue;
		}
		/*
		 * XXX rf_DoAccess() should do this, not just DoAccessKernel()
		 */
		
		if (bp->bio_bcount & raidPtr->sectorMask) {
			bp->bio_error = EINVAL;
			bp->bio_flags |= BIO_ERROR;
			bp->bio_resid = bp->bio_bcount;
			biodone(bp);
			RF_LOCK_MUTEX(raidPtr->mutex);
			continue;
			
		}
		rf_printf(3, "Calling DoAccess..\n");
		

		RF_LOCK_MUTEX(raidPtr->mutex);
		raidPtr->openings--;
		RF_UNLOCK_MUTEX(raidPtr->mutex);

		/*
		 * Everything is async.
		 */
		do_async = 1;

		devstat_start_transaction(&sc->device_stats);

		/* XXX we're still at splbio() here... do we *really* 
		   need to be? */

		/* don't ever condition on bp->bio_cmd & BIO_WRITE.  
		 * always condition on BIO_READ instead */
		
		retcode = rf_DoAccess(raidPtr, (bp->bio_cmd & BIO_READ) ?
				      RF_IO_TYPE_READ : RF_IO_TYPE_WRITE,
				      do_async, raid_addr, num_blocks,
				      bp->bio_data, bp, NULL, NULL, 
				      RF_DAG_NONBLOCKING_IO, NULL, NULL, NULL);


		RF_LOCK_MUTEX(raidPtr->mutex);
	}
	RF_UNLOCK_MUTEX(raidPtr->mutex);
}




/* invoke an I/O from kernel mode.  Disk queue should be locked upon entry */

int 
rf_DispatchKernelIO(queue, req)
	RF_DiskQueue_t *queue;
	RF_DiskQueueData_t *req;
{
	int     op = (req->type == RF_IO_TYPE_READ) ? BIO_READ : BIO_WRITE;
	struct bio *bp;
	struct raidbuf *raidbp = NULL;
	struct raid_softc *sc;

	/* XXX along with the vnode, we also need the softc associated with
	 * this device.. */

	req->queue = queue;

	sc = queue->raidPtr->sc;

	rf_printf(3, "DispatchKernelIO %s\n", sc->sc_dev->si_name);

	bp = req->bp;
#if 1
	/* XXX when there is a physical disk failure, someone is passing us a
	 * buffer that contains old stuff!!  Attempt to deal with this problem
	 * without taking a performance hit... (not sure where the real bug
	 * is.  It's buried in RAIDframe somewhere) :-(  GO ) */

	if (bp->bio_flags & BIO_ERROR) {
		bp->bio_flags &= ~BIO_ERROR;
	}
	if (bp->bio_error != 0) {
		bp->bio_error = 0;
	}
#endif
	raidbp = RAIDGETBUF(sc);

	raidbp->rf_flags = 0;	/* XXX not really used anywhere... */

	/*
	 * context for raidiodone
	 */
	raidbp->rf_obp = bp;
	raidbp->req = req;

#if 0	/* XXX */
	LIST_INIT(&raidbp->rf_buf.b_dep);
#endif

	switch (req->type) {
	case RF_IO_TYPE_NOP:	/* used primarily to unlock a locked queue */
		/* XXX need to do something extra here.. */
		/* I'm leaving this in, as I've never actually seen it used,
		 * and I'd like folks to report it... GO */
		rf_printf(2, "WAKEUP CALLED\n");
		queue->numOutstanding++;

		/* XXX need to glue the original buffer into this?  */

		KernelWakeupFunc(&raidbp->rf_buf);
		break;

	case RF_IO_TYPE_READ:
	case RF_IO_TYPE_WRITE:

		if (req->tracerec) {
			RF_ETIMER_START(req->tracerec->timer);
		}
		InitBP(&raidbp->rf_buf, queue->rf_cinfo->ci_vp,
		    op | bp->bio_cmd, queue->rf_cinfo->ci_dev,
		    req->sectorOffset, req->numSector,
		    req->buf, KernelWakeupFunc, (void *) req,
		    queue->raidPtr->logBytesPerSector, req->b_proc);

		if (rf_debugKernelAccess) {
			rf_printf(0, "dispatch: bp->bio_blkno = %ld\n",
				(long) bp->bio_blkno);
		}
		queue->numOutstanding++;
		queue->last_deq_sector = req->sectorOffset;
		/* acc wouldn't have been let in if there were any pending
		 * reqs at any other priority */
		queue->curPriority = req->priority;

		rf_printf(3, "Going for %c to %s row %d col %d\n",
			req->type, sc->sc_dev->si_name, queue->row, queue->col);
		rf_printf(3, "sector %d count %d (%d bytes) %d\n",
			(int) req->sectorOffset, (int) req->numSector,
			(int) (req->numSector <<
			    queue->raidPtr->logBytesPerSector),
			(int) queue->raidPtr->logBytesPerSector);
#if 0	/* XXX */
		if ((raidbp->rf_buf.bio_cmd & BIO_READ) == 0) {
			raidbp->rf_buf.b_vp->v_numoutput++;
		}
#endif
		BIO_STRATEGY(&raidbp->rf_buf, 0);

		break;

	default:
		panic("bad req->type in rf_DispatchKernelIO");
	}
	rf_printf(3, "Exiting from DispatchKernelIO\n");
	/* splx(s); */ /* want to test this */
	return (0);
}
/* this is the callback function associated with a I/O invoked from
   kernel code.
 */
static void 
KernelWakeupFunc(vbp)
	struct bio *vbp;
{
	RF_DiskQueueData_t *req = NULL;
	RF_DiskQueue_t *queue;
	struct raidbuf *raidbp = (struct raidbuf *) vbp;
	struct bio *bp;
	struct raid_softc *sc;
	int s;

	s = splbio();
	rf_printf(2, "recovering the request queue:\n");
	req = raidbp->req;

	bp = raidbp->rf_obp;
	queue = (RF_DiskQueue_t *) req->queue;
	sc = queue->raidPtr->sc;

	if (raidbp->rf_buf.bio_flags & BIO_ERROR) {
		bp->bio_flags |= BIO_ERROR;
		bp->bio_error = raidbp->rf_buf.bio_error ?
		    raidbp->rf_buf.bio_error : EIO;
	}

	/* XXX methinks this could be wrong... */
#if 1
	bp->bio_resid = raidbp->rf_buf.bio_resid;
#endif

	if (req->tracerec) {
		RF_ETIMER_STOP(req->tracerec->timer);
		RF_ETIMER_EVAL(req->tracerec->timer);
		RF_LOCK_MUTEX(rf_tracing_mutex);
		req->tracerec->diskwait_us += RF_ETIMER_VAL_US(req->tracerec->timer);
		req->tracerec->phys_io_us += RF_ETIMER_VAL_US(req->tracerec->timer);
		req->tracerec->num_phys_ios++;
		RF_UNLOCK_MUTEX(rf_tracing_mutex);
	}
	bp->bio_bcount = raidbp->rf_buf.bio_bcount;	/* XXXX ? */

	/* XXX Ok, let's get aggressive... If BIO_ERROR is set, let's go
	 * ballistic, and mark the component as hosed... */

	if (bp->bio_flags & BIO_ERROR) {
		/* Mark the disk as dead */
		/* but only mark it once... */
		if (queue->raidPtr->Disks[queue->row][queue->col].status ==
		    rf_ds_optimal) {
			rf_printf(0, "%s: IO Error.  Marking %s as "
			    "failed.\n", sc->sc_dev->si_name, queue->raidPtr->
			    Disks[queue->row][queue->col].devname);
			queue->raidPtr->Disks[queue->row][queue->col].status =
			    rf_ds_failed;
			queue->raidPtr->status[queue->row] = rf_rs_degraded;
			queue->raidPtr->numFailures++;
			queue->raidPtr->numNewFailures++;
		} else {	/* Disk is already dead... */
			/* printf("Disk already marked as dead!\n"); */
		}

	}

	RAIDPUTBUF(sc, raidbp);

	rf_DiskIOComplete(queue, req, (bp->bio_flags & BIO_ERROR) ? 1 : 0);
	(req->CompleteFunc)(req->argument, (bp->bio_flags & BIO_ERROR) ? 1 : 0);

	splx(s);
}



/*
 * initialize a buf structure for doing an I/O in the kernel.
 */
static void 
InitBP(bp, b_vp, rw_flag, dev, startSect, numSect, buf, cbFunc, cbArg,
       logBytesPerSector, b_proc)
	struct bio *bp;
	struct vnode *b_vp;
	unsigned rw_flag;
	dev_t dev;
	RF_SectorNum_t startSect;
	RF_SectorCount_t numSect;
	caddr_t buf;
	void (*cbFunc) (struct bio *);
	void *cbArg;
	int logBytesPerSector;
	struct proc *b_proc;
{
	/* bp->b_flags       = B_PHYS | rw_flag; */
	bp->bio_cmd = rw_flag;	/* XXX need B_PHYS here too? */
	bp->bio_bcount = numSect << logBytesPerSector;
#if 0	/* XXX */
	bp->bio_bufsize = bp->bio_bcount;
#endif
	bp->bio_error = 0;
	bp->bio_dev = dev;
	bp->bio_data = buf;
	bp->bio_blkno = startSect;
	bp->bio_resid = bp->bio_bcount;	/* XXX is this right!?!?!! */
	if (bp->bio_bcount == 0) {
		panic("bp->bio_bcount is zero in InitBP!!\n");
	}
/*
	bp->b_proc = b_proc;
	bp->b_vp = b_vp;
*/
	bp->bio_done = cbFunc;

}

static void
raidgetdefaultlabel(raidPtr, sc, dp)
	RF_Raid_t *raidPtr;
	struct raid_softc *sc;
	struct disk *dp;
{
	rf_printf(1, "Building a default label...\n");
	if (dp == NULL)
		panic("raidgetdefaultlabel(): dp is NULL\n");

	/* fabricate a label... */
	dp->d_mediasize = raidPtr->totalSectors * raidPtr->bytesPerSector;
	dp->d_sectorsize = raidPtr->bytesPerSector;
	dp->d_fwsectors = raidPtr->Layout.dataSectorsPerStripe;
	dp->d_fwheads = 4 * raidPtr->numCol;

}
/*
 * Lookup the provided name in the filesystem.  If the file exists,
 * is a valid block device, and isn't being used by anyone else,
 * set *vpp to the file's vnode.
 * You'll find the original of this in ccd.c
 */
int
raidlookup(path, td, vpp)
	char   *path;
	struct thread *td;
	struct vnode **vpp;	/* result */
{
	struct nameidata *nd;
	struct vnode *vp;
	struct vattr *va;
	struct proc *p;
	int     error = 0, flags;

	MALLOC(nd, struct nameidata *, sizeof(struct nameidata), M_TEMP, M_NOWAIT | M_ZERO);
	MALLOC(va, struct vattr *, sizeof(struct vattr), M_TEMP, M_NOWAIT | M_ZERO);
	if ((nd == NULL) || (va == NULL)) {
		printf("Out of memory?\n");
		return (ENOMEM);
	}

	/* Sanity check the p_fd fields.  This is really just a hack */
	p = td->td_proc;
	if (!p->p_fd->fd_rdir || !p->p_fd->fd_cdir)
		printf("Warning: p_fd fields not set\n");

	if (!td->td_proc->p_fd->fd_rdir)
		p->p_fd->fd_rdir = rootvnode;

	if (!p->p_fd->fd_cdir)
		p->p_fd->fd_cdir = rootvnode;

	NDINIT(nd, LOOKUP, FOLLOW, UIO_SYSSPACE, path, curthread);
	flags = FREAD | FWRITE;
	if ((error = vn_open(nd, &flags, 0)) != 0) {
		rf_printf(2, "RAIDframe: vn_open returned %d\n", error);
		goto end1;
	}
	vp = nd->ni_vp;
	if (vp->v_usecount > 1) {
		rf_printf(1, "raidlookup() vp->v_usecount= %d\n", vp->v_usecount);
		error = EBUSY;
		goto end;
	}
	if ((error = VOP_GETATTR(vp, va, td->td_ucred, td)) != 0) {
		rf_printf(1, "raidlookup() VOP_GETATTR returned %d", error);
		goto end;
	}
	/* XXX: eventually we should handle VREG, too. */
	if (va->va_type != VCHR) {
		rf_printf(1, "Returning ENOTBLK\n");
		error = ENOTBLK;
	}
	*vpp = vp;

end:
	VOP_UNLOCK(vp, 0, td);
	NDFREE(nd, NDF_ONLY_PNBUF);
end1:
	FREE(nd, M_TEMP);
	FREE(va, M_TEMP);
	return (error);
}
/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 * (Hmm... where have we seen this warning before :->  GO )
 */
static int
raidlock(sc)
	struct raid_softc *sc;
{
	int     error;

	while ((sc->sc_flags & RAIDF_LOCKED) != 0) {
		sc->sc_flags |= RAIDF_WANTED;
		if ((error =
			tsleep(sc, PRIBIO | PCATCH, "raidlck", 0)) != 0)
			return (error);
	}
	sc->sc_flags |= RAIDF_LOCKED;
	return (0);
}
/*
 * Unlock and wake up any waiters.
 */
static void
raidunlock(sc)
	struct raid_softc *sc;
{

	sc->sc_flags &= ~RAIDF_LOCKED;
	if ((sc->sc_flags & RAIDF_WANTED) != 0) {
		sc->sc_flags &= ~RAIDF_WANTED;
		wakeup(sc);
	}
}
 

#define RF_COMPONENT_INFO_OFFSET  16384 /* bytes */
#define RF_COMPONENT_INFO_SIZE     1024 /* bytes */

int 
raidmarkclean(dev_t dev, struct vnode *b_vp, int mod_counter)
{
	RF_ComponentLabel_t *clabel;

	MALLOC(clabel, RF_ComponentLabel_t *, sizeof(RF_ComponentLabel_t),
	    M_RAIDFRAME, M_NOWAIT | M_ZERO);
	if (clabel == NULL) {
		printf("raidmarkclean: Out of memory?\n");
		return (ENOMEM);
	}

	raidread_component_label(dev, b_vp, clabel);
	clabel->mod_counter = mod_counter;
	clabel->clean = RF_RAID_CLEAN;
	raidwrite_component_label(dev, b_vp, clabel);
	FREE(clabel, M_RAIDFRAME);
	return(0);
}


int 
raidmarkdirty(dev_t dev, struct vnode *b_vp, int mod_counter)
{
	RF_ComponentLabel_t *clabel;

	MALLOC(clabel, RF_ComponentLabel_t *, sizeof(RF_ComponentLabel_t),
	    M_RAIDFRAME, M_NOWAIT | M_ZERO);
	if (clabel == NULL) {
		printf("raidmarkclean: Out of memory?\n");
		return (ENOMEM);
	}

	raidread_component_label(dev, b_vp, clabel);
	clabel->mod_counter = mod_counter;
	clabel->clean = RF_RAID_DIRTY;
	raidwrite_component_label(dev, b_vp, clabel);
	FREE(clabel, M_RAIDFRAME);
	return(0);
}

/* ARGSUSED */
int
raidread_component_label(dev, b_vp, clabel)
	dev_t dev;
	struct vnode *b_vp;
	RF_ComponentLabel_t *clabel;
{
	struct buf *bp;
	int error;
	
	/* XXX should probably ensure that we don't try to do this if
	   someone has changed rf_protected_sectors. */ 

	if (b_vp == NULL) {
		/* For whatever reason, this component is not valid.
		   Don't try to read a component label from it. */
		return(EINVAL);
	}

	/* get a block of the appropriate size... */
	bp = geteblk((int)RF_COMPONENT_INFO_SIZE);
	bp->b_dev = dev;

	/* get our ducks in a row for the read */
	bp->b_blkno = RF_COMPONENT_INFO_OFFSET / DEV_BSIZE;
	bp->b_bcount = RF_COMPONENT_INFO_SIZE;
	bp->b_iocmd = BIO_READ;
 	bp->b_resid = RF_COMPONENT_INFO_SIZE / DEV_BSIZE;

	DEV_STRATEGY(bp, 0);
	error = bufwait(bp); 

	if (!error) {
		memcpy(clabel, bp->b_data, sizeof(RF_ComponentLabel_t));
#if 0
		rf_print_component_label( clabel );
#endif
        } else {
#if 0
		rf_printf(0, "Failed to read RAID component label!\n");
#endif
	}

	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp); 
	return(error);
}
/* ARGSUSED */
int 
raidwrite_component_label(dev, b_vp, clabel)
	dev_t dev; 
	struct vnode *b_vp;
	RF_ComponentLabel_t *clabel;
{
	struct buf *bp;
	int error;

	/* get a block of the appropriate size... */
	bp = geteblk((int)RF_COMPONENT_INFO_SIZE);
	bp->b_dev = dev;

	/* get our ducks in a row for the write */
	bp->b_flags = 0;
	bp->b_blkno = RF_COMPONENT_INFO_OFFSET / DEV_BSIZE;
	bp->b_bcount = RF_COMPONENT_INFO_SIZE;
	bp->b_iocmd = BIO_WRITE;
 	bp->b_resid = RF_COMPONENT_INFO_SIZE / DEV_BSIZE;

	memset(bp->b_data, 0, RF_COMPONENT_INFO_SIZE );

	memcpy(bp->b_data, clabel, sizeof(RF_ComponentLabel_t));

	DEV_STRATEGY(bp, 0);
	error = bufwait(bp); 

	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp);
	if (error) {
#if 1
		rf_printf(0, "Failed to write RAID component info!\n");
		rf_printf(0, "b_error= %d\n", bp->b_error);
#endif
	}

	return(error);
}

void 
rf_markalldirty(raidPtr)
	RF_Raid_t *raidPtr;
{
	RF_ComponentLabel_t *clabel;
	int r,c;

	MALLOC(clabel, RF_ComponentLabel_t *, sizeof(RF_ComponentLabel_t),
	    M_RAIDFRAME, M_NOWAIT | M_ZERO);

	if (clabel == NULL) {
		printf("rf_markalldirty: Out of memory?\n");
		return;
	}

	raidPtr->mod_counter++;
	for (r = 0; r < raidPtr->numRow; r++) {
		for (c = 0; c < raidPtr->numCol; c++) {
			/* we don't want to touch (at all) a disk that has
			   failed */
			if (!RF_DEAD_DISK(raidPtr->Disks[r][c].status)) {
				raidread_component_label(
					raidPtr->Disks[r][c].dev,
					raidPtr->raid_cinfo[r][c].ci_vp,
					clabel);
				if (clabel->status == rf_ds_spared) {
					/* XXX do something special... 
					 but whatever you do, don't 
					 try to access it!! */
				} else {
#if 0
				clabel->status = 
					raidPtr->Disks[r][c].status;
				raidwrite_component_label( 
					raidPtr->Disks[r][c].dev,
					raidPtr->raid_cinfo[r][c].ci_vp,
					clabel);
#endif
				raidmarkdirty( 
				       raidPtr->Disks[r][c].dev, 
				       raidPtr->raid_cinfo[r][c].ci_vp,
				       raidPtr->mod_counter);
				}
			}
		} 
	}
	/* printf("Component labels marked dirty.\n"); */
#if 0
	for( c = 0; c < raidPtr->numSpare ; c++) {
		sparecol = raidPtr->numCol + c;
		if (raidPtr->Disks[r][sparecol].status == rf_ds_used_spare) {
			/* 

			   XXX this is where we get fancy and map this spare
			   into it's correct spot in the array.

			 */
			/* 
			   
			   we claim this disk is "optimal" if it's 
			   rf_ds_used_spare, as that means it should be 
			   directly substitutable for the disk it replaced. 
			   We note that too...

			 */

			for(i=0;i<raidPtr->numRow;i++) {
				for(j=0;j<raidPtr->numCol;j++) {
					if ((raidPtr->Disks[i][j].spareRow == 
					     r) &&
					    (raidPtr->Disks[i][j].spareCol ==
					     sparecol)) {
						srow = r;
						scol = sparecol;
						break;
					}
				}
			}
			
			raidread_component_label( 
				      raidPtr->Disks[r][sparecol].dev,
				      raidPtr->raid_cinfo[r][sparecol].ci_vp,
				      &clabel);
			/* make sure status is noted */
			clabel.version = RF_COMPONENT_LABEL_VERSION; 
			clabel.mod_counter = raidPtr->mod_counter;
			clabel.serial_number = raidPtr->serial_number;
			clabel.row = srow;
			clabel.column = scol;
			clabel.num_rows = raidPtr->numRow;
			clabel.num_columns = raidPtr->numCol;
			clabel.clean = RF_RAID_DIRTY; /* changed in a bit*/
			clabel.status = rf_ds_optimal;
			raidwrite_component_label(
				      raidPtr->Disks[r][sparecol].dev,
				      raidPtr->raid_cinfo[r][sparecol].ci_vp,
				      &clabel);
			raidmarkclean( raidPtr->Disks[r][sparecol].dev, 
			              raidPtr->raid_cinfo[r][sparecol].ci_vp);
		}
	}

#endif
	FREE(clabel, M_RAIDFRAME);
}


void
rf_update_component_labels(raidPtr, final)
	RF_Raid_t *raidPtr;
	int final;
{
	RF_ComponentLabel_t *clabel;
	int sparecol;
	int r,c;
	int i,j;
	int srow, scol;

	srow = -1;
	scol = -1;

	MALLOC(clabel, RF_ComponentLabel_t *, sizeof(RF_ComponentLabel_t),
	    M_RAIDFRAME, M_NOWAIT | M_ZERO);
	if (clabel == NULL) {
		printf("rf_update_component_labels: Out of memory?\n");
		return;
	}

	/* XXX should do extra checks to make sure things really are clean, 
	   rather than blindly setting the clean bit... */

	raidPtr->mod_counter++;

	for (r = 0; r < raidPtr->numRow; r++) {
		for (c = 0; c < raidPtr->numCol; c++) {
			if (raidPtr->Disks[r][c].status == rf_ds_optimal) {
				raidread_component_label(
					raidPtr->Disks[r][c].dev,
					raidPtr->raid_cinfo[r][c].ci_vp,
					clabel);
				/* make sure status is noted */
				clabel->status = rf_ds_optimal;
				/* bump the counter */
				clabel->mod_counter = raidPtr->mod_counter;

				raidwrite_component_label( 
					raidPtr->Disks[r][c].dev,
					raidPtr->raid_cinfo[r][c].ci_vp,
					clabel);
				if (final == RF_FINAL_COMPONENT_UPDATE) {
					if (raidPtr->parity_good == RF_RAID_CLEAN) {
						raidmarkclean( 
							      raidPtr->Disks[r][c].dev, 
							      raidPtr->raid_cinfo[r][c].ci_vp,
							      raidPtr->mod_counter);
					}
				}
			} 
			/* else we don't touch it.. */
		} 
	}

	for( c = 0; c < raidPtr->numSpare ; c++) {
		sparecol = raidPtr->numCol + c;
		if (raidPtr->Disks[0][sparecol].status == rf_ds_used_spare) {
			/* 
			   
			   we claim this disk is "optimal" if it's 
			   rf_ds_used_spare, as that means it should be 
			   directly substitutable for the disk it replaced. 
			   We note that too...

			 */

			for(i=0;i<raidPtr->numRow;i++) {
				for(j=0;j<raidPtr->numCol;j++) {
					if ((raidPtr->Disks[i][j].spareRow == 
					     0) &&
					    (raidPtr->Disks[i][j].spareCol ==
					     sparecol)) {
						srow = i;
						scol = j;
						break;
					}
				}
			}
			
			/* XXX shouldn't *really* need this... */
			raidread_component_label( 
				      raidPtr->Disks[0][sparecol].dev,
				      raidPtr->raid_cinfo[0][sparecol].ci_vp,
				      clabel);
			/* make sure status is noted */

			raid_init_component_label(raidPtr, clabel);

			clabel->mod_counter = raidPtr->mod_counter;
			clabel->row = srow;
			clabel->column = scol;
			clabel->status = rf_ds_optimal;

			raidwrite_component_label(
				      raidPtr->Disks[0][sparecol].dev,
				      raidPtr->raid_cinfo[0][sparecol].ci_vp,
				      clabel);
			if (final == RF_FINAL_COMPONENT_UPDATE) {
				if (raidPtr->parity_good == RF_RAID_CLEAN) {
					raidmarkclean( raidPtr->Disks[0][sparecol].dev,
						       raidPtr->raid_cinfo[0][sparecol].ci_vp,
						       raidPtr->mod_counter);
				}
			}
		}
	}
	FREE(clabel, M_RAIDFRAME);
	rf_printf(1, "Component labels updated\n");
}

void
rf_close_component(raidPtr, vp, auto_configured)
	RF_Raid_t *raidPtr;
	struct vnode *vp;
	int auto_configured;
{
	struct thread *td;

	td = raidPtr->engine_thread;

	if (vp != NULL) {
		if (auto_configured == 1) {
			VOP_CLOSE(vp, FREAD | FWRITE, td->td_ucred, td);
			
			vrele(vp);
		} else {				
			vn_close(vp, FREAD | FWRITE, td->td_ucred, td);
		}
	} else {
		rf_printf(1, "vnode was NULL\n");
	}
}


void
rf_UnconfigureVnodes(raidPtr)
	RF_Raid_t *raidPtr;
{
	int r,c; 
	struct thread *td;
	struct vnode *vp;
	int acd;


	/* We take this opportunity to close the vnodes like we should.. */

	td = raidPtr->engine_thread;

	for (r = 0; r < raidPtr->numRow; r++) {
		for (c = 0; c < raidPtr->numCol; c++) {
			rf_printf(1, "Closing vnode for row: %d col: %d\n", r, c);
			vp = raidPtr->raid_cinfo[r][c].ci_vp;
			acd = raidPtr->Disks[r][c].auto_configured;
			rf_close_component(raidPtr, vp, acd);
			raidPtr->raid_cinfo[r][c].ci_vp = NULL;
			raidPtr->Disks[r][c].auto_configured = 0;
		}
	}
	for (r = 0; r < raidPtr->numSpare; r++) {
		rf_printf(1, "Closing vnode for spare: %d\n", r);
		vp = raidPtr->raid_cinfo[0][raidPtr->numCol + r].ci_vp;
		acd = raidPtr->Disks[0][raidPtr->numCol + r].auto_configured;
		rf_close_component(raidPtr, vp, acd);
		raidPtr->raid_cinfo[0][raidPtr->numCol + r].ci_vp = NULL;
		raidPtr->Disks[0][raidPtr->numCol + r].auto_configured = 0;
	}
}


void 
rf_ReconThread(req)
	struct rf_recon_req *req;
{
	RF_Raid_t *raidPtr;

	mtx_lock(&Giant);
	raidPtr = (RF_Raid_t *) req->raidPtr;
	raidPtr->recon_in_progress = 1;

	rf_FailDisk((RF_Raid_t *) req->raidPtr, req->row, req->col,
		    ((req->flags & RF_FDFLAGS_RECON) ? 1 : 0));

	/* XXX get rid of this! we don't need it at all.. */
	RF_Free(req, sizeof(*req));

	raidPtr->recon_in_progress = 0;

	/* That's all... */
	RF_THREAD_EXIT(0);        /* does not return */
}

void
rf_RewriteParityThread(raidPtr)
	RF_Raid_t *raidPtr;
{
	int retcode;

	mtx_lock(&Giant);
	raidPtr->parity_rewrite_in_progress = 1;
	retcode = rf_RewriteParity(raidPtr);
	if (retcode) {
		rf_printf(0, "raid%d: Error re-writing parity!\n",raidPtr->raidid);
	} else {
		/* set the clean bit!  If we shutdown correctly,
		   the clean bit on each component label will get
		   set */
		raidPtr->parity_good = RF_RAID_CLEAN;
	}
	raidPtr->parity_rewrite_in_progress = 0;

	/* Anyone waiting for us to stop?  If so, inform them... */
	if (raidPtr->waitShutdown) {
		wakeup(&raidPtr->parity_rewrite_in_progress);
	}

	/* That's all... */
	RF_THREAD_EXIT(0);        /* does not return */
}


void
rf_CopybackThread(raidPtr)
	RF_Raid_t *raidPtr;
{
	mtx_lock(&Giant);
	raidPtr->copyback_in_progress = 1;
	rf_CopybackReconstructedData(raidPtr);
	raidPtr->copyback_in_progress = 0;

	/* That's all... */
	RF_THREAD_EXIT(0);        /* does not return */
}


void
rf_ReconstructInPlaceThread(req)
	struct rf_recon_req *req;
{
	int retcode;
	RF_Raid_t *raidPtr;
	
	mtx_lock(&Giant);
	raidPtr = req->raidPtr;
	raidPtr->recon_in_progress = 1;
	retcode = rf_ReconstructInPlace(raidPtr, req->row, req->col);
	RF_Free(req, sizeof(*req));
	raidPtr->recon_in_progress = 0;

	/* That's all... */
	RF_THREAD_EXIT(0);        /* does not return */
}

RF_AutoConfig_t *
rf_find_raid_components()
{
	RF_AutoConfig_t *ac_list = NULL;
#if 0 /* XXX GEOM */
	struct vnode *vp;
	struct disklabel *label;
	struct diskslice *slice;
	struct diskslices *slices;
	struct disk *disk;
	struct thread *td;
	dev_t dev;
	char *devname;
	int error, j;
	int nslices;

	td = curthread;

	MALLOC(label, struct disklabel *, sizeof(struct disklabel),
	    M_RAIDFRAME, M_NOWAIT|M_ZERO);
	MALLOC(slices, struct diskslices *, sizeof(struct diskslices),
	    M_RAIDFRAME, M_NOWAIT|M_ZERO);
	if ((label == NULL) || (slices == NULL)) {
		printf("rf_find_raid_components: Out of Memory?\n");
		return (NULL);
	}

	/* initialize the AutoConfig list */
	ac_list = NULL;

	/* we begin by trolling through *all* the disk devices on the system */

	disk = NULL;
	while ((disk = disk_enumerate(disk))) {

		/* we don't care about floppies... */
		devname = disk->d_dev->si_name;
		if (!strncmp(devname, "fd", 2) ||
		    !strncmp(devname, "cd", 2) ||
		    !strncmp(devname, "acd", 3))
			continue;

		rf_printf(1, "Examining %s\n", disk->d_dev->si_name);
		if (bdevvp(disk->d_dev, &vp))
			panic("RAIDframe can't alloc vnode");
		vref(vp);

		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		error = VOP_OPEN(vp, FREAD, td->td_ucred, td);
		VOP_UNLOCK(vp, 0, td);
		if (error) {
			vput(vp);
			continue;
		}

		error = VOP_IOCTL(vp, DIOCGSLICEINFO, (caddr_t)slices,
		    FREAD, td->td_ucred, td);
		VOP_CLOSE(vp, FREAD | FWRITE, td->td_ucred, td);
		vrele(vp);
		if (error) {
			/* No slice table. */
			continue;
		}

		nslices = slices->dss_nslices;
		if ((nslices == 0) || (nslices > MAX_SLICES))
			continue;

		/* Iterate through the slices */
		for (j = 1; j < nslices; j++) {

			rf_printf(1, "Examining slice %d\n", j);
			slice = &slices->dss_slices[j - 1];
			dev = dkmodslice(disk->d_dev, j);
			if (bdevvp(dev, &vp))
				panic("RAIDframe can't alloc vnode");

			vref(vp);
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
			error = VOP_OPEN(vp, FREAD, td->td_ucred, td);
			VOP_UNLOCK(vp, 0, td);
			if (error) {
				continue;
			}

			error = VOP_IOCTL(vp, DIOCGDINFO, (caddr_t)label,
			    FREAD, td->td_ucred, td);
			VOP_CLOSE(vp, FREAD | FWRITE, td->td_ucred, td);
			vrele(vp);
			if (error)
				continue;

			rf_search_label(dev, label, &ac_list);
		}
	}

	FREE(label, M_RAIDFRAME);
	FREE(slices, M_RAIDFRAME);
#endif
	return (ac_list);
}

static void
rf_search_label(dev_t dev, struct disklabel *label, RF_AutoConfig_t **ac_list)
{
	RF_AutoConfig_t *ac;
	RF_ComponentLabel_t *clabel;
	struct vnode *vp;
	struct thread *td;
	dev_t dev1;
	int i, error, good_one;

	td = curthread;

	/* Iterate through the partitions */
	for (i=0; i < label->d_npartitions; i++) {
		/* We only support partitions marked as RAID */
		if (label->d_partitions[i].p_fstype != FS_RAID)
			continue;

		dev1 = dkmodpart(dev, i);
		if (dev1 == NULL) {
			rf_printf(1, "dev1 == null\n");
			continue;
		}
		if (bdevvp(dev1, &vp))
			panic("RAIDframe can't alloc vnode");

		vref(vp);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		error = VOP_OPEN(vp, FREAD, td->td_ucred, td);
		VOP_UNLOCK(vp, 0, td);
		if (error) {
			/* Whatever... */
			continue;
		}

		good_one = 0;

		clabel = (RF_ComponentLabel_t *) 
			malloc(sizeof(RF_ComponentLabel_t), M_RAIDFRAME,
			       M_NOWAIT);
		if (clabel == NULL) {
			/* XXX CLEANUP HERE */
			panic("RAID autoconfig: no memory!\n");
		}

		if (!raidread_component_label(dev1, vp, clabel)) {
			/* Got the label.  Is it reasonable? */
			if (rf_reasonable_label(clabel) &&
			    (clabel->partitionSize <= 
			     label->d_partitions[i].p_size)) {
				rf_printf(1, "Component on: %s: %d\n",
				    dev1->si_name, label->d_partitions[i].p_size);
				rf_print_component_label(clabel);
				/* if it's reasonable, add it, else ignore it */
				ac = (RF_AutoConfig_t *)
					malloc(sizeof(RF_AutoConfig_t),
					       M_RAIDFRAME, M_NOWAIT);
				if (ac == NULL) {
					/* XXX should panic? */
					panic("RAID autoconfig: no memory!\n");
				}
			
				sprintf(ac->devname, "%s", dev->si_name);
				ac->dev = dev1;
				ac->vp = vp;
				ac->clabel = clabel;
				ac->next = *ac_list;
				*ac_list = ac;
				good_one = 1;
			} 
		}
		if (!good_one) {
			/* cleanup */
			free(clabel, M_RAIDFRAME);
			VOP_CLOSE(vp, FREAD | FWRITE, td->td_ucred, td);
			vrele(vp);
		}
	}
}

static int
rf_reasonable_label(clabel)
	RF_ComponentLabel_t *clabel;
{
	
	if (((clabel->version==RF_COMPONENT_LABEL_VERSION_1) ||
	     (clabel->version==RF_COMPONENT_LABEL_VERSION)) &&
	    ((clabel->clean == RF_RAID_CLEAN) ||
	     (clabel->clean == RF_RAID_DIRTY)) &&
	    clabel->row >=0 && 
	    clabel->column >= 0 && 
	    clabel->num_rows > 0 &&
	    clabel->num_columns > 0 &&
	    clabel->row < clabel->num_rows && 
	    clabel->column < clabel->num_columns &&
	    clabel->blockSize > 0 &&
	    clabel->numBlocks > 0) {
		/* label looks reasonable enough... */
		return(1);
	}
	return(0);
}


void
rf_print_component_label(clabel)
	RF_ComponentLabel_t *clabel;
{
	rf_printf(1, "   Row: %d Column: %d Num Rows: %d Num Columns: %d\n",
	       clabel->row, clabel->column, 
	       clabel->num_rows, clabel->num_columns);
	rf_printf(1, "   Version: %d Serial Number: %d Mod Counter: %d\n",
	       clabel->version, clabel->serial_number,
	       clabel->mod_counter);
	rf_printf(1, "   Clean: %s Status: %d\n",
	       clabel->clean ? "Yes" : "No", clabel->status );
	rf_printf(1, "   sectPerSU: %d SUsPerPU: %d SUsPerRU: %d\n",
	       clabel->sectPerSU, clabel->SUsPerPU, clabel->SUsPerRU);
	rf_printf(1, "   RAID Level: %c  blocksize: %d numBlocks: %d\n",
	       (char) clabel->parityConfig, clabel->blockSize, 
	       clabel->numBlocks);
	rf_printf(1, "   Autoconfig: %s\n", clabel->autoconfigure ? "Yes":"No");
	rf_printf(1, "   Contains root partition: %s\n",  
	       clabel->root_partition ? "Yes" : "No" );
	rf_printf(1, "   Last configured as: raid%d\n", clabel->last_unit );
#if 0
	rf_printf(1, "   Config order: %d\n", clabel->config_order);
#endif
	       
}

RF_ConfigSet_t *
rf_create_auto_sets(ac_list)
	RF_AutoConfig_t *ac_list;
{
	RF_AutoConfig_t *ac;
	RF_ConfigSet_t *config_sets;
	RF_ConfigSet_t *cset;
	RF_AutoConfig_t *ac_next;


	config_sets = NULL;

	/* Go through the AutoConfig list, and figure out which components
	   belong to what sets.  */
	ac = ac_list;
	while(ac!=NULL) {
		/* we're going to putz with ac->next, so save it here
		   for use at the end of the loop */
		ac_next = ac->next;

		if (config_sets == NULL) {
			/* will need at least this one... */
			config_sets = (RF_ConfigSet_t *)
				malloc(sizeof(RF_ConfigSet_t), 
				       M_RAIDFRAME, M_NOWAIT);
			if (config_sets == NULL) {
				panic("rf_create_auto_sets: No memory!\n");
			}
			/* this one is easy :) */
			config_sets->ac = ac;
			config_sets->next = NULL;
			config_sets->rootable = 0;
			ac->next = NULL;
		} else {
			/* which set does this component fit into? */
			cset = config_sets;
			while(cset!=NULL) {
				if (rf_does_it_fit(cset, ac)) {
					/* looks like it matches... */
					ac->next = cset->ac;
					cset->ac = ac;
					break;
				}
				cset = cset->next;
			}
			if (cset==NULL) {
				/* didn't find a match above... new set..*/
				cset = (RF_ConfigSet_t *)
					malloc(sizeof(RF_ConfigSet_t), 
					       M_RAIDFRAME, M_NOWAIT);
				if (cset == NULL) {
					panic("rf_create_auto_sets: No memory!\n");
				}
				cset->ac = ac;
				ac->next = NULL;
				cset->next = config_sets;
				cset->rootable = 0;
				config_sets = cset;
			}
		}
		ac = ac_next;
	}


	return(config_sets);
}

static int
rf_does_it_fit(cset, ac)	
	RF_ConfigSet_t *cset;
	RF_AutoConfig_t *ac;
{
	RF_ComponentLabel_t *clabel1, *clabel2;

	/* If this one matches the *first* one in the set, that's good
	   enough, since the other members of the set would have been
	   through here too... */
	/* note that we are not checking partitionSize here..

	   Note that we are also not checking the mod_counters here.
	   If everything else matches execpt the mod_counter, that's 
	   good enough for this test.  We will deal with the mod_counters
	   a little later in the autoconfiguration process.  

	    (clabel1->mod_counter == clabel2->mod_counter) &&

	   The reason we don't check for this is that failed disks
	   will have lower modification counts.  If those disks are
	   not added to the set they used to belong to, then they will
	   form their own set, which may result in 2 different sets,
	   for example, competing to be configured at raid0, and
	   perhaps competing to be the root filesystem set.  If the
	   wrong ones get configured, or both attempt to become /,
	   weird behaviour and or serious lossage will occur.  Thus we
	   need to bring them into the fold here, and kick them out at
	   a later point.

	*/

	clabel1 = cset->ac->clabel;
	clabel2 = ac->clabel;
	if ((clabel1->version == clabel2->version) &&
	    (clabel1->serial_number == clabel2->serial_number) &&
	    (clabel1->num_rows == clabel2->num_rows) &&
	    (clabel1->num_columns == clabel2->num_columns) &&
	    (clabel1->sectPerSU == clabel2->sectPerSU) &&
	    (clabel1->SUsPerPU == clabel2->SUsPerPU) &&
	    (clabel1->SUsPerRU == clabel2->SUsPerRU) &&
	    (clabel1->parityConfig == clabel2->parityConfig) &&
	    (clabel1->maxOutstanding == clabel2->maxOutstanding) &&
	    (clabel1->blockSize == clabel2->blockSize) &&
	    (clabel1->numBlocks == clabel2->numBlocks) &&
	    (clabel1->autoconfigure == clabel2->autoconfigure) &&
	    (clabel1->root_partition == clabel2->root_partition) &&
	    (clabel1->last_unit == clabel2->last_unit) &&
	    (clabel1->config_order == clabel2->config_order)) {
		/* if it get's here, it almost *has* to be a match */
	} else {
		/* it's not consistent with somebody in the set.. 
		   punt */
		return(0);
	}
	/* all was fine.. it must fit... */
	return(1);
}

int
rf_have_enough_components(cset)
	RF_ConfigSet_t *cset;
{
	RF_AutoConfig_t *ac;
	RF_AutoConfig_t *auto_config;
	RF_ComponentLabel_t *clabel;
	int r,c;
	int num_rows;
	int num_cols;
	int num_missing;
	int mod_counter;
	int mod_counter_found;
	int even_pair_failed;
	char parity_type;
	

	/* check to see that we have enough 'live' components
	   of this set.  If so, we can configure it if necessary */

	num_rows = cset->ac->clabel->num_rows;
	num_cols = cset->ac->clabel->num_columns;
	parity_type = cset->ac->clabel->parityConfig;

	/* XXX Check for duplicate components!?!?!? */

	/* Determine what the mod_counter is supposed to be for this set. */

	mod_counter_found = 0;
	mod_counter = 0;
	ac = cset->ac;
	while(ac!=NULL) {
		if (mod_counter_found==0) {
			mod_counter = ac->clabel->mod_counter;
			mod_counter_found = 1;
		} else {
			if (ac->clabel->mod_counter > mod_counter) {
				mod_counter = ac->clabel->mod_counter;
			}
		}
		ac = ac->next;
	}

	num_missing = 0;
	auto_config = cset->ac;

	for(r=0; r<num_rows; r++) {
		even_pair_failed = 0;
		for(c=0; c<num_cols; c++) {
			ac = auto_config;
			while(ac!=NULL) {
				if ((ac->clabel->row == r) &&
				    (ac->clabel->column == c) && 
				    (ac->clabel->mod_counter == mod_counter)) {
					/* it's this one... */
					rf_printf(1, "Found: %s at %d,%d\n",
					       ac->devname,r,c);
					break;
				}
				ac=ac->next;
			}
			if (ac==NULL) {
				/* Didn't find one here! */
				/* special case for RAID 1, especially
				   where there are more than 2
				   components (where RAIDframe treats
				   things a little differently :( ) */
				if (parity_type == '1') {
					if (c%2 == 0) { /* even component */
						even_pair_failed = 1;
					} else { /* odd component.  If
                                                    we're failed, and
                                                    so is the even
                                                    component, it's
                                                    "Good Night, Charlie" */
						if (even_pair_failed == 1) {
							return(0);
						}
					}
				} else {
					/* normal accounting */
					num_missing++;
				}
			}
			if ((parity_type == '1') && (c%2 == 1)) {
				/* Just did an even component, and we didn't
				   bail.. reset the even_pair_failed flag, 
				   and go on to the next component.... */
				even_pair_failed = 0;
			}
		}
	}

	clabel = cset->ac->clabel;

	if (((clabel->parityConfig == '0') && (num_missing > 0)) ||
	    ((clabel->parityConfig == '4') && (num_missing > 1)) ||
	    ((clabel->parityConfig == '5') && (num_missing > 1))) {
		/* XXX this needs to be made *much* more general */
		/* Too many failures */
		return(0);
	}
	/* otherwise, all is well, and we've got enough to take a kick
	   at autoconfiguring this set */
	return(1);
}

void
rf_create_configuration(ac,config,raidPtr)
	RF_AutoConfig_t *ac;
	RF_Config_t *config;
	RF_Raid_t *raidPtr;
{
	RF_ComponentLabel_t *clabel;
	int i;

	clabel = ac->clabel;

	/* 1. Fill in the common stuff */
	config->numRow = clabel->num_rows;
	config->numCol = clabel->num_columns;
	config->numSpare = 0; /* XXX should this be set here? */
	config->sectPerSU = clabel->sectPerSU;
	config->SUsPerPU = clabel->SUsPerPU;
	config->SUsPerRU = clabel->SUsPerRU;
	config->parityConfig = clabel->parityConfig;
	/* XXX... */
	strcpy(config->diskQueueType,"fifo");
	config->maxOutstandingDiskReqs = clabel->maxOutstanding;
	config->layoutSpecificSize = 0; /* XXX ? */

	while(ac!=NULL) {
		/* row/col values will be in range due to the checks
		   in reasonable_label() */
		strcpy(config->devnames[ac->clabel->row][ac->clabel->column],
		       ac->devname);
		ac = ac->next;
	}

	for(i=0;i<RF_MAXDBGV;i++) {
		config->debugVars[i][0] = NULL;
	}
}

int
rf_set_autoconfig(raidPtr, new_value)
	RF_Raid_t *raidPtr;
	int new_value;
{
	RF_ComponentLabel_t *clabel;
	struct vnode *vp;
	dev_t dev;
	int row, column;

	MALLOC(clabel, RF_ComponentLabel_t *, sizeof(RF_ComponentLabel_t),
	    M_RAIDFRAME, M_WAITOK | M_ZERO);

	raidPtr->autoconfigure = new_value;
	for(row=0; row<raidPtr->numRow; row++) {
		for(column=0; column<raidPtr->numCol; column++) {
			if (raidPtr->Disks[row][column].status == 
			    rf_ds_optimal) {
				dev = raidPtr->Disks[row][column].dev;
				vp = raidPtr->raid_cinfo[row][column].ci_vp;
				raidread_component_label(dev, vp, clabel);
				clabel->autoconfigure = new_value;
				raidwrite_component_label(dev, vp, clabel);
			}
		}
	}
	FREE(clabel, M_RAIDFRAME);
	return(new_value);
}

int
rf_set_rootpartition(raidPtr, new_value)
	RF_Raid_t *raidPtr;
	int new_value;
{
	RF_ComponentLabel_t *clabel;
	struct vnode *vp;
	dev_t dev;
	int row, column;

	MALLOC(clabel, RF_ComponentLabel_t *, sizeof(RF_ComponentLabel_t),
	    M_RAIDFRAME, M_WAITOK | M_ZERO);

	raidPtr->root_partition = new_value;
	for(row=0; row<raidPtr->numRow; row++) {
		for(column=0; column<raidPtr->numCol; column++) {
			if (raidPtr->Disks[row][column].status == 
			    rf_ds_optimal) {
				dev = raidPtr->Disks[row][column].dev;
				vp = raidPtr->raid_cinfo[row][column].ci_vp;
				raidread_component_label(dev, vp, clabel);
				clabel->root_partition = new_value;
				raidwrite_component_label(dev, vp, clabel);
			}
		}
	}
	FREE(clabel, M_RAIDFRAME);
	return(new_value);
}

void
rf_release_all_vps(cset)
	RF_ConfigSet_t *cset;
{
	RF_AutoConfig_t *ac;
	struct thread *td;

	td = curthread;
	ac = cset->ac;
	while(ac!=NULL) {
		/* Close the vp, and give it back */
		if (ac->vp) {
			VOP_CLOSE(ac->vp, FREAD, td->td_ucred, td);
			vrele(ac->vp);
			ac->vp = NULL;
		}
		ac = ac->next;
	}
}


void
rf_cleanup_config_set(cset)
	RF_ConfigSet_t *cset;
{
	RF_AutoConfig_t *ac;
	RF_AutoConfig_t *next_ac;
	
	ac = cset->ac;
	while(ac!=NULL) {
		next_ac = ac->next;
		/* nuke the label */
		free(ac->clabel, M_RAIDFRAME);
		/* cleanup the config structure */
		free(ac, M_RAIDFRAME);
		/* "next.." */
		ac = next_ac;
	}
	/* and, finally, nuke the config set */
	free(cset, M_RAIDFRAME);
}


void
raid_init_component_label(raidPtr, clabel)
	RF_Raid_t *raidPtr;
	RF_ComponentLabel_t *clabel;
{
	/* current version number */
	clabel->version = RF_COMPONENT_LABEL_VERSION; 
	clabel->serial_number = raidPtr->serial_number;
	clabel->mod_counter = raidPtr->mod_counter;
	clabel->num_rows = raidPtr->numRow;
	clabel->num_columns = raidPtr->numCol;
	clabel->clean = RF_RAID_DIRTY; /* not clean */
	clabel->status = rf_ds_optimal; /* "It's good!" */
	
	clabel->sectPerSU = raidPtr->Layout.sectorsPerStripeUnit;
	clabel->SUsPerPU = raidPtr->Layout.SUsPerPU;
	clabel->SUsPerRU = raidPtr->Layout.SUsPerRU;

	clabel->blockSize = raidPtr->bytesPerSector;
	clabel->numBlocks = raidPtr->sectorsPerDisk;

	/* XXX not portable */
	clabel->parityConfig = raidPtr->Layout.map->parityConfig;
	clabel->maxOutstanding = raidPtr->maxOutstanding;
	clabel->autoconfigure = raidPtr->autoconfigure;
	clabel->root_partition = raidPtr->root_partition;
	clabel->last_unit = raidPtr->raidid;
	clabel->config_order = raidPtr->config_order;
}

int
rf_auto_config_set(cset, unit, parent_sc)
	RF_ConfigSet_t *cset;
	int *unit;
	struct raidctl_softc *parent_sc;
{
	int retcode = 0;
	RF_Raid_t *raidPtr;
	RF_Config_t *config;
	int raidID;

	rf_printf(0, "RAIDframe autoconfigure\n");

	*unit = -1;

	/* 1. Create a config structure */

	config = (RF_Config_t *)malloc(sizeof(RF_Config_t), M_RAIDFRAME,
				       M_NOWAIT|M_ZERO);
	if (config==NULL) {
		rf_printf(0, "Out of mem at rf_auto_config_set\n");
				/* XXX do something more intelligent here. */
		return(1);
	}

	/* XXX raidID needs to be set correctly.. */

	/* 
	   2. Figure out what RAID ID this one is supposed to live at 
	   See if we can get the same RAID dev that it was configured
	   on last time.. 
	*/

	raidID = cset->ac->clabel->last_unit;
	if (raidID < 0) {
		/* let's not wander off into lala land. */
		raidID = raidgetunit(parent_sc, 0);
	} else {
		raidID = raidgetunit(parent_sc, raidID);
	}

	if (raidID < 0) {
		/* punt... */
		rf_printf(0, "Unable to auto configure this set!\n");
		rf_printf(1, "Out of RAID devs!\n");
		return(1);
	}
	rf_printf(0, "Configuring raid%d:\n",raidID);
	RF_Malloc(raidPtr, sizeof(*raidPtr), (RF_Raid_t *));
	if (raidPtr == NULL) {
		rf_printf(0, "Out of mem at rf_auto_config_set\n");
		return (1);
	}
	bzero((char *)raidPtr, sizeof(RF_Raid_t));

	/* XXX all this stuff should be done SOMEWHERE ELSE! */
	raidPtr->raidid = raidID;
	raidPtr->openings = RAIDOUTSTANDING;

	/* 3. Build the configuration structure */
	rf_create_configuration(cset->ac, config, raidPtr);

	/* 4. Do the configuration */
	retcode = rf_Configure(raidPtr, config, cset->ac);
	
	if (retcode == 0) {

		parent_sc->sc_raiddevs[raidID] = raidinit(raidPtr);
		if (parent_sc->sc_raiddevs[raidID] == NULL) {
			rf_printf(0, "Could not create RAID device\n");
			RF_Free(raidPtr, sizeof(RF_Raid_t));
			free(config, M_RAIDFRAME);
			return (1);
		}

		parent_sc->sc_numraid++;
		((struct raid_softc *)raidPtr->sc)->sc_parent_dev =
		    parent_sc->sc_dev;
		rf_markalldirty(raidPtr);
		raidPtr->autoconfigure = 1; /* XXX do this here? */
		if (cset->ac->clabel->root_partition==1) {
			/* everything configured just fine.  Make a note
			   that this set is eligible to be root. */
			cset->rootable = 1;
			/* XXX do this here? */
			raidPtr->root_partition = 1; 
		}
	}

	/* 5. Cleanup */
	free(config, M_RAIDFRAME);
	
	*unit = raidID;
	return(retcode);
}

void
rf_disk_unbusy(desc)
	RF_RaidAccessDesc_t *desc;
{
	struct raid_softc *sc;
	struct bio *bp;

	sc = desc->raidPtr->sc;
	bp = (struct bio *)desc->bp;

	devstat_end_transaction_bio(&sc->device_stats, bp);
}

/*
 * Get the next available unit number from the bitmap.  You can also request
 * a particular unit number by passing it in the second arg.  If it's not
 * available, then grab the next free one.  Return -1 if none are available.
 */
static int
raidgetunit(struct raidctl_softc *parent_sc, int id)
{
	int i;

	if (id >= RF_MAX_ARRAYS)
		return (-1);

	for (i = id; i < RF_MAX_ARRAYS; i++) {
		if (parent_sc->sc_raiddevs[i] == NULL)
			return (i);
	}

	if (id != 0) {
		for (i = 0; i < id; i++) {
			if (parent_sc->sc_raiddevs[i] == NULL)
				return (i);
		}
	}

	return (-1);
}

static int
raidshutdown(void)
{
	struct raidctl_softc *parent_sc;
	int i, error = 0;

	parent_sc = raidctl_dev->si_drv1;

	if (parent_sc->sc_numraid != 0) {
#if XXX_KTHREAD_EXIT_RACE
		return (EBUSY);
#else
		for (i = 0; i < RF_MAX_ARRAYS; i++) {
			if (parent_sc->sc_raiddevs[i] != NULL) {
				rf_printf(0, "Shutting down raid%d\n", i);
				error = raidctlioctl(raidctl_dev,
				    RAIDFRAME_SHUTDOWN, (caddr_t)&i, 0, NULL);
				if (error)
					return (error);
				if (parent_sc->sc_numraid == 0)
					break;
			}
		}
#endif
	}

	destroy_dev(raidctl_dev);

	return (error);
}

int
raid_getcomponentsize(RF_Raid_t *raidPtr, RF_RowCol_t row, RF_RowCol_t col)
{
	struct vnode *vp;
	struct vattr va;
	RF_Thread_t td;
	off_t mediasize;
	u_int secsize;
	int retcode;

	td = raidPtr->engine_thread;

	retcode = raidlookup(raidPtr->Disks[row][col].devname, td, &vp);

	if (retcode) {
		printf("raid%d: rebuilding: raidlookup on device: %s failed: %d!\n",raidPtr->raidid,
		       raidPtr->Disks[row][col].devname, retcode);

		/* XXX the component isn't responding properly... 
		   must be still dead :-( */
		raidPtr->reconInProgress--;
		return(retcode);

	} else {

		/* Ok, so we can at least do a lookup... 
		   How about actually getting a vp for it? */

		if ((retcode = VOP_GETATTR(vp, &va, rf_getucred(td),
					   td)) != 0) {
			raidPtr->reconInProgress--;
			return(retcode);
		}
		
		retcode = VOP_IOCTL(vp, DIOCGSECTORSIZE, (caddr_t)&secsize,
		    FREAD, rf_getucred(td), td);
		if (retcode)
			return (retcode);
		raidPtr->Disks[row][col].blockSize = secsize;

		retcode = VOP_IOCTL(vp, DIOCGMEDIASIZE, (caddr_t)&mediasize,
		    FREAD, rf_getucred(td), td);
		if (retcode)
			return (retcode);
		raidPtr->Disks[row][col].numBlocks = mediasize / secsize;

		raidPtr->raid_cinfo[row][col].ci_vp = vp;
		raidPtr->raid_cinfo[row][col].ci_dev = udev2dev(va.va_rdev, 0);
		raidPtr->Disks[row][col].dev = udev2dev(va.va_rdev, 0);
		
		/* we allow the user to specify that only a 
		   fraction of the disks should be used this is 
		   just for debug:  it speeds up
		 * the parity scan */
		raidPtr->Disks[row][col].numBlocks =
			raidPtr->Disks[row][col].numBlocks *
			rf_sizePercentage / 100;
	}

	return(retcode);
}

static int
raid_modevent(mod, type, data)
	module_t mod;
	int type;
	void *data;
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		raidattach();
		break;

	case MOD_UNLOAD:
	case MOD_SHUTDOWN:
		error = raidshutdown();
		break;

	default:
		break;
	}

	return (error);
}

moduledata_t raid_mod = {
	"raidframe",
	(modeventhand_t) raid_modevent,
	0};

DECLARE_MODULE(raidframe, raid_mod, SI_SUB_RAID, SI_ORDER_MIDDLE);
