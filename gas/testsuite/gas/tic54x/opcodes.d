#objdump: -d
#name: c54x opcode list

.*:     file format .*c54x.*

Disassembly of section .text:

0+000 <_opcodes>:
   0:	e39a 	abdst  \*ar3\+,\*ar4\+
   1:	f485 	abs    a
   2:	f585 	abs    a,b
   3:	0090 	add    \*ar0\+,a
   4:	0491 	add    \*ar1\+,ts,a
   5:	3c92 	add    \*ar2\+,16,a
   6:	6f93 	add    \*ar3\+,a,b
   7:	0d00 
   8:	90a1 	add    \*ar4\+,1,a
   9:	a09a 	add    \*ar3\+,\*ar4\+,a
   a:	f000 	add    #-32768,a
   b:	8000 
   c:	f160 	add    #0,16,a,b
   d:	0000 
   e:	f510 	add    a,-16,b
   f:	f580 	add    a,asm,b
  10:	0690 	addc   \*ar0\+,a
  11:	6b91 	addm   #1,\*ar1\+
  12:	0001 
  13:	0292 	adds   \*ar2\+,a
  14:	1893 	and    \*ar3\+,a
  15:	f131 	and    #1,1,a,b
  16:	0001 
  17:	f163 	and    #1,16,a,b
  18:	0001 
  19:	f080 	and    a
  1a:	6890 	andm   #1,\*ar0\+
  1b:	0001 
  1c:	f073 	b      11c <_opcodes_end>
  1d:	011c 
  1e:	f273 	bd     11c <_opcodes_end>
  1f:	011c 
  20:	f495 	nop    
  21:	f495 	nop    
  22:	f4e2 	bacc   a
  23:	f7e2 	baccd  b
  24:	f495 	nop    
  25:	f495 	nop    
  26:	6c91 	banz   11c <_opcodes_end>,\*ar1\+
  27:	011c 
  28:	6e92 	banzd  11c <_opcodes_end>,\*ar2\+
  29:	011c 
  2a:	f495 	nop    
  2b:	f495 	nop    
  2c:	f875 	bc     11c <_opcodes_end>,aeq, aov
  2d:	011c 
  2e:	fa3f 	bcd    11c <_opcodes_end>,tc, c, bio
  2f:	011c 
  30:	f495 	nop    
  31:	f495 	nop    
  32:	9691 	bit    \*ar3\+,1
  33:	6194 	bitf   \*ar4\+,#-1
  34:	ffff 
  35:	3495 	bitt   \*ar5\+
  36:	f4e3 	cala   a
  37:	f7e3 	calad  b
  38:	f495 	nop    
  39:	f495 	nop    
  3a:	f074 	call   11c <_opcodes_end>
  3b:	011c 
  3c:	f274 	calld  11c <_opcodes_end>
  3d:	011c 
  3e:	f495 	nop    
  3f:	f495 	nop    
  40:	f930 	cc     11c <_opcodes_end>,tc
  41:	011c 
  42:	fb45 	ccd    11c <_opcodes_end>,aeq
  43:	011c 
  44:	f495 	nop    
  45:	f495 	nop    
  46:	f693 	cmpl   b,a
  47:	6090 	cmpm   \*ar0\+,#1
  48:	0001 
  49:	f5a9 	cmpr   lt,ar1
  4a:	8e92 	cmps   a,\*ar2\+
  4b:	518b 	dadd   \*ar3-,a,b
  4c:	5a8c 	dadst  \*ar4-,a
  4d:	4d95 	delay  \*ar5\+
  4e:	568e 	dld    \*ar6-,a
  4f:	598f 	drsub  \*ar7-,b
  50:	5e88 	dsadt  \*ar0-,a
  51:	4e89 	dst    a,\*ar1-
  52:	558a 	dsub   \*ar2-,b
  53:	5c8b 	dsubt  \*ar3-,a
  54:	f48e 	exp    a
  55:	e09a 	firs   \*ar3\+,\*ar4\+,11c <_opcodes_end>
  56:	011c 
  57:	ee80 	frame  -128
  58:	f6e1 	idle   2
  59:	f7cf 	intr   15
  5a:	1090 	ld     \*ar0\+,a
  5b:	1491 	ld     \*ar1\+,ts,a
  5c:	4492 	ld     \*ar2\+,16,a
  5d:	9491 	ld     \*ar3\+,1,a
  5e:	94a1 	ld     \*ar4\+,1,a
  5f:	e901 	ld     #1,b
  60:	f021 	ld     #32767,1,a
  61:	7fff 
  62:	f062 	ld     #32767,16,a
  63:	7fff 
  64:	f582 	ld     a,asm,b
  65:	f541 	ld     a,1,b
  66:	3090 	ld     \*ar0\+,t
  67:	4691 	ld     \*ar1\+,dp
  68:	ea02 	ld     #2,dp
  69:	ed0f 	ld     #15,asm
  6a:	f4a7 	ld     #7,arp
  6b:	3292 	ld     \*ar2\+,asm
  6c:	4813 	ldm    ar3,a
  6d:	a889 	ld     \*ar2\+,a || mac    \*ar3\+,a
  6e:	abab 	ld     \*ar4\+,b || macr   \*ar5\+,b
  6f:	ac89 	ld     \*ar2\+,a || mas    \*ar3\+,a
  70:	afab 	ld     \*ar4\+,b || masr   \*ar5\+,b
  71:	1696 	ldr    \*ar6\+,a
  72:	1297 	ldu    \*ar7\+,a
  73:	e19a 	lms    \*ar3\+,\*ar4\+
  74:	4c90 	ltd    \*ar0\+
  75:	2891 	mac    \*ar1\+,a
  76:	2a92 	macr   \*ar2\+,a
  77:	b189 	mac    \*ar2\+,\*ar3\+,a,b
  78:	b5ab 	macr   \*ar4\+,\*ar5\+,a,b
  79:	f167 	mac    #1,a,b
  7a:	0001 
  7b:	6490 	mac    \*ar0\+,#1,a
  7c:	0001 
  7d:	3591 	maca   \*ar1\+,b
  7e:	f588 	maca   t,a,b
  7f:	7a92 	macd   \*ar2\+,11c <_opcodes_end>,a
  80:	011c 
  81:	7893 	macp   \*ar3\+,11c <_opcodes_end>,a
  82:	011c 
  83:	a6ab 	macsu  \*ar4\+,\*ar5\+,a
  84:	6d96 	mar    \*ar6\+
  85:	2c97 	mas    \*ar7\+,a
  86:	2e90 	masr   \*ar0\+,a
  87:	b99a 	mas    \*ar3\+,\*ar4\+,a,b
  88:	bd8b 	masr   \*ar2\+,\*ar5\+,a,b
  89:	3396 	masa   \*ar6\+,b
  8a:	f58a 	masa   t,a,b
  8b:	f48b 	masar  t,a
  8c:	f486 	max    a
  8d:	f587 	min    b
  8e:	2097 	mpy    \*ar7\+,a
  8f:	a59a 	mpy    \*ar3\+,\*ar4\+,b
  90:	6280 	mpy    \*ar0,#1,a
  91:	0001 
  92:	f066 	mpy    #1,a
  93:	0001 
  94:	3190 	mpya   \*ar0\+
  95:	f58c 	mpya   b
  96:	2591 	mpyu   \*ar1\+,b
  97:	e589 	mvdd   \*ar2\+,\*ar3\+
  98:	7194 	mvdk   \*ar4\+,0 <_opcodes>
  99:	0000 
  9a:	7215 	mvdm   0 <_opcodes>,ar5
  9b:	0000 
  9c:	7d96 	mvdp   \*ar6\+,11c <_opcodes_end>
  9d:	011c 
  9e:	7097 	mvkd   0 <_opcodes>,\*ar7\+
  9f:	0000 
  a0:	7310 	mvmd   ar0,0 <_opcodes>
  a1:	0000 
  a2:	e712 	mvmm   ar1,ar2
  a3:	7c93 	mvpd   11c <_opcodes_end>,\*ar3\+
  a4:	011c 
  a5:	f584 	neg    a,b
  a6:	f495 	nop    
  a7:	f48f 	norm   a
  a8:	1b90 	or     \*ar0\+,b
  a9:	f340 	or     #7,b
  aa:	0007 
  ab:	f364 	or     #1,16,b
  ac:	0001 
  ad:	f3a0 	or     b
  ae:	6991 	orm    #1,\*ar1\+
  af:	0001 
  b0:	3692 	poly   \*ar2\+
  b1:	8b93 	popd   \*ar3\+
  b2:	8a14 	popm   ar4
  b3:	7495 	portr  pa0,\*ar5\+
  b4:	0000 
  b5:	7596 	portw  \*ar6\+,pa0
  b6:	0000 
  b7:	4b97 	pshd   \*ar7\+
  b8:	4a10 	pshm   ar0
  b9:	fc44 	rc     aneq
  ba:	fe46 	rcd    agt
  bb:	7e91 	reada  \*ar1\+
  bc:	f7e0 	reset  
  bd:	fc00 	ret    
  be:	fe00 	retd   
  bf:	f495 	nop    
  c0:	f495 	nop    
  c1:	f4eb 	rete   
  c2:	f6eb 	reted  
  c3:	f495 	nop    
  c4:	f495 	nop    
  c5:	f49b 	retf   
  c6:	f69b 	retfd  
  c7:	f491 	rol    a
  c8:	f492 	roltc  a
  c9:	f590 	ror    b
  ca:	4790 	rpt    \*ar0\+
  cb:	f495 	nop    
  cc:	ec20 	rpt    #32
  cd:	f495 	nop    
  ce:	f070 	rpt    #65535
  cf:	ffff 
  d0:	f495 	nop    
  d1:	f072 	rptb   11b <_opcodes\+0x11b>
  d2:	011b 
  d3:	f495 	nop    
  d4:	f272 	rptbd  11b <_opcodes\+0x11b>
  d5:	011b 
  d6:	f495 	nop    
  d7:	f495 	nop    
  d8:	f071 	rptz   a,#32767
  d9:	7fff 
  da:	f495 	nop    
  db:	f6bf 	rsbx   st1,braf
  dc:	9e93 	saccd  a,\*ar3\+,alt
  dd:	f483 	sat    a
  de:	f56f 	sfta   a,15,b
  df:	f494 	sftc   a
  e0:	f0ef 	sftl   a,15
  e1:	e289 	sqdst  \*ar2\+,\*ar3\+
  e2:	2794 	squr   \*ar4\+,b
  e3:	f48d 	squr   a,a
  e4:	3895 	squra  \*ar5\+,a
  e5:	3a96 	squrs  \*ar6\+,a
  e6:	9d87 	srccd  \*ar2\+,aleq
  e7:	f7bf 	ssbx   st1,braf
  e8:	8c90 	st     t,\*ar0\+
  e9:	8d91 	st     trn,\*ar1\+
  ea:	7692 	st     #32767,\*ar2\+
  eb:	7fff 
  ec:	8293 	sth    a,\*ar3\+
  ed:	8694 	sth    a,asm,\*ar4\+
  ee:	9abf 	sth    a,15,\*ar5\+
  ef:	6f96 	sth    a,-16,\*ar6\+
  f0:	0c70 
  f1:	8097 	stl    a,\*ar7\+
  f2:	8490 	stl    a,asm,\*ar0\+
  f3:	6f91 	stl    a,15,\*ar1\+
  f4:	0c8f 
  f5:	988f 	stl    a,15,\*ar2\+
  f6:	8813 	stlm   a,ar3
  f7:	7714 	stm    #32767,ar4
  f8:	7fff 
  f9:	c1ab 	st     a,\*ar5\+ || add    \*ar4\+,b
  fa:	c989 	st     a,\*ar3\+ || ld     \*ar2\+,b
  fb:	e4a9 	st     a,\*ar3\+ || ld     \*ar4\+,t
  fc:	d18b 	st     a,\*ar5\+ || mac    \*ar2\+,b
  fd:	dda9 	st     a,\*ar3\+ || masr   \*ar4\+,b
  fe:	cda9 	st     a,\*ar3\+ || mpy    \*ar4\+,b
  ff:	c5a9 	st     a,\*ar3\+ || sub    \*ar4\+,b
 100:	9cbd 	strcd  \*ar5\+,beq
 101:	0890 	sub    \*ar0\+,a
 102:	0c91 	sub    \*ar1\+,ts,a
 103:	4192 	sub    \*ar2\+,16,a,b
 104:	6f93 	sub    \*ar3\+,a,b
 105:	0d20 
 106:	92af 	sub    \*ar4\+,15,a
 107:	a3ba 	sub    \*ar5\+,\*ar4\+,b
 108:	f11f 	sub    #1,15,a,b
 109:	0001 
 10a:	f161 	sub    #1,16,a,b
 10b:	0001 
 10c:	f530 	sub    a,-16,b
 10d:	f581 	sub    a,asm,b
 10e:	0e90 	subb   \*ar0\+,a
 10f:	1e91 	subc   \*ar1\+,a
 110:	0a92 	subs   \*ar2\+,a
 111:	f4cf 	trap   15
 112:	7f93 	writa  \*ar3\+
 113:	fd70 	xc     1,aov
 114:	1c94 	xor    \*ar4\+,a
 115:	f050 	xor    #1,a
 116:	0001 
 117:	f065 	xor    #1,16,a
 118:	0001 
 119:	f1c1 	xor    a,1,b
 11a:	6a95 	xorm   #1,\*ar5\+
 11b:	0001 
