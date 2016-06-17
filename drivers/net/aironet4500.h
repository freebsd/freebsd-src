/*
 *	 Aironet 4500 Pcmcia driver
 *
 *		Elmer Joandi, Januar 1999
 *	Copyright:	GPL
 *	
 *
 *	Revision 0.1 ,started  30.12.1998
 *
 *
 */
 

#ifndef AIRONET4500_H
#define	AIRONET4500_H
// redefined to avoid PCMCIA includes

 #include <linux/version.h>
/*#include <linux/module.h>
 #include <linux/kernel.h>
*/

/*
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/time.h>
*/
#include <linux/802_11.h>

//damn idiot PCMCIA stuff
#ifndef DEV_NAME_LEN
	#define DEV_NAME_LEN 32
#endif

struct pcmcia_junkdev_node_t {
    char		dev_name[DEV_NAME_LEN];
    u_short		major, minor;
    struct dev_node_t	*next;
};

#ifndef CS_RELEASE
typedef struct pcmcia_junkdev_node_t dev_node_t;
#endif



#include <linux/spinlock.h>


#define AWC_ERROR	-1
#define AWC_SUCCESS	0

struct awc_cis {
	unsigned char cis[0x301];
	unsigned char unknown302[0xdf];
	unsigned short configuration_register;
	unsigned short pin_replacement_register;
	unsigned short socket_and_copy_register;

};


/* timeout for transmit watchdog timer, AP default is 8 sec */
#define AWC_TX_TIMEOUT			(HZ * 8) 



/***************************  REGISTER OFFSETS *********************/
#define awc_Command_register 		0x00
#define awc_Param0_register 		0x02
#define awc_Param1_register 		0x04
#define awc_Param2_register 		0x06
#define awc_Status_register 		0x08
#define awc_Resp0_register 		0x0A
#define awc_Resp1_register 		0x0C
#define awc_Resp2_register 		0x0E
#define awc_EvStat_register 		0x30
#define awc_EvIntEn_register 		0x32
#define awc_EvAck_register 		0x34
#define awc_SWSupport0_register 	0x28
#define awc_SWSupport1_register 	0x2A
#define awc_SWSupport2_register 	0x2C
#define awc_SWSupport3_register 	0x2E
#define awc_LinkStatus_register 	0x10
// Memory access  RID FID
#define awc_Select0_register 		0x18
#define awc_Offset0_register 		0x1C
#define awc_Data0_register 		0x36
#define awc_Select1_register 		0x1A
#define awc_Offset1_register 		0x1E
#define awc_Data1_register 		0x38
//
#define awc_RxFID_register 		0x20
#define awc_TxAllocFID_register 	0x22
#define awc_TxComplFID_register 	0x24
#define awc_AuxPage_register 		0x3A
#define awc_AuxOffset_register 		0x3C
#define awc_AuxData_register 		0x3E


struct awc_bap {
	u16 select;
	u16 offset;
	u16 data;
	volatile int lock;
	volatile int	status;
	struct semaphore sem;
	spinlock_t spinlock;
	unsigned long flags;
};



#define AWC_COMMAND_STATE_WAIT_CMD_BUSY		1
#define AWC_COMMAND_STATE_WAIT_CMD_ACK		2
#define AWC_COMMAND_STATE_WAIT_BAP_BUSY		3
#define AWC_COMMAND_STATE_BAP_NOT_SET		4
#define AWC_COMMAND_STATE_BAP_SET		5

struct awc_command {
	volatile int		state;
	volatile int		lock_state;
	struct net_device *		dev;
	struct awc_private *	priv;
	u16			port;
	struct awc_bap * 	bap;
	u16			command;
	u16			par0;
	u16			par1;
	u16			par2;
	u16			status;
	u16			resp0;
	u16			resp1;
	u16			resp2;
	u16			rid;
	u16			offset;
	u16			len;
	void *			buff;

};




#define DOWN(a) down_interruptible( a ) ; 
//	if (in_interrupt()) { down_interruptible( a ) ; } else printk("semaphore DOWN in interrupt tried \n");
#define UP(a)   up( a ) ;
//	if (in_interrupt()) {up( a ) ; } else printk("semaphore UP in interrupt tried \n");

/*	if (!in_interrupt())\
	printk("bap lock under cli but not in int\n");\
*/

#define AWC_LOCK_COMMAND_ISSUING(a) spin_lock_irqsave(&a->command_issuing_spinlock,a->command_issuing_spinlock_flags);
#define AWC_UNLOCK_COMMAND_ISSUING(a) spin_unlock_irqrestore(&a->command_issuing_spinlock,a->command_issuing_spinlock_flags);

#define AWC_BAP_LOCK_UNDER_CLI_REAL(cmd) \
 	if (!cmd.priv) {\
		printk(KERN_CRIT "awc4500: no priv present in command !");\
	}\
	cmd.bap = &(cmd.priv->bap1);\
	if (both_bap_lock)\
	spin_lock_irqsave(&cmd.priv->both_bap_spinlock,cmd.priv->both_bap_spinlock_flags);\
	if (cmd.bap){\
		spin_lock_irqsave(&(cmd.bap->spinlock),cmd.bap->flags);\
		cmd.bap->lock++;\
		if (cmd.bap->lock > 1)\
			printk("Bap 1 lock high\n");\
		cmd.lock_state |= AWC_BAP_LOCKED;\
	}

#define AWC_BAP_LOCK_NOT_CLI_REAL(cmd) {\
	if (in_interrupt())\
	printk("bap lock not cli in int\n");\
 	if (!cmd.priv) {\
		printk(KERN_CRIT "awc4500: no priv present in command,lockup follows !");\
	}\
	cmd.bap = &(cmd.priv->bap0);\
	if (both_bap_lock)\
		spin_lock_irqsave(&cmd.priv->both_bap_spinlock,cmd.priv->both_bap_spinlock_flags);\
	spin_lock_irqsave(&(cmd.bap->spinlock),cmd.bap->flags);\
	DOWN(&(cmd.priv->bap0.sem));\
	cmd.bap->lock++;\
	if (cmd.bap->lock > 1)\
		printk("Bap 0 lock high\n");\
	cmd.lock_state |= AWC_BAP_SEMALOCKED;\
}

#define AWC_BAP_LOCK_NOT_CLI_CLI_REAL(cmd) {\
	cmd.bap = &(cmd.priv->bap0);\
	if (both_bap_lock)\
		spin_lock_irqsave(&cmd.priv->both_bap_spinlock,cmd.priv->both_bap_spinlock_flags);\
	spin_lock_irqsave(&(cmd.bap->spinlock),cmd.bap->flags);\
	cmd.bap->lock++;\
	if (cmd.bap->lock > 1)\
		printk("Bap 0 lock high\n");\
	cmd.lock_state |= AWC_BAP_LOCKED;\
}

#define BAP_LOCK_ANY(cmd)\
	if (in_interrupt())	AWC_BAP_LOCK_NOT_CLI_CLI_REAL(cmd)\
	else AWC_BAP_LOCK_NOT_CLI_REAL(cmd)
	
#define AWC_BAP_LOCK_NOT_CLI(cmd)	BAP_LOCK_ANY(cmd)
#define AWC_BAP_LOCK_UNDER_CLI(cmd)	AWC_BAP_LOCK_UNDER_CLI_REAL(cmd)
/*
	if (!cmd.priv->bap1.lock ) {BAP_LOCK_ANY(cmd);}\
	else AWC_BAP_LOCK_NOT_CLI_CLI_REAL(cmd);
*/	
#define AWC_BAP_LOCKED 		0x01
#define AWC_BAP_SEMALOCKED 	0x02

#define AWC_BAP_BUSY	0x8000
#define AWC_BAP_ERR	0x4000
#define AWC_BAP_DONE	0x2000

#define AWC_CLI		1
#define AWC_NOT_CLI 	2

/*#define WAIT61x3	inb(0x61);\
         		inb(0x61);\
                    	inb(0x61);
*/ 
#define WAIT61x3 	udelay(bap_sleep)                  	

#define AWC_INIT_COMMAND(context, a_com, a_dev,a_cmmand,a_pr0, a_rid, a_offset, a_len, a_buff) {\
	memset(&a_com,0,sizeof(a_com) );\
	a_com.dev = a_dev;\
	a_com.priv = a_dev->priv;\
	a_com.port = a_dev->base_addr;\
	a_com.bap = NULL;\
	a_com.command = a_cmmand;\
	a_com.par0 = a_pr0;\
	a_com.rid = a_rid;\
	a_com.offset = a_offset;\
	a_com.len = a_len;\
	a_com.buff = a_buff;\
	a_com.lock_state = 0;\
};

/* väga veider asi järgnevast 
 makrost välja jäetud	if (cmd.bap) AWC_IN((cmd.bap)->data);\
*/

#define AWC_BAP_UNLOCK(com) { \
	if (com.bap){ \
		if ( (com.lock_state & AWC_BAP_SEMALOCKED) &&\
		     (com.lock_state & AWC_BAP_LOCKED) ){\
		     	printk("Both Sema and simple lock \n");\
		}\
		if ( com.lock_state & AWC_BAP_SEMALOCKED ){\
			 com.bap->lock--; \
			 com.lock_state &= ~AWC_BAP_SEMALOCKED;\
			 UP(&(com.bap->sem)); \
			 spin_unlock_irqrestore(&(cmd.bap->spinlock),cmd.bap->flags);\
		} else if (com.lock_state & AWC_BAP_LOCKED){\
			 com.bap->lock--; \
			 com.lock_state &= ~AWC_BAP_LOCKED;\
			 spin_unlock_irqrestore(&(cmd.bap->spinlock),cmd.bap->flags);\
		}\
	}\
	if (both_bap_lock)\
		spin_unlock_irqrestore(&cmd.priv->both_bap_spinlock,cmd.priv->both_bap_spinlock_flags);\
}

#define AWC_RELEASE_COMMAND(com) {\
		AWC_BAP_UNLOCK(cmd);\
	}



#define awc_manufacturer_code 	0x015F
#define awc_product_code	0x0005


#define awc_write(base,register,u16value) outw(u16value, (base)+(register))
#define awc_read(base,register)           inw((base)+(register))
#define AWC_OUT(base,val)		outw(val, base)
#define AWC_IN(base)			inw(base)


#define awc_read_response(cmd)	{	\
	cmd->status=awc_read(cmd->port,awc_Status_register);\
	cmd->resp0=awc_read(cmd->port,awc_Resp0_register);\
	cmd->resp1=awc_read(cmd->port,awc_Resp1_register);\
	cmd->resp2=awc_read(cmd->port,awc_Resp2_register);\
};

#define awc_command_busy(base)		(awc_read(base,awc_Command_register) & 0x8000)
#define awc_command_read(base)		awc_read(base,awc_Command_register)
#define awc_command_write(base,cmd)	awc_write(base,awc_Command_register,cmd) 
#define awc_event_status_Awake(base)	(awc_read(base,awc_EvStat_register) & 0x0100)
#define awc_event_status_Link(base)	(awc_read(base,awc_EvStat_register) & 0x0080)
#define awc_event_status_Cmd(base)	(awc_read(base,awc_EvStat_register) & 0x0010)
#define awc_event_status_Alloc(base)	(awc_read(base,awc_EvStat_register) & 0x0008)
#define awc_event_status_TxExc(base)	(awc_read(base,awc_EvStat_register) & 0x0004)
#define awc_event_status_Tx(base)	(awc_read(base,awc_EvStat_register) & 0x0002)
#define awc_event_status_TxResp(base)	(awc_read(base,awc_EvStat_register) & 0x0006)
#define awc_event_status_Rx(base)	(awc_read(base,awc_EvStat_register) & 0x0001)
#define awc_event_status(base)		(awc_read(base,awc_EvStat_register))

#define awc_Link_Status(base)		awc_read(base,awc_LinkStatus_register)

#define awc_Rx_Fid(base)		awc_read(base,awc_RxFID_register)
#define awc_Tx_Allocated_Fid(base)	awc_read(base,awc_TxAllocFID_register)
#define awc_Tx_Compl_Fid(base)		awc_read(base,awc_TxComplFID_register)

#define awc_event_ack_ClrStckCmdBsy(base) awc_write(base,awc_EvAck_register, 0x4000)
#define awc_event_ack_WakeUp(base)	awc_write(base,awc_EvAck_register, 0x2000)
#define awc_event_ack_Awaken(base)	awc_write(base,awc_EvAck_register, 0x0100)
#define awc_event_ack_Link(base)	awc_write(base,awc_EvAck_register, 0x0080)
#define awc_event_ack_Cmd(base)		awc_write(base,awc_EvAck_register, 0x0010)
#define awc_event_ack_Alloc(base)	awc_write(base,awc_EvAck_register, 0x0008)
#define awc_event_ack_TxExc(base)	awc_write(base,awc_EvAck_register, 0x0004)
#define awc_event_ack_Tx(base)		awc_write(base,awc_EvAck_register, 0x0002)
#define awc_event_ack_Rx(base)		awc_write(base,awc_EvAck_register, 0x0001)

#define awc_event_ack(base,ints)	awc_write(base,awc_EvAck_register,ints)

#define awc_ints_enabled(base)		(awc_read(base,awc_EvIntEn_register))
#define awc_ints_enable(base,ints)	awc_write(base,awc_EvIntEn_register,ints)



/************************  	RX TX 	BUFF	************************/


struct aironet4500_radio_rx_header {
	u32	RxTime;
	u16	Status;
	u16	PayloadLength;
	u8	Reserved0;
	u8	RSSI;
	u8	Rate;
	u8	Frequency;
	u8	Rx_association_count;
	u8 	Reserved1[3];
	u8	PLCP_header[4];

};


struct aironet4500_radio_tx_header {
	u32	SWSupport;
	u16	Status;
	#define aironet4500_tx_status_max_retries	0x0002
	#define aironet4500_tx_status_lifetime_exceeded	0x0004
	#define aironet4500_tx_status_AID_failure	0x0008
	#define aironet4500_tx_status_MAC_disabled	0x0010
	#define aironet4500_tx_status_association_lost	0x0020
	u16	PayloadLength;
	u16	TX_Control;
	#define aironet4500_tx_control_tx_ok_event_enable 	0x0002
	#define aironet4500_tx_control_tx_fail_event_enable 	0x0004
	#define aironet4500_tx_control_header_type_802_11 	0x0008
	#define aironet4500_tx_control_payload_type_llc 	0x0010
	#define aironet4500_tx_control_no_release 		0x0020
	#define aironet4500_tx_control_reuse_fid \
		(aironet4500_tx_control_tx_ok_event_enable |\
		 aironet4500_tx_control_tx_fail_event_enable |\
		  aironet4500_tx_control_no_release)
	#define aironet4500_tx_control_no_retries 		0x0040
	#define aironet4500_tx_control_clear_AID 		0x0080
	#define aironet4500_tx_control_strict_order 		0x0100
	#define aironet4500_tx_control_use_rts 			0x0200
	u16	AID;
	u8	Tx_Long_Retry;
	u8	Tx_Short_Retry;
	u8	tx_association_count;
	u8	tx_bit_rate;
	#define	aironet4500_tx_bit_rate_automatic 0
	#define aironet4500_tx_bit_rate_500kbps	1
	#define aironet4500_tx_bit_rate_1Mbps	2
	#define aironet4500_tx_bit_rate_2Mbps	4
	u8	Max_Long_Retry;
	u8	Max_Short_Retry;
	u8	Reserved0[2];
};


struct aironet4500_rx_fid {

	u16						rid;
	struct aironet4500_radio_rx_header 		radio_rx;
	struct ieee_802_11_header 	   		ieee_802_11;
	u16 						gap_length;
	struct ieee_802_3_header	   		ieee_802_3;
	u8					*	payload;
};


struct aironet4500_tx_fid {

	u16						fid;
	u16						fid_size;
	struct aironet4500_radio_tx_header 		radio_tx;
	struct ieee_802_11_header 	   		ieee_802_11;
	u16 						gap_length;
	#define aironet4500_gap_len_without_802_3	6
	#define aironet4500_gap_len_with_802_3		0
	struct ieee_802_3_header	   		ieee_802_3;
	u8					*	payload;	
};

struct awc_fid {

	u32	type;
	#define p80211_llc_snap		0x0100
	#define p80211_8021H		0x0200
	#define p80211_8022		0x0400
	#define p80211_8023		0x0800
	#define p80211_snap_8021H	0x1000
	#define p80211copy_path_skb	0x2000

	u8	priority;
	u8	busy;
	
	#define awc_tx_fid_complete_read 0x01
	u16	state;
	union {
		struct aironet4500_tx_fid tx;
		struct aironet4500_rx_fid rx;	
	} u;
	
	struct ieee_802_11_snap_header snap;
	struct ieee_802_11_802_1H_header bridge;
	u16			bridge_size;
	struct ieee_802_11_802_2_header p8022;

	u16			pkt_len;
	u8	* mac;
	struct sk_buff *	skb;
	long long		transmit_start_time;
	struct awc_fid	*	next;
	struct awc_fid	*	prev;
	
};



struct awc_fid_queue {


	struct awc_fid * head;
	struct awc_fid * tail;
	int	size;
	spinlock_t spinlock;
};


extern  __inline__ void
awc_fid_queue_init(struct awc_fid_queue * queue){

	unsigned long flags;
	memset(queue,0, sizeof(struct awc_fid_queue));	
	spin_lock_init(&queue->spinlock);
	spin_lock_irqsave(&queue->spinlock,flags);
	queue->head = NULL;
	queue->tail = NULL;
	queue->size = 0;
	spin_unlock_irqrestore(&queue->spinlock,flags);	
};

static inline void
awc_fid_queue_push_tail(	struct awc_fid_queue * 	queue,
				struct awc_fid *	fid){

	unsigned long flags;

	spin_lock_irqsave(&queue->spinlock,flags);	
	
	fid->prev = queue->tail;
	fid->next = NULL;
	
	if (queue->tail){
		queue->tail->next = fid;
	} 	
	queue->tail  = fid;
	
	if (!queue->head)
		queue->head = fid;
	queue->size++;

	spin_unlock_irqrestore(&queue->spinlock,flags);
		
};


static inline void
awc_fid_queue_push_head(	struct awc_fid_queue * 	queue,
				struct awc_fid *	fid){

	unsigned long flags;

	spin_lock_irqsave(&queue->spinlock,flags);	
	
	fid->prev = NULL;
	fid->next = queue->head;
	
	if (queue->head){
		queue->head->prev = fid;
	} 	
	queue->head  = fid;
	
	if (!queue->tail)
		queue->tail = fid;
	queue->size++;
	
	spin_unlock_irqrestore(&queue->spinlock,flags);
};



static inline void
awc_fid_queue_rm(		struct awc_fid_queue * 	queue,
				struct awc_fid *	fid){


	if (fid->prev) {
		fid->prev->next = fid->next;
	};

	if (fid->next) {
		fid->next->prev = fid->prev;
	};
	
	if (fid == queue->tail) {
		queue->tail = fid->prev;
	};
	if (fid == queue->head) {
		queue->head = fid->next;
	};
	fid->next = NULL;
	fid->prev = NULL;
	queue->size--;
	if (queue->size ==0 ){
		queue->tail = NULL;
		queue->head = NULL;
	}		
};

static inline void
awc_fid_queue_remove(		struct awc_fid_queue * 	queue,
				struct awc_fid *	fid){
	unsigned long flags;
	spin_lock_irqsave(&queue->spinlock,flags);	
	
	awc_fid_queue_rm(queue,fid);
	
	spin_unlock_irqrestore(&queue->spinlock,flags);
	
};



static inline struct awc_fid * 
awc_fid_queue_pop_head(		struct awc_fid_queue * 	queue){

	unsigned long flags;
	struct awc_fid * fid;
	
	spin_lock_irqsave(&queue->spinlock,flags);	

	fid = queue->head;
	if (fid)
		awc_fid_queue_rm(queue,fid);
		
	spin_unlock_irqrestore(&queue->spinlock,flags);
	
	return fid;
};




static inline struct awc_fid * 
awc_fid_queue_pop_tail(		struct awc_fid_queue * 	queue){

	unsigned long flags;
	struct awc_fid * fid;
	
	spin_lock_irqsave(&queue->spinlock,flags);	

	fid = queue->tail;
	if (fid)
			awc_fid_queue_rm(queue,fid);
	
	spin_unlock_irqrestore(&queue->spinlock,flags);
	
	return fid;
};



#define AWC_TX_HEAD_SIZE		0x44
#define AWC_TX_ALLOC_SMALL_SIZE 	200
#define AWC_RX_BUFFS			50


/*****************************     	RID & CONFIG 	***********************/

struct awc_config{
    unsigned short    Len;                                /* sizeof(PC4500_CONFIG) */
    unsigned short    OperatingMode;                      /* operating mode        */

    #define           MODE_STA_IBSS                0
    #define           MODE_STA_ESS                 1
    #define           MODE_AP                      2
    #define           MODE_AP_RPTR                 3
    #define           MODE_ETHERNET_HOST           (0<<8)    /* rx payloads converted */
    #define           MODE_LLC_HOST                (1<<8)    /* rx payloads left as is */
    #define           MODE_AIRONET_EXTEND          (1<<9)    /* enable Aironet extenstions */
    #define           MODE_AP_INTERFACE            (1<<10) /* enable ap interface extensions */
    unsigned short    ReceiveMode;                        /* receive mode */
    #define           RXMODE_BC_MC_ADDR            0
    #define           RXMODE_BC_ADDR               1         /* ignore multicasts */
    #define           RXMODE_ADDR                  2         /* ignore multicast and broadcast */
    #define           RXMODE_RFMON                 3         /* wireless monitor mode */
    #define           RXMODE_RFMON_ANYBSS 4
    #define           RXMODE_LANMON                5         /* lan style monitor -- data packets only */
    #define           RXMODE_DISABLE_802_3_HEADER  0x100    /* disables 802.3 header on rx */

    unsigned short    FragmentThreshold;
    unsigned short    RtsThreshold;
    unsigned char     StationMacAddress[6];
    unsigned char     Rates[8];
    unsigned short    ShortRetryLimit;
    unsigned short    LongRetryLimit;
    unsigned short    TxLifetime;                         /* in kusec */
    unsigned short    RxLifetime;                         /* in kusec */
    unsigned short    Stationary;
    unsigned short    Ordering;
    unsigned short    DeviceType;                         /* for overriding device type */
    unsigned short    _reserved1[5];                         /*---------- Scanning/Associating ----------*/
    unsigned short    ScanMode;
    #define           SCANMODE_ACTIVE              0
    #define           SCANMODE_PASSIVE             1
    #define           SCANMODE_AIROSCAN            2
    unsigned short    ProbeDelay;                         /* in kusec */
    unsigned short    ProbeEnergyTimeout;                 /* in kusec */
    unsigned short    ProbeResponseTimeout;
    unsigned short    BeaconListenTimeout;
    unsigned short    JoinNetTimeout;
    unsigned short    AuthenticationTimeout;
    unsigned short    AuthenticationType;
    #define           AUTH_OPEN                    1
    #define           AUTH_SHAREDKEY               2
    #define           AUTH_EXCLUDENONWEP           4
    unsigned short    AssociationTimeout;
    unsigned short    SpecifiedApTimeout;
    unsigned short    OfflineScanInterval;
    unsigned short    OfflineScanDuration;
    unsigned short    LinkLossDelay;
    unsigned short    MaxBeaconLostTime;
    unsigned short    RefreshInterval;
   #define           DISABLE_REFRESH           0xFFFF
   unsigned short    _reserved1a[1];                      /*---------- Power save operation ----------*/
   unsigned short    PowerSaveMode;
   #define           POWERSAVE_CAM             0
   #define           POWERSAVE_PSP             1
   #define           POWERSAVE_PSP_CAM         2
   unsigned short    SleepForDtims;
   unsigned short    ListenInterval;
   unsigned short    FastListenInterval;
   unsigned short    ListenDecay;
   unsigned short    FastListenDelay;
   unsigned short    _reserved2[2];                       /*---------- Ap/Ibss config items ----------*/
   unsigned short    BeaconPeriod;
   unsigned short    AtimDuration;
   unsigned short    HopPeriod;
   unsigned short    ChannelSet;
   unsigned short    Channel;
   unsigned short    DtimPeriod;
   unsigned short    _reserved3[2];                       /*---------- Radio configuration ----------*/
   unsigned short    RadioType;
   #define           RADIOTYPE_DEFAULT         0
   #define           RADIOTYPE_802_11          1
   #define           RADIOTYPE_LEGACY          2
   unsigned char     u8RxDiversity;
   unsigned char     u8TxDiversity;
   unsigned short    TxPower;
   #define           TXPOWER_DEFAULT           0
   unsigned short    RssiThreshold;
   #define           RSSI_DEFAULT              0
   unsigned short    RadioSpecific[4];                 /*---------- Aironet Extensions ----------*/
   unsigned char     NodeName[16];
   unsigned short    ArlThreshold;
   unsigned short    ArlDecay;
   unsigned short    ArlDelay;
   unsigned short    _reserved4[1];                       /*---------- Aironet Extensions ----------*/
   unsigned short    MagicAction;
   #define           MAGIC_ACTION_STSCHG       1
   #define           MACIC_ACTION_RESUME       2
   #define           MAGIC_IGNORE_MCAST        (1<<8)
   #define           MAGIC_IGNORE_BCAST        (1<<9)
   #define           MAGIC_SWITCH_TO_PSP       (0<<10)
   #define           MAGIC_STAY_IN_CAM         (1<<10)
};



struct awc_SSID {
	u16 	lenght;
	u8	SSID[32];
};

struct awc_SSIDs {
	u16 	ridLen;
	struct awc_SSID SSID[3];

};

struct awc_fixed_APs{
	u16	ridLen;
	u8	AP[4][6];
};

struct awc_driver_name{
	u16	ridLen;
	u8	name[16];
};

struct awc_encapsulation{
	u16 	etherType;
	u16	Action;
};

struct awc_enc_trans{
	u16				ridLen;
	struct awc_encapsulation 	rules[8];
};

struct awc_wep_key {
	u16	ridLen;
	u16	KeyIndex;
	u8	Address[6];
	u16	KeyLen;
	u8	Key[16];
};

struct awc_modulation {
	u16	ridLen;
	u16	Modulation;
};

struct awc_cap{
	u16		ridLen;
	u8		OUI[3];
	u8		ProductNum[3];
	u8		ManufacturerName[32];
	u8		ProductName[16];
	u8		ProductVersion[8];
	u8		FactoryAddress[6];
	u8		AironetAddress[6];
	u16		RadioType;
	u16		RegDomain;
	u8		Callid[6];
	u8		SupportedRates[8];
	u8		RxDiversity;
	u8		TxDiversity;
	u16		TxPowerLevels[8];
	u16		HardwareVersion;
	u16		HardwareCapabilities;
	u16		TemperatureRange;
	u16		SoftwareVersion;
	u16		SoftwareSubVersion;
	u16		InterfaceVersion;
	u16		SoftwareCapabilities;
	u8		BootBlockVersionMajor;
	u8              BootBlockVersionMinor;
	        
};


struct awc_status{
	u16	ridLen;
	u8	MacAddress[6];
	u16	OperationalMode;
	u16	ErrorCode;
	u16	CurrentSignalQuality;
	u16	SSIDlength;
	u8	SSID[32];
	u8	ApName[16];
	u8	CurrentBssid[32];
	u8	PreviousBSSIDs[3][6];
	u16	BeaconPeriod;
	u16	DtimPeriod;
	u16	AtimDuration;
	u16	HopPeriod;
	u16	ChannelSet;
	u16	Channel;

	u16	HopsToBackbone;
	u16	ApTotalLoad;
	u16	OurGeneratedLoad;
	u16	AccumulatedArl;
	
};


struct awc_AP{
	u16	ridLen;
	u16	TIM_Addr;
	u16	Airo_Addr;
};

struct awc_Statistics_32 {

	u32	RidLen;
	u32	RxOverrunErr;
	u32	RxPlcpCrcErr;
	u32	RxPlcpFormat;
	u32	RxPlcpLength;
	u32	RxMacCrcErr;
	u32	RxMacCrcOk;
	u32	RxWepErr;
	u32	RxWepOk;
	u32	RetryLong;
	u32	RetryShort;
	u32	MaxRetries;
	u32	NoAck;

	u32	NoCts;
	u32	RxAck;
	u32	RxCts;
	u32	TxAck;
	u32	TxRts;
	u32	TxCts;
	u32	TxMc;
	u32	TxBc;
	u32	TxUcFrags;
	u32	TxUcPackets;
	u32	TxBeacon;
	u32	RxBeacon;
	u32	TxSinColl;
	u32	TxMulColl;
	u32	DefersNo;
	u32	DefersProt;
	u32	DefersEngy;
	u32	DupFram;
	u32	RxFragDisc;
	u32	TxAged;
	u32	RxAged;
	u32	LostSync_Max;
	u32	LostSync_Mis;
	u32	LostSync_Arl;
	u32	LostSync_Dea;
	u32	LostSync_Disa;
	u32	LostSync_Tsf;
	u32	HostTxMc;
	u32	HostTxBc;
	u32	HostTxUc;
	u32	HostTxFail;
	u32	HostRxMc;
	u32	HostRxBc;
	u32	HostRxUc;
	u32	HostRxDiscar;
	u32	HmacTxMc;
	u32	HmacTxBc;
	u32	HmacTxUc;
	u32	HmacTxFail;
	u32	HmacRxMc;
	u32	HmacRxBc;
	u32	HmacRxUc;
	u32	HmacRxDisca;
	u32	HmacRxAcce;
	u32	SsidMismatch;
	u32	ApMismatch;
	u32	RatesMismatc;
	u32	AuthReject;
	u32	AuthTimeout;
	u32	AssocReject;
	u32	AssocTimeout;
	u32	NewReason;
	u32	AuthFail_1;
	u32	AuthFail_2;
	u32	AuthFail_3;
	u32	AuthFail_4;
	u32	AuthFail_5;
	u32	AuthFail_6;
	u32	AuthFail_7;
	u32	AuthFail_8;
	u32	AuthFail_9;
	u32	AuthFail_10;
	u32	AuthFail_11;
	u32	AuthFail_12;
	u32	AuthFail_13;
	u32	AuthFail_14;
	u32	AuthFail_15;
	u32	AuthFail_16;
	u32	AuthFail_17;
	u32	AuthFail_18;
	u32	AuthFail_19;
	u32	RxMan;
	u32	TxMan;
	u32	RxRefresh;
	u32	TxRefresh;
	u32	RxPoll;
	u32	TxPoll;
	u32	HostRetries;
	u32	LostSync_HostReq;
	u32	HostTxBytes;
	u32	HostRxBytes;
	u32	ElapsedUsec;
	u32	ElapsedSec;
	u32	LostSyncBett;
};

struct awc_Statistics_16 {

	u16	RidLen;
	u16	RxOverrunErr;
	u16	RxPlcpCrcErr;
	u16	RxPlcpFormat;
	u16	RxPlcpLength;
	u16	RxMacCrcErr;
	u16	RxMacCrcOk;
	u16	RxWepErr;
	u16	RxWepOk;
	u16	RetryLong;
	u16	RetryShort;
	u16	MaxRetries;
	u16	NoAck;
	u16	NoCts;
	u16	RxAck;
	u16	RxCts;
	u16	TxAck;
	u16	TxRts;
	u16	TxCts;
	u16	TxMc;
	u16	TxBc;
	u16	TxUcFrags;
	u16	TxUcPackets;
	u16	TxBeacon;
	u16	RxBeacon;
	u16	TxSinColl;
	u16	TxMulColl;
	u16	DefersNo;
	u16	DefersProt;
	u16	DefersEngy;
	u16	DupFram;
	u16	RxFragDisc;
	u16	TxAged;
	u16	RxAged;
	u16	LostSync_Max;
	u16	LostSync_Mis;
	u16	LostSync_Arl;
	u16	LostSync_Dea;
	u16	LostSync_Disa;
	u16	LostSync_Tsf;
	u16	HostTxMc;
	u16	HostTxBc;
	u16	HostTxUc;
	u16	HostTxFail;
	u16	HostRxMc;
	u16	HostRxBc;
	u16	HostRxUc;
	u16	HostRxDiscar;
	u16	HmacTxMc;
	u16	HmacTxBc;
	u16	HmacTxUc;
	u16	HmacTxFail;
	u16	HmacRxMc;
	u16	HmacRxBc;
	u16	HmacRxUc;
	u16	HmacRxDisca;
	u16	HmacRxAcce;
	u16	SsidMismatch;
	u16	ApMismatch;
	u16	RatesMismatc;
	u16	AuthReject;
	u16	AuthTimeout;
	u16	AssocReject;
	u16	AssocTimeout;
	u16	NewReason;
	u16	AuthFail_1;
	u16	AuthFail_2;
	u16	AuthFail_3;
	u16	AuthFail_4;
	u16	AuthFail_5;
	u16	AuthFail_6;
	u16	AuthFail_7;
	u16	AuthFail_8;
	u16	AuthFail_9;
	u16	AuthFail_10;
	u16	AuthFail_11;
	u16	AuthFail_12;
	u16	AuthFail_13;
	u16	AuthFail_14;
	u16	AuthFail_15;
	u16	AuthFail_16;
	u16	AuthFail_17;
	u16	AuthFail_18;
	u16	AuthFail_19;
	u16	RxMan;
	u16	TxMan;
	u16	RxRefresh;
	u16	TxRefresh;
	u16	RxPoll;
	u16	TxPoll;
	u16	HostRetries;
	u16	LostSync_HostReq;
	u16	HostTxBytes;
	u16	HostRxBytes;
	u16	ElapsedUsec;
	u16	ElapsedSec;
	u16	LostSyncBett;
};


#define AWC_TXCTL_TXOK 		(1<<1)	/* report if tx is ok */
#define AWC_TXCTL_TXEX 		(1<<2)	/* report if tx fails */
#define AWC_TXCTL_802_3		(0<<3)	/* 802.3 packet */
#define AWC_TXCTL_802_11    	(1<<3)	/* 802.11 mac packet */
#define AWC_TXCTL_ETHERNET  	(0<<4)	/* payload has ethertype */
#define AWC_TXCTL_LLC  		(1<<4)	/* payload is llc */
#define AWC_TXCTL_RELEASE   	(0<<5)	/* release after completion */
#define AWC_TXCTL_NORELEASE 	(1<<5)	/* on completion returns to host */


/************************* LINK STATUS STUFF *******************/

#define	awc_link_status_loss_of_sync_missed_beacons	0x8000
#define	awc_link_status_loss_of_sync_max_retries 	0x8001
#define	awc_link_status_loss_of_sync_ARL_exceed  	0x8002
#define	awc_link_status_loss_of_sync_host_request 	0x8003
#define	awc_link_status_loss_of_sync_TSF_sync		0x8004
#define	awc_link_status_deauthentication		0x8100
#define	awc_link_status_disassociation			0x8200
#define	awc_link_status_association_failed		0x8400
#define	awc_link_status_authentication_failed		0x0300
#define	awc_link_status_associated			0x0400

struct awc_strings {
	int	par;
	unsigned int	mask;
	const char * string;

};

#define awc_link_status_strings {\
{awc_link_status_loss_of_sync_missed_beacons,	0xFFFF,"Loss of sync -- missed beacons"},\
{awc_link_status_loss_of_sync_max_retries,	0xFFFF,"Loss of sync -- max retries"},\
{awc_link_status_loss_of_sync_ARL_exceed,	0xFFFF,"Loss of sync -- average retry level (ARL) exceeded"},\
{awc_link_status_loss_of_sync_host_request,	0xFFFF,"Loss of sync -- host request"},\
{awc_link_status_loss_of_sync_TSF_sync,		0xFFFF,"Loss of sync -- TSF synchronization"},\
{awc_link_status_deauthentication,		0xFF00,"Deauthentication "},\
{awc_link_status_disassociation,		0xFF00,"Disassocation "},\
{awc_link_status_association_failed ,		0xFF00,"Association failed "},\
{awc_link_status_authentication_failed,		0xFF00,"Authentication failure"},\
{awc_link_status_associated,			0xFFFF,"Associated "},\
{0,0,NULL}\
} 


/****************************** COMMANDS and DEFAULTS and STATUSES ***********/

/****************************** COMMANDS */


// Command definitions




#define awc4500wout(base, com, p0,p1,p2) {\
	awc_write(base,awc_Param0_register, p0);\
	awc_write(base,awc_Param1_register, p1);\
	awc_write(base,awc_Param2_register, p2);\
	WAIT61x3;\
	awc_write(base,awc_Command_register, com);\
	WAIT61x3;\
}
#define awc_wout(cmd, com, p0,p1,p2) {\
	awc_write(base,awc_Param0_register, p0);\
	awc_write(base,awc_Param1_register, p1);\
	awc_write(base,awc_Param2_register, p2);\
	WAIT61x3;\
	awc_write(base,awc_Command_register, com);\
	WAIT61x3;\
}


#define awc_command_NOP(cmd)			awc_wout( cmd,0x0000,0,0,0) // 	NOP
#define awc_command_Enable_All(cmd)		awc_wout( cmd,0x0001,0,0,0) // 	Enable
#define awc_command_Enable_MAC(cmd)		awc_wout( cmd,0x0101,0,0,0) // 	Enable Mac
#define awc_command_Enable_Rx(cmd)		awc_wout( cmd,0x0201,0,0,0) // 	Enable Rx
#define awc_command_Disable_MAC(cmd)		awc_wout( cmd,0x0002,0,0,0) // 	Disable
#define awc_command_Sync_Loss(cmd)		awc_wout( cmd,0x0003,0,0,0) // 	Force a Loss of Sync
#define awc_command_Soft_Reset(cmd)		awc_wout( cmd,0x0004,0,0,0) // 	Firmware Restart (soft reset)
#define awc_command_Host_Sleep(cmd)		awc_wout( cmd,0x0005,0,0,0) // 	Host Sleep (must be issued as 0x0085)
#define awc_command_Magic_Packet(cmd)		awc_wout( cmd,0x0006,0,0,0) // 	Magic Packet
#define awc_command_Read_Configuration(cmd)	awc_wout( cmd,0x0008,0,0,0) // 	Read the Configuration from nonvolatile  storage
#define awc_command_Allocate_TX_Buff(cmd,size)	awc_wout( cmd,0x000A,size,0,0) // 	Allocate Transmit Buffer
#define awc_command_TX(cmd,FID)			awc_wout( cmd,0x000B,FID ,0,0) // 	Transmit
#define awc_command_Deallocate(cmd,FID)		awc_wout( cmd,0x000C,FID ,0,0) // 	Deallocate
#define awc_command_NOP2(cmd)			awc_wout( cmd,0x0010,0,0,0) // 	NOP (same as 0x0000)
#define awc_command_Read_RID(cmd,RID)		awc_wout( cmd,0x0021,RID ,0,0) // 	Read RID
#define awc_command_Write_RID(cmd,RID)		awc_wout( cmd,0x0121,RID ,0,0) // 	Write RID
#define awc_command_Allocate_Buff(cmd,size)	awc_wout( cmd,0x0028,size,0,0) // 	Allocate Buffer
#define awc_command_PSP_Nodes(cmd)		awc_wout( cmd,0x0030,0,0,0) // 	PSP nodes (AP only)
#define awc_command_Set_Phy_register(cmd,phy_register,clear_bits, set_bits)\
							awc_wout( cmd,0x003E,phy_register,clear_bits, set_bits) // 	Set PHY register
#define awc_command_TX_Test(cmd,command, frequency, pattern)		awc_wout( cmd,0x003F,command, frequency, pattern) // 	Transmitter Test
#define awc_command_RX_Test(cmd)		awc_wout( cmd,0x013F,0,0,0) // 	RX Test
#define awc_command_Sleep(cmd)			awc_wout( cmd,0x0085,0,0,0) // 	Go to Sleep (No Ack bit is mandatory)
#define awc_command_Save_Configuration(cmd)	awc_wout( cmd,0x0108,0,0,0) // 	Save the configuration to nonvolatile


#define AWC_COMMAND_NOOP_BULL 		0x000
#define AWC_COMMAND_ENABLE		0x001
#define AWC_COMMAND_ENABLE_MAC		0x101
#define AWC_COMMAND_ENABLE_RX		0x201
#define AWC_COMMAND_DISABLE		0x002
#define AWC_COMMAND_LOSE_SYNC		0x003
#define AWC_COMMAND_SOFT_RESET		0x004
#define AWC_COMMAND_HOST_SLEEP		0x085
#define AWC_COMMAND_MAGIC_PACKET	0x006
#define AWC_COMMAND_READ_CONF		0x008
#define AWC_COMMAND_SAVE_CONF		0x108
#define AWC_COMMAND_TX_ALLOC		0x00A
#define AWC_COMMAND_TX			0x00B
#define AWC_COMMAND_DEALLOC		0x00C
#define AWC_COMMAND_NOOP		0x010
#define AWC_COMMAND_READ_RID		0x021
#define AWC_COMMAND_WRITE_RID		0x121
#define AWC_COMMAND_ALLOC		0x028
#define AWC_COMMAND_PSP_NODES		0x030
#define AWC_COMMAND_SET_PHY		0x03E
#define AWC_COMMAND_TX_TEST		0x03F
#define AWC_COMMAND_SLEEP		0x085


#define awc_command_name_strings {\
	{0x0000, 0x00FF,"awc_command_NOP " },\
	{0x0001, 0x00FF,"awc_command_Enable_All " },\
	{0x0101, 0x01FF,"awc_command_Enable_MAC " },\
	{0x0201, 0x01FF,"awc_command_Enable_Rx " },\
	{0x0002, 0x00FF,"awc_command_Disable_MAC " },\
	{0x0003, 0x00FF,"awc_command_Sync_Loss " },\
	{0x0004, 0x00FF,"awc_command_Soft_Reset " },\
	{0x0005, 0x00FF,"awc_command_Host_Sleep " },\
	{0x0006, 0x00FF,"awc_command_Magic_Packet " },\
	{0x0008, 0x00FF,"awc_command_Read_Configuration " },\
	{0x000A, 0x00FF,"awc_command_Allocate_TX_Buff " },\
	{0x000B, 0x00FF,"awc_command_TX " },\
	{0x000C, 0x00FF,"awc_command_Deallocate " },\
	{0x0010, 0x00FF,"awc_command_NOP2 " },\
	{0x0021, 0x00FF,"awc_command_Read_RID " },\
	{0x0121, 0x01FF,"awc_command_Write_RID " },\
	{0x0028, 0x00FF,"awc_command_Allocate_Buff " },\
	{0x0030, 0x00FF,"awc_command_PSP_Nodes " },\
	{0x003E, 0x00FF,"awc_command_Set_Phy_register " },\
	{0x003F, 0x00FF,"awc_command_TX_Test " },\
	{0x013F, 0x01FF,"awc_command_RX_Test " },\
	{0x0085, 0x00FF,"awc_command_Sleep " },\
	{0x0108, 0x01FF,"awc_command_Save_Configuration " },\
	{0x0000, 0x00FF, NULL}\
};


/***************************** STATUSES */

#define awc_reply_success 0x0000

#define awc_reply_error_strings {\
   { 0x0000, 0x00FF,"    Success"},\
   { 0x0001, 0x00FF,"    Illegal command."},\
   { 0x0002, 0x00FF,"    Illegal format."},\
   { 0x0003, 0x00FF,"    Invalid FID."},\
   { 0x0004, 0x00FF,"    Invalid RID."},\
   { 0x0005, 0x00FF,"    Too Large"},\
   { 0x0006, 0x00FF,"    MAC is not disabled."},\
   { 0x0007, 0x00FF,"    Alloc is still busy processing previous alloc"},\
   { 0x0008, 0x00FF,"    Invalid Mode Field"},\
   { 0x0009, 0x00FF,"    Tx is not allowed in monitor mode"},\
   { 0x000A, 0x00FF,"    Loop test or memory test error"},\
   { 0x000B, 0x00FF,"    Cannot read this RID."},\
   { 0x000C, 0x00FF,"    Cannot write to this RID."},\
   { 0x000D, 0x00FF,"    Tag not found."},\
   { 0x0080, 0x00FF,"    Config mode is invalid."},\
   { 0x0081, 0x00FF,"    Config hop interval is invalid."},\
   { 0x0082, 0x00FF,"    Config beacon interval is invalid."},\
   { 0x0083, 0x00FF,"    Config receive mode is invalid."},\
   { 0x0084, 0x00FF,"    Config MAC address is invalid."},\
   { 0x0085, 0x00FF,"    Config rates are invalid."},\
   { 0x0086, 0x00FF,"    Config ordering field is invalid."},\
   { 0x0087, 0x00FF,"    Config scan mode is invalid."},\
   { 0x0088, 0x00FF,"    Config authentication type is invalid."},\
   { 0x0089, 0x00FF,"    Config power save mode is invalid."},\
   { 0x008A, 0x00FF,"    Config radio type is invalid."},\
   { 0x008B, 0x00FF,"    Config diversity is invalid."},\
   { 0x008C, 0x00FF,"    Config SSID list is invalid."},\
   { 0x008D, 0x00FF,"    Config specified AP list is invalid."},\
   { 0x0000, 0x00FF, NULL}\
};

#define awc_reply_command_failed( status) ((status & 0x7F00) == 0x7F)


/*************************   PHY and TEST commands   ****************/


// this might be wrong and reading is not implemented(was not in spec properly)
#define awc_Set_PLCP_Word(PLCP_Word)\
	awc_command_Set_Phy_register(base,0x8000,0 ,PLCP_Word)
#define awc_Set_TX_Test_Freq(Tx_Test_Freq)\
	awc_command_Set_Phy_register(base,0x8002,0 ,Tx_Test_Freq)
#define awc_Set_Tx_Power(Tx_Power)\
	awc_command_Set_Phy_register(base,0x8004,0 ,Tx_Power)
#define awc_Set_RSSI_Treshold(RSSI_Treshold)\
	awc_command_Set_Phy_register(base,0x8006,0 ,RSSI_Treshold)
#define awc_Get_PLCP_Word(PLCP_Word)\
	awc_command_Set_Phy_register(base,0x8000,0 ,0)
#define awc_Get_TX_Test_Freq(Tx_Test_Freq)\
	awc_command_Set_Phy_register(base,0x8002,0 ,0)
#define awc_Get_Tx_Power(Tx_Power)\
	awc_command_Set_Phy_register(base,0x8004,0 ,0)
#define awc_Get_RSSI_Treshold(RSSI_Treshold)\
	awc_command_Set_Phy_register(base,0x8006,0 ,0)


#define awc_tx_test_code_end 	0x0000   //  Ends the transmitter test
#define awc_tx_test_code_loop	0x0001   //  Loop back to the beginning of the commands
#define awc_tx_test_code_start	0x0002   //  Start transmitting
#define awc_tx_test_code_stop	0x0003   //  Stop transmitting
#define awc_tx_test_code_delayu 0x0004   //  Delay for N usec where N is the next word
#define awc_tx_test_code_delayk 0x0005   //  Delay for N Kusec where N is the next word
#define awc_tx_test_code_next	0x0006   //  Go to the next frequency in the frequency RID
#define awc_tx_test_code_rx	0x0007   //  Start receive mode

#define awc_tx_test_code_strings {\
{  awc_tx_test_code_end , 	0x000f ,"    Ends the transmitter test"},\
{  awc_tx_test_code_loop , 	0x000f ,"     Loop back to the beginning of the commands"},\
{  awc_tx_test_code_start , 	0x000f ,"    Start transmitting"},\
{  awc_tx_test_code_stop ,	0x000f ,"    Stop transmitting"},\
{  awc_tx_test_code_delayu , 	0x000f ,"    Delay for N usec where N is the next word"},\
{  awc_tx_test_code_delayk , 	0x000f ,"    Delay for N Kusec where N is the next word"},\
{  awc_tx_test_code_next , 	0x000f ,"    Go to the next frequency in the frequency RID"},\
{  awc_tx_test_code_rx 	,	0x000f ,"    Start receive mode"},\
{ 			0   , 0x000f ,NULL}\
};



#define AWC_COMMSTAT_HARD_RESET		0x0000001
#define AWC_COMMSTAT_WAKE		0x0000002
#define AWC_COMMSTAT_SOFT_RESET		0x0000004
#define AWC_COMMSTAT_CONFIGURE		0x0000008
#define AWC_COMMSTAT_READ_CONF		0x0000010
#define AWC_COMMSTAT_SAVE_CONF		0x0000020
#define AWC_COMMSTAT_DEALLOC		0x0000040
#define AWC_COMMSTAT_ALLOC_TX		0x0000080
#define AWC_COMMSTAT_ALLOC_TEST		0x0000100
#define AWC_COMMSTAT_ENABLE_MAC		0x0000200
#define AWC_COMMSTAT_ENABLE_RX		0x0000400
#define AWC_COMMSTAT_DISABLE_MAC	0x0000800
#define AWC_COMMSTAT_RX_ACK		0x0001000
#define AWC_COMMSTAT_TX_ACK		0x0002000
#define AWC_COMMSTAT_AWAKEN_ACK		0x0004000
#define AWC_COMMSTAT_TX_FAIL_ACK	0x0008000
#define AWC_COMMSTAT_LINK_ACK		0x0010000
#define AWC_COMMSTAT_CLR_CMD		0x0020000
#define AWC_COMMSTAT_ALLOC_ACK		0x0040000
#define AWC_COMMSTAT_HOST_SLEEP		0x0080000
#define AWC_COMMSTAT_RX			0x0100000
#define AWC_COMMSTAT_TX			0x0200000
#define AWC_COMMSTAT_SLEEP		0x0400000
#define AWC_COMMSTAT_PSP_NODES		0x0800000
#define AWC_COMMSTAT_SET_TX_POWER 	0x1000000


/*****************************     R  I  D	***************/

#define AWC_NOF_RIDS	18
extern int awc_rid_setup(struct net_device * dev);

struct aironet4500_rid_selector{
	const u16 selector;
	const unsigned 	MAC_Disable_at_write:1;
	const unsigned	read_only:1;
	const unsigned  may_change:1;
	const char *	name;
};





extern const struct aironet4500_rid_selector aironet4500_RID_Select_General_Config;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_SSID_list;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_AP_list	;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_Driver_name;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_Encapsulation;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_Active_Config;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_Capabilities;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_AP_Info	;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_Radio_Info;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_Status	;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_Modulation	;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_WEP_volatile	;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_WEP_nonvolatile	;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_16_stats;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_16_stats_delta;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_16_stats_clear;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_32_stats;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_32_stats_delta;
extern const struct aironet4500_rid_selector aironet4500_RID_Select_32_stats_clear;

#define awc_def_gen_RID(offset,name, bits,mask,value,value_name)\
 {&aironet4500_RID_Select_General_Config,offset, bits,1,1,0,0, mask, value, name, value_name}
#define awc_def_SSID_RID(offset,name, bits,mask,value,value_name)\
  {&aironet4500_RID_Select_SSID_list,offset, bits,1,1,0,0, mask, value, name, value_name}
#define awc_def_AP_List_RID(offset,name, bits,mask,value,value_name)\
 {&aironet4500_RID_Select_AP_list,offset, bits,1,1,0,0, mask, value, name, value_name}
#define awc_def_Dname_RID(offset,name, bits,mask,value,value_name)\
 {&aironet4500_RID_Select_Driver_name,offset, bits,1,1,0,0, mask, value, name, value_name}
#define awc_def_act_RID(offset,name, bits,mask,value,value_name)\
 {&aironet4500_RID_Select_Active_Config,offset, bits,1,1,0,0, mask, value, name, value_name}
#define awc_def_Cap_RID(offset,name, bits,mask,value,value_name)\
 {&aironet4500_RID_Select_Capabilities,offset, bits,1,1,0,0, mask, value, name, value_name}
#define awc_def_AP_RID(offset,name, bits,mask,value,value_name)\
 {&aironet4500_RID_Select_AP_Info,offset, bits,1,1,0,0, mask, value, name, value_name}
#define awc_def_Radio_RID(offset,name, bits,mask,value,value_name)\
 {&aironet4500_RID_Select_Radio_Info,offset, bits,1,1,0,0, mask, value, name, value_name}
#define awc_def_Stat_RID(offset,name, bits,mask,value,value_name)\
 {&aironet4500_RID_Select_Status,offset, bits,1,1,0,0, mask, value, name, value_name}
#define awc_def_Enc_RID(offset,name, bits,mask,value,value_name)\
 {&aironet4500_RID_Select_Encapsulation,offset, bits,1,1,0,0, mask, value, name, value_name}

#define awc_def_WEPv_RID(offset,name, bits,mask,value,value_name)\
 {&aironet4500_RID_Select_WEP_volatile,offset, bits,1,1,0,0, mask, value, name, value_name}
#define awc_def_WEPnv_RID(offset,name, bits,mask,value,value_name)\
 {&aironet4500_RID_Select_WEP_nonvolatile,offset, bits,1,1,0,0, mask, value, name, value_name}
#define awc_def_Modulation_RID(offset,name, bits,mask,value,value_name)\
 {&aironet4500_RID_Select_Modulation,offset, bits,1,1,0,0, mask, value, name, value_name}

#define awc_def_Stats_RID(o16,offset,name, value_name)\
 {&aironet4500_RID_Select_32_stats,offset, 32,1,1,0,0, 0xffffffff, 0, name, value_name}
#define awc_def_Stats_delta_RID(o16,offset,name, value_name)\
 {&aironet4500_RID_Select_32_stats_delta,offset, 32,1,1,0,0, 0xffffffff, 0, name, value_name}
#define awc_def_Stats_clear_RID(o16,offset,name, value_name)\
 {&aironet4500_RID_Select_32_stats_delta,offset,32,1,1,0,0, 0xffffffff,  0, name,value_name}

#define awc_def_Stats16_RID(offset,o32,name, value_name)\
 {&aironet4500_RID_Select_16_stats,offset, 16,1,1,0,0, 0xffffffff, 0, name, value_name}
#define awc_def_Stats16_delta_RID(offset,o32,name, value_name)\
 {&aironet4500_RID_Select_16_stats_delta,offset, 16,1,1,0,0, 0xffffffff,  0, name,value_name}
#define awc_def_Stats16_clear_RID(offset,o32,name, value_name)\
 {&aironet4500_RID_Select_16_stats_delta,offset, 16,1,1,0,0, 0xffffffff,  0, name,value_name}


#define aironet4500_RID_Select_strings {\
{ 0xFF10, 0xffff, "General Configuration"},\
{ 0xFF11, 0xffff, "Valid SSID list" },\
{ 0xFF12, 0xffff, "Valid AP list"},\
{ 0xFF13, 0xffff, "Driver name"},\
{ 0xFF14, 0xffff, "Ethernet Protocol"},\
{ 0xFF15, 0xffff, "WEP volatile"},\
{ 0xFF16, 0xffff, "WEP nonvolatile"},\
{ 0xFF17, 0xffff, "Modulation"},\
{ 0xFF20, 0xffff, "Actual Configuration"},\
{ 0xFF00, 0xffff, "Capabilities"},\
{ 0xFF01, 0xffff, "AP Info"},\
{ 0xFF02, 0xffff, "Radio Info"},\
{ 0xFF50, 0xffff, "Status"},\
{ 0xFF60, 0xffff, "Cumulative 16-bit Statistics"},\
{ 0xFF61, 0xffff, "Delta 16-bit Statistics"},\
{ 0xFF62, 0xffff, "Delta 16-bit Statistics and Clear"},\
{ 0xFF68, 0xffff, "Cumulative 32-bit Statistics"},\
{ 0xFF69, 0xffff, "Delta 32-bit Statistics "},\
{ 0xFF6A, 0xffff, "Delta 32-bit Statistics and Clear"},\
{ 0x0000, 0xffff, NULL}\
}





struct aironet4500_RID {
	const struct aironet4500_rid_selector	*  selector;
	const u32 	offset;
	const u8 	bits;
	const u8 	array;
	const u32 	units;
	const unsigned read_only:1;
	const unsigned null_terminated:1;
	const u32 	mask;
	const u32 	value;
	const char * name;
	const char * value_name;
		
};

struct aironet4500_RID_names{
	struct aironet4500_RID rid;
	char *name;
};

struct aironet4500_RID_names_values{
	struct aironet4500_RID rid;
	char *name;
	u32	mask;	
};

struct awc_rid_dir{
	const struct aironet4500_rid_selector *	selector;
	const int size;
	const struct aironet4500_RID * rids;
	struct net_device * dev ;
	void * 	buff;
	int	bufflen; // just checking
};

extern int awc_nof_rids;
extern struct awc_rid_dir  awc_rids[];





struct awc_private {
	dev_node_t node; // somewhere back in times PCMCIA needed that
	
	int dummy_test; // left for cleanup
	// card rid inmemory copy
	struct awc_config 		config; // card RID mirrors
	struct awc_config 		general_config; // 
	struct awc_SSIDs  		SSIDs;
	struct awc_fixed_APs 		fixed_APs;
	struct awc_driver_name		driver_name;
	struct awc_enc_trans		enc_trans;
	struct awc_cap			capabilities;
	struct awc_status		status;
	struct awc_AP			AP;
	struct awc_Statistics_32 	statistics;
	struct awc_Statistics_32 	statistics_delta;
	struct awc_Statistics_32 	statistics_delta_clear;
	struct awc_Statistics_16 	statistics16;
	struct awc_Statistics_16 	statistics16_delta;
	struct awc_Statistics_16 	statistics16_delta_clear;
	struct awc_wep_key		wep_volatile;
	struct awc_wep_key		wep_nonvolatile;
	struct awc_modulation		modulation;

	// here are just references to rids
	struct awc_rid_dir		rid_dir[AWC_NOF_RIDS];
	int	rids_read;
	
	
	struct awc_bap		bap0;
	struct awc_bap		bap1;
	int			sleeping_bap;
	
	struct awc_fid_queue    tx_small_ready;
	struct awc_fid_queue    tx_large_ready;
	struct awc_fid_queue    tx_post_process;
	struct awc_fid_queue    tx_in_transmit;
	spinlock_t		queues_lock;

	struct awc_fid_queue    rx_ready;
	struct awc_fid_queue    rx_post_process;


	
	struct semaphore	tx_buff_semaphore;
	volatile int		tx_buffs_in_use;
	volatile int 		tx_small_buffs_in_use;
	volatile int		tx_buffs_total;
	volatile int		tx_small_buffs_total;
	int			large_buff_mem;
	int			small_buff_no;
	
	volatile int		mac_enabled;
	u16			link_status;
	u8			link_status_changed;
	
	volatile int		ejected;
	volatile int		bh_running;
	volatile int		bh_active;
	volatile long		tx_chain_active;
	volatile u16		enabled_interrupts;
	volatile u16		waiting_interrupts;
	volatile int		interrupt_count;
	
	// Command serialize stuff
//changed to spinlock        struct semaphore 	command_semaphore;
	spinlock_t		both_bap_spinlock; // on SMP, card should theorethically live without that
	unsigned long		both_bap_spinlock_flags;
	spinlock_t		bap_setup_spinlock; // on SMP, card should theoretically live without that
	unsigned long		bap_setup_spinlock_flags;
	spinlock_t		command_issuing_spinlock;
	unsigned long		command_issuing_spinlock_flags;
	spinlock_t		interrupt_spinlock;

        volatile int		unlock_command_postponed;
        struct awc_command	cmd;
        long long		async_command_start;
        volatile int		command_semaphore_on;
        struct tq_struct 	immediate_bh;
	volatile int		process_tx_results;

	u8			p2p[6];
	u8			bssid[6];
	int			p2p_uc;
	int			p2p_found;
	int			p802_11_send;
	int			simple_bridge;
	int			force_rts_on_shorter;
	int			force_tx_rate;
	int			ip_tos_reliability_rts;
	int			ip_tos_troughput_no_retries;
	int 			full_stats;
	int 			debug;
	
	struct net_device_stats stats;
	
	struct ctl_table * proc_table;

	void	* 		bus;
	int 			card_type;
};

extern int 		awc_init(struct net_device * dev);
extern void 		awc_reset(struct net_device *dev);
extern int 		awc_config(struct net_device *dev);
extern int 		awc_open(struct net_device *dev);
extern void 		awc_tx_timeout(struct net_device *dev);
extern int 		awc_start_xmit(struct sk_buff *, struct net_device *);
extern void 		awc_interrupt(int irq, void *dev_id, struct pt_regs *regs);
extern struct net_device_stats *	awc_get_stats(struct net_device *dev);
extern void		awc_set_multicast_list(struct net_device *dev);
extern int awc_change_mtu(struct net_device *dev, int new_mtu);  
extern int 		awc_close(struct net_device *dev);
extern int		awc_private_init(struct net_device * dev);
extern int awc_register_proc(int (*awc_proc_set_device) (int),int (*awc_proc_unset_device)(int));
extern int awc_unregister_proc(void);
extern int (* awc_proc_set_fun) (int) ;
extern int (* awc_proc_unset_fun) (int) ;
extern int	awc_interrupt_process(struct net_device * dev);
extern int	awc_readrid(struct net_device * dev, struct aironet4500_RID * rid, void *pBuf );
extern int 	awc_writerid(struct net_device * dev, struct aironet4500_RID * rid, void *pBuf);
extern int 	awc_readrid_dir(struct net_device * dev, struct awc_rid_dir * rid );
extern int 	awc_writerid_dir(struct net_device * dev, struct awc_rid_dir * rid);
extern int 	awc_tx_alloc(struct net_device * dev) ;
extern int	awc_tx_dealloc(struct net_device * dev);
extern struct awc_fid *awc_tx_fid_lookup(struct net_device * dev, u16 fid);
extern int 	awc_issue_soft_reset(struct net_device * dev);
extern int	awc_issue_noop(struct net_device * dev);
extern int 	awc_dump_registers(struct net_device * dev);
extern unsigned short  awc_issue_command_and_block(struct awc_command * cmd);
extern int	awc_enable_MAC(struct net_device * dev);
extern int	awc_disable_MAC(struct net_device * dev);
extern int	awc_read_all_rids(struct net_device * dev);
extern int	awc_write_all_rids(struct net_device * dev);
extern int	awc_receive_packet(struct net_device * dev);
extern int	awc_transmit_packet(struct net_device * dev, struct awc_fid * tx_buff) ;
extern int	awc_tx_complete_check(struct net_device * dev);
extern int	awc_interrupt_process(struct net_device * dev);
extern void 	awc_bh(struct net_device *dev);
extern int 	awc_802_11_find_copy_path(struct net_device * dev, struct awc_fid * rx_buff);
extern void 	awc_802_11_router_rx(struct net_device * dev,struct awc_fid * rx_buff);
extern int 	awc_802_11_tx_find_path_and_post(struct net_device * dev, struct sk_buff * skb);
extern void 	awc_802_11_after_tx_packet_to_card_write(struct net_device * dev, struct awc_fid * tx_buff);
extern void 	awc_802_11_after_failed_tx_packet_to_card_write(struct net_device * dev,struct awc_fid * tx_buff);
extern void 	awc_802_11_after_tx_complete(struct net_device * dev, struct awc_fid * tx_buff);
extern void 	awc_802_11_failed_rx_copy(struct net_device * dev,struct awc_fid * rx_buff);
extern int 	awc_tx_alloc(struct net_device * dev) ;
extern int 	awc_tx_dealloc_fid(struct net_device * dev,struct awc_fid * fid);
extern int	awc_tx_dealloc(struct net_device * dev);
extern struct awc_fid *
	awc_tx_fid_lookup_and_remove(struct net_device * dev, u16 fid_handle);
extern int 	awc_queues_init(struct net_device * dev);
extern int 	awc_queues_destroy(struct net_device * dev);
extern int 	awc_rids_setup(struct net_device * dev);



extern int		awc_debug;
extern int bap_sleep ;
extern int bap_sleep_after_setup ;
extern int sleep_before_command  ;
extern int bap_sleep_before_write;
extern int sleep_in_command    ;
extern int both_bap_lock;
extern int bap_setup_spinlock;
extern int tx_queue_len ;
extern int tx_rate;
extern int awc_full_stats;

#define MAX_AWCS	4
extern struct net_device * aironet4500_devices[MAX_AWCS];

#define AWC_DEBUG 1

#ifdef AWC_DEBUG
	#define DEBUG(a,args...) if (awc_debug & a) printk( args)
	#define AWC_ENTRY_EXIT_DEBUG(a)  if (awc_debug & 8) printk( a)
#else
	#define DEBUG(a, args...)
	#define AWC_ENTRY_EXIT_DEBUG(a)
#endif

#endif /* AIRONET4500_H */
