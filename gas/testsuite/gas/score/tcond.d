#as:
#objdump: -d
#source: tcond.s

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	0f05      	tset!
   2:	0f05      	tset!
   4:	0f05      	tset!
   6:	0f05      	tset!
   8:	0f05      	tset!
   a:	0f05      	tset!
   c:	8000bc54 	tset
  10:	8254e010 	add		r18, r20, r24
	...
  20:	0f05      	tset!
  22:	0f05      	tset!
  24:	0f05      	tset!
  26:	0f05      	tset!
  28:	8000bc54 	tset
  2c:	8254e026 	xor		r18, r20, r24
  30:	0005      	tcs!
  32:	0005      	tcs!
  34:	0005      	tcs!
  36:	0005      	tcs!
  38:	0005      	tcs!
  3a:	0005      	tcs!
  3c:	80008054 	tcs
  40:	8254e010 	add		r18, r20, r24
	...
  50:	0005      	tcs!
  52:	0005      	tcs!
  54:	0005      	tcs!
  56:	0005      	tcs!
  58:	80008054 	tcs
  5c:	8254e026 	xor		r18, r20, r24
  60:	0105      	tcc!
  62:	0105      	tcc!
  64:	0105      	tcc!
  66:	0105      	tcc!
  68:	0105      	tcc!
  6a:	0105      	tcc!
  6c:	80008454 	tcc
  70:	8254e010 	add		r18, r20, r24
	...
  80:	0105      	tcc!
  82:	0105      	tcc!
  84:	0105      	tcc!
  86:	0105      	tcc!
  88:	80008454 	tcc
  8c:	8254e026 	xor		r18, r20, r24
  90:	0205      	tgtu!
  92:	0205      	tgtu!
  94:	0205      	tgtu!
  96:	0205      	tgtu!
  98:	0205      	tgtu!
  9a:	0205      	tgtu!
  9c:	80008854 	tgtu
  a0:	8254e010 	add		r18, r20, r24
	...
  b0:	0205      	tgtu!
  b2:	0205      	tgtu!
  b4:	0205      	tgtu!
  b6:	0205      	tgtu!
  b8:	80008854 	tgtu
  bc:	8254e026 	xor		r18, r20, r24
  c0:	0305      	tleu!
  c2:	0305      	tleu!
  c4:	0305      	tleu!
  c6:	0305      	tleu!
  c8:	0305      	tleu!
  ca:	0305      	tleu!
  cc:	80008c54 	tleu
  d0:	8254e010 	add		r18, r20, r24
	...
  e0:	0305      	tleu!
  e2:	0305      	tleu!
  e4:	0305      	tleu!
  e6:	0305      	tleu!
  e8:	80008c54 	tleu
  ec:	8254e026 	xor		r18, r20, r24
  f0:	0405      	teq!
  f2:	0405      	teq!
  f4:	0405      	teq!
  f6:	0405      	teq!
  f8:	0405      	teq!
  fa:	0405      	teq!
  fc:	80009054 	teq
 100:	8254e010 	add		r18, r20, r24
	...
 110:	0405      	teq!
 112:	0405      	teq!
 114:	0405      	teq!
 116:	0405      	teq!
 118:	80009054 	teq
 11c:	8254e026 	xor		r18, r20, r24
 120:	0505      	tne!
 122:	0505      	tne!
 124:	0505      	tne!
 126:	0505      	tne!
 128:	0505      	tne!
 12a:	0505      	tne!
 12c:	80009454 	tne
 130:	8254e010 	add		r18, r20, r24
	...
 140:	0505      	tne!
 142:	0505      	tne!
 144:	0505      	tne!
 146:	0505      	tne!
 148:	80009454 	tne
 14c:	8254e026 	xor		r18, r20, r24
 150:	0605      	tgt!
 152:	0605      	tgt!
 154:	0605      	tgt!
 156:	0605      	tgt!
 158:	0605      	tgt!
 15a:	0605      	tgt!
 15c:	80009854 	tgt
 160:	8254e010 	add		r18, r20, r24
	...
 170:	0605      	tgt!
 172:	0605      	tgt!
 174:	0605      	tgt!
 176:	0605      	tgt!
 178:	80009854 	tgt
 17c:	8254e026 	xor		r18, r20, r24
 180:	0705      	tle!
 182:	0705      	tle!
 184:	0705      	tle!
 186:	0705      	tle!
 188:	0705      	tle!
 18a:	0705      	tle!
 18c:	80009c54 	tle
 190:	8254e010 	add		r18, r20, r24
	...
 1a0:	0705      	tle!
 1a2:	0705      	tle!
 1a4:	0705      	tle!
 1a6:	0705      	tle!
 1a8:	80009c54 	tle
 1ac:	8254e026 	xor		r18, r20, r24
 1b0:	0805      	tge!
 1b2:	0805      	tge!
 1b4:	0805      	tge!
 1b6:	0805      	tge!
 1b8:	0805      	tge!
 1ba:	0805      	tge!
 1bc:	8000a054 	tge
 1c0:	8254e010 	add		r18, r20, r24
	...
 1d0:	0805      	tge!
 1d2:	0805      	tge!
 1d4:	0805      	tge!
 1d6:	0805      	tge!
 1d8:	8000a054 	tge
 1dc:	8254e026 	xor		r18, r20, r24
 1e0:	0905      	tlt!
 1e2:	0905      	tlt!
 1e4:	0905      	tlt!
 1e6:	0905      	tlt!
 1e8:	0905      	tlt!
 1ea:	0905      	tlt!
 1ec:	8000a454 	tlt
 1f0:	8254e010 	add		r18, r20, r24
	...
 200:	0905      	tlt!
 202:	0905      	tlt!
 204:	0905      	tlt!
 206:	0905      	tlt!
 208:	8000a454 	tlt
 20c:	8254e026 	xor		r18, r20, r24
 210:	0a05      	tmi!
 212:	0a05      	tmi!
 214:	0a05      	tmi!
 216:	0a05      	tmi!
 218:	0a05      	tmi!
 21a:	0a05      	tmi!
 21c:	8000a854 	tmi
 220:	8254e010 	add		r18, r20, r24
	...
 230:	0a05      	tmi!
 232:	0a05      	tmi!
 234:	0a05      	tmi!
 236:	0a05      	tmi!
 238:	8000a854 	tmi
 23c:	8254e026 	xor		r18, r20, r24
 240:	0b05      	tpl!
 242:	0b05      	tpl!
 244:	0b05      	tpl!
 246:	0b05      	tpl!
 248:	0b05      	tpl!
 24a:	0b05      	tpl!
 24c:	8000ac54 	tpl
 250:	8254e010 	add		r18, r20, r24
	...
 260:	0b05      	tpl!
 262:	0b05      	tpl!
 264:	0b05      	tpl!
 266:	0b05      	tpl!
 268:	8000ac54 	tpl
 26c:	8254e026 	xor		r18, r20, r24
 270:	0c05      	tvs!
 272:	0c05      	tvs!
 274:	0c05      	tvs!
 276:	0c05      	tvs!
 278:	0c05      	tvs!
 27a:	0c05      	tvs!
 27c:	8000b054 	tvs
 280:	8254e010 	add		r18, r20, r24
	...
 290:	0c05      	tvs!
 292:	0c05      	tvs!
 294:	0c05      	tvs!
 296:	0c05      	tvs!
 298:	8000b054 	tvs
 29c:	8254e026 	xor		r18, r20, r24
 2a0:	0d05      	tvc!
 2a2:	0d05      	tvc!
 2a4:	0d05      	tvc!
 2a6:	0d05      	tvc!
 2a8:	0d05      	tvc!
 2aa:	0d05      	tvc!
 2ac:	8000b454 	tvc
 2b0:	8254e010 	add		r18, r20, r24
	...
 2c0:	0d05      	tvc!
 2c2:	0d05      	tvc!
 2c4:	0d05      	tvc!
 2c6:	0d05      	tvc!
 2c8:	8000b454 	tvc
 2cc:	8254e026 	xor		r18, r20, r24
 2d0:	0e05      	tcnz!
 2d2:	0e05      	tcnz!
 2d4:	0e05      	tcnz!
 2d6:	0e05      	tcnz!
 2d8:	0e05      	tcnz!
 2da:	0e05      	tcnz!
 2dc:	8000b854 	tcnz
 2e0:	8254e010 	add		r18, r20, r24
	...
 2f0:	0e05      	tcnz!
 2f2:	0e05      	tcnz!
 2f4:	0e05      	tcnz!
 2f6:	0e05      	tcnz!
 2f8:	8000b854 	tcnz
 2fc:	8254e026 	xor		r18, r20, r24
 300:	6062      	sdbbp!		12
 302:	6062      	sdbbp!		12
 304:	6062      	sdbbp!		12
 306:	6062      	sdbbp!		12
 308:	6062      	sdbbp!		12
 30a:	6062      	sdbbp!		12
 30c:	800c8006 	sdbbp		12
 310:	8254e010 	add		r18, r20, r24
	...
 320:	6062      	sdbbp!		12
 322:	6062      	sdbbp!		12
 324:	6062      	sdbbp!		12
 326:	6062      	sdbbp!		12
 328:	800c8006 	sdbbp		12
 32c:	8254e026 	xor		r18, r20, r24
