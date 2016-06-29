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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/condvar.h>
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/taskqueue.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_util.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_debug.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_ha.h>
#include <cam/ctl/ctl_private.h>
#include <cam/ctl/ctl_error.h>

#endif

typedef enum {
	CTL_BE_PASSTHROUGH_LUN_UNCONFIGURED	= 0x01,
	CTL_BE_PASSTHROUGH_LUN_CONFIG_ERR	= 0x02,
	CTL_BE_PASSTHROUGH_LUN_WAITING	= 0x04
} ctl_be_passthrough_lun_flags;

struct ctl_be_passthrough_lun {
	struct ctl_lun_create_params params;
	char lunname[32];
	struct cam_periph *periph;
	uint64_t size_bytes;
	uint64_t size_blocks;
	struct ctl_be_passthrough_softc *softc;
	ctl_be_passthrough_lun_flags flags;
	STAILQ_ENTRY(ctl_be_passthrough_lun) links;
	struct ctl_be_lun ctl_be_lun;
	STAILQ_HEAD(, ctl_io_hdr) cont_queue;
	struct mtx_padalign queue_lock;
};

struct ctl_be_passthrough_softc {
	struct mtx lock;
	int num_luns;
	STAILQ_HEAD(, ctl_be_passthrough_lun) lun_list;
};

static struct ctl_be_passthrough_softc rd_softc;
extern struct ctl_softc *control_softc;

static int ctl_backend_passthrough_init(void);
static void ctl_backend_passthrough_shutdown(void);
static void ctl_backend_passthrough_lun_shutdown(void *be_lun);
static void ctl_backend_passthrough_lun_config_status(void *be_lun,
						  ctl_lun_config_status status);

static int ctl_backend_passthrough_create(struct cam_periph *periph);
static struct ctl_backend_driver ctl_be_passthrough_driver = 
{
	.name = "ctlpassthrough",
	.flags = CTL_BE_FLAG_HAS_CONFIG,
	.init = ctl_backend_passthrough_init
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
static int ctl_backend_passthrough_create(struct cam_periph *periph)
{

	struct ctl_be_passthrough_softc *softc = &rd_softc;
	struct ctl_be_passthrough_lun *be_lun;
//	struct ctl_be_lun *ctl_be_lun;
	struct ctl_be_lun *cbe_lun;

	char tmpstr[32];
	
	int retval;

	retval = 0;

	be_lun = malloc(sizeof(*be_lun), M_PASSTHROUGH, M_ZERO | M_WAITOK);

	if(be_lun == NULL)
	{
		printf("error allocation backend lun");
		goto bailout_error;
	}
	STAILQ_INIT(&be_lun->ctl_be_lun.options);

	cbe_lun = &be_lun->ctl_be_lun;
	be_lun->ctl_be_lun.lun_type = T_PASSTHROUGH;
	
	
	be_lun->ctl_be_lun.maxlba=0;
	be_lun->ctl_be_lun.blocksize=0;
	be_lun->size_bytes = 0;
	be_lun->size_blocks =0;

	be_lun->softc=softc;
	be_lun->periph = periph;
	
	be_lun->flags = CTL_BE_PASSTHROUGH_LUN_UNCONFIGURED;
	be_lun->ctl_be_lun.flags = CTL_LUN_FLAG_PRIMARY;
	be_lun->ctl_be_lun.be_lun = be_lun;
	be_lun->ctl_be_lun.req_lun_id=0;

	be_lun->ctl_be_lun.lun_shutdown = ctl_backend_passthrough_lun_shutdown;
	be_lun->ctl_be_lun.lun_config_status = ctl_backend_passthrough_lun_config_status;

	be_lun->ctl_be_lun.be = &ctl_be_passthrough_driver;
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

//	printf("before ctl add _lun\n");

	if(be_lun->ctl_be_lun.lun_type == T_PASSTHROUGH)
		printf("device type is passthrough");
	retval = ctl_add_lun(&be_lun->ctl_be_lun);

//	printf("after ctl add lun");
	if(retval!=0)
	{
		mtx_lock(&softc->lock);
		STAILQ_REMOVE(&softc->lun_list , be_lun ,ctl_be_passthrough_lun, links);
		
		softc->num_luns--;
	        mtx_unlock(&softc->lock);
		printf("failed to create lun type");
		retval =0;
		goto bailout_error;
	}

	mtx_lock(&softc->lock);
	if(be_lun->flags & CTL_BE_PASSTHROUGH_LUN_CONFIG_ERR){
		STAILQ_REMOVE(&softc->lun_list, be_lun ,ctl_be_passthrough_lun, links);
		softc->num_luns--;
		mtx_unlock(&softc->lock);
		goto bailout_error;
	}
	mtx_unlock(&softc->lock);
	
	printf("successfullly created lun");	
	return (retval);
bailout_error:
//	free(be_lun , M_PASSTHROUGH);

	return (retval);

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
		ctl_disable_lun(&lun->ctl_be_lun);
		ctl_invalidate_lun(&lun->ctl_be_lun);
		mtx_lock(&softc->lock);
}
	mtx_unlock(&softc->lock);
/*	
#ifdef CTL_RAMDISK_PAGES
	for (i = 0; i < softc->num_pages; i++)
		free(softc->ramdisk_pages[i], M_RAMDISK);
	free(softc->ramdisk_pages, M_RAMDISK);
#else
	free(softc->ramdisk_buffer, M_RAMDISK);
#endif*/

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

	printf("inside config status");
	if (status == CTL_LUN_CONFIG_OK) {
		mtx_lock(&softc->lock);
		//lun->flags &= ~CTL_BE_PASSTHROUGH_LUN_UNCONFIGURED;
		//if (lun->flags & CTL_BE_PASSTHROUGH_LUN_WAITING)
		//	wakeup(lun);
		mtx_unlock(&softc->lock);
		printf("iam inside status if statement config status");
		/*
		 * We successfully added the LUN, attempt to enable it.
		 */
		if (ctl_enable_lun(&lun->ctl_be_lun) != 0) {
			printf("%s: ctl_enable_lun() failed!\n", __func__);
			if (ctl_invalidate_lun(&lun->ctl_be_lun) != 0) {
				printf("%s: ctl_invalidate_lun() failed!\n",
				       __func__);
			}
		}
	printf("succesfull after lun config");
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
