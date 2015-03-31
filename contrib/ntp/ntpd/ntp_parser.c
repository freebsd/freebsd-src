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
    T_Mdnstries = 346,
    T_Mem = 347,
    T_Memlock = 348,
    T_Minclock = 349,
    T_Mindepth = 350,
    T_Mindist = 351,
    T_Minimum = 352,
    T_Minpoll = 353,
    T_Minsane = 354,
    T_Mode = 355,
    T_Mode7 = 356,
    T_Monitor = 357,
    T_Month = 358,
    T_Mru = 359,
    T_Multicastclient = 360,
    T_Nic = 361,
    T_Nolink = 362,
    T_Nomodify = 363,
    T_Nomrulist = 364,
    T_None = 365,
    T_Nonvolatile = 366,
    T_Nopeer = 367,
    T_Noquery = 368,
    T_Noselect = 369,
    T_Noserve = 370,
    T_Notrap = 371,
    T_Notrust = 372,
    T_Ntp = 373,
    T_Ntpport = 374,
    T_NtpSignDsocket = 375,
    T_Orphan = 376,
    T_Orphanwait = 377,
    T_Panic = 378,
    T_Peer = 379,
    T_Peerstats = 380,
    T_Phone = 381,
    T_Pid = 382,
    T_Pidfile = 383,
    T_Pool = 384,
    T_Port = 385,
    T_Preempt = 386,
    T_Prefer = 387,
    T_Protostats = 388,
    T_Pw = 389,
    T_Randfile = 390,
    T_Rawstats = 391,
    T_Refid = 392,
    T_Requestkey = 393,
    T_Reset = 394,
    T_Restrict = 395,
    T_Revoke = 396,
    T_Rlimit = 397,
    T_Saveconfigdir = 398,
    T_Server = 399,
    T_Setvar = 400,
    T_Source = 401,
    T_Stacksize = 402,
    T_Statistics = 403,
    T_Stats = 404,
    T_Statsdir = 405,
    T_Step = 406,
    T_Stepout = 407,
    T_Stratum = 408,
    T_String = 409,
    T_Sys = 410,
    T_Sysstats = 411,
    T_Tick = 412,
    T_Time1 = 413,
    T_Time2 = 414,
    T_Timer = 415,
    T_Timingstats = 416,
    T_Tinker = 417,
    T_Tos = 418,
    T_Trap = 419,
    T_True = 420,
    T_Trustedkey = 421,
    T_Ttl = 422,
    T_Type = 423,
    T_U_int = 424,
    T_Unconfig = 425,
    T_Unpeer = 426,
    T_Version = 427,
    T_WanderThreshold = 428,
    T_Week = 429,
    T_Wildcard = 430,
    T_Xleave = 431,
    T_Year = 432,
    T_Flag = 433,
    T_EOC = 434,
    T_Simulate = 435,
    T_Beep_Delay = 436,
    T_Sim_Duration = 437,
    T_Server_Offset = 438,
    T_Duration = 439,
    T_Freq_Offset = 440,
    T_Wander = 441,
    T_Jitter = 442,
    T_Prop_Delay = 443,
    T_Proc_Delay = 444
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
#define T_Mdnstries 346
#define T_Mem 347
#define T_Memlock 348
#define T_Minclock 349
#define T_Mindepth 350
#define T_Mindist 351
#define T_Minimum 352
#define T_Minpoll 353
#define T_Minsane 354
#define T_Mode 355
#define T_Mode7 356
#define T_Monitor 357
#define T_Month 358
#define T_Mru 359
#define T_Multicastclient 360
#define T_Nic 361
#define T_Nolink 362
#define T_Nomodify 363
#define T_Nomrulist 364
#define T_None 365
#define T_Nonvolatile 366
#define T_Nopeer 367
#define T_Noquery 368
#define T_Noselect 369
#define T_Noserve 370
#define T_Notrap 371
#define T_Notrust 372
#define T_Ntp 373
#define T_Ntpport 374
#define T_NtpSignDsocket 375
#define T_Orphan 376
#define T_Orphanwait 377
#define T_Panic 378
#define T_Peer 379
#define T_Peerstats 380
#define T_Phone 381
#define T_Pid 382
#define T_Pidfile 383
#define T_Pool 384
#define T_Port 385
#define T_Preempt 386
#define T_Prefer 387
#define T_Protostats 388
#define T_Pw 389
#define T_Randfile 390
#define T_Rawstats 391
#define T_Refid 392
#define T_Requestkey 393
#define T_Reset 394
#define T_Restrict 395
#define T_Revoke 396
#define T_Rlimit 397
#define T_Saveconfigdir 398
#define T_Server 399
#define T_Setvar 400
#define T_Source 401
#define T_Stacksize 402
#define T_Statistics 403
#define T_Stats 404
#define T_Statsdir 405
#define T_Step 406
#define T_Stepout 407
#define T_Stratum 408
#define T_String 409
#define T_Sys 410
#define T_Sysstats 411
#define T_Tick 412
#define T_Time1 413
#define T_Time2 414
#define T_Timer 415
#define T_Timingstats 416
#define T_Tinker 417
#define T_Tos 418
#define T_Trap 419
#define T_True 420
#define T_Trustedkey 421
#define T_Ttl 422
#define T_Type 423
#define T_U_int 424
#define T_Unconfig 425
#define T_Unpeer 426
#define T_Version 427
#define T_WanderThreshold 428
#define T_Week 429
#define T_Wildcard 430
#define T_Xleave 431
#define T_Year 432
#define T_Flag 433
#define T_EOC 434
#define T_Simulate 435
#define T_Beep_Delay 436
#define T_Sim_Duration 437
#define T_Server_Offset 438
#define T_Duration 439
#define T_Freq_Offset 440
#define T_Wander 441
#define T_Jitter 442
#define T_Prop_Delay 443
#define T_Proc_Delay 444

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

#line 537 "ntp_parser.c" /* yacc.c:355  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (struct FILE_INFO *ip_file);

#endif /* !YY_YY_Y_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 552 "ntp_parser.c" /* yacc.c:358  */

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
#define YYFINAL  205
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   634

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  195
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  104
/* YYNRULES -- Number of rules.  */
#define YYNRULES  308
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  413

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   444

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
     191,   192,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   190,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   193,     2,   194,     2,     2,     2,     2,
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
     185,   186,   187,   188,   189
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   364,   364,   368,   369,   370,   384,   385,   386,   387,
     388,   389,   390,   391,   392,   393,   394,   395,   396,   397,
     405,   415,   416,   417,   418,   419,   423,   424,   429,   434,
     436,   442,   443,   451,   452,   453,   457,   462,   463,   464,
     465,   466,   467,   468,   469,   473,   475,   480,   481,   482,
     483,   484,   485,   489,   494,   503,   513,   514,   524,   526,
     528,   530,   541,   548,   550,   555,   557,   559,   561,   563,
     572,   578,   579,   587,   589,   601,   602,   603,   604,   605,
     614,   619,   624,   632,   634,   636,   641,   642,   643,   644,
     645,   646,   650,   651,   652,   653,   662,   664,   673,   683,
     688,   696,   697,   698,   699,   700,   701,   702,   703,   708,
     709,   717,   727,   736,   751,   756,   757,   761,   762,   766,
     767,   768,   769,   770,   771,   772,   781,   785,   789,   797,
     805,   813,   828,   843,   856,   857,   865,   866,   867,   868,
     869,   870,   871,   872,   873,   874,   875,   876,   877,   878,
     879,   883,   888,   896,   901,   902,   903,   907,   912,   920,
     925,   926,   927,   928,   929,   930,   931,   932,   940,   950,
     955,   963,   965,   967,   969,   971,   976,   977,   981,   982,
     983,   984,   992,   997,  1002,  1010,  1015,  1016,  1017,  1026,
    1028,  1033,  1038,  1046,  1048,  1065,  1066,  1067,  1068,  1069,
    1070,  1074,  1075,  1083,  1088,  1093,  1101,  1106,  1107,  1108,
    1109,  1110,  1111,  1112,  1113,  1122,  1123,  1124,  1131,  1138,
    1154,  1173,  1178,  1180,  1182,  1184,  1186,  1193,  1198,  1199,
    1200,  1204,  1205,  1206,  1210,  1211,  1215,  1222,  1232,  1241,
    1246,  1248,  1253,  1254,  1262,  1264,  1272,  1277,  1285,  1310,
    1317,  1327,  1328,  1332,  1333,  1334,  1335,  1339,  1340,  1341,
    1345,  1350,  1355,  1363,  1364,  1365,  1366,  1367,  1368,  1369,
    1379,  1384,  1392,  1397,  1405,  1407,  1411,  1416,  1421,  1429,
    1434,  1442,  1451,  1452,  1456,  1457,  1466,  1484,  1488,  1493,
    1501,  1506,  1507,  1511,  1516,  1524,  1529,  1534,  1539,  1544,
    1552,  1557,  1562,  1570,  1575,  1576,  1577,  1578,  1579
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
  "T_Maxpoll", "T_Mdnstries", "T_Mem", "T_Memlock", "T_Minclock",
  "T_Mindepth", "T_Mindist", "T_Minimum", "T_Minpoll", "T_Minsane",
  "T_Mode", "T_Mode7", "T_Monitor", "T_Month", "T_Mru",
  "T_Multicastclient", "T_Nic", "T_Nolink", "T_Nomodify", "T_Nomrulist",
  "T_None", "T_Nonvolatile", "T_Nopeer", "T_Noquery", "T_Noselect",
  "T_Noserve", "T_Notrap", "T_Notrust", "T_Ntp", "T_Ntpport",
  "T_NtpSignDsocket", "T_Orphan", "T_Orphanwait", "T_Panic", "T_Peer",
  "T_Peerstats", "T_Phone", "T_Pid", "T_Pidfile", "T_Pool", "T_Port",
  "T_Preempt", "T_Prefer", "T_Protostats", "T_Pw", "T_Randfile",
  "T_Rawstats", "T_Refid", "T_Requestkey", "T_Reset", "T_Restrict",
  "T_Revoke", "T_Rlimit", "T_Saveconfigdir", "T_Server", "T_Setvar",
  "T_Source", "T_Stacksize", "T_Statistics", "T_Stats", "T_Statsdir",
  "T_Step", "T_Stepout", "T_Stratum", "T_String", "T_Sys", "T_Sysstats",
  "T_Tick", "T_Time1", "T_Time2", "T_Timer", "T_Timingstats", "T_Tinker",
  "T_Tos", "T_Trap", "T_True", "T_Trustedkey", "T_Ttl", "T_Type",
  "T_U_int", "T_Unconfig", "T_Unpeer", "T_Version", "T_WanderThreshold",
  "T_Week", "T_Wildcard", "T_Xleave", "T_Year", "T_Flag", "T_EOC",
  "T_Simulate", "T_Beep_Delay", "T_Sim_Duration", "T_Server_Offset",
  "T_Duration", "T_Freq_Offset", "T_Wander", "T_Jitter", "T_Prop_Delay",
  "T_Proc_Delay", "'='", "'('", "')'", "'{'", "'}'", "$accept",
  "configuration", "command_list", "command", "server_command",
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
     435,   436,   437,   438,   439,   440,   441,   442,   443,   444,
      61,    40,    41,   123,   125
};
# endif

#define YYPACT_NINF -180

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-180)))

#define YYTABLE_NINF -7

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      26,  -147,    -9,  -180,  -180,  -180,    -6,  -180,   222,     9,
     -96,   222,  -180,   276,   -41,  -180,   -93,  -180,   -90,   -86,
    -180,   -84,  -180,  -180,   -41,     0,   443,   -41,  -180,  -180,
     -83,  -180,   -82,  -180,  -180,    20,    27,   -20,    21,   -19,
    -180,  -180,   -68,   276,   -66,  -180,   221,   328,   -65,   -55,
      38,  -180,  -180,  -180,    99,   188,   -69,  -180,   -41,  -180,
     -41,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,
    -180,    -3,   -40,   -39,  -180,    -4,  -180,  -180,   -77,  -180,
    -180,  -180,   254,  -180,  -180,  -180,  -180,  -180,  -180,  -180,
    -180,   222,  -180,  -180,  -180,  -180,  -180,  -180,     9,  -180,
      47,    90,  -180,   222,  -180,  -180,  -180,  -180,  -180,  -180,
    -180,  -180,  -180,  -180,  -180,  -180,    98,  -180,   -31,   347,
    -180,  -180,  -180,   -84,  -180,  -180,   -41,  -180,  -180,  -180,
    -180,  -180,  -180,  -180,  -180,  -180,   443,  -180,    61,   -41,
    -180,  -180,   -30,  -180,  -180,  -180,  -180,  -180,  -180,  -180,
    -180,    27,  -180,  -180,   102,   106,  -180,  -180,    55,  -180,
    -180,  -180,  -180,   -19,  -180,    87,   -42,  -180,   276,  -180,
    -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,   221,
    -180,    -3,  -180,  -180,   -27,  -180,  -180,  -180,  -180,  -180,
    -180,  -180,  -180,   328,  -180,    91,    -3,  -180,  -180,    93,
     -55,  -180,  -180,  -180,    96,  -180,   -21,  -180,  -180,  -180,
    -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,     3,  -144,
    -180,  -180,  -180,  -180,  -180,   100,  -180,    -5,  -180,  -180,
    -180,  -180,   -23,     5,  -180,  -180,  -180,  -180,     7,   103,
    -180,  -180,    98,  -180,    -3,   -27,  -180,  -180,  -180,  -180,
    -180,  -180,  -180,  -180,   462,  -180,  -180,   462,   462,   -65,
    -180,  -180,     8,  -180,  -180,  -180,  -180,  -180,  -180,  -180,
    -180,  -180,  -180,   -47,   138,  -180,  -180,  -180,   425,  -180,
    -180,  -180,  -180,  -180,  -180,  -180,  -180,  -105,    -2,   -15,
    -180,  -180,  -180,  -180,    25,  -180,  -180,    17,  -180,  -180,
    -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,
    -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,
    -180,  -180,  -180,   462,   462,  -180,   154,   -65,   124,  -180,
     125,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,
    -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,
     -51,  -180,    32,    10,    16,  -116,  -180,     6,  -180,    -3,
    -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,   462,
    -180,  -180,  -180,  -180,    15,  -180,  -180,  -180,   -41,  -180,
    -180,  -180,    18,  -180,  -180,  -180,    22,    24,    -3,    23,
    -169,  -180,    36,    -3,  -180,  -180,  -180,    29,   -94,  -180,
    -180,  -180,  -180,  -180,    89,    37,    30,  -180,    40,  -180,
      -3,  -180,  -180
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       0,     0,     0,    24,    58,   228,     0,    71,     0,     0,
     238,     0,   221,     0,     0,   231,     0,   251,     0,     0,
     232,     0,   234,    25,     0,     0,     0,     0,   252,   229,
       0,    23,     0,   233,    22,     0,     0,     0,     0,     0,
     235,    21,     0,     0,     0,   230,     0,     0,     0,     0,
       0,    56,    57,   287,     0,     2,     0,     7,     0,     8,
       0,     9,    10,    13,    11,    12,    14,    15,    16,    17,
      18,     0,     0,     0,   215,     0,   216,    19,     0,     5,
      62,    63,    64,   195,   196,   197,   198,   201,   199,   200,
     202,   190,   192,   193,   194,   154,   155,   156,   126,   152,
       0,   236,   222,   189,   101,   102,   103,   104,   108,   105,
     106,   107,   109,    29,    30,    28,     0,    26,     0,     6,
      65,    66,   248,   223,   247,   280,    59,    61,   160,   161,
     162,   163,   164,   165,   166,   167,   127,   158,     0,    60,
      70,   278,   224,    67,   263,   264,   265,   266,   267,   268,
     269,   260,   262,   134,    29,    30,   134,   134,    26,    68,
     188,   186,   187,   182,   184,     0,     0,   225,    96,   100,
      97,   207,   208,   209,   210,   211,   212,   213,   214,   203,
     205,     0,    91,    86,     0,    87,    95,    93,    94,    92,
      90,    88,    89,    80,    82,     0,     0,   242,   274,     0,
      69,   273,   275,   271,   227,     1,     0,     4,    31,    55,
     285,   284,   217,   218,   219,   259,   258,   257,     0,     0,
      79,    75,    76,    77,    78,     0,    72,     0,   191,   151,
     153,   237,    98,     0,   178,   179,   180,   181,     0,     0,
     176,   177,   168,   170,     0,     0,    27,   220,   246,   279,
     157,   159,   277,   261,   130,   134,   134,   133,   128,     0,
     183,   185,     0,    99,   204,   206,   283,   281,   282,    85,
      81,    83,    84,   226,     0,   272,   270,     3,    20,   253,
     254,   255,   250,   256,   249,   291,   292,     0,     0,     0,
      74,    73,   118,   117,     0,   115,   116,     0,   110,   113,
     114,   174,   175,   173,   169,   171,   172,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,   148,
     149,   150,   135,   131,   132,   134,   241,     0,     0,   243,
       0,    37,    38,    39,    54,    47,    49,    48,    51,    40,
      41,    42,    43,    50,    52,    44,    32,    33,    36,    34,
       0,    35,     0,     0,     0,     0,   294,     0,   289,     0,
     111,   125,   121,   123,   119,   120,   122,   124,   112,   129,
     240,   239,   245,   244,     0,    45,    46,    53,     0,   288,
     286,   293,     0,   290,   276,   297,     0,     0,     0,     0,
       0,   299,     0,     0,   295,   298,   296,     0,     0,   304,
     305,   306,   307,   308,     0,     0,     0,   300,     0,   302,
       0,   301,   303
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -180,  -180,  -180,   -37,  -180,  -180,   -14,   -36,  -180,  -180,
    -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,  -180,
    -180,  -180,  -180,  -180,  -180,  -180,    33,  -180,  -180,  -180,
    -180,   -33,  -180,  -180,  -180,  -180,  -180,  -180,  -153,  -180,
    -180,   131,  -180,  -180,    97,  -180,  -180,  -180,   -10,  -180,
    -180,  -180,  -180,    73,  -180,  -180,   227,   -72,  -180,  -180,
    -180,  -180,    60,  -180,  -180,  -180,  -180,  -180,  -180,  -180,
    -180,  -180,  -180,  -180,   119,  -180,  -180,  -180,  -180,  -180,
    -180,    92,  -180,  -180,    44,  -180,  -180,   218,     1,  -179,
    -180,  -180,  -180,   -34,  -180,  -180,  -107,  -180,  -180,  -180,
    -140,  -180,  -150,  -180
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    54,    55,    56,    57,    58,   125,   117,   118,   278,
     346,   347,   348,   349,   350,   351,   352,    59,    60,    61,
      62,    82,   226,   227,    63,   193,   194,   195,   196,    64,
     168,   112,   232,   298,   299,   300,   368,    65,   254,   322,
      98,    99,   100,   136,   137,   138,    66,   242,   243,   244,
     245,    67,   163,   164,   165,    68,    91,    92,    93,    94,
      69,   179,   180,   181,    70,    71,    72,    73,   102,   167,
     371,   273,   329,   123,   124,    74,    75,   284,   218,    76,
     151,   152,   204,   200,   201,   202,   142,   126,   269,   212,
      77,    78,   287,   288,   289,   355,   356,   387,   357,   390,
     391,   404,   405,   406
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
     116,   158,   265,   257,   258,   198,   292,   153,   279,   375,
     169,   266,   197,   293,   327,   389,   294,   272,   206,   228,
      95,   361,   160,   157,   113,   394,   114,     1,   353,   210,
     215,   228,    79,   267,   144,   145,     2,   285,   286,   353,
       3,     4,     5,   362,   208,   154,   209,   155,     6,     7,
     216,    80,   146,   295,    81,     8,     9,   211,   101,    10,
     127,   119,    11,    12,   120,   305,    13,   280,   121,   281,
     122,   140,   141,   217,   161,    14,   285,   286,   380,    15,
     143,   159,   247,   328,   296,    16,   166,    17,   170,   115,
     147,   399,   400,   401,   402,   403,    18,    19,   203,   205,
      20,   233,   323,   324,    21,    22,    96,   230,    23,    24,
     207,    97,   249,   115,   213,   214,   219,    25,   376,   148,
     363,   251,   231,   246,   252,   249,   156,   364,   162,   255,
      26,    27,    28,   256,   115,   263,   199,    29,   268,   259,
     234,   235,   236,   237,   365,   297,    30,   261,   262,   291,
      31,   271,    32,   274,    33,    34,   276,   282,   277,   301,
     290,   302,   326,   303,    35,    36,    37,    38,    39,    40,
      41,    42,   369,   330,    43,   359,    44,   358,   283,   360,
     383,   370,   149,    45,   373,   374,   377,   150,    46,    47,
      48,   366,    49,    50,   367,   379,    51,    52,     2,   382,
     378,   386,     3,     4,     5,    -6,    53,   384,   389,   392,
       6,     7,   388,   393,   397,   396,   409,     8,     9,   411,
     410,    10,   398,   325,    11,    12,   270,   171,    13,   229,
      83,   412,   304,   250,    84,   238,   260,    14,   103,   264,
      85,    15,   248,   253,   275,   139,   306,    16,   381,    17,
     395,   239,   172,   354,   408,     0,   240,   241,    18,    19,
       0,     0,    20,     0,     0,     0,    21,    22,     0,   173,
      23,    24,   174,     0,   399,   400,   401,   402,   403,    25,
       0,     0,   220,   407,     0,     0,     0,     0,     0,     0,
      86,   372,    26,    27,    28,     0,   104,     0,     0,    29,
     105,     0,     0,     0,   221,     0,     0,   222,    30,     0,
       0,     0,    31,     0,    32,     0,    33,    34,     0,     0,
       0,     0,     0,    87,    88,     0,    35,    36,    37,    38,
      39,    40,    41,    42,     0,     0,    43,     0,    44,     0,
      89,   182,     0,     0,   175,    45,     0,   183,     0,   184,
      46,    47,    48,     0,    49,    50,   106,     2,    51,    52,
       0,     3,     4,     5,   385,     0,     0,    -6,    53,     6,
       7,    90,   176,   177,     0,   185,     8,     9,   178,     0,
      10,     0,     0,    11,    12,     0,     0,    13,   223,   224,
       0,     0,     0,     0,     0,   225,    14,     0,     0,     0,
      15,   107,     0,     0,     0,     0,    16,     0,    17,   108,
       0,     0,   109,     0,   186,     0,   187,    18,    19,     0,
       0,    20,   188,     0,   189,    21,    22,   190,     0,    23,
      24,     0,   110,     0,   331,     0,     0,   111,    25,     0,
       0,     0,   332,     0,     0,     0,     0,     0,     0,   191,
     192,    26,    27,    28,     0,     0,     0,     0,    29,     0,
       0,     0,     0,     0,     0,     0,     0,    30,     0,     0,
       0,    31,     0,    32,     0,    33,    34,   333,   334,     0,
       0,     0,     0,     0,     0,    35,    36,    37,    38,    39,
      40,    41,    42,     0,   335,    43,     0,    44,   128,   129,
     130,   131,     0,     0,    45,     0,     0,     0,   307,    46,
      47,    48,     0,    49,    50,   336,   308,    51,    52,     0,
       0,     0,     0,   337,     0,   338,     0,    53,   132,     0,
     133,     0,   134,     0,   309,   310,     0,   311,   135,   339,
       0,     0,     0,   312,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   340,   341,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     313,   314,     0,     0,   315,   316,     0,   317,   318,   319,
       0,   320,     0,     0,     0,     0,     0,     0,     0,     0,
     342,     0,   343,     0,     0,     0,     0,   344,     0,     0,
       0,   345,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   321
};

static const yytype_int16 yycheck[] =
{
      14,    37,   181,   156,   157,    60,    29,    27,     5,    60,
      43,    38,    48,    36,    61,   184,    39,   196,    55,    91,
      11,     4,    41,    37,    65,   194,    67,     1,   144,    32,
      34,   103,   179,    60,     7,     8,    10,   181,   182,   144,
      14,    15,    16,    26,    58,    65,    60,    67,    22,    23,
      54,    60,    25,    76,    60,    29,    30,    60,   154,    33,
      60,   154,    36,    37,   154,   244,    40,    64,   154,    66,
     154,   154,   154,    77,    93,    49,   181,   182,   194,    53,
      60,    60,   119,   130,   107,    59,   154,    61,   154,   154,
      63,   185,   186,   187,   188,   189,    70,    71,    60,     0,
      74,     3,   255,   256,    78,    79,    97,    60,    82,    83,
     179,   102,   126,   154,   154,   154,   193,    91,   169,    92,
     103,    60,    32,   154,   154,   139,   146,   110,   147,    27,
     104,   105,   106,    27,   154,   168,   191,   111,   165,    84,
      42,    43,    44,    45,   127,   168,   120,    60,   190,   154,
     124,    60,   126,    60,   128,   129,    60,   154,   179,   154,
      60,   154,   154,    60,   138,   139,   140,   141,   142,   143,
     144,   145,   325,    35,   148,   190,   150,   179,   175,   154,
     359,    27,   155,   157,    60,    60,   154,   160,   162,   163,
     164,   174,   166,   167,   177,   179,   170,   171,    10,   193,
     190,   183,    14,    15,    16,   179,   180,   192,   184,   388,
      22,    23,   190,   190,   393,   179,   179,    29,    30,   179,
     190,    33,   193,   259,    36,    37,   193,     6,    40,    98,
       8,   410,   242,   136,    12,   137,   163,    49,    11,   179,
      18,    53,   123,   151,   200,    27,   245,    59,   355,    61,
     390,   153,    31,   287,   404,    -1,   158,   159,    70,    71,
      -1,    -1,    74,    -1,    -1,    -1,    78,    79,    -1,    48,
      82,    83,    51,    -1,   185,   186,   187,   188,   189,    91,
      -1,    -1,    28,   194,    -1,    -1,    -1,    -1,    -1,    -1,
      68,   327,   104,   105,   106,    -1,    20,    -1,    -1,   111,
      24,    -1,    -1,    -1,    50,    -1,    -1,    53,   120,    -1,
      -1,    -1,   124,    -1,   126,    -1,   128,   129,    -1,    -1,
      -1,    -1,    -1,   101,   102,    -1,   138,   139,   140,   141,
     142,   143,   144,   145,    -1,    -1,   148,    -1,   150,    -1,
     118,    13,    -1,    -1,   123,   157,    -1,    19,    -1,    21,
     162,   163,   164,    -1,   166,   167,    80,    10,   170,   171,
      -1,    14,    15,    16,   378,    -1,    -1,   179,   180,    22,
      23,   149,   151,   152,    -1,    47,    29,    30,   157,    -1,
      33,    -1,    -1,    36,    37,    -1,    -1,    40,   134,   135,
      -1,    -1,    -1,    -1,    -1,   141,    49,    -1,    -1,    -1,
      53,   125,    -1,    -1,    -1,    -1,    59,    -1,    61,   133,
      -1,    -1,   136,    -1,    86,    -1,    88,    70,    71,    -1,
      -1,    74,    94,    -1,    96,    78,    79,    99,    -1,    82,
      83,    -1,   156,    -1,     9,    -1,    -1,   161,    91,    -1,
      -1,    -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,   121,
     122,   104,   105,   106,    -1,    -1,    -1,    -1,   111,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   120,    -1,    -1,
      -1,   124,    -1,   126,    -1,   128,   129,    52,    53,    -1,
      -1,    -1,    -1,    -1,    -1,   138,   139,   140,   141,   142,
     143,   144,   145,    -1,    69,   148,    -1,   150,    55,    56,
      57,    58,    -1,    -1,   157,    -1,    -1,    -1,    46,   162,
     163,   164,    -1,   166,   167,    90,    54,   170,   171,    -1,
      -1,    -1,    -1,    98,    -1,   100,    -1,   180,    85,    -1,
      87,    -1,    89,    -1,    72,    73,    -1,    75,    95,   114,
      -1,    -1,    -1,    81,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   131,   132,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     108,   109,    -1,    -1,   112,   113,    -1,   115,   116,   117,
      -1,   119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     165,    -1,   167,    -1,    -1,    -1,    -1,   172,    -1,    -1,
      -1,   176,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   172
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,     1,    10,    14,    15,    16,    22,    23,    29,    30,
      33,    36,    37,    40,    49,    53,    59,    61,    70,    71,
      74,    78,    79,    82,    83,    91,   104,   105,   106,   111,
     120,   124,   126,   128,   129,   138,   139,   140,   141,   142,
     143,   144,   145,   148,   150,   157,   162,   163,   164,   166,
     167,   170,   171,   180,   196,   197,   198,   199,   200,   212,
     213,   214,   215,   219,   224,   232,   241,   246,   250,   255,
     259,   260,   261,   262,   270,   271,   274,   285,   286,   179,
      60,    60,   216,     8,    12,    18,    68,   101,   102,   118,
     149,   251,   252,   253,   254,    11,    97,   102,   235,   236,
     237,   154,   263,   251,    20,    24,    80,   125,   133,   136,
     156,   161,   226,    65,    67,   154,   201,   202,   203,   154,
     154,   154,   154,   268,   269,   201,   282,    60,    55,    56,
      57,    58,    85,    87,    89,    95,   238,   239,   240,   282,
     154,   154,   281,    60,     7,     8,    25,    63,    92,   155,
     160,   275,   276,    27,    65,    67,   146,   201,   202,    60,
      41,    93,   147,   247,   248,   249,   154,   264,   225,   226,
     154,     6,    31,    48,    51,   123,   151,   152,   157,   256,
     257,   258,    13,    19,    21,    47,    86,    88,    94,    96,
      99,   121,   122,   220,   221,   222,   223,   202,    60,   191,
     278,   279,   280,    60,   277,     0,   198,   179,   201,   201,
      32,    60,   284,   154,   154,    34,    54,    77,   273,   193,
      28,    50,    53,   134,   135,   141,   217,   218,   252,   236,
      60,    32,   227,     3,    42,    43,    44,    45,   137,   153,
     158,   159,   242,   243,   244,   245,   154,   198,   269,   201,
     239,    60,   154,   276,   233,    27,    27,   233,   233,    84,
     248,    60,   190,   226,   257,   284,    38,    60,   165,   283,
     221,    60,   284,   266,    60,   279,    60,   179,   204,     5,
      64,    66,   154,   175,   272,   181,   182,   287,   288,   289,
      60,   154,    29,    36,    39,    76,   107,   168,   228,   229,
     230,   154,   154,    60,   243,   284,   283,    46,    54,    72,
      73,    75,    81,   108,   109,   112,   113,   115,   116,   117,
     119,   172,   234,   233,   233,   202,   154,    61,   130,   267,
      35,     9,    17,    52,    53,    69,    90,    98,   100,   114,
     131,   132,   165,   167,   172,   176,   205,   206,   207,   208,
     209,   210,   211,   144,   288,   290,   291,   293,   179,   190,
     154,     4,    26,   103,   110,   127,   174,   177,   231,   233,
      27,   265,   202,    60,    60,    60,   169,   154,   190,   179,
     194,   291,   193,   284,   192,   201,   183,   292,   190,   184,
     294,   295,   284,   190,   194,   295,   179,   284,   193,   185,
     186,   187,   188,   189,   296,   297,   298,   194,   297,   179,
     190,   179,   284
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   195,   196,   197,   197,   197,   198,   198,   198,   198,
     198,   198,   198,   198,   198,   198,   198,   198,   198,   198,
     199,   200,   200,   200,   200,   200,   201,   201,   202,   203,
     203,   204,   204,   205,   205,   205,   206,   207,   207,   207,
     207,   207,   207,   207,   207,   208,   208,   209,   209,   209,
     209,   209,   209,   210,   211,   212,   213,   213,   214,   214,
     214,   214,   215,   215,   215,   215,   215,   215,   215,   215,
     215,   216,   216,   217,   217,   218,   218,   218,   218,   218,
     219,   220,   220,   221,   221,   221,   222,   222,   222,   222,
     222,   222,   223,   223,   223,   223,   224,   224,   224,   225,
     225,   226,   226,   226,   226,   226,   226,   226,   226,   227,
     227,   228,   228,   228,   228,   229,   229,   230,   230,   231,
     231,   231,   231,   231,   231,   231,   232,   232,   232,   232,
     232,   232,   232,   232,   233,   233,   234,   234,   234,   234,
     234,   234,   234,   234,   234,   234,   234,   234,   234,   234,
     234,   235,   235,   236,   237,   237,   237,   238,   238,   239,
     240,   240,   240,   240,   240,   240,   240,   240,   241,   242,
     242,   243,   243,   243,   243,   243,   244,   244,   245,   245,
     245,   245,   246,   247,   247,   248,   249,   249,   249,   250,
     250,   251,   251,   252,   252,   253,   253,   253,   253,   253,
     253,   254,   254,   255,   256,   256,   257,   258,   258,   258,
     258,   258,   258,   258,   258,   259,   259,   259,   259,   259,
     259,   259,   259,   259,   259,   259,   259,   259,   260,   260,
     260,   261,   261,   261,   262,   262,   263,   263,   263,   264,
     265,   265,   266,   266,   267,   267,   268,   268,   269,   270,
     270,   271,   271,   272,   272,   272,   272,   273,   273,   273,
     274,   275,   275,   276,   276,   276,   276,   276,   276,   276,
     277,   277,   278,   278,   279,   279,   280,   281,   281,   282,
     282,   283,   283,   283,   284,   284,   285,   286,   287,   287,
     288,   289,   289,   290,   290,   291,   292,   293,   294,   294,
     295,   296,   296,   297,   298,   298,   298,   298,   298
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
       1,     1,     1,     2,     2,     1,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     2,     2,
       3,     1,     2,     2,     2,     2,     3,     2,     1,     1,
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
#line 371 "ntp_parser.y" /* yacc.c:1646  */
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
#line 2092 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 20:
#line 406 "ntp_parser.y" /* yacc.c:1646  */
    {
			peer_node *my_node;

			my_node = create_peer_node((yyvsp[-2].Integer), (yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.peers, my_node);
		}
#line 2103 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 27:
#line 425 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = create_address_node((yyvsp[0].String), (yyvsp[-1].Integer)); }
#line 2109 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 28:
#line 430 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = create_address_node((yyvsp[0].String), AF_UNSPEC); }
#line 2115 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 29:
#line 435 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = AF_INET; }
#line 2121 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 30:
#line 437 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = AF_INET6; }
#line 2127 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 31:
#line 442 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2133 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 32:
#line 444 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2142 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 36:
#line 458 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2148 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 45:
#line 474 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2154 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 46:
#line 476 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_uval((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2160 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 53:
#line 490 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2166 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 55:
#line 504 "ntp_parser.y" /* yacc.c:1646  */
    {
			unpeer_node *my_node;
			
			my_node = create_unpeer_node((yyvsp[0].Address_node));
			if (my_node)
				APPEND_G_FIFO(cfgt.unpeers, my_node);
		}
#line 2178 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 58:
#line 525 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.broadcastclient = 1; }
#line 2184 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 59:
#line 527 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.manycastserver, (yyvsp[0].Address_fifo)); }
#line 2190 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 60:
#line 529 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.multicastclient, (yyvsp[0].Address_fifo)); }
#line 2196 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 61:
#line 531 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.mdnstries = (yyvsp[0].Integer); }
#line 2202 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 62:
#line 542 "ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *atrv;
			
			atrv = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			APPEND_G_FIFO(cfgt.vars, atrv);
		}
#line 2213 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 63:
#line 549 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.control_key = (yyvsp[0].Integer); }
#line 2219 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 64:
#line 551 "ntp_parser.y" /* yacc.c:1646  */
    { 
			cfgt.auth.cryptosw++;
			CONCAT_G_FIFOS(cfgt.auth.crypto_cmd_list, (yyvsp[0].Attr_val_fifo));
		}
#line 2228 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 65:
#line 556 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.keys = (yyvsp[0].String); }
#line 2234 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 66:
#line 558 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.keysdir = (yyvsp[0].String); }
#line 2240 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 67:
#line 560 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.request_key = (yyvsp[0].Integer); }
#line 2246 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 68:
#line 562 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.revoke = (yyvsp[0].Integer); }
#line 2252 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 69:
#line 564 "ntp_parser.y" /* yacc.c:1646  */
    {
			cfgt.auth.trusted_key_list = (yyvsp[0].Attr_val_fifo);

			// if (!cfgt.auth.trusted_key_list)
			// 	cfgt.auth.trusted_key_list = $2;
			// else
			// 	LINK_SLIST(cfgt.auth.trusted_key_list, $2, link);
		}
#line 2265 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 70:
#line 573 "ntp_parser.y" /* yacc.c:1646  */
    { cfgt.auth.ntp_signd_socket = (yyvsp[0].String); }
#line 2271 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 71:
#line 578 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2277 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 72:
#line 580 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2286 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 73:
#line 588 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2292 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 74:
#line 590 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val) = NULL;
			cfgt.auth.revoke = (yyvsp[0].Integer);
			msyslog(LOG_WARNING,
				"'crypto revoke %d' is deprecated, "
				"please use 'revoke %d' instead.",
				cfgt.auth.revoke, cfgt.auth.revoke);
		}
#line 2305 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 80:
#line 615 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.orphan_cmds, (yyvsp[0].Attr_val_fifo)); }
#line 2311 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 81:
#line 620 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2320 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 82:
#line 625 "ntp_parser.y" /* yacc.c:1646  */
    {	
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2329 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 83:
#line 633 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2335 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 84:
#line 635 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2341 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 85:
#line 637 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2347 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 96:
#line 663 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.stats_list, (yyvsp[0].Int_fifo)); }
#line 2353 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 97:
#line 665 "ntp_parser.y" /* yacc.c:1646  */
    {
			if (input_from_file) {
				cfgt.stats_dir = (yyvsp[0].String);
			} else {
				YYFREE((yyvsp[0].String));
				yyerror(ip_file, "statsdir remote configuration ignored");
			}
		}
#line 2366 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 98:
#line 674 "ntp_parser.y" /* yacc.c:1646  */
    {
			filegen_node *fgn;
			
			fgn = create_filegen_node((yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.filegen_opts, fgn);
		}
#line 2377 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 99:
#line 684 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2386 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 100:
#line 689 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2395 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 109:
#line 708 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2401 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 110:
#line 710 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2410 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 111:
#line 718 "ntp_parser.y" /* yacc.c:1646  */
    {
			if (input_from_file) {
				(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			} else {
				(yyval.Attr_val) = NULL;
				YYFREE((yyvsp[0].String));
				yyerror(ip_file, "filegen file remote config ignored");
			}
		}
#line 2424 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 112:
#line 728 "ntp_parser.y" /* yacc.c:1646  */
    {
			if (input_from_file) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror(ip_file, "filegen type remote config ignored");
			}
		}
#line 2437 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 113:
#line 737 "ntp_parser.y" /* yacc.c:1646  */
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
#line 2456 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 114:
#line 752 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2462 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 126:
#line 782 "ntp_parser.y" /* yacc.c:1646  */
    {
			CONCAT_G_FIFOS(cfgt.discard_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2470 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 127:
#line 786 "ntp_parser.y" /* yacc.c:1646  */
    {
			CONCAT_G_FIFOS(cfgt.mru_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2478 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 128:
#line 790 "ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[-1].Address_node), NULL, (yyvsp[0].Int_fifo),
						  ip_file->line_no);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2490 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 129:
#line 798 "ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[-3].Address_node), (yyvsp[-1].Address_node), (yyvsp[0].Int_fifo),
						  ip_file->line_no);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2502 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 130:
#line 806 "ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *rn;

			rn = create_restrict_node(NULL, NULL, (yyvsp[0].Int_fifo),
						  ip_file->line_no);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2514 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 131:
#line 814 "ntp_parser.y" /* yacc.c:1646  */
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
#line 2533 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 132:
#line 829 "ntp_parser.y" /* yacc.c:1646  */
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
#line 2552 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 133:
#line 844 "ntp_parser.y" /* yacc.c:1646  */
    {
			restrict_node *	rn;

			APPEND_G_FIFO((yyvsp[0].Int_fifo), create_int_node((yyvsp[-1].Integer)));
			rn = create_restrict_node(
				NULL, NULL, (yyvsp[0].Int_fifo), ip_file->line_no);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2565 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 134:
#line 856 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Int_fifo) = NULL; }
#line 2571 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 135:
#line 858 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2580 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 151:
#line 884 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2589 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 152:
#line 889 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2598 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 153:
#line 897 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2604 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 157:
#line 908 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2613 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 158:
#line 913 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2622 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 159:
#line 921 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2628 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 168:
#line 941 "ntp_parser.y" /* yacc.c:1646  */
    {
			addr_opts_node *aon;
			
			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.fudge, aon);
		}
#line 2639 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 169:
#line 951 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2648 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 170:
#line 956 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2657 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 171:
#line 964 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2663 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 172:
#line 966 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2669 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 173:
#line 968 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2675 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 174:
#line 970 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2681 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 175:
#line 972 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2687 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 182:
#line 993 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.rlimit, (yyvsp[0].Attr_val_fifo)); }
#line 2693 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 183:
#line 998 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2702 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 184:
#line 1003 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2711 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 185:
#line 1011 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2717 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 189:
#line 1027 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.enable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 2723 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 190:
#line 1029 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.disable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 2729 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 191:
#line 1034 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2738 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 192:
#line 1039 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2747 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 193:
#line 1047 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2753 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 194:
#line 1049 "ntp_parser.y" /* yacc.c:1646  */
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
#line 2771 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 203:
#line 1084 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.tinker, (yyvsp[0].Attr_val_fifo)); }
#line 2777 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 204:
#line 1089 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2786 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 205:
#line 1094 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2795 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 206:
#line 1102 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2801 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 217:
#line 1125 "ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;
			
			av = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2812 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 218:
#line 1132 "ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;
			
			av = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2823 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 219:
#line 1139 "ntp_parser.y" /* yacc.c:1646  */
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
#line 2843 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 220:
#line 1155 "ntp_parser.y" /* yacc.c:1646  */
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
#line 2866 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 221:
#line 1174 "ntp_parser.y" /* yacc.c:1646  */
    {
			while (curr_include_level != -1)
				FCLOSE(fp[curr_include_level--]);
		}
#line 2875 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 222:
#line 1179 "ntp_parser.y" /* yacc.c:1646  */
    { /* see drift_parm below for actions */ }
#line 2881 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 223:
#line 1181 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.logconfig, (yyvsp[0].Attr_val_fifo)); }
#line 2887 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 224:
#line 1183 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.phone, (yyvsp[0].String_fifo)); }
#line 2893 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 225:
#line 1185 "ntp_parser.y" /* yacc.c:1646  */
    { APPEND_G_FIFO(cfgt.setvar, (yyvsp[0].Set_var)); }
#line 2899 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 226:
#line 1187 "ntp_parser.y" /* yacc.c:1646  */
    {
			addr_opts_node *aon;
			
			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.trap, aon);
		}
#line 2910 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 227:
#line 1194 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.ttl, (yyvsp[0].Attr_val_fifo)); }
#line 2916 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 236:
#line 1216 "ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;
			
			av = create_attr_sval(T_Driftfile, (yyvsp[0].String));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2927 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 237:
#line 1223 "ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;
			
			av = create_attr_sval(T_Driftfile, (yyvsp[-1].String));
			APPEND_G_FIFO(cfgt.vars, av);
			av = create_attr_dval(T_WanderThreshold, (yyvsp[0].Double));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2940 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 238:
#line 1232 "ntp_parser.y" /* yacc.c:1646  */
    {
			attr_val *av;
			
			av = create_attr_sval(T_Driftfile, "");
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 2951 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 239:
#line 1242 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Set_var) = create_setvar_node((yyvsp[-3].String), (yyvsp[-1].String), (yyvsp[0].Integer)); }
#line 2957 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 241:
#line 1248 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 0; }
#line 2963 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 242:
#line 1253 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val_fifo) = NULL; }
#line 2969 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 243:
#line 1255 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2978 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 244:
#line 1263 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2984 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 245:
#line 1265 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), estrdup((yyvsp[0].Address_node)->address));
			destroy_address_node((yyvsp[0].Address_node));
		}
#line 2993 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 246:
#line 1273 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3002 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 247:
#line 1278 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3011 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 248:
#line 1286 "ntp_parser.y" /* yacc.c:1646  */
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
#line 3037 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 249:
#line 1311 "ntp_parser.y" /* yacc.c:1646  */
    {
			nic_rule_node *nrn;
			
			nrn = create_nic_rule_node((yyvsp[0].Integer), NULL, (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3048 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 250:
#line 1318 "ntp_parser.y" /* yacc.c:1646  */
    {
			nic_rule_node *nrn;
			
			nrn = create_nic_rule_node(0, (yyvsp[0].String), (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3059 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 260:
#line 1346 "ntp_parser.y" /* yacc.c:1646  */
    { CONCAT_G_FIFOS(cfgt.reset_counters, (yyvsp[0].Int_fifo)); }
#line 3065 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 261:
#line 1351 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3074 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 262:
#line 1356 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3083 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 270:
#line 1380 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3092 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 271:
#line 1385 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3101 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 272:
#line 1393 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3110 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 273:
#line 1398 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3119 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 274:
#line 1406 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_ival('i', (yyvsp[0].Integer)); }
#line 3125 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 276:
#line 1412 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_rangeval('-', (yyvsp[-3].Integer), (yyvsp[-1].Integer)); }
#line 3131 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 277:
#line 1417 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.String_fifo) = (yyvsp[-1].String_fifo);
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3140 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 278:
#line 1422 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.String_fifo) = NULL;
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3149 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 279:
#line 1430 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Address_fifo) = (yyvsp[-1].Address_fifo);
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3158 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 280:
#line 1435 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Address_fifo) = NULL;
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3167 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 281:
#line 1443 "ntp_parser.y" /* yacc.c:1646  */
    {
			if ((yyvsp[0].Integer) != 0 && (yyvsp[0].Integer) != 1) {
				yyerror(ip_file, "Integer value is not boolean (0 or 1). Assuming 1");
				(yyval.Integer) = 1;
			} else {
				(yyval.Integer) = (yyvsp[0].Integer);
			}
		}
#line 3180 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 282:
#line 1451 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 1; }
#line 3186 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 283:
#line 1452 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Integer) = 0; }
#line 3192 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 284:
#line 1456 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Double) = (double)(yyvsp[0].Integer); }
#line 3198 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 286:
#line 1467 "ntp_parser.y" /* yacc.c:1646  */
    {
			sim_node *sn;
			
			sn =  create_sim_node((yyvsp[-2].Attr_val_fifo), (yyvsp[-1].Sim_server_fifo));
			APPEND_G_FIFO(cfgt.sim_details, sn);

			/* Revert from ; to \n for end-of-command */
			old_config_style = 1;
		}
#line 3212 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 287:
#line 1484 "ntp_parser.y" /* yacc.c:1646  */
    { old_config_style = 0; }
#line 3218 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 288:
#line 1489 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3227 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 289:
#line 1494 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3236 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 290:
#line 1502 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3242 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 293:
#line 1512 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_server_fifo) = (yyvsp[-1].Sim_server_fifo);
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3251 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 294:
#line 1517 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_server_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3260 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 295:
#line 1525 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Sim_server) = ONLY_SIM(create_sim_server((yyvsp[-4].Address_node), (yyvsp[-2].Double), (yyvsp[-1].Sim_script_fifo))); }
#line 3266 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 296:
#line 1530 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Double) = (yyvsp[-1].Double); }
#line 3272 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 297:
#line 1535 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Address_node) = (yyvsp[0].Address_node); }
#line 3278 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 298:
#line 1540 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_script_fifo) = (yyvsp[-1].Sim_script_fifo);
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3287 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 299:
#line 1545 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Sim_script_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3296 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 300:
#line 1553 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Sim_script) = ONLY_SIM(create_sim_script_info((yyvsp[-3].Double), (yyvsp[-1].Attr_val_fifo))); }
#line 3302 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 301:
#line 1558 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3311 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 302:
#line 1563 "ntp_parser.y" /* yacc.c:1646  */
    {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3320 "ntp_parser.c" /* yacc.c:1646  */
    break;

  case 303:
#line 1571 "ntp_parser.y" /* yacc.c:1646  */
    { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3326 "ntp_parser.c" /* yacc.c:1646  */
    break;


#line 3330 "ntp_parser.c" /* yacc.c:1646  */
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
#line 1582 "ntp_parser.y" /* yacc.c:1906  */


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

