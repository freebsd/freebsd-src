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
 *	i4b_l4mgmt.c - layer 4 calldescriptor management utilites
 *	-----------------------------------------------------------
 *
 *	$Id: i4b_l4mgmt.c,v 1.26 1999/12/13 21:25:28 hm Exp $ 
 *
 * $FreeBSD: src/sys/i4b/layer4/i4b_l4mgmt.c,v 1.6.2.1 2000/05/10 02:04:48 obrien Exp $
 *
 *      last edit-date: [Mon Dec 13 22:06:32 1999]
 *
 *---------------------------------------------------------------------------*/

#include "i4b.h"

#if NI4B > 0

#include <sys/param.h>

#if defined(__FreeBSD__)
#include <sys/ioccom.h>
#include <sys/random.h>
#else
#include <sys/ioctl.h>
#endif

#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#endif

#include <i4b/include/i4b_l2l3.h>
#include <i4b/include/i4b_l3l4.h>
#include <i4b/include/i4b_mbuf.h>
#include <i4b/include/i4b_isdnq931.h>
#include <i4b/include/i4b_global.h>

#include <i4b/layer4/i4b_l4.h>

call_desc_t call_desc[N_CALL_DESC];	/* call descriptor array */

static unsigned int get_cdid(void);

int nctrl;				/* number of attached controllers */

#if defined(__FreeBSD__)
void init_callout(call_desc_t *);
#endif

/*---------------------------------------------------------------------------*
 *      return a new unique call descriptor id
 *	--------------------------------------
 *	returns a new calldescriptor id which is used to uniquely identyfy
 *	a single call in the communication between kernel and userland.
 *	this cdid is then used to associate a calldescriptor with an id.
 *---------------------------------------------------------------------------*/
static unsigned int
get_cdid(void)
{
	static unsigned int cdid_count = 0;
	int i;
	int x;

	x = SPLI4B();   

	/* get next id */
	
	cdid_count++;
	
again:
	if(cdid_count == CDID_UNUSED)		/* zero is invalid */
		cdid_count++;
	else if(cdid_count > CDID_MAX)		/* wraparound ? */
		cdid_count = 1;

	/* check if id already in use */
	
	for(i=0; i < N_CALL_DESC; i++)
	{
		if(call_desc[i].cdid == cdid_count)
		{
			cdid_count++;
			goto again;
		}
	}

	splx(x);
	
	return(cdid_count);
}

/*---------------------------------------------------------------------------*
 *      reserve a calldescriptor for later usage
 *      ----------------------------------------
 *      searches the calldescriptor array until an unused
 *      descriptor is found, gets a new calldescriptor id
 *      and reserves it by putting the id into the cdid field.
 *      returns pointer to the calldescriptor.
 *---------------------------------------------------------------------------*/
call_desc_t *
reserve_cd(void)
{
	call_desc_t *cd;
	int x;
	int i;

	x = SPLI4B();

	cd = NULL;
	
	for(i=0; i < N_CALL_DESC; i++)
	{
		if(call_desc[i].cdid == CDID_UNUSED)
		{
			bzero(&call_desc[i], sizeof(call_desc_t)); /* clear it */
			call_desc[i].cdid = get_cdid();	/* fill in new cdid */
			cd = &(call_desc[i]);	/* get pointer to descriptor */
			DBGL4(L4_MSG, "reserve_cd", ("found free cd - index=%d cdid=%u\n",
				 i, call_desc[i].cdid));
			break;
		}
	}

	splx(x);

	if(cd == NULL)
		panic("reserve_cd: no free call descriptor available!");

#if defined(__FreeBSD__)
	init_callout(cd);
#endif

	return(cd);
}

/*---------------------------------------------------------------------------*
 *      free a calldescriptor
 *      ---------------------
 *      free a unused calldescriptor by giving address of calldescriptor
 *      and writing a 0 into the cdid field marking it as unused.
 *---------------------------------------------------------------------------*/
void
freecd_by_cd(call_desc_t *cd)
{
	int i;
	int x = SPLI4B();
	
	for(i=0; i < N_CALL_DESC; i++)
	{
		if( (call_desc[i].cdid != CDID_UNUSED) &&
		    (&(call_desc[i]) == cd) )
		{
			DBGL4(L4_MSG, "freecd_by_cd", ("releasing cd - index=%d cdid=%u cr=%d\n",
				i, call_desc[i].cdid, cd->cr));
			call_desc[i].cdid = CDID_UNUSED;
			break;
		}
	}

	if(i == N_CALL_DESC)
		panic("freecd_by_cd: ERROR, cd not found, cr = %d\n", cd->cr);

	splx(x);		
}

/*---------------------------------------------------------------------------*
 *      return pointer to calldescriptor by giving the calldescriptor id
 *      ----------------------------------------------------------------
 *      lookup a calldescriptor in the calldescriptor array by looking
 *      at the cdid field. return pointer to calldescriptor if found,
 *      else return NULL if not found.
 *---------------------------------------------------------------------------*/
call_desc_t *
cd_by_cdid(unsigned int cdid)
{
	int i;

	for(i=0; i < N_CALL_DESC; i++)
	{
		if(call_desc[i].cdid == cdid)
		{
			DBGL4(L4_MSG, "cd_by_cdid", ("found cdid - index=%d cdid=%u cr=%d\n",
					i, call_desc[i].cdid, call_desc[i].cr));
#if defined(__FreeBSD__)
			init_callout(&call_desc[i]);
#endif
			return(&(call_desc[i]));
		}
	}
	return(NULL);
}

/*---------------------------------------------------------------------------*
 *      search calldescriptor
 *      ---------------------
 *      This routine searches for the calldescriptor for a passive controller
 *      given by unit number, callreference and callreference flag.
 *	It returns a pointer to the calldescriptor if found, else a NULL.
 *---------------------------------------------------------------------------*/
call_desc_t *
cd_by_unitcr(int unit, int cr, int crf)
{
	int i;

	for(i=0; i < N_CALL_DESC; i++)
	{
	  if((call_desc[i].cdid != CDID_UNUSED)                                       &&
	     (ctrl_desc[call_desc[i].controller].ctrl_type == CTRL_PASSIVE) &&
	     (ctrl_desc[call_desc[i].controller].unit == unit)              &&
	     (call_desc[i].cr == cr)                                        &&
	     (call_desc[i].crflag == crf) )
	  {
	    DBGL4(L4_MSG, "cd_by_unitcr", ("found cd, index=%d cdid=%u cr=%d\n",
				i, call_desc[i].cdid, call_desc[i].cr));
#if defined(__FreeBSD__)
	    init_callout(&call_desc[i]);
#endif
	    return(&(call_desc[i]));
	  }
	}
	return(NULL);
}

/*---------------------------------------------------------------------------*
 *	generate 7 bit "random" number used for outgoing Call Reference
 *---------------------------------------------------------------------------*/
unsigned char
get_rand_cr(int unit)
{
	register int i, j;
	static u_char val, retval;
	static int called = 42;
	
	val += ++called;
	
	for(i=0; i < 50 ; i++, val++)
	{
		int found = 1;
		
#if defined(__FreeBSD__)
		read_random((char *)&val, sizeof(val));
#else
		val |= unit+i;
		val <<= i;
		val ^= (time.tv_sec >> 8) ^ time.tv_usec;
		val <<= i;
		val ^= time.tv_sec ^ (time.tv_usec >> 8);
#endif

		retval = val & 0x7f;
		
		if(retval == 0 || retval == 0x7f)
			continue;

		for(j=0; j < N_CALL_DESC; j++)
		{
			if( (call_desc[j].cdid != CDID_UNUSED) &&
			    (call_desc[j].cr == retval) )
			{
				found = 0;
				break;
			}
		}

		if(found)
			return(retval);
	}
	return(0);	/* XXX */
}

/*---------------------------------------------------------------------------*
 *	initialize the callout handles for FreeBSD
 *---------------------------------------------------------------------------*/
#if defined(__FreeBSD__)
void
init_callout(call_desc_t *cd)
{
	if(cd->callouts_inited == 0)
	{
		callout_handle_init(&cd->idle_timeout_handle);
		callout_handle_init(&cd->T303_callout);
		callout_handle_init(&cd->T305_callout);
		callout_handle_init(&cd->T308_callout);
		callout_handle_init(&cd->T309_callout);
		callout_handle_init(&cd->T310_callout);
		callout_handle_init(&cd->T313_callout);
		callout_handle_init(&cd->T400_callout);
		cd->callouts_inited = 1;
	}
}
#endif

/*---------------------------------------------------------------------------*
 *      daemon is attached
 *---------------------------------------------------------------------------*/
void 
i4b_l4_daemon_attached(void)
{
	int i;

	int x = SPLI4B();
	
	for(i=0; i < nctrl; i++)
	{
/*XXX*/		if(ctrl_desc[i].ctrl_type == CTRL_PASSIVE)
		{
			DBGL4(L4_MSG, "i4b_l4_daemon_attached", ("CMR_DOPEN sent to unit %d\n",	ctrl_desc[i].unit));

			(*ctrl_desc[i].N_MGMT_COMMAND)(ctrl_desc[i].unit, CMR_DOPEN, 0);
		}
	}
	splx(x);
}

/*---------------------------------------------------------------------------*
 *      daemon is detached
 *---------------------------------------------------------------------------*/
void 
i4b_l4_daemon_detached(void)
{
	int i;

	int x = SPLI4B();

	for(i=0; i < nctrl; i++)
	{
/*XXX*/		if(ctrl_desc[i].ctrl_type == CTRL_PASSIVE)
		{
			DBGL4(L4_MSG, "i4b_l4_daemon_detached", ("CMR_DCLOSE sent to unit %d\n", ctrl_desc[i].unit));

			(*ctrl_desc[i].N_MGMT_COMMAND)(ctrl_desc[i].unit, CMR_DCLOSE, 0);
		}
	}
	splx(x);
}

#ifdef I4B_CD_DEBUG_PRINT

extern char *print_l3state(call_desc_t *cd);

void i4b_print_cdp(call_desc_t *cdp);
void i4b_print_cdx(int index);
void i4b_print_cda(void);
void i4b_print_cdaa(void);
	
/*---------------------------------------------------------------------------*
 *	print a call descriptor by cd-pointer
 *---------------------------------------------------------------------------*/
void 
i4b_print_cdp(call_desc_t *cdp)
{
	if((cdp > &(call_desc[N_CALL_DESC])) || (cdp < &(call_desc[0])))
	{
		printf("i4b_print_cd: cdp out of range!\n");
		return;
	}

	printf("i4b_print_cd: printing call descriptor %d at 0x%lx:\n", cdp - (&(call_desc[0])), (unsigned long)cdp);

	printf("         cdid = %d\n", cdp->cdid);
	printf("   controller = %d (u=%d, dl=%d, b1=%d, b2=%d)\n",
			cdp->controller,
			ctrl_desc[cdp->controller].unit,
			ctrl_desc[cdp->controller].dl_est,
			ctrl_desc[cdp->controller].bch_state[CHAN_B1],
			ctrl_desc[cdp->controller].bch_state[CHAN_B2]);	
	printf("           cr = 0x%02x\n", cdp->cr);
	printf("       crflag = %d\n", cdp->crflag);
	printf("    channelid = %d\n", cdp->channelid);
	printf("        bprot = %d\n", cdp->bprot);
	printf("       driver = %d\n", cdp->driver);
	printf("  driver_unit = %d\n", cdp->driver_unit);
	printf("   call_state = %d\n", cdp->call_state);
	printf("    Q931state = %s\n", print_l3state(cdp));
	printf("        event = %d\n", cdp->event);
	printf("     response = %d\n", cdp->response);
	printf("         T303 = %d\n", cdp->T303);
	printf("T303_first_to = %d\n", cdp->T303_first_to);
	printf("         T305 = %d\n", cdp->T305);
	printf("         T308 = %d\n", cdp->T308);
	printf("T308_first_to = %d\n", cdp->T308_first_to);
	printf("         T309 = %d\n", cdp->T309);
	printf("         T310 = %d\n", cdp->T310);
	printf("         T313 = %d\n", cdp->T313);
	printf("         T400 = %d\n", cdp->T400);
	printf("          dir = %s\n", cdp->dir == DIR_OUTGOING ? "out" : "in");
}

/*---------------------------------------------------------------------------*
 *	print a call descriptor by index
 *---------------------------------------------------------------------------*/
void 
i4b_print_cdx(int index)
{
	if(index >= N_CALL_DESC)
	{
		printf("i4b_print_cdx: index %d >= N_CALL_DESC %d\n", index, N_CALL_DESC);
		return;
	}
	i4b_print_cdp(&(call_desc[index]));
}

/*---------------------------------------------------------------------------*
 *	print all call descriptors
 *---------------------------------------------------------------------------*/
void 
i4b_print_cda(void)
{
	int i;

	for(i=0; i < N_CALL_DESC; i++)
	{
		i4b_print_cdp(&(call_desc[i]));
	}
}

/*---------------------------------------------------------------------------*
 *	print all active call descriptors
 *---------------------------------------------------------------------------*/
void 
i4b_print_cdaa(void)
{
	int i;

	for(i=0; i < N_CALL_DESC; i++)
	{
		if(call_desc[i].cdid != CDID_UNUSED)
		{
			i4b_print_cdp(&(call_desc[i]));
		}
	}
}

#endif /* I4B_CD_DEBUG_PRINT */

#endif /* NI4BQ931 > 0 */
