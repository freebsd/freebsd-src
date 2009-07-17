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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/jail.h>
#include <sys/socket.h>

#include <net/if.h>

#include <errno.h>
#include <jail.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	VI_CREATE		0x00000001
#define	VI_DESTROY		0x00000002
#define	VI_SWITCHTO		0x00000008
#define	VI_IFACE		0x00000010
#define	VI_GET			0x00000100
#define	VI_GETNEXT		0x00000200

static int getjail(char *name, int lastjid, int *vnet);

int
main(int argc, char **argv)
{
	int s;
	char *shell;
	int cmd;
	int jid, vnet;
	struct ifreq ifreq;
	char name[MAXHOSTNAMELEN];

	switch (argc) {

	case 1:
		cmd = 0;
		break;

	case 2:
		if (strcmp(argv[1], "-l") == 0)
			cmd = VI_GETNEXT;
		else if (strcmp(argv[1], "-lr") == 0)
			cmd = VI_GETNEXT;
		else {
			strcpy(name, argv[1]);
			cmd = VI_SWITCHTO;
		}
		break;

	case 3:
		strcpy(name, argv[2]);
		if (strcmp(argv[1], "-l") == 0)
			cmd = VI_GET;
		if (strcmp(argv[1], "-c") == 0)
			cmd = VI_CREATE;
		if (strcmp(argv[1], "-d") == 0)
			cmd = VI_DESTROY;
		break;

	default:
		strcpy(name, argv[2]);
		if (strcmp(argv[1], "-c") == 0)
			cmd = VI_CREATE;
		if (strcmp(argv[1], "-i") == 0)
			cmd = VI_IFACE;
	}

	switch (cmd) {

	case VI_GET:
		jid = getjail(name, -1, &vnet);
		if (jid < 0)
			goto abort;
		printf("%d: %s%s\n", jid, name, vnet ? "" : " (no vnet)");
		exit(0);

	case VI_GETNEXT:
		jid = 0;
		while ((jid = getjail(name, jid, &vnet)) > 0)
			printf("%d: %s%s\n", jid, name,
			    vnet ? "" : " (no vnet)");
		exit(0);

	case VI_IFACE:
		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s == -1)
			goto abort;
		jid = jail_getid(name);
		if (jid < 0)
			goto abort;
		ifreq.ifr_jid = jid;
		strncpy(ifreq.ifr_name, argv[3], sizeof(ifreq.ifr_name));
		if (ioctl(s, SIOCSIFVNET, (caddr_t)&ifreq) < 0)
			goto abort;
		printf("%s@%s\n", ifreq.ifr_name, name);
		exit(0);

	case VI_CREATE:
		if (jail_setv(JAIL_CREATE, "name", name, "vnet", NULL,
		    "host", NULL, "persist", NULL, NULL) < 0)
			goto abort;
		exit(0);

	case VI_SWITCHTO:
		jid = jail_getid(name);
		if (jid < 0)
			goto abort;
		if (jail_attach(jid) < 0)
			goto abort;

		if (argc == 2) {
			printf("Switched to jail %s\n", argv[1]);
			if ((shell = getenv("SHELL")) == NULL)
				execlp("/bin/sh", argv[0], NULL);
			else
				execlp(shell, argv[0], NULL);
		} else 
			execvp(argv[2], &argv[2]);
		break;

	case VI_DESTROY:
		jid = jail_getid(name);
		if (jid < 0)
			goto abort;
		if (jail_remove(jid) < 0)
			goto abort;
		exit(0);

	default:
		fprintf(stderr, "usage: %s [-cdilr] vi_name [args]\n",
		    argv[0]);
		exit(1);
	}

abort:
	if (jail_errmsg[0])
		fprintf(stderr, "Error: %s\n", jail_errmsg);
	else
		perror("Error");
	exit(1);
}

static int
getjail(char *name, int lastjid, int *vnet)
{
	struct jailparam params[3];
	int jid;

	if (lastjid < 0) {
		jid = jail_getid(name);
		if (jid < 0)
			return (jid);
		jailparam_init(&params[0], "jid");
		jailparam_import_raw(&params[0], &jid, sizeof jid);
	} else {
		jailparam_init(&params[0], "lastjid");
		jailparam_import_raw(&params[0], &lastjid, sizeof lastjid);
	}
	jailparam_init(&params[1], "name");
	jailparam_import_raw(&params[1], name, MAXHOSTNAMELEN);
	name[0] = 0;
	jailparam_init(&params[2], "vnet");
	jailparam_import_raw(&params[2], vnet, sizeof(*vnet));
	jid = jailparam_get(params, 3, 0);
	jailparam_free(params, 3);
	return (jid);
}
