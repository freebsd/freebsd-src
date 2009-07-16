/*-
 * Copyright (c) 2009 Stanislav Sedov <stas@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef __LIBPROCSTAT_H__
#define	__LIBPROCSTAT_H__

/*
 * Vnode types.
 */
#define	PS_FST_VTYPE_VNON	1
#define	PS_FST_VTYPE_VREG	2
#define	PS_FST_VTYPE_VDIR	3
#define	PS_FST_VTYPE_VBLK	4
#define	PS_FST_VTYPE_VCHR	5
#define	PS_FST_VTYPE_VLNK	6
#define	PS_FST_VTYPE_VSOCK	7
#define	PS_FST_VTYPE_VFIFO	8
#define	PS_FST_VTYPE_VBAD	9
#define	PS_FST_VTYPE_UNKNOWN	255

/*
 * Descriptor types.
 */
#define	PS_FST_TYPE_VNODE	1
#define	PS_FST_TYPE_FIFO	2
#define	PS_FST_TYPE_SOCKET	3
#define	PS_FST_TYPE_PIPE	4
#define	PS_FST_TYPE_PTS		5

/*
 * Special descriptor numbers.
 */
#define	PS_FST_FD_RDIR		-1
#define	PS_FST_FD_CDIR		-2
#define	PS_FST_FD_JAIL		-3
#define	PS_FST_FD_TRACE		-4
#define	PS_FST_FD_TEXT		-5
#define	PS_FST_FD_MMAP		-6

/*
 * Descriptor flags.
 */
#define PS_FST_FFLAG_READ	0x0001
#define PS_FST_FFLAG_WRITE	0x0002
#define	PS_FST_FFLAG_NONBLOCK	0x0004
#define	PS_FST_FFLAG_APPEND	0x0008
#define	PS_FST_FFLAG_SHLOCK	0x0010
#define	PS_FST_FFLAG_EXLOCK	0x0020
#define	PS_FST_FFLAG_ASYNC	0x0040
#define	PS_FST_FFLAG_SYNC	0x0080
#define	PS_FST_FFLAG_NOFOLLOW	0x0100
#define	PS_FST_FFLAG_CREAT	0x0200
#define	PS_FST_FFLAG_TRUNC	0x0400
#define	PS_FST_FFLAG_EXCL	0x0800

struct procstat {
        int     type;
        kvm_t   *kd;
};

void	procstat_close(struct procstat *procstat);
struct filestat_list	*procstat_getfiles(struct procstat *procstat,
    struct kinfo_proc *kp);
struct kinfo_proc	*procstat_getprocs(struct procstat *procstat,
    int what, int arg, unsigned int *count);
int	procstat_get_pipe_info(struct procstat *procstat, struct filestat *fst,
    struct pipestat *pipe, char *errbuf);
int	procstat_get_pts_info(struct procstat *procstat, struct filestat *fst,
    struct ptsstat *pts, char *errbuf);
int	procstat_get_socket_info(struct procstat *procstat, struct filestat *fst,
    struct sockstat *sock, char *errbuf);
int	procstat_get_vnode_info(struct procstat *procstat, struct filestat *fst,
    struct vnstat *vn, char *errbuf);
struct procstat	*procstat_open(const char *nlistf, const char *memf);

#endif	/* !__LIBPROCSTAT_H__ */
