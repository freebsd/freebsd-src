/*-
 * Copyright (c) 2014 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef ARM_AT91_AT91_GPIO_H
#define ARM_AT91_AT91_GPIO_H

typedef uint32_t at91_pin_t;

#define AT91_PIN_NONE	0xfffffffful	/* No pin / Not GPIO controlled */

/*
 * Map Atmel PIO pins to a unique number. They are just numbered sequentially.
 */

#define	AT91_PIN_PA0	(at91_pin_t)0
#define	AT91_PIN_PA1	(at91_pin_t)1
#define	AT91_PIN_PA2	(at91_pin_t)2
#define	AT91_PIN_PA3	(at91_pin_t)3
#define	AT91_PIN_PA4	(at91_pin_t)4
#define	AT91_PIN_PA5	(at91_pin_t)5
#define	AT91_PIN_PA6	(at91_pin_t)6
#define	AT91_PIN_PA7	(at91_pin_t)7
#define	AT91_PIN_PA8	(at91_pin_t)8
#define	AT91_PIN_PA9	(at91_pin_t)9
#define	AT91_PIN_PA10	(at91_pin_t)10
#define	AT91_PIN_PA11	(at91_pin_t)11
#define	AT91_PIN_PA12	(at91_pin_t)12
#define	AT91_PIN_PA13	(at91_pin_t)13
#define	AT91_PIN_PA14	(at91_pin_t)14
#define	AT91_PIN_PA15	(at91_pin_t)15
#define	AT91_PIN_PA16	(at91_pin_t)16
#define	AT91_PIN_PA17	(at91_pin_t)17
#define	AT91_PIN_PA18	(at91_pin_t)18
#define	AT91_PIN_PA19	(at91_pin_t)19
#define	AT91_PIN_PA20	(at91_pin_t)20
#define	AT91_PIN_PA21	(at91_pin_t)21
#define	AT91_PIN_PA22	(at91_pin_t)22
#define	AT91_PIN_PA23	(at91_pin_t)23
#define	AT91_PIN_PA24	(at91_pin_t)24
#define	AT91_PIN_PA25	(at91_pin_t)25
#define	AT91_PIN_PA26	(at91_pin_t)26
#define	AT91_PIN_PA27	(at91_pin_t)27
#define	AT91_PIN_PA28	(at91_pin_t)28
#define	AT91_PIN_PA29	(at91_pin_t)29
#define	AT91_PIN_PA30	(at91_pin_t)30
#define	AT91_PIN_PA31	(at91_pin_t)31
#define	AT91_PIN_PB0	(at91_pin_t)32
#define	AT91_PIN_PB1	(at91_pin_t)33
#define	AT91_PIN_PB2	(at91_pin_t)34
#define	AT91_PIN_PB3	(at91_pin_t)35
#define	AT91_PIN_PB4	(at91_pin_t)36
#define	AT91_PIN_PB5	(at91_pin_t)37
#define	AT91_PIN_PB6	(at91_pin_t)38
#define	AT91_PIN_PB7	(at91_pin_t)39
#define	AT91_PIN_PB8	(at91_pin_t)40
#define	AT91_PIN_PB9	(at91_pin_t)41
#define	AT91_PIN_PB10	(at91_pin_t)42
#define	AT91_PIN_PB11	(at91_pin_t)43
#define	AT91_PIN_PB12	(at91_pin_t)44
#define	AT91_PIN_PB13	(at91_pin_t)45
#define	AT91_PIN_PB14	(at91_pin_t)46
#define	AT91_PIN_PB15	(at91_pin_t)47
#define	AT91_PIN_PB16	(at91_pin_t)48
#define	AT91_PIN_PB17	(at91_pin_t)49
#define	AT91_PIN_PB18	(at91_pin_t)50
#define	AT91_PIN_PB19	(at91_pin_t)51
#define	AT91_PIN_PB20	(at91_pin_t)52
#define	AT91_PIN_PB21	(at91_pin_t)53
#define	AT91_PIN_PB22	(at91_pin_t)54
#define	AT91_PIN_PB23	(at91_pin_t)55
#define	AT91_PIN_PB24	(at91_pin_t)56
#define	AT91_PIN_PB25	(at91_pin_t)57
#define	AT91_PIN_PB26	(at91_pin_t)58
#define	AT91_PIN_PB27	(at91_pin_t)59
#define	AT91_PIN_PB28	(at91_pin_t)60
#define	AT91_PIN_PB29	(at91_pin_t)61
#define	AT91_PIN_PB30	(at91_pin_t)62
#define	AT91_PIN_PB31	(at91_pin_t)63
#define	AT91_PIN_PC0	(at91_pin_t)64
#define	AT91_PIN_PC1	(at91_pin_t)65
#define	AT91_PIN_PC2	(at91_pin_t)66
#define	AT91_PIN_PC3	(at91_pin_t)67
#define	AT91_PIN_PC4	(at91_pin_t)68
#define	AT91_PIN_PC5	(at91_pin_t)69
#define	AT91_PIN_PC6	(at91_pin_t)70
#define	AT91_PIN_PC7	(at91_pin_t)71
#define	AT91_PIN_PC8	(at91_pin_t)72
#define	AT91_PIN_PC9	(at91_pin_t)73
#define	AT91_PIN_PC10	(at91_pin_t)74
#define	AT91_PIN_PC11	(at91_pin_t)75
#define	AT91_PIN_PC12	(at91_pin_t)76
#define	AT91_PIN_PC13	(at91_pin_t)77
#define	AT91_PIN_PC14	(at91_pin_t)78
#define	AT91_PIN_PC15	(at91_pin_t)79
#define	AT91_PIN_PC16	(at91_pin_t)80
#define	AT91_PIN_PC17	(at91_pin_t)81
#define	AT91_PIN_PC18	(at91_pin_t)82
#define	AT91_PIN_PC19	(at91_pin_t)83
#define	AT91_PIN_PC20	(at91_pin_t)84
#define	AT91_PIN_PC21	(at91_pin_t)85
#define	AT91_PIN_PC22	(at91_pin_t)86
#define	AT91_PIN_PC23	(at91_pin_t)87
#define	AT91_PIN_PC24	(at91_pin_t)88
#define	AT91_PIN_PC25	(at91_pin_t)89
#define	AT91_PIN_PC26	(at91_pin_t)90
#define	AT91_PIN_PC27	(at91_pin_t)91
#define	AT91_PIN_PC28	(at91_pin_t)92
#define	AT91_PIN_PC29	(at91_pin_t)93
#define	AT91_PIN_PC30	(at91_pin_t)94
#define	AT91_PIN_PC31	(at91_pin_t)95
#define	AT91_PIN_PD0	(at91_pin_t)96
#define	AT91_PIN_PD1	(at91_pin_t)97
#define	AT91_PIN_PD2	(at91_pin_t)98
#define	AT91_PIN_PD3	(at91_pin_t)99
#define	AT91_PIN_PD4	(at91_pin_t)100
#define	AT91_PIN_PD5	(at91_pin_t)101
#define	AT91_PIN_PD6	(at91_pin_t)102
#define	AT91_PIN_PD7	(at91_pin_t)103
#define	AT91_PIN_PD8	(at91_pin_t)104
#define	AT91_PIN_PD9	(at91_pin_t)105
#define	AT91_PIN_PD10	(at91_pin_t)106
#define	AT91_PIN_PD11	(at91_pin_t)107
#define	AT91_PIN_PD12	(at91_pin_t)108
#define	AT91_PIN_PD13	(at91_pin_t)109
#define	AT91_PIN_PD14	(at91_pin_t)110
#define	AT91_PIN_PD15	(at91_pin_t)111
#define	AT91_PIN_PD16	(at91_pin_t)112
#define	AT91_PIN_PD17	(at91_pin_t)113
#define	AT91_PIN_PD18	(at91_pin_t)114
#define	AT91_PIN_PD19	(at91_pin_t)115
#define	AT91_PIN_PD20	(at91_pin_t)116
#define	AT91_PIN_PD21	(at91_pin_t)117
#define	AT91_PIN_PD22	(at91_pin_t)118
#define	AT91_PIN_PD23	(at91_pin_t)119
#define	AT91_PIN_PD24	(at91_pin_t)120
#define	AT91_PIN_PD25	(at91_pin_t)121
#define	AT91_PIN_PD26	(at91_pin_t)122
#define	AT91_PIN_PD27	(at91_pin_t)123
#define	AT91_PIN_PD28	(at91_pin_t)124
#define	AT91_PIN_PD29	(at91_pin_t)125
#define	AT91_PIN_PD30	(at91_pin_t)126
#define	AT91_PIN_PD31	(at91_pin_t)127
#define	AT91_PIN_PE0	(at91_pin_t)128
#define	AT91_PIN_PE1	(at91_pin_t)129
#define	AT91_PIN_PE2	(at91_pin_t)130
#define	AT91_PIN_PE3	(at91_pin_t)131
#define	AT91_PIN_PE4	(at91_pin_t)132
#define	AT91_PIN_PE5	(at91_pin_t)133
#define	AT91_PIN_PE6	(at91_pin_t)134
#define	AT91_PIN_PE7	(at91_pin_t)135
#define	AT91_PIN_PE8	(at91_pin_t)136
#define	AT91_PIN_PE9	(at91_pin_t)137
#define	AT91_PIN_PE10	(at91_pin_t)138
#define	AT91_PIN_PE11	(at91_pin_t)139
#define	AT91_PIN_PE12	(at91_pin_t)140
#define	AT91_PIN_PE13	(at91_pin_t)141
#define	AT91_PIN_PE14	(at91_pin_t)142
#define	AT91_PIN_PE15	(at91_pin_t)143
#define	AT91_PIN_PE16	(at91_pin_t)144
#define	AT91_PIN_PE17	(at91_pin_t)145
#define	AT91_PIN_PE18	(at91_pin_t)146
#define	AT91_PIN_PE19	(at91_pin_t)147
#define	AT91_PIN_PE20	(at91_pin_t)148
#define	AT91_PIN_PE21	(at91_pin_t)149
#define	AT91_PIN_PE22	(at91_pin_t)150
#define	AT91_PIN_PE23	(at91_pin_t)151
#define	AT91_PIN_PE24	(at91_pin_t)152
#define	AT91_PIN_PE25	(at91_pin_t)153
#define	AT91_PIN_PE26	(at91_pin_t)154
#define	AT91_PIN_PE27	(at91_pin_t)155
#define	AT91_PIN_PE28	(at91_pin_t)156
#define	AT91_PIN_PE29	(at91_pin_t)157
#define	AT91_PIN_PE30	(at91_pin_t)158
#define	AT91_PIN_PE31	(at91_pin_t)159
#define	AT91_PIN_PF0	(at91_pin_t)160
#define	AT91_PIN_PF1	(at91_pin_t)161
#define	AT91_PIN_PF2	(at91_pin_t)162
#define	AT91_PIN_PF3	(at91_pin_t)163
#define	AT91_PIN_PF4	(at91_pin_t)164
#define	AT91_PIN_PF5	(at91_pin_t)165
#define	AT91_PIN_PF6	(at91_pin_t)166
#define	AT91_PIN_PF7	(at91_pin_t)167
#define	AT91_PIN_PF8	(at91_pin_t)168
#define	AT91_PIN_PF9	(at91_pin_t)169
#define	AT91_PIN_PF10	(at91_pin_t)170
#define	AT91_PIN_PF11	(at91_pin_t)171
#define	AT91_PIN_PF12	(at91_pin_t)172
#define	AT91_PIN_PF13	(at91_pin_t)173
#define	AT91_PIN_PF14	(at91_pin_t)174
#define	AT91_PIN_PF15	(at91_pin_t)175
#define	AT91_PIN_PF16	(at91_pin_t)176
#define	AT91_PIN_PF17	(at91_pin_t)177
#define	AT91_PIN_PF18	(at91_pin_t)178
#define	AT91_PIN_PF19	(at91_pin_t)179
#define	AT91_PIN_PF20	(at91_pin_t)180
#define	AT91_PIN_PF21	(at91_pin_t)181
#define	AT91_PIN_PF22	(at91_pin_t)182
#define	AT91_PIN_PF23	(at91_pin_t)183
#define	AT91_PIN_PF24	(at91_pin_t)184
#define	AT91_PIN_PF25	(at91_pin_t)185
#define	AT91_PIN_PF26	(at91_pin_t)186
#define	AT91_PIN_PF27	(at91_pin_t)187
#define	AT91_PIN_PF28	(at91_pin_t)188
#define	AT91_PIN_PF29	(at91_pin_t)189
#define	AT91_PIN_PF30	(at91_pin_t)190
#define	AT91_PIN_PF31	(at91_pin_t)191
#define	AT91_PIN_PG0	(at91_pin_t)192
#define	AT91_PIN_PG1	(at91_pin_t)193
#define	AT91_PIN_PG2	(at91_pin_t)194
#define	AT91_PIN_PG3	(at91_pin_t)195
#define	AT91_PIN_PG4	(at91_pin_t)196
#define	AT91_PIN_PG5	(at91_pin_t)197
#define	AT91_PIN_PG6	(at91_pin_t)198
#define	AT91_PIN_PG7	(at91_pin_t)199
#define	AT91_PIN_PG8	(at91_pin_t)200
#define	AT91_PIN_PG9	(at91_pin_t)201
#define	AT91_PIN_PG10	(at91_pin_t)202
#define	AT91_PIN_PG11	(at91_pin_t)203
#define	AT91_PIN_PG12	(at91_pin_t)204
#define	AT91_PIN_PG13	(at91_pin_t)205
#define	AT91_PIN_PG14	(at91_pin_t)206
#define	AT91_PIN_PG15	(at91_pin_t)207
#define	AT91_PIN_PG16	(at91_pin_t)208
#define	AT91_PIN_PG17	(at91_pin_t)209
#define	AT91_PIN_PG18	(at91_pin_t)210
#define	AT91_PIN_PG19	(at91_pin_t)211
#define	AT91_PIN_PG20	(at91_pin_t)212
#define	AT91_PIN_PG21	(at91_pin_t)213
#define	AT91_PIN_PG22	(at91_pin_t)214
#define	AT91_PIN_PG23	(at91_pin_t)215
#define	AT91_PIN_PG24	(at91_pin_t)216
#define	AT91_PIN_PG25	(at91_pin_t)217
#define	AT91_PIN_PG26	(at91_pin_t)218
#define	AT91_PIN_PG27	(at91_pin_t)219
#define	AT91_PIN_PG28	(at91_pin_t)220
#define	AT91_PIN_PG29	(at91_pin_t)221
#define	AT91_PIN_PG30	(at91_pin_t)222
#define	AT91_PIN_PG31	(at91_pin_t)223
#define	AT91_PIN_PH0	(at91_pin_t)224
#define	AT91_PIN_PH1	(at91_pin_t)225
#define	AT91_PIN_PH2	(at91_pin_t)226
#define	AT91_PIN_PH3	(at91_pin_t)227
#define	AT91_PIN_PH4	(at91_pin_t)228
#define	AT91_PIN_PH5	(at91_pin_t)229
#define	AT91_PIN_PH6	(at91_pin_t)230
#define	AT91_PIN_PH7	(at91_pin_t)231
#define	AT91_PIN_PH8	(at91_pin_t)232
#define	AT91_PIN_PH9	(at91_pin_t)233
#define	AT91_PIN_PH10	(at91_pin_t)234
#define	AT91_PIN_PH11	(at91_pin_t)235
#define	AT91_PIN_PH12	(at91_pin_t)236
#define	AT91_PIN_PH13	(at91_pin_t)237
#define	AT91_PIN_PH14	(at91_pin_t)238
#define	AT91_PIN_PH15	(at91_pin_t)239
#define	AT91_PIN_PH16	(at91_pin_t)240
#define	AT91_PIN_PH17	(at91_pin_t)241
#define	AT91_PIN_PH18	(at91_pin_t)242
#define	AT91_PIN_PH19	(at91_pin_t)243
#define	AT91_PIN_PH20	(at91_pin_t)244
#define	AT91_PIN_PH21	(at91_pin_t)245
#define	AT91_PIN_PH22	(at91_pin_t)246
#define	AT91_PIN_PH23	(at91_pin_t)247
#define	AT91_PIN_PH24	(at91_pin_t)248
#define	AT91_PIN_PH25	(at91_pin_t)249
#define	AT91_PIN_PH26	(at91_pin_t)250
#define	AT91_PIN_PH27	(at91_pin_t)251
#define	AT91_PIN_PH28	(at91_pin_t)252
#define	AT91_PIN_PH29	(at91_pin_t)253
#define	AT91_PIN_PH30	(at91_pin_t)254
#define	AT91_PIN_PH31	(at91_pin_t)255

#endif /* ARM_AT91_AT91_GPIO_H */
