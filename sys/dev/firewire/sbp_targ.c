/*-
 * Copyright (C) 2003
 * 	Hidetoshi Shimokawa. All rights reserved.
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
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#if __FreeBSD_version < 500000
#include <sys/devicestat.h>
#endif

#include <sys/bus.h>
#include <machine/bus.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/iec13213.h>
#include <dev/firewire/sbp.h>
#include <dev/firewire/fwmem.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/cam_periph.h>
#include <cam/scsi/scsi_all.h>

#define SBP_TARG_RECV_LEN	8
#define MAX_INITIATORS		8
#define MAX_LUN			63
#define MAX_LOGINS		63
#define MAX_NODES		63
/*
 * management/command block agent registers
 *
 * BASE 0xffff f001 0000 management port
 * BASE 0xffff f001 0020 command port for login id 0
 * BASE 0xffff f001 0040 command port for login id 1
 *
 */
#define SBP_TARG_MGM	 0x10000	/* offset from 0xffff f000 000 */
#define SBP_TARG_BIND_HI	0xffff
#define SBP_TARG_BIND_LO(l)	(0xf0000000 + SBP_TARG_MGM + 0x20 * ((l) + 1))
#define SBP_TARG_BIND_START	(((u_int64_t)SBP_TARG_BIND_HI << 32) | \
				    SBP_TARG_BIND_LO(-1))
#define SBP_TARG_BIND_END	(((u_int64_t)SBP_TARG_BIND_HI << 32) | \
				    SBP_TARG_BIND_LO(MAX_LOGINS))
#define SBP_TARG_LOGIN_ID(lo)	(((lo) - SBP_TARG_BIND_LO(0))/0x20)

#define FETCH_MGM	0
#define FETCH_CMD	1
#define FETCH_POINTER	2

#define F_LINK_ACTIVE	(1 << 0)
#define F_ATIO_STARVED	(1 << 1)
#define F_LOGIN		(1 << 2)
#define F_HOLD		(1 << 3)
#define F_FREEZED	(1 << 4)

MALLOC_DEFINE(M_SBP_TARG, "sbp_targ", "SBP-II/FireWire target mode");

static int debug = 0;

SYSCTL_INT(_debug, OID_AUTO, sbp_targ_debug, CTLFLAG_RW, &debug, 0,
        "SBP target mode debug flag");

struct sbp_targ_login {
	struct sbp_targ_lstate *lstate;
	struct fw_device *fwdev;
	struct sbp_login_res loginres;
	uint16_t fifo_hi; 
	uint16_t last_hi;
	uint32_t fifo_lo; 
	uint32_t last_lo;
	STAILQ_HEAD(, orb_info) orbs;
	STAILQ_ENTRY(sbp_targ_login) link;
	uint16_t hold_sec;
	uint16_t id;
	uint8_t flags; 
	uint8_t spd; 
	struct callout hold_callout;
};

struct sbp_targ_lstate {
	uint16_t lun;
	struct sbp_targ_softc *sc;
	struct cam_path *path;
	struct ccb_hdr_slist accept_tios;
	struct ccb_hdr_slist immed_notifies;
	struct crom_chunk model;
	uint32_t flags; 
	STAILQ_HEAD(, sbp_targ_login) logins;
};

struct sbp_targ_softc {
        struct firewire_dev_comm fd;
	struct cam_sim *sim;
	struct cam_path *path;
	struct fw_bind fwb;
	int ndevs;
	int flags;
	struct crom_chunk unit;
	struct sbp_targ_lstate *lstate[MAX_LUN];
	struct sbp_targ_lstate *black_hole;
	struct sbp_targ_login *logins[MAX_LOGINS];
	struct mtx mtx;
};
#define SBP_LOCK(sc) mtx_lock(&(sc)->mtx)
#define SBP_UNLOCK(sc) mtx_unlock(&(sc)->mtx)

struct corb4 {
#if BYTE_ORDER == BIG_ENDIAN
	uint32_t n:1,
		  rq_fmt:2,
		  :1,
		  dir:1,
		  spd:3,
		  max_payload:4,
		  page_table_present:1,
		  page_size:3,
		  data_size:16;
#else
	uint32_t data_size:16,
		  page_size:3,
		  page_table_present:1,
		  max_payload:4,
		  spd:3,
		  dir:1,
		  :1,
		  rq_fmt:2,
		  n:1;
#endif
};

struct morb4 {
#if BYTE_ORDER == BIG_ENDIAN
	uint32_t n:1,
		  rq_fmt:2,
		  :9,
		  fun:4,
		  id:16;
#else
	uint32_t id:16,
		  fun:4,
		  :9,
		  rq_fmt:2,
		  n:1;
#endif
};

struct orb_info {
	struct sbp_targ_softc *sc;
	struct fw_device *fwdev;
	struct sbp_targ_login *login;
	union ccb *ccb;
	struct ccb_accept_tio *atio;
	uint8_t state; 
#define ORBI_STATUS_NONE	0
#define ORBI_STATUS_FETCH	1
#define ORBI_STATUS_ATIO	2
#define ORBI_STATUS_CTIO	3
#define ORBI_STATUS_STATUS	4
#define ORBI_STATUS_POINTER	5
#define ORBI_STATUS_ABORTED	7
	uint8_t refcount; 
	uint16_t orb_hi;
	uint32_t orb_lo;
	uint32_t data_hi;
	uint32_t data_lo;
	struct corb4 orb4;
	STAILQ_ENTRY(orb_info) link;
	uint32_t orb[8];
	uint32_t *page_table;
	struct sbp_status status;
};

static char *orb_fun_name[] = {
	ORB_FUN_NAMES
};

static void sbp_targ_recv(struct fw_xfer *);
static void sbp_targ_fetch_orb(struct sbp_targ_softc *, struct fw_device *,
    uint16_t, uint32_t, struct sbp_targ_login *, int);
static void sbp_targ_abort(struct sbp_targ_softc *, struct orb_info *);

static void
sbp_targ_identify(driver_t *driver, device_t parent)
{
	BUS_ADD_CHILD(parent, 0, "sbp_targ", device_get_unit(parent));
}

static int
sbp_targ_probe(device_t dev)
{
	device_t pa;

	pa = device_get_parent(dev);
	if(device_get_unit(dev) != device_get_unit(pa)){
		return(ENXIO);
	}

	device_set_desc(dev, "SBP-2/SCSI over FireWire target mode");
	return (0);
}

static void
sbp_targ_dealloc_login(struct sbp_targ_login *login)
{
	struct orb_info *orbi, *next;

	if (login == NULL) {
		printf("%s: login = NULL\n", __func__);
		return;
	}
	for (orbi = STAILQ_FIRST(&login->orbs); orbi != NULL; orbi = next) {
		next = STAILQ_NEXT(orbi, link);
		free(orbi, M_SBP_TARG);
	}
	callout_stop(&login->hold_callout);

	STAILQ_REMOVE(&login->lstate->logins, login, sbp_targ_login, link);
	login->lstate->sc->logins[login->id] = NULL;
	free((void *)login, M_SBP_TARG);
}

static void
sbp_targ_hold_expire(void *arg)
{
	struct sbp_targ_login *login;

	login = (struct sbp_targ_login *)arg;

	if (login->flags & F_HOLD) {
		printf("%s: login_id=%d expired\n", __func__, login->id);
		sbp_targ_dealloc_login(login);
	} else {
		printf("%s: login_id=%d not hold\n", __func__, login->id);
	}
}

static void
sbp_targ_post_busreset(void *arg)
{
	struct sbp_targ_softc *sc;
	struct crom_src *src;
	struct crom_chunk *root;
	struct crom_chunk *unit;
	struct sbp_targ_lstate *lstate;
	struct sbp_targ_login *login;
	int i;

	sc = (struct sbp_targ_softc *)arg;
	src = sc->fd.fc->crom_src;
	root = sc->fd.fc->crom_root;

	unit = &sc->unit;

	if ((sc->flags & F_FREEZED) == 0) {
		SBP_LOCK(sc);
		sc->flags |= F_FREEZED;
		xpt_freeze_simq(sc->sim, /*count*/1);
		SBP_UNLOCK(sc);
	} else {
		printf("%s: already freezed\n", __func__);
	}

	bzero(unit, sizeof(struct crom_chunk));

	crom_add_chunk(src, root, unit, CROM_UDIR);
	crom_add_entry(unit, CSRKEY_SPEC, CSRVAL_ANSIT10);
	crom_add_entry(unit, CSRKEY_VER, CSRVAL_T10SBP2);
	crom_add_entry(unit, CSRKEY_COM_SPEC, CSRVAL_ANSIT10);
	crom_add_entry(unit, CSRKEY_COM_SET, CSRVAL_SCSI);

	crom_add_entry(unit, CROM_MGM, SBP_TARG_MGM >> 2);
	crom_add_entry(unit, CSRKEY_UNIT_CH, (10<<8) | 8);

	for (i = 0; i < MAX_LUN; i ++) {
		lstate = sc->lstate[i];
		if (lstate == NULL)
			continue;
		crom_add_entry(unit, CSRKEY_FIRM_VER, 1);
		crom_add_entry(unit, CROM_LUN, i);
		crom_add_entry(unit, CSRKEY_MODEL, 1);
		crom_add_simple_text(src, unit, &lstate->model, "TargetMode");
	}

	/* Process for reconnection hold time */
	for (i = 0; i < MAX_LOGINS; i ++) {
		login = sc->logins[i];
		if (login == NULL)
			continue;
		sbp_targ_abort(sc, STAILQ_FIRST(&login->orbs));
		if (login->flags & F_LOGIN) {
			login->flags |= F_HOLD;
			callout_reset(&login->hold_callout,
			    hz * login->hold_sec, 
			    sbp_targ_hold_expire, (void *)login);
		}
	}
}

static void
sbp_targ_post_explore(void *arg)
{
	struct sbp_targ_softc *sc;

	sc = (struct sbp_targ_softc *)arg;
	SBP_LOCK(sc);
	sc->flags &= ~F_FREEZED;
	xpt_release_simq(sc->sim, /*run queue*/TRUE);
	SBP_UNLOCK(sc);
	return;
}

static cam_status
sbp_targ_find_devs(struct sbp_targ_softc *sc, union ccb *ccb,
    struct sbp_targ_lstate **lstate, int notfound_failure)
{
	u_int lun;

	/* XXX 0 is the only vaild target_id */
	if (ccb->ccb_h.target_id == CAM_TARGET_WILDCARD &&
	    ccb->ccb_h.target_lun == CAM_LUN_WILDCARD) {
		*lstate = sc->black_hole;
		return (CAM_REQ_CMP);
	}

	if (ccb->ccb_h.target_id != 0)
		return (CAM_TID_INVALID);

	lun = ccb->ccb_h.target_lun;
	if (lun >= MAX_LUN)
		return (CAM_LUN_INVALID);
	
	*lstate = sc->lstate[lun];

	if (notfound_failure != 0 && *lstate == NULL)
		return (CAM_PATH_INVALID);

	return (CAM_REQ_CMP);
}

static void
sbp_targ_en_lun(struct sbp_targ_softc *sc, union ccb *ccb)
{
	struct ccb_en_lun *cel = &ccb->cel;
	struct sbp_targ_lstate *lstate;
	cam_status status;

	status = sbp_targ_find_devs(sc, ccb, &lstate, 0);
	if (status != CAM_REQ_CMP) {
		ccb->ccb_h.status = status;
		return;
	}

	if (cel->enable != 0) {
		if (lstate != NULL) {
			xpt_print_path(ccb->ccb_h.path);
			printf("Lun already enabled\n");
			ccb->ccb_h.status = CAM_LUN_ALRDY_ENA;
			return;
		}
		if (cel->grp6_len != 0 || cel->grp7_len != 0) {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			printf("Non-zero Group Codes\n");
			return;
		}
		lstate = (struct sbp_targ_lstate *)
		    malloc(sizeof(*lstate), M_SBP_TARG, M_NOWAIT | M_ZERO);
		if (lstate == NULL) {
			xpt_print_path(ccb->ccb_h.path);
			printf("Couldn't allocate lstate\n");
			ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
			return;
		}
		if (ccb->ccb_h.target_id == CAM_TARGET_WILDCARD)
			sc->black_hole = lstate;
		else
			sc->lstate[ccb->ccb_h.target_lun] = lstate;
		memset(lstate, 0, sizeof(*lstate));
		lstate->sc = sc;
		status = xpt_create_path(&lstate->path, /*periph*/NULL,
					 xpt_path_path_id(ccb->ccb_h.path),
					 xpt_path_target_id(ccb->ccb_h.path),
					 xpt_path_lun_id(ccb->ccb_h.path));
		if (status != CAM_REQ_CMP) {
			free(lstate, M_SBP_TARG);
			xpt_print_path(ccb->ccb_h.path);
			printf("Couldn't allocate path\n");
			ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
			return;
		}
		SLIST_INIT(&lstate->accept_tios);
		SLIST_INIT(&lstate->immed_notifies);
		STAILQ_INIT(&lstate->logins);

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_print_path(ccb->ccb_h.path);
		printf("Lun now enabled for target mode\n");
		/* bus reset */
		sc->fd.fc->ibr(sc->fd.fc);
	} else {
		struct sbp_targ_login *login, *next;

		if (lstate == NULL) {
			ccb->ccb_h.status = CAM_LUN_INVALID;
			return;
		}
		ccb->ccb_h.status = CAM_REQ_CMP;

		if (SLIST_FIRST(&lstate->accept_tios) != NULL) {
			printf("ATIOs pending\n");
			ccb->ccb_h.status = CAM_REQ_INVALID;
		}

		if (SLIST_FIRST(&lstate->immed_notifies) != NULL) {
			printf("INOTs pending\n");
			ccb->ccb_h.status = CAM_REQ_INVALID;
		}

		if (ccb->ccb_h.status != CAM_REQ_CMP) {
			return;
		}

		xpt_print_path(ccb->ccb_h.path);
		printf("Target mode disabled\n");
		xpt_free_path(lstate->path);

		for (login = STAILQ_FIRST(&lstate->logins); login != NULL;
		    login = next) {
			next = STAILQ_NEXT(login, link);
			sbp_targ_dealloc_login(login);
		}

		if (ccb->ccb_h.target_id == CAM_TARGET_WILDCARD)
			sc->black_hole = NULL;
		else
			sc->lstate[ccb->ccb_h.target_lun] = NULL;
		free(lstate, M_SBP_TARG);

		/* bus reset */
		sc->fd.fc->ibr(sc->fd.fc);
	}
}

static void
sbp_targ_send_lstate_events(struct sbp_targ_softc *sc,
    struct sbp_targ_lstate *lstate)
{
#if 0
	struct ccb_hdr *ccbh;
	struct ccb_immed_notify *inot;

	printf("%s: not implemented yet\n", __func__);
#endif
}


static __inline void
sbp_targ_remove_orb_info_locked(struct sbp_targ_login *login, struct orb_info *orbi)
{
	STAILQ_REMOVE(&login->orbs, orbi, orb_info, link);
}

static __inline void
sbp_targ_remove_orb_info(struct sbp_targ_login *login, struct orb_info *orbi)
{
	SBP_LOCK(orbi->sc);
	STAILQ_REMOVE(&login->orbs, orbi, orb_info, link);
	SBP_UNLOCK(orbi->sc);
}

/*
 * tag_id/init_id encoding
 *
 * tag_id and init_id has only 32bit for each.
 * scsi_target can handle very limited number(up to 15) of init_id.
 * we have to encode 48bit orb and 64bit EUI64 into these
 * variables.
 *
 * tag_id represents lower 32bit of ORB address.
 * init_id represents login_id.
 *
 */

static struct orb_info *
sbp_targ_get_orb_info(struct sbp_targ_lstate *lstate,
    u_int tag_id, u_int init_id)
{
	struct sbp_targ_login *login;
	struct orb_info *orbi;

	login = lstate->sc->logins[init_id];
	if (login == NULL) {
		printf("%s: no such login\n", __func__);
		return (NULL);
	}
	STAILQ_FOREACH(orbi, &login->orbs, link)
		if (orbi->orb_lo == tag_id)
			goto found;
	printf("%s: orb not found tag_id=0x%08x init_id=%d\n",
				 __func__, tag_id, init_id);
	return (NULL);
found:
	return (orbi);
}

static void
sbp_targ_abort(struct sbp_targ_softc *sc, struct orb_info *orbi)
{
	struct orb_info *norbi;

	SBP_LOCK(sc);
	for (; orbi != NULL; orbi = norbi) {
		printf("%s: status=%d ccb=%p\n", __func__, orbi->state, orbi->ccb);
		norbi = STAILQ_NEXT(orbi, link);
		if (orbi->state != ORBI_STATUS_ABORTED) {
			if (orbi->ccb != NULL) {
				orbi->ccb->ccb_h.status = CAM_REQ_ABORTED;
				xpt_done(orbi->ccb);
				orbi->ccb = NULL;
			}
#if 0
			if (orbi->state <= ORBI_STATUS_ATIO) {
				sbp_targ_remove_orb_info_locked(orbi->login, orbi);
				free(orbi, M_SBP_TARG);
			} else
#endif
				orbi->state = ORBI_STATUS_ABORTED;
		}
	}
	SBP_UNLOCK(sc);
}

static void
sbp_targ_free_orbi(struct fw_xfer *xfer)
{
	struct orb_info *orbi;

	orbi = (struct orb_info *)xfer->sc;
	if (xfer->resp != 0) {
		/* XXX */
		printf("%s: xfer->resp = %d\n", __func__, xfer->resp);
	}
	free(orbi, M_SBP_TARG);
	fw_xfer_free(xfer);
}

static void
sbp_targ_status_FIFO(struct orb_info *orbi,
    uint32_t fifo_hi, uint32_t fifo_lo, int dequeue)
{
	struct fw_xfer *xfer;

	if (dequeue)
		sbp_targ_remove_orb_info(orbi->login, orbi);

	xfer = fwmem_write_block(orbi->fwdev, (void *)orbi,
	    /*spd*/2, fifo_hi, fifo_lo,
	    sizeof(uint32_t) * (orbi->status.len + 1), (char *)&orbi->status,
	    sbp_targ_free_orbi);

	if (xfer == NULL) {
		/* XXX */
		printf("%s: xfer == NULL\n", __func__);
	}
}

static void
sbp_targ_send_status(struct orb_info *orbi, union ccb *ccb)
{
	struct sbp_status *sbp_status;
	struct orb_info *norbi;

	sbp_status = &orbi->status;

	orbi->state = ORBI_STATUS_STATUS;

	sbp_status->resp = 0; /* XXX */
	sbp_status->status = 0; /* XXX */
	sbp_status->dead = 0; /* XXX */

	switch (ccb->csio.scsi_status) {
	case SCSI_STATUS_OK:
		if (debug)
			printf("%s: STATUS_OK\n", __func__);
		sbp_status->len = 1;
		break;
	case SCSI_STATUS_CHECK_COND:
	case SCSI_STATUS_BUSY:
	case SCSI_STATUS_CMD_TERMINATED:
	{
		struct sbp_cmd_status *sbp_cmd_status;
		struct scsi_sense_data *sense;

		if (debug)
			printf("%s: STATUS %d\n", __func__,
			    ccb->csio.scsi_status);
		sbp_cmd_status = (struct sbp_cmd_status *)&sbp_status->data[0];
		sbp_cmd_status->status = ccb->csio.scsi_status;
		sense = &ccb->csio.sense_data;

#if 0		/* XXX What we should do? */
#if 0
		sbp_targ_abort(orbi->sc, STAILQ_NEXT(orbi, link));
#else
		norbi = STAILQ_NEXT(orbi, link);
		while (norbi) {
			printf("%s: status=%d\n", __func__, norbi->state);
			if (norbi->ccb != NULL) {
				norbi->ccb->ccb_h.status = CAM_REQ_ABORTED;
				xpt_done(norbi->ccb);
				norbi->ccb = NULL;
			}
			sbp_targ_remove_orb_info_locked(orbi->login, norbi);
			norbi = STAILQ_NEXT(norbi, link);
			free(norbi, M_SBP_TARG);
		}
#endif
#endif

		if ((sense->error_code & SSD_ERRCODE) == SSD_CURRENT_ERROR)
			sbp_cmd_status->sfmt = SBP_SFMT_CURR;
		else
			sbp_cmd_status->sfmt = SBP_SFMT_DEFER;

		sbp_cmd_status->valid = (sense->error_code & SSD_ERRCODE_VALID)
		    ? 1 : 0;
		sbp_cmd_status->s_key = sense->flags & SSD_KEY;
		sbp_cmd_status->mark = (sense->flags & SSD_FILEMARK)? 1 : 0;
		sbp_cmd_status->eom = (sense->flags & SSD_EOM) ? 1 : 0;
		sbp_cmd_status->ill_len = (sense->flags & SSD_ILI) ? 1 : 0;

		bcopy(&sense->info[0], &sbp_cmd_status->info, 4);

		if (sense->extra_len <= 6)
			/* add_sense_code(_qual), info, cmd_spec_info */
			sbp_status->len = 4;
		else
			/* fru, sense_key_spec */
			sbp_status->len = 5;
			
		bcopy(&sense->cmd_spec_info[0], &sbp_cmd_status->cdb, 4);

		sbp_cmd_status->s_code = sense->add_sense_code;
		sbp_cmd_status->s_qlfr = sense->add_sense_code_qual;
		sbp_cmd_status->fru = sense->fru;

		bcopy(&sense->sense_key_spec[0],
		    &sbp_cmd_status->s_keydep[0], 3);

		break;
	}
	default:
		printf("%s: unknown scsi status 0x%x\n", __func__,
		    sbp_status->status);
	}

	if (orbi->page_table != NULL)
		free(orbi->page_table, M_SBP_TARG);

	sbp_targ_status_FIFO(orbi,
	    orbi->login->fifo_hi, orbi->login->fifo_lo, /*dequeue*/1);
}

static void
sbp_targ_cam_done(struct fw_xfer *xfer)
{
	struct orb_info *orbi;
	union ccb *ccb;

	orbi = (struct orb_info *)xfer->sc;

	if (debug > 1)
		printf("%s: resp=%d refcount=%d\n", __func__,
			xfer->resp, orbi->refcount);

	if (xfer->resp != 0) {
		printf("%s: xfer->resp = %d\n", __func__, xfer->resp);
		orbi->status.resp = SBP_TRANS_FAIL;
		orbi->status.status = OBJ_DATA | SBE_TIMEOUT/*XXX*/;
		orbi->status.dead = 1;
		sbp_targ_abort(orbi->sc, STAILQ_NEXT(orbi, link));
	}

	orbi->refcount --;

	ccb = orbi->ccb;
	if (orbi->refcount == 0) {
		orbi->ccb = NULL;
		if (orbi->state == ORBI_STATUS_ABORTED) {
			if (debug)
				printf("%s: orbi aborted\n", __func__);
			sbp_targ_remove_orb_info(orbi->login, orbi);
			if (orbi->page_table != NULL)
				free(orbi->page_table, M_SBP_TARG);
			free(orbi, M_SBP_TARG);
		} else if (orbi->status.resp == 0) {
			if ((ccb->ccb_h.flags & CAM_SEND_STATUS) != 0)
				sbp_targ_send_status(orbi, ccb);
			ccb->ccb_h.status = CAM_REQ_CMP;
			SBP_LOCK(orbi->sc);
			xpt_done(ccb);
			SBP_UNLOCK(orbi->sc);
		} else {
			orbi->status.len = 1;
			sbp_targ_status_FIFO(orbi,
		    	    orbi->login->fifo_hi, orbi->login->fifo_lo,
			    /*dequeue*/1);
			ccb->ccb_h.status = CAM_REQ_ABORTED;
			SBP_LOCK(orbi->sc);
			xpt_done(ccb);
			SBP_UNLOCK(orbi->sc);
		}
	}

	fw_xfer_free(xfer);
}

static cam_status
sbp_targ_abort_ccb(struct sbp_targ_softc *sc, union ccb *ccb)
{
	union ccb *accb;
	struct sbp_targ_lstate *lstate;
	struct ccb_hdr_slist *list;
	struct ccb_hdr *curelm;
	int found;
	cam_status status;

	status = sbp_targ_find_devs(sc, ccb, &lstate, 0);
	if (status != CAM_REQ_CMP)
		return (status);

	accb = ccb->cab.abort_ccb;

	if (accb->ccb_h.func_code == XPT_ACCEPT_TARGET_IO)
		list = &lstate->accept_tios;
	else if (accb->ccb_h.func_code == XPT_IMMED_NOTIFY)
		list = &lstate->immed_notifies;
	else
		return (CAM_UA_ABORT);

	curelm = SLIST_FIRST(list);
	found = 0;
	if (curelm == &accb->ccb_h) {
		found = 1;
		SLIST_REMOVE_HEAD(list, sim_links.sle);
	} else {
		while(curelm != NULL) {
			struct ccb_hdr *nextelm;

			nextelm = SLIST_NEXT(curelm, sim_links.sle);
			if (nextelm == &accb->ccb_h) {
				found = 1;
				SLIST_NEXT(curelm, sim_links.sle) =
				    SLIST_NEXT(nextelm, sim_links.sle);
				break;
			}
			curelm = nextelm;
		}
	}
	if (found) {
		accb->ccb_h.status = CAM_REQ_ABORTED;
		xpt_done(accb);
		return (CAM_REQ_CMP);
	}
	printf("%s: not found\n", __func__);
	return (CAM_PATH_INVALID);
}

static void
sbp_targ_xfer_buf(struct orb_info *orbi, u_int offset,
    uint16_t dst_hi, uint32_t dst_lo, u_int size,
    void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	u_int len, ccb_dir, off = 0;
	char *ptr;

	if (debug > 1)
		printf("%s: offset=%d size=%d\n", __func__, offset, size);
	ccb_dir = orbi->ccb->ccb_h.flags & CAM_DIR_MASK;
	ptr = (char *)orbi->ccb->csio.data_ptr + offset;

	while (size > 0) {
		/* XXX assume dst_lo + off doesn't overflow */
		len = MIN(size, 2048 /* XXX */);
		size -= len;
		orbi->refcount ++;
		if (ccb_dir == CAM_DIR_OUT)
			xfer = fwmem_read_block(orbi->fwdev,
			   (void *)orbi, /*spd*/2,
			    dst_hi, dst_lo + off, len,
			    ptr + off, hand);
		else
			xfer = fwmem_write_block(orbi->fwdev,
			   (void *)orbi, /*spd*/2,
			    dst_hi, dst_lo + off, len,
			    ptr + off, hand);
		if (xfer == NULL) {
			printf("%s: xfer == NULL", __func__);
			/* XXX what should we do?? */
			orbi->refcount --;
		}
		off += len;
	}
}

static void
sbp_targ_pt_done(struct fw_xfer *xfer)
{
	struct orb_info *orbi;
	union ccb *ccb;
	u_int i, offset, res, len;
	uint32_t t1, t2, *p;

	orbi = (struct orb_info *)xfer->sc;
	ccb = orbi->ccb;
	if (orbi->state == ORBI_STATUS_ABORTED) {
		if (debug)
			printf("%s: orbi aborted\n", __func__);
		sbp_targ_remove_orb_info(orbi->login, orbi);
		free(orbi->page_table, M_SBP_TARG);
		free(orbi, M_SBP_TARG);
		fw_xfer_free(xfer);
		return;
	}
	if (xfer->resp != 0) {
		printf("%s: xfer->resp = %d\n", __func__, xfer->resp);
		orbi->status.resp = SBP_TRANS_FAIL;
		orbi->status.status = OBJ_PT | SBE_TIMEOUT/*XXX*/;
		orbi->status.dead = 1;
		orbi->status.len = 1;
		sbp_targ_abort(orbi->sc, STAILQ_NEXT(orbi, link));

		sbp_targ_status_FIFO(orbi,
		    orbi->login->fifo_hi, orbi->login->fifo_lo, /*dequeue*/1);
		free(orbi->page_table, M_SBP_TARG);
		fw_xfer_free(xfer);
		return;
	}
	res = ccb->csio.dxfer_len;
	offset = 0;
	if (debug)
		printf("%s: dxfer_len=%d\n", __func__, res);
	orbi->refcount ++;
	for (p = orbi->page_table, i = orbi->orb4.data_size; i > 0; i --) {
		t1 = ntohl(*p++);
		t2 = ntohl(*p++);
		if (debug > 1)
			printf("page_table: %04x:%08x %d\n", 
			    t1 & 0xffff, t2, t1>>16);
		len = MIN(t1 >> 16, res);
		res -= len;
		sbp_targ_xfer_buf(orbi, offset, t1 & 0xffff, t2, len,
		    sbp_targ_cam_done);
		offset += len;
		if (res == 0)
			break;
	}
	orbi->refcount --;
	if (orbi->refcount == 0)
		printf("%s: refcount == 0\n", __func__);
	if (res !=0)
		/* XXX handle res != 0 case */
		printf("%s: page table is too small(%d)\n", __func__, res);

	fw_xfer_free(xfer);
	return;
}

static void
sbp_targ_fetch_pt(struct orb_info *orbi)
{
	struct fw_xfer *xfer;

	if (debug)
		printf("%s: page_table_size=%d\n",
		    __func__, orbi->orb4.data_size);
	orbi->page_table = malloc(orbi->orb4.data_size*8, M_SBP_TARG, M_NOWAIT);
	if (orbi->page_table == NULL)
		goto error;
	xfer = fwmem_read_block(orbi->fwdev, (void *)orbi, /*spd*/2,
		    orbi->data_hi, orbi->data_lo, orbi->orb4.data_size*8,
			    (void *)orbi->page_table, sbp_targ_pt_done);
	if (xfer != NULL)
		return;
error:
	orbi->ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
	xpt_done(orbi->ccb);
	return;
}

static void
sbp_targ_action1(struct cam_sim *sim, union ccb *ccb)
{
	struct sbp_targ_softc *sc;
	struct sbp_targ_lstate *lstate;
	cam_status status;
	u_int ccb_dir;

	sc =  (struct sbp_targ_softc *)cam_sim_softc(sim);

	status = sbp_targ_find_devs(sc, ccb, &lstate, TRUE);

	switch (ccb->ccb_h.func_code) {
	case XPT_CONT_TARGET_IO:
	{
		struct orb_info *orbi;

		if (debug)
			printf("%s: XPT_CONT_TARGET_IO (0x%08x)\n",
					 __func__, ccb->csio.tag_id);

		if (status != CAM_REQ_CMP) {
			ccb->ccb_h.status = status;
			xpt_done(ccb);
			break;
		}
		/* XXX transfer from/to initiator */
		orbi = sbp_targ_get_orb_info(lstate,
		    ccb->csio.tag_id, ccb->csio.init_id);
		if (orbi == NULL) {
			ccb->ccb_h.status = CAM_REQ_ABORTED; /* XXX */
			xpt_done(ccb);
			break;
		}
		if (orbi->state == ORBI_STATUS_ABORTED) {
			if (debug)
				printf("%s: ctio aborted\n", __func__);
			sbp_targ_remove_orb_info_locked(orbi->login, orbi);
			free(orbi, M_SBP_TARG);
			ccb->ccb_h.status = CAM_REQ_ABORTED;
			xpt_done(ccb);
			break;
		}
		orbi->state = ORBI_STATUS_CTIO;

		orbi->ccb = ccb;
		ccb_dir = ccb->ccb_h.flags & CAM_DIR_MASK;

		/* XXX */
		if (ccb->csio.dxfer_len == 0)
			ccb_dir = CAM_DIR_NONE;

		/* Sanity check */
		if (ccb_dir == CAM_DIR_IN && orbi->orb4.dir == 0)
			printf("%s: direction mismatch\n", __func__);

		/* check page table */
		if (ccb_dir != CAM_DIR_NONE && orbi->orb4.page_table_present) {
			if (debug)
				printf("%s: page_table_present\n",
				    __func__);
			if (orbi->orb4.page_size != 0) {
				printf("%s: unsupported pagesize %d != 0\n",
			 	    __func__, orbi->orb4.page_size);
				ccb->ccb_h.status = CAM_REQ_INVALID;
				xpt_done(ccb);
				break;
			}
			sbp_targ_fetch_pt(orbi);
			break;
		}

		/* Sanity check */
		if (ccb_dir != CAM_DIR_NONE &&
		    orbi->orb4.data_size != ccb->csio.dxfer_len)
			printf("%s: data_size(%d) != dxfer_len(%d)\n",
			    __func__, orbi->orb4.data_size,
			    ccb->csio.dxfer_len);

		if (ccb_dir != CAM_DIR_NONE)
			sbp_targ_xfer_buf(orbi, 0, orbi->data_hi,
			    orbi->data_lo,
			    MIN(orbi->orb4.data_size, ccb->csio.dxfer_len),
			    sbp_targ_cam_done);

		if (ccb_dir == CAM_DIR_NONE) {
			if ((ccb->ccb_h.flags & CAM_SEND_STATUS) != 0) {
				/* XXX */
				SBP_UNLOCK(sc);
				sbp_targ_send_status(orbi, ccb);
				SBP_LOCK(sc);
			}
			ccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(ccb);
		}
		break;
	}
	case XPT_ACCEPT_TARGET_IO:	/* Add Accept Target IO Resource */
		if (status != CAM_REQ_CMP) {
			ccb->ccb_h.status = status;
			xpt_done(ccb);
			break;
		}
		SLIST_INSERT_HEAD(&lstate->accept_tios, &ccb->ccb_h,
		    sim_links.sle);
		ccb->ccb_h.status = CAM_REQ_INPROG;
		if ((lstate->flags & F_ATIO_STARVED) != 0) {
			struct sbp_targ_login *login;

			if (debug)
				printf("%s: new atio arrived\n", __func__);
			lstate->flags &= ~F_ATIO_STARVED;
			STAILQ_FOREACH(login, &lstate->logins, link)
				if ((login->flags & F_ATIO_STARVED) != 0) {
					login->flags &= ~F_ATIO_STARVED;
					sbp_targ_fetch_orb(lstate->sc,
					    login->fwdev,
					    login->last_hi, login->last_lo,
					    login, FETCH_CMD);
				}
		}
		break;
	case XPT_NOTIFY_ACK:		/* recycle notify ack */
	case XPT_IMMED_NOTIFY:		/* Add Immediate Notify Resource */
		if (status != CAM_REQ_CMP) {
			ccb->ccb_h.status = status;
			xpt_done(ccb);
			break;
		}
		SLIST_INSERT_HEAD(&lstate->immed_notifies, &ccb->ccb_h,
		    sim_links.sle);
		ccb->ccb_h.status = CAM_REQ_INPROG;
		sbp_targ_send_lstate_events(sc, lstate);
		break;
	case XPT_EN_LUN:
		sbp_targ_en_lun(sc, ccb);
		xpt_done(ccb);
		break;
	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = PI_TAG_ABLE;
		cpi->target_sprt = PIT_PROCESSOR
				 | PIT_DISCONNECT
				 | PIT_TERM_IO;
		cpi->hba_misc = PIM_NOBUSRESET | PIM_NO_6_BYTE;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = 7; /* XXX */
		cpi->max_lun = MAX_LUN - 1;
		cpi->initiator_id = 7; /* XXX */
		cpi->bus_id = sim->bus_id;
		cpi->base_transfer_speed = 400 * 1000 / 8;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "SBP_TARG", HBA_IDLEN);
		strncpy(cpi->dev_name, sim->sim_name, DEV_IDLEN);
		cpi->unit_number = sim->unit_number;

		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_ABORT:
	{
		union ccb *accb = ccb->cab.abort_ccb;

		switch (accb->ccb_h.func_code) {
		case XPT_ACCEPT_TARGET_IO:
		case XPT_IMMED_NOTIFY:
			ccb->ccb_h.status = sbp_targ_abort_ccb(sc, ccb);
			break;
		case XPT_CONT_TARGET_IO:
			/* XXX */
			ccb->ccb_h.status = CAM_UA_ABORT;
			break;
		default:
			printf("%s: aborting unknown function %d\n", 
				__func__, accb->ccb_h.func_code);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			break;
		}
		xpt_done(ccb);
		break;
	}
	default:
		printf("%s: unknown function %d\n",
		    __func__, ccb->ccb_h.func_code);
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
	return;
}

static void
sbp_targ_action(struct cam_sim *sim, union ccb *ccb)
{
	int s;

	s = splfw();
	sbp_targ_action1(sim, ccb);
	splx(s);
}

static void
sbp_targ_poll(struct cam_sim *sim)
{
	/* XXX */
	return;
}

static void
sbp_targ_cmd_handler(struct fw_xfer *xfer)
{
	struct fw_pkt *fp;
	uint32_t *orb;
	struct corb4 *orb4;
	struct orb_info *orbi;
	struct ccb_accept_tio *atio;
	u_char *bytes;
	int i;

	orbi = (struct orb_info *)xfer->sc;
	if (xfer->resp != 0) {
		printf("%s: xfer->resp = %d\n", __func__, xfer->resp);
		orbi->status.resp = SBP_TRANS_FAIL;
		orbi->status.status = OBJ_ORB | SBE_TIMEOUT/*XXX*/;
		orbi->status.dead = 1;
		orbi->status.len = 1;
		sbp_targ_abort(orbi->sc, STAILQ_NEXT(orbi, link));

		sbp_targ_status_FIFO(orbi,
		    orbi->login->fifo_hi, orbi->login->fifo_lo, /*dequeue*/1);
		fw_xfer_free(xfer);
		return;
	}
	fp = &xfer->recv.hdr;

	atio = orbi->atio;

	if (orbi->state == ORBI_STATUS_ABORTED) {
		printf("%s: aborted\n", __func__);
		sbp_targ_remove_orb_info(orbi->login, orbi);
		free(orbi, M_SBP_TARG);
		atio->ccb_h.status = CAM_REQ_ABORTED;
		SBP_LOCK(orbi->sc);
		xpt_done((union ccb*)atio);
		SBP_UNLOCK(orbi->sc);
		goto done0;
	}
	orbi->state = ORBI_STATUS_ATIO;

	orb = orbi->orb;
	/* swap payload except SCSI command */
	for (i = 0; i < 5; i ++)
		orb[i] = ntohl(orb[i]);

	orb4 = (struct corb4 *)&orb[4];
	if (orb4->rq_fmt != 0) {
		/* XXX */
		printf("%s: rq_fmt(%d) != 0\n", __func__, orb4->rq_fmt);
	}

	atio->ccb_h.target_id = 0; /* XXX */
	atio->ccb_h.target_lun = orbi->login->lstate->lun;
	atio->sense_len = 0;
	atio->tag_action = 1; /* XXX */
	atio->tag_id = orbi->orb_lo;
	atio->init_id = orbi->login->id;

	atio->ccb_h.flags = CAM_TAG_ACTION_VALID;
	bytes = (u_char *)&orb[5];
	if (debug)
		printf("%s: %p %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		    __func__, (void *)atio,
		    bytes[0], bytes[1], bytes[2], bytes[3], bytes[4],
		    bytes[5], bytes[6], bytes[7], bytes[8], bytes[9]);
	switch (bytes[0] >> 5) {
	case 0:
		atio->cdb_len = 6;
		break;
	case 1:
	case 2:
		atio->cdb_len = 10;
		break;
	case 4:
		atio->cdb_len = 16;
		break;
	case 5:
		atio->cdb_len = 12;
		break;
	case 3:
	default:
		/* Only copy the opcode. */
		atio->cdb_len = 1;
		printf("Reserved or VU command code type encountered\n");
		break;
	}

	memcpy(atio->cdb_io.cdb_bytes, bytes, atio->cdb_len);

	atio->ccb_h.status |= CAM_CDB_RECVD;

	/* next ORB */
	if ((orb[0] & (1<<31)) == 0) {
		if (debug)
			printf("%s: fetch next orb\n", __func__);
		orbi->status.src = SRC_NEXT_EXISTS;
		sbp_targ_fetch_orb(orbi->sc, orbi->fwdev,
		    orb[0], orb[1], orbi->login, FETCH_CMD);
	} else {
		orbi->status.src = SRC_NO_NEXT;
		orbi->login->flags &= ~F_LINK_ACTIVE;
	}

	orbi->data_hi = orb[2];
	orbi->data_lo = orb[3];
	orbi->orb4 = *orb4;

	SBP_LOCK(orbi->sc);
	xpt_done((union ccb*)atio);
	SBP_UNLOCK(orbi->sc);
done0:
	fw_xfer_free(xfer);
	return;
}

static struct sbp_targ_login *
sbp_targ_get_login(struct sbp_targ_softc *sc, struct fw_device *fwdev, int lun)
{
	struct sbp_targ_lstate *lstate;
	struct sbp_targ_login *login;
	int i;

	lstate = sc->lstate[lun];
	
	STAILQ_FOREACH(login, &lstate->logins, link)
		if (login->fwdev == fwdev)
			return (login);

	for (i = 0; i < MAX_LOGINS; i ++)
		if (sc->logins[i] == NULL)
			goto found;

	printf("%s: increase MAX_LOGIN\n", __func__);
	return (NULL);

found:
	login = (struct sbp_targ_login *)malloc(
	    sizeof(struct sbp_targ_login), M_SBP_TARG, M_NOWAIT | M_ZERO);

	if (login == NULL) {
		printf("%s: malloc failed\n", __func__);
		return (NULL);
	}

	login->id = i;
	login->fwdev = fwdev;
	login->lstate = lstate;
	login->last_hi = 0xffff;
	login->last_lo = 0xffffffff;
	login->hold_sec = 1;
	STAILQ_INIT(&login->orbs);
	CALLOUT_INIT(&login->hold_callout);
	sc->logins[i] = login;
	return (login);
}

static void
sbp_targ_mgm_handler(struct fw_xfer *xfer)
{
	struct sbp_targ_lstate *lstate;
	struct sbp_targ_login *login;
	struct fw_pkt *fp;
	uint32_t *orb;
	struct morb4 *orb4;
	struct orb_info *orbi;
	int i;

	orbi = (struct orb_info *)xfer->sc;
	if (xfer->resp != 0) {
		printf("%s: xfer->resp = %d\n", __func__, xfer->resp);
		orbi->status.resp = SBP_TRANS_FAIL;
		orbi->status.status = OBJ_ORB | SBE_TIMEOUT/*XXX*/;
		orbi->status.dead = 1;
		orbi->status.len = 1;
		sbp_targ_abort(orbi->sc, STAILQ_NEXT(orbi, link));

		sbp_targ_status_FIFO(orbi,
		    orbi->login->fifo_hi, orbi->login->fifo_lo, /*dequeue*/0);
		fw_xfer_free(xfer);
		return;
	}
	fp = &xfer->recv.hdr;

	orb = orbi->orb;
	/* swap payload */
	for (i = 0; i < 8; i ++) {
		orb[i] = ntohl(orb[i]);
	}
	orb4 = (struct morb4 *)&orb[4];
	if (debug)
		printf("%s: %s\n", __func__, orb_fun_name[orb4->fun]);

	orbi->status.src = SRC_NO_NEXT;

	switch (orb4->fun << 16) {
	case ORB_FUN_LGI:
	{
		int exclusive = 0, lun;

		if (orb[4] & ORB_EXV)
			exclusive = 1;

		lun = orb4->id;
		lstate = orbi->sc->lstate[lun];

		if (lun >= MAX_LUN || lstate == NULL ||
		    (exclusive && 
		    STAILQ_FIRST(&lstate->logins) != NULL &&
		    STAILQ_FIRST(&lstate->logins)->fwdev != orbi->fwdev)
		    ) {
			/* error */
			orbi->status.dead = 1;
			orbi->status.status = STATUS_ACCESS_DENY;
			orbi->status.len = 1;
			break;
		}

		/* allocate login */
		login = sbp_targ_get_login(orbi->sc, orbi->fwdev, lun);
		if (login == NULL) {
			printf("%s: sbp_targ_get_login failed\n",
			    __func__);
			orbi->status.dead = 1;
			orbi->status.status = STATUS_RES_UNAVAIL;
			orbi->status.len = 1;
			break;
		}
		printf("%s: login id=%d\n", __func__, login->id);

		login->fifo_hi = orb[6];
		login->fifo_lo = orb[7];
		login->loginres.len = htons(sizeof(uint32_t) * 4);
		login->loginres.id = htons(login->id);
		login->loginres.cmd_hi = htons(SBP_TARG_BIND_HI);
		login->loginres.cmd_lo = htonl(SBP_TARG_BIND_LO(login->id));
		login->loginres.recon_hold = htons(login->hold_sec);

		STAILQ_INSERT_TAIL(&lstate->logins, login, link);
		fwmem_write_block(orbi->fwdev, NULL, /*spd*/2, orb[2], orb[3],
		    sizeof(struct sbp_login_res), (void *)&login->loginres,
		    fw_asy_callback_free);
		/* XXX return status after loginres is successfully written */
		break;
	}
	case ORB_FUN_RCN:
		login = orbi->sc->logins[orb4->id];
		if (login != NULL && login->fwdev == orbi->fwdev) {
			login->flags &= ~F_HOLD;
			callout_stop(&login->hold_callout);
			printf("%s: reconnected id=%d\n",
			    __func__, login->id);
		} else {
			orbi->status.dead = 1;
			orbi->status.status = STATUS_ACCESS_DENY;
			printf("%s: reconnection faild id=%d\n",
			    __func__, orb4->id);
		}
		break;
	case ORB_FUN_LGO:
		login = orbi->sc->logins[orb4->id];
		if (login->fwdev != orbi->fwdev) {
			printf("%s: wrong initiator\n", __func__);
			break;
		}
		sbp_targ_dealloc_login(login);
		break;
	default:
		printf("%s: %s not implemented yet\n",
		    __func__, orb_fun_name[orb4->fun]);
		break;
	}
	orbi->status.len = 1;
	sbp_targ_status_FIFO(orbi, orb[6], orb[7], /*dequeue*/0);
	fw_xfer_free(xfer);
	return;
}

static void
sbp_targ_pointer_handler(struct fw_xfer *xfer)
{
	struct orb_info *orbi;
	uint32_t orb0, orb1;

	orbi = (struct orb_info *)xfer->sc;
	if (xfer->resp != 0) {
		printf("%s: xfer->resp = %d\n", __func__, xfer->resp);
		goto done;
	}

	orb0 = ntohl(orbi->orb[0]);
	orb1 = ntohl(orbi->orb[1]);
	if ((orb0 & (1 << 31)) != 0) {
		printf("%s: invalid pointer\n", __func__);
		goto done;
	}
	sbp_targ_fetch_orb(orbi->login->lstate->sc, orbi->fwdev,
	    (uint16_t)orb0, orb1, orbi->login, FETCH_CMD);
done:
	free(orbi, M_SBP_TARG);
	fw_xfer_free(xfer);
	return;
}

static void
sbp_targ_fetch_orb(struct sbp_targ_softc *sc, struct fw_device *fwdev,
    uint16_t orb_hi, uint32_t orb_lo, struct sbp_targ_login *login,
    int mode)
{
	struct orb_info *orbi;

	if (debug)
		printf("%s: fetch orb %04x:%08x\n", __func__, orb_hi, orb_lo);
	orbi = malloc(sizeof(struct orb_info), M_SBP_TARG, M_NOWAIT | M_ZERO);
	if (orbi == NULL) {
		printf("%s: malloc failed\n", __func__);
		return;
	}
	orbi->sc = sc;
	orbi->fwdev = fwdev;
	orbi->login = login;
	orbi->orb_hi = orb_hi;
	orbi->orb_lo = orb_lo;
	orbi->status.orb_hi = htons(orb_hi);
	orbi->status.orb_lo = htonl(orb_lo);

	switch (mode) {
	case FETCH_MGM:
		fwmem_read_block(fwdev, (void *)orbi, /*spd*/2, orb_hi, orb_lo,
		    sizeof(uint32_t) * 8, &orbi->orb[0],
		    sbp_targ_mgm_handler);
		break;
	case FETCH_CMD:
		orbi->state = ORBI_STATUS_FETCH;
		login->last_hi = orb_hi;
		login->last_lo = orb_lo;
		login->flags |= F_LINK_ACTIVE;
		/* dequeue */
		SBP_LOCK(sc);
		orbi->atio = (struct ccb_accept_tio *)
		    SLIST_FIRST(&login->lstate->accept_tios);
		if (orbi->atio == NULL) {
			SBP_UNLOCK(sc);
			printf("%s: no free atio\n", __func__);
			login->lstate->flags |= F_ATIO_STARVED;
			login->flags |= F_ATIO_STARVED;
#if 0
			/* XXX ?? */
			login->fwdev = fwdev;
#endif
			break;
		}
		SLIST_REMOVE_HEAD(&login->lstate->accept_tios, sim_links.sle);
		STAILQ_INSERT_TAIL(&login->orbs, orbi, link);
		SBP_UNLOCK(sc);
		fwmem_read_block(fwdev, (void *)orbi, /*spd*/2, orb_hi, orb_lo,
		    sizeof(uint32_t) * 8, &orbi->orb[0],
		    sbp_targ_cmd_handler);
		break;
	case FETCH_POINTER:
		orbi->state = ORBI_STATUS_POINTER;
		login->flags |= F_LINK_ACTIVE;
		fwmem_read_block(fwdev, (void *)orbi, /*spd*/2, orb_hi, orb_lo,
		    sizeof(uint32_t) * 2, &orbi->orb[0],
		    sbp_targ_pointer_handler);
		break;
	default:
		printf("%s: invalid mode %d\n", __func__, mode);
	}
}

static void
sbp_targ_resp_callback(struct fw_xfer *xfer)
{
	struct sbp_targ_softc *sc;
	int s;

	if (debug)
		printf("%s: xfer=%p\n", __func__, xfer);
	sc = (struct sbp_targ_softc *)xfer->sc;
	fw_xfer_unload(xfer);
	xfer->recv.pay_len = SBP_TARG_RECV_LEN;
	xfer->hand = sbp_targ_recv;
	s = splfw();
	STAILQ_INSERT_TAIL(&sc->fwb.xferlist, xfer, link);
	splx(s);
}

static int
sbp_targ_cmd(struct fw_xfer *xfer, struct fw_device *fwdev, int login_id,
    int reg)
{
	struct sbp_targ_login *login;
	struct sbp_targ_softc *sc;
	int rtcode = 0;

	if (login_id < 0 || login_id >= MAX_LOGINS)
		return(RESP_ADDRESS_ERROR);

	sc = (struct sbp_targ_softc *)xfer->sc;
	login = sc->logins[login_id];
	if (login == NULL)
		return(RESP_ADDRESS_ERROR);

	if (login->fwdev != fwdev) {
		/* XXX */
		return(RESP_ADDRESS_ERROR);
	}

	switch (reg) {
	case 0x08:	/* ORB_POINTER */
		if (debug)
			printf("%s: ORB_POINTER(%d)\n", __func__, login_id);
		if ((login->flags & F_LINK_ACTIVE) != 0) {
			if (debug)
				printf("link active (ORB_POINTER)\n");
			break;
		}
		sbp_targ_fetch_orb(sc, fwdev,
		    ntohl(xfer->recv.payload[0]),
		    ntohl(xfer->recv.payload[1]),
		    login, FETCH_CMD);
		break;
	case 0x04:	/* AGENT_RESET */
		if (debug)
			printf("%s: AGENT RESET(%d)\n", __func__, login_id);
		login->last_hi = 0xffff;
		login->last_lo = 0xffffffff;
		sbp_targ_abort(sc, STAILQ_FIRST(&login->orbs));
		break;
	case 0x10:	/* DOORBELL */
		if (debug)
			printf("%s: DOORBELL(%d)\n", __func__, login_id);
		if (login->last_hi == 0xffff &&
		    login->last_lo == 0xffffffff) {
			printf("%s: no previous pointer(DOORBELL)\n",
			    __func__);
			break;
		}
		if ((login->flags & F_LINK_ACTIVE) != 0) {
			if (debug)
				printf("link active (DOORBELL)\n");
			break;
		}
		sbp_targ_fetch_orb(sc, fwdev,
		    login->last_hi, login->last_lo,
		    login, FETCH_POINTER);
		break;
	case 0x00:	/* AGENT_STATE */
		printf("%s: AGENT_STATE (%d:ignore)\n", __func__, login_id);
		break;
	case 0x14:	/* UNSOLICITED_STATE_ENABLE */
		printf("%s: UNSOLICITED_STATE_ENABLE (%d:ignore)\n",
							 __func__, login_id);
		break;
	default:
		printf("%s: invalid register %d(%d)\n",
						 __func__, reg, login_id);
		rtcode = RESP_ADDRESS_ERROR;
	}

	return (rtcode);
}

static int
sbp_targ_mgm(struct fw_xfer *xfer, struct fw_device *fwdev)
{
	struct sbp_targ_softc *sc;
	struct fw_pkt *fp;

	sc = (struct sbp_targ_softc *)xfer->sc;

	fp = &xfer->recv.hdr;
	if (fp->mode.wreqb.tcode != FWTCODE_WREQB){
		printf("%s: tcode = %d\n", __func__, fp->mode.wreqb.tcode);
		return(RESP_TYPE_ERROR);
        }

	sbp_targ_fetch_orb(sc, fwdev,
	    ntohl(xfer->recv.payload[0]),
	    ntohl(xfer->recv.payload[1]),
	    NULL, FETCH_MGM);
	
	return(0);
}

static void
sbp_targ_recv(struct fw_xfer *xfer)
{
	struct fw_pkt *fp, *sfp;
	struct fw_device *fwdev;
	uint32_t lo;
	int s, rtcode;
	struct sbp_targ_softc *sc;

	s = splfw();
	sc = (struct sbp_targ_softc *)xfer->sc;
	fp = &xfer->recv.hdr;
	fwdev = fw_noderesolve_nodeid(sc->fd.fc, fp->mode.wreqb.src & 0x3f);
	if (fwdev == NULL) {
		printf("%s: cannot resolve nodeid=%d\n",
		    __func__, fp->mode.wreqb.src & 0x3f);
		rtcode = RESP_TYPE_ERROR; /* XXX */
		goto done;
	}
	lo = fp->mode.wreqb.dest_lo;

	if (lo == SBP_TARG_BIND_LO(-1))
		rtcode = sbp_targ_mgm(xfer, fwdev);
	else if (lo >= SBP_TARG_BIND_LO(0))
		rtcode = sbp_targ_cmd(xfer, fwdev, SBP_TARG_LOGIN_ID(lo),
		    lo % 0x20);
	else
		rtcode = RESP_ADDRESS_ERROR;

done:
	if (rtcode != 0)
		printf("%s: rtcode = %d\n", __func__, rtcode);
	sfp = &xfer->send.hdr;
	xfer->send.spd = 2; /* XXX */
	xfer->hand = sbp_targ_resp_callback;
	sfp->mode.wres.dst = fp->mode.wreqb.src;
	sfp->mode.wres.tlrt = fp->mode.wreqb.tlrt;
	sfp->mode.wres.tcode = FWTCODE_WRES;
	sfp->mode.wres.rtcode = rtcode;
	sfp->mode.wres.pri = 0;

	fw_asyreq(xfer->fc, -1, xfer);
	splx(s);
}

static int
sbp_targ_attach(device_t dev)
{
	struct sbp_targ_softc *sc;
	struct cam_devq *devq;
	struct firewire_comm *fc;

        sc = (struct sbp_targ_softc *) device_get_softc(dev);
	bzero((void *)sc, sizeof(struct sbp_targ_softc));

	mtx_init(&sc->mtx, "sbp_targ", NULL, MTX_DEF);
	sc->fd.fc = fc = device_get_ivars(dev);
	sc->fd.dev = dev;
	sc->fd.post_explore = (void *) sbp_targ_post_explore;
	sc->fd.post_busreset = (void *) sbp_targ_post_busreset;

        devq = cam_simq_alloc(/*maxopenings*/MAX_LUN*MAX_INITIATORS);
	if (devq == NULL)
		return (ENXIO);

	sc->sim = cam_sim_alloc(sbp_targ_action, sbp_targ_poll,
	    "sbp_targ", sc, device_get_unit(dev), &sc->mtx,
	    /*untagged*/ 1, /*tagged*/ 1, devq);
	if (sc->sim == NULL) {
		cam_simq_free(devq);
		return (ENXIO);
	}

	SBP_LOCK(sc);
	if (xpt_bus_register(sc->sim, /*bus*/0) != CAM_SUCCESS)
		goto fail;

	if (xpt_create_path(&sc->path, /*periph*/ NULL, cam_sim_path(sc->sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sc->sim));
		goto fail;
	}
	SBP_UNLOCK(sc);

	sc->fwb.start = SBP_TARG_BIND_START;
	sc->fwb.end = SBP_TARG_BIND_END;

	/* pre-allocate xfer */
	STAILQ_INIT(&sc->fwb.xferlist);
	fw_xferlist_add(&sc->fwb.xferlist, M_SBP_TARG,
	    /*send*/ 0, /*recv*/ SBP_TARG_RECV_LEN, MAX_LUN /* XXX */,
	    fc, (void *)sc, sbp_targ_recv);
	fw_bindadd(fc, &sc->fwb);
	return 0;

fail:
	SBP_UNLOCK(sc);
	cam_sim_free(sc->sim, /*free_devq*/TRUE);
	return (ENXIO);
}

static int
sbp_targ_detach(device_t dev)
{
	struct sbp_targ_softc *sc;
	struct sbp_targ_lstate *lstate;
	int i;

	sc = (struct sbp_targ_softc *)device_get_softc(dev);
	sc->fd.post_busreset = NULL;

	SBP_LOCK(sc);
	xpt_free_path(sc->path);
	xpt_bus_deregister(cam_sim_path(sc->sim));
	SBP_UNLOCK(sc);
	cam_sim_free(sc->sim, /*free_devq*/TRUE); 

	for (i = 0; i < MAX_LUN; i ++) {
		lstate = sc->lstate[i];
		if (lstate != NULL) {
			xpt_free_path(lstate->path);
			free(lstate, M_SBP_TARG);
		}
	}
	if (sc->black_hole != NULL) {
		xpt_free_path(sc->black_hole->path);
		free(sc->black_hole, M_SBP_TARG);
	}
			
	fw_bindremove(sc->fd.fc, &sc->fwb);
	fw_xferlist_remove(&sc->fwb.xferlist);

	mtx_destroy(&sc->mtx);

	return 0;
}

static devclass_t sbp_targ_devclass;

static device_method_t sbp_targ_methods[] = {
	/* device interface */
	DEVMETHOD(device_identify,	sbp_targ_identify),
	DEVMETHOD(device_probe,		sbp_targ_probe),
	DEVMETHOD(device_attach,	sbp_targ_attach),
	DEVMETHOD(device_detach,	sbp_targ_detach),
	{ 0, 0 }
};

static driver_t sbp_targ_driver = {
	"sbp_targ",
	sbp_targ_methods,
	sizeof(struct sbp_targ_softc),
};

DRIVER_MODULE(sbp_targ, firewire, sbp_targ_driver, sbp_targ_devclass, 0, 0);
MODULE_VERSION(sbp_targ, 1);
MODULE_DEPEND(sbp_targ, firewire, 1, 1, 1);
MODULE_DEPEND(sbp_targ, cam, 1, 1, 1);
