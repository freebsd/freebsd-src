/*-
 * Copyright (c) 2003-2009 Silicon Graphics International Corp.
 * Copyright (c) 2012 The FreeBSD Foundation
 * Copyright (c) 2015 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <cam/cam.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_frontend.h>
#include <cam/ctl/ctl_util.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_ha.h>
#include <cam/ctl/ctl_private.h>
#include <cam/ctl/ctl_debug.h>
#include <cam/ctl/ctl_error.h>

struct cfi_softc {
	uint32_t		cur_tag_num;
	struct ctl_port		port;
};

static struct cfi_softc cfi_softc;

static int cfi_init(void);
static void cfi_shutdown(void);
static void cfi_datamove(union ctl_io *io);
static void cfi_done(union ctl_io *io);

static struct ctl_frontend cfi_frontend =
{
	.name = "ioctl",
	.init = cfi_init,
	.shutdown = cfi_shutdown,
};
CTL_FRONTEND_DECLARE(ctlioctl, cfi_frontend);

static int
cfi_init(void)
{
	struct cfi_softc *isoftc = &cfi_softc;
	struct ctl_port *port;

	memset(isoftc, 0, sizeof(*isoftc));

	port = &isoftc->port;
	port->frontend = &cfi_frontend;
	port->port_type = CTL_PORT_IOCTL;
	port->num_requested_ctl_io = 100;
	port->port_name = "ioctl";
	port->fe_datamove = cfi_datamove;
	port->fe_done = cfi_done;
	port->max_targets = 1;
	port->max_target_id = 0;
	port->targ_port = -1;
	port->max_initiators = 1;

	if (ctl_port_register(port) != 0) {
		printf("%s: ioctl port registration failed\n", __func__);
		return (0);
	}
	ctl_port_online(port);
	return (0);
}

void
cfi_shutdown(void)
{
	struct cfi_softc *isoftc = &cfi_softc;
	struct ctl_port *port;

	port = &isoftc->port;
	ctl_port_offline(port);
	if (ctl_port_deregister(&isoftc->port) != 0)
		printf("%s: ctl_frontend_deregister() failed\n", __func__);
}

/*
 * Data movement routine for the CTL ioctl frontend port.
 */
static int
ctl_ioctl_do_datamove(struct ctl_scsiio *ctsio)
{
	struct ctl_sg_entry *ext_sglist, *kern_sglist;
	struct ctl_sg_entry ext_entry, kern_entry;
	int ext_sglen, ext_sg_entries, kern_sg_entries;
	int ext_sg_start, ext_offset;
	int len_to_copy, len_copied;
	int kern_watermark, ext_watermark;
	int ext_sglist_malloced;
	int i, j;

	ext_sglist_malloced = 0;
	ext_sg_start = 0;
	ext_offset = 0;

	CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove\n"));

	/*
	 * If this flag is set, fake the data transfer.
	 */
	if (ctsio->io_hdr.flags & CTL_FLAG_NO_DATAMOVE) {
		ctsio->ext_data_filled = ctsio->ext_data_len;
		goto bailout;
	}

	/*
	 * To simplify things here, if we have a single buffer, stick it in
	 * a S/G entry and just make it a single entry S/G list.
	 */
	if (ctsio->io_hdr.flags & CTL_FLAG_EDPTR_SGLIST) {
		int len_seen;

		ext_sglen = ctsio->ext_sg_entries * sizeof(*ext_sglist);

		ext_sglist = (struct ctl_sg_entry *)malloc(ext_sglen, M_CTL,
							   M_WAITOK);
		ext_sglist_malloced = 1;
		if (copyin(ctsio->ext_data_ptr, ext_sglist,
				   ext_sglen) != 0) {
			ctl_set_internal_failure(ctsio,
						 /*sks_valid*/ 0,
						 /*retry_count*/ 0);
			goto bailout;
		}
		ext_sg_entries = ctsio->ext_sg_entries;
		len_seen = 0;
		for (i = 0; i < ext_sg_entries; i++) {
			if ((len_seen + ext_sglist[i].len) >=
			     ctsio->ext_data_filled) {
				ext_sg_start = i;
				ext_offset = ctsio->ext_data_filled - len_seen;
				break;
			}
			len_seen += ext_sglist[i].len;
		}
	} else {
		ext_sglist = &ext_entry;
		ext_sglist->addr = ctsio->ext_data_ptr;
		ext_sglist->len = ctsio->ext_data_len;
		ext_sg_entries = 1;
		ext_sg_start = 0;
		ext_offset = ctsio->ext_data_filled;
	}

	if (ctsio->kern_sg_entries > 0) {
		kern_sglist = (struct ctl_sg_entry *)ctsio->kern_data_ptr;
		kern_sg_entries = ctsio->kern_sg_entries;
	} else {
		kern_sglist = &kern_entry;
		kern_sglist->addr = ctsio->kern_data_ptr;
		kern_sglist->len = ctsio->kern_data_len;
		kern_sg_entries = 1;
	}


	kern_watermark = 0;
	ext_watermark = ext_offset;
	len_copied = 0;
	for (i = ext_sg_start, j = 0;
	     i < ext_sg_entries && j < kern_sg_entries;) {
		uint8_t *ext_ptr, *kern_ptr;

		len_to_copy = MIN(ext_sglist[i].len - ext_watermark,
				  kern_sglist[j].len - kern_watermark);

		ext_ptr = (uint8_t *)ext_sglist[i].addr;
		ext_ptr = ext_ptr + ext_watermark;
		if (ctsio->io_hdr.flags & CTL_FLAG_BUS_ADDR) {
			/*
			 * XXX KDM fix this!
			 */
			panic("need to implement bus address support");
#if 0
			kern_ptr = bus_to_virt(kern_sglist[j].addr);
#endif
		} else
			kern_ptr = (uint8_t *)kern_sglist[j].addr;
		kern_ptr = kern_ptr + kern_watermark;

		kern_watermark += len_to_copy;
		ext_watermark += len_to_copy;

		if ((ctsio->io_hdr.flags & CTL_FLAG_DATA_MASK) ==
		     CTL_FLAG_DATA_IN) {
			CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove: copying %d "
					 "bytes to user\n", len_to_copy));
			CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove: from %p "
					 "to %p\n", kern_ptr, ext_ptr));
			if (copyout(kern_ptr, ext_ptr, len_to_copy) != 0) {
				ctl_set_internal_failure(ctsio,
							 /*sks_valid*/ 0,
							 /*retry_count*/ 0);
				goto bailout;
			}
		} else {
			CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove: copying %d "
					 "bytes from user\n", len_to_copy));
			CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove: from %p "
					 "to %p\n", ext_ptr, kern_ptr));
			if (copyin(ext_ptr, kern_ptr, len_to_copy)!= 0){
				ctl_set_internal_failure(ctsio,
							 /*sks_valid*/ 0,
							 /*retry_count*/0);
				goto bailout;
			}
		}

		len_copied += len_to_copy;

		if (ext_sglist[i].len == ext_watermark) {
			i++;
			ext_watermark = 0;
		}

		if (kern_sglist[j].len == kern_watermark) {
			j++;
			kern_watermark = 0;
		}
	}

	ctsio->ext_data_filled += len_copied;

	CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove: ext_sg_entries: %d, "
			 "kern_sg_entries: %d\n", ext_sg_entries,
			 kern_sg_entries));
	CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove: ext_data_len = %d, "
			 "kern_data_len = %d\n", ctsio->ext_data_len,
			 ctsio->kern_data_len));


	/* XXX KDM set residual?? */
bailout:

	if (ext_sglist_malloced != 0)
		free(ext_sglist, M_CTL);

	return (CTL_RETVAL_COMPLETE);
}

static void
cfi_datamove(union ctl_io *io)
{
	struct ctl_fe_ioctl_params *params;

	params = (struct ctl_fe_ioctl_params *)
		io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;

	mtx_lock(&params->ioctl_mtx);
	params->state = CTL_IOCTL_DATAMOVE;
	cv_broadcast(&params->sem);
	mtx_unlock(&params->ioctl_mtx);
}

static void
cfi_done(union ctl_io *io)
{
	struct ctl_fe_ioctl_params *params;

	params = (struct ctl_fe_ioctl_params *)
		io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;

	mtx_lock(&params->ioctl_mtx);
	params->state = CTL_IOCTL_DONE;
	cv_broadcast(&params->sem);
	mtx_unlock(&params->ioctl_mtx);
}

static int
cfi_submit_wait(union ctl_io *io)
{
	struct ctl_fe_ioctl_params params;
	ctl_fe_ioctl_state last_state;
	int done, retval;

	retval = 0;

	bzero(&params, sizeof(params));

	mtx_init(&params.ioctl_mtx, "ctliocmtx", NULL, MTX_DEF);
	cv_init(&params.sem, "ctlioccv");
	params.state = CTL_IOCTL_INPROG;
	last_state = params.state;

	io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr = &params;

	CTL_DEBUG_PRINT(("cfi_submit_wait\n"));

	/* This shouldn't happen */
	if ((retval = ctl_queue(io)) != CTL_RETVAL_COMPLETE)
		return (retval);

	done = 0;

	do {
		mtx_lock(&params.ioctl_mtx);
		/*
		 * Check the state here, and don't sleep if the state has
		 * already changed (i.e. wakeup has already occured, but we
		 * weren't waiting yet).
		 */
		if (params.state == last_state) {
			/* XXX KDM cv_wait_sig instead? */
			cv_wait(&params.sem, &params.ioctl_mtx);
		}
		last_state = params.state;

		switch (params.state) {
		case CTL_IOCTL_INPROG:
			/* Why did we wake up? */
			/* XXX KDM error here? */
			mtx_unlock(&params.ioctl_mtx);
			break;
		case CTL_IOCTL_DATAMOVE:
			CTL_DEBUG_PRINT(("got CTL_IOCTL_DATAMOVE\n"));

			/*
			 * change last_state back to INPROG to avoid
			 * deadlock on subsequent data moves.
			 */
			params.state = last_state = CTL_IOCTL_INPROG;

			mtx_unlock(&params.ioctl_mtx);
			ctl_ioctl_do_datamove(&io->scsiio);
			/*
			 * Note that in some cases, most notably writes,
			 * this will queue the I/O and call us back later.
			 * In other cases, generally reads, this routine
			 * will immediately call back and wake us up,
			 * probably using our own context.
			 */
			io->scsiio.be_move_done(io);
			break;
		case CTL_IOCTL_DONE:
			mtx_unlock(&params.ioctl_mtx);
			CTL_DEBUG_PRINT(("got CTL_IOCTL_DONE\n"));
			done = 1;
			break;
		default:
			mtx_unlock(&params.ioctl_mtx);
			/* XXX KDM error here? */
			break;
		}
	} while (done == 0);

	mtx_destroy(&params.ioctl_mtx);
	cv_destroy(&params.sem);

	return (CTL_RETVAL_COMPLETE);
}

int
ctl_ioctl_io(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
    struct thread *td)
{
	union ctl_io *io;
	void *pool_tmp;
	int retval = 0;

	/*
	 * If we haven't been "enabled", don't allow any SCSI I/O
	 * to this FETD.
	 */
	if ((cfi_softc.port.status & CTL_PORT_STATUS_ONLINE) == 0)
		return (EPERM);

	io = ctl_alloc_io(cfi_softc.port.ctl_pool_ref);

	/*
	 * Need to save the pool reference so it doesn't get
	 * spammed by the user's ctl_io.
	 */
	pool_tmp = io->io_hdr.pool;
	memcpy(io, (void *)addr, sizeof(*io));
	io->io_hdr.pool = pool_tmp;

	/*
	 * No status yet, so make sure the status is set properly.
	 */
	io->io_hdr.status = CTL_STATUS_NONE;

	/*
	 * The user sets the initiator ID, target and LUN IDs.
	 */
	io->io_hdr.nexus.targ_port = cfi_softc.port.targ_port;
	io->io_hdr.flags |= CTL_FLAG_USER_REQ;
	if ((io->io_hdr.io_type == CTL_IO_SCSI) &&
	    (io->scsiio.tag_type != CTL_TAG_UNTAGGED))
		io->scsiio.tag_num = cfi_softc.cur_tag_num++;

	retval = cfi_submit_wait(io);
	if (retval == 0)
		memcpy((void *)addr, io, sizeof(*io));
	ctl_free_io(io);
	return (retval);
}
