/*
 * Copyright (c) 1999, 2000, 2001 Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_sock.h>
#include <netncp/ncp_subr.h>
#include <netncp/ncp_rq.h>
#include <netncp/ncp_ncp.h>
#include <netncp/nwerror.h>

int ncp_debuglevel = 0;

struct callout_handle ncp_timer_handle;

static void ncp_at_exit(struct proc *p);
static void ncp_timer(void *arg);

/*
 * duplicate string from user space. It should be very-very slow.
 */
char *
ncp_str_dup(char *s) {
	char *p, bt;
	int len = 0;

	for (p = s;;p++) {
		if (copyin(p, &bt, 1)) return NULL;
		len++;
		if (bt == 0) break;
	}
	MALLOC(p, char*, len, M_NCPDATA, 0);
	copyin(s, p, len);
	return p;
}


void
ncp_at_exit(struct proc *p)
{
	struct ncp_conn *ncp, *nncp;

	if (ncp_conn_putprochandles(p) == 0) return;

	ncp_conn_locklist(LK_EXCLUSIVE, p);
	for (ncp = SLIST_FIRST(&conn_list); ncp; ncp = nncp) {
		nncp = SLIST_NEXT(ncp, nc_next);
		if (ncp_conn_lock(ncp, p, p->p_ucred,NCPM_READ|NCPM_EXECUTE|NCPM_WRITE))
			continue;
		if (ncp_conn_free(ncp) != 0)
			ncp_conn_unlock(ncp,p);
	}
	ncp_conn_unlocklist(p);
	return;
}

int
ncp_init(void)
{
	ncp_conn_init();
	if (at_exit(ncp_at_exit)) {
		NCPFATAL("can't register at_exit handler\n");
		return ENOMEM;
	}
	ncp_timer_handle = timeout(ncp_timer,NULL,NCP_TIMER_TICK);
	return 0;
}

int
ncp_done(void)
{
	int error;

	error = ncp_conn_destroy();
	if (error)
		return error;
	untimeout(ncp_timer,NULL,ncp_timer_handle);
	rm_at_exit(ncp_at_exit);
	return 0;
}


/* tick every second and check for watch dog packets and lost connections */
static void
ncp_timer(void *arg)
{
	struct ncp_conn *conn;

	if(ncp_conn_locklist(LK_SHARED | LK_NOWAIT, NULL) == 0) {
		SLIST_FOREACH(conn, &conn_list, nc_next)
			ncp_check_conn(conn);
		ncp_conn_unlocklist(NULL);
	}
	ncp_timer_handle = timeout(ncp_timer,NULL,NCP_TIMER_TICK);
}
