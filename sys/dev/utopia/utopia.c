/*-
 * Copyright (c) 2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
 *
 * Author: Hartmut Brandt <harti@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/unistd.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/if_atm.h>

#include <dev/utopia/suni.h>
#include <dev/utopia/idtphy.h>
#include <dev/utopia/utopia.h>
#include <dev/utopia/utopia_priv.h>

/* known chips */
extern const struct utopia_chip utopia_chip_idt77155;
extern const struct utopia_chip utopia_chip_idt77105;
extern const struct utopia_chip utopia_chip_lite;
extern const struct utopia_chip utopia_chip_ultra;
extern const struct utopia_chip utopia_chip_622;

/*
 * Global list of all registered interfaces
 */
static struct mtx utopia_list_mtx;
static LIST_HEAD(, utopia) utopia_list = LIST_HEAD_INITIALIZER(utopia_list);

#define UTP_RLOCK_LIST()	mtx_lock(&utopia_list_mtx)
#define UTP_RUNLOCK_LIST()	mtx_unlock(&utopia_list_mtx)
#define UTP_WLOCK_LIST()	mtx_lock(&utopia_list_mtx)
#define UTP_WUNLOCK_LIST()	mtx_unlock(&utopia_list_mtx)

#define UTP_LOCK(UTP)		mtx_lock((UTP)->lock)
#define UTP_UNLOCK(UTP)		mtx_unlock((UTP)->lock)
#define UTP_LOCK_ASSERT(UTP)	mtx_assert((UTP)->lock, MA_OWNED)

static struct proc *utopia_kproc;

static void utopia_dump(struct utopia *) __unused;

/*
 * Read a multi-register value.
 */
uint32_t
utopia_update(struct utopia *utp, u_int reg, u_int nreg, uint32_t mask)
{
	int err;
	u_int n;
	uint8_t regs[4];
	uint32_t val;

	n = nreg;
	if ((err = UTP_READREGS(utp, reg, regs, &n)) != 0) {
#ifdef DIAGNOSTIC
		printf("%s: register read error %s(%u,%u): %d\n", __func__,
		    utp->chip->name, reg, nreg, err);
#endif
		return (0);
	}
	if (n < nreg) {
#ifdef DIAGNOSTIC
		printf("%s: got only %u regs %s(%u,%u): %d\n", __func__, n,
		    utp->chip->name, reg, nreg, err);
#endif
		return (0);
	}
	val = 0;
	for (n = nreg; n > 0; n--) {
		val <<= 8;
		val |= regs[n - 1];
	}
	return (val & mask);
}

/*
 * Debugging - dump all registers.
 */
static void
utopia_dump(struct utopia *utp)
{
	uint8_t regs[256];
	u_int n = 256, i;
	int err;

	if ((err = UTP_READREGS(utp, 0, regs, &n)) != 0) {
		printf("UTOPIA reg read error %d\n", err);
		return;
	}
	for (i = 0; i < n; i++) {
		if (i % 16 == 0)
			printf("%02x:", i);
		if (i % 16 == 8)
			printf(" ");
		printf(" %02x", regs[i]);
		if (i % 16 == 15)
			printf("\n");
	}
	if (i % 16 != 0)
		printf("\n");
}

/*
 * Update the carrier status
 */
void
utopia_check_carrier(struct utopia *utp, u_int carr_ok)
{
	int old;

	old = utp->carrier;
	if (carr_ok) {
		/* carrier */
		utp->carrier = UTP_CARR_OK;
		if (old != UTP_CARR_OK) {
			if_printf(utp->ifatm->ifp, "carrier detected\n");
			ATMEV_SEND_IFSTATE_CHANGED(utp->ifatm, 1);
		}
	} else {
		/* no carrier */
		utp->carrier = UTP_CARR_LOST;
		if (old == UTP_CARR_OK) {
			if_printf(utp->ifatm->ifp, "carrier lost\n");
			ATMEV_SEND_IFSTATE_CHANGED(utp->ifatm, 0);
		}
	}
}

static int
unknown_inval(struct utopia *utp, int what __unused)
{

	return (EINVAL);
}

static int
unknown_reset(struct utopia *utp __unused)
{
	return (EIO);
}

static int
unknown_update_carrier(struct utopia *utp)
{
	utp->carrier = UTP_CARR_UNKNOWN;
	return (0);
}

static int
unknown_set_loopback(struct utopia *utp __unused, u_int mode __unused)
{
	return (EINVAL);
}

static void
unknown_intr(struct utopia *utp __unused)
{
}

static void
unknown_update_stats(struct utopia *utp __unused)
{
}

static const struct utopia_chip utopia_chip_unknown = {
	UTP_TYPE_UNKNOWN,
	"unknown",
	0,
	unknown_reset,
	unknown_inval,
	unknown_inval,
	unknown_inval,
	unknown_update_carrier,
	unknown_set_loopback,
	unknown_intr,
	unknown_update_stats,
};

/*
 * Callbacks for the ifmedia infrastructure.
 */
static int
utopia_media_change(struct ifnet *ifp)
{
	struct ifatm *ifatm = (struct ifatm *)ifp->if_softc;
	struct utopia *utp = ifatm->phy;
	int error = 0;

	UTP_LOCK(utp);
	if (utp->chip->type != UTP_TYPE_UNKNOWN && utp->state & UTP_ST_ACTIVE) {
		if (utp->media->ifm_media & IFM_ATM_SDH) {
			if (!(utp->state & UTP_ST_SDH))
				error = utopia_set_sdh(utp, 1);
		} else {
			if (utp->state & UTP_ST_SDH)
				error = utopia_set_sdh(utp, 0);
		}
		if (utp->media->ifm_media & IFM_ATM_UNASSIGNED) {
			if (!(utp->state & UTP_ST_UNASS))
				error = utopia_set_unass(utp, 1);
		} else {
			if (utp->state & UTP_ST_UNASS)
				error = utopia_set_unass(utp, 0);
		}
		if (utp->media->ifm_media & IFM_ATM_NOSCRAMB) {
			if (!(utp->state & UTP_ST_NOSCRAMB))
				error = utopia_set_noscramb(utp, 1);
		} else {
			if (utp->state & UTP_ST_NOSCRAMB)
				error = utopia_set_noscramb(utp, 0);
		}
	} else
		error = EIO;
	UTP_UNLOCK(utp);
	return (error);
}

/*
 * Look at the carrier status.
 */
static void
utopia_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct utopia *utp = ((struct ifatm *)ifp->if_softc)->phy;

	UTP_LOCK(utp);
	if (utp->chip->type != UTP_TYPE_UNKNOWN && utp->state & UTP_ST_ACTIVE) {
		ifmr->ifm_active = IFM_ATM | utp->ifatm->mib.media;

		switch (utp->carrier) {

		  case UTP_CARR_OK:
			ifmr->ifm_status = IFM_AVALID | IFM_ACTIVE;
			break;

		  case UTP_CARR_LOST:
			ifmr->ifm_status = IFM_AVALID;
			break;

		  default:
			ifmr->ifm_status = 0;
			break;
		}
		if (utp->state & UTP_ST_SDH) {
			ifmr->ifm_active |= IFM_ATM_SDH;
			ifmr->ifm_current |= IFM_ATM_SDH;
		}
		if (utp->state & UTP_ST_UNASS) {
			ifmr->ifm_active |= IFM_ATM_UNASSIGNED;
			ifmr->ifm_current |= IFM_ATM_UNASSIGNED;
		}
		if (utp->state & UTP_ST_NOSCRAMB) {
			ifmr->ifm_active |= IFM_ATM_NOSCRAMB;
			ifmr->ifm_current |= IFM_ATM_NOSCRAMB;
		}
	} else {
		ifmr->ifm_active = 0;
		ifmr->ifm_status = 0;
	}
	UTP_UNLOCK(utp);
}

/*
 * Initialize media from the mib
 */
void
utopia_init_media(struct utopia *utp)
{

	ifmedia_removeall(utp->media);
	ifmedia_add(utp->media, IFM_ATM | utp->ifatm->mib.media, 0, NULL);
	ifmedia_set(utp->media, IFM_ATM | utp->ifatm->mib.media);
}

/*
 * Reset all media
 */
void
utopia_reset_media(struct utopia *utp)
{

	ifmedia_removeall(utp->media);
}

/*
 * This is called by the driver as soon as the SUNI registers are accessible.
 * This may be either in the attach routine or the init routine of the driver.
 */
int
utopia_start(struct utopia *utp)
{
	uint8_t reg;
	int err;
	u_int n = 1;

	/*
	 * Try to find out what chip we have
	 */
	if ((err = UTP_READREGS(utp, SUNI_REGO_MRESET, &reg, &n)) != 0)
		return (err);

	switch (reg & SUNI_REGM_MRESET_TYPE) {

	  case SUNI_REGM_MRESET_TYPE_622:
		utp->chip = &utopia_chip_622;
		break;

	  case SUNI_REGM_MRESET_TYPE_LITE:
		/* this may be either a SUNI LITE or a IDT77155 *
		 * Read register 0x70. The SUNI doesn't have it */
		n = 1;
		if ((err = UTP_READREGS(utp, IDTPHY_REGO_RBER, &reg, &n)) != 0)
			return (err);
		if ((reg & ~IDTPHY_REGM_RBER_RESV) ==
		    (IDTPHY_REGM_RBER_FAIL | IDTPHY_REGM_RBER_WARN))
			utp->chip = &utopia_chip_idt77155;
		else
			utp->chip = &utopia_chip_lite;
		break;

	  case SUNI_REGM_MRESET_TYPE_ULTRA:
		utp->chip = &utopia_chip_ultra;
		break;

	  default:
		if (reg == (IDTPHY_REGM_MCR_DRIC | IDTPHY_REGM_MCR_EI))
			utp->chip = &utopia_chip_idt77105;
		else {
			if_printf(utp->ifatm->ifp,
			    "unknown ATM-PHY chip %#x\n", reg);
			utp->chip = &utopia_chip_unknown;
		}
		break;
	}
	utp->state |= UTP_ST_ACTIVE;
	return (0);
}

/*
 * Stop the chip
 */
void
utopia_stop(struct utopia *utp)
{
	utp->state &= ~UTP_ST_ACTIVE;
}

/*
 * Handle the sysctls
 */
static int
utopia_sysctl_regs(SYSCTL_HANDLER_ARGS)
{
	struct utopia *utp = (struct utopia *)arg1;
	int error;
	u_int n;
	uint8_t *val;
	uint8_t new[3];

	if ((n = utp->chip->nregs) == 0)
		return (EIO);
	val = malloc(sizeof(uint8_t) * n, M_TEMP, M_WAITOK);

	UTP_LOCK(utp);
	error = UTP_READREGS(utp, 0, val, &n);
	UTP_UNLOCK(utp);

	if (error) {
		free(val, M_TEMP);
		return (error);
	}

	error = SYSCTL_OUT(req, val, sizeof(uint8_t) * n);
	free(val, M_TEMP);
	if (error != 0 || req->newptr == NULL)
		return (error);

	error = SYSCTL_IN(req, new, sizeof(new));
	if (error)
		return (error);

	UTP_LOCK(utp);
	error = UTP_WRITEREG(utp, new[0], new[1], new[2]);
	UTP_UNLOCK(utp);

	return (error);
}

static int
utopia_sysctl_stats(SYSCTL_HANDLER_ARGS)
{
	struct utopia *utp = (struct utopia *)arg1;
	void *val;
	int error;

	val = malloc(sizeof(utp->stats), M_TEMP, M_WAITOK);

	UTP_LOCK(utp);
	bcopy(&utp->stats, val, sizeof(utp->stats));
	if (req->newptr != NULL)
		bzero((char *)&utp->stats + sizeof(utp->stats.version),
		    sizeof(utp->stats) - sizeof(utp->stats.version));
	UTP_UNLOCK(utp);

	error = SYSCTL_OUT(req, val, sizeof(utp->stats));
	free(val, M_TEMP);

	if (error && req->newptr != NULL)
		bcopy(val, &utp->stats, sizeof(utp->stats));

	/* ignore actual new value */

	return (error);
}

/*
 * Handle the loopback sysctl
 */
static int
utopia_sysctl_loopback(SYSCTL_HANDLER_ARGS)
{
	struct utopia *utp = (struct utopia *)arg1;
	int error;
	u_int loopback;

	error = SYSCTL_OUT(req, &utp->loopback, sizeof(u_int));
	if (error != 0 || req->newptr == NULL)
		return (error);

	error = SYSCTL_IN(req, &loopback, sizeof(u_int));
	if (error)
		return (error);

	UTP_LOCK(utp);
	error = utopia_set_loopback(utp, loopback);
	UTP_UNLOCK(utp);

	return (error);
}

/*
 * Handle the type sysctl
 */
static int
utopia_sysctl_type(SYSCTL_HANDLER_ARGS)
{
	struct utopia *utp = (struct utopia *)arg1;

	return (SYSCTL_OUT(req, &utp->chip->type, sizeof(utp->chip->type)));
}

/*
 * Handle the name sysctl
 */
static int
utopia_sysctl_name(SYSCTL_HANDLER_ARGS)
{
	struct utopia *utp = (struct utopia *)arg1;

	return (SYSCTL_OUT(req, utp->chip->name, strlen(utp->chip->name) + 1));
}

/*
 * Initialize the state. This is called from the drivers attach
 * function. The mutex must be already initialized.
 */
int
utopia_attach(struct utopia *utp, struct ifatm *ifatm, struct ifmedia *media,
    struct mtx *lock, struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *children, const struct utopia_methods *m)
{

	bzero(utp, sizeof(*utp));
	utp->ifatm = ifatm;
	utp->methods = m;
	utp->media = media;
	utp->lock = lock;
	utp->chip = &utopia_chip_unknown;
	utp->stats.version = 1;

	ifmedia_init(media,
	    IFM_ATM_SDH | IFM_ATM_UNASSIGNED | IFM_ATM_NOSCRAMB,
	    utopia_media_change, utopia_media_status);

	if (SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "phy_regs",
	    CTLFLAG_RW | CTLTYPE_OPAQUE, utp, 0, utopia_sysctl_regs, "S",
	    "phy registers") == NULL)
		return (-1);

	if (SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "phy_loopback",
	    CTLFLAG_RW | CTLTYPE_UINT, utp, 0, utopia_sysctl_loopback, "IU",
	    "phy loopback mode") == NULL)
		return (-1);

	if (SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "phy_type",
	    CTLFLAG_RD | CTLTYPE_UINT, utp, 0, utopia_sysctl_type, "IU",
	    "phy type") == NULL)
		return (-1);

	if (SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "phy_name",
	    CTLFLAG_RD | CTLTYPE_STRING, utp, 0, utopia_sysctl_name, "A",
	    "phy name") == NULL)
		return (-1);

	if (SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "phy_stats",
	    CTLFLAG_RW | CTLTYPE_OPAQUE, utp, 0, utopia_sysctl_stats, "S",
	    "phy statistics") == NULL)
		return (-1);

	if (SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "phy_state",
	    CTLFLAG_RD, &utp->state, 0, "phy state") == NULL)
		return (-1);

	if (SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "phy_carrier",
	    CTLFLAG_RD, &utp->carrier, 0, "phy carrier") == NULL)
		return (-1);

	UTP_WLOCK_LIST();
	LIST_INSERT_HEAD(&utopia_list, utp, link);
	UTP_WUNLOCK_LIST();

	utp->state |= UTP_ST_ATTACHED;
	return (0);
}

/*
 * Detach. We set a flag here, wakeup the daemon and let him do it.
 * Here we need the lock for synchronisation with the daemon.
 */
void
utopia_detach(struct utopia *utp)
{

	UTP_LOCK_ASSERT(utp);
	if (utp->state & UTP_ST_ATTACHED) {
		utp->state |= UTP_ST_DETACH;
		while (utp->state & UTP_ST_DETACH) {
			wakeup(&utopia_list);
			msleep(utp, utp->lock, PZERO, "utopia_detach", hz);
		}
	}
}

/*
 * The carrier state kernel proc for those adapters that do not interrupt.
 *
 * We assume, that utopia_attach can safely add a new utopia while we are going
 * through the list without disturbing us (we lock the list while getting
 * the address of the first element, adding is always done at the head).
 * Removing is entirely handled here.
 */
static void
utopia_daemon(void *arg __unused)
{
	struct utopia *utp, *next;

	UTP_RLOCK_LIST();
	while (utopia_kproc != NULL) {
		utp = LIST_FIRST(&utopia_list);
		UTP_RUNLOCK_LIST();

		while (utp != NULL) {
			mtx_lock(&Giant);	/* XXX depend on MPSAFE */
			UTP_LOCK(utp);
			next = LIST_NEXT(utp, link);
			if (utp->state & UTP_ST_DETACH) {
				LIST_REMOVE(utp, link);
				utp->state &= ~UTP_ST_DETACH;
				wakeup_one(utp);
			} else if (utp->state & UTP_ST_ACTIVE) {
				if (utp->flags & UTP_FL_POLL_CARRIER)
					utopia_update_carrier(utp);
				utopia_update_stats(utp);
			}
			UTP_UNLOCK(utp);
			mtx_unlock(&Giant);	/* XXX depend on MPSAFE */
			utp = next;
		}

		UTP_RLOCK_LIST();
		msleep(&utopia_list, &utopia_list_mtx, PZERO, "*idle*", hz);
	}
	wakeup_one(&utopia_list);
	UTP_RUNLOCK_LIST();
	kthread_exit(0);
}

/*
 * Module initialisation
 */
static int
utopia_mod_init(module_t mod, int what, void *arg)
{
	int err;
	struct proc *kp;

	switch (what) {

	  case MOD_LOAD:
		mtx_init(&utopia_list_mtx, "utopia list mutex", NULL, MTX_DEF);
		err = kthread_create(utopia_daemon, NULL, &utopia_kproc,
		    RFHIGHPID, 0, "utopia");
		if (err != 0) {
			printf("cannot created utopia thread %d\n", err);
			return (err);
		}
		break;

	  case MOD_UNLOAD:
		UTP_WLOCK_LIST();
		if ((kp = utopia_kproc) != NULL) {
			utopia_kproc = NULL;
			wakeup_one(&utopia_list);
			PROC_LOCK(kp);
			UTP_WUNLOCK_LIST();
			msleep(kp, &kp->p_mtx, PWAIT, "utopia_destroy", 0);
			PROC_UNLOCK(kp);
		} else
			UTP_WUNLOCK_LIST();
		mtx_destroy(&utopia_list_mtx);
		break;
	  default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t utopia_mod = {
        "utopia",
        utopia_mod_init,
        0
};
                
DECLARE_MODULE(utopia, utopia_mod, SI_SUB_INIT_IF, SI_ORDER_ANY);
MODULE_VERSION(utopia, 1);
