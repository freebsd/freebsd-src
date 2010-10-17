#name: MIPS rel64 n64
#source: rel64.s
#as: -KPIC -EB -64
#readelf: -x 6 -r
#ld: -shared -melf64btsmip

Relocation section '.rel.dyn' at offset 0x5b0 contains 2 entries:
  Offset          Info           Type           Sym. Value    Sym. Name
000000000000  000000000000 R_MIPS_NONE      
                    Type2: R_MIPS_NONE      
                    Type3: R_MIPS_NONE      
0000000005e0  000000001203 R_MIPS_REL32     
                    Type2: R_MIPS_64        
                    Type3: R_MIPS_NONE      

Hex dump of section '.text':
  0x000005d0 00000000 00000000 00000000 00000000 ................
  0x000005e0 00000000 000005e0 00000000 00000000 ................
  0x000005f0 00000000 00000000 00000000 00000000 ................
