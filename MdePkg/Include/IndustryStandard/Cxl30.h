/** @file
  CXL 3.0 Register definitions

  This file contains the register definitions based on the Compute Express Link
  (CXL) Specification Revision 3.0.

  Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CXL30_H_
#define CXL30_H_

#include <IndustryStandard/Cxl20.h>

//
// CXL Cache Memory Capability IDs
// Compute Express Link Specification Revision 3.0 - Chapter 8.2.4 Table 8-22
//
#define CXL_CACHE_MEM_CAPABILITY_ID_TIMEOUT_AND_ISOLATION  0x0009
#define CXL_CACHE_MEM_CAPABILITY_ID_EXTENDED               0x000A
#define CXL_CACHE_MEM_CAPABILITY_ID_BI_ROUTE_TABLE         0x000B
#define CXL_CACHE_MEM_CAPABILITY_ID_BI_DECODER             0x000C
#define CXL_CACHE_MEM_CAPABILITY_ID_CACHE_ID_ROUTE_TABLE   0x000D
#define CXL_CACHE_MEM_CAPABILITY_ID_CACHE_ID_DECODER       0x000E
#define CXL_CACHE_MEM_CAPABILITY_ID_EXTENDED_HDM_DECODER   0x000F

//
// CXL_Capability_Version
// Compute Express ink Specification Revision 3.0 - Chapter 8.2.4.5
//
#define CXL_HDM_DECODER_VERSION_30  0x3

//
// CXL CXL HDM Decoder n Control
// Compute Express Link Specification Revision 3.0 - 8.2.4.19.7
//
//
// Bit4..7: Interleave Ways (IW)
//
#define CXL_HDM_16_WAY_INTERLEAVING  0x4
#define CXL_HDM_3_WAY_INTERLEAVING   0x8
#define CXL_HDM_6_WAY_INTERLEAVING   0x9
#define CXL_HDM_12_WAY_INTERLEAVING  0xA

//
// Ensure proper structure formats
//
#pragma pack(1)

//
// CXL.cachemem Extended Register Capability
// Compute Express Link Specification Revision 3.0  - Chapter 8.2.4.24
//
typedef union {
  struct {
    UINT32    ExtendedRangesBitmap : 16;      // Bit 0..15
    UINT32    Reserved             : 16;      // Bit 16..31
  } Bits;
  UINT32    Uint32;
} CXL_CM_EXTENTED_REGISTER_CAPABILITY;

#define CXL_CM_EXTENTED_RANGES_BITMAP  (BIT2 | BIT3 | BIT4 | BIT5 | BIT6 | BIT7 | BIT8 | BIT9 | BIT10 | BIT11 | BIT12 | BIT13 | BIT15)

//
// CXL BI Route Table Capability
// Compute Express Link Specification Revision 3.0  - Chapter 8.2.4.25
//
typedef union {
  struct {
    UINT32    ExplicitBiRtCommitRequired : 1;                  // bit 0
    UINT32    Reserved                   : 31;                 // bit 1..31
  } Bits;
  UINT32    Uint32;
} CXL_BI_RT_CAPABILITY;

typedef union {
  struct {
    UINT32    BiRtCommit : 1;                                   // bit 0
    UINT32    Reserved   : 31;                                  // bit 1..31
  } Bits;
  UINT32    Uint32;
} CXL_BI_RT_CONTROL;

typedef union {
  struct {
    UINT32    BiRtCommitted          : 1;                      // bit 0
    UINT32    BiRtErrorNotCommitted  : 1;                      // bit 1
    UINT32    Reserved1              : 6;                      // bit 2..7
    UINT32    BiRtCommitTimeoutScale : 4;                      // bit 8..11
    UINT32    BiRtCommitTimeoutBase  : 4;                      // bit 12..15
    UINT32    Reserved2              : 16;                     // bit 16..31
  } Bits;
  UINT32    Uint32;
} CXL_BI_RT_STATUS;

typedef struct {
  CXL_BI_RT_CAPABILITY    BiRtCap;                              // offset 0x00
  CXL_BI_RT_CONTROL       BiRtControl;                          // offset 0x04
  CXL_BI_RT_STATUS        BiRtStatus;                           // offset 0x08
} CXL_BI_ROUTE_TABLE_CAPABILITY;

//
// CXL BI Decoder Capability
// Compute Express Link Specification Revision 3.0  - Chapter 8.2.4.26
//
typedef union {
  struct {
    UINT32    HdmDCapable                     : 1;             // bit 0
    UINT32    ExplicitBiDecoderCommitRequired : 1;             // bit 1
    UINT32    Reserved                        : 30;            // bit 2..31
  } Bits;
  UINT32    Uint32;
} CXL_BI_DECODER_CAP;

typedef union {
  struct {
    UINT32    BiForward       : 1;                             // bit 0
    UINT32    BiEnable        : 1;                             // bit 1
    UINT32    BiDecoderCommit : 1;                             // bit 2
    UINT32    Reserved        : 29;                            // bit 3..31
  } Bits;
  UINT32    Uint32;
} CXL_BI_DECODER_CONTROL;

typedef union {
  struct {
    UINT32    BiDecoderCommitted          : 1;                 // bit 0
    UINT32    BiDecoderErrorNotCommitted  : 1;                 // bit 1
    UINT32    Reserved1                   : 6;                 // bit 2..7
    UINT32    BiDecoderCommitTimeoutScale : 4;                 // bit 8..11
    UINT32    BiDecoderCommitTimeoutBase  : 4;                 // bit 12..15
    UINT32    Reserved2                   : 16;                // bit 16..31
  } Bits;
  UINT32    Uint32;
} CXL_BI_DECODER_STATUS;

typedef struct {
  CXL_BI_DECODER_CAP        BiDecoderCap;                       // offset 0x00
  CXL_BI_DECODER_CONTROL    BiDecoderControl;                   // offset 0x04
  CXL_BI_DECODER_STATUS     BiDecoderStatus;                    // offset 0x08
} CXL_BI_DECODER_CAPABILITY;

//
// CXL Cache ID Route Table Capability
// Compute Express Link Specification Revision 3.0  - Chapter 8.2.4.27
//
typedef union {
  struct {
    UINT32    CacheIdTargetCount              : 5;            // Bit 0..4
    UINT32    Reserved1                       : 3;            // Bit 5..7
    UINT32    HdmDType2DeviceMaxCount         : 4;            // Bit 8..11
    UINT32    Reserved2                       : 4;            // Bit 12..15
    UINT32    ExplicitCacheIdRtCommitRequired : 1;            // Bit 16
    UINT32    Reserved3                       : 15;           // Bit 17:31
  } Bits;
  UINT32    Uint32;
} CXL_CACHE_ID_RT_CAPABILITY;

typedef union {
  struct {
    UINT32    CacheIdRtCommit : 1;                   // Bit 0
    UINT32    Reserved        : 31;                  // Bit 1..31
  } Bits;
  UINT32    Uint32;
} CXL_CACHE_ID_RT_CONTROL;

typedef union {
  struct {
    UINT32    CacheIdRtCommitted          : 1;       // Bit 0
    UINT32    CacheIdRtErrNotCommitted    : 1;       // Bit 1
    UINT32    Reserved1                   : 6;       // Bit 2..7
    UINT32    CacheIdRtCommitTimeoutScale : 4;       // Bit 8..11
    UINT32    CacheIdRtCommitTimeoutBase  : 4;       // Bit 12..15
    UINT32    Reserved2                   : 16;      // Bit 16..31
  } Bits;
  UINT32    Uint32;
} CXL_CACHE_ID_RT_STATUS;

typedef union {
  struct {
    UINT16    Valid      : 1;                       // Bit 0
    UINT16    Reserved   : 7;                       // Bit 1..7
    UINT16    PortNumber : 8;                       // Bit 8..15
  } Bits;
  UINT16    Uint16;
} CXL_CACHE_ID_RT_TARGET;

typedef struct {
  CXL_CACHE_ID_RT_CAPABILITY    CacheIdRtCap;               // offset 0x00
  CXL_CACHE_ID_RT_CONTROL       CacheIdRtControl;           // offset 0x04
  CXL_CACHE_ID_RT_STATUS        CacheIdRtStatus;            // offset 0x08
  UINT32                        Reserved;                   // offset 0x0C
  CXL_CACHE_ID_RT_TARGET        CacheIdRtTarget[];          // offset 0x10
} CXL_CACHE_ID_ROUTE_TABLE_CAPABILITY;

//
// CXL Cache ID Decoder Capability
// Compute Express Link Specification Revision 3.0  - Chapter 8.2.4.28
//
typedef union {
  struct {
    UINT32    ExplicitCacheIdDecoderCommitRequired : 1;            // Bit 0
    UINT32    Reserved                             : 31;           // Bit 1..31
  } Bits;
  UINT32    Uint32;
} CXL_CACHE_ID_DECODER_CAP;

typedef union {
  struct {
    UINT32    ForwardCacheId         : 1;           // Bit 0
    UINT32    AssignCacheId          : 1;           // Bit 1
    UINT32    HdmDType2DevicePresent : 1;           // Bit 2
    UINT32    CacheIdDecoderCommit   : 1;           // Bit 3
    UINT32    Reserved1              : 4;           // Bit 4..7
    UINT32    HdmDType2DeviceCacheId : 4;           // Bit 8..11
    UINT32    Reserved2              : 4;           // Bit 12..15
    UINT32    LocalCacheId           : 4;           // Bit 16..19
    UINT32    Reserved3              : 4;           // Bit 20..23
    UINT32    TrustLevel             : 2;           // Bit 24..25
    UINT32    Reserved4              : 6;           // Bit 26..31
  } Bits;
  UINT32    Uint32;
} CXL_CACHE_ID_DECODER_CONTROL;

typedef union {
  struct {
    UINT32    CacheIdDecoderCommitted          : 1;           // Bit 0
    UINT32    CacheIdDecoderErrorNotCommitted  : 1;           // Bit 1
    UINT32    Reserved1                        : 6;           // Bit 2..7
    UINT32    CacheIdDecoderCommitTimeoutScale : 4;           // Bit 8..11
    UINT32    CacheIdDecoderCommitTimeoutBase  : 4;           // Bit 12..15
    UINT32    Reserved2                        : 16;          // Bit 16..31
  } Bits;
  UINT32    Uint32;
} CXL_CACHE_ID_DECODER_STATUS;

typedef struct {
  CXL_CACHE_ID_DECODER_CAP        CacheIdDecoderCap;           // offset 0x00
  CXL_CACHE_ID_DECODER_CONTROL    CacheIdDecoderControl;       // offset 0x04
  CXL_CACHE_ID_DECODER_STATUS     CacheIdDecoderStatus;        // offset 0x08
} CXL_CACHE_ID_DECODER_CAPABILITY;

//
// CXL Timeout and Isolation Capability Structure
// Compute Express Link Specification Revision 3.0  - Chapter 8.2.4.23
//
typedef union {
  struct {
    UINT32    CxlmemTransactionTimeoutRangesSupported   : 4; // Bits 3:0
    UINT32    CxlmemTransactionTimeoutSupported         : 1; // Bits 4
    UINT32    Reserved1                                 : 3; // Bits 7:5
    UINT32    CxlcacheTransactionTimeoutRangesSupported : 4; // Bits 11:8
    UINT32    CxlcacheTransactionTimeoutSupported       : 1; // Bits 12
    UINT32    Reserved2                                 : 3; // Bits 15:13
    UINT32    CxlmemIsolationSupported                  : 1; // Bits 16
    UINT32    CxlmemIsolationLinkdownSupported          : 1; // Bits 17
    UINT32    CxlcacheIsolationSupported                : 1; // Bits 18
    UINT32    CxlcacheIsolationLinkdownSupported        : 1; // Bits 19
    UINT32    Reserved3                                 : 5; // Bits 24:20
    UINT32    IsolationErrCorSignalingSupported         : 1; // Bits 25
    UINT32    IsolationInterruptSupported               : 1; // Bits 26
    UINT32    IsolationInterruptMessageNumber           : 5; // Bits 31:27
  } Bits;
  UINT32    Uint32;
} CXL_3_0_CXL_TIMEOUT_AND_ISOLATION_CAPABILITY;

typedef union {
  struct {
    UINT32    CxlmemTransactionTimeoutValue    : 4; // Bits 3:0
    UINT32    CxlmemTransactionTimeoutEnable   : 1; // Bits 4
    UINT32    Reserved1                        : 3; // Bits 7:5
    UINT32    CxlcacheTransactionTimeoutValue  : 4; // Bits 11:8
    UINT32    CxlcacheTransactionTimeoutEnable : 1; // Bits 12
    UINT32    Reserved2                        : 3; // Bits 15:13
    UINT32    CxlmemIsolationEnable            : 1; // Bits 16
    UINT32    CxlmemIsolationLinkdownEnable    : 1; // Bits 17
    UINT32    CxlcacheIsolationEnable          : 1; // Bits 18
    UINT32    CxlcacheIsolationLinkdownEnable  : 1; // Bits 19
    UINT32    Reserved3                        : 5; // Bits 24:20
    UINT32    IsolationErrCorSignalingEnable   : 1; // Bits 25
    UINT32    IsolationInterruptEnable         : 1; // Bits 26
    UINT32    Reserved4                        : 5; // Bits 31:27
  } Bits;
  UINT32    Uint32;
} CXL_3_0_CXL_TIMEOUT_AND_ISOLATION_CONTROL;

typedef union {
  struct {
    UINT32    CxlmemTransactionTimeout        : 1;  // Bits 0
    UINT32    Reserved1                       : 3;  // Bits 3:1
    UINT32    CxlcacheTransactionTimeout      : 1;  // Bits 4
    UINT32    Reserved2                       : 3;  // Bits 7:5
    UINT32    CxlmemIsolationStatus           : 1;  // Bits 8
    UINT32    CxlmemIsolationLinkdownStatus   : 1;  // Bits 9
    UINT32    Reserved3                       : 2;  // Bits 11:10
    UINT32    CxlcacheIsolationStatus         : 1;  // Bits 12
    UINT32    CxlcacheIsolationLinkdownStatus : 1;  // Bits 13
    UINT32    CxlRpBusy                       : 1;  // Bits 14
    UINT32    Reserved4                       : 17; // Bits 31:15
  } Bits;
  UINT32    Uint32;
} CXL_3_0_CXL_TIMEOUT_AND_ISOLATION_STATUS;

typedef struct {
  CXL_3_0_CXL_TIMEOUT_AND_ISOLATION_CAPABILITY    TimeoutAndIsolationCap;
  UINT32                                          Reserved;
  CXL_3_0_CXL_TIMEOUT_AND_ISOLATION_CONTROL       TimeoutAndIsolationControl;
  CXL_3_0_CXL_TIMEOUT_AND_ISOLATION_STATUS        TimeoutAndIsolationStatus;
} CXL_3_0_CXL_TIMEOUT_AND_ISOLATION_CAPABILITY_STRUCTURE;

#pragma pack()

#endif
