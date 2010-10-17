#objdump: --syms
#name: ARM Mapping Symbols

# Test the generation of ARM ELF Mapping Symbols

.*: +file format.*arm.*

SYMBOL TABLE:
0+00 l    d  .text	0+0 
0+00 l    d  .data	0+0 
0+00 l    d  .bss	0+0 
0+00 l     F .text	0+0 \$a
0+08 l       .text	0+0 \$t
0+00 l     O .data	0+0 \$d
0+00 l    d  foo	0+0 
0+00 l       foo	0+0 \$t
0+00 g       .text	0+0 mapping
0+08 g       .text	0+0 thumb_mapping
