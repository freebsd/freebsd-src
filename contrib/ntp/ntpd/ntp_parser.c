/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

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
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 11 "../../ntpd/ntp_parser.y" /* yacc.c:339  */

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
  #include "ntp_calendar.h"

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

#line 101 "../../ntpd/ntp_parser.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
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
#ifndef YY_YY__NTPD_NTP_PARSER_H_INCLUDED
# define YY_YY__NTPD_NTP_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    T_Abbrev = 258,
    T_Age = 259,
    T_All = 260,
    T_Allan = 261,
    T_Allpeers = 262,
    T_Auth = 263,
    T_Autokey = 264,
    T_Automax = 265,
    T_Average = 266,
    T_Basedate = 267,
    T_Bclient = 268,
    T_Bcpollbstep = 269,
    T_Beacon = 270,
    T_Broadcast = 271,
    T_Broadcastclient = 272,
    T_Broadcastdelay = 273,
    T_Burst = 274,
    T_Calibrate = 275,
    T_Ceiling = 276,
    T_Checkhash = 277,
    T_Clockstats = 278,
    T_Cohort = 279,
    T_ControlKey = 280,
    T_Crypto = 281,
    T_Cryptostats = 282,
    T_Ctl = 283,
    T_Day = 284,
    T_Default = 285,
    T_Digest = 286,
    T_Disable = 287,
    T_Discard = 288,
    T_Dispersion = 289,
    T_Double = 290,
    T_Driftfile = 291,
    T_Drop = 292,
    T_Dscp = 293,
    T_Ellipsis = 294,
    T_Enable = 295,
    T_End = 296,
    T_Epeer = 297,
    T_False = 298,
    T_File = 299,
    T_Filegen = 300,
    T_Filenum = 301,
    T_Flag1 = 302,
    T_Flag2 = 303,
    T_Flag3 = 304,
    T_Flag4 = 305,
    T_Flake = 306,
    T_Floor = 307,
    T_Freq = 308,
    T_Fudge = 309,
    T_Fuzz = 310,
    T_Host = 311,
    T_Huffpuff = 312,
    T_Iburst = 313,
    T_Ident = 314,
    T_Ignore = 315,
    T_Ignorehash = 316,
    T_Incalloc = 317,
    T_Incmem = 318,
    T_Initalloc = 319,
    T_Initmem = 320,
    T_Includefile = 321,
    T_Integer = 322,
    T_Interface = 323,
    T_Intrange = 324,
    T_Io = 325,
    T_Ippeerlimit = 326,
    T_Ipv4 = 327,
    T_Ipv4_flag = 328,
    T_Ipv6 = 329,
    T_Ipv6_flag = 330,
    T_Kernel = 331,
    T_Key = 332,
    T_Keys = 333,
    T_Keysdir = 334,
    T_Kod = 335,
    T_Leapfile = 336,
    T_Leapsmearinterval = 337,
    T_Limited = 338,
    T_Link = 339,
    T_Listen = 340,
    T_Logconfig = 341,
    T_Logfile = 342,
    T_Loopstats = 343,
    T_Lowpriotrap = 344,
    T_Manycastclient = 345,
    T_Manycastserver = 346,
    T_Mask = 347,
    T_Maxage = 348,
    T_Maxclock = 349,
    T_Maxdepth = 350,
    T_Maxdist = 351,
    T_Maxmem = 352,
    T_Maxpoll = 353,
    T_Mdnstries = 354,
    T_Mem = 355,
    T_Memlock = 356,
    T_Minclock = 357,
    T_Mindepth = 358,
    T_Mindist = 359,
    T_Minimum = 360,
    T_Minjitter = 361,
    T_Minpoll = 362,
    T_Minsane = 363,
    T_Mode = 364,
    T_Mode7 = 365,
    T_Monitor = 366,
    T_Month = 367,
    T_Mru = 368,
    T_Mssntp = 369,
    T_Multicastclient = 370,
    T_Nic = 371,
    T_Nolink = 372,
    T_Nomodify = 373,
    T_Nomrulist = 374,
    T_None = 375,
    T_Nonvolatile = 376,
    T_Noepeer = 377,
    T_Nopeer = 378,
    T_Noquery = 379,
    T_Noselect = 380,
    T_Noserve = 381,
    T_Notrap = 382,
    T_Notrust = 383,
    T_Ntp = 384,
    T_Ntpport = 385,
    T_NtpSignDsocket = 386,
    T_Orphan = 387,
    T_Orphanwait = 388,
    T_PCEdigest = 389,
    T_Panic = 390,
    T_Peer = 391,
    T_Peerstats = 392,
    T_Phone = 393,
    T_Pid = 394,
    T_Pidfile = 395,
    T_Poll = 396,
    T_PollSkewList = 397,
    T_Pool = 398,
    T_Port = 399,
    T_Preempt = 400,
    T_Prefer = 401,
    T_Protostats = 402,
    T_Pw = 403,
    T_Randfile = 404,
    T_Rawstats = 405,
    T_Refid = 406,
    T_Requestkey = 407,
    T_Reset = 408,
    T_Restrict = 409,
    T_Revoke = 410,
    T_Rlimit = 411,
    T_Saveconfigdir = 412,
    T_Server = 413,
    T_Serverresponse = 414,
    T_ServerresponseFuzz = 415,
    T_Setvar = 416,
    T_Source = 417,
    T_Stacksize = 418,
    T_Statistics = 419,
    T_Stats = 420,
    T_Statsdir = 421,
    T_Step = 422,
    T_Stepback = 423,
    T_Stepfwd = 424,
    T_Stepout = 425,
    T_Stratum = 426,
    T_String = 427,
    T_Sys = 428,
    T_Sysstats = 429,
    T_Tick = 430,
    T_Time1 = 431,
    T_Time2 = 432,
    T_Timer = 433,
    T_Timingstats = 434,
    T_Tinker = 435,
    T_Tos = 436,
    T_Trap = 437,
    T_True = 438,
    T_Trustedkey = 439,
    T_Ttl = 440,
    T_Type = 441,
    T_U_int = 442,
    T_UEcrypto = 443,
    T_UEcryptonak = 444,
    T_UEdigest = 445,
    T_Unconfig = 446,
    T_Unpeer = 447,
    T_Version = 448,
    T_WanderThreshold = 449,
    T_Week = 450,
    T_Wildcard = 451,
    T_Xleave = 452,
    T_Xmtnonce = 453,
    T_Year = 454,
    T_Flag = 455,
    T_EOC = 456,
    T_Simulate = 457,
    T_Beep_Delay = 458,
    T_Sim_Duration = 459,
    T_Server_Offset = 460,
    T_Duration = 461,
    T_Freq_Offset = 462,
    T_Wander = 463,
    T_Jitter = 464,
    T_Prop_Delay = 465,
    T_Proc_Delay = 466
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
#define T_Basedate 267
#define T_Bclient 268
#define T_Bcpollbstep 269
#define T_Beacon 270
#define T_Broadcast 271
#define T_Broadcastclient 272
#define T_Broadcastdelay 273
#define T_Burst 274
#define T_Calibrate 275
#define T_Ceiling 276
#define T_Checkhash 277
#define T_Clockstats 278
#define T_Cohort 279
#define T_ControlKey 280
#define T_Crypto 281
#define T_Cryptostats 282
#define T_Ctl 283
#define T_Day 284
#define T_Default 285
#define T_Digest 286
#define T_Disable 287
#define T_Discard 288
#define T_Dispersion 289
#define T_Double 290
#define T_Driftfile 291
#define T_Drop 292
#define T_Dscp 293
#define T_Ellipsis 294
#define T_Enable 295
#define T_End 296
#define T_Epeer 297
#define T_False 298
#define T_File 299
#define T_Filegen 300
#define T_Filenum 301
#define T_Flag1 302
#define T_Flag2 303
#define T_Flag3 304
#define T_Flag4 305
#define T_Flake 306
#define T_Floor 307
#define T_Freq 308
#define T_Fudge 309
#define T_Fuzz 310
#define T_Host 311
#define T_Huffpuff 312
#define T_Iburst 313
#define T_Ident 314
#define T_Ignore 315
#define T_Ignorehash 316
#define T_Incalloc 317
#define T_Incmem 318
#define T_Initalloc 319
#define T_Initmem 320
#define T_Includefile 321
#define T_Integer 322
#define T_Interface 323
#define T_Intrange 324
#define T_Io 325
#define T_Ippeerlimit 326
#define T_Ipv4 327
#define T_Ipv4_flag 328
#define T_Ipv6 329
#define T_Ipv6_flag 330
#define T_Kernel 331
#define T_Key 332
#define T_Keys 333
#define T_Keysdir 334
#define T_Kod 335
#define T_Leapfile 336
#define T_Leapsmearinterval 337
#define T_Limited 338
#define T_Link 339
#define T_Listen 340
#define T_Logconfig 341
#define T_Logfile 342
#define T_Loopstats 343
#define T_Lowpriotrap 344
#define T_Manycastclient 345
#define T_Manycastserver 346
#define T_Mask 347
#define T_Maxage 348
#define T_Maxclock 349
#define T_Maxdepth 350
#define T_Maxdist 351
#define T_Maxmem 352
#define T_Maxpoll 353
#define T_Mdnstries 354
#define T_Mem 355
#define T_Memlock 356
#define T_Minclock 357
#define T_Mindepth 358
#define T_Mindist 359
#define T_Minimum 360
#define T_Minjitter 361
#define T_Minpoll 362
#define T_Minsane 363
#define T_Mode 364
#define T_Mode7 365
#define T_Monitor 366
#define T_Month 367
#define T_Mru 368
#define T_Mssntp 369
#define T_Multicastclient 370
#define T_Nic 371
#define T_Nolink 372
#define T_Nomodify 373
#define T_Nomrulist 374
#define T_None 375
#define T_Nonvolatile 376
#define T_Noepeer 377
#define T_Nopeer 378
#define T_Noquery 379
#define T_Noselect 380
#define T_Noserve 381
#define T_Notrap 382
#define T_Notrust 383
#define T_Ntp 384
#define T_Ntpport 385
#define T_NtpSignDsocket 386
#define T_Orphan 387
#define T_Orphanwait 388
#define T_PCEdigest 389
#define T_Panic 390
#define T_Peer 391
#define T_Peerstats 392
#define T_Phone 393
#define T_Pid 394
#define T_Pidfile 395
#define T_Poll 396
#define T_PollSkewList 397
#define T_Pool 398
#define T_Port 399
#define T_Preempt 400
#define T_Prefer 401
#define T_Protostats 402
#define T_Pw 403
#define T_Randfile 404
#define T_Rawstats 405
#define T_Refid 406
#define T_Requestkey 407
#define T_Reset 408
#define T_Restrict 409
#define T_Revoke 410
#define T_Rlimit 411
#define T_Saveconfigdir 412
#define T_Server 413
#define T_Serverresponse 414
#define T_ServerresponseFuzz 415
#define T_Setvar 416
#define T_Source 417
#define T_Stacksize 418
#define T_Statistics 419
#define T_Stats 420
#define T_Statsdir 421
#define T_Step 422
#define T_Stepback 423
#define T_Stepfwd 424
#define T_Stepout 425
#define T_Stratum 426
#define T_String 427
#define T_Sys 428
#define T_Sysstats 429
#define T_Tick 430
#define T_Time1 431
#define T_Time2 432
#define T_Timer 433
#define T_Timingstats 434
#define T_Tinker 435
#define T_Tos 436
#define T_Trap 437
#define T_True 438
#define T_Trustedkey 439
#define T_Ttl 440
#define T_Type 441
#define T_U_int 442
#define T_UEcrypto 443
#define T_UEcryptonak 444
#define T_UEdigest 445
#define T_Unconfig 446
#define T_Unpeer 447
#define T_Version 448
#define T_WanderThreshold 449
#define T_Week 450
#define T_Wildcard 451
#define T_Xleave 452
#define T_Xmtnonce 453
#define T_Year 454
#define T_Flag 455
#define T_EOC 456
#define T_Simulate 457
#define T_Beep_Delay 458
#define T_Sim_Duration 459
#define T_Server_Offset 460
#define T_Duration 461
#define T_Freq_Offset 462
#define T_Wander 463
#define T_Jitter 464
#define T_Prop_Delay 465
#define T_Proc_Delay 466

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 52 "../../ntpd/ntp_parser.y" /* yacc.c:355  */

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

#line 582 "../../ntpd/ntp_parser.c" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY__NTPD_NTP_PARSER_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 599 "../../ntpd/ntp_parser.c" /* yacc.c:358  */

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
#else
typedef signed char yytype_int8;
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
# elif ! defined YYSIZE_T
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

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
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
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
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
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
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
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

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
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  219
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   740

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  218
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  111
/* YYNRULES -- Number of rules.  */
#define YYNRULES  336
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  453

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   466

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     214,   215,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   213,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   216,   212,   217,     2,     2,     2,     2,
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
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   391,   391,   395,   396,   397,   412,   413,   414,   415,
     416,   417,   418,   419,   420,   421,   422,   423,   424,   425,
     433,   443,   444,   445,   446,   447,   451,   452,   457,   462,
     464,   470,   471,   479,   480,   481,   485,   490,   491,   492,
     493,   494,   495,   496,   497,   498,   502,   504,   509,   510,
     511,   512,   513,   514,   518,   523,   532,   542,   543,   553,
     555,   557,   559,   570,   577,   579,   584,   586,   588,   590,
     592,   602,   608,   609,   617,   619,   631,   632,   633,   634,
     635,   644,   649,   654,   662,   664,   666,   668,   673,   674,
     675,   676,   677,   678,   679,   680,   681,   685,   686,   695,
     697,   706,   716,   721,   729,   730,   731,   732,   733,   734,
     735,   736,   741,   742,   750,   760,   769,   784,   789,   790,
     794,   795,   799,   800,   801,   802,   803,   804,   805,   814,
     818,   822,   830,   838,   846,   861,   876,   889,   890,   910,
     911,   919,   930,   931,   932,   933,   934,   935,   936,   937,
     938,   939,   940,   941,   942,   943,   944,   945,   946,   950,
     955,   963,   968,   969,   970,   974,   979,   987,   992,   993,
     994,   995,   996,   997,   998,   999,  1007,  1017,  1022,  1030,
    1032,  1034,  1043,  1045,  1050,  1051,  1052,  1056,  1057,  1058,
    1059,  1067,  1072,  1077,  1085,  1090,  1091,  1092,  1101,  1103,
    1108,  1113,  1121,  1123,  1140,  1141,  1142,  1143,  1144,  1145,
    1149,  1150,  1151,  1152,  1153,  1154,  1162,  1167,  1172,  1180,
    1185,  1186,  1187,  1188,  1189,  1190,  1191,  1192,  1193,  1194,
    1203,  1204,  1205,  1212,  1219,  1226,  1242,  1261,  1269,  1271,
    1273,  1275,  1277,  1279,  1281,  1288,  1293,  1294,  1295,  1299,
    1303,  1312,  1314,  1317,  1321,  1325,  1326,  1327,  1331,  1342,
    1360,  1373,  1374,  1379,  1405,  1406,  1411,  1416,  1418,  1423,
    1424,  1432,  1434,  1442,  1447,  1455,  1480,  1487,  1497,  1498,
    1502,  1503,  1504,  1505,  1509,  1510,  1511,  1515,  1520,  1525,
    1533,  1534,  1535,  1536,  1537,  1538,  1539,  1549,  1554,  1562,
    1567,  1575,  1577,  1581,  1586,  1591,  1599,  1604,  1612,  1621,
    1622,  1626,  1627,  1631,  1639,  1657,  1661,  1666,  1674,  1679,
    1680,  1684,  1689,  1697,  1702,  1707,  1712,  1717,  1725,  1730,
    1735,  1743,  1748,  1749,  1750,  1751,  1752
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "T_Abbrev", "T_Age", "T_All", "T_Allan",
  "T_Allpeers", "T_Auth", "T_Autokey", "T_Automax", "T_Average",
  "T_Basedate", "T_Bclient", "T_Bcpollbstep", "T_Beacon", "T_Broadcast",
  "T_Broadcastclient", "T_Broadcastdelay", "T_Burst", "T_Calibrate",
  "T_Ceiling", "T_Checkhash", "T_Clockstats", "T_Cohort", "T_ControlKey",
  "T_Crypto", "T_Cryptostats", "T_Ctl", "T_Day", "T_Default", "T_Digest",
  "T_Disable", "T_Discard", "T_Dispersion", "T_Double", "T_Driftfile",
  "T_Drop", "T_Dscp", "T_Ellipsis", "T_Enable", "T_End", "T_Epeer",
  "T_False", "T_File", "T_Filegen", "T_Filenum", "T_Flag1", "T_Flag2",
  "T_Flag3", "T_Flag4", "T_Flake", "T_Floor", "T_Freq", "T_Fudge",
  "T_Fuzz", "T_Host", "T_Huffpuff", "T_Iburst", "T_Ident", "T_Ignore",
  "T_Ignorehash", "T_Incalloc", "T_Incmem", "T_Initalloc", "T_Initmem",
  "T_Includefile", "T_Integer", "T_Interface", "T_Intrange", "T_Io",
  "T_Ippeerlimit", "T_Ipv4", "T_Ipv4_flag", "T_Ipv6", "T_Ipv6_flag",
  "T_Kernel", "T_Key", "T_Keys", "T_Keysdir", "T_Kod", "T_Leapfile",
  "T_Leapsmearinterval", "T_Limited", "T_Link", "T_Listen", "T_Logconfig",
  "T_Logfile", "T_Loopstats", "T_Lowpriotrap", "T_Manycastclient",
  "T_Manycastserver", "T_Mask", "T_Maxage", "T_Maxclock", "T_Maxdepth",
  "T_Maxdist", "T_Maxmem", "T_Maxpoll", "T_Mdnstries", "T_Mem",
  "T_Memlock", "T_Minclock", "T_Mindepth", "T_Mindist", "T_Minimum",
  "T_Minjitter", "T_Minpoll", "T_Minsane", "T_Mode", "T_Mode7",
  "T_Monitor", "T_Month", "T_Mru", "T_Mssntp", "T_Multicastclient",
  "T_Nic", "T_Nolink", "T_Nomodify", "T_Nomrulist", "T_None",
  "T_Nonvolatile", "T_Noepeer", "T_Nopeer", "T_Noquery", "T_Noselect",
  "T_Noserve", "T_Notrap", "T_Notrust", "T_Ntp", "T_Ntpport",
  "T_NtpSignDsocket", "T_Orphan", "T_Orphanwait", "T_PCEdigest", "T_Panic",
  "T_Peer", "T_Peerstats", "T_Phone", "T_Pid", "T_Pidfile", "T_Poll",
  "T_PollSkewList", "T_Pool", "T_Port", "T_Preempt", "T_Prefer",
  "T_Protostats", "T_Pw", "T_Randfile", "T_Rawstats", "T_Refid",
  "T_Requestkey", "T_Reset", "T_Restrict", "T_Revoke", "T_Rlimit",
  "T_Saveconfigdir", "T_Server", "T_Serverresponse",
  "T_ServerresponseFuzz", "T_Setvar", "T_Source", "T_Stacksize",
  "T_Statistics", "T_Stats", "T_Statsdir", "T_Step", "T_Stepback",
  "T_Stepfwd", "T_Stepout", "T_Stratum", "T_String", "T_Sys", "T_Sysstats",
  "T_Tick", "T_Time1", "T_Time2", "T_Timer", "T_Timingstats", "T_Tinker",
  "T_Tos", "T_Trap", "T_True", "T_Trustedkey", "T_Ttl", "T_Type",
  "T_U_int", "T_UEcrypto", "T_UEcryptonak", "T_UEdigest", "T_Unconfig",
  "T_Unpeer", "T_Version", "T_WanderThreshold", "T_Week", "T_Wildcard",
  "T_Xleave", "T_Xmtnonce", "T_Year", "T_Flag", "T_EOC", "T_Simulate",
  "T_Beep_Delay", "T_Sim_Duration", "T_Server_Offset", "T_Duration",
  "T_Freq_Offset", "T_Wander", "T_Jitter", "T_Prop_Delay", "T_Proc_Delay",
  "'|'", "'='", "'('", "')'", "'{'", "'}'", "$accept", "configuration",
  "command_list", "command", "server_command", "client_type", "address",
  "ip_address", "address_fam", "option_list", "option", "option_flag",
  "option_flag_keyword", "option_int", "option_int_keyword", "option_str",
  "option_str_keyword", "unpeer_command", "unpeer_keyword",
  "other_mode_command", "authentication_command", "crypto_command_list",
  "crypto_command", "crypto_str_keyword", "orphan_mode_command",
  "tos_option_list", "tos_option", "tos_option_int_keyword",
  "tos_option_dbl_keyword", "monitoring_command", "stats_list", "stat",
  "filegen_option_list", "filegen_option", "link_nolink", "enable_disable",
  "filegen_type", "access_control_command", "res_ippeerlimit",
  "ac_flag_list", "access_control_flag", "discard_option_list",
  "discard_option", "discard_option_keyword", "mru_option_list",
  "mru_option", "mru_option_keyword", "fudge_command", "fudge_factor_list",
  "fudge_factor", "fudge_factor_dbl_keyword", "fudge_factor_bool_keyword",
  "rlimit_command", "rlimit_option_list", "rlimit_option",
  "rlimit_option_keyword", "system_option_command", "system_option_list",
  "system_option", "system_option_flag_keyword",
  "system_option_local_flag_keyword", "tinker_command",
  "tinker_option_list", "tinker_option", "tinker_option_keyword",
  "miscellaneous_command", "misc_cmd_dbl_keyword", "misc_cmd_int_keyword",
  "opt_hash_check", "misc_cmd_str_keyword", "misc_cmd_str_lcl_keyword",
  "drift_parm", "pollskew_list", "pollskew_spec", "pollskew_cycle",
  "variable_assign", "t_default_or_zero", "trap_option_list",
  "trap_option", "log_config_list", "log_config_command",
  "interface_command", "interface_nic", "nic_rule_class",
  "nic_rule_action", "reset_command", "counter_set_list",
  "counter_set_keyword", "integer_list", "integer_list_range",
  "integer_list_range_elt", "integer_range", "string_list", "address_list",
  "boolean", "number", "basedate", "simulate_command", "sim_conf_start",
  "sim_init_statement_list", "sim_init_statement", "sim_init_keyword",
  "sim_server_list", "sim_server", "sim_server_offset", "sim_server_name",
  "sim_act_list", "sim_act", "sim_act_stmt_list", "sim_act_stmt",
  "sim_act_keyword", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
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
     445,   446,   447,   448,   449,   450,   451,   452,   453,   454,
     455,   456,   457,   458,   459,   460,   461,   462,   463,   464,
     465,   466,   124,    61,    40,    41,   123,   125
};
# endif

#define YYPACT_NINF -261

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-261)))

#define YYTABLE_NINF -7

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      19,  -163,   -36,  -261,  -261,  -261,   -25,  -261,   326,    77,
    -125,  -261,   326,  -261,    16,   -45,  -261,  -119,  -261,  -103,
    -100,   -98,  -261,   -95,  -261,  -261,   -45,    13,   238,   -45,
    -261,  -261,   -88,  -261,   -86,  -261,  -261,  -261,    22,   109,
      -8,    23,   -38,  -261,  -261,   -81,    16,   -80,  -261,   443,
     607,   -73,   -58,    35,  -261,  -261,  -261,   103,   229,   -90,
    -261,   -45,  -261,   -45,  -261,  -261,  -261,  -261,  -261,  -261,
    -261,  -261,  -261,  -261,    -9,    45,   -53,   -51,  -261,   -19,
    -261,  -261,  -102,  -261,  -261,  -261,    82,  -261,  -261,  -261,
    -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,   326,
    -261,  -261,  -261,  -261,  -261,  -261,    77,  -261,    55,    91,
    -261,   326,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,
    -261,  -261,  -261,  -261,    46,  -261,   -42,   407,  -261,  -261,
     -11,  -261,   -95,  -261,  -261,   -45,  -261,  -261,  -261,  -261,
    -261,  -261,  -261,  -261,  -261,   238,  -261,    64,   -45,  -261,
    -261,   -28,   -13,  -261,  -261,  -261,  -261,  -261,  -261,  -261,
    -261,   109,  -261,    74,   118,   119,    74,   -31,  -261,  -261,
    -261,  -261,   -38,  -261,    93,   -55,  -261,    16,  -261,  -261,
    -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,
     443,  -261,    -9,    -7,  -261,  -261,  -261,   -40,  -261,  -261,
    -261,  -261,  -261,  -261,  -261,  -261,   607,  -261,   100,    -9,
    -261,  -261,  -261,   101,   -58,  -261,  -261,  -261,   102,  -261,
     -23,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,
    -261,  -261,  -261,     9,  -170,  -261,  -261,  -261,  -261,  -261,
     122,  -261,    -2,  -261,  -261,  -261,  -261,   107,    14,  -261,
    -261,  -261,  -261,  -261,    20,   126,  -261,  -261,    46,  -261,
      -9,   -40,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,
    -261,  -261,  -261,  -261,  -261,   129,  -261,   139,  -261,    74,
      74,  -261,   -73,  -261,  -261,  -261,    36,  -261,  -261,  -261,
    -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,   -62,   168,
    -261,  -261,  -261,   410,  -261,  -261,  -261,  -261,  -261,  -261,
    -261,  -261,   -96,    17,     6,  -261,  -261,  -261,  -261,    61,
    -261,  -261,     3,  -261,  -261,  -261,  -261,  -261,  -261,  -261,
    -261,  -261,    24,  -261,   534,  -261,  -261,   534,    74,   534,
     204,   -73,   173,  -261,   174,  -261,  -261,  -261,  -261,  -261,
    -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,
    -261,  -261,  -261,  -261,  -261,   -63,  -261,    70,    31,    47,
    -146,  -261,    33,  -261,    -9,  -261,  -261,  -261,  -261,  -261,
    -261,  -261,  -261,  -261,   183,  -261,  -261,  -261,  -261,  -261,
    -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,
    -261,   196,  -261,  -261,   534,   534,  -261,  -261,  -261,  -261,
    -261,    37,  -261,  -261,  -261,   -45,  -261,  -261,  -261,    48,
    -261,  -261,  -261,   534,  -261,  -261,    43,    51,    -9,    50,
    -193,  -261,    57,    -9,  -261,  -261,  -261,    52,     5,  -261,
    -261,  -261,  -261,  -261,    18,    58,    53,  -261,    63,  -261,
      -9,  -261,  -261
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       0,     0,     0,    24,    59,   246,     0,    72,     0,     0,
     260,   249,     0,   238,     0,     0,   254,     0,   278,     0,
       0,     0,   250,     0,   255,    25,     0,     0,     0,     0,
     279,   247,     0,    23,     0,   256,   261,    22,     0,     0,
       0,     0,     0,   257,    21,     0,     0,     0,   248,     0,
       0,     0,     0,     0,    57,    58,   315,     0,     2,     0,
       7,     0,     8,     0,     9,    10,    13,    11,    12,    14,
      15,    16,    17,    18,     0,     0,     0,     0,   230,     0,
     231,    19,     0,     5,    63,    64,    65,   204,   205,   206,
     207,   210,   208,   209,   211,   212,   213,   214,   215,   199,
     201,   202,   203,   162,   163,   164,   129,   160,     0,   258,
     239,   198,   104,   105,   106,   107,   111,   108,   109,   110,
     112,    29,    30,    28,     0,    26,     0,     6,    66,    67,
     253,   275,   240,   274,   307,    60,    62,   168,   169,   170,
     171,   172,   173,   174,   175,   130,   166,     0,    61,    71,
     305,   241,   242,    68,   290,   291,   292,   293,   294,   295,
     296,   287,   289,   137,    29,    30,   137,   137,    69,   197,
     195,   196,   191,   193,     0,     0,   243,    99,   103,   100,
     220,   221,   222,   223,   224,   225,   226,   227,   228,   229,
     216,   218,     0,     0,    88,    89,    90,     0,    91,    92,
      98,    93,    97,    94,    95,    96,    81,    83,     0,     0,
      87,   269,   301,     0,    70,   300,   302,   298,   245,     1,
       0,     4,    31,    56,   312,   311,   232,   233,   234,   235,
     286,   285,   284,     0,     0,    80,    76,    77,    78,    79,
       0,    73,     0,   200,   159,   161,   259,   101,     0,   187,
     188,   189,   190,   186,     0,     0,   184,   185,   176,   178,
       0,     0,    27,   236,   252,   251,   237,   273,   306,   165,
     167,   304,   265,   264,   262,     0,   288,     0,   139,   137,
     137,   139,     0,   139,   192,   194,     0,   102,   217,   219,
     313,   310,   308,   309,    86,    82,    84,    85,   244,     0,
     299,   297,     3,    20,   280,   281,   282,   277,   283,   276,
     319,   320,     0,     0,     0,    75,    74,   121,   120,     0,
     118,   119,     0,   113,   116,   117,   182,   183,   181,   177,
     179,   180,     0,   138,   133,   139,   139,   136,   137,   131,
     268,     0,     0,   270,     0,    37,    38,    39,    55,    48,
      50,    49,    52,    40,    41,    42,    43,    51,    53,    44,
      45,    32,    33,    36,    34,     0,    35,     0,     0,     0,
       0,   322,     0,   317,     0,   114,   128,   124,   126,   122,
     123,   125,   127,   115,     0,   142,   143,   144,   145,   146,
     147,   148,   150,   151,   149,   152,   153,   154,   155,   156,
     157,     0,   158,   140,   134,   135,   139,   267,   266,   272,
     271,     0,    46,    47,    54,     0,   316,   314,   321,     0,
     318,   263,   141,   132,   303,   325,     0,     0,     0,     0,
       0,   327,     0,     0,   323,   326,   324,     0,     0,   332,
     333,   334,   335,   336,     0,     0,     0,   328,     0,   330,
       0,   329,   331
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -261,  -261,  -261,   -48,  -261,  -261,   -15,   -50,  -261,  -261,
    -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,
    -261,  -261,  -261,  -261,  -261,  -261,    65,  -261,  -261,  -261,
    -261,   -41,  -261,  -261,  -261,  -261,  -261,  -261,  -151,  -260,
    -261,  -261,   166,  -261,  -261,   128,  -261,  -261,  -261,    21,
    -261,  -261,  -261,  -261,   104,  -261,  -261,   263,   -43,  -261,
    -261,  -261,  -261,    87,  -261,  -261,  -261,  -261,  -261,  -261,
    -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,  -261,
     146,  -261,  -261,  -261,  -261,  -261,  -261,   120,  -261,  -261,
      66,  -261,  -261,   255,    25,  -190,  -261,  -261,  -261,  -261,
     -27,  -261,  -261,   -78,  -261,  -261,  -261,  -141,  -261,  -154,
    -261
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    57,    58,    59,    60,    61,   134,   125,   126,   303,
     361,   362,   363,   364,   365,   366,   367,    62,    63,    64,
      65,    86,   241,   242,    66,   206,   207,   208,   209,    67,
     177,   120,   247,   323,   324,   325,   383,    68,   278,   334,
     403,   106,   107,   108,   145,   146,   147,    69,   258,   259,
     260,   261,    70,   172,   173,   174,    71,    99,   100,   101,
     102,    72,   190,   191,   192,    73,    74,    75,   266,    76,
      77,   110,   152,   274,   275,   176,   408,   298,   343,   132,
     133,    78,    79,   309,   233,    80,   161,   162,   218,   214,
     215,   216,   151,   135,   294,   226,   210,    81,    82,   312,
     313,   314,   370,   371,   427,   372,   430,   431,   444,   445,
     446
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     124,   211,   289,   291,   412,   178,   341,   376,   169,   212,
     220,   264,   368,   429,   304,   281,   283,   272,   230,   297,
       1,   337,   163,   339,   434,   167,   224,   292,   121,     2,
     122,    84,   377,   310,   311,     3,     4,     5,    83,   112,
     277,   231,    85,   113,     6,     7,   222,   109,   223,   248,
     265,     8,     9,   127,   273,    10,   243,    11,   225,    12,
      13,   282,   368,   170,    14,   164,   232,   165,   243,   128,
     330,   417,   129,    15,   130,   404,   405,   131,    16,   263,
     136,   305,   342,   306,   149,    17,   150,    18,   103,   153,
     168,   175,   179,   249,   250,   251,   252,    19,    20,   123,
      21,    22,   217,   219,   114,    23,    24,   310,   311,    25,
      26,   221,   227,   235,   234,   378,   154,   155,    27,   228,
     268,   229,   245,   379,   413,   171,   246,   123,   335,   336,
     262,   270,    28,   268,    29,    30,   287,   156,   236,   317,
      31,   237,   380,   293,   271,   277,   423,   318,   279,   280,
      32,   319,   253,   115,   166,    33,   213,    34,   286,    35,
     285,    36,    37,   116,   123,   290,   117,   296,   299,   301,
     316,    38,    39,    40,    41,    42,    43,    44,   302,   157,
      45,   307,   104,    46,   420,    47,   326,   406,   105,   315,
     118,   320,   327,   328,    48,   119,   332,   254,   381,    49,
      50,    51,   382,    52,    53,   308,   333,   344,   340,   158,
      54,    55,   439,   440,   441,   442,   443,   255,   373,   374,
      -6,    56,   256,   257,   321,   439,   440,   441,   442,   443,
     238,   239,   338,   375,   407,   447,   384,   240,   432,     2,
     410,   411,   414,   437,   415,     3,     4,     5,   416,   419,
     421,   422,   424,   426,     6,     7,   428,   429,   436,   449,
     452,     8,     9,   433,   451,    10,   450,    11,   438,    12,
      13,   295,   244,   269,    14,   111,   284,   288,   267,   329,
     300,   276,   159,    15,   148,   369,   331,   160,    16,   435,
     448,   409,   418,   322,     0,    17,     0,    18,     0,     0,
     137,   138,   139,   140,     0,     0,     0,    19,    20,     0,
      21,    22,     0,     0,     0,    23,    24,     0,     0,    25,
      26,     0,     0,     0,     0,     0,     0,     0,    27,     0,
       0,   141,     0,   142,    87,   143,     0,     0,     0,    88,
       0,   144,    28,     0,    29,    30,    89,     0,     0,     0,
      31,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      32,     0,     0,     0,     0,    33,     0,    34,     0,    35,
       0,    36,    37,     0,     0,     0,     0,     0,     0,     0,
       0,    38,    39,    40,    41,    42,    43,    44,     0,     0,
      45,     0,     0,    46,     0,    47,     0,     0,     0,     0,
     425,     0,    90,     0,    48,     0,     0,     0,     0,    49,
      50,    51,     0,    52,    53,     0,     0,     2,     0,   345,
      54,    55,     0,     3,     4,     5,     0,     0,     0,   346,
      -6,    56,     6,     7,     0,     0,    91,    92,     0,     8,
       9,     0,     0,    10,     0,    11,     0,    12,    13,   180,
       0,     0,    14,     0,     0,    93,     0,     0,     0,     0,
      94,    15,     0,     0,     0,     0,    16,     0,   347,   348,
       0,     0,     0,    17,     0,    18,     0,   181,     0,     0,
       0,     0,     0,     0,     0,    19,    20,   349,    21,    22,
       0,    95,     0,    23,    24,     0,   182,    25,    26,     0,
     183,     0,     0,     0,     0,     0,    27,     0,   350,     0,
       0,     0,     0,     0,    96,    97,    98,   351,     0,   352,
      28,     0,    29,    30,     0,     0,     0,     0,    31,     0,
       0,     0,     0,     0,     0,   353,     0,     0,    32,     0,
       0,     0,     0,    33,     0,    34,     0,    35,     0,    36,
      37,     0,     0,     0,     0,   354,   355,     0,     0,    38,
      39,    40,    41,    42,    43,    44,     0,     0,    45,     0,
       0,    46,     0,    47,     0,     0,   385,     0,   184,     0,
       0,     0,    48,     0,     0,   386,     0,    49,    50,    51,
       0,    52,    53,   356,   387,   357,     0,     0,    54,    55,
       0,     0,     0,   358,     0,     0,     0,   359,   360,    56,
     185,   186,   187,   188,   388,     0,     0,   389,   189,   193,
       0,   194,   195,   390,     0,     0,     0,     0,   196,     0,
       0,   197,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   391,     0,
       0,     0,   392,   393,     0,     0,   394,   395,   396,   198,
     397,   398,   399,     0,   400,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   401,     0,     0,     0,     0,     0,     0,
       0,   199,     0,   200,     0,     0,     0,     0,     0,   201,
       0,   202,     0,     0,     0,   203,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   402,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   204,
     205
};

static const yytype_int16 yycheck[] =
{
      15,    51,   192,    43,    67,    46,    68,     4,    46,    67,
      58,    22,   158,   206,     5,   166,   167,    30,    37,   209,
       1,   281,    30,   283,   217,    40,    35,    67,    73,    10,
      75,    67,    29,   203,   204,    16,    17,    18,   201,    23,
      71,    60,    67,    27,    25,    26,    61,   172,    63,     3,
      61,    32,    33,   172,    67,    36,    99,    38,    67,    40,
      41,    92,   158,   101,    45,    73,    85,    75,   111,   172,
     260,   217,   172,    54,   172,   335,   336,   172,    59,   127,
      67,    72,   144,    74,   172,    66,   172,    68,    11,    67,
      67,   172,   172,    47,    48,    49,    50,    78,    79,   172,
      81,    82,    67,     0,    88,    86,    87,   203,   204,    90,
      91,   201,    67,    31,   216,   112,     7,     8,    99,   172,
     135,   172,    67,   120,   187,   163,    35,   172,   279,   280,
     172,    67,   113,   148,   115,   116,   177,    28,    56,    32,
     121,    59,   139,   183,   172,    71,   406,    40,    30,    30,
     131,    44,   106,   137,   162,   136,   214,   138,   213,   140,
      67,   142,   143,   147,   172,   172,   150,    67,    67,    67,
     172,   152,   153,   154,   155,   156,   157,   158,   201,    70,
     161,   172,   105,   164,   374,   166,   172,   338,   111,    67,
     174,    84,   172,    67,   175,   179,    67,   151,   195,   180,
     181,   182,   199,   184,   185,   196,    67,    39,   172,   100,
     191,   192,   207,   208,   209,   210,   211,   171,   201,   213,
     201,   202,   176,   177,   117,   207,   208,   209,   210,   211,
     148,   149,   282,   172,    30,   217,   212,   155,   428,    10,
      67,    67,   172,   433,   213,    16,    17,    18,   201,   216,
      67,    55,   215,   205,    25,    26,   213,   206,   201,   201,
     450,    32,    33,   213,   201,    36,   213,    38,   216,    40,
      41,   206,   106,   145,    45,    12,   172,   190,   132,   258,
     214,   161,   173,    54,    29,   312,   261,   178,    59,   430,
     444,   341,   370,   186,    -1,    66,    -1,    68,    -1,    -1,
      62,    63,    64,    65,    -1,    -1,    -1,    78,    79,    -1,
      81,    82,    -1,    -1,    -1,    86,    87,    -1,    -1,    90,
      91,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    99,    -1,
      -1,    93,    -1,    95,     8,    97,    -1,    -1,    -1,    13,
      -1,   103,   113,    -1,   115,   116,    20,    -1,    -1,    -1,
     121,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     131,    -1,    -1,    -1,    -1,   136,    -1,   138,    -1,   140,
      -1,   142,   143,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   152,   153,   154,   155,   156,   157,   158,    -1,    -1,
     161,    -1,    -1,   164,    -1,   166,    -1,    -1,    -1,    -1,
     415,    -1,    76,    -1,   175,    -1,    -1,    -1,    -1,   180,
     181,   182,    -1,   184,   185,    -1,    -1,    10,    -1,     9,
     191,   192,    -1,    16,    17,    18,    -1,    -1,    -1,    19,
     201,   202,    25,    26,    -1,    -1,   110,   111,    -1,    32,
      33,    -1,    -1,    36,    -1,    38,    -1,    40,    41,     6,
      -1,    -1,    45,    -1,    -1,   129,    -1,    -1,    -1,    -1,
     134,    54,    -1,    -1,    -1,    -1,    59,    -1,    58,    59,
      -1,    -1,    -1,    66,    -1,    68,    -1,    34,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    78,    79,    77,    81,    82,
      -1,   165,    -1,    86,    87,    -1,    53,    90,    91,    -1,
      57,    -1,    -1,    -1,    -1,    -1,    99,    -1,    98,    -1,
      -1,    -1,    -1,    -1,   188,   189,   190,   107,    -1,   109,
     113,    -1,   115,   116,    -1,    -1,    -1,    -1,   121,    -1,
      -1,    -1,    -1,    -1,    -1,   125,    -1,    -1,   131,    -1,
      -1,    -1,    -1,   136,    -1,   138,    -1,   140,    -1,   142,
     143,    -1,    -1,    -1,    -1,   145,   146,    -1,    -1,   152,
     153,   154,   155,   156,   157,   158,    -1,    -1,   161,    -1,
      -1,   164,    -1,   166,    -1,    -1,    42,    -1,   135,    -1,
      -1,    -1,   175,    -1,    -1,    51,    -1,   180,   181,   182,
      -1,   184,   185,   183,    60,   185,    -1,    -1,   191,   192,
      -1,    -1,    -1,   193,    -1,    -1,    -1,   197,   198,   202,
     167,   168,   169,   170,    80,    -1,    -1,    83,   175,    12,
      -1,    14,    15,    89,    -1,    -1,    -1,    -1,    21,    -1,
      -1,    24,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   114,    -1,
      -1,    -1,   118,   119,    -1,    -1,   122,   123,   124,    52,
     126,   127,   128,    -1,   130,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   159,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    94,    -1,    96,    -1,    -1,    -1,    -1,    -1,   102,
      -1,   104,    -1,    -1,    -1,   108,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   193,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   132,
     133
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,     1,    10,    16,    17,    18,    25,    26,    32,    33,
      36,    38,    40,    41,    45,    54,    59,    66,    68,    78,
      79,    81,    82,    86,    87,    90,    91,    99,   113,   115,
     116,   121,   131,   136,   138,   140,   142,   143,   152,   153,
     154,   155,   156,   157,   158,   161,   164,   166,   175,   180,
     181,   182,   184,   185,   191,   192,   202,   219,   220,   221,
     222,   223,   235,   236,   237,   238,   242,   247,   255,   265,
     270,   274,   279,   283,   284,   285,   287,   288,   299,   300,
     303,   315,   316,   201,    67,    67,   239,     8,    13,    20,
      76,   110,   111,   129,   134,   165,   188,   189,   190,   275,
     276,   277,   278,    11,   105,   111,   259,   260,   261,   172,
     289,   275,    23,    27,    88,   137,   147,   150,   174,   179,
     249,    73,    75,   172,   224,   225,   226,   172,   172,   172,
     172,   172,   297,   298,   224,   311,    67,    62,    63,    64,
      65,    93,    95,    97,   103,   262,   263,   264,   311,   172,
     172,   310,   290,    67,     7,     8,    28,    70,   100,   173,
     178,   304,   305,    30,    73,    75,   162,   224,    67,    46,
     101,   163,   271,   272,   273,   172,   293,   248,   249,   172,
       6,    34,    53,    57,   135,   167,   168,   169,   170,   175,
     280,   281,   282,    12,    14,    15,    21,    24,    52,    94,
      96,   102,   104,   108,   132,   133,   243,   244,   245,   246,
     314,   225,    67,   214,   307,   308,   309,    67,   306,     0,
     221,   201,   224,   224,    35,    67,   313,    67,   172,   172,
      37,    60,    85,   302,   216,    31,    56,    59,   148,   149,
     155,   240,   241,   276,   260,    67,    35,   250,     3,    47,
      48,    49,    50,   106,   151,   171,   176,   177,   266,   267,
     268,   269,   172,   221,    22,    61,   286,   298,   224,   263,
      67,   172,    30,    67,   291,   292,   305,    71,   256,    30,
      30,   256,    92,   256,   272,    67,   213,   249,   281,   313,
     172,    43,    67,   183,   312,   244,    67,   313,   295,    67,
     308,    67,   201,   227,     5,    72,    74,   172,   196,   301,
     203,   204,   317,   318,   319,    67,   172,    32,    40,    44,
      84,   117,   186,   251,   252,   253,   172,   172,    67,   267,
     313,   312,    67,    67,   257,   256,   256,   257,   225,   257,
     172,    68,   144,   296,    39,     9,    19,    58,    59,    77,
      98,   107,   109,   125,   145,   146,   183,   185,   193,   197,
     198,   228,   229,   230,   231,   232,   233,   234,   158,   318,
     320,   321,   323,   201,   213,   172,     4,    29,   112,   120,
     139,   195,   199,   254,   212,    42,    51,    60,    80,    83,
      89,   114,   118,   119,   122,   123,   124,   126,   127,   128,
     130,   159,   193,   258,   257,   257,   256,    30,   294,   225,
      67,    67,    67,   187,   172,   213,   201,   217,   321,   216,
     313,    67,    55,   257,   215,   224,   205,   322,   213,   206,
     324,   325,   313,   213,   217,   325,   201,   313,   216,   207,
     208,   209,   210,   211,   326,   327,   328,   217,   327,   201,
     213,   201,   313
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   218,   219,   220,   220,   220,   221,   221,   221,   221,
     221,   221,   221,   221,   221,   221,   221,   221,   221,   221,
     222,   223,   223,   223,   223,   223,   224,   224,   225,   226,
     226,   227,   227,   228,   228,   228,   229,   230,   230,   230,
     230,   230,   230,   230,   230,   230,   231,   231,   232,   232,
     232,   232,   232,   232,   233,   234,   235,   236,   236,   237,
     237,   237,   237,   238,   238,   238,   238,   238,   238,   238,
     238,   238,   239,   239,   240,   240,   241,   241,   241,   241,
     241,   242,   243,   243,   244,   244,   244,   244,   245,   245,
     245,   245,   245,   245,   245,   245,   245,   246,   246,   247,
     247,   247,   248,   248,   249,   249,   249,   249,   249,   249,
     249,   249,   250,   250,   251,   251,   251,   251,   252,   252,
     253,   253,   254,   254,   254,   254,   254,   254,   254,   255,
     255,   255,   255,   255,   255,   255,   255,   256,   256,   257,
     257,   257,   258,   258,   258,   258,   258,   258,   258,   258,
     258,   258,   258,   258,   258,   258,   258,   258,   258,   259,
     259,   260,   261,   261,   261,   262,   262,   263,   264,   264,
     264,   264,   264,   264,   264,   264,   265,   266,   266,   267,
     267,   267,   267,   267,   268,   268,   268,   269,   269,   269,
     269,   270,   271,   271,   272,   273,   273,   273,   274,   274,
     275,   275,   276,   276,   277,   277,   277,   277,   277,   277,
     278,   278,   278,   278,   278,   278,   279,   280,   280,   281,
     282,   282,   282,   282,   282,   282,   282,   282,   282,   282,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   284,   284,   284,   285,
     285,   286,   286,   286,   287,   288,   288,   288,   289,   289,
     289,   290,   290,   291,   292,   292,   293,   294,   294,   295,
     295,   296,   296,   297,   297,   298,   299,   299,   300,   300,
     301,   301,   301,   301,   302,   302,   302,   303,   304,   304,
     305,   305,   305,   305,   305,   305,   305,   306,   306,   307,
     307,   308,   308,   309,   310,   310,   311,   311,   312,   312,
     312,   313,   313,   314,   315,   316,   317,   317,   318,   319,
     319,   320,   320,   321,   322,   323,   324,   324,   325,   326,
     326,   327,   328,   328,   328,   328,   328
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     3,     2,     2,     0,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       3,     1,     1,     1,     1,     1,     1,     2,     1,     1,
       1,     0,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     2,     1,     1,
       1,     1,     1,     1,     2,     1,     2,     1,     1,     1,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     0,     2,     2,     2,     1,     1,     1,     1,
       1,     2,     2,     1,     2,     2,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     3,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     0,     2,     2,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     4,     6,     4,     5,     5,     4,     0,     2,     0,
       2,     3,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       1,     2,     1,     1,     1,     2,     1,     2,     1,     1,
       1,     1,     1,     1,     1,     1,     3,     2,     1,     2,
       2,     2,     2,     2,     1,     1,     1,     1,     1,     1,
       1,     2,     2,     1,     2,     1,     1,     1,     2,     2,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     2,     1,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     2,     2,     2,     3,     3,     1,     2,
       2,     2,     2,     2,     3,     2,     1,     1,     1,     1,
       1,     1,     1,     0,     1,     1,     1,     1,     1,     2,
       0,     0,     2,     4,     1,     1,     4,     1,     0,     0,
       2,     2,     2,     2,     1,     1,     3,     3,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     2,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     1,     2,
       1,     1,     1,     5,     2,     1,     2,     1,     1,     1,
       1,     1,     1,     2,     5,     1,     3,     2,     3,     1,
       1,     2,     1,     5,     4,     3,     2,     1,     6,     3,
       2,     3,     1,     1,     1,     1,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


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
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

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
#ifndef YYINITDEPTH
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
static YYSIZE_T
yystrlen (const char *yystr)
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
static char *
yystpcpy (char *yydest, const char *yysrc)
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
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
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
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
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

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

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
      yychar = yylex ();
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
     '$$ = $1'.

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
#line 398 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2194 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 20:
#line 434 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			peer_node *my_node;

			my_node = create_peer_node((yyvsp[-2].Integer), (yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.peers, my_node);
		}
#line 2205 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 27:
#line 453 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = create_address_node((yyvsp[0].String), (yyvsp[-1].Integer)); }
#line 2211 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 28:
#line 458 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = create_address_node((yyvsp[0].String), AF_UNSPEC); }
#line 2217 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 29:
#line 463 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = AF_INET; }
#line 2223 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 30:
#line 465 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = AF_INET6; }
#line 2229 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 31:
#line 470 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2235 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 32:
#line 472 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2244 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 36:
#line 486 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2250 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 46:
#line 503 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2256 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 47:
#line 505 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_uval((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2262 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 54:
#line 519 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2268 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 56:
#line 533 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			unpeer_node *my_node;

			my_node = create_unpeer_node((yyvsp[0].Address_node));
			if (my_node)
				APPEND_G_FIFO(cfgt.unpeers, my_node);
		}
#line 2280 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 59:
#line 554 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.broadcastclient = 1; }
#line 2286 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 60:
#line 556 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.manycastserver, (yyvsp[0].Address_fifo)); }
#line 2292 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 61:
#line 558 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.multicastclient, (yyvsp[0].Address_fifo)); }
#line 2298 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 62:
#line 560 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.mdnstries = (yyvsp[0].Integer); }
#line 2304 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 63:
#line 571 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *atrv;

			atrv = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			APPEND_G_FIFO(cfgt.vars, atrv);
		}
#line 2315 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 64:
#line 578 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.control_key = (yyvsp[0].Integer); }
#line 2321 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 65:
#line 580 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			cfgt.auth.cryptosw++;
			CONCAT_G_FIFOS(cfgt.auth.crypto_cmd_list, (yyvsp[0].Attr_val_fifo));
		}
#line 2330 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 66:
#line 585 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.keys = (yyvsp[0].String); }
#line 2336 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 67:
#line 587 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.keysdir = (yyvsp[0].String); }
#line 2342 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 68:
#line 589 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.request_key = (yyvsp[0].Integer); }
#line 2348 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 69:
#line 591 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.revoke = (yyvsp[0].Integer); }
#line 2354 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 70:
#line 593 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			/* [Bug 948] leaves it open if appending or
			 * replacing the trusted key list is the right
			 * way. In any case, either alternative should
			 * be coded correctly!
			 */
			DESTROY_G_FIFO(cfgt.auth.trusted_key_list, destroy_attr_val); /* remove for append */
			CONCAT_G_FIFOS(cfgt.auth.trusted_key_list, (yyvsp[0].Attr_val_fifo));
		}
#line 2368 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 71:
#line 603 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.ntp_signd_socket = (yyvsp[0].String); }
#line 2374 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 72:
#line 608 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2380 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 73:
#line 610 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2389 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 74:
#line 618 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2395 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 75:
#line 620 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val) = NULL;
			cfgt.auth.revoke = (yyvsp[0].Integer);
			msyslog(LOG_WARNING,
				"'crypto revoke %d' is deprecated, "
				"please use 'revoke %d' instead.",
				cfgt.auth.revoke, cfgt.auth.revoke);
		}
#line 2408 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 81:
#line 645 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.orphan_cmds, (yyvsp[0].Attr_val_fifo)); }
#line 2414 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 82:
#line 650 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2423 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 83:
#line 655 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2432 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 84:
#line 663 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2438 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 85:
#line 665 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2444 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 86:
#line 667 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2450 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 87:
#line 669 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Basedate, (yyvsp[0].Integer)); }
#line 2456 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 99:
#line 696 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.stats_list, (yyvsp[0].Int_fifo)); }
#line 2462 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 100:
#line 698 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				cfgt.stats_dir = (yyvsp[0].String);
			} else {
				YYFREE((yyvsp[0].String));
				yyerror("statsdir remote configuration ignored");
			}
		}
#line 2475 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 101:
#line 707 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			filegen_node *fgn;

			fgn = create_filegen_node((yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.filegen_opts, fgn);
		}
#line 2486 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 102:
#line 717 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2495 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 103:
#line 722 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2504 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 112:
#line 741 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2510 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 113:
#line 743 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2519 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 114:
#line 751 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			} else {
				(yyval.Attr_val) = NULL;
				YYFREE((yyvsp[0].String));
				yyerror("filegen file remote config ignored");
			}
		}
#line 2533 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 115:
#line 761 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror("filegen type remote config ignored");
			}
		}
#line 2546 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 116:
#line 770 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			const char *err;

			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				if (T_Link == (yyvsp[0].Integer))
					err = "filegen link remote config ignored";
				else
					err = "filegen nolink remote config ignored";
				yyerror(err);
			}
		}
#line 2565 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 117:
#line 785 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2571 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 129:
#line 815 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			CONCAT_G_FIFOS(cfgt.discard_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2579 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 130:
#line 819 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			CONCAT_G_FIFOS(cfgt.mru_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2587 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 131:
#line 823 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[-2].Address_node), NULL, (yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2599 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 132:
#line 831 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[-4].Address_node), (yyvsp[-2].Address_node), (yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2611 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 133:
#line 839 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node(NULL, NULL, (yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2623 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 134:
#line 847 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node(
				create_address_node(
					estrdup("0.0.0.0"),
					AF_INET),
				create_address_node(
					estrdup("0.0.0.0"),
					AF_INET),
				(yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo),
				lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2642 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 135:
#line 862 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node(
				create_address_node(
					estrdup("::"),
					AF_INET6),
				create_address_node(
					estrdup("::"),
					AF_INET6),
				(yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo),
				lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2661 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 136:
#line 877 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *	rn;

			APPEND_G_FIFO((yyvsp[0].Attr_val_fifo), create_attr_ival((yyvsp[-2].Integer), 1));
			rn = create_restrict_node(
				NULL, NULL, (yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo), lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2674 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 137:
#line 889 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = -1; }
#line 2680 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 138:
#line 891 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (((yyvsp[0].Integer) < -1) || ((yyvsp[0].Integer) > 100)) {
				struct FILE_INFO * ip_ctx;

				ip_ctx = lex_current();
				msyslog(LOG_ERR,
					"Unreasonable ippeerlimit value (%d) in %s line %d, column %d.  Using 0.",
					(yyvsp[0].Integer),
					ip_ctx->fname,
					ip_ctx->errpos.nline,
					ip_ctx->errpos.ncol);
				(yyvsp[0].Integer) = 0;
			}
			(yyval.Integer) = (yyvsp[0].Integer);
		}
#line 2700 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 139:
#line 910 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2706 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 140:
#line 912 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;

			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			av = create_attr_ival((yyvsp[0].Integer), 1);
			APPEND_G_FIFO((yyval.Attr_val_fifo), av);
		}
#line 2718 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 141:
#line 920 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;

			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			av = create_attr_ival(T_ServerresponseFuzz, 1);
			APPEND_G_FIFO((yyval.Attr_val_fifo), av);
		}
#line 2730 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 159:
#line 951 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2739 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 160:
#line 956 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2748 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 161:
#line 964 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2754 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 165:
#line 975 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2763 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 166:
#line 980 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2772 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 167:
#line 988 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2778 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 176:
#line 1008 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.fudge, aon);
		}
#line 2789 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 177:
#line 1018 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2798 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 178:
#line 1023 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2807 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 179:
#line 1031 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2813 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 180:
#line 1033 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2819 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 181:
#line 1035 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if ((yyvsp[0].Integer) >= 0 && (yyvsp[0].Integer) <= 16) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror("fudge factor: stratum value not in [0..16], ignored");
			}
		}
#line 2832 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 182:
#line 1044 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2838 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 183:
#line 1046 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2844 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 191:
#line 1068 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.rlimit, (yyvsp[0].Attr_val_fifo)); }
#line 2850 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 192:
#line 1073 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2859 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 193:
#line 1078 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2868 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 194:
#line 1086 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2874 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 198:
#line 1102 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.enable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 2880 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 199:
#line 1104 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.disable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 2886 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 200:
#line 1109 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2895 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 201:
#line 1114 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2904 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 202:
#line 1122 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2910 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 203:
#line 1124 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer));
			} else {
				char err_str[128];

				(yyval.Attr_val) = NULL;
				snprintf(err_str, sizeof(err_str),
					 "enable/disable %s remote configuration ignored",
					 keyword((yyvsp[0].Integer)));
				yyerror(err_str);
			}
		}
#line 2928 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 216:
#line 1163 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.tinker, (yyvsp[0].Attr_val_fifo)); }
#line 2934 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 217:
#line 1168 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2943 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 218:
#line 1173 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2952 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 219:
#line 1181 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2958 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 232:
#line 1206 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;

			av = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2969 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 233:
#line 1213 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;

			av = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2980 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 234:
#line 1220 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;

			av = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2991 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 235:
#line 1227 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			char error_text[64];
			attr_val *av;

			if (lex_from_file()) {
				av = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
				APPEND_G_FIFO(cfgt.vars, av);
			} else {
				YYFREE((yyvsp[0].String));
				snprintf(error_text, sizeof(error_text),
					 "%s remote config ignored",
					 keyword((yyvsp[-1].Integer)));
				yyerror(error_text);
			}
		}
#line 3011 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 236:
#line 1243 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (!lex_from_file()) {
				YYFREE((yyvsp[-1].String)); /* avoid leak */
				yyerror("remote includefile ignored");
				break;
			}
			if (lex_level() > MAXINCLUDELEVEL) {
				fprintf(stderr, "getconfig: Maximum include file level exceeded.\n");
				msyslog(LOG_ERR, "getconfig: Maximum include file level exceeded.");
			} else {
				const char * path = FindConfig((yyvsp[-1].String)); /* might return $2! */
				if (!lex_push_file(path, "r")) {
					fprintf(stderr, "getconfig: Couldn't open <%s>\n", path);
					msyslog(LOG_ERR, "getconfig: Couldn't open <%s>", path);
				}
			}
			YYFREE((yyvsp[-1].String)); /* avoid leak */
		}
#line 3034 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 237:
#line 1262 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;

			av = create_attr_sval((yyvsp[-2].Integer), (yyvsp[-1].String));
			av->flag = (yyvsp[0].Integer);
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 3046 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 238:
#line 1270 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { lex_flush_stack(); }
#line 3052 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 239:
#line 1272 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { /* see drift_parm below for actions */ }
#line 3058 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 240:
#line 1274 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.logconfig, (yyvsp[0].Attr_val_fifo)); }
#line 3064 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 241:
#line 1276 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.phone, (yyvsp[0].String_fifo)); }
#line 3070 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 242:
#line 1278 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.pollskewlist, (yyvsp[0].Attr_val_fifo)); }
#line 3076 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 243:
#line 1280 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { APPEND_G_FIFO(cfgt.setvar, (yyvsp[0].Set_var)); }
#line 3082 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 244:
#line 1282 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.trap, aon);
		}
#line 3093 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 245:
#line 1289 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.ttl, (yyvsp[0].Attr_val_fifo)); }
#line 3099 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 250:
#line 1304 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
#ifndef LEAP_SMEAR
			yyerror("Built without LEAP_SMEAR support.");
#endif
		}
#line 3109 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 251:
#line 1313 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = FALSE; }
#line 3115 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 252:
#line 1315 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = TRUE; }
#line 3121 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 253:
#line 1317 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {  (yyval.Integer) = TRUE; }
#line 3127 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 258:
#line 1332 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				attr_val *av;
				av = create_attr_sval(T_Driftfile, (yyvsp[0].String));
				APPEND_G_FIFO(cfgt.vars, av);
			} else {
				YYFREE((yyvsp[0].String));
				yyerror("driftfile remote configuration ignored");
			}
		}
#line 3142 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 259:
#line 1343 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				attr_val *av;
				av = create_attr_sval(T_Driftfile, (yyvsp[-1].String));
				APPEND_G_FIFO(cfgt.vars, av);
				av = create_attr_dval(T_WanderThreshold, (yyvsp[0].Double));
				APPEND_G_FIFO(cfgt.vars, av);
			msyslog(LOG_WARNING,
				"'driftfile FILENAME WanderValue' is deprecated, "
				"please use separate 'driftfile FILENAME' and "
				"'nonvolatile WanderValue' lines instead.");
			} else {
				YYFREE((yyvsp[-1].String));
				yyerror("driftfile remote configuration ignored");
			}
		}
#line 3163 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 260:
#line 1360 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				attr_val *av;
				av = create_attr_sval(T_Driftfile, estrdup(""));
				APPEND_G_FIFO(cfgt.vars, av);
			} else {
				yyerror("driftfile remote configuration ignored");
			}
		}
#line 3177 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 261:
#line 1373 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 3183 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 262:
#line 1375 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = append_gen_fifo((yyvsp[-1].Attr_val_fifo), (yyvsp[0].Attr_val)); }
#line 3189 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 263:
#line 1380 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if ((yyvsp[-2].Integer) < 0 || (yyvsp[0].Integer) < 0) {
				/* bad numbers */
				yyerror("pollskewlist: skew values must be >=0");
				destroy_attr_val((yyvsp[-3].Attr_val));
				(yyvsp[-3].Attr_val) = NULL;
			} else if ((yyvsp[-3].Attr_val) == NULL) {
				yyerror("pollskewlist: poll value must be 3-17, inclusive");
			} else if ((yyvsp[-3].Attr_val)->attr <= 0) {
				/* process default range */
				(yyvsp[-3].Attr_val)->value.r.first = (yyvsp[-2].Integer);
				(yyvsp[-3].Attr_val)->value.r.last  = (yyvsp[0].Integer);
			} else if ((yyvsp[-2].Integer) < (1 << ((yyvsp[-3].Attr_val)->attr - 1)) && (yyvsp[0].Integer) < (1 << ((yyvsp[-3].Attr_val)->attr - 1))) {
				(yyvsp[-3].Attr_val)->value.r.first = (yyvsp[-2].Integer);
				(yyvsp[-3].Attr_val)->value.r.last  = (yyvsp[0].Integer);
			} else {
				yyerror("pollskewlist: randomization limit must be <= half the poll interval");
				destroy_attr_val((yyvsp[-3].Attr_val));
				(yyvsp[-3].Attr_val) = NULL;
			}
			(yyval.Attr_val) = (yyvsp[-3].Attr_val);
		}
#line 3216 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 264:
#line 1405 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = ((yyvsp[0].Integer) >= 3 && (yyvsp[0].Integer) <= 17) ? create_attr_rval((yyvsp[0].Integer), 0, 0) : NULL; }
#line 3222 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 265:
#line 1406 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_rval(-1, 0, 0); }
#line 3228 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 266:
#line 1412 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Set_var) = create_setvar_node((yyvsp[-3].String), (yyvsp[-1].String), (yyvsp[0].Integer)); }
#line 3234 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 268:
#line 1418 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 0; }
#line 3240 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 269:
#line 1423 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 3246 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 270:
#line 1425 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3255 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 271:
#line 1433 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 3261 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 272:
#line 1435 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), estrdup((yyvsp[0].Address_node)->address));
			destroy_address_node((yyvsp[0].Address_node));
		}
#line 3270 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 273:
#line 1443 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3279 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 274:
#line 1448 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3288 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 275:
#line 1456 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			char	prefix;
			char *	type;

			switch ((yyvsp[0].String)[0]) {

			case '+':
			case '-':
			case '=':
				prefix = (yyvsp[0].String)[0];
				type = (yyvsp[0].String) + 1;
				break;

			default:
				prefix = '=';
				type = (yyvsp[0].String);
			}

			(yyval.Attr_val) = create_attr_sval(prefix, estrdup(type));
			YYFREE((yyvsp[0].String));
		}
#line 3314 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 276:
#line 1481 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node((yyvsp[0].Integer), NULL, (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3325 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 277:
#line 1488 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node(0, (yyvsp[0].String), (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3336 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 287:
#line 1516 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.reset_counters, (yyvsp[0].Int_fifo)); }
#line 3342 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 288:
#line 1521 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3351 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 289:
#line 1526 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3360 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 297:
#line 1550 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3369 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 298:
#line 1555 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3378 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 299:
#line 1563 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3387 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 300:
#line 1568 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3396 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 301:
#line 1576 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival('i', (yyvsp[0].Integer)); }
#line 3402 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 303:
#line 1582 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_rval('-', (yyvsp[-3].Integer), (yyvsp[-1].Integer)); }
#line 3408 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 304:
#line 1587 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.String_fifo) = (yyvsp[-1].String_fifo);
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3417 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 305:
#line 1592 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.String_fifo) = NULL;
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3426 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 306:
#line 1600 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Address_fifo) = (yyvsp[-1].Address_fifo);
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3435 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 307:
#line 1605 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Address_fifo) = NULL;
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3444 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 308:
#line 1613 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if ((yyvsp[0].Integer) != 0 && (yyvsp[0].Integer) != 1) {
				yyerror("Integer value is not boolean (0 or 1). Assuming 1");
				(yyval.Integer) = 1;
			} else {
				(yyval.Integer) = (yyvsp[0].Integer);
			}
		}
#line 3457 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 309:
#line 1621 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 1; }
#line 3463 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 310:
#line 1622 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 0; }
#line 3469 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 311:
#line 1626 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Double) = (double)(yyvsp[0].Integer); }
#line 3475 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 313:
#line 1632 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = basedate_eval_string((yyvsp[0].String)); YYFREE((yyvsp[0].String)); }
#line 3481 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 314:
#line 1640 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			sim_node *sn;

			sn =  create_sim_node((yyvsp[-2].Attr_val_fifo), (yyvsp[-1].Sim_server_fifo));
			APPEND_G_FIFO(cfgt.sim_details, sn);

			/* Revert from ; to \n for end-of-command */
			old_config_style = 1;
		}
#line 3495 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 315:
#line 1657 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { old_config_style = 0; }
#line 3501 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 316:
#line 1662 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3510 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 317:
#line 1667 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3519 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 318:
#line 1675 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3525 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 321:
#line 1685 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_server_fifo) = (yyvsp[-1].Sim_server_fifo);
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3534 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 322:
#line 1690 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_server_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3543 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 323:
#line 1698 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Sim_server) = ONLY_SIM(create_sim_server((yyvsp[-4].Address_node), (yyvsp[-2].Double), (yyvsp[-1].Sim_script_fifo))); }
#line 3549 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 324:
#line 1703 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Double) = (yyvsp[-1].Double); }
#line 3555 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 325:
#line 1708 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = (yyvsp[0].Address_node); }
#line 3561 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 326:
#line 1713 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_script_fifo) = (yyvsp[-1].Sim_script_fifo);
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3570 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 327:
#line 1718 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_script_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3579 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 328:
#line 1726 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Sim_script) = ONLY_SIM(create_sim_script_info((yyvsp[-3].Double), (yyvsp[-1].Attr_val_fifo))); }
#line 3585 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 329:
#line 1731 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3594 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 330:
#line 1736 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3603 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 331:
#line 1744 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3609 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;


#line 3613 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
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

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
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

  /* Do not reclaim the symbols of the rule whose action triggered
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
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

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
  /* Do not reclaim the symbols of the rule whose action triggered
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
  return yyresult;
}
#line 1755 "../../ntpd/ntp_parser.y" /* yacc.c:1906  */


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

