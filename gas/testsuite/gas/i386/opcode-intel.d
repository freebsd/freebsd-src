#source: opcode.s
#as: -J
#objdump: -dwMintel
#name: i386 opcodes (Intel disassembly)

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
 *[0-9a-f]+:	00 90 90 90 90 90[ 	]+add[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\],dl
 *[0-9a-f]+:	01 90 90 90 90 90[ 	]+add[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	02 90 90 90 90 90[ 	]+add[ 	]+dl,(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	03 90 90 90 90 90[ 	]+add[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	04 90[ 	]+add[ 	]+al,0x90
 *[0-9a-f]+:	05 90 90 90 90[ 	]+add[ 	]+eax,0x90909090
 *[0-9a-f]+:	06[ 	]+push[ 	]+es
 *[0-9a-f]+:	07[ 	]+pop[ 	]+es
 *[0-9a-f]+:	08 90 90 90 90 90[ 	]+or[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\],dl
 *[0-9a-f]+:	09 90 90 90 90 90[ 	]+or[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	0a 90 90 90 90 90[ 	]+or[ 	]+dl,(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0b 90 90 90 90 90[ 	]+or[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0c 90[ 	]+or[ 	]+al,0x90
 *[0-9a-f]+:	0d 90 90 90 90[ 	]+or[ 	]+eax,0x90909090
 *[0-9a-f]+:	0e[ 	]+push[ 	]+cs
 *[0-9a-f]+:	10 90 90 90 90 90[ 	]+adc[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\],dl
 *[0-9a-f]+:	11 90 90 90 90 90[ 	]+adc[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	12 90 90 90 90 90[ 	]+adc[ 	]+dl,(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	13 90 90 90 90 90[ 	]+adc[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	14 90[ 	]+adc[ 	]+al,0x90
 *[0-9a-f]+:	15 90 90 90 90[ 	]+adc[ 	]+eax,0x90909090
 *[0-9a-f]+:	16[ 	]+push[ 	]+ss
 *[0-9a-f]+:	17[ 	]+pop[ 	]+ss
 *[0-9a-f]+:	18 90 90 90 90 90[ 	]+sbb[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\],dl
 *[0-9a-f]+:	19 90 90 90 90 90[ 	]+sbb[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	1a 90 90 90 90 90[ 	]+sbb[ 	]+dl,(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	1b 90 90 90 90 90[ 	]+sbb[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	1c 90[ 	]+sbb[ 	]+al,0x90
 *[0-9a-f]+:	1d 90 90 90 90[ 	]+sbb[ 	]+eax,0x90909090
 *[0-9a-f]+:	1e[ 	]+push[ 	]+ds
 *[0-9a-f]+:	1f[ 	]+pop[ 	]+ds
 *[0-9a-f]+:	20 90 90 90 90 90[ 	]+and[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\],dl
 *[0-9a-f]+:	21 90 90 90 90 90[ 	]+and[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	22 90 90 90 90 90[ 	]+and[ 	]+dl,(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	23 90 90 90 90 90[ 	]+and[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	24 90[ 	]+and[ 	]+al,0x90
 *[0-9a-f]+:	25 90 90 90 90[ 	]+and[ 	]+eax,0x90909090
 *[0-9a-f]+:	27[ 	]+daa[ 	]*
 *[0-9a-f]+:	28 90 90 90 90 90[ 	]+sub[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\],dl
 *[0-9a-f]+:	29 90 90 90 90 90[ 	]+sub[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	2a 90 90 90 90 90[ 	]+sub[ 	]+dl,(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	2b 90 90 90 90 90[ 	]+sub[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	2c 90[ 	]+sub[ 	]+al,0x90
 *[0-9a-f]+:	2d 90 90 90 90[ 	]+sub[ 	]+eax,0x90909090
 *[0-9a-f]+:	2f[ 	]+das[ 	]*
 *[0-9a-f]+:	30 90 90 90 90 90[ 	]+xor[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\],dl
 *[0-9a-f]+:	31 90 90 90 90 90[ 	]+xor[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	32 90 90 90 90 90[ 	]+xor[ 	]+dl,(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	33 90 90 90 90 90[ 	]+xor[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	34 90[ 	]+xor[ 	]+al,0x90
 *[0-9a-f]+:	35 90 90 90 90[ 	]+xor[ 	]+eax,0x90909090
 *[0-9a-f]+:	37[ 	]+aaa[ 	]*
 *[0-9a-f]+:	38 90 90 90 90 90[ 	]+cmp[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\],dl
 *[0-9a-f]+:	39 90 90 90 90 90[ 	]+cmp[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	3a 90 90 90 90 90[ 	]+cmp[ 	]+dl,(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	3b 90 90 90 90 90[ 	]+cmp[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	3c 90[ 	]+cmp[ 	]+al,0x90
 *[0-9a-f]+:	3d 90 90 90 90[ 	]+cmp[ 	]+eax,0x90909090
 *[0-9a-f]+:	3f[ 	]+aas[ 	]*
 *[0-9a-f]+:	40[ 	]+inc[ 	]+eax
 *[0-9a-f]+:	41[ 	]+inc[ 	]+ecx
 *[0-9a-f]+:	42[ 	]+inc[ 	]+edx
 *[0-9a-f]+:	43[ 	]+inc[ 	]+ebx
 *[0-9a-f]+:	44[ 	]+inc[ 	]+esp
 *[0-9a-f]+:	45[ 	]+inc[ 	]+ebp
 *[0-9a-f]+:	46[ 	]+inc[ 	]+esi
 *[0-9a-f]+:	47[ 	]+inc[ 	]+edi
 *[0-9a-f]+:	48[ 	]+dec[ 	]+eax
 *[0-9a-f]+:	49[ 	]+dec[ 	]+ecx
 *[0-9a-f]+:	4a[ 	]+dec[ 	]+edx
 *[0-9a-f]+:	4b[ 	]+dec[ 	]+ebx
 *[0-9a-f]+:	4c[ 	]+dec[ 	]+esp
 *[0-9a-f]+:	4d[ 	]+dec[ 	]+ebp
 *[0-9a-f]+:	4e[ 	]+dec[ 	]+esi
 *[0-9a-f]+:	4f[ 	]+dec[ 	]+edi
 *[0-9a-f]+:	50[ 	]+push[ 	]+eax
 *[0-9a-f]+:	51[ 	]+push[ 	]+ecx
 *[0-9a-f]+:	52[ 	]+push[ 	]+edx
 *[0-9a-f]+:	53[ 	]+push[ 	]+ebx
 *[0-9a-f]+:	54[ 	]+push[ 	]+esp
 *[0-9a-f]+:	55[ 	]+push[ 	]+ebp
 *[0-9a-f]+:	56[ 	]+push[ 	]+esi
 *[0-9a-f]+:	57[ 	]+push[ 	]+edi
 *[0-9a-f]+:	58[ 	]+pop[ 	]+eax
 *[0-9a-f]+:	59[ 	]+pop[ 	]+ecx
 *[0-9a-f]+:	5a[ 	]+pop[ 	]+edx
 *[0-9a-f]+:	5b[ 	]+pop[ 	]+ebx
 *[0-9a-f]+:	5c[ 	]+pop[ 	]+esp
 *[0-9a-f]+:	5d[ 	]+pop[ 	]+ebp
 *[0-9a-f]+:	5e[ 	]+pop[ 	]+esi
 *[0-9a-f]+:	5f[ 	]+pop[ 	]+edi
 *[0-9a-f]+:	60[ 	]+pusha[ 	]*
 *[0-9a-f]+:	61[ 	]+popa[ 	]*
 *[0-9a-f]+:	62 90 90 90 90 90[ 	]+bound[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	63 90 90 90 90 90[ 	]+arpl[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	68 90 90 90 90[ 	]+push[ 	]+0x90909090
 *[0-9a-f]+:	69 90 90 90 90 90 90 90 90 90[ 	]+imul[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\],0x90909090
 *[0-9a-f]+:	6a 90[ 	]+push[ 	]+0xffffff90
 *[0-9a-f]+:	6b 90 90 90 90 90 90[ 	]+imul[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\],0xffffff90
 *[0-9a-f]+:	6c[ 	]+ins[ 	]+BYTE PTR es:\[edi\],dx
 *[0-9a-f]+:	6d[ 	]+ins[ 	]+DWORD PTR es:\[edi\],dx
 *[0-9a-f]+:	6e[ 	]+outs[ 	]+dx,BYTE PTR ds:\[esi\]
 *[0-9a-f]+:	6f[ 	]+outs[ 	]+dx,DWORD PTR ds:\[esi\]
 *[0-9a-f]+:	70 90[ 	]+jo[ 	]+(0x)?df.*
 *[0-9a-f]+:	71 90[ 	]+jno[ 	]+(0x)?e1.*
 *[0-9a-f]+:	72 90[ 	]+jb[ 	]+(0x)?e3.*
 *[0-9a-f]+:	73 90[ 	]+jae[ 	]+(0x)?e5.*
 *[0-9a-f]+:	74 90[ 	]+je[ 	]+(0x)?e7.*
 *[0-9a-f]+:	75 90[ 	]+jne[ 	]+(0x)?e9.*
 *[0-9a-f]+:	76 90[ 	]+jbe[ 	]+(0x)?eb.*
 *[0-9a-f]+:	77 90[ 	]+ja[ 	]+(0x)?ed.*
 *[0-9a-f]+:	78 90[ 	]+js[ 	]+(0x)?ef.*
 *[0-9a-f]+:	79 90[ 	]+jns[ 	]+(0x)?f1.*
 *[0-9a-f]+:	7a 90[ 	]+jp[ 	]+(0x)?f3.*
 *[0-9a-f]+:	7b 90[ 	]+jnp[ 	]+(0x)?f5.*
 *[0-9a-f]+:	7c 90[ 	]+jl[ 	]+(0x)?f7.*
 *[0-9a-f]+:	7d 90[ 	]+jge[ 	]+(0x)?f9.*
 *[0-9a-f]+:	7e 90[ 	]+jle[ 	]+(0x)?fb.*
 *[0-9a-f]+:	7f 90[ 	]+jg[ 	]+(0x)?fd.*
 *[0-9a-f]+:	80 90 90 90 90 90 90[ 	]+adc[ 	]+BYTE PTR \[eax-0x6f6f6f70\],0x90
 *[0-9a-f]+:	81 90 90 90 90 90 90 90 90 90[ 	]+adc[ 	]+DWORD PTR \[eax-0x6f6f6f70\],0x90909090
 *[0-9a-f]+:	83 90 90 90 90 90 90[ 	]+adc[ 	]+DWORD PTR \[eax-0x6f6f6f70\],0xffffff90
 *[0-9a-f]+:	84 90 90 90 90 90[ 	]+test[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\],dl
 *[0-9a-f]+:	85 90 90 90 90 90[ 	]+test[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	86 90 90 90 90 90[ 	]+xchg[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\],dl
 *[0-9a-f]+:	87 90 90 90 90 90[ 	]+xchg[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	88 90 90 90 90 90[ 	]+mov[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\],dl
 *[0-9a-f]+:	89 90 90 90 90 90[ 	]+mov[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	8a 90 90 90 90 90[ 	]+mov[ 	]+dl,(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	8b 90 90 90 90 90[ 	]+mov[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	8c 90 90 90 90 90[ 	]+mov[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],ss
 *[0-9a-f]+:	8d 90 90 90 90 90[ 	]+lea[ 	]+edx,\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	8e 90 90 90 90 90[ 	]+mov[ 	]+ss,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	8f 80 90 90 90 90[ 	]+pop[ 	]+DWORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	90[ 	]+nop[ 	]*
 *[0-9a-f]+:	91[ 	]+xchg[ 	]+ecx,eax
 *[0-9a-f]+:	92[ 	]+xchg[ 	]+edx,eax
 *[0-9a-f]+:	93[ 	]+xchg[ 	]+ebx,eax
 *[0-9a-f]+:	94[ 	]+xchg[ 	]+esp,eax
 *[0-9a-f]+:	95[ 	]+xchg[ 	]+ebp,eax
 *[0-9a-f]+:	96[ 	]+xchg[ 	]+esi,eax
 *[0-9a-f]+:	97[ 	]+xchg[ 	]+edi,eax
 *[0-9a-f]+:	98[ 	]+cwde[ 	]*
 *[0-9a-f]+:	99[ 	]+cdq[ 	]*
 *[0-9a-f]+:	9a 90 90 90 90 90 90[ 	]+call[ 	]+0x9090:0x90909090
 *[0-9a-f]+:	9b[ 	]+fwait
 *[0-9a-f]+:	9c[ 	]+pushf[ 	]*
 *[0-9a-f]+:	9d[ 	]+popf[ 	]*
 *[0-9a-f]+:	9e[ 	]+sahf[ 	]*
 *[0-9a-f]+:	9f[ 	]+lahf[ 	]*
 *[0-9a-f]+:	a0 90 90 90 90[ 	]+mov[ 	]+al,ds:0x90909090
 *[0-9a-f]+:	a1 90 90 90 90[ 	]+mov[ 	]+eax,ds:0x90909090
 *[0-9a-f]+:	a2 90 90 90 90[ 	]+mov[ 	]+ds:0x90909090,al
 *[0-9a-f]+:	a3 90 90 90 90[ 	]+mov[ 	]+ds:0x90909090,eax
 *[0-9a-f]+:	a4[ 	]+movs[ 	]+BYTE PTR es:\[edi\],(BYTE PTR )?ds:\[esi\]
 *[0-9a-f]+:	a5[ 	]+movs[ 	]+DWORD PTR es:\[edi\],(DWORD PTR )?ds:\[esi\]
 *[0-9a-f]+:	a6[ 	]+cmps[ 	]+BYTE PTR ds:\[esi\],(BYTE PTR )?es:\[edi\]
 *[0-9a-f]+:	a7[ 	]+cmps[ 	]+DWORD PTR ds:\[esi\],(DWORD PTR )?es:\[edi\]
 *[0-9a-f]+:	a8 90[ 	]+test[ 	]+al,0x90
 *[0-9a-f]+:	a9 90 90 90 90[ 	]+test[ 	]+eax,0x90909090
 *[0-9a-f]+:	aa[ 	]+stos[ 	]+BYTE PTR es:\[edi\](,al)?
 *[0-9a-f]+:	ab[ 	]+stos[ 	]+DWORD PTR es:\[edi\](,eax)?
 *[0-9a-f]+:	ac[ 	]+lods[ 	]+(al,)?BYTE PTR ds:\[esi\]
 *[0-9a-f]+:	ad[ 	]+lods[ 	]+(eax,)?DWORD PTR ds:\[esi\]
 *[0-9a-f]+:	ae[ 	]+scas[ 	]+(al,)?BYTE PTR es:\[edi\]
 *[0-9a-f]+:	af[ 	]+scas[ 	]+(eax,)?DWORD PTR es:\[edi\]
 *[0-9a-f]+:	b0 90[ 	]+mov[ 	]+al,0x90
 *[0-9a-f]+:	b1 90[ 	]+mov[ 	]+cl,0x90
 *[0-9a-f]+:	b2 90[ 	]+mov[ 	]+dl,0x90
 *[0-9a-f]+:	b3 90[ 	]+mov[ 	]+bl,0x90
 *[0-9a-f]+:	b4 90[ 	]+mov[ 	]+ah,0x90
 *[0-9a-f]+:	b5 90[ 	]+mov[ 	]+ch,0x90
 *[0-9a-f]+:	b6 90[ 	]+mov[ 	]+dh,0x90
 *[0-9a-f]+:	b7 90[ 	]+mov[ 	]+bh,0x90
 *[0-9a-f]+:	b8 90 90 90 90[ 	]+mov[ 	]+eax,0x90909090
 *[0-9a-f]+:	b9 90 90 90 90[ 	]+mov[ 	]+ecx,0x90909090
 *[0-9a-f]+:	ba 90 90 90 90[ 	]+mov[ 	]+edx,0x90909090
 *[0-9a-f]+:	bb 90 90 90 90[ 	]+mov[ 	]+ebx,0x90909090
 *[0-9a-f]+:	bc 90 90 90 90[ 	]+mov[ 	]+esp,0x90909090
 *[0-9a-f]+:	bd 90 90 90 90[ 	]+mov[ 	]+ebp,0x90909090
 *[0-9a-f]+:	be 90 90 90 90[ 	]+mov[ 	]+esi,0x90909090
 *[0-9a-f]+:	bf 90 90 90 90[ 	]+mov[ 	]+edi,0x90909090
 *[0-9a-f]+:	c0 90 90 90 90 90 90[ 	]+rcl[ 	]+BYTE PTR \[eax-0x6f6f6f70\],0x90
 *[0-9a-f]+:	c1 90 90 90 90 90 90[ 	]+rcl[ 	]+DWORD PTR \[eax-0x6f6f6f70\],0x90
 *[0-9a-f]+:	c2 90 90[ 	]+ret[ 	]+0x9090
 *[0-9a-f]+:	c3[ 	]+ret[ 	]*
 *[0-9a-f]+:	c4 90 90 90 90 90[ 	]+les[ 	]+edx,(FWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	c5 90 90 90 90 90[ 	]+lds[ 	]+edx,(FWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	c6 80 90 90 90 90 90[ 	]+mov[ 	]+BYTE PTR \[eax-0x6f6f6f70\],0x90
 *[0-9a-f]+:	c7 80 90 90 90 90 90 90 90 90[ 	]+mov[ 	]+DWORD PTR \[eax-0x6f6f6f70\],0x90909090
 *[0-9a-f]+:	c8 90 90 90[ 	]+enter[ 	]+0x9090,0x90
 *[0-9a-f]+:	c9[ 	]+leave[ 	]*
 *[0-9a-f]+:	ca 90 90[ 	]+lret[ 	]+0x9090
 *[0-9a-f]+:	cb[ 	]+lret[ 	]*
 *[0-9a-f]+:	cc[ 	]+int3[ 	]*
 *[0-9a-f]+:	cd 90[ 	]+int[ 	]+0x90
 *[0-9a-f]+:	ce[ 	]+into[ 	]*
 *[0-9a-f]+:	cf[ 	]+iret[ 	]*
 *[0-9a-f]+:	d0 90 90 90 90 90[ 	]+rcl[ 	]+BYTE PTR \[eax-0x6f6f6f70\],1
 *[0-9a-f]+:	d1 90 90 90 90 90[ 	]+rcl[ 	]+DWORD PTR \[eax-0x6f6f6f70\],1
 *[0-9a-f]+:	d2 90 90 90 90 90[ 	]+rcl[ 	]+BYTE PTR \[eax-0x6f6f6f70\],cl
 *[0-9a-f]+:	d3 90 90 90 90 90[ 	]+rcl[ 	]+DWORD PTR \[eax-0x6f6f6f70\],cl
 *[0-9a-f]+:	d4 90[ 	]+aam[ 	]+0xffffff90
 *[0-9a-f]+:	d5 90[ 	]+aad[ 	]+0xffffff90
 *[0-9a-f]+:	d7[ 	]+xlat[ 	]+(BYTE PTR )?(ds:)?\[ebx\]
 *[0-9a-f]+:	d8 90 90 90 90 90[ 	]+fcom[ 	]+DWORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	d9 90 90 90 90 90[ 	]+fst[ 	]+DWORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	da 90 90 90 90 90[ 	]+ficom[ 	]+DWORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	db 90 90 90 90 90[ 	]+fist[ 	]+DWORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	dc 90 90 90 90 90[ 	]+fcom[ 	]+QWORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	dd 90 90 90 90 90[ 	]+fst[ 	]+QWORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	de 90 90 90 90 90[ 	]+ficom[ 	]+WORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	df 90 90 90 90 90[ 	]+fist[ 	]+WORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	e0 90[ 	]+loopne[ 	]+(0x)?25c.*
 *[0-9a-f]+:	e1 90[ 	]+loope[ 	]+(0x)?25e.*
 *[0-9a-f]+:	e2 90[ 	]+loop[ 	]+(0x)?260.*
 *[0-9a-f]+:	e3 90[ 	]+jecxz[ 	]+(0x)?262.*
 *[0-9a-f]+:	e4 90[ 	]+in[ 	]+al,0x90
 *[0-9a-f]+:	e5 90[ 	]+in[ 	]+eax,0x90
 *[0-9a-f]+:	e6 90[ 	]+out[ 	]+0x90,al
 *[0-9a-f]+:	e7 90[ 	]+out[ 	]+0x90,eax
 *[0-9a-f]+:	e8 90 90 90 90[ 	]+call[ 	]+(0x)?9090936f.*
 *[0-9a-f]+:	e9 90 90 90 90[ 	]+jmp[ 	]+(0x)?90909374.*
 *[0-9a-f]+:	ea 90 90 90 90 90 90[ 	]+jmp[ 	]+0x9090:0x90909090
 *[0-9a-f]+:	eb 90[ 	]+jmp[ 	]+(0x)?27d.*
 *[0-9a-f]+:	ec[ 	]+in[ 	]+al,dx
 *[0-9a-f]+:	ed[ 	]+in[ 	]+eax,dx
 *[0-9a-f]+:	ee[ 	]+out[ 	]+dx,al
 *[0-9a-f]+:	ef[ 	]+out[ 	]+dx,eax
 *[0-9a-f]+:	f4[ 	]+hlt[ 	]*
 *[0-9a-f]+:	f5[ 	]+cmc[ 	]*
 *[0-9a-f]+:	f6 90 90 90 90 90[ 	]+not[ 	]+BYTE PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	f7 90 90 90 90 90[ 	]+not[ 	]+DWORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	f8[ 	]+clc[ 	]*
 *[0-9a-f]+:	f9[ 	]+stc[ 	]*
 *[0-9a-f]+:	fa[ 	]+cli[ 	]*
 *[0-9a-f]+:	fb[ 	]+sti[ 	]*
 *[0-9a-f]+:	fc[ 	]+cld[ 	]*
 *[0-9a-f]+:	fd[ 	]+std[ 	]*
 *[0-9a-f]+:	ff 90 90 90 90 90[ 	]+call[ 	]+DWORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 00 90 90 90 90 90[ 	]+lldt[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 01 90 90 90 90 90[ 	]+lgdtd[ 	]+\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 02 90 90 90 90 90[ 	]+lar[ 	]+edx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 03 90 90 90 90 90[ 	]+lsl[ 	]+edx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 06[ 	]+clts[ 	]*
 *[0-9a-f]+:	0f 08[ 	]+invd[ 	]*
 *[0-9a-f]+:	0f 09[ 	]+wbinvd[ 	]*
 *[0-9a-f]+:	0f 0b[ 	]+ud2a[ 	]*
 *[0-9a-f]+:	0f 20 d0[ 	]+mov[ 	]+eax,cr2
 *[0-9a-f]+:	0f 21 d0[ 	]+mov[ 	]+eax,db2
 *[0-9a-f]+:	0f 22 d0[ 	]+mov[ 	]+cr2,eax
 *[0-9a-f]+:	0f 23 d0[ 	]+mov[ 	]+db2,eax
 *[0-9a-f]+:	0f 24 d0[ 	]+mov[ 	]+eax,tr2
 *[0-9a-f]+:	0f 26 d0[ 	]+mov[ 	]+tr2,eax
 *[0-9a-f]+:	0f 30[ 	]+wrmsr[ 	]*
 *[0-9a-f]+:	0f 31[ 	]+rdtsc[ 	]*
 *[0-9a-f]+:	0f 32[ 	]+rdmsr[ 	]*
 *[0-9a-f]+:	0f 33[ 	]+rdpmc[ 	]*
 *[0-9a-f]+:	0f 40 90 90 90 90 90[ 	]+cmovo[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 41 90 90 90 90 90[ 	]+cmovno[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 42 90 90 90 90 90[ 	]+cmovb[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 43 90 90 90 90 90[ 	]+cmovae[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 44 90 90 90 90 90[ 	]+cmove[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 45 90 90 90 90 90[ 	]+cmovne[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 46 90 90 90 90 90[ 	]+cmovbe[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 47 90 90 90 90 90[ 	]+cmova[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 48 90 90 90 90 90[ 	]+cmovs[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 49 90 90 90 90 90[ 	]+cmovns[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 4a 90 90 90 90 90[ 	]+cmovp[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 4b 90 90 90 90 90[ 	]+cmovnp[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 4c 90 90 90 90 90[ 	]+cmovl[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 4d 90 90 90 90 90[ 	]+cmovge[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 4e 90 90 90 90 90[ 	]+cmovle[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 4f 90 90 90 90 90[ 	]+cmovg[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 60 90 90 90 90 90[ 	]+punpcklbw[ 	]+mm2,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 61 90 90 90 90 90[ 	]+punpcklwd[ 	]+mm2,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 62 90 90 90 90 90[ 	]+punpckldq[ 	]+mm2,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 63 90 90 90 90 90[ 	]+packsswb[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 64 90 90 90 90 90[ 	]+pcmpgtb[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 65 90 90 90 90 90[ 	]+pcmpgtw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 66 90 90 90 90 90[ 	]+pcmpgtd[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 67 90 90 90 90 90[ 	]+packuswb[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 68 90 90 90 90 90[ 	]+punpckhbw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 69 90 90 90 90 90[ 	]+punpckhwd[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 6a 90 90 90 90 90[ 	]+punpckhdq[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 6b 90 90 90 90 90[ 	]+packssdw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 6e 90 90 90 90 90[ 	]+movd[ 	]+mm2,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 6f 90 90 90 90 90[ 	]+movq[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 71 d0 90[ 	]+psrlw[ 	]+mm0,0x90
 *[0-9a-f]+:	0f 72 d0 90[ 	]+psrld[ 	]+mm0,0x90
 *[0-9a-f]+:	0f 73 d0 90[ 	]+psrlq[ 	]+mm0,0x90
 *[0-9a-f]+:	0f 74 90 90 90 90 90[ 	]+pcmpeqb[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 75 90 90 90 90 90[ 	]+pcmpeqw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 76 90 90 90 90 90[ 	]+pcmpeqd[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 77[ 	]+emms[ 	]*
 *[0-9a-f]+:	0f 7e 90 90 90 90 90[ 	]+movd[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],mm2
 *[0-9a-f]+:	0f 7f 90 90 90 90 90[ 	]+movq[ 	]+(QWORD PTR )?\[eax-0x6f6f6f70\],mm2
 *[0-9a-f]+:	0f 80 90 90 90 90[ 	]+jo[ 	]+909094e2 <foo\+0x909094e2>
 *[0-9a-f]+:	0f 81 90 90 90 90[ 	]+jno[ 	]+909094e8 <foo\+0x909094e8>
 *[0-9a-f]+:	0f 82 90 90 90 90[ 	]+jb[ 	]+909094ee <foo\+0x909094ee>
 *[0-9a-f]+:	0f 83 90 90 90 90[ 	]+jae[ 	]+909094f4 <foo\+0x909094f4>
 *[0-9a-f]+:	0f 84 90 90 90 90[ 	]+je[ 	]+909094fa <foo\+0x909094fa>
 *[0-9a-f]+:	0f 85 90 90 90 90[ 	]+jne[ 	]+90909500 <foo\+0x90909500>
 *[0-9a-f]+:	0f 86 90 90 90 90[ 	]+jbe[ 	]+90909506 <foo\+0x90909506>
 *[0-9a-f]+:	0f 87 90 90 90 90[ 	]+ja[ 	]+9090950c <foo\+0x9090950c>
 *[0-9a-f]+:	0f 88 90 90 90 90[ 	]+js[ 	]+90909512 <foo\+0x90909512>
 *[0-9a-f]+:	0f 89 90 90 90 90[ 	]+jns[ 	]+90909518 <foo\+0x90909518>
 *[0-9a-f]+:	0f 8a 90 90 90 90[ 	]+jp[ 	]+9090951e <foo\+0x9090951e>
 *[0-9a-f]+:	0f 8b 90 90 90 90[ 	]+jnp[ 	]+90909524 <foo\+0x90909524>
 *[0-9a-f]+:	0f 8c 90 90 90 90[ 	]+jl[ 	]+9090952a <foo\+0x9090952a>
 *[0-9a-f]+:	0f 8d 90 90 90 90[ 	]+jge[ 	]+90909530 <foo\+0x90909530>
 *[0-9a-f]+:	0f 8e 90 90 90 90[ 	]+jle[ 	]+90909536 <foo\+0x90909536>
 *[0-9a-f]+:	0f 8f 90 90 90 90[ 	]+jg[ 	]+9090953c <foo\+0x9090953c>
 *[0-9a-f]+:	0f 90 80 90 90 90 90[ 	]+seto[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 91 80 90 90 90 90[ 	]+setno[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 92 80 90 90 90 90[ 	]+setb[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 93 80 90 90 90 90[ 	]+setae[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 94 80 90 90 90 90[ 	]+sete[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 95 80 90 90 90 90[ 	]+setne[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 96 80 90 90 90 90[ 	]+setbe[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 97 80 90 90 90 90[ 	]+seta[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 98 80 90 90 90 90[ 	]+sets[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 99 80 90 90 90 90[ 	]+setns[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 9a 80 90 90 90 90[ 	]+setp[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 9b 80 90 90 90 90[ 	]+setnp[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 9c 80 90 90 90 90[ 	]+setl[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 9d 80 90 90 90 90[ 	]+setge[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 9e 80 90 90 90 90[ 	]+setle[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f 9f 80 90 90 90 90[ 	]+setg[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f a0[ 	]+push[ 	]+fs
 *[0-9a-f]+:	0f a1[ 	]+pop[ 	]+fs
 *[0-9a-f]+:	0f a2[ 	]+cpuid[ 	]*
 *[0-9a-f]+:	0f a3 90 90 90 90 90[ 	]+bt[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	0f a4 90 90 90 90 90 90[ 	]+shld[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx,0x90
 *[0-9a-f]+:	0f a5 90 90 90 90 90[ 	]+shld[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx,cl
 *[0-9a-f]+:	0f a8[ 	]+push[ 	]+gs
 *[0-9a-f]+:	0f a9[ 	]+pop[ 	]+gs
 *[0-9a-f]+:	0f aa[ 	]+rsm[ 	]*
 *[0-9a-f]+:	0f ab 90 90 90 90 90[ 	]+bts[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	0f ac 90 90 90 90 90 90[ 	]+shrd[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx,0x90
 *[0-9a-f]+:	0f ad 90 90 90 90 90[ 	]+shrd[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx,cl
 *[0-9a-f]+:	0f af 90 90 90 90 90[ 	]+imul[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f b0 90 90 90 90 90[ 	]+cmpxchg (BYTE PTR )?\[eax-0x6f6f6f70\],dl
 *[0-9a-f]+:	0f b1 90 90 90 90 90[ 	]+cmpxchg (DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	0f b2 90 90 90 90 90[ 	]+lss[ 	]+edx,(FWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f b3 90 90 90 90 90[ 	]+btr[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	0f b4 90 90 90 90 90[ 	]+lfs[ 	]+edx,(FWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f b5 90 90 90 90 90[ 	]+lgs[ 	]+edx,(FWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f b6 90 90 90 90 90[ 	]+movzx[ 	]+edx,BYTE PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f b7 90 90 90 90 90[ 	]+movzx[ 	]+edx,WORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f b9[ 	]+ud2b[ 	]*
 *[0-9a-f]+:	0f bb 90 90 90 90 90[ 	]+btc[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	0f bc 90 90 90 90 90[ 	]+bsf[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f bd 90 90 90 90 90[ 	]+bsr[ 	]+edx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f be 90 90 90 90 90[ 	]+movsx[ 	]+edx,BYTE PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f bf 90 90 90 90 90[ 	]+movsx[ 	]+edx,WORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f c0 90 90 90 90 90[ 	]+xadd[ 	]+(BYTE PTR )?\[eax-0x6f6f6f70\],dl
 *[0-9a-f]+:	0f c1 90 90 90 90 90[ 	]+xadd[ 	]+(DWORD PTR )?\[eax-0x6f6f6f70\],edx
 *[0-9a-f]+:	0f c8[ 	]+bswap[ 	]+eax
 *[0-9a-f]+:	0f c9[ 	]+bswap[ 	]+ecx
 *[0-9a-f]+:	0f ca[ 	]+bswap[ 	]+edx
 *[0-9a-f]+:	0f cb[ 	]+bswap[ 	]+ebx
 *[0-9a-f]+:	0f cc[ 	]+bswap[ 	]+esp
 *[0-9a-f]+:	0f cd[ 	]+bswap[ 	]+ebp
 *[0-9a-f]+:	0f ce[ 	]+bswap[ 	]+esi
 *[0-9a-f]+:	0f cf[ 	]+bswap[ 	]+edi
 *[0-9a-f]+:	0f d1 90 90 90 90 90[ 	]+psrlw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f d2 90 90 90 90 90[ 	]+psrld[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f d3 90 90 90 90 90[ 	]+psrlq[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f d5 90 90 90 90 90[ 	]+pmullw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f d8 90 90 90 90 90[ 	]+psubusb[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f d9 90 90 90 90 90[ 	]+psubusw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f db 90 90 90 90 90[ 	]+pand[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f dc 90 90 90 90 90[ 	]+paddusb[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f dd 90 90 90 90 90[ 	]+paddusw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f df 90 90 90 90 90[ 	]+pandn[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f e1 90 90 90 90 90[ 	]+psraw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f e2 90 90 90 90 90[ 	]+psrad[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f e5 90 90 90 90 90[ 	]+pmulhw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f e8 90 90 90 90 90[ 	]+psubsb[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f e9 90 90 90 90 90[ 	]+psubsw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f eb 90 90 90 90 90[ 	]+por[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f ec 90 90 90 90 90[ 	]+paddsb[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f ed 90 90 90 90 90[ 	]+paddsw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f ef 90 90 90 90 90[ 	]+pxor[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f f1 90 90 90 90 90[ 	]+psllw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f f2 90 90 90 90 90[ 	]+pslld[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f f3 90 90 90 90 90[ 	]+psllq[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f f5 90 90 90 90 90[ 	]+pmaddwd[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f f8 90 90 90 90 90[ 	]+psubb[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f f9 90 90 90 90 90[ 	]+psubw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f fa 90 90 90 90 90[ 	]+psubd[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f fc 90 90 90 90 90[ 	]+paddb[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f fd 90 90 90 90 90[ 	]+paddw[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	0f fe 90 90 90 90 90[ 	]+paddd[ 	]+mm2,(QWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 01 90 90 90 90 90[ 	]+add[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 03 90 90 90 90 90[ 	]+add[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 05 90 90[ 	]+add[ 	]+ax,0x9090
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	06[ 	]+push[ 	]+es
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	07[ 	]+pop[ 	]+es
 *[0-9a-f]+:	66 09 90 90 90 90 90[ 	]+or[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 0b 90 90 90 90 90[ 	]+or[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0d 90 90[ 	]+or[ 	]+ax,0x9090
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	0e[ 	]+push[ 	]+cs
 *[0-9a-f]+:	66 11 90 90 90 90 90[ 	]+adc[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 13 90 90 90 90 90[ 	]+adc[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 15 90 90[ 	]+adc[ 	]+ax,0x9090
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	16[ 	]+push[ 	]+ss
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	17[ 	]+pop[ 	]+ss
 *[0-9a-f]+:	66 19 90 90 90 90 90[ 	]+sbb[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 1b 90 90 90 90 90[ 	]+sbb[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 1d 90 90[ 	]+sbb[ 	]+ax,0x9090
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	1e[ 	]+push[ 	]+ds
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	1f[ 	]+pop[ 	]+ds
 *[0-9a-f]+:	66 21 90 90 90 90 90[ 	]+and[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 23 90 90 90 90 90[ 	]+and[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 25 90 90[ 	]+and[ 	]+ax,0x9090
 *[0-9a-f]+:	66 29 90 90 90 90 90[ 	]+sub[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 2b 90 90 90 90 90[ 	]+sub[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 2d 90 90[ 	]+sub[ 	]+ax,0x9090
 *[0-9a-f]+:	66 31 90 90 90 90 90[ 	]+xor[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 33 90 90 90 90 90[ 	]+xor[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 35 90 90[ 	]+xor[ 	]+ax,0x9090
 *[0-9a-f]+:	66 39 90 90 90 90 90[ 	]+cmp[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 3b 90 90 90 90 90[ 	]+cmp[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 3d 90 90[ 	]+cmp[ 	]+ax,0x9090
 *[0-9a-f]+:	66 40[ 	]+inc[ 	]+ax
 *[0-9a-f]+:	66 41[ 	]+inc[ 	]+cx
 *[0-9a-f]+:	66 42[ 	]+inc[ 	]+dx
 *[0-9a-f]+:	66 43[ 	]+inc[ 	]+bx
 *[0-9a-f]+:	66 44[ 	]+inc[ 	]+sp
 *[0-9a-f]+:	66 45[ 	]+inc[ 	]+bp
 *[0-9a-f]+:	66 46[ 	]+inc[ 	]+si
 *[0-9a-f]+:	66 47[ 	]+inc[ 	]+di
 *[0-9a-f]+:	66 48[ 	]+dec[ 	]+ax
 *[0-9a-f]+:	66 49[ 	]+dec[ 	]+cx
 *[0-9a-f]+:	66 4a[ 	]+dec[ 	]+dx
 *[0-9a-f]+:	66 4b[ 	]+dec[ 	]+bx
 *[0-9a-f]+:	66 4c[ 	]+dec[ 	]+sp
 *[0-9a-f]+:	66 4d[ 	]+dec[ 	]+bp
 *[0-9a-f]+:	66 4e[ 	]+dec[ 	]+si
 *[0-9a-f]+:	66 4f[ 	]+dec[ 	]+di
 *[0-9a-f]+:	66 50[ 	]+push[ 	]+ax
 *[0-9a-f]+:	66 51[ 	]+push[ 	]+cx
 *[0-9a-f]+:	66 52[ 	]+push[ 	]+dx
 *[0-9a-f]+:	66 53[ 	]+push[ 	]+bx
 *[0-9a-f]+:	66 54[ 	]+push[ 	]+sp
 *[0-9a-f]+:	66 55[ 	]+push[ 	]+bp
 *[0-9a-f]+:	66 56[ 	]+push[ 	]+si
 *[0-9a-f]+:	66 57[ 	]+push[ 	]+di
 *[0-9a-f]+:	66 58[ 	]+pop[ 	]+ax
 *[0-9a-f]+:	66 59[ 	]+pop[ 	]+cx
 *[0-9a-f]+:	66 5a[ 	]+pop[ 	]+dx
 *[0-9a-f]+:	66 5b[ 	]+pop[ 	]+bx
 *[0-9a-f]+:	66 5c[ 	]+pop[ 	]+sp
 *[0-9a-f]+:	66 5d[ 	]+pop[ 	]+bp
 *[0-9a-f]+:	66 5e[ 	]+pop[ 	]+si
 *[0-9a-f]+:	66 5f[ 	]+pop[ 	]+di
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	60[ 	]+pusha[ 	]*
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	61[ 	]+popa[ 	]*
 *[0-9a-f]+:	66 62 90 90 90 90 90[ 	]+bound[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 68 90 90[ 	]+push[ 	]+0x9090
 *[0-9a-f]+:	66 69 90 90 90 90 90 90 90[ 	]+imul[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\],0x9090
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	6a 90[ 	]+push[ 	]+0xffffff90
 *[0-9a-f]+:	66 6b 90 90 90 90 90 90[ 	]+imul[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\],0xffffff90
 *[0-9a-f]+:	66 6d[ 	]+ins[ 	]+WORD PTR es:\[edi\],dx
 *[0-9a-f]+:	66 6f[ 	]+outs[ 	]+dx,WORD PTR ds:\[esi\]
 *[0-9a-f]+:	66 81 90 90 90 90 90 90 90[ 	]+adc[ 	]+WORD PTR \[eax-0x6f6f6f70\],0x9090
 *[0-9a-f]+:	66 83 90 90 90 90 90 90[ 	]+adc[ 	]+WORD PTR \[eax-0x6f6f6f70\],0xffffff90
 *[0-9a-f]+:	66 85 90 90 90 90 90[ 	]+test[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 87 90 90 90 90 90[ 	]+xchg[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 89 90 90 90 90 90[ 	]+mov[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 8b 90 90 90 90 90[ 	]+mov[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	8c 90 90 90 90 90[ 	]+mov[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],ss
 *[0-9a-f]+:	66 8d 90 90 90 90 90[ 	]+lea[ 	]+dx,\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 8f 80 90 90 90 90[ 	]+pop[ 	]+WORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 91[ 	]+xchg[ 	]+cx,ax
 *[0-9a-f]+:	66 92[ 	]+xchg[ 	]+dx,ax
 *[0-9a-f]+:	66 93[ 	]+xchg[ 	]+bx,ax
 *[0-9a-f]+:	66 94[ 	]+xchg[ 	]+sp,ax
 *[0-9a-f]+:	66 95[ 	]+xchg[ 	]+bp,ax
 *[0-9a-f]+:	66 96[ 	]+xchg[ 	]+si,ax
 *[0-9a-f]+:	66 97[ 	]+xchg[ 	]+di,ax
 *[0-9a-f]+:	66 98[ 	]+cbw[ 	]*
 *[0-9a-f]+:	66 99[ 	]+cwd[ 	]*
 *[0-9a-f]+:	66 9a 90 90 90 90[ 	]+call[ 	]+0x9090:0x9090
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	9c[ 	]+pushf[ 	]*
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	9d[ 	]+popf[ 	]*
 *[0-9a-f]+:	66 a1 90 90 90 90[ 	]+mov[ 	]+ax,ds:0x90909090
 *[0-9a-f]+:	66 a3 90 90 90 90[ 	]+mov[ 	]+ds:0x90909090,ax
 *[0-9a-f]+:	66 a5[ 	]+movs[ 	]+WORD PTR es:\[edi\],(WORD PTR )?ds:\[esi\]
 *[0-9a-f]+:	66 a7[ 	]+cmps[ 	]+WORD PTR ds:\[esi\],(WORD PTR )?es:\[edi\]
 *[0-9a-f]+:	66 a9 90 90[ 	]+test[ 	]+ax,0x9090
 *[0-9a-f]+:	66 ab[ 	]+stos[ 	]+WORD PTR es:\[edi\](,ax)?
 *[0-9a-f]+:	66 ad[ 	]+lods[ 	]+(ax,)?WORD PTR ds:\[esi\]
 *[0-9a-f]+:	66 af[ 	]+scas[ 	]+(ax,)?WORD PTR es:\[edi\]
 *[0-9a-f]+:	66 b8 90 90[ 	]+mov[ 	]+ax,0x9090
 *[0-9a-f]+:	66 b9 90 90[ 	]+mov[ 	]+cx,0x9090
 *[0-9a-f]+:	66 ba 90 90[ 	]+mov[ 	]+dx,0x9090
 *[0-9a-f]+:	66 bb 90 90[ 	]+mov[ 	]+bx,0x9090
 *[0-9a-f]+:	66 bc 90 90[ 	]+mov[ 	]+sp,0x9090
 *[0-9a-f]+:	66 bd 90 90[ 	]+mov[ 	]+bp,0x9090
 *[0-9a-f]+:	66 be 90 90[ 	]+mov[ 	]+si,0x9090
 *[0-9a-f]+:	66 bf 90 90[ 	]+mov[ 	]+di,0x9090
 *[0-9a-f]+:	66 c1 90 90 90 90 90 90[ 	]+rcl[ 	]+WORD PTR \[eax-0x6f6f6f70\],0x90
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	c2 90 90[ 	]+ret[ 	]+0x9090
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	c3[ 	]+ret[ 	]*
 *[0-9a-f]+:	66 c4 90 90 90 90 90[ 	]+les[ 	]+dx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 c5 90 90 90 90 90[ 	]+lds[ 	]+dx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 c7 80 90 90 90 90 90 90[ 	]+mov[ 	]+WORD PTR \[eax-0x6f6f6f70\],0x9090
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	c8 90 90 90[ 	]+enter[ 	]+0x9090,0x90
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	c9[ 	]+leave[ 	]*
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	ca 90 90[ 	]+lret[ 	]+0x9090
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	cb[ 	]+lret[ 	]*
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	cf[ 	]+iret[ 	]*
 *[0-9a-f]+:	66 d1 90 90 90 90 90[ 	]+rcl[ 	]+WORD PTR \[eax-0x6f6f6f70\],1
 *[0-9a-f]+:	66 d3 90 90 90 90 90[ 	]+rcl[ 	]+WORD PTR \[eax-0x6f6f6f70\],cl
 *[0-9a-f]+:	66 e5 90[ 	]+in[ 	]+ax,0x90
 *[0-9a-f]+:	66 e7 90[ 	]+out[ 	]+0x90,ax
 *[0-9a-f]+:	66 e8 8f 90[ 	]+call[ 	]+(0x)?9918.*
 *[0-9a-f]+:	66 ea 90 90 90 90[ 	]+jmp[ 	]+0x9090:0x9090
 *[0-9a-f]+:	66 ed[ 	]+in[ 	]+ax,dx
 *[0-9a-f]+:	66 ef[ 	]+out[ 	]+dx,ax
 *[0-9a-f]+:	66 f7 90 90 90 90 90[ 	]+not[ 	]+WORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 ff 90 90 90 90 90[ 	]+call[ 	]+WORD PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 02 90 90 90 90 90[ 	]+lar[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 03 90 90 90 90 90[ 	]+lsl[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 40 90 90 90 90 90[ 	]+cmovo[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 41 90 90 90 90 90[ 	]+cmovno[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 42 90 90 90 90 90[ 	]+cmovb[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 43 90 90 90 90 90[ 	]+cmovae[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 44 90 90 90 90 90[ 	]+cmove[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 45 90 90 90 90 90[ 	]+cmovne[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 46 90 90 90 90 90[ 	]+cmovbe[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 47 90 90 90 90 90[ 	]+cmova[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 48 90 90 90 90 90[ 	]+cmovs[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 49 90 90 90 90 90[ 	]+cmovns[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 4a 90 90 90 90 90[ 	]+cmovp[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 4b 90 90 90 90 90[ 	]+cmovnp[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 4c 90 90 90 90 90[ 	]+cmovl[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 4d 90 90 90 90 90[ 	]+cmovge[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 4e 90 90 90 90 90[ 	]+cmovle[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f 4f 90 90 90 90 90[ 	]+cmovg[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	0f a0[ 	]+push[ 	]+fs
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	0f a1[ 	]+pop[ 	]+fs
 *[0-9a-f]+:	66 0f a3 90 90 90 90 90[ 	]+bt[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 0f a4 90 90 90 90 90 90[ 	]+shld[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx,0x90
 *[0-9a-f]+:	66 0f a5 90 90 90 90 90[ 	]+shld[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx,cl
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	0f a8[ 	]+push[ 	]+gs
 *[0-9a-f]+:	66[ 	]+data16
 *[0-9a-f]+:	0f a9[ 	]+pop[ 	]+gs
 *[0-9a-f]+:	66 0f ab 90 90 90 90 90[ 	]+bts[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 0f ac 90 90 90 90 90 90[ 	]+shrd[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx,0x90
 *[0-9a-f]+:	66 0f ad 90 90 90 90 90[ 	]+shrd[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx,cl
 *[0-9a-f]+:	66 0f af 90 90 90 90 90[ 	]+imul[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f b1 90 90 90 90 90[ 	]+cmpxchg (WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 0f b2 90 90 90 90 90[ 	]+lss[ 	]+dx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f b3 90 90 90 90 90[ 	]+btr[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 0f b4 90 90 90 90 90[ 	]+lfs[ 	]+dx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f b5 90 90 90 90 90[ 	]+lgs[ 	]+dx,(DWORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f b6 90 90 90 90 90[ 	]+movzx[ 	]+dx,BYTE PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f bb 90 90 90 90 90[ 	]+btc[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 0f bc 90 90 90 90 90[ 	]+bsf[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f bd 90 90 90 90 90[ 	]+bsr[ 	]+dx,(WORD PTR )?\[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f be 90 90 90 90 90[ 	]+movsx[ 	]+dx,BYTE PTR \[eax-0x6f6f6f70\]
 *[0-9a-f]+:	66 0f c1 90 90 90 90 90[ 	]+xadd[ 	]+(WORD PTR )?\[eax-0x6f6f6f70\],dx
 *[0-9a-f]+:	66 90[ 	]+xchg[ 	]+ax,ax
 *[0-9a-f]+:	0f 00 c0[ 	]+sldt[ 	]+eax
 *[0-9a-f]+:	66 0f 00 c0[ 	]+sldt[ 	]+ax
 *[0-9a-f]+:	0f 00 00[ 	]+sldt[ 	]+(WORD PTR )?\[eax\]
 *[0-9a-f]+:	0f 01 e0[ 	]+smsw[ 	]+eax
 *[0-9a-f]+:	66 0f 01 e0[ 	]+smsw[ 	]+ax
 *[0-9a-f]+:	0f 01 20[ 	]+smsw[ 	]+(WORD PTR )?\[eax\]
 *[0-9a-f]+:	0f 00 c8[ 	]+str[ 	]+eax
 *[0-9a-f]+:	66 0f 00 c8[ 	]+str[ 	]+ax
 *[0-9a-f]+:	0f 00 08[ 	]+str[ 	]+(WORD PTR )?\[eax\]
 *[0-9a-f]+:	0f ad d0 [ 	]*shrd[ 	]+eax,edx,cl
 *[0-9a-f]+:	0f a5 d0 [ 	]*shld[ 	]+eax,edx,cl
 *[0-9a-f]+:	85 c3 [ 	]*test[ 	]+ebx,eax
 *[0-9a-f]+:	85 d8 [ 	]*test[ 	]+eax,ebx
 *[0-9a-f]+:	85 18 [ 	]*test[ 	]+(DWORD PTR )?\[eax\],ebx
#pass
	\.\.\.
