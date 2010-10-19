# Branch instructions.
 .data
foodata: .word 42
	 .text
footext:

# conditional branch instructions.
	.global beq
beq:
beq *+16
beq *+4090
beq *+567890

	.global bne
bne:
bne *-8
bne *+0xf46
bne *-0xf7812

	.global bcs
bcs:
bcs *+250
bcs *-0x2674
bcs *+0x89052

	.global bcc
bcc:
bcc *-250
bcc *+0xfffe
bcc *+0xfffffffe

	.global bhi
bhi:
bhi *+254
bhi *-0xfffe
bhi *-0xfffffffe

	.global bls
bls:
bls *-2
bls *-0x10000
bls *+0x10000

	.global bgt
bgt:
bgt *+060
bgt *+0xffe
bgt *-0x10002

	.global ble
ble:
ble *-0100 
ble *-258
ble *+0xefff2

	.global bfs
bfs:
bfs *+0x2 
bfs *+0177776
bfs *+0200000 

	.global bfc
bfc:
bfc *+0xfe
bfc *+65534
bfc *+0x80000

	.global blo
blo:
blo *-0xfe
blo *-65534
blo *+4294967294

	.global bhs
bhs:
bhs *-0x100
bhs *-0xf000
bhs *+0xff2

	.global blt
blt:
blt *+34
blt *+1234
blt *+037777777776

	.global bge
bge:
bge *+0x34
bge *-0x1234
bge *+1048578

	.global br
br:
br *+034
br *+01234
br *-04000002

# Decrement and Branch instructions.
	.global dbnzb
dbnzb:
dbnzb r0, *+034
dbnzb r1, *+01234568

	.global dbnzw
dbnzw:
dbnzw r2, *+6552
dbnzw r3, *+6553520

	.global dbnzd
dbnzd:
dbnzd ra, *+0xff2
dbnzd sp, *+0x12ffff4

# Branch/Jump and link instructions.

	.global bal
bal:
bal r1, 0x2
bal r1, -0x2
bal r1, 0xabce
bal r0, -0xb4e
bal r1, 0xabcde
bal r1, -0xabcde

	.global jal
jal:
jal ra
jal r1, sp

	.global jalid
jalid:
jalid r12, r14
