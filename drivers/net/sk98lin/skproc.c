/******************************************************************************
 *
 * Name:	skproc.c
 * Project:	GEnesis, PCI Gigabit Ethernet Adapter
 * Purpose:	Funktions to display statictic data
 *
 ******************************************************************************/
 
/******************************************************************************
 *
 *	(C)Copyright 1998-2002 SysKonnect GmbH.
 *	(C)Copyright 2002-2003 Marvell.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Created 22-Nov-2000
 *	Author: Mirko Lindner (mlindner@syskonnect.de)
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/

#include <linux/proc_fs.h>

#include "h/skdrv1st.h"
#include "h/skdrv2nd.h"
#include "h/skversion.h"

extern struct SK_NET_DEVICE *SkGeRootDev;
static int sk_proc_print(void *writePtr, char *format, ...);
static void sk_gen_browse(void *buffer);
int len;

struct proc_dir_entry *file = NULL;

/*****************************************************************************
 *
 *      sk_proc_read - show proc information of a particular adapter
 *
 * Description:
 *  This function fills the proc entry with statistic data about 
 *  the ethernet device. It invokes the generic sk_gen_browse() to
 *  print out all items one per one.
 *  
 * Returns: number of bytes written
 *      
 */
int sk_proc_read(char   *buffer,
		char  **buffer_location,
		off_t   offset,
		int     buffer_length,
		int    *eof,
		void   *data)
{
	void *castedBuffer = (void *) buffer;
	file               = (struct proc_dir_entry*) data;
	len                = 0; /* initial value */
	sk_gen_browse(castedBuffer);

	if (offset >= len) {
		*eof = 1;
		return 0;
	}

	*buffer_location = buffer + offset;
	if (buffer_length >= len - offset) {
		*eof = 1;
	}
	return (min_t(int, buffer_length, len - offset));
}

/*****************************************************************************
 *
 * 	sk_gen_browse -generic  print "summaries" entry 
 *
 * Description:
 *  This function fills the proc entry with statistic data about 
 *  the ethernet device.
 *  
 * Returns: -
 *	
 */
static void sk_gen_browse(void *buffer)
{
	struct SK_NET_DEVICE	*SkgeProcDev = SkGeRootDev;
	struct SK_NET_DEVICE	*next;
	SK_PNMI_STRUCT_DATA 	*pPnmiStruct;
	SK_PNMI_STAT		*pPnmiStat;
	unsigned long		Flags;	
	unsigned int		Size;
	DEV_NET			*pNet;
	SK_AC			*pAC;
	char			sens_msg[50];
	int			MaxSecurityCount = 0;
	int 			t;
	int 			i;

	while (SkgeProcDev) {
		MaxSecurityCount++;
		if (MaxSecurityCount > 100) {
			printk("Max limit for sk_proc_read security counter!\n");
			return;
		}
		pNet = (DEV_NET*) SkgeProcDev->priv;
		pAC = pNet->pAC;
		next = pAC->Next;
		pPnmiStruct = &pAC->PnmiStruct;
		/* NetIndex in GetStruct is now required, zero is only dummy */

		for (t=pAC->GIni.GIMacsFound; t > 0; t--) {
			if ((pAC->GIni.GIMacsFound == 2) && pAC->RlmtNets == 1)
				t--;

			spin_lock_irqsave(&pAC->SlowPathLock, Flags);
			Size = SK_PNMI_STRUCT_SIZE;
#ifdef SK_DIAG_SUPPORT
			if (pAC->BoardLevel == SK_INIT_DATA) {
				SK_MEMCPY(&(pAC->PnmiStruct), &(pAC->PnmiBackup), sizeof(SK_PNMI_STRUCT_DATA));
				if (pAC->DiagModeActive == DIAG_NOTACTIVE) {
					pAC->Pnmi.DiagAttached = SK_DIAG_IDLE;
				}
			} else {
				SkPnmiGetStruct(pAC, pAC->IoBase, pPnmiStruct, &Size, t-1);
			}
#else
			SkPnmiGetStruct(pAC, pAC->IoBase, 
				pPnmiStruct, &Size, t-1);
#endif
			spin_unlock_irqrestore(&pAC->SlowPathLock, Flags);
	
			if (strcmp(pAC->dev[t-1]->name, file->name) == 0) {
				pPnmiStat = &pPnmiStruct->Stat[0];
				len = sk_proc_print(buffer, 
					"\nDetailed statistic for device %s\n",
					pAC->dev[t-1]->name);
				len += sk_proc_print(buffer,
					"=======================================\n");
	
				/* Board statistics */
				len += sk_proc_print(buffer, 
					"\nBoard statistics\n\n");
				len += sk_proc_print(buffer,
					"Active Port                    %c\n",
					'A' + pAC->Rlmt.Net[t-1].Port[pAC->Rlmt.
					Net[t-1].PrefPort]->PortNumber);
				len += sk_proc_print(buffer,
					"Preferred Port                 %c\n",
					'A' + pAC->Rlmt.Net[t-1].Port[pAC->Rlmt.
					Net[t-1].PrefPort]->PortNumber);

				len += sk_proc_print(buffer,
					"Bus speed (MHz)                %d\n",
					pPnmiStruct->BusSpeed);

				len += sk_proc_print(buffer,
					"Bus width (Bit)                %d\n",
					pPnmiStruct->BusWidth);
				len += sk_proc_print(buffer,
					"Driver version                 %s\n",
					VER_STRING);
				len += sk_proc_print(buffer,
					"Hardware revision              v%d.%d\n",
					(pAC->GIni.GIPciHwRev >> 4) & 0x0F,
					pAC->GIni.GIPciHwRev & 0x0F);

				/* Print sensor informations */
				for (i=0; i < pAC->I2c.MaxSens; i ++) {
					/* Check type */
					switch (pAC->I2c.SenTable[i].SenType) {
					case 1:
						strcpy(sens_msg, pAC->I2c.SenTable[i].SenDesc);
						strcat(sens_msg, " (C)");
						len += sk_proc_print(buffer,
							"%-25s      %d.%02d\n",
							sens_msg,
							pAC->I2c.SenTable[i].SenValue / 10,
							pAC->I2c.SenTable[i].SenValue % 10);

						strcpy(sens_msg, pAC->I2c.SenTable[i].SenDesc);
						strcat(sens_msg, " (F)");
						len += sk_proc_print(buffer,
							"%-25s      %d.%02d\n",
							sens_msg,
							((((pAC->I2c.SenTable[i].SenValue)
							*10)*9)/5 + 3200)/100,
							((((pAC->I2c.SenTable[i].SenValue)
							*10)*9)/5 + 3200) % 10);
						break;
					case 2:
						strcpy(sens_msg, pAC->I2c.SenTable[i].SenDesc);
						strcat(sens_msg, " (V)");
						len += sk_proc_print(buffer,
							"%-25s      %d.%03d\n",
							sens_msg,
							pAC->I2c.SenTable[i].SenValue / 1000,
							pAC->I2c.SenTable[i].SenValue % 1000);
						break;
					case 3:
						strcpy(sens_msg, pAC->I2c.SenTable[i].SenDesc);
						strcat(sens_msg, " (rpm)");
						len += sk_proc_print(buffer,
							"%-25s      %d\n",
							sens_msg,
							pAC->I2c.SenTable[i].SenValue);
						break;
					default:
						break;
					}
				}
				
				/*Receive statistics */
				len += sk_proc_print(buffer, 
				"\nReceive statistics\n\n");

				len += sk_proc_print(buffer,
					"Received bytes                 %Lu\n",
					(unsigned long long) pPnmiStat->StatRxOctetsOkCts);
				len += sk_proc_print(buffer,
					"Received packets               %Lu\n",
					(unsigned long long) pPnmiStat->StatRxOkCts);
#if 0
				if (pAC->GIni.GP[0].PhyType == SK_PHY_XMAC && 
					pAC->HWRevision < 12) {
					pPnmiStruct->InErrorsCts = pPnmiStruct->InErrorsCts - 
						pPnmiStat->StatRxShortsCts;
					pPnmiStat->StatRxShortsCts = 0;
				}
#endif
				if (pNet->Mtu > 1500) 
					pPnmiStruct->InErrorsCts = pPnmiStruct->InErrorsCts -
						pPnmiStat->StatRxTooLongCts;

				len += sk_proc_print(buffer,
					"Receive errors                 %Lu\n",
					(unsigned long long) pPnmiStruct->InErrorsCts);
				len += sk_proc_print(buffer,
					"Receive dropped                %Lu\n",
					(unsigned long long) pPnmiStruct->RxNoBufCts);
				len += sk_proc_print(buffer,
					"Received multicast             %Lu\n",
					(unsigned long long) pPnmiStat->StatRxMulticastOkCts);
				len += sk_proc_print(buffer,
					"Receive error types\n");
				len += sk_proc_print(buffer,
					"   length                      %Lu\n",
					(unsigned long long) pPnmiStat->StatRxRuntCts);
				len += sk_proc_print(buffer,
					"   buffer overflow             %Lu\n",
					(unsigned long long) pPnmiStat->StatRxFifoOverflowCts);
				len += sk_proc_print(buffer,
					"   bad crc                     %Lu\n",
					(unsigned long long) pPnmiStat->StatRxFcsCts);
				len += sk_proc_print(buffer,
					"   framing                     %Lu\n",
					(unsigned long long) pPnmiStat->StatRxFramingCts);
				len += sk_proc_print(buffer,
					"   missed frames               %Lu\n",
					(unsigned long long) pPnmiStat->StatRxMissedCts);

				if (pNet->Mtu > 1500)
					pPnmiStat->StatRxTooLongCts = 0;

				len += sk_proc_print(buffer,
					"   too long                    %Lu\n",
					(unsigned long long) pPnmiStat->StatRxTooLongCts);					
				len += sk_proc_print(buffer,
					"   carrier extension           %Lu\n",
					(unsigned long long) pPnmiStat->StatRxCextCts);				
				len += sk_proc_print(buffer,
					"   too short                   %Lu\n",
					(unsigned long long) pPnmiStat->StatRxShortsCts);				
				len += sk_proc_print(buffer,
					"   symbol                      %Lu\n",
					(unsigned long long) pPnmiStat->StatRxSymbolCts);				
				len += sk_proc_print(buffer,
					"   LLC MAC size                %Lu\n",
					(unsigned long long) pPnmiStat->StatRxIRLengthCts);				
				len += sk_proc_print(buffer,
					"   carrier event               %Lu\n",
					(unsigned long long) pPnmiStat->StatRxCarrierCts);				
				len += sk_proc_print(buffer,
					"   jabber                      %Lu\n",
					(unsigned long long) pPnmiStat->StatRxJabberCts);				


				/*Transmit statistics */
				len += sk_proc_print(buffer, 
				"\nTransmit statistics\n\n");
				
				len += sk_proc_print(buffer,
					"Transmited bytes               %Lu\n",
					(unsigned long long) pPnmiStat->StatTxOctetsOkCts);
				len += sk_proc_print(buffer,
					"Transmited packets             %Lu\n",
					(unsigned long long) pPnmiStat->StatTxOkCts);
				len += sk_proc_print(buffer,
					"Transmit errors                %Lu\n",
					(unsigned long long) pPnmiStat->StatTxSingleCollisionCts);
				len += sk_proc_print(buffer,
					"Transmit dropped               %Lu\n",
					(unsigned long long) pPnmiStruct->TxNoBufCts);
				len += sk_proc_print(buffer,
					"Transmit collisions            %Lu\n",
					(unsigned long long) pPnmiStat->StatTxSingleCollisionCts);
				len += sk_proc_print(buffer,
					"Transmit error types\n");
				len += sk_proc_print(buffer,
					"   excessive collision         %ld\n",
					pAC->stats.tx_aborted_errors);
				len += sk_proc_print(buffer,
					"   carrier                     %Lu\n",
					(unsigned long long) pPnmiStat->StatTxCarrierCts);
				len += sk_proc_print(buffer,
					"   fifo underrun               %Lu\n",
					(unsigned long long) pPnmiStat->StatTxFifoUnderrunCts);
				len += sk_proc_print(buffer,
					"   heartbeat                   %Lu\n",
					(unsigned long long) pPnmiStat->StatTxCarrierCts);
				len += sk_proc_print(buffer,
					"   window                      %ld\n",
					pAC->stats.tx_window_errors);
				
			} /* if (strcmp(pACname, currDeviceName) == 0) */
		}
		SkgeProcDev = next;
	}
}

/*****************************************************************************
 *
 *      sk_proc_print -generic line print  
 *
 * Description:
 *  This function fills the proc entry with statistic data about 
 *  the ethernet device.
 *  
 * Returns: number of bytes written
 *      
 */ 
static int sk_proc_print(void *writePtr, char *format, ...)
{   
#define MAX_LEN_SINGLE_LINE 256
	char     str[MAX_LEN_SINGLE_LINE];
	va_list  a_start;
	int      lenght = 0;

	char    *buffer = (char *) writePtr;
	buffer = buffer + len; /* plus global variable len for current location */

	SK_MEMSET(str, 0, MAX_LEN_SINGLE_LINE);

	va_start(a_start, format);
	vsprintf(str, format, a_start);
	va_end(a_start);

	lenght = strlen(str);

	sprintf(buffer, str);
	return lenght;
}


/*******************************************************************************
 *
 * End of file
 *
 ******************************************************************************/
