/** @file
  This file declares PlatfromOpRom protocols that provide the interface between
  the PCI bus driver/PCI Host Bridge Resource Allocation driver and a platform-specific
  driver to describe the unique features of a platform.
  This protocol is optional.

Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is defined in UEFI Platform Initialization Specification 1.2
  Volume 5: Standards

**/

#ifndef _PCI_PLATFORM_H_
#define _PCI_PLATFORM_H_

///
/// This file must be included because the EFI_PCI_PLATFORM_PROTOCOL uses
/// EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PHASE.
///
#include <Protocol/PciHostBridgeResourceAllocation.h>

///
/// Global ID for the EFI_PCI_PLATFORM_PROTOCOL.
///
#define EFI_PCI_PLATFORM_PROTOCOL_GUID \
  { \
    0x7d75280, 0x27d4, 0x4d69, {0x90, 0xd0, 0x56, 0x43, 0xe2, 0x38, 0xb3, 0x41} \
  }

///
/// Forward declaration for EFI_PCI_PLATFORM_PROTOCOL.
///
typedef struct _EFI_PCI_PLATFORM_PROTOCOL EFI_PCI_PLATFORM_PROTOCOL;

///
/// EFI_PCI_PLATFORM_POLICY that is a bitmask with the following legal combinations:
///   - EFI_RESERVE_NONE_IO_ALIAS:<BR>
///       Does not set aside either ISA or VGA I/O resources during PCI
///       enumeration. By using this selection, the platform indicates that it does
///       not want to support a PCI device that requires ISA or legacy VGA
///       resources. If a PCI device driver asks for these resources, the request
///       will be turned down.
///   - EFI_RESERVE_ISA_IO_ALIAS | EFI_RESERVE_VGA_IO_ALIAS:<BR>
///       Sets aside the ISA I/O range and all the aliases during PCI
///       enumeration. VGA I/O ranges and aliases are included in ISA alias
///       ranges. In this scheme, seventy-five percent of the I/O space remains unused.
///       By using this selection, the platform indicates that it wants to support
///       PCI devices that require the following, at the cost of wasted I/O space:
///       ISA range and its aliases
///       Legacy VGA range and its aliases
///       The PCI bus driver will not allocate I/O addresses out of the ISA I/O
///       range and its aliases. The following are the ISA I/O ranges:
///         - n100..n3FF
///         - n500..n7FF
///         - n900..nBFF
///         - nD00..nFFF
///
///       In this case, the PCI bus driver will ask the PCI host bridge driver for
///       larger I/O ranges. The PCI host bridge driver is not aware of the ISA
///       aliasing policy and merely attempts to allocate the requested ranges.
///       The first device that requests the legacy VGA range will get all the
///       legacy VGA range plus its aliased addresses forwarded to it. The first
///       device that requests the legacy ISA range will get all the legacy ISA
///       range, plus its aliased addresses, forwarded to it.
///   - EFI_RESERVE_ISA_IO_NO_ALIAS | EFI_RESERVE_VGA_IO_ALIAS:<BR>
///       Sets aside the ISA I/O range (0x100 - 0x3FF) during PCI enumeration
///       and the aliases of the VGA I/O ranges. By using this selection, the
///       platform indicates that it will support VGA devices that require VGA
///       ranges, including those that require VGA aliases. The platform further
///       wants to support non-VGA devices that ask for the ISA range (0x100 -
///       3FF), but not if it also asks for the ISA aliases. The PCI bus driver will
///       not allocate I/O addresses out of the legacy ISA I/O range (0x100 -
///       0x3FF) range or the aliases of the VGA I/O range. If a PCI device
///       driver asks for the ISA I/O ranges, including aliases, the request will be
///       turned down. The first device that requests the legacy VGA range will
///       get all the legacy VGA range plus its aliased addresses forwarded to
///       it. When the legacy VGA device asks for legacy VGA ranges and its
///       aliases, all the upstream PCI-to-PCI bridges must be set up to perform
///       10-bit decode on legacy VGA ranges. To prevent two bridges from
///       positively decoding the same address, all PCI-to-PCI bridges that are
///       peers to this bridge will have to be set up to not decode ISA aliased
///       ranges. In that case, all the devices behind the peer bridges can
///       occupy only I/O addresses that are not ISA aliases. This is a limitation
///       of PCI-to-PCI bridges and is described in the white paper PCI-to-PCI
///       Bridges and Card Bus Controllers on Windows 2000, Windows XP,
///       and Windows Server 2003. The PCI enumeration process must be
///       cognizant of this restriction.
///   - EFI_RESERVE_ISA_IO_NO_ALIAS | EFI_RESERVE_VGA_IO_NO_ALIAS:<BR>
///       Sets aside the ISA I/O range (0x100 - 0x3FF) during PCI enumeration.
///       VGA I/O ranges are included in the ISA range. By using this selection,
///       the platform indicates that it wants to support PCI devices that require
///       the ISA range and legacy VGA range, but it does not want to support
///       devices that require ISA alias ranges or VGA alias ranges. The PCI
///       bus driver will not allocate I/O addresses out of the legacy ISA I/O
///       range (0x100-0x3FF). If a PCI device driver asks for the ISA I/O
///       ranges, including aliases, the request will be turned down. By using
///       this selection, the platform indicates that it will support VGA devices
///       that require VGA ranges, but it will not support VGA devices that
///       require VGA aliases. To truly support 16-bit VGA decode, all the PCIto-
///       PCI bridges that are upstream to a VGA device, as well as
///       upstream to the parent PCI root bridge, must support 16-bit VGA I/O
///       decode. See the PCI-to-PCI Bridge Architecture Specification for
///       information regarding the 16-bit VGA decode support. This
///       requirement must hold true for every VGA device in the system. If any
///       of these bridges does not support 16-bit VGA decode, it will positively
///       decode all the aliases of the VGA I/O ranges and this selection must
///       be treated like EFI_RESERVE_ISA_IO_NO_ALIAS |
///       EFI_RESERVE_VGA_IO_ALIAS.
///
typedef UINT32 EFI_PCI_PLATFORM_POLICY;

///
/// Does not set aside either ISA or VGA I/O resources during PCI
/// enumeration.
///
#define     EFI_RESERVE_NONE_IO_ALIAS  0x0000

///
/// Sets aside ISA I/O range and all aliases:
///   - n100..n3FF
///   - n500..n7FF
///   - n900..nBFF
///   - nD00..nFFF.
///
#define     EFI_RESERVE_ISA_IO_ALIAS  0x0001

///
/// Sets aside ISA I/O range 0x100-0x3FF.
///
#define     EFI_RESERVE_ISA_IO_NO_ALIAS  0x0002

///
/// Sets aside VGA I/O ranges and all aliases.
///
#define     EFI_RESERVE_VGA_IO_ALIAS  0x0004

///
/// Sets aside VGA I/O ranges
///
#define     EFI_RESERVE_VGA_IO_NO_ALIAS  0x0008

///
/// EFI_PCI_EXECUTION_PHASE is used to call a platform protocol and execute
/// platform-specific code.
///
typedef enum {
  ///
  /// The phase that indicates the entry point to the PCI Bus Notify phase. This
  /// platform hook is called before the PCI bus driver calls the
  /// EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL driver.
  ///
  BeforePciHostBridge = 0,
  ///
  /// The phase that indicates the entry point to the PCI Bus Notify phase. This
  /// platform hook is called before the PCI bus driver calls the
  /// EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL driver.
  ///
  ChipsetEntry = 0,
  ///
  /// The phase that indicates the exit point to the Chipset Notify phase before
  /// returning to the PCI Bus Driver Notify phase. This platform hook is called after
  /// the PCI bus driver calls the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
  /// driver.
  ///
  AfterPciHostBridge = 1,
  ///
  /// The phase that indicates the exit point to the Chipset Notify phase before
  /// returning to the PCI Bus Driver Notify phase. This platform hook is called after
  /// the PCI bus driver calls the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
  /// driver.
  ///
  ChipsetExit = 1,
  MaximumChipsetPhase
} EFI_PCI_EXECUTION_PHASE;

typedef EFI_PCI_EXECUTION_PHASE EFI_PCI_CHIPSET_EXECUTION_PHASE;

/**
  The notification from the PCI bus enumerator to the platform that it is
  about to enter a certain phase during the enumeration process.

  The PlatformNotify() function can be used to notify the platform driver so that
  it can perform platform-specific actions. No specific actions are required.
  Eight notification points are defined at this time. More synchronization points
  may be added as required in the future. The PCI bus driver calls the platform driver
  twice for every Phase-once before the PCI Host Bridge Resource Allocation Protocol
  driver is notified, and once after the PCI Host Bridge Resource Allocation Protocol
  driver has been notified.
  This member function may not perform any error checking on the input parameters. It
  also does not return any error codes. If this member function detects any error condition,
  it needs to handle those errors on its own because there is no way to surface any
  errors to the caller.

  @param[in] This           The pointer to the EFI_PCI_PLATFORM_PROTOCOL instance.
  @param[in] HostBridge     The handle of the host bridge controller.
  @param[in] Phase          The phase of the PCI bus enumeration.
  @param[in] ExecPhase      Defines the execution phase of the PCI chipset driver.

  @retval EFI_SUCCESS   The function completed successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PCI_PLATFORM_PHASE_NOTIFY)(
  IN EFI_PCI_PLATFORM_PROTOCOL                      *This,
  IN EFI_HANDLE                                     HostBridge,
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PHASE  Phase,
  IN EFI_PCI_EXECUTION_PHASE                        ExecPhase
  );

/**
  The notification from the PCI bus enumerator to the platform for each PCI
  controller at several predefined points during PCI controller initialization.

  The PlatformPrepController() function can be used to notify the platform driver so that
  it can perform platform-specific actions. No specific actions are required.
  Several notification points are defined at this time. More synchronization points may be
  added as required in the future. The PCI bus driver calls the platform driver twice for
  every PCI controller-once before the PCI Host Bridge Resource Allocation Protocol driver
  is notified, and once after the PCI Host Bridge Resource Allocation Protocol driver has
  been notified.
  This member function may not perform any error checking on the input parameters. It also
  does not return any error codes. If this member function detects any error condition, it
  needs to handle those errors on its own because there is no way to surface any errors to
  the caller.

  @param[in] This           The pointer to the EFI_PCI_PLATFORM_PROTOCOL instance.
  @param[in] HostBridge     The associated PCI host bridge handle.
  @param[in] RootBridge     The associated PCI root bridge handle.
  @param[in] PciAddress     The address of the PCI device on the PCI bus.
  @param[in] Phase          The phase of the PCI controller enumeration.
  @param[in] ExecPhase      Defines the execution phase of the PCI chipset driver.

  @retval EFI_SUCCESS   The function completed successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PCI_PLATFORM_PREPROCESS_CONTROLLER)(
  IN EFI_PCI_PLATFORM_PROTOCOL                     *This,
  IN EFI_HANDLE                                    HostBridge,
  IN EFI_HANDLE                                    RootBridge,
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS   PciAddress,
  IN EFI_PCI_CONTROLLER_RESOURCE_ALLOCATION_PHASE  Phase,
  IN EFI_PCI_EXECUTION_PHASE                       ExecPhase
  );

/**
  Retrieves the platform policy regarding enumeration.

  The GetPlatformPolicy() function retrieves the platform policy regarding PCI
  enumeration. The PCI bus driver and the PCI Host Bridge Resource Allocation Protocol
  driver can call this member function to retrieve the policy.

  @param[in]  This        The pointer to the EFI_PCI_PLATFORM_PROTOCOL instance.
  @param[out] PciPolicy   The platform policy with respect to VGA and ISA aliasing.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   PciPolicy is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PCI_PLATFORM_GET_PLATFORM_POLICY)(
  IN  CONST EFI_PCI_PLATFORM_PROTOCOL  *This,
  OUT       EFI_PCI_PLATFORM_POLICY    *PciPolicy
  );

/**
  Gets the PCI device's option ROM from a platform-specific location.

  The GetPciRom() function gets the PCI device's option ROM from a platform-specific location.
  The option ROM will be loaded into memory. This member function is used to return an image
  that is packaged as a PCI 2.2 option ROM. The image may contain both legacy and EFI option
  ROMs. See the UEFI 2.0 Specification for details. This member function can be used to return
  option ROM images for embedded controllers. Option ROMs for embedded controllers are typically
  stored in platform-specific storage, and this member function can retrieve it from that storage
  and return it to the PCI bus driver. The PCI bus driver will call this member function before
  scanning the ROM that is attached to any controller, which allows a platform to specify a ROM
  image that is different from the ROM image on a PCI card.

  @param[in]  This        The pointer to the EFI_PCI_PLATFORM_PROTOCOL instance.
  @param[in]  PciHandle   The handle of the PCI device.
  @param[out] RomImage    If the call succeeds, the pointer to the pointer to the option ROM image.
                          Otherwise, this field is undefined. The memory for RomImage is allocated
                          by EFI_PCI_PLATFORM_PROTOCOL.GetPciRom() using the EFI Boot Service AllocatePool().
                          It is the caller's responsibility to free the memory using the EFI Boot Service
                          FreePool(), when the caller is done with the option ROM.
  @param[out] RomSize     If the call succeeds, a pointer to the size of the option ROM size. Otherwise,
                          this field is undefined.

  @retval EFI_SUCCESS            The option ROM was available for this device and loaded into memory.
  @retval EFI_NOT_FOUND          No option ROM was available for this device.
  @retval EFI_OUT_OF_RESOURCES   No memory was available to load the option ROM.
  @retval EFI_DEVICE_ERROR       An error occurred in obtaining the option ROM.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PCI_PLATFORM_GET_PCI_ROM)(
  IN  CONST EFI_PCI_PLATFORM_PROTOCOL  *This,
  IN        EFI_HANDLE                 PciHandle,
  OUT       VOID                       **RomImage,
  OUT       UINTN                      *RomSize
  );

///
/// This protocol provides the interface between the PCI bus driver/PCI Host
/// Bridge Resource Allocation driver and a platform-specific driver to describe
/// the unique features of a platform.
///
struct _EFI_PCI_PLATFORM_PROTOCOL {
  ///
  /// The notification from the PCI bus enumerator to the platform that it is about to
  /// enter a certain phase during the enumeration process.
  ///
  EFI_PCI_PLATFORM_PHASE_NOTIFY             PlatformNotify;
  ///
  /// The notification from the PCI bus enumerator to the platform for each PCI
  /// controller at several predefined points during PCI controller initialization.
  ///
  EFI_PCI_PLATFORM_PREPROCESS_CONTROLLER    PlatformPrepController;
  ///
  /// Retrieves the platform policy regarding enumeration.
  ///
  EFI_PCI_PLATFORM_GET_PLATFORM_POLICY      GetPlatformPolicy;
  ///
  /// Gets the PCI device's option ROM from a platform-specific location.
  ///
  EFI_PCI_PLATFORM_GET_PCI_ROM              GetPciRom;
};

extern EFI_GUID  gEfiPciPlatformProtocolGuid;

#endif
