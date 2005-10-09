/*-
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
 * $FreeBSD$
 */
#ifndef	_CAM_CAM_DEBUG_H
#define _CAM_CAM_DEBUG_H 1

#if defined(CAMDEBUG) && defined(_KERNEL)
#include <machine/clock.h>
#endif /* CAMDEBUG && _KERNEL */

/*
 * Debugging flags.
 */
typedef enum {
	CAM_DEBUG_NONE		= 0x00, /* no debugging */
	CAM_DEBUG_INFO		= 0x01,	/* scsi commands, errors, data */ 
	CAM_DEBUG_TRACE		= 0x02,	/* routine flow tracking */
	CAM_DEBUG_SUBTRACE	= 0x04,	/* internal to routine flows */
	CAM_DEBUG_CDB		= 0x08, /* print out SCSI CDBs only */
	CAM_DEBUG_XPT		= 0x10,	/* print out xpt scheduling */
	CAM_DEBUG_PERIPH	= 0x20  /* print out peripheral calls */
} cam_debug_flags;

#if defined(CAMDEBUG) && defined(_KERNEL)

/* Path we want to debug */
extern struct cam_path *cam_dpath;
/* Current debug levels set */
extern u_int32_t cam_dflags;
/* Printf delay value (to prevent scrolling) */
extern u_int32_t cam_debug_delay;
 
/* Debugging macros. */
#define	CAM_DEBUGGED(path, flag)			\
	((cam_dflags & (flag))				\
	 && (cam_dpath != NULL)				\
	 && (xpt_path_comp(cam_dpath, path) >= 0)	\
	 && (xpt_path_comp(cam_dpath, path) < 2))
#define	CAM_DEBUG(path, flag, printfargs)		\
	if ((cam_dflags & (flag))			\
	 && (cam_dpath != NULL)				\
	 && (xpt_path_comp(cam_dpath, path) >= 0)	\
	 && (xpt_path_comp(cam_dpath, path) < 2)) {	\
		xpt_print_path(path);			\
 		printf printfargs;			\
		if (cam_debug_delay != 0)		\
			DELAY(cam_debug_delay);		\
	}
#define	CAM_DEBUG_PRINT(flag, printfargs)		\
	if (cam_dflags & (flag)) {			\
		printf("cam_debug: ");			\
 		printf printfargs;			\
		if (cam_debug_delay != 0)		\
			DELAY(cam_debug_delay);		\
	}

#else /* !CAMDEBUG || !_KERNEL */

#define	CAM_DEBUGGED(A, B)	0
#define	CAM_DEBUG(A, B, C)
#define	CAM_DEBUG_PRINT(A, B)

#endif /* CAMDEBUG && _KERNEL */

#endif /* _CAM_CAM_DEBUG_H */
