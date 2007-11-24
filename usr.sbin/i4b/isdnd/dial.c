/*
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
 *
 *---------------------------------------------------------------------------
 *
 *	i4b daemon - dial handling routines
 *	-----------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Tue Mar 26 14:35:19 2002]
 *
 *---------------------------------------------------------------------------*/

#include "isdnd.h"

/*---------------------------------------------------------------------------*
 *	select the first remote number to dial according to the
 *	dial strategy
 *---------------------------------------------------------------------------*/
void
select_first_dialno(cfg_entry_t *cep)
{
	int i, j;

	if(cep->keypad[0] != '\0')
		return;

	if(cep->remote_numbers_count < 1)
	{
		log(LL_ERR, "select_first_dialno: remote_numbers_count < 1!");
		return;
	}

	if(cep->remote_numbers_count == 1)
	{
		strcpy(cep->remote_phone_dialout.number, cep->remote_numbers[0].number);
		strcpy(cep->remote_phone_dialout.subaddr, cep->remote_numbers[0].subaddr);
		DBGL(DL_DIAL, (log(LL_DBG, "select_first_dialno: only one no, no = %s", cep->remote_phone_dialout.number)));
		cep->last_remote_number = 0;
		return;
	}

	if(cep->remote_numbers_handling == RNH_FIRST)
	{
		strcpy(cep->remote_phone_dialout.number, cep->remote_numbers[0].number);
		strcpy(cep->remote_phone_dialout.subaddr, cep->remote_numbers[0].subaddr);
		DBGL(DL_DIAL, (log(LL_DBG, "select_first_dialno: use first, no = %s", cep->remote_phone_dialout.number)));
		cep->last_remote_number = 0;
		return;
	}

	i = cep->last_remote_number;
	   
	for(j = cep->remote_numbers_count; j > 0; j--)
	{
		if(cep->remote_numbers[i].flag == RNF_SUCC)
		{
			if(cep->remote_numbers_handling == RNH_LAST)
			{
				strcpy(cep->remote_phone_dialout.number, cep->remote_numbers[i].number);
				strcpy(cep->remote_phone_dialout.subaddr, cep->remote_numbers[i].subaddr);
				DBGL(DL_DIAL, (log(LL_DBG, "select_first_dialno: use last, no = %s", cep->remote_phone_dialout.number)));
				cep->last_remote_number = i;
				return;
			}
			else
			{
				if(++i >= cep->remote_numbers_count)
					i = 0;

				strcpy(cep->remote_phone_dialout.number, cep->remote_numbers[i].number);
				strcpy(cep->remote_phone_dialout.subaddr, cep->remote_numbers[i].subaddr);
				DBGL(DL_DIAL, (log(LL_DBG, "select_first_dialno: use next, no = %s", cep->remote_phone_dialout.number)));
				cep->last_remote_number = i;
				return;
			}
		}

		if(++i >= cep->remote_numbers_count)
			i = 0;
	}
	strcpy(cep->remote_phone_dialout.number, cep->remote_numbers[0].number);
	DBGL(DL_DIAL, (log(LL_DBG, "select_first_dialno: no last found (use 0), no = %s", cep->remote_phone_dialout.number)));
	cep->last_remote_number = 0;	
}									

/*---------------------------------------------------------------------------*
 *	select next remote number to dial (last was unsuccesfull)
 *---------------------------------------------------------------------------*/
void
select_next_dialno(cfg_entry_t *cep)
{
	if(cep->remote_numbers_count < 1)
	{
		log(LL_ERR, "select_next_dialno: remote_numbers_count < 1!");
		return;
	}

	if(cep->remote_numbers_count == 1)
	{
		strcpy(cep->remote_phone_dialout.number, cep->remote_numbers[0].number);
		strcpy(cep->remote_phone_dialout.subaddr, cep->remote_numbers[0].subaddr);
		DBGL(DL_DIAL, (log(LL_DBG, "select_next_dialno: only one no, no = %s", cep->remote_phone_dialout.number)));
		cep->last_remote_number = 0;
		return;
	}

	/* mark last try as bad */

	cep->remote_numbers[cep->last_remote_number].flag = RNF_IDLE;

	/* next one to try */
	
	cep->last_remote_number++;

	if(cep->last_remote_number >= cep->remote_numbers_count)
		cep->last_remote_number = 0;

	strcpy(cep->remote_phone_dialout.number, cep->remote_numbers[cep->last_remote_number].number);
	
	DBGL(DL_DIAL, (log(LL_DBG, "select_next_dialno: index=%d, no=%s",
		cep->last_remote_number,
		cep->remote_numbers[cep->last_remote_number].number)));
}									

/*---------------------------------------------------------------------------*
 *	dial succeded, store this number as the last successful
 *---------------------------------------------------------------------------*/
void
select_this_dialno(cfg_entry_t *cep)
{
	cep->remote_numbers[cep->last_remote_number].flag = RNF_SUCC;
	
	DBGL(DL_DIAL, (log(LL_DBG, "select_this_dialno: index = %d, no = %s",
		cep->last_remote_number,
		cep->remote_numbers[cep->last_remote_number].number)));
}

/* EOF */
