.explicit
.text
_start:
{.mfi
	nop.f		1 } nop.x 1
{.mfi
	nop.f		2
}	nop.x		2
{.mfb
	nop.f		3
.xdata1 .data, -1 } .xdata1 .data, -1
	nop.x		4 { nop.x 5
} {	nop.x		6 }
	nop.x		7 {.mmf
	nop.f		8
} .xdata1 .data, -1 { .mfb
	nop.f		9
	br.ret.sptk	rp }
