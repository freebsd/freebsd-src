/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_util.c - layer 2 utility routines
 *	-------------------------------------
 *
 *	$Id: i4b_util.c,v 1.22 1999/12/13 21:25:27 hm Exp $ 
 *
 * $FreeBSD: src/sys/i4b/layer2/i4b_util.c,v 1.6 1999/12/14 20:48:29 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 22:04:37 1999]
 *
 *---------------------------------------------------------------------------*/

#ifdef __FreeBSD__
#include "i4bq921.h"
#else
#define	NI4BQ921	1
#endif
#if NI4BQ921 > 0

#include <sys/param.h>

#if defined(__FreeBSD__)
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif

#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#endif

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_l2l3.h>
#include <i4b/include/i4b_isdnq931.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer2/i4b_l2.h>
#include <i4b/layer2/i4b_l2fsm.h>

/*---------------------------------------------------------------------------*
 *	routine ESTABLISH DATA LINK (Q.921 03/93 page 83)
 *---------------------------------------------------------------------------*/
void
i4b_establish_data_link(l2_softc_t *l2sc)
{
	i4b_l1_activate(l2sc);	

	i4b_clear_exception_conditions(l2sc);

	l2sc->RC = 0;

	i4b_tx_sabme(l2sc, P1);

	i4b_T200_restart(l2sc);

	i4b_T203_stop(l2sc);	
}

/*---------------------------------------------------------------------------*
 *	routine CLEAR EXCEPTION CONDITIONS (Q.921 03/93 page 83)
 *---------------------------------------------------------------------------*/
void
i4b_clear_exception_conditions(l2_softc_t *l2sc)
{
	CRIT_VAR;

	CRIT_BEG;
	
/*XXX -------------------------------------------------------------- */
/*XXX is this really appropriate here or should it moved elsewhere ? */

	i4b_Dcleanifq(&l2sc->i_queue);
	
	if(l2sc->ua_num != UA_EMPTY)
	{
		i4b_Dfreembuf(l2sc->ua_frame);
		l2sc->ua_num = UA_EMPTY;
	}
/*XXX -------------------------------------------------------------- */

	l2sc->peer_busy = 0;

	l2sc->rej_excpt = 0;

	l2sc->own_busy = 0;

	l2sc->ack_pend = 0;	

	CRIT_END;	
}

/*---------------------------------------------------------------------------*
 *	routine TRANSMIT ENQUIRE (Q.921 03/93 page 83)
 *---------------------------------------------------------------------------*/
void
i4b_transmit_enquire(l2_softc_t *l2sc)
{
	if(l2sc->own_busy)
		i4b_tx_rnr_command(l2sc, P1);
	else
		i4b_tx_rr_command(l2sc, P1);

	l2sc->ack_pend = 0;

	i4b_T200_start(l2sc);
}

/*---------------------------------------------------------------------------*
 *	routine NR ERROR RECOVERY (Q.921 03/93 page 83)
 *---------------------------------------------------------------------------*/
void
i4b_nr_error_recovery(l2_softc_t *l2sc)
{
	i4b_mdl_error_ind(l2sc, "i4b_nr_error_recovery", MDL_ERR_J);

	i4b_establish_data_link(l2sc);
	
	l2sc->l3initiated = 0;
}

/*---------------------------------------------------------------------------*
 *	routine ENQUIRY RESPONSE (Q.921 03/93 page 84)
 *---------------------------------------------------------------------------*/
void
i4b_enquiry_response(l2_softc_t *l2sc)
{
	if(l2sc->own_busy)
		i4b_tx_rnr_response(l2sc, F1);
	else
		i4b_tx_rr_response(l2sc, F1);

	l2sc->ack_pend = 0;
}

/*---------------------------------------------------------------------------*
 *	routine INVOKE RETRANSMISSION (Q.921 03/93 page 84)
 *---------------------------------------------------------------------------*/
void
i4b_invoke_retransmission(l2_softc_t *l2sc, int nr)
{
	CRIT_VAR;

	CRIT_BEG;

	DBGL2(L2_ERROR, "i4b_invoke_retransmission", ("nr = %d\n", nr ));
	
	while(l2sc->vs != nr)
	{
		DBGL2(L2_ERROR, "i4b_invoke_retransmission", ("nr(%d) != vs(%d)\n", nr, l2sc->vs));

		M128DEC(l2sc->vs);

/* XXXXXXXXXXXXXXXXX */

		if((l2sc->ua_num != UA_EMPTY) && (l2sc->vs == l2sc->ua_num))
		{
			if(IF_QFULL(&l2sc->i_queue))
			{
				DBGL2(L2_ERROR, "i4b_invoke_retransmission", ("ERROR, I-queue full!\n"));
			}
			else
			{
				IF_ENQUEUE(&l2sc->i_queue, l2sc->ua_frame);
				l2sc->ua_num = UA_EMPTY;
			}
		}
		else
		{
			DBGL2(L2_ERROR, "i4b_invoke_retransmission", ("ERROR, l2sc->vs = %d, l2sc->ua_num = %d \n",l2sc->vs, l2sc->ua_num));
		}

/* XXXXXXXXXXXXXXXXX */
			
		i4b_i_frame_queued_up(l2sc);
	}

	CRIT_END;
}

/*---------------------------------------------------------------------------*
 *	routine ACKNOWLEDGE PENDING (Q.921 03/93 p 70)
 *---------------------------------------------------------------------------*/
void
i4b_acknowledge_pending(l2_softc_t *l2sc)
{
	if(l2sc->ack_pend)
	{
		l2sc->ack_pend = 0;
		i4b_tx_rr_response(l2sc, F0);
	}
}

/*---------------------------------------------------------------------------*
 *	i4b_print_frame - just print the hex contents of a frame
 *---------------------------------------------------------------------------*/
void
i4b_print_frame(int len, u_char *buf)
{
#ifdef DO_I4B_DEBUG
	int i;

	if (!(i4b_l2_debug & L2_ERROR))		/* XXXXXXXXXXXXXXXXXXXXX */
		return;

	for(i = 0; i < len; i++)
		printf(" 0x%x", buf[i]);
	printf("\n");
#endif
}

/*---------------------------------------------------------------------------*
 *	i4b_print_l2var - print some l2softc vars
 *---------------------------------------------------------------------------*/
void
i4b_print_l2var(l2_softc_t *l2sc)
{
	DBGL2(L2_ERROR, "i4b_print_l2var", ("unit%d V(R)=%d, V(S)=%d, V(A)=%d,ACKP=%d,PBSY=%d,OBSY=%d\n",
		l2sc->unit,
		l2sc->vr,
		l2sc->vs,
		l2sc->va,
		l2sc->ack_pend,
		l2sc->peer_busy,
		l2sc->own_busy));
}

/*---------------------------------------------------------------------------*
 *	got s or i frame, check if valid ack for last sent frame
 *---------------------------------------------------------------------------*/
void
i4b_rxd_ack(l2_softc_t *l2sc, int nr)
{

#ifdef NOTDEF
	DBGL2(L2_ERROR, "i4b_rxd_ack", ("N(R)=%d, UA=%d, V(R)=%d, V(S)=%d, V(A)=%d\n",
		nr,
		l2sc->ua_num,
		l2sc->vr,
		l2sc->vs,
		l2sc->va));
#endif

	if(l2sc->ua_num != UA_EMPTY)
	{
		CRIT_VAR;

		CRIT_BEG;
		
		M128DEC(nr);

		if(l2sc->ua_num != nr)
			DBGL2(L2_ERROR, "i4b_rxd_ack", ("((N(R)-1)=%d) != (UA=%d) !!!\n", nr, l2sc->ua_num));
			
		i4b_Dfreembuf(l2sc->ua_frame);
		l2sc->ua_num = UA_EMPTY;
		
		CRIT_END;
	}
}

/*---------------------------------------------------------------------------*
 *	if not already active, activate layer 1
 *---------------------------------------------------------------------------*/
void
i4b_l1_activate(l2_softc_t *l2sc)
{
	if(l2sc->ph_active == PH_INACTIVE)
	{
		l2sc->ph_active = PH_ACTIVEPEND;
		PH_Act_Req(l2sc->unit);
	}
};

/*---------------------------------------------------------------------------*
 *	check for v(a) <= n(r) <= v(s)
 *	nr = receive sequence frame counter, va = acknowledge sequence frame
 *	counter and vs = transmit sequence frame counter
 *---------------------------------------------------------------------------*/
int
i4b_l2_nr_ok(int nr, int va, int vs)
{
	if((va > nr) && ((nr != 0) || (va != 127)))
	{
		DBGL2(L2_ERROR, "i4b_l2_nr_ok", ("ERROR, va = %d, nr = %d, vs = %d [1]\n", va, nr, vs));
		return 0;	/* fail */
	}

	if((nr > vs) && ((vs != 0) || (nr != 127)))
	{
		DBGL2(L2_ERROR, "i4b_l2_nr_ok", ("ERROR, va = %d, nr = %d, vs = %d [2]\n", va, nr, vs));
		return 0;	/* fail */
	}
	return 1;		/* good */
}
	
#endif /* NI4BQ921 > 0 */

