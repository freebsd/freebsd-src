/** @file
  IPMI 2.0 definitions from the IPMI Specification Version 2.0, Revision 1.1.

  This file contains all NetFn Sensor/Event commands, including:
    Event Commands (Chapter 29)
    PEF and Alerting Commands (Chapter 30)
    Sensor Device Commands (Chapter 35)

  See IPMI specification, Appendix G, Command Assignments
  and Appendix H, Sub-function Assignments.

  Copyright (c) 1999 - 2015, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _IPMI_NET_FN_SENSOR_EVENT_H_
#define _IPMI_NET_FN_SENSOR_EVENT_H_

#pragma pack(1)
//
// Net function definition for Sensor command
//
#define IPMI_NETFN_SENSOR_EVENT  0x04

//
// All Sensor commands and their structure definitions to follow here
//

//
//  Definitions for Send Platform Event Message command
//
#define IPMI_SENSOR_PLATFORM_EVENT_MESSAGE  0x02

typedef struct {
  UINT8    GeneratorId;
  UINT8    EvMRevision;
  UINT8    SensorType;
  UINT8    SensorNumber;
  UINT8    EventDirType;
  UINT8    OEMEvData1;
  UINT8    OEMEvData2;
  UINT8    OEMEvData3;
} IPMI_PLATFORM_EVENT_MESSAGE_DATA_REQUEST;

//
// Definitions for Set Sensor Thresholds command
//
#define IPMI_SENSOR_SET_SENSOR_THRESHOLDS  0x26

typedef union {
  struct _SENSOR_BITS {
    UINT8    LowerNonCriticalThreshold    : 1;
    UINT8    LowerCriticalThreshold       : 1;
    UINT8    LowerNonRecoverableThreshold : 1;
    UINT8    UpperNonCriticalThreshold    : 1;
    UINT8    UpperCriticalThreshold       : 1;
    UINT8    UpperNonRecoverableThreshold : 1;
    UINT8    Reserved                     : 2;
  } Bits;
  UINT8    Uint8;
} SENSOR_BITS;

typedef struct _IPMI_SENSOR_SET_SENSOR_THRESHOLD_REQUEST_DATA {
  UINT8          SensorNumber;
  SENSOR_BITS    SetBitEnable;
  UINT8          LowerNonCriticalThreshold;
  UINT8          LowerCriticalThreshold;
  UINT8          LowerNonRecoverableThreshold;
  UINT8          UpperNonCriticalThreshold;
  UINT8          UpperCriticalThreshold;
  UINT8          UpperNonRecoverableThreshold;
} IPMI_SENSOR_SET_SENSOR_THRESHOLD_REQUEST_DATA;

//
// Definitions for Get Sensor Thresholds command
//
#define IPMI_SENSOR_GET_SENSOR_THRESHOLDS  0x27

typedef struct _IPMI_SENSOR_GET_SENSOR_THRESHOLD_RESPONSE_DATA {
  UINT8          CompletionCode;
  SENSOR_BITS    GetBitEnable;
  UINT8          LowerNonCriticalThreshold;
  UINT8          LowerCriticalThreshold;
  UINT8          LowerNonRecoverableThreshold;
  UINT8          UpperNonCriticalThreshold;
  UINT8          UpperCriticalThreshold;
  UINT8          UpperNonRecoverableThreshold;
} IPMI_SENSOR_GET_SENSOR_THRESHOLD_RESPONSE_DATA;

#pragma pack()
#endif
