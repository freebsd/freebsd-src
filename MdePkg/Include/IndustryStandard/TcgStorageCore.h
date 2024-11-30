/** @file
  TCG defined values and structures.

  (TCG Storage Architecture Core Specification, Version 2.01, Revision 1.00,
  https://trustedcomputinggroup.org/tcg-storage-architecture-core-specification/)

  Check http://trustedcomputinggroup.org for latest specification updates.

Copyright (c) 2016 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _TCG_STORAGE_CORE_H_
#define _TCG_STORAGE_CORE_H_

#include <Base.h>

#pragma pack(1)

/// UID in host native byte order
typedef UINT64 TCG_UID;

#define TCG_TO_UID(b0, b1, b2, b3, b4, b5, b6, b7)  (TCG_UID)(\
  (UINT64)(b0)         | \
  ((UINT64)(b1) << 8)  | \
  ((UINT64)(b2) << 16) | \
  ((UINT64)(b3) << 24) | \
  ((UINT64)(b4) << 32) | \
  ((UINT64)(b5) << 40) | \
  ((UINT64)(b6) << 48) | \
  ((UINT64)(b7) << 56))

typedef struct {
  UINT32    ReservedBE;
  UINT16    ComIDBE;
  UINT16    ComIDExtensionBE;
  UINT32    OutstandingDataBE;
  UINT32    MinTransferBE;
  UINT32    LengthBE;
  UINT8     Payload[0];
} TCG_COM_PACKET;

typedef struct {
  UINT32    TperSessionNumberBE;
  UINT32    HostSessionNumberBE;
  UINT32    SequenceNumberBE;
  UINT16    ReservedBE;
  UINT16    AckTypeBE;
  UINT32    AcknowledgementBE;
  UINT32    LengthBE;
  UINT8     Payload[0];
} TCG_PACKET;

#define TCG_SUBPACKET_ALIGNMENT  4// 4-byte alignment per spec

typedef struct {
  UINT8     ReservedBE[6];
  UINT16    KindBE;
  UINT32    LengthBE;
  UINT8     Payload[0];
} TCG_SUB_PACKET;

#define SUBPACKET_KIND_DATA            0x0000
#define SUBPACKET_KIND_CREDIT_CONTROL  0x8001

#define TCG_ATOM_TYPE_INTEGER  0x0
#define TCG_ATOM_TYPE_BYTE     0x1
typedef struct {
  UINT8    Data   : 6;
  UINT8    Sign   : 1;
  UINT8    IsZero : 1;
} TCG_TINY_ATOM_BITS;

typedef union {
  UINT8                 Raw;
  TCG_TINY_ATOM_BITS    TinyAtomBits;
} TCG_SIMPLE_TOKEN_TINY_ATOM;

typedef struct {
  UINT8    Length     : 4;
  UINT8    SignOrCont : 1;
  UINT8    ByteOrInt  : 1;
  UINT8    IsZero     : 1;
  UINT8    IsOne      : 1;
} TCG_SHORT_ATOM_BITS;

typedef union {
  UINT8                  RawHeader;
  TCG_SHORT_ATOM_BITS    ShortAtomBits;
} TCG_SIMPLE_TOKEN_SHORT_ATOM;

#define TCG_MEDIUM_ATOM_LENGTH_HIGH_SHIFT  0x8
#define TCG_MEDIUM_ATOM_LENGTH_HIGH_MASK   0x7

typedef struct {
  UINT8    LengthHigh : 3;
  UINT8    SignOrCont : 1;
  UINT8    ByteOrInt  : 1;
  UINT8    IsZero     : 1;
  UINT8    IsOne1     : 1;
  UINT8    IsOne2     : 1;
  UINT8    LengthLow;
} TCG_MEDIUM_ATOM_BITS;

typedef union {
  UINT16                  RawHeader;
  TCG_MEDIUM_ATOM_BITS    MediumAtomBits;
} TCG_SIMPLE_TOKEN_MEDIUM_ATOM;

#define TCG_LONG_ATOM_LENGTH_HIGH_SHIFT  16
#define TCG_LONG_ATOM_LENGTH_MID_SHIFT   8

typedef  struct {
  UINT8    SignOrCont : 1;
  UINT8    ByteOrInt  : 1;
  UINT8    Reserved   : 2;
  UINT8    IsZero     : 1;
  UINT8    IsOne1     : 1;
  UINT8    IsOne2     : 1;
  UINT8    IsOne3     : 1;
  UINT8    LengthHigh;
  UINT8    LengthMid;
  UINT8    LengthLow;
} TCG_LONG_ATOM_BITS;

typedef union {
  UINT32                RawHeader;
  TCG_LONG_ATOM_BITS    LongAtomBits;
} TCG_SIMPLE_TOKEN_LONG_ATOM;

// TCG Core Spec v2 - Table 04 - Token Types
typedef enum {
  TcgTokenTypeReserved,
  TcgTokenTypeTinyAtom,
  TcgTokenTypeShortAtom,
  TcgTokenTypeMediumAtom,
  TcgTokenTypeLongAtom,
  TcgTokenTypeStartList,
  TcgTokenTypeEndList,
  TcgTokenTypeStartName,
  TcgTokenTypeEndName,
  TcgTokenTypeCall,
  TcgTokenTypeEndOfData,
  TcgTokenTypeEndOfSession,
  TcgTokenTypeStartTransaction,
  TcgTokenTypeEndTransaction,
  TcgTokenTypeEmptyAtom,
} TCG_TOKEN_TYPE;

#pragma pack()

#define TCG_TOKEN_SHORTATOM_MAX_BYTE_SIZE   0x0F
#define TCG_TOKEN_MEDIUMATOM_MAX_BYTE_SIZE  0x7FF
#define TCG_TOKEN_LONGATOM_MAX_BYTE_SIZE    0xFFFFFF

#define TCG_TOKEN_TINYATOM_UNSIGNED_MAX_VALUE  0x3F
#define TCG_TOKEN_TINYATOM_SIGNED_MAX_VALUE    0x1F
#define TCG_TOKEN_TINYATOM_SIGNED_MIN_VALUE    -32

// TOKEN TYPES
#define TCG_TOKEN_TINYATOM          0x00
#define TCG_TOKEN_TINYSIGNEDATOM    0x40
#define TCG_TOKEN_SHORTATOM         0x80
#define TCG_TOKEN_SHORTSIGNEDATOM   0x90
#define TCG_TOKEN_SHORTBYTESATOM    0xA0
#define TCG_TOKEN_MEDIUMATOM        0xC0
#define TCG_TOKEN_MEDIUMSIGNEDATOM  0xC8
#define TCG_TOKEN_MEDIUMBYTESATOM   0xD0
#define TCG_TOKEN_LONGATOM          0xE0
#define TCG_TOKEN_LONGSIGNEDATOM    0xE1
#define TCG_TOKEN_LONGBYTESATOM     0xE2
#define TCG_TOKEN_STARTLIST         0xF0
#define TCG_TOKEN_ENDLIST           0xF1
#define TCG_TOKEN_STARTNAME         0xF2
#define TCG_TOKEN_ENDNAME           0xF3
// 0xF4 - 0xF7 TCG Reserved
#define TCG_TOKEN_CALL              0xF8
#define TCG_TOKEN_ENDDATA           0xF9
#define TCG_TOKEN_ENDSESSION        0xFA
#define TCG_TOKEN_STARTTRANSACTION  0xFB
#define TCG_TOKEN_ENDTRANSACTION    0xFC
// 0xFD - 0xFE TCG Reserved
#define TCG_TOKEN_EMPTY  0xFF

// CELLBLOCK reserved Names
#define TCG_CELL_BLOCK_TABLE_NAME         (UINT8)0x00
#define TCG_CELL_BLOCK_START_ROW_NAME     (UINT8)0x01
#define TCG_CELL_BLOCK_END_ROW_NAME       (UINT8)0x02
#define TCG_CELL_BLOCK_START_COLUMN_NAME  (UINT8)0x03
#define TCG_CELL_BLOCK_END_COLUMN_NAME    (UINT8)0x04

// METHOD STATUS CODES
#define TCG_METHOD_STATUS_CODE_SUCCESS                0x00
#define TCG_METHOD_STATUS_CODE_NOT_AUTHORIZED         0x01
#define TCG_METHOD_STATUS_CODE_OBSOLETE               0x02
#define TCG_METHOD_STATUS_CODE_SP_BUSY                0x03
#define TCG_METHOD_STATUS_CODE_SP_FAILED              0x04
#define TCG_METHOD_STATUS_CODE_SP_DISABLED            0x05
#define TCG_METHOD_STATUS_CODE_SP_FROZEN              0x06
#define TCG_METHOD_STATUS_CODE_NO_SESSIONS_AVAILABLE  0x07
#define TCG_METHOD_STATUS_CODE_UNIQUENESS_CONFLICT    0x08
#define TCG_METHOD_STATUS_CODE_INSUFFICIENT_SPACE     0x09
#define TCG_METHOD_STATUS_CODE_INSUFFICIENT_ROWS      0x0A
#define TCG_METHOD_STATUS_CODE_INVALID_PARAMETER      0x0C
#define TCG_METHOD_STATUS_CODE_OBSOLETE2              0x0D
#define TCG_METHOD_STATUS_CODE_OBSOLETE3              0x0E
#define TCG_METHOD_STATUS_CODE_TPER_MALFUNCTION       0x0F
#define TCG_METHOD_STATUS_CODE_TRANSACTION_FAILURE    0x10
#define TCG_METHOD_STATUS_CODE_RESPONSE_OVERFLOW      0x11
#define TCG_METHOD_STATUS_CODE_AUTHORITY_LOCKED_OUT   0x12
#define TCG_METHOD_STATUS_CODE_FAIL                   0x3F

// Feature Codes
#define TCG_FEATURE_INVALID             (UINT16)0x0000
#define TCG_FEATURE_TPER                (UINT16)0x0001
#define TCG_FEATURE_LOCKING             (UINT16)0x0002
#define TCG_FEATURE_GEOMETRY_REPORTING  (UINT16)0x0003
#define TCG_FEATURE_SINGLE_USER_MODE    (UINT16)0x0201
#define TCG_FEATURE_DATASTORE_TABLE     (UINT16)0x0202
#define TCG_FEATURE_OPAL_SSC_V1_0_0     (UINT16)0x0200
#define TCG_FEATURE_OPAL_SSC_V2_0_0     (UINT16)0x0203
#define TCG_FEATURE_OPAL_SSC_LITE       (UINT16)0x0301
#define TCG_FEATURE_PYRITE_SSC          (UINT16)0x0302
#define TCG_FEATURE_PYRITE_SSC_V2_0_0   (UINT16)0x0303
#define TCG_FEATURE_BLOCK_SID           (UINT16)0x0402
#define TCG_FEATURE_DATA_REMOVAL        (UINT16)0x0404

// ACE Expression values
#define TCG_ACE_EXPRESSION_AND  0x0
#define TCG_ACE_EXPRESSION_OR   0x1

/****************************************************************************
TRUSTED RECEIVE - supported security protocols list (SP_Specific = 0000h)
ATA 8 Rev6a Table 68 7.57.6.2
****************************************************************************/
// Security Protocol IDs
#define TCG_SECURITY_PROTOCOL_INFO                    0x00
#define TCG_OPAL_SECURITY_PROTOCOL_1                  0x01
#define TCG_OPAL_SECURITY_PROTOCOL_2                  0x02
#define TCG_SECURITY_PROTOCOL_TCG3                    0x03
#define TCG_SECURITY_PROTOCOL_TCG4                    0x04
#define TCG_SECURITY_PROTOCOL_TCG5                    0x05
#define TCG_SECURITY_PROTOCOL_TCG6                    0x06
#define TCG_SECURITY_PROTOCOL_CBCS                    0x07
#define TCG_SECURITY_PROTOCOL_TAPE_DATA               0x20
#define TCG_SECURITY_PROTOCOL_DATA_ENCRYPT_CONFIG     0x21
#define TCG_SECURITY_PROTOCOL_SA_CREATION_CAPS        0x40
#define TCG_SECURITY_PROTOCOL_IKEV2_SCSI              0x41
#define TCG_SECURITY_PROTOCOL_JEDEC_UFS               0xEC
#define TCG_SECURITY_PROTOCOL_SDCARD_SECURITY         0xED
#define TCG_SECURITY_PROTOCOL_IEEE_1667               0xEE
#define TCG_SECURITY_PROTOCOL_ATA_DEVICE_SERVER_PASS  0xEF

// Security Protocol Specific IDs
#define TCG_SP_SPECIFIC_PROTOCOL_LIST              0x0000
#define TCG_SP_SPECIFIC_PROTOCOL_LEVEL0_DISCOVERY  0x0001

#define TCG_RESERVED_COMID  0x0000

// Defined in TCG Storage Feature Set:Block SID Authentication spec,
// ComId used for BlockSid command is hardcode 0x0005.
#define TCG_BLOCKSID_COMID  0x0005

#pragma pack(1)
typedef struct {
  UINT8     Reserved[6];
  UINT16    ListLength_BE; // 6 - 7
  UINT8     List[504];     // 8...
} TCG_SUPPORTED_SECURITY_PROTOCOLS;

// Level 0 Discovery
typedef struct {
  UINT32    LengthBE; // number of valid bytes in discovery response, not including length field
  UINT16    VerMajorBE;
  UINT16    VerMinorBE;
  UINT8     Reserved[8];
  UINT8     VendorUnique[32];
} TCG_LEVEL0_DISCOVERY_HEADER;

typedef struct _TCG_LEVEL0_FEATURE_DESCRIPTOR_HEADER {
  UINT16    FeatureCode_BE;
  UINT8     Reserved : 4;
  UINT8     Version  : 4;
  UINT8     Length;  // length of feature dependent data in bytes
} TCG_LEVEL0_FEATURE_DESCRIPTOR_HEADER;

typedef struct {
  TCG_LEVEL0_FEATURE_DESCRIPTOR_HEADER    Header;
  UINT8                                   LockingSupported : 1;
  UINT8                                   LockingEnabled   : 1; // means the locking security provider (SP) is enabled
  UINT8                                   Locked           : 1; // means at least 1 locking range is enabled
  UINT8                                   MediaEncryption  : 1;
  UINT8                                   MbrEnabled       : 1;
  UINT8                                   MbrDone          : 1;
  UINT8                                   Reserved         : 2;
  UINT8                                   Reserved515[11];
} TCG_LOCKING_FEATURE_DESCRIPTOR;

typedef struct {
  TCG_LEVEL0_FEATURE_DESCRIPTOR_HEADER    Header;
  UINT8                                   SIDValueState   : 1;
  UINT8                                   SIDBlockedState : 1;
  UINT8                                   Reserved4       : 6;
  UINT8                                   HardwareReset   : 1;
  UINT8                                   Reserved5       : 7;
  UINT8                                   Reserved615[10];
} TCG_BLOCK_SID_FEATURE_DESCRIPTOR;

typedef struct {
  TCG_LEVEL0_FEATURE_DESCRIPTOR_HEADER    Header;
  UINT8                                   SyncSupported       : 1;
  UINT8                                   AsyncSupported      : 1;
  UINT8                                   AckNakSupported     : 1;
  UINT8                                   BufferMgmtSupported : 1;
  UINT8                                   StreamingSupported  : 1;
  UINT8                                   Reserved4b5         : 1;
  UINT8                                   ComIdMgmtSupported  : 1;
  UINT8                                   Reserved4b7         : 1;
  UINT8                                   Reserved515[11];
} TCG_TPER_FEATURE_DESCRIPTOR;

#pragma pack()

// Special Purpose UIDs
#define TCG_UID_NULL     TCG_TO_UID(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)
#define TCG_UID_THIS_SP  TCG_TO_UID(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01)
#define TCG_UID_SMUID    TCG_TO_UID(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF)

// Session Manager Method UIDS
#define TCG_UID_SM_PROPERTIES             TCG_TO_UID(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x01)
#define TCG_UID_SM_START_SESSION          TCG_TO_UID(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x02)
#define TCG_UID_SM_SYNC_SESSION           TCG_TO_UID(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x03)
#define TCG_UID_SM_START_TRUSTED_SESSION  TCG_TO_UID(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x04)
#define TCG_UID_SM_SYNC_TRUSTED_SESSION   TCG_TO_UID(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x05)
#define TCG_UID_SM_CLOSE_SESSION          TCG_TO_UID(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x06)

// MethodID UIDs
#define TCG_UID_METHOD_DELETE_SP          TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01)
#define TCG_UID_METHOD_CREATE_TABLE       TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x02)
#define TCG_UID_METHOD_DELETE             TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x03)
#define TCG_UID_METHOD_CREATE_ROW         TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04)
#define TCG_UID_METHOD_DELETE_ROW         TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05)
#define TCG_UID_METHOD_NEXT               TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x08)
#define TCG_UID_METHOD_GET_FREE_SPACE     TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x09)
#define TCG_UID_METHOD_GET_FREE_ROWS      TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0A)
#define TCG_UID_METHOD_DELETE_METHOD      TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0B)
#define TCG_UID_METHOD_GET_ACL            TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0D)
#define TCG_UID_METHOD_ADD_ACE            TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0E)
#define TCG_UID_METHOD_REMOVE_ACE         TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0F)
#define TCG_UID_METHOD_GEN_KEY            TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x10)
#define TCG_UID_METHOD_GET_PACKAGE        TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x12)
#define TCG_UID_METHOD_SET_PACKAGE        TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x13)
#define TCG_UID_METHOD_GET                TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x16)
#define TCG_UID_METHOD_SET                TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x17)
#define TCG_UID_METHOD_AUTHENTICATE       TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x1C)
#define TCG_UID_METHOD_ISSUE_SP           TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x02, 0x01)
#define TCG_UID_METHOD_GET_CLOCK          TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x04, 0x01)
#define TCG_UID_METHOD_RESET_CLOCK        TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x04, 0x02)
#define TCG_UID_METHOD_SET_CLOCK_HIGH     TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x04, 0x03)
#define TCG_UID_METHOD_SET_LAG_HIGH       TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x04, 0x04)
#define TCG_UID_METHOD_SET_CLOCK_LOW      TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x04, 0x05)
#define TCG_UID_METHOD_SET_LAG_LOW        TCG_TO_UID(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x06)
#define TCG_UID_METHOD_INCREMENT_COUNTER  TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x04, 0x07)
#define TCG_UID_METHOD_RANDOM             TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x01)
#define TCG_UID_METHOD_SALT               TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x02)
#define TCG_UID_METHOD_DECRYPT_INIT       TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x03)
#define TCG_UID_METHOD_DECRYPT            TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x04)
#define TCG_UID_METHOD_DECRYPT_FINALIZE   TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x05)
#define TCG_UID_METHOD_ENCRYPT_INIT       TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x06)
#define TCG_UID_METHOD_ENCRYPT            TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x07)
#define TCG_UID_METHOD_ENCRYPT_FINALIZE   TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x08)
#define TCG_UID_METHOD_HMAC_INIT          TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x09)
#define TCG_UID_METHOD_HMAC               TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x0A)
#define TCG_UID_METHOD_HMAC_FINALIZE      TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x0B)
#define TCG_UID_METHOD_HASH_INIT          TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x0C)
#define TCG_UID_METHOD_HASH               TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x0D)
#define TCG_UID_METHOD_HASH_FINALIZE      TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x0E)
#define TCG_UID_METHOD_SIGN               TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x0F)
#define TCG_UID_METHOD_VERIFY             TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x10)
#define TCG_UID_METHOD_XOR                TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x11)
#define TCG_UID_METHOD_ADD_LOG            TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x0A, 0x01)
#define TCG_UID_METHOD_CREATE_LOG         TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x0A, 0x02)
#define TCG_UID_METHOD_CLEAR_LOG          TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x0A, 0x03)
#define TCG_UID_METHOD_FLUSH_LOG          TCG_TO_UID(0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x0A, 0x04)

#endif // TCG_H_
