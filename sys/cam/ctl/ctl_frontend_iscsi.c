/*-
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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

/*
 * CTL frontend for the iSCSI protocol.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/condvar.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <vm/uma.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_error.h>
#include <cam/ctl/ctl_frontend.h>
#include <cam/ctl/ctl_frontend_internal.h>
#include <cam/ctl/ctl_debug.h>
#include <cam/ctl/ctl_ha.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_private.h>

#include "../../dev/iscsi/icl.h"
#include "../../dev/iscsi/iscsi_proto.h"
#include "ctl_frontend_iscsi.h"

#ifdef ICL_KERNEL_PROXY
#include <sys/socketvar.h>
#endif

#ifdef ICL_KERNEL_PROXY
FEATURE(cfiscsi_kernel_proxy, "iSCSI target built with ICL_KERNEL_PROXY");
#endif

static MALLOC_DEFINE(M_CFISCSI, "cfiscsi", "Memory used for CTL iSCSI frontend");
static uma_zone_t cfiscsi_data_wait_zone;

SYSCTL_NODE(_kern_cam_ctl, OID_AUTO, iscsi, CTLFLAG_RD, 0,
    "CAM Target Layer iSCSI Frontend");
static int debug = 3;
TUNABLE_INT("kern.cam.ctl.iscsi.debug", &debug);
SYSCTL_INT(_kern_cam_ctl_iscsi, OID_AUTO, debug, CTLFLAG_RWTUN,
    &debug, 1, "Enable debug messages");
static int ping_timeout = 5;
TUNABLE_INT("kern.cam.ctl.iscsi.ping_timeout", &ping_timeout);
SYSCTL_INT(_kern_cam_ctl_iscsi, OID_AUTO, ping_timeout, CTLFLAG_RWTUN,
    &ping_timeout, 5, "Interval between ping (NOP-Out) requests, in seconds");
static int login_timeout = 60;
TUNABLE_INT("kern.cam.ctl.iscsi.login_timeout", &login_timeout);
SYSCTL_INT(_kern_cam_ctl_iscsi, OID_AUTO, login_timeout, CTLFLAG_RWTUN,
    &login_timeout, 60, "Time to wait for ctld(8) to finish Login Phase, in seconds");
static int maxcmdsn_delta = 256;
TUNABLE_INT("kern.cam.ctl.iscsi.maxcmdsn_delta", &maxcmdsn_delta);
SYSCTL_INT(_kern_cam_ctl_iscsi, OID_AUTO, maxcmdsn_delta, CTLFLAG_RWTUN,
    &maxcmdsn_delta, 256, "Number of commands the initiator can send "
    "without confirmation");

#define	CFISCSI_DEBUG(X, ...)						\
	do {								\
		if (debug > 1) {					\
			printf("%s: " X "\n",				\
			    __func__, ## __VA_ARGS__);			\
		}							\
	} while (0)

#define	CFISCSI_WARN(X, ...)						\
	do {								\
		if (debug > 0) {					\
			printf("WARNING: %s: " X "\n",			\
			    __func__, ## __VA_ARGS__);			\
		}							\
	} while (0)

#define	CFISCSI_SESSION_DEBUG(S, X, ...)				\
	do {								\
		if (debug > 1) {					\
			printf("%s: %s (%s): " X "\n",			\
			    __func__, S->cs_initiator_addr,		\
			    S->cs_initiator_name, ## __VA_ARGS__);	\
		}							\
	} while (0)

#define	CFISCSI_SESSION_WARN(S, X, ...)					\
	do  {								\
		if (debug > 0) {					\
			printf("WARNING: %s (%s): " X "\n",		\
			    S->cs_initiator_addr,			\
			    S->cs_initiator_name, ## __VA_ARGS__);	\
		}							\
	} while (0)

#define CFISCSI_SESSION_LOCK(X)		mtx_lock(&X->cs_lock)
#define CFISCSI_SESSION_UNLOCK(X)	mtx_unlock(&X->cs_lock)
#define CFISCSI_SESSION_LOCK_ASSERT(X)	mtx_assert(&X->cs_lock, MA_OWNED)

#define	CONN_SESSION(X)			((struct cfiscsi_session *)(X)->ic_prv0)
#define	PDU_SESSION(X)			CONN_SESSION((X)->ip_conn)
#define	PDU_EXPDATASN(X)		(X)->ip_prv0
#define	PDU_TOTAL_TRANSFER_LEN(X)	(X)->ip_prv1
#define	PDU_R2TSN(X)			(X)->ip_prv2

int		cfiscsi_init(void);
static void	cfiscsi_online(void *arg);
static void	cfiscsi_offline(void *arg);
static int	cfiscsi_targ_enable(void *arg, struct ctl_id targ_id);
static int	cfiscsi_targ_disable(void *arg, struct ctl_id targ_id);
static int	cfiscsi_lun_enable(void *arg,
		    struct ctl_id target_id, int lun_id);
static int	cfiscsi_lun_disable(void *arg,
		    struct ctl_id target_id, int lun_id);
static int	cfiscsi_ioctl(struct cdev *dev,
		    u_long cmd, caddr_t addr, int flag, struct thread *td);
static int	cfiscsi_devid(struct ctl_scsiio *ctsio, int alloc_len);
static void	cfiscsi_datamove(union ctl_io *io);
static void	cfiscsi_done(union ctl_io *io);
static uint32_t	cfiscsi_map_lun(void *arg, uint32_t lun);
static bool	cfiscsi_pdu_update_cmdsn(const struct icl_pdu *request);
static void	cfiscsi_pdu_handle_nop_out(struct icl_pdu *request);
static void	cfiscsi_pdu_handle_scsi_command(struct icl_pdu *request);
static void	cfiscsi_pdu_handle_task_request(struct icl_pdu *request);
static void	cfiscsi_pdu_handle_data_out(struct icl_pdu *request);
static void	cfiscsi_pdu_handle_logout_request(struct icl_pdu *request);
static void	cfiscsi_session_terminate(struct cfiscsi_session *cs);
static struct cfiscsi_target	*cfiscsi_target_find(struct cfiscsi_softc
		    *softc, const char *name);
static void	cfiscsi_target_release(struct cfiscsi_target *ct);
static void	cfiscsi_session_delete(struct cfiscsi_session *cs);

static struct cfiscsi_softc cfiscsi_softc;
extern struct ctl_softc *control_softc;

static int cfiscsi_module_event_handler(module_t, int /*modeventtype_t*/, void *);

static moduledata_t cfiscsi_moduledata = {
	"ctlcfiscsi",
	cfiscsi_module_event_handler,
	NULL
};

DECLARE_MODULE(ctlcfiscsi, cfiscsi_moduledata, SI_SUB_CONFIGURE, SI_ORDER_FOURTH);
MODULE_VERSION(ctlcfiscsi, 1);
MODULE_DEPEND(ctlcfiscsi, ctl, 1, 1, 1);
MODULE_DEPEND(ctlcfiscsi, icl, 1, 1, 1);

static struct icl_pdu *
cfiscsi_pdu_new_response(struct icl_pdu *request, int flags)
{

	return (icl_pdu_new_bhs(request->ip_conn, flags));
}

static bool
cfiscsi_pdu_update_cmdsn(const struct icl_pdu *request)
{
	const struct iscsi_bhs_scsi_command *bhssc;
	struct cfiscsi_session *cs;
	uint32_t cmdsn, expstatsn;

	cs = PDU_SESSION(request);

	/*
	 * Every incoming PDU - not just NOP-Out - resets the ping timer.
	 * The purpose of the timeout is to reset the connection when it stalls;
	 * we don't want this to happen when NOP-In or NOP-Out ends up delayed
	 * in some queue.
	 *
	 * XXX: Locking?
	 */
	cs->cs_timeout = 0;

	/*
	 * Data-Out PDUs don't contain CmdSN.
	 */
	if ((request->ip_bhs->bhs_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) ==
	    ISCSI_BHS_OPCODE_SCSI_DATA_OUT)
		return (false);

	/*
	 * We're only using fields common for all the request
	 * (initiator -> target) PDUs.
	 */
	bhssc = (const struct iscsi_bhs_scsi_command *)request->ip_bhs;
	cmdsn = ntohl(bhssc->bhssc_cmdsn);
	expstatsn = ntohl(bhssc->bhssc_expstatsn);

	CFISCSI_SESSION_LOCK(cs);
#if 0
	if (expstatsn != cs->cs_statsn) {
		CFISCSI_SESSION_DEBUG(cs, "received PDU with ExpStatSN %d, "
		    "while current StatSN is %d", expstatsn,
		    cs->cs_statsn);
	}
#endif

	/*
	 * The target MUST silently ignore any non-immediate command outside
	 * of this range.
	 */
	if (cmdsn < cs->cs_cmdsn || cmdsn > cs->cs_cmdsn + maxcmdsn_delta) {
		CFISCSI_SESSION_UNLOCK(cs);
		CFISCSI_SESSION_WARN(cs, "received PDU with CmdSN %d, "
		    "while expected CmdSN was %d", cmdsn, cs->cs_cmdsn);
		return (true);
	}

	if ((request->ip_bhs->bhs_opcode & ISCSI_BHS_OPCODE_IMMEDIATE) == 0)
		cs->cs_cmdsn++;

	CFISCSI_SESSION_UNLOCK(cs);

	return (false);
}

static void
cfiscsi_pdu_handle(struct icl_pdu *request)
{
	struct cfiscsi_session *cs;
	bool ignore;

	cs = PDU_SESSION(request);

	ignore = cfiscsi_pdu_update_cmdsn(request);
	if (ignore) {
		icl_pdu_free(request);
		return;
	}

	/*
	 * Handle the PDU; this includes e.g. receiving the remaining
	 * part of PDU and submitting the SCSI command to CTL
	 * or queueing a reply.  The handling routine is responsible
	 * for freeing the PDU when it's no longer needed.
	 */
	switch (request->ip_bhs->bhs_opcode &
	    ~ISCSI_BHS_OPCODE_IMMEDIATE) {
	case ISCSI_BHS_OPCODE_NOP_OUT:
		cfiscsi_pdu_handle_nop_out(request);
		break;
	case ISCSI_BHS_OPCODE_SCSI_COMMAND:
		cfiscsi_pdu_handle_scsi_command(request);
		break;
	case ISCSI_BHS_OPCODE_TASK_REQUEST:
		cfiscsi_pdu_handle_task_request(request);
		break;
	case ISCSI_BHS_OPCODE_SCSI_DATA_OUT:
		cfiscsi_pdu_handle_data_out(request);
		break;
	case ISCSI_BHS_OPCODE_LOGOUT_REQUEST:
		cfiscsi_pdu_handle_logout_request(request);
		break;
	default:
		CFISCSI_SESSION_WARN(cs, "received PDU with unsupported "
		    "opcode 0x%x; dropping connection",
		    request->ip_bhs->bhs_opcode);
		icl_pdu_free(request);
		cfiscsi_session_terminate(cs);
	}

}

static void
cfiscsi_receive_callback(struct icl_pdu *request)
{
	struct cfiscsi_session *cs;

	cs = PDU_SESSION(request);

#ifdef ICL_KERNEL_PROXY
	if (cs->cs_waiting_for_ctld || cs->cs_login_phase) {
		if (cs->cs_login_pdu == NULL)
			cs->cs_login_pdu = request;
		else
			icl_pdu_free(request);
		cv_signal(&cs->cs_login_cv);
		return;
	}
#endif

	cfiscsi_pdu_handle(request);
}

static void
cfiscsi_error_callback(struct icl_conn *ic)
{
	struct cfiscsi_session *cs;

	cs = CONN_SESSION(ic);

	CFISCSI_SESSION_WARN(cs, "connection error; dropping connection");
	cfiscsi_session_terminate(cs);
}

static int
cfiscsi_pdu_prepare(struct icl_pdu *response)
{
	struct cfiscsi_session *cs;
	struct iscsi_bhs_scsi_response *bhssr;
	bool advance_statsn = true;

	cs = PDU_SESSION(response);

	CFISCSI_SESSION_LOCK_ASSERT(cs);

	/*
	 * We're only using fields common for all the response
	 * (target -> initiator) PDUs.
	 */
	bhssr = (struct iscsi_bhs_scsi_response *)response->ip_bhs;

	/*
	 * 10.8.3: "The StatSN for this connection is not advanced
	 * after this PDU is sent."
	 */
	if (bhssr->bhssr_opcode == ISCSI_BHS_OPCODE_R2T)
		advance_statsn = false;

	/*
	 * 10.19.2: "However, when the Initiator Task Tag is set to 0xffffffff,
	 * StatSN for the connection is not advanced after this PDU is sent."
	 */
	if (bhssr->bhssr_opcode == ISCSI_BHS_OPCODE_NOP_IN && 
	    bhssr->bhssr_initiator_task_tag == 0xffffffff)
		advance_statsn = false;

	/*
	 * See the comment below - StatSN is not meaningful and must
	 * not be advanced.
	 */
	if (bhssr->bhssr_opcode == ISCSI_BHS_OPCODE_SCSI_DATA_IN)
		advance_statsn = false;

	/*
	 * 10.7.3: "The fields StatSN, Status, and Residual Count
	 * only have meaningful content if the S bit is set to 1."
	 */
	if (bhssr->bhssr_opcode != ISCSI_BHS_OPCODE_SCSI_DATA_IN)
		bhssr->bhssr_statsn = htonl(cs->cs_statsn);
	bhssr->bhssr_expcmdsn = htonl(cs->cs_cmdsn);
	bhssr->bhssr_maxcmdsn = htonl(cs->cs_cmdsn + maxcmdsn_delta);

	if (advance_statsn)
		cs->cs_statsn++;

	return (0);
}

static void
cfiscsi_pdu_queue(struct icl_pdu *response)
{
	struct cfiscsi_session *cs;

	cs = PDU_SESSION(response);

	CFISCSI_SESSION_LOCK(cs);
	cfiscsi_pdu_prepare(response);
	icl_pdu_queue(response);
	CFISCSI_SESSION_UNLOCK(cs);
}

static uint32_t
cfiscsi_decode_lun(uint64_t encoded)
{
	uint8_t lun[8];
	uint32_t result;

	/*
	 * The LUN field in iSCSI PDUs may look like an ordinary 64 bit number,
	 * but is in fact an evil, multidimensional structure defined
	 * in SCSI Architecture Model 5 (SAM-5), section 4.6.
	 */
	memcpy(lun, &encoded, sizeof(lun));
	switch (lun[0] & 0xC0) {
	case 0x00:
		if ((lun[0] & 0x3f) != 0 || lun[2] != 0 || lun[3] != 0 ||
		    lun[4] != 0 || lun[5] != 0 || lun[6] != 0 || lun[7] != 0) {
			CFISCSI_WARN("malformed LUN "
			    "(peripheral device addressing method): 0x%jx",
			    (uintmax_t)encoded);
			result = 0xffffffff;
			break;
		}
		result = lun[1];
		break;
	case 0x40:
		if (lun[2] != 0 || lun[3] != 0 || lun[4] != 0 || lun[5] != 0 ||
		    lun[6] != 0 || lun[7] != 0) {
			CFISCSI_WARN("malformed LUN "
			    "(flat address space addressing method): 0x%jx",
			    (uintmax_t)encoded);
			result = 0xffffffff;
			break;
		}
		result = ((lun[0] & 0x3f) << 8) + lun[1];
		break;
	case 0xC0:
		if (lun[0] != 0xD2 || lun[4] != 0 || lun[5] != 0 ||
		    lun[6] != 0 || lun[7] != 0) {
			CFISCSI_WARN("malformed LUN (extended flat "
			    "address space addressing method): 0x%jx",
			    (uintmax_t)encoded);
			result = 0xffffffff;
			break;
		}
		result = (lun[1] << 16) + (lun[2] << 8) + lun[3];
	default:
		CFISCSI_WARN("unsupported LUN format 0x%jx",
		    (uintmax_t)encoded);
		result = 0xffffffff;
		break;
	}

	return (result);
}

static void
cfiscsi_pdu_handle_nop_out(struct icl_pdu *request)
{
	struct cfiscsi_session *cs;
	struct iscsi_bhs_nop_out *bhsno;
	struct iscsi_bhs_nop_in *bhsni;
	struct icl_pdu *response;
	void *data = NULL;
	size_t datasize;
	int error;

	cs = PDU_SESSION(request);
	bhsno = (struct iscsi_bhs_nop_out *)request->ip_bhs;

	if (bhsno->bhsno_initiator_task_tag == 0xffffffff) {
		/*
		 * Nothing to do, iscsi_pdu_update_statsn() already
		 * zeroed the timeout.
		 */
		icl_pdu_free(request);
		return;
	}

	datasize = icl_pdu_data_segment_length(request);
	if (datasize > 0) {
		data = malloc(datasize, M_CFISCSI, M_NOWAIT | M_ZERO);
		if (data == NULL) {
			CFISCSI_SESSION_WARN(cs, "failed to allocate memory; "
			    "dropping connection");
			icl_pdu_free(request);
			cfiscsi_session_terminate(cs);
			return;
		}
		icl_pdu_get_data(request, 0, data, datasize);
	}

	response = cfiscsi_pdu_new_response(request, M_NOWAIT);
	if (response == NULL) {
		CFISCSI_SESSION_WARN(cs, "failed to allocate memory; "
		    "droppping connection");
		free(data, M_CFISCSI);
		icl_pdu_free(request);
		cfiscsi_session_terminate(cs);
		return;
	}
	bhsni = (struct iscsi_bhs_nop_in *)response->ip_bhs;
	bhsni->bhsni_opcode = ISCSI_BHS_OPCODE_NOP_IN;
	bhsni->bhsni_flags = 0x80;
	bhsni->bhsni_initiator_task_tag = bhsno->bhsno_initiator_task_tag;
	bhsni->bhsni_target_transfer_tag = 0xffffffff;
	if (datasize > 0) {
		error = icl_pdu_append_data(response, data, datasize, M_NOWAIT);
		if (error != 0) {
			CFISCSI_SESSION_WARN(cs, "failed to allocate memory; "
			    "dropping connection");
			free(data, M_CFISCSI);
			icl_pdu_free(request);
			icl_pdu_free(response);
			cfiscsi_session_terminate(cs);
			return;
		}
		free(data, M_CFISCSI);
	}

	icl_pdu_free(request);
	cfiscsi_pdu_queue(response);
}

static void
cfiscsi_pdu_handle_scsi_command(struct icl_pdu *request)
{
	struct iscsi_bhs_scsi_command *bhssc;
	struct cfiscsi_session *cs;
	union ctl_io *io;
	int error;

	cs = PDU_SESSION(request);
	bhssc = (struct iscsi_bhs_scsi_command *)request->ip_bhs;
	//CFISCSI_SESSION_DEBUG(cs, "initiator task tag 0x%x",
	//    bhssc->bhssc_initiator_task_tag);

	if (request->ip_data_len > 0 && cs->cs_immediate_data == false) {
		CFISCSI_SESSION_WARN(cs, "unsolicited data with "
		    "ImmediateData=No; dropping connection");
		icl_pdu_free(request);
		cfiscsi_session_terminate(cs);
		return;
	}
	io = ctl_alloc_io(cs->cs_target->ct_softc->fe.ctl_pool_ref);
	if (io == NULL) {
		CFISCSI_SESSION_WARN(cs, "can't allocate ctl_io; "
		    "dropping connection");
		icl_pdu_free(request);
		cfiscsi_session_terminate(cs);
		return;
	}
	ctl_zero_io(io);
	io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr = request;
	io->io_hdr.io_type = CTL_IO_SCSI;
	io->io_hdr.nexus.initid.id = cs->cs_ctl_initid;
	io->io_hdr.nexus.targ_port = cs->cs_target->ct_softc->fe.targ_port;
	io->io_hdr.nexus.targ_target.id = 0;
	io->io_hdr.nexus.targ_lun = cfiscsi_decode_lun(bhssc->bhssc_lun);
	io->io_hdr.nexus.lun_map_fn = cfiscsi_map_lun;
	io->io_hdr.nexus.lun_map_arg = cs;
	io->scsiio.tag_num = bhssc->bhssc_initiator_task_tag;
	switch ((bhssc->bhssc_flags & BHSSC_FLAGS_ATTR)) {
	case BHSSC_FLAGS_ATTR_UNTAGGED:
		io->scsiio.tag_type = CTL_TAG_UNTAGGED;
		break;
	case BHSSC_FLAGS_ATTR_SIMPLE:
		io->scsiio.tag_type = CTL_TAG_SIMPLE;
		break;
	case BHSSC_FLAGS_ATTR_ORDERED:
        	io->scsiio.tag_type = CTL_TAG_ORDERED;
		break;
	case BHSSC_FLAGS_ATTR_HOQ:
        	io->scsiio.tag_type = CTL_TAG_HEAD_OF_QUEUE;
		break;
	case BHSSC_FLAGS_ATTR_ACA:
		io->scsiio.tag_type = CTL_TAG_ACA;
		break;
	default:
		io->scsiio.tag_type = CTL_TAG_UNTAGGED;
		CFISCSI_SESSION_WARN(cs, "unhandled tag type %d",
		    bhssc->bhssc_flags & BHSSC_FLAGS_ATTR);
		break;
	}
	io->scsiio.cdb_len = sizeof(bhssc->bhssc_cdb); /* Which is 16. */
	memcpy(io->scsiio.cdb, bhssc->bhssc_cdb, sizeof(bhssc->bhssc_cdb));
	refcount_acquire(&cs->cs_outstanding_ctl_pdus);
	error = ctl_queue(io);
	if (error != CTL_RETVAL_COMPLETE) {
		CFISCSI_SESSION_WARN(cs, "ctl_queue() failed; error %d; "
		    "dropping connection", error);
		ctl_free_io(io);
		refcount_release(&cs->cs_outstanding_ctl_pdus);
		icl_pdu_free(request);
		cfiscsi_session_terminate(cs);
	}
}

static void
cfiscsi_pdu_handle_task_request(struct icl_pdu *request)
{
	struct iscsi_bhs_task_management_request *bhstmr;
	struct iscsi_bhs_task_management_response *bhstmr2;
	struct icl_pdu *response;
	struct cfiscsi_session *cs;
	union ctl_io *io;
	int error;

	cs = PDU_SESSION(request);
	bhstmr = (struct iscsi_bhs_task_management_request *)request->ip_bhs;
	io = ctl_alloc_io(cs->cs_target->ct_softc->fe.ctl_pool_ref);
	if (io == NULL) {
		CFISCSI_SESSION_WARN(cs, "can't allocate ctl_io;"
		    "dropping connection");
		icl_pdu_free(request);
		cfiscsi_session_terminate(cs);
		return;
	}
	ctl_zero_io(io);
	io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr = request;
	io->io_hdr.io_type = CTL_IO_TASK;
	io->io_hdr.nexus.initid.id = cs->cs_ctl_initid;
	io->io_hdr.nexus.targ_port = cs->cs_target->ct_softc->fe.targ_port;
	io->io_hdr.nexus.targ_target.id = 0;
	io->io_hdr.nexus.targ_lun = cfiscsi_decode_lun(bhstmr->bhstmr_lun);
	io->io_hdr.nexus.lun_map_fn = cfiscsi_map_lun;
	io->io_hdr.nexus.lun_map_arg = cs;
	io->taskio.tag_type = CTL_TAG_SIMPLE; /* XXX */

	switch (bhstmr->bhstmr_function & ~0x80) {
	case BHSTMR_FUNCTION_ABORT_TASK:
#if 0
		CFISCSI_SESSION_DEBUG(cs, "BHSTMR_FUNCTION_ABORT_TASK");
#endif
		io->taskio.task_action = CTL_TASK_ABORT_TASK;
		io->taskio.tag_num = bhstmr->bhstmr_referenced_task_tag;
		break;
	case BHSTMR_FUNCTION_LOGICAL_UNIT_RESET:
#if 0
		CFISCSI_SESSION_DEBUG(cs, "BHSTMR_FUNCTION_LOGICAL_UNIT_RESET");
#endif
		io->taskio.task_action = CTL_TASK_LUN_RESET;
		break;
	case BHSTMR_FUNCTION_TARGET_WARM_RESET:
#if 0
		CFISCSI_SESSION_DEBUG(cs, "BHSTMR_FUNCTION_TARGET_WARM_RESET");
#endif
		io->taskio.task_action = CTL_TASK_TARGET_RESET;
		break;
	default:
		CFISCSI_SESSION_DEBUG(cs, "unsupported function 0x%x",
		    bhstmr->bhstmr_function & ~0x80);
		ctl_free_io(io);

		response = cfiscsi_pdu_new_response(request, M_NOWAIT);
		if (response == NULL) {
			CFISCSI_SESSION_WARN(cs, "failed to allocate memory; "
			    "dropping connection");
			icl_pdu_free(request);
			cfiscsi_session_terminate(cs);
			return;
		}
		bhstmr2 = (struct iscsi_bhs_task_management_response *)
		    response->ip_bhs;
		bhstmr2->bhstmr_opcode = ISCSI_BHS_OPCODE_TASK_RESPONSE;
		bhstmr2->bhstmr_flags = 0x80;
		bhstmr2->bhstmr_response =
		    BHSTMR_RESPONSE_FUNCTION_NOT_SUPPORTED;
		bhstmr2->bhstmr_initiator_task_tag =
		    bhstmr->bhstmr_initiator_task_tag;
		icl_pdu_free(request);
		cfiscsi_pdu_queue(response);
		return;
	}

	refcount_acquire(&cs->cs_outstanding_ctl_pdus);
	error = ctl_queue(io);
	if (error != CTL_RETVAL_COMPLETE) {
		CFISCSI_SESSION_WARN(cs, "ctl_queue() failed; error %d; "
		    "dropping connection", error);
		ctl_free_io(io);
		refcount_release(&cs->cs_outstanding_ctl_pdus);
		icl_pdu_free(request);
		cfiscsi_session_terminate(cs);
	}
}

static bool
cfiscsi_handle_data_segment(struct icl_pdu *request, struct cfiscsi_data_wait *cdw)
{
	struct iscsi_bhs_data_out *bhsdo;
	struct cfiscsi_session *cs;
	struct ctl_sg_entry ctl_sg_entry, *ctl_sglist;
	size_t copy_len, len, off, buffer_offset;
	int ctl_sg_count;
	union ctl_io *io;

	cs = PDU_SESSION(request);

	KASSERT((request->ip_bhs->bhs_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) ==
	    ISCSI_BHS_OPCODE_SCSI_DATA_OUT ||
	    (request->ip_bhs->bhs_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) ==
	    ISCSI_BHS_OPCODE_SCSI_COMMAND,
	    ("bad opcode 0x%x", request->ip_bhs->bhs_opcode));

	/*
	 * We're only using fields common for Data-Out and SCSI Command PDUs.
	 */
	bhsdo = (struct iscsi_bhs_data_out *)request->ip_bhs;

	io = cdw->cdw_ctl_io;
	KASSERT((io->io_hdr.flags & CTL_FLAG_DATA_MASK) != CTL_FLAG_DATA_IN,
	    ("CTL_FLAG_DATA_IN"));

#if 0
	CFISCSI_SESSION_DEBUG(cs, "received %zd bytes out of %d",
	    request->ip_data_len, io->scsiio.kern_total_len);
#endif

	if (io->scsiio.kern_sg_entries > 0) {
		ctl_sglist = (struct ctl_sg_entry *)io->scsiio.kern_data_ptr;
		ctl_sg_count = io->scsiio.kern_sg_entries;
	} else {
		ctl_sglist = &ctl_sg_entry;
		ctl_sglist->addr = io->scsiio.kern_data_ptr;
		ctl_sglist->len = io->scsiio.kern_data_len;
		ctl_sg_count = 1;
	}

	if ((request->ip_bhs->bhs_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) ==
	    ISCSI_BHS_OPCODE_SCSI_DATA_OUT)
		buffer_offset = ntohl(bhsdo->bhsdo_buffer_offset);
	else
		buffer_offset = 0;

	/*
	 * Make sure the offset, as sent by the initiator, matches the offset
	 * we're supposed to be at in the scatter-gather list.
	 */
	if (buffer_offset !=
	    io->scsiio.kern_rel_offset + io->scsiio.ext_data_filled) {
		CFISCSI_SESSION_WARN(cs, "received bad buffer offset %zd, "
		    "expected %zd; dropping connection", buffer_offset,
		    (size_t)io->scsiio.kern_rel_offset +
		    (size_t)io->scsiio.ext_data_filled);
		ctl_set_data_phase_error(&io->scsiio);
		cfiscsi_session_terminate(cs);
		return (true);
	}

	/*
	 * This is the offset within the PDU data segment, as opposed
	 * to buffer_offset, which is the offset within the task (SCSI
	 * command).
	 */
	off = 0;
	len = icl_pdu_data_segment_length(request);

	/*
	 * Iterate over the scatter/gather segments, filling them with data
	 * from the PDU data segment.  Note that this can get called multiple
	 * times for one SCSI command; the cdw structure holds state for the
	 * scatter/gather list.
	 */
	for (;;) {
		KASSERT(cdw->cdw_sg_index < ctl_sg_count,
		    ("cdw->cdw_sg_index >= ctl_sg_count"));
		if (cdw->cdw_sg_len == 0) {
			cdw->cdw_sg_addr = ctl_sglist[cdw->cdw_sg_index].addr;
			cdw->cdw_sg_len = ctl_sglist[cdw->cdw_sg_index].len;
		}
		KASSERT(off <= len, ("len > off"));
		copy_len = len - off;
		if (copy_len > cdw->cdw_sg_len)
			copy_len = cdw->cdw_sg_len;

		icl_pdu_get_data(request, off, cdw->cdw_sg_addr, copy_len);
		cdw->cdw_sg_addr += copy_len;
		cdw->cdw_sg_len -= copy_len;
		off += copy_len;
		io->scsiio.ext_data_filled += copy_len;

		if (cdw->cdw_sg_len == 0) {
			/*
			 * End of current segment.
			 */
			if (cdw->cdw_sg_index == ctl_sg_count - 1) {
				/*
				 * Last segment in scatter/gather list.
				 */
				break;
			}
			cdw->cdw_sg_index++;
		}

		if (off == len) {
			/*
			 * End of PDU payload.
			 */
			break;
		}
	}

	if (len > off) {
		/*
		 * In case of unsolicited data, it's possible that the buffer
		 * provided by CTL is smaller than negotiated FirstBurstLength.
		 * Just ignore the superfluous data; will ask for them with R2T
		 * on next call to cfiscsi_datamove().
		 *
		 * This obviously can only happen with SCSI Command PDU. 
		 */
		if ((request->ip_bhs->bhs_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) ==
		    ISCSI_BHS_OPCODE_SCSI_COMMAND) {
			CFISCSI_SESSION_DEBUG(cs, "received too much immediate "
			    "data: got %zd bytes, expected %zd",
			    icl_pdu_data_segment_length(request), off);
			return (true);
		}

		CFISCSI_SESSION_WARN(cs, "received too much data: got %zd bytes, "
		    "expected %zd; dropping connection",
		    icl_pdu_data_segment_length(request), off);
		ctl_set_data_phase_error(&io->scsiio);
		cfiscsi_session_terminate(cs);
		return (true);
	}

	if (io->scsiio.ext_data_filled == io->scsiio.kern_data_len &&
	    (bhsdo->bhsdo_flags & BHSDO_FLAGS_F) == 0) {
		CFISCSI_SESSION_WARN(cs, "got the final packet without "
		    "the F flag; flags = 0x%x; dropping connection",
		    bhsdo->bhsdo_flags);
		ctl_set_data_phase_error(&io->scsiio);
		cfiscsi_session_terminate(cs);
		return (true);
	}

	if (io->scsiio.ext_data_filled != io->scsiio.kern_data_len &&
	    (bhsdo->bhsdo_flags & BHSDO_FLAGS_F) != 0) {
		if ((request->ip_bhs->bhs_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) ==
		    ISCSI_BHS_OPCODE_SCSI_DATA_OUT) {
			CFISCSI_SESSION_WARN(cs, "got the final packet, but the "
			    "transmitted size was %zd bytes instead of %d; "
			    "dropping connection",
			    (size_t)io->scsiio.ext_data_filled,
			    io->scsiio.kern_data_len);
			ctl_set_data_phase_error(&io->scsiio);
			cfiscsi_session_terminate(cs);
			return (true);
		} else {
			/*
			 * For SCSI Command PDU, this just means we need to
			 * solicit more data by sending R2T.
			 */
			return (false);
		}
	}

	if (io->scsiio.ext_data_filled == io->scsiio.kern_data_len) {
#if 0
		CFISCSI_SESSION_DEBUG(cs, "no longer expecting Data-Out with target "
		    "transfer tag 0x%x", cdw->cdw_target_transfer_tag);
#endif

		return (true);
	}

	return (false);
}

static void
cfiscsi_pdu_handle_data_out(struct icl_pdu *request)
{
	struct iscsi_bhs_data_out *bhsdo;
	struct cfiscsi_session *cs;
	struct cfiscsi_data_wait *cdw = NULL;
	union ctl_io *io;
	bool done;

	cs = PDU_SESSION(request);
	bhsdo = (struct iscsi_bhs_data_out *)request->ip_bhs;

	CFISCSI_SESSION_LOCK(cs);
	TAILQ_FOREACH(cdw, &cs->cs_waiting_for_data_out, cdw_next) {
#if 0
		CFISCSI_SESSION_DEBUG(cs, "have ttt 0x%x, itt 0x%x; looking for "
		    "ttt 0x%x, itt 0x%x",
		    bhsdo->bhsdo_target_transfer_tag,
		    bhsdo->bhsdo_initiator_task_tag,
		    cdw->cdw_target_transfer_tag, cdw->cdw_initiator_task_tag));
#endif
		if (bhsdo->bhsdo_target_transfer_tag ==
		    cdw->cdw_target_transfer_tag)
			break;
	}
	CFISCSI_SESSION_UNLOCK(cs);
	if (cdw == NULL) {
		CFISCSI_SESSION_WARN(cs, "data transfer tag 0x%x, initiator task tag "
		    "0x%x, not found; dropping connection",
		    bhsdo->bhsdo_target_transfer_tag, bhsdo->bhsdo_initiator_task_tag);
		icl_pdu_free(request);
		cfiscsi_session_terminate(cs);
		return;
	}

	io = cdw->cdw_ctl_io;
	KASSERT((io->io_hdr.flags & CTL_FLAG_DATA_MASK) != CTL_FLAG_DATA_IN,
	    ("CTL_FLAG_DATA_IN"));

	done = cfiscsi_handle_data_segment(request, cdw);
	if (done) {
		CFISCSI_SESSION_LOCK(cs);
		TAILQ_REMOVE(&cs->cs_waiting_for_data_out, cdw, cdw_next);
		CFISCSI_SESSION_UNLOCK(cs);
		uma_zfree(cfiscsi_data_wait_zone, cdw);
		io->scsiio.be_move_done(io);
	}

	icl_pdu_free(request);
}

static void
cfiscsi_pdu_handle_logout_request(struct icl_pdu *request)
{
	struct iscsi_bhs_logout_request *bhslr;
	struct iscsi_bhs_logout_response *bhslr2;
	struct icl_pdu *response;
	struct cfiscsi_session *cs;

	cs = PDU_SESSION(request);
	bhslr = (struct iscsi_bhs_logout_request *)request->ip_bhs;
	switch (bhslr->bhslr_reason & 0x7f) {
	case BHSLR_REASON_CLOSE_SESSION:
	case BHSLR_REASON_CLOSE_CONNECTION:
		response = cfiscsi_pdu_new_response(request, M_NOWAIT);
		if (response == NULL) {
			CFISCSI_SESSION_DEBUG(cs, "failed to allocate memory");
			icl_pdu_free(request);
			cfiscsi_session_terminate(cs);
			return;
		}
		bhslr2 = (struct iscsi_bhs_logout_response *)response->ip_bhs;
		bhslr2->bhslr_opcode = ISCSI_BHS_OPCODE_LOGOUT_RESPONSE;
		bhslr2->bhslr_flags = 0x80;
		bhslr2->bhslr_response = BHSLR_RESPONSE_CLOSED_SUCCESSFULLY;
		bhslr2->bhslr_initiator_task_tag =
		    bhslr->bhslr_initiator_task_tag;
		icl_pdu_free(request);
		cfiscsi_pdu_queue(response);
		cfiscsi_session_terminate(cs);
		break;
	case BHSLR_REASON_REMOVE_FOR_RECOVERY:
		response = cfiscsi_pdu_new_response(request, M_NOWAIT);
		if (response == NULL) {
			CFISCSI_SESSION_WARN(cs,
			    "failed to allocate memory; dropping connection");
			icl_pdu_free(request);
			cfiscsi_session_terminate(cs);
			return;
		}
		bhslr2 = (struct iscsi_bhs_logout_response *)response->ip_bhs;
		bhslr2->bhslr_opcode = ISCSI_BHS_OPCODE_LOGOUT_RESPONSE;
		bhslr2->bhslr_flags = 0x80;
		bhslr2->bhslr_response = BHSLR_RESPONSE_RECOVERY_NOT_SUPPORTED;
		bhslr2->bhslr_initiator_task_tag =
		    bhslr->bhslr_initiator_task_tag;
		icl_pdu_free(request);
		cfiscsi_pdu_queue(response);
		break;
	default:
		CFISCSI_SESSION_WARN(cs, "invalid reason 0%x; dropping connection",
		    bhslr->bhslr_reason);
		icl_pdu_free(request);
		cfiscsi_session_terminate(cs);
		break;
	}
}

static void
cfiscsi_callout(void *context)
{
	struct icl_pdu *cp;
	struct iscsi_bhs_nop_in *bhsni;
	struct cfiscsi_session *cs;

	cs = context;

	if (cs->cs_terminating) 
		return;

	callout_schedule(&cs->cs_callout, 1 * hz);

	atomic_add_int(&cs->cs_timeout, 1);

#ifdef ICL_KERNEL_PROXY
	if (cs->cs_waiting_for_ctld || cs->cs_login_phase) {
		if (cs->cs_timeout > login_timeout) {
			CFISCSI_SESSION_WARN(cs, "login timed out after "
			    "%d seconds; dropping connection", cs->cs_timeout);
			cfiscsi_session_terminate(cs);
		}
		return;
	}
#endif

	if (cs->cs_timeout >= ping_timeout) {
		CFISCSI_SESSION_WARN(cs, "no ping reply (NOP-Out) after %d seconds; "
		    "dropping connection",  ping_timeout);
		cfiscsi_session_terminate(cs);
		return;
	}

	/*
	 * If the ping was reset less than one second ago - which means
	 * that we've received some PDU during the last second - assume
	 * the traffic flows correctly and don't bother sending a NOP-Out.
	 *
	 * (It's 2 - one for one second, and one for incrementing is_timeout
	 * earlier in this routine.)
	 */
	if (cs->cs_timeout < 2)
		return;

	cp = icl_pdu_new_bhs(cs->cs_conn, M_NOWAIT);
	if (cp == NULL) {
		CFISCSI_SESSION_WARN(cs, "failed to allocate memory");
		return;
	}
	bhsni = (struct iscsi_bhs_nop_in *)cp->ip_bhs;
	bhsni->bhsni_opcode = ISCSI_BHS_OPCODE_NOP_IN;
	bhsni->bhsni_flags = 0x80;
	bhsni->bhsni_initiator_task_tag = 0xffffffff;

	cfiscsi_pdu_queue(cp);
}

static void
cfiscsi_session_terminate_tasks(struct cfiscsi_session *cs)
{
	struct cfiscsi_data_wait *cdw, *tmpcdw;
	union ctl_io *io;
	int error;

#ifdef notyet
	io = ctl_alloc_io(cs->cs_target->ct_softc->fe.ctl_pool_ref);
	if (io == NULL) {
		CFISCSI_SESSION_WARN(cs, "can't allocate ctl_io");
		return;
	}
	ctl_zero_io(io);
	io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr = NULL;
	io->io_hdr.io_type = CTL_IO_TASK;
	io->io_hdr.nexus.initid.id = cs->cs_ctl_initid;
	io->io_hdr.nexus.targ_port = cs->cs_target->ct_softc->fe.targ_port;
	io->io_hdr.nexus.targ_target.id = 0;
	io->io_hdr.nexus.targ_lun = lun;
	io->taskio.tag_type = CTL_TAG_SIMPLE; /* XXX */
	io->taskio.task_action = CTL_TASK_ABORT_TASK_SET;
	error = ctl_queue(io);
	if (error != CTL_RETVAL_COMPLETE) {
		CFISCSI_SESSION_WARN(cs, "ctl_queue() failed; error %d", error);
		ctl_free_io(io);
	}
#else
	/*
	 * CTL doesn't currently support CTL_TASK_ABORT_TASK_SET, so instead
	 * just iterate over tasks that are waiting for something - data - and
	 * terminate those.
	 */
	CFISCSI_SESSION_LOCK(cs);
	TAILQ_FOREACH_SAFE(cdw,
	    &cs->cs_waiting_for_data_out, cdw_next, tmpcdw) {
		io = ctl_alloc_io(cs->cs_target->ct_softc->fe.ctl_pool_ref);
		if (io == NULL) {
			CFISCSI_SESSION_WARN(cs, "can't allocate ctl_io");
			return;
		}
		ctl_zero_io(io);
		io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr = NULL;
		io->io_hdr.io_type = CTL_IO_TASK;
		io->io_hdr.nexus.initid.id = cs->cs_ctl_initid;
		io->io_hdr.nexus.targ_port =
		    cs->cs_target->ct_softc->fe.targ_port;
		io->io_hdr.nexus.targ_target.id = 0;
		//io->io_hdr.nexus.targ_lun = lun; /* Not needed? */
		io->taskio.tag_type = CTL_TAG_SIMPLE; /* XXX */
		io->taskio.task_action = CTL_TASK_ABORT_TASK;
		io->taskio.tag_num = cdw->cdw_initiator_task_tag;
		error = ctl_queue(io);
		if (error != CTL_RETVAL_COMPLETE) {
			CFISCSI_SESSION_WARN(cs, "ctl_queue() failed; error %d", error);
			ctl_free_io(io);
			return;
		}
#if 0
		CFISCSI_SESSION_DEBUG(cs, "removing csw for initiator task tag "
		    "0x%x", cdw->cdw_initiator_task_tag);
#endif
		cdw->cdw_ctl_io->scsiio.be_move_done(cdw->cdw_ctl_io);
		TAILQ_REMOVE(&cs->cs_waiting_for_data_out, cdw, cdw_next);
		uma_zfree(cfiscsi_data_wait_zone, cdw);
	}
	CFISCSI_SESSION_UNLOCK(cs);
#endif
}

static void
cfiscsi_maintenance_thread(void *arg)
{
	struct cfiscsi_session *cs;

	cs = arg;

	for (;;) {
		CFISCSI_SESSION_LOCK(cs);
		if (cs->cs_terminating == false)
			cv_wait(&cs->cs_maintenance_cv, &cs->cs_lock);
		CFISCSI_SESSION_UNLOCK(cs);

		if (cs->cs_terminating) {
			cfiscsi_session_terminate_tasks(cs);
			callout_drain(&cs->cs_callout);

			icl_conn_shutdown(cs->cs_conn);
			icl_conn_close(cs->cs_conn);

			cs->cs_terminating++;

			/*
			 * XXX: We used to wait up to 30 seconds to deliver queued PDUs
			 * 	to the initiator.  We also tried hard to deliver SCSI Responses
			 * 	for the aborted PDUs.  We don't do that anymore.  We might need
			 * 	to revisit that.
			 */

			cfiscsi_session_delete(cs);
			kthread_exit();
			return;
		}
		CFISCSI_SESSION_DEBUG(cs, "nothing to do");
	}
}

static void
cfiscsi_session_terminate(struct cfiscsi_session *cs)
{

	if (cs->cs_terminating != 0)
		return;
	cs->cs_terminating = 1;
	cv_signal(&cs->cs_maintenance_cv);
#ifdef ICL_KERNEL_PROXY
	cv_signal(&cs->cs_login_cv);
#endif
}

static int
cfiscsi_session_register_initiator(struct cfiscsi_session *cs)
{
	int error, i;
	struct cfiscsi_softc *softc;

	KASSERT(cs->cs_ctl_initid == -1, ("already registered"));

	softc = &cfiscsi_softc;

	mtx_lock(&softc->lock);
	for (i = 0; i < softc->max_initiators; i++) {
		if (softc->ctl_initids[i] == 0)
			break;
	}
	if (i == softc->max_initiators) {
		CFISCSI_SESSION_WARN(cs, "too many concurrent sessions (%d)",
		    softc->max_initiators);
		mtx_unlock(&softc->lock);
		return (1);
	}
	softc->ctl_initids[i] = 1;
	mtx_unlock(&softc->lock);

#if 0
	CFISCSI_SESSION_DEBUG(cs, "adding initiator id %d, max %d",
	    i, softc->max_initiators);
#endif
	cs->cs_ctl_initid = i;
	error = ctl_add_initiator(0x0, softc->fe.targ_port, cs->cs_ctl_initid);
	if (error != 0) {
		CFISCSI_SESSION_WARN(cs, "ctl_add_initiator failed with error %d", error);
		mtx_lock(&softc->lock);
		softc->ctl_initids[cs->cs_ctl_initid] = 0;
		mtx_unlock(&softc->lock);
		cs->cs_ctl_initid = -1;
		return (1);
	}

	return (0);
}

static void
cfiscsi_session_unregister_initiator(struct cfiscsi_session *cs)
{
	int error;
	struct cfiscsi_softc *softc;

	if (cs->cs_ctl_initid == -1)
		return;

	softc = &cfiscsi_softc;

	error = ctl_remove_initiator(softc->fe.targ_port, cs->cs_ctl_initid);
	if (error != 0) {
		CFISCSI_SESSION_WARN(cs, "ctl_remove_initiator failed with error %d",
		    error);
	}
	mtx_lock(&softc->lock);
	softc->ctl_initids[cs->cs_ctl_initid] = 0;
	mtx_unlock(&softc->lock);
	cs->cs_ctl_initid = -1;
}

static struct cfiscsi_session *
cfiscsi_session_new(struct cfiscsi_softc *softc)
{
	struct cfiscsi_session *cs;
	int error;

	cs = malloc(sizeof(*cs), M_CFISCSI, M_NOWAIT | M_ZERO);
	if (cs == NULL) {
		CFISCSI_WARN("malloc failed");
		return (NULL);
	}
	cs->cs_ctl_initid = -1;

	refcount_init(&cs->cs_outstanding_ctl_pdus, 0);
	TAILQ_INIT(&cs->cs_waiting_for_data_out);
	mtx_init(&cs->cs_lock, "cfiscsi_lock", NULL, MTX_DEF);
	cv_init(&cs->cs_maintenance_cv, "cfiscsi_mt");
#ifdef ICL_KERNEL_PROXY
	cv_init(&cs->cs_login_cv, "cfiscsi_login");
#endif

	cs->cs_conn = icl_conn_new("cfiscsi", &cs->cs_lock);
	cs->cs_conn->ic_receive = cfiscsi_receive_callback;
	cs->cs_conn->ic_error = cfiscsi_error_callback;
	cs->cs_conn->ic_prv0 = cs;

	error = kthread_add(cfiscsi_maintenance_thread, cs, NULL, NULL, 0, 0, "cfiscsimt");
	if (error != 0) {
		CFISCSI_SESSION_WARN(cs, "kthread_add(9) failed with error %d", error);
		free(cs, M_CFISCSI);
		return (NULL);
	}

	mtx_lock(&softc->lock);
	cs->cs_id = softc->last_session_id + 1;
	softc->last_session_id++;
	mtx_unlock(&softc->lock);

	mtx_lock(&softc->lock);
	TAILQ_INSERT_TAIL(&softc->sessions, cs, cs_next);
	mtx_unlock(&softc->lock);

	/*
	 * Start pinging the initiator.
	 */
	callout_init(&cs->cs_callout, 1);
	callout_reset(&cs->cs_callout, 1 * hz, cfiscsi_callout, cs);

	return (cs);
}

static void
cfiscsi_session_delete(struct cfiscsi_session *cs)
{
	struct cfiscsi_softc *softc;

	softc = &cfiscsi_softc;

	KASSERT(cs->cs_outstanding_ctl_pdus == 0,
	    ("destroying session with outstanding CTL pdus"));
	KASSERT(TAILQ_EMPTY(&cs->cs_waiting_for_data_out),
	    ("destroying session with non-empty queue"));

	cfiscsi_session_unregister_initiator(cs);
	if (cs->cs_target != NULL)
		cfiscsi_target_release(cs->cs_target);
	icl_conn_close(cs->cs_conn);
	icl_conn_free(cs->cs_conn);

	mtx_lock(&softc->lock);
	TAILQ_REMOVE(&softc->sessions, cs, cs_next);
	mtx_unlock(&softc->lock);

	free(cs, M_CFISCSI);
}

int
cfiscsi_init(void)
{
	struct cfiscsi_softc *softc;
	struct ctl_frontend *fe;
	int retval;

	softc = &cfiscsi_softc;
	retval = 0;
	bzero(softc, sizeof(*softc));
	mtx_init(&softc->lock, "cfiscsi", NULL, MTX_DEF);

#ifdef ICL_KERNEL_PROXY
	cv_init(&softc->accept_cv, "cfiscsi_accept");
#endif
	TAILQ_INIT(&softc->sessions);
	TAILQ_INIT(&softc->targets);

	fe = &softc->fe;
	fe->port_type = CTL_PORT_ISCSI;
	/* XXX KDM what should the real number be here? */
	fe->num_requested_ctl_io = 4096;
	snprintf(softc->port_name, sizeof(softc->port_name), "iscsi");
	fe->port_name = softc->port_name;
	fe->port_online = cfiscsi_online;
	fe->port_offline = cfiscsi_offline;
	fe->onoff_arg = softc;
	fe->targ_enable = cfiscsi_targ_enable;
	fe->targ_disable = cfiscsi_targ_disable;
	fe->lun_enable = cfiscsi_lun_enable;
	fe->lun_disable = cfiscsi_lun_disable;
	fe->targ_lun_arg = softc;
	fe->ioctl = cfiscsi_ioctl;
	fe->devid = cfiscsi_devid;
	fe->fe_datamove = cfiscsi_datamove;
	fe->fe_done = cfiscsi_done;

	/* XXX KDM what should we report here? */
	/* XXX These should probably be fetched from CTL. */
	fe->max_targets = 1;
	fe->max_target_id = 15;

	retval = ctl_frontend_register(fe, /*master_SC*/ 1);
	if (retval != 0) {
		CFISCSI_WARN("ctl_frontend_register() failed with error %d",
		    retval);
		retval = 1;
		goto bailout;
	}

	softc->max_initiators = fe->max_initiators;

	cfiscsi_data_wait_zone = uma_zcreate("cfiscsi_data_wait",
	    sizeof(struct cfiscsi_data_wait), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);

	return (0);

bailout:
	return (retval);
}

static int
cfiscsi_module_event_handler(module_t mod, int what, void *arg)
{

	switch (what) {
	case MOD_LOAD:
		return (cfiscsi_init());
	case MOD_UNLOAD:
		return (EBUSY);
	default:
		return (EOPNOTSUPP);
	}
}

#ifdef ICL_KERNEL_PROXY
static void
cfiscsi_accept(struct socket *so, struct sockaddr *sa, int portal_id)
{
	struct cfiscsi_session *cs;

	cs = cfiscsi_session_new(&cfiscsi_softc);
	if (cs == NULL) {
		CFISCSI_WARN("failed to create session");
		return;
	}

	icl_conn_handoff_sock(cs->cs_conn, so);
	cs->cs_initiator_sa = sa;
	cs->cs_portal_id = portal_id;
	cs->cs_waiting_for_ctld = true;
	cv_signal(&cfiscsi_softc.accept_cv);
}
#endif

static void
cfiscsi_online(void *arg)
{
	struct cfiscsi_softc *softc;

	softc = (struct cfiscsi_softc *)arg;

	softc->online = 1;
#ifdef ICL_KERNEL_PROXY
	if (softc->listener != NULL)
		icl_listen_free(softc->listener);
	softc->listener = icl_listen_new(cfiscsi_accept);
#endif
}

static void
cfiscsi_offline(void *arg)
{
	struct cfiscsi_softc *softc;
	struct cfiscsi_session *cs;

	softc = (struct cfiscsi_softc *)arg;

	softc->online = 0;

	mtx_lock(&softc->lock);
	TAILQ_FOREACH(cs, &softc->sessions, cs_next)
		cfiscsi_session_terminate(cs);
	mtx_unlock(&softc->lock);

#ifdef ICL_KERNEL_PROXY
	icl_listen_free(softc->listener);
	softc->listener = NULL;
#endif
}

static int
cfiscsi_targ_enable(void *arg, struct ctl_id targ_id)
{

	return (0);
}

static int
cfiscsi_targ_disable(void *arg, struct ctl_id targ_id)
{

	return (0);
}

static void
cfiscsi_ioctl_handoff(struct ctl_iscsi *ci)
{
	struct cfiscsi_softc *softc;
	struct cfiscsi_session *cs;
	struct cfiscsi_target *ct;
	struct ctl_iscsi_handoff_params *cihp;
	int error;

	cihp = (struct ctl_iscsi_handoff_params *)&(ci->data);
	softc = &cfiscsi_softc;

	CFISCSI_DEBUG("new connection from %s (%s) to %s",
	    cihp->initiator_name, cihp->initiator_addr,
	    cihp->target_name);

	if (softc->online == 0) {
		ci->status = CTL_ISCSI_ERROR;
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "%s: port offline", __func__);
		return;
	}

	ct = cfiscsi_target_find(softc, cihp->target_name);
	if (ct == NULL) {
		ci->status = CTL_ISCSI_ERROR;
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "%s: target not found", __func__);
		return;
	}

#ifdef ICL_KERNEL_PROXY
	if (cihp->socket > 0 && cihp->connection_id > 0) {
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "both socket and connection_id set");
		ci->status = CTL_ISCSI_ERROR;
		cfiscsi_target_release(ct);
		return;
	}
	if (cihp->socket == 0) {
		mtx_lock(&cfiscsi_softc.lock);
		TAILQ_FOREACH(cs, &cfiscsi_softc.sessions, cs_next) {
			if (cs->cs_id == cihp->socket)
				break;
		}
		if (cs == NULL) {
			mtx_unlock(&cfiscsi_softc.lock);
			snprintf(ci->error_str, sizeof(ci->error_str),
			    "connection not found");
			ci->status = CTL_ISCSI_ERROR;
			cfiscsi_target_release(ct);
			return;
		}
		mtx_unlock(&cfiscsi_softc.lock);
	} else {
#endif
		cs = cfiscsi_session_new(softc);
		if (cs == NULL) {
			ci->status = CTL_ISCSI_ERROR;
			snprintf(ci->error_str, sizeof(ci->error_str),
			    "%s: cfiscsi_session_new failed", __func__);
			cfiscsi_target_release(ct);
			return;
		}
#ifdef ICL_KERNEL_PROXY
	}
#endif
	cs->cs_target = ct;

	/*
	 * First PDU of Full Feature phase has the same CmdSN as the last
	 * PDU from the Login Phase received from the initiator.  Thus,
	 * the -1 below.
	 */
	cs->cs_portal_group_tag = cihp->portal_group_tag;
	cs->cs_cmdsn = cihp->cmdsn;
	cs->cs_statsn = cihp->statsn;
	cs->cs_max_data_segment_length = cihp->max_recv_data_segment_length;
	cs->cs_max_burst_length = cihp->max_burst_length;
	cs->cs_immediate_data = !!cihp->immediate_data;
	if (cihp->header_digest == CTL_ISCSI_DIGEST_CRC32C)
		cs->cs_conn->ic_header_crc32c = true;
	if (cihp->data_digest == CTL_ISCSI_DIGEST_CRC32C)
		cs->cs_conn->ic_data_crc32c = true;

	strlcpy(cs->cs_initiator_name,
	    cihp->initiator_name, sizeof(cs->cs_initiator_name));
	strlcpy(cs->cs_initiator_addr,
	    cihp->initiator_addr, sizeof(cs->cs_initiator_addr));
	strlcpy(cs->cs_initiator_alias,
	    cihp->initiator_alias, sizeof(cs->cs_initiator_alias));

#ifdef ICL_KERNEL_PROXY
	if (cihp->socket > 0) {
#endif
		error = icl_conn_handoff(cs->cs_conn, cihp->socket);
		if (error != 0) {
			cfiscsi_session_delete(cs);
			ci->status = CTL_ISCSI_ERROR;
			snprintf(ci->error_str, sizeof(ci->error_str),
			    "%s: icl_conn_handoff failed with error %d",
			    __func__, error);
			return;
		}
#ifdef ICL_KERNEL_PROXY
	}
#endif

	/*
	 * Register initiator with CTL.
	 */
	cfiscsi_session_register_initiator(cs);

#ifdef ICL_KERNEL_PROXY
	cs->cs_login_phase = false;

	/*
	 * First PDU of the Full Feature phase has likely already arrived.
	 * We have to pick it up and execute properly.
	 */
	if (cs->cs_login_pdu != NULL) {
		CFISCSI_SESSION_DEBUG(cs, "picking up first PDU");
		cfiscsi_pdu_handle(cs->cs_login_pdu);
		cs->cs_login_pdu = NULL;
	}
#endif

	ci->status = CTL_ISCSI_OK;
}

static void
cfiscsi_ioctl_list(struct ctl_iscsi *ci)
{
	struct ctl_iscsi_list_params *cilp;
	struct cfiscsi_session *cs;
	struct cfiscsi_softc *softc;
	struct sbuf *sb;
	int error;

	cilp = (struct ctl_iscsi_list_params *)&(ci->data);
	softc = &cfiscsi_softc;

	sb = sbuf_new(NULL, NULL, cilp->alloc_len, SBUF_FIXEDLEN);
	if (sb == NULL) {
		ci->status = CTL_ISCSI_ERROR;
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "Unable to allocate %d bytes for iSCSI session list",
		    cilp->alloc_len);
		return;
	}

	sbuf_printf(sb, "<ctlislist>\n");
	mtx_lock(&softc->lock);
	TAILQ_FOREACH(cs, &softc->sessions, cs_next) {
#ifdef ICL_KERNEL_PROXY
		if (cs->cs_target == NULL)
			continue;
#endif
		error = sbuf_printf(sb, "<connection id=\"%d\">"
		    "<initiator>%s</initiator>"
		    "<initiator_addr>%s</initiator_addr>"
		    "<initiator_alias>%s</initiator_alias>"
		    "<target>%s</target>"
		    "<target_alias>%s</target_alias>"
		    "<header_digest>%s</header_digest>"
		    "<data_digest>%s</data_digest>"
		    "<max_data_segment_length>%zd</max_data_segment_length>"
		    "<immediate_data>%d</immediate_data>"
		    "<iser>%d</iser>"
		    "</connection>\n",
		    cs->cs_id,
		    cs->cs_initiator_name, cs->cs_initiator_addr, cs->cs_initiator_alias,
		    cs->cs_target->ct_name, cs->cs_target->ct_alias,
		    cs->cs_conn->ic_header_crc32c ? "CRC32C" : "None",
		    cs->cs_conn->ic_data_crc32c ? "CRC32C" : "None",
		    cs->cs_max_data_segment_length,
		    cs->cs_immediate_data,
		    cs->cs_conn->ic_iser);
		if (error != 0)
			break;
	}
	mtx_unlock(&softc->lock);
	error = sbuf_printf(sb, "</ctlislist>\n");
	if (error != 0) {
		sbuf_delete(sb);
		ci->status = CTL_ISCSI_LIST_NEED_MORE_SPACE;
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "Out of space, %d bytes is too small", cilp->alloc_len);
		return;
	}
	sbuf_finish(sb);

	error = copyout(sbuf_data(sb), cilp->conn_xml, sbuf_len(sb) + 1);
	cilp->fill_len = sbuf_len(sb) + 1;
	ci->status = CTL_ISCSI_OK;
	sbuf_delete(sb);
}

static void
cfiscsi_ioctl_terminate(struct ctl_iscsi *ci)
{
	struct icl_pdu *response;
	struct iscsi_bhs_asynchronous_message *bhsam;
	struct ctl_iscsi_terminate_params *citp;
	struct cfiscsi_session *cs;
	struct cfiscsi_softc *softc;
	int found = 0;

	citp = (struct ctl_iscsi_terminate_params *)&(ci->data);
	softc = &cfiscsi_softc;

	mtx_lock(&softc->lock);
	TAILQ_FOREACH(cs, &softc->sessions, cs_next) {
		if (citp->all == 0 && cs->cs_id != citp->connection_id &&
		    strcmp(cs->cs_initiator_name, citp->initiator_name) != 0 &&
		    strcmp(cs->cs_initiator_addr, citp->initiator_addr) != 0)
			continue;

		response = icl_pdu_new_bhs(cs->cs_conn, M_NOWAIT);
		if (response == NULL) {
			/*
			 * Oh well.  Just terminate the connection.
			 */
		} else {
			bhsam = (struct iscsi_bhs_asynchronous_message *)
			    response->ip_bhs;
			bhsam->bhsam_opcode = ISCSI_BHS_OPCODE_ASYNC_MESSAGE;
			bhsam->bhsam_flags = 0x80;
			bhsam->bhsam_0xffffffff = 0xffffffff;
			bhsam->bhsam_async_event =
			    BHSAM_EVENT_TARGET_TERMINATES_SESSION;
			cfiscsi_pdu_queue(response);
		}
		cfiscsi_session_terminate(cs);
		found++;
	}
	mtx_unlock(&softc->lock);

	if (found == 0) {
		ci->status = CTL_ISCSI_SESSION_NOT_FOUND;
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "No matching connections found");
		return;
	}

	ci->status = CTL_ISCSI_OK;
}

static void
cfiscsi_ioctl_logout(struct ctl_iscsi *ci)
{
	struct icl_pdu *response;
	struct iscsi_bhs_asynchronous_message *bhsam;
	struct ctl_iscsi_logout_params *cilp;
	struct cfiscsi_session *cs;
	struct cfiscsi_softc *softc;
	int found = 0;

	cilp = (struct ctl_iscsi_logout_params *)&(ci->data);
	softc = &cfiscsi_softc;

	mtx_lock(&softc->lock);
	TAILQ_FOREACH(cs, &softc->sessions, cs_next) {
		if (cilp->all == 0 && cs->cs_id != cilp->connection_id &&
		    strcmp(cs->cs_initiator_name, cilp->initiator_name) != 0 &&
		    strcmp(cs->cs_initiator_addr, cilp->initiator_addr) != 0)
			continue;

		response = icl_pdu_new_bhs(cs->cs_conn, M_NOWAIT);
		if (response == NULL) {
			ci->status = CTL_ISCSI_ERROR;
			snprintf(ci->error_str, sizeof(ci->error_str),
			    "Unable to allocate memory");
			mtx_unlock(&softc->lock);
			return;
		}
		bhsam =
		    (struct iscsi_bhs_asynchronous_message *)response->ip_bhs;
		bhsam->bhsam_opcode = ISCSI_BHS_OPCODE_ASYNC_MESSAGE;
		bhsam->bhsam_flags = 0x80;
		bhsam->bhsam_async_event = BHSAM_EVENT_TARGET_REQUESTS_LOGOUT;
		bhsam->bhsam_parameter3 = htons(10);
		cfiscsi_pdu_queue(response);
		found++;
	}
	mtx_unlock(&softc->lock);

	if (found == 0) {
		ci->status = CTL_ISCSI_SESSION_NOT_FOUND;
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "No matching connections found");
		return;
	}

	ci->status = CTL_ISCSI_OK;
}

#ifdef ICL_KERNEL_PROXY
static void
cfiscsi_ioctl_listen(struct ctl_iscsi *ci)
{
	struct ctl_iscsi_listen_params *cilp;
	struct sockaddr *sa;
	int error;

	cilp = (struct ctl_iscsi_listen_params *)&(ci->data);

	if (cfiscsi_softc.listener == NULL) {
		CFISCSI_DEBUG("no listener");
		snprintf(ci->error_str, sizeof(ci->error_str), "no listener");
		ci->status = CTL_ISCSI_ERROR;
		return;
	}

	error = getsockaddr(&sa, (void *)cilp->addr, cilp->addrlen);
	if (error != 0) {
		CFISCSI_DEBUG("getsockaddr, error %d", error);
		snprintf(ci->error_str, sizeof(ci->error_str), "getsockaddr failed");
		ci->status = CTL_ISCSI_ERROR;
		return;
	}

	error = icl_listen_add(cfiscsi_softc.listener, cilp->iser, cilp->domain,
	    cilp->socktype, cilp->protocol, sa, cilp->portal_id);
	if (error != 0) {
		free(sa, M_SONAME);
		CFISCSI_DEBUG("icl_listen_add, error %d", error);
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "icl_listen_add failed, error %d", error);
		ci->status = CTL_ISCSI_ERROR;
		return;
	}

	ci->status = CTL_ISCSI_OK;
}

static void
cfiscsi_ioctl_accept(struct ctl_iscsi *ci)
{
	struct ctl_iscsi_accept_params *ciap;
	struct cfiscsi_session *cs;
	int error;

	ciap = (struct ctl_iscsi_accept_params *)&(ci->data);

	mtx_lock(&cfiscsi_softc.lock);
	for (;;) {
		TAILQ_FOREACH(cs, &cfiscsi_softc.sessions, cs_next) {
			if (cs->cs_waiting_for_ctld)
				break;
		}
		if (cs != NULL)
			break;
		error = cv_wait_sig(&cfiscsi_softc.accept_cv, &cfiscsi_softc.lock);
		if (error != 0) {
			mtx_unlock(&cfiscsi_softc.lock);
			snprintf(ci->error_str, sizeof(ci->error_str), "interrupted");
			ci->status = CTL_ISCSI_ERROR;
			return;
		}
	}
	mtx_unlock(&cfiscsi_softc.lock);

	cs->cs_waiting_for_ctld = false;
	cs->cs_login_phase = true;

	ciap->connection_id = cs->cs_id;
	ciap->portal_id = cs->cs_portal_id;
	ciap->initiator_addrlen = cs->cs_initiator_sa->sa_len;
	error = copyout(cs->cs_initiator_sa, ciap->initiator_addr,
	    cs->cs_initiator_sa->sa_len);
	if (error != 0) {
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "copyout failed with error %d", error);
		ci->status = CTL_ISCSI_ERROR;
		return;
	}

	ci->status = CTL_ISCSI_OK;
}

static void
cfiscsi_ioctl_send(struct ctl_iscsi *ci)
{
	struct ctl_iscsi_send_params *cisp;
	struct cfiscsi_session *cs;
	struct icl_pdu *ip;
	size_t datalen;
	void *data;
	int error;

	cisp = (struct ctl_iscsi_send_params *)&(ci->data);

	mtx_lock(&cfiscsi_softc.lock);
	TAILQ_FOREACH(cs, &cfiscsi_softc.sessions, cs_next) {
		if (cs->cs_id == cisp->connection_id)
			break;
	}
	if (cs == NULL) {
		mtx_unlock(&cfiscsi_softc.lock);
		snprintf(ci->error_str, sizeof(ci->error_str), "connection not found");
		ci->status = CTL_ISCSI_ERROR;
		return;
	}
	mtx_unlock(&cfiscsi_softc.lock);

#if 0
	if (cs->cs_login_phase == false)
		return (EBUSY);
#endif

	if (cs->cs_terminating) {
		snprintf(ci->error_str, sizeof(ci->error_str), "connection is terminating");
		ci->status = CTL_ISCSI_ERROR;
		return;
	}

	datalen = cisp->data_segment_len;
	/*
	 * XXX
	 */
	//if (datalen > CFISCSI_MAX_DATA_SEGMENT_LENGTH) {
	if (datalen > 65535) {
		snprintf(ci->error_str, sizeof(ci->error_str), "data segment too big");
		ci->status = CTL_ISCSI_ERROR;
		return;
	}
	if (datalen > 0) {
		data = malloc(datalen, M_CFISCSI, M_WAITOK);
		error = copyin(cisp->data_segment, data, datalen);
		if (error != 0) {
			free(data, M_CFISCSI);
			snprintf(ci->error_str, sizeof(ci->error_str), "copyin error %d", error);
			ci->status = CTL_ISCSI_ERROR;
			return;
		}
	}

	ip = icl_pdu_new_bhs(cs->cs_conn, M_WAITOK);
	memcpy(ip->ip_bhs, cisp->bhs, sizeof(*ip->ip_bhs));
	if (datalen > 0) {
		icl_pdu_append_data(ip, data, datalen, M_WAITOK);
		free(data, M_CFISCSI);
	}
	CFISCSI_SESSION_LOCK(cs);
	icl_pdu_queue(ip);
	CFISCSI_SESSION_UNLOCK(cs);
	ci->status = CTL_ISCSI_OK;
}

static void
cfiscsi_ioctl_receive(struct ctl_iscsi *ci)
{
	struct ctl_iscsi_receive_params *cirp;
	struct cfiscsi_session *cs;
	struct icl_pdu *ip;
	void *data;
	int error;

	cirp = (struct ctl_iscsi_receive_params *)&(ci->data);

	mtx_lock(&cfiscsi_softc.lock);
	TAILQ_FOREACH(cs, &cfiscsi_softc.sessions, cs_next) {
		if (cs->cs_id == cirp->connection_id)
			break;
	}
	if (cs == NULL) {
		mtx_unlock(&cfiscsi_softc.lock);
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "connection not found");
		ci->status = CTL_ISCSI_ERROR;
		return;
	}
	mtx_unlock(&cfiscsi_softc.lock);

#if 0
	if (is->is_login_phase == false)
		return (EBUSY);
#endif

	CFISCSI_SESSION_LOCK(cs);
	while (cs->cs_login_pdu == NULL && cs->cs_terminating == false) {
		error = cv_wait_sig(&cs->cs_login_cv, &cs->cs_lock);
		if (error != 0) {
			CFISCSI_SESSION_UNLOCK(cs);
			snprintf(ci->error_str, sizeof(ci->error_str),
			    "interrupted by signal");
			ci->status = CTL_ISCSI_ERROR;
			return;
		}
	}

	if (cs->cs_terminating) {
		CFISCSI_SESSION_UNLOCK(cs);
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "connection terminating");
		ci->status = CTL_ISCSI_ERROR;
		return;
	}
	ip = cs->cs_login_pdu;
	cs->cs_login_pdu = NULL;
	CFISCSI_SESSION_UNLOCK(cs);

	if (ip->ip_data_len > cirp->data_segment_len) {
		icl_pdu_free(ip);
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "data segment too big");
		ci->status = CTL_ISCSI_ERROR;
		return;
	}

	copyout(ip->ip_bhs, cirp->bhs, sizeof(*ip->ip_bhs));
	if (ip->ip_data_len > 0) {
		data = malloc(ip->ip_data_len, M_CFISCSI, M_WAITOK);
		icl_pdu_get_data(ip, 0, data, ip->ip_data_len);
		copyout(data, cirp->data_segment, ip->ip_data_len);
		free(data, M_CFISCSI);
	}

	icl_pdu_free(ip);
	ci->status = CTL_ISCSI_OK;
}

#endif /* !ICL_KERNEL_PROXY */

static int
cfiscsi_ioctl(struct cdev *dev,
    u_long cmd, caddr_t addr, int flag, struct thread *td)
{
	struct ctl_iscsi *ci;

	if (cmd != CTL_ISCSI)
		return (ENOTTY);

	ci = (struct ctl_iscsi *)addr;
	switch (ci->type) {
	case CTL_ISCSI_HANDOFF:
		cfiscsi_ioctl_handoff(ci);
		break;
	case CTL_ISCSI_LIST:
		cfiscsi_ioctl_list(ci);
		break;
	case CTL_ISCSI_TERMINATE:
		cfiscsi_ioctl_terminate(ci);
		break;
	case CTL_ISCSI_LOGOUT:
		cfiscsi_ioctl_logout(ci);
		break;
#ifdef ICL_KERNEL_PROXY
	case CTL_ISCSI_LISTEN:
		cfiscsi_ioctl_listen(ci);
		break;
	case CTL_ISCSI_ACCEPT:
		cfiscsi_ioctl_accept(ci);
		break;
	case CTL_ISCSI_SEND:
		cfiscsi_ioctl_send(ci);
		break;
	case CTL_ISCSI_RECEIVE:
		cfiscsi_ioctl_receive(ci);
		break;
#else
	case CTL_ISCSI_LISTEN:
	case CTL_ISCSI_ACCEPT:
	case CTL_ISCSI_SEND:
	case CTL_ISCSI_RECEIVE:
		ci->status = CTL_ISCSI_ERROR;
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "%s: CTL compiled without ICL_KERNEL_PROXY",
		    __func__);
		break;
#endif /* !ICL_KERNEL_PROXY */
	default:
		ci->status = CTL_ISCSI_ERROR;
		snprintf(ci->error_str, sizeof(ci->error_str),
		    "%s: invalid iSCSI request type %d", __func__, ci->type);
		break;
	}

	return (0);
}

static int
cfiscsi_devid(struct ctl_scsiio *ctsio, int alloc_len)
{
	struct cfiscsi_session *cs;
	struct scsi_vpd_device_id *devid_ptr;
	struct scsi_vpd_id_descriptor *desc, *desc1, *desc2, *desc3, *desc4;
	struct scsi_vpd_id_t10 *t10id;
	struct ctl_lun *lun;
	const struct icl_pdu *request;
	int i, ret;
	char *val;
	size_t devid_len, wwpn_len, lun_name_len;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	request = ctsio->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;
	cs = PDU_SESSION(request);

	wwpn_len = strlen(cs->cs_target->ct_name);
	wwpn_len += strlen(",t,0x0001");
	wwpn_len += 1; /* '\0' */
	if ((wwpn_len % 4) != 0)
		wwpn_len += (4 - (wwpn_len % 4));

	if (lun == NULL) {
		lun_name_len = 0;
	} else {
		lun_name_len = strlen(cs->cs_target->ct_name);
		lun_name_len += strlen(",lun,XXXXXXXX");
		lun_name_len += 1; /* '\0' */
		if ((lun_name_len % 4) != 0)
			lun_name_len += (4 - (lun_name_len % 4));
	}

	devid_len = sizeof(struct scsi_vpd_device_id) +
		sizeof(struct scsi_vpd_id_descriptor) +
		sizeof(struct scsi_vpd_id_t10) + CTL_DEVID_LEN +
		sizeof(struct scsi_vpd_id_descriptor) + lun_name_len +
		sizeof(struct scsi_vpd_id_descriptor) + wwpn_len +
		sizeof(struct scsi_vpd_id_descriptor) +
		sizeof(struct scsi_vpd_id_rel_trgt_port_id) +
		sizeof(struct scsi_vpd_id_descriptor) +
		sizeof(struct scsi_vpd_id_trgt_port_grp_id);

	ctsio->kern_data_ptr = malloc(devid_len, M_CTL, M_WAITOK | M_ZERO);
	devid_ptr = (struct scsi_vpd_device_id *)ctsio->kern_data_ptr;
	ctsio->kern_sg_entries = 0;

	if (devid_len < alloc_len) {
		ctsio->residual = alloc_len - devid_len;
		ctsio->kern_data_len = devid_len;
		ctsio->kern_total_len = devid_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	desc = (struct scsi_vpd_id_descriptor *)devid_ptr->desc_list;
	t10id = (struct scsi_vpd_id_t10 *)&desc->identifier[0];
	desc1 = (struct scsi_vpd_id_descriptor *)(&desc->identifier[0] +
	    sizeof(struct scsi_vpd_id_t10) + CTL_DEVID_LEN);
	desc2 = (struct scsi_vpd_id_descriptor *)(&desc1->identifier[0] +
	    lun_name_len);
	desc3 = (struct scsi_vpd_id_descriptor *)(&desc2->identifier[0] +
	    wwpn_len);
	desc4 = (struct scsi_vpd_id_descriptor *)(&desc3->identifier[0] +
	    sizeof(struct scsi_vpd_id_rel_trgt_port_id));

	if (lun != NULL)
		devid_ptr->device = (SID_QUAL_LU_CONNECTED << 5) |
		    lun->be_lun->lun_type;
	else
		devid_ptr->device = (SID_QUAL_LU_OFFLINE << 5) | T_DIRECT;

	devid_ptr->page_code = SVPD_DEVICE_ID;

	scsi_ulto2b(devid_len - 4, devid_ptr->length);

	/*
	 * We're using a LUN association here.  i.e., this device ID is a
	 * per-LUN identifier.
	 */
	desc->proto_codeset = (SCSI_PROTO_ISCSI << 4) | SVPD_ID_CODESET_ASCII;
	desc->id_type = SVPD_ID_PIV | SVPD_ID_ASSOC_LUN | SVPD_ID_TYPE_T10;
	desc->length = sizeof(*t10id) + CTL_DEVID_LEN;
	if (lun == NULL || (val = ctl_get_opt(lun->be_lun, "vendor")) == NULL) {
		strncpy((char *)t10id->vendor, CTL_VENDOR, sizeof(t10id->vendor));
	} else {
		memset(t10id->vendor, ' ', sizeof(t10id->vendor));
		strncpy(t10id->vendor, val,
		    min(sizeof(t10id->vendor), strlen(val)));
	}

	/*
	 * If we've actually got a backend, copy the device id from the
	 * per-LUN data.  Otherwise, set it to all spaces.
	 */
	if (lun != NULL) {
		/*
		 * Copy the backend's LUN ID.
		 */
		strncpy((char *)t10id->vendor_spec_id,
		    (char *)lun->be_lun->device_id, CTL_DEVID_LEN);
	} else {
		/*
		 * No backend, set this to spaces.
		 */
		memset(t10id->vendor_spec_id, 0x20, CTL_DEVID_LEN);
	}

	/*
	 * desc1 is for the unique LUN name.
	 *
	 * XXX: According to SPC-3, LUN must report the same ID through
	 *      all the ports.  The code below, however, reports the
	 *      ID only via iSCSI.
	 */
	desc1->proto_codeset = (SCSI_PROTO_ISCSI << 4) | SVPD_ID_CODESET_UTF8;
	desc1->id_type = SVPD_ID_PIV | SVPD_ID_ASSOC_LUN |
		SVPD_ID_TYPE_SCSI_NAME;
	desc1->length = lun_name_len;
	if (lun != NULL) {
		/*
		 * Find the per-target LUN number.
		 */
		for (i = 0; i < CTL_MAX_LUNS; i++) {
			if (cs->cs_target->ct_luns[i] == lun->lun)
				break;
		}
		KASSERT(i < CTL_MAX_LUNS,
		    ("lun %jd not found", (uintmax_t)lun->lun));
		ret = snprintf(desc1->identifier, lun_name_len, "%s,lun,%d",
		    cs->cs_target->ct_name, i);
		KASSERT(ret > 0 && ret <= lun_name_len, ("bad snprintf"));
	} else {
		KASSERT(lun_name_len == 0, ("no lun, but lun_name_len != 0"));
	}

	/*
	 * desc2 is for the WWPN which is a port asscociation.
	 */
       	desc2->proto_codeset = (SCSI_PROTO_ISCSI << 4) | SVPD_ID_CODESET_UTF8;
	desc2->id_type = SVPD_ID_PIV | SVPD_ID_ASSOC_PORT |
	    SVPD_ID_TYPE_SCSI_NAME;
	desc2->length = wwpn_len;
	snprintf(desc2->identifier, wwpn_len, "%s,t,0x%4.4x",
	    cs->cs_target->ct_name, cs->cs_portal_group_tag);

	/*
	 * desc3 is for the Relative Target Port(type 4h) identifier
	 */
       	desc3->proto_codeset = (SCSI_PROTO_ISCSI << 4) | SVPD_ID_CODESET_BINARY;
	desc3->id_type = SVPD_ID_PIV | SVPD_ID_ASSOC_PORT |
	    SVPD_ID_TYPE_RELTARG;
	desc3->length = 4;
	desc3->identifier[3] = 1;

	/*
	 * desc4 is for the Target Port Group(type 5h) identifier
	 */
       	desc4->proto_codeset = (SCSI_PROTO_ISCSI << 4) | SVPD_ID_CODESET_BINARY;
	desc4->id_type = SVPD_ID_PIV | SVPD_ID_ASSOC_PORT |
	    SVPD_ID_TYPE_TPORTGRP;
	desc4->length = 4;
	desc4->identifier[3] = 1;

	ctsio->scsi_status = SCSI_STATUS_OK;

	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);

	return (CTL_RETVAL_COMPLETE);
}

static void
cfiscsi_target_hold(struct cfiscsi_target *ct)
{

	refcount_acquire(&ct->ct_refcount);
}

static void
cfiscsi_target_release(struct cfiscsi_target *ct)
{
	struct cfiscsi_softc *softc;

	softc = ct->ct_softc;
	mtx_lock(&softc->lock);
	if (refcount_release(&ct->ct_refcount)) {
		TAILQ_REMOVE(&softc->targets, ct, ct_next);
		mtx_unlock(&softc->lock);
		free(ct, M_CFISCSI);

		return;
	}
	mtx_unlock(&softc->lock);
}

static struct cfiscsi_target *
cfiscsi_target_find(struct cfiscsi_softc *softc, const char *name)
{
	struct cfiscsi_target *ct;

	mtx_lock(&softc->lock);
	TAILQ_FOREACH(ct, &softc->targets, ct_next) {
		if (strcmp(name, ct->ct_name) != 0)
			continue;
		cfiscsi_target_hold(ct);
		mtx_unlock(&softc->lock);
		return (ct);
	}
	mtx_unlock(&softc->lock);

	return (NULL);
}

static struct cfiscsi_target *
cfiscsi_target_find_or_create(struct cfiscsi_softc *softc, const char *name,
    const char *alias)
{
	struct cfiscsi_target *ct, *newct;
	int i;

	if (name[0] == '\0' || strlen(name) >= CTL_ISCSI_NAME_LEN)
		return (NULL);

	newct = malloc(sizeof(*newct), M_CFISCSI, M_WAITOK | M_ZERO);

	mtx_lock(&softc->lock);
	TAILQ_FOREACH(ct, &softc->targets, ct_next) {
		if (strcmp(name, ct->ct_name) != 0)
			continue;
		cfiscsi_target_hold(ct);
		mtx_unlock(&softc->lock);
		free(newct, M_CFISCSI);
		return (ct);
	}

	for (i = 0; i < CTL_MAX_LUNS; i++)
		newct->ct_luns[i] = -1;

	strlcpy(newct->ct_name, name, sizeof(newct->ct_name));
	if (alias != NULL)
		strlcpy(newct->ct_alias, alias, sizeof(newct->ct_alias));
	refcount_init(&newct->ct_refcount, 1);
	newct->ct_softc = softc;
	TAILQ_INSERT_TAIL(&softc->targets, newct, ct_next);
	mtx_unlock(&softc->lock);

	return (newct);
}

/*
 * Takes LUN from the target space and returns LUN from the CTL space.
 */
static uint32_t
cfiscsi_map_lun(void *arg, uint32_t lun)
{
	struct cfiscsi_session *cs;

	cs = arg;

	if (lun >= CTL_MAX_LUNS) {
		CFISCSI_DEBUG("requested lun number %d is higher "
		    "than maximum %d", lun, CTL_MAX_LUNS - 1);
		return (0xffffffff);
	}

	if (cs->cs_target->ct_luns[lun] < 0)
		return (0xffffffff);

	return (cs->cs_target->ct_luns[lun]);
}

static int
cfiscsi_target_set_lun(struct cfiscsi_target *ct,
    unsigned long lun_id, unsigned long ctl_lun_id)
{

	if (lun_id >= CTL_MAX_LUNS) {
		CFISCSI_WARN("requested lun number %ld is higher "
		    "than maximum %d", lun_id, CTL_MAX_LUNS - 1);
		return (-1);
	}

	if (ct->ct_luns[lun_id] >= 0) {
		/*
		 * CTL calls cfiscsi_lun_enable() twice for each LUN - once
		 * when the LUN is created, and a second time just before
		 * the port is brought online; don't emit warnings
		 * for that case.
		 */
		if (ct->ct_luns[lun_id] == ctl_lun_id)
			return (0);
		CFISCSI_WARN("lun %ld already allocated", lun_id);
		return (-1);
	}

#if 0
	CFISCSI_DEBUG("adding mapping for lun %ld, target %s "
	    "to ctl lun %ld", lun_id, ct->ct_name, ctl_lun_id);
#endif

	ct->ct_luns[lun_id] = ctl_lun_id;
	cfiscsi_target_hold(ct);

	return (0);
}

static int
cfiscsi_target_unset_lun(struct cfiscsi_target *ct, unsigned long lun_id)
{

	if (ct->ct_luns[lun_id] < 0) {
		CFISCSI_WARN("lun %ld not allocated", lun_id);
		return (-1);
	}

	ct->ct_luns[lun_id] = -1;
	cfiscsi_target_release(ct);

	return (0);
}

static int
cfiscsi_lun_enable(void *arg, struct ctl_id target_id, int lun_id)
{
	struct cfiscsi_softc *softc;
	struct cfiscsi_target *ct;
	const char *target = NULL, *target_alias = NULL;
	const char *lun = NULL;
	unsigned long tmp;

	softc = (struct cfiscsi_softc *)arg;

	target = ctl_get_opt(control_softc->ctl_luns[lun_id]->be_lun,
	    "cfiscsi_target");
	target_alias = ctl_get_opt(control_softc->ctl_luns[lun_id]->be_lun,
	    "cfiscsi_target)alias");
	lun = ctl_get_opt(control_softc->ctl_luns[lun_id]->be_lun,
	    "cfiscsi_lun");

	if (target == NULL && lun == NULL)
		return (0);

	if (target == NULL || lun == NULL) {
		CFISCSI_WARN("lun added with cfiscsi_target, but without "
		    "cfiscsi_lun, or the other way around; ignoring");
		return (0);
	}

	ct = cfiscsi_target_find_or_create(softc, target, target_alias);
	if (ct == NULL) {
		CFISCSI_WARN("failed to create target \"%s\"", target);
		return (0);
	}

	tmp = strtoul(lun, NULL, 10);
	cfiscsi_target_set_lun(ct, tmp, lun_id);
	cfiscsi_target_release(ct);
	return (0);
}

static int
cfiscsi_lun_disable(void *arg, struct ctl_id target_id, int lun_id)
{
	struct cfiscsi_softc *softc;
	struct cfiscsi_target *ct;
	int i;

	softc = (struct cfiscsi_softc *)arg;

	mtx_lock(&softc->lock);
	TAILQ_FOREACH(ct, &softc->targets, ct_next) {
		for (i = 0; i < CTL_MAX_LUNS; i++) {
			if (ct->ct_luns[i] < 0)
				continue;
			if (ct->ct_luns[i] != lun_id)
				continue;
			mtx_unlock(&softc->lock);
			cfiscsi_target_unset_lun(ct, i);
			return (0);
		}
	}
	mtx_unlock(&softc->lock);
	return (0);
}

static void
cfiscsi_datamove_in(union ctl_io *io)
{
	struct cfiscsi_session *cs;
	struct icl_pdu *request, *response;
	const struct iscsi_bhs_scsi_command *bhssc;
	struct iscsi_bhs_data_in *bhsdi;
	struct ctl_sg_entry ctl_sg_entry, *ctl_sglist;
	size_t len, expected_len, sg_len, buffer_offset;
	const char *sg_addr;
	int ctl_sg_count, error, i;

	request = io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;
	cs = PDU_SESSION(request);

	bhssc = (const struct iscsi_bhs_scsi_command *)request->ip_bhs;
	KASSERT((bhssc->bhssc_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) ==
	    ISCSI_BHS_OPCODE_SCSI_COMMAND,
	    ("bhssc->bhssc_opcode != ISCSI_BHS_OPCODE_SCSI_COMMAND"));

	if (io->scsiio.kern_sg_entries > 0) {
		ctl_sglist = (struct ctl_sg_entry *)io->scsiio.kern_data_ptr;
		ctl_sg_count = io->scsiio.kern_sg_entries;
	} else {
		ctl_sglist = &ctl_sg_entry;
		ctl_sglist->addr = io->scsiio.kern_data_ptr;
		ctl_sglist->len = io->scsiio.kern_data_len;
		ctl_sg_count = 1;
	}

	/*
	 * This is the total amount of data to be transferred within the current
	 * SCSI command.  We need to record it so that we can properly report
	 * underflow/underflow.
	 */
	PDU_TOTAL_TRANSFER_LEN(request) = io->scsiio.kern_total_len;

	/*
	 * This is the offset within the current SCSI command; for the first
	 * call to cfiscsi_datamove() it will be 0, and for subsequent ones
	 * it will be the sum of lengths of previous ones.
	 */
	buffer_offset = io->scsiio.kern_rel_offset;

	/*
	 * This is the transfer length expected by the initiator.  In theory,
	 * it could be different from the correct amount of data from the SCSI
	 * point of view, even if that doesn't make any sense.
	 */
	expected_len = ntohl(bhssc->bhssc_expected_data_transfer_length);
#if 0
	if (expected_len != io->scsiio.kern_total_len) {
		CFISCSI_SESSION_DEBUG(cs, "expected transfer length %zd, "
		    "actual length %zd", expected_len,
		    (size_t)io->scsiio.kern_total_len);
	}
#endif

	if (buffer_offset >= expected_len) {
#if 0
		CFISCSI_SESSION_DEBUG(cs, "buffer_offset = %zd, "
		    "already sent the expected len", buffer_offset);
#endif
		io->scsiio.be_move_done(io);
		return;
	}

	i = 0;
	sg_addr = NULL;
	sg_len = 0;
	response = NULL;
	bhsdi = NULL;
	for (;;) {
		if (response == NULL) {
			response = cfiscsi_pdu_new_response(request, M_NOWAIT);
			if (response == NULL) {
				CFISCSI_SESSION_WARN(cs, "failed to "
				    "allocate memory; dropping connection");
				ctl_set_busy(&io->scsiio);
				io->scsiio.be_move_done(io);
				cfiscsi_session_terminate(cs);
				return;
			}
			bhsdi = (struct iscsi_bhs_data_in *)response->ip_bhs;
			bhsdi->bhsdi_opcode = ISCSI_BHS_OPCODE_SCSI_DATA_IN;
			bhsdi->bhsdi_initiator_task_tag =
			    bhssc->bhssc_initiator_task_tag;
			bhsdi->bhsdi_datasn = htonl(PDU_EXPDATASN(request));
			PDU_EXPDATASN(request)++;
			bhsdi->bhsdi_buffer_offset = htonl(buffer_offset);
		}

		KASSERT(i < ctl_sg_count, ("i >= ctl_sg_count"));
		if (sg_len == 0) {
			sg_addr = ctl_sglist[i].addr;
			sg_len = ctl_sglist[i].len;
			KASSERT(sg_len > 0, ("sg_len <= 0"));
		}

		len = sg_len;

		/*
		 * Truncate to maximum data segment length.
		 */
		KASSERT(response->ip_data_len < cs->cs_max_data_segment_length,
		    ("ip_data_len %zd >= max_data_segment_length %zd",
		    response->ip_data_len, cs->cs_max_data_segment_length));
		if (response->ip_data_len + len >
		    cs->cs_max_data_segment_length) {
			len = cs->cs_max_data_segment_length -
			    response->ip_data_len;
			KASSERT(len <= sg_len, ("len %zd > sg_len %zd",
			    len, sg_len));
		}

		/*
		 * Truncate to expected data transfer length.
		 */
		KASSERT(buffer_offset + response->ip_data_len < expected_len,
		    ("buffer_offset %zd + ip_data_len %zd >= expected_len %zd",
		    buffer_offset, response->ip_data_len, expected_len));
		if (buffer_offset + response->ip_data_len + len > expected_len) {
			CFISCSI_SESSION_DEBUG(cs, "truncating from %zd "
			    "to expected data transfer length %zd",
			    buffer_offset + response->ip_data_len + len, expected_len);
			len = expected_len - (buffer_offset + response->ip_data_len);
			KASSERT(len <= sg_len, ("len %zd > sg_len %zd",
			    len, sg_len));
		}

		error = icl_pdu_append_data(response, sg_addr, len, M_NOWAIT);
		if (error != 0) {
			CFISCSI_SESSION_WARN(cs, "failed to "
			    "allocate memory; dropping connection");
			icl_pdu_free(response);
			ctl_set_busy(&io->scsiio);
			io->scsiio.be_move_done(io);
			cfiscsi_session_terminate(cs);
			return;
		}
		sg_addr += len;
		sg_len -= len;

		KASSERT(buffer_offset + request->ip_data_len <= expected_len,
		    ("buffer_offset %zd + ip_data_len %zd > expected_len %zd",
		    buffer_offset, request->ip_data_len, expected_len));
		if (buffer_offset + request->ip_data_len == expected_len) {
			/*
			 * Already have the amount of data the initiator wanted.
			 */
			break;
		}

		if (sg_len == 0) {
			/*
			 * End of scatter-gather segment;
			 * proceed to the next one...
			 */
			if (i == ctl_sg_count - 1) {
				/*
				 * ... unless this was the last one.
				 */
				break;
			}
			i++;
		}

		if (response->ip_data_len == cs->cs_max_data_segment_length) {
			/*
			 * Can't stuff more data into the current PDU;
			 * queue it.  Note that's not enough to check
			 * for kern_data_resid == 0 instead; there
			 * may be several Data-In PDUs for the final
			 * call to cfiscsi_datamove(), and we want
			 * to set the F flag only on the last of them.
			 */
			buffer_offset += response->ip_data_len;
			if (buffer_offset == io->scsiio.kern_total_len ||
			    buffer_offset == expected_len)
				bhsdi->bhsdi_flags |= BHSDI_FLAGS_F;
			cfiscsi_pdu_queue(response);
			response = NULL;
			bhsdi = NULL;
		}
	}
	if (response != NULL) {
		buffer_offset += response->ip_data_len;
		if (buffer_offset == io->scsiio.kern_total_len ||
		    buffer_offset == expected_len)
			bhsdi->bhsdi_flags |= BHSDI_FLAGS_F;
		KASSERT(response->ip_data_len > 0, ("sending empty Data-In"));
		cfiscsi_pdu_queue(response);
	}

	io->scsiio.be_move_done(io);
}

static void
cfiscsi_datamove_out(union ctl_io *io)
{
	struct cfiscsi_session *cs;
	struct icl_pdu *request, *response;
	const struct iscsi_bhs_scsi_command *bhssc;
	struct iscsi_bhs_r2t *bhsr2t;
	struct cfiscsi_data_wait *cdw;
	uint32_t target_transfer_tag;
	bool done;

	request = io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;
	cs = PDU_SESSION(request);

	bhssc = (const struct iscsi_bhs_scsi_command *)request->ip_bhs;
	KASSERT((bhssc->bhssc_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) ==
	    ISCSI_BHS_OPCODE_SCSI_COMMAND,
	    ("bhssc->bhssc_opcode != ISCSI_BHS_OPCODE_SCSI_COMMAND"));

	/*
	 * We need to record it so that we can properly report
	 * underflow/underflow.
	 */
	PDU_TOTAL_TRANSFER_LEN(request) = io->scsiio.kern_total_len;

	/*
	 * We hadn't received anything during this datamove yet.
	 */
	io->scsiio.ext_data_filled = 0;

	target_transfer_tag =
	    atomic_fetchadd_32(&cs->cs_target_transfer_tag, 1);

#if 0
	CFISCSI_SESSION_DEBUG(cs, "expecting Data-Out with initiator "
	    "task tag 0x%x, target transfer tag 0x%x",
	    bhssc->bhssc_initiator_task_tag, target_transfer_tag);
#endif
	cdw = uma_zalloc(cfiscsi_data_wait_zone, M_NOWAIT | M_ZERO);
	if (cdw == NULL) {
		CFISCSI_SESSION_WARN(cs, "failed to "
		    "allocate memory; dropping connection");
		ctl_set_busy(&io->scsiio);
		io->scsiio.be_move_done(io);
		cfiscsi_session_terminate(cs);
		return;
	}
	cdw->cdw_ctl_io = io;
	cdw->cdw_target_transfer_tag = target_transfer_tag;
	cdw->cdw_initiator_task_tag = bhssc->bhssc_initiator_task_tag;

	if (cs->cs_immediate_data && io->scsiio.kern_rel_offset == 0 &&
	    icl_pdu_data_segment_length(request) > 0) {
		done = cfiscsi_handle_data_segment(request, cdw);
		if (done) {
			uma_zfree(cfiscsi_data_wait_zone, cdw);
			io->scsiio.be_move_done(io);
			return;
		}
	}

	CFISCSI_SESSION_LOCK(cs);
	TAILQ_INSERT_TAIL(&cs->cs_waiting_for_data_out, cdw, cdw_next);
	CFISCSI_SESSION_UNLOCK(cs);

	/*
	 * XXX: We should limit the number of outstanding R2T PDUs
	 * 	per task to MaxOutstandingR2T.
	 */
	response = cfiscsi_pdu_new_response(request, M_NOWAIT);
	if (response == NULL) {
		CFISCSI_SESSION_WARN(cs, "failed to "
		    "allocate memory; dropping connection");
		ctl_set_busy(&io->scsiio);
		io->scsiio.be_move_done(io);
		cfiscsi_session_terminate(cs);
		return;
	}
	bhsr2t = (struct iscsi_bhs_r2t *)response->ip_bhs;
	bhsr2t->bhsr2t_opcode = ISCSI_BHS_OPCODE_R2T;
	bhsr2t->bhsr2t_flags = 0x80;
	bhsr2t->bhsr2t_lun = bhssc->bhssc_lun;
	bhsr2t->bhsr2t_initiator_task_tag = bhssc->bhssc_initiator_task_tag;
	bhsr2t->bhsr2t_target_transfer_tag = target_transfer_tag;
	/*
	 * XXX: Here we assume that cfiscsi_datamove() won't ever
	 *	be running concurrently on several CPUs for a given
	 *	command.
	 */
	bhsr2t->bhsr2t_r2tsn = htonl(PDU_R2TSN(request));
	PDU_R2TSN(request)++;
	/*
	 * This is the offset within the current SCSI command;
	 * i.e. for the first call of datamove(), it will be 0,
	 * and for subsequent ones it will be the sum of lengths
	 * of previous ones.
	 *
	 * The ext_data_filled is to account for unsolicited
	 * (immediate) data that might have already arrived.
	 */
	bhsr2t->bhsr2t_buffer_offset =
	    htonl(io->scsiio.kern_rel_offset + io->scsiio.ext_data_filled);
	/*
	 * This is the total length (sum of S/G lengths) this call
	 * to cfiscsi_datamove() is supposed to handle.
	 *
	 * XXX: Limit it to MaxBurstLength.
	 */
	bhsr2t->bhsr2t_desired_data_transfer_length =
	    htonl(io->scsiio.kern_data_len - io->scsiio.ext_data_filled);
	cfiscsi_pdu_queue(response);
}

static void
cfiscsi_datamove(union ctl_io *io)
{

	if ((io->io_hdr.flags & CTL_FLAG_DATA_MASK) == CTL_FLAG_DATA_IN)
		cfiscsi_datamove_in(io);
	else
		cfiscsi_datamove_out(io);
}

static void
cfiscsi_scsi_command_done(union ctl_io *io)
{
	struct icl_pdu *request, *response;
	struct iscsi_bhs_scsi_command *bhssc;
	struct iscsi_bhs_scsi_response *bhssr;
#ifdef DIAGNOSTIC
	struct cfiscsi_data_wait *cdw;
#endif
	struct cfiscsi_session *cs;
	uint16_t sense_length;

	request = io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;
	cs = PDU_SESSION(request);
	bhssc = (struct iscsi_bhs_scsi_command *)request->ip_bhs;
	KASSERT((bhssc->bhssc_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) ==
	    ISCSI_BHS_OPCODE_SCSI_COMMAND,
	    ("replying to wrong opcode 0x%x", bhssc->bhssc_opcode));

	//CFISCSI_SESSION_DEBUG(cs, "initiator task tag 0x%x",
	//    bhssc->bhssc_initiator_task_tag);

#ifdef DIAGNOSTIC
	CFISCSI_SESSION_LOCK(cs);
	TAILQ_FOREACH(cdw, &cs->cs_waiting_for_data_out, cdw_next)
		KASSERT(bhssc->bhssc_initiator_task_tag !=
		    cdw->cdw_initiator_task_tag, ("dangling cdw"));
	CFISCSI_SESSION_UNLOCK(cs);
#endif

	response = cfiscsi_pdu_new_response(request, M_WAITOK);
	bhssr = (struct iscsi_bhs_scsi_response *)response->ip_bhs;
	bhssr->bhssr_opcode = ISCSI_BHS_OPCODE_SCSI_RESPONSE;
	bhssr->bhssr_flags = 0x80;
	/*
	 * XXX: We don't deal with bidirectional under/overflows;
	 *	does anything actually support those?
	 */
	if (PDU_TOTAL_TRANSFER_LEN(request) <
	    ntohl(bhssc->bhssc_expected_data_transfer_length)) {
		bhssr->bhssr_flags |= BHSSR_FLAGS_RESIDUAL_UNDERFLOW;
		bhssr->bhssr_residual_count =
		    htonl(ntohl(bhssc->bhssc_expected_data_transfer_length) -
		    PDU_TOTAL_TRANSFER_LEN(request));
		//CFISCSI_SESSION_DEBUG(cs, "underflow; residual count %d",
		//    ntohl(bhssr->bhssr_residual_count));
	} else if (PDU_TOTAL_TRANSFER_LEN(request) > 
	    ntohl(bhssc->bhssc_expected_data_transfer_length)) {
		bhssr->bhssr_flags |= BHSSR_FLAGS_RESIDUAL_OVERFLOW;
		bhssr->bhssr_residual_count =
		    htonl(PDU_TOTAL_TRANSFER_LEN(request) -
		    ntohl(bhssc->bhssc_expected_data_transfer_length));
		//CFISCSI_SESSION_DEBUG(cs, "overflow; residual count %d",
		//    ntohl(bhssr->bhssr_residual_count));
	}
	bhssr->bhssr_response = BHSSR_RESPONSE_COMMAND_COMPLETED;
	bhssr->bhssr_status = io->scsiio.scsi_status;
	bhssr->bhssr_initiator_task_tag = bhssc->bhssc_initiator_task_tag;
	bhssr->bhssr_expdatasn = htonl(PDU_EXPDATASN(request));

	if (io->scsiio.sense_len > 0) {
#if 0
		CFISCSI_SESSION_DEBUG(cs, "returning %d bytes of sense data",
		    io->scsiio.sense_len);
#endif
		sense_length = htons(io->scsiio.sense_len);
		icl_pdu_append_data(response,
		    &sense_length, sizeof(sense_length), M_WAITOK);
		icl_pdu_append_data(response,
		    &io->scsiio.sense_data, io->scsiio.sense_len, M_WAITOK);
	}

	ctl_free_io(io);
	icl_pdu_free(request);
	cfiscsi_pdu_queue(response);
}

static void
cfiscsi_task_management_done(union ctl_io *io)
{
	struct icl_pdu *request, *response;
	struct iscsi_bhs_task_management_request *bhstmr;
	struct iscsi_bhs_task_management_response *bhstmr2;
	struct cfiscsi_data_wait *cdw, *tmpcdw;
	struct cfiscsi_session *cs;

	request = io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;
	cs = PDU_SESSION(request);
	bhstmr = (struct iscsi_bhs_task_management_request *)request->ip_bhs;
	KASSERT((bhstmr->bhstmr_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) ==
	    ISCSI_BHS_OPCODE_TASK_REQUEST,
	    ("replying to wrong opcode 0x%x", bhstmr->bhstmr_opcode));

#if 0
	CFISCSI_SESSION_DEBUG(cs, "initiator task tag 0x%x; referenced task tag 0x%x",
	    bhstmr->bhstmr_initiator_task_tag,
	    bhstmr->bhstmr_referenced_task_tag);
#endif

	if ((bhstmr->bhstmr_function & ~0x80) ==
	    BHSTMR_FUNCTION_ABORT_TASK) {
		/*
		 * Make sure we no longer wait for Data-Out for this command.
		 */
		CFISCSI_SESSION_LOCK(cs);
		TAILQ_FOREACH_SAFE(cdw,
		    &cs->cs_waiting_for_data_out, cdw_next, tmpcdw) {
			if (bhstmr->bhstmr_referenced_task_tag !=
			    cdw->cdw_initiator_task_tag)
				continue;

#if 0
			CFISCSI_SESSION_DEBUG(cs, "removing csw for initiator task "
			    "tag 0x%x", bhstmr->bhstmr_initiator_task_tag);
#endif
			TAILQ_REMOVE(&cs->cs_waiting_for_data_out,
			    cdw, cdw_next);
			cdw->cdw_ctl_io->scsiio.be_move_done(cdw->cdw_ctl_io);
			uma_zfree(cfiscsi_data_wait_zone, cdw);
		}
		CFISCSI_SESSION_UNLOCK(cs);
	}

	response = cfiscsi_pdu_new_response(request, M_WAITOK);
	bhstmr2 = (struct iscsi_bhs_task_management_response *)
	    response->ip_bhs;
	bhstmr2->bhstmr_opcode = ISCSI_BHS_OPCODE_TASK_RESPONSE;
	bhstmr2->bhstmr_flags = 0x80;
	if (io->io_hdr.status == CTL_SUCCESS) {
		bhstmr2->bhstmr_response = BHSTMR_RESPONSE_FUNCTION_COMPLETE;
	} else {
		/*
		 * XXX: How to figure out what exactly went wrong?  iSCSI spec
		 * 	expects us to provide detailed error, e.g. "Task does
		 * 	not exist" or "LUN does not exist".
		 */
		CFISCSI_SESSION_DEBUG(cs, "BHSTMR_RESPONSE_FUNCTION_NOT_SUPPORTED");
		bhstmr2->bhstmr_response =
		    BHSTMR_RESPONSE_FUNCTION_NOT_SUPPORTED;
	}
	bhstmr2->bhstmr_initiator_task_tag = bhstmr->bhstmr_initiator_task_tag;

	ctl_free_io(io);
	icl_pdu_free(request);
	cfiscsi_pdu_queue(response);
}

static void
cfiscsi_done(union ctl_io *io)
{
	struct icl_pdu *request;
	struct cfiscsi_session *cs;

	KASSERT(((io->io_hdr.status & CTL_STATUS_MASK) != CTL_STATUS_NONE),
		("invalid CTL status %#x", io->io_hdr.status));

	request = io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;
	if (request == NULL) {
		/*
		 * Implicit task termination has just completed; nothing to do.
		 */
		return;
	}

	cs = PDU_SESSION(request);
	refcount_release(&cs->cs_outstanding_ctl_pdus);

	switch (request->ip_bhs->bhs_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) {
	case ISCSI_BHS_OPCODE_SCSI_COMMAND:
		cfiscsi_scsi_command_done(io);
		break;
	case ISCSI_BHS_OPCODE_TASK_REQUEST:
		cfiscsi_task_management_done(io);
		break;
	default:
		panic("cfiscsi_done called with wrong opcode 0x%x",
		    request->ip_bhs->bhs_opcode);
	}
}
