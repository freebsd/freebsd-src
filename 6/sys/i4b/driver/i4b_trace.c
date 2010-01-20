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
 *	i4btrc - device driver for trace data read device
 *	---------------------------------------------------
 *	last edit-date: [Sun Mar 17 09:52:51 2002]
 *
 *	NOTE: the code assumes that SPLI4B >= splimp !
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_i4b.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioccom.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/tty.h>

#include <machine/i4b_trace.h>
#include <machine/i4b_ioctl.h>

#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l3l4.h>

static struct ifqueue trace_queue[NI4BTRC];

static int device_state[NI4BTRC];
#define ST_IDLE		0x00
#define ST_ISOPEN	0x01
#define ST_WAITDATA	0x02

static int analyzemode = 0;
static int rxunit = -1;
static int txunit = -1;
static int outunit = -1;

static d_open_t	i4btrcopen;
static d_close_t i4btrcclose;
static d_read_t i4btrcread;
static d_ioctl_t i4btrcioctl;
static d_poll_t i4btrcpoll;


static struct cdevsw i4btrc_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	i4btrcopen,
	.d_close =	i4btrcclose,
	.d_read =	i4btrcread,
	.d_ioctl =	i4btrcioctl,
	.d_poll =	i4btrcpoll,
	.d_name =	"i4btrc",
};

static void i4btrcattach(void *);
PSEUDO_SET(i4btrcattach, i4b_trace);

int get_trace_data_from_l1(i4b_trace_hdr_t *hdr, int len, char *buf);

/*---------------------------------------------------------------------------*
 *	interface attach routine
 *---------------------------------------------------------------------------*/
static void
i4btrcattach(void *dummy)
{
	int i;

	printf("i4btrc: %d ISDN trace device(s) attached\n", NI4BTRC);
	
	for(i=0; i < NI4BTRC; i++)
	{
		make_dev(&i4btrc_cdevsw, i,
				     UID_ROOT, GID_WHEEL, 0600, "i4btrc%d", i);
		trace_queue[i].ifq_maxlen = IFQ_MAXLEN;

		if(!mtx_initialized(&trace_queue[i].ifq_mtx))
			mtx_init(&trace_queue[i].ifq_mtx, "i4b_trace", NULL, MTX_DEF);

		device_state[i] = ST_IDLE;
	}
}

/*---------------------------------------------------------------------------*
 *	get_trace_data_from_l1()
 *	------------------------
 *	is called from layer 1, adds timestamp to trace data and puts
 *	it into a queue, from which it can be read from the i4btrc
 *	device. The unit number in the trace header selects the minor
 *	device's queue the data is put into.
 *---------------------------------------------------------------------------*/
int
get_trace_data_from_l1(i4b_trace_hdr_t *hdr, int len, char *buf)
{
	struct mbuf *m;
	int x;
	int unit;
	int trunc = 0;
	int totlen = len + sizeof(i4b_trace_hdr_t);

	/*
	 * for telephony (or better non-HDLC HSCX mode) we get 
	 * (MCLBYTE + sizeof(i4b_trace_hdr_t)) length packets
	 * to put into the queue to userland. because of this
	 * we detect this situation, strip the length to MCLBYTES
	 * max size, and infor the userland program of this fact
	 * by putting the no of truncated bytes into hdr->trunc.
	 */
	 
	if(totlen > MCLBYTES)
	{
		trunc = 1;
		hdr->trunc = totlen - MCLBYTES;
		totlen = MCLBYTES;
	}
	else
	{
		hdr->trunc = 0;
	}

	/* set length of trace record */
	
	hdr->length = totlen;
	
	/* check valid unit no */
	
	if((unit = hdr->unit) >= NI4BTRC)
	{
		printf("i4b_trace: get_trace_data_from_l1 - unit > NI4BTRC!\n"); 
		return(0);
	}

	/* get mbuf */
	
	if(!(m = i4b_Bgetmbuf(totlen)))
	{
		printf("i4b_trace: get_trace_data_from_l1 - i4b_getmbuf() failed!\n");
		return(0);
	}

	/* check if we are in analyzemode */
	
	if(analyzemode && (unit == rxunit || unit == txunit))
	{
		if(unit == rxunit)
			hdr->dir = FROM_NT;
		else
			hdr->dir = FROM_TE;
		unit = outunit;			
	}

	IF_LOCK(&trace_queue[unit]);

	if(_IF_QFULL(&trace_queue[unit]))
	{
		struct mbuf *m1;

		x = SPLI4B();
		_IF_DEQUEUE(&trace_queue[unit], m1);
		splx(x);		

		i4b_Bfreembuf(m1);
	}
	
	/* copy trace header */
	memcpy(m->m_data, hdr, sizeof(i4b_trace_hdr_t));

	/* copy trace data */
	if(trunc)
		memcpy(&m->m_data[sizeof(i4b_trace_hdr_t)], buf, totlen-sizeof(i4b_trace_hdr_t));
	else
		memcpy(&m->m_data[sizeof(i4b_trace_hdr_t)], buf, len);

	x = SPLI4B();
	
	_IF_ENQUEUE(&trace_queue[unit], m);
	IF_UNLOCK(&trace_queue[unit]);
	
	if(device_state[unit] & ST_WAITDATA)
	{
		device_state[unit] &= ~ST_WAITDATA;
		wakeup( &trace_queue[unit]);
	}

	splx(x);
	
	return(1);
}

/*---------------------------------------------------------------------------*
 *	open trace device
 *---------------------------------------------------------------------------*/
static int
i4btrcopen(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	int x;
	int unit = minor(dev);

	if(unit >= NI4BTRC)
		return(ENXIO);

	if(device_state[unit] & ST_ISOPEN)
		return(EBUSY);

	if(analyzemode && (unit == outunit || unit == rxunit || unit == txunit))
		return(EBUSY);

	x = SPLI4B();
	
	device_state[unit] = ST_ISOPEN;		

	splx(x);
	
	return(0);
}

/*---------------------------------------------------------------------------*
 *	close trace device
 *---------------------------------------------------------------------------*/
static int
i4btrcclose(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	int unit = minor(dev);
	int i, x;
	int cno = -1;

	for(i=0; i < nctrl; i++)
	{
		if((ctrl_desc[i].ctrl_type == CTRL_PASSIVE) &&
			(ctrl_desc[i].unit == unit))
		{
			cno = i;
			break;
		}
	}

	if(analyzemode && (unit == outunit))
	{
		analyzemode = 0;		
		outunit = -1;
		
		if(cno >= 0)
		{
			(*ctrl_desc[cno].N_MGMT_COMMAND)(rxunit, CMR_SETTRACE, TRACE_OFF);
			(*ctrl_desc[cno].N_MGMT_COMMAND)(txunit, CMR_SETTRACE, TRACE_OFF);
		}
		rxunit = -1;
		txunit = -1;
	}
	
	if(cno >= 0)
	{
			(*ctrl_desc[cno].N_MGMT_COMMAND)(ctrl_desc[cno].unit, CMR_SETTRACE, TRACE_OFF);
	}

	x = SPLI4B();
	device_state[unit] = ST_IDLE;
	splx(x);
	
	return(0);
}

/*---------------------------------------------------------------------------*
 *	read from trace device
 *---------------------------------------------------------------------------*/
static int
i4btrcread(struct cdev *dev, struct uio * uio, int ioflag)
{
	struct mbuf *m;
	int x;
	int error = 0;
	int unit = minor(dev);
	
	if(!(device_state[unit] & ST_ISOPEN))
		return(EIO);

	x = SPLI4B();
	
	IF_LOCK(&trace_queue[unit]);

	while(IF_QEMPTY(&trace_queue[unit]) && (device_state[unit] & ST_ISOPEN))
	{
		device_state[unit] |= ST_WAITDATA;
		
		if((error = msleep( &trace_queue[unit],
					&trace_queue[unit].ifq_mtx,
					I4BPRI | PCATCH,
					"bitrc", 0 )) != 0)
		{
			device_state[unit] &= ~ST_WAITDATA;
			IF_UNLOCK(&trace_queue[unit]);
			splx(x);
			return(error);
		}
	}

	_IF_DEQUEUE(&trace_queue[unit], m);
	IF_UNLOCK(&trace_queue[unit]);

	if(m && m->m_len)
		error = uiomove(m->m_data, m->m_len, uio);
	else
		error = EIO;
		
	if(m)
		i4b_Bfreembuf(m);

	splx(x);
	
	return(error);
}

/*---------------------------------------------------------------------------*
 *	poll device
 *---------------------------------------------------------------------------*/
static int
i4btrcpoll(struct cdev *dev, int events, struct thread *td)
{
	return(ENODEV);
}

/*---------------------------------------------------------------------------*
 *	device driver ioctl routine
 *---------------------------------------------------------------------------*/
static int
i4btrcioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	int error = 0;
	int unit = minor(dev);
	i4b_trace_setupa_t *tsa;
	int i;
	int cno = -1;

	/* find the first passive controller matching our unit no */

	for(i=0; i < nctrl; i++)
	{
		if((ctrl_desc[i].ctrl_type == CTRL_PASSIVE) &&
			(ctrl_desc[i].unit == unit))
		{
			cno = i;
			break;
		}
	}
	
	switch(cmd)
	{
		case I4B_TRC_SET:
			if(cno < 0)
				return ENOTTY;
			(*ctrl_desc[cno].N_MGMT_COMMAND)(ctrl_desc[cno].unit, CMR_SETTRACE, (void *)*(unsigned int *)data);
			break;

		case I4B_TRC_SETA:
			tsa = (i4b_trace_setupa_t *)data;

			if(tsa->rxunit >= 0 && tsa->rxunit < NI4BTRC)
				rxunit = tsa->rxunit;
			else
				error = EINVAL;

			if(tsa->txunit >= 0 && tsa->txunit < NI4BTRC)
				txunit = tsa->txunit;
			else
				error = EINVAL;

			if(error)
			{
				outunit = -1;
				rxunit = -1;
				txunit = -1;
			}
			else
			{
				if(cno < 0)
					return ENOTTY;
					
				outunit = unit;
				analyzemode = 1;
				(*ctrl_desc[cno].N_MGMT_COMMAND)(rxunit, CMR_SETTRACE, (int *)(tsa->rxflags & (TRACE_I | TRACE_D_RX | TRACE_B_RX)));
				(*ctrl_desc[cno].N_MGMT_COMMAND)(txunit, CMR_SETTRACE, (int *)(tsa->txflags & (TRACE_I | TRACE_D_RX | TRACE_B_RX)));
			}
			break;

		case I4B_TRC_RESETA:
			analyzemode = 0;		
			outunit = -1;
			rxunit = -1;
			txunit = -1;
			break;
			
		default:
			error = ENOTTY;
			break;
	}
	return(error);
}
