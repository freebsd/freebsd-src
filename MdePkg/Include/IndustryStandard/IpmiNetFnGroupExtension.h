/** @file
  IPMI 2.0 definitions from the IPMI Specification Version 2.0, Revision 1.1.

  Copyright (c) 1999 - 2015, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2024, Ampere Computing LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    - Arm Server Base Manageability Requirements (SBMR) Specification
      Revision 2.0d, Section F
      https://developer.arm.com/documentation/den0069

**/

#ifndef _IPMI_NET_FN_GROUP_EXTENSION_H_
#define _IPMI_NET_FN_GROUP_EXTENSION_H_

#include <Pi/PiStatusCode.h>

//
// Net function definition for Group Extension command
//
#define IPMI_NETFN_GROUP_EXT  0x2C

//
// All Group Extension commands and their structure definitions to follow here
//

///
/// Constants and structure definitions for Boot Progress Codes
///
/// See Section F of the Arm Server Base Manageability Requirements 2.0 specification,
/// https://developer.arm.com/documentation/den0069
///

#pragma pack(1)
//
// Definitions for send progress code command
//
#define IPMI_GROUP_EXTENSION_BOOT_PROGRESS_CODE_SEND  0x02

//
// Definitions for get progress code command
//
#define IPMI_GROUP_EXTENSION_BOOT_PROGRESS_CODE_GET  0x03

//
// Definitions for send and get progress code command response
//
#define IPMI_GROUP_EXTENSION_BOOT_PROGRESS_CODE_COMPLETED_NORMALLY  0x00
#define IPMI_GROUP_EXTENSION_BOOT_PROGRESS_CODE_COMPLETED_ERROR     0x80
#define IPMI_GROUP_EXTENSION_BOOT_PROGRESS_CODE_DEFINING_BODY       0xAE

//
// Structure for the format of the boot progress code data
// See Table 29: SBMR Boot Progress Codes format
//
typedef struct {
  EFI_STATUS_CODE_TYPE     CodeType;
  EFI_STATUS_CODE_VALUE    CodeValue;
  UINT8                    Instance;
} IPMI_GROUP_EXTENSION_BOOT_PROGRESS_CODE_FORMAT;

//
// Structure for the boot progress code send request
//
typedef struct {
  UINT8                                             DefiningBody;
  IPMI_GROUP_EXTENSION_BOOT_PROGRESS_CODE_FORMAT    BootProgressCode;
} IPMI_GROUP_EXTENSION_BOOT_PROGRESS_CODE_SEND_REQUEST;

//
// Structure for the boot progress code send response
//
typedef struct {
  UINT8    CompletionCode;
  UINT8    DefiningBody;
} IPMI_GROUP_EXTENSION_BOOT_PROGRESS_CODE_SEND_RESPONSE;

//
// Structure for the boot progress code get request
//
typedef struct {
  UINT8    DefiningBody;
} IPMI_GROUP_EXTENSION_BOOT_PROGRESS_CODE_GET_REQUEST;

//
// Structure for the boot progress code get response
//
typedef struct {
  UINT8                                             CompletionCode;
  UINT8                                             DefiningBody;
  IPMI_GROUP_EXTENSION_BOOT_PROGRESS_CODE_FORMAT    BootProgressCode;
} IPMI_GROUP_EXTENSION_BOOT_PROGRESS_CODE_GET_RESPONSE;
#pragma pack()

#endif
