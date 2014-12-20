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

#ifndef YY_YY_______NTPD_NTP_PARSER_H_INCLUDED
# define YY_YY_______NTPD_NTP_PARSER_H_INCLUDED
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
#line 54 "ntp_parser.y" /* yacc.c:1909  */

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

#line 449 "../../ntpd/ntp_parser.h" /* yacc.c:1909  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (struct FILE_INFO *ip_file);

#endif /* !YY_YY_______NTPD_NTP_PARSER_H_INCLUDED  */
