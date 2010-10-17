	.text
	.hword global
	.hword global+3
	.hword global-.
	.word global
	.word global+3
	.word global-.
	.byte global
	.byte global-0x7F00
	.byte global+3
	.byte global-.
dglobal:
dwglobal:
	.globl dglobal
	.globl dwglobal
	.weak dwglobal
	.hword dglobal
	.hword dwglobal

	