# source: expdyn1.s
# target: cris-*-*elf* cris-*-*aout*
# as: --em=criself
# ld: -mcriself
# objdump: -d

# Note that the linker script symbol __start is set to the same
# value as _start, and will collate before _start and be chosen
# as the presentation symbol at disassembly.  Anyway, __start
# shouldn't hinder disassembly by posing as an object symbol.

.*:     file format elf32-us-cris

Disassembly of section \.text:

0+6 <__start>:
   6:	0f05                	nop 

0+8 <expfn>:
   8:	0f05                	nop 
