/** @file
This header file declares functions and structures.

Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef MIPI_SYST_PLATFORM_H_
#define MIPI_SYST_PLATFORM_H_

typedef struct {
  UINT64    MmioAddr;
} TRACE_HUB_PLATFORM_SYST_DATA;

struct mipi_syst_platform_handle {
  TRACE_HUB_PLATFORM_SYST_DATA    TraceHubPlatformData;
};

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

/**
  Clear data in Trace Hub MMIO addr + 0x30.

  @param[in]  MipiSystHandle  A pointer to MIPI_SYST_HANDLE structure.
**/
VOID
EFIAPI
MipiSystWriteFlag (
  IN  VOID  *MipiSystHandle
  );

#define MIPI_SYST_PLATFORM_CLOCK()  1000 // (unit: MicroSecond)

#ifndef MIPI_SYST_PCFG_ENABLE_PLATFORM_STATE_DATA
#define MIPI_SYST_OUTPUT_D32TS(MipiSystHandle, Data)   MipiSystWriteD32Ts ((MipiSystHandle), (Data))
#define MIPI_SYST_OUTPUT_D32MTS(MipiSystHandle, Data)  MipiSystWriteD32Mts ((MipiSystHandle), (Data))
#define MIPI_SYST_OUTPUT_D64MTS(MipiSystHandle, Data)  MipiSystWriteD64Mts ((MipiSystHandle), (Data))
#define MIPI_SYST_OUTPUT_D8(MipiSystHandle, Data)      MipiSystWriteD8 ((MipiSystHandle), (Data))
#define MIPI_SYST_OUTPUT_D16(MipiSystHandle, Data)     MipiSystWriteD16 ((MipiSystHandle), (Data))
#define MIPI_SYST_OUTPUT_D32(MipiSystHandle, Data)     MipiSystWriteD32 ((MipiSystHandle), (Data))
  #if defined (MIPI_SYST_PCFG_ENABLE_64BIT_IO)
#define MIPI_SYST_OUTPUT_D64(MipiSystHandle, Data)  MipiSystWriteD64 ((MipiSystHandle), (Data))
  #endif
#define MIPI_SYST_OUTPUT_FLAG(MipiSystHandle)  MipiSystWriteFlag ((MipiSystHandle))
#endif

#endif // MIPI_SYST_PLATFORM_H_
