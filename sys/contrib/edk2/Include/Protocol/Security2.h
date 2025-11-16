/** @file
  Security2 Architectural Protocol as defined in PI Specification1.2.1 VOLUME 2 DXE

  Abstracts security-specific functions from the DXE Foundation of UEFI Image Verification,
  Trusted Computing Group (TCG) measured boot, and User Identity policy for image loading and
  consoles. This protocol must be produced by a boot service or runtime DXE driver.

  This protocol is optional and must be published prior to the EFI_SECURITY_ARCH_PROTOCOL.
  As a result, the same driver must publish both of these interfaces.

  When both Security and Security2 Architectural Protocols are published, LoadImage must use
  them in accordance with the following rules:
    The Security2 protocol must be used on every image being loaded.
    The Security protocol must be used after the Securiy2 protocol and only on images that
    have been read using Firmware Volume protocol.

  When only Security architectural protocol is published, LoadImage must use it on every image
  being loaded.

  Copyright (c) 2012 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ARCH_PROTOCOL_SECURITY2_H__
#define __ARCH_PROTOCOL_SECURITY2_H__

///
/// Global ID for the Security2 Code Architectural Protocol
///
#define EFI_SECURITY2_ARCH_PROTOCOL_GUID \
  { 0x94ab2f58, 0x1438, 0x4ef1, {0x91, 0x52, 0x18, 0x94, 0x1a, 0x3a, 0x0e, 0x68 } }

typedef struct _EFI_SECURITY2_ARCH_PROTOCOL EFI_SECURITY2_ARCH_PROTOCOL;

/**
  The DXE Foundation uses this service to measure and/or verify a UEFI image.

  This service abstracts the invocation of Trusted Computing Group (TCG) measured boot, UEFI
  Secure boot, and UEFI User Identity infrastructure. For the former two, the DXE Foundation
  invokes the FileAuthentication() with a DevicePath and corresponding image in
  FileBuffer memory. The TCG measurement code will record the FileBuffer contents into the
  appropriate PCR. The image verification logic will confirm the integrity and provenance of the
  image in FileBuffer of length FileSize . The origin of the image will be DevicePath in
  these cases.
  If the FileBuffer is NULL, the interface will determine if the DevicePath can be connected
  in order to support the User Identification policy.

  @param  This             The EFI_SECURITY2_ARCH_PROTOCOL instance.
  @param  File             A pointer to the device path of the file that is
                           being dispatched. This will optionally be used for logging.
  @param  FileBuffer       A pointer to the buffer with the UEFI file image.
  @param  FileSize         The size of the file.
  @param  BootPolicy       A boot policy that was used to call LoadImage() UEFI service. If
                           FileAuthentication() is invoked not from the LoadImage(),
                           BootPolicy must be set to FALSE.

  @retval EFI_SUCCESS             The file specified by DevicePath and non-NULL
                                  FileBuffer did authenticate, and the platform policy dictates
                                  that the DXE Foundation may use the file.
  @retval EFI_SUCCESS             The device path specified by NULL device path DevicePath
                                  and non-NULL FileBuffer did authenticate, and the platform
                                  policy dictates that the DXE Foundation may execute the image in
                                  FileBuffer.
  @retval EFI_SUCCESS             FileBuffer is NULL and current user has permission to start
                                  UEFI device drivers on the device path specified by DevicePath.
  @retval EFI_SECURITY_VIOLATION  The file specified by DevicePath and FileBuffer did not
                                  authenticate, and the platform policy dictates that the file should be
                                  placed in the untrusted state. The image has been added to the file
                                  execution table.
  @retval EFI_ACCESS_DENIED       The file specified by File and FileBuffer did not
                                  authenticate, and the platform policy dictates that the DXE
                                  Foundation may not use File.
  @retval EFI_SECURITY_VIOLATION  FileBuffer is NULL and the user has no
                                  permission to start UEFI device drivers on the device path specified
                                  by DevicePath.
  @retval EFI_SECURITY_VIOLATION  FileBuffer is not NULL and the user has no permission to load
                                  drivers from the device path specified by DevicePath. The
                                  image has been added into the list of the deferred images.
**/
typedef EFI_STATUS (EFIAPI *EFI_SECURITY2_FILE_AUTHENTICATION)(
  IN CONST EFI_SECURITY2_ARCH_PROTOCOL *This,
  IN CONST EFI_DEVICE_PATH_PROTOCOL    *File  OPTIONAL,
  IN VOID                              *FileBuffer,
  IN UINTN                             FileSize,
  IN BOOLEAN                           BootPolicy
  );

///
/// The EFI_SECURITY2_ARCH_PROTOCOL is used to abstract platform-specific policy from the
/// DXE Foundation. This includes measuring the PE/COFF image prior to invoking, comparing the
/// image against a policy (whether a white-list/black-list of public image verification keys
/// or registered hashes).
///
struct _EFI_SECURITY2_ARCH_PROTOCOL {
  EFI_SECURITY2_FILE_AUTHENTICATION    FileAuthentication;
};

extern EFI_GUID  gEfiSecurity2ArchProtocolGuid;

#endif
