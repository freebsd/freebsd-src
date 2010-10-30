
.*:     file format elf.*mips

Disassembly of section .text:

.*0090 <__start>:
.*0090:	64c3      	save	24,ra
.*0092:	1a00 002e 	jal	.*00b8 <x\+0x8>
.*0096:	6500      	nop
.*0098:	1e00 0032 	jalx	.*00c8 <z>
.*009c:	6500      	nop
.*009e:	6443      	restore	24,ra
.*00a0:	e8a0      	jrc	ra
.*00a2:	6500      	nop
.*00a4:	6500      	nop
.*00a6:	6500      	nop
.*00a8:	6500      	nop
.*00aa:	6500      	nop
.*00ac:	6500      	nop
.*00ae:	6500      	nop

.*00b0 <x>:
.*00b0:	e8a0      	jrc	ra
.*00b2:	6500      	nop
.*00b4:	6500      	nop
.*00b6:	6500      	nop
.*00b8:	6500      	nop
.*00ba:	6500      	nop
.*00bc:	6500      	nop
.*00be:	6500      	nop

.*00c0 <y>:
.*00c0:	03e00008 	jr	ra
.*00c4:	00000000 	nop

.*00c8 <z>:
.*00c8:	03e00008 	jr	ra
.*00cc:	00000000 	nop
	\.\.\.
