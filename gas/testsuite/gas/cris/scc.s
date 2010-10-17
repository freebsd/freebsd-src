; sCC

 .text
 .syntax no_register_prefix
start:
 scc r1
 shs r0 ; same as scc
 scs r5
 slo r13 ; same as scs
 sne r7
 seq r9
 svc r10
 svs r11
 spl r3
 smi r4
 sls r8
 shi r12
 sge r2
 slt r4
 sgt r12
 sle r8
 sa r1
 sext r11
 swf r8
; Add new condition names here, not above.
end:
