#objdump: -dr --prefix-addresses --show-raw-insn
#name: SH PIC constructs
# Test SH PIC constructs:

.*:     file format elf.*sh.*

Disassembly of section \.text:
0x00000000 c7 0a       	mova	0x0000002c,r0
0x00000002 dc 0a       	mov\.l	0x0000002c,r12	! 0
0x00000004 3c 0c       	add	r0,r12
0x00000006 d0 0a       	mov\.l	0x00000030,r0	! 0
0x00000008 00 ce       	mov\.l	@\(r0,r12\),r0
0x0000000a 40 0b       	jsr	@r0
0x0000000c 00 09       	nop	
0x0000000e d0 09       	mov\.l	0x00000034,r0	! 0
0x00000010 30 cc       	add	r12,r0
0x00000012 40 0b       	jsr	@r0
0x00000014 00 09       	nop	
0x00000016 d1 08       	mov\.l	0x00000038,r1	! 0
0x00000018 c7 07       	mova	0x00000038,r0
0x0000001a 30 1c       	add	r1,r0
0x0000001c 40 0b       	jsr	@r0
0x0000001e 00 09       	nop	
0x00000020 d0 06       	mov\.l	0x0000003c,r0	! 16
0x00000022 40 0b       	jsr	@r0
0x00000024 00 09       	nop	
0x00000026 d0 06       	mov\.l	0x00000040,r0	! 14
0x00000028 40 0b       	jsr	@r0
0x0000002a 00 09       	nop	
	\.\.\.
			2c: R_SH_DIR32	GLOBAL_OFFSET_TABLE
			30: R_SH_GOT32	foo
			34: R_SH_GOTOFF	foo
			38: R_SH_PLT32	foo
0x0000003c 00 00       	\.word 0x0000
			3c: R_SH_PLT32	foo
0x0000003e 00 16       	mov\.l	r1,@\(r0,r0\)
0x00000040 00 00       	\.word 0x0000
			40: R_SH_PLT32	foo
0x00000042 00 14       	mov\.b	r1,@\(r0,r0\)
0x00000044 00 00       	\.word 0x0000
			44: R_SH_PLT32	foo
0x00000046 00 1e       	mov\.l	@\(r0,r1\),r0
