	.text
	.am33_2
dcpf:
	dcpf	(r0)
	dcpf	(r10)
	dcpf	(d1)
	dcpf	(r7)
	dcpf	(e4)
	dcpf	(d2)
	dcpf	(r1)
	dcpf	(r11)
	dcpf	(a0)
	dcpf	(r2)
	dcpf	(e5)
	dcpf	(sp)
	dcpf	(d3, r12)
	dcpf	(a1, r3)
	dcpf	(a2, r13)
	dcpf	(r4, r14)
	dcpf	(a3, r8)
	dcpf	(r5, r15)
	dcpf	(r6, r9)
	dcpf	(r0, r10)
	dcpf	(r7, e4)
	dcpf	(r1, r11)
	dcpf	(r2, e5)
	dcpf	(104, e6)
	dcpf	(1, e0)
	dcpf	(-128, e7)
	dcpf	(32, e1)
	dcpf	(73, e2)
	dcpf	(33, d0)
	dcpf	(-69, e3)
	dcpf	(-1, d1)
	dcpf	(-32, d2)
	dcpf	(-20, a0)
	dcpf	(-95, d3)
	dcpf	(-7903933, a1)
	dcpf	(-8388608, a2)
	dcpf	(4202512, r4)
	dcpf	(130944, a3)
	dcpf	(4194304, r5)
	dcpf	(1193046, r6)
	dcpf	(-8323327, r0)
	dcpf	(-4186096, r7)
	dcpf	(-7903933, r1)
	dcpf	(-8388608, r2)
	dcpf	(4202512, r12)
	dcpf	(33554304, r3)
	dcpf	(1073741824, r13)
	dcpf	(305419896, r14)
	dcpf	(-2130706687, r8)
	dcpf	(-1071640568, r15)
	dcpf	(-2023406815, r9)
	dcpf	(-2147483648, r10)
	dcpf	(1075843080, e4)
	dcpf	(33554304, r11)
	dcpf	(1073741824, e5)
	dcpf	(305419896, e6)
bit:
	bset	1, (32768)
	bset	128, (16416)
	bset	32, (384)
	bset	73, (32767)
	bset	33, (4660)
	bset	187, (32769)
	bset	255, (49184)
	bset	224, (34661)
	bset	236, (32768)
	bset	161, (16416)
	bset	254, (384)
	bclr	0, (32767)
	bclr	127, (4660)
	bclr	24, (32769)
	bclr	229, (49184)
	bclr	104, (34661)
	bclr	1, (32768)
	bclr	128, (16416)
	bclr	32, (384)
	bclr	73, (32767)
	bclr	33, (4660)
	bclr	187, (32769)
	btst	255, (49184)
	btst	224, (34661)
	btst	236, (32768)
	btst	161, (16416)
	btst	254, (384)
	btst	0, (32767)
	btst	127, (4660)
	btst	24, (32769)
	btst	229, (49184)
	btst	104, (34661)
	btst	1, (32768)
fmovs:
	fmov	(r13), fs23
	fmov	(r14), fs17
	fmov	(r8), fs18
	fmov	(r15), fs12
	fmov	(r9), fs19
	fmov	(r10), fs13
	fmov	(e4), fs14
	fmov	(r11), fs8
	fmov	(e5), fs15
	fmov	(e6), fs9
	fmov	(e0), fs10
	fmov	(e7+), fs4
	fmov	(e1+), fs11
	fmov	(e2+), fs5
	fmov	(d0+), fs6
	fmov	(e3+), fs0
	fmov	(d1+), fs7
	fmov	(d2+), fs1
	fmov	(a0+), fs2
	fmov	(d3+), fs28
	fmov	(a1+), fs3
	fmov	(a2+), fs29
	fmov	(sp), fs4
	fmov	(sp), fs30
	fmov	(sp), fs17
	fmov	(sp), fs11
	fmov	(sp), fs24
	fmov	(sp), fs18
	fmov	(sp), fs5
	fmov	(sp), fs31
	fmov	(sp), fs12
	fmov	(sp), fs6
	fmov	(sp), fs25
	fmov	e3, fs0
	fmov	d1, fs7
	fmov	d2, fs1
	fmov	a0, fs2
	fmov	d3, fs28
	fmov	a1, fs3
	fmov	a2, fs29
	fmov	r4, fs30
	fmov	a3, fs24
	fmov	r5, fs31
	fmov	r6, fs25
	fmov	fs0, (r10)
	fmov	fs7, (e4)
	fmov	fs1, (r11)
	fmov	fs2, (e5)
	fmov	fs28, (e6)
	fmov	fs3, (e0)
	fmov	fs29, (e7)
	fmov	fs30, (e1)
	fmov	fs24, (e2)
	fmov	fs31, (d0)
	fmov	fs25, (e3)
	fmov	fs26, (d1+)
	fmov	fs20, (d2+)
	fmov	fs27, (a0+)
	fmov	fs21, (d3+)
	fmov	fs22, (a1+)
	fmov	fs16, (a2+)
	fmov	fs23, (r4+)
	fmov	fs17, (a3+)
	fmov	fs18, (r5+)
	fmov	fs12, (r6+)
	fmov	fs19, (r0+)
	fmov	fs13, (sp)
	fmov	fs7, (sp)
	fmov	fs20, (sp)
	fmov	fs14, (sp)
	fmov	fs1, (sp)
	fmov	fs27, (sp)
	fmov	fs8, (sp)
	fmov	fs2, (sp)
	fmov	fs21, (sp)
	fmov	fs15, (sp)
	fmov	fs28, (sp)
	fmov	fs22, a1
	fmov	fs16, a2
	fmov	fs23, r4
	fmov	fs17, a3
	fmov	fs18, r5
	fmov	fs12, r6
	fmov	fs19, r0
	fmov	fs13, r7
	fmov	fs14, r1
	fmov	fs8, r2
	fmov	fs15, r12
	fmov	fs9, fs3
	fmov	fs10, fs29
	fmov	fs4, fs30
	fmov	fs11, fs24
	fmov	fs5, fs31
	fmov	fs6, fs25
	fmov	fs0, fs26
	fmov	fs7, fs20
	fmov	fs1, fs27
	fmov	fs2, fs21
	fmov	fs28, fs22
	fmov	(1, e0), fs10
	fmov	(-128, e7), fs4
	fmov	(32, e1), fs11
	fmov	(73, e2), fs5
	fmov	(33, d0), fs6
	fmov	(-69, e3), fs0
	fmov	(-1, d1), fs7
	fmov	(-32, d2), fs1
	fmov	(-20, a0), fs2
	fmov	(-95, d3), fs28
	fmov	(-2, a1), fs3
	fmov	(e0+, -1), fs29
	fmov	(e7+, -32), fs30
	fmov	(e1+, -20), fs24
	fmov	(e2+, -95), fs31
	fmov	(d0+, -2), fs25
	fmov	(e3+, 0), fs26
	fmov	(d1+, 127), fs20
	fmov	(d2+, 24), fs27
	fmov	(a0+, -27), fs21
	fmov	(d3+, 104), fs22
	fmov	(a1+, 1), fs16
	fmov	(255, sp), fs29
	fmov	(224, sp), fs30
	fmov	(236, sp), fs24
	fmov	(161, sp), fs31
	fmov	(254, sp), fs25
	fmov	(0, sp), fs26
	fmov	(127, sp), fs20
	fmov	(24, sp), fs27
	fmov	(229, sp), fs21
	fmov	(104, sp), fs22
	fmov	(1, sp), fs16
	fmov	(r13, e7), fs4
	fmov	(r14, e1), fs11
	fmov	(r8, e2), fs5
	fmov	(r15, d0), fs6
	fmov	(r9, e3), fs0
	fmov	(r10, d1), fs7
	fmov	(e4, d2), fs1
	fmov	(r11, a0), fs2
	fmov	(e5, d3), fs28
	fmov	(e6, a1), fs3
	fmov	(e0, a2), fs29
	fmov	fs23, (-32, r14)
	fmov	fs17, (-20, r8)
	fmov	fs18, (-95, r15)
	fmov	fs12, (-2, r9)
	fmov	fs19, (0, r10)
	fmov	fs13, (127, e4)
	fmov	fs14, (24, r11)
	fmov	fs8, (-27, e5)
	fmov	fs15, (104, e6)
	fmov	fs9, (1, e0)
	fmov	fs10, (-128, e7)
	fmov	fs4, (r14+, 24)
	fmov	fs11, (r8+, -27)
	fmov	fs5, (r15+, 104)
	fmov	fs6, (r9+, 1)
	fmov	fs0, (r10+, -128)
	fmov	fs7, (e4+, 32)
	fmov	fs1, (r11+, 73)
	fmov	fs2, (e5+, 33)
	fmov	fs28, (e6+, -69)
	fmov	fs3, (e0+, -1)
	fmov	fs29, (e7+, -32)
	fmov	fs30, (24, sp)
	fmov	fs24, (229, sp)
	fmov	fs31, (104, sp)
	fmov	fs25, (1, sp)
	fmov	fs26, (128, sp)
	fmov	fs20, (32, sp)
	fmov	fs27, (73, sp)
	fmov	fs21, (33, sp)
	fmov	fs22, (187, sp)
	fmov	fs16, (255, sp)
	fmov	fs23, (224, sp)
	fmov	fs17, (a3, r8)
	fmov	fs18, (r5, r15)
	fmov	fs12, (r6, r9)
	fmov	fs19, (r0, r10)
	fmov	fs13, (r7, e4)
	fmov	fs14, (r1, r11)
	fmov	fs8, (r2, e5)
	fmov	fs15, (r12, e6)
	fmov	fs9, (r3, e0)
	fmov	fs10, (r13, e7)
	fmov	fs4, (r14, e1)
	fmov	(-8323327, r8), fs18
	fmov	(-4186096, r15), fs12
	fmov	(-7903933, r9), fs19
	fmov	(-8388608, r10), fs13
	fmov	(4202512, e4), fs14
	fmov	(130944, r11), fs8
	fmov	(4194304, e5), fs15
	fmov	(1193046, e6), fs9
	fmov	(-8323327, e0), fs10
	fmov	(-4186096, e7), fs4
	fmov	(-7903933, e1), fs11
	fmov	(r8+, 4194304), fs5
	fmov	(r15+, 1193046), fs6
	fmov	(r9+, -8323327), fs0
	fmov	(r10+, -4186096), fs7
	fmov	(e4+, -7903933), fs1
	fmov	(r11+, -8388608), fs2
	fmov	(e5+, 4202512), fs28
	fmov	(e6+, 130944), fs3
	fmov	(e0+, 4194304), fs29
	fmov	(e7+, 1193046), fs30
	fmov	(e1+, -8323327), fs24
	fmov	(4194304, sp), fs5
	fmov	(1193046, sp), fs6
	fmov	(8453889, sp), fs0
	fmov	(12591120, sp), fs7
	fmov	(8873283, sp), fs1
	fmov	(8388608, sp), fs2
	fmov	(4202512, sp), fs28
	fmov	(130944, sp), fs3
	fmov	(4194304, sp), fs29
	fmov	(1193046, sp), fs30
	fmov	(8453889, sp), fs24
	fmov	fs5, (4202512, d0)
	fmov	fs6, (130944, e3)
	fmov	fs0, (4194304, d1)
	fmov	fs7, (1193046, d2)
	fmov	fs1, (-8323327, a0)
	fmov	fs2, (-4186096, d3)
	fmov	fs28, (-7903933, a1)
	fmov	fs3, (-8388608, a2)
	fmov	fs29, (4202512, r4)
	fmov	fs30, (130944, a3)
	fmov	fs24, (4194304, r5)
	fmov	fs31, (d0+, -7903933)
	fmov	fs25, (e3+, -8388608)
	fmov	fs26, (d1+, 4202512)
	fmov	fs20, (d2+, 130944)
	fmov	fs27, (a0+, 4194304)
	fmov	fs21, (d3+, 1193046)
	fmov	fs22, (a1+, -8323327)
	fmov	fs16, (a2+, -4186096)
	fmov	fs23, (r4+, -7903933)
	fmov	fs17, (a3+, -8388608)
	fmov	fs18, (r5+, 4202512)
	fmov	fs12, (8873283, sp)
	fmov	fs19, (8388608, sp)
	fmov	fs13, (4202512, sp)
	fmov	fs14, (130944, sp)
	fmov	fs8, (4194304, sp)
	fmov	fs15, (1193046, sp)
	fmov	fs9, (8453889, sp)
	fmov	fs10, (12591120, sp)
	fmov	fs4, (8873283, sp)
	fmov	fs11, (8388608, sp)
	fmov	fs5, (4202512, sp)
	fmov	(-2023406815, r9), fs19
	fmov	(-2147483648, r10), fs13
	fmov	(1075843080, e4), fs14
	fmov	(33554304, r11), fs8
	fmov	(1073741824, e5), fs15
	fmov	(305419896, e6), fs9
	fmov	(-2130706687, e0), fs10
	fmov	(-1071640568, e7), fs4
	fmov	(-2023406815, e1), fs11
	fmov	(-2147483648, e2), fs5
	fmov	(1075843080, d0), fs6
	fmov	(r9+, -2130706687), fs0
	fmov	(r10+, -1071640568), fs7
	fmov	(e4+, -2023406815), fs1
	fmov	(r11+, -2147483648), fs2
	fmov	(e5+, 1075843080), fs28
	fmov	(e6+, 33554304), fs3
	fmov	(e0+, 1073741824), fs29
	fmov	(e7+, 305419896), fs30
	fmov	(e1+, -2130706687), fs24
	fmov	(e2+, -1071640568), fs31
	fmov	(d0+, -2023406815), fs25
	fmov	(-2130706687, sp), fs0
	fmov	(-1071640568, sp), fs7
	fmov	(-2023406815, sp), fs1
	fmov	(-2147483648, sp), fs2
	fmov	(1075843080, sp), fs28
	fmov	(33554304, sp), fs3
	fmov	(1073741824, sp), fs29
	fmov	(305419896, sp), fs30
	fmov	(-2130706687, sp), fs24
	fmov	(-1071640568, sp), fs31
	fmov	(-2023406815, sp), fs25
	fmov	-2147483648, fs26
	fmov	1075843080, fs20
	fmov	33554304, fs27
	fmov	1073741824, fs21
	fmov	305419896, fs22
	fmov	-2130706687, fs16
	fmov	-1071640568, fs23
	fmov	-2023406815, fs17
	fmov	-2147483648, fs18
	fmov	1075843080, fs12
	fmov	33554304, fs19
	fmov	fs26, (-1071640568, r7)
	fmov	fs20, (-2023406815, r1)
	fmov	fs27, (-2147483648, r2)
	fmov	fs21, (1075843080, r12)
	fmov	fs22, (33554304, r3)
	fmov	fs16, (1073741824, r13)
	fmov	fs23, (305419896, r14)
	fmov	fs17, (-2130706687, r8)
	fmov	fs18, (-1071640568, r15)
	fmov	fs12, (-2023406815, r9)
	fmov	fs19, (-2147483648, r10)
	fmov	fs13, (r7+, 305419896)
	fmov	fs14, (r1+, -2130706687)
	fmov	fs8, (r2+, -1071640568)
	fmov	fs15, (r12+, -2023406815)
	fmov	fs9, (r3+, -2147483648)
	fmov	fs10, (r13+, 1075843080)
	fmov	fs4, (r14+, 33554304)
	fmov	fs11, (r8+, 1073741824)
	fmov	fs5, (r15+, 305419896)
	fmov	fs6, (r9+, -2130706687)
	fmov	fs0, (r10+, -1071640568)
	fmov	fs7, (305419896, sp)
	fmov	fs1, (-2130706687, sp)
	fmov	fs2, (-1071640568, sp)
	fmov	fs28, (-2023406815, sp)
	fmov	fs3, (-2147483648, sp)
	fmov	fs29, (1075843080, sp)
	fmov	fs30, (33554304, sp)
	fmov	fs24, (1073741824, sp)
	fmov	fs31, (305419896, sp)
	fmov	fs25, (-2130706687, sp)
	fmov	fs26, (-1071640568, sp)
fmovd:
	fmov	(e4), fd8
	fmov	(r11), fd16
	fmov	(e5), fd4
	fmov	(e6), fd12
	fmov	(e0), fd14
	fmov	(e7), fd6
	fmov	(e1), fd2
	fmov	(e2), fd26
	fmov	(d0), fd8
	fmov	(e3), fd0
	fmov	(d1), fd20
	fmov	(d2+), fd28
	fmov	(a0+), fd26
	fmov	(d3+), fd2
	fmov	(a1+), fd22
	fmov	(a2+), fd10
	fmov	(r4+), fd24
	fmov	(a3+), fd16
	fmov	(r5+), fd12
	fmov	(r6+), fd4
	fmov	(r0+), fd10
	fmov	(r7+), fd18
	fmov	(sp), fd28
	fmov	(sp), fd6
	fmov	(sp), fd16
	fmov	(sp), fd26
	fmov	(sp), fd14
	fmov	(sp), fd4
	fmov	(sp), fd2
	fmov	(sp), fd24
	fmov	(sp), fd12
	fmov	(sp), fd22
	fmov	(sp), fd0
	fmov	fd14, (r13)
	fmov	fd6, (r14)
	fmov	fd2, (r8)
	fmov	fd26, (r15)
	fmov	fd8, (r9)
	fmov	fd0, (r10)
	fmov	fd20, (e4)
	fmov	fd28, (r11)
	fmov	fd26, (e5)
	fmov	fd2, (e6)
	fmov	fd22, (e0)
	fmov	fd10, (e7+)
	fmov	fd24, (e1+)
	fmov	fd16, (e2+)
	fmov	fd12, (d0+)
	fmov	fd4, (e3+)
	fmov	fd10, (d1+)
	fmov	fd18, (d2+)
	fmov	fd6, (a0+)
	fmov	fd14, (d3+)
	fmov	fd24, (a1+)
	fmov	fd0, (a2+)
	fmov	fd28, (sp)
	fmov	fd6, (sp)
	fmov	fd24, (sp)
	fmov	fd20, (sp)
	fmov	fd2, (sp)
	fmov	fd16, (sp)
	fmov	fd30, (sp)
	fmov	fd26, (sp)
	fmov	fd12, (sp)
	fmov	fd22, (sp)
	fmov	fd8, (sp)
	fmov	fd4, fd18
	fmov	fd10, fd30
	fmov	fd18, fd8
	fmov	fd6, fd16
	fmov	fd14, fd4
	fmov	fd24, fd12
	fmov	fd0, fd14
	fmov	fd28, fd6
	fmov	fd20, fd2
	fmov	fd30, fd26
	fmov	fd22, fd8
	fmov	(e3, r0), fd10
	fmov	(d1, r7), fd18
	fmov	(d2, r1), fd6
	fmov	(a0, r2), fd14
	fmov	(d3, r12), fd24
	fmov	(a1, r3), fd0
	fmov	(a2, r13), fd28
	fmov	(r4, r14), fd20
	fmov	(a3, r8), fd30
	fmov	(r5, r15), fd22
	fmov	(r6, r9), fd18
	fmov	fd0, (r10, d1)
	fmov	fd20, (e4, d2)
	fmov	fd28, (r11, a0)
	fmov	fd26, (e5, d3)
	fmov	fd2, (e6, a1)
	fmov	fd22, (e0, a2)
	fmov	fd10, (e7, r4)
	fmov	fd24, (e1, a3)
	fmov	fd16, (e2, r5)
	fmov	fd12, (d0, r6)
	fmov	fd4, (e3, r0)
	fmov	(-1, d1), fd20
	fmov	(-32, d2), fd28
	fmov	(-20, a0), fd26
	fmov	(-95, d3), fd2
	fmov	(-2, a1), fd22
	fmov	(0, a2), fd10
	fmov	(127, r4), fd24
	fmov	(24, a3), fd16
	fmov	(-27, r5), fd12
	fmov	(104, r6), fd4
	fmov	(1, r0), fd10
	fmov	(d1+, 127), fd18
	fmov	(d2+, 24), fd6
	fmov	(a0+, -27), fd14
	fmov	(d3+, 104), fd24
	fmov	(a1+, 1), fd0
	fmov	(a2+, -128), fd28
	fmov	(r4+, 32), fd20
	fmov	(a3+, 73), fd30
	fmov	(r5+, 33), fd22
	fmov	(r6+, -69), fd18
	fmov	(r0+, -1), fd30
	fmov	(127, sp), fd18
	fmov	(24, sp), fd6
	fmov	(229, sp), fd14
	fmov	(104, sp), fd24
	fmov	(1, sp), fd0
	fmov	(128, sp), fd28
	fmov	(32, sp), fd20
	fmov	(73, sp), fd30
	fmov	(33, sp), fd22
	fmov	(187, sp), fd18
	fmov	(255, sp), fd30
	fmov	fd18, (32, r1)
	fmov	fd6, (73, r2)
	fmov	fd14, (33, r12)
	fmov	fd24, (-69, r3)
	fmov	fd0, (-1, r13)
	fmov	fd28, (-32, r14)
	fmov	fd20, (-20, r8)
	fmov	fd30, (-95, r15)
	fmov	fd22, (-2, r9)
	fmov	fd18, (0, r10)
	fmov	fd30, (127, e4)
	fmov	fd8, (r1+, -20)
	fmov	fd16, (r2+, -95)
	fmov	fd4, (r12+, -2)
	fmov	fd12, (r3+, 0)
	fmov	fd14, (r13+, 127)
	fmov	fd6, (r14+, 24)
	fmov	fd2, (r8+, -27)
	fmov	fd26, (r15+, 104)
	fmov	fd8, (r9+, 1)
	fmov	fd0, (r10+, -128)
	fmov	fd20, (e4+, 32)
	fmov	fd28, (236, sp)
	fmov	fd26, (161, sp)
	fmov	fd2, (254, sp)
	fmov	fd22, (0, sp)
	fmov	fd10, (127, sp)
	fmov	fd24, (24, sp)
	fmov	fd16, (229, sp)
	fmov	fd12, (104, sp)
	fmov	fd4, (1, sp)
	fmov	fd10, (128, sp)
	fmov	fd18, (32, sp)
	fmov	(-8323327, a0), fd26
	fmov	(-4186096, d3), fd2
	fmov	(-7903933, a1), fd22
	fmov	(-8388608, a2), fd10
	fmov	(4202512, r4), fd24
	fmov	(130944, a3), fd16
	fmov	(4194304, r5), fd12
	fmov	(1193046, r6), fd4
	fmov	(-8323327, r0), fd10
	fmov	(-4186096, r7), fd18
	fmov	(-7903933, r1), fd6
	fmov	(a0+, 4194304), fd14
	fmov	(d3+, 1193046), fd24
	fmov	(a1+, -8323327), fd0
	fmov	(a2+, -4186096), fd28
	fmov	(r4+, -7903933), fd20
	fmov	(a3+, -8388608), fd30
	fmov	(r5+, 4202512), fd22
	fmov	(r6+, 130944), fd18
	fmov	(r0+, 4194304), fd30
	fmov	(r7+, 1193046), fd8
	fmov	(r1+, -8323327), fd16
	fmov	(4194304, sp), fd14
	fmov	(1193046, sp), fd24
	fmov	(8453889, sp), fd0
	fmov	(12591120, sp), fd28
	fmov	(8873283, sp), fd20
	fmov	(8388608, sp), fd30
	fmov	(4202512, sp), fd22
	fmov	(130944, sp), fd18
	fmov	(4194304, sp), fd30
	fmov	(1193046, sp), fd8
	fmov	(8453889, sp), fd16
	fmov	fd14, (4202512, r12)
	fmov	fd24, (130944, r3)
	fmov	fd0, (4194304, r13)
	fmov	fd28, (1193046, r14)
	fmov	fd20, (-8323327, r8)
	fmov	fd30, (-4186096, r15)
	fmov	fd22, (-7903933, r9)
	fmov	fd18, (-8388608, r10)
	fmov	fd30, (4202512, e4)
	fmov	fd8, (130944, r11)
	fmov	fd16, (4194304, e5)
	fmov	fd4, (r12+, -7903933)
	fmov	fd12, (r3+, -8388608)
	fmov	fd14, (r13+, 4202512)
	fmov	fd6, (r14+, 130944)
	fmov	fd2, (r8+, 4194304)
	fmov	fd26, (r15+, 1193046)
	fmov	fd8, (r9+, -8323327)
	fmov	fd0, (r10+, -4186096)
	fmov	fd20, (e4+, -7903933)
	fmov	fd28, (r11+, -8388608)
	fmov	fd26, (e5+, 4202512)
	fmov	fd2, (8873283, sp)
	fmov	fd22, (8388608, sp)
	fmov	fd10, (4202512, sp)
	fmov	fd24, (130944, sp)
	fmov	fd16, (4194304, sp)
	fmov	fd12, (1193046, sp)
	fmov	fd4, (8453889, sp)
	fmov	fd10, (12591120, sp)
	fmov	fd18, (8873283, sp)
	fmov	fd6, (8388608, sp)
	fmov	fd14, (4202512, sp)
	fmov	(-2023406815, a1), fd22
	fmov	(-2147483648, a2), fd10
	fmov	(1075843080, r4), fd24
	fmov	(33554304, a3), fd16
	fmov	(1073741824, r5), fd12
	fmov	(305419896, r6), fd4
	fmov	(-2130706687, r0), fd10
	fmov	(-1071640568, r7), fd18
	fmov	(-2023406815, r1), fd6
	fmov	(-2147483648, r2), fd14
	fmov	(1075843080, r12), fd24
	fmov	(a1+, -2130706687), fd0
	fmov	(a2+, -1071640568), fd28
	fmov	(r4+, -2023406815), fd20
	fmov	(a3+, -2147483648), fd30
	fmov	(r5+, 1075843080), fd22
	fmov	(r6+, 33554304), fd18
	fmov	(r0+, 1073741824), fd30
	fmov	(r7+, 305419896), fd8
	fmov	(r1+, -2130706687), fd16
	fmov	(r2+, -1071640568), fd4
	fmov	(r12+, -2023406815), fd12
	fmov	(-2130706687, sp), fd0
	fmov	(-1071640568, sp), fd28
	fmov	(-2023406815, sp), fd20
	fmov	(-2147483648, sp), fd30
	fmov	(1075843080, sp), fd22
	fmov	(33554304, sp), fd18
	fmov	(1073741824, sp), fd30
	fmov	(305419896, sp), fd8
	fmov	(-2130706687, sp), fd16
	fmov	(-1071640568, sp), fd4
	fmov	(-2023406815, sp), fd12
	fmov	fd0, (1073741824, r13)
	fmov	fd28, (305419896, r14)
	fmov	fd20, (-2130706687, r8)
	fmov	fd30, (-1071640568, r15)
	fmov	fd22, (-2023406815, r9)
	fmov	fd18, (-2147483648, r10)
	fmov	fd30, (1075843080, e4)
	fmov	fd8, (33554304, r11)
	fmov	fd16, (1073741824, e5)
	fmov	fd4, (305419896, e6)
	fmov	fd12, (-2130706687, e0)
	fmov	fd14, (r13+, 1075843080)
	fmov	fd6, (r14+, 33554304)
	fmov	fd2, (r8+, 1073741824)
	fmov	fd26, (r15+, 305419896)
	fmov	fd8, (r9+, -2130706687)
	fmov	fd0, (r10+, -1071640568)
	fmov	fd20, (e4+, -2023406815)
	fmov	fd28, (r11+, -2147483648)
	fmov	fd26, (e5+, 1075843080)
	fmov	fd2, (e6+, 33554304)
	fmov	fd22, (e0+, 1073741824)
	fmov	fd10, (1075843080, sp)
	fmov	fd24, (33554304, sp)
	fmov	fd16, (1073741824, sp)
	fmov	fd12, (305419896, sp)
	fmov	fd4, (-2130706687, sp)
	fmov	fd10, (-1071640568, sp)
	fmov	fd18, (-2023406815, sp)
	fmov	fd6, (-2147483648, sp)
	fmov	fd14, (1075843080, sp)
	fmov	fd24, (33554304, sp)
	fmov	fd0, (1073741824, sp)
fmovc:
	fmov	e7, fpcr
	fmov	r4, fpcr
	fmov	r14, fpcr
	fmov	e1, fpcr
	fmov	a3, fpcr
	fmov	r8, fpcr
	fmov	e2, fpcr
	fmov	r5, fpcr
	fmov	r15, fpcr
	fmov	d0, fpcr
	fmov	r6, fpcr
	fmov	fpcr, r9
	fmov	fpcr, e3
	fmov	fpcr, r0
	fmov	fpcr, r10
	fmov	fpcr, d1
	fmov	fpcr, r7
	fmov	fpcr, e4
	fmov	fpcr, d2
	fmov	fpcr, r1
	fmov	fpcr, r11
	fmov	fpcr, a0
	fmov	1073741824, fpcr
	fmov	-1071640568, fpcr
	fmov	1075843080, fpcr
	fmov	305419896, fpcr
	fmov	-2023406815, fpcr
	fmov	33554304, fpcr
	fmov	-2130706687, fpcr
	fmov	-2147483648, fpcr
	fmov	1073741824, fpcr
	fmov	-1071640568, fpcr
	fmov	1075843080, fpcr
sfparith:
	fabs	fs4
	fabs	fs30
	fabs	fs17
	fabs	fs11
	fabs	fs24
	fabs	fs18
	fabs	fs5
	fabs	fs31
	fabs	fs12
	fabs	fs6
	fabs	fs25
	fabs	fs19, fs0
	fabs	fs13, fs7
	fabs	fs14, fs1
	fabs	fs8, fs2
	fabs	fs15, fs28
	fabs	fs9, fs3
	fabs	fs10, fs29
	fabs	fs4, fs30
	fabs	fs11, fs24
	fabs	fs5, fs31
	fabs	fs6, fs25
	fneg	fs0
	fneg	fs26
	fneg	fs13
	fneg	fs7
	fneg	fs20
	fneg	fs14
	fneg	fs1
	fneg	fs27
	fneg	fs8
	fneg	fs2
	fneg	fs21
	fneg	fs15, fs28
	fneg	fs9, fs3
	fneg	fs10, fs29
	fneg	fs4, fs30
	fneg	fs11, fs24
	fneg	fs5, fs31
	fneg	fs6, fs25
	fneg	fs0, fs26
	fneg	fs7, fs20
	fneg	fs1, fs27
	fneg	fs2, fs21
	frsqrt	fs28
	frsqrt	fs22
	frsqrt	fs9
	frsqrt	fs3
	frsqrt	fs16
	frsqrt	fs10
	frsqrt	fs29
	frsqrt	fs23
	frsqrt	fs4
	frsqrt	fs30
	frsqrt	fs17
	frsqrt	fs11, fs24
	frsqrt	fs5, fs31
	frsqrt	fs6, fs25
	frsqrt	fs0, fs26
	frsqrt	fs7, fs20
	frsqrt	fs1, fs27
	frsqrt	fs2, fs21
	frsqrt	fs28, fs22
	frsqrt	fs3, fs16
	frsqrt	fs29, fs23
	frsqrt	fs30, fs17
	fsqrt	fs24
	fsqrt	fs18
	fsqrt	fs5
	fsqrt	fs31
	fsqrt	fs12
	fsqrt	fs6
	fsqrt	fs25
	fsqrt	fs19
	fsqrt	fs0
	fsqrt	fs26
	fsqrt	fs13
	fsqrt	fs7, fs20
	fsqrt	fs1, fs27
	fsqrt	fs2, fs21
	fsqrt	fs28, fs22
	fsqrt	fs3, fs16
	fsqrt	fs29, fs23
	fsqrt	fs30, fs17
	fsqrt	fs24, fs18
	fsqrt	fs31, fs12
	fsqrt	fs25, fs19
	fsqrt	fs26, fs13
	fcmp	fs20, fs14
	fcmp	fs27, fs8
	fcmp	fs21, fs15
	fcmp	fs22, fs9
	fcmp	fs16, fs10
	fcmp	fs23, fs4
	fcmp	fs17, fs11
	fcmp	fs18, fs5
	fcmp	fs12, fs6
	fcmp	fs19, fs0
	fcmp	fs13, fs7
	fcmp	-2023406815, fs1
	fcmp	-2147483648, fs2
	fcmp	1075843080, fs28
	fcmp	33554304, fs3
	fcmp	1073741824, fs29
	fcmp	305419896, fs30
	fcmp	-2130706687, fs24
	fcmp	-1071640568, fs31
	fcmp	-2023406815, fs25
	fcmp	-2147483648, fs26
	fcmp	1075843080, fs20
	fadd	fs1, fs27
	fadd	fs2, fs21
	fadd	fs28, fs22
	fadd	fs3, fs16
	fadd	fs29, fs23
	fadd	fs30, fs17
	fadd	fs24, fs18
	fadd	fs31, fs12
	fadd	fs25, fs19
	fadd	fs26, fs13
	fadd	fs20, fs14
	fadd	fs27, fs8, fs2
	fadd	fs21, fs15, fs28
	fadd	fs22, fs9, fs3
	fadd	fs16, fs10, fs29
	fadd	fs23, fs4, fs30
	fadd	fs17, fs11, fs24
	fadd	fs18, fs5, fs31
	fadd	fs12, fs6, fs25
	fadd	fs19, fs0, fs26
	fadd	fs13, fs7, fs20
	fadd	fs14, fs1, fs27
	fadd	-2147483648, fs2, fs21
	fadd	1075843080, fs28, fs22
	fadd	33554304, fs3, fs16
	fadd	1073741824, fs29, fs23
	fadd	305419896, fs30, fs17
	fadd	-2130706687, fs24, fs18
	fadd	-1071640568, fs31, fs12
	fadd	-2023406815, fs25, fs19
	fadd	-2147483648, fs26, fs13
	fadd	1075843080, fs20, fs14
	fadd	33554304, fs27, fs8
	fsub	fs2, fs21
	fsub	fs28, fs22
	fsub	fs3, fs16
	fsub	fs29, fs23
	fsub	fs30, fs17
	fsub	fs24, fs18
	fsub	fs31, fs12
	fsub	fs25, fs19
	fsub	fs26, fs13
	fsub	fs20, fs14
	fsub	fs27, fs8
	fsub	fs21, fs15, fs28
	fsub	fs22, fs9, fs3
	fsub	fs16, fs10, fs29
	fsub	fs23, fs4, fs30
	fsub	fs17, fs11, fs24
	fsub	fs18, fs5, fs31
	fsub	fs12, fs6, fs25
	fsub	fs19, fs0, fs26
	fsub	fs13, fs7, fs20
	fsub	fs14, fs1, fs27
	fsub	fs8, fs2, fs21
	fsub	1075843080, fs28, fs22
	fsub	33554304, fs3, fs16
	fsub	1073741824, fs29, fs23
	fsub	305419896, fs30, fs17
	fsub	-2130706687, fs24, fs18
	fsub	-1071640568, fs31, fs12
	fsub	-2023406815, fs25, fs19
	fsub	-2147483648, fs26, fs13
	fsub	1075843080, fs20, fs14
	fsub	33554304, fs27, fs8
	fsub	1073741824, fs21, fs15
	fmul	fs28, fs22
	fmul	fs3, fs16
	fmul	fs29, fs23
	fmul	fs30, fs17
	fmul	fs24, fs18
	fmul	fs31, fs12
	fmul	fs25, fs19
	fmul	fs26, fs13
	fmul	fs20, fs14
	fmul	fs27, fs8
	fmul	fs21, fs15
	fmul	fs22, fs9, fs3
	fmul	fs16, fs10, fs29
	fmul	fs23, fs4, fs30
	fmul	fs17, fs11, fs24
	fmul	fs18, fs5, fs31
	fmul	fs12, fs6, fs25
	fmul	fs19, fs0, fs26
	fmul	fs13, fs7, fs20
	fmul	fs14, fs1, fs27
	fmul	fs8, fs2, fs21
	fmul	fs15, fs28, fs22
	fmul	33554304, fs3, fs16
	fmul	1073741824, fs29, fs23
	fmul	305419896, fs30, fs17
	fmul	-2130706687, fs24, fs18
	fmul	-1071640568, fs31, fs12
	fmul	-2023406815, fs25, fs19
	fmul	-2147483648, fs26, fs13
	fmul	1075843080, fs20, fs14
	fmul	33554304, fs27, fs8
	fmul	1073741824, fs21, fs15
	fmul	305419896, fs22, fs9
	fdiv	fs3, fs16
	fdiv	fs29, fs23
	fdiv	fs30, fs17
	fdiv	fs24, fs18
	fdiv	fs31, fs12
	fdiv	fs25, fs19
	fdiv	fs26, fs13
	fdiv	fs20, fs14
	fdiv	fs27, fs8
	fdiv	fs21, fs15
	fdiv	fs22, fs9
	fdiv	fs16, fs10, fs29
	fdiv	fs23, fs4, fs30
	fdiv	fs17, fs11, fs24
	fdiv	fs18, fs5, fs31
	fdiv	fs12, fs6, fs25
	fdiv	fs19, fs0, fs26
	fdiv	fs13, fs7, fs20
	fdiv	fs14, fs1, fs27
	fdiv	fs8, fs2, fs21
	fdiv	fs15, fs28, fs22
	fdiv	fs9, fs3, fs16
	fdiv	1073741824, fs29, fs23
	fdiv	305419896, fs30, fs17
	fdiv	-2130706687, fs24, fs18
	fdiv	-1071640568, fs31, fs12
	fdiv	-2023406815, fs25, fs19
	fdiv	-2147483648, fs26, fs13
	fdiv	1075843080, fs20, fs14
	fdiv	33554304, fs27, fs8
	fdiv	1073741824, fs21, fs15
	fdiv	305419896, fs22, fs9
	fdiv	-2130706687, fs16, fs10
fpacc:
	fmadd	fs29, fs23, fs4, fs2
	fmadd	fs11, fs24, fs18, fs5
	fmadd	fs12, fs6, fs25, fs3
	fmadd	fs26, fs13, fs7, fs4
	fmadd	fs1, fs27, fs8, fs2
	fmadd	fs15, fs28, fs22, fs1
	fmadd	fs16, fs10, fs29, fs1
	fmadd	fs30, fs17, fs11, fs0
	fmadd	fs5, fs31, fs12, fs6
	fmadd	fs19, fs0, fs26, fs3
	fmadd	fs20, fs14, fs1, fs5
	fmsub	fs2, fs21, fs15, fs4
	fmsub	fs9, fs3, fs16, fs6
	fmsub	fs23, fs4, fs30, fs7
	fmsub	fs24, fs18, fs5, fs7
	fmsub	fs6, fs25, fs19, fs0
	fmsub	fs13, fs7, fs20, fs2
	fmsub	fs27, fs8, fs2, fs5
	fmsub	fs28, fs22, fs9, fs3
	fmsub	fs10, fs29, fs23, fs4
	fmsub	fs17, fs11, fs24, fs2
	fmsub	fs31, fs12, fs6, fs1
	fnmadd	fs0, fs26, fs13, fs1
	fnmadd	fs14, fs1, fs27, fs0
	fnmadd	fs21, fs15, fs28, fs6
	fnmadd	fs3, fs16, fs10, fs3
	fnmadd	fs4, fs30, fs17, fs5
	fnmadd	fs18, fs5, fs31, fs4
	fnmadd	fs25, fs19, fs0, fs6
	fnmadd	fs7, fs20, fs14, fs7
	fnmadd	fs8, fs2, fs21, fs7
	fnmadd	fs22, fs9, fs3, fs0
	fnmadd	fs29, fs23, fs4, fs2
	fnmsub	fs11, fs24, fs18, fs5
	fnmsub	fs12, fs6, fs25, fs3
	fnmsub	fs26, fs13, fs7, fs4
	fnmsub	fs1, fs27, fs8, fs2
	fnmsub	fs15, fs28, fs22, fs1
	fnmsub	fs16, fs10, fs29, fs1
	fnmsub	fs30, fs17, fs11, fs0
	fnmsub	fs5, fs31, fs12, fs6
	fnmsub	fs19, fs0, fs26, fs3
	fnmsub	fs20, fs14, fs1, fs5
	fnmsub	fs2, fs21, fs15, fs4
dfparith:
	fabs	fd12
	fabs	fd22
	fabs	fd0
	fabs	fd14
	fabs	fd10
	fabs	fd28
	fabs	fd6
	fabs	fd24
	fabs	fd20
	fabs	fd2
	fabs	fd16
	fabs	fd30, fd26
	fabs	fd22, fd8
	fabs	fd18, fd0
	fabs	fd30, fd20
	fabs	fd8, fd28
	fabs	fd16, fd26
	fabs	fd4, fd2
	fabs	fd12, fd22
	fabs	fd14, fd10
	fabs	fd6, fd24
	fabs	fd2, fd16
	fneg	fd26
	fneg	fd12
	fneg	fd22
	fneg	fd8
	fneg	fd4
	fneg	fd18
	fneg	fd0
	fneg	fd10
	fneg	fd30
	fneg	fd20
	fneg	fd18
	fneg	fd8, fd28
	fneg	fd16, fd26
	fneg	fd4, fd2
	fneg	fd12, fd22
	fneg	fd14, fd10
	fneg	fd6, fd24
	fneg	fd2, fd16
	fneg	fd26, fd12
	fneg	fd8, fd4
	fneg	fd0, fd10
	fneg	fd20, fd18
	frsqrt	fd28
	frsqrt	fd6
	frsqrt	fd16
	frsqrt	fd26
	frsqrt	fd14
	frsqrt	fd4
	frsqrt	fd2
	frsqrt	fd24
	frsqrt	fd12
	frsqrt	fd22
	frsqrt	fd0
	frsqrt	fd14, fd10
	frsqrt	fd6, fd24
	frsqrt	fd2, fd16
	frsqrt	fd26, fd12
	frsqrt	fd8, fd4
	frsqrt	fd0, fd10
	frsqrt	fd20, fd18
	frsqrt	fd28, fd6
	frsqrt	fd26, fd14
	frsqrt	fd2, fd24
	frsqrt	fd22, fd0
	fsqrt	fd10
	fsqrt	fd28
	fsqrt	fd6
	fsqrt	fd24
	fsqrt	fd20
	fsqrt	fd2
	fsqrt	fd16
	fsqrt	fd30
	fsqrt	fd26
	fsqrt	fd12
	fsqrt	fd22
	fsqrt	fd8, fd4
	fsqrt	fd0, fd10
	fsqrt	fd20, fd18
	fsqrt	fd28, fd6
	fsqrt	fd26, fd14
	fsqrt	fd2, fd24
	fsqrt	fd22, fd0
	fsqrt	fd10, fd28
	fsqrt	fd24, fd20
	fsqrt	fd16, fd30
	fsqrt	fd12, fd22
	fcmp	fd4, fd18
	fcmp	fd10, fd30
	fcmp	fd18, fd8
	fcmp	fd6, fd16
	fcmp	fd14, fd4
	fcmp	fd24, fd12
	fcmp	fd0, fd14
	fcmp	fd28, fd6
	fcmp	fd20, fd2
	fcmp	fd30, fd26
	fcmp	fd22, fd8
	fadd	fd18, fd0
	fadd	fd30, fd20
	fadd	fd8, fd28
	fadd	fd16, fd26
	fadd	fd4, fd2
	fadd	fd12, fd22
	fadd	fd14, fd10
	fadd	fd6, fd24
	fadd	fd2, fd16
	fadd	fd26, fd12
	fadd	fd8, fd4
	fadd	fd0, fd10, fd30
	fadd	fd20, fd18, fd8
	fadd	fd28, fd6, fd16
	fadd	fd26, fd14, fd4
	fadd	fd2, fd24, fd12
	fadd	fd22, fd0, fd14
	fadd	fd10, fd28, fd6
	fadd	fd24, fd20, fd2
	fadd	fd16, fd30, fd26
	fadd	fd12, fd22, fd8
	fadd	fd4, fd18, fd0
	fsub	fd10, fd30
	fsub	fd18, fd8
	fsub	fd6, fd16
	fsub	fd14, fd4
	fsub	fd24, fd12
	fsub	fd0, fd14
	fsub	fd28, fd6
	fsub	fd20, fd2
	fsub	fd30, fd26
	fsub	fd22, fd8
	fsub	fd18, fd0
	fsub	fd30, fd20, fd18
	fsub	fd8, fd28, fd6
	fsub	fd16, fd26, fd14
	fsub	fd4, fd2, fd24
	fsub	fd12, fd22, fd0
	fsub	fd14, fd10, fd28
	fsub	fd6, fd24, fd20
	fsub	fd2, fd16, fd30
	fsub	fd26, fd12, fd22
	fsub	fd8, fd4, fd18
	fsub	fd0, fd10, fd30
	fmul	fd20, fd18
	fmul	fd28, fd6
	fmul	fd26, fd14
	fmul	fd2, fd24
	fmul	fd22, fd0
	fmul	fd10, fd28
	fmul	fd24, fd20
	fmul	fd16, fd30
	fmul	fd12, fd22
	fmul	fd4, fd18
	fmul	fd10, fd30
	fmul	fd18, fd8, fd28
	fmul	fd6, fd16, fd26
	fmul	fd14, fd4, fd2
	fmul	fd24, fd12, fd22
	fmul	fd0, fd14, fd10
	fmul	fd28, fd6, fd24
	fmul	fd20, fd2, fd16
	fmul	fd30, fd26, fd12
	fmul	fd22, fd8, fd4
	fmul	fd18, fd0, fd10
	fmul	fd30, fd20, fd18
	fdiv	fd8, fd28
	fdiv	fd16, fd26
	fdiv	fd4, fd2
	fdiv	fd12, fd22
	fdiv	fd14, fd10
	fdiv	fd6, fd24
	fdiv	fd2, fd16
	fdiv	fd26, fd12
	fdiv	fd8, fd4
	fdiv	fd0, fd10
	fdiv	fd20, fd18
	fdiv	fd28, fd6, fd16
	fdiv	fd26, fd14, fd4
	fdiv	fd2, fd24, fd12
	fdiv	fd22, fd0, fd14
	fdiv	fd10, fd28, fd6
	fdiv	fd24, fd20, fd2
	fdiv	fd16, fd30, fd26
	fdiv	fd12, fd22, fd8
	fdiv	fd4, fd18, fd0
	fdiv	fd10, fd30, fd20
	fdiv	fd18, fd8, fd28
fpconv:
	ftoi	fs27, fs8
	ftoi	fs21, fs15
	ftoi	fs22, fs9
	ftoi	fs16, fs10
	ftoi	fs23, fs4
	ftoi	fs17, fs11
	ftoi	fs18, fs5
	ftoi	fs12, fs6
	ftoi	fs19, fs0
	ftoi	fs13, fs7
	ftoi	fs14, fs1
	itof	fs8, fs2
	itof	fs15, fs28
	itof	fs9, fs3
	itof	fs10, fs29
	itof	fs4, fs30
	itof	fs11, fs24
	itof	fs5, fs31
	itof	fs6, fs25
	itof	fs0, fs26
	itof	fs7, fs20
	itof	fs1, fs27
	ftod	fs2, fd14
	ftod	fs28, fd24
	ftod	fs3, fd0
	ftod	fs29, fd28
	ftod	fs30, fd20
	ftod	fs24, fd30
	ftod	fs31, fd22
	ftod	fs25, fd18
	ftod	fs26, fd30
	ftod	fs20, fd8
	ftod	fs27, fd16
	dtof	fd14, fs15
	dtof	fd24, fs9
	dtof	fd0, fs10
	dtof	fd28, fs4
	dtof	fd20, fs11
	dtof	fd30, fs5
	dtof	fd22, fs6
	dtof	fd18, fs0
	dtof	fd30, fs7
	dtof	fd8, fs1
	dtof	fd16, fs2
condjmp:
	fbeq	condjmp
	fbne	condjmp
	fbgt	condjmp
	fbge	condjmp
	fblt	condjmp
	fble	condjmp
	fbuo	condjmp
	fblg	condjmp
	fbleg	condjmp
	fbug	condjmp
	fbuge	condjmp
	fbul	condjmp
	fbule	condjmp
	fbue	condjmp
	fleq	
	flne	
	flgt	
	flge	
	fllt	
	flle	
	fluo	
	fllg	
	flleg	
	flug	
	fluge	
	flul	
	flule	
	flue	
