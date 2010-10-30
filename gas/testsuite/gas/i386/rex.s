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

# Test prefixes family.
	rex
	rex.B
	rex.X
	rex.XB
	rex.R
	rex.RB
	rex.RX
	rex.RXB
	rex.W
	rex.WB
	rex.WX
	rex.WXB
	rex.WR
	rex.WRB
	rex.WRX
	rex.WRXB
