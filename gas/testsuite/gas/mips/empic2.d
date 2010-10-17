#objdump: --prefix-addresses -dr --show-raw-insn -mmips:4000
#name: MIPS empic2
#as: -mabi=o64 -membedded-pic -mips3

# Check assembly of and relocs for -membedded-pic la, lw, ld, sw, sd macros.

.*: +file format elf.*mips.*

Disassembly of section .text:
0+000000 <[^>]*> 00000000 	nop
	...
	...
0+01000c <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*1000c: R_MIPS_GNU_REL_HI16	.text
0+010010 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010014 <[^>]*> 6442000c 	daddiu	v0,v0,12
[ 	]*10014: R_MIPS_GNU_REL_LO16	.text
0+010018 <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*10018: R_MIPS_GNU_REL_HI16	.text
0+01001c <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010020 <[^>]*> 64420018 	daddiu	v0,v0,24
[ 	]*10020: R_MIPS_GNU_REL_LO16	.text
0+010024 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*10024: R_MIPS_GNU_REL_HI16	.text
0+010028 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+01002c <[^>]*> 64428028 	daddiu	v0,v0,-32728
[ 	]*1002c: R_MIPS_GNU_REL_LO16	.text
0+010030 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*10030: R_MIPS_GNU_REL_HI16	.text
0+010034 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010038 <[^>]*> 64428034 	daddiu	v0,v0,-32716
[ 	]*10038: R_MIPS_GNU_REL_LO16	.text
0+01003c <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*1003c: R_MIPS_GNU_REL_HI16	.text
0+010040 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010044 <[^>]*> 644202ac 	daddiu	v0,v0,684
[ 	]*10044: R_MIPS_GNU_REL_LO16	.text
0+010048 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*10048: R_MIPS_GNU_REL_HI16	.text
0+01004c <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010050 <[^>]*> 644202b8 	daddiu	v0,v0,696
[ 	]*10050: R_MIPS_GNU_REL_LO16	.text
0+010054 <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*10054: R_MIPS_GNU_REL_HI16	e
0+010058 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+01005c <[^>]*> 64420050 	daddiu	v0,v0,80
[ 	]*1005c: R_MIPS_GNU_REL_LO16	e
0+010060 <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*10060: R_MIPS_GNU_REL_HI16	.text
0+010064 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010068 <[^>]*> 64420060 	daddiu	v0,v0,96
[ 	]*10068: R_MIPS_GNU_REL_LO16	.text
0+01006c <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*1006c: R_MIPS_GNU_REL_HI16	.text
0+010070 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010074 <[^>]*> 6442006c 	daddiu	v0,v0,108
[ 	]*10074: R_MIPS_GNU_REL_LO16	.text
0+010078 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*10078: R_MIPS_GNU_REL_HI16	.text
0+01007c <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010080 <[^>]*> 6442807c 	daddiu	v0,v0,-32644
[ 	]*10080: R_MIPS_GNU_REL_LO16	.text
0+010084 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*10084: R_MIPS_GNU_REL_HI16	.text
0+010088 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+01008c <[^>]*> 64428088 	daddiu	v0,v0,-32632
[ 	]*1008c: R_MIPS_GNU_REL_LO16	.text
0+010090 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*10090: R_MIPS_GNU_REL_HI16	.text
0+010094 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010098 <[^>]*> 64420300 	daddiu	v0,v0,768
[ 	]*10098: R_MIPS_GNU_REL_LO16	.text
0+01009c <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*1009c: R_MIPS_GNU_REL_HI16	.text
0+0100a0 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+0100a4 <[^>]*> 6442030c 	daddiu	v0,v0,780
[ 	]*100a4: R_MIPS_GNU_REL_LO16	.text
0+0100a8 <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*100a8: R_MIPS_GNU_REL_HI16	e
0+0100ac <[^>]*> 0044102d 	daddu	v0,v0,a0
0+0100b0 <[^>]*> 644200a4 	daddiu	v0,v0,164
[ 	]*100b0: R_MIPS_GNU_REL_LO16	e
0+0100b4 <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*100b4: R_MIPS_GNU_REL_HI16	.text
0+0100b8 <[^>]*> 644200b0 	daddiu	v0,v0,176
[ 	]*100b8: R_MIPS_GNU_REL_LO16	.text
0+0100bc <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*100bc: R_MIPS_GNU_REL_HI16	.text
0+0100c0 <[^>]*> 644200b8 	daddiu	v0,v0,184
[ 	]*100c0: R_MIPS_GNU_REL_LO16	.text
0+0100c4 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*100c4: R_MIPS_GNU_REL_HI16	.text
0+0100c8 <[^>]*> 644280c4 	daddiu	v0,v0,-32572
[ 	]*100c8: R_MIPS_GNU_REL_LO16	.text
0+0100cc <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*100cc: R_MIPS_GNU_REL_HI16	.text
0+0100d0 <[^>]*> 644280cc 	daddiu	v0,v0,-32564
[ 	]*100d0: R_MIPS_GNU_REL_LO16	.text
0+0100d4 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*100d4: R_MIPS_GNU_REL_HI16	.text
0+0100d8 <[^>]*> 64420340 	daddiu	v0,v0,832
[ 	]*100d8: R_MIPS_GNU_REL_LO16	.text
0+0100dc <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*100dc: R_MIPS_GNU_REL_HI16	.text
0+0100e0 <[^>]*> 64420348 	daddiu	v0,v0,840
[ 	]*100e0: R_MIPS_GNU_REL_LO16	.text
0+0100e4 <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*100e4: R_MIPS_GNU_REL_HI16	e
0+0100e8 <[^>]*> 644200dc 	daddiu	v0,v0,220
[ 	]*100e8: R_MIPS_GNU_REL_LO16	e
0+0100ec <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*100ec: R_MIPS_GNU_REL_HI16	.text
0+0100f0 <[^>]*> 644200e8 	daddiu	v0,v0,232
[ 	]*100f0: R_MIPS_GNU_REL_LO16	.text
0+0100f4 <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*100f4: R_MIPS_GNU_REL_HI16	.text
0+0100f8 <[^>]*> 644200f0 	daddiu	v0,v0,240
[ 	]*100f8: R_MIPS_GNU_REL_LO16	.text
0+0100fc <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*100fc: R_MIPS_GNU_REL_HI16	.text
0+010100 <[^>]*> 644280fc 	daddiu	v0,v0,-32516
[ 	]*10100: R_MIPS_GNU_REL_LO16	.text
0+010104 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*10104: R_MIPS_GNU_REL_HI16	.text
0+010108 <[^>]*> 64428104 	daddiu	v0,v0,-32508
[ 	]*10108: R_MIPS_GNU_REL_LO16	.text
0+01010c <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*1010c: R_MIPS_GNU_REL_HI16	.text
0+010110 <[^>]*> 64420378 	daddiu	v0,v0,888
[ 	]*10110: R_MIPS_GNU_REL_LO16	.text
0+010114 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*10114: R_MIPS_GNU_REL_HI16	.text
0+010118 <[^>]*> 64420380 	daddiu	v0,v0,896
[ 	]*10118: R_MIPS_GNU_REL_LO16	.text
0+01011c <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*1011c: R_MIPS_GNU_REL_HI16	e
0+010120 <[^>]*> 64420114 	daddiu	v0,v0,276
[ 	]*10120: R_MIPS_GNU_REL_LO16	e
0+010124 <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*10124: R_MIPS_GNU_REL_HI16	.text
0+010128 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+01012c <[^>]*> 8c420124 	lw	v0,292\(v0\)
[ 	]*1012c: R_MIPS_GNU_REL_LO16	.text
0+010130 <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*10130: R_MIPS_GNU_REL_HI16	.text
0+010134 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010138 <[^>]*> 8c420130 	lw	v0,304\(v0\)
[ 	]*10138: R_MIPS_GNU_REL_LO16	.text
0+01013c <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*1013c: R_MIPS_GNU_REL_HI16	.text
0+010140 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010144 <[^>]*> 8c428140 	lw	v0,-32448\(v0\)
[ 	]*10144: R_MIPS_GNU_REL_LO16	.text
0+010148 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*10148: R_MIPS_GNU_REL_HI16	.text
0+01014c <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010150 <[^>]*> 8c42814c 	lw	v0,-32436\(v0\)
[ 	]*10150: R_MIPS_GNU_REL_LO16	.text
0+010154 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*10154: R_MIPS_GNU_REL_HI16	.text
0+010158 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+01015c <[^>]*> 8c4203c4 	lw	v0,964\(v0\)
[ 	]*1015c: R_MIPS_GNU_REL_LO16	.text
0+010160 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*10160: R_MIPS_GNU_REL_HI16	.text
0+010164 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010168 <[^>]*> 8c4203d0 	lw	v0,976\(v0\)
[ 	]*10168: R_MIPS_GNU_REL_LO16	.text
0+01016c <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*1016c: R_MIPS_GNU_REL_HI16	e
0+010170 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010174 <[^>]*> 8c420168 	lw	v0,360\(v0\)
[ 	]*10174: R_MIPS_GNU_REL_LO16	e
0+010178 <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*10178: R_MIPS_GNU_REL_HI16	.text
0+01017c <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010180 <[^>]*> dc420178 	ld	v0,376\(v0\)
[ 	]*10180: R_MIPS_GNU_REL_LO16	.text
0+010184 <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*10184: R_MIPS_GNU_REL_HI16	.text
0+010188 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+01018c <[^>]*> dc420184 	ld	v0,388\(v0\)
[ 	]*1018c: R_MIPS_GNU_REL_LO16	.text
0+010190 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*10190: R_MIPS_GNU_REL_HI16	.text
0+010194 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+010198 <[^>]*> dc428194 	ld	v0,-32364\(v0\)
[ 	]*10198: R_MIPS_GNU_REL_LO16	.text
0+01019c <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*1019c: R_MIPS_GNU_REL_HI16	.text
0+0101a0 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+0101a4 <[^>]*> dc4281a0 	ld	v0,-32352\(v0\)
[ 	]*101a4: R_MIPS_GNU_REL_LO16	.text
0+0101a8 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*101a8: R_MIPS_GNU_REL_HI16	.text
0+0101ac <[^>]*> 0044102d 	daddu	v0,v0,a0
0+0101b0 <[^>]*> dc420418 	ld	v0,1048\(v0\)
[ 	]*101b0: R_MIPS_GNU_REL_LO16	.text
0+0101b4 <[^>]*> 3c020001 	lui	v0,0x1
[ 	]*101b4: R_MIPS_GNU_REL_HI16	.text
0+0101b8 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+0101bc <[^>]*> dc420424 	ld	v0,1060\(v0\)
[ 	]*101bc: R_MIPS_GNU_REL_LO16	.text
0+0101c0 <[^>]*> 3c020000 	lui	v0,0x0
[ 	]*101c0: R_MIPS_GNU_REL_HI16	e
0+0101c4 <[^>]*> 0044102d 	daddu	v0,v0,a0
0+0101c8 <[^>]*> dc4201bc 	ld	v0,444\(v0\)
[ 	]*101c8: R_MIPS_GNU_REL_LO16	e
0+0101cc <[^>]*> 3c010000 	lui	at,0x0
[ 	]*101cc: R_MIPS_GNU_REL_HI16	.text
0+0101d0 <[^>]*> 0024082d 	daddu	at,at,a0
0+0101d4 <[^>]*> ac2201cc 	sw	v0,460\(at\)
[ 	]*101d4: R_MIPS_GNU_REL_LO16	.text
0+0101d8 <[^>]*> 3c010000 	lui	at,0x0
[ 	]*101d8: R_MIPS_GNU_REL_HI16	.text
0+0101dc <[^>]*> 0024082d 	daddu	at,at,a0
0+0101e0 <[^>]*> ac2201d8 	sw	v0,472\(at\)
[ 	]*101e0: R_MIPS_GNU_REL_LO16	.text
0+0101e4 <[^>]*> 3c010001 	lui	at,0x1
[ 	]*101e4: R_MIPS_GNU_REL_HI16	.text
0+0101e8 <[^>]*> 0024082d 	daddu	at,at,a0
0+0101ec <[^>]*> ac2281e8 	sw	v0,-32280\(at\)
[ 	]*101ec: R_MIPS_GNU_REL_LO16	.text
0+0101f0 <[^>]*> 3c010001 	lui	at,0x1
[ 	]*101f0: R_MIPS_GNU_REL_HI16	.text
0+0101f4 <[^>]*> 0024082d 	daddu	at,at,a0
0+0101f8 <[^>]*> ac2281f4 	sw	v0,-32268\(at\)
[ 	]*101f8: R_MIPS_GNU_REL_LO16	.text
0+0101fc <[^>]*> 3c010001 	lui	at,0x1
[ 	]*101fc: R_MIPS_GNU_REL_HI16	.text
0+010200 <[^>]*> 0024082d 	daddu	at,at,a0
0+010204 <[^>]*> ac22046c 	sw	v0,1132\(at\)
[ 	]*10204: R_MIPS_GNU_REL_LO16	.text
0+010208 <[^>]*> 3c010001 	lui	at,0x1
[ 	]*10208: R_MIPS_GNU_REL_HI16	.text
0+01020c <[^>]*> 0024082d 	daddu	at,at,a0
0+010210 <[^>]*> ac220478 	sw	v0,1144\(at\)
[ 	]*10210: R_MIPS_GNU_REL_LO16	.text
0+010214 <[^>]*> 3c010000 	lui	at,0x0
[ 	]*10214: R_MIPS_GNU_REL_HI16	e
0+010218 <[^>]*> 0024082d 	daddu	at,at,a0
0+01021c <[^>]*> ac220210 	sw	v0,528\(at\)
[ 	]*1021c: R_MIPS_GNU_REL_LO16	e
0+010220 <[^>]*> 3c010000 	lui	at,0x0
[ 	]*10220: R_MIPS_GNU_REL_HI16	.text
0+010224 <[^>]*> 0024082d 	daddu	at,at,a0
0+010228 <[^>]*> fc220220 	sd	v0,544\(at\)
[ 	]*10228: R_MIPS_GNU_REL_LO16	.text
0+01022c <[^>]*> 3c010000 	lui	at,0x0
[ 	]*1022c: R_MIPS_GNU_REL_HI16	.text
0+010230 <[^>]*> 0024082d 	daddu	at,at,a0
0+010234 <[^>]*> fc22022c 	sd	v0,556\(at\)
[ 	]*10234: R_MIPS_GNU_REL_LO16	.text
0+010238 <[^>]*> 3c010001 	lui	at,0x1
[ 	]*10238: R_MIPS_GNU_REL_HI16	.text
0+01023c <[^>]*> 0024082d 	daddu	at,at,a0
0+010240 <[^>]*> fc22823c 	sd	v0,-32196\(at\)
[ 	]*10240: R_MIPS_GNU_REL_LO16	.text
0+010244 <[^>]*> 3c010001 	lui	at,0x1
[ 	]*10244: R_MIPS_GNU_REL_HI16	.text
0+010248 <[^>]*> 0024082d 	daddu	at,at,a0
0+01024c <[^>]*> fc228248 	sd	v0,-32184\(at\)
[ 	]*1024c: R_MIPS_GNU_REL_LO16	.text
0+010250 <[^>]*> 3c010001 	lui	at,0x1
[ 	]*10250: R_MIPS_GNU_REL_HI16	.text
0+010254 <[^>]*> 0024082d 	daddu	at,at,a0
0+010258 <[^>]*> fc2204c0 	sd	v0,1216\(at\)
[ 	]*10258: R_MIPS_GNU_REL_LO16	.text
0+01025c <[^>]*> 3c010001 	lui	at,0x1
[ 	]*1025c: R_MIPS_GNU_REL_HI16	.text
0+010260 <[^>]*> 0024082d 	daddu	at,at,a0
0+010264 <[^>]*> fc2204cc 	sd	v0,1228\(at\)
[ 	]*10264: R_MIPS_GNU_REL_LO16	.text
0+010268 <[^>]*> 3c010000 	lui	at,0x0
[ 	]*10268: R_MIPS_GNU_REL_HI16	e
0+01026c <[^>]*> 0024082d 	daddu	at,at,a0
0+010270 <[^>]*> fc220264 	sd	v0,612\(at\)
[ 	]*10270: R_MIPS_GNU_REL_LO16	e
	...
