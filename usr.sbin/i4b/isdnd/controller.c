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
 *	i4b daemon - controller state support routines
 *	----------------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon May 10 21:35:55 1999]
 *
 *---------------------------------------------------------------------------*/

#include "isdnd.h"

/*--------------------------------------------------------------------------*
 *	init controller state table entry
 *--------------------------------------------------------------------------*/
int
init_controller_state(int controller, int ctrl_type, int card_type, int tei)
{
	if((controller < 0) || (controller >= ncontroller))
	{
		log(LL_ERR, "init_controller_state: invalid controller number [%d]!", controller);
		return(ERROR);
	}
	
	/* init controller tab */
		
	if(ctrl_type == CTRL_PASSIVE)
	{
		if((card_type > CARD_TYPEP_UNK) &&
		   (card_type <= CARD_TYPEP_MAX))
		{
			isdn_ctrl_tab[controller].ctrl_type = ctrl_type;
			isdn_ctrl_tab[controller].card_type = card_type;
			isdn_ctrl_tab[controller].state = CTRL_UP;
			isdn_ctrl_tab[controller].stateb1 = CHAN_IDLE;
			isdn_ctrl_tab[controller].stateb2 = CHAN_IDLE;
			isdn_ctrl_tab[controller].freechans = MAX_CHANCTRL;
			isdn_ctrl_tab[controller].tei = tei;
			DBGL(DL_RCCF, (log(LL_DBG, "init_controller_state: controller %d is %s",
			  controller, 
			  name_of_controller(isdn_ctrl_tab[controller].ctrl_type,
					     isdn_ctrl_tab[controller].card_type))));
		}
		else
		{
			log(LL_ERR, "init_controller_state: unknown card type %d", card_type);
			return(ERROR);
		}
		
	}
	else if(ctrl_type == CTRL_DAIC)
	{
		isdn_ctrl_tab[controller].ctrl_type = ctrl_type;
		isdn_ctrl_tab[controller].card_type = card_type;
		isdn_ctrl_tab[controller].state = CTRL_DOWN;
		isdn_ctrl_tab[controller].stateb1 = CHAN_IDLE;
		isdn_ctrl_tab[controller].stateb2 = CHAN_IDLE;
		isdn_ctrl_tab[controller].freechans = MAX_CHANCTRL;
		isdn_ctrl_tab[controller].tei = -1;	
		log(LL_DMN, "init_controller_state: controller %d is %s",
		  controller,
		  name_of_controller(isdn_ctrl_tab[controller].ctrl_type,
				     isdn_ctrl_tab[controller].card_type));
	}
	else if(ctrl_type == CTRL_TINADD)
	{
		isdn_ctrl_tab[controller].ctrl_type = ctrl_type;
		isdn_ctrl_tab[controller].card_type = 0;
		isdn_ctrl_tab[controller].state = CTRL_DOWN;
		isdn_ctrl_tab[controller].stateb1 = CHAN_IDLE;
		isdn_ctrl_tab[controller].stateb2 = CHAN_IDLE;
		isdn_ctrl_tab[controller].freechans = MAX_CHANCTRL;
		isdn_ctrl_tab[controller].tei = -1;	
		log(LL_DMN, "init_controller_state: controller %d is %s",
		  controller,
		  name_of_controller(isdn_ctrl_tab[controller].ctrl_type,
				     isdn_ctrl_tab[controller].card_type));
		
	}
	else
	{
		log(LL_ERR, "init_controller_state: unknown controller type %d", ctrl_type);
		return(ERROR);
	}
	return(GOOD);
}	

/*--------------------------------------------------------------------------*
 *	init active controller
 *--------------------------------------------------------------------------*/
void
init_active_controller(void)
{
	int ret;
	int unit = 0;
	int controller;
	char cmdbuf[MAXPATHLEN+128];

	for(controller = 0; controller < ncontroller; controller++)
	{
		if(isdn_ctrl_tab[controller].ctrl_type == CTRL_TINADD)
		{
			DBGL(DL_RCCF, (log(LL_DBG, "init_active_controller, tina-dd %d: executing [%s %d]", unit, tinainitprog, unit)));
			
			sprintf(cmdbuf, "%s %d", tinainitprog, unit);

			if((ret = system(cmdbuf)) != 0)
			{
				log(LL_ERR, "init_active_controller, tina-dd %d: %s returned %d!", unit, tinainitprog, ret);
				do_exit(1);
			}
		}
	}
}	

/*--------------------------------------------------------------------------*
 *	set controller state to UP/DOWN
 *--------------------------------------------------------------------------*/
int
set_controller_state(int controller, int state)
{
	if((controller < 0) || (controller >= ncontroller))
	{
		log(LL_ERR, "set_controller_state: invalid controller number [%d]!", controller);
		return(ERROR);
	}

	if(state == CTRL_UP)
	{
		isdn_ctrl_tab[controller].state = CTRL_UP;
		DBGL(DL_CNST, (log(LL_DBG, "set_controller_state: controller [%d] set UP!", controller)));
	}
	else if (state == CTRL_DOWN)
	{
		isdn_ctrl_tab[controller].state = CTRL_DOWN;
		DBGL(DL_CNST, (log(LL_DBG, "set_controller_state: controller [%d] set DOWN!", controller)));
	}
	else
	{
		log(LL_ERR, "set_controller_state: invalid controller state [%d]!", state);
		return(ERROR);
	}
	return(GOOD);
}		
	
/*--------------------------------------------------------------------------*
 *	get controller state
 *--------------------------------------------------------------------------*/
int
get_controller_state(int controller)
{
	if((controller < 0) || (controller >= ncontroller))
	{
		log(LL_ERR, "set_controller_state: invalid controller number [%d]!", controller);
		return(ERROR);
	}
	return(isdn_ctrl_tab[controller].state);
}		

/*--------------------------------------------------------------------------*
 *	decrement number of free channels for controller
 *--------------------------------------------------------------------------*/
int
decr_free_channels(int controller)
{
	if((controller < 0) || (controller >= ncontroller))
	{
		log(LL_ERR, "decr_free_channels: invalid controller number [%d]!", controller);
		return(ERROR);
	}
	if(isdn_ctrl_tab[controller].freechans > 0)
	{
		(isdn_ctrl_tab[controller].freechans)--;
		DBGL(DL_CNST, (log(LL_DBG, "decr_free_channels: ctrl %d, now %d chan free", controller, isdn_ctrl_tab[controller].freechans)));
		return(GOOD);
	}
	else
	{
		log(LL_ERR, "decr_free_channels: controller [%d] already 0 free chans!", controller);
		return(ERROR);
	}
}		
	
/*--------------------------------------------------------------------------*
 *	increment number of free channels for controller
 *--------------------------------------------------------------------------*/
int
incr_free_channels(int controller)
{
	if((controller < 0) || (controller >= ncontroller))
	{
		log(LL_ERR, "incr_free_channels: invalid controller number [%d]!", controller);
		return(ERROR);
	}
	if(isdn_ctrl_tab[controller].freechans < MAX_CHANCTRL)
	{
		(isdn_ctrl_tab[controller].freechans)++;
		DBGL(DL_CNST, (log(LL_DBG, "incr_free_channels: ctrl %d, now %d chan free", controller, isdn_ctrl_tab[controller].freechans)));
		return(GOOD);
	}
	else
	{
		log(LL_ERR, "incr_free_channels: controller [%d] already 2 free chans!", controller);
		return(ERROR);
	}
}		
	
/*--------------------------------------------------------------------------*
 *	get number of free channels for controller
 *--------------------------------------------------------------------------*/
int
get_free_channels(int controller)
{
	if((controller < 0) || (controller >= ncontroller))
	{
		log(LL_ERR, "get_free_channels: invalid controller number [%d]!", controller);
		return(ERROR);
	}
	DBGL(DL_CNST, (log(LL_DBG, "get_free_channels: ctrl %d, %d chan free", controller, isdn_ctrl_tab[controller].freechans)));
	return(isdn_ctrl_tab[controller].freechans);
}		
	
/*--------------------------------------------------------------------------*
 *	set channel state to busy
 *--------------------------------------------------------------------------*/
int
set_channel_busy(int controller, int channel)
{
	if((controller < 0) || (controller >= ncontroller))
	{
		log(LL_ERR, "set_channel_busy: invalid controller number [%d]!", controller);
		return(ERROR);
	}
		
	switch(channel)
	{
		case CHAN_B1:
			if(isdn_ctrl_tab[controller].stateb1 == CHAN_RUN)
			{
				DBGL(DL_CNST, (log(LL_DBG, "set_channel_busy: controller [%d] channel B1 already busy!", controller)));
			}
			else
			{
				isdn_ctrl_tab[controller].stateb1 = CHAN_RUN;
				DBGL(DL_CNST, (log(LL_DBG, "set_channel_busy: controller [%d] channel B1 set to BUSY!", controller)));
			}
			break;

		case CHAN_B2:
			if(isdn_ctrl_tab[controller].stateb2 == CHAN_RUN)
			{
				DBGL(DL_CNST, (log(LL_DBG, "set_channel_busy: controller [%d] channel B2 already busy!", controller)));
			}
			else
			{
				isdn_ctrl_tab[controller].stateb2 = CHAN_RUN;
				DBGL(DL_CNST, (log(LL_DBG, "set_channel_busy: controller [%d] channel B2 set to BUSY!", controller)));
			}
			break;

		default:
			log(LL_ERR, "set_channel_busy: controller [%d], invalid channel [%d]!", controller, channel);
			return(ERROR);
			break;
	}
	return(GOOD);
}

/*--------------------------------------------------------------------------*
 *	set channel state to idle
 *--------------------------------------------------------------------------*/
int
set_channel_idle(int controller, int channel)
{
	if((controller < 0) || (controller >= ncontroller))
	{
		log(LL_ERR, "set_channel_idle: invalid controller number [%d]!", controller);
		return(ERROR);
	}
		
	switch(channel)
	{
		case CHAN_B1:
			if(isdn_ctrl_tab[controller].stateb1 == CHAN_IDLE)
			{
				DBGL(DL_CNST, (log(LL_DBG, "set_channel_idle: controller [%d] channel B1 already idle!", controller)));
			}
			else
			{
				isdn_ctrl_tab[controller].stateb1 = CHAN_IDLE;
				DBGL(DL_CNST, (log(LL_DBG, "set_channel_idle: controller [%d] channel B1 set to IDLE!", controller)));
			}
			break;

		case CHAN_B2:
			if(isdn_ctrl_tab[controller].stateb2 == CHAN_IDLE)
			{
				DBGL(DL_CNST, (log(LL_DBG, "set_channel_idle: controller [%d] channel B2 already idle!", controller)));
			}
			else
			{
				isdn_ctrl_tab[controller].stateb2 = CHAN_IDLE;
				DBGL(DL_CNST, (log(LL_DBG, "set_channel_idle: controller [%d] channel B2 set to IDLE!", controller)));
			}
			break;

		default:
			DBGL(DL_CNST, (log(LL_DBG, "set_channel_idle: controller [%d], invalid channel [%d]!", controller, channel)));
			return(ERROR);
			break;
	}
	return(GOOD);
}

/*--------------------------------------------------------------------------*
 *	return channel state
 *--------------------------------------------------------------------------*/
int
ret_channel_state(int controller, int channel)
{
	if((controller < 0) || (controller >= ncontroller))
	{
		log(LL_ERR, "ret_channel_state: invalid controller number [%d]!", controller);
		return(ERROR);
	}
		
	switch(channel)
	{
		case CHAN_B1:
			return(isdn_ctrl_tab[controller].stateb1);
			break;

		case CHAN_B2:
			return(isdn_ctrl_tab[controller].stateb2);
			break;

		default:
			log(LL_ERR, "ret_channel_state: controller [%d], invalid channel [%d]!", controller, channel);
			return(ERROR);
			break;
	}
	return(ERROR);
}

/* EOF */
