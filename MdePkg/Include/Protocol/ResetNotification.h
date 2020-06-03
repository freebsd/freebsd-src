/** @file
  EFI Reset Notification Protocol as defined in UEFI 2.7.
  This protocol provides services to register for a notification when ResetSystem is called.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol is introduced in UEFI Specification 2.7

**/

#ifndef __EFI_RESET_NOTIFICATION_H__
#define __EFI_RESET_NOTIFICATION_H__

#define EFI_RESET_NOTIFICATION_PROTOCOL_GUID \
  { 0x9da34ae0, 0xeaf9, 0x4bbf, { 0x8e, 0xc3, 0xfd, 0x60, 0x22, 0x6c, 0x44, 0xbe } }

typedef struct _EFI_RESET_NOTIFICATION_PROTOCOL EFI_RESET_NOTIFICATION_PROTOCOL;

/**
  Register a notification function to be called when ResetSystem() is called.

  The RegisterResetNotify() function registers a notification function that is called when
  ResetSystem()is called and prior to completing the reset of the platform.
  The registered functions must not perform a platform reset themselves. These
  notifications are intended only for the notification of components which may need some
  special-purpose maintenance prior to the platform resetting.
  The list of registered reset notification functions are processed if ResetSystem()is called
  before ExitBootServices(). The list of registered reset notification functions is ignored if
  ResetSystem()is called after ExitBootServices().

  @param[in]  This              A pointer to the EFI_RESET_NOTIFICATION_PROTOCOL instance.
  @param[in]  ResetFunction     Points to the function to be called when a ResetSystem() is executed.

  @retval EFI_SUCCESS           The reset notification function was successfully registered.
  @retval EFI_INVALID_PARAMETER ResetFunction is NULL.
  @retval EFI_OUT_OF_RESOURCES  There are not enough resources available to register the reset notification function.
  @retval EFI_ALREADY_STARTED   The reset notification function specified by ResetFunction has already been registered.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REGISTER_RESET_NOTIFY) (
  IN EFI_RESET_NOTIFICATION_PROTOCOL *This,
  IN EFI_RESET_SYSTEM                ResetFunction
);

/**
  Unregister a notification function.

  The UnregisterResetNotify() function removes the previously registered
  notification using RegisterResetNotify().

  @param[in]  This              A pointer to the EFI_RESET_NOTIFICATION_PROTOCOL instance.
  @param[in]  ResetFunction     The pointer to the ResetFunction being unregistered.

  @retval EFI_SUCCESS           The reset notification function was unregistered.
  @retval EFI_INVALID_PARAMETER ResetFunction is NULL.
  @retval EFI_INVALID_PARAMETER The reset notification function specified by ResetFunction was not previously
                                registered using RegisterResetNotify().

**/
typedef
EFI_STATUS
(EFIAPI *EFI_UNREGISTER_RESET_NOTIFY) (
  IN EFI_RESET_NOTIFICATION_PROTOCOL *This,
  IN EFI_RESET_SYSTEM                ResetFunction
);

typedef struct _EFI_RESET_NOTIFICATION_PROTOCOL {
  EFI_REGISTER_RESET_NOTIFY   RegisterResetNotify;
  EFI_UNREGISTER_RESET_NOTIFY UnregisterResetNotify;
} EFI_RESET_NOTIFICATION_PROTOCOL;


extern EFI_GUID gEfiResetNotificationProtocolGuid;

#endif

