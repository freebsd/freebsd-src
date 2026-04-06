	.file	"kernel.c"
	.option nopic
	.attribute arch, "rv64i2p1_m2p0_a2p1_c2p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
.Ltext0:
	.cfi_sections	.debug_frame
	.file 0 "/workspaces/freebsd-src/mobile/kernel" "kernel.c"
	.align	1
	.globl	uart_putc
	.type	uart_putc, @function
uart_putc:
.LFB0:
	.file 1 "kernel.c"
	.loc 1 6 24
	.cfi_startproc
.LVL0:
	.loc 1 7 5
	.loc 1 8 5
	.loc 1 8 13 is_stmt 0 discriminator 1
	li	a4,268435456
	addi	a4,a4,24
.L2:
	.loc 1 8 12 is_stmt 1 discriminator 1
	.loc 1 8 13 is_stmt 0 discriminator 1
	lw	a5,0(a4)
	.loc 1 8 12 discriminator 1
	andi	a5,a5,32
	bne	a5,zero,.L2
	.loc 1 9 5 is_stmt 1
	.loc 1 9 11 is_stmt 0
	li	a5,268435456
	sb	a0,0(a5)
	.loc 1 10 1
	ret
	.cfi_endproc
.LFE0:
	.size	uart_putc, .-uart_putc
	.align	1
	.globl	uart_puts
	.type	uart_puts, @function
uart_puts:
.LFB1:
	.loc 1 12 31 is_stmt 1
	.cfi_startproc
.LVL1:
	.loc 1 13 5
	.loc 1 13 12
	lbu	a3,0(a0)
	beq	a3,zero,.L5
.LBB6:
.LBB7:
	.loc 1 8 13 is_stmt 0 discriminator 1
	li	a4,268435456
	li	a2,268435456
	addi	a4,a4,24
.L8:
.LBE7:
.LBE6:
	.loc 1 14 9 is_stmt 1
	.loc 1 14 21 is_stmt 0
	addi	a0,a0,1
.LVL2:
.LBB9:
.LBB8:
	.loc 1 7 5 is_stmt 1
	.loc 1 8 5
.L7:
	.loc 1 8 12 discriminator 1
	.loc 1 8 13 is_stmt 0 discriminator 1
	lw	a5,0(a4)
	.loc 1 8 12 discriminator 1
	andi	a5,a5,32
	bne	a5,zero,.L7
	.loc 1 9 5 is_stmt 1
	.loc 1 9 11 is_stmt 0
	sb	a3,0(a2)
.LVL3:
.LBE8:
.LBE9:
	.loc 1 13 12 is_stmt 1
	lbu	a3,0(a0)
	bne	a3,zero,.L8
.LVL4:
.L5:
	.loc 1 16 1 is_stmt 0
	ret
	.cfi_endproc
.LFE1:
	.size	uart_puts, .-uart_puts
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	3
.LC0:
	.string	"uOS(m) - User OS Mobile booting...\n"
	.align	3
.LC1:
	.string	"Hart ID: "
	.align	3
.LC2:
	.string	"\n"
	.align	3
.LC3:
	.string	"Hybrid kernel initialized\n"
	.align	3
.LC4:
	.string	"POSIX API ready\n"
	.text
	.align	1
	.globl	kernel_init
	.type	kernel_init, @function
kernel_init:
.LFB2:
	.loc 1 18 51 is_stmt 1
	.cfi_startproc
.LVL5:
	.loc 1 20 5
	.loc 1 18 51 is_stmt 0
	addi	sp,sp,-16
	.cfi_def_cfa_offset 16
	sd	s0,0(sp)
	.cfi_offset 8, -16
	mv	s0,a0
	.loc 1 20 5
	lla	a0,.LC0
.LVL6:
	.loc 1 18 51
	sd	ra,8(sp)
	.cfi_offset 1, -8
	.loc 1 20 5
	call	uart_puts
.LVL7:
	.loc 1 21 5 is_stmt 1
	lla	a0,.LC1
	call	uart_puts
.LVL8:
	.loc 1 22 5
	addiw	a3,s0,48
.LBB10:
.LBB11:
	.loc 1 8 13 is_stmt 0 discriminator 1
	li	a4,268435456
.LBE11:
.LBE10:
	.loc 1 22 5
	andi	a3,a3,0xff
.LVL9:
.LBB13:
.LBB12:
	.loc 1 7 5 is_stmt 1
	.loc 1 8 5
	.loc 1 8 13 is_stmt 0 discriminator 1
	addi	a4,a4,24
.L15:
	.loc 1 8 12 is_stmt 1 discriminator 1
	.loc 1 8 13 is_stmt 0 discriminator 1
	lw	a5,0(a4)
	.loc 1 8 12 discriminator 1
	andi	a5,a5,32
	bne	a5,zero,.L15
	.loc 1 9 5 is_stmt 1
	.loc 1 9 11 is_stmt 0
	li	a5,268435456
	sb	a3,0(a5)
.LVL10:
.LBE12:
.LBE13:
	.loc 1 23 5 is_stmt 1
	lla	a0,.LC2
	call	uart_puts
.LVL11:
	.loc 1 24 5
	lla	a0,.LC3
	call	uart_puts
.LVL12:
	.loc 1 25 5
	lla	a0,.LC4
	call	uart_puts
.LVL13:
.L16:
	.loc 1 28 5
	.loc 1 29 9 discriminator 1
 #APP
# 29 "kernel.c" 1
	wfi
# 0 "" 2
	.loc 1 28 11
	.loc 1 28 5
	.loc 1 29 9 discriminator 1
# 29 "kernel.c" 1
	wfi
# 0 "" 2
	.loc 1 28 11
 #NO_APP
	j	.L16
	.cfi_endproc
.LFE2:
	.size	kernel_init, .-kernel_init
	.globl	bss_end
	.globl	bss_start
	.bss
	.align	3
	.type	bss_end, @object
	.size	bss_end, 0
bss_end:
	.type	bss_start, @object
	.size	bss_start, 0
bss_start:
	.text
.Letext0:
	.section	.debug_info,"",@progbits
.Ldebug_info0:
	.4byte	0x234
	.2byte	0x5
	.byte	0x1
	.byte	0x8
	.4byte	.Ldebug_abbrev0
	.uleb128 0xd
	.4byte	.LASF9
	.byte	0x1d
	.4byte	.LASF0
	.4byte	.LASF1
	.8byte	.Ltext0
	.8byte	.Letext0-.Ltext0
	.4byte	.Ldebug_line0
	.uleb128 0x3
	.4byte	0x44
	.4byte	0x3d
	.uleb128 0x4
	.4byte	0x3d
	.byte	0
	.uleb128 0x5
	.byte	0x8
	.byte	0x7
	.4byte	.LASF2
	.uleb128 0x5
	.byte	0x1
	.byte	0x8
	.4byte	.LASF3
	.uleb128 0xe
	.4byte	0x44
	.uleb128 0xf
	.4byte	0x44
	.uleb128 0x6
	.4byte	.LASF4
	.byte	0x22
	.4byte	0x2e
	.uleb128 0x9
	.byte	0x3
	.8byte	bss_start
	.uleb128 0x3
	.4byte	0x44
	.4byte	0x78
	.uleb128 0x4
	.4byte	0x3d
	.byte	0
	.uleb128 0x6
	.4byte	.LASF5
	.byte	0x23
	.4byte	0x69
	.uleb128 0x9
	.byte	0x3
	.8byte	bss_end
	.uleb128 0x7
	.4byte	.LASF7
	.byte	0x12
	.8byte	.LFB2
	.8byte	.LFE2-.LFB2
	.uleb128 0x1
	.byte	0x9c
	.4byte	0x18c
	.uleb128 0x10
	.4byte	.LASF6
	.byte	0x1
	.byte	0x12
	.byte	0x20
	.4byte	0x3d
	.4byte	.LLST4
	.uleb128 0x8
	.string	"dtb"
	.byte	0x12
	.byte	0x2e
	.4byte	0x18c
	.4byte	.LLST5
	.uleb128 0x11
	.4byte	0x1ea
	.8byte	.LBB10
	.4byte	.LLRL6
	.byte	0x1
	.byte	0x16
	.byte	0x5
	.4byte	0xf4
	.uleb128 0x9
	.4byte	0x1f7
	.4byte	.LLST7
	.uleb128 0xa
	.4byte	.LLRL6
	.uleb128 0xb
	.4byte	0x201
	.byte	0
	.byte	0
	.uleb128 0x2
	.8byte	.LVL7
	.4byte	0x18e
	.4byte	0x113
	.uleb128 0x1
	.uleb128 0x1
	.byte	0x5a
	.uleb128 0x9
	.byte	0x3
	.8byte	.LC0
	.byte	0
	.uleb128 0x2
	.8byte	.LVL8
	.4byte	0x18e
	.4byte	0x132
	.uleb128 0x1
	.uleb128 0x1
	.byte	0x5a
	.uleb128 0x9
	.byte	0x3
	.8byte	.LC1
	.byte	0
	.uleb128 0x2
	.8byte	.LVL11
	.4byte	0x18e
	.4byte	0x151
	.uleb128 0x1
	.uleb128 0x1
	.byte	0x5a
	.uleb128 0x9
	.byte	0x3
	.8byte	.LC2
	.byte	0
	.uleb128 0x2
	.8byte	.LVL12
	.4byte	0x18e
	.4byte	0x170
	.uleb128 0x1
	.uleb128 0x1
	.byte	0x5a
	.uleb128 0x9
	.byte	0x3
	.8byte	.LC3
	.byte	0
	.uleb128 0x12
	.8byte	.LVL13
	.4byte	0x18e
	.uleb128 0x1
	.uleb128 0x1
	.byte	0x5a
	.uleb128 0x9
	.byte	0x3
	.8byte	.LC4
	.byte	0
	.byte	0
	.uleb128 0x13
	.byte	0x8
	.uleb128 0x7
	.4byte	.LASF8
	.byte	0xc
	.8byte	.LFB1
	.8byte	.LFE1-.LFB1
	.uleb128 0x1
	.byte	0x9c
	.4byte	0x1e5
	.uleb128 0x8
	.string	"s"
	.byte	0xc
	.byte	0x1c
	.4byte	0x1e5
	.4byte	.LLST0
	.uleb128 0x14
	.4byte	0x1ea
	.8byte	.LBB6
	.4byte	.LLRL1
	.byte	0x1
	.byte	0xe
	.byte	0x9
	.uleb128 0x9
	.4byte	0x1f7
	.4byte	.LLST2
	.uleb128 0xa
	.4byte	.LLRL1
	.uleb128 0x15
	.4byte	0x201
	.4byte	.LLST3
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0xc
	.4byte	0x4b
	.uleb128 0x16
	.4byte	.LASF10
	.byte	0x1
	.byte	0x6
	.byte	0x6
	.byte	0x1
	.4byte	0x20e
	.uleb128 0x17
	.string	"c"
	.byte	0x1
	.byte	0x6
	.byte	0x15
	.4byte	0x44
	.uleb128 0x18
	.4byte	.LASF11
	.byte	0x1
	.byte	0x7
	.byte	0x14
	.4byte	0x20e
	.byte	0
	.uleb128 0xc
	.4byte	0x50
	.uleb128 0x19
	.4byte	0x1ea
	.8byte	.LFB0
	.8byte	.LFE0-.LFB0
	.uleb128 0x1
	.byte	0x9c
	.uleb128 0x1a
	.4byte	0x1f7
	.uleb128 0x1
	.byte	0x5a
	.uleb128 0xb
	.4byte	0x201
	.byte	0
	.byte	0
	.section	.debug_abbrev,"",@progbits
.Ldebug_abbrev0:
	.uleb128 0x1
	.uleb128 0x49
	.byte	0
	.uleb128 0x2
	.uleb128 0x18
	.uleb128 0x7e
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0x2
	.uleb128 0x48
	.byte	0x1
	.uleb128 0x7d
	.uleb128 0x1
	.uleb128 0x7f
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x3
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x4
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x37
	.uleb128 0x21
	.sleb128 0
	.byte	0
	.byte	0
	.uleb128 0x5
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.byte	0
	.byte	0
	.uleb128 0x6
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 1
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0x21
	.sleb128 6
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 1
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0x21
	.sleb128 6
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x7a
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 1
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0x5
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0xb
	.byte	0x1
	.uleb128 0x55
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0xb
	.uleb128 0x34
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x1c
	.uleb128 0x21
	.sleb128 268435456
	.byte	0
	.byte	0
	.uleb128 0xc
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0x21
	.sleb128 8
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x1f
	.uleb128 0x1b
	.uleb128 0x1f
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x10
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xf
	.uleb128 0x35
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x10
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0x1d
	.byte	0x1
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x52
	.uleb128 0x1
	.uleb128 0x55
	.uleb128 0x17
	.uleb128 0x58
	.uleb128 0xb
	.uleb128 0x59
	.uleb128 0xb
	.uleb128 0x57
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x12
	.uleb128 0x48
	.byte	0x1
	.uleb128 0x7d
	.uleb128 0x1
	.uleb128 0x7f
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x13
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x14
	.uleb128 0x1d
	.byte	0x1
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x52
	.uleb128 0x1
	.uleb128 0x55
	.uleb128 0x17
	.uleb128 0x58
	.uleb128 0xb
	.uleb128 0x59
	.uleb128 0xb
	.uleb128 0x57
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x15
	.uleb128 0x34
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x16
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x20
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x17
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x18
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x19
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x7a
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x1a
	.uleb128 0x5
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_loclists,"",@progbits
	.4byte	.Ldebug_loc3-.Ldebug_loc2
.Ldebug_loc2:
	.2byte	0x5
	.byte	0x8
	.byte	0
	.4byte	0
.Ldebug_loc0:
.LLST4:
	.byte	0x4
	.uleb128 .LVL5-.Ltext0
	.uleb128 .LVL6-.Ltext0
	.uleb128 0x1
	.byte	0x5a
	.byte	0x4
	.uleb128 .LVL6-.Ltext0
	.uleb128 .LFE2-.Ltext0
	.uleb128 0x1
	.byte	0x58
	.byte	0
.LLST5:
	.byte	0x4
	.uleb128 .LVL5-.Ltext0
	.uleb128 .LVL7-1-.Ltext0
	.uleb128 0x1
	.byte	0x5b
	.byte	0x4
	.uleb128 .LVL7-1-.Ltext0
	.uleb128 .LFE2-.Ltext0
	.uleb128 0x4
	.byte	0xa3
	.uleb128 0x1
	.byte	0x5b
	.byte	0x9f
	.byte	0
.LLST7:
	.byte	0x4
	.uleb128 .LVL9-.Ltext0
	.uleb128 .LVL10-.Ltext0
	.uleb128 0x1
	.byte	0x5d
	.byte	0
.LLST0:
	.byte	0x4
	.uleb128 .LVL1-.Ltext0
	.uleb128 .LVL2-.Ltext0
	.uleb128 0x1
	.byte	0x5a
	.byte	0x4
	.uleb128 .LVL2-.Ltext0
	.uleb128 .LFE1-.Ltext0
	.uleb128 0x1
	.byte	0x5a
	.byte	0
.LLST2:
	.byte	0x4
	.uleb128 .LVL2-.Ltext0
	.uleb128 .LVL3-.Ltext0
	.uleb128 0x1
	.byte	0x5d
	.byte	0
.LLST3:
	.byte	0x4
	.uleb128 .LVL2-.Ltext0
	.uleb128 .LVL4-.Ltext0
	.uleb128 0x4
	.byte	0x40
	.byte	0x48
	.byte	0x24
	.byte	0x9f
	.byte	0
.Ldebug_loc3:
	.section	.debug_aranges,"",@progbits
	.4byte	0x2c
	.2byte	0x2
	.4byte	.Ldebug_info0
	.byte	0x8
	.byte	0
	.2byte	0
	.2byte	0
	.8byte	.Ltext0
	.8byte	.Letext0-.Ltext0
	.8byte	0
	.8byte	0
	.section	.debug_rnglists,"",@progbits
.Ldebug_ranges0:
	.4byte	.Ldebug_ranges3-.Ldebug_ranges2
.Ldebug_ranges2:
	.2byte	0x5
	.byte	0x8
	.byte	0
	.4byte	0
.LLRL1:
	.byte	0x4
	.uleb128 .LBB6-.Ltext0
	.uleb128 .LBE6-.Ltext0
	.byte	0x4
	.uleb128 .LBB9-.Ltext0
	.uleb128 .LBE9-.Ltext0
	.byte	0
.LLRL6:
	.byte	0x4
	.uleb128 .LBB10-.Ltext0
	.uleb128 .LBE10-.Ltext0
	.byte	0x4
	.uleb128 .LBB13-.Ltext0
	.uleb128 .LBE13-.Ltext0
	.byte	0
.Ldebug_ranges3:
	.section	.debug_line,"",@progbits
.Ldebug_line0:
	.section	.debug_str,"MS",@progbits,1
.LASF5:
	.string	"bss_end"
.LASF2:
	.string	"long unsigned int"
.LASF9:
	.string	"GNU C17 13.2.0 -mabi=lp64 -mcmodel=medany -misa-spec=20191213 -march=rv64imac -g -O2 -ffreestanding"
.LASF6:
	.string	"hartid"
.LASF10:
	.string	"uart_putc"
.LASF11:
	.string	"uart"
.LASF7:
	.string	"kernel_init"
.LASF4:
	.string	"bss_start"
.LASF3:
	.string	"char"
.LASF8:
	.string	"uart_puts"
	.section	.debug_line_str,"MS",@progbits,1
.LASF1:
	.string	"/workspaces/freebsd-src/mobile/kernel"
.LASF0:
	.string	"kernel.c"
	.ident	"GCC: (13.2.0-11ubuntu1+12) 13.2.0"
