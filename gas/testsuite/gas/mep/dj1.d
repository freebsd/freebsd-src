#as:
#objdump: -dr
#name: dj1

dump.o:     file format elf32-mep

Disassembly of section .text:

00000000 <.text>:
   0:	00 00       	nop
   2:	01 00       	mov \$1,\$0
   4:	02 00       	mov \$2,\$0
   6:	03 00       	mov \$3,\$0
   8:	04 00       	mov \$4,\$0
   a:	05 00       	mov \$5,\$0
   c:	06 00       	mov \$6,\$0
   e:	07 00       	mov \$7,\$0
  10:	08 00       	mov \$8,\$0
  12:	09 00       	mov \$9,\$0
  14:	0a 00       	mov \$10,\$0
  16:	0b 00       	mov \$11,\$0
  18:	0c 00       	mov \$12,\$0
  1a:	0d 00       	mov \$tp,\$0
  1c:	0e 00       	mov \$gp,\$0
  1e:	0f 00       	mov \$sp,\$0
  20:	08 00       	mov \$8,\$0
  22:	0d 00       	mov \$tp,\$0
  24:	0e 00       	mov \$gp,\$0
  26:	0f 00       	mov \$sp,\$0
  28:	00 08       	sb \$0,\(\$0\)
  2a:	00 09       	sh \$0,\(\$0\)
  2c:	00 0a       	sw \$0,\(\$0\)
  2e:	00 0c       	lb \$0,\(\$0\)
  30:	00 0d       	lh \$0,\(\$0\)
  32:	00 0e       	lw \$0,\(\$0\)
  34:	00 0b       	lbu \$0,\(\$0\)
  36:	00 0f       	lhu \$0,\(\$0\)
  38:	0f 08       	sb \$sp,\(\$0\)
  3a:	0f 09       	sh \$sp,\(\$0\)
  3c:	0f 0a       	sw \$sp,\(\$0\)
  3e:	0f 0c       	lb \$sp,\(\$0\)
  40:	0f 0d       	lh \$sp,\(\$0\)
  42:	0f 0e       	lw \$sp,\(\$0\)
  44:	0f 0b       	lbu \$sp,\(\$0\)
  46:	0f 0f       	lhu \$sp,\(\$0\)
  48:	00 f8       	sb \$0,\(\$sp\)
  4a:	00 f9       	sh \$0,\(\$sp\)
  4c:	00 fa       	sw \$0,\(\$sp\)
  4e:	00 fc       	lb \$0,\(\$sp\)
  50:	00 fd       	lh \$0,\(\$sp\)
  52:	00 fe       	lw \$0,\(\$sp\)
  54:	00 fb       	lbu \$0,\(\$sp\)
  56:	00 ff       	lhu \$0,\(\$sp\)
  58:	0f f8       	sb \$sp,\(\$sp\)
  5a:	0f f9       	sh \$sp,\(\$sp\)
  5c:	0f fa       	sw \$sp,\(\$sp\)
  5e:	0f fc       	lb \$sp,\(\$sp\)
  60:	0f fd       	lh \$sp,\(\$sp\)
  62:	0f fe       	lw \$sp,\(\$sp\)
  64:	0f fb       	lbu \$sp,\(\$sp\)
  66:	0f ff       	lhu \$sp,\(\$sp\)
  68:	00 fa       	sw \$0,\(\$sp\)
  6a:	00 fe       	lw \$0,\(\$sp\)
  6c:	0f fa       	sw \$sp,\(\$sp\)
  6e:	0f fe       	lw \$sp,\(\$sp\)
  70:	40 7e       	sw \$0,0x7c\(\$sp\)
  72:	40 7f       	lw \$0,0x7c\(\$sp\)
  74:	4f 7e       	sw \$sp,0x7c\(\$sp\)
  76:	4f 7f       	lw \$sp,0x7c\(\$sp\)
  78:	00 fa       	sw \$0,\(\$sp\)
  7a:	00 fe       	lw \$0,\(\$sp\)
  7c:	0f fa       	sw \$sp,\(\$sp\)
  7e:	0f fe       	lw \$sp,\(\$sp\)
  80:	40 7e       	sw \$0,0x7c\(\$sp\)
  82:	40 7f       	lw \$0,0x7c\(\$sp\)
  84:	4f 7e       	sw \$sp,0x7c\(\$sp\)
  86:	4f 7f       	lw \$sp,0x7c\(\$sp\)
  88:	00 d8       	sb \$0,\(\$tp\)
  8a:	00 dc       	lb \$0,\(\$tp\)
  8c:	00 db       	lbu \$0,\(\$tp\)
  8e:	07 d8       	sb \$7,\(\$tp\)
  90:	07 dc       	lb \$7,\(\$tp\)
  92:	07 db       	lbu \$7,\(\$tp\)
  94:	80 7f       	sb \$0,0x7f\(\$tp\)
  96:	88 7f       	lb \$0,0x7f\(\$tp\)
  98:	48 ff       	lbu \$0,0x7f\(\$tp\)
  9a:	87 7f       	sb \$7,0x7f\(\$tp\)
  9c:	8f 7f       	lb \$7,0x7f\(\$tp\)
  9e:	4f ff       	lbu \$7,0x7f\(\$tp\)
  a0:	80 00       	sb \$0,0x0\(\$tp\)
			a0: R_MEP_TPREL7	symbol
  a2:	88 00       	lb \$0,0x0\(\$tp\)
			a2: R_MEP_TPREL7	symbol
  a4:	48 80       	lbu \$0,0x0\(\$tp\)
			a4: R_MEP_TPREL7	symbol
  a6:	87 00       	sb \$7,0x0\(\$tp\)
			a6: R_MEP_TPREL7	symbol
  a8:	8f 00       	lb \$7,0x0\(\$tp\)
			a8: R_MEP_TPREL7	symbol
  aa:	4f 80       	lbu \$7,0x0\(\$tp\)
			aa: R_MEP_TPREL7	symbol
  ac:	00 d8       	sb \$0,\(\$tp\)
  ae:	00 dc       	lb \$0,\(\$tp\)
  b0:	00 db       	lbu \$0,\(\$tp\)
  b2:	07 d8       	sb \$7,\(\$tp\)
  b4:	07 dc       	lb \$7,\(\$tp\)
  b6:	07 db       	lbu \$7,\(\$tp\)
  b8:	80 7f       	sb \$0,0x7f\(\$tp\)
  ba:	88 7f       	lb \$0,0x7f\(\$tp\)
  bc:	48 ff       	lbu \$0,0x7f\(\$tp\)
  be:	87 7f       	sb \$7,0x7f\(\$tp\)
  c0:	8f 7f       	lb \$7,0x7f\(\$tp\)
  c2:	4f ff       	lbu \$7,0x7f\(\$tp\)
  c4:	80 00       	sb \$0,0x0\(\$tp\)
			c4: R_MEP_TPREL7	symbol
  c6:	88 00       	lb \$0,0x0\(\$tp\)
			c6: R_MEP_TPREL7	symbol
  c8:	48 80       	lbu \$0,0x0\(\$tp\)
			c8: R_MEP_TPREL7	symbol
  ca:	87 00       	sb \$7,0x0\(\$tp\)
			ca: R_MEP_TPREL7	symbol
  cc:	8f 00       	lb \$7,0x0\(\$tp\)
			cc: R_MEP_TPREL7	symbol
  ce:	4f 80       	lbu \$7,0x0\(\$tp\)
			ce: R_MEP_TPREL7	symbol
  d0:	00 d9       	sh \$0,\(\$tp\)
  d2:	00 dd       	lh \$0,\(\$tp\)
  d4:	00 df       	lhu \$0,\(\$tp\)
  d6:	07 d9       	sh \$7,\(\$tp\)
  d8:	07 dd       	lh \$7,\(\$tp\)
  da:	07 df       	lhu \$7,\(\$tp\)
  dc:	80 fe       	sh \$0,0x7e\(\$tp\)
  de:	88 fe       	lh \$0,0x7e\(\$tp\)
  e0:	88 ff       	lhu \$0,0x7e\(\$tp\)
  e2:	87 fe       	sh \$7,0x7e\(\$tp\)
  e4:	8f fe       	lh \$7,0x7e\(\$tp\)
  e6:	8f ff       	lhu \$7,0x7e\(\$tp\)
  e8:	80 80       	sh \$0,0x0\(\$tp\)
			e8: R_MEP_TPREL7A2	symbol
  ea:	88 80       	lh \$0,0x0\(\$tp\)
			ea: R_MEP_TPREL7A2	symbol
  ec:	88 81       	lhu \$0,0x0\(\$tp\)
			ec: R_MEP_TPREL7A2	symbol
  ee:	87 80       	sh \$7,0x0\(\$tp\)
			ee: R_MEP_TPREL7A2	symbol
  f0:	8f 80       	lh \$7,0x0\(\$tp\)
			f0: R_MEP_TPREL7A2	symbol
  f2:	8f 81       	lhu \$7,0x0\(\$tp\)
			f2: R_MEP_TPREL7A2	symbol
  f4:	00 d9       	sh \$0,\(\$tp\)
  f6:	00 dd       	lh \$0,\(\$tp\)
  f8:	00 df       	lhu \$0,\(\$tp\)
  fa:	07 d9       	sh \$7,\(\$tp\)
  fc:	07 dd       	lh \$7,\(\$tp\)
  fe:	07 df       	lhu \$7,\(\$tp\)
 100:	80 fe       	sh \$0,0x7e\(\$tp\)
 102:	88 fe       	lh \$0,0x7e\(\$tp\)
 104:	88 ff       	lhu \$0,0x7e\(\$tp\)
 106:	87 fe       	sh \$7,0x7e\(\$tp\)
 108:	8f fe       	lh \$7,0x7e\(\$tp\)
 10a:	8f ff       	lhu \$7,0x7e\(\$tp\)
 10c:	80 80       	sh \$0,0x0\(\$tp\)
			10c: R_MEP_TPREL7A2	symbol
 10e:	88 80       	lh \$0,0x0\(\$tp\)
			10e: R_MEP_TPREL7A2	symbol
 110:	88 81       	lhu \$0,0x0\(\$tp\)
			110: R_MEP_TPREL7A2	symbol
 112:	87 80       	sh \$7,0x0\(\$tp\)
			112: R_MEP_TPREL7A2	symbol
 114:	8f 80       	lh \$7,0x0\(\$tp\)
			114: R_MEP_TPREL7A2	symbol
 116:	8f 81       	lhu \$7,0x0\(\$tp\)
			116: R_MEP_TPREL7A2	symbol
 118:	00 da       	sw \$0,\(\$tp\)
 11a:	00 de       	lw \$0,\(\$tp\)
 11c:	07 da       	sw \$7,\(\$tp\)
 11e:	07 de       	lw \$7,\(\$tp\)
 120:	40 fe       	sw \$0,0x7c\(\$tp\)
 122:	40 ff       	lw \$0,0x7c\(\$tp\)
 124:	47 fe       	sw \$7,0x7c\(\$tp\)
 126:	47 ff       	lw \$7,0x7c\(\$tp\)
 128:	40 82       	sw \$0,0x0\(\$tp\)
			128: R_MEP_TPREL7A4	symbol
 12a:	40 83       	lw \$0,0x0\(\$tp\)
			12a: R_MEP_TPREL7A4	symbol
 12c:	47 82       	sw \$7,0x0\(\$tp\)
			12c: R_MEP_TPREL7A4	symbol
 12e:	47 83       	lw \$7,0x0\(\$tp\)
			12e: R_MEP_TPREL7A4	symbol
 130:	00 da       	sw \$0,\(\$tp\)
 132:	00 de       	lw \$0,\(\$tp\)
 134:	07 da       	sw \$7,\(\$tp\)
 136:	07 de       	lw \$7,\(\$tp\)
 138:	40 fe       	sw \$0,0x7c\(\$tp\)
 13a:	40 ff       	lw \$0,0x7c\(\$tp\)
 13c:	47 fe       	sw \$7,0x7c\(\$tp\)
 13e:	47 ff       	lw \$7,0x7c\(\$tp\)
 140:	40 82       	sw \$0,0x0\(\$tp\)
			140: R_MEP_TPREL7A4	symbol
 142:	40 83       	lw \$0,0x0\(\$tp\)
			142: R_MEP_TPREL7A4	symbol
 144:	47 82       	sw \$7,0x0\(\$tp\)
			144: R_MEP_TPREL7A4	symbol
 146:	47 83       	lw \$7,0x0\(\$tp\)
			146: R_MEP_TPREL7A4	symbol
 148:	c0 08 80 00 	sb \$0,-32768\(\$0\)
 14c:	c0 09 80 00 	sh \$0,-32768\(\$0\)
 150:	c0 0a 80 00 	sw \$0,-32768\(\$0\)
 154:	c0 0c 80 00 	lb \$0,-32768\(\$0\)
 158:	c0 0d 80 00 	lh \$0,-32768\(\$0\)
 15c:	c0 0e 80 00 	lw \$0,-32768\(\$0\)
 160:	c0 0b 80 00 	lbu \$0,-32768\(\$0\)
 164:	c0 0f 80 00 	lhu \$0,-32768\(\$0\)
 168:	cf 08 80 00 	sb \$sp,-32768\(\$0\)
 16c:	cf 09 80 00 	sh \$sp,-32768\(\$0\)
 170:	cf 0a 80 00 	sw \$sp,-32768\(\$0\)
 174:	cf 0c 80 00 	lb \$sp,-32768\(\$0\)
 178:	cf 0d 80 00 	lh \$sp,-32768\(\$0\)
 17c:	cf 0e 80 00 	lw \$sp,-32768\(\$0\)
 180:	cf 0b 80 00 	lbu \$sp,-32768\(\$0\)
 184:	cf 0f 80 00 	lhu \$sp,-32768\(\$0\)
 188:	c0 08 7f ff 	sb \$0,32767\(\$0\)
 18c:	c0 09 7f ff 	sh \$0,32767\(\$0\)
 190:	c0 0a 7f ff 	sw \$0,32767\(\$0\)
 194:	c0 0c 7f ff 	lb \$0,32767\(\$0\)
 198:	c0 0d 7f ff 	lh \$0,32767\(\$0\)
 19c:	c0 0e 7f ff 	lw \$0,32767\(\$0\)
 1a0:	c0 0b 7f ff 	lbu \$0,32767\(\$0\)
 1a4:	c0 0f 7f ff 	lhu \$0,32767\(\$0\)
 1a8:	cf 08 7f ff 	sb \$sp,32767\(\$0\)
 1ac:	cf 09 7f ff 	sh \$sp,32767\(\$0\)
 1b0:	cf 0a 7f ff 	sw \$sp,32767\(\$0\)
 1b4:	cf 0c 7f ff 	lb \$sp,32767\(\$0\)
 1b8:	cf 0d 7f ff 	lh \$sp,32767\(\$0\)
 1bc:	cf 0e 7f ff 	lw \$sp,32767\(\$0\)
 1c0:	cf 0b 7f ff 	lbu \$sp,32767\(\$0\)
 1c4:	cf 0f 7f ff 	lhu \$sp,32767\(\$0\)
 1c8:	c0 08 00 00 	sb \$0,0\(\$0\)
			1c8: R_MEP_GPREL	symbol
 1cc:	c0 09 00 00 	sh \$0,0\(\$0\)
			1cc: R_MEP_GPREL	symbol
 1d0:	c0 0a 00 00 	sw \$0,0\(\$0\)
			1d0: R_MEP_GPREL	symbol
 1d4:	c0 0c 00 00 	lb \$0,0\(\$0\)
			1d4: R_MEP_GPREL	symbol
 1d8:	c0 0d 00 00 	lh \$0,0\(\$0\)
			1d8: R_MEP_GPREL	symbol
 1dc:	c0 0e 00 00 	lw \$0,0\(\$0\)
			1dc: R_MEP_GPREL	symbol
 1e0:	c0 0b 00 00 	lbu \$0,0\(\$0\)
			1e0: R_MEP_GPREL	symbol
 1e4:	c0 0f 00 00 	lhu \$0,0\(\$0\)
			1e4: R_MEP_GPREL	symbol
 1e8:	cf 08 00 00 	sb \$sp,0\(\$0\)
			1e8: R_MEP_GPREL	symbol
 1ec:	cf 09 00 00 	sh \$sp,0\(\$0\)
			1ec: R_MEP_GPREL	symbol
 1f0:	cf 0a 00 00 	sw \$sp,0\(\$0\)
			1f0: R_MEP_GPREL	symbol
 1f4:	cf 0c 00 00 	lb \$sp,0\(\$0\)
			1f4: R_MEP_GPREL	symbol
 1f8:	cf 0d 00 00 	lh \$sp,0\(\$0\)
			1f8: R_MEP_GPREL	symbol
 1fc:	cf 0e 00 00 	lw \$sp,0\(\$0\)
			1fc: R_MEP_GPREL	symbol
 200:	cf 0b 00 00 	lbu \$sp,0\(\$0\)
			200: R_MEP_GPREL	symbol
 204:	cf 0f 00 00 	lhu \$sp,0\(\$0\)
			204: R_MEP_GPREL	symbol
 208:	c0 08 80 00 	sb \$0,-32768\(\$0\)
 20c:	c0 09 80 00 	sh \$0,-32768\(\$0\)
 210:	c0 0a 80 00 	sw \$0,-32768\(\$0\)
 214:	c0 0c 80 00 	lb \$0,-32768\(\$0\)
 218:	c0 0d 80 00 	lh \$0,-32768\(\$0\)
 21c:	c0 0e 80 00 	lw \$0,-32768\(\$0\)
 220:	c0 0b 80 00 	lbu \$0,-32768\(\$0\)
 224:	c0 0f 80 00 	lhu \$0,-32768\(\$0\)
 228:	cf 08 80 00 	sb \$sp,-32768\(\$0\)
 22c:	cf 09 80 00 	sh \$sp,-32768\(\$0\)
 230:	cf 0a 80 00 	sw \$sp,-32768\(\$0\)
 234:	cf 0c 80 00 	lb \$sp,-32768\(\$0\)
 238:	cf 0d 80 00 	lh \$sp,-32768\(\$0\)
 23c:	cf 0e 80 00 	lw \$sp,-32768\(\$0\)
 240:	cf 0b 80 00 	lbu \$sp,-32768\(\$0\)
 244:	cf 0f 80 00 	lhu \$sp,-32768\(\$0\)
 248:	c0 08 7f ff 	sb \$0,32767\(\$0\)
 24c:	c0 09 7f ff 	sh \$0,32767\(\$0\)
 250:	c0 0a 7f ff 	sw \$0,32767\(\$0\)
 254:	c0 0c 7f ff 	lb \$0,32767\(\$0\)
 258:	c0 0d 7f ff 	lh \$0,32767\(\$0\)
 25c:	c0 0e 7f ff 	lw \$0,32767\(\$0\)
 260:	c0 0b 7f ff 	lbu \$0,32767\(\$0\)
 264:	c0 0f 7f ff 	lhu \$0,32767\(\$0\)
 268:	cf 08 7f ff 	sb \$sp,32767\(\$0\)
 26c:	cf 09 7f ff 	sh \$sp,32767\(\$0\)
 270:	cf 0a 7f ff 	sw \$sp,32767\(\$0\)
 274:	cf 0c 7f ff 	lb \$sp,32767\(\$0\)
 278:	cf 0d 7f ff 	lh \$sp,32767\(\$0\)
 27c:	cf 0e 7f ff 	lw \$sp,32767\(\$0\)
 280:	cf 0b 7f ff 	lbu \$sp,32767\(\$0\)
 284:	cf 0f 7f ff 	lhu \$sp,32767\(\$0\)
 288:	c0 08 00 00 	sb \$0,0\(\$0\)
			288: R_MEP_TPREL	symbol
 28c:	c0 09 00 00 	sh \$0,0\(\$0\)
			28c: R_MEP_TPREL	symbol
 290:	c0 0a 00 00 	sw \$0,0\(\$0\)
			290: R_MEP_TPREL	symbol
 294:	c0 0c 00 00 	lb \$0,0\(\$0\)
			294: R_MEP_TPREL	symbol
 298:	c0 0d 00 00 	lh \$0,0\(\$0\)
			298: R_MEP_TPREL	symbol
 29c:	c0 0e 00 00 	lw \$0,0\(\$0\)
			29c: R_MEP_TPREL	symbol
 2a0:	c0 0b 00 00 	lbu \$0,0\(\$0\)
			2a0: R_MEP_TPREL	symbol
 2a4:	c0 0f 00 00 	lhu \$0,0\(\$0\)
			2a4: R_MEP_TPREL	symbol
 2a8:	cf 08 00 00 	sb \$sp,0\(\$0\)
			2a8: R_MEP_TPREL	symbol
 2ac:	cf 09 00 00 	sh \$sp,0\(\$0\)
			2ac: R_MEP_TPREL	symbol
 2b0:	cf 0a 00 00 	sw \$sp,0\(\$0\)
			2b0: R_MEP_TPREL	symbol
 2b4:	cf 0c 00 00 	lb \$sp,0\(\$0\)
			2b4: R_MEP_TPREL	symbol
 2b8:	cf 0d 00 00 	lh \$sp,0\(\$0\)
			2b8: R_MEP_TPREL	symbol
 2bc:	cf 0e 00 00 	lw \$sp,0\(\$0\)
			2bc: R_MEP_TPREL	symbol
 2c0:	cf 0b 00 00 	lbu \$sp,0\(\$0\)
			2c0: R_MEP_TPREL	symbol
 2c4:	cf 0f 00 00 	lhu \$sp,0\(\$0\)
			2c4: R_MEP_TPREL	symbol
 2c8:	c0 f8 80 00 	sb \$0,-32768\(\$sp\)
 2cc:	c0 f9 80 00 	sh \$0,-32768\(\$sp\)
 2d0:	c0 fa 80 00 	sw \$0,-32768\(\$sp\)
 2d4:	c0 fc 80 00 	lb \$0,-32768\(\$sp\)
 2d8:	c0 fd 80 00 	lh \$0,-32768\(\$sp\)
 2dc:	c0 fe 80 00 	lw \$0,-32768\(\$sp\)
 2e0:	c0 fb 80 00 	lbu \$0,-32768\(\$sp\)
 2e4:	c0 ff 80 00 	lhu \$0,-32768\(\$sp\)
 2e8:	cf f8 80 00 	sb \$sp,-32768\(\$sp\)
 2ec:	cf f9 80 00 	sh \$sp,-32768\(\$sp\)
 2f0:	cf fa 80 00 	sw \$sp,-32768\(\$sp\)
 2f4:	cf fc 80 00 	lb \$sp,-32768\(\$sp\)
 2f8:	cf fd 80 00 	lh \$sp,-32768\(\$sp\)
 2fc:	cf fe 80 00 	lw \$sp,-32768\(\$sp\)
 300:	cf fb 80 00 	lbu \$sp,-32768\(\$sp\)
 304:	cf ff 80 00 	lhu \$sp,-32768\(\$sp\)
 308:	c0 f8 7f ff 	sb \$0,32767\(\$sp\)
 30c:	c0 f9 7f ff 	sh \$0,32767\(\$sp\)
 310:	c0 fa 7f ff 	sw \$0,32767\(\$sp\)
 314:	c0 fc 7f ff 	lb \$0,32767\(\$sp\)
 318:	c0 fd 7f ff 	lh \$0,32767\(\$sp\)
 31c:	c0 fe 7f ff 	lw \$0,32767\(\$sp\)
 320:	c0 fb 7f ff 	lbu \$0,32767\(\$sp\)
 324:	c0 ff 7f ff 	lhu \$0,32767\(\$sp\)
 328:	cf f8 7f ff 	sb \$sp,32767\(\$sp\)
 32c:	cf f9 7f ff 	sh \$sp,32767\(\$sp\)
 330:	cf fa 7f ff 	sw \$sp,32767\(\$sp\)
 334:	cf fc 7f ff 	lb \$sp,32767\(\$sp\)
 338:	cf fd 7f ff 	lh \$sp,32767\(\$sp\)
 33c:	cf fe 7f ff 	lw \$sp,32767\(\$sp\)
 340:	cf fb 7f ff 	lbu \$sp,32767\(\$sp\)
 344:	cf ff 7f ff 	lhu \$sp,32767\(\$sp\)
 348:	c0 f8 00 00 	sb \$0,0\(\$sp\)
			348: R_MEP_GPREL	symbol
 34c:	c0 f9 00 00 	sh \$0,0\(\$sp\)
			34c: R_MEP_GPREL	symbol
 350:	c0 fa 00 00 	sw \$0,0\(\$sp\)
			350: R_MEP_GPREL	symbol
 354:	c0 fc 00 00 	lb \$0,0\(\$sp\)
			354: R_MEP_GPREL	symbol
 358:	c0 fd 00 00 	lh \$0,0\(\$sp\)
			358: R_MEP_GPREL	symbol
 35c:	c0 fe 00 00 	lw \$0,0\(\$sp\)
			35c: R_MEP_GPREL	symbol
 360:	c0 fb 00 00 	lbu \$0,0\(\$sp\)
			360: R_MEP_GPREL	symbol
 364:	c0 ff 00 00 	lhu \$0,0\(\$sp\)
			364: R_MEP_GPREL	symbol
 368:	cf f8 00 00 	sb \$sp,0\(\$sp\)
			368: R_MEP_GPREL	symbol
 36c:	cf f9 00 00 	sh \$sp,0\(\$sp\)
			36c: R_MEP_GPREL	symbol
 370:	cf fa 00 00 	sw \$sp,0\(\$sp\)
			370: R_MEP_GPREL	symbol
 374:	cf fc 00 00 	lb \$sp,0\(\$sp\)
			374: R_MEP_GPREL	symbol
 378:	cf fd 00 00 	lh \$sp,0\(\$sp\)
			378: R_MEP_GPREL	symbol
 37c:	cf fe 00 00 	lw \$sp,0\(\$sp\)
			37c: R_MEP_GPREL	symbol
 380:	cf fb 00 00 	lbu \$sp,0\(\$sp\)
			380: R_MEP_GPREL	symbol
 384:	cf ff 00 00 	lhu \$sp,0\(\$sp\)
			384: R_MEP_GPREL	symbol
 388:	c0 f8 80 00 	sb \$0,-32768\(\$sp\)
 38c:	c0 f9 80 00 	sh \$0,-32768\(\$sp\)
 390:	c0 fa 80 00 	sw \$0,-32768\(\$sp\)
 394:	c0 fc 80 00 	lb \$0,-32768\(\$sp\)
 398:	c0 fd 80 00 	lh \$0,-32768\(\$sp\)
 39c:	c0 fe 80 00 	lw \$0,-32768\(\$sp\)
 3a0:	c0 fb 80 00 	lbu \$0,-32768\(\$sp\)
 3a4:	c0 ff 80 00 	lhu \$0,-32768\(\$sp\)
 3a8:	cf f8 80 00 	sb \$sp,-32768\(\$sp\)
 3ac:	cf f9 80 00 	sh \$sp,-32768\(\$sp\)
 3b0:	cf fa 80 00 	sw \$sp,-32768\(\$sp\)
 3b4:	cf fc 80 00 	lb \$sp,-32768\(\$sp\)
 3b8:	cf fd 80 00 	lh \$sp,-32768\(\$sp\)
 3bc:	cf fe 80 00 	lw \$sp,-32768\(\$sp\)
 3c0:	cf fb 80 00 	lbu \$sp,-32768\(\$sp\)
 3c4:	cf ff 80 00 	lhu \$sp,-32768\(\$sp\)
 3c8:	c0 f8 7f ff 	sb \$0,32767\(\$sp\)
 3cc:	c0 f9 7f ff 	sh \$0,32767\(\$sp\)
 3d0:	c0 fa 7f ff 	sw \$0,32767\(\$sp\)
 3d4:	c0 fc 7f ff 	lb \$0,32767\(\$sp\)
 3d8:	c0 fd 7f ff 	lh \$0,32767\(\$sp\)
 3dc:	c0 fe 7f ff 	lw \$0,32767\(\$sp\)
 3e0:	c0 fb 7f ff 	lbu \$0,32767\(\$sp\)
 3e4:	c0 ff 7f ff 	lhu \$0,32767\(\$sp\)
 3e8:	cf f8 7f ff 	sb \$sp,32767\(\$sp\)
 3ec:	cf f9 7f ff 	sh \$sp,32767\(\$sp\)
 3f0:	cf fa 7f ff 	sw \$sp,32767\(\$sp\)
 3f4:	cf fc 7f ff 	lb \$sp,32767\(\$sp\)
 3f8:	cf fd 7f ff 	lh \$sp,32767\(\$sp\)
 3fc:	cf fe 7f ff 	lw \$sp,32767\(\$sp\)
 400:	cf fb 7f ff 	lbu \$sp,32767\(\$sp\)
 404:	cf ff 7f ff 	lhu \$sp,32767\(\$sp\)
 408:	c0 f8 00 00 	sb \$0,0\(\$sp\)
			408: R_MEP_TPREL	symbol
 40c:	c0 f9 00 00 	sh \$0,0\(\$sp\)
			40c: R_MEP_TPREL	symbol
 410:	40 02       	sw \$0,0x0\(\$sp\)
			410: R_MEP_TPREL7A4	symbol
 412:	c0 fc 00 00 	lb \$0,0\(\$sp\)
			412: R_MEP_TPREL	symbol
 416:	c0 fd 00 00 	lh \$0,0\(\$sp\)
			416: R_MEP_TPREL	symbol
 41a:	40 03       	lw \$0,0x0\(\$sp\)
			41a: R_MEP_TPREL7A4	symbol
 41c:	c0 fb 00 00 	lbu \$0,0\(\$sp\)
			41c: R_MEP_TPREL	symbol
 420:	c0 ff 00 00 	lhu \$0,0\(\$sp\)
			420: R_MEP_TPREL	symbol
 424:	cf f8 00 00 	sb \$sp,0\(\$sp\)
			424: R_MEP_TPREL	symbol
 428:	cf f9 00 00 	sh \$sp,0\(\$sp\)
			428: R_MEP_TPREL	symbol
 42c:	4f 02       	sw \$sp,0x0\(\$sp\)
			42c: R_MEP_TPREL7A4	symbol
 42e:	cf fc 00 00 	lb \$sp,0\(\$sp\)
			42e: R_MEP_TPREL	symbol
 432:	cf fd 00 00 	lh \$sp,0\(\$sp\)
			432: R_MEP_TPREL	symbol
 436:	4f 03       	lw \$sp,0x0\(\$sp\)
			436: R_MEP_TPREL7A4	symbol
 438:	cf fb 00 00 	lbu \$sp,0\(\$sp\)
			438: R_MEP_TPREL	symbol
 43c:	cf ff 00 00 	lhu \$sp,0\(\$sp\)
			43c: R_MEP_TPREL	symbol
 440:	e0 02 00 00 	sw \$0,\(0x0\)
 444:	e0 03 00 00 	lw \$0,\(0x0\)
 448:	ef 02 00 00 	sw \$sp,\(0x0\)
 44c:	ef 03 00 00 	lw \$sp,\(0x0\)
 450:	e0 fe ff ff 	sw \$0,\(0xfffffc\)
 454:	e0 ff ff ff 	lw \$0,\(0xfffffc\)
 458:	ef fe ff ff 	sw \$sp,\(0xfffffc\)
 45c:	ef ff ff ff 	lw \$sp,\(0xfffffc\)
 460:	e0 02 00 00 	sw \$0,\(0x0\)
			460: R_MEP_ADDR24A4	symbol
 464:	e0 03 00 00 	lw \$0,\(0x0\)
			464: R_MEP_ADDR24A4	symbol
 468:	ef 02 00 00 	sw \$sp,\(0x0\)
			468: R_MEP_ADDR24A4	symbol
 46c:	ef 03 00 00 	lw \$sp,\(0x0\)
			46c: R_MEP_ADDR24A4	symbol
 470:	10 0d       	extb \$0
 472:	10 8d       	extub \$0
 474:	10 2d       	exth \$0
 476:	10 ad       	extuh \$0
 478:	1f 0d       	extb \$sp
 47a:	1f 8d       	extub \$sp
 47c:	1f 2d       	exth \$sp
 47e:	1f ad       	extuh \$sp
 480:	10 0c       	ssarb 0\(\$0\)
 482:	13 0c       	ssarb 3\(\$0\)
 484:	10 fc       	ssarb 0\(\$sp\)
 486:	13 fc       	ssarb 3\(\$sp\)
 488:	00 00       	nop
 48a:	0f 00       	mov \$sp,\$0
 48c:	00 f0       	mov \$0,\$sp
 48e:	0f f0       	mov \$sp,\$sp
 490:	c0 01 80 00 	mov \$0,-32768
 494:	cf 01 80 00 	mov \$sp,-32768
 498:	50 80       	mov \$0,-128
 49a:	5f 80       	mov \$sp,-128
 49c:	50 00       	mov \$0,0
 49e:	5f 00       	mov \$sp,0
 4a0:	50 7f       	mov \$0,127
 4a2:	5f 7f       	mov \$sp,127
 4a4:	c0 01 7f ff 	mov \$0,32767
 4a8:	cf 01 7f ff 	mov \$sp,32767
 4ac:	c0 01 00 00 	mov \$0,0
			4ac: R_MEP_LOW16	symbol
 4b0:	c0 01 00 00 	mov \$0,0
			4b0: R_MEP_HI16S	symbol
 4b4:	c0 01 00 00 	mov \$0,0
			4b4: R_MEP_HI16U	symbol
 4b8:	c0 01 00 00 	mov \$0,0
			4b8: R_MEP_GPREL	symbol
 4bc:	c0 01 00 00 	mov \$0,0
			4bc: R_MEP_TPREL	symbol
 4c0:	d0 00 00 00 	movu \$0,0x0
 4c4:	d7 00 00 00 	movu \$7,0x0
 4c8:	d0 ff ff ff 	movu \$0,0xffffff
 4cc:	d7 ff ff ff 	movu \$7,0xffffff
 4d0:	c0 11 00 00 	movu \$0,0x0
			4d0: R_MEP_LOW16	symbol
 4d4:	c7 11 00 00 	movu \$7,0x0
			4d4: R_MEP_LOW16	symbol
 4d8:	d0 00 00 00 	movu \$0,0x0
			4d8: R_MEP_UIMM24	symbol
 4dc:	d7 00 00 00 	movu \$7,0x0
			4dc: R_MEP_UIMM24	symbol
 4e0:	d0 00 00 00 	movu \$0,0x0
 4e4:	c0 21 00 00 	movh \$0,0x0
 4e8:	cf 11 00 00 	movu \$sp,0x0
 4ec:	cf 21 00 00 	movh \$sp,0x0
 4f0:	d0 ff 00 ff 	movu \$0,0xffff
 4f4:	c0 21 ff ff 	movh \$0,0xffff
 4f8:	cf 11 ff ff 	movu \$sp,0xffff
 4fc:	cf 21 ff ff 	movh \$sp,0xffff
 500:	c0 11 00 00 	movu \$0,0x0
			500: R_MEP_LOW16	symbol
 504:	c0 21 00 00 	movh \$0,0x0
			504: R_MEP_LOW16	symbol
 508:	cf 11 00 00 	movu \$sp,0x0
			508: R_MEP_LOW16	symbol
 50c:	cf 21 00 00 	movh \$sp,0x0
			50c: R_MEP_LOW16	symbol
 510:	c0 11 00 00 	movu \$0,0x0
			510: R_MEP_HI16S	symbol
 514:	c0 21 00 00 	movh \$0,0x0
			514: R_MEP_HI16S	symbol
 518:	cf 11 00 00 	movu \$sp,0x0
			518: R_MEP_HI16S	symbol
 51c:	cf 21 00 00 	movh \$sp,0x0
			51c: R_MEP_HI16S	symbol
 520:	c0 11 00 00 	movu \$0,0x0
			520: R_MEP_HI16U	symbol
 524:	c0 21 00 00 	movh \$0,0x0
			524: R_MEP_HI16U	symbol
 528:	cf 11 00 00 	movu \$sp,0x0
			528: R_MEP_HI16U	symbol
 52c:	cf 21 00 00 	movh \$sp,0x0
			52c: R_MEP_HI16U	symbol
 530:	c0 11 56 78 	movu \$0,0x5678
 534:	c0 21 56 78 	movh \$0,0x5678
 538:	cf 11 56 78 	movu \$sp,0x5678
 53c:	cf 21 56 78 	movh \$sp,0x5678
 540:	c0 11 12 34 	movu \$0,0x1234
 544:	c0 21 12 34 	movh \$0,0x1234
 548:	cf 11 12 34 	movu \$sp,0x1234
 54c:	cf 21 12 34 	movh \$sp,0x1234
 550:	c0 11 12 34 	movu \$0,0x1234
 554:	c0 21 12 34 	movh \$0,0x1234
 558:	cf 11 12 34 	movu \$sp,0x1234
 55c:	cf 21 12 34 	movh \$sp,0x1234
 560:	90 00       	add3 \$0,\$0,\$0
 562:	90 0f       	add3 \$sp,\$0,\$0
 564:	9f 00       	add3 \$0,\$sp,\$0
 566:	9f 0f       	add3 \$sp,\$sp,\$0
 568:	90 f0       	add3 \$0,\$0,\$sp
 56a:	90 ff       	add3 \$sp,\$0,\$sp
 56c:	9f f0       	add3 \$0,\$sp,\$sp
 56e:	9f ff       	add3 \$sp,\$sp,\$sp
 570:	60 c0       	add \$0,-16
 572:	6f c0       	add \$sp,-16
 574:	60 00       	add \$0,0
 576:	6f 00       	add \$sp,0
 578:	60 3c       	add \$0,15
 57a:	6f 3c       	add \$sp,15
 57c:	40 00       	add3 \$0,\$sp,0x0
 57e:	4f 00       	add3 \$sp,\$sp,0x0
 580:	40 7c       	add3 \$0,\$sp,0x7c
 582:	4f 7c       	add3 \$sp,\$sp,0x7c
 584:	c0 f0 00 01 	add3 \$0,\$sp,1
 588:	cf f0 00 01 	add3 \$sp,\$sp,1
 58c:	00 07       	advck3 \$0,\$0,\$0
 58e:	00 05       	sbvck3 \$0,\$0,\$0
 590:	0f 07       	advck3 \$0,\$sp,\$0
 592:	0f 05       	sbvck3 \$0,\$sp,\$0
 594:	00 f7       	advck3 \$0,\$0,\$sp
 596:	00 f5       	sbvck3 \$0,\$0,\$sp
 598:	0f f7       	advck3 \$0,\$sp,\$sp
 59a:	0f f5       	sbvck3 \$0,\$sp,\$sp
 59c:	00 04       	sub \$0,\$0
 59e:	00 01       	neg \$0,\$0
 5a0:	0f 04       	sub \$sp,\$0
 5a2:	0f 01       	neg \$sp,\$0
 5a4:	00 f4       	sub \$0,\$sp
 5a6:	00 f1       	neg \$0,\$sp
 5a8:	0f f4       	sub \$sp,\$sp
 5aa:	0f f1       	neg \$sp,\$sp
 5ac:	00 02       	slt3 \$0,\$0,\$0
 5ae:	00 03       	sltu3 \$0,\$0,\$0
 5b0:	20 06       	sl1ad3 \$0,\$0,\$0
 5b2:	20 07       	sl2ad3 \$0,\$0,\$0
 5b4:	0f 02       	slt3 \$0,\$sp,\$0
 5b6:	0f 03       	sltu3 \$0,\$sp,\$0
 5b8:	2f 06       	sl1ad3 \$0,\$sp,\$0
 5ba:	2f 07       	sl2ad3 \$0,\$sp,\$0
 5bc:	00 f2       	slt3 \$0,\$0,\$sp
 5be:	00 f3       	sltu3 \$0,\$0,\$sp
 5c0:	20 f6       	sl1ad3 \$0,\$0,\$sp
 5c2:	20 f7       	sl2ad3 \$0,\$0,\$sp
 5c4:	0f f2       	slt3 \$0,\$sp,\$sp
 5c6:	0f f3       	sltu3 \$0,\$sp,\$sp
 5c8:	2f f6       	sl1ad3 \$0,\$sp,\$sp
 5ca:	2f f7       	sl2ad3 \$0,\$sp,\$sp
 5cc:	c0 00 80 00 	add3 \$0,\$0,-32768
 5d0:	cf 00 80 00 	add3 \$sp,\$0,-32768
 5d4:	c0 f0 80 00 	add3 \$0,\$sp,-32768
 5d8:	cf f0 80 00 	add3 \$sp,\$sp,-32768
 5dc:	c0 00 7f ff 	add3 \$0,\$0,32767
 5e0:	cf 00 7f ff 	add3 \$sp,\$0,32767
 5e4:	c0 f0 7f ff 	add3 \$0,\$sp,32767
 5e8:	cf f0 7f ff 	add3 \$sp,\$sp,32767
 5ec:	c0 00 00 00 	add3 \$0,\$0,0
			5ec: R_MEP_LOW16	symbol
 5f0:	cf 00 00 00 	add3 \$sp,\$0,0
			5f0: R_MEP_LOW16	symbol
 5f4:	c0 f0 00 00 	add3 \$0,\$sp,0
			5f4: R_MEP_LOW16	symbol
 5f8:	cf f0 00 00 	add3 \$sp,\$sp,0
			5f8: R_MEP_LOW16	symbol
 5fc:	60 01       	slt3 \$0,\$0,0x0
 5fe:	60 05       	sltu3 \$0,\$0,0x0
 600:	6f 01       	slt3 \$0,\$sp,0x0
 602:	6f 05       	sltu3 \$0,\$sp,0x0
 604:	60 f9       	slt3 \$0,\$0,0x1f
 606:	60 fd       	sltu3 \$0,\$0,0x1f
 608:	6f f9       	slt3 \$0,\$sp,0x1f
 60a:	6f fd       	sltu3 \$0,\$sp,0x1f
 60c:	10 00       	or \$0,\$0
 60e:	10 01       	and \$0,\$0
 610:	10 02       	xor \$0,\$0
 612:	10 03       	nor \$0,\$0
 614:	1f 00       	or \$sp,\$0
 616:	1f 01       	and \$sp,\$0
 618:	1f 02       	xor \$sp,\$0
 61a:	1f 03       	nor \$sp,\$0
 61c:	10 f0       	or \$0,\$sp
 61e:	10 f1       	and \$0,\$sp
 620:	10 f2       	xor \$0,\$sp
 622:	10 f3       	nor \$0,\$sp
 624:	1f f0       	or \$sp,\$sp
 626:	1f f1       	and \$sp,\$sp
 628:	1f f2       	xor \$sp,\$sp
 62a:	1f f3       	nor \$sp,\$sp
 62c:	c0 04 00 00 	or3 \$0,\$0,0x0
 630:	c0 05 00 00 	and3 \$0,\$0,0x0
 634:	c0 06 00 00 	xor3 \$0,\$0,0x0
 638:	cf 04 00 00 	or3 \$sp,\$0,0x0
 63c:	cf 05 00 00 	and3 \$sp,\$0,0x0
 640:	cf 06 00 00 	xor3 \$sp,\$0,0x0
 644:	c0 f4 00 00 	or3 \$0,\$sp,0x0
 648:	c0 f5 00 00 	and3 \$0,\$sp,0x0
 64c:	c0 f6 00 00 	xor3 \$0,\$sp,0x0
 650:	cf f4 00 00 	or3 \$sp,\$sp,0x0
 654:	cf f5 00 00 	and3 \$sp,\$sp,0x0
 658:	cf f6 00 00 	xor3 \$sp,\$sp,0x0
 65c:	c0 04 ff ff 	or3 \$0,\$0,0xffff
 660:	c0 05 ff ff 	and3 \$0,\$0,0xffff
 664:	c0 06 ff ff 	xor3 \$0,\$0,0xffff
 668:	cf 04 ff ff 	or3 \$sp,\$0,0xffff
 66c:	cf 05 ff ff 	and3 \$sp,\$0,0xffff
 670:	cf 06 ff ff 	xor3 \$sp,\$0,0xffff
 674:	c0 f4 ff ff 	or3 \$0,\$sp,0xffff
 678:	c0 f5 ff ff 	and3 \$0,\$sp,0xffff
 67c:	c0 f6 ff ff 	xor3 \$0,\$sp,0xffff
 680:	cf f4 ff ff 	or3 \$sp,\$sp,0xffff
 684:	cf f5 ff ff 	and3 \$sp,\$sp,0xffff
 688:	cf f6 ff ff 	xor3 \$sp,\$sp,0xffff
 68c:	c0 04 00 00 	or3 \$0,\$0,0x0
			68c: R_MEP_LOW16	symbol
 690:	c0 05 00 00 	and3 \$0,\$0,0x0
			690: R_MEP_LOW16	symbol
 694:	c0 06 00 00 	xor3 \$0,\$0,0x0
			694: R_MEP_LOW16	symbol
 698:	cf 04 00 00 	or3 \$sp,\$0,0x0
			698: R_MEP_LOW16	symbol
 69c:	cf 05 00 00 	and3 \$sp,\$0,0x0
			69c: R_MEP_LOW16	symbol
 6a0:	cf 06 00 00 	xor3 \$sp,\$0,0x0
			6a0: R_MEP_LOW16	symbol
 6a4:	c0 f4 00 00 	or3 \$0,\$sp,0x0
			6a4: R_MEP_LOW16	symbol
 6a8:	c0 f5 00 00 	and3 \$0,\$sp,0x0
			6a8: R_MEP_LOW16	symbol
 6ac:	c0 f6 00 00 	xor3 \$0,\$sp,0x0
			6ac: R_MEP_LOW16	symbol
 6b0:	cf f4 00 00 	or3 \$sp,\$sp,0x0
			6b0: R_MEP_LOW16	symbol
 6b4:	cf f5 00 00 	and3 \$sp,\$sp,0x0
			6b4: R_MEP_LOW16	symbol
 6b8:	cf f6 00 00 	xor3 \$sp,\$sp,0x0
			6b8: R_MEP_LOW16	symbol
 6bc:	20 0d       	sra \$0,\$0
 6be:	20 0c       	srl \$0,\$0
 6c0:	20 0e       	sll \$0,\$0
 6c2:	20 0f       	fsft \$0,\$0
 6c4:	2f 0d       	sra \$sp,\$0
 6c6:	2f 0c       	srl \$sp,\$0
 6c8:	2f 0e       	sll \$sp,\$0
 6ca:	2f 0f       	fsft \$sp,\$0
 6cc:	20 fd       	sra \$0,\$sp
 6ce:	20 fc       	srl \$0,\$sp
 6d0:	20 fe       	sll \$0,\$sp
 6d2:	20 ff       	fsft \$0,\$sp
 6d4:	2f fd       	sra \$sp,\$sp
 6d6:	2f fc       	srl \$sp,\$sp
 6d8:	2f fe       	sll \$sp,\$sp
 6da:	2f ff       	fsft \$sp,\$sp
 6dc:	60 03       	sra \$0,0x0
 6de:	60 02       	srl \$0,0x0
 6e0:	60 06       	sll \$0,0x0
 6e2:	6f 03       	sra \$sp,0x0
 6e4:	6f 02       	srl \$sp,0x0
 6e6:	6f 06       	sll \$sp,0x0
 6e8:	60 fb       	sra \$0,0x1f
 6ea:	60 fa       	srl \$0,0x1f
 6ec:	60 fe       	sll \$0,0x1f
 6ee:	6f fb       	sra \$sp,0x1f
 6f0:	6f fa       	srl \$sp,0x1f
 6f2:	6f fe       	sll \$sp,0x1f
 6f4:	60 07       	sll3 \$0,\$0,0x0
 6f6:	6f 07       	sll3 \$0,\$sp,0x0
 6f8:	60 ff       	sll3 \$0,\$0,0x1f
 6fa:	6f ff       	sll3 \$0,\$sp,0x1f
 6fc:	b8 02       	bra 0xfffffefe
 6fe:	e0 01 04 00 	beq \$0,\$0,0xefe
 702:	b0 00       	bra 0x702
			702: R_MEP_PCREL12A2	symbol
 704:	a0 82       	beqz \$0,0x686
 706:	a0 83       	bnez \$0,0x688
 708:	af 82       	beqz \$sp,0x68a
 70a:	af 83       	bnez \$sp,0x68c
 70c:	e0 00 00 40 	beqi \$0,0x0,0x78c
 710:	e0 04 00 40 	bnei \$0,0x0,0x790
 714:	ef 00 00 40 	beqi \$sp,0x0,0x794
 718:	ef 04 00 40 	bnei \$sp,0x0,0x798
 71c:	a0 00       	beqz \$0,0x71c
			71c: R_MEP_PCREL8A2	symbol
 71e:	a0 01       	bnez \$0,0x71e
			71e: R_MEP_PCREL8A2	symbol
 720:	af 00       	beqz \$sp,0x720
			720: R_MEP_PCREL8A2	symbol
 722:	af 01       	bnez \$sp,0x722
			722: R_MEP_PCREL8A2	symbol
 724:	e0 00 80 02 	beqi \$0,0x0,0xffff0728
 728:	e0 04 80 02 	bnei \$0,0x0,0xffff072c
 72c:	e0 0c 80 02 	blti \$0,0x0,0xffff0730
 730:	e0 08 80 02 	bgei \$0,0x0,0xffff0734
 734:	ef 00 80 02 	beqi \$sp,0x0,0xffff0738
 738:	ef 04 80 02 	bnei \$sp,0x0,0xffff073c
 73c:	ef 0c 80 02 	blti \$sp,0x0,0xffff0740
 740:	ef 08 80 02 	bgei \$sp,0x0,0xffff0744
 744:	e0 f0 80 02 	beqi \$0,0xf,0xffff0748
 748:	e0 f4 80 02 	bnei \$0,0xf,0xffff074c
 74c:	e0 fc 80 02 	blti \$0,0xf,0xffff0750
 750:	e0 f8 80 02 	bgei \$0,0xf,0xffff0754
 754:	ef f0 80 02 	beqi \$sp,0xf,0xffff0758
 758:	ef f4 80 02 	bnei \$sp,0xf,0xffff075c
 75c:	ef fc 80 02 	blti \$sp,0xf,0xffff0760
 760:	ef f8 80 02 	bgei \$sp,0xf,0xffff0764
 764:	e0 00 3f ff 	beqi \$0,0x0,0x8762
 768:	e0 04 3f ff 	bnei \$0,0x0,0x8766
 76c:	e0 0c 3f ff 	blti \$0,0x0,0x876a
 770:	e0 08 3f ff 	bgei \$0,0x0,0x876e
 774:	ef 00 3f ff 	beqi \$sp,0x0,0x8772
 778:	ef 04 3f ff 	bnei \$sp,0x0,0x8776
 77c:	ef 0c 3f ff 	blti \$sp,0x0,0x877a
 780:	ef 08 3f ff 	bgei \$sp,0x0,0x877e
 784:	e0 f0 3f ff 	beqi \$0,0xf,0x8782
 788:	e0 f4 3f ff 	bnei \$0,0xf,0x8786
 78c:	e0 fc 3f ff 	blti \$0,0xf,0x878a
 790:	e0 f8 3f ff 	bgei \$0,0xf,0x878e
 794:	ef f0 3f ff 	beqi \$sp,0xf,0x8792
 798:	ef f4 3f ff 	bnei \$sp,0xf,0x8796
 79c:	ef fc 3f ff 	blti \$sp,0xf,0x879a
 7a0:	ef f8 3f ff 	bgei \$sp,0xf,0x879e
 7a4:	e0 00 00 00 	beqi \$0,0x0,0x7a4
			7a4: R_MEP_PCREL17A2	symbol
 7a8:	e0 04 00 00 	bnei \$0,0x0,0x7a8
			7a8: R_MEP_PCREL17A2	symbol
 7ac:	e0 0c 00 00 	blti \$0,0x0,0x7ac
			7ac: R_MEP_PCREL17A2	symbol
 7b0:	e0 08 00 00 	bgei \$0,0x0,0x7b0
			7b0: R_MEP_PCREL17A2	symbol
 7b4:	ef 00 00 00 	beqi \$sp,0x0,0x7b4
			7b4: R_MEP_PCREL17A2	symbol
 7b8:	ef 04 00 00 	bnei \$sp,0x0,0x7b8
			7b8: R_MEP_PCREL17A2	symbol
 7bc:	ef 0c 00 00 	blti \$sp,0x0,0x7bc
			7bc: R_MEP_PCREL17A2	symbol
 7c0:	ef 08 00 00 	bgei \$sp,0x0,0x7c0
			7c0: R_MEP_PCREL17A2	symbol
 7c4:	e0 f0 00 00 	beqi \$0,0xf,0x7c4
			7c4: R_MEP_PCREL17A2	symbol
 7c8:	e0 f4 00 00 	bnei \$0,0xf,0x7c8
			7c8: R_MEP_PCREL17A2	symbol
 7cc:	e0 fc 00 00 	blti \$0,0xf,0x7cc
			7cc: R_MEP_PCREL17A2	symbol
 7d0:	e0 f8 00 00 	bgei \$0,0xf,0x7d0
			7d0: R_MEP_PCREL17A2	symbol
 7d4:	ef f0 00 00 	beqi \$sp,0xf,0x7d4
			7d4: R_MEP_PCREL17A2	symbol
 7d8:	ef f4 00 00 	bnei \$sp,0xf,0x7d8
			7d8: R_MEP_PCREL17A2	symbol
 7dc:	ef fc 00 00 	blti \$sp,0xf,0x7dc
			7dc: R_MEP_PCREL17A2	symbol
 7e0:	ef f8 00 00 	bgei \$sp,0xf,0x7e0
			7e0: R_MEP_PCREL17A2	symbol
 7e4:	e0 01 80 02 	beq \$0,\$0,0xffff07e8
 7e8:	e0 05 80 02 	bne \$0,\$0,0xffff07ec
 7ec:	ef 01 80 02 	beq \$sp,\$0,0xffff07f0
 7f0:	ef 05 80 02 	bne \$sp,\$0,0xffff07f4
 7f4:	e0 f1 80 02 	beq \$0,\$sp,0xffff07f8
 7f8:	e0 f5 80 02 	bne \$0,\$sp,0xffff07fc
 7fc:	ef f1 80 02 	beq \$sp,\$sp,0xffff0800
 800:	ef f5 80 02 	bne \$sp,\$sp,0xffff0804
 804:	e0 01 3f ff 	beq \$0,\$0,0x8802
 808:	e0 05 3f ff 	bne \$0,\$0,0x8806
 80c:	ef 01 3f ff 	beq \$sp,\$0,0x880a
 810:	ef 05 3f ff 	bne \$sp,\$0,0x880e
 814:	e0 f1 3f ff 	beq \$0,\$sp,0x8812
 818:	e0 f5 3f ff 	bne \$0,\$sp,0x8816
 81c:	ef f1 3f ff 	beq \$sp,\$sp,0x881a
 820:	ef f5 3f ff 	bne \$sp,\$sp,0x881e
 824:	e0 01 00 00 	beq \$0,\$0,0x824
			824: R_MEP_PCREL17A2	symbol
 828:	e0 05 00 00 	bne \$0,\$0,0x828
			828: R_MEP_PCREL17A2	symbol
 82c:	ef 01 00 00 	beq \$sp,\$0,0x82c
			82c: R_MEP_PCREL17A2	symbol
 830:	ef 05 00 00 	bne \$sp,\$0,0x830
			830: R_MEP_PCREL17A2	symbol
 834:	e0 f1 00 00 	beq \$0,\$sp,0x834
			834: R_MEP_PCREL17A2	symbol
 838:	e0 f5 00 00 	bne \$0,\$sp,0x838
			838: R_MEP_PCREL17A2	symbol
 83c:	ef f1 00 00 	beq \$sp,\$sp,0x83c
			83c: R_MEP_PCREL17A2	symbol
 840:	ef f5 00 00 	bne \$sp,\$sp,0x840
			840: R_MEP_PCREL17A2	symbol
 844:	d8 29 80 00 	bsr 0xff800848
 848:	b8 03       	bsr 0x4a
 84a:	d8 09 00 08 	bsr 0x104a
 84e:	d8 19 80 00 	bsr 0xff800850
 852:	d8 09 00 00 	bsr 0x852
			852: R_MEP_PCREL24A2	symbol
 856:	10 0e       	jmp \$0
 858:	10 fe       	jmp \$sp
 85a:	d8 08 00 00 	jmp 0x0
 85e:	df f8 ff ff 	jmp 0xfffffe
 862:	d8 08 00 00 	jmp 0x0
			862: R_MEP_PCABS24A2	symbol
 866:	10 0f       	jsr \$0
 868:	10 ff       	jsr \$sp
 86a:	70 02       	ret
 86c:	e0 09 80 02 	repeat \$0,0xffff0870
 870:	ef 09 80 02 	repeat \$sp,0xffff0874
 874:	e0 09 3f ff 	repeat \$0,0x8872
 878:	ef 09 3f ff 	repeat \$sp,0x8876
 87c:	e0 09 00 00 	repeat \$0,0x87c
			87c: R_MEP_PCREL17A2	symbol
 880:	ef 09 00 00 	repeat \$sp,0x880
			880: R_MEP_PCREL17A2	symbol
 884:	e0 19 80 02 	erepeat 0xffff0888
 888:	e0 19 3f ff 	erepeat 0x8886
 88c:	e0 19 00 00 	erepeat 0x88c
			88c: R_MEP_PCREL17A2	symbol
 890:	70 08       	stc \$0,\$pc
 892:	70 0a       	ldc \$0,\$pc
 894:	7f 08       	stc \$sp,\$pc
 896:	7f 0a       	ldc \$sp,\$pc
 898:	70 18       	stc \$0,\$lp
 89a:	70 1a       	ldc \$0,\$lp
 89c:	7f 18       	stc \$sp,\$lp
 89e:	7f 1a       	ldc \$sp,\$lp
 8a0:	70 28       	stc \$0,\$sar
 8a2:	70 2a       	ldc \$0,\$sar
 8a4:	7f 28       	stc \$sp,\$sar
 8a6:	7f 2a       	ldc \$sp,\$sar
 8a8:	70 48       	stc \$0,\$rpb
 8aa:	70 4a       	ldc \$0,\$rpb
 8ac:	7f 48       	stc \$sp,\$rpb
 8ae:	7f 4a       	ldc \$sp,\$rpb
 8b0:	70 58       	stc \$0,\$rpe
 8b2:	70 5a       	ldc \$0,\$rpe
 8b4:	7f 58       	stc \$sp,\$rpe
 8b6:	7f 5a       	ldc \$sp,\$rpe
 8b8:	70 68       	stc \$0,\$rpc
 8ba:	70 6a       	ldc \$0,\$rpc
 8bc:	7f 68       	stc \$sp,\$rpc
 8be:	7f 6a       	ldc \$sp,\$rpc
 8c0:	70 78       	stc \$0,\$hi
 8c2:	70 7a       	ldc \$0,\$hi
 8c4:	7f 78       	stc \$sp,\$hi
 8c6:	7f 7a       	ldc \$sp,\$hi
 8c8:	70 88       	stc \$0,\$lo
 8ca:	70 8a       	ldc \$0,\$lo
 8cc:	7f 88       	stc \$sp,\$lo
 8ce:	7f 8a       	ldc \$sp,\$lo
 8d0:	70 c8       	stc \$0,\$mb0
 8d2:	70 ca       	ldc \$0,\$mb0
 8d4:	7f c8       	stc \$sp,\$mb0
 8d6:	7f ca       	ldc \$sp,\$mb0
 8d8:	70 d8       	stc \$0,\$me0
 8da:	70 da       	ldc \$0,\$me0
 8dc:	7f d8       	stc \$sp,\$me0
 8de:	7f da       	ldc \$sp,\$me0
 8e0:	70 e8       	stc \$0,\$mb1
 8e2:	70 ea       	ldc \$0,\$mb1
 8e4:	7f e8       	stc \$sp,\$mb1
 8e6:	7f ea       	ldc \$sp,\$mb1
 8e8:	70 f8       	stc \$0,\$me1
 8ea:	70 fa       	ldc \$0,\$me1
 8ec:	7f f8       	stc \$sp,\$me1
 8ee:	7f fa       	ldc \$sp,\$me1
 8f0:	70 09       	stc \$0,\$psw
 8f2:	70 0b       	ldc \$0,\$psw
 8f4:	7f 09       	stc \$sp,\$psw
 8f6:	7f 0b       	ldc \$sp,\$psw
 8f8:	70 19       	stc \$0,\$id
 8fa:	70 1b       	ldc \$0,\$id
 8fc:	7f 19       	stc \$sp,\$id
 8fe:	7f 1b       	ldc \$sp,\$id
 900:	70 29       	stc \$0,\$tmp
 902:	70 2b       	ldc \$0,\$tmp
 904:	7f 29       	stc \$sp,\$tmp
 906:	7f 2b       	ldc \$sp,\$tmp
 908:	70 39       	stc \$0,\$epc
 90a:	70 3b       	ldc \$0,\$epc
 90c:	7f 39       	stc \$sp,\$epc
 90e:	7f 3b       	ldc \$sp,\$epc
 910:	70 49       	stc \$0,\$exc
 912:	70 4b       	ldc \$0,\$exc
 914:	7f 49       	stc \$sp,\$exc
 916:	7f 4b       	ldc \$sp,\$exc
 918:	70 59       	stc \$0,\$cfg
 91a:	70 5b       	ldc \$0,\$cfg
 91c:	7f 59       	stc \$sp,\$cfg
 91e:	7f 5b       	ldc \$sp,\$cfg
 920:	70 79       	stc \$0,\$npc
 922:	70 7b       	ldc \$0,\$npc
 924:	7f 79       	stc \$sp,\$npc
 926:	7f 7b       	ldc \$sp,\$npc
 928:	70 89       	stc \$0,\$dbg
 92a:	70 8b       	ldc \$0,\$dbg
 92c:	7f 89       	stc \$sp,\$dbg
 92e:	7f 8b       	ldc \$sp,\$dbg
 930:	70 99       	stc \$0,\$depc
 932:	70 9b       	ldc \$0,\$depc
 934:	7f 99       	stc \$sp,\$depc
 936:	7f 9b       	ldc \$sp,\$depc
 938:	70 a9       	stc \$0,\$opt
 93a:	70 ab       	ldc \$0,\$opt
 93c:	7f a9       	stc \$sp,\$opt
 93e:	7f ab       	ldc \$sp,\$opt
 940:	70 b9       	stc \$0,\$rcfg
 942:	70 bb       	ldc \$0,\$rcfg
 944:	7f b9       	stc \$sp,\$rcfg
 946:	7f bb       	ldc \$sp,\$rcfg
 948:	70 c9       	stc \$0,\$ccfg
 94a:	70 cb       	ldc \$0,\$ccfg
 94c:	7f c9       	stc \$sp,\$ccfg
 94e:	7f cb       	ldc \$sp,\$ccfg
 950:	70 00       	di
 952:	70 10       	ei
 954:	70 12       	reti
 956:	70 22       	halt
 958:	70 32       	break
 95a:	70 11       	syncm
 95c:	70 06       	swi 0x0
 95e:	70 36       	swi 0x3
 960:	f0 04 00 00 	stcb \$0,0x0
 964:	f0 14 00 00 	ldcb \$0,0x0
 968:	ff 04 00 00 	stcb \$sp,0x0
 96c:	ff 14 00 00 	ldcb \$sp,0x0
 970:	f0 04 ff ff 	stcb \$0,0xffff
 974:	f0 14 ff ff 	ldcb \$0,0xffff
 978:	ff 04 ff ff 	stcb \$sp,0xffff
 97c:	ff 14 ff ff 	ldcb \$sp,0xffff
 980:	f0 04 00 00 	stcb \$0,0x0
			982: R_MEP_16	symbol
 984:	f0 14 00 00 	ldcb \$0,0x0
			986: R_MEP_16	symbol
 988:	ff 04 00 00 	stcb \$sp,0x0
			98a: R_MEP_16	symbol
 98c:	ff 14 00 00 	ldcb \$sp,0x0
			98e: R_MEP_16	symbol
 990:	20 00       	bsetm \(\$0\),0x0
 992:	20 01       	bclrm \(\$0\),0x0
 994:	20 02       	bnotm \(\$0\),0x0
 996:	20 f0       	bsetm \(\$sp\),0x0
 998:	20 f1       	bclrm \(\$sp\),0x0
 99a:	20 f2       	bnotm \(\$sp\),0x0
 99c:	27 00       	bsetm \(\$0\),0x7
 99e:	27 01       	bclrm \(\$0\),0x7
 9a0:	27 02       	bnotm \(\$0\),0x7
 9a2:	27 f0       	bsetm \(\$sp\),0x7
 9a4:	27 f1       	bclrm \(\$sp\),0x7
 9a6:	27 f2       	bnotm \(\$sp\),0x7
 9a8:	20 03       	btstm \$0,\(\$0\),0x0
 9aa:	20 f3       	btstm \$0,\(\$sp\),0x0
 9ac:	27 03       	btstm \$0,\(\$0\),0x7
 9ae:	27 f3       	btstm \$0,\(\$sp\),0x7
 9b0:	20 04       	tas \$0,\(\$0\)
 9b2:	2f 04       	tas \$sp,\(\$0\)
 9b4:	20 f4       	tas \$0,\(\$sp\)
 9b6:	2f f4       	tas \$sp,\(\$sp\)
 9b8:	70 04       	cache 0x0,\(\$0\)
 9ba:	73 04       	cache 0x3,\(\$0\)
 9bc:	70 f4       	cache 0x0,\(\$sp\)
 9be:	73 f4       	cache 0x3,\(\$sp\)
 9c0:	10 04       	mul \$0,\$0
 9c2:	f0 01 30 04 	madd \$0,\$0
 9c6:	10 06       	mulr \$0,\$0
 9c8:	f0 01 30 06 	maddr \$0,\$0
 9cc:	10 05       	mulu \$0,\$0
 9ce:	f0 01 30 05 	maddu \$0,\$0
 9d2:	10 07       	mulru \$0,\$0
 9d4:	f0 01 30 07 	maddru \$0,\$0
 9d8:	1f 04       	mul \$sp,\$0
 9da:	ff 01 30 04 	madd \$sp,\$0
 9de:	1f 06       	mulr \$sp,\$0
 9e0:	ff 01 30 06 	maddr \$sp,\$0
 9e4:	1f 05       	mulu \$sp,\$0
 9e6:	ff 01 30 05 	maddu \$sp,\$0
 9ea:	1f 07       	mulru \$sp,\$0
 9ec:	ff 01 30 07 	maddru \$sp,\$0
 9f0:	10 f4       	mul \$0,\$sp
 9f2:	f0 f1 30 04 	madd \$0,\$sp
 9f6:	10 f6       	mulr \$0,\$sp
 9f8:	f0 f1 30 06 	maddr \$0,\$sp
 9fc:	10 f5       	mulu \$0,\$sp
 9fe:	f0 f1 30 05 	maddu \$0,\$sp
 a02:	10 f7       	mulru \$0,\$sp
 a04:	f0 f1 30 07 	maddru \$0,\$sp
 a08:	1f f4       	mul \$sp,\$sp
 a0a:	ff f1 30 04 	madd \$sp,\$sp
 a0e:	1f f6       	mulr \$sp,\$sp
 a10:	ff f1 30 06 	maddr \$sp,\$sp
 a14:	1f f5       	mulu \$sp,\$sp
 a16:	ff f1 30 05 	maddu \$sp,\$sp
 a1a:	1f f7       	mulru \$sp,\$sp
 a1c:	ff f1 30 07 	maddru \$sp,\$sp
 a20:	10 08       	div \$0,\$0
 a22:	10 09       	divu \$0,\$0
 a24:	1f 08       	div \$sp,\$0
 a26:	1f 09       	divu \$sp,\$0
 a28:	10 f8       	div \$0,\$sp
 a2a:	10 f9       	divu \$0,\$sp
 a2c:	1f f8       	div \$sp,\$sp
 a2e:	1f f9       	divu \$sp,\$sp
 a30:	70 13       	dret
 a32:	70 33       	dbreak
 a34:	f0 01 00 00 	ldz \$0,\$0
 a38:	f0 01 00 03 	abs \$0,\$0
 a3c:	f0 01 00 02 	ave \$0,\$0
 a40:	ff 01 00 00 	ldz \$sp,\$0
 a44:	ff 01 00 03 	abs \$sp,\$0
 a48:	ff 01 00 02 	ave \$sp,\$0
 a4c:	f0 f1 00 00 	ldz \$0,\$sp
 a50:	f0 f1 00 03 	abs \$0,\$sp
 a54:	f0 f1 00 02 	ave \$0,\$sp
 a58:	ff f1 00 00 	ldz \$sp,\$sp
 a5c:	ff f1 00 03 	abs \$sp,\$sp
 a60:	ff f1 00 02 	ave \$sp,\$sp
 a64:	f0 01 00 04 	min \$0,\$0
 a68:	f0 01 00 05 	max \$0,\$0
 a6c:	f0 01 00 06 	minu \$0,\$0
 a70:	f0 01 00 07 	maxu \$0,\$0
 a74:	ff 01 00 04 	min \$sp,\$0
 a78:	ff 01 00 05 	max \$sp,\$0
 a7c:	ff 01 00 06 	minu \$sp,\$0
 a80:	ff 01 00 07 	maxu \$sp,\$0
 a84:	f0 f1 00 04 	min \$0,\$sp
 a88:	f0 f1 00 05 	max \$0,\$sp
 a8c:	f0 f1 00 06 	minu \$0,\$sp
 a90:	f0 f1 00 07 	maxu \$0,\$sp
 a94:	ff f1 00 04 	min \$sp,\$sp
 a98:	ff f1 00 05 	max \$sp,\$sp
 a9c:	ff f1 00 06 	minu \$sp,\$sp
 aa0:	ff f1 00 07 	maxu \$sp,\$sp
 aa4:	f0 01 10 00 	clip \$0,0x0
 aa8:	f0 01 10 01 	clipu \$0,0x0
 aac:	ff 01 10 00 	clip \$sp,0x0
 ab0:	ff 01 10 01 	clipu \$sp,0x0
 ab4:	f0 01 10 f8 	clip \$0,0x1f
 ab8:	f0 01 10 f9 	clipu \$0,0x1f
 abc:	ff 01 10 f8 	clip \$sp,0x1f
 ac0:	ff 01 10 f9 	clipu \$sp,0x1f
 ac4:	f0 01 00 08 	sadd \$0,\$0
 ac8:	f0 01 00 0a 	ssub \$0,\$0
 acc:	f0 01 00 09 	saddu \$0,\$0
 ad0:	f0 01 00 0b 	ssubu \$0,\$0
 ad4:	ff 01 00 08 	sadd \$sp,\$0
 ad8:	ff 01 00 0a 	ssub \$sp,\$0
 adc:	ff 01 00 09 	saddu \$sp,\$0
 ae0:	ff 01 00 0b 	ssubu \$sp,\$0
 ae4:	f0 f1 00 08 	sadd \$0,\$sp
 ae8:	f0 f1 00 0a 	ssub \$0,\$sp
 aec:	f0 f1 00 09 	saddu \$0,\$sp
 af0:	f0 f1 00 0b 	ssubu \$0,\$sp
 af4:	ff f1 00 08 	sadd \$sp,\$sp
 af8:	ff f1 00 0a 	ssub \$sp,\$sp
 afc:	ff f1 00 09 	saddu \$sp,\$sp
 b00:	ff f1 00 0b 	ssubu \$sp,\$sp
 b04:	30 08       	swcp \$c0,\(\$0\)
 b06:	30 09       	lwcp \$c0,\(\$0\)
 b08:	30 0a       	smcp \$c0,\(\$0\)
 b0a:	30 0b       	lmcp \$c0,\(\$0\)
 b0c:	3f 08       	swcp \$c15,\(\$0\)
 b0e:	3f 09       	lwcp \$c15,\(\$0\)
 b10:	3f 0a       	smcp \$c15,\(\$0\)
 b12:	3f 0b       	lmcp \$c15,\(\$0\)
 b14:	30 f8       	swcp \$c0,\(\$sp\)
 b16:	30 f9       	lwcp \$c0,\(\$sp\)
 b18:	30 fa       	smcp \$c0,\(\$sp\)
 b1a:	30 fb       	lmcp \$c0,\(\$sp\)
 b1c:	3f f8       	swcp \$c15,\(\$sp\)
 b1e:	3f f9       	lwcp \$c15,\(\$sp\)
 b20:	3f fa       	smcp \$c15,\(\$sp\)
 b22:	3f fb       	lmcp \$c15,\(\$sp\)
 b24:	30 00       	swcpi \$c0,\(\$0\+\)
 b26:	30 01       	lwcpi \$c0,\(\$0\+\)
 b28:	30 02       	smcpi \$c0,\(\$0\+\)
 b2a:	30 03       	lmcpi \$c0,\(\$0\+\)
 b2c:	3f 00       	swcpi \$c15,\(\$0\+\)
 b2e:	3f 01       	lwcpi \$c15,\(\$0\+\)
 b30:	3f 02       	smcpi \$c15,\(\$0\+\)
 b32:	3f 03       	lmcpi \$c15,\(\$0\+\)
 b34:	30 f0       	swcpi \$c0,\(\$sp\+\)
 b36:	30 f1       	lwcpi \$c0,\(\$sp\+\)
 b38:	30 f2       	smcpi \$c0,\(\$sp\+\)
 b3a:	30 f3       	lmcpi \$c0,\(\$sp\+\)
 b3c:	3f f0       	swcpi \$c15,\(\$sp\+\)
 b3e:	3f f1       	lwcpi \$c15,\(\$sp\+\)
 b40:	3f f2       	smcpi \$c15,\(\$sp\+\)
 b42:	3f f3       	lmcpi \$c15,\(\$sp\+\)
 b44:	f0 05 00 80 	sbcpa \$c0,\(\$0\+\),-128
 b48:	f0 05 40 80 	lbcpa \$c0,\(\$0\+\),-128
 b4c:	f0 05 08 80 	sbcpm0 \$c0,\(\$0\+\),-128
 b50:	f0 05 48 80 	lbcpm0 \$c0,\(\$0\+\),-128
 b54:	f0 05 0c 80 	sbcpm1 \$c0,\(\$0\+\),-128
 b58:	f0 05 4c 80 	lbcpm1 \$c0,\(\$0\+\),-128
 b5c:	ff 05 00 80 	sbcpa \$c15,\(\$0\+\),-128
 b60:	ff 05 40 80 	lbcpa \$c15,\(\$0\+\),-128
 b64:	ff 05 08 80 	sbcpm0 \$c15,\(\$0\+\),-128
 b68:	ff 05 48 80 	lbcpm0 \$c15,\(\$0\+\),-128
 b6c:	ff 05 0c 80 	sbcpm1 \$c15,\(\$0\+\),-128
 b70:	ff 05 4c 80 	lbcpm1 \$c15,\(\$0\+\),-128
 b74:	f0 f5 00 80 	sbcpa \$c0,\(\$sp\+\),-128
 b78:	f0 f5 40 80 	lbcpa \$c0,\(\$sp\+\),-128
 b7c:	f0 f5 08 80 	sbcpm0 \$c0,\(\$sp\+\),-128
 b80:	f0 f5 48 80 	lbcpm0 \$c0,\(\$sp\+\),-128
 b84:	f0 f5 0c 80 	sbcpm1 \$c0,\(\$sp\+\),-128
 b88:	f0 f5 4c 80 	lbcpm1 \$c0,\(\$sp\+\),-128
 b8c:	ff f5 00 80 	sbcpa \$c15,\(\$sp\+\),-128
 b90:	ff f5 40 80 	lbcpa \$c15,\(\$sp\+\),-128
 b94:	ff f5 08 80 	sbcpm0 \$c15,\(\$sp\+\),-128
 b98:	ff f5 48 80 	lbcpm0 \$c15,\(\$sp\+\),-128
 b9c:	ff f5 0c 80 	sbcpm1 \$c15,\(\$sp\+\),-128
 ba0:	ff f5 4c 80 	lbcpm1 \$c15,\(\$sp\+\),-128
 ba4:	f0 05 00 7f 	sbcpa \$c0,\(\$0\+\),127
 ba8:	f0 05 40 7f 	lbcpa \$c0,\(\$0\+\),127
 bac:	f0 05 08 7f 	sbcpm0 \$c0,\(\$0\+\),127
 bb0:	f0 05 48 7f 	lbcpm0 \$c0,\(\$0\+\),127
 bb4:	f0 05 0c 7f 	sbcpm1 \$c0,\(\$0\+\),127
 bb8:	f0 05 4c 7f 	lbcpm1 \$c0,\(\$0\+\),127
 bbc:	ff 05 00 7f 	sbcpa \$c15,\(\$0\+\),127
 bc0:	ff 05 40 7f 	lbcpa \$c15,\(\$0\+\),127
 bc4:	ff 05 08 7f 	sbcpm0 \$c15,\(\$0\+\),127
 bc8:	ff 05 48 7f 	lbcpm0 \$c15,\(\$0\+\),127
 bcc:	ff 05 0c 7f 	sbcpm1 \$c15,\(\$0\+\),127
 bd0:	ff 05 4c 7f 	lbcpm1 \$c15,\(\$0\+\),127
 bd4:	f0 f5 00 7f 	sbcpa \$c0,\(\$sp\+\),127
 bd8:	f0 f5 40 7f 	lbcpa \$c0,\(\$sp\+\),127
 bdc:	f0 f5 08 7f 	sbcpm0 \$c0,\(\$sp\+\),127
 be0:	f0 f5 48 7f 	lbcpm0 \$c0,\(\$sp\+\),127
 be4:	f0 f5 0c 7f 	sbcpm1 \$c0,\(\$sp\+\),127
 be8:	f0 f5 4c 7f 	lbcpm1 \$c0,\(\$sp\+\),127
 bec:	ff f5 00 7f 	sbcpa \$c15,\(\$sp\+\),127
 bf0:	ff f5 40 7f 	lbcpa \$c15,\(\$sp\+\),127
 bf4:	ff f5 08 7f 	sbcpm0 \$c15,\(\$sp\+\),127
 bf8:	ff f5 48 7f 	lbcpm0 \$c15,\(\$sp\+\),127
 bfc:	ff f5 0c 7f 	sbcpm1 \$c15,\(\$sp\+\),127
 c00:	ff f5 4c 7f 	lbcpm1 \$c15,\(\$sp\+\),127
 c04:	f0 05 10 80 	shcpa \$c0,\(\$0\+\),-128
 c08:	f0 05 50 80 	lhcpa \$c0,\(\$0\+\),-128
 c0c:	f0 05 18 80 	shcpm0 \$c0,\(\$0\+\),-128
 c10:	f0 05 58 80 	lhcpm0 \$c0,\(\$0\+\),-128
 c14:	f0 05 1c 80 	shcpm1 \$c0,\(\$0\+\),-128
 c18:	f0 05 5c 80 	lhcpm1 \$c0,\(\$0\+\),-128
 c1c:	ff 05 10 80 	shcpa \$c15,\(\$0\+\),-128
 c20:	ff 05 50 80 	lhcpa \$c15,\(\$0\+\),-128
 c24:	ff 05 18 80 	shcpm0 \$c15,\(\$0\+\),-128
 c28:	ff 05 58 80 	lhcpm0 \$c15,\(\$0\+\),-128
 c2c:	ff 05 1c 80 	shcpm1 \$c15,\(\$0\+\),-128
 c30:	ff 05 5c 80 	lhcpm1 \$c15,\(\$0\+\),-128
 c34:	f0 f5 10 80 	shcpa \$c0,\(\$sp\+\),-128
 c38:	f0 f5 50 80 	lhcpa \$c0,\(\$sp\+\),-128
 c3c:	f0 f5 18 80 	shcpm0 \$c0,\(\$sp\+\),-128
 c40:	f0 f5 58 80 	lhcpm0 \$c0,\(\$sp\+\),-128
 c44:	f0 f5 1c 80 	shcpm1 \$c0,\(\$sp\+\),-128
 c48:	f0 f5 5c 80 	lhcpm1 \$c0,\(\$sp\+\),-128
 c4c:	ff f5 10 80 	shcpa \$c15,\(\$sp\+\),-128
 c50:	ff f5 50 80 	lhcpa \$c15,\(\$sp\+\),-128
 c54:	ff f5 18 80 	shcpm0 \$c15,\(\$sp\+\),-128
 c58:	ff f5 58 80 	lhcpm0 \$c15,\(\$sp\+\),-128
 c5c:	ff f5 1c 80 	shcpm1 \$c15,\(\$sp\+\),-128
 c60:	ff f5 5c 80 	lhcpm1 \$c15,\(\$sp\+\),-128
 c64:	f0 05 10 7e 	shcpa \$c0,\(\$0\+\),126
 c68:	f0 05 50 7e 	lhcpa \$c0,\(\$0\+\),126
 c6c:	f0 05 18 7e 	shcpm0 \$c0,\(\$0\+\),126
 c70:	f0 05 58 7e 	lhcpm0 \$c0,\(\$0\+\),126
 c74:	f0 05 1c 7e 	shcpm1 \$c0,\(\$0\+\),126
 c78:	f0 05 5c 7e 	lhcpm1 \$c0,\(\$0\+\),126
 c7c:	ff 05 10 7e 	shcpa \$c15,\(\$0\+\),126
 c80:	ff 05 50 7e 	lhcpa \$c15,\(\$0\+\),126
 c84:	ff 05 18 7e 	shcpm0 \$c15,\(\$0\+\),126
 c88:	ff 05 58 7e 	lhcpm0 \$c15,\(\$0\+\),126
 c8c:	ff 05 1c 7e 	shcpm1 \$c15,\(\$0\+\),126
 c90:	ff 05 5c 7e 	lhcpm1 \$c15,\(\$0\+\),126
 c94:	f0 f5 10 7e 	shcpa \$c0,\(\$sp\+\),126
 c98:	f0 f5 50 7e 	lhcpa \$c0,\(\$sp\+\),126
 c9c:	f0 f5 18 7e 	shcpm0 \$c0,\(\$sp\+\),126
 ca0:	f0 f5 58 7e 	lhcpm0 \$c0,\(\$sp\+\),126
 ca4:	f0 f5 1c 7e 	shcpm1 \$c0,\(\$sp\+\),126
 ca8:	f0 f5 5c 7e 	lhcpm1 \$c0,\(\$sp\+\),126
 cac:	ff f5 10 7e 	shcpa \$c15,\(\$sp\+\),126
 cb0:	ff f5 50 7e 	lhcpa \$c15,\(\$sp\+\),126
 cb4:	ff f5 18 7e 	shcpm0 \$c15,\(\$sp\+\),126
 cb8:	ff f5 58 7e 	lhcpm0 \$c15,\(\$sp\+\),126
 cbc:	ff f5 1c 7e 	shcpm1 \$c15,\(\$sp\+\),126
 cc0:	ff f5 5c 7e 	lhcpm1 \$c15,\(\$sp\+\),126
 cc4:	f0 05 20 80 	swcpa \$c0,\(\$0\+\),-128
 cc8:	f0 05 60 80 	lwcpa \$c0,\(\$0\+\),-128
 ccc:	f0 05 28 80 	swcpm0 \$c0,\(\$0\+\),-128
 cd0:	f0 05 68 80 	lwcpm0 \$c0,\(\$0\+\),-128
 cd4:	f0 05 2c 80 	swcpm1 \$c0,\(\$0\+\),-128
 cd8:	f0 05 6c 80 	lwcpm1 \$c0,\(\$0\+\),-128
 cdc:	ff 05 20 80 	swcpa \$c15,\(\$0\+\),-128
 ce0:	ff 05 60 80 	lwcpa \$c15,\(\$0\+\),-128
 ce4:	ff 05 28 80 	swcpm0 \$c15,\(\$0\+\),-128
 ce8:	ff 05 68 80 	lwcpm0 \$c15,\(\$0\+\),-128
 cec:	ff 05 2c 80 	swcpm1 \$c15,\(\$0\+\),-128
 cf0:	ff 05 6c 80 	lwcpm1 \$c15,\(\$0\+\),-128
 cf4:	f0 f5 20 80 	swcpa \$c0,\(\$sp\+\),-128
 cf8:	f0 f5 60 80 	lwcpa \$c0,\(\$sp\+\),-128
 cfc:	f0 f5 28 80 	swcpm0 \$c0,\(\$sp\+\),-128
 d00:	f0 f5 68 80 	lwcpm0 \$c0,\(\$sp\+\),-128
 d04:	f0 f5 2c 80 	swcpm1 \$c0,\(\$sp\+\),-128
 d08:	f0 f5 6c 80 	lwcpm1 \$c0,\(\$sp\+\),-128
 d0c:	ff f5 20 80 	swcpa \$c15,\(\$sp\+\),-128
 d10:	ff f5 60 80 	lwcpa \$c15,\(\$sp\+\),-128
 d14:	ff f5 28 80 	swcpm0 \$c15,\(\$sp\+\),-128
 d18:	ff f5 68 80 	lwcpm0 \$c15,\(\$sp\+\),-128
 d1c:	ff f5 2c 80 	swcpm1 \$c15,\(\$sp\+\),-128
 d20:	ff f5 6c 80 	lwcpm1 \$c15,\(\$sp\+\),-128
 d24:	f0 05 20 7c 	swcpa \$c0,\(\$0\+\),124
 d28:	f0 05 60 7c 	lwcpa \$c0,\(\$0\+\),124
 d2c:	f0 05 28 7c 	swcpm0 \$c0,\(\$0\+\),124
 d30:	f0 05 68 7c 	lwcpm0 \$c0,\(\$0\+\),124
 d34:	f0 05 2c 7c 	swcpm1 \$c0,\(\$0\+\),124
 d38:	f0 05 6c 7c 	lwcpm1 \$c0,\(\$0\+\),124
 d3c:	ff 05 20 7c 	swcpa \$c15,\(\$0\+\),124
 d40:	ff 05 60 7c 	lwcpa \$c15,\(\$0\+\),124
 d44:	ff 05 28 7c 	swcpm0 \$c15,\(\$0\+\),124
 d48:	ff 05 68 7c 	lwcpm0 \$c15,\(\$0\+\),124
 d4c:	ff 05 2c 7c 	swcpm1 \$c15,\(\$0\+\),124
 d50:	ff 05 6c 7c 	lwcpm1 \$c15,\(\$0\+\),124
 d54:	f0 f5 20 7c 	swcpa \$c0,\(\$sp\+\),124
 d58:	f0 f5 60 7c 	lwcpa \$c0,\(\$sp\+\),124
 d5c:	f0 f5 28 7c 	swcpm0 \$c0,\(\$sp\+\),124
 d60:	f0 f5 68 7c 	lwcpm0 \$c0,\(\$sp\+\),124
 d64:	f0 f5 2c 7c 	swcpm1 \$c0,\(\$sp\+\),124
 d68:	f0 f5 6c 7c 	lwcpm1 \$c0,\(\$sp\+\),124
 d6c:	ff f5 20 7c 	swcpa \$c15,\(\$sp\+\),124
 d70:	ff f5 60 7c 	lwcpa \$c15,\(\$sp\+\),124
 d74:	ff f5 28 7c 	swcpm0 \$c15,\(\$sp\+\),124
 d78:	ff f5 68 7c 	lwcpm0 \$c15,\(\$sp\+\),124
 d7c:	ff f5 2c 7c 	swcpm1 \$c15,\(\$sp\+\),124
 d80:	ff f5 6c 7c 	lwcpm1 \$c15,\(\$sp\+\),124
 d84:	f0 05 30 80 	smcpa \$c0,\(\$0\+\),-128
 d88:	f0 05 70 80 	lmcpa \$c0,\(\$0\+\),-128
 d8c:	f0 05 38 80 	smcpm0 \$c0,\(\$0\+\),-128
 d90:	f0 05 78 80 	lmcpm0 \$c0,\(\$0\+\),-128
 d94:	f0 05 3c 80 	smcpm1 \$c0,\(\$0\+\),-128
 d98:	f0 05 7c 80 	lmcpm1 \$c0,\(\$0\+\),-128
 d9c:	ff 05 30 80 	smcpa \$c15,\(\$0\+\),-128
 da0:	ff 05 70 80 	lmcpa \$c15,\(\$0\+\),-128
 da4:	ff 05 38 80 	smcpm0 \$c15,\(\$0\+\),-128
 da8:	ff 05 78 80 	lmcpm0 \$c15,\(\$0\+\),-128
 dac:	ff 05 3c 80 	smcpm1 \$c15,\(\$0\+\),-128
 db0:	ff 05 7c 80 	lmcpm1 \$c15,\(\$0\+\),-128
 db4:	f0 f5 30 80 	smcpa \$c0,\(\$sp\+\),-128
 db8:	f0 f5 70 80 	lmcpa \$c0,\(\$sp\+\),-128
 dbc:	f0 f5 38 80 	smcpm0 \$c0,\(\$sp\+\),-128
 dc0:	f0 f5 78 80 	lmcpm0 \$c0,\(\$sp\+\),-128
 dc4:	f0 f5 3c 80 	smcpm1 \$c0,\(\$sp\+\),-128
 dc8:	f0 f5 7c 80 	lmcpm1 \$c0,\(\$sp\+\),-128
 dcc:	ff f5 30 80 	smcpa \$c15,\(\$sp\+\),-128
 dd0:	ff f5 70 80 	lmcpa \$c15,\(\$sp\+\),-128
 dd4:	ff f5 38 80 	smcpm0 \$c15,\(\$sp\+\),-128
 dd8:	ff f5 78 80 	lmcpm0 \$c15,\(\$sp\+\),-128
 ddc:	ff f5 3c 80 	smcpm1 \$c15,\(\$sp\+\),-128
 de0:	ff f5 7c 80 	lmcpm1 \$c15,\(\$sp\+\),-128
 de4:	f0 05 30 78 	smcpa \$c0,\(\$0\+\),120
 de8:	f0 05 70 78 	lmcpa \$c0,\(\$0\+\),120
 dec:	f0 05 38 78 	smcpm0 \$c0,\(\$0\+\),120
 df0:	f0 05 78 78 	lmcpm0 \$c0,\(\$0\+\),120
 df4:	f0 05 3c 78 	smcpm1 \$c0,\(\$0\+\),120
 df8:	f0 05 7c 78 	lmcpm1 \$c0,\(\$0\+\),120
 dfc:	ff 05 30 78 	smcpa \$c15,\(\$0\+\),120
 e00:	ff 05 70 78 	lmcpa \$c15,\(\$0\+\),120
 e04:	ff 05 38 78 	smcpm0 \$c15,\(\$0\+\),120
 e08:	ff 05 78 78 	lmcpm0 \$c15,\(\$0\+\),120
 e0c:	ff 05 3c 78 	smcpm1 \$c15,\(\$0\+\),120
 e10:	ff 05 7c 78 	lmcpm1 \$c15,\(\$0\+\),120
 e14:	f0 f5 30 78 	smcpa \$c0,\(\$sp\+\),120
 e18:	f0 f5 70 78 	lmcpa \$c0,\(\$sp\+\),120
 e1c:	f0 f5 38 78 	smcpm0 \$c0,\(\$sp\+\),120
 e20:	f0 f5 78 78 	lmcpm0 \$c0,\(\$sp\+\),120
 e24:	f0 f5 3c 78 	smcpm1 \$c0,\(\$sp\+\),120
 e28:	f0 f5 7c 78 	lmcpm1 \$c0,\(\$sp\+\),120
 e2c:	ff f5 30 78 	smcpa \$c15,\(\$sp\+\),120
 e30:	ff f5 70 78 	lmcpa \$c15,\(\$sp\+\),120
 e34:	ff f5 38 78 	smcpm0 \$c15,\(\$sp\+\),120
 e38:	ff f5 78 78 	lmcpm0 \$c15,\(\$sp\+\),120
 e3c:	ff f5 3c 78 	smcpm1 \$c15,\(\$sp\+\),120
 e40:	ff f5 7c 78 	lmcpm1 \$c15,\(\$sp\+\),120
 e44:	d8 04 80 02 	bcpeq 0x0,0xffff0e48
 e48:	d8 05 80 02 	bcpne 0x0,0xffff0e4c
 e4c:	d8 06 80 02 	bcpat 0x0,0xffff0e50
 e50:	d8 07 80 02 	bcpaf 0x0,0xffff0e54
 e54:	d8 f4 80 02 	bcpeq 0xf,0xffff0e58
 e58:	d8 f5 80 02 	bcpne 0xf,0xffff0e5c
 e5c:	d8 f6 80 02 	bcpat 0xf,0xffff0e60
 e60:	d8 f7 80 02 	bcpaf 0xf,0xffff0e64
 e64:	d8 04 3f ff 	bcpeq 0x0,0x8e62
 e68:	d8 05 3f ff 	bcpne 0x0,0x8e66
 e6c:	d8 06 3f ff 	bcpat 0x0,0x8e6a
 e70:	d8 07 3f ff 	bcpaf 0x0,0x8e6e
 e74:	d8 f4 3f ff 	bcpeq 0xf,0x8e72
 e78:	d8 f5 3f ff 	bcpne 0xf,0x8e76
 e7c:	d8 f6 3f ff 	bcpat 0xf,0x8e7a
 e80:	d8 f7 3f ff 	bcpaf 0xf,0x8e7e
 e84:	d8 04 00 00 	bcpeq 0x0,0xe84
			e84: R_MEP_PCREL17A2	symbol
 e88:	d8 05 00 00 	bcpne 0x0,0xe88
			e88: R_MEP_PCREL17A2	symbol
 e8c:	d8 06 00 00 	bcpat 0x0,0xe8c
			e8c: R_MEP_PCREL17A2	symbol
 e90:	d8 07 00 00 	bcpaf 0x0,0xe90
			e90: R_MEP_PCREL17A2	symbol
 e94:	d8 f4 00 00 	bcpeq 0xf,0xe94
			e94: R_MEP_PCREL17A2	symbol
 e98:	d8 f5 00 00 	bcpne 0xf,0xe98
			e98: R_MEP_PCREL17A2	symbol
 e9c:	d8 f6 00 00 	bcpat 0xf,0xe9c
			e9c: R_MEP_PCREL17A2	symbol
 ea0:	d8 f7 00 00 	bcpaf 0xf,0xea0
			ea0: R_MEP_PCREL17A2	symbol
 ea4:	70 21       	synccp
 ea6:	18 0f       	jsrv \$0
 ea8:	18 ff       	jsrv \$sp
 eaa:	d8 2b 80 00 	bsrv 0xff800eae
 eae:	df fb 7f ff 	bsrv 0x800eac
 eb2:	d8 0b 00 00 	bsrv 0xeb2
			eb2: R_MEP_PCREL24A2	symbol
 eb6:	00 00       	nop
			eb6: R_MEP_8	symbol
			eb7: R_MEP_16	symbol
 eb8:	00 00       	nop
			eb9: R_MEP_32	symbol
 eba:	00 00       	nop
.*

