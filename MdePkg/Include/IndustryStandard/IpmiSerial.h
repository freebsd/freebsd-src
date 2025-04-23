/** @file
  IPMI Serial Definitions

  Copyright (c) 2024, ARM Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    - IPMI Specification
      Version 2.0, Rev. 1.1

  https://www.intel.com/content/www/us/en/products/docs/servers/ipmi/ipmi-second-gen-interface-spec-v2-rev1-1.html
**/

#ifndef IPMI_SERIAL_H_
#define IPMI_SERIAL_H_

///
/// IPMI Serial Escaped Character Definition
/// Section 14.4.1 & 14.4.2
///
#define BASIC_MODE_START                   0xA0
#define BASIC_MODE_STOP                    0xA5
#define BASIC_MODE_HANDSHAKE               0xA6
#define BASIC_MODE_ESCAPE                  0xAA
#define BASIC_MODE_ESC_CHAR                0x1B
#define BASIC_MODE_START_ENCODED_BYTE      0xB0
#define BASIC_MODE_STOP_ENCODED_BYTE       0xB5
#define BASIC_MODE_HANDSHAKE_ENCODED_BYTE  0xB6
#define BASIC_MODE_ESCAPE_ENCODED_BYTE     0xBA
#define BASIC_MODE_ESC_CHAR_ENCODED_BYTE   0x3B

///
/// IPMI Serial State Machine
///
#define MSG_IDLE         0
#define MSG_IN_PROGRESS  1

///
/// IPMI Serial Message Field Definition
/// Section 14.4.3
///
#define IPMI_MAX_LUN                              0x3
#define IPMI_MAX_NETFUNCTION                      0x3F
#define IPMI_SERIAL_CONNECTION_HEADER_LENGTH      3
#define IPMI_SERIAL_REQUEST_DATA_HEADER_LENGTH    3
#define IPMI_SERIAL_MAXIMUM_PACKET_SIZE_IN_BYTES  256
#define IPMI_SERIAL_MIN_REQUEST_LENGTH            7

#pragma pack (1)
///
/// IPMI Serial Message Field
/// Section 14.4.3
///
typedef struct {
  UINT8    ResponderAddress;
  UINT8    ResponderNetFnLun;
  UINT8    CheckSum;
  UINT8    RequesterAddress;
  UINT8    RequesterSeqLun;
  UINT8    Command;
  UINT8    Data[];
} IPMI_SERIAL_HEADER;

#pragma pack ()

#endif /* IPMI_SERIAL_H_ */
