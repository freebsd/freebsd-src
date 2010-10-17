#objdump: -D
#as: -m68hc12
#name: 68HC12 movb movw instructions

.*:  +file format elf32\-m68hc12

Disassembly of section .text:

0+00 <\.text>:
[ ]+ 0:	86 00[ ]+ 	ldaa	#0
[ ]+ 2:	18 0a 0f 0f 	movb	15,X, 15,X
[ ]+ 6:	18 0a 0f 0f 	movb	15,X, 15,X
[ ]+ a:	18 0a 0f 0f 	movb	15,X, 15,X
[ ]+ e:	86 01[ ]+ 	ldaa	#1
[ ]+10:	18 0a 0f 0f 	movb	15,X, 15,X
[ ]+14:	18 0a 0f 0f 	movb	15,X, 15,X
[ ]+18:	18 0a 0f 0f 	movb	15,X, 15,X
[ ]+1c:	86 02[ ]+ 	ldaa	#2
[ ]+1e:	18 0a 0f 10 	movb	15,X, \-16,X
[ ]+22:	18 0a 0f 10 	movb	15,X, \-16,X
[ ]+26:	18 0a 0f 10 	movb	15,X, \-16,X
[ ]+2a:	86 03[ ]+ 	ldaa	#3
[ ]+2c:	18 0a 10 0f 	movb	\-16,X, 15,X
[ ]+30:	18 0a 10 0f 	movb	\-16,X, 15,X
[ ]+34:	18 0a 10 0f 	movb	\-16,X, 15,X
[ ]+38:	86 04[ ]+ 	ldaa	#4
[ ]+3a:	18 02 0f 0f 	movw	15,X, 15,X
[ ]+3e:	18 02 0f 0f 	movw	15,X, 15,X
[ ]+42:	18 02 0f 0f 	movw	15,X, 15,X
[ ]+46:	86 05[ ]+ 	ldaa	#5
[ ]+48:	18 02 0f 0f 	movw	15,X, 15,X
[ ]+4c:	18 02 0f 0f 	movw	15,X, 15,X
[ ]+50:	18 02 0f 0f 	movw	15,X, 15,X
[ ]+54:	86 06[ ]+ 	ldaa	#6
[ ]+56:	18 02 0f 10 	movw	15,X, \-16,X
[ ]+5a:	18 02 0f 10 	movw	15,X, \-16,X
[ ]+5e:	18 02 0f 10 	movw	15,X, \-16,X
[ ]+62:	86 07[ ]+ 	ldaa	#7
[ ]+64:	18 02 10 0f 	movw	\-16,X, 15,X
[ ]+68:	18 02 10 0f 	movw	\-16,X, 15,X
[ ]+6c:	18 02 10 0f 	movw	\-16,X, 15,X
[ ]+70:	86 08[ ]+ 	ldaa	#8
[ ]+72:	18 0a 4f 4f 	movb	15,Y, 15,Y
[ ]+76:	18 0a 4f 4f 	movb	15,Y, 15,Y
[ ]+7a:	18 0a 4f 4f 	movb	15,Y, 15,Y
[ ]+7e:	86 09[ ]+ 	ldaa	#9
[ ]+80:	18 0a 4f 4f 	movb	15,Y, 15,Y
[ ]+84:	18 0a 4f 4f 	movb	15,Y, 15,Y
[ ]+88:	18 0a 4f 4f 	movb	15,Y, 15,Y
[ ]+8c:	86 0a[ ]+ 	ldaa	#10
[ ]+8e:	18 0a 4f 50 	movb	15,Y, \-16,Y
[ ]+92:	18 0a 4f 50 	movb	15,Y, \-16,Y
[ ]+96:	18 0a 4f 50 	movb	15,Y, \-16,Y
[ ]+9a:	86 0b[ ]+ 	ldaa	#11
[ ]+9c:	18 0a 50 4f 	movb	\-16,Y, 15,Y
[ ]+a0:	18 0a 50 4f 	movb	\-16,Y, 15,Y
[ ]+a4:	18 0a 50 4f 	movb	\-16,Y, 15,Y
[ ]+a8:	86 0c[ ]+	ldaa	#12
[ ]+aa:	18 02 4f 4f 	movw	15,Y, 15,Y
[ ]+ae:	18 02 4f 4f 	movw	15,Y, 15,Y
[ ]+b2:	18 02 4f 4f 	movw	15,Y, 15,Y
[ ]+b6:	86 0d[ ]+ 	ldaa	#13
[ ]+b8:	18 02 4f 4f 	movw	15,Y, 15,Y
[ ]+bc:	18 02 4f 4f 	movw	15,Y, 15,Y
[ ]+c0:	18 02 4f 4f 	movw	15,Y, 15,Y
[ ]+c4:	86 0e[ ]+ 	ldaa	#14
[ ]+c6:	18 02 4f 50 	movw	15,Y, \-16,Y
[ ]+ca:	18 02 4f 50 	movw	15,Y, \-16,Y
[ ]+ce:	18 02 4f 50 	movw	15,Y, \-16,Y
[ ]+d2:	86 0f[ ]+ 	ldaa	#15
[ ]+d4:	18 02 50 4f 	movw	\-16,Y, 15,Y
[ ]+d8:	18 02 50 4f 	movw	\-16,Y, 15,Y
[ ]+dc:	18 02 50 4f 	movw	\-16,Y, 15,Y
[ ]+e0:	86 10[ ]+ 	ldaa	#16
[ ]+e2:	18 0a 4f cf 	movb	15,Y, 15,PC \{f5 <cat2\+0xe6>\}
[ ]+e6:	18 0a 4f cf 	movb	15,Y, 15,PC \{f9 <cat2\+0xea>\}
[ ]+ea:	18 0a 4f cf 	movb	15,Y, 15,PC \{fd <cat2\+0xee>\}
[ ]+ee:	86 11[ ]+ 	ldaa	#17
[ ]+f0:	18 0a 4f cf 	movb	15,Y, 15,PC \{103 <cat2\+0xf4>\}
[ ]+f4:	18 0a 4f cf 	movb	15,Y, 15,PC \{107 <cat2\+0xf8>\}
[ ]+f8:	18 0a 4f cf 	movb	15,Y, 15,PC \{10b <cat2\+0xfc>\}
[ ]+fc:	86 12[ ]+ 	ldaa	#18
[ ]+fe:	18 0a 4f d0 	movb	15,Y, \-16,PC \{f2 <cat2\+0xe3>\}
 102:	18 0a 4f d0 	movb	15,Y, \-16,PC \{f6 <cat2\+0xe7>\}
 106:	18 0a 4f d0 	movb	15,Y, \-16,PC \{fa <cat2\+0xeb>\}
 10a:	86 13[ ]+ 	ldaa	#19
 10c:	18 0a 50 cf 	movb	\-16,Y, 15,PC \{11f <cat2\+0x110>\}
 110:	18 0a 50 cf 	movb	\-16,Y, 15,PC \{123 <cat2\+0x114>\}
 114:	18 0a 50 cf 	movb	\-16,Y, 15,PC \{127 <cat2\+0x118>\}
 118:	86 14[ ]+ 	ldaa	#20
 11a:	18 02 4f cf 	movw	15,Y, 15,PC \{12d <cat2\+0x11e>\}
 11e:	18 02 4f cf 	movw	15,Y, 15,PC \{131 <cat2\+0x122>\}
 122:	18 02 4f cf 	movw	15,Y, 15,PC \{135 <cat2\+0x126>\}
 126:	86 15[ ]+ 	ldaa	#21
 128:	18 02 4f cf 	movw	15,Y, 15,PC \{13b <cat2\+0x12c>\}
 12c:	18 02 4f cf 	movw	15,Y, 15,PC \{13f <cat2\+0x130>\}
 130:	18 02 4f cf 	movw	15,Y, 15,PC \{143 <cat2\+0x134>\}
 134:	86 16[ ]+ 	ldaa	#22
 136:	18 02 4f d0 	movw	15,Y, \-16,PC \{12a <cat2\+0x11b>\}
 13a:	18 02 4f d0 	movw	15,Y, \-16,PC \{12e <cat2\+0x11f>\}
 13e:	18 02 4f d0 	movw	15,Y, \-16,PC \{132 <cat2\+0x123>\}
 142:	86 17[ ]+ 	ldaa	#23
 144:	18 02 50 cf 	movw	\-16,Y, 15,PC \{157 <cat2\+0x148>\}
 148:	18 02 50 cf 	movw	\-16,Y, 15,PC \{15b <cat2\+0x14c>\}
 14c:	18 02 50 cf 	movw	\-16,Y, 15,PC \{15f <cat2\+0x150>\}
 150:	86 18[ ]+ 	ldaa	#24
 152:	18 0a 8f cf 	movb	15,SP, 15,PC \{165 <cat2\+0x156>\}
 156:	18 0a 8f cf 	movb	15,SP, 15,PC \{169 <cat2\+0x15a>\}
 15a:	18 0a 8f cf 	movb	15,SP, 15,PC \{16d <cat2\+0x15e>\}
 15e:	86 19[ ]+ 	ldaa	#25
 160:	18 0a 8f cf 	movb	15,SP, 15,PC \{173 <cat2\+0x164>\}
 164:	18 0a 8f cf 	movb	15,SP, 15,PC \{177 <cat2\+0x168>\}
 168:	18 0a 8f cf 	movb	15,SP, 15,PC \{17b <cat2\+0x16c>\}
 16c:	86 1a[ ]+ 	ldaa	#26
 16e:	18 0a 8f d0 	movb	15,SP, \-16,PC \{162 <cat2\+0x153>\}
 172:	18 0a 8f d0 	movb	15,SP, \-16,PC \{166 <cat2\+0x157>\}
 176:	18 0a 8f d0 	movb	15,SP, \-16,PC \{16a <cat2\+0x15b>\}
 17a:	86 1b[ ]+ 	ldaa	#27
 17c:	18 0a 90 cf 	movb	\-16,SP, 15,PC \{18f <cat2\+0x180>\}
 180:	18 0a 90 cf 	movb	\-16,SP, 15,PC \{193 <cat2\+0x184>\}
 184:	18 0a 90 cf 	movb	\-16,SP, 15,PC \{197 <cat2\+0x188>\}
 188:	86 1c[ ]+ 	ldaa	#28
 18a:	18 02 8f cf 	movw	15,SP, 15,PC \{19d <cat2\+0x18e>\}
 18e:	18 02 8f cf 	movw	15,SP, 15,PC \{1a1 <cat2\+0x192>\}
 192:	18 02 8f cf 	movw	15,SP, 15,PC \{1a5 <cat2\+0x196>\}
 196:	86 1d[ ]+ 	ldaa	#29
 198:	18 02 8f cf 	movw	15,SP, 15,PC \{1ab <cat2\+0x19c>\}
 19c:	18 02 8f cf 	movw	15,SP, 15,PC \{1af <cat2\+0x1a0>\}
 1a0:	18 02 8f cf 	movw	15,SP, 15,PC \{1b3 <cat2\+0x1a4>\}
 1a4:	86 1e[ ]+ 	ldaa	#30
 1a6:	18 02 8f d0 	movw	15,SP, \-16,PC \{19a <cat2\+0x18b>\}
 1aa:	18 02 8f d0 	movw	15,SP, \-16,PC \{19e <cat2\+0x18f>\}
 1ae:	18 02 8f d0 	movw	15,SP, \-16,PC \{1a2 <cat2\+0x193>\}
 1b2:	86 1f[ ]+ 	ldaa	#31
 1b4:	18 02 90 cf 	movw	\-16,SP, 15,PC \{1c7 <cat2\+0x1b8>\}
 1b8:	18 02 90 cf 	movw	\-16,SP, 15,PC \{1cb <cat2\+0x1bc>\}
 1bc:	18 02 90 cf 	movw	\-16,SP, 15,PC \{1cf <cat2\+0x1c0>\}
 1c0:	86 20[ ]+ 	ldaa	#32
 1c2:	18 09 0f 10 	movb	1000 <cat2\+0xff1>, 15,X
 1c6:	00 
 1c7:	18 09 0f 10 	movb	1000 <cat2\+0xff1>, 15,X
 1cb:	00 
 1cc:	18 09 0f 10 	movb	1000 <cat2\+0xff1>, 15,X
 1d0:	00 
 1d1:	86 21[ ]+ 	ldaa	#33
 1d3:	18 0d 0f 10 	movb	15,X, 1000 <cat2\+0xff1>
 1d7:	00 
 1d8:	18 0d 0f 10 	movb	15,X, 1000 <cat2\+0xff1>
 1dc:	00 
 1dd:	18 0d 0f 10 	movb	15,X, 1000 <cat2\+0xff1>
 1e1:	00 
 1e2:	86 22[ ]+ 	ldaa	#34
 1e4:	18 09 10 10 	movb	1000 <cat2\+0xff1>, \-16,X
 1e8:	00 
 1e9:	18 09 10 10 	movb	1000 <cat2\+0xff1>, \-16,X
 1ed:	00 
 1ee:	18 09 10 10 	movb	1000 <cat2\+0xff1>, \-16,X
 1f2:	00 
 1f3:	86 23[ ]+ 	ldaa	#35
 1f5:	18 0d 10 10 	movb	\-16,X, 1000 <cat2\+0xff1>
 1f9:	00 
 1fa:	18 0d 10 10 	movb	\-16,X, 1000 <cat2\+0xff1>
 1fe:	00 
 1ff:	18 0d 10 10 	movb	\-16,X, 1000 <cat2\+0xff1>
 203:	00 
 204:	86 24[ ]+ 	ldaa	#36
 206:	18 01 0f 10 	movw	1002 <cat2\+0xff3>, 15,X
 20a:	02 
 20b:	18 01 0f 10 	movw	1002 <cat2\+0xff3>, 15,X
 20f:	02 
 210:	18 01 0f 10 	movw	1002 <cat2\+0xff3>, 15,X
 214:	02 
 215:	86 25[ ]+ 	ldaa	#37
 217:	18 05 0f 10 	movw	15,X, 1002 <cat2\+0xff3>
 21b:	02 
 21c:	18 05 0f 10 	movw	15,X, 1002 <cat2\+0xff3>
 220:	02 
 221:	18 05 0f 10 	movw	15,X, 1002 <cat2\+0xff3>
 225:	02 
 226:	86 26[ ]+ 	ldaa	#38
 228:	18 01 10 10 	movw	1002 <cat2\+0xff3>, \-16,X
 22c:	02 
 22d:	18 01 10 10 	movw	1002 <cat2\+0xff3>, \-16,X
 231:	02 
 232:	18 01 10 10 	movw	1002 <cat2\+0xff3>, \-16,X
 236:	02 
 237:	86 27[ ]+ 	ldaa	#39
 239:	18 05 10 10 	movw	\-16,X, 1002 <cat2\+0xff3>
 23d:	02 
 23e:	18 05 10 10 	movw	\-16,X, 1002 <cat2\+0xff3>
 242:	02 
 243:	18 05 10 10 	movw	\-16,X, 1002 <cat2\+0xff3>
 247:	02 
 248:	86 28[ ]+ 	ldaa	#40
 24a:	18 09 4f 10 	movb	1000 <cat2\+0xff1>, 15,Y
 24e:	00 
 24f:	18 09 4f 10 	movb	1000 <cat2\+0xff1>, 15,Y
 253:	00 
 254:	18 09 4f 10 	movb	1000 <cat2\+0xff1>, 15,Y
 258:	00 
 259:	86 29[ ]+ 	ldaa	#41
 25b:	18 0d 4f 10 	movb	15,Y, 1000 <cat2\+0xff1>
 25f:	00 
 260:	18 0d 4f 10 	movb	15,Y, 1000 <cat2\+0xff1>
 264:	00 
 265:	18 0d 4f 10 	movb	15,Y, 1000 <cat2\+0xff1>
 269:	00 
 26a:	86 2a[ ]+ 	ldaa	#42
 26c:	18 09 50 10 	movb	1000 <cat2\+0xff1>, \-16,Y
 270:	00 
 271:	18 09 50 10 	movb	1000 <cat2\+0xff1>, \-16,Y
 275:	00 
 276:	18 09 50 10 	movb	1000 <cat2\+0xff1>, \-16,Y
 27a:	00 
 27b:	86 2b[ ]+ 	ldaa	#43
 27d:	18 0d 50 10 	movb	\-16,Y, 1000 <cat2\+0xff1>
 281:	00 
 282:	18 0d 50 10 	movb	\-16,Y, 1000 <cat2\+0xff1>
 286:	00 
 287:	18 0d 50 10 	movb	\-16,Y, 1000 <cat2\+0xff1>
 28b:	00 
 28c:	86 2c[ ]+ 	ldaa	#44
 28e:	18 01 4f 10 	movw	1002 <cat2\+0xff3>, 15,Y
 292:	02 
 293:	18 01 4f 10 	movw	1002 <cat2\+0xff3>, 15,Y
 297:	02 
 298:	18 01 4f 10 	movw	1002 <cat2\+0xff3>, 15,Y
 29c:	02 
 29d:	86 2d[ ]+ 	ldaa	#45
 29f:	18 05 4f 10 	movw	15,Y, 1002 <cat2\+0xff3>
 2a3:	02 
 2a4:	18 05 4f 10 	movw	15,Y, 1002 <cat2\+0xff3>
 2a8:	02 
 2a9:	18 05 4f 10 	movw	15,Y, 1002 <cat2\+0xff3>
 2ad:	02 
 2ae:	86 2e[ ]+ 	ldaa	#46
 2b0:	18 01 50 10 	movw	1002 <cat2\+0xff3>, \-16,Y
 2b4:	02 
 2b5:	18 01 50 10 	movw	1002 <cat2\+0xff3>, \-16,Y
 2b9:	02 
 2ba:	18 01 50 10 	movw	1002 <cat2\+0xff3>, \-16,Y
 2be:	02 
 2bf:	86 2f[ ]+ 	ldaa	#47
 2c1:	18 05 50 10 	movw	\-16,Y, 1002 <cat2\+0xff3>
 2c5:	02 
 2c6:	18 05 50 10 	movw	\-16,Y, 1002 <cat2\+0xff3>
 2ca:	02 
 2cb:	18 05 50 10 	movw	\-16,Y, 1002 <cat2\+0xff3>
 2cf:	02 
 2d0:	86 30[ ]+ 	ldaa	#48
 2d2:	18 09 cf 10 	movb	1000 <cat2\+0xff1>, 15,PC \{2e4 <cat2\+0x2d5>\}
 2d6:	00 
 2d7:	18 09 cf 10 	movb	1000 <cat2\+0xff1>, 15,PC \{2e9 <cat2\+0x2da>\}
 2db:	00 
 2dc:	18 09 cf 10 	movb	1000 <cat2\+0xff1>, 15,PC \{2ee <cat2\+0x2df>\}
 2e0:	00 
 2e1:	86 31[ ]+ 	ldaa	#49
 2e3:	18 0d cf 10 	movb	15,PC \{2f5 <cat2\+0x2e6>\}, 1000 <cat2\+0xff1>
 2e7:	00 
 2e8:	18 0d cf 10 	movb	15,PC \{2fa <cat2\+0x2eb>\}, 1000 <cat2\+0xff1>
 2ec:	00 
 2ed:	18 0d cf 10 	movb	15,PC \{2ff <cat2\+0x2f0>\}, 1000 <cat2\+0xff1>
 2f1:	00 
 2f2:	86 32[ ]+ 	ldaa	#50
 2f4:	18 09 d0 10 	movb	1000 <cat2\+0xff1>, \-16,PC \{2e7 <cat2\+0x2d8>\}
 2f8:	00 
 2f9:	18 09 d0 10 	movb	1000 <cat2\+0xff1>, \-16,PC \{2ec <cat2\+0x2dd>\}
 2fd:	00 
 2fe:	18 09 d0 10 	movb	1000 <cat2\+0xff1>, \-16,PC \{2f1 <cat2\+0x2e2>\}
 302:	00 
 303:	86 33[ ]+ 	ldaa	#51
 305:	18 0d d0 10 	movb	\-16,PC \{2f8 <cat2\+0x2e9>\}, 1000 <cat2\+0xff1>
 309:	00 
 30a:	18 0d d0 10 	movb	\-16,PC \{2fd <cat2\+0x2ee>\}, 1000 <cat2\+0xff1>
 30e:	00 
 30f:	18 0d d0 10 	movb	\-16,PC \{302 <cat2\+0x2f3>\}, 1000 <cat2\+0xff1>
 313:	00 
 314:	86 34[ ]+ 	ldaa	#52
 316:	18 01 cf 10 	movw	1002 <cat2\+0xff3>, 15,PC \{328 <cat2\+0x319>\}
 31a:	02 
 31b:	18 01 cf 10 	movw	1002 <cat2\+0xff3>, 15,PC \{32d <cat2\+0x31e>\}
 31f:	02 
 320:	18 01 cf 10 	movw	1002 <cat2\+0xff3>, 15,PC \{332 <cat2\+0x323>\}
 324:	02 
 325:	86 35[ ]+ 	ldaa	#53
 327:	18 05 cf 10 	movw	15,PC \{339 <cat2\+0x32a>\}, 1002 <cat2\+0xff3>
 32b:	02 
 32c:	18 05 cf 10 	movw	15,PC \{33e <cat2\+0x32f>\}, 1002 <cat2\+0xff3>
 330:	02 
 331:	18 05 cf 10 	movw	15,PC \{343 <cat2\+0x334>\}, 1002 <cat2\+0xff3>
 335:	02 
 336:	86 36[ ]+ 	ldaa	#54
 338:	18 01 d0 10 	movw	1002 <cat2\+0xff3>, \-16,PC \{32b <cat2\+0x31c>\}
 33c:	02 
 33d:	18 01 d0 10 	movw	1002 <cat2\+0xff3>, \-16,PC \{330 <cat2\+0x321>\}
 341:	02 
 342:	18 01 d0 10 	movw	1002 <cat2\+0xff3>, \-16,PC \{335 <cat2\+0x326>\}
 346:	02 
 347:	86 37[ ]+ 	ldaa	#55
 349:	18 05 d0 10 	movw	\-16,PC \{33c <cat2\+0x32d>\}, 1002 <cat2\+0xff3>
 34d:	02 
 34e:	18 05 d0 10 	movw	\-16,PC \{341 <cat2\+0x332>\}, 1002 <cat2\+0xff3>
 352:	02 
 353:	18 05 d0 10 	movw	\-16,PC \{346 <cat2\+0x337>\}, 1002 <cat2\+0xff3>
 357:	02 
 358:	86 38[ ]+ 	ldaa	#56
 35a:	18 09 8f 10 	movb	1000 <cat2\+0xff1>, 15,SP
 35e:	00 
 35f:	18 09 8f 10 	movb	1000 <cat2\+0xff1>, 15,SP
 363:	00 
 364:	18 09 8f 10 	movb	1000 <cat2\+0xff1>, 15,SP
 368:	00 
 369:	86 39[ ]+ 	ldaa	#57
 36b:	18 0d 8f 10 	movb	15,SP, 1000 <cat2\+0xff1>
 36f:	00 
 370:	18 0d 8f 10 	movb	15,SP, 1000 <cat2\+0xff1>
 374:	00 
 375:	18 0d 8f 10 	movb	15,SP, 1000 <cat2\+0xff1>
 379:	00 
 37a:	86 3a[ ]+ 	ldaa	#58
 37c:	18 09 90 10 	movb	1000 <cat2\+0xff1>, \-16,SP
 380:	00 
 381:	18 09 90 10 	movb	1000 <cat2\+0xff1>, \-16,SP
 385:	00 
 386:	18 09 90 10 	movb	1000 <cat2\+0xff1>, \-16,SP
 38a:	00 
 38b:	86 3b[ ]+ 	ldaa	#59
 38d:	18 0d 90 10 	movb	\-16,SP, 1000 <cat2\+0xff1>
 391:	00 
 392:	18 0d 90 10 	movb	\-16,SP, 1000 <cat2\+0xff1>
 396:	00 
 397:	18 0d 90 10 	movb	\-16,SP, 1000 <cat2\+0xff1>
 39b:	00 
 39c:	86 3c[ ]+ 	ldaa	#60
 39e:	18 01 8f 10 	movw	1002 <cat2\+0xff3>, 15,SP
 3a2:	02 
 3a3:	18 01 8f 10 	movw	1002 <cat2\+0xff3>, 15,SP
 3a7:	02 
 3a8:	18 01 8f 10 	movw	1002 <cat2\+0xff3>, 15,SP
 3ac:	02 
 3ad:	86 3d[ ]+ 	ldaa	#61
 3af:	18 05 8f 10 	movw	15,SP, 1002 <cat2\+0xff3>
 3b3:	02 
 3b4:	18 05 8f 10 	movw	15,SP, 1002 <cat2\+0xff3>
 3b8:	02 
 3b9:	18 05 8f 10 	movw	15,SP, 1002 <cat2\+0xff3>
 3bd:	02 
 3be:	86 3e[ ]+ 	ldaa	#62
 3c0:	18 01 90 10 	movw	1002 <cat2\+0xff3>, \-16,SP
 3c4:	02 
 3c5:	18 01 90 10 	movw	1002 <cat2\+0xff3>, \-16,SP
 3c9:	02 
 3ca:	18 01 90 10 	movw	1002 <cat2\+0xff3>, \-16,SP
 3ce:	02 
 3cf:	86 3f[ ]+ 	ldaa	#63
 3d1:	18 05 90 10 	movw	\-16,SP, 1002 <cat2\+0xff3>
 3d5:	02 
 3d6:	18 05 90 10 	movw	\-16,SP, 1002 <cat2\+0xff3>
 3da:	02 
 3db:	18 05 90 10 	movw	\-16,SP, 1002 <cat2\+0xff3>
 3df:	02 
 3e0:	86 40[ ]+ 	ldaa	#64
 3e2:	18 08 07 aa 	movb	#170, 7,X
 3e6:	18 08 07 aa 	movb	#170, 7,X
 3ea:	18 08 07 aa 	movb	#170, 7,X
 3ee:	86 41[ ]+ 	ldaa	#65
 3f0:	18 08 18 aa 	movb	#170, \-8,X
 3f4:	18 08 18 aa 	movb	#170, \-8,X
 3f8:	18 08 18 aa 	movb	#170, \-8,X
 3fc:	86 42[ ]+ 	ldaa	#66
 3fe:	18 00 07 00 	movw	#44 <cat2\+0x35>, 7,X
 402:	44 
 403:	18 00 07 00 	movw	#44 <cat2\+0x35>, 7,X
 407:	44 
 408:	18 00 07 00 	movw	#44 <cat2\+0x35>, 7,X
 40c:	44 
 40d:	86 43[ ]+ 	ldaa	#67
 40f:	18 00 18 00 	movw	#44 <cat2\+0x35>, \-8,X
 413:	44 
 414:	18 00 18 00 	movw	#44 <cat2\+0x35>, \-8,X
 418:	44 
 419:	18 00 18 00 	movw	#44 <cat2\+0x35>, \-8,X
 41d:	44 
 41e:	86 44[ ]+ 	ldaa	#68
 420:	18 08 47 aa 	movb	#170, 7,Y
 424:	18 08 47 aa 	movb	#170, 7,Y
 428:	18 08 47 aa 	movb	#170, 7,Y
 42c:	86 45[ ]+ 	ldaa	#69
 42e:	18 08 58 aa 	movb	#170, \-8,Y
 432:	18 08 58 aa 	movb	#170, \-8,Y
 436:	18 08 58 aa 	movb	#170, \-8,Y
 43a:	86 46[ ]+ 	ldaa	#70
 43c:	18 00 47 00 	movw	#44 <cat2\+0x35>, 7,Y
 440:	44 
 441:	18 00 47 00 	movw	#44 <cat2\+0x35>, 7,Y
 445:	44 
 446:	18 00 47 00 	movw	#44 <cat2\+0x35>, 7,Y
 44a:	44 
 44b:	86 47[ ]+ 	ldaa	#71
 44d:	18 00 58 00 	movw	#44 <cat2\+0x35>, \-8,Y
 451:	44 
 452:	18 00 58 00 	movw	#44 <cat2\+0x35>, \-8,Y
 456:	44 
 457:	18 00 58 00 	movw	#44 <cat2\+0x35>, \-8,Y
 45b:	44 
 45c:	86 48[ ]+ 	ldaa	#72
 45e:	18 08 c7 aa 	movb	#170, 7,PC \{468 <cat2\+0x459>\}
 462:	18 08 c7 aa 	movb	#170, 7,PC \{46c <cat2\+0x45d>\}
 466:	18 08 c7 aa 	movb	#170, 7,PC \{470 <cat2\+0x461>\}
 46a:	86 49[ ]+ 	ldaa	#73
 46c:	18 08 d8 aa 	movb	#170, \-8,PC \{467 <cat2\+0x458>\}
 470:	18 08 d8 aa 	movb	#170, \-8,PC \{46b <cat2\+0x45c>\}
 474:	18 08 d8 aa 	movb	#170, \-8,PC \{46f <cat2\+0x460>\}
 478:	86 4a[ ]+ 	ldaa	#74
 47a:	18 00 c7 00 	movw	#44 <cat2\+0x35>, 7,PC \{484 <cat2\+0x475>\}
 47e:	44 
 47f:	18 00 c7 00 	movw	#44 <cat2\+0x35>, 7,PC \{489 <cat2\+0x47a>\}
 483:	44 
 484:	18 00 c7 00 	movw	#44 <cat2\+0x35>, 7,PC \{48e <cat2\+0x47f>\}
 488:	44 
 489:	86 4b[ ]+ 	ldaa	#75
 48b:	18 00 d8 00 	movw	#44 <cat2\+0x35>, \-8,PC \{486 <cat2\+0x477>\}
 48f:	44 
 490:	18 00 d8 00 	movw	#44 <cat2\+0x35>, \-8,PC \{48b <cat2\+0x47c>\}
 494:	44 
 495:	18 00 d8 00 	movw	#44 <cat2\+0x35>, \-8,PC \{490 <cat2\+0x481>\}
 499:	44 
 49a:	86 4c[ ]+ 	ldaa	#76
 49c:	18 08 87 aa 	movb	#170, 7,SP
 4a0:	18 08 87 aa 	movb	#170, 7,SP
 4a4:	18 08 87 aa 	movb	#170, 7,SP
 4a8:	86 4d[ ]+ 	ldaa	#77
 4aa:	18 08 98 aa 	movb	#170, \-8,SP
 4ae:	18 08 98 aa 	movb	#170, \-8,SP
 4b2:	18 08 98 aa 	movb	#170, \-8,SP
 4b6:	86 4e[ ]+ 	ldaa	#78
 4b8:	18 00 87 00 	movw	#44 <cat2\+0x35>, 7,SP
 4bc:	44 
 4bd:	18 00 87 00 	movw	#44 <cat2\+0x35>, 7,SP
 4c1:	44 
 4c2:	18 00 87 00 	movw	#44 <cat2\+0x35>, 7,SP
 4c6:	44 
 4c7:	86 4f[ ]+ 	ldaa	#79
 4c9:	18 00 98 00 	movw	#44 <cat2\+0x35>, \-8,SP
 4cd:	44 
 4ce:	18 00 98 00 	movw	#44 <cat2\+0x35>, \-8,SP
 4d2:	44 
 4d3:	18 00 98 00 	movw	#44 <cat2\+0x35>, \-8,SP
 4d7:	44 
 4d8:	86 50[ ]+ 	ldaa	#80

