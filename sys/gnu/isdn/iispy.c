static char     _ispyid[] = "@(#)$Id: iispy.c,v 1.1 1995/01/25 14:06:18 jkr Exp jkr $";
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
 * $Log: iispy.c,v $
 *
 ******************************************************************************/

#include "ispy.h"
#if NISPY > 0

#include "param.h"
#include "buf.h"
#include "systm.h"
#include "ioctl.h"
#include "tty.h"
#include "proc.h"
#include "user.h"
#include "uio.h"
#include "kernel.h"
/*#include "malloc.h"*/

#include "isdn/isdn_ioctl.h"

int             ispyattach();

int     nispy = NISPY;
int	ispy_applnr;
static int	next_if =0;
static unsigned long ispy_cnt, ispy_out;
static char	dir;
#define ISPY_SIZE	260
#define OPEN		1
#define READ_WAIT	2
#define ISPYBUF		16
#define ISPYMASK	(ISPYBUF-1)
/* ISPYBUF has to be a power of 2 */

static
struct ispy_data
{
	struct ispy_buf
	{
		unsigned long cnt;
		struct timeval stamp;
		char ibuf[ISPY_SIZE];
		unsigned char dir;
		int ilen;
	} b[ISPYBUF];
	int state;
} ispy_data[NISPY];

int
ispyattach(int ap)
{
	struct ispy_data *ispy;
	if(next_if >= NISPY)
		return(-1);
	ispy= &ispy_data[next_if];
	ispy->state= 0;
	ispy_applnr= ap;
	return(next_if++);
}

int
ispy_input(int no, int len, char *buf, int out)
{
	struct ispy_data *ispy= &ispy_data[no];
	struct ispy_buf *b= &ispy->b[ispy_cnt&ISPYMASK];

	if(len > ISPY_SIZE)
		return(0);
	if(len)
	{
		b->cnt= ispy_cnt++;
		b->stamp= time;
		b->dir= out;
		bcopy(buf, b->ibuf, len);
	}
	b->ilen= len;
	if(ispy->state & READ_WAIT)
	{
		ispy->state &= ~READ_WAIT;
		wakeup((caddr_t) &ispy->state);
	}
	return(len);
}

int
ispyopen(dev_t dev, int flag)
{
	int             err;
	struct ispy_data *ispy;

	if (minor(dev)>NISPY)
		return (ENXIO);

	ispy= &ispy_data[minor(dev)];

	if(ispy->state&OPEN) return(EBUSY);
	ispy->state |= OPEN;

	return (0);
}

int
ispyclose(dev_t dev, int flag)
{
	struct ispy_data *ispy= &ispy_data[minor(dev)];

	if(ispy->state & READ_WAIT)
		wakeup((caddr_t) &ispy->state);
	ispy->state = 0;
	return (0);
}

int
ispyioctl (dev, cmd, data, flag)
dev_t           dev;
caddr_t         data;
int cmd, flag;
{
        int     unit = minor(dev);

        switch (cmd) {
            default:
                return (ENOTTY);
        }
        return (0);
}

int
ispyread(dev_t dev, struct uio * uio)
{
	int             x;
	int             error = 0;
	struct ispy_data *ispy= &ispy_data[minor(dev)];
	struct ispy_buf *b;

	if((ispy_cnt-ispy_out) > ISPYBUF)
		ispy_out= ispy_cnt - ISPYBUF;
	b= &ispy->b[ispy_out&ISPYMASK];
	ispy_out++;
	while(b->ilen == 0)
	{
		ispy->state |= READ_WAIT;
		if(error= tsleep((caddr_t) &ispy->state, TTIPRI | PCATCH, "ispy", 0 ))
			return(error);
	}

	x = splhigh();
	if(b->ilen)
	{
		error = uiomove((char *) &b->dir, 1, uio);
		if(error == 0)
			error = uiomove((char *) &b->cnt
			,sizeof(unsigned long)+sizeof(struct timeval)+b->ilen, uio);
		b->ilen= 0;
	}
	splx(x);
	return error;
}

#endif
