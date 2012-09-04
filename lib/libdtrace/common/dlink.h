/*
 * CDDL HEADER START
 *
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2012 by Delphix. All rights reserved.
 */

#ifndef	_DLINK_H
#define	_DLINK_H

#include <link.h>
#include <sys/dtrace.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern const char *devname;

extern void dprintf(int, const char *, ...);
extern void dtrace_link_init(void);
extern void dtrace_link_dof(dof_hdr_t *, Lmid_t, const char *, uintptr_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _DLINK_H */
