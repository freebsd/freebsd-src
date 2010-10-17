 .data
foodata: .word 42
 .text
footext:
	.text
	.global movlmemimm
movlmemimm:
	mov.b 0,#0
	mov.w 255,#65535
	mov.w 128,#32768
	mov.b 127,#32767
	mov.w 1,#1
	mov.w 81,#64681
	mov.w 247,#42230
	mov.b 84,#16647
	.text
	.global movhmemimm
movhmemimm:
	mov.b 0x7f00+0,#0
	mov.w 0x7f00+255,#65535
	mov.w 0x7f00+128,#32768
	mov.b 0x7f00+127,#32767
	mov.w 0x7f00+1,#1
	mov.b 0x7f00+165,#1944
	mov.w 0x7f00+186,#11517
	mov.b 0x7f00+63,#25556
	.text
	.global movlgrmem
movlgrmem:
	mov.b r0,0
	mov.w r7,255
	mov.w r4,128
	mov.b r3,127
	mov.w r1,1
	mov.w r6,179
	mov.w r0,183
	mov.b r3,41
	.text
	.global movhgrmem
movhgrmem:
	mov.b r0,0x7f00+0
	mov.w r7,0x7f00+255
	mov.w r4,0x7f00+128
	mov.b r3,0x7f00+127
	mov.w r1,0x7f00+1
	mov.b r2,0x7f00+114
	mov.w r2,0x7f00+210
	mov.w r5,0x7f00+181
	.text
	.global movlmemgr
movlmemgr:
	mov.b 0,r0
	mov.w 255,r7
	mov.w 128,r4
	mov.b 127,r3
	mov.w 1,r1
	mov.w 137,r0
	mov.w 26,r0
	mov.b 127,r4
	.text
	.global movhmemgr
movhmemgr:
	mov.b 0x7f00+0,r0
	mov.w 0x7f00+255,r7
	mov.w 0x7f00+128,r4
	mov.b 0x7f00+127,r3
	mov.w 0x7f00+1,r1
	mov.w 0x7f00+98,r3
	mov.w 0x7f00+135,r7
	mov.b 0x7f00+229,r2
	.text
	.global movgrgri
movgrgri:
	mov.b r0,(r0)
	mov.w r7,(r15)
	mov.w r4,(r8)
	mov.b r3,(r7)
	mov.w r1,(r1)
	mov.w r6,(r4)
	mov.b r0,(r12)
	mov.w r5,(r9)
	.text
	.global movgrgripostinc
movgrgripostinc:
	mov.b r0,(r0++)
	mov.w r7,(r15++)
	mov.w r4,(r8++)
	mov.b r3,(r7++)
	mov.w r1,(r1++)
	mov.w r4,(r8++)
	mov.w r3,(r12++)
	mov.b r6,(r4++)
	.text
	.global movgrgripredec
movgrgripredec:
	mov.b r0,(--r0)
	mov.w r7,(--r15)
	mov.w r4,(--r8)
	mov.b r3,(--r7)
	mov.w r1,(--r1)
	mov.w r5,(--r9)
	mov.w r4,(--r14)
	mov.b r4,(--r7)
	.text
	.global movgrigr
movgrigr:
	mov.b (r0),r0
	mov.w (r15),r7
	mov.w (r8),r4
	mov.b (r7),r3
	mov.w (r1),r1
	mov.w (r4),r3
	mov.b (r3),r6
	mov.w (r7),r0
	.text
	.global movgripostincgr
movgripostincgr:
	mov.b (r0++),r0
	mov.w (r15++),r7
	mov.w (r8++),r4
	mov.b (r7++),r3
	mov.w (r1++),r1
	mov.w (r12++),r5
	mov.b (r4++),r2
	mov.b (r11++),r6
	.text
	.global movgripredecgr
movgripredecgr:
	mov.b (--r0),r0
	mov.w (--r15),r7
	mov.w (--r8),r4
	mov.b (--r7),r3
	mov.w (--r1),r1
	mov.b (--r8),r3
	mov.b (--r11),r4
	mov.w (--r1),r6
	.text
	.global movgrgrii
movgrgrii:
	mov.b r0,(r0,0)
	mov.w r7,(r15,-1)
	mov.w r4,(r8,-2048)
	mov.b r3,(r7,2047)
	mov.w r1,(r1,1)
	mov.w r6,(r8,-452)
	mov.w r4,(r11,572)
	mov.b r1,(r1,-1718)
	.text
	.global movgrgriipostinc
movgrgriipostinc:
	mov.b r0,(r0++,0)
	mov.w r7,(r15++,-1)
	mov.w r4,(r8++,-2048)
	mov.b r3,(r7++,2047)
	mov.w r1,(r1++,1)
	mov.w r6,(r0++,-64)
	mov.b r7,(r15++,1060)
	mov.b r0,(r7++,847)
	.text
	.global movgrgriipredec
movgrgriipredec:
	mov.b r0,(--r0,0)
	mov.w r7,(--r15,-1)
	mov.w r4,(--r8,-2048)
	mov.b r3,(--r7,2047)
	mov.w r1,(--r1,1)
	mov.w r0,(--r15,1780)
	mov.w r6,(--r1,1506)
	mov.w r7,(--r3,-2033)
	.text
	.global movgriigr
movgriigr:
	mov.b (r0,0),r0
	mov.w (r15,-1),r7
	mov.w (r8,-2048),r4
	mov.b (r7,2047),r3
	mov.w (r1,1),r1
	mov.w (r7,1948),r5
	mov.b (r3,-844),r4
	mov.w (r15,1704),r0
	.text
	.global movgriipostincgr
movgriipostincgr:
	mov.b (r0++,0),r0
	mov.w (r15++,-1),r7
	mov.w (r8++,-2048),r4
	mov.b (r7++,2047),r3
	mov.w (r1++,1),r1
	mov.w (r2++,-176),r7
	mov.w (r8++,1389),r4
	mov.b (r3++,47),r0
	.text
	.global movgriipredecgr
movgriipredecgr:
	mov.b (--r0,0),r0
	mov.w (--r15,-1),r7
	mov.w (--r8,-2048),r4
	mov.b (--r7,2047),r3
	mov.w (--r1,1),r1
	mov.b (--r8,1004),r4
	mov.w (--r14,-1444),r2
	mov.b (--r5,-927),r4
	.text
	.global movgrgr
movgrgr:
	mov r0,r0
	mov r15,r15
	mov r8,r8
	mov r7,r7
	mov r1,r1
	mov r9,r14
	mov r7,r15
	mov r12,r15
	.text
	.global movimm8
movimm8:
	mov Rx,#0
	mov Rx,#255
	mov Rx,#128
	mov Rx,#127
	mov Rx,#1
	mov Rx,#136
	mov Rx,#83
	mov Rx,#104
	.text
	.global movwimm8
movwimm8:
	mov.w Rx,#0
	mov.w Rx,#255
	mov.w Rx,#128
	mov.w Rx,#127
	mov.w Rx,#1
	mov.w Rx,#92
	mov.w Rx,#97
	mov.w Rx,#4
	.text
	.global movgrimm8
movgrimm8:
	mov r0,#0
	mov r7,#255
	mov r4,#128
	mov r3,#127
	mov r1,#1
	mov r2,#206
	mov r4,#55
	mov r2,#3
	.text
	.global movwgrimm8
movwgrimm8:
	mov.w r0,#0
	mov.w r7,#255
	mov.w r4,#128
	mov.w r3,#127
	mov.w r1,#1
	mov.w r4,#243
	mov.w r3,#55
	mov.w r2,#108
	.text
	.global movgrimm16
movgrimm16:
	mov r0,#0
	mov r15,#65535
	mov r8,#32768
	mov r7,#32767
	mov r1,#1
	mov r4,#20066
	mov r3,#7190
	mov r2,#15972
	.text
	.global movwgrimm16
movwgrimm16:
	mov.w r0,#0
	mov.w r15,#65535
	mov.w r8,#32768
	mov.w r7,#32767
	mov.w r1,#1
	mov.w r6,#16648
	mov.w r8,#26865
	mov.w r10,#20010
	.text
	.global movlowgr
movlowgr:
	mov.b r0,RxL
	mov.b r15,RxL
	mov.b r8,RxL
	mov.b r7,RxL
	mov.b r1,RxL
	mov.b r11,RxL
	mov.b r5,RxL
	mov.b r2,RxL
	.text
	.global movhighgr
movhighgr:
	mov.b r0,RxH
	mov.b r15,RxH
	mov.b r8,RxH
	mov.b r7,RxH
	mov.b r1,RxH
	mov.b r2,RxH
	mov.b r7,RxH
	mov.b r2,RxH
	.text
	.global movfgrgri
movfgrgri:
	movf.b r0,(r0)
	movf.w r7,(r15)
	movf.w r4,(r8)
	movf.b r3,(r7)
	movf.w r1,(r1)
	movf.b r6,(r15)
	movf.b r1,(r10)
	movf.b r6,(r1)
	.text
	.global movfgrgripostinc
movfgrgripostinc:
	movf.b r0,(r0++)
	movf.w r7,(r15++)
	movf.w r4,(r8++)
	movf.b r3,(r7++)
	movf.w r1,(r1++)
	movf.b r2,(r5++)
	movf.w r5,(r10++)
	movf.w r7,(r5++)
	.text
	.global movfgrgripredec
movfgrgripredec:
	movf.b r0,(--r0)
	movf.w r7,(--r15)
	movf.w r4,(--r8)
	movf.b r3,(--r7)
	movf.w r1,(--r1)
	movf.w r6,(--r10)
	movf.b r1,(--r14)
	movf.w r3,(--r7)
	.text
	.global movfgrigr
movfgrigr:
	movf.b (r0),r0
	movf.w (r15),r7
	movf.w (r8),r4
	movf.b (r7),r3
	movf.w (r1),r1
	movf.b (r5),r4
	movf.b (r3),r4
	movf.w (r12),r3
	.text
	.global movfgripostincgr
movfgripostincgr:
	movf.b (r0++),r0
	movf.w (r15++),r7
	movf.w (r8++),r4
	movf.b (r7++),r3
	movf.w (r1++),r1
	movf.b (r9++),r5
	movf.w (r10++),r4
	movf.b (r9++),r1
	.text
	.global movfgripredecgr
movfgripredecgr:
	movf.b (--r0),r0
	movf.w (--r15),r7
	movf.w (--r8),r4
	movf.b (--r7),r3
	movf.w (--r1),r1
	movf.b (--r0),r2
	movf.w (--r11),r2
	movf.b (--r10),r5
	.text
	.global movfgrgrii
movfgrgrii:
	movf.b r0,(r8,r0,0)
	movf.w r7,(r15,r15,-1)
	movf.w r4,(r12,r8,-2048)
	movf.b r3,(r11,r7,2047)
	movf.w r1,(r9,r1,1)
	movf.b r7,(r15,r0,1473)
	movf.w r2,(r8,r9,-1522)
	movf.w r2,(r13,r1,480)
	.text
	.global movfgrgriipostinc
movfgrgriipostinc:
	movf.b r0,(r8,r0++,0)
	movf.w r7,(r15,r15++,-1)
	movf.w r4,(r12,r8++,-2048)
	movf.b r3,(r11,r7++,2047)
	movf.w r1,(r9,r1++,1)
	movf.b r1,(r8,r2++,1398)
	movf.w r4,(r8,r9++,-778)
	movf.w r1,(r13,r14++,1564)
	.text
	.global movfgrgriipredec
movfgrgriipredec:
	movf.b r0,(r8,--r0,0)
	movf.w r7,(r15,--r15,-1)
	movf.w r4,(r12,--r8,-2048)
	movf.b r3,(r11,--r7,2047)
	movf.w r1,(r9,--r1,1)
	movf.b r6,(r8,--r7,254)
	movf.w r5,(r12,--r12,1673)
	movf.b r0,(r8,--r10,-38)
	.text
	.global movfgriigr
movfgriigr:
	movf.b (r8,r0,0),r0
	movf.w (r15,r15,-1),r7
	movf.w (r12,r8,-2048),r4
	movf.b (r11,r7,2047),r3
	movf.w (r9,r1,1),r1
	movf.w (r15,r2,-1636),r3
	movf.w (r14,r12,1626),r1
	movf.b (r11,r14,1540),r0
	.text
	.global movfgriipostincgr
movfgriipostincgr:
	movf.b (r8,r0++,0),r0
	movf.w (r15,r15++,-1),r7
	movf.w (r12,r8++,-2048),r4
	movf.b (r11,r7++,2047),r3
	movf.w (r9,r1++,1),r1
	movf.b (r15,r13++,466),r3
	movf.b (r11,r11++,250),r4
	movf.b (r10,r10++,-1480),r7
	.text
	.global movfgriipredecgr
movfgriipredecgr:
	movf.b (r8,--r0,0),r0
	movf.w (r15,--r15,-1),r7
	movf.w (r12,--r8,-2048),r4
	movf.b (r11,--r7,2047),r3
	movf.w (r9,--r1,1),r1
	movf.b (r13,--r10,-608),r0
	movf.b (r9,--r11,831),r7
	movf.w (r15,--r15,-2036),r6
	.text
	.global maskgrgr
maskgrgr:
	mask r0,r0
	mask r15,r15
	mask r8,r8
	mask r7,r7
	mask r1,r1
	mask r4,r0
	mask r6,r11
	mask r8,r4
	.text
	.global maskgrimm16
maskgrimm16:
	mask r0,#0
	mask r15,#65535
	mask r8,#32768
	mask r7,#32767
	mask r1,#1
	mask r7,#18153
	mask r15,#7524
	mask r14,#34349
	.text
	.global pushgr
pushgr:
	push r0
	push r15
	push r8
	push r7
	push r1
	push r9
	push r4
	push r3
	.text
	.global popgr
popgr:
	pop r0
	pop r15
	pop r8
	pop r7
	pop r1
	pop r3
	pop r2
	pop r12
	.text
	.global swpn
swpn:
	swpn r0
	swpn r15
	swpn r8
	swpn r7
	swpn r1
	swpn r15
	swpn r4
	swpn r3
	.text
	.global swpb
swpb:
	swpb r0
	swpb r15
	swpb r8
	swpb r7
	swpb r1
	swpb r2
	swpb r12
	swpb r2
	.text
	.global swpw
swpw:
	swpw r0,r0
	swpw r15,r15
	swpw r8,r8
	swpw r7,r7
	swpw r1,r1
	swpw r12,r4
	swpw r8,r2
	swpw r5,r13
	.text
	.global andgrgr
andgrgr:
	and r0,r0
	and r15,r15
	and r8,r8
	and r7,r7
	and r1,r1
	and r2,r2
	and r15,r5
	and r7,r5
	.text
	.global andimm8
andimm8:
	and Rx,#0
	and Rx,#255
	and Rx,#128
	and Rx,#127
	and Rx,#1
	and Rx,#206
	and Rx,#11
	and Rx,#232
	.text
	.global andgrimm16
andgrimm16:
	and r0,#0
	and r15,#65535
	and r8,#32768
	and r7,#32767
	and r1,#1
	and r10,#17229
	and r11,#61451
	and r5,#46925
	.text
	.global orgrgr
orgrgr:
	or r0,r0
	or r15,r15
	or r8,r8
	or r7,r7
	or r1,r1
	or r3,r5
	or r14,r15
	or r5,r12
	.text
	.global orimm8
orimm8:
	or Rx,#0
	or Rx,#255
	or Rx,#128
	or Rx,#127
	or Rx,#1
	or Rx,#4
	or Rx,#38
	or Rx,#52
	.text
	.global orgrimm16
orgrimm16:
	or r0,#0
	or r15,#65535
	or r8,#32768
	or r7,#32767
	or r1,#1
	or r2,#64563
	or r2,#18395
	or r1,#63059
	.text
	.global xorgrgr
xorgrgr:
	xor r0,r0
	xor r15,r15
	xor r8,r8
	xor r7,r7
	xor r1,r1
	xor r14,r1
	xor r9,r9
	xor r12,r8
	.text
	.global xorimm8
xorimm8:
	xor Rx,#0
	xor Rx,#255
	xor Rx,#128
	xor Rx,#127
	xor Rx,#1
	xor Rx,#208
	xor Rx,#126
	xor Rx,#55
	.text
	.global xorgrimm16
xorgrimm16:
	xor r0,#0
	xor r15,#65535
	xor r8,#32768
	xor r7,#32767
	xor r1,#1
	xor r15,#56437
	xor r3,#901
	xor r2,#37017
	.text
	.global notgr
notgr:
	not r0
	not r15
	not r8
	not r7
	not r1
	not r4
	not r3
	not r3
	.text
	.global addgrgr
addgrgr:
	add r0,r0
	add r15,r15
	add r8,r8
	add r7,r7
	add r1,r1
	add r12,r7
	add r1,r10
	add r14,r14
	.text
	.global addgrimm4
addgrimm4:
	add r0,#0
	add r15,#15
	add r8,#8
	add r7,#7
	add r1,#1
	add r7,#0
	add r10,#9
	add r7,#8
	.text
	.global addimm8
addimm8:
	add Rx,#0
	add Rx,#255
	add Rx,#128
	add Rx,#127
	add Rx,#1
	add Rx,#25
	add Rx,#247
	add Rx,#221
	.text
	.global addgrimm16
addgrimm16:
	add r0,#0
	add r15,#255
	add r8,#128
	add r7,#127
	add r1,#1
	add r3,#99
	add r0,#15
	add r7,#214
	.text
	.global adcgrgr
adcgrgr:
	adc r0,r0
	adc r15,r15
	adc r8,r8
	adc r7,r7
	adc r1,r1
	adc r2,r13
	adc r14,r10
	adc r2,r15
	.text
	.global adcgrimm4
adcgrimm4:
	adc r0,#0
	adc r15,#15
	adc r8,#8
	adc r7,#7
	adc r1,#1
	adc r15,#1
	adc r1,#3
	adc r6,#11
	.text
	.global adcimm8
adcimm8:
	adc Rx,#0
	adc Rx,#255
	adc Rx,#128
	adc Rx,#127
	adc Rx,#1
	adc Rx,#225
	adc Rx,#75
	adc Rx,#18
	.text
	.global adcgrimm16
adcgrimm16:
	adc r0,#0
	adc r15,#65535
	adc r8,#32768
	adc r7,#32767
	adc r1,#1
	adc r13,#63129
	adc r3,#23795
	adc r11,#49245
	.text
	.global subgrgr
subgrgr:
	sub r0,r0
	sub r15,r15
	sub r8,r8
	sub r7,r7
	sub r1,r1
	sub r8,r8
	sub r9,r9
	sub r9,r15
	.text
	.global subgrimm4
subgrimm4:
	sub r0,#0
	sub r15,#15
	sub r8,#8
	sub r7,#7
	sub r1,#1
	sub r2,#15
	sub r12,#9
	sub r8,#4
	.text
	.global subimm8
subimm8:
	sub Rx,#0
	sub Rx,#255
	sub Rx,#128
	sub Rx,#127
	sub Rx,#1
	sub Rx,#205
	sub Rx,#153
	sub Rx,#217
	.text
	.global subgrimm16
subgrimm16:
	sub r0,#0
	sub r15,#65535
	sub r8,#32768
	sub r7,#32767
	sub r1,#1
	sub r3,#51895
	sub r11,#23617
	sub r10,#7754
	.text
	.global sbcgrgr
sbcgrgr:
	sbc r0,r0
	sbc r15,r15
	sbc r8,r8
	sbc r7,r7
	sbc r1,r1
	sbc r11,r2
	sbc r9,r1
	sbc r4,r15
	.text
	.global sbcgrimm4
sbcgrimm4:
	sbc r0,#0
	sbc r15,#15
	sbc r8,#8
	sbc r7,#7
	sbc r1,#1
	sbc r10,#11
	sbc r11,#10
	sbc r13,#10
	.text
	.global sbcgrimm8
sbcgrimm8:
	sbc Rx,#0
	sbc Rx,#255
	sbc Rx,#128
	sbc Rx,#127
	sbc Rx,#1
	sbc Rx,#137
	sbc Rx,#224
	sbc Rx,#156
	.text
	.global sbcgrimm16
sbcgrimm16:
	sbc r0,#0
	sbc r15,#65535
	sbc r8,#32768
	sbc r7,#32767
	sbc r1,#1
	sbc r0,#32507
	sbc r7,#8610
	sbc r14,#20373
	.text
	.global incgr
incgr:
	inc r0
	inc r15
	inc r8
	inc r7
	inc r1
	inc r13
	inc r1
	inc r11
	.text
	.global incgrimm2
incgrimm2:
	inc r0,#0
	inc r15,#3
	inc r8,#2
	inc r7,#1
	inc r1,#1
	inc r14,#1
	inc r5,#0
	inc r12,#3
	.text
	.global decgr
decgr:
	dec r0
	dec r15
	dec r8
	dec r7
	dec r1
	dec r12
	dec r8
	dec r10
	.text
	.global decgrimm2
decgrimm2:
	dec r0,#0
	dec r15,#3
	dec r8,#2
	dec r7,#1
	dec r1,#1
	dec r5,#0
	dec r13,#0
	dec r13,#2
	.text
	.global rrcgrgr
rrcgrgr:
	rrc r0,r0
	rrc r15,r15
	rrc r8,r8
	rrc r7,r7
	rrc r1,r1
	rrc r8,r4
	rrc r10,r14
	rrc r15,r9
	.text
	.global rrcgrimm4
rrcgrimm4:
	rrc r0,#0
	rrc r15,#15
	rrc r8,#8
	rrc r7,#7
	rrc r1,#1
	rrc r11,#3
	rrc r14,#12
	rrc r2,#15
	.text
	.global rlcgrgr
rlcgrgr:
	rlc r0,r0
	rlc r15,r15
	rlc r8,r8
	rlc r7,r7
	rlc r1,r1
	rlc r15,r3
	rlc r15,r7
	rlc r15,r10
	.text
	.global rlcgrimm4
rlcgrimm4:
	rlc r0,#0
	rlc r15,#15
	rlc r8,#8
	rlc r7,#7
	rlc r1,#1
	rlc r8,#2
	rlc r2,#6
	rlc r6,#10
	.text
	.global shrgrgr
shrgrgr:
	shr r0,r0
	shr r15,r15
	shr r8,r8
	shr r7,r7
	shr r1,r1
	shr r13,r2
	shr r7,r8
	shr r6,r8
	.text
	.global shrgrimm
shrgrimm:
	shr r0,#0
	shr r15,#15
	shr r8,#8
	shr r7,#7
	shr r1,#1
	shr r9,#13
	shr r2,#7
	shr r8,#8
	.text
	.global shlgrgr
shlgrgr:
	shl r0,r0
	shl r15,r15
	shl r8,r8
	shl r7,r7
	shl r1,r1
	shl r2,r3
	shl r0,r3
	shl r2,r1
	.text
	.global shlgrimm
shlgrimm:
	shl r0,#0
	shl r15,#15
	shl r8,#8
	shl r7,#7
	shl r1,#1
	shl r6,#13
	shl r3,#6
	shl r15,#15
	.text
	.global asrgrgr
asrgrgr:
	asr r0,r0
	asr r15,r15
	asr r8,r8
	asr r7,r7
	asr r1,r1
	asr r5,r10
	asr r3,r5
	asr r6,r11
	.text
	.global asrgrimm
asrgrimm:
	asr r0,#0
	asr r15,#15
	asr r8,#8
	asr r7,#7
	asr r1,#1
	asr r13,#4
	asr r0,#13
	asr r6,#3
	.text
	.global set1grimm
set1grimm:
	set1 r0,#0
	set1 r15,#15
	set1 r8,#8
	set1 r7,#7
	set1 r1,#1
	set1 r6,#10
	set1 r13,#1
	set1 r13,#15
	.text
	.global set1grgr
set1grgr:
	set1 r0,r0
	set1 r15,r15
	set1 r8,r8
	set1 r7,r7
	set1 r1,r1
	set1 r6,r0
	set1 r6,r7
	set1 r14,r2
	.text
	.global set1lmemimm
set1lmemimm:
	set1 0,#0
	set1 255,#7
	set1 128,#4
	set1 127,#3
	set1 1,#1
	set1 244,#3
	set1 55,#7
	set1 252,#5
	.text
	.global set1hmemimm
set1hmemimm:
	set1 0x7f00+0,#0
	set1 0x7f00+255,#7
	set1 0x7f00+128,#4
	set1 0x7f00+127,#3
	set1 0x7f00+1,#1
	set1 0x7f00+10,#3
	set1 0x7f00+99,#4
	set1 0x7f00+148,#3
	.text
	.global clr1grimm
clr1grimm:
	clr1 r0,#0
	clr1 r15,#15
	clr1 r8,#8
	clr1 r7,#7
	clr1 r1,#1
	clr1 r12,#0
	clr1 r8,#11
	clr1 r7,#7
	.text
	.global clr1grgr
clr1grgr:
	clr1 r0,r0
	clr1 r15,r15
	clr1 r8,r8
	clr1 r7,r7
	clr1 r1,r1
	clr1 r3,r3
	clr1 r0,r1
	clr1 r15,r0
	.text
	.global clr1lmemimm
clr1lmemimm:
	clr1 0,#0
	clr1 255,#7
	clr1 128,#4
	clr1 127,#3
	clr1 1,#1
	clr1 114,#7
	clr1 229,#4
	clr1 86,#1
	.text
	.global clr1hmemimm
clr1hmemimm:
	clr1 0x7f00+0,#0
	clr1 0x7f00+255,#7
	clr1 0x7f00+128,#4
	clr1 0x7f00+127,#3
	clr1 0x7f00+1,#1
	clr1 0x7f00+44,#3
	clr1 0x7f00+212,#5
	clr1 0x7f00+67,#7
	.text
	.global cbwgr
cbwgr:
	cbw r0
	cbw r15
	cbw r8
	cbw r7
	cbw r1
	cbw r8
	cbw r11
	cbw r3
	.text
	.global revgr
revgr:
	rev r0
	rev r15
	rev r8
	rev r7
	rev r1
	rev r1
	rev r1
	rev r14
	.text
	.global bgr
bgr:
	br r0
	br r15
	br r8
	br r7
	br r1
	br r0
	br r15
	br r12
	.text
	.global jmp
jmp:
	jmp r8,r0
	jmp r9,r15
	jmp r9,r8
	jmp r8,r7
	jmp r9,r1
	jmp r9,r7
	jmp r9,r5
	jmp r8,r12
	.text
	.global jmpf
jmpf:
	jmpf 0
	jmpf 16777215
	jmpf 8388608
	jmpf 8388607
	jmpf 1
	jmpf 10731629
	jmpf 15094866
	jmpf 1464024
	.text
	.global callrgr
callrgr:
	callr r0
	callr r15
	callr r8
	callr r7
	callr r1
	callr r1
	callr r12
	callr r8
	.text
	.global callgr
callgr:
	call r8,r0
	call r9,r15
	call r9,r8
	call r8,r7
	call r9,r1
	call r9,r6
	call r9,r14
	call r8,r12
	.text
	.global callfimm
callfimm:
	callf 0
	callf 16777215
	callf 8388608
	callf 8388607
	callf 1
	callf 13546070
	callf 10837983
	callf 15197875
	.text
	.global icallrgr
icallrgr:
	icallr r0
	icallr r15
	icallr r8
	icallr r7
	icallr r1
	icallr r15
	icallr r12
	icallr r9
	.text
	.global icallgr
icallgr:
	icall r8,r0
	icall r9,r15
	icall r9,r8
	icall r8,r7
	icall r9,r1
	icall r9,r10
	icall r8,r15
	icall r8,r10
	.text
	.global icallfimm
icallfimm:
	icallf 0
	icallf 16777215
	icallf 8388608
	icallf 8388607
	icallf 1
	icallf 9649954
	icallf 1979758
	icallf 7661640
	.text
	.global iret
iret:
	iret
	.text
	.global ret
ret:
	ret
	.text
	.global mul
mul:
	mul
	.text
	.global div
div:
	div
	.text
	.global sdiv
sdiv:
	sdiv
	.text
	.global divlh
divlh:
	divlh
	.text
	.global sdivlh
sdivlh:
	sdivlh
	.text
	.global nop
nop:
	nop
	ret
	.text
	.global halt
halt:
	halt
	.text
	.global hold
hold:
	hold
	.text
	.global holdx
holdx:
	holdx
	.text
	.global brk
brk:
	brk
	.text
	.global bccgrgr
bccgrgr:
	bge r0,r0,0+(.+4)
	bz r15,r15,-1+(.+4)
	bpl r8,r8,-2048+(.+4)
	bls r7,r7,2047+(.+4)
	bnc r1,r1,1+(.+4)
	bc r3,r13,1799+(.+4)
	bge r1,r10,-2019+(.+4)
	bz r0,r5,-1132+(.+4)
	.text
	.global bccgrimm8
bccgrimm8:
	bge r0,#0,0+(.+4)
	bz r7,#255,-1+(.+4)
	bpl r4,#128,-2048+(.+4)
	bls r3,#127,2047+(.+4)
	bnc r1,#1,1+(.+4)
	bnc r3,#8,1473+(.+4)
	bnz.b r5,#203,1619+(.+4)
	bc r7,#225,978+(.+4)
	.text
	.global bccimm16
bccimm16:
	bge Rx,#0,0+(.+4)
	bz Rx,#65535,-1+(.+4)
	bpl Rx,#32768,-128+(.+4)
	bls Rx,#32767,127+(.+4)
	bnc Rx,#1,1+(.+4)
	bz.b Rx,#30715,4+(.+4)
	bnv Rx,#62266,-13+(.+4)
	bnv Rx,#48178,108+(.+4)
	.text
	.global bngrimm4
bngrimm4:
	bn r0,#0,0+(.+4)
	bn r15,#15,-1+(.+4)
	bn r8,#8,-2048+(.+4)
	bn r7,#7,2047+(.+4)
	bn r1,#1,1+(.+4)
	bn r11,#3,-1975+(.+4)
	bn r15,#4,-1205+(.+4)
	bn r10,#8,1691+(.+4)
	.text
	.global bngrgr
bngrgr:
	bn r0,r0,0+(.+4)
	bn r15,r15,-1+(.+4)
	bn r8,r8,-2048+(.+4)
	bn r7,r7,2047+(.+4)
	bn r1,r1,1+(.+4)
	bn r4,r3,1181+(.+4)
	bn r5,r2,77+(.+4)
	bn r3,r7,631+(.+4)
	.text
	.global bnlmemimm
bnlmemimm:
	bn 0,#0,0+(.+4)
	bn 255,#7,-1+(.+4)
	bn 128,#4,-2048+(.+4)
	bn 127,#3,2047+(.+4)
	bn 1,#1,1+(.+4)
	bn 153,#7,-847+(.+4)
	bn 204,#0,-1881+(.+4)
	bn 242,#7,1396+(.+4)
	.text
	.global bnhmemimm
bnhmemimm:
	bn 0x7f00+0,#0,0+(.+4)
	bn 0x7f00+255,#7,-1+(.+4)
	bn 0x7f00+128,#4,-2048+(.+4)
	bn 0x7f00+127,#3,2047+(.+4)
	bn 0x7f00+1,#1,1+(.+4)
	bn 0x7f00+185,#3,-614+(.+4)
	bn 0x7f00+105,#1,-668+(.+4)
	bn 0x7f00+79,#7,1312+(.+4)
	.text
	.global bpgrimm4
bpgrimm4:
	bp r0,#0,0+(.+4)
	bp r15,#15,-1+(.+4)
	bp r8,#8,-2048+(.+4)
	bp r7,#7,2047+(.+4)
	bp r1,#1,1+(.+4)
	bp r0,#12,1075+(.+4)
	bp r1,#5,551+(.+4)
	bp r6,#8,1588+(.+4)
	.text
	.global bpgrgr
bpgrgr:
	bp r0,r0,0+(.+4)
	bp r15,r15,-1+(.+4)
	bp r8,r8,-2048+(.+4)
	bp r7,r7,2047+(.+4)
	bp r1,r1,1+(.+4)
	bp r4,r9,-614+(.+4)
	bp r9,r10,-1360+(.+4)
	bp r4,r1,407+(.+4)
	.text
	.global bplmemimm
bplmemimm:
	bp 0,#0,0+(.+4)
	bp 255,#7,-1+(.+4)
	bp 128,#4,-2048+(.+4)
	bp 127,#3,2047+(.+4)
	bp 1,#1,1+(.+4)
	bp 193,#3,-398+(.+4)
	bp 250,#2,-1553+(.+4)
	bp 180,#6,579+(.+4)
	.text
	.global bphmemimm
bphmemimm:
	bp 0x7f00+0,#0,0+(.+4)
	bp 0x7f00+255,#7,-1+(.+4)
	bp 0x7f00+128,#4,-2048+(.+4)
	bp 0x7f00+127,#3,2047+(.+4)
	bp 0x7f00+1,#1,1+(.+4)
	bp 0x7f00+195,#1,-432+(.+4)
	bp 0x7f00+129,#5,-1508+(.+4)
	bp 0x7f00+56,#3,1723+(.+4)
	.text
	.global bcc
bcc:
	bge 0+(.+2)
	bz -1+(.+2)
	bpl -128+(.+2)
	bls 127+(.+2)
	bnc 1+(.+2)
	bnz.b 48+(.+2)
	bnc -7+(.+2)
	bnz.b 74+(.+2)
	.text
	.global br
br:
	br 0+(.+2)
	br -2+(.+2)
	br -2048+(.+2)
	br 2046+(.+2)
	br 1+(.+2)
	br 1472+(.+2)
	br 1618+(.+2)
	br 978+(.+2)
	.text
	.global callrimm
callrimm:
	callr 0+(.+2)
	callr -2+(.+2)
	callr -2048+(.+2)
	callr 2046+(.+2)
	callr 1+(.+2)
	callr 1472+(.+2)
	callr 1618+(.+2)
	callr 978+(.+2)

movgrgrsi:
	mov.b r0,(r0,extsym)
	mov.w r7,(r15,extsym-1)
	mov.w r4,(r8,extsym-2048)
	mov.b r3,(r7,extsym+2047)
	mov.w r1,(r1,extsym+1)
	mov.w r6,(r8,extsym-452)
	mov.w r4,(r11,extsym+572)
	mov.b r1,(r1,extsym-1718)
	.text
	.global movgrgriipostinc
movgrgrsipostinc:
	mov.b r0,(r0++,extsym)
	mov.w r7,(r15++,extsym-1)
	mov.w r4,(r8++,extsym-2048)
	mov.b r3,(r7++,extsym+2047)
	mov.w r1,(r1++,extsym+1)
	mov.w r6,(r0++,extsym-64)
	mov.b r7,(r15++,extsym+1060)
	mov.b r0,(r7++,extsym+847)
	.text
	.global movgrgriipredec
movgrgrsipredec:
	mov.b r0,(--r0,extsym)
	mov.w r7,(--r15,extsym-1)
	mov.w r4,(--r8,extsym-2048)
	mov.b r3,(--r7,extsym+2047)
	mov.w r1,(--r1,extsym+1)
	mov.w r0,(--r15,extsym+1780)
	mov.w r6,(--r1,extsym+1506)
	mov.w r7,(--r3,extsym-2033)
	.text
	.global movgriigr
movgrsigr:
	mov.b (r0,extsym),r0
	mov.w (r15,extsym-1),r7
	mov.w (r8,extsym-2048),r4
	mov.b (r7,extsym+2047),r3
	mov.w (r1,extsym+1),r1
	mov.w (r7,extsym+1948),r5
	mov.b (r3,extsym-844),r4
	mov.w (r15,extsym+1704),r0
	.text
	.global movgriipostincgr
movgrsipostincgr:
	mov.b (r0++,extsym),r0
	mov.w (r15++,extsym-1),r7
	mov.w (r8++,extsym-2048),r4
	mov.b (r7++,extsym+2047),r3
	mov.w (r1++,extsym+1),r1
	mov.w (r2++,extsym-176),r7
	mov.w (r8++,extsym+1389),r4
	mov.b (r3++,extsym+47),r0
	.text
	.global movgriipredecgr
movgrsipredecgr:
	mov.b (--r0,extsym),r0
	mov.w (--r15,extsym-1),r7
	mov.w (--r8,extsym-2048),r4
	mov.b (--r7,extsym+2047),r3
	mov.w (--r1,extsym+1),r1
	mov.b (--r8,extsym+1004),r4
	mov.w (--r14,extsym-1444),r2
	mov.b (--r5,extsym-927),r4
