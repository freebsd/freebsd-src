# define SEOS 1
# define SCOMMENT 2
# define SLABEL 3
# define SUNKNOWN 4
# define SHOLLERITH 5
# define SICON 6
# define SRCON 7
# define SDCON 8
# define SBITCON 9
# define SOCTCON 10
# define SHEXCON 11
# define STRUE 12
# define SFALSE 13
# define SNAME 14
# define SNAMEEQ 15
# define SFIELD 16
# define SSCALE 17
# define SINCLUDE 18
# define SLET 19
# define SASSIGN 20
# define SAUTOMATIC 21
# define SBACKSPACE 22
# define SBLOCK 23
# define SCALL 24
# define SCHARACTER 25
# define SCLOSE 26
# define SCOMMON 27
# define SCOMPLEX 28
# define SCONTINUE 29
# define SDATA 30
# define SDCOMPLEX 31
# define SDIMENSION 32
# define SDO 33
# define SDOUBLE 34
# define SELSE 35
# define SELSEIF 36
# define SEND 37
# define SENDFILE 38
# define SENDIF 39
# define SENTRY 40
# define SEQUIV 41
# define SEXTERNAL 42
# define SFORMAT 43
# define SFUNCTION 44
# define SGOTO 45
# define SASGOTO 46
# define SCOMPGOTO 47
# define SARITHIF 48
# define SLOGIF 49
# define SIMPLICIT 50
# define SINQUIRE 51
# define SINTEGER 52
# define SINTRINSIC 53
# define SLOGICAL 54
# define SNAMELIST 55
# define SOPEN 56
# define SPARAM 57
# define SPAUSE 58
# define SPRINT 59
# define SPROGRAM 60
# define SPUNCH 61
# define SREAD 62
# define SREAL 63
# define SRETURN 64
# define SREWIND 65
# define SSAVE 66
# define SSTATIC 67
# define SSTOP 68
# define SSUBROUTINE 69
# define STHEN 70
# define STO 71
# define SUNDEFINED 72
# define SWRITE 73
# define SLPAR 74
# define SRPAR 75
# define SEQUALS 76
# define SCOLON 77
# define SCOMMA 78
# define SCURRENCY 79
# define SPLUS 80
# define SMINUS 81
# define SSTAR 82
# define SSLASH 83
# define SPOWER 84
# define SCONCAT 85
# define SAND 86
# define SOR 87
# define SNEQV 88
# define SEQV 89
# define SNOT 90
# define SEQ 91
# define SLT 92
# define SGT 93
# define SLE 94
# define SGE 95
# define SNE 96
# define SENDDO 97
# define SWHILE 98
# define SSLASHD 99

/* # line 124 "gram.in" */
#include "defs.h"
#include "p1defs.h"

static int nstars;			/* Number of labels in an
					   alternate return CALL */
static int datagripe;
static int ndim;
static int vartype;
int new_dcl;
static ftnint varleng;
static struct Dims dims[MAXDIM+1];
extern struct Labelblock **labarray;	/* Labels in an alternate
						   return CALL */
extern int maxlablist;

/* The next two variables are used to verify that each statement might be reached
   during runtime.   lastwasbranch   is tested only in the defintion of the
   stat:   nonterminal. */

int lastwasbranch = NO;
static int thiswasbranch = NO;
extern ftnint yystno;
extern flag intonly;
static chainp datastack;
extern long laststfcn, thisstno;
extern int can_include;	/* for netlib */

ftnint convci();
Addrp nextdata();
expptr mklogcon(), mkaddcon(), mkrealcon(), mkstrcon(), mkbitcon();
expptr mkcxcon();
struct Listblock *mklist();
struct Listblock *mklist();
struct Impldoblock *mkiodo();
Extsym *comblock();
#define ESNULL (Extsym *)0
#define NPNULL (Namep)0
#define LBNULL (struct Listblock *)0
extern void freetemps(), make_param();

 static void
pop_datastack() {
	chainp d0 = datastack;
	if (d0->datap)
		curdtp = (chainp)d0->datap;
	datastack = d0->nextp;
	d0->nextp = 0;
	frchain(&d0);
	}


/* # line 178 "gram.in" */
typedef union 	{
	int ival;
	ftnint lval;
	char *charpval;
	chainp chval;
	tagptr tagval;
	expptr expval;
	struct Labelblock *labval;
	struct Nameblock *namval;
	struct Eqvchain *eqvval;
	Extsym *extval;
	} YYSTYPE;
#define yyclearin yychar = -1
#define yyerrok yyerrflag = 0
extern int yychar;
typedef int yytabelem;
extern yytabelem yyerrflag;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
YYSTYPE yylval, yyval;
# define YYERRCODE 256
yytabelem yyexca[] ={
-1, 1,
	0, -1,
	-2, 0,
-1, 20,
	1, 38,
	-2, 228,
-1, 24,
	1, 42,
	-2, 228,
-1, 122,
	6, 240,
	-2, 228,
-1, 150,
	1, 244,
	-2, 188,
-1, 174,
	1, 265,
	78, 265,
	-2, 188,
-1, 223,
	77, 173,
	-2, 139,
-1, 245,
	74, 228,
	-2, 225,
-1, 271,
	1, 286,
	-2, 143,
-1, 275,
	1, 295,
	78, 295,
	-2, 145,
-1, 328,
	77, 174,
	-2, 141,
-1, 358,
	1, 267,
	14, 267,
	74, 267,
	78, 267,
	-2, 189,
-1, 436,
	91, 0,
	92, 0,
	93, 0,
	94, 0,
	95, 0,
	96, 0,
	-2, 153,
-1, 453,
	1, 289,
	78, 289,
	-2, 143,
-1, 455,
	1, 291,
	78, 291,
	-2, 143,
-1, 457,
	1, 293,
	78, 293,
	-2, 143,
-1, 459,
	1, 296,
	78, 296,
	-2, 144,
-1, 504,
	78, 289,
	-2, 143,
	};
# define YYNPROD 301
# define YYLAST 1346
yytabelem yyact[]={

 237, 274, 471, 317, 316, 412, 420, 297, 470, 399,
 413, 397, 386, 357, 398, 266, 128, 356, 273, 252,
 292,   5, 116, 295, 326, 303, 222,  99, 184, 121,
 195, 229,  17, 203, 270, 304, 313, 199, 201, 118,
  94, 202, 396, 104, 210, 183, 236, 101, 106, 234,
 264, 103, 111, 336, 260,  95,  96,  97, 165, 166,
 334, 335, 336, 395, 105, 311, 309, 190, 130, 131,
 132, 133, 120, 135, 119, 114, 157, 129, 157, 475,
 103, 272, 334, 335, 336, 396, 521, 103, 278, 483,
 535, 165, 166, 334, 335, 336, 342, 341, 340, 339,
 338, 137, 343, 345, 344, 347, 346, 348, 450, 258,
 259, 260, 539, 165, 166, 258, 259, 260, 261, 525,
 102, 522, 155, 409, 155, 186, 187, 103, 408, 117,
 165, 166, 258, 259, 260, 318, 100, 527, 484, 188,
 446, 185, 480, 230, 240, 240, 194, 193, 290, 120,
 211, 119, 462, 481, 157, 294, 482, 257, 157, 243,
 468, 214, 463, 469, 461, 464, 460, 239, 241, 220,
 215, 218, 157, 219, 213, 165, 166, 334, 335, 336,
 342, 341, 340, 157, 371, 452, 343, 345, 344, 347,
 346, 348, 443, 428, 377, 294, 102, 102, 102, 102,
 155, 189, 447, 149, 155, 446, 192, 103,  98, 196,
 197, 198, 277, 376, 320, 321, 206, 288, 155, 289,
 300, 375, 299, 324, 315, 328, 275, 275, 330, 155,
 310, 333, 196, 216, 217, 350, 269, 207, 308, 352,
 353, 333, 100, 177, 354, 349, 323, 112, 245, 257,
 247, 110, 157, 417, 286, 287, 418, 362, 157, 157,
 157, 157, 157, 257, 257, 109, 108, 268, 279, 280,
 281, 265, 107, 355,   4, 333, 427, 465, 378, 370,
 170, 172, 176, 257, 165, 166, 258, 259, 260, 261,
 102, 406, 232, 293, 407, 381, 422, 390, 155, 400,
 391, 223, 419, 422, 155, 155, 155, 155, 155, 117,
 221, 314, 392, 319, 387, 359, 372, 196, 360, 373,
 374, 333, 333, 536, 350, 333, 275, 250, 424, 333,
 405, 333, 410, 532, 230, 432, 433, 434, 435, 436,
 437, 438, 439, 440, 441, 403, 331, 156, 401, 332,
 531, 333, 530, 333, 333, 333, 388, 526, 380, 529,
 524, 157, 257, 333, 431, 492, 257, 257, 257, 257,
 257, 382, 383, 235, 426, 384, 358, 494, 296, 333,
 448, 165, 166, 258, 259, 260, 261, 451, 165, 166,
 258, 259, 260, 261, 103, 445, 472, 400, 421, 191,
 402, 196, 103, 150, 307, 174, 285, 155, 474, 246,
 476, 416, 467, 466, 242, 226, 223, 200, 212, 136,
 209, 486, 171, 488, 490, 275, 275, 275, 141, 240,
 496, 429, 329, 333, 333, 333, 333, 333, 333, 333,
 333, 333, 333, 403, 497, 479, 401, 403, 487, 154,
 257, 154, 495, 493, 306, 485, 502, 454, 456, 458,
 500, 491, 268, 499, 505, 506, 507, 103, 451, 271,
 271, 472,  30, 333, 414, 501, 400, 508, 511, 509,
 387, 244, 208, 510, 516, 514, 515, 333, 517, 333,
 513, 333, 520, 293, 518, 225, 240, 333, 402, 523,
  92, 248, 402, 528,   6, 262, 123, 249,  81,  80,
 275, 275, 275,  79, 534, 533, 479,  78, 173, 263,
 314,  77, 403,  76, 537, 401, 351, 154,  75, 333,
 282, 154,  60,  49,  48, 333,  45,  33, 333, 538,
 113, 205, 454, 456, 458, 154, 267, 165, 166, 334,
 335, 336, 342, 540, 503, 411, 154, 204, 394, 393,
 298, 478, 503, 503, 503, 134, 389, 312, 115, 379,
  26,  25,  24,  23, 302,  22, 305, 402,  21, 385,
 284,   9, 503,   8,   7,   2, 519, 301,  20, 319,
 164,  51, 489, 291, 228, 327, 325, 415,  91, 361,
 255,  53, 337,  19,  55, 365, 366, 367, 368, 369,
  37, 224,   3,   1,   0, 351,   0,   0,   0,   0,
   0,   0,   0,   0,   0, 154,   0,   0,   0,   0,
   0, 154, 154, 154, 154, 154,   0,   0,   0, 267,
   0, 512, 267, 267, 165, 166, 334, 335, 336, 342,
 341, 340, 339, 338,   0, 343, 345, 344, 347, 346,
 348, 165, 166, 334, 335, 336, 342, 341, 453, 455,
 457,   0, 343, 345, 344, 347, 346, 348,   0,   0,
 305,   0, 459,   0,   0,   0,   0, 165, 166, 334,
 335, 336, 342, 341, 340, 339, 338, 351, 343, 345,
 344, 347, 346, 348, 444,   0,   0,   0, 449, 165,
 166, 334, 335, 336, 342, 341, 340, 339, 338,   0,
 343, 345, 344, 347, 346, 348, 165, 166, 334, 335,
 336, 342,   0,   0, 154,   0, 498, 343, 345, 344,
 347, 346, 348,   0,   0, 267,   0,   0,   0,   0,
   0, 442,   0, 504, 455, 457, 165, 166, 334, 335,
 336, 342, 341, 340, 339, 338,   0, 343, 345, 344,
 347, 346, 348,   0,   0,   0,   0,   0,   0, 430,
   0, 477,   0, 305, 165, 166, 334, 335, 336, 342,
 341, 340, 339, 338,   0, 343, 345, 344, 347, 346,
 348, 423,   0,   0,   0,   0, 165, 166, 334, 335,
 336, 342, 341, 340, 339, 338,   0, 343, 345, 344,
 347, 346, 348,   0,   0,   0, 267,   0,   0,   0,
   0, 165, 166, 334, 335, 336, 342, 341, 340, 339,
 338,  12, 343, 345, 344, 347, 346, 348,   0,   0,
   0,   0,   0,   0, 305,  10,  56,  46,  73,  85,
  14,  61,  70,  90,  38,  66,  47,  42,  68,  72,
  31,  67,  35,  34,  11,  87,  36,  18,  41,  39,
  28,  16,  57,  58,  59,  50,  54,  43,  88,  64,
  40,  69,  44,  89,  29,  62,  84,  13,   0,  82,
  65,  52,  86,  27,  74,  63,  15,   0,   0,  71,
  83, 160, 161, 162, 163, 169, 168, 167, 158, 159,
 103,   0, 160, 161, 162, 163, 169, 168, 167, 158,
 159, 103,   0,   0,  32, 160, 161, 162, 163, 169,
 168, 167, 158, 159, 103,   0, 160, 161, 162, 163,
 169, 168, 167, 158, 159, 103,   0, 160, 161, 162,
 163, 169, 168, 167, 158, 159, 103,   0, 160, 161,
 162, 163, 169, 168, 167, 158, 159, 103,   0,   0,
 233,   0,   0,   0,   0,   0, 165, 166, 363,   0,
 364, 233, 227,   0,   0,   0, 238, 165, 166, 231,
   0,   0,   0,   0, 233,   0,   0, 238,   0,   0,
 165, 166, 473,   0,   0, 233,   0,   0,   0,   0,
 238, 165, 166, 231,   0,   0, 233,   0,   0,   0,
   0, 238, 165, 166, 425,   0,   0, 233,   0,   0,
   0,   0, 238, 165, 166,   0,   0,   0,   0,   0,
   0,   0,   0, 238, 160, 161, 162, 163, 169, 168,
 167, 158, 159, 103,   0, 160, 161, 162, 163, 169,
 168, 167, 158, 159, 103, 160, 161, 162, 163, 169,
 168, 167, 158, 159, 103,   0,   0,   0, 160, 161,
 162, 163, 169, 168, 167, 158, 159, 103, 256,   0,
  93, 160, 161, 162, 163, 169, 168, 167, 158, 159,
 103,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0, 276,   0,   0,   0,   0,   0, 165,
 166,   0, 122,   0, 322, 125, 126, 127,   0, 238,
 165, 166,   0,   0,   0,   0,   0, 138, 139,   0,
 238, 140,   0, 142, 143, 144,   0, 251, 145, 146,
 147,   0, 148, 165, 166, 253,   0, 254,   0,   0,
 153,   0,   0,   0,   0,   0, 165, 166, 151,   0,
 152, 178, 179, 180, 181, 182, 160, 161, 162, 163,
 169, 168, 167, 158, 159, 103, 160, 161, 162, 163,
 169, 168, 167, 158, 159, 103, 160, 161, 162, 163,
 169, 168, 167, 158, 159, 103, 160, 161, 162, 163,
 169, 168, 167, 158, 159, 103,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0, 251,   0,   0,   0,   0,
   0, 165, 166, 283,   0, 153,   0,   0,   0,   0,
   0, 165, 166, 175,   0, 404,   0,   0,   0,   0,
   0, 165, 166,  56,  46, 251,  85,   0,  61,   0,
  90, 165, 166,  47,  73,   0,   0,   0,  70,   0,
   0,  66,  87,   0,  68,  72,   0,  67,   0,  57,
  58,  59,  50,   0,   0,  88,   0,   0,   0,   0,
  89,   0,  62,  84,   0,  64,  82,  69,  52,  86,
   0,   0,  63,   0, 124,   0,  65,  83,   0,   0,
  74,   0,   0,   0,   0,  71 };
yytabelem yypact[]={

-1000,  18, 503, 837,-1000,-1000,-1000,-1000,-1000,-1000,
 495,-1000,-1000,-1000,-1000,-1000,-1000, 164, 453, -35,
 194, 188, 187, 173,  58, 169,  -8,  66,-1000,-1000,
-1000,-1000,-1000,1264,-1000,-1000,-1000,  -5,-1000,-1000,
-1000,-1000,-1000,-1000,-1000, 453,-1000,-1000,-1000,-1000,
-1000, 354,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,1096, 348,1191, 348, 165,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000,-1000,-1000, 453, 453, 453, 453,-1000, 453,
-1000, 325,-1000,-1000, 453,-1000, -11, 453, 453, 453,
 343,-1000,-1000,-1000, 453, 159,-1000,-1000,-1000,-1000,
 468, 346,  58,-1000,-1000, 344,-1000,-1000,-1000,-1000,
  66, 453, 453, 343,-1000,-1000, 234, 342, 489,-1000,
 341, 917, 963, 963, 340, 475, 453, 335, 453,-1000,
-1000,-1000,-1000,1083,-1000,-1000, 308,1211,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,1083, 193, 158,-1000,-1000,1049,1049,-1000,-1000,
-1000,-1000,1181, 332,-1000,-1000, 325, 325, 453,-1000,
-1000,  73, 304,-1000,  58,-1000, 304,-1000,-1000,-1000,
 453,-1000, 380,-1000, 330,1273, -17,  66, -18, 453,
 475,  37, 963,1060,-1000, 453,-1000,-1000,-1000,-1000,
-1000, 963,-1000, 963, 361,-1000, 963,-1000, 271,-1000,
 751, 475,-1000, 963,-1000,-1000,-1000, 963, 963,-1000,
 751,-1000, 963,-1000,-1000,  58, 475,-1000, 301, 240,
-1000,1211,-1000,-1000,-1000, 906,-1000,1211,1211,1211,
1211,1211, -30, 204, 106, 388,-1000,-1000, 388, 388,
-1000, 143, 135, 116, 751,-1000,1049,-1000,-1000,-1000,
-1000,-1000, 308,-1000,-1000, 300,-1000,-1000, 325,-1000,
-1000, 222,-1000,-1000,-1000,  -5,-1000, -36,1201, 453,
-1000, 216,-1000,  45,-1000,-1000, 380, 460,-1000, 453,
-1000,-1000, 178,-1000, 226,-1000,-1000,-1000, 324, 220,
 726, 751, 952,-1000, 751, 299, 199, 115, 751, 453,
 704,-1000, 941, 963, 963, 963, 963, 963, 963, 963,
 963, 963, 963,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
 676, 114, -31, 646, 629, 321, 127,-1000,-1000,-1000,
1083,  33, 751,-1000,-1000,  27, -30, -30, -30,  50,
-1000, 388, 106, 107, 106,1049,1049,1049, 607,  88,
  86,  74,-1000,-1000,-1000,  87,-1000, 201,-1000, 304,
-1000, 113,-1000,  85, 930,-1000,1201,-1000,-1000,  -3,
1070,-1000,-1000,-1000, 963,-1000,-1000, 453,-1000, 380,
  64,  78,-1000,   8,-1000,  60,-1000,-1000, 453, 963,
  58, 963, 963, 391,-1000, 290, 303, 963, 963,-1000,
 475,-1000,   0, -31, -31, -31, 467,  95,  95, 581,
 646, -22,-1000, 963,-1000, 475, 475,  58,-1000, 308,
-1000,-1000, 388,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
1049,1049,1049,-1000, 466, 465,  -5,-1000,-1000, 930,
-1000,-1000, 564,-1000,-1000,1201,-1000,-1000,-1000,-1000,
 380,-1000, 460, 460, 453,-1000, 751,  37,  11,  43,
 751,-1000,-1000,-1000, 963, 285, 751,  41, 282,  62,
-1000, 963, 284, 227, 282, 277, 275, 258,-1000,-1000,
-1000,-1000, 930,-1000,-1000,   7, 248,-1000,-1000,-1000,
-1000,-1000, 963,-1000,-1000, 475,-1000,-1000, 751,-1000,
-1000,-1000,-1000,-1000, 751,-1000,-1000, 751,  34, 475,
-1000 };
yytabelem yypgo[]={

   0, 613, 612,  13, 611,  81,  15,  32, 610, 604,
 603,  10,   0, 602, 601, 600,  16, 598,  35,  25,
 597, 596, 595,   3,   4, 594,  67, 593, 592,  50,
  34,  18,  26, 101,  20, 591,  30, 373,   1, 292,
  24, 347, 327,   2,   9,  14,  31,  49,  46, 590,
 588,  39,  28,  45, 587, 585, 584, 583, 581,1100,
  40, 580, 579,  12, 578, 575, 573, 572, 571, 570,
 568,  29, 567,  27, 566,  23,  41,   7,  44,   6,
  37, 565,  38, 561, 560,  11,  22,  36, 559, 558,
   8,  17,  33, 557, 555, 541,   5, 540, 472, 537,
 536, 534, 533, 532, 528, 203, 523, 521, 518, 517,
 513, 509,  88, 508, 507,  19 };
yytabelem yyr1[]={

   0,   1,   1,  55,  55,  55,  55,  55,  55,  55,
   2,  56,  56,  56,  56,  56,  56,  56,  60,  52,
  33,  53,  53,  61,  61,  62,  62,  63,  63,  26,
  26,  26,  27,  27,  34,  34,  17,  57,  57,  57,
  57,  57,  57,  57,  57,  57,  57,  57,  57,  10,
  10,  10,  74,   7,   8,   9,   9,   9,   9,   9,
   9,   9,   9,   9,   9,   9,  16,  16,  16,  50,
  50,  50,  50,  51,  51,  64,  64,  65,  65,  66,
  66,  80,  54,  54,  67,  67,  81,  82,  76,  83,
  84,  77,  77,  85,  85,  45,  45,  45,  70,  70,
  86,  86,  72,  72,  87,  36,  18,  18,  19,  19,
  75,  75,  89,  88,  88,  90,  90,  43,  43,  91,
  91,   3,  68,  68,  92,  92,  95,  93,  94,  94,
  96,  96,  11,  69,  69,  97,  20,  20,  71,  21,
  21,  22,  22,  38,  38,  38,  39,  39,  39,  39,
  39,  39,  39,  39,  39,  39,  39,  39,  39,  39,
  12,  12,  13,  13,  13,  13,  13,  13,  37,  37,
  37,  37,  32,  40,  40,  44,  44,  48,  48,  48,
  48,  48,  48,  48,  47,  49,  49,  49,  41,  41,
  42,  42,  42,  42,  42,  42,  42,  42,  58,  58,
  58,  58,  58,  58,  58,  58,  58,  99,  23,  24,
  24,  98,  98,  98,  98,  98,  98,  98,  98,  98,
  98,  98,   4, 100, 101, 101, 101, 101,  73,  73,
  35,  25,  25,  46,  46,  14,  14,  28,  28,  59,
  78,  79, 102, 103, 103, 103, 103, 103, 103, 103,
 103, 103, 103, 103, 103, 103, 103, 104, 111, 111,
 111, 106, 113, 113, 113, 108, 108, 105, 105, 114,
 114, 115, 115, 115, 115, 115, 115,  15, 107, 109,
 110, 110,  29,  29,   6,   6,  30,  30,  30,  31,
  31,  31,  31,  31,  31,   5,   5,   5,   5,   5,
 112 };
yytabelem yyr2[]={

   0,   0,   3,   2,   2,   2,   3,   3,   2,   1,
   1,   3,   4,   3,   4,   4,   5,   3,   0,   1,
   1,   0,   1,   2,   3,   1,   3,   1,   3,   0,
   2,   3,   1,   3,   1,   1,   1,   1,   1,   1,
   1,   1,   1,   1,   1,   1,   2,   1,   5,   7,
   5,   5,   0,   2,   1,   1,   1,   1,   1,   1,
   1,   1,   1,   1,   1,   1,   0,   4,   6,   3,
   4,   5,   3,   1,   3,   3,   3,   3,   3,   3,
   3,   3,   1,   3,   3,   3,   0,   6,   0,   0,
   0,   2,   3,   1,   3,   1,   2,   1,   1,   3,
   1,   1,   1,   3,   3,   2,   1,   5,   1,   3,
   0,   3,   0,   2,   3,   1,   3,   1,   1,   1,
   3,   1,   3,   3,   4,   1,   0,   2,   1,   3,
   1,   3,   1,   1,   2,   4,   1,   3,   0,   0,
   1,   1,   3,   1,   3,   1,   1,   1,   3,   3,
   3,   3,   2,   3,   3,   3,   3,   3,   2,   3,
   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,
   4,   5,   5,   0,   1,   1,   1,   1,   1,   1,
   1,   1,   1,   1,   5,   1,   1,   1,   1,   3,
   1,   1,   3,   3,   3,   3,   2,   3,   1,   7,
   4,   1,   2,   2,   6,   2,   2,   5,   3,   1,
   4,   4,   5,   2,   1,   1,  10,   1,   3,   4,
   3,   3,   1,   1,   3,   3,   7,   7,   0,   1,
   3,   1,   3,   1,   2,   1,   1,   1,   3,   0,
   0,   0,   1,   2,   2,   2,   2,   2,   2,   2,
   3,   4,   4,   2,   3,   1,   3,   3,   1,   1,
   1,   3,   1,   1,   1,   1,   1,   3,   3,   1,
   3,   1,   1,   1,   2,   2,   2,   1,   3,   3,
   4,   4,   1,   3,   1,   5,   1,   1,   1,   3,
   3,   3,   3,   3,   3,   1,   3,   5,   5,   5,
   0 };
yytabelem yychk[]={

-1000,  -1, -55,  -2, 256,   3,   1, -56, -57, -58,
  18,  37,   4,  60,  23,  69,  44,  -7,  40, -10,
 -50, -64, -65, -66, -67, -68, -69,  66,  43,  57,
 -98,  33,  97, -99,  36,  35,  39,  -8,  27,  42,
  53,  41,  30,  50,  55,-100,  20,  29,-101,-102,
  48, -35,  64, -14,  49,  -9,  19,  45,  46,  47,
-103,  24,  58,  68,  52,  63,  28,  34,  31,  54,
  25,  72,  32,  21,  67,-104,-106,-107,-109,-110,
-111,-113,  62,  73,  59,  22,  65,  38,  51,  56,
  26, -17,   5, -59, -60, -60, -60, -60,  44, -73,
  78, -52, -33,  14,  78,  99, -73,  78,  78,  78,
  78, -73,  78, -97,  83, -70, -86, -33, -51,  85,
  83, -71, -59, -98,  70, -59, -59, -59, -16,  82,
 -71, -71, -71, -71, -81, -71, -37, -33, -59, -59,
 -59,  74, -59, -59, -59, -59, -59, -59, -59,-105,
 -42,  82,  84,  74, -37, -48, -41, -12,  12,  13,
   5,   6,   7,   8, -49,  80,  81,  11,  10,   9,
-105,  74,-105,-108, -42,  82,-105,  78, -59, -59,
 -59, -59, -59, -53, -52, -53, -52, -52, -60, -33,
 -26,  74, -33, -76, -51, -36, -33, -33, -33, -80,
  74, -82, -76, -92, -93, -95, -33,  78,  14,  74,
 -78, -73,  74, -78, -36, -51, -33, -33, -80, -82,
 -92,  76, -32,  74,  -4,   6,  74,  75, -25, -46,
 -38,  82, -39,  74, -47, -37, -48, -12,  90, -40,
 -38, -40,  74,  -3,   6, -33,  74, -33, -41,-114,
 -42,  74,-115,  82,  84, -15,  15, -12,  82,  83,
  84,  85, -41, -41, -29,  78,  -6, -37,  74,  78,
 -30, -39,  -5, -31, -38, -47,  74, -30,-112,-112,
-112,-112, -41,  82, -61,  74, -26, -26, -52, -71,
  75, -27, -34, -33,  82, -75,  74, -77, -84, -73,
 -75, -54, -37, -19, -18, -37,  74,  74,  -7,  83,
 -86,  83, -72, -87, -33,  -3, -24, -23,  98, -33,
 -38, -38,  74, -36, -38, -21, -40, -22, -38,  71,
 -38,  75,  78, -12,  82,  83,  84, -13,  89,  88,
  87,  86,  85,  91,  93,  92,  95,  94,  96,  -3,
 -38, -39, -38, -38, -38, -73, -91,  -3,  75,  75,
  78, -41, -38,  82,  84, -41, -41, -41, -41, -41,
  75,  78, -29, -29, -29,  78,  78,  78, -38, -39,
  -5, -31,-112,-112,  75, -62, -63,  14, -26, -74,
  75,  78, -16, -88, -89,  99,  78, -85, -45, -44,
 -12, -47, -33, -48,  74, -36,  75,  78,  83,  78,
 -19, -94, -96, -11,  14, -20, -33,  75,  78,  76,
 -79,  74,  76,  75, -79,  82,  75,  77,  78, -33,
  75, -46, -38, -38, -38, -38, -38, -38, -38, -38,
 -38, -38,  75,  78,  75,  74,  78,  75,-115, -41,
  75,  -6,  78, -39,  -5, -39,  -5, -39,  -5,  75,
  78,  78,  78,  75,  78,  76, -75, -34,  75,  78,
 -90, -43, -38,  82, -85,  82, -44, -37, -83, -18,
  78,  75,  78,  81,  78, -87, -38, -73, -38, -28,
 -38,  70,  75, -32,  74, -40, -38,  -3, -39, -91,
  -3, -73, -23, -33, -39, -23, -23, -23, -63,  14,
 -16, -90,  77, -45, -44, -77, -23, -96, -11, -33,
 -24,  75,  78, -79,  75,  78,  75,  75, -38,  75,
  75,  75,  75, -43, -38,  83,  75, -38,  -3,  78,
  -3 };
yytabelem yydef[]={

   1,  -2,   0,   0,   9,  10,   2,   3,   4,   5,
   0, 239,   8,  18,  18,  18,  18, 228,   0,  37,
  -2,  39,  40,  41,  -2,  43,  44,  45,  47, 138,
 198, 239, 201,   0, 239, 239, 239,  66, 138, 138,
 138, 138,  86, 138, 133,   0, 239, 239, 214, 215,
 239, 217, 239, 239, 239,  54, 223, 239, 239, 239,
 242, 239, 235, 236,  55,  56,  57,  58,  59,  60,
  61,  62,  63,  64,  65,   0,   0,   0,   0, 255,
 239, 239, 239, 239, 239, 258, 259, 260, 262, 263,
 264,   6,  36,   7,  21,  21,   0,   0,  18,   0,
 229,  29,  19,  20,   0,  88,   0, 229,   0,   0,
   0,  88, 126, 134,   0,  46,  98, 100, 101,  73,
   0,   0,  -2, 202, 203,   0, 205, 206,  53, 240,
   0,   0,   0,   0,  88, 126,   0, 168,   0, 213,
   0,   0, 173, 173,   0,   0,   0,   0,   0, 243,
  -2, 245, 246,   0, 190, 191,   0,   0, 177, 178,
 179, 180, 181, 182, 183, 160, 161, 185, 186, 187,
 247,   0, 248, 249,  -2, 266, 253,   0, 300, 300,
 300, 300,   0,  11,  22,  13,  29,  29,   0, 138,
  17,   0, 110,  90, 228,  72, 110,  76,  78,  80,
   0,  85,   0, 123, 125,   0,   0,   0,   0,   0,
   0,   0,   0,   0,  69,   0,  75,  77,  79,  84,
 122,   0, 169,  -2,   0, 222,   0, 218,   0, 231,
 233,   0, 143,   0, 145, 146, 147,   0,   0, 220,
 174, 221,   0, 224, 121,  -2,   0, 230, 271,   0,
 188,   0, 269, 272, 273,   0, 277,   0,   0,   0,
   0,   0, 196, 271, 250,   0, 282, 284,   0,   0,
 254,  -2, 287, 288,   0,  -2,   0, 256, 257, 261,
 278, 279, 300, 300,  12,   0,  14,  15,  29,  52,
  30,   0,  32,  34,  35,  66, 112,   0,   0,   0,
 105,   0,  82,   0, 108, 106,   0,   0, 127,   0,
  99,  74,   0, 102,   0, 241, 200, 209,   0,   0,
   0, 241,   0,  70, 211,   0,   0, 140,  -2,   0,
   0, 219,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0, 162, 163, 164, 165, 166, 167, 234,
   0, 143, 152, 158,   0,   0,   0, 119,  -2, 268,
   0,   0, 274, 275, 276, 192, 193, 194, 195, 197,
 267,   0, 252,   0, 251,   0,   0,   0,   0, 143,
   0,   0, 280, 281,  23,   0,  25,  27,  16, 110,
  31,   0,  50,   0,   0,  51,   0,  91,  93,  95,
   0,  97, 175, 176,   0,  71,  81,   0,  89,   0,
   0,   0, 128, 130, 132, 135, 136,  48,   0,   0,
 228,   0,   0,   0,  67,   0, 170, 173,   0, 212,
   0, 232, 148, 149, 150, 151,  -2, 154, 155, 156,
 157, 159, 144,   0, 207,   0,   0, 228, 270, 271,
 189, 283,   0,  -2, 290,  -2, 292,  -2, 294,  -2,
   0,   0,   0,  24,   0,   0,  66,  33, 111,   0,
 113, 115, 118, 117,  92,   0,  96,  83,  90, 109,
   0, 124,   0,   0,   0, 103, 104,   0,   0, 208,
 237, 204, 241, 171, 173,   0, 142,   0, 143,   0,
 120,   0,   0, 168,  -2,   0,   0,   0,  26,  28,
  49, 114,   0,  94,  95,   0,   0, 129, 131, 137,
 199, 210,   0,  68, 172,   0, 184, 226, 227, 285,
 297, 298, 299, 116, 118,  87, 107, 238,   0,   0,
 216 };
# ifdef YYDEBUG
# include "y.debug"
# endif

# define YYFLAG -1000
# define YYERROR goto yyerrlab
# define YYACCEPT return(0)
# define YYABORT return(1)

/*	parser for yacc output	*/

#ifdef YYDEBUG
int yydebug = 0; /* 1 for debugging */
#endif
YYSTYPE yyv[YYMAXDEPTH]; /* where the values are stored */
int yychar = -1; /* current input token number */
int yynerrs = 0;  /* number of errors */
yytabelem yyerrflag = 0;  /* error recovery flag */

yyparse()
{	yytabelem yys[YYMAXDEPTH];
	int yyj, yym;
	register YYSTYPE *yypvt;
	register int yystate, yyn;
	register yytabelem *yyps;
	register YYSTYPE *yypv;
	register yytabelem *yyxi;

	yystate = 0;
	yychar = -1;
	yynerrs = 0;
	yyerrflag = 0;
	yyps= &yys[-1];
	yypv= &yyv[-1];

yystack:    /* put a state and value onto the stack */
#ifdef YYDEBUG
	if(yydebug >= 3)
		if(yychar < 0 || yytoknames[yychar] == 0)
			printf("char %d in %s", yychar, yystates[yystate]);
		else
			printf("%s in %s", yytoknames[yychar], yystates[yystate]);
#endif
	if( ++yyps >= &yys[YYMAXDEPTH] ) {
		yyerror( "yacc stack overflow" );
		return(1);
	}
	*yyps = yystate;
	++yypv;
	*yypv = yyval;
yynewstate:
	yyn = yypact[yystate];
	if(yyn <= YYFLAG) goto yydefault; /* simple state */
	if(yychar<0) {
		yychar = yylex();
#ifdef YYDEBUG
		if(yydebug >= 2) {
			if(yychar <= 0)
				printf("lex EOF\n");
			else if(yytoknames[yychar])
				printf("lex %s\n", yytoknames[yychar]);
			else
				printf("lex (%c)\n", yychar);
		}
#endif
		if(yychar < 0)
			yychar = 0;
	}
	if((yyn += yychar) < 0 || yyn >= YYLAST)
		goto yydefault;
	if( yychk[ yyn=yyact[ yyn ] ] == yychar ){ /* valid shift */
		yychar = -1;
		yyval = yylval;
		yystate = yyn;
		if( yyerrflag > 0 ) --yyerrflag;
		goto yystack;
	}
yydefault:
	/* default state action */
	if( (yyn=yydef[yystate]) == -2 ) {
		if(yychar < 0) {
			yychar = yylex();
#ifdef YYDEBUG
			if(yydebug >= 2)
				if(yychar < 0)
					printf("lex EOF\n");
				else
					printf("lex %s\n", yytoknames[yychar]);
#endif
			if(yychar < 0)
				yychar = 0;
		}
		/* look through exception table */
		for(yyxi=yyexca; (*yyxi!= (-1)) || (yyxi[1]!=yystate);
			yyxi += 2 ) ; /* VOID */
		while( *(yyxi+=2) >= 0 ){
			if( *yyxi == yychar ) break;
		}
		if( (yyn = yyxi[1]) < 0 ) return(0);   /* accept */
	}
	if( yyn == 0 ){ /* error */
		/* error ... attempt to resume parsing */
		switch( yyerrflag ){
		case 0:   /* brand new error */
#ifdef YYDEBUG
			yyerror("syntax error\n%s", yystates[yystate]);
			if(yytoknames[yychar])
				yyerror("saw %s\n", yytoknames[yychar]);
			else if(yychar >= ' ' && yychar < '\177')
				yyerror("saw `%c'\n", yychar);
			else if(yychar == 0)
				yyerror("saw EOF\n");
			else
				yyerror("saw char 0%o\n", yychar);
#else
			yyerror( "syntax error" );
#endif
yyerrlab:
			++yynerrs;
		case 1:
		case 2: /* incompletely recovered error ... try again */
			yyerrflag = 3;
			/* find a state where "error" is a legal shift action */
			while ( yyps >= yys ) {
				yyn = yypact[*yyps] + YYERRCODE;
				if( yyn>= 0 && yyn < YYLAST && yychk[yyact[yyn]] == YYERRCODE ){
					yystate = yyact[yyn];  /* simulate a shift of "error" */
					goto yystack;
				}
				yyn = yypact[*yyps];
				/* the current yyps has no shift onn "error", pop stack */
#ifdef YYDEBUG
				if( yydebug ) printf( "error recovery pops state %d, uncovers %d\n", *yyps, yyps[-1] );
#endif
				--yyps;
				--yypv;
			}
			/* there is no state on the stack with an error shift ... abort */
yyabort:
			return(1);
		case 3:  /* no shift yet; clobber input char */
#ifdef YYDEBUG
			if( yydebug ) {
				printf("error recovery discards ");
				if(yytoknames[yychar])
					printf("%s\n", yytoknames[yychar]);
				else if(yychar >= ' ' && yychar < '\177')
					printf("`%c'\n", yychar);
				else if(yychar == 0)
					printf("EOF\n");
				else
					printf("char 0%o\n", yychar);
			}
#endif
			if( yychar == 0 ) goto yyabort; /* don't discard EOF, quit */
			yychar = -1;
			goto yynewstate;   /* try again in the same state */
		}
	}
	/* reduction by production yyn */
#ifdef YYDEBUG
	if(yydebug) {	char *s;
		printf("reduce %d in:\n\t", yyn);
		for(s = yystates[yystate]; *s; s++) {
			putchar(*s);
			if(*s == '\n' && *(s+1))
				putchar('\t');
		}
	}
#endif
	yyps -= yyr2[yyn];
	yypvt = yypv;
	yypv -= yyr2[yyn];
	yyval = yypv[1];
	yym=yyn;
	/* consult goto table to find next state */
	yyn = yyr1[yyn];
	yyj = yypgo[yyn] + *yyps + 1;
	if( yyj>=YYLAST || yychk[ yystate = yyact[yyj] ] != -yyn ) yystate = yyact[yypgo[yyn]];
	switch(yym){

case 3:
/* # line 226 "gram.in" */
{
/* stat:   is the nonterminal for Fortran statements */

		  lastwasbranch = NO; } break;
case 5:
/* # line 232 "gram.in" */
{ /* forbid further statement function definitions... */
		  if (parstate == INDATA && laststfcn != thisstno)
			parstate = INEXEC;
		  thisstno++;
		  if(yypvt[-1].labval && (yypvt[-1].labval->labelno==dorange))
			enddo(yypvt[-1].labval->labelno);
		  if(lastwasbranch && thislabel==NULL)
			warn("statement cannot be reached");
		  lastwasbranch = thiswasbranch;
		  thiswasbranch = NO;
		  if(yypvt[-1].labval)
			{
			if(yypvt[-1].labval->labtype == LABFORMAT)
				err("label already that of a format");
			else
				yypvt[-1].labval->labtype = LABEXEC;
			}
		  freetemps();
		} break;
case 6:
/* # line 252 "gram.in" */
{ if (can_include)
			doinclude( yypvt[-0].charpval );
		  else {
			fprintf(diagfile, "Cannot open file %s\n", yypvt[-0].charpval);
			done(1);
			}
		} break;
case 7:
/* # line 260 "gram.in" */
{ if (yypvt[-2].labval)
			lastwasbranch = NO;
		  endproc(); /* lastwasbranch = NO; -- set in endproc() */
		} break;
case 8:
/* # line 265 "gram.in" */
{ extern void unclassifiable();
		  unclassifiable();

/* flline flushes the current line, ignoring the rest of the text there */

		  flline(); } break;
case 9:
/* # line 272 "gram.in" */
{ flline();  needkwd = NO;  inioctl = NO;
		  yyerrok; yyclearin; } break;
case 10:
/* # line 277 "gram.in" */
{
		if(yystno != 0)
			{
			yyval.labval = thislabel =  mklabel(yystno);
			if( ! headerdone ) {
				if (procclass == CLUNKNOWN)
					procclass = CLMAIN;
				puthead(CNULL, procclass);
				}
			if(thislabel->labdefined)
				execerr("label %s already defined",
					convic(thislabel->stateno) );
			else	{
				if(thislabel->blklevel!=0 && thislabel->blklevel<blklevel
				    && thislabel->labtype!=LABFORMAT)
					warn1("there is a branch to label %s from outside block",
					      convic( (ftnint) (thislabel->stateno) ) );
				thislabel->blklevel = blklevel;
				thislabel->labdefined = YES;
				if(thislabel->labtype != LABFORMAT)
					p1_label((long)(thislabel - labeltab));
				}
			}
		else    yyval.labval = thislabel = NULL;
		} break;
case 11:
/* # line 305 "gram.in" */
{startproc(yypvt[-0].extval, CLMAIN); } break;
case 12:
/* # line 307 "gram.in" */
{	warn("ignoring arguments to main program");
			/* hashclear(); */
			startproc(yypvt[-1].extval, CLMAIN); } break;
case 13:
/* # line 311 "gram.in" */
{ if(yypvt[-0].extval) NO66("named BLOCKDATA");
		  startproc(yypvt[-0].extval, CLBLOCK); } break;
case 14:
/* # line 314 "gram.in" */
{ entrypt(CLPROC, TYSUBR, (ftnint) 0,  yypvt[-1].extval, yypvt[-0].chval); } break;
case 15:
/* # line 316 "gram.in" */
{ entrypt(CLPROC, TYUNKNOWN, (ftnint) 0, yypvt[-1].extval, yypvt[-0].chval); } break;
case 16:
/* # line 318 "gram.in" */
{ entrypt(CLPROC, yypvt[-4].ival, varleng, yypvt[-1].extval, yypvt[-0].chval); } break;
case 17:
/* # line 320 "gram.in" */
{ if(parstate==OUTSIDE || procclass==CLMAIN
			|| procclass==CLBLOCK)
				execerr("misplaced entry statement", CNULL);
		  entrypt(CLENTRY, 0, (ftnint) 0, yypvt[-1].extval, yypvt[-0].chval);
		} break;
case 18:
/* # line 328 "gram.in" */
{ newproc(); } break;
case 19:
/* # line 332 "gram.in" */
{ yyval.extval = newentry(yypvt[-0].namval, 1); } break;
case 20:
/* # line 336 "gram.in" */
{ yyval.namval = mkname(token); } break;
case 21:
/* # line 339 "gram.in" */
{ yyval.extval = NULL; } break;
case 29:
/* # line 357 "gram.in" */
{ yyval.chval = 0; } break;
case 30:
/* # line 359 "gram.in" */
{ NO66(" () argument list");
		  yyval.chval = 0; } break;
case 31:
/* # line 362 "gram.in" */
{yyval.chval = yypvt[-1].chval; } break;
case 32:
/* # line 366 "gram.in" */
{ yyval.chval = (yypvt[-0].namval ? mkchain((char *)yypvt[-0].namval,CHNULL) : CHNULL ); } break;
case 33:
/* # line 368 "gram.in" */
{ if(yypvt[-0].namval) yypvt[-2].chval = yyval.chval = mkchain((char *)yypvt[-0].namval, yypvt[-2].chval); } break;
case 34:
/* # line 372 "gram.in" */
{ if(yypvt[-0].namval->vstg!=STGUNKNOWN && yypvt[-0].namval->vstg!=STGARG)
			dclerr("name declared as argument after use", yypvt[-0].namval);
		  yypvt[-0].namval->vstg = STGARG;
		} break;
case 35:
/* # line 377 "gram.in" */
{ NO66("altenate return argument");

/* substars   means that '*'ed formal parameters should be replaced.
   This is used to specify alternate return labels; in theory, only
   parameter slots which have '*' should accept the statement labels.
   This compiler chooses to ignore the '*'s in the formal declaration, and
   always return the proper value anyway.

   This variable is only referred to in   proc.c   */

		  yyval.namval = 0;  substars = YES; } break;
case 36:
/* # line 393 "gram.in" */
{
		char *s;
		s = copyn(toklen+1, token);
		s[toklen] = '\0';
		yyval.charpval = s;
		} break;
case 45:
/* # line 409 "gram.in" */
{ NO66("SAVE statement");
		  saveall = YES; } break;
case 46:
/* # line 412 "gram.in" */
{ NO66("SAVE statement"); } break;
case 47:
/* # line 414 "gram.in" */
{ fmtstmt(thislabel); setfmt(thislabel); } break;
case 48:
/* # line 416 "gram.in" */
{ NO66("PARAMETER statement"); } break;
case 49:
/* # line 420 "gram.in" */
{ settype(yypvt[-4].namval, yypvt[-6].ival, yypvt[-0].lval);
		  if(ndim>0) setbound(yypvt[-4].namval,ndim,dims);
		} break;
case 50:
/* # line 424 "gram.in" */
{ settype(yypvt[-2].namval, yypvt[-4].ival, yypvt[-0].lval);
		  if(ndim>0) setbound(yypvt[-2].namval,ndim,dims);
		} break;
case 51:
/* # line 428 "gram.in" */
{ if (new_dcl == 2) {
			err("attempt to give DATA in type-declaration");
			new_dcl = 1;
			}
		} break;
case 52:
/* # line 435 "gram.in" */
{ new_dcl = 2; } break;
case 53:
/* # line 438 "gram.in" */
{ varleng = yypvt[-0].lval; } break;
case 54:
/* # line 442 "gram.in" */
{ varleng = (yypvt[-0].ival<0 || ONEOF(yypvt[-0].ival,M(TYLOGICAL)|M(TYLONG))
				? 0 : typesize[yypvt[-0].ival]);
		  vartype = yypvt[-0].ival; } break;
case 55:
/* # line 447 "gram.in" */
{ yyval.ival = TYLONG; } break;
case 56:
/* # line 448 "gram.in" */
{ yyval.ival = tyreal; } break;
case 57:
/* # line 449 "gram.in" */
{ ++complex_seen; yyval.ival = tycomplex; } break;
case 58:
/* # line 450 "gram.in" */
{ yyval.ival = TYDREAL; } break;
case 59:
/* # line 451 "gram.in" */
{ ++dcomplex_seen; NOEXT("DOUBLE COMPLEX statement"); yyval.ival = TYDCOMPLEX; } break;
case 60:
/* # line 452 "gram.in" */
{ yyval.ival = TYLOGICAL; } break;
case 61:
/* # line 453 "gram.in" */
{ NO66("CHARACTER statement"); yyval.ival = TYCHAR; } break;
case 62:
/* # line 454 "gram.in" */
{ yyval.ival = TYUNKNOWN; } break;
case 63:
/* # line 455 "gram.in" */
{ yyval.ival = TYUNKNOWN; } break;
case 64:
/* # line 456 "gram.in" */
{ NOEXT("AUTOMATIC statement"); yyval.ival = - STGAUTO; } break;
case 65:
/* # line 457 "gram.in" */
{ NOEXT("STATIC statement"); yyval.ival = - STGBSS; } break;
case 66:
/* # line 461 "gram.in" */
{ yyval.lval = varleng; } break;
case 67:
/* # line 463 "gram.in" */
{
		expptr p;
		p = yypvt[-1].expval;
		NO66("length specification *n");
		if( ! ISICON(p) || p->constblock.Const.ci <= 0 )
			{
			yyval.lval = 0;
			dclerr("length must be a positive integer constant",
				NPNULL);
			}
		else {
			if (vartype == TYCHAR)
				yyval.lval = p->constblock.Const.ci;
			else switch((int)p->constblock.Const.ci) {
				case 1:	yyval.lval = 1; break;
				case 2: yyval.lval = typesize[TYSHORT];	break;
				case 4: yyval.lval = typesize[TYLONG];	break;
				case 8: yyval.lval = typesize[TYDREAL];	break;
				case 16: yyval.lval = typesize[TYDCOMPLEX]; break;
				default:
					dclerr("invalid length",NPNULL);
					yyval.lval = varleng;
				}
			}
		} break;
case 68:
/* # line 489 "gram.in" */
{ NO66("length specification *(*)"); yyval.lval = -1; } break;
case 69:
/* # line 493 "gram.in" */
{ incomm( yyval.extval = comblock("") , yypvt[-0].namval ); } break;
case 70:
/* # line 495 "gram.in" */
{ yyval.extval = yypvt[-1].extval;  incomm(yypvt[-1].extval, yypvt[-0].namval); } break;
case 71:
/* # line 497 "gram.in" */
{ yyval.extval = yypvt[-2].extval;  incomm(yypvt[-2].extval, yypvt[-0].namval); } break;
case 72:
/* # line 499 "gram.in" */
{ incomm(yypvt[-2].extval, yypvt[-0].namval); } break;
case 73:
/* # line 503 "gram.in" */
{ yyval.extval = comblock(""); } break;
case 74:
/* # line 505 "gram.in" */
{ yyval.extval = comblock(token); } break;
case 75:
/* # line 509 "gram.in" */
{ setext(yypvt[-0].namval); } break;
case 76:
/* # line 511 "gram.in" */
{ setext(yypvt[-0].namval); } break;
case 77:
/* # line 515 "gram.in" */
{ NO66("INTRINSIC statement"); setintr(yypvt[-0].namval); } break;
case 78:
/* # line 517 "gram.in" */
{ setintr(yypvt[-0].namval); } break;
case 81:
/* # line 525 "gram.in" */
{
		struct Equivblock *p;
		if(nequiv >= maxequiv)
			many("equivalences", 'q', maxequiv);
		p  =  & eqvclass[nequiv++];
		p->eqvinit = NO;
		p->eqvbottom = 0;
		p->eqvtop = 0;
		p->equivs = yypvt[-1].eqvval;
		} break;
case 82:
/* # line 538 "gram.in" */
{ yyval.eqvval=ALLOC(Eqvchain);
		  yyval.eqvval->eqvitem.eqvlhs = (struct Primblock *)yypvt[-0].expval;
		} break;
case 83:
/* # line 542 "gram.in" */
{ yyval.eqvval=ALLOC(Eqvchain);
		  yyval.eqvval->eqvitem.eqvlhs = (struct Primblock *) yypvt[-0].expval;
		  yyval.eqvval->eqvnextp = yypvt[-2].eqvval;
		} break;
case 86:
/* # line 553 "gram.in" */
{ if(parstate == OUTSIDE)
			{
			newproc();
			startproc(ESNULL, CLMAIN);
			}
		  if(parstate < INDATA)
			{
			enddcl();
			parstate = INDATA;
			datagripe = 1;
			}
		} break;
case 87:
/* # line 568 "gram.in" */
{ ftnint junk;
		  if(nextdata(&junk) != NULL)
			err("too few initializers");
		  frdata(yypvt[-4].chval);
		  frrpl();
		} break;
case 88:
/* # line 576 "gram.in" */
{ frchain(&datastack); curdtp = 0; } break;
case 89:
/* # line 578 "gram.in" */
{ pop_datastack(); } break;
case 90:
/* # line 580 "gram.in" */
{ toomanyinit = NO; } break;
case 93:
/* # line 585 "gram.in" */
{ dataval(ENULL, yypvt[-0].expval); } break;
case 94:
/* # line 587 "gram.in" */
{ dataval(yypvt[-2].expval, yypvt[-0].expval); } break;
case 96:
/* # line 592 "gram.in" */
{ if( yypvt[-1].ival==OPMINUS && ISCONST(yypvt[-0].expval) )
			consnegop((Constp)yypvt[-0].expval);
		  yyval.expval = yypvt[-0].expval;
		} break;
case 100:
/* # line 604 "gram.in" */
{ int k;
		  yypvt[-0].namval->vsave = YES;
		  k = yypvt[-0].namval->vstg;
		if( ! ONEOF(k, M(STGUNKNOWN)|M(STGBSS)|M(STGINIT)) )
			dclerr("can only save static variables", yypvt[-0].namval);
		} break;
case 104:
/* # line 618 "gram.in" */
{ if(yypvt[-2].namval->vclass == CLUNKNOWN)
			make_param((struct Paramblock *)yypvt[-2].namval, yypvt[-0].expval);
		  else dclerr("cannot make into parameter", yypvt[-2].namval);
		} break;
case 105:
/* # line 625 "gram.in" */
{ if(ndim>0) setbound(yypvt[-1].namval, ndim, dims); } break;
case 106:
/* # line 629 "gram.in" */
{ Namep np;
		  np = ( (struct Primblock *) yypvt[-0].expval) -> namep;
		  vardcl(np);
		  if(np->vstg == STGCOMMON)
			extsymtab[np->vardesc.varno].extinit = YES;
		  else if(np->vstg==STGEQUIV)
			eqvclass[np->vardesc.varno].eqvinit = YES;
		  else if(np->vstg!=STGINIT && np->vstg!=STGBSS)
			dclerr("inconsistent storage classes", np);
		  yyval.chval = mkchain((char *)yypvt[-0].expval, CHNULL);
		} break;
case 107:
/* # line 641 "gram.in" */
{ chainp p; struct Impldoblock *q;
		pop_datastack();
		q = ALLOC(Impldoblock);
		q->tag = TIMPLDO;
		(q->varnp = (Namep) (yypvt[-1].chval->datap))->vimpldovar = 1;
		p = yypvt[-1].chval->nextp;
		if(p)  { q->implb = (expptr)(p->datap); p = p->nextp; }
		if(p)  { q->impub = (expptr)(p->datap); p = p->nextp; }
		if(p)  { q->impstep = (expptr)(p->datap); }
		frchain( & (yypvt[-1].chval) );
		yyval.chval = mkchain((char *)q, CHNULL);
		q->datalist = hookup(yypvt[-3].chval, yyval.chval);
		} break;
case 108:
/* # line 657 "gram.in" */
{ if (!datastack)
			curdtp = 0;
		  datastack = mkchain((char *)curdtp, datastack);
		  curdtp = yypvt[-0].chval; curdtelt = 0;
		  } break;
case 109:
/* # line 663 "gram.in" */
{ yyval.chval = hookup(yypvt[-2].chval, yypvt[-0].chval); } break;
case 110:
/* # line 667 "gram.in" */
{ ndim = 0; } break;
case 112:
/* # line 671 "gram.in" */
{ ndim = 0; } break;
case 115:
/* # line 676 "gram.in" */
{
		  if(ndim == maxdim)
			err("too many dimensions");
		  else if(ndim < maxdim)
			{ dims[ndim].lb = 0;
			  dims[ndim].ub = yypvt[-0].expval;
			}
		  ++ndim;
		} break;
case 116:
/* # line 686 "gram.in" */
{
		  if(ndim == maxdim)
			err("too many dimensions");
		  else if(ndim < maxdim)
			{ dims[ndim].lb = yypvt[-2].expval;
			  dims[ndim].ub = yypvt[-0].expval;
			}
		  ++ndim;
		} break;
case 117:
/* # line 698 "gram.in" */
{ yyval.expval = 0; } break;
case 119:
/* # line 703 "gram.in" */
{ nstars = 1; labarray[0] = yypvt[-0].labval; } break;
case 120:
/* # line 705 "gram.in" */
{ if(nstars < maxlablist)  labarray[nstars++] = yypvt[-0].labval; } break;
case 121:
/* # line 709 "gram.in" */
{ yyval.labval = execlab( convci(toklen, token) ); } break;
case 122:
/* # line 713 "gram.in" */
{ NO66("IMPLICIT statement"); } break;
case 125:
/* # line 719 "gram.in" */
{ if (vartype != TYUNKNOWN)
			dclerr("-- expected letter range",NPNULL);
		  setimpl(vartype, varleng, 'a', 'z'); } break;
case 126:
/* # line 724 "gram.in" */
{ needkwd = 1; } break;
case 130:
/* # line 733 "gram.in" */
{ setimpl(vartype, varleng, yypvt[-0].ival, yypvt[-0].ival); } break;
case 131:
/* # line 735 "gram.in" */
{ setimpl(vartype, varleng, yypvt[-2].ival, yypvt[-0].ival); } break;
case 132:
/* # line 739 "gram.in" */
{ if(toklen!=1 || token[0]<'a' || token[0]>'z')
			{
			dclerr("implicit item must be single letter", NPNULL);
			yyval.ival = 0;
			}
		  else yyval.ival = token[0];
		} break;
case 135:
/* # line 753 "gram.in" */
{
		if(yypvt[-2].namval->vclass == CLUNKNOWN)
			{
			yypvt[-2].namval->vclass = CLNAMELIST;
			yypvt[-2].namval->vtype = TYINT;
			yypvt[-2].namval->vstg = STGBSS;
			yypvt[-2].namval->varxptr.namelist = yypvt[-0].chval;
			yypvt[-2].namval->vardesc.varno = ++lastvarno;
			}
		else dclerr("cannot be a namelist name", yypvt[-2].namval);
		} break;
case 136:
/* # line 767 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].namval, CHNULL); } break;
case 137:
/* # line 769 "gram.in" */
{ yyval.chval = hookup(yypvt[-2].chval, mkchain((char *)yypvt[-0].namval, CHNULL)); } break;
case 138:
/* # line 773 "gram.in" */
{ switch(parstate)
			{
			case OUTSIDE:	newproc();
					startproc(ESNULL, CLMAIN);
			case INSIDE:	parstate = INDCL;
			case INDCL:	break;

			case INDATA:
				if (datagripe) {
					errstr(
				"Statement order error: declaration after DATA",
						CNULL);
					datagripe = 0;
					}
				break;

			default:
				dclerr("declaration among executables", NPNULL);
			}
		} break;
case 139:
/* # line 795 "gram.in" */
{ yyval.chval = 0; } break;
case 140:
/* # line 797 "gram.in" */
{ yyval.chval = revchain(yypvt[-0].chval); } break;
case 141:
/* # line 801 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].expval, CHNULL); } break;
case 142:
/* # line 803 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].expval, yypvt[-2].chval); } break;
case 144:
/* # line 808 "gram.in" */
{ yyval.expval = yypvt[-1].expval; if (yyval.expval->tag == TPRIM)
					yyval.expval->primblock.parenused = 1; } break;
case 148:
/* # line 816 "gram.in" */
{ yyval.expval = mkexpr(yypvt[-1].ival, yypvt[-2].expval, yypvt[-0].expval); } break;
case 149:
/* # line 818 "gram.in" */
{ yyval.expval = mkexpr(OPSTAR, yypvt[-2].expval, yypvt[-0].expval); } break;
case 150:
/* # line 820 "gram.in" */
{ yyval.expval = mkexpr(OPSLASH, yypvt[-2].expval, yypvt[-0].expval); } break;
case 151:
/* # line 822 "gram.in" */
{ yyval.expval = mkexpr(OPPOWER, yypvt[-2].expval, yypvt[-0].expval); } break;
case 152:
/* # line 824 "gram.in" */
{ if(yypvt[-1].ival == OPMINUS)
			yyval.expval = mkexpr(OPNEG, yypvt[-0].expval, ENULL);
		  else 	yyval.expval = yypvt[-0].expval;
		} break;
case 153:
/* # line 829 "gram.in" */
{ yyval.expval = mkexpr(yypvt[-1].ival, yypvt[-2].expval, yypvt[-0].expval); } break;
case 154:
/* # line 831 "gram.in" */
{ NO66(".EQV. operator");
		  yyval.expval = mkexpr(OPEQV, yypvt[-2].expval,yypvt[-0].expval); } break;
case 155:
/* # line 834 "gram.in" */
{ NO66(".NEQV. operator");
		  yyval.expval = mkexpr(OPNEQV, yypvt[-2].expval, yypvt[-0].expval); } break;
case 156:
/* # line 837 "gram.in" */
{ yyval.expval = mkexpr(OPOR, yypvt[-2].expval, yypvt[-0].expval); } break;
case 157:
/* # line 839 "gram.in" */
{ yyval.expval = mkexpr(OPAND, yypvt[-2].expval, yypvt[-0].expval); } break;
case 158:
/* # line 841 "gram.in" */
{ yyval.expval = mkexpr(OPNOT, yypvt[-0].expval, ENULL); } break;
case 159:
/* # line 843 "gram.in" */
{ NO66("concatenation operator //");
		  yyval.expval = mkexpr(OPCONCAT, yypvt[-2].expval, yypvt[-0].expval); } break;
case 160:
/* # line 847 "gram.in" */
{ yyval.ival = OPPLUS; } break;
case 161:
/* # line 848 "gram.in" */
{ yyval.ival = OPMINUS; } break;
case 162:
/* # line 851 "gram.in" */
{ yyval.ival = OPEQ; } break;
case 163:
/* # line 852 "gram.in" */
{ yyval.ival = OPGT; } break;
case 164:
/* # line 853 "gram.in" */
{ yyval.ival = OPLT; } break;
case 165:
/* # line 854 "gram.in" */
{ yyval.ival = OPGE; } break;
case 166:
/* # line 855 "gram.in" */
{ yyval.ival = OPLE; } break;
case 167:
/* # line 856 "gram.in" */
{ yyval.ival = OPNE; } break;
case 168:
/* # line 860 "gram.in" */
{ yyval.expval = mkprim(yypvt[-0].namval, LBNULL, CHNULL); } break;
case 169:
/* # line 862 "gram.in" */
{ NO66("substring operator :");
		  yyval.expval = mkprim(yypvt[-1].namval, LBNULL, yypvt[-0].chval); } break;
case 170:
/* # line 865 "gram.in" */
{ yyval.expval = mkprim(yypvt[-3].namval, mklist(yypvt[-1].chval), CHNULL); } break;
case 171:
/* # line 867 "gram.in" */
{ NO66("substring operator :");
		  yyval.expval = mkprim(yypvt[-4].namval, mklist(yypvt[-2].chval), yypvt[-0].chval); } break;
case 172:
/* # line 872 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-3].expval, mkchain((char *)yypvt[-1].expval,CHNULL)); } break;
case 173:
/* # line 876 "gram.in" */
{ yyval.expval = 0; } break;
case 175:
/* # line 881 "gram.in" */
{ if(yypvt[-0].namval->vclass == CLPARAM)
			yyval.expval = (expptr) cpexpr(
				( (struct Paramblock *) (yypvt[-0].namval) ) -> paramval);
		} break;
case 177:
/* # line 888 "gram.in" */
{ yyval.expval = mklogcon(1); } break;
case 178:
/* # line 889 "gram.in" */
{ yyval.expval = mklogcon(0); } break;
case 179:
/* # line 890 "gram.in" */
{ yyval.expval = mkstrcon(toklen, token); } break;
case 180:
/* # line 891 "gram.in" */
 { yyval.expval = mkintcon( convci(toklen, token) ); } break;
case 181:
/* # line 892 "gram.in" */
 { yyval.expval = mkrealcon(tyreal, token); } break;
case 182:
/* # line 893 "gram.in" */
 { yyval.expval = mkrealcon(TYDREAL, token); } break;
case 184:
/* # line 898 "gram.in" */
{ yyval.expval = mkcxcon(yypvt[-3].expval,yypvt[-1].expval); } break;
case 185:
/* # line 902 "gram.in" */
{ NOEXT("hex constant");
		  yyval.expval = mkbitcon(4, toklen, token); } break;
case 186:
/* # line 905 "gram.in" */
{ NOEXT("octal constant");
		  yyval.expval = mkbitcon(3, toklen, token); } break;
case 187:
/* # line 908 "gram.in" */
{ NOEXT("binary constant");
		  yyval.expval = mkbitcon(1, toklen, token); } break;
case 189:
/* # line 914 "gram.in" */
{ yyval.expval = yypvt[-1].expval; } break;
case 192:
/* # line 920 "gram.in" */
{ yyval.expval = mkexpr(yypvt[-1].ival, yypvt[-2].expval, yypvt[-0].expval); } break;
case 193:
/* # line 922 "gram.in" */
{ yyval.expval = mkexpr(OPSTAR, yypvt[-2].expval, yypvt[-0].expval); } break;
case 194:
/* # line 924 "gram.in" */
{ yyval.expval = mkexpr(OPSLASH, yypvt[-2].expval, yypvt[-0].expval); } break;
case 195:
/* # line 926 "gram.in" */
{ yyval.expval = mkexpr(OPPOWER, yypvt[-2].expval, yypvt[-0].expval); } break;
case 196:
/* # line 928 "gram.in" */
{ if(yypvt[-1].ival == OPMINUS)
			yyval.expval = mkexpr(OPNEG, yypvt[-0].expval, ENULL);
		  else	yyval.expval = yypvt[-0].expval;
		} break;
case 197:
/* # line 933 "gram.in" */
{ NO66("concatenation operator //");
		  yyval.expval = mkexpr(OPCONCAT, yypvt[-2].expval, yypvt[-0].expval); } break;
case 199:
/* # line 938 "gram.in" */
{
		if(yypvt[-3].labval->labdefined)
			execerr("no backward DO loops", CNULL);
		yypvt[-3].labval->blklevel = blklevel+1;
		exdo(yypvt[-3].labval->labelno, NPNULL, yypvt[-0].chval);
		} break;
case 200:
/* # line 945 "gram.in" */
{
		exdo((int)(ctls - ctlstack - 2), NPNULL, yypvt[-0].chval);
		NOEXT("DO without label");
		} break;
case 201:
/* # line 950 "gram.in" */
{ exenddo(NPNULL); } break;
case 202:
/* # line 952 "gram.in" */
{ exendif();  thiswasbranch = NO; } break;
case 204:
/* # line 955 "gram.in" */
{ exelif(yypvt[-2].expval); lastwasbranch = NO; } break;
case 205:
/* # line 957 "gram.in" */
{ exelse(); lastwasbranch = NO; } break;
case 206:
/* # line 959 "gram.in" */
{ exendif(); lastwasbranch = NO; } break;
case 207:
/* # line 963 "gram.in" */
{ exif(yypvt[-1].expval); } break;
case 208:
/* # line 967 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-2].namval, yypvt[-0].chval); } break;
case 210:
/* # line 972 "gram.in" */
{ yyval.chval = mkchain(CNULL, (chainp)yypvt[-1].expval); } break;
case 211:
/* # line 976 "gram.in" */
{ exequals((struct Primblock *)yypvt[-2].expval, yypvt[-0].expval); } break;
case 212:
/* # line 978 "gram.in" */
{ exassign(yypvt[-0].namval, yypvt[-2].labval); } break;
case 215:
/* # line 982 "gram.in" */
{ inioctl = NO; } break;
case 216:
/* # line 984 "gram.in" */
{ exarif(yypvt[-6].expval, yypvt[-4].labval, yypvt[-2].labval, yypvt[-0].labval);  thiswasbranch = YES; } break;
case 217:
/* # line 986 "gram.in" */
{ excall(yypvt[-0].namval, LBNULL, 0, labarray); } break;
case 218:
/* # line 988 "gram.in" */
{ excall(yypvt[-2].namval, LBNULL, 0, labarray); } break;
case 219:
/* # line 990 "gram.in" */
{ if(nstars < maxlablist)
			excall(yypvt[-3].namval, mklist(revchain(yypvt[-1].chval)), nstars, labarray);
		  else
			many("alternate returns", 'l', maxlablist);
		} break;
case 220:
/* # line 996 "gram.in" */
{ exreturn(yypvt[-0].expval);  thiswasbranch = YES; } break;
case 221:
/* # line 998 "gram.in" */
{ exstop(yypvt[-2].ival, yypvt[-0].expval);  thiswasbranch = yypvt[-2].ival; } break;
case 222:
/* # line 1002 "gram.in" */
{ yyval.labval = mklabel( convci(toklen, token) ); } break;
case 223:
/* # line 1006 "gram.in" */
{ if(parstate == OUTSIDE)
			{
			newproc();
			startproc(ESNULL, CLMAIN);
			}
		} break;
case 224:
/* # line 1015 "gram.in" */
{ exgoto(yypvt[-0].labval);  thiswasbranch = YES; } break;
case 225:
/* # line 1017 "gram.in" */
{ exasgoto(yypvt[-0].namval);  thiswasbranch = YES; } break;
case 226:
/* # line 1019 "gram.in" */
{ exasgoto(yypvt[-4].namval);  thiswasbranch = YES; } break;
case 227:
/* # line 1021 "gram.in" */
{ if(nstars < maxlablist)
			putcmgo(putx(fixtype(yypvt[-0].expval)), nstars, labarray);
		  else
			many("labels in computed GOTO list", 'l', maxlablist);
		} break;
case 230:
/* # line 1033 "gram.in" */
{ nstars = 0; yyval.namval = yypvt[-0].namval; } break;
case 231:
/* # line 1037 "gram.in" */
{ yyval.chval = yypvt[-0].expval ? mkchain((char *)yypvt[-0].expval,CHNULL) : CHNULL; } break;
case 232:
/* # line 1039 "gram.in" */
{ yyval.chval = yypvt[-0].expval ? mkchain((char *)yypvt[-0].expval, yypvt[-2].chval) : yypvt[-2].chval; } break;
case 234:
/* # line 1044 "gram.in" */
{ if(nstars < maxlablist) labarray[nstars++] = yypvt[-0].labval; yyval.expval = 0; } break;
case 235:
/* # line 1048 "gram.in" */
{ yyval.ival = 0; } break;
case 236:
/* # line 1050 "gram.in" */
{ yyval.ival = 2; } break;
case 237:
/* # line 1054 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].expval, CHNULL); } break;
case 238:
/* # line 1056 "gram.in" */
{ yyval.chval = hookup(yypvt[-2].chval, mkchain((char *)yypvt[-0].expval,CHNULL) ); } break;
case 239:
/* # line 1060 "gram.in" */
{ if(parstate == OUTSIDE)
			{
			newproc();
			startproc(ESNULL, CLMAIN);
			}

/* This next statement depends on the ordering of the state table encoding */

		  if(parstate < INDATA) enddcl();
		} break;
case 240:
/* # line 1073 "gram.in" */
{ intonly = YES; } break;
case 241:
/* # line 1077 "gram.in" */
{ intonly = NO; } break;
case 242:
/* # line 1082 "gram.in" */
{ endio(); } break;
case 244:
/* # line 1087 "gram.in" */
{ ioclause(IOSUNIT, yypvt[-0].expval); endioctl(); } break;
case 245:
/* # line 1089 "gram.in" */
{ ioclause(IOSUNIT, ENULL); endioctl(); } break;
case 246:
/* # line 1091 "gram.in" */
{ ioclause(IOSUNIT, IOSTDERR); endioctl(); } break;
case 248:
/* # line 1094 "gram.in" */
{ doio(CHNULL); } break;
case 249:
/* # line 1096 "gram.in" */
{ doio(CHNULL); } break;
case 250:
/* # line 1098 "gram.in" */
{ doio(revchain(yypvt[-0].chval)); } break;
case 251:
/* # line 1100 "gram.in" */
{ doio(revchain(yypvt[-0].chval)); } break;
case 252:
/* # line 1102 "gram.in" */
{ doio(revchain(yypvt[-0].chval)); } break;
case 253:
/* # line 1104 "gram.in" */
{ doio(CHNULL); } break;
case 254:
/* # line 1106 "gram.in" */
{ doio(revchain(yypvt[-0].chval)); } break;
case 255:
/* # line 1108 "gram.in" */
{ doio(CHNULL); } break;
case 256:
/* # line 1110 "gram.in" */
{ doio(revchain(yypvt[-0].chval)); } break;
case 258:
/* # line 1117 "gram.in" */
{ iostmt = IOBACKSPACE; } break;
case 259:
/* # line 1119 "gram.in" */
{ iostmt = IOREWIND; } break;
case 260:
/* # line 1121 "gram.in" */
{ iostmt = IOENDFILE; } break;
case 262:
/* # line 1128 "gram.in" */
{ iostmt = IOINQUIRE; } break;
case 263:
/* # line 1130 "gram.in" */
{ iostmt = IOOPEN; } break;
case 264:
/* # line 1132 "gram.in" */
{ iostmt = IOCLOSE; } break;
case 265:
/* # line 1136 "gram.in" */
{
		ioclause(IOSUNIT, ENULL);
		ioclause(IOSFMT, yypvt[-0].expval);
		endioctl();
		} break;
case 266:
/* # line 1142 "gram.in" */
{
		ioclause(IOSUNIT, ENULL);
		ioclause(IOSFMT, ENULL);
		endioctl();
		} break;
case 267:
/* # line 1150 "gram.in" */
{
		  ioclause(IOSUNIT, yypvt[-1].expval);
		  endioctl();
		} break;
case 268:
/* # line 1155 "gram.in" */
{ endioctl(); } break;
case 271:
/* # line 1163 "gram.in" */
{ ioclause(IOSPOSITIONAL, yypvt[-0].expval); } break;
case 272:
/* # line 1165 "gram.in" */
{ ioclause(IOSPOSITIONAL, ENULL); } break;
case 273:
/* # line 1167 "gram.in" */
{ ioclause(IOSPOSITIONAL, IOSTDERR); } break;
case 274:
/* # line 1169 "gram.in" */
{ ioclause(yypvt[-1].ival, yypvt[-0].expval); } break;
case 275:
/* # line 1171 "gram.in" */
{ ioclause(yypvt[-1].ival, ENULL); } break;
case 276:
/* # line 1173 "gram.in" */
{ ioclause(yypvt[-1].ival, IOSTDERR); } break;
case 277:
/* # line 1177 "gram.in" */
{ yyval.ival = iocname(); } break;
case 278:
/* # line 1181 "gram.in" */
{ iostmt = IOREAD; } break;
case 279:
/* # line 1185 "gram.in" */
{ iostmt = IOWRITE; } break;
case 280:
/* # line 1189 "gram.in" */
{
		iostmt = IOWRITE;
		ioclause(IOSUNIT, ENULL);
		ioclause(IOSFMT, yypvt[-1].expval);
		endioctl();
		} break;
case 281:
/* # line 1196 "gram.in" */
{
		iostmt = IOWRITE;
		ioclause(IOSUNIT, ENULL);
		ioclause(IOSFMT, ENULL);
		endioctl();
		} break;
case 282:
/* # line 1205 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].tagval, CHNULL); } break;
case 283:
/* # line 1207 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].tagval, yypvt[-2].chval); } break;
case 284:
/* # line 1211 "gram.in" */
{ yyval.tagval = (tagptr) yypvt[-0].expval; } break;
case 285:
/* # line 1213 "gram.in" */
{ yyval.tagval = (tagptr) mkiodo(yypvt[-1].chval,revchain(yypvt[-3].chval)); } break;
case 286:
/* # line 1217 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].expval, CHNULL); } break;
case 287:
/* # line 1219 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].tagval, CHNULL); } break;
case 289:
/* # line 1224 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].expval, mkchain((char *)yypvt[-2].expval, CHNULL) ); } break;
case 290:
/* # line 1226 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].tagval, mkchain((char *)yypvt[-2].expval, CHNULL) ); } break;
case 291:
/* # line 1228 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].expval, mkchain((char *)yypvt[-2].tagval, CHNULL) ); } break;
case 292:
/* # line 1230 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].tagval, mkchain((char *)yypvt[-2].tagval, CHNULL) ); } break;
case 293:
/* # line 1232 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].expval, yypvt[-2].chval); } break;
case 294:
/* # line 1234 "gram.in" */
{ yyval.chval = mkchain((char *)yypvt[-0].tagval, yypvt[-2].chval); } break;
case 295:
/* # line 1238 "gram.in" */
{ yyval.tagval = (tagptr) yypvt[-0].expval; } break;
case 296:
/* # line 1240 "gram.in" */
{ yyval.tagval = (tagptr) yypvt[-1].expval; } break;
case 297:
/* # line 1242 "gram.in" */
{ yyval.tagval = (tagptr) mkiodo(yypvt[-1].chval, mkchain((char *)yypvt[-3].expval, CHNULL) ); } break;
case 298:
/* # line 1244 "gram.in" */
{ yyval.tagval = (tagptr) mkiodo(yypvt[-1].chval, mkchain((char *)yypvt[-3].tagval, CHNULL) ); } break;
case 299:
/* # line 1246 "gram.in" */
{ yyval.tagval = (tagptr) mkiodo(yypvt[-1].chval, revchain(yypvt[-3].chval)); } break;
case 300:
/* # line 1250 "gram.in" */
{ startioctl(); } break;
	}
	goto yystack;  /* stack new state and value */
}
