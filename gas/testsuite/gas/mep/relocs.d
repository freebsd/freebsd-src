
relocs.x:     file format elf32-mep

Contents of section .text:
 1000 00000000 00000000 00000000 00000000  ................
 1010 00000000 00000000 00000000 00000000  ................
 1020 00000000 00000000 00000000 00000000  ................
 1030 0000c53c 1012dee9 ffffe509 ffec0000  ...<............
 1040 0000c53c efeedd49 ffdfe509 efd20000  ...<...I........
 1050 0000c53c 202cdeb9 000fe509 07e9dc88  ...< ,..........
 1060 0080d818 0002dfc8 7fffdf28 7fffdf78  ...........\(...x
 1070 7fffdd98 0001da98 000fdbf8 0070da58  .............p.X
 1080 0002d828 0000d848 0000d8d8 0010d898  ...\(...H........
 1090 0010d808 0000d908 0000d908 0000d808  ................
 10a0 0000d808 0000d908 0000d908 00000000  ................
 10b0 00000000 0000d808 00000000 00000000  ................
Contents of section .rostacktab:
 10c0 001ffff0                             ....            
Contents of section .data:
 11c4 0000002a                             ...*            
Disassembly of section .text:

00001000 <junk1>:
    1000:	00 00       	nop
    1002:	00 00       	nop
    1004:	00 00       	nop
    1006:	00 00       	nop
    1008:	00 00       	nop
    100a:	00 00       	nop
    100c:	00 00       	nop
    100e:	00 00       	nop
    1010:	00 00       	nop

00001012 <foo>:
    1012:	00 00       	nop
    1014:	00 00       	nop
    1016:	00 00       	nop
    1018:	00 00       	nop

0000101a <bar>:
    101a:	00 00       	nop
    101c:	00 00       	nop
    101e:	00 00       	nop
    1020:	00 00       	nop
    1022:	00 00       	nop

00001024 <junk2>:
    1024:	00 00       	nop
    1026:	00 00       	nop
    1028:	00 00       	nop
    102a:	00 00       	nop
    102c:	00 00       	nop

0000102e <main>:
    102e:	00 00       	nop
    1030:	00 00       	nop
    1032:	c5 3c 10 12 	lb \$5,4114\(\$3\)
    1036:	de e9 ff ff 	bsr 1012 <&:s3:foo:s3:bar>
    103a:	e5 09 ff ec 	repeat \$5,1012 <&:s3:foo:s3:bar>
    103e:	00 00       	nop
    1040:	00 00       	nop
    1042:	c5 3c ef ee 	lb \$5,-4114\(\$3\)
    1046:	dd 49 ff df 	bsr ffffefee <0-:s3:foo>
    104a:	e5 09 ef d2 	repeat \$5,ffffefee <0-:s3:foo>
    104e:	00 00       	nop
    1050:	00 00       	nop
    1052:	c5 3c 20 2c 	lb \$5,8236\(\$3\)
    1056:	de b9 00 0f 	bsr 202c <\+:s3:foo:s3:bar>
    105a:	e5 09 07 e9 	repeat \$5,202c <\+:s3:foo:s3:bar>
    105e:	dc 88 00 80 	jmp 8090 <<<:s3:foo:#00000003>
    1062:	d8 18 00 02 	jmp 202 <>>:s3:foo:#00000003>
    1066:	df c8 7f ff 	jmp 7ffff8 <&:-:s3:foo:s3:bar:#007fffff>
    106a:	df 28 7f ff 	jmp 7fffe4 <&:-:s3:foo:s4:main:#007fffff>
    106e:	df 78 7f ff 	jmp 7fffee <&:-:S5:.text:s3:foo:#007fffff>
    1072:	dd 98 00 01 	jmp 1b2 <&:-:S5:.data:s3:foo:#007fffff>
    1076:	da 98 00 0f 	jmp f52 <-:s3:foo:\+:s9:.text.end:0-:S5:.text>
    107a:	db f8 00 70 	jmp 707e <\*:s3:foo:#00000007>
    107e:	da 58 00 02 	jmp 24a <>>:s3:foo:#00000003\+0x48>
    1082:	d8 28 00 00 	jmp 4 <__assert_based_size\+0x3>
    1086:	d8 48 00 00 	jmp 8 <\^:s3:foo:s3:bar>
    108a:	d8 d8 00 10 	jmp 101a <|:s3:foo:s3:bar>
    108e:	d8 98 00 10 	jmp 1012 <&:s3:foo:s3:bar>
    1092:	d8 08 00 00 	jmp 0 <<<:==:s3:foo:s3:bar:#00000005>
    1096:	d9 08 00 00 	jmp 20 <<<:&&:s3:foo:s3:bar:#00000005>
    109a:	d9 08 00 00 	jmp 20 <<<:&&:s3:foo:s3:bar:#00000005>
    109e:	d8 08 00 00 	jmp 0 <<<:==:s3:foo:s3:bar:#00000005>
    10a2:	d8 08 00 00 	jmp 0 <<<:==:s3:foo:s3:bar:#00000005>
    10a6:	d9 08 00 00 	jmp 20 <<<:&&:s3:foo:s3:bar:#00000005>
    10aa:	d9 08 00 00 	jmp 20 <<<:&&:s3:foo:s3:bar:#00000005>
    10ae:	00 00       	nop
    10b0:	00 00       	nop
    10b2:	00 00       	nop
    10b4:	00 00       	nop
    10b6:	d8 08 00 00 	jmp 0 <<<:==:s3:foo:s3:bar:#00000005>
    10ba:	00 00       	nop
    10bc:	00 00       	nop
    10be:	00 00       	nop
#pass
