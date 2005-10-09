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

#define UWX_TRACE_SB		1	/* UWX_TRACE=b: scoreboard mgmt */
#define UWX_TRACE_UINFO		2	/* UWX_TRACE=i: unwind info */
#define UWX_TRACE_RSTATE	4	/* UWX_TRACE=r: reg state vector */
#define UWX_TRACE_STEP		8	/* UWX_TRACE=s: step */
#define UWX_TRACE_UTABLE	16	/* UWX_TRACE=t: unwind tbl search */
#define UWX_TRACE_CONTEXT	32	/* UWX_TRACE=c: context */
#define UWX_TRACE_COPYIN	64	/* UWX_TRACE=C: copyin callback */
#define UWX_TRACE_LOOKUPIP	128	/* UWX_TRACE=L: lookupip callback */

#ifdef UWX_TRACE_ENABLE

extern void uwx_trace_init(struct uwx_env *env);

extern void uwx_dump_rstate(int regid, uint64_t rstate);

struct uwx_rhdr;

extern void uwx_dump_scoreboard(
    struct uwx_scoreboard *scoreboard,
    int nsbreg,
    struct uwx_rhdr *rhdr,
    int cur_slot,
    int ip_slot);

#define TRACE_INIT uwx_trace_init(env);

#define TRACE_B_REUSE(id) \
    if (env->trace & UWX_TRACE_SB) \
	printf("uwx_alloc_scoreboard: reuse id %d\n", (id));

#define TRACE_B_ALLOC(id) \
    if (env->trace & UWX_TRACE_SB) \
	printf("uwx_alloc_scoreboard: alloc id %d\n", (id));

#define TRACE_B_POP(id) \
    if (env->trace & UWX_TRACE_SB) \
	printf("uwx_pop_scoreboards: free id %d\n", (id));

#define TRACE_B_LABEL(label) \
    if (env->trace & UWX_TRACE_SB) \
	printf("uwx_label_scoreboard: label %d\n", (label));

#define TRACE_B_LABEL_COPY(id) \
    if (env->trace & UWX_TRACE_SB) \
	printf("uwx_label_scoreboard: copy id %d\n", (id));

#define TRACE_B_LABEL_REVERSE(back, new) \
    if (env->trace & UWX_TRACE_SB) \
	printf("uwx_label_scoreboard: reverse link %d -> %d\n", \
			    (new)->id, ((back) == 0) ? -1 : (back)->id);

#define TRACE_B_COPY(label, id) \
    if (env->trace & UWX_TRACE_SB) \
	printf("uwx_copy_scoreboard: label %d, cur sb id %d\n", (label), (id));

#define TRACE_B_COPY_FREE(id) \
    if (env->trace & UWX_TRACE_SB) \
	printf("uwx_copy_scoreboard: free id %d\n", (id));

#define TRACE_B_COPY_FOUND(id) \
    if (env->trace & UWX_TRACE_SB) \
	printf("uwx_copy_scoreboard: found id %d\n", (id));

#define TRACE_B_COPY_COPY(id) \
    if (env->trace & UWX_TRACE_SB) \
	printf("uwx_copy_scoreboard: copy id %d\n", (id));

#define TRACE_B_COPY_REVERSE(back, new) \
    if (env->trace & UWX_TRACE_SB) \
	printf("uwx_copy_scoreboard: reverse link %d -> %d\n", \
			    (new)->id, ((back) == 0) ? -1 : (back)->id);

#define TRACE_B_FREE(id) \
    if (env->trace & UWX_TRACE_SB) \
	printf("uwx_free_scoreboards: free id %d\n", (id));

#define TRACE_I_DECODE_RHDR_1(name, b0) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_rhdr:     %02x                   %s\n", \
			(b0), (name));

#define TRACE_I_DECODE_RHDR_1L(name, b0, val) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_rhdr:     %02x %08x          %s\n", \
			(b0), (int)(val), (name));

#define TRACE_I_DECODE_RHDR_2L(name, b0, b1, val) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_rhdr:     %02x %02x %08x       %s\n", \
			(b0), (b1), (int)(val), (name));

#define TRACE_I_DECODE_PROLOGUE_1(name, b0) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_prologue: %02x                   %s\n", \
			(b0), (name));

#define TRACE_I_DECODE_PROLOGUE_1L(name, b0, val) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_prologue: %02x %08x          %s\n", \
			(b0), (int)(val), (name));

#define TRACE_I_DECODE_PROLOGUE_1LL(name, b0, val1, val2) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_prologue: %02x %08x %08x %s\n", \
			(b0), (int)(val1), (int)(val2), (name));

#define TRACE_I_DECODE_PROLOGUE_2(name, b0, b1) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_prologue: %02x %02x                %s\n", \
			(b0), (b1), (name));

#define TRACE_I_DECODE_PROLOGUE_2L(name, b0, b1, val) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_prologue: %02x %02x %08x       %s\n", \
			(b0), (b1), (int)(val), (name));

#define TRACE_I_DECODE_PROLOGUE_3(name, b0, b1, b2) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_prologue: %02x %02x %02x             %s\n", \
			(b0), (b1), (b2), (name));

#define TRACE_I_DECODE_PROLOGUE_4(name, b0, b1, b2, b3) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_prologue: %02x %02x %02x %02x          %s\n", \
			(b0), (b1), (b2), (b3), (name));

#define TRACE_I_DECODE_PROLOGUE_SPILL_BASE(spill_base) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_prologue: spill base = %08x\n", (int)(spill_base));

#define TRACE_I_DECODE_PROLOGUE_MASKS(gr_mem_mask, gr_gr_mask) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_prologue: gr_mem_mask = %02x; gr_gr_mask = %02x\n", \
			(gr_mem_mask), (gr_gr_mask));

#define TRACE_I_DECODE_PROLOGUE_NSPILL(ngr) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_prologue: ngr = %d\n", (ngr));

#define TRACE_I_DECODE_BODY_1(name, b0) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_body:     %02x                   %s\n", \
			(b0), (name));

#define TRACE_I_DECODE_BODY_1L(name, b0, val) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_body:     %02x %08x          %s\n", \
			(b0), (int)(val), (name));

#define TRACE_I_DECODE_BODY_1LL(name, b0, val1, val2) \
    if (env->trace & UWX_TRACE_UINFO) \
	printf("uwx_decode_body:     %02x %08x %08x %s\n", \
			(b0), (int)(val1), (int)(val2), (name));

#define TRACE_R_UIB(uentry, ulen) \
    if (env->trace & UWX_TRACE_RSTATE) \
	printf("Unwind info block (flags = %08x %08x, ulen = %d)\n", \
		    (unsigned int)((uentry)->unwind_flags >> 32), \
		    (unsigned int)(uentry)->unwind_flags, \
		    (ulen));

#define TRACE_R_DUMP_SB(scoreboard, rhdr, cur_slot, ip_slot) \
    if (env->trace & UWX_TRACE_RSTATE) \
	uwx_dump_scoreboard(scoreboard, env->nsbreg, \
				&(rhdr), cur_slot, ip_slot);

#define TRACE_S_STEP(rstate) \
    if (env->trace & UWX_TRACE_STEP) { \
	printf("uwx_restore_markers:\n"); \
	uwx_dump_rstate(SBREG_RP, (rstate)[SBREG_RP]); \
	uwx_dump_rstate(SBREG_PSP, (rstate)[SBREG_PSP]); \
	uwx_dump_rstate(SBREG_PFS, (rstate)[SBREG_PFS]); \
    }

#define TRACE_S_RESTORE_REG(regname, rstate, val) \
    if (env->trace & UWX_TRACE_STEP) \
	printf("  restore %-7s (rstate = %08x %08x) = %08x %08x\n", \
			regname, \
			(unsigned int) ((rstate) >> 32), \
			(unsigned int) (rstate), \
			(unsigned int) ((val) >> 32), \
			(unsigned int) (val));

#define TRACE_S_RESTORE_GR(regid, rstate, val) \
    if (env->trace & UWX_TRACE_STEP) \
	printf("  restore GR%d     (rstate = %08x %08x) = %08x %08x\n", \
			(regid) + 4, \
			(unsigned int) ((rstate) >> 32), \
			(unsigned int) (rstate), \
			(unsigned int) ((val) >> 32), \
			(unsigned int) (val));

#define TRACE_S_RESTORE_BR(regid, rstate, val) \
    if (env->trace & UWX_TRACE_STEP) \
	printf("  restore BR%d     (rstate = %08x %08x) = %08x %08x\n", \
			(regid) + 1, \
			(unsigned int) ((rstate) >> 32), \
			(unsigned int) (rstate), \
			(unsigned int) ((val) >> 32), \
			(unsigned int) (val));

#define TRACE_S_RESTORE_FR(regid, rstate, fval) \
    if (env->trace & UWX_TRACE_STEP) \
	printf("  restore FR%d     (rstate = %08x %08x) = %08x %08x %08x %08x\n", \
			(regid) + 1, \
			(unsigned int) ((rstate) >> 32), \
			(unsigned int) (rstate), \
			(unsigned int) ((fval[0]) >> 32), \
			(unsigned int) (fval[0]), \
			(unsigned int) ((fval[1]) >> 32), \
			(unsigned int) (fval[1]));

#define TRACE_T_SEARCH32(ip) \
    if (env->trace & UWX_TRACE_UTABLE) \
	printf("uwx_search_utable32 (relative ip = %08x)\n", (ip));

#define TRACE_T_BINSEARCH32(lb, ub, mid, code_start, code_end) \
    if (env->trace & UWX_TRACE_UTABLE) \
	printf("    lb/ub = %d/%d, mid = %d, start/end = %08x %08x\n", \
			    lb, ub, mid, code_start, code_end);

#define TRACE_C_GET_REG(regid, bsp) \
	if (env->trace & UWX_TRACE_CONTEXT) \
	    printf("uwx_get_reg (gr%d, bsp = %08x %08x)\n", \
			(regid) - UWX_REG_GR(0), \
			(unsigned int) ((bsp) >> 32), \
			(unsigned int) (bsp));

#define TRACE_C_ROTATE_GR(regid, sor, rrb_gr, newregid) \
	if (env->trace & UWX_TRACE_CONTEXT) \
	    printf("uwx_get_reg (gr%d, sor = %d, rrb = %d) --> gr%d\n", \
			(regid) + 32, \
			(sor), \
			(rrb_gr), \
			(newregid) + 32);

#define TRACE_SELF_COPYIN4(rem, len, wp) \
    if (info->trace & UWX_TRACE_COPYIN) \
	printf("copyin (rem = %08x %08x, len = %d, val = %08x)\n", \
			(unsigned int) ((rem) >> 32), \
			(unsigned int) (rem), \
			(len), *(wp));

#define TRACE_SELF_COPYIN8(rem, len, dp) \
    if (info->trace & UWX_TRACE_COPYIN) \
	printf("copyin (rem = %08x %08x, len = %d, val = %08x %08x)\n", \
			(unsigned int) ((rem) >> 32), \
			(unsigned int) (rem), \
			(len), \
			((unsigned int *)(dp))[0], \
			((unsigned int *)(dp))[1]);

#define TRACE_SELF_LOOKUP(ip) \
    if (info->trace & UWX_TRACE_LOOKUPIP) \
	printf("Lookup IP callback: ip = %08x %08x\n", \
			(unsigned int) ((ip) >> 32), \
			(unsigned int) (ip));

#define TRACE_SELF_LOOKUP_DESC(text_base, unwind_base) \
	if (info->trace & UWX_TRACE_LOOKUPIP) { \
	    printf("  text base:    %08x %08x\n", \
			(unsigned int) ((text_base) >> 32), \
			(unsigned int) (text_base)); \
	    printf("  unwind base:  %08x %08x\n", \
			(unsigned int) ((uint64_t)(unwind_base) >> 32), \
			(unsigned int) (unwind_base)); \
	    printf("  unwind flags: %08x %08x\n", \
			(unsigned int) ((unwind_base)[0] >> 32), \
			(unsigned int) (unwind_base)[0]); \
	    printf("  unwind start: %08x %08x\n", \
			(unsigned int) (((text_base)+(unwind_base)[1]) >> 32), \
			(unsigned int) ((text_base)+(unwind_base)[1])); \
	    printf("  unwind end:   %08x %08x\n", \
			(unsigned int) (((text_base)+(unwind_base)[2]) >> 32), \
			(unsigned int) ((text_base)+(unwind_base)[2])); \
	}

#else /* !UWX_TRACE_ENABLE */

#define TRACE_INIT
#define TRACE_B_REUSE(id)
#define TRACE_B_ALLOC(id)
#define TRACE_B_POP(id)
#define TRACE_B_LABEL(label)
#define TRACE_B_LABEL_COPY(id)
#define TRACE_B_LABEL_REVERSE(back, new)
#define TRACE_B_COPY(label, id)
#define TRACE_B_COPY_FREE(id)
#define TRACE_B_COPY_FOUND(id)
#define TRACE_B_COPY_COPY(id)
#define TRACE_B_COPY_REVERSE(back, new)
#define TRACE_B_FREE(id)
#define TRACE_I_DECODE_RHDR_1(name, b0)
#define TRACE_I_DECODE_RHDR_1L(name, b0, val)
#define TRACE_I_DECODE_RHDR_2L(name, b0, b1, val)
#define TRACE_I_DECODE_PROLOGUE_1(name, b0)
#define TRACE_I_DECODE_PROLOGUE_1L(name, b0, val)
#define TRACE_I_DECODE_PROLOGUE_1LL(name, b0, val1, val2)
#define TRACE_I_DECODE_PROLOGUE_2(name, b0, b1)
#define TRACE_I_DECODE_PROLOGUE_2L(name, b0, b1, parm1)
#define TRACE_I_DECODE_PROLOGUE_3(name, b0, b1, b2)
#define TRACE_I_DECODE_PROLOGUE_4(name, b0, b1, b2, b3)
#define TRACE_I_DECODE_PROLOGUE_SPILL_BASE(spill_base)
#define TRACE_I_DECODE_PROLOGUE_MASKS(gr_mem_mask, gr_gr_mask)
#define TRACE_I_DECODE_PROLOGUE_NSPILL(ngr)
#define TRACE_I_DECODE_BODY_1(name, b0)
#define TRACE_I_DECODE_BODY_1L(name, b0, parm1)
#define TRACE_I_DECODE_BODY_1LL(name, b0, parm1, parm2)
#define TRACE_R_UIB(uentry, ulen)
#define TRACE_R_DUMP_SB(scoreboard, rhdr, cur_slot, ip_slot)
#define TRACE_S_STEP(rstate)
#define TRACE_S_RESTORE_REG(regname, rstate, val)
#define TRACE_S_RESTORE_GR(regid, rstate, val)
#define TRACE_S_RESTORE_BR(regid, rstate, val)
#define TRACE_S_RESTORE_FR(regid, rstate, val)
#define TRACE_T_SEARCH32(ip)
#define TRACE_T_BINSEARCH32(lb, ub, mid, code_start, code_end)
#define TRACE_C_GET_REG(regid, bsp)
#define TRACE_C_ROTATE_GR(regid, sor, rrb_gr, newregid)
#define TRACE_SELF_COPYIN4(rem, len, wp)
#define TRACE_SELF_COPYIN8(rem, len, dp)
#define TRACE_SELF_LOOKUP(ip)
#define TRACE_SELF_LOOKUP_DESC(text_base, unwind_base)

#endif /* UWX_TRACE_ENABLE */

