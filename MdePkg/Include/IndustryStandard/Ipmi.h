/** @file
  IPMI 2.0 definitions from the IPMI Specification Version 2.0, Revision 1.1.
  IPMI Platform Management FRU Information Storage Definition v1.0 Revision 1.3.

  See IPMI specification, Appendix G, Command Assignments
  and Appendix H, Sub-function Assignments.

  Copyright (c) 1999 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _IPMI_H_
#define _IPMI_H_

#include <IndustryStandard/IpmiNetFnChassis.h>
#include <IndustryStandard/IpmiNetFnBridge.h>
#include <IndustryStandard/IpmiNetFnSensorEvent.h>
#include <IndustryStandard/IpmiNetFnApp.h>
#include <IndustryStandard/IpmiNetFnFirmware.h>
#include <IndustryStandard/IpmiNetFnStorage.h>
#include <IndustryStandard/IpmiNetFnTransport.h>
#include <IndustryStandard/IpmiNetFnGroupExtension.h>

#include <IndustryStandard/IpmiFruInformationStorage.h>

//
// Generic Completion Codes definitions
//
#define IPMI_COMP_CODE_NORMAL                           0x00
#define IPMI_COMP_CODE_NODE_BUSY                        0xC0
#define IPMI_COMP_CODE_INVALID_COMMAND                  0xC1
#define IPMI_COMP_CODE_INVALID_FOR_GIVEN_LUN            0xC2
#define IPMI_COMP_CODE_TIMEOUT                          0xC3
#define IPMI_COMP_CODE_OUT_OF_SPACE                     0xC4
#define IPMI_COMP_CODE_RESERVATION_CANCELED_OR_INVALID  0xC5
#define IPMI_COMP_CODE_REQUEST_DATA_TRUNCATED           0xC6
#define IPMI_COMP_CODE_INVALID_REQUEST_DATA_LENGTH      0xC7
#define IPMI_COMP_CODE_REQUEST_EXCEED_LIMIT             0xC8
#define IPMI_COMP_CODE_OUT_OF_RANGE                     0xC9
#define IPMI_COMP_CODE_CANNOT_RETURN                    0xCA
#define IPMI_COMP_CODE_NOT_PRESENT                      0xCB
#define IPMI_COMP_CODE_INVALID_DATA_FIELD               0xCC
#define IPMI_COMP_CODE_COMMAND_ILLEGAL                  0xCD
#define IPMI_COMP_CODE_CMD_RESP_NOT_PROVIDED            0xCE
#define IPMI_COMP_CODE_FAIL_DUP_REQUEST                 0xCF
#define IPMI_COMP_CODE_SDR_REP_IN_UPDATE_MODE           0xD0
#define IPMI_COMP_CODE_DEV_IN_FW_UPDATE_MODE            0xD1
#define IPMI_COMP_CODE_BMC_INIT_IN_PROGRESS             0xD2
#define IPMI_COMP_CODE_DEST_UNAVAILABLE                 0xD3
#define IPMI_COMP_CODE_INSUFFICIENT_PRIVILEGE           0xD4
#define IPMI_COMP_CODE_UNSUPPORTED_IN_PRESENT_STATE     0xD5
#define IPMI_COMP_CODE_SUBFUNCTION_DISABLED             0xD6
#define IPMI_COMP_CODE_UNSPECIFIED                      0xFF

#endif
