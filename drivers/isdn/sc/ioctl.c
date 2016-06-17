/*
 * Copyright (C) 1996  SpellCaster Telecommunications Inc.
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#define __NO_VERSION__
#include "includes.h"
#include "hardware.h"
#include "message.h"
#include "card.h"
#include "scioc.h"

extern int indicate_status(int, int, unsigned long, char *);
extern int startproc(int);
extern int loadproc(int, char *record);
extern int reset(int);
extern int send_and_receive(int, unsigned int, unsigned char,unsigned char,
		unsigned char,unsigned char, 
		unsigned char, unsigned char *, RspMessage *, int);

extern board *adapter[];


int GetStatus(int card, boardInfo *);

/*
 * Process private IOCTL messages (typically from scctrl)
 */
int sc_ioctl(int card, scs_ioctl *data)
{
	switch(data->command) {
	case SCIOCRESET:	/* Perform a hard reset of the adapter */
	{
		pr_debug("%s: SCIOCRESET: ioctl received\n", adapter[card]->devicename);
		adapter[card]->StartOnReset = 0;
		return (reset(card));
	}

	case SCIOCLOAD:
	{
		RspMessage	rcvmsg;
		char		srec[SCIOC_SRECSIZE];
		int		status, err;

		pr_debug("%s: SCIOLOAD: ioctl received\n", adapter[card]->devicename);
		if(adapter[card]->EngineUp) {
			pr_debug("%s: SCIOCLOAD: command failed, LoadProc while engine running.\n",
				adapter[card]->devicename);
			return -1;
		}

		/*
		 * Get the SRec from user space
		 */
		if ((err = copy_from_user(srec, (char *) data->dataptr, sizeof(srec))))
			return err;

		status = send_and_receive(card, CMPID, cmReqType2, cmReqClass0, cmReqLoadProc,
				0, sizeof(srec), srec, &rcvmsg, SAR_TIMEOUT);
		if(status) {
			pr_debug("%s: SCIOCLOAD: command failed, status = %d\n", 
				adapter[card]->devicename, status);
			return -1;
		}
		else {
			pr_debug("%s: SCIOCLOAD: command successful\n", adapter[card]->devicename);
			return 0;
		}
	}

	case SCIOCSTART:
	{
		pr_debug("%s: SCIOSTART: ioctl received\n", adapter[card]->devicename);
		if(adapter[card]->EngineUp) {
			pr_debug("%s: SCIOCSTART: command failed, engine already running.\n",
				adapter[card]->devicename);
			return -1;
		}

		adapter[card]->StartOnReset = 1;
		startproc(card);
		return 0;
	}

	case SCIOCSETSWITCH:
	{
		RspMessage	rcvmsg;
		char		switchtype;
		int 		status, err;

		pr_debug("%s: SCIOSETSWITCH: ioctl received\n", adapter[card]->devicename);

		/*
		 * Get the switch type from user space
		 */
		if ((err = copy_from_user(&switchtype, (char *) data->dataptr, sizeof(char))))
			return err;

		pr_debug("%s: SCIOCSETSWITCH: setting switch type to %d\n", adapter[card]->devicename,
			switchtype);
		status = send_and_receive(card, CEPID, ceReqTypeCall, ceReqClass0, ceReqCallSetSwitchType,
						0, sizeof(char),&switchtype,&rcvmsg, SAR_TIMEOUT);
		if(!status && !rcvmsg.rsp_status) {
			pr_debug("%s: SCIOCSETSWITCH: command successful\n", adapter[card]->devicename);
			return 0;
		}
		else {
			pr_debug("%s: SCIOCSETSWITCH: command failed (status = %d)\n",
				adapter[card]->devicename, status);
			return status;
		}
	}
		
	case SCIOCGETSWITCH:
	{
		RspMessage 	rcvmsg;
		char		switchtype;
		int		status, err;

		pr_debug("%s: SCIOGETSWITCH: ioctl received\n", adapter[card]->devicename);

		/*
		 * Get the switch type from the board
		 */
		status = send_and_receive(card, CEPID, ceReqTypeCall, ceReqClass0, 
			ceReqCallGetSwitchType, 0, 0, 0, &rcvmsg, SAR_TIMEOUT);
		if (!status && !rcvmsg.rsp_status) {
			pr_debug("%s: SCIOCGETSWITCH: command successful\n", adapter[card]->devicename);
		}
		else {
			pr_debug("%s: SCIOCGETSWITCH: command failed (status = %d)\n",
				adapter[card]->devicename, status);
			return status;
		}

		switchtype = rcvmsg.msg_data.byte_array[0];

		/*
		 * Package the switch type and send to user space
		 */
		if ((err = copy_to_user((char *) data->dataptr, &switchtype, sizeof(char))))
			return err;

		return 0;
	}

	case SCIOCGETSPID:
	{
		RspMessage	rcvmsg;
		char		spid[SCIOC_SPIDSIZE];
		int		status, err;

		pr_debug("%s: SCIOGETSPID: ioctl received\n", adapter[card]->devicename);

		/*
		 * Get the spid from the board
		 */
		status = send_and_receive(card, CEPID, ceReqTypeCall, ceReqClass0, ceReqCallGetSPID,
					data->channel, 0, 0, &rcvmsg, SAR_TIMEOUT);
		if (!status) {
			pr_debug("%s: SCIOCGETSPID: command successful\n", adapter[card]->devicename);
		}
		else {
			pr_debug("%s: SCIOCGETSPID: command failed (status = %d)\n",
				adapter[card]->devicename, status);
			return status;
		}
		strcpy(spid, rcvmsg.msg_data.byte_array);

		/*
		 * Package the switch type and send to user space
		 */
		if ((err = copy_to_user((char *) data->dataptr, spid, sizeof(spid))))
			return err;

		return 0;
	}	

	case SCIOCSETSPID:
	{
		RspMessage	rcvmsg;
		char		spid[SCIOC_SPIDSIZE];
		int 		status, err;

		pr_debug("%s: DCBIOSETSPID: ioctl received\n", adapter[card]->devicename);

		/*
		 * Get the spid from user space
		 */
		if ((err = copy_from_user(spid, (char *) data->dataptr, sizeof(spid))))
			return err;

		pr_debug("%s: SCIOCSETSPID: setting channel %d spid to %s\n", 
			adapter[card]->devicename, data->channel, spid);
		status = send_and_receive(card, CEPID, ceReqTypeCall, 
			ceReqClass0, ceReqCallSetSPID, data->channel, 
			strlen(spid), spid, &rcvmsg, SAR_TIMEOUT);
		if(!status && !rcvmsg.rsp_status) {
			pr_debug("%s: SCIOCSETSPID: command successful\n", 
				adapter[card]->devicename);
			return 0;
		}
		else {
			pr_debug("%s: SCIOCSETSPID: command failed (status = %d)\n",
				adapter[card]->devicename, status);
			return status;
		}
	}

	case SCIOCGETDN:
	{
		RspMessage	rcvmsg;
		char		dn[SCIOC_DNSIZE];
		int		status, err;

		pr_debug("%s: SCIOGETDN: ioctl received\n", adapter[card]->devicename);

		/*
		 * Get the dn from the board
		 */
		status = send_and_receive(card, CEPID, ceReqTypeCall, ceReqClass0, ceReqCallGetMyNumber,
					data->channel, 0, 0, &rcvmsg, SAR_TIMEOUT);
		if (!status) {
			pr_debug("%s: SCIOCGETDN: command successful\n", adapter[card]->devicename);
		}
		else {
			pr_debug("%s: SCIOCGETDN: command failed (status = %d)\n",
				adapter[card]->devicename, status);
			return status;
		}

		strcpy(dn, rcvmsg.msg_data.byte_array);

		/*
		 * Package the dn and send to user space
		 */
		if ((err = copy_to_user((char *) data->dataptr, dn, sizeof(dn))))
			return err;

		return 0;
	}	

	case SCIOCSETDN:
	{
		RspMessage	rcvmsg;
		char		dn[SCIOC_DNSIZE];
		int 		status, err;

		pr_debug("%s: SCIOSETDN: ioctl received\n", adapter[card]->devicename);

		/*
		 * Get the spid from user space
		 */
		if ((err = copy_from_user(dn, (char *) data->dataptr, sizeof(dn))))
			return err;

		pr_debug("%s: SCIOCSETDN: setting channel %d dn to %s\n", 
			adapter[card]->devicename, data->channel, dn);
		status = send_and_receive(card, CEPID, ceReqTypeCall, 
			ceReqClass0, ceReqCallSetMyNumber, data->channel, 
			strlen(dn),dn,&rcvmsg, SAR_TIMEOUT);
		if(!status && !rcvmsg.rsp_status) {
			pr_debug("%s: SCIOCSETDN: command successful\n", 
				adapter[card]->devicename);
			return 0;
		}
		else {
			pr_debug("%s: SCIOCSETDN: command failed (status = %d)\n",
				adapter[card]->devicename, status);
			return status;
		}
	}

	case SCIOCTRACE:

		pr_debug("%s: SCIOTRACE: ioctl received\n", adapter[card]->devicename);
/*		adapter[card]->trace = !adapter[card]->trace; 
		pr_debug("%s: SCIOCTRACE: tracing turned %s\n", adapter[card]->devicename,
			adapter[card]->trace ? "ON" : "OFF"); */
		break;

	case SCIOCSTAT:
	{
		boardInfo bi;
		int err;

		pr_debug("%s: SCIOSTAT: ioctl received\n", adapter[card]->devicename);
		GetStatus(card, &bi);
		
		if ((err = copy_to_user((boardInfo *) data->dataptr, &bi, sizeof(boardInfo))))
			return err;

		return 0;
	}

	case SCIOCGETSPEED:
	{
		RspMessage	rcvmsg;
		char		speed;
		int		status, err;

		pr_debug("%s: SCIOGETSPEED: ioctl received\n", adapter[card]->devicename);

		/*
		 * Get the speed from the board
		 */
		status = send_and_receive(card, CEPID, ceReqTypeCall, ceReqClass0, 
			ceReqCallGetCallType, data->channel, 0, 0, &rcvmsg, SAR_TIMEOUT);
		if (!status && !rcvmsg.rsp_status) {
			pr_debug("%s: SCIOCGETSPEED: command successful\n",
				adapter[card]->devicename);
		}
		else {
			pr_debug("%s: SCIOCGETSPEED: command failed (status = %d)\n",
				adapter[card]->devicename, status);
			return status;
		}

		speed = rcvmsg.msg_data.byte_array[0];

		/*
		 * Package the switch type and send to user space
		 */
		if ((err = copy_to_user((char *) data->dataptr, &speed, sizeof(char))))
			return err;

		return 0;
	}

	case SCIOCSETSPEED:
		pr_debug("%s: SCIOCSETSPEED: ioctl received\n", adapter[card]->devicename);
		break;

	case SCIOCLOOPTST:
		pr_debug("%s: SCIOCLOOPTST: ioctl received\n", adapter[card]->devicename);
		break;

	default:
		return -1;
	}

	return 0;
}

int GetStatus(int card, boardInfo *bi)
{
	RspMessage rcvmsg;
	int i, status;

	/*
	 * Fill in some of the basic info about the board
	 */
	bi->modelid = adapter[card]->model;
	strcpy(bi->serial_no, adapter[card]->hwconfig.serial_no);
	strcpy(bi->part_no, adapter[card]->hwconfig.part_no);
	bi->iobase = adapter[card]->iobase;
	bi->rambase = adapter[card]->rambase;
	bi->irq = adapter[card]->interrupt;
	bi->ramsize = adapter[card]->hwconfig.ram_size;
	bi->interface = adapter[card]->hwconfig.st_u_sense;
	strcpy(bi->load_ver, adapter[card]->load_ver);
	strcpy(bi->proc_ver, adapter[card]->proc_ver);

	/*
	 * Get the current PhyStats and LnkStats
	 */
	status = send_and_receive(card, CEPID, ceReqTypePhy, ceReqClass2,
		ceReqPhyStatus, 0, 0, NULL, &rcvmsg, SAR_TIMEOUT);
	if(!status) {
		if(adapter[card]->model < PRI_BOARD) {
			bi->l1_status = rcvmsg.msg_data.byte_array[2];
			for(i = 0 ; i < BRI_CHANNELS ; i++)
				bi->status.bristats[i].phy_stat =
					rcvmsg.msg_data.byte_array[i];
		}
		else {
			bi->l1_status = rcvmsg.msg_data.byte_array[0];
			bi->l2_status = rcvmsg.msg_data.byte_array[1];
			for(i = 0 ; i < PRI_CHANNELS ; i++)
				bi->status.pristats[i].phy_stat = 
					rcvmsg.msg_data.byte_array[i+2];
		}
	}
	
	/*
	 * Get the call types for each channel
	 */
	for (i = 0 ; i < adapter[card]->nChannels ; i++) {
		status = send_and_receive(card, CEPID, ceReqTypeCall, ceReqClass0,
			ceReqCallGetCallType, 0, 0, NULL, &rcvmsg, SAR_TIMEOUT);
		if(!status) {
			if (adapter[card]->model == PRI_BOARD) {
				bi->status.pristats[i].call_type = 
					rcvmsg.msg_data.byte_array[0];
			}
			else {
				bi->status.bristats[i].call_type =
					rcvmsg.msg_data.byte_array[0];
			}
		}
	}
	
	/*
	 * If PRI, get the call states and service states for each channel
	 */
	if (adapter[card]->model == PRI_BOARD) {
		/*
		 * Get the call states
		 */
		status = send_and_receive(card, CEPID, ceReqTypeStat, ceReqClass2,
			ceReqPhyChCallState, 0, 0, NULL, &rcvmsg, SAR_TIMEOUT);
		if(!status) {
			for( i = 0 ; i < PRI_CHANNELS ; i++ )
				bi->status.pristats[i].call_state = 
					rcvmsg.msg_data.byte_array[i];
		}

		/*
		 * Get the service states
		 */
		status = send_and_receive(card, CEPID, ceReqTypeStat, ceReqClass2,
			ceReqPhyChServState, 0, 0, NULL, &rcvmsg, SAR_TIMEOUT);
		if(!status) {
			for( i = 0 ; i < PRI_CHANNELS ; i++ )
				bi->status.pristats[i].serv_state = 
					rcvmsg.msg_data.byte_array[i];
		}

		/*
		 * Get the link stats for the channels
		 */
		for (i = 1 ; i <= PRI_CHANNELS ; i++) {
			status = send_and_receive(card, CEPID, ceReqTypeLnk, ceReqClass0,
				ceReqLnkGetStats, i, 0, NULL, &rcvmsg, SAR_TIMEOUT);
			if (!status) {
				bi->status.pristats[i-1].link_stats.tx_good =
					(unsigned long)rcvmsg.msg_data.byte_array[0];
				bi->status.pristats[i-1].link_stats.tx_bad =
					(unsigned long)rcvmsg.msg_data.byte_array[4];
				bi->status.pristats[i-1].link_stats.rx_good =
					(unsigned long)rcvmsg.msg_data.byte_array[8];
				bi->status.pristats[i-1].link_stats.rx_bad =
					(unsigned long)rcvmsg.msg_data.byte_array[12];
			}
		}

		/*
		 * Link stats for the D channel
		 */
		status = send_and_receive(card, CEPID, ceReqTypeLnk, ceReqClass0,
			ceReqLnkGetStats, 0, 0, NULL, &rcvmsg, SAR_TIMEOUT);
		if (!status) {
			bi->dch_stats.tx_good = (unsigned long)rcvmsg.msg_data.byte_array[0];
			bi->dch_stats.tx_bad = (unsigned long)rcvmsg.msg_data.byte_array[4];
			bi->dch_stats.rx_good = (unsigned long)rcvmsg.msg_data.byte_array[8];
			bi->dch_stats.rx_bad = (unsigned long)rcvmsg.msg_data.byte_array[12];
		}

		return 0;
	}

	/*
	 * If BRI or POTS, Get SPID, DN and call types for each channel
	 */

	/*
	 * Get the link stats for the channels
	 */
	status = send_and_receive(card, CEPID, ceReqTypeLnk, ceReqClass0,
		ceReqLnkGetStats, 0, 0, NULL, &rcvmsg, SAR_TIMEOUT);
	if (!status) {
		bi->dch_stats.tx_good = (unsigned long)rcvmsg.msg_data.byte_array[0];
		bi->dch_stats.tx_bad = (unsigned long)rcvmsg.msg_data.byte_array[4];
		bi->dch_stats.rx_good = (unsigned long)rcvmsg.msg_data.byte_array[8];
		bi->dch_stats.rx_bad = (unsigned long)rcvmsg.msg_data.byte_array[12];
		bi->status.bristats[0].link_stats.tx_good = 
			(unsigned long)rcvmsg.msg_data.byte_array[16];
		bi->status.bristats[0].link_stats.tx_bad = 
			(unsigned long)rcvmsg.msg_data.byte_array[20];
		bi->status.bristats[0].link_stats.rx_good = 
			(unsigned long)rcvmsg.msg_data.byte_array[24];
		bi->status.bristats[0].link_stats.rx_bad = 
			(unsigned long)rcvmsg.msg_data.byte_array[28];
		bi->status.bristats[1].link_stats.tx_good = 
			(unsigned long)rcvmsg.msg_data.byte_array[32];
		bi->status.bristats[1].link_stats.tx_bad = 
			(unsigned long)rcvmsg.msg_data.byte_array[36];
		bi->status.bristats[1].link_stats.rx_good = 
			(unsigned long)rcvmsg.msg_data.byte_array[40];
		bi->status.bristats[1].link_stats.rx_bad = 
			(unsigned long)rcvmsg.msg_data.byte_array[44];
	}

	/*
	 * Get the SPIDs
	 */
	for (i = 0 ; i < BRI_CHANNELS ; i++) {
		status = send_and_receive(card, CEPID, ceReqTypeCall, ceReqClass0,
			ceReqCallGetSPID, i+1, 0, NULL, &rcvmsg, SAR_TIMEOUT);
		if (!status)
			strcpy(bi->status.bristats[i].spid, rcvmsg.msg_data.byte_array);
	}
		
	/*
	 * Get the DNs
	 */
	for (i = 0 ; i < BRI_CHANNELS ; i++) {
		status = send_and_receive(card, CEPID, ceReqTypeCall, ceReqClass0,
			ceReqCallGetMyNumber, i+1, 0, NULL, &rcvmsg, SAR_TIMEOUT);
		if (!status)
			strcpy(bi->status.bristats[i].dn, rcvmsg.msg_data.byte_array);
	}
		
	return 0;
}
