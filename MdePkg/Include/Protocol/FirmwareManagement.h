/** @file
  UEFI Firmware Management Protocol definition
  Firmware Management Protocol provides an abstraction for device to provide firmware
  management support. The base requirements for managing device firmware images include
  identifying firmware image revision level and programming the image into the device.

  GetImageInfo() is the only required function. GetImage(), SetImage(),
  CheckImage(), GetPackageInfo(), and SetPackageInfo() shall return
  EFI_UNSUPPORTED if not supported by the driver.

  Copyright (c) 2009 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2013 - 2014, Hewlett-Packard Development Company, L.P.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.3

**/

#ifndef __EFI_FIRMWARE_MANAGEMENT_PROTOCOL_H__
#define __EFI_FIRMWARE_MANAGEMENT_PROTOCOL_H__


#define EFI_FIRMWARE_MANAGEMENT_PROTOCOL_GUID \
  { \
    0x86c77a67, 0xb97, 0x4633, {0xa1, 0x87, 0x49, 0x10, 0x4d, 0x6, 0x85, 0xc7 } \
  }

typedef struct _EFI_FIRMWARE_MANAGEMENT_PROTOCOL EFI_FIRMWARE_MANAGEMENT_PROTOCOL;

///
/// Dependency Expression Opcode
///
#define EFI_FMP_DEP_PUSH_GUID        0x00
#define EFI_FMP_DEP_PUSH_VERSION     0x01
#define EFI_FMP_DEP_VERSION_STR      0x02
#define EFI_FMP_DEP_AND              0x03
#define EFI_FMP_DEP_OR               0x04
#define EFI_FMP_DEP_NOT              0x05
#define EFI_FMP_DEP_TRUE             0x06
#define EFI_FMP_DEP_FALSE            0x07
#define EFI_FMP_DEP_EQ               0x08
#define EFI_FMP_DEP_GT               0x09
#define EFI_FMP_DEP_GTE              0x0A
#define EFI_FMP_DEP_LT               0x0B
#define EFI_FMP_DEP_LTE              0x0C
#define EFI_FMP_DEP_END              0x0D

///
/// Image Attribute - Dependency
///
typedef struct {
  UINT8 Dependencies[1];
} EFI_FIRMWARE_IMAGE_DEP;

///
/// EFI_FIRMWARE_IMAGE_DESCRIPTOR
///
typedef struct {
  ///
  /// A unique number identifying the firmware image within the device. The number is
  /// between 1 and DescriptorCount.
  ///
  UINT8                            ImageIndex;
  ///
  /// A unique GUID identifying the firmware image type.
  ///
  EFI_GUID                         ImageTypeId;
  ///
  /// A unique number identifying the firmware image.
  ///
  UINT64                           ImageId;
  ///
  /// A pointer to a null-terminated string representing the firmware image name.
  ///
  CHAR16                           *ImageIdName;
  ///
  /// Identifies the version of the device firmware. The format is vendor specific and new
  /// version must have a greater value than an old version.
  ///
  UINT32                           Version;
  ///
  /// A pointer to a null-terminated string representing the firmware image version name.
  ///
  CHAR16                           *VersionName;
  ///
  /// Size of the image in bytes. If size=0, then only ImageIndex and ImageTypeId are valid.
  ///
  UINTN                            Size;
  ///
  /// Image attributes that are supported by this device. See 'Image Attribute Definitions'
  /// for possible returned values of this parameter. A value of 1 indicates the attribute is
  /// supported and the current setting value is indicated in AttributesSetting. A
  /// value of 0 indicates the attribute is not supported and the current setting value in
  /// AttributesSetting is meaningless.
  ///
  UINT64                           AttributesSupported;
  ///
  /// Image attributes. See 'Image Attribute Definitions' for possible returned values of
  /// this parameter.
  ///
  UINT64                           AttributesSetting;
  ///
  /// Image compatibilities. See 'Image Compatibility Definitions' for possible returned
  /// values of this parameter.
  ///
  UINT64                           Compatibilities;
  ///
  /// Describes the lowest ImageDescriptor version that the device will accept. Only
  /// present in version 2 or higher.
  ///
  UINT32                           LowestSupportedImageVersion;
  ///
  /// Describes the version that was last attempted to update. If no update attempted the
  /// value will be 0. If the update attempted was improperly formatted and no version
  /// number was available then the value will be zero. Only present in version 3 or higher.
  UINT32                           LastAttemptVersion;
  ///
  /// Describes the status that was last attempted to update. If no update has been attempted
  /// the value will be LAST_ATTEMPT_STATUS_SUCCESS. Only present in version 3 or higher.
  ///
  UINT32                           LastAttemptStatus;
  ///
  /// An optional number to identify the unique hardware instance within the system for
  /// devices that may have multiple instances (Example: a plug in pci network card). This
  /// number must be unique within the namespace of the ImageTypeId GUID and
  /// ImageIndex. For FMP instances that have multiple descriptors for a single
  /// hardware instance, all descriptors must have the same HardwareInstance value.
  /// This number must be consistent between boots and should be based on some sort of
  /// hardware identified unique id (serial number, etc) whenever possible. If a hardware
  /// based number is not available the FMP provider may use some other characteristic
  /// such as device path, bus/dev/function, slot num, etc for generating the
  /// HardwareInstance. For implementations that will never have more than one
  /// instance a zero can be used. A zero means the FMP provider is not able to determine a
  /// unique hardware instance number or a hardware instance number is not needed. Only
  /// present in version 3 or higher.
  ///
  UINT64                           HardwareInstance;
  EFI_FIRMWARE_IMAGE_DEP           *Dependencies;
} EFI_FIRMWARE_IMAGE_DESCRIPTOR;


//
// Image Attribute Definitions
//
///
/// The attribute IMAGE_ATTRIBUTE_IMAGE_UPDATABLE indicates this device supports firmware
/// image update.
///
#define    IMAGE_ATTRIBUTE_IMAGE_UPDATABLE         0x0000000000000001
///
/// The attribute IMAGE_ATTRIBUTE_RESET_REQUIRED indicates a reset of the device is required
/// for the new firmware image to take effect after a firmware update. The device is the device hosting
/// the firmware image.
///
#define    IMAGE_ATTRIBUTE_RESET_REQUIRED          0x0000000000000002
///
/// The attribute IMAGE_ATTRIBUTE_AUTHENTICATION_REQUIRED indicates authentication is
/// required to perform the following image operations: GetImage(), SetImage(), and
/// CheckImage(). See 'Image Attribute - Authentication'.
///
#define    IMAGE_ATTRIBUTE_AUTHENTICATION_REQUIRED 0x0000000000000004
///
/// The attribute IMAGE_ATTRIBUTE_IN_USE indicates the current state of the firmware image.
/// This distinguishes firmware images in a device that supports redundant images.
///
#define    IMAGE_ATTRIBUTE_IN_USE                  0x0000000000000008
///
/// The attribute IMAGE_ATTRIBUTE_UEFI_IMAGE indicates that this image is an EFI compatible image.
///
#define    IMAGE_ATTRIBUTE_UEFI_IMAGE              0x0000000000000010
///
/// The attribute IMAGE_ATTRIBUTE_DEPENDENCY indicates that there is an EFI_FIRMWARE_IMAGE_DEP
/// section associated with the image.
///
#define    IMAGE_ATTRIBUTE_DEPENDENCY              0x0000000000000020


//
// Image Compatibility Definitions
//
///
/// Values from 0x0000000000000002 thru 0x000000000000FFFF are reserved for future assignments.
/// Values from 0x0000000000010000 thru 0xFFFFFFFFFFFFFFFF are used by firmware vendor for
/// compatibility check.
///
#define   IMAGE_COMPATIBILITY_CHECK_SUPPORTED      0x0000000000000001

///
/// Descriptor Version exposed by GetImageInfo() function
///
#define   EFI_FIRMWARE_IMAGE_DESCRIPTOR_VERSION   4


///
/// Image Attribute - Authentication Required
///
typedef struct {
  ///
  /// It is included in the signature of AuthInfo. It is used to ensure freshness/no replay.
  /// It is incremented during each firmware image operation.
  ///
  UINT64                                  MonotonicCount;
  ///
  /// Provides the authorization for the firmware image operations. It is a signature across
  /// the image data and the Monotonic Count value. Caller uses the private key that is
  /// associated with a public key that has been provisioned via the key exchange.
  /// Because this is defined as a signature, WIN_CERTIFICATE_UEFI_GUID.CertType must
  /// be EFI_CERT_TYPE_PKCS7_GUID.
  ///
  WIN_CERTIFICATE_UEFI_GUID               AuthInfo;
} EFI_FIRMWARE_IMAGE_AUTHENTICATION;


//
// ImageUpdatable Definitions
//
///
/// IMAGE_UPDATABLE_VALID indicates SetImage() will accept the new image and update the
/// device with the new image. The version of the new image could be higher or lower than
/// the current image. SetImage VendorCode is optional but can be used for vendor
/// specific action.
///
#define  IMAGE_UPDATABLE_VALID                     0x0000000000000001
///
/// IMAGE_UPDATABLE_INVALID indicates SetImage() will reject the new image. No additional
/// information is provided for the rejection.
///
#define  IMAGE_UPDATABLE_INVALID                   0x0000000000000002
///
/// IMAGE_UPDATABLE_INVALID_TYPE indicates SetImage() will reject the new image. The
/// rejection is due to the new image is not a firmware image recognized for this device.
///
#define  IMAGE_UPDATABLE_INVALID_TYPE              0x0000000000000004
///
/// IMAGE_UPDATABLE_INVALID_OLD indicates SetImage() will reject the new image. The
/// rejection is due to the new image version is older than the current firmware image
/// version in the device. The device firmware update policy does not support firmware
/// version downgrade.
///
#define  IMAGE_UPDATABLE_INVALID_OLD               0x0000000000000008
///
/// IMAGE_UPDATABLE_VALID_WITH_VENDOR_CODE indicates SetImage() will accept and update
/// the new image only if a correct VendorCode is provided or else image would be
/// rejected and SetImage will return appropriate error.
///
#define IMAGE_UPDATABLE_VALID_WITH_VENDOR_CODE     0x0000000000000010


//
// Package Attribute Definitions
//
///
/// The attribute PACKAGE_ATTRIBUTE_VERSION_UPDATABLE indicates this device supports the
/// update of the firmware package version.
///
#define  PACKAGE_ATTRIBUTE_VERSION_UPDATABLE       0x0000000000000001
///
/// The attribute PACKAGE_ATTRIBUTE_RESET_REQUIRED indicates a reset of the device is
/// required for the new package info to take effect after an update.
///
#define  PACKAGE_ATTRIBUTE_RESET_REQUIRED          0x0000000000000002
///
/// The attribute PACKAGE_ATTRIBUTE_AUTHENTICATION_REQUIRED indicates authentication
/// is required to update the package info.
///
#define  PACKAGE_ATTRIBUTE_AUTHENTICATION_REQUIRED 0x0000000000000004

/**
  Callback function to report the process of the firmware updating.

  @param[in]  Completion    A value between 1 and 100 indicating the current completion
                            progress of the firmware update. Completion progress is
                            reported as from 1 to 100 percent. A value of 0 is used by
                            the driver to indicate that progress reporting is not supported.

  @retval EFI_SUCCESS       SetImage() continues to do the callback if supported.
  @retval other             SetImage() discontinues the callback and completes
                            the update and returns.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS)(
  IN  UINTN                          Completion
  );

/**
  Returns information about the current firmware image(s) of the device.

  This function allows a copy of the current firmware image to be created and saved.
  The saved copy could later been used, for example, in firmware image recovery or rollback.

  @param[in]      This               A pointer to the EFI_FIRMWARE_MANAGEMENT_PROTOCOL instance.
  @param[in, out] ImageInfoSize      A pointer to the size, in bytes, of the ImageInfo buffer.
                                     On input, this is the size of the buffer allocated by the caller.
                                     On output, it is the size of the buffer returned by the firmware
                                     if the buffer was large enough, or the size of the buffer needed
                                     to contain the image(s) information if the buffer was too small.
  @param[in, out] ImageInfo          A pointer to the buffer in which firmware places the current image(s)
                                     information. The information is an array of EFI_FIRMWARE_IMAGE_DESCRIPTORs.
  @param[out]     DescriptorVersion  A pointer to the location in which firmware returns the version number
                                     associated with the EFI_FIRMWARE_IMAGE_DESCRIPTOR.
  @param[out]     DescriptorCount    A pointer to the location in which firmware returns the number of
                                     descriptors or firmware images within this device.
  @param[out]     DescriptorSize     A pointer to the location in which firmware returns the size, in bytes,
                                     of an individual EFI_FIRMWARE_IMAGE_DESCRIPTOR.
  @param[out]     PackageVersion     A version number that represents all the firmware images in the device.
                                     The format is vendor specific and new version must have a greater value
                                     than the old version. If PackageVersion is not supported, the value is
                                     0xFFFFFFFF. A value of 0xFFFFFFFE indicates that package version comparison
                                     is to be performed using PackageVersionName. A value of 0xFFFFFFFD indicates
                                     that package version update is in progress.
  @param[out]     PackageVersionName A pointer to a pointer to a null-terminated string representing the
                                     package version name. The buffer is allocated by this function with
                                     AllocatePool(), and it is the caller's responsibility to free it with a call
                                     to FreePool().

  @retval EFI_SUCCESS                The device was successfully updated with the new image.
  @retval EFI_BUFFER_TOO_SMALL       The ImageInfo buffer was too small. The current buffer size
                                     needed to hold the image(s) information is returned in ImageInfoSize.
  @retval EFI_INVALID_PARAMETER      ImageInfoSize is NULL.
  @retval EFI_DEVICE_ERROR           Valid information could not be returned. Possible corrupted image.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FIRMWARE_MANAGEMENT_PROTOCOL_GET_IMAGE_INFO)(
  IN EFI_FIRMWARE_MANAGEMENT_PROTOCOL       *This,
  IN OUT    UINTN                           *ImageInfoSize,
  IN OUT    EFI_FIRMWARE_IMAGE_DESCRIPTOR   *ImageInfo,
  OUT       UINT32                          *DescriptorVersion,
  OUT       UINT8                           *DescriptorCount,
  OUT       UINTN                           *DescriptorSize,
  OUT       UINT32                          *PackageVersion,
  OUT       CHAR16                          **PackageVersionName
  );

/**
  Retrieves a copy of the current firmware image of the device.

  This function allows a copy of the current firmware image to be created and saved.
  The saved copy could later been used, for example, in firmware image recovery or rollback.

  @param[in]      This           A pointer to the EFI_FIRMWARE_MANAGEMENT_PROTOCOL instance.
  @param[in]      ImageIndex     A unique number identifying the firmware image(s) within the device.
                                 The number is between 1 and DescriptorCount.
  @param[out]     Image          Points to the buffer where the current image is copied to.
  @param[in, out] ImageSize      On entry, points to the size of the buffer pointed to by Image, in bytes.
                                 On return, points to the length of the image, in bytes.

  @retval EFI_SUCCESS            The device was successfully updated with the new image.
  @retval EFI_BUFFER_TOO_SMALL   The buffer specified by ImageSize is too small to hold the
                                 image. The current buffer size needed to hold the image is returned
                                 in ImageSize.
  @retval EFI_INVALID_PARAMETER  The Image was NULL.
  @retval EFI_NOT_FOUND          The current image is not copied to the buffer.
  @retval EFI_UNSUPPORTED        The operation is not supported.
  @retval EFI_SECURITY_VIOLATION The operation could not be performed due to an authentication failure.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FIRMWARE_MANAGEMENT_PROTOCOL_GET_IMAGE)(
  IN  EFI_FIRMWARE_MANAGEMENT_PROTOCOL  *This,
  IN  UINT8                             ImageIndex,
  OUT  VOID                             *Image,
  IN  OUT  UINTN                        *ImageSize
  );

/**
  Updates the firmware image of the device.

  This function updates the hardware with the new firmware image.
  This function returns EFI_UNSUPPORTED if the firmware image is not updatable.
  If the firmware image is updatable, the function should perform the following minimal validations
  before proceeding to do the firmware image update.
  - Validate the image authentication if image has attribute
    IMAGE_ATTRIBUTE_AUTHENTICATION_REQUIRED. The function returns
    EFI_SECURITY_VIOLATION if the validation fails.
  - Validate the image is a supported image for this device. The function returns EFI_ABORTED if
    the image is unsupported. The function can optionally provide more detailed information on
    why the image is not a supported image.
  - Validate the data from VendorCode if not null. Image validation must be performed before
    VendorCode data validation. VendorCode data is ignored or considered invalid if image
    validation failed. The function returns EFI_ABORTED if the data is invalid.

  VendorCode enables vendor to implement vendor-specific firmware image update policy. Null if
  the caller did not specify the policy or use the default policy. As an example, vendor can implement
  a policy to allow an option to force a firmware image update when the abort reason is due to the new
  firmware image version is older than the current firmware image version or bad image checksum.
  Sensitive operations such as those wiping the entire firmware image and render the device to be
  non-functional should be encoded in the image itself rather than passed with the VendorCode.
  AbortReason enables vendor to have the option to provide a more detailed description of the abort
  reason to the caller.

  @param[in]  This               A pointer to the EFI_FIRMWARE_MANAGEMENT_PROTOCOL instance.
  @param[in]  ImageIndex         A unique number identifying the firmware image(s) within the device.
                                 The number is between 1 and DescriptorCount.
  @param[in]  Image              Points to the new image.
  @param[in]  ImageSize          Size of the new image in bytes.
  @param[in]  VendorCode         This enables vendor to implement vendor-specific firmware image update policy.
                                 Null indicates the caller did not specify the policy or use the default policy.
  @param[in]  Progress           A function used by the driver to report the progress of the firmware update.
  @param[out] AbortReason        A pointer to a pointer to a null-terminated string providing more
                                 details for the aborted operation. The buffer is allocated by this function
                                 with AllocatePool(), and it is the caller's responsibility to free it with a
                                 call to FreePool().

  @retval EFI_SUCCESS            The device was successfully updated with the new image.
  @retval EFI_ABORTED            The operation is aborted.
  @retval EFI_INVALID_PARAMETER  The Image was NULL.
  @retval EFI_UNSUPPORTED        The operation is not supported.
  @retval EFI_SECURITY_VIOLATION The operation could not be performed due to an authentication failure.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FIRMWARE_MANAGEMENT_PROTOCOL_SET_IMAGE)(
  IN  EFI_FIRMWARE_MANAGEMENT_PROTOCOL                 *This,
  IN  UINT8                                            ImageIndex,
  IN  CONST VOID                                       *Image,
  IN  UINTN                                            ImageSize,
  IN  CONST VOID                                       *VendorCode,
  IN  EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS    Progress,
  OUT CHAR16                                           **AbortReason
  );

/**
  Checks if the firmware image is valid for the device.

  This function allows firmware update application to validate the firmware image without
  invoking the SetImage() first.

  @param[in]  This               A pointer to the EFI_FIRMWARE_MANAGEMENT_PROTOCOL instance.
  @param[in]  ImageIndex         A unique number identifying the firmware image(s) within the device.
                                 The number is between 1 and DescriptorCount.
  @param[in]  Image              Points to the new image.
  @param[in]  ImageSize          Size of the new image in bytes.
  @param[out] ImageUpdatable     Indicates if the new image is valid for update. It also provides,
                                 if available, additional information if the image is invalid.

  @retval EFI_SUCCESS            The image was successfully checked.
  @retval EFI_INVALID_PARAMETER  The Image was NULL.
  @retval EFI_UNSUPPORTED        The operation is not supported.
  @retval EFI_SECURITY_VIOLATION The operation could not be performed due to an authentication failure.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FIRMWARE_MANAGEMENT_PROTOCOL_CHECK_IMAGE)(
  IN  EFI_FIRMWARE_MANAGEMENT_PROTOCOL  *This,
  IN  UINT8                             ImageIndex,
  IN  CONST VOID                        *Image,
  IN  UINTN                             ImageSize,
  OUT UINT32                            *ImageUpdatable
  );

/**
  Returns information about the firmware package.

  This function returns package information.

  @param[in]  This                     A pointer to the EFI_FIRMWARE_MANAGEMENT_PROTOCOL instance.
  @param[out] PackageVersion           A version number that represents all the firmware images in the device.
                                       The format is vendor specific and new version must have a greater value
                                       than the old version. If PackageVersion is not supported, the value is
                                       0xFFFFFFFF. A value of 0xFFFFFFFE indicates that package version
                                       comparison is to be performed using PackageVersionName. A value of
                                       0xFFFFFFFD indicates that package version update is in progress.
  @param[out] PackageVersionName       A pointer to a pointer to a null-terminated string representing
                                       the package version name. The buffer is allocated by this function with
                                       AllocatePool(), and it is the caller's responsibility to free it with a
                                       call to FreePool().
  @param[out] PackageVersionNameMaxLen The maximum length of package version name if device supports update of
                                       package version name. A value of 0 indicates the device does not support
                                       update of package version name. Length is the number of Unicode characters,
                                       including the terminating null character.
  @param[out] AttributesSupported      Package attributes that are supported by this device. See 'Package Attribute
                                       Definitions' for possible returned values of this parameter. A value of 1
                                       indicates the attribute is supported and the current setting value is
                                       indicated in AttributesSetting. A value of 0 indicates the attribute is not
                                       supported and the current setting value in AttributesSetting is meaningless.
  @param[out] AttributesSetting        Package attributes. See 'Package Attribute Definitions' for possible returned
                                       values of this parameter

  @retval EFI_SUCCESS                  The package information was successfully returned.
  @retval EFI_UNSUPPORTED              The operation is not supported.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FIRMWARE_MANAGEMENT_PROTOCOL_GET_PACKAGE_INFO)(
  IN  EFI_FIRMWARE_MANAGEMENT_PROTOCOL *This,
  OUT UINT32                           *PackageVersion,
  OUT CHAR16                           **PackageVersionName,
  OUT UINT32                           *PackageVersionNameMaxLen,
  OUT UINT64                           *AttributesSupported,
  OUT UINT64                           *AttributesSetting
  );

/**
  Updates information about the firmware package.

  This function updates package information.
  This function returns EFI_UNSUPPORTED if the package information is not updatable.
  VendorCode enables vendor to implement vendor-specific package information update policy.
  Null if the caller did not specify this policy or use the default policy.

  @param[in]  This               A pointer to the EFI_FIRMWARE_MANAGEMENT_PROTOCOL instance.
  @param[in]  Image              Points to the authentication image.
                                 Null if authentication is not required.
  @param[in]  ImageSize          Size of the authentication image in bytes.
                                 0 if authentication is not required.
  @param[in]  VendorCode         This enables vendor to implement vendor-specific firmware
                                 image update policy.
                                 Null indicates the caller did not specify this policy or use
                                 the default policy.
  @param[in]  PackageVersion     The new package version.
  @param[in]  PackageVersionName A pointer to the new null-terminated Unicode string representing
                                 the package version name.
                                 The string length is equal to or less than the value returned in
                                 PackageVersionNameMaxLen.

  @retval EFI_SUCCESS            The device was successfully updated with the new package
                                 information.
  @retval EFI_INVALID_PARAMETER  The PackageVersionName length is longer than the value
                                 returned in PackageVersionNameMaxLen.
  @retval EFI_UNSUPPORTED        The operation is not supported.
  @retval EFI_SECURITY_VIOLATION The operation could not be performed due to an authentication failure.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FIRMWARE_MANAGEMENT_PROTOCOL_SET_PACKAGE_INFO)(
  IN  EFI_FIRMWARE_MANAGEMENT_PROTOCOL   *This,
  IN  CONST VOID                         *Image,
  IN  UINTN                              ImageSize,
  IN  CONST VOID                         *VendorCode,
  IN  UINT32                             PackageVersion,
  IN  CONST CHAR16                       *PackageVersionName
  );

///
/// EFI_FIRMWARE_MANAGEMENT_PROTOCOL
/// The protocol for managing firmware provides the following services.
/// - Get the attributes of the current firmware image. Attributes include revision level.
/// - Get a copy of the current firmware image. As an example, this service could be used by a
///   management application to facilitate a firmware roll-back.
/// - Program the device with a firmware image supplied by the user.
/// - Label all the firmware images within a device with a single version.
///
struct _EFI_FIRMWARE_MANAGEMENT_PROTOCOL {
  EFI_FIRMWARE_MANAGEMENT_PROTOCOL_GET_IMAGE_INFO    GetImageInfo;
  EFI_FIRMWARE_MANAGEMENT_PROTOCOL_GET_IMAGE         GetImage;
  EFI_FIRMWARE_MANAGEMENT_PROTOCOL_SET_IMAGE         SetImage;
  EFI_FIRMWARE_MANAGEMENT_PROTOCOL_CHECK_IMAGE       CheckImage;
  EFI_FIRMWARE_MANAGEMENT_PROTOCOL_GET_PACKAGE_INFO  GetPackageInfo;
  EFI_FIRMWARE_MANAGEMENT_PROTOCOL_SET_PACKAGE_INFO  SetPackageInfo;
};

extern EFI_GUID gEfiFirmwareManagementProtocolGuid;

#endif
