
tmpdir/group-relocs:     file format elf32-(little|big)arm

Disassembly of section .text:

00008000 <_start>:
    8000:	e28f00bc 	add	r0, pc, #188	; 0xbc
    8004:	e28f0c6e 	add	r0, pc, #28160	; 0x6e00
    8008:	e28000ec 	add	r0, r0, #236	; 0xec
    800c:	e28f08ff 	add	r0, pc, #16711680	; 0xff0000
    8010:	e2800c6e 	add	r0, r0, #28160	; 0x6e00
    8014:	e28000e4 	add	r0, r0, #228	; 0xe4
    8018:	e2800000 	add	r0, r0, #0	; 0x0
    801c:	e28f0cee 	add	r0, pc, #60928	; 0xee00
    8020:	e28000f0 	add	r0, r0, #240	; 0xf0
    8024:	e28008ff 	add	r0, r0, #16711680	; 0xff0000
    8028:	e2800cee 	add	r0, r0, #60928	; 0xee00
    802c:	e28000f0 	add	r0, r0, #240	; 0xf0
    8030:	e2800c6e 	add	r0, r0, #28160	; 0x6e00
    8034:	e59010c0 	ldr	r1, \[r0, #192\]
    8038:	e28008ff 	add	r0, r0, #16711680	; 0xff0000
    803c:	e2800c6e 	add	r0, r0, #28160	; 0x6e00
    8040:	e59010b8 	ldr	r1, \[r0, #184\]
    8044:	e5901000 	ldr	r1, \[r0\]
    8048:	e2800cee 	add	r0, r0, #60928	; 0xee00
    804c:	e59010f0 	ldr	r1, \[r0, #240\]
    8050:	e28008ff 	add	r0, r0, #16711680	; 0xff0000
    8054:	e2800cee 	add	r0, r0, #60928	; 0xee00
    8058:	e59010f0 	ldr	r1, \[r0, #240\]
    805c:	e1c026d0 	ldrd	r2, \[r0, #96\]
    8060:	e2800c6e 	add	r0, r0, #28160	; 0x6e00
    8064:	e1c029d0 	ldrd	r2, \[r0, #144\]
    8068:	e28008ff 	add	r0, r0, #16711680	; 0xff0000
    806c:	e2800c6e 	add	r0, r0, #28160	; 0x6e00
    8070:	e1c028d8 	ldrd	r2, \[r0, #136\]
    8074:	e1c020d0 	ldrd	r2, \[r0\]
    8078:	e2800cee 	add	r0, r0, #60928	; 0xee00
    807c:	e1c02fd0 	ldrd	r2, \[r0, #240\]
    8080:	e28008ff 	add	r0, r0, #16711680	; 0xff0000
    8084:	e2800cee 	add	r0, r0, #60928	; 0xee00
    8088:	e1c02fd0 	ldrd	r2, \[r0, #240\]
    808c:	ed90000c 	ldc	0, cr0, \[r0, #48\]
    8090:	e2800c6e 	add	r0, r0, #28160	; 0x6e00
    8094:	ed900018 	ldc	0, cr0, \[r0, #96\]
    8098:	e28008ff 	add	r0, r0, #16711680	; 0xff0000
    809c:	e2800c6e 	add	r0, r0, #28160	; 0x6e00
    80a0:	ed900016 	ldc	0, cr0, \[r0, #88\]
    80a4:	ed900000 	ldc	0, cr0, \[r0\]
    80a8:	e2800cee 	add	r0, r0, #60928	; 0xee00
    80ac:	ed90003c 	ldc	0, cr0, \[r0, #240\]
    80b0:	e28008ff 	add	r0, r0, #16711680	; 0xff0000
    80b4:	e2800cee 	add	r0, r0, #60928	; 0xee00
    80b8:	ed90003c 	ldc	0, cr0, \[r0, #240\]

000080bc <one_group_needed_alu_pc>:
    80bc:	e3a00000 	mov	r0, #0	; 0x0
Disassembly of section zero:

00000000 <one_group_needed_alu_sb>:
   0:	e3a00000 	mov	r0, #0	; 0x0
Disassembly of section alpha:

0000eef0 <two_groups_needed_alu_pc>:
    eef0:	e3a00000 	mov	r0, #0	; 0x0
Disassembly of section beta:

00ffeef0 <three_groups_needed_alu_pc>:
  ffeef0:	e3a00000 	mov	r0, #0	; 0x0
#...
