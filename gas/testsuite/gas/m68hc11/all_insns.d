#objdump: -d --prefix-addresses --reloc
#as: -m68hc11
#name: all_insns

# Test handling of basic instructions.

.*: +file format elf32\-m68hc11

Disassembly of section .text:
0+0+ <L0> aba
0+0001 <L1> abx
0+0002 <L2> aby
0+0004 <L3> adca	#103
0+0006 <L4> adca	\*0+0+ <L0>
			7: R_M68HC11_8	Z198
0+0008 <L5> adca	105,x
0+000a <L6> adca	0+0+ <L0>
			b: R_M68HC11_16	symbol115
0+000d <L7> adca	81,x
0+000f <L8> adcb	#255
0+0011 <L9> adcb	\*0+0+ <L0>
			12: R_M68HC11_8	Z74
0+0013 <L10> adcb	236,x
0+0015 <L11> adcb	0+0+ <L0>
			16: R_M68HC11_16	symbol41
0+0018 <L12> adcb	205,x
0+001a <L13> adda	#186
0+001c <L14> adda	\*0+0+ <L0>
			1d: R_M68HC11_8	Z171
0+001e <L15> adda	242,x
0+0020 <L16> adda	0+0+ <L0>
			21: R_M68HC11_16	symbol251
0+0023 <L17> adda	227,x
0+0025 <L18> addb	#70
0+0027 <L19> addb	\*0+0+ <L0>
			28: R_M68HC11_8	Z124
0+0029 <L20> addb	194,x
0+002b <L21> addb	0+0+ <L0>
			2c: R_M68HC11_16	symbol84
0+002e <L22> addb	248,x
0+0030 <L23> addd	#0+231b <L330\+0x2034>
0+0033 <L24> addd	\*0+0+ <L0>
			34: R_M68HC11_8	Z232
0+0035 <L25> addd	231,x
0+0037 <L26> addd	0+0+ <L0>
			38: R_M68HC11_16	symbol141
0+003a <L27> addd	118,x
0+003c <L28> anda	#90
0+003e <L29> anda	\*0+0+ <L0>
			3f: R_M68HC11_8	Z46
0+0040 <L30> anda	99,x
0+0042 <L31> anda	0+0+ <L0>
			43: R_M68HC11_16	symbol51
0+0045 <L32> anda	159,x
0+0047 <L33> andb	#201
0+0049 <L34> andb	\*0+0+ <L0>
			4a: R_M68HC11_8	Z154
0+004b <L35> andb	102,x
0+004d <L36> andb	0+0+ <L0>
			4e: R_M68HC11_16	symbol50
0+0050 <L37> andb	13,x
0+0052 <L38> asl	183,x
0+0054 <L39> asl	0+0+ <L0>
			55: R_M68HC11_16	symbol49
0+0057 <L40> asl	88,x
0+0059 <L41> asla
0+005a <L42> aslb
0+005b <L43> asld
0+005c <L44> asr	163,x
0+005e <L45> asr	0+0+ <L0>
			5f: R_M68HC11_16	symbol90
0+0061 <L46> asr	37,x
0+0063 <L47> asra
0+0064 <L48> asrb
0+0065 <L49> bcs	0+006a <L50>
			65: R_M68HC11_RL_JUMP	\*ABS\*
0+0067 <L49\+0x2> jmp	0+0+ <L0>
			68: R_M68HC11_16	L93
0+006a <L50> bclr	\*0+0+ <L0> #\$00
			6b: R_M68HC11_8	Z5
			6c: R_M68HC11_8	\$17
0+006d <L51> bclr	88,x #\$00
			6f: R_M68HC11_8	\$e9
0+0070 <L52> bclr	94,x #\$00
			72: R_M68HC11_8	\$d4
0+0073 <L53> bcc	0+0078 <L54>
			73: R_M68HC11_RL_JUMP	\*ABS\*
0+0075 <L53\+0x2> jmp	0+0+ <L0>
			76: R_M68HC11_16	L171
0+0078 <L54> bne	0+007d <L55>
			78: R_M68HC11_RL_JUMP	\*ABS\*
0+007a <L54\+0x2> jmp	0+0+ <L0>
			7b: R_M68HC11_16	L178
0+007d <L55> blt	0+0082 <L56>
			7d: R_M68HC11_RL_JUMP	\*ABS\*
0+007f <L55\+0x2> jmp	0+0+ <L0>
			80: R_M68HC11_16	L205
0+0082 <L56> ble	0+0087 <L57>
			82: R_M68HC11_RL_JUMP	\*ABS\*
0+0084 <L56\+0x2> jmp	0+0+ <L0>
			85: R_M68HC11_16	L198
0+0087 <L57> bls	0+008c <L58>
			87: R_M68HC11_RL_JUMP	\*ABS\*
0+0089 <L57\+0x2> jmp	0+0+ <L0>
			8a: R_M68HC11_16	L155
0+008c <L58> bcs	0+0091 <L59>
			8c: R_M68HC11_RL_JUMP	\*ABS\*
0+008e <L58\+0x2> jmp	0+0+ <L0>
			8f: R_M68HC11_16	L180
0+0091 <L59> bita	#84
0+0093 <L60> bita	\*0+0+ <L0>
			94: R_M68HC11_8	Z17
0+0095 <L61> bita	14,x
0+0097 <L62> bita	0+0+ <L0>
			98: R_M68HC11_16	symbol130
0+009a <L63> bita	116,x
0+009c <L64> bitb	#65
0+009e <L65> bitb	\*0+0+ <L0>
			9f: R_M68HC11_8	Z33
0+00a0 <L66> bitb	61,x
0+00a2 <L67> bitb	0+0+ <L0>
			a3: R_M68HC11_16	symbol220
0+00a5 <L68> bitb	135,x
0+00a7 <L69> ble	0+011d <L112>
			a7: R_M68HC11_RL_JUMP	\*ABS\*
0+00a9 <L70> bcc	0+00ae <L71>
			a9: R_M68HC11_RL_JUMP	\*ABS\*
0+00ab <L70\+0x2> jmp	0+0+ <L0>
			ac: R_M68HC11_16	L233
0+00ae <L71> bls	0+0097 <L62>
			ae: R_M68HC11_RL_JUMP	\*ABS\*
0+00b0 <L72> bge	0+00b5 <L73>
			b0: R_M68HC11_RL_JUMP	\*ABS\*
0+00b2 <L72\+0x2> jmp	0+0+ <L0>
			b3: R_M68HC11_16	L161
0+00b5 <L73> bmi	0+009e <L65>
			b5: R_M68HC11_RL_JUMP	\*ABS\*
0+00b7 <L74> beq	0+00bc <L75>
			b7: R_M68HC11_RL_JUMP	\*ABS\*
0+00b9 <L74\+0x2> jmp	0+0+ <L0>
			ba: R_M68HC11_16	L225
0+00bc <L75> bmi	0+00c1 <L76>
			bc: R_M68HC11_RL_JUMP	\*ABS\*
0+00be <L75\+0x2> jmp	0+0+ <L0>
			bf: R_M68HC11_16	L252
0+00c1 <L76> bra	0+0106 <L103>
			c1: R_M68HC11_RL_JUMP	\*ABS\*
0+00c3 <L77> brclr	\*0+0+ <L0> #\$00 0+0145 <L125\+0x2>
			c3: R_M68HC11_RL_JUMP	\*ABS\*
			c4: R_M68HC11_8	Z62
			c5: R_M68HC11_8	\$01
0+00c7 <L78> brclr	151,x #\$00 0+0127 <L115>
			c7: R_M68HC11_RL_JUMP	\*ABS\*
			c9: R_M68HC11_8	\$ea
0+00cb <L79> brclr	107,x #\$00 0+00de <L84\+0x1>
			cb: R_M68HC11_RL_JUMP	\*ABS\*
			cd: R_M68HC11_8	\$96
0+00cf <L80> brn	0+0082 <L56>
			cf: R_M68HC11_RL_JUMP	\*ABS\*
0+00d1 <L81> brset	\*0+0+ <L0> #\$00 0+0141 <L124>
			d1: R_M68HC11_RL_JUMP	\*ABS\*
			d2: R_M68HC11_8	Z92
			d3: R_M68HC11_8	\$2a
0+00d5 <L82> brset	176,x #\$00 0+0154 <L132>
			d5: R_M68HC11_RL_JUMP	\*ABS\*
			d7: R_M68HC11_8	\$3b
0+00d9 <L83> brset	50,x #\$00 0+0119 <L110\+0x2>
			d9: R_M68HC11_RL_JUMP	\*ABS\*
			db: R_M68HC11_8	\$af
0+00dd <L84> bset	\*0+0+ <L0> #\$00
			de: R_M68HC11_8	Z84
			df: R_M68HC11_8	\$ec
0+00e0 <L85> bset	24,x #\$00
			e2: R_M68HC11_8	\$db
0+00e3 <L86> bset	92,x #\$00
			e5: R_M68HC11_8	\$02
0+00e6 <L87> jsr	0+0+ <L0>
			e6: R_M68HC11_RL_JUMP	\*ABS\*
			e7: R_M68HC11_16	L26
0+00e9 <L88> bvs	0+00ee <L89>
			e9: R_M68HC11_RL_JUMP	\*ABS\*
0+00eb <L88\+0x2> jmp	0+0+ <L0>
			ec: R_M68HC11_16	L254
0+00ee <L89> bvs	0+00a2 <L67>
			ee: R_M68HC11_RL_JUMP	\*ABS\*
0+00f0 <L90> cba
0+00f1 <L91> clc
0+00f2 <L92> cli
0+00f3 <L93> clr	251,x
0+00f5 <L94> clr	0+0+ <L0>
			f6: R_M68HC11_16	symbol250
0+00f8 <L95> clr	170,x
0+00fa <L96> clra
0+00fb <L97> clrb
0+00fc <L98> clv
0+00fd <L99> cmpa	#58
0+00ff <L100> cmpa	\*0+0+ <L0>
			100: R_M68HC11_8	Z251
0+0101 <L101> cmpa	41,x
0+0103 <L102> cmpa	0+0+ <L0>
			104: R_M68HC11_16	symbol209
0+0106 <L103> cmpa	230,x
0+0108 <L104> cmpb	#5
0+010a <L105> cmpb	\*0+0+ <L0>
			10b: R_M68HC11_8	Z60
0+010c <L106> cmpb	124,x
0+010e <L107> cmpb	0+0+ <L0>
			10f: R_M68HC11_16	symbol148
0+0111 <L108> cmpb	117,x
0+0113 <L109> cpd	#0+0fd8 <L330\+0xcf1>
0+0117 <L110> cpd	\*0+0+ <L0>
			119: R_M68HC11_8	Z190
0+011a <L111> cpd	97,x
0+011d <L112> cpd	0+0+ <L0>
			11f: R_M68HC11_16	symbol137
0+0121 <L113> cpd	249,x
0+0124 <L114> cpx	#0+af5c <L330\+0xac75>
0+0127 <L115> cpx	\*0+0+ <L0>
			128: R_M68HC11_8	Z187
0+0129 <L116> cpx	168,x
0+012b <L117> cpx	0+0+ <L0>
			12c: R_M68HC11_16	symbol153
0+012e <L118> cpx	15,x
0+0130 <L119> cpy	#0+4095 <L330\+0x3dae>
0+0134 <L120> cpy	\*0+0+ <L0>
			136: R_M68HC11_8	Z177
0+0137 <L121> cpy	235,x
0+013a <L122> cpy	0+0+ <L0>
			13c: R_M68HC11_16	symbol241
0+013e <L123> cpy	179,x
0+0141 <L124> com	5,x
0+0143 <L125> com	0+0+ <L0>
			144: R_M68HC11_16	symbol239
0+0146 <L126> com	247,x
0+0148 <L127> coma
0+0149 <L128> comb
0+014a <L129> cpd	#0+bf00 <L330\+0xbc19>
0+014e <L130> cpd	\*0+0+ <L0>
			150: R_M68HC11_8	Z233
0+0151 <L131> cpd	161,x
0+0154 <L132> cpd	0+0+ <L0>
			156: R_M68HC11_16	symbol58
0+0158 <L133> cpd	229,x
0+015b <L134> cpx	#0+8fca <L330\+0x8ce3>
0+015e <L135> cpx	\*0+0+ <L0>
			15f: R_M68HC11_8	Z11
0+0160 <L136> cpx	203,x
0+0162 <L137> cpx	0+0+ <L0>
			163: R_M68HC11_16	symbol208
0+0165 <L138> cpx	72,x
0+0167 <L139> cpy	#0+0247 <L248>
0+016b <L140> cpy	\*0+0+ <L0>
			16d: R_M68HC11_8	Z100
0+016e <L141> cpy	189,x
0+0171 <L142> cpy	0+0+ <L0>
			173: R_M68HC11_16	symbol31
0+0175 <L143> cpy	35,x
0+0178 <L144> daa
0+0179 <L145> dec	30,x
0+017b <L146> dec	0+0+ <L0>
			17c: R_M68HC11_16	symbol168
0+017e <L147> dec	28,x
0+0180 <L148> deca
0+0181 <L149> decb
0+0182 <L150> des
0+0183 <L151> dex
0+0184 <L152> dey
0+0186 <L153> eora	#123
0+0188 <L154> eora	\*0+0+ <L0>
			189: R_M68HC11_8	Z100
0+018a <L155> eora	197,x
0+018c <L156> eora	0+0+ <L0>
			18d: R_M68HC11_16	symbol20
0+018f <L157> eora	115,x
0+0191 <L158> eorb	#90
0+0193 <L159> eorb	\*0+0+ <L0>
			194: R_M68HC11_8	Z197
0+0195 <L160> eorb	94,x
0+0197 <L161> eorb	0+0+ <L0>
			198: R_M68HC11_16	symbol75
0+019a <L162> eorb	121,x
0+019c <L163> fdiv
0+019d <L164> idiv
0+019e <L165> inc	99,x
0+01a0 <L166> inc	0+0+ <L0>
			1a1: R_M68HC11_16	symbol59
0+01a3 <L167> inc	112,x
0+01a5 <L168> inca
0+01a6 <L169> incb
0+01a7 <L170> ins
0+01a8 <L171> inx
0+01a9 <L172> iny
0+01ab <L173> jmp	100,x
0+01ad <L174> jmp	0+0+ <L0>
			1ad: R_M68HC11_RL_JUMP	\*ABS\*
			1ae: R_M68HC11_16	symbol36
0+01b0 <L175> jmp	17,x
0+01b2 <L176> jsr	\*0+0+ <L0>
			1b2: R_M68HC11_RL_JUMP	\*ABS\*
			1b3: R_M68HC11_8	Z158
0+01b4 <L177> jsr	9,x
0+01b6 <L178> jsr	0+0+ <L0>
			1b6: R_M68HC11_RL_JUMP	\*ABS\*
			1b7: R_M68HC11_16	symbol220
0+01b9 <L179> jsr	170,x
0+01bb <L180> ldaa	#212
0+01bd <L181> ldaa	\*0+0+ <L0>
			1be: R_M68HC11_8	Z172
0+01bf <L182> ldaa	242,x
0+01c1 <L183> ldaa	0+0+ <L0>
			1c2: R_M68HC11_16	symbol27
0+01c4 <L184> ldaa	16,x
0+01c6 <L185> ldab	#175
0+01c8 <L186> ldab	\*0+0+ <L0>
			1c9: R_M68HC11_8	Z59
0+01ca <L187> ldab	51,x
0+01cc <L188> ldab	0+0+ <L0>
			1cd: R_M68HC11_16	symbol205
0+01cf <L189> ldab	227,x
0+01d1 <L190> ldd	#0+c550 <L330\+0xc269>
0+01d4 <L191> ldd	\*0+0+ <L0>
			1d5: R_M68HC11_8	Z72
0+01d6 <L192> ldd	71,x
0+01d8 <L193> ldd	0+0+ <L0>
			1d9: R_M68HC11_16	symbol21
0+01db <L194> ldd	92,x
0+01dd <L195> lds	#0+4fbb <L330\+0x4cd4>
0+01e0 <L196> lds	\*0+0+ <L0>
			1e1: R_M68HC11_8	Z111
0+01e2 <L197> lds	34,x
0+01e4 <L198> lds	0+0+ <L0>
			1e5: R_M68HC11_16	symbol25
0+01e7 <L199> lds	186,x
0+01e9 <L200> ldx	#0+579b <L330\+0x54b4>
0+01ec <L201> ldx	\*0+0+ <L0>
			1ed: R_M68HC11_8	Z125
0+01ee <L202> ldx	245,x
0+01f0 <L203> ldx	0+0+ <L0>
			1f1: R_M68HC11_16	symbol11
0+01f3 <L204> ldx	225,x
0+01f5 <L205> ldy	#0+ac1a <L330\+0xa933>
0+01f9 <L206> ldy	\*0+0+ <L0>
			1fb: R_M68HC11_8	Z28
0+01fc <L207> ldy	127,x
0+01ff <L208> ldy	0+0+ <L0>
			201: R_M68HC11_16	symbol35
0+0203 <L209> ldy	248,x
0+0206 <L210> asl	41,x
0+0208 <L211> asl	0+0+ <L0>
			209: R_M68HC11_16	symbol248
0+020b <L212> asl	164,x
0+020d <L213> asla
0+020e <L214> aslb
0+020f <L215> asld
0+0210 <L216> lsr	27,x
0+0212 <L217> lsr	0+0+ <L0>
			213: R_M68HC11_16	symbol19
0+0215 <L218> lsr	181,x
0+0217 <L219> lsra
0+0218 <L220> lsrb
0+0219 <L221> lsrd
0+021a <L222> mul
0+021b <L223> neg	202,x
0+021d <L224> neg	0+0+ <L0>
			21e: R_M68HC11_16	symbol78
0+0220 <L225> neg	232,x
0+0222 <L226> nega
0+0223 <L227> negb
0+0224 <L228> nop
0+0225 <L229> oraa	#152
0+0227 <L230> oraa	\*0+0+ <L0>
			228: R_M68HC11_8	Z50
0+0229 <L231> oraa	56,x
0+022b <L232> oraa	0+0+ <L0>
			22c: R_M68HC11_16	symbol224
0+022e <L233> oraa	121,x
0+0230 <L234> orab	#77
0+0232 <L235> orab	\*0+0+ <L0>
			233: R_M68HC11_8	Z61
0+0234 <L236> orab	52,x
0+0236 <L237> orab	0+0+ <L0>
			237: R_M68HC11_16	symbol188
0+0239 <L238> orab	95,x
0+023b <L239> psha
0+023c <L240> pshb
0+023d <L241> pshx
0+023e <L242> pshy
0+0240 <L243> pula
0+0241 <L244> pulb
0+0242 <L245> pulx
0+0243 <L246> puly
0+0245 <L247> rol	78,x
0+0247 <L248> rol	0+0+ <L0>
			248: R_M68HC11_16	symbol119
0+024a <L249> rol	250,x
0+024c <L250> rola
0+024d <L251> rolb
0+024e <L252> ror	203,x
0+0250 <L253> ror	0+0+ <L0>
			251: R_M68HC11_16	symbol108
0+0253 <L254> ror	5,x
0+0255 <L255> rora
0+0256 <L256> rorb
0+0257 <L257> rti
0+0258 <L258> rts
0+0259 <L259> sba
0+025a <L260> sbca	#172
0+025c <L261> sbca	\*0+0+ <L0>
			25d: R_M68HC11_8	Z134
0+025e <L262> sbca	33,x
0+0260 <L263> sbca	0+0+ <L0>
			261: R_M68HC11_16	symbol43
0+0263 <L264> sbca	170,x
0+0265 <L265> sbcb	#26
0+0267 <L266> sbcb	\*0+0+ <L0>
			268: R_M68HC11_8	Z85
0+0269 <L267> sbcb	162,x
0+026b <L268> sbcb	0+0+ <L0>
			26c: R_M68HC11_16	symbol190
0+026e <L269> sbcb	112,x
0+0270 <L270> sec
0+0271 <L271> sei
0+0272 <L272> sev
0+0273 <L273> staa	\*0+0+ <L0>
			274: R_M68HC11_8	Z181
0+0275 <L274> staa	115,x
0+0277 <L275> staa	0+0+ <L0>
			278: R_M68HC11_16	symbol59
0+027a <L276> staa	4,x
0+027c <L277> stab	\*0+0+ <L0>
			27d: R_M68HC11_8	Z92
0+027e <L278> stab	211,x
0+0280 <L279> stab	0+0+ <L0>
			281: R_M68HC11_16	symbol54
0+0283 <L280> stab	148,x
0+0285 <L281> std	\*0+0+ <L0>
			286: R_M68HC11_8	Z179
0+0287 <L282> std	175,x
0+0289 <L283> std	0+0+ <L0>
			28a: R_M68HC11_16	symbol226
0+028c <L284> std	240,x
0+028e <L285> stop
0+028f <L286> sts	\*0+0+ <L0>
			290: R_M68HC11_8	Z228
0+0291 <L287> sts	158,x
0+0293 <L288> sts	0+0+ <L0>
			294: R_M68HC11_16	symbol79
0+0296 <L289> sts	50,x
0+0298 <L290> stx	\*0+0+ <L0>
			299: R_M68HC11_8	Z21
0+029a <L291> stx	73,x
0+029c <L292> stx	0+0+ <L0>
			29d: R_M68HC11_16	symbol253
0+029f <L293> stx	130,x
0+02a1 <L294> sty	\*0+0+ <L0>
			2a3: R_M68HC11_8	Z78
0+02a4 <L295> sty	169,x
0+02a7 <L296> sty	0+0+ <L0>
			2a9: R_M68HC11_16	symbol8
0+02ab <L297> sty	112,x
0+02ae <L298> suba	#212
0+02b0 <L299> suba	\*0+0+ <L0>
			2b1: R_M68HC11_8	Z178
0+02b2 <L300> suba	138,x
0+02b4 <L301> suba	0+0+ <L0>
			2b5: R_M68HC11_16	symbol41
0+02b7 <L302> suba	84,x
0+02b9 <L303> subb	#72
0+02bb <L304> subb	\*0+0+ <L0>
			2bc: R_M68HC11_8	Z154
0+02bd <L305> subb	10,x
0+02bf <L306> subb	0+0+ <L0>
			2c0: R_M68HC11_16	symbol188
0+02c2 <L307> subb	213,x
0+02c4 <L308> subd	#0+f10e <L330\+0xee27>
0+02c7 <L309> subd	\*0+0+ <L0>
			2c8: R_M68HC11_8	Z24
0+02c9 <L310> subd	168,x
0+02cb <L311> subd	0+0+ <L0>
			2cc: R_M68HC11_16	symbol68
0+02ce <L312> subd	172,x
0+02d0 <L313> swi
0+02d1 <L314> tab
0+02d2 <L315> tap
0+02d3 <L316> tba
	...
0+02d5 <L318> tpa
0+02d6 <L319> tst	91,x
0+02d8 <L320> tst	0+0+ <L0>
			2d9: R_M68HC11_16	symbol243
0+02db <L321> tst	142,x
0+02dd <L322> tsta
0+02de <L323> tstb
0+02df <L324> tsx
0+02e0 <L325> tsy
0+02e2 <L326> txs
0+02e3 <L327> tys
0+02e5 <L328> wai
0+02e6 <L329> xgdx
0+02e7 <L330> xgdy
