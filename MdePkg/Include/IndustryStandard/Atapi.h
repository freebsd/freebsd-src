/** @file
  This file contains just some basic definitions that are needed by drivers
  that dealing with ATA/ATAPI interface.

Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _ATAPI_H_
#define _ATAPI_H_

#pragma pack(1)

///
/// ATA5_IDENTIFY_DATA is defined in ATA-5.
/// (This structure is provided mainly for backward-compatibility support.
/// Old drivers may reference fields that are marked "obsolete" in
/// ATA_IDENTIFY_DATA, which currently conforms to ATA-8.)
///
typedef struct {
  UINT16  config;             ///< General Configuration.
  UINT16  cylinders;          ///< Number of Cylinders.
  UINT16  reserved_2;
  UINT16  heads;              ///< Number of logical heads.
  UINT16  vendor_data1;
  UINT16  vendor_data2;
  UINT16  sectors_per_track;
  UINT16  vendor_specific_7_9[3];
  CHAR8   SerialNo[20];       ///< ASCII
  UINT16  vendor_specific_20_21[2];
  UINT16  ecc_bytes_available;
  CHAR8   FirmwareVer[8];     ///< ASCII
  CHAR8   ModelName[40];      ///< ASCII
  UINT16  multi_sector_cmd_max_sct_cnt;
  UINT16  reserved_48;
  UINT16  capabilities;
  UINT16  reserved_50;
  UINT16  pio_cycle_timing;
  UINT16  reserved_52;
  UINT16  field_validity;
  UINT16  current_cylinders;
  UINT16  current_heads;
  UINT16  current_sectors;
  UINT16  CurrentCapacityLsb;
  UINT16  CurrentCapacityMsb;
  UINT16  reserved_59;
  UINT16  user_addressable_sectors_lo;
  UINT16  user_addressable_sectors_hi;
  UINT16  reserved_62;
  UINT16  multi_word_dma_mode;
  UINT16  advanced_pio_modes;
  UINT16  min_multi_word_dma_cycle_time;
  UINT16  rec_multi_word_dma_cycle_time;
  UINT16  min_pio_cycle_time_without_flow_control;
  UINT16  min_pio_cycle_time_with_flow_control;
  UINT16  reserved_69_79[11];
  UINT16  major_version_no;
  UINT16  minor_version_no;
  UINT16  command_set_supported_82;    ///< word 82
  UINT16  command_set_supported_83;    ///< word 83
  UINT16  command_set_feature_extn;    ///< word 84
  UINT16  command_set_feature_enb_85;  ///< word 85
  UINT16  command_set_feature_enb_86;  ///< word 86
  UINT16  command_set_feature_default; ///< word 87
  UINT16  ultra_dma_mode;              ///< word 88
  UINT16  reserved_89_127[39];
  UINT16  security_status;
  UINT16  vendor_data_129_159[31];
  UINT16  reserved_160_255[96];
} ATA5_IDENTIFY_DATA;

///
/// ATA_IDENTIFY_DATA strictly complies with ATA/ATAPI-8 Spec
/// to define the data returned by an ATA device upon successful
/// completion of the ATA IDENTIFY_DEVICE command.
///
typedef struct {
  UINT16  config;                                  ///< General Configuration.
  UINT16  obsolete_1;
  UINT16  specific_config;                         ///< Specific Configuration.
  UINT16  obsolete_3;
  UINT16  retired_4_5[2];
  UINT16  obsolete_6;
  UINT16  cfa_reserved_7_8[2];
  UINT16  retired_9;
  CHAR8   SerialNo[20];                            ///< word 10~19
  UINT16  retired_20_21[2];
  UINT16  obsolete_22;
  CHAR8   FirmwareVer[8];                          ///< word 23~26
  CHAR8   ModelName[40];                           ///< word 27~46
  UINT16  multi_sector_cmd_max_sct_cnt;
  UINT16  trusted_computing_support;
  UINT16  capabilities_49;
  UINT16  capabilities_50;
  UINT16  obsolete_51_52[2];
  UINT16  field_validity;
  UINT16  obsolete_54_58[5];
  UINT16  multi_sector_setting;
  UINT16  user_addressable_sectors_lo;
  UINT16  user_addressable_sectors_hi;
  UINT16  obsolete_62;
  UINT16  multi_word_dma_mode;
  UINT16  advanced_pio_modes;
  UINT16  min_multi_word_dma_cycle_time;
  UINT16  rec_multi_word_dma_cycle_time;
  UINT16  min_pio_cycle_time_without_flow_control;
  UINT16  min_pio_cycle_time_with_flow_control;
  UINT16  additional_supported;                    ///< word 69
  UINT16  reserved_70;
  UINT16  reserved_71_74[4];                       ///< Reserved for IDENTIFY PACKET DEVICE cmd.
  UINT16  queue_depth;
  UINT16  serial_ata_capabilities;
  UINT16  reserved_77;                             ///< Reserved for Serial ATA
  UINT16  serial_ata_features_supported;
  UINT16  serial_ata_features_enabled;
  UINT16  major_version_no;
  UINT16  minor_version_no;
  UINT16  command_set_supported_82;                ///< word 82
  UINT16  command_set_supported_83;                ///< word 83
  UINT16  command_set_feature_extn;                ///< word 84
  UINT16  command_set_feature_enb_85;              ///< word 85
  UINT16  command_set_feature_enb_86;              ///< word 86
  UINT16  command_set_feature_default;             ///< word 87
  UINT16  ultra_dma_mode;                          ///< word 88
  UINT16  time_for_security_erase_unit;
  UINT16  time_for_enhanced_security_erase_unit;
  UINT16  advanced_power_management_level;
  UINT16  master_password_identifier;
  UINT16  hardware_configuration_test_result;
  UINT16  obsolete_94;
  UINT16  stream_minimum_request_size;
  UINT16  streaming_transfer_time_for_dma;
  UINT16  streaming_access_latency_for_dma_and_pio;
  UINT16  streaming_performance_granularity[2];    ///< word 98~99
  UINT16  maximum_lba_for_48bit_addressing[4];     ///< word 100~103
  UINT16  streaming_transfer_time_for_pio;
  UINT16  max_no_of_512byte_blocks_per_data_set_cmd;
  UINT16  phy_logic_sector_support;                ///< word 106
  UINT16  interseek_delay_for_iso7779;
  UINT16  world_wide_name[4];                      ///< word 108~111
  UINT16  reserved_for_128bit_wwn_112_115[4];
  UINT16  reserved_for_technical_report;
  UINT16  logic_sector_size_lo;                    ///< word 117
  UINT16  logic_sector_size_hi;                    ///< word 118
  UINT16  features_and_command_sets_supported_ext; ///< word 119
  UINT16  features_and_command_sets_enabled_ext;   ///< word 120
  UINT16  reserved_121_126[6];
  UINT16  obsolete_127;
  UINT16  security_status;                         ///< word 128
  UINT16  vendor_specific_129_159[31];
  UINT16  cfa_power_mode;                          ///< word 160
  UINT16  reserved_for_compactflash_161_167[7];
  UINT16  device_nominal_form_factor;
  UINT16  is_data_set_cmd_supported;
  CHAR8   additional_product_identifier[8];
  UINT16  reserved_174_175[2];
  CHAR8   media_serial_number[60];                 ///< word 176~205
  UINT16  sct_command_transport;                   ///< word 206
  UINT16  reserved_207_208[2];
  UINT16  alignment_logic_in_phy_blocks;           ///< word 209
  UINT16  write_read_verify_sector_count_mode3[2]; ///< word 210~211
  UINT16  verify_sector_count_mode2[2];
  UINT16  nv_cache_capabilities;
  UINT16  nv_cache_size_in_logical_block_lsw;      ///< word 215
  UINT16  nv_cache_size_in_logical_block_msw;      ///< word 216
  UINT16  nominal_media_rotation_rate;
  UINT16  reserved_218;
  UINT16  nv_cache_options;                        ///< word 219
  UINT16  write_read_verify_mode;                  ///< word 220
  UINT16  reserved_221;
  UINT16  transport_major_revision_number;
  UINT16  transport_minor_revision_number;
  UINT16  reserved_224_229[6];
  UINT64  extended_no_of_addressable_sectors;
  UINT16  min_number_per_download_microcode_mode3; ///< word 234
  UINT16  max_number_per_download_microcode_mode3; ///< word 235
  UINT16  reserved_236_254[19];
  UINT16  integrity_word;
} ATA_IDENTIFY_DATA;

///
/// ATAPI_IDENTIFY_DATA strictly complies with ATA/ATAPI-8 Spec
/// to define the data returned by an ATAPI device upon successful
/// completion of the ATA IDENTIFY_PACKET_DEVICE command.
///
typedef struct {
  UINT16  config;                                  ///< General Configuration.
  UINT16  reserved_1;
  UINT16  specific_config;                         ///< Specific Configuration.
  UINT16  reserved_3_9[7];
  CHAR8   SerialNo[20];                            ///< word 10~19
  UINT16  reserved_20_22[3];
  CHAR8   FirmwareVer[8];                          ///< word 23~26
  CHAR8   ModelName[40];                           ///< word 27~46
  UINT16  reserved_47_48[2];
  UINT16  capabilities_49;
  UINT16  capabilities_50;
  UINT16  obsolete_51;
  UINT16  reserved_52;
  UINT16  field_validity;                          ///< word 53
  UINT16  reserved_54_61[8];
  UINT16  dma_dir;
  UINT16  multi_word_dma_mode;                     ///< word 63
  UINT16  advanced_pio_modes;                      ///< word 64
  UINT16  min_multi_word_dma_cycle_time;
  UINT16  rec_multi_word_dma_cycle_time;
  UINT16  min_pio_cycle_time_without_flow_control;
  UINT16  min_pio_cycle_time_with_flow_control;
  UINT16  reserved_69_70[2];
  UINT16  obsolete_71_72[2];
  UINT16  reserved_73_74[2];
  UINT16  obsolete_75;
  UINT16  serial_ata_capabilities;
  UINT16  reserved_77;                             ///< Reserved for Serial ATA
  UINT16  serial_ata_features_supported;
  UINT16  serial_ata_features_enabled;
  UINT16  major_version_no;                        ///< word 80
  UINT16  minor_version_no;                        ///< word 81
  UINT16  cmd_set_support_82;
  UINT16  cmd_set_support_83;
  UINT16  cmd_feature_support;
  UINT16  cmd_feature_enable_85;
  UINT16  cmd_feature_enable_86;
  UINT16  cmd_feature_default;
  UINT16  ultra_dma_select;
  UINT16  time_required_for_sec_erase;             ///< word 89
  UINT16  time_required_for_enhanced_sec_erase;    ///< word 90
  UINT16  advanced_power_management_level;
  UINT16  master_pwd_revison_code;
  UINT16  hardware_reset_result;                   ///< word 93
  UINT16  obsolete_94;
  UINT16  reserved_95_107[13];
  UINT16  world_wide_name[4];                      ///< word 108~111
  UINT16  reserved_for_128bit_wwn_112_115[4];
  UINT16  reserved_116_118[3];
  UINT16  command_and_feature_sets_supported;      ///< word 119
  UINT16  command_and_feature_sets_supported_enabled;
  UINT16  reserved_121_124[4];
  UINT16  atapi_byte_count_0_behavior;             ///< word 125
  UINT16  obsolete_126_127[2];
  UINT16  security_status;
  UINT16  reserved_129_159[31];
  UINT16  cfa_reserved_160_175[16];
  UINT16  reserved_176_221[46];
  UINT16  transport_major_version;
  UINT16  transport_minor_version;
  UINT16  reserved_224_254[31];
  UINT16  integrity_word;
} ATAPI_IDENTIFY_DATA;


///
/// Standard Quiry Data format, defined in SFF-8070i(ATAPI Removable Rewritable Specification).
///
typedef struct {
  UINT8 peripheral_type;
  UINT8 RMB;
  UINT8 version;
  UINT8 response_data_format;
  UINT8 addnl_length;     ///< n - 4, Numbers of bytes following this one.
  UINT8 reserved_5;
  UINT8 reserved_6;
  UINT8 reserved_7;
  UINT8 vendor_info[8];
  UINT8 product_id[16];
  UINT8 product_revision_level[4];
  UINT8 vendor_specific_36_55[55 - 36 + 1];
  UINT8 reserved_56_95[95 - 56 + 1];
  ///
  /// Vendor-specific parameters fields. The sizeof (ATAPI_INQUIRY_DATA) is 254
  /// since allocation_length is one byte in ATAPI_INQUIRY_CMD.
  ///
  UINT8 vendor_specific_96_253[253 - 96 + 1];
} ATAPI_INQUIRY_DATA;

///
/// Request Sense Standard Data, defined in SFF-8070i(ATAPI Removable Rewritable Specification).
///
typedef struct {
  UINT8 error_code : 7;
  UINT8 valid : 1;
  UINT8 reserved_1;
  UINT8 sense_key : 4;
  UINT8 reserved_2 : 1;
  UINT8 Vendor_specifc_1 : 3;
  UINT8 vendor_specific_3;
  UINT8 vendor_specific_4;
  UINT8 vendor_specific_5;
  UINT8 vendor_specific_6;
  UINT8 addnl_sense_length;           ///< n - 7
  UINT8 vendor_specific_8;
  UINT8 vendor_specific_9;
  UINT8 vendor_specific_10;
  UINT8 vendor_specific_11;
  UINT8 addnl_sense_code;             ///< mandatory
  UINT8 addnl_sense_code_qualifier;   ///< mandatory
  UINT8 field_replaceable_unit_code;  ///< optional
  UINT8 sense_key_specific_15 : 7;
  UINT8 SKSV : 1;
  UINT8 sense_key_specific_16;
  UINT8 sense_key_specific_17;
} ATAPI_REQUEST_SENSE_DATA;

///
/// READ CAPACITY Data, defined in SFF-8070i(ATAPI Removable Rewritable Specification).
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
} ATAPI_READ_CAPACITY_DATA;

///
/// Capacity List Header + Current/Maximum Capacity Descriptor,
/// defined in SFF-8070i(ATAPI Removable Rewritable Specification).
///
typedef struct {
  UINT8 reserved_0;
  UINT8 reserved_1;
  UINT8 reserved_2;
  UINT8 Capacity_Length;
  UINT8 LastLba3;
  UINT8 LastLba2;
  UINT8 LastLba1;
  UINT8 LastLba0;
  UINT8 DesCode : 2;
  UINT8 reserved_9 : 6;
  UINT8 BlockSize2;
  UINT8 BlockSize1;
  UINT8 BlockSize0;
} ATAPI_READ_FORMAT_CAPACITY_DATA;

///
/// Test Unit Ready Command, defined in SFF-8070i(ATAPI Removable Rewritable Specification).
///
typedef struct {
  UINT8 opcode;
  UINT8 reserved_1;
  UINT8 reserved_2;
  UINT8 reserved_3;
  UINT8 reserved_4;
  UINT8 reserved_5;
  UINT8 reserved_6;
  UINT8 reserved_7;
  UINT8 reserved_8;
  UINT8 reserved_9;
  UINT8 reserved_10;
  UINT8 reserved_11;
} ATAPI_TEST_UNIT_READY_CMD;

///
/// INQUIRY Command, defined in SFF-8070i(ATAPI Removable Rewritable Specification).
///
typedef struct {
  UINT8 opcode;
  UINT8 reserved_1 : 5;
  UINT8 lun : 3;
  UINT8 page_code;        ///< defined in SFF8090i, V6
  UINT8 reserved_3;
  UINT8 allocation_length;
  UINT8 reserved_5;
  UINT8 reserved_6;
  UINT8 reserved_7;
  UINT8 reserved_8;
  UINT8 reserved_9;
  UINT8 reserved_10;
  UINT8 reserved_11;
} ATAPI_INQUIRY_CMD;

///
/// REQUEST SENSE Command, defined in SFF-8070i(ATAPI Removable Rewritable Specification).
///
typedef struct {
  UINT8 opcode;
  UINT8 reserved_1 : 5;
  UINT8 lun : 3;
  UINT8 reserved_2;
  UINT8 reserved_3;
  UINT8 allocation_length;
  UINT8 reserved_5;
  UINT8 reserved_6;
  UINT8 reserved_7;
  UINT8 reserved_8;
  UINT8 reserved_9;
  UINT8 reserved_10;
  UINT8 reserved_11;
} ATAPI_REQUEST_SENSE_CMD;

///
/// READ (10) Command, defined in SFF-8070i(ATAPI Removable Rewritable Specification).
///
typedef struct {
  UINT8 opcode;
  UINT8 reserved_1 : 5;
  UINT8 lun : 3;
  UINT8 Lba0;
  UINT8 Lba1;
  UINT8 Lba2;
  UINT8 Lba3;
  UINT8 reserved_6;
  UINT8 TranLen0;
  UINT8 TranLen1;
  UINT8 reserved_9;
  UINT8 reserved_10;
  UINT8 reserved_11;
} ATAPI_READ10_CMD;

///
/// READ Format Capacity Command, defined in SFF-8070i(ATAPI Removable Rewritable Specification).
///
typedef struct {
  UINT8 opcode;
  UINT8 reserved_1 : 5;
  UINT8 lun : 3;
  UINT8 reserved_2;
  UINT8 reserved_3;
  UINT8 reserved_4;
  UINT8 reserved_5;
  UINT8 reserved_6;
  UINT8 allocation_length_hi;
  UINT8 allocation_length_lo;
  UINT8 reserved_9;
  UINT8 reserved_10;
  UINT8 reserved_11;
} ATAPI_READ_FORMAT_CAP_CMD;

///
/// MODE SENSE Command, defined in SFF-8070i(ATAPI Removable Rewritable Specification).
///
typedef struct {
  UINT8 opcode;
  UINT8 reserved_1 : 5;
  UINT8 lun : 3;
  UINT8 page_code : 6;
  UINT8 page_control : 2;
  UINT8 reserved_3;
  UINT8 reserved_4;
  UINT8 reserved_5;
  UINT8 reserved_6;
  UINT8 parameter_list_length_hi;
  UINT8 parameter_list_length_lo;
  UINT8 reserved_9;
  UINT8 reserved_10;
  UINT8 reserved_11;
} ATAPI_MODE_SENSE_CMD;

///
/// ATAPI_PACKET_COMMAND is not defined in the ATA specification.
/// We add it here for the convenience of ATA/ATAPI module writers.
///
typedef union {
  UINT16                    Data16[6];
  ATAPI_TEST_UNIT_READY_CMD TestUnitReady;
  ATAPI_READ10_CMD          Read10;
  ATAPI_REQUEST_SENSE_CMD   RequestSence;
  ATAPI_INQUIRY_CMD         Inquiry;
  ATAPI_MODE_SENSE_CMD      ModeSense;
  ATAPI_READ_FORMAT_CAP_CMD ReadFormatCapacity;
} ATAPI_PACKET_COMMAND;

#pragma pack()


#define ATAPI_MAX_DMA_EXT_CMD_SECTORS                   0x10000
#define ATAPI_MAX_DMA_CMD_SECTORS                       0x100

//  ATA/ATAPI Signature equates
#define ATA_SIGNATURE                                   0x0101       ///< defined in ACS-3
#define ATAPI_SIGNATURE                                 0xeb14       ///< defined in ACS-3
#define ATAPI_SIGNATURE_32                              0xeb140101   ///< defined in ACS-3

//  Spin Up Configuration definitions
#define ATA_SPINUP_CFG_REQUIRED_IDD_INCOMPLETE          0x37c8   ///< defined in ACS-3
#define ATA_SPINUP_CFG_REQUIRED_IDD_COMPLETE            0x738c   ///< defined in ACS-3
#define ATA_SPINUP_CFG_NOT_REQUIRED_IDD_INCOMPLETE      0x8c73   ///< defined in ACS-3
#define ATA_SPINUP_CFG_NOT_REQUIRED_IDD_COMPLETE        0xc837   ///< defined in ACS-3

//
// ATA Packet Command Code
//
#define ATA_CMD_FORMAT_UNIT                             0x04   ///< defined in ATAPI Removable Rewritable Media Devices
#define ATA_CMD_SOFT_RESET                              0x08   ///< defined from ATA-3
#define ATA_CMD_PACKET                                  0xA0   ///< defined from ATA-3
#define ATA_CMD_IDENTIFY_DEVICE                         0xA1   ///< defined from ATA-3
#define ATA_CMD_SERVICE                                 0xA2   ///< defined from ATA-3
#define ATA_CMD_TEST_UNIT_READY                         0x00   ///< defined from ATA-1
#define ATA_CMD_REQUEST_SENSE                           0x03   ///< defined from ATA-4
#define ATA_CMD_INQUIRY                                 0x12   ///< defined in ATAPI Removable Rewritable Media Devices
#define ATA_CMD_READ_FORMAT_CAPACITY                    0x23   ///< defined in ATAPI Removable Rewritable Media Devices
#define ATA_CMD_READ_CAPACITY                           0x25   ///< defined in ATAPI Removable Rewritable Media Devices
#define ATA_CMD_READ_10                                 0x28   ///< defined in ATAPI Removable Rewritable Media Devices
#define ATA_CMD_WRITE_10                                0x2A   ///< defined in ATAPI Removable Rewritable Media Devices
#define ATA_CMD_ATAPI_SEEK                              0x2B   ///< defined in ATAPI Removable Rewritable Media Devices
#define ATA_CMD_WRITE_AND_VERIFY                        0x2E   ///< defined in ATAPI Removable Rewritable Media Devices
#define ATA_CMD_VERIFY                                  0x2F   ///< defined in ATAPI Removable Rewritable Media Devices
#define ATA_CMD_READ_12                                 0xA8   ///< defined in ATAPI Removable Rewritable Media Devices
#define ATA_CMD_WRITE_12                                0xAA   ///< defined in ATAPI Removable Rewritable Media Devices
#define ATA_CMD_START_STOP_UNIT                         0x1B   ///< defined in ATAPI Removable Rewritable Media Devices
#define ATA_CMD_PREVENT_ALLOW_MEDIA_REMOVAL             0x1E   ///< defined in ATAPI Removable Rewritable Media Devices
#define ATA_CMD_MODE_SELECT                             0x55   ///< defined in ATAPI Removable Rewritable Media Devices

#define ATA_CMD_MODE_SENSE                              0x5A   ///< defined in ATAPI Removable Rewritable Media Devices
    #define ATA_PAGE_CODE_READ_WRITE_ERROR                  0x01   ///< defined in ATAPI Removable Rewritable Media Devices
    #define ATA_PAGE_CODE_CACHING_PAGE                      0x08   ///< defined in ATAPI Removable Rewritable Media Devices
    #define ATA_PAGE_CODE_REMOVABLE_BLOCK_CAPABILITIES      0x1B   ///< defined in ATAPI Removable Rewritable Media Devices
    #define ATA_PAGE_CODE_TIMER_PROTECT_PAGE                0x1C   ///< defined in ATAPI Removable Rewritable Media Devices
    #define ATA_PAGE_CODE_RETURN_ALL_PAGES                  0x3F   ///< defined in ATAPI Removable Rewritable Media Devices

#define ATA_CMD_GET_CONFIGURATION                       0x46   ///< defined in ATAPI Multimedia Devices
   #define ATA_GCCD_RT_FIELD_VALUE_ALL                      0x00   ///< defined in ATAPI Multimedia Devices
   #define ATA_GCCD_RT_FIELD_VALUE_CURRENT                  0x01   ///< defined in ATAPI Multimedia Devices
   #define ATA_GCCD_RT_FIELD_VALUE_SINGLE                   0x02   ///< defined in ATAPI Multimedia Devices
   #define ATA_GCCD_RT_FIELD_VALUE_RESERVED                 0x03   ///< defined in ATAPI Multimedia Devices

   #define ATA_FEATURE_LIST_PROFILE_LIST                    0x0000   ///< defined in ATAPI Multimedia Devices
   #define ATA_FEATURE_LIST_CORE                            0x0001   ///< defined in ATAPI Multimedia Devices
   #define ATA_FEATURE_LIST_MORPHING                        0x0002   ///< defined in ATAPI Multimedia Devices
   #define ATA_FEATURE_LIST_REMOVEABLE_MEDIUM               0x0003   ///< defined in ATAPI Multimedia Devices
   #define ATA_FEATURE_LIST_WRITE_PROTECT                   0x0004   ///< defined in ATAPI Multimedia Devices

///
/// Start/Stop and Eject Operations
///
///@{
#define ATA_CMD_SUBOP_STOP_DISC                         0x00   ///< Stop the Disc
#define ATA_CMD_SUBOP_START_DISC                        0x01   ///< Start the Disc and acquire the format type
#define ATA_CMD_SUBOP_EJECT_DISC                        0x02   ///< Eject the Disc if possible
#define ATA_CMD_SUBOP_CLOSE_TRAY                        0x03   ///< Load the Disc (Close Tray)
///@}

//
// ATA Commands Code
//

//
// Class 1: PIO Data-In Commands
//
#define ATA_CMD_IDENTIFY_DRIVE                          0xec   ///< defined from ATA-3
#define ATA_CMD_READ_BUFFER                             0xe4   ///< defined from ATA-1
#define ATA_CMD_READ_SECTORS                            0x20   ///< defined from ATA-1
#define ATA_CMD_READ_SECTORS_WITH_RETRY                 0x21   ///< defined from ATA-1, obsoleted from ATA-5
#define ATA_CMD_READ_LONG                               0x22   ///< defined from ATA-1, obsoleted from ATA-5
#define ATA_CMD_READ_LONG_WITH_RETRY                    0x23   ///< defined from ATA-1, obsoleted from ATA-5
#define ATA_CMD_READ_SECTORS_EXT                        0x24   ///< defined from ATA-6
#define ATA_CMD_READ_MULTIPLE                           0xc4   ///< defined in ACS-3
#define ATA_CMD_READ_MULTIPLE_EXT                       0x29   ///< defined in ACS-3
#define ATA_CMD_READ_LOG_EXT                            0x2f   ///< defined in ACS-3

//
// Class 2: PIO Data-Out Commands
//
#define ATA_CMD_FORMAT_TRACK                            0x50  ///< defined from ATA-1, obsoleted from ATA-4
#define ATA_CMD_WRITE_BUFFER                            0xe8  ///< defined from ATA-1
#define ATA_CMD_WRITE_SECTORS                           0x30  ///< defined from ATA-1
#define ATA_CMD_WRITE_SECTORS_WITH_RETRY                0x31  ///< defined from ATA-1, obsoleted from ATA-5
#define ATA_CMD_WRITE_LONG                              0x32  ///< defined from ATA-1, obsoleted from ATA-5
#define ATA_CMD_WRITE_LONG_WITH_RETRY                   0x33  ///< defined from ATA-1, obsoleted from ATA-5
#define ATA_CMD_WRITE_VERIFY                            0x3c  ///< defined from ATA-1, obsoleted from ATA-5
#define ATA_CMD_WRITE_SECTORS_EXT                       0x34  ///< defined from ATA-6
#define ATA_CMD_WRITE_MULTIPLE                          0xc5  ///< defined in ACS-3
#define ATA_CMD_WRITE_MULTIPLE_EXT                      0x39  ///< defined in ACS-3

//
// Class 3 No Data Command
//
#define ATA_CMD_ACK_MEDIA_CHANGE                        0xdb  ///< defined from ATA-1, obsoleted from ATA-5
#define ATA_CMD_BOOT_POST_BOOT                          0xdc  ///< defined from ATA-1, obsoleted from ATA-3
#define ATA_CMD_BOOT_PRE_BOOT                           0xdd  ///< defined from ATA-1, obsoleted from ATA-3
#define ATA_CMD_CHECK_POWER_MODE                        0x98  ///< defined from ATA-1, obsoleted from ATA-4
#define ATA_CMD_CHECK_POWER_MODE_ALIAS                  0xe5  ///< defined from ATA-1
#define ATA_CMD_DOOR_LOCK                               0xde  ///< defined from ATA-1
#define ATA_CMD_DOOR_UNLOCK                             0xdf  ///< defined from ATA-1
#define ATA_CMD_EXEC_DRIVE_DIAG                         0x90  ///< defined from ATA-1
#define ATA_CMD_IDLE_ALIAS                              0x97  ///< defined from ATA-1, obsoleted from ATA-4
#define ATA_CMD_IDLE                                    0xe3  ///< defined from ATA-1
#define ATA_CMD_IDLE_IMMEDIATE                          0x95  ///< defined from ATA-1, obsoleted from ATA-4
#define ATA_CMD_IDLE_IMMEDIATE_ALIAS                    0xe1  ///< defined from ATA-1
#define ATA_CMD_INIT_DRIVE_PARAM                        0x91  ///< defined from ATA-1, obsoleted from ATA-6
#define ATA_CMD_RECALIBRATE                             0x10  ///< defined from ATA-1, obsoleted from ATA-4
#define ATA_CMD_READ_DRIVE_STATE                        0xe9  ///< defined from ATA-1, obsoleted from ATA-3
#define ATA_CMD_SET_MULTIPLE_MODE                       0xC6  ///< defined from ATA-2
#define ATA_CMD_READ_VERIFY                             0x40  ///< defined from ATA-1
#define ATA_CMD_READ_VERIFY_WITH_RETRY                  0x41  ///< defined from ATA-1, obsoleted from ATA-5
#define ATA_CMD_SEEK                                    0x70  ///< defined from ATA-1
#define ATA_CMD_SET_FEATURES                            0xef  ///< defined from ATA-1
#define ATA_CMD_STANDBY                                 0x96  ///< defined from ATA-1, obsoleted from ATA-4
#define ATA_CMD_STANDBY_ALIAS                           0xe2  ///< defined from ATA-1
#define ATA_CMD_STANDBY_IMMEDIATE                       0x94  ///< defined from ATA-1, obsoleted from ATA-4
#define ATA_CMD_STANDBY_IMMEDIATE_ALIAS                 0xe0  ///< defined from ATA-1
#define ATA_CMD_SLEEP                                   0xe6  ///< defined in ACS-3
#define ATA_CMD_READ_NATIVE_MAX_ADDRESS                 0xf8  ///< defined in ATA-6
#define ATA_CMD_READ_NATIVE_MAX_ADDRESS_EXT             0x27  ///< defined in ATA-6

//
// Set Features Sub Command
//
#define ATA_SUB_CMD_ENABLE_VOLATILE_WRITE_CACHE             0x02   ///< defined in ACS-3
#define ATA_SUB_CMD_SET_TRANSFER_MODE                       0x03   ///< defined in ACS-3
#define ATA_SUB_CMD_ENABLE_APM                              0x05   ///< defined in ACS-3
#define ATA_SUB_CMD_ENABLE_PUIS                             0x06   ///< defined in ACS-3
#define ATA_SUB_CMD_PUIS_SET_DEVICE_SPINUP                  0x07   ///< defined in ACS-3
#define ATA_SUB_CMD_ENABLE_WRITE_READ_VERIFY                0x0b   ///< defined in ACS-3
#define ATA_SUB_CMD_ENABLE_SATA_FEATURE                     0x10   ///< defined in ACS-3
#define ATA_SUB_CMD_DISABLE_MEDIA_STATUS_NOTIFICATION       0x31   ///< defined in ACS-3
#define ATA_SUB_CMD_ENABLE_FREE_FALL_CONTROL                0x41   ///< defined in ACS-3
#define ATA_SUB_CMD_ACOUSTIC_MANAGEMENT_ENABLE              0x42   ///< defined in ACS-3
#define ATA_SUB_CMD_SET_MAX_HOST_INTERFACE_SECTOR_TIMES     0x43   ///< defined in ACS-3
#define ATA_SUB_CMD_EXTENDED_POWER_CONDITIONS               0x4a   ///< defined in ACS-3
#define ATA_SUB_CMD_DISABLE_READ_LOOK_AHEAD                 0x55   ///< defined in ACS-3
#define ATA_SUB_CMD_EN_DIS_DSN_FEATURE                      0x63   ///< defined in ACS-3
#define ATA_SUB_CMD_DISABLE_REVERT_TO_POWER_ON_DEFAULTS     0x66   ///< defined in ACS-3
#define ATA_SUB_CMD_DISABLE_VOLATILE_WRITE_CACHE            0x82   ///< defined in ACS-3
#define ATA_SUB_CMD_DISABLE_APM                             0x85   ///< defined in ACS-3
#define ATA_SUB_CMD_DISABLE_PUIS                            0x86   ///< defined in ACS-3
#define ATA_SUB_CMD_DISABLE_WRITE_READ_VERIFY               0x8b   ///< defined in ACS-3
#define ATA_SUB_CMD_DISABLE_SATA_FEATURE                    0x90   ///< defined in ACS-3
#define ATA_SUB_CMD_ENABLE_MEDIA_STATUS_NOTIFICATION        0x95   ///< defined in ACS-3
#define ATA_SUB_CMD_ENABLE_READ_LOOK_AHEAD                  0xaa   ///< defined in ACS-3
#define ATA_SUB_CMD_DISABLE_FREE_FALL_CONTROL               0xc1   ///< defined in ACS-3
#define ATA_SUB_CMD_ACOUSTIC_MANAGEMENT_DISABLE             0xc2   ///< defined in ACS-3
#define ATA_SUB_CMD_EN_DIS_SENSE_DATA_REPORTING             0xc3   ///< defined in ACS-3
#define ATA_SUB_CMD_ENABLE_REVERT_TO_POWER_ON_DEFAULTS      0xcc   ///< defined in ACS-3

//
// S.M.A.R.T
//
#define ATA_CMD_SMART                                   0xb0  ///< defined from ATA-3
#define ATA_CONSTANT_C2                                 0xc2  ///< reserved
#define ATA_CONSTANT_4F                                 0x4f  ///< reserved

#define ATA_SMART_READ_DATA                             0xd0   ///< defined in ACS-3

#define ATA_SMART_AUTOSAVE                              0xd2  ///< defined in ACS-3
    #define ATA_AUTOSAVE_DISABLE_ATTR                       0x00
    #define ATA_AUTOSAVE_ENABLE_ATTR                        0xf1

#define ATA_SMART_EXECUTE_OFFLINE_IMMEDIATE             0xd4  ///< defined in ACS-3
    #define ATA_EXECUTE_SMART_OFFLINE_ROUTINE               0x00  ///< defined in ACS-3
    #define ATA_EXECUTE_SMART_OFFLINE_SHORT_SELFTEST        0x01  ///< defined in ACS-3
    #define ATA_EXECUTE_SMART_OFFLINE_EXTENDED_SELFTEST     0x02  ///< defined in ACS-3
    #define ATA_EXECUTE_SMART_OFFLINE_CONVEYANCE_SELFTEST   0x03  ///< defined in ACS-3
    #define ATA_EXECUTE_SMART_OFFLINE_SELECTIVE_SELFTEST    0x04  ///< defined in ACS-3
    #define ATA_SMART_ABORT_SELF_TEST_SUBROUTINE            0x7f  ///< defined in ACS-3
    #define ATA_EXECUTE_SMART_CAPTIVE_SHORT_SELFTEST        0x81  ///< defined in ACS-3
    #define ATA_EXECUTE_SMART_CAPTIVE_EXTENDED_SELFTEST     0x82  ///< defined in ACS-3
    #define ATA_EXECUTE_SMART_CAPTIVE_CONVEYANCE_SELFTEST   0x83  ///< defined in ACS-3
    #define ATA_EXECUTE_SMART_CAPTIVE_SELECTIVE_SELFTEST    0x84  ///< defined in ACS-3

#define ATA_SMART_READLOG                               0xd5  ///< defined in ACS-3
#define ATA_SMART_WRITELOG                              0xd6  ///< defined in ACS-3
#define ATA_SMART_ENABLE_OPERATION                      0xd8  ///< reserved
#define ATA_SMART_DISABLE_OPERATION                     0xd9  ///< defined in ACS-3
#define ATA_SMART_RETURN_STATUS                         0xda  ///< defined from ATA-3

#define ATA_SMART_THRESHOLD_NOT_EXCEEDED_VALUE              0xc24f  ///< defined in ACS-3
#define ATA_SMART_THRESHOLD_EXCEEDED_VALUE                  0x2cf4  ///< defined in ACS-3

// SMART Log Definitions
#define ATA_SMART_LOG_DIRECTORY                             0x00  ///< defined in ACS-3
#define ATA_SMART_SUM_SMART_ERROR_LOG                       0x01  ///< defined in ACS-3
#define ATA_SMART_COMP_SMART_ERROR_LOG                      0x02  ///< defined in ACS-3
#define ATA_SMART_EXT_COMP_SMART_ERROR_LOG                  0x03  ///< defined in ACS-3
#define ATA_SMART_SMART_SELFTEST_LOG                        0x06  ///< defined in ACS-3
#define ATA_SMART_EXT_SMART_SELFTEST_LOG                    0x07  ///< defined in ACS-3
#define ATA_SMART_SELECTIVE_SELFTEST_LOG                    0x09  ///< defined in ACS-3
#define ATA_SMART_HOST_VENDOR_SPECIFIC                      0x80  ///< defined in ACS-3
#define ATA_SMART_DEVICE_VENDOR_SPECIFIC                    0xa0  ///< defined in ACS-3

//
// Class 4: DMA Command
//
#define ATA_CMD_READ_DMA                                0xc8   ///< defined from ATA-1
#define ATA_CMD_READ_DMA_WITH_RETRY                     0xc9   ///< defined from ATA-1, obsoleted from ATA-5
#define ATA_CMD_READ_DMA_EXT                            0x25   ///< defined from ATA-6
#define ATA_CMD_WRITE_DMA                               0xca   ///< defined from ATA-1
#define ATA_CMD_WRITE_DMA_WITH_RETRY                    0xcb   ///< defined from ATA-1, obsoleted from ATA-
#define ATA_CMD_WRITE_DMA_EXT                           0x35   ///< defined from ATA-6

//
//  ATA Security commands
//
#define ATA_CMD_SECURITY_SET_PASSWORD                   0xf1  ///< defined in ACS-3
#define ATA_CMD_SECURITY_UNLOCK                         0xf2  ///< defined in ACS-3
#define ATA_CMD_SECURITY_ERASE_PREPARE                  0xf3  ///< defined in ACS-3
#define ATA_CMD_SECURITY_ERASE_UNIT                     0xf4  ///< defined in ACS-3
#define ATA_CMD_SECURITY_FREEZE_LOCK                    0xf5  ///< defined in ACS-3
#define ATA_CMD_SECURITY_DISABLE_PASSWORD               0xf6  ///< defined in ACS-3

#define ATA_SECURITY_BUFFER_LENGTH                          512  ///< defined in ACS-3

//
//  ATA Device Config Overlay
//
#define ATA_CMD_DEV_CONFIG_OVERLAY                      0xb1   ///< defined from ATA-6
    #define ATA_CMD_DEV_CONFIG_RESTORE_FEATURE              0xc0   ///< defined from ATA-6
    #define ATA_CMD_DEV_CONFIG_FREEZELOCK_FEATURE           0xc1   ///< defined from ATA-6
    #define ATA_CMD_DEV_CONFIG_IDENTIFY_FEATURE             0xc2   ///< defined from ATA-6
    #define ATA_CMD_DEV_CONFIG_SET_FEATURE                  0xc3   ///< defined from ATA-6

//
//  ATA Trusted Computing Feature Set Commands
//
#define ATA_CMD_TRUSTED_NON_DATA                        0x5b  ///< defined in ACS-3
#define ATA_CMD_TRUSTED_RECEIVE                         0x5c  ///< defined in ACS-3
#define ATA_CMD_TRUSTED_RECEIVE_DMA                     0x5d  ///< defined in ACS-3
#define ATA_CMD_TRUSTED_SEND                            0x5e  ///< defined in ACS-3
#define ATA_CMD_TRUSTED_SEND_DMA                        0x5f  ///< defined in ACS-3

//
//  ATA Trusted Receive Fields
//
#define ATA_TR_RETURN_SECURITY_PROTOCOL_INFORMATION     0x00  ///< defined in ACS-3
#define ATA_TR_SECURITY_PROTOCOL_JEDEC_RESERVED         0xec  ///< defined in ACS-3
#define ATA_TR_SECURITY_PROTOCOL_SDCARD_RESERVED        0xed  ///< defined in ACS-3
#define ATA_TR_SECURITY_PROTOCOL_IEEE1667_RESERVED      0xee  ///< defined in ACS-3

//
//  Equates used for Acoustic Flags
//
#define ATA_ACOUSTIC_LEVEL_BYPASS                       0xff   ///< defined from ATA-6
#define ATA_ACOUSTIC_LEVEL_MAXIMUM_PERFORMANCE          0xfe   ///< defined from ATA-6
#define ATA_ACOUSTIC_LEVEL_QUIET                        0x80   ///< defined from ATA-6

//
//  Equates used for DiPM Support
//
#define ATA_CMD_DIPM_SUB                                0x03   // defined in ACS-3 : Count value in SetFeature identification : 03h  Device-initiated interface power state transitions
    #define ATA_DIPM_ENABLE                                 0x10   // defined in ACS-3
    #define ATA_DIPM_DISABLE                                0x90   // defined in ACS-3

//
//  Equates used for DevSleep Support
//
#define ATA_CMD_DEVSLEEP_SUB                            0x09   // defined in SATA 3.2 Gold Spec :  Count value in SetFeature identification : 09h  Device Sleep
    #define ATA_DEVSLEEP_ENABLE                             0x10   // defined in SATA 3.2 Gold Spec
    #define ATA_DEVSLEEP_DISABLE                            0x90   // defined in SATA 3.2 Gold Spec

#define ATA_DEVSLP_EXIT_TIMEOUT                             20 // MDAT - 20 ms
#define ATA_DEVSLP_MINIMUM_DETECTION_TIME                   10 // DMDT - 10 us
#define ATA_DEVSLP_MINIMUM_ASSERTION_TIME                   10 // DETO - 10 ms

//
//  Set MAX Commands
//
#define ATA_CMD_SET_MAX_ADDRESS_EXT                     0x37   ///< defined from ATA-6
#define ATA_CMD_SET_MAX_ADDRESS                         0xf9   ///< defined from ATA-6
    #define ATA_SET_MAX_SET_PASSWORD                        0x01   ///< defined from ATA-6
    #define ATA_SET_MAX_LOCK                                0x02   ///< defined from ATA-6
    #define ATA_SET_MAX_UNLOCK                              0x03   ///< defined from ATA-6
    #define ATA_SET_MAX_FREEZE_LOCK                         0x04   ///< defined from ATA-6

///
/// Default content of device control register, disable INT,
/// Bit3 is set to 1 according ATA-1
///
#define ATA_DEFAULT_CTL                                 (0x0a)
///
/// Default context of Device/Head Register,
/// Bit7 and Bit5 are set to 1 for back-compatibilities.
///
#define ATA_DEFAULT_CMD                                 (0xa0)

#define ATAPI_MAX_BYTE_COUNT                            (0xfffe)

#define ATA_REQUEST_SENSE_ERROR                         (0x70) ///< defined in SFF-8070i

//
// Sense Key, Additional Sense Codes and Additional Sense Code Qualifier
// defined in MultiMedia Commands (MMC, MMC-2)
//
// Sense Key
//
#define ATA_SK_NO_SENSE                                 (0x0)
#define ATA_SK_RECOVERY_ERROR                           (0x1)
#define ATA_SK_NOT_READY                                (0x2)
#define ATA_SK_MEDIUM_ERROR                             (0x3)
#define ATA_SK_HARDWARE_ERROR                           (0x4)
#define ATA_SK_ILLEGAL_REQUEST                          (0x5)
#define ATA_SK_UNIT_ATTENTION                           (0x6)
#define ATA_SK_DATA_PROTECT                             (0x7)
#define ATA_SK_BLANK_CHECK                              (0x8)
#define ATA_SK_VENDOR_SPECIFIC                          (0x9)
#define ATA_SK_RESERVED_A                               (0xA)
#define ATA_SK_ABORT                                    (0xB)
#define ATA_SK_RESERVED_C                               (0xC)
#define ATA_SK_OVERFLOW                                 (0xD)
#define ATA_SK_MISCOMPARE                               (0xE)
#define ATA_SK_RESERVED_F                               (0xF)

//
// Additional Sense Codes
//
#define ATA_ASC_NOT_READY                               (0x04)
#define ATA_ASC_MEDIA_ERR1                              (0x10)
#define ATA_ASC_MEDIA_ERR2                              (0x11)
#define ATA_ASC_MEDIA_ERR3                              (0x14)
#define ATA_ASC_MEDIA_ERR4                              (0x30)
#define ATA_ASC_MEDIA_UPSIDE_DOWN                       (0x06)
#define ATA_ASC_INVALID_CMD                             (0x20)
#define ATA_ASC_LBA_OUT_OF_RANGE                        (0x21)
#define ATA_ASC_INVALID_FIELD                           (0x24)
#define ATA_ASC_WRITE_PROTECTED                         (0x27)
#define ATA_ASC_MEDIA_CHANGE                            (0x28)
#define ATA_ASC_RESET                                   (0x29)  ///< Power On Reset or Bus Reset occurred.
#define ATA_ASC_ILLEGAL_FIELD                           (0x26)
#define ATA_ASC_NO_MEDIA                                (0x3A)
#define ATA_ASC_ILLEGAL_MODE_FOR_THIS_TRACK             (0x64)

//
// Additional Sense Code Qualifier
//
#define ATA_ASCQ_IN_PROGRESS                            (0x01)

//
// Error Register
//
#define ATA_ERRREG_BBK                                  BIT7  ///< Bad block detected      defined from ATA-1, obsoleted from ATA-2
#define ATA_ERRREG_UNC                                  BIT6  ///< Uncorrectable Data      defined from ATA-1, obsoleted from ATA-4
#define ATA_ERRREG_MC                                   BIT5  ///< Media Change            defined from ATA-1, obsoleted from ATA-4
#define ATA_ERRREG_IDNF                                 BIT4  ///< ID Not Found            defined from ATA-1, obsoleted from ATA-4
#define ATA_ERRREG_MCR                                  BIT3  ///< Media Change Requested  defined from ATA-1, obsoleted from ATA-4
#define ATA_ERRREG_ABRT                                 BIT2  ///< Aborted Command         defined from ATA-1
#define ATA_ERRREG_TK0NF                                BIT1  ///< Track 0 Not Found       defined from ATA-1, obsoleted from ATA-4
#define ATA_ERRREG_AMNF                                 BIT0  ///< Address Mark Not Found  defined from ATA-1, obsoleted from ATA-4

//
// Status Register
//
#define ATA_STSREG_BSY                                  BIT7  ///< Controller Busy         defined from ATA-1
#define ATA_STSREG_DRDY                                 BIT6  ///< Drive Ready             defined from ATA-1
#define ATA_STSREG_DWF                                  BIT5  ///< Drive Write Fault       defined from ATA-1, obsoleted from ATA-4
#define ATA_STSREG_DF                                   BIT5  ///< Drive Fault             defined from ATA-6
#define ATA_STSREG_DSC                                  BIT4  ///< Disk Seek Complete      defined from ATA-1, obsoleted from ATA-4
#define ATA_STSREG_DRQ                                  BIT3  ///< Data Request            defined from ATA-1
#define ATA_STSREG_CORR                                 BIT2  ///< Corrected Data          defined from ATA-1, obsoleted from ATA-4
#define ATA_STSREG_IDX                                  BIT1  ///< Index                   defined from ATA-1, obsoleted from ATA-4
#define ATA_STSREG_ERR                                  BIT0  ///< Error                   defined from ATA-1

//
// Device Control Register
//
#define ATA_CTLREG_SRST                                 BIT2  ///< Software Reset.
#define ATA_CTLREG_IEN_L                                BIT1  ///< Interrupt Enable #.

#endif
