/*
 * Copyright (c) 2002-2004 Marko Zec <zec@fer.hr>
 * Copyright (c) 2009 University of Zagreb
 * Copyright (c) 2009 FreeBSD Foundation
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/vimage.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void
vi_print(struct vi_req *vi_req)
{

	printf("\"%s\":\n", vi_req->vi_name);
	printf("    %d sockets, %d ifnets, %d processes\n",
	    vi_req->vi_sock_count, vi_req->vi_if_count, vi_req->vi_proc_count);
}

int
main(int argc, char **argv)
{
	int s;
	char *shell;
	int cmd = VI_SWITCHTO;
	struct vi_req vi_req;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		goto abort;

	bzero(&vi_req, sizeof(vi_req));
	strcpy(vi_req.vi_name, ".");	/* . = this vimage. */

	if (argc == 1)
		cmd = VI_GET;

	if (argc == 2 && strcmp(argv[1], "-l") == 0)
		cmd = VI_GETNEXT;

	if (argc == 2 && strcmp(argv[1], "-lr") == 0)
		cmd = VI_GETNEXT_RECURSE;

	if (argc == 3) {
		strcpy(vi_req.vi_name, argv[2]);
		if (strcmp(argv[1], "-l") == 0)
			cmd = VI_GET;
		if (strcmp(argv[1], "-c") == 0)
			cmd = VI_CREATE;
		if (strcmp(argv[1], "-d") == 0)
			cmd = VI_DESTROY;
	}

	if (argc >= 3) {
		strcpy(vi_req.vi_name, argv[2]);
		if (strcmp(argv[1], "-c") == 0)
			cmd = VI_CREATE;
		if (strcmp(argv[1], "-i") == 0)
			cmd = VI_IFACE;
	}

	vi_req.vi_api_cookie = VI_API_COOKIE;
	vi_req.vi_req_action = cmd;
	switch (cmd) {

	case VI_GET:
		if (ioctl(s, SIOCGPVIMAGE, (caddr_t)&vi_req) < 0)
			goto abort;
		if (argc == 1)
			printf("%s\n", vi_req.vi_name);
		else
			vi_print(&vi_req);
		exit(0);

	case VI_GETNEXT:
	case VI_GETNEXT_RECURSE:
		vi_req.vi_req_action = VI_GET;
		if (ioctl(s, SIOCGPVIMAGE, (caddr_t)&vi_req) < 0)
			goto abort;
		vi_print(&vi_req);
		vi_req.vi_req_action = VI_GETNEXT_RECURSE;
		while (ioctl(s, SIOCGPVIMAGE, (caddr_t)&vi_req) == 0) {
			vi_print(&vi_req);
			vi_req.vi_req_action = cmd;
		}
		exit(0);

	case VI_IFACE:
		strncpy(vi_req.vi_if_xname, argv[3],
				sizeof(vi_req.vi_if_xname));
		if (ioctl(s, SIOCSIFVIMAGE, (caddr_t)&vi_req) < 0)
			goto abort;
		printf("%s@%s\n", vi_req.vi_if_xname, vi_req.vi_name);
		exit(0);

	case VI_CREATE:
		if (ioctl(s, SIOCSPVIMAGE, (caddr_t)&vi_req) < 0)
			goto abort;
		exit(0);

	case VI_SWITCHTO:
		strcpy(vi_req.vi_name, argv[1]);
		if (ioctl(s, SIOCSPVIMAGE, (caddr_t)&vi_req) < 0)
			goto abort;

		vi_req.vi_req_action = VI_GET;
		strcpy(vi_req.vi_name, ".");
		if (ioctl(s, SIOCGPVIMAGE, (caddr_t)&vi_req) < 0) {
			printf("XXX this should have not happened!\n");
			goto abort;
		}
		close(s);

		if (argc == 2) {
			printf("Switched to vimage %s\n", argv[1]);
			if ((shell = getenv("SHELL")) == NULL)
				execlp("/bin/sh", argv[0], NULL);
			else
				execlp(shell, argv[0], NULL);
		} else 
			execvp(argv[2], &argv[2]);
		break;

	case VI_DESTROY:
		if (ioctl(s, SIOCSPVIMAGE, (caddr_t)&vi_req) < 0)
			goto abort;
		exit(0);

	default:
		fprintf(stderr, "usage: %s [-cdilr] vi_name [args]\n",
		    argv[0]);
		exit(1);
	}

abort:
	perror("Error");
	exit(1);
}
