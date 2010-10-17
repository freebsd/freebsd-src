# Simple relocations against 16-bit extern symbols.
Main	SETL $4,foo
	POP  45,bar+42
	SWYM  42,baz-2345
