/** @file
  EFI MM Control Protocol as defined in the PI 1.5 specification.

  This protocol is used initiate synchronous MMI activations. This protocol could be published by a
  processor driver to abstract the MMI IPI or a driver which abstracts the ASIC that is supporting the
  APM port. Because of the possibility of performing MMI IPI transactions, the ability to generate this
  event from a platform chipset agent is an optional capability for both IA-32 and x64-based systems.

  The EFI_MM_CONTROL_PROTOCOL is produced by a runtime driver. It provides  an
  abstraction of the platform hardware that generates an MMI.  There are often I/O ports that, when
  accessed, will generate the MMI.  Also, the hardware optionally supports the periodic generation of
  these signals.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _MM_CONTROL_H_
#define _MM_CONTROL_H_

#include <PiDxe.h>

#define EFI_MM_CONTROL_PROTOCOL_GUID \
  { \
    0x843dc720, 0xab1e, 0x42cb, {0x93, 0x57, 0x8a, 0x0, 0x78, 0xf3, 0x56, 0x1b}  \
  }

typedef struct _EFI_MM_CONTROL_PROTOCOL  EFI_MM_CONTROL_PROTOCOL;
typedef UINTN  EFI_MM_PERIOD;

/**
  Invokes MMI activation from either the preboot or runtime environment.

  This function generates an MMI.

  @param[in]     This                The EFI_MM_CONTROL_PROTOCOL instance.
  @param[in,out] CommandPort         The value written to the command port.
  @param[in,out] DataPort            The value written to the data port.
  @param[in]     Periodic            Optional mechanism to engender a periodic stream.
  @param[in]     ActivationInterval  Optional parameter to repeat at this period one
                                     time or, if the Periodic Boolean is set, periodically.

  @retval EFI_SUCCESS            The MMI/PMI has been engendered.
  @retval EFI_DEVICE_ERROR       The timing is unsupported.
  @retval EFI_INVALID_PARAMETER  The activation period is unsupported.
  @retval EFI_INVALID_PARAMETER  The last periodic activation has not been cleared.
  @retval EFI_NOT_STARTED        The MM base service has not been initialized.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MM_ACTIVATE)(
  IN CONST EFI_MM_CONTROL_PROTOCOL    *This,
  IN OUT UINT8                        *CommandPort       OPTIONAL,
  IN OUT UINT8                        *DataPort          OPTIONAL,
  IN BOOLEAN                          Periodic           OPTIONAL,
  IN UINTN                            ActivationInterval OPTIONAL
  );

/**
  Clears any system state that was created in response to the Trigger() call.

  This function acknowledges and causes the deassertion of the MMI activation source.

  @param[in] This                The EFI_MM_CONTROL_PROTOCOL instance.
  @param[in] Periodic            Optional parameter to repeat at this period one time

  @retval EFI_SUCCESS            The MMI/PMI has been engendered.
  @retval EFI_DEVICE_ERROR       The source could not be cleared.
  @retval EFI_INVALID_PARAMETER  The service did not support the Periodic input argument.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MM_DEACTIVATE)(
  IN CONST EFI_MM_CONTROL_PROTOCOL    *This,
  IN BOOLEAN                          Periodic OPTIONAL
  );

///
/// The EFI_MM_CONTROL_PROTOCOL is produced by a runtime driver. It provides  an
/// abstraction of the platform hardware that generates an MMI.  There are often I/O ports that, when
/// accessed, will generate the MMI.  Also, the hardware optionally supports the periodic generation of
/// these signals.
///
struct _EFI_MM_CONTROL_PROTOCOL {
  EFI_MM_ACTIVATE    Trigger;
  EFI_MM_DEACTIVATE  Clear;
  ///
  /// Minimum interval at which the platform can set the period.  A maximum is not
  /// specified in that the MM infrastructure code can emulate a maximum interval that is
  /// greater than the hardware capabilities by using software emulation in the MM
  /// infrastructure code.
  ///
  EFI_MM_PERIOD      MinimumTriggerPeriod;
};

extern EFI_GUID gEfiMmControlProtocolGuid;

#endif

