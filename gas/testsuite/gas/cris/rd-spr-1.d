#as: --underscore --em=criself --march=v32
#objdump: -dr

# Check support for support function register names.

.*:     file format elf32-us-cris

Disassembly of section \.text:

00000000 <a>:
   0:	7a0f                	move s0,r10
   2:	791f                	move s1,r9
   4:	781f                	move s1,r8
   6:	772f                	move s2,r7
   8:	762f                	move s2,r6
   a:	753f                	move s3,r5
   c:	743f                	move s3,r4
   e:	734f                	move s4,r3
  10:	724f                	move s4,r2
  12:	718f                	move s8,r1
  14:	709f                	move s9,r0
  16:	7f8f                	move s8,acr
  18:	7e9f                	move s9,sp
  1a:	7daf                	move s10,r13
  1c:	7bff                	move s15,r11

0000001e <b>:
  1e:	790b                	move r9,s0
  20:	7a1b                	move r10,s1
  22:	771b                	move r7,s1
  24:	782b                	move r8,s2
  26:	752b                	move r5,s2
  28:	763b                	move r6,s3
  2a:	733b                	move r3,s3
  2c:	744b                	move r4,s4
  2e:	714b                	move r1,s4
  30:	708b                	move r0,s8
  32:	7f9b                	move acr,s9
  34:	728b                	move r2,s8
  36:	7d9b                	move r13,s9
  38:	7bab                	move r11,s10
  3a:	7efb                	move sp,s15
