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
 *	i4b_tel.c - device driver for ISDN telephony
 *	--------------------------------------------
 *
 *	$Id: i4b_tel.c,v 1.22 1999/02/16 10:40:18 hm Exp $
 *
 *	last edit-date: [Tue Feb 16 11:30:35 1999]
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
	u_char			*rcvttab;	/* conversion table on read */
	u_char			*wcvttab;	/* conversion table on write */
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

/* audio format conversion tables */
static unsigned char alaw_ulaw[];
static unsigned char ulaw_alaw[];
static unsigned char bitreverse[];

#ifndef __FreeBSD__
#define	PDEVSTATIC	/* - not static - */
PDEVSTATIC void i4btelattach __P((void));
#ifdef __bsdi__
PDEVSTATIC int i4btelioctl __P((dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p));
#else
PDEVSTATIC int i4btelioctl __P((dev_t dev, int cmd, caddr_t data, int flag, struct proc *p));
#endif
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

#ifdef __bsdi__
#include <sys/device.h>
int i4btelmatch(struct device *parent, struct cfdata *cf, void *aux);
void dummy_i4btelattach(struct device*, struct device *, void *);

#define CDEV_MAJOR 62

static struct cfdriver i4btelcd =
	{ NULL, "i4btel", i4btelmatch, dummy_i4btelattach, DV_DULL,
	  sizeof(struct cfdriver) };
struct devsw i4btelsw = 
	{ &i4btelcd,
	  i4btelopen,	i4btelclose,	i4btelread,	i4btelwrite,
	  i4btelioctl,	seltrue,	nommap,		nostrat,
	  nodump,	nopsize,	0,		nostop
};

int
i4btelmatch(struct device *parent, struct cfdata *cf, void *aux)
{
	printf("i4btelmatch: aux=0x%x\n", aux);
	return 1;
}
void
dummy_i4btelattach(struct device *parent, struct device *self, void *aux)
{
	printf("dummy_i4btelattach: aux=0x%x\n", aux);
}
#endif /* __bsdi__ */

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
		tel_sc[i].rcvttab = 0;
		tel_sc[i].wcvttab = 0;
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
#elif defined(__bsdi__)
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
			switch (*(int *)data)
			{
				case CVT_NONE:
					sc->rcvttab = 0;
					sc->wcvttab = 0;
					break;
				case CVT_ALAW2ULAW:
					sc->rcvttab = alaw_ulaw;
					sc->wcvttab = ulaw_alaw;
					break;
				case CVT_ALAW_CANON:
					sc->rcvttab = bitreverse;
					sc->wcvttab = bitreverse;
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
	
#ifdef notdef
/* 
 * XXX
 * With this code enabled, one cannot set or get the audio format 
 * while not connected. 
*/
	if(!(sc->devstate & ST_CONNECTED))
		return(EIO);
#endif

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

	if(m && m->m_len > 0)
	{
		if(sc->rcvttab)
		{
			int i;
		        for(i = 0; i < m->m_len; i++)
                                mtod(m,u_char *)[i] = sc->rcvttab[mtod(m,u_char *)[i]];
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

		if(sc->wcvttab)
		{
			int i;
		        for(i = 0; i < m->m_len; i++)
				mtod(m,u_char *)[i] = sc->wcvttab[mtod(m,u_char *)[i]];
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
 *	Line format to mu-law conversion
 *---------------------------------------------------------------------------*/
static unsigned char alaw_ulaw[256] = {
	0x2a, 0xa9, 0x62, 0xe1, 0x0a, 0x8a, 0x48, 0xc8, 0x39, 0xb9, 0x75, 0xf3, 0x1a, 0x9a, 0x56, 0xd6,
	0x22, 0xa1, 0x5d, 0xdc, 0x02, 0x82, 0x40, 0xc0, 0x31, 0xb1, 0x6a, 0xe9, 0x12, 0x92, 0x4f, 0xcf,
	0x2e, 0xad, 0x66, 0xe5, 0x0e, 0x8e, 0x4c, 0xcc, 0x3d, 0xbd, 0x7d, 0xfb, 0x1e, 0x9e, 0x5a, 0xda,
	0x26, 0xa5, 0x5f, 0xde, 0x06, 0x86, 0x44, 0xc4, 0x35, 0xb5, 0x6e, 0xed, 0x16, 0x96, 0x52, 0xd2,
	0x28, 0xa7, 0x60, 0xdf, 0x08, 0x88, 0x46, 0xc6, 0x37, 0xb7, 0x71, 0xef, 0x18, 0x98, 0x54, 0xd4,
	0x20, 0x9f, 0x5c, 0xdb, 0x00, 0x80, 0x3f, 0xbf, 0x2f, 0xaf, 0x68, 0xe7, 0x10, 0x90, 0x4e, 0xce,
	0x2c, 0xab, 0x64, 0xe3, 0x0c, 0x8c, 0x4a, 0xca, 0x3b, 0xbb, 0x79, 0xf7, 0x1c, 0x9c, 0x58, 0xd8,
	0x24, 0xa3, 0x5e, 0xdd, 0x04, 0x84, 0x42, 0xc2, 0x33, 0xb3, 0x6c, 0xeb, 0x14, 0x94, 0x50, 0xd0,
	0x2b, 0xaa, 0x63, 0xe2, 0x0b, 0x8b, 0x49, 0xc9, 0x3a, 0xba, 0x77, 0xf5, 0x1b, 0x9b, 0x57, 0xd7,
	0x23, 0xa2, 0x5d, 0xdd, 0x03, 0x83, 0x41, 0xc1, 0x32, 0xb2, 0x6b, 0xea, 0x13, 0x93, 0x4f, 0xcf,
	0x2f, 0xae, 0x67, 0xe6, 0x0f, 0x8f, 0x4d, 0xcd, 0x3e, 0xbe, 0xff, 0xfd, 0x1f, 0x9f, 0x5b, 0xdb,
	0x27, 0xa6, 0x5f, 0xdf, 0x07, 0x87, 0x45, 0xc5, 0x36, 0xb6, 0x6f, 0xee, 0x17, 0x97, 0x53, 0xd3,
	0x29, 0xa8, 0x61, 0xe0, 0x09, 0x89, 0x47, 0xc7, 0x38, 0xb8, 0x73, 0xf1, 0x19, 0x99, 0x55, 0xd5,
	0x21, 0xa0, 0x5c, 0xdc, 0x01, 0x81, 0x3f, 0xbf, 0x30, 0xb0, 0x69, 0xe8, 0x11, 0x91, 0x4e, 0xce,
	0x2d, 0xac, 0x65, 0xe4, 0x0d, 0x8d, 0x4b, 0xcb, 0x3c, 0xbc, 0x7b, 0xf9, 0x1d, 0x9d, 0x59, 0xd9,
	0x25, 0xa4, 0x5e, 0xde, 0x05, 0x85, 0x43, 0xc3, 0x34, 0xb4, 0x6d, 0xec, 0x15, 0x95, 0x51, 0xd1
};


/*---------------------------------------------------------------------------*
 *	mu-law to line format conversion
 *---------------------------------------------------------------------------*/
static unsigned char ulaw_alaw[256] = {
	0x54, 0xd4, 0x14, 0x94, 0x74, 0xf4, 0x34, 0xb4, 0x44, 0xc4, 0x04, 0x84, 0x64, 0xe4, 0x24, 0xa4,
	0x5c, 0xdc, 0x1c, 0x9c, 0x7c, 0xfc, 0x3c, 0xbc, 0x4c, 0xcc, 0x0c, 0x8c, 0x6c, 0xec, 0x2c, 0xac,
	0xd0, 0x10, 0x90, 0x70, 0xf0, 0x30, 0xb0, 0x40, 0xc0, 0x00, 0x80, 0x60, 0xe0, 0x20, 0xa0, 0x58,
	0xd8, 0x18, 0x98, 0x78, 0xf8, 0x38, 0xb8, 0x48, 0xc8, 0x08, 0x88, 0x68, 0xe8, 0x28, 0xa8, 0xd6,
	0x16, 0x96, 0x76, 0xf6, 0x36, 0xb6, 0x46, 0xc6, 0x06, 0x86, 0x66, 0xe6, 0x26, 0xa6, 0xde, 0x9e,
	0x7e, 0xfe, 0x3e, 0xbe, 0x4e, 0xce, 0x0e, 0x8e, 0x6e, 0xee, 0x2e, 0xae, 0xd2, 0x92, 0xf2, 0xb2,
	0xc2, 0x02, 0x82, 0x62, 0xe2, 0x22, 0xa2, 0x5a, 0xda, 0x1a, 0x9a, 0x7a, 0xfa, 0x3a, 0xba, 0x4a,
	0x4a, 0xca, 0xca, 0x0a, 0x0a, 0x8a, 0x8a, 0x6a, 0x6a, 0xea, 0xea, 0x2a, 0x2a, 0xaa, 0xab, 0xab,
	0x55, 0xd5, 0x15, 0x95, 0x75, 0xf5, 0x35, 0xb5, 0x45, 0xc5, 0x05, 0x85, 0x65, 0xe5, 0x25, 0xa5,
	0x5d, 0xdd, 0x1d, 0x9d, 0x7d, 0xfd, 0x3d, 0xbd, 0x4d, 0xcd, 0x0d, 0x8d, 0x6d, 0xed, 0x2d, 0xad,
	0x51, 0xd1, 0x11, 0x91, 0x71, 0xf1, 0x31, 0xb1, 0x41, 0xc1, 0x01, 0x81, 0x61, 0xe1, 0x21, 0xa1,
	0xd9, 0x19, 0x99, 0x79, 0xf9, 0x39, 0xb9, 0x49, 0xc9, 0x09, 0x89, 0x69, 0xe9, 0x29, 0xa9, 0x57,
	0x17, 0x97, 0x77, 0xf7, 0x37, 0xb7, 0x47, 0xc7, 0x07, 0x87, 0x67, 0xe7, 0x27, 0xa7, 0x5f, 0x1f,
	0x7f, 0xff, 0x3f, 0xbf, 0x4f, 0xcf, 0x0f, 0x8f, 0x6f, 0xef, 0x2f, 0xaf, 0x53, 0x13, 0x73, 0x33,
	0x43, 0xc3, 0x03, 0x83, 0x63, 0xe3, 0x23, 0xa3, 0x5b, 0xdb, 0x1b, 0x9b, 0x7b, 0xfb, 0x3b, 0xbb,
	0xbb, 0x4b, 0x4b, 0xcb, 0xcb, 0x0b, 0x0b, 0x8b, 0x8b, 0x6b, 0x6b, 0xeb, 0xeb, 0x2b, 0x2b, 0xab
};
  
/*---------------------------------------------------------------------------*
 *	bit-reverse the sample to convert from/to canonical A-law
 *---------------------------------------------------------------------------*/
static unsigned char bitreverse[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0, 
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4, 
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc, 
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2, 
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa, 
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6, 
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1, 
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9, 
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd, 
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3, 
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, 
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7, 
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

/*===========================================================================*/

#endif /* NI4BTEL > 0 */
