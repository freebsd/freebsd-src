        .section .text
        .global _fun
xc16x_mov:

	mov r0,r1
	mov r0,#02
	mov r0,#0xfcbe
	mov r0,[r1]
	mov r0,[r1+]
	mov [r0],r1
	mov [-r0],r1
	mov [r0],[r1]
	mov [r0+],[r1]
	mov [r0],[r1+]
	mov r0,[r0+#0xffcb]
	mov [r0+#0xffcb],r0
	mov [r0],0xffcb
	mov 0xffcb,[r0]
	mov r0,0xffcb
	mov 0xffcb,r0
