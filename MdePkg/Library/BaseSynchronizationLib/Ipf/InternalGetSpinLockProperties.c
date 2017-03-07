/** @file
  Internal function to get spin lock alignment.

  Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

/**
  Internal function to retrieve the architecture specific spin lock alignment
  requirements for optimal spin lock performance.

  @return The architecture specific spin lock alignment.
  
**/
UINTN
InternalGetSpinLockProperties (
  VOID
  )
{
  return 32;
}

