/* A Bison parser, made by GNU Bison 3.8.2.  */

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
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
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

#line 106 "ntp_parser.c"

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
#ifndef YY_YY_NTP_PARSER_H_INCLUDED
# define YY_YY_NTP_PARSER_H_INCLUDED
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
    T_Delrestrict = 286,           /* T_Delrestrict  */
    T_Device = 287,                /* T_Device  */
    T_Digest = 288,                /* T_Digest  */
    T_Disable = 289,               /* T_Disable  */
    T_Discard = 290,               /* T_Discard  */
    T_Dispersion = 291,            /* T_Dispersion  */
    T_Double = 292,                /* T_Double  */
    T_Driftfile = 293,             /* T_Driftfile  */
    T_Drop = 294,                  /* T_Drop  */
    T_Dscp = 295,                  /* T_Dscp  */
    T_Ellipsis = 296,              /* T_Ellipsis  */
    T_Enable = 297,                /* T_Enable  */
    T_End = 298,                   /* T_End  */
    T_Epeer = 299,                 /* T_Epeer  */
    T_False = 300,                 /* T_False  */
    T_File = 301,                  /* T_File  */
    T_Filegen = 302,               /* T_Filegen  */
    T_Filenum = 303,               /* T_Filenum  */
    T_Flag1 = 304,                 /* T_Flag1  */
    T_Flag2 = 305,                 /* T_Flag2  */
    T_Flag3 = 306,                 /* T_Flag3  */
    T_Flag4 = 307,                 /* T_Flag4  */
    T_Flake = 308,                 /* T_Flake  */
    T_Floor = 309,                 /* T_Floor  */
    T_Freq = 310,                  /* T_Freq  */
    T_Fudge = 311,                 /* T_Fudge  */
    T_Fuzz = 312,                  /* T_Fuzz  */
    T_Host = 313,                  /* T_Host  */
    T_Huffpuff = 314,              /* T_Huffpuff  */
    T_Iburst = 315,                /* T_Iburst  */
    T_Ident = 316,                 /* T_Ident  */
    T_Ignore = 317,                /* T_Ignore  */
    T_Ignorehash = 318,            /* T_Ignorehash  */
    T_Incalloc = 319,              /* T_Incalloc  */
    T_Incmem = 320,                /* T_Incmem  */
    T_Initalloc = 321,             /* T_Initalloc  */
    T_Initmem = 322,               /* T_Initmem  */
    T_Includefile = 323,           /* T_Includefile  */
    T_Integer = 324,               /* T_Integer  */
    T_Interface = 325,             /* T_Interface  */
    T_Intrange = 326,              /* T_Intrange  */
    T_Io = 327,                    /* T_Io  */
    T_Ippeerlimit = 328,           /* T_Ippeerlimit  */
    T_Ipv4 = 329,                  /* T_Ipv4  */
    T_Ipv4_flag = 330,             /* T_Ipv4_flag  */
    T_Ipv6 = 331,                  /* T_Ipv6  */
    T_Ipv6_flag = 332,             /* T_Ipv6_flag  */
    T_Kernel = 333,                /* T_Kernel  */
    T_Key = 334,                   /* T_Key  */
    T_Keys = 335,                  /* T_Keys  */
    T_Keysdir = 336,               /* T_Keysdir  */
    T_Kod = 337,                   /* T_Kod  */
    T_Leapfile = 338,              /* T_Leapfile  */
    T_Leapsmearinterval = 339,     /* T_Leapsmearinterval  */
    T_Limited = 340,               /* T_Limited  */
    T_Link = 341,                  /* T_Link  */
    T_Listen = 342,                /* T_Listen  */
    T_Logconfig = 343,             /* T_Logconfig  */
    T_Logfile = 344,               /* T_Logfile  */
    T_Loopstats = 345,             /* T_Loopstats  */
    T_Lowpriotrap = 346,           /* T_Lowpriotrap  */
    T_Manycastclient = 347,        /* T_Manycastclient  */
    T_Manycastserver = 348,        /* T_Manycastserver  */
    T_Mask = 349,                  /* T_Mask  */
    T_Maxage = 350,                /* T_Maxage  */
    T_Maxclock = 351,              /* T_Maxclock  */
    T_Maxdepth = 352,              /* T_Maxdepth  */
    T_Maxdist = 353,               /* T_Maxdist  */
    T_Maxmem = 354,                /* T_Maxmem  */
    T_Maxpoll = 355,               /* T_Maxpoll  */
    T_Mdnstries = 356,             /* T_Mdnstries  */
    T_Mem = 357,                   /* T_Mem  */
    T_Memlock = 358,               /* T_Memlock  */
    T_Minclock = 359,              /* T_Minclock  */
    T_Mindepth = 360,              /* T_Mindepth  */
    T_Mindist = 361,               /* T_Mindist  */
    T_Minimum = 362,               /* T_Minimum  */
    T_Minjitter = 363,             /* T_Minjitter  */
    T_Minpoll = 364,               /* T_Minpoll  */
    T_Minsane = 365,               /* T_Minsane  */
    T_Mode = 366,                  /* T_Mode  */
    T_Mode7 = 367,                 /* T_Mode7  */
    T_Monitor = 368,               /* T_Monitor  */
    T_Month = 369,                 /* T_Month  */
    T_Mru = 370,                   /* T_Mru  */
    T_Mssntp = 371,                /* T_Mssntp  */
    T_Multicastclient = 372,       /* T_Multicastclient  */
    T_Nic = 373,                   /* T_Nic  */
    T_Nolink = 374,                /* T_Nolink  */
    T_Nomodify = 375,              /* T_Nomodify  */
    T_Nomrulist = 376,             /* T_Nomrulist  */
    T_None = 377,                  /* T_None  */
    T_Nonvolatile = 378,           /* T_Nonvolatile  */
    T_Noepeer = 379,               /* T_Noepeer  */
    T_Nopeer = 380,                /* T_Nopeer  */
    T_Noquery = 381,               /* T_Noquery  */
    T_Noselect = 382,              /* T_Noselect  */
    T_Noserve = 383,               /* T_Noserve  */
    T_Notrap = 384,                /* T_Notrap  */
    T_Notrust = 385,               /* T_Notrust  */
    T_Ntp = 386,                   /* T_Ntp  */
    T_Ntpport = 387,               /* T_Ntpport  */
    T_NtpSignDsocket = 388,        /* T_NtpSignDsocket  */
    T_Orphan = 389,                /* T_Orphan  */
    T_Orphanwait = 390,            /* T_Orphanwait  */
    T_PCEdigest = 391,             /* T_PCEdigest  */
    T_Panic = 392,                 /* T_Panic  */
    T_Peer = 393,                  /* T_Peer  */
    T_Peerstats = 394,             /* T_Peerstats  */
    T_Phone = 395,                 /* T_Phone  */
    T_Pid = 396,                   /* T_Pid  */
    T_Pidfile = 397,               /* T_Pidfile  */
    T_Poll = 398,                  /* T_Poll  */
    T_PollSkewList = 399,          /* T_PollSkewList  */
    T_Pool = 400,                  /* T_Pool  */
    T_Port = 401,                  /* T_Port  */
    T_PpsData = 402,               /* T_PpsData  */
    T_Preempt = 403,               /* T_Preempt  */
    T_Prefer = 404,                /* T_Prefer  */
    T_Protostats = 405,            /* T_Protostats  */
    T_Pw = 406,                    /* T_Pw  */
    T_Randfile = 407,              /* T_Randfile  */
    T_Rawstats = 408,              /* T_Rawstats  */
    T_Refid = 409,                 /* T_Refid  */
    T_Requestkey = 410,            /* T_Requestkey  */
    T_Reset = 411,                 /* T_Reset  */
    T_Restrict = 412,              /* T_Restrict  */
    T_Revoke = 413,                /* T_Revoke  */
    T_Rlimit = 414,                /* T_Rlimit  */
    T_Saveconfigdir = 415,         /* T_Saveconfigdir  */
    T_Server = 416,                /* T_Server  */
    T_Serverresponse = 417,        /* T_Serverresponse  */
    T_ServerresponseFuzz = 418,    /* T_ServerresponseFuzz  */
    T_Setvar = 419,                /* T_Setvar  */
    T_Source = 420,                /* T_Source  */
    T_Stacksize = 421,             /* T_Stacksize  */
    T_Statistics = 422,            /* T_Statistics  */
    T_Stats = 423,                 /* T_Stats  */
    T_Statsdir = 424,              /* T_Statsdir  */
    T_Step = 425,                  /* T_Step  */
    T_Stepback = 426,              /* T_Stepback  */
    T_Stepfwd = 427,               /* T_Stepfwd  */
    T_Stepout = 428,               /* T_Stepout  */
    T_Stratum = 429,               /* T_Stratum  */
    T_String = 430,                /* T_String  */
    T_Sys = 431,                   /* T_Sys  */
    T_Sysstats = 432,              /* T_Sysstats  */
    T_Tick = 433,                  /* T_Tick  */
    T_Time1 = 434,                 /* T_Time1  */
    T_Time2 = 435,                 /* T_Time2  */
    T_TimeData = 436,              /* T_TimeData  */
    T_Timer = 437,                 /* T_Timer  */
    T_Timingstats = 438,           /* T_Timingstats  */
    T_Tinker = 439,                /* T_Tinker  */
    T_Tos = 440,                   /* T_Tos  */
    T_Trap = 441,                  /* T_Trap  */
    T_True = 442,                  /* T_True  */
    T_Trustedkey = 443,            /* T_Trustedkey  */
    T_Ttl = 444,                   /* T_Ttl  */
    T_Type = 445,                  /* T_Type  */
    T_U_int = 446,                 /* T_U_int  */
    T_UEcrypto = 447,              /* T_UEcrypto  */
    T_UEcryptonak = 448,           /* T_UEcryptonak  */
    T_UEdigest = 449,              /* T_UEdigest  */
    T_Unconfig = 450,              /* T_Unconfig  */
    T_Unpeer = 451,                /* T_Unpeer  */
    T_Version = 452,               /* T_Version  */
    T_WanderThreshold = 453,       /* T_WanderThreshold  */
    T_Week = 454,                  /* T_Week  */
    T_Wildcard = 455,              /* T_Wildcard  */
    T_Xleave = 456,                /* T_Xleave  */
    T_Xmtnonce = 457,              /* T_Xmtnonce  */
    T_Year = 458,                  /* T_Year  */
    T_Flag = 459,                  /* T_Flag  */
    T_EOC = 460,                   /* T_EOC  */
    T_Simulate = 461,              /* T_Simulate  */
    T_Beep_Delay = 462,            /* T_Beep_Delay  */
    T_Sim_Duration = 463,          /* T_Sim_Duration  */
    T_Server_Offset = 464,         /* T_Server_Offset  */
    T_Duration = 465,              /* T_Duration  */
    T_Freq_Offset = 466,           /* T_Freq_Offset  */
    T_Wander = 467,                /* T_Wander  */
    T_Jitter = 468,                /* T_Jitter  */
    T_Prop_Delay = 469,            /* T_Prop_Delay  */
    T_Proc_Delay = 470             /* T_Proc_Delay  */
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
#define T_Delrestrict 286
#define T_Device 287
#define T_Digest 288
#define T_Disable 289
#define T_Discard 290
#define T_Dispersion 291
#define T_Double 292
#define T_Driftfile 293
#define T_Drop 294
#define T_Dscp 295
#define T_Ellipsis 296
#define T_Enable 297
#define T_End 298
#define T_Epeer 299
#define T_False 300
#define T_File 301
#define T_Filegen 302
#define T_Filenum 303
#define T_Flag1 304
#define T_Flag2 305
#define T_Flag3 306
#define T_Flag4 307
#define T_Flake 308
#define T_Floor 309
#define T_Freq 310
#define T_Fudge 311
#define T_Fuzz 312
#define T_Host 313
#define T_Huffpuff 314
#define T_Iburst 315
#define T_Ident 316
#define T_Ignore 317
#define T_Ignorehash 318
#define T_Incalloc 319
#define T_Incmem 320
#define T_Initalloc 321
#define T_Initmem 322
#define T_Includefile 323
#define T_Integer 324
#define T_Interface 325
#define T_Intrange 326
#define T_Io 327
#define T_Ippeerlimit 328
#define T_Ipv4 329
#define T_Ipv4_flag 330
#define T_Ipv6 331
#define T_Ipv6_flag 332
#define T_Kernel 333
#define T_Key 334
#define T_Keys 335
#define T_Keysdir 336
#define T_Kod 337
#define T_Leapfile 338
#define T_Leapsmearinterval 339
#define T_Limited 340
#define T_Link 341
#define T_Listen 342
#define T_Logconfig 343
#define T_Logfile 344
#define T_Loopstats 345
#define T_Lowpriotrap 346
#define T_Manycastclient 347
#define T_Manycastserver 348
#define T_Mask 349
#define T_Maxage 350
#define T_Maxclock 351
#define T_Maxdepth 352
#define T_Maxdist 353
#define T_Maxmem 354
#define T_Maxpoll 355
#define T_Mdnstries 356
#define T_Mem 357
#define T_Memlock 358
#define T_Minclock 359
#define T_Mindepth 360
#define T_Mindist 361
#define T_Minimum 362
#define T_Minjitter 363
#define T_Minpoll 364
#define T_Minsane 365
#define T_Mode 366
#define T_Mode7 367
#define T_Monitor 368
#define T_Month 369
#define T_Mru 370
#define T_Mssntp 371
#define T_Multicastclient 372
#define T_Nic 373
#define T_Nolink 374
#define T_Nomodify 375
#define T_Nomrulist 376
#define T_None 377
#define T_Nonvolatile 378
#define T_Noepeer 379
#define T_Nopeer 380
#define T_Noquery 381
#define T_Noselect 382
#define T_Noserve 383
#define T_Notrap 384
#define T_Notrust 385
#define T_Ntp 386
#define T_Ntpport 387
#define T_NtpSignDsocket 388
#define T_Orphan 389
#define T_Orphanwait 390
#define T_PCEdigest 391
#define T_Panic 392
#define T_Peer 393
#define T_Peerstats 394
#define T_Phone 395
#define T_Pid 396
#define T_Pidfile 397
#define T_Poll 398
#define T_PollSkewList 399
#define T_Pool 400
#define T_Port 401
#define T_PpsData 402
#define T_Preempt 403
#define T_Prefer 404
#define T_Protostats 405
#define T_Pw 406
#define T_Randfile 407
#define T_Rawstats 408
#define T_Refid 409
#define T_Requestkey 410
#define T_Reset 411
#define T_Restrict 412
#define T_Revoke 413
#define T_Rlimit 414
#define T_Saveconfigdir 415
#define T_Server 416
#define T_Serverresponse 417
#define T_ServerresponseFuzz 418
#define T_Setvar 419
#define T_Source 420
#define T_Stacksize 421
#define T_Statistics 422
#define T_Stats 423
#define T_Statsdir 424
#define T_Step 425
#define T_Stepback 426
#define T_Stepfwd 427
#define T_Stepout 428
#define T_Stratum 429
#define T_String 430
#define T_Sys 431
#define T_Sysstats 432
#define T_Tick 433
#define T_Time1 434
#define T_Time2 435
#define T_TimeData 436
#define T_Timer 437
#define T_Timingstats 438
#define T_Tinker 439
#define T_Tos 440
#define T_Trap 441
#define T_True 442
#define T_Trustedkey 443
#define T_Ttl 444
#define T_Type 445
#define T_U_int 446
#define T_UEcrypto 447
#define T_UEcryptonak 448
#define T_UEdigest 449
#define T_Unconfig 450
#define T_Unpeer 451
#define T_Version 452
#define T_WanderThreshold 453
#define T_Week 454
#define T_Wildcard 455
#define T_Xleave 456
#define T_Xmtnonce 457
#define T_Year 458
#define T_Flag 459
#define T_EOC 460
#define T_Simulate 461
#define T_Beep_Delay 462
#define T_Sim_Duration 463
#define T_Server_Offset 464
#define T_Duration 465
#define T_Freq_Offset 466
#define T_Wander 467
#define T_Jitter 468
#define T_Prop_Delay 469
#define T_Proc_Delay 470

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 52 "../../ntpd/ntp_parser.y"

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

#line 608 "ntp_parser.c"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_NTP_PARSER_H_INCLUDED  */
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
  YYSYMBOL_T_Delrestrict = 31,             /* T_Delrestrict  */
  YYSYMBOL_T_Device = 32,                  /* T_Device  */
  YYSYMBOL_T_Digest = 33,                  /* T_Digest  */
  YYSYMBOL_T_Disable = 34,                 /* T_Disable  */
  YYSYMBOL_T_Discard = 35,                 /* T_Discard  */
  YYSYMBOL_T_Dispersion = 36,              /* T_Dispersion  */
  YYSYMBOL_T_Double = 37,                  /* T_Double  */
  YYSYMBOL_T_Driftfile = 38,               /* T_Driftfile  */
  YYSYMBOL_T_Drop = 39,                    /* T_Drop  */
  YYSYMBOL_T_Dscp = 40,                    /* T_Dscp  */
  YYSYMBOL_T_Ellipsis = 41,                /* T_Ellipsis  */
  YYSYMBOL_T_Enable = 42,                  /* T_Enable  */
  YYSYMBOL_T_End = 43,                     /* T_End  */
  YYSYMBOL_T_Epeer = 44,                   /* T_Epeer  */
  YYSYMBOL_T_False = 45,                   /* T_False  */
  YYSYMBOL_T_File = 46,                    /* T_File  */
  YYSYMBOL_T_Filegen = 47,                 /* T_Filegen  */
  YYSYMBOL_T_Filenum = 48,                 /* T_Filenum  */
  YYSYMBOL_T_Flag1 = 49,                   /* T_Flag1  */
  YYSYMBOL_T_Flag2 = 50,                   /* T_Flag2  */
  YYSYMBOL_T_Flag3 = 51,                   /* T_Flag3  */
  YYSYMBOL_T_Flag4 = 52,                   /* T_Flag4  */
  YYSYMBOL_T_Flake = 53,                   /* T_Flake  */
  YYSYMBOL_T_Floor = 54,                   /* T_Floor  */
  YYSYMBOL_T_Freq = 55,                    /* T_Freq  */
  YYSYMBOL_T_Fudge = 56,                   /* T_Fudge  */
  YYSYMBOL_T_Fuzz = 57,                    /* T_Fuzz  */
  YYSYMBOL_T_Host = 58,                    /* T_Host  */
  YYSYMBOL_T_Huffpuff = 59,                /* T_Huffpuff  */
  YYSYMBOL_T_Iburst = 60,                  /* T_Iburst  */
  YYSYMBOL_T_Ident = 61,                   /* T_Ident  */
  YYSYMBOL_T_Ignore = 62,                  /* T_Ignore  */
  YYSYMBOL_T_Ignorehash = 63,              /* T_Ignorehash  */
  YYSYMBOL_T_Incalloc = 64,                /* T_Incalloc  */
  YYSYMBOL_T_Incmem = 65,                  /* T_Incmem  */
  YYSYMBOL_T_Initalloc = 66,               /* T_Initalloc  */
  YYSYMBOL_T_Initmem = 67,                 /* T_Initmem  */
  YYSYMBOL_T_Includefile = 68,             /* T_Includefile  */
  YYSYMBOL_T_Integer = 69,                 /* T_Integer  */
  YYSYMBOL_T_Interface = 70,               /* T_Interface  */
  YYSYMBOL_T_Intrange = 71,                /* T_Intrange  */
  YYSYMBOL_T_Io = 72,                      /* T_Io  */
  YYSYMBOL_T_Ippeerlimit = 73,             /* T_Ippeerlimit  */
  YYSYMBOL_T_Ipv4 = 74,                    /* T_Ipv4  */
  YYSYMBOL_T_Ipv4_flag = 75,               /* T_Ipv4_flag  */
  YYSYMBOL_T_Ipv6 = 76,                    /* T_Ipv6  */
  YYSYMBOL_T_Ipv6_flag = 77,               /* T_Ipv6_flag  */
  YYSYMBOL_T_Kernel = 78,                  /* T_Kernel  */
  YYSYMBOL_T_Key = 79,                     /* T_Key  */
  YYSYMBOL_T_Keys = 80,                    /* T_Keys  */
  YYSYMBOL_T_Keysdir = 81,                 /* T_Keysdir  */
  YYSYMBOL_T_Kod = 82,                     /* T_Kod  */
  YYSYMBOL_T_Leapfile = 83,                /* T_Leapfile  */
  YYSYMBOL_T_Leapsmearinterval = 84,       /* T_Leapsmearinterval  */
  YYSYMBOL_T_Limited = 85,                 /* T_Limited  */
  YYSYMBOL_T_Link = 86,                    /* T_Link  */
  YYSYMBOL_T_Listen = 87,                  /* T_Listen  */
  YYSYMBOL_T_Logconfig = 88,               /* T_Logconfig  */
  YYSYMBOL_T_Logfile = 89,                 /* T_Logfile  */
  YYSYMBOL_T_Loopstats = 90,               /* T_Loopstats  */
  YYSYMBOL_T_Lowpriotrap = 91,             /* T_Lowpriotrap  */
  YYSYMBOL_T_Manycastclient = 92,          /* T_Manycastclient  */
  YYSYMBOL_T_Manycastserver = 93,          /* T_Manycastserver  */
  YYSYMBOL_T_Mask = 94,                    /* T_Mask  */
  YYSYMBOL_T_Maxage = 95,                  /* T_Maxage  */
  YYSYMBOL_T_Maxclock = 96,                /* T_Maxclock  */
  YYSYMBOL_T_Maxdepth = 97,                /* T_Maxdepth  */
  YYSYMBOL_T_Maxdist = 98,                 /* T_Maxdist  */
  YYSYMBOL_T_Maxmem = 99,                  /* T_Maxmem  */
  YYSYMBOL_T_Maxpoll = 100,                /* T_Maxpoll  */
  YYSYMBOL_T_Mdnstries = 101,              /* T_Mdnstries  */
  YYSYMBOL_T_Mem = 102,                    /* T_Mem  */
  YYSYMBOL_T_Memlock = 103,                /* T_Memlock  */
  YYSYMBOL_T_Minclock = 104,               /* T_Minclock  */
  YYSYMBOL_T_Mindepth = 105,               /* T_Mindepth  */
  YYSYMBOL_T_Mindist = 106,                /* T_Mindist  */
  YYSYMBOL_T_Minimum = 107,                /* T_Minimum  */
  YYSYMBOL_T_Minjitter = 108,              /* T_Minjitter  */
  YYSYMBOL_T_Minpoll = 109,                /* T_Minpoll  */
  YYSYMBOL_T_Minsane = 110,                /* T_Minsane  */
  YYSYMBOL_T_Mode = 111,                   /* T_Mode  */
  YYSYMBOL_T_Mode7 = 112,                  /* T_Mode7  */
  YYSYMBOL_T_Monitor = 113,                /* T_Monitor  */
  YYSYMBOL_T_Month = 114,                  /* T_Month  */
  YYSYMBOL_T_Mru = 115,                    /* T_Mru  */
  YYSYMBOL_T_Mssntp = 116,                 /* T_Mssntp  */
  YYSYMBOL_T_Multicastclient = 117,        /* T_Multicastclient  */
  YYSYMBOL_T_Nic = 118,                    /* T_Nic  */
  YYSYMBOL_T_Nolink = 119,                 /* T_Nolink  */
  YYSYMBOL_T_Nomodify = 120,               /* T_Nomodify  */
  YYSYMBOL_T_Nomrulist = 121,              /* T_Nomrulist  */
  YYSYMBOL_T_None = 122,                   /* T_None  */
  YYSYMBOL_T_Nonvolatile = 123,            /* T_Nonvolatile  */
  YYSYMBOL_T_Noepeer = 124,                /* T_Noepeer  */
  YYSYMBOL_T_Nopeer = 125,                 /* T_Nopeer  */
  YYSYMBOL_T_Noquery = 126,                /* T_Noquery  */
  YYSYMBOL_T_Noselect = 127,               /* T_Noselect  */
  YYSYMBOL_T_Noserve = 128,                /* T_Noserve  */
  YYSYMBOL_T_Notrap = 129,                 /* T_Notrap  */
  YYSYMBOL_T_Notrust = 130,                /* T_Notrust  */
  YYSYMBOL_T_Ntp = 131,                    /* T_Ntp  */
  YYSYMBOL_T_Ntpport = 132,                /* T_Ntpport  */
  YYSYMBOL_T_NtpSignDsocket = 133,         /* T_NtpSignDsocket  */
  YYSYMBOL_T_Orphan = 134,                 /* T_Orphan  */
  YYSYMBOL_T_Orphanwait = 135,             /* T_Orphanwait  */
  YYSYMBOL_T_PCEdigest = 136,              /* T_PCEdigest  */
  YYSYMBOL_T_Panic = 137,                  /* T_Panic  */
  YYSYMBOL_T_Peer = 138,                   /* T_Peer  */
  YYSYMBOL_T_Peerstats = 139,              /* T_Peerstats  */
  YYSYMBOL_T_Phone = 140,                  /* T_Phone  */
  YYSYMBOL_T_Pid = 141,                    /* T_Pid  */
  YYSYMBOL_T_Pidfile = 142,                /* T_Pidfile  */
  YYSYMBOL_T_Poll = 143,                   /* T_Poll  */
  YYSYMBOL_T_PollSkewList = 144,           /* T_PollSkewList  */
  YYSYMBOL_T_Pool = 145,                   /* T_Pool  */
  YYSYMBOL_T_Port = 146,                   /* T_Port  */
  YYSYMBOL_T_PpsData = 147,                /* T_PpsData  */
  YYSYMBOL_T_Preempt = 148,                /* T_Preempt  */
  YYSYMBOL_T_Prefer = 149,                 /* T_Prefer  */
  YYSYMBOL_T_Protostats = 150,             /* T_Protostats  */
  YYSYMBOL_T_Pw = 151,                     /* T_Pw  */
  YYSYMBOL_T_Randfile = 152,               /* T_Randfile  */
  YYSYMBOL_T_Rawstats = 153,               /* T_Rawstats  */
  YYSYMBOL_T_Refid = 154,                  /* T_Refid  */
  YYSYMBOL_T_Requestkey = 155,             /* T_Requestkey  */
  YYSYMBOL_T_Reset = 156,                  /* T_Reset  */
  YYSYMBOL_T_Restrict = 157,               /* T_Restrict  */
  YYSYMBOL_T_Revoke = 158,                 /* T_Revoke  */
  YYSYMBOL_T_Rlimit = 159,                 /* T_Rlimit  */
  YYSYMBOL_T_Saveconfigdir = 160,          /* T_Saveconfigdir  */
  YYSYMBOL_T_Server = 161,                 /* T_Server  */
  YYSYMBOL_T_Serverresponse = 162,         /* T_Serverresponse  */
  YYSYMBOL_T_ServerresponseFuzz = 163,     /* T_ServerresponseFuzz  */
  YYSYMBOL_T_Setvar = 164,                 /* T_Setvar  */
  YYSYMBOL_T_Source = 165,                 /* T_Source  */
  YYSYMBOL_T_Stacksize = 166,              /* T_Stacksize  */
  YYSYMBOL_T_Statistics = 167,             /* T_Statistics  */
  YYSYMBOL_T_Stats = 168,                  /* T_Stats  */
  YYSYMBOL_T_Statsdir = 169,               /* T_Statsdir  */
  YYSYMBOL_T_Step = 170,                   /* T_Step  */
  YYSYMBOL_T_Stepback = 171,               /* T_Stepback  */
  YYSYMBOL_T_Stepfwd = 172,                /* T_Stepfwd  */
  YYSYMBOL_T_Stepout = 173,                /* T_Stepout  */
  YYSYMBOL_T_Stratum = 174,                /* T_Stratum  */
  YYSYMBOL_T_String = 175,                 /* T_String  */
  YYSYMBOL_T_Sys = 176,                    /* T_Sys  */
  YYSYMBOL_T_Sysstats = 177,               /* T_Sysstats  */
  YYSYMBOL_T_Tick = 178,                   /* T_Tick  */
  YYSYMBOL_T_Time1 = 179,                  /* T_Time1  */
  YYSYMBOL_T_Time2 = 180,                  /* T_Time2  */
  YYSYMBOL_T_TimeData = 181,               /* T_TimeData  */
  YYSYMBOL_T_Timer = 182,                  /* T_Timer  */
  YYSYMBOL_T_Timingstats = 183,            /* T_Timingstats  */
  YYSYMBOL_T_Tinker = 184,                 /* T_Tinker  */
  YYSYMBOL_T_Tos = 185,                    /* T_Tos  */
  YYSYMBOL_T_Trap = 186,                   /* T_Trap  */
  YYSYMBOL_T_True = 187,                   /* T_True  */
  YYSYMBOL_T_Trustedkey = 188,             /* T_Trustedkey  */
  YYSYMBOL_T_Ttl = 189,                    /* T_Ttl  */
  YYSYMBOL_T_Type = 190,                   /* T_Type  */
  YYSYMBOL_T_U_int = 191,                  /* T_U_int  */
  YYSYMBOL_T_UEcrypto = 192,               /* T_UEcrypto  */
  YYSYMBOL_T_UEcryptonak = 193,            /* T_UEcryptonak  */
  YYSYMBOL_T_UEdigest = 194,               /* T_UEdigest  */
  YYSYMBOL_T_Unconfig = 195,               /* T_Unconfig  */
  YYSYMBOL_T_Unpeer = 196,                 /* T_Unpeer  */
  YYSYMBOL_T_Version = 197,                /* T_Version  */
  YYSYMBOL_T_WanderThreshold = 198,        /* T_WanderThreshold  */
  YYSYMBOL_T_Week = 199,                   /* T_Week  */
  YYSYMBOL_T_Wildcard = 200,               /* T_Wildcard  */
  YYSYMBOL_T_Xleave = 201,                 /* T_Xleave  */
  YYSYMBOL_T_Xmtnonce = 202,               /* T_Xmtnonce  */
  YYSYMBOL_T_Year = 203,                   /* T_Year  */
  YYSYMBOL_T_Flag = 204,                   /* T_Flag  */
  YYSYMBOL_T_EOC = 205,                    /* T_EOC  */
  YYSYMBOL_T_Simulate = 206,               /* T_Simulate  */
  YYSYMBOL_T_Beep_Delay = 207,             /* T_Beep_Delay  */
  YYSYMBOL_T_Sim_Duration = 208,           /* T_Sim_Duration  */
  YYSYMBOL_T_Server_Offset = 209,          /* T_Server_Offset  */
  YYSYMBOL_T_Duration = 210,               /* T_Duration  */
  YYSYMBOL_T_Freq_Offset = 211,            /* T_Freq_Offset  */
  YYSYMBOL_T_Wander = 212,                 /* T_Wander  */
  YYSYMBOL_T_Jitter = 213,                 /* T_Jitter  */
  YYSYMBOL_T_Prop_Delay = 214,             /* T_Prop_Delay  */
  YYSYMBOL_T_Proc_Delay = 215,             /* T_Proc_Delay  */
  YYSYMBOL_216_ = 216,                     /* '|'  */
  YYSYMBOL_217_ = 217,                     /* '='  */
  YYSYMBOL_218_ = 218,                     /* '('  */
  YYSYMBOL_219_ = 219,                     /* ')'  */
  YYSYMBOL_220_ = 220,                     /* '{'  */
  YYSYMBOL_221_ = 221,                     /* '}'  */
  YYSYMBOL_YYACCEPT = 222,                 /* $accept  */
  YYSYMBOL_configuration = 223,            /* configuration  */
  YYSYMBOL_command_list = 224,             /* command_list  */
  YYSYMBOL_command = 225,                  /* command  */
  YYSYMBOL_server_command = 226,           /* server_command  */
  YYSYMBOL_client_type = 227,              /* client_type  */
  YYSYMBOL_address = 228,                  /* address  */
  YYSYMBOL_ip_address = 229,               /* ip_address  */
  YYSYMBOL_address_fam = 230,              /* address_fam  */
  YYSYMBOL_option_list = 231,              /* option_list  */
  YYSYMBOL_option = 232,                   /* option  */
  YYSYMBOL_option_flag = 233,              /* option_flag  */
  YYSYMBOL_option_flag_keyword = 234,      /* option_flag_keyword  */
  YYSYMBOL_option_int = 235,               /* option_int  */
  YYSYMBOL_option_int_keyword = 236,       /* option_int_keyword  */
  YYSYMBOL_option_str = 237,               /* option_str  */
  YYSYMBOL_option_str_keyword = 238,       /* option_str_keyword  */
  YYSYMBOL_unpeer_command = 239,           /* unpeer_command  */
  YYSYMBOL_unpeer_keyword = 240,           /* unpeer_keyword  */
  YYSYMBOL_other_mode_command = 241,       /* other_mode_command  */
  YYSYMBOL_authentication_command = 242,   /* authentication_command  */
  YYSYMBOL_crypto_command_list = 243,      /* crypto_command_list  */
  YYSYMBOL_crypto_command = 244,           /* crypto_command  */
  YYSYMBOL_crypto_str_keyword = 245,       /* crypto_str_keyword  */
  YYSYMBOL_orphan_mode_command = 246,      /* orphan_mode_command  */
  YYSYMBOL_tos_option_list = 247,          /* tos_option_list  */
  YYSYMBOL_tos_option = 248,               /* tos_option  */
  YYSYMBOL_tos_option_int_keyword = 249,   /* tos_option_int_keyword  */
  YYSYMBOL_tos_option_dbl_keyword = 250,   /* tos_option_dbl_keyword  */
  YYSYMBOL_monitoring_command = 251,       /* monitoring_command  */
  YYSYMBOL_stats_list = 252,               /* stats_list  */
  YYSYMBOL_stat = 253,                     /* stat  */
  YYSYMBOL_filegen_option_list = 254,      /* filegen_option_list  */
  YYSYMBOL_filegen_option = 255,           /* filegen_option  */
  YYSYMBOL_link_nolink = 256,              /* link_nolink  */
  YYSYMBOL_enable_disable = 257,           /* enable_disable  */
  YYSYMBOL_filegen_type = 258,             /* filegen_type  */
  YYSYMBOL_access_control_command = 259,   /* access_control_command  */
  YYSYMBOL_restrict_mask = 260,            /* restrict_mask  */
  YYSYMBOL_res_ippeerlimit = 261,          /* res_ippeerlimit  */
  YYSYMBOL_ac_flag_list = 262,             /* ac_flag_list  */
  YYSYMBOL_access_control_flag = 263,      /* access_control_flag  */
  YYSYMBOL_discard_option_list = 264,      /* discard_option_list  */
  YYSYMBOL_discard_option = 265,           /* discard_option  */
  YYSYMBOL_discard_option_keyword = 266,   /* discard_option_keyword  */
  YYSYMBOL_mru_option_list = 267,          /* mru_option_list  */
  YYSYMBOL_mru_option = 268,               /* mru_option  */
  YYSYMBOL_mru_option_keyword = 269,       /* mru_option_keyword  */
  YYSYMBOL_fudge_command = 270,            /* fudge_command  */
  YYSYMBOL_fudge_factor_list = 271,        /* fudge_factor_list  */
  YYSYMBOL_fudge_factor = 272,             /* fudge_factor  */
  YYSYMBOL_fudge_factor_dbl_keyword = 273, /* fudge_factor_dbl_keyword  */
  YYSYMBOL_fudge_factor_bool_keyword = 274, /* fudge_factor_bool_keyword  */
  YYSYMBOL_device_command = 275,           /* device_command  */
  YYSYMBOL_device_item_list = 276,         /* device_item_list  */
  YYSYMBOL_device_item = 277,              /* device_item  */
  YYSYMBOL_device_item_path_keyword = 278, /* device_item_path_keyword  */
  YYSYMBOL_rlimit_command = 279,           /* rlimit_command  */
  YYSYMBOL_rlimit_option_list = 280,       /* rlimit_option_list  */
  YYSYMBOL_rlimit_option = 281,            /* rlimit_option  */
  YYSYMBOL_rlimit_option_keyword = 282,    /* rlimit_option_keyword  */
  YYSYMBOL_system_option_command = 283,    /* system_option_command  */
  YYSYMBOL_system_option_list = 284,       /* system_option_list  */
  YYSYMBOL_system_option = 285,            /* system_option  */
  YYSYMBOL_system_option_flag_keyword = 286, /* system_option_flag_keyword  */
  YYSYMBOL_system_option_local_flag_keyword = 287, /* system_option_local_flag_keyword  */
  YYSYMBOL_tinker_command = 288,           /* tinker_command  */
  YYSYMBOL_tinker_option_list = 289,       /* tinker_option_list  */
  YYSYMBOL_tinker_option = 290,            /* tinker_option  */
  YYSYMBOL_tinker_option_keyword = 291,    /* tinker_option_keyword  */
  YYSYMBOL_miscellaneous_command = 292,    /* miscellaneous_command  */
  YYSYMBOL_misc_cmd_dbl_keyword = 293,     /* misc_cmd_dbl_keyword  */
  YYSYMBOL_misc_cmd_int_keyword = 294,     /* misc_cmd_int_keyword  */
  YYSYMBOL_opt_hash_check = 295,           /* opt_hash_check  */
  YYSYMBOL_misc_cmd_str_keyword = 296,     /* misc_cmd_str_keyword  */
  YYSYMBOL_misc_cmd_str_lcl_keyword = 297, /* misc_cmd_str_lcl_keyword  */
  YYSYMBOL_drift_parm = 298,               /* drift_parm  */
  YYSYMBOL_pollskew_list = 299,            /* pollskew_list  */
  YYSYMBOL_pollskew_spec = 300,            /* pollskew_spec  */
  YYSYMBOL_pollskew_cycle = 301,           /* pollskew_cycle  */
  YYSYMBOL_variable_assign = 302,          /* variable_assign  */
  YYSYMBOL_t_default_or_zero = 303,        /* t_default_or_zero  */
  YYSYMBOL_trap_option_list = 304,         /* trap_option_list  */
  YYSYMBOL_trap_option = 305,              /* trap_option  */
  YYSYMBOL_log_config_list = 306,          /* log_config_list  */
  YYSYMBOL_log_config_command = 307,       /* log_config_command  */
  YYSYMBOL_interface_command = 308,        /* interface_command  */
  YYSYMBOL_interface_nic = 309,            /* interface_nic  */
  YYSYMBOL_nic_rule_class = 310,           /* nic_rule_class  */
  YYSYMBOL_nic_rule_action = 311,          /* nic_rule_action  */
  YYSYMBOL_reset_command = 312,            /* reset_command  */
  YYSYMBOL_counter_set_list = 313,         /* counter_set_list  */
  YYSYMBOL_counter_set_keyword = 314,      /* counter_set_keyword  */
  YYSYMBOL_integer_list = 315,             /* integer_list  */
  YYSYMBOL_integer_list_range = 316,       /* integer_list_range  */
  YYSYMBOL_integer_list_range_elt = 317,   /* integer_list_range_elt  */
  YYSYMBOL_integer_range = 318,            /* integer_range  */
  YYSYMBOL_string_list = 319,              /* string_list  */
  YYSYMBOL_address_list = 320,             /* address_list  */
  YYSYMBOL_boolean = 321,                  /* boolean  */
  YYSYMBOL_number = 322,                   /* number  */
  YYSYMBOL_basedate = 323,                 /* basedate  */
  YYSYMBOL_simulate_command = 324,         /* simulate_command  */
  YYSYMBOL_sim_conf_start = 325,           /* sim_conf_start  */
  YYSYMBOL_sim_init_statement_list = 326,  /* sim_init_statement_list  */
  YYSYMBOL_sim_init_statement = 327,       /* sim_init_statement  */
  YYSYMBOL_sim_init_keyword = 328,         /* sim_init_keyword  */
  YYSYMBOL_sim_server_list = 329,          /* sim_server_list  */
  YYSYMBOL_sim_server = 330,               /* sim_server  */
  YYSYMBOL_sim_server_offset = 331,        /* sim_server_offset  */
  YYSYMBOL_sim_server_name = 332,          /* sim_server_name  */
  YYSYMBOL_sim_act_list = 333,             /* sim_act_list  */
  YYSYMBOL_sim_act = 334,                  /* sim_act  */
  YYSYMBOL_sim_act_stmt_list = 335,        /* sim_act_stmt_list  */
  YYSYMBOL_sim_act_stmt = 336,             /* sim_act_stmt  */
  YYSYMBOL_sim_act_keyword = 337           /* sim_act_keyword  */
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

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
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
#define YYFINAL  225
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   717

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  222
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  116
/* YYNRULES -- Number of rules.  */
#define YYNRULES  346
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  467

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   470


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
     218,   219,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   217,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   220,   216,   221,     2,     2,     2,     2,
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
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   399,   399,   403,   404,   405,   420,   421,   422,   423,
     424,   425,   426,   427,   428,   429,   430,   431,   432,   433,
     434,   442,   452,   453,   454,   455,   456,   460,   461,   466,
     471,   473,   479,   480,   488,   489,   490,   494,   499,   500,
     501,   502,   503,   504,   505,   506,   507,   511,   513,   518,
     519,   520,   521,   522,   523,   527,   532,   541,   551,   552,
     562,   564,   566,   568,   579,   586,   588,   593,   595,   597,
     599,   601,   611,   617,   618,   626,   628,   640,   641,   642,
     643,   644,   653,   658,   663,   671,   673,   675,   677,   682,
     683,   684,   685,   686,   687,   688,   689,   690,   694,   695,
     704,   706,   715,   725,   730,   738,   739,   740,   741,   742,
     743,   744,   745,   750,   751,   759,   769,   778,   793,   798,
     799,   803,   804,   808,   809,   810,   811,   812,   813,   814,
     823,   827,   831,   840,   849,   865,   881,   891,   900,   916,
     917,   925,   926,   946,   947,   955,   966,   967,   968,   969,
     970,   971,   972,   973,   974,   975,   976,   977,   978,   979,
     980,   981,   982,   986,   991,   999,  1004,  1005,  1006,  1010,
    1015,  1023,  1028,  1029,  1030,  1031,  1032,  1033,  1034,  1035,
    1043,  1053,  1058,  1066,  1068,  1070,  1079,  1081,  1086,  1087,
    1088,  1092,  1093,  1094,  1095,  1103,  1113,  1118,  1126,  1131,
    1132,  1140,  1145,  1150,  1158,  1163,  1164,  1165,  1174,  1176,
    1181,  1186,  1194,  1196,  1213,  1214,  1215,  1216,  1217,  1218,
    1222,  1223,  1224,  1225,  1226,  1227,  1235,  1240,  1245,  1253,
    1258,  1259,  1260,  1261,  1262,  1263,  1264,  1265,  1266,  1267,
    1276,  1277,  1278,  1285,  1292,  1299,  1315,  1334,  1342,  1344,
    1346,  1348,  1350,  1352,  1354,  1361,  1366,  1367,  1368,  1372,
    1376,  1385,  1387,  1390,  1394,  1398,  1399,  1400,  1404,  1415,
    1433,  1446,  1447,  1452,  1478,  1484,  1489,  1494,  1496,  1501,
    1502,  1510,  1512,  1520,  1525,  1533,  1558,  1565,  1575,  1576,
    1580,  1581,  1582,  1583,  1587,  1588,  1589,  1593,  1598,  1603,
    1611,  1612,  1613,  1614,  1615,  1616,  1617,  1627,  1632,  1640,
    1645,  1653,  1655,  1659,  1664,  1669,  1677,  1682,  1690,  1699,
    1700,  1704,  1705,  1709,  1717,  1735,  1739,  1744,  1752,  1757,
    1758,  1762,  1767,  1775,  1780,  1785,  1790,  1795,  1803,  1808,
    1813,  1821,  1826,  1827,  1828,  1829,  1830
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
  "T_Default", "T_Delrestrict", "T_Device", "T_Digest", "T_Disable",
  "T_Discard", "T_Dispersion", "T_Double", "T_Driftfile", "T_Drop",
  "T_Dscp", "T_Ellipsis", "T_Enable", "T_End", "T_Epeer", "T_False",
  "T_File", "T_Filegen", "T_Filenum", "T_Flag1", "T_Flag2", "T_Flag3",
  "T_Flag4", "T_Flake", "T_Floor", "T_Freq", "T_Fudge", "T_Fuzz", "T_Host",
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
  "access_control_command", "restrict_mask", "res_ippeerlimit",
  "ac_flag_list", "access_control_flag", "discard_option_list",
  "discard_option", "discard_option_keyword", "mru_option_list",
  "mru_option", "mru_option_keyword", "fudge_command", "fudge_factor_list",
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

#define YYPACT_NINF (-280)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-7)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      10,  -173,   -31,  -280,  -280,  -280,   -11,  -280,   -89,   -28,
     301,     3,  -115,  -280,   301,  -280,    82,   -28,  -280,   -98,
    -280,   -94,   -83,   -75,  -280,   -74,  -280,  -280,   -28,    19,
     213,   -28,  -280,  -280,   -69,  -280,   -62,  -280,  -280,  -280,
      35,    15,   101,    45,   -42,  -280,  -280,   -56,    82,   -53,
    -280,    53,   582,   -49,   -65,    49,  -280,  -280,  -280,   129,
     202,   -64,  -280,   -28,  -280,   -28,  -280,  -280,  -280,  -280,
    -280,  -280,  -280,  -280,  -280,  -280,  -280,     0,    61,   -29,
     -24,  -280,   -22,  -280,  -280,   -76,  -280,  -280,  -280,   102,
     -49,  -280,    62,  -280,  -280,  -113,  -280,   -18,  -280,  -280,
    -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,
     301,  -280,  -280,  -280,  -280,  -280,  -280,     3,  -280,    89,
     122,  -280,   301,  -280,  -280,  -280,  -280,  -280,  -280,  -280,
    -280,  -280,   281,   384,  -280,  -280,    -1,  -280,   -74,  -280,
    -280,   -28,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,
    -280,   213,  -280,    92,   -28,  -280,  -280,   -13,    -5,  -280,
    -280,  -280,  -280,  -280,  -280,  -280,  -280,    15,  -280,    91,
     143,   145,    91,    62,  -280,  -280,  -280,  -280,   -42,  -280,
     111,   -35,  -280,    82,  -280,  -280,  -280,  -280,  -280,  -280,
    -280,  -280,  -280,  -280,  -280,  -280,    53,  -280,     0,     6,
    -280,  -280,  -280,   -38,  -280,  -280,  -280,  -280,  -280,  -280,
    -280,  -280,   582,  -280,   115,     0,  -280,  -280,  -280,   116,
     -65,  -280,  -280,  -280,   117,  -280,   -16,  -280,  -280,  -280,
    -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,     8,
    -112,  -280,  -280,  -280,  -280,  -280,   118,  -280,    17,  -280,
     -49,  -280,  -280,  -280,  -113,  -280,    26,  -280,  -280,  -280,
    -280,  -280,    21,    27,  -280,  -280,  -280,  -280,  -280,    28,
     138,  -280,  -280,   281,  -280,     0,   -38,  -280,  -280,  -280,
    -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,   140,
    -280,   141,  -280,    91,    91,  -280,    91,  -280,  -280,    38,
    -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,
    -280,   -61,   173,  -280,  -280,  -280,   387,  -280,  -280,  -280,
    -280,  -280,  -280,  -280,  -280,   -87,    12,     5,  -280,  -280,
    -280,  -280,  -280,  -280,  -280,    54,  -280,  -280,     1,  -280,
    -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,    14,  -280,
     513,  -280,  -280,   513,  -280,   208,   -49,   170,  -280,   172,
    -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,
    -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,
     -57,  -280,    72,    31,    47,  -151,  -280,    30,  -280,     0,
    -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,   186,
    -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,
    -280,  -280,  -280,  -280,  -280,  -280,   199,  -280,  -280,   513,
     513,   513,  -280,  -280,  -280,  -280,    42,  -280,  -280,  -280,
     -28,  -280,  -280,  -280,    48,  -280,  -280,  -280,  -280,  -280,
      50,    52,     0,    56,  -192,  -280,    59,     0,  -280,  -280,
    -280,    51,   139,  -280,  -280,  -280,  -280,  -280,    85,    64,
      57,  -280,    70,  -280,     0,  -280,  -280
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       0,     0,     0,    25,    60,   256,     0,    73,     0,     0,
       0,     0,   270,   259,     0,   248,     0,     0,   264,     0,
     288,     0,     0,     0,   260,     0,   265,    26,     0,     0,
       0,     0,   289,   257,     0,    24,     0,   266,   271,    23,
       0,     0,     0,     0,     0,   267,    22,     0,     0,     0,
     258,     0,     0,     0,     0,     0,    58,    59,   325,     0,
       2,     0,     7,     0,     8,     0,     9,    10,    13,    11,
      12,    14,    20,    15,    16,    17,    18,     0,     0,     0,
       0,   240,     0,   241,    19,     0,     5,    64,    65,    66,
       0,    29,   139,    30,    31,     0,    27,     0,   214,   215,
     216,   217,   220,   218,   219,   221,   222,   223,   224,   225,
     209,   211,   212,   213,   166,   167,   168,   130,   164,     0,
     268,   249,   208,   105,   106,   107,   108,   112,   109,   110,
     111,   113,     0,     6,    67,    68,   263,   285,   250,   284,
     317,    61,    63,   172,   173,   174,   175,   176,   177,   178,
     179,   131,   170,     0,    62,    72,   315,   251,   252,    69,
     300,   301,   302,   303,   304,   305,   306,   297,   299,   141,
      30,    31,   141,   139,    70,   207,   205,   206,   201,   203,
       0,     0,   253,   100,   104,   101,   230,   231,   232,   233,
     234,   235,   236,   237,   238,   239,   226,   228,     0,     0,
      89,    90,    91,     0,    92,    93,    99,    94,    98,    95,
      96,    97,    82,    84,     0,     0,    88,   279,   311,     0,
      71,   310,   312,   308,   255,     1,     0,     4,    32,    57,
     322,   321,   242,   243,   244,   245,   296,   295,   294,     0,
       0,    81,    77,    78,    79,    80,     0,    74,     0,   138,
       0,   137,   200,   199,   195,   197,     0,    28,   210,   163,
     165,   269,   102,     0,   191,   192,   193,   194,   190,     0,
       0,   188,   189,   180,   182,     0,     0,   246,   262,   261,
     247,   283,   316,   169,   171,   314,   275,   274,   272,     0,
     298,     0,   143,   141,   141,   143,   141,   202,   204,     0,
     103,   227,   229,   323,   320,   318,   319,    87,    83,    85,
      86,   254,     0,   309,   307,     3,    21,   290,   291,   292,
     287,   293,   286,   329,   330,     0,     0,     0,    76,    75,
     140,   196,   198,   122,   121,     0,   119,   120,     0,   114,
     117,   118,   186,   187,   185,   181,   183,   184,     0,   142,
     133,   143,   143,   136,   143,   278,     0,     0,   280,     0,
      38,    39,    40,    56,    49,    51,    50,    53,    41,    42,
      43,    44,    52,    54,    45,    46,    33,    34,    37,    35,
       0,    36,     0,     0,     0,     0,   332,     0,   327,     0,
     115,   129,   125,   127,   123,   124,   126,   128,   116,     0,
     146,   147,   148,   149,   150,   151,   152,   154,   155,   153,
     156,   157,   158,   159,   160,   161,     0,   162,   144,   134,
     135,   132,   277,   276,   282,   281,     0,    47,    48,    55,
       0,   326,   324,   331,     0,   328,   273,   145,   313,   335,
       0,     0,     0,     0,     0,   337,     0,     0,   333,   336,
     334,     0,     0,   342,   343,   344,   345,   346,     0,     0,
       0,   338,     0,   340,     0,   339,   341
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -280,  -280,  -280,   -36,  -280,  -280,    -9,    -7,  -280,  -280,
    -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,
    -280,  -280,  -280,  -280,  -280,  -280,    69,  -280,  -280,  -280,
    -280,   -45,  -280,  -280,  -280,  -280,  -280,  -280,   114,  -157,
    -279,  -280,  -280,   171,  -280,  -280,   142,  -280,  -280,  -280,
      16,  -280,  -280,  -280,  -280,    68,  -280,  -280,  -280,   123,
    -280,  -280,   278,   -71,  -280,  -280,  -280,  -280,   106,  -280,
    -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,  -280,
    -280,  -280,  -280,  -280,  -280,   166,  -280,  -280,  -280,  -280,
    -280,  -280,   144,  -280,  -280,    87,  -280,  -280,   274,    37,
    -196,  -280,  -280,  -280,  -280,   -10,  -280,  -280,   -59,  -280,
    -280,  -280,  -128,  -280,  -135,  -280
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,    59,    60,    61,    62,    63,   140,    96,    97,   316,
     376,   377,   378,   379,   380,   381,   382,    64,    65,    66,
      67,    89,   247,   248,    68,   212,   213,   214,   215,    69,
     183,   131,   262,   339,   340,   341,   398,    70,   251,   292,
     350,   418,   117,   118,   119,   151,   152,   153,    71,   273,
     274,   275,   276,    72,   254,   255,   256,    73,   178,   179,
     180,    74,   110,   111,   112,   113,    75,   196,   197,   198,
      76,    77,    78,   280,    79,    80,   121,   158,   288,   289,
     182,   423,   311,   358,   138,   139,    81,    82,   322,   239,
      83,   167,   168,   224,   220,   221,   222,   157,   141,   307,
     232,   216,    84,    85,   325,   326,   327,   385,   386,   441,
     387,   444,   445,   458,   459,   460
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      95,    92,   302,   184,   218,   391,   175,   304,   132,   356,
     383,     1,   427,   317,   114,   295,   353,   236,   443,   310,
       2,   278,   160,   161,   226,   286,     3,     4,     5,   448,
     392,   305,    86,   173,   252,     6,     7,   230,    87,   258,
     237,     8,     9,   162,    10,    11,   217,    93,    12,    94,
      13,   258,    14,    15,   228,   333,   229,    16,    88,   186,
     120,   176,   279,   334,   287,   238,    17,   335,   253,   231,
     432,    18,   419,   420,   383,   421,    90,   133,    19,   346,
      20,   134,   318,   249,   319,   357,    91,   163,   142,   187,
      21,    22,   135,    23,    24,   323,   324,   277,    25,    26,
     136,   137,    27,    28,   159,   123,   155,   336,   188,   124,
     115,    29,   189,   156,   174,   393,   116,   164,   223,   181,
     323,   324,   185,   394,   177,    30,    91,    31,    32,   225,
     233,   169,   282,    33,   428,   241,   351,   352,   300,   354,
     337,   227,   395,    34,   240,   282,   234,    91,    35,   306,
      36,   235,    37,   219,    38,    39,   250,   257,   260,   261,
     242,   284,   285,   243,   291,    40,    41,    42,    43,    44,
      45,    46,   125,   293,    47,   294,   170,    48,   171,    49,
     298,   303,   299,   320,   309,   312,   314,   328,    50,   315,
     190,   165,   329,   435,    51,    52,    53,   166,    54,    55,
     396,   332,   342,   343,   397,    56,    57,   344,   321,   348,
     349,   338,     2,   355,   359,    -6,    58,   388,     3,     4,
       5,   126,   389,   191,   192,   193,   194,     6,     7,   390,
     399,   195,   127,     8,     9,   128,    10,    11,   422,   425,
      12,   426,    13,   330,    14,    15,   446,   429,   430,    16,
     434,   451,   431,   244,   245,   436,   437,   440,    17,   129,
     246,   438,   443,    18,   450,   130,   172,   442,   466,   463,
      19,   452,    20,   447,   464,   465,    91,   143,   144,   145,
     146,   308,    21,    22,   263,    23,    24,   296,   259,   345,
      25,    26,   122,   283,    27,    28,   453,   454,   455,   456,
     457,   297,   301,    29,   281,   154,   461,   313,   147,    98,
     148,   290,   149,   347,    99,   384,   449,    30,   150,    31,
      32,   100,   331,   462,     0,    33,   433,     0,     0,     0,
     264,   265,   266,   267,     0,    34,     0,     0,     0,     0,
      35,     0,    36,     0,    37,     0,    38,    39,     0,   424,
     453,   454,   455,   456,   457,     0,     0,    40,    41,    42,
      43,    44,    45,    46,     0,     0,    47,     0,     0,    48,
       0,    49,     0,     0,     0,     0,     0,     0,     0,   101,
      50,     0,     0,     0,     0,     0,    51,    52,    53,   268,
      54,    55,     0,     0,     2,     0,   360,    56,    57,     0,
       3,     4,     5,     0,     0,     0,   361,    -6,    58,     6,
       7,     0,     0,   102,   103,     8,     9,     0,    10,    11,
       0,   439,    12,     0,    13,     0,    14,    15,     0,     0,
       0,    16,   104,     0,     0,   269,     0,   105,     0,     0,
      17,     0,     0,     0,     0,    18,     0,   362,   363,     0,
       0,     0,    19,     0,    20,   270,     0,     0,     0,     0,
     271,   272,     0,     0,    21,    22,   364,    23,    24,   106,
       0,     0,    25,    26,     0,     0,    27,    28,     0,     0,
       0,     0,     0,     0,     0,    29,     0,   365,     0,     0,
       0,     0,     0,   107,   108,   109,   366,     0,   367,    30,
       0,    31,    32,     0,     0,     0,     0,    33,     0,     0,
       0,     0,     0,     0,   368,     0,     0,    34,     0,     0,
       0,     0,    35,     0,    36,     0,    37,     0,    38,    39,
       0,     0,     0,     0,     0,   369,   370,     0,     0,    40,
      41,    42,    43,    44,    45,    46,     0,     0,    47,     0,
       0,    48,     0,    49,     0,     0,     0,   400,     0,     0,
       0,     0,    50,     0,     0,     0,   401,     0,    51,    52,
      53,     0,    54,    55,   371,   402,   372,     0,     0,    56,
      57,     0,     0,     0,   373,     0,     0,     0,   374,   375,
      58,     0,     0,     0,   199,   403,   200,   201,   404,     0,
       0,     0,     0,   202,   405,     0,   203,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   406,
       0,     0,     0,   407,   408,     0,   204,   409,   410,   411,
       0,   412,   413,   414,     0,   415,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   416,     0,     0,   205,     0,
     206,     0,     0,     0,     0,     0,   207,     0,   208,     0,
       0,     0,   209,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     417,     0,     0,     0,     0,     0,   210,   211
};

static const yytype_int16 yycheck[] =
{
       9,     8,   198,    48,    69,     4,    48,    45,    17,    70,
     161,     1,    69,     5,    11,   172,   295,    39,   210,   215,
      10,    22,     7,     8,    60,    30,    16,    17,    18,   221,
      29,    69,   205,    42,   147,    25,    26,    37,    69,   110,
      62,    31,    32,    28,    34,    35,    53,    75,    38,    77,
      40,   122,    42,    43,    63,    34,    65,    47,    69,     6,
     175,   103,    63,    42,    69,    87,    56,    46,   181,    69,
     221,    61,   351,   352,   161,   354,   165,   175,    68,   275,
      70,   175,    74,    90,    76,   146,   175,    72,    69,    36,
      80,    81,   175,    83,    84,   207,   208,   133,    88,    89,
     175,   175,    92,    93,    69,    23,   175,    86,    55,    27,
     107,   101,    59,   175,    69,   114,   113,   102,    69,   175,
     207,   208,   175,   122,   166,   115,   175,   117,   118,     0,
      69,    30,   141,   123,   191,    33,   293,   294,   183,   296,
     119,   205,   141,   133,   220,   154,   175,   175,   138,   187,
     140,   175,   142,   218,   144,   145,    94,   175,    69,    37,
      58,    69,   175,    61,    73,   155,   156,   157,   158,   159,
     160,   161,    90,    30,   164,    30,    75,   167,    77,   169,
      69,   175,   217,   175,    69,    69,    69,    69,   178,   205,
     137,   176,   175,   389,   184,   185,   186,   182,   188,   189,
     199,   175,   175,   175,   203,   195,   196,    69,   200,    69,
      69,   190,    10,   175,    41,   205,   206,   205,    16,    17,
      18,   139,   217,   170,   171,   172,   173,    25,    26,   175,
     216,   178,   150,    31,    32,   153,    34,    35,    30,    69,
      38,    69,    40,   250,    42,    43,   442,   175,   217,    47,
     220,   447,   205,   151,   152,    69,    57,   209,    56,   177,
     158,   219,   210,    61,   205,   183,   165,   217,   464,   205,
      68,   220,    70,   217,   217,   205,   175,    64,    65,    66,
      67,   212,    80,    81,     3,    83,    84,   173,   117,   273,
      88,    89,    14,   151,    92,    93,   211,   212,   213,   214,
     215,   178,   196,   101,   138,    31,   221,   220,    95,     8,
      97,   167,    99,   276,    13,   325,   444,   115,   105,   117,
     118,    20,   254,   458,    -1,   123,   385,    -1,    -1,    -1,
      49,    50,    51,    52,    -1,   133,    -1,    -1,    -1,    -1,
     138,    -1,   140,    -1,   142,    -1,   144,   145,    -1,   356,
     211,   212,   213,   214,   215,    -1,    -1,   155,   156,   157,
     158,   159,   160,   161,    -1,    -1,   164,    -1,    -1,   167,
      -1,   169,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    78,
     178,    -1,    -1,    -1,    -1,    -1,   184,   185,   186,   108,
     188,   189,    -1,    -1,    10,    -1,     9,   195,   196,    -1,
      16,    17,    18,    -1,    -1,    -1,    19,   205,   206,    25,
      26,    -1,    -1,   112,   113,    31,    32,    -1,    34,    35,
      -1,   430,    38,    -1,    40,    -1,    42,    43,    -1,    -1,
      -1,    47,   131,    -1,    -1,   154,    -1,   136,    -1,    -1,
      56,    -1,    -1,    -1,    -1,    61,    -1,    60,    61,    -1,
      -1,    -1,    68,    -1,    70,   174,    -1,    -1,    -1,    -1,
     179,   180,    -1,    -1,    80,    81,    79,    83,    84,   168,
      -1,    -1,    88,    89,    -1,    -1,    92,    93,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   101,    -1,   100,    -1,    -1,
      -1,    -1,    -1,   192,   193,   194,   109,    -1,   111,   115,
      -1,   117,   118,    -1,    -1,    -1,    -1,   123,    -1,    -1,
      -1,    -1,    -1,    -1,   127,    -1,    -1,   133,    -1,    -1,
      -1,    -1,   138,    -1,   140,    -1,   142,    -1,   144,   145,
      -1,    -1,    -1,    -1,    -1,   148,   149,    -1,    -1,   155,
     156,   157,   158,   159,   160,   161,    -1,    -1,   164,    -1,
      -1,   167,    -1,   169,    -1,    -1,    -1,    44,    -1,    -1,
      -1,    -1,   178,    -1,    -1,    -1,    53,    -1,   184,   185,
     186,    -1,   188,   189,   187,    62,   189,    -1,    -1,   195,
     196,    -1,    -1,    -1,   197,    -1,    -1,    -1,   201,   202,
     206,    -1,    -1,    -1,    12,    82,    14,    15,    85,    -1,
      -1,    -1,    -1,    21,    91,    -1,    24,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   116,
      -1,    -1,    -1,   120,   121,    -1,    54,   124,   125,   126,
      -1,   128,   129,   130,    -1,   132,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   162,    -1,    -1,    96,    -1,
      98,    -1,    -1,    -1,    -1,    -1,   104,    -1,   106,    -1,
      -1,    -1,   110,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     197,    -1,    -1,    -1,    -1,    -1,   134,   135
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int16 yystos[] =
{
       0,     1,    10,    16,    17,    18,    25,    26,    31,    32,
      34,    35,    38,    40,    42,    43,    47,    56,    61,    68,
      70,    80,    81,    83,    84,    88,    89,    92,    93,   101,
     115,   117,   118,   123,   133,   138,   140,   142,   144,   145,
     155,   156,   157,   158,   159,   160,   161,   164,   167,   169,
     178,   184,   185,   186,   188,   189,   195,   196,   206,   223,
     224,   225,   226,   227,   239,   240,   241,   242,   246,   251,
     259,   270,   275,   279,   283,   288,   292,   293,   294,   296,
     297,   308,   309,   312,   324,   325,   205,    69,    69,   243,
     165,   175,   229,    75,    77,   228,   229,   230,     8,    13,
      20,    78,   112,   113,   131,   136,   168,   192,   193,   194,
     284,   285,   286,   287,    11,   107,   113,   264,   265,   266,
     175,   298,   284,    23,    27,    90,   139,   150,   153,   177,
     183,   253,   228,   175,   175,   175,   175,   175,   306,   307,
     228,   320,    69,    64,    65,    66,    67,    95,    97,    99,
     105,   267,   268,   269,   320,   175,   175,   319,   299,    69,
       7,     8,    28,    72,   102,   176,   182,   313,   314,    30,
      75,    77,   165,   228,    69,    48,   103,   166,   280,   281,
     282,   175,   302,   252,   253,   175,     6,    36,    55,    59,
     137,   170,   171,   172,   173,   178,   289,   290,   291,    12,
      14,    15,    21,    24,    54,    96,    98,   104,   106,   110,
     134,   135,   247,   248,   249,   250,   323,   229,    69,   218,
     316,   317,   318,    69,   315,     0,   225,   205,   228,   228,
      37,    69,   322,    69,   175,   175,    39,    62,    87,   311,
     220,    33,    58,    61,   151,   152,   158,   244,   245,   229,
      94,   260,   147,   181,   276,   277,   278,   175,   285,   265,
      69,    37,   254,     3,    49,    50,    51,    52,   108,   154,
     174,   179,   180,   271,   272,   273,   274,   225,    22,    63,
     295,   307,   228,   268,    69,   175,    30,    69,   300,   301,
     314,    73,   261,    30,    30,   261,   260,   281,    69,   217,
     253,   290,   322,   175,    45,    69,   187,   321,   248,    69,
     322,   304,    69,   317,    69,   205,   231,     5,    74,    76,
     175,   200,   310,   207,   208,   326,   327,   328,    69,   175,
     229,   277,   175,    34,    42,    46,    86,   119,   190,   255,
     256,   257,   175,   175,    69,   272,   322,   321,    69,    69,
     262,   261,   261,   262,   261,   175,    70,   146,   305,    41,
       9,    19,    60,    61,    79,   100,   109,   111,   127,   148,
     149,   187,   189,   197,   201,   202,   232,   233,   234,   235,
     236,   237,   238,   161,   327,   329,   330,   332,   205,   217,
     175,     4,    29,   114,   122,   141,   199,   203,   258,   216,
      44,    53,    62,    82,    85,    91,   116,   120,   121,   124,
     125,   126,   128,   129,   130,   132,   162,   197,   263,   262,
     262,   262,    30,   303,   229,    69,    69,    69,   191,   175,
     217,   205,   221,   330,   220,   322,    69,    57,   219,   228,
     209,   331,   217,   210,   333,   334,   322,   217,   221,   334,
     205,   322,   220,   211,   212,   213,   214,   215,   335,   336,
     337,   221,   336,   205,   217,   205,   322
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int16 yyr1[] =
{
       0,   222,   223,   224,   224,   224,   225,   225,   225,   225,
     225,   225,   225,   225,   225,   225,   225,   225,   225,   225,
     225,   226,   227,   227,   227,   227,   227,   228,   228,   229,
     230,   230,   231,   231,   232,   232,   232,   233,   234,   234,
     234,   234,   234,   234,   234,   234,   234,   235,   235,   236,
     236,   236,   236,   236,   236,   237,   238,   239,   240,   240,
     241,   241,   241,   241,   242,   242,   242,   242,   242,   242,
     242,   242,   242,   243,   243,   244,   244,   245,   245,   245,
     245,   245,   246,   247,   247,   248,   248,   248,   248,   249,
     249,   249,   249,   249,   249,   249,   249,   249,   250,   250,
     251,   251,   251,   252,   252,   253,   253,   253,   253,   253,
     253,   253,   253,   254,   254,   255,   255,   255,   255,   256,
     256,   257,   257,   258,   258,   258,   258,   258,   258,   258,
     259,   259,   259,   259,   259,   259,   259,   259,   259,   260,
     260,   261,   261,   262,   262,   262,   263,   263,   263,   263,
     263,   263,   263,   263,   263,   263,   263,   263,   263,   263,
     263,   263,   263,   264,   264,   265,   266,   266,   266,   267,
     267,   268,   269,   269,   269,   269,   269,   269,   269,   269,
     270,   271,   271,   272,   272,   272,   272,   272,   273,   273,
     273,   274,   274,   274,   274,   275,   276,   276,   277,   278,
     278,   279,   280,   280,   281,   282,   282,   282,   283,   283,
     284,   284,   285,   285,   286,   286,   286,   286,   286,   286,
     287,   287,   287,   287,   287,   287,   288,   289,   289,   290,
     291,   291,   291,   291,   291,   291,   291,   291,   291,   291,
     292,   292,   292,   292,   292,   292,   292,   292,   292,   292,
     292,   292,   292,   292,   292,   292,   293,   293,   293,   294,
     294,   295,   295,   295,   296,   297,   297,   297,   298,   298,
     298,   299,   299,   300,   301,   301,   302,   303,   303,   304,
     304,   305,   305,   306,   306,   307,   308,   308,   309,   309,
     310,   310,   310,   310,   311,   311,   311,   312,   313,   313,
     314,   314,   314,   314,   314,   314,   314,   315,   315,   316,
     316,   317,   317,   318,   319,   319,   320,   320,   321,   321,
     321,   322,   322,   323,   324,   325,   326,   326,   327,   328,
     328,   329,   329,   330,   331,   332,   333,   333,   334,   335,
     335,   336,   337,   337,   337,   337,   337
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
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
       2,     2,     5,     4,     5,     5,     4,     3,     3,     0,
       2,     0,     2,     0,     2,     3,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     1,     2,     1,     1,     1,     2,
       1,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       3,     2,     1,     2,     2,     2,     2,     2,     1,     1,
       1,     1,     1,     1,     1,     3,     2,     1,     2,     1,
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


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


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
    YYNOMEM;
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
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
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
#line 406 "../../ntpd/ntp_parser.y"
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
#line 2422 "ntp_parser.c"
    break;

  case 21: /* server_command: client_type address option_list  */
#line 443 "../../ntpd/ntp_parser.y"
                {
			peer_node *my_node;

			my_node = create_peer_node((yyvsp[-2].Integer), (yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.peers, my_node);
		}
#line 2433 "ntp_parser.c"
    break;

  case 28: /* address: address_fam T_String  */
#line 462 "../../ntpd/ntp_parser.y"
                        { (yyval.Address_node) = create_address_node((yyvsp[0].String), (yyvsp[-1].Integer)); }
#line 2439 "ntp_parser.c"
    break;

  case 29: /* ip_address: T_String  */
#line 467 "../../ntpd/ntp_parser.y"
                        { (yyval.Address_node) = create_address_node((yyvsp[0].String), AF_UNSPEC); }
#line 2445 "ntp_parser.c"
    break;

  case 30: /* address_fam: T_Ipv4_flag  */
#line 472 "../../ntpd/ntp_parser.y"
                        { (yyval.Integer) = AF_INET; }
#line 2451 "ntp_parser.c"
    break;

  case 31: /* address_fam: T_Ipv6_flag  */
#line 474 "../../ntpd/ntp_parser.y"
                        { (yyval.Integer) = AF_INET6; }
#line 2457 "ntp_parser.c"
    break;

  case 32: /* option_list: %empty  */
#line 479 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = NULL; }
#line 2463 "ntp_parser.c"
    break;

  case 33: /* option_list: option_list option  */
#line 481 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2472 "ntp_parser.c"
    break;

  case 37: /* option_flag: option_flag_keyword  */
#line 495 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2478 "ntp_parser.c"
    break;

  case 47: /* option_int: option_int_keyword T_Integer  */
#line 512 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2484 "ntp_parser.c"
    break;

  case 48: /* option_int: option_int_keyword T_U_int  */
#line 514 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_uval((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 2490 "ntp_parser.c"
    break;

  case 55: /* option_str: option_str_keyword T_String  */
#line 528 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2496 "ntp_parser.c"
    break;

  case 57: /* unpeer_command: unpeer_keyword address  */
#line 542 "../../ntpd/ntp_parser.y"
                {
			unpeer_node *my_node;

			my_node = create_unpeer_node((yyvsp[0].Address_node));
			if (my_node)
				APPEND_G_FIFO(cfgt.unpeers, my_node);
		}
#line 2508 "ntp_parser.c"
    break;

  case 60: /* other_mode_command: T_Broadcastclient  */
#line 563 "../../ntpd/ntp_parser.y"
                        { cfgt.broadcastclient = 1; }
#line 2514 "ntp_parser.c"
    break;

  case 61: /* other_mode_command: T_Manycastserver address_list  */
#line 565 "../../ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.manycastserver, (yyvsp[0].Address_fifo)); }
#line 2520 "ntp_parser.c"
    break;

  case 62: /* other_mode_command: T_Multicastclient address_list  */
#line 567 "../../ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.multicastclient, (yyvsp[0].Address_fifo)); }
#line 2526 "ntp_parser.c"
    break;

  case 63: /* other_mode_command: T_Mdnstries T_Integer  */
#line 569 "../../ntpd/ntp_parser.y"
                        { cfgt.mdnstries = (yyvsp[0].Integer); }
#line 2532 "ntp_parser.c"
    break;

  case 64: /* authentication_command: T_Automax T_Integer  */
#line 580 "../../ntpd/ntp_parser.y"
                {
			attr_val *atrv;

			atrv = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			APPEND_G_FIFO(cfgt.vars, atrv);
		}
#line 2543 "ntp_parser.c"
    break;

  case 65: /* authentication_command: T_ControlKey T_Integer  */
#line 587 "../../ntpd/ntp_parser.y"
                        { cfgt.auth.control_key = (yyvsp[0].Integer); }
#line 2549 "ntp_parser.c"
    break;

  case 66: /* authentication_command: T_Crypto crypto_command_list  */
#line 589 "../../ntpd/ntp_parser.y"
                {
			cfgt.auth.cryptosw++;
			CONCAT_G_FIFOS(cfgt.auth.crypto_cmd_list, (yyvsp[0].Attr_val_fifo));
		}
#line 2558 "ntp_parser.c"
    break;

  case 67: /* authentication_command: T_Keys T_String  */
#line 594 "../../ntpd/ntp_parser.y"
                        { cfgt.auth.keys = (yyvsp[0].String); }
#line 2564 "ntp_parser.c"
    break;

  case 68: /* authentication_command: T_Keysdir T_String  */
#line 596 "../../ntpd/ntp_parser.y"
                        { cfgt.auth.keysdir = (yyvsp[0].String); }
#line 2570 "ntp_parser.c"
    break;

  case 69: /* authentication_command: T_Requestkey T_Integer  */
#line 598 "../../ntpd/ntp_parser.y"
                        { cfgt.auth.request_key = (yyvsp[0].Integer); }
#line 2576 "ntp_parser.c"
    break;

  case 70: /* authentication_command: T_Revoke T_Integer  */
#line 600 "../../ntpd/ntp_parser.y"
                        { cfgt.auth.revoke = (yyvsp[0].Integer); }
#line 2582 "ntp_parser.c"
    break;

  case 71: /* authentication_command: T_Trustedkey integer_list_range  */
#line 602 "../../ntpd/ntp_parser.y"
                {
			/* [Bug 948] leaves it open if appending or
			 * replacing the trusted key list is the right
			 * way. In any case, either alternative should
			 * be coded correctly!
			 */
			DESTROY_G_FIFO(cfgt.auth.trusted_key_list, destroy_attr_val); /* remove for append */
			CONCAT_G_FIFOS(cfgt.auth.trusted_key_list, (yyvsp[0].Attr_val_fifo));
		}
#line 2596 "ntp_parser.c"
    break;

  case 72: /* authentication_command: T_NtpSignDsocket T_String  */
#line 612 "../../ntpd/ntp_parser.y"
                        { cfgt.auth.ntp_signd_socket = (yyvsp[0].String); }
#line 2602 "ntp_parser.c"
    break;

  case 73: /* crypto_command_list: %empty  */
#line 617 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = NULL; }
#line 2608 "ntp_parser.c"
    break;

  case 74: /* crypto_command_list: crypto_command_list crypto_command  */
#line 619 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2617 "ntp_parser.c"
    break;

  case 75: /* crypto_command: crypto_str_keyword T_String  */
#line 627 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 2623 "ntp_parser.c"
    break;

  case 76: /* crypto_command: T_Revoke T_Integer  */
#line 629 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val) = NULL;
			cfgt.auth.revoke = (yyvsp[0].Integer);
			msyslog(LOG_WARNING,
				"'crypto revoke %d' is deprecated, "
				"please use 'revoke %d' instead.",
				cfgt.auth.revoke, cfgt.auth.revoke);
		}
#line 2636 "ntp_parser.c"
    break;

  case 82: /* orphan_mode_command: T_Tos tos_option_list  */
#line 654 "../../ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.orphan_cmds, (yyvsp[0].Attr_val_fifo)); }
#line 2642 "ntp_parser.c"
    break;

  case 83: /* tos_option_list: tos_option_list tos_option  */
#line 659 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2651 "ntp_parser.c"
    break;

  case 84: /* tos_option_list: tos_option  */
#line 664 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2660 "ntp_parser.c"
    break;

  case 85: /* tos_option: tos_option_int_keyword T_Integer  */
#line 672 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2666 "ntp_parser.c"
    break;

  case 86: /* tos_option: tos_option_dbl_keyword number  */
#line 674 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 2672 "ntp_parser.c"
    break;

  case 87: /* tos_option: T_Cohort boolean  */
#line 676 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (double)(yyvsp[0].Integer)); }
#line 2678 "ntp_parser.c"
    break;

  case 88: /* tos_option: basedate  */
#line 678 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival(T_Basedate, (yyvsp[0].Integer)); }
#line 2684 "ntp_parser.c"
    break;

  case 100: /* monitoring_command: T_Statistics stats_list  */
#line 705 "../../ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.stats_list, (yyvsp[0].Int_fifo)); }
#line 2690 "ntp_parser.c"
    break;

  case 101: /* monitoring_command: T_Statsdir T_String  */
#line 707 "../../ntpd/ntp_parser.y"
                {
			if (lex_from_file()) {
				cfgt.stats_dir = (yyvsp[0].String);
			} else {
				YYFREE((yyvsp[0].String));
				yyerror("statsdir remote configuration ignored");
			}
		}
#line 2703 "ntp_parser.c"
    break;

  case 102: /* monitoring_command: T_Filegen stat filegen_option_list  */
#line 716 "../../ntpd/ntp_parser.y"
                {
			filegen_node *fgn;

			fgn = create_filegen_node((yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.filegen_opts, fgn);
		}
#line 2714 "ntp_parser.c"
    break;

  case 103: /* stats_list: stats_list stat  */
#line 726 "../../ntpd/ntp_parser.y"
                {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2723 "ntp_parser.c"
    break;

  case 104: /* stats_list: stat  */
#line 731 "../../ntpd/ntp_parser.y"
                {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 2732 "ntp_parser.c"
    break;

  case 113: /* filegen_option_list: %empty  */
#line 750 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = NULL; }
#line 2738 "ntp_parser.c"
    break;

  case 114: /* filegen_option_list: filegen_option_list filegen_option  */
#line 752 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 2747 "ntp_parser.c"
    break;

  case 115: /* filegen_option: T_File T_String  */
#line 760 "../../ntpd/ntp_parser.y"
                {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			} else {
				(yyval.Attr_val) = NULL;
				YYFREE((yyvsp[0].String));
				yyerror("filegen file remote config ignored");
			}
		}
#line 2761 "ntp_parser.c"
    break;

  case 116: /* filegen_option: T_Type filegen_type  */
#line 770 "../../ntpd/ntp_parser.y"
                {
			if (lex_from_file()) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror("filegen type remote config ignored");
			}
		}
#line 2774 "ntp_parser.c"
    break;

  case 117: /* filegen_option: link_nolink  */
#line 779 "../../ntpd/ntp_parser.y"
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
#line 2793 "ntp_parser.c"
    break;

  case 118: /* filegen_option: enable_disable  */
#line 794 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 2799 "ntp_parser.c"
    break;

  case 130: /* access_control_command: T_Discard discard_option_list  */
#line 824 "../../ntpd/ntp_parser.y"
                {
			CONCAT_G_FIFOS(cfgt.discard_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2807 "ntp_parser.c"
    break;

  case 131: /* access_control_command: T_Mru mru_option_list  */
#line 828 "../../ntpd/ntp_parser.y"
                {
			CONCAT_G_FIFOS(cfgt.mru_opts, (yyvsp[0].Attr_val_fifo));
		}
#line 2815 "ntp_parser.c"
    break;

  case 132: /* access_control_command: T_Restrict address restrict_mask res_ippeerlimit ac_flag_list  */
#line 832 "../../ntpd/ntp_parser.y"
                {
			restrict_node *rn;

			rn = create_restrict_node((yyvsp[-3].Address_node), (yyvsp[-2].Address_node), (yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo), FALSE,
						  lex_current()->curpos.nline,
						  lex_current()->curpos.ncol);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2828 "ntp_parser.c"
    break;

  case 133: /* access_control_command: T_Restrict T_Default res_ippeerlimit ac_flag_list  */
#line 841 "../../ntpd/ntp_parser.y"
                {
			restrict_node *rn;

			rn = create_restrict_node(NULL, NULL, (yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo), FALSE,
						  lex_current()->curpos.nline,
						  lex_current()->curpos.ncol);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2841 "ntp_parser.c"
    break;

  case 134: /* access_control_command: T_Restrict T_Ipv4_flag T_Default res_ippeerlimit ac_flag_list  */
#line 850 "../../ntpd/ntp_parser.y"
                {
			restrict_node *rn;

			rn = create_restrict_node(
				create_address_node(
					estrdup("0.0.0.0"),
					AF_INET),
				create_address_node(
					estrdup("0.0.0.0"),
					AF_INET),
				(yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo), FALSE,
				lex_current()->curpos.nline,
				lex_current()->curpos.ncol);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2861 "ntp_parser.c"
    break;

  case 135: /* access_control_command: T_Restrict T_Ipv6_flag T_Default res_ippeerlimit ac_flag_list  */
#line 866 "../../ntpd/ntp_parser.y"
                {
			restrict_node *rn;

			rn = create_restrict_node(
				create_address_node(
					estrdup("::"),
					AF_INET6),
				create_address_node(
					estrdup("::"),
					AF_INET6),
				(yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo), FALSE,
				lex_current()->curpos.nline,
				lex_current()->curpos.ncol);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2881 "ntp_parser.c"
    break;

  case 136: /* access_control_command: T_Restrict T_Source res_ippeerlimit ac_flag_list  */
#line 882 "../../ntpd/ntp_parser.y"
                {
			restrict_node *	rn;

			APPEND_G_FIFO((yyvsp[0].Attr_val_fifo), create_attr_ival((yyvsp[-2].Integer), 1));
			rn = create_restrict_node(NULL, NULL, (yyvsp[-1].Integer), (yyvsp[0].Attr_val_fifo), FALSE,
						  lex_current()->curpos.nline,
						  lex_current()->curpos.ncol);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2895 "ntp_parser.c"
    break;

  case 137: /* access_control_command: T_Delrestrict ip_address restrict_mask  */
#line 892 "../../ntpd/ntp_parser.y"
                {
			restrict_node *	rn;

			rn = create_restrict_node((yyvsp[-1].Address_node), (yyvsp[0].Address_node), -1, NULL, TRUE,
						  lex_current()->curpos.nline,
						  lex_current()->curpos.ncol);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2908 "ntp_parser.c"
    break;

  case 138: /* access_control_command: T_Delrestrict T_Source ip_address  */
#line 901 "../../ntpd/ntp_parser.y"
                {
			restrict_node *	rn;
			attr_val_fifo * avf;

			avf = NULL;
			APPEND_G_FIFO(avf, create_attr_ival((yyvsp[-1].Integer), 1));
			rn = create_restrict_node((yyvsp[0].Address_node), NULL, -1, avf, TRUE,
						  lex_current()->curpos.nline,
						  lex_current()->curpos.ncol);
			APPEND_G_FIFO(cfgt.restrict_opts, rn);
		}
#line 2924 "ntp_parser.c"
    break;

  case 139: /* restrict_mask: %empty  */
#line 916 "../../ntpd/ntp_parser.y"
                        { (yyval.Address_node) = NULL; }
#line 2930 "ntp_parser.c"
    break;

  case 140: /* restrict_mask: T_Mask ip_address  */
#line 918 "../../ntpd/ntp_parser.y"
                {
			(yyval.Address_node) = (yyvsp[0].Address_node);
		}
#line 2938 "ntp_parser.c"
    break;

  case 141: /* res_ippeerlimit: %empty  */
#line 925 "../../ntpd/ntp_parser.y"
                        { (yyval.Integer) = -1; }
#line 2944 "ntp_parser.c"
    break;

  case 142: /* res_ippeerlimit: T_Ippeerlimit T_Integer  */
#line 927 "../../ntpd/ntp_parser.y"
                {
			if (((yyvsp[0].Integer) < -1) || ((yyvsp[0].Integer) > 100)) {
				struct FILE_INFO * ip_ctx;

				ip_ctx = lex_current();
				msyslog(LOG_ERR,
					"Unreasonable ippeerlimit value (%d) in %s line %d, column %d.  Using 0.",
					(yyvsp[0].Integer),
					ip_ctx->fname,
					ip_ctx->curpos.nline,
					ip_ctx->curpos.ncol);
				(yyvsp[0].Integer) = 0;
			}
			(yyval.Integer) = (yyvsp[0].Integer);
		}
#line 2964 "ntp_parser.c"
    break;

  case 143: /* ac_flag_list: %empty  */
#line 946 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = NULL; }
#line 2970 "ntp_parser.c"
    break;

  case 144: /* ac_flag_list: ac_flag_list access_control_flag  */
#line 948 "../../ntpd/ntp_parser.y"
                {
			attr_val *av;

			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			av = create_attr_ival((yyvsp[0].Integer), 1);
			APPEND_G_FIFO((yyval.Attr_val_fifo), av);
		}
#line 2982 "ntp_parser.c"
    break;

  case 145: /* ac_flag_list: ac_flag_list T_Serverresponse T_Fuzz  */
#line 956 "../../ntpd/ntp_parser.y"
                {
			attr_val *av;

			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			av = create_attr_ival(T_ServerresponseFuzz, 1);
			APPEND_G_FIFO((yyval.Attr_val_fifo), av);
		}
#line 2994 "ntp_parser.c"
    break;

  case 163: /* discard_option_list: discard_option_list discard_option  */
#line 987 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3003 "ntp_parser.c"
    break;

  case 164: /* discard_option_list: discard_option  */
#line 992 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3012 "ntp_parser.c"
    break;

  case 165: /* discard_option: discard_option_keyword T_Integer  */
#line 1000 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 3018 "ntp_parser.c"
    break;

  case 169: /* mru_option_list: mru_option_list mru_option  */
#line 1011 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3027 "ntp_parser.c"
    break;

  case 170: /* mru_option_list: mru_option  */
#line 1016 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3036 "ntp_parser.c"
    break;

  case 171: /* mru_option: mru_option_keyword T_Integer  */
#line 1024 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 3042 "ntp_parser.c"
    break;

  case 180: /* fudge_command: T_Fudge address fudge_factor_list  */
#line 1044 "../../ntpd/ntp_parser.y"
                {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.fudge, aon);
		}
#line 3053 "ntp_parser.c"
    break;

  case 181: /* fudge_factor_list: fudge_factor_list fudge_factor  */
#line 1054 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3062 "ntp_parser.c"
    break;

  case 182: /* fudge_factor_list: fudge_factor  */
#line 1059 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3071 "ntp_parser.c"
    break;

  case 183: /* fudge_factor: fudge_factor_dbl_keyword number  */
#line 1067 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 3077 "ntp_parser.c"
    break;

  case 184: /* fudge_factor: fudge_factor_bool_keyword boolean  */
#line 1069 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 3083 "ntp_parser.c"
    break;

  case 185: /* fudge_factor: T_Stratum T_Integer  */
#line 1071 "../../ntpd/ntp_parser.y"
                {
			if ((yyvsp[0].Integer) >= 0 && (yyvsp[0].Integer) <= 16) {
				(yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			} else {
				(yyval.Attr_val) = NULL;
				yyerror("fudge factor: stratum value not in [0..16], ignored");
			}
		}
#line 3096 "ntp_parser.c"
    break;

  case 186: /* fudge_factor: T_Abbrev T_String  */
#line 1080 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 3102 "ntp_parser.c"
    break;

  case 187: /* fudge_factor: T_Refid T_String  */
#line 1082 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 3108 "ntp_parser.c"
    break;

  case 195: /* device_command: T_Device address device_item_list  */
#line 1104 "../../ntpd/ntp_parser.y"
                {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.device, aon);
		}
#line 3119 "ntp_parser.c"
    break;

  case 196: /* device_item_list: device_item_list device_item  */
#line 1114 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3128 "ntp_parser.c"
    break;

  case 197: /* device_item_list: device_item  */
#line 1119 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3137 "ntp_parser.c"
    break;

  case 198: /* device_item: device_item_path_keyword T_String  */
#line 1127 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String)); }
#line 3143 "ntp_parser.c"
    break;

  case 201: /* rlimit_command: T_Rlimit rlimit_option_list  */
#line 1141 "../../ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.rlimit, (yyvsp[0].Attr_val_fifo)); }
#line 3149 "ntp_parser.c"
    break;

  case 202: /* rlimit_option_list: rlimit_option_list rlimit_option  */
#line 1146 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3158 "ntp_parser.c"
    break;

  case 203: /* rlimit_option_list: rlimit_option  */
#line 1151 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3167 "ntp_parser.c"
    break;

  case 204: /* rlimit_option: rlimit_option_keyword T_Integer  */
#line 1159 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 3173 "ntp_parser.c"
    break;

  case 208: /* system_option_command: T_Enable system_option_list  */
#line 1175 "../../ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.enable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 3179 "ntp_parser.c"
    break;

  case 209: /* system_option_command: T_Disable system_option_list  */
#line 1177 "../../ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.disable_opts, (yyvsp[0].Attr_val_fifo)); }
#line 3185 "ntp_parser.c"
    break;

  case 210: /* system_option_list: system_option_list system_option  */
#line 1182 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3194 "ntp_parser.c"
    break;

  case 211: /* system_option_list: system_option  */
#line 1187 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3203 "ntp_parser.c"
    break;

  case 212: /* system_option: system_option_flag_keyword  */
#line 1195 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival(T_Flag, (yyvsp[0].Integer)); }
#line 3209 "ntp_parser.c"
    break;

  case 213: /* system_option: system_option_local_flag_keyword  */
#line 1197 "../../ntpd/ntp_parser.y"
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
#line 3227 "ntp_parser.c"
    break;

  case 226: /* tinker_command: T_Tinker tinker_option_list  */
#line 1236 "../../ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.tinker, (yyvsp[0].Attr_val_fifo)); }
#line 3233 "ntp_parser.c"
    break;

  case 227: /* tinker_option_list: tinker_option_list tinker_option  */
#line 1241 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3242 "ntp_parser.c"
    break;

  case 228: /* tinker_option_list: tinker_option  */
#line 1246 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3251 "ntp_parser.c"
    break;

  case 229: /* tinker_option: tinker_option_keyword number  */
#line 1254 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double)); }
#line 3257 "ntp_parser.c"
    break;

  case 242: /* miscellaneous_command: misc_cmd_dbl_keyword number  */
#line 1279 "../../ntpd/ntp_parser.y"
                {
			attr_val *av;

			av = create_attr_dval((yyvsp[-1].Integer), (yyvsp[0].Double));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 3268 "ntp_parser.c"
    break;

  case 243: /* miscellaneous_command: misc_cmd_int_keyword T_Integer  */
#line 1286 "../../ntpd/ntp_parser.y"
                {
			attr_val *av;

			av = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 3279 "ntp_parser.c"
    break;

  case 244: /* miscellaneous_command: misc_cmd_str_keyword T_String  */
#line 1293 "../../ntpd/ntp_parser.y"
                {
			attr_val *av;

			av = create_attr_sval((yyvsp[-1].Integer), (yyvsp[0].String));
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 3290 "ntp_parser.c"
    break;

  case 245: /* miscellaneous_command: misc_cmd_str_lcl_keyword T_String  */
#line 1300 "../../ntpd/ntp_parser.y"
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
#line 3310 "ntp_parser.c"
    break;

  case 246: /* miscellaneous_command: T_Includefile T_String command  */
#line 1316 "../../ntpd/ntp_parser.y"
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
#line 3333 "ntp_parser.c"
    break;

  case 247: /* miscellaneous_command: T_Leapfile T_String opt_hash_check  */
#line 1335 "../../ntpd/ntp_parser.y"
                {
			attr_val *av;

			av = create_attr_sval((yyvsp[-2].Integer), (yyvsp[-1].String));
			av->flag = (yyvsp[0].Integer);
			APPEND_G_FIFO(cfgt.vars, av);
		}
#line 3345 "ntp_parser.c"
    break;

  case 248: /* miscellaneous_command: T_End  */
#line 1343 "../../ntpd/ntp_parser.y"
                        { lex_flush_stack(); }
#line 3351 "ntp_parser.c"
    break;

  case 249: /* miscellaneous_command: T_Driftfile drift_parm  */
#line 1345 "../../ntpd/ntp_parser.y"
                        { /* see drift_parm below for actions */ }
#line 3357 "ntp_parser.c"
    break;

  case 250: /* miscellaneous_command: T_Logconfig log_config_list  */
#line 1347 "../../ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.logconfig, (yyvsp[0].Attr_val_fifo)); }
#line 3363 "ntp_parser.c"
    break;

  case 251: /* miscellaneous_command: T_Phone string_list  */
#line 1349 "../../ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.phone, (yyvsp[0].String_fifo)); }
#line 3369 "ntp_parser.c"
    break;

  case 252: /* miscellaneous_command: T_PollSkewList pollskew_list  */
#line 1351 "../../ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.pollskewlist, (yyvsp[0].Attr_val_fifo)); }
#line 3375 "ntp_parser.c"
    break;

  case 253: /* miscellaneous_command: T_Setvar variable_assign  */
#line 1353 "../../ntpd/ntp_parser.y"
                        { APPEND_G_FIFO(cfgt.setvar, (yyvsp[0].Set_var)); }
#line 3381 "ntp_parser.c"
    break;

  case 254: /* miscellaneous_command: T_Trap ip_address trap_option_list  */
#line 1355 "../../ntpd/ntp_parser.y"
                {
			addr_opts_node *aon;

			aon = create_addr_opts_node((yyvsp[-1].Address_node), (yyvsp[0].Attr_val_fifo));
			APPEND_G_FIFO(cfgt.trap, aon);
		}
#line 3392 "ntp_parser.c"
    break;

  case 255: /* miscellaneous_command: T_Ttl integer_list  */
#line 1362 "../../ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.ttl, (yyvsp[0].Attr_val_fifo)); }
#line 3398 "ntp_parser.c"
    break;

  case 260: /* misc_cmd_int_keyword: T_Leapsmearinterval  */
#line 1377 "../../ntpd/ntp_parser.y"
                {
#ifndef LEAP_SMEAR
			yyerror("Built without LEAP_SMEAR support.");
#endif
		}
#line 3408 "ntp_parser.c"
    break;

  case 261: /* opt_hash_check: T_Ignorehash  */
#line 1386 "../../ntpd/ntp_parser.y"
                        { (yyval.Integer) = FALSE; }
#line 3414 "ntp_parser.c"
    break;

  case 262: /* opt_hash_check: T_Checkhash  */
#line 1388 "../../ntpd/ntp_parser.y"
                        { (yyval.Integer) = TRUE; }
#line 3420 "ntp_parser.c"
    break;

  case 263: /* opt_hash_check: %empty  */
#line 1390 "../../ntpd/ntp_parser.y"
                        {  (yyval.Integer) = TRUE; }
#line 3426 "ntp_parser.c"
    break;

  case 268: /* drift_parm: T_String  */
#line 1405 "../../ntpd/ntp_parser.y"
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
#line 3441 "ntp_parser.c"
    break;

  case 269: /* drift_parm: T_String T_Double  */
#line 1416 "../../ntpd/ntp_parser.y"
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
#line 3462 "ntp_parser.c"
    break;

  case 270: /* drift_parm: %empty  */
#line 1433 "../../ntpd/ntp_parser.y"
                {
			if (lex_from_file()) {
				attr_val *av;
				av = create_attr_sval(T_Driftfile, estrdup(""));
				APPEND_G_FIFO(cfgt.vars, av);
			} else {
				yyerror("driftfile remote configuration ignored");
			}
		}
#line 3476 "ntp_parser.c"
    break;

  case 271: /* pollskew_list: %empty  */
#line 1446 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = NULL; }
#line 3482 "ntp_parser.c"
    break;

  case 272: /* pollskew_list: pollskew_list pollskew_spec  */
#line 1448 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = append_gen_fifo((yyvsp[-1].Attr_val_fifo), (yyvsp[0].Attr_val)); }
#line 3488 "ntp_parser.c"
    break;

  case 273: /* pollskew_spec: pollskew_cycle T_Integer '|' T_Integer  */
#line 1453 "../../ntpd/ntp_parser.y"
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
#line 3515 "ntp_parser.c"
    break;

  case 274: /* pollskew_cycle: T_Integer  */
#line 1479 "../../ntpd/ntp_parser.y"
                { 
			(yyval.Attr_val) = ((yyvsp[0].Integer) >= NTP_MINPOLL && (yyvsp[0].Integer) <= NTP_MAXPOLL) 
				? create_attr_rval((yyvsp[0].Integer), 0, 0) 
				: NULL;
		}
#line 3525 "ntp_parser.c"
    break;

  case 275: /* pollskew_cycle: T_Default  */
#line 1484 "../../ntpd/ntp_parser.y"
                          { (yyval.Attr_val) = create_attr_rval(-1, 0, 0); }
#line 3531 "ntp_parser.c"
    break;

  case 276: /* variable_assign: T_String '=' T_String t_default_or_zero  */
#line 1490 "../../ntpd/ntp_parser.y"
                        { (yyval.Set_var) = create_setvar_node((yyvsp[-3].String), (yyvsp[-1].String), (yyvsp[0].Integer)); }
#line 3537 "ntp_parser.c"
    break;

  case 278: /* t_default_or_zero: %empty  */
#line 1496 "../../ntpd/ntp_parser.y"
                        { (yyval.Integer) = 0; }
#line 3543 "ntp_parser.c"
    break;

  case 279: /* trap_option_list: %empty  */
#line 1501 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val_fifo) = NULL; }
#line 3549 "ntp_parser.c"
    break;

  case 280: /* trap_option_list: trap_option_list trap_option  */
#line 1503 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3558 "ntp_parser.c"
    break;

  case 281: /* trap_option: T_Port T_Integer  */
#line 1511 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival((yyvsp[-1].Integer), (yyvsp[0].Integer)); }
#line 3564 "ntp_parser.c"
    break;

  case 282: /* trap_option: T_Interface ip_address  */
#line 1513 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val) = create_attr_sval((yyvsp[-1].Integer), estrdup((yyvsp[0].Address_node)->address));
			destroy_address_node((yyvsp[0].Address_node));
		}
#line 3573 "ntp_parser.c"
    break;

  case 283: /* log_config_list: log_config_list log_config_command  */
#line 1521 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3582 "ntp_parser.c"
    break;

  case 284: /* log_config_list: log_config_command  */
#line 1526 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3591 "ntp_parser.c"
    break;

  case 285: /* log_config_command: T_String  */
#line 1534 "../../ntpd/ntp_parser.y"
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
#line 3617 "ntp_parser.c"
    break;

  case 286: /* interface_command: interface_nic nic_rule_action nic_rule_class  */
#line 1559 "../../ntpd/ntp_parser.y"
                {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node((yyvsp[0].Integer), NULL, (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3628 "ntp_parser.c"
    break;

  case 287: /* interface_command: interface_nic nic_rule_action T_String  */
#line 1566 "../../ntpd/ntp_parser.y"
                {
			nic_rule_node *nrn;

			nrn = create_nic_rule_node(0, (yyvsp[0].String), (yyvsp[-1].Integer));
			APPEND_G_FIFO(cfgt.nic_rules, nrn);
		}
#line 3639 "ntp_parser.c"
    break;

  case 297: /* reset_command: T_Reset counter_set_list  */
#line 1594 "../../ntpd/ntp_parser.y"
                        { CONCAT_G_FIFOS(cfgt.reset_counters, (yyvsp[0].Int_fifo)); }
#line 3645 "ntp_parser.c"
    break;

  case 298: /* counter_set_list: counter_set_list counter_set_keyword  */
#line 1599 "../../ntpd/ntp_parser.y"
                {
			(yyval.Int_fifo) = (yyvsp[-1].Int_fifo);
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3654 "ntp_parser.c"
    break;

  case 299: /* counter_set_list: counter_set_keyword  */
#line 1604 "../../ntpd/ntp_parser.y"
                {
			(yyval.Int_fifo) = NULL;
			APPEND_G_FIFO((yyval.Int_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3663 "ntp_parser.c"
    break;

  case 307: /* integer_list: integer_list T_Integer  */
#line 1628 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3672 "ntp_parser.c"
    break;

  case 308: /* integer_list: T_Integer  */
#line 1633 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), create_int_node((yyvsp[0].Integer)));
		}
#line 3681 "ntp_parser.c"
    break;

  case 309: /* integer_list_range: integer_list_range integer_list_range_elt  */
#line 1641 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-1].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3690 "ntp_parser.c"
    break;

  case 310: /* integer_list_range: integer_list_range_elt  */
#line 1646 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[0].Attr_val));
		}
#line 3699 "ntp_parser.c"
    break;

  case 311: /* integer_list_range_elt: T_Integer  */
#line 1654 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_ival('i', (yyvsp[0].Integer)); }
#line 3705 "ntp_parser.c"
    break;

  case 313: /* integer_range: '(' T_Integer T_Ellipsis T_Integer ')'  */
#line 1660 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_rval('-', (yyvsp[-3].Integer), (yyvsp[-1].Integer)); }
#line 3711 "ntp_parser.c"
    break;

  case 314: /* string_list: string_list T_String  */
#line 1665 "../../ntpd/ntp_parser.y"
                {
			(yyval.String_fifo) = (yyvsp[-1].String_fifo);
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3720 "ntp_parser.c"
    break;

  case 315: /* string_list: T_String  */
#line 1670 "../../ntpd/ntp_parser.y"
                {
			(yyval.String_fifo) = NULL;
			APPEND_G_FIFO((yyval.String_fifo), create_string_node((yyvsp[0].String)));
		}
#line 3729 "ntp_parser.c"
    break;

  case 316: /* address_list: address_list address  */
#line 1678 "../../ntpd/ntp_parser.y"
                {
			(yyval.Address_fifo) = (yyvsp[-1].Address_fifo);
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3738 "ntp_parser.c"
    break;

  case 317: /* address_list: address  */
#line 1683 "../../ntpd/ntp_parser.y"
                {
			(yyval.Address_fifo) = NULL;
			APPEND_G_FIFO((yyval.Address_fifo), (yyvsp[0].Address_node));
		}
#line 3747 "ntp_parser.c"
    break;

  case 318: /* boolean: T_Integer  */
#line 1691 "../../ntpd/ntp_parser.y"
                {
			if ((yyvsp[0].Integer) != 0 && (yyvsp[0].Integer) != 1) {
				yyerror("Integer value is not boolean (0 or 1). Assuming 1");
				(yyval.Integer) = 1;
			} else {
				(yyval.Integer) = (yyvsp[0].Integer);
			}
		}
#line 3760 "ntp_parser.c"
    break;

  case 319: /* boolean: T_True  */
#line 1699 "../../ntpd/ntp_parser.y"
                        { (yyval.Integer) = 1; }
#line 3766 "ntp_parser.c"
    break;

  case 320: /* boolean: T_False  */
#line 1700 "../../ntpd/ntp_parser.y"
                        { (yyval.Integer) = 0; }
#line 3772 "ntp_parser.c"
    break;

  case 321: /* number: T_Integer  */
#line 1704 "../../ntpd/ntp_parser.y"
                                { (yyval.Double) = (double)(yyvsp[0].Integer); }
#line 3778 "ntp_parser.c"
    break;

  case 323: /* basedate: T_Basedate T_String  */
#line 1710 "../../ntpd/ntp_parser.y"
                        { (yyval.Integer) = basedate_eval_string((yyvsp[0].String)); YYFREE((yyvsp[0].String)); }
#line 3784 "ntp_parser.c"
    break;

  case 324: /* simulate_command: sim_conf_start '{' sim_init_statement_list sim_server_list '}'  */
#line 1718 "../../ntpd/ntp_parser.y"
                {
			sim_node *sn;

			sn =  create_sim_node((yyvsp[-2].Attr_val_fifo), (yyvsp[-1].Sim_server_fifo));
			APPEND_G_FIFO(cfgt.sim_details, sn);

			/* Revert from ; to \n for end-of-command */
			old_config_style = 1;
		}
#line 3798 "ntp_parser.c"
    break;

  case 325: /* sim_conf_start: T_Simulate  */
#line 1735 "../../ntpd/ntp_parser.y"
                           { old_config_style = 0; }
#line 3804 "ntp_parser.c"
    break;

  case 326: /* sim_init_statement_list: sim_init_statement_list sim_init_statement T_EOC  */
#line 1740 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3813 "ntp_parser.c"
    break;

  case 327: /* sim_init_statement_list: sim_init_statement T_EOC  */
#line 1745 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3822 "ntp_parser.c"
    break;

  case 328: /* sim_init_statement: sim_init_keyword '=' number  */
#line 1753 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3828 "ntp_parser.c"
    break;

  case 331: /* sim_server_list: sim_server_list sim_server  */
#line 1763 "../../ntpd/ntp_parser.y"
                {
			(yyval.Sim_server_fifo) = (yyvsp[-1].Sim_server_fifo);
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3837 "ntp_parser.c"
    break;

  case 332: /* sim_server_list: sim_server  */
#line 1768 "../../ntpd/ntp_parser.y"
                {
			(yyval.Sim_server_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_server_fifo), (yyvsp[0].Sim_server));
		}
#line 3846 "ntp_parser.c"
    break;

  case 333: /* sim_server: sim_server_name '{' sim_server_offset sim_act_list '}'  */
#line 1776 "../../ntpd/ntp_parser.y"
                        { (yyval.Sim_server) = ONLY_SIM(create_sim_server((yyvsp[-4].Address_node), (yyvsp[-2].Double), (yyvsp[-1].Sim_script_fifo))); }
#line 3852 "ntp_parser.c"
    break;

  case 334: /* sim_server_offset: T_Server_Offset '=' number T_EOC  */
#line 1781 "../../ntpd/ntp_parser.y"
                        { (yyval.Double) = (yyvsp[-1].Double); }
#line 3858 "ntp_parser.c"
    break;

  case 335: /* sim_server_name: T_Server '=' address  */
#line 1786 "../../ntpd/ntp_parser.y"
                        { (yyval.Address_node) = (yyvsp[0].Address_node); }
#line 3864 "ntp_parser.c"
    break;

  case 336: /* sim_act_list: sim_act_list sim_act  */
#line 1791 "../../ntpd/ntp_parser.y"
                {
			(yyval.Sim_script_fifo) = (yyvsp[-1].Sim_script_fifo);
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3873 "ntp_parser.c"
    break;

  case 337: /* sim_act_list: sim_act  */
#line 1796 "../../ntpd/ntp_parser.y"
                {
			(yyval.Sim_script_fifo) = NULL;
			APPEND_G_FIFO((yyval.Sim_script_fifo), (yyvsp[0].Sim_script));
		}
#line 3882 "ntp_parser.c"
    break;

  case 338: /* sim_act: T_Duration '=' number '{' sim_act_stmt_list '}'  */
#line 1804 "../../ntpd/ntp_parser.y"
                        { (yyval.Sim_script) = ONLY_SIM(create_sim_script_info((yyvsp[-3].Double), (yyvsp[-1].Attr_val_fifo))); }
#line 3888 "ntp_parser.c"
    break;

  case 339: /* sim_act_stmt_list: sim_act_stmt_list sim_act_stmt T_EOC  */
#line 1809 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = (yyvsp[-2].Attr_val_fifo);
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3897 "ntp_parser.c"
    break;

  case 340: /* sim_act_stmt_list: sim_act_stmt T_EOC  */
#line 1814 "../../ntpd/ntp_parser.y"
                {
			(yyval.Attr_val_fifo) = NULL;
			APPEND_G_FIFO((yyval.Attr_val_fifo), (yyvsp[-1].Attr_val));
		}
#line 3906 "ntp_parser.c"
    break;

  case 341: /* sim_act_stmt: sim_act_keyword '=' number  */
#line 1822 "../../ntpd/ntp_parser.y"
                        { (yyval.Attr_val) = create_attr_dval((yyvsp[-2].Integer), (yyvsp[0].Double)); }
#line 3912 "ntp_parser.c"
    break;


#line 3916 "ntp_parser.c"

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
  ++yynerrs;

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
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
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

#line 1833 "../../ntpd/ntp_parser.y"


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

