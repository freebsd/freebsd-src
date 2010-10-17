	.text
	.globl	bar
	.type bar,@function
bar:
	ptabs	r18, tr0
	blink	tr0, r63
	.Lfe_bar: .size bar,.Lfe_bar-X
