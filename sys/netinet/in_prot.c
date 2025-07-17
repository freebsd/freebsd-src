/*-
 * Copyright (c) 2000-2001 Robert N. M. Watson.
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
 */

/*
 * Helpers related to visibility and protection of sockets and inpcb.
 */

#include <sys/systm.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

/*-
 * Determine whether the subject represented by cred can "see" a socket.
 * Returns: 0 for permitted, ENOENT otherwise.
 */
int
cr_canseeinpcb(struct ucred *cred, struct inpcb *inp)
{
	int error;

	error = prison_check(cred, inp->inp_cred);
	if (error)
		return (ENOENT);
#ifdef MAC
	INP_LOCK_ASSERT(inp);
	error = mac_inpcb_check_visible(cred, inp);
	if (error)
		return (error);
#endif
	if (cr_bsd_visible(cred, inp->inp_cred))
		return (ENOENT);

	return (0);
}

bool
cr_canexport_ktlskeys(struct thread *td, struct inpcb *inp)
{
	int error;

	if (cr_canseeinpcb(td->td_ucred, inp) == 0 &&
	    cr_xids_subset(td->td_ucred, inp->inp_cred))
		return (true);
	error = priv_check(td, PRIV_NETINET_KTLSKEYS);
	return (error == 0);

}
