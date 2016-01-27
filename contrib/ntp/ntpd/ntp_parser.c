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
     T_UEcrypto = 429,
     T_UEcryptonak = 430,
     T_UEdigest = 431,
     T_Unconfig = 432,
     T_Unpeer = 433,
     T_Version = 434,
     T_WanderThreshold = 435,
     T_Week = 436,
     T_Wildcard = 437,
     T_Xleave = 438,
     T_Year = 439,
     T_Flag = 440,
     T_EOC = 441,
     T_Simulate = 442,
     T_Beep_Delay = 443,
     T_Sim_Duration = 444,
     T_Server_Offset = 445,
     T_Duration = 446,
     T_Freq_Offset = 447,
     T_Wander = 448,
     T_Jitter = 449,
     T_Prop_Delay = 450,
     T_Proc_Delay = 451
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
#define T_UEcrypto 429
#define T_UEcryptonak 430
#define T_UEdigest 431
#define T_Unconfig 432
#define T_Unpeer 433
#define T_Version 434
#define T_WanderThreshold 435
#define T_Week 436
#define T_Wildcard 437
#define T_Xleave 438
#define T_Year 439
#define T_Flag 440
#define T_EOC 441
#define T_Simulate 442
#define T_Beep_Delay 443
#define T_Sim_Duration 444
#define T_Server_Offset 445
#define T_Duration 446
#define T_Freq_Offset 447
#define T_Wander 448
#define T_Jitter 449
#define T_Prop_Delay 450
#define T_Proc_Delay 451



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
#line 557 "ntp_parser.c"
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
#line 585 "ntp_parser.c"

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
#define YYFINAL  213
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   624

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  202
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  105
/* YYNRULES -- Number of rules.  */
#define YYNRULES  316
/* YYNRULES -- Number of states.  */
#define YYNSTATES  422

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   451

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     198,   199,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   197,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   200,     2,   201,     2,     2,     2,     2,
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
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196
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
     471,   473,   475,   477,   479,   481,   483,   486,   489,   491,
     494,   496,   498,   500,   502,   504,   506,   508,   510,   512,
     514,   516,   518,   521,   524,   527,   530,   534,   536,   539,
     542,   545,   548,   552,   555,   557,   559,   561,   563,   565,
     567,   569,   571,   573,   575,   577,   580,   581,   586,   588,
     589,   590,   593,   596,   599,   602,   604,   606,   610,   614,
     616,   618,   620,   622,   624,   626,   628,   630,   632,   635,
     638,   640,   642,   644,   646,   648,   650,   652,   654,   657,
     659,   662,   664,   666,   668,   674,   677,   679,   682,   684,
     686,   688,   690,   692,   694,   700,   702,   706,   709,   713,
     715,   717,   720,   722,   728,   733,   737,   740,   742,   749,
     753,   756,   760,   762,   764,   766,   768
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     203,     0,    -1,   204,    -1,   204,   205,   186,    -1,   205,
     186,    -1,     1,   186,    -1,    -1,   206,    -1,   219,    -1,
     221,    -1,   222,    -1,   231,    -1,   239,    -1,   226,    -1,
     248,    -1,   253,    -1,   257,    -1,   262,    -1,   266,    -1,
     293,    -1,   207,   208,   211,    -1,   146,    -1,   131,    -1,
     126,    -1,    14,    -1,    84,    -1,   209,    -1,   210,   158,
      -1,   158,    -1,    66,    -1,    68,    -1,    -1,   211,   212,
      -1,   213,    -1,   215,    -1,   217,    -1,   214,    -1,     9,
      -1,    17,    -1,    53,    -1,   116,    -1,   133,    -1,   134,
      -1,   169,    -1,   183,    -1,   216,    61,    -1,   216,   173,
      -1,    70,    -1,   100,    -1,    92,    -1,   171,    -1,   102,
      -1,   179,    -1,   218,   158,    -1,    54,    -1,   220,   208,
      -1,   177,    -1,   178,    -1,    15,    -1,    85,   290,    -1,
     107,   290,    -1,    93,    61,    -1,    10,    61,    -1,    22,
      61,    -1,    23,   223,    -1,    71,   158,    -1,    72,   158,
      -1,   140,    61,    -1,   143,    61,    -1,   170,   286,    -1,
     122,   158,    -1,    -1,   223,   224,    -1,   225,   158,    -1,
     143,    61,    -1,    51,    -1,    54,    -1,   136,    -1,   137,
      -1,    28,    -1,   167,   227,    -1,   227,   228,    -1,   228,
      -1,   229,    61,    -1,   230,   292,    -1,    21,   291,    -1,
      19,    -1,    48,    -1,   123,    -1,   124,    -1,   101,    -1,
      13,    -1,    98,    -1,    90,    -1,    96,    -1,    88,    -1,
     150,   232,    -1,   152,   158,    -1,    41,   233,   234,    -1,
     232,   233,    -1,   233,    -1,    20,    -1,    24,    -1,    82,
      -1,   127,    -1,   138,    -1,   160,    -1,   165,    -1,   135,
      -1,    -1,   234,   235,    -1,    40,   158,    -1,   172,   238,
      -1,   236,    -1,   237,    -1,    78,    -1,   109,    -1,    37,
      -1,    29,    -1,   112,    -1,   129,    -1,    26,    -1,   181,
      -1,   105,    -1,   184,    -1,     4,    -1,    30,   242,    -1,
     106,   245,    -1,   142,   208,   240,    -1,   142,   209,    86,
     209,   240,    -1,   142,    27,   240,    -1,   142,    66,    27,
     240,    -1,   142,    68,    27,   240,    -1,   142,   148,   240,
      -1,    -1,   240,   241,    -1,    47,    -1,    55,    -1,    73,
      -1,    74,    -1,    77,    -1,    83,    -1,   110,    -1,   111,
      -1,   114,    -1,   115,    -1,   117,    -1,   118,    -1,   119,
      -1,   121,    -1,   179,    -1,   242,   243,    -1,   243,    -1,
     244,    61,    -1,    11,    -1,    99,    -1,   104,    -1,   245,
     246,    -1,   246,    -1,   247,    61,    -1,    56,    -1,    57,
      -1,    58,    -1,    59,    -1,    87,    -1,    89,    -1,    91,
      -1,    97,    -1,    50,   208,   249,    -1,   249,   250,    -1,
     250,    -1,   251,   292,    -1,   252,   291,    -1,   157,    61,
      -1,     3,   158,    -1,   139,   158,    -1,   162,    -1,   163,
      -1,    43,    -1,    44,    -1,    45,    -1,    46,    -1,   144,
     254,    -1,   254,   255,    -1,   255,    -1,   256,    61,    -1,
      95,    -1,   149,    -1,    42,    -1,    37,   258,    -1,    29,
     258,    -1,   258,   259,    -1,   259,    -1,   260,    -1,   261,
      -1,     8,    -1,    12,    -1,    18,    -1,    69,    -1,   104,
      -1,   120,    -1,   103,    -1,   151,    -1,   174,    -1,   175,
      -1,   176,    -1,   166,   263,    -1,   263,   264,    -1,   264,
      -1,   265,   292,    -1,     6,    -1,    31,    -1,    49,    -1,
      52,    -1,   125,    -1,   153,    -1,   154,    -1,   155,    -1,
     156,    -1,   161,    -1,   278,    -1,   282,    -1,   267,   292,
      -1,   268,    61,    -1,   269,   158,    -1,   270,   158,    -1,
      60,   158,   205,    -1,    38,    -1,    33,   271,    -1,    80,
     276,    -1,   128,   289,    -1,   147,   272,    -1,   168,   209,
     274,    -1,   171,   285,    -1,    16,    -1,   113,    -1,   161,
      -1,    35,    -1,    76,    -1,    54,    -1,    75,    -1,    81,
      -1,   130,    -1,   145,    -1,   158,    -1,   158,    32,    -1,
      -1,   158,   197,   158,   273,    -1,    27,    -1,    -1,    -1,
     274,   275,    -1,   132,    61,    -1,    62,   209,    -1,   276,
     277,    -1,   277,    -1,   158,    -1,   279,   281,   280,    -1,
     279,   281,   158,    -1,    62,    -1,   108,    -1,     5,    -1,
      65,    -1,    67,    -1,   182,    -1,    79,    -1,    55,    -1,
      34,    -1,   141,   283,    -1,   283,   284,    -1,   284,    -1,
       7,    -1,     8,    -1,    25,    -1,    64,    -1,    94,    -1,
     159,    -1,   164,    -1,   285,    61,    -1,    61,    -1,   286,
     287,    -1,   287,    -1,    61,    -1,   288,    -1,   198,    61,
      36,    61,   199,    -1,   289,   158,    -1,   158,    -1,   290,
     208,    -1,   208,    -1,    61,    -1,   169,    -1,    39,    -1,
      61,    -1,    32,    -1,   294,   200,   295,   298,   201,    -1,
     187,    -1,   295,   296,   186,    -1,   296,   186,    -1,   297,
     197,   292,    -1,   188,    -1,   189,    -1,   298,   299,    -1,
     299,    -1,   301,   200,   300,   302,   201,    -1,   190,   197,
     292,   186,    -1,   146,   197,   208,    -1,   302,   303,    -1,
     303,    -1,   191,   197,   292,   200,   304,   201,    -1,   304,
     305,   186,    -1,   305,   186,    -1,   306,   197,   292,    -1,
     192,    -1,   193,    -1,   194,    -1,   195,    -1,   196,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   369,   369,   373,   374,   375,   390,   391,   392,   393,
     394,   395,   396,   397,   398,   399,   400,   401,   402,   403,
     411,   421,   422,   423,   424,   425,   429,   430,   435,   440,
     442,   448,   449,   457,   458,   459,   463,   468,   469,   470,
     471,   472,   473,   474,   475,   479,   481,   486,   487,   488,
     489,   490,   491,   495,   500,   509,   519,   520,   530,   532,
     534,   536,   547,   554,   556,   561,   563,   565,   567,   569,
     578,   584,   585,   593,   595,   607,   608,   609,   610,   611,
     620,   625,   630,   638,   640,   642,   647,   648,   649,   650,
     651,   652,   656,   657,   658,   659,   668,   670,   679,   689,
     694,   702,   703,   704,   705,   706,   707,   708,   709,   714,
     715,   723,   733,   742,   757,   762,   763,   767,   768,   772,
     773,   774,   775,   776,   777,   778,   787,   791,   795,   803,
     811,   819,   834,   849,   862,   863,   871,   872,   873,   874,
     875,   876,   877,   878,   879,   880,   881,   882,   883,   884,
     885,   889,   894,   902,   907,   908,   909,   913,   918,   926,
     931,   932,   933,   934,   935,   936,   937,   938,   946,   956,
     961,   969,   971,   973,   982,   984,   989,   990,   994,   995,
     996,   997,  1005,  1010,  1015,  1023,  1028,  1029,  1030,  1039,
    1041,  1046,  1051,  1059,  1061,  1078,  1079,  1080,  1081,  1082,
    1083,  1087,  1088,  1089,  1090,  1091,  1099,  1104,  1109,  1117,
    1122,  1123,  1124,  1125,  1126,  1127,  1128,  1129,  1130,  1131,
    1140,  1141,  1142,  1149,  1156,  1163,  1179,  1198,  1200,  1202,
    1204,  1206,  1208,  1215,  1220,  1221,  1222,  1226,  1230,  1239,
    1240,  1244,  1245,  1246,  1250,  1261,  1275,  1287,  1292,  1294,
    1299,  1300,  1308,  1310,  1318,  1323,  1331,  1356,  1363,  1373,
    1374,  1378,  1379,  1380,  1381,  1385,  1386,  1387,  1391,  1396,
    1401,  1409,  1410,  1411,  1412,  1413,  1414,  1415,  1425,  1430,
    1438,  1443,  1451,  1453,  1457,  1462,  1467,  1475,  1480,  1488,
    1497,  1498,  1502,  1503,  1512,  1530,  1534,  1539,  1547,  1552,
    1553,  1557,  1562,  1570,  1575,  1580,  1585,  1590,  1598,  1603,
    1608,  1616,  1621,  1622,  1623,  1624,  1625
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
  "T_Trustedkey", "T_Ttl", "T_Type", "T_U_int", "T_UEcrypto",
  "T_UEcryptonak", "T_UEdigest", "T_Unconfig", "T_Unpeer", "T_Version",
  "T_WanderThreshold", "T_Week", "T_Wildcard", "T_Xleave", "T_Year",
  "T_Flag", "T_EOC", "T_Simulate", "T_Beep_Delay", "T_Sim_Duration",
  "T_Server_Offset", "T_Duration", "T_Freq_Offset", "T_Wander", "T_Jitter",
  "T_Prop_Delay", "T_Proc_Delay", "'='", "'('", "')'", "'{'", "'}'",
  "$accept", "configuration", "command_list", "command", "server_command",
  "client_type", "address", "ip_address", "address_fam", "option_list",
  "option", "option_flag", "option_flag_keyword", "option_int",
  "option_int_keyword", "option_str", "option_str_keyword",
  "unpeer_command", "unpeer_keyword", "other_mode_command",
  "authentication_command", "crypto_command_list", "crypto_command",
  "crypto_str_keyword", "orphan_mode_command", "tos_option_list",
  "tos_option", "tos_option_int_keyword", "tos_option_dbl_keyword",
  "monitoring_command", "stats_list", "stat", "filegen_option_list",
  "filegen_option", "link_nolink", "enable_disable", "filegen_type",
  "access_control_command", "ac_flag_list", "access_control_flag",
  "discard_option_list", "discard_option", "discard_option_keyword",
  "mru_option_list", "mru_option", "mru_option_keyword", "fudge_command",
  "fudge_factor_list", "fudge_factor", "fudge_factor_dbl_keyword",
  "fudge_factor_bool_keyword", "rlimit_command", "rlimit_option_list",
  "rlimit_option", "rlimit_option_keyword", "system_option_command",
  "system_option_list", "system_option", "system_option_flag_keyword",
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
     445,   446,   447,   448,   449,   450,   451,    61,    40,    41,
     123,   125
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   202,   203,   204,   204,   204,   205,   205,   205,   205,
     205,   205,   205,   205,   205,   205,   205,   205,   205,   205,
     206,   207,   207,   207,   207,   207,   208,   208,   209,   210,
     210,   211,   211,   212,   212,   212,   213,   214,   214,   214,
     214,   214,   214,   214,   214,   215,   215,   216,   216,   216,
     216,   216,   216,   217,   218,   219,   220,   220,   221,   221,
     221,   221,   222,   222,   222,   222,   222,   222,   222,   222,
     222,   223,   223,   224,   224,   225,   225,   225,   225,   225,
     226,   227,   227,   228,   228,   228,   229,   229,   229,   229,
     229,   229,   230,   230,   230,   230,   231,   231,   231,   232,
     232,   233,   233,   233,   233,   233,   233,   233,   233,   234,
     234,   235,   235,   235,   235,   236,   236,   237,   237,   238,
     238,   238,   238,   238,   238,   238,   239,   239,   239,   239,
     239,   239,   239,   239,   240,   240,   241,   241,   241,   241,
     241,   241,   241,   241,   241,   241,   241,   241,   241,   241,
     241,   242,   242,   243,   244,   244,   244,   245,   245,   246,
     247,   247,   247,   247,   247,   247,   247,   247,   248,   249,
     249,   250,   250,   250,   250,   250,   251,   251,   252,   252,
     252,   252,   253,   254,   254,   255,   256,   256,   256,   257,
     257,   258,   258,   259,   259,   260,   260,   260,   260,   260,
     260,   261,   261,   261,   261,   261,   262,   263,   263,   264,
     265,   265,   265,   265,   265,   265,   265,   265,   265,   265,
     266,   266,   266,   266,   266,   266,   266,   266,   266,   266,
     266,   266,   266,   266,   267,   267,   267,   268,   268,   269,
     269,   270,   270,   270,   271,   271,   271,   272,   273,   273,
     274,   274,   275,   275,   276,   276,   277,   278,   278,   279,
     279,   280,   280,   280,   280,   281,   281,   281,   282,   283,
     283,   284,   284,   284,   284,   284,   284,   284,   285,   285,
     286,   286,   287,   287,   288,   289,   289,   290,   290,   291,
     291,   291,   292,   292,   293,   294,   295,   295,   296,   297,
     297,   298,   298,   299,   300,   301,   302,   302,   303,   304,
     304,   305,   306,   306,   306,   306,   306
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
       1,     1,     1,     1,     1,     1,     2,     2,     1,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     2,     2,     2,     3,     1,     2,     2,
       2,     2,     3,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     0,     4,     1,     0,
       0,     2,     2,     2,     2,     1,     1,     3,     3,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     1,
       2,     1,     1,     1,     5,     2,     1,     2,     1,     1,
       1,     1,     1,     1,     5,     1,     3,     2,     3,     1,
       1,     2,     1,     5,     4,     3,     2,     1,     6,     3,
       2,     3,     1,     1,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       0,     0,     0,    24,    58,   234,     0,    71,     0,     0,
     246,   237,     0,   227,     0,     0,   239,     0,   259,     0,
       0,   240,   238,     0,   241,    25,     0,     0,     0,     0,
     260,   235,     0,    23,     0,   242,    22,     0,     0,     0,
       0,     0,   243,    21,     0,     0,     0,   236,     0,     0,
       0,     0,     0,    56,    57,   295,     0,     2,     0,     7,
       0,     8,     0,     9,    10,    13,    11,    12,    14,    15,
      16,    17,    18,     0,     0,     0,     0,   220,     0,   221,
      19,     0,     5,    62,    63,    64,   195,   196,   197,   198,
     201,   199,   200,   202,   203,   204,   205,   190,   192,   193,
     194,   154,   155,   156,   126,   152,     0,   244,   228,   189,
     101,   102,   103,   104,   108,   105,   106,   107,   109,    29,
      30,    28,     0,    26,     0,     6,    65,    66,   256,   229,
     255,   288,    59,    61,   160,   161,   162,   163,   164,   165,
     166,   167,   127,   158,     0,    60,    70,   286,   230,    67,
     271,   272,   273,   274,   275,   276,   277,   268,   270,   134,
      29,    30,   134,   134,    26,    68,   188,   186,   187,   182,
     184,     0,     0,   231,    96,   100,    97,   210,   211,   212,
     213,   214,   215,   216,   217,   218,   219,   206,   208,     0,
      91,    86,     0,    87,    95,    93,    94,    92,    90,    88,
      89,    80,    82,     0,     0,   250,   282,     0,    69,   281,
     283,   279,   233,     1,     0,     4,    31,    55,   293,   292,
     222,   223,   224,   225,   267,   266,   265,     0,     0,    79,
      75,    76,    77,    78,     0,    72,     0,   191,   151,   153,
     245,    98,     0,   178,   179,   180,   181,     0,     0,   176,
     177,   168,   170,     0,     0,    27,   226,   254,   287,   157,
     159,   285,   269,   130,   134,   134,   133,   128,     0,   183,
     185,     0,    99,   207,   209,   291,   289,   290,    85,    81,
      83,    84,   232,     0,   280,   278,     3,    20,   261,   262,
     263,   258,   264,   257,   299,   300,     0,     0,     0,    74,
      73,   118,   117,     0,   115,   116,     0,   110,   113,   114,
     174,   175,   173,   169,   171,   172,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   135,   131,   132,   134,   249,     0,     0,   251,     0,
      37,    38,    39,    54,    47,    49,    48,    51,    40,    41,
      42,    43,    50,    52,    44,    32,    33,    36,    34,     0,
      35,     0,     0,     0,     0,   302,     0,   297,     0,   111,
     125,   121,   123,   119,   120,   122,   124,   112,   129,   248,
     247,   253,   252,     0,    45,    46,    53,     0,   296,   294,
     301,     0,   298,   284,   305,     0,     0,     0,     0,     0,
     307,     0,     0,   303,   306,   304,     0,     0,   312,   313,
     314,   315,   316,     0,     0,     0,   308,     0,   310,     0,
     309,   311
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    56,    57,    58,    59,    60,   131,   123,   124,   287,
     355,   356,   357,   358,   359,   360,   361,    61,    62,    63,
      64,    85,   235,   236,    65,   201,   202,   203,   204,    66,
     174,   118,   241,   307,   308,   309,   377,    67,   263,   331,
     104,   105,   106,   142,   143,   144,    68,   251,   252,   253,
     254,    69,   169,   170,   171,    70,    97,    98,    99,   100,
      71,   187,   188,   189,    72,    73,    74,    75,    76,   108,
     173,   380,   282,   338,   129,   130,    77,    78,   293,   227,
      79,   157,   158,   212,   208,   209,   210,   148,   132,   278,
     220,    80,    81,   296,   297,   298,   364,   365,   396,   366,
     399,   400,   413,   414,   415
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -188
static const yytype_int16 yypact[] =
{
       5,  -160,   -28,  -188,  -188,  -188,   -24,  -188,    60,     0,
    -119,  -188,    60,  -188,   118,     7,  -188,  -117,  -188,  -110,
    -108,  -188,  -188,  -101,  -188,  -188,     7,    -1,   345,     7,
    -188,  -188,   -96,  -188,   -95,  -188,  -188,    21,    -3,    73,
      33,    11,  -188,  -188,   -94,   118,   -61,  -188,    43,   446,
     -57,   -58,    41,  -188,  -188,  -188,   105,   179,   -79,  -188,
       7,  -188,     7,  -188,  -188,  -188,  -188,  -188,  -188,  -188,
    -188,  -188,  -188,    -7,    48,   -48,   -39,  -188,    24,  -188,
    -188,   -86,  -188,  -188,  -188,    42,  -188,  -188,  -188,  -188,
    -188,  -188,  -188,  -188,  -188,  -188,  -188,    60,  -188,  -188,
    -188,  -188,  -188,  -188,     0,  -188,    59,    89,  -188,    60,
    -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,
    -188,  -188,    80,  -188,     9,   338,  -188,  -188,  -188,  -101,
    -188,  -188,     7,  -188,  -188,  -188,  -188,  -188,  -188,  -188,
    -188,  -188,   345,  -188,    67,     7,  -188,  -188,    12,  -188,
    -188,  -188,  -188,  -188,  -188,  -188,  -188,    -3,  -188,  -188,
     107,   116,  -188,  -188,    83,  -188,  -188,  -188,  -188,    11,
    -188,   113,   -20,  -188,   118,  -188,  -188,  -188,  -188,  -188,
    -188,  -188,  -188,  -188,  -188,  -188,  -188,    43,  -188,    -7,
    -188,  -188,   -25,  -188,  -188,  -188,  -188,  -188,  -188,  -188,
    -188,   446,  -188,   127,    -7,  -188,  -188,   129,   -58,  -188,
    -188,  -188,   142,  -188,    19,  -188,  -188,  -188,  -188,  -188,
    -188,  -188,  -188,  -188,  -188,  -188,  -188,     4,  -158,  -188,
    -188,  -188,  -188,  -188,   145,  -188,    49,  -188,  -188,  -188,
    -188,   233,    55,  -188,  -188,  -188,  -188,    64,   157,  -188,
    -188,    80,  -188,    -7,   -25,  -188,  -188,  -188,  -188,  -188,
    -188,  -188,  -188,   445,  -188,  -188,   445,   445,   -57,  -188,
    -188,    82,  -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,
    -188,  -188,   -44,   202,  -188,  -188,  -188,   324,  -188,  -188,
    -188,  -188,  -188,  -188,  -188,  -188,   -30,    58,    50,  -188,
    -188,  -188,  -188,    88,  -188,  -188,     3,  -188,  -188,  -188,
    -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,
    -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,
    -188,  -188,   445,   445,  -188,   221,   -57,   188,  -188,   191,
    -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,
    -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,   -51,
    -188,    99,    61,    75,  -114,  -188,    65,  -188,    -7,  -188,
    -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,   445,  -188,
    -188,  -188,  -188,    68,  -188,  -188,  -188,     7,  -188,  -188,
    -188,    76,  -188,  -188,  -188,    71,    78,    -7,    74,  -178,
    -188,    90,    -7,  -188,  -188,  -188,    77,    32,  -188,  -188,
    -188,  -188,  -188,   101,    93,    84,  -188,    94,  -188,    -7,
    -188,  -188
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -188,  -188,  -188,   -41,  -188,  -188,   -15,   -38,  -188,  -188,
    -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,  -188,
    -188,  -188,  -188,  -188,  -188,  -188,    81,  -188,  -188,  -188,
    -188,   -37,  -188,  -188,  -188,  -188,  -188,  -188,  -111,  -188,
    -188,   170,  -188,  -188,   133,  -188,  -188,  -188,    37,  -188,
    -188,  -188,  -188,   115,  -188,  -188,   277,   -53,  -188,  -188,
    -188,  -188,   103,  -188,  -188,  -188,  -188,  -188,  -188,  -188,
    -188,  -188,  -188,  -188,  -188,   162,  -188,  -188,  -188,  -188,
    -188,  -188,   143,  -188,  -188,    91,  -188,  -188,   274,    52,
    -187,  -188,  -188,  -188,     8,  -188,  -188,   -56,  -188,  -188,
    -188,   -87,  -188,  -100,  -188
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -7
static const yytype_int16 yytable[] =
{
     122,   164,   274,   206,   150,   151,     1,   370,   175,   288,
     384,   101,   205,   398,   275,     2,   214,   281,   336,     3,
       4,     5,   152,   403,   163,   218,    82,     6,     7,   371,
     294,   295,   362,    83,     8,     9,   276,    84,    10,   107,
      11,   125,    12,    13,   237,   216,    14,   217,   126,   177,
     127,   266,   267,   166,   219,    15,   237,   128,   224,    16,
     133,   153,   146,   147,   172,    17,   314,    18,    86,   289,
     229,   290,    87,   119,   178,   120,    19,    20,    88,   225,
      21,    22,   149,   242,   256,    23,    24,   389,   337,    25,
      26,   154,   179,   230,   165,   180,   231,   176,    27,   102,
     159,   121,   211,   226,   103,   213,   167,   215,   372,   221,
     222,    28,    29,    30,   228,   373,   362,   258,    31,   223,
     239,   240,   385,   243,   244,   245,   246,    32,   260,    89,
     258,    33,   374,    34,   264,    35,    36,   272,   110,   160,
     207,   161,   111,   265,   277,    37,    38,    39,    40,    41,
      42,    43,    44,   332,   333,    45,   155,    46,   294,   295,
     168,   156,   291,    90,    91,   121,    47,   255,   181,   268,
     261,    48,    49,    50,   270,    51,    52,   271,   232,   233,
      92,   392,    53,    54,   375,   234,   292,   376,   280,     2,
     283,    -6,    55,     3,     4,     5,   182,   183,   184,   185,
     112,     6,     7,   285,   186,   286,   299,   300,     8,     9,
     401,    93,    10,   310,    11,   406,    12,    13,   312,   247,
      14,   162,   311,   378,   408,   409,   410,   411,   412,    15,
     334,   121,   421,    16,    94,    95,    96,   248,   339,    17,
     335,    18,   249,   250,   367,   113,   369,   368,   379,   382,
      19,    20,   383,   114,    21,    22,   115,   386,   387,    23,
      24,   388,   301,    25,    26,   391,   395,   393,   397,   398,
     302,   402,    27,   303,   238,   259,   405,   407,   116,   418,
     420,   419,   279,   117,   269,    28,    29,    30,   313,   109,
     273,   257,    31,   408,   409,   410,   411,   412,   381,   284,
     262,    32,   416,   145,   363,    33,   315,    34,   390,    35,
      36,   304,   404,   417,     0,     0,     0,     0,     0,    37,
      38,    39,    40,    41,    42,    43,    44,     0,     0,    45,
       0,    46,     0,   340,     0,     0,     0,     0,     0,     0,
      47,   341,   305,     0,     0,    48,    49,    50,     2,    51,
      52,     0,     3,     4,     5,     0,    53,    54,     0,     0,
       6,     7,     0,     0,     0,    -6,    55,     8,     9,     0,
       0,    10,   394,    11,     0,    12,    13,   342,   343,    14,
       0,     0,     0,     0,     0,     0,     0,     0,    15,     0,
       0,     0,    16,     0,   344,     0,     0,     0,    17,     0,
      18,   134,   135,   136,   137,   306,     0,     0,     0,    19,
      20,     0,     0,    21,    22,     0,   345,     0,    23,    24,
       0,     0,    25,    26,   346,     0,   347,     0,     0,     0,
       0,    27,   138,     0,   139,     0,   140,     0,     0,     0,
     348,     0,   141,     0,    28,    29,    30,     0,     0,     0,
       0,    31,     0,     0,     0,     0,     0,   349,   350,   190,
      32,     0,     0,     0,    33,   191,    34,   192,    35,    36,
       0,     0,     0,     0,     0,     0,     0,     0,    37,    38,
      39,    40,    41,    42,    43,    44,     0,     0,    45,     0,
      46,     0,   316,   351,   193,   352,     0,     0,     0,    47,
     317,     0,     0,   353,    48,    49,    50,   354,    51,    52,
       0,     0,     0,     0,     0,    53,    54,     0,   318,   319,
       0,     0,   320,     0,     0,    55,     0,     0,   321,     0,
       0,     0,     0,     0,   194,     0,   195,     0,     0,     0,
       0,     0,   196,     0,   197,     0,     0,   198,     0,     0,
       0,     0,     0,     0,     0,   322,   323,     0,     0,   324,
     325,     0,   326,   327,   328,     0,   329,     0,     0,   199,
     200,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   330
};

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-188)))

#define yytable_value_is_error(Yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
      15,    39,   189,    61,     7,     8,     1,     4,    45,     5,
      61,    11,    50,   191,    39,    10,    57,   204,    62,    14,
      15,    16,    25,   201,    39,    32,   186,    22,    23,    26,
     188,   189,   146,    61,    29,    30,    61,    61,    33,   158,
      35,   158,    37,    38,    97,    60,    41,    62,   158,     6,
     158,   162,   163,    42,    61,    50,   109,   158,    34,    54,
      61,    64,   158,   158,   158,    60,   253,    62,     8,    65,
      28,    67,    12,    66,    31,    68,    71,    72,    18,    55,
      75,    76,    61,     3,   125,    80,    81,   201,   132,    84,
      85,    94,    49,    51,    61,    52,    54,   158,    93,    99,
      27,   158,    61,    79,   104,     0,    95,   186,   105,    61,
     158,   106,   107,   108,   200,   112,   146,   132,   113,   158,
      61,    32,   173,    43,    44,    45,    46,   122,    61,    69,
     145,   126,   129,   128,    27,   130,   131,   174,    20,    66,
     198,    68,    24,    27,   169,   140,   141,   142,   143,   144,
     145,   146,   147,   264,   265,   150,   159,   152,   188,   189,
     149,   164,   158,   103,   104,   158,   161,   158,   125,    86,
     158,   166,   167,   168,    61,   170,   171,   197,   136,   137,
     120,   368,   177,   178,   181,   143,   182,   184,    61,    10,
      61,   186,   187,    14,    15,    16,   153,   154,   155,   156,
      82,    22,    23,    61,   161,   186,    61,   158,    29,    30,
     397,   151,    33,   158,    35,   402,    37,    38,    61,   139,
      41,   148,   158,   334,   192,   193,   194,   195,   196,    50,
     268,   158,   419,    54,   174,   175,   176,   157,    36,    60,
     158,    62,   162,   163,   186,   127,   158,   197,    27,    61,
      71,    72,    61,   135,    75,    76,   138,   158,   197,    80,
      81,   186,    29,    84,    85,   200,   190,   199,   197,   191,
      37,   197,    93,    40,   104,   142,   186,   200,   160,   186,
     186,   197,   201,   165,   169,   106,   107,   108,   251,    12,
     187,   129,   113,   192,   193,   194,   195,   196,   336,   208,
     157,   122,   201,    29,   296,   126,   254,   128,   364,   130,
     131,    78,   399,   413,    -1,    -1,    -1,    -1,    -1,   140,
     141,   142,   143,   144,   145,   146,   147,    -1,    -1,   150,
      -1,   152,    -1,     9,    -1,    -1,    -1,    -1,    -1,    -1,
     161,    17,   109,    -1,    -1,   166,   167,   168,    10,   170,
     171,    -1,    14,    15,    16,    -1,   177,   178,    -1,    -1,
      22,    23,    -1,    -1,    -1,   186,   187,    29,    30,    -1,
      -1,    33,   387,    35,    -1,    37,    38,    53,    54,    41,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    50,    -1,
      -1,    -1,    54,    -1,    70,    -1,    -1,    -1,    60,    -1,
      62,    56,    57,    58,    59,   172,    -1,    -1,    -1,    71,
      72,    -1,    -1,    75,    76,    -1,    92,    -1,    80,    81,
      -1,    -1,    84,    85,   100,    -1,   102,    -1,    -1,    -1,
      -1,    93,    87,    -1,    89,    -1,    91,    -1,    -1,    -1,
     116,    -1,    97,    -1,   106,   107,   108,    -1,    -1,    -1,
      -1,   113,    -1,    -1,    -1,    -1,    -1,   133,   134,    13,
     122,    -1,    -1,    -1,   126,    19,   128,    21,   130,   131,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   140,   141,
     142,   143,   144,   145,   146,   147,    -1,    -1,   150,    -1,
     152,    -1,    47,   169,    48,   171,    -1,    -1,    -1,   161,
      55,    -1,    -1,   179,   166,   167,   168,   183,   170,   171,
      -1,    -1,    -1,    -1,    -1,   177,   178,    -1,    73,    74,
      -1,    -1,    77,    -1,    -1,   187,    -1,    -1,    83,    -1,
      -1,    -1,    -1,    -1,    88,    -1,    90,    -1,    -1,    -1,
      -1,    -1,    96,    -1,    98,    -1,    -1,   101,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   110,   111,    -1,    -1,   114,
     115,    -1,   117,   118,   119,    -1,   121,    -1,    -1,   123,
     124,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   179
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
     168,   170,   171,   177,   178,   187,   203,   204,   205,   206,
     207,   219,   220,   221,   222,   226,   231,   239,   248,   253,
     257,   262,   266,   267,   268,   269,   270,   278,   279,   282,
     293,   294,   186,    61,    61,   223,     8,    12,    18,    69,
     103,   104,   120,   151,   174,   175,   176,   258,   259,   260,
     261,    11,    99,   104,   242,   243,   244,   158,   271,   258,
      20,    24,    82,   127,   135,   138,   160,   165,   233,    66,
      68,   158,   208,   209,   210,   158,   158,   158,   158,   276,
     277,   208,   290,    61,    56,    57,    58,    59,    87,    89,
      91,    97,   245,   246,   247,   290,   158,   158,   289,    61,
       7,     8,    25,    64,    94,   159,   164,   283,   284,    27,
      66,    68,   148,   208,   209,    61,    42,    95,   149,   254,
     255,   256,   158,   272,   232,   233,   158,     6,    31,    49,
      52,   125,   153,   154,   155,   156,   161,   263,   264,   265,
      13,    19,    21,    48,    88,    90,    96,    98,   101,   123,
     124,   227,   228,   229,   230,   209,    61,   198,   286,   287,
     288,    61,   285,     0,   205,   186,   208,   208,    32,    61,
     292,    61,   158,   158,    34,    55,    79,   281,   200,    28,
      51,    54,   136,   137,   143,   224,   225,   259,   243,    61,
      32,   234,     3,    43,    44,    45,    46,   139,   157,   162,
     163,   249,   250,   251,   252,   158,   205,   277,   208,   246,
      61,   158,   284,   240,    27,    27,   240,   240,    86,   255,
      61,   197,   233,   264,   292,    39,    61,   169,   291,   228,
      61,   292,   274,    61,   287,    61,   186,   211,     5,    65,
      67,   158,   182,   280,   188,   189,   295,   296,   297,    61,
     158,    29,    37,    40,    78,   109,   172,   235,   236,   237,
     158,   158,    61,   250,   292,   291,    47,    55,    73,    74,
      77,    83,   110,   111,   114,   115,   117,   118,   119,   121,
     179,   241,   240,   240,   209,   158,    62,   132,   275,    36,
       9,    17,    53,    54,    70,    92,   100,   102,   116,   133,
     134,   169,   171,   179,   183,   212,   213,   214,   215,   216,
     217,   218,   146,   296,   298,   299,   301,   186,   197,   158,
       4,    26,   105,   112,   129,   181,   184,   238,   240,    27,
     273,   209,    61,    61,    61,   173,   158,   197,   186,   201,
     299,   200,   292,   199,   208,   190,   300,   197,   191,   302,
     303,   292,   197,   201,   303,   186,   292,   200,   192,   193,
     194,   195,   196,   304,   305,   306,   201,   305,   186,   197,
     186,   292
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
#line 376 "../../ntpd/ntp_parser.y"
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
#line 412 "../../ntpd/ntp_parser.y"
    {
			peer_node *my_node;

			my_node = create_peer_node((yyvsp[(1) - (3)].Integer), (yyvsp[(2) - (3)].Address_node), (yyvsp[(3) - (3)].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.peers, my_node);
		}
    break;

  case 27:
/* Line 1787 of yacc.c  */
#line 431 "../../ntpd/ntp_parser.y"
    { (yyval.Address_node) = create_address_node((yyvsp[(2) - (2)].String), (yyvsp[(1) - (2)].Integer)); }
    break;

  case 28:
/* Line 1787 of yacc.c  */
#line 436 "../../ntpd/ntp_parser.y"
    { (yyval.Address_node) = create_address_node((yyvsp[(1) - (1)].String), AF_UNSPEC); }
    break;

  case 29:
/* Line 1787 of yacc.c  */
#line 441 "../../ntpd/ntp_parser.y"
    { (yyval.Integer) = AF_INET; }
    break;

  case 30:
/* Line 1787 of yacc.c  */
#line 443 "../../ntpd/ntp_parser.y"
    { (yyval.Integer) = AF_INET6; }
    break;

  case 31:
/* Line 1787 of yacc.c  */
#line 448 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val_fifo) = NULL; }
    break;

  case 32:
/* Line 1787 of yacc.c  */
#line 450 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 36:
/* Line 1787 of yacc.c  */
#line 464 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[(1) - (1)].Integer)); }
    break;

  case 45:
/* Line 1787 of yacc.c  */
#line 480 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 46:
/* Line 1787 of yacc.c  */
#line 482 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_uval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 53:
/* Line 1787 of yacc.c  */
#line 496 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_sval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].String)); }
    break;

  case 55:
/* Line 1787 of yacc.c  */
#line 510 "../../ntpd/ntp_parser.y"
    {
			unpeer_node *my_node;

			my_node = create_unpeer_node((yyvsp[(2) - (2)].Address_node));
			if (my_node)
				APPEND_G_FIFO(cfgt.unpeers, my_node);
		}
    break;

  case 58:
/* Line 1787 of yacc.c  */
#line 531 "../../ntpd/ntp_parser.y"
    { cfgt.broadcastclient = 1; }
    break;

  case 59:
/* Line 1787 of yacc.c  */
#line 533 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.manycastserver, (yyvsp[(2) - (2)].Address_fifo)); }
    break;

  case 60:
/* Line 1787 of yacc.c  */
#line 535 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.multicastclient, (yyvsp[(2) - (2)].Address_fifo)); }
    break;

  case 61:
/* Line 1787 of yacc.c  */
#line 537 "../../ntpd/ntp_parser.y"
    { cfgt.mdnstries = (yyvsp[(2) - (2)].Integer); }
    break;

  case 62:
/* Line 1787 of yacc.c  */
#line 548 "../../ntpd/ntp_parser.y"
    {
			attr_val *atrv;

			atrv = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer));
			APPEND_G_FIFO(cfgt.vars, atrv);
		}
    break;

  case 63:
/* Line 1787 of yacc.c  */
#line 555 "../../ntpd/ntp_parser.y"
    { cfgt.auth.control_key = (yyvsp[(2) - (2)].Integer); }
    break;

  case 64:
/* Line 1787 of yacc.c  */
#line 557 "../../ntpd/ntp_parser.y"
    {
			cfgt.auth.cryptosw++;
			CONCAT_G_FIFOS(cfgt.auth.crypto_cmd_list, (yyvsp[(2) - (2)].Attr_val_fifo));
		}
    break;

  case 65:
/* Line 1787 of yacc.c  */
#line 562 "../../ntpd/ntp_parser.y"
    { cfgt.auth.keys = (yyvsp[(2) - (2)].String); }
    break;

  case 66:
/* Line 1787 of yacc.c  */
#line 564 "../../ntpd/ntp_parser.y"
    { cfgt.auth.keysdir = (yyvsp[(2) - (2)].String); }
    break;

  case 67:
/* Line 1787 of yacc.c  */
#line 566 "../../ntpd/ntp_parser.y"
    { cfgt.auth.request_key = (yyvsp[(2) - (2)].Integer); }
    break;

  case 68:
/* Line 1787 of yacc.c  */
#line 568 "../../ntpd/ntp_parser.y"
    { cfgt.auth.revoke = (yyvsp[(2) - (2)].Integer); }
    break;

  case 69:
/* Line 1787 of yacc.c  */
#line 570 "../../ntpd/ntp_parser.y"
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
#line 579 "../../ntpd/ntp_parser.y"
    { cfgt.auth.ntp_signd_socket = (yyvsp[(2) - (2)].String); }
    break;

  case 71:
/* Line 1787 of yacc.c  */
#line 584 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val_fifo) = NULL; }
    break;

  case 72:
/* Line 1787 of yacc.c  */
#line 586 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 73:
/* Line 1787 of yacc.c  */
#line 594 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_sval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].String)); }
    break;

  case 74:
/* Line 1787 of yacc.c  */
#line 596 "../../ntpd/ntp_parser.y"
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
#line 621 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.orphan_cmds, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 81:
/* Line 1787 of yacc.c  */
#line 626 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 82:
/* Line 1787 of yacc.c  */
#line 631 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 83:
/* Line 1787 of yacc.c  */
#line 639 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (2)].Integer), (double)(yyvsp[(2) - (2)].Integer)); }
    break;

  case 84:
/* Line 1787 of yacc.c  */
#line 641 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Double)); }
    break;

  case 85:
/* Line 1787 of yacc.c  */
#line 643 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (2)].Integer), (double)(yyvsp[(2) - (2)].Integer)); }
    break;

  case 96:
/* Line 1787 of yacc.c  */
#line 669 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.stats_list, (yyvsp[(2) - (2)].Int_fifo)); }
    break;

  case 97:
/* Line 1787 of yacc.c  */
#line 671 "../../ntpd/ntp_parser.y"
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
#line 680 "../../ntpd/ntp_parser.y"
    {
			filegen_node *fgn;

			fgn = create_filegen_node((yyvsp[(2) - (3)].Integer), (yyvsp[(3) - (3)].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.filegen_opts, fgn);
		}
    break;

  case 99:
/* Line 1787 of yacc.c  */
#line 690 "../../ntpd/ntp_parser.y"
    {
			(yyval.Int_fifo) = (yyvsp[(1) - (2)].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[(2) - (2)].Integer)));
		}
    break;

  case 100:
/* Line 1787 of yacc.c  */
#line 695 "../../ntpd/ntp_parser.y"
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[(1) - (1)].Integer)));
		}
    break;

  case 109:
/* Line 1787 of yacc.c  */
#line 714 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val_fifo) = NULL; }
    break;

  case 110:
/* Line 1787 of yacc.c  */
#line 716 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 111:
/* Line 1787 of yacc.c  */
#line 724 "../../ntpd/ntp_parser.y"
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
#line 734 "../../ntpd/ntp_parser.y"
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
#line 743 "../../ntpd/ntp_parser.y"
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
#line 758 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[(1) - (1)].Integer)); }
    break;

  case 126:
/* Line 1787 of yacc.c  */
#line 788 "../../ntpd/ntp_parser.y"
    {
			CONCAT_G_FIFOS(cfgt.discard_opts, (yyvsp[(2) - (2)].Attr_val_fifo));
		}
    break;

  case 127:
/* Line 1787 of yacc.c  */
#line 792 "../../ntpd/ntp_parser.y"
    {
			CONCAT_G_FIFOS(cfgt.mru_opts, (yyvsp[(2) - (2)].Attr_val_fifo));
		}
    break;

  case 128:
/* Line 1787 of yacc.c  */
#line 796 "../../ntpd/ntp_parser.y"
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[(2) - (3)].Address_node), NULL, (yyvsp[(3) - (3)].Int_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
    break;

  case 129:
/* Line 1787 of yacc.c  */
#line 804 "../../ntpd/ntp_parser.y"
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[(2) - (5)].Address_node), (yyvsp[(4) - (5)].Address_node), (yyvsp[(5) - (5)].Int_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
    break;

  case 130:
/* Line 1787 of yacc.c  */
#line 812 "../../ntpd/ntp_parser.y"
    {
			restrict_node *rn;

			rn = create_restrict_node(NULL, NULL, (yyvsp[(3) - (3)].Int_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
    break;

  case 131:
/* Line 1787 of yacc.c  */
#line 820 "../../ntpd/ntp_parser.y"
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
#line 835 "../../ntpd/ntp_parser.y"
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
#line 850 "../../ntpd/ntp_parser.y"
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
#line 862 "../../ntpd/ntp_parser.y"
    { (yyval.Int_fifo) = NULL; }
    break;

  case 135:
/* Line 1787 of yacc.c  */
#line 864 "../../ntpd/ntp_parser.y"
    {
			(yyval.Int_fifo) = (yyvsp[(1) - (2)].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[(2) - (2)].Integer)));
		}
    break;

  case 151:
/* Line 1787 of yacc.c  */
#line 890 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 152:
/* Line 1787 of yacc.c  */
#line 895 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 153:
/* Line 1787 of yacc.c  */
#line 903 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 157:
/* Line 1787 of yacc.c  */
#line 914 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 158:
/* Line 1787 of yacc.c  */
#line 919 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 159:
/* Line 1787 of yacc.c  */
#line 927 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 168:
/* Line 1787 of yacc.c  */
#line 947 "../../ntpd/ntp_parser.y"
    {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[(2) - (3)].Address_node), (yyvsp[(3) - (3)].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.fudge, aon);
		}
    break;

  case 169:
/* Line 1787 of yacc.c  */
#line 957 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 170:
/* Line 1787 of yacc.c  */
#line 962 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 171:
/* Line 1787 of yacc.c  */
#line 970 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Double)); }
    break;

  case 172:
/* Line 1787 of yacc.c  */
#line 972 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 173:
/* Line 1787 of yacc.c  */
#line 974 "../../ntpd/ntp_parser.y"
    {
			if ((yyvsp[(2) - (2)].Integer) >= 0 && (yyvsp[(2) - (2)].Integer) <= 16) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror("fudge factor: stratum value not in [0..16], ignored");
			}
		}
    break;

  case 174:
/* Line 1787 of yacc.c  */
#line 983 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_sval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].String)); }
    break;

  case 175:
/* Line 1787 of yacc.c  */
#line 985 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_sval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].String)); }
    break;

  case 182:
/* Line 1787 of yacc.c  */
#line 1006 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.rlimit, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 183:
/* Line 1787 of yacc.c  */
#line 1011 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 184:
/* Line 1787 of yacc.c  */
#line 1016 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 185:
/* Line 1787 of yacc.c  */
#line 1024 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 189:
/* Line 1787 of yacc.c  */
#line 1040 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.enable_opts, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 190:
/* Line 1787 of yacc.c  */
#line 1042 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.disable_opts, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 191:
/* Line 1787 of yacc.c  */
#line 1047 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 192:
/* Line 1787 of yacc.c  */
#line 1052 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 193:
/* Line 1787 of yacc.c  */
#line 1060 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[(1) - (1)].Integer)); }
    break;

  case 194:
/* Line 1787 of yacc.c  */
#line 1062 "../../ntpd/ntp_parser.y"
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

  case 206:
/* Line 1787 of yacc.c  */
#line 1100 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.tinker, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 207:
/* Line 1787 of yacc.c  */
#line 1105 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 208:
/* Line 1787 of yacc.c  */
#line 1110 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 209:
/* Line 1787 of yacc.c  */
#line 1118 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Double)); }
    break;

  case 222:
/* Line 1787 of yacc.c  */
#line 1143 "../../ntpd/ntp_parser.y"
    {
			attr_val *av;

			av = create_attr_dval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Double));
			APPEND_G_FIFO(cfgt.vars, av);
		}
    break;

  case 223:
/* Line 1787 of yacc.c  */
#line 1150 "../../ntpd/ntp_parser.y"
    {
			attr_val *av;

			av = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer));
			APPEND_G_FIFO(cfgt.vars, av);
		}
    break;

  case 224:
/* Line 1787 of yacc.c  */
#line 1157 "../../ntpd/ntp_parser.y"
    {
			attr_val *av;

			av = create_attr_sval((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].String));
			APPEND_G_FIFO(cfgt.vars, av);
		}
    break;

  case 225:
/* Line 1787 of yacc.c  */
#line 1164 "../../ntpd/ntp_parser.y"
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

  case 226:
/* Line 1787 of yacc.c  */
#line 1180 "../../ntpd/ntp_parser.y"
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

  case 227:
/* Line 1787 of yacc.c  */
#line 1199 "../../ntpd/ntp_parser.y"
    { lex_flush_stack(); }
    break;

  case 228:
/* Line 1787 of yacc.c  */
#line 1201 "../../ntpd/ntp_parser.y"
    { /* see drift_parm below for actions */ }
    break;

  case 229:
/* Line 1787 of yacc.c  */
#line 1203 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.logconfig, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 230:
/* Line 1787 of yacc.c  */
#line 1205 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.phone, (yyvsp[(2) - (2)].String_fifo)); }
    break;

  case 231:
/* Line 1787 of yacc.c  */
#line 1207 "../../ntpd/ntp_parser.y"
    { APPEND_G_FIFO(cfgt.setvar, (yyvsp[(2) - (2)].Set_var)); }
    break;

  case 232:
/* Line 1787 of yacc.c  */
#line 1209 "../../ntpd/ntp_parser.y"
    {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[(2) - (3)].Address_node), (yyvsp[(3) - (3)].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.trap, aon);
		}
    break;

  case 233:
/* Line 1787 of yacc.c  */
#line 1216 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.ttl, (yyvsp[(2) - (2)].Attr_val_fifo)); }
    break;

  case 238:
/* Line 1787 of yacc.c  */
#line 1231 "../../ntpd/ntp_parser.y"
    {
#ifndef LEAP_SMEAR
			yyerror("Built without LEAP_SMEAR support.");
#endif
		}
    break;

  case 244:
/* Line 1787 of yacc.c  */
#line 1251 "../../ntpd/ntp_parser.y"
    {
			if (lex_from_file()) {
				attr_val *av;
				av = create_attr_sval(T_Driftfile, (yyvsp[(1) - (1)].String));
				APPEND_G_FIFO(cfgt.vars, av);
			} else {
				YYFREE((yyvsp[(1) - (1)].String));
				yyerror("driftfile remote configuration ignored");
			}
		}
    break;

  case 245:
/* Line 1787 of yacc.c  */
#line 1262 "../../ntpd/ntp_parser.y"
    {
			if (lex_from_file()) {
				attr_val *av;
				av = create_attr_sval(T_Driftfile, (yyvsp[(1) - (2)].String));
				APPEND_G_FIFO(cfgt.vars, av);
				av = create_attr_dval(T_WanderThreshold, (yyvsp[(2) - (2)].Double));
				APPEND_G_FIFO(cfgt.vars, av);
			} else {
				YYFREE((yyvsp[(1) - (2)].String));
				yyerror("driftfile remote configuration ignored");
			}
		}
    break;

  case 246:
/* Line 1787 of yacc.c  */
#line 1275 "../../ntpd/ntp_parser.y"
    {
			if (lex_from_file()) {
				attr_val *av;
				av = create_attr_sval(T_Driftfile, estrdup(""));
				APPEND_G_FIFO(cfgt.vars, av);
			} else {
				yyerror("driftfile remote configuration ignored");
			}
		}
    break;

  case 247:
/* Line 1787 of yacc.c  */
#line 1288 "../../ntpd/ntp_parser.y"
    { (yyval.Set_var) = create_setvar_node((yyvsp[(1) - (4)].String), (yyvsp[(3) - (4)].String), (yyvsp[(4) - (4)].Integer)); }
    break;

  case 249:
/* Line 1787 of yacc.c  */
#line 1294 "../../ntpd/ntp_parser.y"
    { (yyval.Integer) = 0; }
    break;

  case 250:
/* Line 1787 of yacc.c  */
#line 1299 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val_fifo) = NULL; }
    break;

  case 251:
/* Line 1787 of yacc.c  */
#line 1301 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 252:
/* Line 1787 of yacc.c  */
#line 1309 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival((yyvsp[(1) - (2)].Integer), (yyvsp[(2) - (2)].Integer)); }
    break;

  case 253:
/* Line 1787 of yacc.c  */
#line 1311 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val) = create_attr_sval((yyvsp[(1) - (2)].Integer), estrdup((yyvsp[(2) - (2)].Address_node)->address));
			destroy_address_node((yyvsp[(2) - (2)].Address_node));
		}
    break;

  case 254:
/* Line 1787 of yacc.c  */
#line 1319 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 255:
/* Line 1787 of yacc.c  */
#line 1324 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 256:
/* Line 1787 of yacc.c  */
#line 1332 "../../ntpd/ntp_parser.y"
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

  case 257:
/* Line 1787 of yacc.c  */
#line 1357 "../../ntpd/ntp_parser.y"
    {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node((yyvsp[(3) - (3)].Integer), NULL, (yyvsp[(2) - (3)].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
    break;

  case 258:
/* Line 1787 of yacc.c  */
#line 1364 "../../ntpd/ntp_parser.y"
    {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node(0, (yyvsp[(3) - (3)].String), (yyvsp[(2) - (3)].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
    break;

  case 268:
/* Line 1787 of yacc.c  */
#line 1392 "../../ntpd/ntp_parser.y"
    { CONCAT_G_FIFOS(cfgt.reset_counters, (yyvsp[(2) - (2)].Int_fifo)); }
    break;

  case 269:
/* Line 1787 of yacc.c  */
#line 1397 "../../ntpd/ntp_parser.y"
    {
			(yyval.Int_fifo) = (yyvsp[(1) - (2)].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[(2) - (2)].Integer)));
		}
    break;

  case 270:
/* Line 1787 of yacc.c  */
#line 1402 "../../ntpd/ntp_parser.y"
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[(1) - (1)].Integer)));
		}
    break;

  case 278:
/* Line 1787 of yacc.c  */
#line 1426 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[(2) - (2)].Integer)));
		}
    break;

  case 279:
/* Line 1787 of yacc.c  */
#line 1431 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[(1) - (1)].Integer)));
		}
    break;

  case 280:
/* Line 1787 of yacc.c  */
#line 1439 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (2)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (2)].Attr_val));
		}
    break;

  case 281:
/* Line 1787 of yacc.c  */
#line 1444 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (1)].Attr_val));
		}
    break;

  case 282:
/* Line 1787 of yacc.c  */
#line 1452 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_ival('i', (yyvsp[(1) - (1)].Integer)); }
    break;

  case 284:
/* Line 1787 of yacc.c  */
#line 1458 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_rangeval('-', (yyvsp[(2) - (5)].Integer), (yyvsp[(4) - (5)].Integer)); }
    break;

  case 285:
/* Line 1787 of yacc.c  */
#line 1463 "../../ntpd/ntp_parser.y"
    {
			(yyval.String_fifo) = (yyvsp[(1) - (2)].String_fifo);
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[(2) - (2)].String)));
		}
    break;

  case 286:
/* Line 1787 of yacc.c  */
#line 1468 "../../ntpd/ntp_parser.y"
    {
			(yyval.String_fifo) = NULL;
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[(1) - (1)].String)));
		}
    break;

  case 287:
/* Line 1787 of yacc.c  */
#line 1476 "../../ntpd/ntp_parser.y"
    {
			(yyval.Address_fifo) = (yyvsp[(1) - (2)].Address_fifo);
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[(2) - (2)].Address_node));
		}
    break;

  case 288:
/* Line 1787 of yacc.c  */
#line 1481 "../../ntpd/ntp_parser.y"
    {
			(yyval.Address_fifo) = NULL;
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[(1) - (1)].Address_node));
		}
    break;

  case 289:
/* Line 1787 of yacc.c  */
#line 1489 "../../ntpd/ntp_parser.y"
    {
			if ((yyvsp[(1) - (1)].Integer) != 0 && (yyvsp[(1) - (1)].Integer) != 1) {
				yyerror("Integer value is not boolean (0 or 1). Assuming 1");
				(yyval.Integer) = 1;
			} else {
				(yyval.Integer) = (yyvsp[(1) - (1)].Integer);
			}
		}
    break;

  case 290:
/* Line 1787 of yacc.c  */
#line 1497 "../../ntpd/ntp_parser.y"
    { (yyval.Integer) = 1; }
    break;

  case 291:
/* Line 1787 of yacc.c  */
#line 1498 "../../ntpd/ntp_parser.y"
    { (yyval.Integer) = 0; }
    break;

  case 292:
/* Line 1787 of yacc.c  */
#line 1502 "../../ntpd/ntp_parser.y"
    { (yyval.Double) = (double)(yyvsp[(1) - (1)].Integer); }
    break;

  case 294:
/* Line 1787 of yacc.c  */
#line 1513 "../../ntpd/ntp_parser.y"
    {
			sim_node *sn;

			sn =  create_sim_node((yyvsp[(3) - (5)].Attr_val_fifo), (yyvsp[(4) - (5)].Sim_server_fifo));
			APPEND_G_FIFO(cfgt.sim_details, sn);

			/* Revert from ; to \n for end-of-command */
			old_config_style = 1;
		}
    break;

  case 295:
/* Line 1787 of yacc.c  */
#line 1530 "../../ntpd/ntp_parser.y"
    { old_config_style = 0; }
    break;

  case 296:
/* Line 1787 of yacc.c  */
#line 1535 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (3)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (3)].Attr_val));
		}
    break;

  case 297:
/* Line 1787 of yacc.c  */
#line 1540 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (2)].Attr_val));
		}
    break;

  case 298:
/* Line 1787 of yacc.c  */
#line 1548 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (3)].Integer), (yyvsp[(3) - (3)].Double)); }
    break;

  case 301:
/* Line 1787 of yacc.c  */
#line 1558 "../../ntpd/ntp_parser.y"
    {
			(yyval.Sim_server_fifo) = (yyvsp[(1) - (2)].Sim_server_fifo);
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[(2) - (2)].Sim_server));
		}
    break;

  case 302:
/* Line 1787 of yacc.c  */
#line 1563 "../../ntpd/ntp_parser.y"
    {
			(yyval.Sim_server_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[(1) - (1)].Sim_server));
		}
    break;

  case 303:
/* Line 1787 of yacc.c  */
#line 1571 "../../ntpd/ntp_parser.y"
    { (yyval.Sim_server) = ONLY_SIM(create_sim_server((yyvsp[(1) - (5)].Address_node), (yyvsp[(3) - (5)].Double), (yyvsp[(4) - (5)].Sim_script_fifo))); }
    break;

  case 304:
/* Line 1787 of yacc.c  */
#line 1576 "../../ntpd/ntp_parser.y"
    { (yyval.Double) = (yyvsp[(3) - (4)].Double); }
    break;

  case 305:
/* Line 1787 of yacc.c  */
#line 1581 "../../ntpd/ntp_parser.y"
    { (yyval.Address_node) = (yyvsp[(3) - (3)].Address_node); }
    break;

  case 306:
/* Line 1787 of yacc.c  */
#line 1586 "../../ntpd/ntp_parser.y"
    {
			(yyval.Sim_script_fifo) = (yyvsp[(1) - (2)].Sim_script_fifo);
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[(2) - (2)].Sim_script));
		}
    break;

  case 307:
/* Line 1787 of yacc.c  */
#line 1591 "../../ntpd/ntp_parser.y"
    {
			(yyval.Sim_script_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[(1) - (1)].Sim_script));
		}
    break;

  case 308:
/* Line 1787 of yacc.c  */
#line 1599 "../../ntpd/ntp_parser.y"
    { (yyval.Sim_script) = ONLY_SIM(create_sim_script_info((yyvsp[(3) - (6)].Double), (yyvsp[(5) - (6)].Attr_val_fifo))); }
    break;

  case 309:
/* Line 1787 of yacc.c  */
#line 1604 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = (yyvsp[(1) - (3)].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(2) - (3)].Attr_val));
		}
    break;

  case 310:
/* Line 1787 of yacc.c  */
#line 1609 "../../ntpd/ntp_parser.y"
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[(1) - (2)].Attr_val));
		}
    break;

  case 311:
/* Line 1787 of yacc.c  */
#line 1617 "../../ntpd/ntp_parser.y"
    { (yyval.Attr_val) = create_attr_dval((yyvsp[(1) - (3)].Integer), (yyvsp[(3) - (3)].Double)); }
    break;


/* Line 1787 of yacc.c  */
#line 3625 "ntp_parser.c"
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
#line 1628 "../../ntpd/ntp_parser.y"


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

