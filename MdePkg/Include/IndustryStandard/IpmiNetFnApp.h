/** @file
  IPMI 2.0 definitions from the IPMI Specification Version 2.0, Revision 1.1.

  This file contains all NetFn App commands, including:
    IPM Device "Global" Commands (Chapter 20)
    Firmware Firewall & Command Discovery Commands (Chapter 21)
    BMC Watchdog Timer Commands (Chapter 27)
    IPMI Messaging Support Commands (Chapter 22)
    RMCP+ Support and Payload Commands (Chapter 24)

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

#ifndef _IPMI_NET_FN_APP_H_
#define _IPMI_NET_FN_APP_H_

#pragma pack(1)
//
// Net function definition for App command
//
#define IPMI_NETFN_APP  0x06

//
//  Below is Definitions for IPM Device "Global" Commands  (Chapter 20)
//

//
//  Definitions for Get Device ID command
//
#define IPMI_APP_GET_DEVICE_ID 0x1

//
//  Constants and Structure definitions for "Get Device ID" command to follow here
//
typedef struct {
  UINT8   CompletionCode;
  UINT8   DeviceId;
  UINT8   DeviceRevision : 4;
  UINT8   Reserved : 3;
  UINT8   DeviceSdr : 1;
  UINT8   MajorFirmwareRev : 7;
  UINT8   UpdateMode : 1;
  UINT8   MinorFirmwareRev;
  UINT8   SpecificationVersion;
  UINT8   SensorDeviceSupport : 1;
  UINT8   SdrRepositorySupport : 1;
  UINT8   SelDeviceSupport : 1;
  UINT8   FruInventorySupport : 1;
  UINT8   IpmbMessageReceiver : 1;
  UINT8   IpmbMessageGenerator : 1;
  UINT8   BridgeSupport : 1;
  UINT8   ChassisSupport : 1;
  UINT8   ManufacturerId[3];
  UINT16  ProductId;
  UINT32  AuxFirmwareRevInfo;
} IPMI_GET_DEVICE_ID_RESPONSE;


//
//  Definitions for Cold Reset command
//
#define IPMI_APP_COLD_RESET  0x2

//
//  Constants and Structure definitions for "Cold Reset" command to follow here
//

//
//  Definitions for Warm Reset command
//
#define IPMI_APP_WARM_RESET  0x3

//
//  Constants and Structure definitions for "Warm Reset" command to follow here
//

//
//  Definitions for Get Self Results command
//
#define IPMI_APP_GET_SELFTEST_RESULTS  0x4

//
//  Constants and Structure definitions for "Get Self Test Results" command to follow here
//
typedef struct {
  UINT8  CompletionCode;
  UINT8  Result;
  UINT8  Param;
} IPMI_SELF_TEST_RESULT_RESPONSE;

#define IPMI_APP_SELFTEST_NO_ERROR             0x55
#define IPMI_APP_SELFTEST_NOT_IMPLEMENTED      0x56
#define IPMI_APP_SELFTEST_ERROR                0x57
#define IPMI_APP_SELFTEST_FATAL_HW_ERROR       0x58
#define IPMI_APP_SELFTEST_INACCESSIBLE_SEL     0x80
#define IPMI_APP_SELFTEST_INACCESSIBLE_SDR     0x40
#define IPMI_APP_SELFTEST_INACCESSIBLE_FRU     0x20
#define IPMI_APP_SELFTEST_IPMB_SIGNAL_FAIL     0x10
#define IPMI_APP_SELFTEST_SDR_REPOSITORY_EMPTY 0x08
#define IPMI_APP_SELFTEST_FRU_CORRUPT          0x04
#define IPMI_APP_SELFTEST_FW_BOOTBLOCK_CORRUPT 0x02
#define IPMI_APP_SELFTEST_FW_CORRUPT           0x01

//
//  Definitions for Manufacturing Test ON command
//
#define IPMI_APP_MANUFACTURING_TEST_ON 0x5

//
//  Constants and Structure definitions for "Manufacturing Test ON" command to follow here
//

//
//  Definitions for Set ACPI Power State command
//
#define IPMI_APP_SET_ACPI_POWERSTATE 0x6

//
//  Constants and Structure definitions for "Set ACPI Power State" command to follow here
//
typedef struct {
  UINT8  AcpiSystemPowerState  : 7;
  UINT8  AcpiSystemStateChange : 1;
  UINT8  AcpiDevicePowerState  : 7;
  UINT8  AcpiDeviceStateChange : 1;
} IPMI_SET_ACPI_POWER_STATE_REQUEST;

//
//  Definitions for Get ACPI Power State command
//
#define IPMI_APP_GET_ACPI_POWERSTATE 0x7

//
//  Constants and Structure definitions for "Get ACPI Power State" command to follow here
//

//
//  Definitions for Get Device GUID command
//
#define IPMI_APP_GET_DEVICE_GUID 0x8

//
//  Constants and Structure definitions for "Get Device GUID" command to follow here
//
//
//  Message structure definition for "Get Device Guid" IPMI command
//
typedef struct {
  UINT8  CompletionCode;
  UINT8  Guid[16];
} IPMI_GET_DEVICE_GUID_RESPONSE;

//
//  Below is Definitions for BMC Watchdog Timer Commands (Chapter 27)
//

//
//  Definitions for Reset WatchDog Timer command
//
#define IPMI_APP_RESET_WATCHDOG_TIMER  0x22

//
//  Constants and Structure definitions for "Reset WatchDog Timer" command to follow here
//
typedef struct {
  UINT8 TimerUse : 3;
  UINT8 Reserved : 3;
  UINT8 TimerRunning : 1;
  UINT8 TimerUseExpirationFlagLog : 1;
} IPMI_WATCHDOG_TIMER_USE;

//
//  Definitions for Set WatchDog Timer command
//
#define IPMI_APP_SET_WATCHDOG_TIMER  0x24

//
//  Constants and Structure definitions for "Set WatchDog Timer" command to follow here
//
typedef struct {
  IPMI_WATCHDOG_TIMER_USE         TimerUse;
  UINT8                           TimerActions;
  UINT8                           PretimeoutInterval;
  UINT8                           TimerUseExpirationFlagsClear;
  UINT16                          InitialCountdownValue;
} IPMI_SET_WATCHDOG_TIMER_REQUEST;

//
//  Definitions for Get WatchDog Timer command
//
#define IPMI_APP_GET_WATCHDOG_TIMER  0x25

//
//  Constants and Structure definitions for "Get WatchDog Timer" command to follow here
//
typedef struct {
  UINT8                           CompletionCode;
  IPMI_WATCHDOG_TIMER_USE         TimerUse;
  UINT8                           TimerActions;
  UINT8                           PretimeoutInterval;
  UINT8                           TimerUseExpirationFlagsClear;
  UINT16                          InitialCountdownValue;
  UINT16                          PresentCountdownValue;
} IPMI_GET_WATCHDOG_TIMER_RESPONSE;

//
//  Below is Definitions for IPMI Messaging Support Commands (Chapter 22)
//

//
//  Definitions for Set BMC Global Enables command
//
#define IPMI_APP_SET_BMC_GLOBAL_ENABLES  0x2E

//
//  Constants and Structure definitions for "Set BMC Global Enables " command to follow here
//

//
//  Definitions for Get BMC Global Enables command
//
#define IPMI_APP_GET_BMC_GLOBAL_ENABLES  0x2F

//
//  Constants and Structure definitions for "Get BMC Global Enables " command to follow here
//

//
//  Definitions for Clear Message Flags command
//
#define IPMI_APP_CLEAR_MESSAGE_FLAGS 0x30

//
//  Constants and Structure definitions for "Clear Message Flags" command to follow here
//

//
//  Definitions for Get Message Flags command
//
#define IPMI_APP_GET_MESSAGE_FLAGS 0x31

//
//  Constants and Structure definitions for "Get Message Flags" command to follow here
//

//
//  Definitions for Enable Message Channel Receive command
//
#define IPMI_APP_ENABLE_MESSAGE_CHANNEL_RECEIVE  0x32

//
//  Constants and Structure definitions for "Enable Message Channel Receive" command to follow here
//

//
//  Definitions for Get Message command
//
#define IPMI_APP_GET_MESSAGE 0x33

//
//  Constants and Structure definitions for "Get Message" command to follow here
//

//
//  Definitions for Send Message command
//
#define IPMI_APP_SEND_MESSAGE  0x34

//
//  Constants and Structure definitions for "Send Message" command to follow here
//

//
//  Definitions for Read Event Message Buffer command
//
#define IPMI_APP_READ_EVENT_MSG_BUFFER 0x35

//
//  Constants and Structure definitions for "Read Event Message Buffer" command to follow here
//

//
//  Definitions for Get BT Interface Capabilities command
//
#define IPMI_APP_GET_BT_INTERFACE_CAPABILITY 0x36

//
//  Constants and Structure definitions for "Get BT Interface Capabilities" command to follow here
//

//
//  Definitions for Get System GUID command
//
#define IPMI_APP_GET_SYSTEM_GUID 0x37

//
//  Constants and Structure definitions for "Get System GUID" command to follow here
//

//
//  Definitions for Get Channel Authentication Capabilities command
//
#define IPMI_APP_GET_CHANNEL_AUTHENTICATION_CAPABILITIES 0x38

//
//  Constants and Structure definitions for "Get Channel Authentication Capabilities" command to follow here
//

//
//  Definitions for Get Session Challenge command
//
#define IPMI_APP_GET_SESSION_CHALLENGE 0x39

//
//  Constants and Structure definitions for "Get Session Challenge" command to follow here
//

//
//  Definitions for Activate Session command
//
#define IPMI_APP_ACTIVATE_SESSION  0x3A

//
//  Constants and Structure definitions for "Activate Session" command to follow here
//

//
//  Definitions for Set Session Privelege Level command
//
#define IPMI_APP_SET_SESSION_PRIVELEGE_LEVEL 0x3B

//
//  Constants and Structure definitions for "Set Session Privelege Level" command to follow here
//

//
//  Definitions for Close Session command
//
#define IPMI_APP_CLOSE_SESSION 0x3C

//
//  Constants and Structure definitions for "Close Session" command to follow here
//

//
//  Definitions for Get Session Info command
//
#define IPMI_APP_GET_SESSION_INFO  0x3D

//
//  Constants and Structure definitions for "Get Session Info" command to follow here
//

//
//  Definitions for Get Auth Code command
//
#define IPMI_APP_GET_AUTHCODE  0x3F

//
//  Constants and Structure definitions for "Get AuthCode" command to follow here
//

//
//  Definitions for Set Channel Access command
//
#define IPMI_APP_SET_CHANNEL_ACCESS  0x40

//
//  Constants and Structure definitions for "Set Channel Access" command to follow here
//

//
//  Definitions for Get Channel Access command
//
#define IPMI_APP_GET_CHANNEL_ACCESS  0x41

//
//  Constants and Structure definitions for "Get Channel Access" command to follow here
//
typedef struct {
  UINT8  ChannelNo : 4;
  UINT8  Reserve1 : 4;
  UINT8  Reserve2 : 6;
  UINT8  MemoryType : 2;
} IPMI_GET_CHANNEL_ACCESS_REQUEST;

typedef struct {
  UINT8  CompletionCode;
  UINT8  AccessMode : 3;
  UINT8  UserLevelAuthEnabled : 1;
  UINT8  MessageAuthEnable : 1;
  UINT8  Alert : 1;
  UINT8  Reserve1 : 2;
  UINT8  ChannelPriviledgeLimit : 4;
  UINT8  Reserve2 : 4;
} IPMI_GET_CHANNEL_ACCESS_RESPONSE;

//
//  Definitions for Get Channel Info command
//
#define IPMI_APP_GET_CHANNEL_INFO  0x42

//
//  Constants and Structure definitions for "Get Channel Info" command to follow here
//
typedef struct {
  UINT8   CompletionCode;
  UINT8   ChannelNo : 4;
  UINT8   Reserve1 : 4;
  UINT8   ChannelMediumType : 7;
  UINT8   Reserve2 : 1;
  UINT8   ChannelProtocolType : 5;
  UINT8   Reserve3 : 3;
  UINT8   ActiveSessionCount : 6;
  UINT8   SessionSupport : 2;
  UINT8   VendorId[3];
  UINT16  AuxChannelInfo;
} IPMI_GET_CHANNEL_INFO_RESPONSE;

//
//  Definitions for Get Channel Info command
//
#define IPMI_APP_GET_CHANNEL_INFO  0x42

//
//  Constants and Structure definitions for "Get Channel Info" command to follow here
//

//
//  Definitions for Set User Access command
//
#define IPMI_APP_SET_USER_ACCESS 0x43

//
//  Constants and Structure definitions for "Set User Access" command to follow here
//

//
//  Definitions for Get User Access command
//
#define IPMI_APP_GET_USER_ACCESS 0x44

//
//  Constants and Structure definitions for "Get User Access" command to follow here
//

//
//  Definitions for Set User Name command
//
#define IPMI_APP_SET_USER_NAME 0x45

//
//  Constants and Structure definitions for "Set User Name" command to follow here
//

//
//  Definitions for Get User Name command
//
#define IPMI_APP_GET_USER_NAME 0x46

//
//  Constants and Structure definitions for "Get User Name" command to follow here
//

//
//  Definitions for Set User Password command
//
#define IPMI_APP_SET_USER_PASSWORD 0x47

//
//  Constants and Structure definitions for "Set User Password" command to follow here
//

//
//  Below is Definitions for RMCP+ Support and Payload Commands (Chapter 24)
//

//
//  Definitions for Activate Payload command
//
#define IPMI_APP_ACTIVATE_PAYLOAD  0x48

//
//  Constants and Structure definitions for "Activate Payload" command to follow here
//

//
//  Definitions for De-Activate Payload command
//
#define IPMI_APP_DEACTIVATE_PAYLOAD  0x49

//
//  Constants and Structure definitions for "DeActivate Payload" command to follow here
//

//
//  Definitions for Get Payload activation Status command
//
#define IPMI_APP_GET_PAYLOAD_ACTIVATION_STATUS 0x4a

//
//  Constants and Structure definitions for "Get Payload activation Status" command to follow here
//

//
//  Definitions for Get Payload Instance Info command
//
#define IPMI_APP_GET_PAYLOAD_INSTANCE_INFO 0x4b

//
//  Constants and Structure definitions for "Get Payload Instance Info" command to follow here
//

//
//  Definitions for Set User Payload Access command
//
#define IPMI_APP_SET_USER_PAYLOAD_ACCESS 0x4C

//
//  Constants and Structure definitions for "Set User Payload Access" command to follow here
//

//
//  Definitions for Get User Payload Access command
//
#define IPMI_APP_GET_USER_PAYLOAD_ACCESS 0x4D

//
//  Constants and Structure definitions for "Get User Payload Access" command to follow here
//

//
//  Definitions for Get Channel Payload Support command
//
#define IPMI_APP_GET_CHANNEL_PAYLOAD_SUPPORT 0x4E

//
//  Constants and Structure definitions for "Get Channel Payload Support" command to follow here
//

//
//  Definitions for Get Channel Payload Version command
//
#define IPMI_APP_GET_CHANNEL_PAYLOAD_VERSION 0x4F

//
//  Constants and Structure definitions for "Get Channel Payload Version" command to follow here
//

//
//  Definitions for Get Channel OEM Payload Info command
//
#define IPMI_APP_GET_CHANNEL_OEM_PAYLOAD_INFO  0x50

//
//  Constants and Structure definitions for "Get Channel OEM Payload Info" command to follow here
//

//
//  Definitions for  Master Write-Read command
//
#define IPMI_APP_MASTER_WRITE_READ 0x52

//
//  Constants and Structure definitions for "Master Write Read" command to follow here
//

//
//  Definitions for  Get Channel Cipher Suites command
//
#define IPMI_APP_GET_CHANNEL_CIPHER_SUITES 0x54

//
//  Constants and Structure definitions for "Get Channel Cipher Suites" command to follow here
//

//
//  Below is Definitions for RMCP+ Support and Payload Commands (Chapter 24, Section 3)
//

//
//  Definitions for  Suspend-Resume Payload Encryption command
//
#define IPMI_APP_SUSPEND_RESUME_PAYLOAD_ENCRYPTION 0x55

//
//  Constants and Structure definitions for "Suspend-Resume Payload Encryption" command to follow here
//

//
//  Below is Definitions for IPMI Messaging Support Commands (Chapter 22, Section 25 and 9)
//

//
//  Definitions for  Set Channel Security Keys command
//
#define IPMI_APP_SET_CHANNEL_SECURITY_KEYS 0x56

//
//  Constants and Structure definitions for "Set Channel Security Keys" command to follow here
//

//
//  Definitions for  Get System Interface Capabilities command
//
#define IPMI_APP_GET_SYSTEM_INTERFACE_CAPABILITIES 0x57

//
//  Constants and Structure definitions for "Get System Interface Capabilities" command to follow here
//

#pragma pack()
#endif
