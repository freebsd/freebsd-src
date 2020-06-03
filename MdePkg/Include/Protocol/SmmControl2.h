/** @file
  EFI SMM Control2 Protocol as defined in the PI 1.2 specification.

  This protocol is used initiate synchronous SMI activations. This protocol could be published by a
  processor driver to abstract the SMI IPI or a driver which abstracts the ASIC that is supporting the
  APM port. Because of the possibility of performing SMI IPI transactions, the ability to generate this
  event from a platform chipset agent is an optional capability for both IA-32 and x64-based systems.

  The EFI_SMM_CONTROL2_PROTOCOL is produced by a runtime driver. It provides  an
  abstraction of the platform hardware that generates an SMI.  There are often I/O ports that, when
  accessed, will generate the SMI.  Also, the hardware optionally supports the periodic generation of
  these signals.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SMM_CONTROL2_H_
#define _SMM_CONTROL2_H_

#include <Protocol/MmControl.h>

#define EFI_SMM_CONTROL2_PROTOCOL_GUID EFI_MM_CONTROL_PROTOCOL_GUID

typedef EFI_MM_CONTROL_PROTOCOL  EFI_SMM_CONTROL2_PROTOCOL;
typedef EFI_MM_PERIOD  EFI_SMM_PERIOD;

typedef EFI_MM_ACTIVATE EFI_SMM_ACTIVATE2;

typedef EFI_MM_DEACTIVATE EFI_SMM_DEACTIVATE2;
extern EFI_GUID gEfiSmmControl2ProtocolGuid;

#endif

