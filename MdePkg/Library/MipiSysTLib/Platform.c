/** @file
This file defines functions that output Trace Hub message.

Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/IoLib.h>
#include <Library/BaseMemoryLib.h>
#include "mipi_syst.h"

/**
  Write 4 bytes to Trace Hub MMIO addr + 0x10.

  @param[in]  MipiSystHandle  A pointer to MIPI_SYST_HANDLE structure.
  @param[in]  Data            Data to be written.
**/
VOID
EFIAPI
MipiSystWriteD32Ts (
  IN  VOID    *MipiSystHandle,
  IN  UINT32  Data
  )
{
  MIPI_SYST_HANDLE  *MipiSystH;

  MipiSystH = (MIPI_SYST_HANDLE *)MipiSystHandle;
  MmioWrite32 ((UINTN)(MipiSystH->systh_platform.TraceHubPlatformData.MmioAddr + 0x10), Data);
}

/**
  Write 4 bytes to Trace Hub MMIO addr + 0x18.

  @param[in]  MipiSystHandle  A pointer to MIPI_SYST_HANDLE structure.
  @param[in]  Data            Data to be written.
**/
VOID
EFIAPI
MipiSystWriteD32Mts (
  IN  VOID    *MipiSystHandle,
  IN  UINT32  Data
  )
{
  MIPI_SYST_HANDLE  *MipiSystH;

  MipiSystH = (MIPI_SYST_HANDLE *)MipiSystHandle;
  MmioWrite32 ((UINTN)(MipiSystH->systh_platform.TraceHubPlatformData.MmioAddr + 0x18), Data);
}

/**
  Write 8 bytes to Trace Hub MMIO addr + 0x18.

  @param[in]  MipiSystHandle  A pointer to MIPI_SYST_HANDLE structure.
  @param[in]  Data            Data to be written.
**/
VOID
EFIAPI
MipiSystWriteD64Mts (
  IN  VOID    *MipiSystHandle,
  IN  UINT64  Data
  )
{
  MIPI_SYST_HANDLE  *MipiSystH;

  MipiSystH = (MIPI_SYST_HANDLE *)MipiSystHandle;
  MmioWrite64 ((UINTN)(MipiSystH->systh_platform.TraceHubPlatformData.MmioAddr + 0x18), Data);
}

/**
  Write 1 byte to Trace Hub MMIO addr + 0x0.

  @param[in]  MipiSystHandle  A pointer to MIPI_SYST_HANDLE structure.
  @param[in]  Data            Data to be written.
**/
VOID
EFIAPI
MipiSystWriteD8 (
  IN  VOID   *MipiSystHandle,
  IN  UINT8  Data
  )
{
  MIPI_SYST_HANDLE  *MipiSystH;

  MipiSystH = (MIPI_SYST_HANDLE *)MipiSystHandle;
  MmioWrite8 ((UINTN)(MipiSystH->systh_platform.TraceHubPlatformData.MmioAddr + 0x0), Data);
}

/**
  Write 2 bytes to Trace Hub MMIO mmio addr + 0x0.

  @param[in]  MipiSystHandle  A pointer to MIPI_SYST_HANDLE structure.
  @param[in]  Data            Data to be written.
**/
VOID
EFIAPI
MipiSystWriteD16 (
  IN  VOID    *MipiSystHandle,
  IN  UINT16  Data
  )
{
  MIPI_SYST_HANDLE  *MipiSystH;

  MipiSystH = (MIPI_SYST_HANDLE *)MipiSystHandle;
  MmioWrite16 ((UINTN)(MipiSystH->systh_platform.TraceHubPlatformData.MmioAddr + 0x0), Data);
}

/**
  Write 4 bytes to Trace Hub MMIO addr + 0x0.

  @param[in]  MipiSystHandle  A pointer to MIPI_SYST_HANDLE structure.
  @param[in]  Data            Data to be written.
**/
VOID
EFIAPI
MipiSystWriteD32 (
  IN  VOID    *MipiSystHandle,
  IN  UINT32  Data
  )
{
  MIPI_SYST_HANDLE  *MipiSystH;

  MipiSystH = (MIPI_SYST_HANDLE *)MipiSystHandle;
  MmioWrite32 ((UINTN)(MipiSystH->systh_platform.TraceHubPlatformData.MmioAddr + 0x0), Data);
}

/**
  Write 8 bytes to Trace Hub MMIO addr + 0x0.

  @param[in]  MipiSystHandle  A pointer to MIPI_SYST_HANDLE structure.
  @param[in]  Data            Data to be written.
**/
VOID
EFIAPI
MipiSystWriteD64 (
  IN  VOID    *MipiSystHandle,
  IN  UINT64  Data
  )
{
  MIPI_SYST_HANDLE  *MipiSystH;

  MipiSystH = (MIPI_SYST_HANDLE *)MipiSystHandle;
  MmioWrite64 ((UINTN)(MipiSystH->systh_platform.TraceHubPlatformData.MmioAddr + 0x0), Data);
}

/**
  Clear data in Trace Hub MMIO addr + 0x30.

  @param[in]  MipiSystHandle  A pointer to MIPI_SYST_HANDLE structure.
**/
VOID
EFIAPI
MipiSystWriteFlag (
  IN  VOID  *MipiSystHandle
  )
{
  MIPI_SYST_HANDLE  *MipiSystH;

  MipiSystH = (MIPI_SYST_HANDLE *)MipiSystHandle;

  MmioWrite32 ((UINTN)(MipiSystH->systh_platform.TraceHubPlatformData.MmioAddr + 0x30), 0x0);
}
