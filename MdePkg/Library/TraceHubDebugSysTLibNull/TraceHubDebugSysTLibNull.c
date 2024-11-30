/** @file
Null library of TraceHubDebugSysTLib.

Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/TraceHubDebugSysTLib.h>

/**
  Write debug string to specified Trace Hub MMIO address.

  @param[in]  SeverityType     Severity type of input message.
  @param[in]  Buffer           A pointer to the data buffer.
  @param[in]  NumberOfBytes    The size of data buffer.

  @retval RETURN_SUCCESS      Data was written to Trace Hub.
  @retval Other               Failed to output Trace Hub message.
**/
RETURN_STATUS
EFIAPI
TraceHubSysTDebugWrite (
  IN TRACE_HUB_SEVERITY_TYPE  SeverityType,
  IN UINT8                    *Buffer,
  IN UINTN                    NumberOfBytes
  )
{
  return RETURN_UNSUPPORTED;
}

/**
  Write catalog status code message to specified Trace Hub MMIO address.

  @param[in]  SeverityType     Severity type of input message.
  @param[in]  Id               Catalog ID.
  @param[in]  Guid             Driver Guid.

  @retval RETURN_SUCCESS      Data was written to Trace Hub.
  @retval Other               Failed to output Trace Hub message.
**/
RETURN_STATUS
EFIAPI
TraceHubSysTWriteCataLog64StatusCode (
  IN TRACE_HUB_SEVERITY_TYPE  SeverityType,
  IN UINT64                   Id,
  IN GUID                     *Guid
  )
{
  return RETURN_UNSUPPORTED;
}

/**
  Write catalog message to specified Trace Hub MMIO address.

  @param[in]  SeverityType   Severity type of input message.
  @param[in]  Id             Catalog ID.
  @param[in]  NumberOfParams Number of entries in argument list.
  @param[in]  ...            Catalog message parameters.

  @retval RETURN_SUCCESS      Data was written to Trace Hub.
  @retval Other               Failed to output Trace Hub message.
**/
RETURN_STATUS
EFIAPI
TraceHubSysTWriteCataLog64 (
  IN TRACE_HUB_SEVERITY_TYPE  SeverityType,
  IN UINT64                   Id,
  IN UINTN                    NumberOfParams,
  ...
  )
{
  return RETURN_UNSUPPORTED;
}
