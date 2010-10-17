;
; Test that branches work: 8- and 16-bit all insns, relaxing to
; 32-bit, forward and backward.  No need to check the border
; cases for *all* insns.
;
 .text
start_original:
 nop
startm32: ;       start     start2    start3
 nop
 .space 32750-(256-21*2+20)-(21*2+10*2+21*4)-12,0
startm16:
 nop
 ; The size of a bunch of short branches is start2-start = 42,
 ; so make the threshold be dependent of the size of that block,
 ; for the next block; half of them will be relaxed.
 .space 256-21*2-20,0
start:
 nop
 ba start
 bcc start
 bcs start
 beq start
 bwf start
 bext start
 bext start ; leftover, used to be never-implemented "bir"
 bge start
 bgt start
 bhi start
 bhs start
 ble start
 blo start
 bls start
 blt start
 bmi start
 bne start
 bpl start
 bvc start
 bvs start
start2:
 nop
 ba startm16
 bcc startm16
 bcs startm16
 beq startm16
 bwf startm16
 bext startm16
 bext startm16
 bge startm16
 bgt startm16
 bhi startm16
 bhs startm16
 ble startm16
 blo startm16
 bls startm16
 blt startm16
 bmi startm16
 bne startm16
 bpl startm16
 bvc startm16
 bvs startm16
start3:
; Ok, once more to make sure *all* 16-bit variants get ok for
; backward references.
 nop
 ba startm16
 bcc startm16
 bcs startm16
 beq startm16
 bwf startm16
 bext startm16
 bext startm16
 bge startm16
 bgt startm16
 bhi startm16
 bhs startm16
 ble startm16
 blo startm16
 bls startm16
 blt startm16
 bmi startm16
 bne startm16
 bpl startm16
 bvc startm16
 bvs startm16
;
; Now check that dynamically relaxing some of these branches
; from 16-bit to 32-bit works.
;
start4:
 nop
 ba startm32
 bcc startm32
 bcs startm32
 beq startm32
 bwf startm32
 bext startm32
 bext startm32
 bge startm32
 bgt startm32
 bhi startm32
 bhs startm32
 ble startm32
 blo startm32
 bls startm32
 blt startm32
 bmi startm32
 bne startm32
 bpl startm32
 bvc startm32
 bvs startm32
;
; Again, so all insns get to be tested for 32-bit relaxing.
;
start5:
 nop
 ba startm32
 bcc startm32
 bcs startm32
 beq startm32
 bwf startm32
 bext startm32
 bext startm32
 bge startm32
 bgt startm32
 bhi startm32
 bhs startm32
 ble startm32
 blo startm32
 bls startm32
 blt startm32
 bmi startm32
 bne startm32
 bpl startm32
 bvc startm32
 bvs startm32
;
; Now test forward references.  Symmetrically as above.
;
; All to 32-bit:
start6:
 nop
 ba endp32
 bcc endp32
 bcs endp32
 beq endp32
 bwf endp32
 bext endp32
 bext endp32
 bge endp32
 bgt endp32
 bhi endp32
 bhs endp32
 ble endp32
 blo endp32
 bls endp32
 blt endp32
 bmi endp32
 bne endp32
 bpl endp32
 bvc endp32
 bvs endp32
;
; Some get relaxed:
;
start7:
 nop
 ba endp32
 bcc endp32
 bcs endp32
 beq endp32
 bwf endp32
 bext endp32
 bext endp32
 bge endp32
 bgt endp32
 bhi endp32
 bhs endp32
 ble endp32
 blo endp32
 bls endp32
 blt endp32
 bmi endp32
 bne endp32
 bpl endp32
 bvc endp32
 bvs endp32
;
; All to 16-bit:
;
start8:
 nop
 ba endp16
 bcc endp16
 bcs endp16
 beq endp16
 bwf endp16
 bext endp16
 bext endp16
 bge endp16
 bgt endp16
 bhi endp16
 bhs endp16
 ble endp16
 blo endp16
 bls endp16
 blt endp16
 bmi endp16
 bne endp16
 bpl endp16
 bvc endp16
 bvs endp16
;
; Some relaxing:
;
start9:
 nop
 ba endp16
 bcc endp16
 bcs endp16
 beq endp16
 bwf endp16
 bext endp16
 bext endp16
 bge endp16
 bgt endp16
 bhi endp16
 bhs endp16
 ble endp16
 blo endp16
 bls endp16
 blt endp16
 bmi endp16
 bne endp16
 bpl endp16
 bvc endp16
 bvs endp16
;
; And all the short ones, forward.
;
start10:
 ba end
 bcc end
 bcs end
 beq end
 bwf end
 bext end
 bext end
 bge end
 bgt end
 bhi end
 bhs end
 ble end
 blo end
 bls end
 blt end
 bmi end
 bne end
 bpl end
 bvc end
 bvs end
 nop
end:
 nop
 .space 256-21*2-20,0
endp16:
 nop
 .space 32750-(256-21*2+20)-(21*2+10*2+21*4)-12,0
endp32:
 nop
