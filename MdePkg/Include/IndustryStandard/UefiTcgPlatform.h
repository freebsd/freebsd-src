/** @file
  TCG EFI Platform Definition in TCG_EFI_Platform_1_20_Final and
  TCG PC Client Platform Firmware Profile Specification, Revision 1.05

  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __UEFI_TCG_PLATFORM_H__
#define __UEFI_TCG_PLATFORM_H__

#include <IndustryStandard/Tpm12.h>
#include <IndustryStandard/Tpm20.h>
#include <Uefi.h>

//
// Standard event types
//
#define EV_PREBOOT_CERT             ((TCG_EVENTTYPE) 0x00000000)
#define EV_POST_CODE                ((TCG_EVENTTYPE) 0x00000001)
#define EV_NO_ACTION                ((TCG_EVENTTYPE) 0x00000003)
#define EV_SEPARATOR                ((TCG_EVENTTYPE) 0x00000004)
#define EV_ACTION                   ((TCG_EVENTTYPE) 0x00000005)
#define EV_EVENT_TAG                ((TCG_EVENTTYPE) 0x00000006)
#define EV_S_CRTM_CONTENTS          ((TCG_EVENTTYPE) 0x00000007)
#define EV_S_CRTM_VERSION           ((TCG_EVENTTYPE) 0x00000008)
#define EV_CPU_MICROCODE            ((TCG_EVENTTYPE) 0x00000009)
#define EV_PLATFORM_CONFIG_FLAGS    ((TCG_EVENTTYPE) 0x0000000A)
#define EV_TABLE_OF_DEVICES         ((TCG_EVENTTYPE) 0x0000000B)
#define EV_COMPACT_HASH             ((TCG_EVENTTYPE) 0x0000000C)
#define EV_NONHOST_CODE             ((TCG_EVENTTYPE) 0x0000000F)
#define EV_NONHOST_CONFIG           ((TCG_EVENTTYPE) 0x00000010)
#define EV_NONHOST_INFO             ((TCG_EVENTTYPE) 0x00000011)
#define EV_OMIT_BOOT_DEVICE_EVENTS  ((TCG_EVENTTYPE) 0x00000012)

//
// EFI specific event types
//
#define EV_EFI_EVENT_BASE                   ((TCG_EVENTTYPE) 0x80000000)
#define EV_EFI_VARIABLE_DRIVER_CONFIG       (EV_EFI_EVENT_BASE + 1)
#define EV_EFI_VARIABLE_BOOT                (EV_EFI_EVENT_BASE + 2)
#define EV_EFI_BOOT_SERVICES_APPLICATION    (EV_EFI_EVENT_BASE + 3)
#define EV_EFI_BOOT_SERVICES_DRIVER         (EV_EFI_EVENT_BASE + 4)
#define EV_EFI_RUNTIME_SERVICES_DRIVER      (EV_EFI_EVENT_BASE + 5)
#define EV_EFI_GPT_EVENT                    (EV_EFI_EVENT_BASE + 6)
#define EV_EFI_ACTION                       (EV_EFI_EVENT_BASE + 7)
#define EV_EFI_PLATFORM_FIRMWARE_BLOB       (EV_EFI_EVENT_BASE + 8)
#define EV_EFI_HANDOFF_TABLES               (EV_EFI_EVENT_BASE + 9)
#define EV_EFI_PLATFORM_FIRMWARE_BLOB2      (EV_EFI_EVENT_BASE + 0xA)
#define EV_EFI_HANDOFF_TABLES2              (EV_EFI_EVENT_BASE + 0xB)
#define EV_EFI_HCRTM_EVENT                  (EV_EFI_EVENT_BASE + 0x10)
#define EV_EFI_VARIABLE_AUTHORITY           (EV_EFI_EVENT_BASE + 0xE0)
#define EV_EFI_SPDM_FIRMWARE_BLOB           (EV_EFI_EVENT_BASE + 0xE1)
#define EV_EFI_SPDM_FIRMWARE_CONFIG         (EV_EFI_EVENT_BASE + 0xE2)

#define EFI_CALLING_EFI_APPLICATION         \
  "Calling EFI Application from Boot Option"
#define EFI_RETURNING_FROM_EFI_APPLICATION  \
  "Returning from EFI Application from Boot Option"
#define EFI_EXIT_BOOT_SERVICES_INVOCATION   \
  "Exit Boot Services Invocation"
#define EFI_EXIT_BOOT_SERVICES_FAILED       \
  "Exit Boot Services Returned with Failure"
#define EFI_EXIT_BOOT_SERVICES_SUCCEEDED    \
  "Exit Boot Services Returned with Success"


#define EV_POSTCODE_INFO_POST_CODE    "POST CODE"
#define POST_CODE_STR_LEN             (sizeof(EV_POSTCODE_INFO_POST_CODE) - 1)

#define EV_POSTCODE_INFO_SMM_CODE     "SMM CODE"
#define SMM_CODE_STR_LEN              (sizeof(EV_POSTCODE_INFO_SMM_CODE) - 1)

#define EV_POSTCODE_INFO_ACPI_DATA    "ACPI DATA"
#define ACPI_DATA_LEN                 (sizeof(EV_POSTCODE_INFO_ACPI_DATA) - 1)

#define EV_POSTCODE_INFO_BIS_CODE     "BIS CODE"
#define BIS_CODE_LEN                  (sizeof(EV_POSTCODE_INFO_BIS_CODE) - 1)

#define EV_POSTCODE_INFO_UEFI_PI      "UEFI PI"
#define UEFI_PI_LEN                   (sizeof(EV_POSTCODE_INFO_UEFI_PI) - 1)

#define EV_POSTCODE_INFO_OPROM        "Embedded Option ROM"
#define OPROM_LEN                     (sizeof(EV_POSTCODE_INFO_OPROM) - 1)

#define EV_POSTCODE_INFO_EMBEDDED_UEFI_DRIVER  "Embedded UEFI Driver"
#define EMBEDDED_UEFI_DRIVER_LEN               (sizeof(EV_POSTCODE_INFO_EMBEDDED_UEFI_DRIVER) - 1)

#define FIRMWARE_DEBUGGER_EVENT_STRING      "UEFI Debug Mode"
#define FIRMWARE_DEBUGGER_EVENT_STRING_LEN  (sizeof(FIRMWARE_DEBUGGER_EVENT_STRING) - 1)

//
// Set structure alignment to 1-byte
//
#pragma pack (1)

typedef UINT32                     TCG_EVENTTYPE;
typedef TPM_PCRINDEX               TCG_PCRINDEX;
typedef TPM_DIGEST                 TCG_DIGEST;
///
/// Event Log Entry Structure Definition
///
typedef struct tdTCG_PCR_EVENT {
  TCG_PCRINDEX                      PCRIndex;  ///< PCRIndex event extended to
  TCG_EVENTTYPE                     EventType; ///< TCG EFI event type
  TCG_DIGEST                        Digest;    ///< Value extended into PCRIndex
  UINT32                            EventSize; ///< Size of the event data
  UINT8                             Event[1];  ///< The event data
} TCG_PCR_EVENT;

#define TSS_EVENT_DATA_MAX_SIZE   256

///
/// TCG_PCR_EVENT_HDR
///
typedef struct tdTCG_PCR_EVENT_HDR {
  TCG_PCRINDEX                      PCRIndex;
  TCG_EVENTTYPE                     EventType;
  TCG_DIGEST                        Digest;
  UINT32                            EventSize;
} TCG_PCR_EVENT_HDR;

///
/// EFI_PLATFORM_FIRMWARE_BLOB
///
/// BlobLength should be of type UINTN but we use UINT64 here
/// because PEI is 32-bit while DXE is 64-bit on x64 platforms
///
typedef struct tdEFI_PLATFORM_FIRMWARE_BLOB {
  EFI_PHYSICAL_ADDRESS              BlobBase;
  UINT64                            BlobLength;
} EFI_PLATFORM_FIRMWARE_BLOB;

///
/// UEFI_PLATFORM_FIRMWARE_BLOB
///
/// This structure is used in EV_EFI_PLATFORM_FIRMWARE_BLOB
/// event to facilitate the measurement of firmware volume.
///
typedef struct tdUEFI_PLATFORM_FIRMWARE_BLOB {
  EFI_PHYSICAL_ADDRESS              BlobBase;
  UINT64                            BlobLength;
} UEFI_PLATFORM_FIRMWARE_BLOB;

///
/// UEFI_PLATFORM_FIRMWARE_BLOB2
///
/// This structure is used in EV_EFI_PLATFORM_FIRMWARE_BLOB2
/// event to facilitate the measurement of firmware volume.
///
typedef struct tdUEFI_PLATFORM_FIRMWARE_BLOB2 {
  UINT8                             BlobDescriptionSize;
//UINT8                             BlobDescription[BlobDescriptionSize];
//EFI_PHYSICAL_ADDRESS              BlobBase;
//UINT64                            BlobLength;
} UEFI_PLATFORM_FIRMWARE_BLOB2;

///
/// EFI_IMAGE_LOAD_EVENT
///
/// This structure is used in EV_EFI_BOOT_SERVICES_APPLICATION,
/// EV_EFI_BOOT_SERVICES_DRIVER and EV_EFI_RUNTIME_SERVICES_DRIVER
///
typedef struct tdEFI_IMAGE_LOAD_EVENT {
  EFI_PHYSICAL_ADDRESS              ImageLocationInMemory;
  UINTN                             ImageLengthInMemory;
  UINTN                             ImageLinkTimeAddress;
  UINTN                             LengthOfDevicePath;
  EFI_DEVICE_PATH_PROTOCOL          DevicePath[1];
} EFI_IMAGE_LOAD_EVENT;

///
/// UEFI_IMAGE_LOAD_EVENT
///
/// This structure is used in EV_EFI_BOOT_SERVICES_APPLICATION,
/// EV_EFI_BOOT_SERVICES_DRIVER and EV_EFI_RUNTIME_SERVICES_DRIVER
///
typedef struct tdUEFI_IMAGE_LOAD_EVENT {
  EFI_PHYSICAL_ADDRESS              ImageLocationInMemory;
  UINT64                            ImageLengthInMemory;
  UINT64                            ImageLinkTimeAddress;
  UINT64                            LengthOfDevicePath;
  EFI_DEVICE_PATH_PROTOCOL          DevicePath[1];
} UEFI_IMAGE_LOAD_EVENT;

///
/// EFI_HANDOFF_TABLE_POINTERS
///
/// This structure is used in EV_EFI_HANDOFF_TABLES event to facilitate
/// the measurement of given configuration tables.
///
typedef struct tdEFI_HANDOFF_TABLE_POINTERS {
  UINTN                             NumberOfTables;
  EFI_CONFIGURATION_TABLE           TableEntry[1];
} EFI_HANDOFF_TABLE_POINTERS;

///
/// UEFI_HANDOFF_TABLE_POINTERS
///
/// This structure is used in EV_EFI_HANDOFF_TABLES event to facilitate
/// the measurement of given configuration tables.
///
typedef struct tdUEFI_HANDOFF_TABLE_POINTERS {
  UINT64                            NumberOfTables;
  EFI_CONFIGURATION_TABLE           TableEntry[1];
} UEFI_HANDOFF_TABLE_POINTERS;

///
/// UEFI_HANDOFF_TABLE_POINTERS2
///
/// This structure is used in EV_EFI_HANDOFF_TABLES2 event to facilitate
/// the measurement of given configuration tables.
///
typedef struct tdUEFI_HANDOFF_TABLE_POINTERS2 {
  UINT8                             TableDescriptionSize;
//UINT8                             TableDescription[TableDescriptionSize];
//UINT64                            NumberOfTables;
//EFI_CONFIGURATION_TABLE           TableEntry[1];
} UEFI_HANDOFF_TABLE_POINTERS2;

///
/// EFI_VARIABLE_DATA
///
/// This structure serves as the header for measuring variables. The name of the
/// variable (in Unicode format) should immediately follow, then the variable
/// data.
/// This is defined in TCG EFI Platform Spec for TPM1.1 or 1.2 V1.22
///
typedef struct tdEFI_VARIABLE_DATA {
  EFI_GUID                          VariableName;
  UINTN                             UnicodeNameLength;
  UINTN                             VariableDataLength;
  CHAR16                            UnicodeName[1];
  INT8                              VariableData[1];  ///< Driver or platform-specific data
} EFI_VARIABLE_DATA;

///
/// UEFI_VARIABLE_DATA
///
/// This structure serves as the header for measuring variables. The name of the
/// variable (in Unicode format) should immediately follow, then the variable
/// data.
/// This is defined in TCG PC Client Firmware Profile Spec 00.21
///
typedef struct tdUEFI_VARIABLE_DATA {
  EFI_GUID                          VariableName;
  UINT64                            UnicodeNameLength;
  UINT64                            VariableDataLength;
  CHAR16                            UnicodeName[1];
  INT8                              VariableData[1];  ///< Driver or platform-specific data
} UEFI_VARIABLE_DATA;

//
// For TrEE1.0 compatibility
//
typedef struct {
  EFI_GUID                          VariableName;
  UINT64                            UnicodeNameLength;   // The TCG Definition used UINTN
  UINT64                            VariableDataLength;  // The TCG Definition used UINTN
  CHAR16                            UnicodeName[1];
  INT8                              VariableData[1];
} EFI_VARIABLE_DATA_TREE;

typedef struct tdEFI_GPT_DATA {
  EFI_PARTITION_TABLE_HEADER  EfiPartitionHeader;
  UINTN                       NumberOfPartitions;
  EFI_PARTITION_ENTRY         Partitions[1];
} EFI_GPT_DATA;

typedef struct tdUEFI_GPT_DATA {
  EFI_PARTITION_TABLE_HEADER  EfiPartitionHeader;
  UINT64                      NumberOfPartitions;
  EFI_PARTITION_ENTRY         Partitions[1];
} UEFI_GPT_DATA;

#define TCG_DEVICE_SECURITY_EVENT_DATA_SIGNATURE "SPDM Device Sec"
#define TCG_DEVICE_SECURITY_EVENT_DATA_VERSION   0

#define TCG_DEVICE_SECURITY_EVENT_DATA_DEVICE_TYPE_NULL  0
#define TCG_DEVICE_SECURITY_EVENT_DATA_DEVICE_TYPE_PCI   1
#define TCG_DEVICE_SECURITY_EVENT_DATA_DEVICE_TYPE_USB   2

///
/// TCG_DEVICE_SECURITY_EVENT_DATA_HEADER
/// This is the header of TCG_DEVICE_SECURITY_EVENT_DATA, which is
/// used in EV_EFI_SPDM_FIRMWARE_BLOB and EV_EFI_SPDM_FIRMWARE_CONFIG.
///
typedef struct {
  UINT8                          Signature[16];
  UINT16                         Version;
  UINT16                         Length;
  UINT32                         SpdmHashAlgo;
  UINT32                         DeviceType;
//SPDM_MEASUREMENT_BLOCK         SpdmMeasurementBlock;
} TCG_DEVICE_SECURITY_EVENT_DATA_HEADER;

#define TCG_DEVICE_SECURITY_EVENT_DATA_PCI_CONTEXT_VERSION  0

///
/// TCG_DEVICE_SECURITY_EVENT_DATA_PCI_CONTEXT
/// This is the PCI context data of TCG_DEVICE_SECURITY_EVENT_DATA, which is
/// used in EV_EFI_SPDM_FIRMWARE_BLOB and EV_EFI_SPDM_FIRMWARE_CONFIG.
///
typedef struct {
  UINT16  Version;
  UINT16  Length;
  UINT16  VendorId;
  UINT16  DeviceId;
  UINT8   RevisionID;
  UINT8   ClassCode[3];
  UINT16  SubsystemVendorID;
  UINT16  SubsystemID;
} TCG_DEVICE_SECURITY_EVENT_DATA_PCI_CONTEXT;

#define TCG_DEVICE_SECURITY_EVENT_DATA_USB_CONTEXT_VERSION  0

///
/// TCG_DEVICE_SECURITY_EVENT_DATA_USB_CONTEXT
/// This is the USB context data of TCG_DEVICE_SECURITY_EVENT_DATA, which is
/// used in EV_EFI_SPDM_FIRMWARE_BLOB and EV_EFI_SPDM_FIRMWARE_CONFIG.
///
typedef struct {
  UINT16  Version;
  UINT16  Length;
//UINT8   DeviceDescriptor[DescLen];
//UINT8   BodDescriptor[DescLen];
//UINT8   ConfigurationDescriptor[DescLen][NumOfConfiguration];
} TCG_DEVICE_SECURITY_EVENT_DATA_USB_CONTEXT;

//
// Crypto Agile Log Entry Format
//
typedef struct tdTCG_PCR_EVENT2 {
  TCG_PCRINDEX        PCRIndex;
  TCG_EVENTTYPE       EventType;
  TPML_DIGEST_VALUES  Digest;
  UINT32              EventSize;
  UINT8               Event[1];
} TCG_PCR_EVENT2;

//
// TCG PCR Event2 Header
// Follow TCG EFI Protocol Spec 5.2 Crypto Agile Log Entry Format
//
typedef struct tdTCG_PCR_EVENT2_HDR{
  TCG_PCRINDEX        PCRIndex;
  TCG_EVENTTYPE       EventType;
  TPML_DIGEST_VALUES  Digests;
  UINT32              EventSize;
} TCG_PCR_EVENT2_HDR;

//
// Log Header Entry Data
//
typedef struct {
  //
  // TCG defined hashing algorithm ID.
  //
  UINT16              algorithmId;
  //
  // The size of the digest for the respective hashing algorithm.
  //
  UINT16              digestSize;
} TCG_EfiSpecIdEventAlgorithmSize;

#define TCG_EfiSpecIDEventStruct_SIGNATURE_02 "Spec ID Event02"
#define TCG_EfiSpecIDEventStruct_SIGNATURE_03 "Spec ID Event03"

#define TCG_EfiSpecIDEventStruct_SPEC_VERSION_MAJOR_TPM12   1
#define TCG_EfiSpecIDEventStruct_SPEC_VERSION_MINOR_TPM12   2
#define TCG_EfiSpecIDEventStruct_SPEC_ERRATA_TPM12          2

#define TCG_EfiSpecIDEventStruct_SPEC_VERSION_MAJOR_TPM2   2
#define TCG_EfiSpecIDEventStruct_SPEC_VERSION_MINOR_TPM2   0
#define TCG_EfiSpecIDEventStruct_SPEC_ERRATA_TPM2          0
#define TCG_EfiSpecIDEventStruct_SPEC_ERRATA_TPM2_REV_105  105

typedef struct {
  UINT8               signature[16];
  //
  // The value for the Platform Class.
  // The enumeration is defined in the TCG ACPI Specification Client Common Header.
  //
  UINT32              platformClass;
  //
  // The TCG EFI Platform Specification minor version number this BIOS supports.
  // Any BIOS supporting version (1.22) MUST set this value to 02h.
  // Any BIOS supporting version (2.0) SHALL set this value to 0x00.
  //
  UINT8               specVersionMinor;
  //
  // The TCG EFI Platform Specification major version number this BIOS supports.
  // Any BIOS supporting version (1.22) MUST set this value to 01h.
  // Any BIOS supporting version (2.0) SHALL set this value to 0x02.
  //
  UINT8               specVersionMajor;
  //
  // The TCG EFI Platform Specification errata for this specification this BIOS supports.
  // Any BIOS supporting version and errata (1.22) MUST set this value to 02h.
  // Any BIOS supporting version and errata (2.0) SHALL set this value to 0x00.
  //
  UINT8               specErrata;
  //
  // Specifies the size of the UINTN fields used in various data structures used in this specification.
  // 0x01 indicates UINT32 and 0x02 indicates UINT64.
  //
  UINT8               uintnSize;
  //
  // This field is added in "Spec ID Event03".
  // The number of hashing algorithms used in this event log (except the first event).
  // All events in this event log use all hashing algorithms defined here.
  //
//UINT32              numberOfAlgorithms;
  //
  // This field is added in "Spec ID Event03".
  // An array of size numberOfAlgorithms of value pairs.
  //
//TCG_EfiSpecIdEventAlgorithmSize digestSize[numberOfAlgorithms];
  //
  // Size in bytes of the VendorInfo field.
  // Maximum value SHALL be FFh bytes.
  //
//UINT8               vendorInfoSize;
  //
  // Provided for use by the BIOS implementer.
  // The value might be used, for example, to provide more detailed information about the specific BIOS such as BIOS revision numbers, etc.
  // The values within this field are not standardized and are implementer-specific.
  // Platform-specific or -unique information SHALL NOT be provided in this field.
  //
//UINT8               vendorInfo[vendorInfoSize];
} TCG_EfiSpecIDEventStruct;

typedef struct tdTCG_PCClientTaggedEvent {
  UINT32              taggedEventID;
  UINT32              taggedEventDataSize;
//UINT8               taggedEventData[taggedEventDataSize];
} TCG_PCClientTaggedEvent;

#define TCG_Sp800_155_PlatformId_Event_SIGNATURE  "SP800-155 Event"
#define TCG_Sp800_155_PlatformId_Event2_SIGNATURE "SP800-155 Event2"

typedef struct tdTCG_Sp800_155_PlatformId_Event2 {
  UINT8               Signature[16];
  //
  // Where Vendor ID is an integer defined
  // at http://www.iana.org/assignments/enterprisenumbers
  //
  UINT32              VendorId;
  //
  // 16-byte identifier of a given platform's static configuration of code
  //
  EFI_GUID            ReferenceManifestGuid;
  //
  // Below structure is newly added in TCG_Sp800_155_PlatformId_Event2.
  //
//UINT8               PlatformManufacturerStrSize;
//UINT8               PlatformManufacturerStr[PlatformManufacturerStrSize];
//UINT8               PlatformModelSize;
//UINT8               PlatformModel[PlatformModelSize];
//UINT8               PlatformVersionSize;
//UINT8               PlatformVersion[PlatformVersionSize];
//UINT8               PlatformModelSize;
//UINT8               PlatformModel[PlatformModelSize];
//UINT8               FirmwareManufacturerStrSize;
//UINT8               FirmwareManufacturerStr[FirmwareManufacturerStrSize];
//UINT32              FirmwareManufacturerId;
//UINT8               FirmwareVersion;
//UINT8               FirmwareVersion[FirmwareVersionSize]];
} TCG_Sp800_155_PlatformId_Event2;

#define TCG_EfiStartupLocalityEvent_SIGNATURE      "StartupLocality"


//
// The Locality Indicator which sent the TPM2_Startup command
//
#define LOCALITY_0_INDICATOR        0x00
#define LOCALITY_3_INDICATOR        0x03

//
// Startup Locality Event
//
typedef struct tdTCG_EfiStartupLocalityEvent{
  UINT8       Signature[16];
  //
  // The Locality Indicator which sent the TPM2_Startup command
  //
  UINT8       StartupLocality;
} TCG_EfiStartupLocalityEvent;


//
// Restore original structure alignment
//
#pragma pack ()

#endif


