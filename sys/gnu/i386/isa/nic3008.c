static char     nic38_id[] = "@(#)$Id: nic3008.c,v 1.12 1995/12/08 11:12:45 julian Exp $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.12 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: nic3008.c,v $
 * Revision 1.12  1995/12/08  11:12:45  julian
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
 * Revision 1.11  1995/11/29  14:39:07  julian
 * If you're going to mechanically replicate something in 50 files
 * it's best to not have a (compiles cleanly) typo in it! (sigh)
 *
 * Revision 1.10  1995/11/29  10:47:04  julian
 * OK, that's it..
 * That's EVERY SINGLE driver that has an entry in conf.c..
 * my next trick will be to define cdevsw[] and bdevsw[]
 * as empty arrays and remove all those DAMNED defines as well..
 *
 * Revision 1.9  1995/11/21  14:56:01  bde
 * Completed function declarations, added prototypes and removed redundant
 * declarations.
 *
 * Revision 1.8  1995/11/18  04:19:44  bde
 * Fixed the type of nic_listen().  A trailing arg was missing.
 *
 * Fixed calls to s_intr().  There was sometimes an extra trailing arg.
 *
 * Revision 1.7  1995/09/08  11:06:46  bde
 * Fix benign type mismatches in devsw functions.  82 out of 299 devsw
 * functions were wrong.
 *
 * Revision 1.6  1995/05/30  07:57:57  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.5  1995/05/11  19:25:55  rgrimes
 * Fix -Wformat warnings from LINT kernel.
 *
 * Revision 1.4  1995/03/28  07:54:31  bde
 * Add and move declarations to fix all of the warnings from `gcc -Wimplicit'
 * (except in netccitt, netiso and netns) that I didn't notice when I fixed
 * "all" such warnings before.
 *
 * Revision 1.3  1995/03/19  14:28:35  davidg
 * Removed redundant newlines that were in some panic strings.
 *
 * Revision 1.2  1995/02/15  11:59:40  jkh
 * Fix a few more nits.  Should compile better now! :_)
 *
 * Revision 1.1  1995/02/14  15:00:10  jkh
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
 * A low level driver for the NICCY-3008 ISDN Card.
 *
 */

#include "nic.h"
#if NNIC > 0

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
#include <gnu/i386/isa/nic3008.h>
#include <gnu/i386/isa/niccyreg.h>
#include <gnu/isdn/isdn_ioctl.h>


#define OPEN		1
#define LOAD_HEAD	3
#define LOAD_DATA	5
#define IS_DIAL(p)	(((p)&0x20)==0)
#define IS_LISTEN(p)	((p)&0x20)
#define	CHAN(pl)	(((pl)&7)-1)
#define	C_CHAN(x)	((x)&1)
#define APPL(pl)	((((pl)>>6)&0x7f)-1)
#define CARD(pl)	(((pl)>>13)&7)
#define MK_APPL(pl)	(((pl)+1)<<6)

#define con_act_resp(sc,pl)	en_q_d(sc,DD_CONN_ACT_RSP, pl ,0,NULL)
#define discon_resp(sc,pl)	en_q_d(sc,DD_DISC_RSP, pl ,0,NULL)
#define inf_resp(sc,pl)		en_q_d(sc,DD_INFO_RSP, pl ,0,NULL)
#define listen_b3_req(sc,mb,pl)	en_q_b(sc,mb,BD_LIST_B3_REQ,pl,0,NULL)
#define con_b3_req(sc,mb,pl)	en_q_b(sc,mb,BD_CONN_B3_REQ,pl,0,NULL)
#define min(a,b)	((a)<(b)?(a):(b))

extern isdn_appl_t isdn_appl[];
extern u_short isdn_state;
extern isdn_ctrl_t isdn_ctrl[];
extern int     ispy_applnr;
extern int Isdn_Appl, Isdn_Ctrl, Isdn_Typ;

static old_spy= 0;

extern int	nicattach __P((struct isa_device *is));
extern int	nicprobe __P((struct isa_device *is));
extern int	nic_accept __P((int cn, int an, int rea));
extern int	nic_connect __P((int cn, int ap, int b_channel, int inf_mask,
				 int out_serv, int out_serv_add,
				 int src_subadr, unsigned ad_len,
				 char *dest_addr, int spv));
extern int	nic_disconnect __P((int cn, int rea));
extern int	nic_listen __P((int cn, int ap, int inf_mask, int subadr_mask,
				int si_mask, int spv));
extern int	nic_output __P((int cn));

static short    bsintr;

struct isa_driver nicdriver = {nicprobe, nicattach, "nic"};

static	d_open_t	nicopen;
static	d_close_t	nicclose;
static	d_ioctl_t	nicioctl;

#define CDEV_MAJOR 54
static struct cdevsw nic_cdevsw = 
	{ nicopen,	nicclose,	noread,		nowrite,	/*54*/
	  nicioctl,	nostop,		nullreset,	nodevtotty,/* nic */
	  seltrue,	nommap,		NULL, "nic",	NULL,	-1 };

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

struct nic_softc
{
	dpr_type       *sc_dpr;	/* card RAM virtual memory base */
	u_short         sc_vector;	/* interrupt vector 		 */
	short           sc_port;
	u_char          sc_flags;
	u_char          sc_unit;
	u_char          sc_ctrl;
	short           sc_stat;
	chan_t          sc_chan[2];
#ifdef	DEVFS
	void		*devfs_token;
#endif
}               nic_sc[NNIC];

static void	badstate __P((mbx_type *mbx, int n, int mb, dpr_type *dpr));
static void	b_intr __P((int mb, int c, struct nic_softc *sc));
static void	bs_intr __P((int mb, int c, struct nic_softc *sc));
static void	con_b3_resp __P((struct nic_softc *sc, int mb, u_short ncci,
				 u_char reject));
static void	con_resp __P((struct nic_softc *sc, int pl, int rea));
static void	d_intr __P((struct nic_softc *sc));
static int	discon_req __P((int w, struct nic_softc *sc, int pl, int rea,
				int err));
static int	en_q_b __P((struct nic_softc *sc, int mb, int t, int pl, int l,
			    u_char *b));
static int	en_q_d __P((struct nic_softc *sc, int t, int pl, int l,
			    u_char *b));
static int	cstrcmp __P((char *str1, char *str2));
static void	make_intr __P((int box, struct nic_softc *sc));
static void	reset_card __P((struct nic_softc *sc));
static int	reset_plci __P((int w_is_defined_bletch, chan_t *chan, int p));
static void	reset_req __P((struct nic_softc *sc, unsigned box, int w));
static void	s_intr __P((struct nic_softc *sc));
static int	sel_b2_prot_req __P((struct nic_softc *sc, int c, int pl,
				     dlpd_t *dlpd));
static void	sel_b3_prot_req __P((struct nic_softc *sc, int mb, u_short pl,
				     ncpd_t *ncpd));

int
nicprobe(struct isa_device * is)
{
	register struct nic_softc *sc = &nic_sc[is->id_unit & 127];
	dpr_type       *dpr;

	sc->sc_vector = is->id_irq;
	sc->sc_port = is->id_iobase;
	sc->sc_unit = is->id_unit;
	dpr = sc->sc_dpr = (dpr_type *) is->id_maddr;

	if (cstrcmp(dpr->niccy_ver, "NICCY V ") == 0)
	{
		printf("NICCY NICCY-Card %d not found at %p\n"
		       ,is->id_unit, is->id_maddr);
		return (0);
	}
	while (dpr->card_state & 1);	/* self test running */

	if (dpr->card_state & 0x8A)
	{
		printf("Check Niccy Card, error state %d \n", dpr->card_state);
		return (0);
	}
	dpr->card_number = is->id_unit;
	is->id_msize = 8192;
	reset_card(sc);
	return (8);
}

/*
 * nicattach() Install device
 */
int
nicattach(struct isa_device * is)
{
	struct nic_softc *sc;
	dpr_type       *dpr;
	int             cn;
	isdn_ctrl_t    *ctrl0, *ctrl1;
	char		name[32];

	sc = &nic_sc[is->id_unit];
	dpr = sc->sc_dpr;
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
	ctrl0->listen = ctrl1->listen = nic_listen;
	ctrl0->disconnect = ctrl1->disconnect = nic_disconnect;
	ctrl0->accept = ctrl1->accept = nic_accept;
	ctrl0->connect = ctrl1->connect = nic_connect;
	ctrl0->output = ctrl1->output = nic_output;
	ctrl0->unit = ctrl1->unit = is->id_unit;
	ctrl0->appl = ctrl1->appl = -1;
	ctrl0->o_len = ctrl1->o_len = -1;

	while (dpr->card_state & 1);	/* self test running */
	dpr->card_number = is->id_unit;
	dpr->int_flg_pc = 0xff;
	reset_req(sc, MBX_MU, 4);
#ifdef	DEVFS
	sprintf(name,"nic%d",is->id_unit);
	sc->devfs_token = devfs_add_devsw( "/isdn", name,
		&nic_cdevsw,is->id_unit, DV_CHR, 0, 0, 0600 );
#endif
	return (1);
}

static int
cstrcmp(char *str1, char *str2)
{
	while (*str2 && (*str2 == *str1))
	{
		str1++;
		str2++;
	}
	if (!*str2)
		return (1);
	return (0);
}

/* If the niccy card wants it: Interupt it. */
static void
make_intr(int box, struct nic_softc * sc)
{
	dpr_type       *dpr = sc->sc_dpr;

	dpr->watchdog_cnt = 0xFF;
	if ((dpr->int_flg_nic & (1 << box)) == 0)
		return;
	if (dpr->ext_hw_config == 1)
	{
		u_char          s;
		s = inb(sc->sc_port + 4);
		outb(sc->sc_port + 4, s & 0xfb);
		outb(sc->sc_port + 4, s | 4);
		outb(sc->sc_port + 4, s);
		return;
	}
	outb(sc->sc_port + 2, 1);
}

static void
reset_req(struct nic_softc * sc, unsigned box, int w)
{
	if(box >= 8)
		return;

	(sc->sc_dpr)->msg_flg[box] = 0;
	make_intr(box, sc);
}

static int
en_q_d(struct nic_softc * sc, int t, int pl, int l, u_char * b)
{
	dpr_type       *dpr = sc->sc_dpr;
	mbx_type       *mbx = &dpr->dpr_mbx[3];

	if (dpr->card_state & ~4)
		return (ENODEV);
	if (dpr->msg_flg[3])
		return (EBUSY);

	bzero(mbx, 18);
	mbx->type = t;
	mbx->add_info = pl;
	if (l)
	{
		mbx->data_len = l;
		bcopy(b, mbx->data, l);
	}
	dpr->msg_flg[3] = 1;
	make_intr(3, sc);
	return (0);
}

static int
en_q_b(struct nic_softc * sc, int mb, int t, int pl, int l, u_char * b)
{
	dpr_type       *dpr = sc->sc_dpr;
	mbx_type       *mbx = &dpr->dpr_mbx[++mb];

	if (mb == 7)
		t |= 0x40;

	if (dpr->card_state)
		return (ENODEV);
	if (dpr->msg_flg[mb])
		return (EBUSY);

	bzero(mbx, 18);
	mbx->type = t;
	mbx->add_info = pl;
	if (l)
	{
		mbx->data_len = l;
		bcopy(b, mbx->data, l);
	}
	dpr->msg_flg[mb] = 1;
	make_intr(mb, sc);
	return (0);
}

static void
badstate(mbx_type * mbx, int n, int mb, dpr_type *dpr)
{
	printf("Niccy: not implemented %x len %d at %d.", mbx->type,mbx->data_len,n);
	if(mbx->data_len)
	{
		u_char         *b = (u_char *) dpr;
		int i;

		b += dpr->buf_ptr[mb];
		for(i=0; i<mbx->data_len; i++) printf(" %x",mbx->data[i]);
		printf(".");
		for(i=0; i<mbx->data_len; i++) printf(" %x",b[i]);
	}
	printf("\n");
}

int
nic_connect(int cn, int ap, int b_channel, int inf_mask, int out_serv
	    ,int out_serv_add, int src_subadr, unsigned ad_len
	    ,char *dest_addr, int spv)
{
	char            buf[128];

	if (ad_len > 22)
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
	return (en_q_d(&nic_sc[isdn_ctrl[cn].unit], DD_CONN_REQ, MK_APPL(ap), ad_len + 10, buf));
}

int
nic_listen(int cn, int ap, int inf_mask, int subadr_mask, int si_mask, int spv)
{
	u_short         sbuf[4];

	*(u_long *) sbuf = inf_mask;
	sbuf[2] = subadr_mask;
	sbuf[3] = si_mask;
	return (en_q_d(&nic_sc[isdn_ctrl[cn].unit], DD_LISTEN_REQ, MK_APPL(ap), 8, (u_char *) sbuf));
}

int
nic_disconnect(int cn, int rea)
{
	isdn_ctrl_t    *ctrl = &isdn_ctrl[cn];
	struct nic_softc *sc = &nic_sc[ctrl->unit];
	chan_t         *chan = &sc->sc_chan[C_CHAN(cn)];
	u_char          buf[16];
	int             l = 3;
	int	p;
	int err;

	if(chan->ncci != -1)
	{
		bzero(buf,16);
		*(u_short *) buf = chan->ncci;
		l += sizeof(ncpi_t);
		err= en_q_b(sc, C_CHAN(cn)?6:4, BD_DISC_B3_REQ, chan->plci, l, buf);
		if(err==0)
		{
			chan->more= 0;
			ctrl->o_len= -1;
		}
		return(err);
	}

	p = chan->plci;
	if((p == 0) || (p == -1))
		return (ENODEV);

	err= en_q_d(sc, DD_DISC_REQ, p, 1, (u_char *) & rea);
	if(err==0)
	{
		chan->more= 0;
		ctrl->o_len= -1;
	}
	return(err);
}

int
nic_accept(int cn, int an, int rea)
{
	isdn_ctrl_t    *ctrl = &isdn_ctrl[cn];
	struct nic_softc *sc = &nic_sc[ctrl->unit];
	chan_t         *chan = &sc->sc_chan[C_CHAN(cn)];
	isdn_appl_t    *appl = &isdn_appl[an];

	if (rea)
	{
		ctrl->appl= -1;
		return(discon_req(1, sc, chan->plci, rea, 0));
	}
	ctrl->appl= an;
	ctrl->lastact = time.tv_sec;
	appl->ctrl= cn;
	appl->state= 4;

	return(sel_b2_prot_req(sc, C_CHAN(cn), chan->plci, &appl->dlpd));
}

int
nic_output(int cn)
{
	isdn_ctrl_t    *ctrl = &isdn_ctrl[cn];
	struct nic_softc *sc = &nic_sc[ctrl->unit];
	chan_t         *chan = &sc->sc_chan[C_CHAN(cn)];
	int             mb = C_CHAN(cn) ? 7 : 5;
	dpr_type       *dpr = sc->sc_dpr;
	mbx_type       *mbx = &dpr->dpr_mbx[mb];
	int             r, l;
	u_char         *b = (u_char *) dpr;
	int		len= ctrl->o_len;
	char	       *buf= ctrl->o_buf;

	if (dpr->card_state /* & ~4 */)
		return (ENODEV);

	if ((chan->ncci == -1) || dpr->msg_flg[mb] || (chan->state != IDLE))
		return (EBUSY);

	chan->state = ACTIVE;

	bzero(mbx, 20);
	mbx->type = BD_DATA_B3_REQ;
	if (C_CHAN(cn))
		mbx->type |= 0x40;
	*(u_short *) mbx->data = chan->ncci;
	mbx->data[4] = chan->msg_nr++;
	b += dpr->buf_ptr[mb];
	l = min(1024, len);
	mbx->data_len = l;
	bcopy(buf, b, l);

	if (l < len)
	{
		chan->more = min(len - l, 1024);	/* This is a bug, but */
		/* max. blocks length is 2048 bytes including protokoll */
		chan->more_b = buf + l;
		mbx->more_data = 1;
	} else
	{
		chan->more = 0;
		ctrl->o_len = -1;
	}

	dpr->msg_flg[mb] = 3;
	bsintr |= (1 << C_CHAN(cn));
	make_intr(mb, sc);
	ctrl->lastact = time.tv_sec;
	return (0);
}

static void
con_resp(struct nic_softc * sc, int pl, int rea)
{
	en_q_d(sc, DD_CONN_RSP, pl, 1, (u_char *) & rea);
}

static int
discon_req(int w, struct nic_softc * sc, int pl, int rea, int err)
{
	if ((pl == 0) || (pl == -1))
		return(0);
	return(en_q_d(sc, DD_DISC_REQ, pl, 1, (u_char *) & rea));
}

static int
sel_b2_prot_req(struct nic_softc * sc, int c, int pl, dlpd_t * dlpd)
{
	return(en_q_b(sc, c ? 6 : 4, BD_SEL_PROT_REQ | 0x200, pl,
		sizeof(dlpd_t), (u_char *) dlpd));
}

static void
sel_b3_prot_req(struct nic_softc * sc, int mb, u_short pl, ncpd_t * ncpd)
{
	en_q_b(sc, mb, BD_SEL_PROT_REQ | 0x300, pl, sizeof(ncpd_t), (u_char *) ncpd);
}

static void
con_b3_resp(struct nic_softc * sc, int mb, u_short ncci, u_char reject)
{
	u_char          buf[32];
	int             l = 4;

	bzero(buf, 32);
	*(u_short *) buf = ncci;
	buf[2] = reject;
	buf[3] = 0; /* ncpi ???? */
	l += 15;
	en_q_b(sc, mb, BD_CONN_B3_RSP, 0, l, buf);
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

static void
reset_card(struct nic_softc * sc)
{
	dpr_type       *dpr = sc->sc_dpr;
	mbx_type       *mbx = &dpr->dpr_mbx[1];
	bzero(mbx, 16);
	mbx->type = MD_RESET_REQ;
	dpr->msg_flg[1] = 1;
	make_intr(1, sc);
}

/*
 * nicopen() New open on device.
 *
 * We forbid all but first open
 */
static	int
nicopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct nic_softc *sc;
	u_char          unit;
	dpr_type       *dpr;
	int             x;

	unit = minor(dev);

	/* minor number out of limits ? */
	if (unit >= NNIC)
		return (ENXIO);
	sc = &nic_sc[unit];

	sc->sc_flags |= OPEN;
	dpr = sc->sc_dpr;
	dpr->card_number = sc->sc_unit;
	dpr->int_flg_pc = 0xff;
	if (dpr->msg_flg[0])
	{
		x = splhigh();
		s_intr(sc);
		splx(x);
	}
	return (0);
}

/*
 * nicclose() Close device
 */
static	int
nicclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct nic_softc *sc = &nic_sc[minor(dev)];

	sc->sc_flags = 0;
	return (0);
}

static	int
nicioctl(dev_t dev, int cmd, caddr_t data, int flags, struct proc *p)
{
	int             error;
	u_char          unit;
	int             i, x;
	struct nic_softc *sc = &nic_sc[minor(dev)];
	dpr_type       *dpr = sc->sc_dpr;
	mbx_type       *mbx;

	dpr->int_flg_pc = 0xff;

	error = 0;
	switch (cmd)
	{
	case NICCY_DEBUG:
		data[0]= 0x38;
		bcopy((char *)dpr, data+1, sizeof(dpr_type));
		break;
	case NICCY_LOAD:
		{
			struct head    *head = (struct head *) data;
			u_char         *b = (u_char *) dpr;
			int len, l, off;

			x = splhigh();
			while (dpr->msg_flg[1])
			{
				error = tsleep((caddr_t) sc, PZERO | PCATCH, "nic1head", 1);
				if (error != EWOULDBLOCK)
				{
					splx(x);
					return (error);
				}
			}
			mbx = &dpr->dpr_mbx[1];
			bzero(mbx, 16);
			mbx->type = MD_DNL_MOD_REQ | ((u_short) head->typ << 8);
			mbx->data_len = 12;
			bcopy(head->nam, mbx->data, 8);
			*(u_long *) (mbx->data + 8) = head->len;

			sc->sc_flags = LOAD_HEAD;
			sc->sc_stat = -1;
			dpr->msg_flg[1] = 1;
			make_intr(1, sc);
			while (sc->sc_flags == LOAD_HEAD)
			{
				error = tsleep((caddr_t) sc, PZERO | PCATCH, "nic2head", 1);
				if (error != EWOULDBLOCK)
					break;
			}

			len= head->d_len;
			off= 0;
			b += dpr->buf_ptr[1];

			while(len > 0)
			{
				while (dpr->msg_flg[1])
				{
					error = tsleep((caddr_t) sc, PZERO | PCATCH, "nic1load", 1);
					if (error != EWOULDBLOCK)
					{
						splx(x);
						return (error);
					}
				}
				bzero(mbx, 16);
				mbx->type = MD_DNL_MOD_DATA | ((u_short) head->typ << 8);
				l= min(len,1024);
				len-= l;
				mbx->buf_valid = 1;
				mbx->more_data = len > 0;
				mbx->data_len = l;
				bcopy(head->nam, mbx->data, 8);

				if(error= copyin(head->data+off, b, l))
				{
					splx(x);
					return(error);
				}
				off+= l;
				sc->sc_flags = LOAD_DATA;
				sc->sc_stat = -1;
				dpr->msg_flg[1] = 3;
				make_intr(1, sc);
			}

			while ((sc->sc_flags == LOAD_DATA) || (dpr->card_state & 0x20))
			{
				error = tsleep((caddr_t) sc, PZERO | PCATCH, "nic2load", 1);
				if (error != EWOULDBLOCK)
					break;
			}
			if (sc->sc_flags)
				sc->sc_flags = OPEN;
			head->status = sc->sc_stat;
			splx(x);
			return (0);
		}
	case NICCY_SET_CLOCK:
		x = splhigh();
		if (dpr->msg_flg[1])
		{
			splx(x);
			return (EBUSY);
		}
		mbx = &dpr->dpr_mbx[1];
		bzero(mbx, 16);
		mbx->type = MD_SET_CLOCK_REQ;
		mbx->data_len = 14;
		bcopy(data, mbx->data, 14);

		dpr->msg_flg[1] = 1;
		if (dpr->int_flg_nic & 2)
			make_intr(1, sc);
		splx(x);
		return (0);
	case NICCY_SPY:
		x = splhigh();
		if (dpr->msg_flg[1])
		{
			splx(x);
			return (EBUSY);
		}
		mbx = &dpr->dpr_mbx[1];
		bzero(mbx, 16);
		mbx->type = MD_MANUFACT_REQ | (18<<8);
		mbx->data_len = 1;
		mbx->add_info = MK_APPL(ispy_applnr);
/* There are ilegal states. So I use them to toggle */
		if((data[0] == 0) && (old_spy == 0)) data[0]= 255;
		else if(data[0] && old_spy ) data[0]= 0;
		old_spy= mbx->data[0]= data[0];

		dpr->msg_flg[1] = 1;
		if (dpr->int_flg_nic & 2)
			make_intr(1, sc);
		splx(x);
		return (0);
	case NICCY_RESET:
		x = splhigh();

		reset_card(sc);

		while (dpr->card_state & 1)	/* self test running */
		{
			error = tsleep((caddr_t) sc, PZERO | PCATCH, "nic: reset", 10);
			if (error != EWOULDBLOCK)
				break;
		}
		dpr->card_number = sc->sc_unit;
		dpr->int_flg_pc = 0xff;
		if (dpr->msg_flg[0])
			s_intr(sc);
		splx(x);
		return (0);

	default:
		error = ENODEV;
	}
	return (error);
}

static void
b_intr(int mb, int c, struct nic_softc * sc)
{
	dpr_type       *dpr = sc->sc_dpr;
	mbx_type       *mbx = &dpr->dpr_mbx[mb];
	chan_t         *chan = &sc->sc_chan[c];
	u_short         ap, n, err = 0;
	u_short         pl = mbx->add_info;
	isdn_ctrl_t    *ctrl = &isdn_ctrl[chan->ctrl];

	if(((unsigned)(mbx->type >> 8) > 3) || ((pl & 0xff00) == 0xff00))
		panic("3008 conflict with 16 bit card\nReconfig your system");

	if (dpr->msg_flg[mb+1])
		return;		/* can happen. Should make no problems */

	if (ISBUSY(ap = ctrl->appl))
		switch (mbx->type & 0x1f)
		{
		case 0:	/* SELECT PROT CONF */
			err = *(u_short *) mbx->data;
			if (err)
			{
				discon_req(2, sc, pl, 0, err);
				break;
			}

			switch ((mbx->type >> 8) & 3)
			{
			case 2:/* SELECT B2 PROTOCOL */
				sel_b3_prot_req(sc, mb, pl, &isdn_appl[ap].ncpd);
				break;

			case 3:/* SELECT B3 PROTOCOL */
				if (IS_DIAL(pl))
					con_b3_req(sc, mb, pl);
				else
					listen_b3_req(sc, mb, pl);
				break;
			}
			break;

		case 1:	/* LISTEN B3 CONF */
			err = *(u_short *) mbx->data;
			if (err)
			{
				discon_req(4, sc, pl, 0, err);
				break;
			}
			con_resp(sc, pl, 0);
			break;

		case 2:	/* CONNECT B3 CONF */
			err = *(u_short *) (mbx->data + 2);
			n = *(u_short *) mbx->data;

			if (err)
			{
				discon_req(5, sc, pl, 0, err);
				break;
			}
			chan->ncci = n;
			chan->state = CONNECT;
			break;

		case 3:	/* CONNECT B3 IND */
			n = *(u_short *) mbx->data;
			chan->ncci = n;
			chan->state = CONNECT;
			con_b3_resp(sc, mb, n, 0);
			break;

		case 4:	/* CONNECT B3 ACTIVE IND */
			if (chan->state < IDLE)
			{
				chan->state = IDLE;
				ctrl->o_len = 0;
				/*
				 * XXX the chan->ctrl arg is very bogus.
				 * Don't just use a cast to "fix" it.
				 */
				timeout(isdn_start_out, chan->ctrl, hz / 5);
				break;
			}
			break;

		case 5:	/* DISCONNECT B3 CONF */
			chan->state = ISDISCON;
			err = *(u_short *) (mbx->data + 2);
			if (err)
			{
				discon_req(6, sc, chan->plci, 0, err);
				break;
			}
			break;
		case 6:	/* DISCONNECT B3 IND */
			chan->state = ISDISCON;
			err = *(u_short *) (mbx->data + 2);
			discon_req(7, sc, chan->plci, 0, err);
			break;

		case 8:	/* DATA B3 CONF */
			err = *(u_short *) (mbx->data + 2);
			if (err)
			{
				ctrl->send_err++;
				isdn_appl[ap].send_err++;
			}
			ctrl->o_len = 0;
			chan->state= IDLE;
			isdn_start_out(chan->ctrl);
			break;

		case 9:	/* DATA B3 IND */
			{
				u_char         *b = (u_char *) dpr;
				u_char	mno;

				b += dpr->buf_ptr[mb];
				if (mbx->more_data)
				{
					chan->morenr= mbx->data[4];
					if(chan->i_len)
					{
						chan->i_len= 0;
						break;
					}
					bcopy(b, &chan->i_buf[chan->i_len], mbx->data_len);
					chan->i_len = mbx->data_len;
					break;
				} /* mbx->more_data == 0 */
				if (chan->i_len)
				{
					int l;
					if(chan->morenr != mbx->data[4])
						break;

					if ((l = chan->i_len + mbx->data_len) <= 2048)
					{
						bcopy(b, &chan->i_buf[chan->i_len], mbx->data_len);
						if(isdn_input(ap, l, chan->i_buf, ctrl->islisten))
							ctrl->lastact = time.tv_sec;
					}
					chan->i_len = 0;
					break;
				} /* chan->i_len == 0 && mbx->more_data == 0 */
				if(isdn_input(ap, mbx->data_len, b, ctrl->islisten))
					ctrl->lastact = time.tv_sec;
				break;
			}
			break;

		default:
			badstate(mbx,1,mb,dpr);
	}
/*
	else badstate(mbx,2,mb,dpr);
*/

	reset_req(sc, mb,1);
}

static void
d_intr(struct nic_softc * sc)
{
	dpr_type       *dpr = sc->sc_dpr;
	mbx_type       *mbx = &dpr->dpr_mbx[2];
	chan_t         *chan;
	u_short         ap, c, pl, err = 0;
	isdn_ctrl_t    *ctrl;
	isdn_appl_t    *appl;

	if (dpr->msg_flg[3])
		return;		/* should not happen. might make problems */
	/* but there should be another intr., so what? */

	pl = mbx->add_info;
	if ((c = CHAN(pl)) < 2)
	{
		chan = &sc->sc_chan[c];
		ctrl = &isdn_ctrl[chan->ctrl];
	} else
	{
		c = 0xffff;
		chan = NULL;
		ctrl = NULL;
	}

	ap= APPL(pl);
	if(ctrl && (ctrl->appl & 0xC0) == 0)
		appl= &isdn_appl[ctrl->appl];
	else if(ap < 0x30)
		appl = &isdn_appl[ap];
	else if(ap < 0x40)
		appl = NULL;
	else
	{
		reset_req(sc, 2,2);
		return;
	}

	switch (mbx->type & 0x1f)
	{
	case 0:		/* CONNECT CONF */
		err = *(u_short *) mbx->data;
		if(err || (appl == NULL) || (chan == NULL) || (ctrl == NULL))
		{
			if(chan) reset_plci(1, chan, pl);
			if(appl) appl->state= 0;
			break;
		}

		if (ISBUSY(ctrl->appl))
		{
			discon_req(8, sc, pl, 0, 0);
			break;
		}
		chan->plci = pl;
		chan->msg_nr= 0;
		chan->ncci = -1;
		ctrl->lastact = time.tv_sec;
		ctrl->appl = ap;
		appl->ctrl = chan->ctrl;
		ctrl->islisten= 0;
		chan->state = DIAL;
		appl->state = 3;
		break;

	case 1:		/* CONNECT IND */
		if (ISBUSY(ctrl->appl))
		{
			discon_req(9, sc, pl, 0, 0);
			break;
		}
		chan->plci = pl;
		chan->msg_nr= 0;
		chan->ncci = -1;
		ctrl->lastact = time.tv_sec;
		ctrl->appl = 0x7f;
		ctrl->islisten= 1;
		chan->state = CALLED;
		mbx->data[mbx->data[3] + 4] = 0;
		isdn_accept_con_ind(ap, chan->ctrl, mbx->data[0], mbx->data[1]
		       ,mbx->data[2], mbx->data[3], (char *) &mbx->data[4]);
		break;

	case 2:		/* CONNECT ACTIVE IND */
		con_act_resp(sc, pl);
		if (IS_LISTEN(pl))
		{
			isdn_conn_ind(ctrl->appl,chan->ctrl,0);
			break;
		}
		isdn_conn_ind(APPL(pl),chan->ctrl,1);
		chan->state = CONNECT;
		ctrl->appl = ap;
		appl->ctrl = chan->ctrl;
		break;

	case 3:		/* DISCONNECT CONF */
		reset_plci(2, chan, pl);
		break;

	case 4:		/* DISCONNECT IND */
		discon_resp(sc, reset_plci(3, chan, pl));
		break;

	case 7:		/* LISTEN CONF */
		isdn_state = *(u_short *) mbx->data;
		break;

	case 10:		/* INFO IND */
		isdn_info(ap,*(u_short *)mbx->data, mbx->data[2], mbx->data+3);
		inf_resp(sc, pl);
		break;

	default:
		badstate(mbx,3,2,dpr);
	}
	reset_req(sc, 2,2);
}

static void
s_intr(struct nic_softc * sc)
{
	dpr_type       *dpr = sc->sc_dpr;
	mbx_type       *mbx = &dpr->dpr_mbx[0];
	mbx_type       *smbx = &dpr->dpr_mbx[1];

	if (dpr->msg_flg[1])
		return;		/* should not happen. might make problems */
	/* but there should be another intr., so what? */

	bzero(smbx, 16);

	switch (mbx->type & 0x1f)
	{
	case 0:		/* INIT CONF */
		break;
	case 1:		/* INIT IND */
		smbx->type = mbx->type + 0x20;
		dpr->msg_flg[1] = 1;
		make_intr(1, sc);
		break;
	case 4:		/* DNL MOD CONF */
		sc->sc_stat = mbx->data[0];
		if (sc->sc_flags)
			sc->sc_flags = OPEN;
		break;
	case 6:		/* DNL MOD IND */
		smbx->type = mbx->type + 0x20;
		smbx->data_len = 1;
		smbx->data[0] = mbx->data[1];
		sc->sc_stat = mbx->data[0];
		if (sc->sc_flags)
			sc->sc_flags = OPEN;
		dpr->msg_flg[1] = 1;
		make_intr(1, sc);
		break;
	case 0x0e:	/* SET CLOCK CONF */
		dpr->watchdog_cnt = 0xFF;
		dpr->int_flg_pc   = 0xFF;
		dpr->api_active   = 1;
		break;
	case 0x15:		/* POLL IND */
		dpr->watchdog_cnt = 0xFF;
		dpr->int_flg_pc   = 0xFF;
		dpr->api_active   = 1;
		smbx->type = mbx->type + 0x20;
		dpr->msg_flg[1] = 1;
		make_intr(1, sc);
		break;
	case 0x1e:		/* MANUFACT CONF */
		if(((mbx->type >> 8) == 18 ) && (*mbx->data == 0))	/* LISTEN */
			break;
		badstate(mbx,4,0,dpr);
		break;
	case 0x1f:		/* MANUFACT IND */
		if((mbx->type >> 8) == 19 )	/* DATA */
		{
			u_char *b = (u_char *) dpr;
			b += dpr->buf_ptr[0];
			isdn_input(ispy_applnr, mbx->data_len, b, 0);
			smbx->type = mbx->type + 0x20;
			dpr->msg_flg[1] = 1;
			make_intr(1, sc);
			break;
		}
	default:
		badstate(mbx,5,0,dpr);
	}
	reset_req(sc, 0, 3);
}

static void
bs_intr(int mb, int c, struct nic_softc * sc)
{
	chan_t         *chan = &sc->sc_chan[c];
	isdn_ctrl_t    *ctrl = &isdn_ctrl[chan->ctrl];

	if (chan->state == ACTIVE)
	{
		if (chan->more)
		{
			dpr_type       *dpr = sc->sc_dpr;
			mbx_type       *mbx = &dpr->dpr_mbx[mb];
			u_char         *b = (u_char *) dpr;

			bzero(mbx, 20);
			mbx->type = BD_DATA_B3_REQ;
			if (mb == 7)
				mbx->type |= 0x40;
			*(u_short *) mbx->data = chan->ncci;
			mbx->data[4] = chan->msg_nr;
			b += dpr->buf_ptr[mb];
			mbx->data_len = chan->more;
			bcopy(chan->more_b, b, chan->more);

			chan->more = 0;
			ctrl->o_len = -1;

			dpr->msg_flg[mb] = 3;
			make_intr(mb, sc);

			ctrl->lastact = time.tv_sec;
			return;
		}
		bsintr &= ~(1 << c);
	}
}

void
nicintr(int unit)
{
	register struct nic_softc *sc = &nic_sc[unit];
	dpr_type       *dpr = sc->sc_dpr;

	if (dpr->msg_flg[2])
		d_intr(sc);
	if (dpr->msg_flg[0])
		s_intr(sc);
	if (dpr->msg_flg[6])
		b_intr(6, 1, sc);
	if (dpr->msg_flg[4])
		b_intr(4, 0, sc);
	if (bsintr)
	{
		if (dpr->msg_flg[7] == 0)
			bs_intr(7, 1, sc);
		if (dpr->msg_flg[5] == 0)
			bs_intr(5, 0, sc);
	}
}


static nic_devsw_installed = 0;

static void 	nic_drvinit(void *unused)
{
	dev_t dev;

	if( ! nic_devsw_installed ) {
		dev = makedev(CDEV_MAJOR ,0);
		cdevsw_add(&dev,&nic_cdevsw, NULL);
		nic_devsw_installed = 1;
    	}
}

SYSINIT(nicdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,nic_drvinit,NULL)

#endif				/* NNIC > 0 */
