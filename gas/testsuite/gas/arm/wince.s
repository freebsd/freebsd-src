	.global	global_data
	.text
	.global	global_sym
	.def global_sym; .scl	2; .type 32; .endef

global_data:
	.word	global_data+7

global_sym:
def_sym:
undef_sym:
	nop
	nop
	nop
	b	global_sym
	bl	global_sym
	beq	global_sym
	b	def_sym
	bl	def_sym
	beq	def_sym
	b	undef_sym
	bl	undef_sym
	ldr	r0, global_sym
	ldr	r0, def_sym
	ldr	r0, undef_sym
