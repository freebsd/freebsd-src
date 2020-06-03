/** @file
  Support for SCSI-2 standard

  Copyright (c) 2006 - 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SCSI_H__
#define __SCSI_H__

//
// SCSI command OP Code
//
//
// Commands for all device types
//
#define EFI_SCSI_OP_CHANGE_DEFINITION 0x40
#define EFI_SCSI_OP_COMPARE           0x39
#define EFI_SCSI_OP_COPY              0x18
#define EFI_SCSI_OP_COPY_VERIFY       0x3a
#define EFI_SCSI_OP_INQUIRY           0x12
#define EFI_SCSI_OP_LOG_SELECT        0x4c
#define EFI_SCSI_OP_LOG_SENSE         0x4d
#define EFI_SCSI_OP_MODE_SEL6         0x15
#define EFI_SCSI_OP_MODE_SEL10        0x55
#define EFI_SCSI_OP_MODE_SEN6         0x1a
#define EFI_SCSI_OP_MODE_SEN10        0x5a
#define EFI_SCSI_OP_READ_BUFFER       0x3c
#define EFI_SCSI_OP_RECEIVE_DIAG      0x1c
#define EFI_SCSI_OP_REQUEST_SENSE     0x03
#define EFI_SCSI_OP_SEND_DIAG         0x1d
#define EFI_SCSI_OP_TEST_UNIT_READY   0x00
#define EFI_SCSI_OP_WRITE_BUFF        0x3b

//
// Additional commands for Direct Access Devices
//
#define EFI_SCSI_OP_FORMAT          0x04
#define EFI_SCSI_OP_LOCK_UN_CACHE   0x36
#define EFI_SCSI_OP_PREFETCH        0x34
#define EFI_SCSI_OP_MEDIA_REMOVAL   0x1e
#define EFI_SCSI_OP_READ6           0x08
#define EFI_SCSI_OP_READ10          0x28
#define EFI_SCSI_OP_READ16          0x88
#define EFI_SCSI_OP_READ_CAPACITY   0x25
#define EFI_SCSI_OP_READ_CAPACITY16 0x9e
#define EFI_SCSI_OP_READ_DEFECT     0x37
#define EFI_SCSI_OP_READ_LONG       0x3e
#define EFI_SCSI_OP_REASSIGN_BLK    0x07
#define EFI_SCSI_OP_RELEASE         0x17
#define EFI_SCSI_OP_REZERO          0x01
#define EFI_SCSI_OP_SEARCH_DATA_E   0x31
#define EFI_SCSI_OP_SEARCH_DATA_H   0x30
#define EFI_SCSI_OP_SEARCH_DATA_L   0x32
#define EFI_SCSI_OP_SEEK6           0x0b
#define EFI_SCSI_OP_SEEK10          0x2b
#define EFI_SCSI_OP_SEND_DIAG       0x1d
#define EFI_SCSI_OP_SET_LIMIT       0x33
#define EFI_SCSI_OP_START_STOP_UNIT 0x1b
#define EFI_SCSI_OP_SYNC_CACHE      0x35
#define EFI_SCSI_OP_VERIFY          0x2f
#define EFI_SCSI_OP_WRITE6          0x0a
#define EFI_SCSI_OP_WRITE10         0x2a
#define EFI_SCSI_OP_WRITE16         0x8a
#define EFI_SCSI_OP_WRITE_VERIFY    0x2e
#define EFI_SCSI_OP_WRITE_LONG      0x3f
#define EFI_SCSI_OP_WRITE_SAME      0x41
#define EFI_SCSI_OP_UNMAP           0x42

//
// Additional commands for Sequential Access Devices
//
#define EFI_SCSI_OP_ERASE             0x19
#define EFI_SCSI_OP_LOAD_UNLOAD       0x1b
#define EFI_SCSI_OP_LOCATE            0x2b
#define EFI_SCSI_OP_READ_BLOCK_LIMIT  0x05
#define EFI_SCSI_OP_READ_POS          0x34
#define EFI_SCSI_OP_READ_REVERSE      0x0f
#define EFI_SCSI_OP_RECOVER_BUF_DATA  0x14
#define EFI_SCSI_OP_RESERVE_UNIT      0x16
#define EFI_SCSI_OP_REWIND            0x01
#define EFI_SCSI_OP_SPACE             0x11
#define EFI_SCSI_OP_VERIFY_TAPE       0x13
#define EFI_SCSI_OP_WRITE_FILEMARK    0x10

//
// Additional commands for Printer Devices
//
#define EFI_SCSI_OP_PRINT       0x0a
#define EFI_SCSI_OP_SLEW_PRINT  0x0b
#define EFI_SCSI_OP_STOP_PRINT  0x1b
#define EFI_SCSI_OP_SYNC_BUFF   0x10

//
// Additional commands for Processor Devices
//
#define EFI_SCSI_OP_RECEIVE 0x08
#define EFI_SCSI_OP_SEND    0x0a

//
// Additional commands for Write-Once Devices
//
#define EFI_SCSI_OP_MEDIUM_SCAN     0x38
#define EFI_SCSI_OP_SEARCH_DAT_E10  0x31
#define EFI_SCSI_OP_SEARCH_DAT_E12  0xb1
#define EFI_SCSI_OP_SEARCH_DAT_H10  0x30
#define EFI_SCSI_OP_SEARCH_DAT_H12  0xb0
#define EFI_SCSI_OP_SEARCH_DAT_L10  0x32
#define EFI_SCSI_OP_SEARCH_DAT_L12  0xb2
#define EFI_SCSI_OP_SET_LIMIT10     0x33
#define EFI_SCSI_OP_SET_LIMIT12     0xb3
#define EFI_SCSI_OP_VERIFY10        0x2f
#define EFI_SCSI_OP_VERIFY12        0xaf
#define EFI_SCSI_OP_WRITE12         0xaa
#define EFI_SCSI_OP_WRITE_VERIFY10  0x2e
#define EFI_SCSI_OP_WRITE_VERIFY12  0xae

//
// Additional commands for CD-ROM Devices
//
#define EFI_SCSI_OP_PLAY_AUD_10       0x45
#define EFI_SCSI_OP_PLAY_AUD_12       0xa5
#define EFI_SCSI_OP_PLAY_AUD_MSF      0x47
#define EFI_SCSI_OP_PLAY_AUD_TKIN     0x48
#define EFI_SCSI_OP_PLAY_TK_REL10     0x49
#define EFI_SCSI_OP_PLAY_TK_REL12     0xa9
#define EFI_SCSI_OP_READ_CD_CAPACITY  0x25
#define EFI_SCSI_OP_READ_HEADER       0x44
#define EFI_SCSI_OP_READ_SUB_CHANNEL  0x42
#define EFI_SCSI_OP_READ_TOC          0x43

//
// Additional commands for Scanner Devices
//
#define EFI_SCSI_OP_GET_DATABUFF_STAT 0x34
#define EFI_SCSI_OP_GET_WINDOW        0x25
#define EFI_SCSI_OP_OBJECT_POS        0x31
#define EFI_SCSI_OP_SCAN              0x1b
#define EFI_SCSI_OP_SET_WINDOW        0x24

//
// Additional commands for Optical Memory Devices
//
#define EFI_SCSI_OP_UPDATE_BLOCK  0x3d

//
// Additional commands for Medium Changer Devices
//
#define EFI_SCSI_OP_EXCHANGE_MEDIUM   0xa6
#define EFI_SCSI_OP_INIT_ELEMENT_STAT 0x07
#define EFI_SCSI_OP_POS_TO_ELEMENT    0x2b
#define EFI_SCSI_OP_REQUEST_VE_ADDR   0xb5
#define EFI_SCSI_OP_SEND_VOL_TAG      0xb6

//
// Additional commands for Communication Devices
//
#define EFI_SCSI_OP_GET_MESSAGE6    0x08
#define EFI_SCSI_OP_GET_MESSAGE10   0x28
#define EFI_SCSI_OP_GET_MESSAGE12   0xa8
#define EFI_SCSI_OP_SEND_MESSAGE6   0x0a
#define EFI_SCSI_OP_SEND_MESSAGE10  0x2a
#define EFI_SCSI_OP_SEND_MESSAGE12  0xaa

//
// Additional commands for Secure Transactions
//
#define EFI_SCSI_OP_SECURITY_PROTOCOL_IN  0xa2
#define EFI_SCSI_OP_SECURITY_PROTOCOL_OUT 0xb5

//
// SCSI Data Transfer Direction
//
#define EFI_SCSI_DATA_IN  0
#define EFI_SCSI_DATA_OUT 1

//
// SCSI Block Command Cache Control Parameters
//
#define EFI_SCSI_BLOCK_FUA  BIT3  ///< Force Unit Access
#define EFI_SCSI_BLOCK_DPO  BIT4  ///< Disable Page Out

//
// Peripheral Device Type Definitions
//
#define EFI_SCSI_TYPE_DISK            0x00  ///< Direct-access device (e.g. magnetic disk)
#define EFI_SCSI_TYPE_TAPE            0x01  ///< Sequential-access device (e.g. magnetic tape)
#define EFI_SCSI_TYPE_PRINTER         0x02  ///< Printer device
#define EFI_SCSI_TYPE_PROCESSOR       0x03  ///< Processor device
#define EFI_SCSI_TYPE_WORM            0x04  ///< Write-once device (e.g. some optical disks)
#define EFI_SCSI_TYPE_CDROM           0x05  ///< CD/DVD device
#define EFI_SCSI_TYPE_SCANNER         0x06  ///< Scanner device (obsolete)
#define EFI_SCSI_TYPE_OPTICAL         0x07  ///< Optical memory device (e.g. some optical disks)
#define EFI_SCSI_TYPE_MEDIUMCHANGER   0x08  ///< Medium changer device (e.g. jukeboxes)
#define EFI_SCSI_TYPE_COMMUNICATION   0x09  ///< Communications device (obsolete)
#define EFI_SCSI_TYPE_ASCIT8_1        0x0A  ///< Defined by ASC IT8 (Graphic arts pre-press devices)
#define EFI_SCSI_TYPE_ASCIT8_2        0x0B  ///< Defined by ASC IT8 (Graphic arts pre-press devices)
#define EFI_SCSI_TYPE_RAID            0x0C  ///< Storage array controller device (e.g., RAID)
#define EFI_SCSI_TYPE_SES             0x0D  ///< Enclosure services device
#define EFI_SCSI_TYPE_RBC             0x0E  ///< Simplified direct-access device (e.g., magnetic disk)
#define EFI_SCSI_TYPE_OCRW            0x0F  ///< Optical card reader/writer device
#define EFI_SCSI_TYPE_BRIDGE          0x10  ///< Bridge Controller Commands
#define EFI_SCSI_TYPE_OSD             0x11  ///< Object-based Storage Device
#define EFI_SCSI_TYPE_AUTOMATION      0x12  ///< Automation/Drive Interface
#define EFI_SCSI_TYPE_SECURITYMANAGER 0x13  ///< Security manager device
#define EFI_SCSI_TYPE_RESERVED_LOW    0x14  ///< Reserved (low)
#define EFI_SCSI_TYPE_RESERVED_HIGH   0x1D  ///< Reserved (high)
#define EFI_SCSI_TYPE_WLUN            0x1E  ///< Well known logical unit
#define EFI_SCSI_TYPE_UNKNOWN         0x1F  ///< Unknown or no device type

//
// Page Codes for INQUIRY command
//
#define EFI_SCSI_PAGE_CODE_SUPPORTED_VPD    0x00
#define EFI_SCSI_PAGE_CODE_BLOCK_LIMITS_VPD 0xB0

#pragma pack(1)
///
/// Standard INQUIRY data format
///
typedef struct {
  UINT8 Peripheral_Type : 5;
  UINT8 Peripheral_Qualifier : 3;
  UINT8 DeviceType_Modifier : 7;
  UINT8 Rmb : 1;
  UINT8 Version;
  UINT8 Response_Data_Format;
  UINT8 Addnl_Length;
  UINT8 Reserved_5_95[95 - 5 + 1];
} EFI_SCSI_INQUIRY_DATA;

///
/// Supported VPD Pages VPD page
///
typedef struct {
  UINT8 Peripheral_Type : 5;
  UINT8 Peripheral_Qualifier : 3;
  UINT8 PageCode;
  UINT8 PageLength2;
  UINT8 PageLength1;
  UINT8 SupportedVpdPageList[0x100];
} EFI_SCSI_SUPPORTED_VPD_PAGES_VPD_PAGE;

///
/// Block Limits VPD page
///
typedef struct {
  UINT8 Peripheral_Type : 5;
  UINT8 Peripheral_Qualifier : 3;
  UINT8 PageCode;
  UINT8 PageLength2;
  UINT8 PageLength1;
  UINT8 WriteSameNonZero : 1;
  UINT8 Reserved_4 : 7;
  UINT8 MaximumCompareAndWriteLength;
  UINT8 OptimalTransferLengthGranularity2;
  UINT8 OptimalTransferLengthGranularity1;
  UINT8 MaximumTransferLength4;
  UINT8 MaximumTransferLength3;
  UINT8 MaximumTransferLength2;
  UINT8 MaximumTransferLength1;
  UINT8 OptimalTransferLength4;
  UINT8 OptimalTransferLength3;
  UINT8 OptimalTransferLength2;
  UINT8 OptimalTransferLength1;
  UINT8 MaximumPrefetchXdreadXdwriteTransferLength4;
  UINT8 MaximumPrefetchXdreadXdwriteTransferLength3;
  UINT8 MaximumPrefetchXdreadXdwriteTransferLength2;
  UINT8 MaximumPrefetchXdreadXdwriteTransferLength1;
  UINT8 MaximumUnmapLbaCount4;
  UINT8 MaximumUnmapLbaCount3;
  UINT8 MaximumUnmapLbaCount2;
  UINT8 MaximumUnmapLbaCount1;
  UINT8 MaximumUnmapBlockDescriptorCount4;
  UINT8 MaximumUnmapBlockDescriptorCount3;
  UINT8 MaximumUnmapBlockDescriptorCount2;
  UINT8 MaximumUnmapBlockDescriptorCount1;
  UINT8 OptimalUnmapGranularity4;
  UINT8 OptimalUnmapGranularity3;
  UINT8 OptimalUnmapGranularity2;
  UINT8 OptimalUnmapGranularity1;
  UINT8 UnmapGranularityAlignment4 : 7;
  UINT8 UnmapGranularityAlignmentValid : 1;
  UINT8 UnmapGranularityAlignment3;
  UINT8 UnmapGranularityAlignment2;
  UINT8 UnmapGranularityAlignment1;
  UINT8 MaximumWriteSameLength4;
  UINT8 MaximumWriteSameLength3;
  UINT8 MaximumWriteSameLength2;
  UINT8 MaximumWriteSameLength1;
  UINT8 MaximumAtomicTransferLength4;
  UINT8 MaximumAtomicTransferLength3;
  UINT8 MaximumAtomicTransferLength2;
  UINT8 MaximumAtomicTransferLength1;
  UINT8 AtomicAlignment4;
  UINT8 AtomicAlignment3;
  UINT8 AtomicAlignment2;
  UINT8 AtomicAlignment1;
  UINT8 AtomicTransferLengthGranularity4;
  UINT8 AtomicTransferLengthGranularity3;
  UINT8 AtomicTransferLengthGranularity2;
  UINT8 AtomicTransferLengthGranularity1;
  UINT8 MaximumAtomicTransferLengthWithAtomicBoundary4;
  UINT8 MaximumAtomicTransferLengthWithAtomicBoundary3;
  UINT8 MaximumAtomicTransferLengthWithAtomicBoundary2;
  UINT8 MaximumAtomicTransferLengthWithAtomicBoundary1;
  UINT8 MaximumAtomicBoundarySize4;
  UINT8 MaximumAtomicBoundarySize3;
  UINT8 MaximumAtomicBoundarySize2;
  UINT8 MaximumAtomicBoundarySize1;
} EFI_SCSI_BLOCK_LIMITS_VPD_PAGE;

///
/// Error codes 70h and 71h sense data format
///
typedef struct {
  UINT8 Error_Code : 7;
  UINT8 Valid : 1;
  UINT8 Segment_Number;
  UINT8 Sense_Key : 4;
  UINT8 Reserved_21 : 1;
  UINT8 Ili : 1;
  UINT8 Reserved_22 : 2;
  UINT8 Information_3_6[4];
  UINT8 Addnl_Sense_Length;           ///< Additional sense length (n-7)
  UINT8 Vendor_Specific_8_11[4];
  UINT8 Addnl_Sense_Code;             ///< Additional sense code
  UINT8 Addnl_Sense_Code_Qualifier;   ///< Additional sense code qualifier
  UINT8 Field_Replaceable_Unit_Code;  ///< Field replaceable unit code
  UINT8 Reserved_15_17[3];
} EFI_SCSI_SENSE_DATA;

///
/// SCSI Disk READ CAPACITY Data
///
typedef struct {
  UINT8 LastLba3;
  UINT8 LastLba2;
  UINT8 LastLba1;
  UINT8 LastLba0;
  UINT8 BlockSize3;
  UINT8 BlockSize2;
  UINT8 BlockSize1;
  UINT8 BlockSize0;
} EFI_SCSI_DISK_CAPACITY_DATA;

typedef struct {
  UINT8 LastLba7;
  UINT8 LastLba6;
  UINT8 LastLba5;
  UINT8 LastLba4;
  UINT8 LastLba3;
  UINT8 LastLba2;
  UINT8 LastLba1;
  UINT8 LastLba0;
  UINT8 BlockSize3;
  UINT8 BlockSize2;
  UINT8 BlockSize1;
  UINT8 BlockSize0;
  UINT8 Protection;
  UINT8 LogicPerPhysical;
  UINT8 LowestAlignLogic2;
  UINT8 LowestAlignLogic1;
  UINT8 Reserved[16];
} EFI_SCSI_DISK_CAPACITY_DATA16;

typedef struct {
  UINT16 DataLen;
  UINT16 BlkDespDataLen;
  UINT8  Reserved[4];
} EFI_SCSI_DISK_UNMAP_PARAM_LIST_HEADER;

typedef struct {
  UINT64 Lba;
  UINT32 BlockNum;
  UINT8  Reserved[4];
} EFI_SCSI_DISK_UNMAP_BLOCK_DESP;


#pragma pack()

//
// Sense Key
//
#define EFI_SCSI_SK_NO_SENSE          (0x0)
#define EFI_SCSI_SK_RECOVERY_ERROR    (0x1)
#define EFI_SCSI_SK_NOT_READY         (0x2)
#define EFI_SCSI_SK_MEDIUM_ERROR      (0x3)
#define EFI_SCSI_SK_HARDWARE_ERROR    (0x4)
#define EFI_SCSI_SK_ILLEGAL_REQUEST   (0x5)
#define EFI_SCSI_SK_UNIT_ATTENTION    (0x6)
#define EFI_SCSI_SK_DATA_PROTECT      (0x7)
#define EFI_SCSI_SK_BLANK_CHECK       (0x8)
#define EFI_SCSI_SK_VENDOR_SPECIFIC   (0x9)
#define EFI_SCSI_SK_RESERVED_A        (0xA)
#define EFI_SCSI_SK_ABORT             (0xB)
#define EFI_SCSI_SK_RESERVED_C        (0xC)
#define EFI_SCSI_SK_OVERFLOW          (0xD)
#define EFI_SCSI_SK_MISCOMPARE        (0xE)
#define EFI_SCSI_SK_RESERVED_F        (0xF)

//
// Additional Sense Codes and Sense Code Qualifiers.
// Only some frequently used additional sense codes and qualifiers are
// defined here. Please refer to SCSI standard for full value definition.
//
#define EFI_SCSI_ASC_NOT_READY                    (0x04)
#define EFI_SCSI_ASCQ_IN_PROGRESS                 (0x01)

#define EFI_SCSI_ASC_MEDIA_ERR1                   (0x10)
#define EFI_SCSI_ASC_MEDIA_ERR2                   (0x11)
#define EFI_SCSI_ASC_MEDIA_ERR3                   (0x14)
#define EFI_SCSI_ASC_MEDIA_ERR4                   (0x30)
#define EFI_SCSI_ASC_MEDIA_UPSIDE_DOWN            (0x06)
#define EFI_SCSI_ASC_INVALID_CMD                  (0x20)
#define EFI_SCSI_ASC_LBA_OUT_OF_RANGE             (0x21)
#define EFI_SCSI_ASC_INVALID_FIELD                (0x24)
#define EFI_SCSI_ASC_WRITE_PROTECTED              (0x27)
#define EFI_SCSI_ASC_MEDIA_CHANGE                 (0x28)
#define EFI_SCSI_ASC_RESET                        (0x29)   ///< Power On Reset or Bus Reset occurred
#define EFI_SCSI_ASC_ILLEGAL_FIELD                (0x26)
#define EFI_SCSI_ASC_NO_MEDIA                     (0x3A)
#define EFI_SCSI_ASC_ILLEGAL_MODE_FOR_THIS_TRACK  (0x64)

#endif
