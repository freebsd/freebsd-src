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

#include "xfs.h"

struct xfsstats xfsstats;

STATIC int
xfs_read_xfsstats(
	char		*buffer,
	char		**start,
	off_t		offset,
	int		count,
	int		*eof,
	void		*data)
{
	int		i, j, len;
	static struct xstats_entry {
		char	*desc;
		int	endpoint;
	} xstats[] = {
		{ "extent_alloc",	XFSSTAT_END_EXTENT_ALLOC	},
		{ "abt",		XFSSTAT_END_ALLOC_BTREE		},
		{ "blk_map",		XFSSTAT_END_BLOCK_MAPPING	},
		{ "bmbt",		XFSSTAT_END_BLOCK_MAP_BTREE	},
		{ "dir",		XFSSTAT_END_DIRECTORY_OPS	},
		{ "trans",		XFSSTAT_END_TRANSACTIONS	},
		{ "ig",			XFSSTAT_END_INODE_OPS		},
		{ "log",		XFSSTAT_END_LOG_OPS		},
		{ "push_ail",		XFSSTAT_END_TAIL_PUSHING	},
		{ "xstrat",		XFSSTAT_END_WRITE_CONVERT	},
		{ "rw",			XFSSTAT_END_READ_WRITE_OPS	},
		{ "attr",		XFSSTAT_END_ATTRIBUTE_OPS	},
		{ "icluster",		XFSSTAT_END_INODE_CLUSTER	},
		{ "vnodes",		XFSSTAT_END_VNODE_OPS		},
	};

	for (i=j=len = 0; i < sizeof(xstats)/sizeof(struct xstats_entry); i++) {
		len += sprintf(buffer + len, xstats[i].desc);
		/* inner loop does each group */
		while (j < xstats[i].endpoint) {
			len += sprintf(buffer + len, " %u",
					*(((__u32*)&xfsstats) + j));
			j++;
		}
		buffer[len++] = '\n';
	}
	/* extra precision counters */
	len += sprintf(buffer + len, "xpc %ju %ju %ju\n",
			(uintmax_t)xfsstats.xs_xstrat_bytes,
			(uintmax_t)xfsstats.xs_write_bytes,
			(uintmax_t)xfsstats.xs_read_bytes);

	if (offset >= len) {
		*start = buffer;
		*eof = 1;
		return 0;
	}
	*start = buffer + offset;
	if ((len -= offset) > count)
		return count;
	*eof = 1;

	return len;
}

void
xfs_init_procfs(void)
{
	if (&xfs_read_xfsstats != NULL);
}

void
xfs_cleanup_procfs(void)
{
}
