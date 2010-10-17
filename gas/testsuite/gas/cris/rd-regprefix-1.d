#objdump: -dr
#as: --underscore
#name: Register prefixes 1 defaulted to no.

.*:[ 	]+file format .*-cris
Disassembly of section \.text:
0+ <start>:
[ 	]+0:[ 	]+6556[	 ]+test\.d[	 ]+r5
[ 	]+2:[ 	]+3496[	 ]+move[	 ]+r4,ibr
[ 	]+4:[ 	]+01a1 e44b[ 	]+move\.d[ 	]+r4,\[r10\+1\]
[ 	]+8:[ 	]+bab9[	 ]+jsr[	 ]+r10
[ 	]+a:[ 	]+607a[	 ]+move\.d[	 ]+\[r0\],r7
[ 	]+c:[ 	]fce1 7ebe[	 ]+push[	 ]+srp
[ 	]+10:[ 	]+74a6[	 ]+move[	 ]+irp,r4
[ 	]+12:[ 	]+40a5 e44b[	 ]+move\.d[	 ]+r4,\[r0\+r10\.b\]
[ 	]+16:[ 	]+6ffd 0000 0000 705a[ 	]+move[	 ]+ccr,\[pc\+0[	 ]+<start>\]
[ 	]+18:[ 	]+(R_CRIS_)?32[	 ]+r16
[ 	]+1e:[ 	]fce1 7ebe[	 ]+push[	 ]+srp
[ 	]+22:[ 	]+60a5 e44b[	 ]+move\.d[	 ]+r4,\[r0\+r10\.d\]
[ 	]+26:[ 	]+6ffd 0000 0000 705a[	 ]+move[	 ]+ccr,\[pc\+0[	 ]+<start>\]
[ 	]+28:[ 	]+(R_CRIS_)?32[	 ]+r16
[ 	]+2e:[ 	]+6556[	 ]+test\.d[	 ]+r5
[ 	]+30:[ 	]+3496[	 ]+move[	 ]+r4,ibr
[ 	]+32:[ 	]+01a1 e44b[	 ]+move\.d[	 ]+r4,\[r10\+1\]
[ 	]+36:[ 	]+bab9[	 ]+jsr[	 ]+r10
[ 	]+38:[ 	]+6f5e 0000 0000[	 ]+move\.d[	 ]+0[	 ]+<start>,r5
[ 	]+3a:[ 	]+(R_CRIS_)?32[	 ]+r5
[ 	]+3e:[ 	]+3f9e 0000 0000[	 ]+move[	 ]+0[	 ]+<start>,ibr
[ 	]+40:[ 	]+(R_CRIS_)?32[	 ]+r4
[ 	]+44:[ 	]+7f0d 0100 0000 e44b[	 ]+move\.d[	 ]+r4,\[1[	 ]+<start\+0x1>\]
[ 	]+46:[ 	]+(R_CRIS_)?32[	 ]+r10\+0x1
[ 	]+4c:[ 	]+3fbd 0000 0000[	 ]+jsr[	 ]+0[	 ]+<start>
[ 	]+4e:[ 	]+(R_CRIS_)?32[	 ]+r10
[ 	]+\.\.\.
