; btsti does not modify r1
; There is no resource conflict so no warning.

	.text
foo:
	ld  r1,@r2+  || btsti r1 , #6
	add r1,r2    || btsti r1 , #6

