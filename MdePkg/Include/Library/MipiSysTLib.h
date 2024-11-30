/** @file
This header file declares functions consuming MIPI Sys-T submodule.

Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef MIPI_SYST_LIB_H_
#define MIPI_SYST_LIB_H_

/**
  Invoke initialization function in Mipi Sys-T module to initialize Mipi Sys-T handle.

  @param[in, out]  MipiSystHandle  A pointer to MIPI_SYST_HANDLE structure.

  @retval RETURN_SUCCESS      MIPI_SYST_HANDLE instance was initialized.
  @retval Other               MIPI_SYST_HANDLE instance was not initialized.
**/
RETURN_STATUS
EFIAPI
InitMipiSystHandle (
  IN OUT VOID  *MipiSystHandle
  );

/**
  Invoke write_debug_string function in Mipi Sys-T module.

  @param[in]  MipiSystHandle  A pointer to MIPI_SYST_HANDLE structure.
  @param[in]  Severity        Severity type of input message.
  @param[in]  Len             Length of data buffer.
  @param[in]  Str             A pointer to data buffer.

  @retval RETURN_SUCCESS               Data in buffer was processed.
  @retval RETURN_ABORTED               No data need to be written to Trace Hub.
  @retval RETURN_INVALID_PARAMETER     On entry, MipiSystHandle or Str is a NULL pointer.
**/
RETURN_STATUS
EFIAPI
MipiSystWriteDebug (
  IN        VOID    *MipiSystHandle,
  IN        UINT32  Severity,
  IN        UINT16  Len,
  IN CONST  CHAR8   *Str
  );

/**
  Invoke catalog_write_message function in Mipi Sys-T module.

  @param[in]  MipiSystHandle  A pointer to MIPI_SYST_HANDLE structure.
  @param[in]  Severity        Severity type of input message.
  @param[in]  CatId           Catalog Id.

  @retval RETURN_SUCCESS               Data in buffer was processed.
  @retval RETURN_INVALID_PARAMETER     On entry, MipiSystHandle is a NULL pointer.
**/
RETURN_STATUS
EFIAPI
MipiSystWriteCatalog (
  IN  VOID    *MipiSystHandle,
  IN  UINT32  Severity,
  IN  UINT64  CatId
  );

#endif // MIPI_SYST_LIB_H_
