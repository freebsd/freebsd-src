/** @file
  Platform TPM Profile Specification definition for TPM2.0.
  It covers both FIFO and CRB interface.

Copyright (c) 2016 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _TPM_PTP_H_
#define _TPM_PTP_H_

//
// PTP FIFO definition
//

//
// Set structure alignment to 1-byte
//
#pragma pack (1)

//
// Register set map as specified in PTP specification Chapter 5
//
typedef struct {
  ///
  /// Used to gain ownership for this particular port.
  ///
  UINT8                             Access;             // 0
  UINT8                             Reserved1[7];       // 1
  ///
  /// Controls interrupts.
  ///
  UINT32                            IntEnable;          // 8
  ///
  /// SIRQ vector to be used by the TPM.
  ///
  UINT8                             IntVector;          // 0ch
  UINT8                             Reserved2[3];       // 0dh
  ///
  /// What caused interrupt.
  ///
  UINT32                            IntSts;             // 10h
  ///
  /// Shows which interrupts are supported by that particular TPM.
  ///
  UINT32                            InterfaceCapability;// 14h
  ///
  /// Status Register. Provides status of the TPM.
  ///
  UINT8                             Status;             // 18h
  ///
  /// Number of consecutive writes that can be done to the TPM.
  ///
  UINT16                            BurstCount;         // 19h
  ///
  /// Additional Status Register.
  ///
  UINT8                             StatusEx;           // 1Bh
  UINT8                             Reserved3[8];
  ///
  /// Read or write FIFO, depending on transaction.
  ///
  UINT32                            DataFifo;           // 24h
  UINT8                             Reserved4[8];       // 28h
  ///
  /// Used to identify the Interface types supported by the TPM.
  ///
  UINT32                            InterfaceId;        // 30h
  UINT8                             Reserved5[0x4c];    // 34h
  ///
  /// Extended ReadFIFO or WriteFIFO, depending on the current bus cycle (read or write)
  ///
  UINT32                            XDataFifo;          // 80h
  UINT8                             Reserved6[0xe7c];   // 84h
  ///
  /// Vendor ID
  ///
  UINT16                            Vid;                // 0f00h
  ///
  /// Device ID
  ///
  UINT16                            Did;                // 0f02h
  ///
  /// Revision ID
  ///
  UINT8                             Rid;                // 0f04h
  UINT8                             Reserved[0xfb];     // 0f05h
} PTP_FIFO_REGISTERS;

//
// Restore original structure alignment
//
#pragma pack ()

//
// Define pointer types used to access TIS registers on PC
//
typedef PTP_FIFO_REGISTERS  *PTP_FIFO_REGISTERS_PTR;

//
// Define bits of FIFO Interface Identifier Register
//
typedef union {
  struct {
    UINT32   InterfaceType:4;
    UINT32   InterfaceVersion:4;
    UINT32   CapLocality:1;
    UINT32   Reserved1:2;
    UINT32   CapDataXferSizeSupport:2;
    UINT32   CapFIFO:1;
    UINT32   CapCRB:1;
    UINT32   CapIFRes:2;
    UINT32   InterfaceSelector:2;
    UINT32   IntfSelLock:1;
    UINT32   Reserved2:4;
    UINT32   Reserved3:8;
  } Bits;
  UINT32   Uint32;
} PTP_FIFO_INTERFACE_IDENTIFIER;

//
// Define bits of FIFO Interface Capability Register
//
typedef union {
  struct {
    UINT32   DataAvailIntSupport:1;
    UINT32   StsValidIntSupport:1;
    UINT32   LocalityChangeIntSupport:1;
    UINT32   InterruptLevelHigh:1;
    UINT32   InterruptLevelLow:1;
    UINT32   InterruptEdgeRising:1;
    UINT32   InterruptEdgeFalling:1;
    UINT32   CommandReadyIntSupport:1;
    UINT32   BurstCountStatic:1;
    UINT32   DataTransferSizeSupport:2;
    UINT32   Reserved:17;
    UINT32   InterfaceVersion:3;
    UINT32   Reserved2:1;
  } Bits;
  UINT32   Uint32;
} PTP_FIFO_INTERFACE_CAPABILITY;

///
/// InterfaceVersion
///
#define INTERFACE_CAPABILITY_INTERFACE_VERSION_TIS_12  0x0
#define INTERFACE_CAPABILITY_INTERFACE_VERSION_TIS_13  0x2
#define INTERFACE_CAPABILITY_INTERFACE_VERSION_PTP     0x3


//
// Define bits of ACCESS and STATUS registers
//

///
/// This bit is a 1 to indicate that the other bits in this register are valid.
///
#define PTP_FIFO_VALID                BIT7
///
/// Indicate that this locality is active.
///
#define PTP_FIFO_ACC_ACTIVE           BIT5
///
/// Set to 1 to indicate that this locality had the TPM taken away while
/// this locality had the TIS_PC_ACC_ACTIVE bit set.
///
#define PTP_FIFO_ACC_SEIZED           BIT4
///
/// Set to 1 to indicate that TPM MUST reset the
/// TIS_PC_ACC_ACTIVE bit and remove ownership for localities less than the
/// locality that is writing this bit.
///
#define PTP_FIFO_ACC_SEIZE            BIT3
///
/// When this bit is 1, another locality is requesting usage of the TPM.
///
#define PTP_FIFO_ACC_PENDIND          BIT2
///
/// Set to 1 to indicate that this locality is requesting to use TPM.
///
#define PTP_FIFO_ACC_RQUUSE           BIT1
///
/// A value of 1 indicates that a T/OS has not been established on the platform
///
#define PTP_FIFO_ACC_ESTABLISH        BIT0

///
/// This field indicates that STS_DATA and STS_EXPECT are valid
///
#define PTP_FIFO_STS_VALID            BIT7
///
/// When this bit is 1, TPM is in the Ready state,
/// indicating it is ready to receive a new command.
///
#define PTP_FIFO_STS_READY            BIT6
///
/// Write a 1 to this bit to cause the TPM to execute that command.
///
#define PTP_FIFO_STS_GO               BIT5
///
/// This bit indicates that the TPM has data available as a response.
///
#define PTP_FIFO_STS_DATA             BIT4
///
/// The TPM sets this bit to a value of 1 when it expects another byte of data for a command.
///
#define PTP_FIFO_STS_EXPECT           BIT3
///
/// Indicates that the TPM has completed all self-test actions following a TPM_ContinueSelfTest command.
///
#define PTP_FIFO_STS_SELFTEST_DONE    BIT2
///
/// Writes a 1 to this bit to force the TPM to re-send the response.
///
#define PTP_FIFO_STS_RETRY            BIT1

///
/// TPM Family Identifier.
/// 00: TPM 1.2 Family
/// 01: TPM 2.0 Family
///
#define PTP_FIFO_STS_EX_TPM_FAMILY    (BIT2 | BIT3)
#define PTP_FIFO_STS_EX_TPM_FAMILY_OFFSET    (2)
#define PTP_FIFO_STS_EX_TPM_FAMILY_TPM12    (0)
#define PTP_FIFO_STS_EX_TPM_FAMILY_TPM20    (BIT2)
///
/// A write of 1 after tpmGo and before dataAvail aborts the currently executing command, resulting in a response of TPM_RC_CANCELLED.
/// A write of 1 after dataAvail and before tpmGo is ignored by the TPM.
///
#define PTP_FIFO_STS_EX_CANCEL        BIT0


//
// PTP CRB definition
//

//
// Set structure alignment to 1-byte
//
#pragma pack (1)

//
// Register set map as specified in PTP specification Chapter 5
//
typedef struct {
  ///
  /// Used to determine current state of Locality of the TPM.
  ///
  UINT32                            LocalityState;             // 0
  UINT8                             Reserved1[4];              // 4
  ///
  /// Used to gain control of the TPM by this Locality.
  ///
  UINT32                            LocalityControl;           // 8
  ///
  /// Used to determine whether Locality has been granted or Seized.
  ///
  UINT32                            LocalityStatus;            // 0ch
  UINT8                             Reserved2[0x20];           // 10h
  ///
  /// Used to identify the Interface types supported by the TPM.
  ///
  UINT32                            InterfaceId;               // 30h
  ///
  /// Vendor ID
  ///
  UINT16                            Vid;                       // 34h
  ///
  /// Device ID
  ///
  UINT16                            Did;                       // 36h
  ///
  /// Optional Register used in low memory environments prior to CRB_DATA_BUFFER availability.
  ///
  UINT64                            CrbControlExtension;       // 38h
  ///
  /// Register used to initiate transactions for the CRB interface.
  ///
  UINT32                            CrbControlRequest;         // 40h
  ///
  /// Register used by the TPM to provide status of the CRB interface.
  ///
  UINT32                            CrbControlStatus;          // 44h
  ///
  /// Register used by software to cancel command processing.
  ///
  UINT32                            CrbControlCancel;          // 48h
  ///
  /// Register used to indicate presence of command or response data in the CRB buffer.
  ///
  UINT32                            CrbControlStart;           // 4Ch
  ///
  /// Register used to configure and respond to interrupts.
  ///
  UINT32                            CrbInterruptEnable;        // 50h
  UINT32                            CrbInterruptStatus;        // 54h
  ///
  /// Size of the Command buffer.
  ///
  UINT32                            CrbControlCommandSize;     // 58h
  ///
  /// Command buffer start address
  ///
  UINT32                            CrbControlCommandAddressLow;   // 5Ch
  UINT32                            CrbControlCommandAddressHigh;  // 60h
  ///
  /// Size of the Response buffer
  ///
  UINT32                            CrbControlResponseSize;    // 64h
  ///
  /// Address of the start of the Response buffer
  ///
  UINT64                            CrbControlResponseAddrss;  // 68h
  UINT8                             Reserved4[0x10];           // 70h
  ///
  /// Command/Response Data may be defined as large as 3968 (0xF80).
  ///
  UINT8                             CrbDataBuffer[0xF80];      // 80h
} PTP_CRB_REGISTERS;

//
// Define pointer types used to access CRB registers on PTP
//
typedef PTP_CRB_REGISTERS  *PTP_CRB_REGISTERS_PTR;

//
// Define bits of CRB Interface Identifier Register
//
typedef union {
  struct {
    UINT32   InterfaceType:4;
    UINT32   InterfaceVersion:4;
    UINT32   CapLocality:1;
    UINT32   CapCRBIdleBypass:1;
    UINT32   Reserved1:1;
    UINT32   CapDataXferSizeSupport:2;
    UINT32   CapFIFO:1;
    UINT32   CapCRB:1;
    UINT32   CapIFRes:2;
    UINT32   InterfaceSelector:2;
    UINT32   IntfSelLock:1;
    UINT32   Reserved2:4;
    UINT32   Rid:8;
  } Bits;
  UINT32   Uint32;
} PTP_CRB_INTERFACE_IDENTIFIER;

///
/// InterfaceType
///
#define PTP_INTERFACE_IDENTIFIER_INTERFACE_TYPE_FIFO  0x0
#define PTP_INTERFACE_IDENTIFIER_INTERFACE_TYPE_CRB   0x1
#define PTP_INTERFACE_IDENTIFIER_INTERFACE_TYPE_TIS   0xF

///
/// InterfaceVersion
///
#define PTP_INTERFACE_IDENTIFIER_INTERFACE_VERSION_FIFO  0x0
#define PTP_INTERFACE_IDENTIFIER_INTERFACE_VERSION_CRB   0x1

///
/// InterfaceSelector
///
#define PTP_INTERFACE_IDENTIFIER_INTERFACE_SELECTOR_FIFO  0x0
#define PTP_INTERFACE_IDENTIFIER_INTERFACE_SELECTOR_CRB   0x1

//
// Define bits of Locality State Register
//

///
/// This bit indicates whether all other bits of this register contain valid values, if it is a 1.
///
#define PTP_CRB_LOCALITY_STATE_TPM_REG_VALID_STATUS       BIT7

///
/// 000 - Locality 0
/// 001 - Locality 1
/// 010 - Locality 2
/// 011 - Locality 3
/// 100 - Locality 4
///
#define PTP_CRB_LOCALITY_STATE_ACTIVE_LOCALITY_MASK       (BIT2 | BIT3 | BIT4)
#define PTP_CRB_LOCALITY_STATE_ACTIVE_LOCALITY_0          (0)
#define PTP_CRB_LOCALITY_STATE_ACTIVE_LOCALITY_1          (BIT2)
#define PTP_CRB_LOCALITY_STATE_ACTIVE_LOCALITY_2          (BIT3)
#define PTP_CRB_LOCALITY_STATE_ACTIVE_LOCALITY_3          (BIT2 | BIT3)
#define PTP_CRB_LOCALITY_STATE_ACTIVE_LOCALITY_4          (BIT4)

///
/// A 0 indicates to the host that no locality is assigned.
/// A 1 indicates a locality has been assigned.
///
#define PTP_CRB_LOCALITY_STATE_LOCALITY_ASSIGNED          BIT1

///
/// The TPM clears this bit to 0 upon receipt of _TPM_Hash_End
/// The TPM sets this bit to a 1 when the TPM_LOC_CTRL_x.resetEstablishment field is set to 1.
///
#define PTP_CRB_LOCALITY_STATE_TPM_ESTABLISHED            BIT0

//
// Define bits of Locality Control Register
//

///
/// Writes (1): Reset TPM_LOC_STATE_x.tpmEstablished bit if the write occurs from Locality 3 or 4.
///
#define PTP_CRB_LOCALITY_CONTROL_RESET_ESTABLISHMENT_BIT  BIT3

///
/// Writes (1): The TPM gives control of the TPM to the locality setting this bit if it is the higher priority locality.
///
#define PTP_CRB_LOCALITY_CONTROL_SEIZE                    BIT2

///
/// Writes (1): The active Locality is done with the TPM.
///
#define PTP_CRB_LOCALITY_CONTROL_RELINQUISH               BIT1

///
/// Writes (1): Interrupt the TPM and generate a locality arbitration algorithm.
///
#define PTP_CRB_LOCALITY_CONTROL_REQUEST_ACCESS           BIT0

//
// Define bits of Locality Status Register
//

///
/// 0: A higher locality has not initiated a Seize arbitration process.
/// 1: A higher locality has Seized the TPM from this locality.
///
#define PTP_CRB_LOCALITY_STATUS_BEEN_SEIZED               BIT1

///
/// 0: Locality has not been granted to the TPM.
/// 1: Locality has been granted access to the TPM
///
#define PTP_CRB_LOCALITY_STATUS_GRANTED                   BIT0

//
// Define bits of CRB Control Area Request Register
//

///
/// Used by Software to indicate transition the TPM to and from the Idle state
/// 1: Set by Software to indicate response has been read from the response buffer and TPM can transition to Idle
/// 0: Cleared to 0 by TPM to acknowledge the request when TPM enters Idle state.
/// TPM SHALL complete this transition within TIMEOUT_C.
///
#define PTP_CRB_CONTROL_AREA_REQUEST_GO_IDLE              BIT1

///
/// Used by Software to request the TPM transition to the Ready State.
/// 1: Set to 1 by Software to indicate the TPM should be ready to receive a command.
/// 0: Cleared to 0 by TPM to acknowledge the request.
/// TPM SHALL complete this transition within TIMEOUT_C.
///
#define PTP_CRB_CONTROL_AREA_REQUEST_COMMAND_READY        BIT0

//
// Define bits of CRB Control Area Status Register
//

///
/// Used by TPM to indicate it is in the Idle State
/// 1: Set by TPM when in the Idle State
/// 0: Cleared by TPM on receipt of TPM_CRB_CTRL_REQ_x.cmdReady when TPM transitions to the Ready State.
/// SHALL be cleared by TIMEOUT_C.
///
#define PTP_CRB_CONTROL_AREA_STATUS_TPM_IDLE              BIT1

///
/// Used by the TPM to indicate current status.
/// 1: Set by TPM to indicate a FATAL Error
/// 0: Indicates TPM is operational
///
#define PTP_CRB_CONTROL_AREA_STATUS_TPM_STATUS            BIT0

//
// Define bits of CRB Control Cancel Register
//

///
/// Used by software to cancel command processing Reads return correct value
/// Writes (0000 0001h): Cancel a command
/// Writes (0000 0000h): Clears field when command has been cancelled
///
#define PTP_CRB_CONTROL_CANCEL                            BIT0

//
// Define bits of CRB Control Start Register
//

///
/// When set by software, indicates a command is ready for processing.
/// Writes (0000 0001h): TPM transitions to Command Execution
/// Writes (0000 0000h): TPM clears this field and transitions to Command Completion
///
#define PTP_CRB_CONTROL_START                             BIT0

//
// Restore original structure alignment
//
#pragma pack ()

//
// Default TimeOut value
//
#define PTP_TIMEOUT_A               (750 * 1000)   // 750ms
#define PTP_TIMEOUT_B               (2000 * 1000)  // 2s
#define PTP_TIMEOUT_C               (200 * 1000)   // 200ms
#define PTP_TIMEOUT_D               (30 * 1000)    // 30ms

#endif
