
/*
 * msg.c
 *
 * Copyright (c) 1999 Whistle Communications, Inc.
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
 * $Whistle: msg.c,v 1.2 1999/11/29 23:38:35 archie Exp $
 * $FreeBSD$
 */

#include "ngctl.h"

#define BUF_SIZE	1024

static int MsgCmd(int ac, char **av);

const struct ngcmd msg_cmd = {
	MsgCmd,
	"msg path command [args ... ]",
	"Send a netgraph control message to the node at \"path\"",
	"The msg command constructs a netgraph control message from the"
	" command name and ASCII arguments (if any) and sends that message"
	" to the node.  It does this by first asking the node to convert"
	" the ASCII message into binary format, and resending the result."
	" The typecookie used for the message is assumed to be the typecookie"
	" corresponding to the target node's type.",
	{ "cmd" }
};

static int
MsgCmd(int ac, char **av)
{
	char buf[BUF_SIZE];
	char *path, *cmdstr;
	int i;

	/* Get arguments */
	if (ac < 3)
		return(CMDRTN_USAGE);
	path = av[1];
	cmdstr = av[2];

	/* Put command and arguments back together as one string */
	for (*buf = '\0', i = 3; i < ac; i++) {
		snprintf(buf + strlen(buf),
		    sizeof(buf) - strlen(buf), " %s", av[i]);
	}

	/* Send it */
	if (NgSendAsciiMsg(csock, path, "%s%s", cmdstr, buf) < 0) {
		warn("send msg");
		return(CMDRTN_ERROR);
	}

	/* Done */
	return(CMDRTN_OK);
}

