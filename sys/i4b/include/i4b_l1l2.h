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
 *	i4b_l1l2.h - i4b layer 1 / layer 2 interactions
 *	---------------------------------------------------
 *
 *	$Id: i4b_l1l2.h,v 1.11 2000/06/02 16:14:35 hm Exp $
 *
 * $FreeBSD$
 *
 *	last edit-date: [Fri Jun  2 14:35:03 2000]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_L1L2_H_
#define _I4B_L1L2_H_

#ifdef __FreeBSD__
#include <machine/i4b_trace.h>
#else
#include <i4b/i4b_trace.h>
#endif

extern struct i4b_l1l2_func i4b_l1l2_func;

struct i4b_l1l2_func
{
	/* Layer 1 --> Layer 2 */
	/* =================== */

	int	(*PH_DATA_IND) (int, struct mbuf *);
	int	(*PH_ACTIVATE_IND) (int);
	int	(*PH_DEACTIVATE_IND) (int);

#define PH_Data_Ind(unit, data)		\
	((*i4b_l1l2_func.PH_DATA_IND)(unit, data))
#define PH_Act_Ind(unit)		\
	((*i4b_l1l2_func.PH_ACTIVATE_IND)(unit))
#define PH_Deact_Ind(unit)		\
	((*i4b_l1l2_func.PH_DEACTIVATE_IND)(unit))


	/* Layer 2 --> Layer 1 */	
	/* =================== */
	
	int	(*PH_DATA_REQ) (int, struct mbuf *, int);
	int	(*PH_ACTIVATE_REQ) (int);

#define PH_Data_Req(unit, data, freeflag)	\
	((*i4b_l1l2_func.PH_DATA_REQ)(unit, data, freeflag))
#define PH_Act_Req(unit)			\
	((*i4b_l1l2_func.PH_ACTIVATE_REQ)(unit))

	/* Layer 1 --> upstream, ISDN trace data */
	/* ===================================== */
	int	(*MPH_TRACE_IND) (i4b_trace_hdr_t *, int, unsigned char *);

#define MPH_Trace_Ind(header, length, pointer)	\
	((*i4b_l1l2_func.MPH_TRACE_IND)(header, length, pointer))

	/* L1/L2 management command and status information */
	/* =============================================== */
	int	(*MPH_STATUS_IND) (int, int, int);
	int	(*MPH_COMMAND_REQ) (int, int, void *);

#define MPH_Status_Ind(unit, status, parm)	\
	((*i4b_l1l2_func.MPH_STATUS_IND)(unit, status, parm))
#define MPH_Command_Req(unit, command, parm)	\
	((*i4b_l1l2_func.MPH_COMMAND_REQ)(unit, command, parm))
};
	
#endif /* _I4B_L1L2_H_ */

