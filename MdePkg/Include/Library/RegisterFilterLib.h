/** @file
  Public include file for the Port IO/MMIO/MSR RegisterFilterLib.

Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef REGISTER_FILTER_LIB_H_
#define REGISTER_FILTER_LIB_H_

typedef enum {
  FilterWidth8,
  FilterWidth16,
  FilterWidth32,
  FilterWidth64
} FILTER_IO_WIDTH;

/**
  Filter IO read operation before read IO port.
  It is used to filter IO read operation.

  It will return the flag to decide whether require read real IO port.
  It can be used for emulation environment.

  @param[in]       Width    Signifies the width of the I/O operation.
  @param[in]       Address  The base address of the I/O operation.
  @param[in]       Buffer   The destination buffer to store the results.

  @retval TRUE         Need to excute the IO read.
  @retval FALSE        Skip the IO read.

**/
BOOLEAN
EFIAPI
FilterBeforeIoRead (
  IN FILTER_IO_WIDTH  Width,
  IN UINTN            Address,
  IN OUT VOID         *Buffer
  );

/**
  Trace IO read operation after read IO port.
  It is used to trace IO operation.

  @param[in]       Width    Signifies the width of the I/O operation.
  @param[in]       Address  The base address of the I/O operation.
  @param[in]       Buffer   The destination buffer to store the results.

**/
VOID
EFIAPI
FilterAfterIoRead (
  IN FILTER_IO_WIDTH  Width,
  IN UINTN            Address,
  IN VOID             *Buffer
  );

/**
  Filter IO Write operation before wirte IO port.
  It is used to filter IO operation.

  It will return the flag to decide whether require read write IO port.
  It can be used for emulation environment.

  @param[in]       Width    Signifies the width of the I/O operation.
  @param[in]       Address  The base address of the I/O operation.
  @param[in]       Buffer   The source buffer from which to BeforeWrite data.

  @retval TRUE         Need to excute the IO write.
  @retval FALSE        Skip the IO write.

**/
BOOLEAN
EFIAPI
FilterBeforeIoWrite (
  IN FILTER_IO_WIDTH  Width,
  IN UINTN            Address,
  IN VOID             *Buffer
  );

/**
Trace IO Write operation after wirte IO port.
It is used to trace IO operation.

@param[in]       Width    Signifies the width of the I/O operation.
@param[in]       Address  The base address of the I/O operation.
@param[in]       Buffer   The source buffer from which to BeforeWrite data.

**/
VOID
EFIAPI
FilterAfterIoWrite (
  IN FILTER_IO_WIDTH  Width,
  IN UINTN            Address,
  IN VOID             *Buffer
  );

/**
  Filter memory IO before Read operation.

  It will return the flag to decide whether require read real MMIO.
  It can be used for emulation environment.

  @param[in]       Width    Signifies the width of the memory I/O operation.
  @param[in]       Address  The base address of the memory I/O operation.
  @param[in]       Buffer   The destination buffer to store the results.

  @retval TRUE         Need to excute the MMIO read.
  @retval FALSE        Skip the MMIO read.

**/
BOOLEAN
EFIAPI
FilterBeforeMmIoRead (
  IN FILTER_IO_WIDTH  Width,
  IN UINTN            Address,
  IN OUT VOID         *Buffer
  );

/**
  Tracer memory IO after read operation

  @param[in]       Width    Signifies the width of the memory I/O operation.
  @param[in]       Address  The base address of the memory I/O operation.
  @param[in]       Buffer   The destination buffer to store the results.

**/
VOID
EFIAPI
FilterAfterMmIoRead (
  IN FILTER_IO_WIDTH  Width,
  IN UINTN            Address,
  IN VOID             *Buffer
  );

/**
  Filter memory IO before write operation

  It will return the flag to decide whether require wirte real MMIO.
  It can be used for emulation environment.

  @param[in]       Width    Signifies the width of the memory I/O operation.
  @param[in]       Address  The base address of the memory I/O operation.
  @param[in]       Buffer   The source buffer from which to BeforeWrite data.

  @retval TRUE         Need to excute the MMIO write.
  @retval FALSE        Skip the MMIO write.

**/
BOOLEAN
EFIAPI
FilterBeforeMmIoWrite (
  IN FILTER_IO_WIDTH  Width,
  IN UINTN            Address,
  IN VOID             *Buffer
  );

/**
  Tracer memory IO after write operation

  @param[in]       Width    Signifies the width of the memory I/O operation.
  @param[in]       Address  The base address of the memory I/O operation.
  @param[in]       Buffer   The source buffer from which to BeforeWrite data.

**/
VOID
EFIAPI
FilterAfterMmIoWrite (
  IN FILTER_IO_WIDTH  Width,
  IN UINTN            Address,
  IN VOID             *Buffer
  );

/**
  Filter MSR before read operation.

  It will return the flag to decide whether require read real MSR.
  It can be used for emulation environment.

  @param  Index                     The 8-bit Machine Specific Register index to BeforeWrite.
  @param  Value                     The 64-bit value to BeforeRead from the Machine Specific Register.

  @retval TRUE         Need to excute the MSR read.
  @retval FALSE        Skip the MSR read.

**/
BOOLEAN
EFIAPI
FilterBeforeMsrRead (
  IN UINT32      Index,
  IN OUT UINT64  *Value
  );

/**
  Trace MSR after read operation

  @param  Index                     The 8-bit Machine Specific Register index to BeforeWrite.
  @param  Value                     The 64-bit value to BeforeRead from the Machine Specific Register.

**/
VOID
EFIAPI
FilterAfterMsrRead (
  IN UINT32  Index,
  IN UINT64  *Value
  );

/**
  Filter MSR before write operation

  It will return the flag to decide whether require write real MSR.
  It can be used for emulation environment.

  @param  Index                     The 8-bit Machine Specific Register index to BeforeWrite.
  @param  Value                     The 64-bit value to BeforeWrite to the Machine Specific Register.

  @retval TRUE         Need to excute the MSR write.
  @retval FALSE        Skip the MSR write.

**/
BOOLEAN
EFIAPI
FilterBeforeMsrWrite (
  IN UINT32  Index,
  IN UINT64  *Value
  );

/**
  Trace MSR after write operation

  @param  Index                     The 8-bit Machine Specific Register index to BeforeWrite.
  @param  Value                     The 64-bit value to BeforeWrite to the Machine Specific Register.

**/
VOID
EFIAPI
FilterAfterMsrWrite (
  IN UINT32  Index,
  IN UINT64  *Value
  );

#endif // REGISTER_FILTER_LIB_H_
