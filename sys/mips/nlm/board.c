/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NETLOGIC_BSD */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <mips/nlm/hal/mips-extns.h>
#include <mips/nlm/hal/mmio.h>
#include <mips/nlm/hal/iomap.h>
#include <mips/nlm/hal/fmn.h>
#include <mips/nlm/hal/pic.h>
#include <mips/nlm/hal/uart.h>

#include <mips/nlm/board.h>

struct xlp_board_info xlp_board_info;

int nlm_setup_xlp_board(void);

/*
 * All our knowledge of chip and board that cannot be detected by probing
 * at run-time goes here
 */

int
nlm_setup_xlp_board(void)
{
	struct xlp_board_info	*boardp;
	int	node;

	/* start with a clean slate */
	boardp = &xlp_board_info;
	memset(boardp, 0, sizeof(xlp_board_info));
	boardp->nodemask = 0x1;	/* only node 0 */

	for (node = 0; node < XLP_MAX_NODES; node++) {
		if ((boardp->nodemask & (1 << node)) == 0)
			continue;
	}
	return 0;
}

int nlm_board_info_setup()
{
	nlm_setup_xlp_board();
	return 0;
}
