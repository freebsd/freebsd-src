/** @file
  Simple Pointer protocol from the UEFI 2.0 specification.

  Abstraction of a very simple pointer device like a mouse or trackball.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SIMPLE_POINTER_H__
#define __SIMPLE_POINTER_H__

#define EFI_SIMPLE_POINTER_PROTOCOL_GUID \
  { \
    0x31878c87, 0xb75, 0x11d5, {0x9a, 0x4f, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

typedef struct _EFI_SIMPLE_POINTER_PROTOCOL  EFI_SIMPLE_POINTER_PROTOCOL;

//
// Data structures
//
typedef struct {
  ///
  /// The signed distance in counts that the pointer device has been moved along the x-axis.
  ///
  INT32   RelativeMovementX;
  ///
  /// The signed distance in counts that the pointer device has been moved along the y-axis.
  ///
  INT32   RelativeMovementY;
  ///
  /// The signed distance in counts that the pointer device has been moved along the z-axis.
  ///
  INT32   RelativeMovementZ;
  ///
  /// If TRUE, then the left button of the pointer device is being
  /// pressed. If FALSE, then the left button of the pointer device is not being pressed.
  ///
  BOOLEAN LeftButton;
  ///
  /// If TRUE, then the right button of the pointer device is being
  /// pressed. If FALSE, then the right button of the pointer device is not being pressed.
  ///
  BOOLEAN RightButton;
} EFI_SIMPLE_POINTER_STATE;

typedef struct {
  ///
  /// The resolution of the pointer device on the x-axis in counts/mm.
  /// If 0, then the pointer device does not support an x-axis.
  ///
  UINT64  ResolutionX;
  ///
  /// The resolution of the pointer device on the y-axis in counts/mm.
  /// If 0, then the pointer device does not support an x-axis.
  ///
  UINT64  ResolutionY;
  ///
  /// The resolution of the pointer device on the z-axis in counts/mm.
  /// If 0, then the pointer device does not support an x-axis.
  ///
  UINT64  ResolutionZ;
  ///
  /// TRUE if a left button is present on the pointer device. Otherwise FALSE.
  ///
  BOOLEAN LeftButton;
  ///
  /// TRUE if a right button is present on the pointer device. Otherwise FALSE.
  ///
  BOOLEAN RightButton;
} EFI_SIMPLE_POINTER_MODE;

/**
  Resets the pointer device hardware.

  @param  This                  A pointer to the EFI_SIMPLE_POINTER_PROTOCOL
                                instance.
  @param  ExtendedVerification  Indicates that the driver may perform a more exhaustive
                                verification operation of the device during reset.

  @retval EFI_SUCCESS           The device was reset.
  @retval EFI_DEVICE_ERROR      The device is not functioning correctly and could not be reset.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SIMPLE_POINTER_RESET)(
  IN EFI_SIMPLE_POINTER_PROTOCOL            *This,
  IN BOOLEAN                                ExtendedVerification
  );

/**
  Retrieves the current state of a pointer device.

  @param  This                  A pointer to the EFI_SIMPLE_POINTER_PROTOCOL
                                instance.
  @param  State                 A pointer to the state information on the pointer device.

  @retval EFI_SUCCESS           The state of the pointer device was returned in State.
  @retval EFI_NOT_READY         The state of the pointer device has not changed since the last call to
                                GetState().
  @retval EFI_DEVICE_ERROR      A device error occurred while attempting to retrieve the pointer device's
                                current state.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SIMPLE_POINTER_GET_STATE)(
  IN EFI_SIMPLE_POINTER_PROTOCOL          *This,
  OUT EFI_SIMPLE_POINTER_STATE            *State
  );

///
/// The EFI_SIMPLE_POINTER_PROTOCOL provides a set of services for a pointer
/// device that can use used as an input device from an application written
/// to this specification. The services include the ability to reset the
/// pointer device, retrieve get the state of the pointer device, and
/// retrieve the capabilities of the pointer device.
///
struct _EFI_SIMPLE_POINTER_PROTOCOL {
  EFI_SIMPLE_POINTER_RESET      Reset;
  EFI_SIMPLE_POINTER_GET_STATE  GetState;
  ///
  /// Event to use with WaitForEvent() to wait for input from the pointer device.
  ///
  EFI_EVENT                     WaitForInput;
  ///
  /// Pointer to EFI_SIMPLE_POINTER_MODE data.
  ///
  EFI_SIMPLE_POINTER_MODE       *Mode;
};

extern EFI_GUID gEfiSimplePointerProtocolGuid;

#endif
