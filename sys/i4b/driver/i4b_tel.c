/*
 * Copyright (c) 1997, 1998 Hellmuth Michaelis. All rights reserved.
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
 *	i4b_tel.c - device driver for ISDN telephony
 *	--------------------------------------------
 *
 *	$Id: i4b_tel.c,v 1.18 1998/12/14 10:31:53 hm Exp $
 *
 *	last edit-date: [Mon Dec 14 11:32:06 1998]
 *
 *---------------------------------------------------------------------------*/

#include "i4btel.h"

#if NI4BTEL > 0

#undef I4BTELDEBUG

#include <sys/param.h>
#include <sys/systm.h>

#if defined(__FreeBSD_version) && __FreeBSD_version >= 300001
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif

#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/proc.h>
#include <sys/tty.h>

#ifdef __FreeBSD__
#include "opt_devfs.h"
#endif

#ifdef DEVFS
#include <sys/devfsext.h>
#endif

#ifdef __FreeBSD__
#include <machine/i4b_ioctl.h>
#include <machine/i4b_tel_ioctl.h>
#else
#include <i4b/i4b_ioctl.h>
#include <i4b/i4b_tel_ioctl.h>
#endif

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_l3l4.h>

typedef struct {
	drvr_link_t		drvr_linktab;	/* driver linktab	*/
	isdn_link_t 		*isdn_linktab;	/* isdn linktab		*/
	int 			devstate;	/* state of this unit	*/
#define ST_IDLE		0x00		/* idle */
#define ST_CONNECTED	0x01		/* isdn connected state */
#define ST_ISOPEN	0x02		/* userland opened */
#define ST_RDWAITDATA	0x04		/* userland read waiting */
#define ST_WRWAITEMPTY	0x08		/* userland write waiting */
	int 			audiofmt;	/* audio format conversion */
	call_desc_t		*cdp;		/* call descriptor pointer */
#ifdef DEVFS
        void                    *devfs_token;   /* token for DEVFS */
#endif
} tel_sc_t;

static tel_sc_t tel_sc[NI4BTEL];
	
/* forward decl */

static void tel_rx_data_rdy(int unit);
static void tel_tx_queue_empty(int unit);
static void tel_init_linktab(int unit);
static void tel_connect(int unit, void *cdp);
static void tel_disconnect(int unit, void *cdp);

static unsigned char alaw_ulaw[];
static unsigned char ulaw_alaw[];

#ifndef __FreeBSD__
#define	PDEVSTATIC	/* - not static - */
PDEVSTATIC void i4btelattach __P((void));
PDEVSTATIC int i4btelioctl __P((dev_t dev, int cmd, caddr_t data, int flag, struct proc *p));
int i4btelopen __P((dev_t dev, int flag, int fmt, struct proc *p));
int i4btelclose __P((dev_t dev, int flag, int fmt, struct proc *p));
int i4btelread __P((dev_t dev, struct uio *uio, int ioflag));
int i4btelwrite __P((dev_t dev, struct uio * uio, int ioflag));
#endif
#if BSD > 199306 && defined(__FreeBSD__)
#define PDEVSTATIC	static
PDEVSTATIC d_open_t	i4btelopen;
PDEVSTATIC d_close_t i4btelclose;
PDEVSTATIC d_read_t i4btelread;
PDEVSTATIC d_read_t i4btelwrite;
PDEVSTATIC d_ioctl_t i4btelioctl;
#if defined(__FreeBSD_version) && __FreeBSD_version >= 300001
PDEVSTATIC d_poll_t i4btelpoll;
#endif

#define CDEV_MAJOR 56
static struct cdevsw i4btel_cdevsw = {
	i4btelopen,	i4btelclose,	i4btelread,	i4btelwrite,
  	i4btelioctl,	nostop,		noreset,	nodevtotty,
#if defined(__FreeBSD_version) && __FreeBSD_version >= 300001
	i4btelpoll,	nommap, 	NULL, "i4btel", NULL, -1
#else
	noselect,	nommap, 	NULL, "i4btel", NULL, -1
#endif
};

PDEVSTATIC void i4btelinit(void *unused);

PDEVSTATIC void i4btelattach(void *);
PSEUDO_SET(i4btelattach, i4b_tel);

/*===========================================================================*
 *			DEVICE DRIVER ROUTINES
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	initialization at kernel load time
 *---------------------------------------------------------------------------*/
PDEVSTATIC void
i4btelinit(void *unused)
{
    dev_t dev;
    
    dev = makedev(CDEV_MAJOR, 0);

    cdevsw_add(&dev, &i4btel_cdevsw, NULL);
}

SYSINIT(i4bteldev, SI_SUB_DRIVERS,
	SI_ORDER_MIDDLE+CDEV_MAJOR, &i4btelinit, NULL);

#endif /* BSD > 199306 && defined(__FreeBSD__) */

/*---------------------------------------------------------------------------*
 *	interface attach routine
 *---------------------------------------------------------------------------*/
PDEVSTATIC void
#ifdef __FreeBSD__
i4btelattach(void *dummy)
#else
i4btelattach()
#endif
{
	int i;

#ifndef HACK_NO_PSEUDO_ATTACH_MSG
	printf("i4btel: %d ISDN telephony interface device(s) attached\n", NI4BTEL);
#endif
	
	for(i=0; i < NI4BTEL; i++)
	{
		tel_sc[i].devstate = ST_IDLE;
		tel_sc[i].audiofmt = CVT_NONE;
		tel_init_linktab(i);
#ifdef DEVFS
	  	tel_sc[i].devfs_token
		  = devfs_add_devswf(&i4btel_cdevsw, i, DV_CHR,
				     UID_ROOT, GID_WHEEL, 0600,
				     "i4btel%d", i);
#endif
	}
}

/*---------------------------------------------------------------------------*
 *	open tel device
 *---------------------------------------------------------------------------*/
PDEVSTATIC int
i4btelopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	int unit = minor(dev);
	tel_sc_t *sc;
	
	if(unit > NI4BTEL)
		return(ENXIO);

	sc = &tel_sc[unit];		

	if(!(sc->devstate & ST_CONNECTED))
		return(EIO);

	if(sc->devstate & ST_ISOPEN)
		return(EBUSY);

	sc->devstate |= ST_ISOPEN;		

	return(0);
}

/*---------------------------------------------------------------------------*
 *	close tel device
 *---------------------------------------------------------------------------*/
PDEVSTATIC int
i4btelclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	int unit = minor(dev);
	tel_sc_t *sc;
	int error = 0;
	
	if(unit > NI4BTEL)
		return(ENXIO);

	sc = &tel_sc[unit];		

	if(sc->isdn_linktab != NULL && sc->isdn_linktab->tx_queue != NULL)
	{
		while(!(IF_QEMPTY(sc->isdn_linktab->tx_queue)))
		{
			sc->devstate |= ST_WRWAITEMPTY;
	
			if((error = tsleep((caddr_t) &sc->isdn_linktab->tx_queue,
					TTIPRI | PCATCH, "wtcl", 0)) != 0)
			{
				break;
			}
		}
		sc->devstate &= ~ST_WRWAITEMPTY;		
	}
	sc->devstate &= ~ST_ISOPEN;		
	return(error);
}

/*---------------------------------------------------------------------------*
 *	i4btelioctl - device driver ioctl routine
 *---------------------------------------------------------------------------*/
PDEVSTATIC int
#if defined(__FreeBSD_version) && __FreeBSD_version >= 300003
i4btelioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
#else
i4btelioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
#endif
{
	int error = 0;
        struct mbuf *m;
        int s;
	tel_sc_t *sc = &tel_sc[minor(dev)];

	switch(cmd)
	{
		case I4B_TEL_GETAUDIOFMT:
			*(int *)data = sc->audiofmt;
			break;
		
		case I4B_TEL_SETAUDIOFMT:
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

		default:
			error = ENOTTY;
			break;
	}
	return(error);
}

/*---------------------------------------------------------------------------*
 *	read from tel device
 *---------------------------------------------------------------------------*/
PDEVSTATIC int
i4btelread(dev_t dev, struct uio *uio, int ioflag)
{
	struct mbuf *m;
	int s;
	int error = 0;
	tel_sc_t *sc = &tel_sc[minor(dev)];
	
	if(!(sc->devstate & ST_CONNECTED))
		return(EIO);

	if(!(sc->devstate & ST_ISOPEN))
		return(EIO);

#ifdef NOTDEF
	while(!(sc->devstate & ST_CONNECTED))
	{
		if((error = tsleep((caddr_t) &sc->devstate,
					TTIPRI | PCATCH,
					"rrtel", 0 )) != 0)
		{
			return(error);
		}
	}
#endif

	while(IF_QEMPTY(sc->isdn_linktab->rx_queue)	&&
		(sc->devstate & ST_ISOPEN)		&&
		(sc->devstate & ST_CONNECTED))		
	{
		sc->devstate |= ST_RDWAITDATA;

		if((error = tsleep((caddr_t) &sc->isdn_linktab->rx_queue,
					TTIPRI | PCATCH,
					"rtel", 0 )) != 0)
		{
			sc->devstate &= ~ST_RDWAITDATA;
			return(error);
		}
	}

	if(!(sc->devstate & ST_ISOPEN))
	{
		return(EIO);
	}

	if(!(sc->devstate & ST_CONNECTED))
	{
		return(EIO);
	}
	
	s = splimp();

	IF_DEQUEUE(sc->isdn_linktab->rx_queue, m);

	if(m && m->m_len)
	{
		if(sc->audiofmt == CVT_ALAW2ULAW)
		{
			int i;
		        for(i = 0; i < m->m_len; i++)
                                m->m_data[i] = alaw_ulaw[(int)m->m_data[i]];
                }
		error = uiomove(m->m_data, m->m_len, uio);
	}
	else
	{
		error = EIO;
	}
		
	if(m)
		i4b_Bfreembuf(m);

	splx(s);

	return(error);
}

/*---------------------------------------------------------------------------*
 *	write to tel device
 *---------------------------------------------------------------------------*/
PDEVSTATIC int
i4btelwrite(dev_t dev, struct uio * uio, int ioflag)
{
	struct mbuf *m;
	int s;
	int error = 0;
	tel_sc_t *sc = &tel_sc[minor(dev)];
	
	if(!(sc->devstate & ST_CONNECTED))
		return(EIO);

	if(!(sc->devstate & ST_ISOPEN))
	{
		return(EIO);
	}

#ifdef NOTDEF
	while(!(sc->devstate & ST_CONNECTED))
	{
		if((error = tsleep((caddr_t) &sc->devstate,
					TTIPRI | PCATCH,
					"wrtel", 0 )) != 0)
		{
			return(error);
		}

		/*
		 * XXX the originations B channel gets much earlier
		 * switched thru than the destinations B channel, so
		 * if the origination starts to send at once, some
		 * 200 bytes (at my site) or so get lost, so i delay
		 * a bit before sending. (-hm)
		 */
		
		tsleep((caddr_t) &sc->devstate, TTIPRI | PCATCH, "xtel", (hz*1));
	}
#endif

	while((IF_QFULL(sc->isdn_linktab->tx_queue)) &&
	      (sc->devstate & ST_ISOPEN))
	{
		sc->devstate |= ST_WRWAITEMPTY;

		if((error = tsleep((caddr_t) &sc->isdn_linktab->tx_queue,
				TTIPRI | PCATCH, "wtel", 0)) != 0)
		{
			sc->devstate &= ~ST_WRWAITEMPTY;			
			return(error);
		}
	}

	if(!(sc->devstate & ST_ISOPEN))
	{
		return(EIO);
	}

	if(!(sc->devstate & ST_CONNECTED))
	{
		return(EIO);
	}

	s = splimp();

	if((m = i4b_Bgetmbuf(BCH_MAX_DATALEN)) != NULL)
	{
		m->m_len = min(BCH_MAX_DATALEN, uio->uio_resid);

		error = uiomove(m->m_data, m->m_len, uio);

		if(sc->audiofmt == CVT_ALAW2ULAW)
		{
			int i;
		        for(i = 0; i < m->m_len; i++)
                                m->m_data[i] = ulaw_alaw[(int)m->m_data[i]];
                }
		
		IF_ENQUEUE(sc->isdn_linktab->tx_queue, m);

		(*sc->isdn_linktab->bch_tx_start)(sc->isdn_linktab->unit, sc->isdn_linktab->channel);
	}

	splx(s);
	
	return(error);
}

/*---------------------------------------------------------------------------*
 *	poll
 *---------------------------------------------------------------------------*/
#if defined(__FreeBSD_version) && __FreeBSD_version >= 300001
PDEVSTATIC int
i4btelpoll (dev_t dev, int events, struct proc *p)
{
	return (ENODEV);
}
#endif

/*===========================================================================*
 *			ISDN INTERFACE ROUTINES
 *===========================================================================*/

/*---------------------------------------------------------------------------*
*	this routine is called from L4 handler at connect time
 *---------------------------------------------------------------------------*/
static void
tel_connect(int unit, void *cdp)
{
	tel_sc_t *sc = &tel_sc[unit];

	sc->cdp = (call_desc_t *)cdp;

#ifdef NOTDEF	
	if(!(sc->devstate & ST_CONNECTED))
	{
		sc->devstate |= ST_CONNECTED;
		wakeup((caddr_t) &sc->devstate);
	}
#else
	sc->devstate |= ST_CONNECTED;
#endif
}

/*---------------------------------------------------------------------------*
 *	this routine is called from L4 handler at disconnect time
 *---------------------------------------------------------------------------*/
static void
tel_disconnect(int unit, void *cdp)
{
/*	call_desc_t *cd = (call_desc_t *)cdp; */

	tel_sc_t *sc = &tel_sc[unit];

	sc->devstate &= ~ST_CONNECTED;

	if(sc->devstate & ST_RDWAITDATA)
	{
		sc->devstate &= ~ST_RDWAITDATA;
		wakeup((caddr_t) &sc->isdn_linktab->rx_queue);
	}

	if(sc->devstate & ST_WRWAITEMPTY)
	{
		sc->devstate &= ~ST_WRWAITEMPTY;
		wakeup((caddr_t) &sc->isdn_linktab->tx_queue);
	}
}

/*---------------------------------------------------------------------------*
 *	feedback from daemon in case of dial problems
 *---------------------------------------------------------------------------*/
static void
tel_dialresponse(int unit, int status)
{
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
	tel_sc_t *sc = &tel_sc[unit];
	
	if(sc->devstate & ST_RDWAITDATA)
	{
		sc->devstate &= ~ST_RDWAITDATA;
		wakeup((caddr_t) &sc->isdn_linktab->rx_queue);
	}
}

/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	when the last frame has been sent out and there is no
 *	further frame (mbuf) in the tx queue.
 *---------------------------------------------------------------------------*/
static void
tel_tx_queue_empty(int unit)
{
	tel_sc_t *sc = &tel_sc[unit];

	if(sc->devstate & ST_WRWAITEMPTY)
	{
		sc->devstate &= ~ST_WRWAITEMPTY;
		wakeup((caddr_t) &sc->isdn_linktab->tx_queue);
	}
}

/*---------------------------------------------------------------------------*
 *	this routine is called from the HSCX interrupt handler
 *	each time a packet is received or transmitted.
 *---------------------------------------------------------------------------*/
static void
tel_activity(int unit, int rxtx)
{
	tel_sc[unit].cdp->last_active_time = SECOND;
}

/*---------------------------------------------------------------------------*
 *	return this drivers linktab address
 *---------------------------------------------------------------------------*/
drvr_link_t *
tel_ret_linktab(int unit)
{
	tel_sc_t *sc = &tel_sc[unit];
	
	tel_init_linktab(unit);
	return(&sc->drvr_linktab);
}

/*---------------------------------------------------------------------------*
 *	setup the isdn_linktab for this driver
 *---------------------------------------------------------------------------*/
void
tel_set_linktab(int unit, isdn_link_t *ilt)
{
	tel_sc_t *sc = &tel_sc[unit];
	sc->isdn_linktab = ilt;
}

/*---------------------------------------------------------------------------*
 *	initialize this drivers linktab
 *---------------------------------------------------------------------------*/
static void
tel_init_linktab(int unit)
{
	tel_sc_t *sc = &tel_sc[unit];
	
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
 *			AUDIO FORMAT CONVERSION
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	A-law to mu-law conversion
 *---------------------------------------------------------------------------*/
static unsigned char alaw_ulaw[] = {
	0x002a, 0x00a9, 0x005f, 0x00e3, 0x001f, 0x009f, 0x0048, 0x00c8,
	0x0039, 0x00b9, 0x006f, 0x00f7, 0x001f, 0x009f, 0x0055, 0x00d7,
	0x0022, 0x00a1, 0x005b, 0x00dd, 0x001f, 0x009f, 0x0040, 0x00c0,
	0x0031, 0x00b1, 0x0067, 0x00eb, 0x001f, 0x009f, 0x004e, 0x00cf,
	0x002e, 0x00ad, 0x0063, 0x00e7, 0x001f, 0x009f, 0x004c, 0x00cc,
	0x003d, 0x00bd, 0x0077, 0x00ff, 0x001f, 0x009f, 0x0059, 0x00db,
	0x0026, 0x00a5, 0x005d, 0x00df, 0x001f, 0x009f, 0x0044, 0x00c4,
	0x0035, 0x00b5, 0x006b, 0x00ef, 0x001f, 0x009f, 0x0051, 0x00d3,
	0x0028, 0x00a7, 0x005f, 0x00e3, 0x001f, 0x009f, 0x0046, 0x00c6,
	0x0037, 0x00b7, 0x006f, 0x00f7, 0x001f, 0x009f, 0x0053, 0x00d5,
	0x0020, 0x009f, 0x005b, 0x00dd, 0x001f, 0x009f, 0x003f, 0x00bf,
	0x002f, 0x00af, 0x0067, 0x00eb, 0x001f, 0x009f, 0x004d, 0x00ce,
	0x002c, 0x00ab, 0x0063, 0x00e7, 0x001f, 0x009f, 0x004a, 0x00ca,
	0x003b, 0x00bb, 0x0077, 0x00ff, 0x001f, 0x009f, 0x0057, 0x00d9,
	0x0024, 0x00a3, 0x005d, 0x00df, 0x001f, 0x009f, 0x0042, 0x00c2,
	0x0033, 0x00b3, 0x006b, 0x00ef, 0x001f, 0x009f, 0x004f, 0x00d1,
	0x002b, 0x00aa, 0x0063, 0x00e3, 0x001f, 0x009f, 0x0049, 0x00c9,
	0x003a, 0x00ba, 0x0077, 0x00f7, 0x001f, 0x009f, 0x0057, 0x00d7,
	0x0023, 0x00a2, 0x005d, 0x00dd, 0x001f, 0x009f, 0x0041, 0x00c1,
	0x0032, 0x00b2, 0x006b, 0x00eb, 0x001f, 0x009f, 0x004f, 0x00cf,
	0x002f, 0x00ae, 0x0067, 0x00e7, 0x001f, 0x009f, 0x004d, 0x00cd,
	0x003e, 0x00be, 0x00ff, 0x00ff, 0x001f, 0x009f, 0x005b, 0x00db,
	0x0027, 0x00a6, 0x005f, 0x00df, 0x001f, 0x009f, 0x0045, 0x00c5,
	0x0036, 0x00b6, 0x006f, 0x00ef, 0x001f, 0x009f, 0x0053, 0x00d3,
	0x0029, 0x00a8, 0x005f, 0x00e3, 0x001f, 0x009f, 0x0047, 0x00c7,
	0x0038, 0x00b8, 0x006f, 0x00f7, 0x001f, 0x009f, 0x0055, 0x00d5,
	0x0021, 0x00a0, 0x005b, 0x00dd, 0x001f, 0x009f, 0x003f, 0x00bf,
	0x0030, 0x00b0, 0x0067, 0x00eb, 0x001f, 0x009f, 0x004e, 0x00ce,
	0x002d, 0x00ac, 0x0063, 0x00e7, 0x001f, 0x009f, 0x004b, 0x00cb,
	0x003c, 0x00bc, 0x0077, 0x00ff, 0x001f, 0x009f, 0x0059, 0x00d9,
	0x0025, 0x00a4, 0x005d, 0x00df, 0x001f, 0x009f, 0x0043, 0x00c3,
	0x0034, 0x00b4, 0x006b, 0x00ef, 0x001f, 0x009f, 0x0051, 0x00d1
};

/*---------------------------------------------------------------------------*
 *	mu-law to A-law conversion
 *---------------------------------------------------------------------------*/
static unsigned char ulaw_alaw[] = {
	0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc,
	0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc,
	0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc,
	0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00ac,
	0x0050, 0x00d0, 0x0010, 0x0090, 0x0070, 0x00f0, 0x0030, 0x00b0,
	0x0040, 0x00c0, 0x0000, 0x0080, 0x0060, 0x00e0, 0x0020, 0x00a0,
	0x00d8, 0x0018, 0x0098, 0x0078, 0x00f8, 0x0038, 0x00b8, 0x0048,
	0x00c8, 0x0008, 0x0088, 0x0068, 0x00e8, 0x0028, 0x00a8, 0x00d6,
	0x0096, 0x0076, 0x00f6, 0x0036, 0x00b6, 0x0046, 0x00c6, 0x0006,
	0x0086, 0x0066, 0x00e6, 0x0026, 0x00a6, 0x00de, 0x009e, 0x00fe,
	0x00fe, 0x00be, 0x00be, 0x00ce, 0x00ce, 0x008e, 0x008e, 0x00ee,
	0x00ee, 0x00d2, 0x00d2, 0x00f2, 0x00f2, 0x00c2, 0x00c2, 0x00e2,
	0x00e2, 0x00e2, 0x00da, 0x00da, 0x00da, 0x00da, 0x00fa, 0x00fa,
	0x00fa, 0x00fa, 0x00ca, 0x00ca, 0x00ca, 0x00ca, 0x00ea, 0x00ea,
	0x00ea, 0x00ea, 0x00ea, 0x00ea, 0x00eb, 0x00eb, 0x00eb, 0x00eb,
	0x00eb, 0x00eb, 0x00eb, 0x00eb, 0x00eb, 0x00eb, 0x00eb, 0x00eb,
	0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd,
	0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd,
	0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd,
	0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd,
	0x00d1, 0x0011, 0x0091, 0x0071, 0x00f1, 0x0031, 0x00b1, 0x0041,
	0x00c1, 0x0001, 0x0081, 0x0061, 0x00e1, 0x0021, 0x00a1, 0x0059,
	0x00d9, 0x0019, 0x0099, 0x0079, 0x00f9, 0x0039, 0x00b9, 0x0049,
	0x00c9, 0x0009, 0x0089, 0x0069, 0x00e9, 0x0029, 0x00a9, 0x0057,
	0x0017, 0x0097, 0x0077, 0x00f7, 0x0037, 0x00b7, 0x0047, 0x00c7,
	0x0007, 0x0087, 0x0067, 0x00e7, 0x0027, 0x00a7, 0x00df, 0x009f,
	0x009f, 0x00ff, 0x00ff, 0x00bf, 0x00bf, 0x00cf, 0x00cf, 0x008f,
	0x008f, 0x00ef, 0x00ef, 0x00af, 0x00af, 0x00d3, 0x00d3, 0x00f3,
	0x00f3, 0x00f3, 0x00c3, 0x00c3, 0x00c3, 0x00c3, 0x00e3, 0x00e3,
	0x00e3, 0x00e3, 0x00db, 0x00db, 0x00db, 0x00db, 0x00fb, 0x00fb,
	0x00fb, 0x00fb, 0x00fb, 0x00fb, 0x00cb, 0x00cb, 0x00cb, 0x00cb,
	0x00cb, 0x00cb, 0x00cb, 0x00cb, 0x00eb, 0x00eb, 0x00eb, 0x00eb
};

/*===========================================================================*/

#endif /* NI4BTEL > 0 */
