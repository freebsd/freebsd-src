#as: -EL
#objdump: -dr
#source: dj1.s
#name: dj1.le

dump.o:     file format elf32-mep-little

Disassembly of section .text:

00000000 <.text>:
   0:	00 00       	nop
   2:	00 01       	mov \$1,\$0
   4:	00 02       	mov \$2,\$0
   6:	00 03       	mov \$3,\$0
   8:	00 04       	mov \$4,\$0
   a:	00 05       	mov \$5,\$0
   c:	00 06       	mov \$6,\$0
   e:	00 07       	mov \$7,\$0
  10:	00 08       	mov \$8,\$0
  12:	00 09       	mov \$9,\$0
  14:	00 0a       	mov \$10,\$0
  16:	00 0b       	mov \$11,\$0
  18:	00 0c       	mov \$12,\$0
  1a:	00 0d       	mov \$tp,\$0
  1c:	00 0e       	mov \$gp,\$0
  1e:	00 0f       	mov \$sp,\$0
  20:	00 08       	mov \$8,\$0
  22:	00 0d       	mov \$tp,\$0
  24:	00 0e       	mov \$gp,\$0
  26:	00 0f       	mov \$sp,\$0
  28:	08 00       	sb \$0,\(\$0\)
  2a:	09 00       	sh \$0,\(\$0\)
  2c:	0a 00       	sw \$0,\(\$0\)
  2e:	0c 00       	lb \$0,\(\$0\)
  30:	0d 00       	lh \$0,\(\$0\)
  32:	0e 00       	lw \$0,\(\$0\)
  34:	0b 00       	lbu \$0,\(\$0\)
  36:	0f 00       	lhu \$0,\(\$0\)
  38:	08 0f       	sb \$sp,\(\$0\)
  3a:	09 0f       	sh \$sp,\(\$0\)
  3c:	0a 0f       	sw \$sp,\(\$0\)
  3e:	0c 0f       	lb \$sp,\(\$0\)
  40:	0d 0f       	lh \$sp,\(\$0\)
  42:	0e 0f       	lw \$sp,\(\$0\)
  44:	0b 0f       	lbu \$sp,\(\$0\)
  46:	0f 0f       	lhu \$sp,\(\$0\)
  48:	f8 00       	sb \$0,\(\$sp\)
  4a:	f9 00       	sh \$0,\(\$sp\)
  4c:	fa 00       	sw \$0,\(\$sp\)
  4e:	fc 00       	lb \$0,\(\$sp\)
  50:	fd 00       	lh \$0,\(\$sp\)
  52:	fe 00       	lw \$0,\(\$sp\)
  54:	fb 00       	lbu \$0,\(\$sp\)
  56:	ff 00       	lhu \$0,\(\$sp\)
  58:	f8 0f       	sb \$sp,\(\$sp\)
  5a:	f9 0f       	sh \$sp,\(\$sp\)
  5c:	fa 0f       	sw \$sp,\(\$sp\)
  5e:	fc 0f       	lb \$sp,\(\$sp\)
  60:	fd 0f       	lh \$sp,\(\$sp\)
  62:	fe 0f       	lw \$sp,\(\$sp\)
  64:	fb 0f       	lbu \$sp,\(\$sp\)
  66:	ff 0f       	lhu \$sp,\(\$sp\)
  68:	fa 00       	sw \$0,\(\$sp\)
  6a:	fe 00       	lw \$0,\(\$sp\)
  6c:	fa 0f       	sw \$sp,\(\$sp\)
  6e:	fe 0f       	lw \$sp,\(\$sp\)
  70:	7e 40       	sw \$0,0x7c\(\$sp\)
  72:	7f 40       	lw \$0,0x7c\(\$sp\)
  74:	7e 4f       	sw \$sp,0x7c\(\$sp\)
  76:	7f 4f       	lw \$sp,0x7c\(\$sp\)
  78:	fa 00       	sw \$0,\(\$sp\)
  7a:	fe 00       	lw \$0,\(\$sp\)
  7c:	fa 0f       	sw \$sp,\(\$sp\)
  7e:	fe 0f       	lw \$sp,\(\$sp\)
  80:	7e 40       	sw \$0,0x7c\(\$sp\)
  82:	7f 40       	lw \$0,0x7c\(\$sp\)
  84:	7e 4f       	sw \$sp,0x7c\(\$sp\)
  86:	7f 4f       	lw \$sp,0x7c\(\$sp\)
  88:	d8 00       	sb \$0,\(\$tp\)
  8a:	dc 00       	lb \$0,\(\$tp\)
  8c:	db 00       	lbu \$0,\(\$tp\)
  8e:	d8 07       	sb \$7,\(\$tp\)
  90:	dc 07       	lb \$7,\(\$tp\)
  92:	db 07       	lbu \$7,\(\$tp\)
  94:	7f 80       	sb \$0,0x7f\(\$tp\)
  96:	7f 88       	lb \$0,0x7f\(\$tp\)
  98:	ff 48       	lbu \$0,0x7f\(\$tp\)
  9a:	7f 87       	sb \$7,0x7f\(\$tp\)
  9c:	7f 8f       	lb \$7,0x7f\(\$tp\)
  9e:	ff 4f       	lbu \$7,0x7f\(\$tp\)
  a0:	00 80       	sb \$0,0x0\(\$tp\)
			a0: R_MEP_TPREL7	symbol
  a2:	00 88       	lb \$0,0x0\(\$tp\)
			a2: R_MEP_TPREL7	symbol
  a4:	80 48       	lbu \$0,0x0\(\$tp\)
			a4: R_MEP_TPREL7	symbol
  a6:	00 87       	sb \$7,0x0\(\$tp\)
			a6: R_MEP_TPREL7	symbol
  a8:	00 8f       	lb \$7,0x0\(\$tp\)
			a8: R_MEP_TPREL7	symbol
  aa:	80 4f       	lbu \$7,0x0\(\$tp\)
			aa: R_MEP_TPREL7	symbol
  ac:	d8 00       	sb \$0,\(\$tp\)
  ae:	dc 00       	lb \$0,\(\$tp\)
  b0:	db 00       	lbu \$0,\(\$tp\)
  b2:	d8 07       	sb \$7,\(\$tp\)
  b4:	dc 07       	lb \$7,\(\$tp\)
  b6:	db 07       	lbu \$7,\(\$tp\)
  b8:	7f 80       	sb \$0,0x7f\(\$tp\)
  ba:	7f 88       	lb \$0,0x7f\(\$tp\)
  bc:	ff 48       	lbu \$0,0x7f\(\$tp\)
  be:	7f 87       	sb \$7,0x7f\(\$tp\)
  c0:	7f 8f       	lb \$7,0x7f\(\$tp\)
  c2:	ff 4f       	lbu \$7,0x7f\(\$tp\)
  c4:	00 80       	sb \$0,0x0\(\$tp\)
			c4: R_MEP_TPREL7	symbol
  c6:	00 88       	lb \$0,0x0\(\$tp\)
			c6: R_MEP_TPREL7	symbol
  c8:	80 48       	lbu \$0,0x0\(\$tp\)
			c8: R_MEP_TPREL7	symbol
  ca:	00 87       	sb \$7,0x0\(\$tp\)
			ca: R_MEP_TPREL7	symbol
  cc:	00 8f       	lb \$7,0x0\(\$tp\)
			cc: R_MEP_TPREL7	symbol
  ce:	80 4f       	lbu \$7,0x0\(\$tp\)
			ce: R_MEP_TPREL7	symbol
  d0:	d9 00       	sh \$0,\(\$tp\)
  d2:	dd 00       	lh \$0,\(\$tp\)
  d4:	df 00       	lhu \$0,\(\$tp\)
  d6:	d9 07       	sh \$7,\(\$tp\)
  d8:	dd 07       	lh \$7,\(\$tp\)
  da:	df 07       	lhu \$7,\(\$tp\)
  dc:	fe 80       	sh \$0,0x7e\(\$tp\)
  de:	fe 88       	lh \$0,0x7e\(\$tp\)
  e0:	ff 88       	lhu \$0,0x7e\(\$tp\)
  e2:	fe 87       	sh \$7,0x7e\(\$tp\)
  e4:	fe 8f       	lh \$7,0x7e\(\$tp\)
  e6:	ff 8f       	lhu \$7,0x7e\(\$tp\)
  e8:	80 80       	sh \$0,0x0\(\$tp\)
			e8: R_MEP_TPREL7A2	symbol
  ea:	80 88       	lh \$0,0x0\(\$tp\)
			ea: R_MEP_TPREL7A2	symbol
  ec:	81 88       	lhu \$0,0x0\(\$tp\)
			ec: R_MEP_TPREL7A2	symbol
  ee:	80 87       	sh \$7,0x0\(\$tp\)
			ee: R_MEP_TPREL7A2	symbol
  f0:	80 8f       	lh \$7,0x0\(\$tp\)
			f0: R_MEP_TPREL7A2	symbol
  f2:	81 8f       	lhu \$7,0x0\(\$tp\)
			f2: R_MEP_TPREL7A2	symbol
  f4:	d9 00       	sh \$0,\(\$tp\)
  f6:	dd 00       	lh \$0,\(\$tp\)
  f8:	df 00       	lhu \$0,\(\$tp\)
  fa:	d9 07       	sh \$7,\(\$tp\)
  fc:	dd 07       	lh \$7,\(\$tp\)
  fe:	df 07       	lhu \$7,\(\$tp\)
 100:	fe 80       	sh \$0,0x7e\(\$tp\)
 102:	fe 88       	lh \$0,0x7e\(\$tp\)
 104:	ff 88       	lhu \$0,0x7e\(\$tp\)
 106:	fe 87       	sh \$7,0x7e\(\$tp\)
 108:	fe 8f       	lh \$7,0x7e\(\$tp\)
 10a:	ff 8f       	lhu \$7,0x7e\(\$tp\)
 10c:	80 80       	sh \$0,0x0\(\$tp\)
			10c: R_MEP_TPREL7A2	symbol
 10e:	80 88       	lh \$0,0x0\(\$tp\)
			10e: R_MEP_TPREL7A2	symbol
 110:	81 88       	lhu \$0,0x0\(\$tp\)
			110: R_MEP_TPREL7A2	symbol
 112:	80 87       	sh \$7,0x0\(\$tp\)
			112: R_MEP_TPREL7A2	symbol
 114:	80 8f       	lh \$7,0x0\(\$tp\)
			114: R_MEP_TPREL7A2	symbol
 116:	81 8f       	lhu \$7,0x0\(\$tp\)
			116: R_MEP_TPREL7A2	symbol
 118:	da 00       	sw \$0,\(\$tp\)
 11a:	de 00       	lw \$0,\(\$tp\)
 11c:	da 07       	sw \$7,\(\$tp\)
 11e:	de 07       	lw \$7,\(\$tp\)
 120:	fe 40       	sw \$0,0x7c\(\$tp\)
 122:	ff 40       	lw \$0,0x7c\(\$tp\)
 124:	fe 47       	sw \$7,0x7c\(\$tp\)
 126:	ff 47       	lw \$7,0x7c\(\$tp\)
 128:	82 40       	sw \$0,0x0\(\$tp\)
			128: R_MEP_TPREL7A4	symbol
 12a:	83 40       	lw \$0,0x0\(\$tp\)
			12a: R_MEP_TPREL7A4	symbol
 12c:	82 47       	sw \$7,0x0\(\$tp\)
			12c: R_MEP_TPREL7A4	symbol
 12e:	83 47       	lw \$7,0x0\(\$tp\)
			12e: R_MEP_TPREL7A4	symbol
 130:	da 00       	sw \$0,\(\$tp\)
 132:	de 00       	lw \$0,\(\$tp\)
 134:	da 07       	sw \$7,\(\$tp\)
 136:	de 07       	lw \$7,\(\$tp\)
 138:	fe 40       	sw \$0,0x7c\(\$tp\)
 13a:	ff 40       	lw \$0,0x7c\(\$tp\)
 13c:	fe 47       	sw \$7,0x7c\(\$tp\)
 13e:	ff 47       	lw \$7,0x7c\(\$tp\)
 140:	82 40       	sw \$0,0x0\(\$tp\)
			140: R_MEP_TPREL7A4	symbol
 142:	83 40       	lw \$0,0x0\(\$tp\)
			142: R_MEP_TPREL7A4	symbol
 144:	82 47       	sw \$7,0x0\(\$tp\)
			144: R_MEP_TPREL7A4	symbol
 146:	83 47       	lw \$7,0x0\(\$tp\)
			146: R_MEP_TPREL7A4	symbol
 148:	08 c0 00 80 	sb \$0,-32768\(\$0\)
 14c:	09 c0 00 80 	sh \$0,-32768\(\$0\)
 150:	0a c0 00 80 	sw \$0,-32768\(\$0\)
 154:	0c c0 00 80 	lb \$0,-32768\(\$0\)
 158:	0d c0 00 80 	lh \$0,-32768\(\$0\)
 15c:	0e c0 00 80 	lw \$0,-32768\(\$0\)
 160:	0b c0 00 80 	lbu \$0,-32768\(\$0\)
 164:	0f c0 00 80 	lhu \$0,-32768\(\$0\)
 168:	08 cf 00 80 	sb \$sp,-32768\(\$0\)
 16c:	09 cf 00 80 	sh \$sp,-32768\(\$0\)
 170:	0a cf 00 80 	sw \$sp,-32768\(\$0\)
 174:	0c cf 00 80 	lb \$sp,-32768\(\$0\)
 178:	0d cf 00 80 	lh \$sp,-32768\(\$0\)
 17c:	0e cf 00 80 	lw \$sp,-32768\(\$0\)
 180:	0b cf 00 80 	lbu \$sp,-32768\(\$0\)
 184:	0f cf 00 80 	lhu \$sp,-32768\(\$0\)
 188:	08 c0 ff 7f 	sb \$0,32767\(\$0\)
 18c:	09 c0 ff 7f 	sh \$0,32767\(\$0\)
 190:	0a c0 ff 7f 	sw \$0,32767\(\$0\)
 194:	0c c0 ff 7f 	lb \$0,32767\(\$0\)
 198:	0d c0 ff 7f 	lh \$0,32767\(\$0\)
 19c:	0e c0 ff 7f 	lw \$0,32767\(\$0\)
 1a0:	0b c0 ff 7f 	lbu \$0,32767\(\$0\)
 1a4:	0f c0 ff 7f 	lhu \$0,32767\(\$0\)
 1a8:	08 cf ff 7f 	sb \$sp,32767\(\$0\)
 1ac:	09 cf ff 7f 	sh \$sp,32767\(\$0\)
 1b0:	0a cf ff 7f 	sw \$sp,32767\(\$0\)
 1b4:	0c cf ff 7f 	lb \$sp,32767\(\$0\)
 1b8:	0d cf ff 7f 	lh \$sp,32767\(\$0\)
 1bc:	0e cf ff 7f 	lw \$sp,32767\(\$0\)
 1c0:	0b cf ff 7f 	lbu \$sp,32767\(\$0\)
 1c4:	0f cf ff 7f 	lhu \$sp,32767\(\$0\)
 1c8:	08 c0 00 00 	sb \$0,0\(\$0\)
			1c8: R_MEP_GPREL	symbol
 1cc:	09 c0 00 00 	sh \$0,0\(\$0\)
			1cc: R_MEP_GPREL	symbol
 1d0:	0a c0 00 00 	sw \$0,0\(\$0\)
			1d0: R_MEP_GPREL	symbol
 1d4:	0c c0 00 00 	lb \$0,0\(\$0\)
			1d4: R_MEP_GPREL	symbol
 1d8:	0d c0 00 00 	lh \$0,0\(\$0\)
			1d8: R_MEP_GPREL	symbol
 1dc:	0e c0 00 00 	lw \$0,0\(\$0\)
			1dc: R_MEP_GPREL	symbol
 1e0:	0b c0 00 00 	lbu \$0,0\(\$0\)
			1e0: R_MEP_GPREL	symbol
 1e4:	0f c0 00 00 	lhu \$0,0\(\$0\)
			1e4: R_MEP_GPREL	symbol
 1e8:	08 cf 00 00 	sb \$sp,0\(\$0\)
			1e8: R_MEP_GPREL	symbol
 1ec:	09 cf 00 00 	sh \$sp,0\(\$0\)
			1ec: R_MEP_GPREL	symbol
 1f0:	0a cf 00 00 	sw \$sp,0\(\$0\)
			1f0: R_MEP_GPREL	symbol
 1f4:	0c cf 00 00 	lb \$sp,0\(\$0\)
			1f4: R_MEP_GPREL	symbol
 1f8:	0d cf 00 00 	lh \$sp,0\(\$0\)
			1f8: R_MEP_GPREL	symbol
 1fc:	0e cf 00 00 	lw \$sp,0\(\$0\)
			1fc: R_MEP_GPREL	symbol
 200:	0b cf 00 00 	lbu \$sp,0\(\$0\)
			200: R_MEP_GPREL	symbol
 204:	0f cf 00 00 	lhu \$sp,0\(\$0\)
			204: R_MEP_GPREL	symbol
 208:	08 c0 00 80 	sb \$0,-32768\(\$0\)
 20c:	09 c0 00 80 	sh \$0,-32768\(\$0\)
 210:	0a c0 00 80 	sw \$0,-32768\(\$0\)
 214:	0c c0 00 80 	lb \$0,-32768\(\$0\)
 218:	0d c0 00 80 	lh \$0,-32768\(\$0\)
 21c:	0e c0 00 80 	lw \$0,-32768\(\$0\)
 220:	0b c0 00 80 	lbu \$0,-32768\(\$0\)
 224:	0f c0 00 80 	lhu \$0,-32768\(\$0\)
 228:	08 cf 00 80 	sb \$sp,-32768\(\$0\)
 22c:	09 cf 00 80 	sh \$sp,-32768\(\$0\)
 230:	0a cf 00 80 	sw \$sp,-32768\(\$0\)
 234:	0c cf 00 80 	lb \$sp,-32768\(\$0\)
 238:	0d cf 00 80 	lh \$sp,-32768\(\$0\)
 23c:	0e cf 00 80 	lw \$sp,-32768\(\$0\)
 240:	0b cf 00 80 	lbu \$sp,-32768\(\$0\)
 244:	0f cf 00 80 	lhu \$sp,-32768\(\$0\)
 248:	08 c0 ff 7f 	sb \$0,32767\(\$0\)
 24c:	09 c0 ff 7f 	sh \$0,32767\(\$0\)
 250:	0a c0 ff 7f 	sw \$0,32767\(\$0\)
 254:	0c c0 ff 7f 	lb \$0,32767\(\$0\)
 258:	0d c0 ff 7f 	lh \$0,32767\(\$0\)
 25c:	0e c0 ff 7f 	lw \$0,32767\(\$0\)
 260:	0b c0 ff 7f 	lbu \$0,32767\(\$0\)
 264:	0f c0 ff 7f 	lhu \$0,32767\(\$0\)
 268:	08 cf ff 7f 	sb \$sp,32767\(\$0\)
 26c:	09 cf ff 7f 	sh \$sp,32767\(\$0\)
 270:	0a cf ff 7f 	sw \$sp,32767\(\$0\)
 274:	0c cf ff 7f 	lb \$sp,32767\(\$0\)
 278:	0d cf ff 7f 	lh \$sp,32767\(\$0\)
 27c:	0e cf ff 7f 	lw \$sp,32767\(\$0\)
 280:	0b cf ff 7f 	lbu \$sp,32767\(\$0\)
 284:	0f cf ff 7f 	lhu \$sp,32767\(\$0\)
 288:	08 c0 00 00 	sb \$0,0\(\$0\)
			288: R_MEP_TPREL	symbol
 28c:	09 c0 00 00 	sh \$0,0\(\$0\)
			28c: R_MEP_TPREL	symbol
 290:	0a c0 00 00 	sw \$0,0\(\$0\)
			290: R_MEP_TPREL	symbol
 294:	0c c0 00 00 	lb \$0,0\(\$0\)
			294: R_MEP_TPREL	symbol
 298:	0d c0 00 00 	lh \$0,0\(\$0\)
			298: R_MEP_TPREL	symbol
 29c:	0e c0 00 00 	lw \$0,0\(\$0\)
			29c: R_MEP_TPREL	symbol
 2a0:	0b c0 00 00 	lbu \$0,0\(\$0\)
			2a0: R_MEP_TPREL	symbol
 2a4:	0f c0 00 00 	lhu \$0,0\(\$0\)
			2a4: R_MEP_TPREL	symbol
 2a8:	08 cf 00 00 	sb \$sp,0\(\$0\)
			2a8: R_MEP_TPREL	symbol
 2ac:	09 cf 00 00 	sh \$sp,0\(\$0\)
			2ac: R_MEP_TPREL	symbol
 2b0:	0a cf 00 00 	sw \$sp,0\(\$0\)
			2b0: R_MEP_TPREL	symbol
 2b4:	0c cf 00 00 	lb \$sp,0\(\$0\)
			2b4: R_MEP_TPREL	symbol
 2b8:	0d cf 00 00 	lh \$sp,0\(\$0\)
			2b8: R_MEP_TPREL	symbol
 2bc:	0e cf 00 00 	lw \$sp,0\(\$0\)
			2bc: R_MEP_TPREL	symbol
 2c0:	0b cf 00 00 	lbu \$sp,0\(\$0\)
			2c0: R_MEP_TPREL	symbol
 2c4:	0f cf 00 00 	lhu \$sp,0\(\$0\)
			2c4: R_MEP_TPREL	symbol
 2c8:	f8 c0 00 80 	sb \$0,-32768\(\$sp\)
 2cc:	f9 c0 00 80 	sh \$0,-32768\(\$sp\)
 2d0:	fa c0 00 80 	sw \$0,-32768\(\$sp\)
 2d4:	fc c0 00 80 	lb \$0,-32768\(\$sp\)
 2d8:	fd c0 00 80 	lh \$0,-32768\(\$sp\)
 2dc:	fe c0 00 80 	lw \$0,-32768\(\$sp\)
 2e0:	fb c0 00 80 	lbu \$0,-32768\(\$sp\)
 2e4:	ff c0 00 80 	lhu \$0,-32768\(\$sp\)
 2e8:	f8 cf 00 80 	sb \$sp,-32768\(\$sp\)
 2ec:	f9 cf 00 80 	sh \$sp,-32768\(\$sp\)
 2f0:	fa cf 00 80 	sw \$sp,-32768\(\$sp\)
 2f4:	fc cf 00 80 	lb \$sp,-32768\(\$sp\)
 2f8:	fd cf 00 80 	lh \$sp,-32768\(\$sp\)
 2fc:	fe cf 00 80 	lw \$sp,-32768\(\$sp\)
 300:	fb cf 00 80 	lbu \$sp,-32768\(\$sp\)
 304:	ff cf 00 80 	lhu \$sp,-32768\(\$sp\)
 308:	f8 c0 ff 7f 	sb \$0,32767\(\$sp\)
 30c:	f9 c0 ff 7f 	sh \$0,32767\(\$sp\)
 310:	fa c0 ff 7f 	sw \$0,32767\(\$sp\)
 314:	fc c0 ff 7f 	lb \$0,32767\(\$sp\)
 318:	fd c0 ff 7f 	lh \$0,32767\(\$sp\)
 31c:	fe c0 ff 7f 	lw \$0,32767\(\$sp\)
 320:	fb c0 ff 7f 	lbu \$0,32767\(\$sp\)
 324:	ff c0 ff 7f 	lhu \$0,32767\(\$sp\)
 328:	f8 cf ff 7f 	sb \$sp,32767\(\$sp\)
 32c:	f9 cf ff 7f 	sh \$sp,32767\(\$sp\)
 330:	fa cf ff 7f 	sw \$sp,32767\(\$sp\)
 334:	fc cf ff 7f 	lb \$sp,32767\(\$sp\)
 338:	fd cf ff 7f 	lh \$sp,32767\(\$sp\)
 33c:	fe cf ff 7f 	lw \$sp,32767\(\$sp\)
 340:	fb cf ff 7f 	lbu \$sp,32767\(\$sp\)
 344:	ff cf ff 7f 	lhu \$sp,32767\(\$sp\)
 348:	f8 c0 00 00 	sb \$0,0\(\$sp\)
			348: R_MEP_GPREL	symbol
 34c:	f9 c0 00 00 	sh \$0,0\(\$sp\)
			34c: R_MEP_GPREL	symbol
 350:	fa c0 00 00 	sw \$0,0\(\$sp\)
			350: R_MEP_GPREL	symbol
 354:	fc c0 00 00 	lb \$0,0\(\$sp\)
			354: R_MEP_GPREL	symbol
 358:	fd c0 00 00 	lh \$0,0\(\$sp\)
			358: R_MEP_GPREL	symbol
 35c:	fe c0 00 00 	lw \$0,0\(\$sp\)
			35c: R_MEP_GPREL	symbol
 360:	fb c0 00 00 	lbu \$0,0\(\$sp\)
			360: R_MEP_GPREL	symbol
 364:	ff c0 00 00 	lhu \$0,0\(\$sp\)
			364: R_MEP_GPREL	symbol
 368:	f8 cf 00 00 	sb \$sp,0\(\$sp\)
			368: R_MEP_GPREL	symbol
 36c:	f9 cf 00 00 	sh \$sp,0\(\$sp\)
			36c: R_MEP_GPREL	symbol
 370:	fa cf 00 00 	sw \$sp,0\(\$sp\)
			370: R_MEP_GPREL	symbol
 374:	fc cf 00 00 	lb \$sp,0\(\$sp\)
			374: R_MEP_GPREL	symbol
 378:	fd cf 00 00 	lh \$sp,0\(\$sp\)
			378: R_MEP_GPREL	symbol
 37c:	fe cf 00 00 	lw \$sp,0\(\$sp\)
			37c: R_MEP_GPREL	symbol
 380:	fb cf 00 00 	lbu \$sp,0\(\$sp\)
			380: R_MEP_GPREL	symbol
 384:	ff cf 00 00 	lhu \$sp,0\(\$sp\)
			384: R_MEP_GPREL	symbol
 388:	f8 c0 00 80 	sb \$0,-32768\(\$sp\)
 38c:	f9 c0 00 80 	sh \$0,-32768\(\$sp\)
 390:	fa c0 00 80 	sw \$0,-32768\(\$sp\)
 394:	fc c0 00 80 	lb \$0,-32768\(\$sp\)
 398:	fd c0 00 80 	lh \$0,-32768\(\$sp\)
 39c:	fe c0 00 80 	lw \$0,-32768\(\$sp\)
 3a0:	fb c0 00 80 	lbu \$0,-32768\(\$sp\)
 3a4:	ff c0 00 80 	lhu \$0,-32768\(\$sp\)
 3a8:	f8 cf 00 80 	sb \$sp,-32768\(\$sp\)
 3ac:	f9 cf 00 80 	sh \$sp,-32768\(\$sp\)
 3b0:	fa cf 00 80 	sw \$sp,-32768\(\$sp\)
 3b4:	fc cf 00 80 	lb \$sp,-32768\(\$sp\)
 3b8:	fd cf 00 80 	lh \$sp,-32768\(\$sp\)
 3bc:	fe cf 00 80 	lw \$sp,-32768\(\$sp\)
 3c0:	fb cf 00 80 	lbu \$sp,-32768\(\$sp\)
 3c4:	ff cf 00 80 	lhu \$sp,-32768\(\$sp\)
 3c8:	f8 c0 ff 7f 	sb \$0,32767\(\$sp\)
 3cc:	f9 c0 ff 7f 	sh \$0,32767\(\$sp\)
 3d0:	fa c0 ff 7f 	sw \$0,32767\(\$sp\)
 3d4:	fc c0 ff 7f 	lb \$0,32767\(\$sp\)
 3d8:	fd c0 ff 7f 	lh \$0,32767\(\$sp\)
 3dc:	fe c0 ff 7f 	lw \$0,32767\(\$sp\)
 3e0:	fb c0 ff 7f 	lbu \$0,32767\(\$sp\)
 3e4:	ff c0 ff 7f 	lhu \$0,32767\(\$sp\)
 3e8:	f8 cf ff 7f 	sb \$sp,32767\(\$sp\)
 3ec:	f9 cf ff 7f 	sh \$sp,32767\(\$sp\)
 3f0:	fa cf ff 7f 	sw \$sp,32767\(\$sp\)
 3f4:	fc cf ff 7f 	lb \$sp,32767\(\$sp\)
 3f8:	fd cf ff 7f 	lh \$sp,32767\(\$sp\)
 3fc:	fe cf ff 7f 	lw \$sp,32767\(\$sp\)
 400:	fb cf ff 7f 	lbu \$sp,32767\(\$sp\)
 404:	ff cf ff 7f 	lhu \$sp,32767\(\$sp\)
 408:	f8 c0 00 00 	sb \$0,0\(\$sp\)
			408: R_MEP_TPREL	symbol
 40c:	f9 c0 00 00 	sh \$0,0\(\$sp\)
			40c: R_MEP_TPREL	symbol
 410:	02 40       	sw \$0,0x0\(\$sp\)
			410: R_MEP_TPREL7A4	symbol
 412:	fc c0 00 00 	lb \$0,0\(\$sp\)
			412: R_MEP_TPREL	symbol
 416:	fd c0 00 00 	lh \$0,0\(\$sp\)
			416: R_MEP_TPREL	symbol
 41a:	03 40       	lw \$0,0x0\(\$sp\)
			41a: R_MEP_TPREL7A4	symbol
 41c:	fb c0 00 00 	lbu \$0,0\(\$sp\)
			41c: R_MEP_TPREL	symbol
 420:	ff c0 00 00 	lhu \$0,0\(\$sp\)
			420: R_MEP_TPREL	symbol
 424:	f8 cf 00 00 	sb \$sp,0\(\$sp\)
			424: R_MEP_TPREL	symbol
 428:	f9 cf 00 00 	sh \$sp,0\(\$sp\)
			428: R_MEP_TPREL	symbol
 42c:	02 4f       	sw \$sp,0x0\(\$sp\)
			42c: R_MEP_TPREL7A4	symbol
 42e:	fc cf 00 00 	lb \$sp,0\(\$sp\)
			42e: R_MEP_TPREL	symbol
 432:	fd cf 00 00 	lh \$sp,0\(\$sp\)
			432: R_MEP_TPREL	symbol
 436:	03 4f       	lw \$sp,0x0\(\$sp\)
			436: R_MEP_TPREL7A4	symbol
 438:	fb cf 00 00 	lbu \$sp,0\(\$sp\)
			438: R_MEP_TPREL	symbol
 43c:	ff cf 00 00 	lhu \$sp,0\(\$sp\)
			43c: R_MEP_TPREL	symbol
 440:	02 e0 00 00 	sw \$0,\(0x0\)
 444:	03 e0 00 00 	lw \$0,\(0x0\)
 448:	02 ef 00 00 	sw \$sp,\(0x0\)
 44c:	03 ef 00 00 	lw \$sp,\(0x0\)
 450:	fe e0 ff ff 	sw \$0,\(0xfffffc\)
 454:	ff e0 ff ff 	lw \$0,\(0xfffffc\)
 458:	fe ef ff ff 	sw \$sp,\(0xfffffc\)
 45c:	ff ef ff ff 	lw \$sp,\(0xfffffc\)
 460:	02 e0 00 00 	sw \$0,\(0x0\)
			460: R_MEP_ADDR24A4	symbol
 464:	03 e0 00 00 	lw \$0,\(0x0\)
			464: R_MEP_ADDR24A4	symbol
 468:	02 ef 00 00 	sw \$sp,\(0x0\)
			468: R_MEP_ADDR24A4	symbol
 46c:	03 ef 00 00 	lw \$sp,\(0x0\)
			46c: R_MEP_ADDR24A4	symbol
 470:	0d 10       	extb \$0
 472:	8d 10       	extub \$0
 474:	2d 10       	exth \$0
 476:	ad 10       	extuh \$0
 478:	0d 1f       	extb \$sp
 47a:	8d 1f       	extub \$sp
 47c:	2d 1f       	exth \$sp
 47e:	ad 1f       	extuh \$sp
 480:	0c 10       	ssarb 0\(\$0\)
 482:	0c 13       	ssarb 3\(\$0\)
 484:	fc 10       	ssarb 0\(\$sp\)
 486:	fc 13       	ssarb 3\(\$sp\)
 488:	00 00       	nop
 48a:	00 0f       	mov \$sp,\$0
 48c:	f0 00       	mov \$0,\$sp
 48e:	f0 0f       	mov \$sp,\$sp
 490:	01 c0 00 80 	mov \$0,-32768
 494:	01 cf 00 80 	mov \$sp,-32768
 498:	80 50       	mov \$0,-128
 49a:	80 5f       	mov \$sp,-128
 49c:	00 50       	mov \$0,0
 49e:	00 5f       	mov \$sp,0
 4a0:	7f 50       	mov \$0,127
 4a2:	7f 5f       	mov \$sp,127
 4a4:	01 c0 ff 7f 	mov \$0,32767
 4a8:	01 cf ff 7f 	mov \$sp,32767
 4ac:	01 c0 00 00 	mov \$0,0
			4ac: R_MEP_LOW16	symbol
 4b0:	01 c0 00 00 	mov \$0,0
			4b0: R_MEP_HI16S	symbol
 4b4:	01 c0 00 00 	mov \$0,0
			4b4: R_MEP_HI16U	symbol
 4b8:	01 c0 00 00 	mov \$0,0
			4b8: R_MEP_GPREL	symbol
 4bc:	01 c0 00 00 	mov \$0,0
			4bc: R_MEP_TPREL	symbol
 4c0:	00 d0 00 00 	movu \$0,0x0
 4c4:	00 d7 00 00 	movu \$7,0x0
 4c8:	ff d0 ff ff 	movu \$0,0xffffff
 4cc:	ff d7 ff ff 	movu \$7,0xffffff
 4d0:	11 c0 00 00 	movu \$0,0x0
			4d0: R_MEP_LOW16	symbol
 4d4:	11 c7 00 00 	movu \$7,0x0
			4d4: R_MEP_LOW16	symbol
 4d8:	00 d0 00 00 	movu \$0,0x0
			4d8: R_MEP_UIMM24	symbol
 4dc:	00 d7 00 00 	movu \$7,0x0
			4dc: R_MEP_UIMM24	symbol
 4e0:	00 d0 00 00 	movu \$0,0x0
 4e4:	21 c0 00 00 	movh \$0,0x0
 4e8:	11 cf 00 00 	movu \$sp,0x0
 4ec:	21 cf 00 00 	movh \$sp,0x0
 4f0:	ff d0 ff 00 	movu \$0,0xffff
 4f4:	21 c0 ff ff 	movh \$0,0xffff
 4f8:	11 cf ff ff 	movu \$sp,0xffff
 4fc:	21 cf ff ff 	movh \$sp,0xffff
 500:	11 c0 00 00 	movu \$0,0x0
			500: R_MEP_LOW16	symbol
 504:	21 c0 00 00 	movh \$0,0x0
			504: R_MEP_LOW16	symbol
 508:	11 cf 00 00 	movu \$sp,0x0
			508: R_MEP_LOW16	symbol
 50c:	21 cf 00 00 	movh \$sp,0x0
			50c: R_MEP_LOW16	symbol
 510:	11 c0 00 00 	movu \$0,0x0
			510: R_MEP_HI16S	symbol
 514:	21 c0 00 00 	movh \$0,0x0
			514: R_MEP_HI16S	symbol
 518:	11 cf 00 00 	movu \$sp,0x0
			518: R_MEP_HI16S	symbol
 51c:	21 cf 00 00 	movh \$sp,0x0
			51c: R_MEP_HI16S	symbol
 520:	11 c0 00 00 	movu \$0,0x0
			520: R_MEP_HI16U	symbol
 524:	21 c0 00 00 	movh \$0,0x0
			524: R_MEP_HI16U	symbol
 528:	11 cf 00 00 	movu \$sp,0x0
			528: R_MEP_HI16U	symbol
 52c:	21 cf 00 00 	movh \$sp,0x0
			52c: R_MEP_HI16U	symbol
 530:	11 c0 78 56 	movu \$0,0x5678
 534:	21 c0 78 56 	movh \$0,0x5678
 538:	11 cf 78 56 	movu \$sp,0x5678
 53c:	21 cf 78 56 	movh \$sp,0x5678
 540:	11 c0 34 12 	movu \$0,0x1234
 544:	21 c0 34 12 	movh \$0,0x1234
 548:	11 cf 34 12 	movu \$sp,0x1234
 54c:	21 cf 34 12 	movh \$sp,0x1234
 550:	11 c0 34 12 	movu \$0,0x1234
 554:	21 c0 34 12 	movh \$0,0x1234
 558:	11 cf 34 12 	movu \$sp,0x1234
 55c:	21 cf 34 12 	movh \$sp,0x1234
 560:	00 90       	add3 \$0,\$0,\$0
 562:	0f 90       	add3 \$sp,\$0,\$0
 564:	00 9f       	add3 \$0,\$sp,\$0
 566:	0f 9f       	add3 \$sp,\$sp,\$0
 568:	f0 90       	add3 \$0,\$0,\$sp
 56a:	ff 90       	add3 \$sp,\$0,\$sp
 56c:	f0 9f       	add3 \$0,\$sp,\$sp
 56e:	ff 9f       	add3 \$sp,\$sp,\$sp
 570:	c0 60       	add \$0,-16
 572:	c0 6f       	add \$sp,-16
 574:	00 60       	add \$0,0
 576:	00 6f       	add \$sp,0
 578:	3c 60       	add \$0,15
 57a:	3c 6f       	add \$sp,15
 57c:	00 40       	add3 \$0,\$sp,0x0
 57e:	00 4f       	add3 \$sp,\$sp,0x0
 580:	7c 40       	add3 \$0,\$sp,0x7c
 582:	7c 4f       	add3 \$sp,\$sp,0x7c
 584:	f0 c0 01 00 	add3 \$0,\$sp,1
 588:	f0 cf 01 00 	add3 \$sp,\$sp,1
 58c:	07 00       	advck3 \$0,\$0,\$0
 58e:	05 00       	sbvck3 \$0,\$0,\$0
 590:	07 0f       	advck3 \$0,\$sp,\$0
 592:	05 0f       	sbvck3 \$0,\$sp,\$0
 594:	f7 00       	advck3 \$0,\$0,\$sp
 596:	f5 00       	sbvck3 \$0,\$0,\$sp
 598:	f7 0f       	advck3 \$0,\$sp,\$sp
 59a:	f5 0f       	sbvck3 \$0,\$sp,\$sp
 59c:	04 00       	sub \$0,\$0
 59e:	01 00       	neg \$0,\$0
 5a0:	04 0f       	sub \$sp,\$0
 5a2:	01 0f       	neg \$sp,\$0
 5a4:	f4 00       	sub \$0,\$sp
 5a6:	f1 00       	neg \$0,\$sp
 5a8:	f4 0f       	sub \$sp,\$sp
 5aa:	f1 0f       	neg \$sp,\$sp
 5ac:	02 00       	slt3 \$0,\$0,\$0
 5ae:	03 00       	sltu3 \$0,\$0,\$0
 5b0:	06 20       	sl1ad3 \$0,\$0,\$0
 5b2:	07 20       	sl2ad3 \$0,\$0,\$0
 5b4:	02 0f       	slt3 \$0,\$sp,\$0
 5b6:	03 0f       	sltu3 \$0,\$sp,\$0
 5b8:	06 2f       	sl1ad3 \$0,\$sp,\$0
 5ba:	07 2f       	sl2ad3 \$0,\$sp,\$0
 5bc:	f2 00       	slt3 \$0,\$0,\$sp
 5be:	f3 00       	sltu3 \$0,\$0,\$sp
 5c0:	f6 20       	sl1ad3 \$0,\$0,\$sp
 5c2:	f7 20       	sl2ad3 \$0,\$0,\$sp
 5c4:	f2 0f       	slt3 \$0,\$sp,\$sp
 5c6:	f3 0f       	sltu3 \$0,\$sp,\$sp
 5c8:	f6 2f       	sl1ad3 \$0,\$sp,\$sp
 5ca:	f7 2f       	sl2ad3 \$0,\$sp,\$sp
 5cc:	00 c0 00 80 	add3 \$0,\$0,-32768
 5d0:	00 cf 00 80 	add3 \$sp,\$0,-32768
 5d4:	f0 c0 00 80 	add3 \$0,\$sp,-32768
 5d8:	f0 cf 00 80 	add3 \$sp,\$sp,-32768
 5dc:	00 c0 ff 7f 	add3 \$0,\$0,32767
 5e0:	00 cf ff 7f 	add3 \$sp,\$0,32767
 5e4:	f0 c0 ff 7f 	add3 \$0,\$sp,32767
 5e8:	f0 cf ff 7f 	add3 \$sp,\$sp,32767
 5ec:	00 c0 00 00 	add3 \$0,\$0,0
			5ec: R_MEP_LOW16	symbol
 5f0:	00 cf 00 00 	add3 \$sp,\$0,0
			5f0: R_MEP_LOW16	symbol
 5f4:	f0 c0 00 00 	add3 \$0,\$sp,0
			5f4: R_MEP_LOW16	symbol
 5f8:	f0 cf 00 00 	add3 \$sp,\$sp,0
			5f8: R_MEP_LOW16	symbol
 5fc:	01 60       	slt3 \$0,\$0,0x0
 5fe:	05 60       	sltu3 \$0,\$0,0x0
 600:	01 6f       	slt3 \$0,\$sp,0x0
 602:	05 6f       	sltu3 \$0,\$sp,0x0
 604:	f9 60       	slt3 \$0,\$0,0x1f
 606:	fd 60       	sltu3 \$0,\$0,0x1f
 608:	f9 6f       	slt3 \$0,\$sp,0x1f
 60a:	fd 6f       	sltu3 \$0,\$sp,0x1f
 60c:	00 10       	or \$0,\$0
 60e:	01 10       	and \$0,\$0
 610:	02 10       	xor \$0,\$0
 612:	03 10       	nor \$0,\$0
 614:	00 1f       	or \$sp,\$0
 616:	01 1f       	and \$sp,\$0
 618:	02 1f       	xor \$sp,\$0
 61a:	03 1f       	nor \$sp,\$0
 61c:	f0 10       	or \$0,\$sp
 61e:	f1 10       	and \$0,\$sp
 620:	f2 10       	xor \$0,\$sp
 622:	f3 10       	nor \$0,\$sp
 624:	f0 1f       	or \$sp,\$sp
 626:	f1 1f       	and \$sp,\$sp
 628:	f2 1f       	xor \$sp,\$sp
 62a:	f3 1f       	nor \$sp,\$sp
 62c:	04 c0 00 00 	or3 \$0,\$0,0x0
 630:	05 c0 00 00 	and3 \$0,\$0,0x0
 634:	06 c0 00 00 	xor3 \$0,\$0,0x0
 638:	04 cf 00 00 	or3 \$sp,\$0,0x0
 63c:	05 cf 00 00 	and3 \$sp,\$0,0x0
 640:	06 cf 00 00 	xor3 \$sp,\$0,0x0
 644:	f4 c0 00 00 	or3 \$0,\$sp,0x0
 648:	f5 c0 00 00 	and3 \$0,\$sp,0x0
 64c:	f6 c0 00 00 	xor3 \$0,\$sp,0x0
 650:	f4 cf 00 00 	or3 \$sp,\$sp,0x0
 654:	f5 cf 00 00 	and3 \$sp,\$sp,0x0
 658:	f6 cf 00 00 	xor3 \$sp,\$sp,0x0
 65c:	04 c0 ff ff 	or3 \$0,\$0,0xffff
 660:	05 c0 ff ff 	and3 \$0,\$0,0xffff
 664:	06 c0 ff ff 	xor3 \$0,\$0,0xffff
 668:	04 cf ff ff 	or3 \$sp,\$0,0xffff
 66c:	05 cf ff ff 	and3 \$sp,\$0,0xffff
 670:	06 cf ff ff 	xor3 \$sp,\$0,0xffff
 674:	f4 c0 ff ff 	or3 \$0,\$sp,0xffff
 678:	f5 c0 ff ff 	and3 \$0,\$sp,0xffff
 67c:	f6 c0 ff ff 	xor3 \$0,\$sp,0xffff
 680:	f4 cf ff ff 	or3 \$sp,\$sp,0xffff
 684:	f5 cf ff ff 	and3 \$sp,\$sp,0xffff
 688:	f6 cf ff ff 	xor3 \$sp,\$sp,0xffff
 68c:	04 c0 00 00 	or3 \$0,\$0,0x0
			68c: R_MEP_LOW16	symbol
 690:	05 c0 00 00 	and3 \$0,\$0,0x0
			690: R_MEP_LOW16	symbol
 694:	06 c0 00 00 	xor3 \$0,\$0,0x0
			694: R_MEP_LOW16	symbol
 698:	04 cf 00 00 	or3 \$sp,\$0,0x0
			698: R_MEP_LOW16	symbol
 69c:	05 cf 00 00 	and3 \$sp,\$0,0x0
			69c: R_MEP_LOW16	symbol
 6a0:	06 cf 00 00 	xor3 \$sp,\$0,0x0
			6a0: R_MEP_LOW16	symbol
 6a4:	f4 c0 00 00 	or3 \$0,\$sp,0x0
			6a4: R_MEP_LOW16	symbol
 6a8:	f5 c0 00 00 	and3 \$0,\$sp,0x0
			6a8: R_MEP_LOW16	symbol
 6ac:	f6 c0 00 00 	xor3 \$0,\$sp,0x0
			6ac: R_MEP_LOW16	symbol
 6b0:	f4 cf 00 00 	or3 \$sp,\$sp,0x0
			6b0: R_MEP_LOW16	symbol
 6b4:	f5 cf 00 00 	and3 \$sp,\$sp,0x0
			6b4: R_MEP_LOW16	symbol
 6b8:	f6 cf 00 00 	xor3 \$sp,\$sp,0x0
			6b8: R_MEP_LOW16	symbol
 6bc:	0d 20       	sra \$0,\$0
 6be:	0c 20       	srl \$0,\$0
 6c0:	0e 20       	sll \$0,\$0
 6c2:	0f 20       	fsft \$0,\$0
 6c4:	0d 2f       	sra \$sp,\$0
 6c6:	0c 2f       	srl \$sp,\$0
 6c8:	0e 2f       	sll \$sp,\$0
 6ca:	0f 2f       	fsft \$sp,\$0
 6cc:	fd 20       	sra \$0,\$sp
 6ce:	fc 20       	srl \$0,\$sp
 6d0:	fe 20       	sll \$0,\$sp
 6d2:	ff 20       	fsft \$0,\$sp
 6d4:	fd 2f       	sra \$sp,\$sp
 6d6:	fc 2f       	srl \$sp,\$sp
 6d8:	fe 2f       	sll \$sp,\$sp
 6da:	ff 2f       	fsft \$sp,\$sp
 6dc:	03 60       	sra \$0,0x0
 6de:	02 60       	srl \$0,0x0
 6e0:	06 60       	sll \$0,0x0
 6e2:	03 6f       	sra \$sp,0x0
 6e4:	02 6f       	srl \$sp,0x0
 6e6:	06 6f       	sll \$sp,0x0
 6e8:	fb 60       	sra \$0,0x1f
 6ea:	fa 60       	srl \$0,0x1f
 6ec:	fe 60       	sll \$0,0x1f
 6ee:	fb 6f       	sra \$sp,0x1f
 6f0:	fa 6f       	srl \$sp,0x1f
 6f2:	fe 6f       	sll \$sp,0x1f
 6f4:	07 60       	sll3 \$0,\$0,0x0
 6f6:	07 6f       	sll3 \$0,\$sp,0x0
 6f8:	ff 60       	sll3 \$0,\$0,0x1f
 6fa:	ff 6f       	sll3 \$0,\$sp,0x1f
 6fc:	02 b8       	bra 0xfffffefe
 6fe:	01 e0 00 04 	beq \$0,\$0,0xefe
 702:	00 b0       	bra 0x702
			702: R_MEP_PCREL12A2	symbol
 704:	82 a0       	beqz \$0,0x686
 706:	83 a0       	bnez \$0,0x688
 708:	82 af       	beqz \$sp,0x68a
 70a:	83 af       	bnez \$sp,0x68c
 70c:	00 e0 40 00 	beqi \$0,0x0,0x78c
 710:	04 e0 40 00 	bnei \$0,0x0,0x790
 714:	00 ef 40 00 	beqi \$sp,0x0,0x794
 718:	04 ef 40 00 	bnei \$sp,0x0,0x798
 71c:	00 a0       	beqz \$0,0x71c
			71c: R_MEP_PCREL8A2	symbol
 71e:	01 a0       	bnez \$0,0x71e
			71e: R_MEP_PCREL8A2	symbol
 720:	00 af       	beqz \$sp,0x720
			720: R_MEP_PCREL8A2	symbol
 722:	01 af       	bnez \$sp,0x722
			722: R_MEP_PCREL8A2	symbol
 724:	00 e0 02 80 	beqi \$0,0x0,0xffff0728
 728:	04 e0 02 80 	bnei \$0,0x0,0xffff072c
 72c:	0c e0 02 80 	blti \$0,0x0,0xffff0730
 730:	08 e0 02 80 	bgei \$0,0x0,0xffff0734
 734:	00 ef 02 80 	beqi \$sp,0x0,0xffff0738
 738:	04 ef 02 80 	bnei \$sp,0x0,0xffff073c
 73c:	0c ef 02 80 	blti \$sp,0x0,0xffff0740
 740:	08 ef 02 80 	bgei \$sp,0x0,0xffff0744
 744:	f0 e0 02 80 	beqi \$0,0xf,0xffff0748
 748:	f4 e0 02 80 	bnei \$0,0xf,0xffff074c
 74c:	fc e0 02 80 	blti \$0,0xf,0xffff0750
 750:	f8 e0 02 80 	bgei \$0,0xf,0xffff0754
 754:	f0 ef 02 80 	beqi \$sp,0xf,0xffff0758
 758:	f4 ef 02 80 	bnei \$sp,0xf,0xffff075c
 75c:	fc ef 02 80 	blti \$sp,0xf,0xffff0760
 760:	f8 ef 02 80 	bgei \$sp,0xf,0xffff0764
 764:	00 e0 ff 3f 	beqi \$0,0x0,0x8762
 768:	04 e0 ff 3f 	bnei \$0,0x0,0x8766
 76c:	0c e0 ff 3f 	blti \$0,0x0,0x876a
 770:	08 e0 ff 3f 	bgei \$0,0x0,0x876e
 774:	00 ef ff 3f 	beqi \$sp,0x0,0x8772
 778:	04 ef ff 3f 	bnei \$sp,0x0,0x8776
 77c:	0c ef ff 3f 	blti \$sp,0x0,0x877a
 780:	08 ef ff 3f 	bgei \$sp,0x0,0x877e
 784:	f0 e0 ff 3f 	beqi \$0,0xf,0x8782
 788:	f4 e0 ff 3f 	bnei \$0,0xf,0x8786
 78c:	fc e0 ff 3f 	blti \$0,0xf,0x878a
 790:	f8 e0 ff 3f 	bgei \$0,0xf,0x878e
 794:	f0 ef ff 3f 	beqi \$sp,0xf,0x8792
 798:	f4 ef ff 3f 	bnei \$sp,0xf,0x8796
 79c:	fc ef ff 3f 	blti \$sp,0xf,0x879a
 7a0:	f8 ef ff 3f 	bgei \$sp,0xf,0x879e
 7a4:	00 e0 00 00 	beqi \$0,0x0,0x7a4
			7a4: R_MEP_PCREL17A2	symbol
 7a8:	04 e0 00 00 	bnei \$0,0x0,0x7a8
			7a8: R_MEP_PCREL17A2	symbol
 7ac:	0c e0 00 00 	blti \$0,0x0,0x7ac
			7ac: R_MEP_PCREL17A2	symbol
 7b0:	08 e0 00 00 	bgei \$0,0x0,0x7b0
			7b0: R_MEP_PCREL17A2	symbol
 7b4:	00 ef 00 00 	beqi \$sp,0x0,0x7b4
			7b4: R_MEP_PCREL17A2	symbol
 7b8:	04 ef 00 00 	bnei \$sp,0x0,0x7b8
			7b8: R_MEP_PCREL17A2	symbol
 7bc:	0c ef 00 00 	blti \$sp,0x0,0x7bc
			7bc: R_MEP_PCREL17A2	symbol
 7c0:	08 ef 00 00 	bgei \$sp,0x0,0x7c0
			7c0: R_MEP_PCREL17A2	symbol
 7c4:	f0 e0 00 00 	beqi \$0,0xf,0x7c4
			7c4: R_MEP_PCREL17A2	symbol
 7c8:	f4 e0 00 00 	bnei \$0,0xf,0x7c8
			7c8: R_MEP_PCREL17A2	symbol
 7cc:	fc e0 00 00 	blti \$0,0xf,0x7cc
			7cc: R_MEP_PCREL17A2	symbol
 7d0:	f8 e0 00 00 	bgei \$0,0xf,0x7d0
			7d0: R_MEP_PCREL17A2	symbol
 7d4:	f0 ef 00 00 	beqi \$sp,0xf,0x7d4
			7d4: R_MEP_PCREL17A2	symbol
 7d8:	f4 ef 00 00 	bnei \$sp,0xf,0x7d8
			7d8: R_MEP_PCREL17A2	symbol
 7dc:	fc ef 00 00 	blti \$sp,0xf,0x7dc
			7dc: R_MEP_PCREL17A2	symbol
 7e0:	f8 ef 00 00 	bgei \$sp,0xf,0x7e0
			7e0: R_MEP_PCREL17A2	symbol
 7e4:	01 e0 02 80 	beq \$0,\$0,0xffff07e8
 7e8:	05 e0 02 80 	bne \$0,\$0,0xffff07ec
 7ec:	01 ef 02 80 	beq \$sp,\$0,0xffff07f0
 7f0:	05 ef 02 80 	bne \$sp,\$0,0xffff07f4
 7f4:	f1 e0 02 80 	beq \$0,\$sp,0xffff07f8
 7f8:	f5 e0 02 80 	bne \$0,\$sp,0xffff07fc
 7fc:	f1 ef 02 80 	beq \$sp,\$sp,0xffff0800
 800:	f5 ef 02 80 	bne \$sp,\$sp,0xffff0804
 804:	01 e0 ff 3f 	beq \$0,\$0,0x8802
 808:	05 e0 ff 3f 	bne \$0,\$0,0x8806
 80c:	01 ef ff 3f 	beq \$sp,\$0,0x880a
 810:	05 ef ff 3f 	bne \$sp,\$0,0x880e
 814:	f1 e0 ff 3f 	beq \$0,\$sp,0x8812
 818:	f5 e0 ff 3f 	bne \$0,\$sp,0x8816
 81c:	f1 ef ff 3f 	beq \$sp,\$sp,0x881a
 820:	f5 ef ff 3f 	bne \$sp,\$sp,0x881e
 824:	01 e0 00 00 	beq \$0,\$0,0x824
			824: R_MEP_PCREL17A2	symbol
 828:	05 e0 00 00 	bne \$0,\$0,0x828
			828: R_MEP_PCREL17A2	symbol
 82c:	01 ef 00 00 	beq \$sp,\$0,0x82c
			82c: R_MEP_PCREL17A2	symbol
 830:	05 ef 00 00 	bne \$sp,\$0,0x830
			830: R_MEP_PCREL17A2	symbol
 834:	f1 e0 00 00 	beq \$0,\$sp,0x834
			834: R_MEP_PCREL17A2	symbol
 838:	f5 e0 00 00 	bne \$0,\$sp,0x838
			838: R_MEP_PCREL17A2	symbol
 83c:	f1 ef 00 00 	beq \$sp,\$sp,0x83c
			83c: R_MEP_PCREL17A2	symbol
 840:	f5 ef 00 00 	bne \$sp,\$sp,0x840
			840: R_MEP_PCREL17A2	symbol
 844:	29 d8 00 80 	bsr 0xff800848
 848:	03 b8       	bsr 0x4a
 84a:	09 d8 08 00 	bsr 0x104a
 84e:	19 d8 00 80 	bsr 0xff800850
 852:	09 d8 00 00 	bsr 0x852
			852: R_MEP_PCREL24A2	symbol
 856:	0e 10       	jmp \$0
 858:	fe 10       	jmp \$sp
 85a:	08 d8 00 00 	jmp 0x0
 85e:	f8 df ff ff 	jmp 0xfffffe
 862:	08 d8 00 00 	jmp 0x0
			862: R_MEP_PCABS24A2	symbol
 866:	0f 10       	jsr \$0
 868:	ff 10       	jsr \$sp
 86a:	02 70       	ret
 86c:	09 e0 02 80 	repeat \$0,0xffff0870
 870:	09 ef 02 80 	repeat \$sp,0xffff0874
 874:	09 e0 ff 3f 	repeat \$0,0x8872
 878:	09 ef ff 3f 	repeat \$sp,0x8876
 87c:	09 e0 00 00 	repeat \$0,0x87c
			87c: R_MEP_PCREL17A2	symbol
 880:	09 ef 00 00 	repeat \$sp,0x880
			880: R_MEP_PCREL17A2	symbol
 884:	19 e0 02 80 	erepeat 0xffff0888
 888:	19 e0 ff 3f 	erepeat 0x8886
 88c:	19 e0 00 00 	erepeat 0x88c
			88c: R_MEP_PCREL17A2	symbol
 890:	08 70       	stc \$0,\$pc
 892:	0a 70       	ldc \$0,\$pc
 894:	08 7f       	stc \$sp,\$pc
 896:	0a 7f       	ldc \$sp,\$pc
 898:	18 70       	stc \$0,\$lp
 89a:	1a 70       	ldc \$0,\$lp
 89c:	18 7f       	stc \$sp,\$lp
 89e:	1a 7f       	ldc \$sp,\$lp
 8a0:	28 70       	stc \$0,\$sar
 8a2:	2a 70       	ldc \$0,\$sar
 8a4:	28 7f       	stc \$sp,\$sar
 8a6:	2a 7f       	ldc \$sp,\$sar
 8a8:	48 70       	stc \$0,\$rpb
 8aa:	4a 70       	ldc \$0,\$rpb
 8ac:	48 7f       	stc \$sp,\$rpb
 8ae:	4a 7f       	ldc \$sp,\$rpb
 8b0:	58 70       	stc \$0,\$rpe
 8b2:	5a 70       	ldc \$0,\$rpe
 8b4:	58 7f       	stc \$sp,\$rpe
 8b6:	5a 7f       	ldc \$sp,\$rpe
 8b8:	68 70       	stc \$0,\$rpc
 8ba:	6a 70       	ldc \$0,\$rpc
 8bc:	68 7f       	stc \$sp,\$rpc
 8be:	6a 7f       	ldc \$sp,\$rpc
 8c0:	78 70       	stc \$0,\$hi
 8c2:	7a 70       	ldc \$0,\$hi
 8c4:	78 7f       	stc \$sp,\$hi
 8c6:	7a 7f       	ldc \$sp,\$hi
 8c8:	88 70       	stc \$0,\$lo
 8ca:	8a 70       	ldc \$0,\$lo
 8cc:	88 7f       	stc \$sp,\$lo
 8ce:	8a 7f       	ldc \$sp,\$lo
 8d0:	c8 70       	stc \$0,\$mb0
 8d2:	ca 70       	ldc \$0,\$mb0
 8d4:	c8 7f       	stc \$sp,\$mb0
 8d6:	ca 7f       	ldc \$sp,\$mb0
 8d8:	d8 70       	stc \$0,\$me0
 8da:	da 70       	ldc \$0,\$me0
 8dc:	d8 7f       	stc \$sp,\$me0
 8de:	da 7f       	ldc \$sp,\$me0
 8e0:	e8 70       	stc \$0,\$mb1
 8e2:	ea 70       	ldc \$0,\$mb1
 8e4:	e8 7f       	stc \$sp,\$mb1
 8e6:	ea 7f       	ldc \$sp,\$mb1
 8e8:	f8 70       	stc \$0,\$me1
 8ea:	fa 70       	ldc \$0,\$me1
 8ec:	f8 7f       	stc \$sp,\$me1
 8ee:	fa 7f       	ldc \$sp,\$me1
 8f0:	09 70       	stc \$0,\$psw
 8f2:	0b 70       	ldc \$0,\$psw
 8f4:	09 7f       	stc \$sp,\$psw
 8f6:	0b 7f       	ldc \$sp,\$psw
 8f8:	19 70       	stc \$0,\$id
 8fa:	1b 70       	ldc \$0,\$id
 8fc:	19 7f       	stc \$sp,\$id
 8fe:	1b 7f       	ldc \$sp,\$id
 900:	29 70       	stc \$0,\$tmp
 902:	2b 70       	ldc \$0,\$tmp
 904:	29 7f       	stc \$sp,\$tmp
 906:	2b 7f       	ldc \$sp,\$tmp
 908:	39 70       	stc \$0,\$epc
 90a:	3b 70       	ldc \$0,\$epc
 90c:	39 7f       	stc \$sp,\$epc
 90e:	3b 7f       	ldc \$sp,\$epc
 910:	49 70       	stc \$0,\$exc
 912:	4b 70       	ldc \$0,\$exc
 914:	49 7f       	stc \$sp,\$exc
 916:	4b 7f       	ldc \$sp,\$exc
 918:	59 70       	stc \$0,\$cfg
 91a:	5b 70       	ldc \$0,\$cfg
 91c:	59 7f       	stc \$sp,\$cfg
 91e:	5b 7f       	ldc \$sp,\$cfg
 920:	79 70       	stc \$0,\$npc
 922:	7b 70       	ldc \$0,\$npc
 924:	79 7f       	stc \$sp,\$npc
 926:	7b 7f       	ldc \$sp,\$npc
 928:	89 70       	stc \$0,\$dbg
 92a:	8b 70       	ldc \$0,\$dbg
 92c:	89 7f       	stc \$sp,\$dbg
 92e:	8b 7f       	ldc \$sp,\$dbg
 930:	99 70       	stc \$0,\$depc
 932:	9b 70       	ldc \$0,\$depc
 934:	99 7f       	stc \$sp,\$depc
 936:	9b 7f       	ldc \$sp,\$depc
 938:	a9 70       	stc \$0,\$opt
 93a:	ab 70       	ldc \$0,\$opt
 93c:	a9 7f       	stc \$sp,\$opt
 93e:	ab 7f       	ldc \$sp,\$opt
 940:	b9 70       	stc \$0,\$rcfg
 942:	bb 70       	ldc \$0,\$rcfg
 944:	b9 7f       	stc \$sp,\$rcfg
 946:	bb 7f       	ldc \$sp,\$rcfg
 948:	c9 70       	stc \$0,\$ccfg
 94a:	cb 70       	ldc \$0,\$ccfg
 94c:	c9 7f       	stc \$sp,\$ccfg
 94e:	cb 7f       	ldc \$sp,\$ccfg
 950:	00 70       	di
 952:	10 70       	ei
 954:	12 70       	reti
 956:	22 70       	halt
 958:	32 70       	break
 95a:	11 70       	syncm
 95c:	06 70       	swi 0x0
 95e:	36 70       	swi 0x3
 960:	04 f0 00 00 	stcb \$0,0x0
 964:	14 f0 00 00 	ldcb \$0,0x0
 968:	04 ff 00 00 	stcb \$sp,0x0
 96c:	14 ff 00 00 	ldcb \$sp,0x0
 970:	04 f0 ff ff 	stcb \$0,0xffff
 974:	14 f0 ff ff 	ldcb \$0,0xffff
 978:	04 ff ff ff 	stcb \$sp,0xffff
 97c:	14 ff ff ff 	ldcb \$sp,0xffff
 980:	04 f0 00 00 	stcb \$0,0x0
			982: R_MEP_16	symbol
 984:	14 f0 00 00 	ldcb \$0,0x0
			986: R_MEP_16	symbol
 988:	04 ff 00 00 	stcb \$sp,0x0
			98a: R_MEP_16	symbol
 98c:	14 ff 00 00 	ldcb \$sp,0x0
			98e: R_MEP_16	symbol
 990:	00 20       	bsetm \(\$0\),0x0
 992:	01 20       	bclrm \(\$0\),0x0
 994:	02 20       	bnotm \(\$0\),0x0
 996:	f0 20       	bsetm \(\$sp\),0x0
 998:	f1 20       	bclrm \(\$sp\),0x0
 99a:	f2 20       	bnotm \(\$sp\),0x0
 99c:	00 27       	bsetm \(\$0\),0x7
 99e:	01 27       	bclrm \(\$0\),0x7
 9a0:	02 27       	bnotm \(\$0\),0x7
 9a2:	f0 27       	bsetm \(\$sp\),0x7
 9a4:	f1 27       	bclrm \(\$sp\),0x7
 9a6:	f2 27       	bnotm \(\$sp\),0x7
 9a8:	03 20       	btstm \$0,\(\$0\),0x0
 9aa:	f3 20       	btstm \$0,\(\$sp\),0x0
 9ac:	03 27       	btstm \$0,\(\$0\),0x7
 9ae:	f3 27       	btstm \$0,\(\$sp\),0x7
 9b0:	04 20       	tas \$0,\(\$0\)
 9b2:	04 2f       	tas \$sp,\(\$0\)
 9b4:	f4 20       	tas \$0,\(\$sp\)
 9b6:	f4 2f       	tas \$sp,\(\$sp\)
 9b8:	04 70       	cache 0x0,\(\$0\)
 9ba:	04 73       	cache 0x3,\(\$0\)
 9bc:	f4 70       	cache 0x0,\(\$sp\)
 9be:	f4 73       	cache 0x3,\(\$sp\)
 9c0:	04 10       	mul \$0,\$0
 9c2:	01 f0 04 30 	madd \$0,\$0
 9c6:	06 10       	mulr \$0,\$0
 9c8:	01 f0 06 30 	maddr \$0,\$0
 9cc:	05 10       	mulu \$0,\$0
 9ce:	01 f0 05 30 	maddu \$0,\$0
 9d2:	07 10       	mulru \$0,\$0
 9d4:	01 f0 07 30 	maddru \$0,\$0
 9d8:	04 1f       	mul \$sp,\$0
 9da:	01 ff 04 30 	madd \$sp,\$0
 9de:	06 1f       	mulr \$sp,\$0
 9e0:	01 ff 06 30 	maddr \$sp,\$0
 9e4:	05 1f       	mulu \$sp,\$0
 9e6:	01 ff 05 30 	maddu \$sp,\$0
 9ea:	07 1f       	mulru \$sp,\$0
 9ec:	01 ff 07 30 	maddru \$sp,\$0
 9f0:	f4 10       	mul \$0,\$sp
 9f2:	f1 f0 04 30 	madd \$0,\$sp
 9f6:	f6 10       	mulr \$0,\$sp
 9f8:	f1 f0 06 30 	maddr \$0,\$sp
 9fc:	f5 10       	mulu \$0,\$sp
 9fe:	f1 f0 05 30 	maddu \$0,\$sp
 a02:	f7 10       	mulru \$0,\$sp
 a04:	f1 f0 07 30 	maddru \$0,\$sp
 a08:	f4 1f       	mul \$sp,\$sp
 a0a:	f1 ff 04 30 	madd \$sp,\$sp
 a0e:	f6 1f       	mulr \$sp,\$sp
 a10:	f1 ff 06 30 	maddr \$sp,\$sp
 a14:	f5 1f       	mulu \$sp,\$sp
 a16:	f1 ff 05 30 	maddu \$sp,\$sp
 a1a:	f7 1f       	mulru \$sp,\$sp
 a1c:	f1 ff 07 30 	maddru \$sp,\$sp
 a20:	08 10       	div \$0,\$0
 a22:	09 10       	divu \$0,\$0
 a24:	08 1f       	div \$sp,\$0
 a26:	09 1f       	divu \$sp,\$0
 a28:	f8 10       	div \$0,\$sp
 a2a:	f9 10       	divu \$0,\$sp
 a2c:	f8 1f       	div \$sp,\$sp
 a2e:	f9 1f       	divu \$sp,\$sp
 a30:	13 70       	dret
 a32:	33 70       	dbreak
 a34:	01 f0 00 00 	ldz \$0,\$0
 a38:	01 f0 03 00 	abs \$0,\$0
 a3c:	01 f0 02 00 	ave \$0,\$0
 a40:	01 ff 00 00 	ldz \$sp,\$0
 a44:	01 ff 03 00 	abs \$sp,\$0
 a48:	01 ff 02 00 	ave \$sp,\$0
 a4c:	f1 f0 00 00 	ldz \$0,\$sp
 a50:	f1 f0 03 00 	abs \$0,\$sp
 a54:	f1 f0 02 00 	ave \$0,\$sp
 a58:	f1 ff 00 00 	ldz \$sp,\$sp
 a5c:	f1 ff 03 00 	abs \$sp,\$sp
 a60:	f1 ff 02 00 	ave \$sp,\$sp
 a64:	01 f0 04 00 	min \$0,\$0
 a68:	01 f0 05 00 	max \$0,\$0
 a6c:	01 f0 06 00 	minu \$0,\$0
 a70:	01 f0 07 00 	maxu \$0,\$0
 a74:	01 ff 04 00 	min \$sp,\$0
 a78:	01 ff 05 00 	max \$sp,\$0
 a7c:	01 ff 06 00 	minu \$sp,\$0
 a80:	01 ff 07 00 	maxu \$sp,\$0
 a84:	f1 f0 04 00 	min \$0,\$sp
 a88:	f1 f0 05 00 	max \$0,\$sp
 a8c:	f1 f0 06 00 	minu \$0,\$sp
 a90:	f1 f0 07 00 	maxu \$0,\$sp
 a94:	f1 ff 04 00 	min \$sp,\$sp
 a98:	f1 ff 05 00 	max \$sp,\$sp
 a9c:	f1 ff 06 00 	minu \$sp,\$sp
 aa0:	f1 ff 07 00 	maxu \$sp,\$sp
 aa4:	01 f0 00 10 	clip \$0,0x0
 aa8:	01 f0 01 10 	clipu \$0,0x0
 aac:	01 ff 00 10 	clip \$sp,0x0
 ab0:	01 ff 01 10 	clipu \$sp,0x0
 ab4:	01 f0 f8 10 	clip \$0,0x1f
 ab8:	01 f0 f9 10 	clipu \$0,0x1f
 abc:	01 ff f8 10 	clip \$sp,0x1f
 ac0:	01 ff f9 10 	clipu \$sp,0x1f
 ac4:	01 f0 08 00 	sadd \$0,\$0
 ac8:	01 f0 0a 00 	ssub \$0,\$0
 acc:	01 f0 09 00 	saddu \$0,\$0
 ad0:	01 f0 0b 00 	ssubu \$0,\$0
 ad4:	01 ff 08 00 	sadd \$sp,\$0
 ad8:	01 ff 0a 00 	ssub \$sp,\$0
 adc:	01 ff 09 00 	saddu \$sp,\$0
 ae0:	01 ff 0b 00 	ssubu \$sp,\$0
 ae4:	f1 f0 08 00 	sadd \$0,\$sp
 ae8:	f1 f0 0a 00 	ssub \$0,\$sp
 aec:	f1 f0 09 00 	saddu \$0,\$sp
 af0:	f1 f0 0b 00 	ssubu \$0,\$sp
 af4:	f1 ff 08 00 	sadd \$sp,\$sp
 af8:	f1 ff 0a 00 	ssub \$sp,\$sp
 afc:	f1 ff 09 00 	saddu \$sp,\$sp
 b00:	f1 ff 0b 00 	ssubu \$sp,\$sp
 b04:	08 30       	swcp \$c0,\(\$0\)
 b06:	09 30       	lwcp \$c0,\(\$0\)
 b08:	0a 30       	smcp \$c0,\(\$0\)
 b0a:	0b 30       	lmcp \$c0,\(\$0\)
 b0c:	08 3f       	swcp \$c15,\(\$0\)
 b0e:	09 3f       	lwcp \$c15,\(\$0\)
 b10:	0a 3f       	smcp \$c15,\(\$0\)
 b12:	0b 3f       	lmcp \$c15,\(\$0\)
 b14:	f8 30       	swcp \$c0,\(\$sp\)
 b16:	f9 30       	lwcp \$c0,\(\$sp\)
 b18:	fa 30       	smcp \$c0,\(\$sp\)
 b1a:	fb 30       	lmcp \$c0,\(\$sp\)
 b1c:	f8 3f       	swcp \$c15,\(\$sp\)
 b1e:	f9 3f       	lwcp \$c15,\(\$sp\)
 b20:	fa 3f       	smcp \$c15,\(\$sp\)
 b22:	fb 3f       	lmcp \$c15,\(\$sp\)
 b24:	00 30       	swcpi \$c0,\(\$0\+\)
 b26:	01 30       	lwcpi \$c0,\(\$0\+\)
 b28:	02 30       	smcpi \$c0,\(\$0\+\)
 b2a:	03 30       	lmcpi \$c0,\(\$0\+\)
 b2c:	00 3f       	swcpi \$c15,\(\$0\+\)
 b2e:	01 3f       	lwcpi \$c15,\(\$0\+\)
 b30:	02 3f       	smcpi \$c15,\(\$0\+\)
 b32:	03 3f       	lmcpi \$c15,\(\$0\+\)
 b34:	f0 30       	swcpi \$c0,\(\$sp\+\)
 b36:	f1 30       	lwcpi \$c0,\(\$sp\+\)
 b38:	f2 30       	smcpi \$c0,\(\$sp\+\)
 b3a:	f3 30       	lmcpi \$c0,\(\$sp\+\)
 b3c:	f0 3f       	swcpi \$c15,\(\$sp\+\)
 b3e:	f1 3f       	lwcpi \$c15,\(\$sp\+\)
 b40:	f2 3f       	smcpi \$c15,\(\$sp\+\)
 b42:	f3 3f       	lmcpi \$c15,\(\$sp\+\)
 b44:	05 f0 80 00 	sbcpa \$c0,\(\$0\+\),-128
 b48:	05 f0 80 40 	lbcpa \$c0,\(\$0\+\),-128
 b4c:	05 f0 80 08 	sbcpm0 \$c0,\(\$0\+\),-128
 b50:	05 f0 80 48 	lbcpm0 \$c0,\(\$0\+\),-128
 b54:	05 f0 80 0c 	sbcpm1 \$c0,\(\$0\+\),-128
 b58:	05 f0 80 4c 	lbcpm1 \$c0,\(\$0\+\),-128
 b5c:	05 ff 80 00 	sbcpa \$c15,\(\$0\+\),-128
 b60:	05 ff 80 40 	lbcpa \$c15,\(\$0\+\),-128
 b64:	05 ff 80 08 	sbcpm0 \$c15,\(\$0\+\),-128
 b68:	05 ff 80 48 	lbcpm0 \$c15,\(\$0\+\),-128
 b6c:	05 ff 80 0c 	sbcpm1 \$c15,\(\$0\+\),-128
 b70:	05 ff 80 4c 	lbcpm1 \$c15,\(\$0\+\),-128
 b74:	f5 f0 80 00 	sbcpa \$c0,\(\$sp\+\),-128
 b78:	f5 f0 80 40 	lbcpa \$c0,\(\$sp\+\),-128
 b7c:	f5 f0 80 08 	sbcpm0 \$c0,\(\$sp\+\),-128
 b80:	f5 f0 80 48 	lbcpm0 \$c0,\(\$sp\+\),-128
 b84:	f5 f0 80 0c 	sbcpm1 \$c0,\(\$sp\+\),-128
 b88:	f5 f0 80 4c 	lbcpm1 \$c0,\(\$sp\+\),-128
 b8c:	f5 ff 80 00 	sbcpa \$c15,\(\$sp\+\),-128
 b90:	f5 ff 80 40 	lbcpa \$c15,\(\$sp\+\),-128
 b94:	f5 ff 80 08 	sbcpm0 \$c15,\(\$sp\+\),-128
 b98:	f5 ff 80 48 	lbcpm0 \$c15,\(\$sp\+\),-128
 b9c:	f5 ff 80 0c 	sbcpm1 \$c15,\(\$sp\+\),-128
 ba0:	f5 ff 80 4c 	lbcpm1 \$c15,\(\$sp\+\),-128
 ba4:	05 f0 7f 00 	sbcpa \$c0,\(\$0\+\),127
 ba8:	05 f0 7f 40 	lbcpa \$c0,\(\$0\+\),127
 bac:	05 f0 7f 08 	sbcpm0 \$c0,\(\$0\+\),127
 bb0:	05 f0 7f 48 	lbcpm0 \$c0,\(\$0\+\),127
 bb4:	05 f0 7f 0c 	sbcpm1 \$c0,\(\$0\+\),127
 bb8:	05 f0 7f 4c 	lbcpm1 \$c0,\(\$0\+\),127
 bbc:	05 ff 7f 00 	sbcpa \$c15,\(\$0\+\),127
 bc0:	05 ff 7f 40 	lbcpa \$c15,\(\$0\+\),127
 bc4:	05 ff 7f 08 	sbcpm0 \$c15,\(\$0\+\),127
 bc8:	05 ff 7f 48 	lbcpm0 \$c15,\(\$0\+\),127
 bcc:	05 ff 7f 0c 	sbcpm1 \$c15,\(\$0\+\),127
 bd0:	05 ff 7f 4c 	lbcpm1 \$c15,\(\$0\+\),127
 bd4:	f5 f0 7f 00 	sbcpa \$c0,\(\$sp\+\),127
 bd8:	f5 f0 7f 40 	lbcpa \$c0,\(\$sp\+\),127
 bdc:	f5 f0 7f 08 	sbcpm0 \$c0,\(\$sp\+\),127
 be0:	f5 f0 7f 48 	lbcpm0 \$c0,\(\$sp\+\),127
 be4:	f5 f0 7f 0c 	sbcpm1 \$c0,\(\$sp\+\),127
 be8:	f5 f0 7f 4c 	lbcpm1 \$c0,\(\$sp\+\),127
 bec:	f5 ff 7f 00 	sbcpa \$c15,\(\$sp\+\),127
 bf0:	f5 ff 7f 40 	lbcpa \$c15,\(\$sp\+\),127
 bf4:	f5 ff 7f 08 	sbcpm0 \$c15,\(\$sp\+\),127
 bf8:	f5 ff 7f 48 	lbcpm0 \$c15,\(\$sp\+\),127
 bfc:	f5 ff 7f 0c 	sbcpm1 \$c15,\(\$sp\+\),127
 c00:	f5 ff 7f 4c 	lbcpm1 \$c15,\(\$sp\+\),127
 c04:	05 f0 80 10 	shcpa \$c0,\(\$0\+\),-128
 c08:	05 f0 80 50 	lhcpa \$c0,\(\$0\+\),-128
 c0c:	05 f0 80 18 	shcpm0 \$c0,\(\$0\+\),-128
 c10:	05 f0 80 58 	lhcpm0 \$c0,\(\$0\+\),-128
 c14:	05 f0 80 1c 	shcpm1 \$c0,\(\$0\+\),-128
 c18:	05 f0 80 5c 	lhcpm1 \$c0,\(\$0\+\),-128
 c1c:	05 ff 80 10 	shcpa \$c15,\(\$0\+\),-128
 c20:	05 ff 80 50 	lhcpa \$c15,\(\$0\+\),-128
 c24:	05 ff 80 18 	shcpm0 \$c15,\(\$0\+\),-128
 c28:	05 ff 80 58 	lhcpm0 \$c15,\(\$0\+\),-128
 c2c:	05 ff 80 1c 	shcpm1 \$c15,\(\$0\+\),-128
 c30:	05 ff 80 5c 	lhcpm1 \$c15,\(\$0\+\),-128
 c34:	f5 f0 80 10 	shcpa \$c0,\(\$sp\+\),-128
 c38:	f5 f0 80 50 	lhcpa \$c0,\(\$sp\+\),-128
 c3c:	f5 f0 80 18 	shcpm0 \$c0,\(\$sp\+\),-128
 c40:	f5 f0 80 58 	lhcpm0 \$c0,\(\$sp\+\),-128
 c44:	f5 f0 80 1c 	shcpm1 \$c0,\(\$sp\+\),-128
 c48:	f5 f0 80 5c 	lhcpm1 \$c0,\(\$sp\+\),-128
 c4c:	f5 ff 80 10 	shcpa \$c15,\(\$sp\+\),-128
 c50:	f5 ff 80 50 	lhcpa \$c15,\(\$sp\+\),-128
 c54:	f5 ff 80 18 	shcpm0 \$c15,\(\$sp\+\),-128
 c58:	f5 ff 80 58 	lhcpm0 \$c15,\(\$sp\+\),-128
 c5c:	f5 ff 80 1c 	shcpm1 \$c15,\(\$sp\+\),-128
 c60:	f5 ff 80 5c 	lhcpm1 \$c15,\(\$sp\+\),-128
 c64:	05 f0 7e 10 	shcpa \$c0,\(\$0\+\),126
 c68:	05 f0 7e 50 	lhcpa \$c0,\(\$0\+\),126
 c6c:	05 f0 7e 18 	shcpm0 \$c0,\(\$0\+\),126
 c70:	05 f0 7e 58 	lhcpm0 \$c0,\(\$0\+\),126
 c74:	05 f0 7e 1c 	shcpm1 \$c0,\(\$0\+\),126
 c78:	05 f0 7e 5c 	lhcpm1 \$c0,\(\$0\+\),126
 c7c:	05 ff 7e 10 	shcpa \$c15,\(\$0\+\),126
 c80:	05 ff 7e 50 	lhcpa \$c15,\(\$0\+\),126
 c84:	05 ff 7e 18 	shcpm0 \$c15,\(\$0\+\),126
 c88:	05 ff 7e 58 	lhcpm0 \$c15,\(\$0\+\),126
 c8c:	05 ff 7e 1c 	shcpm1 \$c15,\(\$0\+\),126
 c90:	05 ff 7e 5c 	lhcpm1 \$c15,\(\$0\+\),126
 c94:	f5 f0 7e 10 	shcpa \$c0,\(\$sp\+\),126
 c98:	f5 f0 7e 50 	lhcpa \$c0,\(\$sp\+\),126
 c9c:	f5 f0 7e 18 	shcpm0 \$c0,\(\$sp\+\),126
 ca0:	f5 f0 7e 58 	lhcpm0 \$c0,\(\$sp\+\),126
 ca4:	f5 f0 7e 1c 	shcpm1 \$c0,\(\$sp\+\),126
 ca8:	f5 f0 7e 5c 	lhcpm1 \$c0,\(\$sp\+\),126
 cac:	f5 ff 7e 10 	shcpa \$c15,\(\$sp\+\),126
 cb0:	f5 ff 7e 50 	lhcpa \$c15,\(\$sp\+\),126
 cb4:	f5 ff 7e 18 	shcpm0 \$c15,\(\$sp\+\),126
 cb8:	f5 ff 7e 58 	lhcpm0 \$c15,\(\$sp\+\),126
 cbc:	f5 ff 7e 1c 	shcpm1 \$c15,\(\$sp\+\),126
 cc0:	f5 ff 7e 5c 	lhcpm1 \$c15,\(\$sp\+\),126
 cc4:	05 f0 80 20 	swcpa \$c0,\(\$0\+\),-128
 cc8:	05 f0 80 60 	lwcpa \$c0,\(\$0\+\),-128
 ccc:	05 f0 80 28 	swcpm0 \$c0,\(\$0\+\),-128
 cd0:	05 f0 80 68 	lwcpm0 \$c0,\(\$0\+\),-128
 cd4:	05 f0 80 2c 	swcpm1 \$c0,\(\$0\+\),-128
 cd8:	05 f0 80 6c 	lwcpm1 \$c0,\(\$0\+\),-128
 cdc:	05 ff 80 20 	swcpa \$c15,\(\$0\+\),-128
 ce0:	05 ff 80 60 	lwcpa \$c15,\(\$0\+\),-128
 ce4:	05 ff 80 28 	swcpm0 \$c15,\(\$0\+\),-128
 ce8:	05 ff 80 68 	lwcpm0 \$c15,\(\$0\+\),-128
 cec:	05 ff 80 2c 	swcpm1 \$c15,\(\$0\+\),-128
 cf0:	05 ff 80 6c 	lwcpm1 \$c15,\(\$0\+\),-128
 cf4:	f5 f0 80 20 	swcpa \$c0,\(\$sp\+\),-128
 cf8:	f5 f0 80 60 	lwcpa \$c0,\(\$sp\+\),-128
 cfc:	f5 f0 80 28 	swcpm0 \$c0,\(\$sp\+\),-128
 d00:	f5 f0 80 68 	lwcpm0 \$c0,\(\$sp\+\),-128
 d04:	f5 f0 80 2c 	swcpm1 \$c0,\(\$sp\+\),-128
 d08:	f5 f0 80 6c 	lwcpm1 \$c0,\(\$sp\+\),-128
 d0c:	f5 ff 80 20 	swcpa \$c15,\(\$sp\+\),-128
 d10:	f5 ff 80 60 	lwcpa \$c15,\(\$sp\+\),-128
 d14:	f5 ff 80 28 	swcpm0 \$c15,\(\$sp\+\),-128
 d18:	f5 ff 80 68 	lwcpm0 \$c15,\(\$sp\+\),-128
 d1c:	f5 ff 80 2c 	swcpm1 \$c15,\(\$sp\+\),-128
 d20:	f5 ff 80 6c 	lwcpm1 \$c15,\(\$sp\+\),-128
 d24:	05 f0 7c 20 	swcpa \$c0,\(\$0\+\),124
 d28:	05 f0 7c 60 	lwcpa \$c0,\(\$0\+\),124
 d2c:	05 f0 7c 28 	swcpm0 \$c0,\(\$0\+\),124
 d30:	05 f0 7c 68 	lwcpm0 \$c0,\(\$0\+\),124
 d34:	05 f0 7c 2c 	swcpm1 \$c0,\(\$0\+\),124
 d38:	05 f0 7c 6c 	lwcpm1 \$c0,\(\$0\+\),124
 d3c:	05 ff 7c 20 	swcpa \$c15,\(\$0\+\),124
 d40:	05 ff 7c 60 	lwcpa \$c15,\(\$0\+\),124
 d44:	05 ff 7c 28 	swcpm0 \$c15,\(\$0\+\),124
 d48:	05 ff 7c 68 	lwcpm0 \$c15,\(\$0\+\),124
 d4c:	05 ff 7c 2c 	swcpm1 \$c15,\(\$0\+\),124
 d50:	05 ff 7c 6c 	lwcpm1 \$c15,\(\$0\+\),124
 d54:	f5 f0 7c 20 	swcpa \$c0,\(\$sp\+\),124
 d58:	f5 f0 7c 60 	lwcpa \$c0,\(\$sp\+\),124
 d5c:	f5 f0 7c 28 	swcpm0 \$c0,\(\$sp\+\),124
 d60:	f5 f0 7c 68 	lwcpm0 \$c0,\(\$sp\+\),124
 d64:	f5 f0 7c 2c 	swcpm1 \$c0,\(\$sp\+\),124
 d68:	f5 f0 7c 6c 	lwcpm1 \$c0,\(\$sp\+\),124
 d6c:	f5 ff 7c 20 	swcpa \$c15,\(\$sp\+\),124
 d70:	f5 ff 7c 60 	lwcpa \$c15,\(\$sp\+\),124
 d74:	f5 ff 7c 28 	swcpm0 \$c15,\(\$sp\+\),124
 d78:	f5 ff 7c 68 	lwcpm0 \$c15,\(\$sp\+\),124
 d7c:	f5 ff 7c 2c 	swcpm1 \$c15,\(\$sp\+\),124
 d80:	f5 ff 7c 6c 	lwcpm1 \$c15,\(\$sp\+\),124
 d84:	05 f0 80 30 	smcpa \$c0,\(\$0\+\),-128
 d88:	05 f0 80 70 	lmcpa \$c0,\(\$0\+\),-128
 d8c:	05 f0 80 38 	smcpm0 \$c0,\(\$0\+\),-128
 d90:	05 f0 80 78 	lmcpm0 \$c0,\(\$0\+\),-128
 d94:	05 f0 80 3c 	smcpm1 \$c0,\(\$0\+\),-128
 d98:	05 f0 80 7c 	lmcpm1 \$c0,\(\$0\+\),-128
 d9c:	05 ff 80 30 	smcpa \$c15,\(\$0\+\),-128
 da0:	05 ff 80 70 	lmcpa \$c15,\(\$0\+\),-128
 da4:	05 ff 80 38 	smcpm0 \$c15,\(\$0\+\),-128
 da8:	05 ff 80 78 	lmcpm0 \$c15,\(\$0\+\),-128
 dac:	05 ff 80 3c 	smcpm1 \$c15,\(\$0\+\),-128
 db0:	05 ff 80 7c 	lmcpm1 \$c15,\(\$0\+\),-128
 db4:	f5 f0 80 30 	smcpa \$c0,\(\$sp\+\),-128
 db8:	f5 f0 80 70 	lmcpa \$c0,\(\$sp\+\),-128
 dbc:	f5 f0 80 38 	smcpm0 \$c0,\(\$sp\+\),-128
 dc0:	f5 f0 80 78 	lmcpm0 \$c0,\(\$sp\+\),-128
 dc4:	f5 f0 80 3c 	smcpm1 \$c0,\(\$sp\+\),-128
 dc8:	f5 f0 80 7c 	lmcpm1 \$c0,\(\$sp\+\),-128
 dcc:	f5 ff 80 30 	smcpa \$c15,\(\$sp\+\),-128
 dd0:	f5 ff 80 70 	lmcpa \$c15,\(\$sp\+\),-128
 dd4:	f5 ff 80 38 	smcpm0 \$c15,\(\$sp\+\),-128
 dd8:	f5 ff 80 78 	lmcpm0 \$c15,\(\$sp\+\),-128
 ddc:	f5 ff 80 3c 	smcpm1 \$c15,\(\$sp\+\),-128
 de0:	f5 ff 80 7c 	lmcpm1 \$c15,\(\$sp\+\),-128
 de4:	05 f0 78 30 	smcpa \$c0,\(\$0\+\),120
 de8:	05 f0 78 70 	lmcpa \$c0,\(\$0\+\),120
 dec:	05 f0 78 38 	smcpm0 \$c0,\(\$0\+\),120
 df0:	05 f0 78 78 	lmcpm0 \$c0,\(\$0\+\),120
 df4:	05 f0 78 3c 	smcpm1 \$c0,\(\$0\+\),120
 df8:	05 f0 78 7c 	lmcpm1 \$c0,\(\$0\+\),120
 dfc:	05 ff 78 30 	smcpa \$c15,\(\$0\+\),120
 e00:	05 ff 78 70 	lmcpa \$c15,\(\$0\+\),120
 e04:	05 ff 78 38 	smcpm0 \$c15,\(\$0\+\),120
 e08:	05 ff 78 78 	lmcpm0 \$c15,\(\$0\+\),120
 e0c:	05 ff 78 3c 	smcpm1 \$c15,\(\$0\+\),120
 e10:	05 ff 78 7c 	lmcpm1 \$c15,\(\$0\+\),120
 e14:	f5 f0 78 30 	smcpa \$c0,\(\$sp\+\),120
 e18:	f5 f0 78 70 	lmcpa \$c0,\(\$sp\+\),120
 e1c:	f5 f0 78 38 	smcpm0 \$c0,\(\$sp\+\),120
 e20:	f5 f0 78 78 	lmcpm0 \$c0,\(\$sp\+\),120
 e24:	f5 f0 78 3c 	smcpm1 \$c0,\(\$sp\+\),120
 e28:	f5 f0 78 7c 	lmcpm1 \$c0,\(\$sp\+\),120
 e2c:	f5 ff 78 30 	smcpa \$c15,\(\$sp\+\),120
 e30:	f5 ff 78 70 	lmcpa \$c15,\(\$sp\+\),120
 e34:	f5 ff 78 38 	smcpm0 \$c15,\(\$sp\+\),120
 e38:	f5 ff 78 78 	lmcpm0 \$c15,\(\$sp\+\),120
 e3c:	f5 ff 78 3c 	smcpm1 \$c15,\(\$sp\+\),120
 e40:	f5 ff 78 7c 	lmcpm1 \$c15,\(\$sp\+\),120
 e44:	04 d8 02 80 	bcpeq 0x0,0xffff0e48
 e48:	05 d8 02 80 	bcpne 0x0,0xffff0e4c
 e4c:	06 d8 02 80 	bcpat 0x0,0xffff0e50
 e50:	07 d8 02 80 	bcpaf 0x0,0xffff0e54
 e54:	f4 d8 02 80 	bcpeq 0xf,0xffff0e58
 e58:	f5 d8 02 80 	bcpne 0xf,0xffff0e5c
 e5c:	f6 d8 02 80 	bcpat 0xf,0xffff0e60
 e60:	f7 d8 02 80 	bcpaf 0xf,0xffff0e64
 e64:	04 d8 ff 3f 	bcpeq 0x0,0x8e62
 e68:	05 d8 ff 3f 	bcpne 0x0,0x8e66
 e6c:	06 d8 ff 3f 	bcpat 0x0,0x8e6a
 e70:	07 d8 ff 3f 	bcpaf 0x0,0x8e6e
 e74:	f4 d8 ff 3f 	bcpeq 0xf,0x8e72
 e78:	f5 d8 ff 3f 	bcpne 0xf,0x8e76
 e7c:	f6 d8 ff 3f 	bcpat 0xf,0x8e7a
 e80:	f7 d8 ff 3f 	bcpaf 0xf,0x8e7e
 e84:	04 d8 00 00 	bcpeq 0x0,0xe84
			e84: R_MEP_PCREL17A2	symbol
 e88:	05 d8 00 00 	bcpne 0x0,0xe88
			e88: R_MEP_PCREL17A2	symbol
 e8c:	06 d8 00 00 	bcpat 0x0,0xe8c
			e8c: R_MEP_PCREL17A2	symbol
 e90:	07 d8 00 00 	bcpaf 0x0,0xe90
			e90: R_MEP_PCREL17A2	symbol
 e94:	f4 d8 00 00 	bcpeq 0xf,0xe94
			e94: R_MEP_PCREL17A2	symbol
 e98:	f5 d8 00 00 	bcpne 0xf,0xe98
			e98: R_MEP_PCREL17A2	symbol
 e9c:	f6 d8 00 00 	bcpat 0xf,0xe9c
			e9c: R_MEP_PCREL17A2	symbol
 ea0:	f7 d8 00 00 	bcpaf 0xf,0xea0
			ea0: R_MEP_PCREL17A2	symbol
 ea4:	21 70       	synccp
 ea6:	0f 18       	jsrv \$0
 ea8:	ff 18       	jsrv \$sp
 eaa:	2b d8 00 80 	bsrv 0xff800eae
 eae:	fb df ff 7f 	bsrv 0x800eac
 eb2:	0b d8 00 00 	bsrv 0xeb2
			eb2: R_MEP_PCREL24A2	symbol
 eb6:	00 00       	nop
			eb6: R_MEP_8	symbol
			eb7: R_MEP_16	symbol
 eb8:	00 00       	nop
			eb9: R_MEP_32	symbol
 eba:	00 00       	nop
.*
