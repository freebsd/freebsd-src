/* $FreeBSD$ */
/*
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
 */
/*
 * Additional Copyright (c) 2002 by Matthew Jacob under same license.
 */

#include <dev/mpt/mpt_freebsd.h>

#define MPT_MAX_TRYS 3
#define MPT_MAX_WAIT 300000

static int maxwait_ack = 0;
static int maxwait_int = 0;
static int maxwait_state = 0;

static __inline u_int32_t mpt_rd_db(mpt_softc_t *mpt);
static __inline  u_int32_t mpt_rd_intr(mpt_softc_t *mpt);

static __inline u_int32_t
mpt_rd_db(mpt_softc_t *mpt)
{
	return mpt_read(mpt, MPT_OFFSET_DOORBELL);
}

static __inline u_int32_t
mpt_rd_intr(mpt_softc_t *mpt)
{
	return mpt_read(mpt, MPT_OFFSET_INTR_STATUS);
}

/* Busy wait for a door bell to be read by IOC */
static int
mpt_wait_db_ack(mpt_softc_t *mpt)
{
	int i;
	for (i=0; i < MPT_MAX_WAIT; i++) {
		if (!MPT_DB_IS_BUSY(mpt_rd_intr(mpt))) {
			maxwait_ack = i > maxwait_ack ? i : maxwait_ack;
			return MPT_OK;
		}

		DELAY(100);
	}
	return MPT_FAIL;
}

/* Busy wait for a door bell interrupt */
static int
mpt_wait_db_int(mpt_softc_t *mpt)
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
mpt_check_doorbell(mpt_softc_t *mpt)
{
	u_int32_t db = mpt_rd_db(mpt);
	if (MPT_STATE(db) != MPT_DB_STATE_RUNNING) {
		device_printf(mpt->dev, "Device not running!\n");
		mpt_print_db(db);
	}
}

/* Wait for IOC to transition to a give state */
static int
mpt_wait_state(mpt_softc_t *mpt, enum DB_STATE_BITS state)
{
	int i;

	for (i = 0; i < MPT_MAX_WAIT; i++) {
		u_int32_t db = mpt_rd_db(mpt);
		if (MPT_STATE(db) == state) {
			maxwait_state = i > maxwait_state ? i : maxwait_state;
			return (MPT_OK);
		}
		DELAY(100);
	}
	return (MPT_FAIL);
}


/* Issue the reset COMMAND to the IOC */
int
mpt_soft_reset(mpt_softc_t *mpt)
{
	if (mpt->verbose) {
		device_printf(mpt->dev,"soft reset\n");
	}

	/* Have to use hard reset if we are not in Running state */
	if (MPT_STATE(mpt_rd_db(mpt)) != MPT_DB_STATE_RUNNING) {
		device_printf(mpt->dev,
		    "soft reset failed: device not running\n");
		return MPT_FAIL;
	}

	/* If door bell is in use we don't have a chance of getting
	 * a word in since the IOC probably crashed in message
	 * processing. So don't waste our time.
	 */
	if (MPT_DB_IS_IN_USE(mpt_rd_db(mpt))) {
		device_printf(mpt->dev, "soft reset failed: doorbell wedged\n");
		return MPT_FAIL;
	}

	/* Send the reset request to the IOC */
	mpt_write(mpt, MPT_OFFSET_DOORBELL,
	    MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET << MPI_DOORBELL_FUNCTION_SHIFT);
	if (mpt_wait_db_ack(mpt) != MPT_OK) {
		device_printf(mpt->dev, "soft reset failed: ack timeout\n");
		return MPT_FAIL;
	}

	/* Wait for the IOC to reload and come out of reset state */
	if (mpt_wait_state(mpt, MPT_DB_STATE_READY) != MPT_OK) {
		device_printf(mpt->dev,
		    "soft reset failed: device did not start running\n");
		return MPT_FAIL;
	}

	return MPT_OK;
}

/* This is a magic diagnostic reset that resets all the ARM
 * processors in the chip. 
 */
void
mpt_hard_reset(mpt_softc_t *mpt)
{
	/* This extra read comes for the Linux source
	 * released by LSI. It's function is undocumented!
	 */
	if (mpt->verbose) {
		device_printf(mpt->dev, "hard reset\n");
	}
	mpt_read(mpt, MPT_OFFSET_FUBAR);

	/* Enable diagnostic registers */
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_1);
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_2);
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_3);
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_4);
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_5);

	/* Diag. port is now active so we can now hit the reset bit */
	mpt_write(mpt, MPT_OFFSET_DIAGNOSTIC, MPT_DIAG_RESET_IOC);

	DELAY(10000);

	/* Disable Diagnostic Register */
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, 0xFF);

	/* Restore the config register values */
	/*   Hard resets are known to screw up the BAR for diagnostic
	     memory accesses (Mem1). */
	mpt_set_config_regs(mpt);
	if (mpt->mpt2 != NULL) {
		mpt_set_config_regs(mpt->mpt2);
	}

	/* Note that if there is no valid firmware to run, the doorbell will
	   remain in the reset state (0x00000000) */
}

/*
 * Reset the IOC when needed. Try software command first then if needed
 * poke at the magic diagnostic reset. Note that a hard reset resets
 * *both* IOCs on dual function chips (FC929 && LSI1030) as well as
 * fouls up the PCI configuration registers.
 */
int
mpt_reset(mpt_softc_t *mpt)
{
	int ret;

	/* Try a soft reset */
	if ((ret = mpt_soft_reset(mpt)) != MPT_OK) {
		/* Failed; do a hard reset */
		mpt_hard_reset(mpt);

		/* Wait for the IOC to reload and come out of reset state */
		ret = mpt_wait_state(mpt, MPT_DB_STATE_READY);
		if (ret != MPT_OK) {
			device_printf(mpt->dev, "failed to reset device\n");
		}
	}

	return ret;
}

/* Return a command buffer to the free queue */
void
mpt_free_request(mpt_softc_t *mpt, request_t *req)
{
	if (req == NULL || req != &mpt->requests[req->index]) {
		panic("mpt_free_request bad req ptr\n");
		return;
	}
	req->ccb = NULL;
	req->debug = REQ_FREE;
	SLIST_INSERT_HEAD(&mpt->request_free_list, req, link);
}

/* Get a command buffer from the free queue */
request_t *
mpt_get_request(mpt_softc_t *mpt)
{
	request_t *req;
	req = SLIST_FIRST(&mpt->request_free_list);
	if (req != NULL) {
		if (req != &mpt->requests[req->index]) {
			panic("mpt_get_request: corrupted request free list\n");
		}
		if (req->ccb != NULL) {
			panic("mpt_get_request: corrupted request free list (ccb)\n");
		}
		SLIST_REMOVE_HEAD(&mpt->request_free_list, link);
		req->debug = REQ_IN_PROGRESS;
	}
	return req;
}

/* Pass the command to the IOC */
void
mpt_send_cmd(mpt_softc_t *mpt, request_t *req)
{
	req->sequence = mpt->sequence++;
	if (mpt->verbose > 1) {
		u_int32_t *pReq;
		pReq = req->req_vbuf;
		device_printf(mpt->dev, "Send Request %d (0x%x):\n",
		    req->index, req->req_pbuf);
		device_printf(mpt->dev, "%08X %08X %08X %08X\n",
		    pReq[0], pReq[1], pReq[2], pReq[3]);
		device_printf(mpt->dev, "%08X %08X %08X %08X\n",
		    pReq[4], pReq[5], pReq[6], pReq[7]);
		device_printf(mpt->dev, "%08X %08X %08X %08X\n",
		    pReq[8], pReq[9], pReq[10], pReq[11]);
		device_printf(mpt->dev, "%08X %08X %08X %08X\n",
		    pReq[12], pReq[13], pReq[14], pReq[15]);
	}
	bus_dmamap_sync(mpt->request_dmat, mpt->request_dmap,
	   BUS_DMASYNC_PREWRITE);
	req->debug = REQ_ON_CHIP;
	mpt_write(mpt, MPT_OFFSET_REQUEST_Q, (u_int32_t) req->req_pbuf);
}

/*
 * Give the reply buffer back to the IOC after we have
 * finished processing it.
 */
void
mpt_free_reply(mpt_softc_t *mpt, u_int32_t ptr)
{
     mpt_write(mpt, MPT_OFFSET_REPLY_Q, ptr);
}

/* Get a reply from the IOC */
u_int32_t
mpt_pop_reply_queue(mpt_softc_t *mpt)
{
     return mpt_read(mpt, MPT_OFFSET_REPLY_Q);
}

/*
 * Send a command to the IOC via the handshake register.
 *
 * Only done at initialization time and for certain unusual
 * commands such as device/bus reset as specified by LSI.
 */
int
mpt_send_handshake_cmd(mpt_softc_t *mpt, size_t len, void *cmd)
{
	int i;
	u_int32_t data, *data32;

	/* Check condition of the IOC */
	data = mpt_rd_db(mpt);
	if (((MPT_STATE(data) != MPT_DB_STATE_READY)	&&
	     (MPT_STATE(data) != MPT_DB_STATE_RUNNING)	&&
	     (MPT_STATE(data) != MPT_DB_STATE_FAULT))	||
	    (  MPT_DB_IS_IN_USE(data) )) {
		device_printf(mpt->dev,
		    "handshake aborted due to invalid doorbell state\n");
		mpt_print_db(data);
		return(EBUSY);
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
		device_printf(mpt->dev, "mpt_send_handshake_cmd timeout1!\n");
		return ETIMEDOUT;
	}

	/* Clear the interrupt */
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	if (mpt_wait_db_ack(mpt) != MPT_OK) {
		device_printf(mpt->dev, "mpt_send_handshake_cmd timeout2!\n");
		return ETIMEDOUT;
	}

	/* Send the command */
	for (i = 0; i < len; i++) {
		mpt_write(mpt, MPT_OFFSET_DOORBELL, *data32++);
		if (mpt_wait_db_ack(mpt) != MPT_OK) {
			device_printf(mpt->dev,
			    "mpt_send_handshake_cmd timeout! index = %d\n", i);
			return ETIMEDOUT;
		}
	}
	return MPT_OK;
}

/* Get the response from the handshake register */
int
mpt_recv_handshake_reply(mpt_softc_t *mpt, size_t reply_len, void *reply)
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
		device_printf(mpt->dev, "mpt_recv_handshake_cmd timeout1!\n");
		return ETIMEDOUT;
	}
	*data16++ = mpt_read(mpt, MPT_OFFSET_DOORBELL) & MPT_DB_DATA_MASK;
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	/* Get Second Word */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		device_printf(mpt->dev, "mpt_recv_handshake_cmd timeout2!\n");
		return ETIMEDOUT;
	}
	*data16++ = mpt_read(mpt, MPT_OFFSET_DOORBELL) & MPT_DB_DATA_MASK;
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	/* With the second word, we can now look at the length */
	if (mpt->verbose > 1 && ((reply_len >> 1) != hdr->MsgLength)) {
		device_printf(mpt->dev,
			"reply length does not match message length: "
			"got 0x%02x, expected 0x%02x\n",
			hdr->MsgLength << 2, reply_len << 1);
	}

	/* Get rest of the reply; but don't overflow the provided buffer */
	left = (hdr->MsgLength << 1) - 2;
	reply_left =  reply_len - 2;
	while (left--) {
		u_int16_t datum;

		if (mpt_wait_db_int(mpt) != MPT_OK) {
			device_printf(mpt->dev,
			    "mpt_recv_handshake_cmd timeout3!\n");
			return ETIMEDOUT;
		}
		datum = mpt_read(mpt, MPT_OFFSET_DOORBELL);

		if (reply_left-- > 0)
			*data16++ = datum & MPT_DB_DATA_MASK;

		mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);
	}

	/* One more wait & clear at the end */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		device_printf(mpt->dev, "mpt_recv_handshake_cmd timeout4!\n");
		return ETIMEDOUT;
	}
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	if ((hdr->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		if (mpt->verbose > 1)
			mpt_print_reply(hdr);
		return (MPT_FAIL | hdr->IOCStatus);
	}

	return (0);
}

static int
mpt_get_iocfacts(mpt_softc_t *mpt, MSG_IOC_FACTS_REPLY *freplp)
{
	MSG_IOC_FACTS f_req;
	int error;
	
	bzero(&f_req, sizeof f_req);
	f_req.Function = MPI_FUNCTION_IOC_FACTS;
	f_req.MsgContext =  0x12071942;
	error = mpt_send_handshake_cmd(mpt, sizeof f_req, &f_req);
	if (error)
		return(error);
	error = mpt_recv_handshake_reply(mpt, sizeof (*freplp), freplp);
	return (error);
}

static int
mpt_get_portfacts(mpt_softc_t *mpt, MSG_PORT_FACTS_REPLY *freplp)
{
	MSG_PORT_FACTS f_req;
	int error;
	
	/* XXX: Only getting PORT FACTS for Port 0 */
	bzero(&f_req, sizeof f_req);
	f_req.Function = MPI_FUNCTION_PORT_FACTS;
	f_req.MsgContext =  0x12071943;
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
mpt_send_ioc_init(mpt_softc_t *mpt, u_int32_t who)
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
	init.MsgContext = 0x12071941;

	if ((error = mpt_send_handshake_cmd(mpt, sizeof init, &init)) != 0) {
		return(error);
	}

	error = mpt_recv_handshake_reply(mpt, sizeof reply, &reply);
	return (error);
}


/*
 * Utiltity routine to read configuration headers and pages
 */

static int
mpt_read_cfg_header(mpt_softc_t *, int, int, int, fCONFIG_PAGE_HEADER *);
static int
mpt_read_cfg_page(mpt_softc_t *, int, fCONFIG_PAGE_HEADER *);
static int
mpt_write_cfg_page(mpt_softc_t *, int, fCONFIG_PAGE_HEADER *);

static int
mpt_read_cfg_header(mpt_softc_t *mpt, int PageType, int PageNumber,
    int PageAddress, fCONFIG_PAGE_HEADER *rslt)
{
	int count;
	request_t *req;
	MSG_CONFIG *cfgp;
	MSG_CONFIG_REPLY *reply;

	req = mpt_get_request(mpt);

	cfgp = req->req_vbuf;
	bzero(cfgp, sizeof *cfgp);

	cfgp->Action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfgp->Function = MPI_FUNCTION_CONFIG;
	cfgp->Header.PageNumber = (U8) PageNumber;
	cfgp->Header.PageType = (U8) PageType;
	cfgp->PageAddress = PageAddress;
	MPI_pSGE_SET_FLAGS(((SGE_SIMPLE32 *) &cfgp->PageBufferSGE),
	    (MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER |
	    MPI_SGE_FLAGS_SIMPLE_ELEMENT | MPI_SGE_FLAGS_END_OF_LIST));
	cfgp->MsgContext = req->index | 0x80000000;

	mpt_check_doorbell(mpt);
	mpt_send_cmd(mpt, req);
	count = 0;
	do {
		DELAY(500);
		mpt_intr(mpt);
		if (++count == 1000) {
			device_printf(mpt->dev, "read_cfg_header timed out\n");
			return (-1);
		}
	} while (req->debug == REQ_ON_CHIP);

	reply = (MSG_CONFIG_REPLY *) MPT_REPLY_PTOV(mpt, req->sequence);
        if ((reply->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		device_printf(mpt->dev,
		    "mpt_read_cfg_header: Config Info Status %x\n",
		    reply->IOCStatus);
		return (-1);
	}
	bcopy(&reply->Header, rslt, sizeof (fCONFIG_PAGE_HEADER));
	mpt_free_reply(mpt, (req->sequence << 1));
	mpt_free_request(mpt, req);
	return (0);
}

#define	CFG_DATA_OFF	40

static int
mpt_read_cfg_page(mpt_softc_t *mpt, int PageAddress, fCONFIG_PAGE_HEADER *hdr)
{
	int count;
	request_t *req;
	SGE_SIMPLE32 *se;
	MSG_CONFIG *cfgp;
	size_t amt;
	MSG_CONFIG_REPLY *reply;

	req = mpt_get_request(mpt);

	cfgp = req->req_vbuf;
 	amt = (cfgp->Header.PageLength * sizeof (uint32_t));
	bzero(cfgp, sizeof *cfgp);
	cfgp->Action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	cfgp->Function = MPI_FUNCTION_CONFIG;
	cfgp->Header = *hdr;
	cfgp->Header.PageType &= MPI_CONFIG_PAGETYPE_MASK;
	cfgp->PageAddress = PageAddress;
	se = (SGE_SIMPLE32 *) &cfgp->PageBufferSGE;
	se->Address = req->req_pbuf + CFG_DATA_OFF;
	MPI_pSGE_SET_LENGTH(se, amt);
	MPI_pSGE_SET_FLAGS(se, (MPI_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER |
	    MPI_SGE_FLAGS_END_OF_LIST));

	cfgp->MsgContext = req->index | 0x80000000;

	mpt_check_doorbell(mpt);
	mpt_send_cmd(mpt, req);
	count = 0;
	do {
		DELAY(500);
		mpt_intr(mpt);
		if (++count == 1000) {
			device_printf(mpt->dev, "read_cfg_page timed out\n");
			return (-1);
		}
	} while (req->debug == REQ_ON_CHIP);

	reply = (MSG_CONFIG_REPLY *) MPT_REPLY_PTOV(mpt, req->sequence);
        if ((reply->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		device_printf(mpt->dev,
		    "mpt_read_cfg_page: Config Info Status %x\n",
		    reply->IOCStatus);
		return (-1);
	}
	mpt_free_reply(mpt, (req->sequence << 1));
	bus_dmamap_sync(mpt->request_dmat, mpt->request_dmap,
	    BUS_DMASYNC_POSTREAD);
	if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (fCONFIG_PAGE_SCSI_PORT_0);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 1) {
		amt = sizeof (fCONFIG_PAGE_SCSI_PORT_1);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 2) {
		amt = sizeof (fCONFIG_PAGE_SCSI_PORT_2);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_DEVICE  &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (fCONFIG_PAGE_SCSI_DEVICE_0);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_DEVICE  &&
	    cfgp->Header.PageNumber == 1) {
		amt = sizeof (fCONFIG_PAGE_SCSI_DEVICE_1);
	}
	bcopy(((caddr_t)req->req_vbuf)+CFG_DATA_OFF, hdr, amt);
	mpt_free_request(mpt, req);
	return (0);
}

static int
mpt_write_cfg_page(mpt_softc_t *mpt, int PageAddress, fCONFIG_PAGE_HEADER *hdr)
{
	int count, hdr_attr;
	request_t *req;
	SGE_SIMPLE32 *se;
	MSG_CONFIG *cfgp;
	size_t amt;
	MSG_CONFIG_REPLY *reply;

	req = mpt_get_request(mpt);

	cfgp = req->req_vbuf;
	bzero(cfgp, sizeof *cfgp);

	hdr_attr = hdr->PageType & MPI_CONFIG_PAGEATTR_MASK;
	if (hdr_attr != MPI_CONFIG_PAGEATTR_CHANGEABLE &&
	    hdr_attr != MPI_CONFIG_PAGEATTR_PERSISTENT) {
		device_printf(mpt->dev, "page type 0x%x not changeable\n",
		    hdr->PageType & MPI_CONFIG_PAGETYPE_MASK);
		return (-1);
	}
	hdr->PageType &= MPI_CONFIG_PAGETYPE_MASK;

 	amt = (cfgp->Header.PageLength * sizeof (uint32_t));
	cfgp->Action = MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	cfgp->Function = MPI_FUNCTION_CONFIG;
	cfgp->Header = *hdr;
	cfgp->PageAddress = PageAddress;

	se = (SGE_SIMPLE32 *) &cfgp->PageBufferSGE;
	se->Address = req->req_pbuf + CFG_DATA_OFF;
	MPI_pSGE_SET_LENGTH(se, amt);
	MPI_pSGE_SET_FLAGS(se, (MPI_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER |
	    MPI_SGE_FLAGS_END_OF_LIST | MPI_SGE_FLAGS_HOST_TO_IOC));

	cfgp->MsgContext = req->index | 0x80000000;

	if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (fCONFIG_PAGE_SCSI_PORT_0);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 1) {
		amt = sizeof (fCONFIG_PAGE_SCSI_PORT_1);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 2) {
		amt = sizeof (fCONFIG_PAGE_SCSI_PORT_2);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_DEVICE  &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (fCONFIG_PAGE_SCSI_DEVICE_0);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_DEVICE  &&
	    cfgp->Header.PageNumber == 1) {
		amt = sizeof (fCONFIG_PAGE_SCSI_DEVICE_1);
	}
	bcopy(hdr, ((caddr_t)req->req_vbuf)+CFG_DATA_OFF, amt);

	mpt_check_doorbell(mpt);
	mpt_send_cmd(mpt, req);
	count = 0;
	do {
		DELAY(500);
		mpt_intr(mpt);
		if (++count == 1000) {
			hdr->PageType |= hdr_attr;
			device_printf(mpt->dev,
			    "mpt_write_cfg_page timed out\n");
			return (-1);
		}
	} while (req->debug == REQ_ON_CHIP);

	reply = (MSG_CONFIG_REPLY *) MPT_REPLY_PTOV(mpt, req->sequence);
        if ((reply->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		device_printf(mpt->dev,
		    "mpt_write_cfg_page: Config Info Status %x\n",
		    reply->IOCStatus);
		return (-1);
	}
	mpt_free_reply(mpt, (req->sequence << 1));

	/*
	 * Restore stripped out attributes
	 */
	hdr->PageType |= hdr_attr;
	mpt_free_request(mpt, req);
	return (0);
}

/*
 * Read SCSI configuration information
 */
static int
mpt_read_config_info_spi(mpt_softc_t *mpt)
{
	int rv, i;

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_PORT, 0,
	    0, &mpt->mpt_port_page0.Header);
	if (rv) {
		return (-1);
	}
	if (mpt->verbose > 1) {
		device_printf(mpt->dev, "SPI Port Page 0 Header: %x %x %x %x\n",
		    mpt->mpt_port_page0.Header.PageVersion,
		    mpt->mpt_port_page0.Header.PageLength,
		    mpt->mpt_port_page0.Header.PageNumber,
		    mpt->mpt_port_page0.Header.PageType);
	}

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_PORT, 1,
	    0, &mpt->mpt_port_page1.Header);
	if (rv) {
		return (-1);
	}
	if (mpt->verbose > 1) {
		device_printf(mpt->dev, "SPI Port Page 1 Header: %x %x %x %x\n",
		    mpt->mpt_port_page1.Header.PageVersion,
		    mpt->mpt_port_page1.Header.PageLength,
		    mpt->mpt_port_page1.Header.PageNumber,
		    mpt->mpt_port_page1.Header.PageType);
	}

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_PORT, 2,
	    0, &mpt->mpt_port_page2.Header);
	if (rv) {
		return (-1);
	}

	if (mpt->verbose > 1) {
		device_printf(mpt->dev, "SPI Port Page 2 Header: %x %x %x %x\n",
		    mpt->mpt_port_page1.Header.PageVersion,
		    mpt->mpt_port_page1.Header.PageLength,
		    mpt->mpt_port_page1.Header.PageNumber,
		    mpt->mpt_port_page1.Header.PageType);
	}

	for (i = 0; i < 16; i++) {
		rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_DEVICE,
		    0, i, &mpt->mpt_dev_page0[i].Header);
		if (rv) {
			return (-1);
		}
		if (mpt->verbose > 1) {
			device_printf(mpt->dev,
			    "SPI Target %d Device Page 0 Header: %x %x %x %x\n",
			    i, mpt->mpt_dev_page0[i].Header.PageVersion,
			    mpt->mpt_dev_page0[i].Header.PageLength,
			    mpt->mpt_dev_page0[i].Header.PageNumber,
			    mpt->mpt_dev_page0[i].Header.PageType);
		}
		
		rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_DEVICE,
		    1, i, &mpt->mpt_dev_page1[i].Header);
		if (rv) {
			return (-1);
		}
		if (mpt->verbose > 1) {
			device_printf(mpt->dev,
			    "SPI Target %d Device Page 1 Header: %x %x %x %x\n",
			    i, mpt->mpt_dev_page1[i].Header.PageVersion,
			    mpt->mpt_dev_page1[i].Header.PageLength,
			    mpt->mpt_dev_page1[i].Header.PageNumber,
			    mpt->mpt_dev_page1[i].Header.PageType);
		}
	}

	/*
	 * At this point, we don't *have* to fail. As long as we have
	 * valid config header information, we can (barely) lurch
	 * along.
	 */

	rv = mpt_read_cfg_page(mpt, 0, &mpt->mpt_port_page0.Header);
	if (rv) {
		device_printf(mpt->dev, "failed to read SPI Port Page 0\n");
	} else if (mpt->verbose > 1) {
		device_printf(mpt->dev,
		    "SPI Port Page 0: Capabilities %x PhysicalInterface %x\n",
		    mpt->mpt_port_page0.Capabilities,
		    mpt->mpt_port_page0.PhysicalInterface);
	}

	rv = mpt_read_cfg_page(mpt, 0, &mpt->mpt_port_page1.Header);
	if (rv) {
		device_printf(mpt->dev, "failed to read SPI Port Page 1\n");
	} else if (mpt->verbose > 1) {
		device_printf(mpt->dev,
		    "SPI Port Page 1: Configuration %x OnBusTimerValue %x\n",
		    mpt->mpt_port_page1.Configuration,
		    mpt->mpt_port_page1.OnBusTimerValue);
	}

	rv = mpt_read_cfg_page(mpt, 0, &mpt->mpt_port_page2.Header);
	if (rv) {
		device_printf(mpt->dev, "failed to read SPI Port Page 2\n");
	} else if (mpt->verbose > 1) {
		device_printf(mpt->dev,
		    "SPI Port Page 2: Flags %x Settings %x\n",
		    mpt->mpt_port_page2.PortFlags,
		    mpt->mpt_port_page2.PortSettings);
		for (i = 0; i < 16; i++) {
			device_printf(mpt->dev,
		  	    "SPI Port Page 2 Tgt %d: timo %x SF %x Flags %x\n",
			    i, mpt->mpt_port_page2.DeviceSettings[i].Timeout,
			    mpt->mpt_port_page2.DeviceSettings[i].SyncFactor,
			    mpt->mpt_port_page2.DeviceSettings[i].DeviceFlags);
		}
	}

	for (i = 0; i < 16; i++) {
		rv = mpt_read_cfg_page(mpt, i, &mpt->mpt_dev_page0[i].Header);
		if (rv) {
			device_printf(mpt->dev,
			    "cannot read SPI Tgt %d Device Page 0\n", i);
			continue;
		}
		if (mpt->verbose > 1) {
			device_printf(mpt->dev,
			    "SPI Tgt %d Page 0: NParms %x Information %x\n",
			    i, mpt->mpt_dev_page0[i].NegotiatedParameters,
			    mpt->mpt_dev_page0[i].Information);
		}
		rv = mpt_read_cfg_page(mpt, i, &mpt->mpt_dev_page1[i].Header);
		if (rv) {
			device_printf(mpt->dev,
			    "cannot read SPI Tgt %d Device Page 1\n", i);
			continue;
		}
		if (mpt->verbose > 1) {
			device_printf(mpt->dev,
			    "SPI Tgt %d Page 1: RParms %x Configuration %x\n",
			    i, mpt->mpt_dev_page1[i].RequestedParameters,
			    mpt->mpt_dev_page1[i].Configuration);
		}
	}
	return (0);
}

/*
 * Validate SPI configuration information.
 *
 * In particular, validate SPI Port Page 1.
 */
static int
mpt_set_initial_config_spi(mpt_softc_t *mpt)
{
	int i, pp1val = ((1 << mpt->mpt_ini_id) << 16) | mpt->mpt_ini_id;

	if (mpt->mpt_port_page1.Configuration != pp1val) {
		fCONFIG_PAGE_SCSI_PORT_1 tmp;
		device_printf(mpt->dev,
		    "SPI Port Page 1 Config value bad (%x)- should be %x\n",
		    mpt->mpt_port_page1.Configuration, pp1val);
		tmp = mpt->mpt_port_page1;
		tmp.Configuration = pp1val;
		if (mpt_write_cfg_page(mpt, 0, &tmp.Header)) {
			return (-1);
		}
		if (mpt_read_cfg_page(mpt, 0, &tmp.Header)) {
			return (-1);
		}
		if (tmp.Configuration != pp1val) {
			device_printf(mpt->dev,
			    "failed to reset SPI Port Page 1 Config value\n");
			return (-1);
		}
		mpt->mpt_port_page1 = tmp;
	}

#if	1
	i = i;
#else
	for (i = 0; i < 16; i++) {
		fCONFIG_PAGE_SCSI_DEVICE_1 tmp;
		tmp = mpt->mpt_dev_page1[i];
		tmp.RequestedParameters = 0;
		tmp.Configuration = 0;
		if (mpt->verbose > 1) {
			device_printf(mpt->dev,
			    "Set Tgt %d SPI DevicePage 1 values to %x 0 %x\n",
			    i, tmp.RequestedParameters, tmp.Configuration);
		}
		if (mpt_write_cfg_page(mpt, i, &tmp.Header)) {
			return (-1);
		}
		if (mpt_read_cfg_page(mpt, i, &tmp.Header)) {
			return (-1);
		}
		mpt->mpt_dev_page1[i] = tmp;
		if (mpt->verbose > 1) {
			device_printf(mpt->dev,
		 	    "SPI Tgt %d Page 1: RParm %x Configuration %x\n", i,
			    mpt->mpt_dev_page1[i].RequestedParameters,
			    mpt->mpt_dev_page1[i].Configuration);
		}
	}
#endif
	return (0);
}

/*
 * Enable IOC port
 */
static int
mpt_send_port_enable(mpt_softc_t *mpt, int port)
{
	int count;
	request_t *req;
	MSG_PORT_ENABLE *enable_req;

	req = mpt_get_request(mpt);

	enable_req = req->req_vbuf;
	bzero(enable_req, sizeof *enable_req);

	enable_req->Function   = MPI_FUNCTION_PORT_ENABLE;
	enable_req->MsgContext = req->index | 0x80000000;
	enable_req->PortNumber = port;

	mpt_check_doorbell(mpt);
	if (mpt->verbose > 1) {
		device_printf(mpt->dev, "enabling port %d\n", port);
	}
	mpt_send_cmd(mpt, req);

	count = 0;
	do {
		DELAY(500);
		mpt_intr(mpt);
		if (++count == 1000) {
			device_printf(mpt->dev, "port enable timed out\n");
			return (-1);
		}
	} while (req->debug == REQ_ON_CHIP);
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
mpt_send_event_request(mpt_softc_t *mpt, int onoff)
{
	request_t *req;
	MSG_EVENT_NOTIFY *enable_req;

	req = mpt_get_request(mpt);

	enable_req = req->req_vbuf;
	bzero(enable_req, sizeof *enable_req);

	enable_req->Function   = MPI_FUNCTION_EVENT_NOTIFICATION;
	enable_req->MsgContext = req->index | 0x80000000;
	enable_req->Switch     = onoff;

	mpt_check_doorbell(mpt);
	if (mpt->verbose > 1) {
		device_printf(mpt->dev, "%sabling async events\n",
		    onoff? "en" : "dis");
	}
	mpt_send_cmd(mpt, req);

	return (0);
}

/*
 * Un-mask the interupts on the chip.
 */
void
mpt_enable_ints(mpt_softc_t *mpt)
{
	/* Unmask every thing except door bell int */
	mpt_write(mpt, MPT_OFFSET_INTR_MASK, MPT_INTR_DB_MASK);
}

/*
 * Mask the interupts on the chip.
 */
void
mpt_disable_ints(mpt_softc_t *mpt)
{
	/* Mask all interrupts */
	mpt_write(mpt, MPT_OFFSET_INTR_MASK, 
	    MPT_INTR_REPLY_MASK | MPT_INTR_DB_MASK);
}

/* (Re)Initialize the chip for use */
int
mpt_init(mpt_softc_t *mpt, u_int32_t who)
{
        int try;
        MSG_IOC_FACTS_REPLY facts;
        MSG_PORT_FACTS_REPLY pfp;
	u_int32_t pptr;
        int val;

	/* Put all request buffers (back) on the free list */
        SLIST_INIT(&mpt->request_free_list);
	for (val = 0; val < MPT_MAX_REQUESTS; val++) {
		mpt_free_request(mpt, &mpt->requests[val]);
	}

	if (mpt->verbose > 1) {
		device_printf(mpt->dev, "doorbell req = %s\n",
		    mpt_ioc_diag(mpt_read(mpt, MPT_OFFSET_DOORBELL)));
	}

	/*
	 * Start by making sure we're not at FAULT or RESET state
	 */
	switch (mpt_rd_db(mpt) & MPT_DB_STATE_MASK) {
	case MPT_DB_STATE_RESET:
	case MPT_DB_STATE_FAULT:
		if (mpt_reset(mpt) != MPT_OK) {
			return (EIO);
		}
	default:
		break;
	}
	

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
		if ((mpt_rd_db(mpt) & MPT_DB_STATE_MASK) !=
		    MPT_DB_STATE_READY) {
			if (mpt_reset(mpt) != MPT_OK) {
				DELAY(10000);
				continue;
			}
		}

		if (mpt_get_iocfacts(mpt, &facts) != MPT_OK) {
			device_printf(mpt->dev, "mpt_get_iocfacts failed\n");
			continue;
		}

		if (mpt->verbose > 1) {
			device_printf(mpt->dev,
			    "mpt_get_iocfacts: GlobalCredits=%d BlockSize=%u "
			    "Request Frame Size %u\n", facts.GlobalCredits,
			    facts.BlockSize, facts.RequestFrameSize);
		}
		mpt->mpt_global_credits = facts.GlobalCredits;
		mpt->request_frame_size = facts.RequestFrameSize;

		if (mpt_get_portfacts(mpt, &pfp) != MPT_OK) {
			device_printf(mpt->dev, "mpt_get_portfacts failed\n");
			continue;
		}

		if (mpt->verbose > 1) {
			device_printf(mpt->dev,
			    "mpt_get_portfacts: Type %x PFlags %x IID %d\n",
			    pfp.PortType, pfp.ProtocolFlags, pfp.PortSCSIID);
		}

		if (pfp.PortType != MPI_PORTFACTS_PORTTYPE_SCSI &&
		    pfp.PortType != MPI_PORTFACTS_PORTTYPE_FC) {
			device_printf(mpt->dev, "Unsupported Port Type (%x)\n",
			    pfp.PortType);
			return (ENXIO);
		}
		if (!(pfp.ProtocolFlags & MPI_PORTFACTS_PROTOCOL_INITIATOR)) {
			device_printf(mpt->dev, "initiator role unsupported\n");
			return (ENXIO);
		}
		if (pfp.PortType == MPI_PORTFACTS_PORTTYPE_FC) {
			mpt->is_fc = 1;
		} else {
			mpt->is_fc = 0;
		}
		mpt->mpt_ini_id = pfp.PortSCSIID;

		if (mpt_send_ioc_init(mpt, who) != MPT_OK) {
			device_printf(mpt->dev, "mpt_send_ioc_init failed\n");
			continue;
		}

		if (mpt->verbose > 1) {
			device_printf(mpt->dev, "mpt_send_ioc_init ok\n");
		}

		if (mpt_wait_state(mpt, MPT_DB_STATE_RUNNING) != MPT_OK) {
			device_printf(mpt->dev,
			    "IOC failed to go to run state\n");
			continue;
		}
		if (mpt->verbose > 1) {
			device_printf(mpt->dev, "IOC now at RUNSTATE\n");
		}

		/*
		 * Give it reply buffers
		 *
		 * Do *not* except global credits.
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
		 * Read set up initial configuration information
		 * (SPI only for now)
		 */

		if (mpt->is_fc == 0) {
			if (mpt_read_config_info_spi(mpt)) {
				return (EIO);
			}
			if (mpt_set_initial_config_spi(mpt)) {
				return (EIO);
			}
		}

		/*
		 * Now enable the port
		 */
		if (mpt_send_port_enable(mpt, 0) != MPT_OK) {
			device_printf(mpt->dev, "failed to enable port 0\n");
			continue;
		}

		if (mpt->verbose > 1) {
			device_printf(mpt->dev, "enabled port 0\n");
		}

		/* Everything worked */
		break;
	}

	if (try >= MPT_MAX_TRYS) {
		device_printf(mpt->dev, "failed to initialize IOC\n");
		return (EIO);
	}

	if (mpt->verbose > 1) {
		device_printf(mpt->dev, "enabling interrupts\n");
	}

	mpt_enable_ints(mpt);
	return (0);
}
