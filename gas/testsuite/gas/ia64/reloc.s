	.global esym
	.section .rodata.4, "a", @progbits
	.section .rodata.8, "a", @progbits
	.text
_start:
	adds		r1 = esym, r0
	mov		r2 = esym
	movl		r3 = esym
	.xdata4 .rodata.4, esym
	.xdata8 .rodata.8, esym

	mov		r2 = @gprel(esym)
	movl		r3 = @gprel(esym)
	.xdata4 .rodata.4, @gprel(esym)
	.xdata8 .rodata.8, @gprel(esym)

	mov		r2 = @ltoff(esym)
	movl		r3 = @ltoff(esym)

	mov		r2 = @pltoff(esym)
	movl		r3 = @pltoff(esym)
	.xdata8 .rodata.8, @pltoff(esym)

	movl		r3 = @fptr(esym)
	.xdata4 .rodata.4, @fptr(esym)
	.xdata8 .rodata.8, @fptr(esym)

	brl.call.sptk	b1 = esym
	br.call.sptk	b2 = esym
	chk.s		r0, esym
	fchkf		esym
	.xdata4 .rodata.4, @pcrel(esym)
	.xdata8 .rodata.8, @pcrel(esym)

	mov		r2 = @ltoff(@fptr(esym))
	movl		r3 = @ltoff(@fptr(esym))
	.xdata4 .rodata.4, @ltoff(@fptr(esym))
	.xdata8 .rodata.8, @ltoff(@fptr(esym))

	.xdata4 .rodata.4, @segrel(esym)
	.xdata8 .rodata.8, @segrel(esym)

	.xdata4 .rodata.4, @secrel(esym)
	.xdata8 .rodata.8, @secrel(esym)

	// REL32 only in executables
	// REL64 only in executables

	.xdata4 .rodata.4, @ltv(esym)
	.xdata8 .rodata.8, @ltv(esym)

//todo PCREL21BI
	mov		r2 = @pcrel(esym)
	movl		r3 = @pcrel(esym)

	.xdata16 .rodata.8, @iplt(esym)

	// COPY only in executables

//todo	movl		r3 = -esym

	mov		r2 = @ltoffx(esym)
	ld8.mov		r3 = [r2], esym

	adds		r1 = @tprel(esym), r0
	mov		r2 = @tprel(esym)
	movl		r3 = @tprel(esym)
	.xdata8 .rodata.8, @tprel(esym)

	mov		r2 = @ltoff(@tprel(esym))

	.xdata8 .rodata.8, @dtpmod(esym)

	mov		r2 = @ltoff(@dtpmod(esym))

	adds		r1 = @dtprel(esym), r0
	mov		r2 = @dtprel(esym)
	movl		r3 = @dtprel(esym)
	.xdata4 .rodata.4, @dtprel(esym)
	.xdata8 .rodata.8, @dtprel(esym)

	mov		r2 = @ltoff(@dtprel(esym))
