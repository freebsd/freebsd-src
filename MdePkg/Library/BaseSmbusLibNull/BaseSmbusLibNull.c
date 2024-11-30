/** @file
Null implementation of SmBusLib class library.

Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/SmbusLib.h>
#include <Library/DebugLib.h>

/**
  Executes an SMBUS quick read command.

  Executes an SMBUS quick read command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address field of SmBusAddress is required.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If PEC is set in SmBusAddress, then ASSERT().
  If Command in SmBusAddress is not zero, then ASSERT().
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress  Address that encodes the SMBUS Slave Address,
                        SMBUS Command, SMBUS Data Length, and PEC.
  @param  Status        Return status for the executed command.
                        This is an optional parameter and may be NULL.
                        RETURN_SUCCESS  The SMBUS command was executed.
                        RETURN_TIMEOUT  A timeout occurred while executing the SMBUS command.
                        RETURN_DEVICE_ERROR The request was not completed because a failure
                        reflected in the Host Status Register bit.  Device errors are a result
                        of a transaction collision, illegal command field, unclaimed cycle
                        (host initiated), or bus errors (collisions).
                        RETURN_UNSUPPORTED  The SMBus operation is not supported.

**/
VOID
EFIAPI
SmBusQuickRead (
  IN  UINTN          SmBusAddress,
  OUT RETURN_STATUS  *Status       OPTIONAL
  )
{
  ASSERT (!SMBUS_LIB_PEC (SmBusAddress));
  ASSERT (SMBUS_LIB_COMMAND (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_RESERVED (SmBusAddress) == 0);
  if (Status != NULL) {
    *Status = RETURN_UNSUPPORTED;
  }
}

/**
  Executes an SMBUS quick write command.

  Executes an SMBUS quick write command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address field of SmBusAddress is required.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If PEC is set in SmBusAddress, then ASSERT().
  If Command in SmBusAddress is not zero, then ASSERT().
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress  Address that encodes the SMBUS Slave Address,
                        SMBUS Command, SMBUS Data Length, and PEC.
  @param  Status        Return status for the executed command.
                        This is an optional parameter and may be NULL.
                        RETURN_SUCCESS The SMBUS command was executed.
                        RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                        RETURN_DEVICE_ERROR  The request was not completed because a failure
                        reflected in the Host Status Register bit.  Device errors are a result
                        of a transaction collision, illegal command field, unclaimed cycle
                        (host initiated), or bus errors (collisions).
                        RETURN_UNSUPPORTED  The SMBus operation is not supported.

**/
VOID
EFIAPI
SmBusQuickWrite (
  IN  UINTN          SmBusAddress,
  OUT RETURN_STATUS  *Status       OPTIONAL
  )
{
  ASSERT (!SMBUS_LIB_PEC (SmBusAddress));
  ASSERT (SMBUS_LIB_COMMAND (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_RESERVED (SmBusAddress) == 0);
  if (Status != NULL) {
    *Status = RETURN_UNSUPPORTED;
  }
}

/**
  Executes an SMBUS receive byte command.

  Executes an SMBUS receive byte command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address field of SmBusAddress is required.
  The byte received from the SMBUS is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Command in SmBusAddress is not zero, then ASSERT().
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress  Address that encodes the SMBUS Slave Address,
                        SMBUS Command, SMBUS Data Length, and PEC.
  @param  Status        Return status for the executed command.
                        This is an optional parameter and may be NULL.
                        RETURN_SUCCESS The SMBUS command was executed.
                        RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                        RETURN_DEVICE_ERROR  The request was not completed because a failure
                        reflected in the Host Status Register bit.  Device errors are a result
                        of a transaction collision, illegal command field, unclaimed cycle
                        (host initiated), or bus errors (collisions).
                        RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect)
                        RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return The byte received from the SMBUS.

**/
UINT8
EFIAPI
SmBusReceiveByte (
  IN  UINTN          SmBusAddress,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  ASSERT (SMBUS_LIB_COMMAND (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_RESERVED (SmBusAddress) == 0);
  if (Status != NULL) {
    *Status = RETURN_UNSUPPORTED;
  }

  return 0;
}

/**
  Executes an SMBUS send byte command.

  Executes an SMBUS send byte command on the SMBUS device specified by SmBusAddress.
  The byte specified by Value is sent.
  Only the SMBUS slave address field of SmBusAddress is required.  Value is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Command in SmBusAddress is not zero, then ASSERT().
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress  Address that encodes the SMBUS Slave Address,
                        SMBUS Command, SMBUS Data Length, and PEC.
  @param  Value         The 8-bit value to send.
  @param  Status        Return status for the executed command.
                        This is an optional parameter and may be NULL.
                        RETURN_SUCCESS The SMBUS command was executed.
                        RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                        RETURN_DEVICE_ERROR  The request was not completed because a failure
                        reflected in the Host Status Register bit.  Device errors are a result
                        of a transaction collision, illegal command field, unclaimed cycle
                        (host initiated), or bus errors (collisions).
                        RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect)
                        RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return The parameter of Value.

**/
UINT8
EFIAPI
SmBusSendByte (
  IN  UINTN          SmBusAddress,
  IN  UINT8          Value,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  ASSERT (SMBUS_LIB_COMMAND (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_RESERVED (SmBusAddress) == 0);
  if (Status != NULL) {
    *Status = RETURN_UNSUPPORTED;
  }

  return 0;
}

/**
  Executes an SMBUS read data byte command.

  Executes an SMBUS read data byte command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  The 8-bit value read from the SMBUS is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress  Address that encodes the SMBUS Slave Address,
                        SMBUS Command, SMBUS Data Length, and PEC.
  @param  Status        Return status for the executed command.
                        This is an optional parameter and may be NULL.
                        RETURN_SUCCESS The SMBUS command was executed.
                        RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                        RETURN_DEVICE_ERROR  The request was not completed because a failure
                        reflected in the Host Status Register bit.  Device errors are a result
                        of a transaction collision, illegal command field, unclaimed cycle
                        (host initiated), or bus errors (collisions).
                        RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect)
                        RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return The byte read from the SMBUS.

**/
UINT8
EFIAPI
SmBusReadDataByte (
  IN  UINTN          SmBusAddress,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_RESERVED (SmBusAddress) == 0);
  if (Status != NULL) {
    *Status = RETURN_UNSUPPORTED;
  }

  return 0;
}

/**
  Executes an SMBUS write data byte command.

  Executes an SMBUS write data byte command on the SMBUS device specified by SmBusAddress.
  The 8-bit value specified by Value is written.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  Value is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress  Address that encodes the SMBUS Slave Address,
                        SMBUS Command, SMBUS Data Length, and PEC.
  @param  Value         The 8-bit value to write.
  @param  Status        Return status for the executed command.
                        This is an optional parameter and may be NULL.
                        RETURN_SUCCESS The SMBUS command was executed.
                        RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                        RETURN_DEVICE_ERROR  The request was not completed because a failure
                        reflected in the Host Status Register bit.  Device errors are a result
                        of a transaction collision, illegal command field, unclaimed cycle
                        (host initiated), or bus errors (collisions).
                        RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect)
                        RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return The parameter of Value.

**/
UINT8
EFIAPI
SmBusWriteDataByte (
  IN  UINTN          SmBusAddress,
  IN  UINT8          Value,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_RESERVED (SmBusAddress) == 0);
  if (Status != NULL) {
    *Status = RETURN_UNSUPPORTED;
  }

  return 0;
}

/**
  Executes an SMBUS read data word command.

  Executes an SMBUS read data word command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  The 16-bit value read from the SMBUS is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress  Address that encodes the SMBUS Slave Address,
                        SMBUS Command, SMBUS Data Length, and PEC.
  @param  Status        Return status for the executed command.
                        This is an optional parameter and may be NULL.
                        RETURN_SUCCESS The SMBUS command was executed.
                        RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                        RETURN_DEVICE_ERROR  The request was not completed because a failure
                        reflected in the Host Status Register bit.  Device errors are a result
                        of a transaction collision, illegal command field, unclaimed cycle
                        (host initiated), or bus errors (collisions).
                        RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect)
                        RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return The byte read from the SMBUS.

**/
UINT16
EFIAPI
SmBusReadDataWord (
  IN  UINTN          SmBusAddress,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_RESERVED (SmBusAddress) == 0);
  if (Status != NULL) {
    *Status = RETURN_UNSUPPORTED;
  }

  return 0;
}

/**
  Executes an SMBUS write data word command.

  Executes an SMBUS write data word command on the SMBUS device specified by SmBusAddress.
  The 16-bit value specified by Value is written.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  Value is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress  Address that encodes the SMBUS Slave Address,
                        SMBUS Command, SMBUS Data Length, and PEC.
  @param  Value         The 16-bit value to write.
  @param  Status        Return status for the executed command.
                        This is an optional parameter and may be NULL.
                        RETURN_SUCCESS The SMBUS command was executed.
                        RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                        RETURN_DEVICE_ERROR  The request was not completed because a failure
                        reflected in the Host Status Register bit.  Device errors are a result
                        of a transaction collision, illegal command field, unclaimed cycle
                        (host initiated), or bus errors (collisions).
                        RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect)
                        RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return The parameter of Value.

**/
UINT16
EFIAPI
SmBusWriteDataWord (
  IN  UINTN          SmBusAddress,
  IN  UINT16         Value,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_RESERVED (SmBusAddress) == 0);
  if (Status != NULL) {
    *Status = RETURN_UNSUPPORTED;
  }

  return 0;
}

/**
  Executes an SMBUS process call command.

  Executes an SMBUS process call command on the SMBUS device specified by SmBusAddress.
  The 16-bit value specified by Value is written.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  The 16-bit value returned by the process call command is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress  Address that encodes the SMBUS Slave Address,
                        SMBUS Command, SMBUS Data Length, and PEC.
  @param  Value         The 16-bit value to write.
  @param  Status        Return status for the executed command.
                        This is an optional parameter and may be NULL.
                        RETURN_SUCCESS The SMBUS command was executed.
                        RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                        RETURN_DEVICE_ERROR  The request was not completed because a failure
                        reflected in the Host Status Register bit.  Device errors are a result
                        of a transaction collision, illegal command field, unclaimed cycle
                        (host initiated), or bus errors (collisions).
                        RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect)
                        RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return The 16-bit value returned by the process call command.

**/
UINT16
EFIAPI
SmBusProcessCall (
  IN  UINTN          SmBusAddress,
  IN  UINT16         Value,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_RESERVED (SmBusAddress) == 0);
  if (Status != NULL) {
    *Status = RETURN_UNSUPPORTED;
  }

  return 0;
}

/**
  Executes an SMBUS read block command.

  Executes an SMBUS read block command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  Bytes are read from the SMBUS and stored in Buffer.
  The number of bytes read is returned, and will never return a value larger than 32-bytes.
  If Status is not NULL, then the status of the executed command is returned in Status.
  It is the caller's responsibility to make sure Buffer is large enough for the total number of bytes read.
  SMBUS supports a maximum transfer size of 32 bytes, so Buffer does not need to be any larger than 32 bytes.
  If Length in SmBusAddress is not zero, then ASSERT().
  If Buffer is NULL, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress  Address that encodes the SMBUS Slave Address,
                        SMBUS Command, SMBUS Data Length, and PEC.
  @param  Buffer        Pointer to the buffer to store the bytes read from the SMBUS.
  @param  Status        Return status for the executed command.
                        This is an optional parameter and may be NULL.
                        RETURN_SUCCESS The SMBUS command was executed.
                        RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                        RETURN_DEVICE_ERROR  The request was not completed because a failure
                        reflected in the Host Status Register bit.  Device errors are a result
                        of a transaction collision, illegal command field, unclaimed cycle
                        (host initiated), or bus errors (collisions).
                        RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect)
                        RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return The number of bytes read.

**/
UINTN
EFIAPI
SmBusReadBlock (
  IN  UINTN          SmBusAddress,
  OUT VOID           *Buffer,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  ASSERT (Buffer != NULL);
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) == 0);
  ASSERT (SMBUS_LIB_RESERVED (SmBusAddress) == 0);
  if (Status != NULL) {
    *Status = RETURN_UNSUPPORTED;
  }

  return 0;
}

/**
  Executes an SMBUS write block command.

  Executes an SMBUS write block command on the SMBUS device specified by SmBusAddress.
  The SMBUS slave address, SMBUS command, and SMBUS length fields of SmBusAddress are required.
  Bytes are written to the SMBUS from Buffer.
  The number of bytes written is returned, and will never return a value larger than 32-bytes.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is zero or greater than 32, then ASSERT().
  If Buffer is NULL, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress  Address that encodes the SMBUS Slave Address,
                        SMBUS Command, SMBUS Data Length, and PEC.
  @param  Buffer        Pointer to the buffer to store the bytes read from the SMBUS.
  @param  Status        Return status for the executed command.
                        This is an optional parameter and may be NULL.
                        RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                        RETURN_DEVICE_ERROR  The request was not completed because a failure
                        reflected in the Host Status Register bit.  Device errors are a result
                        of a transaction collision, illegal command field, unclaimed cycle
                        (host initiated), or bus errors (collisions).
                        RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect)
                        RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return The number of bytes written.

**/
UINTN
EFIAPI
SmBusWriteBlock (
  IN  UINTN          SmBusAddress,
  OUT VOID           *Buffer,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  ASSERT (Buffer != NULL);
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) >= 1);
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) <= 32);
  ASSERT (SMBUS_LIB_RESERVED (SmBusAddress) == 0);
  if (Status != NULL) {
    *Status = RETURN_UNSUPPORTED;
  }

  return 0;
}

/**
  Executes an SMBUS block process call command.

  Executes an SMBUS block process call command on the SMBUS device specified by SmBusAddress.
  The SMBUS slave address, SMBUS command, and SMBUS length fields of SmBusAddress are required.
  Bytes are written to the SMBUS from WriteBuffer.  Bytes are then read from the SMBUS into ReadBuffer.
  If Status is not NULL, then the status of the executed command is returned in Status.
  It is the caller's responsibility to make sure ReadBuffer is large enough for the total number of bytes read.
  SMBUS supports a maximum transfer size of 32 bytes, so Buffer does not need to be any larger than 32 bytes.
  If Length in SmBusAddress is zero or greater than 32, then ASSERT().
  If WriteBuffer is NULL, then ASSERT().
  If ReadBuffer is NULL, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress  Address that encodes the SMBUS Slave Address,
                        SMBUS Command, SMBUS Data Length, and PEC.
  @param  WriteBuffer   Pointer to the buffer of bytes to write to the SMBUS.
  @param  ReadBuffer    Pointer to the buffer of bytes to read from the SMBUS.
  @param  Status        Return status for the executed command.
                        This is an optional parameter and may be NULL.
                        RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                        RETURN_DEVICE_ERROR  The request was not completed because a failure
                        reflected in the Host Status Register bit.  Device errors are a result
                        of a transaction collision, illegal command field, unclaimed cycle
                        (host initiated), or bus errors (collisions).
                        RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect)
                        RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return The number of bytes written.

**/
UINTN
EFIAPI
SmBusBlockProcessCall (
  IN  UINTN          SmBusAddress,
  IN  VOID           *WriteBuffer,
  OUT VOID           *ReadBuffer,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  ASSERT (WriteBuffer != NULL);
  ASSERT (ReadBuffer  != NULL);
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) >= 1);
  ASSERT (SMBUS_LIB_LENGTH (SmBusAddress) <= 32);
  ASSERT (SMBUS_LIB_RESERVED (SmBusAddress) == 0);
  if (Status != NULL) {
    *Status = RETURN_UNSUPPORTED;
  }

  return 0;
}
