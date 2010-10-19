#objdump: -dr
#name: MIPS16 lui/addi
#as: -mips16 -mabi=32
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
			14: R_MIPS16_HI16	\.data
  18:	f400 3480 	sll	a0,16
  1c:	f000 4c04 	addiu	a0,4
			1c: R_MIPS16_LO16	\.data
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
			70: R_MIPS16_HI16	\.data
  74:	f400 3480 	sll	a0,16
  78:	f000 4c01 	addiu	a0,1
			78: R_MIPS16_LO16	\.data
  7c:	f000 6c00 	li	a0,0
			7c: R_MIPS16_HI16	\.data
  80:	f400 3480 	sll	a0,16
  84:	f000 4c05 	addiu	a0,5
			84: R_MIPS16_LO16	\.data
  88:	f000 6c00 	li	a0,0
			88: R_MIPS16_HI16	big_external_data_label
  8c:	f400 3480 	sll	a0,16
  90:	f000 4c01 	addiu	a0,1
			90: R_MIPS16_LO16	big_external_data_label
  94:	f000 6c00 	li	a0,0
			94: R_MIPS16_HI16	small_external_data_label
  98:	f400 3480 	sll	a0,16
  9c:	f000 4c01 	addiu	a0,1
			9c: R_MIPS16_LO16	small_external_data_label
  a0:	f000 6c00 	li	a0,0
			a0: R_MIPS16_HI16	big_external_common
  a4:	f400 3480 	sll	a0,16
  a8:	f000 4c01 	addiu	a0,1
			a8: R_MIPS16_LO16	big_external_common
  ac:	f000 6c00 	li	a0,0
			ac: R_MIPS16_HI16	small_external_common
  b0:	f400 3480 	sll	a0,16
  b4:	f000 4c01 	addiu	a0,1
			b4: R_MIPS16_LO16	small_external_common
  b8:	f000 6c00 	li	a0,0
			b8: R_MIPS16_HI16	\.bss
  bc:	f400 3480 	sll	a0,16
  c0:	f000 4c01 	addiu	a0,1
			c0: R_MIPS16_LO16	\.bss
  c4:	f000 6c00 	li	a0,0
			c4: R_MIPS16_HI16	\.sbss
  c8:	f400 3480 	sll	a0,16
  cc:	f000 4c01 	addiu	a0,1
			cc: R_MIPS16_LO16	\.sbss
  d0:	6c01      	li	a0,1
  d2:	f400 3480 	sll	a0,16
  d6:	f010 4c00 	addiu	a0,-32768
  da:	f000 6c01 	li	a0,1
			da: R_MIPS16_HI16	\.data
  de:	f400 3480 	sll	a0,16
  e2:	f010 4c00 	addiu	a0,-32768
			e2: R_MIPS16_LO16	\.data
  e6:	f000 6c01 	li	a0,1
			e6: R_MIPS16_HI16	\.data
  ea:	f400 3480 	sll	a0,16
  ee:	f010 4c04 	addiu	a0,-32764
			ee: R_MIPS16_LO16	\.data
  f2:	f000 6c01 	li	a0,1
			f2: R_MIPS16_HI16	big_external_data_label
  f6:	f400 3480 	sll	a0,16
  fa:	f010 4c00 	addiu	a0,-32768
			fa: R_MIPS16_LO16	big_external_data_label
  fe:	f000 6c01 	li	a0,1
			fe: R_MIPS16_HI16	small_external_data_label
 102:	f400 3480 	sll	a0,16
 106:	f010 4c00 	addiu	a0,-32768
			106: R_MIPS16_LO16	small_external_data_label
 10a:	f000 6c01 	li	a0,1
			10a: R_MIPS16_HI16	big_external_common
 10e:	f400 3480 	sll	a0,16
 112:	f010 4c00 	addiu	a0,-32768
			112: R_MIPS16_LO16	big_external_common
 116:	f000 6c01 	li	a0,1
			116: R_MIPS16_HI16	small_external_common
 11a:	f400 3480 	sll	a0,16
 11e:	f010 4c00 	addiu	a0,-32768
			11e: R_MIPS16_LO16	small_external_common
 122:	f000 6c01 	li	a0,1
			122: R_MIPS16_HI16	\.bss
 126:	f400 3480 	sll	a0,16
 12a:	f010 4c00 	addiu	a0,-32768
			12a: R_MIPS16_LO16	\.bss
 12e:	f000 6c01 	li	a0,1
			12e: R_MIPS16_HI16	\.sbss
 132:	f400 3480 	sll	a0,16
 136:	f010 4c00 	addiu	a0,-32768
			136: R_MIPS16_LO16	\.sbss
 13a:	6c00      	li	a0,0
 13c:	f400 3480 	sll	a0,16
 140:	f010 4c00 	addiu	a0,-32768
 144:	f000 6c00 	li	a0,0
			144: R_MIPS16_HI16	\.data
 148:	f400 3480 	sll	a0,16
 14c:	f010 4c00 	addiu	a0,-32768
			14c: R_MIPS16_LO16	\.data
 150:	f000 6c00 	li	a0,0
			150: R_MIPS16_HI16	\.data
 154:	f400 3480 	sll	a0,16
 158:	f010 4c04 	addiu	a0,-32764
			158: R_MIPS16_LO16	\.data
 15c:	f000 6c00 	li	a0,0
			15c: R_MIPS16_HI16	big_external_data_label
 160:	f400 3480 	sll	a0,16
 164:	f010 4c00 	addiu	a0,-32768
			164: R_MIPS16_LO16	big_external_data_label
 168:	f000 6c00 	li	a0,0
			168: R_MIPS16_HI16	small_external_data_label
 16c:	f400 3480 	sll	a0,16
 170:	f010 4c00 	addiu	a0,-32768
			170: R_MIPS16_LO16	small_external_data_label
 174:	f000 6c00 	li	a0,0
			174: R_MIPS16_HI16	big_external_common
 178:	f400 3480 	sll	a0,16
 17c:	f010 4c00 	addiu	a0,-32768
			17c: R_MIPS16_LO16	big_external_common
 180:	f000 6c00 	li	a0,0
			180: R_MIPS16_HI16	small_external_common
 184:	f400 3480 	sll	a0,16
 188:	f010 4c00 	addiu	a0,-32768
			188: R_MIPS16_LO16	small_external_common
 18c:	f000 6c00 	li	a0,0
			18c: R_MIPS16_HI16	\.bss
 190:	f400 3480 	sll	a0,16
 194:	f010 4c00 	addiu	a0,-32768
			194: R_MIPS16_LO16	\.bss
 198:	f000 6c00 	li	a0,0
			198: R_MIPS16_HI16	\.sbss
 19c:	f400 3480 	sll	a0,16
 1a0:	f010 4c00 	addiu	a0,-32768
			1a0: R_MIPS16_LO16	\.sbss
 1a4:	6c01      	li	a0,1
 1a6:	f400 3480 	sll	a0,16
 1aa:	4c00      	addiu	a0,0
 1ac:	f000 6c01 	li	a0,1
			1ac: R_MIPS16_HI16	\.data
 1b0:	f400 3480 	sll	a0,16
 1b4:	f000 4c00 	addiu	a0,0
			1b4: R_MIPS16_LO16	\.data
 1b8:	f000 6c01 	li	a0,1
			1b8: R_MIPS16_HI16	\.data
 1bc:	f400 3480 	sll	a0,16
 1c0:	f000 4c04 	addiu	a0,4
			1c0: R_MIPS16_LO16	\.data
 1c4:	f000 6c01 	li	a0,1
			1c4: R_MIPS16_HI16	big_external_data_label
 1c8:	f400 3480 	sll	a0,16
 1cc:	f000 4c00 	addiu	a0,0
			1cc: R_MIPS16_LO16	big_external_data_label
 1d0:	f000 6c01 	li	a0,1
			1d0: R_MIPS16_HI16	small_external_data_label
 1d4:	f400 3480 	sll	a0,16
 1d8:	f000 4c00 	addiu	a0,0
			1d8: R_MIPS16_LO16	small_external_data_label
 1dc:	f000 6c01 	li	a0,1
			1dc: R_MIPS16_HI16	big_external_common
 1e0:	f400 3480 	sll	a0,16
 1e4:	f000 4c00 	addiu	a0,0
			1e4: R_MIPS16_LO16	big_external_common
 1e8:	f000 6c01 	li	a0,1
			1e8: R_MIPS16_HI16	small_external_common
 1ec:	f400 3480 	sll	a0,16
 1f0:	f000 4c00 	addiu	a0,0
			1f0: R_MIPS16_LO16	small_external_common
 1f4:	f000 6c01 	li	a0,1
			1f4: R_MIPS16_HI16	\.bss
 1f8:	f400 3480 	sll	a0,16
 1fc:	f000 4c00 	addiu	a0,0
			1fc: R_MIPS16_LO16	\.bss
 200:	f000 6c01 	li	a0,1
			200: R_MIPS16_HI16	\.sbss
 204:	f400 3480 	sll	a0,16
 208:	f000 4c00 	addiu	a0,0
			208: R_MIPS16_LO16	\.sbss
 20c:	6c02      	li	a0,2
 20e:	f400 3480 	sll	a0,16
 212:	f5b4 4c05 	addiu	a0,-23131
 216:	f000 6c02 	li	a0,2
			216: R_MIPS16_HI16	\.data
 21a:	f400 3480 	sll	a0,16
 21e:	f5b4 4c05 	addiu	a0,-23131
			21e: R_MIPS16_LO16	\.data
 222:	f000 6c02 	li	a0,2
			222: R_MIPS16_HI16	\.data
 226:	f400 3480 	sll	a0,16
 22a:	f5b4 4c09 	addiu	a0,-23127
			22a: R_MIPS16_LO16	\.data
 22e:	f000 6c02 	li	a0,2
			22e: R_MIPS16_HI16	big_external_data_label
 232:	f400 3480 	sll	a0,16
 236:	f5b4 4c05 	addiu	a0,-23131
			236: R_MIPS16_LO16	big_external_data_label
 23a:	f000 6c02 	li	a0,2
			23a: R_MIPS16_HI16	small_external_data_label
 23e:	f400 3480 	sll	a0,16
 242:	f5b4 4c05 	addiu	a0,-23131
			242: R_MIPS16_LO16	small_external_data_label
 246:	f000 6c02 	li	a0,2
			246: R_MIPS16_HI16	big_external_common
 24a:	f400 3480 	sll	a0,16
 24e:	f5b4 4c05 	addiu	a0,-23131
			24e: R_MIPS16_LO16	big_external_common
 252:	f000 6c02 	li	a0,2
			252: R_MIPS16_HI16	small_external_common
 256:	f400 3480 	sll	a0,16
 25a:	f5b4 4c05 	addiu	a0,-23131
			25a: R_MIPS16_LO16	small_external_common
 25e:	f000 6c02 	li	a0,2
			25e: R_MIPS16_HI16	\.bss
 262:	f400 3480 	sll	a0,16
 266:	f5b4 4c05 	addiu	a0,-23131
			266: R_MIPS16_LO16	\.bss
 26a:	f000 6c02 	li	a0,2
			26a: R_MIPS16_HI16	\.sbss
 26e:	f400 3480 	sll	a0,16
 272:	f5b4 4c05 	addiu	a0,-23131
			272: R_MIPS16_LO16	\.sbss
 276:	6d00      	li	a1,0
 278:	f400 35a0 	sll	a1,16
 27c:	9d80      	lw	a0,0\(a1\)
 27e:	f000 6d00 	li	a1,0
			27e: R_MIPS16_HI16	\.data
 282:	f400 35a0 	sll	a1,16
 286:	f000 9d80 	lw	a0,0\(a1\)
			286: R_MIPS16_HI16	\.data
 28a:	f000 6d00 	li	a1,0
			28a: R_MIPS16_HI16	\.data
 28e:	f400 35a0 	sll	a1,16
 292:	f000 9d80 	lw	a0,0\(a1\)
			292: R_MIPS16_HI16	\.data
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
			2e8: R_MIPS16_HI16	\.data
 2ec:	f400 35a0 	sll	a1,16
 2f0:	f000 9d81 	lw	a0,1\(a1\)
			2f0: R_MIPS16_LO16	\.data
 2f4:	f000 6d00 	li	a1,0
			2f4: R_MIPS16_HI16	\.data
 2f8:	f400 35a0 	sll	a1,16
 2fc:	f000 9d85 	lw	a0,5\(a1\)
			2fc: R_MIPS16_LO16	\.data
 300:	f000 6d00 	li	a1,0
			300: R_MIPS16_HI16	big_external_data_label
 304:	f400 35a0 	sll	a1,16
 308:	f000 9d81 	lw	a0,1\(a1\)
			308: R_MIPS16_LO16	big_external_data_label
 30c:	f000 6d00 	li	a1,0
			30c: R_MIPS16_HI16	small_external_data_label
 310:	f400 35a0 	sll	a1,16
 314:	f000 9d81 	lw	a0,1\(a1\)
			314: R_MIPS16_LO16	small_external_data_label
 318:	f000 6d00 	li	a1,0
			318: R_MIPS16_HI16	big_external_common
 31c:	f400 35a0 	sll	a1,16
 320:	f000 9d81 	lw	a0,1\(a1\)
			320: R_MIPS16_LO16	big_external_common
 324:	f000 6d00 	li	a1,0
			324: R_MIPS16_HI16	small_external_common
 328:	f400 35a0 	sll	a1,16
 32c:	f000 9d81 	lw	a0,1\(a1\)
			32c: R_MIPS16_LO16	small_external_common
 330:	f000 6d00 	li	a1,0
			330: R_MIPS16_HI16	\.bss
 334:	f400 35a0 	sll	a1,16
 338:	f000 9d81 	lw	a0,1\(a1\)
			338: R_MIPS16_LO16	\.bss
 33c:	f000 6d00 	li	a1,0
			33c: R_MIPS16_HI16	\.sbss
 340:	f400 35a0 	sll	a1,16
 344:	f000 9d81 	lw	a0,1\(a1\)
			344: R_MIPS16_LO16	\.sbss
 348:	6d01      	li	a1,1
 34a:	f400 35a0 	sll	a1,16
 34e:	f010 9d80 	lw	a0,-32768\(a1\)
 352:	f000 6d01 	li	a1,1
			352: R_MIPS16_HI16	\.data
 356:	f400 35a0 	sll	a1,16
 35a:	f010 9d80 	lw	a0,-32768\(a1\)
			35a: R_MIPS16_LO16	\.data
 35e:	f000 6d01 	li	a1,1
			35e: R_MIPS16_HI16	\.data
 362:	f400 35a0 	sll	a1,16
 366:	f010 9d84 	lw	a0,-32764\(a1\)
			366: R_MIPS16_LO16	\.data
 36a:	f000 6d01 	li	a1,1
			36a: R_MIPS16_HI16	big_external_data_label
 36e:	f400 35a0 	sll	a1,16
 372:	f010 9d80 	lw	a0,-32768\(a1\)
			372: R_MIPS16_LO16	big_external_data_label
 376:	f000 6d01 	li	a1,1
			376: R_MIPS16_HI16	small_external_data_label
 37a:	f400 35a0 	sll	a1,16
 37e:	f010 9d80 	lw	a0,-32768\(a1\)
			37e: R_MIPS16_LO16	small_external_data_label
 382:	f000 6d01 	li	a1,1
			382: R_MIPS16_HI16	big_external_common
 386:	f400 35a0 	sll	a1,16
 38a:	f010 9d80 	lw	a0,-32768\(a1\)
			38a: R_MIPS16_LO16	big_external_common
 38e:	f000 6d01 	li	a1,1
			38e: R_MIPS16_HI16	small_external_common
 392:	f400 35a0 	sll	a1,16
 396:	f010 9d80 	lw	a0,-32768\(a1\)
			396: R_MIPS16_LO16	small_external_common
 39a:	f000 6d01 	li	a1,1
			39a: R_MIPS16_HI16	\.bss
 39e:	f400 35a0 	sll	a1,16
 3a2:	f010 9d80 	lw	a0,-32768\(a1\)
			3a2: R_MIPS16_LO16	\.bss
 3a6:	f000 6d01 	li	a1,1
			3a6: R_MIPS16_HI16	\.sbss
 3aa:	f400 35a0 	sll	a1,16
 3ae:	f010 9d80 	lw	a0,-32768\(a1\)
			3ae: R_MIPS16_LO16	\.sbss
 3b2:	6d00      	li	a1,0
 3b4:	f400 35a0 	sll	a1,16
 3b8:	f010 9d80 	lw	a0,-32768\(a1\)
 3bc:	f000 6d00 	li	a1,0
			3bc: R_MIPS16_HI16	\.data
 3c0:	f400 35a0 	sll	a1,16
 3c4:	f010 9d80 	lw	a0,-32768\(a1\)
			3c4: R_MIPS16_LO16	\.data
 3c8:	f000 6d00 	li	a1,0
			3c8: R_MIPS16_HI16	\.data
 3cc:	f400 35a0 	sll	a1,16
 3d0:	f010 9d84 	lw	a0,-32764\(a1\)
			3d0: R_MIPS16_LO16	\.data
 3d4:	f000 6d00 	li	a1,0
			3d4: R_MIPS16_HI16	big_external_data_label
 3d8:	f400 35a0 	sll	a1,16
 3dc:	f010 9d80 	lw	a0,-32768\(a1\)
			3dc: R_MIPS16_LO16	big_external_data_label
 3e0:	f000 6d00 	li	a1,0
			3e0: R_MIPS16_HI16	small_external_data_label
 3e4:	f400 35a0 	sll	a1,16
 3e8:	f010 9d80 	lw	a0,-32768\(a1\)
			3e8: R_MIPS16_LO16	small_external_data_label
 3ec:	f000 6d00 	li	a1,0
			3ec: R_MIPS16_HI16	big_external_common
 3f0:	f400 35a0 	sll	a1,16
 3f4:	f010 9d80 	lw	a0,-32768\(a1\)
			3f4: R_MIPS16_LO16	big_external_common
 3f8:	f000 6d00 	li	a1,0
			3f8: R_MIPS16_HI16	small_external_common
 3fc:	f400 35a0 	sll	a1,16
 400:	f010 9d80 	lw	a0,-32768\(a1\)
			400: R_MIPS16_LO16	small_external_common
 404:	f000 6d00 	li	a1,0
			404: R_MIPS16_HI16	\.bss
 408:	f400 35a0 	sll	a1,16
 40c:	f010 9d80 	lw	a0,-32768\(a1\)
			40c: R_MIPS16_LO16	\.bss
 410:	f000 6d00 	li	a1,0
			410: R_MIPS16_HI16	\.sbss
 414:	f400 35a0 	sll	a1,16
 418:	f010 9d80 	lw	a0,-32768\(a1\)
			418: R_MIPS16_LO16	\.sbss
 41c:	6d01      	li	a1,1
 41e:	f400 35a0 	sll	a1,16
 422:	9d80      	lw	a0,0\(a1\)
 424:	f000 6d01 	li	a1,1
			424: R_MIPS16_HI16	\.data
 428:	f400 35a0 	sll	a1,16
 42c:	f000 9d80 	lw	a0,0\(a1\)
			42c: R_MIPS16_LO16	\.data
 430:	f000 6d01 	li	a1,1
			430: R_MIPS16_HI16	\.data
 434:	f400 35a0 	sll	a1,16
 438:	f000 9d84 	lw	a0,4\(a1\)
			438: R_MIPS16_LO16	\.data
 43c:	f000 6d01 	li	a1,1
			43c: R_MIPS16_HI16	big_external_data_label
 440:	f400 35a0 	sll	a1,16
 444:	f000 9d80 	lw	a0,0\(a1\)
			444: R_MIPS16_LO16	big_external_data_label
 448:	f000 6d01 	li	a1,1
			448: R_MIPS16_HI16	small_external_data_label
 44c:	f400 35a0 	sll	a1,16
 450:	f000 9d80 	lw	a0,0\(a1\)
			450: R_MIPS16_LO16	small_external_data_label
 454:	f000 6d01 	li	a1,1
			454: R_MIPS16_HI16	big_external_common
 458:	f400 35a0 	sll	a1,16
 45c:	f000 9d80 	lw	a0,0\(a1\)
			45c: R_MIPS16_LO16	big_external_common
 460:	f000 6d01 	li	a1,1
			460: R_MIPS16_HI16	small_external_common
 464:	f400 35a0 	sll	a1,16
 468:	f000 9d80 	lw	a0,0\(a1\)
			468: R_MIPS16_LO16	small_external_common
 46c:	f000 6d01 	li	a1,1
			46c: R_MIPS16_HI16	\.bss
 470:	f400 35a0 	sll	a1,16
 474:	f000 9d80 	lw	a0,0\(a1\)
			474: R_MIPS16_LO16	\.bss
 478:	f000 6d01 	li	a1,1
			478: R_MIPS16_HI16	\.sbss
 47c:	f400 35a0 	sll	a1,16
 480:	f000 9d80 	lw	a0,0\(a1\)
			480: R_MIPS16_LO16	\.sbss
 484:	6d02      	li	a1,2
 486:	f400 35a0 	sll	a1,16
 48a:	f5b4 9d85 	lw	a0,-23131\(a1\)
 48e:	f000 6d02 	li	a1,2
			48e: R_MIPS16_HI16	\.data
 492:	f400 35a0 	sll	a1,16
 496:	f5b4 9d85 	lw	a0,-23131\(a1\)
			496: R_MIPS16_LO16	\.data
 49a:	f000 6d02 	li	a1,2
			49a: R_MIPS16_HI16	\.data
 49e:	f400 35a0 	sll	a1,16
 4a2:	f5b4 9d89 	lw	a0,-23127\(a1\)
			4a2: R_MIPS16_LO16	\.data
 4a6:	f000 6d02 	li	a1,2
			4a6: R_MIPS16_HI16	big_external_data_label
 4aa:	f400 35a0 	sll	a1,16
 4ae:	f5b4 9d85 	lw	a0,-23131\(a1\)
			4ae: R_MIPS16_LO16	big_external_data_label
 4b2:	f000 6d02 	li	a1,2
			4b2: R_MIPS16_HI16	small_external_data_label
 4b6:	f400 35a0 	sll	a1,16
 4ba:	f5b4 9d85 	lw	a0,-23131\(a1\)
			4ba: R_MIPS16_LO16	small_external_data_label
 4be:	f000 6d02 	li	a1,2
			4be: R_MIPS16_HI16	big_external_common
 4c2:	f400 35a0 	sll	a1,16
 4c6:	f5b4 9d85 	lw	a0,-23131\(a1\)
			4c6: R_MIPS16_LO16	big_external_common
 4ca:	f000 6d02 	li	a1,2
			4ca: R_MIPS16_HI16	small_external_common
 4ce:	f400 35a0 	sll	a1,16
 4d2:	f5b4 9d85 	lw	a0,-23131\(a1\)
			4d2: R_MIPS16_LO16	small_external_common
 4d6:	f000 6d02 	li	a1,2
			4d6: R_MIPS16_HI16	\.bss
 4da:	f400 35a0 	sll	a1,16
 4de:	f5b4 9d85 	lw	a0,-23131\(a1\)
			4de: R_MIPS16_LO16	\.bss
 4e2:	f000 6d02 	li	a1,2
			4e2: R_MIPS16_HI16	\.sbss
 4e6:	f400 35a0 	sll	a1,16
 4ea:	f5b4 9d85 	lw	a0,-23131\(a1\)
			4ea: R_MIPS16_LO16	\.sbss
 4ee:	6500      	nop
