/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison interface for Yacc-like parsers in C

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
    T_Panic = 380,
    T_Peer = 381,
    T_Peerstats = 382,
    T_Phone = 383,
    T_Pid = 384,
    T_Pidfile = 385,
    T_Pool = 386,
    T_Port = 387,
    T_Preempt = 388,
    T_Prefer = 389,
    T_Protostats = 390,
    T_Pw = 391,
    T_Randfile = 392,
    T_Rawstats = 393,
    T_Refid = 394,
    T_Requestkey = 395,
    T_Reset = 396,
    T_Restrict = 397,
    T_Revoke = 398,
    T_Rlimit = 399,
    T_Saveconfigdir = 400,
    T_Server = 401,
    T_Setvar = 402,
    T_Source = 403,
    T_Stacksize = 404,
    T_Statistics = 405,
    T_Stats = 406,
    T_Statsdir = 407,
    T_Step = 408,
    T_Stepback = 409,
    T_Stepfwd = 410,
    T_Stepout = 411,
    T_Stratum = 412,
    T_String = 413,
    T_Sys = 414,
    T_Sysstats = 415,
    T_Tick = 416,
    T_Time1 = 417,
    T_Time2 = 418,
    T_Timer = 419,
    T_Timingstats = 420,
    T_Tinker = 421,
    T_Tos = 422,
    T_Trap = 423,
    T_True = 424,
    T_Trustedkey = 425,
    T_Ttl = 426,
    T_Type = 427,
    T_U_int = 428,
    T_Unconfig = 429,
    T_Unpeer = 430,
    T_Version = 431,
    T_WanderThreshold = 432,
    T_Week = 433,
    T_Wildcard = 434,
    T_Xleave = 435,
    T_Year = 436,
    T_Flag = 437,
    T_EOC = 438,
    T_Simulate = 439,
    T_Beep_Delay = 440,
    T_Sim_Duration = 441,
    T_Server_Offset = 442,
    T_Duration = 443,
    T_Freq_Offset = 444,
    T_Wander = 445,
    T_Jitter = 446,
    T_Prop_Delay = 447,
    T_Proc_Delay = 448
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
#define T_Panic 380
#define T_Peer 381
#define T_Peerstats 382
#define T_Phone 383
#define T_Pid 384
#define T_Pidfile 385
#define T_Pool 386
#define T_Port 387
#define T_Preempt 388
#define T_Prefer 389
#define T_Protostats 390
#define T_Pw 391
#define T_Randfile 392
#define T_Rawstats 393
#define T_Refid 394
#define T_Requestkey 395
#define T_Reset 396
#define T_Restrict 397
#define T_Revoke 398
#define T_Rlimit 399
#define T_Saveconfigdir 400
#define T_Server 401
#define T_Setvar 402
#define T_Source 403
#define T_Stacksize 404
#define T_Statistics 405
#define T_Stats 406
#define T_Statsdir 407
#define T_Step 408
#define T_Stepback 409
#define T_Stepfwd 410
#define T_Stepout 411
#define T_Stratum 412
#define T_String 413
#define T_Sys 414
#define T_Sysstats 415
#define T_Tick 416
#define T_Time1 417
#define T_Time2 418
#define T_Timer 419
#define T_Timingstats 420
#define T_Tinker 421
#define T_Tos 422
#define T_Trap 423
#define T_True 424
#define T_Trustedkey 425
#define T_Ttl 426
#define T_Type 427
#define T_U_int 428
#define T_Unconfig 429
#define T_Unpeer 430
#define T_Version 431
#define T_WanderThreshold 432
#define T_Week 433
#define T_Wildcard 434
#define T_Xleave 435
#define T_Year 436
#define T_Flag 437
#define T_EOC 438
#define T_Simulate 439
#define T_Beep_Delay 440
#define T_Sim_Duration 441
#define T_Server_Offset 442
#define T_Duration 443
#define T_Freq_Offset 444
#define T_Wander 445
#define T_Jitter 446
#define T_Prop_Delay 447
#define T_Proc_Delay 448

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
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

#line 459 "../../ntpd/ntp_parser.h" /* yacc.c:1909  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY__NTPD_NTP_PARSER_H_INCLUDED  */
