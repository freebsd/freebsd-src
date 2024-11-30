/** @file
  This file contains definitions for SPD LPDDR.

  Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    - Serial Presence Detect (SPD) for LPDDR3 and LPDDR4 SDRAM Modules Document Release 2
      http://www.jedec.org/standards-documents/docs/spd412m-2
**/

#ifndef _SDRAM_SPD_LPDDR_H_
#define _SDRAM_SPD_LPDDR_H_

#pragma pack (push, 1)

typedef union {
  struct {
    UINT8    BytesUsed   :  4;                       ///< Bits 3:0
    UINT8    BytesTotal  :  3;                       ///< Bits 6:4
    UINT8    CrcCoverage :  1;                       ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD_LPDDR_DEVICE_DESCRIPTION_STRUCT;

typedef union {
  struct {
    UINT8    Minor :  4;                             ///< Bits 3:0
    UINT8    Major :  4;                             ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD_LPDDR_REVISION_STRUCT;

typedef union {
  struct {
    UINT8    Type :  8;                              ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD_LPDDR_DRAM_DEVICE_TYPE_STRUCT;

typedef union {
  struct {
    UINT8    ModuleType  :  4;                       ///< Bits 3:0
    UINT8    HybridMedia :  3;                       ///< Bits 6:4
    UINT8    Hybrid      :  1;                       ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD_LPDDR_MODULE_TYPE_STRUCT;

typedef union {
  struct {
    UINT8    Density     :  4;                       ///< Bits 3:0
    UINT8    BankAddress :  2;                       ///< Bits 5:4
    UINT8    BankGroup   :  2;                       ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD_LPDDR_SDRAM_DENSITY_BANKS_STRUCT;

typedef union {
  struct {
    UINT8    ColumnAddress :  3;                     ///< Bits 2:0
    UINT8    RowAddress    :  3;                     ///< Bits 5:3
    UINT8    Reserved      :  2;                     ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD_LPDDR_SDRAM_ADDRESSING_STRUCT;

typedef union {
  struct {
    UINT8    SignalLoading    :  2;                  ///< Bits 1:0
    UINT8    ChannelsPerDie   :  2;                  ///< Bits 3:2
    UINT8    DieCount         :  3;                  ///< Bits 6:4
    UINT8    SdramPackageType :  1;                  ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD_LPDDR_SDRAM_PACKAGE_TYPE_STRUCT;

typedef union {
  struct {
    UINT8    MaximumActivateCount  :  4;             ///< Bits 3:0
    UINT8    MaximumActivateWindow :  2;             ///< Bits 5:4
    UINT8    Reserved              :  2;             ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD_LPDDR_SDRAM_OPTIONAL_FEATURES_STRUCT;

typedef union {
  struct {
    UINT8    Reserved :  8;                          ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD_LPDDR_SDRAM_THERMAL_REFRESH_STRUCT;

typedef union {
  struct {
    UINT8    Reserved          :  5;                 ///< Bits 4:0
    UINT8    SoftPPR           :  1;                 ///< Bits 5:5
    UINT8    PostPackageRepair :  2;                 ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD_LPDDR_OTHER_SDRAM_OPTIONAL_FEATURES_STRUCT;

typedef union {
  struct {
    UINT8    OperationAt1_20  :  1;                  ///< Bits 0:0
    UINT8    EndurantAt1_20   :  1;                  ///< Bits 1:1
    UINT8    OperationAt1_10  :  1;                  ///< Bits 2:2
    UINT8    EndurantAt1_10   :  1;                  ///< Bits 3:3
    UINT8    OperationAtTBD2V :  1;                  ///< Bits 4:4
    UINT8    EndurantAtTBD2V  :  1;                  ///< Bits 5:5
    UINT8    Reserved         :  2;                  ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD_LPDDR_MODULE_NOMINAL_VOLTAGE_STRUCT;

typedef union {
  struct {
    UINT8    SdramDeviceWidth :  3;                  ///< Bits 2:0
    UINT8    RankCount        :  3;                  ///< Bits 5:3
    UINT8    Reserved         :  2;                  ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD_LPDDR_MODULE_ORGANIZATION_STRUCT;

typedef union {
  struct {
    UINT8    PrimaryBusWidth   :  3;                 ///< Bits 2:0
    UINT8    BusWidthExtension :  2;                 ///< Bits 4:3
    UINT8    NumberofChannels  :  3;                 ///< Bits 7:5
  } Bits;
  UINT8    Data;
} SPD_LPDDR_MODULE_MEMORY_BUS_WIDTH_STRUCT;

typedef union {
  struct {
    UINT8    Reserved              :  7;             ///< Bits 6:0
    UINT8    ThermalSensorPresence :  1;             ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD_LPDDR_MODULE_THERMAL_SENSOR_STRUCT;

typedef union {
  struct {
    UINT8    ExtendedBaseModuleType :  4;            ///< Bits 3:0
    UINT8    Reserved               :  4;            ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD_LPDDR_EXTENDED_MODULE_TYPE_STRUCT;

typedef union {
  struct {
    UINT8    ChipSelectLoading                 :  3; ///< Bits 2:0
    UINT8    CommandAddressControlClockLoading :  3; ///< Bits 5:3
    UINT8    DataStrobeMaskLoading             :  2; ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD_LPDDR_SIGNAL_LOADING_STRUCT;

typedef union {
  struct {
    UINT8    Fine     :  2;                          ///< Bits 1:0
    UINT8    Medium   :  2;                          ///< Bits 3:2
    UINT8    Reserved :  4;                          ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD_LPDDR_TIMEBASE_STRUCT;

typedef union {
  struct {
    UINT8    tCKmin :  8;                            ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD_LPDDR_TCK_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tCKmax :  8;                            ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD_LPDDR_TCK_MAX_MTB_STRUCT;

typedef union {
  struct {
    UINT32    Cl3       :  1;                         ///< Bits 0:0
    UINT32    Cl6       :  1;                         ///< Bits 1:1
    UINT32    Cl8       :  1;                         ///< Bits 2:2
    UINT32    Cl9       :  1;                         ///< Bits 3:3
    UINT32    Cl10      :  1;                         ///< Bits 4:4
    UINT32    Cl11      :  1;                         ///< Bits 5:5
    UINT32    Cl12      :  1;                         ///< Bits 6:6
    UINT32    Cl14      :  1;                         ///< Bits 7:7
    UINT32    Cl16      :  1;                         ///< Bits 8:8
    UINT32    Reserved0 :  1;                         ///< Bits 9:9
    UINT32    Cl20      :  1;                         ///< Bits 10:10
    UINT32    Cl22      :  1;                         ///< Bits 11:11
    UINT32    Cl24      :  1;                         ///< Bits 12:12
    UINT32    Reserved1 :  1;                         ///< Bits 13:13
    UINT32    Cl28      :  1;                         ///< Bits 14:14
    UINT32    Reserved2 :  1;                         ///< Bits 15:15
    UINT32    Cl32      :  1;                         ///< Bits 16:16
    UINT32    Reserved3 :  1;                         ///< Bits 17:17
    UINT32    Cl36      :  1;                         ///< Bits 18:18
    UINT32    Reserved4 :  1;                         ///< Bits 19:19
    UINT32    Cl40      :  1;                         ///< Bits 20:20
    UINT32    Reserved5 :  11;                        ///< Bits 31:21
  } Bits;
  UINT32    Data;
  UINT16    Data16[2];
  UINT8     Data8[4];
} SPD_LPDDR_CAS_LATENCIES_SUPPORTED_STRUCT;

typedef union {
  struct {
    UINT8    tAAmin :  8;                            ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD_LPDDR_TAA_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    ReadLatencyMode :  2;                   ///< Bits 1:0
    UINT8    WriteLatencySet :  2;                   ///< Bits 3:2
    UINT8    Reserved        :  4;                   ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD_LPDDR_RW_LATENCY_OPTION_STRUCT;

typedef union {
  struct {
    UINT8    tRCDmin :  8;                           ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD_LPDDR_TRCD_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tRPab :  8;                             ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD_LPDDR_TRP_AB_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tRPpb :  8;                             ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD_LPDDR_TRP_PB_MTB_STRUCT;

typedef union {
  struct {
    UINT16    tRFCab :  16;                           ///< Bits 15:0
  } Bits;
  UINT16    Data;
  UINT8     Data8[2];
} SPD_LPDDR_TRFC_AB_MTB_STRUCT;

typedef union {
  struct {
    UINT16    tRFCpb :  16;                           ///< Bits 15:0
  } Bits;
  UINT16    Data;
  UINT8     Data8[2];
} SPD_LPDDR_TRFC_PB_MTB_STRUCT;

typedef union {
  struct {
    UINT8    BitOrderatSDRAM         :  5;           ///< Bits 4:0
    UINT8    WiredtoUpperLowerNibble :  1;           ///< Bits 5:5
    UINT8    PackageRankMap          :  2;           ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD_LPDDR_CONNECTOR_BIT_MAPPING_BYTE_STRUCT;

typedef union {
  struct {
    INT8    tRPpbFine :  8;                          ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD_LPDDR_TRP_PB_FTB_STRUCT;

typedef union {
  struct {
    INT8    tRPabFine :  8;                          ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD_LPDDR_TRP_AB_FTB_STRUCT;

typedef union {
  struct {
    INT8    tRCDminFine :  8;                        ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD_LPDDR_TRCD_MIN_FTB_STRUCT;

typedef union {
  struct {
    INT8    tAAminFine :  8;                         ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD_LPDDR_TAA_MIN_FTB_STRUCT;

typedef union {
  struct {
    INT8    tCKmaxFine :  8;                         ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD_LPDDR_TCK_MAX_FTB_STRUCT;

typedef union {
  struct {
    INT8    tCKminFine :  8;                         ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD_LPDDR_TCK_MIN_FTB_STRUCT;

typedef union {
  struct {
    UINT16    ContinuationCount  :  7;               ///< Bits 6:0
    UINT16    ContinuationParity :  1;               ///< Bits 7:7
    UINT16    LastNonZeroByte    :  8;               ///< Bits 15:8
  } Bits;
  UINT16    Data;
  UINT8     Data8[2];
} SPD_LPDDR_MANUFACTURER_ID_CODE;

typedef struct {
  UINT8    Location;                           ///< Module Manufacturing Location
} SPD_LPDDR_MANUFACTURING_LOCATION;

typedef struct {
  UINT8    Year;                               ///< Year represented in BCD (00h = 2000)
  UINT8    Week;                               ///< Year represented in BCD (47h = week 47)
} SPD_LPDDR_MANUFACTURING_DATE;

typedef union {
  UINT32    Data;
  UINT16    SerialNumber16[2];
  UINT8     SerialNumber8[4];
} SPD_LPDDR_MANUFACTURER_SERIAL_NUMBER;

typedef struct {
  SPD_LPDDR_MANUFACTURER_ID_CODE          IdCode;                     ///< Module Manufacturer ID Code
  SPD_LPDDR_MANUFACTURING_LOCATION        Location;                   ///< Module Manufacturing Location
  SPD_LPDDR_MANUFACTURING_DATE            Date;                       ///< Module Manufacturing Year, in BCD (range: 2000-2255)
  SPD_LPDDR_MANUFACTURER_SERIAL_NUMBER    SerialNumber;               ///< Module Serial Number
} SPD_LPDDR_UNIQUE_MODULE_ID;

typedef union {
  struct {
    UINT8    FrontThickness :  4;                    ///< Bits 3:0
    UINT8    BackThickness  :  4;                    ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD_LPDDR_MODULE_MAXIMUM_THICKNESS;

typedef union {
  struct {
    UINT8    Height           :  5;                  ///< Bits 4:0
    UINT8    RawCardExtension :  3;                  ///< Bits 7:5
  } Bits;
  UINT8    Data;
} SPD_LPDDR_MODULE_NOMINAL_HEIGHT;

typedef union {
  struct {
    UINT8    Card      :  5;                         ///< Bits 4:0
    UINT8    Revision  :  2;                         ///< Bits 6:5
    UINT8    Extension :  1;                         ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD_LPDDR_REFERENCE_RAW_CARD;

typedef union {
  UINT16    Crc[1];
  UINT8     Data8[2];
} SPD_LPDDR_CYCLIC_REDUNDANCY_CODE;

typedef struct {
  SPD_LPDDR_DEVICE_DESCRIPTION_STRUCT               Description;              ///< 0       Number of Serial PD Bytes Written / SPD Device Size / CRC Coverage 1, 2
  SPD_LPDDR_REVISION_STRUCT                         Revision;                 ///< 1       SPD Revision
  SPD_LPDDR_DRAM_DEVICE_TYPE_STRUCT                 DramDeviceType;           ///< 2       DRAM Device Type
  SPD_LPDDR_MODULE_TYPE_STRUCT                      ModuleType;               ///< 3       Module Type
  SPD_LPDDR_SDRAM_DENSITY_BANKS_STRUCT              SdramDensityAndBanks;     ///< 4       SDRAM Density and Banks
  SPD_LPDDR_SDRAM_ADDRESSING_STRUCT                 SdramAddressing;          ///< 5       SDRAM Addressing
  SPD_LPDDR_SDRAM_PACKAGE_TYPE_STRUCT               SdramPackageType;         ///< 6       SDRAM Package Type
  SPD_LPDDR_SDRAM_OPTIONAL_FEATURES_STRUCT          SdramOptionalFeatures;    ///< 7       SDRAM Optional Features
  SPD_LPDDR_SDRAM_THERMAL_REFRESH_STRUCT            ThermalAndRefreshOptions; ///< 8       SDRAM Thermal and Refresh Options
  SPD_LPDDR_OTHER_SDRAM_OPTIONAL_FEATURES_STRUCT    OtherOptionalFeatures;    ///< 9      Other SDRAM Optional Features
  UINT8                                             Reserved0;                ///< 10      Reserved
  SPD_LPDDR_MODULE_NOMINAL_VOLTAGE_STRUCT           ModuleNominalVoltage;     ///< 11      Module Nominal Voltage, VDD
  SPD_LPDDR_MODULE_ORGANIZATION_STRUCT              ModuleOrganization;       ///< 12      Module Organization
  SPD_LPDDR_MODULE_MEMORY_BUS_WIDTH_STRUCT          ModuleMemoryBusWidth;     ///< 13      Module Memory Bus Width
  SPD_LPDDR_MODULE_THERMAL_SENSOR_STRUCT            ModuleThermalSensor;      ///< 14      Module Thermal Sensor
  SPD_LPDDR_EXTENDED_MODULE_TYPE_STRUCT             ExtendedModuleType;       ///< 15      Extended Module Type
  SPD_LPDDR_SIGNAL_LOADING_STRUCT                   SignalLoading;            ///< 16      Signal Loading
  SPD_LPDDR_TIMEBASE_STRUCT                         Timebase;                 ///< 17      Timebases
  SPD_LPDDR_TCK_MIN_MTB_STRUCT                      tCKmin;                   ///< 18      SDRAM Minimum Cycle Time (tCKmin)
  SPD_LPDDR_TCK_MAX_MTB_STRUCT                      tCKmax;                   ///< 19      SDRAM Maximum Cycle Time (tCKmax)
  SPD_LPDDR_CAS_LATENCIES_SUPPORTED_STRUCT          CasLatencies;             ///< 20-23   CAS Latencies Supported
  SPD_LPDDR_TAA_MIN_MTB_STRUCT                      tAAmin;                   ///< 24      Minimum CAS Latency Time (tAAmin)
  SPD_LPDDR_RW_LATENCY_OPTION_STRUCT                LatencySetOptions;        ///< 25      Read and Write Latency Set Options
  SPD_LPDDR_TRCD_MIN_MTB_STRUCT                     tRCDmin;                  ///< 26      Minimum RAS# to CAS# Delay Time (tRCDmin)
  SPD_LPDDR_TRP_AB_MTB_STRUCT                       tRPab;                    ///< 27      Minimum Row Precharge Delay Time (tRPab), all banks
  SPD_LPDDR_TRP_PB_MTB_STRUCT                       tRPpb;                    ///< 28      Minimum Row Precharge Delay Time (tRPpb), per bank
  SPD_LPDDR_TRFC_AB_MTB_STRUCT                      tRFCab;                   ///< 29-30   Minimum Refresh Recovery Delay Time (tRFCab), all banks
  SPD_LPDDR_TRFC_PB_MTB_STRUCT                      tRFCpb;                   ///< 31-32   Minimum Refresh Recovery Delay Time (tRFCpb), per bank
  UINT8                                             Reserved1[59 - 33 + 1];   ///< 33-59   Reserved
  SPD_LPDDR_CONNECTOR_BIT_MAPPING_BYTE_STRUCT       BitMapping[77 - 60 + 1];  ///< 60-77   Connector to SDRAM Bit Mapping
  UINT8                                             Reserved2[119 - 78 + 1];  ///< 78-119  Reserved
  SPD_LPDDR_TRP_PB_FTB_STRUCT                       tRPpbFine;                ///< 120     Fine Offset for Minimum Row Precharge Delay Time (tRPpbFine), per bank
  SPD_LPDDR_TRP_AB_FTB_STRUCT                       tRPabFine;                ///< 121     Fine Offset for Minimum Row Precharge Delay Time (tRPabFine), all ranks
  SPD_LPDDR_TRCD_MIN_FTB_STRUCT                     tRCDminFine;              ///< 122     Fine Offset for Minimum RAS# to CAS# Delay Time (tRCDmin)
  SPD_LPDDR_TAA_MIN_FTB_STRUCT                      tAAminFine;               ///< 123     Fine Offset for Minimum CAS Latency Time (tAAmin)
  SPD_LPDDR_TCK_MAX_FTB_STRUCT                      tCKmaxFine;               ///< 124     Fine Offset for SDRAM Maximum Cycle Time (tCKmax)
  SPD_LPDDR_TCK_MIN_FTB_STRUCT                      tCKminFine;               ///< 125     Fine Offset for SDRAM Minimum Cycle Time (tCKmin)
  SPD_LPDDR_CYCLIC_REDUNDANCY_CODE                  Crc;                      ///< 126-127 Cyclical Redundancy Code (CRC)
} SPD_LPDDR_BASE_SECTION;

typedef struct {
  SPD_LPDDR_MODULE_NOMINAL_HEIGHT       ModuleNominalHeight;        ///< 128     Module Nominal Height
  SPD_LPDDR_MODULE_MAXIMUM_THICKNESS    ModuleMaximumThickness;     ///< 129     Module Maximum Thickness
  SPD_LPDDR_REFERENCE_RAW_CARD          ReferenceRawCardUsed;       ///< 130     Reference Raw Card Used
  UINT8                                 Reserved[253 - 131 + 1];    ///< 131-253 Reserved
  SPD_LPDDR_CYCLIC_REDUNDANCY_CODE      Crc;                        ///< 254-255 Cyclical Redundancy Code (CRC)
} SPD_LPDDR_MODULE_LPDIMM;

typedef struct {
  SPD_LPDDR_MODULE_LPDIMM    LpDimm;                                ///< 128-255 Unbuffered Memory Module Types
} SPD_LPDDR_MODULE_SPECIFIC;

typedef struct {
  UINT8    ModulePartNumber[348 - 329 + 1];                                ///< 329-348 Module Part Number
} SPD_LPDDR_MODULE_PART_NUMBER;

typedef struct {
  UINT8    ManufacturerSpecificData[381 - 353 + 1];                                ///< 353-381 Manufacturer's Specific Data
} SPD_LPDDR_MANUFACTURER_SPECIFIC;

typedef UINT8  SPD_LPDDR_MODULE_REVISION_CODE;                           ///< 349     Module Revision Code
typedef UINT8  SPD_LPDDR_DRAM_STEPPING;                                  ///< 352     Dram Stepping

typedef struct {
  SPD_LPDDR_UNIQUE_MODULE_ID         ModuleId;                      ///< 320-328 Unique Module ID
  SPD_LPDDR_MODULE_PART_NUMBER       ModulePartNumber;              ///< 329-348 Module Part Number
  SPD_LPDDR_MODULE_REVISION_CODE     ModuleRevisionCode;            ///< 349     Module Revision Code
  SPD_LPDDR_MANUFACTURER_ID_CODE     DramIdCode;                    ///< 350-351 Dram Manufacturer ID Code
  SPD_LPDDR_DRAM_STEPPING            DramStepping;                  ///< 352     Dram Stepping
  SPD_LPDDR_MANUFACTURER_SPECIFIC    ManufacturerSpecificData;      ///< 353-381 Manufacturer's Specific Data
  UINT8                              Reserved[383 - 382 + 1];       ///< 382-383 Reserved
} SPD_LPDDR_MANUFACTURING_DATA;

typedef struct {
  UINT8    Reserved[511 - 384 + 1];                                 ///< 384-511 End User Programmable
} SPD_LPDDR_END_USER_SECTION;

///
/// LPDDR Serial Presence Detect structure
///
typedef struct {
  SPD_LPDDR_BASE_SECTION          Base;                             ///< 0-127   Base Configuration and DRAM Parameters
  SPD_LPDDR_MODULE_SPECIFIC       Module;                           ///< 128-255 Module-Specific Section
  UINT8                           Reserved[319 - 256 + 1];          ///< 256-319 Hybrid Memory Parameters
  SPD_LPDDR_MANUFACTURING_DATA    ManufactureInfo;                  ///< 320-383 Manufacturing Information
  SPD_LPDDR_END_USER_SECTION      EndUser;                          ///< 384-511 End User Programmable
} SPD_LPDDR;

#pragma pack (pop)
#endif
