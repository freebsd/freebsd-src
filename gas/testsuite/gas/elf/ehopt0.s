	.text
LFB1:
	.4byte	0
L1:
	.4byte	0
LFE1:
	.section	.eh_frame,"aw"
__FRAME_BEGIN__:
	.4byte	LECIE1-LSCIE1
LSCIE1:
	.4byte	0x0
	.byte	0x1
	.ascii "z\0"
	.byte	0x1
	.byte	0x78
	.byte	0x1a
	.byte	0x0
	.byte	0x4
	.4byte	1
	.p2align 1
LECIE1:
LSFDE1:
	.4byte	LEFDE1-LASFDE1
LASFDE1:
	.4byte	LASFDE1-__FRAME_BEGIN__
	.4byte	LFB1
	.4byte	LFE1-LFB1
	.byte	0x4
	.4byte	LFE1-LFB1
	.byte	0x4
	.4byte	L1-LFB1
LEFDE1:
