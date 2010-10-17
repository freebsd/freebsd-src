# D30V array test
	.text
	add r2, r3 , __foo
	add r2, r3 , __foo+1
	add r2, r3 , __foo+2
	add r2, r3 , __foo+3
	add r2, r3 , __foo+4
	add r2, r3 , __foo+5
	add r2, r3 , __foo+6
	add r2, r3 , __foo+7
	add r2, r3 , __foo+8
__foo:
	.int 0x12345678
        .int 0x12345678
        .int 0x12345678
