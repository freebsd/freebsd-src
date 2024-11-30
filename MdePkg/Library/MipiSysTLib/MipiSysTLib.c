/** @file
This file provide functions to communicate with mipi sys-T submodule.

Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include "mipi_syst.h"

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
  )
{
  MIPI_SYST_HANDLE  *MipiSystH;

  MipiSystH = (MIPI_SYST_HANDLE *)MipiSystHandle;
  if (MipiSystH == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  mipi_syst_init (MipiSystH->systh_header, 0, NULL);

  return RETURN_SUCCESS;
}

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
  IN        MIPI_SYST_HANDLE  *MipiSystHandle,
  IN        UINT32            Severity,
  IN        UINT16            Len,
  IN CONST  CHAR8             *Str
  )
{
  MIPI_SYST_HANDLE  *MipiSystH;

  MipiSystH = (MIPI_SYST_HANDLE *)MipiSystHandle;
  if (MipiSystH == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Len == 0) {
    //
    // No data need to be written to Trace Hub
    //
    return RETURN_ABORTED;
  }

  if (Str == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  mipi_syst_write_debug_string (
    MipiSystH,
    MIPI_SYST_NOLOCATION,
    MIPI_SYST_STRING_GENERIC,
    Severity,
    Len,
    Str
    );

  return RETURN_SUCCESS;
}

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
  IN  MIPI_SYST_HANDLE  *MipiSystHandle,
  IN  UINT32            Severity,
  IN  UINT64            CatId
  )
{
  MIPI_SYST_HANDLE  *MipiSystH;

  MipiSystH = (MIPI_SYST_HANDLE *)MipiSystHandle;
  if (MipiSystH == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  mipi_syst_write_catalog64_message (
    MipiSystH,
    MIPI_SYST_NOLOCATION,
    Severity,
    CatId
    );

  return RETURN_SUCCESS;
}
