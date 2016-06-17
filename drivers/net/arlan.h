/*
 *  Copyright (C) 1997 Cullen Jennings
 *  Copyright (C) 1998 Elmer.Joandi@ut.ee, +37-255-13500	
 *  GNU General Public License applies
 */
#include <linux/version.h>

#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>	/* For the statistics structure. */
#include <linux/if_arp.h>	/* For ARPHRD_ETHER */
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>

#include <linux/init.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>


//#define ARLAN_DEBUGGING 1

#define ARLAN_PROC_INTERFACE
#define MAX_ARLANS 4 /* not more than 4 ! */
#define ARLAN_PROC_SHM_DUMP /* shows all card registers, makes driver way larger */

#define ARLAN_MAX_MULTICAST_ADDRS 16
#define ARLAN_RCV_CLEAN 	0
#define ARLAN_RCV_PROMISC 1
#define ARLAN_RCV_CONTROL 2


#ifdef CONFIG_PROC_FS
extern int 	init_arlan_proc(void);
#endif

extern struct net_device *arlan_device[MAX_ARLANS];
extern int	arlan_debug;
extern char *	siteName;
extern int	arlan_entry_debug;
extern int	arlan_exit_debug;
extern int	testMemory;
extern const char* arlan_version;
extern int     arlan_command(struct net_device * dev, int command);
 
#define SIDUNKNOWN -1
#define radioNodeIdUNKNOWN -1
#define encryptionKeyUNKNOWN '\0';
#define irqUNKNOWN 0
#define memUNKNOWN 0
#define debugUNKNOWN 0
#define probeUNKNOWN 1
#define numDevicesUNKNOWN 1
#define testMemoryUNKNOWN 1
#define spreadingCodeUNKNOWN 0
#define channelNumberUNKNOWN 0
#define channelSetUNKNOWN 0
#define systemIdUNKNOWN -1
#define registrationModeUNKNOWN -1
#define siteNameUNKNOWN "LinuxSite"



#define IFDEBUG( L ) if ( (L) & arlan_debug ) 
#define ARLAN_FAKE_HDR_LEN 12 

#ifdef ARLAN_DEBUGGING
	#define DEBUG 1
	#define ARLAN_ENTRY_EXIT_DEBUGGING 1
	#define ARLAN_DEBUG(a,b) printk(KERN_DEBUG a, b)
#else
	#define ARLAN_DEBUG(a,b) 
#endif

struct arlan_shmem
{
      /* Header Signature */ 
      volatile	char textRegion[48];
      volatile	u_char resetFlag;
      volatile	u_char  diagnosticInfo;
      volatile	u_short diagnosticOffset;
      volatile	u_char _1[12];
      volatile	u_char lanCardNodeId[6];
      volatile	u_char broadcastAddress[6];
      volatile	u_char hardwareType;
      volatile	u_char majorHardwareVersion;
      volatile	u_char minorHardwareVersion;
      volatile	u_char radioModule;// shows EEPROM, can be overridden at 0x111
      volatile	u_char defaultChannelSet; // shows EEProm, can be overriiden at 0x10A
      volatile	u_char _2[47];
      
      /* Control/Status Block - 0x0080 */
      volatile	u_char interruptInProgress; /* not used by lancpu */
      volatile	u_char cntrlRegImage; /* not used by lancpu */
      volatile	u_char _3[13];
      volatile	u_char dumpByte;
      volatile	u_char commandByte; /* non-zero = active */
      volatile	u_char commandParameter[15];

      /* Receive Status - 0x00a0 */
      volatile	u_char rxStatus; /* 1- data, 2-control, 0xff - registr change */
      volatile	u_char rxFrmType;
      volatile	u_short rxOffset;
      volatile	u_short rxLength;
      volatile	u_char rxSrc[6];
      volatile	u_char rxBroadcastFlag;
      volatile	u_char rxQuality;
      volatile	u_char scrambled;
      volatile	u_char _4[1];
      
      /* Transmit Status - 0x00b0 */
      volatile	u_char txStatus;
      volatile	u_char txAckQuality;
      volatile	u_char numRetries;
      volatile	u_char _5[14];
      volatile	u_char registeredRouter[6];
      volatile	u_char backboneRouter[6];
      volatile	u_char registrationStatus;
      volatile	u_char configuredStatusFlag;
      volatile	u_char _6[1];
      volatile	u_char ultimateDestAddress[6];
      volatile	u_char immedDestAddress[6];
      volatile	u_char immedSrcAddress[6];
      volatile	u_short rxSequenceNumber;
      volatile	u_char assignedLocaltalkAddress;
      volatile	u_char _7[27];

      /* System Parameter Block */

      /* - Driver Parameters (Novell Specific) */

      volatile	u_short txTimeout;
      volatile	u_short transportTime;
      volatile	u_char _8[4];

      /* - Configuration Parameters */
      volatile	u_char irqLevel;
      volatile	u_char spreadingCode;
      volatile	u_char channelSet;
      volatile	u_char channelNumber;
      volatile	u_short radioNodeId;
      volatile	u_char _9[2];
      volatile	u_char scramblingDisable;
      volatile	u_char radioType;
      volatile	u_short routerId;
      volatile	u_char _10[9];
      volatile	u_char txAttenuation;
      volatile	u_char systemId[4]; 
      volatile	u_short globalChecksum;
      volatile	u_char _11[4];
      volatile	u_short maxDatagramSize;
      volatile	u_short maxFrameSize;
      volatile	u_char maxRetries;
      volatile	u_char receiveMode;
      volatile	u_char priority;
      volatile	u_char rootOrRepeater;
      volatile	u_char specifiedRouter[6];
      volatile	u_short fastPollPeriod;
      volatile	u_char pollDecay;
      volatile	u_char fastPollDelay[2];
      volatile	u_char arlThreshold;
      volatile	u_char arlDecay;
      volatile	u_char _12[1];
      volatile	u_short specRouterTimeout;
      volatile	u_char _13[5];

      /* Scrambled Area */
      volatile	u_char SID[4];
      volatile	u_char encryptionKey[12];
      volatile	u_char _14[2];
      volatile	u_char waitTime[2];
      volatile	u_char lParameter[2];
      volatile	u_char _15[3];
      volatile	u_short headerSize;
      volatile	u_short sectionChecksum;

      volatile	u_char registrationMode;
      volatile	u_char registrationFill;
      volatile	u_short pollPeriod;
      volatile	u_short refreshPeriod;
      volatile	u_char name[16];
      volatile	u_char NID[6];
      volatile	u_char localTalkAddress;
      volatile	u_char codeFormat;
      volatile	u_char numChannels;
      volatile	u_char channel1;
      volatile	u_char channel2;
      volatile	u_char channel3;
      volatile	u_char channel4;
      volatile	u_char SSCode[59];

      volatile	u_char _16[0xC0];
      volatile	u_short auxCmd;
      volatile	u_char  dumpPtr[4];
      volatile	u_char dumpVal;
      volatile	u_char _17[0x6A];
      volatile	u_char wireTest;
      volatile	u_char _18[14];

      /* Statistics Block - 0x0300 */
      volatile	u_char hostcpuLock;
      volatile	u_char lancpuLock;
      volatile	u_char resetTime[18];
      
      volatile	u_char numDatagramsTransmitted[4];
      volatile	u_char numReTransmissions[4];
      volatile	u_char numFramesDiscarded[4];
      volatile	u_char numDatagramsReceived[4];
      volatile	u_char numDuplicateReceivedFrames[4];
      volatile	u_char numDatagramsDiscarded[4];
      
      volatile	u_short maxNumReTransmitDatagram;
      volatile	u_short maxNumReTransmitFrames;
      volatile	u_short maxNumConsecutiveDuplicateFrames;
      /* misaligned here so we have to go to characters */
     
      volatile	u_char numBytesTransmitted[4];
      volatile	u_char numBytesReceived[4];
      volatile	u_char numCRCErrors[4];
      volatile	u_char numLengthErrors[4];
      volatile	u_char numAbortErrors[4];
      volatile	u_char numTXUnderruns[4];
      volatile	u_char numRXOverruns[4];
      volatile	u_char numHoldOffs[4];
      volatile	u_char numFramesTransmitted[4];
      volatile	u_char numFramesReceived[4];
      volatile	u_char numReceiveFramesLost[4];
      volatile	u_char numRXBufferOverflows[4];
      volatile	u_char numFramesDiscardedAddrMismatch[4];
      volatile	u_char numFramesDiscardedSIDMismatch[4];
      volatile	u_char numPollsTransmistted[4];
      volatile	u_char numPollAcknowledges[4];
      volatile	u_char numStatusTimeouts[4];
      volatile	u_char numNACKReceived[4];

      volatile	u_char _19[0x86];

      volatile	u_char txBuffer[0x800];
      volatile	u_char rxBuffer[0x800];

      volatile	u_char _20[0x800];
      volatile	u_char _21[0x3fb];
      volatile	u_char configStatus;
      volatile	u_char _22;
      volatile	u_char progIOCtrl;
      volatile	u_char shareMBase;
      volatile	u_char controlRegister;
};

struct arlan_conf_stru {
      int spreadingCode;
      int channelSet;
      int channelNumber;
      int scramblingDisable;
      int txAttenuation;
      int systemId; 
      int maxDatagramSize;
      int maxFrameSize;
      int maxRetries;
      int receiveMode;
      int priority;
      int rootOrRepeater;
      int SID;
      int radioNodeId;
      int registrationMode;
      int registrationFill;
      int localTalkAddress;
      int codeFormat;
      int numChannels;
      int channel1;
      int channel2;
      int channel3;
      int channel4;
      int txClear;
      int txRetries;
      int txRouting;
      int txScrambled;
      int rxParameter;
      int txTimeoutMs;
      int txAckTimeoutMs;
      int waitCardTimeout;
      int	waitTime;
      int	lParameter;
      int	_15;
      int	headerSize;
      int async;
      int retries;
      int tx_delay_ms;
      int waitReTransmitPacketMaxSize;
      int ReTransmitPacketMaxSize;
      int fastReTransCount;
      int driverRetransmissions;
      int registrationInterrupts;
      int hardwareType;
      int radioType;
      int writeRadioType;
      int writeEEPROM;
      char siteName[17];
      int measure_rate;
      int in_speed;
      int out_speed;
      int in_speed10;
      int out_speed10;
      int in_speed_max;
      int out_speed_max;
      int pre_Command_Wait;
      int rx_tweak1;
      int rx_tweak2;
      int tx_queue_len;
};

extern struct arlan_conf_stru arlan_conf[MAX_ARLANS];

struct TxParam
{
      volatile	short 		offset;
      volatile 	short 		length;
      volatile	u_char 		dest[6];
      volatile	unsigned	char clear;
      volatile	unsigned	char retries;
      volatile	unsigned	char routing;
      volatile	unsigned	char scrambled;
};

struct TxRingPoint  {
	struct TxParam txParam;
	
	
};

#define TX_RING_SIZE 2
/* Information that need to be kept for each board. */
struct arlan_private {
      struct net_device_stats stats;
      long open_time;			/* Useless example local info. */
      struct arlan_shmem * card;
      struct arlan_shmem * conf;
      struct TxParam txParam;      
      int multicastLength;
      char  multicastList[ARLAN_MAX_MULTICAST_ADDRS][6];
      int promiscModeEnabled;
      struct arlan_conf_stru * Conf;	     
      int	bad;
      int 	reset;
      long long lastReset;
      struct timer_list timer;
      struct timer_list tx_delay_timer;
      struct timer_list tx_retry_timer;
      struct timer_list rx_check_timer;
      struct semaphore card_lock;
      atomic_t 	card_users;
      atomic_t	delay_on;
      atomic_t  retr_on;
      int registrationLostCount;
      int reRegisterExp;
      int nof_tx;
      int nof_tx_ack;
      int last_nof_tx;
      int last_nof_tx_ack;
      int irq_test_done;
      int last_command_was_rx;
      struct TxParam txRing[TX_RING_SIZE];
      char reTransmitBuff[0x800];
      volatile int txLast;
      volatile int txNew;
      volatile int txOffset;
      volatile char ReTransmitRequested;
      volatile long long tx_done_delayed;
      volatile long long registrationLastSeen;
      volatile char under_command;
      volatile char under_toggle;
      volatile long long tx_last_sent;
      volatile long long tx_last_cleared;
      volatile u_char under_tx;
      volatile int 	retransmissions;
      volatile int	tx_chain_active;
      volatile int 	timer_chain_active;
      volatile int 	interrupt_ack_requested;
      volatile long	command_lock;
      volatile int	rx_command_needed;
      volatile int	tx_command_needed;
      volatile int 	waiting_command_mask;
      volatile int 	card_polling_interval;
      volatile int 	last_command_buff_free_time;
      volatile int	numResets;
      volatile int 	under_reset;
      volatile int 	under_config;
      volatile int 	rx_command_given;
      volatile long 	tx_command_given;
      volatile long	interrupt_processing_active;
      volatile long long 	last_tx_time;
      volatile long long	last_rx_time;
      volatile long long	last_rx_int_ack_time;
      int	in_bytes;
      int	out_bytes;
      int	in_time;
      int	out_time;
      int	in_time10;
      int	out_time10;
      int	in_bytes10;
      int	out_bytes10;
};



#define ARLAN_CLEAR		0x00
#define ARLAN_RESET 		0x01
#define ARLAN_CHANNEL_ATTENTION 0x02
#define ARLAN_INTERRUPT_ENABLE 	0x04
#define ARLAN_CLEAR_INTERRUPT 	0x08
#define ARLAN_POWER 		0x40
#define ARLAN_ACCESS		0x80

#define ARLAN_COM_CONF                0x01
#define ARLAN_COM_RX_ENABLE           0x03
#define ARLAN_COM_RX_ABORT            0x04
#define ARLAN_COM_TX_ENABLE           0x05
#define ARLAN_COM_TX_ABORT            0x06
#define ARLAN_COM_NOP		      0x07
#define ARLAN_COM_STANDBY             0x08
#define ARLAN_COM_ACTIVATE            0x09
#define ARLAN_COM_GOTO_SLOW_POLL      0x0a
#define ARLAN_COM_INT                 0x80


#define TXLAST(dev) (((struct arlan_private *)dev->priv)->txRing[((struct arlan_private *)dev->priv)->txLast])
#define TXHEAD(dev) (((struct arlan_private *)dev->priv)->txRing[0])
#define TXTAIL(dev) (((struct arlan_private *)dev->priv)->txRing[1])

#define TXBuffStart(dev) \
 ((int)(((struct arlan_private *)dev->priv)->card)->txBuffer) - ((int)(((struct arlan_private *)dev->priv)->card) )
#define TXBuffEnd(dev) \
 ((int)(((struct arlan_private *)dev->priv)->card)->rxBuffer) - ((int)(((struct arlan_private *)dev->priv)->card)
 
#define READSHM(to,from,atype) {\
	atype tmp;\
	memcpy_fromio(&(tmp),&(from),sizeof(atype));\
	to = tmp;\
	}

#define READSHMEM(from,atype)\
	atype from; \
	READSHM(from, arlan->from, atype);

#define WRITESHM(to,from,atype) \
	{ atype tmpSHM = from;\
	memcpy_toio(&(to),&tmpSHM,sizeof(atype));\
	}

#define DEBUGSHM(levelSHM,stringSHM,stuff,atype) \
	{	atype tmpSHM; \
		memcpy_fromio(&tmpSHM,&(stuff),sizeof(atype));\
		IFDEBUG(levelSHM) printk(stringSHM,tmpSHM);\
	}

#define WRITESHMB(to, val) \
	writeb(val,&(to))
#define READSHMB(to) \
	readb(&(to))
#define WRITESHMS(to, val) \
	writew(val,&(to))
#define READSHMS(to) \
	readw(&(to))
#define WRITESHMI(to, val) \
	writel(val,&(to))
#define READSHMI(to) \
	readl(&(to))





#define registrationBad(dev)\
   ( (   READSHMB(((struct arlan_private *)dev->priv)->card->registrationMode)    > 0) && \
     (   READSHMB(((struct arlan_private *)dev->priv)->card->registrationStatus) == 0)    )


#define readControlRegister(dev)\
 	READSHMB(((struct arlan_private *)dev->priv)->card->cntrlRegImage)

#define writeControlRegister(dev, v){\
   WRITESHMB(((struct arlan_private *)dev->priv)->card->cntrlRegImage	,((v) &0xF) );\
   WRITESHMB(((struct arlan_private *)dev->priv)->card->controlRegister	,(v) 	);}


#define arlan_interrupt_lancpu(dev) {\
   int cr;   \
   \
   priv->under_toggle++;   \
   cr = readControlRegister(dev);\
   if (cr & ARLAN_CHANNEL_ATTENTION){ \
      writeControlRegister(dev, (cr & ~ARLAN_CHANNEL_ATTENTION));\
   }else  \
      writeControlRegister(dev, (cr | ARLAN_CHANNEL_ATTENTION));\
   priv->under_toggle=0;     \
}

#define clearChannelAttention(dev){ \
   writeControlRegister(dev,readControlRegister(dev) & ~ARLAN_CHANNEL_ATTENTION);}
#define setHardwareReset(dev) {\
   writeControlRegister(dev,readControlRegister(dev) | ARLAN_RESET);}
#define clearHardwareReset(dev) {\
   writeControlRegister(dev,readControlRegister(dev) & ~ARLAN_RESET);}
#define setInterruptEnable(dev){\
   writeControlRegister(dev,readControlRegister(dev) | ARLAN_INTERRUPT_ENABLE)  ;}
#define clearInterruptEnable(dev){\
   writeControlRegister(dev,readControlRegister(dev) & ~ARLAN_INTERRUPT_ENABLE)  ;}
#define setClearInterrupt(dev){\
   writeControlRegister(dev,readControlRegister(dev) | ARLAN_CLEAR_INTERRUPT)   ;}
#define clearClearInterrupt(dev){\
   writeControlRegister(dev,readControlRegister(dev) & ~ARLAN_CLEAR_INTERRUPT);}
#define setPowerOff(dev){\
   writeControlRegister(dev,readControlRegister(dev) | (ARLAN_POWER && ARLAN_ACCESS));\
   writeControlRegister(dev,readControlRegister(dev) & ~ARLAN_ACCESS);}
#define setPowerOn(dev){\
   writeControlRegister(dev,readControlRegister(dev) & ~(ARLAN_POWER));   }
#define arlan_lock_card_access(dev){\
   writeControlRegister(dev,readControlRegister(dev) & ~ARLAN_ACCESS);}
#define arlan_unlock_card_access(dev){\
   writeControlRegister(dev,readControlRegister(dev) | ARLAN_ACCESS ); }  




#define ARLAN_COMMAND_RX		0x000001
#define ARLAN_COMMAND_NOOP		0x000002
#define ARLAN_COMMAND_NOOPINT		0x000004
#define ARLAN_COMMAND_TX		0x000008
#define ARLAN_COMMAND_CONF		0x000010
#define ARLAN_COMMAND_RESET		0x000020
#define ARLAN_COMMAND_TX_ABORT		0x000040
#define ARLAN_COMMAND_RX_ABORT		0x000080
#define ARLAN_COMMAND_POWERDOWN		0x000100
#define ARLAN_COMMAND_POWERUP		0x000200
#define ARLAN_COMMAND_SLOW_POLL 	0x000400
#define ARLAN_COMMAND_ACTIVATE 		0x000800
#define ARLAN_COMMAND_INT_ACK		0x001000
#define ARLAN_COMMAND_INT_ENABLE	0x002000
#define ARLAN_COMMAND_WAIT_NOW		0x004000
#define ARLAN_COMMAND_LONG_WAIT_NOW	0x008000
#define ARLAN_COMMAND_STANDBY		0x010000
#define ARLAN_COMMAND_INT_RACK		0x020000
#define ARLAN_COMMAND_INT_RENABLE	0x040000
#define ARLAN_COMMAND_CONF_WAIT		0x080000
#define ARLAN_COMMAND_TBUSY_CLEAR	0x100000
#define ARLAN_COMMAND_CLEAN_AND_CONF	(ARLAN_COMMAND_TX_ABORT\
					| ARLAN_COMMAND_RX_ABORT\
					| ARLAN_COMMAND_CONF)
#define ARLAN_COMMAND_CLEAN_AND_RESET   (ARLAN_COMMAND_TX_ABORT\
					| ARLAN_COMMAND_RX_ABORT\
					| ARLAN_COMMAND_RESET)


 
#define ARLAN_DEBUG_CHAIN_LOCKS		0x00001
#define ARLAN_DEBUG_RESET		0x00002
#define ARLAN_DEBUG_TIMING		0x00004
#define ARLAN_DEBUG_CARD_STATE		0x00008
#define ARLAN_DEBUG_TX_CHAIN		0x00010
#define ARLAN_DEBUG_MULTICAST		0x00020
#define ARLAN_DEBUG_HEADER_DUMP		0x00040
#define ARLAN_DEBUG_INTERRUPT		0x00080
#define ARLAN_DEBUG_STARTUP		0x00100
#define ARLAN_DEBUG_SHUTDOWN		0x00200
 
