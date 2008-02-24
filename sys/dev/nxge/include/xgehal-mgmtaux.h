/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
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
 * $FreeBSD: src/sys/dev/nxge/include/xgehal-mgmtaux.h,v 1.1.2.1 2007/11/02 00:52:33 rwatson Exp $
 */

#ifndef XGE_HAL_MGMTAUX_H
#define XGE_HAL_MGMTAUX_H

#include <dev/nxge/include/xgehal-mgmt.h>

__EXTERN_BEGIN_DECLS

#define XGE_HAL_AUX_SEPA        ' '

xge_hal_status_e xge_hal_aux_about_read(xge_hal_device_h devh, int bufsize,
	        char *retbuf, int *retsize);

xge_hal_status_e xge_hal_aux_stats_tmac_read(xge_hal_device_h devh, int bufsize,
	        char *retbuf, int *retsize);

xge_hal_status_e xge_hal_aux_stats_rmac_read(xge_hal_device_h devh, int bufsize,
	        char *retbuf, int *retsize);

xge_hal_status_e xge_hal_aux_stats_sw_dev_read(xge_hal_device_h devh,
	        int bufsize, char *retbuf, int *retsize);

xge_hal_status_e xge_hal_aux_stats_pci_read(xge_hal_device_h devh, int bufsize,
	        char *retbuf, int *retsize);

xge_hal_status_e xge_hal_aux_stats_hal_read(xge_hal_device_h devh, int bufsize,
	        char *retbuf, int *retsize);

xge_hal_status_e xge_hal_aux_bar0_read(xge_hal_device_h devh,
	        unsigned int offset, int bufsize, char *retbuf,
	        int *retsize);

xge_hal_status_e xge_hal_aux_bar0_write(xge_hal_device_h devh,
	        unsigned int offset, u64 value);

xge_hal_status_e xge_hal_aux_bar1_read(xge_hal_device_h devh,
	        unsigned int offset, int bufsize, char *retbuf,
	        int *retsize);

xge_hal_status_e xge_hal_aux_pci_config_read(xge_hal_device_h devh, int bufsize,
	        char *retbuf, int *retsize);

xge_hal_status_e xge_hal_aux_stats_herc_enchanced(xge_hal_device_h devh,
	        int bufsize, char *retbuf, int *retsize);

xge_hal_status_e xge_hal_aux_channel_read(xge_hal_device_h devh, int bufsize,
	        char *retbuf, int *retsize);

xge_hal_status_e xge_hal_aux_device_dump(xge_hal_device_h devh);


xge_hal_status_e xge_hal_aux_driver_config_read(int bufsize, char *retbuf,
	        int *retsize);

xge_hal_status_e xge_hal_aux_device_config_read(xge_hal_device_h devh,
	        int bufsize, char *retbuf, int *retsize);

__EXTERN_END_DECLS

#endif /* XGE_HAL_MGMTAUX_H */
