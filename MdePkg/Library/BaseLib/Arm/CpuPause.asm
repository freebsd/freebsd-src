;------------------------------------------------------------------------------
;
; CpuPause() for ARM
;
; Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
; Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

  EXPORT CpuPause
  AREA cpu_pause, CODE, READONLY

;/**
;  Requests CPU to pause for a short period of time.
;
;  Requests CPU to pause for a short period of time. Typically used in MP
;  systems to prevent memory starvation while waiting for a spin lock.
;
;**/
;VOID
;EFIAPI
;CpuPause (
;  VOID
;  );
;
CpuPause
    NOP
    NOP
    NOP
    NOP
    NOP
    BX LR

  END
