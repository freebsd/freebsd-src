#objdump: -Dr

.* elf32-frv(|fdpic)

Disassembly.*\.text:

.* <begin>:
.*:	80 88 00 00 *	nop
.*:	80 88 00 00 *	nop
.* <f1>:
.*:	80 88 00 00 *	nop
.*:	80 88 00 00 *	nop
.*:	80 88 00 00 *	nop
.* <f2>:
.*:	80 3c 00 00 *	call.*
			.*: R_FRV_LABEL24	f1
.*:	c0 1a 00 00 *	bra.*
			.*: R_FRV_LABEL16	f1
.*:	fe 3f ff fe *	call .* <f2>
.*:	c0 1a ff fd *	bra .* <f2>
.*:	80 3c 00 00 *	call.*
			.*: R_FRV_LABEL24	f3
.*:	c0 1a 00 00 *	bra.*
			.*: R_FRV_LABEL16	f3
	\.\.\.
Disassembly.*\.test:
.* <beginx>:
.*:	80 88 00 00 *	nop
.*:	80 3c 00 00 *	call.*
			.*: R_FRV_LABEL24	f1
.*:	c0 1a 00 00 *	bra.*
			.*: R_FRV_LABEL16	f1
.*:	80 3c 00 05 *	call .* <f2\+.*>
			.*: R_FRV_LABEL24	\.text\+0x14
.*:	c0 1a 00 05 *	bra .* <f2\+.*>
			.*: R_FRV_LABEL16	\.text\+0x14
.*:	80 3c 00 00 *	call.*
			.*: R_FRV_LABEL24	f3
.*:	c0 1a 00 00 *	bra.*
			.*: R_FRV_LABEL16	f3
