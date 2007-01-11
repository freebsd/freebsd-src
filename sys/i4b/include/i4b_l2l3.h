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
 *	i4b_l2l3.h - i4b layer 2 / layer 3 interactions
 *	-----------------------------------------------
 *
 * $FreeBSD: src/sys/i4b/include/i4b_l2l3.h,v 1.9 2005/01/06 22:18:18 imp Exp $
 *
 *	last edit-date: [Sat Mar  9 15:55:16 2002]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_L2L3_H_
#define _I4B_L2L3_H_

extern struct i4b_l2l3_func i4b_l2l3_func;

struct i4b_l2l3_func
{
	/* Layer 2 --> Layer 3 */
	/* =================== */

	int	(*DL_ESTABLISH_IND) (int);
	int	(*DL_ESTABLISH_CNF) (int);
	
	int	(*DL_RELEASE_IND) (int);
	int	(*DL_RELEASE_CNF) (int);

	int	(*DL_DATA_IND) (int, struct mbuf *);

	int	(*DL_UNIT_DATA_IND) (int, struct mbuf *);	
	
#define DL_Est_Ind(unit)		\
	((*i4b_l2l3_func.DL_ESTABLISH_IND)(unit))
#define DL_Est_Cnf(unit)		\
	((*i4b_l2l3_func.DL_ESTABLISH_CNF)(unit))
#define DL_Rel_Ind(unit)		\
	((*i4b_l2l3_func.DL_RELEASE_IND)(unit))
#define DL_Rel_Cnf(unit)		\
	((*i4b_l2l3_func.DL_RELEASE_CNF)(unit))
#define DL_Data_Ind(unit, data)		\
	((*i4b_l2l3_func.DL_DATA_IND)(unit, data))
#define DL_Unit_Data_Ind(unit, data)	\
	((*i4b_l2l3_func.DL_UNIT_DATA_IND)(unit, data))
	
#define DL_Est_Ind_A			\
	(i4b_l2l3_func.DL_ESTABLISH_IND)
#define DL_Est_Cnf_A			\
	(i4b_l2l3_func.DL_ESTABLISH_CNF)
#define DL_Rel_Ind_A			\
	(i4b_l2l3_func.DL_RELEASE_IND)
#define DL_Rel_Cnf_A			\
	(i4b_l2l3_func.DL_RELEASE_CNF)
	
	/* Layer 3 --> Layer 2 */	
	/* =================== */

	int	(*DL_ESTABLISH_REQ) (int);
	
	int	(*DL_RELEASE_REQ) (int);

	int	(*DL_DATA_REQ) (int, struct mbuf *);

	int	(*DL_UNIT_DATA_REQ) (int, struct mbuf *);	
	
#define DL_Est_Req(unit)		\
	((*i4b_l2l3_func.DL_ESTABLISH_REQ)(unit))
#define DL_Rel_Req(unit)		\
	((*i4b_l2l3_func.DL_RELEASE_REQ)(unit))
#define DL_Data_Req(unit, data)		\
	((*i4b_l2l3_func.DL_DATA_REQ)(unit, data))
#define DL_Unit_Data_Req(unit, data)	\
	((*i4b_l2l3_func.DL_UNIT_DATA_REQ)(unit, data))

	/* Layer 2 --> Layer 3 management */
	/* ============================== */

	int	(*MDL_STATUS_IND) (int, int, int); 	/* L2 --> L3 status */
	
#define MDL_Status_Ind(unit, status, parm)		\
	((*i4b_l2l3_func.MDL_STATUS_IND)(unit, status, parm))

	/* Layer 3 --> Layer 2 management */
	/* ============================== */

	int	(*MDL_COMMAND_REQ) (int, int, void *);	/* L3 --> L2 command */

#define MDL_Command_Req(unit, command, parm)		\
	((*i4b_l2l3_func.MDL_COMMAND_REQ)(unit, command, parm))
};

#endif /* _I4B_L2L3_H_ */
