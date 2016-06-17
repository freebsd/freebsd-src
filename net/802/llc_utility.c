/*
 * NET		An implementation of the IEEE 802.2 LLC protocol for the
 *		LINUX operating system.  LLC is implemented as a set of 
 *		state machines and callbacks for higher networking layers.
 *
 *		Small utilities, Linux timer handling.
 *
 *		Written by Tim Alpaerts, Tim_Alpaerts@toyota-motor-europe.com
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	Changes
 *		Alan Cox	:	Chainsawed into Linux form.
 *					Added llc_ function name prefixes.
 *					Fixed bug in stop/start timer.
 *					Added llc_cancel_timers for closing
 *						down an llc
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <net/llc_frame.h>
#include <net/llc.h>

int llc_decode_frametype(frameptr fr)
{
	if (IS_UFRAME(fr)) 
	{      /* unnumbered cmd/rsp */
		switch(fr->u_mm.mm & 0x3B)
		{
			case 0x1B:
			    return(SABME_CMD);
			    break;
			case 0x10:
			    return(DISC_CMD);
			    break;
			case 0x18:
			    return(UA_RSP);
			    break;
			case 0x03:
			    return(DM_RSP);
			    break;
			case 0x21:
			    return(FRMR_RSP);
			    break;
			case 0x00:
			    return(UI_CMD);
			    break;
			case 0x2B:
		 	    if (IS_RSP(fr)) 
		 	    	return(XID_RSP);
			    else
			    	return(XID_CMD);
			    break;
			case 0x38:
		    	    if (IS_RSP(fr))
		    	    	return(TEST_RSP);
			    else
				return(TEST_CMD);
			    break;
			default:
			    return(BAD_FRAME);
		}
	}
	else if (IS_SFRAME(fr))
	{  /* supervisory cmd/rsp */
		switch(fr->s_hdr.ss)
		{
			case 0x00:
			    if (IS_RSP(fr)) 
			    	return(RR_RSP);
			    else
			    	return(RR_CMD);
			    break;
			case 0x02:
			    if (IS_RSP(fr))
			    	return(REJ_RSP);
			    else
			    	return(REJ_CMD);
			    break;
			case 0x01:
			    if (IS_RSP(fr))
			    	return(RNR_RSP);
			    else
			    	return(RNR_CMD);
			    break;
			default:
			    return(BAD_FRAME);
		}
	}
	else
	{			  /* information xfer */
		if (IS_RSP(fr)) 
			return(I_RSP);
		else	
			return(I_CMD);
	}
}


/*
 *	Validate_seq_nos will check N(S) and N(R) to see if they are
 *	invalid or unexpected.
 *	"unexpected" is explained on p44 Send State Variable.
 *	The return value is:
 *		4 * invalid N(R) +
 *		2 * invalid N(S) +
 *		1 * unexpected N(S)
 */

int llc_validate_seq_nos(llcptr lp, frameptr fr)
{
	int res;
     
	/*
	 *	A U-frame is always good 
	 */

	if (IS_UFRAME(fr)) 
		return(0);	

	/*
	 *	For S- and I-frames check N(R): 
	 */

	if (fr->i_hdr.nr == lp->vs) 
	{    	/* if N(R) = V(S)  */
        	res = 0;                        /* N(R) is good */
	}
	else
	{				/* lp->k = transmit window size */
    		if (lp->vs >= lp->k) 
    		{	/* if window not wrapped around 127 */
			if ((fr->i_hdr.nr < lp->vs) &&
				(fr->i_hdr.nr > (lp->vs - lp->k)))
				res = 0;
			else 
				res = 4;		/* N(R) invalid */
		}
		else
		{	/* window wraps around 127 */
			if ((fr->i_hdr.nr < lp->vs) ||
				(fr->i_hdr.nr > (128 + lp->vs - lp->k))) 
				res = 0;
			else
				res = 4;		/* N(R) invalid */
		}
	}

	/*
	 *	For an I-frame, must check N(S) also:  
	 */

	if (IS_IFRAME(fr)) 
	{
    		if (fr->i_hdr.ns == lp->vr) 
    			return res;   /* N(S) good */
		if (lp->vr >= lp->rw) 
		{
			/* if receive window not wrapped */

			if ((fr->i_hdr.ns < lp->vr) &&
				(fr->i_hdr.ns > (lp->vr - lp->k)))
				res = res +1;   	/* N(S) unexpected */
			else  
				res = res +2;         /* N(S) invalid */            
		}
		else
		{
			/* Window wraps around 127 */

			if ((fr->i_hdr.ns < lp->vr) ||
				(fr->i_hdr.ns > (128 + lp->vr - lp->k)))
				res = res +1;   	/* N(S) unexpected */
			else
				res = res +2;         /* N(S) invalid */            
		}
	}					
	return(res);
}

/* **************** timer management routines ********************* */

static void llc_p_timer_expired(unsigned long ulp)
{
	llc_timer_expired((llcptr) ulp, P_TIMER);
}

static void llc_rej_timer_expired(unsigned long ulp)
{
	llc_timer_expired((llcptr) ulp, REJ_TIMER);
}

static void llc_ack_timer_expired(unsigned long ulp)
{
	llc_timer_expired((llcptr) ulp, ACK_TIMER);
} 

static void llc_busy_timer_expired(unsigned long ulp)
{
	llc_timer_expired((llcptr) ulp, BUSY_TIMER);
}

/* exp_fcn is an array holding the 4 entry points of the
   timer expiry routines above.
   It is required to keep start_timer() generic.
   Thank you cdecl.
 */

static void (* exp_fcn[])(unsigned long) = 
{
	llc_p_timer_expired,
	llc_rej_timer_expired,
	llc_ack_timer_expired,
	llc_busy_timer_expired
};   

void llc_start_timer(llcptr lp, int t)
{
	if (lp->timer_state[t] == TIMER_IDLE)
	{
    		lp->tl[t].expires = jiffies + lp->timer_interval[t];
    		lp->tl[t].data = (unsigned long) lp;
    		lp->tl[t].function = exp_fcn[t];
    		add_timer(&lp->tl[t]);
    		lp->timer_state[t] = TIMER_RUNNING;
	}
}

void llc_stop_timer(llcptr lp, int t)
{
	if (lp->timer_state[t] == TIMER_RUNNING)
	{
        	del_timer(&lp->tl[t]);
        	lp->timer_state[t] = TIMER_IDLE;
	}
}

void llc_cancel_timers(llcptr lp)
{
	llc_stop_timer(lp, P_TIMER);
	llc_stop_timer(lp, REJ_TIMER);
	llc_stop_timer(lp, ACK_TIMER);
	llc_stop_timer(lp, BUSY_TIMER);
}

