/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)v_switch.c	8.7 (Berkeley) 3/8/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"
#include "excmd.h"

/*
 * v_switch -- ^^
 *	Switch to the previous file.
 */
int
v_switch(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	EXCMDARG cmd;
	char *name;

	/*
	 * Try the alternate file name, then the previous file
	 * name.  Use the real name, not the user's current name.
	 */
	if (sp->alt_name != NULL)
		name = sp->alt_name;
	else if (sp->p_frp != NULL)
		name = sp->p_frp->name;
	else {
		msgq(sp, M_ERR, "No previous file to edit.");
		return (1);
	}

	/* If autowrite is set, write out the file. */
	if (F_ISSET(ep, F_MODIFIED))
		if (O_ISSET(sp, O_AUTOWRITE)) {
			if (file_write(sp, ep, NULL, NULL, NULL, FS_ALL))
				return (1);
		} else {
			msgq(sp, M_ERR,
		"Modified since last write; write or use :edit! to override.");
			return (1);
		}

	SETCMDARG(cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0, name);
	return (sp->s_ex_cmd(sp, ep, &cmd, &vp->m_final));
}
