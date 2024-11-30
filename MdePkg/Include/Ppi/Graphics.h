/** @file
  This file declares Graphics PPI.
  This PPI is the main interface exposed by the Graphics PEIM to be used by the
  other firmware modules.

  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.4.

**/

#ifndef __PEI_GRAPHICS_PPI_H__
#define __PEI_GRAPHICS_PPI_H__

#include <Protocol/GraphicsOutput.h>

#define EFI_PEI_GRAPHICS_PPI_GUID \
  { \
    0x6ecd1463, 0x4a4a, 0x461b, { 0xaf, 0x5f, 0x5a, 0x33, 0xe3, 0xb2, 0x16, 0x2b } \
  }

typedef struct _EFI_PEI_GRAPHICS_PPI EFI_PEI_GRAPHICS_PPI;

/**
  The GraphicsPpiInit initializes the graphics subsystem in phases.

  @param[in] GraphicsPolicyPtr    GraphicsPolicyPtr points to a configuration data
                                  block of policy settings required by Graphics PEIM.

  @retval EFI_SUCCESS             The invocation was successful.
  @retval EFI_INVALID_PARAMETER   The phase parameter is not valid.
  @retval EFI_NOT_ABORTED         The stages was not called in the proper order.
  @retval EFI_NOT_FOUND           The PeiGraphicsPlatformPolicyPpi is not located.
  @retval EFI_DEVICE_ERROR        The initialization failed due to device error.
  @retval EFI_NOT_READY           The previous init stage is still in progress and not
                                  ready for the current initialization phase yet. The
                                  platform code should call this again sometime later.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_GRAPHICS_INIT)(
  IN VOID                            *GraphicsPolicyPtr
  );

/**
  The GraphicsPpiGetMode returns the mode information supported by the Graphics PEI
  Module.

  @param[in, out] Mode            Pointer to EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE data.

  @retval EFI_SUCCESS             Valid mode information was returned.
  @retval EFI_INVALID_PARAMETER   The Mode parameter is not valid.
  @retval EFI_DEVICE_ERROR        A hardware error occurred trying to retrieve the video
                                  mode.
  @retval EFI_NOT_READY           The Graphics Initialization is not competed and Mode
                                  information is not yet available.The platform code
                                  should call this again after the Graphics
                                  initialization is done.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_GRAPHICS_GET_MODE)(
  IN OUT EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  *Mode
  );

///
/// This PPI is the main interface exposed by the Graphics PEIM to be used by the other
/// firmware modules.
///
struct _EFI_PEI_GRAPHICS_PPI {
  EFI_PEI_GRAPHICS_INIT        GraphicsPpiInit;
  EFI_PEI_GRAPHICS_GET_MODE    GraphicsPpiGetMode;
};

extern EFI_GUID  gEfiPeiGraphicsPpiGuid;

#endif
