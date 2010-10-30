
.*:     file format elf32-m68k

Disassembly of section \.plt:

00020800 <f.@plt-0x14>:
   20800:	2f3b 0170 0000 	movel %pc@\(30404 <_GLOBAL_OFFSET_TABLE_\+0x4>\),%sp@-
   20806:	fc02 
   20808:	4efb 0171 0000 	jmp %pc@\(30408 <_GLOBAL_OFFSET_TABLE_\+0x8>\)@\(0*\)
   2080e:	fbfe 
   20810:	0000 0000      	orib #0,%d0

00020814 <f.@plt>:
   20814:	4efb 0171 0000 	jmp %pc@\(3040c <_GLOBAL_OFFSET_TABLE_\+0xc>\)@\(0*\)
   2081a:	fbf6 
   2081c:	2f3c 0000 0000 	movel #0,%sp@-
   20822:	60ff ffff ffdc 	bral 20800 <f.@plt-0x14>

00020828 <f.@plt>:
   20828:	4efb 0171 0000 	jmp %pc@\(30410 <_GLOBAL_OFFSET_TABLE_\+0x10>\)@\(0*\)
   2082e:	fbe6 
   20830:	2f3c 0000 000c 	movel #12,%sp@-
   20836:	60ff ffff ffc8 	bral 20800 <f.@plt-0x14>

0002083c <f.@plt>:
   2083c:	4efb 0171 0000 	jmp %pc@\(30414 <_GLOBAL_OFFSET_TABLE_\+0x14>\)@\(0*\)
   20842:	fbd6 
   20844:	2f3c 0000 0018 	movel #24,%sp@-
   2084a:	60ff ffff ffb4 	bral 20800 <f.@plt-0x14>
Disassembly of section \.text:

00020c00 <.*>:
   20c00:	61ff ffff fc.. 	bsrl 208.. <f1@plt>
   20c06:	61ff ffff fc.. 	bsrl 208.. <f2@plt>
   20c0c:	61ff ffff fc.. 	bsrl 208.. <f3@plt>
