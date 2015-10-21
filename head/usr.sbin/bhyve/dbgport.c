/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "inout.h"
#include "dbgport.h"
#include "pci_lpc.h"

#define	BVM_DBG_PORT	0x224
#define	BVM_DBG_SIG	('B' << 8 | 'V')

static int listen_fd, conn_fd;

static struct sockaddr_in sin;

static int
dbg_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
	    uint32_t *eax, void *arg)
{
	char ch;
	int nwritten, nread, printonce;

	if (bytes == 2 && in) {
		*eax = BVM_DBG_SIG;
		return (0);
	}

	if (bytes != 4)
		return (-1);

again:
	printonce = 0;
	while (conn_fd < 0) {
		if (!printonce) {
			printf("Waiting for connection from gdb\r\n");
			printonce = 1;
		}
		conn_fd = accept(listen_fd, NULL, NULL);
		if (conn_fd >= 0)
			fcntl(conn_fd, F_SETFL, O_NONBLOCK);
		else if (errno != EINTR)
			perror("accept");
	}

	if (in) {
		nread = read(conn_fd, &ch, 1);
		if (nread == -1 && errno == EAGAIN)
			*eax = -1;
		else if (nread == 1)
			*eax = ch;
		else {
			close(conn_fd);
			conn_fd = -1;
			goto again;
		}
	} else {
		ch = *eax;
		nwritten = write(conn_fd, &ch, 1);
		if (nwritten != 1) {
			close(conn_fd);
			conn_fd = -1;
			goto again;
		}
	}
	return (0);
}

static struct inout_port dbgport = {
	"bvmdbg",
	BVM_DBG_PORT,
	1,
	IOPORT_F_INOUT,
	dbg_handler
};

SYSRES_IO(BVM_DBG_PORT, 4);

void
init_dbgport(int sport)
{
	int reuse;

	conn_fd = -1;

	if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(sport);

	reuse = 1;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse,
	    sizeof(reuse)) < 0) {
		perror("setsockopt");
		exit(1);
	}

	if (bind(listen_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("bind");
		exit(1);
	}

	if (listen(listen_fd, 1) < 0) {
		perror("listen");
		exit(1);
	}

	register_inout(&dbgport);
}
