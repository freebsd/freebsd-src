/*	FreeBSD $Id$ */

/*
 * Copyright (c) 1997, 1998
 *      Nick Hibma <n_hibma@freebsd.org>. All rights reserved.
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
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY NICK HIBMA AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* Macro's to cope with the differences between NetBSD and FreeBSD
 */

/*
 * NetBSD
 *
 */

#if defined(__NetBSD__)
#include "opt_usbverbose.h"

#define DEVICE_NAME(bdev)	\
	printf("%s: ", (bdev).dv_xname)

typedef struct device bdevice;			/* base device */



/*
 * FreeBSD
 *
 */

#elif defined(__FreeBSD__)
#include "opt_usb.h"
#define DEVICE_NAME(bdev)	\
		printf("%s%d: ",	\
			device_get_name(bdev), device_get_unit(bdev))

/* XXX Change this when FreeBSD has memset
 */
#define memset(d, v, s)	\
		do{			\
		if ((v) == 0)		\
			bzero((d), (s));	\
		else			\
			panic("Non zero filler for memset, cannot handle!"); \
		} while (0)

/* XXX can't we put this somehow into a typedef? */
#define bdevice	device_t			/* base device */

#endif


/*
 * General
 *
 */

#define DEVICE_MSG(bdev, s)	(DEVICE_NAME(bdev), printf s)
#define DEVICE_ERROR(bdev, s)	DEVICE_MSG(bdev, s)


/* Returns from attach for NetBSD vs. FreeBSD
 */

/* Error returns */
#if defined(__NetBSD__)
#define ATTACH_ERROR_RETURN	return
#define ATTACH_SUCCESS_RETURN	return
#elif defined(__FreeBSD__)
#define ATTACH_ERROR_RETURN	return ENXIO
#define ATTACH_SUCCESS_RETURN	return 0
#endif


/*
 * The debugging subsystem
 */

/* XXX to be filled in
 */

