#objdump: -dr
#name: MIPS VR4120
#as: -march=vr4120

.*: +file format .*mips.*

Disassembly of section \.text:
0+000 <\.text>:
 + 0:	00002012 	mflo	a0
	\.\.\.
 + c:	00a62029 	dmacc	a0,a1,a2
 +10:	00a62229 	dmacchi	a0,a1,a2
 +14:	00a62629 	dmacchis	a0,a1,a2
 +18:	00a62269 	dmacchiu	a0,a1,a2
 +1c:	00a62669 	dmacchius	a0,a1,a2
 +20:	00a62429 	dmaccs	a0,a1,a2
 +24:	00a62069 	dmaccu	a0,a1,a2
 +28:	00a62469 	dmaccus	a0,a1,a2
 +2c:	00002012 	mflo	a0
	\.\.\.
 +38:	00a62028 	macc	a0,a1,a2
 +3c:	00a62228 	macchi	a0,a1,a2
 +40:	00a62628 	macchis	a0,a1,a2
 +44:	00a62268 	macchiu	a0,a1,a2
 +48:	00a62668 	macchius	a0,a1,a2
 +4c:	00a62428 	maccs	a0,a1,a2
 +50:	00a62068 	maccu	a0,a1,a2
 +54:	00a62468 	maccus	a0,a1,a2
#pass
