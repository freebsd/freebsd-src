;------------------------------------------------------------------------------
;
; SpeculationBarrier() for AArch64
;
; Copyright (c) 2019, Linaro Ltd. All rights reserved.
;
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

    EXPORT SpeculationBarrier

    AREA MemoryBarriers, CODE, READONLY

;/**
;  Uses as a barrier to stop speculative execution.
;
;  Ensures that no later instruction will execute speculatively, until all prior
;  instructions have completed.
;
;**/
;VOID
;EFIAPI
;SpeculationBarrier (
;  VOID
;  );
;
SpeculationBarrier
    dsb
    isb
    bx    lr

  END
