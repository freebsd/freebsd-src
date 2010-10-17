; Check the push and pop builtin "macros".
 .text
 .syntax no_register_prefix
start:
 push r1
 push r0
 push r4
 ; Check that there is no recognition of invalid offsets.
 move.b r5,[sp=sp-8]
 move.w r5,[sp=sp-8]
 move.d r5,[sp=sp-8]

 move.b r5,[sp=sp-5]
 move.w r5,[sp=sp-5]
 move.d r5,[sp=sp-5]

 move.w r5,[sp=sp-4]
 move.b r3,[sp=sp-4]

 move.d r5,[sp=sp-3]
 move.w r11,[sp=sp-3]
 move.b r5,[sp=sp-3]

 move.d r5,[sp=sp-2]
 move.b r5,[sp=sp-2]

 move.d r5,[sp=sp-1]
 move.w r5,[sp=sp-1]

 move.d r5,[sp=sp+0]
 move.b r5,[sp=sp+0]
 move.w r5,[sp=sp+0]

 move.d r5,[sp=sp+1]
 move.w r5,[sp=sp+1]
 move.b r5,[sp=sp+1]

 move.d r5,[sp=sp+2]
 move.w r5,[sp=sp+2]
 move.b r5,[sp=sp+2]

 move.d r5,[sp=sp+3]
 move.w r5,[sp=sp+3]
 move.b r5,[sp=sp+3]

 move.d r5,[sp=sp+4]
 move.w r5,[sp=sp+4]
 move.b r5,[sp=sp+4]

 move.d r5,[sp=sp+5]
 move.w r5,[sp=sp+5]
 move.b r5,[sp=sp+5]

 move.d r1,[sp=sp+8]
 move.w r9,[sp=sp+8]
 move.b r13,[sp=sp+8]

;
; All these will have postincrement on the "real" instruction
; (e.g. "move.d [sp+],r6") which is the actual insn recognized as
; pop; it is *not* e.g. "move.d [sp=sp+4],r6".
;  Here we make sure that neither the combination nor the second
; is interpreted as a pop.
;
 move.b [sp=sp+8],r5
 move.w [sp=sp+8],r5
 move.d [sp=sp+8],r5

 move.b [sp=sp+5],r5
 move.w [sp=sp+5],r5
 move.d [sp=sp+5],r5

 move.d [sp=sp+4],r5
 move.w [sp=sp+4],r5
 move.b [sp=sp+4],r3

 move.d [sp=sp+3],r5
 move.w [sp=sp+3],r11
 move.b [sp=sp+3],r5

 move.d [sp=sp+2],r5
 move.w [sp=sp+2],r5
 move.b [sp=sp+2],r5

 move.d [sp=sp+1],r5
 move.w [sp=sp+1],r5
 move.b [sp=sp+1],r5

 move.d [sp=sp-0],r5
 move.w [sp=sp-0],r5
 move.b [sp=sp-0],r5

 move.d [sp=sp-1],r5
 move.w [sp=sp-1],r5
 move.b [sp=sp-1],r5

 move.d [sp=sp-2],r5
 move.w [sp=sp-2],r5
 move.b [sp=sp-2],r5

 move.d [sp=sp-3],r5
 move.w [sp=sp-3],r5
 move.b [sp=sp-3],r5

 move.d [sp=sp-4],r5
 move.w [sp=sp-4],r5
 move.b [sp=sp-4],r5

 move.d [sp=sp-5],r5
 move.w [sp=sp-5],r5
 move.b [sp=sp-5],r5

 move.d [sp=sp-8],r5
 move.w [sp=sp-8],r5
 move.b [sp=sp-8],r5

 push r0
 pop r2
 pop r3
 push r13
end:
