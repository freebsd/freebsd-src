.att_syntax noprefix

foo:	jmpw es:*(ebx)
	mov (0x8*0xa),ah
	mov $(8*4),dl
	mov $foo,ebx
	fxch st(1)
	mov fs,ss:1234(ecx,eax,4)
	mov gs,ds:(,ebp,8)
	mov ah,es:0
	mov cs:-128(esp,edx),esi
	rep movsl gs:(esi),es:(edi)
	in dx,al
	outw (dx)
 addr16 rclb cl,(si)
	mov cr2,eax
	psrld $4,mm0
	inc di
	push cx
	pop ax
	xchg bx,bp
	pushl $2

# Force a good alignment.
.p2align	4,0
