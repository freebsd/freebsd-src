/** @file 

    This module contains generic macros for an assembly writer.

Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef _ASM_H_
#define _ASM_H_

#define TRUE  1
#define FALSE 0
#define PROCEDURE_ENTRY(name)   .##text;            \
  .##type name, @function; \
  .##proc name; \
  name::

#define PROCEDURE_EXIT(name)  .##endp name

#endif // _ASM_H
