; { dg-do assemble }
; { dg-options "--march=v0_v10" }

; Check that valid v32-specific mnemonics and operands are not
; recognized for v10.  (Also used elsewhere to check that valid
; v32-specific insns and operands are recognized at assembly and
; disassembly for v32.)

 .text
here:
 move.d [$acr],$r3		; No error - $acr treated as a symbol.
 move.d [$r5+],$acr		; { dg-error "(Illegal|Invalid) operands" }
 move.d $acr,$r7		; No error - $acr treated as a symbol.
 move.d $r8,$acr		; { dg-error "(Illegal|Invalid) operands" }
 move $acr,$srp			; No error - $acr treated as a symbol.
 addc $r0,$r0			; { dg-error "Unknown opcode" }
 addc $acr,$acr			; { dg-error "Unknown opcode" }
 addc $r6,$r1			; { dg-error "Unknown opcode" }
 addc [$r3],$r1			; { dg-error "Unknown opcode" }
 addc [$r0],$r0			; { dg-error "Unknown opcode" }
 addc [$acr],$acr		; { dg-error "Unknown opcode" }
 addc [$acr],$r1		; { dg-error "Unknown opcode" }
 addc [$r3+],$r1		; { dg-error "Unknown opcode" }
 addi $r8.w,$r2,$acr		; { dg-error "(Illegal|Invalid) operands" }
 addi $r0.b,$r0,$acr		; { dg-error "(Illegal|Invalid) operands" }
 addi $acr.d,$acr,$acr		; { dg-error "(Illegal|Invalid) operands" }
 addo.d [$r3],$r7,$acr		; { dg-error "Unknown opcode" }
 addo.d [$r13+],$r7,$acr	; { dg-error "Unknown opcode" }
 addo.d [$r3],$acr,$acr		; { dg-error "Unknown opcode" }
 addo.b [$r0],$r0,$acr		; { dg-error "Unknown opcode" }
 addo.d [$acr],$acr,$acr	; { dg-error "Unknown opcode" }
 addo.b -1,$acr,$acr		; { dg-error "Unknown opcode" }
 addo.w -1,$acr,$acr		; { dg-error "Unknown opcode" }
 addo.d -1,$acr,$acr		; { dg-error "Unknown opcode" }
 addo.b extsym1,$r3,$acr	; { dg-error "Unknown opcode" }
 addo.w extsym2,$r3,$acr	; { dg-error "Unknown opcode" }
 addo.d extsym3,$r3,$acr	; { dg-error "Unknown opcode" }
 addo.b 127,$acr,$acr		; { dg-error "Unknown opcode" }
 addo.w 32767,$acr,$acr		; { dg-error "Unknown opcode" }
 addo.d 0xffffff,$acr,$acr	; { dg-error "Unknown opcode" }
 addo.b -128,$acr,$acr		; { dg-error "Unknown opcode" }
 addo.w -32768,$acr,$acr	; { dg-error "Unknown opcode" }
 addo.d 0xffffffff,$acr,$acr	; { dg-error "Unknown opcode" }
 lapc .,$r0			; { dg-error "Unknown opcode" }
 lapc .+30,$r4			; { dg-error "Unknown opcode" }
 lapc .+30,$acr			; { dg-error "Unknown opcode" }
 lapc extsym4,$acr		; { dg-error "Unknown opcode" }
 lapc extsym5,$r4		; { dg-error "Unknown opcode" }
 lapc here,$r4			; { dg-error "Unknown opcode" }
 addoq -1,$acr,$acr		; { dg-error "Unknown opcode" }
 addoq 0,$r0,$acr		; { dg-error "Unknown opcode" }
 addoq 127,$r4,$acr		; { dg-error "Unknown opcode" }
 addoq extsym6,$r4,$acr		; { dg-error "Unknown opcode" }
 bas 0xffffffff,$srp		; { dg-error "Unknown opcode" }
 bas extsym7,$bz		; { dg-error "Unknown opcode" }
 bas here,$erp			; { dg-error "Unknown opcode" }
 basc 0xffffffff,$srp		; { dg-error "Unknown opcode" }
 .dword 0
 basc extsym8,$bz		; { dg-error "Unknown opcode" }
 .dword 0
 basc here,$erp			; { dg-error "Unknown opcode" }
 .dword 0
 bsb .				; { dg-error "Unknown opcode" }
 nop
 bsb here			; { dg-error "Unknown opcode" }
 nop
 bsr extsym9			; { dg-error "Unknown opcode" }
 bsr here			; { dg-error "Unknown opcode" }
 bsrc 0xffffffff		; { dg-error "Unknown opcode" }
 .dword 0
 bsrc extsym10			; { dg-error "Unknown opcode" }
 .dword 0
 bsrc here			; { dg-error "Unknown opcode" }
 .dword 0
 fidxd [$r0]			; { dg-error "Unknown opcode" }
 fidxd [$acr]			; { dg-error "Unknown opcode" }
 fidxi [$r0]			; { dg-error "Unknown opcode" }
 fidxi [$acr]			; { dg-error "Unknown opcode" }
 ftagd [$r0]			; { dg-error "Unknown opcode" }
 ftagd [$acr]			; { dg-error "Unknown opcode" }
 ftagi [$r0]			; { dg-error "Unknown opcode" }
 ftagi [$acr]			; { dg-error "Unknown opcode" }
 jas $r0,$bz			; { dg-error "Unknown opcode" }
 jas $acr,$usp			; { dg-error "Unknown opcode" }
 jas extsym9,$bz		; { dg-error "Unknown opcode" }
 jas here,$srp			; { dg-error "Unknown opcode" }
 jasc $r0,$bz			; { dg-error "Unknown opcode" }
 .dword 0
 jasc $acr,$usp			; { dg-error "Unknown opcode" }
 .dword 0
 jasc 0xffffffff,$srp		; { dg-error "Unknown opcode" }
 .dword 0
 jasc extsym11,$bz		; { dg-error "Unknown opcode" }
 .dword 0
 jasc here,$erp			; { dg-error "Unknown opcode" }
 .dword 0
 jump $srp			; No error - $srp treated as a symbol.
 jump $bz			; No error - $bz treated as a symbol.
 mcp $p0,$r0			; { dg-error "Unknown opcode" }
 mcp $mof,$acr			; { dg-error "Unknown opcode" }
 mcp $srp,$r2			; { dg-error "Unknown opcode" }
 move $s0,$r0			; { dg-error "(Illegal|Invalid) operands" }
 move $s15,$acr			; { dg-error "(Illegal|Invalid) operands" }
 move $s5,$r3			; { dg-error "(Illegal|Invalid) operands" }
 move $r0,$s0			; { dg-error "(Illegal|Invalid) operands" }
 move $acr,$s15			; { dg-error "(Illegal|Invalid) operands" }
 move $r4,$s10			; { dg-error "(Illegal|Invalid) operands" }
 rfe				; { dg-error "Unknown opcode" }
 rfg				; { dg-error "Unknown opcode" }
 rete				; { dg-error "Unknown opcode" }
 retn				; { dg-error "Unknown opcode" }
 ssb $r0			; { dg-error "Unknown opcode" }
 ssb $acr			; { dg-error "Unknown opcode" }
 ssb $r10			; { dg-error "Unknown opcode" }
 sfe				; { dg-error "Unknown opcode" }
 halt				; { dg-error "Unknown opcode" }
 rfn				; { dg-error "Unknown opcode" }
