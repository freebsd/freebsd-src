	.data


; GAS used to mis-parse the embedded quotes
	.STRING "#include \"awk.def\"\x0a\x00"

; Octal escapes used to consume > 3 chars which led to this
; string being screwed in a big way.
	.STRING "\0110x123"


