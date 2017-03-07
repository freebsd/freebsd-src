// @file
// Contains the macros required by calling procedures in Itanium-based assembly code.
//
// Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
// This program and the accompanying materials
// are licensed and made available under the terms and conditions of the BSD License
// which accompanies this distribution.  The full text of the license may be found at
// http://opensource.org/licenses/bsd-license.php
//
// THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
// WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
//

#ifndef  __IA64PROC_I__
#define  __IA64PROC_I__

//
// Delcare the begin of assembly function entry.
//
// @param name Name of function in assembly code.
//
#define PROCEDURE_ENTRY(name)   .##text;            \
                                .##type name, @function;    \
                                .##proc name;           \
name::

//
// End of assembly function.
//
// @param name Name of function in assembly code.
//
#define PROCEDURE_EXIT(name)    .##endp name

//
// NESTED_SETUP Requires number of locals (l) >= 3
//
#define NESTED_SETUP(i,l,o,r) \
         alloc loc1=ar##.##pfs,i,l,o,r ;\
         mov loc0=b0

//
// End of Nested
//
#define NESTED_RETURN \
         mov b0=loc0 ;\
         mov ar##.##pfs=loc1 ;;\
         br##.##ret##.##dpnt  b0;;

//
// Export assembly function as the global function.
//
// @param Function Name of function in assembly code.
//
#define GLOBAL_FUNCTION(Function) \
         .##type   Function, @function; \
         .##globl Function

#endif
