
/*
 * db_sysctl.c
 *
 * Copyright (c) 2000 Whistle Communications, Inc.
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
 * Author: Archie Cobbs <archie@whistle.com>
 *
 * $FreeBSD: src/sys/ddb/db_sysctl.c,v 1.1.4.1 2000/08/03 00:09:27 ps Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/reboot.h>

#define DESCRIPTION	"Set to `ddb' or `gdb' to enter the kernel debugger"
#define READ_MODE	""
#define MODE_DDB	"ddb"
#define MODE_GDB	"gdb"

static int
sysctl_debug_enter_debugger(SYSCTL_HANDLER_ARGS)
{
	char dmode[64];
	int error;

	strncpy(dmode, READ_MODE, sizeof(dmode) - 1);
	dmode[sizeof(dmode) - 1] = '\0';
	error = sysctl_handle_string(oidp, &dmode[0], sizeof(dmode), req);

	if (error == 0 && req->newptr != NULL) {
		if (strcmp(dmode, MODE_DDB) == 0)
			boothowto &= ~RB_GDB;
		else if (strcmp(dmode, MODE_GDB) == 0)
			boothowto |= RB_GDB;
		else
			return EINVAL;
		Debugger("debug.enter_debugger");
	}
	return error;
}

SYSCTL_PROC(_debug, OID_AUTO, enter_debugger, CTLTYPE_STRING | CTLFLAG_RW,
    0, 0, sysctl_debug_enter_debugger, "A", DESCRIPTION);

