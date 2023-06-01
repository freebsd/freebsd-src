/* A Bison parser, made by GNU Bison 3.7.6.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

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

#line 514 "../../../src/ntp-stable-3758/ntpd/ntp_parser.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY__SRC_NTP_STABLE_NTPD_NTP_PARSER_H_INCLUDED  */
