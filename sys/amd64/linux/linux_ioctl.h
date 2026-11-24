/*-
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _MACHINE_LINUX_IOCTL_H_
#define	_MACHINE_LINUX_IOCTL_H_

#define	LINUX_IOC_VOID		0
#define	LINUX_IOC_IN		0x40000000
#define	LINUX_IOC_OUT		0x80000000
#define	LINUX_IOC_INOUT		(LINUX_IOC_IN|LINUX_IOC_OUT)

#endif /* !_MACHINE_LINUX_IOCTL_H_ */
