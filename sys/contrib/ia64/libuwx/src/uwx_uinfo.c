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
#include "uwx_uinfo.h"
#include "uwx_utable.h"
#include "uwx_scoreboard.h"
#include "uwx_bstream.h"
#include "uwx_trace.h"
#include "uwx_swap.h"

int uwx_count_ones(unsigned int mask);

/*
 *  uwx_uinfo.c
 *
 *  This file contains the routines for reading and decoding
 *  the unwind information block. 
 *
 *  The main entry point, uwx_decode_uinfo(), is given a pointer
 *  to an unwind table entry and a pointer (passed by reference)
 *  to be filled in with a pointer to an update vector. It will
 *  read and decode the unwind descriptors contained in the
 *  unwind information block, then build the register state array,
 *  which describes the actions necessary to step from the current
 *  frame to the previous one.
 */

#define COPYIN_UINFO_4(dest, src) \
    (env->remote? \
	(*env->copyin)(UWX_COPYIN_UINFO, (dest), (src), \
						WORDSZ, env->cb_token) : \
	(*(uint32_t *)(dest) = *(uint32_t *)(src), WORDSZ) )

#define COPYIN_UINFO_8(dest, src) \
    (env->remote? \
	(*env->copyin)(UWX_COPYIN_UINFO, (dest), (src), \
						DWORDSZ, env->cb_token) : \
	(*(uint64_t *)(dest) = *(uint64_t *)(src), DWORDSZ) )


/* uwx_default_rstate: Returns the default register state for a leaf routine */

int uwx_default_rstate(struct uwx_env *env, uint64_t **rstatep)
{
    struct uwx_scoreboard *sb;

    sb = uwx_init_scoreboards(env);
    *rstatep = sb->rstate;
    return UWX_OK;
}


/* uwx_decode_uinfo: Decodes unwind info region */

int uwx_decode_uinfo(
    struct uwx_env *env,
    struct uwx_utable_entry *uentry,
    uint64_t **rstatep)
{
    uint64_t uinfohdr;
    unsigned int ulen;
    int len;
    struct uwx_bstream bstream;
    struct uwx_scoreboard *scoreboard;
    int ip_slot;
    int cur_slot;
    int status;
    struct uwx_rhdr rhdr;

    /* Remember the offset from the start of the function */
    /* to the current IP. This helps the client find */
    /* the symbolic information. */

    env->function_offset = env->remapped_ip - uentry->code_start;

    /* Read the unwind info header using the copyin callback. */
    /* (If we're reading a 32-bit unwind table, we need to */
    /* read the header as two 32-bit pieces to preserve the */
    /* guarantee that we always call copyin for aligned */
    /* 4-byte or 8-byte chunks.) */
    /* Then compute the length of the unwind descriptor */
    /* region and initialize a byte stream to read it. */

    if (uentry->unwind_flags & UNWIND_TBL_32BIT) {
	len = COPYIN_UINFO_4((char *)&uinfohdr, uentry->unwind_info);
	len += COPYIN_UINFO_4((char *)&uinfohdr + WORDSZ,
					uentry->unwind_info + WORDSZ);
	}
    else
	len = COPYIN_UINFO_8((char *)&uinfohdr, uentry->unwind_info);
    if (len != DWORDSZ)
	return UWX_ERR_COPYIN_UINFO;
    if (env->byte_swap)
	uwx_swap8(&uinfohdr);
    if (uentry->unwind_flags & UNWIND_TBL_32BIT)
	ulen = UNW_LENGTH(uinfohdr) * WORDSZ;
    else
	ulen = UNW_LENGTH(uinfohdr) * DWORDSZ;
    uwx_init_bstream(&bstream, env,
		uentry->unwind_info + DWORDSZ, ulen, UWX_COPYIN_UINFO);

    TRACE_R_UIB(uentry, ulen)

    /* Create an initial scoreboard for tracking the unwind state. */

    scoreboard = uwx_init_scoreboards(env);

    /* Prepare to read and decode the unwind regions described */
    /* by the unwind info block. Find the target "ip" slot */
    /* relative to the beginning of the region. The lower 4 bits */
    /* of the actual IP encode the slot number within a bundle. */

    cur_slot = 0;
    ip_slot = (int) ((env->context.special[UWX_REG_IP] & ~0x0fLL)
							- uentry->code_start)
		/ BUNDLESZ * SLOTSPERBUNDLE
		+ (unsigned int) (env->context.special[UWX_REG_IP] & 0x0f);

    /* Loop over the regions in the unwind info block. */

    for (;;) {

	/* Decode the next region header. */
	/* We have an error if we reach the end of the info block, */
	/* since we should have found our target ip slot by then. */
	/* We also have an error if the next byte isn't a region */
	/* header record. */

	status = uwx_decode_rhdr(env, &bstream, &rhdr);
	if (status != UWX_OK)
	    return status;

	/* If a prologue region, get a new scoreboard, pushing */
	/* the previous one onto the prologue stack. Then read */
	/* and decode the prologue region records. */

	if (rhdr.is_prologue) {
	    scoreboard = uwx_new_scoreboard(env, scoreboard);
	    if (scoreboard == 0)
		return UWX_ERR_NOMEM;
	    status = uwx_decode_prologue(env, &bstream,
					    scoreboard, &rhdr, ip_slot);
	}

	/* If a body region, read and decode the body region */
	/* records. If the body has an epilogue count, */
	/* uwx_decode_body will note that in the region header */
	/* record for use at the bottom of the loop. */

	else {
	    status = uwx_decode_body(env, &bstream, scoreboard, &rhdr, ip_slot);
	}

	if (status != UWX_OK)
	    return status;

	TRACE_R_DUMP_SB(scoreboard, rhdr, cur_slot, ip_slot)

	/* If the target ip slot is within this region, we're done. */
	/* Return the scoreboard's register state array. */

	if (ip_slot < rhdr.rlen) {
	    *rstatep = scoreboard->rstate;
	    return UWX_OK;
	}

	/* Otherwise, update the current ip slot, pop the */
	/* scoreboard stack based on the epilogue count, */
	/* and loop back around for the next region. */

	cur_slot += rhdr.rlen;
	ip_slot -= rhdr.rlen;
	if (rhdr.ecount > 0) {
	    scoreboard = uwx_pop_scoreboards(env, scoreboard, rhdr.ecount);
	    if (scoreboard == 0)
		return UWX_ERR_PROLOG_UF;
	}
    }
    /*NOTREACHED*/
}


/* uwx_decode_rhdr: Decodes a region header record */

int uwx_decode_rhdr(
    struct uwx_env *env,
    struct uwx_bstream *bstream,
    struct uwx_rhdr *rhdr)
{
    int b0;
    int b1;
    uint64_t val;
    int status;

    /* Get the first byte of the next descriptor record. */
    b0 = uwx_get_byte(bstream);
    if (b0 < 0)
	return UWX_ERR_NOUDESC;

    /* Initialize region header record. */

    rhdr->is_prologue = 0;
    rhdr->rlen = 0;
    rhdr->mask = 0;
    rhdr->grsave = 0;
    rhdr->ecount = 0;

    /* Format R1 */

    if (b0 < 0x40) {
	if ((b0 & 0x20) == 0) {
	    TRACE_I_DECODE_RHDR_1("(R1) prologue", b0)
	    rhdr->is_prologue = 1;
	}
	else {
	    TRACE_I_DECODE_RHDR_1("(R1) body", b0)
	}
	rhdr->rlen = b0 & 0x1f;
    }

    /* Format R2 */

    else if (b0 < 0x60) {
	b1 = uwx_get_byte(bstream);
	if (b1 < 0)
	    return UWX_ERR_BADUDESC;
	status = uwx_get_uleb128(bstream, &val);
	if (status != 0)
	    return UWX_ERR_BADUDESC;
	TRACE_I_DECODE_RHDR_2L("(R2) prologue_gr", b0, b1, val)
	rhdr->is_prologue = 1;
	rhdr->rlen = (unsigned int) val;
	rhdr->mask = ((b0 & 0x07) << 1) | (b1 >> 7);
	rhdr->grsave = b1 & 0x7f;
    }

    /* Format R3 */

    else if (b0 < 0x80) {
	status = uwx_get_uleb128(bstream, &val);
	if (status != 0)
	    return UWX_ERR_BADUDESC;
	if ((b0 & 0x03) == 0) {
	    TRACE_I_DECODE_RHDR_1L("(R3) prologue", b0, val)
	    rhdr->is_prologue = 1;
	}
	else {
	    TRACE_I_DECODE_RHDR_1L("(R3) body", b0, val)
	}
	rhdr->rlen = (unsigned int) val;
    }

    /* Otherwise, not a region header record. */

    else {
	TRACE_I_DECODE_RHDR_1("(?)", b0)
	return UWX_ERR_BADUDESC;
    }

    return UWX_OK;
}


/* uwx_decode_prologue: Decodes a prologue region */

int uwx_decode_prologue(
    struct uwx_env *env,
    struct uwx_bstream *bstream,
    struct uwx_scoreboard *scoreboard,
    struct uwx_rhdr *rhdr,
    int ip_slot)
{
    int status;
    int reg;
    int mask;
    int b0;
    int b1;
    int b2;
    int b3;
    int r;
    int t;
    int i;
    uint64_t parm1;
    uint64_t parm2;
    uint64_t newrstate[NSBREG];
    int tspill[NSBREG];
    int priunat_mem_rstate;
    int t_priunat_mem;
    unsigned int gr_mem_mask;
    unsigned int br_mem_mask;
    unsigned int fr_mem_mask;
    unsigned int gr_gr_mask;
    unsigned int br_gr_mask;
    int ngr;
    int nbr;
    int nfr;
    unsigned int spill_base;
    unsigned int gr_base;
    unsigned int br_base;
    unsigned int fr_base;

    /* Initialize an array of register states from the current */
    /* scoreboard, along with a parallel array of spill times. */
    /* We use this as a temporary scoreboard, then update the */
    /* real scoreboard at the end of the procedure. */
    /* We initialize the spill time to (rhdr.rlen - 1) so that */
    /* spills without a "when" descriptor will take effect */
    /* at the end of the prologue region. */
    /* (Boundary condition: all actions in a zero-length prologue */
    /* will appear to have happened in the instruction slot */
    /* immediately preceding the prologue.) */

    for (i = 0; i < env->nsbreg; i++) {
	newrstate[i] = scoreboard->rstate[i];
	tspill[i] = rhdr->rlen - 1;
    }
    priunat_mem_rstate = UWX_DISP_NONE;
    t_priunat_mem = rhdr->rlen - 1;

    fr_mem_mask = 0;
    gr_mem_mask = 0;
    br_mem_mask = 0;
    gr_gr_mask = 0;
    br_gr_mask = 0;
    nfr = 0;
    ngr = 0;
    nbr = 0;
    spill_base = 0;

    /* If prologue_gr header record supplied mask and grsave, */
    /* record these in the scoreboard. */

    reg = rhdr->grsave;
    mask = rhdr->mask;
    if (mask & 0x8) {
	newrstate[SBREG_RP] = UWX_DISP_REG(UWX_REG_GR(reg));
	reg++;
    }
    if (mask & 0x4) {
	newrstate[SBREG_PFS] = UWX_DISP_REG(UWX_REG_GR(reg));
	reg++;
    }
    if (mask & 0x2) {
	newrstate[SBREG_PSP] = UWX_DISP_REG(UWX_REG_GR(reg));
	reg++;
    }
    if (mask & 0x1) {
	newrstate[SBREG_PREDS] = UWX_DISP_REG(UWX_REG_GR(reg));
	reg++;
    }

    /* Read prologue descriptor records until */
    /* we hit another region header. */

    for (;;) {

	b0 = uwx_get_byte(bstream);

	if (b0 < 0x80) {
	    /* Return the last byte read to the byte stream, since it's */
	    /* really the first byte of the next region header record. */
	    if (b0 >= 0)
		(void) uwx_unget_byte(bstream, b0);
	    break;
	}

	switch ((b0 & 0x70) >> 4) {

	    case 0:			/* 1000 xxxx */
	    case 1:			/* 1001 xxxx */
		/* Format P1 (br_mem) */
		TRACE_I_DECODE_PROLOGUE_1("(P1) br_mem", b0)
		br_mem_mask = b0 & 0x1f;
		break;

	    case 2:			/* 1010 xxxx */
		/* Format P2 (br_gr) */
		b1 = uwx_get_byte(bstream);
		if (b1 < 0)
		    return UWX_ERR_BADUDESC;
		TRACE_I_DECODE_PROLOGUE_2("(P2) br_gr", b0, b1)
		mask = ((b0 & 0x0f) << 1) | (b1 >> 7);
		reg = b1 & 0x7f;
		br_gr_mask = mask;
		for (i = 0; i < NSB_BR && mask != 0; i++) {
		    if (mask & 0x01) {
			newrstate[SBREG_BR + i] = UWX_DISP_REG(UWX_REG_GR(reg));
			reg++;
		    }
		    mask = mask >> 1;
		}
		break;

	    case 3:			/* 1011 xxxx */
		/* Format P3 */
		if (b0 < 0xb8) {
		    b1 = uwx_get_byte(bstream);
		    if (b1 < 0)
			return UWX_ERR_BADUDESC;
		    r = ((b0 & 0x7) << 1) | (b1 >> 7);
		    reg = b1 & 0x7f;
		    switch (r) {
			case 0:		/* psp_gr */
			    TRACE_I_DECODE_PROLOGUE_2("(P3) psp_gr", b0, b1)
			    newrstate[SBREG_PSP] = UWX_DISP_REG(UWX_REG_GR(reg));
			    break;
			case 1:		/* rp_gr */
			    TRACE_I_DECODE_PROLOGUE_2("(P3) rp_gr", b0, b1)
			    newrstate[SBREG_RP] = UWX_DISP_REG(UWX_REG_GR(reg));
			    break;
			case 2:		/* pfs_gr */
			    TRACE_I_DECODE_PROLOGUE_2("(P3) pfs_gr", b0, b1)
			    newrstate[SBREG_PFS] = UWX_DISP_REG(UWX_REG_GR(reg));
			    break;
			case 3:		/* preds_gr */
			    TRACE_I_DECODE_PROLOGUE_2("(P3) preds_gr", b0, b1)
			    newrstate[SBREG_PREDS] =
					UWX_DISP_REG(UWX_REG_GR(reg));
			    break;
			case 4:		/* unat_gr */
			    TRACE_I_DECODE_PROLOGUE_2("(P3) unat_gr", b0, b1)
			    newrstate[SBREG_UNAT] =
					UWX_DISP_REG(UWX_REG_GR(reg));
			    break;
			case 5:		/* lc_gr */
			    TRACE_I_DECODE_PROLOGUE_2("(P3) lc_gr", b0, b1)
			    newrstate[SBREG_LC] =
					UWX_DISP_REG(UWX_REG_GR(reg));
			    break;
			case 6:		/* rp_br */
			    TRACE_I_DECODE_PROLOGUE_2("(P3) rp_br", b0, b1)
			    scoreboard->rstate[SBREG_RP] =
					UWX_DISP_REG(UWX_REG_BR(reg));
			    break;
			case 7:		/* rnat_gr */
			    TRACE_I_DECODE_PROLOGUE_2("(P3) rnat_gr", b0, b1)
			    newrstate[SBREG_RNAT] =
					UWX_DISP_REG(UWX_REG_GR(reg));
			    break;
			case 8:		/* bsp_gr */
			    TRACE_I_DECODE_PROLOGUE_2("(P3) bsp_gr", b0, b1)
			    /* Don't track BSP yet */
			    return UWX_ERR_CANTUNWIND;
			    break;
			case 9:		/* bspstore_gr */
			    TRACE_I_DECODE_PROLOGUE_2("(P3) bspstore_gr", b0, b1)
			    /* Don't track BSPSTORE yet */
			    return UWX_ERR_CANTUNWIND;
			    break;
			case 10:	/* fpsr_gr */
			    TRACE_I_DECODE_PROLOGUE_2("(P3) fpsr_gr", b0, b1)
			    newrstate[SBREG_FPSR] =
					UWX_DISP_REG(UWX_REG_GR(reg));
			    break;
			case 11:	/* priunat_gr */
			    TRACE_I_DECODE_PROLOGUE_2("(P3) priunat_gr", b0, b1)
			    newrstate[SBREG_PRIUNAT] =
					UWX_DISP_REG(UWX_REG_GR(reg));
			    break;
			default:
			    TRACE_I_DECODE_PROLOGUE_2("(P3) ??", b0, b1)
			    return UWX_ERR_BADUDESC;
		    }
		}

		/* Format P4 (spill_mask) */
		else if (b0 == 0xb8) {
		    TRACE_I_DECODE_PROLOGUE_1("(P4) spill_mask", b0)
		    /* The spill_mask descriptor is followed by */
		    /* an imask field whose length is determined */
		    /* by the region length: there are two mask */
		    /* bits per instruction slot in the region. */
		    /* We decode these bits two at a time, counting */
		    /* the number of FRs, GRs, and BRs that are */
		    /* saved up to the slot of interest. Other */
		    /* descriptors describe which sets of these */
		    /* registers are spilled, and we put those */
		    /* two pieces of information together at the */
		    /* end of the main loop. */
		    t = 0;
		    while (t < rhdr->rlen) {
			b1 = uwx_get_byte(bstream);
			if (b1 < 0)
			    return UWX_ERR_BADUDESC;
			for (i = 0; i < 4 && (t + i) < ip_slot; i++) {
			    switch (b1 & 0xc0) {
				case 0x00: break;
				case 0x40: nfr++; break;
				case 0x80: ngr++; break;
				case 0xc0: nbr++; break;
			    }
			    b1 = b1 << 2;
			}
			t += 4;
		    }
		}

		/* Format P5 (frgr_mem) */
		else if (b0 == 0xb9) {
		    b1 = uwx_get_byte(bstream);
		    if (b1 < 0)
			return UWX_ERR_BADUDESC;
		    b2 = uwx_get_byte(bstream);
		    if (b2 < 0)
			return UWX_ERR_BADUDESC;
		    b3 = uwx_get_byte(bstream);
		    if (b3 < 0)
			return UWX_ERR_BADUDESC;
		    TRACE_I_DECODE_PROLOGUE_4("(P5) frgr_mem", b0, b1, b2, b3)
		    gr_mem_mask = b1 >> 4;
		    fr_mem_mask = ((b1 & 0x0f) << 16) | (b2 << 8) | b3;
		}

		/* Invalid descriptor record */
		else {
		    TRACE_I_DECODE_PROLOGUE_1("(?)", b0)
		    return UWX_ERR_BADUDESC;
		}

		break;

	    case 4:			/* 1100 xxxx */
		/* Format P6 (fr_mem) */
		TRACE_I_DECODE_PROLOGUE_1("(P6) fr_mem", b0)
		fr_mem_mask = b0 & 0x0f;
		break;

	    case 5:			/* 1101 xxxx */
		/* Format P6 (gr_mem) */
		TRACE_I_DECODE_PROLOGUE_1("(P6) gr_mem", b0)
		gr_mem_mask = b0 & 0x0f;
		break;

	    case 6:			/* 1110 xxxx */
		/* Format P7 */
		r = b0 & 0xf;
		status = uwx_get_uleb128(bstream, &parm1);
		if (status != 0)
		    return UWX_ERR_BADUDESC;
		switch (r) {
		    case 0:		/* mem_stack_f */
			status = uwx_get_uleb128(bstream, &parm2);
			if (status != 0)
			    return UWX_ERR_BADUDESC;
			TRACE_I_DECODE_PROLOGUE_1LL("(P7) mem_stack_f", b0, parm1, parm2)
			newrstate[SBREG_PSP] = UWX_DISP_SPPLUS(parm2 * 16);
			tspill[SBREG_PSP] = (int) parm1;
			break;
		    case 1:		/* mem_stack_v */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) mem_stack_v", b0, parm1)
			tspill[SBREG_PSP] = (int) parm1;
			break;
		    case 2:		/* spill_base */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) spill_base", b0, parm1)
			spill_base = 4 * (unsigned int) parm1;
			break;
		    case 3:		/* psp_sprel */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) psp_sprel", b0, parm1)
			newrstate[SBREG_PSP] = UWX_DISP_SPREL(parm1 * 4);
			break;
		    case 4:		/* rp_when */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) rp_when", b0, parm1)
			tspill[SBREG_RP] = (int) parm1;
			break;
		    case 5:		/* rp_psprel */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) rp_psprel", b0, parm1)
			newrstate[SBREG_RP] = UWX_DISP_PSPREL(parm1 * 4);
			break;
		    case 6:		/* pfs_when */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) pfs_when", b0, parm1)
			tspill[SBREG_PFS] = (int) parm1;
			break;
		    case 7:		/* pfs_psprel */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) pfs_psprel", b0, parm1)
			newrstate[SBREG_PFS] = UWX_DISP_PSPREL(parm1 * 4);
			break;
		    case 8:		/* preds_when */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) preds_when", b0, parm1)
			tspill[SBREG_PREDS] = (int) parm1;
			break;
		    case 9:		/* preds_psprel */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) preds_psprel", b0, parm1)
			newrstate[SBREG_PREDS] = UWX_DISP_PSPREL(parm1 * 4);
			break;
		    case 10:	/* lc_when */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) lc_when", b0, parm1)
			tspill[SBREG_LC] = (int) parm1;
			break;
		    case 11:	/* lc_psprel */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) lc_psprel", b0, parm1)
			newrstate[SBREG_LC] = UWX_DISP_PSPREL(parm1 * 4);
			break;
		    case 12:	/* unat_when */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) unat_when", b0, parm1)
			tspill[SBREG_UNAT] = (int) parm1;
			break;
		    case 13:	/* unat_psprel */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) unat_psprel", b0, parm1)
			newrstate[SBREG_UNAT] = UWX_DISP_PSPREL(parm1 * 4);
			break;
		    case 14:	/* fpsr_when */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) fpsr_when", b0, parm1)
			tspill[SBREG_FPSR] = (int) parm1;
			break;
		    case 15:	/* fpsr_psprel */
			TRACE_I_DECODE_PROLOGUE_1L("(P7) fpsr_psprel", b0, parm1)
			newrstate[SBREG_FPSR] = UWX_DISP_PSPREL(parm1 * 4);
			break;
		}
		break;

	    case 7:			/* 1111 xxxx */
		/* Format P8 */
		if (b0 == 0xf0) {
		    b1 = uwx_get_byte(bstream);
		    if (b1 < 0)
			return UWX_ERR_BADUDESC;
		    status = uwx_get_uleb128(bstream, &parm1);
		    if (status != 0)
			return UWX_ERR_BADUDESC;
		    switch (b1) {
			case 1:		/* rp_sprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) rp_sprel", b0, b1, parm1)
			    newrstate[SBREG_RP] = UWX_DISP_SPREL(parm1 * 4);
			    break;
			case 2:		/* pfs_sprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) pfs_sprel", b0, b1, parm1)
			    newrstate[SBREG_PFS] = UWX_DISP_SPREL(parm1 * 4);
			    break;
			case 3:		/* preds_sprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) preds_sprel", b0, b1, parm1)
			    newrstate[SBREG_PREDS] = UWX_DISP_SPREL(parm1 * 4);
			    break;
			case 4:		/* lc_sprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) lc_sprel", b0, b1, parm1)
			    newrstate[SBREG_LC] = UWX_DISP_SPREL(parm1 * 4);
			    break;
			case 5:		/* unat_sprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) unat_sprel", b0, b1, parm1)
			    newrstate[SBREG_UNAT] = UWX_DISP_SPREL(parm1 * 4);
			    break;
			case 6:		/* fpsr_sprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) fpsr_sprel", b0, b1, parm1)
			    newrstate[SBREG_FPSR] = UWX_DISP_SPREL(parm1 * 4);
			    break;
			case 7:		/* bsp_when */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) bsp_when", b0, b1, parm1)
			    /* Don't track BSP yet */
			    return UWX_ERR_CANTUNWIND;
			    break;
			case 8:		/* bsp_psprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) bsp_psprel", b0, b1, parm1)
			    /* Don't track BSP yet */
			    return UWX_ERR_CANTUNWIND;
			    break;
			case 9:		/* bsp_sprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) bsp_sprel", b0, b1, parm1)
			    /* Don't track BSP yet */
			    return UWX_ERR_CANTUNWIND;
			    break;
			case 10:	/* bspstore_when */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) bspstore_when", b0, b1, parm1)
			    /* Don't track BSP yet */
			    return UWX_ERR_CANTUNWIND;
			    break;
			case 11:	/* bspstore_psprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) bspstore_psprel", b0, b1, parm1)
			    /* Don't track BSP yet */
			    return UWX_ERR_CANTUNWIND;
			    break;
			case 12:	/* bspstore_sprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) bspstore_sprel", b0, b1, parm1)
			    /* Don't track BSP yet */
			    return UWX_ERR_CANTUNWIND;
			    break;
			case 13:	/* rnat_when */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) rnat_when", b0, b1, parm1)
			    tspill[SBREG_RNAT] = (int) parm1;
			    break;
			case 14:	/* rnat_psprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) rnat_psprel", b0, b1, parm1)
			    newrstate[SBREG_RNAT] = UWX_DISP_PSPREL(parm1 * 4);
			    break;
			case 15:	/* rnat_sprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) rnat_sprel", b0, b1, parm1)
			    newrstate[SBREG_RNAT] = UWX_DISP_SPREL(parm1 * 4);
			    break;
			case 16:	/* priunat_when_gr */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) priunat_when_gr", b0, b1, parm1)
			    tspill[SBREG_PRIUNAT] = (int) parm1;
			    break;
			case 17:	/* priunat_psprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) priunat_psprel", b0, b1, parm1)
			    priunat_mem_rstate = UWX_DISP_PSPREL(parm1 * 4);
			    break;
			case 18:	/* priunat_sprel */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) priunat_sprel", b0, b1, parm1)
			    priunat_mem_rstate = UWX_DISP_SPREL(parm1 * 4);
			    break;
			case 19:	/* priunat_when_mem */
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) priunat_when_mem", b0, b1, parm1)
			    t_priunat_mem = (int) parm1;
			    break;
			default:
			    TRACE_I_DECODE_PROLOGUE_2L("(P8) ??", b0, b1, parm1)
			    return UWX_ERR_BADUDESC;
		    }
		}

		/* Format P9 (gr_gr) */
		else if (b0 == 0xf1) {
		    b1 = uwx_get_byte(bstream);
		    if (b1 < 0)
			return UWX_ERR_BADUDESC;
		    b2 = uwx_get_byte(bstream);
		    if (b2 < 0)
			return UWX_ERR_BADUDESC;
		    TRACE_I_DECODE_PROLOGUE_3("(P9) gr_gr", b0, b1, b2)
		    mask = b1 & 0x0f;
		    reg = b2 & 0x7f;
		    gr_gr_mask = mask;
		    for (i = 0; i < NSB_GR && mask != 0; i++) {
			if (mask & 0x01) {
			    newrstate[SBREG_GR + i] =
					UWX_DISP_REG(UWX_REG_GR(reg));
			    reg++;
			}
			mask = mask >> 1;
		    }
		}

		/* Format X1 */
		else if (b0 == 0xf9) {
		    TRACE_I_DECODE_PROLOGUE_1("(X1)", b0)
		    b1 = uwx_get_byte(bstream);
		    if (b1 < 0)
			return UWX_ERR_BADUDESC;
		    /* Don't support X-format descriptors yet */
		    return UWX_ERR_CANTUNWIND;
		}

		/* Format X2 */
		else if (b0 == 0xfa) {
		    TRACE_I_DECODE_PROLOGUE_1("(X2)", b0)
		    b1 = uwx_get_byte(bstream);
		    if (b1 < 0)
			return UWX_ERR_BADUDESC;
		    b2 = uwx_get_byte(bstream);
		    if (b2 < 0)
			return UWX_ERR_BADUDESC;
		    /* Don't support X-format descriptors yet */
		    return UWX_ERR_CANTUNWIND;
		}

		/* Format X3 */
		else if (b0 == 0xfb) {
		    TRACE_I_DECODE_PROLOGUE_1("(X3)", b0)
		    b1 = uwx_get_byte(bstream);
		    if (b1 < 0)
			return UWX_ERR_BADUDESC;
		    b2 = uwx_get_byte(bstream);
		    if (b2 < 0)
			return UWX_ERR_BADUDESC;
		    /* Don't support X-format descriptors yet */
		    return UWX_ERR_CANTUNWIND;
		}

		/* Format X4 */
		else if (b0 == 0xfc) {
		    TRACE_I_DECODE_PROLOGUE_1("(X4)", b0)
		    b1 = uwx_get_byte(bstream);
		    if (b1 < 0)
			return UWX_ERR_BADUDESC;
		    b2 = uwx_get_byte(bstream);
		    if (b2 < 0)
			return UWX_ERR_BADUDESC;
		    b3 = uwx_get_byte(bstream);
		    if (b3 < 0)
			return UWX_ERR_BADUDESC;
		    /* Don't support X-format descriptors yet */
		    return UWX_ERR_CANTUNWIND;
		}

		/* Format P10 */
		else if (b0 == 0xff) {
		    b1 = uwx_get_byte(bstream);
		    if (b1 < 0)
			return UWX_ERR_BADUDESC;
		    b2 = uwx_get_byte(bstream);
		    if (b2 < 0)
			return UWX_ERR_BADUDESC;
		    TRACE_I_DECODE_PROLOGUE_3("(P10) abi", b0, b1, b2)
		    env->abi_context = (b1 << 8) | b2;
		    return UWX_ABI_FRAME;
		}

		/* Invalid descriptor record */
		else {
		    TRACE_I_DECODE_PROLOGUE_1("(?)", b0)
		    return UWX_ERR_BADUDESC;
		}
		break;
	}
    }

    /* Process the masks of spilled GRs, FRs, and BRs to */
    /* determine when and where each register was saved. */

    fr_base = spill_base + 16 * uwx_count_ones(fr_mem_mask);
    br_base = fr_base + 8 * uwx_count_ones(br_mem_mask);
    gr_base = br_base + 8 * uwx_count_ones(gr_mem_mask);
    TRACE_I_DECODE_PROLOGUE_SPILL_BASE(spill_base)
    TRACE_I_DECODE_PROLOGUE_MASKS(gr_mem_mask, gr_gr_mask)
    TRACE_I_DECODE_PROLOGUE_NSPILL(ngr)
    for (i = 0; ngr > 0 && i <= NSB_GR; i++) {
	if (gr_mem_mask & 1) {
	    newrstate[SBREG_GR + i] = UWX_DISP_PSPREL(gr_base);
	    tspill[SBREG_GR + i] = 0;
	    gr_base -= 8;
	    ngr--;
	}
	else if (gr_gr_mask & 1) {
	    tspill[SBREG_GR + i] = 0;
	    ngr--;
	}
	gr_gr_mask = gr_gr_mask >> 1;
	gr_mem_mask = gr_mem_mask >> 1;
    }
    for (i = 0; nbr > 0 && i <= NSB_BR; i++) {
	if (br_mem_mask & 1) {
	    newrstate[SBREG_BR + i] = UWX_DISP_PSPREL(br_base);
	    tspill[SBREG_BR + i] = 0;
	    br_base -= 8;
	    nbr--;
	}
	else if (br_gr_mask & 1) {
	    tspill[SBREG_BR + i] = 0;
	    nbr--;
	}
	br_gr_mask = br_gr_mask >> 1;
	br_mem_mask = br_mem_mask >> 1;
    }
    for (i = 0; nfr > 0 && i <= NSB_FR; i++) {
	if (fr_mem_mask & 1) {
	    newrstate[SBREG_FR + i] = UWX_DISP_PSPREL(fr_base);
	    tspill[SBREG_FR + i] = 0;
	    fr_base -= 16;
	    nfr--;
	}
	fr_mem_mask = fr_mem_mask >> 1;
    }

    /* Update the scoreboard. */

    for (i = 0; i < env->nsbreg; i++) {
	if (ip_slot >= rhdr->rlen || ip_slot > tspill[i])
	    scoreboard->rstate[i] = newrstate[i];
    }
    if (priunat_mem_rstate != UWX_DISP_NONE && ip_slot > t_priunat_mem)
	scoreboard->rstate[SBREG_PRIUNAT] = priunat_mem_rstate;

    return UWX_OK;
}

int uwx_count_ones(unsigned int mask)
{
    mask = (mask & 0x55555555) + ((mask & 0xaaaaaaaa) >> 1);
    mask = (mask & 0x33333333) + ((mask & 0xcccccccc) >> 2);
    mask = (mask & 0x0f0f0f0f) + ((mask & 0xf0f0f0f0) >> 4);
    mask = (mask & 0x00ff00ff) + ((mask & 0xff00ff00) >> 8);
    return (mask & 0x0000ffff) + ((mask & 0xffff0000) >> 16);
}

/* uwx_decode_body: Decodes a body region */

int uwx_decode_body(
    struct uwx_env *env,
    struct uwx_bstream *bstream,
    struct uwx_scoreboard *scoreboard,
    struct uwx_rhdr *rhdr,
    int ip_slot)
{
    int status;
    int b0;
    int b1;
    int b2;
    int b3;
    int label;
    int ecount;
    int i;
    uint64_t parm1;
    uint64_t parm2;
    uint64_t newrstate[NSBREG];
    int tspill[NSBREG];
    int t_sp_restore;

    /* Initialize an array of register states from the current */
    /* scoreboard, along with a parallel array of spill times. */
    /* We use this as a temporary scoreboard, then update the */
    /* real scoreboard at the end of the procedure. */
    /* We initialize the spill time to (rhdr.rlen - 1) so that */
    /* spills without a "when" descriptor will take effect */
    /* at the end of the prologue region. */
    /* (Boundary condition: all actions in a zero-length prologue */
    /* will appear to have happened in the instruction slot */
    /* immediately preceding the prologue.) */

    for (i = 0; i < env->nsbreg; i++) {
	newrstate[i] = scoreboard->rstate[i];
	tspill[i] = rhdr->rlen - 1;
    }
    t_sp_restore = rhdr->rlen - 1;

    /* Read body descriptor records until */
    /* we hit another region header. */

    for (;;) {

	b0 = uwx_get_byte(bstream);

	if (b0 < 0x80) {
	    /* Return the last byte read to the byte stream, since it's */
	    /* really the first byte of the next region header record. */
	    if (b0 >= 0)
		(void) uwx_unget_byte(bstream, b0);
	    break;
	}

	/* Format B1 (label_state) */
	if (b0 < 0xa0) {
	    TRACE_I_DECODE_BODY_1("(B1) label_state", b0)
	    label = b0 & 0x1f;
	    status = uwx_label_scoreboard(env, scoreboard, label);
	    if (status != UWX_OK)
		return (status);
	}

	/* Format B1 (copy_state)  */
	else if (b0 < 0xc0) {
	    TRACE_I_DECODE_BODY_1("(B1) copy_state", b0)
	    label = b0 & 0x1f;
	    status = uwx_copy_scoreboard(env, scoreboard, label);
	    if (status != UWX_OK)
		return (status);
	    for (i = 0; i < env->nsbreg; i++) {
		newrstate[i] = scoreboard->rstate[i];
		tspill[i] = rhdr->rlen;
	    }
	}

	/* Format B2 (epilogue) */
	else if (b0 < 0xe0) {
	    ecount = b0 & 0x1f;
	    status = uwx_get_uleb128(bstream, &parm1);
	    if (status != 0)
		return UWX_ERR_BADUDESC;
	    TRACE_I_DECODE_BODY_1L("(B2) epilogue", b0, parm1)
	    rhdr->ecount = ecount + 1;
	    t_sp_restore = rhdr->rlen - (unsigned int) parm1;
	}

	/* Format B3 (epilogue) */
	else if (b0 == 0xe0) {
	    status = uwx_get_uleb128(bstream, &parm1);
	    if (status != 0)
		return UWX_ERR_BADUDESC;
	    status = uwx_get_uleb128(bstream, &parm2);
	    if (status != 0)
		return UWX_ERR_BADUDESC;
	    TRACE_I_DECODE_BODY_1LL("(B3) epilogue", b0, parm1, parm2)
	    t_sp_restore = rhdr->rlen - (unsigned int) parm1;
	    rhdr->ecount = (unsigned int) parm2 + 1;
	}

	/* Format B4 (label_state) */
	else if (b0 == 0xf0) {
	    status = uwx_get_uleb128(bstream, &parm1);
	    if (status != 0)
		return UWX_ERR_BADUDESC;
	    TRACE_I_DECODE_BODY_1L("(B4) label_state", b0, parm1)
	    label = (int) parm1;
	    status = uwx_label_scoreboard(env, scoreboard, label);
	    if (status != UWX_OK)
		return (status);
	}

	/* Format B4 (copy_state) */
	else if (b0 == 0xf8) {
	    status = uwx_get_uleb128(bstream, &parm1);
	    if (status != 0)
		return UWX_ERR_BADUDESC;
	    TRACE_I_DECODE_BODY_1L("(B4) copy_state", b0, parm1)
	    label = (int) parm1;
	    status = uwx_copy_scoreboard(env, scoreboard, label);
	    if (status != UWX_OK)
		return (status);
	    for (i = 0; i < env->nsbreg; i++) {
		newrstate[i] = scoreboard->rstate[i];
		tspill[i] = rhdr->rlen;
	    }
	}

	/* Format X1 */
	else if (b0 == 0xf9) {
	    TRACE_I_DECODE_BODY_1("(X1)", b0)
	    b1 = uwx_get_byte(bstream);
	    if (b1 < 0)
		return UWX_ERR_BADUDESC;
	    /* Don't support X-format descriptors yet */
	    return UWX_ERR_CANTUNWIND;
	}

	/* Format X2 */
	else if (b0 == 0xfa) {
	    TRACE_I_DECODE_BODY_1("(X2)", b0)
	    b1 = uwx_get_byte(bstream);
	    if (b1 < 0)
		return UWX_ERR_BADUDESC;
	    b2 = uwx_get_byte(bstream);
	    if (b2 < 0)
		return UWX_ERR_BADUDESC;
	    /* Don't support X-format descriptors yet */
	    return UWX_ERR_CANTUNWIND;
	}

	/* Format X3 */
	else if (b0 == 0xfb) {
	    TRACE_I_DECODE_BODY_1("(X3)", b0)
	    b1 = uwx_get_byte(bstream);
	    if (b1 < 0)
		return UWX_ERR_BADUDESC;
	    b2 = uwx_get_byte(bstream);
	    if (b2 < 0)
		return UWX_ERR_BADUDESC;
	    /* Don't support X-format descriptors yet */
	    return UWX_ERR_CANTUNWIND;
	}

	/* Format X4 */
	else if (b0 == 0xfc) {
	    TRACE_I_DECODE_BODY_1("(X4)", b0)
	    b1 = uwx_get_byte(bstream);
	    if (b1 < 0)
		return UWX_ERR_BADUDESC;
	    b2 = uwx_get_byte(bstream);
	    if (b2 < 0)
		return UWX_ERR_BADUDESC;
	    b3 = uwx_get_byte(bstream);
	    if (b3 < 0)
		return UWX_ERR_BADUDESC;
	    /* Don't support X-format descriptors yet */
	    return UWX_ERR_CANTUNWIND;
	}

	/* Invalid descriptor record */
	else {
	    TRACE_I_DECODE_BODY_1("(?)", b0)
	    return UWX_ERR_BADUDESC;
	}
    }

    /* Update the scoreboard. */

    for (i = 0; i < env->nsbreg; i++) {
	if (ip_slot > tspill[i])
	    scoreboard->rstate[i] = newrstate[i];
    }

    /* If we've passed the point in the epilogue where sp */
    /* is restored, update the scoreboard entry for PSP */
    /* and reset any entries for registers saved in memory. */

    if (ip_slot > t_sp_restore) {
	scoreboard->rstate[SBREG_PSP] = UWX_DISP_SPPLUS(0);
	for (i = 0; i < env->nsbreg; i++) {
	    if (UWX_GET_DISP_CODE(scoreboard->rstate[i]) == UWX_DISP_SPREL(0) ||
		UWX_GET_DISP_CODE(scoreboard->rstate[i]) == UWX_DISP_PSPREL(0))
		scoreboard->rstate[i] = UWX_DISP_NONE;
	}
    }

    return UWX_OK;
}

