/*
 * Copyright (c) 1997, 2001 Hellmuth Michaelis. All rights reserved.
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
 *      last edit-date: [Sun Oct 21 11:02:15 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/types.h>
#include <sys/mman.h>

#include "isdnd.h"

static int
init_controller_state(int controller, int ctrl_type, int card_type, int tei, int nbch);

/*---------------------------------------------------------------------------*
 *	get name of a controller
 *---------------------------------------------------------------------------*/
const char *
name_of_controller(int ctrl_type, int card_type)
{
	static char *passive_card[] = {
		"Teles S0/8",
		"Teles S0/16",
		"Teles S0/16.3",
		"AVM A1 or Fritz!Card",
		"Teles S0/16.3 PnP",
		"Creatix S0 PnP",
		"USRobotics Sportster ISDN TA",
		"Dr. Neuhaus NICCY Go@",
		"Sedlbauer win speed",
 		"Dynalink IS64PH",
		"ISDN Master, MasterII or Blaster",
		"AVM PCMCIA Fritz!Card",
		"ELSA QuickStep 1000pro/ISA",
		"ELSA QuickStep 1000pro/PCI",
		"Siemens I-Talk",
		"ELSA MicroLink ISDN/MC",
		"ELSA MicroLink MCall",
 		"ITK ix1 micro",
		"AVM Fritz!Card PCI",
		"ELSA PCC-16",
		"AVM Fritz!Card PnP",		
		"Siemens I-Surf 2.0 PnP",		
 		"Asuscom ISDNlink 128K PnP",
 		"ASUSCOM P-IN100-ST-D (Winbond W6692)",
		"Teles S0/16.3c PnP",
		"AcerISDN P10 PnP",
		"TELEINT ISDN SPEED No. 1",
		"Cologne Chip HFC-S PCI based",
		"Traverse Tech NETjet-S / Teles PCI-TJ",
		"Eicon.Diehl DIVA 2.0 / 2.02 ISA PnP",
		"Compaq Microcom 610",
	};

	static char *daic_card[] = {
		"EICON.Diehl S",
		"EICON.Diehl SX/SXn",
		"EICON.Diehl SCOM",
		"EICON.Diehl QUADRO",
	};

	static char *capi_card[] = {
   	        "AVM T1 PCI",
		"AVM B1 PCI",
		"AVM B1 ISA",
	};

	if(ctrl_type == CTRL_PASSIVE)
	{
		int index = card_type - CARD_TYPEP_8;
		if (index >= 0 && index < (sizeof passive_card / sizeof passive_card[0]))
			return passive_card[index];
	}
	else if(ctrl_type == CTRL_DAIC)
	{
		int index = card_type - CARD_TYPEA_DAIC_S;
		if (index >= 0 && index < (sizeof daic_card / sizeof daic_card[0] ))
			return daic_card[index];
	}
	else if(ctrl_type == CTRL_TINADD)
	{
		return "Stollmann tina-dd";
	}
	else if(ctrl_type == CTRL_CAPI)
	{
		int index = card_type - CARD_TYPEC_AVM_T1_PCI;
		if (index >= 0 && index < (sizeof capi_card / sizeof capi_card[0] ))
			return capi_card[index];
	}

	return "unknown card type";
}
 
/*---------------------------------------------------------------------------*
 *	init controller state array
 *---------------------------------------------------------------------------*/
void
init_controller(void)
{
	int i;
	int max = 1;
	msg_ctrl_info_req_t mcir;
	
	for(i=0; i < max; i++)
	{
		mcir.controller = i;
		
		if((ioctl(isdnfd, I4B_CTRL_INFO_REQ, &mcir)) < 0)
		{
			log(LL_ERR, "init_controller: ioctl I4B_CTRL_INFO_REQ failed: %s", strerror(errno));
			do_exit(1);
		}

		if((ncontroller = max = mcir.ncontroller) == 0)
		{
			log(LL_ERR, "init_controller: no ISDN controller found!");
			do_exit(1);
		}

		if(mcir.ctrl_type == -1 || mcir.card_type == -1)
		{
			log(LL_ERR, "init_controller: ctrl/card is invalid!");
			do_exit(1);
		}

		/* init controller tab */

		if((init_controller_state(i, mcir.ctrl_type, mcir.card_type, mcir.tei, mcir.nbch)) == ERROR)
		{
			log(LL_ERR, "init_controller: init_controller_state for controller %d failed", i);
			do_exit(1);
		}
	}
	DBGL(DL_RCCF, (log(LL_DBG, "init_controller: found %d ISDN controller(s)", max)));
}

/*--------------------------------------------------------------------------*
 *	init controller state table entry
 *--------------------------------------------------------------------------*/
static int
init_controller_state(int controller, int ctrl_type, int card_type, int tei,
		      int nbch)
{
        int i;

	if((controller < 0) || (controller >= ncontroller))
	{
		log(LL_ERR, "init_controller_state: invalid controller number [%d]!", controller);
		return(ERROR);
	}
	
	/* init controller tab */
		
	switch (ctrl_type) {
	case CTRL_PASSIVE:
		if((card_type > CARD_TYPEP_UNK) &&
		   (card_type <= CARD_TYPEP_MAX))
		{
			isdn_ctrl_tab[controller].ctrl_type = ctrl_type;
			isdn_ctrl_tab[controller].card_type = card_type;
			isdn_ctrl_tab[controller].state = CTRL_UP;
		}
		else
		{
			log(LL_ERR, "init_controller_state: unknown card type %d", card_type);
			return(ERROR);
		}
		break;
		
	case CTRL_DAIC:
		isdn_ctrl_tab[controller].ctrl_type = ctrl_type;
		isdn_ctrl_tab[controller].card_type = card_type;
		isdn_ctrl_tab[controller].state = CTRL_DOWN;
		break;

	case CTRL_TINADD:
		isdn_ctrl_tab[controller].ctrl_type = ctrl_type;
		isdn_ctrl_tab[controller].card_type = 0;
		isdn_ctrl_tab[controller].state = CTRL_DOWN;
		break;

	case CTRL_CAPI:
		isdn_ctrl_tab[controller].ctrl_type = ctrl_type;
		isdn_ctrl_tab[controller].card_type = card_type;
		isdn_ctrl_tab[controller].state = CTRL_UP;
		break;

	default:
		log(LL_ERR, "init_controller_state: unknown controller type %d", ctrl_type);
		return(ERROR);
	}
	
	isdn_ctrl_tab[controller].nbch = nbch;
	isdn_ctrl_tab[controller].freechans = nbch;
	for (i = 0; i < nbch; i++)
	    isdn_ctrl_tab[controller].stateb[i] = CHAN_IDLE;
		isdn_ctrl_tab[controller].tei = tei;	
		isdn_ctrl_tab[controller].l1stat = LAYER_IDLE;
		isdn_ctrl_tab[controller].l2stat = LAYER_IDLE;

		log(LL_DMN, "init_controller_state: controller %d is %s",
		  controller,
		  name_of_controller(isdn_ctrl_tab[controller].ctrl_type,
				     isdn_ctrl_tab[controller].card_type));
		
	return(GOOD);
}	

/*--------------------------------------------------------------------------*
 *	init active or capi controller
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
			
			snprintf(cmdbuf, sizeof(cmdbuf), "%s %d", tinainitprog, unit);

			if((ret = system(cmdbuf)) != 0)
			{
				log(LL_ERR, "init_active_controller, tina-dd %d: %s returned %d!", unit, tinainitprog, ret);
				do_exit(1);
			}
		}

		/*
		 *  Generic microcode loading. If a controller has
		 *  defined a microcode file, load it using the
		 *  I4B_CTRL_DOWNLOAD ioctl.
		 */
		
		if(isdn_ctrl_tab[controller].firmware != NULL)
		{
		    int fd, ret;
		    struct isdn_dr_prot idp;
		    struct isdn_download_request idr;

		    fd = open(isdn_ctrl_tab[controller].firmware, O_RDONLY);
		    if (fd < 0) {
			log(LL_ERR, "init_active_controller %d: open %s: %s!",
			    controller, isdn_ctrl_tab[controller].firmware,
			    strerror(errno));
			do_exit(1);
		    }

		    idp.bytecount = lseek(fd, 0, SEEK_END);
		    idp.microcode = mmap(0, idp.bytecount, PROT_READ,
					 MAP_SHARED, fd, 0);
		    if (idp.microcode == MAP_FAILED) {
			log(LL_ERR, "init_active_controller %d: mmap %s: %s!",
			    controller, isdn_ctrl_tab[controller].firmware,
			    strerror(errno));
			do_exit(1);
		    }
		    
		    DBGL(DL_RCCF, (log(LL_DBG, "init_active_controller %d: loading firmware from [%s]", controller, isdn_ctrl_tab[controller].firmware)));

		    idr.controller = controller;
		    idr.numprotos = 1;
		    idr.protocols = &idp;
		    
		    ret = ioctl(isdnfd, I4B_CTRL_DOWNLOAD, &idr, sizeof(idr));
		    if (ret) {
			log(LL_ERR, "init_active_controller %d: load %s: %s!",
			    controller, isdn_ctrl_tab[controller].firmware,
			    strerror(errno));
			do_exit(1);
		    }

		    munmap(idp.microcode, idp.bytecount);
		    close(fd);
		}
	}
}	

/*--------------------------------------------------------------------------*
 *	init controller D-channel ISDN protocol
 *--------------------------------------------------------------------------*/
void
init_controller_protocol(void)
{
	int controller;
	msg_prot_ind_t mpi;

	for(controller = 0; controller < ncontroller; controller++)
	{
		mpi.controller = controller;
		mpi.protocol = isdn_ctrl_tab[controller].protocol;
		
		if((ioctl(isdnfd, I4B_PROT_IND, &mpi)) < 0)
		{
			log(LL_ERR, "init_controller_protocol: ioctl I4B_PROT_IND failed: %s", strerror(errno));
			do_exit(1);
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
	if(isdn_ctrl_tab[controller].freechans < isdn_ctrl_tab[controller].nbch)
	{
		(isdn_ctrl_tab[controller].freechans)++;
		DBGL(DL_CNST, (log(LL_DBG, "incr_free_channels: ctrl %d, now %d chan free", controller, isdn_ctrl_tab[controller].freechans)));
		return(GOOD);
	}
	else
	{
		log(LL_ERR, "incr_free_channels: controller [%d] already %d free chans!", controller, isdn_ctrl_tab[controller].nbch);
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
		
	if ((channel < 0) || (channel >= isdn_ctrl_tab[controller].nbch))
			{
		log(LL_ERR, "set_channel_busy: controller [%d] invalid channel [%d]!", controller, channel);
		return(ERROR);
			}

	if(isdn_ctrl_tab[controller].stateb[channel] == CHAN_RUN)
			{
	    DBGL(DL_CNST, (log(LL_DBG, "set_channel_busy: controller [%d] channel B%d already busy!", controller, channel+1)));
			}
			else
			{
	    isdn_ctrl_tab[controller].stateb[channel] = CHAN_RUN;
	    DBGL(DL_CNST, (log(LL_DBG, "set_channel_busy: controller [%d] channel B%d set to BUSY!", controller, channel+1)));
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
		
	if ((channel < 0) || (channel >= isdn_ctrl_tab[controller].nbch))
			{
		log(LL_ERR, "set_channel_busy: controller [%d] invalid channel [%d]!", controller, channel);
		return(ERROR);
			}

	if (isdn_ctrl_tab[controller].stateb[channel] == CHAN_IDLE)
			{
	    DBGL(DL_CNST, (log(LL_DBG, "set_channel_idle: controller [%d] channel B%d already idle!", controller, channel+1)));
			}
			else
			{
	    isdn_ctrl_tab[controller].stateb[channel] = CHAN_IDLE;
	    DBGL(DL_CNST, (log(LL_DBG, "set_channel_idle: controller [%d] channel B%d set to IDLE!", controller, channel+1)));
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
		
	if ((channel < 0) || (channel >= isdn_ctrl_tab[controller].nbch))
	{
		log(LL_ERR, "set_channel_busy: controller [%d] invalid channel [%d]!", controller, channel);
			return(ERROR);
	}

	return(isdn_ctrl_tab[controller].stateb[channel]);
}

/* EOF */
