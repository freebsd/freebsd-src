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

#line 100 "ntp_parser.c" /* yacc.c:339  */

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
    T_Bcpollbstep = 268,
    T_Beacon = 269,
    T_Broadcast = 270,
    T_Broadcastclient = 271,
    T_Broadcastdelay = 272,
    T_Burst = 273,
    T_Calibrate = 274,
    T_Ceiling = 275,
    T_Clockstats = 276,
    T_Cohort = 277,
    T_ControlKey = 278,
    T_Crypto = 279,
    T_Cryptostats = 280,
    T_Ctl = 281,
    T_Day = 282,
    T_Default = 283,
    T_Digest = 284,
    T_Disable = 285,
    T_Discard = 286,
    T_Dispersion = 287,
    T_Double = 288,
    T_Driftfile = 289,
    T_Drop = 290,
    T_Dscp = 291,
    T_Ellipsis = 292,
    T_Enable = 293,
    T_End = 294,
    T_False = 295,
    T_File = 296,
    T_Filegen = 297,
    T_Filenum = 298,
    T_Flag1 = 299,
    T_Flag2 = 300,
    T_Flag3 = 301,
    T_Flag4 = 302,
    T_Flake = 303,
    T_Floor = 304,
    T_Freq = 305,
    T_Fudge = 306,
    T_Host = 307,
    T_Huffpuff = 308,
    T_Iburst = 309,
    T_Ident = 310,
    T_Ignore = 311,
    T_Incalloc = 312,
    T_Incmem = 313,
    T_Initalloc = 314,
    T_Initmem = 315,
    T_Includefile = 316,
    T_Integer = 317,
    T_Interface = 318,
    T_Intrange = 319,
    T_Io = 320,
    T_Ipv4 = 321,
    T_Ipv4_flag = 322,
    T_Ipv6 = 323,
    T_Ipv6_flag = 324,
    T_Kernel = 325,
    T_Key = 326,
    T_Keys = 327,
    T_Keysdir = 328,
    T_Kod = 329,
    T_Mssntp = 330,
    T_Leapfile = 331,
    T_Leapsmearinterval = 332,
    T_Limited = 333,
    T_Link = 334,
    T_Listen = 335,
    T_Logconfig = 336,
    T_Logfile = 337,
    T_Loopstats = 338,
    T_Lowpriotrap = 339,
    T_Manycastclient = 340,
    T_Manycastserver = 341,
    T_Mask = 342,
    T_Maxage = 343,
    T_Maxclock = 344,
    T_Maxdepth = 345,
    T_Maxdist = 346,
    T_Maxmem = 347,
    T_Maxpoll = 348,
    T_Mdnstries = 349,
    T_Mem = 350,
    T_Memlock = 351,
    T_Minclock = 352,
    T_Mindepth = 353,
    T_Mindist = 354,
    T_Minimum = 355,
    T_Minpoll = 356,
    T_Minsane = 357,
    T_Mode = 358,
    T_Mode7 = 359,
    T_Monitor = 360,
    T_Month = 361,
    T_Mru = 362,
    T_Multicastclient = 363,
    T_Nic = 364,
    T_Nolink = 365,
    T_Nomodify = 366,
    T_Nomrulist = 367,
    T_None = 368,
    T_Nonvolatile = 369,
    T_Nopeer = 370,
    T_Noquery = 371,
    T_Noselect = 372,
    T_Noserve = 373,
    T_Notrap = 374,
    T_Notrust = 375,
    T_Ntp = 376,
    T_Ntpport = 377,
    T_NtpSignDsocket = 378,
    T_Orphan = 379,
    T_Orphanwait = 380,
    T_PCEdigest = 381,
    T_Panic = 382,
    T_Peer = 383,
    T_Peerstats = 384,
    T_Phone = 385,
    T_Pid = 386,
    T_Pidfile = 387,
    T_Pool = 388,
    T_Port = 389,
    T_Preempt = 390,
    T_Prefer = 391,
    T_Protostats = 392,
    T_Pw = 393,
    T_Randfile = 394,
    T_Rawstats = 395,
    T_Refid = 396,
    T_Requestkey = 397,
    T_Reset = 398,
    T_Restrict = 399,
    T_Revoke = 400,
    T_Rlimit = 401,
    T_Saveconfigdir = 402,
    T_Server = 403,
    T_Setvar = 404,
    T_Source = 405,
    T_Stacksize = 406,
    T_Statistics = 407,
    T_Stats = 408,
    T_Statsdir = 409,
    T_Step = 410,
    T_Stepback = 411,
    T_Stepfwd = 412,
    T_Stepout = 413,
    T_Stratum = 414,
    T_String = 415,
    T_Sys = 416,
    T_Sysstats = 417,
    T_Tick = 418,
    T_Time1 = 419,
    T_Time2 = 420,
    T_Timer = 421,
    T_Timingstats = 422,
    T_Tinker = 423,
    T_Tos = 424,
    T_Trap = 425,
    T_True = 426,
    T_Trustedkey = 427,
    T_Ttl = 428,
    T_Type = 429,
    T_U_int = 430,
    T_UEcrypto = 431,
    T_UEcryptonak = 432,
    T_UEdigest = 433,
    T_Unconfig = 434,
    T_Unpeer = 435,
    T_Version = 436,
    T_WanderThreshold = 437,
    T_Week = 438,
    T_Wildcard = 439,
    T_Xleave = 440,
    T_Year = 441,
    T_Flag = 442,
    T_EOC = 443,
    T_Simulate = 444,
    T_Beep_Delay = 445,
    T_Sim_Duration = 446,
    T_Server_Offset = 447,
    T_Duration = 448,
    T_Freq_Offset = 449,
    T_Wander = 450,
    T_Jitter = 451,
    T_Prop_Delay = 452,
    T_Proc_Delay = 453
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
#define T_Bcpollbstep 268
#define T_Beacon 269
#define T_Broadcast 270
#define T_Broadcastclient 271
#define T_Broadcastdelay 272
#define T_Burst 273
#define T_Calibrate 274
#define T_Ceiling 275
#define T_Clockstats 276
#define T_Cohort 277
#define T_ControlKey 278
#define T_Crypto 279
#define T_Cryptostats 280
#define T_Ctl 281
#define T_Day 282
#define T_Default 283
#define T_Digest 284
#define T_Disable 285
#define T_Discard 286
#define T_Dispersion 287
#define T_Double 288
#define T_Driftfile 289
#define T_Drop 290
#define T_Dscp 291
#define T_Ellipsis 292
#define T_Enable 293
#define T_End 294
#define T_False 295
#define T_File 296
#define T_Filegen 297
#define T_Filenum 298
#define T_Flag1 299
#define T_Flag2 300
#define T_Flag3 301
#define T_Flag4 302
#define T_Flake 303
#define T_Floor 304
#define T_Freq 305
#define T_Fudge 306
#define T_Host 307
#define T_Huffpuff 308
#define T_Iburst 309
#define T_Ident 310
#define T_Ignore 311
#define T_Incalloc 312
#define T_Incmem 313
#define T_Initalloc 314
#define T_Initmem 315
#define T_Includefile 316
#define T_Integer 317
#define T_Interface 318
#define T_Intrange 319
#define T_Io 320
#define T_Ipv4 321
#define T_Ipv4_flag 322
#define T_Ipv6 323
#define T_Ipv6_flag 324
#define T_Kernel 325
#define T_Key 326
#define T_Keys 327
#define T_Keysdir 328
#define T_Kod 329
#define T_Mssntp 330
#define T_Leapfile 331
#define T_Leapsmearinterval 332
#define T_Limited 333
#define T_Link 334
#define T_Listen 335
#define T_Logconfig 336
#define T_Logfile 337
#define T_Loopstats 338
#define T_Lowpriotrap 339
#define T_Manycastclient 340
#define T_Manycastserver 341
#define T_Mask 342
#define T_Maxage 343
#define T_Maxclock 344
#define T_Maxdepth 345
#define T_Maxdist 346
#define T_Maxmem 347
#define T_Maxpoll 348
#define T_Mdnstries 349
#define T_Mem 350
#define T_Memlock 351
#define T_Minclock 352
#define T_Mindepth 353
#define T_Mindist 354
#define T_Minimum 355
#define T_Minpoll 356
#define T_Minsane 357
#define T_Mode 358
#define T_Mode7 359
#define T_Monitor 360
#define T_Month 361
#define T_Mru 362
#define T_Multicastclient 363
#define T_Nic 364
#define T_Nolink 365
#define T_Nomodify 366
#define T_Nomrulist 367
#define T_None 368
#define T_Nonvolatile 369
#define T_Nopeer 370
#define T_Noquery 371
#define T_Noselect 372
#define T_Noserve 373
#define T_Notrap 374
#define T_Notrust 375
#define T_Ntp 376
#define T_Ntpport 377
#define T_NtpSignDsocket 378
#define T_Orphan 379
#define T_Orphanwait 380
#define T_PCEdigest 381
#define T_Panic 382
#define T_Peer 383
#define T_Peerstats 384
#define T_Phone 385
#define T_Pid 386
#define T_Pidfile 387
#define T_Pool 388
#define T_Port 389
#define T_Preempt 390
#define T_Prefer 391
#define T_Protostats 392
#define T_Pw 393
#define T_Randfile 394
#define T_Rawstats 395
#define T_Refid 396
#define T_Requestkey 397
#define T_Reset 398
#define T_Restrict 399
#define T_Revoke 400
#define T_Rlimit 401
#define T_Saveconfigdir 402
#define T_Server 403
#define T_Setvar 404
#define T_Source 405
#define T_Stacksize 406
#define T_Statistics 407
#define T_Stats 408
#define T_Statsdir 409
#define T_Step 410
#define T_Stepback 411
#define T_Stepfwd 412
#define T_Stepout 413
#define T_Stratum 414
#define T_String 415
#define T_Sys 416
#define T_Sysstats 417
#define T_Tick 418
#define T_Time1 419
#define T_Time2 420
#define T_Timer 421
#define T_Timingstats 422
#define T_Tinker 423
#define T_Tos 424
#define T_Trap 425
#define T_True 426
#define T_Trustedkey 427
#define T_Ttl 428
#define T_Type 429
#define T_U_int 430
#define T_UEcrypto 431
#define T_UEcryptonak 432
#define T_UEdigest 433
#define T_Unconfig 434
#define T_Unpeer 435
#define T_Version 436
#define T_WanderThreshold 437
#define T_Week 438
#define T_Wildcard 439
#define T_Xleave 440
#define T_Year 441
#define T_Flag 442
#define T_EOC 443
#define T_Simulate 444
#define T_Beep_Delay 445
#define T_Sim_Duration 446
#define T_Server_Offset 447
#define T_Duration 448
#define T_Freq_Offset 449
#define T_Wander 450
#define T_Jitter 451
#define T_Prop_Delay 452
#define T_Proc_Delay 453

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

#line 555 "ntp_parser.c" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_Y_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 572 "ntp_parser.c" /* yacc.c:358  */

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
#define YYFINAL  215
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   654

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  204
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  105
/* YYNRULES -- Number of rules.  */
#define YYNRULES  318
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  424

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   453

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
     200,   201,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   199,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   202,     2,   203,     2,     2,     2,     2,
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
     195,   196,   197,   198
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   371,   371,   375,   376,   377,   392,   393,   394,   395,
     396,   397,   398,   399,   400,   401,   402,   403,   404,   405,
     413,   423,   424,   425,   426,   427,   431,   432,   437,   442,
     444,   450,   451,   459,   460,   461,   465,   470,   471,   472,
     473,   474,   475,   476,   477,   481,   483,   488,   489,   490,
     491,   492,   493,   497,   502,   511,   521,   522,   532,   534,
     536,   538,   549,   556,   558,   563,   565,   567,   569,   571,
     580,   586,   587,   595,   597,   609,   610,   611,   612,   613,
     622,   627,   632,   640,   642,   644,   649,   650,   651,   652,
     653,   654,   655,   659,   660,   661,   662,   671,   673,   682,
     692,   697,   705,   706,   707,   708,   709,   710,   711,   712,
     717,   718,   726,   736,   745,   760,   765,   766,   770,   771,
     775,   776,   777,   778,   779,   780,   781,   790,   794,   798,
     806,   814,   822,   837,   852,   865,   866,   874,   875,   876,
     877,   878,   879,   880,   881,   882,   883,   884,   885,   886,
     887,   888,   892,   897,   905,   910,   911,   912,   916,   921,
     929,   934,   935,   936,   937,   938,   939,   940,   941,   949,
     959,   964,   972,   974,   976,   985,   987,   992,   993,   997,
     998,   999,  1000,  1008,  1013,  1018,  1026,  1031,  1032,  1033,
    1042,  1044,  1049,  1054,  1062,  1064,  1081,  1082,  1083,  1084,
    1085,  1086,  1090,  1091,  1092,  1093,  1094,  1095,  1103,  1108,
    1113,  1121,  1126,  1127,  1128,  1129,  1130,  1131,  1132,  1133,
    1134,  1135,  1144,  1145,  1146,  1153,  1160,  1167,  1183,  1202,
    1204,  1206,  1208,  1210,  1212,  1219,  1224,  1225,  1226,  1230,
    1234,  1243,  1244,  1248,  1249,  1250,  1254,  1265,  1279,  1291,
    1296,  1298,  1303,  1304,  1312,  1314,  1322,  1327,  1335,  1360,
    1367,  1377,  1378,  1382,  1383,  1384,  1385,  1389,  1390,  1391,
    1395,  1400,  1405,  1413,  1414,  1415,  1416,  1417,  1418,  1419,
    1429,  1434,  1442,  1447,  1455,  1457,  1461,  1466,  1471,  1479,
    1484,  1492,  1501,  1502,  1506,  1507,  1516,  1534,  1538,  1543,
    1551,  1556,  1557,  1561,  1566,  1574,  1579,  1584,  1589,  1594,
    1602,  1607,  1612,  1620,  1625,  1626,  1627,  1628,  1629
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "T_Abbrev", "T_Age", "T_All", "T_Allan",
  "T_Allpeers", "T_Auth", "T_Autokey", "T_Automax", "T_Average",
  "T_Bclient", "T_Bcpollbstep", "T_Beacon", "T_Broadcast",
  "T_Broadcastclient", "T_Broadcastdelay", "T_Burst", "T_Calibrate",
  "T_Ceiling", "T_Clockstats", "T_Cohort", "T_ControlKey", "T_Crypto",
  "T_Cryptostats", "T_Ctl", "T_Day", "T_Default", "T_Digest", "T_Disable",
  "T_Discard", "T_Dispersion", "T_Double", "T_Driftfile", "T_Drop",
  "T_Dscp", "T_Ellipsis", "T_Enable", "T_End", "T_False", "T_File",
  "T_Filegen", "T_Filenum", "T_Flag1", "T_Flag2", "T_Flag3", "T_Flag4",
  "T_Flake", "T_Floor", "T_Freq", "T_Fudge", "T_Host", "T_Huffpuff",
  "T_Iburst", "T_Ident", "T_Ignore", "T_Incalloc", "T_Incmem",
  "T_Initalloc", "T_Initmem", "T_Includefile", "T_Integer", "T_Interface",
  "T_Intrange", "T_Io", "T_Ipv4", "T_Ipv4_flag", "T_Ipv6", "T_Ipv6_flag",
  "T_Kernel", "T_Key", "T_Keys", "T_Keysdir", "T_Kod", "T_Mssntp",
  "T_Leapfile", "T_Leapsmearinterval", "T_Limited", "T_Link", "T_Listen",
  "T_Logconfig", "T_Logfile", "T_Loopstats", "T_Lowpriotrap",
  "T_Manycastclient", "T_Manycastserver", "T_Mask", "T_Maxage",
  "T_Maxclock", "T_Maxdepth", "T_Maxdist", "T_Maxmem", "T_Maxpoll",
  "T_Mdnstries", "T_Mem", "T_Memlock", "T_Minclock", "T_Mindepth",
  "T_Mindist", "T_Minimum", "T_Minpoll", "T_Minsane", "T_Mode", "T_Mode7",
  "T_Monitor", "T_Month", "T_Mru", "T_Multicastclient", "T_Nic",
  "T_Nolink", "T_Nomodify", "T_Nomrulist", "T_None", "T_Nonvolatile",
  "T_Nopeer", "T_Noquery", "T_Noselect", "T_Noserve", "T_Notrap",
  "T_Notrust", "T_Ntp", "T_Ntpport", "T_NtpSignDsocket", "T_Orphan",
  "T_Orphanwait", "T_PCEdigest", "T_Panic", "T_Peer", "T_Peerstats",
  "T_Phone", "T_Pid", "T_Pidfile", "T_Pool", "T_Port", "T_Preempt",
  "T_Prefer", "T_Protostats", "T_Pw", "T_Randfile", "T_Rawstats",
  "T_Refid", "T_Requestkey", "T_Reset", "T_Restrict", "T_Revoke",
  "T_Rlimit", "T_Saveconfigdir", "T_Server", "T_Setvar", "T_Source",
  "T_Stacksize", "T_Statistics", "T_Stats", "T_Statsdir", "T_Step",
  "T_Stepback", "T_Stepfwd", "T_Stepout", "T_Stratum", "T_String", "T_Sys",
  "T_Sysstats", "T_Tick", "T_Time1", "T_Time2", "T_Timer", "T_Timingstats",
  "T_Tinker", "T_Tos", "T_Trap", "T_True", "T_Trustedkey", "T_Ttl",
  "T_Type", "T_U_int", "T_UEcrypto", "T_UEcryptonak", "T_UEdigest",
  "T_Unconfig", "T_Unpeer", "T_Version", "T_WanderThreshold", "T_Week",
  "T_Wildcard", "T_Xleave", "T_Year", "T_Flag", "T_EOC", "T_Simulate",
  "T_Beep_Delay", "T_Sim_Duration", "T_Server_Offset", "T_Duration",
  "T_Freq_Offset", "T_Wander", "T_Jitter", "T_Prop_Delay", "T_Proc_Delay",
  "'='", "'('", "')'", "'{'", "'}'", "$accept", "configuration",
  "command_list", "command", "server_command", "client_type", "address",
  "ip_address", "address_fam", "option_list", "option", "option_flag",
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
     445,   446,   447,   448,   449,   450,   451,   452,   453,    61,
      40,    41,   123,   125
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
      18,  -177,   -45,  -189,  -189,  -189,   -40,  -189,    32,     5,
    -129,  -189,    32,  -189,   204,   -44,  -189,  -117,  -189,  -110,
    -101,  -189,  -189,   -97,  -189,  -189,   -44,    -4,   495,   -44,
    -189,  -189,   -96,  -189,   -94,  -189,  -189,     8,    54,   258,
      10,   -28,  -189,  -189,   -89,   204,   -86,  -189,   270,   529,
     -85,   -56,    14,  -189,  -189,  -189,    83,   207,   -95,  -189,
     -44,  -189,   -44,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,    -7,    24,   -73,   -68,  -189,    -3,  -189,
    -189,  -106,  -189,  -189,  -189,   313,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,    32,  -189,
    -189,  -189,  -189,  -189,  -189,     5,  -189,    35,    65,  -189,
      32,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,   110,  -189,   -59,   368,  -189,  -189,  -189,
     -97,  -189,  -189,   -44,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,   495,  -189,    44,   -44,  -189,  -189,   -51,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,    54,  -189,
    -189,    86,    89,  -189,  -189,    33,  -189,  -189,  -189,  -189,
     -28,  -189,    49,   -75,  -189,   204,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,   270,  -189,
      -7,  -189,  -189,  -189,   -33,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,   529,  -189,    66,    -7,  -189,  -189,    67,
     -56,  -189,  -189,  -189,    68,  -189,   -53,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,    16,
    -153,  -189,  -189,  -189,  -189,  -189,    77,  -189,   -18,  -189,
    -189,  -189,  -189,   226,   -13,  -189,  -189,  -189,  -189,    -8,
      97,  -189,  -189,   110,  -189,    -7,   -33,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,   449,  -189,  -189,   449,   449,
     -85,  -189,  -189,    11,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,   -49,   108,  -189,  -189,  -189,   125,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -102,   -20,
     -30,  -189,  -189,  -189,  -189,    13,  -189,  -189,     9,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,   449,   449,  -189,   146,   -85,   113,
    -189,   116,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,   -54,  -189,    23,   -10,     6,  -138,  -189,    -9,  -189,
      -7,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
     449,  -189,  -189,  -189,  -189,   -17,  -189,  -189,  -189,   -44,
    -189,  -189,  -189,    20,  -189,  -189,  -189,     0,    21,    -7,
      22,  -173,  -189,    25,    -7,  -189,  -189,  -189,    17,     7,
    -189,  -189,  -189,  -189,  -189,   217,    39,    36,  -189,    46,
    -189,    -7,  -189,  -189
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       0,     0,     0,    24,    58,   236,     0,    71,     0,     0,
     248,   239,     0,   229,     0,     0,   241,     0,   261,     0,
       0,   242,   240,     0,   243,    25,     0,     0,     0,     0,
     262,   237,     0,    23,     0,   244,    22,     0,     0,     0,
       0,     0,   245,    21,     0,     0,     0,   238,     0,     0,
       0,     0,     0,    56,    57,   297,     0,     2,     0,     7,
       0,     8,     0,     9,    10,    13,    11,    12,    14,    15,
      16,    17,    18,     0,     0,     0,     0,   222,     0,   223,
      19,     0,     5,    62,    63,    64,   196,   197,   198,   199,
     202,   200,   201,   203,   204,   205,   206,   207,   191,   193,
     194,   195,   155,   156,   157,   127,   153,     0,   246,   230,
     190,   102,   103,   104,   105,   109,   106,   107,   108,   110,
      29,    30,    28,     0,    26,     0,     6,    65,    66,   258,
     231,   257,   290,    59,    61,   161,   162,   163,   164,   165,
     166,   167,   168,   128,   159,     0,    60,    70,   288,   232,
      67,   273,   274,   275,   276,   277,   278,   279,   270,   272,
     135,    29,    30,   135,   135,    26,    68,   189,   187,   188,
     183,   185,     0,     0,   233,    97,   101,    98,   212,   213,
     214,   215,   216,   217,   218,   219,   220,   221,   208,   210,
       0,    86,    92,    87,     0,    88,    96,    94,    95,    93,
      91,    89,    90,    80,    82,     0,     0,   252,   284,     0,
      69,   283,   285,   281,   235,     1,     0,     4,    31,    55,
     295,   294,   224,   225,   226,   227,   269,   268,   267,     0,
       0,    79,    75,    76,    77,    78,     0,    72,     0,   192,
     152,   154,   247,    99,     0,   179,   180,   181,   182,     0,
       0,   177,   178,   169,   171,     0,     0,    27,   228,   256,
     289,   158,   160,   287,   271,   131,   135,   135,   134,   129,
       0,   184,   186,     0,   100,   209,   211,   293,   291,   292,
      85,    81,    83,    84,   234,     0,   282,   280,     3,    20,
     263,   264,   265,   260,   266,   259,   301,   302,     0,     0,
       0,    74,    73,   119,   118,     0,   116,   117,     0,   111,
     114,   115,   175,   176,   174,   170,   172,   173,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,   148,
     149,   150,   151,   136,   132,   133,   135,   251,     0,     0,
     253,     0,    37,    38,    39,    54,    47,    49,    48,    51,
      40,    41,    42,    43,    50,    52,    44,    32,    33,    36,
      34,     0,    35,     0,     0,     0,     0,   304,     0,   299,
       0,   112,   126,   122,   124,   120,   121,   123,   125,   113,
     130,   250,   249,   255,   254,     0,    45,    46,    53,     0,
     298,   296,   303,     0,   300,   286,   307,     0,     0,     0,
       0,     0,   309,     0,     0,   305,   308,   306,     0,     0,
     314,   315,   316,   317,   318,     0,     0,     0,   310,     0,
     312,     0,   311,   313
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -189,  -189,  -189,   -48,  -189,  -189,   -15,   -38,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,  -189,    37,  -189,  -189,  -189,
    -189,   -42,  -189,  -189,  -189,  -189,  -189,  -189,  -159,  -189,
    -189,   131,  -189,  -189,    96,  -189,  -189,  -189,    -6,  -189,
    -189,  -189,  -189,    74,  -189,  -189,   236,   -71,  -189,  -189,
    -189,  -189,    62,  -189,  -189,  -189,  -189,  -189,  -189,  -189,
    -189,  -189,  -189,  -189,  -189,   122,  -189,  -189,  -189,  -189,
    -189,  -189,    95,  -189,  -189,    45,  -189,  -189,   225,     1,
    -188,  -189,  -189,  -189,   -39,  -189,  -189,  -103,  -189,  -189,
    -189,  -136,  -189,  -149,  -189
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    56,    57,    58,    59,    60,   132,   124,   125,   289,
     357,   358,   359,   360,   361,   362,   363,    61,    62,    63,
      64,    85,   237,   238,    65,   203,   204,   205,   206,    66,
     175,   119,   243,   309,   310,   311,   379,    67,   265,   333,
     105,   106,   107,   143,   144,   145,    68,   253,   254,   255,
     256,    69,   170,   171,   172,    70,    98,    99,   100,   101,
      71,   188,   189,   190,    72,    73,    74,    75,    76,   109,
     174,   382,   284,   340,   130,   131,    77,    78,   295,   229,
      79,   158,   159,   214,   210,   211,   212,   149,   133,   280,
     222,    80,    81,   298,   299,   300,   366,   367,   398,   368,
     401,   402,   415,   416,   417
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     123,   165,   276,   176,   268,   269,   208,   277,   386,   216,
     364,    82,   207,   372,   338,   167,   102,    83,   283,     1,
     400,   290,    84,   120,   164,   121,   220,   239,     2,   278,
     405,   108,   226,     3,     4,     5,   373,   296,   297,   239,
      86,     6,     7,   126,    87,   218,   364,   219,     8,     9,
     127,    88,    10,   227,    11,   221,    12,    13,   134,   128,
      14,   151,   152,   129,   147,   391,   148,   316,   168,    15,
     150,   173,   166,    16,   177,   122,   213,   228,   258,    17,
     153,    18,   291,   215,   292,   339,   223,   224,   296,   297,
      19,    20,   225,   217,    21,    22,   230,   241,   242,    23,
      24,   257,    89,    25,    26,   103,   262,   334,   335,   263,
     104,   272,    27,   244,   266,   374,   122,   267,   260,   154,
     270,   387,   375,   169,   273,    28,    29,    30,   282,   285,
     287,   260,    31,   274,   342,   288,    90,    91,   279,   301,
     376,    32,   302,   343,   209,   341,    33,   312,    34,   155,
      35,    36,   313,    92,   245,   246,   247,   248,    93,   314,
      37,    38,    39,    40,    41,    42,    43,    44,   369,   370,
      45,   337,    46,   371,   381,   384,   293,   380,   385,   344,
     345,    47,   394,   388,   395,    94,    48,    49,    50,   389,
      51,    52,   377,   393,   390,   378,   346,    53,    54,   399,
     294,   410,   411,   412,   413,   414,    -6,    55,    95,    96,
      97,   403,   397,   407,   400,   156,   408,     2,   347,   409,
     157,   404,     3,     4,     5,   111,   348,   420,   349,   112,
       6,     7,   336,   423,   422,   421,   240,     8,     9,   261,
     281,    10,   350,    11,   271,    12,    13,   315,   110,    14,
     275,   249,   259,   264,   146,   286,   303,   317,    15,   365,
     351,   352,    16,   392,   304,   406,   419,   305,    17,   250,
      18,     0,     0,     0,   251,   252,   178,     0,     0,    19,
      20,     0,     0,    21,    22,     0,   160,   113,    23,    24,
       0,     0,    25,    26,     0,     0,   353,     0,   354,     0,
     383,    27,   179,     0,     0,   306,   355,     0,     0,     0,
     356,     0,     0,     0,    28,    29,    30,     0,     0,     0,
     180,    31,     0,   181,     0,   161,     0,   162,     0,     0,
      32,     0,     0,   114,     0,    33,   307,    34,     0,    35,
      36,   115,   231,     0,   116,     0,     0,     0,     0,    37,
      38,    39,    40,    41,    42,    43,    44,     0,     0,    45,
       0,    46,     0,     0,     0,   232,   117,     0,   233,     0,
      47,   118,     0,     0,   396,    48,    49,    50,     2,    51,
      52,     0,     0,     3,     4,     5,    53,    54,     0,     0,
       0,     6,     7,     0,     0,    -6,    55,   182,     8,     9,
     308,     0,    10,     0,    11,     0,    12,    13,   163,     0,
      14,   410,   411,   412,   413,   414,     0,     0,   122,    15,
     418,     0,     0,    16,     0,   183,   184,   185,   186,    17,
       0,    18,     0,   187,     0,     0,     0,     0,     0,     0,
      19,    20,     0,     0,    21,    22,     0,     0,     0,    23,
      24,   234,   235,    25,    26,     0,     0,     0,   236,     0,
       0,     0,    27,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    28,    29,    30,     0,     0,
       0,     0,    31,     0,     0,     0,     0,     0,     0,     0,
       0,    32,     0,     0,     0,     0,    33,   318,    34,     0,
      35,    36,     0,     0,     0,   319,     0,     0,     0,     0,
      37,    38,    39,    40,    41,    42,    43,    44,     0,     0,
      45,     0,    46,   320,   321,     0,     0,   322,     0,     0,
       0,    47,     0,   323,     0,     0,    48,    49,    50,     0,
      51,    52,   191,   192,     0,     0,     0,    53,    54,   193,
       0,   194,   135,   136,   137,   138,     0,    55,     0,     0,
     324,   325,     0,     0,   326,   327,     0,   328,   329,   330,
       0,   331,     0,     0,     0,     0,     0,     0,   195,     0,
       0,     0,     0,   139,     0,   140,     0,   141,     0,     0,
       0,     0,     0,   142,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   196,     0,
     197,     0,     0,     0,     0,     0,   198,     0,   199,     0,
     332,   200,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   201,   202
};

static const yytype_int16 yycheck[] =
{
      15,    39,   190,    45,   163,   164,    62,    40,    62,    57,
     148,   188,    50,     4,    63,    43,    11,    62,   206,     1,
     193,     5,    62,    67,    39,    69,    33,    98,    10,    62,
     203,   160,    35,    15,    16,    17,    27,   190,   191,   110,
       8,    23,    24,   160,    12,    60,   148,    62,    30,    31,
     160,    19,    34,    56,    36,    62,    38,    39,    62,   160,
      42,     7,     8,   160,   160,   203,   160,   255,    96,    51,
      62,   160,    62,    55,   160,   160,    62,    80,   126,    61,
      26,    63,    66,     0,    68,   134,    62,   160,   190,   191,
      72,    73,   160,   188,    76,    77,   202,    62,    33,    81,
      82,   160,    70,    85,    86,   100,    62,   266,   267,   160,
     105,    62,    94,     3,    28,   106,   160,    28,   133,    65,
      87,   175,   113,   151,   199,   107,   108,   109,    62,    62,
      62,   146,   114,   175,     9,   188,   104,   105,   171,    62,
     131,   123,   160,    18,   200,    37,   128,   160,   130,    95,
     132,   133,   160,   121,    44,    45,    46,    47,   126,    62,
     142,   143,   144,   145,   146,   147,   148,   149,   188,   199,
     152,   160,   154,   160,    28,    62,   160,   336,    62,    54,
      55,   163,   370,   160,   201,   153,   168,   169,   170,   199,
     172,   173,   183,   202,   188,   186,    71,   179,   180,   199,
     184,   194,   195,   196,   197,   198,   188,   189,   176,   177,
     178,   399,   192,   188,   193,   161,   404,    10,    93,   202,
     166,   199,    15,    16,    17,    21,   101,   188,   103,    25,
      23,    24,   270,   421,   188,   199,   105,    30,    31,   143,
     203,    34,   117,    36,   170,    38,    39,   253,    12,    42,
     188,   141,   130,   158,    29,   210,    30,   256,    51,   298,
     135,   136,    55,   366,    38,   401,   415,    41,    61,   159,
      63,    -1,    -1,    -1,   164,   165,     6,    -1,    -1,    72,
      73,    -1,    -1,    76,    77,    -1,    28,    83,    81,    82,
      -1,    -1,    85,    86,    -1,    -1,   171,    -1,   173,    -1,
     338,    94,    32,    -1,    -1,    79,   181,    -1,    -1,    -1,
     185,    -1,    -1,    -1,   107,   108,   109,    -1,    -1,    -1,
      50,   114,    -1,    53,    -1,    67,    -1,    69,    -1,    -1,
     123,    -1,    -1,   129,    -1,   128,   110,   130,    -1,   132,
     133,   137,    29,    -1,   140,    -1,    -1,    -1,    -1,   142,
     143,   144,   145,   146,   147,   148,   149,    -1,    -1,   152,
      -1,   154,    -1,    -1,    -1,    52,   162,    -1,    55,    -1,
     163,   167,    -1,    -1,   389,   168,   169,   170,    10,   172,
     173,    -1,    -1,    15,    16,    17,   179,   180,    -1,    -1,
      -1,    23,    24,    -1,    -1,   188,   189,   127,    30,    31,
     174,    -1,    34,    -1,    36,    -1,    38,    39,   150,    -1,
      42,   194,   195,   196,   197,   198,    -1,    -1,   160,    51,
     203,    -1,    -1,    55,    -1,   155,   156,   157,   158,    61,
      -1,    63,    -1,   163,    -1,    -1,    -1,    -1,    -1,    -1,
      72,    73,    -1,    -1,    76,    77,    -1,    -1,    -1,    81,
      82,   138,   139,    85,    86,    -1,    -1,    -1,   145,    -1,
      -1,    -1,    94,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   107,   108,   109,    -1,    -1,
      -1,    -1,   114,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   123,    -1,    -1,    -1,    -1,   128,    48,   130,    -1,
     132,   133,    -1,    -1,    -1,    56,    -1,    -1,    -1,    -1,
     142,   143,   144,   145,   146,   147,   148,   149,    -1,    -1,
     152,    -1,   154,    74,    75,    -1,    -1,    78,    -1,    -1,
      -1,   163,    -1,    84,    -1,    -1,   168,   169,   170,    -1,
     172,   173,    13,    14,    -1,    -1,    -1,   179,   180,    20,
      -1,    22,    57,    58,    59,    60,    -1,   189,    -1,    -1,
     111,   112,    -1,    -1,   115,   116,    -1,   118,   119,   120,
      -1,   122,    -1,    -1,    -1,    -1,    -1,    -1,    49,    -1,
      -1,    -1,    -1,    88,    -1,    90,    -1,    92,    -1,    -1,
      -1,    -1,    -1,    98,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    89,    -1,
      91,    -1,    -1,    -1,    -1,    -1,    97,    -1,    99,    -1,
     181,   102,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   124,   125
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,     1,    10,    15,    16,    17,    23,    24,    30,    31,
      34,    36,    38,    39,    42,    51,    55,    61,    63,    72,
      73,    76,    77,    81,    82,    85,    86,    94,   107,   108,
     109,   114,   123,   128,   130,   132,   133,   142,   143,   144,
     145,   146,   147,   148,   149,   152,   154,   163,   168,   169,
     170,   172,   173,   179,   180,   189,   205,   206,   207,   208,
     209,   221,   222,   223,   224,   228,   233,   241,   250,   255,
     259,   264,   268,   269,   270,   271,   272,   280,   281,   284,
     295,   296,   188,    62,    62,   225,     8,    12,    19,    70,
     104,   105,   121,   126,   153,   176,   177,   178,   260,   261,
     262,   263,    11,   100,   105,   244,   245,   246,   160,   273,
     260,    21,    25,    83,   129,   137,   140,   162,   167,   235,
      67,    69,   160,   210,   211,   212,   160,   160,   160,   160,
     278,   279,   210,   292,    62,    57,    58,    59,    60,    88,
      90,    92,    98,   247,   248,   249,   292,   160,   160,   291,
      62,     7,     8,    26,    65,    95,   161,   166,   285,   286,
      28,    67,    69,   150,   210,   211,    62,    43,    96,   151,
     256,   257,   258,   160,   274,   234,   235,   160,     6,    32,
      50,    53,   127,   155,   156,   157,   158,   163,   265,   266,
     267,    13,    14,    20,    22,    49,    89,    91,    97,    99,
     102,   124,   125,   229,   230,   231,   232,   211,    62,   200,
     288,   289,   290,    62,   287,     0,   207,   188,   210,   210,
      33,    62,   294,    62,   160,   160,    35,    56,    80,   283,
     202,    29,    52,    55,   138,   139,   145,   226,   227,   261,
     245,    62,    33,   236,     3,    44,    45,    46,    47,   141,
     159,   164,   165,   251,   252,   253,   254,   160,   207,   279,
     210,   248,    62,   160,   286,   242,    28,    28,   242,   242,
      87,   257,    62,   199,   235,   266,   294,    40,    62,   171,
     293,   230,    62,   294,   276,    62,   289,    62,   188,   213,
       5,    66,    68,   160,   184,   282,   190,   191,   297,   298,
     299,    62,   160,    30,    38,    41,    79,   110,   174,   237,
     238,   239,   160,   160,    62,   252,   294,   293,    48,    56,
      74,    75,    78,    84,   111,   112,   115,   116,   118,   119,
     120,   122,   181,   243,   242,   242,   211,   160,    63,   134,
     277,    37,     9,    18,    54,    55,    71,    93,   101,   103,
     117,   135,   136,   171,   173,   181,   185,   214,   215,   216,
     217,   218,   219,   220,   148,   298,   300,   301,   303,   188,
     199,   160,     4,    27,   106,   113,   131,   183,   186,   240,
     242,    28,   275,   211,    62,    62,    62,   175,   160,   199,
     188,   203,   301,   202,   294,   201,   210,   192,   302,   199,
     193,   304,   305,   294,   199,   203,   305,   188,   294,   202,
     194,   195,   196,   197,   198,   306,   307,   308,   203,   307,
     188,   199,   188,   294
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   204,   205,   206,   206,   206,   207,   207,   207,   207,
     207,   207,   207,   207,   207,   207,   207,   207,   207,   207,
     208,   209,   209,   209,   209,   209,   210,   210,   211,   212,
     212,   213,   213,   214,   214,   214,   215,   216,   216,   216,
     216,   216,   216,   216,   216,   217,   217,   218,   218,   218,
     218,   218,   218,   219,   220,   221,   222,   222,   223,   223,
     223,   223,   224,   224,   224,   224,   224,   224,   224,   224,
     224,   225,   225,   226,   226,   227,   227,   227,   227,   227,
     228,   229,   229,   230,   230,   230,   231,   231,   231,   231,
     231,   231,   231,   232,   232,   232,   232,   233,   233,   233,
     234,   234,   235,   235,   235,   235,   235,   235,   235,   235,
     236,   236,   237,   237,   237,   237,   238,   238,   239,   239,
     240,   240,   240,   240,   240,   240,   240,   241,   241,   241,
     241,   241,   241,   241,   241,   242,   242,   243,   243,   243,
     243,   243,   243,   243,   243,   243,   243,   243,   243,   243,
     243,   243,   244,   244,   245,   246,   246,   246,   247,   247,
     248,   249,   249,   249,   249,   249,   249,   249,   249,   250,
     251,   251,   252,   252,   252,   252,   252,   253,   253,   254,
     254,   254,   254,   255,   256,   256,   257,   258,   258,   258,
     259,   259,   260,   260,   261,   261,   262,   262,   262,   262,
     262,   262,   263,   263,   263,   263,   263,   263,   264,   265,
     265,   266,   267,   267,   267,   267,   267,   267,   267,   267,
     267,   267,   268,   268,   268,   268,   268,   268,   268,   268,
     268,   268,   268,   268,   268,   268,   269,   269,   269,   270,
     270,   271,   271,   272,   272,   272,   273,   273,   273,   274,
     275,   275,   276,   276,   277,   277,   278,   278,   279,   280,
     280,   281,   281,   282,   282,   282,   282,   283,   283,   283,
     284,   285,   285,   286,   286,   286,   286,   286,   286,   286,
     287,   287,   288,   288,   289,   289,   290,   291,   291,   292,
     292,   293,   293,   293,   294,   294,   295,   296,   297,   297,
     298,   299,   299,   300,   300,   301,   302,   303,   304,   304,
     305,   306,   306,   307,   308,   308,   308,   308,   308
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
       1,     1,     1,     1,     1,     1,     1,     2,     2,     3,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       0,     2,     2,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     2,     3,
       5,     3,     4,     4,     3,     0,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     1,     2,     1,     1,     1,     2,     1,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     3,
       2,     1,     2,     2,     2,     2,     2,     1,     1,     1,
       1,     1,     1,     2,     2,     1,     2,     1,     1,     1,
       2,     2,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     2,
       1,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     2,     2,     2,     3,     1,
       2,     2,     2,     2,     3,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     0,     4,
       1,     0,     0,     2,     2,     2,     2,     1,     1,     3,
       3,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     1,     2,     1,     1,     1,     5,     2,     1,     2,
       1,     1,     1,     1,     1,     1,     5,     1,     3,     2,
       3,     1,     1,     2,     1,     5,     4,     3,     2,     1,
       6,     3,     2,     3,     1,     1,     1,     1,     1
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
#line 378 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2126 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 20:
#line 414 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			peer_node *my_node;

			my_node = create_peer_node((yyvsp[-2].Integer), (yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.peers, my_node);
		}
#line 2137 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 27:
#line 433 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = create_address_node((yyvsp[0].String), (yyvsp[-1].Integer)); }
#line 2143 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 28:
#line 438 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = create_address_node((yyvsp[0].String), AF_UNSPEC); }
#line 2149 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 29:
#line 443 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = AF_INET; }
#line 2155 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 30:
#line 445 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = AF_INET6; }
#line 2161 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 31:
#line 450 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2167 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 32:
#line 452 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2176 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 36:
#line 466 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2182 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 45:
#line 482 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2188 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 46:
#line 484 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_uval((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2194 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 53:
#line 498 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2200 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 55:
#line 512 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			unpeer_node *my_node;

			my_node = create_unpeer_node((yyvsp[0].Address_node));
			if (my_node)
				APPEND_G_FIFO(cfgt.unpeers, my_node);
		}
#line 2212 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 58:
#line 533 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.broadcastclient = 1; }
#line 2218 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 59:
#line 535 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.manycastserver, (yyvsp[0].Address_fifo)); }
#line 2224 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 60:
#line 537 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.multicastclient, (yyvsp[0].Address_fifo)); }
#line 2230 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 61:
#line 539 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.mdnstries = (yyvsp[0].Integer); }
#line 2236 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 62:
#line 550 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *atrv;

			atrv = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			APPEND_G_FIFO(cfgt.vars, atrv);
		}
#line 2247 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 63:
#line 557 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.control_key = (yyvsp[0].Integer); }
#line 2253 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 64:
#line 559 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			cfgt.auth.cryptosw++;
			CONCAT_G_FIFOS(cfgt.auth.crypto_cmd_list, (yyvsp[0].Attr_val_fifo));
		}
#line 2262 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 65:
#line 564 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.keys = (yyvsp[0].String); }
#line 2268 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 66:
#line 566 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.keysdir = (yyvsp[0].String); }
#line 2274 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 67:
#line 568 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.request_key = (yyvsp[0].Integer); }
#line 2280 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 68:
#line 570 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.revoke = (yyvsp[0].Integer); }
#line 2286 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 69:
#line 572 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			cfgt.auth.trusted_key_list = (yyvsp[0].Attr_val_fifo);

			// if (!cfgt.auth.trusted_key_list)
			// 	cfgt.auth.trusted_key_list = $2;
			// else
			// 	LINK_SLIST(cfgt.auth.trusted_key_list, $2, link);
		}
#line 2299 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 70:
#line 581 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.ntp_signd_socket = (yyvsp[0].String); }
#line 2305 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 71:
#line 586 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2311 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 72:
#line 588 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2320 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 73:
#line 596 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2326 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 74:
#line 598 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val) = NULL;
			cfgt.auth.revoke = (yyvsp[0].Integer);
			msyslog(LOG_WARNING,
				"'crypto revoke %d' is deprecated, "
				"please use 'revoke %d' instead.",
				cfgt.auth.revoke, cfgt.auth.revoke);
		}
#line 2339 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 80:
#line 623 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.orphan_cmds, (yyvsp[0].Attr_val_fifo)); }
#line 2345 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 81:
#line 628 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2354 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 82:
#line 633 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2363 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 83:
#line 641 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2369 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 84:
#line 643 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2375 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 85:
#line 645 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2381 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 97:
#line 672 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.stats_list, (yyvsp[0].Int_fifo)); }
#line 2387 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 98:
#line 674 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				cfgt.stats_dir = (yyvsp[0].String);
			} else {
				YYFREE((yyvsp[0].String));
				yyerror("statsdir remote configuration ignored");
			}
		}
#line 2400 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 99:
#line 683 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			filegen_node *fgn;

			fgn = create_filegen_node((yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.filegen_opts, fgn);
		}
#line 2411 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 100:
#line 693 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2420 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 101:
#line 698 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2429 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 110:
#line 717 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2435 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 111:
#line 719 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2444 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 112:
#line 727 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			} else {
				(yyval.Attr_val) = NULL;
				YYFREE((yyvsp[0].String));
				yyerror("filegen file remote config ignored");
			}
		}
#line 2458 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 113:
#line 737 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror("filegen type remote config ignored");
			}
		}
#line 2471 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 114:
#line 746 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2490 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 115:
#line 761 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2496 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 127:
#line 791 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			CONCAT_G_FIFOS(cfgt.discard_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2504 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 128:
#line 795 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			CONCAT_G_FIFOS(cfgt.mru_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2512 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 129:
#line 799 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[-1].Address_node), NULL, (yyvsp[0].Int_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2524 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 130:
#line 807 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[-3].Address_node), (yyvsp[-1].Address_node), (yyvsp[0].Int_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2536 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 131:
#line 815 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node(NULL, NULL, (yyvsp[0].Int_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2548 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 132:
#line 823 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2567 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 133:
#line 838 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2586 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 134:
#line 853 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *	rn;

			APPEND_G_FIFO((yyvsp[0].Int_fifo), create_int_node((yyvsp[-1].Integer)));
			rn = create_restrict_node(
				NULL, NULL, (yyvsp[0].Int_fifo), lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2599 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 135:
#line 865 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Int_fifo) = NULL; }
#line 2605 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 136:
#line 867 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2614 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 152:
#line 893 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2623 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 153:
#line 898 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2632 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 154:
#line 906 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2638 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 158:
#line 917 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2647 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 159:
#line 922 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2656 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 160:
#line 930 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2662 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 169:
#line 950 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.fudge, aon);
		}
#line 2673 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 170:
#line 960 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2682 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 171:
#line 965 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2691 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 172:
#line 973 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2697 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 173:
#line 975 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2703 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 174:
#line 977 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if ((yyvsp[0].Integer) >= 0 && (yyvsp[0].Integer) <= 16) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror("fudge factor: stratum value not in [0..16], ignored");
			}
		}
#line 2716 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 175:
#line 986 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2722 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 176:
#line 988 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2728 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 183:
#line 1009 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.rlimit, (yyvsp[0].Attr_val_fifo)); }
#line 2734 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 184:
#line 1014 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2743 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 185:
#line 1019 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2752 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 186:
#line 1027 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2758 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 190:
#line 1043 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.enable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 2764 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 191:
#line 1045 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.disable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 2770 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 192:
#line 1050 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2779 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 193:
#line 1055 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2788 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 194:
#line 1063 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2794 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 195:
#line 1065 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2812 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 208:
#line 1104 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.tinker, (yyvsp[0].Attr_val_fifo)); }
#line 2818 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 209:
#line 1109 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2827 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 210:
#line 1114 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2836 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 211:
#line 1122 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2842 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 224:
#line 1147 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;

			av = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2853 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 225:
#line 1154 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;

			av = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2864 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 226:
#line 1161 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;

			av = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2875 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 227:
#line 1168 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2895 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 228:
#line 1184 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2918 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 229:
#line 1203 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { lex_flush_stack(); }
#line 2924 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 230:
#line 1205 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { /* see drift_parm below for actions */ }
#line 2930 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 231:
#line 1207 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.logconfig, (yyvsp[0].Attr_val_fifo)); }
#line 2936 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 232:
#line 1209 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.phone, (yyvsp[0].String_fifo)); }
#line 2942 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 233:
#line 1211 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { APPEND_G_FIFO(cfgt.setvar, (yyvsp[0].Set_var)); }
#line 2948 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 234:
#line 1213 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.trap, aon);
		}
#line 2959 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 235:
#line 1220 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.ttl, (yyvsp[0].Attr_val_fifo)); }
#line 2965 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 240:
#line 1235 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
#ifndef LEAP_SMEAR
			yyerror("Built without LEAP_SMEAR support.");
#endif
		}
#line 2975 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 246:
#line 1255 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 2990 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 247:
#line 1266 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 3007 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 248:
#line 1279 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if (lex_from_file()) {
				attr_val *av;
				av = create_attr_sval(T_Driftfile, estrdup(""));
				APPEND_G_FIFO(cfgt.vars, av);
			} else {
				yyerror("driftfile remote configuration ignored");
			}
		}
#line 3021 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 249:
#line 1292 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Set_var) = create_setvar_node((yyvsp[-3].String), (yyvsp[-1].String), (yyvsp[0].Integer)); }
#line 3027 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 251:
#line 1298 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 0; }
#line 3033 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 252:
#line 1303 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 3039 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 253:
#line 1305 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3048 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 254:
#line 1313 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 3054 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 255:
#line 1315 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), estrdup((yyvsp[0].Address_node)->address));
			destroy_address_node((yyvsp[0].Address_node));
		}
#line 3063 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 256:
#line 1323 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3072 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 257:
#line 1328 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3081 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 258:
#line 1336 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
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
#line 3107 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 259:
#line 1361 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node((yyvsp[0].Integer), NULL, (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3118 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 260:
#line 1368 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node(0, (yyvsp[0].String), (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3129 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 270:
#line 1396 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.reset_counters, (yyvsp[0].Int_fifo)); }
#line 3135 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 271:
#line 1401 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3144 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 272:
#line 1406 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3153 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 280:
#line 1430 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3162 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 281:
#line 1435 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3171 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 282:
#line 1443 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3180 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 283:
#line 1448 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3189 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 284:
#line 1456 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival('i', (yyvsp[0].Integer)); }
#line 3195 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 286:
#line 1462 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_rangeval('-', (yyvsp[-3].Integer), (yyvsp[-1].Integer)); }
#line 3201 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 287:
#line 1467 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.String_fifo) = (yyvsp[-1].String_fifo);
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3210 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 288:
#line 1472 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.String_fifo) = NULL;
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3219 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 289:
#line 1480 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Address_fifo) = (yyvsp[-1].Address_fifo);
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3228 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 290:
#line 1485 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Address_fifo) = NULL;
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3237 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 291:
#line 1493 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			if ((yyvsp[0].Integer) != 0 && (yyvsp[0].Integer) != 1) {
				yyerror("Integer value is not boolean (0 or 1). Assuming 1");
				(yyval.Integer) = 1;
			} else {
				(yyval.Integer) = (yyvsp[0].Integer);
			}
		}
#line 3250 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 292:
#line 1501 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 1; }
#line 3256 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 293:
#line 1502 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 0; }
#line 3262 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 294:
#line 1506 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Double) = (double)(yyvsp[0].Integer); }
#line 3268 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 296:
#line 1517 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			sim_node *sn;

			sn =  create_sim_node((yyvsp[-2].Attr_val_fifo), (yyvsp[-1].Sim_server_fifo));
			APPEND_G_FIFO(cfgt.sim_details, sn);

			/* Revert from ; to \n for end-of-command */
			old_config_style = 1;
		}
#line 3282 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 297:
#line 1534 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { old_config_style = 0; }
#line 3288 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 298:
#line 1539 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3297 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 299:
#line 1544 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3306 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 300:
#line 1552 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3312 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 303:
#line 1562 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_server_fifo) = (yyvsp[-1].Sim_server_fifo);
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3321 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 304:
#line 1567 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_server_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3330 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 305:
#line 1575 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Sim_server) = ONLY_SIM(create_sim_server((yyvsp[-4].Address_node), (yyvsp[-2].Double), (yyvsp[-1].Sim_script_fifo))); }
#line 3336 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 306:
#line 1580 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Double) = (yyvsp[-1].Double); }
#line 3342 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 307:
#line 1585 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = (yyvsp[0].Address_node); }
#line 3348 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 308:
#line 1590 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_script_fifo) = (yyvsp[-1].Sim_script_fifo);
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3357 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 309:
#line 1595 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_script_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3366 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 310:
#line 1603 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Sim_script) = ONLY_SIM(create_sim_script_info((yyvsp[-3].Double), (yyvsp[-1].Attr_val_fifo))); }
#line 3372 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 311:
#line 1608 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3381 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 312:
#line 1613 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3390 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 313:
#line 1621 "../../ntpd/ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3396 "ntp_parser.c" /* yacc.c:1646  */
    break;


#line 3400 "ntp_parser.c" /* yacc.c:1646  */
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
#line 1632 "../../ntpd/ntp_parser.y" /* yacc.c:1906  */


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

