; Test mismatch of --march=ARCH1 and .arch ARCH2.
; { dg-do assemble }
; { dg-options "--march=v0_v10" }
 .arch v32 ; { dg-error ".arch <arch> requires a matching --march=" }

