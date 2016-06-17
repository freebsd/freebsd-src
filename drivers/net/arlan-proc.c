#include <linux/config.h>
#include "arlan.h"

#include <linux/sysctl.h>

#ifdef CONFIG_PROC_FS


#include <linux/version.h>

/* void enableReceive(struct net_device* dev);
*/



#define ARLAN_STR_SIZE 	0x2ff0
#define DEV_ARLAN_INFO 	1
#define DEV_ARLAN 	1
#define SARLG(type,var) {\
	pos += sprintf(arlan_drive_info+pos, "%s\t=\t0x%x\n", #var, READSHMB(priva->card->var));	\
	}

#define SARLBN(type,var,nn) {\
	pos += sprintf(arlan_drive_info+pos, "%s\t=\t0x",#var);\
	for (i=0; i < nn; i++ ) pos += sprintf(arlan_drive_info+pos, "%02x",READSHMB(priva->card->var[i]));\
	pos += sprintf(arlan_drive_info+pos, "\n");	\
	}

#define SARLBNpln(type,var,nn) {\
	for (i=0; i < nn; i++ ) pos += sprintf(arlan_drive_info+pos, "%02x",READSHMB(priva->card->var[i]));\
	}

#define SARLSTR(var,nn) {\
	char tmpStr[400];\
	int  tmpLn = nn;\
	if (nn > 399 ) tmpLn = 399; \
	memcpy(tmpStr,(char *) priva->conf->var,tmpLn);\
	tmpStr[tmpLn] = 0; \
	pos += sprintf(arlan_drive_info+pos, "%s\t=\t%s \n",#var,priva->conf->var);\
	}

#define SARLUC(var)  	SARLG(u_char, var)
#define SARLUCN(var,nn) SARLBN(u_char,var, nn)
#define SARLUS(var)	SARLG(u_short, var)
#define SARLUSN(var,nn)	SARLBN(u_short,var, nn)
#define SARLUI(var)	SARLG(u_int, var)

#define SARLUSA(var) {\
	u_short tmpVar;\
	memcpy(&tmpVar, (short *) priva->conf->var,2); \
	pos += sprintf(arlan_drive_info+pos, "%s\t=\t0x%x\n",#var, tmpVar);\
}

#define SARLUIA(var) {\
	u_int tmpVar;\
	memcpy(&tmpVar, (int* )priva->conf->var,4); \
	pos += sprintf(arlan_drive_info+pos, "%s\t=\t0x%x\n",#var, tmpVar);\
}


static const char *arlan_diagnostic_info_string(struct net_device *dev)
{

	volatile struct arlan_shmem *arlan = ((struct arlan_private *) dev->priv)->card;
	u_char diagnosticInfo;

	READSHM(diagnosticInfo, arlan->diagnosticInfo, u_char);

	switch (diagnosticInfo)
	{
		case 0xFF:
			return "Diagnostic info is OK";
		case 0xFE:
			return "ERROR EPROM Checksum error ";
		case 0xFD:
			return "ERROR Local Ram Test Failed ";
		case 0xFC:
			return "ERROR SCC failure ";
		case 0xFB:
			return "ERROR BackBone failure ";
		case 0xFA:
			return "ERROR tranceiver not found ";
		case 0xF9:
			return "ERROR no more address space ";
		case 0xF8:
			return "ERROR Checksum error  ";
		case 0xF7:
			return "ERROR Missing SS Code";
		case 0xF6:
			return "ERROR Invalid config format";
		case 0xF5:
			return "ERROR Reserved errorcode F5";
		case 0xF4:
			return "ERROR Invalid spreading code/channel number";
		case 0xF3:
			return "ERROR Load Code Error";
		case 0xF2:
			return "ERROR Reserver errorcode F2 ";
		case 0xF1:
			return "ERROR Invalid command receivec by LAN card ";
		case 0xF0:
			return "ERROR Invalid parameter found in command ";
		case 0xEF:
			return "ERROR On-chip timer failure ";
		case 0xEE:
			return "ERROR T410 timer failure ";
		case 0xED:
			return "ERROR Too Many TxEnable commands ";
		case 0xEC:
			return "ERROR EEPROM error on radio module ";
		default:
			return "ERROR unknown Diagnostic info reply code ";
	  }
};

static const char *arlan_hardware_type_string(struct net_device *dev)
{
	u_char hardwareType;
	volatile struct arlan_shmem *arlan = ((struct arlan_private *) dev->priv)->card;

	READSHM(hardwareType, arlan->hardwareType, u_char);
	switch (hardwareType)
	{
		case 0x00:
			return "type A450";
		case 0x01:
			return "type A650 ";
		case 0x04:
			return "type TMA coproc";
		case 0x0D:
			return "type A650E ";
		case 0x18:
			return "type TMA coproc Australian";
		case 0x19:
			return "type A650A ";
		case 0x26:
			return "type TMA coproc European";
		case 0x2E:
			return "type A655 ";
		case 0x2F:
			return "type A655A ";
		case 0x30:
			return "type A655E ";
		case 0x0B:
			return "type A670 ";
		case 0x0C:
			return "type A670E ";
		case 0x2D:
			return "type A670A ";
		case 0x0F:
			return "type A411T";
		case 0x16:
			return "type A411TA";
		case 0x1B:
			return "type A440T";
		case 0x1C:
			return "type A412T";
		case 0x1E:
			return "type A412TA";
		case 0x22:
			return "type A411TE";
		case 0x24:
			return "type A412TE";
		case 0x27:
			return "type A671T ";
		case 0x29:
			return "type A671TA ";
		case 0x2B:
			return "type A671TE ";
		case 0x31:
			return "type A415T ";
		case 0x33:
			return "type A415TA ";
		case 0x35:
			return "type A415TE ";
		case 0x37:
			return "type A672";
		case 0x39:
			return "type A672A ";
		case 0x3B:
			return "type A672T";
		case 0x6B:
			return "type IC2200";
		default:
			return "type A672T";
	}
}
#ifdef ARLAN_DEBUGGING
static void arlan_print_diagnostic_info(struct net_device *dev)
{
	int i;
	u_char diagnosticInfo;
	u_short diagnosticOffset;
	u_char hardwareType;
	volatile struct arlan_shmem *arlan = ((struct arlan_private *) dev->priv)->card;

	//  ARLAN_DEBUG_ENTRY("arlan_print_diagnostic_info");

	if (READSHMB(arlan->configuredStatusFlag) == 0)
		printk("Arlan: Card NOT configured\n");
	else
		printk("Arlan: Card is configured\n");

	READSHM(diagnosticInfo, arlan->diagnosticInfo, u_char);
	READSHM(diagnosticOffset, arlan->diagnosticOffset, u_short);

	printk(KERN_INFO "%s\n", arlan_diagnostic_info_string(dev));

	if (diagnosticInfo != 0xff)
		printk("%s arlan: Diagnostic Offset %d \n", dev->name, diagnosticOffset);

	printk("arlan: LAN CODE ID = ");
	for (i = 0; i < 6; i++)
		DEBUGSHM(1, "%03d:", arlan->lanCardNodeId[i], u_char);
	printk("\n");

	printk("arlan: Arlan BroadCast address  = ");
	for (i = 0; i < 6; i++)
		DEBUGSHM(1, "%03d:", arlan->broadcastAddress[i], u_char);
	printk("\n");

	READSHM(hardwareType, arlan->hardwareType, u_char);
	printk(KERN_INFO "%s\n", arlan_hardware_type_string(dev));


	DEBUGSHM(1, "arlan: channelNumber=%d\n", arlan->channelNumber, u_char);
	DEBUGSHM(1, "arlan: channelSet=%d\n", arlan->channelSet, u_char);
	DEBUGSHM(1, "arlan: spreadingCode=%d\n", arlan->spreadingCode, u_char);
	DEBUGSHM(1, "arlan: radioNodeId=%d\n", arlan->radioNodeId, u_short);
	DEBUGSHM(1, "arlan: SID	=%d\n", arlan->SID, u_short);
	DEBUGSHM(1, "arlan: rxOffset=%d\n", arlan->rxOffset, u_short);

	DEBUGSHM(1, "arlan: registration mode is %d\n", arlan->registrationMode, u_char);

	printk("arlan: name= ");
	IFDEBUG(1)
	
	for (i = 0; i < 16; i++)
	{
		char c;
		READSHM(c, arlan->name[i], char);
		if (c)
			printk("%c", c);
	}
	printk("\n");

//   ARLAN_DEBUG_EXIT("arlan_print_diagnostic_info");

}


/******************************		TEST 	MEMORY	**************/

static int arlan_hw_test_memory(struct net_device *dev)
{
	u_char *ptr;
	int i;
	int memlen = sizeof(struct arlan_shmem) - 0xF;	/* avoid control register */
	volatile char *arlan_mem = (char *) (dev->mem_start);
	volatile struct arlan_shmem *arlan = ((struct arlan_private *) dev->priv)->card;
	char pattern;

	ptr = NULL;

	/* hold card in reset state */
	setHardwareReset(dev);

	/* test memory */
	pattern = 0;
	for (i = 0; i < memlen; i++)
		WRITESHM(arlan_mem[i], ((u_char) pattern++), u_char);

	pattern = 0;
	for (i = 0; i < memlen; i++)
	{
		char res;
		READSHM(res, arlan_mem[i], char);
		if (res != pattern++)
		{
			printk(KERN_ERR "Arlan driver memory test 1 failed \n");
			return -1;
		}
	}

	pattern = 0;
	for (i = 0; i < memlen; i++)
		WRITESHM(arlan_mem[i], ~(pattern++), char);

	pattern = 0;
	for (i = 0; i < memlen; i++)
	{
		char res;
		READSHM(res, arlan_mem[i], char);
		if (res != ~(pattern++))
		{
			printk(KERN_ERR "Arlan driver memory test 2 failed \n");
			return -1;
		}
	}

	/* zero memory */
	for (i = 0; i < memlen; i++)
		WRITESHM(arlan_mem[i], 0x00, char);

	IFDEBUG(1) printk(KERN_INFO "Arlan: memory tests ok\n");

	/* set reset flag and then release reset */
	WRITESHM(arlan->resetFlag, 0xff, u_char);

	clearChannelAttention(dev);
	clearHardwareReset(dev);

	/* wait for reset flag to become zero, we'll wait for two seconds */
	if (arlan_command(dev, ARLAN_COMMAND_LONG_WAIT_NOW))
	{
		printk(KERN_ERR "%s arlan: failed to come back from memory test\n", dev->name);
		return -1;
	}
	return 0;
}

static int arlan_setup_card_by_book(struct net_device *dev)
{
	u_char irqLevel, configuredStatusFlag;
	volatile struct arlan_shmem *arlan = ((struct arlan_private *) dev->priv)->card;

//	ARLAN_DEBUG_ENTRY("arlan_setup_card");

	READSHM(configuredStatusFlag, arlan->configuredStatusFlag, u_char);

	IFDEBUG(10)
	if (configuredStatusFlag != 0)
		IFDEBUG(10) printk("arlan: CARD IS CONFIGURED\n");
	else
		IFDEBUG(10) printk("arlan: card is NOT configured\n");

	if (testMemory || (READSHMB(arlan->diagnosticInfo) != 0xff))
		if (arlan_hw_test_memory(dev))
			return -1;

	DEBUGSHM(4, "arlan configuredStatus = %d \n", arlan->configuredStatusFlag, u_char);
	DEBUGSHM(4, "arlan driver diagnostic: 0x%2x\n", arlan->diagnosticInfo, u_char);

	/* issue nop command - no interrupt */
	arlan_command(dev, ARLAN_COMMAND_NOOP);
	if (arlan_command(dev, ARLAN_COMMAND_WAIT_NOW) != 0)
		return -1;

	IFDEBUG(50) printk("1st Noop successfully executed !!\n");

	/* try to turn on the arlan interrupts */
	clearClearInterrupt(dev);
	setClearInterrupt(dev);
	setInterruptEnable(dev);

	/* issue nop command - with interrupt */

	arlan_command(dev, ARLAN_COMMAND_NOOPINT);
	if (arlan_command(dev, ARLAN_COMMAND_WAIT_NOW) != 0)
		return -1;


	IFDEBUG(50) printk("2nd Noop successfully executed !!\n");

	READSHM(irqLevel, arlan->irqLevel, u_char)
	
	if (irqLevel != dev->irq)
	{
		IFDEBUG(1) printk(KERN_WARNING "arlan dip switches set irq to %d\n", irqLevel);
		printk(KERN_WARNING "device driver irq set to %d - does not match\n", dev->irq);
		dev->irq = irqLevel;
	}
	else
		IFDEBUG(2) printk("irq level is OK\n");


	IFDEBUG(3) arlan_print_diagnostic_info(dev);

	arlan_command(dev, ARLAN_COMMAND_CONF);

	READSHM(configuredStatusFlag, arlan->configuredStatusFlag, u_char);
	if (configuredStatusFlag == 0)
	{
		printk(KERN_WARNING "arlan configure failed\n");
		return -1;
	}
	arlan_command(dev, ARLAN_COMMAND_LONG_WAIT_NOW);
	arlan_command(dev, ARLAN_COMMAND_RX);
	arlan_command(dev, ARLAN_COMMAND_LONG_WAIT_NOW);
	printk(KERN_NOTICE "%s: arlan driver version %s loaded\n",
	       dev->name, arlan_version);

//	ARLAN_DEBUG_EXIT("arlan_setup_card");

	return 0;		/* no errors */
}
#endif

#ifdef ARLAN_PROC_INTERFACE
#ifdef ARLAN_PROC_SHM_DUMP

static char arlan_drive_info[ARLAN_STR_SIZE] = "A655\n\0";

static int arlan_sysctl_info(ctl_table * ctl, int write, struct file *filp,
		      void *buffer, size_t * lenp)
{
	int i;
	int retv, pos, devnum;
	struct arlan_private *priva = NULL;
	struct net_device *dev;
	pos = 0;
	if (write)
	{
		printk("wrirte: ");
		for (i = 0; i < 100; i++)
			printk("adi %x \n", arlan_drive_info[i]);
	}
	if (ctl->procname == NULL || arlan_drive_info == NULL)
	{
		printk(KERN_WARNING " procname is NULL in sysctl_table or arlan_drive_info is NULL \n at arlan module\n ");
		return -1;
	}
	devnum = ctl->procname[5] - '0';
	if (devnum < 0 || devnum > MAX_ARLANS - 1)
	{
		printk(KERN_WARNING "too strange devnum in procfs parse\n ");
		return -1;
	}
	else if (arlan_device[devnum] == NULL)
	{
		if (ctl->procname)
			pos += sprintf(arlan_drive_info + pos, "\t%s\n\n", ctl->procname);
		pos += sprintf(arlan_drive_info + pos, "No device found here \n");
		goto final;
	}
	else
		priva = arlan_device[devnum]->priv;

	if (priva == NULL)
	{
		printk(KERN_WARNING " Could not find the device private in arlan procsys, bad\n ");
		return -1;
	}
	dev = arlan_device[devnum];

	memcpy_fromio(priva->conf, priva->card, sizeof(struct arlan_shmem));

	pos = sprintf(arlan_drive_info, "Arlan  info \n");
	/* Header Signature */
	SARLSTR(textRegion, 48);
	SARLUC(resetFlag);
	pos += sprintf(arlan_drive_info + pos, "diagnosticInfo\t=\t%s \n", arlan_diagnostic_info_string(dev));
	SARLUC(diagnosticInfo);
	SARLUS(diagnosticOffset);
	SARLUCN(_1, 12);
	SARLUCN(lanCardNodeId, 6);
	SARLUCN(broadcastAddress, 6);
	pos += sprintf(arlan_drive_info + pos, "hardwareType =\t  %s \n", arlan_hardware_type_string(dev));
	SARLUC(hardwareType);
	SARLUC(majorHardwareVersion);
	SARLUC(minorHardwareVersion);
	SARLUC(radioModule);
	SARLUC(defaultChannelSet);
	SARLUCN(_2, 47);

	/* Control/Status Block - 0x0080 */
	SARLUC(interruptInProgress);
	SARLUC(cntrlRegImage);

	SARLUCN(_3, 14);
	SARLUC(commandByte);
	SARLUCN(commandParameter, 15);

	/* Receive Status - 0x00a0 */
	SARLUC(rxStatus);
	SARLUC(rxFrmType);
	SARLUS(rxOffset);
	SARLUS(rxLength);
	SARLUCN(rxSrc, 6);
	SARLUC(rxBroadcastFlag);
	SARLUC(rxQuality);
	SARLUC(scrambled);
	SARLUCN(_4, 1);

	/* Transmit Status - 0x00b0 */
	SARLUC(txStatus);
	SARLUC(txAckQuality);
	SARLUC(numRetries);
	SARLUCN(_5, 14);
	SARLUCN(registeredRouter, 6);
	SARLUCN(backboneRouter, 6);
	SARLUC(registrationStatus);
	SARLUC(configuredStatusFlag);
	SARLUCN(_6, 1);
	SARLUCN(ultimateDestAddress, 6);
	SARLUCN(immedDestAddress, 6);
	SARLUCN(immedSrcAddress, 6);
	SARLUS(rxSequenceNumber);
	SARLUC(assignedLocaltalkAddress);
	SARLUCN(_7, 27);

	/* System Parameter Block */

	/* - Driver Parameters (Novell Specific) */

	SARLUS(txTimeout);
	SARLUS(transportTime);
	SARLUCN(_8, 4);

	/* - Configuration Parameters */
	SARLUC(irqLevel);
	SARLUC(spreadingCode);
	SARLUC(channelSet);
	SARLUC(channelNumber);
	SARLUS(radioNodeId);
	SARLUCN(_9, 2);
	SARLUC(scramblingDisable);
	SARLUC(radioType);
	SARLUS(routerId);
	SARLUCN(_10, 9);
	SARLUC(txAttenuation);
	SARLUIA(systemId);
	SARLUS(globalChecksum);
	SARLUCN(_11, 4);
	SARLUS(maxDatagramSize);
	SARLUS(maxFrameSize);
	SARLUC(maxRetries);
	SARLUC(receiveMode);
	SARLUC(priority);
	SARLUC(rootOrRepeater);
	SARLUCN(specifiedRouter, 6);
	SARLUS(fastPollPeriod);
	SARLUC(pollDecay);
	SARLUSA(fastPollDelay);
	SARLUC(arlThreshold);
	SARLUC(arlDecay);
	SARLUCN(_12, 1);
	SARLUS(specRouterTimeout);
	SARLUCN(_13, 5);

	/* Scrambled Area */
	SARLUIA(SID);
	SARLUCN(encryptionKey, 12);
	SARLUIA(_14);
	SARLUSA(waitTime);
	SARLUSA(lParameter);
	SARLUCN(_15, 3);
	SARLUS(headerSize);
	SARLUS(sectionChecksum);

	SARLUC(registrationMode);
	SARLUC(registrationFill);
	SARLUS(pollPeriod);
	SARLUS(refreshPeriod);
	SARLSTR(name, 16);
	SARLUCN(NID, 6);
	SARLUC(localTalkAddress);
	SARLUC(codeFormat);
	SARLUC(numChannels);
	SARLUC(channel1);
	SARLUC(channel2);
	SARLUC(channel3);
	SARLUC(channel4);
	SARLUCN(SSCode, 59);

/*      SARLUCN( _16, 0x140);
 */
	/* Statistics Block - 0x0300 */
	SARLUC(hostcpuLock);
	SARLUC(lancpuLock);
	SARLUCN(resetTime, 18);
	SARLUIA(numDatagramsTransmitted);
	SARLUIA(numReTransmissions);
	SARLUIA(numFramesDiscarded);
	SARLUIA(numDatagramsReceived);
	SARLUIA(numDuplicateReceivedFrames);
	SARLUIA(numDatagramsDiscarded);
	SARLUS(maxNumReTransmitDatagram);
	SARLUS(maxNumReTransmitFrames);
	SARLUS(maxNumConsecutiveDuplicateFrames);
	/* misaligned here so we have to go to characters */
	SARLUIA(numBytesTransmitted);
	SARLUIA(numBytesReceived);
	SARLUIA(numCRCErrors);
	SARLUIA(numLengthErrors);
	SARLUIA(numAbortErrors);
	SARLUIA(numTXUnderruns);
	SARLUIA(numRXOverruns);
	SARLUIA(numHoldOffs);
	SARLUIA(numFramesTransmitted);
	SARLUIA(numFramesReceived);
	SARLUIA(numReceiveFramesLost);
	SARLUIA(numRXBufferOverflows);
	SARLUIA(numFramesDiscardedAddrMismatch);
	SARLUIA(numFramesDiscardedSIDMismatch);
	SARLUIA(numPollsTransmistted);
	SARLUIA(numPollAcknowledges);
	SARLUIA(numStatusTimeouts);
	SARLUIA(numNACKReceived);
	SARLUS(auxCmd);
	SARLUCN(dumpPtr, 4);
	SARLUC(dumpVal);
	SARLUC(wireTest);
	
	/* next 4 seems too long for procfs, over single page ?
	SARLUCN( _17, 0x86);
	SARLUCN( txBuffer, 0x800);
	SARLUCN( rxBuffer,  0x800); 
	SARLUCN( _18, 0x0bff);
	 */

	pos += sprintf(arlan_drive_info + pos, "rxRing\t=\t0x");
	for (i = 0; i < 0x50; i++)
		pos += sprintf(arlan_drive_info + pos, "%02x", ((char *) priva->conf)[priva->conf->rxOffset + i]);
	pos += sprintf(arlan_drive_info + pos, "\n");

	SARLUC(configStatus);
	SARLUC(_22);
	SARLUC(progIOCtrl);
	SARLUC(shareMBase);
	SARLUC(controlRegister);

	pos += sprintf(arlan_drive_info + pos, " total %d chars\n", pos);
	if (ctl)
		if (ctl->procname)
			pos += sprintf(arlan_drive_info + pos, " driver name : %s\n", ctl->procname);
final:
	*lenp = pos;

	if (!write)
		retv = proc_dostring(ctl, write, filp, buffer, lenp);
	else
	{
		*lenp = 0;
		return -1;
	}
	return retv;
}


static int arlan_sysctl_info161719(ctl_table * ctl, int write, struct file *filp,
			    void *buffer, size_t * lenp)
{
	int i;
	int retv, pos, devnum;
	struct arlan_private *priva = NULL;

	pos = 0;
	devnum = ctl->procname[5] - '0';
	if (arlan_device[devnum] == NULL)
	{
		pos += sprintf(arlan_drive_info + pos, "No device found here \n");
		goto final;
	}
	else
		priva = arlan_device[devnum]->priv;
	if (priva == NULL)
	{
		printk(KERN_WARNING " Could not find the device private in arlan procsys, bad\n ");
		return -1;
	}
	memcpy_fromio(priva->conf, priva->card, sizeof(struct arlan_shmem));
	SARLUCN(_16, 0xC0);
	SARLUCN(_17, 0x6A);
	SARLUCN(_18, 14);
	SARLUCN(_19, 0x86);
	SARLUCN(_21, 0x3fd);

final:
	*lenp = pos;
	retv = proc_dostring(ctl, write, filp, buffer, lenp);
	return retv;
}

static int arlan_sysctl_infotxRing(ctl_table * ctl, int write, struct file *filp,
			    void *buffer, size_t * lenp)
{
	int i;
	int retv, pos, devnum;
	struct arlan_private *priva = NULL;

	pos = 0;
	devnum = ctl->procname[5] - '0';
	if (arlan_device[devnum] == NULL)
	{
		  pos += sprintf(arlan_drive_info + pos, "No device found here \n");
		  goto final;
	}
	else
		priva = arlan_device[devnum]->priv;
	if (priva == NULL)
	{
		printk(KERN_WARNING " Could not find the device private in arlan procsys, bad\n ");
		return -1;
	}
	memcpy_fromio(priva->conf, priva->card, sizeof(struct arlan_shmem));
	SARLBNpln(u_char, txBuffer, 0x800);
final:
	*lenp = pos;
	retv = proc_dostring(ctl, write, filp, buffer, lenp);
	return retv;
}

static int arlan_sysctl_inforxRing(ctl_table * ctl, int write, struct file *filp,
			    void *buffer, size_t * lenp)
{
	int i;
	int retv, pos, devnum;
	struct arlan_private *priva = NULL;

	pos = 0;
	devnum = ctl->procname[5] - '0';
	if (arlan_device[devnum] == NULL)
	{
		  pos += sprintf(arlan_drive_info + pos, "No device found here \n");
		  goto final;
	} else
		priva = arlan_device[devnum]->priv;
	if (priva == NULL)
	{
		printk(KERN_WARNING " Could not find the device private in arlan procsys, bad\n ");
		return -1;
	}
	memcpy_fromio(priva->conf, priva->card, sizeof(struct arlan_shmem));
	SARLBNpln(u_char, rxBuffer, 0x800);
final:
	*lenp = pos;
	retv = proc_dostring(ctl, write, filp, buffer, lenp);
	return retv;
}

static int arlan_sysctl_info18(ctl_table * ctl, int write, struct file *filp,
			void *buffer, size_t * lenp)
{
	int i;
	int retv, pos, devnum;
	struct arlan_private *priva = NULL;

	pos = 0;
	devnum = ctl->procname[5] - '0';
	if (arlan_device[devnum] == NULL)
	{
		pos += sprintf(arlan_drive_info + pos, "No device found here \n");
		goto final;
	}
	else
		priva = arlan_device[devnum]->priv;
	if (priva == NULL)
	{
		printk(KERN_WARNING " Could not find the device private in arlan procsys, bad\n ");
		return -1;
	}
	memcpy_fromio(priva->conf, priva->card, sizeof(struct arlan_shmem));
	SARLBNpln(u_char, _18, 0x800);

final:
	*lenp = pos;
	retv = proc_dostring(ctl, write, filp, buffer, lenp);
	return retv;
}


#endif				/* #ifdef ARLAN_PROC_SHM_DUMP */


static char conf_reset_result[200];

static int arlan_configure(ctl_table * ctl, int write, struct file *filp,
		    void *buffer, size_t * lenp)
{
	int pos = 0;
	int devnum = ctl->procname[6] - '0';
	struct arlan_private *priv;

	if (devnum < 0 || devnum > MAX_ARLANS - 1)
	{
		  printk(KERN_WARNING "too strange devnum in procfs parse\n ");
		  return -1;
	}
	else if (arlan_device[devnum] != NULL)
	{
		  priv = arlan_device[devnum]->priv;

		  arlan_command(arlan_device[devnum], ARLAN_COMMAND_CLEAN_AND_CONF);
	}
	else
		return -1;

	*lenp = pos;
	return proc_dostring(ctl, write, filp, buffer, lenp);
}

static int arlan_sysctl_reset(ctl_table * ctl, int write, struct file *filp,
		       void *buffer, size_t * lenp)
{
	int pos = 0;
	int devnum = ctl->procname[5] - '0';
	struct arlan_private *priv;

	if (devnum < 0 || devnum > MAX_ARLANS - 1)
	{
		  printk(KERN_WARNING "too strange devnum in procfs parse\n ");
		  return -1;
	}
	else if (arlan_device[devnum] != NULL)
	{
		priv = arlan_device[devnum]->priv;
		arlan_command(arlan_device[devnum], ARLAN_COMMAND_CLEAN_AND_RESET);

	} else
		return -1;
	*lenp = pos + 3;
	return proc_dostring(ctl, write, filp, buffer, lenp);
}


/* Place files in /proc/sys/dev/arlan */
#define CTBLN(num,card,nam) \
        {num , #nam, &(arlan_conf[card].nam), \
         sizeof(int), 0600, NULL, &proc_dointvec}
#ifdef ARLAN_DEBUGGING

#define ARLAN_PROC_DEBUG_ENTRIES	{48, "entry_exit_debug", &arlan_entry_and_exit_debug, \
                sizeof(int), 0600, NULL, &proc_dointvec},\
	{49, "debug", &arlan_debug, \
                sizeof(int), 0600, NULL, &proc_dointvec},
#else 
#define ARLAN_PROC_DEBUG_ENTRIES
#endif

#define ARLAN_SYSCTL_TABLE_TOTAL(cardNo)\
	CTBLN(1,cardNo,spreadingCode),\
	CTBLN(2,cardNo, channelNumber),\
	CTBLN(3,cardNo, scramblingDisable),\
	CTBLN(4,cardNo, txAttenuation),\
	CTBLN(5,cardNo, systemId), \
	CTBLN(6,cardNo, maxDatagramSize),\
	CTBLN(7,cardNo, maxFrameSize),\
	CTBLN(8,cardNo, maxRetries),\
	CTBLN(9,cardNo, receiveMode),\
	CTBLN(10,cardNo, priority),\
	CTBLN(11,cardNo, rootOrRepeater),\
	CTBLN(12,cardNo, SID),\
	CTBLN(13,cardNo, registrationMode),\
	CTBLN(14,cardNo, registrationFill),\
	CTBLN(15,cardNo, localTalkAddress),\
	CTBLN(16,cardNo, codeFormat),\
	CTBLN(17,cardNo, numChannels),\
	CTBLN(18,cardNo, channel1),\
	CTBLN(19,cardNo, channel2),\
	CTBLN(20,cardNo, channel3),\
	CTBLN(21,cardNo, channel4),\
	CTBLN(22,cardNo, txClear),\
	CTBLN(23,cardNo, txRetries),\
	CTBLN(24,cardNo, txRouting),\
	CTBLN(25,cardNo, txScrambled),\
	CTBLN(26,cardNo, rxParameter),\
	CTBLN(27,cardNo, txTimeoutMs),\
	CTBLN(28,cardNo, waitCardTimeout),\
	CTBLN(29,cardNo, channelSet), \
	{30, "name", arlan_conf[cardNo].siteName, \
                16, 0600, NULL, &proc_dostring},\
	CTBLN(31,cardNo,waitTime),\
	CTBLN(32,cardNo,lParameter),\
	CTBLN(33,cardNo,_15),\
	CTBLN(34,cardNo,headerSize),\
	CTBLN(35,cardNo,async),\
	CTBLN(36,cardNo,tx_delay_ms),\
	CTBLN(37,cardNo,retries),\
	CTBLN(38,cardNo,ReTransmitPacketMaxSize),\
	CTBLN(39,cardNo,waitReTransmitPacketMaxSize),\
	CTBLN(40,cardNo,fastReTransCount),\
	CTBLN(41,cardNo,driverRetransmissions),\
	CTBLN(42,cardNo,txAckTimeoutMs),\
	CTBLN(43,cardNo,registrationInterrupts),\
	CTBLN(44,cardNo,hardwareType),\
	CTBLN(45,cardNo,radioType),\
	CTBLN(46,cardNo,writeEEPROM),\
	CTBLN(47,cardNo,writeRadioType),\
	ARLAN_PROC_DEBUG_ENTRIES\
	CTBLN(50,cardNo,in_speed),\
	CTBLN(51,cardNo,out_speed),\
	CTBLN(52,cardNo,in_speed10),\
	CTBLN(53,cardNo,out_speed10),\
	CTBLN(54,cardNo,in_speed_max),\
	CTBLN(55,cardNo,out_speed_max),\
	CTBLN(56,cardNo,measure_rate),\
	CTBLN(57,cardNo,pre_Command_Wait),\
	CTBLN(58,cardNo,rx_tweak1),\
	CTBLN(59,cardNo,rx_tweak2),\
	CTBLN(60,cardNo,tx_queue_len),\



static ctl_table arlan_conf_table0[] =
{
	ARLAN_SYSCTL_TABLE_TOTAL(0)

#ifdef ARLAN_PROC_SHM_DUMP
	{150, "arlan0-txRing", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_infotxRing},
	{151, "arlan0-rxRing", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_inforxRing},
	{152, "arlan0-18", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_info18},
	{153, "arlan0-ring", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_info161719},
	{154, "arlan0-shm-cpy", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_info},
#endif
	{155, "config0", &conf_reset_result, \
	 100, 0400, NULL, &arlan_configure}, \
	{156, "reset0", &conf_reset_result, \
	 100, 0400, NULL, &arlan_sysctl_reset}, \
	{0}
};

static ctl_table arlan_conf_table1[] =
{

	ARLAN_SYSCTL_TABLE_TOTAL(1)

#ifdef ARLAN_PROC_SHM_DUMP
	{150, "arlan1-txRing", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_infotxRing},
	{151, "arlan1-rxRing", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_inforxRing},
	{152, "arlan1-18", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_info18},
	{153, "arlan1-ring", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_info161719},
	{154, "arlan1-shm-cpy", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_info},
#endif
	{155, "config1", &conf_reset_result,
	 100, 0400, NULL, &arlan_configure},
	{156, "reset1", &conf_reset_result,
	 100, 0400, NULL, &arlan_sysctl_reset},
	{0}
};

static ctl_table arlan_conf_table2[] =
{

	ARLAN_SYSCTL_TABLE_TOTAL(2)

#ifdef ARLAN_PROC_SHM_DUMP
	{150, "arlan2-txRing", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_infotxRing},
	{151, "arlan2-rxRing", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_inforxRing},
	{152, "arlan2-18", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_info18},
	{153, "arlan2-ring", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_info161719},
	{154, "arlan2-shm-cpy", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_info},
#endif
	{155, "config2", &conf_reset_result,
	 100, 0400, NULL, &arlan_configure},
	{156, "reset2", &conf_reset_result,
	 100, 0400, NULL, &arlan_sysctl_reset},
	{0}
};

static ctl_table arlan_conf_table3[] =
{

	ARLAN_SYSCTL_TABLE_TOTAL(3)

#ifdef ARLAN_PROC_SHM_DUMP
	{150, "arlan3-txRing", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_infotxRing},
	{151, "arlan3-rxRing", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_inforxRing},
	{152, "arlan3-18", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_info18},
	{153, "arlan3-ring", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_info161719},
	{154, "arlan3-shm-cpy", &arlan_drive_info,
	 ARLAN_STR_SIZE, 0400, NULL, &arlan_sysctl_info},
#endif
	{155, "config3", &conf_reset_result,
	 100, 0400, NULL, &arlan_configure},
	{156, "reset3", &conf_reset_result,
	 100, 0400, NULL, &arlan_sysctl_reset},
	{0}
};



static ctl_table arlan_table[] =
{
	{0, "arlan0", NULL, 0, 0600, arlan_conf_table0},
	{0, "arlan1", NULL, 0, 0600, arlan_conf_table1},
	{0, "arlan2", NULL, 0, 0600, arlan_conf_table2},
	{0, "arlan3", NULL, 0, 0600, arlan_conf_table3},
	{0}
};

#else

static ctl_table arlan_table[MAX_ARLANS + 1] =
{
	{0}
};
#endif
#else

static ctl_table arlan_table[MAX_ARLANS + 1] =
{
	{0}
};
#endif


// static int mmtu = 1234;

static ctl_table arlan_root_table[] =
{
	{254, "arlan", NULL, 0, 0555, arlan_table},
	{0}
};

/* Make sure that /proc/sys/dev is there */
//static ctl_table arlan_device_root_table[] =
//{
//	{CTL_DEV, "dev", NULL, 0, 0555, arlan_root_table},
//	{0}
//};



static struct ctl_table_header *arlan_device_sysctl_header;

int init_arlan_proc(void)
{

	int i = 0;
	if (arlan_device_sysctl_header)
		return 0;
	for (i = 0; i < MAX_ARLANS && arlan_device[i]; i++)
		arlan_table[i].ctl_name = i + 1;
	arlan_device_sysctl_header = register_sysctl_table(arlan_root_table, 0);
	if (!arlan_device_sysctl_header)
		return -1;

	return 0;

};



#ifdef MODULE

int init_module(void)
{

	return init_arlan_proc();
};

void cleanup_module(void)
{
	unregister_sysctl_table(arlan_device_sysctl_header);
	arlan_device_sysctl_header = NULL;

	return;
};

#endif				// MODULE
MODULE_LICENSE("GPL");
