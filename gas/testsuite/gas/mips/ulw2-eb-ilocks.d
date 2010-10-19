#as: -EB -32
#objdump: -dr --prefix-addresses --show-raw-insn -M reg-names=numeric
#name: ulw2 -EB interlocked
#source: ulw2.s

# Further checks of ulw macro.
# XXX: note: when 'move' is changed to use 'or' rather than addu/daddu, the
# XXX: 'move' opcodes shown here (whose raw instruction fields are addu/daddu)
# XXX: should be changed to be 'or' instructions and this comment should be
# XXX: removed.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> 88a40000 	lwl	\$4,0\(\$5\)
0+0004 <[^>]*> 98a40003 	lwr	\$4,3\(\$5\)
0+0008 <[^>]*> 88a40001 	lwl	\$4,1\(\$5\)
0+000c <[^>]*> 98a40004 	lwr	\$4,4\(\$5\)
0+0010 <[^>]*> 88a10000 	lwl	\$1,0\(\$5\)
0+0014 <[^>]*> 98a10003 	lwr	\$1,3\(\$5\)
0+0018 <[^>]*> 0020282[1d] 	move	\$5,\$1
0+001c <[^>]*> 88a10001 	lwl	\$1,1\(\$5\)
0+0020 <[^>]*> 98a10004 	lwr	\$1,4\(\$5\)
0+0024 <[^>]*> 0020282[1d] 	move	\$5,\$1
	\.\.\.
