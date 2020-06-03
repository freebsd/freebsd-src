/** @file
  Definitions based on NVMe spec. version 1.1.

  (C) Copyright 2016 Hewlett Packard Enterprise Development LP<BR>
  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  NVMe Specification 1.1

**/

#ifndef __NVM_E_H__
#define __NVM_E_H__

#pragma pack(1)

//
// controller register offsets
//
#define NVME_CAP_OFFSET          0x0000  // Controller Capabilities
#define NVME_VER_OFFSET          0x0008  // Version
#define NVME_INTMS_OFFSET        0x000c  // Interrupt Mask Set
#define NVME_INTMC_OFFSET        0x0010  // Interrupt Mask Clear
#define NVME_CC_OFFSET           0x0014  // Controller Configuration
#define NVME_CSTS_OFFSET         0x001c  // Controller Status
#define NVME_NSSR_OFFSET         0x0020  // NVM Subsystem Reset
#define NVME_AQA_OFFSET          0x0024  // Admin Queue Attributes
#define NVME_ASQ_OFFSET          0x0028  // Admin Submission Queue Base Address
#define NVME_ACQ_OFFSET          0x0030  // Admin Completion Queue Base Address
#define NVME_SQ0_OFFSET          0x1000  // Submission Queue 0 (admin) Tail Doorbell
#define NVME_CQ0_OFFSET          0x1004  // Completion Queue 0 (admin) Head Doorbell

//
// These register offsets are defined as 0x1000 + (N * (4 << CAP.DSTRD))
// Get the doorbell stride bit shift value from the controller capabilities.
//
#define NVME_SQTDBL_OFFSET(QID, DSTRD)    0x1000 + ((2 * (QID)) * (4 << (DSTRD)))       // Submission Queue y (NVM) Tail Doorbell
#define NVME_CQHDBL_OFFSET(QID, DSTRD)    0x1000 + (((2 * (QID)) + 1) * (4 << (DSTRD))) // Completion Queue y (NVM) Head Doorbell


#pragma pack(1)

//
// 3.1.1 Offset 00h: CAP - Controller Capabilities
//
typedef struct {
  UINT16 Mqes;      // Maximum Queue Entries Supported
  UINT8  Cqr:1;     // Contiguous Queues Required
  UINT8  Ams:2;     // Arbitration Mechanism Supported
  UINT8  Rsvd1:5;
  UINT8  To;        // Timeout
  UINT16 Dstrd:4;
  UINT16 Nssrs:1;   // NVM Subsystem Reset Supported NSSRS
  UINT16 Css:4;     // Command Sets Supported - Bit 37
  UINT16 Rsvd3:7;
  UINT8  Mpsmin:4;
  UINT8  Mpsmax:4;
  UINT8  Rsvd4;
} NVME_CAP;

//
// 3.1.2 Offset 08h: VS - Version
//
typedef struct {
  UINT16 Mnr;       // Minor version number
  UINT16 Mjr;       // Major version number
} NVME_VER;

//
// 3.1.5 Offset 14h: CC - Controller Configuration
//
typedef struct {
  UINT16 En:1;       // Enable
  UINT16 Rsvd1:3;
  UINT16 Css:3;      // I/O Command Set Selected
  UINT16 Mps:4;      // Memory Page Size
  UINT16 Ams:3;      // Arbitration Mechanism Selected
  UINT16 Shn:2;      // Shutdown Notification
  UINT8  Iosqes:4;   // I/O Submission Queue Entry Size
  UINT8  Iocqes:4;   // I/O Completion Queue Entry Size
  UINT8  Rsvd2;
} NVME_CC;
#define NVME_CC_SHN_NORMAL_SHUTDOWN    1
#define NVME_CC_SHN_ABRUPT_SHUTDOWN    2

//
// 3.1.6 Offset 1Ch: CSTS - Controller Status
//
typedef struct {
  UINT32 Rdy:1;      // Ready
  UINT32 Cfs:1;      // Controller Fatal Status
  UINT32 Shst:2;     // Shutdown Status
  UINT32 Nssro:1;    // NVM Subsystem Reset Occurred
  UINT32 Rsvd1:27;
} NVME_CSTS;
#define NVME_CSTS_SHST_SHUTDOWN_OCCURRING 1
#define NVME_CSTS_SHST_SHUTDOWN_COMPLETED 2
//
// 3.1.8 Offset 24h: AQA - Admin Queue Attributes
//
typedef struct {
  UINT16 Asqs:12;    // Submission Queue Size
  UINT16 Rsvd1:4;
  UINT16 Acqs:12;    // Completion Queue Size
  UINT16 Rsvd2:4;
} NVME_AQA;

//
// 3.1.9 Offset 28h: ASQ - Admin Submission Queue Base Address
//
#define NVME_ASQ      UINT64
//
// 3.1.10 Offset 30h: ACQ - Admin Completion Queue Base Address
//
#define NVME_ACQ      UINT64

//
// 3.1.11 Offset (1000h + ((2y) * (4 << CAP.DSTRD))): SQyTDBL - Submission Queue y Tail Doorbell
//
typedef struct {
  UINT16 Sqt;
  UINT16 Rsvd1;
} NVME_SQTDBL;

//
// 3.1.12 Offset (1000h + ((2y + 1) * (4 << CAP.DSTRD))): CQyHDBL - Completion Queue y Head Doorbell
//
typedef struct {
  UINT16 Cqh;
  UINT16 Rsvd1;
} NVME_CQHDBL;

//
// NVM command set structures
//
// Read Command
//
typedef struct {
  //
  // CDW 10, 11
  //
  UINT64 Slba;                /* Starting Sector Address */
  //
  // CDW 12
  //
  UINT16 Nlb;                 /* Number of Sectors */
  UINT16 Rsvd1:10;
  UINT16 Prinfo:4;            /* Protection Info Check */
  UINT16 Fua:1;               /* Force Unit Access */
  UINT16 Lr:1;                /* Limited Retry */
  //
  // CDW 13
  //
  UINT32 Af:4;                /* Access Frequency */
  UINT32 Al:2;                /* Access Latency */
  UINT32 Sr:1;                /* Sequential Request */
  UINT32 In:1;                /* Incompressible */
  UINT32 Rsvd2:24;
  //
  // CDW 14
  //
  UINT32 Eilbrt;              /* Expected Initial Logical Block Reference Tag */
  //
  // CDW 15
  //
  UINT16 Elbat;               /* Expected Logical Block Application Tag */
  UINT16 Elbatm;              /* Expected Logical Block Application Tag Mask */
} NVME_READ;

//
// Write Command
//
typedef struct {
  //
  // CDW 10, 11
  //
  UINT64 Slba;                /* Starting Sector Address */
  //
  // CDW 12
  //
  UINT16 Nlb;                 /* Number of Sectors */
  UINT16 Rsvd1:10;
  UINT16 Prinfo:4;            /* Protection Info Check */
  UINT16 Fua:1;               /* Force Unit Access */
  UINT16 Lr:1;                /* Limited Retry */
  //
  // CDW 13
  //
  UINT32 Af:4;                /* Access Frequency */
  UINT32 Al:2;                /* Access Latency */
  UINT32 Sr:1;                /* Sequential Request */
  UINT32 In:1;                /* Incompressible */
  UINT32 Rsvd2:24;
  //
  // CDW 14
  //
  UINT32 Ilbrt;               /* Initial Logical Block Reference Tag */
  //
  // CDW 15
  //
  UINT16 Lbat;                /* Logical Block Application Tag */
  UINT16 Lbatm;               /* Logical Block Application Tag Mask */
} NVME_WRITE;

//
// Flush
//
typedef struct {
  //
  // CDW 10
  //
  UINT32 Flush;               /* Flush */
} NVME_FLUSH;

//
// Write Uncorrectable command
//
typedef struct {
  //
  // CDW 10, 11
  //
  UINT64 Slba;                /* Starting LBA */
  //
  // CDW 12
  //
  UINT32 Nlb:16;              /* Number of  Logical Blocks */
  UINT32 Rsvd1:16;
} NVME_WRITE_UNCORRECTABLE;

//
// Write Zeroes command
//
typedef struct {
  //
  // CDW 10, 11
  //
  UINT64 Slba;                /* Starting LBA */
  //
  // CDW 12
  //
  UINT16 Nlb;                 /* Number of Logical Blocks */
  UINT16 Rsvd1:10;
  UINT16 Prinfo:4;            /* Protection Info Check */
  UINT16 Fua:1;               /* Force Unit Access */
  UINT16 Lr:1;                /* Limited Retry */
  //
  // CDW 13
  //
  UINT32 Rsvd2;
  //
  // CDW 14
  //
  UINT32 Ilbrt;               /* Initial Logical Block Reference Tag */
  //
  // CDW 15
  //
  UINT16 Lbat;                /* Logical Block Application Tag */
  UINT16 Lbatm;               /* Logical Block Application Tag Mask */
} NVME_WRITE_ZEROES;

//
// Compare command
//
typedef struct {
  //
  // CDW 10, 11
  //
  UINT64 Slba;                /* Starting LBA */
  //
  // CDW 12
  //
  UINT16 Nlb;                 /* Number of Logical Blocks */
  UINT16 Rsvd1:10;
  UINT16 Prinfo:4;            /* Protection Info Check */
  UINT16 Fua:1;               /* Force Unit Access */
  UINT16 Lr:1;                /* Limited Retry */
  //
  // CDW 13
  //
  UINT32 Rsvd2;
  //
  // CDW 14
  //
  UINT32 Eilbrt;              /* Expected Initial Logical Block Reference Tag */
  //
  // CDW 15
  //
  UINT16 Elbat;               /* Expected Logical Block Application Tag */
  UINT16 Elbatm;              /* Expected Logical Block Application Tag Mask */
} NVME_COMPARE;

typedef union {
  NVME_READ                   Read;
  NVME_WRITE                  Write;
  NVME_FLUSH                  Flush;
  NVME_WRITE_UNCORRECTABLE    WriteUncorrectable;
  NVME_WRITE_ZEROES           WriteZeros;
  NVME_COMPARE                Compare;
} NVME_CMD;

typedef struct {
  UINT16 Mp;                /* Maximum Power */
  UINT8  Rsvd1;             /* Reserved as of Nvm Express 1.1 Spec */
  UINT8  Mps:1;             /* Max Power Scale */
  UINT8  Nops:1;            /* Non-Operational State */
  UINT8  Rsvd2:6;           /* Reserved as of Nvm Express 1.1 Spec */
  UINT32 Enlat;             /* Entry Latency */
  UINT32 Exlat;             /* Exit Latency */
  UINT8  Rrt:5;             /* Relative Read Throughput */
  UINT8  Rsvd3:3;           /* Reserved as of Nvm Express 1.1 Spec */
  UINT8  Rrl:5;             /* Relative Read Latency */
  UINT8  Rsvd4:3;           /* Reserved as of Nvm Express 1.1 Spec */
  UINT8  Rwt:5;             /* Relative Write Throughput */
  UINT8  Rsvd5:3;           /* Reserved as of Nvm Express 1.1 Spec */
  UINT8  Rwl:5;             /* Relative Write Latency */
  UINT8  Rsvd6:3;           /* Reserved as of Nvm Express 1.1 Spec */
  UINT8  Rsvd7[16];         /* Reserved as of Nvm Express 1.1 Spec */
} NVME_PSDESCRIPTOR;

//
//  Identify Controller Data
//
typedef struct {
  //
  // Controller Capabilities and Features 0-255
  //
  UINT16 Vid;                 /* PCI Vendor ID */
  UINT16 Ssvid;               /* PCI sub-system vendor ID */
  UINT8  Sn[20];              /* Product serial number */

  UINT8  Mn[40];              /* Product model number */
  UINT8  Fr[8];               /* Firmware Revision */
  UINT8  Rab;                 /* Recommended Arbitration Burst */
  UINT8  Ieee_oui[3];         /* Organization Unique Identifier */
  UINT8  Cmic;                /* Multi-interface Capabilities */
  UINT8  Mdts;                /* Maximum Data Transfer Size */
  UINT8  Cntlid[2];           /* Controller ID */
  UINT8  Rsvd1[176];          /* Reserved as of Nvm Express 1.1 Spec */
  //
  // Admin Command Set Attributes
  //
  UINT16 Oacs;                /* Optional Admin Command Support */
    #define NAMESPACE_MANAGEMENT_SUPPORTED  BIT3
    #define FW_DOWNLOAD_ACTIVATE_SUPPORTED  BIT2
    #define FORMAT_NVM_SUPPORTED            BIT1
    #define SECURITY_SEND_RECEIVE_SUPPORTED BIT0
  UINT8  Acl;                 /* Abort Command Limit */
  UINT8  Aerl;                /* Async Event Request Limit */
  UINT8  Frmw;                /* Firmware updates */
  UINT8  Lpa;                 /* Log Page Attributes */
  UINT8  Elpe;                /* Error Log Page Entries */
  UINT8  Npss;                /* Number of Power States Support */
  UINT8  Avscc;               /* Admin Vendor Specific Command Configuration */
  UINT8  Apsta;               /* Autonomous Power State Transition Attributes */
  UINT8  Rsvd2[246];          /* Reserved as of Nvm Express 1.1 Spec */
  //
  // NVM Command Set Attributes
  //
  UINT8  Sqes;                /* Submission Queue Entry Size */
  UINT8  Cqes;                /* Completion Queue Entry Size */
  UINT16 Rsvd3;               /* Reserved as of Nvm Express 1.1 Spec */
  UINT32 Nn;                  /* Number of Namespaces */
  UINT16 Oncs;                /* Optional NVM Command Support */
  UINT16 Fuses;               /* Fused Operation Support */
  UINT8  Fna;                 /* Format NVM Attributes */
  UINT8  Vwc;                 /* Volatile Write Cache */
  UINT16 Awun;                /* Atomic Write Unit Normal */
  UINT16 Awupf;               /* Atomic Write Unit Power Fail */
  UINT8  Nvscc;               /* NVM Vendor Specific Command Configuration */
  UINT8  Rsvd4;               /* Reserved as of Nvm Express 1.1 Spec */
  UINT16 Acwu;                /* Atomic Compare & Write Unit */
  UINT16 Rsvd5;               /* Reserved as of Nvm Express 1.1 Spec */
  UINT32 Sgls;                /* SGL Support  */
  UINT8  Rsvd6[164];          /* Reserved as of Nvm Express 1.1 Spec */
  //
  // I/O Command set Attributes
  //
  UINT8 Rsvd7[1344];          /* Reserved as of Nvm Express 1.1 Spec */
  //
  // Power State Descriptors
  //
  NVME_PSDESCRIPTOR PsDescriptor[32];

  UINT8  VendorData[1024];    /* Vendor specific data */
} NVME_ADMIN_CONTROLLER_DATA;

typedef struct {
  UINT16 Ms;                /* Metadata Size */
  UINT8  Lbads;             /* LBA Data Size */
  UINT8  Rp:2;              /* Relative Performance */
    #define LBAF_RP_BEST      00b
    #define LBAF_RP_BETTER    01b
    #define LBAF_RP_GOOD      10b
    #define LBAF_RP_DEGRADED  11b
  UINT8  Rsvd1:6;           /* Reserved as of Nvm Express 1.1 Spec */
} NVME_LBAFORMAT;

//
// Identify Namespace Data
//
typedef struct {
  //
  // NVM Command Set Specific
  //
  UINT64 Nsze;                /* Namespace Size (total number of blocks in formatted namespace) */
  UINT64 Ncap;                /* Namespace Capacity (max number of logical blocks) */
  UINT64 Nuse;                /* Namespace Utilization */
  UINT8  Nsfeat;              /* Namespace Features */
  UINT8  Nlbaf;               /* Number of LBA Formats */
  UINT8  Flbas;               /* Formatted LBA size */
  UINT8  Mc;                  /* Metadata Capabilities */
  UINT8  Dpc;                 /* End-to-end Data Protection capabilities */
  UINT8  Dps;                 /* End-to-end Data Protection Type Settings */
  UINT8  Nmic;                /* Namespace Multi-path I/O and Namespace Sharing Capabilities */
  UINT8  Rescap;              /* Reservation Capabilities */
  UINT8  Rsvd1[88];           /* Reserved as of Nvm Express 1.1 Spec */
  UINT64 Eui64;               /* IEEE Extended Unique Identifier */
  //
  // LBA Format
  //
  NVME_LBAFORMAT LbaFormat[16];

  UINT8 Rsvd2[192];           /* Reserved as of Nvm Express 1.1 Spec */
  UINT8 VendorData[3712];     /* Vendor specific data */
} NVME_ADMIN_NAMESPACE_DATA;

//
// NvmExpress Admin Identify Cmd
//
typedef struct {
  //
  // CDW 10
  //
  UINT32 Cns:2;
  UINT32 Rsvd1:30;
} NVME_ADMIN_IDENTIFY;

//
// NvmExpress Admin Create I/O Completion Queue
//
typedef struct {
  //
  // CDW 10
  //
  UINT32 Qid:16;              /* Queue Identifier */
  UINT32 Qsize:16;            /* Queue Size */

  //
  // CDW 11
  //
  UINT32 Pc:1;                /* Physically Contiguous */
  UINT32 Ien:1;               /* Interrupts Enabled */
  UINT32 Rsvd1:14;            /* reserved as of Nvm Express 1.1 Spec */
  UINT32 Iv:16;               /* Interrupt Vector for MSI-X or MSI*/
} NVME_ADMIN_CRIOCQ;

//
// NvmExpress Admin Create I/O Submission Queue
//
typedef struct {
  //
  // CDW 10
  //
  UINT32 Qid:16;              /* Queue Identifier */
  UINT32 Qsize:16;            /* Queue Size */

  //
  // CDW 11
  //
  UINT32 Pc:1;                /* Physically Contiguous */
  UINT32 Qprio:2;             /* Queue Priority */
  UINT32 Rsvd1:13;            /* Reserved as of Nvm Express 1.1 Spec */
  UINT32 Cqid:16;             /* Completion Queue ID */
} NVME_ADMIN_CRIOSQ;

//
// NvmExpress Admin Delete I/O Completion Queue
//
typedef struct {
  //
  // CDW 10
  //
  UINT16 Qid;
  UINT16 Rsvd1;
} NVME_ADMIN_DEIOCQ;

//
// NvmExpress Admin Delete I/O Submission Queue
//
typedef struct {
  //
  // CDW 10
  //
  UINT16 Qid;
  UINT16 Rsvd1;
} NVME_ADMIN_DEIOSQ;

//
// NvmExpress Admin Abort Command
//
typedef struct {
  //
  // CDW 10
  //
  UINT32 Sqid:16;             /* Submission Queue identifier */
  UINT32 Cid:16;              /* Command Identifier */
} NVME_ADMIN_ABORT;

//
// NvmExpress Admin Firmware Activate Command
//
typedef struct {
  //
  // CDW 10
  //
  UINT32 Fs:3;                /* Submission Queue identifier */
  UINT32 Aa:2;                /* Command Identifier */
  UINT32 Rsvd1:27;
} NVME_ADMIN_FIRMWARE_ACTIVATE;

//
// NvmExpress Admin Firmware Image Download Command
//
typedef struct {
  //
  // CDW 10
  //
  UINT32 Numd;                /* Number of Dwords */
  //
  // CDW 11
  //
  UINT32 Ofst;                /* Offset */
} NVME_ADMIN_FIRMWARE_IMAGE_DOWNLOAD;

//
// NvmExpress Admin Get Features Command
//
typedef struct {
  //
  // CDW 10
  //
  UINT32 Fid:8;                /* Feature Identifier */
  UINT32 Sel:3;                /* Select */
  UINT32 Rsvd1:21;
} NVME_ADMIN_GET_FEATURES;

//
// NvmExpress Admin Get Log Page Command
//
typedef struct {
  //
  // CDW 10
  //
  UINT32 Lid:8;               /* Log Page Identifier */
    #define LID_ERROR_INFO   0x1
    #define LID_SMART_INFO   0x2
    #define LID_FW_SLOT_INFO 0x3
  UINT32 Rsvd1:8;
  UINT32 Numd:12;             /* Number of Dwords */
  UINT32 Rsvd2:4;             /* Reserved as of Nvm Express 1.1 Spec */
} NVME_ADMIN_GET_LOG_PAGE;

//
// NvmExpress Admin Set Features Command
//
typedef struct {
  //
  // CDW 10
  //
  UINT32 Fid:8;               /* Feature Identifier */
  UINT32 Rsvd1:23;
  UINT32 Sv:1;                /* Save */
} NVME_ADMIN_SET_FEATURES;

//
// NvmExpress Admin Format NVM Command
//
typedef struct {
  //
  // CDW 10
  //
  UINT32 Lbaf:4;              /* LBA Format */
  UINT32 Ms:1;                /* Metadata Settings */
  UINT32 Pi:3;                /* Protection Information */
  UINT32 Pil:1;               /* Protection Information Location */
  UINT32 Ses:3;               /* Secure Erase Settings */
  UINT32 Rsvd1:20;
} NVME_ADMIN_FORMAT_NVM;

//
// NvmExpress Admin Security Receive Command
//
typedef struct {
  //
  // CDW 10
  //
  UINT32 Rsvd1:8;
  UINT32 Spsp:16;             /* SP Specific */
  UINT32 Secp:8;              /* Security Protocol */
  //
  // CDW 11
  //
  UINT32 Al;                  /* Allocation Length */
} NVME_ADMIN_SECURITY_RECEIVE;

//
// NvmExpress Admin Security Send Command
//
typedef struct {
  //
  // CDW 10
  //
  UINT32 Rsvd1:8;
  UINT32 Spsp:16;             /* SP Specific */
  UINT32 Secp:8;              /* Security Protocol */
  //
  // CDW 11
  //
  UINT32 Tl;                  /* Transfer Length */
} NVME_ADMIN_SECURITY_SEND;

typedef union {
  NVME_ADMIN_IDENTIFY                   Identify;
  NVME_ADMIN_CRIOCQ                     CrIoCq;
  NVME_ADMIN_CRIOSQ                     CrIoSq;
  NVME_ADMIN_DEIOCQ                     DeIoCq;
  NVME_ADMIN_DEIOSQ                     DeIoSq;
  NVME_ADMIN_ABORT                      Abort;
  NVME_ADMIN_FIRMWARE_ACTIVATE          Activate;
  NVME_ADMIN_FIRMWARE_IMAGE_DOWNLOAD    FirmwareImageDownload;
  NVME_ADMIN_GET_FEATURES               GetFeatures;
  NVME_ADMIN_GET_LOG_PAGE               GetLogPage;
  NVME_ADMIN_SET_FEATURES               SetFeatures;
  NVME_ADMIN_FORMAT_NVM                 FormatNvm;
  NVME_ADMIN_SECURITY_RECEIVE           SecurityReceive;
  NVME_ADMIN_SECURITY_SEND              SecuritySend;
} NVME_ADMIN_CMD;

typedef struct {
  UINT32 Cdw10;
  UINT32 Cdw11;
  UINT32 Cdw12;
  UINT32 Cdw13;
  UINT32 Cdw14;
  UINT32 Cdw15;
} NVME_RAW;

typedef union {
  NVME_ADMIN_CMD Admin;   // Union of Admin commands
  NVME_CMD       Nvm;     // Union of Nvm commands
  NVME_RAW       Raw;
} NVME_PAYLOAD;

//
// Submission Queue
//
typedef struct {
  //
  // CDW 0, Common to all commands
  //
  UINT8  Opc;               // Opcode
  UINT8  Fuse:2;            // Fused Operation
  UINT8  Rsvd1:5;
  UINT8  Psdt:1;            // PRP or SGL for Data Transfer
  UINT16 Cid;               // Command Identifier

  //
  // CDW 1
  //
  UINT32 Nsid;              // Namespace Identifier

  //
  // CDW 2,3
  //
  UINT64 Rsvd2;

  //
  // CDW 4,5
  //
  UINT64 Mptr;              // Metadata Pointer

  //
  // CDW 6-9
  //
  UINT64 Prp[2];            // First and second PRP entries

  NVME_PAYLOAD Payload;

} NVME_SQ;

//
// Completion Queue
//
typedef struct {
  //
  // CDW 0
  //
  UINT32 Dword0;
  //
  // CDW 1
  //
  UINT32 Rsvd1;
  //
  // CDW 2
  //
  UINT16 Sqhd;              // Submission Queue Head Pointer
  UINT16 Sqid;              // Submission Queue Identifier
  //
  // CDW 3
  //
  UINT16 Cid;               // Command Identifier
  UINT16 Pt:1;              // Phase Tag
  UINT16 Sc:8;              // Status Code
  UINT16 Sct:3;             // Status Code Type
  UINT16 Rsvd2:2;
  UINT16 Mo:1;              // More
  UINT16 Dnr:1;             // Do Not Retry
} NVME_CQ;

//
// Nvm Express Admin cmd opcodes
//
#define NVME_ADMIN_DEIOSQ_CMD                0x00
#define NVME_ADMIN_CRIOSQ_CMD                0x01
#define NVME_ADMIN_GET_LOG_PAGE_CMD          0x02
#define NVME_ADMIN_DEIOCQ_CMD                0x04
#define NVME_ADMIN_CRIOCQ_CMD                0x05
#define NVME_ADMIN_IDENTIFY_CMD              0x06
#define NVME_ADMIN_ABORT_CMD                 0x08
#define NVME_ADMIN_SET_FEATURES_CMD          0x09
#define NVME_ADMIN_GET_FEATURES_CMD          0x0A
#define NVME_ADMIN_ASYNC_EVENT_REQUEST_CMD   0x0C
#define NVME_ADMIN_NAMESACE_MANAGEMENT_CMD   0x0D
#define NVME_ADMIN_FW_COMMIT_CMD             0x10
#define NVME_ADMIN_FW_IAMGE_DOWNLOAD_CMD     0x11
#define NVME_ADMIN_NAMESACE_ATTACHMENT_CMD   0x15
#define NVME_ADMIN_FORMAT_NVM_CMD            0x80
#define NVME_ADMIN_SECURITY_SEND_CMD         0x81
#define NVME_ADMIN_SECURITY_RECEIVE_CMD      0x82

#define NVME_IO_FLUSH_OPC                    0
#define NVME_IO_WRITE_OPC                    1
#define NVME_IO_READ_OPC                     2

typedef enum {
  DeleteIOSubmissionQueueOpcode = NVME_ADMIN_DEIOSQ_CMD,
  CreateIOSubmissionQueueOpcode = NVME_ADMIN_CRIOSQ_CMD,
  GetLogPageOpcode = NVME_ADMIN_GET_LOG_PAGE_CMD,
  DeleteIOCompletionQueueOpcode = NVME_ADMIN_DEIOCQ_CMD,
  CreateIOCompletionQueueOpcode = NVME_ADMIN_CRIOCQ_CMD,
  IdentifyOpcode = NVME_ADMIN_IDENTIFY_CMD,
  AbortOpcode = NVME_ADMIN_ABORT_CMD,
  SetFeaturesOpcode = NVME_ADMIN_SET_FEATURES_CMD,
  GetFeaturesOpcode = NVME_ADMIN_GET_FEATURES_CMD,
  AsyncEventRequestOpcode = NVME_ADMIN_ASYNC_EVENT_REQUEST_CMD,
  NamespaceManagementOpcode = NVME_ADMIN_NAMESACE_MANAGEMENT_CMD,
  FirmwareCommitOpcode = NVME_ADMIN_FW_COMMIT_CMD,
  FirmwareImageDownloadOpcode = NVME_ADMIN_FW_IAMGE_DOWNLOAD_CMD,
  NamespaceAttachmentOpcode = NVME_ADMIN_NAMESACE_ATTACHMENT_CMD,
  FormatNvmOpcode = NVME_ADMIN_FORMAT_NVM_CMD,
  SecuritySendOpcode = NVME_ADMIN_SECURITY_SEND_CMD,
  SecurityReceiveOpcode = NVME_ADMIN_SECURITY_RECEIVE_CMD
} NVME_ADMIN_COMMAND_OPCODE;

//
// Controller or Namespace Structure (CNS) field
// (ref. spec. v1.1 figure 82).
//
typedef enum {
IdentifyNamespaceCns = 0x0,
IdentifyControllerCns = 0x1,
IdentifyActiveNsListCns = 0x2
} NVME_ADMIN_IDENTIFY_CNS;

//
// Commit Action
// (ref. spec. 1.1 figure 60).
//
typedef enum {
  ActivateActionReplace = 0x0,
  ActivateActionReplaceActivate = 0x1,
  ActivateActionActivate = 0x2
} NVME_FW_ACTIVATE_ACTION;

//
// Firmware Slot
// (ref. spec. 1.1 Figure 60).
//
typedef enum {
  FirmwareSlotCtrlChooses = 0x0,
  FirmwareSlot1 = 0x1,
  FirmwareSlot2 = 0x2,
  FirmwareSlot3 = 0x3,
  FirmwareSlot4 = 0x4,
  FirmwareSlot5 = 0x5,
  FirmwareSlot6 = 0x6,
  FirmwareSlot7 = 0x7
} NVME_FW_ACTIVATE_SLOT;

//
// Get Log Page ? Log Page Identifiers
// (ref. spec. v1.1 Figure 73).
//
typedef enum {
  ErrorInfoLogID = LID_ERROR_INFO,
  SmartHealthInfoLogID = LID_SMART_INFO,
  FirmwareSlotInfoLogID = LID_FW_SLOT_INFO
} NVME_LOG_ID;

//
// Get Log Page ? Firmware Slot Information Log
// (ref. spec. v1.1 Figure 77).
//
typedef struct {
  //
  // Indicates the firmware slot from which the actively running firmware revision was loaded.
  //
  UINT8 ActivelyRunningFwSlot:3;
  UINT8 :1;
  //
  // Indicates the firmware slot that is going to be activated at the next controller reset. If this field is 0h, then the controller does not indicate the firmware slot that is going to be activated at the next controller reset.
  //
  UINT8 NextActiveFwSlot:3;
  UINT8 :1;
} NVME_ACTIVE_FW_INFO;

//
// Get Log Page ? Firmware Slot Information Log
// (ref. spec. v1.1 Figure 77).
//
typedef struct {
  //
  // Specifies information about the active firmware revision.
  //s
  NVME_ACTIVE_FW_INFO  ActiveFwInfo;
  UINT8                Reserved1[7];
  //
  // Contains the revision of the firmware downloaded to firmware slot 1/7. If no valid firmware revision is present or if this slot is unsupported, all zeros shall be returned.
  //
  CHAR8                FwRevisionSlot[7][8];
  UINT8                Reserved2[448];
} NVME_FW_SLOT_INFO_LOG;

//
// SMART / Health Information (Log Identifier 02h)
// (ref. spec. v1.1 5.10.1.2)
//
typedef struct {
  //
  // This field indicates critical warnings for the state of the controller.
  //
  UINT8  CriticalWarningAvailableSpare:1;
  UINT8  CriticalWarningTemperature:1;
  UINT8  CriticalWarningReliability:1;
  UINT8  CriticalWarningMediaReadOnly:1;
  UINT8  CriticalWarningVolatileBackup:1;
  UINT8  CriticalWarningReserved:3;
  //
  // Contains a value corresponding to a temperature in degrees Kelvin that represents the current composite temperature of the controller and namespace(s) associated with that controller. The manner in which this value is computed is implementation specific and may not represent the actual temperature of any physical point in the NVM subsystem.
  //
  UINT16 CompositeTemp;
  //
  // Contains a normalized percentage (0 to 100%) of the remaining spare capacity available.
  //
  UINT8  AvailableSpare;
  //
  // When the Available Spare falls below the threshold indicated in this field, an asynchronous event completion may occur. The value is indicated as a normalized percentage (0 to 100%).
  //
  UINT8  AvailableSpareThreshold;
  //
  // Contains a vendor specific estimate of the percentage of NVM subsystem life used based on the actual usage and the manufacturer's prediction of NVM life. A value of 100 indicates that the estimated endurance of the NVM in the NVM subsystem has been consumed, but may not indicate an NVM subsystem failure. The value is allowed to exceed 100. Percentages greater than 254 shall be represented as 255. This value shall be updated once per power-on hour (when the controller is not in a sleep state).
  //
  UINT8  PercentageUsed;
  UINT8  Reserved1[26];
  //
  // Contains the number of 512 byte data units the host has read from the controller; this value does not include metadata.
  //
  UINT8  DataUnitsRead[16];
  //
  // Contains the number of 512 byte data units the host has written to the controller; this value does not include metadata.
  //
  UINT8  DataUnitsWritten[16];
  //
  // Contains the number of read commands completed by the controller.
  //
  UINT8  HostReadCommands[16];
  //
  // Contains the number of write commands completed by the controller.
  //
  UINT8  HostWriteCommands[16];
  //
  // Contains the amount of time the controller is busy with I/O commands. This value is reported in minutes.
  //
  UINT8  ControllerBusyTime[16];
  //
  // Contains the number of power cycles.
  //
  UINT8  PowerCycles[16];
  //
  // Contains the number of power-on hours.
  //
  UINT8  PowerOnHours[16];
  //
  // Contains the number of unsafe shutdowns.
  //
  UINT8  UnsafeShutdowns[16];
  //
  // Contains the number of occurrences where the controller detected an unrecovered data integrity error.
  //
  UINT8  MediaAndDataIntegrityErrors[16];
  //
  // Contains the number of Error Information log entries over the life of the controller.
  //
  UINT8  NumberErrorInformationLogEntries[16];
  //
  // Contains the amount of time in minutes that the controller is operational and the Composite Temperature is greater than or equal to the Warning Composite Temperature Threshold (WCTEMP) field and less than the Critical Composite Temperature Threshold (CCTEMP) field in the Identify Controller data structure in Figure 90.
  //
  UINT32 WarningCompositeTemperatureTime;
  //
  // Contains the amount of time in minutes that the controller is operational and the Composite Temperature is greater the Critical Composite Temperature Threshold (CCTEMP) field in the Identify Controller data structure in Figure 90.
  //
  UINT32 CriticalCompositeTemperatureTime;
  //
  // Contains the current temperature in degrees Kelvin reported by the temperature sensor.  An implementation that does not implement the temperature sensor reports a temperature of zero degrees Kelvin.
  //
  UINT16 TemperatureSensor[8];
  UINT8  Reserved2[296];
} NVME_SMART_HEALTH_INFO_LOG;

#pragma pack()

#endif
