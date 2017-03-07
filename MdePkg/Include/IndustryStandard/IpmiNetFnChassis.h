/** @file
  IPMI 2.0 definitions from the IPMI Specification Version 2.0, Revision 1.1.

  This file contains all NetFn Chassis commands, including:
    Chassis Commands (Chapter 28)

  See IPMI specification, Appendix G, Command Assignments
  and Appendix H, Sub-function Assignments.

  Copyright (c) 1999 - 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef _IPMI_NET_FN_CHASSIS_H_
#define _IPMI_NET_FN_CHASSIS_H_

#pragma pack (1)
//
// Net function definition for Chassis command
//
#define IPMI_NETFN_CHASSIS  0x00

//
//  Below is Definitions for Chassis commands (Chapter 28)
//

//
//  Definitions for Get Chassis Capabilities command
//
#define IPMI_CHASSIS_GET_CAPABILITIES  0x00

//
//  Constants and Structure definitions for "Get Chassis Capabilities" command to follow here
//

//
//  Definitions for Get Chassis Status command
//
#define IPMI_CHASSIS_GET_STATUS  0x01

//
//  Constants and Structure definitions for "Get Chassis Status" command to follow here
//

//
//  Definitions for Chassis Control command
//
#define IPMI_CHASSIS_CONTROL 0x02

//
//  Constants and Structure definitions for "Chassis Control" command to follow here
//

//
//  Definitions for Chassis Reset command
//
#define IPMI_CHASSIS_RESET 0x03

//
//  Constants and Structure definitions for "Chassis Reset" command to follow here
//

//
//  Definitions for Chassis Identify command
//
#define IPMI_CHASSIS_IDENTIFY  0x04

//
//  Constants and Structure definitions for "Chassis Identify" command to follow here
//

//
//  Definitions for Set Chassis Capabilities command
//
#define IPMI_CHASSIS_SET_CAPABILITIES  0x05

//
//  Constants and Structure definitions for "Set Chassis Capabilities" command to follow here
//

//
//  Definitions for Set Power Restore Policy command
//
#define IPMI_CHASSIS_SET_POWER_RESTORE_POLICY  0x06

//
//  Constants and Structure definitions for "Set Power Restore Policy" command to follow here
//

//
//  Definitions for Get System Restart Cause command
//
#define IPMI_CHASSIS_GET_SYSTEM_RESTART_CAUSE  0x07

//
//  Constants and Structure definitions for "Get System Restart Cause" command to follow here
//
typedef enum {
  Unknown,
  ChassisControlCommand,
  ResetViaPushButton,
  PowerupViaPowerButton,
  WatchdogExpiration,
  Oem,
  AutoPowerOnAlwaysRestore,
  AutoPowerOnRestorePrevious,
  ResetViaPef,
  PowerCycleViaPef,
  SoftReset,
  PowerUpViaRtc
} IPMI_SYSTEM_RESTART_CAUSE;

typedef struct {
  UINT8  CompletionCode;
  UINT8  Cause:4;
  UINT8  Reserved:4;
  UINT8  ChannelNumber;
} IPMI_GET_SYSTEM_RESTART_CAUSE_RESPONSE;

//
//  Definitions for Set System BOOT options command
//
#define IPMI_CHASSIS_SET_SYSTEM_BOOT_OPTIONS 0x08

//
//  Constants and Structure definitions for "Set System boot options" command to follow here
//
typedef struct {
  UINT8    ParameterSelector:7;
  UINT8    MarkParameterInvalid:1;
  UINT8    ParameterData[1];
} IPMI_SET_BOOT_OPTIONS_REQUEST;

//
//  Definitions for Get System BOOT options command
//
#define IPMI_CHASSIS_GET_SYSTEM_BOOT_OPTIONS 0x09

//
//  Constants and Structure definitions for "Get System boot options" command to follow here
//
typedef struct {
  UINT8    ParameterSelector:7;
  UINT8    Reserved:1;
  UINT8    SetSelector;
  UINT8    BlockSelector;
} IPMI_GET_BOOT_OPTIONS_REQUEST;

typedef struct {
  UINT8 Parameter;
  UINT8 Valid;
  UINT8 Data1;
  UINT8 Data2;
  UINT8 Data3;
  UINT8 Data4;
  UINT8 Data5;
} IPMI_GET_THE_SYSTEM_BOOT_OPTIONS;

typedef struct {
  UINT8   ParameterVersion;
  UINT8   ParameterValid;
  UINT8   ChannelNumber;
  UINT32  SessionId;
  UINT32  TimeStamp;
  UINT8   Reserved[3];
} IPMI_BOOT_INITIATOR;

//
// Response Parameters for IPMI Get Boot Options
//
typedef struct {
  UINT8   SetInProgress: 2;
  UINT8   Reserved: 6;
} IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_0;

typedef struct {
  UINT8   ServicePartitionSelector;
} IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_1;

typedef struct {
  UINT8   ServicePartitionDiscovered:1;
  UINT8   ServicePartitionScanRequest:1;
  UINT8   Reserved: 5;
} IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_2;

typedef struct {
  UINT8   BmcBootFlagValid: 5;
  UINT8   Reserved: 3;
} IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_3;

typedef struct {
  UINT8   WriteMask;
  UINT8   BootInitiatorAcknowledgeData;
} IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4;

#define BOOT_OPTION_HANDLED_BY_BIOS 0x01

typedef struct {
  //
  // Data 1
  //
  UINT8   Reserved0:6;
  UINT8   PersistentOptions:1;
  UINT8   BootFlagValid:1;
  //
  // Data 2
  //
  UINT8   LockReset:1;
  UINT8   ScreenBlank:1;
  UINT8   BootDeviceSelector:4;
  UINT8   LockKeyboard:1;
  UINT8   CmosClear:1;
  //
  //
  // Data 3
  UINT8   ConsoleRedirection:2;
  UINT8   LockSleep:1;
  UINT8   UserPasswordBypass:1;
  UINT8   ForceProgressEventTrap:1;
  UINT8   BiosVerbosity:2;
  UINT8   LockPower:1;
  //
  // Data 4
  //
  UINT8   BiosMuxControlOverride:2;
  UINT8   BiosSharedModeOverride:1;
  UINT8   Reserved1:4;
} IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5;

typedef struct {
  UINT8   ChannelNumber:4;
  UINT8   Reserved:4;
  UINT8   SessionId[4];
  UINT8   BootInfoTimeStamp[4];
} IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_6;

typedef struct {
  UINT8   SetSelector;
  UINT8   BlockData[16];
} IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_7;

typedef union {
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_0   Parm0;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_1   Parm1;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_2   Parm2;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_3   Parm3;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4   Parm4;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5   Parm5;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_6   Parm6;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_7   Parm7;
} IPMI_BOOT_OPTIONS_PARAMETERS;

typedef struct {
  UINT8    CompletionCode;
  UINT8    ParameterVersion:4;
  UINT8    Reserved:4;
  UINT8    ParameterSelector:7;
  UINT8    ParameterValid:1;
  UINT8    ParameterData[1];
} IPMI_GET_BOOT_OPTIONS_RESPONSE;

//
//  Definitions for Set front panel button enables command
//
#define IPMI_CHASSIS_SET_FRONT_PANEL_BUTTON_ENABLES 0x0A

typedef struct {
  UINT8    DisablePoweroffButton:1;
  UINT8    DisableResetButton:1;
  UINT8    DisableDiagnosticInterruptButton:1;
  UINT8    DisableStandbyButton:1;
  UINT8    Reserved:4;
} IPMI_CHASSIS_SET_FRONT_PANEL_BUTTON_ENABLES_REQUEST;

//
//  Constants and Structure definitions for "Set front panel button enables" command to follow here
//

//
//  Definitions for Set Power Cycle Interval command
//
#define IPMI_CHASSIS_SET_POWER_CYCLE_INTERVALS 0x0B

//
//  Constants and Structure definitions for "Set Power Cycle Interval" command to follow here
//

//
//  Definitions for Get POH Counter command
//
#define IPMI_CHASSIS_GET_POH_COUNTER 0x0F

//
//  Constants and Structure definitions for "Get POH Counter" command to follow here
//
#pragma pack()
#endif
