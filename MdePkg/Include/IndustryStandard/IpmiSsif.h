/** @file
  IPMI SSIF Definitions

  Copyright (c) 2023, Ampere Computing LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    - IPMI Specification
      Version 2.0, Rev. 1.1

  https://www.intel.com/content/www/us/en/products/docs/servers/ipmi/ipmi-second-gen-interface-spec-v2-rev1-1.html
**/

#ifndef IPMI_SSIF_H_
#define IPMI_SSIF_H_

///
/// Definitions for SMBUS Commands for SSIF
/// Table 12 - Summary of SMBUS Commands for SSIF
///

/// Write block
#define IPMI_SSIF_SMBUS_CMD_SINGLE_PART_WRITE        0x02
#define IPMI_SSIF_SMBUS_CMD_MULTI_PART_WRITE_START   0x06
#define IPMI_SSIF_SMBUS_CMD_MULTI_PART_WRITE_MIDDLE  0x07
#define IPMI_SSIF_SMBUS_CMD_MULTI_PART_WRITE_END     0x08

/// Read block
#define IPMI_SSIF_SMBUS_CMD_SINGLE_PART_READ        0x03
#define IPMI_SSIF_SMBUS_CMD_MULTI_PART_READ_START   0x03
#define IPMI_SSIF_SMBUS_CMD_MULTI_PART_READ_MIDDLE  0x09
#define IPMI_SSIF_SMBUS_CMD_MULTI_PART_READ_END     0x09
#define IPMI_SSIF_SMBUS_CMD_MULTI_PART_READ_RETRY   0x0A

///
/// Definitions for Multi-Part Read Transactions
/// Section 12.5
///
#define IPMI_SSIF_MULTI_PART_READ_START_SIZE      0x1E
#define IPMI_SSIF_MULTI_PART_READ_START_PATTERN1  0x00
#define IPMI_SSIF_MULTI_PART_READ_START_PATTERN2  0x01
#define IPMI_SSIF_MULTI_PART_READ_END_PATTERN     0xFF

///
/// IPMI SSIF maximum message size
///
#define IPMI_SSIF_INPUT_MESSAGE_SIZE_MAX   0xFF
#define IPMI_SSIF_OUTPUT_MESSAGE_SIZE_MAX  0xFF

///
/// IPMI SMBus system interface maximum packet size in byte
///
#define IPMI_SSIF_MAXIMUM_PACKET_SIZE_IN_BYTES  0x20

typedef enum {
  IpmiSsifPacketStart = 0,
  IpmiSsifPacketMiddle,
  IpmiSsifPacketEnd,
  IpmiSsifPacketSingle,
  IpmiSsifPacketMax
} IPMI_SSIF_PACKET_ATTRIBUTE;

#pragma pack (1)
///
/// IPMI SSIF Interface Request Format
/// Section 12.2 and 12.3
///
typedef struct {
  UINT8    NetFunc;
  UINT8    Command;
} IPMI_SSIF_REQUEST_HEADER;

///
/// IPMI SSIF Interface Response Format
/// Section 12.4 and 12.5
///
typedef struct {
  UINT8    StartPattern[2];
  UINT8    NetFunc;
  UINT8    Command;
} IPMI_SSIF_RESPONSE_PACKET_START;

typedef struct {
  UINT8    BlockNumber;
} IPMI_SSIF_RESPONSE_PACKET_MIDDLE;

typedef struct {
  UINT8    EndPattern;
} IPMI_SSIF_RESPONSE_PACKET_END;

typedef struct {
  UINT8    NetFunc;
  UINT8    Command;
} IPMI_SSIF_RESPONSE_SINGLE_PACKET;

#pragma pack ()

#endif /* IPMI_SSIF_H_ */
