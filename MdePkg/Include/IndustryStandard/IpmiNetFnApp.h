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

  Copyright (c) 1999 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
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
typedef union {
  struct {
    UINT8  DeviceRevision : 4;
    UINT8  Reserved : 3;
    UINT8  DeviceSdr : 1;
  } Bits;
  UINT8  Uint8;
} IPMI_GET_DEVICE_ID_DEVICE_REV;

typedef union {
  struct {
    UINT8  MajorFirmwareRev : 7;
    UINT8  UpdateMode : 1;
  } Bits;
  UINT8  Uint8;
} IPMI_GET_DEVICE_ID_FIRMWARE_REV_1;

typedef union {
  struct {
    UINT8  SensorDeviceSupport : 1;
    UINT8  SdrRepositorySupport : 1;
    UINT8  SelDeviceSupport : 1;
    UINT8  FruInventorySupport : 1;
    UINT8  IpmbMessageReceiver : 1;
    UINT8  IpmbMessageGenerator : 1;
    UINT8  BridgeSupport : 1;
    UINT8  ChassisSupport : 1;
  } Bits;
  UINT8    Uint8;
} IPMI_GET_DEVICE_ID_DEVICE_SUPPORT;

typedef struct {
  UINT8                              CompletionCode;
  UINT8                              DeviceId;
  IPMI_GET_DEVICE_ID_DEVICE_REV      DeviceRevision;
  IPMI_GET_DEVICE_ID_FIRMWARE_REV_1  FirmwareRev1;
  UINT8                              MinorFirmwareRev;
  UINT8                              SpecificationVersion;
  IPMI_GET_DEVICE_ID_DEVICE_SUPPORT  DeviceSupport;
  UINT8                              ManufacturerId[3];
  UINT16                             ProductId;
  UINT32                             AuxFirmwareRevInfo;
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

//
//  Definitions for System Power State
//
// Working
#define IPMI_SYSTEM_POWER_STATE_S0_G0        0x0
#define IPMI_SYSTEM_POWER_STATE_S1           0x1
#define IPMI_SYSTEM_POWER_STATE_S2           0x2
#define IPMI_SYSTEM_POWER_STATE_S3           0x3
#define IPMI_SYSTEM_POWER_STATE_S4           0x4
// Soft off
#define IPMI_SYSTEM_POWER_STATE_S5_G2        0x5
// Sent when message source cannot differentiate between S4 and S5
#define IPMI_SYSTEM_POWER_STATE_S4_S5        0x6
// Mechanical off
#define IPMI_SYSTEM_POWER_STATE_G3           0x7
// Sleeping - cannot differentiate between S1-S3
#define IPMI_SYSTEM_POWER_STATE_SLEEPING     0x8
// Sleeping - cannot differentiate between S1-S4
#define IPMI_SYSTEM_POWER_STATE_G1_SLEEPING  0x9
// S5 entered by override
#define IPMI_SYSTEM_POWER_STATE_OVERRIDE     0xA
#define IPMI_SYSTEM_POWER_STATE_LEGACY_ON    0x20
#define IPMI_SYSTEM_POWER_STATE_LEGACY_OFF   0x21
#define IPMI_SYSTEM_POWER_STATE_UNKNOWN      0x2A
#define IPMI_SYSTEM_POWER_STATE_NO_CHANGE    0x7F

//
//  Definitions for Device Power State
//
#define IPMI_DEVICE_POWER_STATE_D0           0x0
#define IPMI_DEVICE_POWER_STATE_D1           0x1
#define IPMI_DEVICE_POWER_STATE_D2           0x2
#define IPMI_DEVICE_POWER_STATE_D3           0x3
#define IPMI_DEVICE_POWER_STATE_UNKNOWN      0x2A
#define IPMI_DEVICE_POWER_STATE_NO_CHANGE    0x7F

typedef union {
  struct {
    UINT8  PowerState  : 7;
    UINT8  StateChange : 1;
  } Bits;
  UINT8  Uint8;
} IPMI_ACPI_POWER_STATE;

typedef struct {
  IPMI_ACPI_POWER_STATE  SystemPowerState;
  IPMI_ACPI_POWER_STATE  DevicePowerState;
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
//  Definitions for Set WatchDog Timer command
//
#define IPMI_APP_SET_WATCHDOG_TIMER  0x24

//
//  Constants and Structure definitions for "Set WatchDog Timer" command to follow here
//

//
// Definitions for watchdog timer use
//
#define IPMI_WATCHDOG_TIMER_BIOS_FRB2  0x1
#define IPMI_WATCHDOG_TIMER_BIOS_POST  0x2
#define IPMI_WATCHDOG_TIMER_OS_LOADER  0x3
#define IPMI_WATCHDOG_TIMER_SMS        0x4
#define IPMI_WATCHDOG_TIMER_OEM        0x5

//
//  Structure definition for timer Use
//
typedef union {
  struct {
    UINT8  TimerUse : 3;
    UINT8  Reserved : 3;
    UINT8  TimerRunning : 1;
    UINT8  TimerUseExpirationFlagLog : 1;
  } Bits;
  UINT8  Uint8;
} IPMI_WATCHDOG_TIMER_USE;

//
//  Definitions for watchdog timeout action
//
#define IPMI_WATCHDOG_TIMER_ACTION_NO_ACTION    0x0
#define IPMI_WATCHDOG_TIMER_ACTION_HARD_RESET   0x1
#define IPMI_WATCHDOG_TIMER_ACTION_POWER_DONW   0x2
#define IPMI_WATCHDOG_TIMER_ACTION_POWER_CYCLE  0x3

//
//  Definitions for watchdog pre-timeout interrupt
//
#define IPMI_WATCHDOG_PRE_TIMEOUT_INTERRUPT_NONE       0x0
#define IPMI_WATCHDOG_PRE_TIMEOUT_INTERRUPT_SMI        0x1
#define IPMI_WATCHDOG_PRE_TIMEOUT_INTERRUPT_NMI        0x2
#define IPMI_WATCHDOG_PRE_TIMEOUT_INTERRUPT_MESSAGING  0x3

//
//  Structure definitions for Timer Actions
//
typedef union {
  struct {
    UINT8  TimeoutAction : 3;
    UINT8  Reserved1 : 1;
    UINT8  PreTimeoutInterrupt : 3;
    UINT8  Reserved2 : 1;
  } Bits;
  UINT8  Uint8;
} IPMI_WATCHDOG_TIMER_ACTIONS;

//
//  Bit definitions for Timer use expiration flags
//
#define IPMI_WATCHDOG_TIMER_EXPIRATION_FLAG_BIOS_FRB2  BIT1
#define IPMI_WATCHDOG_TIMER_EXPIRATION_FLAG_BIOS_POST  BIT2
#define IPMI_WATCHDOG_TIMER_EXPIRATION_FLAG_OS_LOAD    BIT3
#define IPMI_WATCHDOG_TIMER_EXPIRATION_FLAG_SMS_OS     BIT4
#define IPMI_WATCHDOG_TIMER_EXPIRATION_FLAG_OEM        BIT5

typedef struct {
  IPMI_WATCHDOG_TIMER_USE         TimerUse;
  IPMI_WATCHDOG_TIMER_ACTIONS     TimerActions;
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
  IPMI_WATCHDOG_TIMER_ACTIONS     TimerActions;
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
typedef union {
  struct {
    UINT8  ReceiveMessageQueueInterrupt : 1;
    UINT8  EventMessageBufferFullInterrupt : 1;
    UINT8  EventMessageBuffer : 1;
    UINT8  SystemEventLogging : 1;
    UINT8  Reserved : 1;
    UINT8  Oem0Enable : 1;
    UINT8  Oem1Enable : 1;
    UINT8  Oem2Enable : 1;
  } Bits;
  UINT8  Uint8;
} IPMI_BMC_GLOBAL_ENABLES;

typedef struct {
  IPMI_BMC_GLOBAL_ENABLES  SetEnables;
} IPMI_SET_BMC_GLOBAL_ENABLES_REQUEST;

//
//  Definitions for Get BMC Global Enables command
//
#define IPMI_APP_GET_BMC_GLOBAL_ENABLES  0x2F

//
//  Constants and Structure definitions for "Get BMC Global Enables " command to follow here
//
typedef struct {
  UINT8                    CompletionCode;
  IPMI_BMC_GLOBAL_ENABLES  GetEnables;
} IPMI_GET_BMC_GLOBAL_ENABLES_RESPONSE;

//
//  Definitions for Clear Message Flags command
//
#define IPMI_APP_CLEAR_MESSAGE_FLAGS 0x30

//
//  Constants and Structure definitions for "Clear Message Flags" command to follow here
//
typedef union {
  struct {
    UINT8  ReceiveMessageQueue : 1;
    UINT8  EventMessageBuffer : 1;
    UINT8  Reserved1 : 1;
    UINT8  WatchdogPerTimeoutInterrupt : 1;
    UINT8  Reserved2 : 1;
    UINT8  Oem0 : 1;
    UINT8  Oem1 : 1;
    UINT8  Oem2 : 1;
  } Bits;
  UINT8    Uint8;
} IPMI_MESSAGE_FLAGS;

typedef struct {
  IPMI_MESSAGE_FLAGS  ClearFlags;
} IPMI_CLEAR_MESSAGE_FLAGS_REQUEST;

//
//  Definitions for Get Message Flags command
//
#define IPMI_APP_GET_MESSAGE_FLAGS 0x31

//
//  Constants and Structure definitions for "Get Message Flags" command to follow here
//
typedef struct {
  UINT8               CompletionCode;
  IPMI_MESSAGE_FLAGS  GetFlags;
} IPMI_GET_MESSAGE_FLAGS_RESPONSE;

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
typedef union {
  struct {
    UINT8  ChannelNumber : 4;
    UINT8  InferredPrivilegeLevel : 4;
  } Bits;
  UINT8  Uint8;
} IPMI_GET_MESSAGE_CHANNEL_NUMBER;

typedef struct {
  UINT8                            CompletionCode;
  IPMI_GET_MESSAGE_CHANNEL_NUMBER  ChannelNumber;
  UINT8                            MessageData[0];
} IPMI_GET_MESSAGE_RESPONSE;

//
//  Definitions for Send Message command
//
#define IPMI_APP_SEND_MESSAGE  0x34

//
//  Constants and Structure definitions for "Send Message" command to follow here
//
typedef union {
  struct {
    UINT8  ChannelNumber : 4;
    UINT8  Authentication : 1;
    UINT8  Encryption : 1;
    UINT8  Tracking : 2;
  } Bits;
  UINT8  Uint8;
} IPMI_SEND_MESSAGE_CHANNEL_NUMBER;

typedef struct {
  UINT8                             CompletionCode;
  IPMI_SEND_MESSAGE_CHANNEL_NUMBER  ChannelNumber;
  UINT8                             MessageData[0];
} IPMI_SEND_MESSAGE_REQUEST;

typedef struct {
  UINT8  CompletionCode;
  UINT8  ResponseData[0];
} IPMI_SEND_MESSAGE_RESPONSE;

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

//
//  Definitions for channel access memory type in Get Channel Access command request
//
#define IPMI_CHANNEL_ACCESS_MEMORY_TYPE_NON_VOLATILE              0x1
#define IPMI_CHANNEL_ACCESS_MEMORY_TYPE_PRESENT_VOLATILE_SETTING  0x2

//
//  Definitions for channel access modes in Get Channel Access command response
//
#define IPMI_CHANNEL_ACCESS_MODES_DISABLED          0x0
#define IPMI_CHANNEL_ACCESS_MODES_PRE_BOOT_ONLY     0x1
#define IPMI_CHANNEL_ACCESS_MODES_ALWAYS_AVAILABLE  0x2
#define IPMI_CHANNEL_ACCESS_MODES_SHARED            0x3

typedef union {
  struct {
    UINT8  ChannelNo : 4;
    UINT8  Reserved : 4;
  } Bits;
  UINT8  Uint8;
} IPMI_GET_CHANNEL_ACCESS_CHANNEL_NUMBER;

typedef union {
  struct {
    UINT8  Reserved : 6;
    UINT8  MemoryType : 2;
  } Bits;
  UINT8  Uint8;
} IPMI_GET_CHANNEL_ACCESS_TYPE;

typedef struct {
  IPMI_GET_CHANNEL_ACCESS_CHANNEL_NUMBER  ChannelNumber;
  IPMI_GET_CHANNEL_ACCESS_TYPE            AccessType;
} IPMI_GET_CHANNEL_ACCESS_REQUEST;

typedef union {
  struct {
    UINT8  AccessMode : 3;
    UINT8  UserLevelAuthEnabled : 1;
    UINT8  MessageAuthEnable : 1;
    UINT8  Alert : 1;
    UINT8  Reserved : 2;
  } Bits;
  UINT8  Uint8;
} IPMI_GET_CHANNEL_ACCESS_CHANNEL_ACCESS;

typedef union {
  struct {
    UINT8  ChannelPriviledgeLimit : 4;
    UINT8  Reserved : 4;
  } Bits;
  UINT8  Uint8;
} IPMI_GET_CHANNEL_ACCESS_PRIVILEGE_LIMIT;

typedef struct {
  UINT8                                    CompletionCode;
  IPMI_GET_CHANNEL_ACCESS_CHANNEL_ACCESS   ChannelAccess;
  IPMI_GET_CHANNEL_ACCESS_PRIVILEGE_LIMIT  PrivilegeLimit;
} IPMI_GET_CHANNEL_ACCESS_RESPONSE;

//
//  Definitions for Get Channel Info command
//
#define IPMI_APP_GET_CHANNEL_INFO  0x42

//
//  Constants and Structure definitions for "Get Channel Info" command to follow here
//

//
//  Definitions for channel media type
//
// IPMB (I2C)
#define IPMI_CHANNEL_MEDIA_TYPE_IPMB              0x1
// ICMB v1.0
#define IPMI_CHANNEL_MEDIA_TYPE_ICMB_1_0          0x2
// ICMB v0.9
#define IPMI_CHANNEL_MEDIA_TYPE_ICMB_0_9          0x3
// 802.3 LAN
#define IPMI_CHANNEL_MEDIA_TYPE_802_3_LAN         0x4
// Asynch. Serial/Modem (RS-232)
#define IPMI_CHANNEL_MEDIA_TYPE_RS_232            0x5
// Other LAN
#define IPMI_CHANNEL_MEDIA_TYPE_OTHER_LAN         0x6
// PCI SMBus
#define IPMI_CHANNEL_MEDIA_TYPE_PCI_SM_BUS        0x7
// SMBus v1.0/1.1
#define IPMI_CHANNEL_MEDIA_TYPE_SM_BUS_V1         0x8
// SMBus v2.0
#define IPMI_CHANNEL_MEDIA_TYPE_SM_BUS_V2         0x9
// USB 1.x
#define IPMI_CHANNEL_MEDIA_TYPE_USB1              0xA
// USB 2.x
#define IPMI_CHANNEL_MEDIA_TYPE_USB2              0xB
// System Interface (KCS, SMIC, or BT)
#define IPMI_CHANNEL_MEDIA_TYPE_SYSTEM_INTERFACE  0xC
// OEM
#define IPMI_CHANNEL_MEDIA_TYPE_OEM_START         0x60
#define IPMI_CHANNEL_MEDIA_TYPE_OEM_END           0x7F

typedef union {
  struct {
    UINT8  ChannelNo : 4;
    UINT8  Reserved : 4;
  } Bits;
  UINT8  Uint8;
} IPMI_CHANNEL_INFO_CHANNEL_NUMBER;

typedef union {
  struct {
    UINT8  ChannelMediumType : 7;
    UINT8  Reserved : 1;
  } Bits;
  UINT8  Uint8;
} IPMI_CHANNEL_INFO_MEDIUM_TYPE;

typedef union {
  struct {
    UINT8  ChannelProtocolType : 5;
    UINT8  Reserved : 3;
  } Bits;
  UINT8  Uint8;
} IPMI_CHANNEL_INFO_PROTOCOL_TYPE;

typedef union {
  struct {
    UINT8  ActiveSessionCount : 6;
    UINT8  SessionSupport : 2;
  } Bits;
  UINT8  Uint8;
} IPMI_CHANNEL_INFO_SESSION_SUPPORT;

typedef struct {
  UINT8   CompletionCode;
  IPMI_CHANNEL_INFO_CHANNEL_NUMBER   ChannelNumber;
  IPMI_CHANNEL_INFO_MEDIUM_TYPE      MediumType;
  IPMI_CHANNEL_INFO_PROTOCOL_TYPE    ProtocolType;
  IPMI_CHANNEL_INFO_SESSION_SUPPORT  SessionSupport;
  UINT8                              VendorId[3];
  UINT16                             AuxChannelInfo;
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
typedef union {
  struct {
    UINT8  ChannelNo : 4;
    UINT8  Reserved : 4;
  } Bits;
  UINT8  Uint8;
} IPMI_GET_USER_ACCESS_CHANNEL_NUMBER;

typedef union {
  struct {
    UINT8  UserId : 6;
    UINT8  Reserved : 2;
  } Bits;
  UINT8  Uint8;
} IPMI_USER_ID;

typedef struct {
  IPMI_GET_USER_ACCESS_CHANNEL_NUMBER  ChannelNumber;
  IPMI_USER_ID                         UserId;
} IPMI_GET_USER_ACCESS_REQUEST;

typedef union {
  struct {
    UINT8  MaxUserId : 6;
    UINT8  Reserved : 2;
  } Bits;
  UINT8  Uint8;
} IPMI_GET_USER_ACCESS_MAX_USER_ID;

typedef union {
  struct {
    UINT8  CurrentUserId : 6;
    UINT8  UserIdEnableStatus : 2;
  } Bits;
  UINT8  Uint8;
} IPMI_GET_USER_ACCESS_CURRENT_USER;

typedef union {
  struct {
    UINT8  FixedUserId : 6;
    UINT8  Reserved : 2;
  } Bits;
  UINT8  Uint8;
} IPMI_GET_USER_ACCESS_FIXED_NAME_USER;

typedef union {
  struct {
    UINT8  UserPrivilegeLimit : 4;
    UINT8  EnableIpmiMessaging : 1;
    UINT8  EnableUserLinkAuthetication : 1;
    UINT8  UserAccessAvailable : 1;
    UINT8  Reserved : 1;
  } Bits;
  UINT8  Uint8;
} IPMI_GET_USER_ACCESS_CHANNEL_ACCESS;

typedef struct {
  UINT8                                 CompletionCode;
  IPMI_GET_USER_ACCESS_MAX_USER_ID      MaxUserId;
  IPMI_GET_USER_ACCESS_CURRENT_USER     CurrentUser;
  IPMI_GET_USER_ACCESS_FIXED_NAME_USER  FixedNameUser;
  IPMI_GET_USER_ACCESS_CHANNEL_ACCESS   ChannelAccess;
} IPMI_GET_USER_ACCESS_RESPONSE;

//
//  Definitions for Set User Name command
//
#define IPMI_APP_SET_USER_NAME 0x45

//
//  Constants and Structure definitions for "Set User Name" command to follow here
//
typedef struct {
  IPMI_USER_ID  UserId;
  UINT8         UserName[16];
} IPMI_SET_USER_NAME_REQUEST;

//
//  Definitions for Get User Name command
//
#define IPMI_APP_GET_USER_NAME 0x46

//
//  Constants and Structure definitions for "Get User Name" command to follow here
//
typedef struct {
  IPMI_USER_ID  UserId;
} IPMI_GET_USER_NAME_REQUEST;

typedef struct {
  UINT8  CompletionCode;
  UINT8  UserName[16];
} IPMI_GET_USER_NAME_RESPONSE;

//
//  Definitions for Set User Password command
//
#define IPMI_APP_SET_USER_PASSWORD 0x47

//
//  Constants and Structure definitions for "Set User Password" command to follow here
//

//
//  Definitions for Set User password command operation type
//
#define IPMI_SET_USER_PASSWORD_OPERATION_TYPE_DISABLE_USER   0x0
#define IPMI_SET_USER_PASSWORD_OPERATION_TYPE_ENABLE_USER    0x1
#define IPMI_SET_USER_PASSWORD_OPERATION_TYPE_SET_PASSWORD   0x2
#define IPMI_SET_USER_PASSWORD_OPERATION_TYPE_TEST_PASSWORD  0x3

//
//  Definitions for Set user password command password size
//
#define IPMI_SET_USER_PASSWORD_PASSWORD_SIZE_16  0x0
#define IPMI_SET_USER_PASSWORD_PASSWORD_SIZE_20  0x1

typedef union {
  struct {
    UINT8  UserId : 6;
    UINT8  Reserved : 1;
    UINT8  PasswordSize : 1;
  } Bits;
  UINT8  Uint8;
} IPMI_SET_USER_PASSWORD_USER_ID;

typedef union {
  struct {
    UINT8  Operation : 2;
    UINT8  Reserved : 6;
  } Bits;
  UINT8  Uint8;
} IPMI_SET_USER_PASSWORD_OPERATION;

typedef struct {
  IPMI_SET_USER_PASSWORD_USER_ID    UserId;
  IPMI_SET_USER_PASSWORD_OPERATION  Operation;
  UINT8                             PasswordData[0];  // 16 or 20 bytes, depending on the 'PasswordSize' field
} IPMI_SET_USER_PASSWORD_REQUEST;

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

//
//  Definitions for Get System Interface Capabilities command SSIF transaction support
//
#define IPMI_GET_SYSTEM_INTERFACE_CAPABILITIES_SSIF_TRANSACTION_SUPPORT_SINGLE_PARTITION_RW             0x0
#define IPMI_GET_SYSTEM_INTERFACE_CAPABILITIES_SSIF_TRANSACTION_SUPPORT_MULTI_PARTITION_RW              0x1
#define IPMI_GET_SYSTEM_INTERFACE_CAPABILITIES_SSIF_TRANSACTION_SUPPORT_MULTI_PARTITION_RW_WITH_MIDDLE  0x2

#pragma pack()
#endif
