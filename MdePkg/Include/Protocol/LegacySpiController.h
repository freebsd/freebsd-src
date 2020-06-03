/** @file
  This file defines the Legacy SPI Controller Protocol.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    This Protocol was introduced in UEFI PI Specification 1.6.

**/

#ifndef __LEGACY_SPI_CONTROLLER_PROTOCOL_H__
#define __LEGACY_SPI_CONTROLLER_PROTOCOL_H__

///
/// Note: The UEFI PI 1.6 specification uses the character 'l' in the GUID
///       definition. This definition assumes it was supposed to be '1'.
///
/// Global ID for the Legacy SPI Controller Protocol
///
#define EFI_LEGACY_SPI_CONTROLLER_GUID  \
  { 0x39136fc7, 0x1a11, 0x49de,         \
    { 0xbf, 0x35, 0x0e, 0x78, 0xdd, 0xb5, 0x24, 0xfc }}

typedef
struct _EFI_LEGACY_SPI_CONTROLLER_PROTOCOL
EFI_LEGACY_SPI_CONTROLLER_PROTOCOL;

/**
  Set the erase block opcode.

  This routine must be called at or below TPL_NOTIFY.
  The menu table contains SPI transaction opcodes which are accessible after
  the legacy SPI flash controller's configuration is locked. The board layer
  specifies the erase block size for the SPI NOR flash part. The SPI NOR flash
  peripheral driver selects the erase block opcode which matches the erase
  block size and uses this API to load the opcode into the opcode menu table.

  @param[in] This              Pointer to an EFI_LEGACY_SPI_CONTROLLER_PROTOCOL
                               structure.
  @param[in] EraseBlockOpcode  Erase block opcode to be placed into the opcode
                               menu table.

  @retval EFI_SUCCESS       The opcode menu table was updated
  @retval EFI_ACCESS_ERROR  The SPI controller is locked

**/
typedef EFI_STATUS
(EFIAPI *EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_ERASE_BLOCK_OPCODE) (
  IN CONST EFI_LEGACY_SPI_CONTROLLER_PROTOCOL  *This,
  IN UINT8                                     EraseBlockOpcode
  );

/**
  Set the write status prefix opcode.

  This routine must be called at or below TPL_NOTIFY.
  The prefix table contains SPI transaction write prefix opcodes which are
  accessible after the legacy SPI flash controller's configuration is locked.
  The board layer specifies the write status prefix opcode for the SPI NOR
  flash part. The SPI NOR flash peripheral driver uses this API to load the
  opcode into the prefix table.

  @param[in] This               Pointer to an
                                EFI_LEGACY_SPI_CONTROLLER_PROTOCOL structure.
  @param[in] WriteStatusPrefix  Prefix opcode for the write status command.

  @retval EFI_SUCCESS       The prefix table was updated
  @retval EFI_ACCESS_ERROR  The SPI controller is locked

**/
typedef
EFI_STATUS
(EFIAPI *EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_WRITE_STATUS_PREFIX) (
  IN CONST EFI_LEGACY_SPI_CONTROLLER_PROTOCOL  *This,
  IN UINT8                                     WriteStatusPrefix
  );

/**
  Set the BIOS base address.

  This routine must be called at or below TPL_NOTIFY.
  The BIOS base address works with the protect range registers to protect
  portions of the SPI NOR flash from erase and write operat ions. The BIOS
  calls this API prior to passing control to the OS loader.

  @param[in] This             Pointer to an EFI_LEGACY_SPI_CONTROLLER_PROTOCOL
                              structure.
  @param[in] BiosBaseAddress  The BIOS base address.

  @retval EFI_SUCCESS            The BIOS base address was properly set
  @retval EFI_ACCESS_ERROR       The SPI controller is locked
  @retval EFI_INVALID_PARAMETER  The BIOS base address is greater than
                                 This->Maxi.mumOffset
  @retval EFI_UNSUPPORTED        The BIOS base address was already set

**/
typedef EFI_STATUS
(EFIAPI *EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_BIOS_BASE_ADDRESS) (
  IN CONST EFI_LEGACY_SPI_CONTROLLER_PROTOCOL  *This,
  IN UINT32 BiosBaseAddress
  );

/**
  Clear the SPI protect range registers.

  This routine must be called at or below TPL_NOTIFY.
  The BIOS uses this routine to set an initial condition on the SPI protect
  range registers.

  @param[in] This  Pointer to an EFI_LEGACY_SPI_CONTROLLER_PROTOCOL structure.

  @retval EFI_SUCCESS       The registers were successfully cleared
  @retval EFI_ACCESS_ERROR  The SPI controller is locked

**/
typedef
EFI_STATUS
(EFIAPI *EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_CLEAR_SPI_PROTECT) (
  IN CONST EFI_LEGACY_SPI_CONTROLLER_PROTOCOL  *This
  );

/**
  Determine if the SPI range is protected.

  This routine must be called at or below TPL_NOTIFY.
  The BIOS uses this routine to verify a range in the SPI is protected.

  @param[in] This            Pointer to an EFI_LEGACY_SPI_CONTROLLER_PROTOCOL
                             structure.
  @param[in] BiosAddress     Address within a 4 KiB block to start protecting.
  @param[in] BytesToProtect  The number of 4 KiB blocks to protect.

  @retval TRUE   The range is protected
  @retval FALSE  The range is not protected

**/
typedef
BOOLEAN
(EFIAPI *EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_IS_RANGE_PROTECTED) (
  IN CONST EFI_LEGACY_SPI_CONTROLLER_PROTOCOL  *This,
  IN UINT32                                    BiosAddress,
  IN UINT32                                    BlocksToProtect
  );

/**
  Set the next protect range register.

  This routine must be called at or below TPL_NOTIFY.
  The BIOS sets the protect range register to prevent write and erase
  operations to a portion of the SPI NOR flash device.

  @param[in] This             Pointer to an EFI_LEGACY_SPI_CONTROLLER_PROTOCOL
                              structure.
  @param[in] BiosAddress      Address within a 4 KiB block to start protecting.
  @param[in] BlocksToProtect  The number of 4 KiB blocks to protect.

  @retval EFI_SUCCESS            The register was successfully updated
  @retval EFI_ACCESS_ERROR       The SPI controller is locked
  @retval EFI_INVALID_PARAMETER  BiosAddress < This->BiosBaseAddress, or
                                 BlocksToProtect * 4 KiB
                                   > This->MaximumRangeBytes, or
                                 BiosAddress - This->BiosBaseAddress
                                   + (BlocksToProtect * 4 KiB)
                                     > This->MaximumRangeBytes
  @retval EFI_OUT_OF_RESOURCES  No protect range register available
  @retval EFI_UNSUPPORTED       Call This->SetBaseAddress because the BIOS base
                                address is not set

**/
typedef
EFI_STATUS
(EFIAPI *EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_PROTECT_NEXT_RANGE) (
  IN CONST EFI_LEGACY_SPI_CONTROLLER_PROTOCOL  *This,
  IN UINT32                                    BiosAddress,
  IN UINT32                                    BlocksToProtect
  );

/**
  Lock the SPI controller configuration.

  This routine must be called at or below TPL_NOTIFY.
  This routine locks the SPI controller's configuration so that the software
  is no longer able to update:
  * Prefix table
  * Opcode menu
  * Opcode type table
  * BIOS base address
  * Protect range registers

  @param[in] This  Pointer to an EFI_LEGACY_SPI_CONTROLLER_PROTOCOL structure.

  @retval EFI_SUCCESS          The SPI controller was successfully locked
  @retval EFI_ALREADY_STARTED  The SPI controller was already locked

**/
typedef EFI_STATUS
(EFIAPI *EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_LOCK_CONTROLLER) (
  IN CONST EFI_LEGACY_SPI_CONTROLLER_PROTOCOL  *This
  );

///
/// Support the extra features of the legacy SPI flash controller.
///
struct _EFI_LEGACY_SPI_CONTROLLER_PROTOCOL {
  ///
  /// Maximum offset from the BIOS base address that is able to be protected.
  ///
  UINT32                                                 MaximumOffset;

  ///
  /// Maximum number of bytes that can be protected by one range register.
  ///
  UINT32                                                 MaximumRangeBytes;

  ///
  /// The number of registers available for protecting the BIOS.
  ///
  UINT32                                                 RangeRegisterCount;

  ///
  /// Set the erase block opcode.
  ///
  EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_ERASE_BLOCK_OPCODE  EraseBlockOpcode;

  ///
  /// Set the write status prefix opcode.
  ///
  EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_WRITE_STATUS_PREFIX WriteStatusPrefix;

  ///
  /// Set the BIOS base address.
  ///
  EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_BIOS_BASE_ADDRESS   BiosBaseAddress;

  ///
  /// Clear the SPI protect range registers.
  ///
  EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_CLEAR_SPI_PROTECT   ClearSpiProtect;

  ///
  /// Determine if the SPI range is protected.
  ///
  EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_IS_RANGE_PROTECTED  IsRangeProtected;

  ///
  /// Set the next protect range register.
  ///
  EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_PROTECT_NEXT_RANGE  ProtectNextRange;

  ///
  /// Lock the SPI controller configuration.
  ///
  EFI_LEGACY_SPI_CONTROLLER_PROTOCOL_LOCK_CONTROLLER     LockController;
};

extern EFI_GUID gEfiLegacySpiControllerProtocolGuid;

#endif // __LEGACY_SPI_CONTROLLER_PROTOCOL_H__
