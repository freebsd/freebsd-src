/*-
 * Copyright (c) 2004, 2005 Gleb Smirnoff <glebius@FreeBSD.org>
 * Copyright (c) 2001-2003 Roman V. Palagin <romanp@unshadow.net>
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
 * $SourceForge: flowctl.c,v 1.15 2004/08/31 20:24:58 glebius Exp $
 */

#ifndef lint
static const char rcs_id[] =
    "@(#) $FreeBSD$";
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <netinet/in.h>

#include <arpa/inet.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netgraph.h>
#include <netgraph/netflow/ng_netflow.h>

#define	CISCO_SH_FLOW_HEADER	"SrcIf         SrcIPaddress    DstIf         DstIPaddress    Pr SrcP DstP  Pkts\n"
#define	CISCO_SH_FLOW	"%-13s %-15s %-13s %-15s %2u %4.4x %4.4x %6lu\n"

int main(int, char **);

static int flow_cache_print(struct ngnf_flows *recs);
static int ctl_show(int, char **);
static void help(void);
static void execute_command(int, char **);

struct ip_ctl_cmd {
	char	*cmd_name;
	int	(*cmd_func)(int argc, char **argv);
};

struct ip_ctl_cmd cmds[] = {
    {"show",	ctl_show},
    {NULL,	NULL},
};

int	cs;
char	ng_nodename[NG_PATHLEN + 1];

int
main(int argc, char **argv)
{
	int c;
	char sname[NG_NODESIZ];
	int rcvbuf = SORCVBUF_SIZE;
	char	*ng_name;

	/* parse options */
	while ((c = getopt(argc, argv, "d:")) != -1) {
		switch (c) {
		case 'd':	/* set libnetgraph debug level. */
			NgSetDebug(atoi(optarg));
			break;
		}
	}

	argc -= optind;
	argv += optind;
	ng_name = argv[0];
	if (ng_name == NULL)
		help();
	argc--;
	argv++;

	snprintf(ng_nodename, sizeof(ng_nodename), "%s:", ng_name);

	/* create control socket. */
	snprintf(sname, sizeof(sname), "flowctl%i", getpid());

	if (NgMkSockNode(sname, &cs, NULL) == -1)
		err(1, "NgMkSockNode");

	/* set receive buffer size */
	if (setsockopt(cs, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(int)) == -1)
		err(1, "setsockopt(SOL_SOCKET, SO_RCVBUF)");

	/* parse and execute command */
	execute_command(argc, argv);

	close(cs);
	
	exit(0);
}

static void
execute_command(int argc, char **argv)
{
	int cindex = -1;
	int i;

	if (!argc)
		help();
	for (i = 0; cmds[i].cmd_name != NULL; i++)
		if (!strncmp(argv[0], cmds[i].cmd_name, strlen(argv[0]))) {
			if (cindex != -1)
				errx(1, "ambiguous command: %s", argv[0]);
			cindex = i;
		}
	if (cindex == -1)
		errx(1, "bad command: %s", argv[0]);
	argc--;
	argv++;
	(*cmds[cindex].cmd_func)(argc, argv);
}

static int
ctl_show(int argc, char **argv)
{
	struct ng_mesg *ng_mesg;
	struct ngnf_flows *data;
	char path[NG_PATHLEN + 1];
	int token, nread, last = 0;

	ng_mesg = alloca(SORCVBUF_SIZE);

	printf(CISCO_SH_FLOW_HEADER);

	for (;;) {
		/* request set of accounting records */
		token = NgSendMsg(cs, ng_nodename, NGM_NETFLOW_COOKIE,
		    NGM_NETFLOW_SHOW, (void *)&last, sizeof(last));
		if (token == -1)
			err(1, "NgSendMsg(NGM_NETFLOW_SHOW)");

		/* read reply */
		nread = NgRecvMsg(cs, ng_mesg, SORCVBUF_SIZE, path);
		if (nread == -1)
			err(1, "NgRecvMsg() failed");

		if (ng_mesg->header.token != token)
			err(1, "NgRecvMsg(NGM_NETFLOW_SHOW): token mismatch");

		data = (struct ngnf_flows*)ng_mesg->data;
		if ((ng_mesg->header.arglen < (sizeof(*data))) ||
		    (ng_mesg->header.arglen < (sizeof(*data) +
		    (data->nentries * sizeof(struct flow_entry_data)))))
			err(1, "NgRecvMsg(NGM_NETFLOW_SHOW): arglen too small");

		(void )flow_cache_print(data);

		if (data->last != 0)
			last = data->last;
		else
			break;
	}
	
	return (0);
}

static int
flow_cache_print(struct ngnf_flows *recs)
{
	struct flow_entry_data *fle;
	char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
	char src_if[IFNAMSIZ], dst_if[IFNAMSIZ];
	int i;

	/* quick check */
	if (recs->nentries == 0)
		return (0);

	fle = recs->entries;
	for (i = 0; i < recs->nentries; i++, fle++) {
		inet_ntop(AF_INET, &fle->r.r_src, src, sizeof(src));
		inet_ntop(AF_INET, &fle->r.r_dst, dst, sizeof(dst));
		printf(CISCO_SH_FLOW,
			if_indextoname(fle->fle_i_ifx, src_if),
			src,
			if_indextoname(fle->fle_o_ifx, dst_if),
			dst,
			fle->r.r_ip_p,
			ntohs(fle->r.r_sport),
			ntohs(fle->r.r_dport),
			fle->packets);
			
	}
	
	return (i);
}

static void
help(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-d level] nodename command\n", __progname);
	exit (0);
}
