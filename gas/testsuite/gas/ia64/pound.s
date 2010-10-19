.explicit

.global esym#

.section .extra#, "a", @progbits

.text

	break		0

.proc psym
psym:
	mov.ret.sptk	b7 = r0, tag#
	mov		r8 = 0
[tag:]	br.ret.sptk	rp
.endp psym

.proc esym#
.entry entry#
esym:
.unwentry
.personality psym#
.regstk 0, 8, 0, 8
.rotp p#[2], p1#[4]
.rotr r#[2], r1#[4]
.reg.val r#[1], 0
.reg.val r1#[3], 0
(p1#[1]) cmp.eq	p[0] = r[1], r1#[1]
(p1#[3]) cmp.eq	p#[1] = r#[1], r1#[3]
.pred.rel "mutex", p#[0], p[1]
	nop		0
	;;
entry:
(p[0])	mov		r8 = 1
(p#[1])	mov		r8 = 0
	br.ret.sptk	rp
.xdata4 .extra#, -1
.endp esym#

.alias esym#, "efunction"
.alias esym, "efunc"
.secalias .extra#, "esection"
.secalias .extra, "esec"
