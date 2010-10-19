
.*:     file format .*

Disassembly of section \.text:

#
# The TLS entries are ordered as follows:
#
#	foo0	(-0x7ff0 + 0x20)
#	foo2	(-0x7ff0 + 0x24)
#	foo3	(-0x7ff0 + 0x28)
#	foo1	(-0x7ff0 + 0x2c)
#
# Any order would be acceptable, but it must match the .got dump.
#
00080c00 <\.text>:
   80c00:	8f848030 	lw	a0,-32720\(gp\)
   80c04:	8f84803c 	lw	a0,-32708\(gp\)
   80c08:	8f848034 	lw	a0,-32716\(gp\)
   80c0c:	8f848038 	lw	a0,-32712\(gp\)
   80c10:	8f848030 	lw	a0,-32720\(gp\)
   80c14:	8f84803c 	lw	a0,-32708\(gp\)
   80c18:	8f848034 	lw	a0,-32716\(gp\)
   80c1c:	8f848038 	lw	a0,-32712\(gp\)
