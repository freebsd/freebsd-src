 	.text

_start:
	rep insb
	rep outsb
	rep movsb
	rep lodsb
	rep stosb
	repz cmpsb
	repz scasb

	rep insw
	rep outsw
	rep movsw
	rep lodsw
	rep stosw
	repz cmpsw
	repz scasw

	rep insl
	rep outsl
	rep movsl
	rep lodsl
	rep stosl
	repz cmpsl
	repz scasl

	addr16 rep insb
	addr16 rep outsb
	addr16 rep movsb
	addr16 rep lodsb
	addr16 rep stosb
	addr16 repz cmpsb
	addr16 repz scasb

	addr16 rep insw
	addr16 rep outsw
	addr16 rep movsw
	addr16 rep lodsw
	addr16 rep stosw
	addr16 repz cmpsw
	addr16 repz scasw

	addr16 rep insl
	addr16 rep outsl
	addr16 rep movsl
	addr16 rep lodsl
	addr16 rep stosl
	addr16 repz cmpsl
	addr16 repz scasl

	.p2align        4,0
