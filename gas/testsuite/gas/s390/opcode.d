#name: s390 opcode
#objdump: -drw

.*: +file format .*

Disassembly of section .text:

.* <foo>:
   0:	5a 65 af ff [	 ]*a	%r6,4095\(%r5,%r10\)
   4:	6a 65 af ff [	 ]*ad	%f6,4095\(%r5,%r10\)
   8:	ed 65 af ff 00 1a [	 ]*adb	%f6,4095\(%r5,%r10\)
   e:	b3 1a 00 69 [	 ]*adbr	%f6,%f9
  12:	2a 69 [	 ]*adr	%f6,%f9
  14:	7a 65 af ff [	 ]*ae	%f6,4095\(%r5,%r10\)
  18:	ed 65 af ff 00 0a [	 ]*aeb	%f6,4095\(%r5,%r10\)
  1e:	b3 0a 00 69 [	 ]*aebr	%f6,%f9
  22:	3a 69 [	 ]*aer	%f6,%f9
  24:	4a 65 af ff [	 ]*ah	%r6,4095\(%r5,%r10\)
  28:	a7 6a 80 01 [	 ]*ahi	%r6,-32767
  2c:	5e 65 af ff [	 ]*al	%r6,4095\(%r5,%r10\)
  30:	1e 69 [	 ]*alr	%r6,%r9
  32:	fa 58 5f ff af ff [	 ]*ap	4095\(6,%r5\),4095\(9,%r10\)
  38:	1a 69 [	 ]*ar	%r6,%r9
  3a:	7e 65 af ff [	 ]*au	%f6,4095\(%r5,%r10\)
  3e:	3e 69 [	 ]*aur	%f6,%f9
  40:	6e 65 af ff [	 ]*aw	%f6,4095\(%r5,%r10\)
  44:	2e 69 [	 ]*awr	%f6,%f9
  46:	b3 4a 00 69 [	 ]*axbr	%f6,%f9
  4a:	36 69 [	 ]*axr	%f6,%f9
  4c:	47 f5 af ff [	 ]*b	4095\(%r5,%r10\)
  50:	b2 40 00 69 [	 ]*bakr	%r6,%r9
  54:	45 65 af ff [	 ]*bal	%r6,4095\(%r5,%r10\)
  58:	05 69 [	 ]*balr	%r6,%r9
  5a:	4d 65 af ff [	 ]*bas	%r6,4095\(%r5,%r10\)
  5e:	0d 69 [	 ]*basr	%r6,%r9
  60:	0c 69 [	 ]*bassm	%r6,%r9
  62:	47 65 af ff [	 ]*blh	4095\(%r5,%r10\)
  66:	07 69 [	 ]*blhr	%r9
  68:	46 65 af ff [	 ]*bct	%r6,4095\(%r5,%r10\)
  6c:	06 69 [	 ]*bctr	%r6,%r9
  6e:	47 85 af ff [	 ]*be	4095\(%r5,%r10\)
  72:	07 89 [	 ]*ber	%r9
  74:	47 25 af ff [	 ]*bh	4095\(%r5,%r10\)
  78:	47 a5 af ff [	 ]*bhe	4095\(%r5,%r10\)
  7c:	07 a9 [	 ]*bher	%r9
  7e:	07 29 [	 ]*bhr	%r9
  80:	47 45 af ff [	 ]*bl	4095\(%r5,%r10\)
  84:	47 c5 af ff [	 ]*ble	4095\(%r5,%r10\)
  88:	07 c9 [	 ]*bler	%r9
  8a:	47 65 af ff [	 ]*blh	4095\(%r5,%r10\)
  8e:	07 69 [	 ]*blhr	%r9
  90:	07 49 [	 ]*blr	%r9
  92:	47 45 af ff [	 ]*bl	4095\(%r5,%r10\)
  96:	07 49 [	 ]*blr	%r9
  98:	47 75 af ff [	 ]*bne	4095\(%r5,%r10\)
  9c:	07 79 [	 ]*bner	%r9
  9e:	47 d5 af ff [	 ]*bnh	4095\(%r5,%r10\)
  a2:	47 55 af ff [	 ]*bnhe	4095\(%r5,%r10\)
  a6:	07 59 [	 ]*bnher	%r9
  a8:	07 d9 [	 ]*bnhr	%r9
  aa:	47 b5 af ff [	 ]*bnl	4095\(%r5,%r10\)
  ae:	47 35 af ff [	 ]*bnle	4095\(%r5,%r10\)
  b2:	07 39 [	 ]*bnler	%r9
  b4:	47 95 af ff [	 ]*bnlh	4095\(%r5,%r10\)
  b8:	07 99 [	 ]*bnlhr	%r9
  ba:	07 b9 [	 ]*bnlr	%r9
  bc:	47 b5 af ff [	 ]*bnl	4095\(%r5,%r10\)
  c0:	07 b9 [	 ]*bnlr	%r9
  c2:	47 e5 af ff [	 ]*bno	4095\(%r5,%r10\)
  c6:	07 e9 [	 ]*bnor	%r9
  c8:	47 d5 af ff [	 ]*bnh	4095\(%r5,%r10\)
  cc:	07 d9 [	 ]*bnhr	%r9
  ce:	47 75 af ff [	 ]*bne	4095\(%r5,%r10\)
  d2:	07 79 [	 ]*bner	%r9
  d4:	47 15 af ff [	 ]*bo	4095\(%r5,%r10\)
  d8:	07 19 [	 ]*bor	%r9
  da:	47 25 af ff [	 ]*bh	4095\(%r5,%r10\)
  de:	07 29 [	 ]*bhr	%r9
  e0:	07 f9 [	 ]*br	%r9
  e2:	a7 95 00 00 [	 ]*bras	%r9,e2 <foo\+0xe2>
  e6:	a7 64 00 00 [	 ]*jlh	e6 <foo\+0xe6>
  ea:	a7 66 00 00 [	 ]*brct	%r6,ea <foo\+0xea>
  ee:	84 69 00 00 [	 ]*brxh	%r6,%r9,ee <foo\+0xee>
  f2:	85 69 00 00 [	 ]*brxle	%r6,%r9,f2 <foo\+0xf2>
  f6:	b2 5a 00 69 [	 ]*bsa	%r6,%r9
  fa:	b2 58 00 69 [	 ]*bsg	%r6,%r9
  fe:	0b 69 [	 ]*bsm	%r6,%r9
 100:	86 69 5f ff [	 ]*bxh	%r6,%r9,4095\(%r5\)
 104:	87 69 5f ff [	 ]*bxle	%r6,%r9,4095\(%r5\)
 108:	47 85 af ff [	 ]*be	4095\(%r5,%r10\)
 10c:	07 89 [	 ]*ber	%r9
 10e:	59 65 af ff [	 ]*c	%r6,4095\(%r5,%r10\)
 112:	69 65 af ff [	 ]*cd	%f6,4095\(%r5,%r10\)
 116:	ed 65 af ff 00 19 [	 ]*cdb	%f6,4095\(%r5,%r10\)
 11c:	b3 19 00 69 [	 ]*cdbr	%f6,%f9
 120:	b3 95 00 69 [	 ]*cdfbr	%r6,%f9
 124:	29 69 [	 ]*cdr	%f6,%f9
 126:	bb 69 5f ff [	 ]*cds	%r6,%r9,4095\(%r5\)
 12a:	79 65 af ff [	 ]*ce	%f6,4095\(%r5,%r10\)
 12e:	ed 65 af ff 00 09 [	 ]*ceb	%f6,4095\(%r5,%r10\)
 134:	b3 09 00 69 [	 ]*cebr	%f6,%f9
 138:	b3 94 00 69 [	 ]*cefbr	%r6,%f9
 13c:	39 69 [	 ]*cer	%f6,%f9
 13e:	b2 1a 5f ff [	 ]*cfc	4095\(%r5\)
 142:	b3 99 50 69 [	 ]*cfdbr	%f6,5,%r9
 146:	b3 98 50 69 [	 ]*cfebr	%f6,5,%r9
 14a:	b3 9a 50 69 [	 ]*cfxbr	%f6,5,%r9
 14e:	49 65 af ff [	 ]*ch	%r6,4095\(%r5,%r10\)
 152:	a7 6e 80 01 [	 ]*chi	%r6,-32767
 156:	b2 41 00 69 [	 ]*cksm	%r6,%r9
 15a:	55 65 af ff [	 ]*cl	%r6,4095\(%r5,%r10\)
 15e:	d5 ff 5f ff af ff [	 ]*clc	4095\(256,%r5\),4095\(%r10\)
 164:	0f 69 [	 ]*clcl	%r6,%r9
 166:	a9 69 00 0a [	 ]*clcle	%r6,%r9,10
 16a:	95 ff 5f ff [	 ]*cli	4095\(%r5\),255
 16e:	bd 6a 5f ff [	 ]*clm	%r6,10,4095\(%r5\)
 172:	15 69 [	 ]*clr	%r6,%r9
 174:	b2 5d 00 69 [	 ]*clst	%r6,%r9
 178:	b2 63 00 69 [	 ]*cmpsc	%r6,%r9
 17c:	f9 58 5f ff af ff [	 ]*cp	4095\(6,%r5\),4095\(9,%r10\)
 182:	b2 4d 00 69 [	 ]*cpya	%a6,%a9
 186:	19 69 [	 ]*cr	%r6,%r9
 188:	ba 69 5f ff [	 ]*cs	%r6,%r9,4095\(%r5\)
 18c:	b2 30 00 00 [	 ]*csch
 190:	b2 50 00 69 [	 ]*csp	%r6,%r9
 194:	b2 57 00 69 [	 ]*cuse	%r6,%r9
 198:	b2 a7 00 69 [	 ]*cutfu	%r6,%r9
 19c:	b2 a6 00 69 [	 ]*cuutf	%r6,%r9
 1a0:	4f 65 af ff [	 ]*cvb	%r6,4095\(%r5,%r10\)
 1a4:	4e 65 af ff [	 ]*cvd	%r6,4095\(%r5,%r10\)
 1a8:	b3 49 00 69 [	 ]*cxbr	%f6,%f9
 1ac:	b3 96 00 69 [	 ]*cxfbr	%r6,%f9
 1b0:	5d 65 af ff [	 ]*d	%r6,4095\(%r5,%r10\)
 1b4:	6d 65 af ff [	 ]*dd	%f6,4095\(%r5,%r10\)
 1b8:	ed 65 af ff 00 1d [	 ]*ddb	%f6,4095\(%r5,%r10\)
 1be:	b3 1d 00 69 [	 ]*ddbr	%f6,%f9
 1c2:	2d 69 [	 ]*ddr	%f6,%f9
 1c4:	7d 65 af ff [	 ]*de	%f6,4095\(%r5,%r10\)
 1c8:	ed 65 af ff 00 0d [	 ]*deb	%f6,4095\(%r5,%r10\)
 1ce:	b3 0d 00 69 [	 ]*debr	%f6,%f9
 1d2:	3d 69 [	 ]*der	%f6,%f9
 1d4:	83 69 5f ff [	 ]*diag	%r6,%r9,4095\(%r5\)
 1d8:	b3 5b 9a 65 [	 ]*didbr	%f6,%f9,%f5,10
 1dc:	b3 53 9a 65 [	 ]*diebr	%f6,%f9,%f5,10
 1e0:	fd 58 5f ff af ff [	 ]*dp	4095\(6,%r5\),4095\(9,%r10\)
 1e6:	1d 69 [	 ]*dr	%r6,%r9
 1e8:	b3 4d 00 69 [	 ]*dxbr	%f6,%f9
 1ec:	b2 2d 00 60 [	 ]*dxr	%f6
 1f0:	b2 4f 00 69 [	 ]*ear	%r6,%a9
 1f4:	de ff 5f ff af ff [	 ]*ed	4095\(256,%r5\),4095\(%r10\)
 1fa:	df ff 5f ff af ff [	 ]*edmk	4095\(256,%r5\),4095\(%r10\)
 200:	b3 8c 00 69 [	 ]*efpc	%r6,%r9
 204:	b2 26 00 60 [	 ]*epar	%r6
 208:	b2 49 00 69 [	 ]*ereg	%r6,%r9
 20c:	b2 27 00 60 [	 ]*esar	%r6
 210:	b2 4a 00 69 [	 ]*esta	%r6,%r9
 214:	44 60 5f ff [	 ]*ex	%r6,4095\(%r5\)
 218:	b3 5f 50 69 [	 ]*fidbr	%f6,5,%f9
 21c:	b3 57 50 69 [	 ]*fiebr	%f6,5,%f9
 220:	b3 47 50 69 [	 ]*fixbr	%f6,5,%f9
 224:	24 69 [	 ]*hdr	%f6,%f9
 226:	34 69 [	 ]*her	%f6,%f9
 228:	b2 31 00 00 [	 ]*hsch
 22c:	b2 24 00 60 [	 ]*iac	%r6
 230:	43 65 af ff [	 ]*ic	%r6,4095\(%r5,%r10\)
 234:	bf 6a 5f ff [	 ]*icm	%r6,10,4095\(%r5\)
 238:	b2 0b 00 00 [	 ]*ipk
 23c:	b2 22 00 60 [	 ]*ipm	%r6
 240:	b2 21 00 69 [	 ]*ipte	%r6,%r9
 244:	b2 29 00 69 [	 ]*iske	%r6,%r9
 248:	b2 23 00 69 [	 ]*ivsk	%r6,%r9
 24c:	a7 f4 00 00 [	 ]*j	24c <foo\+0x24c>
 250:	a7 84 00 00 [	 ]*je	250 <foo\+0x250>
 254:	a7 24 00 00 [	 ]*jh	254 <foo\+0x254>
 258:	a7 a4 00 00 [	 ]*jhe	258 <foo\+0x258>
 25c:	a7 44 00 00 [	 ]*jl	25c <foo\+0x25c>
 260:	a7 c4 00 00 [	 ]*jle	260 <foo\+0x260>
 264:	a7 64 00 00 [	 ]*jlh	264 <foo\+0x264>
 268:	a7 44 00 00 [	 ]*jl	268 <foo\+0x268>
 26c:	a7 74 00 00 [	 ]*jne	26c <foo\+0x26c>
 270:	a7 54 00 00 [	 ]*jnhe	270 <foo\+0x270>
 274:	a7 b4 00 00 [	 ]*jnl	274 <foo\+0x274>
 278:	a7 34 00 00 [	 ]*jnle	278 <foo\+0x278>
 27c:	a7 94 00 00 [	 ]*jnlh	27c <foo\+0x27c>
 280:	a7 b4 00 00 [	 ]*jnl	280 <foo\+0x280>
 284:	a7 e4 00 00 [	 ]*jno	284 <foo\+0x284>
 288:	a7 d4 00 00 [	 ]*jnh	288 <foo\+0x288>
 28c:	a7 74 00 00 [	 ]*jne	28c <foo\+0x28c>
 290:	a7 14 00 00 [	 ]*jo	290 <foo\+0x290>
 294:	a7 24 00 00 [	 ]*jh	294 <foo\+0x294>
 298:	a7 84 00 00 [	 ]*je	298 <foo\+0x298>
 29c:	ed 65 af ff 00 18 [	 ]*kdb	%f6,4095\(%r5,%r10\)
 2a2:	b3 18 00 69 [	 ]*kdbr	%f6,%f9
 2a6:	ed 65 af ff 00 08 [	 ]*keb	%f6,4095\(%r5,%r10\)
 2ac:	b3 08 00 69 [	 ]*kebr	%f6,%f9
 2b0:	b3 48 00 69 [	 ]*kxbr	%f6,%f9
 2b4:	58 65 af ff [	 ]*l	%r6,4095\(%r5,%r10\)
 2b8:	41 65 af ff [	 ]*la	%r6,4095\(%r5,%r10\)
 2bc:	51 65 af ff [	 ]*lae	%r6,4095\(%r5,%r10\)
 2c0:	9a 69 5f ff [	 ]*lam	%a6,%a9,4095\(%r5\)
 2c4:	e5 00 5f ff af ff [	 ]*lasp	4095\(%r5\),4095\(%r10\)
 2ca:	b3 13 00 69 [	 ]*lcdbr	%f6,%f9
 2ce:	23 69 [	 ]*lcdr	%f6,%f9
 2d0:	b3 03 00 69 [	 ]*lcebr	%f6,%f9
 2d4:	33 69 [	 ]*lcer	%f6,%f9
 2d6:	13 69 [	 ]*lcr	%r6,%r9
 2d8:	b7 69 5f ff [	 ]*lctl	%c6,%c9,4095\(%r5\)
 2dc:	b3 43 00 69 [	 ]*lcxbr	%f6,%f9
 2e0:	68 60 5f ff [	 ]*ld	%f6,4095\(%r5\)
 2e4:	ed 60 5f ff 00 04 [	 ]*ldeb	%f6,4095\(%r5\)
 2ea:	b3 04 00 69 [	 ]*ldebr	%f6,%f9
 2ee:	28 69 [	 ]*ldr	%f6,%f9
 2f0:	b3 45 00 69 [	 ]*ldxbr	%f6,%f9
 2f4:	78 60 5f ff [	 ]*le	%f6,4095\(%r5\)
 2f8:	b3 44 00 69 [	 ]*ledbr	%f6,%f9
 2fc:	38 69 [	 ]*ler	%f6,%f9
 2fe:	b3 46 00 69 [	 ]*lexbr	%f6,%f9
 302:	b2 9d 5f ff [	 ]*lfpc	4095\(%r5\)
 306:	48 60 5f ff [	 ]*lh	%r6,4095\(%r5\)
 30a:	a7 68 80 01 [	 ]*lhi	%r6,-32767
 30e:	98 69 5f ff [	 ]*lm	%r6,%r9,4095\(%r5\)
 312:	b3 11 00 69 [	 ]*lndbr	%f6,%f9
 316:	21 69 [	 ]*lndr	%f6,%f9
 318:	b3 01 00 69 [	 ]*lnebr	%f6,%f9
 31c:	31 69 [	 ]*lner	%f6,%f9
 31e:	11 69 [	 ]*lnr	%r6,%r9
 320:	b3 41 00 69 [	 ]*lnxbr	%f6,%f9
 324:	b3 10 00 69 [	 ]*lpdbr	%f6,%f9
 328:	20 69 [	 ]*lpdr	%f6,%f9
 32a:	b3 00 00 69 [	 ]*lpebr	%f6,%f9
 32e:	30 69 [	 ]*lper	%f6,%f9
 330:	10 69 [	 ]*lpr	%r6,%r9
 332:	82 00 5f ff [	 ]*lpsw	4095\(%r5\)
 336:	b3 40 00 69 [	 ]*lpxbr	%f6,%f9
 33a:	18 69 [	 ]*lr	%r6,%r9
 33c:	b1 65 af ff [	 ]*lra	%r6,4095\(%r5,%r10\)
 340:	25 69 [	 ]*lrdr	%f6,%f9
 342:	35 69 [	 ]*lrer	%f6,%f9
 344:	b3 12 00 69 [	 ]*ltdbr	%f6,%f9
 348:	22 69 [	 ]*ltdr	%f6,%f9
 34a:	b3 02 00 69 [	 ]*ltebr	%f6,%f9
 34e:	32 69 [	 ]*lter	%f6,%f9
 350:	12 69 [	 ]*ltr	%r6,%r9
 352:	b3 42 00 69 [	 ]*ltxbr	%f6,%f9
 356:	b2 4b 00 69 [	 ]*lura	%r6,%r9
 35a:	ed 65 af ff 00 05 [	 ]*lxdb	%f6,4095\(%r5,%r10\)
 360:	b3 05 00 69 [	 ]*lxdbr	%f6,%f9
 364:	ed 65 af ff 00 06 [	 ]*lxeb	%f6,4095\(%r5,%r10\)
 36a:	b3 06 00 69 [	 ]*lxebr	%f6,%f9
 36e:	5c 65 af ff [	 ]*m	%r6,4095\(%r5,%r10\)
 372:	ed 95 af ff 60 1e [	 ]*madb	%f6,%f9,4095\(%r5,%r10\)
 378:	b3 1e 60 95 [	 ]*madbr	%f6,%f9,%f5
 37c:	ed 95 af ff 60 0e [	 ]*maeb	%f6,%f9,4095\(%r5,%r10\)
 382:	b3 0e 60 95 [	 ]*maebr	%f6,%f9,%f5
 386:	af 06 5f ff [	 ]*mc	4095\(%r5\),6
 38a:	6c 65 af ff [	 ]*md	%f6,4095\(%r5,%r10\)
 38e:	ed 65 af ff 00 1c [	 ]*mdb	%f6,4095\(%r5,%r10\)
 394:	b3 1c 00 69 [	 ]*mdbr	%f6,%f9
 398:	ed 65 af ff 00 0c [	 ]*mdeb	%f6,4095\(%r5,%r10\)
 39e:	b3 0c 00 69 [	 ]*mdebr	%f6,%f9
 3a2:	2c 69 [	 ]*mdr	%f6,%f9
 3a4:	7c 65 af ff [	 ]*me	%f6,4095\(%r5,%r10\)
 3a8:	ed 65 af ff 00 17 [	 ]*meeb	%f6,4095\(%r5,%r10\)
 3ae:	b3 17 00 69 [	 ]*meebr	%f6,%f9
 3b2:	3c 69 [	 ]*mer	%f6,%f9
 3b4:	4c 65 af ff [	 ]*mh	%r6,4095\(%r5,%r10\)
 3b8:	a7 6c 80 01 [	 ]*mhi	%r6,-32767
 3bc:	fc ff 5f ff af ff [	 ]*mp	4095\(16,%r5\),4095\(16,%r10\)
 3c2:	1c 69 [	 ]*mr	%r6,%r9
 3c4:	71 65 af ff [	 ]*ms	%r6,4095\(%r5,%r10\)
 3c8:	b2 32 5f ff [	 ]*msch	4095\(%r5\)
 3cc:	ed 95 af ff 60 1f [	 ]*msdb	%f6,%f9,4095\(%r5,%r10\)
 3d2:	b3 1f 60 95 [	 ]*msdbr	%f6,%f9,%f5
 3d6:	ed 95 af ff 60 0f [	 ]*mseb	%f6,%f9,4095\(%r5,%r10\)
 3dc:	b3 0f 60 95 [	 ]*msebr	%f6,%f9,%f5
 3e0:	b2 52 00 69 [	 ]*msr	%r6,%r9
 3e4:	b2 47 00 60 [	 ]*msta	%r6
 3e8:	d2 ff 5f ff af ff [	 ]*mvc	4095\(256,%r5\),4095\(%r10\)
 3ee:	e5 0f 5f ff af ff [	 ]*mvcdk	4095\(%r5\),4095\(%r10\)
 3f4:	e8 ff 5f ff af ff [	 ]*mvcin	4095\(256,%r5\),4095\(%r10\)
 3fa:	d9 69 5f ff af ff [	 ]*mvck	4095\(%r6,%r5\),4095\(%r10\),%r9
 400:	0e 69 [	 ]*mvcl	%r6,%r9
 402:	a8 69 00 0a [	 ]*mvcle	%r6,%r9,10
 406:	da 69 5f ff af ff [	 ]*mvcp	4095\(%r6,%r5\),4095\(%r10\),%r9
 40c:	db 69 5f ff af ff [	 ]*mvcs	4095\(%r6,%r5\),4095\(%r10\),%r9
 412:	e5 0e 5f ff af ff [	 ]*mvcsk	4095\(%r5\),4095\(%r10\)
 418:	92 ff 5f ff [	 ]*mvi	4095\(%r5\),255
 41c:	d1 ff 5f ff af ff [	 ]*mvn	4095\(256,%r5\),4095\(%r10\)
 422:	f1 ff 5f ff af ff [	 ]*mvo	4095\(16,%r5\),4095\(16,%r10\)
 428:	b2 54 00 69 [	 ]*mvpg	%r6,%r9
 42c:	b2 55 00 69 [	 ]*mvst	%r6,%r9
 430:	d3 ff 5f ff af ff [	 ]*mvz	4095\(256,%r5\),4095\(%r10\)
 436:	b3 4c 00 69 [	 ]*mxbr	%f6,%f9
 43a:	67 65 af ff [	 ]*mxd	%f6,4095\(%r5,%r10\)
 43e:	ed 65 af ff 00 07 [	 ]*mxdb	%f6,4095\(%r5,%r10\)
 444:	b3 07 00 69 [	 ]*mxdbr	%f6,%f9
 448:	27 69 [	 ]*mxdr	%f6,%f9
 44a:	26 69 [	 ]*mxr	%f6,%f9
 44c:	54 65 af ff [	 ]*n	%r6,4095\(%r5,%r10\)
 450:	d4 ff 5f ff af ff [	 ]*nc	4095\(256,%r5\),4095\(%r10\)
 456:	94 ff 5f ff [	 ]*ni	4095\(%r5\),255
 45a:	47 05 af ff [	 ]*bc	0,4095\(%r5,%r10\)
 45e:	07 06 [	 ]*bcr	0,%r6
 460:	14 69 [	 ]*nr	%r6,%r9
 462:	56 65 af ff [	 ]*o	%r6,4095\(%r5,%r10\)
 466:	d6 ff 5f ff af ff [	 ]*oc	4095\(256,%r5\),4095\(%r10\)
 46c:	96 ff 5f ff [	 ]*oi	4095\(%r5\),255
 470:	16 69 [	 ]*or	%r6,%r9
 472:	f2 ff 5f ff af ff [	 ]*pack	4095\(16,%r5\),4095\(16,%r10\)
 478:	b2 48 00 00 [	 ]*palb
 47c:	b2 18 5f ff [	 ]*pc	4095\(%r5\)
 480:	ee 69 5f ff af ff [	 ]*plo	%r6,4095\(%r5\),%r9,4095\(%r10\)
 486:	01 01 [	 ]*pr
 488:	b2 28 00 69 [	 ]*pt	%r6,%r9
 48c:	b2 0d 00 00 [	 ]*ptlb
 490:	b2 3b 00 00 [	 ]*rchp
 494:	b2 77 5f ff [	 ]*rp	4095\(%r5\)
 498:	b2 2a 00 69 [	 ]*rrbe	%r6,%r9
 49c:	b2 38 00 00 [	 ]*rsch
 4a0:	5b 65 af ff [	 ]*s	%r6,4095\(%r5,%r10\)
 4a4:	b2 19 5f ff [	 ]*sac	4095\(%r5\)
 4a8:	b2 79 5f ff [	 ]*sacf	4095\(%r5\)
 4ac:	b2 37 00 00 [	 ]*sal
 4b0:	b2 4e 00 69 [	 ]*sar	%a6,%r9
 4b4:	b2 3c 00 00 [	 ]*schm
 4b8:	b2 04 5f ff [	 ]*sck	4095\(%r5\)
 4bc:	b2 06 5f ff [	 ]*sckc	4095\(%r5\)
 4c0:	01 07 [	 ]*sckpf
 4c2:	6b 65 af ff [	 ]*sd	%f6,4095\(%r5,%r10\)
 4c6:	ed 65 af ff 00 1b [	 ]*sdb	%f6,4095\(%r5,%r10\)
 4cc:	b3 1b 00 69 [	 ]*sdbr	%f6,%f9
 4d0:	2b 69 [	 ]*sdr	%f6,%f9
 4d2:	7b 65 af ff [	 ]*se	%f6,4095\(%r5,%r10\)
 4d6:	ed 65 af ff 00 0b [	 ]*seb	%f6,4095\(%r5,%r10\)
 4dc:	b3 0b 00 69 [	 ]*sebr	%f6,%f9
 4e0:	3b 69 [	 ]*ser	%f6,%f9
 4e2:	b3 84 00 69 [	 ]*sfpc	%r6,%r9
 4e6:	4b 65 af ff [	 ]*sh	%r6,4095\(%r5,%r10\)
 4ea:	b2 14 5f ff [	 ]*sie	4095\(%r5\)
 4ee:	b2 74 5f ff [	 ]*siga	4095\(%r5\)
 4f2:	ae 69 5f ff [	 ]*sigp	%r6,%r9,4095\(%r5\)
 4f6:	5f 65 af ff [	 ]*sl	%r6,4095\(%r5,%r10\)
 4fa:	8b 60 5f ff [	 ]*sla	%r6,4095\(%r5\)
 4fe:	8f 60 5f ff [	 ]*slda	%r6,4095\(%r5\)
 502:	8d 60 5f ff [	 ]*sldl	%r6,4095\(%r5\)
 506:	89 60 5f ff [	 ]*sll	%r6,4095\(%r5\)
 50a:	1f 69 [	 ]*slr	%r6,%r9
 50c:	fb ff 5f ff af ff [	 ]*sp	4095\(16,%r5\),4095\(16,%r10\)
 512:	b2 0a 5f ff [	 ]*spka	4095\(%r5\)
 516:	04 60 [	 ]*spm	%r6
 518:	b2 08 5f ff [	 ]*spt	4095\(%r5\)
 51c:	b2 10 5f ff [	 ]*spx	4095\(%r5\)
 520:	ed 65 af ff 00 15 [	 ]*sqdb	%f6,4095\(%r5,%r10\)
 526:	b3 15 00 69 [	 ]*sqdbr	%f6,%f9
 52a:	b2 44 00 60 [	 ]*sqdr	%f6
 52e:	ed 65 af ff 00 14 [	 ]*sqeb	%f6,4095\(%r5,%r10\)
 534:	b3 14 00 69 [	 ]*sqebr	%f6,%f9
 538:	b2 45 00 60 [	 ]*sqer	%f6
 53c:	b3 16 00 69 [	 ]*sqxbr	%f6,%f9
 540:	1b 69 [	 ]*sr	%r6,%r9
 542:	8a 60 5f ff [	 ]*sra	%r6,4095\(%r5\)
 546:	8e 60 5f ff [	 ]*srda	%r6,4095\(%r5\)
 54a:	8c 60 5f ff [	 ]*srdl	%r6,4095\(%r5\)
 54e:	88 60 5f ff [	 ]*srl	%r6,4095\(%r5\)
 552:	b2 99 5f ff [	 ]*srnm	4095\(%r5\)
 556:	f0 fa 5f ff af ff [	 ]*srp	4095\(16,%r5\),4095\(%r10\),10
 55c:	b2 5e 00 69 [	 ]*srst	%r6,%r9
 560:	b2 25 00 60 [	 ]*ssar	%r6
 564:	b2 33 5f ff [	 ]*ssch	4095\(%r5\)
 568:	b2 2b 00 69 [	 ]*sske	%r6,%r9
 56c:	80 00 5f ff [	 ]*ssm	4095\(%r5\)
 570:	50 65 af ff [	 ]*st	%r6,4095\(%r5,%r10\)
 574:	9b 69 5f ff [	 ]*stam	%a6,%a9,4095\(%r5\)
 578:	b2 12 5f ff [	 ]*stap	4095\(%r5\)
 57c:	42 65 af ff [	 ]*stc	%r6,4095\(%r5,%r10\)
 580:	b2 05 5f ff [	 ]*stck	4095\(%r5\)
 584:	b2 07 5f ff [	 ]*stckc	4095\(%r5\)
 588:	be 6f 5f ff [	 ]*stcm	%r6,15,4095\(%r5\)
 58c:	b2 3a 5f ff [	 ]*stcps	4095\(%r5\)
 590:	b2 39 5f ff [	 ]*stcrw	4095\(%r5\)
 594:	b6 69 5f ff [	 ]*stctl	%c6,%c9,4095\(%r5\)
 598:	60 65 af ff [	 ]*std	%f6,4095\(%r5,%r10\)
 59c:	70 65 af ff [	 ]*ste	%f6,4095\(%r5,%r10\)
 5a0:	b2 9c 5f ff [	 ]*stfpc	4095\(%r5\)
 5a4:	40 65 af ff [	 ]*sth	%r6,4095\(%r5,%r10\)
 5a8:	b2 02 5f ff [	 ]*stidp	4095\(%r5\)
 5ac:	90 69 5f ff [	 ]*stm	%r6,%r9,4095\(%r5\)
 5b0:	ac ff 5f ff [	 ]*stnsm	4095\(%r5\),255
 5b4:	ad ff 5f ff [	 ]*stosm	4095\(%r5\),255
 5b8:	b2 09 5f ff [	 ]*stpt	4095\(%r5\)
 5bc:	b2 11 5f ff [	 ]*stpx	4095\(%r5\)
 5c0:	b2 34 5f ff [	 ]*stsch	4095\(%r5\)
 5c4:	b2 7d 5f ff [	 ]*stsi	4095\(%r5\)
 5c8:	b2 46 00 69 [	 ]*stura	%r6,%r9
 5cc:	7f 65 af ff [	 ]*su	%f6,4095\(%r5,%r10\)
 5d0:	3f 69 [	 ]*sur	%f6,%f9
 5d2:	0a ff [	 ]*svc	255
 5d4:	6f 65 af ff [	 ]*sw	%f6,4095\(%r5,%r10\)
 5d8:	2f 69 [	 ]*swr	%f6,%f9
 5da:	b3 4b 00 69 [	 ]*sxbr	%f6,%f9
 5de:	37 69 [	 ]*sxr	%f6,%f9
 5e0:	b2 4c 00 69 [	 ]*tar	%a6,%r9
 5e4:	b2 2c 00 06 [	 ]*tb	%r6
 5e8:	ed 65 af ff 00 11 [	 ]*tcdb	%f6,4095\(%r5,%r10\)
 5ee:	ed 65 af ff 00 10 [	 ]*tceb	%f6,4095\(%r5,%r10\)
 5f4:	ed 65 af ff 00 12 [	 ]*tcxb	%f6,4095\(%r5,%r10\)
 5fa:	91 ff 5f ff [	 ]*tm	4095\(%r5\),255
 5fe:	a7 60 ff ff [	 ]*tmh	%r6,65535
 602:	a7 61 ff ff [	 ]*tml	%r6,65535
 606:	b2 36 5f ff [	 ]*tpi	4095\(%r5\)
 60a:	e5 01 5f ff af ff [	 ]*tprot	4095\(%r5\),4095\(%r10\)
 610:	dc ff 5f ff af ff [	 ]*tr	4095\(256,%r5\),4095\(%r10\)
 616:	99 69 5f ff [	 ]*trace	%r6,%r9,4095\(%r5\)
 61a:	01 ff [	 ]*trap2
 61c:	b2 ff 5f ff [	 ]*trap4	4095\(%r5\)
 620:	dd ff 5f ff af ff [	 ]*trt	4095\(256,%r5\),4095\(%r10\)
 626:	93 00 5f ff [	 ]*ts	4095\(%r5\)
 62a:	b2 35 5f ff [	 ]*tsch	4095\(%r5\)
 62e:	f3 ff 5f ff af ff [	 ]*unpk	4095\(16,%r5\),4095\(16,%r10\)
 634:	01 02 [	 ]*upt
 636:	57 65 af ff [	 ]*x	%r6,4095\(%r5,%r10\)
 63a:	d7 ff 5f ff af ff [	 ]*xc	4095\(256,%r5\),4095\(%r10\)
 640:	97 ff 5f ff [	 ]*xi	4095\(%r5\),255
 644:	17 69 [	 ]*xr	%r6,%r9
 646:	f8 ff 5f ff af ff [	 ]*zap	4095\(16,%r5\),4095\(16,%r10\)
