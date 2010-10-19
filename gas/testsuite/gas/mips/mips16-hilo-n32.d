#objdump: -dr
#name: MIPS16 lui/addi n32
#as: -mips16 -mabi=n32 -march=mips64
#source: mips16-hilo.s

.*: +file format .*mips.*

Disassembly of section \.text:

0+0000 <stuff>:
   0:	6c00      	li	a0,0
   2:	f400 3480 	sll	a0,16
   6:	4c00      	addiu	a0,0
   8:	f000 6c00 	li	a0,0
			8: R_MIPS16_HI16	\.data
   c:	f400 3480 	sll	a0,16
  10:	f000 4c00 	addiu	a0,0
			10: R_MIPS16_LO16	\.data
  14:	f000 6c00 	li	a0,0
			14: R_MIPS16_HI16	\.data\+0x4
  18:	f400 3480 	sll	a0,16
  1c:	f000 4c00 	addiu	a0,0
			1c: R_MIPS16_LO16	\.data\+0x4
  20:	f000 6c00 	li	a0,0
			20: R_MIPS16_HI16	big_external_data_label
  24:	f400 3480 	sll	a0,16
  28:	f000 4c00 	addiu	a0,0
			28: R_MIPS16_LO16	big_external_data_label
  2c:	f000 6c00 	li	a0,0
			2c: R_MIPS16_HI16	small_external_data_label
  30:	f400 3480 	sll	a0,16
  34:	f000 4c00 	addiu	a0,0
			34: R_MIPS16_LO16	small_external_data_label
  38:	f000 6c00 	li	a0,0
			38: R_MIPS16_HI16	big_external_common
  3c:	f400 3480 	sll	a0,16
  40:	f000 4c00 	addiu	a0,0
			40: R_MIPS16_LO16	big_external_common
  44:	f000 6c00 	li	a0,0
			44: R_MIPS16_HI16	small_external_common
  48:	f400 3480 	sll	a0,16
  4c:	f000 4c00 	addiu	a0,0
			4c: R_MIPS16_LO16	small_external_common
  50:	f000 6c00 	li	a0,0
			50: R_MIPS16_HI16	\.bss
  54:	f400 3480 	sll	a0,16
  58:	f000 4c00 	addiu	a0,0
			58: R_MIPS16_LO16	\.bss
  5c:	f000 6c00 	li	a0,0
			5c: R_MIPS16_HI16	\.sbss
  60:	f400 3480 	sll	a0,16
  64:	f000 4c00 	addiu	a0,0
			64: R_MIPS16_LO16	\.sbss
  68:	6c00      	li	a0,0
  6a:	f400 3480 	sll	a0,16
  6e:	4c01      	addiu	a0,1
  70:	f000 6c00 	li	a0,0
			70: R_MIPS16_HI16	\.data\+0x1
  74:	f400 3480 	sll	a0,16
  78:	f000 4c00 	addiu	a0,0
			78: R_MIPS16_LO16	\.data\+0x1
  7c:	f000 6c00 	li	a0,0
			7c: R_MIPS16_HI16	\.data\+0x5
  80:	f400 3480 	sll	a0,16
  84:	f000 4c00 	addiu	a0,0
			84: R_MIPS16_LO16	\.data\+0x5
  88:	f000 6c00 	li	a0,0
			88: R_MIPS16_HI16	big_external_data_label\+0x1
  8c:	f400 3480 	sll	a0,16
  90:	f000 4c00 	addiu	a0,0
			90: R_MIPS16_LO16	big_external_data_label\+0x1
  94:	f000 6c00 	li	a0,0
			94: R_MIPS16_HI16	small_external_data_label\+0x1
  98:	f400 3480 	sll	a0,16
  9c:	f000 4c00 	addiu	a0,0
			9c: R_MIPS16_LO16	small_external_data_label\+0x1
  a0:	f000 6c00 	li	a0,0
			a0: R_MIPS16_HI16	big_external_common\+0x1
  a4:	f400 3480 	sll	a0,16
  a8:	f000 4c00 	addiu	a0,0
			a8: R_MIPS16_LO16	big_external_common\+0x1
  ac:	f000 6c00 	li	a0,0
			ac: R_MIPS16_HI16	small_external_common\+0x1
  b0:	f400 3480 	sll	a0,16
  b4:	f000 4c00 	addiu	a0,0
			b4: R_MIPS16_LO16	small_external_common\+0x1
  b8:	f000 6c00 	li	a0,0
			b8: R_MIPS16_HI16	\.bss\+0x1
  bc:	f400 3480 	sll	a0,16
  c0:	f000 4c00 	addiu	a0,0
			c0: R_MIPS16_LO16	\.bss\+0x1
  c4:	f000 6c00 	li	a0,0
			c4: R_MIPS16_HI16	\.sbss\+0x1
  c8:	f400 3480 	sll	a0,16
  cc:	f000 4c00 	addiu	a0,0
			cc: R_MIPS16_LO16	\.sbss\+0x1
  d0:	6c01      	li	a0,1
  d2:	f400 3480 	sll	a0,16
  d6:	f010 4c00 	addiu	a0,-32768
  da:	f000 6c00 	li	a0,0
			da: R_MIPS16_HI16	\.data\+0x8000
  de:	f400 3480 	sll	a0,16
  e2:	f000 4c00 	addiu	a0,0
			e2: R_MIPS16_LO16	\.data\+0x8000
  e6:	f000 6c00 	li	a0,0
			e6: R_MIPS16_HI16	\.data\+0x8004
  ea:	f400 3480 	sll	a0,16
  ee:	f000 4c00 	addiu	a0,0
			ee: R_MIPS16_LO16	\.data\+0x8004
  f2:	f000 6c00 	li	a0,0
			f2: R_MIPS16_HI16	big_external_data_label\+0x8000
  f6:	f400 3480 	sll	a0,16
  fa:	f000 4c00 	addiu	a0,0
			fa: R_MIPS16_LO16	big_external_data_label\+0x8000
  fe:	f000 6c00 	li	a0,0
			fe: R_MIPS16_HI16	small_external_data_label\+0x8000
 102:	f400 3480 	sll	a0,16
 106:	f000 4c00 	addiu	a0,0
			106: R_MIPS16_LO16	small_external_data_label\+0x8000
 10a:	f000 6c00 	li	a0,0
			10a: R_MIPS16_HI16	big_external_common\+0x8000
 10e:	f400 3480 	sll	a0,16
 112:	f000 4c00 	addiu	a0,0
			112: R_MIPS16_LO16	big_external_common\+0x8000
 116:	f000 6c00 	li	a0,0
			116: R_MIPS16_HI16	small_external_common\+0x8000
 11a:	f400 3480 	sll	a0,16
 11e:	f000 4c00 	addiu	a0,0
			11e: R_MIPS16_LO16	small_external_common\+0x8000
 122:	f000 6c00 	li	a0,0
			122: R_MIPS16_HI16	\.bss\+0x8000
 126:	f400 3480 	sll	a0,16
 12a:	f000 4c00 	addiu	a0,0
			12a: R_MIPS16_LO16	\.bss\+0x8000
 12e:	f000 6c00 	li	a0,0
			12e: R_MIPS16_HI16	\.sbss\+0x8000
 132:	f400 3480 	sll	a0,16
 136:	f000 4c00 	addiu	a0,0
			136: R_MIPS16_LO16	\.sbss\+0x8000
 13a:	6c00      	li	a0,0
 13c:	f400 3480 	sll	a0,16
 140:	f010 4c00 	addiu	a0,-32768
 144:	f000 6c00 	li	a0,0
			144: R_MIPS16_HI16	\.data\+0xffff8000
 148:	f400 3480 	sll	a0,16
 14c:	f000 4c00 	addiu	a0,0
			14c: R_MIPS16_LO16	\.data\+0xffff8000
 150:	f000 6c00 	li	a0,0
			150: R_MIPS16_HI16	\.data\+0xffff8004
 154:	f400 3480 	sll	a0,16
 158:	f000 4c00 	addiu	a0,0
			158: R_MIPS16_LO16	\.data\+0xffff8004
 15c:	f000 6c00 	li	a0,0
			15c: R_MIPS16_HI16	big_external_data_label\+0xffff8000
 160:	f400 3480 	sll	a0,16
 164:	f000 4c00 	addiu	a0,0
			164: R_MIPS16_LO16	big_external_data_label\+0xffff8000
 168:	f000 6c00 	li	a0,0
			168: R_MIPS16_HI16	small_external_data_label\+0xffff8000
 16c:	f400 3480 	sll	a0,16
 170:	f000 4c00 	addiu	a0,0
			170: R_MIPS16_LO16	small_external_data_label\+0xffff8000
 174:	f000 6c00 	li	a0,0
			174: R_MIPS16_HI16	big_external_common\+0xffff8000
 178:	f400 3480 	sll	a0,16
 17c:	f000 4c00 	addiu	a0,0
			17c: R_MIPS16_LO16	big_external_common\+0xffff8000
 180:	f000 6c00 	li	a0,0
			180: R_MIPS16_HI16	small_external_common\+0xffff8000
 184:	f400 3480 	sll	a0,16
 188:	f000 4c00 	addiu	a0,0
			188: R_MIPS16_LO16	small_external_common\+0xffff8000
 18c:	f000 6c00 	li	a0,0
			18c: R_MIPS16_HI16	\.bss\+0xffff8000
 190:	f400 3480 	sll	a0,16
 194:	f000 4c00 	addiu	a0,0
			194: R_MIPS16_LO16	\.bss\+0xffff8000
 198:	f000 6c00 	li	a0,0
			198: R_MIPS16_HI16	\.sbss\+0xffff8000
 19c:	f400 3480 	sll	a0,16
 1a0:	f000 4c00 	addiu	a0,0
			1a0: R_MIPS16_LO16	\.sbss\+0xffff8000
 1a4:	6c01      	li	a0,1
 1a6:	f400 3480 	sll	a0,16
 1aa:	4c00      	addiu	a0,0
 1ac:	f000 6c00 	li	a0,0
			1ac: R_MIPS16_HI16	\.data\+0x10000
 1b0:	f400 3480 	sll	a0,16
 1b4:	f000 4c00 	addiu	a0,0
			1b4: R_MIPS16_LO16	\.data\+0x10000
 1b8:	f000 6c00 	li	a0,0
			1b8: R_MIPS16_HI16	\.data\+0x10004
 1bc:	f400 3480 	sll	a0,16
 1c0:	f000 4c00 	addiu	a0,0
			1c0: R_MIPS16_LO16	\.data\+0x10004
 1c4:	f000 6c00 	li	a0,0
			1c4: R_MIPS16_HI16	big_external_data_label\+0x10000
 1c8:	f400 3480 	sll	a0,16
 1cc:	f000 4c00 	addiu	a0,0
			1cc: R_MIPS16_LO16	big_external_data_label\+0x10000
 1d0:	f000 6c00 	li	a0,0
			1d0: R_MIPS16_HI16	small_external_data_label\+0x10000
 1d4:	f400 3480 	sll	a0,16
 1d8:	f000 4c00 	addiu	a0,0
			1d8: R_MIPS16_LO16	small_external_data_label\+0x10000
 1dc:	f000 6c00 	li	a0,0
			1dc: R_MIPS16_HI16	big_external_common\+0x10000
 1e0:	f400 3480 	sll	a0,16
 1e4:	f000 4c00 	addiu	a0,0
			1e4: R_MIPS16_LO16	big_external_common\+0x10000
 1e8:	f000 6c00 	li	a0,0
			1e8: R_MIPS16_HI16	small_external_common\+0x10000
 1ec:	f400 3480 	sll	a0,16
 1f0:	f000 4c00 	addiu	a0,0
			1f0: R_MIPS16_LO16	small_external_common\+0x10000
 1f4:	f000 6c00 	li	a0,0
			1f4: R_MIPS16_HI16	\.bss\+0x10000
 1f8:	f400 3480 	sll	a0,16
 1fc:	f000 4c00 	addiu	a0,0
			1fc: R_MIPS16_LO16	\.bss\+0x10000
 200:	f000 6c00 	li	a0,0
			200: R_MIPS16_HI16	\.sbss\+0x10000
 204:	f400 3480 	sll	a0,16
 208:	f000 4c00 	addiu	a0,0
			208: R_MIPS16_LO16	\.sbss\+0x10000
 20c:	6c02      	li	a0,2
 20e:	f400 3480 	sll	a0,16
 212:	f5b4 4c05 	addiu	a0,-23131
 216:	f000 6c00 	li	a0,0
			216: R_MIPS16_HI16	\.data\+0x1a5a5
 21a:	f400 3480 	sll	a0,16
 21e:	f000 4c00 	addiu	a0,0
			21e: R_MIPS16_LO16	\.data\+0x1a5a5
 222:	f000 6c00 	li	a0,0
			222: R_MIPS16_HI16	\.data\+0x1a5a9
 226:	f400 3480 	sll	a0,16
 22a:	f000 4c00 	addiu	a0,0
			22a: R_MIPS16_LO16	\.data\+0x1a5a9
 22e:	f000 6c00 	li	a0,0
			22e: R_MIPS16_HI16	big_external_data_label\+0x1a5a5
 232:	f400 3480 	sll	a0,16
 236:	f000 4c00 	addiu	a0,0
			236: R_MIPS16_LO16	big_external_data_label\+0x1a5a5
 23a:	f000 6c00 	li	a0,0
			23a: R_MIPS16_HI16	small_external_data_label\+0x1a5a5
 23e:	f400 3480 	sll	a0,16
 242:	f000 4c00 	addiu	a0,0
			242: R_MIPS16_LO16	small_external_data_label\+0x1a5a5
 246:	f000 6c00 	li	a0,0
			246: R_MIPS16_HI16	big_external_common\+0x1a5a5
 24a:	f400 3480 	sll	a0,16
 24e:	f000 4c00 	addiu	a0,0
			24e: R_MIPS16_LO16	big_external_common\+0x1a5a5
 252:	f000 6c00 	li	a0,0
			252: R_MIPS16_HI16	small_external_common\+0x1a5a5
 256:	f400 3480 	sll	a0,16
 25a:	f000 4c00 	addiu	a0,0
			25a: R_MIPS16_LO16	small_external_common\+0x1a5a5
 25e:	f000 6c00 	li	a0,0
			25e: R_MIPS16_HI16	\.bss\+0x1a5a5
 262:	f400 3480 	sll	a0,16
 266:	f000 4c00 	addiu	a0,0
			266: R_MIPS16_LO16	\.bss\+0x1a5a5
 26a:	f000 6c00 	li	a0,0
			26a: R_MIPS16_HI16	\.sbss\+0x1a5a5
 26e:	f400 3480 	sll	a0,16
 272:	f000 4c00 	addiu	a0,0
			272: R_MIPS16_LO16	\.sbss\+0x1a5a5
 276:	6d00      	li	a1,0
 278:	f400 35a0 	sll	a1,16
 27c:	9d80      	lw	a0,0\(a1\)
 27e:	f000 6d00 	li	a1,0
			27e: R_MIPS16_HI16	\.data
 282:	f400 35a0 	sll	a1,16
 286:	f000 9d80 	lw	a0,0\(a1\)
			286: R_MIPS16_HI16	\.data
 28a:	f000 6d00 	li	a1,0
			28a: R_MIPS16_HI16	\.data\+0x4
 28e:	f400 35a0 	sll	a1,16
 292:	f000 9d80 	lw	a0,0\(a1\)
			292: R_MIPS16_HI16	\.data\+0x4
 296:	f000 6d00 	li	a1,0
			296: R_MIPS16_HI16	big_external_data_label
 29a:	f400 35a0 	sll	a1,16
 29e:	f000 9d80 	lw	a0,0\(a1\)
			29e: R_MIPS16_LO16	big_external_data_label
 2a2:	f000 6d00 	li	a1,0
			2a2: R_MIPS16_HI16	small_external_data_label
 2a6:	f400 35a0 	sll	a1,16
 2aa:	f000 9d80 	lw	a0,0\(a1\)
			2aa: R_MIPS16_LO16	small_external_data_label
 2ae:	f000 6d00 	li	a1,0
			2ae: R_MIPS16_HI16	big_external_common
 2b2:	f400 35a0 	sll	a1,16
 2b6:	f000 9d80 	lw	a0,0\(a1\)
			2b6: R_MIPS16_LO16	big_external_common
 2ba:	f000 6d00 	li	a1,0
			2ba: R_MIPS16_HI16	small_external_common
 2be:	f400 35a0 	sll	a1,16
 2c2:	f000 9d80 	lw	a0,0\(a1\)
			2c2: R_MIPS16_LO16	small_external_common
 2c6:	f000 6d00 	li	a1,0
			2c6: R_MIPS16_HI16	\.bss
 2ca:	f400 35a0 	sll	a1,16
 2ce:	f000 9d80 	lw	a0,0\(a1\)
			2ce: R_MIPS16_LO16	\.bss
 2d2:	f000 6d00 	li	a1,0
			2d2: R_MIPS16_HI16	\.sbss
 2d6:	f400 35a0 	sll	a1,16
 2da:	f000 9d80 	lw	a0,0\(a1\)
			2da: R_MIPS16_LO16	\.sbss
 2de:	6d00      	li	a1,0
 2e0:	f400 35a0 	sll	a1,16
 2e4:	f000 9d81 	lw	a0,1\(a1\)
 2e8:	f000 6d00 	li	a1,0
			2e8: R_MIPS16_HI16	\.data\+0x1
 2ec:	f400 35a0 	sll	a1,16
 2f0:	f000 9d80 	lw	a0,0\(a1\)
			2f0: R_MIPS16_LO16	\.data\+0x1
 2f4:	f000 6d00 	li	a1,0
			2f4: R_MIPS16_HI16	\.data\+0x5
 2f8:	f400 35a0 	sll	a1,16
 2fc:	f000 9d80 	lw	a0,0\(a1\)
			2fc: R_MIPS16_LO16	\.data\+0x5
 300:	f000 6d00 	li	a1,0
			300: R_MIPS16_HI16	big_external_data_label\+0x1
 304:	f400 35a0 	sll	a1,16
 308:	f000 9d80 	lw	a0,0\(a1\)
			308: R_MIPS16_LO16	big_external_data_label\+0x1
 30c:	f000 6d00 	li	a1,0
			30c: R_MIPS16_HI16	small_external_data_label\+0x1
 310:	f400 35a0 	sll	a1,16
 314:	f000 9d80 	lw	a0,0\(a1\)
			314: R_MIPS16_LO16	small_external_data_label\+0x1
 318:	f000 6d00 	li	a1,0
			318: R_MIPS16_HI16	big_external_common\+0x1
 31c:	f400 35a0 	sll	a1,16
 320:	f000 9d80 	lw	a0,0\(a1\)
			320: R_MIPS16_LO16	big_external_common\+0x1
 324:	f000 6d00 	li	a1,0
			324: R_MIPS16_HI16	small_external_common\+0x1
 328:	f400 35a0 	sll	a1,16
 32c:	f000 9d80 	lw	a0,0\(a1\)
			32c: R_MIPS16_LO16	small_external_common\+0x1
 330:	f000 6d00 	li	a1,0
			330: R_MIPS16_HI16	\.bss\+0x1
 334:	f400 35a0 	sll	a1,16
 338:	f000 9d80 	lw	a0,0\(a1\)
			338: R_MIPS16_LO16	\.bss\+0x1
 33c:	f000 6d00 	li	a1,0
			33c: R_MIPS16_HI16	\.sbss\+0x1
 340:	f400 35a0 	sll	a1,16
 344:	f000 9d80 	lw	a0,0\(a1\)
			344: R_MIPS16_LO16	\.sbss\+0x1
 348:	6d01      	li	a1,1
 34a:	f400 35a0 	sll	a1,16
 34e:	f010 9d80 	lw	a0,-32768\(a1\)
 352:	f000 6d00 	li	a1,0
			352: R_MIPS16_HI16	\.data\+0x8000
 356:	f400 35a0 	sll	a1,16
 35a:	f000 9d80 	lw	a0,0\(a1\)
			35a: R_MIPS16_LO16	\.data\+0x8000
 35e:	f000 6d00 	li	a1,0
			35e: R_MIPS16_HI16	\.data\+0x8004
 362:	f400 35a0 	sll	a1,16
 366:	f000 9d80 	lw	a0,0\(a1\)
			366: R_MIPS16_LO16	\.data\+0x8004
 36a:	f000 6d00 	li	a1,0
			36a: R_MIPS16_HI16	big_external_data_label\+0x8000
 36e:	f400 35a0 	sll	a1,16
 372:	f000 9d80 	lw	a0,0\(a1\)
			372: R_MIPS16_LO16	big_external_data_label\+0x8000
 376:	f000 6d00 	li	a1,0
			376: R_MIPS16_HI16	small_external_data_label\+0x8000
 37a:	f400 35a0 	sll	a1,16
 37e:	f000 9d80 	lw	a0,0\(a1\)
			37e: R_MIPS16_LO16	small_external_data_label\+0x8000
 382:	f000 6d00 	li	a1,0
			382: R_MIPS16_HI16	big_external_common\+0x8000
 386:	f400 35a0 	sll	a1,16
 38a:	f000 9d80 	lw	a0,0\(a1\)
			38a: R_MIPS16_LO16	big_external_common\+0x8000
 38e:	f000 6d00 	li	a1,0
			38e: R_MIPS16_HI16	small_external_common\+0x8000
 392:	f400 35a0 	sll	a1,16
 396:	f000 9d80 	lw	a0,0\(a1\)
			396: R_MIPS16_LO16	small_external_common\+0x8000
 39a:	f000 6d00 	li	a1,0
			39a: R_MIPS16_HI16	\.bss\+0x8000
 39e:	f400 35a0 	sll	a1,16
 3a2:	f000 9d80 	lw	a0,0\(a1\)
			3a2: R_MIPS16_LO16	\.bss\+0x8000
 3a6:	f000 6d00 	li	a1,0
			3a6: R_MIPS16_HI16	\.sbss\+0x8000
 3aa:	f400 35a0 	sll	a1,16
 3ae:	f000 9d80 	lw	a0,0\(a1\)
			3ae: R_MIPS16_LO16	\.sbss\+0x8000
 3b2:	6d00      	li	a1,0
 3b4:	f400 35a0 	sll	a1,16
 3b8:	f010 9d80 	lw	a0,-32768\(a1\)
 3bc:	f000 6d00 	li	a1,0
			3bc: R_MIPS16_HI16	\.data\+0xffff8000
 3c0:	f400 35a0 	sll	a1,16
 3c4:	f000 9d80 	lw	a0,0\(a1\)
			3c4: R_MIPS16_LO16	\.data\+0xffff8000
 3c8:	f000 6d00 	li	a1,0
			3c8: R_MIPS16_HI16	\.data\+0xffff8004
 3cc:	f400 35a0 	sll	a1,16
 3d0:	f000 9d80 	lw	a0,0\(a1\)
			3d0: R_MIPS16_LO16	\.data\+0xffff8004
 3d4:	f000 6d00 	li	a1,0
			3d4: R_MIPS16_HI16	big_external_data_label\+0xffff8000
 3d8:	f400 35a0 	sll	a1,16
 3dc:	f000 9d80 	lw	a0,0\(a1\)
			3dc: R_MIPS16_LO16	big_external_data_label\+0xffff8000
 3e0:	f000 6d00 	li	a1,0
			3e0: R_MIPS16_HI16	small_external_data_label\+0xffff8000
 3e4:	f400 35a0 	sll	a1,16
 3e8:	f000 9d80 	lw	a0,0\(a1\)
			3e8: R_MIPS16_LO16	small_external_data_label\+0xffff8000
 3ec:	f000 6d00 	li	a1,0
			3ec: R_MIPS16_HI16	big_external_common\+0xffff8000
 3f0:	f400 35a0 	sll	a1,16
 3f4:	f000 9d80 	lw	a0,0\(a1\)
			3f4: R_MIPS16_LO16	big_external_common\+0xffff8000
 3f8:	f000 6d00 	li	a1,0
			3f8: R_MIPS16_HI16	small_external_common\+0xffff8000
 3fc:	f400 35a0 	sll	a1,16
 400:	f000 9d80 	lw	a0,0\(a1\)
			400: R_MIPS16_LO16	small_external_common\+0xffff8000
 404:	f000 6d00 	li	a1,0
			404: R_MIPS16_HI16	\.bss\+0xffff8000
 408:	f400 35a0 	sll	a1,16
 40c:	f000 9d80 	lw	a0,0\(a1\)
			40c: R_MIPS16_LO16	\.bss\+0xffff8000
 410:	f000 6d00 	li	a1,0
			410: R_MIPS16_HI16	\.sbss\+0xffff8000
 414:	f400 35a0 	sll	a1,16
 418:	f000 9d80 	lw	a0,0\(a1\)
			418: R_MIPS16_LO16	\.sbss\+0xffff8000
 41c:	6d01      	li	a1,1
 41e:	f400 35a0 	sll	a1,16
 422:	9d80      	lw	a0,0\(a1\)
 424:	f000 6d00 	li	a1,0
			424: R_MIPS16_HI16	\.data\+0x10000
 428:	f400 35a0 	sll	a1,16
 42c:	f000 9d80 	lw	a0,0\(a1\)
			42c: R_MIPS16_LO16	\.data\+0x10000
 430:	f000 6d00 	li	a1,0
			430: R_MIPS16_HI16	\.data\+0x10004
 434:	f400 35a0 	sll	a1,16
 438:	f000 9d80 	lw	a0,0\(a1\)
			438: R_MIPS16_LO16	\.data\+0x10004
 43c:	f000 6d00 	li	a1,0
			43c: R_MIPS16_HI16	big_external_data_label\+0x10000
 440:	f400 35a0 	sll	a1,16
 444:	f000 9d80 	lw	a0,0\(a1\)
			444: R_MIPS16_LO16	big_external_data_label\+0x10000
 448:	f000 6d00 	li	a1,0
			448: R_MIPS16_HI16	small_external_data_label\+0x10000
 44c:	f400 35a0 	sll	a1,16
 450:	f000 9d80 	lw	a0,0\(a1\)
			450: R_MIPS16_LO16	small_external_data_label\+0x10000
 454:	f000 6d00 	li	a1,0
			454: R_MIPS16_HI16	big_external_common\+0x10000
 458:	f400 35a0 	sll	a1,16
 45c:	f000 9d80 	lw	a0,0\(a1\)
			45c: R_MIPS16_LO16	big_external_common\+0x10000
 460:	f000 6d00 	li	a1,0
			460: R_MIPS16_HI16	small_external_common\+0x10000
 464:	f400 35a0 	sll	a1,16
 468:	f000 9d80 	lw	a0,0\(a1\)
			468: R_MIPS16_LO16	small_external_common\+0x10000
 46c:	f000 6d00 	li	a1,0
			46c: R_MIPS16_HI16	\.bss\+0x10000
 470:	f400 35a0 	sll	a1,16
 474:	f000 9d80 	lw	a0,0\(a1\)
			474: R_MIPS16_LO16	\.bss\+0x10000
 478:	f000 6d00 	li	a1,0
			478: R_MIPS16_HI16	\.sbss\+0x10000
 47c:	f400 35a0 	sll	a1,16
 480:	f000 9d80 	lw	a0,0\(a1\)
			480: R_MIPS16_LO16	\.sbss\+0x10000
 484:	6d02      	li	a1,2
 486:	f400 35a0 	sll	a1,16
 48a:	f5b4 9d85 	lw	a0,-23131\(a1\)
 48e:	f000 6d00 	li	a1,0
			48e: R_MIPS16_HI16	\.data\+0x1a5a5
 492:	f400 35a0 	sll	a1,16
 496:	f000 9d80 	lw	a0,0\(a1\)
			496: R_MIPS16_LO16	\.data\+0x1a5a5
 49a:	f000 6d00 	li	a1,0
			49a: R_MIPS16_HI16	\.data\+0x1a5a9
 49e:	f400 35a0 	sll	a1,16
 4a2:	f000 9d80 	lw	a0,0\(a1\)
			4a2: R_MIPS16_LO16	\.data\+0x1a5a9
 4a6:	f000 6d00 	li	a1,0
			4a6: R_MIPS16_HI16	big_external_data_label\+0x1a5a5
 4aa:	f400 35a0 	sll	a1,16
 4ae:	f000 9d80 	lw	a0,0\(a1\)
			4ae: R_MIPS16_LO16	big_external_data_label\+0x1a5a5
 4b2:	f000 6d00 	li	a1,0
			4b2: R_MIPS16_HI16	small_external_data_label\+0x1a5a5
 4b6:	f400 35a0 	sll	a1,16
 4ba:	f000 9d80 	lw	a0,0\(a1\)
			4ba: R_MIPS16_LO16	small_external_data_label\+0x1a5a5
 4be:	f000 6d00 	li	a1,0
			4be: R_MIPS16_HI16	big_external_common\+0x1a5a5
 4c2:	f400 35a0 	sll	a1,16
 4c6:	f000 9d80 	lw	a0,0\(a1\)
			4c6: R_MIPS16_LO16	big_external_common\+0x1a5a5
 4ca:	f000 6d00 	li	a1,0
			4ca: R_MIPS16_HI16	small_external_common\+0x1a5a5
 4ce:	f400 35a0 	sll	a1,16
 4d2:	f000 9d80 	lw	a0,0\(a1\)
			4d2: R_MIPS16_LO16	small_external_common\+0x1a5a5
 4d6:	f000 6d00 	li	a1,0
			4d6: R_MIPS16_HI16	\.bss\+0x1a5a5
 4da:	f400 35a0 	sll	a1,16
 4de:	f000 9d80 	lw	a0,0\(a1\)
			4de: R_MIPS16_LO16	\.bss\+0x1a5a5
 4e2:	f000 6d00 	li	a1,0
			4e2: R_MIPS16_HI16	\.sbss\+0x1a5a5
 4e6:	f400 35a0 	sll	a1,16
 4ea:	f000 9d80 	lw	a0,0\(a1\)
			4ea: R_MIPS16_LO16	\.sbss\+0x1a5a5
 4ee:	6500      	nop
