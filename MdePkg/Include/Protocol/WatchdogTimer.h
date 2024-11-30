/** @file
  Watchdog Timer Architectural Protocol as defined in PI Specification VOLUME 2 DXE

  Used to provide system watchdog timer services

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __ARCH_PROTOCOL_WATCHDOG_TIMER_H__
#define __ARCH_PROTOCOL_WATCHDOG_TIMER_H__

///
/// Global ID for the Watchdog Timer Architectural Protocol
///
#define EFI_WATCHDOG_TIMER_ARCH_PROTOCOL_GUID \
  { 0x665E3FF5, 0x46CC, 0x11d4, {0x9A, 0x38, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } }

///
/// Declare forward reference for the Timer Architectural Protocol
///
typedef struct _EFI_WATCHDOG_TIMER_ARCH_PROTOCOL EFI_WATCHDOG_TIMER_ARCH_PROTOCOL;

/**
  A function of this type is called when the watchdog timer fires if a
  handler has been registered.

  @param  Time             The time in 100 ns units that has passed since the watchdog
                           timer was armed. For the notify function to be called, this
                           must be greater than TimerPeriod.

  @return None.

**/
typedef
VOID
(EFIAPI *EFI_WATCHDOG_TIMER_NOTIFY)(
  IN UINT64  Time
  );

/**
  This function registers a handler that is to be invoked when the watchdog
  timer fires.  By default, the EFI_WATCHDOG_TIMER protocol will call the
  Runtime Service ResetSystem() when the watchdog timer fires.  If a
  NotifyFunction is registered, then the NotifyFunction will be called before
  the Runtime Service ResetSystem() is called.  If NotifyFunction is NULL, then
  the watchdog handler is unregistered.  If a watchdog handler is registered,
  then EFI_SUCCESS is returned.  If an attempt is made to register a handler
  when a handler is already registered, then EFI_ALREADY_STARTED is returned.
  If an attempt is made to uninstall a handler when a handler is not installed,
  then return EFI_INVALID_PARAMETER.

  @param  This             The EFI_WATCHDOG_TIMER_ARCH_PROTOCOL instance.
  @param  NotifyFunction   The function to call when the watchdog timer fires. If this
                           is NULL, then the handler will be unregistered.

  @retval EFI_SUCCESS           The watchdog timer handler was registered or
                                unregistered.
  @retval EFI_ALREADY_STARTED   NotifyFunction is not NULL, and a handler is already
                                registered.
  @retval EFI_INVALID_PARAMETER NotifyFunction is NULL, and a handler was not
                                previously registered.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_WATCHDOG_TIMER_REGISTER_HANDLER)(
  IN EFI_WATCHDOG_TIMER_ARCH_PROTOCOL  *This,
  IN EFI_WATCHDOG_TIMER_NOTIFY         NotifyFunction
  );

/**
  This function sets the amount of time to wait before firing the watchdog
  timer to TimerPeriod 100 nS units.  If TimerPeriod is 0, then the watchdog
  timer is disabled.

  @param  This             The EFI_WATCHDOG_TIMER_ARCH_PROTOCOL instance.
  @param  TimerPeriod      The amount of time in 100 nS units to wait before the watchdog
                           timer is fired. If TimerPeriod is zero, then the watchdog
                           timer is disabled.

  @retval EFI_SUCCESS           The watchdog timer has been programmed to fire in Time
                                100 nS units.
  @retval EFI_DEVICE_ERROR      A watchdog timer could not be programmed due to a device
                                error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_WATCHDOG_TIMER_SET_TIMER_PERIOD)(
  IN EFI_WATCHDOG_TIMER_ARCH_PROTOCOL  *This,
  IN UINT64                            TimerPeriod
  );

/**
  This function retrieves the amount of time the system will wait before firing
  the watchdog timer.  This period is returned in TimerPeriod, and EFI_SUCCESS
  is returned.  If TimerPeriod is NULL, then EFI_INVALID_PARAMETER is returned.

  @param  This             The EFI_WATCHDOG_TIMER_ARCH_PROTOCOL instance.
  @param  TimerPeriod      A pointer to the amount of time in 100 nS units that the system
                           will wait before the watchdog timer is fired. If TimerPeriod of
                           zero is returned, then the watchdog timer is disabled.

  @retval EFI_SUCCESS           The amount of time that the system will wait before
                                firing the watchdog timer was returned in TimerPeriod.
  @retval EFI_INVALID_PARAMETER TimerPeriod is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_WATCHDOG_TIMER_GET_TIMER_PERIOD)(
  IN  EFI_WATCHDOG_TIMER_ARCH_PROTOCOL  *This,
  OUT UINT64                            *TimerPeriod
  );

///
/// This protocol provides the services required to implement the Boot Service
/// SetWatchdogTimer().  It provides a service to set the amount of time to wait
/// before firing the watchdog timer, and it also provides a service to register
/// a handler that is invoked when the watchdog timer fires.  This protocol can
/// implement the watchdog timer by using the event and timer Boot Services, or
/// it can make use of custom hardware.  When the watchdog timer fires, control
/// will be passed to a handler if one has been registered.  If no handler has
/// been registered, or the registered handler returns, then the system will be
/// reset by calling the Runtime Service ResetSystem().
///
struct _EFI_WATCHDOG_TIMER_ARCH_PROTOCOL {
  EFI_WATCHDOG_TIMER_REGISTER_HANDLER    RegisterHandler;
  EFI_WATCHDOG_TIMER_SET_TIMER_PERIOD    SetTimerPeriod;
  EFI_WATCHDOG_TIMER_GET_TIMER_PERIOD    GetTimerPeriod;
};

extern EFI_GUID  gEfiWatchdogTimerArchProtocolGuid;

#endif
