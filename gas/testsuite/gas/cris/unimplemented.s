; Test the unimplemented insns (of which some are used in xsim
; and rsim).
;  You may need to remove some from here as new insns emerge.
; Just test a few basic cases, to check that the insn table does
; not contain duplicate entries.  No compelling need for complete coverage.
 .text
 .syntax no_register_prefix
start:
 bmod [r11],r2
 bmod [r11+r3.b],r2
 bmod [r13=r3+r5.d],r2
 bmod [r11+external_symbol],r2
 bmod [external_symbol],r2
 bstore [r11],r2
 bstore [r11+r3.b],r2
 bstore [r13=r3+r5.d],r2
 bstore [r11+external_symbol],r2
 bstore [external_symbol],r2
 div.b r8,r8
 div.b r4,r7
 div.b r0,r0
 div.b r4,r4
 div.w r4,r7
 div.w r0,r0
 div.w r4,r4
 div.d r4,r7
 div.d r0,r0
 div.d r4,r4
end:
