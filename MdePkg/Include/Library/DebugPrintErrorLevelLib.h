/** @file
  Debug Print Error Level Library class

  Copyright (c) 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#ifndef _DEBUG_PRINT_ERROR_LEVEL_LIB_H_
#define _DEBUG_PRINT_ERROR_LEVEL_LIB_H_

/**
  Returns the debug print error level mask for the current module.

  @return  Debug print error level mask for the current module.

**/
UINT32
EFIAPI
GetDebugPrintErrorLevel (
  VOID
  );

/**
  Sets the global debug print error level mask fpr the entire platform.
  
  @param   ErrorLevel     Global debug print error level
  
  @retval  TRUE           The debug print error level mask was successfully set.
  @retval  FALSE          The debug print error level mask could not be set.

**/
BOOLEAN
EFIAPI
SetDebugPrintErrorLevel (
  UINT32  ErrorLevel
  );
#endif
