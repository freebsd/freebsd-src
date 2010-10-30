#as: -EL
#objdump: -dr
#source: dj2.s
#name: dj2.le

.*: +file format .*

Disassembly of section .text:

00000000 <.text>:
   0:	88 07       	sb \$7,\(\$8\)
   2:	98 05       	sb \$5,\(\$9\)
