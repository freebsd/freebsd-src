#objdump: -dr -mmips:4000
#as: -mips3 -mtune=r4000 -mabi=64
#name: mips16-64
#source: mips16.s

# Test the mips16 instruction set.

.*: +file format .*mips.*

Disassembly of section .text:

0+000000 <data1>:
   0:	00000000 	nop

0+000004 <insns1>:
   4:	3b40      	ld	v0,0\(v1\)
   6:	f000 3b41 	ld	v0,1\(v1\)
   a:	f000 3b42 	ld	v0,2\(v1\)
   e:	f000 3b43 	ld	v0,3\(v1\)
  12:	f000 3b44 	ld	v0,4\(v1\)
  16:	3b41      	ld	v0,8\(v1\)
  18:	3b42      	ld	v0,16\(v1\)
  1a:	3b44      	ld	v0,32\(v1\)
  1c:	3b48      	ld	v0,64\(v1\)
  1e:	3b50      	ld	v0,128\(v1\)
  20:	f100 3b40 	ld	v0,256\(v1\)
  24:	f200 3b40 	ld	v0,512\(v1\)
  28:	f400 3b40 	ld	v0,1024\(v1\)
  2c:	f001 3b40 	ld	v0,2048\(v1\)
  30:	f7ff 3b5f 	ld	v0,-1\(v1\)
  34:	f7ff 3b5e 	ld	v0,-2\(v1\)
  38:	f7ff 3b5d 	ld	v0,-3\(v1\)
  3c:	f7ff 3b5c 	ld	v0,-4\(v1\)
  40:	f7ff 3b58 	ld	v0,-8\(v1\)
  44:	f7ff 3b50 	ld	v0,-16\(v1\)
  48:	f7ff 3b40 	ld	v0,-32\(v1\)
  4c:	f7df 3b40 	ld	v0,-64\(v1\)
  50:	f79f 3b40 	ld	v0,-128\(v1\)
  54:	f71f 3b40 	ld	v0,-256\(v1\)
  58:	f61f 3b40 	ld	v0,-512\(v1\)
  5c:	f41f 3b40 	ld	v0,-1024\(v1\)
  60:	f01f 3b40 	ld	v0,-2048\(v1\)
  64:	f7bf fc40 	ld	v0,0 <data1>
  68:	f6a0 fc54 	ld	v0,71c <data2>
  6c:	f001 fc40 	ld	v0,868 <bar>
  70:	f0c1 fc40 	ld	v0,930 <quux>
  74:	f840      	ld	v0,0\(sp\)
  76:	f000 f841 	ld	v0,1\(sp\)
  7a:	f000 f842 	ld	v0,2\(sp\)
  7e:	f000 f843 	ld	v0,3\(sp\)
  82:	f000 f844 	ld	v0,4\(sp\)
  86:	f841      	ld	v0,8\(sp\)
  88:	f842      	ld	v0,16\(sp\)
  8a:	f844      	ld	v0,32\(sp\)
  8c:	f848      	ld	v0,64\(sp\)
  8e:	f850      	ld	v0,128\(sp\)
  90:	f100 f840 	ld	v0,256\(sp\)
  94:	f200 f840 	ld	v0,512\(sp\)
  98:	f400 f840 	ld	v0,1024\(sp\)
  9c:	f001 f840 	ld	v0,2048\(sp\)
  a0:	f7ff f85f 	ld	v0,-1\(sp\)
  a4:	f7ff f85e 	ld	v0,-2\(sp\)
  a8:	f7ff f85d 	ld	v0,-3\(sp\)
  ac:	f7ff f85c 	ld	v0,-4\(sp\)
  b0:	f7ff f858 	ld	v0,-8\(sp\)
  b4:	f7ff f850 	ld	v0,-16\(sp\)
  b8:	f7ff f840 	ld	v0,-32\(sp\)
  bc:	f7df f840 	ld	v0,-64\(sp\)
  c0:	f79f f840 	ld	v0,-128\(sp\)
  c4:	f71f f840 	ld	v0,-256\(sp\)
  c8:	f61f f840 	ld	v0,-512\(sp\)
  cc:	f41f f840 	ld	v0,-1024\(sp\)
  d0:	f01f f840 	ld	v0,-2048\(sp\)
  d4:	bb40      	lwu	v0,0\(v1\)
  d6:	f000 bb41 	lwu	v0,1\(v1\)
  da:	f000 bb42 	lwu	v0,2\(v1\)
  de:	f000 bb43 	lwu	v0,3\(v1\)
  e2:	bb41      	lwu	v0,4\(v1\)
  e4:	bb42      	lwu	v0,8\(v1\)
  e6:	bb44      	lwu	v0,16\(v1\)
  e8:	bb48      	lwu	v0,32\(v1\)
  ea:	bb50      	lwu	v0,64\(v1\)
  ec:	f080 bb40 	lwu	v0,128\(v1\)
  f0:	f100 bb40 	lwu	v0,256\(v1\)
  f4:	f200 bb40 	lwu	v0,512\(v1\)
  f8:	f400 bb40 	lwu	v0,1024\(v1\)
  fc:	f001 bb40 	lwu	v0,2048\(v1\)
 100:	f7ff bb5f 	lwu	v0,-1\(v1\)
 104:	f7ff bb5e 	lwu	v0,-2\(v1\)
 108:	f7ff bb5d 	lwu	v0,-3\(v1\)
 10c:	f7ff bb5c 	lwu	v0,-4\(v1\)
 110:	f7ff bb58 	lwu	v0,-8\(v1\)
 114:	f7ff bb50 	lwu	v0,-16\(v1\)
 118:	f7ff bb40 	lwu	v0,-32\(v1\)
 11c:	f7df bb40 	lwu	v0,-64\(v1\)
 120:	f79f bb40 	lwu	v0,-128\(v1\)
 124:	f71f bb40 	lwu	v0,-256\(v1\)
 128:	f61f bb40 	lwu	v0,-512\(v1\)
 12c:	f41f bb40 	lwu	v0,-1024\(v1\)
 130:	f01f bb40 	lwu	v0,-2048\(v1\)
 134:	9b40      	lw	v0,0\(v1\)
 136:	f000 9b41 	lw	v0,1\(v1\)
 13a:	f000 9b42 	lw	v0,2\(v1\)
 13e:	f000 9b43 	lw	v0,3\(v1\)
 142:	9b41      	lw	v0,4\(v1\)
 144:	9b42      	lw	v0,8\(v1\)
 146:	9b44      	lw	v0,16\(v1\)
 148:	9b48      	lw	v0,32\(v1\)
 14a:	9b50      	lw	v0,64\(v1\)
 14c:	f080 9b40 	lw	v0,128\(v1\)
 150:	f100 9b40 	lw	v0,256\(v1\)
 154:	f200 9b40 	lw	v0,512\(v1\)
 158:	f400 9b40 	lw	v0,1024\(v1\)
 15c:	f001 9b40 	lw	v0,2048\(v1\)
 160:	f7ff 9b5f 	lw	v0,-1\(v1\)
 164:	f7ff 9b5e 	lw	v0,-2\(v1\)
 168:	f7ff 9b5d 	lw	v0,-3\(v1\)
 16c:	f7ff 9b5c 	lw	v0,-4\(v1\)
 170:	f7ff 9b58 	lw	v0,-8\(v1\)
 174:	f7ff 9b50 	lw	v0,-16\(v1\)
 178:	f7ff 9b40 	lw	v0,-32\(v1\)
 17c:	f7df 9b40 	lw	v0,-64\(v1\)
 180:	f79f 9b40 	lw	v0,-128\(v1\)
 184:	f71f 9b40 	lw	v0,-256\(v1\)
 188:	f61f 9b40 	lw	v0,-512\(v1\)
 18c:	f41f 9b40 	lw	v0,-1024\(v1\)
 190:	f01f 9b40 	lw	v0,-2048\(v1\)
 194:	f67f b20c 	lw	v0,0 <data1>
 198:	f580 b204 	lw	v0,71c <data2>
 19c:	f6c0 b20c 	lw	v0,868 <bar>
 1a0:	f780 b210 	lw	v0,930 <quux>
 1a4:	9200      	lw	v0,0\(sp\)
 1a6:	f000 9201 	lw	v0,1\(sp\)
 1aa:	f000 9202 	lw	v0,2\(sp\)
 1ae:	f000 9203 	lw	v0,3\(sp\)
 1b2:	9201      	lw	v0,4\(sp\)
 1b4:	9202      	lw	v0,8\(sp\)
 1b6:	9204      	lw	v0,16\(sp\)
 1b8:	9208      	lw	v0,32\(sp\)
 1ba:	9210      	lw	v0,64\(sp\)
 1bc:	9220      	lw	v0,128\(sp\)
 1be:	9240      	lw	v0,256\(sp\)
 1c0:	9280      	lw	v0,512\(sp\)
 1c2:	f400 9200 	lw	v0,1024\(sp\)
 1c6:	f001 9200 	lw	v0,2048\(sp\)
 1ca:	f7ff 921f 	lw	v0,-1\(sp\)
 1ce:	f7ff 921e 	lw	v0,-2\(sp\)
 1d2:	f7ff 921d 	lw	v0,-3\(sp\)
 1d6:	f7ff 921c 	lw	v0,-4\(sp\)
 1da:	f7ff 9218 	lw	v0,-8\(sp\)
 1de:	f7ff 9210 	lw	v0,-16\(sp\)
 1e2:	f7ff 9200 	lw	v0,-32\(sp\)
 1e6:	f7df 9200 	lw	v0,-64\(sp\)
 1ea:	f79f 9200 	lw	v0,-128\(sp\)
 1ee:	f71f 9200 	lw	v0,-256\(sp\)
 1f2:	f61f 9200 	lw	v0,-512\(sp\)
 1f6:	f41f 9200 	lw	v0,-1024\(sp\)
 1fa:	f01f 9200 	lw	v0,-2048\(sp\)
 1fe:	8b40      	lh	v0,0\(v1\)
 200:	f000 8b41 	lh	v0,1\(v1\)
 204:	8b41      	lh	v0,2\(v1\)
 206:	f000 8b43 	lh	v0,3\(v1\)
 20a:	8b42      	lh	v0,4\(v1\)
 20c:	8b44      	lh	v0,8\(v1\)
 20e:	8b48      	lh	v0,16\(v1\)
 210:	8b50      	lh	v0,32\(v1\)
 212:	f040 8b40 	lh	v0,64\(v1\)
 216:	f080 8b40 	lh	v0,128\(v1\)
 21a:	f100 8b40 	lh	v0,256\(v1\)
 21e:	f200 8b40 	lh	v0,512\(v1\)
 222:	f400 8b40 	lh	v0,1024\(v1\)
 226:	f001 8b40 	lh	v0,2048\(v1\)
 22a:	f7ff 8b5f 	lh	v0,-1\(v1\)
 22e:	f7ff 8b5e 	lh	v0,-2\(v1\)
 232:	f7ff 8b5d 	lh	v0,-3\(v1\)
 236:	f7ff 8b5c 	lh	v0,-4\(v1\)
 23a:	f7ff 8b58 	lh	v0,-8\(v1\)
 23e:	f7ff 8b50 	lh	v0,-16\(v1\)
 242:	f7ff 8b40 	lh	v0,-32\(v1\)
 246:	f7df 8b40 	lh	v0,-64\(v1\)
 24a:	f79f 8b40 	lh	v0,-128\(v1\)
 24e:	f71f 8b40 	lh	v0,-256\(v1\)
 252:	f61f 8b40 	lh	v0,-512\(v1\)
 256:	f41f 8b40 	lh	v0,-1024\(v1\)
 25a:	f01f 8b40 	lh	v0,-2048\(v1\)
 25e:	ab40      	lhu	v0,0\(v1\)
 260:	f000 ab41 	lhu	v0,1\(v1\)
 264:	ab41      	lhu	v0,2\(v1\)
 266:	f000 ab43 	lhu	v0,3\(v1\)
 26a:	ab42      	lhu	v0,4\(v1\)
 26c:	ab44      	lhu	v0,8\(v1\)
 26e:	ab48      	lhu	v0,16\(v1\)
 270:	ab50      	lhu	v0,32\(v1\)
 272:	f040 ab40 	lhu	v0,64\(v1\)
 276:	f080 ab40 	lhu	v0,128\(v1\)
 27a:	f100 ab40 	lhu	v0,256\(v1\)
 27e:	f200 ab40 	lhu	v0,512\(v1\)
 282:	f400 ab40 	lhu	v0,1024\(v1\)
 286:	f001 ab40 	lhu	v0,2048\(v1\)
 28a:	f7ff ab5f 	lhu	v0,-1\(v1\)
 28e:	f7ff ab5e 	lhu	v0,-2\(v1\)
 292:	f7ff ab5d 	lhu	v0,-3\(v1\)
 296:	f7ff ab5c 	lhu	v0,-4\(v1\)
 29a:	f7ff ab58 	lhu	v0,-8\(v1\)
 29e:	f7ff ab50 	lhu	v0,-16\(v1\)
 2a2:	f7ff ab40 	lhu	v0,-32\(v1\)
 2a6:	f7df ab40 	lhu	v0,-64\(v1\)
 2aa:	f79f ab40 	lhu	v0,-128\(v1\)
 2ae:	f71f ab40 	lhu	v0,-256\(v1\)
 2b2:	f61f ab40 	lhu	v0,-512\(v1\)
 2b6:	f41f ab40 	lhu	v0,-1024\(v1\)
 2ba:	f01f ab40 	lhu	v0,-2048\(v1\)
 2be:	8340      	lb	v0,0\(v1\)
 2c0:	8341      	lb	v0,1\(v1\)
 2c2:	8342      	lb	v0,2\(v1\)
 2c4:	8343      	lb	v0,3\(v1\)
 2c6:	8344      	lb	v0,4\(v1\)
 2c8:	8348      	lb	v0,8\(v1\)
 2ca:	8350      	lb	v0,16\(v1\)
 2cc:	f020 8340 	lb	v0,32\(v1\)
 2d0:	f040 8340 	lb	v0,64\(v1\)
 2d4:	f080 8340 	lb	v0,128\(v1\)
 2d8:	f100 8340 	lb	v0,256\(v1\)
 2dc:	f200 8340 	lb	v0,512\(v1\)
 2e0:	f400 8340 	lb	v0,1024\(v1\)
 2e4:	f001 8340 	lb	v0,2048\(v1\)
 2e8:	f7ff 835f 	lb	v0,-1\(v1\)
 2ec:	f7ff 835e 	lb	v0,-2\(v1\)
 2f0:	f7ff 835d 	lb	v0,-3\(v1\)
 2f4:	f7ff 835c 	lb	v0,-4\(v1\)
 2f8:	f7ff 8358 	lb	v0,-8\(v1\)
 2fc:	f7ff 8350 	lb	v0,-16\(v1\)
 300:	f7ff 8340 	lb	v0,-32\(v1\)
 304:	f7df 8340 	lb	v0,-64\(v1\)
 308:	f79f 8340 	lb	v0,-128\(v1\)
 30c:	f71f 8340 	lb	v0,-256\(v1\)
 310:	f61f 8340 	lb	v0,-512\(v1\)
 314:	f41f 8340 	lb	v0,-1024\(v1\)
 318:	f01f 8340 	lb	v0,-2048\(v1\)
 31c:	a340      	lbu	v0,0\(v1\)
 31e:	a341      	lbu	v0,1\(v1\)
 320:	a342      	lbu	v0,2\(v1\)
 322:	a343      	lbu	v0,3\(v1\)
 324:	a344      	lbu	v0,4\(v1\)
 326:	a348      	lbu	v0,8\(v1\)
 328:	a350      	lbu	v0,16\(v1\)
 32a:	f020 a340 	lbu	v0,32\(v1\)
 32e:	f040 a340 	lbu	v0,64\(v1\)
 332:	f080 a340 	lbu	v0,128\(v1\)
 336:	f100 a340 	lbu	v0,256\(v1\)
 33a:	f200 a340 	lbu	v0,512\(v1\)
 33e:	f400 a340 	lbu	v0,1024\(v1\)
 342:	f001 a340 	lbu	v0,2048\(v1\)
 346:	f7ff a35f 	lbu	v0,-1\(v1\)
 34a:	f7ff a35e 	lbu	v0,-2\(v1\)
 34e:	f7ff a35d 	lbu	v0,-3\(v1\)
 352:	f7ff a35c 	lbu	v0,-4\(v1\)
 356:	f7ff a358 	lbu	v0,-8\(v1\)
 35a:	f7ff a350 	lbu	v0,-16\(v1\)
 35e:	f7ff a340 	lbu	v0,-32\(v1\)
 362:	f7df a340 	lbu	v0,-64\(v1\)
 366:	f79f a340 	lbu	v0,-128\(v1\)
 36a:	f71f a340 	lbu	v0,-256\(v1\)
 36e:	f61f a340 	lbu	v0,-512\(v1\)
 372:	f41f a340 	lbu	v0,-1024\(v1\)
 376:	f01f a340 	lbu	v0,-2048\(v1\)
 37a:	7b40      	sd	v0,0\(v1\)
 37c:	f000 7b41 	sd	v0,1\(v1\)
 380:	f000 7b42 	sd	v0,2\(v1\)
 384:	f000 7b43 	sd	v0,3\(v1\)
 388:	f000 7b44 	sd	v0,4\(v1\)
 38c:	7b41      	sd	v0,8\(v1\)
 38e:	7b42      	sd	v0,16\(v1\)
 390:	7b44      	sd	v0,32\(v1\)
 392:	7b48      	sd	v0,64\(v1\)
 394:	7b50      	sd	v0,128\(v1\)
 396:	f100 7b40 	sd	v0,256\(v1\)
 39a:	f200 7b40 	sd	v0,512\(v1\)
 39e:	f400 7b40 	sd	v0,1024\(v1\)
 3a2:	f001 7b40 	sd	v0,2048\(v1\)
 3a6:	f7ff 7b5f 	sd	v0,-1\(v1\)
 3aa:	f7ff 7b5e 	sd	v0,-2\(v1\)
 3ae:	f7ff 7b5d 	sd	v0,-3\(v1\)
 3b2:	f7ff 7b5c 	sd	v0,-4\(v1\)
 3b6:	f7ff 7b58 	sd	v0,-8\(v1\)
 3ba:	f7ff 7b50 	sd	v0,-16\(v1\)
 3be:	f7ff 7b40 	sd	v0,-32\(v1\)
 3c2:	f7df 7b40 	sd	v0,-64\(v1\)
 3c6:	f79f 7b40 	sd	v0,-128\(v1\)
 3ca:	f71f 7b40 	sd	v0,-256\(v1\)
 3ce:	f61f 7b40 	sd	v0,-512\(v1\)
 3d2:	f41f 7b40 	sd	v0,-1024\(v1\)
 3d6:	f01f 7b40 	sd	v0,-2048\(v1\)
 3da:	f940      	sd	v0,0\(sp\)
 3dc:	f000 f941 	sd	v0,1\(sp\)
 3e0:	f000 f942 	sd	v0,2\(sp\)
 3e4:	f000 f943 	sd	v0,3\(sp\)
 3e8:	f000 f944 	sd	v0,4\(sp\)
 3ec:	f941      	sd	v0,8\(sp\)
 3ee:	f942      	sd	v0,16\(sp\)
 3f0:	f944      	sd	v0,32\(sp\)
 3f2:	f948      	sd	v0,64\(sp\)
 3f4:	f950      	sd	v0,128\(sp\)
 3f6:	f100 f940 	sd	v0,256\(sp\)
 3fa:	f200 f940 	sd	v0,512\(sp\)
 3fe:	f400 f940 	sd	v0,1024\(sp\)
 402:	f001 f940 	sd	v0,2048\(sp\)
 406:	f7ff f95f 	sd	v0,-1\(sp\)
 40a:	f7ff f95e 	sd	v0,-2\(sp\)
 40e:	f7ff f95d 	sd	v0,-3\(sp\)
 412:	f7ff f95c 	sd	v0,-4\(sp\)
 416:	f7ff f958 	sd	v0,-8\(sp\)
 41a:	f7ff f950 	sd	v0,-16\(sp\)
 41e:	f7ff f940 	sd	v0,-32\(sp\)
 422:	f7df f940 	sd	v0,-64\(sp\)
 426:	f79f f940 	sd	v0,-128\(sp\)
 42a:	f71f f940 	sd	v0,-256\(sp\)
 42e:	f61f f940 	sd	v0,-512\(sp\)
 432:	f41f f940 	sd	v0,-1024\(sp\)
 436:	f01f f940 	sd	v0,-2048\(sp\)
 43a:	fa00      	sd	ra,0\(sp\)
 43c:	f000 fa01 	sd	ra,1\(sp\)
 440:	f000 fa02 	sd	ra,2\(sp\)
 444:	f000 fa03 	sd	ra,3\(sp\)
 448:	f000 fa04 	sd	ra,4\(sp\)
 44c:	fa01      	sd	ra,8\(sp\)
 44e:	fa02      	sd	ra,16\(sp\)
 450:	fa04      	sd	ra,32\(sp\)
 452:	fa08      	sd	ra,64\(sp\)
 454:	fa10      	sd	ra,128\(sp\)
 456:	fa20      	sd	ra,256\(sp\)
 458:	fa40      	sd	ra,512\(sp\)
 45a:	fa80      	sd	ra,1024\(sp\)
 45c:	f001 fa00 	sd	ra,2048\(sp\)
 460:	f7ff fa1f 	sd	ra,-1\(sp\)
 464:	f7ff fa1e 	sd	ra,-2\(sp\)
 468:	f7ff fa1d 	sd	ra,-3\(sp\)
 46c:	f7ff fa1c 	sd	ra,-4\(sp\)
 470:	f7ff fa18 	sd	ra,-8\(sp\)
 474:	f7ff fa10 	sd	ra,-16\(sp\)
 478:	f7ff fa00 	sd	ra,-32\(sp\)
 47c:	f7df fa00 	sd	ra,-64\(sp\)
 480:	f79f fa00 	sd	ra,-128\(sp\)
 484:	f71f fa00 	sd	ra,-256\(sp\)
 488:	f61f fa00 	sd	ra,-512\(sp\)
 48c:	f41f fa00 	sd	ra,-1024\(sp\)
 490:	f01f fa00 	sd	ra,-2048\(sp\)
 494:	db40      	sw	v0,0\(v1\)
 496:	f000 db41 	sw	v0,1\(v1\)
 49a:	f000 db42 	sw	v0,2\(v1\)
 49e:	f000 db43 	sw	v0,3\(v1\)
 4a2:	db41      	sw	v0,4\(v1\)
 4a4:	db42      	sw	v0,8\(v1\)
 4a6:	db44      	sw	v0,16\(v1\)
 4a8:	db48      	sw	v0,32\(v1\)
 4aa:	db50      	sw	v0,64\(v1\)
 4ac:	f080 db40 	sw	v0,128\(v1\)
 4b0:	f100 db40 	sw	v0,256\(v1\)
 4b4:	f200 db40 	sw	v0,512\(v1\)
 4b8:	f400 db40 	sw	v0,1024\(v1\)
 4bc:	f001 db40 	sw	v0,2048\(v1\)
 4c0:	f7ff db5f 	sw	v0,-1\(v1\)
 4c4:	f7ff db5e 	sw	v0,-2\(v1\)
 4c8:	f7ff db5d 	sw	v0,-3\(v1\)
 4cc:	f7ff db5c 	sw	v0,-4\(v1\)
 4d0:	f7ff db58 	sw	v0,-8\(v1\)
 4d4:	f7ff db50 	sw	v0,-16\(v1\)
 4d8:	f7ff db40 	sw	v0,-32\(v1\)
 4dc:	f7df db40 	sw	v0,-64\(v1\)
 4e0:	f79f db40 	sw	v0,-128\(v1\)
 4e4:	f71f db40 	sw	v0,-256\(v1\)
 4e8:	f61f db40 	sw	v0,-512\(v1\)
 4ec:	f41f db40 	sw	v0,-1024\(v1\)
 4f0:	f01f db40 	sw	v0,-2048\(v1\)
 4f4:	d200      	sw	v0,0\(sp\)
 4f6:	f000 d201 	sw	v0,1\(sp\)
 4fa:	f000 d202 	sw	v0,2\(sp\)
 4fe:	f000 d203 	sw	v0,3\(sp\)
 502:	d201      	sw	v0,4\(sp\)
 504:	d202      	sw	v0,8\(sp\)
 506:	d204      	sw	v0,16\(sp\)
 508:	d208      	sw	v0,32\(sp\)
 50a:	d210      	sw	v0,64\(sp\)
 50c:	d220      	sw	v0,128\(sp\)
 50e:	d240      	sw	v0,256\(sp\)
 510:	d280      	sw	v0,512\(sp\)
 512:	f400 d200 	sw	v0,1024\(sp\)
 516:	f001 d200 	sw	v0,2048\(sp\)
 51a:	f7ff d21f 	sw	v0,-1\(sp\)
 51e:	f7ff d21e 	sw	v0,-2\(sp\)
 522:	f7ff d21d 	sw	v0,-3\(sp\)
 526:	f7ff d21c 	sw	v0,-4\(sp\)
 52a:	f7ff d218 	sw	v0,-8\(sp\)
 52e:	f7ff d210 	sw	v0,-16\(sp\)
 532:	f7ff d200 	sw	v0,-32\(sp\)
 536:	f7df d200 	sw	v0,-64\(sp\)
 53a:	f79f d200 	sw	v0,-128\(sp\)
 53e:	f71f d200 	sw	v0,-256\(sp\)
 542:	f61f d200 	sw	v0,-512\(sp\)
 546:	f41f d200 	sw	v0,-1024\(sp\)
 54a:	f01f d200 	sw	v0,-2048\(sp\)
 54e:	6200      	sw	ra,0\(sp\)
 550:	f000 6201 	sw	ra,1\(sp\)
 554:	f000 6202 	sw	ra,2\(sp\)
 558:	f000 6203 	sw	ra,3\(sp\)
 55c:	6201      	sw	ra,4\(sp\)
 55e:	6202      	sw	ra,8\(sp\)
 560:	6204      	sw	ra,16\(sp\)
 562:	6208      	sw	ra,32\(sp\)
 564:	6210      	sw	ra,64\(sp\)
 566:	6220      	sw	ra,128\(sp\)
 568:	6240      	sw	ra,256\(sp\)
 56a:	6280      	sw	ra,512\(sp\)
 56c:	f400 6200 	sw	ra,1024\(sp\)
 570:	f001 6200 	sw	ra,2048\(sp\)
 574:	f7ff 621f 	sw	ra,-1\(sp\)
 578:	f7ff 621e 	sw	ra,-2\(sp\)
 57c:	f7ff 621d 	sw	ra,-3\(sp\)
 580:	f7ff 621c 	sw	ra,-4\(sp\)
 584:	f7ff 6218 	sw	ra,-8\(sp\)
 588:	f7ff 6210 	sw	ra,-16\(sp\)
 58c:	f7ff 6200 	sw	ra,-32\(sp\)
 590:	f7df 6200 	sw	ra,-64\(sp\)
 594:	f79f 6200 	sw	ra,-128\(sp\)
 598:	f71f 6200 	sw	ra,-256\(sp\)
 59c:	f61f 6200 	sw	ra,-512\(sp\)
 5a0:	f41f 6200 	sw	ra,-1024\(sp\)
 5a4:	f01f 6200 	sw	ra,-2048\(sp\)
 5a8:	cb40      	sh	v0,0\(v1\)
 5aa:	f000 cb41 	sh	v0,1\(v1\)
 5ae:	cb41      	sh	v0,2\(v1\)
 5b0:	f000 cb43 	sh	v0,3\(v1\)
 5b4:	cb42      	sh	v0,4\(v1\)
 5b6:	cb44      	sh	v0,8\(v1\)
 5b8:	cb48      	sh	v0,16\(v1\)
 5ba:	cb50      	sh	v0,32\(v1\)
 5bc:	f040 cb40 	sh	v0,64\(v1\)
 5c0:	f080 cb40 	sh	v0,128\(v1\)
 5c4:	f100 cb40 	sh	v0,256\(v1\)
 5c8:	f200 cb40 	sh	v0,512\(v1\)
 5cc:	f400 cb40 	sh	v0,1024\(v1\)
 5d0:	f001 cb40 	sh	v0,2048\(v1\)
 5d4:	f7ff cb5f 	sh	v0,-1\(v1\)
 5d8:	f7ff cb5e 	sh	v0,-2\(v1\)
 5dc:	f7ff cb5d 	sh	v0,-3\(v1\)
 5e0:	f7ff cb5c 	sh	v0,-4\(v1\)
 5e4:	f7ff cb58 	sh	v0,-8\(v1\)
 5e8:	f7ff cb50 	sh	v0,-16\(v1\)
 5ec:	f7ff cb40 	sh	v0,-32\(v1\)
 5f0:	f7df cb40 	sh	v0,-64\(v1\)
 5f4:	f79f cb40 	sh	v0,-128\(v1\)
 5f8:	f71f cb40 	sh	v0,-256\(v1\)
 5fc:	f61f cb40 	sh	v0,-512\(v1\)
 600:	f41f cb40 	sh	v0,-1024\(v1\)
 604:	f01f cb40 	sh	v0,-2048\(v1\)
 608:	c340      	sb	v0,0\(v1\)
 60a:	c341      	sb	v0,1\(v1\)
 60c:	c342      	sb	v0,2\(v1\)
 60e:	c343      	sb	v0,3\(v1\)
 610:	c344      	sb	v0,4\(v1\)
 612:	c348      	sb	v0,8\(v1\)
 614:	c350      	sb	v0,16\(v1\)
 616:	f020 c340 	sb	v0,32\(v1\)
 61a:	f040 c340 	sb	v0,64\(v1\)
 61e:	f080 c340 	sb	v0,128\(v1\)
 622:	f100 c340 	sb	v0,256\(v1\)
 626:	f200 c340 	sb	v0,512\(v1\)
 62a:	f400 c340 	sb	v0,1024\(v1\)
 62e:	f001 c340 	sb	v0,2048\(v1\)
 632:	f7ff c35f 	sb	v0,-1\(v1\)
 636:	f7ff c35e 	sb	v0,-2\(v1\)
 63a:	f7ff c35d 	sb	v0,-3\(v1\)
 63e:	f7ff c35c 	sb	v0,-4\(v1\)
 642:	f7ff c358 	sb	v0,-8\(v1\)
 646:	f7ff c350 	sb	v0,-16\(v1\)
 64a:	f7ff c340 	sb	v0,-32\(v1\)
 64e:	f7df c340 	sb	v0,-64\(v1\)
 652:	f79f c340 	sb	v0,-128\(v1\)
 656:	f71f c340 	sb	v0,-256\(v1\)
 65a:	f61f c340 	sb	v0,-512\(v1\)
 65e:	f41f c340 	sb	v0,-1024\(v1\)
 662:	f01f c340 	sb	v0,-2048\(v1\)
 666:	6a00      	li	v0,0
 668:	6a01      	li	v0,1
 66a:	f100 6a00 	li	v0,256
 66e:	675e      	move	v0,s8
 670:	6592      	move	s4,v0
 672:	4350      	daddiu	v0,v1,0
 674:	4351      	daddiu	v0,v1,1
 676:	435f      	daddiu	v0,v1,-1
 678:	f010 4350 	daddiu	v0,v1,16
 67c:	f7ff 4350 	daddiu	v0,v1,-16
 680:	e388      	daddu	v0,v1,a0
 682:	fd40      	daddiu	v0,0
 684:	fd41      	daddiu	v0,1
 686:	fd5f      	daddiu	v0,-1
 688:	f020 fd40 	daddiu	v0,32
 68c:	f7ff fd40 	daddiu	v0,-32
 690:	f080 fd40 	daddiu	v0,128
 694:	f79f fd40 	daddiu	v0,-128
 698:	f17f fe48 	dla	v0,0 <data1>
 69c:	f080 fe40 	dla	v0,71c <data2>
 6a0:	f1c0 fe48 	dla	v0,868 <bar>
 6a4:	f280 fe4c 	dla	v0,930 <quux>
 6a8:	fb00      	daddiu	sp,0
 6aa:	f000 fb01 	daddiu	sp,1
 6ae:	f7ff fb1f 	daddiu	sp,-1
 6b2:	fb20      	daddiu	sp,256
 6b4:	fbe0      	daddiu	sp,-256
 6b6:	ff40      	daddiu	v0,sp,0
 6b8:	f000 ff41 	daddiu	v0,sp,1
 6bc:	f7ff ff5f 	daddiu	v0,sp,-1
 6c0:	ff48      	daddiu	v0,sp,32
 6c2:	f7ff ff40 	daddiu	v0,sp,-32
 6c6:	f080 ff40 	daddiu	v0,sp,128
 6ca:	f79f ff40 	daddiu	v0,sp,-128
 6ce:	4340      	addiu	v0,v1,0
 6d0:	4341      	addiu	v0,v1,1
 6d2:	434f      	addiu	v0,v1,-1
 6d4:	f010 4340 	addiu	v0,v1,16
 6d8:	f7ff 4340 	addiu	v0,v1,-16
 6dc:	e389      	addu	v0,v1,a0
 6de:	4a00      	addiu	v0,0
 6e0:	4a01      	addiu	v0,1
 6e2:	4aff      	addiu	v0,-1
 6e4:	4a20      	addiu	v0,32
 6e6:	4ae0      	addiu	v0,-32
 6e8:	f080 4a00 	addiu	v0,128
 6ec:	4a80      	addiu	v0,-128
 6ee:	f11f 0a14 	la	v0,0 <data1>
 6f2:	0a0b      	la	v0,71c <data2>
 6f4:	0a5d      	la	v0,868 <bar>
 6f6:	0a8f      	la	v0,930 <quux>
 6f8:	6300      	addiu	sp,0
 6fa:	f000 6301 	addiu	sp,1
 6fe:	f7ff 631f 	addiu	sp,-1
 702:	6320      	addiu	sp,256
 704:	63e0      	addiu	sp,-256
 706:	0200      	addiu	v0,sp,0
 708:	f000 0201 	addiu	v0,sp,1
 70c:	f7ff 021f 	addiu	v0,sp,-1
 710:	0208      	addiu	v0,sp,32
 712:	f7ff 0200 	addiu	v0,sp,-32
 716:	0220      	addiu	v0,sp,128
 718:	f79f 0200 	addiu	v0,sp,-128

0+00071c <data2>:
 71c:	00000000 	nop

0+000720 <insns2>:
 720:	e38a      	dsubu	v0,v1,a0
 722:	e38b      	subu	v0,v1,a0
 724:	ea6b      	neg	v0,v1
 726:	ea6c      	and	v0,v1
 728:	ea6d      	or	v0,v1
 72a:	ea6e      	xor	v0,v1
 72c:	ea6f      	not	v0,v1
 72e:	5200      	slti	v0,0
 730:	5201      	slti	v0,1
 732:	f7ff 521f 	slti	v0,-1
 736:	52ff      	slti	v0,255
 738:	f100 5200 	slti	v0,256
 73c:	ea62      	slt	v0,v1
 73e:	5a00      	sltiu	v0,0
 740:	5a01      	sltiu	v0,1
 742:	f7ff 5a1f 	sltiu	v0,-1
 746:	5aff      	sltiu	v0,255
 748:	f100 5a00 	sltiu	v0,256
 74c:	ea63      	sltu	v0,v1
 74e:	7200      	cmpi	v0,0
 750:	7201      	cmpi	v0,1
 752:	72ff      	cmpi	v0,255
 754:	f100 7200 	cmpi	v0,256
 758:	ea6a      	cmp	v0,v1
 75a:	f000 3261 	dsll	v0,v1,0
 75e:	3265      	dsll	v0,v1,1
 760:	3261      	dsll	v0,v1,8
 762:	f240 3261 	dsll	v0,v1,9
 766:	f7e0 3261 	dsll	v0,v1,63
 76a:	eb54      	dsllv	v0,v1
 76c:	f000 e848 	dsrl	v0,0
 770:	e948      	dsrl	v0,1
 772:	e848      	dsrl	v0,8
 774:	f240 e848 	dsrl	v0,9
 778:	f7e0 e848 	dsrl	v0,63
 77c:	eb56      	dsrlv	v0,v1
 77e:	f000 e853 	dsra	v0,0
 782:	e953      	dsra	v0,1
 784:	e853      	dsra	v0,8
 786:	f240 e853 	dsra	v0,9
 78a:	f7e0 e853 	dsra	v0,63
 78e:	eb57      	dsrav	v0,v1
 790:	ea12      	mflo	v0
 792:	eb10      	mfhi	v1
 794:	f000 3260 	sll	v0,v1,0
 798:	3264      	sll	v0,v1,1
 79a:	3260      	sll	v0,v1,8
 79c:	f240 3260 	sll	v0,v1,9
 7a0:	f7c0 3260 	sll	v0,v1,31
 7a4:	eb44      	sllv	v0,v1
 7a6:	f000 3262 	srl	v0,v1,0
 7aa:	3266      	srl	v0,v1,1
 7ac:	3262      	srl	v0,v1,8
 7ae:	f240 3262 	srl	v0,v1,9
 7b2:	f7c0 3262 	srl	v0,v1,31
 7b6:	eb46      	srlv	v0,v1
 7b8:	f000 3263 	sra	v0,v1,0
 7bc:	3267      	sra	v0,v1,1
 7be:	3263      	sra	v0,v1,8
 7c0:	f240 3263 	sra	v0,v1,9
 7c4:	f7c0 3263 	sra	v0,v1,31
 7c8:	eb47      	srav	v0,v1
 7ca:	ea7c      	dmult	v0,v1
 7cc:	ea7d      	dmultu	v0,v1
 7ce:	ea7e      	ddiv	zero,v0,v1
 7d0:	2b01      	bnez	v1,7d4 <insns2\+(0x|)b4>
 7d2:	e8e5      	break	7
 7d4:	ea12      	mflo	v0
 7d6:	6500      	nop
 7d8:	6500      	nop
 7da:	ea7f      	ddivu	zero,v0,v1
 7dc:	2b01      	bnez	v1,7e0 <insns2\+(0x|)c0>
 7de:	e8e5      	break	7
 7e0:	ea12      	mflo	v0
 7e2:	6500      	nop
 7e4:	6500      	nop
 7e6:	ea78      	mult	v0,v1
 7e8:	ea79      	multu	v0,v1
 7ea:	ea7a      	div	zero,v0,v1
 7ec:	2b01      	bnez	v1,7f0 <insns2\+(0x|)d0>
 7ee:	e8e5      	break	7
 7f0:	ea12      	mflo	v0
 7f2:	6500      	nop
 7f4:	6500      	nop
 7f6:	ea7b      	divu	zero,v0,v1
 7f8:	2b01      	bnez	v1,7fc <insns2\+(0x|)dc>
 7fa:	e8e5      	break	7
 7fc:	ea12      	mflo	v0
 7fe:	ea00      	jr	v0
 800:	6500      	nop
 802:	e820      	jr	ra
 804:	6500      	nop
 806:	ea40      	jalr	v0
 808:	6500      	nop
 80a:	f3ff 221b 	beqz	v0,4 <insns1>
 80e:	2288      	beqz	v0,720 <insns2>
 810:	222b      	beqz	v0,868 <bar>
 812:	f080 220d 	beqz	v0,930 <quux>
 816:	f3ff 2a15 	bnez	v0,4 <insns1>
 81a:	2a82      	bnez	v0,720 <insns2>
 81c:	2a25      	bnez	v0,868 <bar>
 81e:	f080 2a07 	bnez	v0,930 <quux>
 822:	f3ff 600f 	bteqz	4 <insns1>
 826:	f77f 601b 	bteqz	720 <insns2>
 82a:	601e      	bteqz	868 <bar>
 82c:	f080 6000 	bteqz	930 <quux>
 830:	f3ff 6108 	btnez	4 <insns1>
 834:	f77f 6114 	btnez	720 <insns2>
 838:	6117      	btnez	868 <bar>
 83a:	617a      	btnez	930 <quux>
 83c:	f3ff 1002 	b	4 <insns1>
 840:	176f      	b	720 <insns2>
 842:	1012      	b	868 <bar>
 844:	1075      	b	930 <quux>
 846:	e805      	break	0
 848:	e825      	break	1
 84a:	efe5      	break	63
 84c:	1800 0000 	jal	0 <data1>
			84c: R_MIPS16_26	extern
			84c: R_MIPS_NONE	\*ABS\*
			84c: R_MIPS_NONE	\*ABS\*
 850:	6500      	nop
 852:	e809      	entry	
 854:	e909      	entry	a0
 856:	eb49      	entry	a0-a2,s0
 858:	e8a9      	entry	s0-s1,ra
 85a:	e829      	entry	ra
 85c:	ef09      	exit	
 85e:	ef49      	exit	s0
 860:	efa9      	exit	s0-s1,ra
 862:	ef29      	exit	ra
 864:	6500      	nop
 866:	6500      	nop

0+000868 <bar>:
	...
