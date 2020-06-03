/** @file
  Support for the latest PCI standard.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2016 Hewlett Packard Enterprise Development LP<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _PCIEXPRESS21_H_
#define _PCIEXPRESS21_H_

#include <IndustryStandard/Pci30.h>

/**
  Macro that converts PCI Bus, PCI Device, PCI Function and PCI Register to an
  ECAM (Enhanced Configuration Access Mechanism) address. The unused upper bits
  of Bus, Device, Function and Register are stripped prior to the generation of
  the address.

  @param  Bus       PCI Bus number. Range 0..255.
  @param  Device    PCI Device number. Range 0..31.
  @param  Function  PCI Function number. Range 0..7.
  @param  Register  PCI Register number. Range 0..4095.

  @return The encode ECAM address.

**/
#define PCI_ECAM_ADDRESS(Bus,Device,Function,Offset) \
  (((Offset) & 0xfff) | (((Function) & 0x07) << 12) | (((Device) & 0x1f) << 15) | (((Bus) & 0xff) << 20))

#pragma pack(1)
///
/// PCI Express Capability Structure
///
typedef union {
  struct {
    UINT16 Version : 4;
    UINT16 DevicePortType : 4;
    UINT16 SlotImplemented : 1;
    UINT16 InterruptMessageNumber : 5;
    UINT16 Undefined : 1;
    UINT16 Reserved : 1;
  } Bits;
  UINT16   Uint16;
} PCI_REG_PCIE_CAPABILITY;

#define PCIE_DEVICE_PORT_TYPE_PCIE_ENDPOINT                    0
#define PCIE_DEVICE_PORT_TYPE_LEGACY_PCIE_ENDPOINT             1
#define PCIE_DEVICE_PORT_TYPE_ROOT_PORT                        4
#define PCIE_DEVICE_PORT_TYPE_UPSTREAM_PORT                    5
#define PCIE_DEVICE_PORT_TYPE_DOWNSTREAM_PORT                  6
#define PCIE_DEVICE_PORT_TYPE_PCIE_TO_PCI_BRIDGE               7
#define PCIE_DEVICE_PORT_TYPE_PCI_TO_PCIE_BRIDGE               8
#define PCIE_DEVICE_PORT_TYPE_ROOT_COMPLEX_INTEGRATED_ENDPOINT 9
#define PCIE_DEVICE_PORT_TYPE_ROOT_COMPLEX_EVENT_COLLECTOR     10

typedef union {
  struct {
    UINT32 MaxPayloadSize : 3;
    UINT32 PhantomFunctions : 2;
    UINT32 ExtendedTagField : 1;
    UINT32 EndpointL0sAcceptableLatency : 3;
    UINT32 EndpointL1AcceptableLatency : 3;
    UINT32 Undefined : 3;
    UINT32 RoleBasedErrorReporting : 1;
    UINT32 Reserved : 2;
    UINT32 CapturedSlotPowerLimitValue : 8;
    UINT32 CapturedSlotPowerLimitScale : 2;
    UINT32 FunctionLevelReset : 1;
    UINT32 Reserved2 : 3;
  } Bits;
  UINT32   Uint32;
} PCI_REG_PCIE_DEVICE_CAPABILITY;

typedef union {
  struct {
    UINT16 CorrectableError : 1;
    UINT16 NonFatalError : 1;
    UINT16 FatalError : 1;
    UINT16 UnsupportedRequest : 1;
    UINT16 RelaxedOrdering : 1;
    UINT16 MaxPayloadSize : 3;
    UINT16 ExtendedTagField : 1;
    UINT16 PhantomFunctions : 1;
    UINT16 AuxPower : 1;
    UINT16 NoSnoop : 1;
    UINT16 MaxReadRequestSize : 3;
    UINT16 BridgeConfigurationRetryOrFunctionLevelReset : 1;
  } Bits;
  UINT16   Uint16;
} PCI_REG_PCIE_DEVICE_CONTROL;

#define PCIE_MAX_PAYLOAD_SIZE_128B   0
#define PCIE_MAX_PAYLOAD_SIZE_256B   1
#define PCIE_MAX_PAYLOAD_SIZE_512B   2
#define PCIE_MAX_PAYLOAD_SIZE_1024B  3
#define PCIE_MAX_PAYLOAD_SIZE_2048B  4
#define PCIE_MAX_PAYLOAD_SIZE_4096B  5
#define PCIE_MAX_PAYLOAD_SIZE_RVSD1  6
#define PCIE_MAX_PAYLOAD_SIZE_RVSD2  7

#define PCIE_MAX_READ_REQ_SIZE_128B    0
#define PCIE_MAX_READ_REQ_SIZE_256B    1
#define PCIE_MAX_READ_REQ_SIZE_512B    2
#define PCIE_MAX_READ_REQ_SIZE_1024B   3
#define PCIE_MAX_READ_REQ_SIZE_2048B   4
#define PCIE_MAX_READ_REQ_SIZE_4096B   5
#define PCIE_MAX_READ_REQ_SIZE_RVSD1   6
#define PCIE_MAX_READ_REQ_SIZE_RVSD2   7

typedef union {
  struct {
    UINT16 CorrectableError : 1;
    UINT16 NonFatalError : 1;
    UINT16 FatalError : 1;
    UINT16 UnsupportedRequest : 1;
    UINT16 AuxPower : 1;
    UINT16 TransactionsPending : 1;
    UINT16 Reserved : 10;
  } Bits;
  UINT16   Uint16;
} PCI_REG_PCIE_DEVICE_STATUS;

typedef union {
  struct {
    UINT32 MaxLinkSpeed : 4;
    UINT32 MaxLinkWidth : 6;
    UINT32 Aspm : 2;
    UINT32 L0sExitLatency : 3;
    UINT32 L1ExitLatency : 3;
    UINT32 ClockPowerManagement : 1;
    UINT32 SurpriseDownError : 1;
    UINT32 DataLinkLayerLinkActive : 1;
    UINT32 LinkBandwidthNotification : 1;
    UINT32 AspmOptionalityCompliance : 1;
    UINT32 Reserved : 1;
    UINT32 PortNumber : 8;
  } Bits;
  UINT32   Uint32;
} PCI_REG_PCIE_LINK_CAPABILITY;

#define PCIE_LINK_ASPM_L0S BIT0
#define PCIE_LINK_ASPM_L1  BIT1

typedef union {
  struct {
    UINT16 AspmControl : 2;
    UINT16 Reserved : 1;
    UINT16 ReadCompletionBoundary : 1;
    UINT16 LinkDisable : 1;
    UINT16 RetrainLink : 1;
    UINT16 CommonClockConfiguration : 1;
    UINT16 ExtendedSynch : 1;
    UINT16 ClockPowerManagement : 1;
    UINT16 HardwareAutonomousWidthDisable : 1;
    UINT16 LinkBandwidthManagementInterrupt : 1;
    UINT16 LinkAutonomousBandwidthInterrupt : 1;
  } Bits;
  UINT16   Uint16;
} PCI_REG_PCIE_LINK_CONTROL;

typedef union {
  struct {
    UINT16 CurrentLinkSpeed : 4;
    UINT16 NegotiatedLinkWidth : 6;
    UINT16 Undefined : 1;
    UINT16 LinkTraining : 1;
    UINT16 SlotClockConfiguration : 1;
    UINT16 DataLinkLayerLinkActive : 1;
    UINT16 LinkBandwidthManagement : 1;
    UINT16 LinkAutonomousBandwidth : 1;
  } Bits;
  UINT16   Uint16;
} PCI_REG_PCIE_LINK_STATUS;

typedef union {
  struct {
    UINT32 AttentionButton : 1;
    UINT32 PowerController : 1;
    UINT32 MrlSensor : 1;
    UINT32 AttentionIndicator : 1;
    UINT32 PowerIndicator : 1;
    UINT32 HotPlugSurprise : 1;
    UINT32 HotPlugCapable : 1;
    UINT32 SlotPowerLimitValue : 8;
    UINT32 SlotPowerLimitScale : 2;
    UINT32 ElectromechanicalInterlock : 1;
    UINT32 NoCommandCompleted : 1;
    UINT32 PhysicalSlotNumber : 13;
  } Bits;
  UINT32   Uint32;
} PCI_REG_PCIE_SLOT_CAPABILITY;

typedef union {
  struct {
    UINT16 AttentionButtonPressed : 1;
    UINT16 PowerFaultDetected : 1;
    UINT16 MrlSensorChanged : 1;
    UINT16 PresenceDetectChanged : 1;
    UINT16 CommandCompletedInterrupt : 1;
    UINT16 HotPlugInterrupt : 1;
    UINT16 AttentionIndicator : 2;
    UINT16 PowerIndicator : 2;
    UINT16 PowerController : 1;
    UINT16 ElectromechanicalInterlock : 1;
    UINT16 DataLinkLayerStateChanged : 1;
    UINT16 Reserved : 3;
  } Bits;
  UINT16   Uint16;
} PCI_REG_PCIE_SLOT_CONTROL;

typedef union {
  struct {
    UINT16 AttentionButtonPressed : 1;
    UINT16 PowerFaultDetected : 1;
    UINT16 MrlSensorChanged : 1;
    UINT16 PresenceDetectChanged : 1;
    UINT16 CommandCompleted : 1;
    UINT16 MrlSensor : 1;
    UINT16 PresenceDetect : 1;
    UINT16 ElectromechanicalInterlock : 1;
    UINT16 DataLinkLayerStateChanged : 1;
    UINT16 Reserved : 7;
  } Bits;
  UINT16   Uint16;
} PCI_REG_PCIE_SLOT_STATUS;

typedef union {
  struct {
    UINT16 SystemErrorOnCorrectableError : 1;
    UINT16 SystemErrorOnNonFatalError : 1;
    UINT16 SystemErrorOnFatalError : 1;
    UINT16 PmeInterrupt : 1;
    UINT16 CrsSoftwareVisibility : 1;
    UINT16 Reserved : 11;
  } Bits;
  UINT16   Uint16;
} PCI_REG_PCIE_ROOT_CONTROL;

typedef union {
  struct {
    UINT16 CrsSoftwareVisibility : 1;
    UINT16 Reserved : 15;
  } Bits;
  UINT16   Uint16;
} PCI_REG_PCIE_ROOT_CAPABILITY;

typedef union {
  struct {
    UINT32 PmeRequesterId : 16;
    UINT32 PmeStatus : 1;
    UINT32 PmePending : 1;
    UINT32 Reserved : 14;
  } Bits;
  UINT32   Uint32;
} PCI_REG_PCIE_ROOT_STATUS;

typedef union {
  struct {
    UINT32 CompletionTimeoutRanges : 4;
    UINT32 CompletionTimeoutDisable : 1;
    UINT32 AriForwarding : 1;
    UINT32 AtomicOpRouting : 1;
    UINT32 AtomicOp32Completer : 1;
    UINT32 AtomicOp64Completer : 1;
    UINT32 Cas128Completer : 1;
    UINT32 NoRoEnabledPrPrPassing : 1;
    UINT32 LtrMechanism : 1;
    UINT32 TphCompleter : 2;
    UINT32 LnSystemCLS : 2;
    UINT32 TenBitTagCompleterSupported : 1;
    UINT32 TenBitTagRequesterSupported : 1;
    UINT32 Obff : 2;
    UINT32 ExtendedFmtField : 1;
    UINT32 EndEndTlpPrefix : 1;
    UINT32 MaxEndEndTlpPrefixes : 2;
    UINT32 EmergencyPowerReductionSupported : 2;
    UINT32 EmergencyPowerReductionInitializationRequired : 1;
    UINT32 Reserved3 : 4;
    UINT32 FrsSupported : 1;
  } Bits;
  UINT32   Uint32;
} PCI_REG_PCIE_DEVICE_CAPABILITY2;

#define PCIE_COMPLETION_TIMEOUT_NOT_SUPPORTED           0
#define PCIE_COMPLETION_TIMEOUT_RANGE_A_SUPPORTED       1
#define PCIE_COMPLETION_TIMEOUT_RANGE_B_SUPPORTED       2
#define PCIE_COMPLETION_TIMEOUT_RANGE_A_B_SUPPORTED     3
#define PCIE_COMPLETION_TIMEOUT_RANGE_B_C_SUPPORTED     6
#define PCIE_COMPLETION_TIMEOUT_RANGE_A_B_C_SUPPORTED   7
#define PCIE_COMPLETION_TIMEOUT_RANGE_B_C_D_SUPPORTED   14
#define PCIE_COMPLETION_TIMEOUT_RANGE_A_B_C_D_SUPPORTED 15

#define PCIE_DEVICE_CAPABILITY_OBFF_MESSAGE BIT0
#define PCIE_DEVICE_CAPABILITY_OBFF_WAKE    BIT1

typedef union {
  struct {
    UINT16 CompletionTimeoutValue : 4;
    UINT16 CompletionTimeoutDisable : 1;
    UINT16 AriForwarding : 1;
    UINT16 AtomicOpRequester : 1;
    UINT16 AtomicOpEgressBlocking : 1;
    UINT16 IdoRequest : 1;
    UINT16 IdoCompletion : 1;
    UINT16 LtrMechanism : 1;
    UINT16 EmergencyPowerReductionRequest : 1;
    UINT16 TenBitTagRequesterEnable : 1;
    UINT16 Obff : 2;
    UINT16 EndEndTlpPrefixBlocking : 1;
  } Bits;
  UINT16   Uint16;
} PCI_REG_PCIE_DEVICE_CONTROL2;

#define PCIE_COMPLETION_TIMEOUT_50US_50MS   0
#define PCIE_COMPLETION_TIMEOUT_50US_100US  1
#define PCIE_COMPLETION_TIMEOUT_1MS_10MS    2
#define PCIE_COMPLETION_TIMEOUT_16MS_55MS   5
#define PCIE_COMPLETION_TIMEOUT_65MS_210MS  6
#define PCIE_COMPLETION_TIMEOUT_260MS_900MS 9
#define PCIE_COMPLETION_TIMEOUT_1S_3_5S     10
#define PCIE_COMPLETION_TIMEOUT_4S_13S      13
#define PCIE_COMPLETION_TIMEOUT_17S_64S     14

#define PCIE_DEVICE_CONTROL_OBFF_DISABLED  0
#define PCIE_DEVICE_CONTROL_OBFF_MESSAGE_A 1
#define PCIE_DEVICE_CONTROL_OBFF_MESSAGE_B 2
#define PCIE_DEVICE_CONTROL_OBFF_WAKE      3

typedef union {
  struct {
    UINT32 Reserved : 1;
    UINT32 LinkSpeedsVector : 7;
    UINT32 Crosslink : 1;
    UINT32 Reserved2 : 23;
  } Bits;
  UINT32   Uint32;
} PCI_REG_PCIE_LINK_CAPABILITY2;

typedef union {
  struct {
    UINT16 TargetLinkSpeed : 4;
    UINT16 EnterCompliance : 1;
    UINT16 HardwareAutonomousSpeedDisable : 1;
    UINT16 SelectableDeemphasis : 1;
    UINT16 TransmitMargin : 3;
    UINT16 EnterModifiedCompliance : 1;
    UINT16 ComplianceSos : 1;
    UINT16 CompliancePresetDeemphasis : 4;
  } Bits;
  UINT16   Uint16;
} PCI_REG_PCIE_LINK_CONTROL2;

typedef union {
  struct {
    UINT16 CurrentDeemphasisLevel : 1;
    UINT16 EqualizationComplete : 1;
    UINT16 EqualizationPhase1Successful : 1;
    UINT16 EqualizationPhase2Successful : 1;
    UINT16 EqualizationPhase3Successful : 1;
    UINT16 LinkEqualizationRequest : 1;
    UINT16 Reserved : 10;
  } Bits;
  UINT16   Uint16;
} PCI_REG_PCIE_LINK_STATUS2;

typedef struct {
  EFI_PCI_CAPABILITY_HDR          Hdr;
  PCI_REG_PCIE_CAPABILITY         Capability;
  PCI_REG_PCIE_DEVICE_CAPABILITY  DeviceCapability;
  PCI_REG_PCIE_DEVICE_CONTROL     DeviceControl;
  PCI_REG_PCIE_DEVICE_STATUS      DeviceStatus;
  PCI_REG_PCIE_LINK_CAPABILITY    LinkCapability;
  PCI_REG_PCIE_LINK_CONTROL       LinkControl;
  PCI_REG_PCIE_LINK_STATUS        LinkStatus;
  PCI_REG_PCIE_SLOT_CAPABILITY    SlotCapability;
  PCI_REG_PCIE_SLOT_CONTROL       SlotControl;
  PCI_REG_PCIE_SLOT_STATUS        SlotStatus;
  PCI_REG_PCIE_ROOT_CONTROL       RootControl;
  PCI_REG_PCIE_ROOT_CAPABILITY    RootCapability;
  PCI_REG_PCIE_ROOT_STATUS        RootStatus;
  PCI_REG_PCIE_DEVICE_CAPABILITY2 DeviceCapability2;
  PCI_REG_PCIE_DEVICE_CONTROL2    DeviceControl2;
  UINT16                          DeviceStatus2;
  PCI_REG_PCIE_LINK_CAPABILITY2   LinkCapability2;
  PCI_REG_PCIE_LINK_CONTROL2      LinkControl2;
  PCI_REG_PCIE_LINK_STATUS2       LinkStatus2;
  UINT32                          SlotCapability2;
  UINT16                          SlotControl2;
  UINT16                          SlotStatus2;
} PCI_CAPABILITY_PCIEXP;

#define EFI_PCIE_CAPABILITY_BASE_OFFSET                             0x100
#define EFI_PCIE_CAPABILITY_ID_SRIOV_CONTROL_ARI_HIERARCHY          0x10
#define EFI_PCIE_CAPABILITY_DEVICE_CAPABILITIES_2_OFFSET            0x24
#define EFI_PCIE_CAPABILITY_DEVICE_CAPABILITIES_2_ARI_FORWARDING    0x20
#define EFI_PCIE_CAPABILITY_DEVICE_CONTROL_2_OFFSET                 0x28
#define EFI_PCIE_CAPABILITY_DEVICE_CONTROL_2_ARI_FORWARDING         0x20

//
// for SR-IOV
//
#define EFI_PCIE_CAPABILITY_ID_ARI        0x0E
#define EFI_PCIE_CAPABILITY_ID_ATS        0x0F
#define EFI_PCIE_CAPABILITY_ID_SRIOV      0x10
#define EFI_PCIE_CAPABILITY_ID_MRIOV      0x11

typedef struct {
  UINT32  CapabilityHeader;
  UINT32  Capability;
  UINT16  Control;
  UINT16  Status;
  UINT16  InitialVFs;
  UINT16  TotalVFs;
  UINT16  NumVFs;
  UINT8   FunctionDependencyLink;
  UINT8   Reserved0;
  UINT16  FirstVFOffset;
  UINT16  VFStride;
  UINT16  Reserved1;
  UINT16  VFDeviceID;
  UINT32  SupportedPageSize;
  UINT32  SystemPageSize;
  UINT32  VFBar[6];
  UINT32  VFMigrationStateArrayOffset;
} SR_IOV_CAPABILITY_REGISTER;

#define EFI_PCIE_CAPABILITY_ID_SRIOV_CAPABILITIES               0x04
#define EFI_PCIE_CAPABILITY_ID_SRIOV_CONTROL                    0x08
#define EFI_PCIE_CAPABILITY_ID_SRIOV_STATUS                     0x0A
#define EFI_PCIE_CAPABILITY_ID_SRIOV_INITIALVFS                 0x0C
#define EFI_PCIE_CAPABILITY_ID_SRIOV_TOTALVFS                   0x0E
#define EFI_PCIE_CAPABILITY_ID_SRIOV_NUMVFS                     0x10
#define EFI_PCIE_CAPABILITY_ID_SRIOV_FUNCTION_DEPENDENCY_LINK   0x12
#define EFI_PCIE_CAPABILITY_ID_SRIOV_FIRSTVF                    0x14
#define EFI_PCIE_CAPABILITY_ID_SRIOV_VFSTRIDE                   0x16
#define EFI_PCIE_CAPABILITY_ID_SRIOV_VFDEVICEID                 0x1A
#define EFI_PCIE_CAPABILITY_ID_SRIOV_SUPPORTED_PAGE_SIZE        0x1C
#define EFI_PCIE_CAPABILITY_ID_SRIOV_SYSTEM_PAGE_SIZE           0x20
#define EFI_PCIE_CAPABILITY_ID_SRIOV_BAR0                       0x24
#define EFI_PCIE_CAPABILITY_ID_SRIOV_BAR1                       0x28
#define EFI_PCIE_CAPABILITY_ID_SRIOV_BAR2                       0x2C
#define EFI_PCIE_CAPABILITY_ID_SRIOV_BAR3                       0x30
#define EFI_PCIE_CAPABILITY_ID_SRIOV_BAR4                       0x34
#define EFI_PCIE_CAPABILITY_ID_SRIOV_BAR5                       0x38
#define EFI_PCIE_CAPABILITY_ID_SRIOV_VF_MIGRATION_STATE         0x3C

typedef struct {
  UINT32 CapabilityId:16;
  UINT32 CapabilityVersion:4;
  UINT32 NextCapabilityOffset:12;
} PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER;

#define PCI_EXP_EXT_HDR PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER

#define PCI_EXPRESS_EXTENDED_CAPABILITY_ADVANCED_ERROR_REPORTING_ID   0x0001
#define PCI_EXPRESS_EXTENDED_CAPABILITY_ADVANCED_ERROR_REPORTING_VER1 0x1
#define PCI_EXPRESS_EXTENDED_CAPABILITY_ADVANCED_ERROR_REPORTING_VER2 0x2

typedef union {
  struct {
    UINT32 Undefined : 1;
    UINT32 Reserved : 3;
    UINT32 DataLinkProtocolError : 1;
    UINT32 SurpriseDownError : 1;
    UINT32 Reserved2 : 6;
    UINT32 PoisonedTlp : 1;
    UINT32 FlowControlProtocolError : 1;
    UINT32 CompletionTimeout : 1;
    UINT32 CompleterAbort : 1;
    UINT32 UnexpectedCompletion : 1;
    UINT32 ReceiverOverflow : 1;
    UINT32 MalformedTlp : 1;
    UINT32 EcrcError : 1;
    UINT32 UnsupportedRequestError : 1;
    UINT32 AcsVoilation : 1;
    UINT32 UncorrectableInternalError : 1;
    UINT32 McBlockedTlp : 1;
    UINT32 AtomicOpEgressBlocked : 1;
    UINT32 TlpPrefixBlockedError : 1;
    UINT32 Reserved3 : 6;
  } Bits;
  UINT32   Uint32;
} PCI_EXPRESS_REG_UNCORRECTABLE_ERROR;

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER  Header;
  PCI_EXPRESS_REG_UNCORRECTABLE_ERROR       UncorrectableErrorStatus;
  PCI_EXPRESS_REG_UNCORRECTABLE_ERROR       UncorrectableErrorMask;
  PCI_EXPRESS_REG_UNCORRECTABLE_ERROR       UncorrectableErrorSeverity;
  UINT32                                    CorrectableErrorStatus;
  UINT32                                    CorrectableErrorMask;
  UINT32                                    AdvancedErrorCapabilitiesAndControl;
  UINT32                                    HeaderLog[4];
  UINT32                                    RootErrorCommand;
  UINT32                                    RootErrorStatus;
  UINT16                                    ErrorSourceIdentification;
  UINT16                                    CorrectableErrorSourceIdentification;
  UINT32                                    TlpPrefixLog[4];
} PCI_EXPRESS_EXTENDED_CAPABILITIES_ADVANCED_ERROR_REPORTING;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_VIRTUAL_CHANNEL_ID    0x0002
#define PCI_EXPRESS_EXTENDED_CAPABILITY_VIRTUAL_CHANNEL_MFVC  0x0009
#define PCI_EXPRESS_EXTENDED_CAPABILITY_VIRTUAL_CHANNEL_VER1  0x1

typedef struct {
  UINT32                                    VcResourceCapability:24;
  UINT32                                    PortArbTableOffset:8;
  UINT32                                    VcResourceControl;
  UINT16                                    Reserved1;
  UINT16                                    VcResourceStatus;
} PCI_EXPRESS_EXTENDED_CAPABILITIES_VIRTUAL_CHANNEL_VC;

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER              Header;
  UINT32                                                ExtendedVcCount:3;
  UINT32                                                PortVcCapability1:29;
  UINT32                                                PortVcCapability2:24;
  UINT32                                                VcArbTableOffset:8;
  UINT16                                                PortVcControl;
  UINT16                                                PortVcStatus;
  PCI_EXPRESS_EXTENDED_CAPABILITIES_VIRTUAL_CHANNEL_VC  Capability[1];
} PCI_EXPRESS_EXTENDED_CAPABILITIES_VIRTUAL_CHANNEL_CAPABILITY;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_SERIAL_NUMBER_ID    0x0003
#define PCI_EXPRESS_EXTENDED_CAPABILITY_SERIAL_NUMBER_VER1  0x1

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER  Header;
  UINT64                                    SerialNumber;
} PCI_EXPRESS_EXTENDED_CAPABILITIES_SERIAL_NUMBER;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_LINK_DECLARATION_ID   0x0005
#define PCI_EXPRESS_EXTENDED_CAPABILITY_LINK_DECLARATION_VER1 0x1

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER  Header;
  UINT32                                    ElementSelfDescription;
  UINT32                                    Reserved;
  UINT32                                    LinkEntry[1];
} PCI_EXPRESS_EXTENDED_CAPABILITIES_LINK_DECLARATION;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_LINK_DECLARATION_GET_LINK_COUNT(LINK_DECLARATION) (UINT8)(((LINK_DECLARATION->ElementSelfDescription)&0x0000ff00)>>8)

#define PCI_EXPRESS_EXTENDED_CAPABILITY_LINK_CONTROL_ID   0x0006
#define PCI_EXPRESS_EXTENDED_CAPABILITY_LINK_CONTROL_VER1 0x1

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER  Header;
  UINT32                                    RootComplexLinkCapabilities;
  UINT16                                    RootComplexLinkControl;
  UINT16                                    RootComplexLinkStatus;
} PCI_EXPRESS_EXTENDED_CAPABILITIES_INTERNAL_LINK_CONTROL;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_POWER_BUDGETING_ID   0x0004
#define PCI_EXPRESS_EXTENDED_CAPABILITY_POWER_BUDGETING_VER1 0x1

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER  Header;
  UINT32                                    DataSelect:8;
  UINT32                                    Reserved:24;
  UINT32                                    Data;
  UINT32                                    PowerBudgetCapability:1;
  UINT32                                    Reserved2:7;
  UINT32                                    Reserved3:24;
} PCI_EXPRESS_EXTENDED_CAPABILITIES_POWER_BUDGETING;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_ACS_EXTENDED_ID   0x000D
#define PCI_EXPRESS_EXTENDED_CAPABILITY_ACS_EXTENDED_VER1 0x1

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER  Header;
  UINT16                                    AcsCapability;
  UINT16                                    AcsControl;
  UINT8                                     EgressControlVectorArray[1];
} PCI_EXPRESS_EXTENDED_CAPABILITIES_ACS_EXTENDED;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_ACS_EXTENDED_GET_EGRES_CONTROL(ACS_EXTENDED) (UINT8)(((ACS_EXTENDED->AcsCapability)&0x00000020))
#define PCI_EXPRESS_EXTENDED_CAPABILITY_ACS_EXTENDED_GET_EGRES_VECTOR_SIZE(ACS_EXTENDED) (UINT8)(((ACS_EXTENDED->AcsCapability)&0x0000FF00))

#define PCI_EXPRESS_EXTENDED_CAPABILITY_EVENT_COLLECTOR_ENDPOINT_ASSOCIATION_ID   0x0007
#define PCI_EXPRESS_EXTENDED_CAPABILITY_EVENT_COLLECTOR_ENDPOINT_ASSOCIATION_VER1 0x1

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER  Header;
  UINT32                                    AssociationBitmap;
} PCI_EXPRESS_EXTENDED_CAPABILITIES_EVENT_COLLECTOR_ENDPOINT_ASSOCIATION;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_MULTI_FUNCTION_VIRTUAL_CHANNEL_ID    0x0008
#define PCI_EXPRESS_EXTENDED_CAPABILITY_MULTI_FUNCTION_VIRTUAL_CHANNEL_VER1  0x1

typedef PCI_EXPRESS_EXTENDED_CAPABILITIES_VIRTUAL_CHANNEL_CAPABILITY PCI_EXPRESS_EXTENDED_CAPABILITIES_MULTI_FUNCTION_VIRTUAL_CHANNEL_CAPABILITY;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_VENDOR_SPECIFIC_ID   0x000B
#define PCI_EXPRESS_EXTENDED_CAPABILITY_VENDOR_SPECIFIC_VER1 0x1

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER  Header;
  UINT32                                    VendorSpecificHeader;
  UINT8                                     VendorSpecific[1];
} PCI_EXPRESS_EXTENDED_CAPABILITIES_VENDOR_SPECIFIC;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_VENDOR_SPECIFIC_GET_SIZE(VENDOR) (UINT16)(((VENDOR->VendorSpecificHeader)&0xFFF00000)>>20)

#define PCI_EXPRESS_EXTENDED_CAPABILITY_RCRB_HEADER_ID   0x000A
#define PCI_EXPRESS_EXTENDED_CAPABILITY_RCRB_HEADER_VER1 0x1

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER  Header;
  UINT16                                    VendorId;
  UINT16                                    DeviceId;
  UINT32                                    RcrbCapabilities;
  UINT32                                    RcrbControl;
  UINT32                                    Reserved;
} PCI_EXPRESS_EXTENDED_CAPABILITIES_RCRB_HEADER;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_MULTICAST_ID   0x0012
#define PCI_EXPRESS_EXTENDED_CAPABILITY_MULTICAST_VER1 0x1

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER  Header;
  UINT16                                    MultiCastCapability;
  UINT16                                    MulticastControl;
  UINT64                                    McBaseAddress;
  UINT64                                    McReceiveAddress;
  UINT64                                    McBlockAll;
  UINT64                                    McBlockUntranslated;
  UINT64                                    McOverlayBar;
} PCI_EXPRESS_EXTENDED_CAPABILITIES_MULTICAST;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_RESIZABLE_BAR_ID    0x0015
#define PCI_EXPRESS_EXTENDED_CAPABILITY_RESIZABLE_BAR_VER1  0x1

typedef struct {
  UINT32                                                 ResizableBarCapability;
  UINT16                                                 ResizableBarControl;
  UINT16                                                 Reserved;
} PCI_EXPRESS_EXTENDED_CAPABILITIES_RESIZABLE_BAR_ENTRY;

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER               Header;
  PCI_EXPRESS_EXTENDED_CAPABILITIES_RESIZABLE_BAR_ENTRY  Capability[1];
} PCI_EXPRESS_EXTENDED_CAPABILITIES_RESIZABLE_BAR;

#define GET_NUMBER_RESIZABLE_BARS(x) (((x->Capability[0].ResizableBarControl) & 0xE0) >> 5)

#define PCI_EXPRESS_EXTENDED_CAPABILITY_ARI_CAPABILITY_ID    0x000E
#define PCI_EXPRESS_EXTENDED_CAPABILITY_ARI_CAPABILITY_VER1  0x1

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER                Header;
  UINT16                                                  AriCapability;
  UINT16                                                  AriControl;
} PCI_EXPRESS_EXTENDED_CAPABILITIES_ARI_CAPABILITY;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_DYNAMIC_POWER_ALLOCATION_ID    0x0016
#define PCI_EXPRESS_EXTENDED_CAPABILITY_DYNAMIC_POWER_ALLOCATION_VER1  0x1

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER                Header;
  UINT32                                                  DpaCapability;
  UINT32                                                  DpaLatencyIndicator;
  UINT16                                                  DpaStatus;
  UINT16                                                  DpaControl;
  UINT8                                                   DpaPowerAllocationArray[1];
} PCI_EXPRESS_EXTENDED_CAPABILITIES_DYNAMIC_POWER_ALLOCATION;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_DYNAMIC_POWER_ALLOCATION_GET_SUBSTATE_MAX(POWER) (UINT16)(((POWER->DpaCapability)&0x0000000F))


#define PCI_EXPRESS_EXTENDED_CAPABILITY_LATENCE_TOLERANCE_REPORTING_ID    0x0018
#define PCI_EXPRESS_EXTENDED_CAPABILITY_LATENCE_TOLERANCE_REPORTING_VER1  0x1

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER                Header;
  UINT16                                                  MaxSnoopLatency;
  UINT16                                                  MaxNoSnoopLatency;
} PCI_EXPRESS_EXTENDED_CAPABILITIES_LATENCE_TOLERANCE_REPORTING;

#define PCI_EXPRESS_EXTENDED_CAPABILITY_TPH_ID    0x0017
#define PCI_EXPRESS_EXTENDED_CAPABILITY_TPH_VER1  0x1

typedef struct {
  PCI_EXPRESS_EXTENDED_CAPABILITIES_HEADER                Header;
  UINT32                                                  TphRequesterCapability;
  UINT32                                                  TphRequesterControl;
  UINT16                                                  TphStTable[1];
} PCI_EXPRESS_EXTENDED_CAPABILITIES_TPH;

#define GET_TPH_TABLE_SIZE(x) ((x->TphRequesterCapability & 0x7FF0000)>>16) * sizeof(UINT16)

#pragma pack()

#endif
