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
#line 51 "../../ntpd/ntp_parser.y" /* yacc.c:1909  */

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

#line 469 "ntp_parser.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_Y_TAB_H_INCLUDED  */
