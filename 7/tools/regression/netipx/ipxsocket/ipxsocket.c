/*-
 * Copyright (c) 2006 Robert N. M. Watson
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
 *
 * $FreeBSD$
 */

/*
 * Simple regression test to open and then immediately close various types of
 * PF_IPX sockets.  Run with various waits in order to make sure that the
 * various IPX/SPX timers have a chance to walk the pcb lists and hit the
 * sockets.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netipx/ipx.h>

#include <err.h>
#include <unistd.h>

static int
maybe_sleep(int sec)
{

	if (sec == 0)
		return (0);
	return (sleep(sec));
}

int
main(int argc, char *argv[])
{
	int delay, s;

	for (delay = 0; delay < 5; delay++) {
		s = socket(PF_IPX, SOCK_DGRAM, 0);
		if (s < 0)
			warn("socket(PF_IPX, SOCK_DGRAM, 0)");
		else {
			maybe_sleep(delay);
			close(s);
		}

		s = socket(PF_IPX, SOCK_STREAM, 0);
		if (s < 0)
			warn("socket(PF_IPX, SOCK_STREAM, 0)");
		else {
			maybe_sleep(delay);
			close(s);
		}

		s = socket(PF_IPX, SOCK_SEQPACKET, 0);
		if (s < 0)
			warn("socket(PF_IPX, SOCK_SEQPACKET, 0)");
		else {
			maybe_sleep(delay);
			close(s);
		}

		s = socket(PF_IPX, SOCK_RAW, 0);
		if (s < 0)
			warn("socket(PF_IPX, SOCK_RAW, 0)");
		else {
			maybe_sleep(delay);
			close(s);
		}
	}

	return (0);
}
