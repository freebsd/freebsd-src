/*-
 * Copyright (c) 2003, 2008 Silicon Graphics International Corp.
 * Copyright (c) 2012 The FreeBSD Foundation
 * Copyright (c) 2014-2015 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
 *
 * Portions of this software were developed by Edward Tomasz Napierala
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_backend_ramdisk.c#3 $
 */
/*
 * CAM Target Layer backend for a "fake" ramdisk.
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */
#ifndef CTL_BACKEND_PASS
#define CTL_BACKEND_PASS

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <cam/ctl/ctl_backend_passthrough.h>
#include <cam/scsi/scsi_passthrough.c>
#endif

static struct ctl_be_passthrough_softc rd_softc;
extern struct ctl_softc *control_softc;
static int ctl_backend_passthrough_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *td);
static int ctl_backend_passthrough_init(void);
static void ctl_backend_passthrough_shutdown(void);
static void ctl_backend_passthrough_lun_shutdown(void *be_lun);
static void ctl_backend_passthrough_lun_config_status(void *be_lun,
						  ctl_lun_config_status status);
static int ctl_backend_passthrough_move_done(union ctl_io *io);
static int ctl_backend_passthrough_submit(union ctl_io *io);
static void ctl_backend_passthrough_continue(union ctl_io *io);
static int  ctl_backend_passthrough_remove(struct ctl_be_passthrough_softc *softc,struct ctl_lun_req *lun_req);
static int ctl_backend_passthrough_modify(struct ctl_be_passthrough_softc *softc,struct ctl_lun_req *lun_req);
static struct ctl_backend_driver ctl_be_passthrough_driver = 
{
	.name = "passthrough",
	.flags = CTL_BE_FLAG_HAS_CONFIG,
	.init = ctl_backend_passthrough_init,
	.ioctl = ctl_backend_passthrough_ioctl,
	.data_submit = ctl_backend_passthrough_submit
};

static MALLOC_DEFINE(M_PASSTHROUGH, "ctlpassthrough", "Memory used for CTL Passthrough");
CTL_BACKEND_DECLARE(ctlpassthrough, ctl_be_passthrough_driver);


static int
ctl_backend_passthrough_init(void)
{
	struct ctl_be_passthrough_softc *softc;

	softc = &rd_softc;
	memset(softc, 0 ,sizeof(*softc));

	mtx_init(&softc->lock , "ctlpassthrough",NULL, MTX_DEF);

	STAILQ_INIT(&softc->lun_list);

	return (0);

}
static int ctl_backend_passthrough_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *td){

	struct ctl_be_passthrough_softc *softc;
	int retval;
		

	retval =0;
	softc =&rd_softc;
	switch(cmd){
	case CTL_LUN_REQ: {
		struct ctl_lun_req *lun_req;
		
		lun_req = (struct ctl_lun_req *)addr;

		switch(lun_req->reqtype) {

		case CTL_LUNREQ_CREATE:{
		u_int path_id;
		u_int target_id;
		u_int32_t lun_id;
		struct cam_path *path = NULL;
		struct cam_periph *periph=NULL;
		struct ctl_lun_create_params *params = NULL;
		char *name ="ctlpass";	
		params = &lun_req->reqdata.create;
		path_id = params->scbus;
		target_id = params->target;
		lun_id = params->lun_num;
		
		printf("path id %d and target id %d",path_id,target_id);

		if(xpt_create_path(&path, NULL, path_id, target_id, lun_id) == CAM_REQ_CMP)
		{
			if(path == NULL)
				printf("path is NULL");
			xpt_print_path(path);
			xpt_path_lock(path);
		periph = cam_periph_find(path,name);
		xpt_path_unlock(path);
		xpt_free_path(path);	
		retval = ctl_backend_passthrough_create(periph,lun_req);
		}
		lun_req->status = CTL_LUN_OK;	
			break;
	}
		case CTL_LUNREQ_RM:
			retval = ctl_backend_passthrough_remove(softc,lun_req);
			break;
		case CTL_LUNREQ_MODIFY:
			retval = ctl_backend_passthrough_modify(softc,lun_req);
			break;
		default:
			lun_req->status = CTL_LUN_ERROR;
			break;
		}
		break;
	}
	default:
		retval =ENOTTY;
		break;
	}

	return (retval);
}
int ctl_backend_passthrough_create(struct cam_periph *periph,struct ctl_lun_req *lun_req)
{
	struct ctl_lun_create_params *params;
	struct ctl_be_passthrough_softc *softc = &rd_softc;
	struct ctl_be_passthrough_lun *be_lun;
	struct ctl_be_lun *cbe_lun;
	char *value;
      	char num_thread_str[16];
      	int num_threads=0 , tmp_num_threads=0;

	char tmpstr[32];
	
	int retval;

	retval = 0;
	params = &lun_req->reqdata.create;
	if(periph==NULL)
		printf("periph is NULL");

	be_lun = malloc(sizeof(*be_lun), M_PASSTHROUGH, M_ZERO | M_WAITOK);

	if(be_lun == NULL)
	{
		goto bailout_error;
	}
	STAILQ_INIT(&be_lun->cbe_lun.options);

	cbe_lun = &be_lun->cbe_lun;
	cbe_lun->be_lun = be_lun;
	be_lun->params = lun_req->reqdata.create;
	be_lun->cbe_lun.lun_type = T_PASSTHROUGH;
	be_lun->softc=softc;
	be_lun->periph = periph;
	be_lun->flags = CTL_BE_PASSTHROUGH_LUN_UNCONFIGURED;
	cbe_lun->flags =0 ;
	 
	ctl_init_opts(&cbe_lun->options,
            lun_req->num_be_args, lun_req->kern_be_args);
        cbe_lun->scbus = params->scbus;
        cbe_lun->target = params->target;
        cbe_lun->lun = params->lun_num;
        value = ctl_get_opt(&cbe_lun->options, "num_threads");
        if (value != NULL) {
                tmp_num_threads = strtol(value, NULL, 0);

                /*
                 * We don't let the user specify less than one
                 * thread, but hope he's clueful enough not to
                 * specify 1000 threads.
                 */
                if (tmp_num_threads < 1) {
                        snprintf(lun_req->error_str, sizeof(lun_req->error_str),
                                 "invalid number of threads %s",
                                 num_thread_str);
                        goto bailout_error;
                }
                num_threads = tmp_num_threads;
        }

        be_lun->num_threads = num_threads;

	be_lun->cbe_lun.maxlba=0;
	cbe_lun->blocksize=512;
	be_lun->size_bytes = 0;
	be_lun->size_blocks =0;
	cbe_lun->flags |=CTL_LUN_FLAG_UNMAP;
	
	
	be_lun->cbe_lun.flags = CTL_LUN_FLAG_PRIMARY;
	be_lun->cbe_lun.be_lun = be_lun;
	be_lun->cbe_lun.req_lun_id=0;

	be_lun->cbe_lun.lun_shutdown = ctl_backend_passthrough_lun_shutdown;
	be_lun->cbe_lun.lun_config_status = ctl_backend_passthrough_lun_config_status;

	be_lun->cbe_lun.be = &ctl_be_passthrough_driver;
		snprintf(tmpstr, sizeof(tmpstr), "MYSERIAL%4d",
			 softc->num_luns);
		strncpy((char *)cbe_lun->serial_num, tmpstr,
			MIN(sizeof(cbe_lun->serial_num), sizeof(tmpstr)));
	
	
		snprintf(tmpstr, sizeof(tmpstr), "MYDEVID%4d", softc->num_luns);
		strncpy((char *)cbe_lun->device_id, tmpstr,
			MIN(sizeof(cbe_lun->device_id), sizeof(tmpstr)));


	mtx_lock(&softc->lock);
	softc->num_luns++;
	STAILQ_INSERT_TAIL(&softc->lun_list , be_lun , links);
	mtx_unlock(&softc->lock);



	retval = ctl_add_lun(&be_lun->cbe_lun);


	if(retval!=0)
	{
		mtx_lock(&softc->lock);
		STAILQ_REMOVE(&softc->lun_list , be_lun ,ctl_be_passthrough_lun, links);
		
		softc->num_luns--;
	        mtx_unlock(&softc->lock);
	
		retval =0;
		goto bailout_error;
	}
	
	mtx_lock(&softc->lock);
	be_lun->flags |= CTL_BE_PASSTHROUGH_LUN_WAITING;

	while (be_lun->flags & CTL_BE_PASSTHROUGH_LUN_UNCONFIGURED) {
		retval = msleep(be_lun, &softc->lock, PCATCH, "ctlpassthrough", 0);
		if (retval == EINTR)
			break;
	}

	be_lun->flags &= ~CTL_BE_PASSTHROUGH_LUN_WAITING;


	if(be_lun->flags & CTL_BE_PASSTHROUGH_LUN_CONFIG_ERR){
		STAILQ_REMOVE(&softc->lun_list, be_lun ,ctl_be_passthrough_lun, links);
		softc->num_luns--;
		mtx_unlock(&softc->lock);
		goto bailout_error;
	}
	else{
	params->req_lun_id = cbe_lun->lun_id;

	}
	mtx_unlock(&softc->lock);
	

	return (retval);
bailout_error:

	return (retval);

}

static int ctl_backend_passthrough_modify(struct ctl_be_passthrough_softc *softc,struct ctl_lun_req *req){
struct ctl_be_passthrough_lun *be_lun;
	struct ctl_be_lun *cbe_lun;
	struct ctl_lun_modify_params *params;
	char *value;
	uint32_t blocksize;
	int wasprim;

	params = &req->reqdata.modify;

	mtx_lock(&softc->lock);
	STAILQ_FOREACH(be_lun, &softc->lun_list, links) {
		if (be_lun->cbe_lun.lun_id == params->lun_id)
			break;
	}
	mtx_unlock(&softc->lock);
	if (be_lun == NULL) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "%s: LUN %u is not managed by the passthrough backend",
			 __func__, params->lun_id);
		goto bailout_error;
	}
	cbe_lun = &be_lun->cbe_lun;

	if (params->lun_size_bytes != 0)
		be_lun->params.lun_size_bytes = params->lun_size_bytes;
	ctl_update_opts(&cbe_lun->options, req->num_be_args, req->kern_be_args);

	wasprim = (cbe_lun->flags & CTL_LUN_FLAG_PRIMARY);
	value = ctl_get_opt(&cbe_lun->options, "ha_role");
	if (value != NULL) {
		if (strcmp(value, "primary") == 0)
			cbe_lun->flags |= CTL_LUN_FLAG_PRIMARY;
		else
			cbe_lun->flags &= ~CTL_LUN_FLAG_PRIMARY;
	} else if (control_softc->flags & CTL_FLAG_ACTIVE_SHELF)
		cbe_lun->flags |= CTL_LUN_FLAG_PRIMARY;
	else
		cbe_lun->flags &= ~CTL_LUN_FLAG_PRIMARY;
	if (wasprim != (cbe_lun->flags & CTL_LUN_FLAG_PRIMARY)) {
		if (cbe_lun->flags & CTL_LUN_FLAG_PRIMARY)
			ctl_lun_primary(cbe_lun);
		else
			ctl_lun_secondary(cbe_lun);
	}

	blocksize = be_lun->cbe_lun.blocksize;
	if (be_lun->params.lun_size_bytes < blocksize) {
		snprintf(req->error_str, sizeof(req->error_str),
			"%s: LUN size %ju < blocksize %u", __func__,
			be_lun->params.lun_size_bytes, blocksize);
			goto bailout_error;
	}
	be_lun->size_blocks = be_lun->params.lun_size_bytes / blocksize;
	be_lun->size_bytes = be_lun->size_blocks * blocksize;
	be_lun->cbe_lun.maxlba = be_lun->size_blocks - 1;
	ctl_lun_capacity_changed(&be_lun->cbe_lun);

	/* Tell the user the exact size we ended up using */
	params->lun_size_bytes = be_lun->size_bytes;

	req->status = CTL_LUN_OK;
	return (0);

bailout_error:
	req->status = CTL_LUN_ERROR;
	return (0);

}



static int ctl_backend_passthrough_remove(struct ctl_be_passthrough_softc *softc,struct ctl_lun_req *req){
struct ctl_be_passthrough_lun *be_lun;
	struct ctl_lun_rm_params *params;
	int retval;

	params = &req->reqdata.rm;
	mtx_lock(&softc->lock);
	STAILQ_FOREACH(be_lun, &softc->lun_list, links) {
		if (be_lun->cbe_lun.lun_id == params->lun_id)
			break;
	}
	mtx_unlock(&softc->lock);
	if (be_lun == NULL) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "%s: LUN %u is not managed by the passthrough backend",
			 __func__, params->lun_id);
		goto bailout_error;
	}

	retval = ctl_disable_lun(&be_lun->cbe_lun);
	if (retval != 0) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "%s: error %d returned from ctl_disable_lun() for "
			 "LUN %d", __func__, retval, params->lun_id);
		goto bailout_error;
	}

	/*
	 * Set the waiting flag before we invalidate the LUN.  Our shutdown
	 * routine can be called any time after we invalidate the LUN,
	 * and can be called from our context.
	 *
	 * This tells the shutdown routine that we're waiting, or we're
	 * going to wait for the shutdown to happen.
	 */
	mtx_lock(&softc->lock);
	be_lun->flags |= CTL_BE_PASSTHROUGH_LUN_WAITING;
	mtx_unlock(&softc->lock);

	retval = ctl_invalidate_lun(&be_lun->cbe_lun);
	if (retval != 0) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "%s: error %d returned from ctl_invalidate_lun() for "
			 "LUN %d", __func__, retval, params->lun_id);
		mtx_lock(&softc->lock);
		be_lun->flags &= ~CTL_BE_PASSTHROUGH_LUN_WAITING;
		mtx_unlock(&softc->lock);
		goto bailout_error;
	}

	mtx_lock(&softc->lock);

	be_lun->flags &= ~CTL_BE_PASSTHROUGH_LUN_WAITING;

	/*
	 * We only remove this LUN from the list and free it (below) if
	 * retval == 0.  If the user interrupted the wait, we just bail out
	 * without actually freeing the LUN.  We let the shutdown routine
	 * free the LUN if that happens.
	 */
	if (retval == 0) {
		STAILQ_REMOVE(&softc->lun_list, be_lun, ctl_be_passthrough_lun,
			      links);
		softc->num_luns--;
	}

	mtx_unlock(&softc->lock);

	if (retval == 0) {
		ctl_free_opts(&be_lun->cbe_lun.options);
		free(be_lun, M_PASSTHROUGH);
	}

	req->status = CTL_LUN_OK;
	return (retval);

bailout_error:
	req->status = CTL_LUN_ERROR;
	return (0);
}

static void
ctl_backend_passthrough_shutdown(void)
{
struct ctl_be_passthrough_softc *softc = &rd_softc;
	struct ctl_be_passthrough_lun *lun, *next_lun;

	mtx_lock(&softc->lock);
	STAILQ_FOREACH_SAFE(lun, &softc->lun_list, links, next_lun) {
		/*
		 * Drop our lock here.  Since ctl_invalidate_lun() can call
		 * back into us, this could potentially lead to a recursive
		 * lock of the same mutex, which would cause a hang.
		 */
		mtx_unlock(&softc->lock);
		ctl_disable_lun(&lun->cbe_lun);
		ctl_invalidate_lun(&lun->cbe_lun);
		mtx_lock(&softc->lock);
	}
	mtx_unlock(&softc->lock);

	if (ctl_backend_deregister(&ctl_be_passthrough_driver) != 0) {
		printf("ctl_backend_passthrough_shutdown: "
		       "ctl_backend_deregister() failed!\n");
	}
}
static void
ctl_backend_passthrough_lun_config_status(void *be_lun,
				      ctl_lun_config_status status)
{
	struct ctl_be_passthrough_lun *lun;
	struct ctl_be_passthrough_softc *softc;

	lun = (struct ctl_be_passthrough_lun *)be_lun;
	softc = lun->softc;

	if (status == CTL_LUN_CONFIG_OK) {
		mtx_lock(&softc->lock);
		lun->flags &= ~CTL_BE_PASSTHROUGH_LUN_UNCONFIGURED;
		if (lun->flags & CTL_BE_PASSTHROUGH_LUN_WAITING)
			wakeup(lun);
		mtx_unlock(&softc->lock);
		/*
		 * We successfully added the LUN, attempt to enable it.
		 */
		if (ctl_enable_lun(&lun->cbe_lun) != 0) {
			printf("%s: ctl_enable_lun() failed!\n", __func__);
			if (ctl_invalidate_lun(&lun->cbe_lun) != 0) {
				printf("%s: ctl_invalidate_lun() failed!\n",
				       __func__);
			}
		}

		return;
	}


	mtx_lock(&softc->lock);
	lun->flags &= ~CTL_BE_PASSTHROUGH_LUN_UNCONFIGURED;

	/*
	 * If we have a user waiting, let him handle the cleanup.  If not,
	 * clean things up here.
	 */
	if (lun->flags & CTL_BE_PASSTHROUGH_LUN_WAITING) {
		lun->flags |= CTL_BE_PASSTHROUGH_LUN_CONFIG_ERR;
		wakeup(lun);
	} else {
		STAILQ_REMOVE(&softc->lun_list, lun, ctl_be_passthrough_lun,
			      links);
		softc->num_luns--;
		free(lun, M_PASSTHROUGH);
	}
	mtx_unlock(&softc->lock);
}
static int ctl_backend_passthrough_move_done(union ctl_io *io){


struct ctl_lun *lun;
struct ctl_be_passthrough_lun *be_lun; 
	lun = (struct ctl_lun *)io->scsiio.io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	be_lun = (struct ctl_be_passthrough_lun *)lun->be_lun->be_lun;
	return ctlccb(be_lun->periph,io);
}

static int
ctl_backend_passthrough_submit(union ctl_io *io)
{
	struct ctl_be_lun *cbe_lun;
	struct ctl_lba_len_flags *lbalen;
	
	
	cbe_lun = (struct ctl_be_lun *)io->io_hdr.ctl_private[CTL_PRIV_BACKEND_LUN].ptr;
	lbalen = (struct ctl_lba_len_flags *)&io->io_hdr.ctl_private[CTL_PRIV_LBA_LEN];
	if (lbalen->flags & CTL_LLF_VERIFY) {
		ctl_set_success(&io->scsiio);
		ctl_data_submit_done(io);
		return (CTL_RETVAL_COMPLETE);
	}
	io->io_hdr.ctl_private[CTL_PRIV_BACKEND].integer =
	    lbalen->len * cbe_lun->blocksize;
	ctl_backend_passthrough_continue(io);
	return (CTL_RETVAL_COMPLETE);
}

static void ctl_backend_passthrough_continue(union ctl_io *io)
{

	int len;
		
	
	len = io->io_hdr.ctl_private[CTL_PRIV_BACKEND].integer;
	
	io->scsiio.kern_data_ptr = malloc(io->scsiio.ext_data_len,M_PASSTHROUGH,M_WAITOK);

	io->scsiio.be_move_done = ctl_backend_passthrough_move_done;
	io->scsiio.kern_data_resid = 0;
	io->scsiio.kern_data_len = io->scsiio.ext_data_len;
	io->scsiio.kern_sg_entries = 0;
	io->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	io->io_hdr.ctl_private[CTL_PRIV_BACKEND].integer -= len;
	
	ctl_datamove(io);

}

static void
ctl_backend_passthrough_lun_shutdown(void *be_lun)
{
	struct ctl_be_passthrough_lun *lun;
	struct ctl_be_passthrough_softc *softc;
	int do_free;

	lun = (struct ctl_be_passthrough_lun *)be_lun;
	softc = lun->softc;
	do_free = 0;

	mtx_lock(&softc->lock);
	lun->flags |= CTL_BE_PASSTHROUGH_LUN_UNCONFIGURED;
	if (lun->flags & CTL_BE_PASSTHROUGH_LUN_WAITING) {
		wakeup(lun);
	} else {
		STAILQ_REMOVE(&softc->lun_list, lun, ctl_be_passthrough_lun,
			      links);
		softc->num_luns--;
		do_free = 1;
	}
	mtx_unlock(&softc->lock);

	if (do_free != 0)
		free(be_lun, M_PASSTHROUGH);


}
