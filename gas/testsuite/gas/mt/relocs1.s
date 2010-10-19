;; This test is meant to exercise every unusual reloc supported
;; by the mrisc port.  (Ok, so there's only one so far.  :P)

	.text
text:	
	.global _start
_start:	
	add R1,R1,R3

; Make sure local fixups work.
local:
        jmp (dummy2-dummy1)  

; Test the PC16 reloc.
none:
	 or R0,R0,R0 ;nop to conform to scheduling restrictions
	 jmp local                
	                      
; Test the %hi16 and %lo16 relocs
addui R1,R2,#%hi16(d2)
addui R1,R2,#%lo16(d2) 	
addui R1,R2,#%hi16(65536)
addui R1,R2,#%lo16(65536)
addui R1,R2,#%hi16($FFFFEEEE)
addui R1,R2,#%lo16($FFFFEEEE)

dummy1: addui R1, R2, #5
dummy2: addui R1, R2, #6

	.data
d1:	.byte $f
