/*
 * Copyright (c) 2002-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License 
 * as published by the Free Software Foundation.
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
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy, 
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#ifndef _ASM_IA64_SN_SNDRV_H
#define _ASM_IA64_SN_SNDRV_H

/* ioctl commands */
#define SNDRV_GET_ROUTERINFO		1
#define SNDRV_GET_INFOSIZE		2
#define SNDRV_GET_HUBINFO		3
#define SNDRV_GET_FLASHLOGSIZE		4
#define SNDRV_SET_FLASHSYNC		5
#define SNDRV_GET_FLASHLOGDATA		6
#define SNDRV_GET_FLASHLOGALL		7

#define SNDRV_SET_HISTOGRAM_TYPE	14

#define SNDRV_ELSC_COMMAND		19
#define	SNDRV_CLEAR_LOG			20
#define	SNDRV_INIT_LOG			21
#define	SNDRV_GET_PIMM_PSC		22
#define SNDRV_SET_PARTITION		23
#define SNDRV_GET_PARTITION		24

/* see synergy_perf_ioctl() */
#define SNDRV_GET_SYNERGY_VERSION	30
#define SNDRV_GET_SYNERGY_STATUS	31
#define SNDRV_GET_SYNERGYINFO		32
#define SNDRV_SYNERGY_APPEND		33
#define SNDRV_SYNERGY_ENABLE		34
#define SNDRV_SYNERGY_FREQ		35

/* see shubstats_ioctl() */
#define SNDRV_SHUB_INFOSIZE		40
#define SNDRV_SHUB_CONFIGURE		41
#define SNDRV_SHUB_RESETSTATS		42
#define SNDRV_SHUB_GETSTATS		43
#define SNDRV_SHUB_GETNASID		44

/* Devices */
#define SNDRV_UKNOWN_DEVICE		-1
#define SNDRV_ROUTER_DEVICE		1
#define SNDRV_HUB_DEVICE		2
#define SNDRV_ELSC_NVRAM_DEVICE		3
#define SNDRV_ELSC_CONTROLLER_DEVICE	4
#define SNDRV_SYSCTL_SUBCH		5
#define SNDRV_SYNERGY_DEVICE		6

#endif /* _ASM_IA64_SN_SNDRV_H */
