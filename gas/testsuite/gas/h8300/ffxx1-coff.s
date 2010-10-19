	.equ    p6ddr,  0xffb9          ;0x7f for output 
	.equ    p6dr,    0xffbb
	.equ    seed,    0x01
	.text
	.org    0
reset:  .word   main            ;reset vector 
;
	.org    0x400
main:   mov.b   #0x7f,r0l       ;port 6 ddr = 7F 
	mov.b   @0xffbb:8,r0l   ;***test***
	mov.b   r0l,@p6ddr:16
;
	mov.b   #seed,r0l       ;start with 0000001
loop:   mov.b   r0l,@p6dr:16    ;output to port 6 
delay:  mov.w   #0x0000,r1
deloop: adds    #1,r1
	bne     deloop:8        ;not = 0
	rotl    r0l
        bra     loop:8
	.word	0
