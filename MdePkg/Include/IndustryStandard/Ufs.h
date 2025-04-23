/*++ @file

  Common definitions for Universal Flash Storage (UFS)

  Copyright (c) Microsoft Corporation. All rights reserved.
  Copyright (c) 2014 - 2018, Intel Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  JESD220 - Universal Flash Storage (UFS)
  Version 4.0
  https://www.jedec.org/system/files/docs/JESD220F.pdf
--*/

#ifndef __UFS_H__
#define __UFS_H__

#include <Base.h>

#define UFS_LUN_0             0x00
#define UFS_LUN_1             0x01
#define UFS_LUN_2             0x02
#define UFS_LUN_3             0x03
#define UFS_LUN_4             0x04
#define UFS_LUN_5             0x05
#define UFS_LUN_6             0x06
#define UFS_LUN_7             0x07
#define UFS_WLUN_REPORT_LUNS  0x81
#define UFS_WLUN_UFS_DEV      0xD0
#define UFS_WLUN_BOOT         0xB0
#define UFS_WLUN_RPMB         0xC4

#pragma pack(1)

//
// UFS 4.0 Spec Table 10.13 - UTP Command UPIU
//
typedef struct {
  //
  // DW0
  //
  UINT8     TransCode : 6;    /* Transaction Type - 0x01*/
  UINT8     Dd        : 1;
  UINT8     Hd        : 1;
  UINT8     Flags;
  UINT8     Lun;
  UINT8     TaskTag;          /* Task Tag */

  //
  // DW1
  //
  UINT8     CmdSet    : 4;    /* Command Set Type */
  UINT8     Iid       : 4;    /* Initiator ID */
  UINT8     Rsvd1;
  UINT8     Rsvd2;
  UINT8     Rsvd3     : 4;
  UINT8     Ext_Iid   : 4;    /* Initiator ID Extended */

  //
  // DW2
  //
  UINT8     EhsLen;           /* Total EHS Length - 0x00 */
  UINT8     Rsvd4;
  UINT16    DataSegLen;       /* Data Segment Length - Big Endian - 0x0000 */

  //
  // DW3
  //
  UINT32    ExpDataTranLen;   /* Expected Data Transfer Length - Big Endian */

  //
  // DW4 - DW7
  //
  UINT8     Cdb[16];
} UTP_COMMAND_UPIU;

//
// UFS 4.0 Spec Table 10.15 - UTP Response UPIU
//
typedef struct {
  //
  // DW0
  //
  UINT8     TransCode : 6;    /* Transaction Type - 0x21*/
  UINT8     Dd        : 1;
  UINT8     Hd        : 1;
  UINT8     Flags;
  UINT8     Lun;
  UINT8     TaskTag;          /* Task Tag */

  //
  // DW1
  //
  UINT8     CmdSet    : 4;    /* Command Set Type */
  UINT8     Iid       : 4;    /* Initiator ID */
  UINT8     Rsvd1     : 4;
  UINT8     Ext_Iid   : 4;    /* Initiator ID Extended */
  UINT8     Response;         /* Response */
  UINT8     Status;           /* Status */

  //
  // DW2
  //
  UINT8     EhsLen;           /* Total EHS Length - 0x00 */
  UINT8     DevInfo;          /* Device Information */
  UINT16    DataSegLen;       /* Data Segment Length - Big Endian */

  //
  // DW3
  //
  UINT32    ResTranCount;     /* Residual Transfer Count - Big Endian */

  //
  // DW4 - DW7
  //
  UINT8     Rsvd2[16];

  //
  // Data Segment - Sense Data
  //
  UINT16    SenseDataLen;     /* Sense Data Length - Big Endian */
  UINT8     SenseData[18];    /* Sense Data */
} UTP_RESPONSE_UPIU;

//
// UFS 4.0 Spec Table 10.21 - UTP Data-Out UPIU
//
typedef struct {
  //
  // DW0
  //
  UINT8     TransCode : 6;    /* Transaction Type - 0x02*/
  UINT8     Dd        : 1;
  UINT8     Hd        : 1;
  UINT8     Flags;
  UINT8     Lun;
  UINT8     TaskTag;          /* Task Tag */

  //
  // DW1
  //
  UINT8     Rsvd1     : 4;
  UINT8     Iid       : 4;    /* Initiator ID */
  UINT8     Rsvd2[2];
  UINT8     Rsvd3     : 4;
  UINT8     Ext_Iid   : 4;    /* Initiator ID Extended */

  //
  // DW2
  //
  UINT8     EhsLen;           /* Total EHS Length - 0x00 */
  UINT8     Rsvd4;
  UINT16    DataSegLen;       /* Data Segment Length - Big Endian */

  //
  // DW3
  //
  UINT32    DataBufOffset;    /* Data Buffer Offset - Big Endian */

  //
  // DW4
  //
  UINT32    DataTranCount;    /* Data Transfer Count - Big Endian */

  //
  // DW5 - DW7
  //
  UINT8     Rsvd5[12];

  //
  // Data Segment - Data to be sent out
  //
  // UINT8  Data[];            /* Data to be sent out, maximum is 65535 bytes */
} UTP_DATA_OUT_UPIU;

//
// UFS 4.0 Spec Table 10.23 - UTP Data-In UPIU
//
typedef struct {
  //
  // DW0
  //
  UINT8     TransCode : 6;    /* Transaction Type - 0x22*/
  UINT8     Dd        : 1;
  UINT8     Hd        : 1;
  UINT8     Flags;
  UINT8     Lun;
  UINT8     TaskTag;          /* Task Tag */

  //
  // DW1
  //
  UINT8     Rsvd1     : 4;
  UINT8     Iid       : 4;    /* Initiator ID */
  UINT8     Rsvd2     : 4;
  UINT8     Ext_Iid   : 4;    /* Initiator ID Extended */
  UINT8     Rsvd3[2];

  //
  // DW2
  //
  UINT8     EhsLen;           /* Total EHS Length - 0x00 */
  UINT8     Rsvd4;
  UINT16    DataSegLen;       /* Data Segment Length - Big Endian */

  //
  // DW3
  //
  UINT32    DataBufOffset;    /* Data Buffer Offset - Big Endian */

  //
  // DW4
  //
  UINT32    DataTranCount;    /* Data Transfer Count - Big Endian */

  //
  // DW5
  //
  UINT8     HintControl : 4;  /* Hint Control */
  UINT8     Rsvd5       : 4;
  UINT8     HintIid     : 4;  /* Hint Initiator ID */
  UINT8     HintExt_Iid : 4;  /* Hint Initiator ID Extended */
  UINT8     HintLun;          /* Hint LUN */
  UINT8     HintTaskTag;      /* Hint Task Tag */

  //
  // DW6
  //
  UINT32    HintDataBufOffset;  /* Hint Data Buffer Offset - Big Endian */

  //
  // DW7
  //
  UINT32    HintDataCount;    /* Hint Data Count - Big Endian */

  //
  // Data Segment - Data to be read
  //
  // UINT8  Data[];            /* Data to be read, maximum is 65535 bytes */
} UTP_DATA_IN_UPIU;

//
// UFS 4.0 Spec Table 10.25 - UTP Ready-To-Transfer UPIU
//
typedef struct {
  //
  // DW0
  //
  UINT8     TransCode : 6;    /* Transaction Type - 0x31*/
  UINT8     Dd        : 1;
  UINT8     Hd        : 1;
  UINT8     Flags;
  UINT8     Lun;
  UINT8     TaskTag;          /* Task Tag */

  //
  // DW1
  //
  UINT8     Rsvd1     : 4;
  UINT8     Iid       : 4;    /* Initiator ID */
  UINT8     Rsvd2     : 4;
  UINT8     Ext_Iid   : 4;    /* Initiator ID Extended */
  UINT8     Rsvd3[2];

  //
  // DW2
  //
  UINT8     EhsLen;           /* Total EHS Length - 0x00 */
  UINT8     Rsvd4;
  UINT16    DataSegLen;       /* Data Segment Length - Big Endian - 0x0000 */

  //
  // DW3
  //
  UINT32    DataBufOffset;    /* Data Buffer Offset - Big Endian */

  //
  // DW4
  //
  UINT32    DataTranCount;    /* Data Transfer Count - Big Endian */

  //
  // DW5
  //
  UINT8     HintControl : 4;  /* Hint Control */
  UINT8     Rsvd5       : 4;
  UINT8     HintIid     : 4;  /* Hint Initiator ID */
  UINT8     HintExt_Iid : 4;  /* Hint Initiator ID Extended */
  UINT8     HintLun;          /* Hint LUN */
  UINT8     HintTaskTag;      /* Hint Task Tag */

  //
  // DW6
  //
  UINT32    HintDataBufOffset;  /* Hint Data Buffer Offset - Big Endian */

  //
  // DW7
  //
  UINT32    HintDataCount;    /* Hint Data Count - Big Endian */

  //
  // Data Segment - Data to be read
  //
  // UINT8  Data[];            /* Data to be read, maximum is 65535 bytes */
} UTP_RDY_TO_TRAN_UPIU;

//
// UFS 4.0 Spec Table 10.27 - UTP Task Management Request UPIU
//
typedef struct {
  //
  // DW0
  //
  UINT8     TransCode : 6;    /* Transaction Type - 0x04*/
  UINT8     Dd        : 1;
  UINT8     Hd        : 1;
  UINT8     Flags;
  UINT8     Lun;
  UINT8     TaskTag;          /* Task Tag */

  //
  // DW1
  //
  UINT8     Rsvd1     : 4;
  UINT8     Iid       : 4;    /* Initiator ID */
  UINT8     TskManFunc;       /* Task Management Function */
  UINT8     Rsvd2;
  UINT8     Rsvd3     : 4;
  UINT8     Ext_Iid   : 4;    /* Initiator ID Extended */

  //
  // DW2
  //
  UINT8     EhsLen;           /* Total EHS Length - 0x00 */
  UINT8     Rsvd4;
  UINT16    DataSegLen;       /* Data Segment Length - Big Endian - 0x0000 */

  //
  // DW3
  //
  UINT32    InputParam1;      /* Input Parameter 1 - Big Endian */

  //
  // DW4
  //
  UINT32    InputParam2;      /* Input Parameter 2 - Big Endian */

  //
  // DW5
  //
  UINT32    InputParam3;      /* Input Parameter 3 - Big Endian */

  //
  // DW6 - DW7
  //
  UINT8     Rsvd5[8];
} UTP_TM_REQ_UPIU;

//
// UFS 4.0 Spec Table 10.30 - UTP Task Management Response UPIU
//
typedef struct {
  //
  // DW0
  //
  UINT8     TransCode : 6;    /* Transaction Type - 0x24*/
  UINT8     Dd        : 1;
  UINT8     Hd        : 1;
  UINT8     Flags;
  UINT8     Lun;
  UINT8     TaskTag;          /* Task Tag */

  //
  // DW1
  //
  UINT8     Rsvd1     : 4;
  UINT8     Iid       : 4;    /* Initiator ID */
  UINT8     Rsvd2     : 4;
  UINT8     Ext_Iid   : 4;    /* Initiator ID Extended */
  UINT8     Resp;             /* Response */
  UINT8     Rsvd3;

  //
  // DW2
  //
  UINT8     EhsLen;           /* Total EHS Length - 0x00 */
  UINT8     Rsvd4;
  UINT16    DataSegLen;       /* Data Segment Length - Big Endian - 0x0000 */

  //
  // DW3
  //
  UINT32    OutputParam1;     /* Output Parameter 1 - Big Endian */

  //
  // DW4
  //
  UINT32    OutputParam2;     /* Output Parameter 2 - Big Endian */

  //
  // DW5 - DW7
  //
  UINT8     Rsvd5[12];
} UTP_TM_RESP_UPIU;

//
// UFS 4.0 Spec Table 10.35 - 10.57 - Transaction Specific Fields for (Genericized) Opcode
//
typedef struct {
  UINT8     Opcode;
  UINT8     DescId;
  UINT8     Index;
  UINT8     Selector;
  UINT16    Rsvd1;
  UINT16    Length;
  UINT32    Value;
  UINT32    Rsvd2;
} UTP_UPIU_TSF;

//
// UFS 4.0 Spec Table 10.33 - UTP Query Request UPIU
//
typedef struct {
  //
  // DW0
  //
  UINT8           TransCode : 6; /* Transaction Type - 0x16*/
  UINT8           Dd        : 1;
  UINT8           Hd        : 1;
  UINT8           Flags;
  UINT8           Rsvd1;
  UINT8           TaskTag;    /* Task Tag */

  //
  // DW1
  //
  UINT8           Rsvd2;
  UINT8           QueryFunc;  /* Query Function */
  UINT8           Rsvd3[2];

  //
  // DW2
  //
  UINT8           EhsLen;     /* Total EHS Length - 0x00 */
  UINT8           Rsvd4;
  UINT16          DataSegLen; /* Data Segment Length - Big Endian */

  //
  // DW3 - 6
  //
  UTP_UPIU_TSF    Tsf;        /* Transaction Specific Fields */

  //
  // DW7
  //
  UINT8           Rsvd5[4];

  //
  // Data Segment - Data to be transferred
  //
  // UINT8  Data[];            /* Data to be transferred, maximum is 65535 bytes */
} UTP_QUERY_REQ_UPIU;

#define QUERY_FUNC_STD_READ_REQ   0x01
#define QUERY_FUNC_STD_WRITE_REQ  0x81

//
// UFS 4.0 Spec Table 10.36 - Query Function opcode values
//
typedef enum {
  UtpQueryFuncOpcodeNop     = 0x00,
  UtpQueryFuncOpcodeRdDesc  = 0x01,
  UtpQueryFuncOpcodeWrDesc  = 0x02,
  UtpQueryFuncOpcodeRdAttr  = 0x03,
  UtpQueryFuncOpcodeWrAttr  = 0x04,
  UtpQueryFuncOpcodeRdFlag  = 0x05,
  UtpQueryFuncOpcodeSetFlag = 0x06,
  UtpQueryFuncOpcodeClrFlag = 0x07,
  UtpQueryFuncOpcodeTogFlag = 0x08
} UTP_QUERY_FUNC_OPCODE;

//
// UFS 4.0 Spec Table 10.46 - UTP Query Response UPIU
//
typedef struct {
  //
  // DW0
  //
  UINT8           TransCode : 6; /* Transaction Type - 0x36*/
  UINT8           Dd        : 1;
  UINT8           Hd        : 1;
  UINT8           Flags;
  UINT8           Rsvd1;
  UINT8           TaskTag;    /* Task Tag */

  //
  // DW1
  //
  UINT8           Rsvd2;
  UINT8           QueryFunc;  /* Query Function */
  UINT8           QueryResp;  /* Query Response */
  UINT8           Rsvd3;

  //
  // DW2
  //
  UINT8           EhsLen;     /* Total EHS Length - 0x00 */
  UINT8           DevInfo;    /* Device Information */
  UINT16          DataSegLen; /* Data Segment Length - Big Endian */

  //
  // DW3 - 6
  //
  UTP_UPIU_TSF    Tsf;        /* Transaction Specific Fields */

  //
  // DW7
  //
  UINT8           Rsvd4[4];

  //
  // Data Segment - Data to be transferred
  //
  // UINT8      Data[];        /* Data to be transferred, maximum is 65535 bytes */
} UTP_QUERY_RESP_UPIU;

//
// UFS 4.0 Spec Table 10.47 - Query Response Code
//
typedef enum {
  UfsUtpQueryResponseSuccess             = 0x00,
  UfsUtpQueryResponseParamNotReadable    = 0xF6,
  UfsUtpQueryResponseParamNotWriteable   = 0xF7,
  UfsUtpQueryResponseParamAlreadyWritten = 0xF8,
  UfsUtpQueryResponseInvalidLen          = 0xF9,
  UfsUtpQueryResponseInvalidVal          = 0xFA,
  UfsUtpQueryResponseInvalidSelector     = 0xFB,
  UfsUtpQueryResponseInvalidIndex        = 0xFC,
  UfsUtpQueryResponseInvalidIdn          = 0xFD,
  UfsUtpQueryResponseInvalidOpc          = 0xFE,
  UfsUtpQueryResponseGeneralFailure      = 0xFF
} UTP_QUERY_RESP_CODE;

//
// UFS 4.0 Spec Table 10.58 - UTP Reject UPIU
//
typedef struct {
  //
  // DW0
  //
  UINT8     TransCode : 6;    /* Transaction Type - 0x3F*/
  UINT8     Dd        : 1;
  UINT8     Hd        : 1;
  UINT8     Flags;
  UINT8     Lun;
  UINT8     TaskTag;          /* Task Tag */

  //
  // DW1
  //
  UINT8     Rsvd1     : 4;
  UINT8     Iid       : 4;    /* Initiator ID */
  UINT8     Rsvd2     : 4;
  UINT8     Ext_Iid   : 4;    /* Initiator ID Extended */
  UINT8     Response;         /* Response - 0x01 */
  UINT8     Rsvd3;

  //
  // DW2
  //
  UINT8     EhsLen;           /* Total EHS Length - 0x00 */
  UINT8     DevInfo;          /* Device Information - 0x00 */
  UINT16    DataSegLen;       /* Data Segment Length - Big Endian - 0x0000 */

  //
  // DW3
  //
  UINT8     HdrSts;           /* Basic Header Status */
  UINT8     Rsvd4;
  UINT8     E2ESts;           /* End-to-End Status */
  UINT8     Rsvd5;

  //
  // DW4 - DW7
  //
  UINT8     Rsvd6[16];
} UTP_REJ_UPIU;

//
// UFS 4.0 Spec Table 10.61 - UTP NOP OUT UPIU
//
typedef struct {
  //
  // DW0
  //
  UINT8     TransCode : 6;    /* Transaction Type - 0x00*/
  UINT8     Dd        : 1;
  UINT8     Hd        : 1;
  UINT8     Flags;
  UINT8     Rsvd1;
  UINT8     TaskTag;          /* Task Tag */

  //
  // DW1
  //
  UINT8     Rsvd2[4];

  //
  // DW2
  //
  UINT8     EhsLen;           /* Total EHS Length - 0x00 */
  UINT8     Rsvd3;
  UINT16    DataSegLen;       /* Data Segment Length - Big Endian - 0x0000 */

  //
  // DW3 - DW7
  //
  UINT8     Rsvd4[20];
} UTP_NOP_OUT_UPIU;

//
// UFS 4.0 Spec Table 10.62 - UTP NOP IN UPIU
//
typedef struct {
  //
  // DW0
  //
  UINT8     TransCode : 6;    /* Transaction Type - 0x20*/
  UINT8     Dd        : 1;
  UINT8     Hd        : 1;
  UINT8     Flags;
  UINT8     Rsvd1;
  UINT8     TaskTag;          /* Task Tag */

  //
  // DW1
  //
  UINT8     Rsvd2[2];
  UINT8     Resp;             /* Response - 0x00 */
  UINT8     Rsvd3;

  //
  // DW2
  //
  UINT8     EhsLen;           /* Total EHS Length - 0x00 */
  UINT8     DevInfo;          /* Device Information - 0x00 */
  UINT16    DataSegLen;       /* Data Segment Length - Big Endian - 0x0000 */

  //
  // DW3 - DW7
  //
  UINT8     Rsvd4[20];
} UTP_NOP_IN_UPIU;

//
// UFS 4.0 Spec Table 14.1 - Descriptor identification values
//
typedef enum {
  UfsDeviceDesc    = 0x00,
  UfsConfigDesc    = 0x01,
  UfsUnitDesc      = 0x02,
  UfsInterConnDesc = 0x04,
  UfsStringDesc    = 0x05,
  UfsGeometryDesc  = 0x07,
  UfsPowerDesc     = 0x08,
  UfsDevHealthDesc = 0x09
} UFS_DESC_IDN;

//
// UFS 4.0 Spec Table 14.4 - Device Descriptor
//
typedef struct {
  UINT8     Length;
  UINT8     DescType;
  UINT8     Device;
  UINT8     DevClass;
  UINT8     DevSubClass;
  UINT8     Protocol;
  UINT8     NumLun;
  UINT8     NumWLun;
  UINT8     BootEn;
  UINT8     DescAccessEn;
  UINT8     InitPowerMode;
  UINT8     HighPriorityLun;
  UINT8     SecureRemovalType;
  UINT8     SecurityLun;
  UINT8     BgOpsTermLat;
  UINT8     InitActiveIccLevel;
  UINT16    SpecVersion;
  UINT16    ManufactureDate;
  UINT8     ManufacturerName;
  UINT8     ProductName;
  UINT8     SerialName;
  UINT8     OemId;
  UINT16    ManufacturerId;
  UINT8     Ud0BaseOffset;
  UINT8     Ud0ConfParamLen;
  UINT8     DevRttCap;
  UINT16    PeriodicRtcUpdate;
  UINT8     UFSFeaturesSupport; // Deprecated, use ExtendedUFSFeaturesSupport
  UINT8     FFUTimeout;
  UINT8     QueueDepth;
  UINT16    DeviceVersion;
  UINT8     NumSecureWPArea;
  UINT32    PSAMaxDataSize;
  UINT8     PSAStateTimeout;
  UINT8     ProductRevisionLevel;
  UINT8     Rsvd1[5];
  UINT8     Rsvd2[16];
  UINT8     Rsvd3[3];
  UINT8     Rsvd4[12];
  UINT32    ExtendedUFSFeaturesSupport;
  UINT8     WriteBoosterBufPreserveUserSpaceEn;
  UINT8     WriteBoosterBufType;
  UINT32    NumSharedWriteBoosterAllocUnits;
} UFS_DEV_DESC;

//
// UFS 4.0 Spec Table 14.4 (Offset 10h) - Specification version
//
typedef union {
  struct {
    UINT8    Suffix : 4;
    UINT8    Minor  : 4;
    UINT8    Major;
  } Bits;
  UINT16    Data;
} UFS_SPEC_VERSION;

//
// UFS 4.0 Spec Table 14.4 (Offset 4Fh) - Extended UFS Features Support
//
typedef union {
  struct {
    UINT32    FFU                   : 1;
    UINT32    PSA                   : 1;
    UINT32    DeviceLifeSpan        : 1;
    UINT32    RefreshOperation      : 1;
    UINT32    TooHighTemp           : 1;
    UINT32    TooLowTemp            : 1;
    UINT32    ExtendedTemp          : 1;
    UINT32    Rsvd1                 : 1;
    UINT32    WriteBooster          : 1;
    UINT32    PerformanceThrottling : 1;
    UINT32    AdvancedRPMB          : 1;
    UINT32    Rsvd2                 : 3;
    UINT32    Barrier               : 1;
    UINT32    ClearErrorHistory     : 1;
    UINT32    Ext_Iid               : 1;
    UINT32    Rsvd3                 : 1;
    UINT32    Rsvd4                 : 14;
  } Bits;
  UINT32    Data;
} EXTENDED_UFS_FEATURES_SUPPORT;

//
// UFS 4.0 Spec Table 14.10 - Configuration Descriptor Header (INDEX = 0)
//  and Device Descriptor Configuration parameters
//
typedef struct {
  UINT8     Length;
  UINT8     DescType;
  UINT8     ConfDescContinue;
  UINT8     BootEn;
  UINT8     DescAccessEn;
  UINT8     InitPowerMode;
  UINT8     HighPriorityLun;
  UINT8     SecureRemovalType;
  UINT8     InitActiveIccLevel;
  UINT16    PeriodicRtcUpdate;
  UINT8     Rsvd1;
  UINT8     RpmbRegionEnable;
  UINT8     RpmbRegion1Size;
  UINT8     RpmbRegion2Size;
  UINT8     RpmbRegion3Size;
  UINT8     WriteBoosterBufPreserveUserSpaceEn;
  UINT8     WriteBoosterBufType;
  UINT32    NumSharedWriteBoosterAllocUnits;
} UFS_CONFIG_DESC_GEN_HEADER;

//
// UFS 4.0 Spec Table 14.11 - Configuration Descriptor Header (INDEX = 1/2/3)
//
typedef struct {
  UINT8    Length;
  UINT8    DescType;
  UINT8    ConfDescContinue;
  UINT8    Rsvd1[19];
} UFS_CONFIG_DESC_EXT_HEADER;

//
// UFS 4.0 Spec Table 14.12 - UNit Descriptor configurable parameters
//
typedef struct {
  UINT8     LunEn;
  UINT8     BootLunId;
  UINT8     LunWriteProt;
  UINT8     MemType;
  UINT32    NumAllocUnits;
  UINT8     DataReliability;
  UINT8     LogicBlkSize;
  UINT8     ProvisionType;
  UINT16    CtxCap;
  UINT8     Rsvd1[3];
  UINT8     Rsvd2[6];
  UINT32    LuNumWriteBoosterBufAllocUnits;
} UFS_UNIT_DESC_CONFIG_PARAMS;

//
// UFS 4.0 Spec Table 14.6 - Configuration Descriptor Format
//
// WARNING: This struct contains variable-size members! (across spec versions)
// To maintain backward compatibility, UnitDescConfParams should not be
// accessed as a struct member.
// Instead, use `Ud0BaseOffset` and `Ud0ConfParamLen` from the Device
// Descriptor to calculate the offset and location of the Unit Descriptors.
//
typedef struct {
  UFS_CONFIG_DESC_GEN_HEADER     Header;
  UFS_UNIT_DESC_CONFIG_PARAMS    UnitDescConfParams[8];
} UFS_CONFIG_DESC;

//
// UFS 4.0 Spec Table 14.13 - Geometry Descriptor
//
typedef struct {
  UINT8     Length;
  UINT8     DescType;
  UINT8     MediaTech;
  UINT8     Rsvd1;
  UINT64    TotalRawDevCapacity;
  UINT8     MaxNumberLu;
  UINT32    SegSize;
  UINT8     AllocUnitSize;
  UINT8     MinAddrBlkSize;
  UINT8     OptReadBlkSize;
  UINT8     OptWriteBlkSize;
  UINT8     MaxInBufSize;
  UINT8     MaxOutBufSize;
  UINT8     RpmbRwSize;
  UINT8     DynamicCapacityResourcePolicy;
  UINT8     DataOrder;
  UINT8     MaxCtxIdNum;
  UINT8     SysDataTagUnitSize;
  UINT8     SysDataResUnitSize;
  UINT8     SupSecRemovalTypes;
  UINT16    SupMemTypes;
  UINT32    SysCodeMaxNumAllocUnits;
  UINT16    SupCodeCapAdjFac;
  UINT32    NonPersMaxNumAllocUnits;
  UINT16    NonPersCapAdjFac;
  UINT32    Enhance1MaxNumAllocUnits;
  UINT16    Enhance1CapAdjFac;
  UINT32    Enhance2MaxNumAllocUnits;
  UINT16    Enhance2CapAdjFac;
  UINT32    Enhance3MaxNumAllocUnits;
  UINT16    Enhance3CapAdjFac;
  UINT32    Enhance4MaxNumAllocUnits;
  UINT16    Enhance4CapAdjFac;
  UINT32    OptLogicBlkSize;
  UINT8     Rsvd2[5];
  UINT8     Rsvd3[2];
  UINT32    WriteBoosterBufMaxNumAllocUnits;
  UINT8     DeviceMaxWriteBoosterLus;
  UINT8     WriteBoosterBufCapAdjFac;
  UINT8     SupWriteBoosterBufUserSpaceReductionTypes;
  UINT8     SupWriteBoosterBufTypes;
} UFS_GEOMETRY_DESC;

//
// UFS 4.0 Spec Table 14.14 - Unit Descriptor
//
typedef struct {
  UINT8     Length;
  UINT8     DescType;
  UINT8     UnitIdx;
  UINT8     LunEn;
  UINT8     BootLunId;
  UINT8     LunWriteProt;
  UINT8     LunQueueDep;
  UINT8     PsaSensitive;
  UINT8     MemType;
  UINT8     DataReliability;
  UINT8     LogicBlkSize;
  UINT64    LogicBlkCount;
  UINT32    EraseBlkSize;
  UINT8     ProvisionType;
  UINT64    PhyMemResCount;
  UINT16    CtxCap;
  UINT8     LargeUnitGranularity;
  UINT8     Rsvd1[6];
  UINT32    LuNumWriteBoosterBufAllocUnits;
} UFS_UNIT_DESC;

//
// UFS 4.0 Spec Table 14.15 - RPMB Unit Descriptor
//
typedef struct {
  UINT8     Length;
  UINT8     DescType;
  UINT8     UnitIdx;
  UINT8     LunEn;
  UINT8     BootLunId;
  UINT8     LunWriteProt;
  UINT8     LunQueueDep;
  UINT8     PsaSensitive;
  UINT8     MemType;
  UINT8     RpmbRegionEnable;
  UINT8     LogicBlkSize;
  UINT64    LogicBlkCount;
  UINT8     RpmbRegion0Size;
  UINT8     RpmbRegion1Size;
  UINT8     RpmbRegion2Size;
  UINT8     RpmbRegion3Size;
  UINT8     ProvisionType;
  UINT64    PhyMemResCount;
  UINT8     Rsvd3[3];
} UFS_RPMB_UNIT_DESC;

//
// UFS 4.0 Spec Table 7.13 - Format for Power Parameter element
//
typedef struct {
  UINT16    Value : 12;
  UINT16    Rsvd1 : 2;
  UINT16    Unit  : 2;
} UFS_POWER_PARAM_ELEMENT;

//
// UFS 4.0 Spec Table 14.16 - Power Parameters Descriptor
//
typedef struct {
  UINT8                      Length;
  UINT8                      DescType;
  UFS_POWER_PARAM_ELEMENT    ActiveIccLevelVcc[16];
  UFS_POWER_PARAM_ELEMENT    ActiveIccLevelVccQ[16];
  UFS_POWER_PARAM_ELEMENT    ActiveIccLevelVccQ2[16];
} UFS_POWER_DESC;

//
// UFS 4.0 Spec Table 14.17 - Interconnect Descriptor
//
typedef struct {
  UINT8     Length;
  UINT8     DescType;
  UINT16    UniProVer;
  UINT16    MphyVer;
} UFS_INTER_CONNECT_DESC;

//
// UFS 4.0 Spec Table 14.18 - 14.22 - String Descriptor
//
typedef struct {
  UINT8     Length;
  UINT8     DescType;
  CHAR16    Unicode[126];
} UFS_STRING_DESC;

//
// UFS 4.0 Spec Table 14.26 - Flags
//
typedef enum {
  UfsFlagDevInit             = 0x01,
  UfsFlagPermWpEn            = 0x02,
  UfsFlagPowerOnWpEn         = 0x03,
  UfsFlagBgOpsEn             = 0x04,
  UfsFlagDevLifeSpanModeEn   = 0x05,
  UfsFlagPurgeEn             = 0x06,
  UfsFlagRefreshEn           = 0x07,
  UfsFlagPhyResRemoval       = 0x08,
  UfsFlagBusyRtc             = 0x09,
  UfsFlagPermDisFwUpdate     = 0x0B,
  UfsFlagWriteBoosterEn      = 0x0E,
  UfsFlagWbBufFlushEn        = 0x0F,
  UfsFlagWbBufFlushHibernate = 0x10
} UFS_FLAGS_IDN;

//
// UFS 4.0 Spec Table 14.28 - Attributes
//
typedef enum {
  UfsAttrBootLunEn                    = 0x00,
  UfsAttrCurPowerMode                 = 0x02,
  UfsAttrActiveIccLevel               = 0x03,
  UfsAttrOutOfOrderDataEn             = 0x04,
  UfsAttrBgOpStatus                   = 0x05,
  UfsAttrPurgeStatus                  = 0x06,
  UfsAttrMaxDataInSize                = 0x07,
  UfsAttrMaxDataOutSize               = 0x08,
  UfsAttrDynCapNeeded                 = 0x09,
  UfsAttrRefClkFreq                   = 0x0a,
  UfsAttrConfigDescLock               = 0x0b,
  UfsAttrMaxNumOfRtt                  = 0x0c,
  UfsAttrExceptionEvtCtrl             = 0x0d,
  UfsAttrExceptionEvtSts              = 0x0e,
  UfsAttrSecondsPassed                = 0x0f,
  UfsAttrContextConf                  = 0x10,
  UfsAttrDeviceFfuStatus              = 0x14,
  UfsAttrPsaState                     = 0x15,
  UfsAttrPsaDataSize                  = 0x16,
  UfsAttrRefClkGatingWaitTime         = 0x17,
  UfsAttrDeviceCaseRoughTemp          = 0x18,
  UfsAttrDeviceTooHighTempBound       = 0x19,
  UfsAttrDeviceTooLowTempBound        = 0x1a,
  UfsAttrThrottlingStatus             = 0x1b,
  UfsAttrWriteBoosterBufFlushStatus   = 0x1c,
  UfsAttrAvailableWriteBoosterBufSize = 0x1d,
  UfsAttrWriteBoosterBufLifeTimeEst   = 0x1e,
  UfsAttrCurrentWriteBoosterBufSize   = 0x1f,
  UfsAttrExtIidEn                     = 0x2a,
  UfsAttrHostHintCacheSize            = 0x2b,
  UfsAttrRefreshStatus                = 0x2c,
  UfsAttrRefreshFreq                  = 0x2d,
  UfsAttrRefreshUnit                  = 0x2e,
  UfsAttrRefreshMethod                = 0x2f,
  UfsAttrTimestamp                    = 0x30
} UFS_ATTR_IDN;

#pragma pack()

#endif
