/*
 * Macros for tracing/loging information in the CAM layer
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id$
 */
#ifndef	_CAM_CAM_DEBUG_H
#define _CAM_CAM_DEBUG_H 1

#if defined(CAMDEBUG) && defined(KERNEL)
#include <machine/clock.h>
#endif /* CAMDEBUG && KERNEL */

/*
 * Debugging flags.
 */
typedef enum {
	CAM_DEBUG_NONE		= 0x00, /* no debugging */
	CAM_DEBUG_INFO		= 0x01,	/* scsi commands, errors, data */ 
	CAM_DEBUG_TRACE		= 0x02,	/* routine flow tracking */
	CAM_DEBUG_SUBTRACE	= 0x04,	/* internal to routine flows */
} cam_debug_flags;

#if defined(CAMDEBUG) && defined(KERNEL)

/* Path we want to debug */
extern struct cam_path *cam_dpath;
/* Current debug levels set */
extern u_int32_t cam_dflags;
 
/* Debugging macros. */
#define	CAM_DEBUG(path, flag, printfargs)		\
	if ((cam_dflags & (flag))			\
	 && (cam_dpath != NULL)				\
	 && (xpt_path_comp(path, cam_dpath) >= 0)) {	\
		xpt_print_path(path);			\
 		printf printfargs;			\
		DELAY(100000);				\
	}
#define	CAM_DEBUG_PRINT(flag, printfargs)		\
	if (cam_dflags & (flag)) {			\
		printf("cam_debug: ");			\
 		printf printfargs;			\
		DELAY(100000);				\
	}

#else /* !CAMDEBUG || !KERNEL */

#define	CAM_DEBUG(A, B, C)
#define	CAM_DEBUG_PRINT(A, B)

#endif /* CAMDEBUG && KERNEL */

#endif /* _CAM_CAM_DEBUG_H */
