/* $FreeBSD: src/sys/dev/asr/osd_unix.h,v 1.1.2.1 2000/09/21 20:33:50 msmith Exp $ */
/*	BSDI osd_unix.h,v 1.7 1998/06/03 19:14:58 karels Exp	*/

/*
 * Copyright (c) 1996-1999 Distributed Processing Technology Corporation
 * All rights reserved.
 *
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 *
 * This software is provided `as is' by Distributed Processing Technology and
 * any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose,
 * are disclaimed. In no event shall Distributed Processing Technology be
 * liable for any direct, indirect, incidental, special, exemplary or
 * consequential damages (including, but not limited to, procurement of
 * substitute goods or services; loss of use, data, or profits; or business
 * interruptions) however caused and on any theory of liability, whether in
 * contract, strict liability, or tort (including negligence or otherwise)
 * arising in any way out of the use of this driver software, even if advised
 * of the possibility of such damage.
 *
 */

#ifndef         __OSD_UNIX_H
#define         __OSD_UNIX_H

/*File - OSD_UNIX.H */
/*****************************************************************************/
/*                                                                           */
/*Description:                                                               */
/*                                                                           */
/*    This file contains definitions for the UNIX OS dependent layer of the  */
/*DPT engine.                                                                */
/*                                                                           */
/*Copyright Distributed Processing Technology, Corp.                         */
/*        140 Candace Dr.                                                    */
/*        Maitland, Fl. 32751   USA                                          */
/*        Phone: (407) 830-5522  Fax: (407) 260-5366                         */
/*        All Rights Reserved                                                */
/*                                                                           */
/*Author:       Bob Pasteur                                                  */
/*Date:         5/28/93                                                      */
/*                                                                           */
/*Editors:                                                                   */
/*		3/7/96	salyzyn@dpt.com					     */
/*			Added BSDi extensions				     */
/*		30/9/99	salyzyn@dpt.com					     */
/*			Added I2ORESCANCMD				     */
/*		7/12/99	salyzyn@dpt.com					     */
/*			Added I2ORESETCMD				     */
/*                                                                           */
/*Remarks:                                                                   */
/*                                                                           */
/*                                                                           */
/*****************************************************************************/

/* Definitions - Defines & Constants ---------------------------------------*/

#define DPT_TurnAroundKey  0x01    /* TurnAround Message Type for engine      */
#define DPT_EngineKey      0x02    /* Message Que and Type for engine         */
#define DPT_LoggerKey      0x03    /* Message Type For Logger                 */
#define DPT_CommEngineKey  0x04    /* Message Que Type Created                */
 
#define MSG_RECEIVE    0x40000000  /* Ored Into Logger PID For Return Msg     */

#define ENGMSG_ECHO        0x00    /* Turnarround Echo Engine Message         */
#define ENGMSG_OPEN        0x01    /* Turnarround Open Engine Message         */
#define ENGMSG_CLOSE       0x02    /* Turnarround Close Engine Message        */

  /* Message Que Creation Flags */

#define MSG_URD            00400  
#define MSG_UWR            00200  
#define MSG_GRD            00040  
#define MSG_GWR            00020  
#define MSG_ORD            00004  
#define MSG_OWR            00002  
#define MSG_ALLRD          00444
#define MSG_ALLWR          00222

  /* Message Que Creation Flags */

#define SHM_URD            00400  
#define SHM_UWR            00200  
#define SHM_GRD            00040  
#define SHM_GWR            00020  
#define SHM_ORD            00004  
#define SHM_OWR            00002  
#define SHM_ALLRD          00444
#define SHM_ALLWR          00222

  /* Program Exit Codes */

#define ExitGoodStatus           0
#define ExitBadParameter         1
#define ExitSignalFail           3
#define ExitMsqAllocFail         5
#define ExitBuffAllocFail        6
#define ExitMsgSendFail          8
#define ExitMsgReceiveFail       9

#define ExitEngOpenFail          10
#define ExitDuplicateEngine      11

#define ExitCommAllocFail        12 
#define ExitDuplicateCommEng     13
#define ExitCommConnectFail      14

#ifndef MAX_HAS

#define MAX_HAS                  18
#define MAX_NAME                 100

#endif  /* ifndef MAX_HAS */


typedef struct {
	uCHAR ConfigLength[4];       /* Len in bytes after this field.      */
	uCHAR EATAsignature[4];
	uCHAR EATAversion;
        uCHAR Flags1;                
	uCHAR PadLength[2];
	uCHAR HBA[4];
	uCHAR CPlength[4];           /* Command Packet Length               */
	uCHAR SPlength[4];           /* Status Packet Length                */
	uCHAR QueueSize[2];          /* Controller Que depth                */
	uCHAR SG_Size[4];
        uCHAR Flags2;
	uCHAR Reserved0;             /* Reserved Field                       */
        uCHAR Flags3;
        uCHAR ScsiValues; 
	uCHAR MaxLUN;                /* Maximun LUN Supported                */ 
        uCHAR Flags4;
	uCHAR RaidNum;               /* RAID HBA Number For Stripping        */
	uCHAR Reserved3;             /* Reserved Field                       */
	       } DptReadConfig_t;

#if defined ( _DPT_SOLARIS )

#include <sys/types.h>
#include <sys/ddidmareq.h>
#include <sys/mutex.h>
#include <sys/scsi/scsi.h>
//#define _KERNEL
#include <sys/dditypes.h>
#include <sys/ddi_impldefs.h>
#include <sys/scsi/impl/transport.h>
//#undef _KERNEL

#undef MSG_DISCONNECT
#define MSG_DISCONNECT  0x11L

#define EATAUSRCMD     1
#define DPT_SIGNATURE  2
#define DPT_NUMCTRLS   3
#define DPT_CTRLINFO   4
#define DPT_SYSINFO    5
#define DPT_BLINKLED   6
#define I2OUSRCMD      7
//#define I2ORESCANCMD 8	/* Use DPT_IO_ACCESS instead */
//#define I2ORESETCMD  9	/* Use DPT_IO_ACCESS instead */

#define	DPT_MAX_DMA_SEGS  32         /* Max used Scatter/Gather seg         */

struct dpt_sg {
       paddr_t data_addr;
       uLONG data_len;
	      };

typedef struct {
	uSHORT NumHBAs;
	uLONG IOAddrs[18];
	       } GetHbaInfo_t;

#elif defined(_DPT_DGUX)

#ifndef _IOWR
# define _IOWR(x,y,z)	(0x0fff3900|y)
#endif
#ifndef _IOW
# define _IOW(x,y,z)	(0x0fff3900|y)
#endif
#ifndef _IOR
# define _IOR(x,y,z)	(0x0fff3900|y)
#endif
#ifndef _IO
# define _IO(x,y)	(0x0fff3900|y)
#endif
/* EATA PassThrough Command	*/
#define EATAUSRCMD      _IOWR('D',65,EATA_CP)
/* Get Signature Structure	*/
#define DPT_SIGNATURE   _IOR('D',67,dpt_sig_S)
/* Get Number Of DPT Adapters	*/
#define DPT_NUMCTRLS    _IOR('D',68,int)
/* Get Adapter Info Structure	*/
#define DPT_CTRLINFO    _IOR('D',69,CtrlInfo)
/* Get System Info Structure	*/
#define DPT_SYSINFO     _IOR('D',72,sysInfo_S)
/* Get Blink LED Code	        */
#define DPT_BLINKLED    _IOR('D',75,int)
/* Get Statistical information (if available) */
#define DPT_STATS_INFO        _IOR('D',80,STATS_DATA)
/* Clear the statistical information          */
#define DPT_STATS_CLEAR       _IO('D',81)
/* Send an I2O command */
#define I2OUSRCMD	_IO('D',76)
/* Inform driver to re-acquire LCT information */
#define I2ORESCANCMD	_IO('D',77)
/* Inform driver to reset adapter */
#define I2ORESETCMD	_IO('D',78)

#elif defined (SNI_MIPS)
  /* Unix Ioctl Command definitions */

#define EATAUSRCMD     (('D'<<8)|65)
#define DPT_DEBUG      (('D'<<8)|66)
#define DPT_SIGNATURE  (('D'<<8)|67)
#define DPT_NUMCTRLS   (('D'<<8)|68)
#define DPT_CTRLINFO   (('D'<<8)|69)
#define DPT_STATINFO   (('D'<<8)|70)
#define DPT_CLRSTAT    (('D'<<8)|71)
#define DPT_SYSINFO    (('D'<<8)|72)
/* Set Timeout Value		*/
#define DPT_TIMEOUT    (('D'<<8)|73)
/* Get config Data  		*/
#define DPT_CONFIG     (('D'<<8)|74)
/* Get config Data  		*/
#define DPT_BLINKLED   (('D'<<8)|75)
/* Get Statistical information (if available) */
#define DPT_STATS_INFO        (('D'<<8)|80)
/* Clear the statistical information          */
#define DPT_STATS_CLEAR       (('D'<<8)|81)
/* Send an I2O command */
#define I2OUSRCMD	(('D'<<8)|76)
/* Inform driver to re-acquire LCT information */
#define I2ORESCANCMD	(('D'<<8)|77)
/* Inform driver to reset adapter */
#define I2ORESETCMD	(('D'<<8)|78)

#else  

  /* Unix Ioctl Command definitions */

#ifdef _DPT_AIX

#undef _IOWR
#undef _IOW
#undef _IOR
#undef _IO
#endif

#ifndef _IOWR
# define _IOWR(x,y,z)	(((x)<<8)|y)
#endif
#ifndef _IOW
# define _IOW(x,y,z)	(((x)<<8)|y)
#endif
#ifndef _IOR
# define _IOR(x,y,z)	(((x)<<8)|y)
#endif
#ifndef _IO
# define _IO(x,y)	(((x)<<8)|y)
#endif
/* EATA PassThrough Command	*/
#define EATAUSRCMD      _IOWR('D',65,EATA_CP)
/* Set Debug Level If Enabled	*/
#define DPT_DEBUG       _IOW('D',66,int)
/* Get Signature Structure	*/
#define DPT_SIGNATURE   _IOR('D',67,dpt_sig_S)
#if defined __bsdi__
#define DPT_SIGNATURE_PACKED   _IOR('D',67,dpt_sig_S_Packed)
#endif
/* Get Number Of DPT Adapters	*/
#define DPT_NUMCTRLS    _IOR('D',68,int)
/* Get Adapter Info Structure	*/
#define DPT_CTRLINFO    _IOR('D',69,CtrlInfo)
/* Get Statistics If Enabled	*/
#define DPT_STATINFO    _IO('D',70)
/* Clear Stats If Enabled	*/
#define DPT_CLRSTAT     _IO('D',71)
/* Get System Info Structure	*/
#define DPT_SYSINFO     _IOR('D',72,sysInfo_S)
/* Set Timeout Value		*/
#define DPT_TIMEOUT     _IO('D',73)
/* Get config Data  		*/
#define DPT_CONFIG      _IO('D',74)
/* Get Blink LED Code	        */
#define DPT_BLINKLED    _IOR('D',75,int)
/* Get Statistical information (if available) */
#define DPT_STATS_INFO        _IOR('D',80,STATS_DATA)
/* Clear the statistical information          */
#define DPT_STATS_CLEAR       _IO('D',81)
/* Get Performance metrics */
#define DPT_PERF_INFO        _IOR('D',82,dpt_perf_t)
/* Send an I2O command */
#define I2OUSRCMD	_IO('D',76)
/* Inform driver to re-acquire LCT information */
#define I2ORESCANCMD	_IO('D',77)
/* Inform driver to reset adapter */
#define I2ORESETCMD	_IO('D',78)
#if defined _DPT_LINUX
/* See if the target is mounted */
#define DPT_TARGET_BUSY	_IOR('D',79, TARGET_BUSY_T)
#endif


#endif  /* _DPT_SOLARIS else */

		 /* Adapter Flags Field Bit Definitions */

#define CTLR_INSTALLED  0x00000001  /* Adapter Was Installed        */
#define CTLR_DMA        0x00000002  /* DMA Supported                */
#define CTLR_OVERLAP    0x00000004  /* Overlapped Commands Support  */
#define CTLR_SECONDARY  0x00000008  /* I/O Address Not 0x1f0        */
#define CTLR_BLINKLED   0x00000010  /* Adapter In Blink LED State   */
#define CTLR_HBACI      0x00000020  /* Cache Inhibit Supported      */
#define CTLR_CACHE      0x00000040  /* Adapter Has Cache            */
#define CTLR_SANE       0x00000080  /* Adapter Functioning OK       */
#define CTLR_BUS_QUIET  0x00000100  /* Bus Quite On This Adapter    */
#define CTLR_ABOVE_16   0x00000200  /* Support For Mem. Above 16 MB */
#define CTLR_SCAT_GATH  0x00000400  /* Scatter Gather Supported     */


/* Definitions - Structure & Typedef ---------------------------------------*/

typedef struct {
		 uLONG     MsgID;
		 DPT_TAG_T engineTag;
		 DPT_TAG_T targetTag;
		 DPT_MSG_T engEvent;
		 long      BufferID;
		 uLONG     FromEngBuffOffset;
		 uLONG     callerID;
		 DPT_RTN_T result;
		 uLONG     timeOut;
	       } MsgHdr;

#define MsgDataSize sizeof(MsgHdr) - 4

#ifndef SNI_MIPS

/*-------------------------------------------------------------------------*/
/*                     EATA Command Packet definition                      */
/*-------------------------------------------------------------------------*/

typedef struct EATACommandPacket {

#ifdef _DPT_UNIXWARE

	uCHAR     EataID[4];
	uINT      EataCmd;
	uCHAR     *CmdBuffer;

#endif   /* _DPT_UNIXWARE */

#ifdef _DPT_AIX

        uCHAR     HbaTargetID;
        uCHAR     HbaLUN;
 
#endif  /* _DPT_AIX */

        uCHAR    cp_Flags1;          /* Command Flags                       */
	uCHAR    cp_Req_Len;         /* AutoRequestSense Data length.       */
	uCHAR    cp_Resv1[3];        /* Reserved Fields                     */
        uCHAR    cp_Flags2; 
        uCHAR    cp_Flags3;
        uCHAR    cp_ScsiAddr;
	uCHAR    cp_msg0;            /* Identify and Disconnect Message.    */
	uCHAR    cp_msg1;
	uCHAR    cp_msg2;
	uCHAR    cp_msg3;
	uCHAR    cp_cdb[12];         /* SCSI cdb for command.               */
	uLONG    cp_dataLen;         /* Data length in Bytes for command.   */
	uLONG    cp_Vue;             /* Vendor Unique Area                  */
	uCHAR    *cp_DataAddr;       /* Data Address For The Command.       */
	uCHAR    *cp_SpAddr;         /* Status Packet Physical Address.     */
	uCHAR    *cp_SenseAddr;      /* AutoRequestSense Data Phy Address.  */

#ifdef _DPT_SOLARIS

	uCHAR     HostStatus;
	uCHAR     TargetStatus;
	uCHAR     CdbLength;
	uCHAR     SG_Size;
	struct scsi_arq_status ReqSenseData;
	struct  dpt_sg SG_List[DPT_MAX_DMA_SEGS];
	union {
		char *b_scratch;
		struct scsi_cmd *b_ownerp; 
	      } cc;
	paddr_t ccb_paddr;
	uSHORT IOAddress;
	
#else  /* _DPT_SOLARIS */

	uLONG     TimeOut ;
	uCHAR     HostStatus;
	uCHAR     TargetStatus;
	uCHAR     Retries; 

#endif  /* _DPT_SOLARIS else */

				  } EATA_CP;
#endif // SNI_MIPS


                      /* Control Flags 1 Definitions */

#define SCSI_RESET        0x01       /* Cause a SCSI Bus reset on the cmd */
#define HBA_INIT          0x02       /* Cause Controller to reInitialize  */
#define AUTO_REQ_SENSE    0x04       /* Do Auto Request Sense on errors   */
#define SCATTER_GATHER    0x08       /* Data Ptr points to a SG Packet    */
#define INTERPRET         0x20       /* Interpret the SCSI cdb of own use */
#define DATA_OUT          0x04       /* Data Out phase with command       */
#define DATA_IN           0x08       /* Data In phase with command        */

                      /* Control Flags 2 Definitions */

#define FIRMWARE_NESTED   0x01


                      /* Control Flags 3 Definitions */

#define PHYSICAL_UNIT     0x01       /* Send Command Directly To Target   */
#define IAT               0x02       /* Inhibit Address Translation       */
#define HBACI             0x04       /* Inhibit Caching                   */


  /* Structure Returned From Get Controller Info                             */

typedef struct {

	uCHAR    state;            /* Operational state               */
	uCHAR    id;               /* Host adapter SCSI id            */
	int      vect;             /* Interrupt vector number         */
	int      base;             /* Base I/O address                */
	int      njobs;            /* # of jobs sent to HA            */
	int      qdepth;           /* Controller queue depth.         */
	int      wakebase;         /* mpx wakeup base index.          */
	uLONG    SGsize;           /* Scatter/Gather list size.       */
	unsigned heads;            /* heads for drives on cntlr.      */
	unsigned sectors;          /* sectors for drives on cntlr.    */
	uCHAR    do_drive32;       /* Flag for Above 16 MB Ability    */
	uCHAR    BusQuiet;         /* SCSI Bus Quiet Flag             */
	char     idPAL[4];         /* 4 Bytes Of The ID Pal           */
	uCHAR    primary;          /* 1 For Primary, 0 For Secondary  */
	uCHAR    eataVersion;      /* EATA Version                    */
	uLONG    cpLength;         /* EATA Command Packet Length      */
	uLONG    spLength;         /* EATA Status Packet Length       */
	uCHAR    drqNum;           /* DRQ Index (0,5,6,7)             */ 
	uCHAR    flag1;            /* EATA Flags 1 (Byte 9)           */
	uCHAR    flag2;            /* EATA Flags 2 (Byte 30)          */

	       } CtrlInfo;

#ifndef SNI_MIPS
#ifdef _DPT_UNIXWARE

typedef struct {

	uINT     state;            /* Operational state            */ 
	uCHAR    id[4];            /* Host adapter SCSI id         */ 
	uINT     vect;             /* Interrupt vector number      */ 
	uLONG    base;             /* Base I/O address             */ 
	int      ha_max_jobs;      /* Max number of Active Jobs    */
        uLONG    ha_cacheParams;
	int      ha_nbus;          /* Number Of Busses on HBA      */
	int      ha_ntargets;      /* Number Of Targets Supported  */
	int      ha_nluns;         /* Number Of LUNs Supported     */
	int      ha_tshift;        /* Shift value for target       */
	int      ha_bshift;        /* Shift value for bus          */
	uINT     ha_npend;         /* # of jobs sent to HA         */
	int      ha_active_jobs;   /* Number Of Active Jobs        */

	       } HbaInfo;

	/* SDI ioctl prefix for hba specific ioctl's */

#define	SDI_IOC        (('S'<<24)|('D'<<16)|('I'<<8))

#define SDI_HBANAME    ((SDI_IOC)|0x14) /* Get HBA module name      */
#define SDI_SEND       0x0081           /* Send a SCSI command      */

#else

typedef struct {

	uLONG  flags;            /* Operational State Flags         */
	uCHAR  id[4];            /* Host Adapter SCSI ID            */
	int    vect;             /* Interrupt Vector Number         */
	int    base;             /* Base I/O Address                */
	int    njobs;            /* # Of CCBs Outstanding To HBA    */
	int    qdepth;           /* Controller Queue depth.         */
	uLONG  SGsize;           /* Scatter/Gather List Size.       */
	char   idPAL[4];         /* 4 Bytes Of The ID Pal           */
	uCHAR  eataVersion;      /* EATA Version                    */
	uLONG  cpLength;         /* EATA Command Packet Length      */
	uLONG  spLength;         /* EATA Status Packet Length       */
	uCHAR  drqNum;           /* DRQ Index (0,5,6,7)             */ 
	uCHAR  eataflag1;        /* EATA Flags 1 (Byte 9)           */
	uCHAR  eataflag2;        /* EATA Flags 2 (Byte 30)          */
	uCHAR  maxChannel;       /* Maximum Channel Number          */
	uCHAR  maxID;            /* Maximum Target ID               */
	uCHAR  maxLUN;           /* Maximum LUN                     */
	uCHAR  HbaBusType;       /* HBA Bus Type, EISA, PCI, etc    */
	uCHAR  RaidNum;          /* Host Adapter RAID Number        */  

	       } HbaInfo;

#endif  /* _DPT_UNIXWARE */
#endif // SNI_MIPS


#ifdef _DPT_AIX

/*
 * DPT Host Adapter config information structure - this structure contains
 * configuration information about an adapter.  It is imbedded into the 
 * dpt_ctl structure.
 */

typedef struct dpt_cfg {
    uchar	flags;			/* Operational state flags	*/
    uchar	id[4];			/* Host adapter SCSI IDs	*/
    int		vect;			/* Interrupt vector number	*/
    ulong 	base_addr;		/* Base I/O address		*/
    int		qdepth;			/* Controller queue depth.	*/
    ulong	SGsize;			/* Max scatter/gather list sz	*/
    ulong	SGmax;			/* Max s/g we can use per req	*/
    uchar	eataVersion;		/* EATA version			*/
    ushort	cpPadLen;		/* # of pad bytes sent to HA for
					   PIO commands			*/
    ulong	cpLength;		/* EATA Command Packet length	*/
    ulong	spLength;		/* EATA Status Packet length	*/
    uchar	eataflag1;		/* EATA Flags 1 (Byte 9)	*/
    uchar	eataflag2;		/* EATA Flags 2 (Byte 30)	*/
    uchar	maxChan;		/* Maximum Channel number	*/
    uchar	maxID;			/* Maximum target ID		*/
    uchar	maxLUN;			/* Maximum LUN			*/
    uchar	HbaBusType;		/* HBA bus type, EISA, PCI, etc	*/
    uchar	RaidNum;		/* Host adapter RAID number	*/
} DptCfg_t;

#endif /* _DPT_AIX */


#define MAX_ELEMENT_COUNT        64
#define MAX_BUCKET_COUNT         10

/*
 * DPT statistics structure definitions
 */
typedef struct IO_SIZE_STATS 
{
  uLONG TotalIoCount;
  uLONG IoCountRead;
  uLONG IoCountReadSg;
  uLONG IoCountWrite;
  uLONG IoCountWriteSg;
  uLONG UnalignedIoAddress;
  uLONG SgElementCount[MAX_ELEMENT_COUNT];

} IO_SIZE_STATS_T, *pIO_SIZE_STATS_T;

typedef struct STATS_DATA 
{
  uLONG TotalIoCount;
  uLONG TotalUnCachedIoCount;
  uLONG MaxOutstandingIoCount;
  uLONG CurrentOutstandingIoCount;
  uLONG OutstandingIoRunningCount;
  uLONG UnalignedPktCount;
  uLONG UnalignedSgCount;
  uLONG NonPageListAddressSgCount;
  uLONG MaxMessagesPerInterrupt;
  IO_SIZE_STATS_T IoSize[MAX_BUCKET_COUNT];

} STATS_DATA_T, *pSTATS_DATA_T;

typedef struct TARGET_BUSY
{
  uLONG channel;
  uLONG id;
  uLONG lun;
  uLONG isBusy;
} TARGET_BUSY_T;
#endif /* __OSD_UNIX_H */
