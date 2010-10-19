	.psr abi64
	.global esym
	.section .rodata, "a", @progbits
	.text
_start:
	adds		r1 = @gprel(esym), r0

	adds		r1 = @ltoff(esym), r0
	.xdata4 .rodata, @ltoff(esym)
	.xdata8 .rodata, @ltoff(esym)

	adds		r1 = @pltoff(esym), r0
	.xdata4 .rodata, @pltoff(esym)

	adds		r1 = @fptr(esym), r0
	mov		r2 = @fptr(esym)

	adds		r1 = @pcrel(esym), r0

	adds		r1 = @ltoff(@fptr(esym)), r0

	adds		r1 = @segrel(esym), r0
	mov		r2 = @segrel(esym)
	movl		r3 = @segrel(esym)

	adds		r1 = @secrel(esym), r0
	mov		r2 = @secrel(esym)
	movl		r3 = @secrel(esym)

	adds		r1 = @ltv(esym), r0
	mov		r2 = @ltv(esym)
	movl		r3 = @ltv(esym)

	adds		r1 = @iplt(esym), r0
	mov		r2 = @iplt(esym)
	movl		r3 = @iplt(esym)
	.xdata4 .rodata, @iplt(esym)
	.xdata8 .rodata, @iplt(esym)

	adds		r1 = @ltoffx(esym), r0

	.xdata4 .rodata, @tprel(esym)

	adds		r1 = @ltoff(@tprel(esym)), r0
	movl		r3 = @ltoff(@tprel(esym))
	.xdata4 .rodata, @ltoff(@tprel(esym))
	.xdata8 .rodata, @ltoff(@tprel(esym))

	adds		r1 = @dtpmod(esym), r0
	mov		r2 = @dtpmod(esym)
	movl		r3 = @dtpmod(esym)
	.xdata4 .rodata, @dtpmod(esym)

	adds		r1 = @ltoff(@dtpmod(esym)), r0
	movl		r3 = @ltoff(@dtpmod(esym))
	.xdata4 .rodata, @ltoff(@tprel(esym))
	.xdata8 .rodata, @ltoff(@tprel(esym))

	adds		r1 = @ltoff(@dtprel(esym)), r0
	movl		r3 = @ltoff(@dtprel(esym))
	.xdata4 .rodata, @ltoff(@dtprel(esym))
	.xdata8 .rodata, @ltoff(@dtprel(esym))
