/* IBM RS/6000 instruction set definitions, for GNU software.  */

/* These are all possible instruction formats as used in IBM Assembler
   Language Reference, Appendix A. */

typedef enum { A=0, B, D, I, M, SC, X, XL, XO, XFL, XFX } InsnFmt;

/* Extended opcode masks. Used for extracting extended opcode values from
   instructions. Each instruction's format decides which mask applies. 
   They *should* retain the same order as the above formats. */
      
static int eopMask[] =
	{ 0x1f, 0, 0, 0, 0, 0, 0x3ff, 0x3ff, 0x1ff, 0x3ff, 0x3ff };
	     
/* All the things you need to know about an opcode. */

typedef struct rs6000_insn {
  char	*operator;			/* opcode name		*/
  char	*opr_ext;			/* opcode name extension */
  InsnFmt format;			/* opcode format	*/
  char	p_opcode;			/* primary opcode	*/
  int	e_opcode;			/* extended opcode	*/
  char	oprnd_format[6];		/* operand format	*/
} OPCODE;

/* operand format specifiers */

#define	TO	1
#define	RA	2
#define	SI	3
#define	RT	4
#define	UI	5
#define	BF	6
#define	BFA	7
#define	BT	8
#define	BA	9
#define	BB	10
#define	BO	11
#define	BI	12
#define	RB	13
#define	RS	14
#define	SH	15
#define	MB	16
#define	ME	17
#define	SPR	18
#define	DIS	19
#define	FXM	21
#define	FRT	22
#define	NB	23
#define	FRS	24
#define	FRA	25
#define	FRB	26
#define	FRC	27
#define	FLM	28
#define	I	29
#define	LI	30
#define	A2	31
#define	TA14	32		/* 14 bit representation of target address */
#define	TA24	33		/* 24 bit representation of target address */
#define	FL1	34
#define	FL2	35
#define	LEV	36

/*	RS/6000 INSTRUCTION SET
    (sorted on primary and extended opcode)

	     oprtr	      primary  ext.
operator      ext     format  opcode   opcode   operand format
-------	    -------   ------  -------  ------   ---------------   */

struct rs6000_insn rs6k_ops [] = {

{"ti",		0,	D,	3,	-1,	{TO,RA,SI,0}	},
{"muli",	0,	D,	7,	-1,	{RT,RA,SI,0}	},
{"sfi",		0,	D,	8,	-1,	{RT,RA,SI,0}	},
{"dozi",	0,	D,	9,	-1,	{RT,RA,SI,0}	},
{"cmpli",	0,	D,	10,	-1,	{BF,RA,UI,0}	},
{"cmpi",	0,	D,	11,	-1,	{BF,RA,SI,0}	},
{"ai",		0,	D,	12,	-1,	{RT,RA,SI,0}	},
{"ai.",		0,	D,	13,	-1,	{RT,RA,SI,0}	},
{"lil",		0,	D,	14,	-1,	{RT,SI,0}	}, /* same as `cal' */
{"cal",		0,	D,	14,	-1,	{RT,DIS,RA,0}	},
{"liu",		0,	D,	15,	-1,	{RT, UI,0}	}, /* same as `cau' */
{"cau",		0,	D,	15,	-1,	{RT,RA,UI,0}	},

/* "1" indicates an exception--"bb" is only usable for some values of
   BO, so the disassembler first matches this instruction and then changes
   it to "bc" if that is the case.  */
{"bb",		"1tfla",	B,	16,	-1,	{LI,A2,0}	},
{"bc",		"la",	B,	16,	-1,	{BO,BI,TA14,0}	},

{"svc",		"la",	SC,	17,	-1,	{LEV,FL1,FL2,0}	},
{"b",		"la",	I,	18,	-1,	{TA24,0}	},
{"mcrf",	0,	XL,	19,	0,	{BF,BFA,0}	},
{"bcr",		"l",	XL,	19,	16,	{BO,BI,0}	},
{"crnor",	0,	XL,	19,	33,	{BT,BA,BB,0}	},
{"rfi",		0,	X,	19,	50,	{0}		},
{"rfsvc",	0,	X,	19,	82,	{0}		},
{"crandc",	0,	XL,	19,	129,	{BT,BA,BB,0}	},
{"ics",		0,	X,	19,	150,	{0}		},
{"crxor",	0,	XL,	19,	193,	{BT,BA,BB,0}	},
{"crnand",	0,	XL,	19,	225,	{BT,BA,BB,0}	},
{"crand",	0,	XL,	19,	257,	{BT,BA,BB,0}	},
{"creqv",	0,	XL,	19,	289,	{BT,BA,BB,0}	},
{"crorc",	0,	XL,	19,	417,	{BT,BA,BB,0}	},
{"cror",	0,	XL,	19,	449,	{BT,BA,BB,0}	},
{"bcc",		"l",	XL,	19,	528,	{BO,BI,0}	},
{"rlimi",	".",	M,	20,	-1,	{RA,RS,SH,MB,ME,0} /*??*/},
{"rlinm",	".",	M,	21,	-1,	{RA,RS,SH,MB,ME,0} /*??*/},
{"rlmi",	".",	M,	22,	-1,	{RA,RS,RB,MB,ME,0} /*??*/},
{"rlnm",	".",	M,	23,	-1,	{RA,RS,RB,MB,ME,0} /*??*/},
{"oril",	0,	D,	24,	-1,	{RA,RS,UI,0}	},
{"oriu",	0,	D,	25,	-1,	{RA,RS,UI,0}	},
{"xoril",	0,	D,	26,	-1,	{RA,RS,UI,0}	},
{"xoriu",	0,	D,	27,	-1,	{RA,RS,UI,0}	},
{"andil.",	0,	D,	28,	-1,	{RA,RS,UI,0}	},
{"andiu.",	0,	D,	29,	-1,	{RA,RS,UI,0}	},
{"cmp",		0,	X,	31,	0,	{BF,RA,RB,0}	},
{"t",		0,	X,	31,	4,	{TO,RA,RB,0}	},
{"sf",		"o.",	XO,	31,	8,	{RT,RA,RB,0}	},
{"a",		"o.",	XO,	31,	10,	{RT,RA,RB,0}	},
{"mfcr",	0,	X,	31,	19,	{RT,0}		},
{"lx",		0,	X,	31,	23,	{RT,RA,RB,0}	},
{"sl",		".",	X,	31,	24,	{RA,RS,RB,0}	},
{"cntlz",	".",	XO,	31,	26,	{RA,RS,0}	},
{"and",		".",	X,	31,	28,	{RA,RS,RB,0}	},
{"maskg",	".",	X,	31,	29,	{RA,RS,RB,0}	},
{"cmpl",	0,	X,	31,	32,	{BF,RA,RB,0}	},
{"sfe",		"o.",	XO,	31,	136,	{RT,RA,RB,0}	},
{"lux",		0,	X,	31,	55,	{RT,RA,RB,0}	},
{"andc",	".",	X,	31,	60,	{RA,RS,RB,0}	},
{"mfmsr",	0,	X,	31,	83,	{RT,0}		},
{"lbzx",	0,	X,	31,	87,	{RT,RA,RB,0}	},
{"neg",		"o.",	XO,	31,	104,	{RT,RA,0}	},
{"mul",		"o.",	XO,	31,	107,	{RT,RA,RB,0}	},
{"lbzux",	0,	X,	31,	119,	{RT,RA,RB,0}	},
{"nor",		".",	X,	31,	124,	{RA,RS,RB,0}	},
{"ae",		"o.",	XO,	31,	138,	{RT,RA,RB,0}	},
{"mtcrf",	0,	XFX,	31,	144,	{FXM,RS,0}	},
{"stx",		0,	X,	31,	151,	{RS,RA,RB,0}	},
{"slq",		".",	X,	31,	152,	{RA,RS,RB,0}	},
{"sle",		".",	X,	31,	153,	{RA,RS,RB,0}	},
{"stux",	0,	X,	31,	183,	{RS,RA,RB,0}	},
{"sliq",	".",	X,	31,	184,	{RA,RS,SH,0}	},
{"sfze",	"o.",	XO,	31,	200,	{RT,RA,0}	},
{"aze",		"o.",	XO,	31,	202,	{RT,RA,0}	},
{"stbx",	0,	X,	31,	215,	{RS,RA,RB,0}	},
{"sllq",	".",	X,	31,	216,	{RA,RS,RB,0}	},
{"sleq",	".",	X,	31,	217,	{RA,RS,RB,0}	},
{"sfme",	"o.",	XO,	31,	232,	{RT,RA,0}	},
{"ame",		"o.",	XO,	31,	234,	{RT,RA,0}	},
{"muls",	"o.",	XO,	31,	235,	{RT,RA,RB,0}	},
{"stbux",	0,	X,	31,	247,	{RS,RA,RB,0}	},
{"slliq", 	".",	X,	31,	248,	{RA,RS,SH,0}	},
{"doz",		"o.",	X,	31,	264,	{RT,RA,RB,0}	},
{"cax",		"o.",	XO,	31,	266,	{RT,RA,RB,0}	},
{"lscbx",	".",	X,	31,	277,	{RT,RA,RB,0}	},
{"lhzx",	0,	X,	31,	279,	{RT,RA,RB,0}	},
{"eqv",		".",	X,	31,	284,	{RA,RS,RB,0}	},
{"lhzux",	0,	X,	31,	311,	{RT,RA,RB,0}	},
{"xor",		".",	X,	31,	316,	{RA,RS,RB,0}	},
{"div",		"o.",	XO,	31,	331,	{RT,RA,RB,0}	},
{"mfspr",	0,	X,	31,	339,	{RT,SPR,0}	},
{"lhax",	0,	X,	31,	343,	{RT,RA,RB,0}	},
{"abs",		"o.",	XO,	31,	360,	{RT,RA,0}	},
{"divs",	"o.",	XO,	31,	363,	{RT,RA,RB,0}	},
{"lhaux",	0,	X,	31,	375,	{RT,RA,RB,0}	},
{"sthx",	0,	X,	31,	407,	{RS,RA,RB,0}	},
{"orc",		".",	X,	31,	412,	{RA,RS,RB,0}	},
{"sthux",	0,	X,	31,	439,	{RS,RA,RB,0}	},
{"or",		".",	X,	31,	444,	{RA,RS,RB,0}	},
{"mtspr",	0,	X,	31,	467,	{SPR,RS,0}	},
{"nand",	".",	X,	31,	476,	{RA,RS,RB,0}	},
{"nabs",	"o.",	XO,	31,	488,	{RT,RA,0}	},
{"mcrxr",	0,	X,	31,	512,	{BF,0}		},
{"lsx",		0,	X,	31,	533,	{RT,RA,RB,0}	},
{"lbrx",	0,	X,	31,	534,	{RT,RA,RB,0}	},
{"lfsx",	0,	X,	31,	535,	{FRT,RA,RB,0}	},
{"sr",		".",	X,	31,	536,	{RA,RS,RB,0}	},
{"rrib",	".",	X,	31,	537,	{RA,RS,RB,0}	},
{"maskir",	".",	X,	31,	541,	{RA,RS,RB,0}	},
{"lfsux",	0,	X,	31,	567,	{FRT,RA,RB,0}	},
{"lsi",		0,	X,	31,	597,	{RT,RA,NB,0}	},
{"lfdx",	0,	X,	31,	599,	{FRT,RA,RB,0}	},
{"lfdux",	0,	X,	31,	631,	{FRT,RA,RB,0}	},
{"stsx",	0,	X,	31,	661,	{RS,RA,RB,0}	},
{"stbrx",	0,	X,	31,	662,	{RS,RA,RB,0}	},
{"stfsx",	0,	X,	31,	663,	{FRS,RA,RB,0}	},
{"srq",		".",	X,	31,	664,	{RA,RS,RB,0}	},
{"sre",		".",	X,	31,	665,	{RA,RS,RB,0}	},
{"stfsux",	0,	X,	31,	695,	{FRS,RA,RB,0}	},
{"sriq",	".",	X,	31,	696,	{RA,RS,SH,0}	},
{"stsi",	0,	X,	31,	725,	{RS,RA,NB,0}	},
{"stfdx",	0,	X,	31,	727,	{FRS,RA,RB,0}	},
{"srlq",	".",	X,	31,	728,	{RA,RS,RB,0}	},
{"sreq",	".",	X,	31,	729,	{RA,RS,RB,0}	},
{"stfdux",	0,	X,	31,	759,	{FRS,RA,RB,0}	},
{"srliq",	".",	X,	31,	760,	{RA,RS,SH,0}	},
{"lhbrx",	0,	X,	31,	790,	{RT,RA,RB,0}	},
{"sra",		".",	X,	31,	792,	{RA,RS,RB,0}	},
{"srai",	".",	X,	31,	824,	{RA,RS,SH,0}	},
{"sthbrx",	0,	X,	31,	918,	{RS,RA,RB,0}	},
{"sraq",	".",	X,	31,	920,	{RA,RS,RB,0}	},
{"srea",	".",	X,	31,	921,	{RA,RS,RB,0}	},
{"exts",	".",	X,	31,	922,	{RA,RS,0}	},
{"sraiq",	".",	X,	31,	952,	{RA,RS,SH,0}	},
{"l",		0,	D,	32,	-1,	{RT,DIS,RA,0}	},
{"lu",		0,	D,	33,	-1,	{RT,DIS,RA,0}	},
{"lbz",		0,	D,	34,	-1,	{RT,DIS,RA,0}	},
{"lbzu",	0,	D,	35,	-1,	{RT,DIS,RA,0}	},
{"st",		0,	D,	36,	-1,	{RS,DIS,RA,0}	},
{"stu",		0,	D,	37,	-1,	{RS,DIS,RA,0}	},
{"stb",		0,	D,	38,	-1,	{RS,DIS,RA,0}	},
{"stbu",	0,	D,	39,	-1,	{RS,DIS,RA,0}	},
{"lhz",		0,	D,	40,	-1,	{RT,DIS,RA,0}	},
{"lhzu",	0,	D,	41,	-1,	{RT,DIS,RA,0}	},
{"lha",		0,	D,	42,	-1,	{RT,DIS,RA,0}	},
{"lhau",	0,	D,	43,	-1,	{RT,DIS,RA,0}	},
{"sth",		0,	D,	44,	-1,	{RS,DIS,RA,0}	},
{"sthu",	0,	D,	45,	-1,	{RS,DIS,RA,0}	},
{"lm",		0,	D,	46,	-1,	{RT,DIS,RA,0}	},
{"stm",		0,	D,	47,	-1,	{RS,DIS,RA,0}	},
{"lfs",		0,	D,	48,	-1,	{FRT,DIS,RA,0}	},
{"lfsu",	0,	D,	49,	-1,	{FRT,DIS,RA,0}	},
{"lfd",		0,	D,	50,	-1,	{FRT,DIS,RA,0}	},
{"lfdu",	0,	D,	51,	-1,	{FRT,DIS,RA,0}	},
{"stfs",	0,	D,	52,	-1,	{FRS,DIS,RA,0}	},
{"stfsu",	0,	D,	53,	-1,	{FRS,DIS,RA,0}	},
{"stfd",	0,	D,	54,	-1,	{FRS,DIS,RA,0}	},
{"stfdu",	0,	D,	55,	-1,	{FRS,DIS,RA,0}	},
{"fcmpu",	0,	X,	63,	0,	{BF,FRA,FRB,0}	},
{"frsp",	".",	X,	63,	12,	{FRT,FRB,0}	},
{"fd",		".",	A,	63,	18,	{FRT,FRA,FRB,0}	},
{"fs",		".",	A,	63,	20,	{FRT,FRA,FRB,0}	},
{"fa",		".",	A,	63,	21,	{FRT,FRA,FRB,0}	},
{"fm",		".",	A,	63,	25,	{FRT,FRA,FRC,0}	},
{"fms",		".",	A,	63,	28,	{FRT,FRA,FRC,FRB,0}	},
{"fma",		".",	A,	63,	29,	{FRT,FRA,FRC,FRB,0}	},
{"fnms",	".",	A,	63,	30,	{FRT,FRA,FRC,FRB,0}	},
{"fnma",	".",	A,	63,	31,	{FRT,FRA,FRC,FRB,0}	},
{"fcmpo",	0,	X,	63,	32,	{BF,FRA,FRB,0}	},
{"mtfsb1",	".",	X,	63,	38,	{BT,0}		},
{"fneg",	".",	X,	63,	40,	{FRT,FRB,0}	},
{"mcrfs",	0,	X,	63,	64,	{BF,BFA,0}	},
{"mtfsb0",	".",	X,	63,	70,	{BT,0}		},
{"fmr",		".",	X,	63,	72,	{FRT,FRB,0}	},
{"mtfsfi",	".",	X,	63,	134,	{BF,I,0}	},
{"fnabs",	".",	X,	63,	136,	{FRT,FRB,0}	},
{"fabs",	".",	X,	63,	264,	{FRT,FRB,0}	},
{"mffs",	".",	X,	63,	583,	{FRT,0}		},
{"mtfsf",	".",	XFL,	63,	711,	{FLM,FRB,0}	},
};

#define	NOPCODES	(sizeof (rs6k_ops) / sizeof (struct rs6000_insn))
