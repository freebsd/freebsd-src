.text
.intel_syntax noprefix

gs_foo:
 ret

bar:
 lea	eax, .LC0@GOTOFF[ebx]
 mov	eax, DWORD PTR gs_foo@GOT[ebx]
 nop
.p2align 4,0
