/// @file
///   Retrieve of the interrupt state of the running processor for the Itanium
///   architecture.
///
/// Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
/// This program and the accompanying materials
/// are licensed and made available under the terms and conditions of the BSD License
/// which accompanies this distribution.  The full text of the license may be found at
/// http://opensource.org/licenses/bsd-license.php.
///
/// THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
/// WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
///
/// Module Name:  GetInterruptState.s
///
///

.auto
.text

.proc   GetInterruptState
.type   GetInterruptState, @function
GetInterruptState::
        mov                 r8  = psr
        extr.u              r8  = r8, 14, 1
        br.ret.sptk.many    b0
.endp   GetInterruptState
