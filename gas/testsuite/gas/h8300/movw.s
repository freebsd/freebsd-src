	.text
h8300_movw:
	mov.w r0,r1
	mov.w #16,r0
	mov.w @r1,r0
	mov.w @(16:16,r1),r0
	mov.w @r1+,r0
	mov.w @h8300_movw:16,r0
	mov.w r0,@r1
	mov.w r0,@(16:16,r1)
	mov.w r0,@-r1
	mov.w r0,@h8300_movw:16

