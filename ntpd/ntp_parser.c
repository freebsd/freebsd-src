/* A Bison parser, made by GNU Bison 3.0.2.  */

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
#define YYBISON_VERSION "3.0.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 14 "ntp_parser.y" /* yacc.c:339  */

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
  void yyerror(struct FILE_INFO *ip_file, const char *msg);

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
    T_Ellipsis = 290,
    T_Enable = 291,
    T_End = 292,
    T_False = 293,
    T_File = 294,
    T_Filegen = 295,
    T_Filenum = 296,
    T_Flag1 = 297,
    T_Flag2 = 298,
    T_Flag3 = 299,
    T_Flag4 = 300,
    T_Flake = 301,
    T_Floor = 302,
    T_Freq = 303,
    T_Fudge = 304,
    T_Host = 305,
    T_Huffpuff = 306,
    T_Iburst = 307,
    T_Ident = 308,
    T_Ignore = 309,
    T_Incalloc = 310,
    T_Incmem = 311,
    T_Initalloc = 312,
    T_Initmem = 313,
    T_Includefile = 314,
    T_Integer = 315,
    T_Interface = 316,
    T_Intrange = 317,
    T_Io = 318,
    T_Ipv4 = 319,
    T_Ipv4_flag = 320,
    T_Ipv6 = 321,
    T_Ipv6_flag = 322,
    T_Kernel = 323,
    T_Key = 324,
    T_Keys = 325,
    T_Keysdir = 326,
    T_Kod = 327,
    T_Mssntp = 328,
    T_Leapfile = 329,
    T_Limited = 330,
    T_Link = 331,
    T_Listen = 332,
    T_Logconfig = 333,
    T_Logfile = 334,
    T_Loopstats = 335,
    T_Lowpriotrap = 336,
    T_Manycastclient = 337,
    T_Manycastserver = 338,
    T_Mask = 339,
    T_Maxage = 340,
    T_Maxclock = 341,
    T_Maxdepth = 342,
    T_Maxdist = 343,
    T_Maxmem = 344,
    T_Maxpoll = 345,
    T_Mem = 346,
    T_Memlock = 347,
    T_Minclock = 348,
    T_Mindepth = 349,
    T_Mindist = 350,
    T_Minimum = 351,
    T_Minpoll = 352,
    T_Minsane = 353,
    T_Mode = 354,
    T_Mode7 = 355,
    T_Monitor = 356,
    T_Month = 357,
    T_Mru = 358,
    T_Multicastclient = 359,
    T_Nic = 360,
    T_Nolink = 361,
    T_Nomodify = 362,
    T_Nomrulist = 363,
    T_None = 364,
    T_Nonvolatile = 365,
    T_Nopeer = 366,
    T_Noquery = 367,
    T_Noselect = 368,
    T_Noserve = 369,
    T_Notrap = 370,
    T_Notrust = 371,
    T_Ntp = 372,
    T_Ntpport = 373,
    T_NtpSignDsocket = 374,
    T_Orphan = 375,
    T_Orphanwait = 376,
    T_Panic = 377,
    T_Peer = 378,
    T_Peerstats = 379,
    T_Phone = 380,
    T_Pid = 381,
    T_Pidfile = 382,
    T_Pool = 383,
    T_Port = 384,
    T_Preempt = 385,
    T_Prefer = 386,
    T_Protostats = 387,
    T_Pw = 388,
    T_Randfile = 389,
    T_Rawstats = 390,
    T_Refid = 391,
    T_Requestkey = 392,
    T_Reset = 393,
    T_Restrict = 394,
    T_Revoke = 395,
    T_Rlimit = 396,
    T_Saveconfigdir = 397,
    T_Server = 398,
    T_Setvar = 399,
    T_Source = 400,
    T_Stacksize = 401,
    T_Statistics = 402,
    T_Stats = 403,
    T_Statsdir = 404,
    T_Step = 405,
    T_Stepout = 406,
    T_Stratum = 407,
    T_String = 408,
    T_Sys = 409,
    T_Sysstats = 410,
    T_Tick = 411,
    T_Time1 = 412,
    T_Time2 = 413,
    T_Timer = 414,
    T_Timingstats = 415,
    T_Tinker = 416,
    T_Tos = 417,
    T_Trap = 418,
    T_True = 419,
    T_Trustedkey = 420,
    T_Ttl = 421,
    T_Type = 422,
    T_U_int = 423,
    T_Unconfig = 424,
    T_Unpeer = 425,
    T_Version = 426,
    T_WanderThreshold = 427,
    T_Week = 428,
    T_Wildcard = 429,
    T_Xleave = 430,
    T_Year = 431,
    T_Flag = 432,
    T_EOC = 433,
    T_Simulate = 434,
    T_Beep_Delay = 435,
    T_Sim_Duration = 436,
    T_Server_Offset = 437,
    T_Duration = 438,
    T_Freq_Offset = 439,
    T_Wander = 440,
    T_Jitter = 441,
    T_Prop_Delay = 442,
    T_Proc_Delay = 443
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
#define T_Ellipsis 290
#define T_Enable 291
#define T_End 292
#define T_False 293
#define T_File 294
#define T_Filegen 295
#define T_Filenum 296
#define T_Flag1 297
#define T_Flag2 298
#define T_Flag3 299
#define T_Flag4 300
#define T_Flake 301
#define T_Floor 302
#define T_Freq 303
#define T_Fudge 304
#define T_Host 305
#define T_Huffpuff 306
#define T_Iburst 307
#define T_Ident 308
#define T_Ignore 309
#define T_Incalloc 310
#define T_Incmem 311
#define T_Initalloc 312
#define T_Initmem 313
#define T_Includefile 314
#define T_Integer 315
#define T_Interface 316
#define T_Intrange 317
#define T_Io 318
#define T_Ipv4 319
#define T_Ipv4_flag 320
#define T_Ipv6 321
#define T_Ipv6_flag 322
#define T_Kernel 323
#define T_Key 324
#define T_Keys 325
#define T_Keysdir 326
#define T_Kod 327
#define T_Mssntp 328
#define T_Leapfile 329
#define T_Limited 330
#define T_Link 331
#define T_Listen 332
#define T_Logconfig 333
#define T_Logfile 334
#define T_Loopstats 335
#define T_Lowpriotrap 336
#define T_Manycastclient 337
#define T_Manycastserver 338
#define T_Mask 339
#define T_Maxage 340
#define T_Maxclock 341
#define T_Maxdepth 342
#define T_Maxdist 343
#define T_Maxmem 344
#define T_Maxpoll 345
#define T_Mem 346
#define T_Memlock 347
#define T_Minclock 348
#define T_Mindepth 349
#define T_Mindist 350
#define T_Minimum 351
#define T_Minpoll 352
#define T_Minsane 353
#define T_Mode 354
#define T_Mode7 355
#define T_Monitor 356
#define T_Month 357
#define T_Mru 358
#define T_Multicastclient 359
#define T_Nic 360
#define T_Nolink 361
#define T_Nomodify 362
#define T_Nomrulist 363
#define T_None 364
#define T_Nonvolatile 365
#define T_Nopeer 366
#define T_Noquery 367
#define T_Noselect 368
#define T_Noserve 369
#define T_Notrap 370
#define T_Notrust 371
#define T_Ntp 372
#define T_Ntpport 373
#define T_NtpSignDsocket 374
#define T_Orphan 375
#define T_Orphanwait 376
#define T_Panic 377
#define T_Peer 378
#define T_Peerstats 379
#define T_Phone 380
#define T_Pid 381
#define T_Pidfile 382
#define T_Pool 383
#define T_Port 384
#define T_Preempt 385
#define T_Prefer 386
#define T_Protostats 387
#define T_Pw 388
#define T_Randfile 389
#define T_Rawstats 390
#define T_Refid 391
#define T_Requestkey 392
#define T_Reset 393
#define T_Restrict 394
#define T_Revoke 395
#define T_Rlimit 396
#define T_Saveconfigdir 397
#define T_Server 398
#define T_Setvar 399
#define T_Source 400
#define T_Stacksize 401
#define T_Statistics 402
#define T_Stats 403
#define T_Statsdir 404
#define T_Step 405
#define T_Stepout 406
#define T_Stratum 407
#define T_String 408
#define T_Sys 409
#define T_Sysstats 410
#define T_Tick 411
#define T_Time1 412
#define T_Time2 413
#define T_Timer 414
#define T_Timingstats 415
#define T_Tinker 416
#define T_Tos 417
#define T_Trap 418
#define T_True 419
#define T_Trustedkey 420
#define T_Ttl 421
#define T_Type 422
#define T_U_int 423
#define T_Unconfig 424
#define T_Unpeer 425
#define T_Version 426
#define T_WanderThreshold 427
#define T_Week 428
#define T_Wildcard 429
#define T_Xleave 430
#define T_Year 431
#define T_Flag 432
#define T_EOC 433
#define T_Simulate 434
#define T_Beep_Delay 435
#define T_Sim_Duration 436
#define T_Server_Offset 437
#define T_Duration 438
#define T_Freq_Offset 439
#define T_Wander 440
#define T_Jitter 441
#define T_Prop_Delay 442
#define T_Proc_Delay 443

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 54 "ntp_parser.y" /* yacc.c:355  */

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

#line 535 "../../ntpd/ntp_parser.c" /* yacc.c:355  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (struct FILE_INFO *ip_file);

#endif /* !YY_YY_Y_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 550 "../../ntpd/ntp_parser.c" /* yacc.c:358  */

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
#define YYFINAL  203
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   653

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  194
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  104
/* YYNRULES -- Number of rules.  */
#define YYNRULES  307
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  411

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   443

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
     190,   191,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   189,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   192,     2,   193,     2,     2,     2,     2,
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
     185,   186,   187,   188
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   363,   363,   367,   368,   369,   383,   384,   385,   386,
     387,   388,   389,   390,   391,   392,   393,   394,   395,   396,
     404,   414,   415,   416,   417,   418,   422,   423,   428,   433,
     435,   441,   442,   450,   451,   452,   456,   461,   462,   463,
     464,   465,   466,   467,   468,   472,   474,   479,   480,   481,
     482,   483,   484,   488,   493,   502,   512,   513,   523,   525,
     527,   538,   545,   547,   552,   554,   556,   558,   560,   569,
     575,   576,   584,   586,   598,   599,   600,   601,   602,   611,
     616,   621,   629,   631,   633,   638,   639,   640,   641,   642,
     643,   647,   648,   649,   650,   659,   661,   670,   680,   685,
     693,   694,   695,   696,   697,   698,   699,   700,   705,   706,
     714,   724,   733,   748,   753,   754,   758,   759,   763,   764,
     765,   766,   767,   768,   769,   778,   782,   786,   794,   802,
     810,   825,   840,   853,   854,   862,   863,   864,   865,   866,
     867,   868,   869,   870,   871,   872,   873,   874,   875,   876,
     880,   885,   893,   898,   899,   900,   904,   909,   917,   922,
     923,   924,   925,   926,   927,   928,   929,   937,   947,   952,
     960,   962,   964,   966,   968,   973,   974,   978,   979,   980,
     981,   989,   994,   999,  1007,  1012,  1013,  1014,  1023,  1025,
    1030,  1035,  1043,  1045,  1062,  1063,  1064,  1065,  1066,  1067,
    1071,  1072,  1080,  1085,  1090,  1098,  1103,  1104,  1105,  1106,
    1107,  1108,  1109,  1110,  1119,  1120,  1121,  1128,  1135,  1151,
    1170,  1175,  1177,  1179,  1181,  1183,  1190,  1195,  1196,  1197,
    1201,  1202,  1203,  1207,  1208,  1212,  1219,  1229,  1238,  1243,
    1245,  1250,  1251,  1259,  1261,  1269,  1274,  1282,  1307,  1314,
    1324,  1325,  1329,  1330,  1331,  1332,  1336,  1337,  1338,  1342,
    1347,  1352,  1360,  1361,  1362,  1363,  1364,  1365,  1366,  1376,
    1381,  1389,  1394,  1402,  1404,  1408,  1413,  1418,  1426,  1431,
    1439,  1448,  1449,  1453,  1454,  1463,  1481,  1485,  1490,  1498,
    1503,  1504,  1508,  1513,  1521,  1526,  1531,  1536,  1541,  1549,
    1554,  1559,  1567,  1572,  1573,  1574,  1575,  1576
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
  "T_Dispersion", "T_Double", "T_Driftfile", "T_Drop", "T_Ellipsis",
  "T_Enable", "T_End", "T_False", "T_File", "T_Filegen", "T_Filenum",
  "T_Flag1", "T_Flag2", "T_Flag3", "T_Flag4", "T_Flake", "T_Floor",
  "T_Freq", "T_Fudge", "T_Host", "T_Huffpuff", "T_Iburst", "T_Ident",
  "T_Ignore", "T_Incalloc", "T_Incmem", "T_Initalloc", "T_Initmem",
  "T_Includefile", "T_Integer", "T_Interface", "T_Intrange", "T_Io",
  "T_Ipv4", "T_Ipv4_flag", "T_Ipv6", "T_Ipv6_flag", "T_Kernel", "T_Key",
  "T_Keys", "T_Keysdir", "T_Kod", "T_Mssntp", "T_Leapfile", "T_Limited",
  "T_Link", "T_Listen", "T_Logconfig", "T_Logfile", "T_Loopstats",
  "T_Lowpriotrap", "T_Manycastclient", "T_Manycastserver", "T_Mask",
  "T_Maxage", "T_Maxclock", "T_Maxdepth", "T_Maxdist", "T_Maxmem",
  "T_Maxpoll", "T_Mem", "T_Memlock", "T_Minclock", "T_Mindepth",
  "T_Mindist", "T_Minimum", "T_Minpoll", "T_Minsane", "T_Mode", "T_Mode7",
  "T_Monitor", "T_Month", "T_Mru", "T_Multicastclient", "T_Nic",
  "T_Nolink", "T_Nomodify", "T_Nomrulist", "T_None", "T_Nonvolatile",
  "T_Nopeer", "T_Noquery", "T_Noselect", "T_Noserve", "T_Notrap",
  "T_Notrust", "T_Ntp", "T_Ntpport", "T_NtpSignDsocket", "T_Orphan",
  "T_Orphanwait", "T_Panic", "T_Peer", "T_Peerstats", "T_Phone", "T_Pid",
  "T_Pidfile", "T_Pool", "T_Port", "T_Preempt", "T_Prefer", "T_Protostats",
  "T_Pw", "T_Randfile", "T_Rawstats", "T_Refid", "T_Requestkey", "T_Reset",
  "T_Restrict", "T_Revoke", "T_Rlimit", "T_Saveconfigdir", "T_Server",
  "T_Setvar", "T_Source", "T_Stacksize", "T_Statistics", "T_Stats",
  "T_Statsdir", "T_Step", "T_Stepout", "T_Stratum", "T_String", "T_Sys",
  "T_Sysstats", "T_Tick", "T_Time1", "T_Time2", "T_Timer", "T_Timingstats",
  "T_Tinker", "T_Tos", "T_Trap", "T_True", "T_Trustedkey", "T_Ttl",
  "T_Type", "T_U_int", "T_Unconfig", "T_Unpeer", "T_Version",
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
  "miscellaneous_command", "misc_cmd_dbl_keyword", "misc_cmd_str_keyword",
  "misc_cmd_str_lcl_keyword", "drift_parm", "variable_assign",
  "t_default_or_zero", "trap_option_list", "trap_option",
  "log_config_list", "log_config_command", "interface_command",
  "interface_nic", "nic_rule_class", "nic_rule_action", "reset_command",
  "counter_set_list", "counter_set_keyword", "integer_list",
  "integer_list_range", "integer_list_range_elt", "integer_range",
  "string_list", "address_list", "boolean", "number", "simulate_command",
  "sim_conf_start", "sim_init_statement_list", "sim_init_statement",
  "sim_init_keyword", "sim_server_list", "sim_server", "sim_server_offset",
  "sim_server_name", "sim_act_list", "sim_act", "sim_act_stmt_list",
  "sim_act_stmt", "sim_act_keyword", YY_NULLPTR
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
     435,   436,   437,   438,   439,   440,   441,   442,   443,    61,
      40,    41,   123,   125
};
# endif

#define YYPACT_NINF -178

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-178)))

#define YYTABLE_NINF -7

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      26,  -153,   -30,  -178,  -178,  -178,   -28,  -178,   166,    18,
    -109,   166,  -178,   200,   -47,  -178,  -102,  -178,   -96,   -93,
    -178,   -89,  -178,  -178,   -47,   330,   -47,  -178,  -178,   -85,
    -178,   -76,  -178,  -178,    20,    -2,    45,    22,   -22,  -178,
    -178,   -67,   200,   -65,  -178,   107,   520,   -63,   -53,    35,
    -178,  -178,  -178,    46,   203,   -94,  -178,   -47,  -178,   -47,
    -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,
     -21,   -55,   -54,  -178,     4,  -178,  -178,   -77,  -178,  -178,
    -178,   158,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,
     166,  -178,  -178,  -178,  -178,  -178,  -178,    18,  -178,    47,
      84,  -178,   166,  -178,  -178,  -178,  -178,  -178,  -178,  -178,
    -178,  -178,  -178,  -178,  -178,    49,  -178,   -33,   361,  -178,
    -178,  -178,   -89,  -178,  -178,   -47,  -178,  -178,  -178,  -178,
    -178,  -178,  -178,  -178,   330,  -178,    58,   -47,  -178,  -178,
     -31,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,    -2,
    -178,  -178,    94,    98,  -178,  -178,    43,  -178,  -178,  -178,
    -178,   -22,  -178,    68,   -57,  -178,   200,  -178,  -178,  -178,
    -178,  -178,  -178,  -178,  -178,  -178,  -178,   107,  -178,   -21,
    -178,  -178,   -25,  -178,  -178,  -178,  -178,  -178,  -178,  -178,
    -178,   520,  -178,    75,   -21,  -178,  -178,    86,   -53,  -178,
    -178,  -178,    87,  -178,   -19,  -178,  -178,  -178,  -178,  -178,
    -178,  -178,  -178,  -178,  -178,  -178,     3,  -107,  -178,  -178,
    -178,  -178,  -178,    88,  -178,     7,  -178,  -178,  -178,  -178,
      -5,     8,  -178,  -178,  -178,  -178,    23,   111,  -178,  -178,
      49,  -178,   -21,   -25,  -178,  -178,  -178,  -178,  -178,  -178,
    -178,  -178,   482,  -178,  -178,   482,   482,   -63,  -178,  -178,
      28,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,
    -178,   -46,   144,  -178,  -178,  -178,   416,  -178,  -178,  -178,
    -178,  -178,  -178,  -178,  -178,  -127,     5,    10,  -178,  -178,
    -178,  -178,    40,  -178,  -178,    24,  -178,  -178,  -178,  -178,
    -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,
    -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,
    -178,   482,   482,  -178,   167,   -63,   142,  -178,   143,  -178,
    -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,
    -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,   -51,  -178,
      57,    27,    34,  -117,  -178,    29,  -178,   -21,  -178,  -178,
    -178,  -178,  -178,  -178,  -178,  -178,  -178,   482,  -178,  -178,
    -178,  -178,    32,  -178,  -178,  -178,   -47,  -178,  -178,  -178,
      33,  -178,  -178,  -178,    38,    52,   -21,    39,  -146,  -178,
      59,   -21,  -178,  -178,  -178,    50,   -44,  -178,  -178,  -178,
    -178,  -178,    60,    63,    41,  -178,    71,  -178,   -21,  -178,
    -178
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       0,     0,     0,    24,    58,   227,     0,    70,     0,     0,
     237,     0,   220,     0,     0,   230,     0,   250,     0,     0,
     231,     0,   233,    25,     0,     0,     0,   251,   228,     0,
      23,     0,   232,    22,     0,     0,     0,     0,     0,   234,
      21,     0,     0,     0,   229,     0,     0,     0,     0,     0,
      56,    57,   286,     0,     2,     0,     7,     0,     8,     0,
       9,    10,    13,    11,    12,    14,    15,    16,    17,    18,
       0,     0,     0,   214,     0,   215,    19,     0,     5,    61,
      62,    63,   194,   195,   196,   197,   200,   198,   199,   201,
     189,   191,   192,   193,   153,   154,   155,   125,   151,     0,
     235,   221,   188,   100,   101,   102,   103,   107,   104,   105,
     106,   108,    29,    30,    28,     0,    26,     0,     6,    64,
      65,   247,   222,   246,   279,    59,   159,   160,   161,   162,
     163,   164,   165,   166,   126,   157,     0,    60,    69,   277,
     223,    66,   262,   263,   264,   265,   266,   267,   268,   259,
     261,   133,    29,    30,   133,   133,    26,    67,   187,   185,
     186,   181,   183,     0,     0,   224,    95,    99,    96,   206,
     207,   208,   209,   210,   211,   212,   213,   202,   204,     0,
      90,    85,     0,    86,    94,    92,    93,    91,    89,    87,
      88,    79,    81,     0,     0,   241,   273,     0,    68,   272,
     274,   270,   226,     1,     0,     4,    31,    55,   284,   283,
     216,   217,   218,   258,   257,   256,     0,     0,    78,    74,
      75,    76,    77,     0,    71,     0,   190,   150,   152,   236,
      97,     0,   177,   178,   179,   180,     0,     0,   175,   176,
     167,   169,     0,     0,    27,   219,   245,   278,   156,   158,
     276,   260,   129,   133,   133,   132,   127,     0,   182,   184,
       0,    98,   203,   205,   282,   280,   281,    84,    80,    82,
      83,   225,     0,   271,   269,     3,    20,   252,   253,   254,
     249,   255,   248,   290,   291,     0,     0,     0,    73,    72,
     117,   116,     0,   114,   115,     0,   109,   112,   113,   173,
     174,   172,   168,   170,   171,   135,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     134,   130,   131,   133,   240,     0,     0,   242,     0,    37,
      38,    39,    54,    47,    49,    48,    51,    40,    41,    42,
      43,    50,    52,    44,    32,    33,    36,    34,     0,    35,
       0,     0,     0,     0,   293,     0,   288,     0,   110,   124,
     120,   122,   118,   119,   121,   123,   111,   128,   239,   238,
     244,   243,     0,    45,    46,    53,     0,   287,   285,   292,
       0,   289,   275,   296,     0,     0,     0,     0,     0,   298,
       0,     0,   294,   297,   295,     0,     0,   303,   304,   305,
     306,   307,     0,     0,     0,   299,     0,   301,     0,   300,
     302
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -178,  -178,  -178,   -40,  -178,  -178,   -14,   -35,  -178,  -178,
    -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,  -178,
    -178,  -178,  -178,  -178,  -178,  -178,    64,  -178,  -178,  -178,
    -178,   -32,  -178,  -178,  -178,  -178,  -178,  -178,  -151,  -178,
    -178,   141,  -178,  -178,   116,  -178,  -178,  -178,    11,  -178,
    -178,  -178,  -178,    93,  -178,  -178,   248,   -69,  -178,  -178,
    -178,  -178,    83,  -178,  -178,  -178,  -178,  -178,  -178,  -178,
    -178,  -178,  -178,  -178,   139,  -178,  -178,  -178,  -178,  -178,
    -178,   119,  -178,  -178,    67,  -178,  -178,   243,    36,  -177,
    -178,  -178,  -178,   -15,  -178,  -178,   -82,  -178,  -178,  -178,
    -116,  -178,  -126,  -178
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    53,    54,    55,    56,    57,   124,   116,   117,   276,
     344,   345,   346,   347,   348,   349,   350,    58,    59,    60,
      61,    81,   224,   225,    62,   191,   192,   193,   194,    63,
     166,   111,   230,   296,   297,   298,   366,    64,   252,   320,
      97,    98,    99,   134,   135,   136,    65,   240,   241,   242,
     243,    66,   161,   162,   163,    67,    90,    91,    92,    93,
      68,   177,   178,   179,    69,    70,    71,    72,   101,   165,
     369,   271,   327,   122,   123,    73,    74,   282,   216,    75,
     149,   150,   202,   198,   199,   200,   140,   125,   267,   210,
      76,    77,   285,   286,   287,   353,   354,   385,   355,   388,
     389,   402,   403,   404
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     115,   156,   263,   255,   256,   142,   143,   196,   277,   373,
     167,   208,   195,   264,   204,   325,   351,   270,   112,   158,
     113,   226,   155,   144,   290,    78,   351,     1,   359,    94,
      79,   291,    80,   226,   292,   265,     2,   387,   213,   209,
       3,     4,     5,   206,   100,   207,   203,   392,     6,     7,
     360,   118,   231,   283,   284,     8,     9,   119,   214,    10,
     120,   145,    11,    12,   121,   303,    13,   278,   138,   279,
     159,   293,   151,   283,   284,    14,   378,   139,   245,    15,
     141,   215,   157,   326,   205,    16,   164,    17,   168,   146,
     114,   232,   233,   234,   235,   201,    18,    19,   211,   212,
      20,   294,   321,   322,    21,    22,   114,   228,    23,    24,
     152,   247,   153,   169,    95,   217,   229,   374,   249,    96,
     244,   253,   250,   247,   160,   254,   361,   257,   259,    25,
      26,    27,   260,   362,   261,   269,    28,   197,   170,   266,
     397,   398,   399,   400,   401,    29,   272,   274,   288,    30,
     363,    31,   147,    32,    33,   171,   280,   148,   172,   275,
     289,   299,   295,    34,    35,    36,    37,    38,    39,    40,
      41,   301,   367,    42,    82,    43,   300,   281,    83,   328,
     381,   324,    44,   356,    84,   236,   218,    45,    46,    47,
     154,    48,    49,   358,   368,    50,    51,   364,   114,   357,
     365,   237,   371,   372,    -6,    52,   238,   239,   219,   390,
     375,   220,   377,     2,   395,   384,   376,     3,     4,     5,
     103,   380,   323,   382,   104,     6,     7,   386,   391,   173,
     408,   410,     8,     9,    85,   387,    10,   394,   227,    11,
      12,   407,   396,    13,   397,   398,   399,   400,   401,   409,
     248,   302,    14,   405,   258,   268,    15,   174,   175,   102,
     262,   246,    16,   176,    17,   273,    86,    87,   251,   137,
     352,   379,   393,    18,    19,     0,   406,    20,     0,   304,
     105,    21,    22,    88,     0,    23,    24,     0,     0,     0,
     370,   221,   222,     0,     0,     0,     0,     0,   223,     0,
       0,     0,     0,     0,     0,     0,    25,    26,    27,     0,
       0,     0,     0,    28,    89,     0,     0,     0,     0,     0,
       0,     0,    29,     0,   106,     0,    30,     0,    31,     0,
      32,    33,   107,     0,     0,   108,     0,     0,     0,     0,
      34,    35,    36,    37,    38,    39,    40,    41,     0,     0,
      42,     0,    43,     0,     0,   109,     0,     0,     0,    44,
     110,     0,   383,     0,    45,    46,    47,     0,    48,    49,
       0,     2,    50,    51,     0,     3,     4,     5,     0,     0,
       0,    -6,    52,     6,     7,   126,   127,   128,   129,     0,
       8,     9,     0,     0,    10,     0,     0,    11,    12,     0,
       0,    13,     0,     0,     0,     0,     0,     0,     0,     0,
      14,     0,     0,     0,    15,   130,     0,   131,     0,   132,
      16,     0,    17,     0,   133,   329,     0,     0,     0,     0,
       0,    18,    19,   330,     0,    20,     0,     0,     0,    21,
      22,     0,     0,    23,    24,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    25,    26,    27,     0,   331,   332,
       0,    28,     0,     0,     0,     0,     0,     0,     0,     0,
      29,     0,     0,     0,    30,   333,    31,     0,    32,    33,
       0,     0,     0,     0,     0,     0,     0,     0,    34,    35,
      36,    37,    38,    39,    40,    41,   334,     0,    42,     0,
      43,     0,     0,   335,     0,   336,     0,    44,     0,     0,
       0,     0,    45,    46,    47,     0,    48,    49,   305,   337,
      50,    51,     0,   180,     0,     0,   306,     0,     0,   181,
      52,   182,     0,     0,     0,     0,   338,   339,     0,     0,
       0,     0,     0,     0,   307,   308,     0,   309,     0,     0,
       0,     0,     0,   310,     0,     0,     0,   183,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     340,     0,   341,     0,     0,     0,     0,   342,     0,   311,
     312,   343,     0,   313,   314,     0,   315,   316,   317,     0,
     318,     0,     0,     0,     0,     0,   184,     0,   185,     0,
       0,     0,     0,   186,     0,   187,     0,     0,   188,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     189,   190,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   319
};

static const yytype_int16 yycheck[] =
{
      14,    36,   179,   154,   155,     7,     8,    60,     5,    60,
      42,    32,    47,    38,    54,    61,   143,   194,    65,    41,
      67,    90,    36,    25,    29,   178,   143,     1,     4,    11,
      60,    36,    60,   102,    39,    60,    10,   183,    34,    60,
      14,    15,    16,    57,   153,    59,     0,   193,    22,    23,
      26,   153,     3,   180,   181,    29,    30,   153,    54,    33,
     153,    63,    36,    37,   153,   242,    40,    64,   153,    66,
      92,    76,    27,   180,   181,    49,   193,   153,   118,    53,
      60,    77,    60,   129,   178,    59,   153,    61,   153,    91,
     153,    42,    43,    44,    45,    60,    70,    71,   153,   153,
      74,   106,   253,   254,    78,    79,   153,    60,    82,    83,
      65,   125,    67,     6,    96,   192,    32,   168,    60,   101,
     153,    27,   153,   137,   146,    27,   102,    84,    60,   103,
     104,   105,   189,   109,   166,    60,   110,   190,    31,   164,
     184,   185,   186,   187,   188,   119,    60,    60,    60,   123,
     126,   125,   154,   127,   128,    48,   153,   159,    51,   178,
     153,   153,   167,   137,   138,   139,   140,   141,   142,   143,
     144,    60,   323,   147,     8,   149,   153,   174,    12,    35,
     357,   153,   156,   178,    18,   136,    28,   161,   162,   163,
     145,   165,   166,   153,    27,   169,   170,   173,   153,   189,
     176,   152,    60,    60,   178,   179,   157,   158,    50,   386,
     153,    53,   178,    10,   391,   182,   189,    14,    15,    16,
      20,   192,   257,   191,    24,    22,    23,   189,   189,   122,
     189,   408,    29,    30,    68,   183,    33,   178,    97,    36,
      37,   178,   192,    40,   184,   185,   186,   187,   188,   178,
     134,   240,    49,   193,   161,   191,    53,   150,   151,    11,
     177,   122,    59,   156,    61,   198,   100,   101,   149,    26,
     285,   353,   388,    70,    71,    -1,   402,    74,    -1,   243,
      80,    78,    79,   117,    -1,    82,    83,    -1,    -1,    -1,
     325,   133,   134,    -1,    -1,    -1,    -1,    -1,   140,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   103,   104,   105,    -1,
      -1,    -1,    -1,   110,   148,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   119,    -1,   124,    -1,   123,    -1,   125,    -1,
     127,   128,   132,    -1,    -1,   135,    -1,    -1,    -1,    -1,
     137,   138,   139,   140,   141,   142,   143,   144,    -1,    -1,
     147,    -1,   149,    -1,    -1,   155,    -1,    -1,    -1,   156,
     160,    -1,   376,    -1,   161,   162,   163,    -1,   165,   166,
      -1,    10,   169,   170,    -1,    14,    15,    16,    -1,    -1,
      -1,   178,   179,    22,    23,    55,    56,    57,    58,    -1,
      29,    30,    -1,    -1,    33,    -1,    -1,    36,    37,    -1,
      -1,    40,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      49,    -1,    -1,    -1,    53,    85,    -1,    87,    -1,    89,
      59,    -1,    61,    -1,    94,     9,    -1,    -1,    -1,    -1,
      -1,    70,    71,    17,    -1,    74,    -1,    -1,    -1,    78,
      79,    -1,    -1,    82,    83,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   103,   104,   105,    -1,    52,    53,
      -1,   110,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     119,    -1,    -1,    -1,   123,    69,   125,    -1,   127,   128,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   137,   138,
     139,   140,   141,   142,   143,   144,    90,    -1,   147,    -1,
     149,    -1,    -1,    97,    -1,    99,    -1,   156,    -1,    -1,
      -1,    -1,   161,   162,   163,    -1,   165,   166,    46,   113,
     169,   170,    -1,    13,    -1,    -1,    54,    -1,    -1,    19,
     179,    21,    -1,    -1,    -1,    -1,   130,   131,    -1,    -1,
      -1,    -1,    -1,    -1,    72,    73,    -1,    75,    -1,    -1,
      -1,    -1,    -1,    81,    -1,    -1,    -1,    47,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     164,    -1,   166,    -1,    -1,    -1,    -1,   171,    -1,   107,
     108,   175,    -1,   111,   112,    -1,   114,   115,   116,    -1,
     118,    -1,    -1,    -1,    -1,    -1,    86,    -1,    88,    -1,
      -1,    -1,    -1,    93,    -1,    95,    -1,    -1,    98,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     120,   121,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   171
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,     1,    10,    14,    15,    16,    22,    23,    29,    30,
      33,    36,    37,    40,    49,    53,    59,    61,    70,    71,
      74,    78,    79,    82,    83,   103,   104,   105,   110,   119,
     123,   125,   127,   128,   137,   138,   139,   140,   141,   142,
     143,   144,   147,   149,   156,   161,   162,   163,   165,   166,
     169,   170,   179,   195,   196,   197,   198,   199,   211,   212,
     213,   214,   218,   223,   231,   240,   245,   249,   254,   258,
     259,   260,   261,   269,   270,   273,   284,   285,   178,    60,
      60,   215,     8,    12,    18,    68,   100,   101,   117,   148,
     250,   251,   252,   253,    11,    96,   101,   234,   235,   236,
     153,   262,   250,    20,    24,    80,   124,   132,   135,   155,
     160,   225,    65,    67,   153,   200,   201,   202,   153,   153,
     153,   153,   267,   268,   200,   281,    55,    56,    57,    58,
      85,    87,    89,    94,   237,   238,   239,   281,   153,   153,
     280,    60,     7,     8,    25,    63,    91,   154,   159,   274,
     275,    27,    65,    67,   145,   200,   201,    60,    41,    92,
     146,   246,   247,   248,   153,   263,   224,   225,   153,     6,
      31,    48,    51,   122,   150,   151,   156,   255,   256,   257,
      13,    19,    21,    47,    86,    88,    93,    95,    98,   120,
     121,   219,   220,   221,   222,   201,    60,   190,   277,   278,
     279,    60,   276,     0,   197,   178,   200,   200,    32,    60,
     283,   153,   153,    34,    54,    77,   272,   192,    28,    50,
      53,   133,   134,   140,   216,   217,   251,   235,    60,    32,
     226,     3,    42,    43,    44,    45,   136,   152,   157,   158,
     241,   242,   243,   244,   153,   197,   268,   200,   238,    60,
     153,   275,   232,    27,    27,   232,   232,    84,   247,    60,
     189,   225,   256,   283,    38,    60,   164,   282,   220,    60,
     283,   265,    60,   278,    60,   178,   203,     5,    64,    66,
     153,   174,   271,   180,   181,   286,   287,   288,    60,   153,
      29,    36,    39,    76,   106,   167,   227,   228,   229,   153,
     153,    60,   242,   283,   282,    46,    54,    72,    73,    75,
      81,   107,   108,   111,   112,   114,   115,   116,   118,   171,
     233,   232,   232,   201,   153,    61,   129,   266,    35,     9,
      17,    52,    53,    69,    90,    97,    99,   113,   130,   131,
     164,   166,   171,   175,   204,   205,   206,   207,   208,   209,
     210,   143,   287,   289,   290,   292,   178,   189,   153,     4,
      26,   102,   109,   126,   173,   176,   230,   232,    27,   264,
     201,    60,    60,    60,   168,   153,   189,   178,   193,   290,
     192,   283,   191,   200,   182,   291,   189,   183,   293,   294,
     283,   189,   193,   294,   178,   283,   192,   184,   185,   186,
     187,   188,   295,   296,   297,   193,   296,   178,   189,   178,
     283
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   194,   195,   196,   196,   196,   197,   197,   197,   197,
     197,   197,   197,   197,   197,   197,   197,   197,   197,   197,
     198,   199,   199,   199,   199,   199,   200,   200,   201,   202,
     202,   203,   203,   204,   204,   204,   205,   206,   206,   206,
     206,   206,   206,   206,   206,   207,   207,   208,   208,   208,
     208,   208,   208,   209,   210,   211,   212,   212,   213,   213,
     213,   214,   214,   214,   214,   214,   214,   214,   214,   214,
     215,   215,   216,   216,   217,   217,   217,   217,   217,   218,
     219,   219,   220,   220,   220,   221,   221,   221,   221,   221,
     221,   222,   222,   222,   222,   223,   223,   223,   224,   224,
     225,   225,   225,   225,   225,   225,   225,   225,   226,   226,
     227,   227,   227,   227,   228,   228,   229,   229,   230,   230,
     230,   230,   230,   230,   230,   231,   231,   231,   231,   231,
     231,   231,   231,   232,   232,   233,   233,   233,   233,   233,
     233,   233,   233,   233,   233,   233,   233,   233,   233,   233,
     234,   234,   235,   236,   236,   236,   237,   237,   238,   239,
     239,   239,   239,   239,   239,   239,   239,   240,   241,   241,
     242,   242,   242,   242,   242,   243,   243,   244,   244,   244,
     244,   245,   246,   246,   247,   248,   248,   248,   249,   249,
     250,   250,   251,   251,   252,   252,   252,   252,   252,   252,
     253,   253,   254,   255,   255,   256,   257,   257,   257,   257,
     257,   257,   257,   257,   258,   258,   258,   258,   258,   258,
     258,   258,   258,   258,   258,   258,   258,   259,   259,   259,
     260,   260,   260,   261,   261,   262,   262,   262,   263,   264,
     264,   265,   265,   266,   266,   267,   267,   268,   269,   269,
     270,   270,   271,   271,   271,   271,   272,   272,   272,   273,
     274,   274,   275,   275,   275,   275,   275,   275,   275,   276,
     276,   277,   277,   278,   278,   279,   280,   280,   281,   281,
     282,   282,   282,   283,   283,   284,   285,   286,   286,   287,
     288,   288,   289,   289,   290,   291,   292,   293,   293,   294,
     295,   295,   296,   297,   297,   297,   297,   297
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
       0,     2,     2,     2,     1,     1,     1,     1,     1,     2,
       2,     1,     2,     2,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     2,     3,     2,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     0,     2,
       2,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     2,     3,     5,     3,
       4,     4,     3,     0,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     1,     2,     1,     1,     1,     2,     1,     2,     1,
       1,     1,     1,     1,     1,     1,     1,     3,     2,     1,
       2,     2,     2,     2,     2,     1,     1,     1,     1,     1,
       1,     2,     2,     1,     2,     1,     1,     1,     2,     2,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     2,     1,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     2,     2,     3,
       1,     2,     2,     2,     2,     3,     2,     1,     1,     1,
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
      yyerror (ip_file, YY_("syntax error: cannot back up")); \
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
                  Type, Value, ip_file); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, struct FILE_INFO *ip_file)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (ip_file);
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
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, struct FILE_INFO *ip_file)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, ip_file);
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
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, struct FILE_INFO *ip_file)
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
                                              , ip_file);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, ip_file); \
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, struct FILE_INFO *ip_file)
{
  YYUSE (yyvaluep);
  YYUSE (ip_file);
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
yyparse (struct FILE_INFO *ip_file)
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
      yychar = yylex (ip_file);
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
#line 370 "ntp_parser.y" /* yacc.c:1646  */
    {
			/* I will need to incorporate much more fine grained
			 * error messages. The following should suffice for
			 * the time being.
			 */
			msyslog(LOG_ERR, 
				"syntax error in %s line %d, column %d",
				ip_file->fname,
				ip_file->err_line_no,
				ip_file->err_col_no);
		}
#line 2093 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 20:
#line 405 "ntp_parser.y" /* yacc.c:1646  */
    {
			peer_node *my_node;

			my_node = create_peer_node((yyvsp[-2].Integer), (yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.peers, my_node);
		}
#line 2104 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 27:
#line 424 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = create_address_node((yyvsp[0].String), (yyvsp[-1].Integer)); }
#line 2110 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 28:
#line 429 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = create_address_node((yyvsp[0].String), AF_UNSPEC); }
#line 2116 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 29:
#line 434 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = AF_INET; }
#line 2122 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 30:
#line 436 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = AF_INET6; }
#line 2128 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 31:
#line 441 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2134 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 32:
#line 443 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2143 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 36:
#line 457 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2149 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 45:
#line 473 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2155 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 46:
#line 475 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_uval((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2161 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 53:
#line 489 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2167 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 55:
#line 503 "ntp_parser.y" /* yacc.c:1646  */
    {
			unpeer_node *my_node;
			
			my_node = create_unpeer_node((yyvsp[0].Address_node));
			if (my_node)
				APPEND_G_FIFO(cfgt.unpeers, my_node);
		}
#line 2179 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 58:
#line 524 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.broadcastclient = 1; }
#line 2185 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 59:
#line 526 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.manycastserver, (yyvsp[0].Address_fifo)); }
#line 2191 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 60:
#line 528 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.multicastclient, (yyvsp[0].Address_fifo)); }
#line 2197 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 61:
#line 539 "ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *atrv;
			
			atrv = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			APPEND_G_FIFO(cfgt.vars, atrv);
		}
#line 2208 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 62:
#line 546 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.control_key = (yyvsp[0].Integer); }
#line 2214 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 63:
#line 548 "ntp_parser.y" /* yacc.c:1646  */
    { 
			cfgt.auth.cryptosw++;
			CONCAT_G_FIFOS(cfgt.auth.crypto_cmd_list, (yyvsp[0].Attr_val_fifo));
		}
#line 2223 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 64:
#line 553 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.keys = (yyvsp[0].String); }
#line 2229 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 65:
#line 555 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.keysdir = (yyvsp[0].String); }
#line 2235 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 66:
#line 557 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.request_key = (yyvsp[0].Integer); }
#line 2241 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 67:
#line 559 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.revoke = (yyvsp[0].Integer); }
#line 2247 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 68:
#line 561 "ntp_parser.y" /* yacc.c:1646  */
    {
			cfgt.auth.trusted_key_list = (yyvsp[0].Attr_val_fifo);

			// if (!cfgt.auth.trusted_key_list)
			// 	cfgt.auth.trusted_key_list = $2;
			// else
			// 	LINK_SLIST(cfgt.auth.trusted_key_list, $2, link);
		}
#line 2260 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 69:
#line 570 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.ntp_signd_socket = (yyvsp[0].String); }
#line 2266 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 70:
#line 575 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2272 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 71:
#line 577 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2281 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 72:
#line 585 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2287 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 73:
#line 587 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val) = NULL;
			cfgt.auth.revoke = (yyvsp[0].Integer);
			msyslog(LOG_WARNING,
				"'crypto revoke %d' is deprecated, "
				"please use 'revoke %d' instead.",
				cfgt.auth.revoke, cfgt.auth.revoke);
		}
#line 2300 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 79:
#line 612 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.orphan_cmds, (yyvsp[0].Attr_val_fifo)); }
#line 2306 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 80:
#line 617 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2315 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 81:
#line 622 "ntp_parser.y" /* yacc.c:1646  */
    {	
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2324 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 82:
#line 630 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2330 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 83:
#line 632 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2336 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 84:
#line 634 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2342 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 95:
#line 660 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.stats_list, (yyvsp[0].Int_fifo)); }
#line 2348 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 96:
#line 662 "ntp_parser.y" /* yacc.c:1646  */
    {
			if (input_from_file) {
				cfgt.stats_dir = (yyvsp[0].String);
			} else {
				YYFREE((yyvsp[0].String));
				yyerror(ip_file, "statsdir remote configuration ignored");
			}
		}
#line 2361 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 97:
#line 671 "ntp_parser.y" /* yacc.c:1646  */
    {
			filegen_node *fgn;
			
			fgn = create_filegen_node((yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.filegen_opts, fgn);
		}
#line 2372 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 98:
#line 681 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2381 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 99:
#line 686 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2390 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 108:
#line 705 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2396 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 109:
#line 707 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2405 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 110:
#line 715 "ntp_parser.y" /* yacc.c:1646  */
    {
			if (input_from_file) {
				(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			} else {
				(yyval.Attr_val) = NULL;
				YYFREE((yyvsp[0].String));
				yyerror(ip_file, "filegen file remote config ignored");
			}
		}
#line 2419 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 111:
#line 725 "ntp_parser.y" /* yacc.c:1646  */
    {
			if (input_from_file) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror(ip_file, "filegen type remote config ignored");
			}
		}
#line 2432 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 112:
#line 734 "ntp_parser.y" /* yacc.c:1646  */
    {
			const char *err;
			
			if (input_from_file) {
				(yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				if (T_Link == (yyvsp[0].Integer))
					err = "filegen link remote config ignored";
				else
					err = "filegen nolink remote config ignored";
				yyerror(ip_file, err);
			}
		}
#line 2451 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 113:
#line 749 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2457 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 125:
#line 779 "ntp_parser.y" /* yacc.c:1646  */
    {
			CONCAT_G_FIFOS(cfgt.discard_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2465 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 126:
#line 783 "ntp_parser.y" /* yacc.c:1646  */
    {
			CONCAT_G_FIFOS(cfgt.mru_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2473 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 127:
#line 787 "ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[-1].Address_node), NULL, (yyvsp[0].Int_fifo),
						  ip_file->line_no);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2485 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 128:
#line 795 "ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[-3].Address_node), (yyvsp[-1].Address_node), (yyvsp[0].Int_fifo),
						  ip_file->line_no);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2497 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 129:
#line 803 "ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node(NULL, NULL, (yyvsp[0].Int_fifo),
						  ip_file->line_no);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2509 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 130:
#line 811 "ntp_parser.y" /* yacc.c:1646  */
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
				ip_file->line_no);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2528 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 131:
#line 826 "ntp_parser.y" /* yacc.c:1646  */
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
				ip_file->line_no);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2547 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 132:
#line 841 "ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *	rn;

			APPEND_G_FIFO((yyvsp[0].Int_fifo), create_int_node((yyvsp[-1].Integer)));
			rn = create_restrict_node(
				NULL, NULL, (yyvsp[0].Int_fifo), ip_file->line_no);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2560 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 133:
#line 853 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Int_fifo) = NULL; }
#line 2566 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 134:
#line 855 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2575 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 150:
#line 881 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2584 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 151:
#line 886 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2593 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 152:
#line 894 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2599 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 156:
#line 905 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2608 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 157:
#line 910 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2617 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 158:
#line 918 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2623 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 167:
#line 938 "ntp_parser.y" /* yacc.c:1646  */
    {
			addr_opts_node *aon;
			
			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.fudge, aon);
		}
#line 2634 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 168:
#line 948 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2643 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 169:
#line 953 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2652 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 170:
#line 961 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2658 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 171:
#line 963 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2664 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 172:
#line 965 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2670 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 173:
#line 967 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2676 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 174:
#line 969 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2682 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 181:
#line 990 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.rlimit, (yyvsp[0].Attr_val_fifo)); }
#line 2688 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 182:
#line 995 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2697 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 183:
#line 1000 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2706 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 184:
#line 1008 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2712 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 188:
#line 1024 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.enable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 2718 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 189:
#line 1026 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.disable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 2724 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 190:
#line 1031 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2733 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 191:
#line 1036 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2742 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 192:
#line 1044 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2748 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 193:
#line 1046 "ntp_parser.y" /* yacc.c:1646  */
    { 
			if (input_from_file) {
				(yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer));
			} else {
				char err_str[128];
				
				(yyval.Attr_val) = NULL;
				snprintf(err_str, sizeof(err_str),
					 "enable/disable %s remote configuration ignored",
					 keyword((yyvsp[0].Integer)));
				yyerror(ip_file, err_str);
			}
		}
#line 2766 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 202:
#line 1081 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.tinker, (yyvsp[0].Attr_val_fifo)); }
#line 2772 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 203:
#line 1086 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2781 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 204:
#line 1091 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2790 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 205:
#line 1099 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2796 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 216:
#line 1122 "ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;
			
			av = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2807 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 217:
#line 1129 "ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;
			
			av = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2818 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 218:
#line 1136 "ntp_parser.y" /* yacc.c:1646  */
    {
			char error_text[64];
			attr_val *av;

			if (input_from_file) {
				av = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
				APPEND_G_FIFO(cfgt.vars, av);
			} else {
				YYFREE((yyvsp[0].String));
				snprintf(error_text, sizeof(error_text),
					 "%s remote config ignored",
					 keyword((yyvsp[-1].Integer)));
				yyerror(ip_file, error_text);
			}
		}
#line 2838 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 219:
#line 1152 "ntp_parser.y" /* yacc.c:1646  */
    {
			if (!input_from_file) {
				yyerror(ip_file, "remote includefile ignored");
				break;
			}
			if (curr_include_level >= MAXINCLUDELEVEL) {
				fprintf(stderr, "getconfig: Maximum include file level exceeded.\n");
				msyslog(LOG_ERR, "getconfig: Maximum include file level exceeded.");
			} else {
				fp[curr_include_level + 1] = F_OPEN(FindConfig((yyvsp[-1].String)), "r");
				if (fp[curr_include_level + 1] == NULL) {
					fprintf(stderr, "getconfig: Couldn't open <%s>\n", FindConfig((yyvsp[-1].String)));
					msyslog(LOG_ERR, "getconfig: Couldn't open <%s>", FindConfig((yyvsp[-1].String)));
				} else {
					ip_file = fp[++curr_include_level];
				}
			}
		}
#line 2861 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 220:
#line 1171 "ntp_parser.y" /* yacc.c:1646  */
    {
			while (curr_include_level != -1)
				FCLOSE(fp[curr_include_level--]);
		}
#line 2870 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 221:
#line 1176 "ntp_parser.y" /* yacc.c:1646  */
    { /* see drift_parm below for actions */ }
#line 2876 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 222:
#line 1178 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.logconfig, (yyvsp[0].Attr_val_fifo)); }
#line 2882 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 223:
#line 1180 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.phone, (yyvsp[0].String_fifo)); }
#line 2888 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 224:
#line 1182 "ntp_parser.y" /* yacc.c:1646  */
    { APPEND_G_FIFO(cfgt.setvar, (yyvsp[0].Set_var)); }
#line 2894 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 225:
#line 1184 "ntp_parser.y" /* yacc.c:1646  */
    {
			addr_opts_node *aon;
			
			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.trap, aon);
		}
#line 2905 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 226:
#line 1191 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.ttl, (yyvsp[0].Attr_val_fifo)); }
#line 2911 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 235:
#line 1213 "ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;
			
			av = create_attr_sval(T_Driftfile, (yyvsp[0].String));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2922 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 236:
#line 1220 "ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;
			
			av = create_attr_sval(T_Driftfile, (yyvsp[-1].String));
			APPEND_G_FIFO(cfgt.vars, av);
			av = create_attr_dval(T_WanderThreshold, (yyvsp[0].Double));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2935 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 237:
#line 1229 "ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;
			
			av = create_attr_sval(T_Driftfile, "");
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2946 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 238:
#line 1239 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Set_var) = create_setvar_node((yyvsp[-3].String), (yyvsp[-1].String), (yyvsp[0].Integer)); }
#line 2952 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 240:
#line 1245 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 0; }
#line 2958 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 241:
#line 1250 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2964 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 242:
#line 1252 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2973 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 243:
#line 1260 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2979 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 244:
#line 1262 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), estrdup((yyvsp[0].Address_node)->address));
			destroy_address_node((yyvsp[0].Address_node));
		}
#line 2988 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 245:
#line 1270 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2997 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 246:
#line 1275 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3006 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 247:
#line 1283 "ntp_parser.y" /* yacc.c:1646  */
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
#line 3032 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 248:
#line 1308 "ntp_parser.y" /* yacc.c:1646  */
    {
			nic_rule_node *nrn;
			
			nrn = create_nic_rule_node((yyvsp[0].Integer), NULL, (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3043 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 249:
#line 1315 "ntp_parser.y" /* yacc.c:1646  */
    {
			nic_rule_node *nrn;
			
			nrn = create_nic_rule_node(0, (yyvsp[0].String), (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3054 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 259:
#line 1343 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.reset_counters, (yyvsp[0].Int_fifo)); }
#line 3060 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 260:
#line 1348 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3069 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 261:
#line 1353 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3078 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 269:
#line 1377 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3087 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 270:
#line 1382 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3096 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 271:
#line 1390 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3105 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 272:
#line 1395 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3114 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 273:
#line 1403 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival('i', (yyvsp[0].Integer)); }
#line 3120 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 275:
#line 1409 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_rangeval('-', (yyvsp[-3].Integer), (yyvsp[-1].Integer)); }
#line 3126 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 276:
#line 1414 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.String_fifo) = (yyvsp[-1].String_fifo);
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3135 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 277:
#line 1419 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.String_fifo) = NULL;
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3144 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 278:
#line 1427 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Address_fifo) = (yyvsp[-1].Address_fifo);
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3153 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 279:
#line 1432 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Address_fifo) = NULL;
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3162 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 280:
#line 1440 "ntp_parser.y" /* yacc.c:1646  */
    {
			if ((yyvsp[0].Integer) != 0 && (yyvsp[0].Integer) != 1) {
				yyerror(ip_file, "Integer value is not boolean (0 or 1). Assuming 1");
				(yyval.Integer) = 1;
			} else {
				(yyval.Integer) = (yyvsp[0].Integer);
			}
		}
#line 3175 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 281:
#line 1448 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 1; }
#line 3181 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 282:
#line 1449 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 0; }
#line 3187 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 283:
#line 1453 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Double) = (double)(yyvsp[0].Integer); }
#line 3193 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 285:
#line 1464 "ntp_parser.y" /* yacc.c:1646  */
    {
			sim_node *sn;
			
			sn =  create_sim_node((yyvsp[-2].Attr_val_fifo), (yyvsp[-1].Sim_server_fifo));
			APPEND_G_FIFO(cfgt.sim_details, sn);

			/* Revert from ; to \n for end-of-command */
			old_config_style = 1;
		}
#line 3207 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 286:
#line 1481 "ntp_parser.y" /* yacc.c:1646  */
    { old_config_style = 0; }
#line 3213 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 287:
#line 1486 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3222 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 288:
#line 1491 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3231 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 289:
#line 1499 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3237 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 292:
#line 1509 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_server_fifo) = (yyvsp[-1].Sim_server_fifo);
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3246 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 293:
#line 1514 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_server_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3255 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 294:
#line 1522 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Sim_server) = ONLY_SIM(create_sim_server((yyvsp[-4].Address_node), (yyvsp[-2].Double), (yyvsp[-1].Sim_script_fifo))); }
#line 3261 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 295:
#line 1527 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Double) = (yyvsp[-1].Double); }
#line 3267 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 296:
#line 1532 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = (yyvsp[0].Address_node); }
#line 3273 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 297:
#line 1537 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_script_fifo) = (yyvsp[-1].Sim_script_fifo);
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3282 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 298:
#line 1542 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_script_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3291 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 299:
#line 1550 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Sim_script) = ONLY_SIM(create_sim_script_info((yyvsp[-3].Double), (yyvsp[-1].Attr_val_fifo))); }
#line 3297 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 300:
#line 1555 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3306 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 301:
#line 1560 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3315 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;

  case 302:
#line 1568 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3321 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
    break;


#line 3325 "../../ntpd/ntp_parser.c" /* yacc.c:1646  */
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
      yyerror (ip_file, YY_("syntax error"));
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
        yyerror (ip_file, yymsgp);
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
                      yytoken, &yylval, ip_file);
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
                  yystos[yystate], yyvsp, ip_file);
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
  yyerror (ip_file, YY_("memory exhausted"));
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
                  yytoken, &yylval, ip_file);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, ip_file);
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
#line 1579 "ntp_parser.y" /* yacc.c:1906  */


void 
yyerror(
	struct FILE_INFO *ip_file,
	const char *msg
	)
{
	int retval;

	ip_file->err_line_no = ip_file->prev_token_line_no;
	ip_file->err_col_no = ip_file->prev_token_col_no;
	
	msyslog(LOG_ERR, 
		"line %d column %d %s", 
		ip_file->err_line_no,
		ip_file->err_col_no,
		msg);
	if (!input_from_file) {
		/* Save the error message in the correct buffer */
		retval = snprintf(remote_config.err_msg + remote_config.err_pos,
				  MAXLINE - remote_config.err_pos,
				  "column %d %s",
				  ip_file->err_col_no, msg);

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

