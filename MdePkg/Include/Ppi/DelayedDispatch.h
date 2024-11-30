/** @file
    EFI Delayed Dispatch PPI as defined in the PI 1.8A Specification

    Provide timed event service in PEI

    Copyright (c) 2020, American Megatrends International LLC. All rights reserved.
    Copyright (c) Microsoft Corporation.
    SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __DELAYED_DISPATCH_PPI_H__
#define __DELAYED_DISPATCH_PPI_H__

///
/// Global ID for EFI_DELAYED_DISPATCH_PPI_GUID
///
#define EFI_DELAYED_DISPATCH_PPI_GUID \
  { \
    0x869c711d, 0x649c, 0x44fe, { 0x8b, 0x9e, 0x2c, 0xbb, 0x29, 0x11, 0xc3, 0xe6} \
  }

/**
  Delayed Dispatch function.  This routine is called sometime after the required
  delay.  Upon return, if NewDelay is 0, the function is unregistered.  If NewDelay
  is not zero, this routine will be called again after the new delay period.

  @param[in,out] Context         Pointer to Context. Can be updated by routine.
  @param[out]    NewDelay        The new delay in us.  Leave at 0 to unregister callback.

**/

typedef
VOID
(EFIAPI *EFI_DELAYED_DISPATCH_FUNCTION)(
  IN OUT UINT64  *Context,
  OUT UINT32     *NewDelay
  );

///
/// The forward declaration for EFI_DELAYED_DISPATCH_PPI
///

typedef  struct _EFI_DELAYED_DISPATCH_PPI EFI_DELAYED_DISPATCH_PPI;

/**
Register a callback to be called after a minimum delay has occurred.

  @param[in] This           Pointer to the EFI_DELAYED_DISPATCH_PPI instance
  @param[in] Function       Function to call back
  @param[in] Context        Context data
  @param[in] DelayedGroupId Delayed dispatch request ID the caller will wait on
  @param[in] Delay          Delay interval

  @retval EFI_SUCCESS               Function successfully loaded
  @retval EFI_INVALID_PARAMETER     One of the Arguments is not supported
  @retval EFI_OUT_OF_RESOURCES      No more entries

**/
typedef
EFI_STATUS
(EFIAPI *EFI_DELAYED_DISPATCH_REGISTER)(
  IN  EFI_DELAYED_DISPATCH_PPI      *This,
  IN  EFI_DELAYED_DISPATCH_FUNCTION  Function,
  IN  UINT64                         Context,
  IN  EFI_GUID                       *DelayedGroupId  OPTIONAL,
  IN  UINT32                         Delay
  );

/**
  Wait on a registered Delayed Dispatch unit that has a DelayedGroupId. Continue
  to dispatch all registered delayed dispatch entries until *ALL* entries with
  DelayedGroupId have completed.

  @param[in] This            The Delayed Dispatch PPI pointer.
  @param[in] DelayedGroupId  Delayed dispatch request ID the caller will wait on

  @retval EFI_SUCCESS               Function successfully invoked
  @retval EFI_INVALID_PARAMETER     One of the Arguments is not supported

**/

typedef
EFI_STATUS
(EFIAPI *EFI_DELAYED_DISPATCH_WAIT_ON_EVENT)(
  IN  EFI_DELAYED_DISPATCH_PPI  *This,
  IN  EFI_GUID                  DelayedGroupId
  );

///
/// This PPI is a pointer to the Delayed Dispatch Service.
/// This service will be published by the Pei Foundation. The PEI Foundation
/// will use this service to relaunch a known function that requests a delayed
/// execution.
///
struct _EFI_DELAYED_DISPATCH_PPI {
  EFI_DELAYED_DISPATCH_REGISTER         Register;
  EFI_DELAYED_DISPATCH_WAIT_ON_EVENT    WaitOnEvent;
};

extern EFI_GUID  gEfiPeiDelayedDispatchPpiGuid;

#endif
