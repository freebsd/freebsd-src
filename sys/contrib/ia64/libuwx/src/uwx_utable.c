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

#include "uwx_env.h"
#include "uwx_utable.h"
#include "uwx_swap.h"
#include "uwx_trace.h"

/*
 *  uwx_utable.c
 *
 *  This file contains the routines for searching an unwind table.
 *  The main entry point, uwx_search_utable(), gets the
 *  necessary information from the lookup ip callback's result
 *  vector, determines whether the table is 32-bit or 64-bit,
 *  then invokes the binary search routine for that format.
 */


/* Forward declarations */

int uwx_search_utable32(
    struct uwx_env *env,
    uint32_t text_base,
    uint32_t unwind_start,
    uint32_t unwind_end,
    struct uwx_utable_entry *uentry);

int uwx_search_utable64(
    struct uwx_env *env,
    uint64_t text_base,
    uint64_t unwind_start,
    uint64_t unwind_end,
    struct uwx_utable_entry *uentry);


/* uwx_search_utable: Searches an unwind table for IP in current context */

int uwx_search_utable(
    struct uwx_env *env,
    uint64_t *uvec,
    struct uwx_utable_entry *uentry)
{
    uint64_t text_base = 0;
    uint64_t unwind_flags;
    uint64_t unwind_start = 0;
    uint64_t unwind_end = 0;
    int keys;
    int status;

    /* Get unwind table information from the result vector. */
    /* Make sure all three required values are given. */

    keys = 0;
    unwind_flags = 0;
    while (*uvec != 0) {
	switch ((int)*uvec++) {
	    case UWX_KEY_TBASE:
		keys |= 1;
		text_base = *uvec++;
		break;
	    case UWX_KEY_UFLAGS:
		unwind_flags = *uvec++;
		break;
	    case UWX_KEY_USTART:
		keys |= 2;
		unwind_start = *uvec++;
		break;
	    case UWX_KEY_UEND:
		keys |= 4;
		unwind_end = *uvec++;
		break;
	    default:
		return UWX_ERR_BADKEY;
	}
    }
    if (keys != 7)
	return UWX_ERR_BADKEY;

    /* Copy the unwind flags into the unwind entry. */
    /* (uwx_decode_uinfo needs to know whether it's 32-bit or 64-bit.) */

    uentry->unwind_flags = unwind_flags;

    /* Call the appropriate binary search routine. */

    if (unwind_flags & UNWIND_TBL_32BIT)
	status = uwx_search_utable32(env,
			(uint32_t) text_base,
			(uint32_t) unwind_start,
			(uint32_t) unwind_end,
			uentry);
    else
	status = uwx_search_utable64(env,
			text_base, unwind_start, unwind_end, uentry);

    return status;
}


/* uwx_search_utable32: Binary search of 32-bit unwind table */

#define COPYIN_UINFO_4(dest, src) \
    (env->remote? \
	(*env->copyin)(UWX_COPYIN_UINFO, (dest), (src), \
						WORDSZ, env->cb_token) : \
	(*(uint32_t *)(dest) = *(uint32_t *)(src), WORDSZ) )

int uwx_search_utable32(
    struct uwx_env *env,
    uint32_t text_base,
    uint32_t unwind_start,
    uint32_t unwind_end,
    struct uwx_utable_entry *uentry)
{
    int lb;
    int ub;
    int mid = 0;
    int len;
    uint32_t ip;
    uint32_t code_start;
    uint32_t code_end;
    uint32_t unwind_info;

    /* Since the unwind table uses segment-relative offsets, convert */
    /* the IP in the current context to a segment-relative offset. */

    ip = env->context.special[UWX_REG_IP] - text_base;

    TRACE_T_SEARCH32(ip)

    /* Standard binary search. */
    /* Might modify this to do interpolation in the future. */

    lb = 0;
    ub = (unwind_end - unwind_start) / (3 * WORDSZ);
    while (ub > lb) {
	mid = (lb + ub) / 2;
	len = COPYIN_UINFO_4((char *)&code_start,
	    (intptr_t)(unwind_start+mid*3*WORDSZ));
	len += COPYIN_UINFO_4((char *)&code_end,
	    (intptr_t)(unwind_start+mid*3*WORDSZ+WORDSZ));
	if (len != 2 * WORDSZ)
	    return UWX_ERR_COPYIN_UTBL;
	if (env->byte_swap) {
	    uwx_swap4(&code_start);
	    uwx_swap4(&code_end);
	}
	TRACE_T_BINSEARCH32(lb, ub, mid, code_start, code_end)
	if (ip >= code_end)
	    lb = mid + 1;
	else if (ip < code_start)
	    ub = mid;
	else
	    break;
    }
    if (ub <= lb)
	return UWX_ERR_NOUENTRY;
    len = COPYIN_UINFO_4((char *)&unwind_info,
	(intptr_t)(unwind_start+mid*3*WORDSZ+2*WORDSZ));
    if (len != WORDSZ)
	return UWX_ERR_COPYIN_UTBL;
    if (env->byte_swap)
	uwx_swap4(&unwind_info);
    uentry->code_start = text_base + code_start;
    uentry->code_end = text_base + code_end;
    uentry->unwind_info = text_base + unwind_info;
    return UWX_OK;
}


/* uwx_search_utable64: Binary search of 64-bit unwind table */

#define COPYIN_UINFO_8(dest, src) \
    (env->remote? \
	(*env->copyin)(UWX_COPYIN_UINFO, (dest), (src), \
						DWORDSZ, env->cb_token) : \
	(*(uint64_t *)(dest) = *(uint64_t *)(src), DWORDSZ) )

int uwx_search_utable64(
    struct uwx_env *env,
    uint64_t text_base,
    uint64_t unwind_start,
    uint64_t unwind_end,
    struct uwx_utable_entry *uentry)
{
    int lb;
    int ub;
    int mid = 0;
    int len;
    uint64_t ip;
    uint64_t code_start;
    uint64_t code_end;
    uint64_t unwind_info;

    /* Since the unwind table uses segment-relative offsets, convert */
    /* the IP in the current context to a segment-relative offset. */

    ip = env->context.special[UWX_REG_IP] - text_base;

    /* Standard binary search. */
    /* Might modify this to do interpolation in the future. */

    lb = 0;
    ub = (unwind_end - unwind_start) / (3 * DWORDSZ);
    while (ub > lb) {
	mid = (lb + ub) / 2;
	len = COPYIN_UINFO_8((char *)&code_start, unwind_start+mid*3*DWORDSZ);
	len += COPYIN_UINFO_8((char *)&code_end,
				unwind_start+mid*3*DWORDSZ+DWORDSZ);
	if (len != 2 * DWORDSZ)
	    return UWX_ERR_COPYIN_UTBL;
	if (env->byte_swap) {
	    uwx_swap8(&code_start);
	    uwx_swap8(&code_end);
	}
	if (ip >= code_end)
	    lb = mid + 1;
	else if (ip < code_start)
	    ub = mid;
	else
	    break;
    }
    if (ub <= lb)
	return UWX_ERR_NOUENTRY;
    len = COPYIN_UINFO_8((char *)&unwind_info,
			unwind_start+mid*3*DWORDSZ+2*DWORDSZ);
    if (len != DWORDSZ)
	return UWX_ERR_COPYIN_UTBL;
    if (env->byte_swap)
	uwx_swap8(&unwind_info);
    uentry->code_start = text_base + code_start;
    uentry->code_end = text_base + code_end;
    uentry->unwind_info = text_base + unwind_info;
    return UWX_OK;
}
