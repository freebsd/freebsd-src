/** @file
  Provides the basic interfaces to abstract platform information regarding an
  IDE controller.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is defined in UEFI Platform Initialization Specification 1.6
  Volume 5: Standards

**/

#ifndef __DISK_INFO_H__
#define __DISK_INFO_H__

///
/// Global ID for EFI_DISK_INFO_PROTOCOL
///
#define EFI_DISK_INFO_PROTOCOL_GUID \
  { \
    0xd432a67f, 0x14dc, 0x484b, {0xb3, 0xbb, 0x3f, 0x2, 0x91, 0x84, 0x93, 0x27 } \
  }

///
/// Forward declaration for EFI_DISK_INFO_PROTOCOL
///
typedef struct _EFI_DISK_INFO_PROTOCOL  EFI_DISK_INFO_PROTOCOL;

///
/// Global ID for an IDE interface.  Used to fill in EFI_DISK_INFO_PROTOCOL.Interface
///
#define EFI_DISK_INFO_IDE_INTERFACE_GUID \
  { \
    0x5e948fe3, 0x26d3, 0x42b5, {0xaf, 0x17, 0x61, 0x2, 0x87, 0x18, 0x8d, 0xec } \
  }

///
/// Global ID for a SCSI interface.  Used to fill in EFI_DISK_INFO_PROTOCOL.Interface
///
#define EFI_DISK_INFO_SCSI_INTERFACE_GUID \
  { \
    0x8f74baa, 0xea36, 0x41d9, {0x95, 0x21, 0x21, 0xa7, 0xf, 0x87, 0x80, 0xbc } \
  }

///
/// Global ID for a USB interface.  Used to fill in EFI_DISK_INFO_PROTOCOL.Interface
///
#define EFI_DISK_INFO_USB_INTERFACE_GUID \
  { \
    0xcb871572, 0xc11a, 0x47b5, {0xb4, 0x92, 0x67, 0x5e, 0xaf, 0xa7, 0x77, 0x27 } \
  }

///
/// Global ID for an AHCI interface.  Used to fill in EFI_DISK_INFO_PROTOCOL.Interface
///
#define EFI_DISK_INFO_AHCI_INTERFACE_GUID \
  { \
    0x9e498932, 0x4abc, 0x45af, {0xa3, 0x4d, 0x2, 0x47, 0x78, 0x7b, 0xe7, 0xc6 } \
  }

///
/// Global ID for a NVME interface.  Used to fill in EFI_DISK_INFO_PROTOCOL.Interface
///
#define EFI_DISK_INFO_NVME_INTERFACE_GUID \
  { \
    0x3ab14680, 0x5d3f, 0x4a4d, {0xbc, 0xdc, 0xcc, 0x38, 0x0, 0x18, 0xc7, 0xf7 } \
  }

///
/// Global ID for a UFS interface.  Used to fill in EFI_DISK_INFO_PROTOCOL.Interface
///
#define EFI_DISK_INFO_UFS_INTERFACE_GUID \
  { \
    0x4b3029cc, 0x6b98, 0x47fb, { 0xbc, 0x96, 0x76, 0xdc, 0xb8, 0x4, 0x41, 0xf0 } \
  }

///
/// Global ID for an SD/MMC interface.  Used to fill in EFI_DISK_INFO_PROTOCOL.Interface
///
#define EFI_DISK_INFO_SD_MMC_INTERFACE_GUID \
  { \
    0x8deec992, 0xd39c, 0x4a5c, { 0xab, 0x6b, 0x98, 0x6e, 0x14, 0x24, 0x2b, 0x9d } \
  }

/**
  Provides inquiry information for the controller type.

  This function is used by the IDE bus driver to get inquiry data.  Data format
  of Identify data is defined by the Interface GUID.

  @param[in]     This              Pointer to the EFI_DISK_INFO_PROTOCOL instance.
  @param[in,out] InquiryData       Pointer to a buffer for the inquiry data.
  @param[in,out] InquiryDataSize   Pointer to the value for the inquiry data size.

  @retval EFI_SUCCESS            The command was accepted without any errors.
  @retval EFI_NOT_FOUND          Device does not support this data class
  @retval EFI_DEVICE_ERROR       Error reading InquiryData from device
  @retval EFI_BUFFER_TOO_SMALL   InquiryDataSize not big enough

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DISK_INFO_INQUIRY)(
  IN     EFI_DISK_INFO_PROTOCOL  *This,
  IN OUT VOID                    *InquiryData,
  IN OUT UINT32                  *InquiryDataSize
  );

/**
  Provides identify information for the controller type.

  This function is used by the IDE bus driver to get identify data.  Data format
  of Identify data is defined by the Interface GUID.

  @param[in]     This               Pointer to the EFI_DISK_INFO_PROTOCOL
                                    instance.
  @param[in,out] IdentifyData       Pointer to a buffer for the identify data.
  @param[in,out] IdentifyDataSize   Pointer to the value for the identify data
                                    size.

  @retval EFI_SUCCESS            The command was accepted without any errors.
  @retval EFI_NOT_FOUND          Device does not support this data class
  @retval EFI_DEVICE_ERROR       Error reading IdentifyData from device
  @retval EFI_BUFFER_TOO_SMALL   IdentifyDataSize not big enough

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DISK_INFO_IDENTIFY)(
  IN     EFI_DISK_INFO_PROTOCOL  *This,
  IN OUT VOID                    *IdentifyData,
  IN OUT UINT32                  *IdentifyDataSize
  );

/**
  Provides sense data information for the controller type.

  This function is used by the IDE bus driver to get sense data.
  Data format of Sense data is defined by the Interface GUID.

  @param[in]     This              Pointer to the EFI_DISK_INFO_PROTOCOL instance.
  @param[in,out] SenseData         Pointer to the SenseData.
  @param[in,out] SenseDataSize     Size of SenseData in bytes.
  @param[out]    SenseDataNumber   Pointer to the value for the sense data size.

  @retval EFI_SUCCESS            The command was accepted without any errors.
  @retval EFI_NOT_FOUND          Device does not support this data class.
  @retval EFI_DEVICE_ERROR       Error reading SenseData from device.
  @retval EFI_BUFFER_TOO_SMALL   SenseDataSize not big enough.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DISK_INFO_SENSE_DATA)(
  IN     EFI_DISK_INFO_PROTOCOL  *This,
  IN OUT VOID                    *SenseData,
  IN OUT UINT32                  *SenseDataSize,
  OUT    UINT8                   *SenseDataNumber
  );

/**
  This function is used by the IDE bus driver to get controller information.

  @param[in]  This         Pointer to the EFI_DISK_INFO_PROTOCOL instance.
  @param[out] IdeChannel   Pointer to the Ide Channel number.  Primary or secondary.
  @param[out] IdeDevice    Pointer to the Ide Device number.  Master or slave.

  @retval EFI_SUCCESS       IdeChannel and IdeDevice are valid.
  @retval EFI_UNSUPPORTED   This is not an IDE device.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DISK_INFO_WHICH_IDE)(
  IN  EFI_DISK_INFO_PROTOCOL  *This,
  OUT UINT32                  *IdeChannel,
  OUT UINT32                  *IdeDevice
  );

///
/// The EFI_DISK_INFO_PROTOCOL provides controller specific information.
///
struct _EFI_DISK_INFO_PROTOCOL {
  ///
  /// A GUID that defines the format of buffers for the other member functions
  /// of this protocol.
  ///
  EFI_GUID                  Interface;
  ///
  /// Return the results of the Inquiry command to a drive in InquiryData. Data
  /// format of Inquiry data is defined by the Interface GUID.
  ///
  EFI_DISK_INFO_INQUIRY     Inquiry;
  ///
  /// Return the results of the Identify command to a drive in IdentifyData. Data
  /// format of Identify data is defined by the Interface GUID.
  ///
  EFI_DISK_INFO_IDENTIFY    Identify;
  ///
  /// Return the results of the Request Sense command to a drive in SenseData. Data
  /// format of Sense data is defined by the Interface GUID.
  ///
  EFI_DISK_INFO_SENSE_DATA  SenseData;
  ///
  /// Specific controller.
  ///
  EFI_DISK_INFO_WHICH_IDE   WhichIde;
};

extern EFI_GUID gEfiDiskInfoProtocolGuid;

extern EFI_GUID gEfiDiskInfoIdeInterfaceGuid;
extern EFI_GUID gEfiDiskInfoScsiInterfaceGuid;
extern EFI_GUID gEfiDiskInfoUsbInterfaceGuid;
extern EFI_GUID gEfiDiskInfoAhciInterfaceGuid;
extern EFI_GUID gEfiDiskInfoNvmeInterfaceGuid;
extern EFI_GUID gEfiDiskInfoUfsInterfaceGuid;
extern EFI_GUID gEfiDiskInfoSdMmcInterfaceGuid;

#endif
