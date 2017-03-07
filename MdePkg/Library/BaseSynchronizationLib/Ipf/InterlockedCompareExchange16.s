/// @file
///   Contains an implementation of InterlockedCompareExchange16 on Itanium-
///   based architecture.
///
/// Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
/// Copyright (c) 2015, Linaro Ltd. All rights reserved.<BR>
/// This program and the accompanying materials
/// are licensed and made available under the terms and conditions of the BSD License
/// which accompanies this distribution.  The full text of the license may be found at
/// http://opensource.org/licenses/bsd-license.php.
///
/// THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
/// WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
///
/// Module Name:  InterlockedCompareExchange16.s
///
///

.auto
.text

.proc   InternalSyncCompareExchange16
.type   InternalSyncCompareExchange16, @function
InternalSyncCompareExchange16::
        zxt2                r33 = r33
        mov                 ar.ccv = r33
        cmpxchg2.rel        r8  = [r32], r34
        mf
        br.ret.sptk.many    b0
.endp   InternalSyncCompareExchange16
