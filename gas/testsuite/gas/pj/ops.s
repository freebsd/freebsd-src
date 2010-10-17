.L0:	nop
.globl foo0
foo0:
.L1:	nop
.globl foo1
foo1:
.L2:	nop
.globl foo2
foo2:
.L3:	nop
.globl foo3
foo3:
.L4:	nop
.globl foo4
foo4:
.L5:	nop
.globl foo5
foo5:
.L6:	nop
.globl foo6
foo6:
.L7:	nop
.globl foo7
foo7:
.L8:	nop
.globl foo8
foo8:
.L9:	nop
.globl foo9
foo9:
.L10:	nop
.globl foo10
foo10:
.L11:	nop
.globl foo11
foo11:
.L12:	nop
.globl foo12
foo12:
.L13:	nop
.globl foo13
foo13:
.L14:	nop
.globl foo14
foo14:
.L15:	nop
.globl foo15
foo15:
.L16:	nop
.globl foo16
foo16:
.L17:	nop
.globl foo17
foo17:
.L18:	nop
.globl foo18
foo18:
.L19:	nop
.globl foo19
foo19:
	nop
	aconst_null
	iconst_m1
	iconst_0
	iconst_1
	iconst_2
	iconst_3
	iconst_4
	iconst_5
	lconst_0
	lconst_1
	fconst_0
	fconst_1
	fconst_2
	dconst_0
	dconst_1
	bipush -25
	sipush -23610
	ldc
	ldc_w
	ldc2_w
	iload 105
	lload 115
	fload 81
	dload 255
	aload 74
	iload_0
	iload_1
	iload_2
	iload_3
	lload_0
	lload_1
	lload_2
	lload_3
	fload_0
	fload_1
	fload_2
	fload_3
	dload_0
	dload_1
	dload_2
	dload_3
	aload_0
	aload_1
	aload_2
	aload_3
	iaload
	laload
	faload
	daload
	aaload
	baload
	caload
	saload
	istore 236
	lstore 41
	fstore 205
	dstore 186
	astore 171
	istore_0
	istore_1
	istore_2
	istore_3
	lstore_0
	lstore_1
	lstore_2
	lstore_3
	fstore_0
	fstore_1
	fstore_2
	fstore_3
	dstore_0
	dstore_1
	dstore_2
	dstore_3
	astore_0
	astore_1
	astore_2
	astore_3
	iastore
	lastore
	fastore
	dastore
	aastore
	bastore
	castore
	sastore
	pop
	pop2
	dup
	dup_x1
	dup_x2
	dup2
	dup2_x1
	dup2_x2
	swap
	iadd
	ladd
	fadd
	dadd
	isub
	lsub
	fsub
	dsub
	imul
	lmul
	fmul
	dmul
	idiv
	ldiv
	fdiv
	ddiv
	irem
	lrem
	frem
	drem
	ineg
	lneg
	fneg
	dneg
	ishl
	lshl
	ishr
	lshr
	iushr
	lushr
	iand
	land
	ior
	lor
	ixor
	lxor
	iinc 242, 123
	i2l
	i2f
	i2d
	l2i
	l2f
	l2d
	f2i
	f2l
	f2d
	d2i
	d2l
	d2f
	i2b
	i2c
	i2s
	lcmp
	fcmpl
	fcmpg
	dcmpl
	dcmpg
	ifeq .L2
	ifne .L11
	iflt .L8
	ifge .L18
	ifgt .L6
	ifle .L10
	if_icmpeq .L11
	if_icmpne .L16
	if_icmplt .L9
	if_icmpge .L13
	if_icmpgt .L8
	if_icmple .L13
	if_acmpeq .L7
	if_acmpne .L8
	goto .L6
	jsr
	ret
.Lt:	tableswitch

	.align 2
	.long .L2-.Lt
	.long 1
	.long 5
	.long .L7-.Lt
	.long .L5-.Lt
	.long .L18-.Lt
	.long .L7-.Lt
	.long .L5-.Lt
.Ll:	lookupswitch

	.align 2
	.long .L6-.Ll
	.long 2
	.long 7
	.long .L15-.Ll
	.long 37
	.long .L16-.Ll
	ireturn
	lreturn
	freturn
	dreturn
	areturn
	return
	getstatic
	putstatic
	getfield
	putfield
	invokevirtual
	invokespecial
	invokestatic
	invokeinterface
	new
	newarray
	anewarray
	arraylength
	athrow
	checkcast
	instanceof
	monitorenter
	monitorexit
	wide
	multianewarray
	ifnull
	ifnonnull
	goto_w
	jsr_w
	breakpoint
	bytecode
	try
	endtry
	catch
	var
	endvar
	sethi -20317
	load_word_index 90, -91
	load_short_index 93, -123
	load_char_index 23, -40
	load_byte_index 233, -34
	load_ubyte_index 212, 43
	store_word_index 178, 77
	na_store_word_index 198, 27
	store_short_index 180, -44
	store_byte_index 17, -114
	load_ubyte
	load_byte
	load_char
	load_short
	load_word
	priv_ret_from_trap
	priv_read_dcache_tag
	priv_read_dcache_data
	load_char_oe
	load_short_oe
	load_word_oe
	return0
	priv_read_icache_tag
	priv_read_icache_data
	ncload_ubyte
	ncload_byte
	ncload_char
	ncload_short
	ncload_word
	iucmp
	priv_powerdown
	cache_invalidate
	ncload_char_oe
	ncload_short_oe
	ncload_word_oe
	return1
	cache_flush
	cache_index_flush
	store_byte
	store_short
	store_word
	soft_trap
	priv_write_dcache_tag
	priv_write_dcache_data
	store_short_oe
	store_word_oe
	return2
	priv_write_icache_tag
	priv_write_icache_data
	ncstore_byte
	ncstore_short
	ncstore_word
	priv_reset
	get_current_class
	ncstore_short_oe
	ncstore_word_oe
	call
	zero_line
	priv_update_optop
	read_pc
	read_vars
	read_frame
	read_optop
	priv_read_oplim
	read_const_pool
	priv_read_psr
	priv_read_trapbase
	priv_read_lockcount0
	priv_read_lockcount1
	priv_read_lockaddr0
	priv_read_lockaddr1
	priv_read_userrange1
	priv_read_gc_config
	priv_read_brk1a
	priv_read_brk2a
	priv_read_brk12c
	priv_read_userrange2
	priv_read_versionid
	priv_read_hcr
	priv_read_sc_bottom
	read_global0
	read_global1
	read_global2
	read_global3
	write_pc
	write_vars
	write_frame
	write_optop
	priv_write_oplim
	write_const_pool
	priv_write_psr
	priv_write_trapbase
	priv_write_lockcount0
	priv_write_lockcount1
	priv_write_lockaddr0
	priv_write_lockaddr1
	priv_write_userrange1
	priv_write_gc_config
	priv_write_brk1a
	priv_write_brk2a
	priv_write_brk12c
	priv_write_userrange2
	priv_write_sc_bottom
	write_global0
	write_global1
	write_global2
	write_global3
	tm_putchar
	tm_exit
	tm_trap
	tm_minfo
