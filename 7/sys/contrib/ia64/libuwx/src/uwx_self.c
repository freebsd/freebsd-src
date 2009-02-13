/*
Copyright (c) 2003-2006 Hewlett-Packard Development Company, L.P.
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdlib.h>
#include <string.h>
#include <crt0.h>
#include <dlfcn.h>
#include <sys/uc_access.h>

#include "uwx_env.h"
#include "uwx_context.h"
#include "uwx_trace.h"
#include "uwx_self.h"
#include "uwx_self_info.h"

#define UWX_ABI_HPUX_SIGCONTEXT 0x0101	/* abi = HP-UX, context = 1 */

void uwx_free_load_module_cache(struct uwx_self_info *info);

int uwx_self_init_info_block(struct uwx_env *env, struct uwx_self_info *info)
{
    info->env = env;
    info->ucontext = 0;
    info->bspstore = 0;
    info->sendsig_start = __load_info->li_sendsig_txt;
    info->sendsig_end = __load_info->li_sendsig_txt +
				__load_info->li_sendsig_tsz;
    info->on_heap = 0;
    info->trace = env->trace;
    info->load_module_cache = NULL;

    return UWX_OK;
}

struct uwx_self_info *uwx_self_init_info(struct uwx_env *env)
{
    struct uwx_self_info *info;

    info = (struct uwx_self_info *)
			(*env->allocate_cb)(sizeof(struct uwx_self_info));
    if (info == 0)
	return 0;

    uwx_self_init_info_block(env, info);
    info->on_heap = 1;
    return info;
}

int uwx_self_free_info(struct uwx_self_info *info)
{
    int i;

    if (info->load_module_cache != NULL)
	uwx_free_load_module_cache(info);
    if (info->on_heap)
	(*info->env->free_cb)((void *)info);
    return UWX_OK;
}

int uwx_self_init_from_sigcontext(
    struct uwx_env *env,
    struct uwx_self_info *info,
    ucontext_t *ucontext)
{
    int status;
    uint16_t reason;
    uint64_t ip;
    uint64_t sp;
    uint64_t bsp;
    uint64_t cfm;
    unsigned int nat;
    uint64_t ec;
    int adj;

    info->ucontext = ucontext;
    status = __uc_get_reason(ucontext, &reason);
    if (status != 0)
	return UWX_ERR_UCACCESS;
    status = __uc_get_ip(ucontext, &ip);
    if (status != 0)
	return UWX_ERR_UCACCESS;
    status = __uc_get_grs(ucontext, 12, 1, &sp, &nat);
    if (status != 0)
	return UWX_ERR_UCACCESS;
    status = __uc_get_cfm(ucontext, &cfm);
    if (status != 0)
	return UWX_ERR_UCACCESS;
#ifdef NEW_UC_GET_AR
    status = __uc_get_ar_bsp(ucontext, &bsp);
    if (status != 0)
	return UWX_ERR_UCACCESS;
    status = __uc_get_ar_bspstore(ucontext, &info->bspstore);
    if (status != 0)
	return UWX_ERR_UCACCESS;
    status = __uc_get_ar_ec(ucontext, &ec);
    if (status != 0)
	return UWX_ERR_UCACCESS;
#else
    status = __uc_get_ar(ucontext, 17, &bsp);
    if (status != 0)
	return UWX_ERR_UCACCESS;
    status = __uc_get_ar(ucontext, 18, &info->bspstore);
    if (status != 0)
	return UWX_ERR_UCACCESS;
    status = __uc_get_ar(ucontext, 66, &ec);
    if (status != 0)
	return UWX_ERR_UCACCESS;
#endif
    /* The returned bsp needs to be adjusted. */
    /* For interrupt frames, where bsp was advanced by a cover */
    /* instruction, subtract sof (size of frame). For non-interrupt */
    /* frames, where bsp was advanced by br.call, subtract sol */
    /* (size of locals). */
    if (reason != 0)
	adj = (unsigned int)cfm & 0x7f;		/* interrupt frame */
    else
	adj = ((unsigned int)cfm >> 7) & 0x7f;	/* non-interrupt frame */
    bsp = uwx_add_to_bsp(bsp, -adj);
    cfm |= ec << 52;
    uwx_init_context(env, ip, sp, bsp, cfm);
    return UWX_OK;
}

int uwx_self_do_context_frame(
    struct uwx_env *env,
    struct uwx_self_info *info)
{
    int abi_context;
    int status;
    uint64_t ucontext;

    abi_context = uwx_get_abi_context_code(env);
    if (abi_context != UWX_ABI_HPUX_SIGCONTEXT)
	return UWX_SELF_ERR_BADABICONTEXT;
    status = uwx_get_reg(env, UWX_REG_GR(32), (uint64_t *)&ucontext);
    if (status != UWX_OK)
	return status;
    return uwx_self_init_from_sigcontext(env, info,
					(ucontext_t *)(intptr_t)ucontext);
}

int uwx_self_copyin(
    int request,
    char *loc,
    uint64_t rem,
    int len,
    intptr_t tok)
{
    int status;
    int regid;
    unsigned int nat;
    struct uwx_self_info *info = (struct uwx_self_info *) tok;
    unsigned long *wp;
    uint64_t *dp;

    status = -1;

    dp = (uint64_t *) loc;

    switch (request) {
	case UWX_COPYIN_UINFO:
	case UWX_COPYIN_MSTACK:
	    if (len == 4) {
		wp = (unsigned long *) loc;
		*wp = *(unsigned long *)(intptr_t)rem;
		TRACE_SELF_COPYIN4(rem, len, wp)
		status = 0;
	    }
	    else if (len == 8) {
		*dp = *(uint64_t *)(intptr_t)rem;
		TRACE_SELF_COPYIN8(rem, len, dp)
		status = 0;
	    }
	    break;
	case UWX_COPYIN_RSTACK:
	    if (len == 8) {
		if (info->ucontext == 0 && rem == (info->bspstore | 0x1f8)) {
		    *dp = info->env->context.special[UWX_REG_AR_RNAT];
		    status = 0;
		}
		else if (info->ucontext == 0 || rem < info->bspstore) {
		    *dp = *(uint64_t *)(intptr_t)rem;
		    TRACE_SELF_COPYIN8(rem, len, dp)
		    status = 0;
		}
		else {
		    status = __uc_get_rsebs(info->ucontext,
					    (uint64_t *)(intptr_t)rem, 1, dp);
		}
	    }
	    break;
	case UWX_COPYIN_REG:
	    regid = (int)rem;
	    if (info->ucontext != 0) {
		if (len == 8) {
		    if (rem == UWX_REG_PREDS)
			status = __uc_get_prs(info->ucontext, dp);
		    else if (rem == UWX_REG_AR_PFS)
			status = __uc_get_ar(info->ucontext, 64, dp);
		    else if (rem == UWX_REG_AR_RNAT)
			status = __uc_get_ar(info->ucontext, 19, dp);
		    else if (rem == UWX_REG_AR_UNAT)
			status = __uc_get_ar(info->ucontext, 36, dp);
		    else if (rem == UWX_REG_AR_FPSR)
			status = __uc_get_ar(info->ucontext, 40, dp);
		    else if (rem == UWX_REG_AR_LC)
			status = __uc_get_ar(info->ucontext, 65, dp);
		    else if (regid >= UWX_REG_GR(1) &&
						regid <= UWX_REG_GR(31))
			status = __uc_get_grs(info->ucontext,
					    regid - UWX_REG_GR(0), 1, dp, &nat);
		    else if (regid >= UWX_REG_BR(0) &&
						regid <= UWX_REG_BR(7))
			status = __uc_get_brs(info->ucontext,
					    regid - UWX_REG_BR(0), 1, dp);
		}
		else if (len == 16) {
		    if (regid >= UWX_REG_FR(2) && regid <= UWX_REG_FR(127)) {
			status = __uc_get_frs(info->ucontext,
				regid - UWX_REG_FR(0), 1, (fp_regval_t *)dp);
		    }
		}
	    }
	    break;
    }
    if (status != 0)
	return 0;
    return len;
}

#define MODULE_CACHE_SIZE 4

struct load_module_cache {
    int clock;
    char *names[MODULE_CACHE_SIZE];
    struct load_module_desc descs[MODULE_CACHE_SIZE];
    struct uwx_symbol_cache *symbol_cache;
};

void uwx_free_load_module_cache(struct uwx_self_info *info)
{
    int i;

    for (i = 0; i < MODULE_CACHE_SIZE; i++) {
	if (info->load_module_cache->names[i] != NULL)
	    (*info->env->free_cb)((void *)info->load_module_cache->names[i]);
    }

    if (info->load_module_cache->symbol_cache != NULL)
	uwx_release_symbol_cache(info->env,
				    info->load_module_cache->symbol_cache);

    (*info->env->free_cb)((void *)info->load_module_cache);
}

struct load_module_desc *uwx_get_modinfo(
    struct uwx_self_info *info,
    uint64_t ip,
    char **module_name_p)
{
    int i;
    UINT64 handle;
    struct load_module_cache *cache;
    struct load_module_desc *desc;
    char *module_name;

    cache = info->load_module_cache;
    if (cache == NULL) {
	cache = (struct load_module_cache *)
		(*info->env->allocate_cb)(sizeof(struct load_module_cache));
	if (cache == NULL)
	    return NULL;
	for (i = 0; i < MODULE_CACHE_SIZE; i++) {
	    desc = &cache->descs[i];
	    desc->text_base = 0;
	    desc->text_size = 0;
	    cache->names[i] = NULL;
	}
	cache->clock = 0;
	cache->symbol_cache = NULL;
	info->load_module_cache = cache;
    }
    for (i = 0; i < MODULE_CACHE_SIZE; i++) {
	desc = &cache->descs[i];
	if (ip >= desc->text_base && ip < desc->text_base + desc->text_size)
	    break;
    }
    if (i >= MODULE_CACHE_SIZE) {
	i = cache->clock;
	cache->clock = (cache->clock + 1) % MODULE_CACHE_SIZE;
	desc = &cache->descs[i];
	handle = dlmodinfo(ip, desc, sizeof(*desc), 0, 0, 0);
	if (handle == 0)
	    return NULL;
	if (cache->names[i] != NULL)
	    (*info->env->free_cb)(cache->names[i]);
	cache->names[i] = NULL;
    }
    if (module_name_p != NULL) {
	if (cache->names[i] == NULL) {
	    module_name = dlgetname(desc, sizeof(*desc), 0, 0, 0);
	    if (module_name != NULL) {
		cache->names[i] = (char *)
			    (*info->env->allocate_cb)(strlen(module_name)+1);
		if (cache->names[i] != NULL)
		    strcpy(cache->names[i], module_name);
	    }
	}
	*module_name_p = cache->names[i];
    }
    return desc;
}

int uwx_self_lookupip(
    int request,
    uint64_t ip,
    intptr_t tok,
    uint64_t **resultp)
{
    struct uwx_self_info *info = (struct uwx_self_info *) tok;
    UINT64 handle;
    struct load_module_desc *desc;
    uint64_t *unwind_base;
    uint64_t *rvec;
    char *module_name;
    char *func_name;
    uint64_t offset;
    int i;
    int status;

    if (request == UWX_LKUP_LOOKUP) {
	TRACE_SELF_LOOKUP(ip)
	if (ip >= info->sendsig_start && ip < info->sendsig_end) {
	    i = 0;
	    rvec = info->rvec;
	    rvec[i++] = UWX_KEY_CONTEXT;
	    rvec[i++] = UWX_ABI_HPUX_SIGCONTEXT;
	    rvec[i++] = UWX_KEY_END;
	    rvec[i++] = 0;
	    *resultp = rvec;
	    return UWX_LKUP_FDESC;
	}
	else {
	    desc = uwx_get_modinfo(info, ip, NULL);
	    if (desc == NULL)
		return UWX_LKUP_ERR;
	    unwind_base = (uint64_t *) (intptr_t) desc->unwind_base;
	    TRACE_SELF_LOOKUP_DESC(desc->text_base,
					desc->linkage_ptr, unwind_base)
	    i = 0;
	    rvec = info->rvec;
	    rvec[i++] = UWX_KEY_TBASE;
	    rvec[i++] = desc->text_base;
	    rvec[i++] = UWX_KEY_UFLAGS;
	    rvec[i++] = unwind_base[0];
	    rvec[i++] = UWX_KEY_USTART;
	    rvec[i++] = desc->text_base + unwind_base[1];
	    rvec[i++] = UWX_KEY_UEND;
	    rvec[i++] = desc->text_base + unwind_base[2];
	    rvec[i++] = UWX_KEY_GP;
	    rvec[i++] = desc->linkage_ptr;
	    rvec[i++] = UWX_KEY_END;
	    rvec[i++] = 0;
	    *resultp = rvec;
	    return UWX_LKUP_UTABLE;
	}
    }
    else if (request == UWX_LKUP_FREE) {
	return 0;
    }
    else if (request == UWX_LKUP_MODULE) {
	desc = uwx_get_modinfo(info, ip, &module_name);
	if (desc == NULL)
	    return UWX_LKUP_ERR;
	if (module_name == NULL)
	    return UWX_LKUP_ERR;
	i = 0;
	rvec = info->rvec;
	rvec[i++] = UWX_KEY_MODULE;
	rvec[i++] = (uint64_t)(intptr_t)module_name;
	rvec[i++] = UWX_KEY_TBASE;
	rvec[i++] = desc->text_base;
	rvec[i++] = UWX_KEY_END;
	rvec[i++] = 0;
	*resultp = rvec;
	return UWX_LKUP_SYMINFO;
    }
    else if (request == UWX_LKUP_SYMBOLS) {
	rvec = *resultp;
	for (i = 0; rvec[i] != UWX_KEY_END; i += 2) {
	    if (rvec[i] == UWX_KEY_FUNCSTART)
		ip = rvec[i+1];
	}
	desc = uwx_get_modinfo(info, ip, &module_name);
	if (desc == NULL)
	    return UWX_LKUP_ERR;
	if (module_name == NULL)
	    return UWX_LKUP_ERR;
	status = uwx_find_symbol(info->env,
			&info->load_module_cache->symbol_cache,
			module_name, ip - desc->text_base,
			&func_name, &offset);
	i = 0;
	rvec = info->rvec;
	rvec[i++] = UWX_KEY_MODULE;
	rvec[i++] = (uint64_t)(intptr_t)module_name;
	rvec[i++] = UWX_KEY_TBASE;
	rvec[i++] = desc->text_base;
	if (status == UWX_OK) {
	    rvec[i++] = UWX_KEY_FUNC;
	    rvec[i++] = (uint64_t)(intptr_t)func_name;
	    rvec[i++] = UWX_KEY_FUNCSTART;
	    rvec[i++] = ip - offset;
	}
	rvec[i++] = UWX_KEY_END;
	rvec[i++] = 0;
	*resultp = rvec;
	return UWX_LKUP_SYMINFO;
    }
    return UWX_LKUP_ERR;
}
