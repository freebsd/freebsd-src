#name: ARM V6 instructions
#as: -march=armv6zk
#objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]*> f57ff01f ?	clrex
0+004 <[^>]*> e1dc4f9f ?	ldrexb	r4, \[ip\]
0+008 <[^>]*> 11d4cf9f ?	ldrexbne	ip, \[r4\]
0+00c <[^>]*> e1bc4f9f ?	ldrexd	r4, \[ip\]
0+010 <[^>]*> 11b4cf9f ?	ldrexdne	ip, \[r4\]
0+014 <[^>]*> e1fc4f9f ?	ldrexh	r4, \[ip\]
0+018 <[^>]*> 11f4cf9f ?	ldrexhne	ip, \[r4\]
0+01c <[^>]*> e320f080 ?	nop	\{128\}
0+020 <[^>]*> 1320f07f ?	nopne	\{127\}
0+024 <[^>]*> e320f004 ?	sev
0+028 <[^>]*> e1c74f9c ?	strexb	r4, ip, \[r7\]
0+02c <[^>]*> 11c8cf94 ?	strexbne	ip, r4, \[r8\]
0+030 <[^>]*> e1a74f9c ?	strexd	r4, ip, \[r7\]
0+034 <[^>]*> 11a8cf94 ?	strexdne	ip, r4, \[r8\]
0+038 <[^>]*> e1e74f9c ?	strexh	r4, ip, \[r7\]
0+03c <[^>]*> 11e8cf94 ?	strexhne	ip, r4, \[r8\]
0+040 <[^>]*> e320f002 ?	wfe
0+044 <[^>]*> e320f003 ?	wfi
0+048 <[^>]*> e320f001 ?	yield
0+04c <[^>]*> e16ec371 ?	smc	60465
0+050 <[^>]*> 11613c7e ?	smcne	5070
0+054 <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
0+058 <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)
0+05c <[^>]*> e1a00000 ?	nop[ 	]+\(mov r0,r0\)

