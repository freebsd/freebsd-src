/*
Copyright (c) 2003 Hewlett-Packard Development Company, L.P.
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

#include "uwx_env.h"
#include "uwx_context.h"
#include "uwx_utable.h"
#include "uwx_uinfo.h"
#include "uwx_scoreboard.h"
#include "uwx_str.h"
#include "uwx_step.h"
#include "uwx_trace.h"

/*
 *  uwx_step.c
 *
 *  This file contains the routines for stepping from one frame
 *  into its callers frame. The context for the current frame
 *  is maintained inside the current unwind environment
 *  (struct uwx_env), and is updated with each call to
 *  uwx_step() to refer to the previous frame.
 */


/* Forward Declarations */

int uwx_decode_uvec(struct uwx_env *env, uint64_t *uvec, uint64_t **rstate);
int uwx_restore_reg(struct uwx_env *env, uint64_t rstate,
					uint64_t *valp, uint64_t *histp);
int uwx_restore_freg(struct uwx_env *env, uint64_t rstate,
					uint64_t *valp, uint64_t *histp);
int uwx_restore_nat(struct uwx_env *env, uint64_t rstate, int unat);


/* uwx_lookupip_hook: Hook routine so dynamic instrumentation */
/*      tools can intercept Lookup IP events. When not */
/*      intercepted, it just returns "Not found", so that */
/*      the callback routine is invoked. */

/*ARGSUSED*/
int uwx_lookupip_hook(int request, uint64_t ip, intptr_t tok, uint64_t **vecp,
			size_t uvecsize)
{
    return UWX_LKUP_NOTFOUND;
}


/* uwx_get_frame_info: Gets unwind info for current frame */
static
int uwx_get_frame_info(struct uwx_env *env)
{
    int i;
    int status;
    int cbstatus;
    int cbcalled = 0;
    uint64_t ip;
    uint64_t *uvec;
    uint64_t *rstate;
    struct uwx_utable_entry uentry;
    uint64_t uvecout[UVECSIZE];

    if (env->copyin == 0 || env->lookupip == 0)
	return UWX_ERR_NOCALLBACKS;

    env->function_offset = -1LL;
    env->function_name = 0;
    env->module_name = 0;
    uwx_reset_str_pool(env);

    /* Use the lookup IP callback routine to find out about the */
    /* current IP. If the predicate registers are valid, pass them */
    /* in the uvec. */

    /* When self-unwinding, we call a hook routine before the */
    /* callback. If the application is running under control of */
    /* a dynamic instrumentation tool, that tool will have an */
    /* opportunity to intercept lookup IP requests. */

    i = 0;
    uvecout[i++] = UWX_KEY_VERSION;
    uvecout[i++] = UWX_VERSION;
    if (env->context.valid_regs & (1 << UWX_REG_PREDS)) {
	uvecout[i++] = UWX_KEY_PREDS;
	uvecout[i++] = env->context.special[UWX_REG_PREDS];
    }
    uvecout[i++] = UWX_KEY_END;
    uvecout[i++] = 0;
    uvec = uvecout;
    cbstatus = UWX_LKUP_NOTFOUND;
    ip = env->context.special[UWX_REG_IP];
    env->remapped_ip = ip;

    /* Call the hook routine. */

    if (env->remote == 0)
	cbstatus = uwx_lookupip_hook(UWX_LKUP_LOOKUP, ip, env->cb_token, &uvec,
		sizeof(uvecout));

    /* If the hook routine remapped the IP, use the new IP for */
    /* the callback instead of the original IP. */

    if (cbstatus == UWX_LKUP_REMAP) {
	for (i = 0; uvec[i] != UWX_KEY_END; i += 2) {
	    switch ((int)uvec[i]) {
		case UWX_KEY_NEWIP:
		    ip = uvec[i+1];
		    break;
	    }
	}
	env->remapped_ip = ip;
    }

    /* Now call the callback routine unless the hook routine gave */
    /* us all the info. */

    if (cbstatus == UWX_LKUP_NOTFOUND || cbstatus == UWX_LKUP_REMAP) {
	cbcalled = 1;
	cbstatus = (*env->lookupip)(UWX_LKUP_LOOKUP, ip, env->cb_token, &uvec);
    }

    /* If the callback routine remapped the IP, call it one more time */
    /* with the new IP. */

    if (cbstatus == UWX_LKUP_REMAP) {
	for (i = 0; uvec[i] != UWX_KEY_END; i += 2) {
	    switch ((int)uvec[i]) {
		case UWX_KEY_NEWIP:
		    ip = uvec[i+1];
		    break;
	    }
	}
	env->remapped_ip = ip;
	cbstatus = (*env->lookupip)(UWX_LKUP_LOOKUP, ip, env->cb_token, &uvec);
    }

    /* If NOTFOUND, there's nothing we can do but return an error. */

    if (cbstatus == UWX_LKUP_NOTFOUND) {
	status = UWX_ERR_IPNOTFOUND;
    }

    /* If the callback returns an unwind table, we need to */
    /* search the table for an unwind entry that describes the */
    /* code region of interest, then decode the unwind information */
    /* associated with that unwind table entry, and store the */
    /* resulting register state array in the unwind environment */
    /* block. */

    else if (cbstatus == UWX_LKUP_UTABLE) {
	status = uwx_search_utable(env, ip, uvec, &uentry);
	if (cbcalled)
	    (void) (*env->lookupip)(UWX_LKUP_FREE, 0, env->cb_token, &uvec);
	if (status == UWX_OK)
	    status = uwx_decode_uinfo(env, &uentry, &rstate);
	else if (status == UWX_ERR_NOUENTRY)
	    status = uwx_default_rstate(env, &rstate);
	if (status == UWX_OK)
	    env->rstate = rstate;
    }

    /* If the callback returns an unwind info block, we can */
    /* proceed directly to decoding the unwind information. */

    else if (cbstatus == UWX_LKUP_UINFO) {
	uentry.code_start = 0;
	uentry.code_end = 0;
	uentry.unwind_info = 0;
	uentry.unwind_flags = 0;
	for (i = 0; uvec[i] != UWX_KEY_END; i += 2) {
	    switch ((int)uvec[i]) {
		case UWX_KEY_UFLAGS:
		    uentry.unwind_flags = uvec[i+1];
		    break;
		case UWX_KEY_UINFO:
		    uentry.unwind_info = uvec[i+1];
		    break;
		case UWX_KEY_MODULE:
		    env->module_name =
				uwx_alloc_str(env, (char *)(uvec[i+1]));
		    break;
		case UWX_KEY_FUNC:
		    env->function_name =
				uwx_alloc_str(env, (char *)(uvec[i+1]));
		    break;
		case UWX_KEY_FUNCSTART:
		    uentry.code_start = uvec[i+1];
		    break;
	    }
	}
	if (cbcalled)
	    (void) (*env->lookupip)(UWX_LKUP_FREE, 0, env->cb_token, &uvec);
	status = uwx_decode_uinfo(env, &uentry, &rstate);
	if (status == UWX_OK)
	    env->rstate = rstate;
    }

    /* If the callback returns a frame description (in the form */
    /* of an update vector), convert the update vector into a */
    /* register state array, then invoke the callback again to */
    /* let it free any memory it allocated. */

    else if (cbstatus == UWX_LKUP_FDESC) {
	status = uwx_decode_uvec(env, uvec, &rstate);
	if (cbcalled)
	    (void) (*env->lookupip)(UWX_LKUP_FREE, 0, env->cb_token, &uvec);
	if (status == UWX_OK)
	    env->rstate = rstate;
    }

    /* Any other return from the callback is an error. */

    else {
	status = UWX_ERR_LOOKUPERR;
    }
    return status;
}


/* uwx_restore_markers: Restores the stack markers -- PSP, RP, PFS */

int uwx_restore_markers(struct uwx_env *env)
{
    int status;
    uint64_t val;
    uint64_t hist;

    if ((env->context.valid_regs & VALID_BASIC4) != VALID_BASIC4)
	return UWX_ERR_NOCONTEXT;

    /* If we haven't already obtained the frame info for the */
    /* current frame, get it now. */

    if (env->rstate == 0) {
	status = uwx_get_frame_info(env);
	if (status != UWX_OK)
	    return status;
    }

    TRACE_S_STEP(env->rstate)

    if (env->rstate[SBREG_PSP] != UWX_DISP_NONE) {
	status = uwx_restore_reg(env, env->rstate[SBREG_PSP], &val, &hist);
	if (status != UWX_OK)
	    return status;
	env->context.special[UWX_REG_PSP] = val;
	env->history.special[UWX_REG_PSP] = hist;
	env->context.valid_regs |= 1 << UWX_REG_PSP;
	TRACE_S_RESTORE_REG("PSP", env->rstate[SBREG_PSP], val)
    }

    if (env->rstate[SBREG_RP] != UWX_DISP_NONE) {
	status = uwx_restore_reg(env, env->rstate[SBREG_RP], &val, &hist);
	if (status != UWX_OK)
	    return status;
	env->context.special[UWX_REG_RP] = val;
	env->history.special[UWX_REG_RP] = hist;
	env->context.valid_regs |= 1 << UWX_REG_RP;
	TRACE_S_RESTORE_REG("RP", env->rstate[SBREG_RP], val)
    }

    if (env->rstate[SBREG_PFS] != UWX_DISP_NONE) {
	status = uwx_restore_reg(env, env->rstate[SBREG_PFS], &val, &hist);
	if (status != UWX_OK)
	    return status;
	env->context.special[UWX_REG_PFS] = val;
	env->history.special[UWX_REG_PFS] = hist;
	env->context.valid_regs |= 1 << UWX_REG_PFS;
	TRACE_S_RESTORE_REG("PFS", env->rstate[SBREG_PFS], val)
    }

    return UWX_OK;
}

/* uwx_get_sym_info: Gets symbolic info from current frame */

int uwx_get_sym_info(
    struct uwx_env *env,
    char **modp,
    char **symp,
    uint64_t *offsetp)
{
    int status;
    int cbstatus;
    uint64_t ip;
    uint64_t *uvec;
    uint64_t uvecout[2];
    int i;

    if (env == 0)
	return UWX_ERR_NOENV;

    /* If we haven't already obtained the frame info for the */
    /* current frame, get it now. */

    if (env->rstate == 0) {
	status = uwx_get_frame_info(env);
	if (status != UWX_OK)
	    return status;
    }

    /* Get the symbolic information from the lookup IP callback. */
    if (env->function_name == 0) {
	ip = env->remapped_ip;
	i = 0;
	if (env->function_offset >= 0) {
	    uvecout[i++] = UWX_KEY_FUNCSTART;
	    uvecout[i++] = ip - env->function_offset;
	}
	uvecout[i++] = UWX_KEY_END;
	uvecout[i++] = 0;
	uvec = uvecout;
	cbstatus = (*env->lookupip)(UWX_LKUP_SYMBOLS, ip, env->cb_token, &uvec);

	if (cbstatus == UWX_LKUP_SYMINFO) {
	    for (i = 0; uvec[i] != UWX_KEY_END; i += 2) {
		switch ((int)uvec[i]) {
		    case UWX_KEY_MODULE:
			env->module_name =
				uwx_alloc_str(env, (char *)(uvec[i+1]));
			break;
		    case UWX_KEY_FUNC:
			env->function_name =
				uwx_alloc_str(env, (char *)(uvec[i+1]));
			break;
		    case UWX_KEY_FUNCSTART:
			env->function_offset = ip - uvec[i+1];
			break;
		}
	    }
	    (void) (*env->lookupip)(UWX_LKUP_FREE, 0, env->cb_token, &uvec);
	}
    }

    *modp = env->module_name;
    *symp = env->function_name;
    *offsetp = env->function_offset;

    return UWX_OK;
}


/* uwx_step: Steps from the current frame to the previous frame */

int uwx_step(struct uwx_env *env)
{
    int i;
    int status;
    int pfs_sol;
    int dispcode;
    uint64_t val;
    uint64_t fval[2];
    uint64_t hist;
    uint64_t tempgr[NPRESERVEDGR];
    int needpriunat;
    int unat;
    int tempnat;

    if (env == 0)
	return UWX_ERR_NOENV;

    /* Complete the current context by restoring the current values */
    /* of psp, rp, and pfs. */

    if (env->rstate == 0 ||
	    (env->context.valid_regs & VALID_MARKERS) != VALID_MARKERS) {
	status = uwx_restore_markers(env);
	if (status != UWX_OK)
	    return status;
    }

    /* Check for bottom of stack (rp == 0). */

    if (env->context.special[UWX_REG_RP] == 0)
	return UWX_BOTTOM;

    /* Find where the primary unat is saved, get a copy. */
    /* Then, as we restore the GRs, we'll merge the NaT bits into the */
    /* priunat register in the context. */
    /* (Make sure we need it, though, before we try to get it, */
    /* because the attempt to get it might invoke the copy-in callback. */
    /* We don't need the priunat unless one of GR 4-7 was */
    /* saved to the memory stack.) */

    needpriunat = 0;
    for (i = 0; i < NSB_GR; i++) {
	dispcode = UWX_GET_DISP_CODE(env->rstate[SBREG_GR + i]);
	if (dispcode == UWX_DISP_SPREL(0) || dispcode == UWX_DISP_PSPREL(0))
	    needpriunat = 1;
    }
    unat = 0;
    if (needpriunat && env->rstate[SBREG_PRIUNAT] != UWX_DISP_NONE) {
	status = uwx_restore_reg(env, env->rstate[SBREG_PRIUNAT], &val, &hist);
	if (status != UWX_OK)
	    return status;
	unat = (int) val;
	env->history.special[UWX_REG_PRIUNAT] = hist;
	TRACE_S_RESTORE_REG("PRIUNAT", env->rstate[SBREG_PRIUNAT], val)
    }

    /* Retrieve saved values of the preserved GRs into temporaries. */

    tempnat = (int) env->context.special[UWX_REG_PRIUNAT];
    for (i = 0; i < NSB_GR; i++) {
	if (env->rstate[SBREG_GR + i] != UWX_DISP_NONE) {
	    status = uwx_restore_reg(env,
			env->rstate[SBREG_GR + i], &val, &hist);
	    if (status != UWX_OK)
		return status;
	    tempgr[i] = val;
	    if (uwx_restore_nat(env, env->rstate[SBREG_GR + i], unat))
		tempnat |= 1 << i;
	    else
		tempnat &= ~(1 << i);
	    env->history.gr[i] = hist;
	    env->context.valid_regs |= 1 << (i + VALID_GR_SHIFT);
	    TRACE_S_RESTORE_GR(i, env->rstate[SBREG_GR + i], val)
	}
    }

    /* Now we have everything we need to step back to the previous frame. */

    /* Restore preserved BRs. */

    for (i = 0; i < NSB_BR; i++) {
	if (env->rstate[SBREG_BR + i] != UWX_DISP_NONE) {
	    status = uwx_restore_reg(env,
			env->rstate[SBREG_BR + i], &val, &hist);
	    if (status != UWX_OK)
		return status;
	    env->context.br[i] = val;
	    env->history.br[i] = hist;
	    env->context.valid_regs |= 1 << (i + VALID_BR_SHIFT);
	    TRACE_S_RESTORE_BR(i, env->rstate[SBREG_BR + i], val)
	}
    }

    /* Restore preserved FRs. */

    if (env->nsbreg == NSBREG) {
	for (i = 0; i < NSB_FR; i++) {
	    if (env->rstate[SBREG_FR + i] != UWX_DISP_NONE) {
		status = uwx_restore_freg(env,
			    env->rstate[SBREG_FR + i], fval, &hist);
		if (status != UWX_OK)
		    return status;
		env->context.fr[i].part0 = fval[0];
		env->context.fr[i].part1 = fval[1];
		env->history.fr[i] = hist;
		env->context.valid_frs |= 1 << i;
		TRACE_S_RESTORE_FR(i, env->rstate[SBREG_FR + i], fval)
	    }
	}
    }

    /* Restore other preserved regs. */

    if (env->rstate[SBREG_PREDS] != UWX_DISP_NONE) {
	status = uwx_restore_reg(env, env->rstate[SBREG_PREDS], &val, &hist);
	if (status != UWX_OK)
	    return status;
	env->context.special[UWX_REG_PREDS] = val;
	env->history.special[UWX_REG_PREDS] = hist;
	env->context.valid_regs |= 1 << UWX_REG_PREDS;
	TRACE_S_RESTORE_REG("PREDS", env->rstate[SBREG_PREDS], val)
    }
    if (env->rstate[SBREG_RNAT] != UWX_DISP_NONE) {
	status = uwx_restore_reg(env, env->rstate[SBREG_RNAT], &val, &hist);
	if (status != UWX_OK)
	    return status;
	env->context.special[UWX_REG_AR_RNAT] = val;
	env->history.special[UWX_REG_AR_RNAT] = hist;
	env->context.valid_regs |= 1 << UWX_REG_AR_RNAT;
	TRACE_S_RESTORE_REG("RNAT", env->rstate[SBREG_RNAT], val)
    }
    if (env->rstate[SBREG_UNAT] != UWX_DISP_NONE) {
	status = uwx_restore_reg(env, env->rstate[SBREG_UNAT], &val, &hist);
	if (status != UWX_OK)
	    return status;
	env->context.special[UWX_REG_AR_UNAT] = val;
	env->history.special[UWX_REG_AR_UNAT] = hist;
	env->context.valid_regs |= 1 << UWX_REG_AR_UNAT;
	TRACE_S_RESTORE_REG("UNAT", env->rstate[SBREG_UNAT], val)
    }
    if (env->rstate[SBREG_FPSR] != UWX_DISP_NONE) {
	status = uwx_restore_reg(env, env->rstate[SBREG_FPSR], &val, &hist);
	if (status != UWX_OK)
	    return status;
	env->context.special[UWX_REG_AR_FPSR] = val;
	env->history.special[UWX_REG_AR_FPSR] = hist;
	env->context.valid_regs |= 1 << UWX_REG_AR_FPSR;
	TRACE_S_RESTORE_REG("FPSR", env->rstate[SBREG_FPSR], val)
    }
    if (env->rstate[SBREG_LC] != UWX_DISP_NONE) {
	status = uwx_restore_reg(env, env->rstate[SBREG_LC], &val, &hist);
	if (status != UWX_OK)
	    return status;
	env->context.special[UWX_REG_AR_LC] = val;
	env->history.special[UWX_REG_AR_LC] = hist;
	env->context.valid_regs |= 1 << UWX_REG_AR_LC;
	TRACE_S_RESTORE_REG("LC", env->rstate[SBREG_LC], val)
    }

    /* Restore preserved GRs from temporaries. */

    for (i = 0; i < NSB_GR; i++) {
	if (env->rstate[SBREG_GR + i] != UWX_DISP_NONE)
	    env->context.gr[i] = tempgr[i];
    }
    env->context.special[UWX_REG_PRIUNAT] = tempnat;

    /* Restore the frame markers. */

    env->context.special[UWX_REG_IP] = env->context.special[UWX_REG_RP];
    env->history.special[UWX_REG_IP] = env->history.special[UWX_REG_RP];

    env->context.special[UWX_REG_SP] = env->context.special[UWX_REG_PSP];
    env->history.special[UWX_REG_SP] = env->history.special[UWX_REG_PSP];

    pfs_sol = ((unsigned int)env->context.special[UWX_REG_PFS] >> 7) & 0x7f;
    env->context.special[UWX_REG_BSP] = uwx_add_to_bsp(
				env->context.special[UWX_REG_BSP],
				-pfs_sol);

    env->context.special[UWX_REG_CFM] = env->context.special[UWX_REG_PFS];
    env->history.special[UWX_REG_CFM] = env->history.special[UWX_REG_PFS];

    env->context.special[UWX_REG_RP] = 0;

    /* The frame info for the new frame isn't yet available. */

    env->rstate = 0;
    env->context.valid_regs &= ~VALID_MARKERS;

    return UWX_OK;
}


/* uwx_decode_uvec: Converts the update vector into a register state array */

int uwx_decode_uvec(struct uwx_env *env, uint64_t *uvec, uint64_t **rstate)
{
    int i;
    int status;

    status = uwx_default_rstate(env, rstate);
    if (status != UWX_OK)
	return status;

    for (i = 0; uvec[i] != UWX_KEY_END; i += 2) {
	switch ((int)uvec[i]) {
	    case UWX_KEY_CONTEXT:
		env->abi_context = (int)(uvec[i+1]);
		status = UWX_ABI_FRAME;
		break;
	    case UWX_KEY_MODULE:
		env->module_name =
				uwx_alloc_str(env, (char *)(uvec[i+1]));
		break;
	    case UWX_KEY_FUNC:
		env->function_name =
				uwx_alloc_str(env, (char *)(uvec[i+1]));
		break;
	    case UWX_KEY_FUNCSTART:
		env->function_offset = env->remapped_ip - uvec[i+1];
		break;
	    default:
		return UWX_ERR_CANTUNWIND;
	}
    }
    return status;
}


/* uwx_restore_reg: Restores a register according to the scoreboard */

#define COPYIN_MSTACK_8(dest, src) \
    (env->remote? \
	(*env->copyin)(UWX_COPYIN_MSTACK, (dest), (src), \
						DWORDSZ, env->cb_token) : \
	(*(uint64_t *)(dest) = *(uint64_t *)(src), DWORDSZ) )

int uwx_restore_reg(struct uwx_env *env, uint64_t rstate,
				uint64_t *valp, uint64_t *histp)
{
    int status;
    uint64_t p;
    int n;
    int regid;

    status = UWX_OK;

    switch (UWX_GET_DISP_CODE(rstate)) {
	case UWX_DISP_SPPLUS(0):
	    *valp = env->context.special[UWX_REG_SP] +
				UWX_GET_DISP_OFFSET(rstate);
	    *histp = UWX_DISP_NONE;
	    break;
	case UWX_DISP_SPREL(0):
	    p = env->context.special[UWX_REG_SP] +
				UWX_GET_DISP_OFFSET(rstate);
	    n = COPYIN_MSTACK_8((char *)valp, p);
	    if (n != DWORDSZ)
		status = UWX_ERR_COPYIN_MSTK;
	    *histp = UWX_DISP_MSTK(p);
	    break;
	case UWX_DISP_PSPREL(0):
	    p = env->context.special[UWX_REG_PSP] + 16 -
				UWX_GET_DISP_OFFSET(rstate);
	    n = COPYIN_MSTACK_8((char *)valp, p);
	    if (n != DWORDSZ)
		status = UWX_ERR_COPYIN_MSTK;
	    *histp = UWX_DISP_MSTK(p);
	    break;
	case UWX_DISP_REG(0):
	    regid = UWX_GET_DISP_REGID(rstate);
	    status = uwx_get_reg(env, regid, valp);
	    (void) uwx_get_spill_loc(env, regid, histp);
	    break;
    }
    return status;
}

#define COPYIN_MSTACK_16(dest, src) \
    (env->remote? \
	(*env->copyin)(UWX_COPYIN_MSTACK, (dest), (src), \
						2*DWORDSZ, env->cb_token) : \
	(*(uint64_t *)(dest) = *(uint64_t *)(src), \
		*(uint64_t *)((dest)+8) = *(uint64_t *)((src)+8), \
		2*DWORDSZ) )

int uwx_restore_freg(struct uwx_env *env, uint64_t rstate,
				uint64_t *valp, uint64_t *histp)
{
    int status;
    uint64_t p;
    int n;
    int regid;

    status = UWX_OK;

    switch (UWX_GET_DISP_CODE(rstate)) {
	case UWX_DISP_SPREL(0):
	    p = env->context.special[UWX_REG_SP] +
				UWX_GET_DISP_OFFSET(rstate);
	    n = COPYIN_MSTACK_16((char *)valp, p);
	    if (n != 2*DWORDSZ)
		status = UWX_ERR_COPYIN_MSTK;
	    *histp = UWX_DISP_MSTK(p);
	    break;
	case UWX_DISP_PSPREL(0):
	    p = env->context.special[UWX_REG_PSP] + 16 -
				UWX_GET_DISP_OFFSET(rstate);
	    n = COPYIN_MSTACK_16((char *)valp, p);
	    if (n != 2*DWORDSZ)
		status = UWX_ERR_COPYIN_MSTK;
	    *histp = UWX_DISP_MSTK(p);
	    break;
	case UWX_DISP_REG(0):
	    regid = UWX_GET_DISP_REGID(rstate);
	    status = uwx_get_reg(env, regid, valp);
	    (void) uwx_get_spill_loc(env, regid, histp);
	    break;
    }
    return status;
}

/* uwx_restore_nat: Returns the saved NaT bit for a preserved GR */

int uwx_restore_nat(struct uwx_env *env, uint64_t rstate, int unat)
{
    int nat;
    uint64_t p;

    nat = 0;
    switch (UWX_GET_DISP_CODE(rstate)) {
	case UWX_DISP_SPREL(0):
	    p = env->context.special[UWX_REG_SP] +
				UWX_GET_DISP_OFFSET(rstate);
	    nat = (unat >> (((int)p >> 3) & 0x3f)) & 0x01;
	    break;
	case UWX_DISP_PSPREL(0):
	    p = env->context.special[UWX_REG_PSP] + 16 -
				UWX_GET_DISP_OFFSET(rstate);
	    nat = (unat >> (((int)p >> 3) & 0x3f)) & 0x01;
	    break;
	case UWX_DISP_REG(0):
	    (void) uwx_get_nat(env, UWX_GET_DISP_REGID(rstate), &nat);
	    break;
    }
    return nat;
}

