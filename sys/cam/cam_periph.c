/*
 * Common functions for CAM "type" (peripheral) drivers.
 *
 * Copyright (c) 1997, 1998 Justin T. Gibbs.
 * Copyright (c) 1997, 1998, 1999, 2000 Kenneth D. Merry.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/linker_set.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/devicestat.h>
#include <sys/bus.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_periph.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_da.h>
#include <cam/scsi/scsi_pass.h>

static	u_int		camperiphnextunit(struct periph_driver *p_drv,
					  u_int newunit, int wired,
					  path_id_t pathid, target_id_t target,
					  lun_id_t lun);
static	u_int		camperiphunit(struct periph_driver *p_drv,
				      path_id_t pathid, target_id_t target,
				      lun_id_t lun); 
static	void		camperiphdone(struct cam_periph *periph, 
					union ccb *done_ccb);
static  void		camperiphfree(struct cam_periph *periph);

cam_status
cam_periph_alloc(periph_ctor_t *periph_ctor,
		 periph_oninv_t *periph_oninvalidate,
		 periph_dtor_t *periph_dtor, periph_start_t *periph_start,
		 char *name, cam_periph_type type, struct cam_path *path,
		 ac_callback_t *ac_callback, ac_code code, void *arg)
{
	struct		periph_driver **p_drv;
	struct		cam_periph *periph;
	struct		cam_periph *cur_periph;
	path_id_t	path_id;
	target_id_t	target_id;
	lun_id_t	lun_id;
	cam_status	status;
	u_int		init_level;
	int s;

	init_level = 0;
	/*
	 * Handle Hot-Plug scenarios.  If there is already a peripheral
	 * of our type assigned to this path, we are likely waiting for
	 * final close on an old, invalidated, peripheral.  If this is
	 * the case, queue up a deferred call to the peripheral's async
	 * handler.  If it looks like a mistaken re-alloation, complain.
	 */
	if ((periph = cam_periph_find(path, name)) != NULL) {

		if ((periph->flags & CAM_PERIPH_INVALID) != 0
		 && (periph->flags & CAM_PERIPH_NEW_DEV_FOUND) == 0) {
			periph->flags |= CAM_PERIPH_NEW_DEV_FOUND;
			periph->deferred_callback = ac_callback;
			periph->deferred_ac = code;
			return (CAM_REQ_INPROG);
		} else {
			printf("cam_periph_alloc: attempt to re-allocate "
			       "valid device %s%d rejected\n",
			       periph->periph_name, periph->unit_number);
		}
		return (CAM_REQ_INVALID);
	}
	
	periph = (struct cam_periph *)malloc(sizeof(*periph), M_DEVBUF,
					     M_NOWAIT);

	if (periph == NULL)
		return (CAM_RESRC_UNAVAIL);
	
	init_level++;

	for (p_drv = (struct periph_driver **)periphdriver_set.ls_items;
	     *p_drv != NULL; p_drv++) {
		if (strcmp((*p_drv)->driver_name, name) == 0)
			break;
	}
	
	path_id = xpt_path_path_id(path);
	target_id = xpt_path_target_id(path);
	lun_id = xpt_path_lun_id(path);
	bzero(periph, sizeof(*periph));
	cam_init_pinfo(&periph->pinfo);
	periph->periph_start = periph_start;
	periph->periph_dtor = periph_dtor;
	periph->periph_oninval = periph_oninvalidate;
	periph->type = type;
	periph->periph_name = name;
	periph->unit_number = camperiphunit(*p_drv, path_id, target_id, lun_id);
	periph->immediate_priority = CAM_PRIORITY_NONE;
	periph->refcount = 0;
	SLIST_INIT(&periph->ccb_list);
	status = xpt_create_path(&path, periph, path_id, target_id, lun_id);
	if (status != CAM_REQ_CMP)
		goto failure;

	periph->path = path;
	init_level++;

	status = xpt_add_periph(periph);

	if (status != CAM_REQ_CMP)
		goto failure;

	s = splsoftcam();
	cur_periph = TAILQ_FIRST(&(*p_drv)->units);
	while (cur_periph != NULL
	    && cur_periph->unit_number < periph->unit_number)
		cur_periph = TAILQ_NEXT(cur_periph, unit_links);

	if (cur_periph != NULL)
		TAILQ_INSERT_BEFORE(cur_periph, periph, unit_links);
	else {
		TAILQ_INSERT_TAIL(&(*p_drv)->units, periph, unit_links);
		(*p_drv)->generation++;
	}

	splx(s);

	init_level++;

	status = periph_ctor(periph, arg);

	if (status == CAM_REQ_CMP)
		init_level++;

failure:
	switch (init_level) {
	case 4:
		/* Initialized successfully */
		break;
	case 3:
		s = splsoftcam();
		TAILQ_REMOVE(&(*p_drv)->units, periph, unit_links);
		splx(s);
		xpt_remove_periph(periph);
	case 2:
		xpt_free_path(periph->path);
	case 1:
		free(periph, M_DEVBUF);
	case 0:
		/* No cleanup to perform. */
		break;
	default:
		panic("cam_periph_alloc: Unkown init level");
	}
	return(status);
}

/*
 * Find a peripheral structure with the specified path, target, lun, 
 * and (optionally) type.  If the name is NULL, this function will return
 * the first peripheral driver that matches the specified path.
 */
struct cam_periph *
cam_periph_find(struct cam_path *path, char *name)
{
	struct periph_driver **p_drv;
	struct cam_periph *periph;
	int s;

	for (p_drv = (struct periph_driver **)periphdriver_set.ls_items;
	     *p_drv != NULL; p_drv++) {

		if (name != NULL && (strcmp((*p_drv)->driver_name, name) != 0))
			continue;

		s = splsoftcam();
		for (periph = TAILQ_FIRST(&(*p_drv)->units); periph != NULL;
		     periph = TAILQ_NEXT(periph, unit_links)) {
			if (xpt_path_comp(periph->path, path) == 0) {
				splx(s);
				return(periph);
			}
		}
		splx(s);
		if (name != NULL)
			return(NULL);
	}
	return(NULL);
}

cam_status
cam_periph_acquire(struct cam_periph *periph)
{
	int s;

	if (periph == NULL)
		return(CAM_REQ_CMP_ERR);

	s = splsoftcam();
	periph->refcount++;
	splx(s);

	return(CAM_REQ_CMP);
}

void
cam_periph_release(struct cam_periph *periph)
{
	int s;

	if (periph == NULL)
		return;

	s = splsoftcam();
	if ((--periph->refcount == 0)
	 && (periph->flags & CAM_PERIPH_INVALID)) {
		camperiphfree(periph);
	}
	splx(s);

}

/*
 * Look for the next unit number that is not currently in use for this
 * peripheral type starting at "newunit".  Also exclude unit numbers that
 * are reserved by for future "hardwiring" unless we already know that this
 * is a potential wired device.  Only assume that the device is "wired" the
 * first time through the loop since after that we'll be looking at unit
 * numbers that did not match a wiring entry.
 */
static u_int
camperiphnextunit(struct periph_driver *p_drv, u_int newunit, int wired,
		  path_id_t pathid, target_id_t target, lun_id_t lun)
{
	struct	cam_periph *periph;
	char	*periph_name, *strval;
	int	s;
	int	i, val, dunit;
	const char *dname;

	s = splsoftcam();
	periph_name = p_drv->driver_name;
	for (;;newunit++) {

		for (periph = TAILQ_FIRST(&p_drv->units);
		     periph != NULL && periph->unit_number != newunit;
		     periph = TAILQ_NEXT(periph, unit_links))
			;

		if (periph != NULL && periph->unit_number == newunit) {
			if (wired != 0) {
				xpt_print_path(periph->path);
				printf("Duplicate Wired Device entry!\n");
				xpt_print_path(periph->path);
				printf("Second device (%s device at scbus%d "
				       "target %d lun %d) will not be wired\n",
				       periph_name, pathid, target, lun);
				wired = 0;
			}
			continue;
		}
		if (wired)
			break;

		/*
		 * Don't match entries like "da 4" as a wired down
		 * device, but do match entries like "da 4 target 5"
		 * or even "da 4 scbus 1". 
		 */
		i = -1;
		while ((i = resource_locate(i, periph_name)) != -1) {
			dname = resource_query_name(i);
			dunit = resource_query_unit(i);
			/* if no "target" and no specific scbus, skip */
			if (resource_int_value(dname, dunit, "target", &val) &&
			    (resource_string_value(dname, dunit, "at",&strval)||
			     strcmp(strval, "scbus") == 0))
				continue;
			if (newunit == dunit)
				break;
		}
		if (i == -1)
			break;
	}
	splx(s);
	return (newunit);
}

static u_int
camperiphunit(struct periph_driver *p_drv, path_id_t pathid,
	      target_id_t target, lun_id_t lun)
{
	u_int	unit;
	int	hit, i, val, dunit;
	const char *dname;
	char	pathbuf[32], *strval, *periph_name;

	unit = 0;
	hit = 0;

	periph_name = p_drv->driver_name;
	snprintf(pathbuf, sizeof(pathbuf), "scbus%d", pathid);
	i = -1;
	while ((i = resource_locate(i, periph_name)) != -1) {
		dname = resource_query_name(i);
		dunit = resource_query_unit(i);
		if (resource_string_value(dname, dunit, "at", &strval) == 0) {
			if (strcmp(strval, pathbuf) != 0)
				continue;
			hit++;
		}
		if (resource_int_value(dname, dunit, "target", &val) == 0) {
			if (val != target)
				continue;
			hit++;
		}
		if (resource_int_value(dname, dunit, "lun", &val) == 0) {
			if (val != lun)
				continue;
			hit++;
		}
		if (hit != 0) {
			unit = dunit;
			break;
		}
	}

	/*
	 * Either start from 0 looking for the next unit or from
	 * the unit number given in the resource config.  This way,
	 * if we have wildcard matches, we don't return the same
	 * unit number twice.
	 */
	unit = camperiphnextunit(p_drv, unit, /*wired*/hit, pathid,
				 target, lun);

	return (unit);
}

void
cam_periph_invalidate(struct cam_periph *periph)
{
	int s;

	s = splsoftcam();
	/*
	 * We only call this routine the first time a peripheral is
	 * invalidated.  The oninvalidate() routine is always called at
	 * splsoftcam().
	 */
	if (((periph->flags & CAM_PERIPH_INVALID) == 0)
	 && (periph->periph_oninval != NULL))
		periph->periph_oninval(periph);

	periph->flags |= CAM_PERIPH_INVALID;
	periph->flags &= ~CAM_PERIPH_NEW_DEV_FOUND;

	if (periph->refcount == 0)
		camperiphfree(periph);
	else if (periph->refcount < 0)
		printf("cam_invalidate_periph: refcount < 0!!\n");
	splx(s);
}

static void
camperiphfree(struct cam_periph *periph)
{
	int s;
	struct periph_driver **p_drv;

	for (p_drv = (struct periph_driver **)periphdriver_set.ls_items;
	     *p_drv != NULL; p_drv++) {
		if (strcmp((*p_drv)->driver_name, periph->periph_name) == 0)
			break;
	}
	
	if (periph->periph_dtor != NULL)
		periph->periph_dtor(periph);
	
	s = splsoftcam();
	TAILQ_REMOVE(&(*p_drv)->units, periph, unit_links);
	(*p_drv)->generation++;
	splx(s);

	xpt_remove_periph(periph);

	if (periph->flags & CAM_PERIPH_NEW_DEV_FOUND) {
		union ccb ccb;
		void *arg;

		switch (periph->deferred_ac) {
		case AC_FOUND_DEVICE:
			ccb.ccb_h.func_code = XPT_GDEV_TYPE;
			xpt_setup_ccb(&ccb.ccb_h, periph->path, /*priority*/ 1);
			xpt_action(&ccb);
			arg = &ccb;
			break;
		case AC_PATH_REGISTERED:
			ccb.ccb_h.func_code = XPT_PATH_INQ;
			xpt_setup_ccb(&ccb.ccb_h, periph->path, /*priority*/ 1);
			xpt_action(&ccb);
			arg = &ccb;
			break;
		default:
			arg = NULL;
			break;
		}
		periph->deferred_callback(NULL, periph->deferred_ac,
					  periph->path, arg);
	}
	xpt_free_path(periph->path);
	free(periph, M_DEVBUF);
}

/*
 * Wait interruptibly for an exclusive lock.
 */
int
cam_periph_lock(struct cam_periph *periph, int priority)
{
	int error;

	while ((periph->flags & CAM_PERIPH_LOCKED) != 0) {
		periph->flags |= CAM_PERIPH_LOCK_WANTED;
		if ((error = tsleep(periph, priority, "caplck", 0)) != 0)
			return error;
	}

	if (cam_periph_acquire(periph) != CAM_REQ_CMP)
		return(ENXIO);

	periph->flags |= CAM_PERIPH_LOCKED;
	return 0;
}

/*
 * Unlock and wake up any waiters.
 */
void
cam_periph_unlock(struct cam_periph *periph)
{
	periph->flags &= ~CAM_PERIPH_LOCKED;
	if ((periph->flags & CAM_PERIPH_LOCK_WANTED) != 0) {
		periph->flags &= ~CAM_PERIPH_LOCK_WANTED;
		wakeup(periph);
	}

	cam_periph_release(periph);
}

/*
 * Map user virtual pointers into kernel virtual address space, so we can
 * access the memory.  This won't work on physical pointers, for now it's
 * up to the caller to check for that.  (XXX KDM -- should we do that here
 * instead?)  This also only works for up to MAXPHYS memory.  Since we use
 * buffers to map stuff in and out, we're limited to the buffer size.
 */
int
cam_periph_mapmem(union ccb *ccb, struct cam_periph_map_info *mapinfo)
{
	int numbufs, i;
	int flags[CAM_PERIPH_MAXMAPS];
	u_int8_t **data_ptrs[CAM_PERIPH_MAXMAPS];
	u_int32_t lengths[CAM_PERIPH_MAXMAPS];
	u_int32_t dirs[CAM_PERIPH_MAXMAPS];

	switch(ccb->ccb_h.func_code) {
	case XPT_DEV_MATCH:
		if (ccb->cdm.match_buf_len == 0) {
			printf("cam_periph_mapmem: invalid match buffer "
			       "length 0\n");
			return(EINVAL);
		}
		if (ccb->cdm.pattern_buf_len > 0) {
			data_ptrs[0] = (u_int8_t **)&ccb->cdm.patterns;
			lengths[0] = ccb->cdm.pattern_buf_len;
			dirs[0] = CAM_DIR_OUT;
			data_ptrs[1] = (u_int8_t **)&ccb->cdm.matches;
			lengths[1] = ccb->cdm.match_buf_len;
			dirs[1] = CAM_DIR_IN;
			numbufs = 2;
		} else {
			data_ptrs[0] = (u_int8_t **)&ccb->cdm.matches;
			lengths[0] = ccb->cdm.match_buf_len;
			dirs[0] = CAM_DIR_IN;
			numbufs = 1;
		}
		break;
	case XPT_SCSI_IO:
	case XPT_CONT_TARGET_IO:
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_NONE)
			return(0);

		data_ptrs[0] = &ccb->csio.data_ptr;
		lengths[0] = ccb->csio.dxfer_len;
		dirs[0] = ccb->ccb_h.flags & CAM_DIR_MASK;
		numbufs = 1;
		break;
	default:
		return(EINVAL);
		break; /* NOTREACHED */
	}

	/*
	 * Check the transfer length and permissions first, so we don't
	 * have to unmap any previously mapped buffers.
	 */
	for (i = 0; i < numbufs; i++) {

		flags[i] = 0;

		/*
		 * The userland data pointer passed in may not be page
		 * aligned.  vmapbuf() truncates the address to a page
		 * boundary, so if the address isn't page aligned, we'll
		 * need enough space for the given transfer length, plus
		 * whatever extra space is necessary to make it to the page
		 * boundary.
		 */
		if ((lengths[i] +
		    (((vm_offset_t)(*data_ptrs[i])) & PAGE_MASK)) > DFLTPHYS){
			printf("cam_periph_mapmem: attempt to map %lu bytes, "
			       "which is greater than DFLTPHYS(%d)\n",
			       (long)(lengths[i] +
			       (((vm_offset_t)(*data_ptrs[i])) & PAGE_MASK)),
			       DFLTPHYS);
			return(E2BIG);
		}

		if (dirs[i] & CAM_DIR_OUT) {
			flags[i] = BIO_WRITE;
			if (!useracc(*data_ptrs[i], lengths[i], 
				     VM_PROT_READ)) {
				printf("cam_periph_mapmem: error, "
					"address %p, length %lu isn't "
					"user accessible for READ\n",
					(void *)*data_ptrs[i],
					(u_long)lengths[i]);
				return(EACCES);
			}
		}

		if (dirs[i] & CAM_DIR_IN) {
			flags[i] = BIO_READ;
			if (!useracc(*data_ptrs[i], lengths[i], 
				     VM_PROT_WRITE)) {
				printf("cam_periph_mapmem: error, "
					"address %p, length %lu isn't "
					"user accessible for WRITE\n",
					(void *)*data_ptrs[i],
					(u_long)lengths[i]);

				return(EACCES);
			}
		}

	}

	/* this keeps the current process from getting swapped */
	/*
	 * XXX KDM should I use P_NOSWAP instead?
	 */
	PHOLD(curproc);

	for (i = 0; i < numbufs; i++) {
		/*
		 * Get the buffer.
		 */
		mapinfo->bp[i] = getpbuf(NULL);

		/* save the buffer's data address */
		mapinfo->bp[i]->b_saveaddr = mapinfo->bp[i]->b_data;

		/* put our pointer in the data slot */
		mapinfo->bp[i]->b_data = *data_ptrs[i];

		/* set the transfer length, we know it's < DFLTPHYS */
		mapinfo->bp[i]->b_bufsize = lengths[i];

		/* set the flags */
		mapinfo->bp[i]->b_flags = B_PHYS;

		/* set the direction */
		mapinfo->bp[i]->b_iocmd = flags[i];

		/* map the buffer into kernel memory */
		vmapbuf(mapinfo->bp[i]);

		/* set our pointer to the new mapped area */
		*data_ptrs[i] = mapinfo->bp[i]->b_data;

		mapinfo->num_bufs_used++;
	}

	return(0);
}

/*
 * Unmap memory segments mapped into kernel virtual address space by
 * cam_periph_mapmem().
 */
void
cam_periph_unmapmem(union ccb *ccb, struct cam_periph_map_info *mapinfo)
{
	int numbufs, i;
	u_int8_t **data_ptrs[CAM_PERIPH_MAXMAPS];

	if (mapinfo->num_bufs_used <= 0) {
		/* allow ourselves to be swapped once again */
		PRELE(curproc);
		return;
	}

	switch (ccb->ccb_h.func_code) {
	case XPT_DEV_MATCH:
		numbufs = min(mapinfo->num_bufs_used, 2);

		if (numbufs == 1) {
			data_ptrs[0] = (u_int8_t **)&ccb->cdm.matches;
		} else {
			data_ptrs[0] = (u_int8_t **)&ccb->cdm.patterns;
			data_ptrs[1] = (u_int8_t **)&ccb->cdm.matches;
		}
		break;
	case XPT_SCSI_IO:
	case XPT_CONT_TARGET_IO:
		data_ptrs[0] = &ccb->csio.data_ptr;
		numbufs = min(mapinfo->num_bufs_used, 1);
		break;
	default:
		/* allow ourselves to be swapped once again */
		PRELE(curproc);
		return;
		break; /* NOTREACHED */ 
	}

	for (i = 0; i < numbufs; i++) {
		/* Set the user's pointer back to the original value */
		*data_ptrs[i] = mapinfo->bp[i]->b_saveaddr;

		/* unmap the buffer */
		vunmapbuf(mapinfo->bp[i]);

		/* clear the flags we set above */
		mapinfo->bp[i]->b_flags &= ~B_PHYS;

		/* release the buffer */
		relpbuf(mapinfo->bp[i], NULL);
	}

	/* allow ourselves to be swapped once again */
	PRELE(curproc);
}

union ccb *
cam_periph_getccb(struct cam_periph *periph, u_int32_t priority)
{
	struct ccb_hdr *ccb_h;
	int s;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("entering cdgetccb\n"));

	s = splsoftcam();
	
	while (periph->ccb_list.slh_first == NULL) {
		if (periph->immediate_priority > priority)
			periph->immediate_priority = priority;
		xpt_schedule(periph, priority);
		if ((periph->ccb_list.slh_first != NULL)
		 && (periph->ccb_list.slh_first->pinfo.priority == priority))
			break;
		tsleep(&periph->ccb_list, PRIBIO, "cgticb", 0);
	}

	ccb_h = periph->ccb_list.slh_first;
	SLIST_REMOVE_HEAD(&periph->ccb_list, periph_links.sle);
	splx(s);
	return ((union ccb *)ccb_h);
}

void
cam_periph_ccbwait(union ccb *ccb)
{
	int s;

	s = splsoftcam();
	if ((ccb->ccb_h.pinfo.index != CAM_UNQUEUED_INDEX)
	 || ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INPROG))
		tsleep(&ccb->ccb_h.cbfcnp, PRIBIO, "cbwait", 0);

	splx(s);
}

int
cam_periph_ioctl(struct cam_periph *periph, int cmd, caddr_t addr,
		 int (*error_routine)(union ccb *ccb, 
				      cam_flags camflags,
				      u_int32_t sense_flags))
{
	union ccb 	     *ccb;
	int 		     error;
	int		     found;

	error = found = 0;

	switch(cmd){
	case CAMGETPASSTHRU:
		ccb = cam_periph_getccb(periph, /* priority */ 1);
		xpt_setup_ccb(&ccb->ccb_h,
			      ccb->ccb_h.path,
			      /*priority*/1);
		ccb->ccb_h.func_code = XPT_GDEVLIST;

		/*
		 * Basically, the point of this is that we go through
		 * getting the list of devices, until we find a passthrough
		 * device.  In the current version of the CAM code, the
		 * only way to determine what type of device we're dealing
		 * with is by its name.
		 */
		while (found == 0) {
			ccb->cgdl.index = 0;
			ccb->cgdl.status = CAM_GDEVLIST_MORE_DEVS;
			while (ccb->cgdl.status == CAM_GDEVLIST_MORE_DEVS) {

				/* we want the next device in the list */
				xpt_action(ccb);
				if (strncmp(ccb->cgdl.periph_name, 
				    "pass", 4) == 0){
					found = 1;
					break;
				}
			}
			if ((ccb->cgdl.status == CAM_GDEVLIST_LAST_DEVICE) &&
			    (found == 0)) {
				ccb->cgdl.periph_name[0] = '\0';
				ccb->cgdl.unit_number = 0;
				break;
			}
		}

		/* copy the result back out */	
		bcopy(ccb, addr, sizeof(union ccb));

		/* and release the ccb */
		xpt_release_ccb(ccb);

		break;
	default:
		error = ENOTTY;
		break;
	}
	return(error);
}

int
cam_periph_runccb(union ccb *ccb,
		  int (*error_routine)(union ccb *ccb,
				       cam_flags camflags,
				       u_int32_t sense_flags),
		  cam_flags camflags, u_int32_t sense_flags,
		  struct devstat *ds)
{
	int error;
 
	error = 0;
        
	/*
	 * If the user has supplied a stats structure, and if we understand
	 * this particular type of ccb, record the transaction start.
	 */
	if ((ds != NULL) && (ccb->ccb_h.func_code == XPT_SCSI_IO))
		devstat_start_transaction(ds);

	xpt_action(ccb);
 
	do {
		cam_periph_ccbwait(ccb);
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP)
			error = 0;
		else if (error_routine != NULL)
			error = (*error_routine)(ccb, camflags, sense_flags);
		else
			error = 0;

	} while (error == ERESTART);
          
	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) 
		cam_release_devq(ccb->ccb_h.path,
				 /* relsim_flags */0,
				 /* openings */0,
				 /* timeout */0,
				 /* getcount_only */ FALSE);

	if ((ds != NULL) && (ccb->ccb_h.func_code == XPT_SCSI_IO))
		devstat_end_transaction(ds,
					ccb->csio.dxfer_len,
					ccb->csio.tag_action & 0xf,
					((ccb->ccb_h.flags & CAM_DIR_MASK) ==
					CAM_DIR_NONE) ?  DEVSTAT_NO_DATA : 
					(ccb->ccb_h.flags & CAM_DIR_OUT) ?
					DEVSTAT_WRITE : 
					DEVSTAT_READ);

	return(error);
}

void
cam_freeze_devq(struct cam_path *path)
{
	struct ccb_hdr ccb_h;

	xpt_setup_ccb(&ccb_h, path, /*priority*/1);
	ccb_h.func_code = XPT_NOOP;
	ccb_h.flags = CAM_DEV_QFREEZE;
	xpt_action((union ccb *)&ccb_h);
}

u_int32_t
cam_release_devq(struct cam_path *path, u_int32_t relsim_flags,
		 u_int32_t openings, u_int32_t timeout,
		 int getcount_only)
{
	struct ccb_relsim crs;

	xpt_setup_ccb(&crs.ccb_h, path,
		      /*priority*/1);
	crs.ccb_h.func_code = XPT_REL_SIMQ;
	crs.ccb_h.flags = getcount_only ? CAM_DEV_QFREEZE : 0;
	crs.release_flags = relsim_flags;
	crs.openings = openings;
	crs.release_timeout = timeout;
	xpt_action((union ccb *)&crs);
	return (crs.qfrozen_cnt);
}

#define saved_ccb_ptr ppriv_ptr0
static void
camperiphdone(struct cam_periph *periph, union ccb *done_ccb)
{
	cam_status	status;
	int		frozen;
	int		sense;
	struct scsi_start_stop_unit *scsi_cmd;
	u_int32_t	relsim_flags, timeout;
	u_int32_t	qfrozen_cnt;

	status = done_ccb->ccb_h.status;
	frozen = (status & CAM_DEV_QFRZN) != 0;
	sense  = (status & CAM_AUTOSNS_VALID) != 0;
	status &= CAM_STATUS_MASK;

	timeout = 0;
	relsim_flags = 0;

	/* 
	 * Unfreeze the queue once if it is already frozen..
	 */
	if (frozen != 0) {
		qfrozen_cnt = cam_release_devq(done_ccb->ccb_h.path,
					      /*relsim_flags*/0,
					      /*openings*/0,
					      /*timeout*/0,
					      /*getcount_only*/0);
	}

	switch (status) {

	case CAM_REQ_CMP:

		/*
		 * If we have successfully taken a device from the not
		 * ready to ready state, re-scan the device and re-get the
		 * inquiry information.  Many devices (mostly disks) don't
		 * properly report their inquiry information unless they
		 * are spun up.
		 */
		if (done_ccb->ccb_h.func_code == XPT_SCSI_IO) {
			scsi_cmd = (struct scsi_start_stop_unit *)
					&done_ccb->csio.cdb_io.cdb_bytes;

		 	if (scsi_cmd->opcode == START_STOP_UNIT)
				xpt_async(AC_INQ_CHANGED,
					  done_ccb->ccb_h.path, NULL);
		}
		bcopy(done_ccb->ccb_h.saved_ccb_ptr, done_ccb,
		      sizeof(union ccb));

		periph->flags &= ~CAM_PERIPH_RECOVERY_INPROG;

		xpt_action(done_ccb);

		break;
	case CAM_SCSI_STATUS_ERROR:
		scsi_cmd = (struct scsi_start_stop_unit *)
				&done_ccb->csio.cdb_io.cdb_bytes;
		if (sense != 0) {
			struct scsi_sense_data *sense;
			int    error_code, sense_key, asc, ascq;	

			sense = &done_ccb->csio.sense_data;
			scsi_extract_sense(sense, &error_code, 
					   &sense_key, &asc, &ascq);

			/*
	 		 * If the error is "invalid field in CDB", 
			 * and the load/eject flag is set, turn the 
			 * flag off and try again.  This is just in 
			 * case the drive in question barfs on the 
			 * load eject flag.  The CAM code should set 
			 * the load/eject flag by default for 
			 * removable media.
			 */

			/* XXX KDM 
			 * Should we check to see what the specific
			 * scsi status is??  Or does it not matter
			 * since we already know that there was an
			 * error, and we know what the specific
			 * error code was, and we know what the
			 * opcode is..
			 */
			if ((scsi_cmd->opcode == START_STOP_UNIT) &&
			    ((scsi_cmd->how & SSS_LOEJ) != 0) &&
			     (asc == 0x24) && (ascq == 0x00) &&
			     (done_ccb->ccb_h.retry_count > 0)) {

				scsi_cmd->how &= ~SSS_LOEJ;

				xpt_action(done_ccb);

			} else if (done_ccb->ccb_h.retry_count > 0) {
				/*
				 * In this case, the error recovery
				 * command failed, but we've got 
				 * some retries left on it.  Give
				 * it another try.
				 */

				/* set the timeout to .5 sec */
				relsim_flags =
					RELSIM_RELEASE_AFTER_TIMEOUT;
				timeout = 500;

				xpt_action(done_ccb);

				break;

			} else {
				/* 
				 * Copy the original CCB back and
				 * send it back to the caller.
				 */
				bcopy(done_ccb->ccb_h.saved_ccb_ptr,		
				      done_ccb, sizeof(union ccb));

				periph->flags &= ~CAM_PERIPH_RECOVERY_INPROG;

				xpt_action(done_ccb);
			}
		} else {
			/*
			 * Eh??  The command failed, but we don't
			 * have any sense.  What's up with that?
			 * Fire the CCB again to return it to the
			 * caller.
			 */
			bcopy(done_ccb->ccb_h.saved_ccb_ptr,
			      done_ccb, sizeof(union ccb));

			periph->flags &= ~CAM_PERIPH_RECOVERY_INPROG;

			xpt_action(done_ccb);

		}
		break;
	default:
		bcopy(done_ccb->ccb_h.saved_ccb_ptr, done_ccb,
		      sizeof(union ccb));

		periph->flags &= ~CAM_PERIPH_RECOVERY_INPROG;

		xpt_action(done_ccb);

		break;
	}

	/* decrement the retry count */
	if (done_ccb->ccb_h.retry_count > 0)
		done_ccb->ccb_h.retry_count--;

	qfrozen_cnt = cam_release_devq(done_ccb->ccb_h.path,
				      /*relsim_flags*/relsim_flags,
				      /*openings*/0,
				      /*timeout*/timeout,
				      /*getcount_only*/0);
}

/*
 * Generic Async Event handler.  Peripheral drivers usually
 * filter out the events that require personal attention,
 * and leave the rest to this function.
 */
void
cam_periph_async(struct cam_periph *periph, u_int32_t code,
		 struct cam_path *path, void *arg)
{
	switch (code) {
	case AC_LOST_DEVICE:
		cam_periph_invalidate(periph);
		break; 
	case AC_SENT_BDR:
	case AC_BUS_RESET:
	{
		cam_periph_bus_settle(periph, SCSI_DELAY);
		break;
	}
	default:
		break;
	}
}

void
cam_periph_bus_settle(struct cam_periph *periph, u_int bus_settle)
{
	struct ccb_getdevstats cgds;

	xpt_setup_ccb(&cgds.ccb_h, periph->path, /*priority*/1);
	cgds.ccb_h.func_code = XPT_GDEV_STATS;
	xpt_action((union ccb *)&cgds);
	cam_periph_freeze_after_event(periph, &cgds.last_reset, bus_settle);
}

void
cam_periph_freeze_after_event(struct cam_periph *periph,
			      struct timeval* event_time, u_int duration_ms)
{
	struct timeval delta;
	struct timeval duration_tv;
	int s;

	s = splclock();
	microtime(&delta);
	splx(s);
	timevalsub(&delta, event_time);
	duration_tv.tv_sec = duration_ms / 1000;
	duration_tv.tv_usec = (duration_ms % 1000) * 1000;
	if (timevalcmp(&delta, &duration_tv, <)) {
		timevalsub(&duration_tv, &delta);

		duration_ms = duration_tv.tv_sec * 1000;
		duration_ms += duration_tv.tv_usec / 1000;
		cam_freeze_devq(periph->path); 
		cam_release_devq(periph->path,
				RELSIM_RELEASE_AFTER_TIMEOUT,
				/*reduction*/0,
				/*timeout*/duration_ms,
				/*getcount_only*/0);
	}

}

/*
 * Generic error handler.  Peripheral drivers usually filter
 * out the errors that they handle in a unique mannor, then
 * call this function.
 */
int
cam_periph_error(union ccb *ccb, cam_flags camflags,
		 u_int32_t sense_flags, union ccb *save_ccb)
{
	cam_status status;
	int	   frozen;
	int	   sense;
	int	   error;
	int        openings;
	int	   retry;
	u_int32_t  relsim_flags;
	u_int32_t  timeout;
	
	status = ccb->ccb_h.status;
	frozen = (status & CAM_DEV_QFRZN) != 0;
	sense  = (status & CAM_AUTOSNS_VALID) != 0;
	status &= CAM_STATUS_MASK;
	relsim_flags = 0;

	switch (status) {
	case CAM_REQ_CMP:
		/* decrement the number of retries */
		retry = ccb->ccb_h.retry_count > 0;
		if (retry)
			ccb->ccb_h.retry_count--;
		error = 0;
		break;
	case CAM_AUTOSENSE_FAIL:
	case CAM_SCSI_STATUS_ERROR:

		switch (ccb->csio.scsi_status) {
		case SCSI_STATUS_OK:
		case SCSI_STATUS_COND_MET:
		case SCSI_STATUS_INTERMED:
		case SCSI_STATUS_INTERMED_COND_MET:
			error = 0;
			break;
		case SCSI_STATUS_CMD_TERMINATED:
		case SCSI_STATUS_CHECK_COND:
			if (sense != 0) {
				struct scsi_sense_data *sense;
				int    error_code, sense_key, asc, ascq;
				struct cam_periph *periph;
				scsi_sense_action err_action;
				struct ccb_getdev cgd;

				sense = &ccb->csio.sense_data;
				scsi_extract_sense(sense, &error_code,
						   &sense_key, &asc, &ascq);
				periph = xpt_path_periph(ccb->ccb_h.path);

				/*
				 * Grab the inquiry data for this device.
				 */
				xpt_setup_ccb(&cgd.ccb_h, ccb->ccb_h.path,
					      /*priority*/ 1);
				cgd.ccb_h.func_code = XPT_GDEV_TYPE;
				xpt_action((union ccb *)&cgd);

				err_action = scsi_error_action(asc, ascq, 
							       &cgd.inq_data);

				/*
				 * Send a Test Unit Ready to the device.
				 * If the 'many' flag is set, we send 120
				 * test unit ready commands, one every half 
				 * second.  Otherwise, we just send one TUR.
				 * We only want to do this if the retry 
				 * count has not been exhausted.
				 */
				if (((err_action & SS_MASK) == SS_TUR)
				 && save_ccb != NULL 
				 && ccb->ccb_h.retry_count > 0) {

					/*
					 * Since error recovery is already
					 * in progress, don't attempt to
					 * process this error.  It is probably
					 * related to the error that caused
					 * the currently active error recovery
					 * action.  Also, we only have
					 * space for one saved CCB, so if we
					 * had two concurrent error recovery
					 * actions, we would end up
					 * over-writing one error recovery
					 * CCB with another one.
					 */
					if (periph->flags &
					    CAM_PERIPH_RECOVERY_INPROG) {
						error = ERESTART;
						break;
					}

					periph->flags |=
						CAM_PERIPH_RECOVERY_INPROG;

					/* decrement the number of retries */
					if ((err_action & 
					     SSQ_DECREMENT_COUNT) != 0) {
						retry = 1;
						ccb->ccb_h.retry_count--;
					}

					bcopy(ccb, save_ccb, sizeof(*save_ccb));

					/*
					 * We retry this one every half
					 * second for a minute.  If the
					 * device hasn't become ready in a
					 * minute's time, it's unlikely to
					 * ever become ready.  If the table
					 * doesn't specify SSQ_MANY, we can
					 * only try this once.  Oh well.
					 */
					if ((err_action & SSQ_MANY) != 0)
						scsi_test_unit_ready(&ccb->csio,
							       /*retries*/120,
							       camperiphdone,
						 	       MSG_SIMPLE_Q_TAG,
							       SSD_FULL_SIZE,
							       /*timeout*/5000);
					else
						scsi_test_unit_ready(&ccb->csio,
							       /*retries*/1,
							       camperiphdone,
						 	       MSG_SIMPLE_Q_TAG,
							       SSD_FULL_SIZE,
							       /*timeout*/5000);

					/* release the queue after .5 sec.  */
					relsim_flags = 
						RELSIM_RELEASE_AFTER_TIMEOUT;
					timeout = 500;
					/*
					 * Drop the priority to 0 so that 
					 * we are the first to execute.  Also 
					 * freeze the queue after this command 
					 * is sent so that we can restore the 
					 * old csio and have it queued in the 
					 * proper order before we let normal 
					 * transactions go to the drive.
					 */
					ccb->ccb_h.pinfo.priority = 0;
					ccb->ccb_h.flags |= CAM_DEV_QFREEZE;

					/*
					 * Save a pointer to the original
					 * CCB in the new CCB.
					 */
					ccb->ccb_h.saved_ccb_ptr = save_ccb;

					error = ERESTART;
				}
				/*
				 * Send a start unit command to the device,
				 * and then retry the command.  We only 
				 * want to do this if the retry count has 
				 * not been exhausted.  If the user 
				 * specified 0 retries, then we follow 
				 * their request and do not retry.
				 */
				else if (((err_action & SS_MASK) == SS_START)
				      && save_ccb != NULL 
				      && ccb->ccb_h.retry_count > 0) {
					int le;

					/*
					 * Only one error recovery action
					 * at a time.  See above.
					 */
					if (periph->flags &
					    CAM_PERIPH_RECOVERY_INPROG) {
						error = ERESTART;
						break;
					}

					periph->flags |=
						CAM_PERIPH_RECOVERY_INPROG;

					/* decrement the number of retries */
					retry = 1;
					ccb->ccb_h.retry_count--;

					/*
					 * Check for removable media and
					 * set load/eject flag
					 * appropriately.
					 */
					if (SID_IS_REMOVABLE(&cgd.inq_data))
						le = TRUE;
					else
						le = FALSE;

					/*
					 * Attempt to start the drive up.
					 *
					 * Save the current ccb so it can 
					 * be restored and retried once the 
					 * drive is started up.
					 */
					bcopy(ccb, save_ccb, sizeof(*save_ccb));

					scsi_start_stop(&ccb->csio,
							/*retries*/1,
							camperiphdone,
							MSG_SIMPLE_Q_TAG,
							/*start*/TRUE,
							/*load/eject*/le,
							/*immediate*/FALSE,
							SSD_FULL_SIZE,
							/*timeout*/50000);
					/*
					 * Drop the priority to 0 so that 
					 * we are the first to execute.  Also 
					 * freeze the queue after this command 
					 * is sent so that we can restore the 
					 * old csio and have it queued in the 
					 * proper order before we let normal 
					 * transactions go to the drive.
					 */
					ccb->ccb_h.pinfo.priority = 0;
					ccb->ccb_h.flags |= CAM_DEV_QFREEZE;

					/*
					 * Save a pointer to the original
					 * CCB in the new CCB.
					 */
					ccb->ccb_h.saved_ccb_ptr = save_ccb;

					error = ERESTART;
				} else if ((sense_flags & SF_RETRY_UA) != 0) {
					/*
					 * XXX KDM this is a *horrible*
					 * hack.  
					 */
					error = scsi_interpret_sense(ccb,
								  sense_flags,
								  &relsim_flags,
								  &openings,
								  &timeout,
								  err_action);
				} 

				/*
				 * Theoretically, this code should send a
				 * test unit ready to the given device, and 
				 * if it returns and error, send a start 
				 * unit command.  Since we don't yet have
				 * the capability to do two-command error
				 * recovery, just send a start unit.
				 * XXX KDM fix this!
				 */
				else if (((err_action & SS_MASK) == SS_TURSTART)
				      && save_ccb != NULL
				      && ccb->ccb_h.retry_count > 0) {
					int le;

					/*
					 * Only one error recovery action
					 * at a time.  See above.
					 */
					if (periph->flags &
					    CAM_PERIPH_RECOVERY_INPROG) {
						error = ERESTART;
						break;
					}

					periph->flags |=
						CAM_PERIPH_RECOVERY_INPROG;

					/* decrement the number of retries */
					retry = 1;
					ccb->ccb_h.retry_count--;

					/*
					 * Check for removable media and
					 * set load/eject flag
					 * appropriately.
					 */
					if (SID_IS_REMOVABLE(&cgd.inq_data))
						le = TRUE;
					else
						le = FALSE;

					/*
					 * Attempt to start the drive up.
					 *
					 * Save the current ccb so it can 
					 * be restored and retried once the 
					 * drive is started up.
					 */
					bcopy(ccb, save_ccb, sizeof(*save_ccb));

					scsi_start_stop(&ccb->csio,
							/*retries*/1,
							camperiphdone,
							MSG_SIMPLE_Q_TAG,
							/*start*/TRUE,
							/*load/eject*/le,
							/*immediate*/FALSE,
							SSD_FULL_SIZE,
							/*timeout*/50000);

					/* release the queue after .5 sec.  */
					relsim_flags = 
						RELSIM_RELEASE_AFTER_TIMEOUT;
					timeout = 500;
					/*
					 * Drop the priority to 0 so that 
					 * we are the first to execute.  Also 
					 * freeze the queue after this command 
					 * is sent so that we can restore the 
					 * old csio and have it queued in the 
					 * proper order before we let normal 
					 * transactions go to the drive.
					 */
					ccb->ccb_h.pinfo.priority = 0;
					ccb->ccb_h.flags |= CAM_DEV_QFREEZE;

					/*
					 * Save a pointer to the original
					 * CCB in the new CCB.
					 */
					ccb->ccb_h.saved_ccb_ptr = save_ccb;

					error = ERESTART;
				} else {
					error = scsi_interpret_sense(ccb,
								  sense_flags,
								  &relsim_flags,
								  &openings,
								  &timeout,
								  err_action);
				}
			} else if (ccb->csio.scsi_status == 
				   SCSI_STATUS_CHECK_COND
				&& status != CAM_AUTOSENSE_FAIL) {
				/* no point in decrementing the retry count */
				panic("cam_periph_error: scsi status of "
				      "CHECK COND returned but no sense "
				      "information is availible.  "
				      "Controller should have returned "
				      "CAM_AUTOSENSE_FAILED");
				/* NOTREACHED */
				error = EIO;
			} else if (ccb->ccb_h.retry_count == 0) {
				/*
				 * XXX KDM shouldn't there be a better
				 * argument to return??
				 */
				error = EIO;
			} else {
				/* decrement the number of retries */
				retry = ccb->ccb_h.retry_count > 0;
				if (retry)
					ccb->ccb_h.retry_count--;
				/*
				 * If it was aborted with no
				 * clue as to the reason, just
				 * retry it again.
				 */
				error = ERESTART;
			}
			break;
		case SCSI_STATUS_QUEUE_FULL:
		{
			/* no decrement */
			struct ccb_getdevstats cgds;

			/*
			 * First off, find out what the current
			 * transaction counts are.
			 */
			xpt_setup_ccb(&cgds.ccb_h,
				      ccb->ccb_h.path,
				      /*priority*/1);
			cgds.ccb_h.func_code = XPT_GDEV_STATS;
			xpt_action((union ccb *)&cgds);

			/*
			 * If we were the only transaction active, treat
			 * the QUEUE FULL as if it were a BUSY condition.
			 */
			if (cgds.dev_active != 0) {
				int total_openings;

				/*
			 	 * Reduce the number of openings to
				 * be 1 less than the amount it took
				 * to get a queue full bounded by the
				 * minimum allowed tag count for this
				 * device.
			 	 */
				total_openings =
				    cgds.dev_active+cgds.dev_openings;
				openings = cgds.dev_active;
				if (openings < cgds.mintags)
					openings = cgds.mintags;
				if (openings < total_openings)
					relsim_flags = RELSIM_ADJUST_OPENINGS;
				else {
					/*
					 * Some devices report queue full for
					 * temporary resource shortages.  For
					 * this reason, we allow a minimum
					 * tag count to be entered via a
					 * quirk entry to prevent the queue
					 * count on these devices from falling
					 * to a pessimisticly low value.  We
					 * still wait for the next successful
					 * completion, however, before queueing
					 * more transactions to the device.
					 */
					relsim_flags =
					    RELSIM_RELEASE_AFTER_CMDCMPLT;
				}
				timeout = 0;
				error = ERESTART;
				break;
			}
			/* FALLTHROUGH */
		}
		case SCSI_STATUS_BUSY:
			/*
			 * Restart the queue after either another
			 * command completes or a 1 second timeout.
			 * If we have any retries left, that is.
			 */
			retry = ccb->ccb_h.retry_count > 0;
			if (retry) {
				ccb->ccb_h.retry_count--;
				error = ERESTART;
				relsim_flags = RELSIM_RELEASE_AFTER_TIMEOUT
					     | RELSIM_RELEASE_AFTER_CMDCMPLT;
				timeout = 1000;
			} else {
				error = EIO;
			}
			break;
		case SCSI_STATUS_RESERV_CONFLICT:
			error = EIO;
			break;
		default:
			error = EIO;
			break;
		}
		break;
	case CAM_REQ_CMP_ERR:
	case CAM_CMD_TIMEOUT:
	case CAM_UNEXP_BUSFREE:
	case CAM_UNCOR_PARITY:
	case CAM_DATA_RUN_ERR:
		/* decrement the number of retries */
		retry = ccb->ccb_h.retry_count > 0;
		if (retry) {
			ccb->ccb_h.retry_count--;
			error = ERESTART;
		} else {
			error = EIO;
		}
		break;
	case CAM_UA_ABORT:
	case CAM_UA_TERMIO:
	case CAM_MSG_REJECT_REC:
		/* XXX Don't know that these are correct */
		error = EIO;
		break;
	case CAM_SEL_TIMEOUT:
	{
		/*
		 * XXX
		 * A single selection timeout should not be enough
		 * to invalidate a device.  We should retry for multiple
		 * seconds assuming this isn't a probe.  We'll probably
		 * need a special flag for that.
		 */
#if 0
		struct cam_path *newpath;

		/* Should we do more if we can't create the path?? */
		if (xpt_create_path(&newpath, xpt_path_periph(ccb->ccb_h.path),
				    xpt_path_path_id(ccb->ccb_h.path),
				    xpt_path_target_id(ccb->ccb_h.path),
				    CAM_LUN_WILDCARD) != CAM_REQ_CMP) 
			break;
		/*
		 * Let peripheral drivers know that this device has gone
		 * away.
		 */
		xpt_async(AC_LOST_DEVICE, newpath, NULL);
		xpt_free_path(newpath);
#endif
		if ((sense_flags & SF_RETRY_SELTO) != 0) {
			retry = ccb->ccb_h.retry_count > 0;
			if (retry) {
				ccb->ccb_h.retry_count--;
				error = ERESTART;
				/*
				 * Wait half a second to give the device
				 * time to recover before we try again.
				 */
				relsim_flags = RELSIM_RELEASE_AFTER_TIMEOUT;
				timeout = 500;
			} else {
				error = ENXIO;
			}
		} else {
			error = ENXIO;
		}
		break;
	}
	case CAM_REQ_INVALID:
	case CAM_PATH_INVALID:
	case CAM_DEV_NOT_THERE:
	case CAM_NO_HBA:
	case CAM_PROVIDE_FAIL:
	case CAM_REQ_TOO_BIG:		
		error = EINVAL;
		break;
	case CAM_SCSI_BUS_RESET:
	case CAM_BDR_SENT:		
	case CAM_REQUEUE_REQ:
		/* Unconditional requeue, dammit */
		error = ERESTART;
		break;
	case CAM_RESRC_UNAVAIL:
	case CAM_BUSY:
		/* timeout??? */
	default:
		/* decrement the number of retries */
		retry = ccb->ccb_h.retry_count > 0;
		if (retry) {
			ccb->ccb_h.retry_count--;
			error = ERESTART;
		} else {
			/* Check the sense codes */
			error = EIO;
		}
		break;
	}

	/* Attempt a retry */
	if (error == ERESTART || error == 0) {	
		if (frozen != 0)
			ccb->ccb_h.status &= ~CAM_DEV_QFRZN;

		if (error == ERESTART)
			xpt_action(ccb);
		
		if (frozen != 0) {
			cam_release_devq(ccb->ccb_h.path,
					 relsim_flags,
					 openings,
					 timeout,
					 /*getcount_only*/0);
		}
	}


	return (error);
}
