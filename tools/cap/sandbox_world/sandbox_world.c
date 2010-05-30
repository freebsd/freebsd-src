/*-
 * Copyright (c) 2009-2010 Robert N. M. Watson
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <libcapsicum.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Almost your standard "hello world" application, only we run the printf in
 * a sandbox, and we use a 1-byte synchronous RPC to make sure that the host
 * doesn't exit until the sandbox is done.
 */

#define	MYNAME	"sandbox_world"		/* Binary to run in sandbox. */

int	ld_insandbox(void);
int	sandbox(void);

/*
 * Unsandboxed host process with full user rights.
 */
int
main(int argc, char *argv[])
{
	struct lc_sandbox *lcsp;
	char *sandbox_argv[3] = { argv[1], "nested", NULL };
	struct iovec iov;
	size_t len;
	char ch;

	if (ld_insandbox()) return sandbox();

	if (argc != 1)
		errx(-1, "usage: sandbox_world");

	/*
	 * Create a sandbox, do permit access to stdout and stderr.
	 */
	if (lch_start(MYNAME, sandbox_argv, LCH_PERMIT_STDERR |
	    LCH_PERMIT_STDOUT, NULL, &lcsp) < 0)
		err(-1, "lch_start %s", argv[1]);

	/*
	 * Send a one-byte message to the sandbox and wait for a one-byte
	 * reply.
	 */
	ch = 'X';
	iov.iov_base = &ch;
	iov.iov_len = sizeof(ch);
	if (lch_rpc(lcsp, 0, &iov, 1, &iov, 1, &len) < 0)
		err(-1, "lch_rpc");
	if (len != sizeof(ch))
		errx(-1, "lch_rpc returned size %zd not %zd", len, sizeof(ch));
	if (ch != 'X')
		errx(-1, "lch_recv: expected %d and got %d", 'X', ch);

	/*
	 * Terminate the sandbox when done.
	 */
	lch_stop(lcsp);
}

/*
 * Sandboxed process implementing a 'printf hello world' RPC.
 */
int
sandbox()
{
	struct lc_host *lchp;
	u_int32_t opno, seqno;
	struct iovec iov;
	u_char *buffer;
	size_t len;

	if (lcs_get(&lchp) < 0)
		err(-1, "lcs_get");

	/*
	 * Serve RPCs from the host until the sandbox is killed.
	 */
	while (1) {
		/*
		 * Receive a one-byte RPC from the host.
		 */
		if (lcs_recvrpc(lchp, &opno, &seqno, &buffer, &len) < 0) {
			if (errno != EPIPE)
				err(-6, "lcs_recvrpc");
			else
				exit(-6);
		}
		if (len != 1)
			errx(-7, "lcs_recvrpc len");
		printf("Hello world!\n");
		fflush(stdout);

		/*
		 * Reply with the same message.  Remember to free the message
		 * when done.
		 */
		iov.iov_base = buffer;
		iov.iov_len = 1;
		if (lcs_sendrpc(lchp, opno, seqno, &iov, 1) < 0) {
			if (errno != EPIPE)
				err(-8, "lcs_sendrpc");
			else
				exit(-8);
		}
		free(buffer);
	}

	return 0;
}
