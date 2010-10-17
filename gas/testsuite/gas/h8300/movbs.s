	.h8300s
	.text
h8300s_movb:
	mov.b r0l,r1l
	mov.b #16,r0l
	mov.b @er1,r0l
	mov.b @(16:16,er1),r0l
	mov.b @(32:32,er1),r0l
	mov.b @er1+,r0l
	mov.b @16:8,r0l
	mov.b @h8300s_movb:16,r0l
	mov.b @h8300s_movb:32,r0l
	mov.b r0l,@er1
	mov.b r0l,@(16:16,er1)
	mov.b r0l,@(32:32,er1)
	mov.b r0l,@-er1
	mov.b r0l,@16:8
	mov.b r0l,@h8300s_movb:16
	mov.b r0l,@h8300s_movb:32

