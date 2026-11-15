/*-
 * This file is in the public domain.
 */

#ifndef _MACHINE_PROCCTL_H_
#define	_MACHINE_PROCCTL_H_

/*
 * Per-process user address space size, see vmparam.h: la48 is the default
 * 48-bit (256 TiB) address space, la52 the full 52 bits of the radix MMU.
 * The values match amd64's PROC_LA_CTL so that proccontrol(1) can share
 * the code.
 */
#define	PROC_LA_CTL		(PROC_PROCCTL_MD_MIN + 2)
#define	PROC_LA_STATUS		(PROC_PROCCTL_MD_MIN + 3)

#define	PROC_LA_CTL_LA48_ON_EXEC	1
#define	PROC_LA_CTL_LA52_ON_EXEC	2
#define	PROC_LA_CTL_DEFAULT_ON_EXEC	3

#define	PROC_LA_STATUS_LA48		0x01000000
#define	PROC_LA_STATUS_LA52		0x02000000

#endif /* !_MACHINE_PROCCTL_H_ */
