/** @file
  EFI MM Control PPI definition.

  This PPI is used initiate synchronous MMI activations. This PPI could be published by a processor
  driver to abstract the MMI IPI or a driver which abstracts the ASIC that is supporting the APM port.
  Because of the possibility of performing MMI IPI transactions, the ability to generate this event
  from a platform chipset agent is an optional capability for both IA-32 and x64-based systems.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.5.

**/


#ifndef _MM_CONTROL_PPI_H_
#define _MM_CONTROL_PPI_H_

#define EFI_PEI_MM_CONTROL_PPI_GUID \
  { 0x61c68702, 0x4d7e, 0x4f43, 0x8d, 0xef, 0xa7, 0x43, 0x5, 0xce, 0x74, 0xc5 }

typedef struct _EFI_PEI_MM_CONTROL_PPI  EFI_PEI_MM_CONTROL_PPI;

/**
  Invokes PPI activation from the PI PEI environment.

  @param  PeiServices           An indirect pointer to the PEI Services Table published by the PEI Foundation.
  @param  This                  The PEI_MM_CONTROL_PPI instance.
  @param  ArgumentBuffer        The value passed to the MMI handler. This value corresponds to the
                                SwMmiInputValue in the RegisterContext parameter for the Register()
                                function in the EFI_MM_SW_DISPATCH_PROTOCOL and in the Context parameter
                                in the call to the DispatchFunction
  @param  ArgumentBufferSize    The size of the data passed in ArgumentBuffer or NULL if ArgumentBuffer is NULL.
  @param  Periodic              An optional mechanism to periodically repeat activation.
  @param  ActivationInterval    An optional parameter to repeat at this period one
                                time or, if the Periodic Boolean is set, periodically.

  @retval EFI_SUCCESS           The MMI has been engendered.
  @retval EFI_DEVICE_ERROR      The timing is unsupported.
  @retval EFI_INVALID_PARAMETER The activation period is unsupported.
  @retval EFI_NOT_STARTED       The MM base service has not been initialized.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MM_ACTIVATE) (
  IN EFI_PEI_SERVICES                                **PeiServices,
  IN EFI_PEI_MM_CONTROL_PPI                          * This,
  IN OUT INT8                                        *ArgumentBuffer OPTIONAL,
  IN OUT UINTN                                       *ArgumentBufferSize OPTIONAL,
  IN BOOLEAN                                         Periodic OPTIONAL,
  IN UINTN                                           ActivationInterval OPTIONAL
  );

/**
  Clears any system state that was created in response to the Trigger() call.

  @param  PeiServices           General purpose services available to every PEIM.
  @param  This                  The PEI_MM_CONTROL_PPI instance.
  @param  Periodic              Optional parameter to repeat at this period one
                                time or, if the Periodic Boolean is set, periodically.

  @retval EFI_SUCCESS           The MMI has been engendered.
  @retval EFI_DEVICE_ERROR      The source could not be cleared.
  @retval EFI_INVALID_PARAMETER The service did not support the Periodic input argument.

**/
typedef
EFI_STATUS
(EFIAPI *PEI_MM_DEACTIVATE) (
  IN EFI_PEI_SERVICES                      **PeiServices,
  IN EFI_PEI_MM_CONTROL_PPI                * This,
  IN BOOLEAN                               Periodic OPTIONAL
  );

///
///  The EFI_PEI_MM_CONTROL_PPI is produced by a PEIM. It provides an abstraction of the
///  platform hardware that generates an MMI. There are often I/O ports that, when accessed, will
///  generate the MMI. Also, the hardware optionally supports the periodic generation of these signals.
///
struct _PEI_MM_CONTROL_PPI {
  PEI_MM_ACTIVATE    Trigger;
  PEI_MM_DEACTIVATE  Clear;
};

extern EFI_GUID gEfiPeiMmControlPpiGuid;

#endif
