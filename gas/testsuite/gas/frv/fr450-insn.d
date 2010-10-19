#as: -mcpu=fr450
#objdump: -dr

.*:     file format elf32-frv(|fdpic)

Disassembly of section \.text:

00000000 <.*>:
#
.*:	80 0d f8 00 	lrai gr31,gr0,0x0,0x0,0x0
.*:	be 0c 08 00 	lrai gr0,gr31,0x0,0x0,0x0
.*:	80 0c 08 20 	lrai gr0,gr0,0x1,0x0,0x0
.*:	80 0c 08 10 	lrai gr0,gr0,0x0,0x1,0x0
.*:	80 0c 08 08 	lrai gr0,gr0,0x0,0x0,0x1
#
.*:	80 0d f8 40 	lrad gr31,gr0,0x0,0x0,0x0
.*:	be 0c 08 40 	lrad gr0,gr31,0x0,0x0,0x0
.*:	80 0c 08 60 	lrad gr0,gr0,0x1,0x0,0x0
.*:	80 0c 08 50 	lrad gr0,gr0,0x0,0x1,0x0
.*:	80 0c 08 48 	lrad gr0,gr0,0x0,0x0,0x1
#
.*:	80 0d f9 00 	tlbpr gr31,gr0,0x0,0x0
.*:	80 0c 09 1f 	tlbpr gr0,gr31,0x0,0x0
.*:	9c 0c 09 00 	tlbpr gr0,gr0,0x7,0x0
.*:	82 0c 09 00 	tlbpr gr0,gr0,0x0,0x1
#
.*:	81 e1 e4 00 	mqlclrhs fr30,fr0,fr0
.*:	81 e0 04 1e 	mqlclrhs fr0,fr30,fr0
.*:	bd e0 04 00 	mqlclrhs fr0,fr0,fr30
#
.*:	81 e1 e5 00 	mqlmths fr30,fr0,fr0
.*:	81 e0 05 1e 	mqlmths fr0,fr30,fr0
.*:	bd e0 05 00 	mqlmths fr0,fr0,fr30
#
.*:	81 e1 e4 40 	mqsllhi fr30,0x0,fr0
.*:	81 e0 04 7f 	mqsllhi fr0,0x3f,fr0
.*:	bd e0 04 40 	mqsllhi fr0,0x0,fr30
#
.*:	81 e1 e4 c0 	mqsrahi fr30,0x0,fr0
.*:	81 e0 04 ff 	mqsrahi fr0,0x3f,fr0
.*:	bd e0 04 c0 	mqsrahi fr0,0x0,fr30
