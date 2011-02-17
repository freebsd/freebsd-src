/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
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

static xfs_mac_label_t *mac_low_high_lp;
static xfs_mac_label_t *mac_high_low_lp;
static xfs_mac_label_t *mac_admin_high_lp;
static xfs_mac_label_t *mac_equal_equal_lp;

/*
 * Test for the existence of a MAC label as efficiently as possible.
 */
int
xfs_mac_vhaslabel(
	xfs_vnode_t	*vp)
{
	int		error;
	int		len = sizeof(xfs_mac_label_t);
	int		flags = ATTR_KERNOVAL|ATTR_ROOT;

	XVOP_ATTR_GET(vp, SGI_MAC_FILE, NULL, &len, flags, sys_cred, error);
	return (error == 0);
}

int
xfs_mac_iaccess(xfs_inode_t *ip, mode_t mode, struct cred *cr)
{
	xfs_mac_label_t mac;
	xfs_mac_label_t *mp = mac_high_low_lp;

	if (cr == NULL || sys_cred == NULL ) {
		return EACCES;
	}

	if (xfs_attr_fetch(ip, SGI_MAC_FILE, (char *)&mac, sizeof(mac)) == 0) {
		if ((mp = mac_add_label(&mac)) == NULL) {
			return mac_access(mac_high_low_lp, cr, mode);
		}
	}

	return mac_access(mp, cr, mode);
}
