;------------------------------------------------------------------------------ 
;
; CpuBreakpoint() for ARM
;
; Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
; Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
;------------------------------------------------------------------------------

    EXPORT CpuBreakpoint

  AREA Cpu_Breakpoint, CODE, READONLY

;/**
;  Generates a breakpoint on the CPU.
;
;  Generates a breakpoint on the CPU. The breakpoint must be implemented such
;  that code can resume normal execution after the breakpoint.
;
;**/
;VOID
;EFIAPI
;CpuBreakpoint (
;  VOID
;  );
;
CpuBreakpoint
    swi   0xdbdbdb
    bx    lr
    
  END
