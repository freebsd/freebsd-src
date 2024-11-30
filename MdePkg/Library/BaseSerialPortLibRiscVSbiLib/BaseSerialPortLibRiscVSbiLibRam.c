/** @file
  Serial Port Library backed by SBI console.

  Meant for PrePi and DXE environments (where globals are allowed). See
  BaseSerialPortLibRiscVSbiLib.c for a reduced variant appropriate for
  SEC and PEI (XIP) environments.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/SerialPortLib.h>
#include <Library/BaseRiscVSbiLib.h>
#include "Common.h"

STATIC BOOLEAN  mHaveDbcn          = FALSE;
STATIC BOOLEAN  mHaveLegacyPutchar = FALSE;
STATIC BOOLEAN  mHaveLegacyGetchar = FALSE;
STATIC INT64    mLastGetChar       = -1;

/**
  Return whether the legacy console getchar extension is implemented.

  @retval TRUE                  Extension is implemented.
  @retval FALSE                 Extension is not implemented.

**/
STATIC
BOOLEAN
SbiImplementsLegacyGetchar (
  VOID
  )
{
  SBI_RET  Ret;

  Ret = SbiCall (SBI_EXT_BASE, SBI_EXT_BASE_PROBE_EXT, 1, SBI_EXT_0_1_CONSOLE_GETCHAR);
  if ((TranslateError (Ret.Error) == EFI_SUCCESS) &&
      (Ret.Value != 0))
  {
    return TRUE;
  }

  return FALSE;
}

/**
  Initialize the serial device hardware.

  If no initialization is required, then return RETURN_SUCCESS.
  If the serial device was successfully initialized, then return RETURN_SUCCESS.
  If the serial device could not be initialized, then return RETURN_DEVICE_ERROR.

  @retval RETURN_SUCCESS        The serial device was initialized.
  @retval RETURN_DEVICE_ERROR   The serial device could not be initialized.

**/
RETURN_STATUS
EFIAPI
SerialPortInitialize (
  VOID
  )
{
  if (SbiImplementsDbcn ()) {
    mHaveDbcn = TRUE;
    return RETURN_SUCCESS;
  }

  if (SbiImplementsLegacyPutchar ()) {
    mHaveLegacyPutchar = TRUE;
  }

  if (SbiImplementsLegacyGetchar ()) {
    mHaveLegacyGetchar = TRUE;
  }

  return (mHaveLegacyGetchar && mHaveLegacyPutchar) ?
         RETURN_SUCCESS :
         RETURN_DEVICE_ERROR;
}

/**
  Write data from buffer to serial device.

  Writes NumberOfBytes data bytes from Buffer to the serial device.
  The number of bytes actually written to the serial device is returned.
  If the return value is less than NumberOfBytes, then the write operation failed.
  If NumberOfBytes is zero, then return 0.

  @param  Buffer           The pointer to the data buffer to be written.
  @param  NumberOfBytes    The number of bytes to written to the serial device.

  @retval 0                NumberOfBytes is 0.
  @retval >0               The number of bytes written to the serial device.
                           If this value is less than NumberOfBytes, then the write operation failed.

**/
UINTN
EFIAPI
SerialPortWrite (
  IN UINT8  *Buffer,
  IN UINTN  NumberOfBytes
  )
{
  if (NumberOfBytes == 0) {
    return 0;
  }

  if (mHaveDbcn) {
    return SbiDbcnWrite (Buffer, NumberOfBytes);
  } else if (mHaveLegacyPutchar) {
    return SbiLegacyPutchar (Buffer, NumberOfBytes);
  }

  /*
   * Neither DBCN or legacy extension were present.
   */
  return 0;
}

/**
  Read data from serial device and save the datas in buffer.

  Reads NumberOfBytes data bytes from a serial device into the buffer
  specified by Buffer. The number of bytes actually read is returned.
  If the return value is less than NumberOfBytes, then the rest operation failed.
  If NumberOfBytes is zero, then return 0.

  @param  Buffer           The pointer to the data buffer to store the data read from the serial device.
  @param  NumberOfBytes    The number of bytes which will be read.

  @retval 0                Read data failed; No data is to be read.
  @retval >0               The actual number of bytes read from serial device.

**/
UINTN
EFIAPI
SerialPortRead (
  OUT UINT8  *Buffer,
  IN  UINTN  NumberOfBytes
  )
{
  UINTN  Index;

  Index = 0;
  while ((Index < NumberOfBytes) && SerialPortPoll ()) {
    Buffer[Index++] = (UINT8)mLastGetChar;
    mLastGetChar    = -1;
  }

  return Index;
}

/**
  Polls a serial device to see if there is any data waiting to be read.

  Polls a serial device to see if there is any data waiting to be read.
  If there is data waiting to be read from the serial device, then TRUE is returned.
  If there is no data waiting to be read from the serial device, then FALSE is returned.

  @retval TRUE             Data is waiting to be read from the serial device.
  @retval FALSE            There is no data waiting to be read from the serial device.

**/
BOOLEAN
EFIAPI
SerialPortPoll (
  VOID
  )
{
  /*
   * Careful. OpenSBI with HTIF console will return -1 followed by -2
   * if there is no character received. So just check for values >= 0.
   */

  if (mLastGetChar >= 0) {
    return TRUE;
  }

  if (mHaveDbcn) {
    UINT8    Buffer;
    SBI_RET  Ret;

    Ret = SbiCall (
            SBI_EXT_DBCN,
            SBI_EXT_DBCN_READ,
            3,
            1,
            ((UINTN)&Buffer),
            0
            );
    if ((TranslateError (Ret.Error) == EFI_SUCCESS) &&
        (Ret.Value == 1))
    {
      mLastGetChar = Buffer;
    }
  } else if (mHaveLegacyGetchar) {
    mLastGetChar = (INT64)SbiCall (SBI_EXT_0_1_CONSOLE_GETCHAR, 0, 0).Error;
  }

  return mLastGetChar >= 0;
}

/**
  Sets the control bits on a serial device.

  @param Control                Sets the bits of Control that are settable.

  @retval RETURN_SUCCESS        The new control bits were set on the serial device.
  @retval RETURN_UNSUPPORTED    The serial device does not support this operation.
  @retval RETURN_DEVICE_ERROR   The serial device is not functioning correctly.

**/
RETURN_STATUS
EFIAPI
SerialPortSetControl (
  IN UINT32  Control
  )
{
  return RETURN_SUCCESS;
}

/**
  Retrieve the status of the control bits on a serial device.

  @param Control                A pointer to return the current control signals from the serial device.

  @retval RETURN_SUCCESS        The control bits were read from the serial device.
  @retval RETURN_UNSUPPORTED    The serial device does not support this operation.
  @retval RETURN_DEVICE_ERROR   The serial device is not functioning correctly.

**/
RETURN_STATUS
EFIAPI
SerialPortGetControl (
  OUT UINT32  *Control
  )
{
  *Control = 0;
  return RETURN_SUCCESS;
}

/**
  Sets the baud rate, receive FIFO depth, transmit/receice time out, parity,
  data bits, and stop bits on a serial device.

  @param BaudRate           The requested baud rate. A BaudRate value of 0 will use the
                            device's default interface speed.
                            On output, the value actually set.
  @param ReveiveFifoDepth   The requested depth of the FIFO on the receive side of the
                            serial interface. A ReceiveFifoDepth value of 0 will use
                            the device's default FIFO depth.
                            On output, the value actually set.
  @param Timeout            The requested time out for a single character in microseconds.
                            This timeout applies to both the transmit and receive side of the
                            interface. A Timeout value of 0 will use the device's default time
                            out value.
                            On output, the value actually set.
  @param Parity             The type of parity to use on this serial device. A Parity value of
                            DefaultParity will use the device's default parity value.
                            On output, the value actually set.
  @param DataBits           The number of data bits to use on the serial device. A DataBits
                            vaule of 0 will use the device's default data bit setting.
                            On output, the value actually set.
  @param StopBits           The number of stop bits to use on this serial device. A StopBits
                            value of DefaultStopBits will use the device's default number of
                            stop bits.
                            On output, the value actually set.

  @retval RETURN_SUCCESS            The new attributes were set on the serial device.
  @retval RETURN_UNSUPPORTED        The serial device does not support this operation.
  @retval RETURN_INVALID_PARAMETER  One or more of the attributes has an unsupported value.
  @retval RETURN_DEVICE_ERROR       The serial device is not functioning correctly.

**/
RETURN_STATUS
EFIAPI
SerialPortSetAttributes (
  IN OUT UINT64              *BaudRate,
  IN OUT UINT32              *ReceiveFifoDepth,
  IN OUT UINT32              *Timeout,
  IN OUT EFI_PARITY_TYPE     *Parity,
  IN OUT UINT8               *DataBits,
  IN OUT EFI_STOP_BITS_TYPE  *StopBits
  )
{
  return RETURN_SUCCESS;
}
