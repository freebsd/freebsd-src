/** @file
  The Legacy Region Protocol controls the read, write and boot-lock attributes for
  the region 0xC0000 to 0xFFFFF.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is defined in UEFI Platform Initialization Specification 1.2
  Volume 5: Standards

**/

#ifndef __LEGACY_REGION2_H__
#define __LEGACY_REGION2_H__

#define EFI_LEGACY_REGION2_PROTOCOL_GUID \
{ \
  0x70101eaf, 0x85, 0x440c, {0xb3, 0x56, 0x8e, 0xe3, 0x6f, 0xef, 0x24, 0xf0 } \
}

typedef struct _EFI_LEGACY_REGION2_PROTOCOL EFI_LEGACY_REGION2_PROTOCOL;

/**
  Modify the hardware to allow (decode) or disallow (not decode) memory reads in a region.

  If the On parameter evaluates to TRUE, this function enables memory reads in the address range
  Start to (Start + Length - 1).
  If the On parameter evaluates to FALSE, this function disables memory reads in the address range
  Start to (Start + Length - 1).

  @param  This[in]              Indicates the EFI_LEGACY_REGION2_PROTOCOL instance.
  @param  Start[in]             The beginning of the physical address of the region whose attributes
                                should be modified.
  @param  Length[in]            The number of bytes of memory whose attributes should be modified.
                                The actual number of bytes modified may be greater than the number
                                specified.
  @param  Granularity[out]      The number of bytes in the last region affected. This may be less
                                than the total number of bytes affected if the starting address
                                was not aligned to a region's starting address or if the length
                                was greater than the number of bytes in the first region.
  @param  On[in]                Decode / Non-Decode flag.

  @retval EFI_SUCCESS           The region's attributes were successfully modified.
  @retval EFI_INVALID_PARAMETER If Start or Length describe an address not in the Legacy Region.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_LEGACY_REGION2_DECODE)(
  IN  EFI_LEGACY_REGION2_PROTOCOL  *This,
  IN  UINT32                       Start,
  IN  UINT32                       Length,
  OUT UINT32                       *Granularity,
  IN  BOOLEAN                      *On
  );

/**
  Modify the hardware to disallow memory writes in a region.

  This function changes the attributes of a memory range to not allow writes.

  @param  This[in]              Indicates the EFI_LEGACY_REGION2_PROTOCOL instance.
  @param  Start[in]             The beginning of the physical address of the region whose
                                attributes should be modified.
  @param  Length[in]            The number of bytes of memory whose attributes should be modified.
                                The actual number of bytes modified may be greater than the number
                                specified.
  @param  Granularity[out]      The number of bytes in the last region affected. This may be less
                                than the total number of bytes affected if the starting address was
                                not aligned to a region's starting address or if the length was
                                greater than the number of bytes in the first region.

  @retval EFI_SUCCESS           The region's attributes were successfully modified.
  @retval EFI_INVALID_PARAMETER If Start or Length describe an address not in the Legacy Region.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_LEGACY_REGION2_LOCK)(
  IN  EFI_LEGACY_REGION2_PROTOCOL   *This,
  IN  UINT32                        Start,
  IN  UINT32                        Length,
  OUT UINT32                        *Granularity
  );

/**
  Modify the hardware to disallow memory attribute changes in a region.

  This function makes the attributes of a region read only. Once a region is boot-locked with this
  function, the read and write attributes of that region cannot be changed until a power cycle has
  reset the boot-lock attribute. Calls to Decode(), Lock() and Unlock() will have no effect.

  @param  This[in]              Indicates the EFI_LEGACY_REGION2_PROTOCOL instance.
  @param  Start[in]             The beginning of the physical address of the region whose
                                attributes should be modified.
  @param  Length[in]            The number of bytes of memory whose attributes should be modified.
                                The actual number of bytes modified may be greater than the number
                                specified.
  @param  Granularity[out]      The number of bytes in the last region affected. This may be less
                                than the total number of bytes affected if the starting address was
                                not aligned to a region's starting address or if the length was
                                greater than the number of bytes in the first region.

  @retval EFI_SUCCESS           The region's attributes were successfully modified.
  @retval EFI_INVALID_PARAMETER If Start or Length describe an address not in the Legacy Region.
  @retval EFI_UNSUPPORTED       The chipset does not support locking the configuration registers in
                                a way that will not affect memory regions outside the legacy memory
                                region.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_LEGACY_REGION2_BOOT_LOCK)(
  IN  EFI_LEGACY_REGION2_PROTOCOL         *This,
  IN  UINT32                              Start,
  IN  UINT32                              Length,
  OUT UINT32                              *Granularity OPTIONAL
  );

/**
  Modify the hardware to allow memory writes in a region.

  This function changes the attributes of a memory range to allow writes.

  @param  This[in]              Indicates the EFI_LEGACY_REGION2_PROTOCOL instance.
  @param  Start[in]             The beginning of the physical address of the region whose
                                attributes should be modified.
  @param  Length[in]            The number of bytes of memory whose attributes should be modified.
                                The actual number of bytes modified may be greater than the number
                                specified.
  @param  Granularity[out]      The number of bytes in the last region affected. This may be less
                                than the total number of bytes affected if the starting address was
                                not aligned to a region's starting address or if the length was
                                greater than the number of bytes in the first region.

  @retval EFI_SUCCESS           The region's attributes were successfully modified.
  @retval EFI_INVALID_PARAMETER If Start or Length describe an address not in the Legacy Region.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_LEGACY_REGION2_UNLOCK)(
  IN  EFI_LEGACY_REGION2_PROTOCOL  *This,
  IN  UINT32                       Start,
  IN  UINT32                       Length,
  OUT UINT32                       *Granularity
  );

typedef enum {
  LegacyRegionDecoded,         ///< This region is currently set to allow reads.
  LegacyRegionNotDecoded,      ///< This region is currently set to not allow reads.
  LegacyRegionWriteEnabled,    ///< This region is currently set to allow writes.
  LegacyRegionWriteDisabled,   ///< This region is currently set to write protected.
  LegacyRegionBootLocked,      ///< This region's attributes are locked, cannot be modified until
                               ///< after a power cycle.
  LegacyRegionNotLocked        ///< This region's attributes are not locked.
} EFI_LEGACY_REGION_ATTRIBUTE;

typedef struct {
  ///
  /// The beginning of the physical address of this
  /// region.
  ///
  UINT32                         Start;
  ///
  /// The number of bytes in this region.
  ///
  UINT32                         Length;
  ///
  /// Attribute of the Legacy Region Descriptor that
  /// describes the capabilities for that memory region.
  ///
  EFI_LEGACY_REGION_ATTRIBUTE    Attribute;
  ///
  /// Describes the byte length programmability
  /// associated with the Start address and the specified
  /// Attribute setting.
  UINT32                         Granularity;
} EFI_LEGACY_REGION_DESCRIPTOR;

/**
  Get region information for the attributes of the Legacy Region.

  This function is used to discover the granularity of the attributes for the memory in the legacy
  region. Each attribute may have a different granularity and the granularity may not be the same
  for all memory ranges in the legacy region.

  @param  This[in]              Indicates the EFI_LEGACY_REGION2_PROTOCOL instance.
  @param  DescriptorCount[out]  The number of region descriptor entries returned in the Descriptor
                                buffer.
  @param  Descriptor[out]       A pointer to a pointer used to return a buffer where the legacy
                                region information is deposited. This buffer will contain a list of
                                DescriptorCount number of region descriptors.  This function will
                                provide the memory for the buffer.

  @retval EFI_SUCCESS           The information structure was returned.
  @retval EFI_UNSUPPORTED       This function is not supported.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_LEGACY_REGION_GET_INFO)(
  IN  EFI_LEGACY_REGION2_PROTOCOL   *This,
  OUT UINT32                        *DescriptorCount,
  OUT EFI_LEGACY_REGION_DESCRIPTOR  **Descriptor
  );

///
/// The EFI_LEGACY_REGION2_PROTOCOL is used to abstract the hardware control of the memory
/// attributes of the Option ROM shadowing region, 0xC0000 to 0xFFFFF.
/// There are three memory attributes that can be modified through this protocol: read, write and
/// boot-lock. These protocols may be set in any combination.
///
struct _EFI_LEGACY_REGION2_PROTOCOL {
  EFI_LEGACY_REGION2_DECODE       Decode;
  EFI_LEGACY_REGION2_LOCK         Lock;
  EFI_LEGACY_REGION2_BOOT_LOCK    BootLock;
  EFI_LEGACY_REGION2_UNLOCK       UnLock;
  EFI_LEGACY_REGION_GET_INFO      GetInfo;
};

extern EFI_GUID  gEfiLegacyRegion2ProtocolGuid;

#endif
