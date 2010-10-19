# Miscellaneous instructions.
 .data
foodata: .word 42
	 .text
footext:

# Multiply instructions.
	.global macsb
macsb:
macsb r0 , r1

	.global macub
macub:
macub r2 , r3

	.global macqb
macqb:
macqb r4 , r5

	.global macsw
macsw:
macsw r6 , r7

	.global macuw
macuw:
macuw r8 , r9

	.global macqw
macqw:
macqw r10 , r11

	.global macsd
macsd:
macsd r12 , r13

	.global macud
macud:
macud r14 , r15

	.global macqd
macqd:
macqd ra , sp

	.global mullsd
mullsd:
mullsd r0 , r2

	.global mullud
mullud:
mullud r1 , r3

	.global mulsbw
mulsbw:
mulsbw r4 , r6

	.global mulubw
mulubw:
mulubw r5 , r7

	.global mulswd
mulswd:
mulswd r8 , r10

	.global muluwd
muluwd:
muluwd r9 , r11

# Signextend instructions.
	.global sextbw
sextbw:
sextbw r12 , ra

	.global sextbd
sextbd:
sextbd r13 , sp

	.global sextwd
sextwd:
sextwd r14 , r15

	.global zextbw
zextbw:
zextbw r5 , r0

	.global zextbd
zextbd:
zextbd r10 , r6

	.global zextwd
zextwd:
zextwd r7 , r15

# Misc. instructions.

	.global getrfid
getrfid:
getrfid r14

	.global setrfid
setrfid:
setrfid sp

	.global bswap
bswap:
bswap r14 , r2

	.global maxsb
maxsb:
maxsb r8 , r3

	.global minsb
minsb:
minsb r15 , r14

	.global maxub
maxub:
maxub r13 , r12

	.global minub
minub:
minub r11 , r10

	.global absb
absb:
absb r9 , r8

	.global negb
negb:
negb r7 , r6

	.global cntl0b
cntl0b:
cntl0b r5 , r4

	.global cntl1b
cntl1b:
cntl1b r3 , r2

	.global popcntb
popcntb:
popcntb r1 , r0

	.global rotlb
rotlb:
rotlb r11 , r4

	.global rotrb
rotrb:
rotrb r7 , r2

	.global mulqb
mulqb:
mulqb r14 , ra

	.global addqb
addqb:
addqb r15 , sp

	.global subqb
subqb:
subqb r0 , r10

	.global cntlsb
cntlsb:
cntlsb r2 , r12

	.global maxsw
maxsw:
maxsw r8 , r3

	.global minsw
minsw:
minsw r15 , r14

	.global maxuw
maxuw:
maxuw r13 , r12

	.global minuw
minuw:
minuw r11 , r10

	.global absw
absw:
absw r9 , r8

	.global negw
negw:
negw r7 , r6

	.global cntl0w
cntl0w:
cntl0w r5 , r4

	.global cntl1w
cntl1w:
cntl1w r3 , r2

	.global popcntw
popcntw:
popcntw r1 , r0

	.global rotlw
rotlw:
rotlw r11 , r4

	.global rotrw
rotrw:
rotrw r7 , r2

	.global mulqw
mulqw:
mulqw r14 , ra

	.global addqw
addqw:
addqw r15 , sp

	.global subqw
subqw:
subqw r0 , r10

	.global cntlsw
cntlsw:
cntlsw r2 , r12

	.global maxsd
maxsd:
maxsd r8 , r3

	.global minsd
minsd:
minsd r15 , r14

	.global maxud
maxud:
maxud r13 , r12

	.global minud
minud:
minud r11 , r10

	.global absd
absd:
absd r9 , r8

	.global negd
negd:
negd r7 , r6

	.global cntl0d
cntl0d:
cntl0d r5 , r4

	.global cntl1d
cntl1d:
cntl1d r3 , r2

	.global popcntd
popcntd:
popcntd r1 , r0

	.global rotld
rotld:
rotld r11 , r4

	.global rotrd
rotrd:
rotrd r7 , r2

	.global mulqd
mulqd:
mulqd r14 , ra

	.global addqd
addqd:
addqd r15 , sp

	.global subqd
subqd:
subqd r0 , r10

	.global cntlsd
cntlsd:
cntlsd r2 , r12

	.global excp
excp:
excp BPT
excp svc

	.global ram
ram:
ram $24, $9, $1, ra, r12

	.global rim
rim:
rim $0x1f, $0xf, $0xe, r2, r1

	.global rotb
rotb:
rotb $7, r1

	.global rotw
rotw:
rotw $13, r3

	.global rotd
rotd:
rotd $27, r2


