/** @file
  GUID used to identify id for the caller who is initiating the Status Code.

  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  These GUIDs and structures are defined in UEFI Platform Initialization Specification 1.2
  Volume 3: Shared Architectural Elements

**/

#ifndef __PI_STATUS_CODE_DATA_TYPE_ID_GUID_H__
#define __PI_STATUS_CODE_DATA_TYPE_ID_GUID_H__

#include <PiDxe.h>
#include <Protocol/DebugSupport.h>

///
/// Global ID for the EFI_STATUS_CODE_STRING structure
///
#define EFI_STATUS_CODE_DATA_TYPE_STRING_GUID \
  { 0x92D11080, 0x496F, 0x4D95, { 0xBE, 0x7E, 0x03, 0x74, 0x88, 0x38, 0x2B, 0x0A } }

typedef enum {
  ///
  /// A NULL-terminated ASCII string.
  ///
  EfiStringAscii,
  ///
  /// A double NULL-terminated Unicode string.
  ///
  EfiStringUnicode,
  ///
  /// An EFI_STATUS_CODE_STRING_TOKEN representing the string.  The actual
  /// string can be obtained by querying the HII Database
  ///
  EfiStringToken
} EFI_STRING_TYPE;

///
/// Specifies the format of the data in EFI_STATUS_CODE_STRING_DATA.String.
///
typedef struct {
  ///
  /// The HII package list which contains the string.  Handle is a dynamic value that may
  /// not be the same for different boots.  Type EFI_HII_HANDLE is defined in
  /// EFI_HII_DATABASE_PROTOCOL.NewPackageList() in the UEFI Specification.
  ///
  EFI_HII_HANDLE    Handle;
  ///
  /// When combined with Handle, the string token can be used to retrieve the string.
  /// Type EFI_STRING_ID is defined in EFI_IFR_OP_HEADER in the UEFI Specification.
  ///
  EFI_STRING_ID     Token;
} EFI_STATUS_CODE_STRING_TOKEN;

typedef union {
  ///
  /// ASCII formatted string.
  ///
  CHAR8                           *Ascii;
  ///
  /// Unicode formatted string.
  ///
  CHAR16                          *Unicode;
  ///
  /// HII handle/token pair.
  ///
  EFI_STATUS_CODE_STRING_TOKEN    Hii;
} EFI_STATUS_CODE_STRING;

///
/// This data type defines a string type of extended data. A string can accompany
/// any status code. The string can provide additional information about the
/// status code. The string can be ASCII, Unicode, or a Human Interface Infrastructure
/// (HII) token/GUID pair.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_STATUS_CODE_STRING_DATA) - HeaderSize, and
  /// DataHeader.Type should be
  /// EFI_STATUS_CODE_DATA_TYPE_STRING_GUID.
  ///
  EFI_STATUS_CODE_DATA      DataHeader;
  ///
  /// Specifies the format of the data in String.
  ///
  EFI_STRING_TYPE           StringType;
  ///
  /// A pointer to the extended data. The data follows the format specified by
  /// StringType.
  ///
  EFI_STATUS_CODE_STRING    String;
} EFI_STATUS_CODE_STRING_DATA;

extern EFI_GUID  gEfiStatusCodeDataTypeStringGuid;

///
/// Global ID for the following structures:
///   - EFI_DEVICE_PATH_EXTENDED_DATA
///   - EFI_DEVICE_HANDLE_EXTENDED_DATA
///   - EFI_RESOURCE_ALLOC_FAILURE_ERROR_DATA
///   - EFI_COMPUTING_UNIT_VOLTAGE_ERROR_DATA
///   - EFI_COMPUTING_UNIT_MICROCODE_UPDATE_ERROR_DATA
///   - EFI_COMPUTING_UNIT_TIMER_EXPIRED_ERROR_DATA
///   - EFI_HOST_PROCESSOR_MISMATCH_ERROR_DATA
///   - EFI_MEMORY_RANGE_EXTENDED_DATA
///   - EFI_DEBUG_ASSERT_DATA
///   - EFI_STATUS_CODE_EXCEP_EXTENDED_DATA
///   - EFI_STATUS_CODE_START_EXTENDED_DATA
///   - EFI_LEGACY_OPROM_EXTENDED_DATA
///   - EFI_RETURN_STATUS_EXTENDED_DATA
///
#define EFI_STATUS_CODE_SPECIFIC_DATA_GUID \
  { 0x335984bd, 0xe805, 0x409a, { 0xb8, 0xf8, 0xd2, 0x7e, 0xce, 0x5f, 0xf7, 0xa6 } }

///
/// Extended data about the device path, which is used for many errors and
/// progress codes to point to the device.
///
/// The device path is used to point to the physical device in case there is more than one device
/// belonging to the same subclass. For example, the system may contain two USB keyboards and one
/// PS/2* keyboard. The driver that parses the status code can use the device path extended data to
/// differentiate between the three. The index field is not useful in this case because there is no standard
/// numbering convention. Device paths are preferred over using device handles because device handles
/// for a given device can change from one boot to another and do not mean anything beyond Boot
/// Services time. In certain cases, the bus driver may not create a device handle for a given device if it
/// detects a critical error. In these cases, the device path extended data can be used to refer to the
/// device, but there may not be any device handles with an instance of
/// EFI_DEVICE_PATH_PROTOCOL that matches DevicePath. The variable device path structure
/// is included in this structure to make it self sufficient.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA). DataHeader.Size should be the size
  /// of variable-length DevicePath, and DataHeader.Size is zero for a virtual
  /// device that does not have a device path. DataHeader.Type should be
  /// EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The device path to the controller or the hardware device. Note that this parameter is a
  /// variable-length device path structure and not a pointer to such a structure. This structure is
  /// populated only if it is a physical device. For virtual devices, the Size field in DataHeader
  /// is set to zero and this field is not populated.
  ///
  //  EFI_DEVICE_PATH_PROTOCOL         DevicePath;
} EFI_DEVICE_PATH_EXTENDED_DATA;

///
/// Device handle Extended Data. Used for many
/// errors and progress codes to point to the device.
///
/// The handle of the device with which the progress or error code is associated. The handle is
/// guaranteed to be accurate only at the time the status code is reported. Handles are dynamic entities
/// between boots, so handles cannot be considered to be valid if the system has reset subsequent to the
/// status code being reported. Handles may be used to determine a wide variety of useful information
/// about the source of the status code.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_DEVICE_HANDLE_EXTENDED_DATA) - HeaderSize, and
  /// DataHeader.Type should be EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The device handle.
  ///
  EFI_HANDLE              Handle;
} EFI_DEVICE_HANDLE_EXTENDED_DATA;

///
/// This structure defines extended data describing a PCI resource allocation error.
///
/// @par Note:
///   The following structure contains variable-length fields and cannot be defined as a C-style
///   structure.
///
/// This extended data conveys details for a PCI resource allocation failure error. See the PCI
/// specification and the ACPI specification for details on PCI resource allocations and the format for
/// resource descriptors. This error does not detail why the resource allocation failed. It may be due to a
/// bad resource request or a lack of available resources to satisfy a valid request. The variable device
/// path structure and the resource structures are included in this structure to make it self sufficient.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be sizeof
  /// (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// (DevicePathSize + DevicePathSize + DevicePathSize +
  /// sizeof(UINT32) + 3 * sizeof (UINT16) ), and DataHeader.Type
  /// should be EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The PCI BAR. Applicable only for PCI devices. Ignored for all other devices.
  ///
  UINT32                  Bar;
  ///
  /// DevicePathSize should be zero if it is a virtual device that is not associated with
  /// a device path. Otherwise, this parameter is the length of the variable-length
  /// DevicePath.
  ///
  UINT16                  DevicePathSize;
  ///
  /// Represents the size the ReqRes parameter. ReqResSize should be zero if the
  /// requested resources are not provided as a part of extended data.
  ///
  UINT16                  ReqResSize;
  ///
  /// Represents the size the AllocRes parameter. AllocResSize should be zero if the
  /// allocated resources are not provided as a part of extended data.
  ///
  UINT16                  AllocResSize;
  ///
  /// The device path to the controller or the hardware device that did not get the requested
  /// resources. Note that this parameter is the variable-length device path structure and not
  /// a pointer to this structure.
  ///
  //  EFI_DEVICE_PATH_PROTOCOL       DevicePath;
  ///
  /// The requested resources in the format of an ACPI 2.0 resource descriptor. This
  /// parameter is not a pointer; it is the complete resource descriptor.
  ///
  //  UINT8                          ReqRes[];
  ///
  /// The allocated resources in the format of an ACPI 2.0 resource descriptor. This
  /// parameter is not a pointer; it is the complete resource descriptor.
  ///
  //  UINT8                          AllocRes[];
} EFI_RESOURCE_ALLOC_FAILURE_ERROR_DATA;

///
/// This structure provides a calculation for base-10 representations.
///
/// Not consistent with PI 1.2 Specification.
/// This data type is not defined in the PI 1.2 Specification, but is
/// required by several of the other data structures in this file.
///
typedef struct {
  ///
  /// The INT16 number by which to multiply the base-2 representation.
  ///
  INT16    Value;
  ///
  /// The INT16 number by which to raise the base-2 calculation.
  ///
  INT16    Exponent;
} EFI_EXP_BASE10_DATA;

///
/// This structure provides the voltage at the time of error. It also provides
/// the threshold value indicating the minimum or maximum voltage that is considered
/// an error. If the voltage is less then the threshold, the error indicates that the
/// voltage fell below the minimum acceptable value. If the voltage is greater then the threshold,
/// the error indicates that the voltage rose above the maximum acceptable value.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_COMPUTING_UNIT_VOLTAGE_ERROR_DATA) -
  /// HeaderSize, and DataHeader.Type should be
  /// EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The voltage value at the time of the error.
  ///
  EFI_EXP_BASE10_DATA     Voltage;
  ///
  /// The voltage threshold.
  ///
  EFI_EXP_BASE10_DATA     Threshold;
} EFI_COMPUTING_UNIT_VOLTAGE_ERROR_DATA;

///
/// Microcode Update Extended Error Data
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_COMPUTING_UNIT_MICROCODE_UPDATE_ERROR_DATA) -
  /// HeaderSize, and DataHeader.Type should be
  /// EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The version of the microcode update from the header.
  ///
  UINT32                  Version;
} EFI_COMPUTING_UNIT_MICROCODE_UPDATE_ERROR_DATA;

///
/// This structure provides details about the computing unit timer expiration error.
/// The timer limit provides the timeout value of the timer prior to expiration.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_COMPUTING_UNIT_TIMER_EXPIRED_ERROR_DATA) -
  /// HeaderSize, and DataHeader.Type should be
  /// EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The number of seconds that the computing unit timer was configured to expire.
  ///
  EFI_EXP_BASE10_DATA     TimerLimit;
} EFI_COMPUTING_UNIT_TIMER_EXPIRED_ERROR_DATA;

///
/// Attribute bits for EFI_HOST_PROCESSOR_MISMATCH_ERROR_DATA.Attributes
/// All other attributes are reserved for future use and must be initialized to 0.
///
///@{
#define EFI_COMPUTING_UNIT_MISMATCH_SPEED       0x0001
#define EFI_COMPUTING_UNIT_MISMATCH_FSB_SPEED   0x0002
#define EFI_COMPUTING_UNIT_MISMATCH_FAMILY      0x0004
#define EFI_COMPUTING_UNIT_MISMATCH_MODEL       0x0008
#define EFI_COMPUTING_UNIT_MISMATCH_STEPPING    0x0010
#define EFI_COMPUTING_UNIT_MISMATCH_CACHE_SIZE  0x0020
#define EFI_COMPUTING_UNIT_MISMATCH_OEM1        0x1000
#define EFI_COMPUTING_UNIT_MISMATCH_OEM2        0x2000
#define EFI_COMPUTING_UNIT_MISMATCH_OEM3        0x4000
#define EFI_COMPUTING_UNIT_MISMATCH_OEM4        0x8000
///@}

///
/// This structure defines extended data for processor mismatch errors.
///
/// This provides information to indicate which processors mismatch, and how they mismatch. The
/// status code contains the instance number of the processor that is in error. This structure's
/// Instance indicates the second processor that does not match. This differentiation allows the
/// consumer to determine which two processors do not match. The Attributes indicate what
/// mismatch is being reported. Because Attributes is a bit field, more than one mismatch can be
/// reported with one error code.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_ HOST_PROCESSOR_MISMATCH_ERROR_DATA) -
  /// HeaderSize , and DataHeader.Type should be
  /// EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The unit number of the computing unit that does not match.
  ///
  UINT32                  Instance;
  ///
  /// The attributes describing the failure.
  ///
  UINT16                  Attributes;
} EFI_HOST_PROCESSOR_MISMATCH_ERROR_DATA;

///
/// This structure provides details about the computing unit thermal failure.
///
/// This structure provides the temperature at the time of error. It also provides the threshold value
/// indicating the minimum temperature that is considered an error.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_COMPUTING_UNIT_THERMAL_ERROR_DATA) -
  /// HeaderSize , and DataHeader.Type should be
  /// EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The thermal value at the time of the error.
  ///
  EFI_EXP_BASE10_DATA     Temperature;
  ///
  /// The thermal threshold.
  ///
  EFI_EXP_BASE10_DATA     Threshold;
} EFI_COMPUTING_UNIT_THERMAL_ERROR_DATA;

///
/// Enumeration of valid cache types
///
typedef enum {
  EfiInitCacheDataOnly,
  EfiInitCacheInstrOnly,
  EfiInitCacheBoth,
  EfiInitCacheUnspecified
} EFI_INIT_CACHE_TYPE;

///
/// Embedded cache init extended data
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_CACHE_INIT_DATA) - HeaderSize , and
  /// DataHeader.Type should be EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The cache level. Starts with 1 for level 1 cache.
  ///
  UINT32                  Level;
  ///
  /// The type of cache.
  ///
  EFI_INIT_CACHE_TYPE     Type;
} EFI_CACHE_INIT_DATA;

///
///
///
typedef UINT32 EFI_CPU_STATE_CHANGE_CAUSE;

///
/// The reasons that the processor is disabled.
/// Used to fill in EFI_COMPUTING_UNIT_CPU_DISABLED_ERROR_DATA.Cause.
///
///@{
#define EFI_CPU_CAUSE_INTERNAL_ERROR    0x0001
#define EFI_CPU_CAUSE_THERMAL_ERROR     0x0002
#define EFI_CPU_CAUSE_SELFTEST_FAILURE  0x0004
#define EFI_CPU_CAUSE_PREBOOT_TIMEOUT   0x0008
#define EFI_CPU_CAUSE_FAILED_TO_START   0x0010
#define EFI_CPU_CAUSE_CONFIG_ERROR      0x0020
#define EFI_CPU_CAUSE_USER_SELECTION    0x0080
#define EFI_CPU_CAUSE_BY_ASSOCIATION    0x0100
#define EFI_CPU_CAUSE_UNSPECIFIED       0x8000
///@}

///
/// This structure provides information about the disabled computing unit.
///
/// This structure provides details as to why and how the computing unit was disabled. The causes
/// should cover the typical reasons a processor would be disabled. How the processor was disabled is
/// important because there are distinct differences between hardware and software disabling.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_COMPUTING_UNIT_CPU_DISABLED_ERROR_DATA) -
  /// HeaderSize, and DataHeader.Type should be
  /// EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The reason for disabling the processor.
  ///
  UINT32                  Cause;
  ///
  /// TRUE if the processor is disabled via software means such as not listing it in the ACPI tables.
  /// Such a processor will respond to Interprocessor Interrupts (IPIs). FALSE if the processor is hardware
  /// disabled, which means it is invisible to software and will not respond to IPIs.
  ///
  BOOLEAN                 SoftwareDisabled;
} EFI_COMPUTING_UNIT_CPU_DISABLED_ERROR_DATA;

///
/// Memory Error Granularity Definition
///
typedef UINT8 EFI_MEMORY_ERROR_GRANULARITY;

///
/// Memory Error Granularities.  Used to fill in EFI_MEMORY_EXTENDED_ERROR_DATA.Granularity.
///
///@{
#define EFI_MEMORY_ERROR_OTHER      0x01
#define EFI_MEMORY_ERROR_UNKNOWN    0x02
#define EFI_MEMORY_ERROR_DEVICE     0x03
#define EFI_MEMORY_ERROR_PARTITION  0x04
///@}

///
/// Memory Error Operation Definition
///
typedef UINT8 EFI_MEMORY_ERROR_OPERATION;

///
/// Memory Error Operations.  Used to fill in EFI_MEMORY_EXTENDED_ERROR_DATA.Operation.
///
///@{
#define EFI_MEMORY_OPERATION_OTHER          0x01
#define EFI_MEMORY_OPERATION_UNKNOWN        0x02
#define EFI_MEMORY_OPERATION_READ           0x03
#define EFI_MEMORY_OPERATION_WRITE          0x04
#define EFI_MEMORY_OPERATION_PARTIAL_WRITE  0x05
///@}

///
/// This structure provides specific details about the memory error that was detected. It provides
/// enough information so that consumers can identify the exact failure and provides enough
/// information to enable corrective action if necessary.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_MEMORY_EXTENDED_ERROR_DATA) - HeaderSize, and
  /// DataHeader.Type should be EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA            DataHeader;
  ///
  /// The error granularity type.
  ///
  EFI_MEMORY_ERROR_GRANULARITY    Granularity;
  ///
  /// The operation that resulted in the error being detected.
  ///
  EFI_MEMORY_ERROR_OPERATION      Operation;
  ///
  /// The error syndrome, vendor-specific ECC syndrome, or CRC data associated with
  /// the error.  If unknown, should be initialized to 0.
  /// Inconsistent with specification here:
  /// This field in StatusCodes spec0.9 is defined as UINT32, keep code unchanged.
  ///
  UINTN                           Syndrome;
  ///
  /// The physical address of the error.
  ///
  EFI_PHYSICAL_ADDRESS            Address;
  ///
  /// The range, in bytes, within which the error address can be determined.
  ///
  UINTN                           Resolution;
} EFI_MEMORY_EXTENDED_ERROR_DATA;

///
/// A definition to describe that the operation is performed on multiple devices within the array.
/// May be used for EFI_STATUS_CODE_DIMM_NUMBER.Array and EFI_STATUS_CODE_DIMM_NUMBER.Device.
///
#define EFI_MULTIPLE_MEMORY_DEVICE_OPERATION  0xfffe

///
/// A definition to describe that the operation is performed on all devices within the array.
/// May be used for EFI_STATUS_CODE_DIMM_NUMBER.Array and EFI_STATUS_CODE_DIMM_NUMBER.Device.
///
#define EFI_ALL_MEMORY_DEVICE_OPERATION  0xffff

///
/// A definition to describe that the operation is performed on multiple arrays.
/// May be used for EFI_STATUS_CODE_DIMM_NUMBER.Array and EFI_STATUS_CODE_DIMM_NUMBER.Device.
///
#define EFI_MULTIPLE_MEMORY_ARRAY_OPERATION  0xfffe

///
/// A definition to describe that the operation is performed on all the arrays.
/// May be used for EFI_STATUS_CODE_DIMM_NUMBER.Array and EFI_STATUS_CODE_DIMM_NUMBER.Device.
///
#define EFI_ALL_MEMORY_ARRAY_OPERATION  0xffff

///
/// This extended data provides some context that consumers can use to locate a DIMM within the
/// overall memory scheme.
///
/// This extended data provides some context that consumers can use to locate a DIMM within the
/// overall memory scheme. The Array and Device numbers may indicate a specific DIMM, or they
/// may be populated with the group definitions in "Related Definitions" below.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_STATUS_CODE_DIMM_NUMBER) - HeaderSize, and
  /// DataHeader.Type should be EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The memory array number.
  ///
  UINT16                  Array;
  ///
  /// The device number within that Array.
  ///
  UINT16                  Device;
} EFI_STATUS_CODE_DIMM_NUMBER;

///
/// This structure defines extended data describing memory modules that do not match.
///
/// This extended data may be used to convey the specifics of memory modules that do not match.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_MEMORY_MODULE_MISMATCH_ERROR_DATA) -
  /// HeaderSize, and DataHeader.Type should be
  /// EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA           DataHeader;
  ///
  /// The instance number of the memory module that does not match.
  ///
  EFI_STATUS_CODE_DIMM_NUMBER    Instance;
} EFI_MEMORY_MODULE_MISMATCH_ERROR_DATA;

///
/// This structure defines extended data describing a memory range.
///
/// This extended data may be used to convey the specifics of a memory range.  Ranges are specified
/// with a start address and a length.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_MEMORY_RANGE_EXTENDED_DATA) - HeaderSize, and
  /// DataHeader.Type should be EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The starting address of the memory range.
  ///
  EFI_PHYSICAL_ADDRESS    Start;
  ///
  /// The length in bytes of the memory range.
  ///
  EFI_PHYSICAL_ADDRESS    Length;
} EFI_MEMORY_RANGE_EXTENDED_DATA;

///
/// This structure provides the assert information that is typically associated with a debug assertion failing.
///
/// The data indicates the location of the assertion that failed in the source code. This information
/// includes the file name and line number that are necessary to find the failing assertion in source code.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_DEBUG_ASSERT_DATA) - HeaderSize , and
  /// DataHeader.Type should be EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA           DataHeader;
  ///
  /// The line number of the source file where the fault was generated.
  ///
  UINT32                         LineNumber;
  ///
  /// The size in bytes of FileName.
  ///
  UINT32                         FileNameSize;
  ///
  /// A pointer to a NULL-terminated ASCII or Unicode string that represents
  /// the file name of the source file where the fault was generated.
  ///
  EFI_STATUS_CODE_STRING_DATA    *FileName;
} EFI_DEBUG_ASSERT_DATA;

///
/// System Context Data EBC/IA32/IPF
///
typedef union {
  ///
  /// The context of the EBC virtual machine when the exception was generated. Type
  /// EFI_SYSTEM_CONTEXT_EBC is defined in EFI_DEBUG_SUPPORT_PROTOCOL
  /// in the UEFI Specification.
  ///
  EFI_SYSTEM_CONTEXT_EBC     SystemContextEbc;
  ///
  /// The context of the IA-32 processor when the exception was generated. Type
  /// EFI_SYSTEM_CONTEXT_IA32 is defined in the
  /// EFI_DEBUG_SUPPORT_PROTOCOL in the UEFI Specification.
  ///
  EFI_SYSTEM_CONTEXT_IA32    SystemContextIa32;
  ///
  /// The context of the Itanium(R) processor when the exception was generated. Type
  /// EFI_SYSTEM_CONTEXT_IPF is defined in the
  /// EFI_DEBUG_SUPPORT_PROTOCOL in the UEFI Specification.
  ///
  EFI_SYSTEM_CONTEXT_IPF     SystemContextIpf;
  ///
  /// The context of the X64 processor when the exception was generated. Type
  /// EFI_SYSTEM_CONTEXT_X64 is defined in the
  /// EFI_DEBUG_SUPPORT_PROTOCOL in the UEFI Specification.
  ///
  EFI_SYSTEM_CONTEXT_X64     SystemContextX64;
  ///
  /// The context of the ARM processor when the exception was generated. Type
  /// EFI_SYSTEM_CONTEXT_ARM is defined in the
  /// EFI_DEBUG_SUPPORT_PROTOCOL in the UEFI Specification.
  ///
  EFI_SYSTEM_CONTEXT_ARM     SystemContextArm;
} EFI_STATUS_CODE_EXCEP_SYSTEM_CONTEXT;

///
/// This structure defines extended data describing a processor exception error.
///
/// This extended data allows the processor context that is present at the time of the exception to be
/// reported with the exception. The format and contents of the context data varies depending on the
/// processor architecture.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_STATUS_CODE_EXCEP_EXTENDED_DATA) - HeaderSize,
  /// and DataHeader.Type should be
  /// EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA                    DataHeader;
  ///
  /// The system context.
  ///
  EFI_STATUS_CODE_EXCEP_SYSTEM_CONTEXT    Context;
} EFI_STATUS_CODE_EXCEP_EXTENDED_DATA;

///
/// This structure defines extended data describing a call to a driver binding protocol start function.
///
/// This extended data records information about a Start() function call. Start() is a member of
/// the UEFI Driver Binding Protocol.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_STATUS_CODE_START_EXTENDED_DATA) - HeaderSize,
  /// and DataHeader.Type should be
  /// EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The controller handle.
  ///
  EFI_HANDLE              ControllerHandle;
  ///
  /// The driver binding handle.
  ///
  EFI_HANDLE              DriverBindingHandle;
  ///
  /// The size of the RemainingDevicePath. It is zero if the Start() function is
  /// called with RemainingDevicePath = NULL.  The UEFI Specification allows
  /// that the Start() function of bus drivers can be called in this way.
  ///
  UINT16                  DevicePathSize;
  ///
  /// Matches the RemainingDevicePath parameter being passed to the Start() function.
  /// Note that this parameter is the variable-length device path and not a pointer
  /// to the device path.
  ///
  //  EFI_DEVICE_PATH_PROTOCOL   RemainingDevicePath;
} EFI_STATUS_CODE_START_EXTENDED_DATA;

///
/// This structure defines extended data describing a legacy option ROM (OpROM).
///
/// The device handle and ROM image base can be used by consumers to determine which option ROM
/// failed. Due to the black-box nature of legacy option ROMs, the amount of information that can be
/// obtained may be limited.
///
typedef struct {
  ///
  /// The data header identifying the data. DataHeader.HeaderSize should be
  /// sizeof (EFI_STATUS_CODE_DATA), DataHeader.Size should be
  /// sizeof (EFI_LEGACY_OPROM_EXTENDED_DATA) - HeaderSize, and
  /// DataHeader.Type should be EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The handle corresponding to the device that this legacy option ROM is being invoked.
  ///
  EFI_HANDLE              DeviceHandle;
  ///
  /// The base address of the shadowed legacy ROM image.  May or may not point to the shadow RAM area.
  ///
  EFI_PHYSICAL_ADDRESS    RomImageBase;
} EFI_LEGACY_OPROM_EXTENDED_DATA;

///
/// This structure defines extended data describing an EFI_STATUS return value that stands for a
/// failed function call (such as a UEFI boot service).
///
typedef struct {
  ///
  /// The data header identifying the data:
  /// DataHeader.HeaderSize should be sizeof(EFI_STATUS_CODE_DATA),
  /// DataHeader.Size should be sizeof(EFI_RETURN_STATUS_EXTENDED_DATA) - HeaderSize,
  /// DataHeader.Type should be EFI_STATUS_CODE_SPECIFIC_DATA_GUID.
  ///
  EFI_STATUS_CODE_DATA    DataHeader;
  ///
  /// The EFI_STATUS return value of the service or function whose failure triggered the
  /// reporting of the status code (generally an error code or a debug code).
  ///
  EFI_STATUS              ReturnStatus;
} EFI_RETURN_STATUS_EXTENDED_DATA;

extern EFI_GUID  gEfiStatusCodeSpecificDataGuid;

#endif
