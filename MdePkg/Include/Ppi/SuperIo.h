/** @file
  This PPI provides the super I/O register access functionality.

  Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is from PI Version 1.2.1.

**/

#ifndef __EFI_SUPER_IO_PPI_H__
#define __EFI_SUPER_IO_PPI_H__

#include <Protocol/SuperIo.h>

#define EFI_SIO_PPI_GUID \
  { \
    0x23a464ad, 0xcb83, 0x48b8, {0x94, 0xab, 0x1a, 0x6f, 0xef, 0xcf, 0xe5, 0x22} \
  }

typedef struct _EFI_SIO_PPI EFI_SIO_PPI;
typedef struct _EFI_SIO_PPI *PEFI_SIO_PPI;

typedef UINT16 EFI_SIO_REGISTER;
#define EFI_SIO_REG(ldn,reg)    (EFI_SIO_REGISTER) (((ldn) << 8) | reg)
#define EFI_SIO_LDN_GLOBAL      0xFF

/**
  Read a Super I/O register.

  The register is specified as an 8-bit logical device number and an 8-bit
  register value. The logical device numbers for specific SIO devices can be
  determined using the Info member of the PPI structure.

  @param PeiServices  A pointer to a pointer to the PEI Services.
  @param This         A pointer to this instance of the EFI_SIO_PPI.
  @param ExitCfgMode  A boolean specifying whether the driver should turn on
                      configuration mode (FALSE) or turn off configuration mode
                      (TRUE) after completing the read operation. The driver must
                      track the current state of the configuration mode (if any)
                      and turn on configuration mode (if necessary) prior to
                      register access.
  @param Register     A value specifying the logical device number (bits 15:8)
                      and the register to read (bits 7:0). The logical device
                      number of EFI_SIO_LDN_GLOBAL indicates that global
                      registers will be used.
  @param IoData       A pointer to the returned register value.

  @retval EFI_SUCCESS            Success.
  @retval EFI_TIMEOUT            The register could not be read in the a reasonable
                                 amount of time. The exact time is device-specific.
  @retval EFI_INVALID_PARAMETERS Register was out of range for this device.
  @retval EFI_INVALID_PARAMETERS IoData was NULL
  @retval EFI_DEVICE_ERROR       There was a device fault or the device was not present.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_SIO_REGISTER_READ)(
  IN EFI_PEI_SERVICES       **PeiServices,
  IN CONST EFI_SIO_PPI      *This,
  IN BOOLEAN                ExitCfgMode,
  IN EFI_SIO_REGISTER       Register,
  OUT UINT8                 *IoData
  );

/**
  Write a Super I/O register.

  The register is specified as an 8-bit logical device number and an 8-bit register
  value. The logical device numbers for specific SIO devices can be determined
  using the Info member of the PPI structure.

  @param PeiServices  A pointer to a pointer to the PEI Services.
  @param This         A pointer to this instance of the EFI_SIO_PPI.
  @param ExitCfgMode  A boolean specifying whether the driver should turn on
                      configuration mode (FALSE) or turn off configuration mode
                      (TRUE) after completing the read operation. The driver must
                      track the current state of the configuration mode (if any)
                      and turn on configuration mode (if necessary) prior to
                      register access.
  @param Register     A value specifying the logical device number (bits 15:8)
                      and the register to read (bits 7:0). The logical device
                      number of EFI_SIO_LDN_GLOBAL indicates that global
                      registers will be used.
  @param IoData       A pointer to the returned register value.

  @retval EFI_SUCCESS            Success.
  @retval EFI_TIMEOUT            The register could not be read in the a reasonable
                                 amount of time. The exact time is device-specific.
  @retval EFI_INVALID_PARAMETERS Register was out of range for this device.
  @retval EFI_INVALID_PARAMETERS IoData was NULL
  @retval EFI_DEVICE_ERROR       There was a device fault or the device was not present.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_SIO_REGISTER_WRITE)(
  IN EFI_PEI_SERVICES       **PeiServices,
  IN CONST EFI_SIO_PPI      *This,
  IN BOOLEAN                ExitCfgMode,
  IN EFI_SIO_REGISTER       Register,
  IN UINT8                  IoData
  );

/**
  Provides an interface for a table based programming of the Super I/O registers.

  The Modify() function provides an interface for table based programming of the
  Super I/O registers. This function can be used to perform programming of
  multiple Super I/O registers with a single function call. For each table entry,
  the Register is read, its content is bitwise ANDed with AndMask, and then ORed
  with OrMask before being written back to the Register. The Super I/O driver
  must track the current state of the Super I/O and enable the configuration mode
  of Super I/O if necessary prior to table processing. Once the table is processed,
  the Super I/O device must be returned to the original state.

  @param PeiServices      A pointer to a pointer to the PEI Services.
  @param This             A pointer to this instance of the EFI_SIO_PPI.
  @param Command          A pointer to an array of NumberOfCommands EFI_SIO_REGISTER_MODIFY
                          structures. Each structure specifies a single Super I/O register
                          modify operation.
  @param NumberOfCommands The number of elements in the Command array.

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_INVALID_PARAMETERS  Command is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_SIO_REGISTER_MODIFY)(
  IN EFI_PEI_SERVICES              **PeiServices,
  IN CONST EFI_SIO_PPI             *This,
  IN CONST EFI_SIO_REGISTER_MODIFY *Command,
  IN UINTN                         NumberOfCommands
  );

///
/// Specifies the end of the information list.
///
#define EFI_ACPI_PNP_HID_END       EFI_PNP_ID (0x0000)

typedef UINT32                     EFI_ACPI_HID;
typedef UINT32                     EFI_ACPI_UID;
#pragma pack(1)
typedef struct _EFI_SIO_INFO {
  EFI_ACPI_HID                     Hid;
  EFI_ACPI_UID                     Uid;
  UINT8                            Ldn;
} EFI_SIO_INFO, *PEFI_SIO_INFO;
#pragma pack()

///
/// This PPI provides low-level access to Super I/O registers using Read() and
/// Write(). It also uniquely identifies this Super I/O controller using a GUID
/// and provides mappings between ACPI style PNP IDs and the logical device numbers.
/// There is one instance of this PPI per Super I/O device.
///
struct _EFI_SIO_PPI {
  ///
  /// This function reads a register's value from the Super I/O controller.
  ///
  EFI_PEI_SIO_REGISTER_READ   Read;
  ///
  /// This function writes a value to a register in the Super I/O controller.
  ///
  EFI_PEI_SIO_REGISTER_WRITE  Write;
  ///
  /// This function modifies zero or more registers in the Super I/O controller
  /// using a table.
  ///
  EFI_PEI_SIO_REGISTER_MODIFY Modify;
  ///
  /// This GUID uniquely identifies the Super I/O controller.
  ///
  EFI_GUID                    SioGuid;
  ///
  /// This pointer is to an array which maps EISA identifiers to logical devices numbers.
  ///
  PEFI_SIO_INFO               Info;
};

extern EFI_GUID gEfiSioPpiGuid;

#endif
