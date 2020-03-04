/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

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
#line 52 "../../ntpd/ntp_parser.y" /* yacc.c:1909  */

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

#line 495 "../../ntpd/ntp_parser.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY__NTPD_NTP_PARSER_H_INCLUDED  */
