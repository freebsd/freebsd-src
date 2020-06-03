/** @file
  Timer Architectural Protocol as defined in PI Specification VOLUME 2 DXE

  This code is used to provide the timer tick for the DXE core.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ARCH_PROTOCOL_TIMER_H__
#define __ARCH_PROTOCOL_TIMER_H__

///
/// Global ID for the Timer Architectural Protocol
///
#define EFI_TIMER_ARCH_PROTOCOL_GUID \
  { 0x26baccb3, 0x6f42, 0x11d4, {0xbc, 0xe7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } }

///
/// Declare forward reference for the Timer Architectural Protocol
///
typedef struct _EFI_TIMER_ARCH_PROTOCOL   EFI_TIMER_ARCH_PROTOCOL;

/**
  This function of this type is called when a timer interrupt fires.  This
  function executes at TPL_HIGH_LEVEL.  The DXE Core will register a function
  of this type to be called for the timer interrupt, so it can know how much
  time has passed.  This information is used to signal timer based events.

  @param  Time   Time since the last timer interrupt in 100 ns units. This will
                 typically be TimerPeriod, but if a timer interrupt is missed, and the
                 EFI_TIMER_ARCH_PROTOCOL driver can detect missed interrupts, then Time
                 will contain the actual amount of time since the last interrupt.

  None.

**/
typedef
VOID
(EFIAPI *EFI_TIMER_NOTIFY)(
  IN UINT64  Time
  );

/**
  This function registers the handler NotifyFunction so it is called every time
  the timer interrupt fires.  It also passes the amount of time since the last
  handler call to the NotifyFunction.  If NotifyFunction is NULL, then the
  handler is unregistered.  If the handler is registered, then EFI_SUCCESS is
  returned.  If the CPU does not support registering a timer interrupt handler,
  then EFI_UNSUPPORTED is returned.  If an attempt is made to register a handler
  when a handler is already registered, then EFI_ALREADY_STARTED is returned.
  If an attempt is made to unregister a handler when a handler is not registered,
  then EFI_INVALID_PARAMETER is returned.  If an error occurs attempting to
  register the NotifyFunction with the timer interrupt, then EFI_DEVICE_ERROR
  is returned.

  @param  This             The EFI_TIMER_ARCH_PROTOCOL instance.
  @param  NotifyFunction   The function to call when a timer interrupt fires. This
                           function executes at TPL_HIGH_LEVEL. The DXE Core will
                           register a handler for the timer interrupt, so it can know
                           how much time has passed. This information is used to
                           signal timer based events. NULL will unregister the handler.

  @retval EFI_SUCCESS           The timer handler was registered.
  @retval EFI_UNSUPPORTED       The platform does not support timer interrupts.
  @retval EFI_ALREADY_STARTED   NotifyFunction is not NULL, and a handler is already
                                registered.
  @retval EFI_INVALID_PARAMETER NotifyFunction is NULL, and a handler was not
                                previously registered.
  @retval EFI_DEVICE_ERROR      The timer handler could not be registered.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TIMER_REGISTER_HANDLER)(
  IN EFI_TIMER_ARCH_PROTOCOL    *This,
  IN EFI_TIMER_NOTIFY           NotifyFunction
);

/**
  This function adjusts the period of timer interrupts to the value specified
  by TimerPeriod.  If the timer period is updated, then the selected timer
  period is stored in EFI_TIMER.TimerPeriod, and EFI_SUCCESS is returned.  If
  the timer hardware is not programmable, then EFI_UNSUPPORTED is returned.
  If an error occurs while attempting to update the timer period, then the
  timer hardware will be put back in its state prior to this call, and
  EFI_DEVICE_ERROR is returned.  If TimerPeriod is 0, then the timer interrupt
  is disabled.  This is not the same as disabling the CPU's interrupts.
  Instead, it must either turn off the timer hardware, or it must adjust the
  interrupt controller so that a CPU interrupt is not generated when the timer
  interrupt fires.

  @param  This             The EFI_TIMER_ARCH_PROTOCOL instance.
  @param  TimerPeriod      The rate to program the timer interrupt in 100 nS units. If
                           the timer hardware is not programmable, then EFI_UNSUPPORTED is
                           returned. If the timer is programmable, then the timer period
                           will be rounded up to the nearest timer period that is supported
                           by the timer hardware. If TimerPeriod is set to 0, then the
                           timer interrupts will be disabled.

  @retval EFI_SUCCESS           The timer period was changed.
  @retval EFI_UNSUPPORTED       The platform cannot change the period of the timer interrupt.
  @retval EFI_DEVICE_ERROR      The timer period could not be changed due to a device error.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TIMER_SET_TIMER_PERIOD)(
  IN EFI_TIMER_ARCH_PROTOCOL    *This,
  IN UINT64                     TimerPeriod
  );

/**
  This function retrieves the period of timer interrupts in 100 ns units,
  returns that value in TimerPeriod, and returns EFI_SUCCESS.  If TimerPeriod
  is NULL, then EFI_INVALID_PARAMETER is returned.  If a TimerPeriod of 0 is
  returned, then the timer is currently disabled.

  @param  This             The EFI_TIMER_ARCH_PROTOCOL instance.
  @param  TimerPeriod      A pointer to the timer period to retrieve in 100 ns units. If
                           0 is returned, then the timer is currently disabled.

  @retval EFI_SUCCESS           The timer period was returned in TimerPeriod.
  @retval EFI_INVALID_PARAMETER TimerPeriod is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TIMER_GET_TIMER_PERIOD)(
  IN EFI_TIMER_ARCH_PROTOCOL      *This,
  OUT UINT64                      *TimerPeriod
  );

/**
  This function generates a soft timer interrupt. If the platform does not support soft
  timer interrupts, then EFI_UNSUPPORTED is returned. Otherwise, EFI_SUCCESS is returned.
  If a handler has been registered through the EFI_TIMER_ARCH_PROTOCOL.RegisterHandler()
  service, then a soft timer interrupt will be generated. If the timer interrupt is
  enabled when this service is called, then the registered handler will be invoked. The
  registered handler should not be able to distinguish a hardware-generated timer
  interrupt from a software-generated timer interrupt.

  @param  This                  The EFI_TIMER_ARCH_PROTOCOL instance.

  @retval EFI_SUCCESS           The soft timer interrupt was generated.
  @retval EFI_UNSUPPORTED       The platform does not support the generation of soft timer interrupts.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_TIMER_GENERATE_SOFT_INTERRUPT)(
  IN EFI_TIMER_ARCH_PROTOCOL    *This
  );


///
/// This protocol provides the services to initialize a periodic timer
/// interrupt, and to register a handler that is called each time the timer
/// interrupt fires.  It may also provide a service to adjust the rate of the
/// periodic timer interrupt.  When a timer interrupt occurs, the handler is
/// passed the amount of time that has passed since the previous timer
/// interrupt.
///
struct _EFI_TIMER_ARCH_PROTOCOL {
  EFI_TIMER_REGISTER_HANDLER          RegisterHandler;
  EFI_TIMER_SET_TIMER_PERIOD          SetTimerPeriod;
  EFI_TIMER_GET_TIMER_PERIOD          GetTimerPeriod;
  EFI_TIMER_GENERATE_SOFT_INTERRUPT   GenerateSoftInterrupt;
};

extern EFI_GUID gEfiTimerArchProtocolGuid;

#endif
