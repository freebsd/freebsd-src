#as: -EL
#objdump: -dr --prefix-addresses --show-raw-insn -M reg-names=numeric
#name: uld2 -EL
#source: uld2.s
#stderr: uld2.l

# Further checks of uld macro.
# XXX: note: when 'move' is changed to use 'or' rather than daddu, the
# XXX: 'move' opcodes shown here (whose raw instruction fields are daddu)
# XXX: should be changed to be 'or' instructions and this comment should be
# XXX: removed.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> 68a40007 	ldl	\$4,7\(\$5\)
0+0004 <[^>]*> 6ca40000 	ldr	\$4,0\(\$5\)
0+0008 <[^>]*> 68a40008 	ldl	\$4,8\(\$5\)
0+000c <[^>]*> 6ca40001 	ldr	\$4,1\(\$5\)
0+0010 <[^>]*> 68a10007 	ldl	\$1,7\(\$5\)
0+0014 <[^>]*> 6ca10000 	ldr	\$1,0\(\$5\)
0+0018 <[^>]*> 0020282[1d] 	move	\$5,\$1
0+001c <[^>]*> 68a10008 	ldl	\$1,8\(\$5\)
0+0020 <[^>]*> 6ca10001 	ldr	\$1,1\(\$5\)
0+0024 <[^>]*> 0020282[1d] 	move	\$5,\$1
	\.\.\.
