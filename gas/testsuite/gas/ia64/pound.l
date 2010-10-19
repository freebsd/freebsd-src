.*: Assembler messages:
.*:35: Warning: .* WAW .*
#...
.*:41: Error: symbol .esym. .* .efunction.
.*:43: Error: section .\.extra. .* .esection.
GAS LISTING .*
#...
[[:space:]]*[[:digit:]]+[[:space:]]+\.explicit
[[:space:]]*[[:digit:]]+[[:space:]]+
[[:space:]]*[[:digit:]]+[[:space:]]+\.global esym#
[[:space:]]*[[:digit:]]+[[:space:]]+
[[:space:]]*[[:digit:]]+[[:space:]]+\.section \.extra#, "a", @progbits
[[:space:]]*[[:digit:]]+[[:space:]]+
[[:space:]]*[[:digit:]]+[[:space:]]+\.text
[[:space:]]*[[:digit:]]+[[:space:]]+
[[:space:]]*[[:digit:]]+[[:space:]]+	break		0
[[:space:]]*[[:digit:]]+[[:space:]]+
[[:space:]]*[[:digit:]]+[[:space:]]+\?*[[:space:]]+[[:xdigit:]]+[[:space:]]+\.proc psym
#...
[[:space:]]*[[:digit:]]+[[:space:]]+psym:
[[:space:]]*[[:digit:]]+[[:space:]]+	mov\.ret\.sptk	b7 = r0, tag#
[[:space:]]*[[:digit:]]+[[:space:]]+	mov		r8 = 0
[[:space:]]*[[:digit:]]+[[:space:]]+\[tag:\]	br\.ret\.sptk	rp
[[:space:]]*[[:digit:]]+[[:space:]]+\?*[[:space:]]+[[:xdigit:]]+[[:space:]]+\.endp psym
#...
[[:space:]]*[[:digit:]]+[[:space:]]+
[[:space:]]*[[:digit:]]+[[:space:]]+\.proc esym#
[[:space:]]*[[:digit:]]+[[:space:]]+\.entry entry#
[[:space:]]*[[:digit:]]+[[:space:]]+esym:
[[:space:]]*[[:digit:]]+[[:space:]]+\.unwentry
[[:space:]]*[[:digit:]]+[[:space:]]+\.personality psym#
[[:space:]]*[[:digit:]]+[[:space:]]+\.regstk 0, 8, 0, 8
[[:space:]]*[[:digit:]]+[[:space:]]+\.rotp p#\[2\], p1#\[4\]
[[:space:]]*[[:digit:]]+[[:space:]]+\.rotr r#\[2\], r1#\[4\]
[[:space:]]*[[:digit:]]+[[:space:]]+\.reg\.val r#\[1\], 0
[[:space:]]*[[:digit:]]+[[:space:]]+\.reg\.val r1#\[3\], 0
[[:space:]]*[[:digit:]]+[[:space:]]+\(p1#\[1\]\) cmp\.eq	p\[0\] = r\[1\], r1#\[1\]
[[:space:]]*[[:digit:]]+[[:space:]]+\(p1#\[3\]\) cmp\.eq	p#\[1\] = r#\[1\], r1#\[3\]
[[:space:]]*[[:digit:]]+[[:space:]]+\.pred\.rel "mutex", p#\[0\], p\[1\]
[[:space:]]*[[:digit:]]+[[:space:]]+	nop		0
[[:space:]]*[[:digit:]]+[[:space:]]+	;;
[[:space:]]*[[:digit:]]+[[:space:]]+entry:
[[:space:]]*[[:digit:]]+[[:space:]]+\?*[[:space:]]+61828446[[:space:]]+\(p\[0\]\)	mov		r8 = 1
[[:space:]]*[[:digit:]]+[[:space:]]+00781509[[:space:]]*
[[:space:]]*[[:digit:]]+[[:space:]]+95007000[[:space:]]*
[[:space:]]*[[:digit:]]+[[:space:]]+00000400[[:space:]]*
[[:space:]]*[[:digit:]]+[[:space:]]+\(p#\[1\]\)	mov		r8 = 0
[[:space:]]*[[:digit:]]+[[:space:]]+	br\.ret\.sptk	rp
[[:space:]]*[[:digit:]]+[[:space:]]+\.xdata4 \.extra#, -1
[[:space:]]*[[:digit:]]+[[:space:]]+\?*[[:space:]]+11420400+[[:space:]]+\.endp esym#
[[:space:]]*[[:digit:]]+[[:space:]]+00648400[[:space:]]*
[[:space:]]*[[:digit:]]+[[:space:]]+00004880[[:space:]]*
[[:space:]]*[[:digit:]]+[[:space:]]+00008400[[:space:]]*
#...
[[:space:]]*[[:digit:]]+[[:space:]]+\.alias esym#, "efunction"
[[:space:]]*[[:digit:]]+[[:space:]]+\.alias esym, "efunc"
[[:space:]]*[[:digit:]]+[[:space:]]+\.secalias \.extra#, "esection"
[[:space:]]*[[:digit:]]+[[:space:]]+\.secalias \.extra, "esec"
