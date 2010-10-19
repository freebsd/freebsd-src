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

	rep movsq
	rep lodsq
	rep stosq
	repz cmpsq
	repz scasq

	addr32 rep insb
	addr32 rep outsb
	addr32 rep movsb
	addr32 rep lodsb
	addr32 rep stosb
	addr32 repz cmpsb
	addr32 repz scasb

	addr32 rep insw
	addr32 rep outsw
	addr32 rep movsw
	addr32 rep lodsw
	addr32 rep stosw
	addr32 repz cmpsw
	addr32 repz scasw

	addr32 rep insl
	addr32 rep outsl
	addr32 rep movsl
	addr32 rep lodsl
	addr32 rep stosl
	addr32 repz cmpsl
	addr32 repz scasl

	addr32 rep movsq
	addr32 rep lodsq
	addr32 rep stosq
	addr32 repz cmpsq
	addr32 repz scasq
