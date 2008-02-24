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
 *	i4b_tel.c - device driver for ISDN telephony
 *	--------------------------------------------
 *	last edit-date: [Tue Aug 27 13:54:08 2002]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i4b/driver/i4b_tel.c,v 1.37 2007/07/06 07:17:18 bz Exp $");

#include "opt_i4b.h"

#undef I4BTELDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioccom.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/tty.h>

#include <i4b/include/i4b_ioctl.h>
#include <i4b/include/i4b_tel_ioctl.h>
#include <i4b/include/i4b_debug.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_l3l4.h>

#include <i4b/layer4/i4b_l4.h>

/* minor number: lower 6 bits = unit number */
#define UNITBITS	6
#define UNITMASK	0x3f
#define	UNIT(n)		(minor(n) & UNITMASK)

/* minor number: upper 2 bits = function number */

#define FUNCMASK	0x03
#define	FUNC(n)		(((minor(n)) >> UNITBITS) & FUNCMASK)

#define FUNCTEL		0	/* 0 = normal i4btel device	*/
#define FUNCDIAL	1	/* 1 = i4bteld dialout device	*/

#define NOFUNCS		2	/* number of device classes	*/

typedef struct {

	/* used only in func = FUNCTEL */

	drvr_link_t		drvr_linktab;	/* driver linktab */
	isdn_link_t 		*isdn_linktab;	/* isdn linktab	*/
	int 			audiofmt;	/* audio format conversion */
	u_char			*rcvttab;	/* conversion table on read */
	u_char			*wcvttab;	/* conversion table on write */
	call_desc_t		*cdp;		/* call descriptor pointer */

	/* used only in func = FUNCDIAL */

	char			result;		/* result code for dial dev */	

	/* used in func = FUNCDIAL and func = FUNCTEL*/
	
	int 			devstate;	/* state of this unit	*/
#define ST_IDLE		0x00		/* idle */
#define ST_CONNECTED	0x01		/* isdn connected state */
#define ST_ISOPEN	0x02		/* userland opened */
#define ST_RDWAITDATA	0x04		/* userland read waiting */
#define ST_WRWAITEMPTY	0x08		/* userland write waiting */
#define ST_TONE		0x10		/* tone generator */

	struct selinfo		selp;		/* select / poll */

	struct i4b_tel_tones	tones;
	int			toneidx;
	int			toneomega;
	int			tonefreq;

} tel_sc_t;

static tel_sc_t tel_sc[NI4BTEL][NOFUNCS];
	
/* forward decl */

static void tel_rx_data_rdy(int unit);
static void tel_tx_queue_empty(int unit);
static void tel_init_linktab(int unit);
static void tel_connect(int unit, void *cdp);
static void tel_disconnect(int unit, void *cdp);
static void tel_tone(tel_sc_t *sc);

/* audio format conversion tables */
static unsigned char a2u_tab[];
static unsigned char u2a_tab[];
static unsigned char bitreverse[];
static u_char sinetab[];

static d_open_t i4btelopen;
static d_close_t i4btelclose;
static d_read_t i4btelread;
static d_read_t i4btelwrite;
static d_ioctl_t i4btelioctl;
static d_poll_t i4btelpoll;


static struct cdevsw i4btel_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	i4btelopen,
	.d_close =	i4btelclose,
	.d_read =	i4btelread,
	.d_write =	i4btelwrite,
	.d_ioctl =	i4btelioctl,
	.d_poll =	i4btelpoll,
	.d_name =	"i4btel",
};

static void i4btelattach(void *);

PSEUDO_SET(i4btelattach, i4b_tel);

/*===========================================================================*
 *			DEVICE DRIVER ROUTINES
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	interface attach routine
 *---------------------------------------------------------------------------*/
static void
i4btelattach(void *dummy)
{
	int i, j;

	printf("i4btel: %d ISDN telephony interface device(s) attached\n", NI4BTEL);
	
	for(i=0; i < NI4BTEL; i++)
	{
		for(j=0; j < NOFUNCS; j++)
		{
			tel_sc[i][j].devstate = ST_IDLE;
			tel_sc[i][j].audiofmt = CVT_NONE;
			tel_sc[i][j].rcvttab = 0;
			tel_sc[i][j].wcvttab = 0;
			tel_sc[i][j].result = 0;

			switch(j)
			{
				case FUNCTEL:	/* normal i4btel device */
				  	make_dev(&i4btel_cdevsw, i,
						UID_ROOT, GID_WHEEL,
						0600, "i4btel%d", i);
					break;
				
				case FUNCDIAL:	/* i4bteld dialout device */
				  	make_dev(&i4btel_cdevsw, i+(1<<UNITBITS),
						UID_ROOT, GID_WHEEL,
						0600, "i4bteld%d", i);
					break;
			}
		}
		tel_init_linktab(i);		
	}
}

/*---------------------------------------------------------------------------*
 *	open tel device
 *---------------------------------------------------------------------------*/
static int
i4btelopen(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	int unit = UNIT(dev);
	int func = FUNC(dev);
	
	tel_sc_t *sc;
	
	if(unit >= NI4BTEL)
		return(ENXIO);

	sc = &tel_sc[unit][func];		

	if(sc->devstate & ST_ISOPEN)
		return(EBUSY);

	sc->devstate |= ST_ISOPEN;		

	if(func == FUNCDIAL)
	{
		sc->result = 0;
	}
	
	return(0);
}

/*---------------------------------------------------------------------------*
 *	close tel device
 *---------------------------------------------------------------------------*/
static int
i4btelclose(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	int unit = UNIT(dev);
	int func = FUNC(dev);
	tel_sc_t *sc;
	int error = 0;
	int x;
	
	if(unit > NI4BTEL)
		return(ENXIO);

	sc = &tel_sc[unit][func];		

	x = splimp();
	sc->devstate &= ~ST_TONE;		

	if((func == FUNCTEL) &&
	   (sc->isdn_linktab != NULL && sc->isdn_linktab->tx_queue != NULL))
	{
		while(!(IF_QEMPTY(sc->isdn_linktab->tx_queue)))
		{
			sc->devstate |= ST_WRWAITEMPTY;
	
			if((error = tsleep( &sc->isdn_linktab->tx_queue,
					I4BPRI | PCATCH, "wtcl", 0)) != 0)
			{
				break;
			}
		}
		sc->devstate &= ~ST_WRWAITEMPTY;		
	}

	sc->devstate &= ~ST_ISOPEN;		
	splx(x);
	wakeup( &sc->tones);

	return(error);
}

/*---------------------------------------------------------------------------*
 *	i4btelioctl - device driver ioctl routine
 *---------------------------------------------------------------------------*/
static int
i4btelioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	int unit = UNIT(dev);
	int func = FUNC(dev);
	int error = 0;
        struct mbuf *m;
        int s;

	tel_sc_t *sc = &tel_sc[unit][func];

	if(func == FUNCTEL)
	{
		switch(cmd)
		{
			case I4B_TEL_GETAUDIOFMT:
				*(int *)data = sc->audiofmt;
				break;
			
			case I4B_TEL_SETAUDIOFMT:
				switch (*(int *)data)
				{
					case CVT_NONE:
						sc->rcvttab = 0;
						sc->wcvttab = 0;
						break;
					case CVT_ALAW2ULAW:
						/* ISDN: a-law */
						/* user: u-law */ 
						sc->rcvttab = a2u_tab;
						sc->wcvttab = u2a_tab;
						break;
					case CVT_ULAW2ALAW:
						/* ISDN: u-law */
						/* user: a-law */ 
						sc->rcvttab = u2a_tab;
						sc->wcvttab = a2u_tab;
						break;
					default:
						error = ENODEV;
						break;
				}
				if(error == 0)
					sc->audiofmt = *(int *)data;
				break;
	
			case I4B_TEL_EMPTYINPUTQUEUE:
				s = splimp();
				while((sc->devstate & ST_CONNECTED)	&&
					(sc->devstate & ST_ISOPEN) 	&&
					!IF_QEMPTY(sc->isdn_linktab->rx_queue))
				{
					IF_DEQUEUE(sc->isdn_linktab->rx_queue, m);
					if(m)
						i4b_Bfreembuf(m);
				}
				splx(s);
				break;

			case I4B_TEL_VR_REQ:
                	{
				msg_vr_req_t *mvr;

				mvr = (msg_vr_req_t *)data;

				mvr->version = VERSION;
				mvr->release = REL;
				mvr->step = STEP;			
				break;
			}
			case I4B_TEL_TONES:
			{
				struct i4b_tel_tones *tt;

				tt = (struct i4b_tel_tones *)data;
				s = splimp();
				while ((sc->devstate & ST_TONE) && 
				    sc->tones.duration[sc->toneidx] != 0) {
					if((error = tsleep( &sc->tones,
					    I4BPRI | PCATCH, "rtone", 0 )) != 0) {
					    	splx(s);
						return(error);
					}
				} 
				if(!(sc->devstate & ST_ISOPEN)) {
					splx(s);
					return (EIO);
				}
				if(!(sc->devstate & ST_CONNECTED)) {
					splx(s);
					return (EIO);
				}

				sc->tones = *tt;
				sc->toneidx = 0;
				sc->tonefreq = tt->frequency[0];
				sc->devstate |= ST_TONE;
				splx(s);
				tel_tone(sc);
				break;
			}
	
			default:
				error = ENOTTY;
				break;
		}
	}
	else if(func == FUNCDIAL)
	{
		switch(cmd)
		{
			default:
				error = ENOTTY;
				break;
		}
	}		
	return(error);
}

/*---------------------------------------------------------------------------*
 *	read from tel device
 *---------------------------------------------------------------------------*/
static int
i4btelread(struct cdev *dev, struct uio *uio, int ioflag)
{
	int unit = UNIT(dev);
	int func = FUNC(dev);

	struct mbuf *m;
	int s;
	int error = 0;

	tel_sc_t *sc = &tel_sc[unit][func];
	
	if(!(sc->devstate & ST_ISOPEN))
		return(EIO);

	if(func == FUNCTEL)
	{
		s = splimp();
		IF_LOCK(sc->isdn_linktab->rx_queue);

		while((sc->devstate & ST_ISOPEN)        &&
		      (sc->devstate & ST_CONNECTED)	&&
		      IF_QEMPTY(sc->isdn_linktab->rx_queue))		
		{
			sc->devstate |= ST_RDWAITDATA;

			NDBGL4(L4_TELDBG, "i4btel%d, queue empty!", unit);

			if((error = msleep( &sc->isdn_linktab->rx_queue,
					&sc->isdn_linktab->rx_queue->ifq_mtx,
					I4BPRI | PCATCH,
					"rtel", 0 )) != 0)
			{
				sc->devstate &= ~ST_RDWAITDATA;
				IF_UNLOCK(sc->isdn_linktab->rx_queue);
				splx(s);
				return(error);
			}
		}
	
		if(!(sc->devstate & ST_ISOPEN))
		{
			IF_UNLOCK(sc->isdn_linktab->rx_queue);
			splx(s);
			return(EIO);
		}
	
		if(!(sc->devstate & ST_CONNECTED))
		{
			IF_UNLOCK(sc->isdn_linktab->rx_queue);
			splx(s);
			return(EIO);
		}
		
	
		_IF_DEQUEUE(sc->isdn_linktab->rx_queue, m);
		IF_UNLOCK(sc->isdn_linktab->rx_queue);
		
		if(m && m->m_len > 0)
		{
			register int i;

		        for(i = 0; i < m->m_len; i++)
		        {
		        	/* always reverse bit order from line */
				mtod(m,u_char *)[i] = bitreverse[mtod(m,u_char *)[i]];

				/* convert if necessary */
				if(sc->rcvttab)
	                                mtod(m,u_char *)[i] = sc->rcvttab[mtod(m,u_char *)[i]];
	                }
			error = uiomove(m->m_data, m->m_len, uio);

			NDBGL4(L4_TELDBG, "i4btel%d, mbuf (%d bytes), uiomove %d!", unit, m->m_len, error);
		}
		else
		{
			NDBGL4(L4_TELDBG, "i4btel%d, empty mbuf from queue!", unit);
			error = EIO;
		}
			
		if(m)
			i4b_Bfreembuf(m);
	
		splx(s);
	}
	else if(func == FUNCDIAL)
	{
		s = splimp();
		while((sc->result == 0) && (sc->devstate & ST_ISOPEN))
		{
			sc->devstate |= ST_RDWAITDATA;
	
			NDBGL4(L4_TELDBG, "i4btel%d, wait for result!", unit);
			
			if((error = tsleep( &sc->result,
						I4BPRI | PCATCH,
						"rtel1", 0 )) != 0)
			{
				sc->devstate &= ~ST_RDWAITDATA;
				splx(s);
				NDBGL4(L4_TELDBG, "i4btel%d, wait for result: sleep error!", unit);
				return(error);
			}
		}
	
		if(!(sc->devstate & ST_ISOPEN))
		{
			splx(s);
			NDBGL4(L4_TELDBG, "i4btel%d, wait for result: device closed!", unit);
			return(EIO);
		}
	
		if(sc->result != 0)
		{
			NDBGL4(L4_TELDBG, "i4btel%d, wait for result: 0x%02x!", unit, sc->result);
			error = uiomove(&sc->result, 1, uio);
			sc->result = 0;
		}
		else
		{
			NDBGL4(L4_TELDBG, "i4btel%d, wait for result: result=0!", unit);
			error = EIO;
		}

		splx(s);			
	}
	return(error);
}

/*---------------------------------------------------------------------------*
 *	write to tel device
 *---------------------------------------------------------------------------*/
static int
i4btelwrite(struct cdev *dev, struct uio * uio, int ioflag)
{
	int unit = UNIT(dev);
	int func = FUNC(dev);
	struct mbuf *m;
	int s;
	int error = 0;
	tel_sc_t *sc = &tel_sc[unit][func];
	
	if(!(sc->devstate & ST_ISOPEN))
	{
		return(EIO);
	}

	if(func == FUNCTEL)
	{
		s = splimp();
		
		if(!(sc->devstate & ST_CONNECTED)) {
			splx(s);
			return(EIO);
		}
			
		sc->devstate &= ~ST_TONE;		
		IF_LOCK(sc->isdn_linktab->tx_queue);
		while((_IF_QFULL(sc->isdn_linktab->tx_queue)) &&
		      (sc->devstate & ST_ISOPEN))
		{
			sc->devstate |= ST_WRWAITEMPTY;

			if((error = msleep( &sc->isdn_linktab->tx_queue,
					&sc->isdn_linktab->tx_queue->ifq_mtx,
					I4BPRI | PCATCH, "wtel", 0)) != 0)
			{
				sc->devstate &= ~ST_WRWAITEMPTY;
				IF_UNLOCK(sc->isdn_linktab->tx_queue);
				splx(s);
				return(error);
			}
		}
		IF_UNLOCK(sc->isdn_linktab->tx_queue);
	
		if(!(sc->devstate & ST_ISOPEN))
		{
			splx(s);
			return(EIO);
		}
	
		if(!(sc->devstate & ST_CONNECTED))
		{
			splx(s);
			return(EIO);
		}

		if((m = i4b_Bgetmbuf(BCH_MAX_DATALEN)) != NULL)
		{
			register int i;
			
			m->m_len = min(BCH_MAX_DATALEN, uio->uio_resid);
	
			error = uiomove(m->m_data, m->m_len, uio);
	
		        for(i = 0; i < m->m_len; i++)
		        {
				/* convert if necessary */
				if(sc->wcvttab)
					mtod(m,u_char *)[i] = sc->wcvttab[mtod(m,u_char *)[i]];

				/* always reverse bitorder to line */
				mtod(m,u_char *)[i] = bitreverse[mtod(m,u_char *)[i]];
			}
			(void) IF_HANDOFF(sc->isdn_linktab->tx_queue, m, NULL);
			(*sc->isdn_linktab->bch_tx_start)(sc->isdn_linktab->unit, sc->isdn_linktab->channel);
		}
	
		splx(s);
	}
	else if(func == FUNCDIAL)
	{
#define CMDBUFSIZ 80 
		char cmdbuf[CMDBUFSIZ];
		int len = min(CMDBUFSIZ-1, uio->uio_resid);
	
		error = uiomove(cmdbuf, len, uio);

		if(cmdbuf[0] == CMD_DIAL)
		{
			i4b_l4_dialoutnumber(BDRV_TEL, unit, len-1, &cmdbuf[1]);
		}
		else if(cmdbuf[0] == CMD_HUP)
		{
			i4b_l4_drvrdisc(BDRV_TEL, unit);
		}
		else if(cmdbuf[0] == CMD_KEYP)
		{
			i4b_l4_keypad(BDRV_TEL, unit, len-1, &cmdbuf[1]);
		}
	}
	else
	{
		error = EIO;
	}		
	
	return(error);
}

/*---------------------------------------------------------------------------*
 *	
 *---------------------------------------------------------------------------*/
#define NTONESAMP 32
static void
tel_tone(tel_sc_t *sc)
{
	struct mbuf *m;
	u_char *p;
	int i;

	if((m = i4b_Bgetmbuf(NTONESAMP)) == NULL) {
		printf("no mbuf in tel_tone\n");
		return;
	}
	p = m->m_data;
	m->m_len = 0;
	for (i = 0; i < NTONESAMP && (sc->devstate & ST_TONE); i++) {

		if (sc->tones.duration[sc->toneidx] > 0) {
			if (--sc->tones.duration[sc->toneidx] == 0) {
				sc->toneidx++;
				if (sc->toneidx == I4B_TEL_MAXTONES) {
					sc->devstate &= ~ST_TONE;
					sc->toneomega = 0;
					sc->tonefreq = 0;
				} else if (sc->tones.frequency[sc->toneidx] == 0 &&
					   sc->tones.duration[sc->toneidx] == 0) {
					sc->devstate &= ~ST_TONE;
					sc->toneomega = 0;
					sc->tonefreq = 0;
				} else {
					sc->tonefreq = sc->tones.frequency[sc->toneidx];
				}
				if (sc->tones.duration[sc->toneidx] == 0) {
					wakeup( &sc->tones);
				}
			}
		}

		sc->toneomega += sc->tonefreq;
		if (sc->toneomega >= 8000)
			sc->toneomega -= 8000;
		*p++ = bitreverse[sinetab[sc->toneomega]];
		m->m_len++;
	}
	IF_ENQUEUE(sc->isdn_linktab->tx_queue, m);
	(*sc->isdn_linktab->bch_tx_start)(sc->isdn_linktab->unit, sc->isdn_linktab->channel);
}

/*---------------------------------------------------------------------------*
 *	device driver poll
 *---------------------------------------------------------------------------*/
static int
i4btelpoll(struct cdev *dev, int events, struct thread *td)
{
	int revents = 0;	/* Events we found */
	int s;
	int unit = UNIT(dev);
	int func = FUNC(dev);	

	tel_sc_t *sc = &tel_sc[unit][func];
	
	s = splhigh();

	if(!(sc->devstate & ST_ISOPEN))
	{
		NDBGL4(L4_TELDBG, "i4btel%d, !ST_ISOPEN", unit);
		splx(s);
		return(0);
	}

	if(func == FUNCTEL)
	{
		/*
		 * Writes are OK if we are connected and the
	         * transmit queue can take them
		 */
		 
		if((events & (POLLOUT|POLLWRNORM))	&&
			(sc->devstate & ST_CONNECTED)	&&
			(sc->isdn_linktab != NULL)	&&
			(!_IF_QFULL(sc->isdn_linktab->tx_queue)))
		{
			NDBGL4(L4_TELDBG, "i4btel%d, POLLOUT", unit);
			revents |= (events & (POLLOUT|POLLWRNORM));
		}
		
		/* ... while reads are OK if we have any data */
	
		if((events & (POLLIN|POLLRDNORM))	&&
			(sc->devstate & ST_CONNECTED)	&&
			(sc->isdn_linktab != NULL)	&&
			(!IF_QEMPTY(sc->isdn_linktab->rx_queue)))
		{
			NDBGL4(L4_TELDBG, "i4btel%d, POLLIN", unit);
			revents |= (events & (POLLIN|POLLRDNORM));
		}
			
		if(revents == 0)
		{
			NDBGL4(L4_TELDBG, "i4btel%d, selrecord", unit);
			selrecord(td, &sc->selp);
		}
	}
	else if(func == FUNCDIAL)
	{
		if(events & (POLLOUT|POLLWRNORM))
		{
			NDBGL4(L4_TELDBG, "i4bteld%d,  POLLOUT", unit);
			revents |= (events & (POLLOUT|POLLWRNORM));
		}

		if(events & (POLLIN|POLLRDNORM))
		{
			NDBGL4(L4_TELDBG, "i4bteld%d,  POLLIN, result = %d", unit, sc->result);
			if(sc->result != 0)
				revents |= (events & (POLLIN|POLLRDNORM));
		}
			
		if(revents == 0)
		{
			NDBGL4(L4_TELDBG, "i4bteld%d,  selrecord", unit);
			selrecord(td, &sc->selp);
		}
	}
	splx(s);
	return(revents);
}

/*===========================================================================*
 *			ISDN INTERFACE ROUTINES
 *===========================================================================*/

/*---------------------------------------------------------------------------*
*	this routine is called from L4 handler at connect time
 *---------------------------------------------------------------------------*/
static void
tel_connect(int unit, void *cdp)
{
	tel_sc_t *sc = &tel_sc[unit][FUNCTEL];

	/* audio device */
	
	sc->cdp = (call_desc_t *)cdp;

	sc->devstate |= ST_CONNECTED;

	/* dialer device */
	
	sc = &tel_sc[unit][FUNCDIAL];

	if(sc->devstate & ST_ISOPEN)
	{
		NDBGL4(L4_TELDBG, "i4btel%d, tel_connect!", unit);		
		sc->result = RSP_CONN;

		if(sc->devstate & ST_RDWAITDATA)
		{
			sc->devstate &= ~ST_RDWAITDATA;
			wakeup( &sc->result);
		}
		selwakeuppri(&sc->selp, I4BPRI);
	}
}

/*---------------------------------------------------------------------------*
 *	this routine is called from L4 handler at disconnect time
 *---------------------------------------------------------------------------*/
static void
tel_disconnect(int unit, void *cdp)
{
/*	call_desc_t *cd = (call_desc_t *)cdp; */

	tel_sc_t *sc = &tel_sc[unit][FUNCTEL];
	
	/* audio device */
	
	sc->devstate &= ~ST_CONNECTED;

	if(sc->devstate & ST_RDWAITDATA)
	{
		sc->devstate &= ~ST_RDWAITDATA;
		wakeup( &sc->isdn_linktab->rx_queue);
	}

	if(sc->devstate & ST_WRWAITEMPTY)
	{
		sc->devstate &= ~ST_WRWAITEMPTY;
		wakeup( &sc->isdn_linktab->tx_queue);
	}

	/* dialer device */
	
	sc = &tel_sc[unit][FUNCDIAL];

	if(sc->devstate & ST_ISOPEN)
	{
		NDBGL4(L4_TELDBG, "i4btel%d, tel_disconnect!", unit);
		sc->result = RSP_HUP;

		if(sc->devstate & ST_RDWAITDATA)
		{
			sc->devstate &= ~ST_RDWAITDATA;
			wakeup( &sc->result);
		}
		selwakeuppri(&sc->selp, I4BPRI);

		if (sc->devstate & ST_TONE) {
			sc->devstate &= ~ST_TONE;
			wakeup( &sc->tones);
		}
	}
}

/*---------------------------------------------------------------------------*
 *	feedback from daemon in case of dial problems
 *---------------------------------------------------------------------------*/
static void
tel_dialresponse(int unit, int status, cause_t cause)
{	
	tel_sc_t *sc = &tel_sc[unit][FUNCDIAL];

	NDBGL4(L4_TELDBG, "i4btel%d,  status=%d, cause=0x%4x", unit, status, cause);

	if((sc->devstate == ST_ISOPEN) && status)
	{
		NDBGL4(L4_TELDBG, "i4btel%d, tel_dialresponse!", unit);		
		sc->result = RSP_NOA;

		if(sc->devstate & ST_RDWAITDATA)
		{
			sc->devstate &= ~ST_RDWAITDATA;
			wakeup( &sc->result);
		}
		selwakeuppri(&sc->selp, I4BPRI);
	}
}
	
/*---------------------------------------------------------------------------*
 *	interface up/down
 *---------------------------------------------------------------------------*/
static void
tel_updown(int unit, int updown)
{
}
	
/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	when a new frame (mbuf) has been received and was put on
 *	the rx queue.
 *---------------------------------------------------------------------------*/
static void
tel_rx_data_rdy(int unit)
{
	tel_sc_t *sc = &tel_sc[unit][FUNCTEL];
	
	if(sc->devstate & ST_RDWAITDATA)
	{
		sc->devstate &= ~ST_RDWAITDATA;
		wakeup( &sc->isdn_linktab->rx_queue);
	}
	selwakeuppri(&sc->selp, TTOPRI);
}

/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	when the last frame has been sent out and there is no
 *	further frame (mbuf) in the tx queue.
 *---------------------------------------------------------------------------*/
static void
tel_tx_queue_empty(int unit)
{
	tel_sc_t *sc = &tel_sc[unit][FUNCTEL];

	if(sc->devstate & ST_WRWAITEMPTY)
	{
		sc->devstate &= ~ST_WRWAITEMPTY;
		wakeup( &sc->isdn_linktab->tx_queue);
	}
	if(sc->devstate & ST_TONE) {
		tel_tone(sc);
	} else {
		selwakeuppri(&sc->selp, I4BPRI);
	}
}

/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	each time a packet is received or transmitted.
 *---------------------------------------------------------------------------*/
static void
tel_activity(int unit, int rxtx)
{
	if(tel_sc[unit][FUNCTEL].cdp)
		tel_sc[unit][FUNCTEL].cdp->last_active_time = SECOND;
}

/*---------------------------------------------------------------------------*
 *	return this drivers linktab address
 *---------------------------------------------------------------------------*/
drvr_link_t *
tel_ret_linktab(int unit)
{
	tel_sc_t *sc = &tel_sc[unit][FUNCTEL];
	
	tel_init_linktab(unit);
	return(&sc->drvr_linktab);
}

/*---------------------------------------------------------------------------*
 *	setup the isdn_linktab for this driver
 *---------------------------------------------------------------------------*/
void
tel_set_linktab(int unit, isdn_link_t *ilt)
{
	tel_sc_t *sc = &tel_sc[unit][FUNCTEL];
	sc->isdn_linktab = ilt;
}

/*---------------------------------------------------------------------------*
 *	initialize this drivers linktab
 *---------------------------------------------------------------------------*/
static void
tel_init_linktab(int unit)
{
	tel_sc_t *sc = &tel_sc[unit][FUNCTEL];
	
	sc->drvr_linktab.unit = unit;
	sc->drvr_linktab.bch_rx_data_ready = tel_rx_data_rdy;
	sc->drvr_linktab.bch_tx_queue_empty = tel_tx_queue_empty;
	sc->drvr_linktab.bch_activity = tel_activity;	
	sc->drvr_linktab.line_connected = tel_connect;
	sc->drvr_linktab.line_disconnected = tel_disconnect;
	sc->drvr_linktab.dial_response = tel_dialresponse;
	sc->drvr_linktab.updown_ind = tel_updown;	
}

/*===========================================================================*
 *	AUDIO FORMAT CONVERSION (produced by running g711conv)
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	A-law to u-law conversion
 *---------------------------------------------------------------------------*/
static unsigned char a2u_tab[256] = {
/* 00 */	0x2a, 0x2b, 0x28, 0x29, 0x2e, 0x2f, 0x2c, 0x2d, 
/* 08 */	0x22, 0x23, 0x20, 0x21, 0x26, 0x27, 0x24, 0x25, 
/* 10 */	0x39, 0x3a, 0x37, 0x38, 0x3d, 0x3e, 0x3b, 0x3c, 
/* 18 */	0x31, 0x32, 0x30, 0x30, 0x35, 0x36, 0x33, 0x34, 
/* 20 */	0x0a, 0x0b, 0x08, 0x09, 0x0e, 0x0f, 0x0c, 0x0d, 
/* 28 */	0x02, 0x03, 0x00, 0x01, 0x06, 0x07, 0x04, 0x05, 
/* 30 */	0x1a, 0x1b, 0x18, 0x19, 0x1e, 0x1f, 0x1c, 0x1d, 
/* 38 */	0x12, 0x13, 0x10, 0x11, 0x16, 0x17, 0x14, 0x15, 
/* 40 */	0x62, 0x63, 0x60, 0x61, 0x66, 0x67, 0x64, 0x65, 
/* 48 */	0x5d, 0x5d, 0x5c, 0x5c, 0x5f, 0x5f, 0x5e, 0x5e, 
/* 50 */	0x74, 0x76, 0x70, 0x72, 0x7c, 0x7e, 0x78, 0x7a, 
/* 58 */	0x6a, 0x6b, 0x68, 0x69, 0x6e, 0x6f, 0x6c, 0x6d, 
/* 60 */	0x48, 0x49, 0x46, 0x47, 0x4c, 0x4d, 0x4a, 0x4b, 
/* 68 */	0x40, 0x41, 0x3f, 0x3f, 0x44, 0x45, 0x42, 0x43, 
/* 70 */	0x56, 0x57, 0x54, 0x55, 0x5a, 0x5b, 0x58, 0x59, 
/* 78 */	0x4f, 0x4f, 0x4e, 0x4e, 0x52, 0x53, 0x50, 0x51, 
/* 80 */	0xaa, 0xab, 0xa8, 0xa9, 0xae, 0xaf, 0xac, 0xad, 
/* 88 */	0xa2, 0xa3, 0xa0, 0xa1, 0xa6, 0xa7, 0xa4, 0xa5, 
/* 90 */	0xb9, 0xba, 0xb7, 0xb8, 0xbd, 0xbe, 0xbb, 0xbc, 
/* 98 */	0xb1, 0xb2, 0xb0, 0xb0, 0xb5, 0xb6, 0xb3, 0xb4, 
/* a0 */	0x8a, 0x8b, 0x88, 0x89, 0x8e, 0x8f, 0x8c, 0x8d, 
/* a8 */	0x82, 0x83, 0x80, 0x81, 0x86, 0x87, 0x84, 0x85, 
/* b0 */	0x9a, 0x9b, 0x98, 0x99, 0x9e, 0x9f, 0x9c, 0x9d, 
/* b8 */	0x92, 0x93, 0x90, 0x91, 0x96, 0x97, 0x94, 0x95, 
/* c0 */	0xe2, 0xe3, 0xe0, 0xe1, 0xe6, 0xe7, 0xe4, 0xe5, 
/* c8 */	0xdd, 0xdd, 0xdc, 0xdc, 0xdf, 0xdf, 0xde, 0xde, 
/* d0 */	0xf4, 0xf6, 0xf0, 0xf2, 0xfc, 0xfe, 0xf8, 0xfa, 
/* d8 */	0xea, 0xeb, 0xe8, 0xe9, 0xee, 0xef, 0xec, 0xed, 
/* e0 */	0xc8, 0xc9, 0xc6, 0xc7, 0xcc, 0xcd, 0xca, 0xcb, 
/* e8 */	0xc0, 0xc1, 0xbf, 0xbf, 0xc4, 0xc5, 0xc2, 0xc3, 
/* f0 */	0xd6, 0xd7, 0xd4, 0xd5, 0xda, 0xdb, 0xd8, 0xd9, 
/* f8 */	0xcf, 0xcf, 0xce, 0xce, 0xd2, 0xd3, 0xd0, 0xd1
};

/*---------------------------------------------------------------------------*
 *	u-law to A-law conversion
 *---------------------------------------------------------------------------*/
static unsigned char u2a_tab[256] = {
/* 00 */	0x2a, 0x2b, 0x28, 0x29, 0x2e, 0x2f, 0x2c, 0x2d, 
/* 08 */	0x22, 0x23, 0x20, 0x21, 0x26, 0x27, 0x24, 0x25, 
/* 10 */	0x3a, 0x3b, 0x38, 0x39, 0x3e, 0x3f, 0x3c, 0x3d, 
/* 18 */	0x32, 0x33, 0x30, 0x31, 0x36, 0x37, 0x34, 0x35, 
/* 20 */	0x0a, 0x0b, 0x08, 0x09, 0x0e, 0x0f, 0x0c, 0x0d, 
/* 28 */	0x02, 0x03, 0x00, 0x01, 0x06, 0x07, 0x04, 0x05, 
/* 30 */	0x1b, 0x18, 0x19, 0x1e, 0x1f, 0x1c, 0x1d, 0x12, 
/* 38 */	0x13, 0x10, 0x11, 0x16, 0x17, 0x14, 0x15, 0x6a, 
/* 40 */	0x68, 0x69, 0x6e, 0x6f, 0x6c, 0x6d, 0x62, 0x63, 
/* 48 */	0x60, 0x61, 0x66, 0x67, 0x64, 0x65, 0x7a, 0x78, 
/* 50 */	0x7e, 0x7f, 0x7c, 0x7d, 0x72, 0x73, 0x70, 0x71, 
/* 58 */	0x76, 0x77, 0x74, 0x75, 0x4b, 0x49, 0x4f, 0x4d, 
/* 60 */	0x42, 0x43, 0x40, 0x41, 0x46, 0x47, 0x44, 0x45, 
/* 68 */	0x5a, 0x5b, 0x58, 0x59, 0x5e, 0x5f, 0x5c, 0x5d, 
/* 70 */	0x52, 0x52, 0x53, 0x53, 0x50, 0x50, 0x51, 0x51, 
/* 78 */	0x56, 0x56, 0x57, 0x57, 0x54, 0x54, 0x55, 0x55, 
/* 80 */	0xaa, 0xab, 0xa8, 0xa9, 0xae, 0xaf, 0xac, 0xad, 
/* 88 */	0xa2, 0xa3, 0xa0, 0xa1, 0xa6, 0xa7, 0xa4, 0xa5, 
/* 90 */	0xba, 0xbb, 0xb8, 0xb9, 0xbe, 0xbf, 0xbc, 0xbd, 
/* 98 */	0xb2, 0xb3, 0xb0, 0xb1, 0xb6, 0xb7, 0xb4, 0xb5, 
/* a0 */	0x8a, 0x8b, 0x88, 0x89, 0x8e, 0x8f, 0x8c, 0x8d, 
/* a8 */	0x82, 0x83, 0x80, 0x81, 0x86, 0x87, 0x84, 0x85, 
/* b0 */	0x9b, 0x98, 0x99, 0x9e, 0x9f, 0x9c, 0x9d, 0x92, 
/* b8 */	0x93, 0x90, 0x91, 0x96, 0x97, 0x94, 0x95, 0xea, 
/* c0 */	0xe8, 0xe9, 0xee, 0xef, 0xec, 0xed, 0xe2, 0xe3, 
/* c8 */	0xe0, 0xe1, 0xe6, 0xe7, 0xe4, 0xe5, 0xfa, 0xf8, 
/* d0 */	0xfe, 0xff, 0xfc, 0xfd, 0xf2, 0xf3, 0xf0, 0xf1, 
/* d8 */	0xf6, 0xf7, 0xf4, 0xf5, 0xcb, 0xc9, 0xcf, 0xcd, 
/* e0 */	0xc2, 0xc3, 0xc0, 0xc1, 0xc6, 0xc7, 0xc4, 0xc5, 
/* e8 */	0xda, 0xdb, 0xd8, 0xd9, 0xde, 0xdf, 0xdc, 0xdd, 
/* f0 */	0xd2, 0xd2, 0xd3, 0xd3, 0xd0, 0xd0, 0xd1, 0xd1, 
/* f8 */	0xd6, 0xd6, 0xd7, 0xd7, 0xd4, 0xd4, 0xd5, 0xd5
};
  
/*---------------------------------------------------------------------------*
 *	reverse bits in a byte
 *---------------------------------------------------------------------------*/
static unsigned char bitreverse[256] = {
/* 00 */	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 
/* 08 */	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0, 
/* 10 */	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 
/* 18 */	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 
/* 20 */	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 
/* 28 */	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4, 
/* 30 */	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 
/* 38 */	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc, 
/* 40 */	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 
/* 48 */	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2, 
/* 50 */	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 
/* 58 */	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa, 
/* 60 */	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 
/* 68 */	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6, 
/* 70 */	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 
/* 78 */	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 
/* 80 */	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 
/* 88 */	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1, 
/* 90 */	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 
/* 98 */	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9, 
/* a0 */	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 
/* a8 */	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 
/* b0 */	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 
/* b8 */	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd, 
/* c0 */	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 
/* c8 */	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3, 
/* d0 */	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 
/* d8 */	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, 
/* e0 */	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 
/* e8 */	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7, 
/* f0 */	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 
/* f8 */	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

static u_char sinetab[8000] = { 213, 213, 213, 213, 213, 213, 213, 212,
212, 212, 212, 212, 212, 215, 215, 215, 215, 215, 215, 214, 214,
214, 214, 214, 214, 209, 209, 209, 209, 209, 209, 209, 208, 208,
208, 208, 208, 208, 211, 211, 211, 211, 211, 211, 210, 210, 210,
210, 210, 210, 221, 221, 221, 221, 221, 221, 220, 220, 220, 220,
220, 220, 220, 223, 223, 223, 223, 223, 223, 222, 222, 222, 222,
222, 222, 217, 217, 217, 217, 217, 217, 216, 216, 216, 216, 216,
216, 216, 219, 219, 219, 219, 219, 219, 218, 218, 218, 218, 218,
218, 197, 197, 197, 197, 197, 197, 196, 196, 196, 196, 196, 196,
196, 199, 199, 199, 199, 199, 199, 198, 198, 198, 198, 198, 198,
193, 193, 193, 193, 193, 193, 192, 192, 192, 192, 192, 192, 192,
195, 195, 195, 195, 195, 195, 194, 194, 194, 194, 194, 194, 205,
205, 205, 205, 205, 205, 204, 204, 204, 204, 204, 204, 204, 207,
207, 207, 207, 207, 207, 206, 206, 206, 206, 206, 206, 201, 201,
201, 201, 201, 201, 200, 200, 200, 200, 200, 200, 200, 203, 203,
203, 203, 203, 203, 202, 202, 202, 202, 202, 202, 245, 245, 245,
245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 244, 244, 244,
244, 244, 244, 244, 244, 244, 244, 244, 244, 247, 247, 247, 247,
247, 247, 247, 247, 247, 247, 247, 247, 247, 246, 246, 246, 246,
246, 246, 246, 246, 246, 246, 246, 246, 246, 241, 241, 241, 241,
241, 241, 241, 241, 241, 241, 241, 241, 240, 240, 240, 240, 240,
240, 240, 240, 240, 240, 240, 240, 240, 243, 243, 243, 243, 243,
243, 243, 243, 243, 243, 243, 243, 243, 242, 242, 242, 242, 242,
242, 242, 242, 242, 242, 242, 242, 242, 253, 253, 253, 253, 253,
253, 253, 253, 253, 253, 253, 253, 253, 252, 252, 252, 252, 252,
252, 252, 252, 252, 252, 252, 252, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 254, 254, 254,
254, 254, 254, 254, 254, 254, 254, 249, 249, 249, 249, 249, 249,
249, 249, 249, 249, 249, 249, 249, 248, 248, 248, 248, 248, 248,
248, 248, 248, 248, 248, 248, 248, 251, 251, 251, 251, 251, 251,
251, 251, 251, 251, 251, 251, 251, 250, 250, 250, 250, 250, 250,
250, 250, 250, 250, 250, 250, 250, 229, 229, 229, 229, 229, 229,
229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229,
229, 229, 229, 229, 229, 229, 229, 228, 228, 228, 228, 228, 228,
228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228,
228, 228, 228, 228, 228, 228, 228, 228, 231, 231, 231, 231, 231,
231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231,
231, 231, 231, 231, 231, 231, 231, 231, 231, 230, 230, 230, 230,
230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
230, 230, 230, 230, 230, 230, 230, 230, 230, 225, 225, 225, 225,
225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 224, 224,
224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 227,
227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227,
227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227,
227, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226,
226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226,
226, 226, 226, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237,
237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237,
237, 237, 237, 237, 237, 236, 236, 236, 236, 236, 236, 236, 236,
236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
236, 236, 236, 236, 236, 236, 236, 236, 239, 239, 239, 239, 239,
239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 238, 238,
238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
238, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233,
233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233,
233, 233, 233, 233, 233, 232, 232, 232, 232, 232, 232, 232, 232,
232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232,
232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 235, 235, 235,
235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235,
235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235,
235, 235, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234,
234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234,
234, 234, 234, 234, 234, 234, 234, 149, 149, 149, 149, 149, 149,
149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
149, 149, 149, 149, 149, 149, 149, 148, 148, 148, 148, 148, 148,
148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 151, 151, 151,
151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
151, 151, 151, 151, 150, 150, 150, 150, 150, 150, 150, 150, 150,
150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
150, 150, 150, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
145, 145, 145, 145, 145, 145, 145, 145, 144, 144, 144, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
156, 156, 156, 156, 156, 156, 156, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
157, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 145,
145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
145, 145, 145, 145, 150, 150, 150, 150, 150, 150, 150, 150, 150,
150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
150, 150, 150, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 148, 148, 148,
148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234,
234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234,
234, 234, 234, 234, 234, 235, 235, 235, 235, 235, 235, 235, 235,
235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235,
235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 232, 232, 232,
232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232,
232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232,
232, 232, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233,
233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233,
233, 233, 233, 233, 233, 233, 238, 238, 238, 238, 238, 238, 238,
238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
238, 238, 238, 238, 238, 238, 238, 238, 238, 239, 239, 239, 239,
239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 236,
236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
236, 236, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237,
237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237,
237, 237, 237, 237, 226, 226, 226, 226, 226, 226, 226, 226, 226,
226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226,
226, 226, 226, 226, 226, 226, 227, 227, 227, 227, 227, 227, 227,
227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227,
227, 227, 227, 227, 227, 227, 227, 227, 224, 224, 224, 224, 224,
224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
224, 224, 224, 224, 224, 224, 224, 224, 224, 225, 225, 225, 225,
225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 230, 230,
230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 231, 231,
231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231,
231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 228,
228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228,
228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228,
229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229,
229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229,
250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 253,
253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 242,
242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 243,
243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 240,
240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 241,
241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 246, 246,
246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 247, 247,
247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 244, 244,
244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 245, 245, 245,
245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 202, 202, 202,
202, 202, 202, 203, 203, 203, 203, 203, 203, 200, 200, 200, 200,
200, 200, 200, 201, 201, 201, 201, 201, 201, 206, 206, 206, 206,
206, 206, 207, 207, 207, 207, 207, 207, 204, 204, 204, 204, 204,
204, 204, 205, 205, 205, 205, 205, 205, 194, 194, 194, 194, 194,
194, 195, 195, 195, 195, 195, 195, 192, 192, 192, 192, 192, 192,
192, 193, 193, 193, 193, 193, 193, 198, 198, 198, 198, 198, 198,
199, 199, 199, 199, 199, 199, 196, 196, 196, 196, 196, 196, 196,
197, 197, 197, 197, 197, 197, 218, 218, 218, 218, 218, 218, 219,
219, 219, 219, 219, 219, 216, 216, 216, 216, 216, 216, 216, 217,
217, 217, 217, 217, 217, 222, 222, 222, 222, 222, 222, 223, 223,
223, 223, 223, 223, 220, 220, 220, 220, 220, 220, 220, 221, 221,
221, 221, 221, 221, 210, 210, 210, 210, 210, 210, 211, 211, 211,
211, 211, 211, 208, 208, 208, 208, 208, 208, 209, 209, 209, 209,
209, 209, 209, 214, 214, 214, 214, 214, 214, 215, 215, 215, 215,
215, 215, 212, 212, 212, 212, 212, 212, 213, 213, 213, 213, 213,
213, 213, 90, 90, 90, 85, 85, 85, 85, 85, 85, 84, 84, 84, 84, 84,
84, 87, 87, 87, 87, 87, 87, 86, 86, 86, 86, 86, 86, 81, 81, 81,
81, 81, 81, 81, 80, 80, 80, 80, 80, 80, 83, 83, 83, 83, 83, 83,
82, 82, 82, 82, 82, 82, 93, 93, 93, 93, 93, 93, 93, 92, 92, 92,
92, 92, 92, 95, 95, 95, 95, 95, 95, 94, 94, 94, 94, 94, 94, 89,
89, 89, 89, 89, 89, 88, 88, 88, 88, 88, 88, 88, 91, 91, 91, 91,
91, 91, 90, 90, 90, 90, 90, 90, 69, 69, 69, 69, 69, 69, 68, 68,
68, 68, 68, 68, 68, 71, 71, 71, 71, 71, 71, 70, 70, 70, 70, 70,
70, 65, 65, 65, 65, 65, 65, 64, 64, 64, 64, 64, 64, 64, 67, 67,
67, 67, 67, 67, 66, 66, 66, 66, 66, 66, 77, 77, 77, 77, 77, 77,
76, 76, 76, 76, 76, 76, 76, 79, 79, 79, 79, 79, 79, 78, 78, 78,
78, 78, 78, 73, 73, 73, 73, 73, 73, 73, 72, 72, 72, 72, 72, 72,
75, 75, 75, 75, 75, 75, 74, 74, 74, 74, 74, 74, 117, 117, 117, 117,
117, 117, 117, 117, 117, 117, 117, 117, 117, 116, 116, 116, 116,
116, 116, 116, 116, 116, 116, 116, 116, 116, 119, 119, 119, 119,
119, 119, 119, 119, 119, 119, 119, 119, 118, 118, 118, 118, 118,
118, 118, 118, 118, 118, 118, 118, 118, 113, 113, 113, 113, 113,
113, 113, 113, 113, 113, 113, 113, 113, 112, 112, 112, 112, 112,
112, 112, 112, 112, 112, 112, 112, 115, 115, 115, 115, 115, 115,
115, 115, 115, 115, 115, 115, 115, 114, 114, 114, 114, 114, 114,
114, 114, 114, 114, 114, 114, 114, 125, 125, 125, 125, 125, 125,
125, 125, 125, 125, 125, 125, 125, 124, 124, 124, 124, 124, 124,
124, 124, 124, 124, 124, 124, 124, 127, 127, 127, 127, 127, 127,
127, 127, 127, 127, 127, 127, 126, 126, 126, 126, 126, 126, 126,
126, 126, 126, 126, 126, 126, 121, 121, 121, 121, 121, 121, 121,
121, 121, 121, 121, 121, 121, 120, 120, 120, 120, 120, 120, 120,
120, 120, 120, 120, 120, 120, 123, 123, 123, 123, 123, 123, 123,
123, 123, 123, 123, 123, 123, 122, 122, 122, 122, 122, 122, 122,
122, 122, 122, 122, 122, 122, 101, 101, 101, 101, 101, 101, 101,
101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101,
101, 101, 101, 101, 101, 101, 101, 100, 100, 100, 100, 100, 100,
100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
100, 100, 100, 100, 100, 100, 100, 103, 103, 103, 103, 103, 103,
103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103,
103, 103, 103, 103, 103, 103, 103, 103, 102, 102, 102, 102, 102,
102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
102, 102, 102, 102, 102, 102, 102, 102, 102, 97, 97, 97, 97, 97,
97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97,
97, 97, 97, 97, 97, 97, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
96, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98,
98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
98, 98, 98, 98, 98, 98, 98, 98, 98, 109, 109, 109, 109, 109, 109,
109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 108, 108, 108,
108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108,
108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 111,
111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
111, 111, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110,
110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110,
110, 110, 110, 110, 110, 110, 105, 105, 105, 105, 105, 105, 105,
105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105,
105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 104, 104, 104,
104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104,
104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104,
104, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
107, 107, 107, 107, 107, 107, 106, 106, 106, 106, 106, 106, 106,
106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106,
106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 21,
21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
20, 20, 20, 20, 20, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 22, 22, 22,
22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 17, 17, 17, 17, 17, 17,
17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 16, 16,
16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 19, 19, 19, 19, 19, 19,
19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
19, 19, 19, 19, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19,
19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
19, 19, 19, 19, 19, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
17, 17, 17, 17, 17, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 20, 20, 20, 20, 20, 20,
20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 21,
21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106,
106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106,
106, 106, 106, 106, 106, 106, 107, 107, 107, 107, 107, 107, 107,
107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 104, 104,
104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104,
104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104,
104, 104, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105,
105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105,
105, 105, 105, 105, 105, 105, 110, 110, 110, 110, 110, 110, 110,
110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110,
110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 111, 111, 111,
111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108,
108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108,
108, 108, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
109, 109, 109, 109, 109, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
98, 98, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 96, 96,
96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
96, 96, 96, 96, 96, 96, 96, 96, 96, 97, 97, 97, 97, 97, 97, 97,
97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97,
97, 97, 97, 97, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
102, 102, 102, 102, 103, 103, 103, 103, 103, 103, 103, 103, 103,
103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103,
103, 103, 103, 103, 103, 100, 100, 100, 100, 100, 100, 100, 100,
100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
100, 100, 100, 100, 100, 101, 101, 101, 101, 101, 101, 101, 101,
101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101,
101, 101, 101, 101, 101, 101, 122, 122, 122, 122, 122, 122, 122,
122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123,
123, 123, 123, 123, 123, 123, 120, 120, 120, 120, 120, 120, 120,
120, 120, 120, 120, 120, 120, 121, 121, 121, 121, 121, 121, 121,
121, 121, 121, 121, 121, 121, 126, 126, 126, 126, 126, 126, 126,
126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127,
127, 127, 127, 127, 127, 124, 124, 124, 124, 124, 124, 124, 124,
124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125,
125, 125, 125, 125, 125, 114, 114, 114, 114, 114, 114, 114, 114,
114, 114, 114, 114, 114, 115, 115, 115, 115, 115, 115, 115, 115,
115, 115, 115, 115, 115, 112, 112, 112, 112, 112, 112, 112, 112,
112, 112, 112, 112, 113, 113, 113, 113, 113, 113, 113, 113, 113,
113, 113, 113, 113, 118, 118, 118, 118, 118, 118, 118, 118, 118,
118, 118, 118, 118, 119, 119, 119, 119, 119, 119, 119, 119, 119,
119, 119, 119, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116,
116, 116, 116, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
117, 117, 117, 74, 74, 74, 74, 74, 74, 75, 75, 75, 75, 75, 75, 72,
72, 72, 72, 72, 72, 73, 73, 73, 73, 73, 73, 73, 78, 78, 78, 78,
78, 78, 79, 79, 79, 79, 79, 79, 76, 76, 76, 76, 76, 76, 76, 77,
77, 77, 77, 77, 77, 66, 66, 66, 66, 66, 66, 67, 67, 67, 67, 67,
67, 64, 64, 64, 64, 64, 64, 64, 65, 65, 65, 65, 65, 65, 70, 70,
70, 70, 70, 70, 71, 71, 71, 71, 71, 71, 68, 68, 68, 68, 68, 68,
68, 69, 69, 69, 69, 69, 69, 90, 90, 90, 90, 90, 90, 91, 91, 91,
91, 91, 91, 88, 88, 88, 88, 88, 88, 88, 89, 89, 89, 89, 89, 89,
94, 94, 94, 94, 94, 94, 95, 95, 95, 95, 95, 95, 92, 92, 92, 92,
92, 92, 93, 93, 93, 93, 93, 93, 93, 82, 82, 82, 82, 82, 82, 83,
83, 83, 83, 83, 83, 80, 80, 80, 80, 80, 80, 81, 81, 81, 81, 81,
81, 81, 86, 86, 86, 86, 86, 86, 87, 87, 87, 87, 87, 87, 84, 84,
84, 84, 84, 84, 85, 85, 85, 85, 85, 85, 90, 90, 90 };

/*===========================================================================*/
