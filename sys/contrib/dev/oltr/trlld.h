/*
 ******************************  trlld.h  ***********************************
 *
 *                          Copyright (c) 1997
 *                          OLICOM A/S
 *                          Denmark
 *
 *                          All Rights Reserved
 *
 *      This source file is subject to the terms and conditions of the
 *      OLICOM Software License Agreement which restricts the manner
 *      in which it may be used.
 *
 *---------------------------------------------------------------------------
 *
 * Description: PowerMACH Works header file
 * $FreeBSD: src/sys/contrib/dev/oltr/trlld.h,v 1.4 2000/03/18 23:51:51 lile Exp $
 *
 *---------------------------------------------------------------------------
 * $Log:   J:/usr/project/trlld/libsrc/include/trlld.h_v  $
 * 
 *    Rev 1.9   25 Jan 1999 09:56:28   EGS
 * Added 3150
 * 
 *    Rev 1.8   10 Dec 1998 12:24:52   JHM
 * version 1.2.0,prominfo structure with shorts.
 * 
 *    Rev 1.7   25 Nov 1998 16:18:48   JHM
 * Bullseye mac, 100MBPS, mactype in config structure,
 * 3540 adapter, TRlldTransmitFree, TRlldReceiveFree,
 * TRlldAdapterName
 * 
 *    Rev 1.6   23 Oct 1998 16:00:36   JHM
 * hawkeye adapter types
 * 
 *    Rev 1.5   11 Aug 1998 12:22:06   JHM
 * split hawkeye types into PCI4,5,6
 * 
 *    Rev 1.4   10 Jul 1998 14:39:22   JHM
 * OC_3140,OC_3250
 * 
 *    Rev 1.3   18 Jun 1998 11:32:20   JHM
 * AddMemory,OC_3250
 * 
 *    Rev 1.2   18 Apr 1998 15:11:20   JHM
 * 
 *    Rev 1.1   09 Dec 1997 18:17:52   JHM
 * rel111: TRlldDataPtr_t
 * 
 *    Rev 1.0   24 Nov 1997 11:08:58   JHM
 * Initial revision.
   
      Rev 1.5   18 Jun 1997 11:31:36   JHM
   Checks for version
   
      Rev 1.4   13 Jun 1997 13:47:34   JHM
   
      Rev 1.3   13 Jun 1997 13:27:56   JHM
   DTR support, version change
   
      Rev 1.2   12 Jun 1997 11:43:20   JHM
   TRLLD_INTERRUPT_TIMEOUT defined
   
      Rev 1.1   11 Apr 1997 15:24:18   JHM
   replaced tabs with spaces
   
      Rev 1.0   11 Apr 1997 14:43:04   JHM
   Initial revision.
 *
 ****************************************************************************
*/

#ifndef TRLLD_H
#define TRLLD_H

/* Data buffer pointers are always 32 bits.
   For 16:16 it is segment:offset while it for 32:32 is a linear address. */

#ifdef TRlldSmall
#define TRlldDataPtr_t      unsigned char far *
#define TRlldWordDataPtr_t  unsigned short far *
#define TRlldDWordDataPtr_t	unsigned long far *
#else
#define TRlldDataPtr_t      unsigned char *
#define TRlldWordDataPtr_t  unsigned short *
#define TRlldDWordDataPtr_t	unsigned long *
#endif

#ifdef __WATCOMC__
#define CDECL    _cdecl
#else
#define CDECL
#endif

/*****************************************************************************/
/*                                                                           */
/* Firmware                                                                  */
/*                                                                           */
/*****************************************************************************/

extern unsigned char TRlldMacCode[];       /* 3115,17,18,29,33,36,37 */
extern unsigned char TRlldHawkeyeMac[];    /* 3139,3140,3141,3250    */
extern unsigned char TRlldBullseyeMac[];   /* 3150,3540              */

/*****************************************************************************/
/*                                                                           */
/* Maximal numbers of concurrent receive and transmit slots                  */
/*                                                                           */
/*****************************************************************************/

#define TRLLD_MAX_RECEIVE        32
#define TRLLD_MAX_TRANSMIT       32

/*****************************************************************************/
/*                                                                           */
/* Maximal frame sizes                                                       */
/*                                                                           */
/*****************************************************************************/

#define TRLLD_MAXFRAME_100MBPS 18000
#define TRLLD_MAXFRAME_16MBPS  18000
#define TRLLD_MAXFRAME_4MBPS    4500

/*****************************************************************************/
/*                                                                           */
/* TRlldStatus contains the adapter status used in a DriverStatus call-back. */
/*                                                                           */
/*****************************************************************************/

struct OnWireInformation {
	unsigned short RingStatus;
	unsigned short Speed;
	unsigned short AccessProtocol;
	unsigned short Reserved;
};

typedef struct TRlldStatus {
    unsigned short Type;
    unsigned char Closed;
    unsigned char AccessProtocol;
    unsigned short MaxFrameSize;
    unsigned short Reserved;
    union {
        unsigned short OnWireRingStatus;	/* for compability */
        unsigned short SelftestStatus;
        unsigned short InitStatus;
        unsigned short RingStatus;
        unsigned short AdapterCheck[4];
        unsigned short InternalError[4];
        unsigned short PromRemovedCause;
        unsigned short AdapterTimeout;
        struct OnWireInformation OnWireInformation;
    } Specification;
} TRlldStatus_t;

/* values of TRlldStatus.Type */

#define TRLLD_STS_ON_WIRE                0
#define TRLLD_STS_SELFTEST_STATUS        1
#define TRLLD_STS_INIT_STATUS            2
#define TRLLD_STS_RING_STATUS            3
#define TRLLD_STS_ADAPTER_CHECK          4
#define TRLLD_STS_PROMISCUOUS_STOPPED    5
#define TRLLD_STS_LLD_ERROR              6
#define TRLLD_STS_ADAPTER_TIMEOUT        7

/* values of TRlldStatus.Closed */

#define TRLLD_STS_STATUS_OK              0
#define TRLLD_STS_STATUS_CLOSED          1

/* values of TRlldStatus.AccessProtocol */

#define TRLLD_ACCESS_UNKNOWN             0
#define TRLLD_ACCESS_TKP                 1
#define TRLLD_ACCESS_TXI                 2

/* values of TRlldStatus.SelftestStatus */

#define TRLLD_ST_OK            0
#define TRLLD_ST_ERROR    0x0100       /* actual errors are 010x, where x is */
                                       /* 0: Initial Test Error              */
                                       /* 1: Adapter Software Checksum Error */
                                       /* 2: Adapter RAM Error               */
                                       /* 4: Instruction Test Error          */
                                       /* 5: Protocol Handler/RI Hw Error    */
                                       /* 6: System Interface Register Error */

#define TRLLD_ST_TIMEOUT  0x0200       /* The adapter did not complete */
                                       /* selftest after download      */

/* values of TRlldStatus.Specification.InitStatus */

/* the most likely cause of an init error (whatever the code) is a wrong */
/* physical or virtual address of the adapter block in TRlldAdapterInit  */

#define TRLLD_INIT_ERROR       0x100   /* actual errors are 010x, where x is */
                                       /* 1: Invalid init block (LLD error)  */
                                       /* 2: Invalid options (LLD error)     */
                                       /* 3: Invalid rcv burst (LLD error)   */
                                       /* 4: Invalid xmt burst (LLD error)   */
                                       /* 5: Invalid DMA threshold (LLDerror)*/
                                       /* 6: Invalid scb addr                */
                                       /* 7: Invalid ssb addr                */
                                       /* 8: DIO parity error (HW error)     */
                                       /* 9: DMA timeout (May be interrupt
                                             failing if PIO mode or PCI2)    */
                                       /* A: DMA parity error (HW error)     */
                                       /* B: DMA bus error (HW error)        */
                                       /* C: DMA data error                  */
                                       /* D: Adapter check                   */

#define TRLLD_INIT_TIMEOUT     0x200   /* adapter init did not complete      */
#define TRLLD_INIT_DMA_ERROR   0x300   /* adapter cannot access sys memory   */
#define TRLLD_INIT_INTR_ERROR  0x400   /* adapter cannot interrupt           */
#define TRLLD_OPEN_TIMEOUT     0x500   /* adapter open did not complete      */
                                       /* within 30 seconds                  */

#define TRLLD_OPEN_ERROR       0x600   /* actual errors are 06xx, where the  */
                                       /* bits in x mean:                    */
                                       /* 01: Invalid open options (LLDerror)*/
                                       /* 04: TxBuffer count error (LLDerror)*/
                                       /* 10: Buffer size error (LLD error)  */
                                       /* 20: List size error (LLD error)    */
                                       /* 40: Node address error             */

#define TRLLD_OPEN_REPEAT      0x700   /* actual errors are 07xy, where      */
                                       /* x is the open phase:               */
                                       /* 1: Lobe media test                 */
                                       /* 2: Physical Insertion              */
                                       /* 3: Address verification            */
                                       /* 4: Participation in ring poll      */
                                       /* 5: Request Initialization          */
                                       /* 9: Request registration (TXI)      */
                                       /* A: Lobe Media Test (TXI)           */
                                       /* B: Address verification (TXI)      */
                                       /* y is the type of error:            */
                                       /* 1: Function failure (No Cable ?)   */
                                       /* 2: Signal loss                     */
                                       /* 5: Timeout                         */
                                       /* 6: Ring failure (TKP)              */
                                       /* 6: Protocol error (TXI)            */
                                       /* 7: Ring beaconing                  */
                                       /* 8: Duplicate Node Address (TKP)    */
                                       /* 8: Insert Denied (TXI)             */
                                       /* 9: Request Initialization (TKP)    */
                                       /* 9: Heart beat failure (TXI)        */
                                       /* A: Remove received                 */
                                       /* B: C-port address changed (TXI)    */
                                       /* C: Wire Fault (TKP)                */
                                       /* D: Auto Speed, 1. on ring (TKP)    */
                                       /* E: Speed sense failed              */

/* When opening with FORCE_TXI and only classic token ring attachment is     */
/* possible, the error is Request Registration/Timeout or 0x795              */

#define TRLLD_OPEN_1ST_ON_RING	0x800  /* Speed sense is active, but no other*/
                                       /* station is present to set the speed*/


/* values of TRlldStatus.Specification.RingStatus */

#define TRLLD_RS_SIGNAL_LOSS        0x8000
#define TRLLD_RS_HARD_ERROR         0x4000
#define TRLLD_RS_SOFT_ERROR         0x2000
#define TRLLD_RS_TRANSMIT_BEACON    0x1000
#define TRLLD_RS_LOBE_WIRE_FAULT    0x0800
#define TRLLD_RS_AUTO_REMOVAL_ERROR 0x0400
#define TRLLD_RS_REMOVE_RECEIVED    0x0100
#define TRLLD_RS_COUNTER_OVERFLOW   0x0080
#define TRLLD_RS_SINGLE_STATION     0x0040
#define TRLLD_RS_RING_RECOVERY      0x0020

/* values of TRlldStatus.Specification.AdapterCheck */
/* MISSING */

/* values of TRlldStatus.Specification.PromRemovedCause */

#define TRLLD_PROM_REMOVE_RECEIVED    1
#define TRLLD_PROM_POLL_FAILURE       2
#define TRLLD_PROM_BUFFER_SIZE        3

/* values of TRlldStatus.Specification.InternalError */

#define TRLLD_INTERNAL_PIO   1        /* A PIO transfer to or from adapter  */
                                      /* did not complete                   */
#define TRLLD_INTERNAL_TX    2        /* Trouble with clean up of tx frames */
#define TRLLD_INTERNAL_RX    3        /* Trouble with clean up of receive   */
                                      /* fragments                          */
#define TRLLD_INTERNAL_CMD   4        /* error response from adapter        */
#define TRLLD_INTERNAL_STATE 5        /* event happened in unexpected state */

/* values of TRlldStatus.Specification.AdapterTimeout */

#define TRLLD_COMMAND_TIMEOUT    1
#define TRLLD_TRANSMIT_TIMEOUT   2
#define TRLLD_INTERRUPT_TIMEOUT  3


/*****************************************************************************/
/*                                                                           */
/* TRlldStatistics contains the adapter statistics returned to Driver        */
/* in TRlldStatistics calls and DriverStatistics call-backs                  */
/*                                                                           */
/*****************************************************************************/

typedef struct TRlldStatistics {
    unsigned long LineErrors;
    unsigned long InternalErrors;       /* Not maintained by TMS based boards */
    unsigned long BurstErrors;
    unsigned long ARIFCIErrors;
    unsigned long AbortDelimiters;      /* Not maintained by TMS based boards */
    unsigned long LostFrames;
    unsigned long CongestionErrors;
    unsigned long FrameCopiedErrors;
    unsigned long FrequencyErrors;      /* Not maintained by TMS based boards */
    unsigned long TokenErrors;
    unsigned long DMABusErrors;         /* Not maintained by 3139 */
    unsigned long DMAParityErrors;      /* Not maintained by 3139 */
    unsigned long ReceiveLongFrame;     /* Not maintained by TMS based boards */
    unsigned long ReceiveCRCErrors;     /* Not maintained by TMS based boards */
    unsigned long ReceiveOverflow;      /* Not maintained by TMS based boards */
    unsigned long TransmitUnderrun;     /* Not maintained by TMS based boards */
	unsigned long UnderrunLock;			/* Not maintained by TMS based boards */
	unsigned long OverflowReset;
    unsigned char UpstreamNeighbour[6];
    unsigned short RingNumber;
    unsigned char BeaconingUpstreamNeighbour[6];
    unsigned short padding;
} TRlldStatistics_t;


/*****************************************************************************/
/*                                                                           */
/* TRlldDriver contains the Driver call-backs                                */
/*                                                                           */
/*****************************************************************************/

typedef struct TRlldDriver {
    unsigned long TRlldVersion;
#ifndef TRlldInlineIO
    void (CDECL * DriverOutByte)(unsigned short IOAddress,
                                 unsigned char Value);
    void (CDECL * DriverOutWord)(unsigned short IOAddress,
                                 unsigned short Value);
    void (CDECL * DriverOutDWord)(unsigned short IOAddress,
                                  unsigned long Value);
    void (CDECL * DriverRepOutByte)(unsigned short IOAddress,
                                    TRlldDataPtr_t DataPointer,
                                    int ByteCount);
    void (CDECL * DriverRepOutWord)(unsigned short IOAddress,
                                    TRlldWordDataPtr_t DataPointer,
                                    int WordCount);
    void (CDECL * DriverRepOutDWord)(unsigned short IOAddress,
                                     TRlldDWordDataPtr_t DataPointer,
                                     int DWordCount);
    unsigned char (CDECL * DriverInByte)(unsigned short IOAddress);
    unsigned short (CDECL * DriverInWord)(unsigned short IOAddress);
    unsigned long (CDECL * DriverInDWord)(unsigned short IOAddress);
    void (CDECL * DriverRepInByte)(unsigned short IOAddress,
                                   TRlldDataPtr_t DataPointer,
                                   int ByteCount);
    void (CDECL * DriverRepInWord)(unsigned short IOAddress,
                                   TRlldWordDataPtr_t DataPointer,
                                   int WordCount);
    void (CDECL * DriverRepInDWord)(unsigned short IOAddress,
                                    TRlldDWordDataPtr_t DataPointer,
                                    int DWordCount);
#endif
    void (CDECL * DriverSuspend)(unsigned short MicroSeconds);
    void (CDECL * DriverStatus)(void * DriverHandle,
                                TRlldStatus_t * Status);
    void (CDECL * DriverCloseCmpltd)(void * DriverHandle);
    void (CDECL * DriverStatistics)(void * DriverHandle,
                                    TRlldStatistics_t * Statistics);
    void (CDECL * DriverTxFrameCmpltd)(void * DriverHandle,
                                       void * FrameHandle,
                                       int TxStatus);
    void (CDECL * DriverRcvFrameCmpltd)(void * DriverHandle,
                                        int ByteCount,
                                        int FragmentCount,
                                        void * FragmentHandle,
                                        int RcvStatus);
} TRlldDriver_t;

/* Version and model control */

#define TRLLD_VERSION_INLINEIO    0x8000
#define TRLLD_VERSION_SMALL       0x4000
#ifdef TRlldInlineIO
#ifdef TRlldSmall
#define TRLLD_VERSION    0x4120
#else
#define TRLLD_VERSION    0x0120
#endif
#else
#ifdef TRlldSmall
#define TRLLD_VERSION    0xC120
#else
#define TRLLD_VERSION    0x8120
#endif
#endif


/*****************************************************************************/
/*                                                                           */
/* TRlldAdapterConfig contains the properties found for an adapter           */
/* used when finding and defining adapters to use                            */
/*                                                                           */
/*****************************************************************************/


struct pnp_id {
    unsigned short vendor;
    unsigned short device;
};

struct pci_id {
    unsigned short vendor;
    unsigned short device;
    unsigned char revision;
    unsigned char reserved_byte;
    unsigned short reserved_word;
};

struct pcmcia_id {
    /* unknown as yet */
    unsigned char x;
};

struct pci_slot {
    unsigned short bus_no;
    unsigned short device_no;
};

struct pcmcia_socket {
    /* unknown as yet */
    unsigned char x;
};

typedef struct TRlldAdapterConfig {
    unsigned char type;
    unsigned char bus;
    unsigned short magic;
    union {
        struct pnp_id pnp;
        unsigned long eisa;
        unsigned short mca;
        struct pci_id pci;
        struct pcmcia_id pcmcia;
    } id;
    union {
        unsigned short csn;
        unsigned short eisa;
        unsigned short mca;
        struct pci_slot pci;
        struct pcmcia_socket pcmcia;
    } slot;
    unsigned short iobase0;
    unsigned short iolength0;
    unsigned short iobase1;
    unsigned short iolength1;
    unsigned long memorybase;
    unsigned short memorylength;
    unsigned char mode;
    unsigned char xmode;
    unsigned char interruptlevel;
    unsigned char dmalevel;
    unsigned char macaddress[6];
    unsigned long prombase;
    unsigned char speed;
    unsigned char cachelinesize;
    unsigned short pcicommand;
	unsigned char mactype;
	unsigned char reserved[3];
} TRlldAdapterConfig_t;

/* values of TRlldAdapterConfig.Type */

#define TRLLD_ADAPTER_XT            0        /* not supported             */
#define TRLLD_ADAPTER_ISA1          1        /* OC-3115                   */
#define TRLLD_ADAPTER_ISA2          2        /* OC-3117                   */
#define TRLLD_ADAPTER_ISA3          3        /* OC-3118                   */
#define TRLLD_ADAPTER_MCA1          4        /* OC-3129 id A84            */
#define TRLLD_ADAPTER_MCA2          5        /* OC-3129 id A85            */
#define TRLLD_ADAPTER_MCA3          6        /* OC-3129 id A86            */
#define TRLLD_ADAPTER_EISA1         7        /* OC-3133 id 0109833D       */
#define TRLLD_ADAPTER_EISA2         8        /* OC-3133 id 0209833D       */
#define TRLLD_ADAPTER_EISA3         9        /* OC-3135 not supported     */
#define TRLLD_ADAPTER_PCI1         10        /* OC-3136 id 108d0001 rev 1 */
#define TRLLD_ADAPTER_PCI2         11        /* OC-3136 id 108d0001 rev 2 */
#define TRLLD_ADAPTER_PCI3         12        /* OC-3137 id 108d0001 rev 3 */
#define TRLLD_ADAPTER_PCI4         13        /* OC-3139 id 108d0004 rev 2 */
#define TRLLD_ADAPTER_PCI5         14        /* OC-3140 id 108d0004 rev 3 */
#define TRLLD_ADAPTER_PCI6         15        /* OC-3141 id 108d0007 rev 1 */
#define TRLLD_ADAPTER_PCI7         19        /* OC-3540 id 108d0008 rev 1 */
#define TRLLD_ADAPTER_PCI8         20        /* OC-3150 id 108d000a rev 1 */
#ifdef PCMCIA
#define TRLLD_ADAPTER_PCCARD1      16        /* OC-3220                   */
#define TRLLD_ADAPTER_PCCARD2      17        /* OC-3221,OC-3230,OC-3232   */
#endif
#define TRLLD_ADAPTER_PCCARD3      18        /* OC-3250 id 108d0005 rev 1 */

/* values of TRlldAdapterConfig.Bus */

#define TRLLD_BUS_ISA           1
#define TRLLD_BUS_EISA          2
#define TRLLD_BUS_MCA           3
#define TRLLD_BUS_PCI           4
#define TRLLD_BUS_PCMCIA        5

/* values of TRlldAdapterConfig.mode */

#define TRLLD_MODE_16M             0x01  /* needs data buffers below 16 M     */
#define TRLLD_MODE_PHYSICAL        0x02  /* needs valid physical addresses    */
#define TRLLD_MODE_FIXED_CFG       0x04  /* cannot be reconfigured            */
#define TRLLD_MODE_SHORT_SLOT      0x08  /* in short ISA slot, cannot use DMA */
#define TRLLD_MODE_CANNOT_DISABLE  0x10  /* can not disable interrupt         */
#define TRLLD_MODE_SHARE_INTERRUPT 0x20  /* may share interrupt               */
#define TRLLD_MODE_MEMORY          0x40  /* is configured with a memory window*/

/* values of TRlldAdapterConfig.dma */

#define TRLLD_DMA_PIO      4       /* other values signifies the DMA channel */
#define TRLLD_DMA_MASTER   0xff    /* to use                                 */

/* values of TRlldAdapterConfig.mactype */

                                   /* download with:   */
#define TRLLD_MAC_TMS       1      /* TRlldMACCode     */
#define TRLLD_MAC_HAWKEYE   2      /* TRlldHawkeyeMAC  */
#define TRLLD_MAC_BULLSEYE  3      /* TRlldBullseyeMAC */


typedef void * TRlldAdapter_t;
typedef void * TRlldAdapterType_t;

#ifndef MAX_FRAGMENTS
#define MAX_FRAGMENTS 32
#endif

typedef struct TRlldTransmit {
    unsigned short FragmentCount;
    unsigned short TRlldTransmitReserved;
    struct TRlldTransmitFragment {
        unsigned long PhysicalAddress;
        TRlldDataPtr_t VirtualAddress;
        unsigned short count;
        unsigned short TRlldTransmitFragmentReserved;
    } TransmitFragment[MAX_FRAGMENTS];
} TRlldTransmit_t;

int CDECL TRlldAdapterSize(void);

int CDECL TRlldInit(int TypeCount,
                    TRlldAdapterType_t * AdapterTypeTable);

extern TRlldAdapterType_t CDECL TRlld3115;    /* ISA adapters */
extern TRlldAdapterType_t CDECL TRlld3117;
extern TRlldAdapterType_t CDECL TRlld3118;
extern TRlldAdapterType_t CDECL TRlld3129;    /* MCA adapters */
extern TRlldAdapterType_t CDECL TRlld3133;    /* EISA adapters */
extern TRlldAdapterType_t CDECL TRlld3136;    /* PCI adapters */
extern TRlldAdapterType_t CDECL TRlld3137;
extern TRlldAdapterType_t CDECL TRlld3139;    /* Hawkeye adapters */
extern TRlldAdapterType_t CDECL TRlld3540;    /* Bullseye adapters */

#define T3115   &TRlld3115
#define T3117   &TRlld3117
#define T3118   &TRlld3118
#define T3129   &TRlld3129
#define T3133   &TRlld3133
#define T3136   &TRlld3136
#define T3137   &TRlld3137
#define T3139   &TRlld3139
#define T3540   &TRlld3540

/* Only for Boot Prom Page Zero code */

extern TRlldAdapterType_t CDECL TRlld3115Boot;    /* ISA adapters */
extern TRlldAdapterType_t CDECL TRlld3117Boot;
extern TRlldAdapterType_t CDECL TRlld3118Boot;
extern TRlldAdapterType_t CDECL TRlld3129Boot;    /* MCA adapters */
extern TRlldAdapterType_t CDECL TRlld3133Boot;    /* EISA adapters */
extern TRlldAdapterType_t CDECL TRlld3136Boot;    /* PCI adapters */
extern TRlldAdapterType_t CDECL TRlld3137Boot;
extern TRlldAdapterType_t CDECL TRlld3139Boot;    /* Hawkeye adapters */
extern TRlldAdapterType_t CDECL TRlld3150Boot;
extern TRlldAdapterType_t CDECL TRlld3250Boot;
extern TRlldAdapterType_t CDECL TRlld3540Boot;    /* Bullseye adapter */

#define B3115   &TRlld3115Boot
#define B3117   &TRlld3117Boot
#define B3118   &TRlld3118Boot
#define B3129   &TRlld3129Boot
#define B3133   &TRlld3133Boot
#define B3136   &TRlld3136Boot
#define B3137   &TRlld3137Boot
#define B3139   &TRlld3139Boot
#define B3150   &TRlld3150Boot
#define B3250   &TRlld3250Boot
#define B3540   &TRlld3540Boot

#define TRLLD_INIT_OK           0
#define TRLLD_INIT_UNKNOWN      5

int CDECL TRlldAdapterInit(TRlldDriver_t * DriverDefinition,
                           TRlldAdapter_t * TRlldAdapter,
                           unsigned long TRlldAdapterPhysical,
                           void * DriverHandle,
                           TRlldAdapterConfig_t * config);

#define TRLLD_INIT_OK           0
#define TRLLD_INIT_NOT_FOUND    1
#define TRLLD_INIT_UNSUPPORTED  2
#define TRLLD_INIT_PHYS16       3
#define TRLLD_INIT_VERSION      4

int CDECL TRlldSetSpeed(TRlldAdapter_t * adapter,
                        unsigned char speed);

#define TRLLD_SPEED_4MBPS      4
#define TRLLD_SPEED_16MBPS    16
#define TRLLD_SPEED_100MBPS  100

int CDECL TRlldSetInterrupt(TRlldAdapter_t * adapter,
                            unsigned char interruptlevel);

int CDECL TRlldSetDMA(TRlldAdapter_t * adapter,
                      unsigned char dma, unsigned char * mode);

#define TRLLD_CONFIG_OK       0
#define TRLLD_CONFIG_STATE    1
#define TRLLD_CONFIG_ILLEGAL  2
#define TRLLD_CONFIG_FAILED   3

int CDECL TRlldSetSpecial(TRlldAdapter_t * adapter,
                          unsigned short param1, unsigned short param2,
                          unsigned short param3, unsigned short param4);

int CDECL TRlldAddMemory(TRlldAdapter_t * adapter,
                         TRlldDataPtr_t virtual,
                         unsigned long physical,
                         long size);

int CDECL TRlldDisable(TRlldAdapter_t * adapter);

#define TRLLD_OK             0
#define TRLLD_NOT_SUPPORTED  1

void CDECL TRlldEnable(TRlldAdapter_t * adapter);

int CDECL TRlldInterruptPresent(TRlldAdapter_t * adapter);

#define TRLLD_NO_INTERRUPT  0
#define TRLLD_INTERRUPT     1

int CDECL TRlldInterruptService(TRlldAdapter_t * adapter);

int CDECL TRlldInterruptPreService(TRlldAdapter_t * adapter);

void CDECL TRlldInterruptPostService(TRlldAdapter_t * adapter);


int CDECL TRlldPoll(TRlldAdapter_t * adapter);

int CDECL TRlldDownload(TRlldAdapter_t * adapter,
                        char * DownLoadCode);

#define TRLLD_DOWNLOAD_OK     0
#define TRLLD_DOWNLOAD_ERROR  1
#define TRLLD_STATE           2

typedef int (CDECL * GetCode_t)(void * handle, unsigned char * maccodebyte);

int CDECL TRlldStreamDownload(TRlldAdapter_t * adapter,
                              GetCode_t procedure, void * handle);

int CDECL TRlldOpen(TRlldAdapter_t * adapter,
                    unsigned char * MACAddress,
                    unsigned long GroupAddress,
                    unsigned long FunctionalAddress,
                    unsigned short MaxFrameSize,
                    unsigned short OpenModes);

#define TRLLD_OPEN_OK             0
#define TRLLD_OPEN_STATE          1
#define TRLLD_OPEN_ADDRESS_ERROR  2
#define TRLLD_OPEN_MODE_ERROR     3
#define TRLLD_OPEN_MEMORY         4

#define TRLLD_MODE_TX_STATUS      0x01
#define TRLLD_MODE_RX_SINGLE      0x02
#define TRLLD_MODE_FORCE_TKP      0x04
#define TRLLD_MODE_FORCE_TXI      0x08
#define TRLLD_MODE_TX_CRC         0x10

void CDECL TRlldClose(TRlldAdapter_t * adapter, int immediate);

void CDECL TRlldSetGroupAddress(TRlldAdapter_t * adapter,
                                unsigned long GroupAddress);

void CDECL TRlldSetFunctionalAddress(TRlldAdapter_t * adapter,
                                     unsigned long FunctionalAddress);

void CDECL TRlldSetPromiscuousMode(TRlldAdapter_t * adapter,
                                   unsigned char mode);

/* mode bits */

#define TRLLD_PROM_LLC          1
#define TRLLD_PROM_MAC          2
#define TRLLD_PROM_ERRORFRAMES  4

int CDECL TRlldGetStatistics(TRlldAdapter_t * adapter,
                             TRlldStatistics_t * statistics,
                             int immediate);

#define TRLLD_IMMEDIATE_STATISTICS  1

#define TRLLD_STATISTICS_RETRIEVED  0
#define TRLLD_STATISTICS_PENDING    1

int CDECL TRlldTransmitFrame(TRlldAdapter_t * adapter,
                             TRlldTransmit_t * TransmitFrame,
                             void * FrameHandle);

#define TRLLD_TRANSMIT_OK        0
#define TRLLD_TRANSMIT_NOT_OPEN  1
#define TRLLD_TRANSMIT_TOO_MANY  2
#define TRLLD_TRANSMIT_MAX16     3
#define TRLLD_TRANSMIT_SIZE      4
#define TRLLD_TRANSMIT_EMPTY     5

/* completion flags */

#define TRLLD_TX_OK              0
#define TRLLD_TX_NOT_PROCESSED   1
#define TRLLD_TX_NOT_RECOGNIZED  2
#define TRLLD_TX_NOT_COPIED      3

/* number of free transmit fragments */

int CDECL TRlldTransmitFree(TRlldAdapter_t * adapter);

int CDECL TRlldReceiveFragment(TRlldAdapter_t * adapter,
                               TRlldDataPtr_t FragmentStart,
                               unsigned long FragmentPhysical,
                               int count,
                               void * FragmentHandle);

#define TRLLD_RECEIVE_OK        0
#define TRLLD_RECEIVE_NOT_OPEN  1
#define TRLLD_RECEIVE_TOO_MANY  2
#define TRLLD_RECEIVE_SIZE      3
#define TRLLD_RECEIVE_MAX16     4

/* completion flags */

#define TRLLD_RCV_OK       0
#define TRLLD_RCV_NO_DATA  1
#define TRLLD_RCV_ERROR    2    /* Only when TRLLD_PROM_ERRORFRAMES */
#define TRLLD_RCV_LONG     3

/* number of free receive fragments */

int CDECL TRlldReceiveFree(TRlldAdapter_t * adapter);

int CDECL TRlldFind(TRlldDriver_t * driver,
                    TRlldAdapterConfig_t * config_table,
                    unsigned long type_mask,
                    int max);

/* type mask bits */

#define OC_3115            0x0001
#define OC_3117            0x0002
#define OC_3118            0x0004
#define OC_3129            0x0008
#define OC_3133            0x0010
#define OC_3136            0x0040
#define OC_3137            0x0080
#define OC_3139            0x0100
#define OC_3140            0x0200
#define OC_3141            0x0400
#define OC_3540            0x0800
#define OC_3150            0x1000

#ifdef PCMCIA
#define OC_3220            0x0800
#define OC_3221            0x1000
#define OC_3230            0x2000
#define OC_3232            0x4000
#endif

#define OC_3250            0x8000

int CDECL TRlldIOAddressConfig(TRlldDriver_t * driver,
                               TRlldAdapterConfig_t * config,
                               unsigned short address);


#define TRLLD_FIND_OK        1
#define TRLLD_FIND_ERROR     0
#define TRLLD_FIND_VERSION  -1

int CDECL TRlldEISASlotConfig(TRlldDriver_t * driver,
                              TRlldAdapterConfig_t * config,
                              int slot);

int CDECL TRlldMCASlotConfig(TRlldDriver_t * driver,
                             TRlldAdapterConfig_t * config,
                             int slot);

int CDECL TRlldPCIConfig(TRlldDriver_t * driver,
                         TRlldAdapterConfig_t * config,
                         char * PCIConfigurationSpace);

#define TRLLD_PCICONFIG_OK           0
#define TRLLD_PCICONFIG_FAIL         1
#define TRLLD_PCICONFIG_SET_COMMAND  2
#define TRLLD_PCICONFIG_VERSION      3


int CDECL TRlldFindPCI(TRlldDriver_t * driver,
                       TRlldAdapterConfig_t * config_table,
                       unsigned long type_mask,
                       int max);

#ifdef PCMCIA
typedef void * PCCardHandle_t;

typedef int (CDECL * GetTupleData_t)(PCCardHandle_t handle,
                                     unsigned short TupleIdent,
                                     char * TupleData,
                                     int length);

int CDECL TRlldPCCardConfig(TRlldDriver_t * driver,
                            TRlldAdapterConfig_t * config,
                            unsigned short address,
                            int irq,
                            GetTupleData_t GetTuple,
                            PCCardHandle_t handle);

#define TRLLD_PCCARD_CONFIG_OK    0
#define TRLLD_PCCARD_CONFIG_FAIL  1
#endif

/* Boot Prom Support */

typedef struct TRlldPromInfo {
	unsigned short PromIdent;
	short PromPages;
	short PromPageSize;
} TRlldPromInfo_t;

int CDECL TRlldMapBootProm(TRlldAdapter_t * adapter,
                           TRlldDataPtr_t prompointer);

#define TRLLD_PROM_OK           0
#define TRLLD_PROM_FAILED       3

int CDECL TRlldGetPromInfo(TRlldAdapter_t * adapter, TRlldPromInfo_t * info);

#define TRLLD_PROM_OK           0
#define TRLLD_PROM_NOT_MOUNTED  1
#define TRLLD_PROM_NOT_MAPPED   2

void CDECL TRlldSetPromPage(TRlldAdapter_t * adapter, int page);

int CDECL TRlldSetMemoryUse(TRlldAdapter_t * adapter, int use);

#define TRLLD_PROM_TO_MEMORY       0
#define TRLLD_REGISTERS_TO_MEMORY  1

#define TRLLD_MEMORY_USE_OK         0
#define TRLLD_MEMORY_USE_NO_MEMORY  1
#define TRLLD_MEMORY_USE_STATE      2
#define TRLLD_MEMORY_USE_ILLEGAL    3

int CDECL TRlldPromErase(TRlldAdapter_t * adapter,
                         void (CDECL * delay)(int milliseconds));

#define TRLLD_PROM_OK           0
#define TRLLD_PROM_NOT_MOUNTED  1
#define TRLLD_PROM_NOT_MAPPED   2
#define TRLLD_PROM_FAILED       3

int CDECL TRlldPromWrite(TRlldAdapter_t * adapter, char * data,
                         int offset, int count);

#define TRLLD_PROM_OK           0
#define TRLLD_PROM_NOT_MOUNTED  1
#define TRLLD_PROM_NOT_MAPPED   2
#define TRLLD_PROM_FAILED       3
#define TRLLD_PROM_ILLEGAL      4

void CDECL TRlldEmergency(TRlldAdapter_t * adapter);

/* Convert from TRlldAdapterConfig.type to name string */
char * CDECL TRlldAdapterName(int type);
#endif
