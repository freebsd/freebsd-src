/*-
 * Generic routines for LSI '909 FC  adapters.
 * FreeBSD Version.
 *
 * Copyright (c) 2000, 2001 by Greg Ansley
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Additional Copyright (c) 2002 by Matthew Jacob under same license.
 */
/*
 * Copyright (c) 2004, Avid Technology, Inc. and its contributors.
 * Copyright (c) 2005, WHEEL Sp. z o.o.
 * Copyright (c) 2004, 2005 Justin T. Gibbs
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the names of the above listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/mpt/mpt.h>
#include <dev/mpt/mpt_cam.h> /* XXX For static handler registration */
#include <dev/mpt/mpt_raid.h> /* XXX For static handler registration */

#include <dev/mpt/mpilib/mpi.h>
#include <dev/mpt/mpilib/mpi_ioc.h>

#include <sys/sysctl.h>

#define MPT_MAX_TRYS 3
#define MPT_MAX_WAIT 300000

static int maxwait_ack = 0;
static int maxwait_int = 0;
static int maxwait_state = 0;

TAILQ_HEAD(, mpt_softc)	mpt_tailq = TAILQ_HEAD_INITIALIZER(mpt_tailq);
mpt_reply_handler_t *mpt_reply_handlers[MPT_NUM_REPLY_HANDLERS];

static mpt_reply_handler_t mpt_default_reply_handler;
static mpt_reply_handler_t mpt_config_reply_handler;
static mpt_reply_handler_t mpt_handshake_reply_handler;
static mpt_reply_handler_t mpt_event_reply_handler;
static void mpt_send_event_ack(struct mpt_softc *mpt, request_t *ack_req,
			       MSG_EVENT_NOTIFY_REPLY *msg, uint32_t context);
static int mpt_soft_reset(struct mpt_softc *mpt);
static void mpt_hard_reset(struct mpt_softc *mpt);
static int mpt_configure_ioc(struct mpt_softc *mpt);
static int mpt_enable_ioc(struct mpt_softc *mpt);

/************************* Personality Module Support *************************/
/*
 * We include one extra entry that is guaranteed to be NULL
 * to simplify our itterator.
 */
static struct mpt_personality *mpt_personalities[MPT_MAX_PERSONALITIES + 1];
static __inline struct mpt_personality*
	mpt_pers_find(struct mpt_softc *, u_int);
static __inline struct mpt_personality*
	mpt_pers_find_reverse(struct mpt_softc *, u_int);

static __inline struct mpt_personality *
mpt_pers_find(struct mpt_softc *mpt, u_int start_at)
{
	KASSERT(start_at <= MPT_MAX_PERSONALITIES,
		("mpt_pers_find: starting position out of range\n"));

	while (start_at < MPT_MAX_PERSONALITIES
	    && (mpt->mpt_pers_mask & (0x1 << start_at)) == 0) {
		start_at++;
	}
	return (mpt_personalities[start_at]);
}

/*
 * Used infrequenstly, so no need to optimize like a forward
 * traversal where we use the MAX+1 is guaranteed to be NULL
 * trick.
 */
static __inline struct mpt_personality *
mpt_pers_find_reverse(struct mpt_softc *mpt, u_int start_at)
{
	while (start_at < MPT_MAX_PERSONALITIES
	    && (mpt->mpt_pers_mask & (0x1 << start_at)) == 0) {
		start_at--;
	}
	if (start_at < MPT_MAX_PERSONALITIES)
		return (mpt_personalities[start_at]);
	return (NULL);
}

#define MPT_PERS_FOREACH(mpt, pers)				\
	for (pers = mpt_pers_find(mpt, /*start_at*/0);		\
	     pers != NULL;					\
	     pers = mpt_pers_find(mpt, /*start_at*/pers->id+1))

#define MPT_PERS_FOREACH_REVERSE(mpt, pers)				\
	for (pers = mpt_pers_find_reverse(mpt, MPT_MAX_PERSONALITIES-1);\
	     pers != NULL;						\
	     pers = mpt_pers_find_reverse(mpt, /*start_at*/pers->id-1))

static mpt_load_handler_t      mpt_stdload;
static mpt_probe_handler_t     mpt_stdprobe;
static mpt_attach_handler_t    mpt_stdattach;
static mpt_event_handler_t     mpt_stdevent;
static mpt_reset_handler_t     mpt_stdreset;
static mpt_shutdown_handler_t  mpt_stdshutdown;
static mpt_detach_handler_t    mpt_stddetach;
static mpt_unload_handler_t    mpt_stdunload;
static struct mpt_personality mpt_default_personality =
{
	.load		= mpt_stdload,
	.probe		= mpt_stdprobe,
	.attach		= mpt_stdattach,
	.event		= mpt_stdevent,
	.reset		= mpt_stdreset,
	.shutdown	= mpt_stdshutdown,
	.detach		= mpt_stddetach,
	.unload		= mpt_stdunload
};

static mpt_load_handler_t      mpt_core_load;
static mpt_attach_handler_t    mpt_core_attach;
static mpt_reset_handler_t     mpt_core_ioc_reset;
static mpt_event_handler_t     mpt_core_event;
static mpt_shutdown_handler_t  mpt_core_shutdown;
static mpt_shutdown_handler_t  mpt_core_detach;
static mpt_unload_handler_t    mpt_core_unload;
static struct mpt_personality mpt_core_personality =
{
	.name		= "mpt_core",
	.load		= mpt_core_load,
	.attach		= mpt_core_attach,
	.event		= mpt_core_event,
	.reset		= mpt_core_ioc_reset,
	.shutdown	= mpt_core_shutdown,
	.detach		= mpt_core_detach,
	.unload		= mpt_core_unload,
};

/*
 * Manual declaration so that DECLARE_MPT_PERSONALITY doesn't need
 * ordering information.  We want the core to always register FIRST.
 * other modules are set to SI_ORDER_SECOND.
 */
static moduledata_t mpt_core_mod = {
	"mpt_core", mpt_modevent, &mpt_core_personality
};
DECLARE_MODULE(mpt_core, mpt_core_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(mpt_core, 1);

#define MPT_PERS_ATACHED(pers, mpt) \
	((mpt)->pers_mask & (0x1 << pers->id))


int
mpt_modevent(module_t mod, int type, void *data)
{
	struct mpt_personality *pers;
	int error;

	pers = (struct mpt_personality *)data;

	error = 0;
	switch (type) {
	case MOD_LOAD:
	{
		mpt_load_handler_t **def_handler;
		mpt_load_handler_t **pers_handler;
		int i;

		for (i = 0; i < MPT_MAX_PERSONALITIES; i++) {
			if (mpt_personalities[i] == NULL)
				break;
		}
		if (i >= MPT_MAX_PERSONALITIES) {
			error = ENOMEM;
			break;
		}
		pers->id = i;
		mpt_personalities[i] = pers;

		/* Install standard/noop handlers for any NULL entries. */
		def_handler = MPT_PERS_FIRST_HANDLER(&mpt_default_personality);
		pers_handler = MPT_PERS_FIRST_HANDLER(pers);
		while (pers_handler <= MPT_PERS_LAST_HANDLER(pers)) {
			if (*pers_handler == NULL)
				*pers_handler = *def_handler;
			pers_handler++;
			def_handler++;
		}
		
		error = (pers->load(pers));
		if (error != 0)
			mpt_personalities[i] = NULL;
		break;
	}
	case MOD_SHUTDOWN:
		break;
	case MOD_QUIESCE:
		break;
	case MOD_UNLOAD:
		error = pers->unload(pers);
		mpt_personalities[pers->id] = NULL;
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

int
mpt_stdload(struct mpt_personality *pers)
{
	/* Load is always successfull. */
	return (0);
}

int
mpt_stdprobe(struct mpt_softc *mpt)
{
	/* Probe is always successfull. */
	return (0);
}

int
mpt_stdattach(struct mpt_softc *mpt)
{
	/* Attach is always successfull. */
	return (0);
}

int
mpt_stdevent(struct mpt_softc *mpt, request_t *req, MSG_EVENT_NOTIFY_REPLY *rep)
{
	/* Event was not for us. */
	return (0);
}

void
mpt_stdreset(struct mpt_softc *mpt, int type)
{
}

void
mpt_stdshutdown(struct mpt_softc *mpt)
{
}

void
mpt_stddetach(struct mpt_softc *mpt)
{
}

int
mpt_stdunload(struct mpt_personality *pers)
{
	/* Unload is always successfull. */
	return (0);
}

/******************************* Bus DMA Support ******************************/
void
mpt_map_rquest(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct mpt_map_info *map_info;

	map_info = (struct mpt_map_info *)arg;
	map_info->error = error;
	map_info->phys = segs->ds_addr;
}

/**************************** Reply/Event Handling ****************************/
int
mpt_register_handler(struct mpt_softc *mpt, mpt_handler_type type,
		     mpt_handler_t handler, uint32_t *phandler_id)
{

	switch (type) {
	case MPT_HANDLER_REPLY:
	{
		u_int cbi;
		u_int free_cbi;

		if (phandler_id == NULL)
			return (EINVAL);

		free_cbi = MPT_HANDLER_ID_NONE;
		for (cbi = 0; cbi < MPT_NUM_REPLY_HANDLERS; cbi++) {
			/*
			 * If the same handler is registered multiple
			 * times, don't error out.  Just return the
			 * index of the original registration.
			 */
			if (mpt_reply_handlers[cbi] == handler.reply_handler) {
				*phandler_id = MPT_CBI_TO_HID(cbi);
				return (0);
			}

			/*
			 * Fill from the front in the hope that
			 * all registered handlers consume only a
			 * single cache line.
			 *
			 * We don't break on the first empty slot so
			 * that the full table is checked to see if
			 * this handler was previously registered.
			 */
			if (free_cbi == MPT_HANDLER_ID_NONE
			 && (mpt_reply_handlers[cbi]
			  == mpt_default_reply_handler))
				free_cbi = cbi;
		}
		if (free_cbi == MPT_HANDLER_ID_NONE)
			return (ENOMEM);
		mpt_reply_handlers[free_cbi] = handler.reply_handler;
		*phandler_id = MPT_CBI_TO_HID(free_cbi);
		break;
	}
	default:
		mpt_prt(mpt, "mpt_register_handler unknown type %d\n", type);
		return (EINVAL);
	}
	return (0);
}

int
mpt_deregister_handler(struct mpt_softc *mpt, mpt_handler_type type,
		       mpt_handler_t handler, uint32_t handler_id)
{

	switch (type) {
	case MPT_HANDLER_REPLY:
	{
		u_int cbi;

		cbi = MPT_CBI(handler_id);
		if (cbi >= MPT_NUM_REPLY_HANDLERS
		 || mpt_reply_handlers[cbi] != handler.reply_handler)
			return (ENOENT);
		mpt_reply_handlers[cbi] = mpt_default_reply_handler;
		break;
	}
	default:
		mpt_prt(mpt, "mpt_deregister_handler unknown type %d\n", type);
		return (EINVAL);
	}
	return (0);
}

static int
mpt_default_reply_handler(struct mpt_softc *mpt, request_t *req,
			  MSG_DEFAULT_REPLY *reply_frame)
{
	mpt_prt(mpt, "XXXX Default Handler Called.  Req %p, Frame %p\n",
		req, reply_frame);

	if (reply_frame != NULL)
		mpt_dump_reply_frame(mpt, reply_frame);

	mpt_prt(mpt, "XXXX Reply Frame Ignored\n");

	return (/*free_reply*/TRUE);
}

static int
mpt_config_reply_handler(struct mpt_softc *mpt, request_t *req,
				MSG_DEFAULT_REPLY *reply_frame)
{
	if (req != NULL) {

		if (reply_frame != NULL) {
			MSG_CONFIG *cfgp;
			MSG_CONFIG_REPLY *reply;

			cfgp = (MSG_CONFIG *)req->req_vbuf;
			reply = (MSG_CONFIG_REPLY *)reply_frame;
			req->IOCStatus = le16toh(reply_frame->IOCStatus);
			bcopy(&reply->Header, &cfgp->Header,
			      sizeof(cfgp->Header));
		}
		req->state &= ~REQ_STATE_QUEUED;
		req->state |= REQ_STATE_DONE;
		TAILQ_REMOVE(&mpt->request_pending_list, req, links);

		if ((req->state & REQ_STATE_NEED_WAKEUP) != 0)
			wakeup(req);
	}

	return (/*free_reply*/TRUE);
}

static int
mpt_handshake_reply_handler(struct mpt_softc *mpt, request_t *req,
			 MSG_DEFAULT_REPLY *reply_frame)
{
	/* Nothing to be done. */
	return (/*free_reply*/TRUE);
}

static int
mpt_event_reply_handler(struct mpt_softc *mpt, request_t *req,
			MSG_DEFAULT_REPLY *reply_frame)
{
	int free_reply;

	if (reply_frame == NULL) {
		mpt_prt(mpt, "Event Handler: req %p - Unexpected NULL reply\n");
		return (/*free_reply*/TRUE);
	}

	free_reply = TRUE;
	switch (reply_frame->Function) {
	case MPI_FUNCTION_EVENT_NOTIFICATION:
	{
		MSG_EVENT_NOTIFY_REPLY *msg;
		struct mpt_personality *pers;
		u_int handled;

		handled = 0;
		msg = (MSG_EVENT_NOTIFY_REPLY *)reply_frame;
		MPT_PERS_FOREACH(mpt, pers)
			handled += pers->event(mpt, req, msg);

		if (handled == 0)
			mpt_prt(mpt,
				"Unhandled Event Notify Frame. Event %#x.\n",
				msg->Event);

		if (msg->AckRequired) {
			request_t *ack_req;
			uint32_t context;

			context = htole32(req->index|MPT_REPLY_HANDLER_EVENTS);
			ack_req = mpt_get_request(mpt, /*sleep_ok*/FALSE);
			if (ack_req == NULL) {
				struct mpt_evtf_record *evtf;

				evtf = (struct mpt_evtf_record *)reply_frame;
				evtf->context = context;
				LIST_INSERT_HEAD(&mpt->ack_frames, evtf, links);
				free_reply = FALSE;
				break;
			}
			mpt_send_event_ack(mpt, ack_req, msg, context);
		}
		break;
	}
	case MPI_FUNCTION_PORT_ENABLE:
		mpt_lprt(mpt, MPT_PRT_DEBUG, "enable port reply\n");
		break;
	case MPI_FUNCTION_EVENT_ACK:
		break;
	default:
		mpt_prt(mpt, "Unknown Event Function: %x\n",
			reply_frame->Function);
		break;
	}

	if (req != NULL
	 && (reply_frame->MsgFlags & MPI_MSGFLAGS_CONTINUATION_REPLY) == 0) {

		req->state &= ~REQ_STATE_QUEUED;
		req->state |= REQ_STATE_DONE;
		TAILQ_REMOVE(&mpt->request_pending_list, req, links);

		if ((req->state & REQ_STATE_NEED_WAKEUP) != 0)
			wakeup(req);
		else
			mpt_free_request(mpt, req);
	}
	return (free_reply);
}

/*
 * Process an asynchronous event from the IOC.
 */
static int
mpt_core_event(struct mpt_softc *mpt, request_t *req,
	       MSG_EVENT_NOTIFY_REPLY *msg)
{
	switch(msg->Event & 0xFF) {
	case MPI_EVENT_NONE:
		break;
	case MPI_EVENT_LOG_DATA:
	{
		int i;

		/* Some error occured that LSI wants logged */
		mpt_prt(mpt, "EvtLogData: IOCLogInfo: 0x%08x\n",
			msg->IOCLogInfo);
		mpt_prt(mpt, "\tEvtLogData: Event Data:");
		for (i = 0; i < msg->EventDataLength; i++)
			mpt_prtc(mpt, "  %08x", msg->Data[i]);
		mpt_prtc(mpt, "\n");
		break;
	}
	case MPI_EVENT_EVENT_CHANGE:
		/*
		 * This is just an acknowledgement
		 * of our mpt_send_event_request.
		 */
		break;
	default:
		return (/*handled*/0);
		break;
	}
	return (/*handled*/1);
}

static void
mpt_send_event_ack(struct mpt_softc *mpt, request_t *ack_req,
		   MSG_EVENT_NOTIFY_REPLY *msg, uint32_t context)
{
	MSG_EVENT_ACK *ackp;

	ackp = (MSG_EVENT_ACK *)ack_req->req_vbuf;
	bzero(ackp, sizeof *ackp);
	ackp->Function = MPI_FUNCTION_EVENT_ACK;
	ackp->Event = msg->Event;
	ackp->EventContext = msg->EventContext;
	ackp->MsgContext = context;
	mpt_check_doorbell(mpt);
	mpt_send_cmd(mpt, ack_req);
}

/***************************** Interrupt Handling *****************************/
void
mpt_intr(void *arg)
{
	struct mpt_softc *mpt;
	uint32_t     reply_desc;

	mpt = (struct mpt_softc *)arg;
	while ((reply_desc = mpt_pop_reply_queue(mpt)) != MPT_REPLY_EMPTY) {
		request_t	  *req;
		MSG_DEFAULT_REPLY *reply_frame;
		uint32_t	   reply_baddr;
		u_int		   cb_index;
		u_int		   req_index;
		int		   free_rf;

		req = NULL;
		reply_frame = NULL;
		reply_baddr = 0;
		if ((reply_desc & MPI_ADDRESS_REPLY_A_BIT) != 0) {
			u_int offset;

			/*
			 * Insure that the reply frame is coherent.
			 */
			reply_baddr = (reply_desc << 1);
			offset = reply_baddr - (mpt->reply_phys & 0xFFFFFFFF);
			bus_dmamap_sync_range(mpt->reply_dmat, mpt->reply_dmap,
					      offset, MPT_REPLY_SIZE,
					      BUS_DMASYNC_POSTREAD);
			reply_frame = MPT_REPLY_OTOV(mpt, offset);
			reply_desc = le32toh(reply_frame->MsgContext);
		}
		cb_index = MPT_CONTEXT_TO_CBI(reply_desc);
		req_index = MPT_CONTEXT_TO_REQI(reply_desc);
		if (req_index < MPT_MAX_REQUESTS(mpt))
			req = &mpt->request_pool[req_index];

		free_rf = mpt_reply_handlers[cb_index](mpt, req, reply_frame);

		if (reply_frame != NULL && free_rf)
			mpt_free_reply(mpt, reply_baddr);
	}
}

/******************************* Error Recovery *******************************/
void
mpt_complete_request_chain(struct mpt_softc *mpt, struct req_queue *chain,
			    u_int iocstatus)
{
	MSG_DEFAULT_REPLY  ioc_status_frame;
	request_t	  *req;

	bzero(&ioc_status_frame, sizeof(ioc_status_frame));
	ioc_status_frame.MsgLength = roundup2(sizeof(ioc_status_frame), 4);
	ioc_status_frame.IOCStatus = iocstatus; 
	while((req = TAILQ_FIRST(chain)) != NULL) {
		MSG_REQUEST_HEADER *msg_hdr;
		u_int		    cb_index;

		msg_hdr = (MSG_REQUEST_HEADER *)req->req_vbuf;
		ioc_status_frame.Function = msg_hdr->Function; 
		ioc_status_frame.MsgContext = msg_hdr->MsgContext; 
		cb_index = MPT_CONTEXT_TO_CBI(le32toh(msg_hdr->MsgContext));
		mpt_reply_handlers[cb_index](mpt, req, &ioc_status_frame);
	}
}

/********************************* Diagnostics ********************************/
/*
 * Perform a diagnostic dump of a reply frame.
 */
void
mpt_dump_reply_frame(struct mpt_softc *mpt, MSG_DEFAULT_REPLY *reply_frame)
{

	mpt_prt(mpt, "Address Reply:\n");
	mpt_print_reply(reply_frame);
}

/******************************* Doorbell Access ******************************/
static __inline uint32_t mpt_rd_db(struct mpt_softc *mpt);
static __inline  uint32_t mpt_rd_intr(struct mpt_softc *mpt);

static __inline uint32_t
mpt_rd_db(struct mpt_softc *mpt)
{
	return mpt_read(mpt, MPT_OFFSET_DOORBELL);
}

static __inline uint32_t
mpt_rd_intr(struct mpt_softc *mpt)
{
	return mpt_read(mpt, MPT_OFFSET_INTR_STATUS);
}

/* Busy wait for a door bell to be read by IOC */
static int
mpt_wait_db_ack(struct mpt_softc *mpt)
{
	int i;
	for (i=0; i < MPT_MAX_WAIT; i++) {
		if (!MPT_DB_IS_BUSY(mpt_rd_intr(mpt))) {
			maxwait_ack = i > maxwait_ack ? i : maxwait_ack;
			return MPT_OK;
		}

		DELAY(1000);
	}
	return MPT_FAIL;
}

/* Busy wait for a door bell interrupt */
static int
mpt_wait_db_int(struct mpt_softc *mpt)
{
	int i;
	for (i=0; i < MPT_MAX_WAIT; i++) {
		if (MPT_DB_INTR(mpt_rd_intr(mpt))) {
			maxwait_int = i > maxwait_int ? i : maxwait_int;
			return MPT_OK;
		}
		DELAY(100);
	}
	return MPT_FAIL;
}

/* Wait for IOC to transition to a give state */
void
mpt_check_doorbell(struct mpt_softc *mpt)
{
	uint32_t db = mpt_rd_db(mpt);
	if (MPT_STATE(db) != MPT_DB_STATE_RUNNING) {
		mpt_prt(mpt, "Device not running\n");
		mpt_print_db(db);
	}
}

/* Wait for IOC to transition to a give state */
static int
mpt_wait_state(struct mpt_softc *mpt, enum DB_STATE_BITS state)
{
	int i;

	for (i = 0; i < MPT_MAX_WAIT; i++) {
		uint32_t db = mpt_rd_db(mpt);
		if (MPT_STATE(db) == state) {
			maxwait_state = i > maxwait_state ? i : maxwait_state;
			return (MPT_OK);
		}
		DELAY(100);
	}
	return (MPT_FAIL);
}


/************************* Intialization/Configuration ************************/
static int mpt_download_fw(struct mpt_softc *mpt);

/* Issue the reset COMMAND to the IOC */
static int
mpt_soft_reset(struct mpt_softc *mpt)
{
	mpt_lprt(mpt, MPT_PRT_DEBUG, "soft reset\n");

	/* Have to use hard reset if we are not in Running state */
	if (MPT_STATE(mpt_rd_db(mpt)) != MPT_DB_STATE_RUNNING) {
		mpt_prt(mpt, "soft reset failed: device not running\n");
		return MPT_FAIL;
	}

	/* If door bell is in use we don't have a chance of getting
	 * a word in since the IOC probably crashed in message
	 * processing. So don't waste our time.
	 */
	if (MPT_DB_IS_IN_USE(mpt_rd_db(mpt))) {
		mpt_prt(mpt, "soft reset failed: doorbell wedged\n");
		return MPT_FAIL;
	}

	/* Send the reset request to the IOC */
	mpt_write(mpt, MPT_OFFSET_DOORBELL,
	    MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET << MPI_DOORBELL_FUNCTION_SHIFT);
	if (mpt_wait_db_ack(mpt) != MPT_OK) {
		mpt_prt(mpt, "soft reset failed: ack timeout\n");
		return MPT_FAIL;
	}

	/* Wait for the IOC to reload and come out of reset state */
	if (mpt_wait_state(mpt, MPT_DB_STATE_READY) != MPT_OK) {
		mpt_prt(mpt, "soft reset failed: device did not restart\n");
		return MPT_FAIL;
	}

	return MPT_OK;
}

static int
mpt_enable_diag_mode(struct mpt_softc *mpt)
{
	int try;

	try = 20;
	while (--try) {

		if ((mpt_read(mpt, MPT_OFFSET_DIAGNOSTIC) & MPI_DIAG_DRWE) != 0)
			break;

		/* Enable diagnostic registers */
		mpt_write(mpt, MPT_OFFSET_SEQUENCE, 0xFF);
		mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPI_WRSEQ_1ST_KEY_VALUE);
		mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPI_WRSEQ_2ND_KEY_VALUE);
		mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPI_WRSEQ_3RD_KEY_VALUE);
		mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPI_WRSEQ_4TH_KEY_VALUE);
		mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPI_WRSEQ_5TH_KEY_VALUE);

		DELAY(100000);
	}
	if (try == 0)
		return (EIO);
	return (0);
}

static void
mpt_disable_diag_mode(struct mpt_softc *mpt)
{
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, 0xFFFFFFFF);
}

/* This is a magic diagnostic reset that resets all the ARM
 * processors in the chip. 
 */
static void
mpt_hard_reset(struct mpt_softc *mpt)
{
	int error;
	int wait;
	uint32_t diagreg;

	mpt_lprt(mpt, MPT_PRT_DEBUG, "hard reset\n");

	error = mpt_enable_diag_mode(mpt);
	if (error) {
		mpt_prt(mpt, "WARNING - Could not enter diagnostic mode !\n");
		mpt_prt(mpt, "Trying to reset anyway.\n");
	}

	diagreg = mpt_read(mpt, MPT_OFFSET_DIAGNOSTIC);

	/*
	 * This appears to be a workaround required for some
	 * firmware or hardware revs.
	 */
	mpt_write(mpt, MPT_OFFSET_DIAGNOSTIC, diagreg | MPI_DIAG_DISABLE_ARM);
	DELAY(1000);

	/* Diag. port is now active so we can now hit the reset bit */
	mpt_write(mpt, MPT_OFFSET_DIAGNOSTIC, diagreg | MPI_DIAG_RESET_ADAPTER);

        /*
         * Ensure that the reset has finished.  We delay 1ms
         * prior to reading the register to make sure the chip
         * has sufficiently completed its reset to handle register
         * accesses.
         */
	wait = 5000;
	do {
		DELAY(1000);
		diagreg = mpt_read(mpt, MPT_OFFSET_DIAGNOSTIC);
	} while (--wait && (diagreg & MPI_DIAG_RESET_ADAPTER) == 0);

	if (wait == 0) {
		mpt_prt(mpt, "WARNING - Failed hard reset! "
			"Trying to initialize anyway.\n");
	}

	/*
	 * If we have firmware to download, it must be loaded before
	 * the controller will become operational.  Do so now.
	 */
	if (mpt->fw_image != NULL) {

		error = mpt_download_fw(mpt);

		if (error) {
			mpt_prt(mpt, "WARNING - Firmware Download Failed!\n");
			mpt_prt(mpt, "Trying to initialize anyway.\n");
		}
	}

	/*
	 * Reseting the controller should have disabled write
	 * access to the diagnostic registers, but disable
	 * manually to be sure.
	 */
	mpt_disable_diag_mode(mpt);
}

static void
mpt_core_ioc_reset(struct mpt_softc *mpt, int type)
{
	/*
	 * Complete all pending requests with a status
	 * appropriate for an IOC reset.
	 */
	mpt_complete_request_chain(mpt, &mpt->request_pending_list,
				   MPI_IOCSTATUS_INVALID_STATE);
}


/*
 * Reset the IOC when needed. Try software command first then if needed
 * poke at the magic diagnostic reset. Note that a hard reset resets
 * *both* IOCs on dual function chips (FC929 && LSI1030) as well as
 * fouls up the PCI configuration registers.
 */
int
mpt_reset(struct mpt_softc *mpt, int reinit)
{
	struct	mpt_personality *pers;
	int	ret;

	/* Try a soft reset */
	if ((ret = mpt_soft_reset(mpt)) != MPT_OK) {
		/* Failed; do a hard reset */
		mpt_hard_reset(mpt);

		/* Wait for the IOC to reload and come out of reset state */
		ret = mpt_wait_state(mpt, MPT_DB_STATE_READY);
		if (ret != MPT_OK)
			mpt_prt(mpt, "failed to reset device\n");
	}

	/*
	 * Invoke reset handlers.  We bump the reset count so
	 * that mpt_wait_req() understands that regardless of
	 * the specified wait condition, it should stop its wait.
	 */
	mpt->reset_cnt++;
	MPT_PERS_FOREACH(mpt, pers)
		pers->reset(mpt, ret);

	if (reinit != 0)
		mpt_enable_ioc(mpt);

	return ret;
}

/* Return a command buffer to the free queue */
void
mpt_free_request(struct mpt_softc *mpt, request_t *req)
{
	struct mpt_evtf_record *record;
	uint32_t reply_baddr;
	
	if (req == NULL || req != &mpt->request_pool[req->index]) {
		panic("mpt_free_request bad req ptr\n");
		return;
	}
	req->ccb = NULL;
	req->state = REQ_STATE_FREE;
	if (LIST_EMPTY(&mpt->ack_frames)) {
		TAILQ_INSERT_HEAD(&mpt->request_free_list, req, links);
		if (mpt->getreqwaiter != 0) {
			mpt->getreqwaiter = 0;
			wakeup(&mpt->request_free_list);
		}
		return;
	}

	/*
	 * Process an ack frame deferred due to resource shortage.
	 */
	record = LIST_FIRST(&mpt->ack_frames);
	LIST_REMOVE(record, links);
	mpt_send_event_ack(mpt, req, &record->reply, record->context);
	reply_baddr = (uint32_t)((uint8_t *)record - mpt->reply)
		    + (mpt->reply_phys & 0xFFFFFFFF);
	mpt_free_reply(mpt, reply_baddr);
}

/* Get a command buffer from the free queue */
request_t *
mpt_get_request(struct mpt_softc *mpt, int sleep_ok)
{
	request_t *req;

retry:
	req = TAILQ_FIRST(&mpt->request_free_list);
	if (req != NULL) {
		KASSERT(req == &mpt->request_pool[req->index],
		    ("mpt_get_request: corrupted request free list\n"));
		TAILQ_REMOVE(&mpt->request_free_list, req, links);
		req->state = REQ_STATE_ALLOCATED;
	} else if (sleep_ok != 0) {
		mpt->getreqwaiter = 1;
		mpt_sleep(mpt, &mpt->request_free_list, PUSER, "mptgreq", 0);
		goto retry;
	}
	return req;
}

/* Pass the command to the IOC */
void
mpt_send_cmd(struct mpt_softc *mpt, request_t *req)
{
	uint32_t *pReq;

	pReq = req->req_vbuf;
	mpt_lprt(mpt, MPT_PRT_TRACE, "Send Request %d (0x%x):\n",
		 req->index, req->req_pbuf);
	mpt_lprt(mpt, MPT_PRT_TRACE, "%08x %08x %08x %08x\n",
		 pReq[0], pReq[1], pReq[2], pReq[3]);
	mpt_lprt(mpt, MPT_PRT_TRACE, "%08x %08x %08x %08x\n",
		 pReq[4], pReq[5], pReq[6], pReq[7]);
	mpt_lprt(mpt, MPT_PRT_TRACE, "%08x %08x %08x %08x\n",
		 pReq[8], pReq[9], pReq[10], pReq[11]);
	mpt_lprt(mpt, MPT_PRT_TRACE, "%08x %08x %08x %08x\n",
		 pReq[12], pReq[13], pReq[14], pReq[15]);

	bus_dmamap_sync(mpt->request_dmat, mpt->request_dmap,
	    BUS_DMASYNC_PREWRITE);
	req->state |= REQ_STATE_QUEUED;
	TAILQ_INSERT_HEAD(&mpt->request_pending_list, req, links);
	mpt_write(mpt, MPT_OFFSET_REQUEST_Q, (uint32_t) req->req_pbuf);
}

/*
 * Wait for a request to complete.
 *
 * Inputs:
 *	mpt		softc of controller executing request
 *	req		request to wait for
 *	sleep_ok	nonzero implies may sleep in this context
 *	time_ms		timeout in ms.  0 implies no timeout.
 *
 * Return Values:
 *	0		Request completed
 *	non-0		Timeout fired before request completion.
 */
int
mpt_wait_req(struct mpt_softc *mpt, request_t *req,
	     mpt_req_state_t state, mpt_req_state_t mask,
	     int sleep_ok, int time_ms)
{
	int   error;
	int   timeout;
	u_int saved_cnt;

	/*
	 * timeout is in ms.  0 indicates infinite wait.
	 * Convert to ticks or 500us units depending on
	 * our sleep mode.
	 */
	if (sleep_ok != 0)
		timeout = (time_ms * hz) / 1000;
	else
		timeout = time_ms * 2;
	saved_cnt = mpt->reset_cnt;
	req->state |= REQ_STATE_NEED_WAKEUP;
	mask &= ~REQ_STATE_NEED_WAKEUP;
	while ((req->state & mask) != state
	    && mpt->reset_cnt == saved_cnt) {

		if (sleep_ok != 0) {
			error = mpt_sleep(mpt, req, PUSER, "mptreq", timeout);
			if (error == EWOULDBLOCK) {
				timeout = 0;
				break;
			}
		} else {
			if (time_ms != 0 && --timeout == 0) {
				mpt_prt(mpt, "mpt_wait_req timed out\n");
				break;
			}
			DELAY(500);
			mpt_intr(mpt);
		}
	}
	req->state &= ~REQ_STATE_NEED_WAKEUP;
	if (mpt->reset_cnt != saved_cnt)
		return (EIO);
	if (time_ms && timeout == 0)
		return (ETIMEDOUT);
	return (0);
}

/*
 * Send a command to the IOC via the handshake register.
 *
 * Only done at initialization time and for certain unusual
 * commands such as device/bus reset as specified by LSI.
 */
int
mpt_send_handshake_cmd(struct mpt_softc *mpt, size_t len, void *cmd)
{
	int i;
	uint32_t data, *data32;

	/* Check condition of the IOC */
	data = mpt_rd_db(mpt);
	if ((MPT_STATE(data) != MPT_DB_STATE_READY
	  && MPT_STATE(data) != MPT_DB_STATE_RUNNING
	  && MPT_STATE(data) != MPT_DB_STATE_FAULT)
	 || MPT_DB_IS_IN_USE(data)) {
		mpt_prt(mpt, "handshake aborted - invalid doorbell state\n");
		mpt_print_db(data);
		return (EBUSY);
	}

	/* We move things in 32 bit chunks */
	len = (len + 3) >> 2;
	data32 = cmd;

	/* Clear any left over pending doorbell interupts */
	if (MPT_DB_INTR(mpt_rd_intr(mpt)))
		mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	/*
	 * Tell the handshake reg. we are going to send a command
         * and how long it is going to be.
	 */
	data = (MPI_FUNCTION_HANDSHAKE << MPI_DOORBELL_FUNCTION_SHIFT) |
	    (len << MPI_DOORBELL_ADD_DWORDS_SHIFT);
	mpt_write(mpt, MPT_OFFSET_DOORBELL, data);

	/* Wait for the chip to notice */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_send_handshake_cmd timeout1\n");
		return (ETIMEDOUT);
	}

	/* Clear the interrupt */
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	if (mpt_wait_db_ack(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_send_handshake_cmd timeout2\n");
		return (ETIMEDOUT);
	}

	/* Send the command */
	for (i = 0; i < len; i++) {
		mpt_write(mpt, MPT_OFFSET_DOORBELL, *data32++);
		if (mpt_wait_db_ack(mpt) != MPT_OK) {
			mpt_prt(mpt, 
				"mpt_send_handshake_cmd timeout! index = %d\n",
				i);
			return (ETIMEDOUT);
		}
	}
	return MPT_OK;
}

/* Get the response from the handshake register */
int
mpt_recv_handshake_reply(struct mpt_softc *mpt, size_t reply_len, void *reply)
{
	int left, reply_left;
	u_int16_t *data16;
	MSG_DEFAULT_REPLY *hdr;

	/* We move things out in 16 bit chunks */
	reply_len >>= 1;
	data16 = (u_int16_t *)reply;

	hdr = (MSG_DEFAULT_REPLY *)reply;

	/* Get first word */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_recv_handshake_cmd timeout1\n");
		return ETIMEDOUT;
	}
	*data16++ = mpt_read(mpt, MPT_OFFSET_DOORBELL) & MPT_DB_DATA_MASK;
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	/* Get Second Word */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_recv_handshake_cmd timeout2\n");
		return ETIMEDOUT;
	}
	*data16++ = mpt_read(mpt, MPT_OFFSET_DOORBELL) & MPT_DB_DATA_MASK;
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	/* With the second word, we can now look at the length */
	if (((reply_len >> 1) != hdr->MsgLength)) {
		mpt_prt(mpt, "reply length does not match message length: "
			"got 0x%02x, expected 0x%02x\n",
			hdr->MsgLength << 2, reply_len << 1);
	}

	/* Get rest of the reply; but don't overflow the provided buffer */
	left = (hdr->MsgLength << 1) - 2;
	reply_left =  reply_len - 2;
	while (left--) {
		u_int16_t datum;

		if (mpt_wait_db_int(mpt) != MPT_OK) {
			mpt_prt(mpt, "mpt_recv_handshake_cmd timeout3\n");
			return ETIMEDOUT;
		}
		datum = mpt_read(mpt, MPT_OFFSET_DOORBELL);

		if (reply_left-- > 0)
			*data16++ = datum & MPT_DB_DATA_MASK;

		mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);
	}

	/* One more wait & clear at the end */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_recv_handshake_cmd timeout4\n");
		return ETIMEDOUT;
	}
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	if ((hdr->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		if (mpt->verbose >= MPT_PRT_TRACE)
			mpt_print_reply(hdr);
		return (MPT_FAIL | hdr->IOCStatus);
	}

	return (0);
}

static int
mpt_get_iocfacts(struct mpt_softc *mpt, MSG_IOC_FACTS_REPLY *freplp)
{
	MSG_IOC_FACTS f_req;
	int error;
	
	bzero(&f_req, sizeof f_req);
	f_req.Function = MPI_FUNCTION_IOC_FACTS;
	f_req.MsgContext = htole32(MPT_REPLY_HANDLER_HANDSHAKE);
	error = mpt_send_handshake_cmd(mpt, sizeof f_req, &f_req);
	if (error)
		return(error);
	error = mpt_recv_handshake_reply(mpt, sizeof (*freplp), freplp);
	return (error);
}

static int
mpt_get_portfacts(struct mpt_softc *mpt, MSG_PORT_FACTS_REPLY *freplp)
{
	MSG_PORT_FACTS f_req;
	int error;
	
	/* XXX: Only getting PORT FACTS for Port 0 */
	memset(&f_req, 0, sizeof f_req);
	f_req.Function = MPI_FUNCTION_PORT_FACTS;
	f_req.MsgContext = htole32(MPT_REPLY_HANDLER_HANDSHAKE);
	error = mpt_send_handshake_cmd(mpt, sizeof f_req, &f_req);
	if (error)
		return(error);
	error = mpt_recv_handshake_reply(mpt, sizeof (*freplp), freplp);
	return (error);
}

/*
 * Send the initialization request. This is where we specify how many
 * SCSI busses and how many devices per bus we wish to emulate.
 * This is also the command that specifies the max size of the reply
 * frames from the IOC that we will be allocating.
 */
static int
mpt_send_ioc_init(struct mpt_softc *mpt, uint32_t who)
{
	int error = 0;
	MSG_IOC_INIT init;
	MSG_IOC_INIT_REPLY reply;

	bzero(&init, sizeof init);
	init.WhoInit = who;
	init.Function = MPI_FUNCTION_IOC_INIT;
	if (mpt->is_fc) {
		init.MaxDevices = 255;
	} else {
		init.MaxDevices = 16;
	}
	init.MaxBuses = 1;
	init.ReplyFrameSize = MPT_REPLY_SIZE;
	init.MsgContext = htole32(MPT_REPLY_HANDLER_HANDSHAKE);

	if ((error = mpt_send_handshake_cmd(mpt, sizeof init, &init)) != 0) {
		return(error);
	}

	error = mpt_recv_handshake_reply(mpt, sizeof reply, &reply);
	return (error);
}


/*
 * Utiltity routine to read configuration headers and pages
 */
int
mpt_issue_cfg_req(struct mpt_softc *mpt, request_t *req, u_int Action,
		  u_int PageVersion, u_int PageLength, u_int PageNumber,
		  u_int PageType, uint32_t PageAddress, bus_addr_t addr,
		  bus_size_t len, int sleep_ok, int timeout_ms)
{
	MSG_CONFIG *cfgp;
	SGE_SIMPLE32 *se;

	cfgp = req->req_vbuf;
	memset(cfgp, 0, sizeof *cfgp);
	cfgp->Action = Action;
	cfgp->Function = MPI_FUNCTION_CONFIG;
	cfgp->Header.PageVersion = PageVersion;
	cfgp->Header.PageLength = PageLength;
	cfgp->Header.PageNumber = PageNumber;
	cfgp->Header.PageType = PageType;
	cfgp->PageAddress = PageAddress;
	se = (SGE_SIMPLE32 *)&cfgp->PageBufferSGE;
	se->Address = addr;
	MPI_pSGE_SET_LENGTH(se, len);
	MPI_pSGE_SET_FLAGS(se, (MPI_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER |
	    MPI_SGE_FLAGS_END_OF_LIST |
	    ((Action == MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT
	  || Action == MPI_CONFIG_ACTION_PAGE_WRITE_NVRAM)
	   ? MPI_SGE_FLAGS_HOST_TO_IOC : MPI_SGE_FLAGS_IOC_TO_HOST)));
	cfgp->MsgContext = htole32(req->index | MPT_REPLY_HANDLER_CONFIG);

	mpt_check_doorbell(mpt);
	mpt_send_cmd(mpt, req);
	return (mpt_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE,
			     sleep_ok, timeout_ms));
}


int
mpt_read_cfg_header(struct mpt_softc *mpt, int PageType, int PageNumber,
		    uint32_t PageAddress, CONFIG_PAGE_HEADER *rslt,
		    int sleep_ok, int timeout_ms)
{
	request_t  *req;
	int	    error;

	req = mpt_get_request(mpt, sleep_ok);
	if (req == NULL) {
		mpt_prt(mpt, "mpt_read_cfg_header: Get request failed!\n");
		return (-1);
	}

	error = mpt_issue_cfg_req(mpt, req, MPI_CONFIG_ACTION_PAGE_HEADER,
				  /*PageVersion*/0, /*PageLength*/0, PageNumber,
				  PageType, PageAddress, /*addr*/0, /*len*/0,
				  sleep_ok, timeout_ms);
	if (error != 0) {
		mpt_prt(mpt, "read_cfg_header timed out\n");
		return (-1);
	}

        if ((req->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		mpt_prt(mpt, "mpt_read_cfg_header: Config Info Status %x\n",
			req->IOCStatus);
		error = -1;
	} else {
		MSG_CONFIG *cfgp;

		cfgp = req->req_vbuf;
		bcopy(&cfgp->Header, rslt, sizeof(*rslt));
		error = 0;
	}
	mpt_free_request(mpt, req);
	return (error);
}

#define	CFG_DATA_OFF	128

int
mpt_read_cfg_page(struct mpt_softc *mpt, int Action, uint32_t PageAddress,
		  CONFIG_PAGE_HEADER *hdr, size_t len, int sleep_ok,
		  int timeout_ms)
{
	request_t    *req;
	int	      error;

	req = mpt_get_request(mpt, sleep_ok);
	if (req == NULL) {
		mpt_prt(mpt, "mpt_read_cfg_page: Get request failed!\n");
		return (-1);
	}

	error = mpt_issue_cfg_req(mpt, req, Action, hdr->PageVersion,
				  hdr->PageLength, hdr->PageNumber,
				  hdr->PageType & MPI_CONFIG_PAGETYPE_MASK,
				  PageAddress, req->req_pbuf + CFG_DATA_OFF,
				  len, sleep_ok, timeout_ms);
	if (error != 0) {
		mpt_prt(mpt, "read_cfg_page(%d) timed out\n", Action);
		return (-1);
	}

	if ((req->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		mpt_prt(mpt, "mpt_read_cfg_page: Config Info Status %x\n",
			req->IOCStatus);
		mpt_free_request(mpt, req);
		return (-1);
	}
	bus_dmamap_sync(mpt->request_dmat, mpt->request_dmap,
	    BUS_DMASYNC_POSTREAD);
	memcpy(hdr, ((uint8_t *)req->req_vbuf)+CFG_DATA_OFF, len);
	mpt_free_request(mpt, req);
	return (0);
}

int
mpt_write_cfg_page(struct mpt_softc *mpt, int Action, uint32_t PageAddress,
		   CONFIG_PAGE_HEADER *hdr, size_t len, int sleep_ok,
		   int timeout_ms)
{
	request_t    *req;
	u_int	      hdr_attr;
	int	      error;

	hdr_attr = hdr->PageType & MPI_CONFIG_PAGEATTR_MASK;
	if (hdr_attr != MPI_CONFIG_PAGEATTR_CHANGEABLE &&
	    hdr_attr != MPI_CONFIG_PAGEATTR_PERSISTENT) {
		mpt_prt(mpt, "page type 0x%x not changeable\n",
			hdr->PageType & MPI_CONFIG_PAGETYPE_MASK);
		return (-1);
	}
	hdr->PageType &= MPI_CONFIG_PAGETYPE_MASK,

	req = mpt_get_request(mpt, sleep_ok);
	if (req == NULL)
		return (-1);

	memcpy(((caddr_t)req->req_vbuf)+CFG_DATA_OFF, hdr, len);
	/* Restore stripped out attributes */
	hdr->PageType |= hdr_attr;

	error = mpt_issue_cfg_req(mpt, req, Action, hdr->PageVersion,
				  hdr->PageLength, hdr->PageNumber,
				  hdr->PageType & MPI_CONFIG_PAGETYPE_MASK,
				  PageAddress, req->req_pbuf + CFG_DATA_OFF,
				  len, sleep_ok, timeout_ms);
	if (error != 0) {
		mpt_prt(mpt, "mpt_write_cfg_page timed out\n");
		return (-1);
	}

        if ((req->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		mpt_prt(mpt, "mpt_write_cfg_page: Config Info Status %x\n",
			req->IOCStatus);
		mpt_free_request(mpt, req);
		return (-1);
	}
	mpt_free_request(mpt, req);
	return (0);
}

/*
 * Read IOC configuration information
 */
static int
mpt_read_config_info_ioc(struct mpt_softc *mpt)
{
	CONFIG_PAGE_HEADER hdr;
	struct mpt_raid_volume *mpt_raid;
	int rv;
	int i;
	size_t len;

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_IOC,
				 /*PageNumber*/2, /*PageAddress*/0, &hdr,
				 /*sleep_ok*/FALSE, /*timeout_ms*/5000);
	if (rv)
		return (EIO);

	mpt_lprt(mpt, MPT_PRT_DEBUG,  "IOC Page 2 Header: ver %x, len %x, "
		 "num %x, type %x\n", hdr.PageVersion,
		 hdr.PageLength * sizeof(uint32_t),
		 hdr.PageNumber, hdr.PageType);

	len = hdr.PageLength * sizeof(uint32_t);
	mpt->ioc_page2 = malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (mpt->ioc_page2 == NULL)
		return (ENOMEM);
	memcpy(&mpt->ioc_page2->Header, &hdr, sizeof(hdr));
	rv = mpt_read_cur_cfg_page(mpt, /*PageAddress*/0,
				   &mpt->ioc_page2->Header, len,
				   /*sleep_ok*/FALSE, /*timeout_ms*/5000);
	if (rv) {
		mpt_prt(mpt, "failed to read IOC Page 2\n");
	} else if (mpt->ioc_page2->CapabilitiesFlags != 0) {
		uint32_t mask;

		mpt_prt(mpt, "Capabilities: (");
		for (mask = 1; mask != 0; mask <<= 1) {
			if ((mpt->ioc_page2->CapabilitiesFlags & mask) == 0)
				continue;

			switch (mask) {
			case MPI_IOCPAGE2_CAP_FLAGS_IS_SUPPORT:
				mpt_prtc(mpt, " RAID-0");
				break;
			case MPI_IOCPAGE2_CAP_FLAGS_IME_SUPPORT:
				mpt_prtc(mpt, " RAID-1E");
				break;
			case MPI_IOCPAGE2_CAP_FLAGS_IM_SUPPORT:
				mpt_prtc(mpt, " RAID-1");
				break;
			case MPI_IOCPAGE2_CAP_FLAGS_SES_SUPPORT:
				mpt_prtc(mpt, " SES");
				break;
			case MPI_IOCPAGE2_CAP_FLAGS_SAFTE_SUPPORT:
				mpt_prtc(mpt, " SAFTE");
				break;
			case MPI_IOCPAGE2_CAP_FLAGS_CROSS_CHANNEL_SUPPORT:
				mpt_prtc(mpt, " Multi-Channel-Arrays");
			default:
				break;
			}
		}
		mpt_prtc(mpt, " )\n");
		if ((mpt->ioc_page2->CapabilitiesFlags
		   & (MPI_IOCPAGE2_CAP_FLAGS_IS_SUPPORT
		    | MPI_IOCPAGE2_CAP_FLAGS_IME_SUPPORT
		    | MPI_IOCPAGE2_CAP_FLAGS_IM_SUPPORT)) != 0) {
			mpt_prt(mpt, "%d Active Volume%s(%d Max)\n",
				mpt->ioc_page2->NumActiveVolumes,
				mpt->ioc_page2->NumActiveVolumes != 1
			      ? "s " : " ",
				mpt->ioc_page2->MaxVolumes);
			mpt_prt(mpt, "%d Hidden Drive Member%s(%d Max)\n",
				mpt->ioc_page2->NumActivePhysDisks,
				mpt->ioc_page2->NumActivePhysDisks != 1
			      ? "s " : " ",
				mpt->ioc_page2->MaxPhysDisks);
		}
	}

	len = mpt->ioc_page2->MaxVolumes * sizeof(struct mpt_raid_volume);
	mpt->raid_volumes = malloc(len, M_DEVBUF, M_NOWAIT);
	if (mpt->raid_volumes == NULL) {
		mpt_prt(mpt, "Could not allocate RAID volume data\n");
	} else {
		memset(mpt->raid_volumes, 0, len);
	}

	/*
	 * Copy critical data out of ioc_page2 so that we can
	 * safely refresh the page without windows of unreliable
	 * data.
	 */
	mpt->raid_max_volumes =  mpt->ioc_page2->MaxVolumes;

	len = sizeof(*mpt->raid_volumes->config_page)
	    + (sizeof(RAID_VOL0_PHYS_DISK)*(mpt->ioc_page2->MaxPhysDisks - 1));
	for (i = 0; i < mpt->ioc_page2->MaxVolumes; i++) {
		mpt_raid = &mpt->raid_volumes[i];
		mpt_raid->config_page = malloc(len, M_DEVBUF, M_NOWAIT);
		if (mpt_raid->config_page == NULL) {
			mpt_prt(mpt, "Could not allocate RAID page data\n");
			break;
		}
		memset(mpt_raid->config_page, 0, len);
	}
	mpt->raid_page0_len = len;

	len = mpt->ioc_page2->MaxPhysDisks * sizeof(struct mpt_raid_disk);
	mpt->raid_disks = malloc(len, M_DEVBUF, M_NOWAIT);
	if (mpt->raid_disks == NULL) {
		mpt_prt(mpt, "Could not allocate RAID disk data\n");
	} else {
		memset(mpt->raid_disks, 0, len);
	}

	mpt->raid_max_disks =  mpt->ioc_page2->MaxPhysDisks;

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_IOC,
				 /*PageNumber*/3, /*PageAddress*/0, &hdr,
				 /*sleep_ok*/FALSE, /*timeout_ms*/5000);
	if (rv)
		return (EIO);

	mpt_lprt(mpt, MPT_PRT_DEBUG, "IOC Page 3 Header: %x %x %x %x\n",
		 hdr.PageVersion, hdr.PageLength, hdr.PageNumber, hdr.PageType);

	if (mpt->ioc_page3 != NULL)
		free(mpt->ioc_page3, M_DEVBUF);
	len = hdr.PageLength * sizeof(uint32_t);
	mpt->ioc_page3 = malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (mpt->ioc_page3 == NULL)
		return (-1);
	memcpy(&mpt->ioc_page3->Header, &hdr, sizeof(hdr));
	rv = mpt_read_cur_cfg_page(mpt, /*PageAddress*/0,
				   &mpt->ioc_page3->Header, len,
				   /*sleep_ok*/FALSE, /*timeout_ms*/5000);
	if (rv) {
		mpt_prt(mpt, "failed to read IOC Page 3\n");
	}

	mpt_raid_wakeup(mpt);

	return (0);
}

/*
 * Read SCSI configuration information
 */
static int
mpt_read_config_info_spi(struct mpt_softc *mpt)
{
	int rv, i;

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_PORT, 0,
				 0, &mpt->mpt_port_page0.Header,
				 /*sleep_ok*/FALSE, /*timeout_ms*/5000);
	if (rv)
		return (-1);
	mpt_lprt(mpt, MPT_PRT_DEBUG,
		 "SPI Port Page 0 Header: %x %x %x %x\n",
		 mpt->mpt_port_page0.Header.PageVersion,
		 mpt->mpt_port_page0.Header.PageLength,
		 mpt->mpt_port_page0.Header.PageNumber,
		 mpt->mpt_port_page0.Header.PageType);

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_PORT, 1,
				 0, &mpt->mpt_port_page1.Header,
				 /*sleep_ok*/FALSE, /*timeout_ms*/5000);
	if (rv)
		return (-1);

	mpt_lprt(mpt, MPT_PRT_DEBUG, "SPI Port Page 1 Header: %x %x %x %x\n",
		 mpt->mpt_port_page1.Header.PageVersion,
		 mpt->mpt_port_page1.Header.PageLength,
		 mpt->mpt_port_page1.Header.PageNumber,
		 mpt->mpt_port_page1.Header.PageType);

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_PORT, 2,
				 /*PageAddress*/0, &mpt->mpt_port_page2.Header,
				 /*sleep_ok*/FALSE, /*timeout_ms*/5000);
	if (rv)
		return (-1);

	mpt_lprt(mpt, MPT_PRT_DEBUG,
		 "SPI Port Page 2 Header: %x %x %x %x\n",
		 mpt->mpt_port_page1.Header.PageVersion,
		 mpt->mpt_port_page1.Header.PageLength,
		 mpt->mpt_port_page1.Header.PageNumber,
		 mpt->mpt_port_page1.Header.PageType);

	for (i = 0; i < 16; i++) {
		rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_DEVICE,
					 0, i, &mpt->mpt_dev_page0[i].Header,
					 /*sleep_ok*/FALSE, /*timeout_ms*/5000);
		if (rv)
			return (-1);

		mpt_lprt(mpt, MPT_PRT_DEBUG,
			 "SPI Target %d Device Page 0 Header: %x %x %x %x\n",
			 i, mpt->mpt_dev_page0[i].Header.PageVersion,
			 mpt->mpt_dev_page0[i].Header.PageLength,
			 mpt->mpt_dev_page0[i].Header.PageNumber,
			 mpt->mpt_dev_page0[i].Header.PageType);
		
		rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_DEVICE,
					 1, i, &mpt->mpt_dev_page1[i].Header,
					 /*sleep_ok*/FALSE, /*timeout_ms*/5000);
		if (rv)
			return (-1);

		mpt_lprt(mpt, MPT_PRT_DEBUG,
			 "SPI Target %d Device Page 1 Header: %x %x %x %x\n",
			 i, mpt->mpt_dev_page1[i].Header.PageVersion,
			 mpt->mpt_dev_page1[i].Header.PageLength,
			 mpt->mpt_dev_page1[i].Header.PageNumber,
			 mpt->mpt_dev_page1[i].Header.PageType);
	}

	/*
	 * At this point, we don't *have* to fail. As long as we have
	 * valid config header information, we can (barely) lurch
	 * along.
	 */

	rv = mpt_read_cur_cfg_page(mpt, /*PageAddress*/0,
				   &mpt->mpt_port_page0.Header,
				   sizeof(mpt->mpt_port_page0),
				   /*sleep_ok*/FALSE, /*timeout_ms*/5000);
	if (rv) {
		mpt_prt(mpt, "failed to read SPI Port Page 0\n");
	} else {
		mpt_lprt(mpt, MPT_PRT_DEBUG,
		    "SPI Port Page 0: Capabilities %x PhysicalInterface %x\n",
		    mpt->mpt_port_page0.Capabilities,
		    mpt->mpt_port_page0.PhysicalInterface);
	}

	rv = mpt_read_cur_cfg_page(mpt, /*PageAddress*/0,
				   &mpt->mpt_port_page1.Header,
				   sizeof(mpt->mpt_port_page1),
				   /*sleep_ok*/FALSE, /*timeout_ms*/5000);
	if (rv) {
		mpt_prt(mpt, "failed to read SPI Port Page 1\n");
	} else {
		mpt_lprt(mpt, MPT_PRT_DEBUG,
		    "SPI Port Page 1: Configuration %x OnBusTimerValue %x\n",
		    mpt->mpt_port_page1.Configuration,
		    mpt->mpt_port_page1.OnBusTimerValue);
	}

	rv = mpt_read_cur_cfg_page(mpt, /*PageAddress*/0,
				   &mpt->mpt_port_page2.Header,
				   sizeof(mpt->mpt_port_page2),
				   /*sleep_ok*/FALSE, /*timeout_ms*/5000);
	if (rv) {
		mpt_prt(mpt, "failed to read SPI Port Page 2\n");
	} else {
		mpt_lprt(mpt, MPT_PRT_DEBUG,
		    "SPI Port Page 2: Flags %x Settings %x\n",
		    mpt->mpt_port_page2.PortFlags,
		    mpt->mpt_port_page2.PortSettings);
		for (i = 0; i < 16; i++) {
			mpt_lprt(mpt, MPT_PRT_DEBUG,
		  	    "SPI Port Page 2 Tgt %d: timo %x SF %x Flags %x\n",
			    i, mpt->mpt_port_page2.DeviceSettings[i].Timeout,
			    mpt->mpt_port_page2.DeviceSettings[i].SyncFactor,
			    mpt->mpt_port_page2.DeviceSettings[i].DeviceFlags);
		}
	}

	for (i = 0; i < 16; i++) {
		rv = mpt_read_cur_cfg_page(mpt, /*PageAddress*/i,
					   &mpt->mpt_dev_page0[i].Header,
					   sizeof(*mpt->mpt_dev_page0),
					   /*sleep_ok*/FALSE,
					   /*timeout_ms*/5000);
		if (rv) {
			mpt_prt(mpt,
			    "cannot read SPI Tgt %d Device Page 0\n", i);
			continue;
		}
		mpt_lprt(mpt, MPT_PRT_DEBUG,
			 "SPI Tgt %d Page 0: NParms %x Information %x",
			 i, mpt->mpt_dev_page0[i].NegotiatedParameters,
			 mpt->mpt_dev_page0[i].Information);

		rv = mpt_read_cur_cfg_page(mpt, /*PageAddress*/i,
					   &mpt->mpt_dev_page1[i].Header,
					   sizeof(*mpt->mpt_dev_page1),
					   /*sleep_ok*/FALSE,
					   /*timeout_ms*/5000);
		if (rv) {
			mpt_prt(mpt,
			    "cannot read SPI Tgt %d Device Page 1\n", i);
			continue;
		}
		mpt_lprt(mpt, MPT_PRT_DEBUG,
			 "SPI Tgt %d Page 1: RParms %x Configuration %x\n",
			 i, mpt->mpt_dev_page1[i].RequestedParameters,
			 mpt->mpt_dev_page1[i].Configuration);
	}
	return (0);
}

/*
 * Validate SPI configuration information.
 *
 * In particular, validate SPI Port Page 1.
 */
static int
mpt_set_initial_config_spi(struct mpt_softc *mpt)
{
	int i, pp1val = ((1 << mpt->mpt_ini_id) << 16) | mpt->mpt_ini_id;
	int error;

	mpt->mpt_disc_enable = 0xff;
	mpt->mpt_tag_enable = 0;

	if (mpt->mpt_port_page1.Configuration != pp1val) {
		CONFIG_PAGE_SCSI_PORT_1 tmp;

		mpt_prt(mpt,
		    "SPI Port Page 1 Config value bad (%x)- should be %x\n",
		    mpt->mpt_port_page1.Configuration, pp1val);
		tmp = mpt->mpt_port_page1;
		tmp.Configuration = pp1val;
		error = mpt_write_cur_cfg_page(mpt, /*PageAddress*/0,
					       &tmp.Header, sizeof(tmp),
					       /*sleep_ok*/FALSE,
					       /*timeout_ms*/5000);
		if (error)
			return (-1);
		error = mpt_read_cur_cfg_page(mpt, /*PageAddress*/0,
					      &tmp.Header, sizeof(tmp),
					      /*sleep_ok*/FALSE,
					      /*timeout_ms*/5000);
		if (error)
			return (-1);
		if (tmp.Configuration != pp1val) {
			mpt_prt(mpt,
			    "failed to reset SPI Port Page 1 Config value\n");
			return (-1);
		}
		mpt->mpt_port_page1 = tmp;
	}

	for (i = 0; i < 16; i++) {
		CONFIG_PAGE_SCSI_DEVICE_1 tmp;
		tmp = mpt->mpt_dev_page1[i];
		tmp.RequestedParameters = 0;
		tmp.Configuration = 0;
		mpt_lprt(mpt, MPT_PRT_DEBUG,
			 "Set Tgt %d SPI DevicePage 1 values to %x 0 %x\n",
			 i, tmp.RequestedParameters, tmp.Configuration);
		error = mpt_write_cur_cfg_page(mpt, /*PageAddress*/i,
					       &tmp.Header, sizeof(tmp),
					       /*sleep_ok*/FALSE,
					       /*timeout_ms*/5000);
		if (error)
			return (-1);
		error = mpt_read_cur_cfg_page(mpt, /*PageAddress*/i,
					      &tmp.Header, sizeof(tmp),
					      /*sleep_ok*/FALSE,
					      /*timeout_ms*/5000);
		if (error)
			return (-1);
		mpt->mpt_dev_page1[i] = tmp;
		mpt_lprt(mpt, MPT_PRT_DEBUG,
			 "SPI Tgt %d Page 1: RParm %x Configuration %x\n", i,
			 mpt->mpt_dev_page1[i].RequestedParameters,
			 mpt->mpt_dev_page1[i].Configuration);
	}
	return (0);
}

/*
 * Enable IOC port
 */
static int
mpt_send_port_enable(struct mpt_softc *mpt, int port)
{
	request_t	*req;
	MSG_PORT_ENABLE *enable_req;
	int		 error;

	req = mpt_get_request(mpt, /*sleep_ok*/FALSE);
	if (req == NULL)
		return (-1);

	enable_req = req->req_vbuf;
	bzero(enable_req, sizeof *enable_req);

	enable_req->Function   = MPI_FUNCTION_PORT_ENABLE;
	enable_req->MsgContext = htole32(req->index | MPT_REPLY_HANDLER_CONFIG);
	enable_req->PortNumber = port;

	mpt_check_doorbell(mpt);
	mpt_lprt(mpt, MPT_PRT_DEBUG, "enabling port %d\n", port);

	mpt_send_cmd(mpt, req);
	error = mpt_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE,
	    /*sleep_ok*/FALSE, /*time_ms*/500);
	if (error != 0) {
		mpt_prt(mpt, "port enable timed out");
		return (-1);
	}
	mpt_free_request(mpt, req);
	return (0);
}

/*
 * Enable/Disable asynchronous event reporting.
 *
 * NB: this is the first command we send via shared memory
 * instead of the handshake register.
 */
static int
mpt_send_event_request(struct mpt_softc *mpt, int onoff)
{
	request_t *req;
	MSG_EVENT_NOTIFY *enable_req;

	req = mpt_get_request(mpt, /*sleep_ok*/FALSE);

	enable_req = req->req_vbuf;
	bzero(enable_req, sizeof *enable_req);

	enable_req->Function   = MPI_FUNCTION_EVENT_NOTIFICATION;
	enable_req->MsgContext = htole32(req->index | MPT_REPLY_HANDLER_EVENTS);
	enable_req->Switch     = onoff;

	mpt_check_doorbell(mpt);
	mpt_lprt(mpt, MPT_PRT_DEBUG,
		 "%sabling async events\n", onoff ? "en" : "dis");
	mpt_send_cmd(mpt, req);

	return (0);
}

/*
 * Un-mask the interupts on the chip.
 */
void
mpt_enable_ints(struct mpt_softc *mpt)
{
	/* Unmask every thing except door bell int */
	mpt_write(mpt, MPT_OFFSET_INTR_MASK, MPT_INTR_DB_MASK);
}

/*
 * Mask the interupts on the chip.
 */
void
mpt_disable_ints(struct mpt_softc *mpt)
{
	/* Mask all interrupts */
	mpt_write(mpt, MPT_OFFSET_INTR_MASK, 
	    MPT_INTR_REPLY_MASK | MPT_INTR_DB_MASK);
}

static void
mpt_sysctl_attach(struct mpt_softc *mpt)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(mpt->dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(mpt->dev);

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, 
		       "debug", CTLFLAG_RW, &mpt->verbose, 0,
		       "Debugging/Verbose level");
}

int
mpt_attach(struct mpt_softc *mpt)
{
	int i;

	for (i = 0; i < MPT_MAX_PERSONALITIES; i++) {
		struct mpt_personality *pers;
		int error;

		pers = mpt_personalities[i];
		if (pers == NULL)
			continue;

		if (pers->probe(mpt) == 0) {
			error = pers->attach(mpt);
			if (error != 0) {
				mpt_detach(mpt);
				return (error);
			}
			mpt->mpt_pers_mask |= (0x1 << pers->id);
			pers->use_count++;
		}
	}
	return (0);
}

int
mpt_shutdown(struct mpt_softc *mpt)
{
	struct mpt_personality *pers;

	MPT_PERS_FOREACH_REVERSE(mpt, pers)
		pers->shutdown(mpt);

	mpt_reset(mpt, /*reinit*/FALSE);
	return (0);
}

int
mpt_detach(struct mpt_softc *mpt)
{
	struct mpt_personality *pers;

	MPT_PERS_FOREACH_REVERSE(mpt, pers) {
		pers->detach(mpt);
		mpt->mpt_pers_mask &= ~(0x1 << pers->id);
		pers->use_count--;
	}

	return (0);
}

int
mpt_core_load(struct mpt_personality *pers)
{
	int i;

	/*
	 * Setup core handlers and insert the default handler
	 * into all "empty slots".
	 */
	for (i = 0; i < MPT_NUM_REPLY_HANDLERS; i++)
		mpt_reply_handlers[i] = mpt_default_reply_handler;

	mpt_reply_handlers[MPT_CBI(MPT_REPLY_HANDLER_EVENTS)] =
	    mpt_event_reply_handler;
	mpt_reply_handlers[MPT_CBI(MPT_REPLY_HANDLER_CONFIG)] =
	    mpt_config_reply_handler;
	mpt_reply_handlers[MPT_CBI(MPT_REPLY_HANDLER_HANDSHAKE)] =
	    mpt_handshake_reply_handler;

	return (0);
}

/*
 * Initialize per-instance driver data and perform
 * initial controller configuration.
 */
int
mpt_core_attach(struct mpt_softc *mpt)
{
        int val;
	int error;

	LIST_INIT(&mpt->ack_frames);

	/* Put all request buffers on the free list */
	TAILQ_INIT(&mpt->request_pending_list);
	TAILQ_INIT(&mpt->request_free_list);
	for (val = 0; val < MPT_MAX_REQUESTS(mpt); val++)
		mpt_free_request(mpt, &mpt->request_pool[val]);

	mpt_sysctl_attach(mpt);

	mpt_lprt(mpt, MPT_PRT_DEBUG, "doorbell req = %s\n",
		 mpt_ioc_diag(mpt_read(mpt, MPT_OFFSET_DOORBELL)));

	error = mpt_configure_ioc(mpt);

	return (error);
}

void
mpt_core_shutdown(struct mpt_softc *mpt)
{
}

void
mpt_core_detach(struct mpt_softc *mpt)
{
}

int
mpt_core_unload(struct mpt_personality *pers)
{
	/* Unload is always successfull. */
	return (0);
}

#define FW_UPLOAD_REQ_SIZE				\
	(sizeof(MSG_FW_UPLOAD) - sizeof(SGE_MPI_UNION)	\
       + sizeof(FW_UPLOAD_TCSGE) + sizeof(SGE_SIMPLE32))

static int
mpt_upload_fw(struct mpt_softc *mpt)
{
	uint8_t fw_req_buf[FW_UPLOAD_REQ_SIZE];
	MSG_FW_UPLOAD_REPLY fw_reply;
	MSG_FW_UPLOAD *fw_req;
	FW_UPLOAD_TCSGE *tsge;
	SGE_SIMPLE32 *sge;
	uint32_t flags;
	int error;
	
	memset(&fw_req_buf, 0, sizeof(fw_req_buf));
	fw_req = (MSG_FW_UPLOAD *)fw_req_buf;
	fw_req->ImageType = MPI_FW_UPLOAD_ITYPE_FW_IOC_MEM;
	fw_req->Function = MPI_FUNCTION_FW_UPLOAD;
	fw_req->MsgContext = htole32(MPT_REPLY_HANDLER_HANDSHAKE);
	tsge = (FW_UPLOAD_TCSGE *)&fw_req->SGL;
	tsge->DetailsLength = 12;
	tsge->Flags = MPI_SGE_FLAGS_TRANSACTION_ELEMENT;
	tsge->ImageSize = htole32(mpt->fw_image_size);
	sge = (SGE_SIMPLE32 *)(tsge + 1);
	flags = (MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER
	      | MPI_SGE_FLAGS_END_OF_LIST | MPI_SGE_FLAGS_SIMPLE_ELEMENT
	      | MPI_SGE_FLAGS_32_BIT_ADDRESSING | MPI_SGE_FLAGS_IOC_TO_HOST);
	flags <<= MPI_SGE_FLAGS_SHIFT;
	sge->FlagsLength = htole32(flags | mpt->fw_image_size);
	sge->Address = htole32(mpt->fw_phys);
	error = mpt_send_handshake_cmd(mpt, sizeof(fw_req_buf), &fw_req_buf);
	if (error)
		return(error);
	error = mpt_recv_handshake_reply(mpt, sizeof(fw_reply), &fw_reply);
	return (error);
}

static void
mpt_diag_outsl(struct mpt_softc *mpt, uint32_t addr,
	       uint32_t *data, bus_size_t len)
{
	uint32_t *data_end;

	data_end = data + (roundup2(len, sizeof(uint32_t)) / 4);
	mpt_pio_write(mpt, MPT_OFFSET_DIAG_ADDR, addr);
	while (data != data_end) {
		mpt_pio_write(mpt, MPT_OFFSET_DIAG_DATA, *data);
		data++;
	}
}

static int
mpt_download_fw(struct mpt_softc *mpt)
{
	MpiFwHeader_t *fw_hdr;
	int error;
	uint32_t ext_offset;
	uint32_t data;

	mpt_prt(mpt, "Downloading Firmware - Image Size %d\n",
		mpt->fw_image_size);

	error = mpt_enable_diag_mode(mpt);
	if (error != 0) {
		mpt_prt(mpt, "Could not enter diagnostic mode!\n");
		return (EIO);
	}

	mpt_write(mpt, MPT_OFFSET_DIAGNOSTIC,
		  MPI_DIAG_RW_ENABLE|MPI_DIAG_DISABLE_ARM);

	fw_hdr = (MpiFwHeader_t *)mpt->fw_image;
	mpt_diag_outsl(mpt, fw_hdr->LoadStartAddress, (uint32_t*)fw_hdr,
		       fw_hdr->ImageSize);

	ext_offset = fw_hdr->NextImageHeaderOffset;
	while (ext_offset != 0) {
		MpiExtImageHeader_t *ext;

		ext = (MpiExtImageHeader_t *)((uintptr_t)fw_hdr + ext_offset);
		ext_offset = ext->NextImageHeaderOffset;

		mpt_diag_outsl(mpt, ext->LoadStartAddress, (uint32_t*)ext,
			       ext->ImageSize);
	}

	/* Setup the address to jump to on reset. */
	mpt_pio_write(mpt, MPT_OFFSET_DIAG_ADDR, fw_hdr->IopResetRegAddr);
	mpt_pio_write(mpt, MPT_OFFSET_DIAG_DATA, fw_hdr->IopResetVectorValue);

	/*
	 * The controller sets the "flash bad" status after attempting
	 * to auto-boot from flash.  Clear the status so that the controller
	 * will continue the boot process with our newly installed firmware.
	 */
	mpt_pio_write(mpt, MPT_OFFSET_DIAG_ADDR, MPT_DIAG_MEM_CFG_BASE);
	data = mpt_pio_read(mpt, MPT_OFFSET_DIAG_DATA) | MPT_DIAG_MEM_CFG_BADFL;
	mpt_pio_write(mpt, MPT_OFFSET_DIAG_ADDR, MPT_DIAG_MEM_CFG_BASE);
	mpt_pio_write(mpt, MPT_OFFSET_DIAG_DATA, data);

	/*
	 * Re-enable the processor and clear the boot halt flag.
	 */
	data = mpt_read(mpt, MPT_OFFSET_DIAGNOSTIC);
	data &= ~(MPI_DIAG_PREVENT_IOC_BOOT|MPI_DIAG_DISABLE_ARM);
	mpt_write(mpt, MPT_OFFSET_DIAGNOSTIC, data);

	mpt_disable_diag_mode(mpt);
	return (0);
}

/*
 * Allocate/Initialize data structures for the controller.  Called
 * once at instance startup.
 */
static int
mpt_configure_ioc(struct mpt_softc *mpt)
{
        MSG_PORT_FACTS_REPLY pfp;
        MSG_IOC_FACTS_REPLY facts;
	int try;
	int needreset;

	needreset = 0;
	for (try = 0; try < MPT_MAX_TRYS; try++) {

		/*
		 * No need to reset if the IOC is already in the READY state.
		 *
		 * Force reset if initialization failed previously.
		 * Note that a hard_reset of the second channel of a '929
		 * will stop operation of the first channel.  Hopefully, if the
		 * first channel is ok, the second will not require a hard 
		 * reset.
		 */
		if (needreset || (mpt_rd_db(mpt) & MPT_DB_STATE_MASK) !=
		    MPT_DB_STATE_READY) {
			if (mpt_reset(mpt, /*reinit*/FALSE) != MPT_OK)
				continue;
		}
		needreset = 0;

		if (mpt_get_iocfacts(mpt, &facts) != MPT_OK) {
			mpt_prt(mpt, "mpt_get_iocfacts failed\n");
			needreset = 1;
			continue;
		}

		mpt->mpt_global_credits = le16toh(facts.GlobalCredits);
		mpt->request_frame_size = le16toh(facts.RequestFrameSize);
		mpt_prt(mpt, "MPI Version=%d.%d.%d.%d\n",
			    le16toh(facts.MsgVersion) >> 8,
			    le16toh(facts.MsgVersion) & 0xFF,
			    le16toh(facts.HeaderVersion) >> 8,
			    le16toh(facts.HeaderVersion) & 0xFF);
		mpt_lprt(mpt, MPT_PRT_DEBUG,
			 "MsgLength=%u IOCNumber = %d\n",
			 facts.MsgLength, facts.IOCNumber);
		mpt_lprt(mpt, MPT_PRT_DEBUG,
			 "IOCFACTS: GlobalCredits=%d BlockSize=%u "
			 "Request Frame Size %u\n", mpt->mpt_global_credits,
			 facts.BlockSize * 8, mpt->request_frame_size * 8);
		mpt_lprt(mpt, MPT_PRT_DEBUG,
			 "IOCFACTS: Num Ports %d, FWImageSize %d, "
			 "Flags=%#x\n", facts.NumberOfPorts,
			 le32toh(facts.FWImageSize), facts.Flags);

		if ((facts.Flags & MPI_IOCFACTS_FLAGS_FW_DOWNLOAD_BOOT) != 0) {
			struct mpt_map_info mi;
			int error;

			/*
			 * In some configurations, the IOC's firmware is
			 * stored in a shared piece of system NVRAM that
			 * is only accessable via the BIOS.  In this
			 * case, the firmware keeps a copy of firmware in
			 * RAM until the OS driver retrieves it.  Once
			 * retrieved, we are responsible for re-downloading
			 * the firmware after any hard-reset.
			 */
			mpt->fw_image_size = le32toh(facts.FWImageSize);
			error = mpt_dma_tag_create(mpt, mpt->parent_dmat,
			    /*alignment*/1, /*boundary*/0,
			    /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
			    /*highaddr*/BUS_SPACE_MAXADDR, /*filter*/NULL,
			    /*filterarg*/NULL, mpt->fw_image_size,
			    /*nsegments*/1, /*maxsegsz*/mpt->fw_image_size,
			    /*flags*/0, &mpt->fw_dmat);
			if (error != 0) {
				mpt_prt(mpt, "cannot create fw dma tag\n");
				return (ENOMEM);
			}
			error = bus_dmamem_alloc(mpt->fw_dmat,
			    (void **)&mpt->fw_image, BUS_DMA_NOWAIT,
			    &mpt->fw_dmap);
			if (error != 0) {
				mpt_prt(mpt, "cannot allocate fw mem.\n");
				bus_dma_tag_destroy(mpt->fw_dmat);
				return (ENOMEM);
			}
			mi.mpt = mpt;
			mi.error = 0;
			bus_dmamap_load(mpt->fw_dmat, mpt->fw_dmap,
			    mpt->fw_image, mpt->fw_image_size, mpt_map_rquest,
			    &mi, 0);
			mpt->fw_phys = mi.phys;

			error = mpt_upload_fw(mpt);
			if (error != 0) {
				mpt_prt(mpt, "fw upload failed.\n");
				bus_dmamap_unload(mpt->fw_dmat, mpt->fw_dmap);
				bus_dmamem_free(mpt->fw_dmat, mpt->fw_image,
				    mpt->fw_dmap);
				bus_dma_tag_destroy(mpt->fw_dmat);
				mpt->fw_image = NULL;
				return (EIO);
			}
		}

		if (mpt_get_portfacts(mpt, &pfp) != MPT_OK) {
			mpt_prt(mpt, "mpt_get_portfacts failed\n");
			needreset = 1;
			continue;
		}

		mpt_lprt(mpt, MPT_PRT_DEBUG,
			 "PORTFACTS: Type %x PFlags %x IID %d MaxDev %d\n",
			 pfp.PortType, pfp.ProtocolFlags, pfp.PortSCSIID,
			 pfp.MaxDevices);

		mpt->mpt_port_type = pfp.PortType;
		mpt->mpt_proto_flags = pfp.ProtocolFlags;
		if (pfp.PortType != MPI_PORTFACTS_PORTTYPE_SCSI &&
		    pfp.PortType != MPI_PORTFACTS_PORTTYPE_FC) {
			mpt_prt(mpt, "Unsupported Port Type (%x)\n",
			    pfp.PortType);
			return (ENXIO);
		}
		if (!(pfp.ProtocolFlags & MPI_PORTFACTS_PROTOCOL_INITIATOR)) {
			mpt_prt(mpt, "initiator role unsupported\n");
			return (ENXIO);
		}
		if (pfp.PortType == MPI_PORTFACTS_PORTTYPE_FC) {
			mpt->is_fc = 1;
		} else {
			mpt->is_fc = 0;
		}
		mpt->mpt_ini_id = pfp.PortSCSIID;

		if (mpt_enable_ioc(mpt) != 0) {
			mpt_prt(mpt, "Unable to initialize IOC\n");
			return (ENXIO);
		}

		/*
		 * Read and set up initial configuration information
		 * (IOC and SPI only for now)
		 *
		 * XXX Should figure out what "personalities" are
		 * available and defer all initialization junk to
		 * them.
		 */
		mpt_read_config_info_ioc(mpt);

		if (mpt->is_fc == 0) {
			if (mpt_read_config_info_spi(mpt)) {
				return (EIO);
			}
			if (mpt_set_initial_config_spi(mpt)) {
				return (EIO);
			}
		}

		/* Everything worked */
		break;
	}

	if (try >= MPT_MAX_TRYS) {
		mpt_prt(mpt, "failed to initialize IOC");
		return (EIO);
	}

	mpt_lprt(mpt, MPT_PRT_DEBUG, "enabling interrupts\n");

	mpt_enable_ints(mpt);
	return (0);
}

static int
mpt_enable_ioc(struct mpt_softc *mpt)
{
	uint32_t pptr;
	int val;

	if (mpt_send_ioc_init(mpt, MPT_DB_INIT_HOST) != MPT_OK) {
		mpt_prt(mpt, "mpt_send_ioc_init failed\n");
		return (EIO);
	}

	mpt_lprt(mpt, MPT_PRT_DEBUG, "mpt_send_ioc_init ok\n");

	if (mpt_wait_state(mpt, MPT_DB_STATE_RUNNING) != MPT_OK) {
		mpt_prt(mpt, "IOC failed to go to run state\n");
		return (ENXIO);
	}
	mpt_lprt(mpt, MPT_PRT_DEBUG, "IOC now at RUNSTATE");

	/*
	 * Give it reply buffers
	 *
	 * Do *not* exceed global credits.
	 */
	for (val = 0, pptr = mpt->reply_phys; 
	    (pptr + MPT_REPLY_SIZE) < (mpt->reply_phys + PAGE_SIZE); 
	     pptr += MPT_REPLY_SIZE) {
		mpt_free_reply(mpt, pptr);
		if (++val == mpt->mpt_global_credits - 1)
			break;
	}

	/*
	 * Enable asynchronous event reporting
	 */
	mpt_send_event_request(mpt, 1);

	/*
	 * Now enable the port
	 */
	if (mpt_send_port_enable(mpt, 0) != MPT_OK) {
		mpt_prt(mpt, "failed to enable port 0\n");
		return (ENXIO);
	}

	mpt_lprt(mpt, MPT_PRT_DEBUG, "enabled port 0\n");

	return (0);
}
