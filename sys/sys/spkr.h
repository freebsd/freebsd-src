/*
 * spkr.h -- interface definitions for speaker ioctl()
 *
 * v1.1 by Eric S. Raymond (esr@snark.thyrsus.com) Feb 1990
 *      modified for 386bsd by Andrew A. Chernov <ache@astral.msk.su>
 *      386bsd only clean version, all SYSV stuff removed
 *
 *	$Id: spkr.h,v 1.2 1993/10/16 17:17:48 rgrimes Exp $
 */

#ifndef _SPKR_H_
#define _SPKR_H_

#ifndef KERNEL
#include <sys/ioctl.h>
#else
#include "ioctl.h"
#endif

#define SPKRTONE        _IOW('S', 1, tone_t)    /* emit tone */
#define SPKRTUNE        _IO('S', 2)             /* emit tone sequence*/

typedef struct
{
    int	frequency;	/* in hertz */
    int duration;	/* in 1/100ths of a second */
}
tone_t;

#endif /* _SPKR_H_ */
