/*
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <machine/setjmp.h>
#include <sys/linker_set.h>
#include <proc_service.h>
#include <stdlib.h>
#include <string.h>
#include <thread_db.h>

#include "libc_r_db.h"
#include "thread_db_int.h"

struct td_thragent {
	TD_THRAGENT_FIELDS;
	struct ps_prochandle	*ta_ph;
	psaddr_t ta_thread_initial;
	psaddr_t ta_thread_list;
	psaddr_t ta_thread_run;
	int	ta_ofs_ctx;
	int	ta_ofs_next;
	int	ta_ofs_uniqueid;
};

static td_err_e
libc_r_db_init(void)
{
	return (TD_OK);
}

static td_err_e
libc_r_db_ta_clear_event(const td_thragent_t *ta __unused,
    td_thr_events_t *ev __unused)
{
	return (0);
}

static td_err_e
libc_r_db_ta_delete(td_thragent_t *ta)
{
	free(ta);
	return (TD_OK);
}

static td_err_e
libc_r_db_ta_event_addr(const td_thragent_t *ta __unused,
    td_thr_events_e ev __unused, td_notify_t *n __unused)
{
	return (TD_ERR);
}

static td_err_e
libc_r_db_ta_event_getmsg(const td_thragent_t *ta __unused,
    td_event_msg_t *msg __unused)
{
	return (TD_ERR);
}

static td_err_e
libc_r_db_ta_map_id2thr(const td_thragent_t *ta __unused, thread_t tid __unused,
    td_thrhandle_t *th __unused)
{
	return (TD_ERR);
}

static td_err_e
libc_r_db_ta_map_lwp2thr(const td_thragent_t *ta, lwpid_t lwpid __unused,
    td_thrhandle_t *th)
{
	psaddr_t addr;
	ps_err_e err;

	th->th_ta = ta;
	err = ps_pread(ta->ta_ph, ta->ta_thread_initial, &addr, sizeof(addr));
	if (err != PS_OK)
		return (TD_ERR);
	if (addr == 0)
		return (TD_NOLWP);
	err = ps_pread(ta->ta_ph, ta->ta_thread_run, &th->th_thread,
	    sizeof(psaddr_t));
	return ((err == PS_OK) ? TD_OK : TD_ERR);
}

static td_err_e
libc_r_db_ta_new(struct ps_prochandle *ph, td_thragent_t **ta_p)
{
	td_thragent_t *ta;
	psaddr_t addr;
	ps_err_e err;

	ta = malloc(sizeof(td_thragent_t));
	if (ta == NULL)
		return (TD_MALLOC);

	ta->ta_ph = ph;

	err = ps_pglobal_lookup(ph, NULL, "_thread_initial",
	    &ta->ta_thread_initial);
	if (err != PS_OK)
		goto fail;
	err = ps_pglobal_lookup(ph, NULL, "_thread_list", &ta->ta_thread_list);
	if (err != PS_OK)
		goto fail;
	err = ps_pglobal_lookup(ph, NULL, "_thread_run", &ta->ta_thread_run);
	if (err != PS_OK)
		goto fail;
	err = ps_pglobal_lookup(ph, NULL, "_thread_ctx_offset", &addr);
	if (err != PS_OK)
		goto fail;
	err = ps_pread(ph, addr, &ta->ta_ofs_ctx, sizeof(int));
	if (err != PS_OK)
		goto fail;
	err = ps_pglobal_lookup(ph, NULL, "_thread_next_offset", &addr);
	if (err != PS_OK)
		goto fail;
	err = ps_pread(ph, addr, &ta->ta_ofs_next, sizeof(int));
	if (err != PS_OK)
		goto fail;
	err = ps_pglobal_lookup(ph, NULL, "_thread_uniqueid_offset", &addr);
	if (err != PS_OK)
		goto fail;
	err = ps_pread(ph, addr, &ta->ta_ofs_uniqueid, sizeof(int));
	if (err != PS_OK)
		goto fail;

	*ta_p = ta;
	return (TD_OK);

 fail:
	free(ta);
	*ta_p = NULL;
	return (TD_ERR);
}

static td_err_e
libc_r_db_ta_set_event(const td_thragent_t *ta __unused,
    td_thr_events_t *ev __unused)
{
	return (0);
}

static td_err_e
libc_r_db_ta_thr_iter(const td_thragent_t *ta, td_thr_iter_f *cb, void *data,
    td_thr_state_e state __unused, int pri __unused, sigset_t *mask __unused,
    unsigned int flags __unused)
{
	td_thrhandle_t th;
	psaddr_t addr;
	ps_err_e err;

	th.th_ta = ta;

	err = ps_pread(ta->ta_ph, ta->ta_thread_list, &th.th_thread,
	    sizeof(th.th_thread));
	if (err != PS_OK)
		return (TD_ERR);
	while (th.th_thread != 0) {
		if (cb(&th, data) != 0)
			return (TD_OK);
		addr = (psaddr_t)(th.th_thread + ta->ta_ofs_next);
		err = ps_pread(ta->ta_ph, addr, &th.th_thread,
		    sizeof(th.th_thread));
		if (err != PS_OK)
			return (TD_ERR);
	}
	return (TD_OK);
}

static td_err_e
libc_r_db_thr_clear_event(const td_thrhandle_t *th __unused,
    td_thr_events_t *ev __unused)
{
	return (0);
}

static td_err_e
libc_r_db_thr_event_enable(const td_thrhandle_t *th __unused, int oo __unused)
{
	return (0);
}

static td_err_e
libc_r_db_thr_event_getmsg(const td_thrhandle_t *th __unused,
    td_event_msg_t *msg __unused)
{
	return (TD_ERR);
}

static td_err_e
libc_r_db_thr_get_info(const td_thrhandle_t *th, td_thrinfo_t *ti)
{
	const td_thragent_t *ta;
	psaddr_t addr, current;
	ps_err_e err;

	ta = th->th_ta;
	ti->ti_ta_p = ta;
	err = ps_pread(ta->ta_ph, ta->ta_thread_run, &current,
	    sizeof(psaddr_t));
	if (err != PS_OK)
		return (TD_ERR);
	ti->ti_lid = (th->th_thread == current) ? -1 : 0;
	addr = (psaddr_t)((uintptr_t)th->th_thread + ta->ta_ofs_uniqueid);
	err = ps_pread(ta->ta_ph, addr, &ti->ti_tid, sizeof(thread_t));
	/* libc_r numbers its threads starting with 0. Not smart. */
	ti->ti_tid++;
	return ((err == PS_OK) ? TD_OK : TD_ERR);
}

#ifdef __i386__
static td_err_e
libc_r_db_thr_getxmmregs(const td_thrhandle_t *th __unused,
    char *fxsave __unused)
{
	return (TD_NOFPREGS);
}
#endif

static td_err_e
libc_r_db_thr_getfpregs(const td_thrhandle_t *th, prfpregset_t *r)
{
	jmp_buf jb;
	const td_thragent_t *ta;
	psaddr_t addr;
	ps_err_e err;

	ta = th->th_ta;
	err = ps_lgetfpregs(ta->ta_ph, -1, r);
	if (err != PS_OK)
		return (TD_ERR);
	err = ps_pread(ta->ta_ph, ta->ta_thread_run, &addr, sizeof(psaddr_t));
	if (err != PS_OK)
		return (TD_ERR);
	if (th->th_thread == addr)
		return (TD_OK);
	addr = (psaddr_t)((uintptr_t)th->th_thread + ta->ta_ofs_ctx);
	err = ps_pread(ta->ta_ph, addr, jb, sizeof(jb));
	if (err != PS_OK)
		return (TD_ERR);
	libc_r_md_getfpregs(jb, r);
	return (TD_OK);
}

static td_err_e
libc_r_db_thr_getgregs(const td_thrhandle_t *th, prgregset_t r)
{
	jmp_buf jb;
	const td_thragent_t *ta;
	psaddr_t addr;
	ps_err_e err;

	ta = th->th_ta;
	err = ps_lgetregs(ta->ta_ph, -1, r);
	if (err != PS_OK)
		return (TD_ERR);
	err = ps_pread(ta->ta_ph, ta->ta_thread_run, &addr, sizeof(psaddr_t));
	if (err != PS_OK)
		return (TD_ERR);
	if (th->th_thread == addr)
		return (TD_OK);
	addr = (psaddr_t)((uintptr_t)th->th_thread + ta->ta_ofs_ctx);
	err = ps_pread(ta->ta_ph, addr, jb, sizeof(jb));
	if (err != PS_OK)
		return (TD_ERR);
	libc_r_md_getgregs(jb, r);
	return (TD_OK);
}

static td_err_e
libc_r_db_thr_set_event(const td_thrhandle_t *th __unused,
    td_thr_events_t *ev __unused)
{
	return (0);
}

#ifdef __i386__
static td_err_e
libc_r_db_thr_setxmmregs(const td_thrhandle_t *th __unused,
    const char *fxsave __unused)
{
	return (TD_NOFPREGS);
}
#endif

static td_err_e
libc_r_db_thr_setfpregs(const td_thrhandle_t *th __unused,
    const prfpregset_t *r __unused)
{
	return (TD_ERR);
}

static td_err_e
libc_r_db_thr_setgregs(const td_thrhandle_t *th __unused,
    const prgregset_t r __unused)
{
	return (TD_ERR);
}

static td_err_e
libc_r_db_thr_validate(const td_thrhandle_t *th __unused)
{
	return (TD_ERR);
}

struct ta_ops libc_r_db_ops = {
	.to_init		= libc_r_db_init,

	.to_ta_clear_event	= libc_r_db_ta_clear_event,
	.to_ta_delete		= libc_r_db_ta_delete,
	.to_ta_event_addr	= libc_r_db_ta_event_addr,
	.to_ta_event_getmsg	= libc_r_db_ta_event_getmsg,
	.to_ta_map_id2thr	= libc_r_db_ta_map_id2thr,
	.to_ta_map_lwp2thr	= libc_r_db_ta_map_lwp2thr,
	.to_ta_new		= libc_r_db_ta_new,
	.to_ta_set_event	= libc_r_db_ta_set_event,
	.to_ta_thr_iter		= libc_r_db_ta_thr_iter,

	.to_thr_clear_event     = libc_r_db_thr_clear_event,
	.to_thr_event_enable    = libc_r_db_thr_event_enable,
	.to_thr_event_getmsg	= libc_r_db_thr_event_getmsg,
	.to_thr_get_info        = libc_r_db_thr_get_info,
	.to_thr_getfpregs       = libc_r_db_thr_getfpregs,
	.to_thr_getgregs        = libc_r_db_thr_getgregs,
	.to_thr_set_event       = libc_r_db_thr_set_event,
	.to_thr_setfpregs       = libc_r_db_thr_setfpregs,
	.to_thr_setgregs        = libc_r_db_thr_setgregs,
	.to_thr_validate        = libc_r_db_thr_validate,
#ifdef __i386__
	.to_thr_getxmmregs	= libc_r_db_thr_getxmmregs,
	.to_thr_setxmmregs	= libc_r_db_thr_setxmmregs,
#endif
};

DATA_SET(__ta_ops, libc_r_db_ops);
