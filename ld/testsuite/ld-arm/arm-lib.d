
tmpdir/arm-lib.so:     file format elf32-(little|big)arm
architecture: arm, flags 0x00000150:
HAS_SYMS, DYNAMIC, D_PAGED
start address 0x.*

Disassembly of section .plt:

.* <.plt>:
 .*:	e52de004 	push	{lr}		; \(str lr, \[sp, #-4\]!\)
 .*:	e59fe004 	ldr	lr, \[pc, #4\]	; .* <lib_func1-0x10>
 .*:	e08fe00e 	add	lr, pc, lr
 .*:	e5bef008 	ldr	pc, \[lr, #8\]!
 .*:	.*
 .*:	e28fc6.* 	add	ip, pc, #.*	; 0x.*
 .*:	e28cca.* 	add	ip, ip, #.*	; 0x.*
 .*:	e5bcf.* 	ldr	pc, \[ip, #.*\]!
Disassembly of section .text:

.* <lib_func1>:
 .*:	e1a0c00d 	mov	ip, sp
 .*:	e92dd800 	push	{fp, ip, lr, pc}
 .*:	ebfffff9 	bl	.* <lib_func1-0xc>
 .*:	e89d6800 	ldm	sp, {fp, sp, lr}
 .*:	e12fff1e 	bx	lr

.* <lib_func2>:
 .*:	e12fff1e 	bx	lr
