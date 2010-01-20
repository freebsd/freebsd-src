/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
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

/*
 * This file contains globals needed by XFS that were normally defined
 * somewhere else in IRIX.
 */

#include "xfs.h"
#include "xfs_types.h"
#include "xfs_bmap_btree.h"
#include "xfs_bit.h"
#include "xfs_refcache.h"

/*
 * Tunable XFS parameters.  xfs_params is required even when CONFIG_SYSCTL=n,
 * other XFS code uses these values.
 */

xfs_param_t xfs_params = {
			  /*	MIN	DFLT	MAX	*/
#ifdef HAVE_REFCACHE
	.refcache_size	= {	0,	128,	XFS_REFCACHE_SIZE_MAX },
	.refcache_purge	= {	0,	32,	XFS_REFCACHE_SIZE_MAX },
#endif
	.restrict_chown	= {	0,		1,		1	},
	.sgid_inherit	= {	0,		0,		1	},
	.symlink_mode	= {	0,		0,		1	},
	.panic_mask	= {	0,		0,		127	},
	.error_level	= {	0,		3,		11	},
	.syncd_timer	= {	1*100,		30*100,		7200*100},
	.probe_dmapi	= {	0,		0,		1	},
	.probe_ioops	= {	0,		0,		1	},
	.probe_quota	= {	0,		1,		1	},
	.stats_clear	= {	0,		0,		1	},
	.inherit_sync	= {	0,		1,		1	},
	.inherit_nodump	= {	0,		1,		1	},
	.inherit_noatim = {	0,		1,		1	},
	.xfs_buf_timer	= {	100/2,		1*100,		30*100	},
	.xfs_buf_age	= {	1*100,		15*100,		7200*100},
	.inherit_nosym	= {	0,		0,		1	},
	.rotorstep	= {	1,		1,		255	},
};

/*
 * Global system credential structure.
 */
cred_t sys_cred_val, *sys_cred = &sys_cred_val;
