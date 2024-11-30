/** @file
  SPI NOR Flash JEDEC Serial Flash Discoverable Parameters (SFDP)
  header file.

  Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    - JEDEC Standard, JESD216F.02
      https://www.jedec.org/document_search?search_api_views_fulltext=JESD216

  @par Glossary:
    - SFDP - Serial Flash Discoverable Parameters
    - PTP  - Parameter Table Pointer
**/

#ifndef SPI_NOR_FLASH_JEDEC_H_
#define SPI_NOR_FLASH_JEDEC_H_

#include <Base.h>

#define SFDP_HEADER_SIGNATURE          0x50444653
#define SFDP_SUPPORTED_MAJOR_REVISION  0x1ul

/// JEDEC Basic Flash Parameter Header
#define SFDP_BASIC_PARAMETER_ID_LSB  0x00
#define SFDP_BASIC_PARAMETER_ID_MSB  0xFF

///
/// JDEC Sector Map Parameter Header and Table
///
#define SFDP_SECTOR_MAP_PARAMETER_ID_LSB        0x81
#define SFDP_FOUR_BYTE_ADDRESS_INSTRUCTION_LSB  0x84
#define SFDP_SECTOR_MAP_PARAMETER_ID_MSB        0xFF

#define SFDP_FLASH_MEMORY_DENSITY_4GBIT  0x80000000

#pragma pack (1)
typedef struct _SFDP_HEADER {
  UINT32    Signature;
  UINT32    MinorRev            : 8;
  UINT32    MajorRev            : 8;
  UINT32    NumParameterHeaders : 8;
  UINT32    AccessProtocol      : 8;
} SFDP_HEADER;

typedef struct _SFDP_PARAMETER_HEADER {
  UINT32    IdLsb        : 8;
  UINT32    MinorRev     : 8;
  UINT32    MajorRev     : 8;
  UINT32    Length       : 8;
  UINT32    TablePointer : 24;
  UINT32    IdMsb        : 8;
} SFDP_PARAMETER_HEADER;

typedef struct _SFDP_BASIC_FLASH_PARAMETER {
  // DWORD 1
  UINT32    EraseSizes                 : 2;
  UINT32    WriteGranularity           : 1;
  UINT32    VolatileStatusBlockProtect : 1;
  UINT32    WriteEnableVolatileStatus  : 1;
  UINT32    Unused1Dw1                 : 3;
  UINT32    FourKEraseInstr            : 8;
  UINT32    FastRead112                : 1;
  UINT32    AddressBytes               : 2;
  UINT32    DtrClocking                : 1;
  UINT32    FastRead122                : 1;
  UINT32    FastRead144                : 1;
  UINT32    FastRead114                : 1;
  UINT32    Unused2Dw1                 : 9;
  // DWORD 2
  UINT32    Density;
  // DWORD 3
  // Fast Read 144
  UINT32    FastRead144Dummy           : 5;
  UINT32    FastRead144ModeClk         : 3;
  UINT32    FastRead144Instr           : 8;
  // Fast Read 114
  UINT32    FastRead114Dummy           : 5;
  UINT32    FastRead114ModeClk         : 3;
  UINT32    FastRead114Instr           : 8;
  // DWORD 4
  // Fast Read 112
  UINT32    FastRead112Dummy           : 5;
  UINT32    FastRead112ModeClk         : 3;
  UINT32    FastRead112Instr           : 8;
  // Fast Read 122
  UINT32    FastRead122Dummy           : 5;
  UINT32    FastRead122ModeClk         : 3;
  UINT32    FastRead122Instr           : 8;
  // DWORD 5
  UINT32    FastRead222                : 1;
  UINT32    Unused1Dw5                 : 3;
  UINT32    FastRead444                : 1;
  UINT32    Unused2Dw5                 : 27;
  // DWORD 6
  UINT32    UnusedDw6                  : 16;
  // Fast Read 222
  UINT32    FastRead222Dummy           : 5;
  UINT32    FastRead222ModeClk         : 3;
  UINT32    FastRead222Instr           : 8;
  // DWORD 7
  UINT32    UnusedDw7                  : 16;
  // Fast Read 444
  UINT32    FastRead444Dummy           : 5;
  UINT32    FastRead444ModeClk         : 3;
  UINT32    FastRead444Instr           : 8;
  // DWORD 8
  UINT32    Erase1Size                 : 8;
  UINT32    Erase1Instr                : 8;
  UINT32    Erase2Size                 : 8;
  UINT32    Erase2Instr                : 8;
  // DWORD 9
  UINT32    Erase3Size                 : 8;
  UINT32    Erase3Instr                : 8;
  UINT32    Erase4Size                 : 8;
  UINT32    Erase4Instr                : 8;
  // DWORD 10
  UINT32    EraseMultiplier            : 4;
  UINT32    Erase1Time                 : 7;
  UINT32    Erase2Time                 : 7;
  UINT32    Erase3Time                 : 7;
  UINT32    Erase4Time                 : 7;
  // DWORD 11
  UINT32    ProgramMultiplier          : 4;
  UINT32    PageSize                   : 4;
  UINT32    PPTime                     : 6;
  UINT32    BPFirstTime                : 5;
  UINT32    BPAdditionalTime           : 5;
  UINT32    ChipEraseTime              : 7;
  UINT32    Unused1Dw11                : 1;
  // DWORD 12
  UINT32    ProgSuspendProhibit        : 4;
  UINT32    EraseSuspendProhibit       : 4;
  UINT32    Unused1Dw13                : 1;
  UINT32    ProgResumeToSuspend        : 4;
  UINT32    ProgSuspendInProgressTime  : 7;
  UINT32    EraseResumeToSuspend       : 4;
  UINT32    EraseSuspendInProgressTime : 7;
  UINT32    SuspendResumeSupported     : 1;
  // DWORD 13
  UINT32    Unused13;
  // DWORD 14
  UINT32    Unused14;
  // DWORD 15
  UINT32    Unused15;
  // DWORD 16
  UINT32    Unused16;
  // DWORD 17
  UINT32    FastRead188Dummy   : 5;
  UINT32    FastRead188ModeClk : 3;
  UINT32    FastRead188Instr   : 8;
  UINT32    FastRead118Dummy   : 5;
  UINT32    FastRead118ModeClk : 3;
  UINT32    FastRead118Instr   : 8;
  //
  // Don't care about remaining DWORDs
  // DWORD 18 to DWORD 23
  //
  UINT32    Unused18;
  UINT32    Unused19;
  UINT32    Unused20;
  UINT32    Unused21;
  UINT32    Unused22;
  UINT32    Unused23;
} SFDP_BASIC_FLASH_PARAMETER;
#pragma pack ()

#define SPI_UNIFORM_4K_ERASE_SUPPORTED    0x01
#define SPI_UNIFORM_4K_ERASE_UNSUPPORTED  0x03

///
/// Number of address bytes opcode can support
///
#define SPI_ADDR_3BYTE_ONLY  0x00
#define SPI_ADDR_3OR4BYTE    0x01
#define SPI_ADDR_4BYTE_ONLY  0x02

#define SFDP_ERASE_TYPES_NUMBER  4
#define SFDP_ERASE_TYPE_1        0x0001
#define SFDP_ERASE_TYPE_2        0x0002
#define SFDP_ERASE_TYPE_3        0x0003
#define SFDP_ERASE_TYPE_4        0x0004

///
///  Read/Write Array Commands
///
#define SPI_FLASH_READ                    0x03
#define   SPI_FLASH_READ_DUMMY            0x00
#define   SPI_FLASH_READ_ADDR_BYTES       SPI_ADDR_3OR4BYTE
#define SPI_FLASH_FAST_READ               0x0B
#define   SPI_FLASH_FAST_READ_DUMMY       0x01
#define   SPI_FLASH_FAST_READ_ADDR_BYTES  SPI_ADDR_3OR4BYTE
#define SPI_FLASH_PP                      0x02
#define   SPI_FLASH_PP_DUMMY              0x00
#define   SPI_FLASH_PP_ADDR_BYTES         SPI_ADDR_3OR4BYTE
#define   SPI_FLASH_PAGE_SIZE             256
#define SPI_FLASH_SE                      0x20
#define   SPI_FLASH_SE_DUMMY              0x00
#define   SPI_FLASH_SE_ADDR_BYTES         SPI_ADDR_3OR4BYTE
#define SPI_FLASH_BE32K                   0x52
#define   SPI_FLASH_BE32K_DUMMY           0x00
#define   SPI_FLASH_BE32K_ADDR_BYTES      SPI_ADDR_3OR4BYTE
#define SPI_FLASH_BE                      0xD8
#define   SPI_FLASH_BE_DUMMY              0x00
#define   SPI_FLASH_BE_ADDR_BYTES         SPI_ADDR_3OR4BYTE
#define SPI_FLASH_CE                      0x60
#define   SPI_FLASH_CE_DUMMY              0x00
#define   SPI_FLASH_CE_ADDR_BYTES         SPI_ADDR_3OR4BYTE
#define SPI_FLASH_RDID                    0x9F
#define   SPI_FLASH_RDID_DUMMY            0x00
#define   SPI_FLASH_RDID_ADDR_BYTES       SPI_ADDR_3OR4BYTE

///
/// Register Setting Commands
///
#define SPI_FLASH_WREN                              0x06
#define   SPI_FLASH_WREN_DUMMY                      0x00
#define   SPI_FLASH_WREN_ADDR_BYTES                 SPI_ADDR_3OR4BYTE
#define SPI_FLASH_WRDI                              0x04
#define   SPI_FLASH_WRDI_DUMMY                      0x00
#define   SPI_FLASH_WRDI_ADDR_BYTES                 SPI_ADDR_3OR4BYTE
#define SPI_FLASH_RDSR                              0x05
#define   SPI_FLASH_RDSR_DUMMY                      0x00
#define   SPI_FLASH_RDSR_ADDR_BYTES                 SPI_ADDR_3OR4BYTE
#define   SPI_FLASH_SR_NOT_WIP                      0x0
#define   SPI_FLASH_SR_WIP                          BIT0
#define   SPI_FLASH_SR_WEL                          BIT1
#define SPI_FLASH_WRSR                              0x01
#define   SPI_FLASH_WRSR_DUMMY                      0x00
#define   SPI_FLASH_WRSR_ADDR_BYTES                 SPI_ADDR_3OR4BYTE
#define SPI_FLASH_WREN_50H                          0x50
#define SPI_FLASH_RDSFDP                            0x5A
#define   SPI_FLASH_RDSFDP_DUMMY                    0x01
#define   SPI_FLASH_RDSFDP_ADDR_BYTES               SPI_ADDR_3BYTE_ONLY
#define   ERASE_TYPICAL_TIME_UNITS_MASK             0x60
#define   ERASE_TYPICAL_TIME_BIT_POSITION           5
#define     ERASE_TYPICAL_TIME_UNIT_1_MS_BITMAP     0x00
#define     ERASE_TYPICAL_TIME_UNIT_1_MS            1
#define     ERASE_TYPICAL_TIME_UNIT_16_MS_BITMAP    0x01
#define     ERASE_TYPICAL_TIME_UNIT_16_MS           16
#define     ERASE_TYPICAL_TIME_UNIT_128_MS_BITMAP   0x02
#define     ERASE_TYPICAL_TIME_UNIT_128_MS          128
#define     ERASE_TYPICAL_TIME_UNIT_1000_MS_BITMAP  0x03
#define     ERASE_TYPICAL_TIME_UNIT_1000_MS         1000
#define   ERASE_TYPICAL_TIME_COUNT_MASK             0x1f

///
/// Flash Device Configuration Detection Command descriptor.
///
typedef struct {
  // DWORD 1
  UINT32    DescriptorEnd              : 1; ///< Descriptor Sequence End Indicator.
  UINT32    DescriptorType             : 1; ///< Descriptor Type.
  UINT32    Reserve1                   : 6; ///< Bit [7:2] is reserved.
  UINT32    DetectionInstruction       : 8; ///< Sector map configuration detection command.
  UINT32    DetectionLatency           : 4; ///< Configuration detection command read latency.
  UINT32    Reserve2                   : 2; ///< Bit [21:20] is reserved.
  UINT32    DetectionCommandAddressLen : 2; ///< Configuration detection command address length.
  UINT32    ReadDataMask               : 8; ///< Bit mask of the interst bit of the returned
                                            ///< byte read from the detection command.
  // DWORD 2
  UINT32    CommandAddress             : 32; ///< Sector map configuration detection command address.
} SFDP_SECTOR_CONFIGURATION_COMMAND;

#define SFDP_SECTOR_MAP_TABLE_ENTRY_TYPE_COMMAND  0
#define SFDP_SECTOR_MAP_TABLE_ENTRY_TYPE_MAP      1
#define SFDP_SECTOR_MAP_TABLE_ENTRY_LAST          1

///
/// Definition of Configuration detection command address length.
///
typedef enum {
  SpdfConfigurationCommandAddressNone     = 0,
  SpdfConfigurationCommandAddress3Byte    = 1,
  SpdfConfigurationCommandAddress4Byte    = 2,
  SpdfConfigurationCommandAddressVariable = 3
} SPDF_CONFIGURATION_COMMAND_ADDR_LENGTH;

///
/// Flash Device Configuration Map descriptor.
///
typedef struct {
  // DWORD 1
  UINT32    DescriptorEnd   : 1;          ///< Descriptor Sequence End Indicator.
  UINT32    DescriptorType  : 1;          ///< Descriptor Type.
  UINT32    Reserve1        : 6;          ///< Bit [7:2] is reserved.
  UINT32    ConfigurationID : 8;          ///< ID of this configuration.
  UINT32    RegionCount     : 8;          ///< The region count of this configuration.
  UINT32    Reserve2        : 8;          ///< [31:24] is reserved.
} SFDP_SECTOR_CONFIGURATION_MAP;

typedef struct {
  UINT32    DescriptorEnd  : 1;           ///< Descriptor Sequence End Indicator.
  UINT32    DescriptorType : 1;           ///< Descriptor Type.
} SFDP_SECTOR_CONFIGURATION_GENERIC_HEADER;

///
/// Flash Device Region Definition.
///
typedef struct _SFDP_SECTOR_REGION {
  // DWORD 1
  UINT32    EraseType1 : 1;               ///< Earse type 1 is supported.
  UINT32    EraseType2 : 1;               ///< Earse type 2 is supported.
  UINT32    EraseType3 : 1;               ///< Earse type 3 is supported.
  UINT32    EraseType4 : 1;               ///< Earse type 4 is supported.
  UINT32    Reserve1   : 4;               ///< Bit [7:4] is reserved.
  UINT32    RegionSize : 24;              ///< Region size in 256 Byte unit.
} SFDP_SECTOR_REGION;
#define SFDP_SECTOR_REGION_SIZE_UNIT  256

///
/// Sector Map Table structure, the entry could be
/// either Configuration Detection Command descriptor,
/// or Configuration Map descriptor.
///
typedef union _SFDP_SECTOR_MAP_TABLE {
  SFDP_SECTOR_CONFIGURATION_GENERIC_HEADER    GenericHeader;
  SFDP_SECTOR_CONFIGURATION_COMMAND           ConfigurationCommand; ///< Fash configuration detection command.
  SFDP_SECTOR_CONFIGURATION_MAP               ConfigurationMap;     ///< Flash map descriptor.
} SFDP_SECTOR_MAP_TABLE;

#endif // SPI_NOR_FLASH_JEDEC_H_
