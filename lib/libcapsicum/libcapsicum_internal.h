/*-
 * Copyright (c) 2009 Robert N. M. Watson
 * All rights reserved.
 *
 * WARNING: THIS IS EXPERIMENTAL SECURITY SOFTWARE THAT MUST NOT BE RELIED
 * ON IN PRODUCTION SYSTEMS.  IT WILL BREAK YOUR SOFTWARE IN NEW AND
 * UNEXPECTED WAYS.
 * 
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc. 
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
 * $P4: //depot/projects/trustedbsd/capabilities/src/lib/libcapsicum/libcapsicum_internal.h#1 $
 */

#ifndef _LIBCAPABILITY_INTERNAL_H_
#define	_LIBCAPABILITY_INTERNAL_H_

struct lc_host {
	int	lch_fd_sock;
};

struct lc_sandbox {
	int	lcs_fd_sock;
	int	lcs_fd_procdesc;
	pid_t	lcs_pid;
};

/*
 * Communications flags for recv/send calls (lc_flags).
 */
#define	LC_IGNOREEINTR	0x00000001

struct msghdr;
void	_lc_dispose_rights(int *fdp, int fdcount);
int	_lc_receive_rights(struct msghdr *msg, int *fdp, int *fdcountp);

ssize_t	_lc_recv(int fd, void *buf, size_t len, int flags, int lc_flags);
ssize_t	_lc_recv_rights(int fd, void *buf, size_t len, int flags,
	    int lc_flags, int *fdp, int *fdcountp);
ssize_t	_lc_send(int fd, const void *msg, size_t len, int flags,
	    int lc_flags);
ssize_t	_lc_send_rights(int fd, const void *msg, size_t len, int flags,
	    int lc_flags, int *fdp, int fdcount);

#endif /* !_LIBCAPABILITY_INTERNAL_H_ */
