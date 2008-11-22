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
 *	i4b_i4bdrv.c - i4b userland interface driver
 *	--------------------------------------------
 *      last edit-date: [Sun Aug 11 12:42:46 2002]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_i4b.h"

#include <sys/param.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/selinfo.h>
#include <net/if.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#include <machine/i4b_cause.h>

#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_global.h>

#include <i4b/layer4/i4b_l4.h>

#include <sys/poll.h>

struct selinfo select_rd_info;

static struct ifqueue i4b_rdqueue;
static int openflag = 0;
static int selflag = 0;
static int readflag = 0;

static	d_open_t	i4bopen;
static	d_close_t	i4bclose;
static	d_read_t	i4bread;
static	d_ioctl_t	i4bioctl;
static	d_poll_t	i4bpoll;


static struct cdevsw i4b_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	i4bopen,
	.d_close =	i4bclose,
	.d_read =	i4bread,
	.d_ioctl =	i4bioctl,
	.d_poll =	i4bpoll,
	.d_name =	"i4b",
};

static void i4battach(void *);
PSEUDO_SET(i4battach, i4b_i4bdrv);

/*---------------------------------------------------------------------------*
 *	interface attach routine
 *---------------------------------------------------------------------------*/
static void
i4battach(void *dummy)
{
	printf("i4b: ISDN call control device attached\n");

	i4b_rdqueue.ifq_maxlen = IFQ_MAXLEN;

	if(!mtx_initialized(&i4b_rdqueue.ifq_mtx))
		mtx_init(&i4b_rdqueue.ifq_mtx, "i4b_rdqueue", NULL, MTX_DEF);

	make_dev(&i4b_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "i4b");
}

/*---------------------------------------------------------------------------*
 *	i4bopen - device driver open routine
 *---------------------------------------------------------------------------*/
static int
i4bopen(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	int x;
	
	if(minor(dev))
		return(ENXIO);

	if(openflag)
		return(EBUSY);
	
	x = splimp();
	openflag = 1;
	i4b_l4_daemon_attached();
	splx(x);
	
	return(0);
}

/*---------------------------------------------------------------------------*
 *	i4bclose - device driver close routine
 *---------------------------------------------------------------------------*/
static int
i4bclose(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	int x = splimp();	
	openflag = 0;
	i4b_l4_daemon_detached();
	i4b_Dcleanifq(&i4b_rdqueue);
	splx(x);
	return(0);
}

/*---------------------------------------------------------------------------*
 *	i4bread - device driver read routine
 *---------------------------------------------------------------------------*/
static int
i4bread(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct mbuf *m;
	int x;
	int error = 0;

	if(minor(dev))
		return(ENODEV);

	x = splimp();
	IF_LOCK(&i4b_rdqueue);
	while(IF_QEMPTY(&i4b_rdqueue))
	{
		readflag = 1;

		error = msleep( &i4b_rdqueue, &i4b_rdqueue.ifq_mtx,
			(PZERO + 1) | PCATCH, "bird", 0);

		if (error != 0) {
			IF_UNLOCK(&i4b_rdqueue);
			splx(x);
			return error;
		}
	}

	_IF_DEQUEUE(&i4b_rdqueue, m);
	IF_UNLOCK(&i4b_rdqueue);

	splx(x);
		
	if(m && m->m_len)
		error = uiomove(m->m_data, m->m_len, uio);
	else
		error = EIO;
		
	if(m)
		i4b_Dfreembuf(m);

	return(error);
}

/*---------------------------------------------------------------------------*
 *	i4bioctl - device driver ioctl routine
 *---------------------------------------------------------------------------*/
static int
i4bioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	call_desc_t *cd;
	int error = 0;
	
	if(minor(dev))
		return(ENODEV);

	switch(cmd)
	{
		/* cdid request, reserve cd and return cdid */

		case I4B_CDID_REQ:
		{
			msg_cdid_req_t *mir;
			mir = (msg_cdid_req_t *)data;
			cd = reserve_cd();
			mir->cdid = cd->cdid;
			break;
		}
		
		/* connect request, dial out to remote */
		
		case I4B_CONNECT_REQ:
		{
			msg_connect_req_t *mcr;
			mcr = (msg_connect_req_t *)data;	/* setup ptr */

			if((cd = cd_by_cdid(mcr->cdid)) == NULL)/* get cd */
			{
				NDBGL4(L4_ERR, "I4B_CONNECT_REQ ioctl, cdid not found!"); 
				error = EINVAL;
				break;
			}

			/* prevent dialling on leased lines */
			if(ctrl_desc[mcr->controller].protocol == PROTOCOL_D64S)
			{
				SET_CAUSE_TYPE(cd->cause_in, CAUSET_I4B);
				SET_CAUSE_VAL(cd->cause_in, CAUSE_I4B_LLDIAL);
				i4b_l4_disconnect_ind(cd);
				freecd_by_cd(cd);
				break;
			}

			cd->controller = mcr->controller;	/* fill cd */
			cd->bprot = mcr->bprot;
			cd->bcap = mcr->bcap;			
			cd->driver = mcr->driver;
			cd->driver_unit = mcr->driver_unit;
			cd->cr = get_rand_cr(ctrl_desc[cd->controller].unit);

			cd->shorthold_data.shorthold_algorithm = mcr->shorthold_data.shorthold_algorithm;
			cd->shorthold_data.unitlen_time  = mcr->shorthold_data.unitlen_time;
			cd->shorthold_data.idle_time     = mcr->shorthold_data.idle_time;
			cd->shorthold_data.earlyhup_time = mcr->shorthold_data.earlyhup_time;

			cd->last_aocd_time = 0;
			if(mcr->unitlen_method == ULEN_METHOD_DYNAMIC)
				cd->aocd_flag = 1;
			else
				cd->aocd_flag = 0;
				
			cd->cunits = 0;

			cd->max_idle_time = 0;	/* this is outgoing */

			cd->dir = DIR_OUTGOING;
			
			NDBGL4(L4_TIMO, "I4B_CONNECT_REQ times, algorithm=%ld unitlen=%ld idle=%ld earlyhup=%ld",
					(long)cd->shorthold_data.shorthold_algorithm, (long)cd->shorthold_data.unitlen_time,
					(long)cd->shorthold_data.idle_time, (long)cd->shorthold_data.earlyhup_time);

			strcpy(cd->dst_telno, mcr->dst_telno);
			strcpy(cd->src_telno, mcr->src_telno);

			strcpy(cd->dst_subaddr, mcr->dst_subaddr);
			strcpy(cd->src_subaddr, mcr->src_subaddr);

			if(mcr->keypad[0] != '\0')
				strcpy(cd->keypad, mcr->keypad);
			else
				cd->keypad[0] = '\0';
				
			cd->display[0] = '\0';

			SET_CAUSE_TYPE(cd->cause_in, CAUSET_I4B);
			SET_CAUSE_VAL(cd->cause_in, CAUSE_I4B_NORMAL);
			
			switch(mcr->channel)
			{
				case CHAN_B1:
				case CHAN_B2:
					if(ctrl_desc[mcr->controller].bch_state[mcr->channel] != BCH_ST_FREE)
						SET_CAUSE_VAL(cd->cause_in, CAUSE_I4B_NOCHAN);
					break;

				case CHAN_ANY:
				{
				    int i;
				    for (i = 0;
					 i < ctrl_desc[mcr->controller].nbch &&
					 ctrl_desc[mcr->controller].bch_state[i] != BCH_ST_FREE;
					 i++);
				    if (i == ctrl_desc[mcr->controller].nbch)
						SET_CAUSE_VAL(cd->cause_in, CAUSE_I4B_NOCHAN);
				    /* else mcr->channel = i; XXX */
				}
					break;

				default:
					SET_CAUSE_VAL(cd->cause_in, CAUSE_I4B_NOCHAN);
					break;
			}

			cd->channelid = mcr->channel;

			cd->isdntxdelay = mcr->txdelay;
			
			/* check whether we have a pointer. Seems like */
			/* this should be adequate. GJ 19.09.97 */
			if(ctrl_desc[cd->controller].N_CONNECT_REQUEST == NULL)
/*XXX*/				SET_CAUSE_VAL(cd->cause_in, CAUSE_I4B_NOCHAN);

			if((GET_CAUSE_VAL(cd->cause_in)) != CAUSE_I4B_NORMAL)
			{
				i4b_l4_disconnect_ind(cd);
				freecd_by_cd(cd);
			}
			else
			{
				(*ctrl_desc[cd->controller].N_CONNECT_REQUEST)(mcr->cdid);
			}
			break;
		}
		
		/* connect response, accept/reject/ignore incoming call */
		
		case I4B_CONNECT_RESP:
		{
			msg_connect_resp_t *mcrsp;
			
			mcrsp = (msg_connect_resp_t *)data;

			if((cd = cd_by_cdid(mcrsp->cdid)) == NULL)/* get cd */
			{
				NDBGL4(L4_ERR, "I4B_CONNECT_RESP ioctl, cdid not found!"); 
				error = EINVAL;
				break;
			}

			T400_stop(cd);

			cd->driver = mcrsp->driver;
			cd->driver_unit = mcrsp->driver_unit;
			cd->max_idle_time = mcrsp->max_idle_time;

			cd->shorthold_data.shorthold_algorithm = SHA_FIXU;
			cd->shorthold_data.unitlen_time = 0;	/* this is incoming */
			cd->shorthold_data.idle_time = 0;
			cd->shorthold_data.earlyhup_time = 0;

			cd->isdntxdelay = mcrsp->txdelay;			
			
			NDBGL4(L4_TIMO, "I4B_CONNECT_RESP max_idle_time set to %ld seconds", (long)cd->max_idle_time);

			(*ctrl_desc[cd->controller].N_CONNECT_RESPONSE)(mcrsp->cdid, mcrsp->response, mcrsp->cause);
			break;
		}
		
		/* disconnect request, actively terminate connection */
		
		case I4B_DISCONNECT_REQ:
		{
			msg_discon_req_t *mdr;
			
			mdr = (msg_discon_req_t *)data;

			if((cd = cd_by_cdid(mdr->cdid)) == NULL)/* get cd */
			{
				NDBGL4(L4_ERR, "I4B_DISCONNECT_REQ ioctl, cdid not found!"); 
				error = EINVAL;
				break;
			}

			/* preset causes with our cause */
			cd->cause_in = cd->cause_out = mdr->cause;
			
			(*ctrl_desc[cd->controller].N_DISCONNECT_REQUEST)(mdr->cdid, mdr->cause);
			break;
		}
		
		/* controller info request */

		case I4B_CTRL_INFO_REQ:
		{
			msg_ctrl_info_req_t *mcir;
			
			mcir = (msg_ctrl_info_req_t *)data;
			mcir->ncontroller = nctrl;

			if(mcir->controller > nctrl)
			{
				mcir->ctrl_type = -1;
				mcir->card_type = -1;
			}
			else
			{
				mcir->ctrl_type = 
					ctrl_desc[mcir->controller].ctrl_type;
				mcir->card_type = 
					ctrl_desc[mcir->controller].card_type;
				mcir->nbch =
					ctrl_desc[mcir->controller].nbch;

				if(ctrl_desc[mcir->controller].ctrl_type == CTRL_PASSIVE)
					mcir->tei = ctrl_desc[mcir->controller].tei;
				else
					mcir->tei = -1;
			}
			break;
		}
		
		/* dial response */
		
		case I4B_DIALOUT_RESP:
		{
			drvr_link_t *dlt = NULL;
			msg_dialout_resp_t *mdrsp;
			
			mdrsp = (msg_dialout_resp_t *)data;

			switch(mdrsp->driver)
			{
#if NI4BIPR > 0
				case BDRV_IPR:
					dlt = ipr_ret_linktab(mdrsp->driver_unit);
					break;
#endif					

#if NI4BISPPP > 0
				case BDRV_ISPPP:
					dlt = i4bisppp_ret_linktab(mdrsp->driver_unit);
					break;
#endif

#if NI4BTEL > 0
				case BDRV_TEL:
					dlt = tel_ret_linktab(mdrsp->driver_unit);
					break;
#endif

#if NIBC > 0
				case BDRV_IBC:
					dlt = ibc_ret_linktab(mdrsp->driver_unit);
					break;
#endif

#if NI4BING > 0
				case BDRV_ING:
					dlt = ing_ret_linktab(mdrsp->driver_unit);
					break;
#endif					
			}

			if(dlt != NULL)		
				(*dlt->dial_response)(mdrsp->driver_unit, mdrsp->stat, mdrsp->cause);
			break;
		}
		
		/* update timeout value */
		
		case I4B_TIMEOUT_UPD:
		{
			msg_timeout_upd_t *mtu;
			int x;
			
			mtu = (msg_timeout_upd_t *)data;

			NDBGL4(L4_TIMO, "I4B_TIMEOUT_UPD ioctl, alg %d, unit %d, idle %d, early %d!",
					mtu->shorthold_data.shorthold_algorithm, mtu->shorthold_data.unitlen_time,
					mtu->shorthold_data.idle_time, mtu->shorthold_data.earlyhup_time); 

			if((cd = cd_by_cdid(mtu->cdid)) == NULL)/* get cd */
			{
				NDBGL4(L4_ERR, "I4B_TIMEOUT_UPD ioctl, cdid not found!"); 
				error = EINVAL;
				break;
			}

			switch( mtu->shorthold_data.shorthold_algorithm )
			{
				case SHA_FIXU:
					/*
					 * For this algorithm unitlen_time,
					 * idle_time and earlyhup_time are used.
					 */

					if(!(mtu->shorthold_data.unitlen_time >= 0 &&
					     mtu->shorthold_data.idle_time >= 0    &&
					     mtu->shorthold_data.earlyhup_time >= 0))
					{
						NDBGL4(L4_ERR, "I4B_TIMEOUT_UPD ioctl, invalid args for fix unit algorithm!"); 
						error = EINVAL;
					}
					break;
	
				case SHA_VARU:
					/*
					 * For this algorithm unitlen_time and
					 * idle_time are used. both must be
					 * positive integers. earlyhup_time is
					 * not used and must be 0.
					 */

					if(!(mtu->shorthold_data.unitlen_time > 0 &&
					     mtu->shorthold_data.idle_time >= 0   &&
					     mtu->shorthold_data.earlyhup_time == 0))
					{
						NDBGL4(L4_ERR, "I4B_TIMEOUT_UPD ioctl, invalid args for var unit algorithm!"); 
						error = EINVAL;
					}
					break;
	
				default:
					NDBGL4(L4_ERR, "I4B_TIMEOUT_UPD ioctl, invalid algorithm!"); 
					error = EINVAL;
					break;
			}

			/*
			 * any error set above requires us to break
			 * out of the outer switch
			 */
			if(error != 0)
				break;

			x = SPLI4B();
			cd->shorthold_data.shorthold_algorithm = mtu->shorthold_data.shorthold_algorithm;
			cd->shorthold_data.unitlen_time = mtu->shorthold_data.unitlen_time;
			cd->shorthold_data.idle_time = mtu->shorthold_data.idle_time;
			cd->shorthold_data.earlyhup_time = mtu->shorthold_data.earlyhup_time;
			splx(x);
			break;
		}
			
		/* soft enable/disable interface */
		
		case I4B_UPDOWN_IND:
		{
			msg_updown_ind_t *mui;
			
			mui = (msg_updown_ind_t *)data;

#if NI4BIPR > 0
			if(mui->driver == BDRV_IPR)
			{
				drvr_link_t *dlt;
				dlt = ipr_ret_linktab(mui->driver_unit);
				(*dlt->updown_ind)(mui->driver_unit, mui->updown);
			}
#endif
			break;
		}
		
		/* send ALERT request */
		
		case I4B_ALERT_REQ:
		{
			msg_alert_req_t *mar;
			
			mar = (msg_alert_req_t *)data;

			if((cd = cd_by_cdid(mar->cdid)) == NULL)
			{
				NDBGL4(L4_ERR, "I4B_ALERT_REQ ioctl, cdid not found!"); 
				error = EINVAL;
				break;
			}

			T400_stop(cd);
			
			(*ctrl_desc[cd->controller].N_ALERT_REQUEST)(mar->cdid);

			break;
		}

		/* version/release number request */
		
		case I4B_VR_REQ:
                {
			msg_vr_req_t *mvr;

			mvr = (msg_vr_req_t *)data;

			mvr->version = VERSION;
			mvr->release = REL;
			mvr->step = STEP;			
			break;
		}

		/* set D-channel protocol for a controller */
		
		case I4B_PROT_IND:
		{
			msg_prot_ind_t *mpi;
			
			mpi = (msg_prot_ind_t *)data;

			ctrl_desc[mpi->controller].protocol = mpi->protocol;
			
			break;
		}
		
		/* Download request */

		case I4B_CTRL_DOWNLOAD:
		{
			struct isdn_dr_prot *prots = NULL, *prots2 = NULL;
			struct isdn_download_request *r =
				(struct isdn_download_request*)data;
			int i;

			if (r->controller < 0 || r->controller >= nctrl)
			{
				error = ENODEV;
				goto download_done;
			}

			if(!ctrl_desc[r->controller].N_DOWNLOAD)
			{
				error = ENODEV;
				goto download_done;
			}

			prots = malloc(r->numprotos * sizeof(struct isdn_dr_prot),
					M_DEVBUF, M_WAITOK);

			prots2 = malloc(r->numprotos * sizeof(struct isdn_dr_prot),
					M_DEVBUF, M_WAITOK);

			if(!prots || !prots2)
			{
				error = ENOMEM;
				goto download_done;
			}

			copyin(r->protocols, prots, r->numprotos * sizeof(struct isdn_dr_prot));

			for(i = 0; i < r->numprotos; i++)
			{
				prots2[i].microcode = malloc(prots[i].bytecount, M_DEVBUF, M_WAITOK);
				copyin(prots[i].microcode, prots2[i].microcode, prots[i].bytecount);
				prots2[i].bytecount = prots[i].bytecount; 
			}

			error = ctrl_desc[r->controller].N_DOWNLOAD(
						ctrl_desc[r->controller].unit,
						r->numprotos, prots2);

download_done:
			if(prots2)
			{
				for(i = 0; i < r->numprotos; i++)
				{
					if(prots2[i].microcode)
					{
						free(prots2[i].microcode, M_DEVBUF);
					}
				}
				free(prots2, M_DEVBUF);
			}

			if(prots)
			{
				free(prots, M_DEVBUF);
			}
			break;
		}

		/* Diagnostic request */

		case I4B_ACTIVE_DIAGNOSTIC:
		{
			struct isdn_diagnostic_request req, *r =
				(struct isdn_diagnostic_request*)data;

			req.in_param = req.out_param = NULL;
			if (r->controller < 0 || r->controller >= nctrl)
			{
				error = ENODEV;
				goto diag_done;
			}

			if(!ctrl_desc[r->controller].N_DIAGNOSTICS)
			{
				error = ENODEV;
				goto diag_done;
			}

			memcpy(&req, r, sizeof(req));

			if(req.in_param_len)
			{
				/* XXX arbitrary limit */
				if (req.in_param_len >
				    I4B_ACTIVE_DIAGNOSTIC_MAXPARAMLEN) {
				    	error = EINVAL;
				    	goto diag_done;
				}	

				req.in_param = malloc(r->in_param_len, M_DEVBUF, M_WAITOK);

				if(!req.in_param)
				{
					error = ENOMEM;
					goto diag_done;
				}
				error = copyin(r->in_param, req.in_param, req.in_param_len);
				if (error)
					goto diag_done;
			}

			if(req.out_param_len)
			{
				req.out_param = malloc(r->out_param_len, M_DEVBUF, M_WAITOK);

				if(!req.out_param)
				{
					error = ENOMEM;
					goto diag_done;
				}
			}
			
			error = ctrl_desc[r->controller].N_DIAGNOSTICS(r->controller, &req);

			if(!error && req.out_param_len)
				error = copyout(req.out_param, r->out_param, req.out_param_len);

diag_done:
			if(req.in_param)
				free(req.in_param, M_DEVBUF);
				
			if(req.out_param)
				free(req.out_param, M_DEVBUF);

			break;
		}

		/* default */
		
		default:
			error = ENOTTY;
			break;
	}
	
	return(error);
}

/*---------------------------------------------------------------------------*
 *	i4bpoll - device driver poll routine
 *---------------------------------------------------------------------------*/
static int
i4bpoll(struct cdev *dev, int events, struct thread *td)
{
	int x;
	
	if(minor(dev))
		return(ENODEV);

	if((events & POLLIN) || (events & POLLRDNORM))
	{
		if(!IF_QEMPTY(&i4b_rdqueue))
			return(1);

		x = splimp();
		selrecord(td, &select_rd_info);
		selflag = 1;
		splx(x);
		return(0);
	}
	else if((events & POLLOUT) || (events & POLLWRNORM))
	{
		return(1);
	}

	return(0);
}

/*---------------------------------------------------------------------------*
 *	i4bputqueue - put message into queue to userland
 *---------------------------------------------------------------------------*/
void
i4bputqueue(struct mbuf *m)
{
	int x;
	
	if(!openflag)
	{
		i4b_Dfreembuf(m);
		return;
	}

	x = splimp();
	
	IF_LOCK(&i4b_rdqueue);
	if(_IF_QFULL(&i4b_rdqueue))
	{
		struct mbuf *m1;
		_IF_DEQUEUE(&i4b_rdqueue, m1);
		i4b_Dfreembuf(m1);
		NDBGL4(L4_ERR, "ERROR, queue full, removing entry!");
	}

	_IF_ENQUEUE(&i4b_rdqueue, m);
	IF_UNLOCK(&i4b_rdqueue);

	splx(x);	

	if(readflag)
	{
		readflag = 0;
		wakeup( &i4b_rdqueue);
	}

	if(selflag)
	{
		selflag = 0;
		selwakeuppri(&select_rd_info, I4BPRI);
	}
}

/*---------------------------------------------------------------------------*
 *	i4bputqueue_hipri - put message into front of queue to userland
 *---------------------------------------------------------------------------*/
void
i4bputqueue_hipri(struct mbuf *m)
{
	int x;
	
	if(!openflag)
	{
		i4b_Dfreembuf(m);
		return;
	}

	x = splimp();
	
	IF_LOCK(&i4b_rdqueue);
	if(_IF_QFULL(&i4b_rdqueue))
	{
		struct mbuf *m1;
		_IF_DEQUEUE(&i4b_rdqueue, m1);
		i4b_Dfreembuf(m1);
		NDBGL4(L4_ERR, "ERROR, queue full, removing entry!");
	}

	_IF_PREPEND(&i4b_rdqueue, m);
	IF_UNLOCK(&i4b_rdqueue);

	splx(x);	

	if(readflag)
	{
		readflag = 0;
		wakeup( &i4b_rdqueue);
	}

	if(selflag)
	{
		selflag = 0;
		selwakeuppri(&select_rd_info, I4BPRI);
	}
}
