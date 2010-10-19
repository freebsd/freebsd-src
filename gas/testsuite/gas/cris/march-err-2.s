; Test unsupported ARCH in -march=ARCH, where there's an option
; which is a proper substring.
; { dg-do assemble }
; { dg-options "--march=v10_v32" }
; { dg-error ".* invalid <arch> in --march=<arch>: v10_v32" "" { target cris-*-* } 0 }
 nop
