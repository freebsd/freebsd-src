static char     rcsid[] = "@(#)$Id: nic5000.c,v 1.1 1995/02/14 15:00:37 jkh Exp $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.1 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: nic5000.c,v $
 * Revision 1.1  1995/02/14  15:00:37  jkh
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
 *
 * Copyright (c) 1994 Dietmar Friede (dietmar@friede.de) All rights reserved.
 * FSF/FSAG GNU Copyright applies
 *
 * A low level driver for the NICCY-5000 ISDN/SCSI device
 *
 */

#include "snic.h"
#if NSNIC > 0

#define SPLSNIC splbio
#define ESUCCESS 0
#define SNIC_RETRIES 8
#include "sys/types.h"
#include "sys/param.h"
#include "sys/ioctl.h"
#include "sys/malloc.h"
#include "sys/kernel.h"

#include "scsi/scsi_all.h"
#include "scsi/scsiconf.h"
#include "gnu/isdn/isdn_ioctl.h"
#include "gnu/i386/isa/niccyreg.h"
#include "gnu/scsi/scsi_nic.h"
/* #define NETBSD */

#undef	SCSI_NOMASK
#define OPEN		1
#define LOAD_HEAD	2
#define LOAD_DATA	4
#define	LOAD_ENTITY	8
#define IS_DIAL(p)	(((p)&0x20)==0)
#define IS_LISTEN(p)	((p)&0x20)
#define	CHAN(pl)	(((pl)&7)-1)
#define	C_CHAN(x)	((x)&1)
#define APPL(pl)	((((pl)>>6)&0x7f)-1)
#define CARD(pl)	(((pl)>>13)&7)
#define MK_APPL(pl)	(((pl)+1)<<6)
#define min(a,b)	((a)<(b)?(a):(b))

#define	SNICOUTSTANDING	2

extern int hz;

struct	snic_data
{
	struct	scsi_switch *sc_sw;	/* address of scsi low level switch */
	int	ctrl;			/* so they know which one we want */
	int	targ;			/* our scsi target ID */
	int	lu;			/* out scsi lu */
	int	cmdscount;		/* cmds allowed outstanding by board*/
	int		xfer_block_wait;
	struct	scsi_xfer	*free_xfer;
	struct	scsi_xfer	scsi_xfer[SNICOUTSTANDING]; /* XXX */
};

struct	snic_driver
{
	int	size;
	struct	snic_data	**snic_data;
}*snic_driver;

static	int	next_snic_unit = 0;
static unsigned dnlnum = 0;

static u_char ack_msg= 0xff;
static u_char snic_nxt_b;

typedef enum
{
	DISCON, ISDISCON, DIAL, CALLED, CONNECT, IDLE, ACTIVE, WAITING, WAIT_ACK
}               io_state;

typedef struct
{
	char            ctrl;
	u_char          msg_nr;
	short           plci;
	short           ncci;
	short           state;
	Buffer          o_buf;
}               chan_t;

struct snic_softc
{
	short           sc_stat;
	u_char          sc_flags;
	u_char          sc_unit;
	u_char          sc_ctrl;
	u_char		sc_type;
	u_short		sc_istat;
	struct scsi_msg sc_icmd;
	Buffer		sc_imsg;
	Header		sc_imsg0;
	u_short		sc_ostat;
	struct scsi_msg sc_ocmd;
	Buffer		sc_omsg;
	chan_t          sc_chan[2];
	u_char		sc_state_ind[8];
	u_char		sc_gotack;
}               snic_sc[NSNIC];

extern isdn_appl_t isdn_appl[];
extern isdn_ctrl_t isdn_ctrl[];
extern u_short isdn_state;
extern int     ispy_applnr;
extern int Isdn_Appl, Isdn_Ctrl, Isdn_Typ;
extern void isdn_start_out();

static old_spy= 0;
static void snic_interupt();
static int snic_get_msg();
static void snic_start();

int             snic_connect(), snic_listen(), snic_disconnect(), snic_accept();
int             snic_output();

#ifdef NETBSD
int	snicattach(int ctrl, struct scsi_switch *scsi_switch, int physid, int *sunit)
{
	int targ, lu;
#else	/* FreeBSD */
int	snicattach(int ctrl, int targ, int lu, struct scsi_switch *scsi_switch)
{
#endif
	int		unit,i;
	struct snic_data	*snic, **snicrealloc;
	struct snic_softc *sc;
	int             cn;
	isdn_ctrl_t    *ctrl0, *ctrl1;

#ifdef NETBSD
	targ = physid >> 3;
	lu = physid & 7;
#endif

	if(next_snic_unit >= NSNIC)
		return(0);

	unit = next_snic_unit;
	if (next_snic_unit == 0)
	{
		snic_driver =
			malloc(sizeof(struct snic_driver),M_DEVBUF,M_NOWAIT);
		if(!snic_driver)
		{
			printf("snic%d: malloc failed\n",unit);
			return(0);
		}
		bzero(snic_driver,sizeof(snic_driver));
		snic_driver->size = 0;
	}
	next_snic_unit++;

	if(unit >= snic_driver->size)
	{
		snicrealloc =
			malloc(sizeof(snic_driver->snic_data) * next_snic_unit,
				M_DEVBUF,M_NOWAIT);
		if(!snicrealloc)
		{
			printf("snic%d: malloc failed\n",unit);
			return(0);
		}
		/* Make sure we have something to copy before we copy it */
		bzero(snicrealloc,sizeof(snic_driver->snic_data) * next_snic_unit);
		if(snic_driver->size)
		{
			bcopy(snic_driver->snic_data,snicrealloc,
				sizeof(snic_driver->snic_data) * snic_driver->size);
			free(snic_driver->snic_data,M_DEVBUF);
		}
		snic_driver->snic_data = snicrealloc;
		snic_driver->snic_data[unit] = NULL;
		snic_driver->size++;
	}

	if(snic_driver->snic_data[unit])
	{
		return(0);
	}

	snic = snic_driver->snic_data[unit] =
		malloc(sizeof(struct snic_data),M_DEVBUF,M_NOWAIT);
	if(!snic)
	{
		printf("snic%d: malloc failed\n",unit);
		return(0);
	}
#ifdef NETBSD
	*sunit= unit;
#endif
	bzero(snic,sizeof(struct snic_data));

	snic->sc_sw	=	scsi_switch;
	snic->ctrl	=	ctrl;
	snic->targ	=	targ;
	snic->lu		=	lu;
	snic->cmdscount =	SNICOUTSTANDING; /* XXX (ask the board) */

	i = snic->cmdscount;
	while(i-- )
	{
		snic->scsi_xfer[i].next = snic->free_xfer;
		snic->free_xfer = &snic->scsi_xfer[i];
	}

	sc = &snic_sc[unit];
	sc->sc_ctrl = -1;
	sc->sc_gotack= 1;
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
	ctrl0->o_buf = &sc->sc_chan[0].o_buf.Data[5];
	ctrl1->o_buf = &sc->sc_chan[1].o_buf.Data[5];

	ctrl0->listen = ctrl1->listen = snic_listen;
	ctrl0->disconnect = ctrl1->disconnect = snic_disconnect;
	ctrl0->accept = ctrl1->accept = snic_accept;
	ctrl0->connect = ctrl1->connect = snic_connect;
	ctrl0->output = ctrl1->output = snic_output;
	ctrl0->unit = ctrl1->unit = unit;
	ctrl0->appl = ctrl1->appl = -1;
	ctrl0->o_len = ctrl1->o_len = -1;
	sc->sc_flags= LOAD_ENTITY;
	return(1);
}

static
struct scsi_xfer *snic_get_xs(int unit)
{
	struct scsi_xfer *xs;
	struct snic_data *snic;
	int	s;

	snic = snic_driver->snic_data[unit];
	if (xs = snic->free_xfer)
	{
		snic->free_xfer = xs->next;
		xs->flags = 0;
	}
	return(xs);
}

static void
snic_free_xs(int unit, struct scsi_xfer *xs)
{
	struct snic_data *snic;
	
	snic = snic_driver->snic_data[unit];
	xs->next = snic->free_xfer;
	snic->free_xfer = xs;
}

static void
snic_timout(int unit)
{
	struct snic_softc * sc= &snic_sc[unit&0xff];

	if(sc->sc_istat&0x100)
	{
		snic_interupt(unit);
		return;
	}
	if(sc->sc_istat & 2)
		sc->sc_istat= sc->sc_ostat= 0;
	else if((sc->sc_istat & 0x200) == 0 ) return;
	if(sc->sc_ostat & 0xff)
	{
		sc->sc_istat|= 0x200;
		timeout(snic_timout,unit,2);
		return;
	}
	if(sc->sc_gotack) snic_start(unit);
	snic_get_msg(unit);
}

static int
isdn_small_interupt(int unit, struct scsi_xfer *xs)
{
	struct snic_data *snic = snic_driver->snic_data[unit];
	struct snic_softc * sc= &snic_sc[unit];
	Header *msg = &sc->sc_imsg0;
	int c;

	switch (msg->Type)
	{
	case 0:
		if(sc->sc_istat&0x200)
			break;
		sc->sc_istat|= 0x200;
		timeout(snic_timout,unit,2);
		break;
	case 0xff:
		sc->sc_gotack= 1;
		break;
	case 0xfe:
printf("f");
		sc->sc_gotack= 1;
		for(c= 0; c < 2; c++)
		{
			chan_t *chan = &sc->sc_chan[c];
			if(chan->state == WAIT_ACK)
			{
				chan->state = WAITING;
				sc->sc_ostat |= c?0x800:0x400;
			}
		}
		break;
	case 0xfd:
printf("fd");
		break;
	default:
		return(0);
	}
	sc->sc_istat&= ~0xff;
	sc->sc_imsg0.Type= 0;
	return(1);
}

static void
snic_get_done(int unit, struct scsi_xfer *xs)
{
	struct snic_data *snic = snic_driver->snic_data[unit];
	struct snic_softc * sc= &snic_sc[unit];
	Header *msg = &sc->sc_imsg0;
	int len, error;

	error= xs->error;

	switch(error)
	{
	case	XS_NOERROR:
		if(xs->datalen == 0)
			sc->sc_imsg.h.Type= 0;
		
		if(isdn_small_interupt(unit,xs)) break;

		if(xs->datalen < (len=(msg->DataLen + 10)))
		{
			struct scsi_msg *scsi_cmd= &sc->sc_icmd;
			/* resubmit it */

			sc->sc_imsg.h.Type= 0xba;
			scsi_cmd->len[1]= (len>>8)&0xff;
			scsi_cmd->len[2]= len&0xff;
			xs->retries= SNIC_RETRIES;
			xs->error = XS_NOERROR;
			xs->flags &= ~ITSDONE;
			xs->data        =       (char *) &sc->sc_imsg;
			xs->datalen	=	len;
			xs->resid	=	len;

			if ((*(snic->sc_sw->scsi_cmd))(xs) == SUCCESSFULLY_QUEUED)
			{
				return;
			}
			error= xs->error | 0x1000;
			break;
		}
		if(xs->datalen <= 10)
		{
			sc->sc_istat|= 0x400;
			sc->sc_imsg.h = sc->sc_imsg0;
		}
		sc->sc_imsg0.Type= 0;
		break;

	case	XS_TIMEOUT:
	case	XS_BUSY:
	case	XS_DRIVER_STUFFUP:
		break;
	default:
		printf("snic%d: unknown error %x\n",unit,xs->error);
	}	

	if(error)
	{
		sc->sc_imsg.h.Type= sc->sc_imsg0.Type= 0;
		sc->sc_istat&= 0x200;
		if((sc->sc_istat&0x200) == 0)
		{
			sc->sc_istat= 0x200;
			timeout(snic_timout,unit,2);
		}
	}

	snic_free_xs(unit,xs);
	if(sc->sc_istat&0x4ff == 0x400 )
		sc->sc_istat|= 1;
	if(sc->sc_istat&0xff)
	{
		snic_interupt(unit);
		return;
	}
	if(sc->sc_gotack) snic_start(unit);
	if(sc->sc_istat & 0x200)
		return;
	sc->sc_istat|= 0x200;
	timeout(snic_timout,unit,2);
}

static int
snic_get_msg(unit)
int	unit;
{
	struct snic_data *snic = snic_driver->snic_data[unit];
	struct snic_softc * sc= &snic_sc[unit];
	struct scsi_msg *scsi_cmd= &sc->sc_icmd;
	struct	scsi_xfer *xs;
	Header		*data= &sc->sc_imsg0;
	int	retval;

	if(sc->sc_istat&0xff)
		return(-1);
	sc->sc_istat |= 1;

	data->Type= 0xbb;
	sc->sc_istat &= ~0x200;

	bzero(scsi_cmd, sizeof(struct scsi_msg));
	bzero(data,10);

	scsi_cmd->op_code = GET_MSG_COMMAND;
	scsi_cmd->len[2]= 10;

	xs = snic_get_xs(unit);
	if(!xs)
	{
		sc->sc_istat&= ~0xff;
		data->Type= 0;
		return(EBUSY);
	}

	xs->flags |= (INUSE | SCSI_DATA_IN | SCSI_NOSLEEP);
	xs->adapter	=	snic->ctrl;
	xs->targ	=	snic->targ;
	xs->lu		=	snic->lu;
	xs->retries	=	SNIC_RETRIES;
	xs->timeout	=	2000;
	xs->cmd		=	(struct scsi_generic *) scsi_cmd;
	xs->cmdlen	=	sizeof(struct scsi_msg);
	xs->data	=	(char *) data;
	xs->datalen	=	10;
	xs->resid	=	10;
	xs->when_done	=	snic_get_done;
	xs->done_arg	=	unit;
	xs->done_arg2	=	(int)xs;
	xs->bp		=	NULL;
	xs->error	=	XS_NOERROR;

	if(retval = (*(snic->sc_sw->scsi_cmd))(xs))
	{
		sc->sc_istat= ~0xff;
		data->Type= 0;
		snic_free_xs(unit,xs);
	}
	return (retval);
}

static void
snic_put_done(int unit, struct scsi_xfer *xs)
{
	int	retval;
	struct snic_data *snic = snic_driver->snic_data[unit];
	struct snic_softc * sc= &snic_sc[unit];
	Header *b= (Header *) xs->data;
	int c;

	sc->sc_ostat&= ~0xff;
	if(xs->error != XS_NOERROR)
	{
		snic_free_xs(unit,xs);
		switch(b->Type)
		{
		case 0:
			return;
		case 0xff:
			sc->sc_ostat|= 0x100;
			return;
		case BD_DATA_B3_REQ | 0x40:
		case BD_DATA_B3_REQ:
			sc->sc_ostat|= 0x400;
			return;
		default:
			sc->sc_ostat|= 0x200;
			return;
		}
	}

	snic_free_xs(unit,xs);

	c= 0;
	switch(b->Type)
	{
	case 0xff: break;
	case BD_DATA_B3_REQ | 0x40:
		c= 1;
	case BD_DATA_B3_REQ:
		sc->sc_chan[c].state = WAIT_ACK;
		break;
	default:
		b->Type= 0;
	}

	if(sc->sc_istat&0x100)
	{
		snic_interupt(unit);
		return;
	}

	if(sc->sc_ostat&0x100)
	{
		sc->sc_ostat&= ~0x100;
		if(snic_put_msg(unit,&ack_msg,1,0))
			sc->sc_ostat|= 0x100;
		else return;
	}

	if(sc->sc_gotack) snic_start(unit);
	if(sc->sc_istat&0x200)
		return;
	sc->sc_istat|= 0x200;
	timeout(snic_timout,unit,2);
}

static void
snic_start(int unit)
{
	int	retval;
	struct snic_softc * sc= &snic_sc[unit];
	Header *b;
	int c;

	if(sc->sc_ostat&0x200)
	{
		b= &sc->sc_omsg.h;
		sc->sc_ostat&= ~0x200;
		if(snic_put_msg(unit,b, b->DataLen+10,2))
			sc->sc_ostat|= 0x200;
		else return;
	}


	for(c= 0; c<2; c++)
	{
		int cc= (snic_nxt_b++)&1;
		u_short m= 0x400 << cc;

		if(sc->sc_ostat&m)
		{
			chan_t         *chan= &sc->sc_chan[cc];
			b= &chan->o_buf.h;
			sc->sc_ostat&= ~m;
			if(chan->state == WAITING)
			{
				chan->state= ACTIVE;
				if(snic_put_msg(unit,b, b->DataLen+10,4))
				{
					chan->state= WAITING;
					sc->sc_ostat|= m;
				}
				else return;
			}
		}
	}
}

int
snic_put_msg(int unit, Header *data, unsigned len, int w)
{
	struct snic_softc *sc = &snic_sc[unit];
	struct scsi_msg *scsi_cmd = &sc->sc_ocmd;
	int	retval;
	struct	scsi_xfer *xs;
	struct snic_data *snic = snic_driver->snic_data[unit];

	if(data->Type==0)
		return(0);

	if(sc->sc_ostat&0xff)
		return(EBUSY);

	sc->sc_ostat |= 1;
	if((data->Type == 0xa8) || (data->Type == 0xe8))
	{
		if(sc->sc_gotack==0)
		{
			sc->sc_ostat &= ~0xff;
			return(EBUSY);
		}
	}
	if(data->Type != 0xff)
		sc->sc_gotack= 0;
	bzero(scsi_cmd, sizeof(struct scsi_msg));

	scsi_cmd->op_code = PUT_MSG_COMMAND;
	if(len > 2063)
	{
		printf("snic%d: unsupported length %d\n",unit,len);
		sc->sc_ostat &= ~0xff;
		return(ENODEV);
	}
	scsi_cmd->len[1]= (len >> 8) & 0xff;
	scsi_cmd->len[2]= len & 0xff;

	xs = snic_get_xs(unit);
	if(!xs)
	{
		printf("snic pm%d: busy %d\n", unit, w);
		sc->sc_ostat &= ~0xff;
		return(EBUSY);
	}
	xs->flags |= (INUSE | SCSI_DATA_OUT | SCSI_NOSLEEP);
	xs->adapter	=	snic->ctrl;
	xs->targ	=	snic->targ;
	xs->lu		=	snic->lu;
	xs->retries	=	SNIC_RETRIES;
	xs->timeout	=	2000;
	xs->cmd		=	(struct scsi_generic *) scsi_cmd;
	xs->cmdlen	=	sizeof(struct scsi_msg);
	xs->data	=	(char *)data;
	xs->datalen	=	len;
	xs->resid	=	len;
	xs->when_done	=	snic_put_done;
	xs->done_arg	=	unit;
	xs->done_arg2	=	(int)xs;
	xs->bp		=	NULL;
	xs->error	=	XS_NOERROR;

	if(retval = (*(snic->sc_sw->scsi_cmd))(xs))
	{
		sc->sc_ostat &= ~0xff;
		snic_free_xs(unit,xs);
		return(EBUSY);
	}

	return(0);
}

int
snicopen(dev_t dev, int flag)
{
	struct snic_softc *sc;
	u_char          unit;
	int             x;
	unsigned	error;
	u_char	b= 0xff;

	unit = minor(dev);
	/* minor number out of limits ? */
	if (unit >= next_snic_unit)
		return (ENXIO);
	sc = &snic_sc[unit];

	x= splhigh();
	/* Card busy ? */
	if (sc->sc_flags & 7)
	{
		splx(x);
		return (EBUSY);
	}
	sc->sc_flags |= OPEN;

	if(sc->sc_flags & LOAD_ENTITY)
	{
		snic_get_msg(unit);
/*
		if(snic_put_msg(unit,(Header *) &ack_msg,1,5))
			sc->sc_ostat|= 0x100;
*/
	}

	splx(x);
	return (0);
}

int
snicclose(dev_t dev, int flag)
{
	struct snic_softc *sc = &snic_sc[minor(dev)];

	sc->sc_flags &= ~7;
	return (0);
}

int
snicioctl(dev_t dev, int cmd, caddr_t data, int flag)
{
	int             error;
	u_char          unit= minor(dev);
	int             i, x;
	struct snic_softc *sc = &snic_sc[minor(dev)];
	Buffer *b= &sc->sc_omsg;

	error = 0;
	x= splhigh();
	while(sc->sc_ostat || (sc->sc_gotack==0))
	{
		error = tsleep((caddr_t) sc, PZERO | PCATCH, "ioctl", 2);
		if (error != EWOULDBLOCK)
		{
			splx(x);
			return(error);
		}
	}

	switch (cmd)
	{
	case NICCY_DEBUG:
		data[0]= 0x50;
		bcopy(sc->sc_state_ind,data+1,8);
		break;
	case NICCY_LOAD:
		{
			struct head    *head = (struct head *) data;
			int len, l, off;

			bzero(b, 22);
			b->h.Type = MD_DNL_MOD_REQ;
			sc->sc_type = head->typ;
			b->h.SubType = head->typ;
			b->h.DataLen = 12;
			bcopy(head->nam, b->Data, 8);
			bcopy(&head->len, &b->Data[8], 4);

			sc->sc_flags |= LOAD_HEAD;
			sc->sc_stat = -1;
			while((error= snic_put_msg(unit,(Header *) b,22,6)) == EBUSY)
			{
				error = tsleep((caddr_t) sc, PZERO | PCATCH, "nic1", 1);
				if (error != EWOULDBLOCK)
					break;
			}
			if(error == 0)
			{
				while (sc->sc_flags & LOAD_HEAD)
				{
					error = tsleep((caddr_t) sc, PZERO | PCATCH, "nic2", 1);
					if (error != EWOULDBLOCK)
						break;
					error= 0;
				}
			}
			if (sc->sc_flags & 7)
				sc->sc_flags = (sc->sc_flags & ~7 ) | OPEN;
			if(error)
			{
				head->status = sc->sc_stat;
				splx(x);
				return (error);
			}

			len= head->d_len;
			off= 0;
			while(len > 0)
			{
				while(sc->sc_ostat || (sc->sc_gotack==0))
				{
					error = tsleep((caddr_t) sc, PZERO | PCATCH, "nic7", 2);
					if (error != EWOULDBLOCK)
					{
						splx(x);
						return(error);
					}
				}
				bzero(b,10);
				b->h.Type = MD_DNL_MOD_DATA;
				sc->sc_type = head->typ;
				b->h.SubType = head->typ;
				l= min(len,512);
				len-= l;
				b->h.DataLen = l + 8;
				b->h.Number = dnlnum++;
				b->h.MoreData= len>0;
				bcopy(head->nam, b->Data, 8);
				if(error= copyin(head->data+off, b->Data+8, l))
				{
					splx(x);
					return(error);
				}
				off+= l;
				sc->sc_flags |= LOAD_DATA;
				sc->sc_stat = -1;

				while((error= snic_put_msg(unit,(Header *) b,b->h.DataLen+10,7)) == EBUSY)
				{
					error = tsleep((caddr_t) sc, PZERO | PCATCH, "nic3", 1);
					if (error != EWOULDBLOCK)
						break;
				}
			}

			if(error == 0)
			{
				while (sc->sc_flags & LOAD_DATA)
				{
					error = tsleep((caddr_t) sc, PZERO | PCATCH, "nic4", 1);
					if (error != EWOULDBLOCK)
						break;
					error= 0;
				}
			}
			if (sc->sc_flags & 7)
				sc->sc_flags = (sc->sc_flags & ~7 ) | OPEN;
			head->status = sc->sc_stat;
			splx(x);
			return (error);
		}
	case NICCY_SET_CLOCK:
		bzero(b,10);
		b->h.Type = MD_SET_CLOCK_REQ;
		b->h.DataLen = 14;
		bcopy(data, b->Data,14);
		while((error= snic_put_msg(unit,(Header *) b,24,8)) == EBUSY)
		{
			error = tsleep((caddr_t) sc, PZERO | PCATCH, "nic5", 1);
			if (error != EWOULDBLOCK)
				break;
		}
		splx(x);
		return (error);
	case NICCY_SPY:
		bzero(b,10);
		b->h.Type = MD_MANUFACT_REQ;
		b->h.SubType = 18;
		b->h.DataLen = 1;
/* There are ilegal states. So I use them to toggle */
		if((data[0] == 0) && (old_spy == 0)) data[0]= 255;
		else if(data[0] && old_spy ) data[0]= 0;
		old_spy= b->Data[0]= data[0];
		while((error= snic_put_msg(unit,(Header *) b,11,9)) == EBUSY)
		{
			error = tsleep((caddr_t) sc, PZERO | PCATCH, "nic6", 1);
			if (error != EWOULDBLOCK)
				break;
		}
		splx(x);
		return (error);
	case NICCY_RESET:
		bzero(b,10);
		b->h.Type = MD_RESET_REQ;
		while((error= snic_put_msg(unit,(Header *) b,10,9)) == EBUSY)
		{
			error = tsleep((caddr_t) sc, PZERO | PCATCH, "nic6", 1);
			if (error != EWOULDBLOCK)
				break;
		}
		sc->sc_flags|= LOAD_ENTITY;
		splx(x);
		return (error);

	default:
		error = ENODEV;
	}
	splx(x);
	return (error);
}

#define con_b3_req(unit,mb,pl)	en_q(unit,mb|BD_CONN_B3_REQ,0,pl,0,NULL)
#define con_act_resp(unit,pl)	en_q(unit,DD_CONN_ACT_RSP,0, pl,0,NULL)
#define discon_resp(sc,pl)	en_q(unit,DD_DISC_RSP,0, pl,0,NULL)
#define inf_resp(unit,pl)	en_q(unit,DD_INFO_RSP,0, pl,0,NULL)
#define listen_b3_req(unit,mb,pl) en_q(unit,mb|BD_LIST_B3_REQ,0,pl,0,NULL)
#define con_resp(unit,pl,rea)	en_q(unit,DD_CONN_RSP,0, pl, 1,(u_char *) &rea)

static int
en_q(int unit, int t, int st, int pl, int l, u_char *val)
{
	struct snic_softc * sc= &snic_sc[unit];
	Buffer *b= &sc->sc_omsg;
	int error= 0;

	if(b->h.Type)
        {
                return(EBUSY);
        }
	bzero(b,10);
if(( t >= 0x80) && CHAN(pl) && ((t & 0x40) == 0))
printf("?%x %x",t,pl);
if(t>=0x40)
printf("S%x %x",t,pl);
	
	b->h.Type = t;
	b->h.SubType = st;
	b->h.PLCI = pl;
	if(l)
	{
		b->h.DataLen= l;
		bcopy(val,b->Data,l);
	}

	if((error= snic_put_msg(unit,(Header *) b,10+l,13)) == EBUSY)
	{
		sc->sc_ostat|= 0x200;
		return(0);
	}
	return(error);
}

static int
reset_plci(int w, chan_t * chan, short p)
{
	isdn_ctrl_t    *ctrl;

	if (p == -1)
		return (-1);

	if(chan == NULL)
		return(p);

	ctrl = &isdn_ctrl[chan->ctrl];
	if(chan->plci == p)
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
		chan->o_buf.h.Type= 0;
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
state_ind(int unit, int api, int spv)
{
	u_char	buf[3];

	buf[0]= unit; buf[1]= api; buf[2]= spv;
	return(en_q(unit, MD_STATE_IND,0, 0, 3, buf));
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

int
snic_connect(int cn, int ap, int b_channel, int inf_mask, int out_serv
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

int
snic_listen(int cn, int ap, int inf_mask, int subadr_mask, int si_mask, int spv)
{
	u_short         sbuf[4];

	if (spv)
		inf_mask |= 0x40000000;
	*(u_long *) sbuf = inf_mask;
	sbuf[2] = subadr_mask;
	sbuf[3] = si_mask;
	return (en_q(isdn_ctrl[cn].unit, DD_LISTEN_REQ, 0, MK_APPL(ap), 8, (u_char *) sbuf));
}

int
snic_disconnect(int cn, int rea)
{
	isdn_ctrl_t    *ctrl = &isdn_ctrl[cn];
	chan_t         *chan = &snic_sc[ctrl->unit].sc_chan[C_CHAN(cn)];
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

int
snic_accept(int cn, int an, int rea)
{
	isdn_ctrl_t    *ctrl = &isdn_ctrl[cn];
	struct snic_softc *sc = &snic_sc[ctrl->unit];
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

int
snic_output(int cn)
{
	isdn_ctrl_t    *ctrl = &isdn_ctrl[cn];
	struct snic_softc *sc = &snic_sc[ctrl->unit];
	chan_t         *chan = &sc->sc_chan[C_CHAN(cn)];
	int		len= ctrl->o_len;
	Buffer *b= &chan->o_buf;
	int error= 0;

	if (sc->sc_state_ind[1] || (chan->ncci == -1))
		return (ENODEV);

	if(chan->state != IDLE)
		return(EBUSY);
	chan->state= WAITING;

	bzero(b,10);
	
	b->h.Type = BD_DATA_B3_REQ;
	if(C_CHAN(cn)) b->h.Type |= 0x40;
	b->h.PLCI = chan->plci;
	b->h.DataLen= len+5;
	*(u_short *) b->Data = chan->ncci;
	*(u_short *) &b->Data[2] = 0;
	b->h.Number = b->Data[4] = chan->msg_nr++;

	chan->state = ACTIVE;
	ctrl->lastact = time.tv_sec;

	if((error= snic_put_msg(ctrl->unit,(Header *) b,15+len,14)) == EBUSY)
	{
		sc->sc_ostat|= C_CHAN(cn)?0x800:0x400;
		chan->state= WAITING;
		return(0);
	}
	return(error);
}

static void
badstate(Header *h, int n)
{
	int i;
	u_char *p= (u_char *)h;
	printf("Niccy: not implemented %x.%x len %d at %d", h->Type,
		h->SubType, h->DataLen,n);
	if(h->DataLen)
	{
		p+= 10;
		for(i=0; i < h->DataLen ; i++) printf(" %x",p[i]);
	}
	printf("\n");
}

unsigned  SavMsgTyp;

static void
snic_interupt(unsigned unit)
{
	struct snic_softc * sc= &snic_sc[unit&0xff];
	Buffer *msg;
	chan_t         *chan;
	u_short        n, mb, c, pl, err = 0;
	isdn_ctrl_t    *ctrl;
	isdn_appl_t    *appl;
	int error= 0;

	msg = &sc->sc_imsg;
	chan= NULL;
	ctrl= NULL;
	appl= NULL;

SavMsgTyp= msg->h.Type;

	if(sc->sc_istat & 2)
		return;

	if(sc->sc_ostat&0xff)
	{
		sc->sc_istat|= 0x101;
		if(sc->sc_istat&0x200)
			return;
		sc->sc_istat|= 0x200;
		timeout(snic_timout,unit,2);
		return;
	}

	mb= 0;
	pl = msg->h.PLCI;
	if(pl && (msg->h.Type >= 0x40) && (msg->h.Type < 0xfd) && (msg->h.Type != 0x47))
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

		if(msg->h.Type >= 0x80)
		{
			mb= msg->h.Type & 0x40;
			msg->h.Type &= 0xbf;
		}
	}
SavMsgTyp|= 0x100;

if(msg->h.Type>=0x40)
printf("I%x %x %x",msg->h.Type,pl,mb);
	switch (msg->h.Type)
	{
	case 0x01:		/* INIT IND */
	case 0x15:		/* POLL IND */
		error= en_q(unit,msg->h.Type|0x20,0,0,0,NULL);
		break;
	case 0x04:		/* DNL MOD CONF */
		sc->sc_stat = msg->Data[0];
		if (sc->sc_flags & 7)
			sc->sc_flags = (sc->sc_flags & ~7) | OPEN;
		break;
	case 0x06:		/* DNL MOD IND */
		sc->sc_stat = msg->Data[0];
		error= en_q(unit,msg->h.Type|0x20,sc->sc_type,0,1, &msg->Data[1]);
		if(sc->sc_flags & LOAD_ENTITY)
		{
			sc->sc_istat= sc->sc_ostat= 2;
			timeout(snic_timout,unit,hz);
			msg->h.Type= 0;
			return;
		}
		if (sc->sc_flags)
			sc->sc_flags = OPEN;
		break;
	case 0x0e:		/* SET CLOCK CONF */
		error= state_ind(unit,1,0);
		break;
	case 0x16:		/* STATE IND */
		if(sc->sc_flags & LOAD_ENTITY)
		{
			if(sc->sc_flags & 7)
				sc->sc_flags = OPEN;
			else sc->sc_flags= 0;
		}
		bcopy( msg->Data, sc->sc_state_ind, 8);
		error= en_q(unit,msg->h.Type|0x20,0,0,0,NULL);
		break;
	case 0x17:		/* STATE RESP */
		bcopy( msg->Data, sc->sc_state_ind, 8);
		break;
	case 0x1e:              /* MANUFACT CONF */
		if(msg->h.SubType == 18)
			break;
		badstate(&msg->h,1);
		break;
	case 0x1f:              /* MANUFACT IND */
		if(msg->h.SubType == 19)
		{
			isdn_input(ispy_applnr, msg->h.DataLen, msg->Data,0);
			error= en_q(unit,msg->h.Type|0x20,msg->h.SubType,0,0,NULL);
			break;
		}
		badstate(&msg->h,2);
		break;
	case 0x40:		/* CONNECT CONF */
		err = *(u_short *) msg->Data;
		if (err || (appl == NULL) || (chan == NULL) || (ctrl == NULL))
		{
			if(chan) reset_plci(3, chan, pl);
			if(appl) appl->state= 0;
			break;
		}
		if (ISBUSY(ctrl->appl))
		{
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
		msg->Data[msg->Data[3] + 4] = 0;
		isdn_accept_con_ind(APPL(pl), chan->ctrl, msg->Data[0], msg->Data[1]
		       ,msg->Data[2], msg->Data[3], (char *) &msg->Data[4]);
		break;

	case 0x42:		/* CONNECT ACTIVE IND */
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
		error= discon_resp(unit, reset_plci(5, chan, pl));
		break;

	case 0x47:		/* LISTEN CONF */
		isdn_state = *(u_short *) msg->Data;
		break;

	case 0x4a:		/* INFO IND */
		isdn_info(APPL(pl),*(u_short *)msg->Data, msg->Data[2], msg->Data+3);
		error= inf_resp(unit, pl);
		break;
	case 0x80:	/* SELECT PROT CONF */
		err = *(u_short *) msg->Data;
		if (err)
		{
			error= discon_req(4, unit, pl, 0, err);
			break;
		}

		switch (msg->h.SubType)
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
		err = *(u_short *) msg->Data;
		if (err)
		{
			error= discon_req(5, unit, pl, 0, err);
			break;
		}
		error= con_resp(unit, pl, err);
		break;

	case 0x82:	/* CONNECT B3 CONF */
		err = *(u_short *) (msg->Data + 2);
		n = *(u_short *) msg->Data;

		if (err)
		{
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
		n = *(u_short *) msg->Data;
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
			timeout(isdn_start_out,chan->ctrl,hz/5);
		}
		break;

	case 0x85:	/* DISCONNECT B3 CONF */
		if(ISBUSY(ctrl->appl))
			chan->state = ISDISCON;
		err = *(u_short *) (msg->Data + 2);
		if (err)
		{
			error= discon_req(7, unit, pl, 0, err);
			break;
		}
		break;
	case 0x86:	/* DISCONNECT B3 IND */
		if(ISBUSY(ctrl->appl))
			chan->state = ISDISCON;
		err = *(u_short *) (msg->Data + 2);
		error= discon_req(8, unit, pl, 0, err);
		break;

	case 0x88:	/* DATA B3 CONF */
		if(ISFREE(ctrl->appl))
			break;
		err = *(u_short *) (msg->Data + 2);
		if (err)
		{
printf("e%x\n",err);
			ctrl->send_err++;
			isdn_appl[ctrl->appl].send_err++;
		}
		chan->state = IDLE;
		chan->o_buf.h.Type= 0;
		ctrl->o_len = 0;
		isdn_start_out(chan->ctrl);
		break;

	case 0x89:	/* DATA B3 IND */
		if(ISFREE(ctrl->appl))
			break;
		if(isdn_input(ctrl->appl, msg->h.DataLen-5, msg->Data+5,ctrl->islisten))
			ctrl->lastact = time.tv_sec;
		break;

	default:
		badstate(&msg->h,3);
		break;
	}

fin:
	if(error)
	{
printf("x%x %x %x %x %x\n",error,msg->h.Type,sc->sc_istat,sc->sc_ostat,sc->sc_omsg.h.Type);
		sc->sc_istat|= 0x101;
		if(sc->sc_istat&0x200)
			return;
		sc->sc_istat|= 0x200;
		timeout(snic_timout,unit,2);
		return;
	}

	msg->h.Type= 0;
	if(snic_put_msg(unit,(Header *) &ack_msg,1,15))
		sc->sc_ostat|= 0x100;
	sc->sc_istat= 0x200;
	snic_get_msg(unit);
}

#endif				/* NSNIC > 0 */
