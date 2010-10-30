
.*:     file format.*

Disassembly of section .text:

00008000 <_start>:
    8000:	eb00000d 	bl	803c <arm>
    8004:	fa00000d 	blx	8040 <t1>
    8008:	fb00000c 	blx	8042 <t2>
    800c:	fb00000d 	blx	804a <t5>
    8010:	fa00000a 	blx	8040 <t1>
    8014:	fb000009 	blx	8042 <t2>
    8018:	ea00000f 	b	805c <__t1_from_arm>
    801c:	ea000010 	b	8064 <__t2_from_arm>
    8020:	1b00000d 	blne	805c <__t1_from_arm>
    8024:	1b00000e 	blne	8064 <__t2_from_arm>
    8028:	1b000003 	blne	803c <arm>
    802c:	eb000002 	bl	803c <arm>
    8030:	faffffff 	blx	8034 <thumblocal>

00008034 <thumblocal>:
    8034:	4770      	bx	lr

00008036 <t3>:
    8036:	4770      	bx	lr

00008038 <t4>:
    8038:	4770      	bx	lr
    803a:	46c0      	nop			\(mov r8, r8\)

0000803c <arm>:
    803c:	e12fff1e 	bx	lr

00008040 <t1>:
    8040:	4770      	bx	lr

00008042 <t2>:
    8042:	f7ff fff8 	bl	8036 <t3>
    8046:	f7ff fff7 	bl	8038 <t4>

0000804a <t5>:
    804a:	f000 f801 	bl	8050 <local_thumb>
    804e:	46c0      	nop			\(mov r8, r8\)

00008050 <local_thumb>:
    8050:	f7ff fff1 	bl	8036 <t3>
    8054:	f7ff efd4 	blx	8000 <_start>
    8058:	f7ff efd2 	blx	8000 <_start>

0000805c <__t1_from_arm>:
    805c:	e51ff004 	ldr	pc, \[pc, #-4\]	; 8060 <__t1_from_arm\+0x4>
    8060:	00008041 	.word	0x00008041

00008064 <__t2_from_arm>:
    8064:	e51ff004 	ldr	pc, \[pc, #-4\]	; 8068 <__t2_from_arm\+0x4>
    8068:	00008043 	.word	0x00008043
