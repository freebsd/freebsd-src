/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include "xfs.h"

#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_dmapi.h"
#include "xfs_mount.h"

static int nopkg(void);

static __inline int
nopkg()
{
	return (ENOSYS);
}

int
dmapi_init (void)
{
	return (0);
}

void
dmapi_uninit (void)
{
}

int dm_data_event(void);
int
dm_data_event (void)
{
	return nopkg();
}

int dm_namesp_event(void);
int
dm_namesp_event (void)
{
	return nopkg();
}

/*	The following stubs are for routines needed for the X/Open
 *	version of DMAPI.
 */
int xfs_dm_mount(xfs_vfs_t *, xfs_vnode_t *, char *, char *); 
int
xfs_dm_mount(
	xfs_vfs_t	*vfsp,
	xfs_vnode_t	*mvp,
	char		*dir_name,
	char		*fsname)
{
	return nopkg();
}

int
dm_send_destroy_event(bhv_desc_t *bdp, dm_right_t vp_right);
int
dm_send_destroy_event(bhv_desc_t *bdp, dm_right_t vp_right)
{
	return nopkg();
}

int
dm_send_mount_event(xfs_vfs_t *vfsp, dm_right_t vfsp_right, bhv_desc_t *bdp,
	dm_right_t vp_right, bhv_desc_t *rootbdp, dm_right_t rootvp_right,
	char *name1, char *name2);
int
dm_send_mount_event(xfs_vfs_t *vfsp, dm_right_t vfsp_right, bhv_desc_t *bdp,
	dm_right_t vp_right, bhv_desc_t *rootbdp, dm_right_t rootvp_right,
	char *name1, char *name2)
{
	return nopkg();
}


int
dm_send_namesp_event(dm_eventtype_t event, bhv_desc_t *bdp1,
	dm_right_t vp1_right, bhv_desc_t *bdp2, dm_right_t vp2_right,
	char *name1, char *name2, mode_t mode, int retcode, int flags);
int
dm_send_namesp_event(dm_eventtype_t event, bhv_desc_t *bdp1,
	dm_right_t vp1_right, bhv_desc_t *bdp2, dm_right_t vp2_right,
	char *name1, char *name2, mode_t mode, int retcode, int flags)
{
	return nopkg();
}


void
dm_send_unmount_event(xfs_vfs_t *vfsp, xfs_vnode_t *vp, dm_right_t vfsp_right,
	mode_t mode, int retcode, int flags);
void
dm_send_unmount_event(xfs_vfs_t *vfsp, xfs_vnode_t *vp, dm_right_t vfsp_right,
	mode_t mode, int retcode, int flags)
{
}


int
dm_vp_to_handle (xfs_vnode_t *vp, xfs_handle_t *handlep);
int
dm_vp_to_handle (xfs_vnode_t *vp, xfs_handle_t *handlep)
{
	return nopkg();
}
