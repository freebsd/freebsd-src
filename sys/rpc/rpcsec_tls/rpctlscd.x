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

/* Modified from gssd.x for the client side of RPC-over-TLS. */


struct rpctlscd_connect_arg {
	char certname<>;
};

struct rpctlscd_connect_res {
	uint32_t reterr;
	uint64_t sec;
	uint64_t usec;
	uint64_t ssl;
};

struct rpctlscd_handlerecord_arg {
	uint64_t sec;
	uint64_t usec;
	uint64_t ssl;
};

struct rpctlscd_handlerecord_res {
	uint32_t reterr;
};

struct rpctlscd_disconnect_arg {
	uint64_t sec;
	uint64_t usec;
	uint64_t ssl;
};

struct rpctlscd_disconnect_res {
	uint32_t reterr;
};

program RPCTLSCD {
	version RPCTLSCDVERS {
		void RPCTLSCD_NULL(void) = 0;

		rpctlscd_connect_res
		RPCTLSCD_CONNECT(rpctlscd_connect_arg) = 1;

		rpctlscd_handlerecord_res
		RPCTLSCD_HANDLERECORD(rpctlscd_handlerecord_arg) = 2;

		rpctlscd_disconnect_res
		RPCTLSCD_DISCONNECT(rpctlscd_disconnect_arg) = 3;
	} = 1;
} = 0x40677374;
