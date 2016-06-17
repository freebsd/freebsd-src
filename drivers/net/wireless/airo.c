/*======================================================================

    Aironet driver for 4500 and 4800 series cards

    This code is released under both the GPL version 2 and BSD licenses.
    Either license may be used.  The respective licenses are found at
    the end of this file.

    This code was developed by Benjamin Reed <breed@users.sourceforge.net>
    including portions of which come from the Aironet PC4500
    Developer's Reference Manual and used with permission.  Copyright
    (C) 1999 Benjamin Reed.  All Rights Reserved.  Permission to use
    code in the Developer's manual was granted for this driver by
    Aironet.  Major code contributions were received from Javier Achirica
    <achirica@users.sourceforge.net> and Jean Tourrilhes <jt@hpl.hp.com>.
    Code was also integrated from the Cisco Aironet driver for Linux.

======================================================================*/

#include <linux/config.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/bitops.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <asm/uaccess.h>

#ifdef CONFIG_PCI
static struct pci_device_id card_ids[] = {
	{ 0x14b9, 1, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0x14b9, 0x4500, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x14b9, 0x4800, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0x14b9, 0x0340, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0x14b9, 0x0350, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, card_ids);

static int airo_pci_probe(struct pci_dev *, const struct pci_device_id *);
static void airo_pci_remove(struct pci_dev *);

static struct pci_driver airo_driver = {
	.name     = "airo",
	.id_table = card_ids,
	.probe    = airo_pci_probe,
	.remove   = __devexit_p(airo_pci_remove),
};
#endif /* CONFIG_PCI */

/* Include Wireless Extension definition and check version - Jean II */
#include <linux/wireless.h>
#define WIRELESS_SPY		// enable iwspy support
#if WIRELESS_EXT > 12
#include <net/iw_handler.h>	// New driver API
#endif	/* WIRELESS_EXT > 12 */

#define CISCO_EXT		// enable Cisco extensions
#ifdef CISCO_EXT
#include <linux/delay.h>
#endif

/* Support Cisco MIC feature */
#define MICSUPPORT

#if defined(MICSUPPORT) && !defined(CONFIG_CRYPTO)
#warning MIC support requires Crypto API
#undef MICSUPPORT
#endif

/* Hack to do some power saving */
#define POWER_ON_DOWN

/* As you can see this list is HUGH!
   I really don't know what a lot of these counts are about, but they
   are all here for completeness.  If the IGNLABEL macro is put in
   infront of the label, that statistic will not be included in the list
   of statistics in the /proc filesystem */

#define IGNLABEL(comment) 0
static char *statsLabels[] = {
	"RxOverrun",
	IGNLABEL("RxPlcpCrcErr"),
	IGNLABEL("RxPlcpFormatErr"),
	IGNLABEL("RxPlcpLengthErr"),
	"RxMacCrcErr",
	"RxMacCrcOk",
	"RxWepErr",
	"RxWepOk",
	"RetryLong",
	"RetryShort",
	"MaxRetries",
	"NoAck",
	"NoCts",
	"RxAck",
	"RxCts",
	"TxAck",
	"TxRts",
	"TxCts",
	"TxMc",
	"TxBc",
	"TxUcFrags",
	"TxUcPackets",
	"TxBeacon",
	"RxBeacon",
	"TxSinColl",
	"TxMulColl",
	"DefersNo",
	"DefersProt",
	"DefersEngy",
	"DupFram",
	"RxFragDisc",
	"TxAged",
	"RxAged",
	"LostSync-MaxRetry",
	"LostSync-MissedBeacons",
	"LostSync-ArlExceeded",
	"LostSync-Deauth",
	"LostSync-Disassoced",
	"LostSync-TsfTiming",
	"HostTxMc",
	"HostTxBc",
	"HostTxUc",
	"HostTxFail",
	"HostRxMc",
	"HostRxBc",
	"HostRxUc",
	"HostRxDiscard",
	IGNLABEL("HmacTxMc"),
	IGNLABEL("HmacTxBc"),
	IGNLABEL("HmacTxUc"),
	IGNLABEL("HmacTxFail"),
	IGNLABEL("HmacRxMc"),
	IGNLABEL("HmacRxBc"),
	IGNLABEL("HmacRxUc"),
	IGNLABEL("HmacRxDiscard"),
	IGNLABEL("HmacRxAccepted"),
	"SsidMismatch",
	"ApMismatch",
	"RatesMismatch",
	"AuthReject",
	"AuthTimeout",
	"AssocReject",
	"AssocTimeout",
	IGNLABEL("ReasonOutsideTable"),
	IGNLABEL("ReasonStatus1"),
	IGNLABEL("ReasonStatus2"),
	IGNLABEL("ReasonStatus3"),
	IGNLABEL("ReasonStatus4"),
	IGNLABEL("ReasonStatus5"),
	IGNLABEL("ReasonStatus6"),
	IGNLABEL("ReasonStatus7"),
	IGNLABEL("ReasonStatus8"),
	IGNLABEL("ReasonStatus9"),
	IGNLABEL("ReasonStatus10"),
	IGNLABEL("ReasonStatus11"),
	IGNLABEL("ReasonStatus12"),
	IGNLABEL("ReasonStatus13"),
	IGNLABEL("ReasonStatus14"),
	IGNLABEL("ReasonStatus15"),
	IGNLABEL("ReasonStatus16"),
	IGNLABEL("ReasonStatus17"),
	IGNLABEL("ReasonStatus18"),
	IGNLABEL("ReasonStatus19"),
	"RxMan",
	"TxMan",
	"RxRefresh",
	"TxRefresh",
	"RxPoll",
	"TxPoll",
	"HostRetries",
	"LostSync-HostReq",
	"HostTxBytes",
	"HostRxBytes",
	"ElapsedUsec",
	"ElapsedSec",
	"LostSyncBetterAP",
	"PrivacyMismatch",
	"Jammed",
	"DiscRxNotWepped",
	"PhyEleMismatch",
	(char*)-1 };
#ifndef RUN_AT
#define RUN_AT(x) (jiffies+(x))
#endif


/* These variables are for insmod, since it seems that the rates
   can only be set in setup_card.  Rates should be a comma separated
   (no spaces) list of rates (up to 8). */

static int rates[8];
static int basic_rate;
static char *ssids[3];

static int io[4];
static int irq[4];

static
int maxencrypt /* = 0 */; /* The highest rate that the card can encrypt at.
		       0 means no limit.  For old cards this was 4 */

static int auto_wep /* = 0 */; /* If set, it tries to figure out the wep mode */
static int aux_bap /* = 0 */; /* Checks to see if the aux ports are needed to read
		    the bap, needed on some older cards and buses. */
static int adhoc;

static int probe = 1;

static int proc_uid /* = 0 */;

static int proc_gid /* = 0 */;

static int airo_perm = 0555;

static int proc_perm = 0644;

MODULE_AUTHOR("Benjamin Reed");
MODULE_DESCRIPTION("Support for Cisco/Aironet 802.11 wireless ethernet \
                   cards.  Direct support for ISA/PCI cards and support \
		   for PCMCIA when used with airo_cs.");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_SUPPORTED_DEVICE("Aironet 4500, 4800 and Cisco 340");
MODULE_PARM(io,"1-4i");
MODULE_PARM(irq,"1-4i");
MODULE_PARM(basic_rate,"i");
MODULE_PARM(rates,"1-8i");
MODULE_PARM(ssids,"1-3s");
MODULE_PARM(auto_wep,"i");
MODULE_PARM_DESC(auto_wep, "If non-zero, the driver will keep looping through \
the authentication options until an association is made.  The value of \
auto_wep is number of the wep keys to check.  A value of 2 will try using \
the key at index 0 and index 1.");
MODULE_PARM(aux_bap,"i");
MODULE_PARM_DESC(aux_bap, "If non-zero, the driver will switch into a mode \
than seems to work better for older cards with some older buses.  Before \
switching it checks that the switch is needed.");
MODULE_PARM(maxencrypt, "i");
MODULE_PARM_DESC(maxencrypt, "The maximum speed that the card can do \
encryption.  Units are in 512kbs.  Zero (default) means there is no limit. \
Older cards used to be limited to 2mbs (4).");
MODULE_PARM(adhoc, "i");
MODULE_PARM_DESC(adhoc, "If non-zero, the card will start in adhoc mode.");
MODULE_PARM(probe, "i");
MODULE_PARM_DESC(probe, "If zero, the driver won't start the card.");

MODULE_PARM(proc_uid, "i");
MODULE_PARM_DESC(proc_uid, "The uid that the /proc files will belong to.");
MODULE_PARM(proc_gid, "i");
MODULE_PARM_DESC(proc_gid, "The gid that the /proc files will belong to.");
MODULE_PARM(airo_perm, "i");
MODULE_PARM_DESC(airo_perm, "The permission bits of /proc/[driver/]aironet.");
MODULE_PARM(proc_perm, "i");
MODULE_PARM_DESC(proc_perm, "The permission bits of the files in /proc");

/* This is a kind of sloppy hack to get this information to OUT4500 and
   IN4500.  I would be extremely interested in the situation where this
   doesn't work though!!! */
static int do8bitIO = 0;

/* Return codes */
#define SUCCESS 0
#define ERROR -1
#define NO_PACKET -2

/* Commands */
#define NOP2		0x0000
#define MAC_ENABLE	0x0001
#define MAC_DISABLE	0x0002
#define CMD_LOSE_SYNC	0x0003 /* Not sure what this does... */
#define CMD_SOFTRESET	0x0004
#define HOSTSLEEP	0x0005
#define CMD_MAGIC_PKT	0x0006
#define CMD_SETWAKEMASK	0x0007
#define CMD_READCFG	0x0008
#define CMD_SETMODE	0x0009
#define CMD_ALLOCATETX	0x000a
#define CMD_TRANSMIT	0x000b
#define CMD_DEALLOCATETX 0x000c
#define NOP		0x0010
#define CMD_WORKAROUND	0x0011
#define CMD_ALLOCATEAUX 0x0020
#define CMD_ACCESS	0x0021
#define CMD_PCIBAP	0x0022
#define CMD_PCIAUX	0x0023
#define CMD_ALLOCBUF	0x0028
#define CMD_GETTLV	0x0029
#define CMD_PUTTLV	0x002a
#define CMD_DELTLV	0x002b
#define CMD_FINDNEXTTLV	0x002c
#define CMD_PSPNODES	0x0030
#define CMD_SETCW	0x0031    
#define CMD_SETPCF	0x0032    
#define CMD_SETPHYREG	0x003e
#define CMD_TXTEST	0x003f
#define MAC_ENABLETX	0x0101
#define CMD_LISTBSS	0x0103
#define CMD_SAVECFG	0x0108
#define CMD_ENABLEAUX	0x0111
#define CMD_WRITERID	0x0121
#define CMD_USEPSPNODES	0x0130
#define MAC_ENABLERX	0x0201

/* Command errors */
#define ERROR_QUALIF 0x00
#define ERROR_ILLCMD 0x01
#define ERROR_ILLFMT 0x02
#define ERROR_INVFID 0x03
#define ERROR_INVRID 0x04
#define ERROR_LARGE 0x05
#define ERROR_NDISABL 0x06
#define ERROR_ALLOCBSY 0x07
#define ERROR_NORD 0x0B
#define ERROR_NOWR 0x0C
#define ERROR_INVFIDTX 0x0D
#define ERROR_TESTACT 0x0E
#define ERROR_TAGNFND 0x12
#define ERROR_DECODE 0x20
#define ERROR_DESCUNAV 0x21
#define ERROR_BADLEN 0x22
#define ERROR_MODE 0x80
#define ERROR_HOP 0x81
#define ERROR_BINTER 0x82
#define ERROR_RXMODE 0x83
#define ERROR_MACADDR 0x84
#define ERROR_RATES 0x85
#define ERROR_ORDER 0x86
#define ERROR_SCAN 0x87
#define ERROR_AUTH 0x88
#define ERROR_PSMODE 0x89
#define ERROR_RTYPE 0x8A
#define ERROR_DIVER 0x8B
#define ERROR_SSID 0x8C
#define ERROR_APLIST 0x8D
#define ERROR_AUTOWAKE 0x8E
#define ERROR_LEAP 0x8F

/* Registers */
#define COMMAND 0x00
#define PARAM0 0x02
#define PARAM1 0x04
#define PARAM2 0x06
#define STATUS 0x08
#define RESP0 0x0a
#define RESP1 0x0c
#define RESP2 0x0e
#define LINKSTAT 0x10
#define SELECT0 0x18
#define OFFSET0 0x1c
#define RXFID 0x20
#define TXALLOCFID 0x22
#define TXCOMPLFID 0x24
#define DATA0 0x36
#define EVSTAT 0x30
#define EVINTEN 0x32
#define EVACK 0x34
#define SWS0 0x28
#define SWS1 0x2a
#define SWS2 0x2c
#define SWS3 0x2e
#define AUXPAGE 0x3A
#define AUXOFF 0x3C
#define AUXDATA 0x3E

/* BAP selectors */
#define BAP0 0 // Used for receiving packets
#define BAP1 2 // Used for xmiting packets and working with RIDS

/* Flags */
#define COMMAND_BUSY 0x8000

#define BAP_BUSY 0x8000
#define BAP_ERR 0x4000
#define BAP_DONE 0x2000

#define PROMISC 0xffff
#define NOPROMISC 0x0000

#define EV_CMD 0x10
#define EV_CLEARCOMMANDBUSY 0x4000
#define EV_RX 0x01
#define EV_TX 0x02
#define EV_TXEXC 0x04
#define EV_ALLOC 0x08
#define EV_LINK 0x80
#define EV_AWAKE 0x100
#define EV_TXCPY 0x400
#define EV_UNKNOWN 0x800
#define EV_MIC 0x1000 /* Message Integrity Check Interrupt */
#define STATUS_INTS ( EV_AWAKE | EV_LINK | EV_TXEXC | EV_TX | EV_RX | EV_MIC )

#ifdef CHECK_UNKNOWN_INTS
#define IGNORE_INTS ( EV_CMD | EV_UNKNOWN)
#else
#define IGNORE_INTS (~STATUS_INTS)
#endif

/* The RIDs */
#define RID_CAPABILITIES 0xFF00
#define RID_APINFO     0xFF01
#define RID_RADIOINFO  0xFF02
#define RID_UNKNOWN3   0xFF03
#define RID_RSSI       0xFF04
#define RID_CONFIG     0xFF10
#define RID_SSID       0xFF11
#define RID_APLIST     0xFF12
#define RID_DRVNAME    0xFF13
#define RID_ETHERENCAP 0xFF14
#define RID_WEP_TEMP   0xFF15
#define RID_WEP_PERM   0xFF16
#define RID_MODULATION 0xFF17
#define RID_OPTIONS    0xFF18
#define RID_ACTUALCONFIG 0xFF20 /*readonly*/
#define RID_FACTORYCONFIG 0xFF21
#define RID_UNKNOWN22  0xFF22
#define RID_LEAPUSERNAME 0xFF23
#define RID_LEAPPASSWORD 0xFF24
#define RID_STATUS     0xFF50
#define RID_BEACON_HST 0xFF51
#define RID_BUSY_HST   0xFF52
#define RID_RETRIES_HST 0xFF53
#define RID_UNKNOWN54  0xFF54
#define RID_UNKNOWN55  0xFF55
#define RID_UNKNOWN56  0xFF56
#define RID_MIC        0xFF57
#define RID_STATS16    0xFF60
#define RID_STATS16DELTA 0xFF61
#define RID_STATS16DELTACLEAR 0xFF62
#define RID_STATS      0xFF68
#define RID_STATSDELTA 0xFF69
#define RID_STATSDELTACLEAR 0xFF6A
#define RID_ECHOTEST_RID 0xFF70
#define RID_ECHOTEST_RESULTS 0xFF71
#define RID_BSSLISTFIRST 0xFF72
#define RID_BSSLISTNEXT  0xFF73

typedef struct {
	u16 cmd;
	u16 parm0;
	u16 parm1;
	u16 parm2;
} Cmd;

typedef struct {
	u16 status;
	u16 rsp0;
	u16 rsp1;
	u16 rsp2;
} Resp;

/*
 * Rids and endian-ness:  The Rids will always be in cpu endian, since
 * this all the patches from the big-endian guys end up doing that.
 * so all rid access should use the read/writeXXXRid routines.
 */

/* This is redundant for x86 archs, but it seems necessary for ARM */
#pragma pack(1)

/* This structure came from an email sent to me from an engineer at
   aironet for inclusion into this driver */
typedef struct {
	u16 len;
	u16 kindex;
	u8 mac[ETH_ALEN];
	u16 klen;
	u8 key[16];
} WepKeyRid;

/* These structures are from the Aironet's PC4500 Developers Manual */
typedef struct {
	u16 len;
	u8 ssid[32];
} Ssid;

typedef struct {
	u16 len;
	Ssid ssids[3];
} SsidRid;

typedef struct {
        u16 len;
        u16 modulation;
#define MOD_DEFAULT 0
#define MOD_CCK 1
#define MOD_MOK 2
} ModulationRid;

typedef struct {
	u16 len; /* sizeof(ConfigRid) */
	u16 opmode; /* operating mode */
#define MODE_STA_IBSS 0
#define MODE_STA_ESS 1
#define MODE_AP 2
#define MODE_AP_RPTR 3
#define MODE_ETHERNET_HOST (0<<8) /* rx payloads converted */
#define MODE_LLC_HOST (1<<8) /* rx payloads left as is */
#define MODE_AIRONET_EXTEND (1<<9) /* enable Aironet extenstions */
#define MODE_AP_INTERFACE (1<<10) /* enable ap interface extensions */
#define MODE_ANTENNA_ALIGN (1<<11) /* enable antenna alignment */
#define MODE_ETHER_LLC (1<<12) /* enable ethernet LLC */
#define MODE_LEAF_NODE (1<<13) /* enable leaf node bridge */
#define MODE_CF_POLLABLE (1<<14) /* enable CF pollable */
#define MODE_MIC (1<<15) /* enable MIC */
	u16 rmode; /* receive mode */
#define RXMODE_BC_MC_ADDR 0
#define RXMODE_BC_ADDR 1 /* ignore multicasts */
#define RXMODE_ADDR 2 /* ignore multicast and broadcast */
#define RXMODE_RFMON 3 /* wireless monitor mode */
#define RXMODE_RFMON_ANYBSS 4
#define RXMODE_LANMON 5 /* lan style monitor -- data packets only */
#define RXMODE_DISABLE_802_3_HEADER (1<<8) /* disables 802.3 header on rx */
#define RXMODE_NORMALIZED_RSSI (1<<9) /* return normalized RSSI */
	u16 fragThresh;
	u16 rtsThres;
	u8 macAddr[ETH_ALEN];
	u8 rates[8];
	u16 shortRetryLimit;
	u16 longRetryLimit;
	u16 txLifetime; /* in kusec */
	u16 rxLifetime; /* in kusec */
	u16 stationary;
	u16 ordering;
	u16 u16deviceType; /* for overriding device type */
	u16 cfpRate;
	u16 cfpDuration;
	u16 _reserved1[3];
	/*---------- Scanning/Associating ----------*/
	u16 scanMode;
#define SCANMODE_ACTIVE 0
#define SCANMODE_PASSIVE 1
#define SCANMODE_AIROSCAN 2
	u16 probeDelay; /* in kusec */
	u16 probeEnergyTimeout; /* in kusec */
        u16 probeResponseTimeout;
	u16 beaconListenTimeout;
	u16 joinNetTimeout;
	u16 authTimeout;
	u16 authType;
#define AUTH_OPEN 0x1
#define AUTH_ENCRYPT 0x101
#define AUTH_SHAREDKEY 0x102
#define AUTH_ALLOW_UNENCRYPTED 0x200
	u16 associationTimeout;
	u16 specifiedApTimeout;
	u16 offlineScanInterval;
	u16 offlineScanDuration;
	u16 linkLossDelay;
	u16 maxBeaconLostTime;
	u16 refreshInterval;
#define DISABLE_REFRESH 0xFFFF
	u16 _reserved1a[1];
	/*---------- Power save operation ----------*/
	u16 powerSaveMode;
#define POWERSAVE_CAM 0
#define POWERSAVE_PSP 1
#define POWERSAVE_PSPCAM 2
	u16 sleepForDtims;
	u16 listenInterval;
	u16 fastListenInterval;
	u16 listenDecay;
	u16 fastListenDelay;
	u16 _reserved2[2];
	/*---------- Ap/Ibss config items ----------*/
	u16 beaconPeriod;
	u16 atimDuration;
	u16 hopPeriod;
	u16 channelSet;
	u16 channel;
	u16 dtimPeriod;
	u16 bridgeDistance;
	u16 radioID;
	/*---------- Radio configuration ----------*/
	u16 radioType;
#define RADIOTYPE_DEFAULT 0
#define RADIOTYPE_802_11 1
#define RADIOTYPE_LEGACY 2
	u8 rxDiversity;
	u8 txDiversity;
	u16 txPower;
#define TXPOWER_DEFAULT 0
	u16 rssiThreshold;
#define RSSI_DEFAULT 0
        u16 modulation;
#define PREAMBLE_AUTO 0
#define PREAMBLE_LONG 1
#define PREAMBLE_SHORT 2
	u16 preamble;
	u16 homeProduct;
	u16 radioSpecific;
	/*---------- Aironet Extensions ----------*/
	u8 nodeName[16];
	u16 arlThreshold;
	u16 arlDecay;
	u16 arlDelay;
	u16 _reserved4[1];
	/*---------- Aironet Extensions ----------*/
	u8 magicAction;
#define MAGIC_ACTION_STSCHG 1
#define MACIC_ACTION_RESUME 2
#define MAGIC_IGNORE_MCAST (1<<8)
#define MAGIC_IGNORE_BCAST (1<<9)
#define MAGIC_SWITCH_TO_PSP (0<<10)
#define MAGIC_STAY_IN_CAM (1<<10)
	u8 magicControl;
	u16 autoWake;
} ConfigRid;

typedef struct {
	u16 len;
	u8 mac[ETH_ALEN];
	u16 mode;
	u16 errorCode;
	u16 sigQuality;
	u16 SSIDlen;
	char SSID[32];
	char apName[16];
	u8 bssid[4][ETH_ALEN];
	u16 beaconPeriod;
	u16 dimPeriod;
	u16 atimDuration;
	u16 hopPeriod;
	u16 channelSet;
	u16 channel;
	u16 hopsToBackbone;
	u16 apTotalLoad;
	u16 generatedLoad;
	u16 accumulatedArl;
	u16 signalQuality;
	u16 currentXmitRate;
	u16 apDevExtensions;
	u16 normalizedSignalStrength;
	u16 shortPreamble;
	u8 apIP[4];
	u8 noisePercent; /* Noise percent in last second */
	u8 noisedBm; /* Noise dBm in last second */
	u8 noiseAvePercent; /* Noise percent in last minute */
	u8 noiseAvedBm; /* Noise dBm in last minute */
	u8 noiseMaxPercent; /* Highest noise percent in last minute */
	u8 noiseMaxdBm; /* Highest noise dbm in last minute */
	u16 load;
	u8 carrier[4];
	u16 assocStatus;
#define STAT_NOPACKETS 0
#define STAT_NOCARRIERSET 10
#define STAT_GOTCARRIERSET 11
#define STAT_WRONGSSID 20
#define STAT_BADCHANNEL 25
#define STAT_BADBITRATES 30
#define STAT_BADPRIVACY 35
#define STAT_APFOUND 40
#define STAT_APREJECTED 50
#define STAT_AUTHENTICATING 60
#define STAT_DEAUTHENTICATED 61
#define STAT_AUTHTIMEOUT 62
#define STAT_ASSOCIATING 70
#define STAT_DEASSOCIATED 71
#define STAT_ASSOCTIMEOUT 72
#define STAT_NOTAIROAP 73
#define STAT_ASSOCIATED 80
#define STAT_LEAPING 90
#define STAT_LEAPFAILED 91
#define STAT_LEAPTIMEDOUT 92
#define STAT_LEAPCOMPLETE 93
} StatusRid;

typedef struct {
	u16 len;
	u16 spacer;
	u32 vals[100];
} StatsRid;


typedef struct {
	u16 len;
	u8 ap[4][ETH_ALEN];
} APListRid;

typedef struct {
	u16 len;
	char oui[3];
	char zero;
	u16 prodNum;
	char manName[32];
	char prodName[16];
	char prodVer[8];
	char factoryAddr[ETH_ALEN];
	char aironetAddr[ETH_ALEN];
	u16 radioType;
	u16 country;
	char callid[ETH_ALEN];
	char supportedRates[8];
	char rxDiversity;
	char txDiversity;
	u16 txPowerLevels[8];
	u16 hardVer;
	u16 hardCap;
	u16 tempRange;
	u16 softVer;
	u16 softSubVer;
	u16 interfaceVer;
	u16 softCap;
	u16 bootBlockVer;
	u16 requiredHard;
	u16 extSoftCap;
} CapabilityRid;

typedef struct {
  u16 len;
  u16 index; /* First is 0 and 0xffff means end of list */
#define RADIO_FH 1 /* Frequency hopping radio type */
#define RADIO_DS 2 /* Direct sequence radio type */
#define RADIO_TMA 4 /* Proprietary radio used in old cards (2500) */
  u16 radioType;
  u8 bssid[ETH_ALEN]; /* Mac address of the BSS */
  u8 zero;
  u8 ssidLen;
  u8 ssid[32];
  u16 rssi;
#define CAP_ESS (1<<0)
#define CAP_IBSS (1<<1)
#define CAP_PRIVACY (1<<4)
#define CAP_SHORTHDR (1<<5)
  u16 cap;
  u16 beaconInterval;
  u8 rates[8]; /* Same as rates for config rid */
  struct { /* For frequency hopping only */
    u16 dwell;
    u8 hopSet;
    u8 hopPattern;
    u8 hopIndex;
    u8 fill;
  } fh;
  u16 dsChannel;
  u16 atimWindow;
} BSSListRid;

typedef struct {
  u8 rssipct;
  u8 rssidBm;
} tdsRssiEntry;

typedef struct {
  u16 len;
  tdsRssiEntry x[256];
} tdsRssiRid;

typedef struct {
	u16 len;
	u16 state;
	u16 multicastValid;
	u8  multicast[16];
	u16 unicastValid;
	u8  unicast[16];
} MICRid;

typedef struct {
	u16 typelen;

	union {
	    u8 snap[8];
	    struct {
		u8 dsap;
		u8 ssap;
		u8 control;
		u8 orgcode[3];
		u8 fieldtype[2];
	    } llc;
	} u;
	u32 mic;
	u32 seq;
} MICBuffer;

typedef struct {
	u8 da[ETH_ALEN];
	u8 sa[ETH_ALEN];
} etherHead;

#pragma pack()

#define TXCTL_TXOK (1<<1) /* report if tx is ok */
#define TXCTL_TXEX (1<<2) /* report if tx fails */
#define TXCTL_802_3 (0<<3) /* 802.3 packet */
#define TXCTL_802_11 (1<<3) /* 802.11 mac packet */
#define TXCTL_ETHERNET (0<<4) /* payload has ethertype */
#define TXCTL_LLC (1<<4) /* payload is llc */
#define TXCTL_RELEASE (0<<5) /* release after completion */
#define TXCTL_NORELEASE (1<<5) /* on completion returns to host */

#define BUSY_FID 0x10000

#ifdef CISCO_EXT
#define AIROMAGIC	0xa55a
/* Warning : SIOCDEVPRIVATE may disapear during 2.5.X - Jean II */
#ifdef SIOCIWFIRSTPRIV
#ifdef SIOCDEVPRIVATE
#define AIROOLDIOCTL	SIOCDEVPRIVATE
#define AIROOLDIDIFC 	AIROOLDIOCTL + 1
#endif /* SIOCDEVPRIVATE */
#else /* SIOCIWFIRSTPRIV */
#define SIOCIWFIRSTPRIV SIOCDEVPRIVATE
#endif /* SIOCIWFIRSTPRIV */
/* This may be wrong. When using the new SIOCIWFIRSTPRIV range, we probably
 * should use only "GET" ioctls (last bit set to 1). "SET" ioctls are root
 * only and don't return the modified struct ifreq to the application which
 * is usually a problem. - Jean II */
#define AIROIOCTL	SIOCIWFIRSTPRIV
#define AIROIDIFC 	AIROIOCTL + 1

/* Ioctl constants to be used in airo_ioctl.command */

#define	AIROGCAP  		0	// Capability rid
#define AIROGCFG		1       // USED A LOT
#define AIROGSLIST		2	// System ID list
#define AIROGVLIST		3       // List of specified AP's
#define AIROGDRVNAM		4	//  NOTUSED
#define AIROGEHTENC		5	// NOTUSED
#define AIROGWEPKTMP		6
#define AIROGWEPKNV		7
#define AIROGSTAT		8
#define AIROGSTATSC32		9
#define AIROGSTATSD32		10
#define AIROGMICRID		11
#define AIROGMICSTATS		12
#define AIROGFLAGS		13
#define AIRORRID		15

/* Leave gap of 40 commands after AIROGSTATSD32 for future */

#define AIROPCAP               	AIROGSTATSD32 + 40
#define AIROPVLIST              AIROPCAP      + 1
#define AIROPSLIST		AIROPVLIST    + 1
#define AIROPCFG		AIROPSLIST    + 1
#define AIROPSIDS		AIROPCFG      + 1
#define AIROPAPLIST		AIROPSIDS     + 1
#define AIROPMACON		AIROPAPLIST   + 1	/* Enable mac  */
#define AIROPMACOFF		AIROPMACON    + 1 	/* Disable mac */
#define AIROPSTCLR		AIROPMACOFF   + 1
#define AIROPWEPKEY		AIROPSTCLR    + 1
#define AIROPWEPKEYNV		AIROPWEPKEY   + 1
#define AIROPLEAPPWD            AIROPWEPKEYNV + 1
#define AIROPLEAPUSR            AIROPLEAPPWD  + 1

/* Flash codes */

#define AIROFLSHRST	       AIROPWEPKEYNV  + 40
#define AIROFLSHGCHR           AIROFLSHRST    + 1
#define AIROFLSHSTFL           AIROFLSHGCHR   + 1
#define AIROFLSHPCHR           AIROFLSHSTFL   + 1
#define AIROFLPUTBUF           AIROFLSHPCHR   + 1
#define AIRORESTART            AIROFLPUTBUF   + 1

#define FLASHSIZE	32768

typedef struct aironet_ioctl {
	unsigned short command;	// What to do
	unsigned short len;		// Len of data
	unsigned char *data;		// d-data
} aironet_ioctl;
#endif /* CISCO_EXT */

#define NUM_MODULES       2
#define MIC_MSGLEN_MAX    2400
#define EMMH32_MSGLEN_MAX MIC_MSGLEN_MAX

typedef struct {
	u32   size;            // size
	u8    enabled;         // MIC enabled or not
	u32   rxSuccess;       // successful packets received
	u32   rxIncorrectMIC;  // pkts dropped due to incorrect MIC comparison
	u32   rxNotMICed;      // pkts dropped due to not being MIC'd
	u32   rxMICPlummed;    // pkts dropped due to not having a MIC plummed
	u32   rxWrongSequence; // pkts dropped due to sequence number violation
	u32   reserve[32];
} mic_statistics;

typedef struct {
	u32 coeff[((EMMH32_MSGLEN_MAX)+3)>>2];
	u64 accum;	// accumulated mic, reduced to u32 in final()
	int position;	// current position (byte offset) in message
	union {
		u8  d8[4];
		u32 d32;
	} part;	// saves partial message word across update() calls
} emmh32_context;

typedef struct {
	emmh32_context seed;	    // Context - the seed
	u32		 rx;	    // Received sequence number
	u32		 tx;	    // Tx sequence number
	u32		 window;    // Start of window
	u8		 valid;	    // Flag to say if context is valid or not
	u8		 key[16];
} miccntx;

typedef struct {
	miccntx mCtx;		// Multicast context
	miccntx uCtx;		// Unicast context
} mic_module;

#ifdef WIRELESS_EXT
// Frequency list (map channels to frequencies)
static const long frequency_list[] = { 2412, 2417, 2422, 2427, 2432, 2437, 2442,
				2447, 2452, 2457, 2462, 2467, 2472, 2484 };

// A few details needed for WEP (Wireless Equivalent Privacy)
#define MAX_KEY_SIZE 13			// 128 (?) bits
#define MIN_KEY_SIZE  5			// 40 bits RC4 - WEP
typedef struct wep_key_t {
	u16	len;
	u8	key[16];	/* 40-bit and 104-bit keys */
} wep_key_t;

/* Backward compatibility */
#ifndef IW_ENCODE_NOKEY
#define IW_ENCODE_NOKEY         0x0800  /* Key is write only, so not present */
#define IW_ENCODE_MODE  (IW_ENCODE_DISABLED | IW_ENCODE_RESTRICTED | IW_ENCODE_OPEN)
#endif /* IW_ENCODE_NOKEY */

#if WIRELESS_EXT > 12
/* List of Wireless Handlers (new API) */
static const struct iw_handler_def	airo_handler_def;
#else	/* WIRELESS_EXT > 12 */
/* More Wireless Extensions backward compatibility */
/* Part of iw_handler prototype we need (apart that we don't need it) */
struct iw_request_info {};
#endif	/* WIRELESS_EXT > 12 */
#endif /* WIRELESS_EXT */

static const char version[] = "airo.c 0.6 (Ben Reed & Javier Achirica)";

struct airo_info;

static int get_dec_u16( char *buffer, int *start, int limit );
static void OUT4500( struct airo_info *, u16 register, u16 value );
static unsigned short IN4500( struct airo_info *, u16 register );
static u16 setup_card(struct airo_info*, u8 *mac);
static int enable_MAC( struct airo_info *ai, Resp *rsp, int lock );
static void disable_MAC(struct airo_info *ai, int lock);
static void enable_interrupts(struct airo_info*);
static void disable_interrupts(struct airo_info*);
static u16 issuecommand(struct airo_info*, Cmd *pCmd, Resp *pRsp);
static int bap_setup(struct airo_info*, u16 rid, u16 offset, int whichbap);
static int aux_bap_read(struct airo_info*, u16 *pu16Dst, int bytelen,
			int whichbap);
static int fast_bap_read(struct airo_info*, u16 *pu16Dst, int bytelen,
			 int whichbap);
static int bap_write(struct airo_info*, const u16 *pu16Src, int bytelen,
		     int whichbap);
static int PC4500_accessrid(struct airo_info*, u16 rid, u16 accmd);
static int PC4500_readrid(struct airo_info*, u16 rid, void *pBuf, int len, int lock);
static int PC4500_writerid(struct airo_info*, u16 rid, const void
			   *pBuf, int len, int lock);
static int do_writerid( struct airo_info*, u16 rid, const void *rid_data,
			int len, int dummy );
static u16 transmit_allocate(struct airo_info*, int lenPayload, int raw);
static int transmit_802_3_packet(struct airo_info*, int len, char *pPacket);
static int transmit_802_11_packet(struct airo_info*, int len, char *pPacket);

static irqreturn_t airo_interrupt( int irq, void* dev_id, struct pt_regs
			    *regs);
static int airo_thread(void *data);
static void timer_func( struct net_device *dev );
static int airo_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
#ifdef WIRELESS_EXT
struct iw_statistics *airo_get_wireless_stats (struct net_device *dev);
static void airo_read_wireless_stats (struct airo_info *local);
#endif /* WIRELESS_EXT */
#ifdef CISCO_EXT
static int readrids(struct net_device *dev, aironet_ioctl *comp);
static int writerids(struct net_device *dev, aironet_ioctl *comp);
int flashcard(struct net_device *dev, aironet_ioctl *comp);
#endif /* CISCO_EXT */
#ifdef MICSUPPORT
static void micinit(struct airo_info *ai);
static int micsetup(struct airo_info *ai);
static int encapsulate(struct airo_info *ai, etherHead *pPacket, MICBuffer *buffer, int len);
static int decapsulate(struct airo_info *ai, MICBuffer *mic, etherHead *pPacket, u16 payLen);

#include <linux/crypto.h>
#endif

struct airo_info {
	struct net_device_stats	stats;
	int open;
	struct net_device             *dev;
	/* Note, we can have MAX_FIDS outstanding.  FIDs are 16-bits, so we
	   use the high bit to mark whether it is in use. */
#define MAX_FIDS 6
	int                           fids[MAX_FIDS];
	int registered;
	ConfigRid config;
	int need_commit;	// Need to set config
	char keyindex; // Used with auto wep
	char defindex; // Used with auto wep
	struct proc_dir_entry *proc_entry;
	struct airo_info *next;
        spinlock_t aux_lock;
        unsigned long flags;
#define FLAG_PROMISC	8	/* IFF_PROMISC 0x100 - include/linux/if.h */
#define FLAG_RADIO_OFF	0	/* User disabling of MAC */
#define FLAG_RADIO_DOWN	1	/* ifup/ifdown disabling of MAC */
#define FLAG_RADIO_MASK 0x03
#define FLAG_FLASHING	2
#define FLAG_ADHOC	3	/* Needed by MIC */
#define FLAG_MIC_CAPABLE 4
#define FLAG_UPDATE_MULTI 5
#define FLAG_UPDATE_UNI 6
#define FLAG_802_11	7
#define FLAG_PENDING_XMIT 9
#define FLAG_PENDING_XMIT11 10
#define FLAG_PCI	11
#define JOB_MASK	0x1ff0000
#define JOB_DIE		16
#define JOB_XMIT	17
#define JOB_XMIT11	18
#define JOB_STATS	19
#define JOB_PROMISC	20
#define JOB_MIC		21
#define JOB_EVENT	22
#define JOB_AUTOWEP	23
#define JOB_WSTATS	24
	int (*bap_read)(struct airo_info*, u16 *pu16Dst, int bytelen,
			int whichbap);
	unsigned short *flash;
	tdsRssiEntry *rssi;
	struct task_struct *task;
	struct semaphore sem;
	pid_t thr_pid;
	wait_queue_head_t thr_wait;
	struct completion thr_exited;
	unsigned long expires;
	struct {
		struct sk_buff *skb;
		int fid;
	} xmit, xmit11;
	struct net_device *wifidev;
#ifdef WIRELESS_EXT
	struct iw_statistics	wstats;		// wireless stats
	unsigned long		scan_timestamp;	/* Time started to scan */
#if WIRELESS_EXT > 15
	struct iw_spy_data	spy_data;
#else /* WIRELESS_EXT > 15 */
#ifdef WIRELESS_SPY
	int			spy_number;
	u_char			spy_address[IW_MAX_SPY][ETH_ALEN];
	struct iw_quality	spy_stat[IW_MAX_SPY];
#endif /* WIRELESS_SPY */
#endif /* WIRELESS_EXT > 15 */
#endif /* WIRELESS_EXT */
#ifdef MICSUPPORT
	/* MIC stuff */
	struct crypto_tfm	*tfm;
	mic_module		mod[2];
	mic_statistics		micstats;
#endif
};

static inline int bap_read(struct airo_info *ai, u16 *pu16Dst, int bytelen,
			   int whichbap) {
	return ai->bap_read(ai, pu16Dst, bytelen, whichbap);
}

static int setup_proc_entry( struct net_device *dev,
			     struct airo_info *apriv );
static int takedown_proc_entry( struct net_device *dev,
				struct airo_info *apriv );

#ifdef MICSUPPORT
/***********************************************************************
 *                              MIC ROUTINES                           *
 ***********************************************************************
 */

static int RxSeqValid (struct airo_info *ai,miccntx *context,int mcast,u32 micSeq);
static void MoveWindow(miccntx *context, u32 micSeq);
void emmh32_setseed(emmh32_context *context, u8 *pkey, int keylen, struct crypto_tfm *);
void emmh32_init(emmh32_context *context);
void emmh32_update(emmh32_context *context, u8 *pOctets, int len);
void emmh32_final(emmh32_context *context, u8 digest[4]);

/* micinit - Initialize mic seed */

static void micinit(struct airo_info *ai)
{
	MICRid mic_rid;

	clear_bit(JOB_MIC, &ai->flags);
	PC4500_readrid(ai, RID_MIC, &mic_rid, sizeof(mic_rid), 0);
	up(&ai->sem);

	ai->micstats.enabled = (mic_rid.state & 0x00FF) ? 1 : 0;

	if (ai->micstats.enabled) {
		/* Key must be valid and different */
		if (mic_rid.multicastValid && (!ai->mod[0].mCtx.valid ||
		    (memcmp (ai->mod[0].mCtx.key, mic_rid.multicast,
			     sizeof(ai->mod[0].mCtx.key)) != 0))) {
			/* Age current mic Context */
			memcpy(&ai->mod[1].mCtx,&ai->mod[0].mCtx,sizeof(miccntx));
			/* Initialize new context */
			memcpy(&ai->mod[0].mCtx.key,mic_rid.multicast,sizeof(mic_rid.multicast));
			ai->mod[0].mCtx.window  = 33; //Window always points to the middle
			ai->mod[0].mCtx.rx      = 0;  //Rx Sequence numbers
			ai->mod[0].mCtx.tx      = 0;  //Tx sequence numbers
			ai->mod[0].mCtx.valid   = 1;  //Key is now valid
  
			/* Give key to mic seed */
			emmh32_setseed(&ai->mod[0].mCtx.seed,mic_rid.multicast,sizeof(mic_rid.multicast), ai->tfm);
		}

		/* Key must be valid and different */
		if (mic_rid.unicastValid && (!ai->mod[0].uCtx.valid || 
		    (memcmp(ai->mod[0].uCtx.key, mic_rid.unicast,
			    sizeof(ai->mod[0].uCtx.key)) != 0))) {
			/* Age current mic Context */
			memcpy(&ai->mod[1].uCtx,&ai->mod[0].uCtx,sizeof(miccntx));
			/* Initialize new context */
			memcpy(&ai->mod[0].uCtx.key,mic_rid.unicast,sizeof(mic_rid.unicast));
	
			ai->mod[0].uCtx.window  = 33; //Window always points to the middle
			ai->mod[0].uCtx.rx      = 0;  //Rx Sequence numbers
			ai->mod[0].uCtx.tx      = 0;  //Tx sequence numbers
			ai->mod[0].uCtx.valid   = 1;  //Key is now valid
	
			//Give key to mic seed
			emmh32_setseed(&ai->mod[0].uCtx.seed, mic_rid.unicast, sizeof(mic_rid.unicast), ai->tfm);
		}
	} else {
      /* So next time we have a valid key and mic is enabled, we will update
       * the sequence number if the key is the same as before.
       */
		ai->mod[0].uCtx.valid = 0;
		ai->mod[0].mCtx.valid = 0;
	}
}

/* micsetup - Get ready for business */

static int micsetup(struct airo_info *ai) {
	int i;

	if (ai->tfm == NULL)
	        ai->tfm = crypto_alloc_tfm("aes", 0);

        if (ai->tfm == NULL) {
                printk(KERN_ERR "airo: failed to load transform for AES\n");
                return ERROR;
        }

	for (i=0; i < NUM_MODULES; i++) {
		memset(&ai->mod[i].mCtx,0,sizeof(miccntx));
		memset(&ai->mod[i].uCtx,0,sizeof(miccntx));
	}
	return SUCCESS;
}

char micsnap[]= {0xAA,0xAA,0x03,0x00,0x40,0x96,0x00,0x02};

/*===========================================================================
 * Description: Mic a packet
 *    
 *      Inputs: etherHead * pointer to an 802.3 frame
 *    
 *     Returns: BOOLEAN if successful, otherwise false.
 *             PacketTxLen will be updated with the mic'd packets size.
 *
 *    Caveats: It is assumed that the frame buffer will already
 *             be big enough to hold the largets mic message possible.
 *            (No memory allocation is done here).
 *  
 *    Author: sbraneky (10/15/01)
 *    Merciless hacks by rwilcher (1/14/02)
 */

static int encapsulate(struct airo_info *ai ,etherHead *frame, MICBuffer *mic, int payLen)
{
	miccntx   *context;

	// Determine correct context
	// If not adhoc, always use unicast key

	if (test_bit(FLAG_ADHOC, &ai->flags) && (frame->da[0] & 0x1))
		context = &ai->mod[0].mCtx;
	else
		context = &ai->mod[0].uCtx;
  
	if (!context->valid)
		return ERROR;

	mic->typelen = htons(payLen + 16); //Length of Mic'd packet

	memcpy(&mic->u.snap, micsnap, sizeof(micsnap)); // Add Snap

	// Add Tx sequence
	mic->seq = htonl(context->tx);
	context->tx += 2;

	emmh32_init(&context->seed); // Mic the packet
	emmh32_update(&context->seed,frame->da,ETH_ALEN * 2); // DA,SA
	emmh32_update(&context->seed,(u8*)&mic->typelen,10); // Type/Length and Snap
	emmh32_update(&context->seed,(u8*)&mic->seq,sizeof(mic->seq)); //SEQ
	emmh32_update(&context->seed,frame->da + ETH_ALEN * 2,payLen); //payload
	emmh32_final(&context->seed, (u8*)&mic->mic);

	/*    New Type/length ?????????? */
	mic->typelen = 0; //Let NIC know it could be an oversized packet
	return SUCCESS;
}

typedef enum {
    NONE,
    NOMIC,
    NOMICPLUMMED,
    SEQUENCE,
    INCORRECTMIC,
} mic_error;

/*===========================================================================
 *  Description: Decapsulates a MIC'd packet and returns the 802.3 packet
 *               (removes the MIC stuff) if packet is a valid packet.
 *      
 *       Inputs: etherHead  pointer to the 802.3 packet             
 *     
 *      Returns: BOOLEAN - TRUE if packet should be dropped otherwise FALSE
 *     
 *      Author: sbraneky (10/15/01)
 *    Merciless hacks by rwilcher (1/14/02)
 *---------------------------------------------------------------------------
 */

static int decapsulate(struct airo_info *ai, MICBuffer *mic, etherHead *eth, u16 payLen)
{
	int      i;
	u32      micSEQ;
	miccntx  *context;
	u8       digest[4];
	mic_error micError = NONE;

	// Check if the packet is a Mic'd packet

	if (!ai->micstats.enabled) {
		//No Mic set or Mic OFF but we received a MIC'd packet.
		if (memcmp ((u8*)eth + 14, micsnap, sizeof(micsnap)) == 0) {
			ai->micstats.rxMICPlummed++;
			return ERROR;
		}
		return SUCCESS;
	}

	if (ntohs(mic->typelen) == 0x888E)
		return SUCCESS;

	if (memcmp (mic->u.snap, micsnap, sizeof(micsnap)) != 0) {
	    // Mic enabled but packet isn't Mic'd
		ai->micstats.rxMICPlummed++;
	    	return ERROR;
	}

	micSEQ = ntohl(mic->seq);            //store SEQ as CPU order

	//At this point we a have a mic'd packet and mic is enabled
	//Now do the mic error checking.

	//Receive seq must be odd
	if ( (micSEQ & 1) == 0 ) {
		ai->micstats.rxWrongSequence++;
		return ERROR;
	}

	for (i = 0; i < NUM_MODULES; i++) {
		int mcast = eth->da[0] & 1;
		//Determine proper context 
		context = mcast ? &ai->mod[i].mCtx : &ai->mod[i].uCtx;
	
		//Make sure context is valid
		if (!context->valid) {
			if (i == 0)
				micError = NOMICPLUMMED;
			continue;                
		}
	       	//DeMic it 

		if (!mic->typelen)
			mic->typelen = htons(payLen + sizeof(MICBuffer) - 2);
	
		emmh32_init(&context->seed);
		emmh32_update(&context->seed, eth->da, ETH_ALEN*2); 
		emmh32_update(&context->seed, (u8 *)&mic->typelen, sizeof(mic->typelen)+sizeof(mic->u.snap)); 
		emmh32_update(&context->seed, (u8 *)&mic->seq,sizeof(mic->seq));	
		emmh32_update(&context->seed, eth->da + ETH_ALEN*2,payLen);	
		//Calculate MIC
		emmh32_final(&context->seed, digest);
	
		if (memcmp(digest, &mic->mic, 4)) { //Make sure the mics match
		  //Invalid Mic
			if (i == 0)
				micError = INCORRECTMIC;
			continue;
		}

		//Check Sequence number if mics pass
		if (RxSeqValid(ai, context, mcast, micSEQ) == SUCCESS) {
			ai->micstats.rxSuccess++;
			return SUCCESS;
		}
		if (i == 0)
			micError = SEQUENCE;
	}

	// Update statistics
	switch (micError) {
		case NOMICPLUMMED: ai->micstats.rxMICPlummed++;   break;
		case SEQUENCE:    ai->micstats.rxWrongSequence++; break;
		case INCORRECTMIC: ai->micstats.rxIncorrectMIC++; break;
		case NONE:  break;
		case NOMIC: break;
	}
	return ERROR;
}

/*===========================================================================
 * Description:  Checks the Rx Seq number to make sure it is valid
 *               and hasn't already been received
 *   
 *     Inputs: miccntx - mic context to check seq against
 *             micSeq  - the Mic seq number
 *   
 *    Returns: TRUE if valid otherwise FALSE. 
 *
 *    Author: sbraneky (10/15/01)
 *    Merciless hacks by rwilcher (1/14/02)
 *---------------------------------------------------------------------------
 */

static int RxSeqValid (struct airo_info *ai,miccntx *context,int mcast,u32 micSeq)
{
	u32 seq,index;

	//Allow for the ap being rebooted - if it is then use the next 
	//sequence number of the current sequence number - might go backwards

	if (mcast) {
		if (test_bit(FLAG_UPDATE_MULTI, &ai->flags)) {
			clear_bit (FLAG_UPDATE_MULTI, &ai->flags);
			context->window = (micSeq > 33) ? micSeq : 33;
			context->rx     = 0;        // Reset rx
		}
	} else if (test_bit(FLAG_UPDATE_UNI, &ai->flags)) {
		clear_bit (FLAG_UPDATE_UNI, &ai->flags);
		context->window = (micSeq > 33) ? micSeq : 33; // Move window
		context->rx     = 0;        // Reset rx
	}

	//Make sequence number relative to START of window
	seq = micSeq - (context->window - 33);

	//Too old of a SEQ number to check.
	if ((u32)seq < 0)
		return ERROR;
    
	if ( seq > 64 ) {
		//Window is infinite forward
		MoveWindow(context,micSeq);
		return SUCCESS;
	}

	// We are in the window. Now check the context rx bit to see if it was already sent
	seq >>= 1;         //divide by 2 because we only have odd numbers
	index = 1 << seq;  //Get an index number

	if (!(context->rx & index)) {
		//micSEQ falls inside the window.
		//Add seqence number to the list of received numbers.
		context->rx |= index;

		MoveWindow(context,micSeq);

		return SUCCESS;
	}
	return ERROR;
}

static void MoveWindow(miccntx *context, u32 micSeq)
{
	u32 shift;

	//Move window if seq greater than the middle of the window
	if (micSeq > context->window) {
		shift = (micSeq - context->window) >> 1;
    
		    //Shift out old
		if (shift < 32)
			context->rx >>= shift;
		else
			context->rx = 0;

		context->window = micSeq;      //Move window
	}
}

/*==============================================*/
/*========== EMMH ROUTINES  ====================*/
/*==============================================*/

/* mic accumulate */
#define MIC_ACCUM(val)	\
	context->accum += (u64)(val) * context->coeff[coeff_position++];

static unsigned char aes_counter[16];

/* expand the key to fill the MMH coefficient array */
void emmh32_setseed(emmh32_context *context, u8 *pkey, int keylen, struct crypto_tfm *tfm)
{
  /* take the keying material, expand if necessary, truncate at 16-bytes */
  /* run through AES counter mode to generate context->coeff[] */
  
	int i,j;
	u32 counter;
	u8 *cipher, plain[16];
	struct scatterlist sg[1];

	crypto_cipher_setkey(tfm, pkey, 16);
	counter = 0;
	for (i = 0; i < (sizeof(context->coeff)/sizeof(context->coeff[0])); ) {
		aes_counter[15] = (u8)(counter >> 0);
		aes_counter[14] = (u8)(counter >> 8);
		aes_counter[13] = (u8)(counter >> 16);
		aes_counter[12] = (u8)(counter >> 24);
		counter++;
		memcpy (plain, aes_counter, 16);
		sg[0].page = virt_to_page(plain);
		sg[0].offset = ((long) plain & ~PAGE_MASK);
		sg[0].length = 16;
		crypto_cipher_encrypt(tfm, sg, sg, 16);
		cipher = kmap(sg[0].page) + sg[0].offset;
		for (j=0; (j<16) && (i< (sizeof(context->coeff)/sizeof(context->coeff[0]))); ) {
			context->coeff[i++] = ntohl(*(u32 *)&cipher[j]);
			j += 4;
		}
	}
}

/* prepare for calculation of a new mic */
void emmh32_init(emmh32_context *context)
{
	/* prepare for new mic calculation */
	context->accum = 0;
	context->position = 0;
}

/* add some bytes to the mic calculation */
void emmh32_update(emmh32_context *context, u8 *pOctets, int len)
{
	int	coeff_position, byte_position;
  
	if (len == 0) return;
  
	coeff_position = context->position >> 2;
  
	/* deal with partial 32-bit word left over from last update */
	byte_position = context->position & 3;
	if (byte_position) {
		/* have a partial word in part to deal with */
		do {
			if (len == 0) return;
			context->part.d8[byte_position++] = *pOctets++;
			context->position++;
			len--;
		} while (byte_position < 4);
		MIC_ACCUM(htonl(context->part.d32));
	}

	/* deal with full 32-bit words */
	while (len >= 4) {
		MIC_ACCUM(htonl(*(u32 *)pOctets));
		context->position += 4;
		pOctets += 4;
		len -= 4;
	}

	/* deal with partial 32-bit word that will be left over from this update */
	byte_position = 0;
	while (len > 0) {
		context->part.d8[byte_position++] = *pOctets++;
		context->position++;
		len--;
	}
}

/* mask used to zero empty bytes for final partial word */
static u32 mask32[4] = { 0x00000000L, 0xFF000000L, 0xFFFF0000L, 0xFFFFFF00L };

/* calculate the mic */
void emmh32_final(emmh32_context *context, u8 digest[4])
{
	int	coeff_position, byte_position;
	u32	val;
  
	u64 sum, utmp;
	s64 stmp;

	coeff_position = context->position >> 2;
  
	/* deal with partial 32-bit word left over from last update */
	byte_position = context->position & 3;
	if (byte_position) {
		/* have a partial word in part to deal with */
		val = htonl(context->part.d32);
		MIC_ACCUM(val & mask32[byte_position]);	/* zero empty bytes */
	}

	/* reduce the accumulated u64 to a 32-bit MIC */
	sum = context->accum;
	stmp = (sum  & 0xffffffffLL) - ((sum >> 32)  * 15);
	utmp = (stmp & 0xffffffffLL) - ((stmp >> 32) * 15);
	sum = utmp & 0xffffffffLL;
	if (utmp > 0x10000000fLL)
		sum -= 15;

	val = (u32)sum;
	digest[0] = (val>>24) & 0xFF;
	digest[1] = (val>>16) & 0xFF;
	digest[2] = (val>>8) & 0xFF;
	digest[3] = val & 0xFF;
}
#endif

static int readBSSListRid(struct airo_info *ai, int first,
		      BSSListRid *list) {
	int rc;
			Cmd cmd;
			Resp rsp;

	if (first == 1) {
			memset(&cmd, 0, sizeof(cmd));
			cmd.cmd=CMD_LISTBSS;
			if (down_interruptible(&ai->sem))
				return -ERESTARTSYS;
			issuecommand(ai, &cmd, &rsp);
			up(&ai->sem);
			/* Let the command take effect */
			set_current_state (TASK_INTERRUPTIBLE);
			ai->task = current;
			schedule_timeout (3*HZ);
			ai->task = NULL;
		}
	rc = PC4500_readrid(ai, first ? RID_BSSLISTFIRST : RID_BSSLISTNEXT,
			    list, sizeof(*list), 1);

	list->len = le16_to_cpu(list->len);
	list->index = le16_to_cpu(list->index);
	list->radioType = le16_to_cpu(list->radioType);
	list->cap = le16_to_cpu(list->cap);
	list->beaconInterval = le16_to_cpu(list->beaconInterval);
	list->fh.dwell = le16_to_cpu(list->fh.dwell);
	list->dsChannel = le16_to_cpu(list->dsChannel);
	list->atimWindow = le16_to_cpu(list->atimWindow);
	return rc;
}

static int readWepKeyRid(struct airo_info*ai, WepKeyRid *wkr, int temp) {
	int rc = PC4500_readrid(ai, temp ? RID_WEP_TEMP : RID_WEP_PERM,
				wkr, sizeof(*wkr), 1);

	wkr->len = le16_to_cpu(wkr->len);
	wkr->kindex = le16_to_cpu(wkr->kindex);
	wkr->klen = le16_to_cpu(wkr->klen);
	return rc;
}
/* In the writeXXXRid routines we copy the rids so that we don't screwup
 * the originals when we endian them... */
static int writeWepKeyRid(struct airo_info*ai, WepKeyRid *pwkr, int perm, int lock) {
	int rc;
	WepKeyRid wkr = *pwkr;

	wkr.len = cpu_to_le16(wkr.len);
	wkr.kindex = cpu_to_le16(wkr.kindex);
	wkr.klen = cpu_to_le16(wkr.klen);
	rc = PC4500_writerid(ai, RID_WEP_TEMP, &wkr, sizeof(wkr), lock);
	if (rc!=SUCCESS) printk(KERN_ERR "airo:  WEP_TEMP set %x\n", rc);
	if (perm) {
		rc = PC4500_writerid(ai, RID_WEP_PERM, &wkr, sizeof(wkr), lock);
		if (rc!=SUCCESS) {
			printk(KERN_ERR "airo:  WEP_PERM set %x\n", rc);
		}
	}
	return rc;
}

static int readSsidRid(struct airo_info*ai, SsidRid *ssidr) {
	int i;
	int rc = PC4500_readrid(ai, RID_SSID, ssidr, sizeof(*ssidr), 1);

	ssidr->len = le16_to_cpu(ssidr->len);
	for(i = 0; i < 3; i++) {
		ssidr->ssids[i].len = le16_to_cpu(ssidr->ssids[i].len);
	}
	return rc;
}
static int writeSsidRid(struct airo_info*ai, SsidRid *pssidr) {
	int rc;
	int i;
	SsidRid ssidr = *pssidr;

	ssidr.len = cpu_to_le16(ssidr.len);
	for(i = 0; i < 3; i++) {
		ssidr.ssids[i].len = cpu_to_le16(ssidr.ssids[i].len);
	}
	rc = PC4500_writerid(ai, RID_SSID, &ssidr, sizeof(ssidr), 1);
	return rc;
}
static int readConfigRid(struct airo_info*ai, int lock) {
	int rc;
	u16 *s;
	ConfigRid cfg;

	if (ai->config.len)
		return SUCCESS;

	rc = PC4500_readrid(ai, RID_ACTUALCONFIG, &cfg, sizeof(cfg), lock);
	if (rc != SUCCESS)
		return rc;

	for(s = &cfg.len; s <= &cfg.rtsThres; s++) *s = le16_to_cpu(*s);

	for(s = &cfg.shortRetryLimit; s <= &cfg.radioType; s++)
		*s = le16_to_cpu(*s);

	for(s = &cfg.txPower; s <= &cfg.radioSpecific; s++)
		*s = le16_to_cpu(*s);

	for(s = &cfg.arlThreshold; s <= &cfg.autoWake; s++)
		*s = le16_to_cpu(*s);

	ai->config = cfg;
	return SUCCESS;
}
static inline void checkThrottle(struct airo_info *ai) {
	int i;
/* Old hardware had a limit on encryption speed */
	if (ai->config.authType != AUTH_OPEN && maxencrypt) {
		for(i=0; i<8; i++) {
			if (ai->config.rates[i] > maxencrypt) {
				ai->config.rates[i] = 0;
			}
		}
	}
}
static int writeConfigRid(struct airo_info*ai, int lock) {
	u16 *s;
	ConfigRid cfgr;

	if (!ai->need_commit)
		return SUCCESS;

	ai->need_commit = 0;
	checkThrottle(ai);
	cfgr = ai->config;

	if ((cfgr.opmode & 0xFF) == MODE_STA_IBSS)
		set_bit(FLAG_ADHOC, &ai->flags);
	else
		clear_bit(FLAG_ADHOC, &ai->flags);

	for(s = &cfgr.len; s <= &cfgr.rtsThres; s++) *s = cpu_to_le16(*s);

	for(s = &cfgr.shortRetryLimit; s <= &cfgr.radioType; s++)
		*s = cpu_to_le16(*s);

	for(s = &cfgr.txPower; s <= &cfgr.radioSpecific; s++)
		*s = cpu_to_le16(*s);

	for(s = &cfgr.arlThreshold; s <= &cfgr.autoWake; s++)
		*s = cpu_to_le16(*s);

	return PC4500_writerid( ai, RID_CONFIG, &cfgr, sizeof(cfgr), lock);
}
static int readStatusRid(struct airo_info*ai, StatusRid *statr, int lock) {
	int rc = PC4500_readrid(ai, RID_STATUS, statr, sizeof(*statr), lock);
	u16 *s;

	statr->len = le16_to_cpu(statr->len);
	for(s = &statr->mode; s <= &statr->SSIDlen; s++) *s = le16_to_cpu(*s);

	for(s = &statr->beaconPeriod; s <= &statr->shortPreamble; s++)
		*s = le16_to_cpu(*s);
	statr->load = le16_to_cpu(statr->load);
	statr->assocStatus = le16_to_cpu(statr->assocStatus);
	return rc;
}
static int readAPListRid(struct airo_info*ai, APListRid *aplr) {
	int rc =  PC4500_readrid(ai, RID_APLIST, aplr, sizeof(*aplr), 1);
	aplr->len = le16_to_cpu(aplr->len);
	return rc;
}
static int writeAPListRid(struct airo_info*ai, APListRid *aplr) {
	int rc;
	aplr->len = cpu_to_le16(aplr->len);
	rc = PC4500_writerid(ai, RID_APLIST, aplr, sizeof(*aplr), 1);
	return rc;
}
static int readCapabilityRid(struct airo_info*ai, CapabilityRid *capr) {
	int rc = PC4500_readrid(ai, RID_CAPABILITIES, capr, sizeof(*capr), 1);
	u16 *s;

	capr->len = le16_to_cpu(capr->len);
	capr->prodNum = le16_to_cpu(capr->prodNum);
	capr->radioType = le16_to_cpu(capr->radioType);
	capr->country = le16_to_cpu(capr->country);
	for(s = &capr->txPowerLevels[0]; s <= &capr->requiredHard; s++)
		*s = le16_to_cpu(*s);
	return rc;
}
static int readStatsRid(struct airo_info*ai, StatsRid *sr, int rid, int lock) {
	int rc = PC4500_readrid(ai, rid, sr, sizeof(*sr), lock);
	u32 *i;

	sr->len = le16_to_cpu(sr->len);
	for(i = &sr->vals[0]; i <= &sr->vals[99]; i++) *i = le32_to_cpu(*i);
	return rc;
}

static int airo_open(struct net_device *dev) {
	struct airo_info *info = dev->priv;
	Resp rsp;

	if (test_bit(FLAG_FLASHING, &info->flags))
		return -EIO;

	/* Make sure the card is configured.
	 * Wireless Extensions may postpone config changes until the card
	 * is open (to pipeline changes and speed-up card setup). If
	 * those changes are not yet commited, do it now - Jean II */
	if(info->need_commit) {
		disable_MAC(info, 1);
		writeConfigRid(info, 1);
	}

	if (info->wifidev != dev) {
		/* Power on the MAC controller (which may have been disabled) */
		clear_bit(FLAG_RADIO_DOWN, &info->flags);
		enable_interrupts(info);
	}
	enable_MAC(info, &rsp, 1);

	netif_start_queue(dev);
	return 0;
}

static void get_tx_error(struct airo_info *ai, u32 fid)
{
	u16 status;

	if (bap_setup(ai, ai->fids[fid] & 0xffff, 4, BAP0) == SUCCESS) {
		bap_read(ai, &status, 2, BAP0);
		if (le16_to_cpu(status) & 2) /* Too many retries */
			ai->stats.tx_aborted_errors++;
		if (le16_to_cpu(status) & 4) /* Transmit lifetime exceeded */
			ai->stats.tx_heartbeat_errors++;
		if (le16_to_cpu(status) & 8) /* Aid fail */
			{ }
		if (le16_to_cpu(status) & 0x10) /* MAC disabled */
			ai->stats.tx_carrier_errors++;
		if (le16_to_cpu(status) & 0x20) /* Association lost */
			{ }
#if WIRELESS_EXT > 13
		/* We produce a TXDROP event only for retry or lifetime
		 * exceeded, because that's the only status that really mean
		 * that this particular node went away.
		 * Other errors means that *we* screwed up. - Jean II */
		if ((le16_to_cpu(status) & 2) ||
		     (le16_to_cpu(status) & 4)) {
			union iwreq_data	wrqu;
			char junk[0x18];

			/* Faster to skip over useless data than to do
			 * another bap_setup(). We are at offset 0x6 and
			 * need to go to 0x18 and read 6 bytes - Jean II */
			bap_read(ai, (u16 *) junk, 0x18, BAP0);

			/* Copy 802.11 dest address.
			 * We use the 802.11 header because the frame may
			 * not be 802.3 or may be mangled...
			 * In Ad-Hoc mode, it will be the node address.
			 * In managed mode, it will be most likely the AP addr
			 * User space will figure out how to convert it to
			 * whatever it needs (IP address or else).
			 * - Jean II */
			memcpy(wrqu.addr.sa_data, junk + 0x12, ETH_ALEN);
			wrqu.addr.sa_family = ARPHRD_ETHER;

			/* Send event to user space */
			wireless_send_event(ai->dev, IWEVTXDROP, &wrqu, NULL);
		}
#endif /* WIRELESS_EXT > 13 */
	}
}

static void airo_end_xmit(struct net_device *dev) {
	u16 status;
	int i;
	struct airo_info *priv = dev->priv;
	struct sk_buff *skb = priv->xmit.skb;
	int fid = priv->xmit.fid;
	u32 *fids = priv->fids;

	clear_bit(JOB_XMIT, &priv->flags);
	clear_bit(FLAG_PENDING_XMIT, &priv->flags);
	status = transmit_802_3_packet (priv, fids[fid], skb->data);
	up(&priv->sem);

	i = 0;
	if ( status == SUCCESS ) {
		dev->trans_start = jiffies;
		for (; i < MAX_FIDS / 2 && (priv->fids[i] & 0xffff0000); i++);
	} else {
		priv->fids[fid] &= 0xffff;
		priv->stats.tx_window_errors++;
	}
	if (i < MAX_FIDS / 2)
		netif_wake_queue(dev);
	dev_kfree_skb(skb);
}

static int airo_start_xmit(struct sk_buff *skb, struct net_device *dev) {
	s16 len;
	int i, j;
	struct airo_info *priv = dev->priv;
	u32 *fids = priv->fids;

	if ( skb == NULL ) {
		printk( KERN_ERR "airo:  skb == NULL!!!\n" );
		return 0;
	}

	/* Find a vacant FID */
	for( i = 0; i < MAX_FIDS / 2 && (fids[i] & 0xffff0000); i++ );
	for( j = i + 1; j < MAX_FIDS / 2 && (fids[j] & 0xffff0000); j++ );

	if ( j >= MAX_FIDS / 2 ) {
		netif_stop_queue(dev);

		if (i == MAX_FIDS / 2) {
			priv->stats.tx_fifo_errors++;
			return 1;
		}
	}
	/* check min length*/
	len = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
        /* Mark fid as used & save length for later */
	fids[i] |= (len << 16);
	priv->xmit.skb = skb;
	priv->xmit.fid = i;
	if (down_trylock(&priv->sem) != 0) {
		set_bit(FLAG_PENDING_XMIT, &priv->flags);
		netif_stop_queue(dev);
		set_bit(JOB_XMIT, &priv->flags);
		wake_up_interruptible(&priv->thr_wait);
	} else
		airo_end_xmit(dev);
	return 0;
}

static void airo_end_xmit11(struct net_device *dev) {
	u16 status;
	int i;
	struct airo_info *priv = dev->priv;
	struct sk_buff *skb = priv->xmit11.skb;
	int fid = priv->xmit11.fid;
	u32 *fids = priv->fids;

	clear_bit(JOB_XMIT11, &priv->flags);
	clear_bit(FLAG_PENDING_XMIT11, &priv->flags);
	status = transmit_802_11_packet (priv, fids[fid], skb->data);
	up(&priv->sem);

	i = MAX_FIDS / 2;
	if ( status == SUCCESS ) {
		dev->trans_start = jiffies;
		for (; i < MAX_FIDS && (priv->fids[i] & 0xffff0000); i++);
	} else {
		priv->fids[fid] &= 0xffff;
		priv->stats.tx_window_errors++;
	}
	if (i < MAX_FIDS)
		netif_wake_queue(dev);
	dev_kfree_skb(skb);
}

static int airo_start_xmit11(struct sk_buff *skb, struct net_device *dev) {
	s16 len;
	int i, j;
	struct airo_info *priv = dev->priv;
	u32 *fids = priv->fids;

	if ( skb == NULL ) {
		printk( KERN_ERR "airo:  skb == NULL!!!\n" );
		return 0;
	}

	/* Find a vacant FID */
	for( i = MAX_FIDS / 2; i < MAX_FIDS && (fids[i] & 0xffff0000); i++ );
	for( j = i + 1; j < MAX_FIDS && (fids[j] & 0xffff0000); j++ );

	if ( j >= MAX_FIDS ) {
		netif_stop_queue(dev);

		if (i == MAX_FIDS) {
			priv->stats.tx_fifo_errors++;
			return 1;
		}
	}
	/* check min length*/
	len = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
        /* Mark fid as used & save length for later */
	fids[i] |= (len << 16);
	priv->xmit11.skb = skb;
	priv->xmit11.fid = i;
	if (down_trylock(&priv->sem) != 0) {
		set_bit(FLAG_PENDING_XMIT11, &priv->flags);
		netif_stop_queue(dev);
		set_bit(JOB_XMIT11, &priv->flags);
		wake_up_interruptible(&priv->thr_wait);
	} else
		airo_end_xmit11(dev);
	return 0;
}

static void airo_read_stats(struct airo_info *ai) {
	StatsRid stats_rid;
	u32 *vals = stats_rid.vals;

	clear_bit(JOB_STATS, &ai->flags);
	readStatsRid(ai, &stats_rid, RID_STATS, 0);
	up(&ai->sem);

	ai->stats.rx_packets = vals[43] + vals[44] + vals[45];
	ai->stats.tx_packets = vals[39] + vals[40] + vals[41];
	ai->stats.rx_bytes = vals[92];
	ai->stats.tx_bytes = vals[91];
	ai->stats.rx_errors = vals[0] + vals[2] + vals[3] + vals[4];
	ai->stats.tx_errors = vals[42] + ai->stats.tx_fifo_errors;
	ai->stats.multicast = vals[43];
	ai->stats.collisions = vals[89];

	/* detailed rx_errors: */
	ai->stats.rx_length_errors = vals[3];
	ai->stats.rx_crc_errors = vals[4];
	ai->stats.rx_frame_errors = vals[2];
	ai->stats.rx_fifo_errors = vals[0];
}

struct net_device_stats *airo_get_stats(struct net_device *dev)
{
	struct airo_info *local =  dev->priv;

	/* Get stats out of the card if available */
	if (down_trylock(&local->sem) != 0) {
		set_bit(JOB_STATS, &local->flags);
		wake_up_interruptible(&local->thr_wait);
	} else
		airo_read_stats(local);

	return &local->stats;
}

static void airo_set_promisc(struct airo_info *ai) {
	Cmd cmd;
	Resp rsp;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd=CMD_SETMODE;
	clear_bit(JOB_PROMISC, &ai->flags);
	cmd.parm0=(ai->flags&IFF_PROMISC) ? PROMISC : NOPROMISC;
	issuecommand(ai, &cmd, &rsp);
	up(&ai->sem);
}

static void airo_set_multicast_list(struct net_device *dev) {
	struct airo_info *ai = dev->priv;

	if ((dev->flags ^ ai->flags) & IFF_PROMISC) {
		change_bit(FLAG_PROMISC, &ai->flags);
		if (down_trylock(&ai->sem) != 0) {
			set_bit(JOB_PROMISC, &ai->flags);
			wake_up_interruptible(&ai->thr_wait);
		} else
			airo_set_promisc(ai);
	}

	if ((dev->flags&IFF_ALLMULTI)||dev->mc_count>0) {
		/* Turn on multicast.  (Should be already setup...) */
	}
}

static int airo_set_mac_address(struct net_device *dev, void *p)
{
	struct airo_info *ai = dev->priv;
	struct sockaddr *addr = p;
	Resp rsp;

	readConfigRid(ai, 1);
	memcpy (ai->config.macAddr, addr->sa_data, dev->addr_len);
	ai->need_commit = 1;
	disable_MAC(ai, 1);
	writeConfigRid (ai, 1);
	enable_MAC(ai, &rsp, 1);
	memcpy (ai->dev->dev_addr, addr->sa_data, dev->addr_len);
	if (ai->wifidev)
		memcpy (ai->wifidev->dev_addr, addr->sa_data, dev->addr_len);
	return 0;
}

static int airo_change_mtu(struct net_device *dev, int new_mtu)
{
	if ((new_mtu < 68) || (new_mtu > 2400))
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}


static int airo_close(struct net_device *dev) {
	struct airo_info *ai = dev->priv;

	netif_stop_queue(dev);

	if (ai->wifidev != dev) {
#ifdef POWER_ON_DOWN
		/* Shut power to the card. The idea is that the user can save
		 * power when he doesn't need the card with "ifconfig down".
		 * That's the method that is most friendly towards the network
		 * stack (i.e. the network stack won't try to broadcast
		 * anything on the interface and routes are gone. Jean II */
		set_bit(FLAG_RADIO_DOWN, &ai->flags);
		disable_MAC(ai, 1);
#endif
		disable_interrupts( ai );
	}
	return 0;
}

static void del_airo_dev( struct net_device *dev );

void stop_airo_card( struct net_device *dev, int freeres )
{
	struct airo_info *ai = dev->priv;
	disable_interrupts(ai);
	free_irq( dev->irq, dev );
	takedown_proc_entry( dev, ai );
	if (ai->registered) {
		unregister_netdev( dev );
		if (ai->wifidev) {
			unregister_netdev(ai->wifidev);
			free_netdev(ai->wifidev);
			ai->wifidev = 0;
		}
		ai->registered = 0;
	}
	set_bit(JOB_DIE, &ai->flags);
	kill_proc(ai->thr_pid, SIGTERM, 1);
	wait_for_completion(&ai->thr_exited);
	if (ai->flash)
		kfree(ai->flash);
	if (ai->rssi)
		kfree(ai->rssi);
	if (freeres) {
		/* PCMCIA frees this stuff, so only for PCI and ISA */
	        release_region( dev->base_addr, 64 );
        }
#ifdef MICSUPPORT
	if (ai->tfm)
		crypto_free_tfm(ai->tfm);
#endif
	del_airo_dev( dev );
	free_netdev( dev );
}

EXPORT_SYMBOL(stop_airo_card);

static int add_airo_dev( struct net_device *dev );

int wll_header_parse(struct sk_buff *skb, unsigned char *haddr)
{
	memcpy(haddr, skb->mac.raw + 10, ETH_ALEN);
	return ETH_ALEN;
}

static void wifi_setup(struct net_device *dev, struct net_device *ethdev)
{
	struct airo_info *ai = ethdev->priv;
	dev->priv = ai;
	dev->hard_header        = 0;
	dev->rebuild_header     = 0;
	dev->hard_header_cache  = 0;
	dev->header_cache_update= 0;

	dev->hard_header_parse  = wll_header_parse;
	dev->hard_start_xmit = &airo_start_xmit11;
	dev->get_stats = &airo_get_stats;
	dev->set_mac_address = &airo_set_mac_address;
	dev->do_ioctl = &airo_ioctl;
#ifdef WIRELESS_EXT
	dev->get_wireless_stats = airo_get_wireless_stats;
#if WIRELESS_EXT > 12
	dev->wireless_handlers = (struct iw_handler_def *)&airo_handler_def;
#endif /* WIRELESS_EXT > 12 */
#endif /* WIRELESS_EXT */
	dev->change_mtu = &airo_change_mtu;
	dev->open = &airo_open;
	dev->stop = &airo_close;
	dev->irq = ethdev->irq;
	dev->base_addr = ethdev->base_addr;

	dev->type               = ARPHRD_IEEE80211;
	dev->hard_header_len    = ETH_HLEN;
	dev->mtu                = 2312;
	dev->addr_len           = ETH_ALEN;
	memcpy(dev->dev_addr, ethdev->dev_addr, dev->addr_len);
	dev->tx_queue_len       = 100; 

	memset(dev->broadcast,0xFF, ETH_ALEN);

	dev->flags              = IFF_BROADCAST|IFF_MULTICAST;
}

static struct net_device *init_wifidev(struct airo_info *ai,
					struct net_device *ethdev)
{
	int err;
	struct net_device *dev = (struct net_device*)kmalloc(sizeof *dev,GFP_KERNEL);
	if (!dev) return 0;
	memset(dev, 0, sizeof(*dev));

	strcpy(dev->name, "wifi%d");
	dev->priv = ai;
	wifi_setup(dev, ethdev);
	err = register_netdev(dev);
	if (err<0) {
		kfree(dev);
		return 0;
	}
	return dev;
}

struct net_device *init_airo_card( unsigned short irq, int port, int is_pcmcia )
{
	struct net_device *dev;
	struct airo_info *ai;
	int i, rc;

	/* Create the network device object. */
        dev = alloc_etherdev(sizeof(*ai));
        if (!dev) {
		printk(KERN_ERR "airo:  Couldn't alloc_etherdev\n");
		return NULL;
        }
	if (dev_alloc_name(dev, dev->name) < 0) {
		printk(KERN_ERR "airo:  Couldn't get name!\n");
		goto err_out_free;
	}

	ai = dev->priv;
	ai->wifidev = 0;
	ai->registered = 0;
        ai->dev = dev;
	ai->aux_lock = SPIN_LOCK_UNLOCKED;
	sema_init(&ai->sem, 1);
	ai->need_commit = 0;
	ai->config.len = 0;
	init_waitqueue_head (&ai->thr_wait);
	init_completion (&ai->thr_exited);
	ai->thr_pid = kernel_thread(airo_thread, dev, CLONE_FS | CLONE_FILES);
	if (ai->thr_pid < 0)
		goto err_out_free;
#ifdef MICSUPPORT
	ai->tfm = NULL;
#endif
	rc = add_airo_dev( dev );
	if (rc)
		goto err_out_thr;

	/* The Airo-specific entries in the device structure. */
	dev->hard_start_xmit = &airo_start_xmit;
	dev->get_stats = &airo_get_stats;
	dev->set_multicast_list = &airo_set_multicast_list;
	dev->set_mac_address = &airo_set_mac_address;
	dev->do_ioctl = &airo_ioctl;
#ifdef WIRELESS_EXT
	dev->get_wireless_stats = airo_get_wireless_stats;
#if WIRELESS_EXT > 12
	dev->wireless_handlers = (struct iw_handler_def *)&airo_handler_def;
#endif /* WIRELESS_EXT > 12 */
#endif /* WIRELESS_EXT */
	dev->change_mtu = &airo_change_mtu;
	dev->open = &airo_open;
	dev->stop = &airo_close;
	dev->irq = irq;
	dev->base_addr = port;

	rc = request_irq( dev->irq, airo_interrupt, SA_SHIRQ, dev->name, dev );
	if (rc) {
		printk(KERN_ERR "airo: register interrupt %d failed, rc %d\n", irq, rc );
		goto err_out_unlink;
	}
	if (!is_pcmcia) {
		if (!request_region( dev->base_addr, 64, dev->name )) {
			rc = -EBUSY;
			goto err_out_irq;
		}
	}

	if (probe) {
		if ( setup_card( ai, dev->dev_addr ) != SUCCESS ) {
			printk( KERN_ERR "airo: MAC could not be enabled\n" );
			rc = -EIO;
			goto err_out_res;
		}
	} else {
		ai->bap_read = fast_bap_read;
		set_bit(FLAG_FLASHING, &ai->flags);
	}

	rc = register_netdev(dev);
	if (rc)
		goto err_out_res;
	ai->wifidev = init_wifidev(ai, dev);

	ai->registered = 1;
	printk( KERN_INFO "airo: MAC enabled %s %x:%x:%x:%x:%x:%x\n",
		dev->name,
		dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
		dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5] );

	/* Allocate the transmit buffers */
	if (probe)
		for( i = 0; i < MAX_FIDS; i++ )
			ai->fids[i] = transmit_allocate(ai,2312,i>=MAX_FIDS/2);

	setup_proc_entry( dev, dev->priv ); /* XXX check for failure */
	netif_start_queue(dev);
	SET_MODULE_OWNER(dev);
	return dev;

err_out_res:
	if (!is_pcmcia)
	        release_region( dev->base_addr, 64 );
err_out_irq:
	free_irq(dev->irq, dev);
err_out_unlink:
	del_airo_dev(dev);
err_out_thr:
	set_bit(JOB_DIE, &ai->flags);
	kill_proc(ai->thr_pid, SIGTERM, 1);
	wait_for_completion(&ai->thr_exited);
err_out_free:
	kfree(dev);
	return NULL;
}

EXPORT_SYMBOL(init_airo_card);

static int waitbusy (struct airo_info *ai) {
	int delay = 0;
	while ((IN4500 (ai, COMMAND) & COMMAND_BUSY) & (delay < 10000)) {
		udelay (10);
		if (++delay % 20)
			OUT4500(ai, EVACK, EV_CLEARCOMMANDBUSY);
	}
	return delay < 10000;
}

int reset_airo_card( struct net_device *dev ) {
	int i;
	struct airo_info *ai = dev->priv;


	if (down_interruptible(&ai->sem))
		return -1;
	waitbusy (ai);
	OUT4500(ai,COMMAND,CMD_SOFTRESET);
	set_current_state (TASK_UNINTERRUPTIBLE);
	schedule_timeout (HZ/5);
	waitbusy (ai);
	set_current_state (TASK_UNINTERRUPTIBLE);
	schedule_timeout (HZ/5);
	if ( setup_card(ai, dev->dev_addr ) != SUCCESS ) {
		printk( KERN_ERR "airo: MAC could not be enabled\n" );
		up(&ai->sem);
		return -1;
	} else {
		printk( KERN_INFO "airo: MAC enabled %s %x:%x:%x:%x:%x:%x\n",
			dev->name,
			dev->dev_addr[0],
			dev->dev_addr[1],
			dev->dev_addr[2],
			dev->dev_addr[3],
			dev->dev_addr[4],
			dev->dev_addr[5]
			);
		/* Allocate the transmit buffers */
		for( i = 0; i < MAX_FIDS; i++ )
			ai->fids[i] = transmit_allocate(ai,2312,i>=MAX_FIDS/2);
	}
	enable_interrupts( ai );
	netif_wake_queue(dev);
	up(&ai->sem);
	return 0;
}

EXPORT_SYMBOL(reset_airo_card);

#if WIRELESS_EXT > 13
static void airo_send_event(struct net_device *dev) {
	struct airo_info *ai = dev->priv;
	union iwreq_data wrqu;
	StatusRid status_rid;

	clear_bit(JOB_EVENT, &ai->flags);
	PC4500_readrid(ai, RID_STATUS, &status_rid, sizeof(status_rid), 0);
	up(&ai->sem);
	wrqu.data.length = 0;
	wrqu.data.flags = 0;
	memcpy(wrqu.ap_addr.sa_data, status_rid.bssid[0], ETH_ALEN);
	wrqu.ap_addr.sa_family = ARPHRD_ETHER;

	/* Send event to user space */
	wireless_send_event(dev, SIOCGIWAP, &wrqu, NULL);
}
#endif

static int airo_thread(void *data) {
	struct net_device *dev = data;
	struct airo_info *ai = dev->priv;
	int locked;
	
	daemonize();
	reparent_to_init();
	spin_lock_irq(&current->sigmask_lock);
	sigemptyset(&current->blocked);
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	strncpy (current->comm, dev->name, sizeof(current->comm) - 1);
	current->comm[sizeof(current->comm) - 1] = '\0';

	while(1) {
		if (signal_pending(current)) {
			spin_lock_irq(&current->sigmask_lock);
			flush_signals(current);
			spin_unlock_irq(&current->sigmask_lock);
		}

		if (test_bit(JOB_DIE, &ai->flags))
			break;

		if (ai->flags & JOB_MASK) {
			locked = down_interruptible(&ai->sem);
		} else {
			wait_queue_t wait;

			init_waitqueue_entry(&wait, current);
			add_wait_queue(&ai->thr_wait, &wait);
			for (;;) {
				set_current_state(TASK_INTERRUPTIBLE);
				if (ai->flags & JOB_MASK)
					break;
				if (ai->expires) {
					if (time_after_eq(jiffies,ai->expires)){
						set_bit(JOB_AUTOWEP,&ai->flags);
						break;
					}
					if (!signal_pending(current)) {
						schedule_timeout(ai->expires - jiffies);
						continue;
					}
				} else if (!signal_pending(current)) {
					schedule();
					continue;
				}
				break;
			}
			current->state = TASK_RUNNING;
			remove_wait_queue(&ai->thr_wait, &wait);
			locked = 1;
		}

		if (locked)
			continue;

		if (test_bit(JOB_DIE, &ai->flags)) {
			up(&ai->sem);
			break;
		}

		if (test_bit(FLAG_FLASHING, &ai->flags)) {
			up(&ai->sem);
			continue;
		}

		if (test_bit(JOB_XMIT, &ai->flags))
			airo_end_xmit(dev);
		else if (test_bit(JOB_XMIT11, &ai->flags))
			airo_end_xmit11(dev);
		else if (test_bit(JOB_STATS, &ai->flags))
			airo_read_stats(ai);
		else if (test_bit(JOB_WSTATS, &ai->flags))
			airo_read_wireless_stats(ai);
		else if (test_bit(JOB_PROMISC, &ai->flags))
			airo_set_promisc(ai);
#ifdef MICSUPPORT
		else if (test_bit(JOB_MIC, &ai->flags))
			micinit(ai);
#endif
#if WIRELESS_EXT > 13
		else if (test_bit(JOB_EVENT, &ai->flags))
			airo_send_event(dev);
#endif
		else if (test_bit(JOB_AUTOWEP, &ai->flags))
			timer_func(dev);
	}
	complete_and_exit (&ai->thr_exited, 0);
}

static irqreturn_t airo_interrupt ( int irq, void* dev_id, struct pt_regs *regs) {
	struct net_device *dev = (struct net_device *)dev_id;
	u16 status;
	u16 fid;
	struct airo_info *apriv = dev->priv;
	u16 savedInterrupts = 0;
	int handled = 0;

	if (!netif_device_present(dev))
		return IRQ_NONE;

	for (;;) {
		status = IN4500( apriv, EVSTAT );
		if ( !(status & STATUS_INTS) || status == 0xffff ) break;

		handled = 1;

		if ( status & EV_AWAKE ) {
			OUT4500( apriv, EVACK, EV_AWAKE );
			OUT4500( apriv, EVACK, EV_AWAKE );
		}

		if (!savedInterrupts) {
			savedInterrupts = IN4500( apriv, EVINTEN );
			OUT4500( apriv, EVINTEN, 0 );
		}

		if ( status & EV_MIC ) {
			OUT4500( apriv, EVACK, EV_MIC );
#ifdef MICSUPPORT
			if (test_bit(FLAG_MIC_CAPABLE, &apriv->flags)) {
				set_bit(JOB_MIC, &apriv->flags);
				wake_up_interruptible(&apriv->thr_wait);
			}
#endif
		}
		if ( status & EV_LINK ) {
#if WIRELESS_EXT > 13
			union iwreq_data	wrqu;
#endif /* WIRELESS_EXT > 13 */
			/* The link status has changed, if you want to put a
			   monitor hook in, do it here.  (Remember that
			   interrupts are still disabled!)
			*/
			u16 newStatus = IN4500(apriv, LINKSTAT);
			OUT4500( apriv, EVACK, EV_LINK);
			/* Here is what newStatus means: */
#define NOBEACON 0x8000 /* Loss of sync - missed beacons */
#define MAXRETRIES 0x8001 /* Loss of sync - max retries */
#define MAXARL 0x8002 /* Loss of sync - average retry level exceeded*/
#define FORCELOSS 0x8003 /* Loss of sync - host request */
#define TSFSYNC 0x8004 /* Loss of sync - TSF synchronization */
#define DEAUTH 0x8100 /* Deauthentication (low byte is reason code) */
#define DISASS 0x8200 /* Disassociation (low byte is reason code) */
#define ASSFAIL 0x8400 /* Association failure (low byte is reason
			  code) */
#define AUTHFAIL 0x0300 /* Authentication failure (low byte is reason
			   code) */
#define ASSOCIATED 0x0400 /* Assocatied */
#define RC_RESERVED 0 /* Reserved return code */
#define RC_NOREASON 1 /* Unspecified reason */
#define RC_AUTHINV 2 /* Previous authentication invalid */
#define RC_DEAUTH 3 /* Deauthenticated because sending station is
		       leaving */
#define RC_NOACT 4 /* Disassociated due to inactivity */
#define RC_MAXLOAD 5 /* Disassociated because AP is unable to handle
			all currently associated stations */
#define RC_BADCLASS2 6 /* Class 2 frame received from
			  non-Authenticated station */
#define RC_BADCLASS3 7 /* Class 3 frame received from
			  non-Associated station */
#define RC_STATLEAVE 8 /* Disassociated because sending station is
			  leaving BSS */
#define RC_NOAUTH 9 /* Station requesting (Re)Association is not
		       Authenticated with the responding station */
			if (newStatus != ASSOCIATED) {
				if (auto_wep && !apriv->expires) {
					apriv->expires = RUN_AT(3*HZ);
					wake_up_interruptible(&apriv->thr_wait);
				}
			} else {
				struct task_struct *task = apriv->task;
				if (auto_wep)
					apriv->expires = 0;
				if (task)
					wake_up_process (task);
				set_bit(FLAG_UPDATE_UNI, &apriv->flags);
				set_bit(FLAG_UPDATE_MULTI, &apriv->flags);
			}
#if WIRELESS_EXT > 13
			/* Question : is ASSOCIATED the only status
			 * that is valid ? We want to catch handover
			 * and reassociations as valid status
			 * Jean II */
			if(newStatus == ASSOCIATED) {
				if (apriv->scan_timestamp) {
					/* Send an empty event to user space.
					 * We don't send the received data on
					 * the event because it would require
					 * us to do complex transcoding, and
					 * we want to minimise the work done in
					 * the irq handler. Use a request to
					 * extract the data - Jean II */
					wrqu.data.length = 0;
					wrqu.data.flags = 0;
					wireless_send_event(dev, SIOCGIWSCAN, &wrqu, NULL);
					apriv->scan_timestamp = 0;
				}
				if (down_trylock(&apriv->sem) != 0) {
					set_bit(JOB_EVENT, &apriv->flags);
					wake_up_interruptible(&apriv->thr_wait);
				} else
					airo_send_event(dev);
			} else {
				memset(wrqu.ap_addr.sa_data, '\0', ETH_ALEN);
				wrqu.ap_addr.sa_family = ARPHRD_ETHER;

				/* Send event to user space */
				wireless_send_event(dev, SIOCGIWAP, &wrqu,NULL);
			}
#endif /* WIRELESS_EXT > 13 */
		}

		/* Check to see if there is something to receive */
		if ( status & EV_RX  ) {
			struct sk_buff *skb = NULL;
			u16 fc, len, hdrlen = 0;
#pragma pack(1)
			struct {
				u16 status, len;
				u8 rssi[2];
				u8 rate;
				u8 freq;
				u16 tmp[4];
			} hdr;
#pragma pack()
			u16 gap;
			u16 tmpbuf[4];
			u16 *buffer;

			fid = IN4500( apriv, RXFID );

			/* Get the packet length */
			if (test_bit(FLAG_802_11, &apriv->flags)) {
				bap_setup (apriv, fid, 4, BAP0);
				bap_read (apriv, (u16*)&hdr, sizeof(hdr), BAP0);
				/* Bad CRC. Ignore packet */
				if (le16_to_cpu(hdr.status) & 2)
					hdr.len = 0;
				if (apriv->wifidev == NULL)
					hdr.len = 0;
			} else {
				bap_setup (apriv, fid, 0x36, BAP0);
				bap_read (apriv, (u16*)&hdr.len, 2, BAP0);
			}
			len = le16_to_cpu(hdr.len);

			if (len > 2312) {
				printk( KERN_ERR "airo: Bad size %d\n", len );
				goto badrx;
			}
			if (len == 0)
				goto badrx;

			if (test_bit(FLAG_802_11, &apriv->flags)) {
				bap_read (apriv, (u16*)&fc, sizeof(fc), BAP0);
				fc = le16_to_cpu(fc);
				switch (fc & 0xc) {
					case 4:
						if ((fc & 0xe0) == 0xc0)
							hdrlen = 10;
						else
							hdrlen = 16;
						break;
					case 8:
						if ((fc&0x300)==0x300){
							hdrlen = 30;
							break;
						}
					default:
						hdrlen = 24;
				}
			} else
				hdrlen = ETH_ALEN * 2;

			skb = dev_alloc_skb( len + hdrlen + 2 );
			if ( !skb ) {
				apriv->stats.rx_dropped++;
				goto badrx;
			}
			buffer = (u16*)skb_put (skb, len + hdrlen);
			if (test_bit(FLAG_802_11, &apriv->flags)) {
				buffer[0] = fc;
				bap_read (apriv, buffer + 1, hdrlen - 2, BAP0);
				if (hdrlen == 24)
					bap_read (apriv, tmpbuf, 6, BAP0);

				bap_read (apriv, &gap, sizeof(gap), BAP0);
				gap = le16_to_cpu(gap);
				if (gap) {
					if (gap <= 8)
						bap_read (apriv, tmpbuf, gap, BAP0);
					else
						printk(KERN_ERR "airo: gaplen too big. Problems will follow...\n");
				}
				bap_read (apriv, buffer + hdrlen/2, len, BAP0);
			} else {
#ifdef MICSUPPORT
				MICBuffer micbuf;
#endif
				bap_read (apriv, buffer, ETH_ALEN*2, BAP0);
#ifdef MICSUPPORT
				if (apriv->micstats.enabled) {
					bap_read (apriv,(u16*)&micbuf,sizeof(micbuf),BAP0);
					if (ntohs(micbuf.typelen) > 0x05DC)
						bap_setup (apriv, fid, 0x44, BAP0);
					else {
						if (len <= sizeof(micbuf))
							goto badmic;

						len -= sizeof(micbuf);
						skb_trim (skb, len + hdrlen);
					}
				}
#endif
				bap_read(apriv,buffer+ETH_ALEN,len,BAP0);
#ifdef MICSUPPORT
				if (decapsulate(apriv,&micbuf,(etherHead*)buffer,len)) {
badmic:
					dev_kfree_skb_irq (skb);
#else
				if (0) {
#endif
badrx:
					OUT4500( apriv, EVACK, EV_RX);
					goto exitrx;
				}
			}
#if WIRELESS_EXT > 15
#ifdef IW_WIRELESS_SPY		/* defined in iw_handler.h */
			if (apriv->spy_data.spy_number > 0) {
				char *sa;
				struct iw_quality wstats;
				/* Prepare spy data : addr + qual */
				if (!test_bit(FLAG_802_11, &apriv->flags)) {
					sa = (char*)buffer + 6;
					bap_setup (apriv, fid, 8, BAP0);
					bap_read (apriv, (u16*)hdr.rssi, 2, BAP0);
				} else
					sa = (char*)buffer + 10;
				wstats.qual = hdr.rssi[0];
				if (apriv->rssi)
					wstats.level = 0x100 - apriv->rssi[hdr.rssi[1]].rssidBm;
				else
					wstats.level = (hdr.rssi[1] + 321) / 2;
				wstats.updated = 3;	
				/* Update spy records */
				wireless_spy_update(dev, sa, &wstats);
			}
#endif /* IW_WIRELESS_SPY */
#else /* WIRELESS_EXT > 15 */
#ifdef WIRELESS_SPY
			if (apriv->spy_number > 0) {
				int i;
				char *sa;

				sa = (char*)buffer + (test_bit(FLAG_802_11, &apriv->flags) ? 10 : 6);

				for (i=0; i<apriv->spy_number; i++)
					if (!memcmp(sa,apriv->spy_address[i],ETH_ALEN))
					{
						if (!test_bit(FLAG_802_11, &apriv->flags)) {
							bap_setup (apriv, fid, 8, BAP0);
							bap_read (apriv, (u16*)hdr.rssi, 2, BAP0);
						}
						apriv->spy_stat[i].qual = hdr.rssi[0];
						if (apriv->rssi)
							apriv->spy_stat[i].level = 0x100 - apriv->rssi[hdr.rssi[1]].rssidBm;
						else
							apriv->spy_stat[i].level = (hdr.rssi[1] + 321) / 2;
						apriv->spy_stat[i].noise = 0;
						apriv->spy_stat[i].updated = 3;
						break;
					}
			}
#endif /* WIRELESS_SPY  */
#endif /* WIRELESS_EXT > 15 */
			OUT4500( apriv, EVACK, EV_RX);

			if (test_bit(FLAG_802_11, &apriv->flags)) {
				skb->mac.raw = skb->data;
				skb->pkt_type = PACKET_OTHERHOST;
				skb->dev = apriv->wifidev;
				skb->protocol = htons(ETH_P_802_2);
			} else {
				skb->dev = dev;
				skb->protocol = eth_type_trans(skb,dev);
			}
			skb->dev->last_rx = jiffies;
			skb->ip_summed = CHECKSUM_NONE;

			netif_rx( skb );
		}
exitrx:

		/* Check to see if a packet has been transmitted */
		if (  status & ( EV_TX|EV_TXEXC ) ) {
			int i;
			int len = 0;
			int index = -1;

			fid = IN4500(apriv, TXCOMPLFID);

			for( i = 0; i < MAX_FIDS; i++ ) {
				if ( ( apriv->fids[i] & 0xffff ) == fid ) {
					len = apriv->fids[i] >> 16;
					index = i;
				}
			}
			if (index != -1) {
				if (status & EV_TXEXC)
					get_tx_error(apriv, index);
				OUT4500( apriv, EVACK, status & (EV_TX | EV_TXEXC));
				/* Set up to be used again */
				apriv->fids[index] &= 0xffff;
				if (index < MAX_FIDS / 2) {
					if (!test_bit(FLAG_PENDING_XMIT, &apriv->flags))
						netif_wake_queue(dev);
				} else {
					if (!test_bit(FLAG_PENDING_XMIT11, &apriv->flags))
						netif_wake_queue(apriv->wifidev);
				}
			} else {
				OUT4500( apriv, EVACK, status & (EV_TX | EV_TXEXC));
				printk( KERN_ERR "airo: Unallocated FID was used to xmit\n" );
			}
		}
		if ( status & ~STATUS_INTS & ~IGNORE_INTS )
			printk( KERN_WARNING "airo: Got weird status %x\n",
				status & ~STATUS_INTS & ~IGNORE_INTS );
	}

	if (savedInterrupts)
		OUT4500( apriv, EVINTEN, savedInterrupts );

	/* done.. */
	return IRQ_RETVAL(handled);
}

/*
 *  Routines to talk to the card
 */

/*
 *  This was originally written for the 4500, hence the name
 *  NOTE:  If use with 8bit mode and SMP bad things will happen!
 *         Why would some one do 8 bit IO in an SMP machine?!?
 */
static void OUT4500( struct airo_info *ai, u16 reg, u16 val ) {
	if ( !do8bitIO )
		outw( val, ai->dev->base_addr + reg );
	else {
		outb( val & 0xff, ai->dev->base_addr + reg );
		outb( val >> 8, ai->dev->base_addr + reg + 1 );
	}
}

static u16 IN4500( struct airo_info *ai, u16 reg ) {
	unsigned short rc;

	if ( !do8bitIO )
		rc = inw( ai->dev->base_addr + reg );
	else {
		rc = inb( ai->dev->base_addr + reg );
		rc += ((int)inb( ai->dev->base_addr + reg + 1 )) << 8;
	}
	return rc;
}

static int enable_MAC( struct airo_info *ai, Resp *rsp, int lock ) {
	int rc;
        Cmd cmd;

	/* FLAG_RADIO_OFF : Radio disabled via /proc or Wireless Extensions
	 * FLAG_RADIO_DOWN : Radio disabled via "ifconfig ethX down"
	 * Note : we could try to use !netif_running(dev) in enable_MAC()
	 * instead of this flag, but I don't trust it *within* the
	 * open/close functions, and testing both flags together is
	 * "cheaper" - Jean II */
	if (ai->flags & FLAG_RADIO_MASK) return SUCCESS;
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = MAC_ENABLE;
	if (!lock)
		return issuecommand(ai, &cmd, rsp);

	if (down_interruptible(&ai->sem))
		return -ERESTARTSYS;
	rc = issuecommand(ai, &cmd, rsp);
	up(&ai->sem);
	return rc;
}

static void disable_MAC( struct airo_info *ai, int lock ) {
        Cmd cmd;
	Resp rsp;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = MAC_DISABLE; // disable in case already enabled
	if (!lock) {
		issuecommand(ai, &cmd, &rsp);
		return;
	}

	if (down_interruptible(&ai->sem))
		return;
	issuecommand(ai, &cmd, &rsp);
	up(&ai->sem);
}

static void enable_interrupts( struct airo_info *ai ) {
	/* Reset the status register */
	u16 status = IN4500( ai, EVSTAT );
	OUT4500( ai, EVACK, status );
	/* Enable the interrupts */
	OUT4500( ai, EVINTEN, STATUS_INTS );
	/* Note there is a race condition between the last two lines that
	   I don't know how to get rid of right now... */
}

static void disable_interrupts( struct airo_info *ai ) {
	OUT4500( ai, EVINTEN, 0 );
}

static u16 setup_card(struct airo_info *ai, u8 *mac)
{
	Cmd cmd;
	Resp rsp;
	int status;
	int i;
	SsidRid mySsid;
	u16 lastindex;
	WepKeyRid wkr;
	int rc;

	memset( &mySsid, 0, sizeof( mySsid ) );
	if (ai->flash) {
		kfree (ai->flash);
		ai->flash = NULL;
	}

	/* The NOP is the first step in getting the card going */
	cmd.cmd = NOP;
	cmd.parm0 = cmd.parm1 = cmd.parm2 = 0;
	if (down_interruptible(&ai->sem))
		return ERROR;
	if ( issuecommand( ai, &cmd, &rsp ) != SUCCESS ) {
		up(&ai->sem);
		return ERROR;
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = MAC_DISABLE; // disable in case already enabled
	if ( issuecommand( ai, &cmd, &rsp ) != SUCCESS ) {
		up(&ai->sem);
		return ERROR;
	}

	// Let's figure out if we need to use the AUX port
	cmd.cmd = CMD_ENABLEAUX;
	if (issuecommand(ai, &cmd, &rsp) != SUCCESS) {
		up(&ai->sem);
		printk(KERN_ERR "airo: Error checking for AUX port\n");
		return ERROR;
	}
	if (!aux_bap || rsp.status & 0xff00) {
		ai->bap_read = fast_bap_read;
		printk(KERN_DEBUG "airo: Doing fast bap_reads\n");
	} else {
		ai->bap_read = aux_bap_read;
		printk(KERN_DEBUG "airo: Doing AUX bap_reads\n");
	}
	up(&ai->sem);
	if (ai->config.len == 0) {
		tdsRssiRid rssi_rid;
		CapabilityRid cap_rid;

		// general configuration (read/modify/write)
		status = readConfigRid(ai, 1);
		if ( status != SUCCESS ) return ERROR;

		status = readCapabilityRid(ai, &cap_rid);
		if ( status != SUCCESS ) return ERROR;

		status = PC4500_readrid(ai,RID_RSSI,&rssi_rid,sizeof(rssi_rid),1);
		if ( status == SUCCESS ) {
			if (ai->rssi || (ai->rssi = kmalloc(512, GFP_KERNEL)) != NULL)
				memcpy(ai->rssi, (u8*)&rssi_rid + 2, 512);
		}
		else {
			if (ai->rssi) {
				kfree(ai->rssi);
				ai->rssi = NULL;
			}
			if (cap_rid.softCap & 8)
				ai->config.rmode |= RXMODE_NORMALIZED_RSSI;
			else
				printk(KERN_WARNING "airo: unknown received signal level scale\n");
		}
		ai->config.opmode = adhoc ? MODE_STA_IBSS : MODE_STA_ESS;
		ai->config.authType = AUTH_OPEN;
		ai->config.modulation = MOD_CCK;

#ifdef MICSUPPORT
		if ((cap_rid.len>=sizeof(cap_rid)) && (cap_rid.extSoftCap&1) &&
		    (micsetup(ai) == SUCCESS)) {
			ai->config.opmode |= MODE_MIC;
			set_bit(FLAG_MIC_CAPABLE, &ai->flags);
		}
#endif

		/* Save off the MAC */
		for( i = 0; i < ETH_ALEN; i++ ) {
			mac[i] = ai->config.macAddr[i];
		}

		/* Check to see if there are any insmod configured
		   rates to add */
		if ( rates ) {
			int i = 0;
			if ( rates[0] ) memset(ai->config.rates,0,sizeof(ai->config.rates));
			for( i = 0; i < 8 && rates[i]; i++ ) {
				ai->config.rates[i] = rates[i];
			}
		}
		if ( basic_rate > 0 ) {
			int i;
			for( i = 0; i < 8; i++ ) {
				if ( ai->config.rates[i] == basic_rate ||
				     !ai->config.rates ) {
					ai->config.rates[i] = basic_rate | 0x80;
					break;
				}
			}
		}
		ai->need_commit = 1;
	}

	/* Setup the SSIDs if present */
	if ( ssids[0] ) {
		int i;
		for( i = 0; i < 3 && ssids[i]; i++ ) {
			mySsid.ssids[i].len = strlen(ssids[i]);
			if ( mySsid.ssids[i].len > 32 )
				mySsid.ssids[i].len = 32;
			memcpy(mySsid.ssids[i].ssid, ssids[i],
			       mySsid.ssids[i].len);
		}
		mySsid.len = sizeof(mySsid);
	}

	status = writeConfigRid(ai, 1);
	if ( status != SUCCESS ) return ERROR;

	/* Set up the SSID list */
	status = writeSsidRid(ai, &mySsid);
	if ( status != SUCCESS ) return ERROR;

	status = enable_MAC(ai, &rsp, 1);
	if ( status != SUCCESS || (rsp.status & 0xFF00) != 0) {
		printk( KERN_ERR "airo: Bad MAC enable reason = %x, rid = %x, offset = %d\n", rsp.rsp0, rsp.rsp1, rsp.rsp2 );
		return ERROR;
	}

	/* Grab the initial wep key, we gotta save it for auto_wep */
	rc = readWepKeyRid(ai, &wkr, 1);
	if (rc == SUCCESS) do {
		lastindex = wkr.kindex;
		if (wkr.kindex == 0xffff) {
			ai->defindex = wkr.mac[0];
		}
		rc = readWepKeyRid(ai, &wkr, 0);
	} while(lastindex != wkr.kindex);

	if (auto_wep) {
		ai->expires = RUN_AT(3*HZ);
		wake_up_interruptible(&ai->thr_wait);
	}

	return SUCCESS;
}

static u16 issuecommand(struct airo_info *ai, Cmd *pCmd, Resp *pRsp) {
        // Im really paranoid about letting it run forever!
	int max_tries = 600000;
	u16 cmd;

	OUT4500(ai, PARAM0, pCmd->parm0);
	OUT4500(ai, PARAM1, pCmd->parm1);
	OUT4500(ai, PARAM2, pCmd->parm2);
	OUT4500(ai, COMMAND, pCmd->cmd);
	while ( max_tries-- && (IN4500(ai, EVSTAT) & EV_CMD) == 0 &&
		(cmd = IN4500(ai, COMMAND)) != 0 )
			if (cmd == pCmd->cmd)
				// PC4500 didn't notice command, try again
				OUT4500(ai, COMMAND, pCmd->cmd);
	if ( max_tries == -1 ) {
		printk( KERN_ERR
			"airo: Max tries exceeded when issueing command\n" );
                return ERROR;
	}

	while (max_tries-- && (IN4500(ai, EVSTAT) & EV_CMD) == 0) {
		if (!in_interrupt() && (max_tries & 255) == 0)
			schedule();
	}
	if ( max_tries == -1 ) {
		printk( KERN_ERR
			"airo: Max tries exceeded waiting for command\n" );
                return ERROR;
	}
	// command completed
	pRsp->status = IN4500(ai, STATUS);
	pRsp->rsp0 = IN4500(ai, RESP0);
	pRsp->rsp1 = IN4500(ai, RESP1);
	pRsp->rsp2 = IN4500(ai, RESP2);

	// clear stuck command busy if necessary
	if (IN4500(ai, COMMAND) & COMMAND_BUSY) {
		OUT4500(ai, EVACK, EV_CLEARCOMMANDBUSY);
	}
	// acknowledge processing the status/response
	OUT4500(ai, EVACK, EV_CMD);

	return SUCCESS;
}

/* Sets up the bap to start exchange data.  whichbap should
 * be one of the BAP0 or BAP1 defines.  Locks should be held before
 * calling! */
static int bap_setup(struct airo_info *ai, u16 rid, u16 offset, int whichbap )
{
	int timeout = 50;
	int max_tries = 3;

	OUT4500(ai, SELECT0+whichbap, rid);
	OUT4500(ai, OFFSET0+whichbap, offset);
	while (1) {
		int status = IN4500(ai, OFFSET0+whichbap);
		if (status & BAP_BUSY) {
                        /* This isn't really a timeout, but its kinda
			   close */
			if (timeout--) {
				continue;
			}
		} else if ( status & BAP_ERR ) {
			/* invalid rid or offset */
			printk( KERN_ERR "airo: BAP error %x %d\n",
				status, whichbap );
			return ERROR;
		} else if (status & BAP_DONE) { // success
			return SUCCESS;
		}
		if ( !(max_tries--) ) {
			printk( KERN_ERR
				"airo: BAP setup error too many retries\n" );
			return ERROR;
		}
		// -- PC4500 missed it, try again
		OUT4500(ai, SELECT0+whichbap, rid);
		OUT4500(ai, OFFSET0+whichbap, offset);
		timeout = 50;
	}
}

/* should only be called by aux_bap_read.  This aux function and the
   following use concepts not documented in the developers guide.  I
   got them from a patch given to my by Aironet */
static u16 aux_setup(struct airo_info *ai, u16 page,
		     u16 offset, u16 *len)
{
	u16 next;

	OUT4500(ai, AUXPAGE, page);
	OUT4500(ai, AUXOFF, 0);
	next = IN4500(ai, AUXDATA);
	*len = IN4500(ai, AUXDATA)&0xff;
	if (offset != 4) OUT4500(ai, AUXOFF, offset);
	return next;
}

/* requires call to bap_setup() first */
static int aux_bap_read(struct airo_info *ai, u16 *pu16Dst,
			int bytelen, int whichbap)
{
	u16 len;
	u16 page;
	u16 offset;
	u16 next;
	int words;
	int i;
	unsigned long flags;

	spin_lock_irqsave(&ai->aux_lock, flags);
	page = IN4500(ai, SWS0+whichbap);
	offset = IN4500(ai, SWS2+whichbap);
	next = aux_setup(ai, page, offset, &len);
	words = (bytelen+1)>>1;

	for (i=0; i<words;) {
		int count;
		count = (len>>1) < (words-i) ? (len>>1) : (words-i);
		if ( !do8bitIO )
			insw( ai->dev->base_addr+DATA0+whichbap,
			      pu16Dst+i,count );
		else
			insb( ai->dev->base_addr+DATA0+whichbap,
			      pu16Dst+i, count << 1 );
		i += count;
		if (i<words) {
			next = aux_setup(ai, next, 4, &len);
		}
	}
	spin_unlock_irqrestore(&ai->aux_lock, flags);
	return SUCCESS;
}


/* requires call to bap_setup() first */
static int fast_bap_read(struct airo_info *ai, u16 *pu16Dst,
			 int bytelen, int whichbap)
{
	bytelen = (bytelen + 1) & (~1); // round up to even value
	if ( !do8bitIO )
		insw( ai->dev->base_addr+DATA0+whichbap, pu16Dst, bytelen>>1 );
	else
		insb( ai->dev->base_addr+DATA0+whichbap, pu16Dst, bytelen );
	return SUCCESS;
}

/* requires call to bap_setup() first */
static int bap_write(struct airo_info *ai, const u16 *pu16Src,
		     int bytelen, int whichbap)
{
	bytelen = (bytelen + 1) & (~1); // round up to even value
	if ( !do8bitIO )
		outsw( ai->dev->base_addr+DATA0+whichbap,
		       pu16Src, bytelen>>1 );
	else
		outsb( ai->dev->base_addr+DATA0+whichbap, pu16Src, bytelen );
	return SUCCESS;
}

static int PC4500_accessrid(struct airo_info *ai, u16 rid, u16 accmd)
{
	Cmd cmd; /* for issuing commands */
	Resp rsp; /* response from commands */
	u16 status;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = accmd;
	cmd.parm0 = rid;
	status = issuecommand(ai, &cmd, &rsp);
	if (status != 0) return status;
	if ( (rsp.status & 0x7F00) != 0) {
		return (accmd << 8) + (rsp.rsp0 & 0xFF);
	}
	return 0;
}

/*  Note, that we are using BAP1 which is also used by transmit, so
 *  we must get a lock. */
static int PC4500_readrid(struct airo_info *ai, u16 rid, void *pBuf, int len, int lock)
{
	u16 status;
        int rc = SUCCESS;

	if (lock) {
		if (down_interruptible(&ai->sem))
			return ERROR;
	}
	if ( (status = PC4500_accessrid(ai, rid, CMD_ACCESS)) != SUCCESS) {
                rc = status;
                goto done;
        }
	if (bap_setup(ai, rid, 0, BAP1) != SUCCESS) {
		rc = ERROR;
                goto done;
        }
	// read the rid length field
	bap_read(ai, pBuf, 2, BAP1);
	// length for remaining part of rid
	len = min(len, (int)le16_to_cpu(*(u16*)pBuf)) - 2;

	if ( len <= 2 ) {
		printk( KERN_ERR
			"airo: Rid %x has a length of %d which is too short\n",
			(int)rid,
			(int)len );
		rc = ERROR;
                goto done;
	}
	// read remainder of the rid
	rc = bap_read(ai, ((u16*)pBuf)+1, len, BAP1);
done:
	if (lock)
		up(&ai->sem);
	return rc;
}

/*  Note, that we are using BAP1 which is also used by transmit, so
 *  make sure this isnt called when a transmit is happening */
static int PC4500_writerid(struct airo_info *ai, u16 rid,
			   const void *pBuf, int len, int lock)
{
	u16 status;
	int rc = SUCCESS;

	*(u16*)pBuf = cpu_to_le16((u16)len);

	if (lock) {
		if (down_interruptible(&ai->sem))
			return ERROR;
	}
	// --- first access so that we can write the rid data
	if ( (status = PC4500_accessrid(ai, rid, CMD_ACCESS)) != 0) {
                rc = status;
                goto done;
        }
	// --- now write the rid data
	if (bap_setup(ai, rid, 0, BAP1) != SUCCESS) {
                rc = ERROR;
                goto done;
        }
	bap_write(ai, pBuf, len, BAP1);
	// ---now commit the rid data
	rc = PC4500_accessrid(ai, rid, 0x100|CMD_ACCESS);
 done:
	if (lock)
		up(&ai->sem);
        return rc;
}

/* Allocates a FID to be used for transmitting packets.  We only use
   one for now. */
static u16 transmit_allocate(struct airo_info *ai, int lenPayload, int raw)
{
	unsigned int loop = 3000;
	Cmd cmd;
	Resp rsp;
	u16 txFid;
	u16 txControl;

	cmd.cmd = CMD_ALLOCATETX;
	cmd.parm0 = lenPayload;
	if (down_interruptible(&ai->sem))
		return ERROR;
	if (issuecommand(ai, &cmd, &rsp) != SUCCESS) {
		txFid = ERROR;
		goto done;
	}
	if ( (rsp.status & 0xFF00) != 0) {
		txFid = ERROR;
		goto done;
	}
	/* wait for the allocate event/indication
	 * It makes me kind of nervous that this can just sit here and spin,
	 * but in practice it only loops like four times. */
	while (((IN4500(ai, EVSTAT) & EV_ALLOC) == 0) && --loop);
	if (!loop) {
		txFid = ERROR;
		goto done;
	}

	// get the allocated fid and acknowledge
	txFid = IN4500(ai, TXALLOCFID);
	OUT4500(ai, EVACK, EV_ALLOC);

	/*  The CARD is pretty cool since it converts the ethernet packet
	 *  into 802.11.  Also note that we don't release the FID since we
	 *  will be using the same one over and over again. */
	/*  We only have to setup the control once since we are not
	 *  releasing the fid. */
	if (raw)
		txControl = cpu_to_le16(TXCTL_TXOK | TXCTL_TXEX | TXCTL_802_11
			| TXCTL_ETHERNET | TXCTL_NORELEASE);
	else
		txControl = cpu_to_le16(TXCTL_TXOK | TXCTL_TXEX | TXCTL_802_3
			| TXCTL_ETHERNET | TXCTL_NORELEASE);
	if (bap_setup(ai, txFid, 0x0008, BAP1) != SUCCESS)
		txFid = ERROR;
	else
		bap_write(ai, &txControl, sizeof(txControl), BAP1);

done:
	up(&ai->sem);

	return txFid;
}

/* In general BAP1 is dedicated to transmiting packets.  However,
   since we need a BAP when accessing RIDs, we also use BAP1 for that.
   Make sure the BAP1 spinlock is held when this is called. */
static int transmit_802_3_packet(struct airo_info *ai, int len, char *pPacket)
{
	u16 payloadLen;
	Cmd cmd;
	Resp rsp;
	int miclen = 0;
	u16 txFid = len;
	MICBuffer pMic;

	len >>= 16;

	if (len <= ETH_ALEN * 2) {
		printk( KERN_WARNING "Short packet %d\n", len );
		return ERROR;
	}
	len -= ETH_ALEN * 2;

#ifdef MICSUPPORT
	if (test_bit(FLAG_MIC_CAPABLE, &ai->flags) && ai->micstats.enabled && 
	    (ntohs(((u16 *)pPacket)[6]) != 0x888E)) {
		if (encapsulate(ai,(etherHead *)pPacket,&pMic,len) != SUCCESS)
			return ERROR;
		miclen = sizeof(pMic);
	}
#endif

	// packet is destination[6], source[6], payload[len-12]
	// write the payload length and dst/src/payload
	if (bap_setup(ai, txFid, 0x0036, BAP1) != SUCCESS) return ERROR;
	/* The hardware addresses aren't counted as part of the payload, so
	 * we have to subtract the 12 bytes for the addresses off */
	payloadLen = cpu_to_le16(len + miclen);
	bap_write(ai, &payloadLen, sizeof(payloadLen),BAP1);
	bap_write(ai, (const u16*)pPacket, sizeof(etherHead), BAP1);
	if (miclen)
		bap_write(ai, (const u16*)&pMic, miclen, BAP1);
	bap_write(ai, (const u16*)(pPacket + sizeof(etherHead)), len, BAP1);
	// issue the transmit command
	memset( &cmd, 0, sizeof( cmd ) );
	cmd.cmd = CMD_TRANSMIT;
	cmd.parm0 = txFid;
	if (issuecommand(ai, &cmd, &rsp) != SUCCESS) return ERROR;
	if ( (rsp.status & 0xFF00) != 0) return ERROR;
	return SUCCESS;
}

static int transmit_802_11_packet(struct airo_info *ai, int len, char *pPacket)
{
	u16 fc, payloadLen;
	Cmd cmd;
	Resp rsp;
	int hdrlen;
	struct {
		u8 addr4[ETH_ALEN];
		u16 gaplen;
		u8 gap[6];
	} gap;
	u16 txFid = len;
	len >>= 16;
	gap.gaplen = 6;

	fc = le16_to_cpu(*(const u16*)pPacket);
	switch (fc & 0xc) {
		case 4:
			if ((fc & 0xe0) == 0xc0)
				hdrlen = 10;
			else
				hdrlen = 16;
			break;
		case 8:
			if ((fc&0x300)==0x300){
				hdrlen = 30;
				break;
			}
		default:
			hdrlen = 24;
	}

	if (len < hdrlen) {
		printk( KERN_WARNING "Short packet %d\n", len );
		return ERROR;
	}

	/* packet is 802.11 header +  payload
	 * write the payload length and dst/src/payload */
	if (bap_setup(ai, txFid, 6, BAP1) != SUCCESS) return ERROR;
	/* The 802.11 header aren't counted as part of the payload, so
	 * we have to subtract the header bytes off */
	payloadLen = cpu_to_le16(len-hdrlen);
	bap_write(ai, &payloadLen, sizeof(payloadLen),BAP1);
	if (bap_setup(ai, txFid, 0x0014, BAP1) != SUCCESS) return ERROR;
	bap_write(ai, (const u16*)pPacket, hdrlen, BAP1);
	bap_write(ai, hdrlen == 30 ?
		(const u16*)&gap.gaplen : (const u16*)&gap, 38 - hdrlen, BAP1);

	bap_write(ai, (const u16*)(pPacket + hdrlen), len - hdrlen, BAP1);
	// issue the transmit command
	memset( &cmd, 0, sizeof( cmd ) );
	cmd.cmd = CMD_TRANSMIT;
	cmd.parm0 = txFid;
	if (issuecommand(ai, &cmd, &rsp) != SUCCESS) return ERROR;
	if ( (rsp.status & 0xFF00) != 0) return ERROR;
	return SUCCESS;
}

/*
 *  This is the proc_fs routines.  It is a bit messier than I would
 *  like!  Feel free to clean it up!
 */

static ssize_t proc_read( struct file *file,
			  char *buffer,
			  size_t len,
			  loff_t *offset);

static ssize_t proc_write( struct file *file,
			   const char *buffer,
			   size_t len,
			   loff_t *offset );
static int proc_close( struct inode *inode, struct file *file );

static int proc_stats_open( struct inode *inode, struct file *file );
static int proc_statsdelta_open( struct inode *inode, struct file *file );
static int proc_status_open( struct inode *inode, struct file *file );
static int proc_SSID_open( struct inode *inode, struct file *file );
static int proc_APList_open( struct inode *inode, struct file *file );
static int proc_BSSList_open( struct inode *inode, struct file *file );
static int proc_config_open( struct inode *inode, struct file *file );
static int proc_wepkey_open( struct inode *inode, struct file *file );

static struct file_operations proc_statsdelta_ops = {
	.read		= proc_read,
	.open		= proc_statsdelta_open,
	.release	= proc_close
};

static struct file_operations proc_stats_ops = {
	.read		= proc_read,
	.open		= proc_stats_open,
	.release	= proc_close
};

static struct file_operations proc_status_ops = {
	.read		= proc_read,
	.open		= proc_status_open,
	.release	= proc_close
};

static struct file_operations proc_SSID_ops = {
	.read		= proc_read,
	.write		= proc_write,
	.open		= proc_SSID_open,
	.release	= proc_close
};

static struct file_operations proc_BSSList_ops = {
	.read		= proc_read,
	.write		= proc_write,
	.open		= proc_BSSList_open,
	.release	= proc_close
};

static struct file_operations proc_APList_ops = {
	.read		= proc_read,
	.write		= proc_write,
	.open		= proc_APList_open,
	.release	= proc_close
};

static struct file_operations proc_config_ops = {
	.read		= proc_read,
	.write		= proc_write,
	.open		= proc_config_open,
	.release	= proc_close
};

static struct file_operations proc_wepkey_ops = {
	.read		= proc_read,
	.write		= proc_write,
	.open		= proc_wepkey_open,
	.release	= proc_close
};

static struct proc_dir_entry *airo_entry = 0;

struct proc_data {
	int release_buffer;
	int readlen;
	char *rbuffer;
	int writelen;
	int maxwritelen;
	char *wbuffer;
	void (*on_close) (struct inode *, struct file *);
};

#ifndef SETPROC_OPS
#define SETPROC_OPS(entry, ops) (entry)->proc_fops = &(ops)
#endif

static int setup_proc_entry( struct net_device *dev,
			     struct airo_info *apriv ) {
	struct proc_dir_entry *entry;
	/* First setup the device directory */
	apriv->proc_entry = create_proc_entry(dev->name,
					      S_IFDIR|airo_perm,
					      airo_entry);
        apriv->proc_entry->uid = proc_uid;
        apriv->proc_entry->gid = proc_gid;
	apriv->proc_entry->owner = THIS_MODULE;

	/* Setup the StatsDelta */
	entry = create_proc_entry("StatsDelta",
				  S_IFREG | (S_IRUGO&proc_perm),
				  apriv->proc_entry);
        entry->uid = proc_uid;
        entry->gid = proc_gid;
	entry->data = dev;
	entry->owner = THIS_MODULE;
	SETPROC_OPS(entry, proc_statsdelta_ops);

	/* Setup the Stats */
	entry = create_proc_entry("Stats",
				  S_IFREG | (S_IRUGO&proc_perm),
				  apriv->proc_entry);
        entry->uid = proc_uid;
        entry->gid = proc_gid;
	entry->data = dev;
	entry->owner = THIS_MODULE;
	SETPROC_OPS(entry, proc_stats_ops);

	/* Setup the Status */
	entry = create_proc_entry("Status",
				  S_IFREG | (S_IRUGO&proc_perm),
				  apriv->proc_entry);
        entry->uid = proc_uid;
        entry->gid = proc_gid;
	entry->data = dev;
	entry->owner = THIS_MODULE;
	SETPROC_OPS(entry, proc_status_ops);

	/* Setup the Config */
	entry = create_proc_entry("Config",
				  S_IFREG | proc_perm,
				  apriv->proc_entry);
        entry->uid = proc_uid;
        entry->gid = proc_gid;
	entry->data = dev;
	entry->owner = THIS_MODULE;
	SETPROC_OPS(entry, proc_config_ops);

	/* Setup the SSID */
	entry = create_proc_entry("SSID",
				  S_IFREG | proc_perm,
				  apriv->proc_entry);
        entry->uid = proc_uid;
        entry->gid = proc_gid;
	entry->data = dev;
	entry->owner = THIS_MODULE;
	SETPROC_OPS(entry, proc_SSID_ops);

	/* Setup the APList */
	entry = create_proc_entry("APList",
				  S_IFREG | proc_perm,
				  apriv->proc_entry);
        entry->uid = proc_uid;
        entry->gid = proc_gid;
	entry->data = dev;
	entry->owner = THIS_MODULE;
	SETPROC_OPS(entry, proc_APList_ops);

	/* Setup the BSSList */
	entry = create_proc_entry("BSSList",
				  S_IFREG | proc_perm,
				  apriv->proc_entry);
	entry->uid = proc_uid;
	entry->gid = proc_gid;
	entry->data = dev;
	entry->owner = THIS_MODULE;
	SETPROC_OPS(entry, proc_BSSList_ops);

	/* Setup the WepKey */
	entry = create_proc_entry("WepKey",
				  S_IFREG | proc_perm,
				  apriv->proc_entry);
        entry->uid = proc_uid;
        entry->gid = proc_gid;
	entry->data = dev;
	entry->owner = THIS_MODULE;
	SETPROC_OPS(entry, proc_wepkey_ops);

	return 0;
}

static int takedown_proc_entry( struct net_device *dev,
				struct airo_info *apriv ) {
	if ( !apriv->proc_entry->namelen ) return 0;
	remove_proc_entry("Stats",apriv->proc_entry);
	remove_proc_entry("StatsDelta",apriv->proc_entry);
	remove_proc_entry("Status",apriv->proc_entry);
	remove_proc_entry("Config",apriv->proc_entry);
	remove_proc_entry("SSID",apriv->proc_entry);
	remove_proc_entry("APList",apriv->proc_entry);
	remove_proc_entry("BSSList",apriv->proc_entry);
	remove_proc_entry("WepKey",apriv->proc_entry);
	remove_proc_entry(dev->name,airo_entry);
	return 0;
}

/*
 *  What we want from the proc_fs is to be able to efficiently read
 *  and write the configuration.  To do this, we want to read the
 *  configuration when the file is opened and write it when the file is
 *  closed.  So basically we allocate a read buffer at open and fill it
 *  with data, and allocate a write buffer and read it at close.
 */

/*
 *  The read routine is generic, it relies on the preallocated rbuffer
 *  to supply the data.
 */
static ssize_t proc_read( struct file *file,
			  char *buffer,
			  size_t len,
			  loff_t *offset )
{
	int i;
	int pos;
	struct proc_data *priv = (struct proc_data*)file->private_data;

	if( !priv->rbuffer ) return -EINVAL;

	pos = *offset;
	for( i = 0; i+pos < priv->readlen && i < len; i++ ) {
		if (put_user( priv->rbuffer[i+pos], buffer+i ))
			return -EFAULT;
	}
	*offset += i;
	return i;
}

/*
 *  The write routine is generic, it fills in a preallocated rbuffer
 *  to supply the data.
 */
static ssize_t proc_write( struct file *file,
			   const char *buffer,
			   size_t len,
			   loff_t *offset )
{
	int i;
	int pos;
	struct proc_data *priv = (struct proc_data*)file->private_data;

	if ( !priv->wbuffer ) {
		return -EINVAL;
	}

	pos = *offset;

	for( i = 0; i + pos <  priv->maxwritelen &&
		     i < len; i++ ) {
		if (get_user( priv->wbuffer[i+pos], buffer + i ))
			return -EFAULT;
	}
	if ( i+pos > priv->writelen ) priv->writelen = i+file->f_pos;
	*offset += i;
	return i;
}

static int proc_status_open( struct inode *inode, struct file *file ) {
	struct proc_data *data;
	struct proc_dir_entry *dp = PDE(inode);
	struct net_device *dev = dp->data;
	struct airo_info *apriv = dev->priv;
	CapabilityRid cap_rid;
	StatusRid status_rid;
	int i;

	if ((file->private_data = kmalloc(sizeof(struct proc_data ), GFP_KERNEL)) == NULL)
		return -ENOMEM;
	memset(file->private_data, 0, sizeof(struct proc_data));
	data = (struct proc_data *)file->private_data;
	if ((data->rbuffer = kmalloc( 2048, GFP_KERNEL )) == NULL) {
		kfree (file->private_data);
		return -ENOMEM;
	}

	readStatusRid(apriv, &status_rid, 1);
	readCapabilityRid(apriv, &cap_rid);

        i = sprintf(data->rbuffer, "Status: %s%s%s%s%s%s%s%s%s\n",
                    status_rid.mode & 1 ? "CFG ": "",
                    status_rid.mode & 2 ? "ACT ": "",
                    status_rid.mode & 0x10 ? "SYN ": "",
                    status_rid.mode & 0x20 ? "LNK ": "",
                    status_rid.mode & 0x40 ? "LEAP ": "",
                    status_rid.mode & 0x80 ? "PRIV ": "",
                    status_rid.mode & 0x100 ? "KEY ": "",
                    status_rid.mode & 0x200 ? "WEP ": "",
                    status_rid.mode & 0x8000 ? "ERR ": "");
	sprintf( data->rbuffer+i, "Mode: %x\n"
		 "Signal Strength: %d\n"
		 "Signal Quality: %d\n"
		 "SSID: %-.*s\n"
		 "AP: %-.16s\n"
		 "Freq: %d\n"
		 "BitRate: %dmbs\n"
		 "Driver Version: %s\n"
		 "Device: %s\nManufacturer: %s\nFirmware Version: %s\n"
		 "Radio type: %x\nCountry: %x\nHardware Version: %x\n"
		 "Software Version: %x\nSoftware Subversion: %x\n"
		 "Boot block version: %x\n",
		 (int)status_rid.mode,
		 (int)status_rid.normalizedSignalStrength,
		 (int)status_rid.signalQuality,
		 (int)status_rid.SSIDlen,
		 status_rid.SSID,
		 status_rid.apName,
		 (int)status_rid.channel,
		 (int)status_rid.currentXmitRate/2,
		 version,
		 cap_rid.prodName,
		 cap_rid.manName,
		 cap_rid.prodVer,
		 cap_rid.radioType,
		 cap_rid.country,
		 cap_rid.hardVer,
		 (int)cap_rid.softVer,
		 (int)cap_rid.softSubVer,
		 (int)cap_rid.bootBlockVer );
	data->readlen = strlen( data->rbuffer );
	return 0;
}

static int proc_stats_rid_open(struct inode*, struct file*, u16);
static int proc_statsdelta_open( struct inode *inode,
				 struct file *file ) {
	if (file->f_mode&FMODE_WRITE) {
		return proc_stats_rid_open(inode, file, RID_STATSDELTACLEAR);
	}
	return proc_stats_rid_open(inode, file, RID_STATSDELTA);
}

static int proc_stats_open( struct inode *inode, struct file *file ) {
	return proc_stats_rid_open(inode, file, RID_STATS);
}

static int proc_stats_rid_open( struct inode *inode,
				struct file *file,
				u16 rid ) {
	struct proc_data *data;
	struct proc_dir_entry *dp = PDE(inode);
	struct net_device *dev = dp->data;
	struct airo_info *apriv = dev->priv;
	StatsRid stats;
	int i, j;
	int *vals = stats.vals;

	if ((file->private_data = kmalloc(sizeof(struct proc_data ), GFP_KERNEL)) == NULL)
		return -ENOMEM;
	memset(file->private_data, 0, sizeof(struct proc_data));
	data = (struct proc_data *)file->private_data;
	if ((data->rbuffer = kmalloc( 4096, GFP_KERNEL )) == NULL) {
		kfree (file->private_data);
		return -ENOMEM;
	}

	readStatsRid(apriv, &stats, rid, 1);

        j = 0;
	for(i=0; statsLabels[i]!=(char *)-1 &&
		    i*4<stats.len; i++){
		if (!statsLabels[i]) continue;
		if (j+strlen(statsLabels[i])+16>4096) {
			printk(KERN_WARNING
			       "airo: Potentially disasterous buffer overflow averted!\n");
			break;
		}
		j+=sprintf(data->rbuffer+j, "%s: %d\n", statsLabels[i], vals[i]);
	}
	if (i*4>=stats.len){
		printk(KERN_WARNING
		       "airo: Got a short rid\n");
	}
	data->readlen = j;
	return 0;
}

static int get_dec_u16( char *buffer, int *start, int limit ) {
	u16 value;
	int valid = 0;
	for( value = 0; buffer[*start] >= '0' &&
		     buffer[*start] <= '9' &&
		     *start < limit; (*start)++ ) {
		valid = 1;
		value *= 10;
		value += buffer[*start] - '0';
	}
	if ( !valid ) return -1;
	return value;
}

static int airo_config_commit(struct net_device *dev,
			      struct iw_request_info *info, void *zwrq,
			      char *extra);

static void proc_config_on_close( struct inode *inode, struct file *file ) {
	struct proc_data *data = file->private_data;
	struct proc_dir_entry *dp = PDE(inode);
	struct net_device *dev = dp->data;
	struct airo_info *ai = dev->priv;
	char *line;

	if ( !data->writelen ) return;

	readConfigRid(ai, 1);
	ai->need_commit = 1;

	line = data->wbuffer;
	while( line[0] ) {
/*** Mode processing */
		if ( !strncmp( line, "Mode: ", 6 ) ) {
			line += 6;
			if ((ai->config.rmode & 0xff) >= RXMODE_RFMON)
					ai->need_commit = 2;
			ai->config.rmode &= 0xfe00;
			clear_bit (FLAG_802_11, &ai->flags);
			ai->config.opmode &= 0xFF00;
			ai->config.scanMode = SCANMODE_ACTIVE;
			if ( line[0] == 'a' ) {
				ai->config.opmode |= 0;
			} else {
				ai->config.opmode |= 1;
				if ( line[0] == 'r' ) {
					ai->config.rmode |= RXMODE_RFMON | RXMODE_DISABLE_802_3_HEADER;
					ai->config.scanMode = SCANMODE_PASSIVE;
					set_bit (FLAG_802_11, &ai->flags);
				} else if ( line[0] == 'y' ) {
					ai->config.rmode |= RXMODE_RFMON_ANYBSS | RXMODE_DISABLE_802_3_HEADER;
					ai->config.scanMode = SCANMODE_PASSIVE;
					set_bit (FLAG_802_11, &ai->flags);
				} else if ( line[0] == 'l' )
					ai->config.rmode |= RXMODE_LANMON;
			}
			ai->need_commit = 1;
		}

/*** Radio status */
		else if (!strncmp(line,"Radio: ", 7)) {
			line += 7;
			if (!strncmp(line,"off",3)) {
				set_bit (FLAG_RADIO_OFF, &ai->flags);
			} else {
				clear_bit (FLAG_RADIO_OFF, &ai->flags);
			}
		}
/*** NodeName processing */
		else if ( !strncmp( line, "NodeName: ", 10 ) ) {
			int j;

			line += 10;
			memset( ai->config.nodeName, 0, 16 );
/* Do the name, assume a space between the mode and node name */
			for( j = 0; j < 16 && line[j] != '\n'; j++ ) {
				ai->config.nodeName[j] = line[j];
			}
			ai->need_commit = 1;
		}

/*** PowerMode processing */
		else if ( !strncmp( line, "PowerMode: ", 11 ) ) {
			line += 11;
			if ( !strncmp( line, "PSPCAM", 6 ) ) {
				ai->config.powerSaveMode = POWERSAVE_PSPCAM;
				ai->need_commit = 1;
			} else if ( !strncmp( line, "PSP", 3 ) ) {
				ai->config.powerSaveMode = POWERSAVE_PSP;
				ai->need_commit = 1;
			} else {
				ai->config.powerSaveMode = POWERSAVE_CAM;
				ai->need_commit = 1;
			}
		} else if ( !strncmp( line, "DataRates: ", 11 ) ) {
			int v, i = 0, k = 0; /* i is index into line,
						k is index to rates */

			line += 11;
			while((v = get_dec_u16(line, &i, 3))!=-1) {
				ai->config.rates[k++] = (u8)v;
				line += i + 1;
				i = 0;
			}
			ai->need_commit = 1;
		} else if ( !strncmp( line, "Channel: ", 9 ) ) {
			int v, i = 0;
			line += 9;
			v = get_dec_u16(line, &i, i+3);
			if ( v != -1 ) {
				ai->config.channelSet = (u16)v;
				ai->need_commit = 1;
			}
		} else if ( !strncmp( line, "XmitPower: ", 11 ) ) {
			int v, i = 0;
			line += 11;
			v = get_dec_u16(line, &i, i+3);
			if ( v != -1 ) {
				ai->config.txPower = (u16)v;
				ai->need_commit = 1;
			}
		} else if ( !strncmp( line, "WEP: ", 5 ) ) {
			line += 5;
			switch( line[0] ) {
			case 's':
				ai->config.authType = (u16)AUTH_SHAREDKEY;
				break;
			case 'e':
				ai->config.authType = (u16)AUTH_ENCRYPT;
				break;
			default:
				ai->config.authType = (u16)AUTH_OPEN;
				break;
			}
			ai->need_commit = 1;
		} else if ( !strncmp( line, "LongRetryLimit: ", 16 ) ) {
			int v, i = 0;

			line += 16;
			v = get_dec_u16(line, &i, 3);
			v = (v<0) ? 0 : ((v>255) ? 255 : v);
			ai->config.longRetryLimit = (u16)v;
			ai->need_commit = 1;
		} else if ( !strncmp( line, "ShortRetryLimit: ", 17 ) ) {
			int v, i = 0;

			line += 17;
			v = get_dec_u16(line, &i, 3);
			v = (v<0) ? 0 : ((v>255) ? 255 : v);
			ai->config.shortRetryLimit = (u16)v;
			ai->need_commit = 1;
		} else if ( !strncmp( line, "RTSThreshold: ", 14 ) ) {
			int v, i = 0;

			line += 14;
			v = get_dec_u16(line, &i, 4);
			v = (v<0) ? 0 : ((v>2312) ? 2312 : v);
			ai->config.rtsThres = (u16)v;
			ai->need_commit = 1;
		} else if ( !strncmp( line, "TXMSDULifetime: ", 16 ) ) {
			int v, i = 0;

			line += 16;
			v = get_dec_u16(line, &i, 5);
			v = (v<0) ? 0 : v;
			ai->config.txLifetime = (u16)v;
			ai->need_commit = 1;
		} else if ( !strncmp( line, "RXMSDULifetime: ", 16 ) ) {
			int v, i = 0;

			line += 16;
			v = get_dec_u16(line, &i, 5);
			v = (v<0) ? 0 : v;
			ai->config.rxLifetime = (u16)v;
			ai->need_commit = 1;
		} else if ( !strncmp( line, "TXDiversity: ", 13 ) ) {
			ai->config.txDiversity =
				(line[13]=='l') ? 1 :
				((line[13]=='r')? 2: 3);
			ai->need_commit = 1;
		} else if ( !strncmp( line, "RXDiversity: ", 13 ) ) {
			ai->config.rxDiversity =
				(line[13]=='l') ? 1 :
				((line[13]=='r')? 2: 3);
			ai->need_commit = 1;
		} else if ( !strncmp( line, "FragThreshold: ", 15 ) ) {
			int v, i = 0;

			line += 15;
			v = get_dec_u16(line, &i, 4);
			v = (v<256) ? 256 : ((v>2312) ? 2312 : v);
			v = v & 0xfffe; /* Make sure its even */
			ai->config.fragThresh = (u16)v;
			ai->need_commit = 1;
		} else if (!strncmp(line, "Modulation: ", 12)) {
			line += 12;
			switch(*line) {
			case 'd':  ai->config.modulation=MOD_DEFAULT; ai->need_commit=1; break;
			case 'c':  ai->config.modulation=MOD_CCK; ai->need_commit=1; break;
			case 'm':  ai->config.modulation=MOD_MOK; ai->need_commit=1; break;
			default:
				printk( KERN_WARNING "airo: Unknown modulation\n" );
			}
		} else if (!strncmp(line, "Preamble: ", 10)) {
			line += 10;
			switch(*line) {
			case 'a': ai->config.preamble=PREAMBLE_AUTO; ai->need_commit=1; break;
			case 'l': ai->config.preamble=PREAMBLE_LONG; ai->need_commit=1; break;
			case 's': ai->config.preamble=PREAMBLE_SHORT; ai->need_commit=1; break;
		        default: printk(KERN_WARNING "airo: Unknown preamble\n");
			}
		} else {
			printk( KERN_WARNING "Couldn't figure out %s\n", line );
		}
		while( line[0] && line[0] != '\n' ) line++;
		if ( line[0] ) line++;
	}
	airo_config_commit(dev, NULL, NULL, NULL);
}

static char *get_rmode(u16 mode) {
        switch(mode&0xff) {
        case RXMODE_RFMON:  return "rfmon";
        case RXMODE_RFMON_ANYBSS:  return "yna (any) bss rfmon";
        case RXMODE_LANMON:  return "lanmon";
        }
        return "ESS";
}

static int proc_config_open( struct inode *inode, struct file *file ) {
	struct proc_data *data;
	struct proc_dir_entry *dp = PDE(inode);
	struct net_device *dev = dp->data;
	struct airo_info *ai = dev->priv;
	int i;

	if ((file->private_data = kmalloc(sizeof(struct proc_data ), GFP_KERNEL)) == NULL)
		return -ENOMEM;
	memset(file->private_data, 0, sizeof(struct proc_data));
	data = (struct proc_data *)file->private_data;
	if ((data->rbuffer = kmalloc( 2048, GFP_KERNEL )) == NULL) {
		kfree (file->private_data);
		return -ENOMEM;
	}
	if ((data->wbuffer = kmalloc( 2048, GFP_KERNEL )) == NULL) {
		kfree (data->rbuffer);
		kfree (file->private_data);
		return -ENOMEM;
	}
	memset( data->wbuffer, 0, 2048 );
	data->maxwritelen = 2048;
	data->on_close = proc_config_on_close;

	readConfigRid(ai, 1);

	i = sprintf( data->rbuffer,
		     "Mode: %s\n"
		     "Radio: %s\n"
		     "NodeName: %-16s\n"
		     "PowerMode: %s\n"
		     "DataRates: %d %d %d %d %d %d %d %d\n"
		     "Channel: %d\n"
		     "XmitPower: %d\n",
		     (ai->config.opmode & 0xFF) == 0 ? "adhoc" :
		     (ai->config.opmode & 0xFF) == 1 ? get_rmode(ai->config.rmode):
		     (ai->config.opmode & 0xFF) == 2 ? "AP" :
		     (ai->config.opmode & 0xFF) == 3 ? "AP RPTR" : "Error",
		     test_bit(FLAG_RADIO_OFF, &ai->flags) ? "off" : "on",
		     ai->config.nodeName,
		     ai->config.powerSaveMode == 0 ? "CAM" :
		     ai->config.powerSaveMode == 1 ? "PSP" :
		     ai->config.powerSaveMode == 2 ? "PSPCAM" : "Error",
		     (int)ai->config.rates[0],
		     (int)ai->config.rates[1],
		     (int)ai->config.rates[2],
		     (int)ai->config.rates[3],
		     (int)ai->config.rates[4],
		     (int)ai->config.rates[5],
		     (int)ai->config.rates[6],
		     (int)ai->config.rates[7],
		     (int)ai->config.channelSet,
		     (int)ai->config.txPower
		);
	sprintf( data->rbuffer + i,
		 "LongRetryLimit: %d\n"
		 "ShortRetryLimit: %d\n"
		 "RTSThreshold: %d\n"
		 "TXMSDULifetime: %d\n"
		 "RXMSDULifetime: %d\n"
		 "TXDiversity: %s\n"
		 "RXDiversity: %s\n"
		 "FragThreshold: %d\n"
		 "WEP: %s\n"
		 "Modulation: %s\n"
		 "Preamble: %s\n",
		 (int)ai->config.longRetryLimit,
		 (int)ai->config.shortRetryLimit,
		 (int)ai->config.rtsThres,
		 (int)ai->config.txLifetime,
		 (int)ai->config.rxLifetime,
		 ai->config.txDiversity == 1 ? "left" :
		 ai->config.txDiversity == 2 ? "right" : "both",
		 ai->config.rxDiversity == 1 ? "left" :
		 ai->config.rxDiversity == 2 ? "right" : "both",
		 (int)ai->config.fragThresh,
		 ai->config.authType == AUTH_ENCRYPT ? "encrypt" :
		 ai->config.authType == AUTH_SHAREDKEY ? "shared" : "open",
		 ai->config.modulation == 0 ? "default" :
		 ai->config.modulation == MOD_CCK ? "cck" :
		 ai->config.modulation == MOD_MOK ? "mok" : "error",
		 ai->config.preamble == PREAMBLE_AUTO ? "auto" :
		 ai->config.preamble == PREAMBLE_LONG ? "long" :
		 ai->config.preamble == PREAMBLE_SHORT ? "short" : "error"
		);
	data->readlen = strlen( data->rbuffer );
	return 0;
}

static void proc_SSID_on_close( struct inode *inode, struct file *file ) {
	struct proc_data *data = (struct proc_data *)file->private_data;
	struct proc_dir_entry *dp = PDE(inode);
	struct net_device *dev = dp->data;
	struct airo_info *ai = dev->priv;
	SsidRid SSID_rid;
	Resp rsp;
	int i;
	int offset = 0;

	if ( !data->writelen ) return;

	memset( &SSID_rid, 0, sizeof( SSID_rid ) );

	for( i = 0; i < 3; i++ ) {
		int j;
		for( j = 0; j+offset < data->writelen && j < 32 &&
			     data->wbuffer[offset+j] != '\n'; j++ ) {
			SSID_rid.ssids[i].ssid[j] = data->wbuffer[offset+j];
		}
		if ( j == 0 ) break;
		SSID_rid.ssids[i].len = j;
		offset += j;
		while( data->wbuffer[offset] != '\n' &&
		       offset < data->writelen ) offset++;
		offset++;
	}
	if (i)
		SSID_rid.len = sizeof(SSID_rid);
	disable_MAC(ai, 1);
	writeSsidRid(ai, &SSID_rid);
	enable_MAC(ai, &rsp, 1);
}

inline static u8 hexVal(char c) {
	if (c>='0' && c<='9') return c -= '0';
	if (c>='a' && c<='f') return c -= 'a'-10;
	if (c>='A' && c<='F') return c -= 'A'-10;
	return 0;
}

static void proc_APList_on_close( struct inode *inode, struct file *file ) {
	struct proc_data *data = (struct proc_data *)file->private_data;
	struct proc_dir_entry *dp = PDE(inode);
	struct net_device *dev = dp->data;
	struct airo_info *ai = dev->priv;
	APListRid APList_rid;
	Resp rsp;
	int i;

	if ( !data->writelen ) return;

	memset( &APList_rid, 0, sizeof(APList_rid) );
	APList_rid.len = sizeof(APList_rid);

	for( i = 0; i < 4 && data->writelen >= (i+1)*6*3; i++ ) {
		int j;
		for( j = 0; j < 6*3 && data->wbuffer[j+i*6*3]; j++ ) {
			switch(j%3) {
			case 0:
				APList_rid.ap[i][j/3]=
					hexVal(data->wbuffer[j+i*6*3])<<4;
				break;
			case 1:
				APList_rid.ap[i][j/3]|=
					hexVal(data->wbuffer[j+i*6*3]);
				break;
			}
		}
	}
	disable_MAC(ai, 1);
	writeAPListRid(ai, &APList_rid);
	enable_MAC(ai, &rsp, 1);
}

/* This function wraps PC4500_writerid with a MAC disable */
static int do_writerid( struct airo_info *ai, u16 rid, const void *rid_data,
			int len, int dummy ) {
	int rc;
	Resp rsp;

	disable_MAC(ai, 1);
	rc = PC4500_writerid(ai, rid, rid_data, len, 1);
	enable_MAC(ai, &rsp, 1);
	return rc;
}

/* Returns the length of the key at the index.  If index == 0xffff
 * the index of the transmit key is returned.  If the key doesn't exist,
 * -1 will be returned.
 */
static int get_wep_key(struct airo_info *ai, u16 index) {
	WepKeyRid wkr;
	int rc;
	u16 lastindex;

	rc = readWepKeyRid(ai, &wkr, 1);
	if (rc == SUCCESS) do {
		lastindex = wkr.kindex;
		if (wkr.kindex == index) {
			if (index == 0xffff) {
				return wkr.mac[0];
			}
			return wkr.klen;
		}
		readWepKeyRid(ai, &wkr, 0);
	} while(lastindex != wkr.kindex);
	return -1;
}

static int set_wep_key(struct airo_info *ai, u16 index,
		       const char *key, u16 keylen, int perm, int lock ) {
	static const unsigned char macaddr[ETH_ALEN] = { 0x01, 0, 0, 0, 0, 0 };
	WepKeyRid wkr;

	memset(&wkr, 0, sizeof(wkr));
	if (keylen == 0) {
// We are selecting which key to use
		wkr.len = sizeof(wkr);
		wkr.kindex = 0xffff;
		wkr.mac[0] = (char)index;
		if (perm) printk(KERN_INFO "Setting transmit key to %d\n", index);
		if (perm) ai->defindex = (char)index;
	} else {
// We are actually setting the key
		wkr.len = sizeof(wkr);
		wkr.kindex = index;
		wkr.klen = keylen;
		memcpy( wkr.key, key, keylen );
		memcpy( wkr.mac, macaddr, ETH_ALEN );
		printk(KERN_INFO "Setting key %d\n", index);
	}

	writeWepKeyRid(ai, &wkr, perm, lock);
	return 0;
}

static void proc_wepkey_on_close( struct inode *inode, struct file *file ) {
	struct proc_data *data;
	struct proc_dir_entry *dp = PDE(inode);
	struct net_device *dev = dp->data;
	struct airo_info *ai = dev->priv;
	int i;
	char key[16];
	u16 index = 0;
	int j = 0;

	memset(key, 0, sizeof(key));

	data = (struct proc_data *)file->private_data;
	if ( !data->writelen ) return;

	if (data->wbuffer[0] >= '0' && data->wbuffer[0] <= '3' &&
	    (data->wbuffer[1] == ' ' || data->wbuffer[1] == '\n')) {
		index = data->wbuffer[0] - '0';
		if (data->wbuffer[1] == '\n') {
			set_wep_key(ai, index, 0, 0, 1, 1);
			return;
		}
		j = 2;
	} else {
		printk(KERN_ERR "airo:  WepKey passed invalid key index\n");
		return;
	}

	for( i = 0; i < 16*3 && data->wbuffer[i+j]; i++ ) {
		switch(i%3) {
		case 0:
			key[i/3] = hexVal(data->wbuffer[i+j])<<4;
			break;
		case 1:
			key[i/3] |= hexVal(data->wbuffer[i+j]);
			break;
		}
	}
	set_wep_key(ai, index, key, i/3, 1, 1);
}

static int proc_wepkey_open( struct inode *inode, struct file *file ) {
	struct proc_data *data;
	struct proc_dir_entry *dp = PDE(inode);
	struct net_device *dev = dp->data;
	struct airo_info *ai = dev->priv;
	char *ptr;
	WepKeyRid wkr;
	u16 lastindex;
	int j=0;
	int rc;

	if ((file->private_data = kmalloc(sizeof(struct proc_data ), GFP_KERNEL)) == NULL)
		return -ENOMEM;
	memset(file->private_data, 0, sizeof(struct proc_data));
	memset(&wkr, 0, sizeof(wkr));
	data = (struct proc_data *)file->private_data;
	if ((data->rbuffer = kmalloc( 180, GFP_KERNEL )) == NULL) {
		kfree (file->private_data);
		return -ENOMEM;
	}
	memset(data->rbuffer, 0, 180);
	data->writelen = 0;
	data->maxwritelen = 80;
	if ((data->wbuffer = kmalloc( 80, GFP_KERNEL )) == NULL) {
		kfree (data->rbuffer);
		kfree (file->private_data);
		return -ENOMEM;
	}
	memset( data->wbuffer, 0, 80 );
	data->on_close = proc_wepkey_on_close;

	ptr = data->rbuffer;
	strcpy(ptr, "No wep keys\n");
	rc = readWepKeyRid(ai, &wkr, 1);
	if (rc == SUCCESS) do {
		lastindex = wkr.kindex;
		if (wkr.kindex == 0xffff) {
			j += sprintf(ptr+j, "Tx key = %d\n",
				     (int)wkr.mac[0]);
		} else {
			j += sprintf(ptr+j, "Key %d set with length = %d\n",
				     (int)wkr.kindex, (int)wkr.klen);
		}
		readWepKeyRid(ai, &wkr, 0);
	} while((lastindex != wkr.kindex) && (j < 180-30));

	data->readlen = strlen( data->rbuffer );
	return 0;
}

static int proc_SSID_open( struct inode *inode, struct file *file ) {
	struct proc_data *data;
	struct proc_dir_entry *dp = PDE(inode);
	struct net_device *dev = dp->data;
	struct airo_info *ai = dev->priv;
	int i;
	char *ptr;
	SsidRid SSID_rid;

	if ((file->private_data = kmalloc(sizeof(struct proc_data ), GFP_KERNEL)) == NULL)
		return -ENOMEM;
	memset(file->private_data, 0, sizeof(struct proc_data));
	data = (struct proc_data *)file->private_data;
	if ((data->rbuffer = kmalloc( 104, GFP_KERNEL )) == NULL) {
		kfree (file->private_data);
		return -ENOMEM;
	}
	data->writelen = 0;
	data->maxwritelen = 33*3;
	if ((data->wbuffer = kmalloc( 33*3, GFP_KERNEL )) == NULL) {
		kfree (data->rbuffer);
		kfree (file->private_data);
		return -ENOMEM;
	}
	memset( data->wbuffer, 0, 33*3 );
	data->on_close = proc_SSID_on_close;

	readSsidRid(ai, &SSID_rid);
	ptr = data->rbuffer;
	for( i = 0; i < 3; i++ ) {
		int j;
		if ( !SSID_rid.ssids[i].len ) break;
		for( j = 0; j < 32 &&
			     j < SSID_rid.ssids[i].len &&
			     SSID_rid.ssids[i].ssid[j]; j++ ) {
			*ptr++ = SSID_rid.ssids[i].ssid[j];
		}
		*ptr++ = '\n';
	}
	*ptr = '\0';
	data->readlen = strlen( data->rbuffer );
	return 0;
}

static int proc_APList_open( struct inode *inode, struct file *file ) {
	struct proc_data *data;
	struct proc_dir_entry *dp = PDE(inode);
	struct net_device *dev = dp->data;
	struct airo_info *ai = dev->priv;
	int i;
	char *ptr;
	APListRid APList_rid;

	if ((file->private_data = kmalloc(sizeof(struct proc_data ), GFP_KERNEL)) == NULL)
		return -ENOMEM;
	memset(file->private_data, 0, sizeof(struct proc_data));
	data = (struct proc_data *)file->private_data;
	if ((data->rbuffer = kmalloc( 104, GFP_KERNEL )) == NULL) {
		kfree (file->private_data);
		return -ENOMEM;
	}
	data->writelen = 0;
	data->maxwritelen = 4*6*3;
	if ((data->wbuffer = kmalloc( data->maxwritelen, GFP_KERNEL )) == NULL) {
		kfree (data->rbuffer);
		kfree (file->private_data);
		return -ENOMEM;
	}
	memset( data->wbuffer, 0, data->maxwritelen );
	data->on_close = proc_APList_on_close;

	readAPListRid(ai, &APList_rid);
	ptr = data->rbuffer;
	for( i = 0; i < 4; i++ ) {
// We end when we find a zero MAC
		if ( !*(int*)APList_rid.ap[i] &&
		     !*(int*)&APList_rid.ap[i][2]) break;
		ptr += sprintf(ptr, "%02x:%02x:%02x:%02x:%02x:%02x\n",
			       (int)APList_rid.ap[i][0],
			       (int)APList_rid.ap[i][1],
			       (int)APList_rid.ap[i][2],
			       (int)APList_rid.ap[i][3],
			       (int)APList_rid.ap[i][4],
			       (int)APList_rid.ap[i][5]);
	}
	if (i==0) ptr += sprintf(ptr, "Not using specific APs\n");

	*ptr = '\0';
	data->readlen = strlen( data->rbuffer );
	return 0;
}

static int proc_BSSList_open( struct inode *inode, struct file *file ) {
	struct proc_data *data;
	struct proc_dir_entry *dp = PDE(inode);
	struct net_device *dev = dp->data;
	struct airo_info *ai = dev->priv;
	char *ptr;
	BSSListRid BSSList_rid;
	int rc;
	/* If doLoseSync is not 1, we won't do a Lose Sync */
	int doLoseSync = -1;

	if ((file->private_data = kmalloc(sizeof(struct proc_data ), GFP_KERNEL)) == NULL)
		return -ENOMEM;
	memset(file->private_data, 0, sizeof(struct proc_data));
	data = (struct proc_data *)file->private_data;
	if ((data->rbuffer = kmalloc( 1024, GFP_KERNEL )) == NULL) {
		kfree (file->private_data);
		return -ENOMEM;
	}
	data->writelen = 0;
	data->maxwritelen = 0;
	data->wbuffer = 0;
	data->on_close = 0;

	if (file->f_mode & FMODE_WRITE) {
		if (!(file->f_mode & FMODE_READ)) {
			Cmd cmd;
			Resp rsp;

			memset(&cmd, 0, sizeof(cmd));
			cmd.cmd=CMD_LISTBSS;
			if (down_interruptible(&ai->sem))
				return -ERESTARTSYS;
			issuecommand(ai, &cmd, &rsp);
			up(&ai->sem);
			data->readlen = 0;
			return 0;
		}
		doLoseSync = 1;
	}
	ptr = data->rbuffer;
	/* There is a race condition here if there are concurrent opens.
           Since it is a rare condition, we'll just live with it, otherwise
           we have to add a spin lock... */
	rc = readBSSListRid(ai, doLoseSync, &BSSList_rid);
	while(rc == 0 && BSSList_rid.index != 0xffff) {
		ptr += sprintf(ptr, "%02x:%02x:%02x:%02x:%02x:%02x %*s rssi = %d",
				(int)BSSList_rid.bssid[0],
				(int)BSSList_rid.bssid[1],
				(int)BSSList_rid.bssid[2],
				(int)BSSList_rid.bssid[3],
				(int)BSSList_rid.bssid[4],
				(int)BSSList_rid.bssid[5],
				(int)BSSList_rid.ssidLen,
				BSSList_rid.ssid,
				(int)BSSList_rid.rssi);
		ptr += sprintf(ptr, " channel = %d %s %s %s %s\n",
				(int)BSSList_rid.dsChannel,
				BSSList_rid.cap & CAP_ESS ? "ESS" : "",
				BSSList_rid.cap & CAP_IBSS ? "adhoc" : "",
				BSSList_rid.cap & CAP_PRIVACY ? "wep" : "",
				BSSList_rid.cap & CAP_SHORTHDR ? "shorthdr" : "");
		rc = readBSSListRid(ai, 0, &BSSList_rid);
	}
	*ptr = '\0';
	data->readlen = strlen( data->rbuffer );
	return 0;
}

static int proc_close( struct inode *inode, struct file *file )
{
	struct proc_data *data = (struct proc_data *)file->private_data;
	if ( data->on_close != NULL ) data->on_close( inode, file );
	if ( data->rbuffer ) kfree( data->rbuffer );
	if ( data->wbuffer ) kfree( data->wbuffer );
	kfree( data );
	return 0;
}

static struct net_device_list {
	struct net_device *dev;
	struct net_device_list *next;
} *airo_devices = 0;

/* Since the card doesn't automatically switch to the right WEP mode,
   we will make it do it.  If the card isn't associated, every secs we
   will switch WEP modes to see if that will help.  If the card is
   associated we will check every minute to see if anything has
   changed. */
static void timer_func( struct net_device *dev ) {
	struct airo_info *apriv = dev->priv;
	Resp rsp;

/* We don't have a link so try changing the authtype */
	readConfigRid(apriv, 0);
	disable_MAC(apriv, 0);
	switch(apriv->config.authType) {
		case AUTH_ENCRYPT:
/* So drop to OPEN */
			apriv->config.authType = AUTH_OPEN;
			break;
		case AUTH_SHAREDKEY:
			if (apriv->keyindex < auto_wep) {
				set_wep_key(apriv, apriv->keyindex, 0, 0, 0, 0);
				apriv->config.authType = AUTH_SHAREDKEY;
				apriv->keyindex++;
			} else {
			        /* Drop to ENCRYPT */
				apriv->keyindex = 0;
				set_wep_key(apriv, apriv->defindex, 0, 0, 0, 0);
				apriv->config.authType = AUTH_ENCRYPT;
			}
			break;
		default:  /* We'll escalate to SHAREDKEY */
			apriv->config.authType = AUTH_SHAREDKEY;
	}
	apriv->need_commit = 1;
	writeConfigRid(apriv, 0);
	enable_MAC(apriv, &rsp, 0);
	up(&apriv->sem);

/* Schedule check to see if the change worked */
	clear_bit(JOB_AUTOWEP, &apriv->flags);
	apriv->expires = RUN_AT(HZ*3);
}

static int add_airo_dev( struct net_device *dev ) {
	struct net_device_list *node = kmalloc( sizeof( *node ), GFP_KERNEL );
	if ( !node )
		return -ENOMEM;

	node->dev = dev;
	node->next = airo_devices;
	airo_devices = node;

	return 0;
}

static void del_airo_dev( struct net_device *dev ) {
	struct net_device_list **p = &airo_devices;
	while( *p && ( (*p)->dev != dev ) )
		p = &(*p)->next;
	if ( *p && (*p)->dev == dev )
		*p = (*p)->next;
}

#ifdef CONFIG_PCI
static int __devinit airo_pci_probe(struct pci_dev *pdev,
				    const struct pci_device_id *pent)
{
	struct net_device *dev;

	if (pci_enable_device(pdev))
		return -ENODEV;
	pci_set_master(pdev);

	dev = init_airo_card(pdev->irq,	pdev->resource[2].start, 0);
	if (!dev)
		return -ENODEV;

	pci_set_drvdata(pdev, dev);
	set_bit (FLAG_PCI, &((struct airo_info *)dev->priv)->flags);
	return 0;
}

static void __devexit airo_pci_remove(struct pci_dev *pdev)
{
	stop_airo_card(pci_get_drvdata(pdev), 1);
}
#endif

static int __init airo_init_module( void )
{
	int i, have_isa_dev = 0;

	airo_entry = create_proc_entry("aironet",
				       S_IFDIR | airo_perm,
				       proc_root_driver);
        airo_entry->uid = proc_uid;
        airo_entry->gid = proc_gid;

	for( i = 0; i < 4 && io[i] && irq[i]; i++ ) {
		printk( KERN_INFO
			"airo:  Trying to configure ISA adapter at irq=%d io=0x%x\n",
			irq[i], io[i] );
		if (init_airo_card( irq[i], io[i], 0 ))
			have_isa_dev = 1;
	}

#ifdef CONFIG_PCI
	printk( KERN_INFO "airo:  Probing for PCI adapters\n" );
	pci_module_init(&airo_driver);		/* FIXME: check return val */
	printk( KERN_INFO "airo:  Finished probing for PCI adapters\n" );
#endif

	/* Always exit with success, as we are a library module
	 * as well as a driver module
	 */
	return 0;
}

static void __exit airo_cleanup_module( void )
{
	int is_pci = 0;
	while( airo_devices ) {
		printk( KERN_INFO "airo: Unregistering %s\n", airo_devices->dev->name );
#ifdef CONFIG_PCI
		if (test_bit(FLAG_PCI, &((struct airo_info *)airo_devices->dev->priv)->flags))
			is_pci = 1;
#endif
		stop_airo_card( airo_devices->dev, 1 );
	}
	remove_proc_entry("aironet", proc_root_driver);

	if (is_pci) {
#ifdef CONFIG_PCI
		pci_unregister_driver(&airo_driver);
#endif
	}
}

#ifdef WIRELESS_EXT
/*
 * Initial Wireless Extension code for Aironet driver by :
 *	Jean Tourrilhes <jt@hpl.hp.com> - HPL - 17 November 00
 * Conversion to new driver API by :
 *	Jean Tourrilhes <jt@hpl.hp.com> - HPL - 26 March 02
 * Javier also did a good amount of work here, adding some new extensions
 * and fixing my code. Let's just say that without him this code just
 * would not work at all... - Jean II
 */

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get protocol name
 */
static int airo_get_name(struct net_device *dev,
			 struct iw_request_info *info,
			 char *cwrq,
			 char *extra)
{
	strcpy(cwrq, "IEEE 802.11-DS");
	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set frequency
 */
static int airo_set_freq(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_freq *fwrq,
			 char *extra)
{
	struct airo_info *local = dev->priv;
	int rc = -EINPROGRESS;		/* Call commit handler */

	/* If setting by frequency, convert to a channel */
	if((fwrq->e == 1) &&
	   (fwrq->m >= (int) 2.412e8) &&
	   (fwrq->m <= (int) 2.487e8)) {
		int f = fwrq->m / 100000;
		int c = 0;
		while((c < 14) && (f != frequency_list[c]))
			c++;
		/* Hack to fall through... */
		fwrq->e = 0;
		fwrq->m = c + 1;
	}
	/* Setting by channel number */
	if((fwrq->m > 1000) || (fwrq->e > 0))
		rc = -EOPNOTSUPP;
	else {
		int channel = fwrq->m;
		/* We should do a better check than that,
		 * based on the card capability !!! */
		if((channel < 1) || (channel > 16)) {
			printk(KERN_DEBUG "%s: New channel value of %d is invalid!\n", dev->name, fwrq->m);
			rc = -EINVAL;
		} else {
			readConfigRid(local, 1);
			/* Yes ! We can set it !!! */
			local->config.channelSet = (u16)(channel - 1);
			local->need_commit = 1;
		}
	}
	return rc;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get frequency
 */
static int airo_get_freq(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_freq *fwrq,
			 char *extra)
{
	struct airo_info *local = dev->priv;
	StatusRid status_rid;		/* Card status info */

	readConfigRid(local, 1);
	if ((local->config.opmode & 0xFF) == MODE_STA_ESS)
		status_rid.channel = local->config.channelSet;
	else
		readStatusRid(local, &status_rid, 1);

#ifdef WEXT_USECHANNELS
	fwrq->m = ((int)status_rid.channel) + 1;
	fwrq->e = 0;
#else
	{
		int f = (int)status_rid.channel;
		fwrq->m = frequency_list[f] * 100000;
		fwrq->e = 1;
	}
#endif

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set ESSID
 */
static int airo_set_essid(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *dwrq,
			  char *extra)
{
	struct airo_info *local = dev->priv;
	Resp rsp;
	SsidRid SSID_rid;		/* SSIDs */

	/* Reload the list of current SSID */
	readSsidRid(local, &SSID_rid);

	/* Check if we asked for `any' */
	if(dwrq->flags == 0) {
		/* Just send an empty SSID list */
		memset(&SSID_rid, 0, sizeof(SSID_rid));
	} else {
		int	index = (dwrq->flags & IW_ENCODE_INDEX) - 1;

		/* Check the size of the string */
		if(dwrq->length > IW_ESSID_MAX_SIZE+1) {
			return -E2BIG ;
		}
		/* Check if index is valid */
		if((index < 0) || (index >= 4)) {
			return -EINVAL;
		}

		/* Set the SSID */
		memset(SSID_rid.ssids[index].ssid, 0,
		       sizeof(SSID_rid.ssids[index].ssid));
		memcpy(SSID_rid.ssids[index].ssid, extra, dwrq->length);
		SSID_rid.ssids[index].len = dwrq->length - 1;
		SSID_rid.len = sizeof(SSID_rid);
	}
	/* Write it to the card */
	disable_MAC(local, 1);
	writeSsidRid(local, &SSID_rid);
	enable_MAC(local, &rsp, 1);

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get ESSID
 */
static int airo_get_essid(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *dwrq,
			  char *extra)
{
	struct airo_info *local = dev->priv;
	StatusRid status_rid;		/* Card status info */

	readStatusRid(local, &status_rid, 1);

	/* Note : if dwrq->flags != 0, we should
	 * get the relevant SSID from the SSID list... */

	/* Get the current SSID */
	memcpy(extra, status_rid.SSID, status_rid.SSIDlen);
	extra[status_rid.SSIDlen] = '\0';
	/* If none, we may want to get the one that was set */

	/* Push it out ! */
	dwrq->length = status_rid.SSIDlen + 1;
	dwrq->flags = 1; /* active */

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set AP address
 */
static int airo_set_wap(struct net_device *dev,
			struct iw_request_info *info,
			struct sockaddr *awrq,
			char *extra)
{
	struct airo_info *local = dev->priv;
	Cmd cmd;
	Resp rsp;
	APListRid APList_rid;
	static const unsigned char bcast[ETH_ALEN] = { 255, 255, 255, 255, 255, 255 };

	if (awrq->sa_family != ARPHRD_ETHER)
		return -EINVAL;
	else if (!memcmp(bcast, awrq->sa_data, ETH_ALEN)) {
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd=CMD_LOSE_SYNC;
		if (down_interruptible(&local->sem))
			return -ERESTARTSYS;
		issuecommand(local, &cmd, &rsp);
		up(&local->sem);
	} else {
		memset(&APList_rid, 0, sizeof(APList_rid));
		APList_rid.len = sizeof(APList_rid);
		memcpy(APList_rid.ap[0], awrq->sa_data, ETH_ALEN);
		disable_MAC(local, 1);
		writeAPListRid(local, &APList_rid);
		enable_MAC(local, &rsp, 1);
	}
	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get AP address
 */
static int airo_get_wap(struct net_device *dev,
			struct iw_request_info *info,
			struct sockaddr *awrq,
			char *extra)
{
	struct airo_info *local = dev->priv;
	StatusRid status_rid;		/* Card status info */

	readStatusRid(local, &status_rid, 1);

	/* Tentative. This seems to work, wow, I'm lucky !!! */
	memcpy(awrq->sa_data, status_rid.bssid[0], ETH_ALEN);
	awrq->sa_family = ARPHRD_ETHER;

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set Nickname
 */
static int airo_set_nick(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *dwrq,
			 char *extra)
{
	struct airo_info *local = dev->priv;

	/* Check the size of the string */
	if(dwrq->length > 16 + 1) {
		return -E2BIG;
	}
	readConfigRid(local, 1);
	memset(local->config.nodeName, 0, sizeof(local->config.nodeName));
	memcpy(local->config.nodeName, extra, dwrq->length);
	local->need_commit = 1;

	return -EINPROGRESS;		/* Call commit handler */
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get Nickname
 */
static int airo_get_nick(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *dwrq,
			 char *extra)
{
	struct airo_info *local = dev->priv;

	readConfigRid(local, 1);
	strncpy(extra, local->config.nodeName, 16);
	extra[16] = '\0';
	dwrq->length = strlen(extra) + 1;

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set Bit-Rate
 */
static int airo_set_rate(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *vwrq,
			 char *extra)
{
	struct airo_info *local = dev->priv;
	CapabilityRid cap_rid;		/* Card capability info */
	u8	brate = 0;
	int	i;

	/* First : get a valid bit rate value */
	readCapabilityRid(local, &cap_rid);

	/* Which type of value ? */
	if((vwrq->value < 8) && (vwrq->value >= 0)) {
		/* Setting by rate index */
		/* Find value in the magic rate table */
		brate = cap_rid.supportedRates[vwrq->value];
	} else {
		/* Setting by frequency value */
		u8	normvalue = (u8) (vwrq->value/500000);

		/* Check if rate is valid */
		for(i = 0 ; i < 8 ; i++) {
			if(normvalue == cap_rid.supportedRates[i]) {
				brate = normvalue;
				break;
			}
		}
	}
	/* -1 designed the max rate (mostly auto mode) */
	if(vwrq->value == -1) {
		/* Get the highest available rate */
		for(i = 0 ; i < 8 ; i++) {
			if(cap_rid.supportedRates[i] == 0)
				break;
		}
		if(i != 0)
			brate = cap_rid.supportedRates[i - 1];
	}
	/* Check that it is valid */
	if(brate == 0) {
		return -EINVAL;
	}

	readConfigRid(local, 1);
	/* Now, check if we want a fixed or auto value */
	if(vwrq->fixed == 0) {
		/* Fill all the rates up to this max rate */
		memset(local->config.rates, 0, 8);
		for(i = 0 ; i < 8 ; i++) {
			local->config.rates[i] = cap_rid.supportedRates[i];
			if(local->config.rates[i] == brate)
				break;
		}
	} else {
		/* Fixed mode */
		/* One rate, fixed */
		memset(local->config.rates, 0, 8);
		local->config.rates[0] = brate;
	}
	local->need_commit = 1;

	return -EINPROGRESS;		/* Call commit handler */
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get Bit-Rate
 */
static int airo_get_rate(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *vwrq,
			 char *extra)
{
	struct airo_info *local = dev->priv;
	StatusRid status_rid;		/* Card status info */

	readStatusRid(local, &status_rid, 1);

	vwrq->value = status_rid.currentXmitRate * 500000;
	/* If more than one rate, set auto */
	readConfigRid(local, 1);
	vwrq->fixed = (local->config.rates[1] == 0);

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set RTS threshold
 */
static int airo_set_rts(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_param *vwrq,
			char *extra)
{
	struct airo_info *local = dev->priv;
	int rthr = vwrq->value;

	if(vwrq->disabled)
		rthr = 2312;
	if((rthr < 0) || (rthr > 2312)) {
		return -EINVAL;
	}
	readConfigRid(local, 1);
	local->config.rtsThres = rthr;
	local->need_commit = 1;

	return -EINPROGRESS;		/* Call commit handler */
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get RTS threshold
 */
static int airo_get_rts(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_param *vwrq,
			char *extra)
{
	struct airo_info *local = dev->priv;

	readConfigRid(local, 1);
	vwrq->value = local->config.rtsThres;
	vwrq->disabled = (vwrq->value >= 2312);
	vwrq->fixed = 1;

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set Fragmentation threshold
 */
static int airo_set_frag(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *vwrq,
			 char *extra)
{
	struct airo_info *local = dev->priv;
	int fthr = vwrq->value;

	if(vwrq->disabled)
		fthr = 2312;
	if((fthr < 256) || (fthr > 2312)) {
		return -EINVAL;
	}
	fthr &= ~0x1;	/* Get an even value - is it really needed ??? */
	readConfigRid(local, 1);
	local->config.fragThresh = (u16)fthr;
	local->need_commit = 1;

	return -EINPROGRESS;		/* Call commit handler */
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get Fragmentation threshold
 */
static int airo_get_frag(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *vwrq,
			 char *extra)
{
	struct airo_info *local = dev->priv;

	readConfigRid(local, 1);
	vwrq->value = local->config.fragThresh;
	vwrq->disabled = (vwrq->value >= 2312);
	vwrq->fixed = 1;

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set Mode of Operation
 */
static int airo_set_mode(struct net_device *dev,
			 struct iw_request_info *info,
			 __u32 *uwrq,
			 char *extra)
{
	struct airo_info *local = dev->priv;
	int commit = 1;

	readConfigRid(local, 1);
	if ((local->config.rmode & 0xff) >= RXMODE_RFMON)
		commit = 2;

	switch(*uwrq) {
		case IW_MODE_ADHOC:
			local->config.opmode &= 0xFF00;
			local->config.opmode |= MODE_STA_IBSS;
			local->config.rmode &= 0xfe00;
			local->config.scanMode = SCANMODE_ACTIVE;
			clear_bit (FLAG_802_11, &local->flags);
			break;
		case IW_MODE_INFRA:
			local->config.opmode &= 0xFF00;
			local->config.opmode |= MODE_STA_ESS;
			local->config.rmode &= 0xfe00;
			local->config.scanMode = SCANMODE_ACTIVE;
			clear_bit (FLAG_802_11, &local->flags);
			break;
		case IW_MODE_MASTER:
			local->config.opmode &= 0xFF00;
			local->config.opmode |= MODE_AP;
			local->config.rmode &= 0xfe00;
			local->config.scanMode = SCANMODE_ACTIVE;
			clear_bit (FLAG_802_11, &local->flags);
			break;
		case IW_MODE_REPEAT:
			local->config.opmode &= 0xFF00;
			local->config.opmode |= MODE_AP_RPTR;
			local->config.rmode &= 0xfe00;
			local->config.scanMode = SCANMODE_ACTIVE;
			clear_bit (FLAG_802_11, &local->flags);
			break;
		case IW_MODE_MONITOR:
			local->config.opmode &= 0xFF00;
			local->config.opmode |= MODE_STA_ESS;
			local->config.rmode &= 0xfe00;
			local->config.rmode |= RXMODE_RFMON | RXMODE_DISABLE_802_3_HEADER;
			local->config.scanMode = SCANMODE_PASSIVE;
			set_bit (FLAG_802_11, &local->flags);
			break;
		default:
			return -EINVAL;
	}
	local->need_commit = commit;

	return -EINPROGRESS;		/* Call commit handler */
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get Mode of Operation
 */
static int airo_get_mode(struct net_device *dev,
			 struct iw_request_info *info,
			 __u32 *uwrq,
			 char *extra)
{
	struct airo_info *local = dev->priv;

	readConfigRid(local, 1);
	/* If not managed, assume it's ad-hoc */
	switch (local->config.opmode & 0xFF) {
		case MODE_STA_ESS:
			*uwrq = IW_MODE_INFRA;
			break;
		case MODE_AP:
			*uwrq = IW_MODE_MASTER;
			break;
		case MODE_AP_RPTR:
			*uwrq = IW_MODE_REPEAT;
			break;
		default:
			*uwrq = IW_MODE_ADHOC;
	}

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set Encryption Key
 */
static int airo_set_encode(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_point *dwrq,
			   char *extra)
{
	struct airo_info *local = dev->priv;
	CapabilityRid cap_rid;		/* Card capability info */

	/* Is WEP supported ? */
	readCapabilityRid(local, &cap_rid);
	/* Older firmware doesn't support this...
	if(!(cap_rid.softCap & 2)) {
		return -EOPNOTSUPP;
	} */
	readConfigRid(local, 1);

	/* Basic checking: do we have a key to set ?
	 * Note : with the new API, it's impossible to get a NULL pointer.
	 * Therefore, we need to check a key size == 0 instead.
	 * New version of iwconfig properly set the IW_ENCODE_NOKEY flag
	 * when no key is present (only change flags), but older versions
	 * don't do it. - Jean II */
	if (dwrq->length > 0) {
		wep_key_t key;
		int index = (dwrq->flags & IW_ENCODE_INDEX) - 1;
		int current_index = get_wep_key(local, 0xffff);
		/* Check the size of the key */
		if (dwrq->length > MAX_KEY_SIZE) {
			return -EINVAL;
		}
		/* Check the index (none -> use current) */
		if ((index < 0) || (index>=(cap_rid.softCap&0x80)?4:1))
			index = current_index;
		/* Set the length */
		if (dwrq->length > MIN_KEY_SIZE)
			key.len = MAX_KEY_SIZE;
		else
			if (dwrq->length > 0)
				key.len = MIN_KEY_SIZE;
			else
				/* Disable the key */
				key.len = 0;
		/* Check if the key is not marked as invalid */
		if(!(dwrq->flags & IW_ENCODE_NOKEY)) {
			/* Cleanup */
			memset(key.key, 0, MAX_KEY_SIZE);
			/* Copy the key in the driver */
			memcpy(key.key, extra, dwrq->length);
			/* Send the key to the card */
			set_wep_key(local, index, key.key, key.len, 1, 1);
		}
		/* WE specify that if a valid key is set, encryption
		 * should be enabled (user may turn it off later)
		 * This is also how "iwconfig ethX key on" works */
		if((index == current_index) && (key.len > 0) &&
		   (local->config.authType == AUTH_OPEN)) {
			local->config.authType = AUTH_ENCRYPT;
			local->need_commit = 1;
		}
	} else {
		/* Do we want to just set the transmit key index ? */
		int index = (dwrq->flags & IW_ENCODE_INDEX) - 1;
		if ((index>=0) && (index<(cap_rid.softCap&0x80)?4:1)) {
			set_wep_key(local, index, 0, 0, 1, 1);
		} else
			/* Don't complain if only change the mode */
			if(!dwrq->flags & IW_ENCODE_MODE) {
				return -EINVAL;
			}
	}
	/* Read the flags */
	if(dwrq->flags & IW_ENCODE_DISABLED)
		local->config.authType = AUTH_OPEN;	// disable encryption
	if(dwrq->flags & IW_ENCODE_RESTRICTED)
		local->config.authType = AUTH_SHAREDKEY;	// Only Both
	if(dwrq->flags & IW_ENCODE_OPEN)
		local->config.authType = AUTH_ENCRYPT;	// Only Wep
	/* Commit the changes to flags if needed */
	if(dwrq->flags & IW_ENCODE_MODE)
		local->need_commit = 1;
	return -EINPROGRESS;		/* Call commit handler */
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get Encryption Key
 */
static int airo_get_encode(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_point *dwrq,
			   char *extra)
{
	struct airo_info *local = dev->priv;
	int index = (dwrq->flags & IW_ENCODE_INDEX) - 1;
	CapabilityRid cap_rid;		/* Card capability info */

	/* Is it supported ? */
	readCapabilityRid(local, &cap_rid);
	if(!(cap_rid.softCap & 2)) {
		return -EOPNOTSUPP;
	}
	readConfigRid(local, 1);
	/* Check encryption mode */
	switch(local->config.authType)	{
		case AUTH_ENCRYPT:
			dwrq->flags = IW_ENCODE_OPEN;
			break;
		case AUTH_SHAREDKEY:
			dwrq->flags = IW_ENCODE_RESTRICTED;
			break;
		default:
		case AUTH_OPEN:
			dwrq->flags = IW_ENCODE_DISABLED;
			break;
	}
	/* We can't return the key, so set the proper flag and return zero */
	dwrq->flags |= IW_ENCODE_NOKEY;
	memset(extra, 0, 16);

	/* Which key do we want ? -1 -> tx index */
	if((index < 0) || (index >= (cap_rid.softCap & 0x80) ? 4 : 1))
		index = get_wep_key(local, 0xffff);
	dwrq->flags |= index + 1;
	/* Copy the key to the user buffer */
	dwrq->length = get_wep_key(local, index);
	if (dwrq->length > 16) {
		dwrq->length=0;
	}
	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set Tx-Power
 */
static int airo_set_txpow(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_param *vwrq,
			  char *extra)
{
	struct airo_info *local = dev->priv;
	CapabilityRid cap_rid;		/* Card capability info */
	int i;
	int rc = -EINVAL;

	readCapabilityRid(local, &cap_rid);

	if (vwrq->disabled) {
		set_bit (FLAG_RADIO_OFF, &local->flags);
		local->need_commit = 1;
		return -EINPROGRESS;		/* Call commit handler */
	}
	if (vwrq->flags != IW_TXPOW_MWATT) {
		return -EINVAL;
	}
	clear_bit (FLAG_RADIO_OFF, &local->flags);
	for (i = 0; cap_rid.txPowerLevels[i] && (i < 8); i++)
		if ((vwrq->value==cap_rid.txPowerLevels[i])) {
			readConfigRid(local, 1);
			local->config.txPower = vwrq->value;
			local->need_commit = 1;
			rc = -EINPROGRESS;	/* Call commit handler */
			break;
		}
	return rc;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get Tx-Power
 */
static int airo_get_txpow(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_param *vwrq,
			  char *extra)
{
	struct airo_info *local = dev->priv;

	readConfigRid(local, 1);
	vwrq->value = local->config.txPower;
	vwrq->fixed = 1;	/* No power control */
	vwrq->disabled = test_bit(FLAG_RADIO_OFF, &local->flags);
	vwrq->flags = IW_TXPOW_MWATT;

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set Retry limits
 */
static int airo_set_retry(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_param *vwrq,
			  char *extra)
{
	struct airo_info *local = dev->priv;
	int rc = -EINVAL;

	if(vwrq->disabled) {
		return -EINVAL;
	}
	readConfigRid(local, 1);
	if(vwrq->flags & IW_RETRY_LIMIT) {
		if(vwrq->flags & IW_RETRY_MAX)
			local->config.longRetryLimit = vwrq->value;
		else if (vwrq->flags & IW_RETRY_MIN)
			local->config.shortRetryLimit = vwrq->value;
		else {
			/* No modifier : set both */
			local->config.longRetryLimit = vwrq->value;
			local->config.shortRetryLimit = vwrq->value;
		}
		local->need_commit = 1;
		rc = -EINPROGRESS;		/* Call commit handler */
	}
	if(vwrq->flags & IW_RETRY_LIFETIME) {
		local->config.txLifetime = vwrq->value / 1024;
		local->need_commit = 1;
		rc = -EINPROGRESS;		/* Call commit handler */
	}
	return rc;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get Retry limits
 */
static int airo_get_retry(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_param *vwrq,
			  char *extra)
{
	struct airo_info *local = dev->priv;

	vwrq->disabled = 0;      /* Can't be disabled */

	readConfigRid(local, 1);
	/* Note : by default, display the min retry number */
	if((vwrq->flags & IW_RETRY_TYPE) == IW_RETRY_LIFETIME) {
		vwrq->flags = IW_RETRY_LIFETIME;
		vwrq->value = (int)local->config.txLifetime * 1024;
	} else if((vwrq->flags & IW_RETRY_MAX)) {
		vwrq->flags = IW_RETRY_LIMIT | IW_RETRY_MAX;
		vwrq->value = (int)local->config.longRetryLimit;
	} else {
		vwrq->flags = IW_RETRY_LIMIT;
		vwrq->value = (int)local->config.shortRetryLimit;
		if((int)local->config.shortRetryLimit != (int)local->config.longRetryLimit)
			vwrq->flags |= IW_RETRY_MIN;
	}

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get range info
 */
static int airo_get_range(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *dwrq,
			  char *extra)
{
	struct airo_info *local = dev->priv;
	struct iw_range *range = (struct iw_range *) extra;
	CapabilityRid cap_rid;		/* Card capability info */
	int		i;
	int		k;

	readCapabilityRid(local, &cap_rid);

	dwrq->length = sizeof(struct iw_range);
	memset(range, 0, sizeof(*range));
	range->min_nwid = 0x0000;
	range->max_nwid = 0x0000;
	range->num_channels = 14;
	/* Should be based on cap_rid.country to give only
	 * what the current card support */
	k = 0;
	for(i = 0; i < 14; i++) {
		range->freq[k].i = i + 1; /* List index */
		range->freq[k].m = frequency_list[i] * 100000;
		range->freq[k++].e = 1;	/* Values in table in MHz -> * 10^5 * 10 */
	}
	range->num_frequency = k;

	/* Hum... Should put the right values there */
	range->max_qual.qual = 10;
	range->max_qual.level = 0x100 - 120;	/* -120 dBm */
	range->max_qual.noise = 0;
	range->sensitivity = 65535;

	for(i = 0 ; i < 8 ; i++) {
		range->bitrate[i] = cap_rid.supportedRates[i] * 500000;
		if(range->bitrate[i] == 0)
			break;
	}
	range->num_bitrates = i;

	/* Set an indication of the max TCP throughput
	 * in bit/s that we can expect using this interface.
	 * May be use for QoS stuff... Jean II */
	if(i > 2)
		range->throughput = 5000 * 1000;
	else
		range->throughput = 1500 * 1000;

	range->min_rts = 0;
	range->max_rts = 2312;
	range->min_frag = 256;
	range->max_frag = 2312;

	if(cap_rid.softCap & 2) {
		// WEP: RC4 40 bits
		range->encoding_size[0] = 5;
		// RC4 ~128 bits
		if (cap_rid.softCap & 0x100) {
			range->encoding_size[1] = 13;
			range->num_encoding_sizes = 2;
		} else
			range->num_encoding_sizes = 1;
		range->max_encoding_tokens = (cap_rid.softCap & 0x80) ? 4 : 1;
	} else {
		range->num_encoding_sizes = 0;
		range->max_encoding_tokens = 0;
	}
	range->min_pmp = 0;
	range->max_pmp = 5000000;	/* 5 secs */
	range->min_pmt = 0;
	range->max_pmt = 65535 * 1024;	/* ??? */
	range->pmp_flags = IW_POWER_PERIOD;
	range->pmt_flags = IW_POWER_TIMEOUT;
	range->pm_capa = IW_POWER_PERIOD | IW_POWER_TIMEOUT | IW_POWER_ALL_R;

	/* Transmit Power - values are in mW */
	for(i = 0 ; i < 8 ; i++) {
		range->txpower[i] = cap_rid.txPowerLevels[i];
		if(range->txpower[i] == 0)
			break;
	}
	range->num_txpower = i;
	range->txpower_capa = IW_TXPOW_MWATT;
	range->we_version_source = 12;
	range->we_version_compiled = WIRELESS_EXT;
	range->retry_capa = IW_RETRY_LIMIT | IW_RETRY_LIFETIME;
	range->retry_flags = IW_RETRY_LIMIT;
	range->r_time_flags = IW_RETRY_LIFETIME;
	range->min_retry = 1;
	range->max_retry = 65535;
	range->min_r_time = 1024;
	range->max_r_time = 65535 * 1024;
	/* Experimental measurements - boundary 11/5.5 Mb/s */
	/* Note : with or without the (local->rssi), results
	 * are somewhat different. - Jean II */
	range->avg_qual.qual = 6;
	if (local->rssi)
		range->avg_qual.level = 186;	/* -70 dBm */
	else
		range->avg_qual.level = 176;	/* -80 dBm */
	range->avg_qual.noise = 0;

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set Power Management
 */
static int airo_set_power(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_param *vwrq,
			  char *extra)
{
	struct airo_info *local = dev->priv;

	readConfigRid(local, 1);
	if (vwrq->disabled) {
		if ((local->config.rmode & 0xFF) >= RXMODE_RFMON) {
			return -EINVAL;
		}
		local->config.powerSaveMode = POWERSAVE_CAM;
		local->config.rmode &= 0xFF00;
		local->config.rmode |= RXMODE_BC_MC_ADDR;
		local->need_commit = 1;
		return -EINPROGRESS;		/* Call commit handler */
	}
	if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_TIMEOUT) {
		local->config.fastListenDelay = (vwrq->value + 500) / 1024;
		local->config.powerSaveMode = POWERSAVE_PSPCAM;
		local->need_commit = 1;
	} else if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_PERIOD) {
		local->config.fastListenInterval = local->config.listenInterval = (vwrq->value + 500) / 1024;
		local->config.powerSaveMode = POWERSAVE_PSPCAM;
		local->need_commit = 1;
	}
	switch (vwrq->flags & IW_POWER_MODE) {
		case IW_POWER_UNICAST_R:
			if ((local->config.rmode & 0xFF) >= RXMODE_RFMON) {
				return -EINVAL;
			}
			local->config.rmode &= 0xFF00;
			local->config.rmode |= RXMODE_ADDR;
			local->need_commit = 1;
			break;
		case IW_POWER_ALL_R:
			if ((local->config.rmode & 0xFF) >= RXMODE_RFMON) {
				return -EINVAL;
			}
			local->config.rmode &= 0xFF00;
			local->config.rmode |= RXMODE_BC_MC_ADDR;
			local->need_commit = 1;
		case IW_POWER_ON:
			break;
		default:
			return -EINVAL;
	}
	// Note : we may want to factor local->need_commit here
	// Note2 : may also want to factor RXMODE_RFMON test
	return -EINPROGRESS;		/* Call commit handler */
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get Power Management
 */
static int airo_get_power(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_param *vwrq,
			  char *extra)
{
	struct airo_info *local = dev->priv;
	int mode;

	readConfigRid(local, 1);
	mode = local->config.powerSaveMode;
	if ((vwrq->disabled = (mode == POWERSAVE_CAM)))
		return 0;
	if ((vwrq->flags & IW_POWER_TYPE) == IW_POWER_TIMEOUT) {
		vwrq->value = (int)local->config.fastListenDelay * 1024;
		vwrq->flags = IW_POWER_TIMEOUT;
	} else {
		vwrq->value = (int)local->config.fastListenInterval * 1024;
		vwrq->flags = IW_POWER_PERIOD;
	}
	if ((local->config.rmode & 0xFF) == RXMODE_ADDR)
		vwrq->flags |= IW_POWER_UNICAST_R;
	else
		vwrq->flags |= IW_POWER_ALL_R;

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set Sensitivity
 */
static int airo_set_sens(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *vwrq,
			 char *extra)
{
	struct airo_info *local = dev->priv;

	readConfigRid(local, 1);
	local->config.rssiThreshold = vwrq->disabled ? RSSI_DEFAULT : vwrq->value;
	local->need_commit = 1;

	return -EINPROGRESS;		/* Call commit handler */
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get Sensitivity
 */
static int airo_get_sens(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *vwrq,
			 char *extra)
{
	struct airo_info *local = dev->priv;

	readConfigRid(local, 1);
	vwrq->value = local->config.rssiThreshold;
	vwrq->disabled = (vwrq->value == 0);
	vwrq->fixed = 1;

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get AP List
 * Note : this is deprecated in favor of IWSCAN
 */
static int airo_get_aplist(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_point *dwrq,
			   char *extra)
{
	struct airo_info *local = dev->priv;
	struct sockaddr *address = (struct sockaddr *) extra;
	struct iw_quality qual[IW_MAX_AP];
	BSSListRid BSSList;
	int i;
	int loseSync = capable(CAP_NET_ADMIN) ? 1: -1;

	for (i = 0; i < IW_MAX_AP; i++) {
		if (readBSSListRid(local, loseSync, &BSSList))
			break;
		loseSync = 0;
		memcpy(address[i].sa_data, BSSList.bssid, ETH_ALEN);
		address[i].sa_family = ARPHRD_ETHER;
		if (local->rssi)
			qual[i].level = 0x100 - local->rssi[BSSList.rssi].rssidBm;
		else
			qual[i].level = (BSSList.rssi + 321) / 2;
		qual[i].qual = qual[i].noise = 0;
		qual[i].updated = 2;
		if (BSSList.index == 0xffff)
			break;
	}
	if (!i) {
		StatusRid status_rid;		/* Card status info */
		readStatusRid(local, &status_rid, 1);
		for (i = 0;
		     i < min(IW_MAX_AP, 4) &&
			     (status_rid.bssid[i][0]
			      & status_rid.bssid[i][1]
			      & status_rid.bssid[i][2]
			      & status_rid.bssid[i][3]
			      & status_rid.bssid[i][4]
			      & status_rid.bssid[i][5])!=0xff &&
			     (status_rid.bssid[i][0]
			      | status_rid.bssid[i][1]
			      | status_rid.bssid[i][2]
			      | status_rid.bssid[i][3]
			      | status_rid.bssid[i][4]
			      | status_rid.bssid[i][5]);
		     i++) {
			memcpy(address[i].sa_data,
			       status_rid.bssid[i], ETH_ALEN);
			address[i].sa_family = ARPHRD_ETHER;
		}
	} else {
		dwrq->flags = 1; /* Should be define'd */
		memcpy(extra + sizeof(struct sockaddr)*i,
		       &qual,  sizeof(struct iw_quality)*i);
	}
	dwrq->length = i;

	return 0;
}

#if WIRELESS_EXT > 13
/*------------------------------------------------------------------*/
/*
 * Wireless Handler : Initiate Scan
 */
static int airo_set_scan(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *vwrq,
			 char *extra)
{
	struct airo_info *ai = dev->priv;
	Cmd cmd;
	Resp rsp;

	/* Note : you may have realised that, as this is a SET operation,
	 * this is privileged and therefore a normal user can't
	 * perform scanning.
	 * This is not an error, while the device perform scanning,
	 * traffic doesn't flow, so it's a perfect DoS...
	 * Jean II */

	/* Initiate a scan command */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd=CMD_LISTBSS;
	if (down_interruptible(&ai->sem))
		return -ERESTARTSYS;
	issuecommand(ai, &cmd, &rsp);
	ai->scan_timestamp = jiffies;
	up(&ai->sem);

	/* At this point, just return to the user. */

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Translate scan data returned from the card to a card independent
 * format that the Wireless Tools will understand - Jean II
 */
static inline char *airo_translate_scan(struct net_device *dev,
					char *current_ev,
					char *end_buf,
					BSSListRid *list)
{
	struct airo_info *ai = dev->priv;
	struct iw_event		iwe;		/* Temporary buffer */
	u16			capabilities;
	char *			current_val;	/* For rates */
	int			i;

	/* First entry *MUST* be the AP MAC address */
	iwe.cmd = SIOCGIWAP;
	iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
	memcpy(iwe.u.ap_addr.sa_data, list->bssid, ETH_ALEN);
	current_ev = iwe_stream_add_event(current_ev, end_buf, &iwe, IW_EV_ADDR_LEN);

	/* Other entries will be displayed in the order we give them */

	/* Add the ESSID */
	iwe.u.data.length = list->ssidLen;
	if(iwe.u.data.length > 32)
		iwe.u.data.length = 32;
	iwe.cmd = SIOCGIWESSID;
	iwe.u.data.flags = 1;
	current_ev = iwe_stream_add_point(current_ev, end_buf, &iwe, list->ssid);

	/* Add mode */
	iwe.cmd = SIOCGIWMODE;
	capabilities = le16_to_cpu(list->cap);
	if(capabilities & (CAP_ESS | CAP_IBSS)) {
		if(capabilities & CAP_ESS)
			iwe.u.mode = IW_MODE_MASTER;
		else
			iwe.u.mode = IW_MODE_ADHOC;
		current_ev = iwe_stream_add_event(current_ev, end_buf, &iwe, IW_EV_UINT_LEN);
	}

	/* Add frequency */
	iwe.cmd = SIOCGIWFREQ;
	iwe.u.freq.m = le16_to_cpu(list->dsChannel);
	iwe.u.freq.m = frequency_list[iwe.u.freq.m] * 100000;
	iwe.u.freq.e = 1;
	current_ev = iwe_stream_add_event(current_ev, end_buf, &iwe, IW_EV_FREQ_LEN);

	/* Add quality statistics */
	iwe.cmd = IWEVQUAL;
	if (ai->rssi)
		iwe.u.qual.level = 0x100 - ai->rssi[list->rssi].rssidBm;
	else
		iwe.u.qual.level = (list->rssi + 321) / 2;
	iwe.u.qual.noise = 0;
	iwe.u.qual.qual = 0;
	current_ev = iwe_stream_add_event(current_ev, end_buf, &iwe, IW_EV_QUAL_LEN);

	/* Add encryption capability */
	iwe.cmd = SIOCGIWENCODE;
	if(capabilities & CAP_PRIVACY)
		iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
	else
		iwe.u.data.flags = IW_ENCODE_DISABLED;
	iwe.u.data.length = 0;
	current_ev = iwe_stream_add_point(current_ev, end_buf, &iwe, list->ssid);

	/* Rate : stuffing multiple values in a single event require a bit
	 * more of magic - Jean II */
	current_val = current_ev + IW_EV_LCP_LEN;

	iwe.cmd = SIOCGIWRATE;
	/* Those two flags are ignored... */
	iwe.u.bitrate.fixed = iwe.u.bitrate.disabled = 0;
	/* Max 8 values */
	for(i = 0 ; i < 8 ; i++) {
		/* NULL terminated */
		if(list->rates[i] == 0)
			break;
		/* Bit rate given in 500 kb/s units (+ 0x80) */
		iwe.u.bitrate.value = ((list->rates[i] & 0x7f) * 500000);
		/* Add new value to event */
		current_val = iwe_stream_add_value(current_ev, current_val, end_buf, &iwe, IW_EV_PARAM_LEN);
	}
	/* Check if we added any event */
	if((current_val - current_ev) > IW_EV_LCP_LEN)
		current_ev = current_val;

	/* The other data in the scan result are not really
	 * interesting, so for now drop it - Jean II */
	return current_ev;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : Read Scan Results
 */
static int airo_get_scan(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *dwrq,
			 char *extra)
{
	struct airo_info *ai = dev->priv;
	BSSListRid BSSList;
	int rc;
	char *current_ev = extra;

	/* When we are associated again, the scan has surely finished.
	 * Just in case, let's make sure enough time has elapsed since
	 * we started the scan. - Javier */
	if(ai->scan_timestamp && time_before(jiffies,ai->scan_timestamp+3*HZ)) {
		/* Important note : we don't want to block the caller
		 * until results are ready for various reasons.
		 * First, managing wait queues is complex and racy
		 * (there may be multiple simultaneous callers).
		 * Second, we grab some rtnetlink lock before comming
		 * here (in dev_ioctl()).
		 * Third, the caller can wait on the Wireless Event
		 * - Jean II */
		return -EAGAIN;
	}
	ai->scan_timestamp = 0;

	/* There's only a race with proc_BSSList_open(), but its
	 * consequences are begnign. So I don't bother fixing it - Javier */

	/* Try to read the first entry of the scan result */
	rc = PC4500_readrid(ai, RID_BSSLISTFIRST, &BSSList, sizeof(BSSList), 1);
	if((rc) || (BSSList.index == 0xffff)) {
		/* Client error, no scan results...
		 * The caller need to restart the scan. */
		return -ENODATA;
	}

	/* Read and parse all entries */
	while((!rc) && (BSSList.index != 0xffff)) {
		/* Translate to WE format this entry */
		current_ev = airo_translate_scan(dev, current_ev,
						 extra + IW_SCAN_MAX_DATA,
						 &BSSList);

		/* Read next entry */
		rc = PC4500_readrid(ai, RID_BSSLISTNEXT,
				    &BSSList, sizeof(BSSList), 1);
	}
	/* Length of data */
	dwrq->length = (current_ev - extra);
	dwrq->flags = 0;	/* todo */

	return 0;
}
#endif	/* WIRELESS_EXT > 13 */

#if WIRELESS_EXT <= 15
#ifdef WIRELESS_SPY
/*------------------------------------------------------------------*/
/*
 * Wireless Handler : set Spy List
 */
static int airo_set_spy(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_point *dwrq,
			char *extra)
{
	struct airo_info *local = dev->priv;
	struct sockaddr *address = (struct sockaddr *) extra;

	/* Disable spy while we copy the addresses.
	 * As we don't disable interrupts, we need to do this to avoid races */
	local->spy_number = 0;

	if (dwrq->length > 0) {
		int i;

		/* Copy addresses */
		for (i = 0; i < dwrq->length; i++)
			memcpy(local->spy_address[i], address[i].sa_data, ETH_ALEN);
		/* Reset stats */
		memset(local->spy_stat, 0, sizeof(struct iw_quality) * IW_MAX_SPY);
	}
	/* Enable addresses */
	local->spy_number = dwrq->length;

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Wireless Handler : get Spy List
 */
static int airo_get_spy(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_point *dwrq,
			char *extra)
{
	struct airo_info *local = dev->priv;
	struct sockaddr *address = (struct sockaddr *) extra;
	int i;

	dwrq->length = local->spy_number;

	/* Copy addresses. */
	for(i = 0; i < local->spy_number; i++) 	{
		memcpy(address[i].sa_data, local->spy_address[i], ETH_ALEN);
		address[i].sa_family = AF_UNIX;
	}
	/* Copy stats to the user buffer (just after). */
	if(local->spy_number > 0)
		memcpy(extra  + (sizeof(struct sockaddr) * local->spy_number),
		       local->spy_stat, sizeof(struct iw_quality) * local->spy_number);
	/* Reset updated flags. */
	for (i=0; i<local->spy_number; i++)
		local->spy_stat[i].updated = 0;
	return 0;
}
#endif			/* WIRELESS_SPY */
#endif /* WIRELESS_EXT <= 15 */

/*------------------------------------------------------------------*/
/*
 * Commit handler : called after a bunch of SET operations
 */
static int airo_config_commit(struct net_device *dev,
			      struct iw_request_info *info,	/* NULL */
			      void *zwrq,			/* NULL */
			      char *extra)			/* NULL */
{
	struct airo_info *local = dev->priv;
	Resp rsp;

	if (!local->need_commit)
		return 0;

	/* Some of the "SET" function may have modified some of the
	 * parameters. It's now time to commit them in the card */
	disable_MAC(local, 1);
	if (local->need_commit > 1) {
		APListRid APList_rid;
		SsidRid SSID_rid;

		readAPListRid(local, &APList_rid);
		readSsidRid(local, &SSID_rid);
		reset_airo_card(dev);
		disable_MAC(local, 1);
		writeSsidRid(local, &SSID_rid);
		writeAPListRid(local, &APList_rid);
	}
	if (down_interruptible(&local->sem))
		return -ERESTARTSYS;
	writeConfigRid(local, 0);
	enable_MAC(local, &rsp, 0);
	if (local->need_commit > 1)
		airo_set_promisc(local);
	else
		up(&local->sem);

	return 0;
}

/*------------------------------------------------------------------*/
/*
 * Structures to export the Wireless Handlers
 */

static const struct iw_priv_args airo_private_args[] = {
/*{ cmd,         set_args,                            get_args, name } */
  { AIROIOCTL, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | sizeof (aironet_ioctl),
    IW_PRIV_TYPE_BYTE | 2047, "airoioctl" },
  { AIROIDIFC, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | sizeof (aironet_ioctl),
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "airoidifc" },
};

#if WIRELESS_EXT > 12
static const iw_handler		airo_handler[] =
{
	(iw_handler) airo_config_commit,	/* SIOCSIWCOMMIT */
	(iw_handler) airo_get_name,		/* SIOCGIWNAME */
	(iw_handler) NULL,			/* SIOCSIWNWID */
	(iw_handler) NULL,			/* SIOCGIWNWID */
	(iw_handler) airo_set_freq,		/* SIOCSIWFREQ */
	(iw_handler) airo_get_freq,		/* SIOCGIWFREQ */
	(iw_handler) airo_set_mode,		/* SIOCSIWMODE */
	(iw_handler) airo_get_mode,		/* SIOCGIWMODE */
	(iw_handler) airo_set_sens,		/* SIOCSIWSENS */
	(iw_handler) airo_get_sens,		/* SIOCGIWSENS */
	(iw_handler) NULL,			/* SIOCSIWRANGE */
	(iw_handler) airo_get_range,		/* SIOCGIWRANGE */
	(iw_handler) NULL,			/* SIOCSIWPRIV */
	(iw_handler) NULL,			/* SIOCGIWPRIV */
	(iw_handler) NULL,			/* SIOCSIWSTATS */
	(iw_handler) NULL,			/* SIOCGIWSTATS */
#if WIRELESS_EXT > 15
	iw_handler_set_spy,			/* SIOCSIWSPY */
	iw_handler_get_spy,			/* SIOCGIWSPY */
	iw_handler_set_thrspy,			/* SIOCSIWTHRSPY */
	iw_handler_get_thrspy,			/* SIOCGIWTHRSPY */
#else /* WIRELESS_EXT > 15 */
#ifdef WIRELESS_SPY
	(iw_handler) airo_set_spy,		/* SIOCSIWSPY */
	(iw_handler) airo_get_spy,		/* SIOCGIWSPY */
#else	/* WIRELESS_SPY */
	(iw_handler) NULL,			/* SIOCSIWSPY */
	(iw_handler) NULL,			/* SIOCGIWSPY */
#endif	/* WIRELESS_SPY */
	(iw_handler) NULL,			/* -- hole -- */
	(iw_handler) NULL,			/* -- hole -- */
#endif /* WIRELESS_EXT > 15 */
	(iw_handler) airo_set_wap,		/* SIOCSIWAP */
	(iw_handler) airo_get_wap,		/* SIOCGIWAP */
	(iw_handler) NULL,			/* -- hole -- */
	(iw_handler) airo_get_aplist,		/* SIOCGIWAPLIST */
#if WIRELESS_EXT > 13
	(iw_handler) airo_set_scan,		/* SIOCSIWSCAN */
	(iw_handler) airo_get_scan,		/* SIOCGIWSCAN */
#else	/* WIRELESS_EXT > 13 */
	(iw_handler) NULL,			/* SIOCSIWSCAN */
	(iw_handler) NULL,			/* SIOCGIWSCAN */
#endif	/* WIRELESS_EXT > 13 */
	(iw_handler) airo_set_essid,		/* SIOCSIWESSID */
	(iw_handler) airo_get_essid,		/* SIOCGIWESSID */
	(iw_handler) airo_set_nick,		/* SIOCSIWNICKN */
	(iw_handler) airo_get_nick,		/* SIOCGIWNICKN */
	(iw_handler) NULL,			/* -- hole -- */
	(iw_handler) NULL,			/* -- hole -- */
	(iw_handler) airo_set_rate,		/* SIOCSIWRATE */
	(iw_handler) airo_get_rate,		/* SIOCGIWRATE */
	(iw_handler) airo_set_rts,		/* SIOCSIWRTS */
	(iw_handler) airo_get_rts,		/* SIOCGIWRTS */
	(iw_handler) airo_set_frag,		/* SIOCSIWFRAG */
	(iw_handler) airo_get_frag,		/* SIOCGIWFRAG */
	(iw_handler) airo_set_txpow,		/* SIOCSIWTXPOW */
	(iw_handler) airo_get_txpow,		/* SIOCGIWTXPOW */
	(iw_handler) airo_set_retry,		/* SIOCSIWRETRY */
	(iw_handler) airo_get_retry,		/* SIOCGIWRETRY */
	(iw_handler) airo_set_encode,		/* SIOCSIWENCODE */
	(iw_handler) airo_get_encode,		/* SIOCGIWENCODE */
	(iw_handler) airo_set_power,		/* SIOCSIWPOWER */
	(iw_handler) airo_get_power,		/* SIOCGIWPOWER */
};

/* Note : don't describe AIROIDIFC and AIROOLDIDIFC in here.
 * We want to force the use of the ioctl code, because those can't be
 * won't work the iw_handler code (because they simultaneously read
 * and write data and iw_handler can't do that).
 * Note that it's perfectly legal to read/write on a single ioctl command,
 * you just can't use iwpriv and need to force it via the ioctl handler.
 * Jean II */
static const iw_handler		airo_private_handler[] =
{
	NULL,				/* SIOCIWFIRSTPRIV */
};

static const struct iw_handler_def	airo_handler_def =
{
	.num_standard	= sizeof(airo_handler)/sizeof(iw_handler),
	.num_private	= sizeof(airo_private_handler)/sizeof(iw_handler),
	.num_private_args = sizeof(airo_private_args)/sizeof(struct iw_priv_args),
	.standard	= (iw_handler *) airo_handler,
	.private	= (iw_handler *) airo_private_handler,
	.private_args	= (struct iw_priv_args *) airo_private_args,
#if WIRELESS_EXT > 15
	.spy_offset	= ((void *) (&((struct airo_info *) NULL)->spy_data) -
			   (void *) NULL),
#endif /* WIRELESS_EXT > 15 */

};

#endif /* WIRELESS_EXT > 12 */
#endif /* WIRELESS_EXT */

/*
 * This defines the configuration part of the Wireless Extensions
 * Note : irq and spinlock protection will occur in the subroutines
 *
 * TODO :
 *	o Check input value more carefully and fill correct values in range
 *	o Test and shakeout the bugs (if any)
 *
 * Jean II
 *
 * Javier Achirica did a great job of merging code from the unnamed CISCO
 * developer that added support for flashing the card.
 */
static int airo_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	int rc = 0;
#if defined(WIRELESS_EXT) && WIRELESS_EXT < 13
	struct iwreq *wrq = (struct iwreq *) rq;
#endif /* WIRELESS_EXT < 13 */

	switch (cmd) {
/* WE 13 and higher will use airo_handler_def */
#if defined(WIRELESS_EXT) && WIRELESS_EXT < 13
	case SIOCGIWNAME:	// Get name
		airo_get_name(dev, NULL, (char *) &(wrq->u.name), NULL);
		break;

	case SIOCSIWFREQ:	// Set frequency/channel
		rc = airo_set_freq(dev, NULL, &(wrq->u.freq), NULL);
		break;

	case SIOCGIWFREQ:	// Get frequency/channel
		rc = airo_get_freq(dev, NULL, &(wrq->u.freq), NULL);
		break;

	case SIOCSIWESSID:	// Set desired network name (ESSID)
		{
			char essidbuf[IW_ESSID_MAX_SIZE+1];
			if (wrq->u.essid.length > IW_ESSID_MAX_SIZE) {
				rc = -E2BIG;
				break;
			}
			if (copy_from_user(essidbuf, wrq->u.essid.pointer,
					   wrq->u.essid.length)) {
				rc = -EFAULT;
				break;
			}
			rc = airo_set_essid(dev, NULL,
					    &(wrq->u.essid), essidbuf);
		}
		break;

	case SIOCGIWESSID:	// Get current network name (ESSID)
		{
			char essidbuf[IW_ESSID_MAX_SIZE+1];
			if (wrq->u.essid.pointer)
				rc = airo_get_essid(dev, NULL,
						    &(wrq->u.essid), essidbuf);
				if ( copy_to_user(wrq->u.essid.pointer,
						  essidbuf,
						  wrq->u.essid.length) )
					rc = -EFAULT;
		}
		break;

	case SIOCSIWAP:
		rc = airo_set_wap(dev, NULL, &(wrq->u.ap_addr), NULL);
		break;

	case SIOCGIWAP:		// Get current Access Point (BSSID)
		rc = airo_get_wap(dev, NULL, &(wrq->u.ap_addr), NULL);
		break;

	case SIOCSIWNICKN:	// Set desired station name
		{
			char nickbuf[IW_ESSID_MAX_SIZE+1];
			if (wrq->u.data.length > IW_ESSID_MAX_SIZE) {
				rc = -E2BIG;
				break;
			}
			if (copy_from_user(nickbuf, wrq->u.data.pointer,
					   wrq->u.data.length)) {
				rc = -EFAULT;
				break;
			}
			rc = airo_set_nick(dev, NULL,
					   &(wrq->u.data), nickbuf);
		}
		break;

	case SIOCGIWNICKN:	// Get current station name
		{
			char nickbuf[IW_ESSID_MAX_SIZE+1];
			if (wrq->u.data.pointer)
				rc = airo_get_nick(dev, NULL,
						   &(wrq->u.data), nickbuf);
				if ( copy_to_user(wrq->u.data.pointer,
						  nickbuf,
						  wrq->u.data.length) )
					rc = -EFAULT;
		}
		break;

	case SIOCSIWRATE:	// Set the desired bit-rate
		rc = airo_set_rate(dev, NULL, &(wrq->u.bitrate), NULL);
		break;

	case SIOCGIWRATE:	// Get the current bit-rate
		rc = airo_get_rate(dev, NULL, &(wrq->u.bitrate), NULL);
		break;

	case SIOCSIWRTS:	// Set the desired RTS threshold
		rc = airo_set_rts(dev, NULL, &(wrq->u.rts), NULL);
		break;

	case SIOCGIWRTS:	// Get the current RTS threshold
		rc = airo_get_rts(dev, NULL, &(wrq->u.rts), NULL);
		break;

	case SIOCSIWFRAG:	// Set the desired fragmentation threshold
		rc = airo_set_frag(dev, NULL, &(wrq->u.frag), NULL);
		break;

	case SIOCGIWFRAG:	// Get the current fragmentation threshold
		rc = airo_get_frag(dev, NULL, &(wrq->u.frag), NULL);
		break;

	case SIOCSIWMODE:	// Set mode of operation
		rc = airo_set_mode(dev, NULL, &(wrq->u.mode), NULL);
		break;

	case SIOCGIWMODE:	// Get mode of operation
		rc = airo_get_mode(dev, NULL, &(wrq->u.mode), NULL);
		break;

	case SIOCSIWENCODE:	// Set WEP keys and mode
		{
			char keybuf[MAX_KEY_SIZE];
			if (wrq->u.encoding.pointer) {
				/* We actually have a key to set */
				if (wrq->u.encoding.length > MAX_KEY_SIZE) {
					rc = -E2BIG;
					break;
				}
				if (copy_from_user(keybuf,
						   wrq->u.encoding.pointer,
						   wrq->u.encoding.length)) {
					rc = -EFAULT;
					break;
				}
			} else if (wrq->u.encoding.length != 0) {
				rc = -EINVAL;
				break;
			}
			rc = airo_set_encode(dev, NULL,
					     &(wrq->u.encoding), keybuf);
		}
		break;

	case SIOCGIWENCODE:	// Get the WEP keys and mode
		// Only super-user can see WEP key
		// Note : this is needed only for very old versions of WE
		if (!capable(CAP_NET_ADMIN)) {
			rc = -EPERM;
			break;
		}
		{
			char keybuf[MAX_KEY_SIZE];
			rc = airo_get_encode(dev, NULL,
					     &(wrq->u.encoding), keybuf);
			if (wrq->u.encoding.pointer) {
				if (copy_to_user(wrq->u.encoding.pointer,
						 keybuf,
						 wrq->u.encoding.length))
					rc = -EFAULT;
			}
		}
		break;

	case SIOCGIWTXPOW:	// Get the current Tx-Power
		rc=airo_get_txpow(dev, NULL, &(wrq->u.txpower), NULL);
		break;
	case SIOCSIWTXPOW:
		rc=airo_set_txpow(dev, NULL, &(wrq->u.txpower), NULL);
		break;

	case SIOCSIWRETRY:
		rc=airo_set_retry(dev, NULL, &(wrq->u.retry), NULL);
		break;
	case SIOCGIWRETRY:
		rc=airo_get_retry(dev, NULL, &(wrq->u.retry), NULL);
		break;

	case SIOCGIWRANGE:	// Get range of parameters
		{
			struct iw_range range;
			rc = airo_get_range(dev, NULL,
					    &(wrq->u.data), (char *) &range);
			if (copy_to_user(wrq->u.data.pointer, &range,
					 sizeof(struct iw_range)))
				rc = -EFAULT;
		}
		break;

	case SIOCGIWPOWER:
		rc=airo_get_power(dev, NULL, &(wrq->u.power), NULL);
		break;

	case SIOCSIWPOWER:
		rc=airo_set_power(dev, NULL, &(wrq->u.power), NULL);
		break;

	case SIOCGIWSENS:
		rc = airo_get_sens(dev, NULL, &(wrq->u.sens), NULL);
		break;

	case SIOCSIWSENS:
		rc = airo_set_sens(dev, NULL, &(wrq->u.sens), NULL);
		break;

	case SIOCGIWAPLIST:
		{
			char buffer[IW_MAX_AP * (sizeof(struct sockaddr) +
						  sizeof(struct iw_quality))];
			if (wrq->u.data.pointer) {
				rc = airo_get_aplist(dev, NULL,
						     &(wrq->u.data), buffer);
				if (copy_to_user(wrq->u.data.pointer,
						 buffer,
						 (wrq->u.data.length *
						  (sizeof(struct sockaddr) +
						   sizeof(struct iw_quality)))
						 ))
					rc = -EFAULT;
			}
		}
		break;

#ifdef WIRELESS_SPY
	case SIOCSIWSPY:	// Set the spy list
		{
			struct sockaddr address[IW_MAX_SPY];
			/* Check the number of addresses */
			if (wrq->u.data.length > IW_MAX_SPY) {
				rc = -E2BIG;
				break;
			}
			/* Get the data in the driver */
			if (wrq->u.data.pointer) {
				if (copy_from_user((char *) address,
						   wrq->u.data.pointer,
						   sizeof(struct sockaddr) *
						   wrq->u.data.length)) {
				rc = -EFAULT;
				break;
				}
			} else if (wrq->u.data.length != 0) {
				rc = -EINVAL;
				break;
			}
			rc=airo_set_spy(dev, NULL, &(wrq->u.data),
					(char *) address);
		}
		break;

	case SIOCGIWSPY:	// Get the spy list
		{
			char buffer[IW_MAX_SPY * (sizeof(struct sockaddr) +
						  sizeof(struct iw_quality))];
			if (wrq->u.data.pointer) {
				rc = airo_get_spy(dev, NULL,
						  &(wrq->u.data), buffer);
				if (copy_to_user(wrq->u.data.pointer,
						 buffer,
						 (wrq->u.data.length *
						  (sizeof(struct sockaddr) +
						   sizeof(struct iw_quality)))
						 ))
					rc = -EFAULT;
			}
		}
		break;
#endif /* WIRELESS_SPY */

#ifdef CISCO_EXT
	case SIOCGIWPRIV:
		if(wrq->u.data.pointer) {
			/* Set the number of ioctl available */
			wrq->u.data.length = sizeof(airo_private_args) / sizeof( airo_private_args[0]);

			/* Copy structure to the user buffer */
			if(copy_to_user(wrq->u.data.pointer,
					(u_char *) airo_private_args,
					sizeof(airo_private_args)))
				rc = -EFAULT;
		}
		break;
#endif /* CISCO_EXT */
#endif /* WIRELESS_EXT < 13 */

#ifdef CISCO_EXT
	case AIROIDIFC:
#ifdef AIROOLDIDIFC
	case AIROOLDIDIFC:
#endif
	{
		int val = AIROMAGIC;
		aironet_ioctl com;
		if (copy_from_user(&com,rq->ifr_data,sizeof(com)))
			rc = -EFAULT;
		else if (copy_to_user(com.data,(char *)&val,sizeof(val)))
			rc = -EFAULT;
	}
	break;

	case AIROIOCTL:
#ifdef AIROOLDIOCTL
	case AIROOLDIOCTL:
#endif
		/* Get the command struct and hand it off for evaluation by
		 * the proper subfunction
		 */
	{
		aironet_ioctl com;
		if (copy_from_user(&com,rq->ifr_data,sizeof(com))) {
			rc = -EFAULT;
			break;
		}

		/* Separate R/W functions bracket legality here
		 */
		if ( com.command <= AIRORRID )
			rc = readrids(dev,&com);
		else if ( com.command >= AIROPCAP && com.command <= AIROPLEAPUSR )
			rc = writerids(dev,&com);
		else if ( com.command >= AIROFLSHRST && com.command <= AIRORESTART )
			rc = flashcard(dev,&com);
		else
			rc = -EINVAL;      /* Bad command in ioctl */
	}
	break;
#endif /* CISCO_EXT */

	// All other calls are currently unsupported
	default:
		rc = -EOPNOTSUPP;
	}

#if defined(WIRELESS_EXT) && WIRELESS_EXT < 13
	/* WE 13 and higher will use airo_config_commit */
	/* Some of the "SET" function may have modified some of the
	 * parameters. It's now time to commit them in the card */
	airo_config_commit(dev, NULL, NULL, NULL);
	if (rc == -EINPROGRESS)
		return 0;
#endif /* WIRELESS_EXT < 13 */

	return rc;
}

#ifdef WIRELESS_EXT
/*
 * Get the Wireless stats out of the driver
 * Note : irq and spinlock protection will occur in the subroutines
 *
 * TODO :
 *	o Check if work in Ad-Hoc mode (otherwise, use SPY, as in wvlan_cs)
 *
 * Jean
 */
static void airo_read_wireless_stats(struct airo_info *local)
{
	StatusRid status_rid;
	StatsRid stats_rid;
	u32 *vals = stats_rid.vals;

	/* Get stats out of the card */
	clear_bit(JOB_WSTATS, &local->flags);
	readStatusRid(local, &status_rid, 0);
	readStatsRid(local, &stats_rid, RID_STATS, 0);
	up(&local->sem);

	/* The status */
	local->wstats.status = status_rid.mode;

	/* Signal quality and co. But where is the noise level ??? */
	local->wstats.qual.qual = status_rid.signalQuality;
	if (local->rssi)
		local->wstats.qual.level = 0x100 - local->rssi[status_rid.sigQuality].rssidBm;
	else
		local->wstats.qual.level = (status_rid.normalizedSignalStrength + 321) / 2;
	if (status_rid.len >= 124) {
		local->wstats.qual.noise = 256 - status_rid.noisedBm;
		local->wstats.qual.updated = 7;
	} else {
		local->wstats.qual.noise = 0;
		local->wstats.qual.updated = 3;
	}

	/* Packets discarded in the wireless adapter due to wireless
	 * specific problems */
	local->wstats.discard.nwid = vals[56] + vals[57] + vals[58];/* SSID Mismatch */
	local->wstats.discard.code = vals[6];/* RxWepErr */
	local->wstats.discard.fragment = vals[30];
	local->wstats.discard.retries = vals[10];
	local->wstats.discard.misc = vals[1] + vals[32];
	local->wstats.miss.beacon = vals[34];
}

struct iw_statistics *airo_get_wireless_stats(struct net_device *dev)
{
	struct airo_info *local =  dev->priv;

	/* Get stats out of the card if available */
	if (down_trylock(&local->sem) != 0) {
		set_bit(JOB_WSTATS, &local->flags);
		wake_up_interruptible(&local->thr_wait);
	} else
		airo_read_wireless_stats(local);

	return &local->wstats;
}
#endif /* WIRELESS_EXT */

#ifdef CISCO_EXT
#define RIDS_SIZE	2048
/*
 * This just translates from driver IOCTL codes to the command codes to
 * feed to the radio's host interface. Things can be added/deleted
 * as needed.  This represents the READ side of control I/O to
 * the card
 */
static int readrids(struct net_device *dev, aironet_ioctl *comp) {
	unsigned short ridcode;
	unsigned char *iobuf;
	int len;
	struct airo_info *ai = dev->priv;

	if (test_bit(FLAG_FLASHING, &ai->flags))
		return -EIO;

	switch(comp->command)
	{
	case AIROGCAP:      ridcode = RID_CAPABILITIES; break;
	case AIROGCFG: writeConfigRid (ai, 1);
			    ridcode = RID_CONFIG;       break;
	case AIROGSLIST:    ridcode = RID_SSID;         break;
	case AIROGVLIST:    ridcode = RID_APLIST;       break;
	case AIROGDRVNAM:   ridcode = RID_DRVNAME;      break;
	case AIROGEHTENC:   ridcode = RID_ETHERENCAP;   break;
	case AIROGWEPKTMP:  ridcode = RID_WEP_TEMP;
		/* Only super-user can read WEP keys */
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		break;
	case AIROGWEPKNV:   ridcode = RID_WEP_PERM;
		/* Only super-user can read WEP keys */
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		break;
	case AIROGSTAT:     ridcode = RID_STATUS;       break;
	case AIROGSTATSD32: ridcode = RID_STATSDELTA;   break;
	case AIROGSTATSC32: ridcode = RID_STATS;        break;
#ifdef MICSUPPORT
	case AIROGMICSTATS:
		if (copy_to_user(comp->data, &ai->micstats,
				 min((int)comp->len,(int)sizeof(ai->micstats))))
			return -EFAULT;
		return 0;
#endif
	case AIRORRID:      ridcode = comp->len;        break;
	default:
		return -EINVAL;
		break;
	}

	if ((iobuf = kmalloc(RIDS_SIZE, GFP_KERNEL)) == NULL)
		return -ENOMEM;

	PC4500_readrid(ai,ridcode,iobuf,RIDS_SIZE, 1);
	/* get the count of bytes in the rid  docs say 1st 2 bytes is it.
	 * then return it to the user
	 * 9/22/2000 Honor user given length
	 */
	if (comp->command == AIRORRID)
		len = le16_to_cpu(*(unsigned short *)iobuf); /* Yuck! */
	else
		len = comp->len;

	if (copy_to_user(comp->data, iobuf, min(len, (int)RIDS_SIZE))) {
		kfree (iobuf);
		return -EFAULT;
	}
	kfree (iobuf);
	return 0;
}

/*
 * Danger Will Robinson write the rids here
 */

static int writerids(struct net_device *dev, aironet_ioctl *comp) {
	struct airo_info *ai = dev->priv;
	int  ridcode, enabled;
	Resp      rsp;
	static int (* writer)(struct airo_info *, u16 rid, const void *, int, int);
	unsigned char *iobuf;

	/* Only super-user can write RIDs */
	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (test_bit(FLAG_FLASHING, &ai->flags))
		return -EIO;

	ridcode = 0;
	writer = do_writerid;

	switch(comp->command)
	{
	case AIROPSIDS:     ridcode = RID_SSID;         break;
	case AIROPCAP:      ridcode = RID_CAPABILITIES; break;
	case AIROPAPLIST:   ridcode = RID_APLIST;       break;
	case AIROPCFG: ai->config.len = 0;
			    ridcode = RID_CONFIG;       break;
	case AIROPWEPKEYNV: ridcode = RID_WEP_PERM;     break;
	case AIROPLEAPUSR:  ridcode = RID_LEAPUSERNAME; break;
	case AIROPLEAPPWD:  ridcode = RID_LEAPPASSWORD; break;
	case AIROPWEPKEY:   ridcode = RID_WEP_TEMP; writer = PC4500_writerid;
		break;

		/* this is not really a rid but a command given to the card
		 * same with MAC off
		 */
	case AIROPMACON:
		if (enable_MAC(ai, &rsp, 1) != 0)
			return -EIO;
		return 0;

		/*
		 * Evidently this code in the airo driver does not get a symbol
		 * as disable_MAC. it's probably so short the compiler does not gen one.
		 */
	case AIROPMACOFF:
		disable_MAC(ai, 1);
		return 0;

		/* This command merely clears the counts does not actually store any data
		 * only reads rid. But as it changes the cards state, I put it in the
		 * writerid routines.
		 */
	case AIROPSTCLR:
		if ((iobuf = kmalloc(RIDS_SIZE, GFP_KERNEL)) == NULL)
			return -ENOMEM;

		PC4500_readrid(ai,RID_STATSDELTACLEAR,iobuf,RIDS_SIZE, 1);

#ifdef MICSUPPORT
		enabled = ai->micstats.enabled;
		memset(&ai->micstats,0,sizeof(ai->micstats));
		ai->micstats.enabled = enabled;
#endif

		if (copy_to_user(comp->data, iobuf,
				 min((int)comp->len, (int)RIDS_SIZE))) {
			kfree (iobuf);
			return -EFAULT;
		}
		kfree (iobuf);
		return 0;

	default:
		return -EOPNOTSUPP;	/* Blarg! */
	}
	if(comp->len > RIDS_SIZE)
		return -EINVAL;

	if ((iobuf = kmalloc(RIDS_SIZE, GFP_KERNEL)) == NULL)
		return -ENOMEM;

	if (copy_from_user(iobuf,comp->data,comp->len)) {
		kfree (iobuf);
		return -EFAULT;
	}

	if (comp->command == AIROPCFG) {
		ConfigRid *cfg = (ConfigRid *)iobuf;

		if (test_bit(FLAG_MIC_CAPABLE, &ai->flags))
			cfg->opmode |= MODE_MIC;

		if ((cfg->opmode & 0xFF) == MODE_STA_IBSS)
			set_bit (FLAG_ADHOC, &ai->flags);
		else
			clear_bit (FLAG_ADHOC, &ai->flags);
	}

	if((*writer)(ai, ridcode, iobuf,comp->len,1)) {
		kfree (iobuf);
		return -EIO;
	}
	kfree (iobuf);
	return 0;
}

/*****************************************************************************
 * Ancillary flash / mod functions much black magic lurkes here              *
 *****************************************************************************
 */

/*
 * Flash command switch table
 */

int flashcard(struct net_device *dev, aironet_ioctl *comp) {
	int z;
	int cmdreset(struct airo_info *);
	int setflashmode(struct airo_info *);
	int flashgchar(struct airo_info *,int,int);
	int flashpchar(struct airo_info *,int,int);
	int flashputbuf(struct airo_info *);
	int flashrestart(struct airo_info *,struct net_device *);

	/* Only super-user can modify flash */
	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	switch(comp->command)
	{
	case AIROFLSHRST:
		return cmdreset((struct airo_info *)dev->priv);

	case AIROFLSHSTFL:
		if (!((struct airo_info *)dev->priv)->flash &&
			(((struct airo_info *)dev->priv)->flash = kmalloc (FLASHSIZE, GFP_KERNEL)) == NULL)
			return -ENOMEM;
		return setflashmode((struct airo_info *)dev->priv);

	case AIROFLSHGCHR: /* Get char from aux */
		if(comp->len != sizeof(int))
			return -EINVAL;
		if (copy_from_user(&z,comp->data,comp->len))
			return -EFAULT;
		return flashgchar((struct airo_info *)dev->priv,z,8000);

	case AIROFLSHPCHR: /* Send char to card. */
		if(comp->len != sizeof(int))
			return -EINVAL;
		if (copy_from_user(&z,comp->data,comp->len))
			return -EFAULT;
		return flashpchar((struct airo_info *)dev->priv,z,8000);

	case AIROFLPUTBUF: /* Send 32k to card */
		if (!((struct airo_info *)dev->priv)->flash)
			return -ENOMEM;
		if(comp->len > FLASHSIZE)
			return -EINVAL;
		if(copy_from_user(((struct airo_info *)dev->priv)->flash,comp->data,comp->len))
			return -EFAULT;

		flashputbuf((struct airo_info *)dev->priv);
		return 0;

	case AIRORESTART:
		if(flashrestart((struct airo_info *)dev->priv,dev))
			return -EIO;
		return 0;
	}
	return -EINVAL;
}

#define FLASH_COMMAND  0x7e7e

/*
 * STEP 1)
 * Disable MAC and do soft reset on
 * card.
 */

int cmdreset(struct airo_info *ai) {
	disable_MAC(ai, 1);

	if(!waitbusy (ai)){
		printk(KERN_INFO "Waitbusy hang before RESET\n");
		return -EBUSY;
	}

	OUT4500(ai,COMMAND,CMD_SOFTRESET);

	set_current_state (TASK_UNINTERRUPTIBLE);
	schedule_timeout (HZ);          /* WAS 600 12/7/00 */

	if(!waitbusy (ai)){
		printk(KERN_INFO "Waitbusy hang AFTER RESET\n");
		return -EBUSY;
	}
	return 0;
}

/* STEP 2)
 * Put the card in legendary flash
 * mode
 */

int setflashmode (struct airo_info *ai) {
	set_bit (FLAG_FLASHING, &ai->flags);

	OUT4500(ai, SWS0, FLASH_COMMAND);
	OUT4500(ai, SWS1, FLASH_COMMAND);
	if (probe) {
		OUT4500(ai, SWS0, FLASH_COMMAND);
		OUT4500(ai, COMMAND,0x10);
	} else {
		OUT4500(ai, SWS2, FLASH_COMMAND);
		OUT4500(ai, SWS3, FLASH_COMMAND);
		OUT4500(ai, COMMAND,0);
	}
	set_current_state (TASK_UNINTERRUPTIBLE);
	schedule_timeout (HZ/2); /* 500ms delay */

	if(!waitbusy(ai)) {
		clear_bit (FLAG_FLASHING, &ai->flags);
		printk(KERN_INFO "Waitbusy hang after setflash mode\n");
		return -EIO;
	}
	return 0;
}

/* Put character to SWS0 wait for dwelltime
 * x 50us for  echo .
 */

int flashpchar(struct airo_info *ai,int byte,int dwelltime) {
	int echo;
	int waittime;

	byte |= 0x8000;

	if(dwelltime == 0 )
		dwelltime = 200;

	waittime=dwelltime;

	/* Wait for busy bit d15 to go false indicating buffer empty */
	while ((IN4500 (ai, SWS0) & 0x8000) && waittime > 0) {
		udelay (50);
		waittime -= 50;
	}

	/* timeout for busy clear wait */
	if(waittime <= 0 ){
		printk(KERN_INFO "flash putchar busywait timeout! \n");
		return -EBUSY;
	}

	/* Port is clear now write byte and wait for it to echo back */
	do {
		OUT4500(ai,SWS0,byte);
		udelay(50);
		dwelltime -= 50;
		echo = IN4500(ai,SWS1);
	} while (dwelltime >= 0 && echo != byte);

	OUT4500(ai,SWS1,0);

	return (echo == byte) ? 0 : -EIO;
}

/*
 * Get a character from the card matching matchbyte
 * Step 3)
 */
int flashgchar(struct airo_info *ai,int matchbyte,int dwelltime){
	int           rchar;
	unsigned char rbyte=0;

	do {
		rchar = IN4500(ai,SWS1);

		if(dwelltime && !(0x8000 & rchar)){
			dwelltime -= 10;
			mdelay(10);
			continue;
		}
		rbyte = 0xff & rchar;

		if( (rbyte == matchbyte) && (0x8000 & rchar) ){
			OUT4500(ai,SWS1,0);
			return 0;
		}
		if( rbyte == 0x81 || rbyte == 0x82 || rbyte == 0x83 || rbyte == 0x1a || 0xffff == rchar)
			break;
		OUT4500(ai,SWS1,0);

	}while(dwelltime > 0);
	return -EIO;
}

/*
 * Transfer 32k of firmware data from user buffer to our buffer and
 * send to the card
 */

int flashputbuf(struct airo_info *ai){
	int            nwords;

	/* Write stuff */
	OUT4500(ai,AUXPAGE,0x100);
	OUT4500(ai,AUXOFF,0);

	for(nwords=0;nwords != FLASHSIZE / 2;nwords++){
		OUT4500(ai,AUXDATA,ai->flash[nwords] & 0xffff);
	}

	OUT4500(ai,SWS0,0x8000);

	return 0;
}

/*
 *
 */
int flashrestart(struct airo_info *ai,struct net_device *dev){
	int    i,status;

	set_current_state (TASK_UNINTERRUPTIBLE);
	schedule_timeout (HZ);          /* Added 12/7/00 */
	clear_bit (FLAG_FLASHING, &ai->flags);
	status = setup_card(ai, dev->dev_addr);

	for( i = 0; i < MAX_FIDS; i++ ) {
		ai->fids[i] = transmit_allocate( ai, 2312, i >= MAX_FIDS / 2 );
	}

	set_current_state (TASK_UNINTERRUPTIBLE);
	schedule_timeout (HZ);          /* Added 12/7/00 */
	return status;
}
#endif /* CISCO_EXT */

/*
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    In addition:

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. The name of the author may not be used to endorse or promote
       products derived from this software without specific prior written
       permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

module_init(airo_init_module);
module_exit(airo_cleanup_module);
