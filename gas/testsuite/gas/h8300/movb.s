	.text
h8300_movb:
	mov.b r0l,r1l
	mov.b #16,r0l
	mov.b @r1,r0l
	mov.b @(16:16,r1),r0l
	mov.b @r1+,r0l
	mov.b @16:8,r0l
	mov.b @h8300_movb:16,r0l
	mov.b r0l,@r1
	mov.b r0l,@(16:16,r1)
	mov.b r0l,@-r1
	mov.b r0l,@16:8
	mov.b r0l,@h8300_movb:16

