/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1999 Marcel Moolenaar
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
 */

#ifndef _LINUX_MIB_H_
#define _LINUX_MIB_H_

#ifdef SYSCTL_DECL
SYSCTL_DECL(_compat_linux);
#endif

void	linux_osd_jail_register(void);
void	linux_osd_jail_deregister(void);

void	linux_get_osname(struct thread *td, char *dst);

void	linux_get_osrelease(struct thread *td, char *dst);

int	linux_get_oss_version(struct thread *td);

int	linux_kernver(struct thread *td);

#define	LINUX_KVERSION		5
#define	LINUX_KPATCHLEVEL	15
#define	LINUX_KSUBLEVEL		0

#define	LINUX_KERNVER(a,b,c)	(((a) << 16) + ((b) << 8) + (c))
#define	LINUX_VERSION_CODE	LINUX_KERNVER(LINUX_KVERSION,		\
				    LINUX_KPATCHLEVEL, LINUX_KSUBLEVEL)
#define	LINUX_KERNVERSTR(x)	#x
#define	LINUX_XKERNVERSTR(x)	LINUX_KERNVERSTR(x)
#define	LINUX_VERSION_STR	LINUX_XKERNVERSTR(LINUX_KVERSION.LINUX_KPATCHLEVEL.LINUX_KSUBLEVEL)

extern int linux_default_openfiles;
extern int linux_default_stacksize;
extern int linux_dummy_rlimits;
extern int linux_ignore_ip_recverr;
extern int linux_preserve_vstatus;
extern bool linux_map_sched_prio;

struct image_params;
int linux_setid_allowed_query(struct thread *td, struct image_params *imgp);

#endif /* _LINUX_MIB_H_ */
