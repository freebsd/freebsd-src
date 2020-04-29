/*
 * Copyright 2016 Jakub Klama <jceel@FreeBSD.org>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#include "../lib9p.h"
#include "../backend/fs.h"
#include "../transport/socket.h"

int
main(int argc, char **argv)
{
	struct l9p_backend *fs_backend;
	struct l9p_server *server;
	char *host = "0.0.0.0";
	char *port = "564";
	char *path;
	bool ro = false;
	int rootfd;
	int opt;

	while ((opt = getopt(argc, argv, "h:p:r")) != -1) {
		switch (opt) {
		case 'h':
			host = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		case 'r':
			ro = true;
			break;
		case '?':
		default:
			goto usage;
		}
	}

	if (optind >= argc) {
usage:
		errx(1, "Usage: server [-h <host>] [-p <port>] [-r] <path>");
	}

	path = argv[optind];
	rootfd = open(path, O_DIRECTORY);

	if (rootfd < 0)
		err(1, "cannot open root directory");

	if (l9p_backend_fs_init(&fs_backend, rootfd, ro) != 0)
		err(1, "cannot init backend");

	if (l9p_server_init(&server, fs_backend) != 0)
		err(1, "cannot create server");

	server->ls_max_version = L9P_2000L;
	if (l9p_start_server(server, host, port))
		err(1, "l9p_start_server() failed");

	/* XXX - we never get here, l9p_start_server does not return */
	exit(0);
}
