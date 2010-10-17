#objdump: -dr

.*:     file format elf64-mmix

Disassembly of section \.text:

0+ <Main>:
   0:	bf2a0000 	pushgo \$42,\$0,0
			2: R_MMIX_REG	\.MMIX\.reg_contents
   4:	fd000000 	swym 0,0,0

0+8 <extfn>:
   8:	f8000000 	pop 0,0
