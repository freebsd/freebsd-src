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
 *	i4b_ctl.c - i4b system control port driver
 *	------------------------------------------
 *	last edit-date: [Sun Mar 17 09:49:24 2002]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i4b/driver/i4b_ctl.c,v 1.27.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/socket.h>
#include <net/if.h>

#include <i4b/include/i4b_debug.h>
#include <i4b/include/i4b_ioctl.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_l3l4.h>

#include <i4b/layer2/i4b_l2.h>

static int openflag = 0;

static	d_open_t	i4bctlopen;
static	d_close_t	i4bctlclose;
static	d_ioctl_t	i4bctlioctl;
static	d_poll_t	i4bctlpoll;


static struct cdevsw i4bctl_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	i4bctlopen,
	.d_close =	i4bctlclose,
	.d_ioctl =	i4bctlioctl,
	.d_poll =	i4bctlpoll,
	.d_name =	"i4bctl",
};

static void i4bctlattach(void *);
PSEUDO_SET(i4bctlattach, i4b_i4bctldrv);

/*---------------------------------------------------------------------------*
 *	interface attach routine
 *---------------------------------------------------------------------------*/
static void
i4bctlattach(void *dummy)
{
	printf("i4bctl: ISDN system control port attached\n");
	make_dev(&i4bctl_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "i4bctl");
}

/*---------------------------------------------------------------------------*
 *	i4bctlopen - device driver open routine
 *---------------------------------------------------------------------------*/
static int
i4bctlopen(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	if(minor(dev))
		return (ENXIO);

	if(openflag)
		return (EBUSY);
	
	openflag = 1;
	
	return (0);
}

/*---------------------------------------------------------------------------*
 *	i4bctlclose - device driver close routine
 *---------------------------------------------------------------------------*/
static int
i4bctlclose(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	openflag = 0;
	return (0);
}

/*---------------------------------------------------------------------------*
 *	i4bctlioctl - device driver ioctl routine
 *---------------------------------------------------------------------------*/
static int
i4bctlioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
#if DO_I4B_DEBUG
	ctl_debug_t *cdbg;	
	int error = 0;
#endif
	
#if !DO_I4B_DEBUG
       return(ENODEV);
#else
	if(minor(dev))
		return(ENODEV);

	switch(cmd)
	{
		case I4B_CTL_GET_DEBUG:
			cdbg = (ctl_debug_t *)data;
			cdbg->l1 = i4b_l1_debug;
			cdbg->l2 = i4b_l2_debug;
			cdbg->l3 = i4b_l3_debug;
			cdbg->l4 = i4b_l4_debug;
			break;
		
		case I4B_CTL_SET_DEBUG:
			cdbg = (ctl_debug_t *)data;
			i4b_l1_debug = cdbg->l1;
			i4b_l2_debug = cdbg->l2;
			i4b_l3_debug = cdbg->l3;
			i4b_l4_debug = cdbg->l4;
			break;

                case I4B_CTL_GET_CHIPSTAT:
                {
                        struct chipstat *cst;
			cst = (struct chipstat *)data;
			(*ctrl_desc[cst->driver_unit].N_MGMT_COMMAND)(cst->driver_unit, CMR_GCST, cst);
                        break;
                }

                case I4B_CTL_CLR_CHIPSTAT:
                {
                        struct chipstat *cst;
			cst = (struct chipstat *)data;
			(*ctrl_desc[cst->driver_unit].N_MGMT_COMMAND)(cst->driver_unit, CMR_CCST, cst);
                        break;
                }

                case I4B_CTL_GET_LAPDSTAT:
                {
                        l2stat_t *l2s;
                        l2_softc_t *sc;
                        l2s = (l2stat_t *)data;

                        if( l2s->unit < 0 || l2s->unit > MAXL1UNITS)
                        {
                        	error = EINVAL;
				break;
			}
			  
			sc = &l2_softc[l2s->unit];

			bcopy(&sc->stat, &l2s->lapdstat, sizeof(lapdstat_t));
                        break;
                }

                case I4B_CTL_CLR_LAPDSTAT:
                {
                        int *up;
                        l2_softc_t *sc;
                        up = (int *)data;

                        if( *up < 0 || *up > MAXL1UNITS)
                        {
                        	error = EINVAL;
				break;
			}
			  
			sc = &l2_softc[*up];

			bzero(&sc->stat, sizeof(lapdstat_t));
                        break;
                }

		default:
			error = ENOTTY;
			break;
	}
	return(error);
#endif /* DO_I4B_DEBUG */
}

/*---------------------------------------------------------------------------*
 *	i4bctlpoll - device driver poll routine
 *---------------------------------------------------------------------------*/
static int
i4bctlpoll (struct cdev *dev, int events, struct thread *td)
{
	return (ENODEV);
}
