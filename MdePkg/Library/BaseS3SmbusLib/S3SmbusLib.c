/** @file
  Smbus Library Services that do SMBus transactions and also enable the operatation
  to be replayed during an S3 resume. This library class maps directly on top
  of the SmbusLib class. 

  Copyright (c) 2007, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions
  of the BSD License which accompanies this distribution.  The
  full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/


#include <Base.h>

#include <Library/DebugLib.h>
#include <Library/S3BootScriptLib.h>
#include <Library/SmbusLib.h>
#include <Library/S3SmbusLib.h>

/**
  Saves an SMBus operation to S3 script to be replayed on S3 resume. 

  This function provides a standard way to save SMBus operation to S3 boot Script.
  The data can either be of the Length byte, word, or a block of data.
  If it falis to save S3 boot script, then ASSERT ().

  @param  SmbusOperation  Signifies which particular SMBus hardware protocol instance that it will use to
                          execute the SMBus transactions.
  @param  SmBusAddress    Address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  Length          Signifies the number of bytes that this operation will do. The maximum number of
                          bytes can be revision specific and operation specific.
  @param  Buffer          Contains the value of data to execute to the SMBus slave device. Not all operations
                          require this argument. The length of this buffer is identified by Length.

**/
VOID
InternalSaveSmBusExecToBootScript (
  IN     EFI_SMBUS_OPERATION        SmbusOperation,
  IN     UINTN                      SmBusAddress,
  IN     UINTN                      Length,
  IN OUT VOID                       *Buffer
  )
{
  RETURN_STATUS                Status;

  Status = S3BootScriptSaveSmbusExecute (
             SmBusAddress,
             SmbusOperation,
            &Length,
             Buffer
             );
  ASSERT (Status == RETURN_SUCCESS);
}

/**
  Executes an SMBUS quick read command and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS quick read command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address field of SmBusAddress is required.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If PEC is set in SmBusAddress, then ASSERT().
  If Command in SmBusAddress is not zero, then ASSERT().
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress    Address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  Status          Return status for the executed command.
                          This is an optional parameter and may be NULL.

**/
VOID
EFIAPI
S3SmBusQuickRead (
  IN  UINTN                     SmBusAddress,
  OUT RETURN_STATUS             *Status       OPTIONAL
  )
{
  SmBusQuickRead (SmBusAddress, Status);
  
  InternalSaveSmBusExecToBootScript (EfiSmbusQuickRead, SmBusAddress, 0, NULL);
}

/**
  Executes an SMBUS quick write command and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS quick write command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address field of SmBusAddress is required.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If PEC is set in SmBusAddress, then ASSERT().
  If Command in SmBusAddress is not zero, then ASSERT().
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress    Address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  Status          Return status for the executed command.
                          This is an optional parameter and may be NULL.

**/
VOID
EFIAPI
S3SmBusQuickWrite (
  IN  UINTN                     SmBusAddress,
  OUT RETURN_STATUS             *Status       OPTIONAL
  )
{
  SmBusQuickWrite (SmBusAddress, Status);

  InternalSaveSmBusExecToBootScript (EfiSmbusQuickWrite, SmBusAddress, 0, NULL);
}
  
/**
  Executes an SMBUS receive byte command and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS receive byte command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address field of SmBusAddress is required.
  The byte received from the SMBUS is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Command in SmBusAddress is not zero, then ASSERT().
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress    Address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  Status          Return status for the executed command.
                          This is an optional parameter and may be NULL.

  @return The byte received from the SMBUS.

**/
UINT8
EFIAPI
S3SmBusReceiveByte (
  IN  UINTN          SmBusAddress,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINT8   Byte;

  Byte = SmBusReceiveByte (SmBusAddress, Status);

  InternalSaveSmBusExecToBootScript (EfiSmbusReceiveByte, SmBusAddress, 1, &Byte);

  return Byte;
}

/**
  Executes an SMBUS send byte command and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS send byte command on the SMBUS device specified by SmBusAddress.
  The byte specified by Value is sent.
  Only the SMBUS slave address field of SmBusAddress is required.  Value is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Command in SmBusAddress is not zero, then ASSERT().
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress    Address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  Value           The 8-bit value to send.
  @param  Status          Return status for the executed command.
                          This is an optional parameter and may be NULL.

  @return The parameter of Value.

**/
UINT8
EFIAPI
S3SmBusSendByte (
  IN  UINTN          SmBusAddress,
  IN  UINT8          Value,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINT8   Byte;

  Byte = SmBusSendByte (SmBusAddress, Value, Status);

  InternalSaveSmBusExecToBootScript (EfiSmbusSendByte, SmBusAddress, 1, &Byte);

  return Byte;
}

/**
  Executes an SMBUS read data byte command and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS read data byte command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  The 8-bit value read from the SMBUS is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress    Address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  Status          Return status for the executed command.
                          This is an optional parameter and may be NULL.

  @return The byte read from the SMBUS.

**/
UINT8
EFIAPI
S3SmBusReadDataByte (
  IN  UINTN          SmBusAddress,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINT8   Byte;

  Byte = SmBusReadDataByte (SmBusAddress, Status);

  InternalSaveSmBusExecToBootScript (EfiSmbusReadByte, SmBusAddress, 1, &Byte);

  return Byte;
}

/**
  Executes an SMBUS write data byte command and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS write data byte command on the SMBUS device specified by SmBusAddress.
  The 8-bit value specified by Value is written.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  Value is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress    Address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  Value           The 8-bit value to write.
  @param  Status          Return status for the executed command.
                          This is an optional parameter and may be NULL.

  @return The parameter of Value.

**/
UINT8
EFIAPI
S3SmBusWriteDataByte (
  IN  UINTN          SmBusAddress,
  IN  UINT8          Value,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINT8   Byte;

  Byte = SmBusWriteDataByte (SmBusAddress, Value, Status);

  InternalSaveSmBusExecToBootScript (EfiSmbusWriteByte, SmBusAddress, 1, &Byte);

  return Byte;
}

/**
  Executes an SMBUS read data word command and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS read data word command on the SMBUS device specified by SmBusAddress.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  The 16-bit value read from the SMBUS is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().
  
  @param  SmBusAddress    Address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  Status          Return status for the executed command.
                          This is an optional parameter and may be NULL.

  @return The byte read from the SMBUS.

**/
UINT16
EFIAPI
S3SmBusReadDataWord (
  IN  UINTN          SmBusAddress,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINT16  Word;
  
  Word = SmBusReadDataWord (SmBusAddress, Status);

  InternalSaveSmBusExecToBootScript (EfiSmbusReadWord, SmBusAddress, 2, &Word);

  return Word;
}

/**
  Executes an SMBUS write data word command and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS write data word command on the SMBUS device specified by SmBusAddress.
  The 16-bit value specified by Value is written.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  Value is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress    Address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  Value           The 16-bit value to write.
  @param  Status          Return status for the executed command.
                          This is an optional parameter and may be NULL.

  @return The parameter of Value.

**/
UINT16
EFIAPI
S3SmBusWriteDataWord (
  IN  UINTN          SmBusAddress,
  IN  UINT16         Value,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINT16  Word;

  Word = SmBusWriteDataWord (SmBusAddress, Value, Status);

  InternalSaveSmBusExecToBootScript (EfiSmbusWriteWord, SmBusAddress, 2, &Word);

  return Word;
}

/**
  Executes an SMBUS process call command and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS process call command on the SMBUS device specified by SmBusAddress.
  The 16-bit value specified by Value is written.
  Only the SMBUS slave address and SMBUS command fields of SmBusAddress are required.
  The 16-bit value returned by the process call command is returned.
  If Status is not NULL, then the status of the executed command is returned in Status.
  If Length in SmBusAddress is not zero, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress    Address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  Value           The 16-bit value to write.
  @param  Status          Return status for the executed command.
                          This is an optional parameter and may be NULL.

  @return The 16-bit value returned by the process call command.

**/
UINT16
EFIAPI
S3SmBusProcessCall (
  IN  UINTN          SmBusAddress,
  IN  UINT16         Value,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINT16  Word;

  Word = SmBusProcessCall (SmBusAddress, Value, Status);

  InternalSaveSmBusExecToBootScript (EfiSmbusProcessCall, SmBusAddress, 2, &Value);

  return Word; 
}

/**
  Executes an SMBUS read block command and saves the value in the S3 script to be replayed
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

  @param  SmBusAddress    Address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  Buffer          Pointer to the buffer to store the bytes read from the SMBUS.
  @param  Status          Return status for the executed command.
                          This is an optional parameter and may be NULL.

  @return The number of bytes read.

**/
UINTN
EFIAPI
S3SmBusReadBlock (
  IN  UINTN          SmBusAddress,
  OUT VOID           *Buffer,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINTN   Length;

  Length = SmBusReadBlock (SmBusAddress, Buffer, Status);

  InternalSaveSmBusExecToBootScript (EfiSmbusReadBlock, SmBusAddress, Length, Buffer);

  return Length;
}

/**
  Executes an SMBUS write block command and saves the value in the S3 script to be replayed
  on S3 resume.

  Executes an SMBUS write block command on the SMBUS device specified by SmBusAddress.
  The SMBUS slave address, SMBUS command, and SMBUS length fields of SmBusAddress are required.
  Bytes are written to the SMBUS from Buffer.
  The number of bytes written is returned, and will never return a value larger than 32-bytes.
  If Status is not NULL, then the status of the executed command is returned in Status.  
  If Length in SmBusAddress is zero or greater than 32, then ASSERT().
  If Buffer is NULL, then ASSERT().
  If any reserved bits of SmBusAddress are set, then ASSERT().

  @param  SmBusAddress    Address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  Buffer          Pointer to the buffer to store the bytes read from the SMBUS.
  @param  Status          Return status for the executed command.
                          This is an optional parameter and may be NULL.

  @return The number of bytes written.

**/
UINTN
EFIAPI
S3SmBusWriteBlock (
  IN  UINTN          SmBusAddress,
  OUT VOID           *Buffer,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINTN  Length;

  Length = SmBusWriteBlock (SmBusAddress, Buffer, Status);

  InternalSaveSmBusExecToBootScript (EfiSmbusWriteBlock, SmBusAddress, SMBUS_LIB_LENGTH (SmBusAddress), Buffer);
  
  return Length;
}

/**
  Executes an SMBUS block process call command and saves the value in the S3 script to be replayed
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

  @param  SmBusAddress    Address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  WriteBuffer     Pointer to the buffer of bytes to write to the SMBUS.
  @param  ReadBuffer      Pointer to the buffer of bytes to read from the SMBUS.
  @param  Status          Return status for the executed command.
                          This is an optional parameter and may be NULL.

  @return The number of bytes written.

**/
UINTN
EFIAPI
S3SmBusBlockProcessCall (
  IN  UINTN          SmBusAddress,
  IN  VOID           *WriteBuffer,
  OUT VOID           *ReadBuffer,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINTN   Length;
  
  Length = SmBusBlockProcessCall (SmBusAddress, WriteBuffer, ReadBuffer, Status);
  
  InternalSaveSmBusExecToBootScript (EfiSmbusBWBRProcessCall, SmBusAddress, SMBUS_LIB_LENGTH (SmBusAddress), ReadBuffer);

  return Length;
}
