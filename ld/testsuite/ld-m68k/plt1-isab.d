
.*:     file format elf32-m68k

Disassembly of section \.plt:

00020800 <f.@plt-0x18>:
# _GLOBAL_OFFSET_TABLE_ + 4 == 0x30404 == 0x20802 + 0xfc02
   20800:	203c 0000 fc02 	movel #64514,%d0
   20806:	2f3b 08fa      	movel %pc@\(20802 <f.@plt-0x16>,%d0:l\),%sp@-
# _GLOBAL_OFFSET_TABLE_ + 8 == 0x30408 == 0x2080c + 0xfbfc
   2080a:	203c 0000 fbfc 	movel #64508,%d0
   20810:	207b 08fa      	moveal %pc@\(2080c <f.@plt-0xc>,%d0:l\),%a0
   20814:	4ed0           	jmp %a0@
   20816:	4e71           	nop

00020818 <f.@plt>:
# _GLOBAL_OFFSET_TABLE_ + 12 == 0x3040c == 0x2081a + 0xfbf2
   20818:	203c 0000 fbf2 	movel #64498,%d0
   2081e:	207b 08fa      	moveal %pc@\(2081a <f.@plt\+0x2>,%d0:l\),%a0
   20822:	4ed0           	jmp %a0@
   20824:	2f3c 0000 0000 	movel #0,%sp@-
   2082a:	60ff ffff ffd4 	bral 20800 <f.@plt-0x18>

00020830 <f.@plt>:
# _GLOBAL_OFFSET_TABLE_ + 16 == 0x30410 == 0x20832 + 0xfbde
   20830:	203c 0000 fbde 	movel #64478,%d0
   20836:	207b 08fa      	moveal %pc@\(20832 <f.@plt\+0x2>,%d0:l\),%a0
   2083a:	4ed0           	jmp %a0@
   2083c:	2f3c 0000 000c 	movel #12,%sp@-
   20842:	60ff ffff ffbc 	bral 20800 <f.@plt-0x18>

00020848 <f.@plt>:
# _GLOBAL_OFFSET_TABLE_ + 20 == 0x30414 == 0x2084a + 0xfbca
   20848:	203c 0000 fbca 	movel #64458,%d0
   2084e:	207b 08fa      	moveal %pc@\(2084a <f.@plt\+0x2>,%d0:l\),%a0
   20852:	4ed0           	jmp %a0@
   20854:	2f3c 0000 0018 	movel #24,%sp@-
   2085a:	60ff ffff ffa4 	bral 20800 <f.@plt-0x18>
Disassembly of section \.text:

00020c00 <.*>:
   20c00:	61ff ffff fc.. 	bsrl 208.. <f1@plt>
   20c06:	61ff ffff fc.. 	bsrl 208.. <f2@plt>
   20c0c:	61ff ffff fc.. 	bsrl 208.. <f3@plt>
