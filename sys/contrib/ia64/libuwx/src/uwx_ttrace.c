/*
 * Copyright (c) 2002,2003 Hewlett-Packard Company
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _KERNEL
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/ttrace.h>
#include <sys/uc_access.h>
#include <machine/sys/uregs.h>
#endif

#include "uwx_env.h"
#include "uwx_context.h"
#include "uwx_trace.h"
#include "uwx_ttrace.h"

struct uwx_ttrace_info {
    uint64_t bspstore;
    uint64_t load_map;
    uint64_t rvec[10];
    alloc_cb allocate_cb;
    free_cb free_cb;
    int have_ucontext;
    pid_t pid;
    lwpid_t lwpid;
    int trace;
    ucontext_t ucontext;
};

void *uwx_ttrace_memcpy(void *buffer, uint64_t ptr, size_t bufsiz, int ident)
{
    uint64_t *dest;
    uint64_t val;
    int status;

    status = ttrace(TT_PROC_RDDATA, (pid_t)ident,
			0, ptr, bufsiz, (uint64_t)buffer);
    if (status != 0)
	return NULL;
    return buffer;
}

struct uwx_ttrace_info *uwx_ttrace_init_info(
    struct uwx_env *env,
    pid_t pid,
    lwpid_t lwpid,
    uint64_t load_map)
{
    struct uwx_ttrace_info *info;

    if (env->allocate_cb == 0)
	info = (struct uwx_ttrace_info *)
			malloc(sizeof(struct uwx_ttrace_info));
    else
	info = (struct uwx_ttrace_info *)
			(*env->allocate_cb)(sizeof(struct uwx_ttrace_info));
    if (info == 0)
	return 0;

    info->bspstore = 0;
    info->load_map = load_map;
    info->allocate_cb = env->allocate_cb;
    info->free_cb = env->free_cb;
    info->have_ucontext = 0;
    info->pid = pid;
    info->lwpid = lwpid;
    info->trace = env->trace;
    return info;
}

int uwx_ttrace_free_info(struct uwx_ttrace_info *info)
{
    if (info->free_cb == 0)
	free((void *)info);
    else
	(*info->free_cb)((void *)info);
    return UWX_OK;
}

int uwx_ttrace_init_context(struct uwx_env *env, struct uwx_ttrace_info *info)
{
    uint64_t reason;
    uint64_t ip;
    uint64_t sp;
    uint64_t bsp;
    uint64_t cfm;
    uint64_t ec;
    int status;

    status = ttrace(TT_LWP_RUREGS, info->pid, info->lwpid,
	    (uint64_t)__reason, (uint64_t)8, (uint64_t)&reason);
    if (status != 0)
	return UWX_TT_ERR_TTRACE;
    status = ttrace(TT_LWP_RUREGS, info->pid, info->lwpid,
	    (uint64_t)__ip, (uint64_t)8, (uint64_t)&ip);
    if (status != 0)
	return UWX_TT_ERR_TTRACE;
    status = ttrace(TT_LWP_RUREGS, info->pid, info->lwpid,
	    (uint64_t)__r12, (uint64_t)8, (uint64_t)&sp);
    if (status != 0)
	return UWX_TT_ERR_TTRACE;
    status = ttrace(TT_LWP_RUREGS, info->pid, info->lwpid,
	    (uint64_t)__ar_bsp, (uint64_t)8, (uint64_t)&bsp);
    if (status != 0)
	return UWX_TT_ERR_TTRACE;
    status = ttrace(TT_LWP_RUREGS, info->pid, info->lwpid,
	    (uint64_t)__ar_bspstore, (uint64_t)8, (uint64_t)&info->bspstore);
    if (status != 0)
	return UWX_TT_ERR_TTRACE;
    status = ttrace(TT_LWP_RUREGS, info->pid, info->lwpid,
	    (uint64_t)__cfm, (uint64_t)8, (uint64_t)&cfm);
    if (status != 0)
	return UWX_TT_ERR_TTRACE;
    status = ttrace(TT_LWP_RUREGS, info->pid, info->lwpid,
	    (uint64_t)__ar_ec, (uint64_t)8, (uint64_t)&ec);
    if (status != 0)
	return UWX_TT_ERR_TTRACE;

    cfm |= ec << 52;

    if (reason != 0)
	bsp = uwx_add_to_bsp(bsp, -((unsigned int)cfm & 0x7f));

    return uwx_init_context(env, ip, sp, bsp, cfm);
}

int uwx_ttrace_init_from_sigcontext(
    struct uwx_env *env,
    struct uwx_ttrace_info *info,
    uint64_t ucontext)
{
    int status;
    uint16_t reason;
    uint64_t ip;
    uint64_t sp;
    uint64_t bsp;
    uint64_t cfm;
    unsigned int nat;
    uint64_t ec;

    info->have_ucontext = 1;
    uwx_ttrace_memcpy(&info->ucontext,
			ucontext,
			sizeof(__uc_misc_t),
			info->pid);
    uwx_ttrace_memcpy(&info->ucontext.__uc_mcontext,
			(uint64_t) &((ucontext_t *)ucontext)->__uc_mcontext,
			sizeof(mcontext_t),
			info->pid);
    status = __uc_get_reason(&info->ucontext, &reason);
    status = __uc_get_ip(&info->ucontext, &ip);
    status = __uc_get_grs(&info->ucontext, 12, 1, &sp, &nat);
    status = __uc_get_ar(&info->ucontext, 17, &bsp);
    status = __uc_get_ar(&info->ucontext, 18, &info->bspstore);
    status = __uc_get_ar(&info->ucontext, 66, &ec);
    status = __uc_get_cfm(&info->ucontext, &cfm);
    cfm |= ec << 52;
    if (reason != 0)
	bsp = uwx_add_to_bsp(bsp, -((unsigned int)cfm & 0x7f));
    uwx_init_context(env, ip, sp, bsp, cfm);
    return UWX_OK;
}

int uwx_ttrace_do_context_frame(
    struct uwx_env *env,
    struct uwx_ttrace_info *info)
{
    int abi_context;
    int status;
    uint64_t ucontext;

    abi_context = uwx_get_abi_context_code(env);
    if (abi_context != 0x0101)	/* abi = HP-UX, context = 1 */
	return UWX_TT_ERR_BADABICONTEXT;
    status = uwx_get_reg(env, UWX_REG_GR(32), &ucontext);
    if (status != 0)
	return status;
    return uwx_ttrace_init_from_sigcontext(env, info, ucontext);
}

int uwx_ttrace_copyin(
    int request,
    char *loc,
    uint64_t rem,
    int len,
    intptr_t tok)
{
    int status;
    int regid;
    unsigned int nat;
    struct uwx_ttrace_info *info = (struct uwx_ttrace_info *) tok;
    unsigned long *wp;
    uint64_t *dp;
    int ttreg;

    dp = (uint64_t *) loc;

    if (request == UWX_COPYIN_UINFO) {
	if (len == 4) {
	    status = ttrace(TT_PROC_RDTEXT, info->pid,
				0, rem, 4, (uint64_t)loc);
	    wp = (unsigned long *) loc;
	    TRACE_SELF_COPYIN4(rem, len, wp)
	}
	else if (len == 8) {
	    status = ttrace(TT_PROC_RDTEXT, info->pid,
				0, rem, 8, (uint64_t)loc);
	    TRACE_SELF_COPYIN4(rem, len, dp)
	}
	else
	    return 0;
    }
    else if (request == UWX_COPYIN_MSTACK && len == 8) {
	status = ttrace(TT_PROC_RDDATA, info->pid, 0, rem, 8, (uint64_t)loc);
	TRACE_SELF_COPYIN4(rem, len, dp)
    }
    else if (request == UWX_COPYIN_RSTACK && len == 8) {
	if (info->have_ucontext == 0 || rem < info->bspstore) {
	    status = ttrace(TT_PROC_RDDATA, info->pid, 0, rem, 8, (uint64_t)loc);
	    TRACE_SELF_COPYIN4(rem, len, dp)
	}
	else {
	    status = __uc_get_rsebs(&info->ucontext, (uint64_t *)rem, 1, dp);
	    if (status != 0)
		return 0;
	}
    }
    else if (request == UWX_COPYIN_REG && len == 8) {
	regid = (int)rem;
	if (info->have_ucontext) {
	    if (regid < UWX_REG_GR(0)) {
		switch (regid) {
		    case UWX_REG_PFS:
			status = __uc_get_ar(&info->ucontext, 64, dp);
			break;
		    case UWX_REG_PREDS:
			status = __uc_get_prs(&info->ucontext, dp);
			break;
		    case UWX_REG_RNAT:
			status = __uc_get_ar(&info->ucontext, 19, dp);
			break;
		    case UWX_REG_UNAT:
			status = __uc_get_ar(&info->ucontext, 36, dp);
			break;
		    case UWX_REG_FPSR:
			status = __uc_get_ar(&info->ucontext, 40, dp);
			break;
		    case UWX_REG_LC:
			status = __uc_get_ar(&info->ucontext, 65, dp);
			break;
		    default:
			return 0;
		}
	    }
	    else if (regid >= UWX_REG_GR(1) && regid <= UWX_REG_GR(31)) {
		status = __uc_get_grs(&info->ucontext,
				    regid - UWX_REG_GR(0), 1, dp, &nat);
	    }
	    else if (regid >= UWX_REG_BR(0) && regid <= UWX_REG_BR(7)) {
		status = __uc_get_brs(&info->ucontext,
				    regid - UWX_REG_BR(0), 1, dp);
	    }
	}
	else {
	    if (regid < UWX_REG_GR(0)) {
		switch (regid) {
		    case UWX_REG_PFS:
			ttreg = __ar_pfs;
			break;
		    case UWX_REG_PREDS:
			ttreg = __pr;
			break;
		    case UWX_REG_RNAT:
			ttreg = __ar_rnat;
			break;
		    case UWX_REG_UNAT:
			ttreg = __ar_unat;
			break;
		    case UWX_REG_FPSR:
			ttreg = __ar_fpsr;
			break;
		    case UWX_REG_LC:
			ttreg = __ar_lc;
			break;
		    default:
			return 0;
		}
	    }
	    else if (regid >= UWX_REG_GR(1) && regid <= UWX_REG_GR(31)) {
		ttreg = regid - UWX_REG_GR(1) + __r1;
	    }
	    else if (regid >= UWX_REG_BR(0) && regid <= UWX_REG_BR(7)) {
		ttreg = regid - UWX_REG_BR(0) + __b0;
	    }
	    else
		return 0;
	    status == ttrace(TT_LWP_RUREGS, info->pid, info->lwpid,
				ttreg, 8, (uint64_t)loc);
	}
	if (status != 0)
	    return 0;
    }
    return len;
}


int uwx_ttrace_lookupip(
    int request,
    uint64_t ip,
    intptr_t tok,
    uint64_t **resultp)
{
    struct uwx_ttrace_info *info = (struct uwx_ttrace_info *) tok;
    UINT64 handle;
    struct load_module_desc desc;
    uint64_t *unwind_base;
    uint64_t *rvec;
    int i;

    if (request == UWX_LKUP_LOOKUP) {
	TRACE_SELF_LOOKUP(ip)
	handle = dlmodinfo((unsigned long) ip, &desc, sizeof(desc),
				uwx_ttrace_memcpy, info->pid, info->load_map);
	if (handle == 0)
	    return UWX_LKUP_ERR;
	unwind_base = (uint64_t *) desc.unwind_base;
	TRACE_SELF_LOOKUP_DESC(desc.text_base, unwind_base)
	i = 0;
	rvec = info->rvec;
	rvec[i++] = UWX_KEY_TBASE;
	rvec[i++] = desc.text_base;
	rvec[i++] = UWX_KEY_UFLAGS;
	rvec[i++] = unwind_base[0];
	rvec[i++] = UWX_KEY_USTART;
	rvec[i++] = desc.text_base + unwind_base[1];
	rvec[i++] = UWX_KEY_UEND;
	rvec[i++] = desc.text_base + unwind_base[2];
	rvec[i++] = 0;
	rvec[i++] = 0;
	*resultp = rvec;
	return UWX_LKUP_UTABLE;
    }
    else if (request == UWX_LKUP_FREE) {
	return 0;
    }
}
