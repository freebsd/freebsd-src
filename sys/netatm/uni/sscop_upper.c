/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * ATM Forum UNI Support
 * ---------------------
 *
 * SSCOP - CPCS SAP interface processing
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <netatm/uni/sscop.h>
#include <netatm/uni/sscop_misc.h>
#include <netatm/uni/sscop_pdu.h>
#include <netatm/uni/sscop_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local functions
 */
static caddr_t	sscop_pdu_receive(KBuffer *, struct sscop *, int *);


/*
 * Local variables
 */
static union {
	struct bgn_pdu		t_bgn;
	struct bgak_pdu		t_bgak;
	struct end_pdu		t_end;
	struct endak_q2110_pdu	t_endak_q2110;
	struct endak_qsaal_pdu	t_endak_qsaal;
	struct rs_pdu		t_rs;
	struct rsak_q2110_pdu	t_rsak_q2110;
	struct rsak_qsaal_pdu	t_rsak_qsaal;
	struct bgrej_pdu	t_bgrej;
	struct sd_pdu		t_sd;
	struct sdp_pdu		t_sdp;
	struct er_pdu		t_er;
	struct poll_pdu		t_poll;
	struct stat_pdu		t_stat;
	struct ustat_pdu	t_ustat;
	struct ud_pdu		t_ud;
	struct md_pdu		t_md;
	struct erak_pdu		t_erak;
} sscop_trailer;


/*
 * PDU length validation table
 */
struct pdulen {
	int	min;
	int	max;
};

static struct pdulen qsaal_pdulen[] = {
	{0,				0},
	{sizeof(struct bgn_pdu),	sizeof(struct bgn_pdu)},
	{sizeof(struct bgak_pdu),	sizeof(struct bgak_pdu)},
	{sizeof(struct end_pdu),	sizeof(struct end_pdu)},
	{sizeof(struct endak_qsaal_pdu),sizeof(struct endak_qsaal_pdu)},
	{sizeof(struct rs_pdu),		sizeof(struct rs_pdu)},
	{sizeof(struct rsak_qsaal_pdu),	sizeof(struct rsak_qsaal_pdu)},
	{sizeof(struct bgrej_pdu),	sizeof(struct bgrej_pdu)},
	{sizeof(struct sd_pdu),		sizeof(struct sd_pdu) + PDU_MAX_INFO},
	{sizeof(struct sdp_pdu),	sizeof(struct sdp_pdu) + PDU_MAX_INFO},
	{sizeof(struct poll_pdu),	sizeof(struct poll_pdu)},
	{sizeof(struct stat_pdu),	sizeof(struct stat_pdu) + PDU_MAX_STAT},
	{sizeof(struct ustat_pdu),	sizeof(struct ustat_pdu)},
	{sizeof(struct ud_pdu),		sizeof(struct ud_pdu) + PDU_MAX_INFO},
	{sizeof(struct md_pdu),		sizeof(struct md_pdu) + PDU_MAX_INFO},
	{0,				0}
};

static struct pdulen q2110_pdulen[] = {
	{0,				0},
	{sizeof(struct bgn_pdu),	sizeof(struct bgn_pdu) + PDU_MAX_UU},
	{sizeof(struct bgak_pdu),	sizeof(struct bgak_pdu) + PDU_MAX_UU},
	{sizeof(struct end_pdu),	sizeof(struct end_pdu) + PDU_MAX_UU},
	{sizeof(struct endak_q2110_pdu),sizeof(struct endak_q2110_pdu)},
	{sizeof(struct rs_pdu),		sizeof(struct rs_pdu) + PDU_MAX_UU},
	{sizeof(struct rsak_q2110_pdu),	sizeof(struct rsak_q2110_pdu)},
	{sizeof(struct bgrej_pdu),	sizeof(struct bgrej_pdu) + PDU_MAX_UU},
	{sizeof(struct sd_pdu),		sizeof(struct sd_pdu) + PDU_MAX_INFO},
	{sizeof(struct er_pdu),		sizeof(struct er_pdu)},
	{sizeof(struct poll_pdu),	sizeof(struct poll_pdu)},
	{sizeof(struct stat_pdu),	sizeof(struct stat_pdu) + PDU_MAX_STAT},
	{sizeof(struct ustat_pdu),	sizeof(struct ustat_pdu)},
	{sizeof(struct ud_pdu),		sizeof(struct ud_pdu) + PDU_MAX_INFO},
	{sizeof(struct md_pdu),		sizeof(struct md_pdu) + PDU_MAX_INFO},
	{sizeof(struct erak_pdu),	sizeof(struct erak_pdu)}
};


/*
 * PDUs with Pad Length Fields
 */
static u_char qsaal_padlen[] = {
	0,			/* --- */
	0,			/* BGN */
	0,			/* BGAK */
	0,			/* END */
	0,			/* ENDAK */
	0,			/* RS */
	0,			/* RSAK */
	0,			/* BGREJ */
	1,			/* SD */
	1,			/* SDP */
	0,			/* POLL */
	0,			/* STAT */
	0,			/* USTAT */
	1,			/* UD */
	1,			/* MD */
	0			/* --- */
};

static u_char q2110_padlen[] = {
	0,			/* --- */
	1,			/* BGN */
	1,			/* BGAK */
	1,			/* END */
	0,			/* ENDAK */
	1,			/* RS */
	0,			/* RSAK */
	1,			/* BGREJ */
	1,			/* SD */
	0,			/* ER */
	0,			/* POLL */
	0,			/* STAT */
	0,			/* USTAT */
	1,			/* UD */
	1,			/* MD */
	0			/* ERAK */
};


/*
 * SSCOP Upper Stack Command Handler
 * 
 * This function will receive all of the stack commands issued from the 
 * layer below SSCOP (ie. CPCS).  Currently, only incoming PDUs will be
 * received here.  The appropriate processing function will be determined 
 * based on the received PDU type and the current sscop control block state.
 *
 * Arguments:
 *	cmd	stack command code
 *	tok	session token
 *	arg1	command specific argument
 *	arg2	command specific argument
 *
 * Returns:
 *	none
 *
 */
void
sscop_upper(cmd, tok, arg1, arg2)
	int	cmd;
	void	*tok;
	intptr_t	arg1;
	intptr_t	arg2;
{
	struct sscop	*sop = (struct sscop *)tok;
	void		(**ptab)(struct sscop *, KBuffer *, caddr_t);
	void		(*func)(struct sscop *, KBuffer *, caddr_t);
	caddr_t		trlr;
	int		type;

	ATM_DEBUG5("sscop_upper: cmd=0x%x, sop=%p, state=%d, arg1=%p, arg2=%p\n",
		cmd, sop, sop->so_state, (void *)arg1, (void *)arg2);

	switch (cmd) {

	case CPCS_UNITDATA_SIG:
		/*
		 * Decode/validate received PDU
		 */
		trlr = sscop_pdu_receive((KBuffer *)arg1, sop, &type);
		if (trlr == NULL) {
			return;
		}

		/*
		 * Validate sscop state
		 */
		if (sop->so_state > SOS_MAXSTATE) {
			log(LOG_ERR, 
				"sscop_upper: invalid state sop=%p, state=%d\n",
				sop, sop->so_state);
			KB_FREEALL((KBuffer *)arg1);
			return;
		}

		/*
		 * Call event processing function
		 */
		ptab = sop->so_vers == SSCOP_VERS_QSAAL ?
				sscop_qsaal_pdutab[type]:
				sscop_q2110_pdutab[type];
		func = ptab[sop->so_state];
		if (func == NULL) {
			log(LOG_ERR, 
				"sscop_upper: unsupported pdu=%d, state=%d\n",
				type, sop->so_state);
			break;
		}
		(*func)(sop, (KBuffer *)arg1, trlr);
		break;

	default:
		log(LOG_ERR, "sscop_upper: unknown cmd 0x%x, sop=%p\n",
			cmd, sop);
	}

	return;
}


/*
 * Decode and Validate Received PDU
 * 
 * This function will process all received SSCOP PDUs.  The PDU type will be
 * determined and PDU format validation will be performed.  If the PDU is
 * successfully decoded and validated, the buffer chain will have the PDU
 * trailer removed, but any resultant zero-length buffers will NOT be freed.  
 * If the PDU fails validation, then the buffer chain will be freed.
 *
 * Arguments:
 *	m	pointer to PDU buffer chain
 *	sop	pointer to sscop connection block
 *	typep	address to store PDU type
 *
 * Returns:
 *	addr	pointer to (contiguous) PDU trailer
 *	0	invalid PDU, buffer chain freed
 *
 */
static caddr_t
sscop_pdu_receive(m, sop, typep)
	KBuffer		*m;
	struct sscop	*sop;
	int		*typep;
{
	KBuffer		*m0, *ml, *mn;
	caddr_t		cp, tp;
	int		len, tlen, type, plen;

	/*
	 * Calculate PDU length and find the last two buffers in the chain
	 */
	len = 0;
	for (m0 = m, ml = mn = NULL; m0; m0 = KB_NEXT(m0)) {
		len += KB_LEN(m0);
		mn = ml;
		ml = m0;
	}

	/*
	 * Make sure we've got a minimum sized PDU
	 */
	if (len < PDU_MIN_LEN)
		goto badpdu;

	/*
	 * Get PDU type field
	 */
	if (KB_LEN(ml) >= PDU_MIN_LEN) {
		KB_DATAEND(ml, tp, caddr_t);
		tp -= PDU_MIN_LEN;
	} else {
		KB_DATAEND(mn, tp, caddr_t);
		tp -= (PDU_MIN_LEN - KB_LEN(ml));
	}
	*typep = type = *tp & PT_TYPE_MASK;

	/*
	 * Check up on PDU length
	 */
	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		if ((len < (tlen = qsaal_pdulen[type].min)) ||
		    (len > qsaal_pdulen[type].max) ||
		    (len & PDU_LEN_MASK))
			goto badpdu;
	} else {
		if ((len < (tlen = q2110_pdulen[type].min)) ||
		    (len > q2110_pdulen[type].max) ||
		    (len & PDU_LEN_MASK))
			goto badpdu;
	}

	/*
	 * Get a contiguous, aligned PDU trailer and adjust buffer
	 * controls to remove trailer
	 */
	if (KB_LEN(ml) >= tlen) {
		/*
		 * Trailer is contained in last buffer
		 */
		KB_TAILADJ(ml, -tlen);
		KB_DATAEND(ml, cp, caddr_t);
		if ((intptr_t)cp & PDU_ADDR_MASK) {
			/*
			 * Trailer not aligned in buffer, use local memory
			 */
			bcopy(cp, (caddr_t)&sscop_trailer, tlen);
			cp = (caddr_t)&sscop_trailer;
		}
	} else {
		/*
		 * Trailer is split across buffers, use local memory
		 */
		caddr_t	cp1;
		int	off = tlen - KB_LEN(ml);

		cp = (caddr_t)&sscop_trailer;

		/*
		 * Ensure trailer is within last two buffers
		 */
		if ((mn == NULL) || (KB_LEN(mn) < off))
			goto badpdu;

		KB_DATASTART(ml, cp1, caddr_t);
		bcopy(cp1, cp + off, KB_LEN(ml));
		KB_LEN(ml) = 0;
		KB_TAILADJ(mn, -off);
		KB_DATAEND(mn, cp1, caddr_t);
		bcopy(cp1, cp, off);
	}

	/*
	 * Get possible PDU Pad Length
	 */
	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		if (qsaal_padlen[type])
			plen = (*tp & PT_PAD_MASK) >> PT_PAD_SHIFT;
		else
			plen = 0;
	} else {
		if (q2110_padlen[type])
			plen = (*tp & PT_PAD_MASK) >> PT_PAD_SHIFT;
		else
			plen = 0;
	}

	/*
	 * Perform Pad Length adjustments 
	 */
	if (plen) {
		if (KB_LEN(ml) >= plen) {
			/*
			 * All pad bytes in last buffer
			 */
			KB_TAILADJ(ml, -plen);
		} else {
			/*
			 * Pad bytes split between buffers
			 */
			plen -= KB_LEN(ml);
			if ((mn == NULL) || (KB_LEN(mn) < plen))
				goto badpdu;
			KB_LEN(ml) = 0;
			KB_TAILADJ(mn, -plen);
		}
	}

	return (cp);

badpdu:
	/*
	 * This MAA Error is only supposed to be for a PDU length violation,
	 * but we use it for any PDU format error.
	 */
	sscop_maa_error(sop, 'U');
	sscop_pdu_print(sop, m, "badpdu received");
	KB_FREEALL(m);
	return (NULL);
}

