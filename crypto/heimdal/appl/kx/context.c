/*
 * Copyright (c) 1995 - 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kx.h"

RCSID("$Id: context.c,v 1.4 1999/12/02 16:58:32 joda Exp $");

/*
 * Set the common part of the context `kc'
 */

void
context_set (kx_context *kc, const char *host, const char *user, int port,
	     int debug_flag, int keepalive_flag, int tcp_flag)
{
    kc->host		= host;
    kc->user		= user;
    kc->port		= port;
    kc->debug_flag	= debug_flag;
    kc->keepalive_flag	= keepalive_flag;
    kc->tcp_flag	= tcp_flag;
}

/*
 * dispatch functions
 */

void
context_destroy (kx_context *kc)
{
    (*kc->destroy)(kc);
}

int
context_authenticate (kx_context *kc, int s)
{
    return (*kc->authenticate)(kc, s);
}

int
context_userok (kx_context *kc, char *user)
{
    return (*kc->userok)(kc, user);
}

ssize_t
kx_read (kx_context *kc, int fd, void *buf, size_t len)
{
    return (*kc->read)(kc, fd, buf, len);
}

ssize_t
kx_write (kx_context *kc, int fd, const void *buf, size_t len)
{
    return (*kc->write)(kc, fd, buf, len);
}

int
copy_encrypted (kx_context *kc, int fd1, int fd2)
{
    return (*kc->copy_encrypted)(kc, fd1, fd2);
}
