/*
 * Copyright (c) 1998,1999,2000,2001 Katsushi Kobayashi and Hidetosh Shimokawa
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
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <machine/bus.h>
#include <sys/malloc.h>
#include <sys/devicestat.h>	/* for struct devstat */


#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/cam_periph.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_da.h>

#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/iec13213.h>

#define ccb_sdev_ptr	spriv_ptr0
#define ccb_sbp_ptr	spriv_ptr1

#define SBP_NUM_TARGETS 8
#define SBP_NUM_LUNS 8	/* limited by CAM_SCSI2_MAXLUN in cam_xpt.c */
#define SBP_QUEUE_LEN 4
#define SBP_NUM_OCB (SBP_QUEUE_LEN * SBP_NUM_TARGETS)
#define SBP_INITIATOR 7
#define SBP_ESELECT_TIMEOUT 1
#define SBP_BIND_HI 0x1
#define SBP_DEV2ADDR(u, t, l)	\
	((((u) & 0xff) << 16) | (((l) & 0xff) << 8) | (((t) & 0x3f) << 2))
#define SBP_ADDR2TRG(a)	(((a) >> 2) & 0x3f)
#define SBP_ADDR2LUN(a)	(((a) >> 8) & 0xff)

#define ORB_NOTIFY	(1 << 31)
#define	ORB_FMT_STD	(0 << 29)
#define	ORB_FMT_VED	(2 << 29)
#define	ORB_FMT_NOP	(3 << 29)
#define	ORB_FMT_MSK	(3 << 29)
#define	ORB_EXV		(1 << 28)
/* */
#define	ORB_CMD_IN	(1 << 27)
/* */
#define	ORB_CMD_SPD(x)	((x) << 24)
#define	ORB_CMD_MAXP(x)	((x) << 20)
#define	ORB_RCN_TMO(x)	((x) << 20)
#define	ORB_CMD_PTBL	(1 << 19)
#define	ORB_CMD_PSZ(x)	((x) << 16)

#define	ORB_FUN_LGI	(0 << 16)
#define	ORB_FUN_QLG	(1 << 16)
#define	ORB_FUN_RCN	(3 << 16)
#define	ORB_FUN_LGO	(7 << 16)
#define	ORB_FUN_ATA	(0xb << 16)
#define	ORB_FUN_ATS	(0xc << 16)
#define	ORB_FUN_LUR	(0xe << 16)
#define	ORB_FUN_RST	(0xf << 16)
#define	ORB_FUN_MSK	(0xf << 16)

static char *orb_fun_name[] = {
	/* 0 */ "LOGIN",
	/* 1 */ "QUERY LOGINS",
	/* 2 */ "Reserved",
	/* 3 */ "RECONNECT",
	/* 4 */ "SET PASSWORD",
	/* 5 */ "Reserved",
	/* 6 */ "Reserved",
	/* 7 */ "LOGOUT",
	/* 8 */ "Reserved",
	/* 9 */ "Reserved",
	/* A */ "Reserved",
	/* B */ "ABORT TASK",
	/* C */ "ABORT TASK SET",
	/* D */ "Reserved",
	/* E */ "LOGICAL UNIT RESET",
	/* F */ "TARGET RESET"
};

#define ORB_RES_CMPL 0
#define ORB_RES_FAIL 1
#define ORB_RES_ILLE 2
#define ORB_RES_VEND 3

static int debug = 1;
static int auto_login = 1;
static int max_speed = 2;

SYSCTL_DECL(_hw_firewire);
SYSCTL_NODE(_hw_firewire, OID_AUTO, sbp, CTLFLAG_RD, 0, "SBP-II Subsystem");
SYSCTL_INT(_debug, OID_AUTO, sbp_debug, CTLFLAG_RW, &debug, 0,
	"SBP debug flag");
SYSCTL_INT(_hw_firewire_sbp, OID_AUTO, auto_login, CTLFLAG_RW, &auto_login, 0,
	"SBP perform login automatically");
SYSCTL_INT(_hw_firewire_sbp, OID_AUTO, max_speed, CTLFLAG_RW, &max_speed, 0,
	"SBP transfer max speed");

#define SBP_DEBUG(x)	if (debug > x) {
#define END_DEBUG	}

#define NEED_RESPONSE 0

struct ind_ptr {
	u_int32_t hi,lo;
};
#define SBP_IND_MAX 0x20
struct sbp_ocb {
	STAILQ_ENTRY(sbp_ocb)	ocb;
	union ccb	*ccb;
	volatile u_int32_t	orb[8];
	volatile struct ind_ptr  ind_ptr[SBP_IND_MAX];
	struct sbp_dev	*sdev;
	int		flags;
	bus_dmamap_t	dmamap;
};
#define OCB_ACT_MGM 0
#define OCB_ACT_CMD 1
#define OCB_ACT_MASK 3
#define OCB_RESERVED 0x10
#define OCB_DONE 0x20

#define SBP_RESOURCE_SHORTAGE 0x10

struct sbp_login_res{
#if FW_ENDIANSWAP == 0 && BYTE_ORDER == LITTLE_ENDIAN
	u_int16_t	len;
	u_int16_t	id;
	u_int16_t	res0;
	u_int16_t	cmd_hi;
	u_int32_t	cmd_lo;
	u_int16_t	res1;
	u_int16_t	recon_hold;
#else
	u_int16_t	id;
	u_int16_t	len;
	u_int16_t	cmd_hi;
	u_int16_t	res0;
	u_int32_t	cmd_lo;
	u_int16_t	recon_hold;
	u_int16_t	res1;
#endif
};
struct sbp_status{
#if FW_ENDIANSWAP == 0 && BYTE_ORDER == LITTLE_ENDIAN
	u_int8_t	len:3,
			dead:1,
			resp:2,
			src:2;
	u_int8_t	status:8;
	u_int16_t	orb_hi;
	u_int32_t	orb_lo;
	u_int32_t	data[6];
#else
	u_int16_t	orb_hi;
	u_int8_t	status:8;
	u_int8_t	len:3,
			dead:1,
			resp:2,
			src:2;
	u_int32_t	orb_lo;
	u_int32_t	data[6];
#endif
};
struct sbp_cmd_status{
#define SBP_SFMT_CURR 0
#define SBP_SFMT_DEFER 1
#if FW_ENDIANSWAP == 0 && BYTE_ORDER == LITTLE_ENDIAN
	u_int8_t	status:6,
			sfmt:2;
	u_int8_t	s_key:4,
			ill_len:1,
			eom:1,
			mark:1,
			valid:1;
	u_int8_t	s_code;
	u_int8_t	s_qlfr;
	u_int32_t	info;
	u_int32_t	cdb;
	u_int32_t	fru:8,
			s_keydep:24;
	u_int32_t	vend[2];
#else
	u_int8_t	s_qlfr;
	u_int8_t	s_code;
	u_int8_t	s_key:4,
			ill_len:1,
			eom:1,
			mark:1,
			valid:1;
	u_int8_t	status:6,
			sfmt:2;
	u_int32_t	info;
	u_int32_t	cdb;
	u_int32_t	s_keydep:24,
			fru:8;
	u_int32_t	vend[2];
#endif
};

struct sbp_dev{
#define SBP_DEV_RESET		0	/* accept login */
#define SBP_DEV_LOGIN		1	/* to login */
#define SBP_DEV_RECONN		2	/* to reconnect */
#define SBP_DEV_TOATTACH	3	/* to attach */
#define SBP_DEV_PROBE		4	/* scan lun */
#define SBP_DEV_ATTACHED	5	/* in operation */
#define SBP_DEV_DEAD		6	/* unavailable unit */
#define SBP_DEV_RETRY		7	/* unavailable unit */
	int status;
	int lun_id;
	struct cam_path *path;
	struct sbp_target *target;
	struct sbp_login_res login;
	STAILQ_HEAD(, sbp_ocb) ocbs;
	char vendor[32];
	char product[32];
	char revision[10];
};

struct sbp_target {
	int target_id;
	int num_lun;
	struct sbp_dev	*luns;
	struct sbp_softc *sbp;
	struct fw_device *fwdev;
	u_int32_t mgm_hi, mgm_lo;
};

struct sbp_softc {
	struct firewire_dev_comm fd;
	unsigned char flags;
	struct cam_sim  *sim;
	struct sbp_target targets[SBP_NUM_TARGETS];
	struct fw_bind fwb;
	STAILQ_HEAD(, sbp_ocb) free_ocbs;
	struct sbp_ocb *ocb;
	bus_dma_tag_t	dmat;
};
static void sbp_post_explore __P((void *));
static void sbp_recv __P((struct fw_xfer *));
static void sbp_login_callback __P((struct fw_xfer *));
static void sbp_cmd_callback __P((struct fw_xfer *));
static void sbp_orb_pointer __P((struct sbp_dev *, struct sbp_ocb *));
static void sbp_execute_ocb __P((void *,  bus_dma_segment_t *, int, int));
static void sbp_free_ocb __P((struct sbp_softc *, struct sbp_ocb *));
static void sbp_abort_ocb __P((struct sbp_ocb *, int));
static void sbp_abort_all_ocbs __P((struct sbp_dev *, int));
static struct fw_xfer * sbp_write_cmd __P((struct sbp_dev *, int, int));
static struct sbp_ocb * sbp_get_ocb __P((struct sbp_softc *));
static struct sbp_ocb * sbp_enqueue_ocb __P((struct sbp_dev *, struct sbp_ocb *));
static struct sbp_ocb * sbp_dequeue_ocb __P((struct sbp_dev *, u_int32_t));
static void sbp_detach_target __P((struct sbp_target *));
static void sbp_timeout __P((void *arg));
static void sbp_mgm_orb __P((struct sbp_dev *, int));

MALLOC_DEFINE(M_SBP, "sbp", "SBP-II/Firewire");

/* cam related functions */
static void	sbp_action(struct cam_sim *sim, union ccb *ccb);
static void	sbp_poll(struct cam_sim *sim);
static void	sbp_cam_callback(struct cam_periph *periph,
					union ccb *ccb);
static void	sbp_cam_scan_lun(struct sbp_dev *sdev);

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

	device_set_desc(dev, "SBP2/SCSI over firewire");
	return (0);
}

static void
sbp_show_sdev_info(struct sbp_dev *sdev, int new)
{
	int lun;
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
	lun = getcsrdata(fwdev, 0x14);
	printf("ordered:%d type:%d EUI:%08x%08x node:%d "
		"speed:%d maxrec:%d",
		(lun & 0x00400000) >> 22,
		(lun & 0x001f0000) >> 16,
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

static struct sbp_target *
sbp_alloc_target(struct sbp_softc *sbp, struct fw_device *fwdev)
{
	int i, lun;
	struct sbp_target *target;
	struct sbp_dev *sdev;

SBP_DEBUG(1)
	printf("sbp_alloc_target\n");
END_DEBUG
	for (i = 0; i < SBP_NUM_TARGETS; i++)
		if(sbp->targets[i].fwdev == NULL) break;
	if (i == SBP_NUM_TARGETS) {
		printf("increase SBP_NUM_TARGETS!\n");
		return NULL;
	}
	/* new target */
	target = &sbp->targets[i];
	target->sbp = sbp;
	target->fwdev = fwdev;
	target->target_id = i;
	if((target->mgm_lo = getcsrdata(fwdev, 0x54)) == 0 ){
		/* bad target */
		printf("NULL management address\n");
		target->fwdev = NULL;
		return NULL;
	}
	target->mgm_hi = 0xffff;
	target->mgm_lo = 0xf0000000 | target->mgm_lo << 2;
	/* XXX should probe all luns */
	/* XXX num_lun may be changed. realloc luns? */
	lun = getcsrdata(target->fwdev, 0x14) & 0xff;
	target->num_lun = lun + 1;
	target->luns = (struct sbp_dev *) malloc(
				sizeof(struct sbp_dev) * target->num_lun, 
				M_SBP, M_NOWAIT | M_ZERO);
	for (i = 0; i < target->num_lun; i++) {
		sdev = &target->luns[i];
		sdev->lun_id = i;
		sdev->target = target;
		STAILQ_INIT(&sdev->ocbs);
		if (i == lun)
			sdev->status = SBP_DEV_RESET;
		else
			sdev->status = SBP_DEV_DEAD;
	}
	return target;
}

static void
sbp_get_text_leaf(struct fw_device *fwdev, int key, char *buf, int len)
{
	static char *nullstr = "(null)";
	int i, clen, found=0;
	struct csrhdr *chdr;
	struct csrreg *creg;
	u_int32_t *src, *dst;

	chdr = (struct csrhdr *)&fwdev->csrrom[0];
	creg = (struct csrreg *)chdr;
	creg += chdr->info_len;
	for( i = chdr->info_len + 4; i <= fwdev->rommax; i+=4){
		if((creg++)->key == key){
			found = 1;
			break;
		}
	}
	if (!found) {
		strncpy(buf, nullstr, len);
		return;
	}
	src = (u_int32_t *) creg + creg->val;
	clen = ((*src >> 16) - 2) * 4;
	src += 3;
	dst = (u_int32_t *) buf;
	if (len < clen)
		clen = len;
	for (i = 0; i < clen/4; i++)
		*dst++ = htonl(*src++);
	buf[clen] = 0;
}

static void
sbp_probe_lun(struct sbp_dev *sdev)
{
	struct fw_device *fwdev;
	int rev;

	fwdev = sdev->target->fwdev;
	bzero(sdev->vendor, sizeof(sdev->vendor));
	bzero(sdev->product, sizeof(sdev->product));
	sbp_get_text_leaf(fwdev, 0x03, sdev->vendor, sizeof(sdev->vendor));
	sbp_get_text_leaf(fwdev, 0x17, sdev->product, sizeof(sdev->product));
	rev = getcsrdata(sdev->target->fwdev, 0x3c);
	snprintf(sdev->revision, sizeof(sdev->revision), "%06x", rev);
}
static void
sbp_probe_target(struct sbp_target *target, int alive)
{
	struct sbp_softc *sbp;
	struct sbp_dev *sdev;
	struct firewire_comm *fc;
	int i;

SBP_DEBUG(1)
	printf("sbp_probe_target %d\n", target->target_id);
	if (!alive)
		printf("not alive\n");
END_DEBUG

	sbp = target->sbp;
	fc = target->sbp->fd.fc;
	for (i=0; i < target->num_lun; i++) {
		sdev = &target->luns[i];
		if (alive && (sdev->status != SBP_DEV_DEAD)) {
			if (sdev->path != NULL) {
				xpt_freeze_devq(sdev->path, 1);
			}
			sbp_abort_all_ocbs(sdev, CAM_REQUEUE_REQ);
			switch (sdev->status) {
			case SBP_DEV_ATTACHED:
				sbp_mgm_orb(sdev, ORB_FUN_RCN);
				break;
			case SBP_DEV_RETRY:
				sbp_probe_lun(sdev);
				sbp_mgm_orb(sdev, ORB_FUN_LGI);
				break;
			default:
				/* new or revived target */
				sbp_probe_lun(sdev);
				if (auto_login) {
					sdev->status = SBP_DEV_TOATTACH;
					sbp_mgm_orb(sdev, ORB_FUN_LGI);
				}
				break;
			}
			sbp_show_sdev_info(sdev, 
					(sdev->status == SBP_DEV_TOATTACH));
		} else {
			switch (sdev->status) {
			case SBP_DEV_ATTACHED:
SBP_DEBUG(0)
				/* the device has gone */
				sbp_show_sdev_info(sdev, 2);
				printf("lost target\n");
END_DEBUG
				if (sdev->path)
					xpt_freeze_devq(sdev->path, 1);
				sdev->status = SBP_DEV_RETRY;
				sbp_abort_all_ocbs(sdev, CAM_REQUEUE_REQ);
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

#if 0
static void
sbp_release_queue(void *arg)
{
	struct sbp_softc *sbp;

SBP_DEBUG(0)
	printf("sbp_release_queue\n");
END_DEBUG
	sbp = (struct sbp_softc *)arg;
	xpt_release_simq(sbp->sim, 1);
}

static void
sbp_release_devq(void *arg)
{
	struct sbp_dev *sdev;
	int s;

	sdev = (struct sbp_dev *)arg;
SBP_DEBUG(0)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_release_devq\n");
END_DEBUG
	s = splcam();
	xpt_release_devq(sdev->path, 1, TRUE);
	splx(s);
}
#endif

static void
sbp_post_explore(void *arg)
{
	struct sbp_softc *sbp = (struct sbp_softc *)arg;
	struct sbp_target *target;
	struct fw_device *fwdev;
	int i, alive;

SBP_DEBUG(1)
	printf("sbp_post_explore\n");
END_DEBUG
#if 0
	xpt_freeze_simq(sbp->sim, /*count*/ 1);
#endif
	/* Gabage Collection */
	for(i = 0 ; i < SBP_NUM_TARGETS ; i ++){
		target = &sbp->targets[i];
		for( fwdev  = TAILQ_FIRST(&sbp->fd.fc->devices);
			fwdev != NULL; fwdev = TAILQ_NEXT(fwdev, link)){
			if(target->fwdev == NULL) break;
			if(target->fwdev == fwdev) break;
		}
		if(fwdev == NULL){
			/* device has removed in lower driver */
			sbp_detach_target(target);
		}
	}
	/* traverse device list */
	for( fwdev  = TAILQ_FIRST(&sbp->fd.fc->devices);
		fwdev != NULL; fwdev = TAILQ_NEXT(fwdev, link)){
SBP_DEBUG(0)
		printf("sbp_post_explore: EUI:%08x%08x ",
				fwdev->eui.hi, fwdev->eui.lo);
		if (fwdev->status == FWDEVATTACHED) {
			printf("spec=%d key=%d.\n",
			getcsrdata(fwdev, CSRKEY_SPEC) == CSRVAL_ANSIT10,
			getcsrdata(fwdev, CSRKEY_VER) == CSRVAL_T10SBP2);
		} else {
			printf("not attached, state=%d.\n", fwdev->status);
		}
END_DEBUG
		alive = (fwdev->status == FWDEVATTACHED)
			&& (getcsrdata(fwdev, CSRKEY_SPEC) == CSRVAL_ANSIT10)
			&& (getcsrdata(fwdev, CSRKEY_VER) == CSRVAL_T10SBP2);
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
		sbp_probe_target(target, alive);
	}
#if 0
	timeout(sbp_release_queue, (caddr_t)sbp, bus_reset_rest * hz / 1000);
#endif
}

#if NEED_RESPONSE
static void
sbp_loginres_callback(struct fw_xfer *xfer){
SBP_DEBUG(1)
	struct sbp_dev *sdev;
	sdev = (struct sbp_dev *)xfer->sc;
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_loginres_callback\n");
END_DEBUG
	fw_xfer_free(xfer);
	return;
}
#endif

static void
sbp_login_callback(struct fw_xfer *xfer)
{
SBP_DEBUG(1)
	struct sbp_dev *sdev;
	sdev = (struct sbp_dev *)xfer->sc;
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_login_callback\n");
END_DEBUG
	fw_xfer_free(xfer);
	return;
}

static void
sbp_cmd_callback(struct fw_xfer *xfer)
{
SBP_DEBUG(2)
	struct sbp_dev *sdev;
	sdev = (struct sbp_dev *)xfer->sc;
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_cmd_callback\n");
END_DEBUG
	fw_xfer_free(xfer);
	return;
}

static void
sbp_cam_callback(struct cam_periph *periph, union ccb *ccb)
{
	struct sbp_dev *sdev;
	sdev = (struct sbp_dev *) ccb->ccb_h.ccb_sdev_ptr;
SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_cam_callback\n");
END_DEBUG
	sdev->status = SBP_DEV_ATTACHED;
	free(ccb, M_SBP);
}

static void
sbp_cam_scan_lun(struct sbp_dev *sdev)
{
	union ccb *ccb = malloc(sizeof(union ccb), M_SBP, M_WAITOK | M_ZERO);

SBP_DEBUG(0)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_cam_scan_lun\n");
END_DEBUG
	xpt_setup_ccb(&ccb->ccb_h, sdev->path, 5/*priority (low)*/);
	ccb->ccb_h.func_code = XPT_SCAN_LUN;
	ccb->ccb_h.cbfcnp = sbp_cam_callback;
	ccb->crcn.flags = CAM_FLAG_NONE;
	ccb->ccb_h.ccb_sdev_ptr = sdev;
	xpt_action(ccb);

	/* The scan is in progress now. */
}


static void
sbp_ping_unit_callback(struct cam_periph *periph, union ccb *ccb)
{
	struct sbp_dev *sdev;
	sdev = (struct sbp_dev *) ccb->ccb_h.ccb_sdev_ptr;
SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_ping_unit_callback\n");
END_DEBUG
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		if (--ccb->ccb_h.retry_count == 0) {
			sbp_show_sdev_info(sdev, 2);
			printf("sbp_tur_callback: retry count exceeded\n");
			sdev->status = SBP_DEV_RETRY;
			free(ccb, M_SBP);
		} else {
			/* requeue */
			xpt_action(ccb);
			xpt_release_devq(sdev->path, 1, TRUE);
		}
	} else {
		free(ccb->csio.data_ptr, M_SBP);
		free(ccb, M_SBP);
		sdev->status = SBP_DEV_ATTACHED;
		xpt_release_devq(sdev->path, 1, TRUE);
	}
}

/* 
 * XXX Some devices need to execute inquiry or read_capacity
 * after bus_rest during busy transfer.
 * Otherwise they return incorrect result for READ(and WRITE?)
 * command without any SBP-II/SCSI error.
 *
 * e.g. Maxtor 3000XT, Yano A-dish.
 */
static void
sbp_ping_unit(struct sbp_dev *sdev)
{
	union ccb *ccb;
	struct scsi_inquiry_data *inq_buf;

	ccb = malloc(sizeof(union ccb), M_SBP, M_WAITOK | M_ZERO);
	inq_buf = (struct scsi_inquiry_data *)
			malloc(sizeof(*inq_buf), M_SBP, M_WAITOK);

SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_ping_unit\n");
END_DEBUG

	/*
	 * We need to execute this command before any other queued command.
	 * Make priority 0 and freeze queue after execution for retry.
	 * cam's scan_lun command doesn't provide this feature.
	 */
	xpt_setup_ccb(&ccb->ccb_h, sdev->path, 0/*priority (high)*/);
	scsi_inquiry(
		&ccb->csio,
		/*retries*/ 5,
		sbp_ping_unit_callback,
		MSG_SIMPLE_Q_TAG,
		(u_int8_t *)inq_buf,
		SHORT_INQUIRY_LENGTH,
		/*evpd*/FALSE,
		/*page_code*/0,
		SSD_MIN_SIZE,
		/*timeout*/60000
	);
	ccb->ccb_h.flags |= CAM_DEV_QFREEZE;
	xpt_action(ccb);
}

static void
sbp_do_attach(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev;

	sdev = (struct sbp_dev *)xfer->sc;
SBP_DEBUG(0)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_do_attach\n");
END_DEBUG
	fw_xfer_free(xfer);
	if (sdev->path == NULL)
		xpt_create_path(&sdev->path, xpt_periph,
			cam_sim_path(sdev->target->sbp->sim),
			sdev->target->target_id, sdev->lun_id);

	if (sdev->status == SBP_DEV_RETRY) {
		sdev->status = SBP_DEV_PROBE;
		sbp_ping_unit(sdev);
		/* freezed twice */
		xpt_release_devq(sdev->path, 1, TRUE);
	} else {
		sdev->status = SBP_DEV_PROBE;
		sbp_cam_scan_lun(sdev);
	}
	xpt_release_devq(sdev->path, 1, TRUE);
	return;
}

static void
sbp_agent_reset_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev;

	sdev = (struct sbp_dev *)xfer->sc;
SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_cmd_callback\n");
END_DEBUG
	fw_xfer_free(xfer);
	sbp_abort_all_ocbs(sdev, CAM_REQUEUE_REQ);
	if (sdev->path)
		xpt_release_devq(sdev->path, 1, TRUE);
}

static void
sbp_agent_reset(struct sbp_dev *sdev, int attach)
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
	if (attach)
		xfer->act.hand = sbp_do_attach;
	else
		xfer->act.hand = sbp_agent_reset_callback;
	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.wreqq.data = htonl(0xf);
	fw_asyreq(xfer->fc, -1, xfer);
}

static void
sbp_busy_timeout_callback(struct fw_xfer *xfer)
{
	struct sbp_dev *sdev;

	sdev = (struct sbp_dev *)xfer->sc;
SBP_DEBUG(1)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_but_timeout_callback\n");
END_DEBUG
	fw_xfer_free(xfer);
	sbp_agent_reset(sdev, 1);
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
	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.wreqq.dest_hi = htons(0xffff);
	fp->mode.wreqq.dest_lo = htonl(0xf0000000 | BUS_TIME);
	fp->mode.wreqq.data = htonl(0xf);
	fw_asyreq(xfer->fc, -1, xfer);
}

#if 0
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

	xfer->act.hand = sbp_busy_timeout;
	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.wreqq.dest_hi = htons(0xffff);
	fp->mode.wreqq.dest_lo = htonl(0xf0000000 | RESET_START);
	fp->mode.wreqq.data = htonl(0xf);
	fw_asyreq(xfer->fc, -1, xfer);
}
#endif

static void
sbp_orb_pointer(struct sbp_dev *sdev, struct sbp_ocb *ocb)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
SBP_DEBUG(2)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_orb_pointer\n");
END_DEBUG

	xfer = sbp_write_cmd(sdev, FWTCODE_WREQB, 0x08);
	if (xfer == NULL)
		return;
	xfer->act.hand = sbp_cmd_callback;

	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.wreqb.len = htons(8);
	fp->mode.wreqb.extcode = 0;
	fp->mode.wreqb.payload[0] = 
		htonl(((sdev->target->sbp->fd.fc->nodeid | FWLOCALBUS )<< 16));
	fp->mode.wreqb.payload[1] = htonl(vtophys(&ocb->orb[0]));

	if(fw_asyreq(xfer->fc, -1, xfer) != 0){
			fw_xfer_free(xfer);
			ocb->ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ocb->ccb);
	}
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

static struct fw_xfer *
sbp_write_cmd(struct sbp_dev *sdev, int tcode, int offset)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;

	xfer = fw_xfer_alloc();
	if(xfer == NULL){
		return NULL;
	}
	if (tcode == FWTCODE_WREQQ)
		xfer->send.len = 16;
	else
		xfer->send.len = 24;

	xfer->send.buf = malloc(xfer->send.len, M_DEVBUF, M_NOWAIT);
	if(xfer->send.buf == NULL){
		fw_xfer_free( xfer);
		return NULL;
	}

	xfer->send.off = 0; 
	xfer->spd = min(sdev->target->fwdev->speed, max_speed);
	xfer->sc = (caddr_t)sdev;
	xfer->fc = sdev->target->sbp->fd.fc;
	xfer->retry_req = fw_asybusy;

	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.wreqq.dest_hi = htons(sdev->login.cmd_hi);
	fp->mode.wreqq.dest_lo = htonl(sdev->login.cmd_lo + offset);
	fp->mode.wreqq.tlrt = 0;
	fp->mode.wreqq.tcode = tcode;
	fp->mode.wreqq.pri = 0;
	xfer->dst = FWLOCALBUS | sdev->target->fwdev->dst;
	fp->mode.wreqq.dst = htons(xfer->dst);

	return xfer;

}

static void
sbp_mgm_orb(struct sbp_dev *sdev, int func)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	struct sbp_ocb *ocb;
	int s, nid;

	if ((ocb = sbp_get_ocb(sdev->target->sbp)) == NULL) {
		s = splfw();
		sdev->target->sbp->flags |= SBP_RESOURCE_SHORTAGE;
		splx(s);
		return;
	}
	ocb->flags = OCB_ACT_MGM;
	ocb->sdev = sdev;
	ocb->ccb = NULL;

	nid = sdev->target->sbp->fd.fc->nodeid | FWLOCALBUS;
	bzero((void *)(uintptr_t)(volatile void *)ocb->orb, sizeof(ocb->orb));
	ocb->orb[6] = htonl((nid << 16) | SBP_BIND_HI);
	ocb->orb[7] = htonl(SBP_DEV2ADDR(
		device_get_unit(sdev->target->sbp->fd.dev),
		sdev->target->target_id,
		sdev->lun_id));

	sbp_show_sdev_info(sdev, 2);
	printf("%s\n", orb_fun_name[(func>>16)&0xf]);
	switch (func) {
	case ORB_FUN_LGI:
		ocb->orb[2] = htonl(nid << 16);
		ocb->orb[3] = htonl(vtophys(&sdev->login));
		ocb->orb[4] = htonl(ORB_NOTIFY | ORB_EXV | sdev->lun_id);
		ocb->orb[5] = htonl(sizeof(struct sbp_login_res));
		break;
	case ORB_FUN_RCN:
	case ORB_FUN_LGO:
	case ORB_FUN_LUR:
	case ORB_FUN_RST:
	case ORB_FUN_ATA:
	case ORB_FUN_ATS:
		ocb->orb[4] = htonl(ORB_NOTIFY | func | sdev->login.id);
		break;
	}

	xfer = sbp_write_cmd(sdev, FWTCODE_WREQB, 0);
	if(xfer == NULL){
		return;
	}
	xfer->act.hand = sbp_login_callback;

	fp = (struct fw_pkt *)xfer->send.buf;
	fp->mode.wreqb.dest_hi = htons(sdev->target->mgm_hi);
	fp->mode.wreqb.dest_lo = htonl(sdev->target->mgm_lo);
	fp->mode.wreqb.len = htons(8);
	fp->mode.wreqb.extcode = 0;
	fp->mode.wreqb.payload[0] = htonl(((sdev->target->sbp->fd.fc->nodeid | FWLOCALBUS )<< 16));
	fp->mode.wreqb.payload[1] = htonl(vtophys(&ocb->orb[0]));
	sbp_enqueue_ocb(sdev, ocb);

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
	printf("SCSI status %x sfmt %x valid %x key %x code %x qlfr %x len %d",
		sbp_cmd_status->status,
		sbp_cmd_status->sfmt,
		sbp_cmd_status->valid,
		sbp_cmd_status->s_key,
		sbp_cmd_status->s_code,
		sbp_cmd_status->s_qlfr,
		sbp_status->len
	);
#if 0	 /* XXX */
	if (sbp_cmd_status->status == SCSI_STATUS_CHECK_COND) {
		printf(" %s\n", scsi_sense_key_text[sbp_cmd_status->s_key]);
			scsi_sense_desc(
				sbp_cmd_status->s_code,
				sbp_cmd_status->s_qlfr,
				ocb->ccb->ccb_h.path->device->inq_data
			)
	} else {
		printf("\n");
	}
#else
	printf("\n");
#endif
END_DEBUG


	if(sbp_cmd_status->status == SCSI_STATUS_CHECK_COND ||
			sbp_cmd_status->status == SCSI_STATUS_CMD_TERMINATED){
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
		sense->info[0] = ntohl(sbp_cmd_status->info) & 0xff;
		sense->info[1] =(ntohl(sbp_cmd_status->info) >> 8) & 0xff;
		sense->info[2] =(ntohl(sbp_cmd_status->info) >> 16) & 0xff;
		sense->info[3] =(ntohl(sbp_cmd_status->info) >> 24) & 0xff;
		if (sbp_status->len <= 1)
			/* XXX not scsi status. shouldn't be happened */ 
			sense->extra_len = 0;
		else if (sbp_status->len <= 4)
			/* add_sense_code(_qual), info, cmd_spec_info */
			sense->extra_len = 6;
		else
			/* fru, sense_key_spec */
			sense->extra_len = 10;
		sense->cmd_spec_info[0] = ntohl(sbp_cmd_status->cdb) & 0xff;
		sense->cmd_spec_info[1] = (ntohl(sbp_cmd_status->cdb) >> 8) & 0xff;
		sense->cmd_spec_info[2] = (ntohl(sbp_cmd_status->cdb) >> 16) & 0xff;
		sense->cmd_spec_info[3] = (ntohl(sbp_cmd_status->cdb) >> 24) & 0xff;
		sense->add_sense_code = sbp_cmd_status->s_code;
		sense->add_sense_code_qual = sbp_cmd_status->s_qlfr;
		sense->fru = sbp_cmd_status->fru;
		sense->sense_key_spec[0] = ntohl(sbp_cmd_status->s_keydep) & 0xff;
		sense->sense_key_spec[1] = (ntohl(sbp_cmd_status->s_keydep) >>8) & 0xff;
		sense->sense_key_spec[2] = (ntohl(sbp_cmd_status->s_keydep) >>16) & 0xff;

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
	} else {
		printf("sbp_scsi_status: unknown scsi status\n");
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
		/* 
		 * XXX Convert Direct Access device to RBC.
		 * I've never seen Firewire DA devices which support READ_6.
		 */
#if 1
		if (SID_TYPE(inq) == T_DIRECT)
			inq->device |= T_RBC; /*  T_DIRECT == 0 */
#endif
		/* fall through */
	case T_RBC:
		/* disable tag queuing */
		inq->flags &= ~SID_CmdQue;
		/*
		 * Override vendor/product/revision information.
		 * Some devices sometimes return strange strings.
		 */
		bcopy(sdev->vendor, inq->vendor, sizeof(inq->vendor));
		bcopy(sdev->product, inq->product, sizeof(inq->product));
		bcopy(sdev->revision+2, inq->revision, sizeof(inq->revision));
		break;
	}
}

static void
sbp_recv1(struct fw_xfer *xfer){
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
	int	orb_fun, status_valid;
	u_int32_t addr;
/*
	u_int32_t *ld;
	ld = xfer->recv.buf;
printf("sbp %x %d %d %08x %08x %08x %08x\n",
			xfer->resp, xfer->recv.len, xfer->recv.off, ntohl(ld[0]), ntohl(ld[1]), ntohl(ld[2]), ntohl(ld[3]));
printf("sbp %08x %08x %08x %08x\n", ntohl(ld[4]), ntohl(ld[5]), ntohl(ld[6]), ntohl(ld[7]));
printf("sbp %08x %08x %08x %08x\n", ntohl(ld[8]), ntohl(ld[9]), ntohl(ld[10]), ntohl(ld[11]));
*/
	if(xfer->resp != 0){
		printf("sbp_recv: xfer->resp != 0\n");
		fw_xfer_free( xfer);
		return;
	}
	if(xfer->recv.buf == NULL){
		printf("sbp_recv: xfer->recv.buf == NULL\n");
		fw_xfer_free( xfer);
		return;
	}
	sbp = (struct sbp_softc *)xfer->sc;
	rfp = (struct fw_pkt *)xfer->recv.buf;
	if(rfp->mode.wreqb.tcode != FWTCODE_WREQB){
		printf("sbp_recv: tcode = %d\n", rfp->mode.wreqb.tcode);
		fw_xfer_free( xfer);
		return;
	}
	sbp_status = (struct sbp_status *)rfp->mode.wreqb.payload;
	addr = ntohl(rfp->mode.wreqb.dest_lo);
SBP_DEBUG(2)
	printf("received address 0x%x\n", addr);
END_DEBUG
	target = &sbp->targets[SBP_ADDR2TRG(addr)];
	sdev = &target->luns[SBP_ADDR2LUN(addr)];

	status_valid = (sbp_status->resp == ORB_RES_CMPL
			&& sbp_status->dead == 0
			&& sbp_status->status == 0);

SBP_DEBUG(0)
	if (!status_valid || debug > 1){
		int status;

		sbp_show_sdev_info(sdev, 2);
		printf("ORB status src:%x resp:%x dead:%x"
				" len:%x stat:%x orb:%x%08x\n",
			sbp_status->src, sbp_status->resp, sbp_status->dead,
			sbp_status->len, sbp_status->status,
			ntohl(sbp_status->orb_hi), ntohl(sbp_status->orb_lo));
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
			printf("Object: %s, Serial Bus Error: %s\n",
				orb_status1_object[(status>>6) & 3],
				orb_status1_serial_bus_error[status & 0xf]);
			break;
		default:
			printf("unknown respose code\n");
		}
	}
END_DEBUG
	ocb = sbp_dequeue_ocb(sdev, ntohl(sbp_status->orb_lo));

	/* we have to reset the fetch agent if it's dead */
	if (sbp_status->dead) {
		if (sdev->path)
			xpt_freeze_devq(sdev->path, 1);
		sbp_agent_reset(sdev, 0);
	}


	if (ocb == NULL) {
		printf("No ocb on the queue for target %d.\n", sdev->target->target_id);
		fw_xfer_free( xfer);
		return;
	}

	switch(ntohl(ocb->orb[4]) & ORB_FMT_MSK){
	case ORB_FMT_NOP:
		break;
	case ORB_FMT_VED:
		break;
	case ORB_FMT_STD:
		switch(ocb->flags & OCB_ACT_MASK){
		case OCB_ACT_MGM:
			orb_fun = ntohl(ocb->orb[4]) & ORB_FUN_MSK;
			switch(orb_fun) {
			case ORB_FUN_LGI:
				login_res = &sdev->login;
				login_res->len = ntohs(login_res->len);
				login_res->id = ntohs(login_res->id);
				login_res->cmd_hi = ntohs(login_res->cmd_hi);
				login_res->cmd_lo = ntohl(login_res->cmd_lo);
				if (status_valid) {
SBP_DEBUG(0)
sbp_show_sdev_info(sdev, 2);
printf("login: len %d, ID %d, cmd %08x%08x, recon_hold %d\n", login_res->len, login_res->id, login_res->cmd_hi, login_res->cmd_lo, ntohs(login_res->recon_hold));
END_DEBUG
#if 1
					sbp_busy_timeout(sdev);
#else
					sbp_mgm_orb(sdev, ORB_FUN_ATS);
#endif
				} else {
					/* forgot logout ? */
					printf("login failed\n");
					sdev->status = SBP_DEV_RESET;
				}
				break;
			case ORB_FUN_RCN:
				login_res = &sdev->login;
				if (status_valid) {
					sdev->status = SBP_DEV_ATTACHED;
SBP_DEBUG(0)
sbp_show_sdev_info(sdev, 2);
printf("reconnect: len %d, ID %d, cmd %08x%08x\n", login_res->len, login_res->id, login_res->cmd_hi, login_res->cmd_lo);
END_DEBUG
#if 1
					sbp_ping_unit(sdev);
					xpt_release_devq(sdev->path, 1, TRUE);
#else
					sbp_mgm_orb(sdev, ORB_FUN_ATS);
#endif
				} else {
					/* reconnection hold time exceed? */
					printf("reconnect failed\n");
					sbp_mgm_orb(sdev, ORB_FUN_LGI);
				}
				break;
			case ORB_FUN_LGO:
				sdev->status = SBP_DEV_RESET;
				break;
			case ORB_FUN_LUR:
			case ORB_FUN_RST:
			case ORB_FUN_ATA:
			case ORB_FUN_ATS:
				if (sdev->status == SBP_DEV_ATTACHED) {
					xpt_release_devq(sdev->path, 1, TRUE);
				} else {
					sbp_busy_timeout(sdev);
				}
				break;
			default:
				break;
			}
			break;
		case OCB_ACT_CMD:
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

	if (!(ocb->flags & OCB_RESERVED))
		sbp_free_ocb(sbp, ocb);

/* The received packet is usually small enough to be stored within
 * the buffer. In that case, the controller return ack_complete and
 * no respose is necessary.
 *
 * XXX fwohci.c and firewire.c should inform event_code such as 
 * ack_complete or ack_pending to upper driver.
 */
#if NEED_RESPONSE
	xfer->send.buf = malloc(12, M_SBP, M_NOWAIT | M_ZERO);
	xfer->send.len = 12;
	xfer->send.off = 0;
	sfp = (struct fw_pkt *)xfer->send.buf;
	sfp->mode.wres.dst = rfp->mode.wreqb.src;
	xfer->dst = ntohs(sfp->mode.wres.dst);
	xfer->spd = min(sdev->target->fwdev->speed, max_speed);
	xfer->act.hand = sbp_loginres_callback;
	xfer->retry_req = fw_asybusy;

	sfp->mode.wres.tlrt = rfp->mode.wreqb.tlrt;
	sfp->mode.wres.tcode = FWTCODE_WRES;
	sfp->mode.wres.rtcode = 0;
	sfp->mode.wres.pri = 0;

	fw_asyreq(xfer->fc, -1, xfer);
#else
	fw_xfer_free(xfer);
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
	printf("sbp_attach\n");
END_DEBUG

	sbp = ((struct sbp_softc *)device_get_softc(dev));
	bzero(sbp, sizeof(struct sbp_softc));
	sbp->fd.dev = dev;
	sbp->fd.fc = device_get_ivars(dev);
	error = bus_dma_tag_create(/*parent*/NULL, /*alignment*/1,
				/*boundary*/0,
				/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
				/*highaddr*/BUS_SPACE_MAXADDR,
				/*filter*/NULL, /*filterarg*/NULL,
				/*maxsize*/0x100000, /*nsegments*/SBP_IND_MAX,
				/*maxsegsz*/0x8000,
				/*flags*/BUS_DMA_ALLOCNOW,
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
				 /*untagged*/ SBP_QUEUE_LEN,
				 /*tagged*/0, devq);

	if (sbp->sim == NULL) {
		cam_simq_free(devq);
		return (ENXIO);
	}

	sbp->ocb = (struct sbp_ocb *) contigmalloc(
		sizeof (struct sbp_ocb) * SBP_NUM_OCB,
		M_SBP, M_DONTWAIT, 0x10000, 0xffffffff, PAGE_SIZE, 0ul);
	bzero(sbp->ocb, sizeof (struct sbp_ocb) * SBP_NUM_OCB);

	if (sbp->ocb == NULL) {
		printf("sbp0: ocb alloction failure\n");
		return (ENOMEM);
	}

	STAILQ_INIT(&sbp->free_ocbs);
	for (i = 0; i < SBP_NUM_OCB; i++) {
		sbp_free_ocb(sbp, &sbp->ocb[i]);
	}

	if (xpt_bus_register(sbp->sim, /*bus*/0) != CAM_SUCCESS) {
		cam_sim_free(sbp->sim, /*free_devq*/TRUE);
		contigfree(sbp->ocb, sizeof (struct sbp_ocb) * SBP_NUM_OCB,
									M_SBP);
		return (ENXIO);
	}

	xfer = fw_xfer_alloc();
	xfer->act.hand = sbp_recv;
	xfer->act_type = FWACT_XFER;
#if NEED_RESPONSE
	xfer->fc = sbp->fd.fc;
#endif
	xfer->sc = (caddr_t)sbp;

	sbp->fwb.start_hi = SBP_BIND_HI;
	sbp->fwb.start_lo = SBP_DEV2ADDR(device_get_unit(sbp->fd.dev), 0, 0);
	/* We reserve 16 bit space (4 bytes X 64 targets X 256 luns) */
	sbp->fwb.addrlen = 0xffff;
	sbp->fwb.xfer = xfer;
	fw_bindadd(sbp->fd.fc, &sbp->fwb);

	sbp->fd.post_explore = sbp_post_explore;
	s = splfw();
	sbp_post_explore((void *)sbp);
	splx(s);

	return (0);
}

static int
sbp_detach(device_t dev)
{
	struct sbp_softc *sbp = ((struct sbp_softc *)device_get_softc(dev));
	struct firewire_comm *fc = sbp->fd.fc;
	int i;

SBP_DEBUG(0)
	printf("sbp_detach\n");
END_DEBUG

	/* bus reset for logout */
	sbp->fd.post_explore = NULL;
	fc->ibr(fc);
	
	contigfree(sbp->ocb, sizeof (struct sbp_ocb) * SBP_NUM_OCB, M_SBP);
	fw_bindremove(fc, &sbp->fwb);
	for (i = 0; i < SBP_NUM_TARGETS; i ++) 
		sbp_detach_target(&sbp->targets[i]);
	xpt_bus_deregister(cam_sim_path(sbp->sim));
	bus_dma_tag_destroy(sbp->dmat);
	return (0);
}

static void
sbp_detach_target(struct sbp_target *target)
{
	int i;
	struct sbp_dev *sdev;

	if (target->luns != NULL) {
		printf("sbp_detach_target %d\n", target->target_id);
		for (i=0; i < target->num_lun; i++) {
			sdev = &target->luns[i];
			if (sdev->status == SBP_DEV_RESET ||
					sdev->status == SBP_DEV_DEAD)
				continue;
			if (sdev->path)
				xpt_async(AC_LOST_DEVICE, sdev->path, NULL);
			xpt_free_path(sdev->path);
			sdev->path = NULL;
			sbp_abort_all_ocbs(sdev, CAM_DEV_NOT_THERE);
		}
		free(target->luns, M_SBP);
		target->luns = NULL;
	}
	target->fwdev = NULL;
}

static void
sbp_timeout(void *arg)
{
	struct sbp_ocb *ocb = (struct sbp_ocb *)arg;
	struct sbp_dev *sdev = ocb->sdev;
	int s;

	sbp_show_sdev_info(sdev, 2);
	printf("request timeout ... requeue\n");

	/* XXX need reset? */

	s = splfw();
	sbp_abort_all_ocbs(sdev, CAM_CMD_TIMEOUT);
	splx(s);
	return;
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
			sdev = &target->luns[ccb->ccb_h.target_lun];
			if (sdev->status != SBP_DEV_ATTACHED &&
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

			ccb->ccb_h.status = CAM_TID_INVALID;
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
			ccb->ccb_h.status = CAM_TID_INVALID;
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
		int s, speed;
		void *cdb;

		csio = &ccb->csio;

SBP_DEBUG(1)
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
		if ((ocb = sbp_get_ocb(sbp)) == NULL) {
			s = splfw();
			sbp->flags |= SBP_RESOURCE_SHORTAGE;
			splx(s);
			return;
		}
		ocb->flags = OCB_ACT_CMD;
		ocb->sdev = sdev;
		ocb->ccb = ccb;
		ccb->ccb_h.ccb_sdev_ptr = sdev;
		ocb->orb[0] = htonl(1 << 31);
		ocb->orb[1] = 0;
		ocb->orb[2] = htonl(((sbp->fd.fc->nodeid | FWLOCALBUS )<< 16) );
		ocb->orb[3] = htonl(vtophys(ocb->ind_ptr));
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
		bcopy(cdb,
			(void *)(uintptr_t)(volatile void *)&ocb->orb[5],
				csio->cdb_len);
/*
printf("ORB %08x %08x %08x %08x\n", ntohl(ocb->orb[0]), ntohl(ocb->orb[1]), ntohl(ocb->orb[2]), ntohl(ocb->orb[3]));
printf("ORB %08x %08x %08x %08x\n", ntohl(ocb->orb[4]), ntohl(ocb->orb[5]), ntohl(ocb->orb[6]), ntohl(ocb->orb[7]));
*/
		if (ccb->csio.dxfer_len > 0) {
			int s;

			if (bus_dmamap_create(sbp->dmat, 0, &ocb->dmamap)) {
				printf("sbp_action1: cannot create dmamap\n");
				break;
			}

			s = splsoftvm();
			bus_dmamap_load(/*dma tag*/sbp->dmat,
					/*dma map*/ocb->dmamap,
					ccb->csio.data_ptr,
					ccb->csio.dxfer_len,
					sbp_execute_ocb,
					ocb,
					/*flags*/0);
			splx(s);
		} else
			sbp_execute_ocb(ocb, NULL, 0, 0);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		struct ccb_calc_geometry *ccg;
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;
		int extended = 1;
		ccg = &ccb->ccg;

		if (ccg->block_size == 0) {
			printf("sbp_action1: block_size is 0.\n");
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			break;
		}
SBP_DEBUG(1)
		printf("%s:%d:%d:%d:XPT_CALC_GEOMETRY: "
			"Volume size = %d\n",
			device_get_nameunit(sbp->fd.dev), cam_sim_path(sbp->sim),
			ccb->ccb_h.target_id, ccb->ccb_h.target_lun,
			ccg->volume_size);
END_DEBUG

		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);

		if (size_mb >= 1024 && extended) {
			ccg->heads = 255;
			ccg->secs_per_track = 63;
		} else {
			ccg->heads = 64;
			ccg->secs_per_track = 32;
		}
		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;
		ccb->ccb_h.status = CAM_REQ_CMP;
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
		cpi->hba_inquiry = 0;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
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
		/* Disable disconnect and tagged queuing */
		cts->valid = CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;
		cts->flags = 0;

		cts->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_ABORT:
		ccb->ccb_h.status = CAM_UA_ABORT;
		xpt_done(ccb);
		break;
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
	union ccb *ccb;
	bus_dma_segment_t *s;

	if (error)
		printf("sbp_execute_ocb: error=%d\n", error);

	ocb = (struct sbp_ocb *)arg;
	if (seg == 1) {
		/* direct pointer */
		ocb->orb[3] = htonl(segments[0].ds_addr);
		ocb->orb[4] |= htonl(segments[0].ds_len);
	} else if(seg > 1) {
		/* page table */
SBP_DEBUG(1)
		printf("sbp_execute_ocb: seg %d", seg);
		for (i = 0; i < seg; i++)
			printf(", %tx:%zd", segments[i].ds_addr,
						segments[i].ds_len);
		printf("\n");
END_DEBUG
		for (i = 0; i < seg; i++) {
			s = &segments[i];
#if 1			/* XXX LSI Logic "< 16 byte" bug might be hit */
			if (s->ds_len < 16)
				printf("sbp_execute_ocb: warning, "
					"segment length(%zd) is less than 16."
					"(seg=%d/%d)\n", s->ds_len, i+1, seg);
#endif
			ocb->ind_ptr[i].hi = htonl(s->ds_len << 16);
			ocb->ind_ptr[i].lo = htonl(s->ds_addr);
		}
		ocb->orb[4] |= htonl(ORB_CMD_PTBL | seg);
	}
	
	ccb = ocb->ccb;
	prev = sbp_enqueue_ocb(ocb->sdev, ocb);
	if (prev)
		sbp_doorbell(ocb->sdev);
	else
		sbp_orb_pointer(ocb->sdev, ocb); 
}

static void
sbp_poll(struct cam_sim *sim)
{       
	/* should call fwohci_intr? */
	return;
}
static struct sbp_ocb *
sbp_dequeue_ocb(struct sbp_dev *sdev, u_int32_t orb_lo)
{
	struct sbp_ocb *ocb;
	struct sbp_ocb *next;
	int s = splfw(), order = 0;
	int flags;

	for (ocb = STAILQ_FIRST(&sdev->ocbs); ocb != NULL; ocb = next) {
		next = STAILQ_NEXT(ocb, ocb);
		flags = ocb->flags;
SBP_DEBUG(1)
		printf("orb: 0x%tx next: 0x%x, flags %x\n",
			vtophys(&ocb->orb[0]), ntohl(ocb->orb[1]), flags);
END_DEBUG
		if (vtophys(&ocb->orb[0]) == orb_lo) {
			/* found */
			if (ocb->flags & OCB_RESERVED)
				ocb->flags |= OCB_DONE;
			else
				STAILQ_REMOVE(&sdev->ocbs, ocb, sbp_ocb, ocb);
			if (ocb->ccb != NULL)
				untimeout(sbp_timeout, (caddr_t)ocb,
						ocb->ccb->ccb_h.timeout_ch);
			if (ocb->dmamap != NULL) {
				bus_dmamap_destroy(sdev->target->sbp->dmat,
							ocb->dmamap);
				ocb->dmamap = NULL;
			}
			break;
		} else {
			if ((ocb->flags & OCB_RESERVED) &&
					(ocb->flags & OCB_DONE)) {
				/* next orb must be fetched already */
				STAILQ_REMOVE(&sdev->ocbs, ocb, sbp_ocb, ocb);
				sbp_free_ocb(sdev->target->sbp, ocb);
			} else
				order ++;
		}
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

SBP_DEBUG(2)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_enqueue_ocb orb=0x%tx in physical memory\n", vtophys(&ocb->orb[0]));
END_DEBUG
	prev = STAILQ_LAST(&sdev->ocbs, sbp_ocb, ocb);
	STAILQ_INSERT_TAIL(&sdev->ocbs, ocb, ocb);

	if (ocb->ccb != NULL)
		ocb->ccb->ccb_h.timeout_ch = timeout(sbp_timeout, (caddr_t)ocb,
					(ocb->ccb->ccb_h.timeout * hz) / 1000);

	if (prev != NULL
		&& ((prev->flags & OCB_ACT_MASK) == OCB_ACT_CMD)
		&& ((ocb->flags & OCB_ACT_MASK) == OCB_ACT_CMD)) {
SBP_DEBUG(1)
	printf("linking chain 0x%tx -> 0x%tx\n", vtophys(&prev->orb[0]),
			vtophys(&ocb->orb[0]));
END_DEBUG
		prev->flags |= OCB_RESERVED;
		prev->orb[1] = htonl(vtophys(&ocb->orb[0]));
		prev->orb[0] = 0;
	} else {
		prev = NULL;
	}
	splx(s);

	return prev;
}

static struct sbp_ocb *
sbp_get_ocb(struct sbp_softc *sbp)
{
	struct sbp_ocb *ocb;
	int s = splfw();
	ocb = STAILQ_FIRST(&sbp->free_ocbs);
	if (ocb == NULL) {
		printf("ocb shortage!!!\n");
		return NULL;
	}
	STAILQ_REMOVE(&sbp->free_ocbs, ocb, sbp_ocb, ocb);
	splx(s);
	ocb->ccb = NULL;
	return (ocb);
}

static void
sbp_free_ocb(struct sbp_softc *sbp, struct sbp_ocb *ocb)
{
#if 0 /* XXX make sure that ocb has ccb */
	if ((sbp->flags & SBP_RESOURCE_SHORTAGE) != 0 &&
	    (ocb->ccb->ccb_h.status & CAM_RELEASE_SIMQ) == 0) {
		ocb->ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		sbp->flags &= ~SBP_RESOURCE_SHORTAGE;
	}
#else
	if ((sbp->flags & SBP_RESOURCE_SHORTAGE) != 0)
		sbp->flags &= ~SBP_RESOURCE_SHORTAGE;
#endif
	ocb->flags = 0;
	ocb->ccb = NULL;
	STAILQ_INSERT_TAIL(&sbp->free_ocbs, ocb, ocb);
}

static void
sbp_abort_ocb(struct sbp_ocb *ocb, int status)
{
	struct sbp_dev *sdev;

	sdev = ocb->sdev;
SBP_DEBUG(0)
	sbp_show_sdev_info(sdev, 2);
	printf("sbp_abort_ocb 0x%x\n", status);
	if (ocb->ccb != NULL)
		sbp_print_scsi_cmd(ocb);
END_DEBUG
	if (ocb->ccb != NULL && !(ocb->flags & OCB_DONE)) {
		if (status != CAM_CMD_TIMEOUT)
			untimeout(sbp_timeout, (caddr_t)ocb,
						ocb->ccb->ccb_h.timeout_ch);
		ocb->ccb->ccb_h.status = status;
		xpt_done(ocb->ccb);
	}
	if (ocb->dmamap != NULL) {
		bus_dmamap_destroy(sdev->target->sbp->dmat, ocb->dmamap);
		ocb->dmamap = NULL;
	}
	sbp_free_ocb(sdev->target->sbp, ocb);
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
