/* A Bison parser, made by GNU Bison 3.8.2.  */

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

#line 516 "ntp_parser.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_NTP_PARSER_H_INCLUDED  */
