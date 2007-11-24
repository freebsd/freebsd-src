/*-
 * Copyright (c) 1997, 2002 Hellmuth Michaelis. All rights reserved.
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

/*---------------------------------------------------------------------------
 *
 *      i4b_l2.c - ISDN layer 2 (Q.921)
 *	-------------------------------
 *      last edit-date: [Sat Mar  9 16:11:14 2002]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#include <machine/i4b_debug.h>

#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_l2l3.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_global.h>

#include <i4b/layer2/i4b_l2.h>
#include <i4b/layer2/i4b_l2fsm.h>

int i4b_dl_establish_ind(int);
int i4b_dl_establish_cnf(int);
int i4b_dl_release_ind(int);
int i4b_dl_release_cnf(int);
int i4b_dl_data_ind(int, struct mbuf *);
int i4b_dl_unit_data_ind(int, struct mbuf *);

static int i4b_mdl_command_req(int, int, void *);

/* from layer 2 */

extern int i4b_mdl_status_ind(int, int, int);

/* this layers debug level */

unsigned int i4b_l2_debug = L2_DEBUG_DEFAULT;

struct i4b_l2l3_func i4b_l2l3_func = {

	/* Layer 2 --> Layer 3 */
	
	(int (*)(int))				i4b_dl_establish_ind,
	(int (*)(int)) 				i4b_dl_establish_cnf,
	(int (*)(int))				i4b_dl_release_ind,
	(int (*)(int))				i4b_dl_release_cnf,
	(int (*)(int, struct mbuf *))		i4b_dl_data_ind,
	(int (*)(int, struct mbuf *))		i4b_dl_unit_data_ind,

	/* Layer 3 --> Layer 2 */

	(int (*)(int))				i4b_dl_establish_req,
	(int (*)(int))				i4b_dl_release_req,
	(int (*)(int, struct mbuf *))		i4b_dl_data_req,
	(int (*)(int, struct mbuf *))		i4b_dl_unit_data_req,

	/* Layer 2 --> Layer 3 management */
	
	(int (*)(int, int, int))		i4b_mdl_status_ind,

	/* Layer 3  --> Layer 2 management */
	
	(int (*)(int, int, void *))		i4b_mdl_command_req	
};

/*---------------------------------------------------------------------------*
 *	DL_ESTABLISH_REQ from layer 3
 *---------------------------------------------------------------------------*/
int i4b_dl_establish_req(int unit)
{
	l2_softc_t *l2sc = &l2_softc[unit];
	
	NDBGL2(L2_PRIM, "unit %d",unit);
	i4b_l1_activate(l2sc);
	i4b_next_l2state(l2sc, EV_DLESTRQ);
	return(0);
}

/*---------------------------------------------------------------------------*
 *	DL_RELEASE_REQ from layer 3
 *---------------------------------------------------------------------------*/
int i4b_dl_release_req(int unit)
{
	l2_softc_t *l2sc = &l2_softc[unit];

	NDBGL2(L2_PRIM, "unit %d",unit);	
	i4b_next_l2state(l2sc, EV_DLRELRQ);
	return(0);	
}

/*---------------------------------------------------------------------------*
 *	DL UNIT DATA REQUEST from Layer 3
 *---------------------------------------------------------------------------*/
int i4b_dl_unit_data_req(int unit, struct mbuf *m)
{
#ifdef NOTDEF
	NDBGL2(L2_PRIM, "unit %d",unit);
#endif
	return(0);
}

/*---------------------------------------------------------------------------*
 *	DL DATA REQUEST from Layer 3
 *---------------------------------------------------------------------------*/
int i4b_dl_data_req(int unit, struct mbuf *m)
{
	l2_softc_t *l2sc = &l2_softc[unit];

#ifdef NOTDEF
	NDBGL2(L2_PRIM, "unit %d",unit);
#endif
	switch(l2sc->Q921_state)
	{
		case ST_AW_EST:
		case ST_MULTIFR:
		case ST_TIMREC:
		
		        if(_IF_QFULL(&l2sc->i_queue))
		        {
		        	NDBGL2(L2_ERROR, "i_queue full!!");
		        	i4b_Dfreembuf(m);
		        }
		        else
		        {
		        	CRIT_VAR;

		        	CRIT_BEG;
				IF_ENQUEUE(&l2sc->i_queue, m);
				CRIT_END;

				i4b_i_frame_queued_up(l2sc);
			}
			break;
			
		default:
			NDBGL2(L2_ERROR, "unit %d ERROR in state [%s], freeing mbuf", unit, i4b_print_l2state(l2sc));
			i4b_Dfreembuf(m);
			break;
	}		
	return(0);
}

/*---------------------------------------------------------------------------*
 *	i4b_ph_activate_ind - link activation indication from layer 1
 *---------------------------------------------------------------------------*/
int
i4b_ph_activate_ind(int unit)
{
	l2_softc_t *l2sc = &l2_softc[unit];

	NDBGL1(L1_PRIM, "unit %d",unit);
	l2sc->ph_active = PH_ACTIVE;
	return(0);
}

/*---------------------------------------------------------------------------*
 *	i4b_ph_deactivate_ind - link deactivation indication from layer 1
 *---------------------------------------------------------------------------*/
int
i4b_ph_deactivate_ind(int unit)
{
	l2_softc_t *l2sc = &l2_softc[unit];

	NDBGL1(L1_PRIM, "unit %d",unit);
	l2sc->ph_active = PH_INACTIVE;
	return(0);
}


/*---------------------------------------------------------------------------*
 *	i4b_l2_unit_init - place layer 2 unit into known state
 *---------------------------------------------------------------------------*/
static void
i4b_l2_unit_init(int unit)
{
	l2_softc_t *l2sc = &l2_softc[unit];
	CRIT_VAR;

	CRIT_BEG;
	l2sc->Q921_state = ST_TEI_UNAS;
	l2sc->tei_valid = TEI_INVALID;
	l2sc->vr = 0;
	l2sc->vs = 0;
	l2sc->va = 0;
	l2sc->ack_pend = 0;
	l2sc->rej_excpt = 0;
	l2sc->peer_busy = 0;
	l2sc->own_busy = 0;
	l2sc->l3initiated = 0;

	l2sc->rxd_CR = 0;
	l2sc->rxd_PF = 0;
	l2sc->rxd_NR = 0;
	l2sc->RC = 0;
	l2sc->iframe_sent = 0;
		
	l2sc->postfsmfunc = NULL;

	if(l2sc->ua_num != UA_EMPTY)
	{
		i4b_Dfreembuf(l2sc->ua_frame);
		l2sc->ua_num = UA_EMPTY;
		l2sc->ua_frame = NULL;
	}

	i4b_T200_stop(l2sc);
	i4b_T202_stop(l2sc);
	i4b_T203_stop(l2sc);

	CRIT_END;	
}

/*---------------------------------------------------------------------------*
 *	i4b_mph_status_ind - status indication upward
 *---------------------------------------------------------------------------*/
int
i4b_mph_status_ind(int unit, int status, int parm)
{
	l2_softc_t *l2sc = &l2_softc[unit];
	CRIT_VAR;
	int sendup = 1;
	
	CRIT_BEG;

	NDBGL1(L1_PRIM, "unit %d, status=%d, parm=%d", unit, status, parm);

	switch(status)
	{
		case STI_ATTACH:
			l2sc->unit = unit;
			l2sc->i_queue.ifq_maxlen = IQUEUE_MAXLEN;

			if(!mtx_initialized(&l2sc->i_queue.ifq_mtx))
				mtx_init(&l2sc->i_queue.ifq_mtx, "i4b_l2sc", NULL, MTX_DEF);

			l2sc->ua_frame = NULL;
			bzero(&l2sc->stat, sizeof(lapdstat_t));			
			i4b_l2_unit_init(unit);
			
			/* initialize the callout handles for timeout routines */
			callout_handle_init(&l2sc->T200_callout);
			callout_handle_init(&l2sc->T202_callout);
			callout_handle_init(&l2sc->T203_callout);
			callout_handle_init(&l2sc->IFQU_callout);
			break;

		case STI_L1STAT:	/* state of layer 1 */
			break;
		
		case STI_PDEACT:	/* Timer 4 expired */
/*XXX*/			if((l2sc->Q921_state >= ST_AW_EST) &&
			   (l2sc->Q921_state <= ST_TIMREC))
			{
				NDBGL2(L2_ERROR, "unit %d, persistent deactivation!", unit);
				i4b_l2_unit_init(unit);
			}
			else
			{
				sendup = 0;
			}
			break;

		case STI_NOL1ACC:
			i4b_l2_unit_init(unit);
			NDBGL2(L2_ERROR, "unit %d, cannot access S0 bus!", unit);
			break;
			
		default:
			NDBGL2(L2_ERROR, "ERROR, unit %d, unknown status message!", unit);
			break;
	}
	
	if(sendup)
		MDL_Status_Ind(unit, status, parm);  /* send up to layer 3 */

	CRIT_END;
	
	return(0);
}

/*---------------------------------------------------------------------------*
 *	MDL_COMMAND_REQ from layer 3
 *---------------------------------------------------------------------------*/
static int
i4b_mdl_command_req(int unit, int command, void * parm)
{
	NDBGL2(L2_PRIM, "unit %d, command=%d, parm=%d", unit, command, (unsigned int)parm);

	switch(command)
	{
		case CMR_DOPEN:
			i4b_l2_unit_init(unit);
			break;
	}		

	MPH_Command_Req(unit, command, parm);
	
	return(0);
}

/*---------------------------------------------------------------------------*
 *	i4b_ph_data_ind - process a rx'd frame got from layer 1
 *---------------------------------------------------------------------------*/
int
i4b_ph_data_ind(int unit, struct mbuf *m)
{
	l2_softc_t *l2sc = &l2_softc[unit];
#ifdef NOTDEF
	NDBGL1(L1_PRIM, "unit %d", unit);
#endif
	u_char *ptr = m->m_data;

	if ( (*(ptr + OFF_CNTL) & 0x01) == 0 )
	{
		if(m->m_len < 4)	/* 6 oct - 2 chksum oct */
		{
			l2sc->stat.err_rx_len++;
			NDBGL2(L2_ERROR, "ERROR, I-frame < 6 octetts!");
			i4b_Dfreembuf(m);
			return(0);
		}
		i4b_rxd_i_frame(unit, m);
	}
	else if ( (*(ptr + OFF_CNTL) & 0x03) == 0x01 )
	{
		if(m->m_len < 4)	/* 6 oct - 2 chksum oct */
		{
			l2sc->stat.err_rx_len++;
			NDBGL2(L2_ERROR, "ERROR, S-frame < 6 octetts!");
			i4b_Dfreembuf(m);
			return(0);
		}
		i4b_rxd_s_frame(unit, m);
	}
	else if ( (*(ptr + OFF_CNTL) & 0x03) == 0x03 )
	{
		if(m->m_len < 3)	/* 5 oct - 2 chksum oct */
		{
			l2sc->stat.err_rx_len++;
			NDBGL2(L2_ERROR, "ERROR, U-frame < 5 octetts!");
			i4b_Dfreembuf(m);
			return(0);
		}
		i4b_rxd_u_frame(unit, m);
	}
	else
	{
		l2sc->stat.err_rx_badf++;
		NDBGL2(L2_ERROR, "ERROR, bad frame rx'd - ");
		i4b_print_frame(m->m_len, m->m_data);
		i4b_Dfreembuf(m);
	}
	return(0);
}
