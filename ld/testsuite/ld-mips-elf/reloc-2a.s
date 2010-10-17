	.globl	_start
	.globl	sdg
	.set	noreorder
	.ent	tstarta
tstarta:
_start:
	lui	$4,%hi(tstarta - 0x8010)
	addiu	$4,$4,%lo(tstarta - 0x8010)
	lui	$4,%hi(tstarta - 0x8000)
	addiu	$4,$4,%lo(tstarta - 0x8000)
	lui	$4,%hi(tstarta)
	addiu	$4,$4,%lo(tstarta)
	lui	$4,%hi(tstarta + 0x7ff0)
	addiu	$4,$4,%lo(tstarta + 0x7ff0)
t32a:
	lui	$4,%hi(tstarta + 0x8010)
	addiu	$4,$4,%lo(tstarta + 0x8010)

	lui	$4,%hi(t32a - 0x8010)
	addiu	$4,$4,%lo(t32a - 0x8010)
	lui	$4,%hi(t32a - 0x8000)
	addiu	$4,$4,%lo(t32a - 0x8000)
	lui	$4,%hi(t32a)
	addiu	$4,$4,%lo(t32a)
	lui	$4,%hi(t32a + 0x7ff0)
	addiu	$4,$4,%lo(t32a + 0x7ff0)
	lui	$4,%hi(t32a + 0x8010)
	addiu	$4,$4,%lo(t32a + 0x8010)

	lui	$4,%hi(_start - 0x8010)
	addiu	$4,$4,%lo(_start - 0x8010)
	lui	$4,%hi(_start - 0x8000)
	addiu	$4,$4,%lo(_start - 0x8000)
	lui	$4,%hi(_start)
	addiu	$4,$4,%lo(_start)
	lui	$4,%hi(_start + 0x7ff0)
	addiu	$4,$4,%lo(_start + 0x7ff0)
	lui	$4,%hi(_start + 0x8010)
	addiu	$4,$4,%lo(_start + 0x8010)

	addiu	$4,$4,%gp_rel(sdg - 4)
	addiu	$4,$4,%gp_rel(sdg)
	addiu	$4,$4,%gp_rel(sdg + 4)

	addiu	$4,$4,%gp_rel(sdla - 4)
	addiu	$4,$4,%gp_rel(sdla)
	addiu	$4,$4,%gp_rel(sdla + 4)

	jal	tstarta - 4
	nop
	jal	tstarta
	nop
	jal	tstarta + 4
	nop

	jal	t32a - 4
	nop
	jal	t32a
	nop
	jal	t32a + 4
	nop

	jal	_start - 4
	nop
	jal	_start
	nop
	jal	_start + 4
	nop

	.org	0xfff0

	.end	tstarta

	.section .sdata
	.space	16
sdg:
sdla:
	.space	16
