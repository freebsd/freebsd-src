
/*
 * list.c
 *
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
 * $FreeBSD: src/usr.sbin/ngctl/list.c,v 1.2 1999/11/30 02:45:30 archie Exp $
 */

#include "ngctl.h"

static int ListCmd(int ac, char **av);

const struct ngcmd list_cmd = {
	ListCmd,
	"list [-n]",
	"Show information about all nodes",
	"The list command shows information every node that currently"
	" exists in the netgraph system. The optional -n argument limits"
	" this list to only those nodes with a global name assignment.",
	{ "ls" }
};

static int
ListCmd(int ac, char **av)
{
	u_char rbuf[16 * 1024];
	struct ng_mesg *const resp = (struct ng_mesg *) rbuf;
	struct namelist *const nlist = (struct namelist *) resp->data;
	int named_only = 0;
	int k, ch, rtn = CMDRTN_OK;

	/* Get options */
	optind = 1;
	while ((ch = getopt(ac, av, "n")) != EOF) {
		switch (ch) {
		case 'n':
			named_only = 1;
			break;
		case '?':
		default:
			return(CMDRTN_USAGE);
			break;
		}
	}
	ac -= optind;
	av += optind;

	/* Get arguments */
	switch (ac) {
	case 0:
		break;
	default:
		return(CMDRTN_USAGE);
	}

	/* Get list of nodes */
	if (NgSendMsg(csock, ".", NGM_GENERIC_COOKIE,
	    named_only ? NGM_LISTNAMES : NGM_LISTNODES, NULL, 0) < 0) {
		warn("send msg");
		return(CMDRTN_ERROR);
	}
	if (NgRecvMsg(csock, resp, sizeof(rbuf), NULL) < 0) {
		warn("recv msg");
		return(CMDRTN_ERROR);
	}

	/* Show each node */
	printf("There are %d total %snodes:\n",
	    nlist->numnames, named_only ? "named " : "");
	for (k = 0; k < nlist->numnames; k++) {
		char	path[NG_PATHLEN+1];
		char	*av[3] = { "list", "-n", path };

		snprintf(path, sizeof(path),
		    "[%lx]:", (u_long) nlist->nodeinfo[k].id);
		if ((rtn = (*show_cmd.func)(3, av)) != CMDRTN_OK)
			break;
	}

	/* Done */
	return (rtn);
}

