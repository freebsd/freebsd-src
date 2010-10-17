	.h8300h
	.text
h8300h_movl:
	mov.l er0,er1
	mov.l #64,er0
	mov.l @er1,er0
	mov.l @(16:16,er1),er0
	mov.l @(32:24,er1),er0
	mov.l @er1+,er0
	mov.l @h8300h_movl:16,er0
	mov.l @h8300h_movl:24,er0
	mov.l er0,@er1
	mov.l er0,@(16:16,er1)
	mov.l er0,@(32:24,er1)
	mov.l er0,@-er1
	mov.l er0,@h8300h_movl:16
	mov.l er0,@h8300h_movl:24

