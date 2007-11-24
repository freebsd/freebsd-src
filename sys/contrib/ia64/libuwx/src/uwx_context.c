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
#include "uwx_scoreboard.h"
#include "uwx_step.h"
#include "uwx_trace.h"

int uwx_init_context(
    struct uwx_env *env,
    uint64_t ip,
    uint64_t sp,
    uint64_t bsp,
    uint64_t cfm)
{
    int i;

    if (env == 0)
	return UWX_ERR_NOENV;

    env->context.special[UWX_REG_IP] = ip;
    env->context.special[UWX_REG_SP] = sp;
    env->context.special[UWX_REG_BSP] = bsp;
    env->context.special[UWX_REG_CFM] = cfm;
    for (i = UWX_REG_RP; i < NSPECIALREG; i++)
	env->context.special[i] = 0;
    for (i = 0; i < NPRESERVEDGR; i++)
	env->context.gr[i] = 0;
    env->context.valid_regs = VALID_BASIC4;
    env->context.valid_frs = 0;
    env->rstate = 0;
    (void)uwx_init_history(env);
    return UWX_OK;
}

int uwx_get_reg(struct uwx_env *env, int regid, uint64_t *valp)
{
    int status;
    int sor;
    int rrb_gr;
    uint64_t bsp;
    int n;

    if (env == 0)
	return UWX_ERR_NOENV;

    status = UWX_OK;

    if (regid == UWX_REG_GR(12))
	regid = UWX_REG_SP;
    if (regid < NSPECIALREG && (env->context.valid_regs & (1 << regid)))
	*valp = env->context.special[regid];
    else if (regid == UWX_REG_PSP || regid == UWX_REG_RP ||
					regid == UWX_REG_PFS) {
	status = uwx_restore_markers(env);
	if (status != UWX_OK)
	    return status;
	*valp = env->context.special[regid];
    }
    else if (regid >= UWX_REG_GR(4) && regid <= UWX_REG_GR(7) &&
		(env->context.valid_regs &
		    (1 << (regid - UWX_REG_GR(4) + VALID_GR_SHIFT))) )
	*valp = env->context.gr[regid - UWX_REG_GR(4)];
    else if (regid >= UWX_REG_GR(32) && regid <= UWX_REG_GR(127)) {
	if (env->copyin == 0)
	    return UWX_ERR_NOCALLBACKS;
	bsp = env->context.special[UWX_REG_BSP];
	TRACE_C_GET_REG(regid, bsp)
	regid -= UWX_REG_GR(32);
	sor = (((int) env->context.special[UWX_REG_CFM] >> 14) & 0x0f) * 8;
	rrb_gr = ((int) env->context.special[UWX_REG_CFM] >> 18) & 0x7f;
	if (sor != 0 && rrb_gr != 0 && regid < sor) {
	    TRACE_C_ROTATE_GR(regid, sor, rrb_gr, (regid+rrb_gr)%sor)
	    regid = (regid + rrb_gr) % sor;
	}
	bsp = uwx_add_to_bsp(bsp, regid);
	n = (*env->copyin)(UWX_COPYIN_RSTACK, (char *)valp,
		    bsp, DWORDSZ, env->cb_token);
	if (n != DWORDSZ)
	    status = UWX_ERR_COPYIN_RSTK;
    }
    else if (regid == UWX_REG_GR(0))
	*valp = 0;
    else if (regid >= UWX_REG_BR(1) && regid <= UWX_REG_BR(5) &&
		(env->context.valid_regs &
		    (1 << (regid - UWX_REG_BR(1) + VALID_BR_SHIFT))) )
	*valp = env->context.br[regid - UWX_REG_BR(1)];
    else if (regid >= UWX_REG_FR(2) && regid <= UWX_REG_FR(5) &&
	    (env->context.valid_frs & (1 << (regid - UWX_REG_FR(2)))) ) {
	valp[0] = env->context.fr[regid - UWX_REG_FR(2)].part0;
	valp[1] = env->context.fr[regid - UWX_REG_FR(2)].part1;
    }
    else if (regid >= UWX_REG_FR(16) && regid <= UWX_REG_FR(31) &&
	    (env->context.valid_frs & (1 << (regid - UWX_REG_FR(16) + 4))) ) {
	valp[0] = env->context.fr[regid - UWX_REG_FR(16) + 4].part0;
	valp[1] = env->context.fr[regid - UWX_REG_FR(16) + 4].part1;
    }
    else if ( (regid < NSPECIALREG) ||
		(regid >= UWX_REG_GR(1) && regid <= UWX_REG_GR(31)) ||
		(regid >= UWX_REG_BR(0) && regid <= UWX_REG_BR(7)) ) {
	if (env->copyin == 0)
	    return UWX_ERR_NOCALLBACKS;
	n = (*env->copyin)(UWX_COPYIN_REG, (char *)valp,
			    regid, DWORDSZ, env->cb_token);
	if (n != DWORDSZ)
	    status = UWX_ERR_COPYIN_REG;
    }
    else if (regid >= UWX_REG_FR(2) && regid <= UWX_REG_FR(127)) {
	if (env->copyin == 0)
	    return UWX_ERR_NOCALLBACKS;
	n = (*env->copyin)(UWX_COPYIN_REG, (char *)valp,
			    regid, 2*DWORDSZ, env->cb_token);
	if (n != 2*DWORDSZ)
	    status = UWX_ERR_COPYIN_REG;
    }
    else if (regid == UWX_REG_FR(0)) {
	valp[0] = 0;
	valp[1] = 0;
    }
    else if (regid == UWX_REG_FR(1)) {
	valp[0] = 0x000000000000ffffULL;
	valp[1] = 0x8000000000000000ULL;
    }
    else
	status = UWX_ERR_BADREGID;
    return status;
}

int uwx_get_nat(struct uwx_env *env, int regid, int *natp)
{
    int status;
    int sor;
    int rrb_gr;
    uint64_t bsp;
    uint64_t natcollp;
    uint64_t natcoll;
    int n;

    if (env == 0)
	return UWX_ERR_NOENV;

    status = UWX_OK;

    if (regid >= UWX_REG_GR(4) && regid <= UWX_REG_GR(7) &&
		(env->context.valid_regs &
		    (1 << (regid - UWX_REG_GR(4) + VALID_GR_SHIFT))) ) {
	*natp = (env->context.special[UWX_REG_PRIUNAT] >>
				(regid - UWX_REG_GR(4)) ) & 0x01;
    }
    else if (regid >= UWX_REG_GR(32) && regid <= UWX_REG_GR(127)) {
	if (env->copyin == 0)
	    return UWX_ERR_NOCALLBACKS;
	bsp = env->context.special[UWX_REG_BSP];
	regid -= UWX_REG_GR(32);
	sor = (((int) env->context.special[UWX_REG_CFM] >> 14) & 0x0f) * 8;
	rrb_gr = ((int) env->context.special[UWX_REG_CFM] >> 18) & 0x7f;
	if (sor != 0 && rrb_gr != 0 && regid < sor) {
	    regid = (regid + rrb_gr) % sor;
	}
	bsp = uwx_add_to_bsp(bsp, regid);
	natcollp = bsp | 0x01f8;
	n = (*env->copyin)(UWX_COPYIN_RSTACK, (char *)&natcoll,
			bsp, DWORDSZ, env->cb_token);
	if (n != DWORDSZ)
	    return UWX_ERR_COPYIN_RSTK;
	*natp = (int)(natcoll >> (((int)bsp >> 3) & 0x3f)) & 0x01;
    }
    else if (regid == UWX_REG_GR(0))
	*natp = 0;
    else
	status = UWX_ERR_BADREGID;
    return status;
}

int uwx_get_spill_loc(struct uwx_env *env, int regid, uint64_t *dispp)
{
    int status;
    int sor;
    int rrb_gr;
    uint64_t bsp;

    if (env == 0)
	return UWX_ERR_NOENV;

    status = UWX_OK;

    if (regid == UWX_REG_GR(12))
	regid = UWX_REG_SP;
    if (regid < NSPECIALREG)
	*dispp = env->history.special[regid];
    else if (regid >= UWX_REG_GR(4) && regid <= UWX_REG_GR(7))
	*dispp = env->history.gr[regid - UWX_REG_GR(4)];
    else if (regid >= UWX_REG_GR(32) && regid <= UWX_REG_GR(127)) {
	bsp = env->context.special[UWX_REG_BSP];
	regid -= UWX_REG_GR(32);
	sor = (((int) env->context.special[UWX_REG_CFM] >> 14) & 0x0f) * 8;
	rrb_gr = ((int) env->context.special[UWX_REG_CFM] >> 18) & 0x7f;
	if (sor != 0 && rrb_gr != 0 && regid < sor)
	    regid = (regid + rrb_gr) % sor;
	bsp = uwx_add_to_bsp(bsp, regid);
	*dispp = UWX_DISP_RSTK(bsp);
    }
    else if (regid >= UWX_REG_BR(1) && regid <= UWX_REG_GR(5))
	*dispp = env->history.br[regid - UWX_REG_BR(1)];
    else if (regid >= UWX_REG_FR(2) && regid <= UWX_REG_FR(5))
	*dispp = env->history.fr[regid - UWX_REG_FR(2)];
    else if (regid >= UWX_REG_FR(16) && regid <= UWX_REG_FR(31))
	*dispp = env->history.fr[regid - UWX_REG_FR(16) + 4];
    else if ( (regid >= UWX_REG_GR(1) && regid <= UWX_REG_GR(31)) ||
		(regid >= UWX_REG_BR(0) && regid <= UWX_REG_BR(7)) ||
		(regid >= UWX_REG_FR(2) && regid <= UWX_REG_FR(127)) )
	*dispp = UWX_DISP_REG(regid);
    else
	status = UWX_ERR_BADREGID;
    return status;
}

int uwx_set_reg(struct uwx_env *env, int regid, uint64_t val)
{
    int status;

    if (env == 0)
	return UWX_ERR_NOENV;

    if (regid == UWX_REG_GR(12))
	regid = UWX_REG_SP;
    if (regid < NSPECIALREG) {
	env->context.special[regid] = val;
	env->context.valid_regs |= 1 << regid;
	status = UWX_OK;
    }
    else if (regid >= UWX_REG_GR(4) && regid <= UWX_REG_GR(7)) {
	env->context.gr[regid - UWX_REG_GR(4)] = val;
	env->context.valid_regs |=
			1 << (regid - UWX_REG_GR(4) + VALID_GR_SHIFT);
	status = UWX_OK;
    }
    else if (regid >= UWX_REG_GR(32) && regid <= UWX_REG_GR(127)) {
	status = UWX_ERR_BADREGID;
    }
    else if (regid >= UWX_REG_BR(1) && regid <= UWX_REG_BR(5)) {
	env->context.br[regid - UWX_REG_BR(1)] = val;
	env->context.valid_regs |=
			1 << (regid - UWX_REG_BR(1) + VALID_BR_SHIFT);
	status = UWX_OK;
    }
    else
	status = UWX_ERR_BADREGID;
    return status;
}

int uwx_set_fr(struct uwx_env *env, int regid, uint64_t *val)
{

    if (regid >= UWX_REG_FR(2) && regid <= UWX_REG_FR(5))
	regid -= UWX_REG_FR(2);
    else if (regid >= UWX_REG_FR(16) && regid <= UWX_REG_FR(31))
	regid -= UWX_REG_FR(16) - 4;
    else
	return UWX_ERR_BADREGID;

    env->context.fr[regid].part0 = val[0];
    env->context.fr[regid].part1 = val[1];
    env->context.valid_frs |= 1 << regid;
    env->nsbreg = NSBREG;
    return UWX_OK;
}

uint64_t uwx_add_to_bsp(uint64_t bsp, int nslots)
{
    int bias;

    /*
     *  Here's a picture of the backing store as modeled in
     *  the computations below. "X" marks NaT collections at
     *  every 0x1f8 mod 0x200 address.
     *
     *  To make the NaT adjustments easier, we bias the current bsp
     *  by enough slots to place it at the previous NaT collection.
     *  Then we need to add the bias to the number of slots,
     *  then add 1 for every 63 slots to account for NaT collections.
     *  Then we can remove the bias again and add the adjusted
     *  number of slots to the bsp.
     *
     *   0                           1f8                             3f8
     *  +---------------------------------------------------------------+
     *  |                              X                               X|
     *  +---------------------------------------------------------------+
     *   <-------- bias -------->
     *                           <--- nslots --->
     *                           ^
     *                           |
     *                          bsp
     *   <------- adjusted (nslots + bias) ------->

     *  When subtracting from bsp, we avoid depending on the sign of
     *  the quotient by adding 63*8 before division and subtracting 8
     *  after division. (Assumes that we will never be called upon
     *  to subtract more than 504 slots from bsp.)
     *
     *   0                           1f8                             3f8
     *  +---------------------------------------------------------------+
     *  |                              X                               X|
     *  +---------------------------------------------------------------+
     *                                  <-- bias -->
     *                            <--- |nslots| --->
     *                                              ^
     *                                              |
     *                                             bsp
     *                           <----------------->
     *                        adjusted |nslots + bias|
     */

    bias = ((unsigned int)bsp & 0x1f8) / DWORDSZ;
    nslots += (nslots + bias + 63*8) / 63 - 8;
    return bsp + nslots * DWORDSZ;
}

#if 0
int uwx_selftest_bsp_arithmetic()
{
    int i;
    int j;
    int r;
    uint64_t bstore[161];
    uint64_t *bsp;
    uint64_t *p;
    int failed = 0;

    printf("uwx_selftest_bsp_arithmetic: bsp at %08lx\n", (unsigned int)bstore);
    r = 0;
    bsp = bstore;
    for (i = 0; i < 161; i++) {
	if (((unsigned int)bsp & 0x1f8) == 0x1f8)
	    *bsp++ = 1000 + r;
	else
	    *bsp++ = r++;
    }

    printf("uwx_selftest_bsp_arithmetic: plus tests...\n");
    bsp = bstore;
    for (i = 0; i < 64; i++) {
	r = (int)*bsp;
	if (r >= 1000)
	    r -= 1000;
	for (j = 0; j < 96; j++) {
	    p = (uint64_t *)uwx_add_to_bsp((uint64_t)bsp, j);
	    if (*p != (r + j)) {
		failed++;
		printf("%d [%08lx] + %d -> %08lx ",
				i, (unsigned int)bsp, j, (unsigned int)p);
		printf("(read %d instead of %d)\n", (int)*p, r + j);
	    }
	}
	bsp++;
    }

    printf("uwx_selftest_bsp_arithmetic: minus tests...\n");
    bsp = &bstore[161];
    for (i = 63; i >= 0; i--) {
	bsp--;
	r = (int)*bsp;
	if (r >= 1000)
	    r -= 1000;
	for (j = 0; j < 96; j++) {
	    p = (uint64_t *)uwx_add_to_bsp((uint64_t)bsp, -j);
	    if (*p != (r - j)) {
		failed++;
		printf("%d [%08lx] - %d -> %08lx ",
				i, (unsigned int)bsp, j, (unsigned int)p);
		printf("(read %d instead of %d)\n", (int)*p, r - j);
	    }
	}
    }

    return failed;
}
#endif
