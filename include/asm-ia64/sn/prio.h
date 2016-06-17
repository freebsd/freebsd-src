/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_PRIO_H
#define _ASM_IA64_SN_PRIO_H

#include <linux/types.h>

/*
 * Priority I/O function prototypes and macro definitions
 */

typedef long long bandwidth_t;

/* These should be the same as FREAD/FWRITE */
#define PRIO_READ_ALLOCATE	0x1
#define PRIO_WRITE_ALLOCATE	0x2
#define PRIO_READWRITE_ALLOCATE	(PRIO_READ_ALLOCATE | PRIO_WRITE_ALLOCATE)

extern int prioSetBandwidth (int		/* fd */,
                             int		/* alloc_type */,
                             bandwidth_t	/* bytes_per_sec */,
                             pid_t *		/* pid */);
extern int prioGetBandwidth (int		/* fd */,
                             bandwidth_t *	/* read_bw */,
                             bandwidth_t *	/* write_bw */);
extern int prioLock (pid_t *);
extern int prioUnlock (void);

/* Error returns */
#define PRIO_SUCCESS     0
#define PRIO_FAIL       (-1) 

#endif /* _ASM_IA64_SN_PRIO_H */
