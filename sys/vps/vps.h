/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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
 */

/* $Id: vps.h 153 2013-06-03 16:18:17Z klaus $ */

#ifndef _VPS_H
#define _VPS_H

#include <sys/cdefs.h>

#ifdef VPS
#ifndef VIMAGE
#error "You can't have option VPS without option VIMAGE !"
#endif
#endif

/* For sysctl stuff. */
#include <sys/vnet2.h>

#define TD_TO_VPS(x)	(x)->td_ucred->cr_vps
#define P_TO_VPS(x)	(x)->p_ucred->cr_vps

/*
 * At least for now, just use vnet's facility for virtualized
 * global variables.
 * But map to our own names for easier change in the future.
 */

/* Keep in sync with ''struct vps'' declared in vps/vps2.h ! */
struct vps2 {
	struct vnet *vnet;
};

#define VPS_NAME		VNET_NAME
#define VPS_DECLARE		VNET_DECLARE
#define VPS_DEFINE		VNET_DEFINE

#define VPS_VPS(vps, n)		\
    VNET_VNET(((struct vps2 *)vps)->vnet, n)
#define VPS_VPS_PTR(vps, n)	\
    VNET_VNET_PTR(((struct vps2 *)vps)->vnet, n)
#define VPSV(n)			\
    VNET_VNET(((struct vps2 *)curthread->td_vps)->vnet, n)

#define SYSCTL_VPS_INT		SYSCTL_VNET_INT
#define SYSCTL_VPS_PROC		SYSCTL_VNET_PROC
#define SYSCTL_VPS_STRING	SYSCTL_VNET_STRING
#define SYSCTL_VPS_STRUCT	SYSCTL_VNET_STRUCT
#define SYSCTL_VPS_UINT		SYSCTL_VNET_UINT
#define SYSCTL_VPS_LONG		SYSCTL_VNET_LONG
#define SYSCTL_VPS_ULONG	SYSCTL_VNET_ULONG

#define vps_sysctl_handle_int		vnet_sysctl_handle_int
#define vps_sysctl_handle_opaque	vnet_sysctl_handle_opaque
#define vps_sysctl_handle_string	vnet_sysctl_handle_string
#define vps_sysctl_handle_uint		vnet_sysctl_handle_uint

struct vps;
extern struct vps *vps0;

#endif /* _VPS_H */

/* EOF */
