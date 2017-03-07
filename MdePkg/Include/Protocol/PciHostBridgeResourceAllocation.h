/** @file
  This file declares PCI Host Bridge Resource Allocation Protocol which 
  provides the basic interfaces to abstract a PCI host bridge resource allocation. 
  This protocol is mandatory if the system includes PCI devices.
  
Copyright (c) 2007 - 2010, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available under 
the terms and conditions of the BSD License that accompanies this distribution.  
The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.                                          
    
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  @par Revision Reference:
  This Protocol is defined in UEFI Platform Initialization Specification 1.2 
  Volume 5: Standards.
  
**/

#ifndef _PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_H_
#define _PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_H_

//
// This file must be included because EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
// uses EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS
//
#include <Protocol/PciRootBridgeIo.h>

///
/// Global ID for the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL.
///
#define EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_GUID \
  { \
    0xCF8034BE, 0x6768, 0x4d8b, {0xB7,0x39,0x7C,0xCE,0x68,0x3A,0x9F,0xBE } \
  }

///
/// Forward declaration for EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL.
///
typedef struct _EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL;

/// If this bit is set, then the PCI Root Bridge does not
/// support separate windows for Non-prefetchable and Prefetchable
/// memory. A PCI bus driver needs to include requests for Prefetchable
/// memory in the Non-prefetchable memory pool.
///
#define EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM  1

///
/// If this bit is set, then the PCI Root Bridge supports
/// 64 bit memory windows.  If this bit is not set,
/// the PCI bus driver needs to include requests for 64 bit
/// memory address in the corresponding 32 bit memory pool.
///
#define EFI_PCI_HOST_BRIDGE_MEM64_DECODE   2

///
/// A UINT64 value that contains the status of a PCI resource requested 
/// in the Configuration parameter returned by GetProposedResources()
/// The legal values are EFI_RESOURCE_SATISFIED and EFI_RESOURCE_NOT_SATISFIED
///
typedef UINT64 EFI_RESOURCE_ALLOCATION_STATUS;

///
/// The request of this resource type could be fulfilled.  Used in the 
/// Configuration parameter returned by GetProposedResources() to identify
/// a PCI resources request that can be satisfied.
///
#define EFI_RESOURCE_SATISFIED      0x0000000000000000ULL

///
/// The request of this resource type could not be fulfilled for its
/// absence in the host bridge resource pool.  Used in the Configuration parameter 
/// returned by GetProposedResources() to identify a PCI resources request that
/// can not be satisfied.
///
#define EFI_RESOURCE_NOT_SATISFIED  0xFFFFFFFFFFFFFFFFULL

///
/// This  enum is used to specify the phase of the PCI enumaeration process.
///
typedef enum {
  ///
  /// Reset the host bridge PCI apertures and internal data structures.
  /// PCI enumerator should issue this notification before starting fresh
  /// enumeration process. Enumeration cannot be restarted after sending
  /// any other notification such as EfiPciHostBridgeBeginBusAllocation.
  ///
  EfiPciHostBridgeBeginEnumeration,

  ///
  /// The bus allocation phase is about to begin. No specific action
  /// is required here. This notification can be used to perform any
  /// chipset specific programming.  
  ///
  EfiPciHostBridgeBeginBusAllocation,

  ///
  /// The bus allocation and bus programming phase is complete. No specific
  /// action is required here. This notification can be used to perform any
  /// chipset specific programming.  
  ///
  EfiPciHostBridgeEndBusAllocation,
  
  ///
  /// The resource allocation phase is about to begin.No specific action is
  /// required here. This notification can be used to perform any chipset specific programming.  
  ///
  EfiPciHostBridgeBeginResourceAllocation,
  
  ///
  /// Allocate resources per previously submitted requests for all the PCI Root
  /// Bridges. These resource settings are returned on the next call to
  /// GetProposedResources().  
  ///
  EfiPciHostBridgeAllocateResources,
  
  ///
  /// Program the Host Bridge hardware to decode previously allocated resources
  /// (proposed resources) for all the PCI Root Bridges.
  ///
  EfiPciHostBridgeSetResources,
  
  ///
  /// De-allocate previously allocated resources previously for all the PCI
  /// Root Bridges and reset the I/O and memory apertures to initial state.  
  ///
  EfiPciHostBridgeFreeResources,
  
  ///
  /// The resource allocation phase is completed.  No specific action is required
  /// here. This notification can be used to perform any chipset specific programming.  
  ///
  EfiPciHostBridgeEndResourceAllocation,

  ///
  /// The Host Bridge Enumeration is completed. No specific action is required here.
  /// This notification can be used to perform any chipset specific programming.
  ///
  EfiPciHostBridgeEndEnumeration,
  EfiMaxPciHostBridgeEnumerationPhase
} EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PHASE;

///
/// Definitions of 2 notification points.
///
typedef enum {
  ///
  /// This notification is only applicable to PCI-PCI bridges and
  /// indicates that the PCI enumerator is about to begin enumerating
  /// the bus behind the PCI-PCI Bridge. This notification is sent after
  /// the primary bus number, the secondary bus number and the subordinate
  /// bus number registers in the PCI-PCI Bridge are programmed to valid
  /// (not necessary final) values
  ///
  EfiPciBeforeChildBusEnumeration,

  ///
  /// This notification is sent before the PCI enumerator probes BAR registers
  /// for every valid PCI function.  
  ///
  EfiPciBeforeResourceCollection
} EFI_PCI_CONTROLLER_RESOURCE_ALLOCATION_PHASE;

/**
  These are the notifications from the PCI bus driver that it is about to enter a certain phase of the PCI 
  enumeration process.

  @param[in] This    The pointer to the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL 
                     instance.
  @param[in] Phase   The phase during enumeration.

  @retval EFI_SUCCESS             The notification was accepted without any errors.
  @retval EFI_INVALID_PARAMETER   The Phase is invalid.
  @retval EFI_NOT_READY           This phase cannot be entered at this time. For example, this error 
                                  is valid for a Phase of EfiPciHostBridgeAllocateResources if 
                                  SubmitResources() has not been called for one or more 
                                  PCI root bridges before this call.
  @retval EFI_DEVICE_ERROR        Programming failed due to a hardware error. This error is valid for 
                                  a Phase of EfiPciHostBridgeSetResources.
  @retval EFI_OUT_OF_RESOURCES    The request could not be completed due to a lack of resources. 
                                  This error is valid for a Phase of EfiPciHostBridgeAllocateResources
                                  if the previously submitted resource requests cannot be fulfilled or were only 
                                  partially fulfilled

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_NOTIFY_PHASE)(
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL  *This,
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PHASE     Phase
  );

/**
  Returns the device handle of the next PCI root bridge that is associated with this host bridge.

  @param[in]     This               The pointer to the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL 
                                    instance.
  @param[in,out] RootBridgeHandle   Returns the device handle of the next PCI root bridge. On input, it holds the 
                                    RootBridgeHandle that was returned by the most recent call to 
                                    GetNextRootBridge(). If RootBridgeHandle is NULL on input, the handle 
                                    for the first PCI root bridge is returned.

  @retval EFI_SUCCESS             The requested attribute information was returned.
  @retval EFI_INVALID_PARAMETER   RootBridgeHandle is not an EFI_HANDLE that was returned 
                                  on a previous call to GetNextRootBridge().
  @retval EFI_NOT_FOUND           There are no more PCI root bridge device handles.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_GET_NEXT_ROOT_BRIDGE)(
  IN     EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL  *This,
  IN OUT EFI_HANDLE                                        *RootBridgeHandle
  );

/**
  Returns the allocation attributes of a PCI root bridge.

  @param[in]  This               The pointer to the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL 
                                 instance.
  @param[in]  RootBridgeHandle   The device handle of the PCI root bridge in which the caller is interested.
  @param[out] Attribute          The pointer to attributes of the PCI root bridge.

  @retval EFI_SUCCESS             The requested attribute information was returned.
  @retval EFI_INVALID_PARAMETER   RootBridgeHandle is not a valid root bridge handle.
  @retval EFI_INVALID_PARAMETER   Attributes is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_GET_ATTRIBUTES)(
  IN  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL   *This,
  IN  EFI_HANDLE                                         RootBridgeHandle,
  OUT UINT64                                             *Attributes
  );

/**
  Sets up the specified PCI root bridge for the bus enumeration process.

  @param[in]  This               The pointer to the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL 
                                 instance.
  @param[in]  RootBridgeHandle   The PCI root bridge to be set up.
  @param[out] Configuration      The pointer to the pointer to the PCI bus resource descriptor.

  @retval EFI_SUCCESS             The PCI root bridge was set up and the bus range was returned in 
                                  Configuration.
  @retval EFI_INVALID_PARAMETER   RootBridgeHandle is not a valid root bridge handle.
  @retval EFI_DEVICE_ERROR        Programming failed due to a hardware error.
  @retval EFI_OUT_OF_RESOURCES    The request could not be completed due to a lack of resources.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_START_BUS_ENUMERATION)(
  IN  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL   *This,
  IN  EFI_HANDLE                                         RootBridgeHandle,
  OUT VOID                                               **Configuration
  );

/**
  Programs the PCI root bridge hardware so that it decodes the specified PCI bus range.

  @param[in] This               The pointer to the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
                                instance.  
  @param[in] RootBridgeHandle   The PCI root bridge whose bus range is to be programmed.
  @param[in] Configuration      The pointer to the PCI bus resource descriptor.

  @retval EFI_SUCCESS             The bus range for the PCI root bridge was programmed.
  @retval EFI_INVALID_PARAMETER   RootBridgeHandle is not a valid root bridge handle.
  @retval EFI_INVALID_PARAMETER   Configuration is NULL
  @retval EFI_INVALID_PARAMETER   Configuration does not point to a valid ACPI (2.0 & 3.0) 
                                  resource descriptor.
  @retval EFI_INVALID_PARAMETER   Configuration does not include a valid ACPI 2.0 bus resource
                                  descriptor.
  @retval EFI_INVALID_PARAMETER   Configuration includes valid ACPI (2.0 & 3.0) resource 
                                  descriptors other than bus descriptors.
  @retval EFI_INVALID_PARAMETER   Configuration contains one or more invalid ACPI resource 
                                  descriptors.
  @retval EFI_INVALID_PARAMETER   "Address Range Minimum" is invalid for this root bridge.
  @retval EFI_INVALID_PARAMETER   "Address Range Length" is invalid for this root bridge.
  @retval EFI_DEVICE_ERROR        Programming failed due to a hardware error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_SET_BUS_NUMBERS)(
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL   *This,
  IN EFI_HANDLE                                         RootBridgeHandle,
  IN VOID                                               *Configuration
  );

/**
  Submits the I/O and memory resource requirements for the specified PCI root bridge.

  @param[in] This               The pointer to the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL 
                                instance.
  @param[in] RootBridgeHandle   The PCI root bridge whose I/O and memory resource requirements are being 
                                submitted.
  @param[in] Configuration      The pointer to the PCI I/O and PCI memory resource descriptor.

  @retval EFI_SUCCESS             The I/O and memory resource requests for a PCI root bridge were 
                                  accepted.
  @retval EFI_INVALID_PARAMETER   RootBridgeHandle is not a valid root bridge handle.
  @retval EFI_INVALID_PARAMETER   Configuration is NULL.
  @retval EFI_INVALID_PARAMETER   Configuration does not point to a valid ACPI (2.0 & 3.0) 
                                  resource descriptor.
  @retval EFI_INVALID_PARAMETER   Configuration includes requests for one or more resource 
                                  types that are not supported by this PCI root bridge. This error will 
                                  happen if the caller did not combine resources according to 
                                  Attributes that were returned by GetAllocAttributes().
  @retval EFI_INVALID_PARAMETER   "Address Range Maximum" is invalid.
  @retval EFI_INVALID_PARAMETER   "Address Range Length" is invalid for this PCI root bridge.
  @retval EFI_INVALID_PARAMETER   "Address Space Granularity" is invalid for this PCI root bridge.
  
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_SUBMIT_RESOURCES)(
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL   *This,
  IN EFI_HANDLE                                         RootBridgeHandle,
  IN VOID                                               *Configuration
  );

/**
  Returns the proposed resource settings for the specified PCI root bridge.

  @param[in]  This               The pointer to the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL 
                                 instance.
  @param[in]  RootBridgeHandle   The PCI root bridge handle.
  @param[out] Configuration      The pointer to the pointer to the PCI I/O and memory resource descriptor.

  @retval EFI_SUCCESS             The requested parameters were returned.
  @retval EFI_INVALID_PARAMETER   RootBridgeHandle is not a valid root bridge handle.
  @retval EFI_DEVICE_ERROR        Programming failed due to a hardware error.
  @retval EFI_OUT_OF_RESOURCES    The request could not be completed due to a lack of resources.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_GET_PROPOSED_RESOURCES)(
  IN  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL  *This,
  IN  EFI_HANDLE                                        RootBridgeHandle,
  OUT VOID                                              **Configuration
  );

/**
  Provides the hooks from the PCI bus driver to every PCI controller (device/function) at various 
  stages of the PCI enumeration process that allow the host bridge driver to preinitialize individual 
  PCI controllers before enumeration.

  @param[in]  This                  The pointer to the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL instance.
  @param[in]  RootBridgeHandle      The associated PCI root bridge handle.
  @param[in]  PciAddress            The address of the PCI device on the PCI bus.
  @param[in]  Phase                 The phase of the PCI device enumeration.

  @retval EFI_SUCCESS             The requested parameters were returned.
  @retval EFI_INVALID_PARAMETER   RootBridgeHandle is not a valid root bridge handle.
  @retval EFI_INVALID_PARAMETER   Phase is not a valid phase that is defined in 
                                  EFI_PCI_CONTROLLER_RESOURCE_ALLOCATION_PHASE.
  @retval EFI_DEVICE_ERROR        Programming failed due to a hardware error. The PCI enumerator 
                                  should not enumerate this device, including its child devices if it is 
                                  a PCI-to-PCI bridge.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_PREPROCESS_CONTROLLER)(
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL  *This,
  IN EFI_HANDLE                                        RootBridgeHandle,
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS       PciAddress,
  IN EFI_PCI_CONTROLLER_RESOURCE_ALLOCATION_PHASE      Phase
  );

///
/// Provides the basic interfaces to abstract a PCI host bridge resource allocation.
///
struct _EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL {
  ///
  /// The notification from the PCI bus enumerator that it is about to enter
  /// a certain phase during the enumeration process.
  ///
  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_NOTIFY_PHASE           NotifyPhase;
  
  ///
  /// Retrieves the device handle for the next PCI root bridge that is produced by the
  /// host bridge to which this instance of the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL is attached.  
  ///
  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_GET_NEXT_ROOT_BRIDGE   GetNextRootBridge;
  
  ///
  /// Retrieves the allocation-related attributes of a PCI root bridge.
  ///
  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_GET_ATTRIBUTES         GetAllocAttributes;
  
  ///
  /// Sets up a PCI root bridge for bus enumeration.
  ///
  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_START_BUS_ENUMERATION  StartBusEnumeration;
  
  ///
  /// Sets up the PCI root bridge so that it decodes a specific range of bus numbers.
  ///
  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_SET_BUS_NUMBERS        SetBusNumbers;
  
  ///
  /// Submits the resource requirements for the specified PCI root bridge.
  ///
  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_SUBMIT_RESOURCES       SubmitResources;
  
  ///
  /// Returns the proposed resource assignment for the specified PCI root bridges.
  ///
  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_GET_PROPOSED_RESOURCES GetProposedResources;
  
  ///
  /// Provides hooks from the PCI bus driver to every PCI controller
  /// (device/function) at various stages of the PCI enumeration process that
  /// allow the host bridge driver to preinitialize individual PCI controllers
  /// before enumeration.  
  ///
  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL_PREPROCESS_CONTROLLER  PreprocessController;
};

extern EFI_GUID gEfiPciHostBridgeResourceAllocationProtocolGuid;

#endif
