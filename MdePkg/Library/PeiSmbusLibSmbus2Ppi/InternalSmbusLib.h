/** @file
Internal header file for Smbus library.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent


**/

#ifndef __INTERNAL_SMBUS_LIB_H_
#define __INTERNAL_SMBUS_LIB_H_


#include <PiPei.h>

#include <Ppi/Smbus2.h>

#include <Library/SmbusLib.h>
#include <Library/DebugLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/BaseMemoryLib.h>

//
// Declaration for internal functions
//

/**
  Gets Smbus PPIs.

  This internal function retrieves Smbus PPI from PPI database.

  @param  VOID

  @return The pointer to Smbus PPI.

**/
EFI_PEI_SMBUS2_PPI *
InternalGetSmbusPpi (
  VOID
  );

/**
  Executes an SMBus operation to an SMBus controller.

  This function provides a standard way to execute Smbus script
  as defined in the SmBus Specification. The data can either be of
  the Length byte, word, or a block of data.

  @param  SmbusOperation  Signifies which particular SMBus hardware protocol
                          instance that it will use to execute the SMBus transactions.
  @param  SmBusAddress    The address that encodes the SMBUS Slave Address,
                          SMBUS Command, SMBUS Data Length, and PEC.
  @param  Length          Signifies the number of bytes that this operation will
                          do. The maximum number of bytes can be revision specific
                          and operation specific.
  @param  Buffer          Contains the value of data to execute to the SMBus slave
                          device. Not all operations require this argument. The
                          length of this buffer is identified by Length.
  @param  Status          Return status for the executed command.
                          This is an optional parameter and may be NULL.

  @return The actual number of bytes that are executed for this operation.

**/
UINTN
InternalSmBusExec (
  IN     EFI_SMBUS_OPERATION        SmbusOperation,
  IN     UINTN                      SmBusAddress,
  IN     UINTN                      Length,
  IN OUT VOID                       *Buffer,
     OUT RETURN_STATUS              *Status        OPTIONAL
  );

#endif
