			/* THIS FILE IS AUTOMAGICALLY GENERATED, DON'T EDIT IT */
#define ARG_MASK 0x0f
#define ARG_SRC 0x01
#define ARG_DST 0x02
#define ARG_RS 0x01
#define ARG_RD 0x02
#define ARG_RA 0x03
#define ARG_RB 0x04
#define ARG_RR 0x05
#define ARG_RX 0x06
#define ARG_IMM4 0x01
#define ARG_IMM8 0x02
#define ARG_IMM16 0x03
#define ARG_IMM32 0x04
#define ARG_IMMN 0x05
#define ARG_IMMNMINUS1 0x05
#define ARG_IMM_1 0x06
#define ARG_IMM_2 0x07
#define ARG_DISP16 0x08
#define ARG_NIM8 0x09
#define ARG_IMM2 0x0a
#define ARG_IMM1OR2 0x0b
#define ARG_DISP12 0x0b
#define ARG_DISP8 0x0c
#define ARG_IMM4M1 0x0d
#define CLASS_MASK 0x1fff0
#define CLASS_X 0x10
#define CLASS_BA 0x20
#define CLASS_DA 0x30
#define CLASS_BX 0x40
#define CLASS_DISP 0x50
#define CLASS_IMM 0x60
#define CLASS_CC 0x70
#define CLASS_CTRL 0x80
#define CLASS_ADDRESS 0xd0
#define CLASS_0CCC 0xe0
#define CLASS_1CCC 0xf0
#define CLASS_0DISP7 0x100
#define CLASS_1DISP7 0x200
#define CLASS_01II 0x300
#define CLASS_00II 0x400
#define CLASS_BIT 0x500
#define CLASS_FLAGS 0x600
#define CLASS_IR 0x700
#define CLASS_DISP8 0x800
#define CLASS_BIT_1OR2 0x900
#define CLASS_REG 0x7000
#define CLASS_REG_BYTE 0x2000
#define CLASS_REG_WORD 0x3000
#define CLASS_REG_QUAD 0x4000
#define CLASS_REG_LONG 0x5000
#define CLASS_REGN0 0x8000
#define CLASS_PR 0x10000
#define OPC_adc 0
#define OPC_adcb 1
#define OPC_add 2
#define OPC_addb 3
#define OPC_addl 4
#define OPC_and 5
#define OPC_andb 6
#define OPC_bit 7
#define OPC_bitb 8
#define OPC_call 9
#define OPC_calr 10
#define OPC_clr 11
#define OPC_clrb 12
#define OPC_com 13
#define OPC_comb 14
#define OPC_comflg 15
#define OPC_cp 16
#define OPC_cpb 17
#define OPC_cpd 18
#define OPC_cpdb 19
#define OPC_cpdr 20
#define OPC_cpdrb 21
#define OPC_cpi 22
#define OPC_cpib 23
#define OPC_cpir 24
#define OPC_cpirb 25
#define OPC_cpl 26
#define OPC_cpsd 27
#define OPC_cpsdb 28
#define OPC_cpsdr 29
#define OPC_cpsdrb 30
#define OPC_cpsi 31
#define OPC_cpsib 32
#define OPC_cpsir 33
#define OPC_cpsirb 34
#define OPC_dab 35
#define OPC_dbjnz 36
#define OPC_dec 37
#define OPC_decb 38
#define OPC_di 39
#define OPC_div 40
#define OPC_divl 41
#define OPC_djnz 42
#define OPC_ei 43
#define OPC_ex 44
#define OPC_exb 45
#define OPC_exts 46
#define OPC_extsb 47
#define OPC_extsl 48
#define OPC_halt 49
#define OPC_in 50
#define OPC_inb 51
#define OPC_inc 52
#define OPC_incb 53
#define OPC_ind 54
#define OPC_indb 55
#define OPC_inib 56
#define OPC_inibr 57
#define OPC_iret 58
#define OPC_jp 59
#define OPC_jr 60
#define OPC_ld 61
#define OPC_lda 62
#define OPC_ldar 63
#define OPC_ldb 64
#define OPC_ldctl 65
#define OPC_ldir 66
#define OPC_ldirb 67
#define OPC_ldk 68
#define OPC_ldl 69
#define OPC_ldm 70
#define OPC_ldps 71
#define OPC_ldr 72
#define OPC_ldrb 73
#define OPC_ldrl 74
#define OPC_mbit 75
#define OPC_mreq 76
#define OPC_mres 77
#define OPC_mset 78
#define OPC_mult 79
#define OPC_multl 80
#define OPC_neg 81
#define OPC_negb 82
#define OPC_nop 83
#define OPC_or 84
#define OPC_orb 85
#define OPC_out 86
#define OPC_outb 87
#define OPC_outd 88
#define OPC_outdb 89
#define OPC_outib 90
#define OPC_outibr 91
#define OPC_pop 92
#define OPC_popl 93
#define OPC_push 94
#define OPC_pushl 95
#define OPC_res 96
#define OPC_resb 97
#define OPC_resflg 98
#define OPC_ret 99
#define OPC_rl 100
#define OPC_rlb 101
#define OPC_rlc 102
#define OPC_rlcb 103
#define OPC_rldb 104
#define OPC_rr 105
#define OPC_rrb 106
#define OPC_rrc 107
#define OPC_rrcb 108
#define OPC_rrdb 109
#define OPC_sbc 110
#define OPC_sbcb 111
#define OPC_sda 112
#define OPC_sdab 113
#define OPC_sdal 114
#define OPC_sdl 115
#define OPC_sdlb 116
#define OPC_sdll 117
#define OPC_set 118
#define OPC_setb 119
#define OPC_setflg 120
#define OPC_sinb 121
#define OPC_sind 122
#define OPC_sindb 123
#define OPC_sinib 124
#define OPC_sinibr 125
#define OPC_sla 126
#define OPC_slab 127
#define OPC_slal 128
#define OPC_sll 129
#define OPC_sllb 130
#define OPC_slll 131
#define OPC_sout 132
#define OPC_soutb 133
#define OPC_soutd 134
#define OPC_soutdb 135
#define OPC_soutib 136
#define OPC_soutibr 137
#define OPC_sra 138
#define OPC_srab 139
#define OPC_sral 140
#define OPC_srl 141
#define OPC_srlb 142
#define OPC_srll 143
#define OPC_sub 144
#define OPC_subb 145
#define OPC_subl 146
#define OPC_tcc 147
#define OPC_tccb 148
#define OPC_test 149
#define OPC_testb 150
#define OPC_testl 151
#define OPC_trdb 152
#define OPC_trdrb 153
#define OPC_trib 154
#define OPC_trirb 155
#define OPC_trtdrb 156
#define OPC_trtib 157
#define OPC_trtirb 158
#define OPC_trtrb 159
#define OPC_tset 160
#define OPC_tsetb 161
#define OPC_xor 162
#define OPC_xorb 163
#define OPC_ldd  164 
#define OPC_lddb  165 
#define OPC_lddr  166 
#define OPC_lddrb 167  
#define OPC_ldi  168 
#define OPC_ldib 169  
#define OPC_sc   170
#define OPC_bpt   171
#define OPC_ext0e 172
#define OPC_ext0f 172
#define OPC_ext8e 172
#define OPC_ext8f 172
#define OPC_rsvd36 172
#define OPC_rsvd38 172
#define OPC_rsvd78 172
#define OPC_rsvd7e 172
#define OPC_rsvd9d 172
#define OPC_rsvd9f 172
#define OPC_rsvdb9 172
#define OPC_rsvdbf 172
#define OPC_outi 173
#define OPC_ldctlb 174
#define OPC_sin 175
#define OPC_trtdb 176
typedef struct {
#ifdef NICENAMES
char *nicename;
int type;
int cycles;
int flags;
#endif
char *name;
unsigned char opcode;
void (*func) PARAMS ((void));
unsigned int arg_info[4];
unsigned int byte_info[10];
int noperands;
int length;
int idx;
} opcode_entry_type;
#ifdef DEFINE_TABLE
opcode_entry_type z8k_table[] = {


/* 1011 0101 ssss dddd *** adc rd,rs */
{
#ifdef NICENAMES
"adc rd,rs",16,5,
0x3c,
#endif
"adc",OPC_adc,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+0xb,CLASS_BIT+5,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,0},


/* 1011 0100 ssss dddd *** adcb rbd,rbs */
{
#ifdef NICENAMES
"adcb rbd,rbs",8,5,
0x3f,
#endif
"adcb",OPC_adcb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+0xb,CLASS_BIT+4,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,1},


/* 0000 0001 ssN0 dddd *** add rd,@rs */
{
#ifdef NICENAMES
"add rd,@rs",16,7,
0x3c,
#endif
"add",OPC_add,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+0,CLASS_BIT+1,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,2},


/* 0100 0001 0000 dddd address_src *** add rd,address_src */
{
#ifdef NICENAMES
"add rd,address_src",16,9,
0x3c,
#endif
"add",OPC_add,0,{CLASS_REG_WORD+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+4,CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,3},


/* 0100 0001 ssN0 dddd address_src *** add rd,address_src(rs) */
{
#ifdef NICENAMES
"add rd,address_src(rs)",16,10,
0x3c,
#endif
"add",OPC_add,0,{CLASS_REG_WORD+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+4,CLASS_BIT+1,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,4},


/* 0000 0001 0000 dddd imm16 *** add rd,imm16 */
{
#ifdef NICENAMES
"add rd,imm16",16,7,
0x3c,
#endif
"add",OPC_add,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+0,CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,5},


/* 1000 0001 ssss dddd *** add rd,rs */
{
#ifdef NICENAMES
"add rd,rs",16,4,
0x3c,
#endif
"add",OPC_add,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+1,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,6},


/* 0000 0000 ssN0 dddd *** addb rbd,@rs */
{
#ifdef NICENAMES
"addb rbd,@rs",8,7,
0x3f,
#endif
"addb",OPC_addb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+0,CLASS_BIT+0,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,7},


/* 0100 0000 0000 dddd address_src *** addb rbd,address_src */
{
#ifdef NICENAMES
"addb rbd,address_src",8,9,
0x3f,
#endif
"addb",OPC_addb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+4,CLASS_BIT+0,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,8},


/* 0100 0000 ssN0 dddd address_src *** addb rbd,address_src(rs) */
{
#ifdef NICENAMES
"addb rbd,address_src(rs)",8,10,
0x3f,
#endif
"addb",OPC_addb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+4,CLASS_BIT+0,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,9},


/* 0000 0000 0000 dddd imm8 imm8 *** addb rbd,imm8 */
{
#ifdef NICENAMES
"addb rbd,imm8",8,7,
0x3f,
#endif
"addb",OPC_addb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0,CLASS_BIT+0,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM8),CLASS_IMM+(ARG_IMM8),0,0,0,},2,4,10},


/* 1000 0000 ssss dddd *** addb rbd,rbs */
{
#ifdef NICENAMES
"addb rbd,rbs",8,4,
0x3f,
#endif
"addb",OPC_addb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,11},


/* 0001 0110 ssN0 dddd *** addl rrd,@rs */
{
#ifdef NICENAMES
"addl rrd,@rs",32,14,
0x3c,
#endif
"addl",OPC_addl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+1,CLASS_BIT+6,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,12},


/* 0101 0110 0000 dddd address_src *** addl rrd,address_src */
{
#ifdef NICENAMES
"addl rrd,address_src",32,15,
0x3c,
#endif
"addl",OPC_addl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+5,CLASS_BIT+6,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,13},


/* 0101 0110 ssN0 dddd address_src *** addl rrd,address_src(rs) */
{
#ifdef NICENAMES
"addl rrd,address_src(rs)",32,16,
0x3c,
#endif
"addl",OPC_addl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+6,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,14},


/* 0001 0110 0000 dddd imm32 *** addl rrd,imm32 */
{
#ifdef NICENAMES
"addl rrd,imm32",32,14,
0x3c,
#endif
"addl",OPC_addl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IMM+(ARG_IMM32),},
	{CLASS_BIT+1,CLASS_BIT+6,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM32),0,0,0,0,},2,6,15},


/* 1001 0110 ssss dddd *** addl rrd,rrs */
{
#ifdef NICENAMES
"addl rrd,rrs",32,8,
0x3c,
#endif
"addl",OPC_addl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_REG_LONG+(ARG_RS),},
	{CLASS_BIT+9,CLASS_BIT+6,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,16},


/* 0000 0111 ssN0 dddd *** and rd,@rs */
{
#ifdef NICENAMES
"and rd,@rs",16,7,
0x18,
#endif
"and",OPC_and,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+0,CLASS_BIT+7,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,17},


/* 0100 0111 0000 dddd address_src *** and rd,address_src */
{
#ifdef NICENAMES
"and rd,address_src",16,9,
0x18,
#endif
"and",OPC_and,0,{CLASS_REG_WORD+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+4,CLASS_BIT+7,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,18},


/* 0100 0111 ssN0 dddd address_src *** and rd,address_src(rs) */
{
#ifdef NICENAMES
"and rd,address_src(rs)",16,10,
0x18,
#endif
"and",OPC_and,0,{CLASS_REG_WORD+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+4,CLASS_BIT+7,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,19},


/* 0000 0111 0000 dddd imm16 *** and rd,imm16 */
{
#ifdef NICENAMES
"and rd,imm16",16,7,
0x18,
#endif
"and",OPC_and,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+0,CLASS_BIT+7,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,20},


/* 1000 0111 ssss dddd *** and rd,rs */
{
#ifdef NICENAMES
"and rd,rs",16,4,
0x18,
#endif
"and",OPC_and,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+7,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,21},


/* 0000 0110 ssN0 dddd *** andb rbd,@rs */
{
#ifdef NICENAMES
"andb rbd,@rs",8,7,
0x1c,
#endif
"andb",OPC_andb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+0,CLASS_BIT+6,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,22},


/* 0100 0110 0000 dddd address_src *** andb rbd,address_src */
{
#ifdef NICENAMES
"andb rbd,address_src",8,9,
0x1c,
#endif
"andb",OPC_andb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+4,CLASS_BIT+6,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,23},


/* 0100 0110 ssN0 dddd address_src *** andb rbd,address_src(rs) */
{
#ifdef NICENAMES
"andb rbd,address_src(rs)",8,10,
0x1c,
#endif
"andb",OPC_andb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+4,CLASS_BIT+6,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,24},


/* 0000 0110 0000 dddd imm8 imm8 *** andb rbd,imm8 */
{
#ifdef NICENAMES
"andb rbd,imm8",8,7,
0x1c,
#endif
"andb",OPC_andb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0,CLASS_BIT+6,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM8),CLASS_IMM+(ARG_IMM8),0,0,0,},2,4,25},


/* 1000 0110 ssss dddd *** andb rbd,rbs */
{
#ifdef NICENAMES
"andb rbd,rbs",8,4,
0x1c,
#endif
"andb",OPC_andb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+6,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,26},


/* 0010 0111 ddN0 imm4 *** bit @rd,imm4 */
{
#ifdef NICENAMES
"bit @rd,imm4",16,8,
0x10,
#endif
"bit",OPC_bit,0,{CLASS_IR+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+2,CLASS_BIT+7,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4),0,0,0,0,0,},2,2,27},


/* 0110 0111 ddN0 imm4 address_dst *** bit address_dst(rd),imm4 */
{
#ifdef NICENAMES
"bit address_dst(rd),imm4",16,11,
0x10,
#endif
"bit",OPC_bit,0,{CLASS_X+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+6,CLASS_BIT+7,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,28},


/* 0110 0111 0000 imm4 address_dst *** bit address_dst,imm4 */
{
#ifdef NICENAMES
"bit address_dst,imm4",16,10,
0x10,
#endif
"bit",OPC_bit,0,{CLASS_DA+(ARG_DST),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+6,CLASS_BIT+7,CLASS_BIT+0,CLASS_IMM+(ARG_IMM4),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,29},


/* 1010 0111 dddd imm4 *** bit rd,imm4 */
{
#ifdef NICENAMES
"bit rd,imm4",16,4,
0x10,
#endif
"bit",OPC_bit,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+0xa,CLASS_BIT+7,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM4),0,0,0,0,0,},2,2,30},


/* 0010 0111 0000 ssss 0000 dddd 0000 0000 *** bit rd,rs */
{
#ifdef NICENAMES
"bit rd,rs",16,10,
0x10,
#endif
"bit",OPC_bit,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+2,CLASS_BIT+7,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,31},


/* 0010 0110 ddN0 imm4 *** bitb @rd,imm4 */
{
#ifdef NICENAMES
"bitb @rd,imm4",8,8,
0x10,
#endif
"bitb",OPC_bitb,0,{CLASS_IR+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+2,CLASS_BIT+6,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4),0,0,0,0,0,},2,2,32},


/* 0110 0110 ddN0 imm4 address_dst *** bitb address_dst(rd),imm4 */
{
#ifdef NICENAMES
"bitb address_dst(rd),imm4",8,11,
0x10,
#endif
"bitb",OPC_bitb,0,{CLASS_X+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+6,CLASS_BIT+6,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,33},


/* 0110 0110 0000 imm4 address_dst *** bitb address_dst,imm4 */
{
#ifdef NICENAMES
"bitb address_dst,imm4",8,10,
0x10,
#endif
"bitb",OPC_bitb,0,{CLASS_DA+(ARG_DST),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+6,CLASS_BIT+6,CLASS_BIT+0,CLASS_IMM+(ARG_IMM4),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,34},


/* 1010 0110 dddd imm4 *** bitb rbd,imm4 */
{
#ifdef NICENAMES
"bitb rbd,imm4",8,4,
0x10,
#endif
"bitb",OPC_bitb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+0xa,CLASS_BIT+6,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM4),0,0,0,0,0,},2,2,35},


/* 0010 0110 0000 ssss 0000 dddd 0000 0000 *** bitb rbd,rs */
{
#ifdef NICENAMES
"bitb rbd,rs",8,10,
0x10,
#endif
"bitb",OPC_bitb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+2,CLASS_BIT+6,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,36},


/* 0011 0110 0000 0000 *** bpt */
{
#ifdef NICENAMES
"bpt",8,2,
0x00,
#endif
"bpt",OPC_bpt,0,{0},
	{CLASS_BIT+3,CLASS_BIT+6,CLASS_BIT+0,CLASS_BIT+0,0,0,0,0,0,},0,2,37},


/* 0001 1111 ddN0 0000 *** call @rd */
{
#ifdef NICENAMES
"call @rd",32,10,
0x00,
#endif
"call",OPC_call,0,{CLASS_IR+(ARG_RD),},
	{CLASS_BIT+1,CLASS_BIT+0xf,CLASS_REGN0+(ARG_RD),CLASS_BIT+0,0,0,0,0,0,},1,2,38},


/* 0101 1111 0000 0000 address_dst *** call address_dst */
{
#ifdef NICENAMES
"call address_dst",32,12,
0x00,
#endif
"call",OPC_call,0,{CLASS_DA+(ARG_DST),},
	{CLASS_BIT+5,CLASS_BIT+0xf,CLASS_BIT+0,CLASS_BIT+0,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,39},


/* 0101 1111 ddN0 0000 address_dst *** call address_dst(rd) */
{
#ifdef NICENAMES
"call address_dst(rd)",32,13,
0x00,
#endif
"call",OPC_call,0,{CLASS_X+(ARG_RD),},
	{CLASS_BIT+5,CLASS_BIT+0xf,CLASS_REGN0+(ARG_RD),CLASS_BIT+0,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,40},


/* 1101 disp12 *** calr disp12 */
{
#ifdef NICENAMES
"calr disp12",16,10,
0x00,
#endif
"calr",OPC_calr,0,{CLASS_DISP,},
	{CLASS_BIT+0xd,CLASS_DISP+(ARG_DISP12),0,0,0,0,0,0,0,},1,2,41},


/* 0000 1101 ddN0 1000 *** clr @rd */
{
#ifdef NICENAMES
"clr @rd",16,8,
0x00,
#endif
"clr",OPC_clr,0,{CLASS_IR+(ARG_RD),},
	{CLASS_BIT+0,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,0,0,0,0,},1,2,42},


/* 0100 1101 0000 1000 address_dst *** clr address_dst */
{
#ifdef NICENAMES
"clr address_dst",16,11,
0x00,
#endif
"clr",OPC_clr,0,{CLASS_DA+(ARG_DST),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_BIT+0,CLASS_BIT+8,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,43},


/* 0100 1101 ddN0 1000 address_dst *** clr address_dst(rd) */
{
#ifdef NICENAMES
"clr address_dst(rd)",16,12,
0x00,
#endif
"clr",OPC_clr,0,{CLASS_X+(ARG_RD),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+8,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,44},


/* 1000 1101 dddd 1000 *** clr rd */
{
#ifdef NICENAMES
"clr rd",16,7,
0x00,
#endif
"clr",OPC_clr,0,{CLASS_REG_WORD+(ARG_RD),},
	{CLASS_BIT+8,CLASS_BIT+0xd,CLASS_REG+(ARG_RD),CLASS_BIT+8,0,0,0,0,0,},1,2,45},


/* 0000 1100 ddN0 1000 *** clrb @rd */
{
#ifdef NICENAMES
"clrb @rd",8,8,
0x00,
#endif
"clrb",OPC_clrb,0,{CLASS_IR+(ARG_RD),},
	{CLASS_BIT+0,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,0,0,0,0,},1,2,46},


/* 0100 1100 0000 1000 address_dst *** clrb address_dst */
{
#ifdef NICENAMES
"clrb address_dst",8,11,
0x00,
#endif
"clrb",OPC_clrb,0,{CLASS_DA+(ARG_DST),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_BIT+0,CLASS_BIT+8,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,47},


/* 0100 1100 ddN0 1000 address_dst *** clrb address_dst(rd) */
{
#ifdef NICENAMES
"clrb address_dst(rd)",8,12,
0x00,
#endif
"clrb",OPC_clrb,0,{CLASS_X+(ARG_RD),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+8,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,48},


/* 1000 1100 dddd 1000 *** clrb rbd */
{
#ifdef NICENAMES
"clrb rbd",8,7,
0x00,
#endif
"clrb",OPC_clrb,0,{CLASS_REG_BYTE+(ARG_RD),},
	{CLASS_BIT+8,CLASS_BIT+0xc,CLASS_REG+(ARG_RD),CLASS_BIT+8,0,0,0,0,0,},1,2,49},


/* 0000 1101 ddN0 0000 *** com @rd */
{
#ifdef NICENAMES
"com @rd",16,12,
0x18,
#endif
"com",OPC_com,0,{CLASS_IR+(ARG_RD),},
	{CLASS_BIT+0,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+0,0,0,0,0,0,},1,2,50},


/* 0100 1101 0000 0000 address_dst *** com address_dst */
{
#ifdef NICENAMES
"com address_dst",16,15,
0x18,
#endif
"com",OPC_com,0,{CLASS_DA+(ARG_DST),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_BIT+0,CLASS_BIT+0,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,51},


/* 0100 1101 ddN0 0000 address_dst *** com address_dst(rd) */
{
#ifdef NICENAMES
"com address_dst(rd)",16,16,
0x18,
#endif
"com",OPC_com,0,{CLASS_X+(ARG_RD),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+0,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,52},


/* 1000 1101 dddd 0000 *** com rd */
{
#ifdef NICENAMES
"com rd",16,7,
0x18,
#endif
"com",OPC_com,0,{CLASS_REG_WORD+(ARG_RD),},
	{CLASS_BIT+8,CLASS_BIT+0xd,CLASS_REG+(ARG_RD),CLASS_BIT+0,0,0,0,0,0,},1,2,53},


/* 0000 1100 ddN0 0000 *** comb @rd */
{
#ifdef NICENAMES
"comb @rd",8,12,
0x1c,
#endif
"comb",OPC_comb,0,{CLASS_IR+(ARG_RD),},
	{CLASS_BIT+0,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+0,0,0,0,0,0,},1,2,54},


/* 0100 1100 0000 0000 address_dst *** comb address_dst */
{
#ifdef NICENAMES
"comb address_dst",8,15,
0x1c,
#endif
"comb",OPC_comb,0,{CLASS_DA+(ARG_DST),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_BIT+0,CLASS_BIT+0,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,55},


/* 0100 1100 ddN0 0000 address_dst *** comb address_dst(rd) */
{
#ifdef NICENAMES
"comb address_dst(rd)",8,16,
0x1c,
#endif
"comb",OPC_comb,0,{CLASS_X+(ARG_RD),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+0,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,56},


/* 1000 1100 dddd 0000 *** comb rbd */
{
#ifdef NICENAMES
"comb rbd",8,7,
0x1c,
#endif
"comb",OPC_comb,0,{CLASS_REG_BYTE+(ARG_RD),},
	{CLASS_BIT+8,CLASS_BIT+0xc,CLASS_REG+(ARG_RD),CLASS_BIT+0,0,0,0,0,0,},1,2,57},


/* 1000 1101 flags 0101 *** comflg flags */
{
#ifdef NICENAMES
"comflg flags",16,7,
0x3c,
#endif
"comflg",OPC_comflg,0,{CLASS_FLAGS,},
	{CLASS_BIT+8,CLASS_BIT+0xd,CLASS_FLAGS,CLASS_BIT+5,0,0,0,0,0,},1,2,58},


/* 0000 1101 ddN0 0001 imm16 *** cp @rd,imm16 */
{
#ifdef NICENAMES
"cp @rd,imm16",16,11,
0x3c,
#endif
"cp",OPC_cp,0,{CLASS_IR+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+0,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+1,CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,59},


/* 0100 1101 ddN0 0001 address_dst imm16 *** cp address_dst(rd),imm16 */
{
#ifdef NICENAMES
"cp address_dst(rd),imm16",16,15,
0x3c,
#endif
"cp",OPC_cp,0,{CLASS_X+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+1,CLASS_ADDRESS+(ARG_DST),CLASS_IMM+(ARG_IMM16),0,0,0,},2,6,60},


/* 0100 1101 0000 0001 address_dst imm16 *** cp address_dst,imm16 */
{
#ifdef NICENAMES
"cp address_dst,imm16",16,14,
0x3c,
#endif
"cp",OPC_cp,0,{CLASS_DA+(ARG_DST),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_BIT+0,CLASS_BIT+1,CLASS_ADDRESS+(ARG_DST),CLASS_IMM+(ARG_IMM16),0,0,0,},2,6,61},


/* 0000 1011 ssN0 dddd *** cp rd,@rs */
{
#ifdef NICENAMES
"cp rd,@rs",16,7,
0x3c,
#endif
"cp",OPC_cp,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+0,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,62},


/* 0100 1011 0000 dddd address_src *** cp rd,address_src */
{
#ifdef NICENAMES
"cp rd,address_src",16,9,
0x3c,
#endif
"cp",OPC_cp,0,{CLASS_REG_WORD+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+4,CLASS_BIT+0xb,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,63},


/* 0100 1011 ssN0 dddd address_src *** cp rd,address_src(rs) */
{
#ifdef NICENAMES
"cp rd,address_src(rs)",16,10,
0x3c,
#endif
"cp",OPC_cp,0,{CLASS_REG_WORD+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+4,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,64},


/* 0000 1011 0000 dddd imm16 *** cp rd,imm16 */
{
#ifdef NICENAMES
"cp rd,imm16",16,7,
0x3c,
#endif
"cp",OPC_cp,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+0,CLASS_BIT+0xb,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,65},


/* 1000 1011 ssss dddd *** cp rd,rs */
{
#ifdef NICENAMES
"cp rd,rs",16,4,
0x3c,
#endif
"cp",OPC_cp,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+0xb,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,66},


/* 0000 1100 ddN0 0001 imm8 imm8 *** cpb @rd,imm8 */
{
#ifdef NICENAMES
"cpb @rd,imm8",8,11,
0x3c,
#endif
"cpb",OPC_cpb,0,{CLASS_IR+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+1,CLASS_IMM+(ARG_IMM8),CLASS_IMM+(ARG_IMM8),0,0,0,},2,4,67},


/* 0100 1100 ddN0 0001 address_dst imm8 imm8 *** cpb address_dst(rd),imm8 */
{
#ifdef NICENAMES
"cpb address_dst(rd),imm8",8,15,
0x3c,
#endif
"cpb",OPC_cpb,0,{CLASS_X+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+1,CLASS_ADDRESS+(ARG_DST),CLASS_IMM+(ARG_IMM8),CLASS_IMM+(ARG_IMM8),0,0,},2,6,68},


/* 0100 1100 0000 0001 address_dst imm8 imm8 *** cpb address_dst,imm8 */
{
#ifdef NICENAMES
"cpb address_dst,imm8",8,14,
0x3c,
#endif
"cpb",OPC_cpb,0,{CLASS_DA+(ARG_DST),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_BIT+0,CLASS_BIT+1,CLASS_ADDRESS+(ARG_DST),CLASS_IMM+(ARG_IMM8),CLASS_IMM+(ARG_IMM8),0,0,},2,6,69},


/* 0000 1010 ssN0 dddd *** cpb rbd,@rs */
{
#ifdef NICENAMES
"cpb rbd,@rs",8,7,
0x3c,
#endif
"cpb",OPC_cpb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+0,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,70},


/* 0100 1010 0000 dddd address_src *** cpb rbd,address_src */
{
#ifdef NICENAMES
"cpb rbd,address_src",8,9,
0x3c,
#endif
"cpb",OPC_cpb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+4,CLASS_BIT+0xa,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,71},


/* 0100 1010 ssN0 dddd address_src *** cpb rbd,address_src(rs) */
{
#ifdef NICENAMES
"cpb rbd,address_src(rs)",8,10,
0x3c,
#endif
"cpb",OPC_cpb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+4,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,72},


/* 0000 1010 0000 dddd imm8 imm8 *** cpb rbd,imm8 */
{
#ifdef NICENAMES
"cpb rbd,imm8",8,7,
0x3c,
#endif
"cpb",OPC_cpb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0,CLASS_BIT+0xa,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM8),CLASS_IMM+(ARG_IMM8),0,0,0,},2,4,73},


/* 1000 1010 ssss dddd *** cpb rbd,rbs */
{
#ifdef NICENAMES
"cpb rbd,rbs",8,4,
0x3c,
#endif
"cpb",OPC_cpb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+0xa,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,74},


/* 1011 1011 ssN0 1000 0000 rrrr dddd cccc *** cpd rd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpd rd,@rs,rr,cc",16,11,
0x3c,
#endif
"cpd",OPC_cpd,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+8,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REG+(ARG_RD),CLASS_CC,0,},4,4,75},


/* 1011 1010 ssN0 1000 0000 rrrr dddd cccc *** cpdb rbd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpdb rbd,@rs,rr,cc",8,11,
0x3c,
#endif
"cpdb",OPC_cpdb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+8,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REG+(ARG_RD),CLASS_CC,0,},4,4,76},


/* 1011 1011 ssN0 1100 0000 rrrr dddd cccc *** cpdr rd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpdr rd,@rs,rr,cc",16,11,
0x3c,
#endif
"cpdr",OPC_cpdr,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+0xc,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REG+(ARG_RD),CLASS_CC,0,},4,4,77},


/* 1011 1010 ssN0 1100 0000 rrrr dddd cccc *** cpdrb rbd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpdrb rbd,@rs,rr,cc",8,11,
0x3c,
#endif
"cpdrb",OPC_cpdrb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+0xc,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REG+(ARG_RD),CLASS_CC,0,},4,4,78},


/* 1011 1011 ssN0 0000 0000 rrrr dddd cccc *** cpi rd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpi rd,@rs,rr,cc",16,11,
0x3c,
#endif
"cpi",OPC_cpi,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+0,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REG+(ARG_RD),CLASS_CC,0,},4,4,79},


/* 1011 1010 ssN0 0000 0000 rrrr dddd cccc *** cpib rbd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpib rbd,@rs,rr,cc",8,11,
0x3c,
#endif
"cpib",OPC_cpib,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+0,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REG+(ARG_RD),CLASS_CC,0,},4,4,80},


/* 1011 1011 ssN0 0100 0000 rrrr dddd cccc *** cpir rd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpir rd,@rs,rr,cc",16,11,
0x3c,
#endif
"cpir",OPC_cpir,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+4,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REG+(ARG_RD),CLASS_CC,0,},4,4,81},


/* 1011 1010 ssN0 0100 0000 rrrr dddd cccc *** cpirb rbd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpirb rbd,@rs,rr,cc",8,11,
0x3c,
#endif
"cpirb",OPC_cpirb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+4,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REG+(ARG_RD),CLASS_CC,0,},4,4,82},


/* 0001 0000 ssN0 dddd *** cpl rrd,@rs */
{
#ifdef NICENAMES
"cpl rrd,@rs",32,14,
0x3c,
#endif
"cpl",OPC_cpl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+1,CLASS_BIT+0,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,83},


/* 0101 0000 0000 dddd address_src *** cpl rrd,address_src */
{
#ifdef NICENAMES
"cpl rrd,address_src",32,15,
0x3c,
#endif
"cpl",OPC_cpl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+5,CLASS_BIT+0,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,84},


/* 0101 0000 ssN0 dddd address_src *** cpl rrd,address_src(rs) */
{
#ifdef NICENAMES
"cpl rrd,address_src(rs)",32,16,
0x3c,
#endif
"cpl",OPC_cpl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+0,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,85},


/* 0001 0000 0000 dddd imm32 *** cpl rrd,imm32 */
{
#ifdef NICENAMES
"cpl rrd,imm32",32,14,
0x3c,
#endif
"cpl",OPC_cpl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IMM+(ARG_IMM32),},
	{CLASS_BIT+1,CLASS_BIT+0,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM32),0,0,0,0,},2,6,86},


/* 1001 0000 ssss dddd *** cpl rrd,rrs */
{
#ifdef NICENAMES
"cpl rrd,rrs",32,8,
0x3c,
#endif
"cpl",OPC_cpl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_REG_LONG+(ARG_RS),},
	{CLASS_BIT+9,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,87},


/* 1011 1011 ssN0 1010 0000 rrrr ddN0 cccc *** cpsd @rd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpsd @rd,@rs,rr,cc",16,11,
0x3c,
#endif
"cpsd",OPC_cpsd,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+0xa,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_CC,0,},4,4,88},


/* 1011 1010 ssN0 1010 0000 rrrr ddN0 cccc *** cpsdb @rd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpsdb @rd,@rs,rr,cc",8,11,
0x3c,
#endif
"cpsdb",OPC_cpsdb,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+0xa,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_CC,0,},4,4,89},


/* 1011 1011 ssN0 1110 0000 rrrr ddN0 cccc *** cpsdr @rd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpsdr @rd,@rs,rr,cc",16,11,
0x3c,
#endif
"cpsdr",OPC_cpsdr,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+0xe,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_CC,0,},4,4,90},


/* 1011 1010 ssN0 1110 0000 rrrr ddN0 cccc *** cpsdrb @rd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpsdrb @rd,@rs,rr,cc",8,11,
0x3c,
#endif
"cpsdrb",OPC_cpsdrb,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+0xe,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_CC,0,},4,4,91},


/* 1011 1011 ssN0 0010 0000 rrrr ddN0 cccc *** cpsi @rd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpsi @rd,@rs,rr,cc",16,11,
0x3c,
#endif
"cpsi",OPC_cpsi,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+2,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_CC,0,},4,4,92},


/* 1011 1010 ssN0 0010 0000 rrrr ddN0 cccc *** cpsib @rd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpsib @rd,@rs,rr,cc",8,11,
0x3c,
#endif
"cpsib",OPC_cpsib,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+2,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_CC,0,},4,4,93},


/* 1011 1011 ssN0 0110 0000 rrrr ddN0 cccc *** cpsir @rd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpsir @rd,@rs,rr,cc",16,11,
0x3c,
#endif
"cpsir",OPC_cpsir,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+6,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_CC,0,},4,4,94},


/* 1011 1010 ssN0 0110 0000 rrrr ddN0 cccc *** cpsirb @rd,@rs,rr,cc */
{
#ifdef NICENAMES
"cpsirb @rd,@rs,rr,cc",8,11,
0x3c,
#endif
"cpsirb",OPC_cpsirb,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),CLASS_CC,},
	{CLASS_BIT+0xb,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+6,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_CC,0,},4,4,95},


/* 1011 0000 dddd 0000 *** dab rbd */
{
#ifdef NICENAMES
"dab rbd",8,5,
0x38,
#endif
"dab",OPC_dab,0,{CLASS_REG_BYTE+(ARG_RD),},
	{CLASS_BIT+0xb,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_BIT+0,0,0,0,0,0,},1,2,96},


/* 1111 dddd 0disp7 *** dbjnz rbd,disp7 */
{
#ifdef NICENAMES
"dbjnz rbd,disp7",16,11,
0x00,
#endif
"dbjnz",OPC_dbjnz,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_DISP,},
	{CLASS_BIT+0xf,CLASS_REG+(ARG_RD),CLASS_0DISP7,0,0,0,0,0,0,},2,2,97},


/* 0010 1011 ddN0 imm4m1 *** dec @rd,imm4m1 */
{
#ifdef NICENAMES
"dec @rd,imm4m1",16,11,
0x1c,
#endif
"dec",OPC_dec,0,{CLASS_IR+(ARG_RD),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+2,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4M1),0,0,0,0,0,},2,2,98},


/* 0110 1011 ddN0 imm4m1 address_dst *** dec address_dst(rd),imm4m1 */
{
#ifdef NICENAMES
"dec address_dst(rd),imm4m1",16,14,
0x1c,
#endif
"dec",OPC_dec,0,{CLASS_X+(ARG_RD),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+6,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4M1),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,99},


/* 0110 1011 0000 imm4m1 address_dst *** dec address_dst,imm4m1 */
{
#ifdef NICENAMES
"dec address_dst,imm4m1",16,13,
0x1c,
#endif
"dec",OPC_dec,0,{CLASS_DA+(ARG_DST),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+6,CLASS_BIT+0xb,CLASS_BIT+0,CLASS_IMM+(ARG_IMM4M1),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,100},


/* 1010 1011 dddd imm4m1 *** dec rd,imm4m1 */
{
#ifdef NICENAMES
"dec rd,imm4m1",16,4,
0x1c,
#endif
"dec",OPC_dec,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+0xa,CLASS_BIT+0xb,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM4M1),0,0,0,0,0,},2,2,101},


/* 0010 1010 ddN0 imm4m1 *** decb @rd,imm4m1 */
{
#ifdef NICENAMES
"decb @rd,imm4m1",8,11,
0x1c,
#endif
"decb",OPC_decb,0,{CLASS_IR+(ARG_RD),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+2,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4M1),0,0,0,0,0,},2,2,102},


/* 0110 1010 ddN0 imm4m1 address_dst *** decb address_dst(rd),imm4m1 */
{
#ifdef NICENAMES
"decb address_dst(rd),imm4m1",8,14,
0x1c,
#endif
"decb",OPC_decb,0,{CLASS_X+(ARG_RD),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+6,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4M1),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,103},


/* 0110 1010 0000 imm4m1 address_dst *** decb address_dst,imm4m1 */
{
#ifdef NICENAMES
"decb address_dst,imm4m1",8,13,
0x1c,
#endif
"decb",OPC_decb,0,{CLASS_DA+(ARG_DST),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+6,CLASS_BIT+0xa,CLASS_BIT+0,CLASS_IMM+(ARG_IMM4M1),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,104},


/* 1010 1010 dddd imm4m1 *** decb rbd,imm4m1 */
{
#ifdef NICENAMES
"decb rbd,imm4m1",8,4,
0x1c,
#endif
"decb",OPC_decb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+0xa,CLASS_BIT+0xa,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM4M1),0,0,0,0,0,},2,2,105},


/* 0111 1100 0000 00ii *** di i2 */
{
#ifdef NICENAMES
"di i2",16,7,
0x00,
#endif
"di",OPC_di,0,{CLASS_IMM+(ARG_IMM2),},
	{CLASS_BIT+7,CLASS_BIT+0xc,CLASS_BIT+0,CLASS_00II,0,0,0,0,0,},1,2,106},


/* 0001 1011 ssN0 dddd *** div rrd,@rs */
{
#ifdef NICENAMES
"div rrd,@rs",16,107,
0x3c,
#endif
"div",OPC_div,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+1,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,107},


/* 0101 1011 0000 dddd address_src *** div rrd,address_src */
{
#ifdef NICENAMES
"div rrd,address_src",16,107,
0x3c,
#endif
"div",OPC_div,0,{CLASS_REG_LONG+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+5,CLASS_BIT+0xb,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,108},


/* 0101 1011 ssN0 dddd address_src *** div rrd,address_src(rs) */
{
#ifdef NICENAMES
"div rrd,address_src(rs)",16,107,
0x3c,
#endif
"div",OPC_div,0,{CLASS_REG_LONG+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,109},


/* 0001 1011 0000 dddd imm16 *** div rrd,imm16 */
{
#ifdef NICENAMES
"div rrd,imm16",16,107,
0x3c,
#endif
"div",OPC_div,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+1,CLASS_BIT+0xb,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,110},


/* 1001 1011 ssss dddd *** div rrd,rs */
{
#ifdef NICENAMES
"div rrd,rs",16,107,
0x3c,
#endif
"div",OPC_div,0,{CLASS_REG_LONG+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+9,CLASS_BIT+0xb,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,111},


/* 0001 1010 ssN0 dddd *** divl rqd,@rs */
{
#ifdef NICENAMES
"divl rqd,@rs",32,744,
0x3c,
#endif
"divl",OPC_divl,0,{CLASS_REG_QUAD+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+1,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,112},


/* 0101 1010 0000 dddd address_src *** divl rqd,address_src */
{
#ifdef NICENAMES
"divl rqd,address_src",32,745,
0x3c,
#endif
"divl",OPC_divl,0,{CLASS_REG_QUAD+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+5,CLASS_BIT+0xa,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,113},


/* 0101 1010 ssN0 dddd address_src *** divl rqd,address_src(rs) */
{
#ifdef NICENAMES
"divl rqd,address_src(rs)",32,746,
0x3c,
#endif
"divl",OPC_divl,0,{CLASS_REG_QUAD+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,114},


/* 0001 1010 0000 dddd imm32 *** divl rqd,imm32 */
{
#ifdef NICENAMES
"divl rqd,imm32",32,744,
0x3c,
#endif
"divl",OPC_divl,0,{CLASS_REG_QUAD+(ARG_RD),CLASS_IMM+(ARG_IMM32),},
	{CLASS_BIT+1,CLASS_BIT+0xa,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM32),0,0,0,0,},2,6,115},


/* 1001 1010 ssss dddd *** divl rqd,rrs */
{
#ifdef NICENAMES
"divl rqd,rrs",32,744,
0x3c,
#endif
"divl",OPC_divl,0,{CLASS_REG_QUAD+(ARG_RD),CLASS_REG_LONG+(ARG_RS),},
	{CLASS_BIT+9,CLASS_BIT+0xa,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,116},


/* 1111 dddd 1disp7 *** djnz rd,disp7 */
{
#ifdef NICENAMES
"djnz rd,disp7",16,11,
0x00,
#endif
"djnz",OPC_djnz,0,{CLASS_REG_WORD+(ARG_RD),CLASS_DISP,},
	{CLASS_BIT+0xf,CLASS_REG+(ARG_RD),CLASS_1DISP7,0,0,0,0,0,0,},2,2,117},


/* 0111 1100 0000 01ii *** ei i2 */
{
#ifdef NICENAMES
"ei i2",16,7,
0x00,
#endif
"ei",OPC_ei,0,{CLASS_IMM+(ARG_IMM2),},
	{CLASS_BIT+7,CLASS_BIT+0xc,CLASS_BIT+0,CLASS_01II,0,0,0,0,0,},1,2,118},


/* 0010 1101 ssN0 dddd *** ex rd,@rs */
{
#ifdef NICENAMES
"ex rd,@rs",16,12,
0x00,
#endif
"ex",OPC_ex,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+2,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,119},


/* 0110 1101 0000 dddd address_src *** ex rd,address_src */
{
#ifdef NICENAMES
"ex rd,address_src",16,15,
0x00,
#endif
"ex",OPC_ex,0,{CLASS_REG_WORD+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+6,CLASS_BIT+0xd,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,120},


/* 0110 1101 ssN0 dddd address_src *** ex rd,address_src(rs) */
{
#ifdef NICENAMES
"ex rd,address_src(rs)",16,16,
0x00,
#endif
"ex",OPC_ex,0,{CLASS_REG_WORD+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+6,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,121},


/* 1010 1101 ssss dddd *** ex rd,rs */
{
#ifdef NICENAMES
"ex rd,rs",16,6,
0x00,
#endif
"ex",OPC_ex,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+0xa,CLASS_BIT+0xd,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,122},


/* 0010 1100 ssN0 dddd *** exb rbd,@rs */
{
#ifdef NICENAMES
"exb rbd,@rs",8,12,
0x00,
#endif
"exb",OPC_exb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+2,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,123},


/* 0110 1100 0000 dddd address_src *** exb rbd,address_src */
{
#ifdef NICENAMES
"exb rbd,address_src",8,15,
0x00,
#endif
"exb",OPC_exb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+6,CLASS_BIT+0xc,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,124},


/* 0110 1100 ssN0 dddd address_src *** exb rbd,address_src(rs) */
{
#ifdef NICENAMES
"exb rbd,address_src(rs)",8,16,
0x00,
#endif
"exb",OPC_exb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+6,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,125},


/* 1010 1100 ssss dddd *** exb rbd,rbs */
{
#ifdef NICENAMES
"exb rbd,rbs",8,6,
0x00,
#endif
"exb",OPC_exb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+0xa,CLASS_BIT+0xc,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,126},


/* 0000 1110 imm8 *** ext0e imm8 */
{
#ifdef NICENAMES
"ext0e imm8",8,10,
0x00,
#endif
"ext0e",OPC_ext0e,0,{CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0,CLASS_BIT+0xe,CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},1,2,127},


/* 0000 1111 imm8 *** ext0f imm8 */
{
#ifdef NICENAMES
"ext0f imm8",8,10,
0x00,
#endif
"ext0f",OPC_ext0f,0,{CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0,CLASS_BIT+0xf,CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},1,2,128},


/* 1000 1110 imm8 *** ext8e imm8 */
{
#ifdef NICENAMES
"ext8e imm8",8,10,
0x00,
#endif
"ext8e",OPC_ext8e,0,{CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+8,CLASS_BIT+0xe,CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},1,2,129},


/* 1000 1111 imm8 *** ext8f imm8 */
{
#ifdef NICENAMES
"ext8f imm8",8,10,
0x00,
#endif
"ext8f",OPC_ext8f,0,{CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+8,CLASS_BIT+0xf,CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},1,2,130},


/* 1011 0001 dddd 1010 *** exts rrd */
{
#ifdef NICENAMES
"exts rrd",16,11,
0x00,
#endif
"exts",OPC_exts,0,{CLASS_REG_LONG+(ARG_RD),},
	{CLASS_BIT+0xb,CLASS_BIT+1,CLASS_REG+(ARG_RD),CLASS_BIT+0xa,0,0,0,0,0,},1,2,131},


/* 1011 0001 dddd 0000 *** extsb rd */
{
#ifdef NICENAMES
"extsb rd",8,11,
0x00,
#endif
"extsb",OPC_extsb,0,{CLASS_REG_WORD+(ARG_RD),},
	{CLASS_BIT+0xb,CLASS_BIT+1,CLASS_REG+(ARG_RD),CLASS_BIT+0,0,0,0,0,0,},1,2,132},


/* 1011 0001 dddd 0111 *** extsl rqd */
{
#ifdef NICENAMES
"extsl rqd",32,11,
0x00,
#endif
"extsl",OPC_extsl,0,{CLASS_REG_QUAD+(ARG_RD),},
	{CLASS_BIT+0xb,CLASS_BIT+1,CLASS_REG+(ARG_RD),CLASS_BIT+7,0,0,0,0,0,},1,2,133},


/* 0111 1010 0000 0000 *** halt */
{
#ifdef NICENAMES
"halt",16,8,
0x00,
#endif
"halt",OPC_halt,0,{0},
	{CLASS_BIT+7,CLASS_BIT+0xa,CLASS_BIT+0,CLASS_BIT+0,0,0,0,0,0,},0,2,134},


/* 0011 1101 ssN0 dddd *** in rd,@rs */
{
#ifdef NICENAMES
"in rd,@rs",16,10,
0x00,
#endif
"in",OPC_in,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,135},


/* 0011 1101 dddd 0100 imm16 *** in rd,imm16 */
{
#ifdef NICENAMES
"in rd,imm16",16,12,
0x00,
#endif
"in",OPC_in,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+3,CLASS_BIT+0xd,CLASS_REG+(ARG_RD),CLASS_BIT+4,CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,136},


/* 0011 1100 ssN0 dddd *** inb rbd,@rs */
{
#ifdef NICENAMES
"inb rbd,@rs",8,12,
0x00,
#endif
"inb",OPC_inb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,137},


/* 0011 1010 dddd 0100 imm16 *** inb rbd,imm16 */
{
#ifdef NICENAMES
"inb rbd,imm16",8,10,
0x00,
#endif
"inb",OPC_inb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REG+(ARG_RD),CLASS_BIT+4,CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,138},


/* 0010 1001 ddN0 imm4m1 *** inc @rd,imm4m1 */
{
#ifdef NICENAMES
"inc @rd,imm4m1",16,11,
0x1c,
#endif
"inc",OPC_inc,0,{CLASS_IR+(ARG_RD),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+2,CLASS_BIT+9,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4M1),0,0,0,0,0,},2,2,139},


/* 0110 1001 ddN0 imm4m1 address_dst *** inc address_dst(rd),imm4m1 */
{
#ifdef NICENAMES
"inc address_dst(rd),imm4m1",16,14,
0x1c,
#endif
"inc",OPC_inc,0,{CLASS_X+(ARG_RD),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+6,CLASS_BIT+9,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4M1),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,140},


/* 0110 1001 0000 imm4m1 address_dst *** inc address_dst,imm4m1 */
{
#ifdef NICENAMES
"inc address_dst,imm4m1",16,13,
0x1c,
#endif
"inc",OPC_inc,0,{CLASS_DA+(ARG_DST),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+6,CLASS_BIT+9,CLASS_BIT+0,CLASS_IMM+(ARG_IMM4M1),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,141},


/* 1010 1001 dddd imm4m1 *** inc rd,imm4m1 */
{
#ifdef NICENAMES
"inc rd,imm4m1",16,4,
0x1c,
#endif
"inc",OPC_inc,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+0xa,CLASS_BIT+9,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM4M1),0,0,0,0,0,},2,2,142},


/* 0010 1000 ddN0 imm4m1 *** incb @rd,imm4m1 */
{
#ifdef NICENAMES
"incb @rd,imm4m1",8,11,
0x1c,
#endif
"incb",OPC_incb,0,{CLASS_IR+(ARG_RD),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+2,CLASS_BIT+8,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4M1),0,0,0,0,0,},2,2,143},


/* 0110 1000 ddN0 imm4m1 address_dst *** incb address_dst(rd),imm4m1 */
{
#ifdef NICENAMES
"incb address_dst(rd),imm4m1",8,14,
0x1c,
#endif
"incb",OPC_incb,0,{CLASS_X+(ARG_RD),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+6,CLASS_BIT+8,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4M1),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,144},


/* 0110 1000 0000 imm4m1 address_dst *** incb address_dst,imm4m1 */
{
#ifdef NICENAMES
"incb address_dst,imm4m1",8,13,
0x1c,
#endif
"incb",OPC_incb,0,{CLASS_DA+(ARG_DST),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+6,CLASS_BIT+8,CLASS_BIT+0,CLASS_IMM+(ARG_IMM4M1),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,145},


/* 1010 1000 dddd imm4m1 *** incb rbd,imm4m1 */
{
#ifdef NICENAMES
"incb rbd,imm4m1",8,4,
0x1c,
#endif
"incb",OPC_incb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM +(ARG_IMM4M1),},
	{CLASS_BIT+0xa,CLASS_BIT+8,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM4M1),0,0,0,0,0,},2,2,146},


/* 0011 1011 ssN0 1000 0000 aaaa ddN0 1000 *** ind @rd,@rs,ra */
{
#ifdef NICENAMES
"ind @rd,@rs,ra",16,21,
0x04,
#endif
"ind",OPC_ind,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+8,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,147},


/* 0011 1010 ssN0 1000 0000 aaaa ddN0 1000 *** indb @rd,@rs,rba */
{
#ifdef NICENAMES
"indb @rd,@rs,rba",8,21,
0x04,
#endif
"indb",OPC_indb,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_BYTE+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+8,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,148},


/* 0011 1010 ssN0 0000 0000 aaaa ddN0 1000 *** inib @rd,@rs,ra */
{
#ifdef NICENAMES
"inib @rd,@rs,ra",8,21,
0x04,
#endif
"inib",OPC_inib,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+0,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,149},


/* 0011 1010 ssN0 0000 0000 aaaa ddN0 0000 *** inibr @rd,@rs,ra */
{
#ifdef NICENAMES
"inibr @rd,@rs,ra",16,21,
0x04,
#endif
"inibr",OPC_inibr,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+0,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+0,0,},3,4,150},


/* 0111 1011 0000 0000 *** iret */
{
#ifdef NICENAMES
"iret",16,13,
0x3f,
#endif
"iret",OPC_iret,0,{0},
	{CLASS_BIT+7,CLASS_BIT+0xb,CLASS_BIT+0,CLASS_BIT+0,0,0,0,0,0,},0,2,151},


/* 0001 1110 ddN0 cccc *** jp cc,@rd */
{
#ifdef NICENAMES
"jp cc,@rd",16,10,
0x00,
#endif
"jp",OPC_jp,0,{CLASS_CC,CLASS_IR+(ARG_RD),},
	{CLASS_BIT+1,CLASS_BIT+0xe,CLASS_REGN0+(ARG_RD),CLASS_CC,0,0,0,0,0,},2,2,152},


/* 0101 1110 0000 cccc address_dst *** jp cc,address_dst */
{
#ifdef NICENAMES
"jp cc,address_dst",16,7,
0x00,
#endif
"jp",OPC_jp,0,{CLASS_CC,CLASS_DA+(ARG_DST),},
	{CLASS_BIT+5,CLASS_BIT+0xe,CLASS_BIT+0,CLASS_CC,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,153},


/* 0101 1110 ddN0 cccc address_dst *** jp cc,address_dst(rd) */
{
#ifdef NICENAMES
"jp cc,address_dst(rd)",16,8,
0x00,
#endif
"jp",OPC_jp,0,{CLASS_CC,CLASS_X+(ARG_RD),},
	{CLASS_BIT+5,CLASS_BIT+0xe,CLASS_REGN0+(ARG_RD),CLASS_CC,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,154},


/* 1110 cccc disp8 *** jr cc,disp8 */
{
#ifdef NICENAMES
"jr cc,disp8",16,6,
0x00,
#endif
"jr",OPC_jr,0,{CLASS_CC,CLASS_DISP,},
	{CLASS_BIT+0xe,CLASS_CC,CLASS_DISP8,0,0,0,0,0,0,},2,2,155},


/* 0000 1101 ddN0 0101 imm16 *** ld @rd,imm16 */
{
#ifdef NICENAMES
"ld @rd,imm16",16,7,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_IR+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+0,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+5,CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,156},


/* 0010 1111 ddN0 ssss *** ld @rd,rs */
{
#ifdef NICENAMES
"ld @rd,rs",16,8,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_IR+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+2,CLASS_BIT+0xf,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),0,0,0,0,0,},2,2,157},


/* 0100 1101 ddN0 0101 address_dst imm16 *** ld address_dst(rd),imm16 */
{
#ifdef NICENAMES
"ld address_dst(rd),imm16",16,15,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_X+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+5,CLASS_ADDRESS+(ARG_DST),CLASS_IMM+(ARG_IMM16),0,0,0,},2,6,158},


/* 0110 1111 ddN0 ssss address_dst *** ld address_dst(rd),rs */
{
#ifdef NICENAMES
"ld address_dst(rd),rs",16,12,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_X+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+6,CLASS_BIT+0xf,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,159},


/* 0100 1101 0000 0101 address_dst imm16 *** ld address_dst,imm16 */
{
#ifdef NICENAMES
"ld address_dst,imm16",16,14,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_DA+(ARG_DST),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_BIT+0,CLASS_BIT+5,CLASS_ADDRESS+(ARG_DST),CLASS_IMM+(ARG_IMM16),0,0,0,},2,6,160},


/* 0110 1111 0000 ssss address_dst *** ld address_dst,rs */
{
#ifdef NICENAMES
"ld address_dst,rs",16,11,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_DA+(ARG_DST),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+6,CLASS_BIT+0xf,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,161},


/* 0011 0011 ddN0 ssss imm16 *** ld rd(imm16),rs */
{
#ifdef NICENAMES
"ld rd(imm16),rs",16,14,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_BA+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+3,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,162},


/* 0111 0011 ddN0 ssss 0000 xxxx 0000 0000 *** ld rd(rx),rs */
{
#ifdef NICENAMES
"ld rd(rx),rs",16,14,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_BX+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+7,CLASS_BIT+3,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_REG+(ARG_RX),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,163},


/* 0010 0001 ssN0 dddd *** ld rd,@rs */
{
#ifdef NICENAMES
"ld rd,@rs",16,7,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+2,CLASS_BIT+1,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,164},


/* 0110 0001 0000 dddd address_src *** ld rd,address_src */
{
#ifdef NICENAMES
"ld rd,address_src",16,9,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_REG_WORD+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+6,CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,165},


/* 0110 0001 ssN0 dddd address_src *** ld rd,address_src(rs) */
{
#ifdef NICENAMES
"ld rd,address_src(rs)",16,10,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_REG_WORD+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+6,CLASS_BIT+1,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,166},


/* 0010 0001 0000 dddd imm16 *** ld rd,imm16 */
{
#ifdef NICENAMES
"ld rd,imm16",16,7,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+2,CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,167},


/* 1010 0001 ssss dddd *** ld rd,rs */
{
#ifdef NICENAMES
"ld rd,rs",16,3,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+0xa,CLASS_BIT+1,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,168},


/* 0011 0001 ssN0 dddd imm16 *** ld rd,rs(imm16) */
{
#ifdef NICENAMES
"ld rd,rs(imm16)",16,14,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_REG_WORD+(ARG_RD),CLASS_BA+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+1,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,169},


/* 0111 0001 ssN0 dddd 0000 xxxx 0000 0000 *** ld rd,rs(rx) */
{
#ifdef NICENAMES
"ld rd,rs(rx)",16,14,
0x00,
#endif
"ld",OPC_ld,0,{CLASS_REG_WORD+(ARG_RD),CLASS_BX+(ARG_RS),},
	{CLASS_BIT+7,CLASS_BIT+1,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_BIT+0,CLASS_REG+(ARG_RX),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,170},


/* 0111 0110 0000 dddd address_src *** lda prd,address_src */
{
#ifdef NICENAMES
"lda prd,address_src",16,12,
0x00,
#endif
"lda",OPC_lda,0,{CLASS_PR+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+7,CLASS_BIT+6,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,171},


/* 0111 0110 ssN0 dddd address_src *** lda prd,address_src(rs) */
{
#ifdef NICENAMES
"lda prd,address_src(rs)",16,13,
0x00,
#endif
"lda",OPC_lda,0,{CLASS_PR+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+7,CLASS_BIT+6,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,172},


/* 0011 0100 ssN0 dddd imm16 *** lda prd,rs(imm16) */
{
#ifdef NICENAMES
"lda prd,rs(imm16)",16,15,
0x00,
#endif
"lda",OPC_lda,0,{CLASS_PR+(ARG_RD),CLASS_BA+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+4,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,173},


/* 0111 0100 ssN0 dddd 0000 xxxx 0000 0000 *** lda prd,rs(rx) */
{
#ifdef NICENAMES
"lda prd,rs(rx)",16,15,
0x00,
#endif
"lda",OPC_lda,0,{CLASS_PR+(ARG_RD),CLASS_BX+(ARG_RS),},
	{CLASS_BIT+7,CLASS_BIT+4,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_BIT+0,CLASS_REG+(ARG_RX),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,174},


/* 0011 0100 0000 dddd disp16 *** ldar prd,disp16 */
{
#ifdef NICENAMES
"ldar prd,disp16",16,15,
0x00,
#endif
"ldar",OPC_ldar,0,{CLASS_PR+(ARG_RD),CLASS_DISP,},
	{CLASS_BIT+3,CLASS_BIT+4,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_DISP+(ARG_DISP16),0,0,0,0,},2,4,175},


/* 0000 1100 ddN0 0101 imm8 imm8 *** ldb @rd,imm8 */
{
#ifdef NICENAMES
"ldb @rd,imm8",8,7,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_IR+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+5,CLASS_IMM+(ARG_IMM8),CLASS_IMM+(ARG_IMM8),0,0,0,},2,4,176},


/* 0010 1110 ddN0 ssss *** ldb @rd,rbs */
{
#ifdef NICENAMES
"ldb @rd,rbs",8,8,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_IR+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+2,CLASS_BIT+0xe,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),0,0,0,0,0,},2,2,177},


/* 0100 1100 ddN0 0101 address_dst imm8 imm8 *** ldb address_dst(rd),imm8 */
{
#ifdef NICENAMES
"ldb address_dst(rd),imm8",8,15,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_X+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+5,CLASS_ADDRESS+(ARG_DST),CLASS_IMM+(ARG_IMM8),CLASS_IMM+(ARG_IMM8),0,0,},2,6,178},


/* 0110 1110 ddN0 ssss address_dst *** ldb address_dst(rd),rbs */
{
#ifdef NICENAMES
"ldb address_dst(rd),rbs",8,12,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_X+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+6,CLASS_BIT+0xe,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,179},


/* 0100 1100 0000 0101 address_dst imm8 imm8 *** ldb address_dst,imm8 */
{
#ifdef NICENAMES
"ldb address_dst,imm8",8,14,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_DA+(ARG_DST),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_BIT+0,CLASS_BIT+5,CLASS_ADDRESS+(ARG_DST),CLASS_IMM+(ARG_IMM8),CLASS_IMM+(ARG_IMM8),0,0,},2,6,180},


/* 0110 1110 0000 ssss address_dst *** ldb address_dst,rbs */
{
#ifdef NICENAMES
"ldb address_dst,rbs",8,11,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_DA+(ARG_DST),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+6,CLASS_BIT+0xe,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,181},


/* 0010 0000 ssN0 dddd *** ldb rbd,@rs */
{
#ifdef NICENAMES
"ldb rbd,@rs",8,7,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+2,CLASS_BIT+0,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,182},


/* 0110 0000 0000 dddd address_src *** ldb rbd,address_src */
{
#ifdef NICENAMES
"ldb rbd,address_src",8,9,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+6,CLASS_BIT+0,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,183},


/* 0110 0000 ssN0 dddd address_src *** ldb rbd,address_src(rs) */
{
#ifdef NICENAMES
"ldb rbd,address_src(rs)",8,10,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+6,CLASS_BIT+0,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,184},


/* 1100 dddd imm8 *** ldb rbd,imm8 */
{
#ifdef NICENAMES
"ldb rbd,imm8",8,5,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0xc,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},2,2,185},


/* 1010 0000 ssss dddd *** ldb rbd,rbs */
{
#ifdef NICENAMES
"ldb rbd,rbs",8,3,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+0xa,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,186},


/* 0011 0000 ssN0 dddd imm16 *** ldb rbd,rs(imm16) */
{
#ifdef NICENAMES
"ldb rbd,rs(imm16)",8,14,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_BA+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+0,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,187},


/* 0111 0000 ssN0 dddd 0000 xxxx 0000 0000 *** ldb rbd,rs(rx) */
{
#ifdef NICENAMES
"ldb rbd,rs(rx)",8,14,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_BX+(ARG_RS),},
	{CLASS_BIT+7,CLASS_BIT+0,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_BIT+0,CLASS_REG+(ARG_RX),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,188},


/* 0011 0010 ddN0 ssss imm16 *** ldb rd(imm16),rbs */
{
#ifdef NICENAMES
"ldb rd(imm16),rbs",8,14,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_BA+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+2,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,189},


/* 0111 0010 ddN0 ssss 0000 xxxx 0000 0000 *** ldb rd(rx),rbs */
{
#ifdef NICENAMES
"ldb rd(rx),rbs",8,14,
0x00,
#endif
"ldb",OPC_ldb,0,{CLASS_BX+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+7,CLASS_BIT+2,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_REG+(ARG_RX),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,190},


/* 0111 1101 ssss 1ccc *** ldctl ctrl,rs */
{
#ifdef NICENAMES
"ldctl ctrl,rs",32,7,
0x00,
#endif
"ldctl",OPC_ldctl,0,{CLASS_CTRL,CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+7,CLASS_BIT+0xd,CLASS_REG+(ARG_RS),CLASS_1CCC,0,0,0,0,0,},2,2,191},


/* 0111 1101 dddd 0ccc *** ldctl rd,ctrl */
{
#ifdef NICENAMES
"ldctl rd,ctrl",32,7,
0x00,
#endif
"ldctl",OPC_ldctl,0,{CLASS_REG_WORD+(ARG_RD),CLASS_CTRL,},
	{CLASS_BIT+7,CLASS_BIT+0xd,CLASS_REG+(ARG_RD),CLASS_0CCC,0,0,0,0,0,},2,2,192},


/* 1000 1100 ssss 1001 *** ldctlb ctrl,rbs */
{
#ifdef NICENAMES
"ldctlb ctrl,rbs",32,7,
0x3f,
#endif
"ldctlb",OPC_ldctlb,0,{CLASS_CTRL,CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+0xc,CLASS_REG+(ARG_RS),CLASS_BIT+9,0,0,0,0,0,},2,2,193},


/* 1000 1100 dddd 0001 *** ldctlb rbd,ctrl */
{
#ifdef NICENAMES
"ldctlb rbd,ctrl",32,7,
0x00,
#endif
"ldctlb",OPC_ldctlb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_CTRL,},
	{CLASS_BIT+8,CLASS_BIT+0xc,CLASS_REG+(ARG_RD),CLASS_BIT+1,0,0,0,0,0,},2,2,194},


/* 1011 1011 ssN0 1001 0000 rrrr ddN0 1000 *** ldd @rd,@rs,rr */
{
#ifdef NICENAMES
"ldd @rd,@rs,rr",16,11,
0x04,
#endif
"ldd",OPC_ldd,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+9,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,195},


/* 1011 1010 ssN0 1001 0000 rrrr ddN0 1000 *** lddb @rd,@rs,rr */
{
#ifdef NICENAMES
"lddb @rd,@rs,rr",8,11,
0x04,
#endif
"lddb",OPC_lddb,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+9,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,196},


/* 1011 1011 ssN0 1001 0000 rrrr ddN0 0000 *** lddr @rd,@rs,rr */
{
#ifdef NICENAMES
"lddr @rd,@rs,rr",16,11,
0x04,
#endif
"lddr",OPC_lddr,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+9,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_BIT+0,0,},3,4,197},


/* 1011 1010 ssN0 1001 0000 rrrr ddN0 0000 *** lddrb @rd,@rs,rr */
{
#ifdef NICENAMES
"lddrb @rd,@rs,rr",8,11,
0x04,
#endif
"lddrb",OPC_lddrb,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+9,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_BIT+0,0,},3,4,198},


/* 1011 1011 ssN0 0001 0000 rrrr ddN0 1000 *** ldi @rd,@rs,rr */
{
#ifdef NICENAMES
"ldi @rd,@rs,rr",16,11,
0x04,
#endif
"ldi",OPC_ldi,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,199},


/* 1011 1010 ssN0 0001 0000 rrrr ddN0 1000 *** ldib @rd,@rs,rr */
{
#ifdef NICENAMES
"ldib @rd,@rs,rr",8,11,
0x04,
#endif
"ldib",OPC_ldib,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,200},


/* 1011 1011 ssN0 0001 0000 rrrr ddN0 0000 *** ldir @rd,@rs,rr */
{
#ifdef NICENAMES
"ldir @rd,@rs,rr",16,11,
0x04,
#endif
"ldir",OPC_ldir,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_BIT+0,0,},3,4,201},


/* 1011 1010 ssN0 0001 0000 rrrr ddN0 0000 *** ldirb @rd,@rs,rr */
{
#ifdef NICENAMES
"ldirb @rd,@rs,rr",8,11,
0x04,
#endif
"ldirb",OPC_ldirb,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RD),CLASS_BIT+0,0,},3,4,202},


/* 1011 1101 dddd imm4 *** ldk rd,imm4 */
{
#ifdef NICENAMES
"ldk rd,imm4",16,5,
0x00,
#endif
"ldk",OPC_ldk,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+0xb,CLASS_BIT+0xd,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM4),0,0,0,0,0,},2,2,203},


/* 0001 1101 ddN0 ssss *** ldl @rd,rrs */
{
#ifdef NICENAMES
"ldl @rd,rrs",32,11,
0x00,
#endif
"ldl",OPC_ldl,0,{CLASS_IR+(ARG_RD),CLASS_REG_LONG+(ARG_RS),},
	{CLASS_BIT+1,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),0,0,0,0,0,},2,2,204},


/* 0101 1101 ddN0 ssss address_dst *** ldl address_dst(rd),rrs */
{
#ifdef NICENAMES
"ldl address_dst(rd),rrs",32,14,
0x00,
#endif
"ldl",OPC_ldl,0,{CLASS_X+(ARG_RD),CLASS_REG_LONG+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,205},


/* 0101 1101 0000 ssss address_dst *** ldl address_dst,rrs */
{
#ifdef NICENAMES
"ldl address_dst,rrs",32,15,
0x00,
#endif
"ldl",OPC_ldl,0,{CLASS_DA+(ARG_DST),CLASS_REG_LONG+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+0xd,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,206},


/* 0011 0111 ddN0 ssss imm16 *** ldl rd(imm16),rrs */
{
#ifdef NICENAMES
"ldl rd(imm16),rrs",32,17,
0x00,
#endif
"ldl",OPC_ldl,0,{CLASS_BA+(ARG_RD),CLASS_REG_LONG+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+7,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,207},


/* 0111 0111 ddN0 ssss 0000 xxxx 0000 0000 *** ldl rd(rx),rrs */
{
#ifdef NICENAMES
"ldl rd(rx),rrs",32,17,
0x00,
#endif
"ldl",OPC_ldl,0,{CLASS_BX+(ARG_RD),CLASS_REG_LONG+(ARG_RS),},
	{CLASS_BIT+7,CLASS_BIT+7,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_REG+(ARG_RX),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,208},


/* 0001 0100 ssN0 dddd *** ldl rrd,@rs */
{
#ifdef NICENAMES
"ldl rrd,@rs",32,11,
0x00,
#endif
"ldl",OPC_ldl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+1,CLASS_BIT+4,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,209},


/* 0101 0100 0000 dddd address_src *** ldl rrd,address_src */
{
#ifdef NICENAMES
"ldl rrd,address_src",32,12,
0x00,
#endif
"ldl",OPC_ldl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+5,CLASS_BIT+4,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,210},


/* 0101 0100 ssN0 dddd address_src *** ldl rrd,address_src(rs) */
{
#ifdef NICENAMES
"ldl rrd,address_src(rs)",32,13,
0x00,
#endif
"ldl",OPC_ldl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+4,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,211},


/* 0001 0100 0000 dddd imm32 *** ldl rrd,imm32 */
{
#ifdef NICENAMES
"ldl rrd,imm32",32,11,
0x00,
#endif
"ldl",OPC_ldl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IMM+(ARG_IMM32),},
	{CLASS_BIT+1,CLASS_BIT+4,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM32),0,0,0,0,},2,6,212},


/* 1001 0100 ssss dddd *** ldl rrd,rrs */
{
#ifdef NICENAMES
"ldl rrd,rrs",32,5,
0x00,
#endif
"ldl",OPC_ldl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_REG_LONG+(ARG_RS),},
	{CLASS_BIT+9,CLASS_BIT+4,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,213},


/* 0011 0101 ssN0 dddd imm16 *** ldl rrd,rs(imm16) */
{
#ifdef NICENAMES
"ldl rrd,rs(imm16)",32,17,
0x00,
#endif
"ldl",OPC_ldl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_BA+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+5,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,214},


/* 0111 0101 ssN0 dddd 0000 xxxx 0000 0000 *** ldl rrd,rs(rx) */
{
#ifdef NICENAMES
"ldl rrd,rs(rx)",32,17,
0x00,
#endif
"ldl",OPC_ldl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_BX+(ARG_RS),},
	{CLASS_BIT+7,CLASS_BIT+5,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_BIT+0,CLASS_REG+(ARG_RX),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,215},


/* 0001 1100 ddN0 1001 0000 ssss 0000 nminus1 *** ldm @rd,rs,n */
{
#ifdef NICENAMES
"ldm @rd,rs,n",16,11,
0x00,
#endif
"ldm",OPC_ldm,0,{CLASS_IR+(ARG_RD),CLASS_REG_WORD+(ARG_RS),CLASS_IMM + (ARG_IMMN),},
	{CLASS_BIT+1,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+9,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_IMM+(ARG_IMMNMINUS1),0,},3,4,216},


/* 0101 1100 ddN0 1001 0000 ssss 0000 nminus1 address_dst *** ldm address_dst(rd),rs,n */
{
#ifdef NICENAMES
"ldm address_dst(rd),rs,n",16,15,
0x00,
#endif
"ldm",OPC_ldm,0,{CLASS_X+(ARG_RD),CLASS_REG_WORD+(ARG_RS),CLASS_IMM + (ARG_IMMN),},
	{CLASS_BIT+5,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+9,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_IMM+(ARG_IMMNMINUS1),CLASS_ADDRESS+(ARG_DST),},3,6,217},


/* 0101 1100 0000 1001 0000 ssss 0000 nminus1 address_dst *** ldm address_dst,rs,n */
{
#ifdef NICENAMES
"ldm address_dst,rs,n",16,14,
0x00,
#endif
"ldm",OPC_ldm,0,{CLASS_DA+(ARG_DST),CLASS_REG_WORD+(ARG_RS),CLASS_IMM + (ARG_IMMN),},
	{CLASS_BIT+5,CLASS_BIT+0xc,CLASS_BIT+0,CLASS_BIT+9,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_IMM+(ARG_IMMNMINUS1),CLASS_ADDRESS+(ARG_DST),},3,6,218},


/* 0001 1100 ssN0 0001 0000 dddd 0000 nminus1 *** ldm rd,@rs,n */
{
#ifdef NICENAMES
"ldm rd,@rs,n",16,11,
0x00,
#endif
"ldm",OPC_ldm,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_IMM + (ARG_IMMN),},
	{CLASS_BIT+1,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RS),CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_BIT+0,CLASS_IMM+(ARG_IMMNMINUS1),0,},3,4,219},


/* 0101 1100 ssN0 0001 0000 dddd 0000 nminus1 address_src *** ldm rd,address_src(rs),n */
{
#ifdef NICENAMES
"ldm rd,address_src(rs),n",16,15,
0x00,
#endif
"ldm",OPC_ldm,0,{CLASS_REG_WORD+(ARG_RD),CLASS_X+(ARG_RS),CLASS_IMM + (ARG_IMMN),},
	{CLASS_BIT+5,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RS),CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_BIT+0,CLASS_IMM+(ARG_IMMNMINUS1),CLASS_ADDRESS+(ARG_SRC),},3,6,220},


/* 0101 1100 0000 0001 0000 dddd 0000 nminus1 address_src *** ldm rd,address_src,n */
{
#ifdef NICENAMES
"ldm rd,address_src,n",16,14,
0x00,
#endif
"ldm",OPC_ldm,0,{CLASS_REG_WORD+(ARG_RD),CLASS_DA+(ARG_SRC),CLASS_IMM + (ARG_IMMN),},
	{CLASS_BIT+5,CLASS_BIT+0xc,CLASS_BIT+0,CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_BIT+0,CLASS_IMM+(ARG_IMMNMINUS1),CLASS_ADDRESS+(ARG_SRC),},3,6,221},


/* 0011 1001 ssN0 0000 *** ldps @rs */
{
#ifdef NICENAMES
"ldps @rs",16,12,
0x3f,
#endif
"ldps",OPC_ldps,0,{CLASS_IR+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+9,CLASS_REGN0+(ARG_RS),CLASS_BIT+0,0,0,0,0,0,},1,2,222},


/* 0111 1001 0000 0000 address_src *** ldps address_src */
{
#ifdef NICENAMES
"ldps address_src",16,16,
0x3f,
#endif
"ldps",OPC_ldps,0,{CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+7,CLASS_BIT+9,CLASS_BIT+0,CLASS_BIT+0,CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},1,4,223},


/* 0111 1001 ssN0 0000 address_src *** ldps address_src(rs) */
{
#ifdef NICENAMES
"ldps address_src(rs)",16,17,
0x3f,
#endif
"ldps",OPC_ldps,0,{CLASS_X+(ARG_RS),},
	{CLASS_BIT+7,CLASS_BIT+9,CLASS_REGN0+(ARG_RS),CLASS_BIT+0,CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},1,4,224},


/* 0011 0011 0000 ssss disp16 *** ldr disp16,rs */
{
#ifdef NICENAMES
"ldr disp16,rs",16,14,
0x00,
#endif
"ldr",OPC_ldr,0,{CLASS_DISP,CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+3,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_DISP+(ARG_DISP16),0,0,0,0,},2,4,225},


/* 0011 0001 0000 dddd disp16 *** ldr rd,disp16 */
{
#ifdef NICENAMES
"ldr rd,disp16",16,14,
0x00,
#endif
"ldr",OPC_ldr,0,{CLASS_REG_WORD+(ARG_RD),CLASS_DISP,},
	{CLASS_BIT+3,CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_DISP+(ARG_DISP16),0,0,0,0,},2,4,226},


/* 0011 0010 0000 ssss disp16 *** ldrb disp16,rbs */
{
#ifdef NICENAMES
"ldrb disp16,rbs",8,14,
0x00,
#endif
"ldrb",OPC_ldrb,0,{CLASS_DISP,CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+2,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_DISP+(ARG_DISP16),0,0,0,0,},2,4,227},


/* 0011 0000 0000 dddd disp16 *** ldrb rbd,disp16 */
{
#ifdef NICENAMES
"ldrb rbd,disp16",8,14,
0x00,
#endif
"ldrb",OPC_ldrb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_DISP,},
	{CLASS_BIT+3,CLASS_BIT+0,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_DISP+(ARG_DISP16),0,0,0,0,},2,4,228},


/* 0011 0111 0000 ssss disp16 *** ldrl disp16,rrs */
{
#ifdef NICENAMES
"ldrl disp16,rrs",32,17,
0x00,
#endif
"ldrl",OPC_ldrl,0,{CLASS_DISP,CLASS_REG_LONG+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+7,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_DISP+(ARG_DISP16),0,0,0,0,},2,4,229},


/* 0011 0101 0000 dddd disp16 *** ldrl rrd,disp16 */
{
#ifdef NICENAMES
"ldrl rrd,disp16",32,17,
0x00,
#endif
"ldrl",OPC_ldrl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_DISP,},
	{CLASS_BIT+3,CLASS_BIT+5,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_DISP+(ARG_DISP16),0,0,0,0,},2,4,230},


/* 0111 1011 0000 1010 *** mbit */
{
#ifdef NICENAMES
"mbit",16,7,
0x38,
#endif
"mbit",OPC_mbit,0,{0},
	{CLASS_BIT+7,CLASS_BIT+0xb,CLASS_BIT+0,CLASS_BIT+0xa,0,0,0,0,0,},0,2,231},


/* 0111 1011 dddd 1101 *** mreq rd */
{
#ifdef NICENAMES
"mreq rd",16,12,
0x18,
#endif
"mreq",OPC_mreq,0,{CLASS_REG_WORD+(ARG_RD),},
	{CLASS_BIT+7,CLASS_BIT+0xb,CLASS_REG+(ARG_RD),CLASS_BIT+0xd,0,0,0,0,0,},1,2,232},


/* 0111 1011 0000 1001 *** mres */
{
#ifdef NICENAMES
"mres",16,5,
0x00,
#endif
"mres",OPC_mres,0,{0},
	{CLASS_BIT+7,CLASS_BIT+0xb,CLASS_BIT+0,CLASS_BIT+9,0,0,0,0,0,},0,2,233},


/* 0111 1011 0000 1000 *** mset */
{
#ifdef NICENAMES
"mset",16,5,
0x00,
#endif
"mset",OPC_mset,0,{0},
	{CLASS_BIT+7,CLASS_BIT+0xb,CLASS_BIT+0,CLASS_BIT+8,0,0,0,0,0,},0,2,234},


/* 0001 1001 ssN0 dddd *** mult rrd,@rs */
{
#ifdef NICENAMES
"mult rrd,@rs",16,70,
0x3c,
#endif
"mult",OPC_mult,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+1,CLASS_BIT+9,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,235},


/* 0101 1001 0000 dddd address_src *** mult rrd,address_src */
{
#ifdef NICENAMES
"mult rrd,address_src",16,70,
0x3c,
#endif
"mult",OPC_mult,0,{CLASS_REG_LONG+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+5,CLASS_BIT+9,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,236},


/* 0101 1001 ssN0 dddd address_src *** mult rrd,address_src(rs) */
{
#ifdef NICENAMES
"mult rrd,address_src(rs)",16,70,
0x3c,
#endif
"mult",OPC_mult,0,{CLASS_REG_LONG+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+9,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,237},


/* 0001 1001 0000 dddd imm16 *** mult rrd,imm16 */
{
#ifdef NICENAMES
"mult rrd,imm16",16,70,
0x3c,
#endif
"mult",OPC_mult,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+1,CLASS_BIT+9,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,238},


/* 1001 1001 ssss dddd *** mult rrd,rs */
{
#ifdef NICENAMES
"mult rrd,rs",16,70,
0x3c,
#endif
"mult",OPC_mult,0,{CLASS_REG_LONG+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+9,CLASS_BIT+9,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,239},


/* 0001 1000 ssN0 dddd *** multl rqd,@rs */
{
#ifdef NICENAMES
"multl rqd,@rs",32,282,
0x3c,
#endif
"multl",OPC_multl,0,{CLASS_REG_QUAD+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+1,CLASS_BIT+8,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,240},


/* 0101 1000 0000 dddd address_src *** multl rqd,address_src */
{
#ifdef NICENAMES
"multl rqd,address_src",32,282,
0x3c,
#endif
"multl",OPC_multl,0,{CLASS_REG_QUAD+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+5,CLASS_BIT+8,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,241},


/* 0101 1000 ssN0 dddd address_src *** multl rqd,address_src(rs) */
{
#ifdef NICENAMES
"multl rqd,address_src(rs)",32,282,
0x3c,
#endif
"multl",OPC_multl,0,{CLASS_REG_QUAD+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+8,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,242},


/* 0001 1000 0000 dddd imm32 *** multl rqd,imm32 */
{
#ifdef NICENAMES
"multl rqd,imm32",32,282,
0x3c,
#endif
"multl",OPC_multl,0,{CLASS_REG_QUAD+(ARG_RD),CLASS_IMM+(ARG_IMM32),},
	{CLASS_BIT+1,CLASS_BIT+8,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM32),0,0,0,0,},2,6,243},


/* 1001 1000 ssss dddd *** multl rqd,rrs */
{
#ifdef NICENAMES
"multl rqd,rrs",32,282,
0x3c,
#endif
"multl",OPC_multl,0,{CLASS_REG_QUAD+(ARG_RD),CLASS_REG_LONG+(ARG_RS),},
	{CLASS_BIT+9,CLASS_BIT+8,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,244},


/* 0000 1101 ddN0 0010 *** neg @rd */
{
#ifdef NICENAMES
"neg @rd",16,12,
0x3c,
#endif
"neg",OPC_neg,0,{CLASS_IR+(ARG_RD),},
	{CLASS_BIT+0,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+2,0,0,0,0,0,},1,2,245},


/* 0100 1101 0000 0010 address_dst *** neg address_dst */
{
#ifdef NICENAMES
"neg address_dst",16,15,
0x3c,
#endif
"neg",OPC_neg,0,{CLASS_DA+(ARG_DST),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_BIT+0,CLASS_BIT+2,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,246},


/* 0100 1101 ddN0 0010 address_dst *** neg address_dst(rd) */
{
#ifdef NICENAMES
"neg address_dst(rd)",16,16,
0x3c,
#endif
"neg",OPC_neg,0,{CLASS_X+(ARG_RD),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+2,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,247},


/* 1000 1101 dddd 0010 *** neg rd */
{
#ifdef NICENAMES
"neg rd",16,7,
0x3c,
#endif
"neg",OPC_neg,0,{CLASS_REG_WORD+(ARG_RD),},
	{CLASS_BIT+8,CLASS_BIT+0xd,CLASS_REG+(ARG_RD),CLASS_BIT+2,0,0,0,0,0,},1,2,248},


/* 0000 1100 ddN0 0010 *** negb @rd */
{
#ifdef NICENAMES
"negb @rd",8,12,
0x3c,
#endif
"negb",OPC_negb,0,{CLASS_IR+(ARG_RD),},
	{CLASS_BIT+0,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+2,0,0,0,0,0,},1,2,249},


/* 0100 1100 0000 0010 address_dst *** negb address_dst */
{
#ifdef NICENAMES
"negb address_dst",8,15,
0x3c,
#endif
"negb",OPC_negb,0,{CLASS_DA+(ARG_DST),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_BIT+0,CLASS_BIT+2,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,250},


/* 0100 1100 ddN0 0010 address_dst *** negb address_dst(rd) */
{
#ifdef NICENAMES
"negb address_dst(rd)",8,16,
0x3c,
#endif
"negb",OPC_negb,0,{CLASS_X+(ARG_RD),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+2,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,251},


/* 1000 1100 dddd 0010 *** negb rbd */
{
#ifdef NICENAMES
"negb rbd",8,7,
0x3c,
#endif
"negb",OPC_negb,0,{CLASS_REG_BYTE+(ARG_RD),},
	{CLASS_BIT+8,CLASS_BIT+0xc,CLASS_REG+(ARG_RD),CLASS_BIT+2,0,0,0,0,0,},1,2,252},


/* 1000 1101 0000 0111 *** nop */
{
#ifdef NICENAMES
"nop",16,7,
0x00,
#endif
"nop",OPC_nop,0,{0},
	{CLASS_BIT+8,CLASS_BIT+0xd,CLASS_BIT+0,CLASS_BIT+7,0,0,0,0,0,},0,2,253},


/* 0000 0101 ssN0 dddd *** or rd,@rs */
{
#ifdef NICENAMES
"or rd,@rs",16,7,
0x38,
#endif
"or",OPC_or,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+0,CLASS_BIT+5,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,254},


/* 0100 0101 0000 dddd address_src *** or rd,address_src */
{
#ifdef NICENAMES
"or rd,address_src",16,9,
0x38,
#endif
"or",OPC_or,0,{CLASS_REG_WORD+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+4,CLASS_BIT+5,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,255},


/* 0100 0101 ssN0 dddd address_src *** or rd,address_src(rs) */
{
#ifdef NICENAMES
"or rd,address_src(rs)",16,10,
0x38,
#endif
"or",OPC_or,0,{CLASS_REG_WORD+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+4,CLASS_BIT+5,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,256},


/* 0000 0101 0000 dddd imm16 *** or rd,imm16 */
{
#ifdef NICENAMES
"or rd,imm16",16,7,
0x38,
#endif
"or",OPC_or,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+0,CLASS_BIT+5,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,257},


/* 1000 0101 ssss dddd *** or rd,rs */
{
#ifdef NICENAMES
"or rd,rs",16,4,
0x38,
#endif
"or",OPC_or,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+5,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,258},


/* 0000 0100 ssN0 dddd *** orb rbd,@rs */
{
#ifdef NICENAMES
"orb rbd,@rs",8,7,
0x3c,
#endif
"orb",OPC_orb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+0,CLASS_BIT+4,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,259},


/* 0100 0100 0000 dddd address_src *** orb rbd,address_src */
{
#ifdef NICENAMES
"orb rbd,address_src",8,9,
0x3c,
#endif
"orb",OPC_orb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+4,CLASS_BIT+4,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,260},


/* 0100 0100 ssN0 dddd address_src *** orb rbd,address_src(rs) */
{
#ifdef NICENAMES
"orb rbd,address_src(rs)",8,10,
0x3c,
#endif
"orb",OPC_orb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+4,CLASS_BIT+4,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,261},


/* 0000 0100 0000 dddd imm8 imm8 *** orb rbd,imm8 */
{
#ifdef NICENAMES
"orb rbd,imm8",8,7,
0x3c,
#endif
"orb",OPC_orb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0,CLASS_BIT+4,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM8),CLASS_IMM+(ARG_IMM8),0,0,0,},2,4,262},


/* 1000 0100 ssss dddd *** orb rbd,rbs */
{
#ifdef NICENAMES
"orb rbd,rbs",8,4,
0x3c,
#endif
"orb",OPC_orb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+4,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,263},


/* 0011 1111 ddN0 ssss *** out @rd,rs */
{
#ifdef NICENAMES
"out @rd,rs",16,0,
0x04,
#endif
"out",OPC_out,0,{CLASS_IR+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+0xf,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),0,0,0,0,0,},2,2,264},


/* 0011 1011 ssss 0110 imm16 *** out imm16,rs */
{
#ifdef NICENAMES
"out imm16,rs",16,0,
0x04,
#endif
"out",OPC_out,0,{CLASS_IMM+(ARG_IMM16),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+0xb,CLASS_REG+(ARG_RS),CLASS_BIT+6,CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,265},


/* 0011 1110 ddN0 ssss *** outb @rd,rbs */
{
#ifdef NICENAMES
"outb @rd,rbs",8,0,
0x04,
#endif
"outb",OPC_outb,0,{CLASS_IR+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+0xe,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),0,0,0,0,0,},2,2,266},


/* 0011 1010 ssss 0110 imm16 *** outb imm16,rbs */
{
#ifdef NICENAMES
"outb imm16,rbs",8,0,
0x04,
#endif
"outb",OPC_outb,0,{CLASS_IMM+(ARG_IMM16),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REG+(ARG_RS),CLASS_BIT+6,CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,267},


/* 0011 1011 ssN0 1010 0000 aaaa ddN0 1000 *** outd @rd,@rs,ra */
{
#ifdef NICENAMES
"outd @rd,@rs,ra",16,0,
0x04,
#endif
"outd",OPC_outd,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+0xa,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,268},


/* 0011 1010 ssN0 1010 0000 aaaa ddN0 1000 *** outdb @rd,@rs,rba */
{
#ifdef NICENAMES
"outdb @rd,@rs,rba",16,0,
0x04,
#endif
"outdb",OPC_outdb,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_BYTE+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+0xa,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,269},


/* 0011 1011 ssN0 0010 0000 aaaa ddN0 1000 *** outi @rd,@rs,ra */
{
#ifdef NICENAMES
"outi @rd,@rs,ra",16,0,
0x04,
#endif
"outi",OPC_outi,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+2,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,270},


/* 0011 1010 ssN0 0010 0000 aaaa ddN0 1000 *** outib @rd,@rs,ra */
{
#ifdef NICENAMES
"outib @rd,@rs,ra",16,0,
0x04,
#endif
"outib",OPC_outib,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+2,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,271},


/* 0011 1010 ssN0 0010 0000 aaaa ddN0 0000 *** outibr @rd,@rs,ra */
{
#ifdef NICENAMES
"outibr @rd,@rs,ra",16,0,
0x04,
#endif
"outibr",OPC_outibr,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+2,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+0,0,},3,4,272},


/* 0001 0111 ssN0 ddN0 *** pop @rd,@rs */
{
#ifdef NICENAMES
"pop @rd,@rs",16,12,
0x00,
#endif
"pop",OPC_pop,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+1,CLASS_BIT+7,CLASS_REGN0+(ARG_RS),CLASS_REGN0+(ARG_RD),0,0,0,0,0,},2,2,273},


/* 0101 0111 ssN0 ddN0 address_dst *** pop address_dst(rd),@rs */
{
#ifdef NICENAMES
"pop address_dst(rd),@rs",16,16,
0x00,
#endif
"pop",OPC_pop,0,{CLASS_X+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+7,CLASS_REGN0+(ARG_RS),CLASS_REGN0+(ARG_RD),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,274},


/* 0101 0111 ssN0 0000 address_dst *** pop address_dst,@rs */
{
#ifdef NICENAMES
"pop address_dst,@rs",16,16,
0x00,
#endif
"pop",OPC_pop,0,{CLASS_DA+(ARG_DST),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+7,CLASS_REGN0+(ARG_RS),CLASS_BIT+0,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,275},


/* 1001 0111 ssN0 dddd *** pop rd,@rs */
{
#ifdef NICENAMES
"pop rd,@rs",16,8,
0x00,
#endif
"pop",OPC_pop,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+9,CLASS_BIT+7,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,276},


/* 0001 0101 ssN0 ddN0 *** popl @rd,@rs */
{
#ifdef NICENAMES
"popl @rd,@rs",32,19,
0x00,
#endif
"popl",OPC_popl,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+1,CLASS_BIT+5,CLASS_REGN0+(ARG_RS),CLASS_REGN0+(ARG_RD),0,0,0,0,0,},2,2,277},


/* 0101 0101 ssN0 ddN0 address_dst *** popl address_dst(rd),@rs */
{
#ifdef NICENAMES
"popl address_dst(rd),@rs",32,23,
0x00,
#endif
"popl",OPC_popl,0,{CLASS_X+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+5,CLASS_REGN0+(ARG_RS),CLASS_REGN0+(ARG_RD),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,278},


/* 0101 0101 ssN0 0000 address_dst *** popl address_dst,@rs */
{
#ifdef NICENAMES
"popl address_dst,@rs",32,23,
0x00,
#endif
"popl",OPC_popl,0,{CLASS_DA+(ARG_DST),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+5,CLASS_REGN0+(ARG_RS),CLASS_BIT+0,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,279},


/* 1001 0101 ssN0 dddd *** popl rrd,@rs */
{
#ifdef NICENAMES
"popl rrd,@rs",32,12,
0x00,
#endif
"popl",OPC_popl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+9,CLASS_BIT+5,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,280},


/* 0001 0011 ddN0 ssN0 *** push @rd,@rs */
{
#ifdef NICENAMES
"push @rd,@rs",16,13,
0x00,
#endif
"push",OPC_push,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+1,CLASS_BIT+3,CLASS_REGN0+(ARG_RD),CLASS_REGN0+(ARG_RS),0,0,0,0,0,},2,2,281},


/* 0101 0011 ddN0 0000 address_src *** push @rd,address_src */
{
#ifdef NICENAMES
"push @rd,address_src",16,14,
0x00,
#endif
"push",OPC_push,0,{CLASS_IR+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+5,CLASS_BIT+3,CLASS_REGN0+(ARG_RD),CLASS_BIT+0,CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,282},


/* 0101 0011 ddN0 ssN0 address_src *** push @rd,address_src(rs) */
{
#ifdef NICENAMES
"push @rd,address_src(rs)",16,14,
0x00,
#endif
"push",OPC_push,0,{CLASS_IR+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+3,CLASS_REGN0+(ARG_RD),CLASS_REGN0+(ARG_RS),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,283},


/* 0000 1101 ddN0 1001 imm16 *** push @rd,imm16 */
{
#ifdef NICENAMES
"push @rd,imm16",16,12,
0x00,
#endif
"push",OPC_push,0,{CLASS_IR+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+0,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+9,CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,284},


/* 1001 0011 ddN0 ssss *** push @rd,rs */
{
#ifdef NICENAMES
"push @rd,rs",16,9,
0x00,
#endif
"push",OPC_push,0,{CLASS_IR+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+9,CLASS_BIT+3,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),0,0,0,0,0,},2,2,285},


/* 0001 0001 ddN0 ssN0 *** pushl @rd,@rs */
{
#ifdef NICENAMES
"pushl @rd,@rs",32,20,
0x00,
#endif
"pushl",OPC_pushl,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+1,CLASS_BIT+1,CLASS_REGN0+(ARG_RD),CLASS_REGN0+(ARG_RS),0,0,0,0,0,},2,2,286},


/* 0101 0001 ddN0 0000 address_src *** pushl @rd,address_src */
{
#ifdef NICENAMES
"pushl @rd,address_src",32,21,
0x00,
#endif
"pushl",OPC_pushl,0,{CLASS_IR+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+5,CLASS_BIT+1,CLASS_REGN0+(ARG_RD),CLASS_BIT+0,CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,287},


/* 0101 0001 ddN0 ssN0 address_src *** pushl @rd,address_src(rs) */
{
#ifdef NICENAMES
"pushl @rd,address_src(rs)",32,21,
0x00,
#endif
"pushl",OPC_pushl,0,{CLASS_IR+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+1,CLASS_REGN0+(ARG_RD),CLASS_REGN0+(ARG_RS),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,288},


/* 1001 0001 ddN0 ssss *** pushl @rd,rrs */
{
#ifdef NICENAMES
"pushl @rd,rrs",32,12,
0x00,
#endif
"pushl",OPC_pushl,0,{CLASS_IR+(ARG_RD),CLASS_REG_LONG+(ARG_RS),},
	{CLASS_BIT+9,CLASS_BIT+1,CLASS_REGN0+(ARG_RD),CLASS_REG+(ARG_RS),0,0,0,0,0,},2,2,289},


/* 0010 0011 ddN0 imm4 *** res @rd,imm4 */
{
#ifdef NICENAMES
"res @rd,imm4",16,11,
0x00,
#endif
"res",OPC_res,0,{CLASS_IR+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+2,CLASS_BIT+3,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4),0,0,0,0,0,},2,2,290},


/* 0110 0011 ddN0 imm4 address_dst *** res address_dst(rd),imm4 */
{
#ifdef NICENAMES
"res address_dst(rd),imm4",16,14,
0x00,
#endif
"res",OPC_res,0,{CLASS_X+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+6,CLASS_BIT+3,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,291},


/* 0110 0011 0000 imm4 address_dst *** res address_dst,imm4 */
{
#ifdef NICENAMES
"res address_dst,imm4",16,13,
0x00,
#endif
"res",OPC_res,0,{CLASS_DA+(ARG_DST),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+6,CLASS_BIT+3,CLASS_BIT+0,CLASS_IMM+(ARG_IMM4),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,292},


/* 1010 0011 dddd imm4 *** res rd,imm4 */
{
#ifdef NICENAMES
"res rd,imm4",16,4,
0x00,
#endif
"res",OPC_res,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+0xa,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM4),0,0,0,0,0,},2,2,293},


/* 0010 0011 0000 ssss 0000 dddd 0000 0000 *** res rd,rs */
{
#ifdef NICENAMES
"res rd,rs",16,10,
0x00,
#endif
"res",OPC_res,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+2,CLASS_BIT+3,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,294},


/* 0010 0010 ddN0 imm4 *** resb @rd,imm4 */
{
#ifdef NICENAMES
"resb @rd,imm4",8,11,
0x00,
#endif
"resb",OPC_resb,0,{CLASS_IR+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+2,CLASS_BIT+2,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4),0,0,0,0,0,},2,2,295},


/* 0110 0010 ddN0 imm4 address_dst *** resb address_dst(rd),imm4 */
{
#ifdef NICENAMES
"resb address_dst(rd),imm4",8,14,
0x00,
#endif
"resb",OPC_resb,0,{CLASS_X+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+6,CLASS_BIT+2,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,296},


/* 0110 0010 0000 imm4 address_dst *** resb address_dst,imm4 */
{
#ifdef NICENAMES
"resb address_dst,imm4",8,13,
0x00,
#endif
"resb",OPC_resb,0,{CLASS_DA+(ARG_DST),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+6,CLASS_BIT+2,CLASS_BIT+0,CLASS_IMM+(ARG_IMM4),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,297},


/* 1010 0010 dddd imm4 *** resb rbd,imm4 */
{
#ifdef NICENAMES
"resb rbd,imm4",8,4,
0x00,
#endif
"resb",OPC_resb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+0xa,CLASS_BIT+2,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM4),0,0,0,0,0,},2,2,298},


/* 0010 0010 0000 ssss 0000 dddd 0000 0000 *** resb rbd,rs */
{
#ifdef NICENAMES
"resb rbd,rs",8,10,
0x00,
#endif
"resb",OPC_resb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+2,CLASS_BIT+2,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,299},


/* 1000 1101 flags 0011 *** resflg flags */
{
#ifdef NICENAMES
"resflg flags",16,7,
0x3c,
#endif
"resflg",OPC_resflg,0,{CLASS_FLAGS,},
	{CLASS_BIT+8,CLASS_BIT+0xd,CLASS_FLAGS,CLASS_BIT+3,0,0,0,0,0,},1,2,300},


/* 1001 1110 0000 cccc *** ret cc */
{
#ifdef NICENAMES
"ret cc",16,10,
0x00,
#endif
"ret",OPC_ret,0,{CLASS_CC,},
	{CLASS_BIT+9,CLASS_BIT+0xe,CLASS_BIT+0,CLASS_CC,0,0,0,0,0,},1,2,301},


/* 1011 0011 dddd 00I0 *** rl rd,imm1or2 */
{
#ifdef NICENAMES
"rl rd,imm1or2",16,6,
0x3c,
#endif
"rl",OPC_rl,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM1OR2),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT_1OR2+0,0,0,0,0,0,},2,2,302},


/* 1011 0010 dddd 00I0 *** rlb rbd,imm1or2 */
{
#ifdef NICENAMES
"rlb rbd,imm1or2",8,6,
0x3c,
#endif
"rlb",OPC_rlb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM1OR2),},
	{CLASS_BIT+0xb,CLASS_BIT+2,CLASS_REG+(ARG_RD),CLASS_BIT_1OR2+0,0,0,0,0,0,},2,2,303},


/* 1011 0011 dddd 10I0 *** rlc rd,imm1or2 */
{
#ifdef NICENAMES
"rlc rd,imm1or2",16,6,
0x3c,
#endif
"rlc",OPC_rlc,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM1OR2),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT_1OR2+8,0,0,0,0,0,},2,2,304},


/* 1011 0010 dddd 10I0 *** rlcb rbd,imm1or2 */
{
#ifdef NICENAMES
"rlcb rbd,imm1or2",8,9,
0x10,
#endif
"rlcb",OPC_rlcb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM1OR2),},
	{CLASS_BIT+0xb,CLASS_BIT+2,CLASS_REG+(ARG_RD),CLASS_BIT_1OR2+8,0,0,0,0,0,},2,2,305},


/* 1011 1110 aaaa bbbb *** rldb rbb,rba */
{
#ifdef NICENAMES
"rldb rbb,rba",8,9,
0x10,
#endif
"rldb",OPC_rldb,0,{CLASS_REG_BYTE+(ARG_RB),CLASS_REG_BYTE+(ARG_RA),},
	{CLASS_BIT+0xb,CLASS_BIT+0xe,CLASS_REG+(ARG_RA),CLASS_REG+(ARG_RB),0,0,0,0,0,},2,2,306},


/* 1011 0011 dddd 01I0 *** rr rd,imm1or2 */
{
#ifdef NICENAMES
"rr rd,imm1or2",16,6,
0x3c,
#endif
"rr",OPC_rr,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM1OR2),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT_1OR2+4,0,0,0,0,0,},2,2,307},


/* 1011 0010 dddd 01I0 *** rrb rbd,imm1or2 */
{
#ifdef NICENAMES
"rrb rbd,imm1or2",8,6,
0x3c,
#endif
"rrb",OPC_rrb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM1OR2),},
	{CLASS_BIT+0xb,CLASS_BIT+2,CLASS_REG+(ARG_RD),CLASS_BIT_1OR2+4,0,0,0,0,0,},2,2,308},


/* 1011 0011 dddd 11I0 *** rrc rd,imm1or2 */
{
#ifdef NICENAMES
"rrc rd,imm1or2",16,6,
0x3c,
#endif
"rrc",OPC_rrc,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM1OR2),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT_1OR2+0xc,0,0,0,0,0,},2,2,309},


/* 1011 0010 dddd 11I0 *** rrcb rbd,imm1or2 */
{
#ifdef NICENAMES
"rrcb rbd,imm1or2",8,9,
0x10,
#endif
"rrcb",OPC_rrcb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM1OR2),},
	{CLASS_BIT+0xb,CLASS_BIT+2,CLASS_REG+(ARG_RD),CLASS_BIT_1OR2+0xc,0,0,0,0,0,},2,2,310},


/* 1011 1100 aaaa bbbb *** rrdb rbb,rba */
{
#ifdef NICENAMES
"rrdb rbb,rba",8,9,
0x10,
#endif
"rrdb",OPC_rrdb,0,{CLASS_REG_BYTE+(ARG_RB),CLASS_REG_BYTE+(ARG_RA),},
	{CLASS_BIT+0xb,CLASS_BIT+0xc,CLASS_REG+(ARG_RA),CLASS_REG+(ARG_RB),0,0,0,0,0,},2,2,311},


/* 0011 0110 imm8 *** rsvd36 */
{
#ifdef NICENAMES
"rsvd36",8,10,
0x00,
#endif
"rsvd36",OPC_rsvd36,0,{0},
	{CLASS_BIT+3,CLASS_BIT+6,CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},0,2,312},


/* 0011 1000 imm8 *** rsvd38 */
{
#ifdef NICENAMES
"rsvd38",8,10,
0x00,
#endif
"rsvd38",OPC_rsvd38,0,{0},
	{CLASS_BIT+3,CLASS_BIT+8,CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},0,2,313},


/* 0111 1000 imm8 *** rsvd78 */
{
#ifdef NICENAMES
"rsvd78",8,10,
0x00,
#endif
"rsvd78",OPC_rsvd78,0,{0},
	{CLASS_BIT+7,CLASS_BIT+8,CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},0,2,314},


/* 0111 1110 imm8 *** rsvd7e */
{
#ifdef NICENAMES
"rsvd7e",8,10,
0x00,
#endif
"rsvd7e",OPC_rsvd7e,0,{0},
	{CLASS_BIT+7,CLASS_BIT+0xe,CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},0,2,315},


/* 1001 1101 imm8 *** rsvd9d */
{
#ifdef NICENAMES
"rsvd9d",8,10,
0x00,
#endif
"rsvd9d",OPC_rsvd9d,0,{0},
	{CLASS_BIT+9,CLASS_BIT+0xd,CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},0,2,316},


/* 1001 1111 imm8 *** rsvd9f */
{
#ifdef NICENAMES
"rsvd9f",8,10,
0x00,
#endif
"rsvd9f",OPC_rsvd9f,0,{0},
	{CLASS_BIT+9,CLASS_BIT+0xf,CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},0,2,317},


/* 1011 1001 imm8 *** rsvdb9 */
{
#ifdef NICENAMES
"rsvdb9",8,10,
0x00,
#endif
"rsvdb9",OPC_rsvdb9,0,{0},
	{CLASS_BIT+0xb,CLASS_BIT+9,CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},0,2,318},


/* 1011 1111 imm8 *** rsvdbf */
{
#ifdef NICENAMES
"rsvdbf",8,10,
0x00,
#endif
"rsvdbf",OPC_rsvdbf,0,{0},
	{CLASS_BIT+0xb,CLASS_BIT+0xf,CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},0,2,319},


/* 1011 0111 ssss dddd *** sbc rd,rs */
{
#ifdef NICENAMES
"sbc rd,rs",16,5,
0x3c,
#endif
"sbc",OPC_sbc,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+0xb,CLASS_BIT+7,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,320},


/* 1011 0110 ssss dddd *** sbcb rbd,rbs */
{
#ifdef NICENAMES
"sbcb rbd,rbs",8,5,
0x3f,
#endif
"sbcb",OPC_sbcb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+0xb,CLASS_BIT+6,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,321},


/* 0111 1111 imm8 *** sc imm8 */
{
#ifdef NICENAMES
"sc imm8",8,33,
0x3f,
#endif
"sc",OPC_sc,0,{CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+7,CLASS_BIT+0xf,CLASS_IMM+(ARG_IMM8),0,0,0,0,0,0,},1,2,322},


/* 1011 0011 dddd 1011 0000 ssss 0000 0000 *** sda rd,rs */
{
#ifdef NICENAMES
"sda rd,rs",16,15,
0x3c,
#endif
"sda",OPC_sda,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT+0xb,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,323},


/* 1011 0010 dddd 1011 0000 ssss 0000 0000 *** sdab rbd,rs */
{
#ifdef NICENAMES
"sdab rbd,rs",8,15,
0x3c,
#endif
"sdab",OPC_sdab,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+0xb,CLASS_BIT+2,CLASS_REG+(ARG_RD),CLASS_BIT+0xb,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,324},


/* 1011 0011 dddd 1111 0000 ssss 0000 0000 *** sdal rrd,rs */
{
#ifdef NICENAMES
"sdal rrd,rs",32,15,
0x3c,
#endif
"sdal",OPC_sdal,0,{CLASS_REG_LONG+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT+0xf,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,325},


/* 1011 0011 dddd 0011 0000 ssss 0000 0000 *** sdl rd,rs */
{
#ifdef NICENAMES
"sdl rd,rs",16,15,
0x38,
#endif
"sdl",OPC_sdl,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT+3,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,326},


/* 1011 0010 dddd 0011 0000 ssss 0000 0000 *** sdlb rbd,rs */
{
#ifdef NICENAMES
"sdlb rbd,rs",8,15,
0x38,
#endif
"sdlb",OPC_sdlb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+0xb,CLASS_BIT+2,CLASS_REG+(ARG_RD),CLASS_BIT+3,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,327},


/* 1011 0011 dddd 0111 0000 ssss 0000 0000 *** sdll rrd,rs */
{
#ifdef NICENAMES
"sdll rrd,rs",32,15,
0x38,
#endif
"sdll",OPC_sdll,0,{CLASS_REG_LONG+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT+7,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,328},


/* 0010 0101 ddN0 imm4 *** set @rd,imm4 */
{
#ifdef NICENAMES
"set @rd,imm4",16,11,
0x00,
#endif
"set",OPC_set,0,{CLASS_IR+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+2,CLASS_BIT+5,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4),0,0,0,0,0,},2,2,329},


/* 0110 0101 ddN0 imm4 address_dst *** set address_dst(rd),imm4 */
{
#ifdef NICENAMES
"set address_dst(rd),imm4",16,14,
0x00,
#endif
"set",OPC_set,0,{CLASS_X+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+6,CLASS_BIT+5,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,330},


/* 0110 0101 0000 imm4 address_dst *** set address_dst,imm4 */
{
#ifdef NICENAMES
"set address_dst,imm4",16,13,
0x00,
#endif
"set",OPC_set,0,{CLASS_DA+(ARG_DST),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+6,CLASS_BIT+5,CLASS_BIT+0,CLASS_IMM+(ARG_IMM4),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,331},


/* 1010 0101 dddd imm4 *** set rd,imm4 */
{
#ifdef NICENAMES
"set rd,imm4",16,4,
0x00,
#endif
"set",OPC_set,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+0xa,CLASS_BIT+5,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM4),0,0,0,0,0,},2,2,332},


/* 0010 0101 0000 ssss 0000 dddd 0000 0000 *** set rd,rs */
{
#ifdef NICENAMES
"set rd,rs",16,10,
0x00,
#endif
"set",OPC_set,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+2,CLASS_BIT+5,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,333},


/* 0010 0100 ddN0 imm4 *** setb @rd,imm4 */
{
#ifdef NICENAMES
"setb @rd,imm4",8,11,
0x00,
#endif
"setb",OPC_setb,0,{CLASS_IR+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+2,CLASS_BIT+4,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4),0,0,0,0,0,},2,2,334},


/* 0110 0100 ddN0 imm4 address_dst *** setb address_dst(rd),imm4 */
{
#ifdef NICENAMES
"setb address_dst(rd),imm4",8,14,
0x00,
#endif
"setb",OPC_setb,0,{CLASS_X+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+6,CLASS_BIT+4,CLASS_REGN0+(ARG_RD),CLASS_IMM+(ARG_IMM4),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,335},


/* 0110 0100 0000 imm4 address_dst *** setb address_dst,imm4 */
{
#ifdef NICENAMES
"setb address_dst,imm4",8,13,
0x00,
#endif
"setb",OPC_setb,0,{CLASS_DA+(ARG_DST),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+6,CLASS_BIT+4,CLASS_BIT+0,CLASS_IMM+(ARG_IMM4),CLASS_ADDRESS+(ARG_DST),0,0,0,0,},2,4,336},


/* 1010 0100 dddd imm4 *** setb rbd,imm4 */
{
#ifdef NICENAMES
"setb rbd,imm4",8,4,
0x00,
#endif
"setb",OPC_setb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM +(ARG_IMM4),},
	{CLASS_BIT+0xa,CLASS_BIT+4,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM4),0,0,0,0,0,},2,2,337},


/* 0010 0100 0000 ssss 0000 dddd 0000 0000 *** setb rbd,rs */
{
#ifdef NICENAMES
"setb rbd,rs",8,10,
0x00,
#endif
"setb",OPC_setb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+2,CLASS_BIT+4,CLASS_BIT+0,CLASS_REG+(ARG_RS),CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_BIT+0,CLASS_BIT+0,0,},2,4,338},


/* 1000 1101 flags 0001 *** setflg flags */
{
#ifdef NICENAMES
"setflg flags",16,7,
0x3c,
#endif
"setflg",OPC_setflg,0,{CLASS_FLAGS,},
	{CLASS_BIT+8,CLASS_BIT+0xd,CLASS_FLAGS,CLASS_BIT+1,0,0,0,0,0,},1,2,339},


/* 0011 1011 dddd 0101 imm16 *** sin rd,imm16 */
{
#ifdef NICENAMES
"sin rd,imm16",8,0,
0x00,
#endif
"sin",OPC_sin,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+3,CLASS_BIT+0xb,CLASS_REG+(ARG_RD),CLASS_BIT+5,CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,340},


/* 0011 1010 dddd 0101 imm16 *** sinb rbd,imm16 */
{
#ifdef NICENAMES
"sinb rbd,imm16",8,0,
0x00,
#endif
"sinb",OPC_sinb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REG+(ARG_RD),CLASS_BIT+5,CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,341},


/* 0011 1011 ssN0 1000 0001 aaaa ddN0 1000 *** sind @rd,@rs,ra */
{
#ifdef NICENAMES
"sind @rd,@rs,ra",16,0,
0x00,
#endif
"sind",OPC_sind,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+8,CLASS_BIT+1,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,342},


/* 0011 1010 ssN0 1000 0001 aaaa ddN0 1000 *** sindb @rd,@rs,rba */
{
#ifdef NICENAMES
"sindb @rd,@rs,rba",8,0,
0x00,
#endif
"sindb",OPC_sindb,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_BYTE+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+8,CLASS_BIT+1,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,343},


/* 0011 1010 ssN0 0001 0000 aaaa ddN0 1000 *** sinib @rd,@rs,ra */
{
#ifdef NICENAMES
"sinib @rd,@rs,ra",8,0,
0x00,
#endif
"sinib",OPC_sinib,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,344},


/* 0011 1010 ssN0 0001 0000 aaaa ddN0 0000 *** sinibr @rd,@rs,ra */
{
#ifdef NICENAMES
"sinibr @rd,@rs,ra",16,0,
0x00,
#endif
"sinibr",OPC_sinibr,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+1,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+0,0,},3,4,345},


/* 1011 0011 dddd 1001 0000 0000 imm8 *** sla rd,imm8 */
{
#ifdef NICENAMES
"sla rd,imm8",16,13,
0x3c,
#endif
"sla",OPC_sla,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT+9,CLASS_BIT+0,CLASS_BIT+0,CLASS_IMM+(ARG_IMM8),0,0,},2,4,346},


/* 1011 0010 dddd 1001  0000 0000 imm8 *** slab rbd,imm8 */
{
#ifdef NICENAMES
"slab rbd,imm8",8,13,
0x3c,
#endif
"slab",OPC_slab,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0xb,CLASS_BIT+2,CLASS_REG+(ARG_RD),CLASS_BIT+9,CLASS_BIT+0,CLASS_BIT+0,CLASS_IMM+(ARG_IMM8),0,0,},2,4,347},


/* 1011 0011 dddd 1101 0000 0000 imm8 *** slal rrd,imm8 */
{
#ifdef NICENAMES
"slal rrd,imm8",32,13,
0x3c,
#endif
"slal",OPC_slal,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT+0xd,CLASS_BIT+0,CLASS_BIT+0,CLASS_IMM+(ARG_IMM8),0,0,},2,4,348},


/* 1011 0011 dddd 0001 0000 0000 imm8 *** sll rd,imm8 */
{
#ifdef NICENAMES
"sll rd,imm8",16,13,
0x38,
#endif
"sll",OPC_sll,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT+1,CLASS_BIT+0,CLASS_BIT+0,CLASS_IMM+(ARG_IMM8),0,0,},2,4,349},


/* 1011 0010 dddd 0001  0000 0000 imm8 *** sllb rbd,imm8 */
{
#ifdef NICENAMES
"sllb rbd,imm8",8,13,
0x38,
#endif
"sllb",OPC_sllb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0xb,CLASS_BIT+2,CLASS_REG+(ARG_RD),CLASS_BIT+1,CLASS_BIT+0,CLASS_BIT+0,CLASS_IMM+(ARG_IMM8),0,0,},2,4,350},


/* 1011 0011 dddd 0101 0000 0000 imm8 *** slll rrd,imm8 */
{
#ifdef NICENAMES
"slll rrd,imm8",32,13,
0x38,
#endif
"slll",OPC_slll,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT+5,CLASS_BIT+0,CLASS_BIT+0,CLASS_IMM+(ARG_IMM8),0,0,},2,4,351},


/* 0011 1011 ssss 0111 imm16 *** sout imm16,rs */
{
#ifdef NICENAMES
"sout imm16,rs",16,0,
0x00,
#endif
"sout",OPC_sout,0,{CLASS_IMM+(ARG_IMM16),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+0xb,CLASS_REG+(ARG_RS),CLASS_BIT+7,CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,352},


/* 0011 1010 ssss 0111 imm16 *** soutb imm16,rbs */
{
#ifdef NICENAMES
"soutb imm16,rbs",8,0,
0x00,
#endif
"soutb",OPC_soutb,0,{CLASS_IMM+(ARG_IMM16),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REG+(ARG_RS),CLASS_BIT+7,CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,353},


/* 0011 1011 ssN0 1011 0000 aaaa ddN0 1000 *** soutd @rd,@rs,ra */
{
#ifdef NICENAMES
"soutd @rd,@rs,ra",16,0,
0x00,
#endif
"soutd",OPC_soutd,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xb,CLASS_REGN0+(ARG_RS),CLASS_BIT+0xb,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,354},


/* 0011 1010 ssN0 1011 0000 aaaa ddN0 1000 *** soutdb @rd,@rs,rba */
{
#ifdef NICENAMES
"soutdb @rd,@rs,rba",8,0,
0x00,
#endif
"soutdb",OPC_soutdb,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_BYTE+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+0xb,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,355},


/* 0011 1010 ssN0 0011 0000 aaaa ddN0 1000 *** soutib @rd,@rs,ra */
{
#ifdef NICENAMES
"soutib @rd,@rs,ra",8,0,
0x00,
#endif
"soutib",OPC_soutib,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+3,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,},3,4,356},


/* 0011 1010 ssN0 0011 0000 aaaa ddN0 0000 *** soutibr @rd,@rs,ra */
{
#ifdef NICENAMES
"soutibr @rd,@rs,ra",16,0,
0x00,
#endif
"soutibr",OPC_soutibr,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_WORD+(ARG_RA),},
	{CLASS_BIT+3,CLASS_BIT+0xa,CLASS_REGN0+(ARG_RS),CLASS_BIT+3,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RD),CLASS_BIT+0,0,},3,4,357},


/* 1011 0011 dddd 1001 1111 1111 nim8 *** sra rd,imm8 */
{
#ifdef NICENAMES
"sra rd,imm8",16,13,
0x3c,
#endif
"sra",OPC_sra,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT+9,CLASS_BIT+0xf,CLASS_BIT+0xf,CLASS_IMM+(ARG_NIM8),0,0,},2,4,358},


/* 1011 0010 dddd 1001 0000 0000 nim8 *** srab rbd,imm8 */
{
#ifdef NICENAMES
"srab rbd,imm8",8,13,
0x3c,
#endif
"srab",OPC_srab,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0xb,CLASS_BIT+2,CLASS_REG+(ARG_RD),CLASS_BIT+9,CLASS_BIT+0,CLASS_BIT+0,CLASS_IMM+(ARG_NIM8),0,0,},2,4,359},


/* 1011 0011 dddd 1101 1111 1111 nim8 *** sral rrd,imm8 */
{
#ifdef NICENAMES
"sral rrd,imm8",32,13,
0x3c,
#endif
"sral",OPC_sral,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT+0xd,CLASS_BIT+0xf,CLASS_BIT+0xf,CLASS_IMM+(ARG_NIM8),0,0,},2,4,360},


/* 1011 0011 dddd 0001 1111 1111 nim8 *** srl rd,imm8 */
{
#ifdef NICENAMES
"srl rd,imm8",16,13,
0x3c,
#endif
"srl",OPC_srl,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT+1,CLASS_BIT+0xf,CLASS_BIT+0xf,CLASS_IMM+(ARG_NIM8),0,0,},2,4,361},


/* 1011 0010 dddd 0001 0000 0000 nim8 *** srlb rbd,imm8 */
{
#ifdef NICENAMES
"srlb rbd,imm8",8,13,
0x3c,
#endif
"srlb",OPC_srlb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0xb,CLASS_BIT+2,CLASS_REG+(ARG_RD),CLASS_BIT+1,CLASS_BIT+0,CLASS_BIT+0,CLASS_IMM+(ARG_NIM8),0,0,},2,4,362},


/* 1011 0011 dddd 0101 1111 1111 nim8 *** srll rrd,imm8 */
{
#ifdef NICENAMES
"srll rrd,imm8",32,13,
0x3c,
#endif
"srll",OPC_srll,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0xb,CLASS_BIT+3,CLASS_REG+(ARG_RD),CLASS_BIT+5,CLASS_BIT+0xf,CLASS_BIT+0xf,CLASS_IMM+(ARG_NIM8),0,0,},2,4,363},


/* 0000 0011 ssN0 dddd *** sub rd,@rs */
{
#ifdef NICENAMES
"sub rd,@rs",16,7,
0x3c,
#endif
"sub",OPC_sub,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+0,CLASS_BIT+3,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,364},


/* 0100 0011 0000 dddd address_src *** sub rd,address_src */
{
#ifdef NICENAMES
"sub rd,address_src",16,9,
0x3c,
#endif
"sub",OPC_sub,0,{CLASS_REG_WORD+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+4,CLASS_BIT+3,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,365},


/* 0100 0011 ssN0 dddd address_src *** sub rd,address_src(rs) */
{
#ifdef NICENAMES
"sub rd,address_src(rs)",16,10,
0x3c,
#endif
"sub",OPC_sub,0,{CLASS_REG_WORD+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+4,CLASS_BIT+3,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,366},


/* 0000 0011 0000 dddd imm16 *** sub rd,imm16 */
{
#ifdef NICENAMES
"sub rd,imm16",16,7,
0x3c,
#endif
"sub",OPC_sub,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+0,CLASS_BIT+3,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,367},


/* 1000 0011 ssss dddd *** sub rd,rs */
{
#ifdef NICENAMES
"sub rd,rs",16,4,
0x3c,
#endif
"sub",OPC_sub,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+3,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,368},


/* 0000 0010 ssN0 dddd *** subb rbd,@rs */
{
#ifdef NICENAMES
"subb rbd,@rs",8,7,
0x3f,
#endif
"subb",OPC_subb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+0,CLASS_BIT+2,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,369},


/* 0100 0010 0000 dddd address_src *** subb rbd,address_src */
{
#ifdef NICENAMES
"subb rbd,address_src",8,9,
0x3f,
#endif
"subb",OPC_subb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+4,CLASS_BIT+2,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,370},


/* 0100 0010 ssN0 dddd address_src *** subb rbd,address_src(rs) */
{
#ifdef NICENAMES
"subb rbd,address_src(rs)",8,10,
0x3f,
#endif
"subb",OPC_subb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+4,CLASS_BIT+2,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,371},


/* 0000 0010 0000 dddd imm8 imm8 *** subb rbd,imm8 */
{
#ifdef NICENAMES
"subb rbd,imm8",8,7,
0x3f,
#endif
"subb",OPC_subb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0,CLASS_BIT+2,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM8),CLASS_IMM+(ARG_IMM8),0,0,0,},2,4,372},


/* 1000 0010 ssss dddd *** subb rbd,rbs */
{
#ifdef NICENAMES
"subb rbd,rbs",8,4,
0x3f,
#endif
"subb",OPC_subb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+2,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,373},


/* 0001 0010 ssN0 dddd *** subl rrd,@rs */
{
#ifdef NICENAMES
"subl rrd,@rs",32,14,
0x3c,
#endif
"subl",OPC_subl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+1,CLASS_BIT+2,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,374},


/* 0101 0010 0000 dddd address_src *** subl rrd,address_src */
{
#ifdef NICENAMES
"subl rrd,address_src",32,15,
0x3c,
#endif
"subl",OPC_subl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+5,CLASS_BIT+2,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,375},


/* 0101 0010 ssN0 dddd address_src *** subl rrd,address_src(rs) */
{
#ifdef NICENAMES
"subl rrd,address_src(rs)",32,16,
0x3c,
#endif
"subl",OPC_subl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+5,CLASS_BIT+2,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,376},


/* 0001 0010 0000 dddd imm32 *** subl rrd,imm32 */
{
#ifdef NICENAMES
"subl rrd,imm32",32,14,
0x3c,
#endif
"subl",OPC_subl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_IMM+(ARG_IMM32),},
	{CLASS_BIT+1,CLASS_BIT+2,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM32),0,0,0,0,},2,6,377},


/* 1001 0010 ssss dddd *** subl rrd,rrs */
{
#ifdef NICENAMES
"subl rrd,rrs",32,8,
0x3c,
#endif
"subl",OPC_subl,0,{CLASS_REG_LONG+(ARG_RD),CLASS_REG_LONG+(ARG_RS),},
	{CLASS_BIT+9,CLASS_BIT+2,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,378},


/* 1010 1111 dddd cccc *** tcc cc,rd */
{
#ifdef NICENAMES
"tcc cc,rd",16,5,
0x00,
#endif
"tcc",OPC_tcc,0,{CLASS_CC,CLASS_REG_WORD+(ARG_RD),},
	{CLASS_BIT+0xa,CLASS_BIT+0xf,CLASS_REG+(ARG_RD),CLASS_CC,0,0,0,0,0,},2,2,379},


/* 1010 1110 dddd cccc *** tccb cc,rbd */
{
#ifdef NICENAMES
"tccb cc,rbd",8,5,
0x00,
#endif
"tccb",OPC_tccb,0,{CLASS_CC,CLASS_REG_BYTE+(ARG_RD),},
	{CLASS_BIT+0xa,CLASS_BIT+0xe,CLASS_REG+(ARG_RD),CLASS_CC,0,0,0,0,0,},2,2,380},


/* 0000 1101 ddN0 0100 *** test @rd */
{
#ifdef NICENAMES
"test @rd",16,8,
0x18,
#endif
"test",OPC_test,0,{CLASS_IR+(ARG_RD),},
	{CLASS_BIT+0,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+4,0,0,0,0,0,},1,2,381},


/* 0100 1101 0000 0100 address_dst *** test address_dst */
{
#ifdef NICENAMES
"test address_dst",16,11,
0x00,
#endif
"test",OPC_test,0,{CLASS_DA+(ARG_DST),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_BIT+0,CLASS_BIT+4,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,382},


/* 0100 1101 ddN0 0100 address_dst *** test address_dst(rd) */
{
#ifdef NICENAMES
"test address_dst(rd)",16,12,
0x00,
#endif
"test",OPC_test,0,{CLASS_X+(ARG_RD),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+4,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,383},


/* 1000 1101 dddd 0100 *** test rd */
{
#ifdef NICENAMES
"test rd",16,7,
0x00,
#endif
"test",OPC_test,0,{CLASS_REG_WORD+(ARG_RD),},
	{CLASS_BIT+8,CLASS_BIT+0xd,CLASS_REG+(ARG_RD),CLASS_BIT+4,0,0,0,0,0,},1,2,384},


/* 0000 1100 ddN0 0100 *** testb @rd */
{
#ifdef NICENAMES
"testb @rd",8,8,
0x1c,
#endif
"testb",OPC_testb,0,{CLASS_IR+(ARG_RD),},
	{CLASS_BIT+0,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+4,0,0,0,0,0,},1,2,385},


/* 0100 1100 0000 0100 address_dst *** testb address_dst */
{
#ifdef NICENAMES
"testb address_dst",8,11,
0x1c,
#endif
"testb",OPC_testb,0,{CLASS_DA+(ARG_DST),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_BIT+0,CLASS_BIT+4,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,386},


/* 0100 1100 ddN0 0100 address_dst *** testb address_dst(rd) */
{
#ifdef NICENAMES
"testb address_dst(rd)",8,12,
0x1c,
#endif
"testb",OPC_testb,0,{CLASS_X+(ARG_RD),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+4,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,387},


/* 1000 1100 dddd 0100 *** testb rbd */
{
#ifdef NICENAMES
"testb rbd",8,7,
0x1c,
#endif
"testb",OPC_testb,0,{CLASS_REG_BYTE+(ARG_RD),},
	{CLASS_BIT+8,CLASS_BIT+0xc,CLASS_REG+(ARG_RD),CLASS_BIT+4,0,0,0,0,0,},1,2,388},


/* 0001 1100 ddN0 1000 *** testl @rd */
{
#ifdef NICENAMES
"testl @rd",32,13,
0x18,
#endif
"testl",OPC_testl,0,{CLASS_IR+(ARG_RD),},
	{CLASS_BIT+1,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+8,0,0,0,0,0,},1,2,389},


/* 0101 1100 0000 1000 address_dst *** testl address_dst */
{
#ifdef NICENAMES
"testl address_dst",32,16,
0x18,
#endif
"testl",OPC_testl,0,{CLASS_DA+(ARG_DST),},
	{CLASS_BIT+5,CLASS_BIT+0xc,CLASS_BIT+0,CLASS_BIT+8,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,390},


/* 0101 1100 ddN0 1000 address_dst *** testl address_dst(rd) */
{
#ifdef NICENAMES
"testl address_dst(rd)",32,17,
0x18,
#endif
"testl",OPC_testl,0,{CLASS_X+(ARG_RD),},
	{CLASS_BIT+5,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+8,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,391},


/* 1001 1100 dddd 1000 *** testl rrd */
{
#ifdef NICENAMES
"testl rrd",32,13,
0x18,
#endif
"testl",OPC_testl,0,{CLASS_REG_LONG+(ARG_RD),},
	{CLASS_BIT+9,CLASS_BIT+0xc,CLASS_REG+(ARG_RD),CLASS_BIT+8,0,0,0,0,0,},1,2,392},


/* 1011 1000 ddN0 1000 0000 aaaa ssN0 0000 *** trdb @rd,@rs,rba */
{
#ifdef NICENAMES
"trdb @rd,@rs,rba",8,25,
0x1c,
#endif
"trdb",OPC_trdb,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_BYTE+(ARG_RA),},
	{CLASS_BIT+0xb,CLASS_BIT+8,CLASS_REGN0+(ARG_RD),CLASS_BIT+8,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RS),CLASS_BIT+0,0,},3,4,393},


/* 1011 1000 ddN0 1100 0000 aaaa ssN0 0000 *** trdrb @rd,@rs,rba */
{
#ifdef NICENAMES
"trdrb @rd,@rs,rba",8,25,
0x1c,
#endif
"trdrb",OPC_trdrb,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_BYTE+(ARG_RA),},
	{CLASS_BIT+0xb,CLASS_BIT+8,CLASS_REGN0+(ARG_RD),CLASS_BIT+0xc,CLASS_BIT+0,CLASS_REG+(ARG_RA),CLASS_REGN0+(ARG_RS),CLASS_BIT+0,0,},3,4,394},


/* 1011 1000 ddN0 0000 0000 rrrr ssN0 0000 *** trib @rd,@rs,rbr */
{
#ifdef NICENAMES
"trib @rd,@rs,rbr",8,25,
0x1c,
#endif
"trib",OPC_trib,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_BYTE+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+8,CLASS_REGN0+(ARG_RD),CLASS_BIT+0,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RS),CLASS_BIT+0,0,},3,4,395},


/* 1011 1000 ddN0 0100 0000 rrrr ssN0 0000 *** trirb @rd,@rs,rbr */
{
#ifdef NICENAMES
"trirb @rd,@rs,rbr",8,25,
0x1c,
#endif
"trirb",OPC_trirb,0,{CLASS_IR+(ARG_RD),CLASS_IR+(ARG_RS),CLASS_REG_BYTE+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+8,CLASS_REGN0+(ARG_RD),CLASS_BIT+4,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RS),CLASS_BIT+0,0,},3,4,396},


/* 1011 1000 aaN0 1010 0000 rrrr bbN0 0000 *** trtdb @ra,@rb,rbr */
{
#ifdef NICENAMES
"trtdb @ra,@rb,rbr",8,25,
0x1c,
#endif
"trtdb",OPC_trtdb,0,{CLASS_IR+(ARG_RA),CLASS_IR+(ARG_RB),CLASS_REG_BYTE+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+8,CLASS_REGN0+(ARG_RA),CLASS_BIT+0xa,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RB),CLASS_BIT+0,0,},3,4,397},


/* 1011 1000 aaN0 1110 0000 rrrr bbN0 1110 *** trtdrb @ra,@rb,rbr */
{
#ifdef NICENAMES
"trtdrb @ra,@rb,rbr",8,25,
0x1c,
#endif
"trtdrb",OPC_trtdrb,0,{CLASS_IR+(ARG_RA),CLASS_IR+(ARG_RB),CLASS_REG_BYTE+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+8,CLASS_REGN0+(ARG_RA),CLASS_BIT+0xe,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RB),CLASS_BIT+0xe,0,},3,4,398},


/* 1011 1000 aaN0 0010 0000 rrrr bbN0 0000 *** trtib @ra,@rb,rbr */
{
#ifdef NICENAMES
"trtib @ra,@rb,rbr",8,25,
0x1c,
#endif
"trtib",OPC_trtib,0,{CLASS_IR+(ARG_RA),CLASS_IR+(ARG_RB),CLASS_REG_BYTE+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+8,CLASS_REGN0+(ARG_RA),CLASS_BIT+2,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RB),CLASS_BIT+0,0,},3,4,399},


/* 1011 1000 aaN0 0110 0000 rrrr bbN0 1110 *** trtirb @ra,@rb,rbr */
{
#ifdef NICENAMES
"trtirb @ra,@rb,rbr",8,25,
0x1c,
#endif
"trtirb",OPC_trtirb,0,{CLASS_IR+(ARG_RA),CLASS_IR+(ARG_RB),CLASS_REG_BYTE+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+8,CLASS_REGN0+(ARG_RA),CLASS_BIT+6,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RB),CLASS_BIT+0xe,0,},3,4,400},


/* 1011 1000 aaN0 1010 0000 rrrr bbN0 0000 *** trtrb @ra,@rb,rbr */
{
#ifdef NICENAMES
"trtrb @ra,@rb,rbr",8,25,
0x1c,
#endif
"trtrb",OPC_trtrb,0,{CLASS_IR+(ARG_RA),CLASS_IR+(ARG_RB),CLASS_REG_BYTE+(ARG_RR),},
	{CLASS_BIT+0xb,CLASS_BIT+8,CLASS_REGN0+(ARG_RA),CLASS_BIT+0xa,CLASS_BIT+0,CLASS_REG+(ARG_RR),CLASS_REGN0+(ARG_RB),CLASS_BIT+0,0,},3,4,401},


/* 0000 1101 ddN0 0110 *** tset @rd */
{
#ifdef NICENAMES
"tset @rd",16,11,
0x08,
#endif
"tset",OPC_tset,0,{CLASS_IR+(ARG_RD),},
	{CLASS_BIT+0,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+6,0,0,0,0,0,},1,2,402},


/* 0100 1101 0000 0110 address_dst *** tset address_dst */
{
#ifdef NICENAMES
"tset address_dst",16,14,
0x08,
#endif
"tset",OPC_tset,0,{CLASS_DA+(ARG_DST),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_BIT+0,CLASS_BIT+6,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,403},


/* 0100 1101 ddN0 0110 address_dst *** tset address_dst(rd) */
{
#ifdef NICENAMES
"tset address_dst(rd)",16,15,
0x08,
#endif
"tset",OPC_tset,0,{CLASS_X+(ARG_RD),},
	{CLASS_BIT+4,CLASS_BIT+0xd,CLASS_REGN0+(ARG_RD),CLASS_BIT+6,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,404},


/* 1000 1101 dddd 0110 *** tset rd */
{
#ifdef NICENAMES
"tset rd",16,7,
0x08,
#endif
"tset",OPC_tset,0,{CLASS_REG_WORD+(ARG_RD),},
	{CLASS_BIT+8,CLASS_BIT+0xd,CLASS_REG+(ARG_RD),CLASS_BIT+6,0,0,0,0,0,},1,2,405},


/* 0000 1100 ddN0 0110 *** tsetb @rd */
{
#ifdef NICENAMES
"tsetb @rd",8,11,
0x08,
#endif
"tsetb",OPC_tsetb,0,{CLASS_IR+(ARG_RD),},
	{CLASS_BIT+0,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+6,0,0,0,0,0,},1,2,406},


/* 0100 1100 0000 0110 address_dst *** tsetb address_dst */
{
#ifdef NICENAMES
"tsetb address_dst",8,14,
0x08,
#endif
"tsetb",OPC_tsetb,0,{CLASS_DA+(ARG_DST),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_BIT+0,CLASS_BIT+6,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,407},


/* 0100 1100 ddN0 0110 address_dst *** tsetb address_dst(rd) */
{
#ifdef NICENAMES
"tsetb address_dst(rd)",8,15,
0x08,
#endif
"tsetb",OPC_tsetb,0,{CLASS_X+(ARG_RD),},
	{CLASS_BIT+4,CLASS_BIT+0xc,CLASS_REGN0+(ARG_RD),CLASS_BIT+6,CLASS_ADDRESS+(ARG_DST),0,0,0,0,},1,4,408},


/* 1000 1100 dddd 0110 *** tsetb rbd */
{
#ifdef NICENAMES
"tsetb rbd",8,7,
0x08,
#endif
"tsetb",OPC_tsetb,0,{CLASS_REG_BYTE+(ARG_RD),},
	{CLASS_BIT+8,CLASS_BIT+0xc,CLASS_REG+(ARG_RD),CLASS_BIT+6,0,0,0,0,0,},1,2,409},


/* 0000 1001 ssN0 dddd *** xor rd,@rs */
{
#ifdef NICENAMES
"xor rd,@rs",16,7,
0x18,
#endif
"xor",OPC_xor,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+0,CLASS_BIT+9,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,410},


/* 0100 1001 0000 dddd address_src *** xor rd,address_src */
{
#ifdef NICENAMES
"xor rd,address_src",16,9,
0x18,
#endif
"xor",OPC_xor,0,{CLASS_REG_WORD+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+4,CLASS_BIT+9,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,411},


/* 0100 1001 ssN0 dddd address_src *** xor rd,address_src(rs) */
{
#ifdef NICENAMES
"xor rd,address_src(rs)",16,10,
0x18,
#endif
"xor",OPC_xor,0,{CLASS_REG_WORD+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+4,CLASS_BIT+9,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,412},


/* 0000 1001 0000 dddd imm16 *** xor rd,imm16 */
{
#ifdef NICENAMES
"xor rd,imm16",16,7,
0x18,
#endif
"xor",OPC_xor,0,{CLASS_REG_WORD+(ARG_RD),CLASS_IMM+(ARG_IMM16),},
	{CLASS_BIT+0,CLASS_BIT+9,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM16),0,0,0,0,},2,4,413},


/* 1000 1001 ssss dddd *** xor rd,rs */
{
#ifdef NICENAMES
"xor rd,rs",16,4,
0x18,
#endif
"xor",OPC_xor,0,{CLASS_REG_WORD+(ARG_RD),CLASS_REG_WORD+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+9,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,414},


/* 0000 1000 ssN0 dddd *** xorb rbd,@rs */
{
#ifdef NICENAMES
"xorb rbd,@rs",8,7,
0x1c,
#endif
"xorb",OPC_xorb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IR+(ARG_RS),},
	{CLASS_BIT+0,CLASS_BIT+8,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,415},


/* 0100 1000 0000 dddd address_src *** xorb rbd,address_src */
{
#ifdef NICENAMES
"xorb rbd,address_src",8,9,
0x1c,
#endif
"xorb",OPC_xorb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_DA+(ARG_SRC),},
	{CLASS_BIT+4,CLASS_BIT+8,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,416},


/* 0100 1000 ssN0 dddd address_src *** xorb rbd,address_src(rs) */
{
#ifdef NICENAMES
"xorb rbd,address_src(rs)",8,10,
0x1c,
#endif
"xorb",OPC_xorb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_X+(ARG_RS),},
	{CLASS_BIT+4,CLASS_BIT+8,CLASS_REGN0+(ARG_RS),CLASS_REG+(ARG_RD),CLASS_ADDRESS+(ARG_SRC),0,0,0,0,},2,4,417},


/* 0000 1000 0000 dddd imm8 imm8 *** xorb rbd,imm8 */
{
#ifdef NICENAMES
"xorb rbd,imm8",8,7,
0x1c,
#endif
"xorb",OPC_xorb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_IMM+(ARG_IMM8),},
	{CLASS_BIT+0,CLASS_BIT+8,CLASS_BIT+0,CLASS_REG+(ARG_RD),CLASS_IMM+(ARG_IMM8),CLASS_IMM+(ARG_IMM8),0,0,0,},2,4,418},


/* 1000 1000 ssss dddd *** xorb rbd,rbs */
{
#ifdef NICENAMES
"xorb rbd,rbs",8,4,
0x1c,
#endif
"xorb",OPC_xorb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+8,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,419},


/* 1000 1000 ssss dddd *** xorb rbd,rbs */
{
#ifdef NICENAMES
"xorb rbd,rbs",8,4,
0x01,
#endif
"xorb",OPC_xorb,0,{CLASS_REG_BYTE+(ARG_RD),CLASS_REG_BYTE+(ARG_RS),},
	{CLASS_BIT+8,CLASS_BIT+8,CLASS_REG+(ARG_RS),CLASS_REG+(ARG_RD),0,0,0,0,0,},2,2,420},

/* end marker */
{
#ifdef NICENAMES
NULL,0,0,
0,
#endif
NULL,0,0,{0,0,0,0},{0,0,0,0,0,0,0,0,0,0},0,0,0}
};
#endif
