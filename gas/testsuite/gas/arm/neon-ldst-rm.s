@ test register and multi-register loads and stores.

	.text
	.arm
	.syntax unified

	.macro multi op dir="" wb=""
	\op\dir r2\wb,{d2}
	\op\dir r2\wb,{d2-d3}
	\op\dir r2\wb,{q2-q3}
	\op\dir r2\wb,{q12-q14,q15}
	\op\dir r2\wb,{d3,d4,d5-d8,d9,d10,d11,d12-d16,d17-d18}
	.endm

	multi vldm
	multi vldm ia
	multi vldm ia "!"
	multi vldm db "!"

	multi vstm
	multi vstm ia
	multi vstm ia "!"
	multi vstm db "!"

backward:
	.word 500

	.macro single op offset=""
	\op d5,[r3]
	\op d5,[r3,#-\offset]
	\op d5,[r3,#\offset]
	.endm

	vldr d22, forward

	single vldr 4
	single vstr 4
	single vldr 256
	single vstr 256

forward:
	.word 700

	vldr d7, backward
