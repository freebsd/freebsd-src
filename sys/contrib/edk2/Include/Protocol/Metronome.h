/** @file
  Metronome Architectural Protocol as defined in PI SPEC VOLUME 2 DXE

  This code abstracts the DXE core to provide delay services.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ARCH_PROTOCOL_METRONOME_H__
#define __ARCH_PROTOCOL_METRONOME_H__

///
/// Global ID for the Metronome Architectural Protocol
///
#define EFI_METRONOME_ARCH_PROTOCOL_GUID \
  { 0x26baccb2, 0x6f42, 0x11d4, {0xbc, 0xe7, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } }

///
/// Declare forward reference for the Metronome Architectural Protocol
///
typedef struct _EFI_METRONOME_ARCH_PROTOCOL EFI_METRONOME_ARCH_PROTOCOL;

/**
  The WaitForTick() function waits for the number of ticks specified by
  TickNumber from a known time source in the platform.  If TickNumber of
  ticks are detected, then EFI_SUCCESS is returned.  The actual time passed
  between entry of this function and the first tick is between 0 and
  TickPeriod 100 nS units.  If you want to guarantee that at least TickPeriod
  time has elapsed, wait for two ticks.  This function waits for a hardware
  event to determine when a tick occurs.  It is possible for interrupt
  processing, or exception processing to interrupt the execution of the
  WaitForTick() function.  Depending on the hardware source for the ticks, it
  is possible for a tick to be missed.  This function cannot guarantee that
  ticks will not be missed.  If a timeout occurs waiting for the specified
  number of ticks, then EFI_TIMEOUT is returned.

  @param  This             The EFI_METRONOME_ARCH_PROTOCOL instance.
  @param  TickNumber       Number of ticks to wait.

  @retval EFI_SUCCESS           The wait for the number of ticks specified by TickNumber
                                succeeded.
  @retval EFI_TIMEOUT           A timeout occurred waiting for the specified number of ticks.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_METRONOME_WAIT_FOR_TICK)(
  IN EFI_METRONOME_ARCH_PROTOCOL   *This,
  IN UINT32                        TickNumber
  );

///
/// This protocol provides access to a known time source in the platform to the
/// core.  The core uses this known time source to produce core services that
/// require calibrated delays.
///
struct _EFI_METRONOME_ARCH_PROTOCOL {
  EFI_METRONOME_WAIT_FOR_TICK    WaitForTick;

  ///
  /// The period of platform's known time source in 100 nS units.
  /// This value on any platform must be at least 10 uS, and must not
  /// exceed 200 uS.  The value in this field is a constant that must
  /// not be modified after the Metronome architectural protocol is
  /// installed.  All consumers must treat this as a read-only field.
  ///
  UINT32    TickPeriod;
};

extern EFI_GUID  gEfiMetronomeArchProtocolGuid;

#endif
