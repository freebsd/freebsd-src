#ifndef lint
static const char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif

#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9

#define YYEMPTY        (-1)
#define yyclearin      (yychar = YYEMPTY)
#define yyerrok        (yyerrflag = 0)
#define YYRECOVERING() (yyerrflag != 0)


#ifndef yyparse
#define yyparse    ftp_parse
#endif /* yyparse */

#ifndef yylex
#define yylex      ftp_lex
#endif /* yylex */

#ifndef yyerror
#define yyerror    ftp_error
#endif /* yyerror */

#ifndef yychar
#define yychar     ftp_char
#endif /* yychar */

#ifndef yyval
#define yyval      ftp_val
#endif /* yyval */

#ifndef yylval
#define yylval     ftp_lval
#endif /* yylval */

#ifndef yydebug
#define yydebug    ftp_debug
#endif /* yydebug */

#ifndef yynerrs
#define yynerrs    ftp_nerrs
#endif /* yynerrs */

#ifndef yyerrflag
#define yyerrflag  ftp_errflag
#endif /* yyerrflag */

#ifndef yylhs
#define yylhs      ftp_lhs
#endif /* yylhs */

#ifndef yylen
#define yylen      ftp_len
#endif /* yylen */

#ifndef yydefred
#define yydefred   ftp_defred
#endif /* yydefred */

#ifndef yydgoto
#define yydgoto    ftp_dgoto
#endif /* yydgoto */

#ifndef yysindex
#define yysindex   ftp_sindex
#endif /* yysindex */

#ifndef yyrindex
#define yyrindex   ftp_rindex
#endif /* yyrindex */

#ifndef yygindex
#define yygindex   ftp_gindex
#endif /* yygindex */

#ifndef yytable
#define yytable    ftp_table
#endif /* yytable */

#ifndef yycheck
#define yycheck    ftp_check
#endif /* yycheck */

#ifndef yyname
#define yyname     ftp_name
#endif /* yyname */

#ifndef yyrule
#define yyrule     ftp_rule
#endif /* yyrule */
#define YYPREFIX "ftp_"

#define YYPURE 0

#line 26 "ftp.y"

/* sccsid[] = "@(#)ftpcmd.y	5.20.1.1 (Berkeley) 3/2/89"; */

#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/ftp.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <pwd.h>
#include <setjmp.h>
#include <syslog.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#ifdef YYBISON
int yylex(void);
static void yyerror(const char *);
#endif

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

extern char **glob(char *);
extern char *renamefrom(char *);
extern void cwd(const char *);

extern void dologout(int);
extern void fatal(const char *);
extern void makedir(const char *);
extern void nack(const char *);
extern void pass(const char *);
extern void passive(void);
extern void pwd(void);
extern void removedir(char *);
extern void renamecmd(char *, char *);
extern void retrieve(const char *, const char *);
extern void send_file_list(const char *);
extern void statcmd(void);
extern void statfilecmd(const char *);
extern void store(char *, const char *, int);
extern void user(const char *);

extern void perror_reply(int, const char *, ...);
extern void reply(int, const char *, ...);
extern void lreply(int, const char *, ...);

static	int cmd_type;
static	int cmd_form;
static	int cmd_bytesz;
char	cbuf[512];
char	*fromname;

struct tab {
	const char *name;
	short	token;
	short	state;
	short	implemented;	/* 1 if command is implemented */
	const char *help;
};

static char * copy(const char *);

#ifdef YYBISON
static void sizecmd(char *filename);
static void help(struct tab *ctab, char *s);
struct tab cmdtab[];
struct tab sitetab[];
#endif

static void
yyerror(const char *msg)
{
	perror(msg);
}
#line 126 "ftp.y"
#ifdef YYSTYPE
#undef  YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
#endif
#ifndef YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
typedef union
{
	int ival;
	char *sval;
} YYSTYPE;
#endif /* !YYSTYPE_IS_DECLARED */
#line 211 "ftp.tab.c"

/* compatibility with bison */
#ifdef YYPARSE_PARAM
/* compatibility with FreeBSD */
# ifdef YYPARSE_PARAM_TYPE
#  define YYPARSE_DECL() yyparse(YYPARSE_PARAM_TYPE YYPARSE_PARAM)
# else
#  define YYPARSE_DECL() yyparse(void *YYPARSE_PARAM)
# endif
#else
# define YYPARSE_DECL() yyparse(void)
#endif

/* Parameters sent to lex. */
#ifdef YYLEX_PARAM
# define YYLEX_DECL() yylex(void *YYLEX_PARAM)
# define YYLEX yylex(YYLEX_PARAM)
#else
# define YYLEX_DECL() yylex(void)
# define YYLEX yylex()
#endif

/* Parameters sent to yyerror. */
#ifndef YYERROR_DECL
#define YYERROR_DECL() yyerror(const char *s)
#endif
#ifndef YYERROR_CALL
#define YYERROR_CALL(msg) yyerror(msg)
#endif

extern int YYPARSE_DECL();

#define NUMBER 257
#define STRING 258
#define A 259
#define B 260
#define C 261
#define E 262
#define F 263
#define I 264
#define L 265
#define N 266
#define P 267
#define R 268
#define S 269
#define T 270
#define SP 271
#define CRLF 272
#define COMMA 273
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
static const short ftp_lhs[] = {                         -1,
    0,    0,    0,   11,   11,   11,   11,   11,   11,   11,
   11,   11,   11,   11,   11,   11,   11,   11,   11,   11,
   11,   11,   11,   11,   11,   11,   11,   11,   11,   11,
   11,   11,   11,   11,   11,   11,   11,   11,   11,   11,
   11,   11,   11,   11,   11,   11,   12,   10,    7,    7,
    1,   13,    3,    3,    3,   14,   14,   14,   14,   14,
   14,   14,   14,    6,    6,    6,    4,    4,    4,    8,
    9,    5,    2,
};
static const short ftp_len[] = {                          2,
    0,    2,    2,    4,    4,    4,    2,    4,    4,    4,
    4,    8,    5,    5,    5,    3,    5,    3,    5,    5,
    2,    5,    4,    2,    3,    5,    2,    4,    2,    5,
    5,    3,    3,    4,    6,    5,    7,    9,    4,    6,
    5,    2,    5,    5,    2,    2,    5,    1,    0,    1,
    1,   11,    1,    1,    1,    1,    3,    1,    3,    1,
    1,    3,    2,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    0,
};
static const short ftp_defred[] = {                       1,
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
    0,    0,   51,    0,   63,    8,    9,   10,    0,    0,
    0,    0,   11,    0,   23,    0,    0,    0,    0,    0,
   34,    0,    0,   39,    0,    0,   28,    0,    0,    0,
    0,    0,    0,   55,   53,   54,   57,   59,   62,   13,
   14,   15,    0,   47,   22,   26,   19,   17,    0,    0,
   36,    0,    0,   20,   30,   31,   41,   43,   44,    0,
    0,   35,   72,    0,   40,    0,    0,    0,   37,    0,
    0,   12,    0,    0,   38,    0,    0,    0,   52,
};
static const short ftp_dgoto[] = {                        1,
  125,   45,  157,   88,  184,   84,   73,   95,   96,   71,
   34,   35,   75,   80,
};
static const short ftp_sindex[] = {                       0,
 -224, -256, -248, -241, -239, -233, -225, -218, -200, -165,
    0,    0,    0, -164,    0, -163, -176,    0,    0,    0,
    0, -162, -161, -231, -160,    0,    0,    0,    0,    0,
 -159,    0,    0,    0,    0,    0, -240, -148,    0, -143,
    0, -252, -175, -255, -156, -155, -154, -139, -152, -138,
    0, -149, -205, -203, -177, -253,    0, -147, -133,    0,
    0, -145, -144, -142, -141, -137,    0, -136, -135,    0,
 -140,    0, -134, -132, -130, -131, -128,    0, -254, -127,
    0,    0,    0, -126,    0,    0,    0, -125, -138, -138,
 -138, -174, -138,    0, -124,    0, -138, -138,    0, -138,
    0, -129,    0, -172,    0, -169,    0, -138, -123, -138,
 -138,    0,    0, -138, -138, -138,    0,    0, -120,    0,
 -246, -246,    0, -118,    0,    0,    0,    0, -122, -121,
 -119, -116,    0, -117,    0, -115, -114, -113, -112, -104,
    0, -167, -101,    0, -110, -109,    0, -108, -107, -106,
 -105, -103, -111,    0,    0,    0,    0,    0,    0,    0,
    0,    0, -100,    0,    0,    0,    0,    0, -102,  -85,
    0,  -99,  -85,    0,    0,    0,    0,    0,    0,  -83,
  -82,    0,    0,  -96,    0,  -94,  -95,  -93,    0, -138,
  -77,    0,  -91,  -90,    0,  -75,  -88,  -73,    0,
};
static const short ftp_rindex[] = {                       0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  -84,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  -86,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  -81,  -80,    0, -160,    0,
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
static const short ftp_gindex[] = {                       0,
    4,   16,   11,    0,  -29,    0,    0,  -89,    0,    0,
    0,    0,    0,    0,
};
#define YYTABLESIZE 192
static const short ftp_table[] = {                      129,
  130,  131,  123,  134,   85,   86,   76,  136,  137,   77,
  138,   78,   79,   87,  154,   36,  124,   70,  146,  155,
  148,  149,   37,  156,  150,  151,  152,   46,   47,   38,
   49,    2,   39,   52,   53,   54,   55,   40,   58,   59,
   60,   62,   63,   64,   65,   66,   41,   68,   69,    3,
    4,  104,   42,    5,    6,    7,    8,    9,   10,   11,
   12,   13,  105,  106,  107,   98,   99,  100,  101,   14,
   43,   15,   16,   17,   18,   19,   20,   21,   22,   23,
   24,   25,   26,   27,   28,   29,   30,   81,   31,   32,
   33,   82,   83,  102,  103,   51,  132,  133,  140,  141,
  193,  143,  144,  170,  171,   44,   48,   50,   56,   72,
   57,   61,   67,   74,   89,   90,   91,   92,   93,   94,
  142,   97,  145,  108,  109,  110,  111,  159,  139,  112,
  113,  117,  158,  114,  115,  116,  153,  118,  123,  121,
  119,  120,  122,  186,  126,  127,  128,  135,  147,  160,
  161,  163,  162,  169,  164,  172,  165,  166,  167,  168,
  173,  180,  174,  175,  176,  177,  178,    0,  179,  182,
  181,  183,  185,  187,  188,  189,  190,  191,  192,  194,
  195,  197,  196,  199,  198,   49,   73,    0,    0,    0,
   56,   58,
};
static const short ftp_check[] = {                       89,
   90,   91,  257,   93,  260,  261,  259,   97,   98,  262,
  100,  264,  265,  269,  261,  272,  271,  258,  108,  266,
  110,  111,  271,  270,  114,  115,  116,   12,   13,  271,
   15,  256,  272,   18,   19,   20,   21,  271,   23,  271,
  272,   26,   27,   28,   29,   30,  272,   32,   33,  274,
  275,  305,  271,  278,  279,  280,  281,  282,  283,  284,
  285,  286,  316,  317,  318,  271,  272,  271,  272,  294,
  271,  296,  297,  298,  299,  300,  301,  302,  303,  304,
  305,  306,  307,  308,  309,  310,  311,  263,  313,  314,
  315,  267,  268,  271,  272,  272,  271,  272,  271,  272,
  190,  271,  272,  271,  272,  271,  271,  271,  271,  258,
  272,  272,  272,  257,  271,  271,  271,  257,  271,  258,
  105,  271,  107,  271,  258,  271,  271,  124,  258,  272,
  272,  272,  122,  271,  271,  271,  257,  272,  257,  271,
  273,  272,  271,  173,  272,  272,  272,  272,  272,  272,
  272,  268,  272,  258,  272,  257,  272,  272,  272,  272,
  271,  273,  272,  272,  272,  272,  272,   -1,  272,  272,
  271,  257,  272,  257,  257,  272,  271,  273,  272,  257,
  272,  257,  273,  257,  273,  272,  271,   -1,   -1,   -1,
  272,  272,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 319
#define YYTRANSLATE(a) ((a) > YYMAXTOKEN ? (YYMAXTOKEN + 1) : (a))
#if YYDEBUG
static const char *yyname[] = {

"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"NUMBER","STRING","A","B","C","E",
"F","I","L","N","P","R","S","T","SP","CRLF","COMMA","USER","PASS","ACCT","REIN",
"QUIT","PORT","PASV","TYPE","STRU","MODE","RETR","STOR","APPE","MLFL","MAIL",
"MSND","MSOM","MSAM","MRSQ","MRCP","ALLO","REST","RNFR","RNTO","ABOR","DELE",
"CWD","LIST","NLST","SITE","STAT","HELP","NOOP","MKD","RMD","PWD","CDUP","STOU",
"SMNT","SYST","SIZE","MDTM","UMASK","IDLE","CHMOD","LEXERR","illegal-symbol",
};
static const char *yyrule[] = {
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

int      yydebug;
int      yynerrs;

int      yyerrflag;
int      yychar;
YYSTYPE  yyval;
YYSTYPE  yylval;

/* define the initial stack-sizes */
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH  YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH  10000
#endif
#endif

#define YYINITSTACKSIZE 200

typedef struct {
    unsigned stacksize;
    short    *s_base;
    short    *s_mark;
    short    *s_last;
    YYSTYPE  *l_base;
    YYSTYPE  *l_mark;
} YYSTACKDATA;
/* variables for the parser stack */
static YYSTACKDATA yystack;
#line 733 "ftp.y"

#ifdef YYBYACC
extern int YYLEX_DECL();
#endif

extern jmp_buf errcatch;

static void upper(char *);

#define	CMD	0	/* beginning of command */
#define	ARGS	1	/* expect miscellaneous arguments */
#define	STR1	2	/* expect SP followed by STRING */
#define	STR2	3	/* expect STRING */
#define	OSTR	4	/* optional SP then STRING */
#define	ZSTR1	5	/* SP then optional STRING */
#define	ZSTR2	6	/* optional STRING after SP */
#define	SITECMD	7	/* SITE command */
#define	NSTR	8	/* Number followed by a string */

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
	{ 0,   0,    0,    0,	0 }
};

struct tab sitetab[] = {
	{ "UMASK", UMASK, ARGS, 1,	"[ <sp> umask ]" },
	{ "IDLE", IDLE, ARGS, 1,	"[ <sp> maximum-idle-time ]" },
	{ "CHMOD", CHMOD, NSTR, 1,	"<sp> mode <sp> file-name" },
	{ "HELP", HELP, OSTR, 1,	"[ <sp> <string> ]" },
	{ 0,   0,    0,    0,	0 }
};

static struct tab *
lookup(struct tab *p, char *cmd)
{

	for (; p->name != 0; p++)
		if (strcmp(cmd, p->name) == 0)
			return (p);
	return (0);
}

#include <arpa/telnet.h>

/*
 * get_line - a hacked up version of fgets to ignore TELNET escape codes.
 */
static char *
get_line(char *s, int n, FILE *iop)
{
	register int c;
	register char *cs;

	cs = s;
/* tmpline may contain saved command from urgent mode interruption */
	for (c = 0; tmpline[c] != '\0' && --n > 0; ++c) {
		*cs++ = tmpline[c];
		if (tmpline[c] == '\n') {
			*cs = '\0';
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
		*cs++ = (char) c;
		if (--n <= 0 || c == '\n')
			break;
	}
	if (c == EOF && cs == s)
		return (0);
	*cs = '\0';
	if (debug)
		syslog(LOG_DEBUG, "command: %s", s);
	return (s);
}

static void
toolong(int sig)
{
	time_t now;

	(void) sig;
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

int
yylex(void)
{
	static int cpos, state;
	register char *cp, *cp2;
	register struct tab *p;
	int n;
	char c;

	for (;;) {
		switch (state) {

		case CMD:
			(void) signal(SIGALRM, toolong);
			(void) alarm((unsigned) timeout);
			if (get_line(cbuf, sizeof(cbuf)-1, stdin) == 0) {
				reply(221, "You could at least say goodbye.");
				dologout(0);
			}
			(void) alarm(0);
#ifdef SETPROCTITLE
			if (strncasecmp(cbuf, "PASS", 4) != 0)
				setproctitle("%s: %s", proctitle, cbuf);
#endif /* SETPROCTITLE */
			if ((cp = strchr(cbuf, '\r'))) {
				*cp++ = '\n';
				*cp = '\0';
			}
			if ((cp = strpbrk(cbuf, " \n")))
				cpos = (int) (cp - cbuf);
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
				*(const char **)(&yylval) = p->name;
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
				cpos = (int) (cp2 - cbuf);
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
				*(const char **)(&yylval) = p->name;
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
				if (state == OSTR)
					state = STR2;
				else
					++state;
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
			n = (int) strlen(cp);
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
				yylval.ival = atoi(cp);
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
				yylval.ival = atoi(cp);
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

static void
upper(char *s)
{
	while (*s != '\0') {
		if (islower(*s))
			*s = toupper(*s);
		s++;
	}
}

static char *
copy(const char *s)
{
	char *p;

	p = (char * )malloc(strlen(s) + 1);
	if (p == 0)
		fatal("Ran out of memory.");
	else
		(void) strcpy(p, s);
	return (p);
}

static void
help(struct tab *ctab, char *s)
{
	register struct tab *c;
	register int width, NCMDS;
	const char *help_type;

	if (ctab == sitetab)
		help_type = "SITE ";
	else
		help_type = "";
	width = 0, NCMDS = 0;
	for (c = ctab; c->name != 0; c++) {
		int len = (int) strlen(c->name);

		if (len > width)
			width = len;
		NCMDS++;
	}
	width = (width + 8) &~ 7;
	if (s == 0) {
		register int i, j, w;
		int columns, lines;

		lreply(214, "The following %scommands are recognized %s.",
		    help_type, "(* =>'s unimplemented)");
		columns = 76 / width;
		if (columns == 0)
			columns = 1;
		lines = (NCMDS + columns - 1) / columns;
		for (i = 0; i < lines; i++) {
			printf("   ");
			for (j = 0; j < columns; j++) {
				c = ctab + j * lines + i;
				assert(c->name != 0);
				printf("%s%c", c->name,
					c->implemented ? ' ' : '*');
				if (c + lines >= &ctab[NCMDS])
					break;
				w = (int) strlen(c->name) + 1;
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
		reply(214, "Syntax: %s%s %s", help_type, c->name, c->help);
	else
		reply(214, "%s%-*s\t%s; unimplemented.", help_type, width,
		    c->name, c->help);
}

static void
sizecmd(char *filename)
{
	switch (type) {
	case TYPE_L:
	case TYPE_I: {
		struct stat stbuf;
		if (stat(filename, &stbuf) < 0 ||
		    (stbuf.st_mode&S_IFMT) != S_IFREG)
			reply(550, "%s: not a plain file.", filename);
		else
#ifdef HAVE_LONG_LONG
			reply(213, "%llu", (long long) stbuf.st_size);
#else
			reply(213, "%lu", stbuf.st_size);
#endif
		break;}
	case TYPE_A: {
		FILE *fin;
		register int c, count;
		struct stat stbuf;
		fin = fopen(filename, "r");
		if (fin == 0) {
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
#line 1103 "ftp.tab.c"

#if YYDEBUG
#include <stdio.h>		/* needed for printf */
#endif

#include <stdlib.h>	/* needed for malloc, etc */
#include <string.h>	/* needed for memset */

/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(YYSTACKDATA *data)
{
    int i;
    unsigned newsize;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = data->stacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;

    i = (int) (data->s_mark - data->s_base);
    newss = (short *)realloc(data->s_base, newsize * sizeof(*newss));
    if (newss == 0)
        return -1;

    data->s_base = newss;
    data->s_mark = newss + i;

    newvs = (YYSTYPE *)realloc(data->l_base, newsize * sizeof(*newvs));
    if (newvs == 0)
        return -1;

    data->l_base = newvs;
    data->l_mark = newvs + i;

    data->stacksize = newsize;
    data->s_last = data->s_base + newsize - 1;
    return 0;
}

#if YYPURE || defined(YY_NO_LEAKS)
static void yyfreestack(YYSTACKDATA *data)
{
    free(data->s_base);
    free(data->l_base);
    memset(data, 0, sizeof(*data));
}
#else
#define yyfreestack(data) /* nothing */
#endif

#define YYABORT  goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR  goto yyerrlab

int
YYPARSE_DECL()
{
    int yym, yyn, yystate;
#if YYDEBUG
    const char *yys;

    if ((yys = getenv("YYDEBUG")) != 0)
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = YYEMPTY;
    yystate = 0;

#if YYPURE
    memset(&yystack, 0, sizeof(yystack));
#endif

    if (yystack.s_base == NULL && yygrowstack(&yystack)) goto yyoverflow;
    yystack.s_mark = yystack.s_base;
    yystack.l_mark = yystack.l_base;
    yystate = 0;
    *yystack.s_mark = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = YYLEX) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = yyname[YYTRANSLATE(yychar)];
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
        if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack))
        {
            goto yyoverflow;
        }
        yystate = yytable[yyn];
        *++yystack.s_mark = yytable[yyn];
        *++yystack.l_mark = yylval;
        yychar = YYEMPTY;
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

    yyerror("syntax error");

    goto yyerrlab;

yyerrlab:
    ++yynerrs;

yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yystack.s_mark]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yystack.s_mark, yytable[yyn]);
#endif
                if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack))
                {
                    goto yyoverflow;
                }
                yystate = yytable[yyn];
                *++yystack.s_mark = yytable[yyn];
                *++yystack.l_mark = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yystack.s_mark);
#endif
                if (yystack.s_mark <= yystack.s_base) goto yyabort;
                --yystack.s_mark;
                --yystack.l_mark;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = yyname[YYTRANSLATE(yychar)];
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = YYEMPTY;
        goto yyloop;
    }

yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    if (yym)
        yyval = yystack.l_mark[1-yym];
    else
        memset(&yyval, 0, sizeof yyval);
    switch (yyn)
    {
case 2:
#line 172 "ftp.y"
	{
			fromname = (char *) 0;
		}
break;
case 4:
#line 179 "ftp.y"
	{
			user(yystack.l_mark[-1].sval);
			free(yystack.l_mark[-1].sval);
		}
break;
case 5:
#line 184 "ftp.y"
	{
			pass(yystack.l_mark[-1].sval);
			free(yystack.l_mark[-1].sval);
		}
break;
case 6:
#line 189 "ftp.y"
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
#line 198 "ftp.y"
	{
			passive();
		}
break;
case 8:
#line 202 "ftp.y"
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
#line 237 "ftp.y"
	{
			switch (yystack.l_mark[-1].ival) {

			case STRU_F:
				reply(200, "STRU F ok.");
				break;

			default:
				reply(504, "Unimplemented STRU type.");
			}
		}
break;
case 10:
#line 249 "ftp.y"
	{
			switch (yystack.l_mark[-1].ival) {

			case MODE_S:
				reply(200, "MODE S ok.");
				break;

			default:
				reply(502, "Unimplemented MODE type.");
			}
		}
break;
case 11:
#line 261 "ftp.y"
	{
			reply(202, "ALLO command ignored.");
		}
break;
case 12:
#line 265 "ftp.y"
	{
			reply(202, "ALLO command ignored.");
		}
break;
case 13:
#line 269 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval != 0)
				retrieve((char *) 0, yystack.l_mark[-1].sval);
			if (yystack.l_mark[-1].sval != 0)
				free(yystack.l_mark[-1].sval);
		}
break;
case 14:
#line 276 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval != 0)
				store(yystack.l_mark[-1].sval, "w", 0);
			if (yystack.l_mark[-1].sval != 0)
				free(yystack.l_mark[-1].sval);
		}
break;
case 15:
#line 283 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval != 0)
				store(yystack.l_mark[-1].sval, "a", 0);
			if (yystack.l_mark[-1].sval != 0)
				free(yystack.l_mark[-1].sval);
		}
break;
case 16:
#line 290 "ftp.y"
	{
			if (yystack.l_mark[-1].ival)
				send_file_list(".");
		}
break;
case 17:
#line 295 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval != 0)
				send_file_list((char *) yystack.l_mark[-1].sval);
			if (yystack.l_mark[-1].sval != 0)
				free((char *) yystack.l_mark[-1].sval);
		}
break;
case 18:
#line 302 "ftp.y"
	{
			if (yystack.l_mark[-1].ival)
				retrieve("/bin/ls -lgA", "");
		}
break;
case 19:
#line 307 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval != 0)
				retrieve("/bin/ls -lgA %s", yystack.l_mark[-1].sval);
			if (yystack.l_mark[-1].sval != 0)
				free(yystack.l_mark[-1].sval);
		}
break;
case 20:
#line 314 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval != 0)
				statfilecmd(yystack.l_mark[-1].sval);
			if (yystack.l_mark[-1].sval != 0)
				free(yystack.l_mark[-1].sval);
		}
break;
case 21:
#line 321 "ftp.y"
	{
			statcmd();
		}
break;
case 22:
#line 325 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval != 0)
				remove((char *) yystack.l_mark[-1].sval);
			if (yystack.l_mark[-1].sval != 0)
				free((char *) yystack.l_mark[-1].sval);
		}
break;
case 23:
#line 332 "ftp.y"
	{
			if (fromname) {
				renamecmd(fromname, (char *) yystack.l_mark[-1].sval);
				free(fromname);
				fromname = (char *) 0;
			} else {
				reply(503, "Bad sequence of commands.");
			}
			free((char *) yystack.l_mark[-1].sval);
		}
break;
case 24:
#line 343 "ftp.y"
	{
			reply(225, "ABOR command successful.");
		}
break;
case 25:
#line 347 "ftp.y"
	{
			if (yystack.l_mark[-1].ival)
				cwd(pw->pw_dir);
		}
break;
case 26:
#line 352 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval != 0)
				cwd((char *) yystack.l_mark[-1].sval);
			if (yystack.l_mark[-1].sval != 0)
				free((char *) yystack.l_mark[-1].sval);
		}
break;
case 27:
#line 359 "ftp.y"
	{
			help(cmdtab, (char *) 0);
		}
break;
case 28:
#line 363 "ftp.y"
	{
			register char *cp = (char *)yystack.l_mark[-1].sval;

			if (strncasecmp(cp, "SITE", 4) == 0) {
				cp = (char *)yystack.l_mark[-1].sval + 4;
				if (*cp == ' ')
					cp++;
				if (*cp)
					help(sitetab, cp);
				else
					help(sitetab, (char *) 0);
			} else
				help(cmdtab, (char *) yystack.l_mark[-1].sval);
		}
break;
case 29:
#line 378 "ftp.y"
	{
			reply(200, "NOOP command successful.");
		}
break;
case 30:
#line 382 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval != 0)
				makedir((char *) yystack.l_mark[-1].sval);
			if (yystack.l_mark[-1].sval != 0)
				free((char *) yystack.l_mark[-1].sval);
		}
break;
case 31:
#line 389 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval != 0)
				removedir((char *) yystack.l_mark[-1].sval);
			if (yystack.l_mark[-1].sval != 0)
				free((char *) yystack.l_mark[-1].sval);
		}
break;
case 32:
#line 396 "ftp.y"
	{
			if (yystack.l_mark[-1].ival)
				pwd();
		}
break;
case 33:
#line 401 "ftp.y"
	{
			if (yystack.l_mark[-1].ival)
				cwd("..");
		}
break;
case 34:
#line 406 "ftp.y"
	{
			help(sitetab, (char *) 0);
		}
break;
case 35:
#line 410 "ftp.y"
	{
			help(sitetab, (char *) yystack.l_mark[-1].sval);
		}
break;
case 36:
#line 414 "ftp.y"
	{
			int oldmask;

			if (yystack.l_mark[-1].ival) {
				oldmask = umask(0);
				(void) umask(oldmask);
				reply(200, "Current UMASK is %03o", oldmask);
			}
		}
break;
case 37:
#line 424 "ftp.y"
	{
			int oldmask;

			if (yystack.l_mark[-3].ival) {
				if ((yystack.l_mark[-1].ival == -1) || (yystack.l_mark[-1].ival > 0777)) {
					reply(501, "Bad UMASK value");
				} else {
					oldmask = umask(yystack.l_mark[-1].ival);
					reply(200,
					    "UMASK set to %03o (was %03o)",
					    yystack.l_mark[-1].ival, oldmask);
				}
			}
		}
break;
case 38:
#line 439 "ftp.y"
	{
			if (yystack.l_mark[-5].ival && (yystack.l_mark[-1].sval != 0)) {
				if (yystack.l_mark[-3].ival > 0777)
					reply(501,
				"CHMOD: Mode value must be between 0 and 0777");
				else if (chmod((char *) yystack.l_mark[-1].sval, yystack.l_mark[-3].ival) < 0)
					perror_reply(550, (char *) yystack.l_mark[-1].sval);
				else
					reply(200, "CHMOD command successful.");
			}
			if (yystack.l_mark[-1].sval != 0)
				free((char *) yystack.l_mark[-1].sval);
		}
break;
case 39:
#line 453 "ftp.y"
	{
			reply(200,
			    "Current IDLE time limit is %d seconds; max %d",
				timeout, maxtimeout);
		}
break;
case 40:
#line 459 "ftp.y"
	{
			if (yystack.l_mark[-1].ival < 30 || yystack.l_mark[-1].ival > maxtimeout) {
				reply(501,
			"Maximum IDLE time must be between 30 and %d seconds",
				    maxtimeout);
			} else {
				timeout = yystack.l_mark[-1].ival;
				(void) alarm((unsigned) timeout);
				reply(200,
				    "Maximum IDLE time set to %d seconds",
				    timeout);
			}
		}
break;
case 41:
#line 473 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval != 0)
				store((char *) yystack.l_mark[-1].sval, "w", 1);
			if (yystack.l_mark[-1].sval != 0)
				free((char *) yystack.l_mark[-1].sval);
		}
break;
case 42:
#line 480 "ftp.y"
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
#line 501 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval != 0)
				sizecmd((char *) yystack.l_mark[-1].sval);
			if (yystack.l_mark[-1].sval != 0)
				free((char *) yystack.l_mark[-1].sval);
		}
break;
case 44:
#line 518 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval != 0) {
				struct stat stbuf;
				if (stat((char *) yystack.l_mark[-1].sval, &stbuf) < 0)
					perror_reply(550, "%s", (char *) yystack.l_mark[-1].sval);
				else if ((stbuf.st_mode&S_IFMT) != S_IFREG) {
					reply(550, "%s: not a plain file.",
						(char *) yystack.l_mark[-1].sval);
				} else {
					register struct tm *t;
					t = gmtime(&stbuf.st_mtime);
					reply(213,
					    "%04d%02d%02d%02d%02d%02d",
					    1900 + t->tm_year,
					    t->tm_mon+1, t->tm_mday,
					    t->tm_hour, t->tm_min, t->tm_sec);
				}
			}
			if (yystack.l_mark[-1].sval != 0)
				free((char *) yystack.l_mark[-1].sval);
		}
break;
case 45:
#line 540 "ftp.y"
	{
			reply(221, "Goodbye.");
			dologout(0);
		}
break;
case 46:
#line 545 "ftp.y"
	{
			yyerrok;
		}
break;
case 47:
#line 550 "ftp.y"
	{
			if (yystack.l_mark[-3].ival && yystack.l_mark[-1].sval) {
				fromname = renamefrom((char *) yystack.l_mark[-1].sval);
				if (fromname == (char *) 0 && yystack.l_mark[-1].sval) {
					free((char *) yystack.l_mark[-1].sval);
				}
			}
		}
break;
case 49:
#line 564 "ftp.y"
	{
			*(const char **)(&(yyval.sval)) = "";
		}
break;
case 52:
#line 575 "ftp.y"
	{
			register char *a, *p;

			a = (char *)&data_dest.sin_addr;
			a[0] = (char) yystack.l_mark[-10].ival;
			a[1] = (char) yystack.l_mark[-8].ival;
			a[2] = (char) yystack.l_mark[-6].ival;
			a[3] = (char) yystack.l_mark[-4].ival;
			p = (char *)&data_dest.sin_port;
			p[0] = (char) yystack.l_mark[-2].ival;
			p[1] = (char) yystack.l_mark[0].ival;
			data_dest.sin_family = AF_INET;
		}
break;
case 53:
#line 591 "ftp.y"
	{
		yyval.ival = FORM_N;
	}
break;
case 54:
#line 595 "ftp.y"
	{
		yyval.ival = FORM_T;
	}
break;
case 55:
#line 599 "ftp.y"
	{
		yyval.ival = FORM_C;
	}
break;
case 56:
#line 605 "ftp.y"
	{
		cmd_type = TYPE_A;
		cmd_form = FORM_N;
	}
break;
case 57:
#line 610 "ftp.y"
	{
		cmd_type = TYPE_A;
		cmd_form = yystack.l_mark[0].ival;
	}
break;
case 58:
#line 615 "ftp.y"
	{
		cmd_type = TYPE_E;
		cmd_form = FORM_N;
	}
break;
case 59:
#line 620 "ftp.y"
	{
		cmd_type = TYPE_E;
		cmd_form = yystack.l_mark[0].ival;
	}
break;
case 60:
#line 625 "ftp.y"
	{
		cmd_type = TYPE_I;
	}
break;
case 61:
#line 629 "ftp.y"
	{
		cmd_type = TYPE_L;
		cmd_bytesz = NBBY;
	}
break;
case 62:
#line 634 "ftp.y"
	{
		cmd_type = TYPE_L;
		cmd_bytesz = yystack.l_mark[0].ival;
	}
break;
case 63:
#line 640 "ftp.y"
	{
		cmd_type = TYPE_L;
		cmd_bytesz = yystack.l_mark[0].ival;
	}
break;
case 64:
#line 647 "ftp.y"
	{
		yyval.ival = STRU_F;
	}
break;
case 65:
#line 651 "ftp.y"
	{
		yyval.ival = STRU_R;
	}
break;
case 66:
#line 655 "ftp.y"
	{
		yyval.ival = STRU_P;
	}
break;
case 67:
#line 661 "ftp.y"
	{
		yyval.ival = MODE_S;
	}
break;
case 68:
#line 665 "ftp.y"
	{
		yyval.ival = MODE_B;
	}
break;
case 69:
#line 669 "ftp.y"
	{
		yyval.ival = MODE_C;
	}
break;
case 70:
#line 675 "ftp.y"
	{
		/*
		 * Problem: this production is used for all pathname
		 * processing, but only gives a 550 error reply.
		 * This is a valid reply in some cases but not in others.
		 */
		if (logged_in && yystack.l_mark[0].sval && strncmp((char *) yystack.l_mark[0].sval, "~", 1) == 0) {
			*(char **)&(yyval.sval) = *glob((char *) yystack.l_mark[0].sval);
			if (globerr != 0) {
				reply(550, globerr);
				yyval.sval = 0;
			}
			free((char *) yystack.l_mark[0].sval);
		} else
			yyval.sval = yystack.l_mark[0].sval;
	}
break;
case 72:
#line 697 "ftp.y"
	{
		register int ret, dec, multby, digit;

		/*
		 * Convert a number that was read as decimal number
		 * to what it would be if it had been read as octal.
		 */
		dec = yystack.l_mark[0].ival;
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
		yyval.ival = ret;
	}
break;
case 73:
#line 722 "ftp.y"
	{
		if (logged_in)
			yyval.ival = 1;
		else {
			reply(530, "Please login with USER and PASS.");
			yyval.ival = 0;
		}
	}
break;
#line 1946 "ftp.tab.c"
    }
    yystack.s_mark -= yym;
    yystate = *yystack.s_mark;
    yystack.l_mark -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yystack.s_mark = YYFINAL;
        *++yystack.l_mark = yyval;
        if (yychar < 0)
        {
            if ((yychar = YYLEX) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = yyname[YYTRANSLATE(yychar)];
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
to state %d\n", YYPREFIX, *yystack.s_mark, yystate);
#endif
    if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack))
    {
        goto yyoverflow;
    }
    *++yystack.s_mark = (short) yystate;
    *++yystack.l_mark = yyval;
    goto yyloop;

yyoverflow:
    yyerror("yacc stack overflow");

yyabort:
    yyfreestack(&yystack);
    return (1);

yyaccept:
    yyfreestack(&yystack);
    return (0);
}
