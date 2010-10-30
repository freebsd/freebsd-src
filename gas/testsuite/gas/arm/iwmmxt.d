#objdump: -dr --prefix-addresses --show-raw-insn -miwmmxt
#name: Intel(r) Wireless MMX(tm) technology instructions
#as: -mcpu=xscale+iwmmxt -EL

.*: +file format .*arm.*

Disassembly of section .text:
0+00 <iwmmxt> ee13f130[ 	]+tandcb[ 	]+pc
0+04 <[^>]*> de53f130[ 	]+tandchle[ 	]+pc
0+08 <[^>]*> ae93f130[ 	]+tandcwge[ 	]+pc
0+0c <[^>]*> be401010[ 	]+tbcstblt[ 	]+wr0, r1
0+10 <[^>]*> ee412050[ 	]+tbcsth[ 	]+wr1, r2
0+14 <[^>]*> ce423090[ 	]+tbcstwgt[ 	]+wr2, r3
0+18 <[^>]*> ee13f177[ 	]+textrcb[ 	]+pc, #7
0+1c <[^>]*> 0e53f172[ 	]+textrcheq[ 	]+pc, #2
0+20 <[^>]*> ee93f170[ 	]+textrcw[ 	]+pc, #0
0+24 <[^>]*> ee13e076[ 	]+textrmub[ 	]+lr, wr3, #6
0+28 <[^>]*> 1e14d07d[ 	]+textrmsbne[ 	]+sp, wr4, #5
0+2c <[^>]*> ee55c072[ 	]+textrmuh[ 	]+ip, wr5, #2
0+30 <[^>]*> ee56b078[ 	]+textrmsh[ 	]+fp, wr6, #0
0+34 <[^>]*> 2e97a071[ 	]+textrmuwcs[ 	]+sl, wr7, #1
0+38 <[^>]*> 2e989078[ 	]+textrmswcs[ 	]+r9, wr8, #0
0+3c <[^>]*> ee698014[ 	]+tinsrb[ 	]+wr9, r8, #4
0+40 <[^>]*> 3e6a7050[ 	]+tinsrhcc[ 	]+wr10, r7, #0
0+44 <[^>]*> ee6b6091[ 	]+tinsrw[ 	]+wr11, r6, #1
0+48 <[^>]*> 3e005110[ 	]+tmcrcc[ 	]+wcid, r5
0+4c <[^>]*> ec47600c[ 	]+tmcrr[ 	]+wr12, r6, r7
0+50 <[^>]*> 3e2041b5[ 	]+tmiacc[ 	]+wr13, r5, r4
0+54 <[^>]*> 4e2821d3[ 	]+tmiaphmi[ 	]+wr14, r3, r2
0+58 <[^>]*> ee2c11f0[ 	]+tmiabb[ 	]+wr15, r0, r1
0+5c <[^>]*> 5e2d31b2[ 	]+tmiabtpl[ 	]+wr13, r2, r3
0+60 <[^>]*> 6e2d5034[ 	]+tmiabtvs[ 	]+wr1, r4, r5
0+64 <[^>]*> 7e2f7056[ 	]+tmiattvc[ 	]+wr2, r6, r7
0+68 <[^>]*> ee138030[ 	]+tmovmskb[ 	]+r8, wr3
0+6c <[^>]*> 8e549030[ 	]+tmovmskhhi[ 	]+r9, wr4
0+70 <[^>]*> 9e95a030[ 	]+tmovmskwls[ 	]+sl, wr5
0+74 <[^>]*> ee11b110[ 	]+tmrc[ 	]+fp, wcon
0+78 <[^>]*> ac5dc006[ 	]+tmrrcge[ 	]+ip, sp, wr6
0+7c <[^>]*> ee13f150[ 	]+torcb[ 	]+pc
0+80 <[^>]*> be53f150[ 	]+torchlt[ 	]+pc
0+84 <[^>]*> ee93f150[ 	]+torcw[ 	]+pc
0+88 <[^>]*> ee0871c0[ 	]+waccb[ 	]+wr7, wr8
0+8c <[^>]*> be4a91c0[ 	]+wacchlt[ 	]+wr9, wr10
0+90 <[^>]*> ce8cb1c0[ 	]+waccwgt[ 	]+wr11, wr12
0+94 <[^>]*> de0ed18f[ 	]+waddble[ 	]+wr13, wr14, wr15
0+98 <[^>]*> ee120184[ 	]+waddbus[ 	]+wr0, wr2, wr4
0+9c <[^>]*> ee38618a[ 	]+waddbss[ 	]+wr6, wr8, wr10
0+a0 <[^>]*> ee4ec18f[ 	]+waddh[ 	]+wr12, wr14, wr15
0+a4 <[^>]*> de5cd18b[ 	]+waddhusle[ 	]+wr13, wr12, wr11
0+a8 <[^>]*> 0e79a188[ 	]+waddhsseq[ 	]+wr10, wr9, wr8
0+ac <[^>]*> 1e867185[ 	]+waddwne[ 	]+wr7, wr6, wr5
0+b0 <[^>]*> ee934182[ 	]+waddwus[ 	]+wr4, wr3, wr2
0+b4 <[^>]*> 2eb0118f[ 	]+waddwsscs[ 	]+wr1, wr0, wr15
0+b8 <[^>]*> ee553027[ 	]+waligni[ 	]+wr3, wr5, wr7, #5
0+bc <[^>]*> 2e8b902d[ 	]+walignr0cs[ 	]+wr9, wr11, wr13
0+c0 <[^>]*> ee967025[ 	]+walignr1[ 	]+wr7, wr6, wr5
0+c4 <[^>]*> 3ea42028[ 	]+walignr2cc[ 	]+wr2, wr4, wr8
0+c8 <[^>]*> 3eb95021[ 	]+walignr3cc[ 	]+wr5, wr9, wr1
0+cc <[^>]*> ee283001[ 	]+wand[ 	]+wr3, wr8, wr1
0+d0 <[^>]*> ee323006[ 	]+wandn[ 	]+wr3, wr2, wr6
0+d4 <[^>]*> ee887009[ 	]+wavg2b[ 	]+wr7, wr8, wr9
0+d8 <[^>]*> decba00c[ 	]+wavg2hle[ 	]+wr10, wr11, wr12
0+dc <[^>]*> ae9ed00f[ 	]+wavg2brge[ 	]+wr13, wr14, wr15
0+e0 <[^>]*> eed1000c[ 	]+wavg2hr[ 	]+wr0, wr1, wr12
0+e4 <[^>]*> ee04d065[ 	]+wcmpeqb[ 	]+wr13, wr4, wr5
0+e8 <[^>]*> 0e474060[ 	]+wcmpeqheq[ 	]+wr4, wr7, wr0
0+ec <[^>]*> be896068[ 	]+wcmpeqwlt[ 	]+wr6, wr9, wr8
0+f0 <[^>]*> 3e121063[ 	]+wcmpgtubcc[ 	]+wr1, wr2, wr3
0+f4 <[^>]*> ee354066[ 	]+wcmpgtsb[ 	]+wr4, wr5, wr6
0+f8 <[^>]*> 3e587069[ 	]+wcmpgtuhcc[ 	]+wr7, wr8, wr9
0+fc <[^>]*> ee7ba06d[ 	]+wcmpgtsh[ 	]+wr10, wr11, wr13
0+100 <[^>]*> ee942063[ 	]+wcmpgtuw[ 	]+wr2, wr4, wr3
0+104 <[^>]*> 8eb65063[ 	]+wcmpgtswhi[ 	]+wr5, wr6, wr3
0+108 <[^>]*> ed901024[ 	]+wldrb[ 	]+wr1, \[r0, #36\]
0+10c <[^>]*> 0df12018[ 	]+wldrheq[ 	]+wr2, \[r1, #24\]!
0+110 <[^>]*> 1cb23104[ 	]+wldrwne[ 	]+wr3, \[r2\], #16
0+114 <[^>]*> 6d534153[ 	]+wldrdvs[ 	]+wr4, \[r3, #-332\]
0+118 <[^>]*> fdb12105[ 	]+wldrw[ 	]+wcssf, \[r1, #20\]!
0+11c <[^>]*> ee474109[ 	]+wmacu[ 	]+wr4, wr7, wr9
0+120 <[^>]*> 2e6a810e[ 	]+wmacscs[ 	]+wr8, wr10, wr14
0+124 <[^>]*> ee5cf10b[ 	]+wmacuz[ 	]+wr15, wr12, wr11
0+128 <[^>]*> ee78310a[ 	]+wmacsz[ 	]+wr3, wr8, wr10
0+12c <[^>]*> ee8bc107[ 	]+wmaddu[ 	]+wr12, wr11, wr7
0+130 <[^>]*> cea3510f[ 	]+wmaddsgt[ 	]+wr5, wr3, wr15
0+134 <[^>]*> 2e043165[ 	]+wmaxubcs[ 	]+wr3, wr4, wr5
0+138 <[^>]*> ee243165[ 	]+wmaxsb[ 	]+wr3, wr4, wr5
0+13c <[^>]*> 5e443165[ 	]+wmaxuhpl[ 	]+wr3, wr4, wr5
0+140 <[^>]*> 4e643165[ 	]+wmaxshmi[ 	]+wr3, wr4, wr5
0+144 <[^>]*> ae843165[ 	]+wmaxuwge[ 	]+wr3, wr4, wr5
0+148 <[^>]*> dea43165[ 	]+wmaxswle[ 	]+wr3, wr4, wr5
0+14c <[^>]*> 3e1c416a[ 	]+wminubcc[ 	]+wr4, wr12, wr10
0+150 <[^>]*> ee3c416a[ 	]+wminsb[ 	]+wr4, wr12, wr10
0+154 <[^>]*> 7e5c416a[ 	]+wminuhvc[ 	]+wr4, wr12, wr10
0+158 <[^>]*> ee7c416a[ 	]+wminsh[ 	]+wr4, wr12, wr10
0+15c <[^>]*> ee9c416a[ 	]+wminuw[ 	]+wr4, wr12, wr10
0+160 <[^>]*> 3ebc416a[ 	]+wminswcc[ 	]+wr4, wr12, wr10
0+164 <[^>]*> 0e043004[ 	]+woreq[ 	]+wr3, wr4, wr4
0+168 <[^>]*> ee112108[ 	]+wmulum[ 	]+wr2, wr1, wr8
0+16c <[^>]*> ee312108[ 	]+wmulsm[ 	]+wr2, wr1, wr8
0+170 <[^>]*> ee012108[ 	]+wmulul[ 	]+wr2, wr1, wr8
0+174 <[^>]*> de212108[ 	]+wmulslle[ 	]+wr2, wr1, wr8
0+178 <[^>]*> 0e08b00e[ 	]+woreq[ 	]+wr11, wr8, wr14
0+17c <[^>]*> 0e510083[ 	]+wpackhuseq[ 	]+wr0, wr1, wr3
0+180 <[^>]*> ee910083[ 	]+wpackwus[ 	]+wr0, wr1, wr3
0+184 <[^>]*> eed10083[ 	]+wpackdus[ 	]+wr0, wr1, wr3
0+188 <[^>]*> 8e710083[ 	]+wpackhsshi[ 	]+wr0, wr1, wr3
0+18c <[^>]*> eeb10083[ 	]+wpackwss[ 	]+wr0, wr1, wr3
0+190 <[^>]*> 0ef10083[ 	]+wpackdsseq[ 	]+wr0, wr1, wr3
0+194 <[^>]*> ee754046[ 	]+wrorh[ 	]+wr4, wr5, wr6
0+198 <[^>]*> 4eb54046[ 	]+wrorwmi[ 	]+wr4, wr5, wr6
0+19c <[^>]*> eef54046[ 	]+wrord[ 	]+wr4, wr5, wr6
0+1a0 <[^>]*> ee7a9148[ 	]+wrorhg[ 	]+wr9, wr10, wcgr0
0+1a4 <[^>]*> aeba9149[ 	]+wrorwgge[ 	]+wr9, wr10, wcgr1
0+1a8 <[^>]*> eefa914a[ 	]+wrordg[ 	]+wr9, wr10, wcgr2
0+1ac <[^>]*> ee00212a[ 	]+wsadb[ 	]+wr2, wr0, wr10
0+1b0 <[^>]*> ee40212a[ 	]+wsadh[ 	]+wr2, wr0, wr10
0+1b4 <[^>]*> ee10212a[ 	]+wsadbz[ 	]+wr2, wr0, wr10
0+1b8 <[^>]*> de50212a[ 	]+wsadhzle[ 	]+wr2, wr0, wr10
0+1bc <[^>]*> 0ef941eb[ 	]+wshufheq[ 	]+wr4, wr9, #251
0+1c0 <[^>]*> ee592044[ 	]+wsllh[ 	]+wr2, wr9, wr4
0+1c4 <[^>]*> ee992044[ 	]+wsllw[ 	]+wr2, wr9, wr4
0+1c8 <[^>]*> 0ed92044[ 	]+wslldeq[ 	]+wr2, wr9, wr4
0+1cc <[^>]*> 0e59214b[ 	]+wsllhgeq[ 	]+wr2, wr9, wcgr3
0+1d0 <[^>]*> 7e99214a[ 	]+wsllwgvc[ 	]+wr2, wr9, wcgr2
0+1d4 <[^>]*> eed92149[ 	]+wslldg[ 	]+wr2, wr9, wcgr1
0+1d8 <[^>]*> ee451047[ 	]+wsrah[ 	]+wr1, wr5, wr7
0+1dc <[^>]*> ee851047[ 	]+wsraw[ 	]+wr1, wr5, wr7
0+1e0 <[^>]*> 0ec51047[ 	]+wsradeq[ 	]+wr1, wr5, wr7
0+1e4 <[^>]*> ee45114b[ 	]+wsrahg[ 	]+wr1, wr5, wcgr3
0+1e8 <[^>]*> 4e851148[ 	]+wsrawgmi[ 	]+wr1, wr5, wcgr0
0+1ec <[^>]*> eec51149[ 	]+wsradg[ 	]+wr1, wr5, wcgr1
0+1f0 <[^>]*> ee651047[ 	]+wsrlh[ 	]+wr1, wr5, wr7
0+1f4 <[^>]*> eea51047[ 	]+wsrlw[ 	]+wr1, wr5, wr7
0+1f8 <[^>]*> 0ee51047[ 	]+wsrldeq[ 	]+wr1, wr5, wr7
0+1fc <[^>]*> ee65114b[ 	]+wsrlhg[ 	]+wr1, wr5, wcgr3
0+200 <[^>]*> 4ea51148[ 	]+wsrlwgmi[ 	]+wr1, wr5, wcgr0
0+204 <[^>]*> eee51149[ 	]+wsrldg[ 	]+wr1, wr5, wcgr1
0+208 <[^>]*> ed8110ff[ 	]+wstrb[ 	]+wr1, \[r1, #255\]
0+20c <[^>]*> ed6110ff[ 	]+wstrh[ 	]+wr1, \[r1, #-255\]!
0+210 <[^>]*> eca11101[ 	]+wstrw[ 	]+wr1, \[r1\], #4
0+214 <[^>]*> edc111ff[ 	]+wstrd[ 	]+wr1, \[r1, #1020\]
0+218 <[^>]*> fca1314b[ 	]+wstrw[ 	]+wcasf, \[r1\], #300
0+21c <[^>]*> 3e1311ae[ 	]+wsubbuscc[ 	]+wr1, wr3, wr14
0+220 <[^>]*> ee5311ae[ 	]+wsubhus[ 	]+wr1, wr3, wr14
0+224 <[^>]*> 3e9311ae[ 	]+wsubwuscc[ 	]+wr1, wr3, wr14
0+228 <[^>]*> 3e3311ae[ 	]+wsubbsscc[ 	]+wr1, wr3, wr14
0+22c <[^>]*> 3e7311ae[ 	]+wsubhsscc[ 	]+wr1, wr3, wr14
0+230 <[^>]*> eeb311ae[ 	]+wsubwss[ 	]+wr1, wr3, wr14
0+234 <[^>]*> ee0630c0[ 	]+wunpckehub[ 	]+wr3, wr6
0+238 <[^>]*> 4e4630c0[ 	]+wunpckehuhmi[ 	]+wr3, wr6
0+23c <[^>]*> ee8630c0[ 	]+wunpckehuw[ 	]+wr3, wr6
0+240 <[^>]*> ee2630c0[ 	]+wunpckehsb[ 	]+wr3, wr6
0+244 <[^>]*> ee6630c0[ 	]+wunpckehsh[ 	]+wr3, wr6
0+248 <[^>]*> 0ea630c0[ 	]+wunpckehsweq[ 	]+wr3, wr6
0+24c <[^>]*> ee1c50ca[ 	]+wunpckihb[ 	]+wr5, wr12, wr10
0+250 <[^>]*> 8e5c50ca[ 	]+wunpckihhhi[ 	]+wr5, wr12, wr10
0+254 <[^>]*> ee9c50ca[ 	]+wunpckihw[ 	]+wr5, wr12, wr10
0+258 <[^>]*> ee0530e0[ 	]+wunpckelub[ 	]+wr3, wr5
0+25c <[^>]*> 1e4530e0[ 	]+wunpckeluhne[ 	]+wr3, wr5
0+260 <[^>]*> ee8530e0[ 	]+wunpckeluw[ 	]+wr3, wr5
0+264 <[^>]*> ce2530e0[ 	]+wunpckelsbgt[ 	]+wr3, wr5
0+268 <[^>]*> ee6530e0[ 	]+wunpckelsh[ 	]+wr3, wr5
0+26c <[^>]*> eea530e0[ 	]+wunpckelsw[ 	]+wr3, wr5
0+270 <[^>]*> ee1540ea[ 	]+wunpckilb[ 	]+wr4, wr5, wr10
0+274 <[^>]*> ee5540ea[ 	]+wunpckilh[ 	]+wr4, wr5, wr10
0+278 <[^>]*> 0e9540ea[ 	]+wunpckilweq[ 	]+wr4, wr5, wr10
0+27c <[^>]*> 1e143005[ 	]+wxorne[ 	]+wr3, wr4, wr5
0+280 <[^>]*> ae377007[ 	]+wandnge[ 	]+wr7, wr7, wr7
0+284 <[^>]*> ee080110[ 	]+tmcr[ 	]+wcgr0, r0
0+288 <[^>]*> ee1a1110[ 	]+tmrc[ 	]+r1, wcgr2
0+28c <[^>]*> e1a00000[ 	]+nop[ 	]+\(mov r0,r0\)
