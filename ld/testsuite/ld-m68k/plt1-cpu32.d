
.*:     file format elf32-m68k

Disassembly of section \.plt:

00020800 <f.@plt-0x18>:
   20800:	2f3b 0170 0000 	movel %pc@\(30404 <_GLOBAL_OFFSET_TABLE_\+0x4>\),%sp@-
   20806:	fc02 
   20808:	227b 0170 0000 	moveal %pc@\(30408 <_GLOBAL_OFFSET_TABLE_\+0x8>\),%a1
   2080e:	fbfe 
   20810:	4ed1           	jmp %a1@
   20812:	0000 0000      	orib #0,%d0
	\.\.\.

00020818 <f.@plt>:
   20818:	227b 0170 0000 	moveal %pc@\(3040c <_GLOBAL_OFFSET_TABLE_\+0xc>\),%a1
   2081e:	fbf2 
   20820:	4ed1           	jmp %a1@
   20822:	2f3c 0000 0000 	movel #0,%sp@-
   20828:	60ff ffff ffd6 	bral 20800 <f.@plt-0x18>
	\.\.\.

00020830 <f.@plt>:
   20830:	227b 0170 0000 	moveal %pc@\(30410 <_GLOBAL_OFFSET_TABLE_\+0x10>\),%a1
   20836:	fbde 
   20838:	4ed1           	jmp %a1@
   2083a:	2f3c 0000 000c 	movel #12,%sp@-
   20840:	60ff ffff ffbe 	bral 20800 <f.@plt-0x18>
	\.\.\.

00020848 <f.@plt>:
   20848:	227b 0170 0000 	moveal %pc@\(30414 <_GLOBAL_OFFSET_TABLE_\+0x14>\),%a1
   2084e:	fbca 
   20850:	4ed1           	jmp %a1@
   20852:	2f3c 0000 0018 	movel #24,%sp@-
   20858:	60ff ffff ffa6 	bral 20800 <f.@plt-0x18>
	\.\.\.
Disassembly of section \.text:

00020c00 <.*>:
   20c00:	61ff ffff fc.. 	bsrl 208.. <f1@plt>
   20c06:	61ff ffff fc.. 	bsrl 208.. <f2@plt>
   20c0c:	61ff ffff fc.. 	bsrl 208.. <f3@plt>
