/* $NetBSD: awictl.h,v 1.1 2000/03/23 06:04:24 onoe Exp $ */
/* $FreeBSD$ */

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Atsushi Onoe
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _IC_AWICTL_H
#define	_IC_AWICTL_H

#define	AWICTL_BUFSIZE	32

#define	AWICTL_REGION	1	/* u_int8_t: region code */
#define	AWICTL_CHANSET	2	/* u_int8_t[3]: cur, min, max */
#define	AWICTL_RAWBPF	3	/* u_int8_t: pass raw 802.11 header to bpf */
#define	AWICTL_DESSID	4	/* u_int8_t[IEEE80211_NWID_LEN]: desired ESSID*/
#define	AWICTL_CESSID	5	/* u_int8_t[IEEE80211_NWID_LEN]: current ESSID*/
#define	AWICTL_MODE	6	/* u_int8_t: mode */
#define	  AWICTL_MODE_INFRA	0	/* infrastructure mode */
#define	  AWICTL_MODE_ADHOC	1	/* adhoc mode */
#define	  AWICTL_MODE_NOBSSID	2	/* adhoc without bssid mode */

#ifndef SIOCSDRVSPEC
#define	SIOCSDRVSPEC	_IOW('i', 123, struct ifdrv)
#endif

#ifndef SIOCGDRVSPEC
#define	SIOCGDRVSPEC	_IOWR('i', 122, struct ifdrv)
#endif

#ifdef __FreeBSD__
struct ifdrv {
	char		ifd_name[IFNAMSIZ];     /* if name, e.g. "en0" */
	unsigned long	ifd_cmd;
	size_t		ifd_len;
	void		*ifd_data;
};
#endif

#endif /* !_IC_AWICTL_H */
