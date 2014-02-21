/* $OpenBSD: msg.c,v 1.15 2006/08/03 03:34:42 deraadt Exp $ */
/*
 * Copyright (c) 2002 Markus Friedl.  All rights reserved.
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
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/uio.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include "buffer.h"
#include "log.h"
#include "atomicio.h"
#include "msg.h"
#include "misc.h"

int
ssh_msg_send(int fd, u_char type, Buffer *m)
{
	u_char buf[5];
	u_int mlen = buffer_len(m);

	debug3("ssh_msg_send: type %u", (unsigned int)type & 0xff);

	put_u32(buf, mlen + 1);
	buf[4] = type;		/* 1st byte of payload is mesg-type */
	if (atomicio(vwrite, fd, buf, sizeof(buf)) != sizeof(buf)) {
		error("ssh_msg_send: write");
		return (-1);
	}
	if (atomicio(vwrite, fd, buffer_ptr(m), mlen) != mlen) {
		error("ssh_msg_send: write");
		return (-1);
	}
	return (0);
}

int
ssh_msg_recv(int fd, Buffer *m)
{
	u_char buf[4];
	u_int msg_len;

	debug3("ssh_msg_recv entering");

	if (atomicio(read, fd, buf, sizeof(buf)) != sizeof(buf)) {
		if (errno != EPIPE)
			error("ssh_msg_recv: read: header");
		return (-1);
	}
	msg_len = get_u32(buf);
	if (msg_len > 256 * 1024) {
		error("ssh_msg_recv: read: bad msg_len %u", msg_len);
		return (-1);
	}
	buffer_clear(m);
	buffer_append_space(m, msg_len);
	if (atomicio(read, fd, buffer_ptr(m), msg_len) != msg_len) {
		error("ssh_msg_recv: read: %s", strerror(errno));
		return (-1);
	}
	return (0);
}
