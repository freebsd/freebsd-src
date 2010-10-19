 .data
foodata: .word 42
 .text
footext:
	.text
	.global add
add:
	ADD sp,sp,sp
	.text
	.global sub
sub:
	sub sp,sp,sp
	.text
	.global udiv
udiv:
	udiv sp,sp,sp
	.text
	.global and
and:
	and sp,sp,sp
	.text
	.global or
or:
	or sp,sp,sp
	.text
	.global xor
xor:
	xor sp,sp,sp
	.text
	.global not
not:
	not sp,sp
	.text
	.global sdiv
sdiv:
	sdiv sp,sp,sp
	.text
	.global nsdiv
nsdiv:
	nsdiv sp,sp,sp
	.text
	.global nudiv
nudiv:
	nudiv sp,sp,sp
	.text
	.global smul
smul:
	smul fp,fp,fp
	.text
	.global umul
umul:
	umul fp,fp,fp
	.text
	.global sll
sll:
	sll sp,sp,sp
	.text
	.global srl
srl:
	srl sp,sp,sp
	.text
	.global sra
sra:
	sra sp,sp,sp
	.text
	.global scan
scan:
	scan sp,sp,sp
	.text
	.global cadd
cadd:
	cadd sp,sp,sp,cc0,0
	.text
	.global csub
csub:
	csub sp,sp,sp,cc0,0
	.text
	.global cudiv
cudiv:
	cudiv sp,sp,sp,cc0,0
	.text
	.global cand
cand:
	cand sp,sp,sp,cc0,0
	.text
	.global cor
cor:
	cor sp,sp,sp,cc0,0
	.text
	.global cxor
cxor:
	cxor sp,sp,sp,cc0,0
	.text
	.global cnot
cnot:
	cnot sp,sp,cc0,0
	.text
	.global csmul
csmul:
	csmul fp,fp,fp,cc0,0
	.text
	.global csdiv
csdiv:
	csdiv sp,sp,sp,cc0,0
	.text
	.global csll
csll:
	csll sp,sp,sp,cc0,0
	.text
	.global csrl
csrl:
	csrl sp,sp,sp,cc0,0
	.text
	.global csra
csra:
	csra sp,sp,sp,cc0,0
	.text
	.global cscan
cscan:
	cscan sp,sp,sp,cc0,0
	.text
	.global addcc
addcc:
	addcc sp,sp,sp,icc0
	.text
	.global subcc
subcc:
	subcc sp,sp,sp,icc0
	.text
	.global andcc
andcc:
	andcc sp,sp,sp,icc0
	.text
	.global orcc
orcc:
	orcc sp,sp,sp,icc0
	.text
	.global xorcc
xorcc:
	xorcc sp,sp,sp,icc0
	.text
	.global sllcc
sllcc:
	sllcc sp,sp,sp,icc0
	.text
	.global srlcc
srlcc:
	srlcc sp,sp,sp,icc0
	.text
	.global sracc
sracc:
	sracc sp,sp,sp,icc0
	.text
	.global smulcc
smulcc:
	smulcc fp,fp,fp,icc0
	.text
	.global umulcc
umulcc:
	umulcc fp,fp,fp,icc0
	.text
	.global caddcc
caddcc:
	caddcc sp,sp,sp,cc0,0
	.text
	.global csubcc
csubcc:
	csubcc sp,sp,sp,cc0,0
	.text
	.global csmulcc
csmulcc:
	csmulcc fp,fp,fp,cc0,0
	.text
	.global candcc
candcc:
	candcc sp,sp,sp,cc0,0
	.text
	.global corcc
corcc:
	corcc sp,sp,sp,cc0,0
	.text
	.global cxorcc
cxorcc:
	cxorcc sp,sp,sp,cc0,0
	.text
	.global csllcc
csllcc:
	csllcc sp,sp,sp,cc0,0
	.text
	.global csrlcc
csrlcc:
	csrlcc sp,sp,sp,cc0,0
	.text
	.global csracc
csracc:
	csracc sp,sp,sp,cc0,0
	.text
	.global addx
addx:
	addx sp,sp,sp,icc0
	.text
	.global subx
subx:
	subx sp,sp,sp,icc0
	.text
	.global addxcc
addxcc:
	addxcc sp,sp,sp,icc0
	.text
	.global subxcc
subxcc:
	subxcc sp,sp,sp,icc0
	.text
	.global addi
addi:
	addi sp,0,sp
	.text
	.global subi
subi:
	subi sp,0,sp
	.text
	.global udivi
udivi:
	udivi sp,0,sp
	.text
	.global andi
andi:
	andi sp,0,sp
	.text
	.global ori
ori:
	ori sp,0,sp
	.text
	.global xori
xori:
	xori sp,0,sp
	.text
	.global sdivi
sdivi:
	sdivi sp,0,sp
	.text
	.global nsdivi
nsdivi:
	nsdivi sp,0,sp
	.text
	.global nudivi
nudivi:
	nudivi sp,0,sp
	.text
	.global smuli
smuli:
	smuli fp,0,fp
	.text
	.global umuli
umuli:
	umuli fp,0,fp
	.text
	.global slli
slli:
	slli sp,0,sp
	.text
	.global srli
srli:
	srli sp,0,sp
	.text
	.global srai
srai:
	srai sp,0,sp
	.text
	.global scani
scani:
	scani sp,0,sp
	.text
	.global addicc
addicc:
	addicc sp,0,sp,icc0
	.text
	.global subicc
subicc:
	subicc sp,0,sp,icc0
	.text
	.global andicc
andicc:
	andicc sp,0,sp,icc0
	.text
	.global oricc
oricc:
	oricc sp,0,sp,icc0
	.text
	.global xoricc
xoricc:
	xoricc sp,0,sp,icc0
	.text
	.global smulicc
smulicc:
	smulicc fp,0,fp,icc0
	.text
	.global umulicc
umulicc:
	umulicc fp,0,fp,icc0
	.text
	.global sllicc
sllicc:
	sllicc sp,0,sp,icc0
	.text
	.global srlicc
srlicc:
	srlicc sp,0,sp,icc0
	.text
	.global sraicc
sraicc:
	sraicc sp,0,sp,icc0
	.text
	.global addxi
addxi:
	addxi sp,0,sp,icc0
	.text
	.global subxi
subxi:
	subxi sp,0,sp,icc0
	.text
	.global addxicc
addxicc:
	addxicc sp,0,sp,icc0
	.text
	.global subxicc
subxicc:
	subxicc sp,0,sp,icc0
	.text
	.global setlo
setlo:
	setlo 0,sp
	.text
	.global sethi
sethi:
	sethi 0,sp
	.text
	.global setlos
setlos:
	setlos 0,sp
	.text
	.global ldsb
ldsb:
	ldsb @(sp,sp),sp
	.text
	.global ldub
ldub:
	ldub @(sp,sp),sp
	.text
	.global ldsh
ldsh:
	ldsh @(sp,sp),sp
	.text
	.global lduh
lduh:
	lduh @(sp,sp),sp
	.text
	.global ld
ld:
	ld @(sp,sp),sp
	.text
	.global ldbf
ldbf:
	ldbf @(sp,sp),fr0
	.text
	.global ldhf
ldhf:
	ldhf @(sp,sp),fr0
	.text
	.global ldf
ldf:
	ldf @(sp,sp),fr0
	.text
	.global ldc
ldc:
	ldc @(sp,sp),cpr0
	.text
	.global nldsb
nldsb:
	nldsb @(sp,sp),sp
	.text
	.global nldub
nldub:
	nldub @(sp,sp),sp
	.text
	.global nldsh
nldsh:
	nldsh @(sp,sp),sp
	.text
	.global nlduh
nlduh:
	nlduh @(sp,sp),sp
	.text
	.global nld
nld:
	nld @(sp,sp),sp
	.text
	.global nldbf
nldbf:
	nldbf @(sp,sp),fr0
	.text
	.global nldhf
nldhf:
	nldhf @(sp,sp),fr0
	.text
	.global nldf
nldf:
	nldf @(sp,sp),fr0
	.text
	.global ldd
ldd:
	ldd @(sp,sp),fp
	.text
	.global lddf
lddf:
	lddf @(sp,sp),fr0
	.text
	.global lddc
lddc:
	lddc @(sp,sp),cpr0
	.text
	.global nldd
nldd:
	nldd @(sp,sp),fp
	.text
	.global nlddf
nlddf:
	nlddf @(sp,sp),fr0
	.text
	.global ldq
ldq:
	ldq @(sp,sp),sp
	.text
	.global ldqf
ldqf:
	ldqf @(sp,sp),fr0
	.text
	.global ldqc
ldqc:
	ldqc @(sp,sp),cpr0
	.text
	.global nldq
nldq:
	nldq @(sp,sp),sp
	.text
	.global nldqf
nldqf:
	nldqf @(sp,sp),fr0
	.text
	.global ldsbu
ldsbu:
	ldsbu @(sp,sp),sp
	.text
	.global ldubu
ldubu:
	ldubu @(sp,sp),sp
	.text
	.global ldshu
ldshu:
	ldshu @(sp,sp),sp
	.text
	.global lduhu
lduhu:
	lduhu @(sp,sp),sp
	.text
	.global ldu
ldu:
	ldu @(sp,sp),sp
	.text
	.global nldsbu
nldsbu:
	nldsbu @(sp,sp),sp
	.text
	.global nldubu
nldubu:
	nldubu @(sp,sp),sp
	.text
	.global nldshu
nldshu:
	nldshu @(sp,sp),sp
	.text
	.global nlduhu
nlduhu:
	nlduhu @(sp,sp),sp
	.text
	.global nldu
nldu:
	nldu @(sp,sp),sp
	.text
	.global ldbfu
ldbfu:
	ldbfu @(sp,sp),fr0
	.text
	.global ldhfu
ldhfu:
	ldhfu @(sp,sp),fr0
	.text
	.global ldfu
ldfu:
	ldfu @(sp,sp),fr0
	.text
	.global ldcu
ldcu:
	ldcu @(sp,sp),cpr0
	.text
	.global nldbfu
nldbfu:
	nldbfu @(sp,sp),fr0
	.text
	.global nldhfu
nldhfu:
	nldhfu @(sp,sp),fr0
	.text
	.global nldfu
nldfu:
	nldfu @(sp,sp),fr0
	.text
	.global lddu
lddu:
	lddu @(sp,sp),fp
	.text
	.global nlddu
nlddu:
	nlddu @(sp,sp),fp
	.text
	.global lddfu
lddfu:
	lddfu @(sp,sp),fr0
	.text
	.global lddcu
lddcu:
	lddcu @(sp,sp),cpr0
	.text
	.global nlddfu
nlddfu:
	nlddfu @(sp,sp),fr0
	.text
	.global ldqu
ldqu:
	ldqu @(sp,sp),sp
	.text
	.global nldqu
nldqu:
	nldqu @(sp,sp),sp
	.text
	.global ldqfu
ldqfu:
	ldqfu @(sp,sp),fr0
	.text
	.global ldqcu
ldqcu:
	ldqcu @(sp,sp),cpr0
	.text
	.global nldqfu
nldqfu:
	nldqfu @(sp,sp),fr0
	.text
	.global ldsbi
ldsbi:
	ldsbi @(sp,0),sp
	.text
	.global ldshi
ldshi:
	ldshi @(sp,0),sp
	.text
	.global ldi
ldi:
	ldi @(sp,0),sp
	.text
	.global ldubi
ldubi:
	ldubi @(sp,0),sp
	.text
	.global lduhi
lduhi:
	lduhi @(sp,0),sp
	.text
	.global ldbfi
ldbfi:
	ldbfi @(sp,0),fr0
	.text
	.global ldhfi
ldhfi:
	ldhfi @(sp,0),fr0
	.text
	.global ldfi
ldfi:
	ldfi @(sp,0),fr0
	.text
	.global nldsbi
nldsbi:
	nldsbi @(sp,0),sp
	.text
	.global nldubi
nldubi:
	nldubi @(sp,0),sp
	.text
	.global nldshi
nldshi:
	nldshi @(sp,0),sp
	.text
	.global nlduhi
nlduhi:
	nlduhi @(sp,0),sp
	.text
	.global nldi
nldi:
	nldi @(sp,0),sp
	.text
	.global nldbfi
nldbfi:
	nldbfi @(sp,0),fr0
	.text
	.global nldhfi
nldhfi:
	nldhfi @(sp,0),fr0
	.text
	.global nldfi
nldfi:
	nldfi @(sp,0),fr0
	.text
	.global lddi
lddi:
	lddi @(sp,0),fp
	.text
	.global lddfi
lddfi:
	lddfi @(sp,0),fr0
	.text
	.global nlddi
nlddi:
	nlddi @(sp,0),fp
	.text
	.global nlddfi
nlddfi:
	nlddfi @(sp,0),fr0
	.text
	.global ldqi
ldqi:
	ldqi @(sp,0),sp
	.text
	.global ldqfi
ldqfi:
	ldqfi @(sp,0),fr0
	.text
	.global nop
nop:
	nop
	.text
	.global nldqfi
nldqfi:
	nldqfi @(sp,0),fr0
	.text
	.global stb
stb:
	stb sp,@(sp,sp)
	.text
	.global sth
sth:
	sth sp,@(sp,sp)
	.text
	.global st
st:
	st sp,@(sp,sp)
	.text
	.global stbf
stbf:
	stbf fr0,@(sp,sp)
	.text
	.global sthf
sthf:
	sthf fr0,@(sp,sp)
	.text
	.global stf
stf:
	stf fr0,@(sp,sp)
	.text
	.global stc
stc:
	stc cpr0,@(sp,sp)
	.text
	.global rstb
rstb:
	nop
	.text
	.global rsth
rsth:
	nop
	.text
	.global rst
rst:
	nop
	.text
	.global rstbf
rstbf:
	nop
	.text
	.global rsthf
rsthf:
	nop
	.text
	.global rstf
rstf:
	nop
	.text
	.global std
std:
	std fp,@(sp,sp)
	.text
	.global stdf
stdf:
	stdf fr0,@(sp,sp)
	.text
	.global stdc
stdc:
	stdc cpr0,@(sp,sp)
	.text
	.global rstd
rstd:
	nop
	.text
	.global rstdf
rstdf:
	nop
	.text
	.global stq
stq:
	stq sp,@(sp,sp)
	.text
	.global stqf
stqf:
	stqf fr0,@(sp,sp)
	.text
	.global stqc
stqc:
	stqc cpr0,@(sp,sp)
	.text
	.global rstq
rstq:
	nop
	.text
	.global rstqf
rstqf:
	nop
	.text
	.global stbu
stbu:
	stbu sp,@(sp,sp)
	.text
	.global sthu
sthu:
	sthu sp,@(sp,sp)
	.text
	.global stu
stu:
	stu sp,@(sp,sp)
	.text
	.global stbfu
stbfu:
	stbfu fr0,@(sp,sp)
	.text
	.global sthfu
sthfu:
	sthfu fr0,@(sp,sp)
	.text
	.global stfu
stfu:
	stfu fr0,@(sp,sp)
	.text
	.global stcu
stcu:
	stcu cpr0,@(sp,sp)
	.text
	.global stdu
stdu:
	stdu fp,@(sp,sp)
	.text
	.global stdfu
stdfu:
	stdfu fr0,@(sp,sp)
	.text
	.global stdcu
stdcu:
	stdcu cpr0,@(sp,sp)
	.text
	.global stqu
stqu:
	stqu sp,@(sp,sp)
	.text
	.global stqfu
stqfu:
	stqfu fr0,@(sp,sp)
	.text
	.global stqcu
stqcu:
	stqcu cpr0,@(sp,sp)
	.text
	.global cldsb
cldsb:
	cldsb @(sp,sp),sp,cc0,0
	.text
	.global cldub
cldub:
	cldub @(sp,sp),sp,cc0,0
	.text
	.global cldsh
cldsh:
	cldsh @(sp,sp),sp,cc0,0
	.text
	.global clduh
clduh:
	clduh @(sp,sp),sp,cc0,0
	.text
	.global cld
cld:
	cld @(sp,sp),sp,cc0,0
	.text
	.global cldbf
cldbf:
	cldbf @(sp,sp),fr0,cc0,0
	.text
	.global cldhf
cldhf:
	cldhf @(sp,sp),fr0,cc0,0
	.text
	.global cldf
cldf:
	cldf @(sp,sp),fr0,cc0,0
	.text
	.global cldd
cldd:
	cldd @(sp,sp),fp,cc0,0
	.text
	.global clddf
clddf:
	clddf @(sp,sp),fr0,cc0,0
	.text
	.global cldq
cldq:
	cldq @(sp,sp),sp,cc0,0
	.text
	.global cldsbu
cldsbu:
	cldsbu @(sp,sp),sp,cc0,0
	.text
	.global cldubu
cldubu:
	cldubu @(sp,sp),sp,cc0,0
	.text
	.global cldshu
cldshu:
	cldshu @(sp,sp),sp,cc0,0
	.text
	.global clduhu
clduhu:
	clduhu @(sp,sp),sp,cc0,0
	.text
	.global cldu
cldu:
	cldu @(sp,sp),sp,cc0,0
	.text
	.global cldbfu
cldbfu:
	cldbfu @(sp,sp),fr0,cc0,0
	.text
	.global cldhfu
cldhfu:
	cldhfu @(sp,sp),fr0,cc0,0
	.text
	.global cldfu
cldfu:
	cldfu @(sp,sp),fr0,cc0,0
	.text
	.global clddu
clddu:
	clddu @(sp,sp),fp,cc0,0
	.text
	.global clddfu
clddfu:
	clddfu @(sp,sp),fr0,cc0,0
	.text
	.global cldqu
cldqu:
	cldqu @(sp,sp),sp,cc0,0
	.text
	.global cstb
cstb:
	cstb sp,@(sp,sp),cc0,0
	.text
	.global csth
csth:
	csth sp,@(sp,sp),cc0,0
	.text
	.global cst
cst:
	cst sp,@(sp,sp),cc0,0
	.text
	.global cstbf
cstbf:
	cstbf fr0,@(sp,sp),cc0,0
	.text
	.global csthf
csthf:
	csthf fr0,@(sp,sp),cc0,0
	.text
	.global cstf
cstf:
	cstf fr0,@(sp,sp),cc0,0
	.text
	.global cstd
cstd:
	cstd fp,@(sp,sp),cc0,0
	.text
	.global cstdf
cstdf:
	cstdf fr0,@(sp,sp),cc0,0
	.text
	.global cstq
cstq:
	cstq sp,@(sp,sp),cc0,0
	.text
	.global cstbu
cstbu:
	cstbu sp,@(sp,sp),cc0,0
	.text
	.global csthu
csthu:
	csthu sp,@(sp,sp),cc0,0
	.text
	.global cstu
cstu:
	cstu sp,@(sp,sp),cc0,0
	.text
	.global cstbfu
cstbfu:
	cstbfu fr0,@(sp,sp),cc0,0
	.text
	.global csthfu
csthfu:
	csthfu fr0,@(sp,sp),cc0,0
	.text
	.global cstfu
cstfu:
	cstfu fr0,@(sp,sp),cc0,0
	.text
	.global cstdu
cstdu:
	cstdu fp,@(sp,sp),cc0,0
	.text
	.global cstdfu
cstdfu:
	cstdfu fr0,@(sp,sp),cc0,0
	.text
	.global stbi
stbi:
	stbi sp,@(sp,0)
	.text
	.global sthi
sthi:
	sthi sp,@(sp,0)
	.text
	.global sti
sti:
	sti sp,@(sp,0)
	.text
	.global stbfi
stbfi:
	stbfi fr0,@(sp,0)
	.text
	.global sthfi
sthfi:
	sthfi fr0,@(sp,0)
	.text
	.global stfi
stfi:
	stfi fr0,@(sp,0)
	.text
	.global stdi
stdi:
	stdi fp,@(sp,0)
	.text
	.global stdfi
stdfi:
	stdfi fr0,@(sp,0)
	.text
	.global stqi
stqi:
	stqi sp,@(sp,0)
	.text
	.global stqfi
stqfi:
	stqfi fr0,@(sp,0)
	.text
	.global swap
swap:
	swap @(sp,sp),sp
	.text
	.global swapi
swapi:
	swapi @(sp,0),sp
	.text
	.global cswap
cswap:
	cswap @(sp,sp),sp,cc0,0
	.text
	.global movgf
movgf:
	movgf sp,fr0
	.text
	.global movfg
movfg:
	movfg fr0,sp
	.text
	.global movgfd
movgfd:
	movgfd sp,fr0
	.text
	.global movfgd
movfgd:
	movfgd fr0,sp
	.text
	.global movgfq
movgfq:
	movgfq sp,fr0
	.text
	.global movfgq
movfgq:
	movfgq fr0,sp
	.text
	.global cmovgf
cmovgf:
	cmovgf sp,fr0,cc0,0
	.text
	.global cmovfg
cmovfg:
	cmovfg fr0,sp,cc0,0
	.text
	.global cmovgfd
cmovgfd:
	cmovgfd sp,fr0,cc0,0
	.text
	.global cmovfgd
cmovfgd:
	cmovfgd fr0,sp,cc0,0
	.text
	.global movgs
movgs:
	movgs sp,psr
	.text
	.global movsg
movsg:
	movsg psr,sp
	.text
	.global bno
bno:
	bno
	.text
	.global bra
bra:
	bra footext
	.text
	.global beq
beq:
	beq icc0,0,footext
	.text
	.global bne
bne:
	bne icc0,0,footext
	.text
	.global ble
ble:
	ble icc0,0,footext
	.text
	.global bgt
bgt:
	bgt icc0,0,footext
	.text
	.global blt
blt:
	blt icc0,0,footext
	.text
	.global bge
bge:
	bge icc0,0,footext
	.text
	.global bls
bls:
	bls icc0,0,footext
	.text
	.global bhi
bhi:
	bhi icc0,0,footext
	.text
	.global bc
bc:
	bc icc0,0,footext
	.text
	.global bnc
bnc:
	bnc icc0,0,footext
	.text
	.global bn
bn:
	bn icc0,0,footext
	.text
	.global bp
bp:
	bp icc0,0,footext
	.text
	.global bv
bv:
	bv icc0,0,footext
	.text
	.global bnv
bnv:
	bnv icc0,0,footext
	.text
	.global fbno
fbno:
	fbno
	.text
	.global fbra
fbra:
	fbra footext
	.text
	.global fbne
fbne:
	fbne fcc0,0,footext
	.text
	.global fbeq
fbeq:
	fbeq fcc0,0,footext
	.text
	.global fblg
fblg:
	fblg fcc0,0,footext
	.text
	.global fbue
fbue:
	fbue fcc0,0,footext
	.text
	.global fbul
fbul:
	fbul fcc0,0,footext
	.text
	.global fbge
fbge:
	fbge fcc0,0,footext
	.text
	.global fblt
fblt:
	fblt fcc0,0,footext
	.text
	.global fbuge
fbuge:
	fbuge fcc0,0,footext
	.text
	.global fbug
fbug:
	fbug fcc0,0,footext
	.text
	.global fble
fble:
	fble fcc0,0,footext
	.text
	.global fbgt
fbgt:
	fbgt fcc0,0,footext
	.text
	.global fbule
fbule:
	fbule fcc0,0,footext
	.text
	.global fbu
fbu:
	fbu fcc0,0,footext
	.text
	.global fbo
fbo:
	fbo fcc0,0,footext
	.text
	.global bctrlr
bctrlr:
	bctrlr 0,0
	.text
	.global bnolr
bnolr:
	bnolr
	.text
	.global bralr
bralr:
	bralr
	.text
	.global beqlr
beqlr:
	beqlr icc0,0
	.text
	.global bnelr
bnelr:
	bnelr icc0,0
	.text
	.global blelr
blelr:
	blelr icc0,0
	.text
	.global bgtlr
bgtlr:
	bgtlr icc0,0
	.text
	.global bltlr
bltlr:
	bltlr icc0,0
	.text
	.global bgelr
bgelr:
	bgelr icc0,0
	.text
	.global blslr
blslr:
	blslr icc0,0
	.text
	.global bhilr
bhilr:
	bhilr icc0,0
	.text
	.global bclr
bclr:
	bclr icc0,0
	.text
	.global bnclr
bnclr:
	bnclr icc0,0
	.text
	.global bnlr
bnlr:
	bnlr icc0,0
	.text
	.global bplr
bplr:
	bplr icc0,0
	.text
	.global bvlr
bvlr:
	bvlr icc0,0
	.text
	.global bnvlr
bnvlr:
	bnvlr icc0,0
	.text
	.global fbnolr
fbnolr:
	fbnolr
	.text
	.global fbralr
fbralr:
	fbralr
	.text
	.global fbeqlr
fbeqlr:
	fbeqlr fcc0,0
	.text
	.global fbnelr
fbnelr:
	fbnelr fcc0,0
	.text
	.global fblglr
fblglr:
	fblglr fcc0,0
	.text
	.global fbuelr
fbuelr:
	fbuelr fcc0,0
	.text
	.global fbullr
fbullr:
	fbullr fcc0,0
	.text
	.global fbgelr
fbgelr:
	fbgelr fcc0,0
	.text
	.global fbltlr
fbltlr:
	fbltlr fcc0,0
	.text
	.global fbugelr
fbugelr:
	fbugelr fcc0,0
	.text
	.global fbuglr
fbuglr:
	fbuglr fcc0,0
	.text
	.global fblelr
fblelr:
	fblelr fcc0,0
	.text
	.global fbgtlr
fbgtlr:
	fbgtlr fcc0,0
	.text
	.global fbulelr
fbulelr:
	fbulelr fcc0,0
	.text
	.global fbulr
fbulr:
	fbulr fcc0,0
	.text
	.global fbolr
fbolr:
	fbolr fcc0,0
	.text
	.global bcnolr
bcnolr:
	bcnolr
	.text
	.global bcralr
bcralr:
	bcralr 0
	.text
	.global bceqlr
bceqlr:
	bceqlr icc0,0,0
	.text
	.global bcnelr
bcnelr:
	bcnelr icc0,0,0
	.text
	.global bclelr
bclelr:
	bclelr icc0,0,0
	.text
	.global bcgtlr
bcgtlr:
	bcgtlr icc0,0,0
	.text
	.global bcltlr
bcltlr:
	bcltlr icc0,0,0
	.text
	.global bcgelr
bcgelr:
	bcgelr icc0,0,0
	.text
	.global bclslr
bclslr:
	bclslr icc0,0,0
	.text
	.global bchilr
bchilr:
	bchilr icc0,0,0
	.text
	.global bcclr
bcclr:
	bcclr icc0,0,0
	.text
	.global bcnclr
bcnclr:
	bcnclr icc0,0,0
	.text
	.global bcnlr
bcnlr:
	bcnlr icc0,0,0
	.text
	.global bcplr
bcplr:
	bcplr icc0,0,0
	.text
	.global bcvlr
bcvlr:
	bcvlr icc0,0,0
	.text
	.global bcnvlr
bcnvlr:
	bcnvlr icc0,0,0
	.text
	.global fcbnolr
fcbnolr:
	fcbnolr
	.text
	.global fcbralr
fcbralr:
	fcbralr 0
	.text
	.global fcbeqlr
fcbeqlr:
	fcbeqlr fcc0,0,0
	.text
	.global fcbnelr
fcbnelr:
	fcbnelr fcc0,0,0
	.text
	.global fcblglr
fcblglr:
	fcblglr fcc0,0,0
	.text
	.global fcbuelr
fcbuelr:
	fcbuelr fcc0,0,0
	.text
	.global fcbullr
fcbullr:
	fcbullr fcc0,0,0
	.text
	.global fcbgelr
fcbgelr:
	fcbgelr fcc0,0,0
	.text
	.global fcbltlr
fcbltlr:
	fcbltlr fcc0,0,0
	.text
	.global fcbugelr
fcbugelr:
	fcbugelr fcc0,0,0
	.text
	.global fcbuglr
fcbuglr:
	fcbuglr fcc0,0,0
	.text
	.global fcblelr
fcblelr:
	fcblelr fcc0,0,0
	.text
	.global fcbgtlr
fcbgtlr:
	fcbgtlr fcc0,0,0
	.text
	.global fcbulelr
fcbulelr:
	fcbulelr fcc0,0,0
	.text
	.global fcbulr
fcbulr:
	fcbulr fcc0,0,0
	.text
	.global fcbolr
fcbolr:
	fcbolr fcc0,0,0
	.text
	.global jmpl
jmpl:
	jmpl @(sp,sp)
	.text
	.global jmpil
jmpil:
	jmpil @(sp,0)
	.text
	.global call
call:
	call footext
	.text
	.global rett
rett:
	.text
	.global rei
rei:
	rei 0
	.text
	.global tno
tno:
	tno
	.text
	.global tra
tra:
	tra sp,sp
	.text
	.global teq
teq:
	teq icc0,sp,sp
	.text
	.global tne
tne:
	tne icc0,sp,sp
	.text
	.global tle
tle:
	tle icc0,sp,sp
	.text
	.global tgt
tgt:
	tgt icc0,sp,sp
	.text
	.global tlt
tlt:
	tlt icc0,sp,sp
	.text
	.global tge
tge:
	tge icc0,sp,sp
	.text
	.global tls
tls:
	tls icc0,sp,sp
	.text
	.global thi
thi:
	thi icc0,sp,sp
	.text
	.global tc
tc:
	tc icc0,sp,sp
	.text
	.global tnc
tnc:
	tnc icc0,sp,sp
	.text
	.global tn
tn:
	tn icc0,sp,sp
	.text
	.global tp
tp:
	tp icc0,sp,sp
	.text
	.global tv
tv:
	tv icc0,sp,sp
	.text
	.global tnv
tnv:
	tnv icc0,sp,sp
	.text
	.global ftno
ftno:
	ftno
	.text
	.global ftra
ftra:
	ftra sp,sp
	.text
	.global ftne
ftne:
	ftne fcc0,sp,sp
	.text
	.global fteq
fteq:
	fteq fcc0,sp,sp
	.text
	.global ftlg
ftlg:
	ftlg fcc0,sp,sp
	.text
	.global ftue
ftue:
	ftue fcc0,sp,sp
	.text
	.global ftul
ftul:
	ftul fcc0,sp,sp
	.text
	.global ftge
ftge:
	ftge fcc0,sp,sp
	.text
	.global ftlt
ftlt:
	ftlt fcc0,sp,sp
	.text
	.global ftuge
ftuge:
	ftuge fcc0,sp,sp
	.text
	.global ftug
ftug:
	ftug fcc0,sp,sp
	.text
	.global ftle
ftle:
	ftle fcc0,sp,sp
	.text
	.global ftgt
ftgt:
	ftgt fcc0,sp,sp
	.text
	.global ftule
ftule:
	ftule fcc0,sp,sp
	.text
	.global ftu
ftu:
	ftu fcc0,sp,sp
	.text
	.global fto
fto:
	fto fcc0,sp,sp
	.text
	.global tino
tino:
	tino
	.text
	.global tira
tira:
	tira sp,0
	.text
	.global tieq
tieq:
	tieq icc0,sp,0
	.text
	.global tine
tine:
	tine icc0,sp,0
	.text
	.global tile
tile:
	tile icc0,sp,0
	.text
	.global tigt
tigt:
	tigt icc0,sp,0
	.text
	.global tilt
tilt:
	tilt icc0,sp,0
	.text
	.global tige
tige:
	tige icc0,sp,0
	.text
	.global tils
tils:
	tils icc0,sp,0
	.text
	.global tihi
tihi:
	tihi icc0,sp,0
	.text
	.global tic
tic:
	tic icc0,sp,0
	.text
	.global tinc
tinc:
	tinc icc0,sp,0
	.text
	.global tin
tin:
	tin icc0,sp,0
	.text
	.global tip
tip:
	tip icc0,sp,0
	.text
	.global tiv
tiv:
	tiv icc0,sp,0
	.text
	.global tinv
tinv:
	tinv icc0,sp,0
	.text
	.global ftino
ftino:
	ftino
	.text
	.global ftira
ftira:
	ftira sp,0
	.text
	.global ftine
ftine:
	ftine fcc0,sp,0
	.text
	.global ftieq
ftieq:
	ftieq fcc0,sp,0
	.text
	.global ftilg
ftilg:
	ftilg fcc0,sp,0
	.text
	.global ftiue
ftiue:
	ftiue fcc0,sp,0
	.text
	.global ftiul
ftiul:
	ftiul fcc0,sp,0
	.text
	.global ftige
ftige:
	ftige fcc0,sp,0
	.text
	.global ftilt
ftilt:
	ftilt fcc0,sp,0
	.text
	.global ftiuge
ftiuge:
	ftiuge fcc0,sp,0
	.text
	.global ftiug
ftiug:
	ftiug fcc0,sp,0
	.text
	.global ftile
ftile:
	ftile fcc0,sp,0
	.text
	.global ftigt
ftigt:
	ftigt fcc0,sp,0
	.text
	.global ftiule
ftiule:
	ftiule fcc0,sp,0
	.text
	.global ftiu
ftiu:
	ftiu fcc0,sp,0
	.text
	.global ftio
ftio:
	ftio fcc0,sp,0
	.text
	.global break
break:
	.text
	.global mtrap
mtrap:
	.text
	.global andcr
andcr:
	andcr cc0,cc0,cc0
	.text
	.global orcr
orcr:
	orcr cc0,cc0,cc0
	.text
	.global xorcr
xorcr:
	xorcr cc0,cc0,cc0
	.text
	.global nandcr
nandcr:
	nandcr cc0,cc0,cc0
	.text
	.global norcr
norcr:
	norcr cc0,cc0,cc0
	.text
	.global andncr
andncr:
	andncr cc0,cc0,cc0
	.text
	.global orncr
orncr:
	orncr cc0,cc0,cc0
	.text
	.global nandncr
nandncr:
	nandncr cc0,cc0,cc0
	.text
	.global norncr
norncr:
	norncr cc0,cc0,cc0
	.text
	.global notcr
notcr:
	notcr cc0,cc0
	.text
	.global ckno
ckno:
	ckno cc7
	.text
	.global ckra
ckra:
	ckra cc7
	.text
	.global ckeq
ckeq:
	ckeq icc0,cc7
	.text
	.global ckne
ckne:
	ckne icc0,cc7
	.text
	.global ckle
ckle:
	ckle icc0,cc7
	.text
	.global ckgt
ckgt:
	ckgt icc0,cc7
	.text
	.global cklt
cklt:
	cklt icc0,cc7
	.text
	.global ckge
ckge:
	ckge icc0,cc7
	.text
	.global ckls
ckls:
	ckls icc0,cc7
	.text
	.global ckhi
ckhi:
	ckhi icc0,cc7
	.text
	.global ckc
ckc:
	ckc icc0,cc7
	.text
	.global cknc
cknc:
	cknc icc0,cc7
	.text
	.global ckn
ckn:
	ckn icc0,cc7
	.text
	.global ckp
ckp:
	ckp icc0,cc7
	.text
	.global ckv
ckv:
	ckv icc0,cc7
	.text
	.global cknv
cknv:
	cknv icc0,cc7
	.text
	.global fckno
fckno:
	fckno cc0
	.text
	.global fckra
fckra:
	fckra cc0
	.text
	.global fckne
fckne:
	fckne fcc0,cc0
	.text
	.global fckeq
fckeq:
	fckeq fcc0,cc0
	.text
	.global fcklg
fcklg:
	fcklg fcc0,cc0
	.text
	.global fckue
fckue:
	fckue fcc0,cc0
	.text
	.global fckul
fckul:
	fckul fcc0,cc0
	.text
	.global fckge
fckge:
	fckge fcc0,cc0
	.text
	.global fcklt
fcklt:
	fcklt fcc0,cc0
	.text
	.global fckuge
fckuge:
	fckuge fcc0,cc0
	.text
	.global fckug
fckug:
	fckug fcc0,cc0
	.text
	.global fckle
fckle:
	fckle fcc0,cc0
	.text
	.global fckgt
fckgt:
	fckgt fcc0,cc0
	.text
	.global fckule
fckule:
	fckule fcc0,cc0
	.text
	.global fcku
fcku:
	fcku fcc0,cc0
	.text
	.global fcko
fcko:
	fcko fcc0,cc0
	.text
	.global cckno
cckno:
	cckno cc7,cc3,0
	.text
	.global cckra
cckra:
	cckra cc7,cc3,0
	.text
	.global cckeq
cckeq:
	cckeq icc0,cc7,cc3,0
	.text
	.global cckne
cckne:
	cckne icc0,cc7,cc3,0
	.text
	.global cckle
cckle:
	cckle icc0,cc7,cc3,0
	.text
	.global cckgt
cckgt:
	cckgt icc0,cc7,cc3,0
	.text
	.global ccklt
ccklt:
	ccklt icc0,cc7,cc3,0
	.text
	.global cckge
cckge:
	cckge icc0,cc7,cc3,0
	.text
	.global cckls
cckls:
	cckls icc0,cc7,cc3,0
	.text
	.global cckhi
cckhi:
	cckhi icc0,cc7,cc3,0
	.text
	.global cckc
cckc:
	cckc icc0,cc7,cc3,0
	.text
	.global ccknc
ccknc:
	ccknc icc0,cc7,cc3,0
	.text
	.global cckn
cckn:
	cckn icc0,cc7,cc3,0
	.text
	.global cckp
cckp:
	cckp icc0,cc7,cc3,0
	.text
	.global cckv
cckv:
	cckv icc0,cc7,cc3,0
	.text
	.global ccknv
ccknv:
	ccknv icc0,cc7,cc3,0
	.text
	.global cfckno
cfckno:
	cfckno cc0,cc0,0
	.text
	.global cfckra
cfckra:
	cfckra cc0,cc0,0
	.text
	.global cfckne
cfckne:
	cfckne fcc0,cc0,cc0,0
	.text
	.global cfckeq
cfckeq:
	cfckeq fcc0,cc0,cc0,0
	.text
	.global cfcklg
cfcklg:
	cfcklg fcc0,cc0,cc0,0
	.text
	.global cfckue
cfckue:
	cfckue fcc0,cc0,cc0,0
	.text
	.global cfckul
cfckul:
	cfckul fcc0,cc0,cc0,0
	.text
	.global cfckge
cfckge:
	cfckge fcc0,cc0,cc0,0
	.text
	.global cfcklt
cfcklt:
	cfcklt fcc0,cc0,cc0,0
	.text
	.global cfckuge
cfckuge:
	cfckuge fcc0,cc0,cc0,0
	.text
	.global cfckug
cfckug:
	cfckug fcc0,cc0,cc0,0
	.text
	.global cfckle
cfckle:
	cfckle fcc0,cc0,cc0,0
	.text
	.global cfckgt
cfckgt:
	cfckgt fcc0,cc0,cc0,0
	.text
	.global cfckule
cfckule:
	cfckule fcc0,cc0,cc0,0
	.text
	.global cfcku
cfcku:
	cfcku fcc0,cc0,cc0,0
	.text
	.global cfcko
cfcko:
	cfcko fcc0,cc0,cc0,0
	.text
	.global cjmpl
cjmpl:
	cjmpl @(sp,sp),cc0,0
	.text
	.global ici
ici:
	ici @(sp,sp)
	.text
	.global dci
dci:
	dci @(sp,sp)
	.text
	.global dcf
dcf:
	dcf @(sp,sp)
	.text
	.global witlb
witlb:
	witlb sp,@(sp,sp)
	.text
	.global wdtlb
wdtlb:
	wdtlb sp,@(sp,sp)
	.text
	.global itlbi
itlbi:
	itlbi @(sp,sp)
	.text
	.global dtlbi
dtlbi:
	dtlbi @(sp,sp)
	.text
	.global icpl
icpl:
	icpl sp,sp,0
	.text
	.global dcpl
dcpl:
	dcpl sp,sp,0
	.text
	.global icul
icul:
	icul sp
	.text
	.global dcul
dcul:
	dcul sp
	.text
	.global bar
bar:
	.text
	.global membar
membar:
	.text
	.global clrgr
clrgr:
	clrgr sp
	.text
	.global clrfr
clrfr:
	clrfr fr0
	.text
	.global clrga
clrga:
	.text
	.global clrfa
clrfa:
	.text
	.global commitgr
commitgr:
	commitgr sp
	.text
	.global commitfr
commitfr:
	commitfr fr0
	.text
	.global commitgra
commitgra:
	.text
	.global commitfra
commitfra:
	.text
	.global fitos
fitos:
	fitos fr0,fr0
	.text
	.global fstoi
fstoi:
	fstoi fr0,fr0
	.text
	.global fitod
fitod:
	fitod fr0,fr0
	.text
	.global fdtoi
fdtoi:
	fdtoi fr0,fr0
	.text
	.global fmovs
fmovs:
	fmovs fr0,fr0
	.text
	.global fmovd
fmovd:
	fmovd fr0,fr0
	.text
	.global fnegs
fnegs:
	fnegs fr0,fr0
	.text
	.global fnegd
fnegd:
	fnegd fr0,fr0
	.text
	.global fabss
fabss:
	fabss fr0,fr0
	.text
	.global fabsd
fabsd:
	fabsd fr0,fr0
	.text
	.global fsqrts
fsqrts:
	fsqrts fr0,fr0
	.text
	.global fsqrtd
fsqrtd:
	fsqrtd fr0,fr0
	.text
	.global fadds
fadds:
	fadds fr0,fr0,fr0
	.text
	.global fsubs
fsubs:
	fsubs fr0,fr0,fr0
	.text
	.global fmuls
fmuls:
	fmuls fr0,fr0,fr0
	.text
	.global fdivs
fdivs:
	fdivs fr0,fr0,fr0
	.text
	.global faddd
faddd:
	faddd fr0,fr0,fr0
	.text
	.global fsubd
fsubd:
	fsubd fr0,fr0,fr0
	.text
	.global fmuld
fmuld:
	fmuld fr0,fr0,fr0
	.text
	.global fdivd
fdivd:
	fdivd fr0,fr0,fr0
	.text
	.global fcmps
fcmps:
	fcmps fr0,fr0,fcc0
	.text
	.global fcmpd
fcmpd:
	fcmpd fr0,fr0,fcc0
	.text
	.global fmadds
fmadds:
	fmadds fr0,fr0,fr0
	.text
	.global fmsubs
fmsubs:
	fmsubs fr0,fr0,fr0
	.text
	.global fmaddd
fmaddd:
	fmaddd fr0,fr0,fr0
	.text
	.global fmsubd
fmsubd:
	fmsubd fr0,fr0,fr0
	.text
	.global mand
mand:
	mand fr0,fr0,fr0
	.text
	.global mor
mor:
	mor fr0,fr0,fr0
	.text
	.global mxor
mxor:
	mxor fr0,fr0,fr0
	.text
	.global mnot
mnot:
	mnot fr0,fr0
	.text
	.global mrotli
mrotli:
	mrotli fr0,0,fr0
	.text
	.global mrotri
mrotri:
	mrotri fr0,0,fr0
	.text
	.global mwcut
mwcut:
	mwcut fr0,fr0,fr0
	.text
	.global mwcuti
mwcuti:
	mwcuti fr0,0,fr0
