/*	$FreeBSD$ */
/*	$NetBSD: rf_netbsd.h,v 1.12 2000/05/28 22:53:49 oster Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Greg Oster; Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RF__RF_BSD_H_
#define _RF__RF_BSD_H_

#ifdef _KERNEL
#include <sys/fcntl.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include "opt_raid.h"

#ifdef RAID_DEBUG
#define rf_printf(lvl, fmt, args...)				\
	do {							\
		if (lvl <= RAID_DEBUG) printf(fmt, ##args);	\
	} while(0)

#else				/* DEBUG */
#define rf_printf(lvl, fmt, args...) { }
#endif				/* DEBUG */
#endif /* _KERNEL */

/* The per-component label information that the user can set */
typedef struct RF_ComponentInfo_s {
	int row;              /* the row number of this component */
	int column;           /* the column number of this component */
	int serial_number;    /* a user-specified serial number for this
				 RAID set */
} RF_ComponentInfo_t;

/* The per-component label information */
typedef struct RF_ComponentLabel_s {
	int version;          /* The version of this label. */
	int serial_number;    /* a user-specified serial number for this
				 RAID set */
	int mod_counter;      /* modification counter.  Changed (usually
				 by incrementing) every time the label 
				 is changed */
	int row;              /* the row number of this component */
	int column;           /* the column number of this component */
	int num_rows;         /* number of rows in this RAID set */
	int num_columns;      /* number of columns in this RAID set */
	int clean;            /* 1 when clean, 0 when dirty */
	int status;           /* rf_ds_optimal, rf_ds_dist_spared, whatever. */
	/* stuff that will be in version 2 of the label */
	int sectPerSU;        /* Sectors per Stripe Unit */
	int SUsPerPU;         /* Stripe Units per Parity Units */
	int SUsPerRU;         /* Stripe Units per Reconstruction Units */
	int parityConfig;     /* '0' == RAID0, '1' == RAID1, etc. */
	int maxOutstanding;   /* maxOutstanding disk requests */
	int blockSize;        /* size of component block. 
				 (disklabel->d_secsize) */
	int numBlocks;        /* number of blocks on this component.  May
			         be smaller than the partition size. */
	int partitionSize;    /* number of blocks on this *partition*. 
				 Must exactly match the partition size
				 from the disklabel. */
	int future_use[33];   /* Future expansion */
	int autoconfigure;    /* automatically configure this RAID set. 
				 0 == no, 1 == yes */
	int root_partition;   /* Use this set as /
				 0 == no, 1 == yes*/
	int last_unit;        /* last unit number (e.g. 0 for /dev/raid0) 
				 of this component.  Used for autoconfigure
				 only. */
	int config_order;     /* 0 .. n.  The order in which the component
				 should be auto-configured.  E.g. 0 is will 
				 done first, (and would become raid0).
				 This may be in conflict with last_unit!!?! */
	                      /* Not currently used. */
	int future_use2[44];  /* More future expansion */
} RF_ComponentLabel_t;

typedef struct RF_SingleComponent_s {
	int row;
	int column;
	char component_name[50]; /* name of the component */
} RF_SingleComponent_t; 

#ifdef _KERNEL

struct raidcinfo {
	struct vnode *ci_vp;	/* component device's vnode */
	dev_t   ci_dev;		/* component device's dev_t */
	RF_ComponentLabel_t ci_label; /* components RAIDframe label */
#if 0
	size_t  ci_size;	/* size */
	char   *ci_path;	/* path to component */
	size_t  ci_pathlen;	/* length of component path */
#endif
};



/* XXX probably belongs in a different .h file. */
typedef struct RF_AutoConfig_s {
	char devname[56];       /* the name of this component */
	int flag;               /* a general-purpose flag */
	dev_t dev;              /* the device for this component */
	struct vnode *vp;       /* Mr. Vnode Pointer */
	RF_ComponentLabel_t *clabel;  /* the label */
	struct RF_AutoConfig_s *next; /* the next autoconfig structure 
				         in this set. */
} RF_AutoConfig_t;

typedef struct RF_ConfigSet_s {
	struct RF_AutoConfig_s *ac; /* all of the autoconfig structures for
				       this config set. */
	int rootable;               /* Set to 1 if this set can be root */
	struct RF_ConfigSet_s *next;
} RF_ConfigSet_t;

#endif /* _KERNEL */
#endif /* _RF__RF_BSD_H_ */
