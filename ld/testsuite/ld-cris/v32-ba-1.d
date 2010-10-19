# notarget: cris*-*-linux-gnu
# as: --march=v32 --em=criself
# ld: -m criself
# objdump: -d

# Check that 32-bit branches (PCREL:s) are relocated right.
# Source code and "-m criself" doesn't work with *-linux-gnu.

.*:     file format elf32-us-cris

Disassembly of section \.text:

0+ <a>:
   0:	bf0e 0800 0000      	ba 8 <b>
   6:	5e82                	moveq 30,r8

0+8 <b>:
   8:	4312                	moveq 3,r1
   a:	bf0e f6ff ffff      	ba 0 <[^>]*>
  10:	4db2                	moveq 13,r11
