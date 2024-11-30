/** @file
  This file contains definitions for SPD DDR4.

  Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    - Serial Presence Detect (SPD) for DDR4 SDRAM Modules Document Release 4
      http://www.jedec.org/standards-documents/docs/spd412l-4
**/

#ifndef _SDRAM_SPD_DDR4_H_
#define _SDRAM_SPD_DDR4_H_

#pragma pack (push, 1)

typedef union {
  struct {
    UINT8    BytesUsed   :  4;                       ///< Bits 3:0
    UINT8    BytesTotal  :  3;                       ///< Bits 6:4
    UINT8    CrcCoverage :  1;                       ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_DEVICE_DESCRIPTION_STRUCT;

typedef union {
  struct {
    UINT8    Minor :  4;                             ///< Bits 3:0
    UINT8    Major :  4;                             ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_REVISION_STRUCT;

typedef union {
  struct {
    UINT8    Type :  8;                              ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_DRAM_DEVICE_TYPE_STRUCT;

typedef union {
  struct {
    UINT8    ModuleType  :  4;                       ///< Bits 3:0
    UINT8    HybridMedia :  3;                       ///< Bits 6:4
    UINT8    Hybrid      :  1;                       ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_MODULE_TYPE_STRUCT;

typedef union {
  struct {
    UINT8    Density     :  4;                       ///< Bits 3:0
    UINT8    BankAddress :  2;                       ///< Bits 5:4
    UINT8    BankGroup   :  2;                       ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD4_SDRAM_DENSITY_BANKS_STRUCT;

typedef union {
  struct {
    UINT8    ColumnAddress :  3;                     ///< Bits 2:0
    UINT8    RowAddress    :  3;                     ///< Bits 5:3
    UINT8    Reserved      :  2;                     ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD4_SDRAM_ADDRESSING_STRUCT;

typedef union {
  struct {
    UINT8    SignalLoading    :  2;                  ///< Bits 1:0
    UINT8    Reserved         :  2;                  ///< Bits 3:2
    UINT8    DieCount         :  3;                  ///< Bits 6:4
    UINT8    SdramPackageType :  1;                  ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_PRIMARY_SDRAM_PACKAGE_TYPE_STRUCT;

typedef union {
  struct {
    UINT8    MaximumActivateCount  :  4;             ///< Bits 3:0
    UINT8    MaximumActivateWindow :  2;             ///< Bits 5:4
    UINT8    Reserved              :  2;             ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD4_SDRAM_OPTIONAL_FEATURES_STRUCT;

typedef union {
  struct {
    UINT8    Reserved :  8;                          ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_SDRAM_THERMAL_REFRESH_STRUCT;

typedef union {
  struct {
    UINT8    Reserved          :  5;                 ///< Bits 4:0
    UINT8    SoftPPR           :  1;                 ///< Bits 5:5
    UINT8    PostPackageRepair :  2;                 ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD4_OTHER_SDRAM_OPTIONAL_FEATURES_STRUCT;

typedef union {
  struct {
    UINT8    SignalLoading    :  2;                  ///< Bits 1:0
    UINT8    DRAMDensityRatio :  2;                  ///< Bits 3:2
    UINT8    DieCount         :  3;                  ///< Bits 6:4
    UINT8    SdramPackageType :  1;                  ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_SECONDARY_SDRAM_PACKAGE_TYPE_STRUCT;

typedef union {
  struct {
    UINT8    OperationAt1_20 :  1;                   ///< Bits 0:0
    UINT8    EndurantAt1_20  :  1;                   ///< Bits 1:1
    UINT8    Reserved        :  6;                   ///< Bits 7:2
  } Bits;
  UINT8    Data;
} SPD4_MODULE_NOMINAL_VOLTAGE_STRUCT;

typedef union {
  struct {
    UINT8    SdramDeviceWidth :  3;                  ///< Bits 2:0
    UINT8    RankCount        :  3;                  ///< Bits 5:3
    UINT8    RankMix          :  1;                  ///< Bits 6:6
    UINT8    Reserved         :  1;                  ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_MODULE_ORGANIZATION_STRUCT;

typedef union {
  struct {
    UINT8    PrimaryBusWidth   :  3;                 ///< Bits 2:0
    UINT8    BusWidthExtension :  2;                 ///< Bits 4:3
    UINT8    Reserved          :  3;                 ///< Bits 7:5
  } Bits;
  UINT8    Data;
} SPD4_MODULE_MEMORY_BUS_WIDTH_STRUCT;

typedef union {
  struct {
    UINT8    Reserved              :  7;             ///< Bits 6:0
    UINT8    ThermalSensorPresence :  1;             ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_MODULE_THERMAL_SENSOR_STRUCT;

typedef union {
  struct {
    UINT8    ExtendedBaseModuleType :  4;            ///< Bits 3:0
    UINT8    Reserved               :  4;            ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_EXTENDED_MODULE_TYPE_STRUCT;

typedef union {
  struct {
    UINT8    Fine     :  2;                          ///< Bits 1:0
    UINT8    Medium   :  2;                          ///< Bits 3:2
    UINT8    Reserved :  4;                          ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_TIMEBASE_STRUCT;

typedef union {
  struct {
    UINT8    tCKmin :  8;                            ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_TCK_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tCKmax :  8;                            ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_TCK_MAX_MTB_STRUCT;

typedef union {
  struct {
    UINT32    Cl7      :  1;                         ///< Bits 0:0
    UINT32    Cl8      :  1;                         ///< Bits 1:1
    UINT32    Cl9      :  1;                         ///< Bits 2:2
    UINT32    Cl10     :  1;                         ///< Bits 3:3
    UINT32    Cl11     :  1;                         ///< Bits 4:4
    UINT32    Cl12     :  1;                         ///< Bits 5:5
    UINT32    Cl13     :  1;                         ///< Bits 6:6
    UINT32    Cl14     :  1;                         ///< Bits 7:7
    UINT32    Cl15     :  1;                         ///< Bits 8:8
    UINT32    Cl16     :  1;                         ///< Bits 9:9
    UINT32    Cl17     :  1;                         ///< Bits 10:10
    UINT32    Cl18     :  1;                         ///< Bits 11:11
    UINT32    Cl19     :  1;                         ///< Bits 12:12
    UINT32    Cl20     :  1;                         ///< Bits 13:13
    UINT32    Cl21     :  1;                         ///< Bits 14:14
    UINT32    Cl22     :  1;                         ///< Bits 15:15
    UINT32    Cl23     :  1;                         ///< Bits 16:16
    UINT32    Cl24     :  1;                         ///< Bits 17:17
    UINT32    Cl25     :  1;                         ///< Bits 18:18
    UINT32    Cl26     :  1;                         ///< Bits 19:19
    UINT32    Cl27     :  1;                         ///< Bits 20:20
    UINT32    Cl28     :  1;                         ///< Bits 21:21
    UINT32    Cl29     :  1;                         ///< Bits 22:22
    UINT32    Cl30     :  1;                         ///< Bits 23:23
    UINT32    Cl31     :  1;                         ///< Bits 24:24
    UINT32    Cl32     :  1;                         ///< Bits 25:25
    UINT32    Cl33     :  1;                         ///< Bits 26:26
    UINT32    Cl34     :  1;                         ///< Bits 27:27
    UINT32    Cl35     :  1;                         ///< Bits 28:28
    UINT32    Cl36     :  1;                         ///< Bits 29:29
    UINT32    Reserved :  1;                         ///< Bits 30:30
    UINT32    ClRange  :  1;                         ///< Bits 31:31
  } Bits;
  struct {
    UINT32    Cl23     :  1;                         ///< Bits 0:0
    UINT32    Cl24     :  1;                         ///< Bits 1:1
    UINT32    Cl25     :  1;                         ///< Bits 2:2
    UINT32    Cl26     :  1;                         ///< Bits 3:3
    UINT32    Cl27     :  1;                         ///< Bits 4:4
    UINT32    Cl28     :  1;                         ///< Bits 5:5
    UINT32    Cl29     :  1;                         ///< Bits 6:6
    UINT32    Cl30     :  1;                         ///< Bits 7:7
    UINT32    Cl31     :  1;                         ///< Bits 8:8
    UINT32    Cl32     :  1;                         ///< Bits 9:9
    UINT32    Cl33     :  1;                         ///< Bits 10:10
    UINT32    Cl34     :  1;                         ///< Bits 11:11
    UINT32    Cl35     :  1;                         ///< Bits 12:12
    UINT32    Cl36     :  1;                         ///< Bits 13:13
    UINT32    Cl37     :  1;                         ///< Bits 14:14
    UINT32    Cl38     :  1;                         ///< Bits 15:15
    UINT32    Cl39     :  1;                         ///< Bits 16:16
    UINT32    Cl40     :  1;                         ///< Bits 17:17
    UINT32    Cl41     :  1;                         ///< Bits 18:18
    UINT32    Cl42     :  1;                         ///< Bits 19:19
    UINT32    Cl43     :  1;                         ///< Bits 20:20
    UINT32    Cl44     :  1;                         ///< Bits 21:21
    UINT32    Cl45     :  1;                         ///< Bits 22:22
    UINT32    Cl46     :  1;                         ///< Bits 23:23
    UINT32    Cl47     :  1;                         ///< Bits 24:24
    UINT32    Cl48     :  1;                         ///< Bits 25:25
    UINT32    Cl49     :  1;                         ///< Bits 26:26
    UINT32    Cl50     :  1;                         ///< Bits 27:27
    UINT32    Cl51     :  1;                         ///< Bits 28:28
    UINT32    Cl52     :  1;                         ///< Bits 29:29
    UINT32    Reserved :  1;                         ///< Bits 30:30
    UINT32    ClRange  :  1;                         ///< Bits 31:31
  } HighRangeBits;
  UINT32    Data;
  UINT16    Data16[2];
  UINT8     Data8[4];
} SPD4_CAS_LATENCIES_SUPPORTED_STRUCT;

typedef union {
  struct {
    UINT8    tAAmin :  8;                            ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_TAA_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tRCDmin :  8;                           ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_TRCD_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tRPmin :  8;                            ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_TRP_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tRASminUpper :  4;                      ///< Bits 3:0
    UINT8    tRCminUpper  :  4;                      ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_TRAS_TRC_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tRASmin :  8;                           ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_TRAS_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tRCmin :  8;                            ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_TRC_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT16    tRFCmin :  16;                          ///< Bits 15:0
  } Bits;
  UINT16    Data;
  UINT8     Data8[2];
} SPD4_TRFC_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tFAWminUpper :  4;                      ///< Bits 3:0
    UINT8    Reserved     :  4;                      ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_TFAW_MIN_MTB_UPPER_STRUCT;

typedef union {
  struct {
    UINT8    tFAWmin :  8;                           ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_TFAW_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tRRDmin :  8;                           ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_TRRD_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tCCDmin :  8;                           ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_TCCD_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tWRminMostSignificantNibble :  4;       ///< Bits 3:0
    UINT8    Reserved                    :  4;       ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_TWR_UPPER_NIBBLE_STRUCT;

typedef union {
  struct {
    UINT8    tWRmin :  8;                            ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_TWR_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    tWTR_SminMostSignificantNibble :  4;    ///< Bits 3:0
    UINT8    tWTR_LminMostSignificantNibble :  4;    ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_TWTR_UPPER_NIBBLE_STRUCT;

typedef union {
  struct {
    UINT8    tWTRmin :  8;                           ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_TWTR_MIN_MTB_STRUCT;

typedef union {
  struct {
    UINT8    BitOrderatSDRAM         :  5;           ///< Bits 4:0
    UINT8    WiredtoUpperLowerNibble :  1;           ///< Bits 5:5
    UINT8    PackageRankMap          :  2;           ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD4_CONNECTOR_BIT_MAPPING_BYTE_STRUCT;

typedef union {
  struct {
    INT8    tCCDminFine :  8;                        ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD4_TCCD_MIN_FTB_STRUCT;

typedef union {
  struct {
    INT8    tRRDminFine :  8;                        ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD4_TRRD_MIN_FTB_STRUCT;

typedef union {
  struct {
    INT8    tRCminFine :  8;                         ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD4_TRC_MIN_FTB_STRUCT;

typedef union {
  struct {
    INT8    tRPminFine :  8;                         ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD4_TRP_MIN_FTB_STRUCT;

typedef union {
  struct {
    INT8    tRCDminFine :  8;                        ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD4_TRCD_MIN_FTB_STRUCT;

typedef union {
  struct {
    INT8    tAAminFine :  8;                         ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD4_TAA_MIN_FTB_STRUCT;

typedef union {
  struct {
    INT8    tCKmaxFine :  8;                         ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD4_TCK_MAX_FTB_STRUCT;

typedef union {
  struct {
    INT8    tCKminFine :  8;                         ///< Bits 7:0
  } Bits;
  INT8    Data;
} SPD4_TCK_MIN_FTB_STRUCT;

typedef union {
  struct {
    UINT8    Height           :  5;                  ///< Bits 4:0
    UINT8    RawCardExtension :  3;                  ///< Bits 7:5
  } Bits;
  UINT8    Data;
} SPD4_UNBUF_MODULE_NOMINAL_HEIGHT;

typedef union {
  struct {
    UINT8    FrontThickness :  4;                    ///< Bits 3:0
    UINT8    BackThickness  :  4;                    ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_UNBUF_MODULE_NOMINAL_THICKNESS;

typedef union {
  struct {
    UINT8    Card      :  5;                         ///< Bits 4:0
    UINT8    Revision  :  2;                         ///< Bits 6:5
    UINT8    Extension :  1;                         ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_UNBUF_REFERENCE_RAW_CARD;

typedef union {
  struct {
    UINT8    MappingRank1 :  1;                      ///< Bits 0:0
    UINT8    Reserved     :  7;                      ///< Bits 7:1
  } Bits;
  UINT8    Data;
} SPD4_UNBUF_ADDRESS_MAPPING;

typedef union {
  struct {
    UINT8    Height   :  5;                          ///< Bits 4:0
    UINT8    Reserved :  3;                          ///< Bits 7:5
  } Bits;
  UINT8    Data;
} SPD4_RDIMM_MODULE_NOMINAL_HEIGHT;

typedef union {
  struct {
    UINT8    FrontThickness :  4;                    ///< Bits 3:0
    UINT8    BackThickness  :  4;                    ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_RDIMM_MODULE_NOMINAL_THICKNESS;

typedef union {
  struct {
    UINT8    Card      :  5;                         ///< Bits 4:0
    UINT8    Revision  :  2;                         ///< Bits 6:5
    UINT8    Extension :  1;                         ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_RDIMM_REFERENCE_RAW_CARD;

typedef union {
  struct {
    UINT8    RegisterCount :  2;                     ///< Bits 1:0
    UINT8    DramRowCount  :  2;                     ///< Bits 3:2
    UINT8    RegisterType  :  4;                     ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_RDIMM_MODULE_ATTRIBUTES;

typedef union {
  struct {
    UINT8    HeatSpreaderThermalCharacteristics :  7; ///< Bits 6:0
    UINT8    HeatSpreaderSolution               :  1; ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_RDIMM_THERMAL_HEAT_SPREADER_SOLUTION;

typedef union {
  struct {
    UINT16    ContinuationCount  :  7;               ///< Bits 6:0
    UINT16    ContinuationParity :  1;               ///< Bits 7:7
    UINT16    LastNonZeroByte    :  8;               ///< Bits 15:8
  } Bits;
  UINT16    Data;
  UINT8     Data8[2];
} SPD4_MANUFACTURER_ID_CODE;

typedef union {
  struct {
    UINT8    RegisterRevisionNumber;           ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_RDIMM_REGISTER_REVISION_NUMBER;

typedef union {
  struct {
    UINT8    Rank1Mapping :  1;                     ///< Bits 0:0
    UINT8    Reserved     :  7;                     ///< Bits 7:1
  } Bits;
  UINT8    Data;
} SPD4_RDIMM_ADDRESS_MAPPING_FROM_REGISTER_TO_DRAM;

typedef union {
  struct {
    UINT8    Cke            :  2;                   ///< Bits 1:0
    UINT8    Odt            :  2;                   ///< Bits 3:2
    UINT8    CommandAddress :  2;                   ///< Bits 5:4
    UINT8    ChipSelect     :  2;                   ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD4_RDIMM_REGISTER_OUTPUT_DRIVE_STRENGTH_FOR_CONTROL_COMMAND_ADDRESS;

typedef union {
  struct {
    UINT8    Y0Y2                     :  2;         ///< Bits 1:0
    UINT8    Y1Y3                     :  2;         ///< Bits 3:2
    UINT8    Reserved0                :  2;         ///< Bits 5:4
    UINT8    RcdOutputSlewRateControl :  1;         ///< Bits 6:6
    UINT8    Reserved1                :  1;         ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_RDIMM_REGISTER_OUTPUT_DRIVE_STRENGTH_FOR_CLOCK;

typedef union {
  struct {
    UINT8    Height   :  5;                         ///< Bits 4:0
    UINT8    Reserved :  3;                         ///< Bits 7:5
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_MODULE_NOMINAL_HEIGHT;

typedef union {
  struct {
    UINT8    FrontThickness :  4;                   ///< Bits 3:0
    UINT8    BackThickness  :  4;                   ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_MODULE_NOMINAL_THICKNESS;

typedef union {
  struct {
    UINT8    Card      :  5;                        ///< Bits 4:0
    UINT8    Revision  :  2;                        ///< Bits 6:5
    UINT8    Extension :  1;                        ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_REFERENCE_RAW_CARD;

typedef union {
  struct {
    UINT8    RegisterCount :  2;                    ///< Bits 1:0
    UINT8    DramRowCount  :  2;                    ///< Bits 3:2
    UINT8    RegisterType  :  4;                    ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_MODULE_ATTRIBUTES;

typedef union {
  struct {
    UINT8    HeatSpreaderThermalCharacteristics :  7; ///< Bits 6:0
    UINT8    HeatSpreaderSolution               :  1; ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_THERMAL_HEAT_SPREADER_SOLUTION;

typedef union {
  struct {
    UINT8    RegisterRevisionNumber;                ///< Bits 7:0
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_REGISTER_REVISION_NUMBER;

typedef union {
  struct {
    UINT8    Rank1Mapping :  1;                     ///< Bits 0:0
    UINT8    Reserved     :  7;                     ///< Bits 7:1
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_ADDRESS_MAPPING_FROM_REGISTER_TO_DRAM;

typedef union {
  struct {
    UINT8    Cke            :  2;                   ///< Bits 1:0
    UINT8    Odt            :  2;                   ///< Bits 3:2
    UINT8    CommandAddress :  2;                   ///< Bits 5:4
    UINT8    ChipSelect     :  2;                   ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_REGISTER_OUTPUT_DRIVE_STRENGTH_FOR_CONTROL_COMMAND_ADDRESS;

typedef union {
  struct {
    UINT8    Y0Y2                     :  2;         ///< Bits 1:0
    UINT8    Y1Y3                     :  2;         ///< Bits 3:2
    UINT8    Reserved0                :  2;         ///< Bits 5:4
    UINT8    RcdOutputSlewRateControl :  1;         ///< Bits 6:6
    UINT8    Reserved1                :  1;         ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_REGISTER_OUTPUT_DRIVE_STRENGTH_FOR_CLOCK;

typedef struct {
  UINT8    DataBufferRevisionNumber;
} SPD4_LRDIMM_DATA_BUFFER_REVISION_NUMBER;

typedef union {
  struct {
    UINT8    DramVrefDQForPackageRank0 :  6;        ///< Bits 5:0
    UINT8    Reserved                  :  2;        ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_DRAM_VREFDQ_FOR_PACKAGE_RANK;

typedef struct {
  UINT8    DataBufferVrefDQforDramInterface;
} SPD4_LRDIMM_DATA_BUFFER_VREFDQ_FOR_DRAM_INTERFACE;

typedef union {
  struct {
    UINT8    DramInterfaceMdqDriveStrength           :  4; ///< Bits 3:0
    UINT8    DramInterfaceMdqReadTerminationStrength :  4; ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_DATA_BUFFER_MDQ_DRIVE_STRENGTH_RTT_FOR_DATA_RATE;

typedef union {
  struct {
    UINT8    DataRateLe1866 :  2;                         ///< Bits 1:0
    UINT8    DataRateLe2400 :  2;                         ///< Bits 3:2
    UINT8    DataRateLe3200 :  2;                         ///< Bits 5:4
    UINT8    Reserved       :  2;                         ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_DRAM_DRIVE_STRENGTH;

typedef union {
  struct {
    UINT8    Rtt_Nom  :  3;                               ///< Bits 2:0
    UINT8    Rtt_WR   :  3;                               ///< Bits 5:3
    UINT8    Reserved :  2;                               ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_DRAM_ODT_RTT_WR_RTT_NOM_FOR_DATA_RATE;

typedef union {
  struct {
    UINT8    PackageRanks0_1 :  3;                        ///< Bits 2:0
    UINT8    PackageRanks2_3 :  3;                        ///< Bits 5:3
    UINT8    Reserved        :  2;                        ///< Bits 7:6
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_DRAM_ODT_RTT_PARK_FOR_DATA_RATE;

typedef union {
  struct {
    UINT8    Rank0      :  1;                             ///< Bits 0:0
    UINT8    Rank1      :  1;                             ///< Bits 1:1
    UINT8    Rank2      :  1;                             ///< Bits 2:2
    UINT8    Rank3      :  1;                             ///< Bits 3:3
    UINT8    DataBuffer :  1;                             ///< Bits 4:4
    UINT8    Reserved   :  3;                             ///< Bits 7:5
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_DATA_BUFFER_VREFDQ_FOR_DRAM_INTERFACE_RANGE;

typedef union {
  struct {
    UINT8    DataBufferGainAdjustment :  1;               ///< Bits 0:0
    UINT8    DataBufferDfe            :  1;               ///< Bits 1:1
    UINT8    Reserved                 :  6;               ///< Bits 7:2
  } Bits;
  UINT8    Data;
} SPD4_LRDIMM_DATA_BUFFER_DQ_DECISION_FEEDBACK_EQUALIZATION;

typedef UINT16 SPD4_NVDIMM_MODULE_PRODUCT_IDENTIFIER;

typedef union {
  struct {
    UINT16    ContinuationCount  :  7;               ///< Bits 6:0
    UINT16    ContinuationParity :  1;               ///< Bits 7:7
    UINT16    LastNonZeroByte    :  8;               ///< Bits 15:8
  } Bits;
  UINT16    Data;
  UINT8     Data8[2];
} SPD4_NVDIMM_SUBSYSTEM_CONTROLLER_MANUFACTURER_ID_CODE;

typedef UINT16 SPD4_NVDIMM_SUBSYSTEM_CONTROLLER_IDENTIFIER;

typedef UINT8 SPD4_NVDIMM_SUBSYSTEM_CONTROLLER_REVISION_CODE;

typedef union {
  struct {
    UINT8    Card      :  5;                         ///< Bits 4:0
    UINT8    Revision  :  2;                         ///< Bits 6:5
    UINT8    Extension :  1;                         ///< Bits 7:7
  } Bits;
  UINT8    Data;
} SPD4_NVDIMM_REFERENCE_RAW_CARD;

typedef union {
  struct {
    UINT8    Reserved  :  4;                         ///< Bits 3:0
    UINT8    Extension :  4;                         ///< Bits 7:4
  } Bits;
  UINT8    Data;
} SPD4_NVDIMM_MODULE_CHARACTERISTICS;

typedef struct {
  UINT8    Reserved;
  UINT8    MediaType;
} SPD4_NVDIMM_HYBRID_MODULE_MEDIA_TYPES;

typedef UINT8 SPD4_NVDIMM_MAXIMUM_NONVOLATILE_MEMORY_INITIALIZATION_TIME;

typedef union {
  struct {
    UINT16    FunctionInterface :  5;                ///< Bits 4:0
    UINT16    FunctionClass     :  5;                ///< Bits 9:5
    UINT16    BlockOffset       :  4;                ///< Bits 13:10
    UINT16    Reserved          :  1;                ///< Bits 14:14
    UINT16    Implemented       :  1;                ///< Bits 15:15
  } Bits;
  UINT16    Data;
  UINT8     Data8[2];
} SPD4_NVDIMM_FUNCTION_INTERFACE_DESCRIPTOR;

typedef struct {
  UINT8    Year;                               ///< Year represented in BCD (00h = 2000)
  UINT8    Week;                               ///< Year represented in BCD (47h = week 47)
} SPD4_MANUFACTURING_DATE;

typedef union {
  UINT32    Data;
  UINT16    SerialNumber16[2];
  UINT8     SerialNumber8[4];
} SPD4_MANUFACTURER_SERIAL_NUMBER;

typedef struct {
  UINT8    Location;                           ///< Module Manufacturing Location
} SPD4_MANUFACTURING_LOCATION;

typedef struct {
  SPD4_MANUFACTURER_ID_CODE          IdCode;                     ///< Module Manufacturer ID Code
  SPD4_MANUFACTURING_LOCATION        Location;                   ///< Module Manufacturing Location
  SPD4_MANUFACTURING_DATE            Date;                       ///< Module Manufacturing Year, in BCD (range: 2000-2255)
  SPD4_MANUFACTURER_SERIAL_NUMBER    SerialNumber;               ///< Module Serial Number
} SPD4_UNIQUE_MODULE_ID;

typedef union {
  UINT16    Crc[1];
  UINT8     Data8[2];
} SPD4_CYCLIC_REDUNDANCY_CODE;

typedef struct {
  SPD4_DEVICE_DESCRIPTION_STRUCT               Description;               ///< 0       Number of Serial PD Bytes Written / SPD Device Size / CRC Coverage 1, 2
  SPD4_REVISION_STRUCT                         Revision;                  ///< 1       SPD Revision
  SPD4_DRAM_DEVICE_TYPE_STRUCT                 DramDeviceType;            ///< 2       DRAM Device Type
  SPD4_MODULE_TYPE_STRUCT                      ModuleType;                ///< 3       Module Type
  SPD4_SDRAM_DENSITY_BANKS_STRUCT              SdramDensityAndBanks;      ///< 4       SDRAM Density and Banks
  SPD4_SDRAM_ADDRESSING_STRUCT                 SdramAddressing;           ///< 5       SDRAM Addressing
  SPD4_PRIMARY_SDRAM_PACKAGE_TYPE_STRUCT       PrimarySdramPackageType;   ///< 6       Primary SDRAM Package Type
  SPD4_SDRAM_OPTIONAL_FEATURES_STRUCT          SdramOptionalFeatures;     ///< 7       SDRAM Optional Features
  SPD4_SDRAM_THERMAL_REFRESH_STRUCT            ThermalAndRefreshOptions;  ///< 8       SDRAM Thermal and Refresh Options
  SPD4_OTHER_SDRAM_OPTIONAL_FEATURES_STRUCT    OtherOptionalFeatures;     ///< 9       Other SDRAM Optional Features
  SPD4_SECONDARY_SDRAM_PACKAGE_TYPE_STRUCT     SecondarySdramPackageType; ///< 10      Secondary SDRAM Package Type
  SPD4_MODULE_NOMINAL_VOLTAGE_STRUCT           ModuleNominalVoltage;      ///< 11      Module Nominal Voltage, VDD
  SPD4_MODULE_ORGANIZATION_STRUCT              ModuleOrganization;        ///< 12      Module Organization
  SPD4_MODULE_MEMORY_BUS_WIDTH_STRUCT          ModuleMemoryBusWidth;      ///< 13      Module Memory Bus Width
  SPD4_MODULE_THERMAL_SENSOR_STRUCT            ModuleThermalSensor;       ///< 14      Module Thermal Sensor
  SPD4_EXTENDED_MODULE_TYPE_STRUCT             ExtendedModuleType;        ///< 15      Extended Module Type
  UINT8                                        Reserved0;                 ///< 16      Reserved
  SPD4_TIMEBASE_STRUCT                         Timebase;                  ///< 17      Timebases
  SPD4_TCK_MIN_MTB_STRUCT                      tCKmin;                    ///< 18      SDRAM Minimum Cycle Time (tCKmin)
  SPD4_TCK_MAX_MTB_STRUCT                      tCKmax;                    ///< 19      SDRAM Maximum Cycle Time (tCKmax)
  SPD4_CAS_LATENCIES_SUPPORTED_STRUCT          CasLatencies;              ///< 20-23   CAS Latencies Supported
  SPD4_TAA_MIN_MTB_STRUCT                      tAAmin;                    ///< 24      Minimum CAS Latency Time (tAAmin)
  SPD4_TRCD_MIN_MTB_STRUCT                     tRCDmin;                   ///< 25      Minimum RAS# to CAS# Delay Time (tRCDmin)
  SPD4_TRP_MIN_MTB_STRUCT                      tRPmin;                    ///< 26      Minimum Row Precharge Delay Time (tRPmin)
  SPD4_TRAS_TRC_MIN_MTB_STRUCT                 tRASMintRCMinUpper;        ///< 27      Upper Nibbles for tRAS and tRC
  SPD4_TRAS_MIN_MTB_STRUCT                     tRASmin;                   ///< 28      Minimum Active to Precharge Delay Time (tRASmin), Least Significant Byte
  SPD4_TRC_MIN_MTB_STRUCT                      tRCmin;                    ///< 29      Minimum Active to Active/Refresh Delay Time (tRCmin), Least Significant Byte
  SPD4_TRFC_MIN_MTB_STRUCT                     tRFC1min;                  ///< 30-31   Minimum Refresh Recovery Delay Time (tRFC1min)
  SPD4_TRFC_MIN_MTB_STRUCT                     tRFC2min;                  ///< 32-33   Minimum Refresh Recovery Delay Time (tRFC2min)
  SPD4_TRFC_MIN_MTB_STRUCT                     tRFC4min;                  ///< 34-35   Minimum Refresh Recovery Delay Time (tRFC4min)
  SPD4_TFAW_MIN_MTB_UPPER_STRUCT               tFAWMinUpper;              ///< 36      Upper Nibble for tFAW
  SPD4_TFAW_MIN_MTB_STRUCT                     tFAWmin;                   ///< 37      Minimum Four Activate Window Delay Time (tFAWmin)
  SPD4_TRRD_MIN_MTB_STRUCT                     tRRD_Smin;                 ///< 38      Minimum Activate to Activate Delay Time (tRRD_Smin), different bank group
  SPD4_TRRD_MIN_MTB_STRUCT                     tRRD_Lmin;                 ///< 39      Minimum Activate to Activate Delay Time (tRRD_Lmin), same bank group
  SPD4_TCCD_MIN_MTB_STRUCT                     tCCD_Lmin;                 ///< 40      Minimum CAS to CAS Delay Time (tCCD_Lmin), Same Bank Group
  SPD4_TWR_UPPER_NIBBLE_STRUCT                 tWRUpperNibble;            ///< 41      Upper Nibble for tWRmin
  SPD4_TWR_MIN_MTB_STRUCT                      tWRmin;                    ///< 42      Minimum Write Recovery Time (tWRmin)
  SPD4_TWTR_UPPER_NIBBLE_STRUCT                tWTRUpperNibble;           ///< 43      Upper Nibbles for tWTRmin
  SPD4_TWTR_MIN_MTB_STRUCT                     tWTR_Smin;                 ///< 44      Minimum Write to Read Time (tWTR_Smin), Different Bank Group
  SPD4_TWTR_MIN_MTB_STRUCT                     tWTR_Lmin;                 ///< 45      Minimum Write to Read Time (tWTR_Lmin), Same Bank Group
  UINT8                                        Reserved1[59 - 46 + 1];    ///< 46-59   Reserved
  SPD4_CONNECTOR_BIT_MAPPING_BYTE_STRUCT       BitMapping[77 - 60 + 1];   ///< 60-77   Connector to SDRAM Bit Mapping
  UINT8                                        Reserved2[116 - 78 + 1];   ///< 78-116  Reserved
  SPD4_TCCD_MIN_FTB_STRUCT                     tCCD_LminFine;             ///< 117     Fine Offset for Minimum CAS to CAS Delay Time (tCCD_Lmin), same bank group
  SPD4_TRRD_MIN_FTB_STRUCT                     tRRD_LminFine;             ///< 118     Fine Offset for Minimum Activate to Activate Delay Time (tRRD_Lmin), different bank group
  SPD4_TRRD_MIN_FTB_STRUCT                     tRRD_SminFine;             ///< 119     Fine Offset for Minimum Activate to Activate Delay Time (tRRD_Smin), same bank group
  SPD4_TRC_MIN_FTB_STRUCT                      tRCminFine;                ///< 120     Fine Offset for Minimum Active to Active/Refresh Delay Time (tRCmin)
  SPD4_TRP_MIN_FTB_STRUCT                      tRPminFine;                ///< 121     Fine Offset for Minimum Row Precharge Delay Time (tRPabmin)
  SPD4_TRCD_MIN_FTB_STRUCT                     tRCDminFine;               ///< 122     Fine Offset for Minimum RAS# to CAS# Delay Time (tRCDmin)
  SPD4_TAA_MIN_FTB_STRUCT                      tAAminFine;                ///< 123     Fine Offset for Minimum CAS Latency Time (tAAmin)
  SPD4_TCK_MAX_FTB_STRUCT                      tCKmaxFine;                ///< 124     Fine Offset for SDRAM Minimum Cycle Time (tCKmax)
  SPD4_TCK_MIN_FTB_STRUCT                      tCKminFine;                ///< 125     Fine Offset for SDRAM Maximum Cycle Time (tCKmin)
  SPD4_CYCLIC_REDUNDANCY_CODE                  Crc;                       ///< 126-127 Cyclical Redundancy Code (CRC)
} SPD4_BASE_SECTION;

typedef struct {
  SPD4_UNBUF_MODULE_NOMINAL_HEIGHT       ModuleNominalHeight;     ///< 128     Module Nominal Height
  SPD4_UNBUF_MODULE_NOMINAL_THICKNESS    ModuleMaximumThickness;  ///< 129     Module Maximum Thickness
  SPD4_UNBUF_REFERENCE_RAW_CARD          ReferenceRawCardUsed;    ///< 130     Reference Raw Card Used
  SPD4_UNBUF_ADDRESS_MAPPING             AddressMappingEdgeConn;  ///< 131     Address Mapping from Edge Connector to DRAM
  UINT8                                  Reserved[253 - 132 + 1]; ///< 132-253 Reserved
  SPD4_CYCLIC_REDUNDANCY_CODE            Crc;                     ///< 254-255 Cyclical Redundancy Code (CRC)
} SPD4_MODULE_UNBUFFERED;

typedef struct {
  SPD4_RDIMM_MODULE_NOMINAL_HEIGHT                                         ModuleNominalHeight;                                 ///< 128     Module Nominal Height
  SPD4_RDIMM_MODULE_NOMINAL_THICKNESS                                      ModuleMaximumThickness;                              ///< 129     Module Maximum Thickness
  SPD4_RDIMM_REFERENCE_RAW_CARD                                            ReferenceRawCardUsed;                                ///< 130     Reference Raw Card Used
  SPD4_RDIMM_MODULE_ATTRIBUTES                                             DimmModuleAttributes;                                ///< 131     DIMM Module Attributes
  SPD4_RDIMM_THERMAL_HEAT_SPREADER_SOLUTION                                DimmThermalHeatSpreaderSolution;                     ///< 132     RDIMM Thermal Heat Spreader Solution
  SPD4_MANUFACTURER_ID_CODE                                                RegisterManufacturerIdCode;                          ///< 133-134 Register Manufacturer ID Code
  SPD4_RDIMM_REGISTER_REVISION_NUMBER                                      RegisterRevisionNumber;                              ///< 135     Register Revision Number
  SPD4_RDIMM_ADDRESS_MAPPING_FROM_REGISTER_TO_DRAM                         AddressMappingFromRegisterToDRAM;                    ///< 136     Address Mapping from Register to DRAM
  SPD4_RDIMM_REGISTER_OUTPUT_DRIVE_STRENGTH_FOR_CONTROL_COMMAND_ADDRESS    RegisterOutputDriveStrengthForControlCommandAddress; ///< 137 Register Output Drive Strength for Control and Command Address
  SPD4_RDIMM_REGISTER_OUTPUT_DRIVE_STRENGTH_FOR_CLOCK                      RegisterOutputDriveStrengthForClock;                 ///< 138     Register Output Drive Strength for Clock
  UINT8                                                                    Reserved[253 - 139 + 1];                             ///< 253-139 Reserved
  SPD4_CYCLIC_REDUNDANCY_CODE                                              Crc;                                                 ///< 254-255 Cyclical Redundancy Code (CRC)
} SPD4_MODULE_REGISTERED;

typedef struct {
  SPD4_LRDIMM_MODULE_NOMINAL_HEIGHT                                         ModuleNominalHeight;                                 ///< 128     Module Nominal Height
  SPD4_LRDIMM_MODULE_NOMINAL_THICKNESS                                      ModuleMaximumThickness;                              ///< 129     Module Maximum Thickness
  SPD4_LRDIMM_REFERENCE_RAW_CARD                                            ReferenceRawCardUsed;                                ///< 130     Reference Raw Card Used
  SPD4_LRDIMM_MODULE_ATTRIBUTES                                             DimmModuleAttributes;                                ///< 131     DIMM Module Attributes
  SPD4_LRDIMM_THERMAL_HEAT_SPREADER_SOLUTION                                ThermalHeatSpreaderSolution;                         ///< 132     RDIMM Thermal Heat Spreader Solution
  SPD4_MANUFACTURER_ID_CODE                                                 RegisterManufacturerIdCode;                          ///< 133-134 Register Manufacturer ID Code
  SPD4_LRDIMM_REGISTER_REVISION_NUMBER                                      RegisterRevisionNumber;                              ///< 135     Register Revision Number
  SPD4_LRDIMM_ADDRESS_MAPPING_FROM_REGISTER_TO_DRAM                         AddressMappingFromRegisterToDram;                    ///< 136 Address Mapping from Register to DRAM
  SPD4_LRDIMM_REGISTER_OUTPUT_DRIVE_STRENGTH_FOR_CONTROL_COMMAND_ADDRESS    RegisterOutputDriveStrengthForControlCommandAddress; ///< 137 Register Output Drive Strength for Control and Command Address
  SPD4_LRDIMM_REGISTER_OUTPUT_DRIVE_STRENGTH_FOR_CLOCK                      RegisterOutputDriveStrengthForClock;                 ///< 138 Register Output Drive Strength for Clock
  SPD4_LRDIMM_DATA_BUFFER_REVISION_NUMBER                                   DataBufferRevisionNumber;                            ///< 139     Data Buffer Revision Number
  SPD4_LRDIMM_DRAM_VREFDQ_FOR_PACKAGE_RANK                                  DramVrefDQForPackageRank0;                           ///< 140     DRAM VrefDQ for Package Rank 0
  SPD4_LRDIMM_DRAM_VREFDQ_FOR_PACKAGE_RANK                                  DramVrefDQForPackageRank1;                           ///< 141     DRAM VrefDQ for Package Rank 1
  SPD4_LRDIMM_DRAM_VREFDQ_FOR_PACKAGE_RANK                                  DramVrefDQForPackageRank2;                           ///< 142     DRAM VrefDQ for Package Rank 2
  SPD4_LRDIMM_DRAM_VREFDQ_FOR_PACKAGE_RANK                                  DramVrefDQForPackageRank3;                           ///< 143     DRAM VrefDQ for Package Rank 3
  SPD4_LRDIMM_DATA_BUFFER_VREFDQ_FOR_DRAM_INTERFACE                         DataBufferVrefDQForDramInterface;                    ///< 144     Data Buffer VrefDQ for DRAM Interface
  SPD4_LRDIMM_DATA_BUFFER_MDQ_DRIVE_STRENGTH_RTT_FOR_DATA_RATE              DataBufferMdqDriveStrengthRttForDataRateLe1866;      ///< 145     Data Buffer MDQ Drive Strength and RTT for data rate <= 1866
  SPD4_LRDIMM_DATA_BUFFER_MDQ_DRIVE_STRENGTH_RTT_FOR_DATA_RATE              DataBufferMdqDriveStrengthRttForDataRateLe2400;      ///< 146     Data Buffer MDQ Drive Strength and RTT for data rate <=2400
  SPD4_LRDIMM_DATA_BUFFER_MDQ_DRIVE_STRENGTH_RTT_FOR_DATA_RATE              DataBufferMdqDriveStrengthRttForDataRateLe3200;      ///< 147     Data Buffer MDQ Drive Strength and RTT for data rate <=3200
  SPD4_LRDIMM_DRAM_DRIVE_STRENGTH                                           DramDriveStrength;                                   ///< 148     DRAM Drive Strength
  SPD4_LRDIMM_DRAM_ODT_RTT_WR_RTT_NOM_FOR_DATA_RATE                         DramOdtRttWrRttNomForDataRateLe1866;                 ///< 149     DRAM ODT (RTT_WR and RTT_NOM) for data rate <= 1866
  SPD4_LRDIMM_DRAM_ODT_RTT_WR_RTT_NOM_FOR_DATA_RATE                         DramOdtRttWrRttNomForDataRateLe2400;                 ///< 150     DRAM ODT (RTT_WR and RTT_NOM) for data rate <= 2400
  SPD4_LRDIMM_DRAM_ODT_RTT_WR_RTT_NOM_FOR_DATA_RATE                         DramOdtRttWrRttNomForDataRateLe3200;                 ///< 151     DRAM ODT (RTT_WR and RTT_NOM) for data rate <= 3200
  SPD4_LRDIMM_DRAM_ODT_RTT_PARK_FOR_DATA_RATE                               DramOdtRttParkForDataRateLe1866;                     ///< 152     DRAM ODT (RTT_PARK) for data rate <= 1866
  SPD4_LRDIMM_DRAM_ODT_RTT_PARK_FOR_DATA_RATE                               DramOdtRttParkForDataRateLe2400;                     ///< 153     DRAM ODT (RTT_PARK) for data rate <= 2400
  SPD4_LRDIMM_DRAM_ODT_RTT_PARK_FOR_DATA_RATE                               DramOdtRttParkForDataRateLe3200;                     ///< 154     DRAM ODT (RTT_PARK) for data rate <= 3200
  SPD4_LRDIMM_DATA_BUFFER_VREFDQ_FOR_DRAM_INTERFACE_RANGE                   DataBufferVrefDQForDramInterfaceRange;               ///< 155     Data Buffer VrefDQ for DRAM Interface Range
  SPD4_LRDIMM_DATA_BUFFER_DQ_DECISION_FEEDBACK_EQUALIZATION                 DataBufferDqDecisionFeedbackEqualization;            ///< 156     Data Buffer DQ Decision Feedback Equalization
  UINT8                                                                     Reserved[253 - 157 + 1];                             ///< 253-132 Reserved
  SPD4_CYCLIC_REDUNDANCY_CODE                                               Crc;                                                 ///< 254-255 Cyclical Redundancy Code (CRC)
} SPD4_MODULE_LOADREDUCED;

typedef struct {
  UINT8                                                         Reserved0[191 - 128 + 1];                   ///< 128-191  Reserved
  SPD4_NVDIMM_MODULE_PRODUCT_IDENTIFIER                         ModuleProductIdentifier;                    ///< 192-193  Module Product Identifier
  SPD4_NVDIMM_SUBSYSTEM_CONTROLLER_MANUFACTURER_ID_CODE         SubsystemControllerManufacturerIdCode;      ///< 194-195  Subsystem Controller Manufacturer's ID Code
  SPD4_NVDIMM_SUBSYSTEM_CONTROLLER_IDENTIFIER                   SubsystemControllerIdentifier;              ///< 196-197  Subsystem Controller Identifier
  SPD4_NVDIMM_SUBSYSTEM_CONTROLLER_REVISION_CODE                SubsystemControllerRevisionCode;            ///< 198      Subsystem Controller Revision Code
  SPD4_NVDIMM_REFERENCE_RAW_CARD                                ReferenceRawCardUsed;                       ///< 199      Reference Raw Card Used
  SPD4_NVDIMM_MODULE_CHARACTERISTICS                            ModuleCharacteristics;                      ///< 200      Module Characteristics
  SPD4_NVDIMM_HYBRID_MODULE_MEDIA_TYPES                         HybridModuleMediaTypes;                     ///< 201-202  Hybrid Module Media Types
  SPD4_NVDIMM_MAXIMUM_NONVOLATILE_MEMORY_INITIALIZATION_TIME    MaximumNonVolatileMemoryInitializationTime; ///< 203 Maximum Non-Volatile Memory Initialization Time
  SPD4_NVDIMM_FUNCTION_INTERFACE_DESCRIPTOR                     FunctionInterfaceDescriptors[8];            ///< 204-219  Function Interface Descriptors
  UINT8                                                         Reserved[253 - 220 + 1];                    ///< 220-253  Reserved
  SPD4_CYCLIC_REDUNDANCY_CODE                                   Crc;                                        ///< 254-255  Cyclical Redundancy Code (CRC)
} SPD4_MODULE_NVDIMM;

typedef union {
  SPD4_MODULE_UNBUFFERED     Unbuffered;                        ///< 128-255 Unbuffered Memory Module Types
  SPD4_MODULE_REGISTERED     Registered;                        ///< 128-255 Registered Memory Module Types
  SPD4_MODULE_LOADREDUCED    LoadReduced;                       ///< 128-255 Load Reduced Memory Module Types
  SPD4_MODULE_NVDIMM         NonVolatile;                       ///< 128-255 Non-Volatile (NVDIMM-N) Hybrid Memory Parameters
} SPD4_MODULE_SPECIFIC;

typedef struct {
  UINT8    ModulePartNumber[348 - 329 + 1];                            ///< 329-348 Module Part Number
} SPD4_MODULE_PART_NUMBER;

typedef struct {
  UINT8    ManufacturerSpecificData[381 - 353 + 1];                            ///< 353-381 Manufacturer's Specific Data
} SPD4_MANUFACTURER_SPECIFIC;

typedef UINT8 SPD4_MODULE_REVISION_CODE;                        ///< 349     Module Revision Code
typedef UINT8 SPD4_DRAM_STEPPING;                               ///< 352     Dram Stepping

typedef struct {
  SPD4_UNIQUE_MODULE_ID         ModuleId;                       ///< 320-328 Unique Module ID
  SPD4_MODULE_PART_NUMBER       ModulePartNumber;               ///< 329-348 Module Part Number
  SPD4_MODULE_REVISION_CODE     ModuleRevisionCode;             ///< 349     Module Revision Code
  SPD4_MANUFACTURER_ID_CODE     DramIdCode;                     ///< 350-351 Dram Manufacturer ID Code
  SPD4_DRAM_STEPPING            DramStepping;                   ///< 352     Dram Stepping
  SPD4_MANUFACTURER_SPECIFIC    ManufacturerSpecificData;       ///< 353-381 Manufacturer's Specific Data
  UINT8                         Reserved[2];                    ///< 382-383 Reserved
} SPD4_MANUFACTURING_DATA;

typedef struct {
  UINT8    Reserved[511 - 384 + 1];                             ///< 384-511 Unbuffered Memory Module Types
} SPD4_END_USER_SECTION;

///
/// DDR4 Serial Presence Detect structure
///
typedef struct {
  SPD4_BASE_SECTION          Base;                              ///< 0-127   Base Configuration and DRAM Parameters
  SPD4_MODULE_SPECIFIC       Module;                            ///< 128-255 Module-Specific Section
  UINT8                      Reserved[319 - 256 + 1];           ///< 256-319 Reserved
  SPD4_MANUFACTURING_DATA    ManufactureInfo;                   ///< 320-383 Manufacturing Information
  SPD4_END_USER_SECTION      EndUser;                           ///< 384-511 End User Programmable
} SPD_DDR4;

#pragma pack (pop)
#endif
