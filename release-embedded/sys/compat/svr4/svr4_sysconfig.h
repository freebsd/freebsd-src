/*-
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1995 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD$
 */

#ifndef	_SVR4_SYSCONFIG_H_
#define	_SVR4_SYSCONFIG_H_

#define SVR4_CONFIG_UNUSED_1		0x01
#define SVR4_CONFIG_NGROUPS		0x02
#define SVR4_CONFIG_CHILD_MAX		0x03
#define SVR4_CONFIG_OPEN_FILES		0x04
#define SVR4_CONFIG_POSIX_VER		0x05
#define SVR4_CONFIG_PAGESIZE		0x06
#define SVR4_CONFIG_CLK_TCK		0x07
#define SVR4_CONFIG_XOPEN_VER		0x08
#define SVR4_CONFIG_UNUSED_9		0x09
#define SVR4_CONFIG_PROF_TCK		0x0a
#define SVR4_CONFIG_NPROC_CONF		0x0b
#define	SVR4_CONFIG_NPROC_ONLN		0x0c
#define	SVR4_CONFIG_AIO_LISTIO_MAX	0x0d
#define	SVR4_CONFIG_AIO_MAX		0x0e
#define	SVR4_CONFIG_AIO_PRIO_DELTA_MAX	0x0f
#define	SVR4_CONFIG_DELAYTIMER_MAX	0x10
#define	SVR4_CONFIG_MQ_OPEN_MAX		0x11
#define	SVR4_CONFIG_MQ_PRIO_MAX		0x12
#define	SVR4_CONFIG_RTSIG_MAX		0x13
#define	SVR4_CONFIG_SEM_NSEMS_MAX	0x14
#define	SVR4_CONFIG_SEM_VALUE_MAX	0x15
#define	SVR4_CONFIG_SIGQUEUE_MAX	0x16
#define	SVR4_CONFIG_SIGRT_MIN		0x17
#define	SVR4_CONFIG_SIGRT_MAX		0x18
#define	SVR4_CONFIG_TIMER_MAX		0x19
#define	SVR4_CONFIG_PHYS_PAGES		0x1a
#define	SVR4_CONFIG_AVPHYS_PAGES	0x1b
#define	SVR4_CONFIG_COHERENCY		0x1c
#define	SVR4_CONFIG_SPLIT_CACHE		0x1d
#define	SVR4_CONFIG_ICACHESZ		0x1e
#define	SVR4_CONFIG_DCACHESZ		0x1f
#define	SVR4_CONFIG_ICACHELINESZ	0x20
#define	SVR4_CONFIG_DCACHELINESZ	0x21
#define	SVR4_CONFIG_ICACHEBLKSZ		0x22
#define	SVR4_CONFIG_DCACHEBLKSZ		0x23
#define	SVR4_CONFIG_DCACHETBLKSZ	0x24
#define	SVR4_CONFIG_ICACHE_ASSOC	0x25
#define	SVR4_CONFIG_DCACHE_ASSOC	0x26
#define	SVR4_CONFIG_UNUSED_2		0x27
#define	SVR4_CONFIG_UNUSED_3		0x28
#define	SVR4_CONFIG_UNUSED_4		0x29
#define	SVR4_CONFIG_MAXPID		0x2a
#define	SVR4_CONFIG_STACK_PROT		0x2b

#endif /* !_SVR4_SYSCONFIG_H_ */
