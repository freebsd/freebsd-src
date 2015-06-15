/*
 * Copyright (c) 2010 The FreeBSD Foundation 
 * All rights reserved. 
 * 
 * This software was developed by Rui Paulo under sponsorship from the
 * FreeBSD Foundation. 
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
 */ 
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <machine/_inttypes.h>
#include <sys/types.h>
#include <sys/user.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libproc.h>
#include <libutil.h>

#include "rtld_db.h"

static int _librtld_db_debug = 0;
#define DPRINTF(...) do {				\
	if (_librtld_db_debug) {			\
		fprintf(stderr, "librtld_db: DEBUG: ");	\
		fprintf(stderr, __VA_ARGS__);		\
	}						\
} while (0)

void
rd_delete(rd_agent_t *rdap)
{

	free(rdap);
}

const char *
rd_errstr(rd_err_e rderr)
{

	switch (rderr) {
	case RD_ERR:
		return "generic error";
	case RD_OK:
		return "no error";
	case RD_NOCAPAB:
		return "capability not supported";
	case RD_DBERR:
		return "database error";
	case RD_NOBASE:
		return "NOBASE";
	case RD_NOMAPS:
		return "NOMAPS";
	default:
		return "unknown error";
	}
}

rd_err_e
rd_event_addr(rd_agent_t *rdap, rd_event_e event, rd_notify_t *notify)
{
	rd_err_e ret;

	DPRINTF("%s rdap %p event %d notify %p\n", __func__, rdap, event,
	    notify);

	ret = RD_OK;
	switch (event) {
	case RD_NONE:
		break;
	case RD_PREINIT:
		notify->type = RD_NOTIFY_BPT;
		notify->u.bptaddr = rdap->rda_preinit_addr;
		break;
	case RD_POSTINIT:
		notify->type = RD_NOTIFY_BPT;
		notify->u.bptaddr = rdap->rda_postinit_addr;
		break;
	case RD_DLACTIVITY:
		notify->type = RD_NOTIFY_BPT;
		notify->u.bptaddr = rdap->rda_dlactivity_addr;
		break;
	default:
		ret = RD_ERR;
		break;
	}
	return (ret);
}

rd_err_e
rd_event_enable(rd_agent_t *rdap __unused, int onoff)
{
	DPRINTF("%s onoff %d\n", __func__, onoff);

	return (RD_OK);
}

rd_err_e
rd_event_getmsg(rd_agent_t *rdap __unused, rd_event_msg_t *msg)
{
	DPRINTF("%s\n", __func__);

	msg->type = RD_POSTINIT;
	msg->u.state = RD_CONSISTENT;

	return (RD_OK);
}

rd_err_e
rd_init(int version)
{
	char *debug = NULL;

	if (version == RD_VERSION) {
		debug = getenv("LIBRTLD_DB_DEBUG");
		_librtld_db_debug = debug ? atoi(debug) : 0;
		return (RD_OK);
	} else
		return (RD_NOCAPAB);
}

rd_err_e
rd_loadobj_iter(rd_agent_t *rdap, rl_iter_f *cb, void *clnt_data)
{
	int cnt, i, lastvn = 0;
	rd_loadobj_t rdl;
	struct kinfo_vmentry *kves, *kve;

	DPRINTF("%s\n", __func__);

        if ((kves = kinfo_getvmmap(proc_getpid(rdap->rda_php), &cnt)) == NULL) {
		warn("ERROR: kinfo_getvmmap() failed");
		return (RD_ERR);
	}
	for (i = 0; i < cnt; i++) {
		kve = kves + i;
		if (kve->kve_type == KVME_TYPE_VNODE)
			lastvn = i;
		memset(&rdl, 0, sizeof(rdl));
		/*
		 * Map the kinfo_vmentry struct to the rd_loadobj structure.
		 */
		rdl.rdl_saddr = kve->kve_start;
		rdl.rdl_eaddr = kve->kve_end;
		rdl.rdl_offset = kve->kve_offset;
		if (kve->kve_protection & KVME_PROT_READ)
			rdl.rdl_prot |= RD_RDL_R;
		if (kve->kve_protection & KVME_PROT_WRITE)
			rdl.rdl_prot |= RD_RDL_W;
		if (kve->kve_protection & KVME_PROT_EXEC)
			rdl.rdl_prot |= RD_RDL_X;
		strlcpy(rdl.rdl_path, kves[lastvn].kve_path,
			sizeof(rdl.rdl_path));
		(*cb)(&rdl, clnt_data);
	}
	free(kves);

	return (RD_OK);
}

void
rd_log(const int onoff)
{
	DPRINTF("%s\n", __func__);

	(void)onoff;
}

rd_agent_t *
rd_new(struct proc_handle *php)
{
	rd_agent_t *rdap;

	rdap = malloc(sizeof(rd_agent_t));
	if (rdap) {
		memset(rdap, 0, sizeof(rd_agent_t));
		rdap->rda_php = php;
		rd_reset(rdap);
	}

	return (rdap);
}

rd_err_e
rd_objpad_enable(rd_agent_t *rdap, size_t padsize)
{
	DPRINTF("%s\n", __func__);

	(void)rdap;
	(void)padsize;

	return (RD_ERR);
}

rd_err_e
rd_plt_resolution(rd_agent_t *rdap, uintptr_t pc, struct proc *proc,
    uintptr_t plt_base, rd_plt_info_t *rpi)
{
	DPRINTF("%s\n", __func__);

	(void)rdap;
	(void)pc;
	(void)proc;
	(void)plt_base;
	(void)rpi;

	return (RD_ERR);
}

rd_err_e
rd_reset(rd_agent_t *rdap)
{
	GElf_Sym sym;

	if (proc_name2sym(rdap->rda_php, "ld-elf.so.1", "r_debug_state",
	    &sym, NULL) < 0)
		return (RD_ERR);
	DPRINTF("found r_debug_state at 0x%lx\n", (unsigned long)sym.st_value);
	rdap->rda_preinit_addr = sym.st_value;
	rdap->rda_dlactivity_addr = sym.st_value;

	if (proc_name2sym(rdap->rda_php, "ld-elf.so.1", "_r_debug_postinit",
	    &sym, NULL) < 0)
		return (RD_ERR);
	DPRINTF("found _r_debug_postinit at 0x%lx\n",
	    (unsigned long)sym.st_value);
	rdap->rda_postinit_addr = sym.st_value;

	return (RD_OK);
}
