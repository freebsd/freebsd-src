/*-
 * Data structures and definitions for dealing with the 
 * Common Access Method Transport (xpt) layer.
 *
 * Copyright (c) 1997 Justin T. Gibbs.
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

#ifndef _CAM_CAM_XPT_H
#define _CAM_CAM_XPT_H 1

/* Forward Declarations */
union ccb;
struct cam_periph;
struct cam_sim;

/*
 * Definition of a CAM path.  Paths are created from bus, target, and lun ids
 * via xpt_create_path and allow for reference to devices without recurring
 * lookups in the edt.
 */
struct cam_path;

/* Path functions */

#ifdef _KERNEL

void			xpt_action(union ccb *new_ccb);
void			xpt_setup_ccb(struct ccb_hdr *ccb_h,
				      struct cam_path *path,
				      u_int32_t priority);
void			xpt_merge_ccb(union ccb *master_ccb,
				      union ccb *slave_ccb);
cam_status		xpt_create_path(struct cam_path **new_path_ptr,
					struct cam_periph *perph,
					path_id_t path_id,
					target_id_t target_id, lun_id_t lun_id);
void			xpt_free_path(struct cam_path *path);
int			xpt_path_comp(struct cam_path *path1,
				      struct cam_path *path2);
void			xpt_print_path(struct cam_path *path);
int			xpt_path_string(struct cam_path *path, char *str,
					size_t str_len);
path_id_t		xpt_path_path_id(struct cam_path *path);
target_id_t		xpt_path_target_id(struct cam_path *path);
lun_id_t		xpt_path_lun_id(struct cam_path *path);
struct cam_sim		*xpt_path_sim(struct cam_path *path);
struct cam_periph	*xpt_path_periph(struct cam_path *path);
void			xpt_async(u_int32_t async_code, struct cam_path *path,
				  void *async_arg);
#endif /* _KERNEL */

#endif /* _CAM_CAM_XPT_H */

