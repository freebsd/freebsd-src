#as: --defsym __amd64__=1
#objdump: -dw
#name: 64-bit SVME
#source: svme.s

.*: +file format .*

Disassembly of section .text:

0+000 <common>:
[	 ]*[0-9a-f]+:[	 ]+0f 01 dd[	 ]+clgi[	 ]*
[	 ]*[0-9a-f]+:[	 ]+0f 01 df[	 ]+invlpga[	 ]*
[	 ]*[0-9a-f]+:[	 ]+0f 01 de[	 ]+skinit[	 ]*
[	 ]*[0-9a-f]+:[	 ]+0f 01 dc[	 ]+stgi[	 ]*
[	 ]*[0-9a-f]+:[	 ]+0f 01 da[	 ]+vmload[	 ]*
[	 ]*[0-9a-f]+:[	 ]+0f 01 d9[	 ]+vmmcall[	 ]*
[	 ]*[0-9a-f]+:[	 ]+0f 01 d8[	 ]+vmrun[	 ]*
[	 ]*[0-9a-f]+:[	 ]+0f 01 db[	 ]+vmsave[	 ]*
[0-9a-f]+ <att64>:
[	 ]*[0-9a-f]+:[	 ]+0f 01 df[	 ]+invlpga[	 ]*
[	 ]*[0-9a-f]+:[	 ]+0f 01 da[	 ]+vmload[	 ]*
[	 ]*[0-9a-f]+:[	 ]+0f 01 d8[	 ]+vmrun[	 ]*
[	 ]*[0-9a-f]+:[	 ]+0f 01 db[	 ]+vmsave[	 ]*
[0-9a-f]+ <att32>:
[	 ]*[0-9a-f]+:[	 ]+0f 01 de[	 ]+skinit[	 ]*
[	 ]*[0-9a-f]+:[	 ]+67 0f 01 df[	 ]+(addr32 )?invlpga[	 ]*\(%eax\),[	 ]*%ecx
[	 ]*[0-9a-f]+:[	 ]+67 0f 01 da[	 ]+(addr32 )?vmload[	 ]*\(%eax\)
[	 ]*[0-9a-f]+:[	 ]+67 0f 01 d8[	 ]+(addr32 )?vmrun[	 ]*\(%eax\)
[	 ]*[0-9a-f]+:[	 ]+67 0f 01 db[	 ]+(addr32 )?vmsave[	 ]*\(%eax\)
[0-9a-f]+ <intel64>:
[	 ]*[0-9a-f]+:[	 ]+0f 01 df[	 ]+invlpga[	 ]*
[	 ]*[0-9a-f]+:[	 ]+0f 01 da[	 ]+vmload[	 ]*
[	 ]*[0-9a-f]+:[	 ]+0f 01 d8[	 ]+vmrun[	 ]*
[	 ]*[0-9a-f]+:[	 ]+0f 01 db[	 ]+vmsave[	 ]*
[0-9a-f]+ <intel32>:
[	 ]*[0-9a-f]+:[	 ]+0f 01 de[	 ]+skinit[	 ]*
[	 ]*[0-9a-f]+:[	 ]+67 0f 01 df[	 ]+(addr32 )?invlpga[	 ]*\(%eax\),[	 ]*%ecx
[	 ]*[0-9a-f]+:[	 ]+67 0f 01 da[	 ]+(addr32 )?vmload[	 ]*\(%eax\)
[	 ]*[0-9a-f]+:[	 ]+67 0f 01 d8[	 ]+(addr32 )?vmrun[	 ]*\(%eax\)
[	 ]*[0-9a-f]+:[	 ]+67 0f 01 db[	 ]+(addr32 )?vmsave[	 ]*\(%eax\)
#pass
