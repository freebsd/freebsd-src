 .text

_start:
	rex/fxsave (%rax)
	rex64/fxsave (%rax)
	rex/fxsave (%r8)
	rex64/fxsave (%r8)
	rex/fxsave (,%r8)
	rex64/fxsave (,%r8)
	rex/fxsave (%r8,%r8)
	rex64/fxsave (%r8,%r8)
