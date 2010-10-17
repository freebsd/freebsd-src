#objdump: -dr --prefix-addresses --show-raw-insn -mmips:isa32r2 -M gpr-names=numeric,hwr-names=mips32r2
#name: MIPS HWR disassembly (mips32r2)
#as: -32 -mips32r2
#source: hwr-names.s

# Check objdump's handling of -M hwr-names=foo options.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> 7c00003b 	rdhwr	\$0,hwr_cpunum
0+0004 <[^>]*> 7c00083b 	rdhwr	\$0,hwr_synci_step
0+0008 <[^>]*> 7c00103b 	rdhwr	\$0,hwr_cc
0+000c <[^>]*> 7c00183b 	rdhwr	\$0,hwr_ccres
0+0010 <[^>]*> 7c00203b 	rdhwr	\$0,\$4
0+0014 <[^>]*> 7c00283b 	rdhwr	\$0,\$5
0+0018 <[^>]*> 7c00303b 	rdhwr	\$0,\$6
0+001c <[^>]*> 7c00383b 	rdhwr	\$0,\$7
0+0020 <[^>]*> 7c00403b 	rdhwr	\$0,\$8
0+0024 <[^>]*> 7c00483b 	rdhwr	\$0,\$9
0+0028 <[^>]*> 7c00503b 	rdhwr	\$0,\$10
0+002c <[^>]*> 7c00583b 	rdhwr	\$0,\$11
0+0030 <[^>]*> 7c00603b 	rdhwr	\$0,\$12
0+0034 <[^>]*> 7c00683b 	rdhwr	\$0,\$13
0+0038 <[^>]*> 7c00703b 	rdhwr	\$0,\$14
0+003c <[^>]*> 7c00783b 	rdhwr	\$0,\$15
0+0040 <[^>]*> 7c00803b 	rdhwr	\$0,\$16
0+0044 <[^>]*> 7c00883b 	rdhwr	\$0,\$17
0+0048 <[^>]*> 7c00903b 	rdhwr	\$0,\$18
0+004c <[^>]*> 7c00983b 	rdhwr	\$0,\$19
0+0050 <[^>]*> 7c00a03b 	rdhwr	\$0,\$20
0+0054 <[^>]*> 7c00a83b 	rdhwr	\$0,\$21
0+0058 <[^>]*> 7c00b03b 	rdhwr	\$0,\$22
0+005c <[^>]*> 7c00b83b 	rdhwr	\$0,\$23
0+0060 <[^>]*> 7c00c03b 	rdhwr	\$0,\$24
0+0064 <[^>]*> 7c00c83b 	rdhwr	\$0,\$25
0+0068 <[^>]*> 7c00d03b 	rdhwr	\$0,\$26
0+006c <[^>]*> 7c00d83b 	rdhwr	\$0,\$27
0+0070 <[^>]*> 7c00e03b 	rdhwr	\$0,\$28
0+0074 <[^>]*> 7c00e83b 	rdhwr	\$0,\$29
0+0078 <[^>]*> 7c00f03b 	rdhwr	\$0,\$30
0+007c <[^>]*> 7c00f83b 	rdhwr	\$0,\$31
	\.\.\.
