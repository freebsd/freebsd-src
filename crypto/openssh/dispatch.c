/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
 *      This product includes software developed by Markus Friedl.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
RCSID("$Id: dispatch.c,v 1.2 2000/04/14 10:30:31 markus Exp $");
#include "ssh.h"
#include "dispatch.h"
#include "packet.h"

#define DISPATCH_MIN	0
#define DISPATCH_MAX	255

dispatch_fn *dispatch[DISPATCH_MAX];

void
dispatch_protocol_error(int type, int plen)
{
	error("Hm, dispatch protocol error: type %d plen %d", type, plen);
}
void
dispatch_init(dispatch_fn *dflt)
{
	int i;
	for (i = 0; i < DISPATCH_MAX; i++)
		dispatch[i] = dflt;
}
void
dispatch_set(int type, dispatch_fn *fn)
{
	dispatch[type] = fn;
}
void
dispatch_run(int mode, int *done)
{
	for (;;) {
		int plen;
		int type;

		if (mode == DISPATCH_BLOCK) {
			type = packet_read(&plen);
		} else {
			type = packet_read_poll(&plen);
			if (type == SSH_MSG_NONE)
				return;
		}
		if (type > 0 && type < DISPATCH_MAX && dispatch[type] != NULL)
			(*dispatch[type])(type, plen);
		else
			packet_disconnect("protocol error: rcvd type %d", type);	
		if (done != NULL && *done)
			return;
	}
}
