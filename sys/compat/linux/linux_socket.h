/*-
 * Copyright (c) 2000 Assar Westerlund
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
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

#ifndef _LINUX_SOCKET_H_
#define _LINUX_SOCKET_H_

/* msg flags in recvfrom/recvmsg */

#define LINUX_MSG_OOB		0x01
#define LINUX_MSG_PEEK		0x02
#define LINUX_MSG_DONTROUTE	0x04
#define LINUX_MSG_CTRUNC	0x08
#define LINUX_MSG_PROXY		0x10
#define LINUX_MSG_TRUNC		0x20
#define LINUX_MSG_DONTWAIT	0x40
#define LINUX_MSG_EOR		0x80
#define LINUX_MSG_WAITALL	0x100
#define LINUX_MSG_FIN		0x200
#define LINUX_MSG_SYN		0x400
#define LINUX_MSG_CONFIRM	0x800
#define LINUX_MSG_RST		0x1000
#define LINUX_MSG_ERRQUEUE	0x2000
#define LINUX_MSG_NOSIGNAL	0x4000

/* Socket-level control message types */

#define LINUX_SCM_RIGHTS	0x01

/* Ancilliary data object information macros */

#define LINUX_CMSG_ALIGN(len)	roundup2(len, sizeof(l_ulong))
#define LINUX_CMSG_DATA(cmsg)	((void *)((char *)(cmsg) + \
				    LINUX_CMSG_ALIGN(sizeof(struct l_cmsghdr))))
#define LINUX_CMSG_SPACE(len)	(LINUX_CMSG_ALIGN(sizeof(struct l_cmsghdr)) + \
				    LINUX_CMSG_ALIGN(len))
#define LINUX_CMSG_LEN(len)	(LINUX_CMSG_ALIGN(sizeof(struct l_cmsghdr)) + \
				    (len))
#define LINUX_CMSG_FIRSTHDR(msg) \
				((msg)->msg_controllen >= \
				    sizeof(struct l_cmsghdr) ? \
				    (struct l_cmsghdr *)((msg)->msg_control) : \
				    (struct l_cmsghdr *)(NULL))
#define LINUX_CMSG_NXTHDR(msg, cmsg) \
				((((char *)(cmsg) + \
				    LINUX_CMSG_ALIGN((cmsg)->cmsg_len) + \
				    sizeof(*(cmsg))) > \
				    (((char *)(msg)->msg_control) + \
				    (msg)->msg_controllen)) ? \
				    (struct l_cmsghdr *) NULL : \
				    (struct l_cmsghdr *)((char *)(cmsg) + \
				    LINUX_CMSG_ALIGN((cmsg)->cmsg_len)))

#define CMSG_HDRSZ		CMSG_LEN(0)
#define L_CMSG_HDRSZ		LINUX_CMSG_LEN(0)

#endif /* _LINUX_SOCKET_H_ */
