; Test mismatch of --march=ARCH1 and .arch ARCH2.
; { dg-do assemble }
; { dg-options "--march=common_v10_v32" }
 .arch v32 ; { dg-error ".arch <arch> requires a matching --march=" }

