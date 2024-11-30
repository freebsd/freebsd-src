//------------------------------------------------------------------------------
// AArch64/StackCookieInterrupt.S
//
// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: BSD-2-Clause-Patent
//------------------------------------------------------------------------------

    .text

//------------------------------------------------------------------------------
// Calls an interrupt using the vector specified by PcdStackCookieExceptionVector
//
// VOID
// TriggerStackCookieInterrupt (
//   VOID
//   );
//------------------------------------------------------------------------------
.global ASM_PFX(TriggerStackCookieInterrupt)
ASM_PFX(TriggerStackCookieInterrupt):
    svc FixedPcdGet8 (PcdStackCookieExceptionVector)
    ret
