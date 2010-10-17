# Example of M68hc11 instructions
	.sect .text
_start:
L0:	aba 
L1:	abx 
L2:	aby 
L3:	adca #103
L4:	adca *Z198
L5:	adca 105,X
L6:	adca symbol115
L7:	adca 81,X
L8:	adcb #255
L9:	adcb *Z74
L10:	adcb 236,X
L11:	adcb symbol41
L12:	adcb 205,X
L13:	adda #186
L14:	adda *Z171
L15:	adda 242,X
L16:	adda symbol251
L17:	adda 227,X
L18:	addb #70
L19:	addb *Z124
L20:	addb 194,X
L21:	addb symbol84
L22:	addb 248,X
L23:	addd #8987
L24:	addd *Z232
L25:	addd 231,X
L26:	addd symbol141
L27:	addd 118,X
L28:	anda #90
L29:	anda *Z46
L30:	anda 99,X
L31:	anda symbol51
L32:	anda 159,X
L33:	andb #201
L34:	andb *Z154
L35:	andb 102,X
L36:	andb symbol50
L37:	andb 13,X
L38:	asl 183,X
L39:	asl symbol49
L40:	asl 88,X
L41:	asla 
L42:	aslb 
L43:	asld 
L44:	asr 163,X
L45:	asr symbol90
L46:	asr 37,X
L47:	asra 
L48:	asrb 
L49:	bcc L93
L50:	bclr *Z5 #$17
L51:	bclr 88,X #$e9
L52:	bclr 94,X #$d4
L53:	bcs L171
L54:	beq L178
L55:	bge L205
L56:	bgt L198
L57:	bhi L155
L58:	bhs L180
L59:	bita #84
L60:	bita *Z17
L61:	bita 14,X
L62:	bita symbol130
L63:	bita 116,X
L64:	bitb #65
L65:	bitb *Z33
L66:	bitb 61,X
L67:	bitb symbol220
L68:	bitb 135,X
L69:	ble L112
L70:	blo L233
L71:	bls L62
L72:	blt L161
L73:	bmi L65
L74:	bne L225
L75:	bpl L252
L76:	bra L103
L77:	brclr *Z62 #$01 .+126
L78:	brclr 151,X #$ea .+92
L79:	brclr 107,X #$96 .+15
L80:	brn L56
L81:	brset *Z92 #$2a .+108
L82:	brset 176,X #$3b .+123
L83:	brset 50,X #$af .+60
L84:	bset *Z84 #$ec
L85:	bset 24,X #$db
L86:	bset 92,X #$02
L87:	bsr L26
L88:	bvc L254
L89:	bvs L67
L90:	cba 
L91:	clc 
L92:	cli 
L93:	clr 251,X
L94:	clr symbol250
L95:	clr 170,X
L96:	clra 
L97:	clrb 
L98:	clv 
L99:	cmpa #58
L100:	cmpa *Z251
L101:	cmpa 41,X
L102:	cmpa symbol209
L103:	cmpa 230,X
L104:	cmpb #5
L105:	cmpb *Z60
L106:	cmpb 124,X
L107:	cmpb symbol148
L108:	cmpb 117,X
L109:	cmpd #4056
L110:	cmpd *Z190
L111:	cmpd 97,X
L112:	cmpd symbol137
L113:	cmpd 249,X
L114:	cmpx #44892
L115:	cmpx *Z187
L116:	cmpx 168,X
L117:	cmpx symbol153
L118:	cmpx 15,X
L119:	cmpy #16533
L120:	cmpy *Z177
L121:	cmpy 235,X
L122:	cmpy symbol241
L123:	cmpy 179,X
L124:	com 5,X
L125:	com symbol239
L126:	com 247,X
L127:	coma 
L128:	comb 
L129:	cpd #48896
L130:	cpd *Z233
L131:	cpd 161,X
L132:	cpd symbol58
L133:	cpd 229,X
L134:	cpx #36810
L135:	cpx *Z11
L136:	cpx 203,X
L137:	cpx symbol208
L138:	cpx 72,X
L139:	cpy #583
L140:	cpy *Z100
L141:	cpy 189,X
L142:	cpy symbol31
L143:	cpy 35,X
L144:	daa 
L145:	dec 30,X
L146:	dec symbol168
L147:	dec 28,X
L148:	deca 
L149:	decb 
L150:	des 
L151:	dex 
L152:	dey 
L153:	eora #123
L154:	eora *Z100
L155:	eora 197,X
L156:	eora symbol20
L157:	eora 115,X
L158:	eorb #90
L159:	eorb *Z197
L160:	eorb 94,X
L161:	eorb symbol75
L162:	eorb 121,X
L163:	fdiv 
L164:	idiv 
L165:	inc 99,X
L166:	inc symbol59
L167:	inc 112,X
L168:	inca 
L169:	incb 
L170:	ins 
L171:	inx 
L172:	iny 
L173:	jmp 100,X
L174:	jmp symbol36
L175:	jmp 17,X
L176:	jsr *Z158
L177:	jsr 9,X
L178:	jsr symbol220
L179:	jsr 170,X
L180:	ldaa #212
L181:	ldaa *Z172
L182:	ldaa 242,X
L183:	ldaa symbol27
L184:	ldaa 16,X
L185:	ldab #175
L186:	ldab *Z59
L187:	ldab 51,X
L188:	ldab symbol205
L189:	ldab 227,X
L190:	ldd #50512
L191:	ldd *Z72
L192:	ldd 71,X
L193:	ldd symbol21
L194:	ldd 92,X
L195:	lds #20411
L196:	lds *Z111
L197:	lds 34,X
L198:	lds symbol25
L199:	lds 186,X
L200:	ldx #22427
L201:	ldx *Z125
L202:	ldx 245,X
L203:	ldx symbol11
L204:	ldx 225,X
L205:	ldy #44058
L206:	ldy *Z28
L207:	ldy 127,X
L208:	ldy symbol35
L209:	ldy 248,X
L210:	lsl 41,X
L211:	lsl symbol248
L212:	lsl 164,X
L213:	lsla 
L214:	lslb 
L215:	lsld 
L216:	lsr 27,X
L217:	lsr symbol19
L218:	lsr 181,X
L219:	lsra 
L220:	lsrb 
L221:	lsrd 
L222:	mul 
L223:	neg 202,X
L224:	neg symbol78
L225:	neg 232,X
L226:	nega 
L227:	negb 
L228:	nop 
L229:	oraa #152
L230:	oraa *Z50
L231:	oraa 56,X
L232:	oraa symbol224
L233:	oraa 121,X
L234:	orab #77
L235:	orab *Z61
L236:	orab 52,X
L237:	orab symbol188
L238:	orab 95,X
L239:	psha 
L240:	pshb 
L241:	pshx 
L242:	pshy 
L243:	pula 
L244:	pulb 
L245:	pulx 
L246:	puly 
L247:	rol 78,X
L248:	rol symbol119
L249:	rol 250,X
L250:	rola 
L251:	rolb 
L252:	ror 203,X
L253:	ror symbol108
L254:	ror 5,X
L255:	rora 
L256:	rorb 
L257:	rti 
L258:	rts 
L259:	sba 
L260:	sbca #172
L261:	sbca *Z134
L262:	sbca 33,X
L263:	sbca symbol43
L264:	sbca 170,X
L265:	sbcb #26
L266:	sbcb *Z85
L267:	sbcb 162,X
L268:	sbcb symbol190
L269:	sbcb 112,X
L270:	sec 
L271:	sei 
L272:	sev 
L273:	staa *Z181
L274:	staa 115,X
L275:	staa symbol59
L276:	staa 4,X
L277:	stab *Z92
L278:	stab 211,X
L279:	stab symbol54
L280:	stab 148,X
L281:	std *Z179
L282:	std 175,X
L283:	std symbol226
L284:	std 240,X
L285:	stop 
L286:	sts *Z228
L287:	sts 158,X
L288:	sts symbol79
L289:	sts 50,X
L290:	stx *Z21
L291:	stx 73,X
L292:	stx symbol253
L293:	stx 130,X
L294:	sty *Z78
L295:	sty 169,X
L296:	sty symbol8
L297:	sty 112,X
L298:	suba #212
L299:	suba *Z178
L300:	suba 138,X
L301:	suba symbol41
L302:	suba 84,X
L303:	subb #72
L304:	subb *Z154
L305:	subb 10,X
L306:	subb symbol188
L307:	subb 213,X
L308:	subd #61710
L309:	subd *Z24
L310:	subd 168,X
L311:	subd symbol68
L312:	subd 172,X
L313:	swi 
L314:	tab 
L315:	tap 
L316:	tba 
L317:	test 
L318:	tpa 
L319:	tst 91,X
L320:	tst symbol243
L321:	tst 142,X
L322:	tsta 
L323:	tstb 
L324:	tsx 
L325:	tsy 
L326:	txs 
L327:	tys 
L328:	wai 
L329:	xgdx 
L330:	xgdy 

