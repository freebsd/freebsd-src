/** @file
  Provides the services required to access a block I/O device during PEI recovery
  boot mode.

  The Recovery Module PPI and the Device Recovery Module PPI are device neutral.
  This PPI is device specific and addresses the most common form of recovery
  media-block I/O devices such as legacy floppy, CD-ROM, or IDE devices.

  The Recovery Block I/O PPI is used to access block devices. Because the Recovery
  Block I/O PPIs that are provided by the PEI ATAPI driver and PEI legacy floppy
  driver are the same, here we define a set of general PPIs for both drivers to use.

Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is defined in UEFI Platform Initialization Specification 1.2 Volume 1:
  Pre-EFI Initialization Core Interface.

**/

#ifndef _PEI_BLOCK_IO_H_
#define _PEI_BLOCK_IO_H_

///
/// Global ID for EFI_PEI_RECOVERY_BLOCK_IO_PPI
///
#define EFI_PEI_RECOVERY_BLOCK_IO_PPI_GUID \
  { \
    0x695d8aa1, 0x42ee, 0x4c46, { 0x80, 0x5c, 0x6e, 0xa6, 0xbc, 0xe7, 0x99, 0xe3 } \
  }

///
/// The forward declaration for EFI_PEI_RECOVERY_BLOCK_IO_PPI.
///
typedef struct _EFI_PEI_RECOVERY_BLOCK_IO_PPI EFI_PEI_RECOVERY_BLOCK_IO_PPI;

///
/// All blocks on the recovery device are addressed with a 64-bit Logical Block Address (LBA).
///
typedef UINT64 EFI_PEI_LBA;

///
/// EFI_PEI_BLOCK_DEVICE_TYPE
///
typedef enum {
  LegacyFloppy   = 0,  ///< The recovery device is a floppy.
  IdeCDROM       = 1,  ///< The recovery device is an IDE CD-ROM
  IdeLS120       = 2,  ///< The recovery device is an IDE LS-120
  UsbMassStorage = 3,  ///< The recovery device is a USB Mass Storage device
  SD             = 4,  ///< The recovery device is a Secure Digital device
  EMMC           = 5,  ///< The recovery device is a eMMC device
  UfsDevice      = 6,  ///< The recovery device is a Universal Flash Storage device
  MaxDeviceType
} EFI_PEI_BLOCK_DEVICE_TYPE;

///
/// Specification inconsistency here:
/// PEI_BLOCK_IO_MEDIA has been changed to EFI_PEI_BLOCK_IO_MEDIA.
/// Inconsistency exists in UEFI Platform Initialization Specification 1.2
/// Volume 1: Pre-EFI Initialization Core Interface, where all references to
/// this structure name are with the "EFI_" prefix, except for the definition
/// which is without "EFI_".  So the name of PEI_BLOCK_IO_MEDIA is taken as the
/// exception, and EFI_PEI_BLOCK_IO_MEDIA is used to comply with most of
/// the specification.
///
typedef struct {
  ///
  /// The type of media device being referenced by DeviceIndex.
  ///
  EFI_PEI_BLOCK_DEVICE_TYPE    DeviceType;
  ///
  /// A flag that indicates if media is present. This flag is always set for
  /// nonremovable media devices.
  ///
  BOOLEAN                      MediaPresent;
  ///
  /// The last logical block that the device supports.
  ///
  UINTN                        LastBlock;
  ///
  /// The size of a logical block in bytes.
  ///
  UINTN                        BlockSize;
} EFI_PEI_BLOCK_IO_MEDIA;

/**
  Gets the count of block I/O devices that one specific block driver detects.

  This function is used for getting the count of block I/O devices that one
  specific block driver detects.  To the PEI ATAPI driver, it returns the number
  of all the detected ATAPI devices it detects during the enumeration process.
  To the PEI legacy floppy driver, it returns the number of all the legacy
  devices it finds during its enumeration process. If no device is detected,
  then the function will return zero.

  @param[in]  PeiServices          General-purpose services that are available
                                   to every PEIM.
  @param[in]  This                 Indicates the EFI_PEI_RECOVERY_BLOCK_IO_PPI
                                   instance.
  @param[out] NumberBlockDevices   The number of block I/O devices discovered.

  @retval     EFI_SUCCESS          The operation performed successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_GET_NUMBER_BLOCK_DEVICES)(
  IN  EFI_PEI_SERVICES               **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO_PPI  *This,
  OUT UINTN                          *NumberBlockDevices
  );

/**
  Gets a block device's media information.

  This function will provide the caller with the specified block device's media
  information. If the media changes, calling this function will update the media
  information accordingly.

  @param[in]  PeiServices   General-purpose services that are available to every
                            PEIM
  @param[in]  This          Indicates the EFI_PEI_RECOVERY_BLOCK_IO_PPI instance.
  @param[in]  DeviceIndex   Specifies the block device to which the function wants
                            to talk. Because the driver that implements Block I/O
                            PPIs will manage multiple block devices, the PPIs that
                            want to talk to a single device must specify the
                            device index that was assigned during the enumeration
                            process. This index is a number from one to
                            NumberBlockDevices.
  @param[out] MediaInfo     The media information of the specified block media.
                            The caller is responsible for the ownership of this
                            data structure.

  @par Note:
      The MediaInfo structure describes an enumeration of possible block device
      types.  This enumeration exists because no device paths are actually passed
      across interfaces that describe the type or class of hardware that is publishing
      the block I/O interface. This enumeration will allow for policy decisions
      in the Recovery PEIM, such as "Try to recover from legacy floppy first,
      LS-120 second, CD-ROM third." If there are multiple partitions abstracted
      by a given device type, they should be reported in ascending order; this
      order also applies to nested partitions, such as legacy MBR, where the
      outermost partitions would have precedence in the reporting order. The
      same logic applies to systems such as IDE that have precedence relationships
      like "Master/Slave" or "Primary/Secondary". The master device should be
      reported first, the slave second.

  @retval EFI_SUCCESS        Media information about the specified block device
                             was obtained successfully.
  @retval EFI_DEVICE_ERROR   Cannot get the media information due to a hardware
                             error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_GET_DEVICE_MEDIA_INFORMATION)(
  IN  EFI_PEI_SERVICES               **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO_PPI  *This,
  IN  UINTN                          DeviceIndex,
  OUT EFI_PEI_BLOCK_IO_MEDIA         *MediaInfo
  );

/**
  Reads the requested number of blocks from the specified block device.

  The function reads the requested number of blocks from the device. All the
  blocks are read, or an error is returned. If there is no media in the device,
  the function returns EFI_NO_MEDIA.

  @param[in]  PeiServices   General-purpose services that are available to
                            every PEIM.
  @param[in]  This          Indicates the EFI_PEI_RECOVERY_BLOCK_IO_PPI instance.
  @param[in]  DeviceIndex   Specifies the block device to which the function wants
                            to talk. Because the driver that implements Block I/O
                            PPIs will manage multiple block devices, PPIs that
                            want to talk to a single device must specify the device
                            index that was assigned during the enumeration process.
                            This index is a number from one to NumberBlockDevices.
  @param[in]  StartLBA      The starting logical block address (LBA) to read from
                            on the device
  @param[in]  BufferSize    The size of the Buffer in bytes. This number must be
                            a multiple of the intrinsic block size of the device.
  @param[out] Buffer        A pointer to the destination buffer for the data.
                            The caller is responsible for the ownership of the
                            buffer.

  @retval EFI_SUCCESS             The data was read correctly from the device.
  @retval EFI_DEVICE_ERROR        The device reported an error while attempting
                                  to perform the read operation.
  @retval EFI_INVALID_PARAMETER   The read request contains LBAs that are not
                                  valid, or the buffer is not properly aligned.
  @retval EFI_NO_MEDIA            There is no media in the device.
  @retval EFI_BAD_BUFFER_SIZE     The BufferSize parameter is not a multiple of
                                  the intrinsic block size of the device.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_READ_BLOCKS)(
  IN  EFI_PEI_SERVICES               **PeiServices,
  IN  EFI_PEI_RECOVERY_BLOCK_IO_PPI  *This,
  IN  UINTN                          DeviceIndex,
  IN  EFI_PEI_LBA                    StartLBA,
  IN  UINTN                          BufferSize,
  OUT VOID                           *Buffer
  );

///
///  EFI_PEI_RECOVERY_BLOCK_IO_PPI provides the services that are required
///  to access a block I/O device during PEI recovery boot mode.
///
struct _EFI_PEI_RECOVERY_BLOCK_IO_PPI {
  ///
  /// Gets the number of block I/O devices that the specific block driver manages.
  ///
  EFI_PEI_GET_NUMBER_BLOCK_DEVICES        GetNumberOfBlockDevices;

  ///
  /// Gets the specified media information.
  ///
  EFI_PEI_GET_DEVICE_MEDIA_INFORMATION    GetBlockDeviceMediaInfo;

  ///
  /// Reads the requested number of blocks from the specified block device.
  ///
  EFI_PEI_READ_BLOCKS                     ReadBlocks;
};

extern EFI_GUID  gEfiPeiVirtualBlockIoPpiGuid;

#endif
