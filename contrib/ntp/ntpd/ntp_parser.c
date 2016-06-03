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

#line 100 "../../ntpd/ntp_parser.c" /* yacc.c:339  */

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
#ifndef YY_YY_Y_TAB_H_INCLUDED
# define YY_YY_Y_TAB_H_INCLUDED
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
    T_PCEdigest = 380,
    T_Panic = 381,
    T_Peer = 382,
    T_Peerstats = 383,
    T_Phone = 384,
    T_Pid = 385,
    T_Pidfile = 386,
    T_Pool = 387,
    T_Port = 388,
    T_Preempt = 389,
    T_Prefer = 390,
    T_Protostats = 391,
    T_Pw = 392,
    T_Randfile = 393,
    T_Rawstats = 394,
    T_Refid = 395,
    T_Requestkey = 396,
    T_Reset = 397,
    T_Restrict = 398,
    T_Revoke = 399,
    T_Rlimit = 400,
    T_Saveconfigdir = 401,
    T_Server = 402,
    T_Setvar = 403,
    T_Source = 404,
    T_Stacksize = 405,
    T_Statistics = 406,
    T_Stats = 407,
    T_Statsdir = 408,
    T_Step = 409,
    T_Stepback = 410,
    T_Stepfwd = 411,
    T_Stepout = 412,
    T_Stratum = 413,
    T_String = 414,
    T_Sys = 415,
    T_Sysstats = 416,
    T_Tick = 417,
    T_Time1 = 418,
    T_Time2 = 419,
    T_Timer = 420,
    T_Timingstats = 421,
    T_Tinker = 422,
    T_Tos = 423,
    T_Trap = 424,
    T_True = 425,
    T_Trustedkey = 426,
    T_Ttl = 427,
    T_Type = 428,
    T_U_int = 429,
    T_UEcrypto = 430,
    T_UEcryptonak = 431,
    T_UEdigest = 432,
    T_Unconfig = 433,
    T_Unpeer = 434,
    T_Version = 435,
    T_WanderThreshold = 436,
    T_Week = 437,
    T_Wildcard = 438,
    T_Xleave = 439,
    T_Year = 440,
    T_Flag = 441,
    T_EOC = 442,
    T_Simulate = 443,
    T_Beep_Delay = 444,
    T_Sim_Duration = 445,
    T_Server_Offset = 446,
    T_Duration = 447,
    T_Freq_Offset = 448,
    T_Wander = 449,
    T_Jitter = 450,
    T_Prop_Delay = 451,
    T_Proc_Delay = 452
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
#define T_PCEdigest 380
#define T_Panic 381
#define T_Peer 382
#define T_Peerstats 383
#define T_Phone 384
#define T_Pid 385
#define T_Pidfile 386
#define T_Pool 387
#define T_Port 388
#define T_Preempt 389
#define T_Prefer 390
#define T_Protostats 391
#define T_Pw 392
#define T_Randfile 393
#define T_Rawstats 394
#define T_Refid 395
#define T_Requestkey 396
#define T_Reset 397
#define T_Restrict 398
#define T_Revoke 399
#define T_Rlimit 400
#define T_Saveconfigdir 401
#define T_Server 402
#define T_Setvar 403
#define T_Source 404
#define T_Stacksize 405
#define T_Statistics 406
#define T_Stats 407
#define T_Statsdir 408
#define T_Step 409
#define T_Stepback 410
#define T_Stepfwd 411
#define T_Stepout 412
#define T_Stratum 413
#define T_String 414
#define T_Sys 415
#define T_Sysstats 416
#define T_Tick 417
#define T_Time1 418
#define T_Time2 419
#define T_Timer 420
#define T_Timingstats 421
#define T_Tinker 422
#define T_Tos 423
#define T_Trap 424
#define T_True 425
#define T_Trustedkey 426
#define T_Ttl 427
#define T_Type 428
#define T_U_int 429
#define T_UEcrypto 430
#define T_UEcryptonak 431
#define T_UEdigest 432
#define T_Unconfig 433
#define T_Unpeer 434
#define T_Version 435
#define T_WanderThreshold 436
#define T_Week 437
#define T_Wildcard 438
#define T_Xleave 439
#define T_Year 440
#define T_Flag 441
#define T_EOC 442
#define T_Simulate 443
#define T_Beep_Delay 444
#define T_Sim_Duration 445
#define T_Server_Offset 446
#define T_Duration 447
#define T_Freq_Offset 448
#define T_Wander 449
#define T_Jitter 450
#define T_Prop_Delay 451
#define T_Proc_Delay 452

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 51 "../../ntpd/ntp_parser.y" /* yacc.c:355  */

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

#line 553 "../../ntpd/ntp_parser.c" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_Y_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 570 "../../ntpd/ntp_parser.c" /* yacc.c:358  */

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
#define YYFINAL  214
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   655

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  203
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  105
/* YYNRULES -- Number of rules.  */
#define YYNRULES  317
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  423

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   452

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
     199,   200,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   198,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   201,     2,   202,     2,     2,     2,     2,
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
     195,   196,   197
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   370,   370,   374,   375,   376,   391,   392,   393,   394,
     395,   396,   397,   398,   399,   400,   401,   402,   403,   404,
     412,   422,   423,   424,   425,   426,   430,   431,   436,   441,
     443,   449,   450,   458,   459,   460,   464,   469,   470,   471,
     472,   473,   474,   475,   476,   480,   482,   487,   488,   489,
     490,   491,   492,   496,   501,   510,   520,   521,   531,   533,
     535,   537,   548,   555,   557,   562,   564,   566,   568,   570,
     579,   585,   586,   594,   596,   608,   609,   610,   611,   612,
     621,   626,   631,   639,   641,   643,   648,   649,   650,   651,
     652,   653,   657,   658,   659,   660,   669,   671,   680,   690,
     695,   703,   704,   705,   706,   707,   708,   709,   710,   715,
     716,   724,   734,   743,   758,   763,   764,   768,   769,   773,
     774,   775,   776,   777,   778,   779,   788,   792,   796,   804,
     812,   820,   835,   850,   863,   864,   872,   873,   874,   875,
     876,   877,   878,   879,   880,   881,   882,   883,   884,   885,
     886,   890,   895,   903,   908,   909,   910,   914,   919,   927,
     932,   933,   934,   935,   936,   937,   938,   939,   947,   957,
     962,   970,   972,   974,   983,   985,   990,   991,   995,   996,
     997,   998,  1006,  1011,  1016,  1024,  1029,  1030,  1031,  1040,
    1042,  1047,  1052,  1060,  1062,  1079,  1080,  1081,  1082,  1083,
    1084,  1088,  1089,  1090,  1091,  1092,  1093,  1101,  1106,  1111,
    1119,  1124,  1125,  1126,  1127,  1128,  1129,  1130,  1131,  1132,
    1133,  1142,  1143,  1144,  1151,  1158,  1165,  1181,  1200,  1202,
    1204,  1206,  1208,  1210,  1217,  1222,  1223,  1224,  1228,  1232,
    1241,  1242,  1246,  1247,  1248,  1252,  1263,  1277,  1289,  1294,
    1296,  1301,  1302,  1310,  1312,  1320,  1325,  1333,  1358,  1365,
    1375,  1376,  1380,  1381,  1382,  1383,  1387,  1388,  1389,  1393,
    1398,  1403,  1411,  1412,  1413,  1414,  1415,  1416,  1417,  1427,
    1432,  1440,  1445,  1453,  1455,  1459,  1464,  1469,  1477,  1482,
    1490,  1499,  1500,  1504,  1505,  1514,  1532,  1536,  1541,  1549,
    1554,  1555,  1559,  1564,  1572,  1577,  1582,  1587,  1592,  1600,
    1605,  1610,  1618,  1623,  1624,  1625,  1626,  1627
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
  "T_NtpSignDsocket", "T_Orphan", "T_Orphanwait", "T_PCEdigest", "T_Panic",
  "T_Peer", "T_Peerstats", "T_Phone", "T_Pid", "T_Pidfile", "T_Pool",
  "T_Port", "T_Preempt", "T_Prefer", "T_Protostats", "T_Pw", "T_Randfile",
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
     445,   446,   447,   448,   449,   450,   451,   452,    61,    40,
      41,   123,   125
};
# endif

#define YYPACT_NINF -189

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-189)))

#define YYTABLE_NINF -7

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      20,  -174,   -32,  -189,  -189,  -189,   -29,  -189,   315,     3,
    -133,  -189,   315,  -189,   119,   -48,  -189,  -126,  -189,  -118,
    -115,  -189,  -189,  -113,  -189,  -189,   -48,    -5,   374,   -48,
    -189,  -189,  -105,  -189,  -100,  -189,  -189,     1,    81,    46,
       2,   -31,  -189,  -189,   -90,   119,   -88,  -189,   148,   380,
     -81,   -53,    23,  -189,  -189,  -189,    87,   207,  -106,  -189,
     -48,  -189,   -48,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,    -9,    29,   -62,   -61,  -189,    -7,  -189,
    -189,  -102,  -189,  -189,  -189,    32,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,   315,  -189,
    -189,  -189,  -189,  -189,  -189,     3,  -189,    42,    78,  -189,
     315,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,   112,  -189,   -39,   367,  -189,  -189,  -189,
    -113,  -189,  -189,   -48,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,   374,  -189,    60,   -48,  -189,  -189,   -34,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,    81,  -189,
    -189,    97,   102,  -189,  -189,    44,  -189,  -189,  -189,  -189,
     -31,  -189,    71,   -63,  -189,   119,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,   148,  -189,
      -9,  -189,  -189,   -36,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,   380,  -189,    75,    -9,  -189,  -189,    76,   -53,
    -189,  -189,  -189,    77,  -189,   -43,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,     0,  -150,
    -189,  -189,  -189,  -189,  -189,    89,  -189,    -6,  -189,  -189,
    -189,  -189,    39,    13,  -189,  -189,  -189,  -189,    15,    99,
    -189,  -189,   112,  -189,    -9,   -36,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,   475,  -189,  -189,   475,   475,   -81,
    -189,  -189,    18,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,   -58,   144,  -189,  -189,  -189,   353,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,   -96,    -3,   -13,
    -189,  -189,  -189,  -189,    27,  -189,  -189,    11,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,   475,   475,  -189,   163,   -81,   133,  -189,
     141,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
     -52,  -189,    45,     5,    19,  -125,  -189,     8,  -189,    -9,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,   475,
    -189,  -189,  -189,  -189,    14,  -189,  -189,  -189,   -48,  -189,
    -189,  -189,    22,  -189,  -189,  -189,    21,    24,    -9,    36,
    -164,  -189,    33,    -9,  -189,  -189,  -189,    10,    69,  -189,
    -189,  -189,  -189,  -189,    31,    48,    40,  -189,    52,  -189,
      -9,  -189,  -189
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       0,     0,     0,    24,    58,   235,     0,    71,     0,     0,
     247,   238,     0,   228,     0,     0,   240,     0,   260,     0,
       0,   241,   239,     0,   242,    25,     0,     0,     0,     0,
     261,   236,     0,    23,     0,   243,    22,     0,     0,     0,
       0,     0,   244,    21,     0,     0,     0,   237,     0,     0,
       0,     0,     0,    56,    57,   296,     0,     2,     0,     7,
       0,     8,     0,     9,    10,    13,    11,    12,    14,    15,
      16,    17,    18,     0,     0,     0,     0,   221,     0,   222,
      19,     0,     5,    62,    63,    64,   195,   196,   197,   198,
     201,   199,   200,   202,   203,   204,   205,   206,   190,   192,
     193,   194,   154,   155,   156,   126,   152,     0,   245,   229,
     189,   101,   102,   103,   104,   108,   105,   106,   107,   109,
      29,    30,    28,     0,    26,     0,     6,    65,    66,   257,
     230,   256,   289,    59,    61,   160,   161,   162,   163,   164,
     165,   166,   167,   127,   158,     0,    60,    70,   287,   231,
      67,   272,   273,   274,   275,   276,   277,   278,   269,   271,
     134,    29,    30,   134,   134,    26,    68,   188,   186,   187,
     182,   184,     0,     0,   232,    96,   100,    97,   211,   212,
     213,   214,   215,   216,   217,   218,   219,   220,   207,   209,
       0,    91,    86,     0,    87,    95,    93,    94,    92,    90,
      88,    89,    80,    82,     0,     0,   251,   283,     0,    69,
     282,   284,   280,   234,     1,     0,     4,    31,    55,   294,
     293,   223,   224,   225,   226,   268,   267,   266,     0,     0,
      79,    75,    76,    77,    78,     0,    72,     0,   191,   151,
     153,   246,    98,     0,   178,   179,   180,   181,     0,     0,
     176,   177,   168,   170,     0,     0,    27,   227,   255,   288,
     157,   159,   286,   270,   130,   134,   134,   133,   128,     0,
     183,   185,     0,    99,   208,   210,   292,   290,   291,    85,
      81,    83,    84,   233,     0,   281,   279,     3,    20,   262,
     263,   264,   259,   265,   258,   300,   301,     0,     0,     0,
      74,    73,   118,   117,     0,   115,   116,     0,   110,   113,
     114,   174,   175,   173,   169,   171,   172,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,   148,
     149,   150,   135,   131,   132,   134,   250,     0,     0,   252,
       0,    37,    38,    39,    54,    47,    49,    48,    51,    40,
      41,    42,    43,    50,    52,    44,    32,    33,    36,    34,
       0,    35,     0,     0,     0,     0,   303,     0,   298,     0,
     111,   125,   121,   123,   119,   120,   122,   124,   112,   129,
     249,   248,   254,   253,     0,    45,    46,    53,     0,   297,
     295,   302,     0,   299,   285,   306,     0,     0,     0,     0,
       0,   308,     0,     0,   304,   307,   305,     0,     0,   313,
     314,   315,   316,   317,     0,     0,     0,   309,     0,   311,
       0,   310,   312
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -189,  -189,  -189,   -41,  -189,  -189,   -15,   -38,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,    16,  -189,  -189,  -189,
    -189,   -35,  -189,  -189,  -189,  -189,  -189,  -189,  -157,  -189,
    -189,   138,  -189,  -189,   106,  -189,  -189,  -189,    -2,  -189,
    -189,  -189,  -189,    83,  -189,  -189,   239,   -79,  -189,  -189,
    -189,  -189,    66,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,   126,  -189,  -189,  -189,  -189,
    -189,  -189,   101,  -189,  -189,    51,  -189,  -189,   242,    17,
    -188,  -189,  -189,  -189,   -24,  -189,  -189,   -97,  -189,  -189,
    -189,  -123,  -189,  -130,  -189
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    56,    57,    58,    59,    60,   132,   124,   125,   288,
     356,   357,   358,   359,   360,   361,   362,    61,    62,    63,
      64,    85,   236,   237,    65,   202,   203,   204,   205,    66,
     175,   119,   242,   308,   309,   310,   378,    67,   264,   332,
     105,   106,   107,   143,   144,   145,    68,   252,   253,   254,
     255,    69,   170,   171,   172,    70,    98,    99,   100,   101,
      71,   188,   189,   190,    72,    73,    74,    75,    76,   109,
     174,   381,   283,   339,   130,   131,    77,    78,   294,   228,
      79,   158,   159,   213,   209,   210,   211,   149,   133,   279,
     221,    80,    81,   297,   298,   299,   365,   366,   397,   367,
     400,   401,   414,   415,   416
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     123,   165,   275,   276,   337,   289,   267,   268,   207,   385,
     176,   167,   206,    82,   102,   371,   215,   282,   120,   238,
     121,     1,   363,   219,   164,   277,   108,   225,   399,    83,
       2,   238,    84,   126,     3,     4,     5,   372,   404,   295,
     296,   127,     6,     7,   128,   217,   129,   218,   226,     8,
       9,   363,   220,    10,   147,    11,   134,    12,    13,   148,
     230,    14,   150,   166,   168,   290,   315,   291,   302,   173,
      15,   177,   227,   160,    16,   338,   303,   390,   122,   304,
      17,   216,    18,   231,   212,   257,   232,   214,   151,   152,
     222,    19,    20,   295,   296,    21,    22,   223,   224,   229,
      23,    24,   103,   240,    25,    26,   153,   104,   333,   334,
     241,   122,   161,    27,   162,   243,   373,   305,   259,   169,
     256,   261,   386,   374,   265,   262,    28,    29,    30,   266,
     269,   259,   271,    31,   278,   272,   281,   284,   286,   111,
     273,   375,    32,   112,   287,   154,   208,    33,   306,    34,
     300,    35,    36,   301,   178,   244,   245,   246,   247,   292,
     313,    37,    38,    39,    40,    41,    42,    43,    44,   233,
     234,    45,   311,    46,   312,   155,   235,   336,   379,   179,
     340,   393,    47,   293,   368,   369,   370,    48,    49,    50,
     380,    51,    52,   376,   383,   163,   377,   180,    53,    54,
     181,   113,   384,   388,   387,   122,   389,    -6,    55,   392,
     402,   408,   307,   396,   394,   407,   399,     2,   280,   398,
     406,     3,     4,     5,   409,   410,   411,   412,   413,     6,
       7,   335,   422,   417,   403,   419,     8,     9,   420,   421,
      10,   156,    11,   239,    12,    13,   157,   114,    14,   260,
     314,   110,   248,   270,   274,   115,   258,    15,   116,   263,
     285,    16,   409,   410,   411,   412,   413,    17,   391,    18,
     249,   146,   316,   364,   182,   250,   251,   405,    19,    20,
     117,     0,    21,    22,   418,   118,     0,    23,    24,     0,
       0,    25,    26,     0,     0,     0,     0,     0,     0,   382,
      27,     0,   183,   184,   185,   186,     0,     0,     0,     0,
     187,     0,     0,    28,    29,    30,     0,     0,     0,     0,
      31,     0,     0,    86,     0,     0,     0,    87,     0,    32,
       0,     0,     0,    88,    33,     0,    34,     0,    35,    36,
       0,     0,     0,     0,     0,     0,     0,     0,    37,    38,
      39,    40,    41,    42,    43,    44,     0,     0,    45,     0,
      46,     0,   341,     0,     0,     0,     0,     0,     0,    47,
     342,     0,     0,   395,    48,    49,    50,     2,    51,    52,
       0,     3,     4,     5,    89,    53,    54,     0,     0,     6,
       7,     0,     0,   191,    -6,    55,     8,     9,     0,   192,
      10,   193,    11,     0,    12,    13,   343,   344,    14,     0,
       0,     0,     0,     0,     0,     0,     0,    15,    90,    91,
       0,    16,     0,   345,     0,     0,     0,    17,   194,    18,
     135,   136,   137,   138,     0,    92,     0,     0,    19,    20,
      93,     0,    21,    22,     0,   346,     0,    23,    24,     0,
       0,    25,    26,   347,     0,   348,     0,     0,     0,     0,
      27,   139,     0,   140,     0,   141,     0,    94,   195,   349,
     196,   142,     0,    28,    29,    30,   197,     0,   198,     0,
      31,   199,     0,     0,     0,     0,     0,   350,   351,    32,
      95,    96,    97,     0,    33,     0,    34,     0,    35,    36,
       0,     0,     0,   200,   201,     0,     0,     0,    37,    38,
      39,    40,    41,    42,    43,    44,     0,     0,    45,     0,
      46,     0,   317,   352,     0,   353,     0,     0,     0,    47,
     318,     0,     0,   354,    48,    49,    50,   355,    51,    52,
       0,     0,     0,     0,     0,    53,    54,     0,   319,   320,
       0,     0,   321,     0,     0,    55,     0,     0,   322,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   323,   324,     0,     0,   325,
     326,     0,   327,   328,   329,     0,   330,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   331
};

static const yytype_int16 yycheck[] =
{
      15,    39,   190,    39,    62,     5,   163,   164,    61,    61,
      45,    42,    50,   187,    11,     4,    57,   205,    66,    98,
      68,     1,   147,    32,    39,    61,   159,    34,   192,    61,
      10,   110,    61,   159,    14,    15,    16,    26,   202,   189,
     190,   159,    22,    23,   159,    60,   159,    62,    55,    29,
      30,   147,    61,    33,   159,    35,    61,    37,    38,   159,
      28,    41,    61,    61,    95,    65,   254,    67,    29,   159,
      50,   159,    79,    27,    54,   133,    37,   202,   159,    40,
      60,   187,    62,    51,    61,   126,    54,     0,     7,     8,
      61,    71,    72,   189,   190,    75,    76,   159,   159,   201,
      80,    81,    99,    61,    84,    85,    25,   104,   265,   266,
      32,   159,    66,    93,    68,     3,   105,    78,   133,   150,
     159,    61,   174,   112,    27,   159,   106,   107,   108,    27,
      86,   146,    61,   113,   170,   198,    61,    61,    61,    20,
     175,   130,   122,    24,   187,    64,   199,   127,   109,   129,
      61,   131,   132,   159,     6,    43,    44,    45,    46,   159,
      61,   141,   142,   143,   144,   145,   146,   147,   148,   137,
     138,   151,   159,   153,   159,    94,   144,   159,   335,    31,
      36,   369,   162,   183,   187,   198,   159,   167,   168,   169,
      27,   171,   172,   182,    61,   149,   185,    49,   178,   179,
      52,    82,    61,   198,   159,   159,   187,   187,   188,   201,
     398,   201,   173,   191,   200,   403,   192,    10,   202,   198,
     187,    14,    15,    16,   193,   194,   195,   196,   197,    22,
      23,   269,   420,   202,   198,   187,    29,    30,   198,   187,
      33,   160,    35,   105,    37,    38,   165,   128,    41,   143,
     252,    12,   140,   170,   188,   136,   130,    50,   139,   158,
     209,    54,   193,   194,   195,   196,   197,    60,   365,    62,
     158,    29,   255,   297,   126,   163,   164,   400,    71,    72,
     161,    -1,    75,    76,   414,   166,    -1,    80,    81,    -1,
      -1,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,   337,
      93,    -1,   154,   155,   156,   157,    -1,    -1,    -1,    -1,
     162,    -1,    -1,   106,   107,   108,    -1,    -1,    -1,    -1,
     113,    -1,    -1,     8,    -1,    -1,    -1,    12,    -1,   122,
      -1,    -1,    -1,    18,   127,    -1,   129,    -1,   131,   132,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   141,   142,
     143,   144,   145,   146,   147,   148,    -1,    -1,   151,    -1,
     153,    -1,     9,    -1,    -1,    -1,    -1,    -1,    -1,   162,
      17,    -1,    -1,   388,   167,   168,   169,    10,   171,   172,
      -1,    14,    15,    16,    69,   178,   179,    -1,    -1,    22,
      23,    -1,    -1,    13,   187,   188,    29,    30,    -1,    19,
      33,    21,    35,    -1,    37,    38,    53,    54,    41,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    50,   103,   104,
      -1,    54,    -1,    70,    -1,    -1,    -1,    60,    48,    62,
      56,    57,    58,    59,    -1,   120,    -1,    -1,    71,    72,
     125,    -1,    75,    76,    -1,    92,    -1,    80,    81,    -1,
      -1,    84,    85,   100,    -1,   102,    -1,    -1,    -1,    -1,
      93,    87,    -1,    89,    -1,    91,    -1,   152,    88,   116,
      90,    97,    -1,   106,   107,   108,    96,    -1,    98,    -1,
     113,   101,    -1,    -1,    -1,    -1,    -1,   134,   135,   122,
     175,   176,   177,    -1,   127,    -1,   129,    -1,   131,   132,
      -1,    -1,    -1,   123,   124,    -1,    -1,    -1,   141,   142,
     143,   144,   145,   146,   147,   148,    -1,    -1,   151,    -1,
     153,    -1,    47,   170,    -1,   172,    -1,    -1,    -1,   162,
      55,    -1,    -1,   180,   167,   168,   169,   184,   171,   172,
      -1,    -1,    -1,    -1,    -1,   178,   179,    -1,    73,    74,
      -1,    -1,    77,    -1,    -1,   188,    -1,    -1,    83,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   110,   111,    -1,    -1,   114,
     115,    -1,   117,   118,   119,    -1,   121,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   180
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,     1,    10,    14,    15,    16,    22,    23,    29,    30,
      33,    35,    37,    38,    41,    50,    54,    60,    62,    71,
      72,    75,    76,    80,    81,    84,    85,    93,   106,   107,
     108,   113,   122,   127,   129,   131,   132,   141,   142,   143,
     144,   145,   146,   147,   148,   151,   153,   162,   167,   168,
     169,   171,   172,   178,   179,   188,   204,   205,   206,   207,
     208,   220,   221,   222,   223,   227,   232,   240,   249,   254,
     258,   263,   267,   268,   269,   270,   271,   279,   280,   283,
     294,   295,   187,    61,    61,   224,     8,    12,    18,    69,
     103,   104,   120,   125,   152,   175,   176,   177,   259,   260,
     261,   262,    11,    99,   104,   243,   244,   245,   159,   272,
     259,    20,    24,    82,   128,   136,   139,   161,   166,   234,
      66,    68,   159,   209,   210,   211,   159,   159,   159,   159,
     277,   278,   209,   291,    61,    56,    57,    58,    59,    87,
      89,    91,    97,   246,   247,   248,   291,   159,   159,   290,
      61,     7,     8,    25,    64,    94,   160,   165,   284,   285,
      27,    66,    68,   149,   209,   210,    61,    42,    95,   150,
     255,   256,   257,   159,   273,   233,   234,   159,     6,    31,
      49,    52,   126,   154,   155,   156,   157,   162,   264,   265,
     266,    13,    19,    21,    48,    88,    90,    96,    98,   101,
     123,   124,   228,   229,   230,   231,   210,    61,   199,   287,
     288,   289,    61,   286,     0,   206,   187,   209,   209,    32,
      61,   293,    61,   159,   159,    34,    55,    79,   282,   201,
      28,    51,    54,   137,   138,   144,   225,   226,   260,   244,
      61,    32,   235,     3,    43,    44,    45,    46,   140,   158,
     163,   164,   250,   251,   252,   253,   159,   206,   278,   209,
     247,    61,   159,   285,   241,    27,    27,   241,   241,    86,
     256,    61,   198,   234,   265,   293,    39,    61,   170,   292,
     229,    61,   293,   275,    61,   288,    61,   187,   212,     5,
      65,    67,   159,   183,   281,   189,   190,   296,   297,   298,
      61,   159,    29,    37,    40,    78,   109,   173,   236,   237,
     238,   159,   159,    61,   251,   293,   292,    47,    55,    73,
      74,    77,    83,   110,   111,   114,   115,   117,   118,   119,
     121,   180,   242,   241,   241,   210,   159,    62,   133,   276,
      36,     9,    17,    53,    54,    70,    92,   100,   102,   116,
     134,   135,   170,   172,   180,   184,   213,   214,   215,   216,
     217,   218,   219,   147,   297,   299,   300,   302,   187,   198,
     159,     4,    26,   105,   112,   130,   182,   185,   239,   241,
      27,   274,   210,    61,    61,    61,   174,   159,   198,   187,
     202,   300,   201,   293,   200,   209,   191,   301,   198,   192,
     303,   304,   293,   198,   202,   304,   187,   293,   201,   193,
     194,   195,   196,   197,   305,   306,   307,   202,   306,   187,
     198,   187,   293
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   203,   204,   205,   205,   205,   206,   206,   206,   206,
     206,   206,   206,   206,   206,   206,   206,   206,   206,   206,
     207,   208,   208,   208,   208,   208,   209,   209,   210,   211,
     211,   212,   212,   213,   213,   213,   214,   215,   215,   215,
     215,   215,   215,   215,   215,   216,   216,   217,   217,   217,
     217,   217,   217,   218,   219,   220,   221,   221,   222,   222,
     222,   222,   223,   223,   223,   223,   223,   223,   223,   223,
     223,   224,   224,   225,   225,   226,   226,   226,   226,   226,
     227,   228,   228,   229,   229,   229,   230,   230,   230,   230,
     230,   230,   231,   231,   231,   231,   232,   232,   232,   233,
     233,   234,   234,   234,   234,   234,   234,   234,   234,   235,
     235,   236,   236,   236,   236,   237,   237,   238,   238,   239,
     239,   239,   239,   239,   239,   239,   240,   240,   240,   240,
     240,   240,   240,   240,   241,   241,   242,   242,   242,   242,
     242,   242,   242,   242,   242,   242,   242,   242,   242,   242,
     242,   243,   243,   244,   245,   245,   245,   246,   246,   247,
     248,   248,   248,   248,   248,   248,   248,   248,   249,   250,
     250,   251,   251,   251,   251,   251,   252,   252,   253,   253,
     253,   253,   254,   255,   255,   256,   257,   257,   257,   258,
     258,   259,   259,   260,   260,   261,   261,   261,   261,   261,
     261,   262,   262,   262,   262,   262,   262,   263,   264,   264,
     265,   266,   266,   266,   266,   266,   266,   266,   266,   266,
     266,   267,   267,   267,   267,   267,   267,   267,   267,   267,
     267,   267,   267,   267,   267,   268,   268,   268,   269,   269,
     270,   270,   271,   271,   271,   272,   272,   272,   273,   274,
     274,   275,   275,   276,   276,   277,   277,   278,   279,   279,
     280,   280,   281,   281,   281,   281,   282,   282,   282,   283,
     284,   284,   285,   285,   285,   285,   285,   285,   285,   286,
     286,   287,   287,   288,   288,   289,   290,   290,   291,   291,
     292,   292,   292,   293,   293,   294,   295,   296,   296,   297,
     298,   298,   299,   299,   300,   301,   302,   303,   303,   304,
     305,   305,   306,   307,   307,   307,   307,   307
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
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
       1,     1,     1,     1,     1,     1,     1,     2,     2,     1,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     2,     2,     2,     3,     1,     2,
       2,     2,     2,     3,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     0,     4,     1,
       0,     0,     2,     2,     2,     2,     1,     1,     3,     3,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       1,     2,     1,     1,     1,     5,     2,     1,     2,     1,
       1,     1,     1,     1,     1,     5,     1,     3,     2,     3,
       1,     1,     2,     1,     5,     4,     3,     2,     1,     6,
       3,     2,     3,     1,     1,     1,     1,     1
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
#line 377 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2123 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 20:
#line 413 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			peer_node *my_node;

			my_node = create_peer_node((yyvsp[-2].Integer), (yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.peers, my_node);
		}
#line 2134 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 27:
#line 432 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = create_address_node((yyvsp[0].String), (yyvsp[-1].Integer)); }
#line 2140 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 28:
#line 437 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = create_address_node((yyvsp[0].String), AF_UNSPEC); }
#line 2146 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 29:
#line 442 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = AF_INET; }
#line 2152 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 30:
#line 444 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = AF_INET6; }
#line 2158 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 31:
#line 449 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2164 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 32:
#line 451 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2173 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 36:
#line 465 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2179 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 45:
#line 481 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2185 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 46:
#line 483 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_uval((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2191 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 53:
#line 497 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2197 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 55:
#line 511 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			unpeer_node *my_node;

			my_node = create_unpeer_node((yyvsp[0].Address_node));
			if (my_node)
				APPEND_G_FIFO(cfgt.unpeers, my_node);
		}
#line 2209 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 58:
#line 532 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.broadcastclient = 1; }
#line 2215 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 59:
#line 534 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.manycastserver, (yyvsp[0].Address_fifo)); }
#line 2221 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 60:
#line 536 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.multicastclient, (yyvsp[0].Address_fifo)); }
#line 2227 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 61:
#line 538 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.mdnstries = (yyvsp[0].Integer); }
#line 2233 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 62:
#line 549 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *atrv;

			atrv = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			APPEND_G_FIFO(cfgt.vars, atrv);
		}
#line 2244 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 63:
#line 556 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.control_key = (yyvsp[0].Integer); }
#line 2250 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 64:
#line 558 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			cfgt.auth.cryptosw++;
			CONCAT_G_FIFOS(cfgt.auth.crypto_cmd_list, (yyvsp[0].Attr_val_fifo));
		}
#line 2259 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 65:
#line 563 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.keys = (yyvsp[0].String); }
#line 2265 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 66:
#line 565 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.keysdir = (yyvsp[0].String); }
#line 2271 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 67:
#line 567 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.request_key = (yyvsp[0].Integer); }
#line 2277 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 68:
#line 569 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.revoke = (yyvsp[0].Integer); }
#line 2283 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 69:
#line 571 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			cfgt.auth.trusted_key_list = (yyvsp[0].Attr_val_fifo);

			// if (!cfgt.auth.trusted_key_list)
			// 	cfgt.auth.trusted_key_list = $2;
			// else
			// 	LINK_SLIST(cfgt.auth.trusted_key_list, $2, link);
		}
#line 2296 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 70:
#line 580 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.ntp_signd_socket = (yyvsp[0].String); }
#line 2302 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 71:
#line 585 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2308 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 72:
#line 587 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2317 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 73:
#line 595 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2323 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 74:
#line 597 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val) = NULL;
			cfgt.auth.revoke = (yyvsp[0].Integer);
			msyslog(LOG_WARNING,
				"'crypto revoke %d' is deprecated, "
				"please use 'revoke %d' instead.",
				cfgt.auth.revoke, cfgt.auth.revoke);
		}
#line 2336 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 80:
#line 622 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.orphan_cmds, (yyvsp[0].Attr_val_fifo)); }
#line 2342 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 81:
#line 627 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2351 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 82:
#line 632 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2360 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 83:
#line 640 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2366 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 84:
#line 642 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2372 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 85:
#line 644 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2378 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 96:
#line 670 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.stats_list, (yyvsp[0].Int_fifo)); }
#line 2384 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 97:
#line 672 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				cfgt.stats_dir = (yyvsp[0].String);
			} else {
				YYFREE((yyvsp[0].String));
				yyerror("statsdir remote configuration ignored");
			}
		}
#line 2397 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 98:
#line 681 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			filegen_node *fgn;

			fgn = create_filegen_node((yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.filegen_opts, fgn);
		}
#line 2408 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 99:
#line 691 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2417 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 100:
#line 696 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2426 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 109:
#line 715 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2432 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 110:
#line 717 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2441 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 111:
#line 725 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			} else {
				(yyval.Attr_val) = NULL;
				YYFREE((yyvsp[0].String));
				yyerror("filegen file remote config ignored");
			}
		}
#line 2455 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 112:
#line 735 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror("filegen type remote config ignored");
			}
		}
#line 2468 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 113:
#line 744 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2487 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 114:
#line 759 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2493 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 126:
#line 789 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			CONCAT_G_FIFOS(cfgt.discard_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2501 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 127:
#line 793 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			CONCAT_G_FIFOS(cfgt.mru_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2509 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 128:
#line 797 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[-1].Address_node), NULL, (yyvsp[0].Int_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2521 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 129:
#line 805 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[-3].Address_node), (yyvsp[-1].Address_node), (yyvsp[0].Int_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2533 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 130:
#line 813 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node(NULL, NULL, (yyvsp[0].Int_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2545 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 131:
#line 821 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node(
				create_address_node(
					estrdup("0.0.0.0"),
					AF_INET),
				create_address_node(
					estrdup("0.0.0.0"),
					AF_INET),
				(yyvsp[0].Int_fifo),
				lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2564 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 132:
#line 836 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node(
				create_address_node(
					estrdup("::"),
					AF_INET6),
				create_address_node(
					estrdup("::"),
					AF_INET6),
				(yyvsp[0].Int_fifo),
				lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2583 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 133:
#line 851 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *	rn;

			APPEND_G_FIFO((yyvsp[0].Int_fifo), create_int_node((yyvsp[-1].Integer)));
			rn = create_restrict_node(
				NULL, NULL, (yyvsp[0].Int_fifo), lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2596 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 134:
#line 863 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Int_fifo) = NULL; }
#line 2602 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 135:
#line 865 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2611 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 151:
#line 891 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2620 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 152:
#line 896 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2629 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 153:
#line 904 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2635 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 157:
#line 915 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2644 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 158:
#line 920 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2653 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 159:
#line 928 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2659 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 168:
#line 948 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.fudge, aon);
		}
#line 2670 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 169:
#line 958 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2679 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 170:
#line 963 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2688 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 171:
#line 971 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2694 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 172:
#line 973 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2700 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 173:
#line 975 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if ((yyvsp[0].Integer) >= 0 && (yyvsp[0].Integer) <= 16) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror("fudge factor: stratum value not in [0..16], ignored");
			}
		}
#line 2713 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 174:
#line 984 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2719 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 175:
#line 986 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2725 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 182:
#line 1007 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.rlimit, (yyvsp[0].Attr_val_fifo)); }
#line 2731 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 183:
#line 1012 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2740 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 184:
#line 1017 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2749 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 185:
#line 1025 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2755 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 189:
#line 1041 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.enable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 2761 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 190:
#line 1043 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.disable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 2767 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 191:
#line 1048 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2776 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 192:
#line 1053 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2785 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 193:
#line 1061 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2791 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 194:
#line 1063 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2809 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 207:
#line 1102 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.tinker, (yyvsp[0].Attr_val_fifo)); }
#line 2815 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 208:
#line 1107 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2824 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 209:
#line 1112 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2833 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 210:
#line 1120 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2839 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 223:
#line 1145 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;

			av = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2850 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 224:
#line 1152 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;

			av = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2861 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 225:
#line 1159 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;

			av = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2872 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 226:
#line 1166 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2892 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 227:
#line 1182 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2915 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 228:
#line 1201 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { lex_flush_stack(); }
#line 2921 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 229:
#line 1203 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { /* see drift_parm below for actions */ }
#line 2927 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 230:
#line 1205 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.logconfig, (yyvsp[0].Attr_val_fifo)); }
#line 2933 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 231:
#line 1207 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.phone, (yyvsp[0].String_fifo)); }
#line 2939 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 232:
#line 1209 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { APPEND_G_FIFO(cfgt.setvar, (yyvsp[0].Set_var)); }
#line 2945 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 233:
#line 1211 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.trap, aon);
		}
#line 2956 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 234:
#line 1218 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.ttl, (yyvsp[0].Attr_val_fifo)); }
#line 2962 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 239:
#line 1233 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
#ifndef LEAP_SMEAR
			yyerror("Built without LEAP_SMEAR support.");
#endif
		}
#line 2972 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 245:
#line 1253 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2987 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 246:
#line 1264 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				attr_val *av;
				av = create_attr_sval(T_Driftfile, (yyvsp[-1].String));
				APPEND_G_FIFO(cfgt.vars, av);
				av = create_attr_dval(T_WanderThreshold, (yyvsp[0].Double));
				APPEND_G_FIFO(cfgt.vars, av);
			} else {
				YYFREE((yyvsp[-1].String));
				yyerror("driftfile remote configuration ignored");
			}
		}
#line 3004 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 247:
#line 1277 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				attr_val *av;
				av = create_attr_sval(T_Driftfile, estrdup(""));
				APPEND_G_FIFO(cfgt.vars, av);
			} else {
				yyerror("driftfile remote configuration ignored");
			}
		}
#line 3018 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 248:
#line 1290 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Set_var) = create_setvar_node((yyvsp[-3].String), (yyvsp[-1].String), (yyvsp[0].Integer)); }
#line 3024 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 250:
#line 1296 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 0; }
#line 3030 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 251:
#line 1301 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 3036 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 252:
#line 1303 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3045 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 253:
#line 1311 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 3051 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 254:
#line 1313 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), estrdup((yyvsp[0].Address_node)->address));
			destroy_address_node((yyvsp[0].Address_node));
		}
#line 3060 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 255:
#line 1321 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3069 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 256:
#line 1326 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3078 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 257:
#line 1334 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 3104 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 258:
#line 1359 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node((yyvsp[0].Integer), NULL, (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3115 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 259:
#line 1366 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node(0, (yyvsp[0].String), (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3126 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 269:
#line 1394 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.reset_counters, (yyvsp[0].Int_fifo)); }
#line 3132 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 270:
#line 1399 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3141 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 271:
#line 1404 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3150 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 279:
#line 1428 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3159 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 280:
#line 1433 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3168 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 281:
#line 1441 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3177 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 282:
#line 1446 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3186 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 283:
#line 1454 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival('i', (yyvsp[0].Integer)); }
#line 3192 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 285:
#line 1460 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_rangeval('-', (yyvsp[-3].Integer), (yyvsp[-1].Integer)); }
#line 3198 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 286:
#line 1465 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.String_fifo) = (yyvsp[-1].String_fifo);
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3207 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 287:
#line 1470 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.String_fifo) = NULL;
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3216 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 288:
#line 1478 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Address_fifo) = (yyvsp[-1].Address_fifo);
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3225 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 289:
#line 1483 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Address_fifo) = NULL;
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3234 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 290:
#line 1491 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if ((yyvsp[0].Integer) != 0 && (yyvsp[0].Integer) != 1) {
				yyerror("Integer value is not boolean (0 or 1). Assuming 1");
				(yyval.Integer) = 1;
			} else {
				(yyval.Integer) = (yyvsp[0].Integer);
			}
		}
#line 3247 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 291:
#line 1499 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 1; }
#line 3253 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 292:
#line 1500 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 0; }
#line 3259 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 293:
#line 1504 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Double) = (double)(yyvsp[0].Integer); }
#line 3265 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 295:
#line 1515 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			sim_node *sn;

			sn =  create_sim_node((yyvsp[-2].Attr_val_fifo), (yyvsp[-1].Sim_server_fifo));
			APPEND_G_FIFO(cfgt.sim_details, sn);

			/* Revert from ; to \n for end-of-command */
			old_config_style = 1;
		}
#line 3279 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 296:
#line 1532 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { old_config_style = 0; }
#line 3285 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 297:
#line 1537 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3294 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 298:
#line 1542 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3303 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 299:
#line 1550 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3309 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 302:
#line 1560 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_server_fifo) = (yyvsp[-1].Sim_server_fifo);
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3318 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 303:
#line 1565 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_server_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3327 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 304:
#line 1573 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Sim_server) = ONLY_SIM(create_sim_server((yyvsp[-4].Address_node), (yyvsp[-2].Double), (yyvsp[-1].Sim_script_fifo))); }
#line 3333 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 305:
#line 1578 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Double) = (yyvsp[-1].Double); }
#line 3339 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 306:
#line 1583 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = (yyvsp[0].Address_node); }
#line 3345 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 307:
#line 1588 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_script_fifo) = (yyvsp[-1].Sim_script_fifo);
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3354 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 308:
#line 1593 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_script_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3363 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 309:
#line 1601 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Sim_script) = ONLY_SIM(create_sim_script_info((yyvsp[-3].Double), (yyvsp[-1].Attr_val_fifo))); }
#line 3369 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 310:
#line 1606 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3378 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 311:
#line 1611 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3387 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 312:
#line 1619 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3393 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;


#line 3397 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
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
#line 1630 "../../ntpd/ntp_parser.y" /* yacc.c:1906  */


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

