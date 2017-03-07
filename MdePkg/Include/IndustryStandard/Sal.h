/** @file
  Main SAL API's defined in Intel Itanium Processor Family System Abstraction
  Layer Specification Revision 3.2 (December 2003)

Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available under 
the terms and conditions of the BSD License that accompanies this distribution.  
The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.                                          
    
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef __SAL_API_H__
#define __SAL_API_H__

///
/// SAL return status type 
///
typedef INTN EFI_SAL_STATUS;

///
/// Call completed without error. 
///
#define EFI_SAL_SUCCESS               ((EFI_SAL_STATUS) 0)
///
/// Call completed without error, but some information was lost due to overflow. 
///
#define EFI_SAL_OVERFLOW              ((EFI_SAL_STATUS) 1)
///
/// Call completed without error; effect a warm boot of the system to complete the update.
///
#define EFI_SAL_WARM_BOOT_NEEDED      ((EFI_SAL_STATUS) 2)
///
/// More information is available for retrieval. 
///
#define EFI_SAL_MORE_RECORDS          ((EFI_SAL_STATUS) 3)
///
/// Not implemented.
///
#define EFI_SAL_NOT_IMPLEMENTED       ((EFI_SAL_STATUS) - 1)
///
/// Invalid Argument.
///
#define EFI_SAL_INVALID_ARGUMENT      ((EFI_SAL_STATUS) - 2)
///
/// Call completed without error. 
///
#define EFI_SAL_ERROR                 ((EFI_SAL_STATUS) - 3)
///
/// Virtual address not registered. 
///
#define EFI_SAL_VIRTUAL_ADDRESS_ERROR ((EFI_SAL_STATUS) - 4)
///
/// No information available. 
///
#define EFI_SAL_NO_INFORMATION        ((EFI_SAL_STATUS) - 5)
///
/// Scratch buffer required.
///
#define EFI_SAL_NOT_ENOUGH_SCRATCH    ((EFI_SAL_STATUS) - 9)

///
/// Return registers from SAL.
///
typedef struct {
  ///
  /// SAL return status value in r8.
  ///
  EFI_SAL_STATUS  Status;
  ///
  /// SAL returned value in r9.
  ///
  UINTN           r9;
  ///
  /// SAL returned value in r10.
  ///
  UINTN           r10;
  ///
  /// SAL returned value in r11.
  ///
  UINTN           r11;
} SAL_RETURN_REGS;

/**
  Prototype of SAL procedures.

  @param  FunctionId         Functional identifier.
                             The upper 32 bits are ignored and only the lower 32 bits
                             are used. The following functional identifiers are defined:
                             0x01XXXXXX - Architected SAL functional group.
                             0x02XXXXXX to 0x03XXXXXX - OEM SAL functional group. Each OEM is
                             allowed to use the entire range in the 0x02XXXXXX to 0x03XXXXXX range.
                             0x04XXXXXX to 0xFFFFFFFF - Reserved.
  @param  Arg1               The first parameter of the architected/OEM specific SAL functions.
  @param  Arg2               The second parameter of the architected/OEM specific SAL functions.
  @param  Arg3               The third parameter passed to the ESAL function based.
  @param  Arg4               The fourth parameter passed to the ESAL function based.
  @param  Arg5               The fifth parameter passed to the ESAL function based.
  @param  Arg6               The sixth parameter passed to the ESAL function.
  @param  Arg7               The seventh parameter passed to the ESAL function based.

  @return r8                 Return status: positive number indicates successful,
                             negative number indicates failure.
          r9                 Other return parameter in r9.
          r10                Other return parameter in r10.
          r11                Other return parameter in r11.

**/
typedef
SAL_RETURN_REGS
(EFIAPI *SAL_PROC)(
  IN UINT64 FunctionId,
  IN UINT64 Arg1,
  IN UINT64 Arg2,
  IN UINT64 Arg3,
  IN UINT64 Arg4,
  IN UINT64 Arg5,
  IN UINT64 Arg6,
  IN UINT64 Arg7
  );

//
// SAL Procedure FunctionId definition
//

///
/// Register software code locations with SAL.
///
#define EFI_SAL_SET_VECTORS             0x01000000
///
/// Return Machine State information obtained by SAL.
///
#define EFI_SAL_GET_STATE_INFO          0x01000001
///
/// Obtain size of Machine State information.
///
#define EFI_SAL_GET_STATE_INFO_SIZE     0x01000002
///
/// Clear Machine State information.
///
#define EFI_SAL_CLEAR_STATE_INFO        0x01000003
///
/// Cause the processor to go into a spin loop within SAL.
///
#define EFI_SAL_MC_RENDEZ               0x01000004
///
/// Register the machine check interface layer with SAL.
///
#define EFI_SAL_MC_SET_PARAMS           0x01000005
///
/// Register the physical addresses of locations needed by SAL.
///
#define EFI_SAL_REGISTER_PHYSICAL_ADDR  0x01000006
///
/// Flush the instruction or data caches.
///
#define EFI_SAL_CACHE_FLUSH             0x01000008
///
/// Initialize the instruction and data caches.
///
#define EFI_SAL_CACHE_INIT              0x01000009
///
/// Read from the PCI configuration space.
///
#define EFI_SAL_PCI_CONFIG_READ         0x01000010
///
/// Write to the PCI configuration space.
///
#define EFI_SAL_PCI_CONFIG_WRITE        0x01000011
///
/// Return the base frequency of the platform.
///
#define EFI_SAL_FREQ_BASE               0x01000012
///
/// Returns information on the physical processor mapping within the platform.
///
#define EFI_SAL_PHYSICAL_ID_INFO        0x01000013
///
/// Update the contents of firmware blocks.
///
#define EFI_SAL_UPDATE_PAL              0x01000020

#define EFI_SAL_FUNCTION_ID_MASK        0x0000ffff
#define EFI_SAL_MAX_SAL_FUNCTION_ID     0x00000021

//
// SAL Procedure parameter definitions
// Not much point in using typedefs or enums because all params
// are UINT64 and the entry point is common
//

//
// Parameter of EFI_SAL_SET_VECTORS
//
// Vector type
//
#define EFI_SAL_SET_MCA_VECTOR          0x0
#define EFI_SAL_SET_INIT_VECTOR         0x1
#define EFI_SAL_SET_BOOT_RENDEZ_VECTOR  0x2
///
/// The format of a length_cs_n argument.
///
typedef struct {
  UINT64  Length : 32;
  UINT64  ChecksumValid : 1;
  UINT64  Reserved1 : 7;
  UINT64  ByteChecksum : 8;
  UINT64  Reserved2 : 16;
} SAL_SET_VECTORS_CS_N;

//
// Parameter of EFI_SAL_GET_STATE_INFO, EFI_SAL_GET_STATE_INFO_SIZE, and EFI_SAL_CLEAR_STATE_INFO
// 
// Type of information
//
#define EFI_SAL_MCA_STATE_INFO  0x0
#define EFI_SAL_INIT_STATE_INFO 0x1
#define EFI_SAL_CMC_STATE_INFO  0x2
#define EFI_SAL_CP_STATE_INFO   0x3

//
// Parameter of EFI_SAL_MC_SET_PARAMS
//
// Unsigned 64-bit integer value for the parameter type of the machine check interface
//
#define EFI_SAL_MC_SET_RENDEZ_PARAM 0x1
#define EFI_SAL_MC_SET_WAKEUP_PARAM 0x2
#define EFI_SAL_MC_SET_CPE_PARAM    0x3
//
// Unsigned 64-bit integer value indicating whether interrupt vector or
// memory address is specified
//
#define EFI_SAL_MC_SET_INTR_PARAM   0x1
#define EFI_SAL_MC_SET_MEM_PARAM    0x2

//
// Parameter of EFI_SAL_REGISTER_PAL_PHYSICAL_ADDR
//
// The encoded value of the entity whose physical address is registered
//
#define EFI_SAL_REGISTER_PAL_ADDR 0x0

//
// Parameter of EFI_SAL_CACHE_FLUSH
//
// Unsigned 64-bit integer denoting type of cache flush operation
//
#define EFI_SAL_FLUSH_I_CACHE       0x01
#define EFI_SAL_FLUSH_D_CACHE       0x02
#define EFI_SAL_FLUSH_BOTH_CACHE    0x03
#define EFI_SAL_FLUSH_MAKE_COHERENT 0x04

//
// Parameter of EFI_SAL_PCI_CONFIG_READ and EFI_SAL_PCI_CONFIG_WRITE
//
// PCI config size
//
#define EFI_SAL_PCI_CONFIG_ONE_BYTE   0x1
#define EFI_SAL_PCI_CONFIG_TWO_BYTES  0x2
#define EFI_SAL_PCI_CONFIG_FOUR_BYTES 0x4
//
// The type of PCI configuration address
//
#define EFI_SAL_PCI_COMPATIBLE_ADDRESS         0x0
#define EFI_SAL_PCI_EXTENDED_REGISTER_ADDRESS  0x1
///
/// The format of PCI Compatible Address.
///
typedef struct {
  UINT64  Register : 8;
  UINT64  Function : 3;
  UINT64  Device : 5;
  UINT64  Bus : 8;
  UINT64  Segment : 8;
  UINT64  Reserved : 32;
} SAL_PCI_ADDRESS;
///
/// The format of Extended Register Address.
///
typedef struct {
  UINT64  Register : 8;
  UINT64  ExtendedRegister : 4;
  UINT64  Function : 3;
  UINT64  Device : 5;
  UINT64  Bus : 8;
  UINT64  Segment : 16;
  UINT64  Reserved : 20;
} SAL_PCI_EXTENDED_REGISTER_ADDRESS;

//
// Parameter of EFI_SAL_FREQ_BASE
//
// Unsigned 64-bit integer specifying the type of clock source
//
#define EFI_SAL_CPU_INPUT_FREQ_BASE     0x0
#define EFI_SAL_PLATFORM_IT_FREQ_BASE   0x1
#define EFI_SAL_PLATFORM_RTC_FREQ_BASE  0x2

//
// Parameter and return value of EFI_SAL_UPDATE_PAL
//
// Return parameter provides additional information on the
// failure when the status field contains a value of -3,
// returned in r9.
//
#define EFI_SAL_UPDATE_BAD_PAL_VERSION  ((UINT64) -1)
#define EFI_SAL_UPDATE_PAL_AUTH_FAIL    ((UINT64) -2)
#define EFI_SAL_UPDATE_PAL_BAD_TYPE     ((UINT64) -3)
#define EFI_SAL_UPDATE_PAL_READONLY     ((UINT64) -4)
#define EFI_SAL_UPDATE_PAL_WRITE_FAIL   ((UINT64) -10)
#define EFI_SAL_UPDATE_PAL_ERASE_FAIL   ((UINT64) -11)
#define EFI_SAL_UPDATE_PAL_READ_FAIL    ((UINT64) -12)
#define EFI_SAL_UPDATE_PAL_CANT_FIT     ((UINT64) -13)
///
/// 64-byte header of update data block.
///
typedef struct {
  UINT32  Size;
  UINT32  MmddyyyyDate;
  UINT16  Version;
  UINT8   Type;
  UINT8   Reserved[5];
  UINT64  FwVendorId;
  UINT8   Reserved2[40];
} SAL_UPDATE_PAL_DATA_BLOCK;
///
/// Data structure pointed by the parameter param_buf.
/// It is a 16-byte aligned data structure in memory with a length of 32 bytes
/// that describes the new firmware. This information is organized in the form
/// of a linked list with each element describing one firmware component.
///
typedef struct _SAL_UPDATE_PAL_INFO_BLOCK {
  struct _SAL_UPDATE_PAL_INFO_BLOCK *Next;
  struct SAL_UPDATE_PAL_DATA_BLOCK  *DataBlock;
  UINT8                             StoreChecksum;
  UINT8                             Reserved[15];
} SAL_UPDATE_PAL_INFO_BLOCK;

///
/// SAL System Table Definitions.
///
#pragma pack(1)
typedef struct {
  ///
  /// The ASCII string representation of "SST_" that confirms the presence of the table. 
  /// 
  UINT32  Signature;
  ///
  /// The length of the entire table in bytes, starting from offset zero and including the
  /// header and all entries indicated by the EntryCount field.
  ///
  UINT32  Length;
  ///
  /// The revision number of the Itanium Processor Family System Abstraction Layer
  /// Specification supported by the SAL implementation, in binary coded decimal (BCD) format.
  ///
  UINT16  SalRevision;
  ///
  /// The number of entries in the variable portion of the table.
  ///
  UINT16  EntryCount;
  ///
  /// A modulo checksum of the entire table and the entries following this table.
  ///
  UINT8   CheckSum;
  ///
  /// Unused, must be zero.
  ///
  UINT8   Reserved[7];
  ///
  /// Version Number of the SAL_A firmware implementation in BCD format.
  ///
  UINT16  SalAVersion;
  ///
  /// Version Number of the SAL_B firmware implementation in BCD format.
  ///
  UINT16  SalBVersion;
  ///
  /// An ASCII identification string which uniquely identifies the manufacturer
  /// of the system hardware.
  ///
  UINT8   OemId[32];
  ///
  /// An ASCII identification string which uniquely identifies a family of
  /// compatible products from the manufacturer.
  ///
  UINT8   ProductId[32];
  ///
  /// Unused, must be zero.
  ///
  UINT8   Reserved2[8];
} SAL_SYSTEM_TABLE_HEADER;

#define EFI_SAL_ST_HEADER_SIGNATURE "SST_"
#define EFI_SAL_REVISION            0x0320
//
// SAL System Types
//
#define EFI_SAL_ST_ENTRY_POINT        0
#define EFI_SAL_ST_MEMORY_DESCRIPTOR  1
#define EFI_SAL_ST_PLATFORM_FEATURES  2
#define EFI_SAL_ST_TR_USAGE           3
#define EFI_SAL_ST_PTC                4
#define EFI_SAL_ST_AP_WAKEUP          5

//
// SAL System Type Sizes
//
#define EFI_SAL_ST_ENTRY_POINT_SIZE        48
#define EFI_SAL_ST_MEMORY_DESCRIPTOR_SIZE  32
#define EFI_SAL_ST_PLATFORM_FEATURES_SIZE  16
#define EFI_SAL_ST_TR_USAGE_SIZE           32
#define EFI_SAL_ST_PTC_SIZE                16
#define EFI_SAL_ST_AP_WAKEUP_SIZE          16

///
/// Format of Entrypoint Descriptor Entry.
///
typedef struct {
  UINT8   Type;         ///< Type here should be 0.
  UINT8   Reserved[7];
  UINT64  PalProcEntry;
  UINT64  SalProcEntry;
  UINT64  SalGlobalDataPointer;
  UINT64  Reserved2[2];
} SAL_ST_ENTRY_POINT_DESCRIPTOR;

///
/// Format of Platform Features Descriptor Entry.
///
typedef struct {
  UINT8 Type;           ///< Type here should be 2.
  UINT8 PlatformFeatures;
  UINT8 Reserved[14];
} SAL_ST_PLATFORM_FEATURES;

//
// Value of Platform Feature List
//
#define SAL_PLAT_FEAT_BUS_LOCK      0x01
#define SAL_PLAT_FEAT_PLAT_IPI_HINT 0x02
#define SAL_PLAT_FEAT_PROC_IPI_HINT 0x04

///
/// Format of Translation Register Descriptor Entry.
///
typedef struct {
  UINT8   Type;         ///< Type here should be 3.
  UINT8   TRType;
  UINT8   TRNumber;
  UINT8   Reserved[5];
  UINT64  VirtualAddress;
  UINT64  EncodedPageSize;
  UINT64  Reserved1;
} SAL_ST_TR_DECRIPTOR;

//
// Type of Translation Register
//
#define EFI_SAL_ST_TR_USAGE_INSTRUCTION 00
#define EFI_SAL_ST_TR_USAGE_DATA        01

///
/// Definition of Coherence Domain Information.
///
typedef struct {
  UINT64  NumberOfProcessors;
  UINT64  LocalIDRegister;
} SAL_COHERENCE_DOMAIN_INFO;
           
///
/// Format of Purge Translation Cache Coherence Domain Entry.
///
typedef struct {
  UINT8                     Type;       ///< Type here should be 4.
  UINT8                     Reserved[3];
  UINT32                    NumberOfDomains;
  SAL_COHERENCE_DOMAIN_INFO *DomainInformation;
} SAL_ST_CACHE_COHERENCE_DECRIPTOR;

///
/// Format of Application Processor Wake-Up Descriptor Entry.
///
typedef struct {
  UINT8   Type;                   ///< Type here should be 5.
  UINT8   WakeUpType;
  UINT8   Reserved[6];
  UINT64  ExternalInterruptVector;
} SAL_ST_AP_WAKEUP_DECRIPTOR;

///
/// Format of Firmware Interface Table (FIT) Entry.
///
typedef struct {
  UINT64  Address;
  UINT8   Size[3];
  UINT8   Reserved;
  UINT16  Revision;
  UINT8   Type : 7;
  UINT8   CheckSumValid : 1;
  UINT8   CheckSum;
} EFI_SAL_FIT_ENTRY;
//
// FIT Types 
//
#define EFI_SAL_FIT_FIT_HEADER_TYPE                0x00
#define EFI_SAL_FIT_PAL_B_TYPE                     0x01
//
// Type from 0x02 to 0x0D is reserved.
//
#define EFI_SAL_FIT_PROCESSOR_SPECIFIC_PAL_A_TYPE  0x0E
#define EFI_SAL_FIT_PAL_A_TYPE                     0x0F
//
// OEM-defined type range is from 0x10 to 0x7E.
// Here we defined the PEI_CORE type as 0x10
//
#define EFI_SAL_FIT_PEI_CORE_TYPE                  0x10
#define EFI_SAL_FIT_UNUSED_TYPE                    0x7F

//
// FIT Entry
//
#define EFI_SAL_FIT_ENTRY_PTR   (0x100000000 - 32)  // 4GB - 24
#define EFI_SAL_FIT_PALA_ENTRY  (0x100000000 - 48)  // 4GB - 32
#define EFI_SAL_FIT_PALB_TYPE   01

//
// Following definitions are for Error Record Structure
//

///
/// Format of TimeStamp field in Record Header.
///
typedef struct {
  UINT8 Seconds;
  UINT8 Minutes;
  UINT8 Hours;
  UINT8 Reserved;
  UINT8 Day;
  UINT8 Month;
  UINT8 Year;
  UINT8 Century;
} SAL_TIME_STAMP;
///
/// Definition of Record Header.
///
typedef struct {
  UINT64          RecordId;
  UINT16          Revision;
  UINT8           ErrorSeverity;
  UINT8           ValidationBits;
  UINT32          RecordLength;
  SAL_TIME_STAMP  TimeStamp;
  UINT8           OemPlatformId[16];
} SAL_RECORD_HEADER;
///
/// Definition of Section Header.
///
typedef struct {
  GUID      Guid;
  UINT16    Revision;
  UINT8     ErrorRecoveryInfo;
  UINT8     Reserved;
  UINT32    SectionLength;
} SAL_SEC_HEADER;

///
/// GUID of Processor Machine Check Errors.
///
#define SAL_PROCESSOR_ERROR_RECORD_INFO \
  { \
    0xe429faf1, 0x3cb7, 0x11d4, {0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } \
  }
//
// Bit masks for valid bits of MOD_ERROR_INFO
//
#define CHECK_INFO_VALID_BIT_MASK   0x1
#define REQUESTOR_ID_VALID_BIT_MASK 0x2
#define RESPONDER_ID_VALID_BIT_MASK 0x4
#define TARGER_ID_VALID_BIT_MASK    0x8
#define PRECISE_IP_VALID_BIT_MASK   0x10
///
/// Definition of MOD_ERROR_INFO_STRUCT.
///
typedef struct {
  UINT64  InfoValid : 1;
  UINT64  ReqValid : 1;
  UINT64  RespValid : 1;
  UINT64  TargetValid : 1;
  UINT64  IpValid : 1;
  UINT64  Reserved : 59;
  UINT64  Info;
  UINT64  Req;
  UINT64  Resp;
  UINT64  Target;
  UINT64  Ip;
} MOD_ERROR_INFO;
///
/// Definition of CPUID_INFO_STRUCT.
///
typedef struct {
  UINT8 CpuidInfo[40];
  UINT8 Reserved;
} CPUID_INFO;

typedef struct {
  UINT64  FrLow;
  UINT64  FrHigh;
} FR_STRUCT;
//
// Bit masks for PSI_STATIC_STRUCT.ValidFieldBits
//
#define MIN_STATE_VALID_BIT_MASK  0x1
#define BR_VALID_BIT_MASK         0x2
#define CR_VALID_BIT_MASK         0x4
#define AR_VALID_BIT_MASK         0x8
#define RR_VALID_BIT_MASK         0x10
#define FR_VALID_BIT_MASK         0x20
///
/// Definition of PSI_STATIC_STRUCT.
///
typedef struct {
  UINT64    ValidFieldBits;
  UINT8     MinStateInfo[1024];
  UINT64    Br[8];
  UINT64    Cr[128];
  UINT64    Ar[128];
  UINT64    Rr[8];
  FR_STRUCT Fr[128];
} PSI_STATIC_STRUCT;
//
// Bit masks for SAL_PROCESSOR_ERROR_RECORD.ValidationBits
//
#define PROC_ERROR_MAP_VALID_BIT_MASK       0x1
#define PROC_STATE_PARAMETER_VALID_BIT_MASK 0x2
#define PROC_CR_LID_VALID_BIT_MASK          0x4
#define PROC_STATIC_STRUCT_VALID_BIT_MASK   0x8
#define CPU_INFO_VALID_BIT_MASK             0x1000000
///
/// Definition of Processor Machine Check Error Record.
///
typedef struct {
  SAL_SEC_HEADER    SectionHeader;
  UINT64            ValidationBits;
  UINT64            ProcErrorMap;
  UINT64            ProcStateParameter;
  UINT64            ProcCrLid;
  MOD_ERROR_INFO    CacheError[15];
  MOD_ERROR_INFO    TlbError[15];
  MOD_ERROR_INFO    BusError[15];
  MOD_ERROR_INFO    RegFileCheck[15];
  MOD_ERROR_INFO    MsCheck[15];
  CPUID_INFO        CpuInfo;
  PSI_STATIC_STRUCT PsiValidData;
} SAL_PROCESSOR_ERROR_RECORD;

///
/// GUID of Platform Memory Device Error Info.
///
#define SAL_MEMORY_ERROR_RECORD_INFO \
  { \
    0xe429faf2, 0x3cb7, 0x11d4, {0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } \
  }
//
// Bit masks for SAL_MEMORY_ERROR_RECORD.ValidationBits
//
#define MEMORY_ERROR_STATUS_VALID_BIT_MASK                0x1
#define MEMORY_PHYSICAL_ADDRESS_VALID_BIT_MASK            0x2
#define MEMORY_ADDR_BIT_MASK                              0x4
#define MEMORY_NODE_VALID_BIT_MASK                        0x8
#define MEMORY_CARD_VALID_BIT_MASK                        0x10
#define MEMORY_MODULE_VALID_BIT_MASK                      0x20
#define MEMORY_BANK_VALID_BIT_MASK                        0x40
#define MEMORY_DEVICE_VALID_BIT_MASK                      0x80
#define MEMORY_ROW_VALID_BIT_MASK                         0x100
#define MEMORY_COLUMN_VALID_BIT_MASK                      0x200
#define MEMORY_BIT_POSITION_VALID_BIT_MASK                0x400
#define MEMORY_PLATFORM_REQUESTOR_ID_VALID_BIT_MASK       0x800
#define MEMORY_PLATFORM_RESPONDER_ID_VALID_BIT_MASK       0x1000
#define MEMORY_PLATFORM_TARGET_VALID_BIT_MASK             0x2000
#define MEMORY_PLATFORM_BUS_SPECIFIC_DATA_VALID_BIT_MASK  0x4000
#define MEMORY_PLATFORM_OEM_ID_VALID_BIT_MASK             0x8000
#define MEMORY_PLATFORM_OEM_DATA_STRUCT_VALID_BIT_MASK    0x10000
///
/// Definition of Platform Memory Device Error Info Record.
///
typedef struct {
  SAL_SEC_HEADER  SectionHeader;
  UINT64          ValidationBits;
  UINT64          MemErrorStatus;
  UINT64          MemPhysicalAddress;
  UINT64          MemPhysicalAddressMask;
  UINT16          MemNode;
  UINT16          MemCard;
  UINT16          MemModule;
  UINT16          MemBank;
  UINT16          MemDevice;
  UINT16          MemRow;
  UINT16          MemColumn;
  UINT16          MemBitPosition;
  UINT64          ModRequestorId;
  UINT64          ModResponderId;
  UINT64          ModTargetId;
  UINT64          BusSpecificData;
  UINT8           MemPlatformOemId[16];
} SAL_MEMORY_ERROR_RECORD;

///
/// GUID of Platform PCI Bus Error Info.
///
#define SAL_PCI_BUS_ERROR_RECORD_INFO \
  { \
    0xe429faf4, 0x3cb7, 0x11d4, {0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } \
  }
//
// Bit masks for SAL_PCI_BUS_ERROR_RECORD.ValidationBits
//
#define PCI_BUS_ERROR_STATUS_VALID_BIT_MASK     0x1
#define PCI_BUS_ERROR_TYPE_VALID_BIT_MASK       0x2
#define PCI_BUS_ID_VALID_BIT_MASK               0x4
#define PCI_BUS_ADDRESS_VALID_BIT_MASK          0x8
#define PCI_BUS_DATA_VALID_BIT_MASK             0x10
#define PCI_BUS_CMD_VALID_BIT_MASK              0x20
#define PCI_BUS_REQUESTOR_ID_VALID_BIT_MASK     0x40
#define PCI_BUS_RESPONDER_ID_VALID_BIT_MASK     0x80
#define PCI_BUS_TARGET_VALID_BIT_MASK           0x100
#define PCI_BUS_OEM_ID_VALID_BIT_MASK           0x200
#define PCI_BUS_OEM_DATA_STRUCT_VALID_BIT_MASK  0x400

///
/// Designated PCI Bus identifier.
///
typedef struct {
  UINT8 BusNumber;
  UINT8 SegmentNumber;
} PCI_BUS_ID;

///
/// Definition of Platform PCI Bus Error Info Record.
///
typedef struct {
  SAL_SEC_HEADER  SectionHeader;
  UINT64          ValidationBits;
  UINT64          PciBusErrorStatus;
  UINT16          PciBusErrorType;
  PCI_BUS_ID      PciBusId;
  UINT32          Reserved;
  UINT64          PciBusAddress;
  UINT64          PciBusData;
  UINT64          PciBusCommand;
  UINT64          PciBusRequestorId;
  UINT64          PciBusResponderId;
  UINT64          PciBusTargetId;
  UINT8           PciBusOemId[16];
} SAL_PCI_BUS_ERROR_RECORD;

///
/// GUID of Platform PCI Component Error Info.
///
#define SAL_PCI_COMP_ERROR_RECORD_INFO \
  { \
    0xe429faf6, 0x3cb7, 0x11d4, {0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } \
  }
//
// Bit masks for SAL_PCI_COMPONENT_ERROR_RECORD.ValidationBits
//
#define PCI_COMP_ERROR_STATUS_VALID_BIT_MASK    0x1
#define PCI_COMP_INFO_VALID_BIT_MASK            0x2
#define PCI_COMP_MEM_NUM_VALID_BIT_MASK         0x4
#define PCI_COMP_IO_NUM_VALID_BIT_MASK          0x8
#define PCI_COMP_REG_DATA_PAIR_VALID_BIT_MASK   0x10
#define PCI_COMP_OEM_DATA_STRUCT_VALID_BIT_MASK 0x20
///
/// Format of PCI Component Information to identify the device.
///
typedef struct {
  UINT16  VendorId;
  UINT16  DeviceId;
  UINT8   ClassCode[3];
  UINT8   FunctionNumber;
  UINT8   DeviceNumber;
  UINT8   BusNumber;
  UINT8   SegmentNumber;
  UINT8   Reserved[5];
} PCI_COMP_INFO;
///
/// Definition of Platform PCI Component Error Info.
///
typedef struct {
  SAL_SEC_HEADER  SectionHeader;
  UINT64          ValidationBits;
  UINT64          PciComponentErrorStatus;
  PCI_COMP_INFO   PciComponentInfo;
  UINT32          PciComponentMemNum;
  UINT32          PciComponentIoNum;
  UINT8           PciBusOemId[16];
} SAL_PCI_COMPONENT_ERROR_RECORD;

///
/// Platform SEL Device Error Info.
///
#define SAL_SEL_DEVICE_ERROR_RECORD_INFO \
  { \
    0xe429faf3, 0x3cb7, 0x11d4, {0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } \
  }
//
// Bit masks for SAL_SEL_DEVICE_ERROR_RECORD.ValidationBits
//
#define SEL_RECORD_ID_VALID_BIT_MASK      0x1;
#define SEL_RECORD_TYPE_VALID_BIT_MASK    0x2;
#define SEL_GENERATOR_ID_VALID_BIT_MASK   0x4;
#define SEL_EVM_REV_VALID_BIT_MASK        0x8;
#define SEL_SENSOR_TYPE_VALID_BIT_MASK    0x10;
#define SEL_SENSOR_NUM_VALID_BIT_MASK     0x20;
#define SEL_EVENT_DIR_TYPE_VALID_BIT_MASK 0x40;
#define SEL_EVENT_DATA1_VALID_BIT_MASK    0x80;
#define SEL_EVENT_DATA2_VALID_BIT_MASK    0x100;
#define SEL_EVENT_DATA3_VALID_BIT_MASK    0x200;
///
/// Definition of Platform SEL Device Error Info Record.
///
typedef struct {
  SAL_SEC_HEADER  SectionHeader;
  UINT64          ValidationBits;
  UINT16          SelRecordId;
  UINT8           SelRecordType;
  UINT32          TimeStamp;
  UINT16          GeneratorId;
  UINT8           EvmRevision;
  UINT8           SensorType;
  UINT8           SensorNum;
  UINT8           EventDirType;
  UINT8           Data1;
  UINT8           Data2;
  UINT8           Data3;
} SAL_SEL_DEVICE_ERROR_RECORD;

///
/// GUID of Platform SMBIOS Device Error Info.
///
#define SAL_SMBIOS_ERROR_RECORD_INFO \
  { \
    0xe429faf5, 0x3cb7, 0x11d4, {0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } \
  }
//
// Bit masks for SAL_SMBIOS_DEVICE_ERROR_RECORD.ValidationBits
//
#define SMBIOS_EVENT_TYPE_VALID_BIT_MASK  0x1
#define SMBIOS_LENGTH_VALID_BIT_MASK      0x2
#define SMBIOS_TIME_STAMP_VALID_BIT_MASK  0x4
#define SMBIOS_DATA_VALID_BIT_MASK        0x8
///
/// Definition of Platform SMBIOS Device Error Info Record.
///
typedef struct {
  SAL_SEC_HEADER  SectionHeader;
  UINT64          ValidationBits;
  UINT8           SmbiosEventType;
  UINT8           SmbiosLength;
  UINT8           SmbiosBcdTimeStamp[6];
} SAL_SMBIOS_DEVICE_ERROR_RECORD;

///
/// GUID of Platform Specific Error Info.
///
#define SAL_PLATFORM_ERROR_RECORD_INFO \
  { \
    0xe429faf7, 0x3cb7, 0x11d4, {0xbc, 0xa7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } \
  }
//
// Bit masks for SAL_PLATFORM_SPECIFIC_ERROR_RECORD.ValidationBits
//
#define PLATFORM_ERROR_STATUS_VALID_BIT_MASK    0x1
#define PLATFORM_REQUESTOR_ID_VALID_BIT_MASK    0x2
#define PLATFORM_RESPONDER_ID_VALID_BIT_MASK    0x4
#define PLATFORM_TARGET_VALID_BIT_MASK          0x8
#define PLATFORM_SPECIFIC_DATA_VALID_BIT_MASK   0x10
#define PLATFORM_OEM_ID_VALID_BIT_MASK          0x20
#define PLATFORM_OEM_DATA_STRUCT_VALID_BIT_MASK 0x40
#define PLATFORM_OEM_DEVICE_PATH_VALID_BIT_MASK 0x80
///
/// Definition of Platform Specific Error Info Record.
///
typedef struct {
  SAL_SEC_HEADER  SectionHeader;
  UINT64          ValidationBits;
  UINT64          PlatformErrorStatus;
  UINT64          PlatformRequestorId;
  UINT64          PlatformResponderId;
  UINT64          PlatformTargetId;
  UINT64          PlatformBusSpecificData;
  UINT8           OemComponentId[16];
} SAL_PLATFORM_SPECIFIC_ERROR_RECORD;

///
/// Union of all the possible SAL Error Record Types.
///
typedef union {
  SAL_RECORD_HEADER                   *RecordHeader;
  SAL_PROCESSOR_ERROR_RECORD          *SalProcessorRecord;
  SAL_PCI_BUS_ERROR_RECORD            *SalPciBusRecord;
  SAL_PCI_COMPONENT_ERROR_RECORD      *SalPciComponentRecord;
  SAL_SEL_DEVICE_ERROR_RECORD         *ImpiRecord;
  SAL_SMBIOS_DEVICE_ERROR_RECORD      *SmbiosRecord;
  SAL_PLATFORM_SPECIFIC_ERROR_RECORD  *PlatformRecord;
  SAL_MEMORY_ERROR_RECORD             *MemoryRecord;
  UINT8                               *Raw;
} SAL_ERROR_RECORDS_POINTERS;

#pragma pack()

#endif
