/** @file
  Math worker functions.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/




#include "BaseLibInternals.h"

/**
  Switches the endianess of a 16-bit integer.

  This function swaps the bytes in a 16-bit unsigned value to switch the value
  from little endian to big endian or vice versa. The byte swapped value is
  returned.

  @param  Value A 16-bit unsigned value.

  @return The byte swapped Value.

**/
UINT16
EFIAPI
SwapBytes16 (
  IN      UINT16                    Value
  )
{
  return (UINT16) ((Value<< 8) | (Value>> 8));
}
