#ifndef lint
static char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define yyclearin (yychar=(-1))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING (yyerrflag!=0)
#define YYPREFIX "yy"
#line 26 "ftp.y"

#ifndef lint
static char sccsid[] = "@(#)ftpcmd.y	5.20.1.1 (Berkeley) 3/2/89";
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/ftp.h>

#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <pwd.h>
#include <setjmp.h>
#include <syslog.h>
#include <sys/stat.h>
#include <time.h>

extern	struct sockaddr_in data_dest;
extern	int logged_in;
extern	struct passwd *pw;
extern	int guest;
extern	int logging;
extern	int type;
extern	int form;
extern	int debug;
extern	int timeout;
extern	int maxtimeout;
extern  int pdata;
extern	char hostname[], remotehost[];
extern	char proctitle[];
extern	char *globerr;
extern	int usedefault;
extern  int transflag;
extern  char tmpline[];
char	**glob();

static	int cmd_type;
static	int cmd_form;
static	int cmd_bytesz;
char	cbuf[512];
char	*fromname;

char	*index();
#line 60 "ftp.tab.c"
#define A 257
#define B 258
#define C 259
#define E 260
#define F 261
#define I 262
#define L 263
#define N 264
#define P 265
#define R 266
#define S 267
#define T 268
#define SP 269
#define CRLF 270
#define COMMA 271
#define STRING 272
#define NUMBER 273
#define USER 274
#define PASS 275
#define ACCT 276
#define REIN 277
#define QUIT 278
#define PORT 279
#define PASV 280
#define TYPE 281
#define STRU 282
#define MODE 283
#define RETR 284
#define STOR 285
#define APPE 286
#define MLFL 287
#define MAIL 288
#define MSND 289
#define MSOM 290
#define MSAM 291
#define MRSQ 292
#define MRCP 293
#define ALLO 294
#define REST 295
#define RNFR 296
#define RNTO 297
#define ABOR 298
#define DELE 299
#define CWD 300
#define LIST 301
#define NLST 302
#define SITE 303
#define STAT 304
#define HELP 305
#define NOOP 306
#define MKD 307
#define RMD 308
#define PWD 309
#define CDUP 310
#define STOU 311
#define SMNT 312
#define SYST 313
#define SIZE 314
#define MDTM 315
#define UMASK 316
#define IDLE 317
#define CHMOD 318
#define LEXERR 319
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,    0,    0,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    2,    3,    4,    4,
   12,    5,   13,   13,   13,    6,    6,    6,    6,    6,
    6,    6,    6,    7,    7,    7,    8,    8,    8,   10,
   14,   11,    9,
};
short yylen[] = {                                         2,
    0,    2,    2,    4,    4,    4,    2,    4,    4,    4,
    4,    8,    5,    5,    5,    3,    5,    3,    5,    5,
    2,    5,    4,    2,    3,    5,    2,    4,    2,    5,
    5,    3,    3,    4,    6,    5,    7,    9,    4,    6,
    5,    2,    5,    5,    2,    2,    5,    1,    0,    1,
    1,   11,    1,    1,    1,    1,    3,    1,    3,    1,
    1,    3,    2,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    0,
};
short yydefred[] = {                                      1,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   73,   73,   73,    0,   73,    0,    0,   73,   73,   73,
   73,    0,    0,    0,    0,   73,   73,   73,   73,   73,
    0,   73,   73,    2,    3,   46,    0,    0,   45,    0,
    7,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   24,    0,    0,    0,    0,    0,   21,    0,    0,   27,
   29,    0,    0,    0,    0,    0,   42,    0,    0,   48,
    0,   50,    0,    0,    0,    0,    0,   60,    0,    0,
   64,   66,   65,    0,   68,   69,   67,    0,    0,    0,
    0,    0,    0,   71,    0,   70,    0,    0,   25,    0,
   18,    0,   16,    0,   73,    0,   73,    0,    0,    0,
    0,   32,   33,    0,    0,    0,    4,    5,    0,    6,
    0,    0,    0,   51,   63,    8,    9,   10,    0,    0,
    0,    0,   11,    0,   23,    0,    0,    0,    0,    0,
   34,    0,    0,   39,    0,    0,   28,    0,    0,    0,
    0,    0,    0,   55,   53,   54,   57,   59,   62,   13,
   14,   15,    0,   47,   22,   26,   19,   17,    0,    0,
   36,    0,    0,   20,   30,   31,   41,   43,   44,    0,
    0,   35,   72,    0,   40,    0,    0,    0,   37,    0,
    0,   12,    0,    0,   38,    0,    0,    0,   52,
};
short yydgoto[] = {                                       1,
   34,   35,   71,   73,   75,   80,   84,   88,   45,   95,
  184,  125,  157,   96,
};
short yysindex[] = {                                      0,
 -224, -247, -239, -236, -232, -222, -204, -200, -181, -177,
    0,    0,    0, -166,    0, -161, -199,    0,    0,    0,
    0, -160, -159, -264, -158,    0,    0,    0,    0,    0,
 -157,    0,    0,    0,    0,    0, -167, -162,    0, -156,
    0, -250, -198, -165, -155, -154, -153, -151, -150, -152,
    0, -145, -252, -229, -217, -302,    0, -144, -146,    0,
    0, -142, -141, -140, -139, -137,    0, -136, -135,    0,
 -134,    0, -133, -132, -130, -131, -128,    0, -249, -127,
    0,    0,    0, -126,    0,    0,    0, -125, -152, -152,
 -152, -205, -152,    0, -124,    0, -152, -152,    0, -152,
    0, -143,    0, -173,    0, -171,    0, -152, -123, -152,
 -152,    0,    0, -152, -152, -152,    0,    0, -138,    0,
 -164, -164, -122,    0,    0,    0,    0,    0, -121, -120,
 -118, -148,    0, -117,    0, -116, -115, -114, -113, -112,
    0, -163, -111,    0, -110, -109,    0, -107, -106, -105,
 -104, -103, -129,    0,    0,    0,    0,    0,    0,    0,
    0,    0, -101,    0,    0,    0,    0,    0, -100, -102,
    0,  -98, -102,    0,    0,    0,    0,    0,    0,  -99,
  -97,    0,    0,  -95,    0,  -96,  -94,  -92,    0, -152,
  -93,    0,  -91,  -90,    0,  -88,  -87,  -86,    0,
};
short yyrindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  -83,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  -82,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  -81,  -80,    0, -158,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,
};
short yygindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,   16,  -89,
  -25,   35,   47,    0,
};
#define YYTABLESIZE 190
short yytable[] = {                                     129,
  130,  131,  104,  134,   59,   60,   76,  136,  137,   77,
  138,   78,   79,  105,  106,  107,   98,   99,  146,  123,
  148,  149,   36,  124,  150,  151,  152,   46,   47,   37,
   49,    2,   38,   52,   53,   54,   55,   39,   58,  100,
  101,   62,   63,   64,   65,   66,   40,   68,   69,    3,
    4,  102,  103,    5,    6,    7,    8,    9,   10,   11,
   12,   13,   81,  132,  133,   41,   82,   83,   42,   14,
   51,   15,   16,   17,   18,   19,   20,   21,   22,   23,
   24,   25,   26,   27,   28,   29,   30,   43,   31,   32,
   33,   44,   85,   86,  154,  140,  141,  143,  144,  155,
  193,   87,   48,  156,   70,  170,  171,   50,   56,   72,
   57,   61,   67,   89,   90,   91,   74,  163,   93,   94,
  142,   92,  145,   97,  108,  109,  110,  111,  139,  112,
  113,  114,  115,  116,  153,  117,  118,  121,  119,  120,
  122,  180,  126,  127,  128,  135,  147,  186,  160,  161,
  124,  162,  164,  165,  166,  167,  168,  159,  173,  169,
  174,  172,  175,  176,  177,  178,  179,  181,  158,  182,
  183,  185,  190,  187,  189,  188,  191,  192,  195,  194,
  196,    0,    0,  198,  197,   73,  199,   49,   56,   58,
};
short yycheck[] = {                                      89,
   90,   91,  305,   93,  269,  270,  257,   97,   98,  260,
  100,  262,  263,  316,  317,  318,  269,  270,  108,  269,
  110,  111,  270,  273,  114,  115,  116,   12,   13,  269,
   15,  256,  269,   18,   19,   20,   21,  270,   23,  269,
  270,   26,   27,   28,   29,   30,  269,   32,   33,  274,
  275,  269,  270,  278,  279,  280,  281,  282,  283,  284,
  285,  286,  261,  269,  270,  270,  265,  266,  269,  294,
  270,  296,  297,  298,  299,  300,  301,  302,  303,  304,
  305,  306,  307,  308,  309,  310,  311,  269,  313,  314,
  315,  269,  258,  259,  259,  269,  270,  269,  270,  264,
  190,  267,  269,  268,  272,  269,  270,  269,  269,  272,
  270,  270,  270,  269,  269,  269,  273,  266,  269,  272,
  105,  273,  107,  269,  269,  272,  269,  269,  272,  270,
  270,  269,  269,  269,  273,  270,  270,  269,  271,  270,
  269,  271,  270,  270,  270,  270,  270,  173,  270,  270,
  273,  270,  270,  270,  270,  270,  270,  123,  269,  272,
  270,  273,  270,  270,  270,  270,  270,  269,  122,  270,
  273,  270,  269,  273,  270,  273,  271,  270,  270,  273,
  271,   -1,   -1,  271,  273,  269,  273,  270,  270,  270,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 319
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"A","B","C","E","F","I","L","N",
"P","R","S","T","SP","CRLF","COMMA","STRING","NUMBER","USER","PASS","ACCT",
"REIN","QUIT","PORT","PASV","TYPE","STRU","MODE","RETR","STOR","APPE","MLFL",
"MAIL","MSND","MSOM","MSAM","MRSQ","MRCP","ALLO","REST","RNFR","RNTO","ABOR",
"DELE","CWD","LIST","NLST","SITE","STAT","HELP","NOOP","MKD","RMD","PWD","CDUP",
"STOU","SMNT","SYST","SIZE","MDTM","UMASK","IDLE","CHMOD","LEXERR",
};
char *yyrule[] = {
"$accept : cmd_list",
"cmd_list :",
"cmd_list : cmd_list cmd",
"cmd_list : cmd_list rcmd",
"cmd : USER SP username CRLF",
"cmd : PASS SP password CRLF",
"cmd : PORT SP host_port CRLF",
"cmd : PASV CRLF",
"cmd : TYPE SP type_code CRLF",
"cmd : STRU SP struct_code CRLF",
"cmd : MODE SP mode_code CRLF",
"cmd : ALLO SP NUMBER CRLF",
"cmd : ALLO SP NUMBER SP R SP NUMBER CRLF",
"cmd : RETR check_login SP pathname CRLF",
"cmd : STOR check_login SP pathname CRLF",
"cmd : APPE check_login SP pathname CRLF",
"cmd : NLST check_login CRLF",
"cmd : NLST check_login SP STRING CRLF",
"cmd : LIST check_login CRLF",
"cmd : LIST check_login SP pathname CRLF",
"cmd : STAT check_login SP pathname CRLF",
"cmd : STAT CRLF",
"cmd : DELE check_login SP pathname CRLF",
"cmd : RNTO SP pathname CRLF",
"cmd : ABOR CRLF",
"cmd : CWD check_login CRLF",
"cmd : CWD check_login SP pathname CRLF",
"cmd : HELP CRLF",
"cmd : HELP SP STRING CRLF",
"cmd : NOOP CRLF",
"cmd : MKD check_login SP pathname CRLF",
"cmd : RMD check_login SP pathname CRLF",
"cmd : PWD check_login CRLF",
"cmd : CDUP check_login CRLF",
"cmd : SITE SP HELP CRLF",
"cmd : SITE SP HELP SP STRING CRLF",
"cmd : SITE SP UMASK check_login CRLF",
"cmd : SITE SP UMASK check_login SP octal_number CRLF",
"cmd : SITE SP CHMOD check_login SP octal_number SP pathname CRLF",
"cmd : SITE SP IDLE CRLF",
"cmd : SITE SP IDLE SP NUMBER CRLF",
"cmd : STOU check_login SP pathname CRLF",
"cmd : SYST CRLF",
"cmd : SIZE check_login SP pathname CRLF",
"cmd : MDTM check_login SP pathname CRLF",
"cmd : QUIT CRLF",
"cmd : error CRLF",
"rcmd : RNFR check_login SP pathname CRLF",
"username : STRING",
"password :",
"password : STRING",
"byte_size : NUMBER",
"host_port : NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER",
"form_code : N",
"form_code : T",
"form_code : C",
"type_code : A",
"type_code : A SP form_code",
"type_code : E",
"type_code : E SP form_code",
"type_code : I",
"type_code : L",
"type_code : L SP byte_size",
"type_code : L byte_size",
"struct_code : F",
"struct_code : R",
"struct_code : P",
"mode_code : S",
"mode_code : B",
"mode_code : C",
"pathname : pathstring",
"pathstring : STRING",
"octal_number : NUMBER",
"check_login :",
};
#endif
#ifndef YYSTYPE
typedef int YYSTYPE;
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 500
#define YYMAXDEPTH 500
#endif
#endif
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short yyss[YYSTACKSIZE];
YYSTYPE yyvs[YYSTACKSIZE];
#define yystacksize YYSTACKSIZE
#line 658 "ftp.y"

extern jmp_buf errcatch;

#define	CMD	0	/* beginning of command */
#define	ARGS	1	/* expect miscellaneous arguments */
#define	STR1	2	/* expect SP followed by STRING */
#define	STR2	3	/* expect STRING */
#define	OSTR	4	/* optional SP then STRING */
#define	ZSTR1	5	/* SP then optional STRING */
#define	ZSTR2	6	/* optional STRING after SP */
#define	SITECMD	7	/* SITE command */
#define	NSTR	8	/* Number followed by a string */

struct tab {
	char	*name;
	short	token;
	short	state;
	short	implemented;	/* 1 if command is implemented */
	char	*help;
};

struct tab cmdtab[] = {		/* In order defined in RFC 765 */
	{ "USER", USER, STR1, 1,	"<sp> username" },
	{ "PASS", PASS, ZSTR1, 1,	"<sp> password" },
	{ "ACCT", ACCT, STR1, 0,	"(specify account)" },
	{ "SMNT", SMNT, ARGS, 0,	"(structure mount)" },
	{ "REIN", REIN, ARGS, 0,	"(reinitialize server state)" },
	{ "QUIT", QUIT, ARGS, 1,	"(terminate service)", },
	{ "PORT", PORT, ARGS, 1,	"<sp> b0, b1, b2, b3, b4" },
	{ "PASV", PASV, ARGS, 1,	"(set server in passive mode)" },
	{ "TYPE", TYPE, ARGS, 1,	"<sp> [ A | E | I | L ]" },
	{ "STRU", STRU, ARGS, 1,	"(specify file structure)" },
	{ "MODE", MODE, ARGS, 1,	"(specify transfer mode)" },
	{ "RETR", RETR, STR1, 1,	"<sp> file-name" },
	{ "STOR", STOR, STR1, 1,	"<sp> file-name" },
	{ "APPE", APPE, STR1, 1,	"<sp> file-name" },
	{ "MLFL", MLFL, OSTR, 0,	"(mail file)" },
	{ "MAIL", MAIL, OSTR, 0,	"(mail to user)" },
	{ "MSND", MSND, OSTR, 0,	"(mail send to terminal)" },
	{ "MSOM", MSOM, OSTR, 0,	"(mail send to terminal or mailbox)" },
	{ "MSAM", MSAM, OSTR, 0,	"(mail send to terminal and mailbox)" },
	{ "MRSQ", MRSQ, OSTR, 0,	"(mail recipient scheme question)" },
	{ "MRCP", MRCP, STR1, 0,	"(mail recipient)" },
	{ "ALLO", ALLO, ARGS, 1,	"allocate storage (vacuously)" },
	{ "REST", REST, ARGS, 0,	"(restart command)" },
	{ "RNFR", RNFR, STR1, 1,	"<sp> file-name" },
	{ "RNTO", RNTO, STR1, 1,	"<sp> file-name" },
	{ "ABOR", ABOR, ARGS, 1,	"(abort operation)" },
	{ "DELE", DELE, STR1, 1,	"<sp> file-name" },
	{ "CWD",  CWD,  OSTR, 1,	"[ <sp> directory-name ]" },
	{ "XCWD", CWD,	OSTR, 1,	"[ <sp> directory-name ]" },
	{ "LIST", LIST, OSTR, 1,	"[ <sp> path-name ]" },
	{ "NLST", NLST, OSTR, 1,	"[ <sp> path-name ]" },
	{ "SITE", SITE, SITECMD, 1,	"site-cmd [ <sp> arguments ]" },
	{ "SYST", SYST, ARGS, 1,	"(get type of operating system)" },
	{ "STAT", STAT, OSTR, 1,	"[ <sp> path-name ]" },
	{ "HELP", HELP, OSTR, 1,	"[ <sp> <string> ]" },
	{ "NOOP", NOOP, ARGS, 1,	"" },
	{ "MKD",  MKD,  STR1, 1,	"<sp> path-name" },
	{ "XMKD", MKD,  STR1, 1,	"<sp> path-name" },
	{ "RMD",  RMD,  STR1, 1,	"<sp> path-name" },
	{ "XRMD", RMD,  STR1, 1,	"<sp> path-name" },
	{ "PWD",  PWD,  ARGS, 1,	"(return current directory)" },
	{ "XPWD", PWD,  ARGS, 1,	"(return current directory)" },
	{ "CDUP", CDUP, ARGS, 1,	"(change to parent directory)" },
	{ "XCUP", CDUP, ARGS, 1,	"(change to parent directory)" },
	{ "STOU", STOU, STR1, 1,	"<sp> file-name" },
	{ "SIZE", SIZE, OSTR, 1,	"<sp> path-name" },
	{ "MDTM", MDTM, OSTR, 1,	"<sp> path-name" },
	{ NULL,   0,    0,    0,	0 }
};

struct tab sitetab[] = {
	{ "UMASK", UMASK, ARGS, 1,	"[ <sp> umask ]" },
	{ "IDLE", IDLE, ARGS, 1,	"[ <sp> maximum-idle-time ]" },
	{ "CHMOD", CHMOD, NSTR, 1,	"<sp> mode <sp> file-name" },
	{ "HELP", HELP, OSTR, 1,	"[ <sp> <string> ]" },
	{ NULL,   0,    0,    0,	0 }
};

struct tab *
lookup(p, cmd)
	register struct tab *p;
	char *cmd;
{

	for (; p->name != NULL; p++)
		if (strcmp(cmd, p->name) == 0)
			return (p);
	return (0);
}

#include <arpa/telnet.h>

/*
 * getline - a hacked up version of fgets to ignore TELNET escape codes.
 */
char *
getline(s, n, iop)
	char *s;
	register FILE *iop;
{
	register c;
	register char *cs;

	cs = s;
/* tmpline may contain saved command from urgent mode interruption */
	for (c = 0; tmpline[c] != '\0' && --n > 0; ++c) {
		*cs++ = tmpline[c];
		if (tmpline[c] == '\n') {
			*cs++ = '\0';
			if (debug)
				syslog(LOG_DEBUG, "command: %s", s);
			tmpline[0] = '\0';
			return(s);
		}
		if (c == 0)
			tmpline[0] = '\0';
	}
	while ((c = getc(iop)) != EOF) {
		c &= 0377;
		if (c == IAC) {
		    if ((c = getc(iop)) != EOF) {
			c &= 0377;
			switch (c) {
			case WILL:
			case WONT:
				c = getc(iop);
				printf("%c%c%c", IAC, DONT, 0377&c);
				(void) fflush(stdout);
				continue;
			case DO:
			case DONT:
				c = getc(iop);
				printf("%c%c%c", IAC, WONT, 0377&c);
				(void) fflush(stdout);
				continue;
			case IAC:
				break;
			default:
				continue;	/* ignore command */
			}
		    }
		}
		*cs++ = c;
		if (--n <= 0 || c == '\n')
			break;
	}
	if (c == EOF && cs == s)
		return (NULL);
	*cs++ = '\0';
	if (debug)
		syslog(LOG_DEBUG, "command: %s", s);
	return (s);
}

static int
toolong()
{
	time_t now;
	extern char *ctime();
	extern time_t time();

	reply(421,
	  "Timeout (%d seconds): closing control connection.", timeout);
	(void) time(&now);
	if (logging) {
		syslog(LOG_INFO,
			"User %s timed out after %d seconds at %s",
			(pw ? pw -> pw_name : "unknown"), timeout, ctime(&now));
	}
	dologout(1);
}

yylex()
{
	static int cpos, state;
	register char *cp, *cp2;
	register struct tab *p;
	int n;
	char c, *strpbrk();
	char *copy();

	for (;;) {
		switch (state) {

		case CMD:
			(void) signal(SIGALRM, toolong);
			(void) alarm((unsigned) timeout);
			if (getline(cbuf, sizeof(cbuf)-1, stdin) == NULL) {
				reply(221, "You could at least say goodbye.");
				dologout(0);
			}
			(void) alarm(0);
#ifdef SETPROCTITLE
			if (strncasecmp(cbuf, "PASS", 4) != NULL)
				setproctitle("%s: %s", proctitle, cbuf);
#endif /* SETPROCTITLE */
			if ((cp = index(cbuf, '\r'))) {
				*cp++ = '\n';
				*cp = '\0';
			}
			if ((cp = strpbrk(cbuf, " \n")))
				cpos = cp - cbuf;
			if (cpos == 0)
				cpos = 4;
			c = cbuf[cpos];
			cbuf[cpos] = '\0';
			upper(cbuf);
			p = lookup(cmdtab, cbuf);
			cbuf[cpos] = c;
			if (p != 0) {
				if (p->implemented == 0) {
					nack(p->name);
					longjmp(errcatch,0);
					/* NOTREACHED */
				}
				state = p->state;
				*(char **)&yylval = p->name;
				return (p->token);
			}
			break;

		case SITECMD:
			if (cbuf[cpos] == ' ') {
				cpos++;
				return (SP);
			}
			cp = &cbuf[cpos];
			if ((cp2 = strpbrk(cp, " \n")))
				cpos = cp2 - cbuf;
			c = cbuf[cpos];
			cbuf[cpos] = '\0';
			upper(cp);
			p = lookup(sitetab, cp);
			cbuf[cpos] = c;
			if (p != 0) {
				if (p->implemented == 0) {
					state = CMD;
					nack(p->name);
					longjmp(errcatch,0);
					/* NOTREACHED */
				}
				state = p->state;
				*(char **)&yylval = p->name;
				return (p->token);
			}
			state = CMD;
			break;

		case OSTR:
			if (cbuf[cpos] == '\n') {
				state = CMD;
				return (CRLF);
			}
			/* FALLTHROUGH */

		case STR1:
		case ZSTR1:
		dostr1:
			if (cbuf[cpos] == ' ') {
				cpos++;
				state = state == OSTR ? STR2 : ++state;
				return (SP);
			}
			break;

		case ZSTR2:
			if (cbuf[cpos] == '\n') {
				state = CMD;
				return (CRLF);
			}
			/* FALLTHROUGH */

		case STR2:
			cp = &cbuf[cpos];
			n = strlen(cp);
			cpos += n - 1;
			/*
			 * Make sure the string is nonempty and \n terminated.
			 */
			if (n > 1 && cbuf[cpos] == '\n') {
				cbuf[cpos] = '\0';
				*(char **)&yylval = copy(cp);
				cbuf[cpos] = '\n';
				state = ARGS;
				return (STRING);
			}
			break;

		case NSTR:
			if (cbuf[cpos] == ' ') {
				cpos++;
				return (SP);
			}
			if (isdigit(cbuf[cpos])) {
				cp = &cbuf[cpos];
				while (isdigit(cbuf[++cpos]))
					;
				c = cbuf[cpos];
				cbuf[cpos] = '\0';
				yylval = atoi(cp);
				cbuf[cpos] = c;
				state = STR1;
				return (NUMBER);
			}
			state = STR1;
			goto dostr1;

		case ARGS:
			if (isdigit(cbuf[cpos])) {
				cp = &cbuf[cpos];
				while (isdigit(cbuf[++cpos]))
					;
				c = cbuf[cpos];
				cbuf[cpos] = '\0';
				yylval = atoi(cp);
				cbuf[cpos] = c;
				return (NUMBER);
			}
			switch (cbuf[cpos++]) {

			case '\n':
				state = CMD;
				return (CRLF);

			case ' ':
				return (SP);

			case ',':
				return (COMMA);

			case 'A':
			case 'a':
				return (A);

			case 'B':
			case 'b':
				return (B);

			case 'C':
			case 'c':
				return (C);

			case 'E':
			case 'e':
				return (E);

			case 'F':
			case 'f':
				return (F);

			case 'I':
			case 'i':
				return (I);

			case 'L':
			case 'l':
				return (L);

			case 'N':
			case 'n':
				return (N);

			case 'P':
			case 'p':
				return (P);

			case 'R':
			case 'r':
				return (R);

			case 'S':
			case 's':
				return (S);

			case 'T':
			case 't':
				return (T);

			}
			break;

		default:
			fatal("Unknown state in scanner.");
		}
		yyerror((char *) 0);
		state = CMD;
		longjmp(errcatch,0);
	}
}

upper(s)
	register char *s;
{
	while (*s != '\0') {
		if (islower(*s))
			*s = toupper(*s);
		s++;
	}
}

char *
copy(s)
	char *s;
{
	char *p;
	extern char *malloc(), *strcpy();

	p = malloc((unsigned) strlen(s) + 1);
	if (p == NULL)
		fatal("Ran out of memory.");
	(void) strcpy(p, s);
	return (p);
}

help(ctab, s)
	struct tab *ctab;
	char *s;
{
	register struct tab *c;
	register int width, NCMDS;
	char *type;

	if (ctab == sitetab)
		type = "SITE ";
	else
		type = "";
	width = 0, NCMDS = 0;
	for (c = ctab; c->name != NULL; c++) {
		int len = strlen(c->name);

		if (len > width)
			width = len;
		NCMDS++;
	}
	width = (width + 8) &~ 7;
	if (s == 0) {
		register int i, j, w;
		int columns, lines;

		lreply(214, "The following %scommands are recognized %s.",
		    type, "(* =>'s unimplemented)");
		columns = 76 / width;
		if (columns == 0)
			columns = 1;
		lines = (NCMDS + columns - 1) / columns;
		for (i = 0; i < lines; i++) {
			printf("   ");
			for (j = 0; j < columns; j++) {
				c = ctab + j * lines + i;
				printf("%s%c", c->name,
					c->implemented ? ' ' : '*');
				if (c + lines >= &ctab[NCMDS])
					break;
				w = strlen(c->name) + 1;
				while (w < width) {
					putchar(' ');
					w++;
				}
			}
			printf("\r\n");
		}
		(void) fflush(stdout);
		reply(214, "Direct comments to ftp-bugs@%s.", hostname);
		return;
	}
	upper(s);
	c = lookup(ctab, s);
	if (c == (struct tab *)0) {
		reply(502, "Unknown command %s.", s);
		return;
	}
	if (c->implemented)
		reply(214, "Syntax: %s%s %s", type, c->name, c->help);
	else
		reply(214, "%s%-*s\t%s; unimplemented.", type, width,
		    c->name, c->help);
}

sizecmd(filename)
char *filename;
{
	switch (type) {
	case TYPE_L:
	case TYPE_I: {
		struct stat stbuf;
		if (stat(filename, &stbuf) < 0 ||
		    (stbuf.st_mode&S_IFMT) != S_IFREG)
			reply(550, "%s: not a plain file.", filename);
		else
			reply(213, "%lu", stbuf.st_size);
		break;}
	case TYPE_A: {
		FILE *fin;
		register int c, count;
		struct stat stbuf;
		fin = fopen(filename, "r");
		if (fin == NULL) {
			perror_reply(550, filename);
			return;
		}
		if (fstat(fileno(fin), &stbuf) < 0 ||
		    (stbuf.st_mode&S_IFMT) != S_IFREG) {
			reply(550, "%s: not a plain file.", filename);
			(void) fclose(fin);
			return;
		}

		count = 0;
		while((c=getc(fin)) != EOF) {
			if (c == '\n')	/* will get expanded to \r\n */
				count++;
			count++;
		}
		(void) fclose(fin);

		reply(213, "%ld", count);
		break;}
	default:
		reply(504, "SIZE not implemented for Type %c.", "?AEIL"[type]);
	}
}
#line 908 "ftp.tab.c"
#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse()
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register char *yys;
    extern char *getenv();

    if (yys = getenv("YYDEBUG"))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if (yyn = yydefred[yystate]) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yyss + yystacksize - 1)
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#ifdef lint
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#ifdef lint
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yyss + yystacksize - 1)
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 2:
#line 99 "ftp.y"
 {
			fromname = (char *) 0;
		}
break;
case 4:
#line 106 "ftp.y"
 {
			user((char *) yyvsp[-1]);
			free((char *) yyvsp[-1]);
		}
break;
case 5:
#line 111 "ftp.y"
 {
			pass((char *) yyvsp[-1]);
			free((char *) yyvsp[-1]);
		}
break;
case 6:
#line 116 "ftp.y"
 {
			usedefault = 0;
			if (pdata >= 0) {
				(void) close(pdata);
				pdata = -1;
			}
			reply(200, "PORT command successful.");
		}
break;
case 7:
#line 125 "ftp.y"
 {
			passive();
		}
break;
case 8:
#line 129 "ftp.y"
 {
			switch (cmd_type) {

			case TYPE_A:
				if (cmd_form == FORM_N) {
					reply(200, "Type set to A.");
					type = cmd_type;
					form = cmd_form;
				} else
					reply(504, "Form must be N.");
				break;

			case TYPE_E:
				reply(504, "Type E not implemented.");
				break;

			case TYPE_I:
				reply(200, "Type set to I.");
				type = cmd_type;
				break;

			case TYPE_L:
#if NBBY == 8
				if (cmd_bytesz == 8) {
					reply(200,
					    "Type set to L (byte size 8).");
					type = cmd_type;
				} else
					reply(504, "Byte size must be 8.");
#else /* NBBY == 8 */
				UNIMPLEMENTED for NBBY != 8
#endif /* NBBY == 8 */
			}
		}
break;
case 9:
#line 164 "ftp.y"
 {
			switch (yyvsp[-1]) {

			case STRU_F:
				reply(200, "STRU F ok.");
				break;

			default:
				reply(504, "Unimplemented STRU type.");
			}
		}
break;
case 10:
#line 176 "ftp.y"
 {
			switch (yyvsp[-1]) {

			case MODE_S:
				reply(200, "MODE S ok.");
				break;

			default:
				reply(502, "Unimplemented MODE type.");
			}
		}
break;
case 11:
#line 188 "ftp.y"
 {
			reply(202, "ALLO command ignored.");
		}
break;
case 12:
#line 192 "ftp.y"
 {
			reply(202, "ALLO command ignored.");
		}
break;
case 13:
#line 196 "ftp.y"
 {
			if (yyvsp[-3] && yyvsp[-1] != NULL)
				retrieve((char *) 0, (char *) yyvsp[-1]);
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 14:
#line 203 "ftp.y"
 {
			if (yyvsp[-3] && yyvsp[-1] != NULL)
				store((char *) yyvsp[-1], "w", 0);
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 15:
#line 210 "ftp.y"
 {
			if (yyvsp[-3] && yyvsp[-1] != NULL)
				store((char *) yyvsp[-1], "a", 0);
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 16:
#line 217 "ftp.y"
 {
			if (yyvsp[-1])
				send_file_list(".");
		}
break;
case 17:
#line 222 "ftp.y"
 {
			if (yyvsp[-3] && yyvsp[-1] != NULL) 
				send_file_list((char *) yyvsp[-1]);
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 18:
#line 229 "ftp.y"
 {
			if (yyvsp[-1])
				retrieve("/bin/ls -lgA", "");
		}
break;
case 19:
#line 234 "ftp.y"
 {
			if (yyvsp[-3] && yyvsp[-1] != NULL)
				retrieve("/bin/ls -lgA %s", (char *) yyvsp[-1]);
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 20:
#line 241 "ftp.y"
 {
			if (yyvsp[-3] && yyvsp[-1] != NULL)
				statfilecmd((char *) yyvsp[-1]);
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 21:
#line 248 "ftp.y"
 {
			statcmd();
		}
break;
case 22:
#line 252 "ftp.y"
 {
			if (yyvsp[-3] && yyvsp[-1] != NULL)
				delete((char *) yyvsp[-1]);
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 23:
#line 259 "ftp.y"
 {
			if (fromname) {
				renamecmd(fromname, (char *) yyvsp[-1]);
				free(fromname);
				fromname = (char *) 0;
			} else {
				reply(503, "Bad sequence of commands.");
			}
			free((char *) yyvsp[-1]);
		}
break;
case 24:
#line 270 "ftp.y"
 {
			reply(225, "ABOR command successful.");
		}
break;
case 25:
#line 274 "ftp.y"
 {
			if (yyvsp[-1])
				cwd(pw->pw_dir);
		}
break;
case 26:
#line 279 "ftp.y"
 {
			if (yyvsp[-3] && yyvsp[-1] != NULL)
				cwd((char *) yyvsp[-1]);
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 27:
#line 286 "ftp.y"
 {
			help(cmdtab, (char *) 0);
		}
break;
case 28:
#line 290 "ftp.y"
 {
			register char *cp = (char *)yyvsp[-1];

			if (strncasecmp(cp, "SITE", 4) == 0) {
				cp = (char *)yyvsp[-1] + 4;
				if (*cp == ' ')
					cp++;
				if (*cp)
					help(sitetab, cp);
				else
					help(sitetab, (char *) 0);
			} else
				help(cmdtab, (char *) yyvsp[-1]);
		}
break;
case 29:
#line 305 "ftp.y"
 {
			reply(200, "NOOP command successful.");
		}
break;
case 30:
#line 309 "ftp.y"
 {
			if (yyvsp[-3] && yyvsp[-1] != NULL)
				makedir((char *) yyvsp[-1]);
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 31:
#line 316 "ftp.y"
 {
			if (yyvsp[-3] && yyvsp[-1] != NULL)
				removedir((char *) yyvsp[-1]);
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 32:
#line 323 "ftp.y"
 {
			if (yyvsp[-1])
				pwd();
		}
break;
case 33:
#line 328 "ftp.y"
 {
			if (yyvsp[-1])
				cwd("..");
		}
break;
case 34:
#line 333 "ftp.y"
 {
			help(sitetab, (char *) 0);
		}
break;
case 35:
#line 337 "ftp.y"
 {
			help(sitetab, (char *) yyvsp[-1]);
		}
break;
case 36:
#line 341 "ftp.y"
 {
			int oldmask;

			if (yyvsp[-1]) {
				oldmask = umask(0);
				(void) umask(oldmask);
				reply(200, "Current UMASK is %03o", oldmask);
			}
		}
break;
case 37:
#line 351 "ftp.y"
 {
			int oldmask;

			if (yyvsp[-3]) {
				if ((yyvsp[-1] == -1) || (yyvsp[-1] > 0777)) {
					reply(501, "Bad UMASK value");
				} else {
					oldmask = umask(yyvsp[-1]);
					reply(200,
					    "UMASK set to %03o (was %03o)",
					    yyvsp[-1], oldmask);
				}
			}
		}
break;
case 38:
#line 366 "ftp.y"
 {
			if (yyvsp[-5] && (yyvsp[-1] != NULL)) {
				if (yyvsp[-3] > 0777)
					reply(501,
				"CHMOD: Mode value must be between 0 and 0777");
				else if (chmod((char *) yyvsp[-1], yyvsp[-3]) < 0)
					perror_reply(550, (char *) yyvsp[-1]);
				else
					reply(200, "CHMOD command successful.");
			}
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 39:
#line 380 "ftp.y"
 {
			reply(200,
			    "Current IDLE time limit is %d seconds; max %d",
				timeout, maxtimeout);
		}
break;
case 40:
#line 386 "ftp.y"
 {
			if (yyvsp[-1] < 30 || yyvsp[-1] > maxtimeout) {
				reply(501,
			"Maximum IDLE time must be between 30 and %d seconds",
				    maxtimeout);
			} else {
				timeout = yyvsp[-1];
				(void) alarm((unsigned) timeout);
				reply(200,
				    "Maximum IDLE time set to %d seconds",
				    timeout);
			}
		}
break;
case 41:
#line 400 "ftp.y"
 {
			if (yyvsp[-3] && yyvsp[-1] != NULL)
				store((char *) yyvsp[-1], "w", 1);
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 42:
#line 407 "ftp.y"
 {
#ifdef unix
#ifdef BSD
			reply(215, "UNIX Type: L%d Version: BSD-%d",
				NBBY, BSD);
#else /* BSD */
			reply(215, "UNIX Type: L%d", NBBY);
#endif /* BSD */
#else /* unix */
			reply(215, "UNKNOWN Type: L%d", NBBY);
#endif /* unix */
		}
break;
case 43:
#line 428 "ftp.y"
 {
			if (yyvsp[-3] && yyvsp[-1] != NULL)
				sizecmd((char *) yyvsp[-1]);
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 44:
#line 445 "ftp.y"
 {
			if (yyvsp[-3] && yyvsp[-1] != NULL) {
				struct stat stbuf;
				if (stat((char *) yyvsp[-1], &stbuf) < 0)
					perror_reply(550, "%s", (char *) yyvsp[-1]);
				else if ((stbuf.st_mode&S_IFMT) != S_IFREG) {
					reply(550, "%s: not a plain file.",
						(char *) yyvsp[-1]);
				} else {
					register struct tm *t;
					struct tm *gmtime();
					t = gmtime(&stbuf.st_mtime);
					reply(213,
					    "19%02d%02d%02d%02d%02d%02d",
					    t->tm_year, t->tm_mon+1, t->tm_mday,
					    t->tm_hour, t->tm_min, t->tm_sec);
				}
			}
			if (yyvsp[-1] != NULL)
				free((char *) yyvsp[-1]);
		}
break;
case 45:
#line 467 "ftp.y"
 {
			reply(221, "Goodbye.");
			dologout(0);
		}
break;
case 46:
#line 472 "ftp.y"
 {
			yyerrok;
		}
break;
case 47:
#line 477 "ftp.y"
 {
			char *renamefrom();

			if (yyvsp[-3] && yyvsp[-1]) {
				fromname = renamefrom((char *) yyvsp[-1]);
				if (fromname == (char *) 0 && yyvsp[-1]) {
					free((char *) yyvsp[-1]);
				}
			}
		}
break;
case 49:
#line 493 "ftp.y"
 {
			*(char **)&(yyval) = "";
		}
break;
case 52:
#line 504 "ftp.y"
 {
			register char *a, *p;

			a = (char *)&data_dest.sin_addr;
			a[0] = yyvsp[-10]; a[1] = yyvsp[-8]; a[2] = yyvsp[-6]; a[3] = yyvsp[-4];
			p = (char *)&data_dest.sin_port;
			p[0] = yyvsp[-2]; p[1] = yyvsp[0];
			data_dest.sin_family = AF_INET;
		}
break;
case 53:
#line 516 "ftp.y"
 {
		yyval = FORM_N;
	}
break;
case 54:
#line 520 "ftp.y"
 {
		yyval = FORM_T;
	}
break;
case 55:
#line 524 "ftp.y"
 {
		yyval = FORM_C;
	}
break;
case 56:
#line 530 "ftp.y"
 {
		cmd_type = TYPE_A;
		cmd_form = FORM_N;
	}
break;
case 57:
#line 535 "ftp.y"
 {
		cmd_type = TYPE_A;
		cmd_form = yyvsp[0];
	}
break;
case 58:
#line 540 "ftp.y"
 {
		cmd_type = TYPE_E;
		cmd_form = FORM_N;
	}
break;
case 59:
#line 545 "ftp.y"
 {
		cmd_type = TYPE_E;
		cmd_form = yyvsp[0];
	}
break;
case 60:
#line 550 "ftp.y"
 {
		cmd_type = TYPE_I;
	}
break;
case 61:
#line 554 "ftp.y"
 {
		cmd_type = TYPE_L;
		cmd_bytesz = NBBY;
	}
break;
case 62:
#line 559 "ftp.y"
 {
		cmd_type = TYPE_L;
		cmd_bytesz = yyvsp[0];
	}
break;
case 63:
#line 565 "ftp.y"
 {
		cmd_type = TYPE_L;
		cmd_bytesz = yyvsp[0];
	}
break;
case 64:
#line 572 "ftp.y"
 {
		yyval = STRU_F;
	}
break;
case 65:
#line 576 "ftp.y"
 {
		yyval = STRU_R;
	}
break;
case 66:
#line 580 "ftp.y"
 {
		yyval = STRU_P;
	}
break;
case 67:
#line 586 "ftp.y"
 {
		yyval = MODE_S;
	}
break;
case 68:
#line 590 "ftp.y"
 {
		yyval = MODE_B;
	}
break;
case 69:
#line 594 "ftp.y"
 {
		yyval = MODE_C;
	}
break;
case 70:
#line 600 "ftp.y"
 {
		/*
		 * Problem: this production is used for all pathname
		 * processing, but only gives a 550 error reply.
		 * This is a valid reply in some cases but not in others.
		 */
		if (logged_in && yyvsp[0] && strncmp((char *) yyvsp[0], "~", 1) == 0) {
			*(char **)&(yyval) = *glob((char *) yyvsp[0]);
			if (globerr != NULL) {
				reply(550, globerr);
				yyval = NULL;
			}
			free((char *) yyvsp[0]);
		} else
			yyval = yyvsp[0];
	}
break;
case 72:
#line 622 "ftp.y"
 {
		register int ret, dec, multby, digit;

		/*
		 * Convert a number that was read as decimal number
		 * to what it would be if it had been read as octal.
		 */
		dec = yyvsp[0];
		multby = 1;
		ret = 0;
		while (dec) {
			digit = dec%10;
			if (digit > 7) {
				ret = -1;
				break;
			}
			ret += digit * multby;
			multby *= 8;
			dec /= 10;
		}
		yyval = ret;
	}
break;
case 73:
#line 647 "ftp.y"
 {
		if (logged_in)
			yyval = 1;
		else {
			reply(530, "Please login with USER and PASS.");
			yyval = 0;
		}
	}
break;
#line 1688 "ftp.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yyss + yystacksize - 1)
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
