/* A Bison parser, made by GNU Bison 3.7.6.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30706

/* Bison version string.  */
#define YYBISON_VERSION "3.7.6"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 11 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"

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

#line 106 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
#ifndef YY_YY__SRC_NTP_STABLE_NTPD_NTP_PARSER_H_INCLUDED
# define YY_YY__SRC_NTP_STABLE_NTPD_NTP_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    T_Abbrev = 258,                /* T_Abbrev  */
    T_Age = 259,                   /* T_Age  */
    T_All = 260,                   /* T_All  */
    T_Allan = 261,                 /* T_Allan  */
    T_Allpeers = 262,              /* T_Allpeers  */
    T_Auth = 263,                  /* T_Auth  */
    T_Autokey = 264,               /* T_Autokey  */
    T_Automax = 265,               /* T_Automax  */
    T_Average = 266,               /* T_Average  */
    T_Basedate = 267,              /* T_Basedate  */
    T_Bclient = 268,               /* T_Bclient  */
    T_Bcpollbstep = 269,           /* T_Bcpollbstep  */
    T_Beacon = 270,                /* T_Beacon  */
    T_Broadcast = 271,             /* T_Broadcast  */
    T_Broadcastclient = 272,       /* T_Broadcastclient  */
    T_Broadcastdelay = 273,        /* T_Broadcastdelay  */
    T_Burst = 274,                 /* T_Burst  */
    T_Calibrate = 275,             /* T_Calibrate  */
    T_Ceiling = 276,               /* T_Ceiling  */
    T_Checkhash = 277,             /* T_Checkhash  */
    T_Clockstats = 278,            /* T_Clockstats  */
    T_Cohort = 279,                /* T_Cohort  */
    T_ControlKey = 280,            /* T_ControlKey  */
    T_Crypto = 281,                /* T_Crypto  */
    T_Cryptostats = 282,           /* T_Cryptostats  */
    T_Ctl = 283,                   /* T_Ctl  */
    T_Day = 284,                   /* T_Day  */
    T_Default = 285,               /* T_Default  */
    T_Device = 286,                /* T_Device  */
    T_Digest = 287,                /* T_Digest  */
    T_Disable = 288,               /* T_Disable  */
    T_Discard = 289,               /* T_Discard  */
    T_Dispersion = 290,            /* T_Dispersion  */
    T_Double = 291,                /* T_Double  */
    T_Driftfile = 292,             /* T_Driftfile  */
    T_Drop = 293,                  /* T_Drop  */
    T_Dscp = 294,                  /* T_Dscp  */
    T_Ellipsis = 295,              /* T_Ellipsis  */
    T_Enable = 296,                /* T_Enable  */
    T_End = 297,                   /* T_End  */
    T_Epeer = 298,                 /* T_Epeer  */
    T_False = 299,                 /* T_False  */
    T_File = 300,                  /* T_File  */
    T_Filegen = 301,               /* T_Filegen  */
    T_Filenum = 302,               /* T_Filenum  */
    T_Flag1 = 303,                 /* T_Flag1  */
    T_Flag2 = 304,                 /* T_Flag2  */
    T_Flag3 = 305,                 /* T_Flag3  */
    T_Flag4 = 306,                 /* T_Flag4  */
    T_Flake = 307,                 /* T_Flake  */
    T_Floor = 308,                 /* T_Floor  */
    T_Freq = 309,                  /* T_Freq  */
    T_Fudge = 310,                 /* T_Fudge  */
    T_Fuzz = 311,                  /* T_Fuzz  */
    T_Host = 312,                  /* T_Host  */
    T_Huffpuff = 313,              /* T_Huffpuff  */
    T_Iburst = 314,                /* T_Iburst  */
    T_Ident = 315,                 /* T_Ident  */
    T_Ignore = 316,                /* T_Ignore  */
    T_Ignorehash = 317,            /* T_Ignorehash  */
    T_Incalloc = 318,              /* T_Incalloc  */
    T_Incmem = 319,                /* T_Incmem  */
    T_Initalloc = 320,             /* T_Initalloc  */
    T_Initmem = 321,               /* T_Initmem  */
    T_Includefile = 322,           /* T_Includefile  */
    T_Integer = 323,               /* T_Integer  */
    T_Interface = 324,             /* T_Interface  */
    T_Intrange = 325,              /* T_Intrange  */
    T_Io = 326,                    /* T_Io  */
    T_Ippeerlimit = 327,           /* T_Ippeerlimit  */
    T_Ipv4 = 328,                  /* T_Ipv4  */
    T_Ipv4_flag = 329,             /* T_Ipv4_flag  */
    T_Ipv6 = 330,                  /* T_Ipv6  */
    T_Ipv6_flag = 331,             /* T_Ipv6_flag  */
    T_Kernel = 332,                /* T_Kernel  */
    T_Key = 333,                   /* T_Key  */
    T_Keys = 334,                  /* T_Keys  */
    T_Keysdir = 335,               /* T_Keysdir  */
    T_Kod = 336,                   /* T_Kod  */
    T_Leapfile = 337,              /* T_Leapfile  */
    T_Leapsmearinterval = 338,     /* T_Leapsmearinterval  */
    T_Limited = 339,               /* T_Limited  */
    T_Link = 340,                  /* T_Link  */
    T_Listen = 341,                /* T_Listen  */
    T_Logconfig = 342,             /* T_Logconfig  */
    T_Logfile = 343,               /* T_Logfile  */
    T_Loopstats = 344,             /* T_Loopstats  */
    T_Lowpriotrap = 345,           /* T_Lowpriotrap  */
    T_Manycastclient = 346,        /* T_Manycastclient  */
    T_Manycastserver = 347,        /* T_Manycastserver  */
    T_Mask = 348,                  /* T_Mask  */
    T_Maxage = 349,                /* T_Maxage  */
    T_Maxclock = 350,              /* T_Maxclock  */
    T_Maxdepth = 351,              /* T_Maxdepth  */
    T_Maxdist = 352,               /* T_Maxdist  */
    T_Maxmem = 353,                /* T_Maxmem  */
    T_Maxpoll = 354,               /* T_Maxpoll  */
    T_Mdnstries = 355,             /* T_Mdnstries  */
    T_Mem = 356,                   /* T_Mem  */
    T_Memlock = 357,               /* T_Memlock  */
    T_Minclock = 358,              /* T_Minclock  */
    T_Mindepth = 359,              /* T_Mindepth  */
    T_Mindist = 360,               /* T_Mindist  */
    T_Minimum = 361,               /* T_Minimum  */
    T_Minjitter = 362,             /* T_Minjitter  */
    T_Minpoll = 363,               /* T_Minpoll  */
    T_Minsane = 364,               /* T_Minsane  */
    T_Mode = 365,                  /* T_Mode  */
    T_Mode7 = 366,                 /* T_Mode7  */
    T_Monitor = 367,               /* T_Monitor  */
    T_Month = 368,                 /* T_Month  */
    T_Mru = 369,                   /* T_Mru  */
    T_Mssntp = 370,                /* T_Mssntp  */
    T_Multicastclient = 371,       /* T_Multicastclient  */
    T_Nic = 372,                   /* T_Nic  */
    T_Nolink = 373,                /* T_Nolink  */
    T_Nomodify = 374,              /* T_Nomodify  */
    T_Nomrulist = 375,             /* T_Nomrulist  */
    T_None = 376,                  /* T_None  */
    T_Nonvolatile = 377,           /* T_Nonvolatile  */
    T_Noepeer = 378,               /* T_Noepeer  */
    T_Nopeer = 379,                /* T_Nopeer  */
    T_Noquery = 380,               /* T_Noquery  */
    T_Noselect = 381,              /* T_Noselect  */
    T_Noserve = 382,               /* T_Noserve  */
    T_Notrap = 383,                /* T_Notrap  */
    T_Notrust = 384,               /* T_Notrust  */
    T_Ntp = 385,                   /* T_Ntp  */
    T_Ntpport = 386,               /* T_Ntpport  */
    T_NtpSignDsocket = 387,        /* T_NtpSignDsocket  */
    T_Orphan = 388,                /* T_Orphan  */
    T_Orphanwait = 389,            /* T_Orphanwait  */
    T_PCEdigest = 390,             /* T_PCEdigest  */
    T_Panic = 391,                 /* T_Panic  */
    T_Peer = 392,                  /* T_Peer  */
    T_Peerstats = 393,             /* T_Peerstats  */
    T_Phone = 394,                 /* T_Phone  */
    T_Pid = 395,                   /* T_Pid  */
    T_Pidfile = 396,               /* T_Pidfile  */
    T_Poll = 397,                  /* T_Poll  */
    T_PollSkewList = 398,          /* T_PollSkewList  */
    T_Pool = 399,                  /* T_Pool  */
    T_Port = 400,                  /* T_Port  */
    T_PpsData = 401,               /* T_PpsData  */
    T_Preempt = 402,               /* T_Preempt  */
    T_Prefer = 403,                /* T_Prefer  */
    T_Protostats = 404,            /* T_Protostats  */
    T_Pw = 405,                    /* T_Pw  */
    T_Randfile = 406,              /* T_Randfile  */
    T_Rawstats = 407,              /* T_Rawstats  */
    T_Refid = 408,                 /* T_Refid  */
    T_Requestkey = 409,            /* T_Requestkey  */
    T_Reset = 410,                 /* T_Reset  */
    T_Restrict = 411,              /* T_Restrict  */
    T_Revoke = 412,                /* T_Revoke  */
    T_Rlimit = 413,                /* T_Rlimit  */
    T_Saveconfigdir = 414,         /* T_Saveconfigdir  */
    T_Server = 415,                /* T_Server  */
    T_Serverresponse = 416,        /* T_Serverresponse  */
    T_ServerresponseFuzz = 417,    /* T_ServerresponseFuzz  */
    T_Setvar = 418,                /* T_Setvar  */
    T_Source = 419,                /* T_Source  */
    T_Stacksize = 420,             /* T_Stacksize  */
    T_Statistics = 421,            /* T_Statistics  */
    T_Stats = 422,                 /* T_Stats  */
    T_Statsdir = 423,              /* T_Statsdir  */
    T_Step = 424,                  /* T_Step  */
    T_Stepback = 425,              /* T_Stepback  */
    T_Stepfwd = 426,               /* T_Stepfwd  */
    T_Stepout = 427,               /* T_Stepout  */
    T_Stratum = 428,               /* T_Stratum  */
    T_String = 429,                /* T_String  */
    T_Sys = 430,                   /* T_Sys  */
    T_Sysstats = 431,              /* T_Sysstats  */
    T_Tick = 432,                  /* T_Tick  */
    T_Time1 = 433,                 /* T_Time1  */
    T_Time2 = 434,                 /* T_Time2  */
    T_TimeData = 435,              /* T_TimeData  */
    T_Timer = 436,                 /* T_Timer  */
    T_Timingstats = 437,           /* T_Timingstats  */
    T_Tinker = 438,                /* T_Tinker  */
    T_Tos = 439,                   /* T_Tos  */
    T_Trap = 440,                  /* T_Trap  */
    T_True = 441,                  /* T_True  */
    T_Trustedkey = 442,            /* T_Trustedkey  */
    T_Ttl = 443,                   /* T_Ttl  */
    T_Type = 444,                  /* T_Type  */
    T_U_int = 445,                 /* T_U_int  */
    T_UEcrypto = 446,              /* T_UEcrypto  */
    T_UEcryptonak = 447,           /* T_UEcryptonak  */
    T_UEdigest = 448,              /* T_UEdigest  */
    T_Unconfig = 449,              /* T_Unconfig  */
    T_Unpeer = 450,                /* T_Unpeer  */
    T_Version = 451,               /* T_Version  */
    T_WanderThreshold = 452,       /* T_WanderThreshold  */
    T_Week = 453,                  /* T_Week  */
    T_Wildcard = 454,              /* T_Wildcard  */
    T_Xleave = 455,                /* T_Xleave  */
    T_Xmtnonce = 456,              /* T_Xmtnonce  */
    T_Year = 457,                  /* T_Year  */
    T_Flag = 458,                  /* T_Flag  */
    T_EOC = 459,                   /* T_EOC  */
    T_Simulate = 460,              /* T_Simulate  */
    T_Beep_Delay = 461,            /* T_Beep_Delay  */
    T_Sim_Duration = 462,          /* T_Sim_Duration  */
    T_Server_Offset = 463,         /* T_Server_Offset  */
    T_Duration = 464,              /* T_Duration  */
    T_Freq_Offset = 465,           /* T_Freq_Offset  */
    T_Wander = 466,                /* T_Wander  */
    T_Jitter = 467,                /* T_Jitter  */
    T_Prop_Delay = 468,            /* T_Prop_Delay  */
    T_Proc_Delay = 469             /* T_Proc_Delay  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif
/* Token kinds.  */
#define YYEMPTY -2
#define YYEOF 0
#define YYerror 256
#define YYUNDEF 257
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
#define T_Device 286
#define T_Digest 287
#define T_Disable 288
#define T_Discard 289
#define T_Dispersion 290
#define T_Double 291
#define T_Driftfile 292
#define T_Drop 293
#define T_Dscp 294
#define T_Ellipsis 295
#define T_Enable 296
#define T_End 297
#define T_Epeer 298
#define T_False 299
#define T_File 300
#define T_Filegen 301
#define T_Filenum 302
#define T_Flag1 303
#define T_Flag2 304
#define T_Flag3 305
#define T_Flag4 306
#define T_Flake 307
#define T_Floor 308
#define T_Freq 309
#define T_Fudge 310
#define T_Fuzz 311
#define T_Host 312
#define T_Huffpuff 313
#define T_Iburst 314
#define T_Ident 315
#define T_Ignore 316
#define T_Ignorehash 317
#define T_Incalloc 318
#define T_Incmem 319
#define T_Initalloc 320
#define T_Initmem 321
#define T_Includefile 322
#define T_Integer 323
#define T_Interface 324
#define T_Intrange 325
#define T_Io 326
#define T_Ippeerlimit 327
#define T_Ipv4 328
#define T_Ipv4_flag 329
#define T_Ipv6 330
#define T_Ipv6_flag 331
#define T_Kernel 332
#define T_Key 333
#define T_Keys 334
#define T_Keysdir 335
#define T_Kod 336
#define T_Leapfile 337
#define T_Leapsmearinterval 338
#define T_Limited 339
#define T_Link 340
#define T_Listen 341
#define T_Logconfig 342
#define T_Logfile 343
#define T_Loopstats 344
#define T_Lowpriotrap 345
#define T_Manycastclient 346
#define T_Manycastserver 347
#define T_Mask 348
#define T_Maxage 349
#define T_Maxclock 350
#define T_Maxdepth 351
#define T_Maxdist 352
#define T_Maxmem 353
#define T_Maxpoll 354
#define T_Mdnstries 355
#define T_Mem 356
#define T_Memlock 357
#define T_Minclock 358
#define T_Mindepth 359
#define T_Mindist 360
#define T_Minimum 361
#define T_Minjitter 362
#define T_Minpoll 363
#define T_Minsane 364
#define T_Mode 365
#define T_Mode7 366
#define T_Monitor 367
#define T_Month 368
#define T_Mru 369
#define T_Mssntp 370
#define T_Multicastclient 371
#define T_Nic 372
#define T_Nolink 373
#define T_Nomodify 374
#define T_Nomrulist 375
#define T_None 376
#define T_Nonvolatile 377
#define T_Noepeer 378
#define T_Nopeer 379
#define T_Noquery 380
#define T_Noselect 381
#define T_Noserve 382
#define T_Notrap 383
#define T_Notrust 384
#define T_Ntp 385
#define T_Ntpport 386
#define T_NtpSignDsocket 387
#define T_Orphan 388
#define T_Orphanwait 389
#define T_PCEdigest 390
#define T_Panic 391
#define T_Peer 392
#define T_Peerstats 393
#define T_Phone 394
#define T_Pid 395
#define T_Pidfile 396
#define T_Poll 397
#define T_PollSkewList 398
#define T_Pool 399
#define T_Port 400
#define T_PpsData 401
#define T_Preempt 402
#define T_Prefer 403
#define T_Protostats 404
#define T_Pw 405
#define T_Randfile 406
#define T_Rawstats 407
#define T_Refid 408
#define T_Requestkey 409
#define T_Reset 410
#define T_Restrict 411
#define T_Revoke 412
#define T_Rlimit 413
#define T_Saveconfigdir 414
#define T_Server 415
#define T_Serverresponse 416
#define T_ServerresponseFuzz 417
#define T_Setvar 418
#define T_Source 419
#define T_Stacksize 420
#define T_Statistics 421
#define T_Stats 422
#define T_Statsdir 423
#define T_Step 424
#define T_Stepback 425
#define T_Stepfwd 426
#define T_Stepout 427
#define T_Stratum 428
#define T_String 429
#define T_Sys 430
#define T_Sysstats 431
#define T_Tick 432
#define T_Time1 433
#define T_Time2 434
#define T_TimeData 435
#define T_Timer 436
#define T_Timingstats 437
#define T_Tinker 438
#define T_Tos 439
#define T_Trap 440
#define T_True 441
#define T_Trustedkey 442
#define T_Ttl 443
#define T_Type 444
#define T_U_int 445
#define T_UEcrypto 446
#define T_UEcryptonak 447
#define T_UEdigest 448
#define T_Unconfig 449
#define T_Unpeer 450
#define T_Version 451
#define T_WanderThreshold 452
#define T_Week 453
#define T_Wildcard 454
#define T_Xleave 455
#define T_Xmtnonce 456
#define T_Year 457
#define T_Flag 458
#define T_EOC 459
#define T_Simulate 460
#define T_Beep_Delay 461
#define T_Sim_Duration 462
#define T_Server_Offset 463
#define T_Duration 464
#define T_Freq_Offset 465
#define T_Wander 466
#define T_Jitter 467
#define T_Prop_Delay 468
#define T_Proc_Delay 469

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 52 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"

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

#line 606 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY__SRC_NTP_STABLE_NTPD_NTP_PARSER_H_INCLUDED  */
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_T_Abbrev = 3,                   /* T_Abbrev  */
  YYSYMBOL_T_Age = 4,                      /* T_Age  */
  YYSYMBOL_T_All = 5,                      /* T_All  */
  YYSYMBOL_T_Allan = 6,                    /* T_Allan  */
  YYSYMBOL_T_Allpeers = 7,                 /* T_Allpeers  */
  YYSYMBOL_T_Auth = 8,                     /* T_Auth  */
  YYSYMBOL_T_Autokey = 9,                  /* T_Autokey  */
  YYSYMBOL_T_Automax = 10,                 /* T_Automax  */
  YYSYMBOL_T_Average = 11,                 /* T_Average  */
  YYSYMBOL_T_Basedate = 12,                /* T_Basedate  */
  YYSYMBOL_T_Bclient = 13,                 /* T_Bclient  */
  YYSYMBOL_T_Bcpollbstep = 14,             /* T_Bcpollbstep  */
  YYSYMBOL_T_Beacon = 15,                  /* T_Beacon  */
  YYSYMBOL_T_Broadcast = 16,               /* T_Broadcast  */
  YYSYMBOL_T_Broadcastclient = 17,         /* T_Broadcastclient  */
  YYSYMBOL_T_Broadcastdelay = 18,          /* T_Broadcastdelay  */
  YYSYMBOL_T_Burst = 19,                   /* T_Burst  */
  YYSYMBOL_T_Calibrate = 20,               /* T_Calibrate  */
  YYSYMBOL_T_Ceiling = 21,                 /* T_Ceiling  */
  YYSYMBOL_T_Checkhash = 22,               /* T_Checkhash  */
  YYSYMBOL_T_Clockstats = 23,              /* T_Clockstats  */
  YYSYMBOL_T_Cohort = 24,                  /* T_Cohort  */
  YYSYMBOL_T_ControlKey = 25,              /* T_ControlKey  */
  YYSYMBOL_T_Crypto = 26,                  /* T_Crypto  */
  YYSYMBOL_T_Cryptostats = 27,             /* T_Cryptostats  */
  YYSYMBOL_T_Ctl = 28,                     /* T_Ctl  */
  YYSYMBOL_T_Day = 29,                     /* T_Day  */
  YYSYMBOL_T_Default = 30,                 /* T_Default  */
  YYSYMBOL_T_Device = 31,                  /* T_Device  */
  YYSYMBOL_T_Digest = 32,                  /* T_Digest  */
  YYSYMBOL_T_Disable = 33,                 /* T_Disable  */
  YYSYMBOL_T_Discard = 34,                 /* T_Discard  */
  YYSYMBOL_T_Dispersion = 35,              /* T_Dispersion  */
  YYSYMBOL_T_Double = 36,                  /* T_Double  */
  YYSYMBOL_T_Driftfile = 37,               /* T_Driftfile  */
  YYSYMBOL_T_Drop = 38,                    /* T_Drop  */
  YYSYMBOL_T_Dscp = 39,                    /* T_Dscp  */
  YYSYMBOL_T_Ellipsis = 40,                /* T_Ellipsis  */
  YYSYMBOL_T_Enable = 41,                  /* T_Enable  */
  YYSYMBOL_T_End = 42,                     /* T_End  */
  YYSYMBOL_T_Epeer = 43,                   /* T_Epeer  */
  YYSYMBOL_T_False = 44,                   /* T_False  */
  YYSYMBOL_T_File = 45,                    /* T_File  */
  YYSYMBOL_T_Filegen = 46,                 /* T_Filegen  */
  YYSYMBOL_T_Filenum = 47,                 /* T_Filenum  */
  YYSYMBOL_T_Flag1 = 48,                   /* T_Flag1  */
  YYSYMBOL_T_Flag2 = 49,                   /* T_Flag2  */
  YYSYMBOL_T_Flag3 = 50,                   /* T_Flag3  */
  YYSYMBOL_T_Flag4 = 51,                   /* T_Flag4  */
  YYSYMBOL_T_Flake = 52,                   /* T_Flake  */
  YYSYMBOL_T_Floor = 53,                   /* T_Floor  */
  YYSYMBOL_T_Freq = 54,                    /* T_Freq  */
  YYSYMBOL_T_Fudge = 55,                   /* T_Fudge  */
  YYSYMBOL_T_Fuzz = 56,                    /* T_Fuzz  */
  YYSYMBOL_T_Host = 57,                    /* T_Host  */
  YYSYMBOL_T_Huffpuff = 58,                /* T_Huffpuff  */
  YYSYMBOL_T_Iburst = 59,                  /* T_Iburst  */
  YYSYMBOL_T_Ident = 60,                   /* T_Ident  */
  YYSYMBOL_T_Ignore = 61,                  /* T_Ignore  */
  YYSYMBOL_T_Ignorehash = 62,              /* T_Ignorehash  */
  YYSYMBOL_T_Incalloc = 63,                /* T_Incalloc  */
  YYSYMBOL_T_Incmem = 64,                  /* T_Incmem  */
  YYSYMBOL_T_Initalloc = 65,               /* T_Initalloc  */
  YYSYMBOL_T_Initmem = 66,                 /* T_Initmem  */
  YYSYMBOL_T_Includefile = 67,             /* T_Includefile  */
  YYSYMBOL_T_Integer = 68,                 /* T_Integer  */
  YYSYMBOL_T_Interface = 69,               /* T_Interface  */
  YYSYMBOL_T_Intrange = 70,                /* T_Intrange  */
  YYSYMBOL_T_Io = 71,                      /* T_Io  */
  YYSYMBOL_T_Ippeerlimit = 72,             /* T_Ippeerlimit  */
  YYSYMBOL_T_Ipv4 = 73,                    /* T_Ipv4  */
  YYSYMBOL_T_Ipv4_flag = 74,               /* T_Ipv4_flag  */
  YYSYMBOL_T_Ipv6 = 75,                    /* T_Ipv6  */
  YYSYMBOL_T_Ipv6_flag = 76,               /* T_Ipv6_flag  */
  YYSYMBOL_T_Kernel = 77,                  /* T_Kernel  */
  YYSYMBOL_T_Key = 78,                     /* T_Key  */
  YYSYMBOL_T_Keys = 79,                    /* T_Keys  */
  YYSYMBOL_T_Keysdir = 80,                 /* T_Keysdir  */
  YYSYMBOL_T_Kod = 81,                     /* T_Kod  */
  YYSYMBOL_T_Leapfile = 82,                /* T_Leapfile  */
  YYSYMBOL_T_Leapsmearinterval = 83,       /* T_Leapsmearinterval  */
  YYSYMBOL_T_Limited = 84,                 /* T_Limited  */
  YYSYMBOL_T_Link = 85,                    /* T_Link  */
  YYSYMBOL_T_Listen = 86,                  /* T_Listen  */
  YYSYMBOL_T_Logconfig = 87,               /* T_Logconfig  */
  YYSYMBOL_T_Logfile = 88,                 /* T_Logfile  */
  YYSYMBOL_T_Loopstats = 89,               /* T_Loopstats  */
  YYSYMBOL_T_Lowpriotrap = 90,             /* T_Lowpriotrap  */
  YYSYMBOL_T_Manycastclient = 91,          /* T_Manycastclient  */
  YYSYMBOL_T_Manycastserver = 92,          /* T_Manycastserver  */
  YYSYMBOL_T_Mask = 93,                    /* T_Mask  */
  YYSYMBOL_T_Maxage = 94,                  /* T_Maxage  */
  YYSYMBOL_T_Maxclock = 95,                /* T_Maxclock  */
  YYSYMBOL_T_Maxdepth = 96,                /* T_Maxdepth  */
  YYSYMBOL_T_Maxdist = 97,                 /* T_Maxdist  */
  YYSYMBOL_T_Maxmem = 98,                  /* T_Maxmem  */
  YYSYMBOL_T_Maxpoll = 99,                 /* T_Maxpoll  */
  YYSYMBOL_T_Mdnstries = 100,              /* T_Mdnstries  */
  YYSYMBOL_T_Mem = 101,                    /* T_Mem  */
  YYSYMBOL_T_Memlock = 102,                /* T_Memlock  */
  YYSYMBOL_T_Minclock = 103,               /* T_Minclock  */
  YYSYMBOL_T_Mindepth = 104,               /* T_Mindepth  */
  YYSYMBOL_T_Mindist = 105,                /* T_Mindist  */
  YYSYMBOL_T_Minimum = 106,                /* T_Minimum  */
  YYSYMBOL_T_Minjitter = 107,              /* T_Minjitter  */
  YYSYMBOL_T_Minpoll = 108,                /* T_Minpoll  */
  YYSYMBOL_T_Minsane = 109,                /* T_Minsane  */
  YYSYMBOL_T_Mode = 110,                   /* T_Mode  */
  YYSYMBOL_T_Mode7 = 111,                  /* T_Mode7  */
  YYSYMBOL_T_Monitor = 112,                /* T_Monitor  */
  YYSYMBOL_T_Month = 113,                  /* T_Month  */
  YYSYMBOL_T_Mru = 114,                    /* T_Mru  */
  YYSYMBOL_T_Mssntp = 115,                 /* T_Mssntp  */
  YYSYMBOL_T_Multicastclient = 116,        /* T_Multicastclient  */
  YYSYMBOL_T_Nic = 117,                    /* T_Nic  */
  YYSYMBOL_T_Nolink = 118,                 /* T_Nolink  */
  YYSYMBOL_T_Nomodify = 119,               /* T_Nomodify  */
  YYSYMBOL_T_Nomrulist = 120,              /* T_Nomrulist  */
  YYSYMBOL_T_None = 121,                   /* T_None  */
  YYSYMBOL_T_Nonvolatile = 122,            /* T_Nonvolatile  */
  YYSYMBOL_T_Noepeer = 123,                /* T_Noepeer  */
  YYSYMBOL_T_Nopeer = 124,                 /* T_Nopeer  */
  YYSYMBOL_T_Noquery = 125,                /* T_Noquery  */
  YYSYMBOL_T_Noselect = 126,               /* T_Noselect  */
  YYSYMBOL_T_Noserve = 127,                /* T_Noserve  */
  YYSYMBOL_T_Notrap = 128,                 /* T_Notrap  */
  YYSYMBOL_T_Notrust = 129,                /* T_Notrust  */
  YYSYMBOL_T_Ntp = 130,                    /* T_Ntp  */
  YYSYMBOL_T_Ntpport = 131,                /* T_Ntpport  */
  YYSYMBOL_T_NtpSignDsocket = 132,         /* T_NtpSignDsocket  */
  YYSYMBOL_T_Orphan = 133,                 /* T_Orphan  */
  YYSYMBOL_T_Orphanwait = 134,             /* T_Orphanwait  */
  YYSYMBOL_T_PCEdigest = 135,              /* T_PCEdigest  */
  YYSYMBOL_T_Panic = 136,                  /* T_Panic  */
  YYSYMBOL_T_Peer = 137,                   /* T_Peer  */
  YYSYMBOL_T_Peerstats = 138,              /* T_Peerstats  */
  YYSYMBOL_T_Phone = 139,                  /* T_Phone  */
  YYSYMBOL_T_Pid = 140,                    /* T_Pid  */
  YYSYMBOL_T_Pidfile = 141,                /* T_Pidfile  */
  YYSYMBOL_T_Poll = 142,                   /* T_Poll  */
  YYSYMBOL_T_PollSkewList = 143,           /* T_PollSkewList  */
  YYSYMBOL_T_Pool = 144,                   /* T_Pool  */
  YYSYMBOL_T_Port = 145,                   /* T_Port  */
  YYSYMBOL_T_PpsData = 146,                /* T_PpsData  */
  YYSYMBOL_T_Preempt = 147,                /* T_Preempt  */
  YYSYMBOL_T_Prefer = 148,                 /* T_Prefer  */
  YYSYMBOL_T_Protostats = 149,             /* T_Protostats  */
  YYSYMBOL_T_Pw = 150,                     /* T_Pw  */
  YYSYMBOL_T_Randfile = 151,               /* T_Randfile  */
  YYSYMBOL_T_Rawstats = 152,               /* T_Rawstats  */
  YYSYMBOL_T_Refid = 153,                  /* T_Refid  */
  YYSYMBOL_T_Requestkey = 154,             /* T_Requestkey  */
  YYSYMBOL_T_Reset = 155,                  /* T_Reset  */
  YYSYMBOL_T_Restrict = 156,               /* T_Restrict  */
  YYSYMBOL_T_Revoke = 157,                 /* T_Revoke  */
  YYSYMBOL_T_Rlimit = 158,                 /* T_Rlimit  */
  YYSYMBOL_T_Saveconfigdir = 159,          /* T_Saveconfigdir  */
  YYSYMBOL_T_Server = 160,                 /* T_Server  */
  YYSYMBOL_T_Serverresponse = 161,         /* T_Serverresponse  */
  YYSYMBOL_T_ServerresponseFuzz = 162,     /* T_ServerresponseFuzz  */
  YYSYMBOL_T_Setvar = 163,                 /* T_Setvar  */
  YYSYMBOL_T_Source = 164,                 /* T_Source  */
  YYSYMBOL_T_Stacksize = 165,              /* T_Stacksize  */
  YYSYMBOL_T_Statistics = 166,             /* T_Statistics  */
  YYSYMBOL_T_Stats = 167,                  /* T_Stats  */
  YYSYMBOL_T_Statsdir = 168,               /* T_Statsdir  */
  YYSYMBOL_T_Step = 169,                   /* T_Step  */
  YYSYMBOL_T_Stepback = 170,               /* T_Stepback  */
  YYSYMBOL_T_Stepfwd = 171,                /* T_Stepfwd  */
  YYSYMBOL_T_Stepout = 172,                /* T_Stepout  */
  YYSYMBOL_T_Stratum = 173,                /* T_Stratum  */
  YYSYMBOL_T_String = 174,                 /* T_String  */
  YYSYMBOL_T_Sys = 175,                    /* T_Sys  */
  YYSYMBOL_T_Sysstats = 176,               /* T_Sysstats  */
  YYSYMBOL_T_Tick = 177,                   /* T_Tick  */
  YYSYMBOL_T_Time1 = 178,                  /* T_Time1  */
  YYSYMBOL_T_Time2 = 179,                  /* T_Time2  */
  YYSYMBOL_T_TimeData = 180,               /* T_TimeData  */
  YYSYMBOL_T_Timer = 181,                  /* T_Timer  */
  YYSYMBOL_T_Timingstats = 182,            /* T_Timingstats  */
  YYSYMBOL_T_Tinker = 183,                 /* T_Tinker  */
  YYSYMBOL_T_Tos = 184,                    /* T_Tos  */
  YYSYMBOL_T_Trap = 185,                   /* T_Trap  */
  YYSYMBOL_T_True = 186,                   /* T_True  */
  YYSYMBOL_T_Trustedkey = 187,             /* T_Trustedkey  */
  YYSYMBOL_T_Ttl = 188,                    /* T_Ttl  */
  YYSYMBOL_T_Type = 189,                   /* T_Type  */
  YYSYMBOL_T_U_int = 190,                  /* T_U_int  */
  YYSYMBOL_T_UEcrypto = 191,               /* T_UEcrypto  */
  YYSYMBOL_T_UEcryptonak = 192,            /* T_UEcryptonak  */
  YYSYMBOL_T_UEdigest = 193,               /* T_UEdigest  */
  YYSYMBOL_T_Unconfig = 194,               /* T_Unconfig  */
  YYSYMBOL_T_Unpeer = 195,                 /* T_Unpeer  */
  YYSYMBOL_T_Version = 196,                /* T_Version  */
  YYSYMBOL_T_WanderThreshold = 197,        /* T_WanderThreshold  */
  YYSYMBOL_T_Week = 198,                   /* T_Week  */
  YYSYMBOL_T_Wildcard = 199,               /* T_Wildcard  */
  YYSYMBOL_T_Xleave = 200,                 /* T_Xleave  */
  YYSYMBOL_T_Xmtnonce = 201,               /* T_Xmtnonce  */
  YYSYMBOL_T_Year = 202,                   /* T_Year  */
  YYSYMBOL_T_Flag = 203,                   /* T_Flag  */
  YYSYMBOL_T_EOC = 204,                    /* T_EOC  */
  YYSYMBOL_T_Simulate = 205,               /* T_Simulate  */
  YYSYMBOL_T_Beep_Delay = 206,             /* T_Beep_Delay  */
  YYSYMBOL_T_Sim_Duration = 207,           /* T_Sim_Duration  */
  YYSYMBOL_T_Server_Offset = 208,          /* T_Server_Offset  */
  YYSYMBOL_T_Duration = 209,               /* T_Duration  */
  YYSYMBOL_T_Freq_Offset = 210,            /* T_Freq_Offset  */
  YYSYMBOL_T_Wander = 211,                 /* T_Wander  */
  YYSYMBOL_T_Jitter = 212,                 /* T_Jitter  */
  YYSYMBOL_T_Prop_Delay = 213,             /* T_Prop_Delay  */
  YYSYMBOL_T_Proc_Delay = 214,             /* T_Proc_Delay  */
  YYSYMBOL_215_ = 215,                     /* '|'  */
  YYSYMBOL_216_ = 216,                     /* '='  */
  YYSYMBOL_217_ = 217,                     /* '('  */
  YYSYMBOL_218_ = 218,                     /* ')'  */
  YYSYMBOL_219_ = 219,                     /* '{'  */
  YYSYMBOL_220_ = 220,                     /* '}'  */
  YYSYMBOL_YYACCEPT = 221,                 /* $accept  */
  YYSYMBOL_configuration = 222,            /* configuration  */
  YYSYMBOL_command_list = 223,             /* command_list  */
  YYSYMBOL_command = 224,                  /* command  */
  YYSYMBOL_server_command = 225,           /* server_command  */
  YYSYMBOL_client_type = 226,              /* client_type  */
  YYSYMBOL_address = 227,                  /* address  */
  YYSYMBOL_ip_address = 228,               /* ip_address  */
  YYSYMBOL_address_fam = 229,              /* address_fam  */
  YYSYMBOL_option_list = 230,              /* option_list  */
  YYSYMBOL_option = 231,                   /* option  */
  YYSYMBOL_option_flag = 232,              /* option_flag  */
  YYSYMBOL_option_flag_keyword = 233,      /* option_flag_keyword  */
  YYSYMBOL_option_int = 234,               /* option_int  */
  YYSYMBOL_option_int_keyword = 235,       /* option_int_keyword  */
  YYSYMBOL_option_str = 236,               /* option_str  */
  YYSYMBOL_option_str_keyword = 237,       /* option_str_keyword  */
  YYSYMBOL_unpeer_command = 238,           /* unpeer_command  */
  YYSYMBOL_unpeer_keyword = 239,           /* unpeer_keyword  */
  YYSYMBOL_other_mode_command = 240,       /* other_mode_command  */
  YYSYMBOL_authentication_command = 241,   /* authentication_command  */
  YYSYMBOL_crypto_command_list = 242,      /* crypto_command_list  */
  YYSYMBOL_crypto_command = 243,           /* crypto_command  */
  YYSYMBOL_crypto_str_keyword = 244,       /* crypto_str_keyword  */
  YYSYMBOL_orphan_mode_command = 245,      /* orphan_mode_command  */
  YYSYMBOL_tos_option_list = 246,          /* tos_option_list  */
  YYSYMBOL_tos_option = 247,               /* tos_option  */
  YYSYMBOL_tos_option_int_keyword = 248,   /* tos_option_int_keyword  */
  YYSYMBOL_tos_option_dbl_keyword = 249,   /* tos_option_dbl_keyword  */
  YYSYMBOL_monitoring_command = 250,       /* monitoring_command  */
  YYSYMBOL_stats_list = 251,               /* stats_list  */
  YYSYMBOL_stat = 252,                     /* stat  */
  YYSYMBOL_filegen_option_list = 253,      /* filegen_option_list  */
  YYSYMBOL_filegen_option = 254,           /* filegen_option  */
  YYSYMBOL_link_nolink = 255,              /* link_nolink  */
  YYSYMBOL_enable_disable = 256,           /* enable_disable  */
  YYSYMBOL_filegen_type = 257,             /* filegen_type  */
  YYSYMBOL_access_control_command = 258,   /* access_control_command  */
  YYSYMBOL_res_ippeerlimit = 259,          /* res_ippeerlimit  */
  YYSYMBOL_ac_flag_list = 260,             /* ac_flag_list  */
  YYSYMBOL_access_control_flag = 261,      /* access_control_flag  */
  YYSYMBOL_discard_option_list = 262,      /* discard_option_list  */
  YYSYMBOL_discard_option = 263,           /* discard_option  */
  YYSYMBOL_discard_option_keyword = 264,   /* discard_option_keyword  */
  YYSYMBOL_mru_option_list = 265,          /* mru_option_list  */
  YYSYMBOL_mru_option = 266,               /* mru_option  */
  YYSYMBOL_mru_option_keyword = 267,       /* mru_option_keyword  */
  YYSYMBOL_fudge_command = 268,            /* fudge_command  */
  YYSYMBOL_fudge_factor_list = 269,        /* fudge_factor_list  */
  YYSYMBOL_fudge_factor = 270,             /* fudge_factor  */
  YYSYMBOL_fudge_factor_dbl_keyword = 271, /* fudge_factor_dbl_keyword  */
  YYSYMBOL_fudge_factor_bool_keyword = 272, /* fudge_factor_bool_keyword  */
  YYSYMBOL_device_command = 273,           /* device_command  */
  YYSYMBOL_device_item_list = 274,         /* device_item_list  */
  YYSYMBOL_device_item = 275,              /* device_item  */
  YYSYMBOL_device_item_path_keyword = 276, /* device_item_path_keyword  */
  YYSYMBOL_rlimit_command = 277,           /* rlimit_command  */
  YYSYMBOL_rlimit_option_list = 278,       /* rlimit_option_list  */
  YYSYMBOL_rlimit_option = 279,            /* rlimit_option  */
  YYSYMBOL_rlimit_option_keyword = 280,    /* rlimit_option_keyword  */
  YYSYMBOL_system_option_command = 281,    /* system_option_command  */
  YYSYMBOL_system_option_list = 282,       /* system_option_list  */
  YYSYMBOL_system_option = 283,            /* system_option  */
  YYSYMBOL_system_option_flag_keyword = 284, /* system_option_flag_keyword  */
  YYSYMBOL_system_option_local_flag_keyword = 285, /* system_option_local_flag_keyword  */
  YYSYMBOL_tinker_command = 286,           /* tinker_command  */
  YYSYMBOL_tinker_option_list = 287,       /* tinker_option_list  */
  YYSYMBOL_tinker_option = 288,            /* tinker_option  */
  YYSYMBOL_tinker_option_keyword = 289,    /* tinker_option_keyword  */
  YYSYMBOL_miscellaneous_command = 290,    /* miscellaneous_command  */
  YYSYMBOL_misc_cmd_dbl_keyword = 291,     /* misc_cmd_dbl_keyword  */
  YYSYMBOL_misc_cmd_int_keyword = 292,     /* misc_cmd_int_keyword  */
  YYSYMBOL_opt_hash_check = 293,           /* opt_hash_check  */
  YYSYMBOL_misc_cmd_str_keyword = 294,     /* misc_cmd_str_keyword  */
  YYSYMBOL_misc_cmd_str_lcl_keyword = 295, /* misc_cmd_str_lcl_keyword  */
  YYSYMBOL_drift_parm = 296,               /* drift_parm  */
  YYSYMBOL_pollskew_list = 297,            /* pollskew_list  */
  YYSYMBOL_pollskew_spec = 298,            /* pollskew_spec  */
  YYSYMBOL_pollskew_cycle = 299,           /* pollskew_cycle  */
  YYSYMBOL_variable_assign = 300,          /* variable_assign  */
  YYSYMBOL_t_default_or_zero = 301,        /* t_default_or_zero  */
  YYSYMBOL_trap_option_list = 302,         /* trap_option_list  */
  YYSYMBOL_trap_option = 303,              /* trap_option  */
  YYSYMBOL_log_config_list = 304,          /* log_config_list  */
  YYSYMBOL_log_config_command = 305,       /* log_config_command  */
  YYSYMBOL_interface_command = 306,        /* interface_command  */
  YYSYMBOL_interface_nic = 307,            /* interface_nic  */
  YYSYMBOL_nic_rule_class = 308,           /* nic_rule_class  */
  YYSYMBOL_nic_rule_action = 309,          /* nic_rule_action  */
  YYSYMBOL_reset_command = 310,            /* reset_command  */
  YYSYMBOL_counter_set_list = 311,         /* counter_set_list  */
  YYSYMBOL_counter_set_keyword = 312,      /* counter_set_keyword  */
  YYSYMBOL_integer_list = 313,             /* integer_list  */
  YYSYMBOL_integer_list_range = 314,       /* integer_list_range  */
  YYSYMBOL_integer_list_range_elt = 315,   /* integer_list_range_elt  */
  YYSYMBOL_integer_range = 316,            /* integer_range  */
  YYSYMBOL_string_list = 317,              /* string_list  */
  YYSYMBOL_address_list = 318,             /* address_list  */
  YYSYMBOL_boolean = 319,                  /* boolean  */
  YYSYMBOL_number = 320,                   /* number  */
  YYSYMBOL_basedate = 321,                 /* basedate  */
  YYSYMBOL_simulate_command = 322,         /* simulate_command  */
  YYSYMBOL_sim_conf_start = 323,           /* sim_conf_start  */
  YYSYMBOL_sim_init_statement_list = 324,  /* sim_init_statement_list  */
  YYSYMBOL_sim_init_statement = 325,       /* sim_init_statement  */
  YYSYMBOL_sim_init_keyword = 326,         /* sim_init_keyword  */
  YYSYMBOL_sim_server_list = 327,          /* sim_server_list  */
  YYSYMBOL_sim_server = 328,               /* sim_server  */
  YYSYMBOL_sim_server_offset = 329,        /* sim_server_offset  */
  YYSYMBOL_sim_server_name = 330,          /* sim_server_name  */
  YYSYMBOL_sim_act_list = 331,             /* sim_act_list  */
  YYSYMBOL_sim_act = 332,                  /* sim_act  */
  YYSYMBOL_sim_act_stmt_list = 333,        /* sim_act_stmt_list  */
  YYSYMBOL_sim_act_stmt = 334,             /* sim_act_stmt  */
  YYSYMBOL_sim_act_keyword = 335           /* sim_act_keyword  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

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


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
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

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

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
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
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
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  222
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   688

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  221
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  115
/* YYNRULES -- Number of rules.  */
#define YYNRULES  343
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  463

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   469


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     217,   218,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   216,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   219,   215,   220,     2,     2,     2,     2,
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
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   397,   397,   401,   402,   403,   418,   419,   420,   421,
     422,   423,   424,   425,   426,   427,   428,   429,   430,   431,
     432,   440,   450,   451,   452,   453,   454,   458,   459,   464,
     469,   471,   477,   478,   486,   487,   488,   492,   497,   498,
     499,   500,   501,   502,   503,   504,   505,   509,   511,   516,
     517,   518,   519,   520,   521,   525,   530,   539,   549,   550,
     560,   562,   564,   566,   577,   584,   586,   591,   593,   595,
     597,   599,   609,   615,   616,   624,   626,   638,   639,   640,
     641,   642,   651,   656,   661,   669,   671,   673,   675,   680,
     681,   682,   683,   684,   685,   686,   687,   688,   692,   693,
     702,   704,   713,   723,   728,   736,   737,   738,   739,   740,
     741,   742,   743,   748,   749,   757,   767,   776,   791,   796,
     797,   801,   802,   806,   807,   808,   809,   810,   811,   812,
     821,   825,   829,   837,   845,   853,   868,   883,   896,   897,
     917,   918,   926,   937,   938,   939,   940,   941,   942,   943,
     944,   945,   946,   947,   948,   949,   950,   951,   952,   953,
     957,   962,   970,   975,   976,   977,   981,   986,   994,   999,
    1000,  1001,  1002,  1003,  1004,  1005,  1006,  1014,  1024,  1029,
    1037,  1039,  1041,  1050,  1052,  1057,  1058,  1059,  1063,  1064,
    1065,  1066,  1074,  1084,  1089,  1097,  1102,  1103,  1111,  1116,
    1121,  1129,  1134,  1135,  1136,  1145,  1147,  1152,  1157,  1165,
    1167,  1184,  1185,  1186,  1187,  1188,  1189,  1193,  1194,  1195,
    1196,  1197,  1198,  1206,  1211,  1216,  1224,  1229,  1230,  1231,
    1232,  1233,  1234,  1235,  1236,  1237,  1238,  1247,  1248,  1249,
    1256,  1263,  1270,  1286,  1305,  1313,  1315,  1317,  1319,  1321,
    1323,  1325,  1332,  1337,  1338,  1339,  1343,  1347,  1356,  1358,
    1361,  1365,  1369,  1370,  1371,  1375,  1386,  1404,  1417,  1418,
    1423,  1449,  1450,  1455,  1460,  1462,  1467,  1468,  1476,  1478,
    1486,  1491,  1499,  1524,  1531,  1541,  1542,  1546,  1547,  1548,
    1549,  1553,  1554,  1555,  1559,  1564,  1569,  1577,  1578,  1579,
    1580,  1581,  1582,  1583,  1593,  1598,  1606,  1611,  1619,  1621,
    1625,  1630,  1635,  1643,  1648,  1656,  1665,  1666,  1670,  1671,
    1675,  1683,  1701,  1705,  1710,  1718,  1723,  1724,  1728,  1733,
    1741,  1746,  1751,  1756,  1761,  1769,  1774,  1779,  1787,  1792,
    1793,  1794,  1795,  1796
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "T_Abbrev", "T_Age",
  "T_All", "T_Allan", "T_Allpeers", "T_Auth", "T_Autokey", "T_Automax",
  "T_Average", "T_Basedate", "T_Bclient", "T_Bcpollbstep", "T_Beacon",
  "T_Broadcast", "T_Broadcastclient", "T_Broadcastdelay", "T_Burst",
  "T_Calibrate", "T_Ceiling", "T_Checkhash", "T_Clockstats", "T_Cohort",
  "T_ControlKey", "T_Crypto", "T_Cryptostats", "T_Ctl", "T_Day",
  "T_Default", "T_Device", "T_Digest", "T_Disable", "T_Discard",
  "T_Dispersion", "T_Double", "T_Driftfile", "T_Drop", "T_Dscp",
  "T_Ellipsis", "T_Enable", "T_End", "T_Epeer", "T_False", "T_File",
  "T_Filegen", "T_Filenum", "T_Flag1", "T_Flag2", "T_Flag3", "T_Flag4",
  "T_Flake", "T_Floor", "T_Freq", "T_Fudge", "T_Fuzz", "T_Host",
  "T_Huffpuff", "T_Iburst", "T_Ident", "T_Ignore", "T_Ignorehash",
  "T_Incalloc", "T_Incmem", "T_Initalloc", "T_Initmem", "T_Includefile",
  "T_Integer", "T_Interface", "T_Intrange", "T_Io", "T_Ippeerlimit",
  "T_Ipv4", "T_Ipv4_flag", "T_Ipv6", "T_Ipv6_flag", "T_Kernel", "T_Key",
  "T_Keys", "T_Keysdir", "T_Kod", "T_Leapfile", "T_Leapsmearinterval",
  "T_Limited", "T_Link", "T_Listen", "T_Logconfig", "T_Logfile",
  "T_Loopstats", "T_Lowpriotrap", "T_Manycastclient", "T_Manycastserver",
  "T_Mask", "T_Maxage", "T_Maxclock", "T_Maxdepth", "T_Maxdist",
  "T_Maxmem", "T_Maxpoll", "T_Mdnstries", "T_Mem", "T_Memlock",
  "T_Minclock", "T_Mindepth", "T_Mindist", "T_Minimum", "T_Minjitter",
  "T_Minpoll", "T_Minsane", "T_Mode", "T_Mode7", "T_Monitor", "T_Month",
  "T_Mru", "T_Mssntp", "T_Multicastclient", "T_Nic", "T_Nolink",
  "T_Nomodify", "T_Nomrulist", "T_None", "T_Nonvolatile", "T_Noepeer",
  "T_Nopeer", "T_Noquery", "T_Noselect", "T_Noserve", "T_Notrap",
  "T_Notrust", "T_Ntp", "T_Ntpport", "T_NtpSignDsocket", "T_Orphan",
  "T_Orphanwait", "T_PCEdigest", "T_Panic", "T_Peer", "T_Peerstats",
  "T_Phone", "T_Pid", "T_Pidfile", "T_Poll", "T_PollSkewList", "T_Pool",
  "T_Port", "T_PpsData", "T_Preempt", "T_Prefer", "T_Protostats", "T_Pw",
  "T_Randfile", "T_Rawstats", "T_Refid", "T_Requestkey", "T_Reset",
  "T_Restrict", "T_Revoke", "T_Rlimit", "T_Saveconfigdir", "T_Server",
  "T_Serverresponse", "T_ServerresponseFuzz", "T_Setvar", "T_Source",
  "T_Stacksize", "T_Statistics", "T_Stats", "T_Statsdir", "T_Step",
  "T_Stepback", "T_Stepfwd", "T_Stepout", "T_Stratum", "T_String", "T_Sys",
  "T_Sysstats", "T_Tick", "T_Time1", "T_Time2", "T_TimeData", "T_Timer",
  "T_Timingstats", "T_Tinker", "T_Tos", "T_Trap", "T_True", "T_Trustedkey",
  "T_Ttl", "T_Type", "T_U_int", "T_UEcrypto", "T_UEcryptonak",
  "T_UEdigest", "T_Unconfig", "T_Unpeer", "T_Version", "T_WanderThreshold",
  "T_Week", "T_Wildcard", "T_Xleave", "T_Xmtnonce", "T_Year", "T_Flag",
  "T_EOC", "T_Simulate", "T_Beep_Delay", "T_Sim_Duration",
  "T_Server_Offset", "T_Duration", "T_Freq_Offset", "T_Wander", "T_Jitter",
  "T_Prop_Delay", "T_Proc_Delay", "'|'", "'='", "'('", "')'", "'{'", "'}'",
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
  "access_control_command", "res_ippeerlimit", "ac_flag_list",
  "access_control_flag", "discard_option_list", "discard_option",
  "discard_option_keyword", "mru_option_list", "mru_option",
  "mru_option_keyword", "fudge_command", "fudge_factor_list",
  "fudge_factor", "fudge_factor_dbl_keyword", "fudge_factor_bool_keyword",
  "device_command", "device_item_list", "device_item",
  "device_item_path_keyword", "rlimit_command", "rlimit_option_list",
  "rlimit_option", "rlimit_option_keyword", "system_option_command",
  "system_option_list", "system_option", "system_option_flag_keyword",
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

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
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
     465,   466,   467,   468,   469,   124,    61,    40,    41,   123,
     125
};
#endif

#define YYPACT_NINF (-247)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-7)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
       5,  -178,   -19,  -247,  -247,  -247,    -2,  -247,   -58,   294,
      29,   -94,  -247,   294,  -247,   109,   -58,  -247,   -79,  -247,
     -70,   -68,   -65,  -247,   -63,  -247,  -247,   -58,    42,   205,
     -58,  -247,  -247,   -56,  -247,   -50,  -247,  -247,  -247,    45,
      27,    41,    55,   -34,  -247,  -247,   -49,   109,   -41,  -247,
     271,   554,   -40,   -64,    72,  -247,  -247,  -247,   145,   196,
     -54,  -247,   -58,  -247,   -58,  -247,  -247,  -247,  -247,  -247,
    -247,  -247,  -247,  -247,  -247,  -247,    33,    83,   -22,   -17,
    -247,   -11,  -247,  -247,   -52,  -247,  -247,  -247,    26,  -247,
    -247,  -247,  -121,  -247,    -5,  -247,  -247,  -247,  -247,  -247,
    -247,  -247,  -247,  -247,  -247,  -247,  -247,   294,  -247,  -247,
    -247,  -247,  -247,  -247,    29,  -247,    86,   119,  -247,   294,
    -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,   319,
     377,  -247,  -247,   -10,  -247,   -63,  -247,  -247,   -58,  -247,
    -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,   205,  -247,
     104,   -58,  -247,  -247,     7,    23,  -247,  -247,  -247,  -247,
    -247,  -247,  -247,  -247,    27,  -247,   103,   154,   156,   103,
       1,  -247,  -247,  -247,  -247,   -34,  -247,   126,   -29,  -247,
     109,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,
    -247,  -247,  -247,   271,  -247,    33,    30,  -247,  -247,  -247,
     -20,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,   554,
    -247,   139,    33,  -247,  -247,  -247,   143,   -64,  -247,  -247,
    -247,   155,  -247,    20,  -247,  -247,  -247,  -247,  -247,  -247,
    -247,  -247,  -247,  -247,  -247,  -247,     4,  -104,  -247,  -247,
    -247,  -247,  -247,   157,  -247,    54,  -247,  -247,  -121,  -247,
      57,  -247,  -247,  -247,  -247,  -247,    -4,    58,  -247,  -247,
    -247,  -247,  -247,    67,   175,  -247,  -247,   319,  -247,    33,
     -20,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,
    -247,  -247,  -247,   178,  -247,   184,  -247,   103,   103,  -247,
     -40,  -247,  -247,  -247,    79,  -247,  -247,  -247,  -247,  -247,
    -247,  -247,  -247,  -247,  -247,  -247,   -55,   214,  -247,  -247,
    -247,    48,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,
    -143,    51,    43,  -247,  -247,  -247,  -247,  -247,  -247,    88,
    -247,  -247,    -1,  -247,  -247,  -247,  -247,  -247,  -247,  -247,
    -247,  -247,    49,  -247,   465,  -247,  -247,   465,   103,   465,
     227,   -40,   192,  -247,   198,  -247,  -247,  -247,  -247,  -247,
    -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,
    -247,  -247,  -247,  -247,  -247,   -61,  -247,    98,    64,    69,
    -150,  -247,    62,  -247,    33,  -247,  -247,  -247,  -247,  -247,
    -247,  -247,  -247,  -247,   206,  -247,  -247,  -247,  -247,  -247,
    -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,
    -247,   226,  -247,  -247,   465,   465,  -247,  -247,  -247,  -247,
    -247,    68,  -247,  -247,  -247,   -58,  -247,  -247,  -247,    81,
    -247,  -247,  -247,   465,  -247,  -247,    74,    84,    33,    76,
    -131,  -247,    90,    33,  -247,  -247,  -247,    78,   131,  -247,
    -247,  -247,  -247,  -247,     6,    91,    82,  -247,   100,  -247,
      33,  -247,  -247
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       0,     0,     0,    25,    60,   253,     0,    73,     0,     0,
       0,   267,   256,     0,   245,     0,     0,   261,     0,   285,
       0,     0,     0,   257,     0,   262,    26,     0,     0,     0,
       0,   286,   254,     0,    24,     0,   263,   268,    23,     0,
       0,     0,     0,     0,   264,    22,     0,     0,     0,   255,
       0,     0,     0,     0,     0,    58,    59,   322,     0,     2,
       0,     7,     0,     8,     0,     9,    10,    13,    11,    12,
      14,    20,    15,    16,    17,    18,     0,     0,     0,     0,
     237,     0,   238,    19,     0,     5,    64,    65,    66,    30,
      31,    29,     0,    27,     0,   211,   212,   213,   214,   217,
     215,   216,   218,   219,   220,   221,   222,   206,   208,   209,
     210,   163,   164,   165,   130,   161,     0,   265,   246,   205,
     105,   106,   107,   108,   112,   109,   110,   111,   113,     0,
       6,    67,    68,   260,   282,   247,   281,   314,    61,    63,
     169,   170,   171,   172,   173,   174,   175,   176,   131,   167,
       0,    62,    72,   312,   248,   249,    69,   297,   298,   299,
     300,   301,   302,   303,   294,   296,   138,    30,    31,   138,
     138,    70,   204,   202,   203,   198,   200,     0,     0,   250,
     100,   104,   101,   227,   228,   229,   230,   231,   232,   233,
     234,   235,   236,   223,   225,     0,     0,    89,    90,    91,
       0,    92,    93,    99,    94,    98,    95,    96,    97,    82,
      84,     0,     0,    88,   276,   308,     0,    71,   307,   309,
     305,   252,     1,     0,     4,    32,    57,   319,   318,   239,
     240,   241,   242,   293,   292,   291,     0,     0,    81,    77,
      78,    79,    80,     0,    74,     0,   197,   196,   192,   194,
       0,    28,   207,   160,   162,   266,   102,     0,   188,   189,
     190,   191,   187,     0,     0,   185,   186,   177,   179,     0,
       0,   243,   259,   258,   244,   280,   313,   166,   168,   311,
     272,   271,   269,     0,   295,     0,   140,   138,   138,   140,
       0,   140,   199,   201,     0,   103,   224,   226,   320,   317,
     315,   316,    87,    83,    85,    86,   251,     0,   306,   304,
       3,    21,   287,   288,   289,   284,   290,   283,   326,   327,
       0,     0,     0,    76,    75,   193,   195,   122,   121,     0,
     119,   120,     0,   114,   117,   118,   183,   184,   182,   178,
     180,   181,     0,   139,   134,   140,   140,   137,   138,   132,
     275,     0,     0,   277,     0,    38,    39,    40,    56,    49,
      51,    50,    53,    41,    42,    43,    44,    52,    54,    45,
      46,    33,    34,    37,    35,     0,    36,     0,     0,     0,
       0,   329,     0,   324,     0,   115,   129,   125,   127,   123,
     124,   126,   128,   116,     0,   143,   144,   145,   146,   147,
     148,   149,   151,   152,   150,   153,   154,   155,   156,   157,
     158,     0,   159,   141,   135,   136,   140,   274,   273,   279,
     278,     0,    47,    48,    55,     0,   323,   321,   328,     0,
     325,   270,   142,   133,   310,   332,     0,     0,     0,     0,
       0,   334,     0,     0,   330,   333,   331,     0,     0,   339,
     340,   341,   342,   343,     0,     0,     0,   335,     0,   337,
       0,   336,   338
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -247,  -247,  -247,   -48,  -247,  -247,    -8,   -51,  -247,  -247,
    -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,
    -247,  -247,  -247,  -247,  -247,  -247,    96,  -247,  -247,  -247,
    -247,   -42,  -247,  -247,  -247,  -247,  -247,  -247,  -108,  -246,
    -247,  -247,   194,  -247,  -247,   163,  -247,  -247,  -247,    50,
    -247,  -247,  -247,  -247,    71,  -247,  -247,  -247,   140,  -247,
    -247,   303,   -87,  -247,  -247,  -247,  -247,   127,  -247,  -247,
    -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,  -247,
    -247,  -247,  -247,  -247,   186,  -247,  -247,  -247,  -247,  -247,
    -247,   159,  -247,  -247,   107,  -247,  -247,   296,    60,  -193,
    -247,  -247,  -247,  -247,    11,  -247,  -247,   -53,  -247,  -247,
    -247,  -106,  -247,  -122,  -247
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,    58,    59,    60,    61,    62,   137,    93,    94,   311,
     371,   372,   373,   374,   375,   376,   377,    63,    64,    65,
      66,    88,   244,   245,    67,   209,   210,   211,   212,    68,
     180,   128,   256,   333,   334,   335,   393,    69,   286,   344,
     413,   114,   115,   116,   148,   149,   150,    70,   267,   268,
     269,   270,    71,   248,   249,   250,    72,   175,   176,   177,
      73,   107,   108,   109,   110,    74,   193,   194,   195,    75,
      76,    77,   274,    78,    79,   118,   155,   282,   283,   179,
     418,   306,   353,   135,   136,    80,    81,   317,   236,    82,
     164,   165,   221,   217,   218,   219,   154,   138,   302,   229,
     213,    83,    84,   320,   321,   322,   380,   381,   437,   382,
     440,   441,   454,   455,   456
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      92,   214,   297,   386,   215,   181,     1,   422,   129,   312,
     378,   223,   272,   172,   351,     2,    89,   378,    90,   305,
     252,     3,     4,     5,   299,   246,    85,   233,   387,   327,
       6,     7,   252,   170,   157,   158,     8,   328,     9,    10,
     111,   329,    11,   347,    12,   349,    13,    14,   300,    86,
     234,    15,   273,   280,   225,   159,   226,   355,   238,   247,
      16,   289,   291,   318,   319,    17,    87,   356,   173,   227,
     427,   166,    18,   285,    19,   235,   340,   313,   439,   314,
     117,   330,   271,   239,    20,    21,   240,    22,    23,   444,
     352,   281,    24,    25,   290,   130,    26,    27,   160,   414,
     415,   228,   318,   319,   131,    28,   132,   357,   358,   133,
     139,   134,   388,   156,   331,   167,    91,   168,   152,    29,
     389,    30,    31,   171,   153,   178,   359,    32,   161,   423,
     276,   174,   120,   182,    91,   112,   121,    33,   295,   390,
     220,   113,    34,   276,    35,   222,    36,   360,    37,    38,
     224,   230,   231,   216,   254,   255,   361,   232,   362,    39,
      40,    41,    42,    43,    44,    45,   301,   237,    46,   251,
     433,    47,   278,    48,   363,   285,   241,   242,   315,   345,
     346,   279,    49,   243,   287,   332,   288,   294,    50,    51,
      52,   430,    53,    54,   293,   364,   365,   391,   122,    55,
      56,   392,   162,   316,   298,   169,     2,   304,   163,    -6,
      57,   307,     3,     4,     5,    91,   449,   450,   451,   452,
     453,     6,     7,   309,   310,   323,   457,     8,   324,     9,
      10,   326,   336,    11,   366,    12,   367,    13,    14,   348,
     416,   337,    15,   338,   368,   442,   342,   123,   369,   370,
     447,    16,   343,   350,   354,   383,    17,   417,   124,   384,
     420,   125,   385,    18,   394,    19,   421,   462,   140,   141,
     142,   143,   424,   426,   431,    20,    21,   183,    22,    23,
     425,   429,   432,    24,    25,   126,   434,    26,    27,   436,
     438,   127,   443,   439,   446,   459,    28,   448,   460,   144,
     419,   145,    95,   146,   461,   303,   184,    96,   253,   147,
      29,   277,    30,    31,    97,   292,   119,   339,    32,   325,
     296,   275,   257,   284,   308,   185,   151,   428,    33,   186,
     341,   379,   458,    34,   445,    35,     0,    36,     0,    37,
      38,   449,   450,   451,   452,   453,     0,     0,     0,     0,
      39,    40,    41,    42,    43,    44,    45,     0,     0,    46,
       0,     0,    47,     0,    48,     0,     0,   258,   259,   260,
     261,    98,     0,    49,     0,     0,     0,     0,     0,    50,
      51,    52,     0,    53,    54,     0,     0,     2,     0,     0,
      55,    56,     0,     3,     4,     5,     0,     0,     0,     0,
      -6,    57,     6,     7,     0,    99,   100,   187,     8,     0,
       9,    10,     0,     0,    11,     0,    12,   435,    13,    14,
       0,     0,     0,    15,   101,     0,   262,     0,     0,   102,
       0,     0,    16,     0,     0,     0,     0,    17,     0,     0,
     188,   189,   190,   191,    18,     0,    19,     0,   192,     0,
       0,     0,     0,     0,     0,     0,    20,    21,     0,    22,
      23,   103,     0,     0,    24,    25,     0,     0,    26,    27,
       0,     0,   263,     0,     0,     0,     0,    28,     0,     0,
       0,     0,     0,     0,     0,   104,   105,   106,     0,     0,
       0,    29,   264,    30,    31,     0,     0,   265,   266,    32,
       0,     0,     0,     0,     0,     0,     0,     0,   395,    33,
       0,     0,     0,     0,    34,     0,    35,   396,    36,     0,
      37,    38,     0,     0,     0,     0,   397,     0,     0,     0,
       0,    39,    40,    41,    42,    43,    44,    45,     0,     0,
      46,     0,     0,    47,     0,    48,   398,     0,     0,   399,
       0,     0,     0,     0,    49,   400,     0,     0,     0,     0,
      50,    51,    52,     0,    53,    54,   196,     0,   197,   198,
       0,    55,    56,     0,     0,   199,     0,     0,   200,     0,
     401,     0,    57,     0,   402,   403,     0,     0,   404,   405,
     406,     0,   407,   408,   409,     0,   410,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   201,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   411,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   202,
       0,   203,     0,     0,     0,     0,     0,   204,     0,   205,
       0,   412,     0,   206,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   207,   208
};

static const yytype_int16 yycheck[] =
{
       8,    52,   195,     4,    68,    47,     1,    68,    16,     5,
     160,    59,    22,    47,    69,    10,    74,   160,    76,   212,
     107,    16,    17,    18,    44,   146,   204,    38,    29,    33,
      25,    26,   119,    41,     7,     8,    31,    41,    33,    34,
      11,    45,    37,   289,    39,   291,    41,    42,    68,    68,
      61,    46,    62,    30,    62,    28,    64,     9,    32,   180,
      55,   169,   170,   206,   207,    60,    68,    19,   102,    36,
     220,    30,    67,    72,    69,    86,   269,    73,   209,    75,
     174,    85,   130,    57,    79,    80,    60,    82,    83,   220,
     145,    68,    87,    88,    93,   174,    91,    92,    71,   345,
     346,    68,   206,   207,   174,   100,   174,    59,    60,   174,
      68,   174,   113,    68,   118,    74,   174,    76,   174,   114,
     121,   116,   117,    68,   174,   174,    78,   122,   101,   190,
     138,   165,    23,   174,   174,   106,    27,   132,   180,   140,
      68,   112,   137,   151,   139,     0,   141,    99,   143,   144,
     204,    68,   174,   217,    68,    36,   108,   174,   110,   154,
     155,   156,   157,   158,   159,   160,   186,   219,   163,   174,
     416,   166,    68,   168,   126,    72,   150,   151,   174,   287,
     288,   174,   177,   157,    30,   189,    30,   216,   183,   184,
     185,   384,   187,   188,    68,   147,   148,   198,    89,   194,
     195,   202,   175,   199,   174,   164,    10,    68,   181,   204,
     205,    68,    16,    17,    18,   174,   210,   211,   212,   213,
     214,    25,    26,    68,   204,    68,   220,    31,   174,    33,
      34,   174,   174,    37,   186,    39,   188,    41,    42,   290,
     348,   174,    46,    68,   196,   438,    68,   138,   200,   201,
     443,    55,    68,   174,    40,   204,    60,    30,   149,   216,
      68,   152,   174,    67,   215,    69,    68,   460,    63,    64,
      65,    66,   174,   204,    68,    79,    80,     6,    82,    83,
     216,   219,    56,    87,    88,   176,   218,    91,    92,   208,
     216,   182,   216,   209,   204,   204,   100,   219,   216,    94,
     351,    96,     8,    98,   204,   209,    35,    13,   114,   104,
     114,   148,   116,   117,    20,   175,    13,   267,   122,   248,
     193,   135,     3,   164,   217,    54,    30,   380,   132,    58,
     270,   320,   454,   137,   440,   139,    -1,   141,    -1,   143,
     144,   210,   211,   212,   213,   214,    -1,    -1,    -1,    -1,
     154,   155,   156,   157,   158,   159,   160,    -1,    -1,   163,
      -1,    -1,   166,    -1,   168,    -1,    -1,    48,    49,    50,
      51,    77,    -1,   177,    -1,    -1,    -1,    -1,    -1,   183,
     184,   185,    -1,   187,   188,    -1,    -1,    10,    -1,    -1,
     194,   195,    -1,    16,    17,    18,    -1,    -1,    -1,    -1,
     204,   205,    25,    26,    -1,   111,   112,   136,    31,    -1,
      33,    34,    -1,    -1,    37,    -1,    39,   425,    41,    42,
      -1,    -1,    -1,    46,   130,    -1,   107,    -1,    -1,   135,
      -1,    -1,    55,    -1,    -1,    -1,    -1,    60,    -1,    -1,
     169,   170,   171,   172,    67,    -1,    69,    -1,   177,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    79,    80,    -1,    82,
      83,   167,    -1,    -1,    87,    88,    -1,    -1,    91,    92,
      -1,    -1,   153,    -1,    -1,    -1,    -1,   100,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   191,   192,   193,    -1,    -1,
      -1,   114,   173,   116,   117,    -1,    -1,   178,   179,   122,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    43,   132,
      -1,    -1,    -1,    -1,   137,    -1,   139,    52,   141,    -1,
     143,   144,    -1,    -1,    -1,    -1,    61,    -1,    -1,    -1,
      -1,   154,   155,   156,   157,   158,   159,   160,    -1,    -1,
     163,    -1,    -1,   166,    -1,   168,    81,    -1,    -1,    84,
      -1,    -1,    -1,    -1,   177,    90,    -1,    -1,    -1,    -1,
     183,   184,   185,    -1,   187,   188,    12,    -1,    14,    15,
      -1,   194,   195,    -1,    -1,    21,    -1,    -1,    24,    -1,
     115,    -1,   205,    -1,   119,   120,    -1,    -1,   123,   124,
     125,    -1,   127,   128,   129,    -1,   131,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    53,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   161,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    95,
      -1,    97,    -1,    -1,    -1,    -1,    -1,   103,    -1,   105,
      -1,   196,    -1,   109,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   133,   134
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_int16 yystos[] =
{
       0,     1,    10,    16,    17,    18,    25,    26,    31,    33,
      34,    37,    39,    41,    42,    46,    55,    60,    67,    69,
      79,    80,    82,    83,    87,    88,    91,    92,   100,   114,
     116,   117,   122,   132,   137,   139,   141,   143,   144,   154,
     155,   156,   157,   158,   159,   160,   163,   166,   168,   177,
     183,   184,   185,   187,   188,   194,   195,   205,   222,   223,
     224,   225,   226,   238,   239,   240,   241,   245,   250,   258,
     268,   273,   277,   281,   286,   290,   291,   292,   294,   295,
     306,   307,   310,   322,   323,   204,    68,    68,   242,    74,
      76,   174,   227,   228,   229,     8,    13,    20,    77,   111,
     112,   130,   135,   167,   191,   192,   193,   282,   283,   284,
     285,    11,   106,   112,   262,   263,   264,   174,   296,   282,
      23,    27,    89,   138,   149,   152,   176,   182,   252,   227,
     174,   174,   174,   174,   174,   304,   305,   227,   318,    68,
      63,    64,    65,    66,    94,    96,    98,   104,   265,   266,
     267,   318,   174,   174,   317,   297,    68,     7,     8,    28,
      71,   101,   175,   181,   311,   312,    30,    74,    76,   164,
     227,    68,    47,   102,   165,   278,   279,   280,   174,   300,
     251,   252,   174,     6,    35,    54,    58,   136,   169,   170,
     171,   172,   177,   287,   288,   289,    12,    14,    15,    21,
      24,    53,    95,    97,   103,   105,   109,   133,   134,   246,
     247,   248,   249,   321,   228,    68,   217,   314,   315,   316,
      68,   313,     0,   224,   204,   227,   227,    36,    68,   320,
      68,   174,   174,    38,    61,    86,   309,   219,    32,    57,
      60,   150,   151,   157,   243,   244,   146,   180,   274,   275,
     276,   174,   283,   263,    68,    36,   253,     3,    48,    49,
      50,    51,   107,   153,   173,   178,   179,   269,   270,   271,
     272,   224,    22,    62,   293,   305,   227,   266,    68,   174,
      30,    68,   298,   299,   312,    72,   259,    30,    30,   259,
      93,   259,   279,    68,   216,   252,   288,   320,   174,    44,
      68,   186,   319,   247,    68,   320,   302,    68,   315,    68,
     204,   230,     5,    73,    75,   174,   199,   308,   206,   207,
     324,   325,   326,    68,   174,   275,   174,    33,    41,    45,
      85,   118,   189,   254,   255,   256,   174,   174,    68,   270,
     320,   319,    68,    68,   260,   259,   259,   260,   228,   260,
     174,    69,   145,   303,    40,     9,    19,    59,    60,    78,
      99,   108,   110,   126,   147,   148,   186,   188,   196,   200,
     201,   231,   232,   233,   234,   235,   236,   237,   160,   325,
     327,   328,   330,   204,   216,   174,     4,    29,   113,   121,
     140,   198,   202,   257,   215,    43,    52,    61,    81,    84,
      90,   115,   119,   120,   123,   124,   125,   127,   128,   129,
     131,   161,   196,   261,   260,   260,   259,    30,   301,   228,
      68,    68,    68,   190,   174,   216,   204,   220,   328,   219,
     320,    68,    56,   260,   218,   227,   208,   329,   216,   209,
     331,   332,   320,   216,   220,   332,   204,   320,   219,   210,
     211,   212,   213,   214,   333,   334,   335,   220,   334,   204,
     216,   204,   320
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int16 yyr1[] =
{
       0,   221,   222,   223,   223,   223,   224,   224,   224,   224,
     224,   224,   224,   224,   224,   224,   224,   224,   224,   224,
     224,   225,   226,   226,   226,   226,   226,   227,   227,   228,
     229,   229,   230,   230,   231,   231,   231,   232,   233,   233,
     233,   233,   233,   233,   233,   233,   233,   234,   234,   235,
     235,   235,   235,   235,   235,   236,   237,   238,   239,   239,
     240,   240,   240,   240,   241,   241,   241,   241,   241,   241,
     241,   241,   241,   242,   242,   243,   243,   244,   244,   244,
     244,   244,   245,   246,   246,   247,   247,   247,   247,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   249,   249,
     250,   250,   250,   251,   251,   252,   252,   252,   252,   252,
     252,   252,   252,   253,   253,   254,   254,   254,   254,   255,
     255,   256,   256,   257,   257,   257,   257,   257,   257,   257,
     258,   258,   258,   258,   258,   258,   258,   258,   259,   259,
     260,   260,   260,   261,   261,   261,   261,   261,   261,   261,
     261,   261,   261,   261,   261,   261,   261,   261,   261,   261,
     262,   262,   263,   264,   264,   264,   265,   265,   266,   267,
     267,   267,   267,   267,   267,   267,   267,   268,   269,   269,
     270,   270,   270,   270,   270,   271,   271,   271,   272,   272,
     272,   272,   273,   274,   274,   275,   276,   276,   277,   278,
     278,   279,   280,   280,   280,   281,   281,   282,   282,   283,
     283,   284,   284,   284,   284,   284,   284,   285,   285,   285,
     285,   285,   285,   286,   287,   287,   288,   289,   289,   289,
     289,   289,   289,   289,   289,   289,   289,   290,   290,   290,
     290,   290,   290,   290,   290,   290,   290,   290,   290,   290,
     290,   290,   290,   291,   291,   291,   292,   292,   293,   293,
     293,   294,   295,   295,   295,   296,   296,   296,   297,   297,
     298,   299,   299,   300,   301,   301,   302,   302,   303,   303,
     304,   304,   305,   306,   306,   307,   307,   308,   308,   308,
     308,   309,   309,   309,   310,   311,   311,   312,   312,   312,
     312,   312,   312,   312,   313,   313,   314,   314,   315,   315,
     316,   317,   317,   318,   318,   319,   319,   319,   320,   320,
     321,   322,   323,   324,   324,   325,   326,   326,   327,   327,
     328,   329,   330,   331,   331,   332,   333,   333,   334,   335,
     335,   335,   335,   335
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     3,     2,     2,     0,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     3,     1,     1,     1,     1,     1,     1,     2,     1,
       1,     1,     0,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     2,     1,
       1,     1,     1,     1,     1,     2,     1,     2,     1,     1,
       1,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     0,     2,     2,     2,     1,     1,     1,
       1,     1,     2,     2,     1,     2,     2,     2,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     2,     3,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     0,     2,     2,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     2,     4,     6,     4,     5,     5,     4,     0,     2,
       0,     2,     3,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     1,     2,     1,     1,     1,     2,     1,     2,     1,
       1,     1,     1,     1,     1,     1,     1,     3,     2,     1,
       2,     2,     2,     2,     2,     1,     1,     1,     1,     1,
       1,     1,     3,     2,     1,     2,     1,     1,     2,     2,
       1,     2,     1,     1,     1,     2,     2,     2,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     2,     1,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     2,     2,     3,     3,     1,     2,     2,     2,     2,
       2,     3,     2,     1,     1,     1,     1,     1,     1,     1,
       0,     1,     1,     1,     1,     1,     2,     0,     0,     2,
       4,     1,     1,     4,     1,     0,     0,     2,     2,     2,
       2,     1,     1,     3,     3,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     1,     2,     1,     1,     1,
       5,     2,     1,     2,     1,     1,     1,     1,     1,     1,
       2,     5,     1,     3,     2,     3,     1,     1,     2,     1,
       5,     4,     3,     2,     1,     6,     3,     2,     3,     1,
       1,     1,     1,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
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

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


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
# ifndef YY_LOCATION_PRINT
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yykind < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yykind], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
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
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
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
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
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






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
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
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

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

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
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
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
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
| yyreduce -- do a reduction.  |
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
  case 5: /* command_list: error T_EOC  */
#line 404 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
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
#line 2438 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 21: /* server_command: client_type address option_list  */
#line 441 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			peer_node *my_node;

			my_node = create_peer_node((yyvsp[-2].Integer), (yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.peers, my_node);
		}
#line 2449 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 28: /* address: address_fam T_String  */
#line 460 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Address_node) = create_address_node((yyvsp[0].String), (yyvsp[-1].Integer)); }
#line 2455 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 29: /* ip_address: T_String  */
#line 465 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Address_node) = create_address_node((yyvsp[0].String), AF_UNSPEC); }
#line 2461 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 30: /* address_fam: T_Ipv4_flag  */
#line 470 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Integer) = AF_INET; }
#line 2467 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 31: /* address_fam: T_Ipv6_flag  */
#line 472 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Integer) = AF_INET6; }
#line 2473 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 32: /* option_list: %empty  */
#line 477 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = NULL; }
#line 2479 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 33: /* option_list: option_list option  */
#line 479 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2488 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 37: /* option_flag: option_flag_keyword  */
#line 493 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2494 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 47: /* option_int: option_int_keyword T_Integer  */
#line 510 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2500 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 48: /* option_int: option_int_keyword T_U_int  */
#line 512 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_uval((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2506 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 55: /* option_str: option_str_keyword T_String  */
#line 526 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2512 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 57: /* unpeer_command: unpeer_keyword address  */
#line 540 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			unpeer_node *my_node;

			my_node = create_unpeer_node((yyvsp[0].Address_node));
			if (my_node)
				APPEND_G_FIFO(cfgt.unpeers, my_node);
		}
#line 2524 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 60: /* other_mode_command: T_Broadcastclient  */
#line 561 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { cfgt.broadcastclient = 1; }
#line 2530 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 61: /* other_mode_command: T_Manycastserver address_list  */
#line 563 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.manycastserver, (yyvsp[0].Address_fifo)); }
#line 2536 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 62: /* other_mode_command: T_Multicastclient address_list  */
#line 565 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.multicastclient, (yyvsp[0].Address_fifo)); }
#line 2542 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 63: /* other_mode_command: T_Mdnstries T_Integer  */
#line 567 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { cfgt.mdnstries = (yyvsp[0].Integer); }
#line 2548 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 64: /* authentication_command: T_Automax T_Integer  */
#line 578 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			attr_val *atrv;

			atrv = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			APPEND_G_FIFO(cfgt.vars, atrv);
		}
#line 2559 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 65: /* authentication_command: T_ControlKey T_Integer  */
#line 585 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { cfgt.auth.control_key = (yyvsp[0].Integer); }
#line 2565 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 66: /* authentication_command: T_Crypto crypto_command_list  */
#line 587 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			cfgt.auth.cryptosw++;
			CONCAT_G_FIFOS(cfgt.auth.crypto_cmd_list, (yyvsp[0].Attr_val_fifo));
		}
#line 2574 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 67: /* authentication_command: T_Keys T_String  */
#line 592 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { cfgt.auth.keys = (yyvsp[0].String); }
#line 2580 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 68: /* authentication_command: T_Keysdir T_String  */
#line 594 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { cfgt.auth.keysdir = (yyvsp[0].String); }
#line 2586 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 69: /* authentication_command: T_Requestkey T_Integer  */
#line 596 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { cfgt.auth.request_key = (yyvsp[0].Integer); }
#line 2592 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 70: /* authentication_command: T_Revoke T_Integer  */
#line 598 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { cfgt.auth.revoke = (yyvsp[0].Integer); }
#line 2598 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 71: /* authentication_command: T_Trustedkey integer_list_range  */
#line 600 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			/* [Bug 948] leaves it open if appending or
			 * replacing the trusted key list is the right
			 * way. In any case, either alternative should
			 * be coded correctly!
			 */
			DESTROY_G_FIFO(cfgt.auth.trusted_key_list, destroy_attr_val); /* remove for append */
			CONCAT_G_FIFOS(cfgt.auth.trusted_key_list, (yyvsp[0].Attr_val_fifo));
		}
#line 2612 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 72: /* authentication_command: T_NtpSignDsocket T_String  */
#line 610 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { cfgt.auth.ntp_signd_socket = (yyvsp[0].String); }
#line 2618 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 73: /* crypto_command_list: %empty  */
#line 615 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = NULL; }
#line 2624 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 74: /* crypto_command_list: crypto_command_list crypto_command  */
#line 617 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2633 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 75: /* crypto_command: crypto_str_keyword T_String  */
#line 625 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2639 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 76: /* crypto_command: T_Revoke T_Integer  */
#line 627 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val) = NULL;
			cfgt.auth.revoke = (yyvsp[0].Integer);
			msyslog(LOG_WARNING,
				"'crypto revoke %d' is deprecated, "
				"please use 'revoke %d' instead.",
				cfgt.auth.revoke, cfgt.auth.revoke);
		}
#line 2652 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 82: /* orphan_mode_command: T_Tos tos_option_list  */
#line 652 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.orphan_cmds, (yyvsp[0].Attr_val_fifo)); }
#line 2658 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 83: /* tos_option_list: tos_option_list tos_option  */
#line 657 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2667 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 84: /* tos_option_list: tos_option  */
#line 662 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2676 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 85: /* tos_option: tos_option_int_keyword T_Integer  */
#line 670 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2682 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 86: /* tos_option: tos_option_dbl_keyword number  */
#line 672 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2688 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 87: /* tos_option: T_Cohort boolean  */
#line 674 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2694 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 88: /* tos_option: basedate  */
#line 676 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival(T_Basedate, (yyvsp[0].Integer)); }
#line 2700 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 100: /* monitoring_command: T_Statistics stats_list  */
#line 703 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.stats_list, (yyvsp[0].Int_fifo)); }
#line 2706 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 101: /* monitoring_command: T_Statsdir T_String  */
#line 705 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			if (lex_from_file()) {
				cfgt.stats_dir = (yyvsp[0].String);
			} else {
				YYFREE((yyvsp[0].String));
				yyerror("statsdir remote configuration ignored");
			}
		}
#line 2719 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 102: /* monitoring_command: T_Filegen stat filegen_option_list  */
#line 714 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			filegen_node *fgn;

			fgn = create_filegen_node((yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.filegen_opts, fgn);
		}
#line 2730 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 103: /* stats_list: stats_list stat  */
#line 724 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2739 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 104: /* stats_list: stat  */
#line 729 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2748 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 113: /* filegen_option_list: %empty  */
#line 748 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = NULL; }
#line 2754 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 114: /* filegen_option_list: filegen_option_list filegen_option  */
#line 750 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2763 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 115: /* filegen_option: T_File T_String  */
#line 758 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			} else {
				(yyval.Attr_val) = NULL;
				YYFREE((yyvsp[0].String));
				yyerror("filegen file remote config ignored");
			}
		}
#line 2777 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 116: /* filegen_option: T_Type filegen_type  */
#line 768 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror("filegen type remote config ignored");
			}
		}
#line 2790 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 117: /* filegen_option: link_nolink  */
#line 777 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
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
#line 2809 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 118: /* filegen_option: enable_disable  */
#line 792 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2815 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 130: /* access_control_command: T_Discard discard_option_list  */
#line 822 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			CONCAT_G_FIFOS(cfgt.discard_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2823 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 131: /* access_control_command: T_Mru mru_option_list  */
#line 826 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			CONCAT_G_FIFOS(cfgt.mru_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2831 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 132: /* access_control_command: T_Restrict address res_ippeerlimit ac_flag_list  */
#line 830 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[-2].Address_node), NULL, (yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2843 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 133: /* access_control_command: T_Restrict address T_Mask ip_address res_ippeerlimit ac_flag_list  */
#line 838 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[-4].Address_node), (yyvsp[-2].Address_node), (yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2855 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 134: /* access_control_command: T_Restrict T_Default res_ippeerlimit ac_flag_list  */
#line 846 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			restrict_node *rn;

			rn = create_restrict_node(NULL, NULL, (yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo),
						  lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2867 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 135: /* access_control_command: T_Restrict T_Ipv4_flag T_Default res_ippeerlimit ac_flag_list  */
#line 854 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
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
#line 2886 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 136: /* access_control_command: T_Restrict T_Ipv6_flag T_Default res_ippeerlimit ac_flag_list  */
#line 869 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
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
#line 2905 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 137: /* access_control_command: T_Restrict T_Source res_ippeerlimit ac_flag_list  */
#line 884 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			restrict_node *	rn;

			APPEND_G_FIFO((yyvsp[0].Attr_val_fifo), create_attr_ival((yyvsp[-2].Integer), 1));
			rn = create_restrict_node(
				NULL, NULL, (yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo), lex_current()->curpos.nline);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2918 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 138: /* res_ippeerlimit: %empty  */
#line 896 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Integer) = -1; }
#line 2924 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 139: /* res_ippeerlimit: T_Ippeerlimit T_Integer  */
#line 898 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
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
#line 2944 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 140: /* ac_flag_list: %empty  */
#line 917 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = NULL; }
#line 2950 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 141: /* ac_flag_list: ac_flag_list access_control_flag  */
#line 919 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			attr_val *av;

			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			av = create_attr_ival((yyvsp[0].Integer), 1);
			APPEND_G_FIFO((yyval.Attr_val_fifo), av);
		}
#line 2962 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 142: /* ac_flag_list: ac_flag_list T_Serverresponse T_Fuzz  */
#line 927 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			attr_val *av;

			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			av = create_attr_ival(T_ServerresponseFuzz, 1);
			APPEND_G_FIFO((yyval.Attr_val_fifo), av);
		}
#line 2974 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 160: /* discard_option_list: discard_option_list discard_option  */
#line 958 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2983 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 161: /* discard_option_list: discard_option  */
#line 963 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2992 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 162: /* discard_option: discard_option_keyword T_Integer  */
#line 971 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2998 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 166: /* mru_option_list: mru_option_list mru_option  */
#line 982 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3007 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 167: /* mru_option_list: mru_option  */
#line 987 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3016 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 168: /* mru_option: mru_option_keyword T_Integer  */
#line 995 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 3022 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 177: /* fudge_command: T_Fudge address fudge_factor_list  */
#line 1015 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.fudge, aon);
		}
#line 3033 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 178: /* fudge_factor_list: fudge_factor_list fudge_factor  */
#line 1025 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3042 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 179: /* fudge_factor_list: fudge_factor  */
#line 1030 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3051 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 180: /* fudge_factor: fudge_factor_dbl_keyword number  */
#line 1038 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 3057 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 181: /* fudge_factor: fudge_factor_bool_keyword boolean  */
#line 1040 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 3063 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 182: /* fudge_factor: T_Stratum T_Integer  */
#line 1042 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			if ((yyvsp[0].Integer) >= 0 && (yyvsp[0].Integer) <= 16) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror("fudge factor: stratum value not in [0..16], ignored");
			}
		}
#line 3076 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 183: /* fudge_factor: T_Abbrev T_String  */
#line 1051 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 3082 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 184: /* fudge_factor: T_Refid T_String  */
#line 1053 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 3088 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 192: /* device_command: T_Device address device_item_list  */
#line 1075 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.device, aon);
		}
#line 3099 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 193: /* device_item_list: device_item_list device_item  */
#line 1085 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3108 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 194: /* device_item_list: device_item  */
#line 1090 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3117 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 195: /* device_item: device_item_path_keyword T_String  */
#line 1098 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 3123 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 198: /* rlimit_command: T_Rlimit rlimit_option_list  */
#line 1112 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.rlimit, (yyvsp[0].Attr_val_fifo)); }
#line 3129 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 199: /* rlimit_option_list: rlimit_option_list rlimit_option  */
#line 1117 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3138 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 200: /* rlimit_option_list: rlimit_option  */
#line 1122 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3147 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 201: /* rlimit_option: rlimit_option_keyword T_Integer  */
#line 1130 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 3153 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 205: /* system_option_command: T_Enable system_option_list  */
#line 1146 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.enable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 3159 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 206: /* system_option_command: T_Disable system_option_list  */
#line 1148 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.disable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 3165 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 207: /* system_option_list: system_option_list system_option  */
#line 1153 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3174 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 208: /* system_option_list: system_option  */
#line 1158 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3183 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 209: /* system_option: system_option_flag_keyword  */
#line 1166 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 3189 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 210: /* system_option: system_option_local_flag_keyword  */
#line 1168 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
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
#line 3207 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 223: /* tinker_command: T_Tinker tinker_option_list  */
#line 1207 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.tinker, (yyvsp[0].Attr_val_fifo)); }
#line 3213 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 224: /* tinker_option_list: tinker_option_list tinker_option  */
#line 1212 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3222 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 225: /* tinker_option_list: tinker_option  */
#line 1217 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3231 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 226: /* tinker_option: tinker_option_keyword number  */
#line 1225 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 3237 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 239: /* miscellaneous_command: misc_cmd_dbl_keyword number  */
#line 1250 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			attr_val *av;

			av = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 3248 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 240: /* miscellaneous_command: misc_cmd_int_keyword T_Integer  */
#line 1257 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			attr_val *av;

			av = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 3259 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 241: /* miscellaneous_command: misc_cmd_str_keyword T_String  */
#line 1264 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			attr_val *av;

			av = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 3270 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 242: /* miscellaneous_command: misc_cmd_str_lcl_keyword T_String  */
#line 1271 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
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
#line 3290 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 243: /* miscellaneous_command: T_Includefile T_String command  */
#line 1287 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
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
#line 3313 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 244: /* miscellaneous_command: T_Leapfile T_String opt_hash_check  */
#line 1306 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			attr_val *av;

			av = create_attr_sval((yyvsp[-2].Integer), (yyvsp[-1].String));
			av->flag = (yyvsp[0].Integer);
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 3325 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 245: /* miscellaneous_command: T_End  */
#line 1314 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { lex_flush_stack(); }
#line 3331 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 246: /* miscellaneous_command: T_Driftfile drift_parm  */
#line 1316 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { /* see drift_parm below for actions */ }
#line 3337 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 247: /* miscellaneous_command: T_Logconfig log_config_list  */
#line 1318 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.logconfig, (yyvsp[0].Attr_val_fifo)); }
#line 3343 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 248: /* miscellaneous_command: T_Phone string_list  */
#line 1320 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.phone, (yyvsp[0].String_fifo)); }
#line 3349 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 249: /* miscellaneous_command: T_PollSkewList pollskew_list  */
#line 1322 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.pollskewlist, (yyvsp[0].Attr_val_fifo)); }
#line 3355 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 250: /* miscellaneous_command: T_Setvar variable_assign  */
#line 1324 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { APPEND_G_FIFO(cfgt.setvar, (yyvsp[0].Set_var)); }
#line 3361 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 251: /* miscellaneous_command: T_Trap ip_address trap_option_list  */
#line 1326 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.trap, aon);
		}
#line 3372 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 252: /* miscellaneous_command: T_Ttl integer_list  */
#line 1333 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.ttl, (yyvsp[0].Attr_val_fifo)); }
#line 3378 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 257: /* misc_cmd_int_keyword: T_Leapsmearinterval  */
#line 1348 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
#ifndef LEAP_SMEAR
			yyerror("Built without LEAP_SMEAR support.");
#endif
		}
#line 3388 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 258: /* opt_hash_check: T_Ignorehash  */
#line 1357 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Integer) = FALSE; }
#line 3394 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 259: /* opt_hash_check: T_Checkhash  */
#line 1359 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Integer) = TRUE; }
#line 3400 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 260: /* opt_hash_check: %empty  */
#line 1361 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        {  (yyval.Integer) = TRUE; }
#line 3406 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 265: /* drift_parm: T_String  */
#line 1376 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
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
#line 3421 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 266: /* drift_parm: T_String T_Double  */
#line 1387 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
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
#line 3442 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 267: /* drift_parm: %empty  */
#line 1404 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			if (lex_from_file()) {
				attr_val *av;
				av = create_attr_sval(T_Driftfile, estrdup(""));
				APPEND_G_FIFO(cfgt.vars, av);
			} else {
				yyerror("driftfile remote configuration ignored");
			}
		}
#line 3456 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 268: /* pollskew_list: %empty  */
#line 1417 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = NULL; }
#line 3462 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 269: /* pollskew_list: pollskew_list pollskew_spec  */
#line 1419 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = append_gen_fifo((yyvsp[-1].Attr_val_fifo), (yyvsp[0].Attr_val)); }
#line 3468 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 270: /* pollskew_spec: pollskew_cycle T_Integer '|' T_Integer  */
#line 1424 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
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
#line 3495 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 271: /* pollskew_cycle: T_Integer  */
#line 1449 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                          { (yyval.Attr_val) = ((yyvsp[0].Integer) >= 3 && (yyvsp[0].Integer) <= 17) ? create_attr_rval((yyvsp[0].Integer), 0, 0) : NULL; }
#line 3501 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 272: /* pollskew_cycle: T_Default  */
#line 1450 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                          { (yyval.Attr_val) = create_attr_rval(-1, 0, 0); }
#line 3507 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 273: /* variable_assign: T_String '=' T_String t_default_or_zero  */
#line 1456 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Set_var) = create_setvar_node((yyvsp[-3].String), (yyvsp[-1].String), (yyvsp[0].Integer)); }
#line 3513 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 275: /* t_default_or_zero: %empty  */
#line 1462 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Integer) = 0; }
#line 3519 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 276: /* trap_option_list: %empty  */
#line 1467 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = NULL; }
#line 3525 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 277: /* trap_option_list: trap_option_list trap_option  */
#line 1469 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3534 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 278: /* trap_option: T_Port T_Integer  */
#line 1477 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 3540 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 279: /* trap_option: T_Interface ip_address  */
#line 1479 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), estrdup((yyvsp[0].Address_node)->address));
			destroy_address_node((yyvsp[0].Address_node));
		}
#line 3549 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 280: /* log_config_list: log_config_list log_config_command  */
#line 1487 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3558 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 281: /* log_config_list: log_config_command  */
#line 1492 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3567 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 282: /* log_config_command: T_String  */
#line 1500 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
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
#line 3593 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 283: /* interface_command: interface_nic nic_rule_action nic_rule_class  */
#line 1525 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node((yyvsp[0].Integer), NULL, (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3604 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 284: /* interface_command: interface_nic nic_rule_action T_String  */
#line 1532 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node(0, (yyvsp[0].String), (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3615 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 294: /* reset_command: T_Reset counter_set_list  */
#line 1560 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.reset_counters, (yyvsp[0].Int_fifo)); }
#line 3621 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 295: /* counter_set_list: counter_set_list counter_set_keyword  */
#line 1565 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3630 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 296: /* counter_set_list: counter_set_keyword  */
#line 1570 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3639 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 304: /* integer_list: integer_list T_Integer  */
#line 1594 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3648 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 305: /* integer_list: T_Integer  */
#line 1599 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3657 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 306: /* integer_list_range: integer_list_range integer_list_range_elt  */
#line 1607 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3666 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 307: /* integer_list_range: integer_list_range_elt  */
#line 1612 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3675 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 308: /* integer_list_range_elt: T_Integer  */
#line 1620 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival('i', (yyvsp[0].Integer)); }
#line 3681 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 310: /* integer_range: '(' T_Integer T_Ellipsis T_Integer ')'  */
#line 1626 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_rval('-', (yyvsp[-3].Integer), (yyvsp[-1].Integer)); }
#line 3687 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 311: /* string_list: string_list T_String  */
#line 1631 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.String_fifo) = (yyvsp[-1].String_fifo);
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3696 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 312: /* string_list: T_String  */
#line 1636 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.String_fifo) = NULL;
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3705 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 313: /* address_list: address_list address  */
#line 1644 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Address_fifo) = (yyvsp[-1].Address_fifo);
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3714 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 314: /* address_list: address  */
#line 1649 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Address_fifo) = NULL;
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3723 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 315: /* boolean: T_Integer  */
#line 1657 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			if ((yyvsp[0].Integer) != 0 && (yyvsp[0].Integer) != 1) {
				yyerror("Integer value is not boolean (0 or 1). Assuming 1");
				(yyval.Integer) = 1;
			} else {
				(yyval.Integer) = (yyvsp[0].Integer);
			}
		}
#line 3736 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 316: /* boolean: T_True  */
#line 1665 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Integer) = 1; }
#line 3742 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 317: /* boolean: T_False  */
#line 1666 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Integer) = 0; }
#line 3748 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 318: /* number: T_Integer  */
#line 1670 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                                { (yyval.Double) = (double)(yyvsp[0].Integer); }
#line 3754 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 320: /* basedate: T_Basedate T_String  */
#line 1676 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Integer) = basedate_eval_string((yyvsp[0].String)); YYFREE((yyvsp[0].String)); }
#line 3760 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 321: /* simulate_command: sim_conf_start '{' sim_init_statement_list sim_server_list '}'  */
#line 1684 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			sim_node *sn;

			sn =  create_sim_node((yyvsp[-2].Attr_val_fifo), (yyvsp[-1].Sim_server_fifo));
			APPEND_G_FIFO(cfgt.sim_details, sn);

			/* Revert from ; to \n for end-of-command */
			old_config_style = 1;
		}
#line 3774 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 322: /* sim_conf_start: T_Simulate  */
#line 1701 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                           { old_config_style = 0; }
#line 3780 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 323: /* sim_init_statement_list: sim_init_statement_list sim_init_statement T_EOC  */
#line 1706 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3789 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 324: /* sim_init_statement_list: sim_init_statement T_EOC  */
#line 1711 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3798 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 325: /* sim_init_statement: sim_init_keyword '=' number  */
#line 1719 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3804 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 328: /* sim_server_list: sim_server_list sim_server  */
#line 1729 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Sim_server_fifo) = (yyvsp[-1].Sim_server_fifo);
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3813 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 329: /* sim_server_list: sim_server  */
#line 1734 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Sim_server_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3822 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 330: /* sim_server: sim_server_name '{' sim_server_offset sim_act_list '}'  */
#line 1742 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Sim_server) = ONLY_SIM(create_sim_server((yyvsp[-4].Address_node), (yyvsp[-2].Double), (yyvsp[-1].Sim_script_fifo))); }
#line 3828 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 331: /* sim_server_offset: T_Server_Offset '=' number T_EOC  */
#line 1747 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Double) = (yyvsp[-1].Double); }
#line 3834 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 332: /* sim_server_name: T_Server '=' address  */
#line 1752 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Address_node) = (yyvsp[0].Address_node); }
#line 3840 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 333: /* sim_act_list: sim_act_list sim_act  */
#line 1757 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Sim_script_fifo) = (yyvsp[-1].Sim_script_fifo);
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3849 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 334: /* sim_act_list: sim_act  */
#line 1762 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Sim_script_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3858 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 335: /* sim_act: T_Duration '=' number '{' sim_act_stmt_list '}'  */
#line 1770 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Sim_script) = ONLY_SIM(create_sim_script_info((yyvsp[-3].Double), (yyvsp[-1].Attr_val_fifo))); }
#line 3864 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 336: /* sim_act_stmt_list: sim_act_stmt_list sim_act_stmt T_EOC  */
#line 1775 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3873 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 337: /* sim_act_stmt_list: sim_act_stmt T_EOC  */
#line 1780 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3882 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;

  case 338: /* sim_act_stmt: sim_act_keyword '=' number  */
#line 1788 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3888 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"
    break;


#line 3892 "../../../src/ntp-stable-3758/ntpd/ntp_parser.c"

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
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
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
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

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

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
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
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

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


#if !defined yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;
#endif


/*-------------------------------------------------------.
| yyreturn -- parsing is finished, clean up and return.  |
`-------------------------------------------------------*/
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
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 1799 "../../../src/ntp-stable-3758/ntpd/ntp_parser.y"


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
				  sizeof remote_config.err_msg - remote_config.err_pos,
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

