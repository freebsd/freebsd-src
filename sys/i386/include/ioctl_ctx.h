/*
 *
 *	Copyright (C) 1994, Paul S. LaFollette, Jr. This software may be used,
 *	modified, copied, distributed, and sold, in both source and binary form
 *	provided that the above copyright and these terms are retained. Under
 *	no circumstances is the author responsible for the proper functioning
 *	of this software, nor does the author assume any responsibility
 *	for damages incurred with its use
 *
 *	$FreeBSD$
 */

/*
 *	ioctl constants for Cortex-I frame grabber
 */

#ifndef	_MACHINE_IOCTL_CTX_H_
#define	_MACHINE_IOCTL_CTX_H_

#include <sys/ioccom.h>

typedef char _CTX_LUTBUF[256];	/* look up table buffer */

#define CTX_LIVE _IO('x', 1)		/* live video */
#define CTX_GRAB _IO('x', 2)		/* frame grab */
#define CTX_H_ORGANIZE _IO('x', 3)  /* file goes across screen (horiz. read) */
#define CTX_V_ORGANIZE _IO('x', 4)  /* file goes down screen (vert. read)    */
#define CTX_SET_LUT _IOW('x', 5, _CTX_LUTBUF)  /* set lookup table */
#define CTX_GET_LUT _IOR('x', 6, _CTX_LUTBUF)  /* get lookup table */

#endif /* !_MACHINE_IOCTL_CTX_H_ */
