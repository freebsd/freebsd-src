/** @file
  SMBIOS Protocol as defined in PI1.2 Specification VOLUME 5 Standard.

  SMBIOS protocol allows consumers to log SMBIOS data records, and enables the producer
  to create the SMBIOS tables for a platform.

  This protocol provides an interface to add, remove or discover SMBIOS records. The driver which
  produces this protocol is responsible for creating the SMBIOS data tables and installing the pointer
  to the tables in the EFI System Configuration Table.
  The caller is responsible for only adding SMBIOS records that are valid for the SMBIOS
  MajorVersion and MinorVersion. When an enumerated SMBIOS field's values are
  controlled by the DMTF, new values can be used as soon as they are defined by the DMTF without
  requiring an update to MajorVersion and MinorVersion.
  The SMBIOS protocol can only be called a TPL < TPL_NOTIFY.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SMBIOS_PROTOCOL_H__
#define __SMBIOS_PROTOCOL_H__

#include <IndustryStandard/SmBios.h>

#define EFI_SMBIOS_PROTOCOL_GUID \
    { 0x3583ff6, 0xcb36, 0x4940, { 0x94, 0x7e, 0xb9, 0xb3, 0x9f, 0x4a, 0xfa, 0xf7 }}

#define EFI_SMBIOS_TYPE_BIOS_INFORMATION                     SMBIOS_TYPE_BIOS_INFORMATION
#define EFI_SMBIOS_TYPE_SYSTEM_INFORMATION                   SMBIOS_TYPE_SYSTEM_INFORMATION
#define EFI_SMBIOS_TYPE_BASEBOARD_INFORMATION                SMBIOS_TYPE_BASEBOARD_INFORMATION
#define EFI_SMBIOS_TYPE_SYSTEM_ENCLOSURE                     SMBIOS_TYPE_SYSTEM_ENCLOSURE
#define EFI_SMBIOS_TYPE_PROCESSOR_INFORMATION                SMBIOS_TYPE_PROCESSOR_INFORMATION
#define EFI_SMBIOS_TYPE_MEMORY_CONTROLLER_INFORMATION        SMBIOS_TYPE_MEMORY_CONTROLLER_INFORMATION
#define EFI_SMBIOS_TYPE_MEMORY_MODULE_INFORMATON             SMBIOS_TYPE_MEMORY_MODULE_INFORMATON
#define EFI_SMBIOS_TYPE_CACHE_INFORMATION                    SMBIOS_TYPE_CACHE_INFORMATION
#define EFI_SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION           SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION
#define EFI_SMBIOS_TYPE_SYSTEM_SLOTS                         SMBIOS_TYPE_SYSTEM_SLOTS
#define EFI_SMBIOS_TYPE_ONBOARD_DEVICE_INFORMATION           SMBIOS_TYPE_ONBOARD_DEVICE_INFORMATION
#define EFI_SMBIOS_TYPE_OEM_STRINGS                          SMBIOS_TYPE_OEM_STRINGS
#define EFI_SMBIOS_TYPE_SYSTEM_CONFIGURATION_OPTIONS         SMBIOS_TYPE_SYSTEM_CONFIGURATION_OPTIONS
#define EFI_SMBIOS_TYPE_BIOS_LANGUAGE_INFORMATION            SMBIOS_TYPE_BIOS_LANGUAGE_INFORMATION
#define EFI_SMBIOS_TYPE_GROUP_ASSOCIATIONS                   SMBIOS_TYPE_GROUP_ASSOCIATIONS
#define EFI_SMBIOS_TYPE_SYSTEM_EVENT_LOG                     SMBIOS_TYPE_SYSTEM_EVENT_LOG
#define EFI_SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY                SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY
#define EFI_SMBIOS_TYPE_MEMORY_DEVICE                        SMBIOS_TYPE_MEMORY_DEVICE
#define EFI_SMBIOS_TYPE_32BIT_MEMORY_ERROR_INFORMATION       SMBIOS_TYPE_32BIT_MEMORY_ERROR_INFORMATION
#define EFI_SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS          SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS
#define EFI_SMBIOS_TYPE_MEMORY_DEVICE_MAPPED_ADDRESS         SMBIOS_TYPE_MEMORY_DEVICE_MAPPED_ADDRESS
#define EFI_SMBIOS_TYPE_BUILT_IN_POINTING_DEVICE             SMBIOS_TYPE_BUILT_IN_POINTING_DEVICE
#define EFI_SMBIOS_TYPE_PORTABLE_BATTERY                     SMBIOS_TYPE_PORTABLE_BATTERY
#define EFI_SMBIOS_TYPE_SYSTEM_RESET                         SMBIOS_TYPE_SYSTEM_RESET
#define EFI_SMBIOS_TYPE_HARDWARE_SECURITY                    SMBIOS_TYPE_HARDWARE_SECURITY
#define EFI_SMBIOS_TYPE_SYSTEM_POWER_CONTROLS                SMBIOS_TYPE_SYSTEM_POWER_CONTROLS
#define EFI_SMBIOS_TYPE_VOLTAGE_PROBE                        SMBIOS_TYPE_VOLTAGE_PROBE
#define EFI_SMBIOS_TYPE_COOLING_DEVICE                       SMBIOS_TYPE_COOLING_DEVICE
#define EFI_SMBIOS_TYPE_TEMPERATURE_PROBE                    SMBIOS_TYPE_TEMPERATURE_PROBE
#define EFI_SMBIOS_TYPE_ELECTRICAL_CURRENT_PROBE             SMBIOS_TYPE_ELECTRICAL_CURRENT_PROBE
#define EFI_SMBIOS_TYPE_OUT_OF_BAND_REMOTE_ACCESS            SMBIOS_TYPE_OUT_OF_BAND_REMOTE_ACCESS
#define EFI_SMBIOS_TYPE_BOOT_INTEGRITY_SERVICE               SMBIOS_TYPE_BOOT_INTEGRITY_SERVICE
#define EFI_SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION              SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION
#define EFI_SMBIOS_TYPE_64BIT_MEMORY_ERROR_INFORMATION       SMBIOS_TYPE_64BIT_MEMORY_ERROR_INFORMATION
#define EFI_SMBIOS_TYPE_MANAGEMENT_DEVICE                    SMBIOS_TYPE_MANAGEMENT_DEVICE
#define EFI_SMBIOS_TYPE_MANAGEMENT_DEVICE_COMPONENT          SMBIOS_TYPE_MANAGEMENT_DEVICE_COMPONENT
#define EFI_SMBIOS_TYPE_MANAGEMENT_DEVICE_THRESHOLD_DATA     SMBIOS_TYPE_MANAGEMENT_DEVICE_THRESHOLD_DATA
#define EFI_SMBIOS_TYPE_MEMORY_CHANNEL                       SMBIOS_TYPE_MEMORY_CHANNEL
#define EFI_SMBIOS_TYPE_IPMI_DEVICE_INFORMATION              SMBIOS_TYPE_IPMI_DEVICE_INFORMATION
#define EFI_SMBIOS_TYPE_SYSTEM_POWER_SUPPLY                  SMBIOS_TYPE_SYSTEM_POWER_SUPPLY
#define EFI_SMBIOS_TYPE_ADDITIONAL_INFORMATION               SMBIOS_TYPE_ADDITIONAL_INFORMATION
#define EFI_SMBIOS_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION SMBIOS_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION
#define EFI_SMBIOS_TYPE_MANAGEMENT_CONTROLLER_HOST_INTERFACE SMBIOS_TYPE_MANAGEMENT_CONTROLLER_HOST_INTERFACE
#define EFI_SMBIOS_TYPE_INACTIVE                             SMBIOS_TYPE_INACTIVE
#define EFI_SMBIOS_TYPE_END_OF_TABLE                         SMBIOS_TYPE_END_OF_TABLE
#define EFI_SMBIOS_OEM_BEGIN                                 SMBIOS_OEM_BEGIN
#define EFI_SMBIOS_OEM_END                                   SMBIOS_OEM_END

typedef SMBIOS_TABLE_STRING EFI_SMBIOS_STRING;
typedef SMBIOS_TYPE         EFI_SMBIOS_TYPE;
typedef SMBIOS_HANDLE       EFI_SMBIOS_HANDLE;
typedef SMBIOS_STRUCTURE    EFI_SMBIOS_TABLE_HEADER;

typedef struct _EFI_SMBIOS_PROTOCOL EFI_SMBIOS_PROTOCOL;

/**
  Add an SMBIOS record.

  This function allows any agent to add SMBIOS records. The caller is responsible for ensuring
  Record is formatted in a way that matches the version of the SMBIOS specification as defined in
  the MajorRevision and MinorRevision fields of the EFI_SMBIOS_PROTOCOL.
  Record must follow the SMBIOS structure evolution and usage guidelines in the SMBIOS
  specification. Record starts with the formatted area of the SMBIOS structure and the length is
  defined by EFI_SMBIOS_TABLE_HEADER.Length. Each SMBIOS structure is terminated by a
  double-null (0x0000), either directly following the formatted area (if no strings are present) or
  directly following the last string. The number of optional strings is not defined by the formatted area,
  but is fixed by the call to Add(). A string can be a place holder, but it must not be a NULL string as
  two NULL strings look like the double-null that terminates the structure.

  @param[in]        This                The EFI_SMBIOS_PROTOCOL instance.
  @param[in]        ProducerHandle      The handle of the controller or driver associated with the SMBIOS information. NULL means no handle.
  @param[in, out]   SmbiosHandle        On entry, the handle of the SMBIOS record to add. If FFFEh, then a unique handle
                                        will be assigned to the SMBIOS record. If the SMBIOS handle is already in use,
                                        EFI_ALREADY_STARTED is returned and the SMBIOS record is not updated.
  @param[in]        Record              The data for the fixed portion of the SMBIOS record. The format of the record is
                                        determined by EFI_SMBIOS_TABLE_HEADER.Type. The size of the formatted
                                        area is defined by EFI_SMBIOS_TABLE_HEADER.Length and either followed
                                        by a double-null (0x0000) or a set of null terminated strings and a null.

  @retval EFI_SUCCESS                   Record was added.
  @retval EFI_OUT_OF_RESOURCES          Record was not added.
  @retval EFI_ALREADY_STARTED           The SmbiosHandle passed in was already in use.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMBIOS_ADD)(
  IN CONST      EFI_SMBIOS_PROTOCOL     *This,
  IN            EFI_HANDLE              ProducerHandle OPTIONAL,
  IN OUT        EFI_SMBIOS_HANDLE       *SmbiosHandle,
  IN            EFI_SMBIOS_TABLE_HEADER *Record
);

/**
  Update the string associated with an existing SMBIOS record.

  This function allows the update of specific SMBIOS strings. The number of valid strings for any
  SMBIOS record is defined by how many strings were present when Add() was called.

  @param[in]    This            The EFI_SMBIOS_PROTOCOL instance.
  @param[in]    SmbiosHandle    SMBIOS Handle of structure that will have its string updated.
  @param[in]    StringNumber    The non-zero string number of the string to update.
  @param[in]    String          Update the StringNumber string with String.

  @retval EFI_SUCCESS           SmbiosHandle had its StringNumber String updated.
  @retval EFI_INVALID_PARAMETER SmbiosHandle does not exist.
  @retval EFI_UNSUPPORTED       String was not added because it is longer than the SMBIOS Table supports.
  @retval EFI_NOT_FOUND         The StringNumber.is not valid for this SMBIOS record.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMBIOS_UPDATE_STRING)(
   IN CONST EFI_SMBIOS_PROTOCOL *This,
   IN       EFI_SMBIOS_HANDLE   *SmbiosHandle,
   IN       UINTN               *StringNumber,
   IN       CHAR8               *String
);

/**
  Remove an SMBIOS record.

  This function removes an SMBIOS record using the handle specified by SmbiosHandle.

  @param[in]    This                The EFI_SMBIOS_PROTOCOL instance.
  @param[in]    SmbiosHandle        The handle of the SMBIOS record to remove.

  @retval EFI_SUCCESS               SMBIOS record was removed.
  @retval EFI_INVALID_PARAMETER     SmbiosHandle does not specify a valid SMBIOS record.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMBIOS_REMOVE)(
   IN CONST EFI_SMBIOS_PROTOCOL *This,
   IN       EFI_SMBIOS_HANDLE   SmbiosHandle
);

/**
  Allow the caller to discover all or some of the SMBIOS records.

  This function allows all of the SMBIOS records to be discovered. It's possible to find
  only the SMBIOS records that match the optional Type argument.

  @param[in]        This            The EFI_SMBIOS_PROTOCOL instance.
  @param[in, out]   SmbiosHandle    On entry, points to the previous handle of the SMBIOS record. On exit, points to the
                                    next SMBIOS record handle. If it is FFFEh on entry, then the first SMBIOS record
                                    handle will be returned. If it returns FFFEh on exit, then there are no more SMBIOS records.
  @param[in]        Type            On entry, it points to the type of the next SMBIOS record to return. If NULL, it
                                    indicates that the next record of any type will be returned. Type is not
                                    modified by the this function.
  @param[out]       Record          On exit, points to a pointer to the the SMBIOS Record consisting of the formatted area
                                    followed by the unformatted area. The unformatted area optionally contains text strings.
  @param[out]       ProducerHandle  On exit, points to the ProducerHandle registered by Add(). If no
                                    ProducerHandle was passed into Add() NULL is returned. If a NULL pointer is
                                    passed in no data will be returned.
  @retval EFI_SUCCESS               SMBIOS record information was successfully returned in Record.
                                    SmbiosHandle is the handle of the current SMBIOS record
  @retval EFI_NOT_FOUND             The SMBIOS record with SmbiosHandle was the last available record.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMBIOS_GET_NEXT)(
   IN     CONST EFI_SMBIOS_PROTOCOL     *This,
   IN OUT       EFI_SMBIOS_HANDLE       *SmbiosHandle,
   IN           EFI_SMBIOS_TYPE         *Type              OPTIONAL,
   OUT          EFI_SMBIOS_TABLE_HEADER **Record,
   OUT          EFI_HANDLE              *ProducerHandle    OPTIONAL
);

struct _EFI_SMBIOS_PROTOCOL {
  EFI_SMBIOS_ADD           Add;
  EFI_SMBIOS_UPDATE_STRING UpdateString;
  EFI_SMBIOS_REMOVE        Remove;
  EFI_SMBIOS_GET_NEXT      GetNext;
  UINT8                    MajorVersion;    ///< The major revision of the SMBIOS specification supported.
  UINT8                    MinorVersion;    ///< The minor revision of the SMBIOS specification supported.
};

extern EFI_GUID gEfiSmbiosProtocolGuid;

#endif // __SMBIOS_PROTOCOL_H__
