.text
	.type _start,@function
_start:
	ld1 r4 = [r5]
	ld1 r4 = [r5], r6
	ld1 r4 = [r5], -256
	ld1.nt1 r4 = [r5]
	ld1.nt1 r4 = [r5], r6
	ld1.nt1 r4 = [r5], -243
	ld1.nta r4 = [r5]
	ld1.nta r4 = [r5], r6
	ld1.nta r4 = [r5], -230

	ld1.s r4 = [r5]
	ld1.s r4 = [r5], r6
	ld1.s r4 = [r5], -217
	ld1.s.nt1 r4 = [r5]
	ld1.s.nt1 r4 = [r5], r6
	ld1.s.nt1 r4 = [r5], -204
	ld1.s.nta r4 = [r5]
	ld1.s.nta r4 = [r5], r6
	ld1.s.nta r4 = [r5], -191

	ld1.a r4 = [r5]
	ld1.a r4 = [r5], r6
	ld1.a r4 = [r5], -178
	ld1.a.nt1 r4 = [r5]
	ld1.a.nt1 r4 = [r5], r6
	ld1.a.nt1 r4 = [r5], -165
	ld1.a.nta r4 = [r5]
	ld1.a.nta r4 = [r5], r6
	ld1.a.nta r4 = [r5], -152

	ld1.sa r4 = [r5]
	ld1.sa r4 = [r5], r6
	ld1.sa r4 = [r5], -139
	ld1.sa.nt1 r4 = [r5]
	ld1.sa.nt1 r4 = [r5], r6
	ld1.sa.nt1 r4 = [r5], -126
	ld1.sa.nta r4 = [r5]
	ld1.sa.nta r4 = [r5], r6
	ld1.sa.nta r4 = [r5], -113

	ld1.c.clr r4 = [r5]
	ld1.c.clr r4 = [r5], r6
	ld1.c.clr r4 = [r5], -100
	ld1.c.clr.nt1 r4 = [r5]
	ld1.c.clr.nt1 r4 = [r5], r6
	ld1.c.clr.nt1 r4 = [r5], -87
	ld1.c.clr.nta r4 = [r5]
	ld1.c.clr.nta r4 = [r5], r6
	ld1.c.clr.nta r4 = [r5], -74

	ld1.c.nc r4 = [r5]
	ld1.c.nc r4 = [r5], r6
	ld1.c.nc r4 = [r5], -61
	ld1.c.nc.nt1 r4 = [r5]
	ld1.c.nc.nt1 r4 = [r5], r6
	ld1.c.nc.nt1 r4 = [r5], -48
	ld1.c.nc.nta r4 = [r5]
	ld1.c.nc.nta r4 = [r5], r6
	ld1.c.nc.nta r4 = [r5], -35

	ld1.bias r4 = [r5]
	ld1.bias r4 = [r5], r6
	ld1.bias r4 = [r5], -22
	ld1.bias.nt1 r4 = [r5]
	ld1.bias.nt1 r4 = [r5], r6
	ld1.bias.nt1 r4 = [r5], -9
	ld1.bias.nta r4 = [r5]
	ld1.bias.nta r4 = [r5], r6
	ld1.bias.nta r4 = [r5], 4

	ld1.acq r4 = [r5]
	ld1.acq r4 = [r5], r6
	ld1.acq r4 = [r5], 17
	ld1.acq.nt1 r4 = [r5]
	ld1.acq.nt1 r4 = [r5], r6
	ld1.acq.nt1 r4 = [r5], 30
	ld1.acq.nta r4 = [r5]
	ld1.acq.nta r4 = [r5], r6
	ld1.acq.nta r4 = [r5], 43

	ld1.c.clr.acq r4 = [r5]
	ld1.c.clr.acq r4 = [r5], r6
	ld1.c.clr.acq r4 = [r5], 56
	ld1.c.clr.acq.nt1 r4 = [r5]
	ld1.c.clr.acq.nt1 r4 = [r5], r6
	ld1.c.clr.acq.nt1 r4 = [r5], 69
	ld1.c.clr.acq.nta r4 = [r5]
	ld1.c.clr.acq.nta r4 = [r5], r6
	ld1.c.clr.acq.nta r4 = [r5], 82

	ld2 r4 = [r5]
	ld2 r4 = [r5], r6
	ld2 r4 = [r5], 95
	ld2.nt1 r4 = [r5]
	ld2.nt1 r4 = [r5], r6
	ld2.nt1 r4 = [r5], 108
	ld2.nta r4 = [r5]
	ld2.nta r4 = [r5], r6
	ld2.nta r4 = [r5], 121

	ld2.s r4 = [r5]
	ld2.s r4 = [r5], r6
	ld2.s r4 = [r5], 134
	ld2.s.nt1 r4 = [r5]
	ld2.s.nt1 r4 = [r5], r6
	ld2.s.nt1 r4 = [r5], 147
	ld2.s.nta r4 = [r5]
	ld2.s.nta r4 = [r5], r6
	ld2.s.nta r4 = [r5], 160

	ld2.a r4 = [r5]
	ld2.a r4 = [r5], r6
	ld2.a r4 = [r5], 173
	ld2.a.nt1 r4 = [r5]
	ld2.a.nt1 r4 = [r5], r6
	ld2.a.nt1 r4 = [r5], 186
	ld2.a.nta r4 = [r5]
	ld2.a.nta r4 = [r5], r6
	ld2.a.nta r4 = [r5], 199

	ld2.sa r4 = [r5]
	ld2.sa r4 = [r5], r6
	ld2.sa r4 = [r5], 212
	ld2.sa.nt1 r4 = [r5]
	ld2.sa.nt1 r4 = [r5], r6
	ld2.sa.nt1 r4 = [r5], 225
	ld2.sa.nta r4 = [r5]
	ld2.sa.nta r4 = [r5], r6
	ld2.sa.nta r4 = [r5], 238

	ld2.c.clr r4 = [r5]
	ld2.c.clr r4 = [r5], r6
	ld2.c.clr r4 = [r5], 251
	ld2.c.clr.nt1 r4 = [r5]
	ld2.c.clr.nt1 r4 = [r5], r6
	ld2.c.clr.nt1 r4 = [r5], -248
	ld2.c.clr.nta r4 = [r5]
	ld2.c.clr.nta r4 = [r5], r6
	ld2.c.clr.nta r4 = [r5], -235

	ld2.c.nc r4 = [r5]
	ld2.c.nc r4 = [r5], r6
	ld2.c.nc r4 = [r5], -222
	ld2.c.nc.nt1 r4 = [r5]
	ld2.c.nc.nt1 r4 = [r5], r6
	ld2.c.nc.nt1 r4 = [r5], -209
	ld2.c.nc.nta r4 = [r5]
	ld2.c.nc.nta r4 = [r5], r6
	ld2.c.nc.nta r4 = [r5], -196

	ld2.bias r4 = [r5]
	ld2.bias r4 = [r5], r6
	ld2.bias r4 = [r5], -183
	ld2.bias.nt1 r4 = [r5]
	ld2.bias.nt1 r4 = [r5], r6
	ld2.bias.nt1 r4 = [r5], -170
	ld2.bias.nta r4 = [r5]
	ld2.bias.nta r4 = [r5], r6
	ld2.bias.nta r4 = [r5], -157

	ld2.acq r4 = [r5]
	ld2.acq r4 = [r5], r6
	ld2.acq r4 = [r5], -144
	ld2.acq.nt1 r4 = [r5]
	ld2.acq.nt1 r4 = [r5], r6
	ld2.acq.nt1 r4 = [r5], -131
	ld2.acq.nta r4 = [r5]
	ld2.acq.nta r4 = [r5], r6
	ld2.acq.nta r4 = [r5], -118

	ld2.c.clr.acq r4 = [r5]
	ld2.c.clr.acq r4 = [r5], r6
	ld2.c.clr.acq r4 = [r5], -105
	ld2.c.clr.acq.nt1 r4 = [r5]
	ld2.c.clr.acq.nt1 r4 = [r5], r6
	ld2.c.clr.acq.nt1 r4 = [r5], -92
	ld2.c.clr.acq.nta r4 = [r5]
	ld2.c.clr.acq.nta r4 = [r5], r6
	ld2.c.clr.acq.nta r4 = [r5], -79

	ld4 r4 = [r5]
	ld4 r4 = [r5], r6
	ld4 r4 = [r5], -66
	ld4.nt1 r4 = [r5]
	ld4.nt1 r4 = [r5], r6
	ld4.nt1 r4 = [r5], -53
	ld4.nta r4 = [r5]
	ld4.nta r4 = [r5], r6
	ld4.nta r4 = [r5], -40

	ld4.s r4 = [r5]
	ld4.s r4 = [r5], r6
	ld4.s r4 = [r5], -27
	ld4.s.nt1 r4 = [r5]
	ld4.s.nt1 r4 = [r5], r6
	ld4.s.nt1 r4 = [r5], -14
	ld4.s.nta r4 = [r5]
	ld4.s.nta r4 = [r5], r6
	ld4.s.nta r4 = [r5], -1

	ld4.a r4 = [r5]
	ld4.a r4 = [r5], r6
	ld4.a r4 = [r5], 12
	ld4.a.nt1 r4 = [r5]
	ld4.a.nt1 r4 = [r5], r6
	ld4.a.nt1 r4 = [r5], 25
	ld4.a.nta r4 = [r5]
	ld4.a.nta r4 = [r5], r6
	ld4.a.nta r4 = [r5], 38

	ld4.sa r4 = [r5]
	ld4.sa r4 = [r5], r6
	ld4.sa r4 = [r5], 51
	ld4.sa.nt1 r4 = [r5]
	ld4.sa.nt1 r4 = [r5], r6
	ld4.sa.nt1 r4 = [r5], 64
	ld4.sa.nta r4 = [r5]
	ld4.sa.nta r4 = [r5], r6
	ld4.sa.nta r4 = [r5], 77

	ld4.c.clr r4 = [r5]
	ld4.c.clr r4 = [r5], r6
	ld4.c.clr r4 = [r5], 90
	ld4.c.clr.nt1 r4 = [r5]
	ld4.c.clr.nt1 r4 = [r5], r6
	ld4.c.clr.nt1 r4 = [r5], 103
	ld4.c.clr.nta r4 = [r5]
	ld4.c.clr.nta r4 = [r5], r6
	ld4.c.clr.nta r4 = [r5], 116

	ld4.c.nc r4 = [r5]
	ld4.c.nc r4 = [r5], r6
	ld4.c.nc r4 = [r5], 129
	ld4.c.nc.nt1 r4 = [r5]
	ld4.c.nc.nt1 r4 = [r5], r6
	ld4.c.nc.nt1 r4 = [r5], 142
	ld4.c.nc.nta r4 = [r5]
	ld4.c.nc.nta r4 = [r5], r6
	ld4.c.nc.nta r4 = [r5], 155

	ld4.bias r4 = [r5]
	ld4.bias r4 = [r5], r6
	ld4.bias r4 = [r5], 168
	ld4.bias.nt1 r4 = [r5]
	ld4.bias.nt1 r4 = [r5], r6
	ld4.bias.nt1 r4 = [r5], 181
	ld4.bias.nta r4 = [r5]
	ld4.bias.nta r4 = [r5], r6
	ld4.bias.nta r4 = [r5], 194

	ld4.acq r4 = [r5]
	ld4.acq r4 = [r5], r6
	ld4.acq r4 = [r5], 207
	ld4.acq.nt1 r4 = [r5]
	ld4.acq.nt1 r4 = [r5], r6
	ld4.acq.nt1 r4 = [r5], 220
	ld4.acq.nta r4 = [r5]
	ld4.acq.nta r4 = [r5], r6
	ld4.acq.nta r4 = [r5], 233

	ld4.c.clr.acq r4 = [r5]
	ld4.c.clr.acq r4 = [r5], r6
	ld4.c.clr.acq r4 = [r5], 246
	ld4.c.clr.acq.nt1 r4 = [r5]
	ld4.c.clr.acq.nt1 r4 = [r5], r6
	ld4.c.clr.acq.nt1 r4 = [r5], -253
	ld4.c.clr.acq.nta r4 = [r5]
	ld4.c.clr.acq.nta r4 = [r5], r6
	ld4.c.clr.acq.nta r4 = [r5], -240

	ld8 r4 = [r5]
	ld8 r4 = [r5], r6
	ld8 r4 = [r5], -227
	ld8.nt1 r4 = [r5]
	ld8.nt1 r4 = [r5], r6
	ld8.nt1 r4 = [r5], -214
	ld8.nta r4 = [r5]
	ld8.nta r4 = [r5], r6
	ld8.nta r4 = [r5], -201

	ld8.s r4 = [r5]
	ld8.s r4 = [r5], r6
	ld8.s r4 = [r5], -188
	ld8.s.nt1 r4 = [r5]
	ld8.s.nt1 r4 = [r5], r6
	ld8.s.nt1 r4 = [r5], -175
	ld8.s.nta r4 = [r5]
	ld8.s.nta r4 = [r5], r6
	ld8.s.nta r4 = [r5], -162

	ld8.a r4 = [r5]
	ld8.a r4 = [r5], r6
	ld8.a r4 = [r5], -149
	ld8.a.nt1 r4 = [r5]
	ld8.a.nt1 r4 = [r5], r6
	ld8.a.nt1 r4 = [r5], -136
	ld8.a.nta r4 = [r5]
	ld8.a.nta r4 = [r5], r6
	ld8.a.nta r4 = [r5], -123

	ld8.sa r4 = [r5]
	ld8.sa r4 = [r5], r6
	ld8.sa r4 = [r5], -110
	ld8.sa.nt1 r4 = [r5]
	ld8.sa.nt1 r4 = [r5], r6
	ld8.sa.nt1 r4 = [r5], -97
	ld8.sa.nta r4 = [r5]
	ld8.sa.nta r4 = [r5], r6
	ld8.sa.nta r4 = [r5], -84

	ld8.c.clr r4 = [r5]
	ld8.c.clr r4 = [r5], r6
	ld8.c.clr r4 = [r5], -71
	ld8.c.clr.nt1 r4 = [r5]
	ld8.c.clr.nt1 r4 = [r5], r6
	ld8.c.clr.nt1 r4 = [r5], -58
	ld8.c.clr.nta r4 = [r5]
	ld8.c.clr.nta r4 = [r5], r6
	ld8.c.clr.nta r4 = [r5], -45

	ld8.c.nc r4 = [r5]
	ld8.c.nc r4 = [r5], r6
	ld8.c.nc r4 = [r5], -32
	ld8.c.nc.nt1 r4 = [r5]
	ld8.c.nc.nt1 r4 = [r5], r6
	ld8.c.nc.nt1 r4 = [r5], -19
	ld8.c.nc.nta r4 = [r5]
	ld8.c.nc.nta r4 = [r5], r6
	ld8.c.nc.nta r4 = [r5], -6

	ld8.bias r4 = [r5]
	ld8.bias r4 = [r5], r6
	ld8.bias r4 = [r5], 7
	ld8.bias.nt1 r4 = [r5]
	ld8.bias.nt1 r4 = [r5], r6
	ld8.bias.nt1 r4 = [r5], 20
	ld8.bias.nta r4 = [r5]
	ld8.bias.nta r4 = [r5], r6
	ld8.bias.nta r4 = [r5], 33

	ld8.acq r4 = [r5]
	ld8.acq r4 = [r5], r6
	ld8.acq r4 = [r5], 46
	ld8.acq.nt1 r4 = [r5]
	ld8.acq.nt1 r4 = [r5], r6
	ld8.acq.nt1 r4 = [r5], 59
	ld8.acq.nta r4 = [r5]
	ld8.acq.nta r4 = [r5], r6
	ld8.acq.nta r4 = [r5], 72

	ld8.c.clr.acq r4 = [r5]
	ld8.c.clr.acq r4 = [r5], r6
	ld8.c.clr.acq r4 = [r5], 85
	ld8.c.clr.acq.nt1 r4 = [r5]
	ld8.c.clr.acq.nt1 r4 = [r5], r6
	ld8.c.clr.acq.nt1 r4 = [r5], 98
	ld8.c.clr.acq.nta r4 = [r5]
	ld8.c.clr.acq.nta r4 = [r5], r6
	ld8.c.clr.acq.nta r4 = [r5], 111

	ld8.fill r4 = [r5]
	ld8.fill r4 = [r5], r6
	ld8.fill r4 = [r5], 124
	ld8.fill.nt1 r4 = [r5]
	ld8.fill.nt1 r4 = [r5], r6
	ld8.fill.nt1 r4 = [r5], 137
	ld8.fill.nta r4 = [r5]
	ld8.fill.nta r4 = [r5], r6
	ld8.fill.nta r4 = [r5], 150

	st1 [r4] = r5
	st1 [r4] = r5, 163
	st1.nta [r4] = r5
	st1.nta [r4] = r5, 176

	st2 [r4] = r5
	st2 [r4] = r5, 189
	st2.nta [r4] = r5
	st2.nta [r4] = r5, 202

	st4 [r4] = r5
	st4 [r4] = r5, 215
	st4.nta [r4] = r5
	st4.nta [r4] = r5, 228

	st8 [r4] = r5
	st8 [r4] = r5, 241
	st8.nta [r4] = r5
	st8.nta [r4] = r5, 254

	st1.rel [r4] = r5
	st1.rel [r4] = r5, -245
	st1.rel.nta [r4] = r5
	st1.rel.nta [r4] = r5, -232

	st2.rel [r4] = r5
	st2.rel [r4] = r5, -219
	st2.rel.nta [r4] = r5
	st2.rel.nta [r4] = r5, -206

	st4.rel [r4] = r5
	st4.rel [r4] = r5, -193
	st4.rel.nta [r4] = r5
	st4.rel.nta [r4] = r5, -180

	st8.rel [r4] = r5
	st8.rel [r4] = r5, -167
	st8.rel.nta [r4] = r5
	st8.rel.nta [r4] = r5, -154

	st8.spill [r4] = r5
	st8.spill [r4] = r5, -141
	st8.spill.nta [r4] = r5
	st8.spill.nta [r4] = r5, -128

	ldfs f4 = [r5]
	ldfs f4 = [r5], r6
	ldfs f4 = [r5], -115
	ldfs.nt1 f4 = [r5]
	ldfs.nt1 f4 = [r5], r6
	ldfs.nt1 f4 = [r5], -102
	ldfs.nta f4 = [r5]
	ldfs.nta f4 = [r5], r6
	ldfs.nta f4 = [r5], -89

	ldfs.s f4 = [r5]
	ldfs.s f4 = [r5], r6
	ldfs.s f4 = [r5], -76
	ldfs.s.nt1 f4 = [r5]
	ldfs.s.nt1 f4 = [r5], r6
	ldfs.s.nt1 f4 = [r5], -63
	ldfs.s.nta f4 = [r5]
	ldfs.s.nta f4 = [r5], r6
	ldfs.s.nta f4 = [r5], -50

	ldfs.a f4 = [r5]
	ldfs.a f4 = [r5], r6
	ldfs.a f4 = [r5], -37
	ldfs.a.nt1 f4 = [r5]
	ldfs.a.nt1 f4 = [r5], r6
	ldfs.a.nt1 f4 = [r5], -24
	ldfs.a.nta f4 = [r5]
	ldfs.a.nta f4 = [r5], r6
	ldfs.a.nta f4 = [r5], -11

	ldfs.sa f4 = [r5]
	ldfs.sa f4 = [r5], r6
	ldfs.sa f4 = [r5], 2
	ldfs.sa.nt1 f4 = [r5]
	ldfs.sa.nt1 f4 = [r5], r6
	ldfs.sa.nt1 f4 = [r5], 15
	ldfs.sa.nta f4 = [r5]
	ldfs.sa.nta f4 = [r5], r6
	ldfs.sa.nta f4 = [r5], 28

	ldfs.c.clr f4 = [r5]
	ldfs.c.clr f4 = [r5], r6
	ldfs.c.clr f4 = [r5], 41
	ldfs.c.clr.nt1 f4 = [r5]
	ldfs.c.clr.nt1 f4 = [r5], r6
	ldfs.c.clr.nt1 f4 = [r5], 54
	ldfs.c.clr.nta f4 = [r5]
	ldfs.c.clr.nta f4 = [r5], r6
	ldfs.c.clr.nta f4 = [r5], 67

	ldfs.c.nc f4 = [r5]
	ldfs.c.nc f4 = [r5], r6
	ldfs.c.nc f4 = [r5], 80
	ldfs.c.nc.nt1 f4 = [r5]
	ldfs.c.nc.nt1 f4 = [r5], r6
	ldfs.c.nc.nt1 f4 = [r5], 93
	ldfs.c.nc.nta f4 = [r5]
	ldfs.c.nc.nta f4 = [r5], r6
	ldfs.c.nc.nta f4 = [r5], 106

	ldfd f4 = [r5]
	ldfd f4 = [r5], r6
	ldfd f4 = [r5], 119
	ldfd.nt1 f4 = [r5]
	ldfd.nt1 f4 = [r5], r6
	ldfd.nt1 f4 = [r5], 132
	ldfd.nta f4 = [r5]
	ldfd.nta f4 = [r5], r6
	ldfd.nta f4 = [r5], 145

	ldfd.s f4 = [r5]
	ldfd.s f4 = [r5], r6
	ldfd.s f4 = [r5], 158
	ldfd.s.nt1 f4 = [r5]
	ldfd.s.nt1 f4 = [r5], r6
	ldfd.s.nt1 f4 = [r5], 171
	ldfd.s.nta f4 = [r5]
	ldfd.s.nta f4 = [r5], r6
	ldfd.s.nta f4 = [r5], 184

	ldfd.a f4 = [r5]
	ldfd.a f4 = [r5], r6
	ldfd.a f4 = [r5], 197
	ldfd.a.nt1 f4 = [r5]
	ldfd.a.nt1 f4 = [r5], r6
	ldfd.a.nt1 f4 = [r5], 210
	ldfd.a.nta f4 = [r5]
	ldfd.a.nta f4 = [r5], r6
	ldfd.a.nta f4 = [r5], 223

	ldfd.sa f4 = [r5]
	ldfd.sa f4 = [r5], r6
	ldfd.sa f4 = [r5], 236
	ldfd.sa.nt1 f4 = [r5]
	ldfd.sa.nt1 f4 = [r5], r6
	ldfd.sa.nt1 f4 = [r5], 249
	ldfd.sa.nta f4 = [r5]
	ldfd.sa.nta f4 = [r5], r6
	ldfd.sa.nta f4 = [r5], -250

	ldfd.c.clr f4 = [r5]
	ldfd.c.clr f4 = [r5], r6
	ldfd.c.clr f4 = [r5], -237
	ldfd.c.clr.nt1 f4 = [r5]
	ldfd.c.clr.nt1 f4 = [r5], r6
	ldfd.c.clr.nt1 f4 = [r5], -224
	ldfd.c.clr.nta f4 = [r5]
	ldfd.c.clr.nta f4 = [r5], r6
	ldfd.c.clr.nta f4 = [r5], -211

	ldfd.c.nc f4 = [r5]
	ldfd.c.nc f4 = [r5], r6
	ldfd.c.nc f4 = [r5], -198
	ldfd.c.nc.nt1 f4 = [r5]
	ldfd.c.nc.nt1 f4 = [r5], r6
	ldfd.c.nc.nt1 f4 = [r5], -185
	ldfd.c.nc.nta f4 = [r5]
	ldfd.c.nc.nta f4 = [r5], r6
	ldfd.c.nc.nta f4 = [r5], -172

	ldf8 f4 = [r5]
	ldf8 f4 = [r5], r6
	ldf8 f4 = [r5], -159
	ldf8.nt1 f4 = [r5]
	ldf8.nt1 f4 = [r5], r6
	ldf8.nt1 f4 = [r5], -146
	ldf8.nta f4 = [r5]
	ldf8.nta f4 = [r5], r6
	ldf8.nta f4 = [r5], -133

	ldf8.s f4 = [r5]
	ldf8.s f4 = [r5], r6
	ldf8.s f4 = [r5], -120
	ldf8.s.nt1 f4 = [r5]
	ldf8.s.nt1 f4 = [r5], r6
	ldf8.s.nt1 f4 = [r5], -107
	ldf8.s.nta f4 = [r5]
	ldf8.s.nta f4 = [r5], r6
	ldf8.s.nta f4 = [r5], -94

	ldf8.a f4 = [r5]
	ldf8.a f4 = [r5], r6
	ldf8.a f4 = [r5], -81
	ldf8.a.nt1 f4 = [r5]
	ldf8.a.nt1 f4 = [r5], r6
	ldf8.a.nt1 f4 = [r5], -68
	ldf8.a.nta f4 = [r5]
	ldf8.a.nta f4 = [r5], r6
	ldf8.a.nta f4 = [r5], -55

	ldf8.sa f4 = [r5]
	ldf8.sa f4 = [r5], r6
	ldf8.sa f4 = [r5], -42
	ldf8.sa.nt1 f4 = [r5]
	ldf8.sa.nt1 f4 = [r5], r6
	ldf8.sa.nt1 f4 = [r5], -29
	ldf8.sa.nta f4 = [r5]
	ldf8.sa.nta f4 = [r5], r6
	ldf8.sa.nta f4 = [r5], -16

	ldf8.c.clr f4 = [r5]
	ldf8.c.clr f4 = [r5], r6
	ldf8.c.clr f4 = [r5], -3
	ldf8.c.clr.nt1 f4 = [r5]
	ldf8.c.clr.nt1 f4 = [r5], r6
	ldf8.c.clr.nt1 f4 = [r5], 10
	ldf8.c.clr.nta f4 = [r5]
	ldf8.c.clr.nta f4 = [r5], r6
	ldf8.c.clr.nta f4 = [r5], 23

	ldf8.c.nc f4 = [r5]
	ldf8.c.nc f4 = [r5], r6
	ldf8.c.nc f4 = [r5], 36
	ldf8.c.nc.nt1 f4 = [r5]
	ldf8.c.nc.nt1 f4 = [r5], r6
	ldf8.c.nc.nt1 f4 = [r5], 49
	ldf8.c.nc.nta f4 = [r5]
	ldf8.c.nc.nta f4 = [r5], r6
	ldf8.c.nc.nta f4 = [r5], 62

	ldfe f4 = [r5]
	ldfe f4 = [r5], r6
	ldfe f4 = [r5], 75
	ldfe.nt1 f4 = [r5]
	ldfe.nt1 f4 = [r5], r6
	ldfe.nt1 f4 = [r5], 88
	ldfe.nta f4 = [r5]
	ldfe.nta f4 = [r5], r6
	ldfe.nta f4 = [r5], 101

	ldfe.s f4 = [r5]
	ldfe.s f4 = [r5], r6
	ldfe.s f4 = [r5], 114
	ldfe.s.nt1 f4 = [r5]
	ldfe.s.nt1 f4 = [r5], r6
	ldfe.s.nt1 f4 = [r5], 127
	ldfe.s.nta f4 = [r5]
	ldfe.s.nta f4 = [r5], r6
	ldfe.s.nta f4 = [r5], 140

	ldfe.a f4 = [r5]
	ldfe.a f4 = [r5], r6
	ldfe.a f4 = [r5], 153
	ldfe.a.nt1 f4 = [r5]
	ldfe.a.nt1 f4 = [r5], r6
	ldfe.a.nt1 f4 = [r5], 166
	ldfe.a.nta f4 = [r5]
	ldfe.a.nta f4 = [r5], r6
	ldfe.a.nta f4 = [r5], 179

	ldfe.sa f4 = [r5]
	ldfe.sa f4 = [r5], r6
	ldfe.sa f4 = [r5], 192
	ldfe.sa.nt1 f4 = [r5]
	ldfe.sa.nt1 f4 = [r5], r6
	ldfe.sa.nt1 f4 = [r5], 205
	ldfe.sa.nta f4 = [r5]
	ldfe.sa.nta f4 = [r5], r6
	ldfe.sa.nta f4 = [r5], 218

	ldfe.c.clr f4 = [r5]
	ldfe.c.clr f4 = [r5], r6
	ldfe.c.clr f4 = [r5], 231
	ldfe.c.clr.nt1 f4 = [r5]
	ldfe.c.clr.nt1 f4 = [r5], r6
	ldfe.c.clr.nt1 f4 = [r5], 244
	ldfe.c.clr.nta f4 = [r5]
	ldfe.c.clr.nta f4 = [r5], r6
	ldfe.c.clr.nta f4 = [r5], -255

	ldfe.c.nc f4 = [r5]
	ldfe.c.nc f4 = [r5], r6
	ldfe.c.nc f4 = [r5], -242
	ldfe.c.nc.nt1 f4 = [r5]
	ldfe.c.nc.nt1 f4 = [r5], r6
	ldfe.c.nc.nt1 f4 = [r5], -229
	ldfe.c.nc.nta f4 = [r5]
	ldfe.c.nc.nta f4 = [r5], r6
	ldfe.c.nc.nta f4 = [r5], -216

	ldf.fill f4 = [r5]
	ldf.fill f4 = [r5], r6
	ldf.fill f4 = [r5], -203
	ldf.fill.nt1 f4 = [r5]
	ldf.fill.nt1 f4 = [r5], r6
	ldf.fill.nt1 f4 = [r5], -190
	ldf.fill.nta f4 = [r5]
	ldf.fill.nta f4 = [r5], r6
	ldf.fill.nta f4 = [r5], -177

	stfs [r4] = f5
	stfs [r4] = f5, -164
	stfs.nta [r4] = f5
	stfs.nta [r4] = f5, -151

	stfd [r4] = f5
	stfd [r4] = f5, -138
	stfd.nta [r4] = f5
	stfd.nta [r4] = f5, -125

	stf8 [r4] = f5
	stf8 [r4] = f5, -112
	stf8.nta [r4] = f5
	stf8.nta [r4] = f5, -99

	stfe [r4] = f5
	stfe [r4] = f5, -86
	stfe.nta [r4] = f5
	stfe.nta [r4] = f5, -73

	stf.spill [r4] = f5
	stf.spill [r4] = f5, -60
	stf.spill.nta [r4] = f5
	stf.spill.nta [r4] = f5, -47

	ldfps f4, f5 = [r5]
	ldfps f4, f5 = [r5], 8
	ldfps.nt1 f4, f5 = [r5]
	ldfps.nt1 f4, f5 = [r5], 8
	ldfps.nta f4, f5 = [r5]
	ldfps.nta f4, f5 = [r5], 8

	ldfps.s f4, f5 = [r5]
	ldfps.s f4, f5 = [r5], 8
	ldfps.s.nt1 f4, f5 = [r5]
	ldfps.s.nt1 f4, f5 = [r5], 8
	ldfps.s.nta f4, f5 = [r5]
	ldfps.s.nta f4, f5 = [r5], 8

	ldfps.a f4, f5 = [r5]
	ldfps.a f4, f5 = [r5], 8
	ldfps.a.nt1 f4, f5 = [r5]
	ldfps.a.nt1 f4, f5 = [r5], 8
	ldfps.a.nta f4, f5 = [r5]
	ldfps.a.nta f4, f5 = [r5], 8

	ldfps.sa f4, f5 = [r5]
	ldfps.sa f4, f5 = [r5], 8
	ldfps.sa.nt1 f4, f5 = [r5]
	ldfps.sa.nt1 f4, f5 = [r5], 8
	ldfps.sa.nta f4, f5 = [r5]
	ldfps.sa.nta f4, f5 = [r5], 8

	ldfps.c.clr f4, f5 = [r5]
	ldfps.c.clr f4, f5 = [r5], 8
	ldfps.c.clr.nt1 f4, f5 = [r5]
	ldfps.c.clr.nt1 f4, f5 = [r5], 8
	ldfps.c.clr.nta f4, f5 = [r5]
	ldfps.c.clr.nta f4, f5 = [r5], 8

	ldfps.c.nc f4, f5 = [r5]
	ldfps.c.nc f4, f5 = [r5], 8
	ldfps.c.nc.nt1 f4, f5 = [r5]
	ldfps.c.nc.nt1 f4, f5 = [r5], 8
	ldfps.c.nc.nta f4, f5 = [r5]
	ldfps.c.nc.nta f4, f5 = [r5], 8

	ldfpd f4, f5 = [r5]
	ldfpd f4, f5 = [r5], 16
	ldfpd.nt1 f4, f5 = [r5]
	ldfpd.nt1 f4, f5 = [r5], 16
	ldfpd.nta f4, f5 = [r5]
	ldfpd.nta f4, f5 = [r5], 16

	ldfpd.s f4, f5 = [r5]
	ldfpd.s f4, f5 = [r5], 16
	ldfpd.s.nt1 f4, f5 = [r5]
	ldfpd.s.nt1 f4, f5 = [r5], 16
	ldfpd.s.nta f4, f5 = [r5]
	ldfpd.s.nta f4, f5 = [r5], 16

	ldfpd.a f4, f5 = [r5]
	ldfpd.a f4, f5 = [r5], 16
	ldfpd.a.nt1 f4, f5 = [r5]
	ldfpd.a.nt1 f4, f5 = [r5], 16
	ldfpd.a.nta f4, f5 = [r5]
	ldfpd.a.nta f4, f5 = [r5], 16

	ldfpd.sa f4, f5 = [r5]
	ldfpd.sa f4, f5 = [r5], 16
	ldfpd.sa.nt1 f4, f5 = [r5]
	ldfpd.sa.nt1 f4, f5 = [r5], 16
	ldfpd.sa.nta f4, f5 = [r5]
	ldfpd.sa.nta f4, f5 = [r5], 16

	ldfpd.c.clr f4, f5 = [r5]
	ldfpd.c.clr f4, f5 = [r5], 16
	ldfpd.c.clr.nt1 f4, f5 = [r5]
	ldfpd.c.clr.nt1 f4, f5 = [r5], 16
	ldfpd.c.clr.nta f4, f5 = [r5]
	ldfpd.c.clr.nta f4, f5 = [r5], 16

	ldfpd.c.nc f4, f5 = [r5]
	ldfpd.c.nc f4, f5 = [r5], 16
	ldfpd.c.nc.nt1 f4, f5 = [r5]
	ldfpd.c.nc.nt1 f4, f5 = [r5], 16
	ldfpd.c.nc.nta f4, f5 = [r5]
	ldfpd.c.nc.nta f4, f5 = [r5], 16

	ldfp8 f4, f5 = [r5]
	ldfp8 f4, f5 = [r5], 16
	ldfp8.nt1 f4, f5 = [r5]
	ldfp8.nt1 f4, f5 = [r5], 16
	ldfp8.nta f4, f5 = [r5]
	ldfp8.nta f4, f5 = [r5], 16

	ldfp8.s f4, f5 = [r5]
	ldfp8.s f4, f5 = [r5], 16
	ldfp8.s.nt1 f4, f5 = [r5]
	ldfp8.s.nt1 f4, f5 = [r5], 16
	ldfp8.s.nta f4, f5 = [r5]
	ldfp8.s.nta f4, f5 = [r5], 16

	ldfp8.a f4, f5 = [r5]
	ldfp8.a f4, f5 = [r5], 16
	ldfp8.a.nt1 f4, f5 = [r5]
	ldfp8.a.nt1 f4, f5 = [r5], 16
	ldfp8.a.nta f4, f5 = [r5]
	ldfp8.a.nta f4, f5 = [r5], 16

	ldfp8.sa f4, f5 = [r5]
	ldfp8.sa f4, f5 = [r5], 16
	ldfp8.sa.nt1 f4, f5 = [r5]
	ldfp8.sa.nt1 f4, f5 = [r5], 16
	ldfp8.sa.nta f4, f5 = [r5]
	ldfp8.sa.nta f4, f5 = [r5], 16

	ldfp8.c.clr f4, f5 = [r5]
	ldfp8.c.clr f4, f5 = [r5], 16
	ldfp8.c.clr.nt1 f4, f5 = [r5]
	ldfp8.c.clr.nt1 f4, f5 = [r5], 16
	ldfp8.c.clr.nta f4, f5 = [r5]
	ldfp8.c.clr.nta f4, f5 = [r5], 16

	ldfp8.c.nc f4, f5 = [r5]
	ldfp8.c.nc f4, f5 = [r5], 16
	ldfp8.c.nc.nt1 f4, f5 = [r5]
	ldfp8.c.nc.nt1 f4, f5 = [r5], 16
	ldfp8.c.nc.nta f4, f5 = [r5]
	ldfp8.c.nc.nta f4, f5 = [r5], 16

	lfetch [r4]
	lfetch [r4], r5
	lfetch [r4], -34
	lfetch.nt1 [r4]
	lfetch.nt1 [r4], r5
	lfetch.nt1 [r4], -21
	lfetch.nt2 [r4]
	lfetch.nt2 [r4], r5
	lfetch.nt2 [r4], -8
	lfetch.nta [r4]
	lfetch.nta [r4], r5
	lfetch.nta [r4], 5

	lfetch.fault [r4]
	lfetch.fault [r4], r5
	lfetch.fault [r4], 18
	lfetch.fault.nt1 [r4]
	lfetch.fault.nt1 [r4], r5
	lfetch.fault.nt1 [r4], 31
	lfetch.fault.nt2 [r4]
	lfetch.fault.nt2 [r4], r5
	lfetch.fault.nt2 [r4], 44
	lfetch.fault.nta [r4]
	lfetch.fault.nta [r4], r5
	lfetch.fault.nta [r4], 57

	lfetch.excl [r4]
	lfetch.excl [r4], r5
	lfetch.excl [r4], 70
	lfetch.excl.nt1 [r4]
	lfetch.excl.nt1 [r4], r5
	lfetch.excl.nt1 [r4], 83
	lfetch.excl.nt2 [r4]
	lfetch.excl.nt2 [r4], r5
	lfetch.excl.nt2 [r4], 96
	lfetch.excl.nta [r4]
	lfetch.excl.nta [r4], r5
	lfetch.excl.nta [r4], 109

	lfetch.fault.excl [r4]
	lfetch.fault.excl [r4], r5
	lfetch.fault.excl [r4], 122
	lfetch.fault.excl.nt1 [r4]
	lfetch.fault.excl.nt1 [r4], r5
	lfetch.fault.excl.nt1 [r4], 135
	lfetch.fault.excl.nt2 [r4]
	lfetch.fault.excl.nt2 [r4], r5
	lfetch.fault.excl.nt2 [r4], 148
	lfetch.fault.excl.nta [r4]
	lfetch.fault.excl.nta [r4], r5
	lfetch.fault.excl.nta [r4], 161

	cmpxchg1.acq r4 = [r5], r6, ar.ccv
	cmpxchg1.acq.nt1 r4 = [r5], r6, ar.ccv
	cmpxchg1.acq.nta r4 = [r5], r6, ar.ccv

	cmpxchg1.rel r4 = [r5], r6, ar.ccv
	cmpxchg1.rel.nt1 r4 = [r5], r6, ar.ccv
	cmpxchg1.rel.nta r4 = [r5], r6, ar.ccv

	cmpxchg2.acq r4 = [r5], r6, ar.ccv
	cmpxchg2.acq.nt1 r4 = [r5], r6, ar.ccv
	cmpxchg2.acq.nta r4 = [r5], r6, ar.ccv

	cmpxchg2.rel r4 = [r5], r6, ar.ccv
	cmpxchg2.rel.nt1 r4 = [r5], r6, ar.ccv
	cmpxchg2.rel.nta r4 = [r5], r6, ar.ccv

	cmpxchg4.acq r4 = [r5], r6, ar.ccv
	cmpxchg4.acq.nt1 r4 = [r5], r6, ar.ccv
	cmpxchg4.acq.nta r4 = [r5], r6, ar.ccv

	cmpxchg4.rel r4 = [r5], r6, ar.ccv
	cmpxchg4.rel.nt1 r4 = [r5], r6, ar.ccv
	cmpxchg4.rel.nta r4 = [r5], r6, ar.ccv

	cmpxchg8.acq r4 = [r5], r6, ar.ccv
	cmpxchg8.acq.nt1 r4 = [r5], r6, ar.ccv
	cmpxchg8.acq.nta r4 = [r5], r6, ar.ccv

	cmpxchg8.rel r4 = [r5], r6, ar.ccv
	cmpxchg8.rel.nt1 r4 = [r5], r6, ar.ccv
	cmpxchg8.rel.nta r4 = [r5], r6, ar.ccv

	xchg1 r4 = [r5], r6
	xchg1.nt1 r4 = [r5], r6
	xchg1.nta r4 = [r5], r6

	xchg2 r4 = [r5], r6
	xchg2.nt1 r4 = [r5], r6
	xchg2.nta r4 = [r5], r6

	xchg4 r4 = [r5], r6
	xchg4.nt1 r4 = [r5], r6
	xchg4.nta r4 = [r5], r6

	xchg8 r4 = [r5], r6
	xchg8.nt1 r4 = [r5], r6
	xchg8.nta r4 = [r5], r6

	fetchadd4.acq r4 = [r5], -16
	fetchadd4.acq.nt1 r4 = [r5], -8
	fetchadd4.acq.nta r4 = [r5], -4

	fetchadd8.acq r4 = [r5], -1
	fetchadd8.acq.nt1 r4 = [r5], 1
	fetchadd8.acq.nta r4 = [r5], 4

	fetchadd4.rel r4 = [r5], 8
	fetchadd4.rel.nt1 r4 = [r5], 16
	fetchadd4.rel.nta r4 = [r5], -16

	fetchadd8.rel r4 = [r5], -8
	fetchadd8.rel.nt1 r4 = [r5], -4
	fetchadd8.rel.nta r4 = [r5], -1

	setf.sig f4 = r5
	setf.exp f4 = r5
	setf.s f4 = r5
	setf.d f4 = r5

	getf.sig r4 = f5
	getf.exp r4 = f5
	getf.s r4 = f5
	getf.d r4 = f5

	chk.s.m r4, _start
	chk.s f4, _start
	chk.a.nc r4, _start
	chk.a.clr r4, _start
	chk.a.nc f4, _start
	chk.a.clr f4, _start

	invala
	fwb
	mf
	mf.a
	srlz.d
	srlz.i
	sync.i
	nop.m 0
	nop.i 0;;

	{ .mii; alloc r4 = ar.pfs, 2, 10, 16, 16;; }

	{ .mii; flushrs;; }
	{ .mii; loadrs }

	invala.e r4
	invala.e f4

	fc r4
	ptc.e r4

	break.m 0
	break.m 0x1ffff

	nop.m 0
	nop.m 0x1ffff

	probe.r r4 = r5, r6
	probe.w r4 = r5, r6

	probe.r r4 = r5, 0
	probe.w r4 = r5, 1

	probe.r.fault r3, 2
	probe.w.fault r3, 3
	probe.rw.fault r3, 0

	{ .mmi; itc.d r8;; nop.m 0x0; nop.i 0x0;; }
	itc.i r9;; 
	
	sum 0x1234
	rum 0x5aaaaa
	ssm 0xffffff
	rsm 0x400000

	ptc.l r4, r5
	{ .mmi; ptc.g r4, r5;; nop.m 0x0; nop.i 0x0 }
	{ .mmi; ptc.ga r4, r5;; nop.m 0x0; nop.i 0x0 }
	ptr.d r4, r5
	ptr.i r4, r5

	thash r4 = r5
	ttag r4 = r5
	tpa r4 = r5
	tak r4 = r5

	# instructions added by SDM2.1:

	hint.m 0
	hint.m @pause
	hint.m 0x1ffff

	cmp8xchg16.acq r4 = [r5], r6, ar25, ar.ccv
	cmp8xchg16.acq.nt1 r4 = [r5], r6, ar.csd, ar.ccv
	cmp8xchg16.acq.nta r4 = [r5], r6, ar.csd, ar.ccv

	cmp8xchg16.rel r4 = [r5], r6, ar.csd, ar.ccv
	cmp8xchg16.rel.nt1 r4 = [r5], r6, ar.csd, ar.ccv
	cmp8xchg16.rel.nta r4 = [r5], r6, ar.csd, ar.ccv

	fc.i r4

	ld16 r4, ar25 = [r5]
	ld16.nt1 r4, ar.csd = [r5]
	ld16.nta r4, ar.csd = [r5]

	ld16.acq r4, ar25 = [r5]
	ld16.acq.nt1 r4, ar.csd = [r5]
	ld16.acq.nta r4, ar.csd = [r5]

	st16 [r4] = r5, ar25
	st16.nta [r4] = r5, ar.csd

	st16.rel [r4] = r5, ar.csd
	st16.rel.nta [r4] = r5, ar.csd
