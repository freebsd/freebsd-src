/** @file
  This file defines the SPI NOR Flash Protocol.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    This Protocol was introduced in UEFI PI Specification 1.6.

**/

#ifndef __SPI_NOR_FLASH_PROTOCOL_H__
#define __SPI_NOR_FLASH_PROTOCOL_H__

#include <Protocol/SpiConfiguration.h>

///
/// Global ID for the SPI NOR Flash Protocol
///
#define EFI_SPI_NOR_FLASH_PROTOCOL_GUID  \
  { 0xb57ec3fe, 0xf833, 0x4ba6,          \
    { 0x85, 0x78, 0x2a, 0x7d, 0x6a, 0x87, 0x44, 0x4b }}

typedef struct _EFI_SPI_NOR_FLASH_PROTOCOL EFI_SPI_NOR_FLASH_PROTOCOL;

/**
  Read the 3 byte manufacture and device ID from the SPI flash.

  This routine must be called at or below TPL_NOTIFY.
  This routine reads the 3 byte manufacture and device ID from the flash part
  filling the buffer provided.

  @param[in]  This    Pointer to an EFI_SPI_NOR_FLASH_PROTOCOL data structure.
  @param[out] Buffer  Pointer to a 3 byte buffer to receive the manufacture and
                      device ID.



  @retval EFI_SUCCESS            The manufacture and device ID was read
                                 successfully.
  @retval EFI_INVALID_PARAMETER  Buffer is NULL
  @retval EFI_DEVICE_ERROR       Invalid data received from SPI flash part.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SPI_NOR_FLASH_PROTOCOL_GET_FLASH_ID)(
  IN  CONST EFI_SPI_NOR_FLASH_PROTOCOL  *This,
  OUT UINT8                             *Buffer
  );

/**
  Read data from the SPI flash.

  This routine must be called at or below TPL_NOTIFY.
  This routine reads data from the SPI part in the buffer provided.

  @param[in]  This           Pointer to an EFI_SPI_NOR_FLASH_PROTOCOL data
                             structure.
  @param[in]  FlashAddress   Address in the flash to start reading
  @param[in]  LengthInBytes  Read length in bytes
  @param[out] Buffer         Address of a buffer to receive the data

  @retval EFI_SUCCESS            The data was read successfully.
  @retval EFI_INVALID_PARAMETER  Buffer is NULL, or
                                 FlashAddress >= This->FlashSize, or
                                 LengthInBytes > This->FlashSize - FlashAddress

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SPI_NOR_FLASH_PROTOCOL_READ_DATA)(
  IN  CONST EFI_SPI_NOR_FLASH_PROTOCOL  *This,
  IN  UINT32                            FlashAddress,
  IN  UINT32                            LengthInBytes,
  OUT UINT8                             *Buffer
  );

/**
  Read the flash status register.

  This routine must be called at or below TPL_NOTIFY.
  This routine reads the flash part status register.

  @param[in]  This           Pointer to an EFI_SPI_NOR_FLASH_PROTOCOL data
                             structure.
  @param[in]  LengthInBytes  Number of status bytes to read.
  @param[out] FlashStatus    Pointer to a buffer to receive the flash status.

  @retval EFI_SUCCESS  The status register was read successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SPI_NOR_FLASH_PROTOCOL_READ_STATUS)(
  IN  CONST EFI_SPI_NOR_FLASH_PROTOCOL  *This,
  IN  UINT32                            LengthInBytes,
  OUT UINT8                             *FlashStatus
  );

/**
  Write the flash status register.

  This routine must be called at or below TPL_N OTIFY.
  This routine writes the flash part status register.

  @param[in] This           Pointer to an EFI_SPI_NOR_FLASH_PROTOCOL data
                            structure.
  @param[in] LengthInBytes  Number of status bytes to write.
  @param[in] FlashStatus    Pointer to a buffer containing the new status.

  @retval EFI_SUCCESS           The status write was successful.
  @retval EFI_OUT_OF_RESOURCES  Failed to allocate the write buffer.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SPI_NOR_FLASH_PROTOCOL_WRITE_STATUS)(
  IN CONST EFI_SPI_NOR_FLASH_PROTOCOL  *This,
  IN UINT32                            LengthInBytes,
  IN UINT8                             *FlashStatus
  );

/**
  Write data to the SPI flash.

  This routine must be called at or below TPL_NOTIFY.
  This routine breaks up the write operation as necessary to write the data to
  the SPI part.

  @param[in] This           Pointer to an EFI_SPI_NOR_FLASH_PROTOCOL data
                            structure.
  @param[in] FlashAddress   Address in the flash to start writing
  @param[in] LengthInBytes  Write length in bytes
  @param[in] Buffer         Address of a buffer containing the data

  @retval EFI_SUCCESS            The data was written successfully.
  @retval EFI_INVALID_PARAMETER  Buffer is NULL, or
                                 FlashAddress >= This->FlashSize, or
                                 LengthInBytes > This->FlashSize - FlashAddress
  @retval EFI_OUT_OF_RESOURCES   Insufficient memory to copy buffer.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SPI_NOR_FLASH_PROTOCOL_WRITE_DATA)(
  IN CONST EFI_SPI_NOR_FLASH_PROTOCOL  *This,
  IN UINT32                            FlashAddress,
  IN UINT32                            LengthInBytes,
  IN UINT8                             *Buffer
  );

/**
  Efficiently erases one or more 4KiB regions in the SPI flash.

  This routine must be called at or below TPL_NOTIFY.
  This routine uses a combination of 4 KiB and larger blocks to erase the
  specified area.

  @param[in] This          Pointer to an EFI_SPI_NOR_FLASH_PROTOCOL data
                           structure.
  @param[in] FlashAddress  Address within a 4 KiB block to start erasing
  @param[in] BlockCount    Number of 4 KiB blocks to erase

  @retval EFI_SUCCESS            The erase was completed successfully.
  @retval EFI_INVALID_PARAMETER  FlashAddress >= This->FlashSize, or
                                 BlockCount * 4 KiB
                                   > This->FlashSize - FlashAddress

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SPI_NOR_FLASH_PROTOCOL_ERASE)(
  IN CONST EFI_SPI_NOR_FLASH_PROTOCOL  *This,
  IN UINT32                            FlashAddress,
  IN UINT32                            BlockCount
  );

///
/// The EFI_SPI_NOR_FLASH_PROTOCOL exists in the SPI peripheral layer.
/// This protocol manipulates the SPI NOR flash parts using a common set of
/// commands. The board layer provides the interconnection and configuration
/// details for the SPI NOR flash part. The SPI NOR flash driver uses this
/// configuration data to expose a generic interface which provides the
/// following APls:
/// * Read manufacture and device ID
/// * Read data
/// * Read data using low frequency
/// * Read status
/// * Write data
/// * Erase 4 KiB blocks
/// * Erase 32 or 64 KiB blocks
/// * Write status
/// The EFI_SPI_NOR_FLASH_PROTOCOL also exposes some APls to set the security
/// features on the legacy SPI flash controller.
///
struct _EFI_SPI_NOR_FLASH_PROTOCOL {
  ///
  /// Pointer to an EFI_SPI_PERIPHERAL data structure
  ///
  CONST EFI_SPI_PERIPHERAL                   *SpiPeripheral;

  ///
  /// Flash size in bytes
  ///
  UINT32                                     FlashSize;

  ///
  /// Manufacture and Device ID
  ///
  UINT8                                      Deviceid[3];

  ///
  /// Erase block size in bytes
  ///
  UINT32                                     EraseBlockBytes;

  ///
  /// Read the 3 byte manufacture and device ID from the SPI flash.
  ///
  EFI_SPI_NOR_FLASH_PROTOCOL_GET_FLASH_ID    GetFlashid;

  ///
  /// Read data from the SPI flash.
  ///
  EFI_SPI_NOR_FLASH_PROTOCOL_READ_DATA       ReadData;

  ///
  /// Low frequency read data from the SPI flash.
  ///
  EFI_SPI_NOR_FLASH_PROTOCOL_READ_DATA       LfReadData;

  ///
  /// Read the flash status register.
  ///
  EFI_SPI_NOR_FLASH_PROTOCOL_READ_STATUS     ReadStatus;

  ///
  /// Write the flash status register.
  ///
  EFI_SPI_NOR_FLASH_PROTOCOL_WRITE_STATUS    WriteStatus;

  ///
  /// Write data to the SPI flash.
  ///
  EFI_SPI_NOR_FLASH_PROTOCOL_WRITE_DATA      WriteData;

  ///
  /// Efficiently erases one or more 4KiB regions in the SPI flash.
  ///
  EFI_SPI_NOR_FLASH_PROTOCOL_ERASE           Erase;
};

extern EFI_GUID  gEfiSpiNorFlashProtocolGuid;

#endif // __SPI_NOR_FLASH_PROTOCOL_H__
