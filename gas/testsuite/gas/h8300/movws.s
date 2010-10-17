	.h8300s
	.text
h8300s_movw:
	mov.w r0,r1
	mov.w #16,r0
	mov.w @er1,r0
	mov.w @(16:16,er1),r0
	mov.w @(32:32,er1),r0
	mov.w @er1+,r0
	mov.w @h8300s_movw:16,r0
	mov.w @h8300s_movw:32,r0
	mov.w r0,@er1
	mov.w r0,@(16:16,er1)
	mov.w r0,@(32:32,er1)
	mov.w r0,@-er1
	mov.w r0,@h8300s_movw:16
	mov.w r0,@h8300s_movw:32

