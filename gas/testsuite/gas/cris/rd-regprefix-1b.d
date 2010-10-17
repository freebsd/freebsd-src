#objdump: -dr
#as: --no-underscore --em=criself
#source: rd-regprefix-1.s
#name: Register prefixes 1 defaulted to yes.

# We have to force ELF here, since --no-underscore is invalid with a.out
# (separately tested).  We make sure we get the target translation to
# elf32-cris (not elf32-us-cris) as well so we spell out the target name.

.*:[ 	]+file format elf32-cris
Disassembly of section \.text:
0+ <start>:
[ 	]+0:[ 	]+6f5e 0000 0000[ 	]+move\.d[ 	]+0[ 	]+<start>,\$r5
[ 	]+2:[ 	]+(R_CRIS_)?32[ 	]+r5
[ 	]+6:[ 	]+3f9e 0000 0000[ 	]+move[ 	]+0[ 	]+<start>,\$ibr
[ 	]+8:[ 	]+(R_CRIS_)?32[ 	]+r4
[ 	]+c:[ 	]+7f0d 0100 0000 e44b[ 	]+move\.d[ 	]+\$r4,\[1[ 	]+<start\+0x1>\]
[ 	]+e:[ 	]+(R_CRIS_)?32[ 	]+r10\+0x1
[ 	]+14:[ 	]+3fbd 0000 0000[ 	]+jsr[ 	]+0[ 	]+<start>
[ 	]+16:[ 	]+(R_CRIS_)?32[ 	]+r10
[ 	]+1a:[ 	]+7f0d 0000 0000[ 	]+677a[ 	]+move\.d[ 	]+\[0[ 	]+<start>\],\$r7
[ 	]+1c:[ 	]+(R_CRIS_)?32[ 	]+r0
[ 	]+22:[ 	]+fce1 7ebe[ 	]+push[ 	]+\$srp
[ 	]+26:[ 	]+74a6[ 	]+move[ 	]+\$irp,\$r4
[ 	]+28:[ 	]+40a5 e44b[ 	]+move\.d[ 	]+\$r4,\[\$r0\+\$r10\.b\]
[ 	]+2c:[ 	]+6ffd 0000 0000 705a[ 	]+move[ 	]+\$ccr,\[\$pc\+0[ 	]+<start>\]
[ 	]+2e:[ 	]+(R_CRIS_)?32[ 	]+r16
[ 	]+34:[ 	]+fce1 7ebe[ 	]+push[ 	]+\$srp
[ 	]+38:[ 	]+60a5 e44b[ 	]+move\.d[ 	]+\$r4,\[\$r0\+\$r10\.d\]
[ 	]+3c:[ 	]+6ffd 0000 0000 705a[ 	]+move[ 	]+\$ccr,\[\$pc\+0[ 	]+<start>\]
[ 	]+3e:[ 	]+(R_CRIS_)?32[ 	]+r16
[ 	]+44:[ 	]+6556[ 	]+test\.d[ 	]+\$r5
[ 	]+46:[ 	]+3496[ 	]+move[ 	]+\$r4,\$ibr
[ 	]+48:[ 	]+01a1 e44b[ 	]+move\.d[ 	]+\$r4,\[\$r10\+1\]
[ 	]+4c:[ 	]+bab9[ 	]+jsr[ 	]+\$r10
[ 	]+4e:[ 	]+6f5e 0000 0000[ 	]+move\.d[ 	]+0[ 	]+<start>,\$r5
[ 	]+50:[ 	]+(R_CRIS_)?32[ 	]+r5
[ 	]+54:[ 	]+3f9e 0000 0000[ 	]+move[ 	]+0[ 	]+<start>,\$ibr
[ 	]+56:[ 	]+(R_CRIS_)?32[ 	]+r4
[ 	]+5a:[ 	]+7f0d 0100 0000[ 	]+e44b[ 	]+move\.d[ 	]+\$r4,\[1[ 	]+<start\+0x1>\]
[ 	]+5c:[ 	]+(R_CRIS_)?32[ 	]+r10\+0x1
[ 	]+62:[ 	]+3fbd 0000 0000[ 	]+jsr[ 	]+0[ 	]+<start>
[ 	]+64:[ 	]+(R_CRIS_)?32[ 	]+r10
