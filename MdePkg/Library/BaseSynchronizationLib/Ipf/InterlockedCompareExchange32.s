/// @file
///   Contains an implementation of InterlockedCompareExchange32 on Itanium-
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
/// Module Name:  InterlockedCompareExchange32.s
///
///

.auto
.text

.proc   InternalSyncCompareExchange32
.type   InternalSyncCompareExchange32, @function
InternalSyncCompareExchange32::
        zxt4                r33 = r33
        mov                 ar.ccv = r33
        cmpxchg4.rel        r8  = [r32], r34
        mf
        br.ret.sptk.many    b0
.endp   InternalSyncCompareExchange32
