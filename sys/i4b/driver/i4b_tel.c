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
 *	$Id: i4b_tel.c,v 1.47 1999/12/13 21:25:24 hm Exp $
 *
 * $FreeBSD: src/sys/i4b/driver/i4b_tel.c,v 1.10 1999/12/14 20:48:13 hm Exp $
 *
 *	last edit-date: [Mon Dec 13 21:39:26 1999]
 *
 *---------------------------------------------------------------------------*/

#include "i4btel.h"

#if NI4BTEL > 0

#undef I4BTELDEBUG

#include <sys/param.h>
#include <sys/systm.h>

#if (defined(__FreeBSD__) && __FreeBSD__ >= 3) || defined(__NetBSD__)
#include <sys/ioccom.h>
#include <sys/poll.h>
#else
#include <sys/ioctl.h>
#include <sys/fcntl.h>
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

#if defined(__FreeBSD__) && __FreeBSD__ == 3
#include "opt_devfs.h"
#endif

#ifdef DEVFS
#include <sys/devfsext.h>
#endif

#endif /* __FreeBSD__ */

#ifdef __bsdi__
#include <sys/device.h>
#endif

#ifdef __FreeBSD__
#include <machine/i4b_ioctl.h>
#include <machine/i4b_tel_ioctl.h>
#include <machine/i4b_debug.h>
#else
#include <i4b/i4b_ioctl.h>
#include <i4b/i4b_tel_ioctl.h>
#include <i4b/i4b_debug.h>
#endif

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_l3l4.h>

#include <i4b/layer4/i4b_l4.h>

/* minor number: lower 6 bits = unit number */

#include <i4b/layer4/i4b_l4.h>

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

	struct selinfo		selp;		/* select / poll */

#if defined(__FreeBSD__) && __FreeBSD__ == 3
#ifdef DEVFS
        void                    *devfs_token;   /* token for DEVFS */
#endif
#endif

} tel_sc_t;

static tel_sc_t tel_sc[NI4BTEL][NOFUNCS];
	
/* forward decl */

static void tel_rx_data_rdy(int unit);
static void tel_tx_queue_empty(int unit);
static void tel_init_linktab(int unit);
static void tel_connect(int unit, void *cdp);
static void tel_disconnect(int unit, void *cdp);

/* audio format conversion tables */
static unsigned char a2u_tab[];
static unsigned char u2a_tab[];
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

#ifdef OS_USES_POLL
int i4btelpoll	__P((dev_t dev, int events, struct proc *p));
#else
int i4btelsel __P((dev_t dev, int rw, struct proc *p));
#endif

#endif /* __FreeBSD__ */


#if BSD > 199306 && defined(__FreeBSD__)

#define PDEVSTATIC	static
PDEVSTATIC d_open_t	i4btelopen;
PDEVSTATIC d_close_t i4btelclose;
PDEVSTATIC d_read_t i4btelread;
PDEVSTATIC d_read_t i4btelwrite;
PDEVSTATIC d_ioctl_t i4btelioctl;

#ifdef OS_USES_POLL
PDEVSTATIC d_poll_t i4btelpoll;
#define POLLFIELD i4btelpoll
#else
PDEVSTATIC d_select_t i4btelsel;
#define POLLFIELD i4btelsel
#endif

#define CDEV_MAJOR 56

#if defined(__FreeBSD__) && __FreeBSD__ >= 4
static struct cdevsw i4btel_cdevsw = {
	/* open */      i4btelopen,
	/* close */     i4btelclose,
	/* read */      i4btelread,
	/* write */     i4btelwrite,
	/* ioctl */     i4btelioctl,
	/* poll */      POLLFIELD,
	/* mmap */      nommap,
	/* strategy */  nostrategy,
	/* name */      "i4btel",
	/* maj */       CDEV_MAJOR,
	/* dump */      nodump,
	/* psize */     nopsize,
	/* flags */     0,
	/* bmaj */      -1
};
#else
static struct cdevsw i4btel_cdevsw = {
	i4btelopen,	i4btelclose,	i4btelread,	i4btelwrite,
  	i4btelioctl,	nostop,		noreset,	nodevtotty,
	POLLFIELD,	nommap, 	NULL, "i4btel", NULL, -1
};
#endif

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
#if defined(__FreeBSD__) && __FreeBSD__ >= 4
	cdevsw_add(&i4btel_cdevsw);
#else
	dev_t dev = makedev(CDEV_MAJOR, 0);
	cdevsw_add(&dev, &i4btel_cdevsw, NULL);
#endif
}

SYSINIT(i4bteldev, SI_SUB_DRIVERS,
	SI_ORDER_MIDDLE+CDEV_MAJOR, &i4btelinit, NULL);

#endif /* BSD > 199306 && defined(__FreeBSD__) */

#ifdef __bsdi__

int i4btelsel(dev_t dev, int rw, struct proc *p);
int i4btelmatch(struct device *parent, struct cfdata *cf, void *aux);
void dummy_i4btelattach(struct device*, struct device *, void *);

#define CDEV_MAJOR 62

static struct cfdriver i4btelcd =
	{ NULL, "i4btel", i4btelmatch, dummy_i4btelattach, DV_DULL,
	  sizeof(struct cfdriver) };
struct devsw i4btelsw = 
	{ &i4btelcd,
	  i4btelopen,	i4btelclose,	i4btelread,	i4btelwrite,
	  i4btelioctl,	i4btelsel,	nommap,		nostrat,
	  nodump,	nopsize,	0,		nostop
};

int
i4btelmatch(struct device *parent, struct cfdata *cf, void *aux)
{
	DBGL4(L4_TELDBG, "i4btelmatch", ("aux=0x%x\n", aux));	
	return 1;
}

void
dummy_i4btelattach(struct device *parent, struct device *self, void *aux)
{
	DBGL4(L4_TELDBG, "dummy_i4btelattach", ("aux=0x%x\n", aux));
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
	int i, j;

#ifndef HACK_NO_PSEUDO_ATTACH_MSG
	printf("i4btel: %d ISDN telephony interface device(s) attached\n", NI4BTEL);
#endif
	
	for(i=0; i < NI4BTEL; i++)
	{
		for(j=0; j < NOFUNCS; j++)
		{
			tel_sc[i][j].devstate = ST_IDLE;
			tel_sc[i][j].audiofmt = CVT_NONE;
			tel_sc[i][j].rcvttab = 0;
			tel_sc[i][j].wcvttab = 0;
			tel_sc[i][j].result = 0;

#if defined(__FreeBSD__)
#if __FreeBSD__ == 3

#ifdef DEVFS

/* XXX */  		tel_sc[i][j].devfs_token
		  		= devfs_add_devswf(&i4btel_cdevsw, i, DV_CHR,
				     UID_ROOT, GID_WHEEL, 0600,
				     "i4btel%d", i);
#endif

#else
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
#endif
#endif
		}
		tel_init_linktab(i);		
	}
}

/*---------------------------------------------------------------------------*
 *	open tel device
 *---------------------------------------------------------------------------*/
PDEVSTATIC int
i4btelopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	int unit = UNIT(dev);
	int func = FUNC(dev);
	
	tel_sc_t *sc;
	
	if(unit > NI4BTEL)
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
PDEVSTATIC int
i4btelclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	int unit = UNIT(dev);
	int func = FUNC(dev);
	tel_sc_t *sc;
	int error = 0;
	
	if(unit > NI4BTEL)
		return(ENXIO);

	sc = &tel_sc[unit][func];		

	if((func == FUNCTEL) &&
	   (sc->isdn_linktab != NULL && sc->isdn_linktab->tx_queue != NULL))
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
PDEVSTATIC int
i4btelread(dev_t dev, struct uio *uio, int ioflag)
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
		while(IF_QEMPTY(sc->isdn_linktab->rx_queue) &&
			(sc->devstate & ST_ISOPEN)          &&
			(sc->devstate & ST_CONNECTED))		
		{
			sc->devstate |= ST_RDWAITDATA;

			DBGL4(L4_TELDBG, "i4btelread", ("i4btel%d, queue empty!\n", unit));
			
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

			DBGL4(L4_TELDBG, "i4btelread", ("i4btel%d, mbuf (%d bytes), uiomove %d!\n", unit, m->m_len, error));
		}
		else
		{
			DBGL4(L4_TELDBG, "i4btelread", ("i4btel%d, empty mbuf from queue!\n", unit));
			error = EIO;
		}
			
		if(m)
			i4b_Bfreembuf(m);
	
		splx(s);
	}
	else if(func == FUNCDIAL)
	{
		while((sc->result == 0) && (sc->devstate & ST_ISOPEN))
		{
			sc->devstate |= ST_RDWAITDATA;
	
			if((error = tsleep((caddr_t) &sc->result,
						TTIPRI | PCATCH,
						"rtel1", 0 )) != 0)
			{
				sc->devstate &= ~ST_RDWAITDATA;
				return(error);
			}
		}
	
		if(!(sc->devstate & ST_ISOPEN))
		{
			return(EIO);
		}
	
		s = splimp();

		if(sc->result != 0)
		{
			error = uiomove(&sc->result, 1, uio);
			sc->result = 0;
		}
		else
		{
			error = EIO;
		}

		splx(s);			
	}
	return(error);
}

/*---------------------------------------------------------------------------*
 *	write to tel device
 *---------------------------------------------------------------------------*/
PDEVSTATIC int
i4btelwrite(dev_t dev, struct uio * uio, int ioflag)
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
		if(!(sc->devstate & ST_CONNECTED))
			return(EIO);
			
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
			
			if(IF_QFULL(sc->isdn_linktab->tx_queue))
			{
				m_freem(m);			
			}
			else
			{
				IF_ENQUEUE(sc->isdn_linktab->tx_queue, m);
			}
	
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
	}
	else
	{
		error = EIO;
	}		
	
	return(error);
}

#ifdef OS_USES_POLL
/*---------------------------------------------------------------------------*
 *	device driver poll
 *---------------------------------------------------------------------------*/
PDEVSTATIC int
i4btelpoll(dev_t dev, int events, struct proc *p)
{
	int revents = 0;	/* Events we found */
	int s;
	int unit = UNIT(dev);
	int func = FUNC(dev);	

	tel_sc_t *sc = &tel_sc[unit][func];
	
	s = splhigh();

	if(!(sc->devstate & ST_ISOPEN))
	{
		DBGL4(L4_TELDBG, "i4btelpoll", ("i4btel%d, !ST_ISOPEN\n", unit));
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
			(!IF_QFULL(sc->isdn_linktab->tx_queue)))
		{
			DBGL4(L4_TELDBG, "i4btelpoll", ("i4btel%d, POLLOUT\n", unit));
			revents |= (events & (POLLOUT|POLLWRNORM));
		}
		
		/* ... while reads are OK if we have any data */
	
		if((events & (POLLIN|POLLRDNORM))	&&
			(sc->devstate & ST_CONNECTED)	&&
			(sc->isdn_linktab != NULL)	&&
			(!IF_QEMPTY(sc->isdn_linktab->rx_queue)))
		{
			DBGL4(L4_TELDBG, "i4btelpoll", ("i4btel%d, POLLIN\n", unit));
			revents |= (events & (POLLIN|POLLRDNORM));
		}
			
		if(revents == 0)
		{
			DBGL4(L4_TELDBG, "i4btelpoll", ("i4btel%d, selrecord\n", unit));
			selrecord(p, &sc->selp);
		}
	}
	else if(func == FUNCDIAL)
	{
		if(events & (POLLOUT|POLLWRNORM))
		{
			DBGL4(L4_TELDBG, "i4btelpoll", ("i4bteld%d,  POLLOUT\n", unit));
			revents |= (events & (POLLOUT|POLLWRNORM));
		}

		if(events & (POLLIN|POLLRDNORM))
		{
			DBGL4(L4_TELDBG, "i4btelpoll", ("i4bteld%d,  POLLIN, result = %d\n", unit, sc->result));
			if(sc->result != 0)
				revents |= (events & (POLLIN|POLLRDNORM));
		}
			
		if(revents == 0)
		{
			DBGL4(L4_TELDBG, "i4btelpoll", ("i4bteld%d,  selrecord\n", unit));
			selrecord(p, &sc->selp);
		}
	}
	splx(s);
	return(revents);
}

#else /* OS_USES_POLL */

/*---------------------------------------------------------------------------*
 *	device driver select
 *---------------------------------------------------------------------------*/
PDEVSTATIC int
i4btelsel(dev_t dev, int rw, struct proc *p)
{
	int s;
	int unit = UNIT(dev);
	int func = FUNC(dev);	

	tel_sc_t *sc = &tel_sc[unit][func];
	
	s = splhigh();

	if (!(sc->devstate & ST_ISOPEN))
	{
		DBGL4(L4_TELDBG, "i4btelsel", ("i4btel%d, !ST_ISOPEN\n", unit));
		splx(s);
		return(0);
	}

	if (func == FUNCTEL)
	{
		/* Don't even bother if we're not connected */
		if (!(sc->devstate & ST_CONNECTED) || sc->isdn_linktab == NULL)
		{
			splx(s);
			return 0;
		}

		if (rw == FREAD)
		{
			if (!IF_QEMPTY(sc->isdn_linktab->rx_queue))
			{
				DBGL4(L4_TELDBG, "i4btelsel", ("i4btel%d, FREAD\n", unit));
				splx(s);
				return 1;
			}
		}
		else if (rw == FWRITE)
		{
			if (!IF_QFULL(sc->isdn_linktab->tx_queue))
			{
				DBGL4(L4_TELDBG, "i4btelsel", ("i4btel%d, FWRITE\n", unit));
				splx(s);
				return 1;
			}
		}
	}
	else if (func == FUNCDIAL)
	{
		if (rw == FWRITE)
		{
			DBGL4(L4_TELDBG, "i4btelsel", ("i4bteld%d,  FWRITE\n", unit));
			splx(s);
			return 1;
		}

		if (rw == FREAD)
		{
			DBGL4(L4_TELDBG, "i4btelsel", ("i4bteld%d,  FREAD, result = %d\n", unit, sc->result));
			if (sc->result != 0)
			{
				splx(s);
				return 1;
			}
		}
	}

	DBGL4(L4_TELDBG, "i4btelsel", ("i4bteld%d,  selrecord\n", unit));
	selrecord(p, &sc->selp);
	splx(s);
	return 0;
}

#endif /* OS_USES_POLL */

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

	if(sc->devstate == ST_ISOPEN)
	{
		sc->result = RSP_CONN;

		if(sc->devstate & ST_RDWAITDATA)
		{
			sc->devstate &= ~ST_RDWAITDATA;
			wakeup((caddr_t) &sc->result);
		}
		selwakeup(&sc->selp);
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
		wakeup((caddr_t) &sc->isdn_linktab->rx_queue);
	}

	if(sc->devstate & ST_WRWAITEMPTY)
	{
		sc->devstate &= ~ST_WRWAITEMPTY;
		wakeup((caddr_t) &sc->isdn_linktab->tx_queue);
	}

	/* dialer device */
	
	sc = &tel_sc[unit][FUNCDIAL];

	if(sc->devstate == ST_ISOPEN)
	{
		sc->result = RSP_HUP;

		if(sc->devstate & ST_RDWAITDATA)
		{
			sc->devstate &= ~ST_RDWAITDATA;
			wakeup((caddr_t) &sc->result);
		}
		selwakeup(&sc->selp);
	}
}

/*---------------------------------------------------------------------------*
 *	feedback from daemon in case of dial problems
 *---------------------------------------------------------------------------*/
static void
tel_dialresponse(int unit, int status, cause_t cause)
{	
	tel_sc_t *sc = &tel_sc[unit][FUNCDIAL];

	DBGL4(L4_TELDBG, "tel_dialresponse", ("i4btel%d,  status=%d, cause=0x%4x\n", unit, status, cause));

	if((sc->devstate == ST_ISOPEN) && status)
	{	
		sc->result = RSP_NOA;

		if(sc->devstate & ST_RDWAITDATA)
		{
			sc->devstate &= ~ST_RDWAITDATA;
			wakeup((caddr_t) &sc->result);
		}
		selwakeup(&sc->selp);
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
		wakeup((caddr_t) &sc->isdn_linktab->rx_queue);
	}
	selwakeup(&sc->selp);
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
		wakeup((caddr_t) &sc->isdn_linktab->tx_queue);
	}
	selwakeup(&sc->selp);
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

/*===========================================================================*/

#endif /* NI4BTEL > 0 */
