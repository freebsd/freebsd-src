/*
 * Copyright (c) 1996-1999 Whistle Communications, Inc.
 * All rights reserved.
 * 
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Whistle Communications;
 * provided, however, that:
 * 1. Any and all reproductions of the source or object code must include the
 *    copyright notice above and the following disclaimer of warranties; and
 * 2. No rights are granted, in any manner or form, to use Whistle
 *    Communications, Inc. trademarks, including the mark "WHISTLE
 *    COMMUNICATIONS" on advertising, endorsements, or otherwise except as
 *    such appears in the above copyright notice or in the software.
 * 
 * THIS SOFTWARE IS BEING PROVIDED BY WHISTLE COMMUNICATIONS "AS IS", AND
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, WHISTLE COMMUNICATIONS MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE,
 * INCLUDING WITHOUT LIMITATION, ANY AND ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 * WHISTLE COMMUNICATIONS DOES NOT WARRANT, GUARANTEE, OR MAKE ANY
 * REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE USE OF THIS
 * SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY OR OTHERWISE.
 * IN NO EVENT SHALL WHISTLE COMMUNICATIONS BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF WHISTLE COMMUNICATIONS IS ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/netstat/netgraph.c,v 1.3 1999/10/24 02:58:39 dillon Exp $
 */

#ifndef lint
static const char rcsid[] =
	"$Id: atalk.c,v 1.11 1998/07/06 21:01:22 bde Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/linker.h>

#include <net/route.h>

#include <netgraph.h>
#include <netgraph/ng_message.h>
#include <netgraph/ng_socket.h>
#include <netgraph/ng_socketvar.h>

#include <nlist.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include "netstat.h"

static	int first = 1;
static	int csock = -1;

void
netgraphprotopr(u_long off, char *name)
{
	struct ngpcb *this, *next;
	struct ngpcb ngpcb;
	struct ngsock info;
	struct socket sockb;
	int debug = 1;

	/* If symbol not found, try looking in the KLD module */
	if (off == 0) {
		const char *const modname = "ng_socket.ko";
/* XXX We should get "mpath" from "sysctl kern.module_path" */
		const char *mpath[] = { "/", "/boot/", "/modules/", NULL };
		struct nlist sym[] = { { "_ngsocklist" }, { NULL } };
		const char **pre;
		struct kld_file_stat ks;
		int fileid;

		/* See if module is loaded */
		if ((fileid = kldfind(modname)) < 0) {
			if (debug)
				warn("kldfind(%s)", modname);
			return;
		}

		/* Get module info */
		memset(&ks, 0, sizeof(ks));
		ks.version = sizeof(struct kld_file_stat);
		if (kldstat(fileid, &ks) < 0) {
			if (debug)
				warn("kldstat(%d)", fileid);
			return;
		}

		/* Get symbol table from module file */
		for (pre = mpath; *pre; pre++) {
			char path[MAXPATHLEN];

			snprintf(path, sizeof(path), "%s%s", *pre, modname);
			if (nlist(path, sym) == 0)
				break;
		}

		/* Did we find it? */
		if (sym[0].n_value == 0) {
			if (debug)
				warnx("%s not found", modname);
			return;
		}

		/* Symbol found at load address plus symbol offset */
		off = (u_long) ks.address + sym[0].n_value;
	}

	/* Get pointer to first socket */
	kread(off, (char *)&this, sizeof(this));

	/* Get my own socket node */
	if (csock == -1)
		NgMkSockNode(NULL, &csock, NULL);

	for (; this != NULL; this = next) {
		u_char rbuf[sizeof(struct ng_mesg) + sizeof(struct nodeinfo)];
		struct ng_mesg *resp = (struct ng_mesg *) rbuf;
		struct nodeinfo *ni = (struct nodeinfo *) resp->data;
		char path[64];

		/* Read in ngpcb structure */
		kread((u_long)this, (char *)&ngpcb, sizeof(ngpcb));
		next = ngpcb.socks.le_next;

		/* Read in socket structure */
		kread((u_long)ngpcb.ng_socket, (char *)&sockb, sizeof(sockb));

		/* Check type of socket */
		if (strcmp(name, "ctrl") == 0 && ngpcb.type != NG_CONTROL)
			continue;
		if (strcmp(name, "data") == 0 && ngpcb.type != NG_DATA)
			continue;

		/* Do headline */
		if (first) {
			printf("Netgraph sockets\n");
			if (Aflag)
				printf("%-8.8s ", "PCB");
			printf("%-5.5s %-6.6s %-6.6s %-14.14s %s\n",
			    "Type", "Recv-Q", "Send-Q",
			    "Node Address", "#Hooks");
			first = 0;
		}

		/* Show socket */
		if (Aflag)
			printf("%8lx ", (u_long) this);
		printf("%-5.5s %6lu %6lu ",
		    name, sockb.so_rcv.sb_cc, sockb.so_snd.sb_cc);

		/* Get ngsock structure */
		if (ngpcb.sockdata == 0)	/* unconnected data socket */
			goto finish;
		kread((u_long)ngpcb.sockdata, (char *)&info, sizeof(info));

		/* Get info on associated node */
		if (info.node == 0 || csock == -1)
			goto finish;
		snprintf(path, sizeof(path), "[%lx]:", (u_long) info.node);
		if (NgSendMsg(csock, path,
		    NGM_GENERIC_COOKIE, NGM_NODEINFO, NULL, 0) < 0)
			goto finish;
		if (NgRecvMsg(csock, resp, sizeof(rbuf), NULL) < 0)
			goto finish;

		/* Display associated node info */
		if (*ni->name != '\0')
			snprintf(path, sizeof(path), "%s:", ni->name);
		printf("%-14.14s %4d", path, ni->hooks);
finish:
		putchar('\n');
	}
}

