	.h8300s
	.text
h8300s_movl:
	mov.l er0,er1
	mov.l #64,er0
	mov.l @er1,er0
	mov.l @(16:16,er1),er0
	mov.l @(32:32,er1),er0
	mov.l @er1+,er0
	mov.l @h8300s_movl:16,er0
	mov.l @h8300s_movl:32,er0
	mov.l er0,@er1
	mov.l er0,@(16:16,er1)
	mov.l er0,@(32:32,er1)
	mov.l er0,@-er1
	mov.l er0,@h8300s_movl:16
	mov.l er0,@h8300s_movl:32

