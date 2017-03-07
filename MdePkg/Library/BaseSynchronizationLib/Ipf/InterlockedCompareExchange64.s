/// @file
///   Contains an implementation of InterlockedCompareExchange64 on Itanium-
///   based architecture.
///
/// Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
/// This program and the accompanying materials
/// are licensed and made available under the terms and conditions of the BSD License
/// which accompanies this distribution.  The full text of the license may be found at
/// http://opensource.org/licenses/bsd-license.php.
///
/// THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
/// WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
///
/// Module Name:  InterlockedCompareExchange64.s
///
///

.auto
.text

.proc   InternalSyncCompareExchange64
.type   InternalSyncCompareExchange64, @function
InternalSyncCompareExchange64::
        mov                 ar.ccv = r33
        cmpxchg8.rel        r8  = [r32], r34
        mf
        br.ret.sptk.many    b0
.endp   InternalSyncCompareExchange64
