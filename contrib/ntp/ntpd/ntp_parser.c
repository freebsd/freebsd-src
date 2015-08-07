/* A Bison parser, made by GNU Bison 2.7.12-4996.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.7.12-4996"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
/* Line 371 of yacc.c  */
#line 11 "../../ntpd/ntp_parser.y"

  #ifdef HAVE_CONFIG_H
  # include <config.h>
  #endif

  #include "ntp.h"
  #include "ntpd.h"
  #include "ntp_machine.h"
  #include "ntp_stdlib.h"
  #include "ntp_filegen.h"
  #include "ntp_scanner.h"
  #include "ntp_config.h"
  #include "ntp_crypto.h"

  #include "ntpsim.h"		/* HMS: Do we really want this all the time? */
				/* SK: It might be a good idea to always
				   include the simulator code. That way
				   someone can use the same configuration file
				   for both the simulator and the daemon
				*/

  #define YYMALLOC	emalloc
  #define YYFREE	free
  #define YYERROR_VERBOSE
  #define YYMAXDEPTH	1000	/* stop the madness sooner */
  void yyerror(const char *msg);

  #ifdef SIM
  #  define ONLY_SIM(a)	(a)
  #else
  #  define ONLY_SIM(a)	NULL
  #endif

/* Line 371 of yacc.c  */
#line 102 "ntp_parser.c"

# ifndef YY_NULL
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULL nullptr
#  else
#   define YY_NULL 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
#ifndef YY_YY_NTP_PARSER_H_INCLUDED
# define YY_YY_NTP_PARSER_H_INCLUDED
/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     T_Abbrev = 258,
     T_Age = 259,
     T_All = 260,
     T_Allan = 261,
     T_Allpeers = 262,
     T_Auth = 263,
     T_Autokey = 264,
     T_Automax = 265,
     T_Average = 266,
     T_Bclient = 267,
     T_Beacon = 268,
     T_Broadcast = 269,
     T_Broadcastclient = 270,
     T_Broadcastdelay = 271,
     T_Burst = 272,
     T_Calibrate = 273,
     T_Ceiling = 274,
     T_Clockstats = 275,
     T_Cohort = 276,
     T_ControlKey = 277,
     T_Crypto = 278,
     T_Cryptostats = 279,
     T_Ctl = 280,
     T_Day = 281,
     T_Default = 282,
     T_Digest = 283,
     T_Disable = 284,
     T_Discard = 285,
     T_Dispersion = 286,
     T_Double = 287,
     T_Driftfile = 288,
     T_Drop = 289,
     T_Dscp = 290,
     T_Ellipsis = 291,
     T_Enable = 292,
     T_End = 293,
     T_False = 294,
     T_File = 295,
     T_Filegen = 296,
     T_Filenum = 297,
     T_Flag1 = 298,
     T_Flag2 = 299,
     T_Flag3 = 300,
     T_Flag4 = 301,
     T_Flake = 302,
     T_Floor = 303,
     T_Freq = 304,
     T_Fudge = 305,
     T_Host = 306,
     T_Huffpuff = 307,
     T_Iburst = 308,
     T_Ident = 309,
     T_Ignore = 310,
     T_Incalloc = 311,
     T_Incmem = 312,
     T_Initalloc = 313,
     T_Initmem = 314,
     T_Includefile = 315,
     T_Integer = 316,
     T_Interface = 317,
     T_Intrange = 318,
     T_Io = 319,
     T_Ipv4 = 320,
     T_Ipv4_flag = 321,
     T_Ipv6 = 322,
     T_Ipv6_flag = 323,
     T_Kernel = 324,
     T_Key = 325,
     T_Keys = 326,
     T_Keysdir = 327,
     T_Kod = 328,
     T_Mssntp = 329,
     T_Leapfile = 330,
     T_Leapsmearinterval = 331,
     T_Limited = 332,
     T_Link = 333,
     T_Listen = 334,
     T_Logconfig = 335,
     T_Logfile = 336,
     T_Loopstats = 337,
     T_Lowpriotrap = 338,
     T_Manycastclient = 339,
     T_Manycastserver = 340,
     T_Mask = 341,
     T_Maxage = 342,
     T_Maxclock = 343,
     T_Maxdepth = 344,
     T_Maxdist = 345,
     T_Maxmem = 346,
     T_Maxpoll = 347,
     T_Mdnstries = 348,
     T_Mem = 349,
     T_Memlock = 350,
     T_Minclock = 351,
     T_Mindepth = 352,
     T_Mindist = 353,
     T_Minimum = 354,
     T_Minpoll = 355,
     T_Minsane = 356,
     T_Mode = 357,
     T_Mode7 = 358,
     T_Monitor = 359,
     T_Month = 360,
     T_Mru = 361,
     T_Multicastclient = 362,
     T_Nic = 363,
     T_Nolink = 364,
     T_Nomodify = 365,
     T_Nomrulist = 366,
     T_None = 367,
     T_Nonvolatile = 368,
     T_Nopeer = 369,
     T_Noquery = 370,
     T_Noselect = 371,
     T_Noserve = 372,
     T_Notrap = 373,
     T_Notrust = 374,
     T_Ntp = 375,
     T_Ntpport = 376,
     T_NtpSignDsocket = 377,
     T_Orphan = 378,
     T_Orphanwait = 379,
     T_Panic = 380,
     T_Peer = 381,
     T_Peerstats = 382,
     T_Phone = 383,
     T_Pid = 384,
     T_Pidfile = 385,
     T_Pool = 386,
     T_Port = 387,
     T_Preempt = 388,
     T_Prefer = 389,
     T_Protostats = 390,
     T_Pw = 391,
     T_Randfile = 392,
     T_Rawstats = 393,
     T_Refid = 394,
     T_Requestkey = 395,
     T_Reset = 396,
     T_Restrict = 397,
     T_Revoke = 398,
     T_Rlimit = 399,
     T_Saveconfigdir = 400,
     T_Server = 401,
     T_Setvar = 402,
     T_Source = 403,
     T_Stacksize = 404,
     T_Statistics = 405,
     T_Stats = 406,
     T_Statsdir = 407,
     T_Step = 408,
     T_Stepback = 409,
     T_Stepfwd = 410,
     T_Stepout = 411,
     T_Stratum = 412,
     T_String = 413,
     T_Sys = 414,
     T_Sysstats = 415,
     T_Tick = 416,
     T_Time1 = 417,
     T_Time2 = 418,
     T_Timer = 419,
     T_Timingstats = 420,
     T_Tinker = 421,
     T_Tos = 422,
     T_Trap = 423,
     T_True = 424,
     T_Trustedkey = 425,
     T_Ttl = 426,
     T_Type = 427,
     T_U_int = 428,
     T_Unconfig = 429,
     T_Unpeer = 430,
     T_Version = 431,
     T_WanderThreshold = 432,
     T_Week = 433,
     T_Wildcard = 434,
     T_Xleave = 435,
     T_Year = 436,
     T_Flag = 437,
     T_EOC = 438,
     T_Simulate = 439,
     T_Beep_Delay = 440,
     T_Sim_Duration = 441,
     T_Server_Offset = 442,
     T_Duration = 443,
     T_Freq_Offset = 444,
     T_Wander = 445,
     T_Jitter = 446,
     T_Prop_Delay = 447,
     T_Proc_Delay = 448
   };
#endif
/* Tokens.  */
#define T_Abbrev 258
#define T_Age 259
#define T_All 260
#define T_Allan 261
#define T_Allpeers 262
#define T_Auth 263
#define T_Autokey 264
#define T_Automax 265
#define T_Average 266
#define T_Bclient 267
#define T_Beacon 268
#define T_Broadcast 269
#define T_Broadcastclient 270
#define T_Broadcastdelay 271
#define T_Burst 272
#define T_Calibrate 273
#define T_Ceiling 274
#define T_Clockstats 275
#define T_Cohort 276
#define T_ControlKey 277
#define T_Crypto 278
#define T_Cryptostats 279
#define T_Ctl 280
#define T_Day 281
#define T_Default 282
#define T_Digest 283
#define T_Disable 284
#define T_Discard 285
#define T_Dispersion 286
#define T_Double 287
#define T_Driftfile 288
#define T_Drop 289
#define T_Dscp 290
#define T_Ellipsis 291
#define T_Enable 292
#define T_End 293
#define T_False 294
#define T_File 295
#define T_Filegen 296
#define T_Filenum 297
#define T_Flag1 298
#define T_Flag2 299
#define T_Flag3 300
#define T_Flag4 301
#define T_Flake 302
#define T_Floor 303
#define T_Freq 304
#define T_Fudge 305
#define T_Host 306
#define T_Huffpuff 307
#define T_Iburst 308
#define T_Ident 309
#define T_Ignore 310
#define T_Incalloc 311
#define T_Incmem 312
#define T_Initalloc 313
#define T_Initmem 314
#define T_Includefile 315
#define T_Integer 316
#define T_Interface 317
#define T_Intrange 318
#define T_Io 319
#define T_Ipv4 320
#define T_Ipv4_flag 321
#define T_Ipv6 322
#define T_Ipv6_flag 323
#define T_Kernel 324
#define T_Key 325
#define T_Keys 326
#define T_Keysdir 327
#define T_Kod 328
#define T_Mssntp 329
#define T_Leapfile 330
#define T_Leapsmearinterval 331
#define T_Limited 332
#define T_Link 333
#define T_Listen 334
#define T_Logconfig 335
#define T_Logfile 336
#define T_Loopstats 337
#define T_Lowpriotrap 338
#define T_Manycastclient 339
#define T_Manycastserver 340
#define T_Mask 341
#define T_Maxage 342
#define T_Maxclock 343
#define T_Maxdepth 344
#define T_Maxdist 345
#define T_Maxmem 346
#define T_Maxpoll 347
#define T_Mdnstries 348
#define T_Mem 349
#define T_Memlock 350
#define T_Minclock 351
#define T_Mindepth 352
#define T_Mindist 353
#define T_Minimum 354
#define T_Minpoll 355
#define T_Minsane 356
#define T_Mode 357
#define T_Mode7 358
#define T_Monitor 359
#define T_Month 360
#define T_Mru 361
#define T_Multicastclient 362
#define T_Nic 363
#define T_Nolink 364
#define T_Nomodify 365
#define T_Nomrulist 366
#define T_None 367
#define T_Nonvolatile 368
#define T_Nopeer 369
#define T_Noquery 370
#define T_Noselect 371
#define T_Noserve 372
#define T_Notrap 373
#define T_Notrust 374
#define T_Ntp 375
#define T_Ntpport 376
#define T_NtpSignDsocket 377
#define T_Orphan 378
#define T_Orphanwait 379
#define T_Panic 380
#define T_Peer 381
#define T_Peerstats 382
#define T_Phone 383
#define T_Pid 384
#define T_Pidfile 385
#define T_Pool 386
#define T_Port 387
#define T_Preempt 388
#define T_Prefer 389
#define T_Protostats 390
#define T_Pw 391
#define T_Randfile 392
#define T_Rawstats 393
#define T_Refid 394
#define T_Requestkey 395
#define T_Reset 396
#define T_Restrict 397
#define T_Revoke 398
#define T_Rlimit 399
#define T_Saveconfigdir 400
#define T_Server 401
#define T_Setvar 402
#define T_Source 403
#define T_Stacksize 404
#define T_Statistics 405
#define T_Stats 406
#define T_Statsdir 407
#define T_Step 408
#define T_Stepback 409
#define T_Stepfwd 410
#define T_Stepout 411
#define T_Stratum 412
#define T_String 413
#define T_Sys 414
#define T_Sysstats 415
#define T_Tick 416
#define T_Time1 417
#define T_Time2 418
#define T_Timer 419
#define T_Timingstats 420
#define T_Tinker 421
#define T_Tos 422
#define T_Trap 423
#define T_True 424
#define T_Trustedkey 425
#define T_Ttl 426
#define T_Type 427
#define T_U_int 428
#define T_Unconfig 429
#define T_Unpeer 430
#define T_Version 431
#define T_WanderThreshold 432
#define T_Week 433
#define T_Wildcard 434
#define T_Xleave 435
#define T_Year 436
#define T_Flag 437
#define T_EOC 438
#define T_Simulate 439
#define T_Beep_Delay 440
#define T_Sim_Duration 441
#define T_Server_Offset 442
#define T_Duration 443
#define T_Freq_Offset 444
#define T_Wander 445
#define T_Jitter 446
#define T_Prop_Delay 447
#define T_Proc_Delay 448



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{
/* Line 387 of yacc.c  */
#line 51 "../../ntpd/ntp_parser.y"

	char *			String;
	double			Double;
	int			Integer;
	unsigned		U_int;
	gen_fifo *		Generic_fifo;
	attr_val *		Attr_val;
	attr_val_fifo *		Attr_val_fifo;
	int_fifo *		Int_fifo;
	string_fifo *		String_fifo;
	address_node *		Address_node;
	address_fifo *		Address_fifo;
	setvar_node *		Set_var;
	server_info *		Sim_server;
	server_info_fifo *	Sim_server_fifo;
	script_info *		Sim_script;
	script_info_fifo *	Sim_script_fifo;


/* Line 387 of yacc.c  */
#line 551 "ntp_parser.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */

#endif /* !YY_YY_NTP_PARSER_H_INCLUDED  */

/* Copy the second part of user declarations.  */

/* Line 390 of yacc.c  */
#line 579 "ntp_parser.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef __attribute__
/* This feature is available in gcc versions 2.5 and later.  */
# if (! defined __GNUC__ || __GNUC__ < 2 \
      || (__GNUC__ == 2 && __GNUC_MINOR__ < 5))
#  define __attribute__(Spec) /* empty */
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif


/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(N) (N)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  210
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   647

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  199
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  105
/* YYNRULES -- Number of rules.  */
#define YYNRULES  313
/* YYNRULES -- Number of states.  */
#define YYNSTATES  419

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   448

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     195,   196,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   194,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   197,     2,   198,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     5,     9,    12,    15,    16,    18,    20,
      22,    24,    26,    28,    30,    32,    34,    36,    38,    40,
      42,    46,    48,    50,    52,    54,    56,    58,    61,    63,
      65,    67,    68,    71,    73,    75,    77,    79,    81,    83,
      85,    87,    89,    91,    93,    95,    98,   101,   103,   105,
     107,   109,   111,   113,   116,   118,   121,   123,   125,   127,
     130,   133,   136,   139,   142,   145,   148,   151,   154,   157,
     160,   163,   164,   167,   170,   173,   175,   177,   179,   181,
     183,   186,   189,   191,   194,   197,   200,   202,   204,   206,
     208,   210,   212,   214,   216,   218,   220,   223,   226,   230,
     233,   235,   237,   239,   241,   243,   245,   247,   249,   251,
     252,   255,   258,   261,   263,   265,   267,   269,   271,   273,
     275,   277,   279,   281,   283,   285,   287,   290,   293,   297,
     303,   307,   312,   317,   321,   322,   325,   327,   329,   331,
     333,   335,   337,   339,   341,   343,   345,   347,   349,   351,
     353,   355,   358,   360,   363,   365,   367,   369,   372,   374,
     377,   379,   381,   383,   385,   387,   389,   391,   393,   397,
     400,   402,   405,   408,   411,   414,   417,   419,   421,   423,
     425,   427,   429,   432,   435,   437,   440,   442,   444,   446,
     449,   452,   455,   457,   459,   461,   463,   465,   467,   469,
     471,   473,   475,   477,   480,   483,   485,   488,   490,   492,
     494,   496,   498,   500,   502,   504,   506,   508,   510,   512,
     515,   518,   521,   524,   528,   530,   533,   536,   539,   542,
     546,   549,   551,   553,   555,   557,   559,   561,   563,   565,
     567,   569,   571,   574,   575,   580,   582,   583,   584,   587,
     590,   593,   596,   598,   600,   604,   608,   610,   612,   614,
     616,   618,   620,   622,   624,   626,   629,   632,   634,   636,
     638,   640,   642,   644,   646,   648,   651,   653,   656,   658,
     660,   662,   668,   671,   673,   676,   678,   680,   682,   684,
     686,   688,   694,   696,   700,   703,   707,   709,   711,   714,
     716,   722,   727,   731,   734,   736,   743,   747,   750,   754,
     756,   758,   760,   762
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     200,     0,    -1,   201,    -1,   201,   202,   183,    -1,   202,
     183,    -1,     1,   183,    -1,    -1,   203,    -1,   216,    -1,
     218,    -1,   219,    -1,   228,    -1,   236,    -1,   223,    -1,
     245,    -1,   250,    -1,   254,    -1,   259,    -1,   263,    -1,
     290,    -1,   204,   205,   208,    -1,   146,    -1,   131,    -1,
     126,    -1,    14,    -1,    84,    -1,   206,    -1,   207,   158,
      -1,   158,    -1,    66,    -1,    68,    -1,    -1,   208,   209,
      -1,   210,    -1,   212,    -1,   214,    -1,   211,    -1,     9,
      -1,    17,    -1,    53,    -1,   116,    -1,   133,    -1,   134,
      -1,   169,    -1,   180,    -1,   213,    61,    -1,   213,   173,
      -1,    70,    -1,   100,    -1,    92,    -1,   171,    -1,   102,
      -1,   176,    -1,   215,   158,    -1,    54,    -1,   217,   205,
      -1,   174,    -1,   175,    -1,    15,    -1,    85,   287,    -1,
     107,   287,    -1,    93,    61,    -1,    10,    61,    -1,    22,
      61,    -1,    23,   220,    -1,    71,   158,    -1,    72,   158,
      -1,   140,    61,    -1,   143,    61,    -1,   170,   283,    -1,
     122,   158,    -1,    -1,   220,   221,    -1,   222,   158,    -1,
     143,    61,    -1,    51,    -1,    54,    -1,   136,    -1,   137,
      -1,    28,    -1,   167,   224,    -1,   224,   225,    -1,   225,
      -1,   226,    61,    -1,   227,   289,    -1,    21,   288,    -1,
      19,    -1,    48,    -1,   123,    -1,   124,    -1,   101,    -1,
      13,    -1,    98,    -1,    90,    -1,    96,    -1,    88,    -1,
     150,   229,    -1,   152,   158,    -1,    41,   230,   231,    -1,
     229,   230,    -1,   230,    -1,    20,    -1,    24,    -1,    82,
      -1,   127,    -1,   138,    -1,   160,    -1,   165,    -1,   135,
      -1,    -1,   231,   232,    -1,    40,   158,    -1,   172,   235,
      -1,   233,    -1,   234,    -1,    78,    -1,   109,    -1,    37,
      -1,    29,    -1,   112,    -1,   129,    -1,    26,    -1,   178,
      -1,   105,    -1,   181,    -1,     4,    -1,    30,   239,    -1,
     106,   242,    -1,   142,   205,   237,    -1,   142,   206,    86,
     206,   237,    -1,   142,    27,   237,    -1,   142,    66,    27,
     237,    -1,   142,    68,    27,   237,    -1,   142,   148,   237,
      -1,    -1,   237,   238,    -1,    47,    -1,    55,    -1,    73,
      -1,    74,    -1,    77,    -1,    83,    -1,   110,    -1,   111,
      -1,   114,    -1,   115,    -1,   117,    -1,   118,    -1,   119,
      -1,   121,    -1,   176,    -1,   239,   240,    -1,   240,    -1,
     241,    61,    -1,    11,    -1,    99,    -1,   104,    -1,   242,
     243,    -1,   243,    -1,   244,    61,    -1,    56,    -1,    57,
      -1,    58,    -1,    59,    -1,    87,    -1,    89,    -1,    91,
      -1,    97,    -1,    50,   205,   246,    -1,   246,   247,    -1,
     247,    -1,   248,   289,    -1,   249,   288,    -1,   157,    61,
      -1,     3,   158,    -1,   139,   158,    -1,   162,    -1,   163,
      -1,    43,    -1,    44,    -1,    45,    -1,    46,    -1,   144,
     251,    -1,   251,   252,    -1,   252,    -1,   253,    61,    -1,
      95,    -1,   149,    -1,    42,    -1,    37,   255,    -1,    29,
     255,    -1,   255,   256,    -1,   256,    -1,   257,    -1,   258,
      -1,     8,    -1,    12,    -1,    18,    -1,    69,    -1,   104,
      -1,   120,    -1,   103,    -1,   151,    -1,   166,   260,    -1,
     260,   261,    -1,   261,    -1,   262,   289,    -1,     6,    -1,
      31,    -1,    49,    -1,    52,    -1,   125,    -1,   153,    -1,
     154,    -1,   155,    -1,   156,    -1,   161,    -1,   275,    -1,
     279,    -1,   264,   289,    -1,   265,    61,    -1,   266,   158,
      -1,   267,   158,    -1,    60,   158,   202,    -1,    38,    -1,
      33,   268,    -1,    80,   273,    -1,   128,   286,    -1,   147,
     269,    -1,   168,   206,   271,    -1,   171,   282,    -1,    16,
      -1,   113,    -1,   161,    -1,    35,    -1,    76,    -1,    54,
      -1,    75,    -1,   130,    -1,    81,    -1,   145,    -1,   158,
      -1,   158,    32,    -1,    -1,   158,   194,   158,   270,    -1,
      27,    -1,    -1,    -1,   271,   272,    -1,   132,    61,    -1,
      62,   206,    -1,   273,   274,    -1,   274,    -1,   158,    -1,
     276,   278,   277,    -1,   276,   278,   158,    -1,    62,    -1,
     108,    -1,     5,    -1,    65,    -1,    67,    -1,   179,    -1,
      79,    -1,    55,    -1,    34,    -1,   141,   280,    -1,   280,
     281,    -1,   281,    -1,     7,    -1,     8,    -1,    25,    -1,
      64,    -1,    94,    -1,   159,    -1,   164,    -1,   282,    61,
      -1,    61,    -1,   283,   284,    -1,   284,    -1,    61,    -1,
     285,    -1,   195,    61,    36,    61,   196,    -1,   286,   158,
      -1,   158,    -1,   287,   205,    -1,   205,    -1,    61,    -1,
     169,    -1,    39,    -1,    61,    -1,    32,    -1,   291,   197,
     292,   295,   198,    -1,   184,    -1,   292,   293,   183,    -1,
     293,   183,    -1,   294,   194,   289,    -1,   185,    -1,   186,
      -1,   295,   296,    -1,   296,    -1,   298,   197,   297,   299,
     198,    -1,   187,   194,   289,   183,    -1,   146,   194,   205,
      -1,   299,   300,    -1,   300,    -1,   188,   194,   289,   197,
     301,   198,    -1,   301,   302,   183,    -1,   302,   183,    -1,
     303,   194,   289,    -1,   189,    -1,   190,    -1,   191,    -1,
     192,    -1,   193,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   366,   366,   370,   371,   372,   387,   388,   389,   390,
     391,   392,   393,   394,   395,   396,   397,   398,   399,   400,
     408,   418,   419,   420,   421,   422,   426,   427,   432,   437,
     439,   445,   446,   454,   455,   456,   460,   465,   466,   467,
     468,   469,   470,   471,   472,   476,   478,   483,   484,   485,
     486,   487,   488,   492,   497,   506,   516,   517,   527,   529,
     531,   533,   544,   551,   553,   558,   560,   562,   564,   566,
     575,   581,   582,   590,   592,   604,   605,   606,   607,   608,
     617,   622,   627,   635,   637,   639,   644,   645,   646,   647,
     648,   649,   653,   654,   655,   656,   665,   667,   676,   686,
     691,   699,   700,   701,   702,   703,   704,   705,   706,   711,
     712,   720,   730,   739,   754,   759,   760,   764,   765,   769,
     770,   771,   772,   773,   774,   775,   784,   788,   792,   800,
     808,   816,   831,   846,   859,   860,   868,   869,   870,   871,
     872,   873,   874,   875,   876,   877,   878,   879,   880,   881,
     882,   886,   891,   899,   904,   905,   906,   910,   915,   923,
     928,   929,   930,   931,   932,   933,   934,   935,   943,   953,
     958,   966,   968,   970,   972,   974,   979,   980,   984,   985,
     986,   987,   995,  1000,  1005,  1013,  1018,  1019,  1020,  1029,
    1031,  1036,  1041,  1049,  1051,  1068,  1069,  1070,  1071,  1072,
    1073,  1077,  1078,  1086,  1091,  1096,  1104,  1109,  1110,  1111,
    1112,  1113,  1114,  1115,  1116,  1117,  1118,  1127,  1128,  1129,
    1136,  1143,  1150,  1166,  1185,  1187,  1189,  1191,  1193,  1195,
    1202,  1207,  1208,  1209,  1213,  1217,  1226,  1227,  1228,  1232,
    1233,  1237,  1244,  1254,  1263,  1268,  1270,  1275,  1276,  1284,
    1286,  1294,  1299,  1307,  1332,  1339,  1349,  1350,  1354,  1355,
    1356,  1357,  1361,  1362,  1363,  1367,  1372,  1377,  1385,  1386,
    1387,  1388,  1389,  1390,  1391,  1401,  1406,  1414,  1419,  1427,
    1429,  1433,  1438,  1443,  1451,  1456,  1464,  1473,  1474,  1478,
    1479,  1488,  1506,  1510,  1515,  1523,  1528,  1529,  1533,  1538,
    1546,  1551,  1556,  1561,  1566,  1574,  1579,  1584,  1592,  1597,
    1598,  1599,  1600,  1601
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "T_Abbrev", "T_Age", "T_All", "T_Allan",
  "T_Allpeers", "T_Auth", "T_Autokey", "T_Automax", "T_Average",
  "T_Bclient", "T_Beacon", "T_Broadcast", "T_Broadcastclient",
  "T_Broadcastdelay", "T_Burst", "T_Calibrate", "T_Ceiling",
  "T_Clockstats", "T_Cohort", "T_ControlKey", "T_Crypto", "T_Cryptostats",
  "T_Ctl", "T_Day", "T_Default", "T_Digest", "T_Disable", "T_Discard",
  "T_Dispersion", "T_Double", "T_Driftfile", "T_Drop", "T_Dscp",
  "T_Ellipsis", "T_Enable", "T_End", "T_False", "T_File", "T_Filegen",
  "T_Filenum", "T_Flag1", "T_Flag2", "T_Flag3", "T_Flag4", "T_Flake",
  "T_Floor", "T_Freq", "T_Fudge", "T_Host", "T_Huffpuff", "T_Iburst",
  "T_Ident", "T_Ignore", "T_Incalloc", "T_Incmem", "T_Initalloc",
  "T_Initmem", "T_Includefile", "T_Integer", "T_Interface", "T_Intrange",
  "T_Io", "T_Ipv4", "T_Ipv4_flag", "T_Ipv6", "T_Ipv6_flag", "T_Kernel",
  "T_Key", "T_Keys", "T_Keysdir", "T_Kod", "T_Mssntp", "T_Leapfile",
  "T_Leapsmearinterval", "T_Limited", "T_Link", "T_Listen", "T_Logconfig",
  "T_Logfile", "T_Loopstats", "T_Lowpriotrap", "T_Manycastclient",
  "T_Manycastserver", "T_Mask", "T_Maxage", "T_Maxclock", "T_Maxdepth",
  "T_Maxdist", "T_Maxmem", "T_Maxpoll", "T_Mdnstries", "T_Mem",
  "T_Memlock", "T_Minclock", "T_Mindepth", "T_Mindist", "T_Minimum",
  "T_Minpoll", "T_Minsane", "T_Mode", "T_Mode7", "T_Monitor", "T_Month",
  "T_Mru", "T_Multicastclient", "T_Nic", "T_Nolink", "T_Nomodify",
  "T_Nomrulist", "T_None", "T_Nonvolatile", "T_Nopeer", "T_Noquery",
  "T_Noselect", "T_Noserve", "T_Notrap", "T_Notrust", "T_Ntp", "T_Ntpport",
  "T_NtpSignDsocket", "T_Orphan", "T_Orphanwait", "T_Panic", "T_Peer",
  "T_Peerstats", "T_Phone", "T_Pid", "T_Pidfile", "T_Pool", "T_Port",
  "T_Preempt", "T_Prefer", "T_Protostats", "T_Pw", "T_Randfile",
  "T_Rawstats", "T_Refid", "T_Requestkey", "T_Reset", "T_Restrict",
  "T_Revoke", "T_Rlimit", "T_Saveconfigdir", "T_Server", "T_Setvar",
  "T_Source", "T_Stacksize", "T_Statistics", "T_Stats", "T_Statsdir",
  "T_Step", "T_Stepback", "T_Stepfwd", "T_Stepout", "T_Stratum",
  "T_String", "T_Sys", "T_Sysstats", "T_Tick", "T_Time1", "T_Time2",
  "T_Timer", "T_Timingstats", "T_Tinker", "T_Tos", "T_Trap", "T_True",
  "T_Trustedkey", "T_Ttl", "T_Type", "T_U_int", "T_Unconfig", "T_Unpeer",
  "T_Version", "T_WanderThreshold", "T_Week", "T_Wildcard", "T_Xleave",
  "T_Year", "T_Flag", "T_EOC", "T_Simulate", "T_Beep_Delay",
  "T_Sim_Duration", "T_Server_Offset", "T_Duration", "T_Freq_Offset",
  "T_Wander", "T_Jitter", "T_Prop_Delay", "T_Proc_Delay", "'='", "'('",
  "')'", "'{'", "'}'", "$accept", "configuration", "command_list",
  "command", "server_command", "client_type", "address", "ip_address",
  "address_fam", "option_list", "option", "option_flag",
  "option_flag_keyword", "option_int", "option_int_keyword", "option_str",
  "option_str_keyword", "unpeer_command", "unpeer_keyword",
  "other_mode_command", "authentication_command", "crypto_command_list",
  "crypto_command", "crypto_str_keyword", "orphan_mode_command",
  "tos_option_list", "tos_option", "tos_option_int_keyword",
  "tos_option_dbl_keyword", "monitoring_command", "stats_list", "stat",
  "filegen_option_list", "filegen_option", "link_nolink", "enable_disable",
  "filegen_type", "access_control_command", "ac_flag_list",
  "access_control_flag", "discard_option_list", "discard_option",
  "discard_option_keyword", "mru_option_list", "mru_option",
  "mru_option_keyword", "fudge_command", "fudge_factor_list",
  "fudge_factor", "fudge_factor_dbl_keyword", "fudge_factor_bool_keyword",
  "rlimit_command", "rlimit_option_list", "rlimit_option",
  "rlimit_option_keyword", "system_option_command", "system_option_list",
  "system_option", "system_option_flag_keyword",
  "system_option_local_flag_keyword", "tinker_command",
  "tinker_option_list", "tinker_option", "tinker_option_keyword",
  "miscellaneous_command", "misc_cmd_dbl_keyword", "misc_cmd_int_keyword",
  "misc_cmd_str_keyword", "misc_cmd_str_lcl_keyword", "drift_parm",
  "variable_assign", "t_default_or_zero", "trap_option_list",
  "trap_option", "log_config_list", "log_config_command",
  "interface_command", "interface_nic", "nic_rule_class",
  "nic_rule_action", "reset_command", "counter_set_list",
  "counter_set_keyword", "integer_list", "integer_list_range",
  "integer_list_range_elt", "integer_range", "string_list", "address_list",
  "boolean", "number", "simulate_command", "sim_conf_start",
  "sim_init_statement_list", "sim_init_statement", "sim_init_keyword",
  "sim_server_list", "sim_server", "sim_server_offset", "sim_server_name",
  "sim_act_list", "sim_act", "sim_act_stmt_list", "sim_act_stmt",
  "sim_act_keyword", YY_NULL
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381,   382,   383,   384,
     385,   386,   387,   388,   389,   390,   391,   392,   393,   394,
     395,   396,   397,   398,   399,   400,   401,   402,   403,   404,
     405,   406,   407,   408,   409,   410,   411,   412,   413,   414,
     415,   416,   417,   418,   419,   420,   421,   422,   423,   424,
     425,   426,   427,   428,   429,   430,   431,   432,   433,   434,
     435,   436,   437,   438,   439,   440,   441,   442,   443,   444,
     445,   446,   447,   448,    61,    40,    41,   123,   125
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   199,   200,   201,   201,   201,   202,   202,   202,   202,
     202,   202,   202,   202,   202,   202,   202,   202,   202,   202,
     203,   204,   204,   204,   204,   204,   205,   205,   206,   207,
     207,   208,   208,   209,   209,   209,   210,   211,   211,   211,
     211,   211,   211,   211,   211,   212,   212,   213,   213,   213,
     213,   213,   213,   214,   215,   216,   217,   217,   218,   218,
     218,   218,   219,   219,   219,   219,   219,   219,   219,   219,
     219,   220,   220,   221,   221,   222,   222,   222,   222,   222,
     223,   224,   224,   225,   225,   225,   226,   226,   226,   226,
     226,   226,   227,   227,   227,   227,   228,   228,   228,   229,
     229,   230,   230,   230,   230,   230,   230,   230,   230,   231,
     231,   232,   232,   232,   232,   233,   233,   234,   234,   235,
     235,   235,   235,   235,   235,   235,   236,   236,   236,   236,
     236,   236,   236,   236,   237,   237,   238,   238,   238,   238,
     238,   238,   238,   238,   238,   238,   238,   238,   238,   238,
     238,   239,   239,   240,   241,   241,   241,   242,   242,   243,
     244,   244,   244,   244,   244,   244,   244,   244,   245,   246,
     246,   247,   247,   247,   247,   247,   248,   248,   249,   249,
     249,   249,   250,   251,   251,   252,   253,   253,   253,   254,
     254,   255,   255,   256,   256,   257,   257,   257,   257,   257,
     257,   258,   258,   259,   260,   260,   261,   262,   262,   262,
     262,   262,   262,   262,   262,   262,   262,   263,   263,   263,
     263,   263,   263,   263,   263,   263,   263,   263,   263,   263,
     263,   264,   264,   264,   265,   265,   266,   266,   266,   267,
     267,   268,   268,   268,   269,   270,   270,   271,   271,   272,
     272,   273,   273,   274,   275,   275,   276,   276,   277,   277,
     277,   277,   278,   278,   278,   279,   280,   280,   281,   281,
     281,   281,   281,   281,   281,   282,   282,   283,   283,   284,
     284,   285,   286,   286,   287,   287,   288,   288,   288,   289,
     289,   290,   291,   292,   292,   293,   294,   294,   295,   295,
     296,   297,   298,   299,   299,   300,   301,   301,   302,   303,
     303,   303,   303,   303
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     3,     2,     2,     0,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       3,     1,     1,     1,     1,     1,     1,     2,     1,     1,
       1,     0,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     2,     1,     1,     1,
       1,     1,     1,     2,     1,     2,     1,     1,     1,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     0,     2,     2,     2,     1,     1,     1,     1,     1,
       2,     2,     1,     2,     2,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     2,     3,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     0,
       2,     2,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     2,     3,     5,
       3,     4,     4,     3,     0,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     1,     2,     1,     1,     1,     2,     1,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     3,     2,
       1,     2,     2,     2,     2,     2,     1,     1,     1,     1,
       1,     1,     2,     2,     1,     2,     1,     1,     1,     2,
       2,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     2,     1,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     2,     2,     3,     1,     2,     2,     2,     2,     3,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     0,     4,     1,     0,     0,     2,     2,
       2,     2,     1,     1,     3,     3,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     1,     2,     1,     1,
       1,     5,     2,     1,     2,     1,     1,     1,     1,     1,
       1,     5,     1,     3,     2,     3,     1,     1,     2,     1,
       5,     4,     3,     2,     1,     6,     3,     2,     3,     1,
       1,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       0,     0,     0,    24,    58,   231,     0,    71,     0,     0,
     243,   234,     0,   224,     0,     0,   236,     0,   256,     0,
       0,   237,   235,     0,   239,    25,     0,     0,     0,     0,
     257,   232,     0,    23,     0,   238,    22,     0,     0,     0,
       0,     0,   240,    21,     0,     0,     0,   233,     0,     0,
       0,     0,     0,    56,    57,   292,     0,     2,     0,     7,
       0,     8,     0,     9,    10,    13,    11,    12,    14,    15,
      16,    17,    18,     0,     0,     0,     0,   217,     0,   218,
      19,     0,     5,    62,    63,    64,   195,   196,   197,   198,
     201,   199,   200,   202,   190,   192,   193,   194,   154,   155,
     156,   126,   152,     0,   241,   225,   189,   101,   102,   103,
     104,   108,   105,   106,   107,   109,    29,    30,    28,     0,
      26,     0,     6,    65,    66,   253,   226,   252,   285,    59,
      61,   160,   161,   162,   163,   164,   165,   166,   167,   127,
     158,     0,    60,    70,   283,   227,    67,   268,   269,   270,
     271,   272,   273,   274,   265,   267,   134,    29,    30,   134,
     134,    26,    68,   188,   186,   187,   182,   184,     0,     0,
     228,    96,   100,    97,   207,   208,   209,   210,   211,   212,
     213,   214,   215,   216,   203,   205,     0,    91,    86,     0,
      87,    95,    93,    94,    92,    90,    88,    89,    80,    82,
       0,     0,   247,   279,     0,    69,   278,   280,   276,   230,
       1,     0,     4,    31,    55,   290,   289,   219,   220,   221,
     222,   264,   263,   262,     0,     0,    79,    75,    76,    77,
      78,     0,    72,     0,   191,   151,   153,   242,    98,     0,
     178,   179,   180,   181,     0,     0,   176,   177,   168,   170,
       0,     0,    27,   223,   251,   284,   157,   159,   282,   266,
     130,   134,   134,   133,   128,     0,   183,   185,     0,    99,
     204,   206,   288,   286,   287,    85,    81,    83,    84,   229,
       0,   277,   275,     3,    20,   258,   259,   260,   255,   261,
     254,   296,   297,     0,     0,     0,    74,    73,   118,   117,
       0,   115,   116,     0,   110,   113,   114,   174,   175,   173,
     169,   171,   172,   136,   137,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,   148,   149,   150,   135,   131,
     132,   134,   246,     0,     0,   248,     0,    37,    38,    39,
      54,    47,    49,    48,    51,    40,    41,    42,    43,    50,
      52,    44,    32,    33,    36,    34,     0,    35,     0,     0,
       0,     0,   299,     0,   294,     0,   111,   125,   121,   123,
     119,   120,   122,   124,   112,   129,   245,   244,   250,   249,
       0,    45,    46,    53,     0,   293,   291,   298,     0,   295,
     281,   302,     0,     0,     0,     0,     0,   304,     0,     0,
     300,   303,   301,     0,     0,   309,   310,   311,   312,   313,
       0,     0,     0,   305,     0,   307,     0,   306,   308
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    56,    57,    58,    59,    60,   128,   120,   121,   284,
     352,   353,   354,   355,   356,   357,   358,    61,    62,    63,
      64,    85,   232,   233,    65,   198,   199,   200,   201,    66,
     171,   115,   238,   304,   305,   306,   374,    67,   260,   328,
     101,   102,   103,   139,   140,   141,    68,   248,   249,   250,
     251,    69,   166,   167,   168,    70,    94,    95,    96,    97,
      71,   184,   185,   186,    72,    73,    74,    75,    76,   105,
     170,   377,   279,   335,   126,   127,    77,    78,   290,   224,
      79,   154,   155,   209,   205,   206,   207,   145,   129,   275,
     217,    80,    81,   293,   294,   295,   361,   362,   393,   363,
     396,   397,   410,   411,   412
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -185
static const yytype_int16 yypact[] =
{
      78,  -169,   -34,  -185,  -185,  -185,   -29,  -185,    17,    43,
    -124,  -185,    17,  -185,    -5,   -27,  -185,  -121,  -185,  -112,
    -110,  -185,  -185,  -100,  -185,  -185,   -27,     0,   116,   -27,
    -185,  -185,   -91,  -185,   -89,  -185,  -185,    11,    35,    30,
      13,    31,  -185,  -185,   -83,    -5,   -78,  -185,   186,   523,
     -76,   -56,    15,  -185,  -185,  -185,    83,   244,   -99,  -185,
     -27,  -185,   -27,  -185,  -185,  -185,  -185,  -185,  -185,  -185,
    -185,  -185,  -185,   -12,    24,   -71,   -69,  -185,   -11,  -185,
    -185,  -107,  -185,  -185,  -185,     8,  -185,  -185,  -185,  -185,
    -185,  -185,  -185,  -185,    17,  -185,  -185,  -185,  -185,  -185,
    -185,    43,  -185,    34,    59,  -185,    17,  -185,  -185,  -185,
    -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,     7,
    -185,   -61,   407,  -185,  -185,  -185,  -100,  -185,  -185,   -27,
    -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,   116,
    -185,    44,   -27,  -185,  -185,   -52,  -185,  -185,  -185,  -185,
    -185,  -185,  -185,  -185,    35,  -185,  -185,    85,    96,  -185,
    -185,    39,  -185,  -185,  -185,  -185,    31,  -185,    75,   -46,
    -185,    -5,  -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,
    -185,  -185,  -185,  -185,   186,  -185,   -12,  -185,  -185,   -35,
    -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,   523,  -185,
      82,   -12,  -185,  -185,    91,   -56,  -185,  -185,  -185,   100,
    -185,   -26,  -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,
    -185,  -185,  -185,  -185,    -2,  -130,  -185,  -185,  -185,  -185,
    -185,   105,  -185,     9,  -185,  -185,  -185,  -185,    -7,    18,
    -185,  -185,  -185,  -185,    25,   121,  -185,  -185,     7,  -185,
     -12,   -35,  -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,
     391,  -185,  -185,   391,   391,   -76,  -185,  -185,    29,  -185,
    -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,   -51,
     153,  -185,  -185,  -185,   464,  -185,  -185,  -185,  -185,  -185,
    -185,  -185,  -185,   -82,    14,     1,  -185,  -185,  -185,  -185,
      38,  -185,  -185,    12,  -185,  -185,  -185,  -185,  -185,  -185,
    -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,
    -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,   391,
     391,  -185,   171,   -76,   140,  -185,   141,  -185,  -185,  -185,
    -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,
    -185,  -185,  -185,  -185,  -185,  -185,   -55,  -185,    53,    20,
      33,  -128,  -185,    32,  -185,   -12,  -185,  -185,  -185,  -185,
    -185,  -185,  -185,  -185,  -185,   391,  -185,  -185,  -185,  -185,
      16,  -185,  -185,  -185,   -27,  -185,  -185,  -185,    46,  -185,
    -185,  -185,    37,    48,   -12,    40,  -167,  -185,    54,   -12,
    -185,  -185,  -185,    45,    79,  -185,  -185,  -185,  -185,  -185,
      98,    57,    47,  -185,    60,  -185,   -12,  -185,  -185
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -185,  -185,  -185,   -44,  -185,  -185,   -15,   -38,  -185,  -185,
    -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,  -185,
    -185,  -185,  -185,  -185,  -185,  -185,    28,  -185,  -185,  -185,
    -185,   -36,  -185,  -185,  -185,  -185,  -185,  -185,  -152,  -185,
    -185,   146,  -185,  -185,   111,  -185,  -185,  -185,     3,  -185,
    -185,  -185,  -185,    89,  -185,  -185,   245,   -66,  -185,  -185,
    -185,  -185,    72,  -185,  -185,  -185,  -185,  -185,  -185,  -185,
    -185,  -185,  -185,  -185,  -185,   137,  -185,  -185,  -185,  -185,
    -185,  -185,   110,  -185,  -185,    70,  -185,  -185,   236,    27,
    -184,  -185,  -185,  -185,   -17,  -185,  -185,   -81,  -185,  -185,
    -185,  -113,  -185,  -126,  -185
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -7
static const yytype_int16 yytable[] =
{
     119,   161,   271,   285,   272,   203,   381,   263,   264,   172,
     239,   333,   202,   211,    82,   107,   367,   278,   359,   108,
     215,   395,   298,   221,   160,    86,   273,    83,   234,    87,
     299,   400,    84,   300,   104,    88,   226,   122,   368,   116,
     234,   117,   147,   148,   222,   213,   123,   214,   124,   216,
     240,   241,   242,   243,    98,   291,   292,   156,   125,   227,
     149,   130,   228,   286,   359,   287,   311,   143,   223,   144,
     386,   301,   146,   163,   162,   169,   208,   109,   253,     1,
     173,   334,   118,   210,   212,   218,    89,   219,     2,   220,
     225,   237,     3,     4,     5,   236,   157,   252,   158,   150,
       6,     7,   302,   291,   292,   257,   258,     8,     9,   329,
     330,    10,   261,    11,   255,    12,    13,   369,   382,    14,
      90,    91,   110,   262,   370,   265,   164,   255,    15,   151,
     111,   118,    16,   112,   274,   269,   267,    92,    17,   204,
      18,   371,    99,   277,   229,   230,   244,   100,   268,    19,
      20,   231,   280,    21,    22,   113,   288,   283,    23,    24,
     114,   282,    25,    26,   245,   303,   296,   297,    93,   246,
     247,    27,   131,   132,   133,   134,   307,   289,   159,   375,
     165,   389,   309,   308,    28,    29,    30,   332,   118,   336,
     372,    31,   174,   373,   152,   365,   366,   364,   376,   153,
      32,   379,   380,   135,    33,   136,    34,   137,    35,    36,
     398,   383,   390,   138,   384,   403,   385,   175,    37,    38,
      39,    40,    41,    42,    43,    44,   276,   331,    45,   388,
      46,   394,   418,   392,   399,   176,   395,   402,   177,    47,
     415,   416,   404,   417,    48,    49,    50,   235,    51,    52,
     256,   310,    53,    54,     2,   266,   270,   106,     3,     4,
       5,    -6,    55,   254,   259,   142,     6,     7,   405,   406,
     407,   408,   409,     8,     9,   281,   360,    10,   312,    11,
     387,    12,    13,   401,   414,    14,     0,   405,   406,   407,
     408,   409,     0,     0,    15,   378,   413,     0,    16,     0,
       0,     0,     0,     0,    17,     0,    18,     0,     0,     0,
       0,   178,     0,     0,     0,    19,    20,     0,     0,    21,
      22,     0,     0,     0,    23,    24,     0,     0,    25,    26,
       0,     0,     0,     0,     0,     0,     0,    27,     0,   179,
     180,   181,   182,     0,     0,     0,     0,   183,     0,     0,
      28,    29,    30,     0,     0,     0,     0,    31,     0,     0,
       0,     0,     0,     0,     0,     0,    32,     0,     0,   391,
      33,     0,    34,     0,    35,    36,     0,     0,     0,     0,
       0,     0,     0,     0,    37,    38,    39,    40,    41,    42,
      43,    44,     0,     0,    45,     0,    46,     0,     0,     0,
       0,     0,     0,     0,     0,    47,     0,     0,     0,     0,
      48,    49,    50,     0,    51,    52,     0,     2,    53,    54,
       0,     3,     4,     5,     0,     0,     0,    -6,    55,     6,
       7,     0,     0,     0,     0,     0,     8,     9,   313,     0,
      10,     0,    11,     0,    12,    13,   314,     0,    14,     0,
       0,     0,     0,     0,     0,     0,     0,    15,     0,     0,
       0,    16,     0,     0,   315,   316,     0,    17,   317,    18,
       0,     0,     0,   337,   318,     0,     0,     0,    19,    20,
       0,   338,    21,    22,     0,     0,     0,    23,    24,     0,
       0,    25,    26,     0,     0,     0,     0,     0,     0,     0,
      27,   319,   320,     0,     0,   321,   322,     0,   323,   324,
     325,     0,   326,    28,    29,    30,     0,   339,   340,     0,
      31,     0,     0,     0,     0,     0,     0,     0,     0,    32,
       0,     0,     0,    33,   341,    34,   187,    35,    36,     0,
       0,     0,   188,     0,   189,     0,     0,    37,    38,    39,
      40,    41,    42,    43,    44,     0,   342,    45,     0,    46,
       0,     0,     0,     0,   343,     0,   344,   327,    47,     0,
       0,   190,     0,    48,    49,    50,     0,    51,    52,     0,
     345,    53,    54,     0,     0,     0,     0,     0,     0,     0,
       0,    55,     0,     0,     0,     0,     0,   346,   347,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   191,     0,   192,     0,     0,     0,     0,     0,   193,
       0,   194,     0,     0,   195,     0,     0,     0,     0,     0,
       0,     0,     0,   348,     0,   349,     0,     0,     0,     0,
     350,     0,     0,     0,   351,     0,   196,   197
};

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-185)))

#define yytable_value_is_error(Yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
      15,    39,   186,     5,    39,    61,    61,   159,   160,    45,
       3,    62,    50,    57,   183,    20,     4,   201,   146,    24,
      32,   188,    29,    34,    39,     8,    61,    61,    94,    12,
      37,   198,    61,    40,   158,    18,    28,   158,    26,    66,
     106,    68,     7,     8,    55,    60,   158,    62,   158,    61,
      43,    44,    45,    46,    11,   185,   186,    27,   158,    51,
      25,    61,    54,    65,   146,    67,   250,   158,    79,   158,
     198,    78,    61,    42,    61,   158,    61,    82,   122,     1,
     158,   132,   158,     0,   183,    61,    69,   158,    10,   158,
     197,    32,    14,    15,    16,    61,    66,   158,    68,    64,
      22,    23,   109,   185,   186,    61,   158,    29,    30,   261,
     262,    33,    27,    35,   129,    37,    38,   105,   173,    41,
     103,   104,   127,    27,   112,    86,    95,   142,    50,    94,
     135,   158,    54,   138,   169,   171,    61,   120,    60,   195,
      62,   129,    99,    61,   136,   137,   139,   104,   194,    71,
      72,   143,    61,    75,    76,   160,   158,   183,    80,    81,
     165,    61,    84,    85,   157,   172,    61,   158,   151,   162,
     163,    93,    56,    57,    58,    59,   158,   179,   148,   331,
     149,   365,    61,   158,   106,   107,   108,   158,   158,    36,
     178,   113,     6,   181,   159,   194,   158,   183,    27,   164,
     122,    61,    61,    87,   126,    89,   128,    91,   130,   131,
     394,   158,   196,    97,   194,   399,   183,    31,   140,   141,
     142,   143,   144,   145,   146,   147,   198,   265,   150,   197,
     152,   194,   416,   187,   194,    49,   188,   183,    52,   161,
     183,   194,   197,   183,   166,   167,   168,   101,   170,   171,
     139,   248,   174,   175,    10,   166,   184,    12,    14,    15,
      16,   183,   184,   126,   154,    29,    22,    23,   189,   190,
     191,   192,   193,    29,    30,   205,   293,    33,   251,    35,
     361,    37,    38,   396,   410,    41,    -1,   189,   190,   191,
     192,   193,    -1,    -1,    50,   333,   198,    -1,    54,    -1,
      -1,    -1,    -1,    -1,    60,    -1,    62,    -1,    -1,    -1,
      -1,   125,    -1,    -1,    -1,    71,    72,    -1,    -1,    75,
      76,    -1,    -1,    -1,    80,    81,    -1,    -1,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    93,    -1,   153,
     154,   155,   156,    -1,    -1,    -1,    -1,   161,    -1,    -1,
     106,   107,   108,    -1,    -1,    -1,    -1,   113,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   122,    -1,    -1,   384,
     126,    -1,   128,    -1,   130,   131,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   140,   141,   142,   143,   144,   145,
     146,   147,    -1,    -1,   150,    -1,   152,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   161,    -1,    -1,    -1,    -1,
     166,   167,   168,    -1,   170,   171,    -1,    10,   174,   175,
      -1,    14,    15,    16,    -1,    -1,    -1,   183,   184,    22,
      23,    -1,    -1,    -1,    -1,    -1,    29,    30,    47,    -1,
      33,    -1,    35,    -1,    37,    38,    55,    -1,    41,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    50,    -1,    -1,
      -1,    54,    -1,    -1,    73,    74,    -1,    60,    77,    62,
      -1,    -1,    -1,     9,    83,    -1,    -1,    -1,    71,    72,
      -1,    17,    75,    76,    -1,    -1,    -1,    80,    81,    -1,
      -1,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      93,   110,   111,    -1,    -1,   114,   115,    -1,   117,   118,
     119,    -1,   121,   106,   107,   108,    -1,    53,    54,    -1,
     113,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   122,
      -1,    -1,    -1,   126,    70,   128,    13,   130,   131,    -1,
      -1,    -1,    19,    -1,    21,    -1,    -1,   140,   141,   142,
     143,   144,   145,   146,   147,    -1,    92,   150,    -1,   152,
      -1,    -1,    -1,    -1,   100,    -1,   102,   176,   161,    -1,
      -1,    48,    -1,   166,   167,   168,    -1,   170,   171,    -1,
     116,   174,   175,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   184,    -1,    -1,    -1,    -1,    -1,   133,   134,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    88,    -1,    90,    -1,    -1,    -1,    -1,    -1,    96,
      -1,    98,    -1,    -1,   101,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   169,    -1,   171,    -1,    -1,    -1,    -1,
     176,    -1,    -1,    -1,   180,    -1,   123,   124
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,     1,    10,    14,    15,    16,    22,    23,    29,    30,
      33,    35,    37,    38,    41,    50,    54,    60,    62,    71,
      72,    75,    76,    80,    81,    84,    85,    93,   106,   107,
     108,   113,   122,   126,   128,   130,   131,   140,   141,   142,
     143,   144,   145,   146,   147,   150,   152,   161,   166,   167,
     168,   170,   171,   174,   175,   184,   200,   201,   202,   203,
     204,   216,   217,   218,   219,   223,   228,   236,   245,   250,
     254,   259,   263,   264,   265,   266,   267,   275,   276,   279,
     290,   291,   183,    61,    61,   220,     8,    12,    18,    69,
     103,   104,   120,   151,   255,   256,   257,   258,    11,    99,
     104,   239,   240,   241,   158,   268,   255,    20,    24,    82,
     127,   135,   138,   160,   165,   230,    66,    68,   158,   205,
     206,   207,   158,   158,   158,   158,   273,   274,   205,   287,
      61,    56,    57,    58,    59,    87,    89,    91,    97,   242,
     243,   244,   287,   158,   158,   286,    61,     7,     8,    25,
      64,    94,   159,   164,   280,   281,    27,    66,    68,   148,
     205,   206,    61,    42,    95,   149,   251,   252,   253,   158,
     269,   229,   230,   158,     6,    31,    49,    52,   125,   153,
     154,   155,   156,   161,   260,   261,   262,    13,    19,    21,
      48,    88,    90,    96,    98,   101,   123,   124,   224,   225,
     226,   227,   206,    61,   195,   283,   284,   285,    61,   282,
       0,   202,   183,   205,   205,    32,    61,   289,    61,   158,
     158,    34,    55,    79,   278,   197,    28,    51,    54,   136,
     137,   143,   221,   222,   256,   240,    61,    32,   231,     3,
      43,    44,    45,    46,   139,   157,   162,   163,   246,   247,
     248,   249,   158,   202,   274,   205,   243,    61,   158,   281,
     237,    27,    27,   237,   237,    86,   252,    61,   194,   230,
     261,   289,    39,    61,   169,   288,   225,    61,   289,   271,
      61,   284,    61,   183,   208,     5,    65,    67,   158,   179,
     277,   185,   186,   292,   293,   294,    61,   158,    29,    37,
      40,    78,   109,   172,   232,   233,   234,   158,   158,    61,
     247,   289,   288,    47,    55,    73,    74,    77,    83,   110,
     111,   114,   115,   117,   118,   119,   121,   176,   238,   237,
     237,   206,   158,    62,   132,   272,    36,     9,    17,    53,
      54,    70,    92,   100,   102,   116,   133,   134,   169,   171,
     176,   180,   209,   210,   211,   212,   213,   214,   215,   146,
     293,   295,   296,   298,   183,   194,   158,     4,    26,   105,
     112,   129,   178,   181,   235,   237,    27,   270,   206,    61,
      61,    61,   173,   158,   194,   183,   198,   296,   197,   289,
     196,   205,   187,   297,   194,   188,   299,   300,   289,   194,
     198,   300,   183,   289,   197,   189,   190,   191,   192,   193,
     301,   302,   303,   198,   302,   183,   194,   183,   289
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))

/* Error token number */
#define YYTERROR	1
#define YYERRCODE	256


/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */
#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULL, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULL;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULL, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YYUSE (yytype);
}




/* The lookahead symbol.  */
int yychar;


#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval YY_INITIAL_VALUE(yyval_default);

/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 5:
/* Line 1787 of yacc.c  */
#line 373 "../../ntpd/ntp_parser.y"
    {
			/* I will need to incorporate much more fine grained
			 * error messages. The following should suffice for
			 * the time being.
			 */
			struct FILE_INFO * ip_ctx = lex_current();
			msyslog(LOG_ERR,
				"syntax error in %s line %d, column %d",
				ip_ctx->fname,
				ip_ctx->errpos.nline,
				ip_ctx->errpos.ncol);
		}
    break;

  case 20:
/* Line 1787 of yacc.c  */
#line 409 "../../ntpd/ntp_parser.y"
    {
			peer_node *my_node;

			my_node = create_peer_node((yyvsp[(1) - (3)].Integer), (yyvsp[(2) - (3)].Address_node), (yyvsp[(3) - (3)].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.peers, my_node);
		}
    break;

  case 27:
/* Line 1787 of yacc.c  */
#line 428 "../../ntpd/ntp_parser.y"
    { (yyval.Address_node) = create_address_node((yyvsp[(2) - (2)].String), (yyvsp[(1) - (2)].Integer)); }
    break;

  case 28:
/* Line 1787 of yacc.c  */
#line 433 "../../ntpd/ntp_parser.y"
    { (yyval.Address_node) = create_address_node((yyvsp[(1) - (1)].String), AF_UNSPEC); }
    break;

  case 29:
/* Line 1787 of yacc.c  */
#line 438 "../../ntpd/ntp_parser.y"
    { (yyval.Integer) = AF_INET; }
    break;

  case 30:
/* Line 1787 of yacc.c  */
#line 440 "../../ntpd/ntp_parser.y"
    { (yyval.Integer) = AF_INET6; }
    break;

  case 31:
/* Line 1787 of yacc.c  */
#line 445 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val_fifo) = NULL; }
    break;

  case 32:
/* Line 1787 of yacc.c  */
#line 447 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 36:
/* Line 1787 of yacc.c  */
#line 461 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[(1) - (1)].Integer)); }
    break;

  case 45:
/* Line 1787 of yacc.c  */
#line 477 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 46:
/* Line 1787 of yacc.c  */
#line 479 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_uval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 53:
/* Line 1787 of yacc.c  */
#line 493 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_sval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].String)); }
    break;

  case 55:
/* Line 1787 of yacc.c  */
#line 507 "../../ntpd/ntp_parser.y"
    {
			unpeer_node *my_node;

			my_node = create_unpeer_node((yyvsp[(2) - (2)].Address_node));
			if (my_node)
				APPEND_G_FIFO(cfgt.unpeers, my_node);
		}
    break;

  case 58:
/* Line 1787 of yacc.c  */
#line 528 "../../ntpd/ntp_parser.y"
    { cfgt.broadcastclient = 1; }
    break;

  case 59:
/* Line 1787 of yacc.c  */
#line 530 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.manycastserver, (yyvsp[(2) - (2)].Address_fifo)); }
    break;

  case 60:
/* Line 1787 of yacc.c  */
#line 532 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.multicastclient, (yyvsp[(2) - (2)].Address_fifo)); }
    break;

  case 61:
/* Line 1787 of yacc.c  */
#line 534 "../../ntpd/ntp_parser.y"
    { cfgt.mdnstries = (yyvsp[(2) - (2)].Integer); }
    break;

  case 62:
/* Line 1787 of yacc.c  */
#line 545 "../../ntpd/ntp_parser.y"
    {
			attr_val *atrv;

			atrv = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer));
			APPEND_G_FIFO(cfgt.vars, atrv);
		}
    break;

  case 63:
/* Line 1787 of yacc.c  */
#line 552 "../../ntpd/ntp_parser.y"
    { cfgt.auth.control_key = (yyvsp[(2) - (2)].Integer); }
    break;

  case 64:
/* Line 1787 of yacc.c  */
#line 554 "../../ntpd/ntp_parser.y"
    {
			cfgt.auth.cryptosw++;
			CONCAT_G_FIFOS(cfgt.auth.crypto_cmd_list, (yyvsp[(2) - (2)].Attr_val_fifo));
		}
    break;

  case 65:
/* Line 1787 of yacc.c  */
#line 559 "../../ntpd/ntp_parser.y"
    { cfgt.auth.keys = (yyvsp[(2) - (2)].String); }
    break;

  case 66:
/* Line 1787 of yacc.c  */
#line 561 "../../ntpd/ntp_parser.y"
    { cfgt.auth.keysdir = (yyvsp[(2) - (2)].String); }
    break;

  case 67:
/* Line 1787 of yacc.c  */
#line 563 "../../ntpd/ntp_parser.y"
    { cfgt.auth.request_key = (yyvsp[(2) - (2)].Integer); }
    break;

  case 68:
/* Line 1787 of yacc.c  */
#line 565 "../../ntpd/ntp_parser.y"
    { cfgt.auth.revoke = (yyvsp[(2) - (2)].Integer); }
    break;

  case 69:
/* Line 1787 of yacc.c  */
#line 567 "../../ntpd/ntp_parser.y"
    {
			cfgt.auth.trusted_key_list = (yyvsp[(2) - (2)].Attr_val_fifo);

			// if (!cfgt.auth.trusted_key_list)
			// 	cfgt.auth.trusted_key_list = $2;
			// else
			// 	LINK_SLIST(cfgt.auth.trusted_key_list, $2, link);
		}
    break;

  case 70:
/* Line 1787 of yacc.c  */
#line 576 "../../ntpd/ntp_parser.y"
    { cfgt.auth.ntp_signd_socket = (yyvsp[(2) - (2)].String); }
    break;

  case 71:
/* Line 1787 of yacc.c  */
#line 581 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val_fifo) = NULL; }
    break;

  case 72:
/* Line 1787 of yacc.c  */
#line 583 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 73:
/* Line 1787 of yacc.c  */
#line 591 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_sval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].String)); }
    break;

  case 74:
/* Line 1787 of yacc.c  */
#line 593 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val) = NULL;
			cfgt.auth.revoke = (yyvsp[(2) - (2)].Integer);
			msyslog(LOG_WARNING,
				"'crypto revoke %d' is deprecated, "
				"please use 'revoke %d' instead.",
				cfgt.auth.revoke, cfgt.auth.revoke);
		}
    break;

  case 80:
/* Line 1787 of yacc.c  */
#line 618 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.orphan_cmds, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 81:
/* Line 1787 of yacc.c  */
#line 623 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 82:
/* Line 1787 of yacc.c  */
#line 628 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 83:
/* Line 1787 of yacc.c  */
#line 636 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (2)].Integer), (double)(yyvsp[(2) - (2)].Integer)); }
    break;

  case 84:
/* Line 1787 of yacc.c  */
#line 638 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Double)); }
    break;

  case 85:
/* Line 1787 of yacc.c  */
#line 640 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (2)].Integer), (double)(yyvsp[(2) - (2)].Integer)); }
    break;

  case 96:
/* Line 1787 of yacc.c  */
#line 666 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.stats_list, (yyvsp[(2) - (2)].Int_fifo)); }
    break;

  case 97:
/* Line 1787 of yacc.c  */
#line 668 "../../ntpd/ntp_parser.y"
    {
			if (lex_from_file()) {
				cfgt.stats_dir = (yyvsp[(2) - (2)].String);
			} else {
				YYFREE((yyvsp[(2) - (2)].String));
				yyerror("statsdir remote configuration ignored");
			}
		}
    break;

  case 98:
/* Line 1787 of yacc.c  */
#line 677 "../../ntpd/ntp_parser.y"
    {
			filegen_node *fgn;

			fgn = create_filegen_node((yyvsp[(2) - (3)].Integer), (yyvsp[(3) - (3)].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.filegen_opts, fgn);
		}
    break;

  case 99:
/* Line 1787 of yacc.c  */
#line 687 "../../ntpd/ntp_parser.y"
    {
			(yyval.Int_fifo) = (yyvsp[(1) - (2)].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[(2) - (2)].Integer)));
		}
    break;

  case 100:
/* Line 1787 of yacc.c  */
#line 692 "../../ntpd/ntp_parser.y"
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[(1) - (1)].Integer)));
		}
    break;

  case 109:
/* Line 1787 of yacc.c  */
#line 711 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val_fifo) = NULL; }
    break;

  case 110:
/* Line 1787 of yacc.c  */
#line 713 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 111:
/* Line 1787 of yacc.c  */
#line 721 "../../ntpd/ntp_parser.y"
    {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_sval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].String));
			} else {
				(yyval.Attr_val) = NULL;
				YYFREE((yyvsp[(2) - (2)].String));
				yyerror("filegen file remote config ignored");
			}
		}
    break;

  case 112:
/* Line 1787 of yacc.c  */
#line 731 "../../ntpd/ntp_parser.y"
    {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror("filegen type remote config ignored");
			}
		}
    break;

  case 113:
/* Line 1787 of yacc.c  */
#line 740 "../../ntpd/ntp_parser.y"
    {
			const char *err;

			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[(1) - (1)].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				if (T_Link == (yyvsp[(1) - (1)].Integer))
					err = "filegen link remote config ignored";
				else
					err = "filegen nolink remote config ignored";
				yyerror(err);
			}
		}
    break;

  case 114:
/* Line 1787 of yacc.c  */
#line 755 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[(1) - (1)].Integer)); }
    break;

  case 126:
/* Line 1787 of yacc.c  */
#line 785 "../../ntpd/ntp_parser.y"
    {
			CONCAT_G_FIFOS(cfgt.discard_opts, (yyvsp[(2) - (2)].Attr_val_fifo));
		}
    break;

  case 127:
/* Line 1787 of yacc.c  */
#line 789 "../../ntpd/ntp_parser.y"
    {
			CONCAT_G_FIFOS(cfgt.mru_opts, (yyvsp[(2) - (2)].Attr_val_fifo));
		}
    break;

  case 128:
/* Line 1787 of yacc.c  */
#line 793 "../../ntpd/ntp_parser.y"
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[(2) - (3)].Address_node), NULL, (yyvsp[(3) - (3)].Int_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
    break;

  case 129:
/* Line 1787 of yacc.c  */
#line 801 "../../ntpd/ntp_parser.y"
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[(2) - (5)].Address_node), (yyvsp[(4) - (5)].Address_node), (yyvsp[(5) - (5)].Int_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
    break;

  case 130:
/* Line 1787 of yacc.c  */
#line 809 "../../ntpd/ntp_parser.y"
    {
			restrict_node *rn;

			rn = create_restrict_node(NULL, NULL, (yyvsp[(3) - (3)].Int_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
    break;

  case 131:
/* Line 1787 of yacc.c  */
#line 817 "../../ntpd/ntp_parser.y"
    {
			restrict_node *rn;

			rn = create_restrict_node(
				create_address_node(
					estrdup("0.0.0.0"),
					AF_INET),
				create_address_node(
					estrdup("0.0.0.0"),
					AF_INET),
				(yyvsp[(4) - (4)].Int_fifo),
				lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
    break;

  case 132:
/* Line 1787 of yacc.c  */
#line 832 "../../ntpd/ntp_parser.y"
    {
			restrict_node *rn;

			rn = create_restrict_node(
				create_address_node(
					estrdup("::"),
					AF_INET6),
				create_address_node(
					estrdup("::"),
					AF_INET6),
				(yyvsp[(4) - (4)].Int_fifo),
				lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
    break;

  case 133:
/* Line 1787 of yacc.c  */
#line 847 "../../ntpd/ntp_parser.y"
    {
			restrict_node *	rn;

			APPEND_G_FIFO((yyvsp[(3) - (3)].Int_fifo), create_int_node((yyvsp[(2) - (3)].Integer)));
			rn = create_restrict_node(
				NULL, NULL, (yyvsp[(3) - (3)].Int_fifo), lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
    break;

  case 134:
/* Line 1787 of yacc.c  */
#line 859 "../../ntpd/ntp_parser.y"
    { (yyval.Int_fifo) = NULL; }
    break;

  case 135:
/* Line 1787 of yacc.c  */
#line 861 "../../ntpd/ntp_parser.y"
    {
			(yyval.Int_fifo) = (yyvsp[(1) - (2)].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[(2) - (2)].Integer)));
		}
    break;

  case 151:
/* Line 1787 of yacc.c  */
#line 887 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 152:
/* Line 1787 of yacc.c  */
#line 892 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 153:
/* Line 1787 of yacc.c  */
#line 900 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 157:
/* Line 1787 of yacc.c  */
#line 911 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 158:
/* Line 1787 of yacc.c  */
#line 916 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 159:
/* Line 1787 of yacc.c  */
#line 924 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 168:
/* Line 1787 of yacc.c  */
#line 944 "../../ntpd/ntp_parser.y"
    {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[(2) - (3)].Address_node), (yyvsp[(3) - (3)].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.fudge, aon);
		}
    break;

  case 169:
/* Line 1787 of yacc.c  */
#line 954 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 170:
/* Line 1787 of yacc.c  */
#line 959 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 171:
/* Line 1787 of yacc.c  */
#line 967 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Double)); }
    break;

  case 172:
/* Line 1787 of yacc.c  */
#line 969 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 173:
/* Line 1787 of yacc.c  */
#line 971 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 174:
/* Line 1787 of yacc.c  */
#line 973 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_sval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].String)); }
    break;

  case 175:
/* Line 1787 of yacc.c  */
#line 975 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_sval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].String)); }
    break;

  case 182:
/* Line 1787 of yacc.c  */
#line 996 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.rlimit, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 183:
/* Line 1787 of yacc.c  */
#line 1001 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 184:
/* Line 1787 of yacc.c  */
#line 1006 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 185:
/* Line 1787 of yacc.c  */
#line 1014 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 189:
/* Line 1787 of yacc.c  */
#line 1030 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.enable_opts, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 190:
/* Line 1787 of yacc.c  */
#line 1032 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.disable_opts, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 191:
/* Line 1787 of yacc.c  */
#line 1037 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 192:
/* Line 1787 of yacc.c  */
#line 1042 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 193:
/* Line 1787 of yacc.c  */
#line 1050 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[(1) - (1)].Integer)); }
    break;

  case 194:
/* Line 1787 of yacc.c  */
#line 1052 "../../ntpd/ntp_parser.y"
    {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[(1) - (1)].Integer));
			} else {
				char err_str[128];

				(yyval.Attr_val) = NULL;
				snprintf(err_str, sizeof(err_str),
					 "enable/disable %s remote configuration ignored",
					 keyword((yyvsp[(1) - (1)].Integer)));
				yyerror(err_str);
			}
		}
    break;

  case 203:
/* Line 1787 of yacc.c  */
#line 1087 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.tinker, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 204:
/* Line 1787 of yacc.c  */
#line 1092 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 205:
/* Line 1787 of yacc.c  */
#line 1097 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 206:
/* Line 1787 of yacc.c  */
#line 1105 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Double)); }
    break;

  case 219:
/* Line 1787 of yacc.c  */
#line 1130 "../../ntpd/ntp_parser.y"
    {
			attr_val *av;

			av = create_attr_dval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Double));
			APPEND_G_FIFO(cfgt.vars, av);
		}
    break;

  case 220:
/* Line 1787 of yacc.c  */
#line 1137 "../../ntpd/ntp_parser.y"
    {
			attr_val *av;

			av = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer));
			APPEND_G_FIFO(cfgt.vars, av);
		}
    break;

  case 221:
/* Line 1787 of yacc.c  */
#line 1144 "../../ntpd/ntp_parser.y"
    {
			attr_val *av;

			av = create_attr_sval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].String));
			APPEND_G_FIFO(cfgt.vars, av);
		}
    break;

  case 222:
/* Line 1787 of yacc.c  */
#line 1151 "../../ntpd/ntp_parser.y"
    {
			char error_text[64];
			attr_val *av;

			if (lex_from_file()) {
				av = create_attr_sval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].String));
				APPEND_G_FIFO(cfgt.vars, av);
			} else {
				YYFREE((yyvsp[(2) - (2)].String));
				snprintf(error_text, sizeof(error_text),
					 "%s remote config ignored",
					 keyword((yyvsp[(1) - (2)].Integer)));
				yyerror(error_text);
			}
		}
    break;

  case 223:
/* Line 1787 of yacc.c  */
#line 1167 "../../ntpd/ntp_parser.y"
    {
			if (!lex_from_file()) {
				YYFREE((yyvsp[(2) - (3)].String)); /* avoid leak */
				yyerror("remote includefile ignored");
				break;
			}
			if (lex_level() > MAXINCLUDELEVEL) {
				fprintf(stderr, "getconfig: Maximum include file level exceeded.\n");
				msyslog(LOG_ERR, "getconfig: Maximum include file level exceeded.");
			} else {
				const char * path = FindConfig((yyvsp[(2) - (3)].String)); /* might return $2! */
				if (!lex_push_file(path, "r")) {
					fprintf(stderr, "getconfig: Couldn't open <%s>\n", path);
					msyslog(LOG_ERR, "getconfig: Couldn't open <%s>", path);
				}
			}
			YYFREE((yyvsp[(2) - (3)].String)); /* avoid leak */
		}
    break;

  case 224:
/* Line 1787 of yacc.c  */
#line 1186 "../../ntpd/ntp_parser.y"
    { lex_flush_stack(); }
    break;

  case 225:
/* Line 1787 of yacc.c  */
#line 1188 "../../ntpd/ntp_parser.y"
    { /* see drift_parm below for actions */ }
    break;

  case 226:
/* Line 1787 of yacc.c  */
#line 1190 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.logconfig, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 227:
/* Line 1787 of yacc.c  */
#line 1192 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.phone, (yyvsp[(2) - (2)].String_fifo)); }
    break;

  case 228:
/* Line 1787 of yacc.c  */
#line 1194 "../../ntpd/ntp_parser.y"
    { APPEND_G_FIFO(cfgt.setvar, (yyvsp[(2) - (2)].Set_var)); }
    break;

  case 229:
/* Line 1787 of yacc.c  */
#line 1196 "../../ntpd/ntp_parser.y"
    {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[(2) - (3)].Address_node), (yyvsp[(3) - (3)].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.trap, aon);
		}
    break;

  case 230:
/* Line 1787 of yacc.c  */
#line 1203 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.ttl, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 235:
/* Line 1787 of yacc.c  */
#line 1218 "../../ntpd/ntp_parser.y"
    {
#ifndef LEAP_SMEAR
			yyerror("Built without LEAP_SMEAR support.");
#endif
		}
    break;

  case 241:
/* Line 1787 of yacc.c  */
#line 1238 "../../ntpd/ntp_parser.y"
    {
			attr_val *av;

			av = create_attr_sval(T_Driftfile, (yyvsp[(1) - (1)].String));
			APPEND_G_FIFO(cfgt.vars, av);
		}
    break;

  case 242:
/* Line 1787 of yacc.c  */
#line 1245 "../../ntpd/ntp_parser.y"
    {
			attr_val *av;

			av = create_attr_sval(T_Driftfile, (yyvsp[(1) - (2)].String));
			APPEND_G_FIFO(cfgt.vars, av);
			av = create_attr_dval(T_WanderThreshold, (yyvsp[(2) - (2)].Double));
			APPEND_G_FIFO(cfgt.vars, av);
		}
    break;

  case 243:
/* Line 1787 of yacc.c  */
#line 1254 "../../ntpd/ntp_parser.y"
    {
			attr_val *av;

			av = create_attr_sval(T_Driftfile, "");
			APPEND_G_FIFO(cfgt.vars, av);
		}
    break;

  case 244:
/* Line 1787 of yacc.c  */
#line 1264 "../../ntpd/ntp_parser.y"
    { (yyval.Set_var) = create_setvar_node((yyvsp[(1) - (4)].String), (yyvsp[(3) - (4)].String), (yyvsp[(4) - (4)].Integer)); }
    break;

  case 246:
/* Line 1787 of yacc.c  */
#line 1270 "../../ntpd/ntp_parser.y"
    { (yyval.Integer) = 0; }
    break;

  case 247:
/* Line 1787 of yacc.c  */
#line 1275 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val_fifo) = NULL; }
    break;

  case 248:
/* Line 1787 of yacc.c  */
#line 1277 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 249:
/* Line 1787 of yacc.c  */
#line 1285 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 250:
/* Line 1787 of yacc.c  */
#line 1287 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val) = create_attr_sval((yyvsp[(1) - (2)].Integer), estrdup((yyvsp[(2) - (2)].Address_node)->address));
			destroy_address_node((yyvsp[(2) - (2)].Address_node));
		}
    break;

  case 251:
/* Line 1787 of yacc.c  */
#line 1295 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 252:
/* Line 1787 of yacc.c  */
#line 1300 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 253:
/* Line 1787 of yacc.c  */
#line 1308 "../../ntpd/ntp_parser.y"
    {
			char	prefix;
			char *	type;

			switch ((yyvsp[(1) - (1)].String)[0]) {

			case '+':
			case '-':
			case '=':
				prefix = (yyvsp[(1) - (1)].String)[0];
				type = (yyvsp[(1) - (1)].String) + 1;
				break;

			default:
				prefix = '=';
				type = (yyvsp[(1) - (1)].String);
			}

			(yyval.Attr_val) = create_attr_sval(prefix, estrdup(type));
			YYFREE((yyvsp[(1) - (1)].String));
		}
    break;

  case 254:
/* Line 1787 of yacc.c  */
#line 1333 "../../ntpd/ntp_parser.y"
    {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node((yyvsp[(3) - (3)].Integer), NULL, (yyvsp[(2) - (3)].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
    break;

  case 255:
/* Line 1787 of yacc.c  */
#line 1340 "../../ntpd/ntp_parser.y"
    {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node(0, (yyvsp[(3) - (3)].String), (yyvsp[(2) - (3)].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
    break;

  case 265:
/* Line 1787 of yacc.c  */
#line 1368 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.reset_counters, (yyvsp[(2) - (2)].Int_fifo)); }
    break;

  case 266:
/* Line 1787 of yacc.c  */
#line 1373 "../../ntpd/ntp_parser.y"
    {
			(yyval.Int_fifo) = (yyvsp[(1) - (2)].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[(2) - (2)].Integer)));
		}
    break;

  case 267:
/* Line 1787 of yacc.c  */
#line 1378 "../../ntpd/ntp_parser.y"
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[(1) - (1)].Integer)));
		}
    break;

  case 275:
/* Line 1787 of yacc.c  */
#line 1402 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[(2) - (2)].Integer)));
		}
    break;

  case 276:
/* Line 1787 of yacc.c  */
#line 1407 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[(1) - (1)].Integer)));
		}
    break;

  case 277:
/* Line 1787 of yacc.c  */
#line 1415 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 278:
/* Line 1787 of yacc.c  */
#line 1420 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 279:
/* Line 1787 of yacc.c  */
#line 1428 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival('i', (yyvsp[(1) - (1)].Integer)); }
    break;

  case 281:
/* Line 1787 of yacc.c  */
#line 1434 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_rangeval('-', (yyvsp[(2) - (5)].Integer), (yyvsp[(4) - (5)].Integer)); }
    break;

  case 282:
/* Line 1787 of yacc.c  */
#line 1439 "../../ntpd/ntp_parser.y"
    {
			(yyval.String_fifo) = (yyvsp[(1) - (2)].String_fifo);
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[(2) - (2)].String)));
		}
    break;

  case 283:
/* Line 1787 of yacc.c  */
#line 1444 "../../ntpd/ntp_parser.y"
    {
			(yyval.String_fifo) = NULL;
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[(1) - (1)].String)));
		}
    break;

  case 284:
/* Line 1787 of yacc.c  */
#line 1452 "../../ntpd/ntp_parser.y"
    {
			(yyval.Address_fifo) = (yyvsp[(1) - (2)].Address_fifo);
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[(2) - (2)].Address_node));
		}
    break;

  case 285:
/* Line 1787 of yacc.c  */
#line 1457 "../../ntpd/ntp_parser.y"
    {
			(yyval.Address_fifo) = NULL;
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[(1) - (1)].Address_node));
		}
    break;

  case 286:
/* Line 1787 of yacc.c  */
#line 1465 "../../ntpd/ntp_parser.y"
    {
			if ((yyvsp[(1) - (1)].Integer) != 0 && (yyvsp[(1) - (1)].Integer) != 1) {
				yyerror("Integer value is not boolean (0 or 1). Assuming 1");
				(yyval.Integer) = 1;
			} else {
				(yyval.Integer) = (yyvsp[(1) - (1)].Integer);
			}
		}
    break;

  case 287:
/* Line 1787 of yacc.c  */
#line 1473 "../../ntpd/ntp_parser.y"
    { (yyval.Integer) = 1; }
    break;

  case 288:
/* Line 1787 of yacc.c  */
#line 1474 "../../ntpd/ntp_parser.y"
    { (yyval.Integer) = 0; }
    break;

  case 289:
/* Line 1787 of yacc.c  */
#line 1478 "../../ntpd/ntp_parser.y"
    { (yyval.Double) = (double)(yyvsp[(1) - (1)].Integer); }
    break;

  case 291:
/* Line 1787 of yacc.c  */
#line 1489 "../../ntpd/ntp_parser.y"
    {
			sim_node *sn;

			sn =  create_sim_node((yyvsp[(3) - (5)].Attr_val_fifo), (yyvsp[(4) - (5)].Sim_server_fifo));
			APPEND_G_FIFO(cfgt.sim_details, sn);

			/* Revert from ; to \n for end-of-command */
			old_config_style = 1;
		}
    break;

  case 292:
/* Line 1787 of yacc.c  */
#line 1506 "../../ntpd/ntp_parser.y"
    { old_config_style = 0; }
    break;

  case 293:
/* Line 1787 of yacc.c  */
#line 1511 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (3)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (3)].Attr_val));
		}
    break;

  case 294:
/* Line 1787 of yacc.c  */
#line 1516 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (2)].Attr_val));
		}
    break;

  case 295:
/* Line 1787 of yacc.c  */
#line 1524 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (3)].Integer), (yyvsp[(3) - (3)].Double)); }
    break;

  case 298:
/* Line 1787 of yacc.c  */
#line 1534 "../../ntpd/ntp_parser.y"
    {
			(yyval.Sim_server_fifo) = (yyvsp[(1) - (2)].Sim_server_fifo);
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[(2) - (2)].Sim_server));
		}
    break;

  case 299:
/* Line 1787 of yacc.c  */
#line 1539 "../../ntpd/ntp_parser.y"
    {
			(yyval.Sim_server_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[(1) - (1)].Sim_server));
		}
    break;

  case 300:
/* Line 1787 of yacc.c  */
#line 1547 "../../ntpd/ntp_parser.y"
    { (yyval.Sim_server) = ONLY_SIM(create_sim_server((yyvsp[(1) - (5)].Address_node), (yyvsp[(3) - (5)].Double), (yyvsp[(4) - (5)].Sim_script_fifo))); }
    break;

  case 301:
/* Line 1787 of yacc.c  */
#line 1552 "../../ntpd/ntp_parser.y"
    { (yyval.Double) = (yyvsp[(3) - (4)].Double); }
    break;

  case 302:
/* Line 1787 of yacc.c  */
#line 1557 "../../ntpd/ntp_parser.y"
    { (yyval.Address_node) = (yyvsp[(3) - (3)].Address_node); }
    break;

  case 303:
/* Line 1787 of yacc.c  */
#line 1562 "../../ntpd/ntp_parser.y"
    {
			(yyval.Sim_script_fifo) = (yyvsp[(1) - (2)].Sim_script_fifo);
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[(2) - (2)].Sim_script));
		}
    break;

  case 304:
/* Line 1787 of yacc.c  */
#line 1567 "../../ntpd/ntp_parser.y"
    {
			(yyval.Sim_script_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[(1) - (1)].Sim_script));
		}
    break;

  case 305:
/* Line 1787 of yacc.c  */
#line 1575 "../../ntpd/ntp_parser.y"
    { (yyval.Sim_script) = ONLY_SIM(create_sim_script_info((yyvsp[(3) - (6)].Double), (yyvsp[(5) - (6)].Attr_val_fifo))); }
    break;

  case 306:
/* Line 1787 of yacc.c  */
#line 1580 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (3)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (3)].Attr_val));
		}
    break;

  case 307:
/* Line 1787 of yacc.c  */
#line 1585 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (2)].Attr_val));
		}
    break;

  case 308:
/* Line 1787 of yacc.c  */
#line 1593 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (3)].Integer), (yyvsp[(3) - (3)].Double)); }
    break;


/* Line 1787 of yacc.c  */
#line 3600 "ntp_parser.c"
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


/* Line 2050 of yacc.c  */
#line 1604 "../../ntpd/ntp_parser.y"


void
yyerror(
	const char *msg
	)
{
	int retval;
	struct FILE_INFO * ip_ctx;

	ip_ctx = lex_current();
	ip_ctx->errpos = ip_ctx->tokpos;

	msyslog(LOG_ERR, "line %d column %d %s",
		ip_ctx->errpos.nline, ip_ctx->errpos.ncol, msg);
	if (!lex_from_file()) {
		/* Save the error message in the correct buffer */
		retval = snprintf(remote_config.err_msg + remote_config.err_pos,
				  MAXLINE - remote_config.err_pos,
				  "column %d %s",
				  ip_ctx->errpos.ncol, msg);

		/* Increment the value of err_pos */
		if (retval > 0)
			remote_config.err_pos += retval;

		/* Increment the number of errors */
		++remote_config.no_errors;
	}
}


/*
 * token_name - convert T_ token integers to text
 *		example: token_name(T_Server) returns "T_Server"
 */
const char *
token_name(
	int token
	)
{
	return yytname[YYTRANSLATE(token)];
}


/* Initial Testing function -- ignore */
#if 0
int main(int argc, char *argv[])
{
	ip_file = FOPEN(argv[1], "r");
	if (!ip_file)
		fprintf(stderr, "ERROR!! Could not open file: %s\n", argv[1]);
	yyparse();
	return 0;
}
#endif

