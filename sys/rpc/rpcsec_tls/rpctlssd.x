/*-
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 * Authors: Doug Rabson <dfr@rabson.org>
 * Developed with Red Inc: Alfred Perlstein <alfred@freebsd.org>
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

/* Modified from gssd.x for the server side of RPC-over-TLS. */


struct rpctlssd_connect_res {
	uint32_t flags;
	uint64_t sec;
	uint64_t usec;
	uint64_t ssl;
	uint32_t uid;
	uint32_t gid<>;
};

struct rpctlssd_handlerecord_arg {
	uint64_t sec;
	uint64_t usec;
	uint64_t ssl;
};

struct rpctlssd_handlerecord_res {
	uint32_t reterr;
};

struct rpctlssd_disconnect_arg {
	uint64_t sec;
	uint64_t usec;
	uint64_t ssl;
};

struct rpctlssd_disconnect_res {
	uint32_t reterr;
};

program RPCTLSSD {
	version RPCTLSSDVERS {
		void RPCTLSSD_NULL(void) = 0;

		rpctlssd_connect_res
		RPCTLSSD_CONNECT(void) = 1;

		rpctlssd_handlerecord_res
		RPCTLSSD_HANDLERECORD(rpctlssd_handlerecord_arg) = 2;

		rpctlssd_disconnect_res
		RPCTLSSD_DISCONNECT(rpctlssd_disconnect_arg) = 3;
	} = 1;
} = 0x40677375;
