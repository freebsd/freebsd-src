	.set	noreorder
	.ent	tstartb
tstartb:
	lui	$4,%hi(tstartb - 0x8010)	# .text + 0x7fe0
	addiu	$4,$4,%lo(tstartb - 0x8010)
	lui	$4,%hi(tstartb - 0x8000)	# .text + 0x7ff0
	addiu	$4,$4,%lo(tstartb - 0x8000)
	lui	$4,%hi(tstartb)			# .text + 0xfff0
	addiu	$4,$4,%lo(tstartb)
	lui	$4,%hi(tstartb + 0x7ff0)	# .text + 0x17fe0
	addiu	$4,$4,%lo(tstartb + 0x7ff0)
t32b:
	lui	$4,%hi(tstartb + 0x8010)	# .text + 0x18000
	addiu	$4,$4,%lo(tstartb + 0x8010)

	lui	$4,%hi(t32b - 0x8010)		# .text + 0x8000
	addiu	$4,$4,%lo(t32b - 0x8010)
	lui	$4,%hi(t32b - 0x8000)		# .text + 0x8010
	addiu	$4,$4,%lo(t32b - 0x8000)
	lui	$4,%hi(t32b)			# .text + 0x10010
	addiu	$4,$4,%lo(t32b)
	lui	$4,%hi(t32b + 0x7ff0)		# .text + 0x18000
	addiu	$4,$4,%lo(t32b + 0x7ff0)
	lui	$4,%hi(t32b + 0x8010)		# .text + 0x18020
	addiu	$4,$4,%lo(t32b + 0x8010)

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

	lui	$4,%got(tstartb - 0x8010)	# .text + 0x7fe0
	addiu	$4,$4,%lo(tstartb - 0x8010)
	lui	$4,%got(tstartb - 0x8000)	# .text + 0x7ff0
	addiu	$4,$4,%lo(tstartb - 0x8000)
	lui	$4,%got(tstartb)			# .text + 0xfff0
	addiu	$4,$4,%lo(tstartb)
	lui	$4,%got(tstartb + 0x7ff0)	# .text + 0x17fe0
	addiu	$4,$4,%lo(tstartb + 0x7ff0)
	lui	$4,%got(tstartb + 0x8010)	# .text + 0x18000
	addiu	$4,$4,%lo(tstartb + 0x8010)

	lui	$4,%got(t32b - 0x8010)		# .text + 0x8000
	addiu	$4,$4,%lo(t32b - 0x8010)
	lui	$4,%got(t32b - 0x8000)		# .text + 0x8010
	addiu	$4,$4,%lo(t32b - 0x8000)
	lui	$4,%got(t32b)			# .text + 0x10010
	addiu	$4,$4,%lo(t32b)
	lui	$4,%got(t32b + 0x7ff0)		# .text + 0x18000
	addiu	$4,$4,%lo(t32b + 0x7ff0)
	lui	$4,%got(t32b + 0x8010)		# .text + 0x18020
	addiu	$4,$4,%lo(t32b + 0x8010)

	addiu	$4,$4,%gp_rel(sdg - 4)
	addiu	$4,$4,%gp_rel(sdg)
	addiu	$4,$4,%gp_rel(sdg + 4)

	addiu	$4,$4,%gp_rel(sdlb - 4)
	addiu	$4,$4,%gp_rel(sdlb)
	addiu	$4,$4,%gp_rel(sdlb + 4)

	jal	tstartb - 4			# .text + 0xffec
	nop
	jal	tstartb				# .text + 0xfff0
	nop
	jal	tstartb + 4			# .text + 0xfff4
	nop

	jal	t32b - 4			# .text + 0x1000c
	nop
	jal	t32b				# .text + 0x10010
	nop
	jal	t32b + 4			# .text + 0x10014
	nop

	jal	_start - 4
	nop
	jal	_start
	nop
	jal	_start + 4
	nop

	.space	16
	.end	tstartb

	.section .sdata
	.space	16
sdlb:
	.space	16
