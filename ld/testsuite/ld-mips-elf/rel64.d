#name: MIPS rel64 n64
#source: rel64.s
#as: -KPIC -EB -64
#readelf: -x .text -r
#ld: -shared -melf64btsmip

Relocation section '.rel.dyn' at offset .* contains 2 entries:
  Offset          Info           Type           Sym. Value    Sym. Name
[0-9a-f ]+R_MIPS_NONE      
 +Type2: R_MIPS_NONE      
 +Type3: R_MIPS_NONE      
[0-9a-f ]+R_MIPS_REL32     
 +Type2: R_MIPS_64        
 +Type3: R_MIPS_NONE      

Hex dump of section '.text':
  0x00000450 00000000 00000000 00000000 00000000 ................
  0x00000460 00000000 00000460 00000000 00000000 ................
  0x00000470 00000000 00000000 00000000 00000000 ................
