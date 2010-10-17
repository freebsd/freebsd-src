#objdump: -dr --prefix-addresses --show-raw-insn -M gpr-names=numeric,cp0-names=mips64r2
#name: MIPS CP0 register disassembly (mips64r2)
#source: cp0-names.s

# Check objdump's handling of -M cp0-names=foo options.

.*: +file format .*mips.*

Disassembly of section .text:
0+0000 <[^>]*> 40800000 	mtc0	\$0,c0_index
0+0004 <[^>]*> 40800800 	mtc0	\$0,c0_random
0+0008 <[^>]*> 40801000 	mtc0	\$0,c0_entrylo0
0+000c <[^>]*> 40801800 	mtc0	\$0,c0_entrylo1
0+0010 <[^>]*> 40802000 	mtc0	\$0,c0_context
0+0014 <[^>]*> 40802800 	mtc0	\$0,c0_pagemask
0+0018 <[^>]*> 40803000 	mtc0	\$0,c0_wired
0+001c <[^>]*> 40803800 	mtc0	\$0,c0_hwrena
0+0020 <[^>]*> 40804000 	mtc0	\$0,c0_badvaddr
0+0024 <[^>]*> 40804800 	mtc0	\$0,c0_count
0+0028 <[^>]*> 40805000 	mtc0	\$0,c0_entryhi
0+002c <[^>]*> 40805800 	mtc0	\$0,c0_compare
0+0030 <[^>]*> 40806000 	mtc0	\$0,c0_status
0+0034 <[^>]*> 40806800 	mtc0	\$0,c0_cause
0+0038 <[^>]*> 40807000 	mtc0	\$0,c0_epc
0+003c <[^>]*> 40807800 	mtc0	\$0,c0_prid
0+0040 <[^>]*> 40808000 	mtc0	\$0,c0_config
0+0044 <[^>]*> 40808800 	mtc0	\$0,c0_lladdr
0+0048 <[^>]*> 40809000 	mtc0	\$0,c0_watchlo
0+004c <[^>]*> 40809800 	mtc0	\$0,c0_watchhi
0+0050 <[^>]*> 4080a000 	mtc0	\$0,c0_xcontext
0+0054 <[^>]*> 4080a800 	mtc0	\$0,\$21
0+0058 <[^>]*> 4080b000 	mtc0	\$0,\$22
0+005c <[^>]*> 4080b800 	mtc0	\$0,c0_debug
0+0060 <[^>]*> 4080c000 	mtc0	\$0,c0_depc
0+0064 <[^>]*> 4080c800 	mtc0	\$0,c0_perfcnt
0+0068 <[^>]*> 4080d000 	mtc0	\$0,c0_errctl
0+006c <[^>]*> 4080d800 	mtc0	\$0,c0_cacheerr
0+0070 <[^>]*> 4080e000 	mtc0	\$0,c0_taglo
0+0074 <[^>]*> 4080e800 	mtc0	\$0,c0_taghi
0+0078 <[^>]*> 4080f000 	mtc0	\$0,c0_errorepc
0+007c <[^>]*> 4080f800 	mtc0	\$0,c0_desave
	\.\.\.
