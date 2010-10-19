; Test unsupported ARCH in -march=ARCH.
; { dg-do assemble }
; { dg-options "--march=whatever" }
; { dg-error ".* invalid <arch> in --march=<arch>: whatever" "" { target cris-*-* } 0 }
 nop
