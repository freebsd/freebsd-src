/*
 * Copyright (c) 2004-05 Applied Micro Circuits Corporation.
 * Copyright (c) 2004-05 Vinod Kashyap.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

/*
 * AMCC'S 3ware driver for 9000 series storage controllers.
 *
 * Author: Vinod Kashyap
 */



#ifndef TW_OSL_SHARE_H

#define TW_OSL_SHARE_H


/*
 * Macros, structures and functions shared between OSL and CL,
 * and defined by OSL.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/endian.h>
#include <machine/stdarg.h>

#include "tw_osl_types.h"
#include "opt_twa.h"


#ifdef TWA_DEBUG
#define TW_OSL_DEBUG	TWA_DEBUG
#endif

#ifdef TWA_FLASH_FIRMWARE
#define TW_OSL_FLASH_FIRMWARE
#endif

#define TW_OSL_DRIVER_VERSION_STRING	"3.50.00.016"

#define	TW_OSL_CAN_SLEEP

#ifdef TW_OSL_CAN_SLEEP
typedef TW_VOID			*TW_SLEEP_HANDLE;
#endif /* TW_OSL_CAN_SLEEP */

/*#define TW_OSL_DMA_MEM_ALLOC_PER_REQUEST*/
#define TW_OSL_PCI_CONFIG_ACCESSIBLE

#if _BYTE_ORDER == _BIG_ENDIAN
#define TW_OSL_BIG_ENDIAN
#else
#define TW_OSL_LITTLE_ENDIAN
#endif

#ifdef TW_OSL_DEBUG
extern TW_INT32		TW_OSL_DEBUG_LEVEL_FOR_CL;
#endif /* TW_OSL_DEBUG */


/* Possible return codes from/to Common Layer functions. */
#define TW_OSL_ESUCCESS		0		/* success */
#define TW_OSL_EGENFAILURE	1		/* general failure */
#define TW_OSL_ENOMEM		ENOMEM		/* insufficient memory */
#define TW_OSL_EIO		EIO		/* I/O error */
#define TW_OSL_ETIMEDOUT	ETIMEDOUT	/* time out */
#define TW_OSL_ENOTTY		ENOTTY		/* invalid command */
#define TW_OSL_EBUSY		EBUSY		/* busy -- try later */
#define TW_OSL_EBIG		EFBIG		/* request too big */
#define TW_OSL_EWOULDBLOCK	EWOULDBLOCK	/* sleep timed out */
#define TW_OSL_ERESTART		ERESTART /* sleep terminated by a signal */


#define tw_osl_breakpoint()		breakpoint

#define tw_osl_cur_func()		__func__

/*
 * Submit any requests previously returned with TW_OSL_EBUSY.
 * We don't use it as of now.
 */
#define tw_osl_ctlr_ready(ctlr_handle)

#define tw_osl_destroy_lock(ctlr_handle, lock)	\
	mtx_destroy(lock)

#define tw_osl_free_lock(ctlr_handle, lock)	\
	mtx_unlock_spin(lock)

#define tw_osl_get_lock(ctlr_handle, lock)	\
	mtx_lock_spin(lock)

#define tw_osl_init_lock(ctlr_handle, lock_name, lock)	\
	mtx_init(lock, lock_name, NULL, MTX_SPIN)

#define tw_osl_memcpy(dest, src, size)	bcopy(src, dest, size)
#define tw_osl_memzero			bzero
#define tw_osl_sprintf			sprintf
#define tw_osl_strcpy			strcpy
#define tw_osl_strlen			strlen
#define tw_osl_vsprintf			vsprintf



#endif /* TW_OSL_SHARE_H */
