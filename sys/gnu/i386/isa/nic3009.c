static char     nic39_id[] = "@(#)$Id: nic3009.c,v 1.14 1995/12/17 21:14:36 phk Exp $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.14 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: nic3009.c,v $
 * Revision 1.14  1995/12/17 21:14:36  phk
 * Staticize.
 *
 * Revision 1.13  1995/12/08  23:19:30  phk
 * Julian forgot to make the *devsw structures static.
 *
 * Revision 1.12  1995/12/08  11:12:47  julian
 * Pass 3 of the great devsw changes
 * most devsw referenced functions are now static, as they are
 * in the same file as their devsw structure. I've also added DEVFS
 * support for nearly every device in the system, however
 * many of the devices have 'incorrect' names under DEVFS
 * because I couldn't quickly work out the correct naming conventions.
 * (but devfs won't be coming on line for a month or so anyhow so that doesn't
 * matter)
 *
 * If you "OWN" a device which would normally have an entry in /dev
 * then search for the devfs_add_devsw() entries and munge to make them right..
 * check out similar devices to see what I might have done in them in you
 * can't see what's going on..
 * for a laugh compare conf.c conf.h defore and after... :)
 * I have not doen DEVFS entries for any DISKSLICE devices yet as that will be
 * a much more complicated job.. (pass 5 :)
 *
 * pass 4 will be to make the devsw tables of type (cdevsw * )
 * rather than (cdevsw)
 * seems to work here..
 * complaints to the usual places.. :)
 *
 * Revision 1.11  1995/11/29  14:39:08  julian
 * If you're going to mechanically replicate something in 50 files
 * it's best to not have a (compiles cleanly) typo in it! (sigh)
 *
 * Revision 1.10  1995/11/29  10:47:05  julian
 * OK, that's it..
 * That's EVERY SINGLE driver that has an entry in conf.c..
 * my next trick will be to define cdevsw[] and bdevsw[]
 * as empty arrays and remove all those DAMNED defines as well..
 *
 * Revision 1.9  1995/11/21  14:56:02  bde
 * Completed function declarations, added prototypes and removed redundant
 * declarations.
 *
 * Revision 1.8  1995/09/19  18:54:42  bde
 * Fix benign type mismatches in isa interrupt handlers.  Many returned int
 * instead of void.
 *
 * Revision 1.7  1995/09/08  11:06:47  bde
 * Fix benign type mismatches in devsw functions.  82 out of 299 devsw
 * functions were wrong.
 *
 * Revision 1.6  1995/05/11  19:25:56  rgrimes
 * Fix -Wformat warnings from LINT kernel.
 *
 * Revision 1.5  1995/03/28  07:54:33  bde
 * Add and move declarations to fix all of the warnings from `gcc -Wimplicit'
 * (except in netccitt, netiso and netns) that I didn't notice when I fixed
 * "all" such warnings before.
 *
 * Revision 1.4  1995/02/16  08:06:21  jkh
 * Fix a few bogons introduced when config lost the 3 char limitation.
 *
 * Revision 1.3  1995/02/15  11:59:41  jkh
 * Fix a few more nits.  Should compile better now! :_)
 *
 * Revision 1.2  1995/02/15  06:28:20  jkh
 * Fix up include paths, nuke some warnings.
 *
 * Revision 1.1  1995/02/14  15:00:14  jkh
 * An ISDN driver that supports the EDSS1 and the 1TR6 ISDN interfaces.
 * EDSS1 is the "Euro-ISDN", 1TR6 is the soon obsolete german ISDN Interface.
 * Obtained from: Dietmar Friede <dfriede@drnhh.neuhaus.de> and
 * 	Juergen Krause <jkr@saarlink.de>
 *
 * This is only one part - the rest to follow in a couple of hours.
 * This part is a benign import, since it doesn't affect anything else.
 *
 *
 ******************************************************************************/

/*
 * Copyright (c) 1994 Dietmar Friede (dietmar@friede.de) All rights reserved.
 * FSF/FSAG GNU Copyright applies
 *
 * A low level driver for the NICCY-3009 ISDN Card.
 *
 */

#include "nnic.h"
#if NNNIC > 0

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <i386/isa/isa_device.h>
#include <gnu/i386/isa/nic3009.h>
#include <gnu/i386/isa/niccyreg.h>
#include <gnu/isdn/isdn_ioctl.h>


#define OPEN		1
#define LOAD_HEAD	3
#define LOAD_DATA	5
#define LOAD_ENTITY     8
#define IS_DIAL(p)	(((p)&0x20)==0)
#define IS_LISTEN(p)	((p)&0x20)
#define	CHAN(pl)	(((pl)&7)-1)
#define	C_CHAN(x)	((x)&1)
#define APPL(pl)	((((pl)>>6)&0x7f)-1)
#define CARD(pl)	(((pl)>>13)&7)
#define MK_APPL(pl)	(((pl)+1)<<6)

#define min(a,b)	((a)<(b)?(a):(b))

extern isdn_appl_t isdn_appl[];
extern u_short isdn_state;
extern isdn_ctrl_t isdn_ctrl[];
extern int     ispy_applnr;
extern int Isdn_Appl, Isdn_Ctrl, Isdn_Typ;

static old_spy= 0;

static int	nnicattach __P((struct isa_device *is));
static int	nnicprobe __P((struct isa_device *is));
static int	nnic_accept __P((int cn, int an, int rea));
static int	nnic_connect __P((int cn, int ap, int b_channel, int inf_mask,
				  int out_serv, int out_serv_add,
				  int src_subadr, unsigned ad_len,
				  char *dest_addr, int spv));
static int	nnic_disconnect __P((int cn, int rea));
static int	nnic_listen __P((int cn, int ap, int inf_mask,
				 int subadr_mask, int si_mask, int spv));
static int	nnic_output __P((int cn));
static int	nnic_state __P((int cn));

static	d_open_t	nnicopen;
static	d_close_t	nnicclose;
static	d_ioctl_t	nnicioctl;

#define CDEV_MAJOR 60
static struct cdevsw nnic_cdevsw = 
	{ nnicopen,	nnicclose,	noread,		nowrite,	/*60*/
	  nnicioctl,	nostop,		nullreset,	nodevtotty,/* nnic */
	  seltrue,	nommap,		NULL,	"nnic",	NULL,	-1 };


static short    bsintr;

struct isa_driver nnicdriver = {nnicprobe, nnicattach, "nnic"};

typedef enum
{
	DISCON, ISDISCON, DIAL, CALLED, CONNECT, IDLE, ACTIVE
}               io_state;

typedef struct
{
	char            ctrl;
	u_char          msg_nr;
	u_char		morenr;
	short           plci;
	short           ncci;
	short           state;
	short           i_len;
	char            i_buf[2048];
	char            o_buf[2048];
	u_short         more;
	char           *more_b;
}               chan_t;

static struct nnic_softc
{
	dpr_type       *sc_dpr;	/* card RAM virtual memory base */
	u_short         sc_vector;	/* interrupt vector 		 */
	short           sc_port;
	u_char          sc_flags;
	u_char          sc_unit;
	u_char          sc_ctrl;
	u_char          sc_type;
	short           sc_stat;
	chan_t          sc_chan[2];
#ifdef	DEVFS
	void		*devfs_token;
#endif
}               nnic_sc[NNNIC];

static void	badstate __P((mbx_type *mbx, int n, int mb, dpr_type *dpr));
static int	con_b3_resp __P((int unit, int mb, u_short ncci, u_short pl,
				 u_char reject));
static int	discon_req __P((int w, int unit, int pl, int rea, int err));
static int	con_resp __P((int unit, int pl, int rea));
static void	dn_intr __P((unsigned unit, struct nnic_softc *sc));
static int	en_q __P((int unit, int t, int st, int pl, int l, u_char *b));
static void	make_intr __P((void *gen));
static void	nnnicintr __P((void *gen));
static void	nnic_reset __P((struct nnic_softc *sc, int reset));
static int	reset_plci __P((int w, chan_t *chan, int p));
static int	sel_b2_prot_req __P((int unit, int c, int pl, dlpd_t *dlpd));
static int	sel_b3_prot_req __P((int unit, int mb, u_short pl,
				     ncpd_t *ncpd));
static void	up_intr __P((unsigned unit, struct nnic_softc *sc));

static int
nnicprobe(struct isa_device * is)
{
	register struct nnic_softc *sc = &nnic_sc[is->id_unit & 127];
	dpr_type       *dpr;
	u_char *w;
	int i;

	sc->sc_vector = is->id_irq;
	sc->sc_port = is->id_iobase;
	sc->sc_unit = is->id_unit;
	w= (u_char *) dpr = sc->sc_dpr = (dpr_type *) is->id_maddr;

	i= ffs(sc->sc_vector)-1;
	if(i == 9) i= 1;
	outb(sc->sc_port, 0);
	outb(sc->sc_port+1, i);
	outb(sc->sc_port, ((unsigned) dpr >> 12) & 0xff);

/* There should be memory, so lets test that */
	for (i=0;i<DPR_LEN;i++)
		w[i] = (i+0xaf) & 0xff;
	for (i=0;i<DPR_LEN;i++)
		if (w[i] != ((i+0xaf) & 0xff))
		{
			printf("Niccy card not found or bad memory %p\n",
				is->id_maddr);
			outb(sc->sc_port, 0);
			return(0);
		}
	bzero(w,DPR_LEN-4);

	is->id_msize = DPR_LEN;
	return (2);
}

static void
nnic_reset(struct nnic_softc *sc, int reset)
{
	u_char o;
	o= ffs(sc->sc_vector)-1;
	if(reset == 0)
		o|= 0x80;
	outb(sc->sc_port+1,o);
}

/*
 * nnicattach() Install device
 */
static int
nnicattach(struct isa_device * is)
{
	struct nnic_softc *sc;
	int             cn;
	isdn_ctrl_t    *ctrl0, *ctrl1;

	sc = &nnic_sc[is->id_unit];
	sc->sc_ctrl = -1;
	if ((cn = isdn_ctrl_attach(2)) == -1)
	{
		return (0);
	}
	sc->sc_ctrl = cn;
	sc->sc_chan[0].plci = sc->sc_chan[1].plci = -1;

	ctrl0 = &isdn_ctrl[cn];
	ctrl1 = &isdn_ctrl[cn + 1];
	sc->sc_chan[0].ctrl = ctrl0->ctrl = cn;
	sc->sc_chan[1].ctrl = ctrl1->ctrl = cn + 1;
	ctrl0->o_buf = sc->sc_chan[0].o_buf;
	ctrl1->o_buf = sc->sc_chan[1].o_buf;
	ctrl0->listen = ctrl1->listen = nnic_listen;
	ctrl0->disconnect = ctrl1->disconnect = nnic_disconnect;
	ctrl0->accept = ctrl1->accept = nnic_accept;
	ctrl0->connect = ctrl1->connect = nnic_connect;
	ctrl0->output = ctrl1->output = nnic_output;
	ctrl0->state = ctrl1->state = nnic_state;
	ctrl0->unit = ctrl1->unit = is->id_unit;
	ctrl0->appl = ctrl1->appl = -1;
	ctrl0->o_len = ctrl1->o_len = -1;
	sc->sc_flags= LOAD_ENTITY;
#ifdef	DEVFS
	sc->devfs_token = 
		devfs_add_devswf(&nnic_cdevsw, is->id_unit, DV_CHR, 0, 0, 
				0600, "/isdn/nnic%d", is->id_unit);
#endif

	return (1);
}

#define con_b3_req(unit,mb,pl)	en_q(unit,mb|BD_CONN_B3_REQ,0,pl,0,NULL)
#define con_act_resp(unit,pl)	en_q(unit,DD_CONN_ACT_RSP,0, pl,0,NULL)
#define discon_resp(sc,pl)	en_q(unit,DD_DISC_RSP,0, pl,0,NULL)
#define inf_resp(unit,pl)	en_q(unit,DD_INFO_RSP,0, pl,0,NULL)
#define listen_b3_req(unit,mb,pl) en_q(unit,mb|BD_LIST_B3_REQ,0,pl,0,NULL)

/* If the Niccy card wants it: Interupt it. */
static void
make_intr(void *gen)
{
	struct nnic_softc * sc = (struct nnic_softc *)gen;
	dpr_type       *dpr = sc->sc_dpr;

	dpr->watchdog_cnt = 0xFF;

	if (dpr->int_flg_nic)
		dpr->signal_pc_to_niccy++;
}

static int
en_q(int unit, int t, int st, int pl, int l, u_char *b)
{
	struct nnic_softc * sc= &nnic_sc[unit];
	dpr_type       *dpr = sc->sc_dpr;
	mbx_type       *mbx = &dpr->dn_mbx;

/*
	if (dpr->card_state & ~4)
		return (ENODEV);
*/
	if (mbx->msg_flag)
		return (EBUSY);

	bzero(&mbx->type, 18);
	mbx->type = t;
	mbx->subtype = st;
	mbx->plci = pl;
	if (l)
	{
		mbx->data_len = l;
		bcopy(b, mbx->data, l);
	}
	mbx->msg_flag = 1;
	make_intr(sc);
	return (0);
}

static void
badstate(mbx_type * mbx, int n, int mb, dpr_type *dpr)
{
	printf("Niccy: not implemented %x len %d at %d.", mbx->type,mbx->data_len,n);
	if(mbx->data_len)
	{
		int i;

		for(i=0; i<mbx->data_len; i++) printf(" %x",mbx->data[i]);
	}
	printf("\n");
}

static int
nnic_state(int cn)
{
	isdn_ctrl_t    *ctrl = &isdn_ctrl[cn];
	struct nnic_softc *sc = &nnic_sc[ctrl->unit];
	chan_t         *chan0 = &sc->sc_chan[0];
	chan_t         *chan1 = &sc->sc_chan[1];
	dpr_type       *dpr = sc->sc_dpr;

	if(sc->sc_flags == LOAD_ENTITY)
		return(ENODEV);
	if (dpr->card_state & ~4 )
		return (ENODEV);
	if (dpr->card_state & 4 )
		return (EAGAIN);
	if(chan0->state && chan1->state)
		return(EBUSY);
	return(0);
}

static int
nnic_output(int cn)
{
	isdn_ctrl_t    *ctrl = &isdn_ctrl[cn];
	struct nnic_softc *sc = &nnic_sc[ctrl->unit];
	chan_t         *chan = &sc->sc_chan[C_CHAN(cn)];
	dpr_type       *dpr = sc->sc_dpr;
	mbx_type       *mbx = &dpr->dn_mbx;
	int             l;
	int		len= ctrl->o_len;

	if (dpr->card_state /* & ~4 */)
		return (ENODEV);

	if (bsintr || (chan->ncci == -1) || mbx->msg_flag || (chan->state != IDLE))
		return (EBUSY);

	chan->state = ACTIVE;

	bzero(&mbx->type, 20);
	mbx->type = BD_DATA_B3_REQ;
	if (C_CHAN(cn))
		mbx->type |= 0x40;
	*(u_short *) mbx->data = chan->ncci;
	mbx->data[4] = chan->msg_nr;
	l = min(DATAFIELD_LEN-5, len);
	mbx->data_len = l+5;
	bcopy(ctrl->o_buf, &mbx->data[5], l);

	if (l < len)
	{
		chan->more = len - l;
		chan->more_b = ctrl->o_buf + l;
		mbx->more_data = 1;
		bsintr = C_CHAN(cn)+1;
	} else
	{
		chan->more = 0;
		ctrl->o_len = -1;
		bsintr= 0;
		++chan->msg_nr;
	}

	mbx->msg_flag = 1;
	make_intr(sc);
	ctrl->lastact = time.tv_sec;
	return (0);
}

static int
con_resp(int unit, int pl, int rea)
{
	return(en_q(unit, DD_CONN_RSP, 0, pl, 1, (u_char *) & rea));
}

static int
reset_plci(int w, chan_t * chan, int p)
{
	isdn_ctrl_t    *ctrl;

	if (p == -1)
		return (-1);

	if(chan == NULL)
		return(p);

	ctrl = &isdn_ctrl[chan->ctrl];
	if (chan->plci == p)
	{
		if (ISBUSY(ctrl->appl))
		{
			isdn_disconn_ind(ctrl->appl);
			isdn_appl[ctrl->appl].ctrl = -1;
			isdn_appl[ctrl->appl].state = 0;
		}
		ctrl->appl = -1;
		ctrl->o_len = -1;
		chan->plci = -1;
		chan->ncci = -1;
		chan->state = DISCON;
		chan->i_len = 0;
		chan->more = 0;
	}
	return (p);
}

static int
sel_b2_prot_req(int unit, int c, int pl, dlpd_t * dlpd)
{
	return(en_q(unit, (c ? 0x40 : 0)| BD_SEL_PROT_REQ, 2, pl, sizeof(dlpd_t), (u_char *) dlpd));
}

static int
sel_b3_prot_req(int unit, int mb, u_short pl, ncpd_t * ncpd)
{
	return(en_q(unit, mb | BD_SEL_PROT_REQ, 3, pl, sizeof(ncpd_t), (u_char *) ncpd));
}

static int
discon_req(int w, int unit , int pl, int rea, int err)
{
	if((pl == 0) || (pl == -1))
		return(0);
	return(en_q(unit, DD_DISC_REQ,0, pl, 1, (u_char *) &rea));
}

static int
con_b3_resp(int unit, int mb, u_short ncci, u_short pl, u_char reject)
{
	u_char          buf[32];
	int             l = 4;

	bzero(buf, 32);
	*(u_short *) buf = ncci;
	buf[2] = reject;
	buf[3] = 0; /* ncpi ??? */
	l += 15;
	return(en_q(unit, mb | BD_CONN_B3_RSP,0, pl, l, buf));
}

static int
nnic_connect(int cn, int ap, int b_channel, int inf_mask, int out_serv
	    ,int out_serv_add, int src_subadr, unsigned ad_len
	    ,char *dest_addr, int spv)
{
	char            buf[128];

	if (ad_len > 118)
		return (-1);

	buf[0] = spv ? 0x53 : 0;
	buf[1] = b_channel;
	if (spv)
		inf_mask |= 0x40000000;
	*(u_long *) & buf[2] = inf_mask;
	buf[6] = out_serv;
	buf[7] = out_serv_add;
	buf[8] = src_subadr;
	buf[9] = ad_len;
	bcopy(dest_addr, &buf[10], ad_len);
	return (en_q(isdn_ctrl[cn].unit, DD_CONN_REQ, 0, MK_APPL(ap), ad_len + 10, buf));
}

static int
nnic_listen(int cn, int ap, int inf_mask, int subadr_mask, int si_mask, int spv)
{
	u_short         sbuf[4];

	if (spv)
		inf_mask |= 0x40000000;
	*(u_long *) sbuf = inf_mask;
	sbuf[2] = subadr_mask;
	sbuf[3] = si_mask;
	return (en_q(isdn_ctrl[cn].unit, DD_LISTEN_REQ, 0, MK_APPL(ap), 8, (u_char *) sbuf));
}

static int
nnic_disconnect(int cn, int rea)
{
	isdn_ctrl_t    *ctrl = &isdn_ctrl[cn];
	chan_t         *chan = &nnic_sc[ctrl->unit].sc_chan[C_CHAN(cn)];
	int             p, err;
	u_char		buf[16];

	if(chan->ncci != -1)
	{
		bzero(buf,16);
		*(u_short *) buf = chan->ncci;
		err= en_q(ctrl->unit, (C_CHAN(cn)?0x40:0)|BD_DISC_B3_REQ, 0
				, chan->plci, 3+sizeof(ncpi_t), buf);
		if((err==0) && (ctrl->o_len == 0))
			ctrl->o_len= -1;
		return(err);
	}
	p = chan->plci;
	if ((p == 0) || (p == -1))
		return (ENODEV);

	err= en_q(ctrl->unit, DD_DISC_REQ, 0, p, 1, (u_char *) &rea);
	if((err==0) && (ctrl->o_len == 0))
		ctrl->o_len= -1;
	return(err);
}

static int
nnic_accept(int cn, int an, int rea)
{
	isdn_ctrl_t    *ctrl = &isdn_ctrl[cn];
	struct nnic_softc *sc = &nnic_sc[ctrl->unit];
	chan_t         *chan = &sc->sc_chan[C_CHAN(cn)];
	isdn_appl_t    *appl  = &isdn_appl[an];

	if(ISFREE(ctrl->appl))
		return(ENODEV);

	if (rea)
	{
		ctrl->appl= -1;
		return(discon_req(1, ctrl->unit, chan->plci, rea, 0));
	}
	ctrl->appl= an;
	ctrl->lastact = time.tv_sec;
	appl->ctrl= cn;
	appl->state= 4;

	return(sel_b2_prot_req(ctrl->unit, C_CHAN(cn), chan->plci, &appl->dlpd));
}

static	int
nnicopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct nnic_softc *sc;
	u_char          unit;
	int             x;
	unsigned	error;
	u_char	b= 0xff;

	unit = minor(dev);
	/* minor number out of limits ? */
	if (unit >= NNNIC)
		return (ENXIO);
	sc = &nnic_sc[unit];

	x= splhigh();
	/* Card busy ? */
/*
	if (sc->sc_flags & 7)
	{
		splx(x);
		return (EBUSY);
	}
*/
	sc->sc_flags |= OPEN;

	splx(x);
	return (0);
}

static	int
nnicclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct nnic_softc *sc = &nnic_sc[minor(dev)];

	sc->sc_flags &= ~7;
	return (0);
}

static	int
nnicioctl(dev_t dev, int cmd, caddr_t data, int flags, struct proc *pr)
{
	int             error;
	int             i, x;
	struct nnic_softc *sc = &nnic_sc[minor(dev)];
	dpr_type       *dpr = sc->sc_dpr;
	mbx_type       *mbx= &dpr->dn_mbx;
	u_char	*p= (u_char *)dpr;
	struct head    *head = (struct head *) data;

	error = 0;
	switch (cmd)
	{
	case NICCY_DEBUG:
		data[0]= 0x39;
		bcopy(p, &data[1], 2044);
		break;
	case NICCY_LOAD:
		switch(head->status)
		{
		case 0: /* Loading initial boot code */
			x = splhigh();
			nnic_reset(sc,1);
			if(error = copyin(head->data+0x16,p, head->d_len-0x16))
			{
				splx(x);
				return (error);
			}
			nnic_reset(sc,0);
			i= hz;
			while (p[7] && i--)
			{
				error = tsleep((caddr_t) sc, PZERO | PCATCH, "l9_0", 1);
				if (error != EWOULDBLOCK)
				{
					splx(x);
					return (error);
				}
			}
			nnic_reset(sc,1);
			splx(x);
			if(p[7]) return(ENODEV);
			return(0);

		case 1: /* Loading boot code */
			x = splhigh();

			if(error = copyin(head->data+0x64,p, head->d_len-0x64))
			{
				splx(x);
				return (error);
			}
			nnic_reset(sc,0);
			i= 5*hz;
			while ((dpr->mainloop_cnt != 0x1147) && i--)
			{
				error = tsleep((caddr_t) sc, PZERO | PCATCH, "l9_1", 1);
				if (error != EWOULDBLOCK)
				{
					splx(x);
					return (error);
				}
			}

			if(dpr->mainloop_cnt != 0x1147)
			{
				splx(x);
				return(ENODEV);
			}

			i= 1*hz;
			while ((dpr->up_mbx.type != 1) && i--)
			{
				error = tsleep((caddr_t) sc, PZERO | PCATCH, "l9_2", 1);
				if (error != EWOULDBLOCK)
				{
					splx(x);
					return (error);
				}
			}
			if(dpr->up_mbx.type != 1)
			{
				splx(x);
				return(ENODEV);
			}
			bzero(&mbx->type, 16);
			dpr->up_mbx.msg_flag= 0;
			mbx->type= 0x21;
			mbx->msg_flag= 1;

			i= 1*hz;
			while (mbx->msg_flag && i--)
			{
				error = tsleep((caddr_t) sc, PZERO | PCATCH, "l9_3", 1);
				if (error != EWOULDBLOCK)
				{
					splx(x);
					return (error);
				}
			}
			if(mbx->msg_flag)
			{
				splx(x);
				return(ENODEV);
			}

			head->status= 0;
			splx(x);
			return(0);

		default:
			x = splhigh();
			while (mbx->msg_flag)
			{
				error = tsleep((caddr_t) sc, PZERO | PCATCH, "l9_1h", 1);
				if (error != EWOULDBLOCK)
				{
					splx(x);
					return (error);
				}
			}

			bzero(&mbx->type, 16);
			mbx->type = MD_DNL_MOD_REQ;
			mbx->subtype = head->typ;
			sc->sc_type = head->typ;
			mbx->data_len = 12;
			bcopy(head->nam, mbx->data, 8);
			*(u_long *) (mbx->data + 8) = head->len;

			mbx->msg_flag = 1;
			make_intr(sc);
			i= 1*hz;
			while ((dpr->up_mbx.msg_flag == 0) && i--)
			{
				error = tsleep((caddr_t) sc, PZERO | PCATCH, "l9_2", 1);
				if (error != EWOULDBLOCK)
				{
					splx(x);
					return (error);
				}
			}

			if((dpr->up_mbx.type != MU_DNL_MOD_CNF) || dpr->up_mbx.data[0])
			{
				dpr->up_mbx.msg_flag= 0;
				make_intr(sc);
				splx(x);
				return(ENODEV);
			}
			dpr->up_mbx.msg_flag= 0;
			make_intr(sc);
			{
				int len, l, off;
				len= head->d_len;
				off= 0;
				l= 0x64;

				while(len > 0)
				{
					while (mbx->msg_flag)
					{
						error = tsleep((caddr_t) sc, PZERO | PCATCH, "l9_4load", 1);
						if (error != EWOULDBLOCK)
						{
							splx(x);
							return (error);
						}
					}
					mbx->type = MD_DNL_MOD_DATA;
					len-= l;
					mbx->more_data = len > 0;
					mbx->data_len = l;
					if(error= copyin(head->data+off, mbx->data, l))
					{
						splx(x);
						return (error);
					}
					off+= l;
					l= min(len,512);
					mbx->msg_flag = 1;
					make_intr(sc);
				}
			}

			i= 3*hz;
			while ((dpr->up_mbx.msg_flag == 0) && i--)
			{
				error = tsleep((caddr_t) sc, PZERO | PCATCH, "l9_2", 1);
				if (error != EWOULDBLOCK)
				{
					splx(x);
					return (error);
				}
			}
			if(dpr->up_mbx.type == 0)
			{
				dpr->up_mbx.msg_flag= 0;
				make_intr(sc);
				i= 3*hz;
				while ((dpr->up_mbx.msg_flag == 0) && i--)
				{
					error = tsleep((caddr_t) sc, PZERO | PCATCH, "l9_3", 1);
					if (error != EWOULDBLOCK)
					{
						splx(x);
						return (error);
					}
				}
			}

			if(dpr->up_mbx.type != MU_DNL_MOD_IND)
			{
				dpr->up_mbx.msg_flag= 0;
				make_intr(sc);
				splx(x);
				return(ENODEV);
			}
			head->status = dpr->up_mbx.data[0];
			mbx->data[0] = dpr->up_mbx.data[1];
			mbx->type= 0x20|MU_DNL_MOD_IND;
			mbx->subtype = head->typ;
			mbx->data_len = 1;
			dpr->card_number = sc->sc_unit;
			dpr->up_mbx.msg_flag= 0;
			mbx->msg_flag= 1;
			make_intr(sc);
			splx(x);
			return (0);
		}
		splx(x);
		return(0);
	case NICCY_SET_CLOCK:
		x = splhigh();
		dpr->int_flg_pc = 0xff;
		dpr->card_number = sc->sc_unit;
		if (mbx->msg_flag)
		{
			splx(x);
			return (EBUSY);
		}
		bzero(&mbx->type, 16);
		mbx->type = MD_SET_CLOCK_REQ;
		mbx->data_len = 14;
		bcopy(data, mbx->data, 14);

		mbx->msg_flag = 1;
		make_intr(sc);
		splx(x);
		return (0);
	case NICCY_SPY:
		x = splhigh();
		if (mbx->msg_flag)
		{
			splx(x);
			return (EBUSY);
		}
		bzero(&mbx->type, 16);
		mbx->type = MD_MANUFACT_REQ;
		mbx->subtype = 18;
		mbx->data_len = 1;
		mbx->plci = MK_APPL(ispy_applnr);
/* There are ilegal states. So I use them to toggle */
		if((data[0] == 0) && (old_spy == 0)) data[0]= 255;
		else if(data[0] && old_spy ) data[0]= 0;
		old_spy= mbx->data[0]= data[0];

		mbx->msg_flag = 1;
		make_intr(sc);
		splx(x);
		return (0);

	case NICCY_RESET:
		x = splhigh();
		nnic_reset(sc,1);
		bzero((u_char*)dpr,DPR_LEN);
		sc->sc_flags= LOAD_ENTITY;
		splx(x);
		return (0);

	default:
		error = ENODEV;
	}
	return (error);
}

static void
dn_intr(unsigned unit, struct nnic_softc * sc)
{
	dpr_type       *dpr = sc->sc_dpr;
	mbx_type *mbx= &dpr->dn_mbx;
	chan_t         *chan;
	isdn_ctrl_t    *ctrl;
	int c,l, len;

	c= bsintr-1;
	chan = &sc->sc_chan[c];
	ctrl = &isdn_ctrl[chan->ctrl];

	if ((chan->state == ACTIVE) && (chan->more))
	{
		len= chan->more;

		bzero(&mbx->type, 20);
		mbx->type = BD_DATA_B3_REQ;
		if (c)
			mbx->type |= 0x40;
		*(u_short *) mbx->data = chan->ncci;
		mbx->data[4] = chan->msg_nr;
		l = min(DATAFIELD_LEN-5, len);
		mbx->data_len = l+5;
		bcopy(chan->more_b, &mbx->data[5], l);

		if (l < len)
		{
			chan->more = len - l;
			chan->more_b += l;
			mbx->more_data = 1;
		} else
		{
			chan->more = 0;
			ctrl->o_len = -1;
			bsintr= 0;
			++chan->msg_nr;
		}

		mbx->msg_flag = 1;
		make_intr(sc);
		ctrl->lastact = time.tv_sec;
		return;
	}
	bsintr= 0;
}

static void
up_intr(unsigned unit, struct nnic_softc * sc)
{
	dpr_type       *dpr = sc->sc_dpr;
	mbx_type *msg= &dpr->up_mbx;
	chan_t         *chan;
	u_short        n, mb, c, pl, err = 0;
	isdn_ctrl_t    *ctrl;
	isdn_appl_t    *appl;
	int error= 0;

	chan= NULL;
	ctrl= NULL;
	appl= NULL;
	mb= 0;
	pl = msg->plci;

	if(pl && (msg->type >= 0x40) && (msg->type < 0xfd) && (msg->type != 0x47))
	{
		if ((c = CHAN(pl)) < 2)
		{
			chan = &sc->sc_chan[c];
			ctrl = &isdn_ctrl[chan->ctrl];
		} else
		{
			c = 0xffff;
			chan= NULL;
			ctrl= NULL;
		}

		if(ctrl && (ctrl->appl & 0xC0) == 0)
			appl= &isdn_appl[ctrl->appl];
		else if( APPL(pl) < 0x30)
			appl = &isdn_appl[APPL(pl)];
		else if( APPL(pl) < 0x40)
			appl= NULL;
		else goto fin;

		if(msg->type >= 0x80)
		{
			mb= msg->type & 0x40;
			msg->type &= 0xbf;
		}
	}

	switch (msg->type)
	{
	case 0x01:		/* INIT IND */
		if(dpr->dn_mbx.msg_flag) return;
		error= en_q(unit,msg->type|0x20,0,0,0,NULL);
		break;
	case 0x04:		/* DNL MOD  */
		sc->sc_stat = msg->data[0];
		if (sc->sc_flags )
			sc->sc_flags = OPEN;
		break;
	case 0x06:		/* DNL MOD IND */
		if(dpr->dn_mbx.msg_flag) return;
		sc->sc_stat = msg->data[0];
		if (sc->sc_flags)
			sc->sc_flags = OPEN;
		if(sc->sc_stat)
			break;
		error= en_q(unit,msg->type|0x20,sc->sc_type,0,1, &msg->data[1]);
		break;
	case 0x0e:		/* SET CLOCK CONF */
		dpr->card_number = unit;
/*
		dpr->api_active   = 1;
		dpr->watchdog_cnt = 0xFF;
		dpr->api_area[0] = 0;
		dpr->api_area[2] = 0;
*/
		dpr->int_flg_pc   = 0xFF;
		break;
	case 0x15:		/* POLL IND */
		if(dpr->dn_mbx.msg_flag) return;
		dpr->api_active   = 1;
		dpr->watchdog_cnt = 0xFF;
		dpr->int_flg_pc   = 0xFF;
		error= en_q(unit,msg->type|0x20,0,0,0,NULL);
		break;
	case 0x16:		/* STATE IND */
		if(dpr->dn_mbx.msg_flag) return;
		if(sc->sc_flags & LOAD_ENTITY)
		{
			if(sc->sc_flags & 7)
				sc->sc_flags = OPEN;
			else sc->sc_flags= 0;
		}
		error= en_q(unit,msg->type|0x20,0,0,0,NULL);
		break;
	case 0x17:		/* STATE RESP */
		break;
	case 0x1e:              /* MANUFACT CONF */
		if(msg->subtype == 18)
			break;
		badstate(msg,1,0,dpr);
		break;
	case 0x1f:              /* MANUFACT IND */
		if(msg->subtype == 19)
		{
			if(dpr->dn_mbx.msg_flag) return;
			isdn_input(ispy_applnr, msg->data_len, msg->data,0);
			error= en_q(unit,msg->type|0x20,msg->subtype,0,0,NULL);
			break;
		}
		badstate(msg,2,0,dpr);
		break;
	case 0x40:		/* CONNECT CONF */
		err = *(u_short *) msg->data;
		if (err || (appl == NULL) || (chan == NULL) || (ctrl == NULL))
		{
			if(chan) reset_plci(3, chan, pl);
			if(appl) appl->state= 0;
			break;
		}
		if (ISBUSY(ctrl->appl))
		{
			if(dpr->dn_mbx.msg_flag) return;
			error= discon_req(2, unit, pl, 0, 0);
			break;
		}
		chan->plci = pl;
		chan->msg_nr = 0;
		chan->ncci = -1;
		ctrl->lastact = time.tv_sec;
		ctrl->appl = APPL(pl);
		appl->ctrl = chan->ctrl;
		ctrl->islisten= 0;
		chan->state = DIAL;
		appl->state= 3;
		break;

	case 0x41:		/* CONNECT IND */
		if (ISBUSY(ctrl->appl))
		{
			if(dpr->dn_mbx.msg_flag) return;
			error= discon_req(3, unit, pl, 0, 0);
			break;
		}
		chan->plci = pl;
		chan->msg_nr = 0;
		chan->ncci = -1;
		ctrl->lastact = time.tv_sec;
		ctrl->appl = 0x7f;
		ctrl->islisten= 1;
		chan->state = CALLED;
		msg->data[msg->data[3] + 4] = 0;
		isdn_accept_con_ind(APPL(pl), chan->ctrl, msg->data[0], msg->data[1]
		       ,msg->data[2], msg->data[3], (char *) &msg->data[4]);
		break;

	case 0x42:		/* CONNECT ACTIVE IND */
		if(dpr->dn_mbx.msg_flag) return;
		error= con_act_resp(unit, pl);
		if (IS_LISTEN(pl))
		{
			isdn_conn_ind(ctrl->appl,chan->ctrl,0);
			break;
		}
		isdn_conn_ind(APPL(pl),chan->ctrl,1);
		chan->state = CONNECT;
		ctrl->appl = APPL(pl);
		appl->ctrl = chan->ctrl;
		break;

	case 0x43:		/* DISCONNECT CONF */
		reset_plci(4, chan, pl);
		break;

	case 0x44:		/* DISCONNECT IND */
		if(dpr->dn_mbx.msg_flag) return;
		error= discon_resp(unit, reset_plci(5, chan, pl));
		break;

	case 0x47:		/* LISTEN CONF */
		isdn_state = *(u_short *) msg->data;
		break;

	case 0x4a:		/* INFO IND */
		if(dpr->dn_mbx.msg_flag) return;
		isdn_info(APPL(pl),*(u_short *)msg->data, msg->data[2], msg->data+3);
		error= inf_resp(unit, pl);
		break;
	case 0x80:	/* SELECT PROT CONF */
		if(dpr->dn_mbx.msg_flag) return;
		err = *(u_short *) msg->data;
		if (err)
		{
			error= discon_req(4, unit, pl, 0, err);
			break;
		}

		switch (msg->subtype)
		{
		case 2:/* SELECT B2 PROTOCOL */
			if(ISFREE(ctrl->appl))
				break;
			error= sel_b3_prot_req(unit, mb, pl, &isdn_appl[ctrl->appl].ncpd);
			break;

		case 3:/* SELECT B3 PROTOCOL */
			if (IS_DIAL(pl))
				error= con_b3_req(unit, mb, pl);
			else
				error= listen_b3_req(unit, mb, pl);
			break;
		}
		break;

	case 0x81:	/* LISTEN B3 CONF */
		if(dpr->dn_mbx.msg_flag) return;
		err = *(u_short *) msg->data;
		if (err)
		{
			error= discon_req(5, unit, pl, 0, err);
			break;
		}
		error= con_resp(unit, pl, 0);
		break;

	case 0x82:	/* CONNECT B3 CONF */
		err = *(u_short *) (msg->data + 2);
		n = *(u_short *) msg->data;

		if (err)
		{
			if(dpr->dn_mbx.msg_flag) return;
			error= discon_req(6, unit, pl, 0, err);
			break;
		}
		if(ISFREE(ctrl->appl))
			break;
		chan->ncci = n;
		chan->state = CONNECT;
		break;

	case 0x83:	/* CONNECT B3 IND */
		if(ISFREE(ctrl->appl))
			break;
		if(dpr->dn_mbx.msg_flag) return;
		n = *(u_short *) msg->data;
		chan->ncci = n;
		chan->state = CONNECT;
		error= con_b3_resp(unit, mb, n, pl, 0);
		break;

	case 0x84:	/* CONNECT B3 ACTIVE IND */
		if(ISFREE(ctrl->appl))
			break;
		if (chan->state < IDLE)
		{
			chan->state = IDLE;
			ctrl->o_len = 0;
			/*
			 * XXX the chan->ctrl arg is very bogus.
			 * Don't just use a cast to "fix" it.
			 */
			timeout(isdn_start_out, chan->ctrl, hz / 5);
		}
		break;

	case 0x85:	/* DISCONNECT B3 CONF */
		if(ISBUSY(ctrl->appl))
			chan->state = ISDISCON;
		err = *(u_short *) (msg->data + 2);
		if (err)
		{
			if(dpr->dn_mbx.msg_flag) return;
			error= discon_req(7, unit, pl, 0, err);
			break;
		}
		break;
	case 0x86:	/* DISCONNECT B3 IND */
		if(dpr->dn_mbx.msg_flag) return;
		if(ISBUSY(ctrl->appl))
			chan->state = ISDISCON;
		err = *(u_short *) (msg->data + 2);
		error= discon_req(8, unit, pl, 0, err);
		break;

	case 0x88:	/* DATA B3 CONF */
		if(ISFREE(ctrl->appl))
			break;
		err = *(u_short *) (msg->data + 2);
		if (err)
		{
			ctrl->send_err++;
			isdn_appl[ctrl->appl].send_err++;
		}
		chan->state = IDLE;
		ctrl->o_len = 0;
		isdn_start_out(chan->ctrl);
		break;

	case 0x89:	/* DATA B3 IND */
		if(ISFREE(ctrl->appl))
			break;
		if (msg->more_data)
		{
			if(chan->i_len)
			{
				if((chan->morenr != msg->data[4]) || ((chan->i_len + msg->data_len - 5) > 2048))
					break;
			}
			else
				chan->morenr= msg->data[4];
			bcopy(msg->data+5, &chan->i_buf[chan->i_len], msg->data_len-5);
			chan->i_len += msg->data_len -5;
			break;
		} /* msg->more_data == 0 */
		if (chan->i_len)
		{
			int l;

			if(chan->morenr != msg->data[4])
				break;

			if ((l = chan->i_len + msg->data_len - 5) <= 2048)
			{
				bcopy(msg->data+5, &chan->i_buf[chan->i_len], msg->data_len);
				if(isdn_input(ctrl->appl, l, chan->i_buf, ctrl->islisten))
					ctrl->lastact = time.tv_sec;
			}
			chan->i_len = 0;
			break;
		} /* chan->i_len == 0 && msg->more_data == 0 */
		if(isdn_input(ctrl->appl, msg->data_len-5, msg->data+5,ctrl->islisten))
			ctrl->lastact = time.tv_sec;
		break;

	default:
		badstate(msg,3,mb,dpr);
		break;
	}

fin:
	if(error)
	{
printf("E?%x",error);
		return;
	}
	msg->msg_flag= 0;
	timeout(make_intr, (void *)sc,1);
}

static void
nnnicintr(void *gen)
{
	unsigned int unit = (int)gen;
	register struct nnic_softc *sc = &nnic_sc[unit];
	dpr_type       *dpr = sc->sc_dpr;

	if(dpr->up_mbx.msg_flag)
		up_intr(unit,sc);
	if (bsintr && (dpr->dn_mbx.msg_flag == 0))
		dn_intr(unit,sc);
}
void
nnicintr(int unit)
{
        timeout(nnnicintr, (void *)unit,1);
}


static nnic_devsw_installed = 0;

static void 	nnic_drvinit(void *unused)
{
	dev_t dev;

	if( ! nnic_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&nnic_cdevsw, NULL);
		nnic_devsw_installed = 1;
    	}
}

SYSINIT(nnicdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,nnic_drvinit,NULL)

#endif				/* NNNIC > 0 */
