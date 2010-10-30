	.global main

test:
	mov     $0,0

# negative test from case 106708

L1:
        mov     $1,1
        mov     $1,((L1 & 0x00007fff) | 0x00008000)
        ret
        mov     $0,0
main:
	mov     $0,0
	ret
