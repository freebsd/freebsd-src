	.text
.L1:
        .align 4
.L2:
.L3:
        .section        .data,"",%progbits
        .long		.L3 - .L1
        .section        .text,"axG",%progbits,foo,comdat
	.word		0
	
	