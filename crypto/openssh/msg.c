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
RCSID("$OpenBSD: msg.c,v 1.4 2002/07/01 16:15:25 deraadt Exp $");

#include "buffer.h"
#include "getput.h"
#include "log.h"
#include "atomicio.h"
#include "msg.h"

void
ssh_msg_send(int fd, u_char type, Buffer *m)
{
	u_char buf[5];
	u_int mlen = buffer_len(m);

	debug3("ssh_msg_send: type %u", (unsigned int)type & 0xff);

	PUT_32BIT(buf, mlen + 1);
	buf[4] = type;		/* 1st byte of payload is mesg-type */
	if (atomicio(write, fd, buf, sizeof(buf)) != sizeof(buf))
		fatal("ssh_msg_send: write");
	if (atomicio(write, fd, buffer_ptr(m), mlen) != mlen)
		fatal("ssh_msg_send: write");
}

int
ssh_msg_recv(int fd, Buffer *m)
{
	u_char buf[4];
	ssize_t res;
	u_int msg_len;

	debug3("ssh_msg_recv entering");

	res = atomicio(read, fd, buf, sizeof(buf));
	if (res != sizeof(buf)) {
		if (res == 0)
			return -1;
		fatal("ssh_msg_recv: read: header %ld", (long)res);
	}
	msg_len = GET_32BIT(buf);
	if (msg_len > 256 * 1024)
		fatal("ssh_msg_recv: read: bad msg_len %u", msg_len);
	buffer_clear(m);
	buffer_append_space(m, msg_len);
	res = atomicio(read, fd, buffer_ptr(m), msg_len);
	if (res != msg_len)
		fatal("ssh_msg_recv: read: %ld != msg_len", (long)res);
	return 0;
}
