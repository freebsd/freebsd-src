/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 */

#ifndef _DEV_VMM_PARAM_H_
#define	_DEV_VMM_PARAM_H_

/*
 * The VM name has to fit into the pathname length constraints of devfs,
 * governed primarily by SPECNAMELEN.  The length is the total number of
 * characters in the full path, relative to the mount point and not
 * including any leading '/' characters.
 * A prefix and a suffix are added to the name specified by the user.
 * The prefix is usually "vmm/" or "vmm.io/", but can be a few characters
 * longer for future use.
 * The suffix is a string that identifies a bootrom image or some similar
 * image that is attached to the VM. A separator character gets added to
 * the suffix automatically when generating the full path, so it must be
 * accounted for, reducing the effective length by 1.
 * The effective length of a VM name is 229 bytes for FreeBSD 13 and 37
 * bytes for FreeBSD 12.  A minimum length is set for safety and supports
 * a SPECNAMELEN as small as 32 on old systems.
 */
#define	VM_MAX_PREFIXLEN	10
#define	VM_MAX_SUFFIXLEN	15
#define	VM_MIN_NAMELEN		6
#define	VM_MAX_NAMELEN		\
	(SPECNAMELEN - VM_MAX_PREFIXLEN - VM_MAX_SUFFIXLEN - 1)

#endif /* !_DEV_VMM_PARAM_H_ */
