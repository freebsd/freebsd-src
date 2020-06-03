/** @file
  Smbus Library Services that conduct SMBus transactions and enable the operatation
  to be replayed during an S3 resume. This library class maps directly on top
  of the SmbusLib class.

  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __S3_SMBUS_LIB_H__
#define __S3_SMBUS_LIB_H__

/**
  Executes an SMBUS quick read command, and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS quick read command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address field of SmBusAddress is required.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If PEC is set in SmBusAddress, then ASSERT().
  If Command in SmBusAddress is not zero, then ASSERT().
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param[in]  SmBusAddress   The address that encodes the SMBUS Slave Address,
                             SMBUS Command, SMBUS Data Length, and PEC.
  @param[out] Status         The return status for the executed command.
                             This is an optional parameter and may be NULL.
                             RETURN_SUCCESS       The SMBUS command was executed.
                             RETURN_TIMEOUT       A timeout occurred while executing the SMBUS command.
                             RETURN_DEVICE_ERROR The request was not completed because a failure
                              was recorded in the Host Status Register bit.  Device errors are a result
                              of a transaction collision, illegal command field, unclaimed cycle
                              (host initiated), or bus error (collision).
                             RETURN_UNSUPPORTED    The SMBus operation is not supported.

**/
VOID
EFIAPI
S3SmBusQuickRead (
  IN  UINTN          SmBusAddress,
  OUT RETURN_STATUS  *Status       OPTIONAL
  );

/**
  Executes an SMBUS quick write command, and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS quick write command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address field of SmBusAddress is required.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If PEC is set in SmBusAddress, then ASSERT().
  If Command in SmBusAddress is not zero, then ASSERT().
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param[in]  SmBusAddress   The address that encodes the SMBUS Slave Address,
                             SMBUS Command, SMBUS Data Length, and PEC.
  @param[out] Status         The return status for the executed command.
                             This is an optional parameter and may be NULL.
                             RETURN_SUCCESS      The SMBUS command was executed.
                             RETURN_TIMEOUT      A timeout occurred while executing the SMBUS command.
                             RETURN_DEVICE_ERROR The request was not completed because a failure
                             was recorded in the Host Status Register bit.  Device errors are a result
                             of a transaction collision, illegal command field, unclaimed cycle
                             (host initiated), or bus error (collision).
                             RETURN_UNSUPPORTED    The SMBus operation is not supported.

**/
VOID
EFIAPI
S3SmBusQuickWrite (
  IN  UINTN          SmBusAddress,
  OUT RETURN_STATUS  *Status       OPTIONAL
  );

/**
  Executes an SMBUS receive byte command, and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS receive byte command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address field of SmBusAddress is required.
  The byte received from the SMBUS is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Command in SmBusAddress is not zero, then ASSERT().
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param[in]  SmBusAddress   The address that encodes the SMBUS Slave Address,
                             SMBUS Command, SMBUS Data Length, and PEC.
  @param[out] Status         The return status for the executed command.
                             This is an optional parameter and may be NULL.
                             RETURN_SUCCESS      The SMBUS command was executed.
                             RETURN_TIMEOUT      A timeout occurred while executing the SMBUS command.
                             RETURN_DEVICE_ERROR The request was not completed because a failure
                             was recorded in the Host Status Register bit.  Device errors are a result
                             of a transaction collision, illegal command field, unclaimed cycle
                             (host initiated), or bus error (collision).
                             RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect).
                             RETURN_UNSUPPORTED    The SMBus operation is not supported.

  @return   The byte received from the SMBUS.

**/
UINT8
EFIAPI
S3SmBusReceiveByte (
  IN  UINTN          SmBusAddress,
  OUT RETURN_STATUS  *Status        OPTIONAL
  );

/**
  Executes an SMBUS send byte command, and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS send byte command on the SMBUS device specified by SmBusAddress.
  The byte specified by Value is sent.
  Only the SMBUS slave address field of SmBusAddress is required.  Value is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Command in SmBusAddress is not zero, then ASSERT().
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param[in]  SmBusAddress   The address that encodes the SMBUS Slave Address,
                             SMBUS Command, SMBUS Data Length, and PEC.
  @param[in]  Value          The 8-bit value to send.
  @param[out] Status         The return status for the executed command.
                             This is an optional parameter and may be NULL.
                             RETURN_SUCCESS The SMBUS command was executed.
                             RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                             RETURN_DEVICE_ERROR  The request was not completed because a failure
                             was recorded in the Host Status Register bit.  Device errors are a result
                             of a transaction collision, illegal command field, unclaimed cycle
                             (host initiated), or bus errors (collisions).
                             RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect).
                             RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return   The parameter of Value.

**/
UINT8
EFIAPI
S3SmBusSendByte (
  IN  UINTN          SmBusAddress,
  IN  UINT8          Value,
  OUT RETURN_STATUS  *Status        OPTIONAL
  );

/**
  Executes an SMBUS read data byte command, and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS read data byte command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  The 8-bit value read from the SMBUS is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param[in]  SmBusAddress   The address that encodes the SMBUS Slave Address,
                             SMBUS Command, SMBUS Data Length, and PEC.
  @param[out] Status         The return status for the executed command.
                             This is an optional parameter and may be NULL.
                             RETURN_SUCCESS The SMBUS command was executed.
                             RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                             RETURN_DEVICE_ERROR  The request was not completed because a failure
                             was recorded in the Host Status Register bit.  Device errors are a result
                             of a transaction collision, illegal command field, unclaimed cycle
                             (host initiated), or bus error (collision).
                             RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect).
                             RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return   The byte read from the SMBUS.

**/
UINT8
EFIAPI
S3SmBusReadDataByte (
  IN  UINTN          SmBusAddress,
  OUT RETURN_STATUS  *Status        OPTIONAL
  );

/**
  Executes an SMBUS write data byte command, and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS write data byte command on the SMBUS device specified by SmBusAddress.
  The 8-bit value specified by Value is written.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  Value is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param[in]  SmBusAddress   The address that encodes the SMBUS Slave Address,
                             SMBUS Command, SMBUS Data Length, and PEC.
  @param[in]  Value          The 8-bit value to write.
  @param[out] Status         The return status for the executed command.
                             This is an optional parameter and may be NULL.
                             RETURN_SUCCESS The SMBUS command was executed.
                             RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                             RETURN_DEVICE_ERROR  The request was not completed because a failure
                             was recorded in the Host Status Register bit.  Device errors are a result
                             of a transaction collision, illegal command field, unclaimed cycle
                             (host initiated), or bus error (collision).
                             RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect).
                             RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return   The parameter of Value.

**/
UINT8
EFIAPI
S3SmBusWriteDataByte (
  IN  UINTN          SmBusAddress,
  IN  UINT8          Value,
  OUT RETURN_STATUS  *Status        OPTIONAL
  );

/**
  Executes an SMBUS read data word command, and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS read data word command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  The 16-bit value read from the SMBUS is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param[in]  SmBusAddress   The address that encodes the SMBUS Slave Address,
                             SMBUS Command, SMBUS Data Length, and PEC.
  @param[out] Status         The return status for the executed command.
                             This is an optional parameter and may be NULL.
                             RETURN_SUCCESS The SMBUS command was executed.
                             RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                             RETURN_DEVICE_ERROR  The request was not completed because a failure
                             was recorded in the Host Status Register bit.  Device errors are a result
                             of a transaction collision, illegal command field, unclaimed cycle
                             (host initiated), or bus error (collision).
                             RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect).
                             RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return   The byte read from the SMBUS.

**/
UINT16
EFIAPI
S3SmBusReadDataWord (
  IN  UINTN          SmBusAddress,
  OUT RETURN_STATUS  *Status        OPTIONAL
  );

/**
  Executes an SMBUS write data word command, and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS write data word command on the SMBUS device specified by SmBusAddress.
  The 16-bit value specified by Value is written.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  Value is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param[in]  SmBusAddress   The address that encodes the SMBUS Slave Address,
                             SMBUS Command, SMBUS Data Length, and PEC.
  @param[in]  Value          The 16-bit value to write.
  @param[out] Status         The return status for the executed command.
                             This is an optional parameter and may be NULL.
                             RETURN_SUCCESS The SMBUS command was executed.
                             RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                             RETURN_DEVICE_ERROR  The request was not completed because a failure
                             was recorded in the Host Status Register bit.  Device errors are a result
                             of a transaction collision, illegal command field, unclaimed cycle
                             (host initiated), or bus error (collision).
                             RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect).
                             RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return   The parameter of Value.

**/
UINT16
EFIAPI
S3SmBusWriteDataWord (
  IN  UINTN          SmBusAddress,
  IN  UINT16         Value,
  OUT RETURN_STATUS  *Status        OPTIONAL
  );

/**
  Executes an SMBUS process call command, and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS process call command on the SMBUS device specified by SmBusAddress.
  The 16-bit value specified by Value is written.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  The 16-bit value returned by the process call command is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param[in]  SmBusAddress   The address that encodes the SMBUS Slave Address,
                             SMBUS Command, SMBUS Data Length, and PEC.
  @param[in]  Value          The 16-bit value to write.
  @param[out] Status         The return status for the executed command.
                             This is an optional parameter and may be NULL.
                             RETURN_SUCCESS The SMBUS command was executed.
                             RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                             RETURN_DEVICE_ERROR  The request was not completed because a failure
                             was recorded in the Host Status Register bit.  Device errors are a result
                             of a transaction collision, illegal command field, unclaimed cycle
                             (host initiated), or bus error (collision).
                             RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect).
                             RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return   The 16-bit value returned by the process call command.

**/
UINT16
EFIAPI
S3SmBusProcessCall (
  IN  UINTN          SmBusAddress,
  IN  UINT16         Value,
  OUT RETURN_STATUS  *Status        OPTIONAL
  );

/**
  Executes an SMBUS read block command, and saves the value in the S3 script to be replayed
  on S3 resume.

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

  @param[in]  SmBusAddress   The address that encodes the SMBUS Slave Address,
                             SMBUS Command, SMBUS Data Length, and PEC.
  @param[out] Buffer         The pointer to the buffer to store the bytes read from the SMBUS.
  @param[out] Status         The return status for the executed command.
                             This is an optional parameter and may be NULL.
                             RETURN_SUCCESS The SMBUS command was executed.
                             RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                             RETURN_DEVICE_ERROR  The request was not completed because a failure
                             was recorded in the Host Status Register bit.  Device errors are a result
                             of a transaction collision, illegal command field, unclaimed cycle
                             (host initiated), or bus error (collision).
                             RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect).
                             RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return   The number of bytes read.

**/
UINTN
EFIAPI
S3SmBusReadBlock (
  IN  UINTN          SmBusAddress,
  OUT VOID           *Buffer,
  OUT RETURN_STATUS  *Status        OPTIONAL
  );

/**
  Executes an SMBUS write block command, and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS write block command on the SMBUS device specified by SmBusAddress.
  The SMBUS slave address, SMBUS command, and SMBUS length fields of SmBusAddress are required.
  Bytes are written to the SMBUS from Buffer.
  The number of bytes written is returned, and will never return a value larger than 32-bytes.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is zero or greater than 32, then ASSERT().
  If Buffer is NULL, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param[in]  SmBusAddress   The address that encodes the SMBUS Slave Address,
                             SMBUS Command, SMBUS Data Length, and PEC.
  @param[out] Buffer         The pointer to the buffer to store the bytes read from the SMBUS.
  @param[out] Status         The return status for the executed command.
                             This is an optional parameter and may be NULL.
                             RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                             RETURN_DEVICE_ERROR  The request was not completed because a failure
                             was recorded in the Host Status Register bit.  Device errors are a result
                             of a transaction collision, illegal command field, unclaimed cycle
                             (host initiated), or bus error (collision).
                             RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect).
                             RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return   The number of bytes written.

**/
UINTN
EFIAPI
S3SmBusWriteBlock (
  IN  UINTN          SmBusAddress,
  OUT VOID           *Buffer,
  OUT RETURN_STATUS  *Status        OPTIONAL
  );

/**
  Executes an SMBUS block process call command, and saves the value in the S3 script to be replayed
  on S3 resume.

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

  @param[in]  SmBusAddress   The address that encodes the SMBUS Slave Address,
                             SMBUS Command, SMBUS Data Length, and PEC.
  @param[in]  WriteBuffer    The pointer to the buffer of bytes to write to the SMBUS.
  @param[out] ReadBuffer     The pointer to the buffer of bytes to read from the SMBUS.
  @param[out] Status         The return status for the executed command.
                             This is an optional parameter and may be NULL.
                             RETURN_TIMEOUT A timeout occurred while executing the SMBUS command.
                             RETURN_DEVICE_ERROR  The request was not completed because a failure
                             was recorded in the Host Status Register bit.  Device errors are a result
                             of a transaction collision, illegal command field, unclaimed cycle
                             (host initiated), or bus error (collision).
                             RETURN_CRC_ERROR  The checksum is not correct (PEC is incorrect).
                             RETURN_UNSUPPORTED  The SMBus operation is not supported.

  @return   The number of bytes written.

**/
UINTN
EFIAPI
S3SmBusBlockProcessCall (
  IN  UINTN          SmBusAddress,
  IN  VOID           *WriteBuffer,
  OUT VOID           *ReadBuffer,
  OUT RETURN_STATUS  *Status        OPTIONAL
  );

#endif
