/*
 * Copyright (c) 2003 Hidetosh Shimokawa
 * Copyright (c) 1998-2002 Katsushi Kobayashi and Hidetosh Shimokawa
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the acknowledgement as bellow:
 *
 *    This product includes software developed by K. Kobayashi and H. Shimokawa
 *
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD$
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/sysctl.h>
#include <machine/bus.h>
#include <sys/malloc.h>
#if __FreeBSD_version >= 501102
#include <sys/lock.h>
#include <sys/mutex.h>
#endif

#if __FreeBSD_version < 500106
#include <sys/devicestat.h>	/* for struct devstat */
#endif

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/cam_periph.h>

#include <cam/scsi/scsi_all.h>

#include <sys/kernel.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/fwdma.h>
#include <dev/firewire/iec13213.h>
#include <dev/firewire/sbp.h>

#define ccb_sdev_ptr	spriv_ptr0
#define ccb_sbp_ptr	spriv_ptr1

#define SBP_NUM_TARGETS 8 /* MAX 64 */
/*
 * Scan_bus doesn't work for more than 8 LUNs
 * because of CAM_SCSI2_MAXLUN in cam_xpt.c
 */
#define SBP_NUM_LUNS 64
#define SBP_DMA_SIZE PAGE_SIZE
#define SBP_LOGIN_SIZE sizeof(struct sbp_login_res)
#define SBP_QUEUE_LEN ((SBP_DMA_SIZE - SBP_LOGIN_SIZE) / sizeof(struct sbp_ocb))
#define SBP_NUM_OCB (SBP_QUEUE_LEN * SBP_NUM_TARGETS)

/* 
 * STATUS FIFO addressing
 *   bit
 * -----------------------
 *  0- 1( 2): 0 (alingment)
 *  2- 7( 6): target
 *  8-15( 8): lun
 * 16-31( 8): reserved
 * 32-47(16): SBP_BIND_HI 
 * 48-64(16): bus_id, node_id 
 */
#define SBP_BIND_HI 0x1
#define SBP_DEV2ADDR(t, l) \
	(((u_int64_t)SBP_BIND_HI << 32) \
	| (((l) & 0xff) << 8) \
	| (((t) & 0x3f) << 2))
#define SBP_ADDR2TRG(a)	(((a) >> 2) & 0x3f)
#define SBP_ADDR2LUN(a)	(((a) >> 8) & 0xff)
#define SBP_INITIATOR 7

static char *orb_fun_name[] = {
	ORB_FUN_NAMES
};

static int debug = 0;
static int auto_login = 1;
static int max_speed = 2;
static int sbp_cold = 1;
static int ex_login = 1;
static int login_delay = 1000;	/* msec */
static int scan_delay = 500;	/* msec */

SYSCTL_DECL(_hw_firewire);
SYSCTL_NODE(_hw_firewire, OID_AUTO, sbp, CTLFLAG_RD, 0, "SBP-II Subsystem");
SYSCTL_INT(_debug, OID_AUTO, sbp_debug, CTLFLAG_RW, &debug, 0,
	"SBP debug flag");
SYSCTL_INT(_hw_firewire_sbp, OID_AUTO, auto_login, CTLFLAG_RW, &auto_login, 0,
	"SBP perform login automatically");
SYSCTL_INT(_hw_firewire_sbp, OID_AUTO, max_speed, CTLFLAG_RW, &max_speed, 0,
	"SBP transfer max speed");
SYSCTL_INT(_hw_firewire_sbp, OID_AUTO, exclusive_login, CTLFLAG_RW,
	&ex_login, 0, "SBP transfer max speed");
SYSCTL_INT(_hw_firewire_sbp, OID_AUTO, login_delay, CTLFLAG_RW,
	&login_delay, 0, "SBP login delay in msec");
SYSCTL_INT(_hw_firewire_sbp, OID_AUTO, scan_delay, CTLFLAG_RW,
	&scan_delay, 0, "SBP scan delay in msec");

TUNABLE_INT("hw.firewire.sbp.auto_login", &auto_login);
TUNABLE_INT("hw.firewire.sbp.max_speed", &max_speed);
TUNABLE_INT("hw.firewire.sbp.exclusive_login", &ex_login);
TUNABLE_INT("hw.firewire.sbp.login_delay", &login_delay);
TUNABLE_INT("hw.firewire.sbp.scan_delay", &scan_delay);

#define NEED_RESPONSE 0

#define SBP_SEG_MAX rounddown(0xffff, PAGE_SIZE)
#ifdef __sparc64__ /* iommu */
#define SBP_IND_MAX howmany(MAXPHYS, SBP_SEG_MAX)
#else
#define SBP_IND_MAX howmany(MAXPHYS, PAGE_SIZE)
#endif
struct sbp_ocb {
	STAILQ_ENTRY(sbp_ocb)	ocb;
	union ccb	*ccb;
	bus_addr_t	bus_addr;
	u_int32_t	orb[8];
#define IND_PTR_OFFSET	(8*sizeof(u_int32_t))
	struct ind_ptr  ind_ptr[SBP_IND_MAX];
	struct sbp_dev	*sdev;
	int		flags; /* XXX should be removed */
	bus_dmamap_t	dmamap;
};

#define OCB_ACT_MGM 0
#define OCB_ACT_CMD 1
#define OCB_MATCH(o,s)	((o)->bus_addr == ntohl((s)->orb_lo))

struct sbp_dev{
#define SBP_DEV_RESET		0	/* accept login */
#define SBP_DEV_LOGIN		1	/* to login */
#if 0
#define SBP_DEV_RECONN		2	/* to reconnect */
#endif
#define SBP_DEV_TOATTACH	3	/* to attach */
#define SBP_DEV_PROBE		4	/* scan lun */
#define SBP_DEV_ATTACHED	5	/* in operation */
#define SBP_DEV_DEAD		6	/* unavailable unit */
#define SBP_DEV_RETRY		7	/* unavailable unit */
	u_int8_t status:4,
		 timeout:4;
	u_int8_t type;
	u_int16_t lun_id;
	u_int16_t freeze;
#define	ORB_LINK_DEAD		(1 << 0)
#define	VALID_LUN		(1 << 1)
#define	ORB_POINTER_ACTIVE	(1 << 2)
#define	ORB_POINTER_NEED	(1 << 3)
	u_int16_t flags;
	struct cam_path *path;
	struct sbp_target *target;
	struct fwdma_alloc dma;
	struct sbp_login_res *login;
	struct callout login_callout;
	struct sbp_ocb *ocb;
	STAILQ_HEAD(, sbp_ocb) ocbs;
	STAILQ_HEAD(, sbp_ocb) free_ocbs;
	char vendor[32];
	char product[32];
	char revision[10];
};

struct sbp_target {
	int target_id;
	int num_lun;
	struct sbp_dev	**luns;
	struct sbp_softc *sbp;
	struct fw_device *fwdev;
	u_int32_t mgm_hi, mgm_lo;
	struct sbp_ocb *mgm_ocb_cur;
	STAILQ_HEAD(, sbp_ocb) mgm_ocb_queue;
	struct callout mgm_ocb_timeout;
	struct callout scan_callout;
	STAILQ_HEAD(, fw_xfer) xferlist;
	int n_xfer;
};

struct sbp_softc {
	struct firewire_dev_comm fd;
	struct cam_sim  *sim;
	struct cam_path  *path;
	struct sbp_target targets[SBP_NUM_TARGETS];
	struct fw_bind fwb;
	bus_dma_tag_t	dmat;
	struct timeval last_busreset;
};

static void sbp_post_explore __P((void *));
static void sbp_recv __P((struct fw_xfer *));
static void sbp_mgm_callback __P((struct fw_xfer *));
#if 0
static void sbp_cmd_callback __P((struct fw_xfer *));
#endif
static void sbp_orb_pointer __P((struct sbp_dev *, struct sbp_ocb *));
static void sbp_execute_ocb __P((void *,  bus_dma_segment_t *, int, int));
static void sbp_free_ocb __P((struct sbp_dev *, struct sbp_ocb *));
static void sbp_abort_ocb __P((struct sbp_ocb *, int));
static void sbp_abort_all_ocbs __P((struct sbp_dev *, int));
static struct fw_xfer * sbp_write_cmd __P((struct sbp_dev *, int, int));
static struct sbp_ocb * sbp_get_ocb __P((struct sbp_dev *));
static struct sbp_ocb * sbp_enqueue_ocb __P((struct sbp_dev *, struct sbp_ocb *));
static struct sbp_ocb * sbp_dequeue_ocb __P((struct sbp_dev *, struct sbp_status *));
static void sbp_cam_detach_sdev(struct sbp_dev *);
static void sbp_free_sdev(struct sbp_dev *);
static void sbp_cam_detach_target __P((struct sbp_target *));
static void sbp_free_target __P((struct sbp_target *));
static void sbp_mgm_timeout __P((void *arg));
static void sbp_timeout __P((void *arg));
static void sbp_mgm_orb __P((struct sbp_dev *, int, struct sbp_ocb *));

MALLOC_DEFINE(M_SBP, "sbp", "SBP-II/FireWire");

/* cam related functions */
static void	sbp_action(struct cam_sim *sim, union ccb *ccb);
static void	sbp_poll(struct cam_sim *sim);
static void	sbp_cam_scan_lun(struct cam_periph *, union ccb *);
static void	sbp_cam_scan_target(void *arg);

static char *orb_status0[] = {
	/* 0 */ "No additional information to report",
	/* 1 */ "Request type not supported",
	/* 2 */ "Speed not supported",
	/* 3 */ "Page size not supported",
	/* 4 */ "Access denied",
	/* 5 */ "Logical unit not supported",
	/* 6 */ "Maximum payload too small",
	/* 7 */ "Reserved for future standardization",
	/* 8 */ "Resources unavailable",
	/* 9 */ "Function rejected",
	/* A */ "Login ID not recognized",
	/* B */ "Dummy ORB completed",
	/* C */ "Request aborted",
	/* FF */ "Unspecified error"
#define MAX_ORB_STATUS0 0xd
};

static char *orb_status1_object[] = {
	/* 0 */ "Operation request block (ORB)",
	/* 1 */ "Data buffer",
	/* 2 */ "Page table",
	/* 3 */ "Unable to specify"
};

static char *orb_status1_serial_bus_error[] = {
	/* 0 */ "Missing acknowledge",
	/* 1 */ "Reserved; not to be used",
	/* 2 */ "Time-out error",
	/* 3 */ "Reserved; not to be used",
	/* 4 */ "Busy retry limit exceeded(X)",
	/* 5 */ "Busy retry limit exceeded(A)",
	/* 6 */ "Busy retry limit exceeded(B)",
	/* 7 */ "Reserved for future standardization",
	/* 8 */ "Reserved for future standardization",
	/* 9 */ "Reserved for future standardization",
	/* A */ "Reserved for future standardization",
	/* B */ "Tardy retry limit exceeded",
	/* C */ "Conflict error",
	/* D */ "Data error",
	/* E */ "Type error",
	/* F */ "Address error"
};

static void
sbp_identify(driver_t *driver, device_t parent)
{
	device_t child;
SBP_DEBUG(0)
	printf("sbp_identify\n");
END_DEBUG

	child = BUS_ADD_CHILD(parent, 0, "sbp", device_get_unit(parent));
}

/*
 * sbp_probe()
 */
static int
sbp_probe(device_t dev)
{
	device_t pa;

SBP_DEBUG(0)
	printf("sbp_probe\n");
END_DEBUG

	pa = device_get_parent(dev);
	if(device_get_unit(dev) != device_get_unit(pa)){
		return(ENXIO);
	}

	device_set_desc(dev, "SBP-2/SCSI over FireWire");

	if (bootverbose)
		debug = bootverbose;
	return (0);
}

static void
sbp_show_sdev_info(struct sbp_dev *sdev, int new)
{
	struct fw_device *fwdev;

	printf("%s:%d:%d ",
		device_get_nameunit(sdev->target->sbp->fd.dev),
		sdev->target->target_id,
		sdev->lun_id
	);
	if (new == 2) {
		return;
	}
	fwdev = sdev->target->fwdev;
	printf("ordered:%d type:%d EUI:%08x%08x node:%d "
		"speed:%d maxrec:%d",
		(sdev->type & 0x40) >> 6,
		(sdev->type & 0x1f),
		fwdev->eui.hi,
		fwdev->eui.lo,
		fwdev->dst,
		fwdev->speed,
		fwdev->maxrec
	);
	if (new)
		printf(" new!\n");
	else
		printf("\n");
	sbp_show_sdev_info(sdev, 2);
	printf("'%s' '%s' '%s'\n", sdev->vendor, sdev->product, sdev->revision);
}

static struct {
	int bus;
	int target;
	struct fw_eui64 eui;
} wired[] = {
	/* Bus	Target	EUI64 */
#if 0
	{0,	2,	{0x00018ea0, 0x01fd0154}},	/* Logitec HDD */
	{0,	0,	{0x00018ea6, 0x00100682}},	/* Logitec DVD */
	{0,	1,	{0x00d03200, 0xa412006a}},	/* Yano HDD */
#endif
	{-1,	-1,	{0,0}}
};

static int
sbp_new_target(struct sbp_softc *sbp, struct fw_device *fwdev)
{
	int bus, i, target=-1;
	char w[SBP_NUM_TARGETS];

	bzero(w, sizeof(w));
	bus = device_get_unit(sbp->fd.dev);

	/* XXX wired-down configuration should be gotten from
					tunable or device hint */
	for (i = 0; wired[i].bus >= 0; i ++) {
		if (wired[i].bus == bus) {
			w[wired[i].target] = 1;
			if (wired[i].eui.hi == fwdev->eui.hi &&
					wired[i].eui.lo == fwdev->eui.lo)
				target = wired[i].target;
		}
	}
	if (target >= 0) {
		if(target < SBP_NUM_TARGETS &&
				sbp->targets[target].fwdev == NULL)
			return(target);
		device_printf(sbp->fd.dev,
			"target %d is not free for %08x:%08x\n", 
			target, fwdev->eui.hi, fwdev->eui.lo);
		target = -1;
	}
	/* non-wired target */
	for (i = 0; i < SBP_NUM_TARGETS; i ++)
		if (sbp->targets[i].fwdev == NULL && w[i] == 0) {
			target = i;
			break;
		}

	return target;
}

static void
sbp_alloc_lun(struct sbp_target *target)
{
	struct crom_context cc;
	struct csrreg *reg;
	struct sbp_dev *sdev, **newluns;
	struct sbp_softc *sbp;
	int maxlun, lun, i;

	sbp = target->sbp;
	crom_init_context(&cc, target->fwdev->csrrom);
	/* XXX shoud parse appropriate unit directories only */
	maxlun = -1;
	while (cc.depth >= 0) {
		reg = crom_search_key(&cc, CROM_LUN);
		if (reg == NULL)
			break;
		lun = reg->val & 0xffff;
SBP_DEBUG(0)
		printf("target %d lun %d found\n", target->target_id, lun);
END_DEBUG
		if (maxlun < lun)
			maxlun = lun;
		crom_next(&cc);
	}
	if (maxlun < 0)
		printf("%s:%d no LUN found\n",
		    device_get_nameunit(target->sbp->fd.dev),
		    target->target_id);

	maxlun ++;
	if (maxlun >= SBP_NUM_LUNS)
		maxlun = SBP_NUM_LUNS;

	/* Invalidiate stale devices */
	for (lun = 0; lun < target->num_lun; lun ++) {
		sdev = target->luns[lun];
		if (sdev == NULL)
			continue;
		sdev->flags &= ~VALID_LUN;
		if (lun >= maxlun) {
			/* lost device */
			sbp_cam_detach_sdev(sdev);
			sbp_free_sdev(sdev);
		}
	}

	/* Reallocate */
	if (maxlun != target->num_lun) {
		newluns = (struct sbp_dev **) realloc(target->luns,
		    sizeof(struct sbp_dev *) * maxlun,
		    M_SBP, M_NOWAIT | M_ZERO);
		
		if (newluns == NULL) {
			printf("%s: realloc failed\n", __FUNCTION__);
			newluns = target->luns;
			maxlun = target->num_lun;
		}

		/*
		 * We must zero the extended region for the case
		 * realloc() doesn't allocate new buffer.
		 */
		if (maxlun > target->num_lun)
			bzero(&newluns[target->num_lun],
			    sizeof(struct sbp_dev *) *
			    (maxlun - target->num_lun));

		target->luns = newluns;
		target->num_lun = maxlun;
	}

	crom_init_context(&cc, target->fwdev->csrrom);
	while (cc.depth >= 0) {
		int new = 0;

		reg = crom_search_key(&cc, CROM_LUN);
		if (reg == NULL)
			break;
		lun = reg->val & 0xffff;
		if (lun >= SBP_NUM_LUNS) {
			printf("too large lun %d\n", lun);
			goto next;
		}

		sdev = target->luns[lun];
		if (sdev == NULL) {
			sdev = malloc(sizeof(struct sbp_dev),
			    M_SBP, M_NOWAIT | M_ZERO);
			if (sdev == NULL) {
				printf("%s: malloc failed\n", __FUNCTION__);
				goto next;
			}
			target->luns[lun] = sdev;
			sdev->lun_id = lun;
			sdev->target = target;
			STAILQ_INIT(&sdev->ocbs);
			CALLOUT_INIT(&sdev->login_callout);
			sdev->status = SBP_DEV_RESET;
			new = 1;
		}
		sdev->flags |= VALID_LUN;
		sdev->type = (reg->val & 0xff0000) >> 16;

		if (new == 0)
			goto next;

		fwdma_malloc(sbp->fd.fc, 
			/* alignment */ sizeof(u_int32_t),
			SBP_DMA_SIZE, &sdev->dma, BUS_DMA_NOWAIT);
		if (sdev->dma.v_addr == NULL) {
			printf("%s: dma space allocation failed\n",
							__FUNCTION__);
			free(sdev, M_SBP);
			target->luns[lun] = NULL;
			goto next;
		}
		sdev->login = (struct sbp_login_res *) sdev->dma.v_addr;
		sdev->ocb = (struct sbp_ocb *)
				((char *)sdev->dma.v_addr + SBP_LOGIN_SIZE);
		bzero((char *)sdev->ocb,
			sizeof (struct sbp_ocb) * SBP_QUEUE_LEN);

		STAILQ_INIT(&sdev->free_ocbs);
		for (i = 0; i < SBP_QUEUE_LEN; i++) {
			struct sbp_ocb *ocb;
			ocb = &sdev->ocb[i];
			ocb->bus_addr = sdev->dma.bus_addr
				+ SBP_LOGIN_SIZE
				+ sizeof(struct sbp_ocb) * i
				+ offsetof(struct sbp_ocb, orb[0]);
			if (bus_dmamap_create(sbp->dmat, 0, &ocb->dmamap)) {
				printf("sbp_attach: cannot create dmamap\n");
				/* XXX */
				goto next;
			}
			sbp_free_ocb(sdev, ocb);
		}
next:
		crom_next(&cc);
	}

	for (lun = 0; lun < target->num_lun; lun ++) {
		sdev = target->luns[lun];
		if (sdev != NULL && (sdev->flags & VALID_LUN) == 0) {
			sbp_cam_detach_sdev(sdev);
			sbp_free_sdev(sdev);
			target->luns[lun] = NULL;
		}
	}
}

static struct sbp_target *
sbp_alloc_target(struct sbp_softc *sbp, struct fw_device *fwdev)
{
	int i;
	struct sbp_target *target;
	struct crom_context cc;
	struct csrreg *reg;

SBP_DEBUG(1)
	printf("sbp_alloc_target\n");
END_DEBUG
	i = sbp_new_target(sbp, fwdev);
	if (i < 0) {
		device_printf(sbp->fd.dev, "increase SBP_NUM_TARGETS!\n");
		return NULL;
	}
	/* new target */
	target = &sbp->targets[i];
	target->sbp = sbp;
	target->fwdev = fwdev;
	target->target_id = i;
	/* XXX we may want to reload mgm port after each bus reset */
	/* XXX there might be multiple management agents */
	crom_init_context(&cc, target->fwdev->csrrom);
	reg = crom_search_key(&cc, CROM_MGM);
	if (reg == NULL || reg->val == 0) {
		printf("NULL management address\n");
		target->fwdev = NULL;
		return NULL;
	}
	target->mgm_hi = 0xffff;
	target->mgm_lo = 0xf0000000 | (reg->val << 2);
	target->mgm_ocb_cur = NULL;
SBP_DEBUG(1)
	printf("target:%d mgm_port: %x\n", i, target->mgm_lo);
END_DEBUG
	STAILQ_INIT(&target->xferlist);
	target->n_xfer = 0;
	STAILQ_INIT(&target->mgm_ocb_queue);
	CALLOUT_INIT(&target->mgm_ocb_timeout);
	CALLOUT_INIT(&target->scan_callout);

	target->luns = NULL;
	target->num_lun = 0;
	return target;
}

static void
sbp_probe_lun(struct sbp_dev *sdev)
{
	struct fw_device *fwdev;
	struct crom_context c, *cc = &c;
	struct csrreg *reg;

	bzero(sdev->vendor, sizeof(sdev->vendor));
	bzero(sdev->product, sizeof(sdev->product));

	fwdev = sdev->target->fwdev;
	crom_init_context(cc, fwdev->csrrom);
	/* get vendor string */
	crom_search_key(cc, CSRKEY_VENDOR);
	crom_next(cc);
	crom_parse_text(cc, sdev->vendor, sizeof(sdev->vendor));
	/* skip to the unit directory for SBP-2 */
	while ((reg = crom_search_key(cc, CSRKEY_VER)) != NULL) {
		if (reg->val == CSRVAL_T10SBP2)
			break;
		crom_next(cc);
	}
	/* get firmware revision */
	reg = crom_search_key(cc, CSRKEY_FIRM_VER);
	if (reg != NULL)
		snprintf(sdev->revision, sizeof(sdev->revision),
						"%06x", reg->val);
	/* get product string */
	crom_search_key(cc, CSRKEY_MODEL);
	crom_next(cc);
	crom_parse_text(cc, sdev->product, sizeof(sdev->product));
}

static void
sbp_login_callout(void *arg)
{
	struct sbp_dev *sdev = (struct sbp_dev *)arg;
	sbp_mgm_orb(sdev, ORB_FUN_LGI, NULL);
}

static void
sbp_login(struct sbp_dev *sdev)
{
	struct timeval delta;
	struct timeval t;
	int ticks = 0;

	microtime(&delta);
	timevalsub(&delta, &sdev->target->sbp->last_busreset);
	t.tv_sec = login_delay / 1000;
	t.tv_usec = (login_delay % 1000) * 1000;
	timevalsub(&t, &delta);
	if (t.tv_sec >= 0 && t.tv_usec > 0)
		ticks = (t.tv_sec * 1000 + t.tv_usec / 1000) * hz / 1000;
SBP_DEBUG(0)
	printf("%s: sec = %ld usec = %ld ticks = %d\n", __FUNCTION__,
	    t.tv_sec, t.tv_usec, ticks);
END_DEBUG
	callout_reset(&sdev->login_callout, ticks,
			sbp_login_callout, (void *)(sdev));
}

#define SBP_FWDEV_ALIVE(fwdev) (((fwdev)->status == FWDEVATTACHED) \
	&& crom_has_specver((fwdev)->csrrom, CSRVAL_ANSIT10, CSRVAL_T10SBP2))

static void
sbp_probe_target(void *arg)
{
	struct sbp_target *target = (struct sbp_target *)arg;
	struct sbp_softc *sbp;
	struct sbp_dev *sdev;
	struct firewire_comm *fc;
	int i, alive;

	alive = SBP_FWDEV_ALIVE(target->fwdev);
SBP_DEBUG(1)
	printf("sbp_probe_target %d\n", target->target_id);
	if (!alive)
		printf("not alive\n");
END_DEBUG

	sbp = target->sbp;
	fc = target->sbp->fd.fc;
	sbp_alloc_lun(target);

	/* XXX untimeout mgm_ocb and dequeue */
	for (i=0; i < target->num_lun; i++) {
		sdev = target->luns[i];
		if (sdev == NULL)
			continue;
		if (alive && (sdev->status != SBP_DEV_DEAD)) {
			if (sdev->path != NULL) {
				xpt_freeze_devq(sdev->path, 1);
				sdev->freeze ++;
			}
			sbp_probe_lun(sdev);
SBP_DEBUG(0)
			sbp_show_sdev_info(sdev, 
					(sdev->status == SBP_DEV_RESET));
END_DEBUG

			sbp_abort_all_ocbs(sdev, CAM_SCSI_BUS_RESET);
			switch (sdev->status) {
			case SBP_DEV_RESET:
				/* new or revived target */
				if (auto_login)
					sbp_login(sdev);
				break;
			case SBP_DEV_TOATTACH:
			case SBP_DEV_PROBE:
			case SBP_DEV_ATTACHED:
			case SBP_DEV_RETRY:
			default:
				sbp_mgm_orb(sdev, ORB_FUN_RCN, NULL);
				break;
			}
		} else {
			switch (sdev->status) {
			case SBP_DEV_ATTACHED:
SBP_DEBUG(0)
				/* the device has gone */
				sbp_show_sdev_info(sdev, 2);
				printf("lost target\n");
END_DEBUG
				if (sdev->path) {
					xpt_freeze_devq(sdev->path, 1);
					sdev->freeze ++;
				}
				sdev->status = SBP_DEV_RETRY;
				sbp_abort_all_ocbs(sdev, CAM_SCSI_BUS_RESET);
				break;
			case SBP_DEV_PROBE:
			case SBP_DEV_TOATTACH:
				sdev->status = SBP_DEV_RESET;
				break;
			case SBP_DEV_RETRY:
			case SBP_DEV_RESET:
			case SBP_DEV_DEAD:
				break;
			}
		}
	}
}

static void
sbp_post_busreset(void *arg)
{
	struct sbp_softc *sbp;

	sbp = (struct sbp_softc *)arg;
SBP_DEBUG(0)
	printf("sbp_post_busreset\n");
END_DEBUG
	microtime(&sbp->last_busreset);
}

static void
sbp_post_explore(void *arg)
{
	struct sbp_softc *sbp = (struct sbp_softc *)arg;
	struct sbp_target *target;
	struct fw_device *fwdev;
	int i, alive;

SBP_DEBUG(0)
	printf("sbp_post_explore (sbp_cold=%d)\n", sbp_cold);
END_DEBUG
	if (sbp_cold > 0)
		sbp_cold --;

#if 0
	/*
	 * XXX don't let CAM the bus rest.
	 * CAM tries to do something with freezed (DEV_RETRY) devices.
	 */
	xpt_async(AC_BUS_RESET, sbp->path, /*arg*/ NULL);
#endif

	/* Gabage Collection */
	for(i = 0 ; i < SBP_NUM_TARGETS ; i ++){
		target = &sbp->targets[i];
		STAILQ_FOREACH(fwdev, &sbp->fd.fc->devices, link)
			if (target->fwdev == NULL || target->fwdev == fwdev)
				break;
		if (fwdev == NULL) {
			/* device has removed in lower driver */
			sbp_cam_detach_target(target);
			sbp_free_target(target);
		}
	}
	/* traverse device list */
	STAILQ_FOREACH(fwdev, &sbp->fd.fc->devices, link) {
SBP_DEBUG(0)
		printf("sbp_post_explore: EUI:%08x%08x ",
				fwdev->eui.hi, fwdev->eui.lo);
		if (fwdev->status != FWDEVATTACHED)
			printf("not attached, state=%d.\n", fwdev->status);
		else
			printf("attached\n");
END_DEBUG
		alive = SBP_FWDEV_ALIVE(fwdev);
		for(i = 0 ; i < SBP_NUM_TARGETS ; i ++){
			target = &sbp->targets[i];
			if(target->fwdev == fwdev ) {
				/* known target */
				break;
			}
		}
		if(i == SBP_NUM_TARGETS){
			if (alive) {
				/* new target */
				target = sbp_alloc_target(sbp, fwdev);
				if (target == NULL)
					continue;
			} else {
				continue;
			}
		}
		sbp_probe_target((void *)target);
		if (target->num_lun == 0)
			sbp_free_target(target);
	}
}

#if NEED_RESPONSE
static void
sbp_loginres_callback(struct fw_xfer *xfer){
	int s;
	struct sbp_dev *sdev;
	sdev = (struct sbp_dev *)xfer->sc;
SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_loginres_callback\n");
END_DEBUG
	/* recycle */
	s = splfw();
	STAILQ_INSERT_TAIL(&sdev->target->sbp->fwb.xferlist, xfer, link);
	splx(s);
	return;
}
#endif

static __inline void
sbp_xfer_free(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev;
	int s;

	sdev = (struct sbp_dev *)xfer->sc;
	fw_xfer_unload(xfer);
	s = splfw();
	STAILQ_INSERT_TAIL(&sdev->target->xferlist, xfer, link);
	splx(s);
}

static void
sbp_reset_start_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *tsdev, *sdev = (struct sbp_dev *)xfer->sc;
	struct sbp_target *target = sdev->target;
	int i;

	if (xfer->resp != 0) {
		sbp_show_sdev_info(sdev, 2);
		printf("sbp_reset_start failed: resp=%d\n", xfer->resp);
	}

	for (i = 0; i < target->num_lun; i++) {
		tsdev = target->luns[i];
		if (tsdev != NULL && tsdev->status == SBP_DEV_LOGIN)
			sbp_login(tsdev);
	}
}

static void
sbp_reset_start(struct sbp_dev *sdev)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

SBP_DEBUG(0)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_reset_start\n");
END_DEBUG

	xfer = sbp_write_cmd(sdev, FWTCODE_WREQQ, 0);
	xfer->act.hand = sbp_reset_start_callback;
	fp = &xfer->send.hdr;
	fp->mode.wreqq.dest_hi = 0xffff;
	fp->mode.wreqq.dest_lo = 0xf0000000 | RESET_START;
	fp->mode.wreqq.data = htonl(0xf);
	fw_asyreq(xfer->fc, -1, xfer);
}

static void
sbp_mgm_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev;
	int resp;

	sdev = (struct sbp_dev *)xfer->sc;

SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_mgm_callback\n");
END_DEBUG
	resp = xfer->resp;
	sbp_xfer_free(xfer);
#if 0
	if (resp != 0) {
		sbp_show_sdev_info(sdev, 2);
		printf("management ORB failed(%d) ... RESET_START\n", resp);
		sbp_reset_start(sdev);
	}
#endif
	return;
}

static struct sbp_dev *
sbp_next_dev(struct sbp_target *target, int lun)
{
	struct sbp_dev **sdevp;
	int i;

	for (i = lun, sdevp = &target->luns[lun]; i < target->num_lun;
	    i++, sdevp++)
		if (*sdevp != NULL && (*sdevp)->status == SBP_DEV_PROBE)
			return(*sdevp);
	return(NULL);
}

#define SCAN_PRI 1
static void
sbp_cam_scan_lun(struct cam_periph *periph, union ccb *ccb)
{
	struct sbp_target *target;
	struct sbp_dev *sdev;

	sdev = (struct sbp_dev *) ccb->ccb_h.ccb_sdev_ptr;
	target = sdev->target;
SBP_DEBUG(0)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_cam_scan_lun\n");
END_DEBUG
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
		sdev->status = SBP_DEV_ATTACHED;
	} else {
		sbp_show_sdev_info(sdev, 2);
		printf("scan failed\n");
	}
	sdev = sbp_next_dev(target, sdev->lun_id + 1);
	if (sdev == NULL) {
		free(ccb, M_SBP);
		return;
	}
	/* reuse ccb */
	xpt_setup_ccb(&ccb->ccb_h, sdev->path, SCAN_PRI);
	ccb->ccb_h.ccb_sdev_ptr = sdev;
	xpt_action(ccb);
	xpt_release_devq(sdev->path, sdev->freeze, TRUE);
	sdev->freeze = 1;
}

static void
sbp_cam_scan_target(void *arg)
{
	struct sbp_target *target = (struct sbp_target *)arg;
	struct sbp_dev *sdev;
	union ccb *ccb;

	sdev = sbp_next_dev(target, 0);
	if (sdev == NULL) {
		printf("sbp_cam_scan_target: nothing to do for target%d\n",
							target->target_id);
		return;
	}
SBP_DEBUG(0)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_cam_scan_target\n");
END_DEBUG
	ccb = malloc(sizeof(union ccb), M_SBP, M_NOWAIT | M_ZERO);
	if (ccb == NULL) {
		printf("sbp_cam_scan_target: malloc failed\n");
		return;
	}
	xpt_setup_ccb(&ccb->ccb_h, sdev->path, SCAN_PRI);
	ccb->ccb_h.func_code = XPT_SCAN_LUN;
	ccb->ccb_h.cbfcnp = sbp_cam_scan_lun;
	ccb->ccb_h.flags |= CAM_DEV_QFREEZE;
	ccb->crcn.flags = CAM_FLAG_NONE;
	ccb->ccb_h.ccb_sdev_ptr = sdev;

	/* The scan is in progress now. */
	xpt_action(ccb);
	xpt_release_devq(sdev->path, sdev->freeze, TRUE);
	sdev->freeze = 1;
}

static __inline void
sbp_scan_dev(struct sbp_dev *sdev)
{
	sdev->status = SBP_DEV_PROBE;
	callout_reset(&sdev->target->scan_callout, scan_delay * hz / 1000,
			sbp_cam_scan_target, (void *)sdev->target);
}

static void
sbp_do_attach(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev;
	struct sbp_target *target;
	struct sbp_softc *sbp;

	sdev = (struct sbp_dev *)xfer->sc;
	target = sdev->target;
	sbp = target->sbp;
SBP_DEBUG(0)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_do_attach\n");
END_DEBUG
	sbp_xfer_free(xfer);

	if (sdev->path == NULL)
		xpt_create_path(&sdev->path, xpt_periph,
			cam_sim_path(target->sbp->sim),
			target->target_id, sdev->lun_id);

	/*
	 * Let CAM scan the bus if we are in the boot process.
	 * XXX xpt_scan_bus cannot detect LUN larger than 0
	 * if LUN 0 doesn't exists.
	 */
	if (sbp_cold > 0) {
		sdev->status = SBP_DEV_ATTACHED;
		return;
	}

	sbp_scan_dev(sdev);
	return;
}

static void
sbp_agent_reset_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev;

	sdev = (struct sbp_dev *)xfer->sc;
SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
	printf("%s\n", __FUNCTION__);
END_DEBUG
	if (xfer->resp != 0) {
		sbp_show_sdev_info(sdev, 2);
		printf("%s: resp=%d\n", __FUNCTION__, xfer->resp);
	}

	sbp_xfer_free(xfer);
	if (sdev->path) {
		xpt_release_devq(sdev->path, sdev->freeze, TRUE);
		sdev->freeze = 0;
	}
}

static void
sbp_agent_reset(struct sbp_dev *sdev)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

SBP_DEBUG(0)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_agent_reset\n");
END_DEBUG
	xfer = sbp_write_cmd(sdev, FWTCODE_WREQQ, 0x04);
	if (xfer == NULL)
		return;
	if (sdev->status == SBP_DEV_ATTACHED || sdev->status == SBP_DEV_PROBE)
		xfer->act.hand = sbp_agent_reset_callback;
	else
		xfer->act.hand = sbp_do_attach;
	fp = &xfer->send.hdr;
	fp->mode.wreqq.data = htonl(0xf);
	fw_asyreq(xfer->fc, -1, xfer);
	sbp_abort_all_ocbs(sdev, CAM_BDR_SENT);
}

static void
sbp_busy_timeout_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev;

	sdev = (struct sbp_dev *)xfer->sc;
SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_busy_timeout_callback\n");
END_DEBUG
	sbp_xfer_free(xfer);
	sbp_agent_reset(sdev);
}

static void
sbp_busy_timeout(struct sbp_dev *sdev)
{
	struct fw_pkt *fp;
	struct fw_xfer *xfer;
SBP_DEBUG(0)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_busy_timeout\n");
END_DEBUG
	xfer = sbp_write_cmd(sdev, FWTCODE_WREQQ, 0);

	xfer->act.hand = sbp_busy_timeout_callback;
	fp = &xfer->send.hdr;
	fp->mode.wreqq.dest_hi = 0xffff;
	fp->mode.wreqq.dest_lo = 0xf0000000 | BUSY_TIMEOUT;
	fp->mode.wreqq.data = htonl((1 << (13+12)) | 0xf);
	fw_asyreq(xfer->fc, -1, xfer);
}

static void
sbp_orb_pointer_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev;
	sdev = (struct sbp_dev *)xfer->sc;

SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
	printf("%s\n", __FUNCTION__);
END_DEBUG
	if (xfer->resp != 0) {
		/* XXX */
		printf("%s: xfer->resp != 0\n", __FUNCTION__);
	}
	sbp_xfer_free(xfer);
	sdev->flags &= ~ORB_POINTER_ACTIVE;

	if ((sdev->flags & ORB_POINTER_NEED) != 0) {
		struct sbp_ocb *ocb;

		sdev->flags &= ~ORB_POINTER_NEED;
		ocb = STAILQ_FIRST(&sdev->ocbs);
		if (ocb != NULL)
			sbp_orb_pointer(sdev, ocb);
	}
	return;
}

static void
sbp_orb_pointer(struct sbp_dev *sdev, struct sbp_ocb *ocb)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
	printf("%s: 0x%08x\n", __FUNCTION__, (u_int32_t)ocb->bus_addr);
END_DEBUG

	if ((sdev->flags & ORB_POINTER_ACTIVE) != 0) {
SBP_DEBUG(0)
		printf("%s: orb pointer active\n", __FUNCTION__);
END_DEBUG
		sdev->flags |= ORB_POINTER_NEED;
		return;
	}

	sdev->flags |= ORB_POINTER_ACTIVE;
	xfer = sbp_write_cmd(sdev, FWTCODE_WREQB, 0x08);
	if (xfer == NULL)
		return;
	xfer->act.hand = sbp_orb_pointer_callback;

	fp = &xfer->send.hdr;
	fp->mode.wreqb.len = 8;
	fp->mode.wreqb.extcode = 0;
	xfer->send.payload[0] = 
		htonl(((sdev->target->sbp->fd.fc->nodeid | FWLOCALBUS )<< 16));
	xfer->send.payload[1] = htonl((u_int32_t)ocb->bus_addr);

	if(fw_asyreq(xfer->fc, -1, xfer) != 0){
			sbp_xfer_free(xfer);
			ocb->ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ocb->ccb);
	}
}

#if 0
static void
sbp_cmd_callback(struct fw_xfer *xfer)
{
SBP_DEBUG(1)
	struct sbp_dev *sdev;
	sdev = (struct sbp_dev *)xfer->sc;
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_cmd_callback\n");
END_DEBUG
	if (xfer->resp != 0) {
		/* XXX */
		printf("%s: xfer->resp != 0\n", __FUNCTION__);
	}
	sbp_xfer_free(xfer);
	return;
}

static void
sbp_doorbell(struct sbp_dev *sdev)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_doorbell\n");
END_DEBUG

	xfer = sbp_write_cmd(sdev, FWTCODE_WREQQ, 0x10);
	if (xfer == NULL)
		return;
	xfer->act.hand = sbp_cmd_callback;
	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.wreqq.data = htonl(0xf);
	fw_asyreq(xfer->fc, -1, xfer);
}
#endif

static struct fw_xfer *
sbp_write_cmd(struct sbp_dev *sdev, int tcode, int offset)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	struct sbp_target *target;
	int s, new = 0;

	target = sdev->target;
	s = splfw();
	xfer = STAILQ_FIRST(&target->xferlist);
	if (xfer == NULL) {
		if (target->n_xfer > 5 /* XXX */) {
			printf("sbp: no more xfer for this target\n");
			splx(s);
			return(NULL);
		}
		xfer = fw_xfer_alloc_buf(M_SBP, 8, 0);
		if(xfer == NULL){
			printf("sbp: fw_xfer_alloc_buf failed\n");
			splx(s);
			return NULL;
		}
		target->n_xfer ++;
		if (debug)
			printf("sbp: alloc %d xfer\n", target->n_xfer);
		new = 1;
	} else {
		STAILQ_REMOVE_HEAD(&target->xferlist, link);
	}
	splx(s);

	microtime(&xfer->tv);

	if (new) {
		xfer->recv.pay_len = 0;
		xfer->send.spd = min(sdev->target->fwdev->speed, max_speed);
		xfer->fc = sdev->target->sbp->fd.fc;
		xfer->retry_req = fw_asybusy;
	}

	if (tcode == FWTCODE_WREQB)
		xfer->send.pay_len = 8;
	else
		xfer->send.pay_len = 0;

	xfer->sc = (caddr_t)sdev;
	fp = &xfer->send.hdr;
	fp->mode.wreqq.dest_hi = sdev->login->cmd_hi;
	fp->mode.wreqq.dest_lo = sdev->login->cmd_lo + offset;
	fp->mode.wreqq.tlrt = 0;
	fp->mode.wreqq.tcode = tcode;
	fp->mode.wreqq.pri = 0;
	fp->mode.wreqq.dst = FWLOCALBUS | sdev->target->fwdev->dst;

	return xfer;

}

static void
sbp_mgm_orb(struct sbp_dev *sdev, int func, struct sbp_ocb *aocb)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	struct sbp_ocb *ocb;
	struct sbp_target *target;
	int s, nid;

	target = sdev->target;
	nid = target->sbp->fd.fc->nodeid | FWLOCALBUS;

	s = splfw();
	if (func == ORB_FUN_RUNQUEUE) {
		ocb = STAILQ_FIRST(&target->mgm_ocb_queue);
		if (target->mgm_ocb_cur != NULL || ocb == NULL) {
			splx(s);
			return;
		}
		STAILQ_REMOVE_HEAD(&target->mgm_ocb_queue, ocb);
		goto start;
	}
	if ((ocb = sbp_get_ocb(sdev)) == NULL) {
		splx(s);
		return;
	}
	ocb->flags = OCB_ACT_MGM;
	ocb->sdev = sdev;

	bzero((void *)ocb->orb, sizeof(ocb->orb));
	ocb->orb[6] = htonl((nid << 16) | SBP_BIND_HI);
	ocb->orb[7] = htonl(SBP_DEV2ADDR(target->target_id, sdev->lun_id));

SBP_DEBUG(0)
	sbp_show_sdev_info(sdev, 2);
	printf("%s\n", orb_fun_name[(func>>16)&0xf]);
END_DEBUG
	switch (func) {
	case ORB_FUN_LGI:
		ocb->orb[0] = ocb->orb[1] = 0; /* password */
		ocb->orb[2] = htonl(nid << 16);
		ocb->orb[3] = htonl(sdev->dma.bus_addr);
		ocb->orb[4] = htonl(ORB_NOTIFY | sdev->lun_id);
		if (ex_login)
			ocb->orb[4] |= htonl(ORB_EXV);
		ocb->orb[5] = htonl(SBP_LOGIN_SIZE);
		fwdma_sync(&sdev->dma, BUS_DMASYNC_PREREAD);
		break;
	case ORB_FUN_ATA:
		ocb->orb[0] = htonl((0 << 16) | 0);
		ocb->orb[1] = htonl(aocb->bus_addr & 0xffffffff);
		/* fall through */
	case ORB_FUN_RCN:
	case ORB_FUN_LGO:
	case ORB_FUN_LUR:
	case ORB_FUN_RST:
	case ORB_FUN_ATS:
		ocb->orb[4] = htonl(ORB_NOTIFY | func | sdev->login->id);
		break;
	}

	if (target->mgm_ocb_cur != NULL) {
		/* there is a standing ORB */
		STAILQ_INSERT_TAIL(&sdev->target->mgm_ocb_queue, ocb, ocb);
		splx(s);
		return;
	}
start:
	target->mgm_ocb_cur = ocb;
	splx(s);

	callout_reset(&target->mgm_ocb_timeout, 5*hz,
				sbp_mgm_timeout, (caddr_t)ocb);
	xfer = sbp_write_cmd(sdev, FWTCODE_WREQB, 0);
	if(xfer == NULL){
		return;
	}
	xfer->act.hand = sbp_mgm_callback;

	fp = &xfer->send.hdr;
	fp->mode.wreqb.dest_hi = sdev->target->mgm_hi;
	fp->mode.wreqb.dest_lo = sdev->target->mgm_lo;
	fp->mode.wreqb.len = 8;
	fp->mode.wreqb.extcode = 0;
	xfer->send.payload[0] = htonl(nid << 16);
	xfer->send.payload[1] = htonl(ocb->bus_addr & 0xffffffff);
SBP_DEBUG(0)
	sbp_show_sdev_info(sdev, 2);
	printf("mgm orb: %08x\n", (u_int32_t)ocb->bus_addr);
END_DEBUG

	fw_asyreq(xfer->fc, -1, xfer);
}

static void
sbp_print_scsi_cmd(struct sbp_ocb *ocb)
{
	struct ccb_scsiio *csio;

	csio = &ocb->ccb->csio;
	printf("%s:%d:%d XPT_SCSI_IO: "
		"cmd: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x"
		", flags: 0x%02x, "
		"%db cmd/%db data/%db sense\n",
		device_get_nameunit(ocb->sdev->target->sbp->fd.dev),
		ocb->ccb->ccb_h.target_id, ocb->ccb->ccb_h.target_lun,
		csio->cdb_io.cdb_bytes[0],
		csio->cdb_io.cdb_bytes[1],
		csio->cdb_io.cdb_bytes[2],
		csio->cdb_io.cdb_bytes[3],
		csio->cdb_io.cdb_bytes[4],
		csio->cdb_io.cdb_bytes[5],
		csio->cdb_io.cdb_bytes[6],
		csio->cdb_io.cdb_bytes[7],
		csio->cdb_io.cdb_bytes[8],
		csio->cdb_io.cdb_bytes[9],
		ocb->ccb->ccb_h.flags & CAM_DIR_MASK,
		csio->cdb_len, csio->dxfer_len,
		csio->sense_len);
}

static void
sbp_scsi_status(struct sbp_status *sbp_status, struct sbp_ocb *ocb)
{
	struct sbp_cmd_status *sbp_cmd_status;
	struct scsi_sense_data *sense;

	sbp_cmd_status = (struct sbp_cmd_status *)sbp_status->data;
	sense = &ocb->ccb->csio.sense_data;

SBP_DEBUG(0)
	sbp_print_scsi_cmd(ocb);
	/* XXX need decode status */
	sbp_show_sdev_info(ocb->sdev, 2);
	printf("SCSI status %x sfmt %x valid %x key %x code %x qlfr %x len %d\n",
		sbp_cmd_status->status,
		sbp_cmd_status->sfmt,
		sbp_cmd_status->valid,
		sbp_cmd_status->s_key,
		sbp_cmd_status->s_code,
		sbp_cmd_status->s_qlfr,
		sbp_status->len
	);
END_DEBUG

	switch (sbp_cmd_status->status) {
	case SCSI_STATUS_CHECK_COND:
	case SCSI_STATUS_BUSY:
	case SCSI_STATUS_CMD_TERMINATED:
		if(sbp_cmd_status->sfmt == SBP_SFMT_CURR){
			sense->error_code = SSD_CURRENT_ERROR;
		}else{
			sense->error_code = SSD_DEFERRED_ERROR;
		}
		if(sbp_cmd_status->valid)
			sense->error_code |= SSD_ERRCODE_VALID;
		sense->flags = sbp_cmd_status->s_key;
		if(sbp_cmd_status->mark)
			sense->flags |= SSD_FILEMARK;
		if(sbp_cmd_status->eom)
			sense->flags |= SSD_EOM;
		if(sbp_cmd_status->ill_len)
			sense->flags |= SSD_ILI;

		bcopy(&sbp_cmd_status->info, &sense->info[0], 4);

		if (sbp_status->len <= 1)
			/* XXX not scsi status. shouldn't be happened */ 
			sense->extra_len = 0;
		else if (sbp_status->len <= 4)
			/* add_sense_code(_qual), info, cmd_spec_info */
			sense->extra_len = 6;
		else
			/* fru, sense_key_spec */
			sense->extra_len = 10;

		bcopy(&sbp_cmd_status->cdb, &sense->cmd_spec_info[0], 4);

		sense->add_sense_code = sbp_cmd_status->s_code;
		sense->add_sense_code_qual = sbp_cmd_status->s_qlfr;
		sense->fru = sbp_cmd_status->fru;

		bcopy(&sbp_cmd_status->s_keydep[0],
		    &sense->sense_key_spec[0], 3);

		ocb->ccb->csio.scsi_status = sbp_cmd_status->status;;
		ocb->ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR
							| CAM_AUTOSNS_VALID;
/*
{
		u_int8_t j, *tmp;
		tmp = sense;
		for( j = 0 ; j < 32 ; j+=8){
			printf("sense %02x%02x %02x%02x %02x%02x %02x%02x\n", 
				tmp[j], tmp[j+1], tmp[j+2], tmp[j+3],
				tmp[j+4], tmp[j+5], tmp[j+6], tmp[j+7]);
		}

}
*/
		break;
	default:
		sbp_show_sdev_info(ocb->sdev, 2);
		printf("sbp_scsi_status: unknown scsi status 0x%x\n",
						sbp_cmd_status->status);
	}
}

static void
sbp_fix_inq_data(struct sbp_ocb *ocb)
{
	union ccb *ccb;
	struct sbp_dev *sdev;
	struct scsi_inquiry_data *inq;

	ccb = ocb->ccb;
	sdev = ocb->sdev;

	if (ccb->csio.cdb_io.cdb_bytes[1] & SI_EVPD)
		return;
SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_fix_inq_data\n");
END_DEBUG
	inq = (struct scsi_inquiry_data *) ccb->csio.data_ptr;
	switch (SID_TYPE(inq)) {
	case T_DIRECT:
#if 0
		/* 
		 * XXX Convert Direct Access device to RBC.
		 * I've never seen FireWire DA devices which support READ_6.
		 */
		if (SID_TYPE(inq) == T_DIRECT)
			inq->device |= T_RBC; /*  T_DIRECT == 0 */
#endif
		/* fall through */
	case T_RBC:
		/* enable tagged queuing */
#if 1
		inq->flags |= SID_CmdQue;
#endif
		/*
		 * Override vendor/product/revision information.
		 * Some devices sometimes return strange strings.
		 */
#if 1
		bcopy(sdev->vendor, inq->vendor, sizeof(inq->vendor));
		bcopy(sdev->product, inq->product, sizeof(inq->product));
		bcopy(sdev->revision+2, inq->revision, sizeof(inq->revision));
#endif
		break;
	}
}

static void
sbp_recv1(struct fw_xfer *xfer)
{
	struct fw_pkt *rfp;
#if NEED_RESPONSE
	struct fw_pkt *sfp;
#endif
	struct sbp_softc *sbp;
	struct sbp_dev *sdev;
	struct sbp_ocb *ocb;
	struct sbp_login_res *login_res = NULL;
	struct sbp_status *sbp_status;
	struct sbp_target *target;
	int	orb_fun, status_valid0, status_valid, t, l, reset_agent = 0;
	u_int32_t addr;
/*
	u_int32_t *ld;
	ld = xfer->recv.buf;
printf("sbp %x %d %d %08x %08x %08x %08x\n",
			xfer->resp, xfer->recv.len, xfer->recv.off, ntohl(ld[0]), ntohl(ld[1]), ntohl(ld[2]), ntohl(ld[3]));
printf("sbp %08x %08x %08x %08x\n", ntohl(ld[4]), ntohl(ld[5]), ntohl(ld[6]), ntohl(ld[7]));
printf("sbp %08x %08x %08x %08x\n", ntohl(ld[8]), ntohl(ld[9]), ntohl(ld[10]), ntohl(ld[11]));
*/
	sbp = (struct sbp_softc *)xfer->sc;
	if (xfer->resp != 0){
		printf("sbp_recv: xfer->resp != 0\n");
		goto done0;
	}
	if (xfer->recv.payload == NULL){
		printf("sbp_recv: xfer->recv.payload == NULL\n");
		goto done0;
	}
	rfp = &xfer->recv.hdr;
	if(rfp->mode.wreqb.tcode != FWTCODE_WREQB){
		printf("sbp_recv: tcode = %d\n", rfp->mode.wreqb.tcode);
		goto done0;
	}
	sbp_status = (struct sbp_status *)xfer->recv.payload;
	addr = rfp->mode.wreqb.dest_lo;
SBP_DEBUG(2)
	printf("received address 0x%x\n", addr);
END_DEBUG
	t = SBP_ADDR2TRG(addr);
	if (t >= SBP_NUM_TARGETS) {
		device_printf(sbp->fd.dev,
			"sbp_recv1: invalid target %d\n", t);
		goto done0;
	}
	target = &sbp->targets[t];
	l = SBP_ADDR2LUN(addr);
	if (l >= target->num_lun || target->luns[l] == NULL) {
		device_printf(sbp->fd.dev,
			"sbp_recv1: invalid lun %d (target=%d)\n", l, t);
		goto done0;
	}
	sdev = target->luns[l];

	ocb = NULL;
	switch (sbp_status->src) {
	case 0:
	case 1:
		/* check mgm_ocb_cur first */
		ocb  = target->mgm_ocb_cur;
		if (ocb != NULL) {
			if (OCB_MATCH(ocb, sbp_status)) {
				callout_stop(&target->mgm_ocb_timeout);
				target->mgm_ocb_cur = NULL;
				break;
			}
		}
		ocb = sbp_dequeue_ocb(sdev, sbp_status);
		if (ocb == NULL) {
			sbp_show_sdev_info(sdev, 2);
#if __FreeBSD_version >= 500000
			printf("No ocb(%x) on the queue\n",
#else
			printf("No ocb(%lx) on the queue\n",
#endif
					ntohl(sbp_status->orb_lo));
		}
		break;
	case 2:
		/* unsolicit */
		sbp_show_sdev_info(sdev, 2);
		printf("unsolicit status received\n");
		break;
	default:
		sbp_show_sdev_info(sdev, 2);
		printf("unknown sbp_status->src\n");
	}

	status_valid0 = (sbp_status->src < 2
			&& sbp_status->resp == ORB_RES_CMPL
			&& sbp_status->dead == 0);
	status_valid = (status_valid0 && sbp_status->status == 0);

	if (!status_valid0 || debug > 2){
		int status;
SBP_DEBUG(0)
		sbp_show_sdev_info(sdev, 2);
		printf("ORB status src:%x resp:%x dead:%x"
#if __FreeBSD_version >= 500000
				" len:%x stat:%x orb:%x%08x\n",
#else
				" len:%x stat:%x orb:%x%08lx\n",
#endif
			sbp_status->src, sbp_status->resp, sbp_status->dead,
			sbp_status->len, sbp_status->status,
			ntohs(sbp_status->orb_hi), ntohl(sbp_status->orb_lo));
END_DEBUG
		sbp_show_sdev_info(sdev, 2);
		status = sbp_status->status;
		switch(sbp_status->resp) {
		case 0:
			if (status > MAX_ORB_STATUS0)
				printf("%s\n", orb_status0[MAX_ORB_STATUS0]);
			else
				printf("%s\n", orb_status0[status]);
			break;
		case 1:
			printf("Obj: %s, Error: %s\n",
				orb_status1_object[(status>>6) & 3],
				orb_status1_serial_bus_error[status & 0xf]);
			break;
		case 2:
			printf("Illegal request\n");
			break;
		case 3:
			printf("Vendor dependent\n");
			break;
		default:
			printf("unknown respose code %d\n", sbp_status->resp);
		}
	}

	/* we have to reset the fetch agent if it's dead */
	if (sbp_status->dead) {
		if (sdev->path) {
			xpt_freeze_devq(sdev->path, 1);
			sdev->freeze ++;
		}
		reset_agent = 1;
	}

	if (ocb == NULL)
		goto done;

	switch(ntohl(ocb->orb[4]) & ORB_FMT_MSK){
	case ORB_FMT_NOP:
		break;
	case ORB_FMT_VED:
		break;
	case ORB_FMT_STD:
		switch(ocb->flags) {
		case OCB_ACT_MGM:
			orb_fun = ntohl(ocb->orb[4]) & ORB_FUN_MSK;
			reset_agent = 0;
			switch(orb_fun) {
			case ORB_FUN_LGI:
				fwdma_sync(&sdev->dma, BUS_DMASYNC_POSTREAD);
				login_res = sdev->login;
				login_res->len = ntohs(login_res->len);
				login_res->id = ntohs(login_res->id);
				login_res->cmd_hi = ntohs(login_res->cmd_hi);
				login_res->cmd_lo = ntohl(login_res->cmd_lo);
				if (status_valid) {
SBP_DEBUG(0)
sbp_show_sdev_info(sdev, 2);
printf("login: len %d, ID %d, cmd %08x%08x, recon_hold %d\n", login_res->len, login_res->id, login_res->cmd_hi, login_res->cmd_lo, ntohs(login_res->recon_hold));
END_DEBUG
					sbp_busy_timeout(sdev);
				} else {
					/* forgot logout? */
					sbp_show_sdev_info(sdev, 2);
					printf("login failed\n");
					sdev->status = SBP_DEV_RESET;
				}
				break;
			case ORB_FUN_RCN:
				login_res = sdev->login;
				if (status_valid) {
SBP_DEBUG(0)
sbp_show_sdev_info(sdev, 2);
printf("reconnect: len %d, ID %d, cmd %08x%08x\n", login_res->len, login_res->id, login_res->cmd_hi, login_res->cmd_lo);
END_DEBUG
#if 1
					if (sdev->status == SBP_DEV_ATTACHED)
						sbp_scan_dev(sdev);
					else
						sbp_agent_reset(sdev);
#else
					sdev->status = SBP_DEV_ATTACHED;
					sbp_mgm_orb(sdev, ORB_FUN_ATS, NULL);
#endif
				} else {
					/* reconnection hold time exceed? */
SBP_DEBUG(0)
					sbp_show_sdev_info(sdev, 2);
					printf("reconnect failed\n");
END_DEBUG
					sbp_login(sdev);
				}
				break;
			case ORB_FUN_LGO:
				sdev->status = SBP_DEV_RESET;
				break;
			case ORB_FUN_RST:
				sbp_busy_timeout(sdev);
				break;
			case ORB_FUN_LUR:
			case ORB_FUN_ATA:
			case ORB_FUN_ATS:
				sbp_agent_reset(sdev);
				break;
			default:
				sbp_show_sdev_info(sdev, 2);
				printf("unknown function %d\n", orb_fun);
				break;
			}
			sbp_mgm_orb(sdev, ORB_FUN_RUNQUEUE, NULL);
			break;
		case OCB_ACT_CMD:
			sdev->timeout = 0;
			if(ocb->ccb != NULL){
				union ccb *ccb;
/*
				u_int32_t *ld;
				ld = ocb->ccb->csio.data_ptr;
				if(ld != NULL && ocb->ccb->csio.dxfer_len != 0)
					printf("ptr %08x %08x %08x %08x\n", ld[0], ld[1], ld[2], ld[3]);
				else
					printf("ptr NULL\n");
printf("len %d\n", sbp_status->len);
*/
				ccb = ocb->ccb;
				if(sbp_status->len > 1){
					sbp_scsi_status(sbp_status, ocb);
				}else{
					if(sbp_status->resp != ORB_RES_CMPL){
						ccb->ccb_h.status = CAM_REQ_CMP_ERR;
					}else{
						ccb->ccb_h.status = CAM_REQ_CMP;
					}
				}
				/* fix up inq data */
				if (ccb->csio.cdb_io.cdb_bytes[0] == INQUIRY)
					sbp_fix_inq_data(ocb);
				xpt_done(ccb);
			}
			break;
		default:
			break;
		}
	}

	sbp_free_ocb(sdev, ocb);
done:
	if (reset_agent)
		sbp_agent_reset(sdev);

done0:
	xfer->recv.pay_len = SBP_RECV_LEN;
/* The received packet is usually small enough to be stored within
 * the buffer. In that case, the controller return ack_complete and
 * no respose is necessary.
 *
 * XXX fwohci.c and firewire.c should inform event_code such as 
 * ack_complete or ack_pending to upper driver.
 */
#if NEED_RESPONSE
	xfer->send.off = 0;
	sfp = (struct fw_pkt *)xfer->send.buf;
	sfp->mode.wres.dst = rfp->mode.wreqb.src;
	xfer->dst = sfp->mode.wres.dst;
	xfer->spd = min(sdev->target->fwdev->speed, max_speed);
	xfer->act.hand = sbp_loginres_callback;
	xfer->retry_req = fw_asybusy;

	sfp->mode.wres.tlrt = rfp->mode.wreqb.tlrt;
	sfp->mode.wres.tcode = FWTCODE_WRES;
	sfp->mode.wres.rtcode = 0;
	sfp->mode.wres.pri = 0;

	fw_asyreq(xfer->fc, -1, xfer);
#else
	/* recycle */
	STAILQ_INSERT_TAIL(&sbp->fwb.xferlist, xfer, link);
#endif

	return;

}

static void
sbp_recv(struct fw_xfer *xfer)
{
	int s;

	s = splcam();
	sbp_recv1(xfer);
	splx(s);
}
/*
 * sbp_attach()
 */
static int
sbp_attach(device_t dev)
{
	struct sbp_softc *sbp;
	struct cam_devq *devq;
	struct fw_xfer *xfer;
	int i, s, error;

SBP_DEBUG(0)
	printf("sbp_attach (cold=%d)\n", cold);
END_DEBUG

	if (cold)
		sbp_cold ++;
	sbp = ((struct sbp_softc *)device_get_softc(dev));
	bzero(sbp, sizeof(struct sbp_softc));
	sbp->fd.dev = dev;
	sbp->fd.fc = device_get_ivars(dev);
	error = bus_dma_tag_create(/*parent*/sbp->fd.fc->dmat,
				/* XXX shoud be 4 for sane backend? */
				/*alignment*/1,
				/*boundary*/0,
				/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
				/*highaddr*/BUS_SPACE_MAXADDR,
				/*filter*/NULL, /*filterarg*/NULL,
				/*maxsize*/0x100000, /*nsegments*/SBP_IND_MAX,
				/*maxsegsz*/SBP_SEG_MAX,
				/*flags*/BUS_DMA_ALLOCNOW,
#if __FreeBSD_version >= 501102
				/*lockfunc*/busdma_lock_mutex,
				/*lockarg*/&Giant,
#endif
				&sbp->dmat);
	if (error != 0) {
		printf("sbp_attach: Could not allocate DMA tag "
			"- error %d\n", error);
			return (ENOMEM);
	}

	devq = cam_simq_alloc(/*maxopenings*/SBP_NUM_OCB);
	if (devq == NULL)
		return (ENXIO);

	for( i = 0 ; i < SBP_NUM_TARGETS ; i++){
		sbp->targets[i].fwdev = NULL;
		sbp->targets[i].luns = NULL;
	}

	sbp->sim = cam_sim_alloc(sbp_action, sbp_poll, "sbp", sbp,
				 device_get_unit(dev),
				 /*untagged*/ 1,
				 /*tagged*/ SBP_QUEUE_LEN,
				 devq);

	if (sbp->sim == NULL) {
		cam_simq_free(devq);
		return (ENXIO);
	}


	if (xpt_bus_register(sbp->sim, /*bus*/0) != CAM_SUCCESS)
		goto fail;

	if (xpt_create_path(&sbp->path, xpt_periph, cam_sim_path(sbp->sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sbp->sim));
		goto fail;
	}

	/* We reserve 16 bit space (4 bytes X 64 targets X 256 luns) */
	sbp->fwb.start = ((u_int64_t)SBP_BIND_HI << 32) | SBP_DEV2ADDR(0, 0);
	sbp->fwb.end = sbp->fwb.start + 0xffff;
	sbp->fwb.act_type = FWACT_XFER;
	/* pre-allocate xfer */
	STAILQ_INIT(&sbp->fwb.xferlist);
	for (i = 0; i < SBP_NUM_OCB/2; i ++) {
		xfer = fw_xfer_alloc_buf(M_SBP,
			/* send */0,
			/* recv */SBP_RECV_LEN);
		xfer->act.hand = sbp_recv;
#if NEED_RESPONSE
		xfer->fc = sbp->fd.fc;
#endif
		xfer->sc = (caddr_t)sbp;
		STAILQ_INSERT_TAIL(&sbp->fwb.xferlist, xfer, link);
	}
	fw_bindadd(sbp->fd.fc, &sbp->fwb);

	sbp->fd.post_busreset = sbp_post_busreset;
	sbp->fd.post_explore = sbp_post_explore;

	if (sbp->fd.fc->status != -1) {
		s = splfw();
		sbp_post_busreset((void *)sbp);
		sbp_post_explore((void *)sbp);
		splx(s);
	}
	xpt_async(AC_BUS_RESET, sbp->path, /*arg*/ NULL);

	return (0);
fail:
	cam_sim_free(sbp->sim, /*free_devq*/TRUE);
	return (ENXIO);
}

static int
sbp_logout_all(struct sbp_softc *sbp)
{
	struct sbp_target *target;
	struct sbp_dev *sdev;
	int i, j;

SBP_DEBUG(0)
	printf("sbp_logout_all\n");
END_DEBUG
	for (i = 0 ; i < SBP_NUM_TARGETS ; i ++) {
		target = &sbp->targets[i];
		if (target->luns == NULL)
			continue;
		for (j = 0; j < target->num_lun; j++) {
			sdev = target->luns[j];
			if (sdev == NULL)
				continue;
			callout_stop(&sdev->login_callout);
			if (sdev->status >= SBP_DEV_TOATTACH &&
					sdev->status <= SBP_DEV_ATTACHED)
				sbp_mgm_orb(sdev, ORB_FUN_LGO, NULL);
		}
	}

	return 0;
}

static int
sbp_shutdown(device_t dev)
{
	struct sbp_softc *sbp = ((struct sbp_softc *)device_get_softc(dev));

	sbp_logout_all(sbp);
	return (0);
}

static void
sbp_free_sdev(struct sbp_dev *sdev)
{
	int i;

	if (sdev == NULL)
		return;
	for (i = 0; i < SBP_QUEUE_LEN; i++)
		bus_dmamap_destroy(sdev->target->sbp->dmat,
		    sdev->ocb[i].dmamap);
	fwdma_free(sdev->target->sbp->fd.fc, &sdev->dma);
	free(sdev, M_SBP);
}

static void
sbp_free_target(struct sbp_target *target)
{
	struct sbp_softc *sbp;
	struct fw_xfer *xfer, *next;
	int i;

	if (target->luns == NULL)
		return;
	callout_stop(&target->mgm_ocb_timeout);
	sbp = target->sbp;
	for (i = 0; i < target->num_lun; i++)
		sbp_free_sdev(target->luns[i]);

	for (xfer = STAILQ_FIRST(&target->xferlist);
			xfer != NULL; xfer = next) {
		next = STAILQ_NEXT(xfer, link);
		fw_xfer_free_buf(xfer);
	}
	STAILQ_INIT(&target->xferlist);
	free(target->luns, M_SBP);
	target->num_lun = 0;;
	target->luns = NULL;
	target->fwdev = NULL;
}

static int
sbp_detach(device_t dev)
{
	struct sbp_softc *sbp = ((struct sbp_softc *)device_get_softc(dev));
	struct firewire_comm *fc = sbp->fd.fc;
	struct fw_xfer *xfer, *next;
	int i;

SBP_DEBUG(0)
	printf("sbp_detach\n");
END_DEBUG

	for (i = 0; i < SBP_NUM_TARGETS; i ++) 
		sbp_cam_detach_target(&sbp->targets[i]);
	xpt_async(AC_LOST_DEVICE, sbp->path, NULL);
	xpt_free_path(sbp->path);
	xpt_bus_deregister(cam_sim_path(sbp->sim));
	cam_sim_free(sbp->sim, /*free_devq*/ TRUE),

	sbp_logout_all(sbp);

	/* XXX wait for logout completion */
	tsleep(&i, FWPRI, "sbpdtc", hz/2);

	for (i = 0 ; i < SBP_NUM_TARGETS ; i ++)
		sbp_free_target(&sbp->targets[i]);

	for (xfer = STAILQ_FIRST(&sbp->fwb.xferlist);
				xfer != NULL; xfer = next) {
		next = STAILQ_NEXT(xfer, link);
		fw_xfer_free_buf(xfer);
	}
	STAILQ_INIT(&sbp->fwb.xferlist);
	fw_bindremove(fc, &sbp->fwb);

	bus_dma_tag_destroy(sbp->dmat);

	return (0);
}

static void
sbp_cam_detach_sdev(struct sbp_dev *sdev)
{
	if (sdev == NULL)
		return;
	if (sdev->status == SBP_DEV_DEAD)
		return;
	if (sdev->status == SBP_DEV_RESET)
		return;
	if (sdev->path) {
		xpt_release_devq(sdev->path,
				 sdev->freeze, TRUE);
		sdev->freeze = 0;
		xpt_async(AC_LOST_DEVICE, sdev->path, NULL);
		xpt_free_path(sdev->path);
		sdev->path = NULL;
	}
	sbp_abort_all_ocbs(sdev, CAM_DEV_NOT_THERE);
}

static void
sbp_cam_detach_target(struct sbp_target *target)
{
	int i;

	if (target->luns != NULL) {
SBP_DEBUG(0)
		printf("sbp_detach_target %d\n", target->target_id);
END_DEBUG
		callout_stop(&target->scan_callout);
		for (i = 0; i < target->num_lun; i++)
			sbp_cam_detach_sdev(target->luns[i]);
	}
}

static void
sbp_target_reset(struct sbp_dev *sdev, int method)
{
	int i;
	struct sbp_target *target = sdev->target;
	struct sbp_dev *tsdev;

	for (i = 0; i < target->num_lun; i++) {
		tsdev = target->luns[i];
		if (tsdev == NULL)
			continue;
		if (tsdev->status == SBP_DEV_DEAD)
			continue;
		if (tsdev->status == SBP_DEV_RESET)
			continue;
		xpt_freeze_devq(tsdev->path, 1);
		tsdev->freeze ++;
		sbp_abort_all_ocbs(tsdev, CAM_CMD_TIMEOUT);
		if (method == 2)
			tsdev->status = SBP_DEV_LOGIN;
	}
	switch(method) {
	case 1:
		printf("target reset\n");
		sbp_mgm_orb(sdev, ORB_FUN_RST, NULL);
		break;
	case 2:
		printf("reset start\n");
		sbp_reset_start(sdev);
		break;
	}
			
}

static void
sbp_mgm_timeout(void *arg)
{
	struct sbp_ocb *ocb = (struct sbp_ocb *)arg;
	struct sbp_dev *sdev = ocb->sdev;
	struct sbp_target *target = sdev->target;

	sbp_show_sdev_info(sdev, 2);
	printf("request timeout(mgm orb:0x%08x) ... ",
	    (u_int32_t)ocb->bus_addr);
	target->mgm_ocb_cur = NULL;
	sbp_free_ocb(sdev, ocb);
#if 0
	/* XXX */
	printf("run next request\n");
	sbp_mgm_orb(sdev, ORB_FUN_RUNQUEUE, NULL);
#endif
#if 1
	printf("reset start\n");
	sbp_reset_start(sdev);
#endif
}

static void
sbp_timeout(void *arg)
{
	struct sbp_ocb *ocb = (struct sbp_ocb *)arg;
	struct sbp_dev *sdev = ocb->sdev;

	sbp_show_sdev_info(sdev, 2);
	printf("request timeout(cmd orb:0x%08x) ... ",
	    (u_int32_t)ocb->bus_addr);

	sdev->timeout ++;
	switch(sdev->timeout) {
	case 1:
		printf("agent reset\n");
		xpt_freeze_devq(sdev->path, 1);
		sdev->freeze ++;
		sbp_abort_all_ocbs(sdev, CAM_CMD_TIMEOUT);
		sbp_agent_reset(sdev);
		break;
	case 2:
	case 3:
		sbp_target_reset(sdev, sdev->timeout - 1);
		break;
#if 0
	default:
		/* XXX give up */
		sbp_cam_detach_target(target);
		if (target->luns != NULL)
			free(target->luns, M_SBP);
		target->num_lun = 0;;
		target->luns = NULL;
		target->fwdev = NULL;
#endif
	}
}

static void
sbp_action1(struct cam_sim *sim, union ccb *ccb)
{

	struct sbp_softc *sbp = (struct sbp_softc *)sim->softc;
	struct sbp_target *target = NULL;
	struct sbp_dev *sdev = NULL;

	/* target:lun -> sdev mapping */
	if (sbp != NULL
			&& ccb->ccb_h.target_id != CAM_TARGET_WILDCARD
			&& ccb->ccb_h.target_id < SBP_NUM_TARGETS) {
		target = &sbp->targets[ccb->ccb_h.target_id];
		if (target->fwdev != NULL
				&& ccb->ccb_h.target_lun != CAM_LUN_WILDCARD
				&& ccb->ccb_h.target_lun < target->num_lun) {
			sdev = target->luns[ccb->ccb_h.target_lun];
			if (sdev != NULL && sdev->status != SBP_DEV_ATTACHED &&
				sdev->status != SBP_DEV_PROBE)
				sdev = NULL;
		}
	}

SBP_DEBUG(1)
	if (sdev == NULL)
		printf("invalid target %d lun %d\n",
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun);
END_DEBUG

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
	case XPT_RESET_DEV:
	case XPT_GET_TRAN_SETTINGS:
	case XPT_SET_TRAN_SETTINGS:
	case XPT_CALC_GEOMETRY:
		if (sdev == NULL) {
SBP_DEBUG(1)
			printf("%s:%d:%d:func_code 0x%04x: "
				"Invalid target (target needed)\n",
				device_get_nameunit(sbp->fd.dev),
				ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
				ccb->ccb_h.func_code);
END_DEBUG

			ccb->ccb_h.status = CAM_DEV_NOT_THERE;
			xpt_done(ccb);
			return;
		}
		break;
	case XPT_PATH_INQ:
	case XPT_NOOP:
		/* The opcodes sometimes aimed at a target (sc is valid),
		 * sometimes aimed at the SIM (sc is invalid and target is
		 * CAM_TARGET_WILDCARD)
		 */
		if (sbp == NULL && 
			ccb->ccb_h.target_id != CAM_TARGET_WILDCARD) {
SBP_DEBUG(0)
			printf("%s:%d:%d func_code 0x%04x: "
				"Invalid target (no wildcard)\n",
				device_get_nameunit(sbp->fd.dev),
				ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
				ccb->ccb_h.func_code);
END_DEBUG
			ccb->ccb_h.status = CAM_DEV_NOT_THERE;
			xpt_done(ccb);
			return;
		}
		break;
	default:
		/* XXX Hm, we should check the input parameters */
		break;
	}

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
	{
		struct ccb_scsiio *csio;
		struct sbp_ocb *ocb;
		int speed;
		void *cdb;

		csio = &ccb->csio;

SBP_DEBUG(2)
		printf("%s:%d:%d XPT_SCSI_IO: "
			"cmd: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x"
			", flags: 0x%02x, "
			"%db cmd/%db data/%db sense\n",
			device_get_nameunit(sbp->fd.dev),
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
			csio->cdb_io.cdb_bytes[0],
			csio->cdb_io.cdb_bytes[1],
			csio->cdb_io.cdb_bytes[2],
			csio->cdb_io.cdb_bytes[3],
			csio->cdb_io.cdb_bytes[4],
			csio->cdb_io.cdb_bytes[5],
			csio->cdb_io.cdb_bytes[6],
			csio->cdb_io.cdb_bytes[7],
			csio->cdb_io.cdb_bytes[8],
			csio->cdb_io.cdb_bytes[9],
			ccb->ccb_h.flags & CAM_DIR_MASK,
			csio->cdb_len, csio->dxfer_len,
			csio->sense_len);
END_DEBUG
		if(sdev == NULL){
			ccb->ccb_h.status = CAM_DEV_NOT_THERE;
			xpt_done(ccb);
			return;
		}
#if 0
		/* if we are in probe stage, pass only probe commands */
		if (sdev->status == SBP_DEV_PROBE) {
			char *name;
			name = xpt_path_periph(ccb->ccb_h.path)->periph_name;
			printf("probe stage, periph name: %s\n", name);
			if (strcmp(name, "probe") != 0) {
				ccb->ccb_h.status = CAM_REQUEUE_REQ;
				xpt_done(ccb);
				return;
			}
		}
#endif
		if ((ocb = sbp_get_ocb(sdev)) == NULL)
			return;

		ocb->flags = OCB_ACT_CMD;
		ocb->sdev = sdev;
		ocb->ccb = ccb;
		ccb->ccb_h.ccb_sdev_ptr = sdev;
		ocb->orb[0] = htonl(1 << 31);
		ocb->orb[1] = 0;
		ocb->orb[2] = htonl(((sbp->fd.fc->nodeid | FWLOCALBUS )<< 16) );
		ocb->orb[3] = htonl(ocb->bus_addr + IND_PTR_OFFSET);
		speed = min(target->fwdev->speed, max_speed);
		ocb->orb[4] = htonl(ORB_NOTIFY | ORB_CMD_SPD(speed)
						| ORB_CMD_MAXP(speed + 7));
		if((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN){
			ocb->orb[4] |= htonl(ORB_CMD_IN);
		}

		if (csio->ccb_h.flags & CAM_SCATTER_VALID)
			printf("sbp: CAM_SCATTER_VALID\n");
		if (csio->ccb_h.flags & CAM_DATA_PHYS)
			printf("sbp: CAM_DATA_PHYS\n");

		if (csio->ccb_h.flags & CAM_CDB_POINTER)
			cdb = (void *)csio->cdb_io.cdb_ptr;
		else
			cdb = (void *)&csio->cdb_io.cdb_bytes;
		bcopy(cdb, (void *)&ocb->orb[5], csio->cdb_len);
/*
printf("ORB %08x %08x %08x %08x\n", ntohl(ocb->orb[0]), ntohl(ocb->orb[1]), ntohl(ocb->orb[2]), ntohl(ocb->orb[3]));
printf("ORB %08x %08x %08x %08x\n", ntohl(ocb->orb[4]), ntohl(ocb->orb[5]), ntohl(ocb->orb[6]), ntohl(ocb->orb[7]));
*/
		if (ccb->csio.dxfer_len > 0) {
			int s, error;

			s = splsoftvm();
			error = bus_dmamap_load(/*dma tag*/sbp->dmat,
					/*dma map*/ocb->dmamap,
					ccb->csio.data_ptr,
					ccb->csio.dxfer_len,
					sbp_execute_ocb,
					ocb,
					/*flags*/0);
			splx(s);
			if (error)
				printf("sbp: bus_dmamap_load error %d\n", error);
		} else
			sbp_execute_ocb(ocb, NULL, 0, 0);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		struct ccb_calc_geometry *ccg;
#if __FreeBSD_version < 501100
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;
		int extended = 1;
#endif

		ccg = &ccb->ccg;
		if (ccg->block_size == 0) {
			printf("sbp_action1: block_size is 0.\n");
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			break;
		}
SBP_DEBUG(1)
		printf("%s:%d:%d:%d:XPT_CALC_GEOMETRY: "
#if __FreeBSD_version >= 500000
			"Volume size = %jd\n",
#else
			"Volume size = %d\n",
#endif
			device_get_nameunit(sbp->fd.dev),
			cam_sim_path(sbp->sim),
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
#if __FreeBSD_version >= 500000
			(uintmax_t)
#endif
				ccg->volume_size);
END_DEBUG

#if __FreeBSD_version < 501100
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);

		if (size_mb > 1024 && extended) {
			ccg->heads = 255;
			ccg->secs_per_track = 63;
		} else {
			ccg->heads = 64;
			ccg->secs_per_track = 32;
		}
		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;
		ccb->ccb_h.status = CAM_REQ_CMP;
#else
		cam_calc_geometry(ccg, /*extended*/1);
#endif
		xpt_done(ccb);
		break;
	}
	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
	{

SBP_DEBUG(1)
		printf("%s:%d:XPT_RESET_BUS: \n",
			device_get_nameunit(sbp->fd.dev), cam_sim_path(sbp->sim));
END_DEBUG

		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;
		
SBP_DEBUG(1)
		printf("%s:%d:%d XPT_PATH_INQ:.\n",
			device_get_nameunit(sbp->fd.dev),
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun);
END_DEBUG
		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = PI_TAG_ABLE;
		cpi->target_sprt = 0;
		cpi->hba_misc = PIM_NOBUSRESET | PIM_NO_6_BYTE;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = SBP_NUM_TARGETS - 1;
		cpi->max_lun = SBP_NUM_LUNS - 1;
		cpi->initiator_id = SBP_INITIATOR;
		cpi->bus_id = sim->bus_id;
		cpi->base_transfer_speed = 400 * 1000 / 8;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "SBP", HBA_IDLEN);
		strncpy(cpi->dev_name, sim->sim_name, DEV_IDLEN);
		cpi->unit_number = sim->unit_number;

		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_GET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings *cts = &ccb->cts;
SBP_DEBUG(1)
		printf("%s:%d:%d XPT_GET_TRAN_SETTINGS:.\n",
			device_get_nameunit(sbp->fd.dev),
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun);
END_DEBUG
		/* Enable disconnect and tagged queuing */
		cts->valid = CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;
		cts->flags = CCB_TRANS_DISC_ENB | CCB_TRANS_TAG_ENB;

		cts->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_ABORT:
		ccb->ccb_h.status = CAM_UA_ABORT;
		xpt_done(ccb);
		break;
	case XPT_SET_TRAN_SETTINGS:
		/* XXX */
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
	return;
}

static void
sbp_action(struct cam_sim *sim, union ccb *ccb)
{
	int s;

	s = splfw();
	sbp_action1(sim, ccb);
	splx(s);
}

static void
sbp_execute_ocb(void *arg,  bus_dma_segment_t *segments, int seg, int error)
{
	int i;
	struct sbp_ocb *ocb;
	struct sbp_ocb *prev;
	bus_dma_segment_t *s;

	if (error)
		printf("sbp_execute_ocb: error=%d\n", error);

	ocb = (struct sbp_ocb *)arg;

SBP_DEBUG(2)
	printf("sbp_execute_ocb: seg %d", seg);
	for (i = 0; i < seg; i++)
#if __FreeBSD_version >= 500000
		printf(", %jx:%jd", (uintmax_t)segments[i].ds_addr,
					(uintmax_t)segments[i].ds_len);
#else
		printf(", %x:%d", segments[i].ds_addr, segments[i].ds_len);
#endif
	printf("\n");
END_DEBUG

	if (seg == 1) {
		/* direct pointer */
		s = &segments[0];
		if (s->ds_len > SBP_SEG_MAX)
			panic("ds_len > SBP_SEG_MAX, fix busdma code");
		ocb->orb[3] = htonl(s->ds_addr);
		ocb->orb[4] |= htonl(s->ds_len);
	} else if(seg > 1) {
		/* page table */
		for (i = 0; i < seg; i++) {
			s = &segments[i];
SBP_DEBUG(0)
			/* XXX LSI Logic "< 16 byte" bug might be hit */
			if (s->ds_len < 16)
				printf("sbp_execute_ocb: warning, "
#if __FreeBSD_version >= 500000
					"segment length(%zd) is less than 16."
#else
					"segment length(%d) is less than 16."
#endif
					"(seg=%d/%d)\n", s->ds_len, i+1, seg);
END_DEBUG
			if (s->ds_len > SBP_SEG_MAX)
				panic("ds_len > SBP_SEG_MAX, fix busdma code");
			ocb->ind_ptr[i].hi = htonl(s->ds_len << 16);
			ocb->ind_ptr[i].lo = htonl(s->ds_addr);
		}
		ocb->orb[4] |= htonl(ORB_CMD_PTBL | seg);
	}
	
	if (seg > 0)
		bus_dmamap_sync(ocb->sdev->target->sbp->dmat, ocb->dmamap,
			(ntohl(ocb->orb[4]) & ORB_CMD_IN) ?
			BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);
	prev = sbp_enqueue_ocb(ocb->sdev, ocb);
	fwdma_sync(&ocb->sdev->dma, BUS_DMASYNC_PREWRITE);
	if (prev == NULL || (ocb->sdev->flags & ORB_LINK_DEAD) != 0) {
		ocb->sdev->flags &= ~ORB_LINK_DEAD;
		sbp_orb_pointer(ocb->sdev, ocb); 
	}
}

static void
sbp_poll(struct cam_sim *sim)
{       
	struct sbp_softc *sbp;
	struct firewire_comm *fc;

	sbp = (struct sbp_softc *)sim->softc;
	fc = sbp->fd.fc;

	fc->poll(fc, 0, -1);

	return;
}

static struct sbp_ocb *
sbp_dequeue_ocb(struct sbp_dev *sdev, struct sbp_status *sbp_status)
{
	struct sbp_ocb *ocb;
	struct sbp_ocb *next;
	int s = splfw(), order = 0;
	int flags;

SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
#if __FreeBSD_version >= 500000
	printf("%s: 0x%08x src %d\n",
#else
	printf("%s: 0x%08lx src %d\n",
#endif
	    __FUNCTION__, ntohl(sbp_status->orb_lo), sbp_status->src);
END_DEBUG
	for (ocb = STAILQ_FIRST(&sdev->ocbs); ocb != NULL; ocb = next) {
		next = STAILQ_NEXT(ocb, ocb);
		flags = ocb->flags;
		if (OCB_MATCH(ocb, sbp_status)) {
			/* found */
			STAILQ_REMOVE(&sdev->ocbs, ocb, sbp_ocb, ocb);
			if (ocb->ccb != NULL)
				untimeout(sbp_timeout, (caddr_t)ocb,
						ocb->ccb->ccb_h.timeout_ch);
			if (ntohl(ocb->orb[4]) & 0xffff) {
				bus_dmamap_sync(sdev->target->sbp->dmat,
					ocb->dmamap,
					(ntohl(ocb->orb[4]) & ORB_CMD_IN) ?
					BUS_DMASYNC_POSTREAD :
					BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(sdev->target->sbp->dmat,
					ocb->dmamap);
			}
			if (sbp_status->src == SRC_NO_NEXT) {
				if (next != NULL)
					sbp_orb_pointer(sdev, next); 
				else if (order > 0) {
					/*
					 * Unordered execution
					 * We need to send pointer for
					 * next ORB
					 */
					sdev->flags |= ORB_LINK_DEAD;
				}
			}
			break;
		} else
			order ++;
	}
	splx(s);
SBP_DEBUG(0)
	if (ocb && order > 0) {
		sbp_show_sdev_info(sdev, 2);
		printf("unordered execution order:%d\n", order);
	}
END_DEBUG
	return (ocb);
}

static struct sbp_ocb *
sbp_enqueue_ocb(struct sbp_dev *sdev, struct sbp_ocb *ocb)
{
	int s = splfw();
	struct sbp_ocb *prev;

SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
#if __FreeBSD_version >= 500000
	printf("%s: 0x%08jx\n", __FUNCTION__, (uintmax_t)ocb->bus_addr);
#else
	printf("%s: 0x%08x\n", __FUNCTION__, ocb->bus_addr);
#endif
END_DEBUG
	prev = STAILQ_LAST(&sdev->ocbs, sbp_ocb, ocb);
	STAILQ_INSERT_TAIL(&sdev->ocbs, ocb, ocb);

	if (ocb->ccb != NULL)
		ocb->ccb->ccb_h.timeout_ch = timeout(sbp_timeout, (caddr_t)ocb,
					(ocb->ccb->ccb_h.timeout * hz) / 1000);

	if (prev != NULL) {
SBP_DEBUG(2)
#if __FreeBSD_version >= 500000
		printf("linking chain 0x%jx -> 0x%jx\n",
		    (uintmax_t)prev->bus_addr, (uintmax_t)ocb->bus_addr);
#else
		printf("linking chain 0x%x -> 0x%x\n",
		    prev->bus_addr, ocb->bus_addr);
#endif
END_DEBUG
		prev->orb[1] = htonl(ocb->bus_addr);
		prev->orb[0] = 0;
	}
	splx(s);

	return prev;
}

static struct sbp_ocb *
sbp_get_ocb(struct sbp_dev *sdev)
{
	struct sbp_ocb *ocb;
	int s = splfw();
	ocb = STAILQ_FIRST(&sdev->free_ocbs);
	if (ocb == NULL) {
		printf("ocb shortage!!!\n");
		return NULL;
	}
	STAILQ_REMOVE_HEAD(&sdev->free_ocbs, ocb);
	splx(s);
	ocb->ccb = NULL;
	return (ocb);
}

static void
sbp_free_ocb(struct sbp_dev *sdev, struct sbp_ocb *ocb)
{
	ocb->flags = 0;
	ocb->ccb = NULL;
	STAILQ_INSERT_TAIL(&sdev->free_ocbs, ocb, ocb);
}

static void
sbp_abort_ocb(struct sbp_ocb *ocb, int status)
{
	struct sbp_dev *sdev;

	sdev = ocb->sdev;
SBP_DEBUG(0)
	sbp_show_sdev_info(sdev, 2);
#if __FreeBSD_version >= 500000
	printf("sbp_abort_ocb 0x%jx\n", (uintmax_t)ocb->bus_addr);
#else
	printf("sbp_abort_ocb 0x%x\n", ocb->bus_addr);
#endif
END_DEBUG
SBP_DEBUG(1)
	if (ocb->ccb != NULL)
		sbp_print_scsi_cmd(ocb);
END_DEBUG
	if (ntohl(ocb->orb[4]) & 0xffff) {
		bus_dmamap_sync(sdev->target->sbp->dmat, ocb->dmamap,
			(ntohl(ocb->orb[4]) & ORB_CMD_IN) ?
			BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sdev->target->sbp->dmat, ocb->dmamap);
	}
	if (ocb->ccb != NULL) {
		untimeout(sbp_timeout, (caddr_t)ocb,
					ocb->ccb->ccb_h.timeout_ch);
		ocb->ccb->ccb_h.status = status;
		xpt_done(ocb->ccb);
	}
	sbp_free_ocb(sdev, ocb);
}

static void
sbp_abort_all_ocbs(struct sbp_dev *sdev, int status)
{
	int s;
	struct sbp_ocb *ocb, *next;
	STAILQ_HEAD(, sbp_ocb) temp;

	s = splfw();

	bcopy(&sdev->ocbs, &temp, sizeof(temp));
	STAILQ_INIT(&sdev->ocbs);
	for (ocb = STAILQ_FIRST(&temp); ocb != NULL; ocb = next) {
		next = STAILQ_NEXT(ocb, ocb);
		sbp_abort_ocb(ocb, status);
	}

	splx(s);
}

static devclass_t sbp_devclass;

static device_method_t sbp_methods[] = {
	/* device interface */
	DEVMETHOD(device_identify,	sbp_identify),
	DEVMETHOD(device_probe,		sbp_probe),
	DEVMETHOD(device_attach,	sbp_attach),
	DEVMETHOD(device_detach,	sbp_detach),
	DEVMETHOD(device_shutdown,	sbp_shutdown),

	{ 0, 0 }
};

static driver_t sbp_driver = {
	"sbp",
	sbp_methods,
	sizeof(struct sbp_softc),
};
DRIVER_MODULE(sbp, firewire, sbp_driver, sbp_devclass, 0, 0);
MODULE_VERSION(sbp, 1);
MODULE_DEPEND(sbp, firewire, 1, 1, 1);
MODULE_DEPEND(sbp, cam, 1, 1, 1);
