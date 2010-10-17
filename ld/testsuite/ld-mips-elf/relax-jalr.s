.globl __start
	.space 8
.ent __start
__start:
.Lstart:
	.space 16
        jal __start
	.space 32
        jal __start
	.space 64
	jal .Lstart
.end __start

# make objdump print ...
	.space 8
