#objdump: -dr --prefix-addresses --show-raw-insn -miwmmxt
#name: Intel(r) Wireless MMX(tm) technology instructions version 2
#as: -mcpu=xscale+iwmmxt+iwmmxt2 -EL

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <iwmmxt2> ee654186[ 	]+waddhc[ 	]+wr4, wr5, wr6
0+004 <[^>]*> eea87189[ 	]+waddwc[ 	]+wr7, wr8, wr9
0+008 <[^>]*> ce954106[ 	]+wmadduxgt[ 	]+wr4, wr5, wr6
0+00c <[^>]*> 0ec87109[ 	]+wmadduneq[ 	]+wr7, wr8, wr9
0+010 <[^>]*> 1eb54106[ 	]+wmaddsxne[ 	]+wr4, wr5, wr6
0+014 <[^>]*> aee87109[ 	]+wmaddsnge[ 	]+wr7, wr8, wr9
0+018 <[^>]*> eed21103[ 	]+wmulumr[ 	]+wr1, wr2, wr3
0+01c <[^>]*> eef21103[ 	]+wmulsmr[ 	]+wr1, wr2, wr3
0+020 <[^>]*> ce13f190[ 	]+torvscbgt[ 	]+pc
0+024 <[^>]*> 1e53f190[ 	]+torvschne[ 	]+pc
0+028 <[^>]*> 0e93f190[ 	]+torvscweq[ 	]+pc
0+02c <[^>]*> ee2211c0[ 	]+wabsb[ 	]+wr1, wr2
0+030 <[^>]*> ee6431c0[ 	]+wabsh[ 	]+wr3, wr4
0+034 <[^>]*> eea651c0[ 	]+wabsw[ 	]+wr5, wr6
0+038 <[^>]*> ce2211c0[ 	]+wabsbgt[ 	]+wr1, wr2
0+03c <[^>]*> ee1211c3[ 	]+wabsdiffb[ 	]+wr1, wr2, wr3
0+040 <[^>]*> ee5541c6[ 	]+wabsdiffh[ 	]+wr4, wr5, wr6
0+044 <[^>]*> ee9871c9[ 	]+wabsdiffw[ 	]+wr7, wr8, wr9
0+048 <[^>]*> ce1211c3[ 	]+wabsdiffbgt[ 	]+wr1, wr2, wr3
0+04c <[^>]*> ee6211a3[ 	]+waddbhusm[ 	]+wr1, wr2, wr3
0+050 <[^>]*> ee2541a6[ 	]+waddbhusl[ 	]+wr4, wr5, wr6
0+054 <[^>]*> ce6211a3[ 	]+waddbhusmgt[ 	]+wr1, wr2, wr3
0+058 <[^>]*> ce2541a6[ 	]+waddbhuslgt[ 	]+wr4, wr5, wr6
0+05c <[^>]*> ee421003[ 	]+wavg4[ 	]+wr1, wr2, wr3
0+060 <[^>]*> ce454006[ 	]+wavg4gt[ 	]+wr4, wr5, wr6
0+064 <[^>]*> ee521003[ 	]+wavg4r[ 	]+wr1, wr2, wr3
0+068 <[^>]*> ce554006[ 	]+wavg4rgt[ 	]+wr4, wr5, wr6
0+06c <[^>]*> fc711102[ 	]+wldrd[ 	]+wr1, \[r1\], -r2
0+070 <[^>]*> fc712132[ 	]+wldrd[ 	]+wr2, \[r1\], -r2, lsl #3
0+074 <[^>]*> fcf13102[ 	]+wldrd[ 	]+wr3, \[r1\], \+r2
0+078 <[^>]*> fcf14142[ 	]+wldrd[ 	]+wr4, \[r1\], \+r2, lsl #4
0+07c <[^>]*> fd515102[ 	]+wldrd[ 	]+wr5, \[r1, -r2\]
0+080 <[^>]*> fd516132[ 	]+wldrd[ 	]+wr6, \[r1, -r2, lsl #3\]
0+084 <[^>]*> fdd17102[ 	]+wldrd[ 	]wr7, \[r1, \+r2\]
0+088 <[^>]*> fdd18142[ 	]+wldrd[ 	]wr8, \[r1, \+r2, lsl #4\]
0+08c <[^>]*> fd719102[ 	]+wldrd[ 	]wr9, \[r1, -r2\]!
0+090 <[^>]*> fd71a132[ 	]+wldrd[ 	]wr10, \[r1, -r2, lsl #3\]!
0+094 <[^>]*> fdf1b102[ 	]+wldrd[ 	]wr11, \[r1, \+r2\]!
0+098 <[^>]*> fdf1c142[ 	]+wldrd[ 	]wr12, \[r1, \+r2, lsl #4\]!
0+09c <[^>]*> ee821083[ 	]+wmerge[ 	]wr1, wr2, wr3, #4
0+0a0 <[^>]*> ce821083[ 	]+wmergegt[ 	]wr1, wr2, wr3, #4
0+0a4 <[^>]*> 0e3210a3[ 	]+wmiatteq[ 	]wr1, wr2, wr3
0+0a8 <[^>]*> ce2210a3[ 	]+wmiatbgt[ 	]wr1, wr2, wr3
0+0ac <[^>]*> 1e1210a3[ 	]+wmiabtne[ 	]wr1, wr2, wr3
0+0b0 <[^>]*> ce0210a3[ 	]+wmiabbgt[ 	]wr1, wr2, wr3
0+0b4 <[^>]*> 0e7210a3[ 	]+wmiattneq[ 	]wr1, wr2, wr3
0+0b8 <[^>]*> 1e6210a3[ 	]+wmiatbnne[ 	]wr1, wr2, wr3
0+0bc <[^>]*> ce5210a3[ 	]+wmiabtngt[ 	]wr1, wr2, wr3
0+0c0 <[^>]*> 0e4210a3[ 	]+wmiabbneq[ 	]wr1, wr2, wr3
0+0c4 <[^>]*> 0eb21123[ 	]+wmiawtteq[ 	]wr1, wr2, wr3
0+0c8 <[^>]*> cea21123[ 	]+wmiawtbgt[ 	]wr1, wr2, wr3
0+0cc <[^>]*> 1e921123[ 	]+wmiawbtne[ 	]wr1, wr2, wr3
0+0d0 <[^>]*> ce821123[ 	]+wmiawbbgt[ 	]wr1, wr2, wr3
0+0d4 <[^>]*> 1ef21123[ 	]+wmiawttnne[ 	]wr1, wr2, wr3
0+0d8 <[^>]*> cee21123[ 	]+wmiawtbngt[ 	]wr1, wr2, wr3
0+0dc <[^>]*> 0ed21123[ 	]+wmiawbtneq[ 	]wr1, wr2, wr3
0+0e0 <[^>]*> 1ec21123[ 	]+wmiawbbnne[ 	]wr1, wr2, wr3
0+0e4 <[^>]*> 0ed210c3[ 	]+wmulwumeq[ 	]wr1, wr2, wr3
0+0e8 <[^>]*> cec210c3[ 	]+wmulwumrgt[ 	]wr1, wr2, wr3
0+0ec <[^>]*> 1ef210c3[ 	]+wmulwsmne[ 	]wr1, wr2, wr3
0+0f0 <[^>]*> 0ee210c3[ 	]+wmulwsmreq[ 	]wr1, wr2, wr3
0+0f4 <[^>]*> ceb210c3[ 	]+wmulwlgt[ 	]wr1, wr2, wr3
0+0f8 <[^>]*> aeb210c3[ 	]+wmulwlge[ 	]wr1, wr2, wr3
0+0fc <[^>]*> 1eb210a3[ 	]+wqmiattne[ 	]wr1, wr2, wr3
0+100 <[^>]*> 0ef210a3[ 	]+wqmiattneq[ 	]wr1, wr2, wr3
0+104 <[^>]*> cea210a3[ 	]+wqmiatbgt[ 	]wr1, wr2, wr3
0+108 <[^>]*> aee210a3[ 	]+wqmiatbnge[ 	]wr1, wr2, wr3
0+10c <[^>]*> 1e9210a3[ 	]+wqmiabtne[ 	]wr1, wr2, wr3
0+110 <[^>]*> 0ed210a3[ 	]+wqmiabtneq[ 	]wr1, wr2, wr3
0+114 <[^>]*> ce8210a3[ 	]+wqmiabbgt[ 	]wr1, wr2, wr3
0+118 <[^>]*> 1ec210a3[ 	]+wqmiabbnne[ 	]wr1, wr2, wr3
0+11c <[^>]*> ce121083[ 	]+wqmulmgt[ 	]wr1, wr2, wr3
0+120 <[^>]*> 0e321083[ 	]+wqmulmreq[ 	]wr1, wr2, wr3
0+124 <[^>]*> cec210e3[ 	]+wqmulwmgt[ 	]wr1, wr2, wr3
0+128 <[^>]*> 0ee210e3[ 	]+wqmulwmreq[ 	]wr1, wr2, wr3
0+12c <[^>]*> fc611102[ 	]+wstrd[ 	]+wr1, \[r1\], -r2
0+130 <[^>]*> fc612132[ 	]+wstrd[ 	]+wr2, \[r1\], -r2, lsl #3
0+134 <[^>]*> fce13102[ 	]+wstrd[ 	]+wr3, \[r1\], \+r2
0+138 <[^>]*> fce14142[ 	]+wstrd[ 	]+wr4, \[r1\], \+r2, lsl #4
0+13c <[^>]*> fd415102[ 	]+wstrd[ 	]+wr5, \[r1, -r2\]
0+140 <[^>]*> fd416132[ 	]+wstrd[ 	]+wr6, \[r1, -r2, lsl #3\]
0+144 <[^>]*> fdc17102[ 	]+wstrd[ 	]wr7, \[r1, \+r2\]
0+148 <[^>]*> fdc18142[ 	]+wstrd[ 	]wr8, \[r1, \+r2, lsl #4\]
0+14c <[^>]*> fd619102[ 	]+wstrd[ 	]wr9, \[r1, -r2\]!
0+150 <[^>]*> fd61a132[ 	]+wstrd[ 	]wr10, \[r1, -r2, lsl #3\]!
0+154 <[^>]*> fde1b102[ 	]+wstrd[ 	]wr11, \[r1, \+r2\]!
0+158 <[^>]*> fde1c142[ 	]+wstrd[ 	]wr12, \[r1, \+r2, lsl #4\]!
0+15c <[^>]*> ced211c3[ 	]+wsubaddhxgt[ 	]wr1, wr2, wr3
0+160 <[^>]*> fe721140[ 	]+wrorh[ 	]wr1, wr2, #16
0+164 <[^>]*> feb21040[ 	]+wrorw[ 	]wr1, wr2, #32
0+168 <[^>]*> ee021002[ 	]+wor[ 	]wr1, wr2, wr2
0+16c <[^>]*> fe721145[ 	]+wrorh[ 	]wr1, wr2, #21
0+170 <[^>]*> feb2104d[ 	]+wrorw[ 	]wr1, wr2, #13
0+174 <[^>]*> fef2104e[ 	]+wrord[ 	]wr1, wr2, #14
0+178 <[^>]*> fe721140[ 	]+wrorh[ 	]wr1, wr2, #16
0+17c <[^>]*> feb21040[ 	]+wrorw[ 	]wr1, wr2, #32
0+180 <[^>]*> ee021002[ 	]+wor[ 	]wr1, wr2, wr2
0+184 <[^>]*> fe59204b[ 	]+wsllh[ 	]wr2, wr9, #11
0+188 <[^>]*> fe95304d[ 	]+wsllw[ 	]wr3, wr5, #13
0+18c <[^>]*> fed8304f[ 	]+wslld[ 	]wr3, wr8, #15
0+190 <[^>]*> fe721140[ 	]+wrorh[ 	]wr1, wr2, #16
0+194 <[^>]*> feb21040[ 	]+wrorw[ 	]wr1, wr2, #32
0+198 <[^>]*> ee021002[ 	]+wor[ 	]wr1, wr2, wr2
0+19c <[^>]*> fe49204c[ 	]+wsrah[ 	]wr2, wr9, #12
0+1a0 <[^>]*> fe85304e[ 	]+wsraw[ 	]wr3, wr5, #14
0+1a4 <[^>]*> fec83140[ 	]+wsrad[ 	]wr3, wr8, #16
0+1a8 <[^>]*> fe721140[ 	]+wrorh[ 	]wr1, wr2, #16
0+1ac <[^>]*> feb21040[ 	]+wrorw[ 	]wr1, wr2, #32
0+1b0 <[^>]*> ee021002[ 	]+wor[ 	]wr1, wr2, wr2
0+1b4 <[^>]*> fe69204c[ 	]+wsrlh[ 	]wr2, wr9, #12
0+1b8 <[^>]*> fea5304e[ 	]+wsrlw[ 	]wr3, wr5, #14
0+1bc <[^>]*> fee83140[ 	]+wsrld[ 	]wr3, wr8, #16
