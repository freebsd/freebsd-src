#ifndef lint
static char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93 (BSDI)";
#endif
#include <stdlib.h>
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYEMPTY (-1)
#define YYLEX yylex()
#define yyclearin (yychar=YYEMPTY)
#define yyerrok (yyerrflag=0)
#define YYRECOVERING (yyerrflag!=0)
#define YYPREFIX "yy"
#line 2 "ns_parser.y"
#if !defined(lint) && !defined(SABER)
static char rcsid[] = "$Id: ns_parser.y,v 8.51 1999/11/12 05:29:18 vixie Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* Global C stuff goes here. */

#include "port_before.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <limits.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>

#include <isc/dst.h>

#include "port_after.h"

#include "named.h"
#include "ns_parseutil.h"
#include "ns_lexer.h"

#define SYM_ZONE	0x010000
#define SYM_SERVER	0x020000
#define SYM_KEY		0x030000
#define SYM_ACL		0x040000
#define SYM_CHANNEL	0x050000
#define SYM_PORT	0x060000

#define SYMBOL_TABLE_SIZE 29989		/* should always be prime */
static symbol_table symtab;

#define AUTH_TABLE_SIZE 397		/* should always be prime */
static symbol_table authtab = NULL;

static zone_config current_zone;
static int should_install;

static options current_options;
static int seen_options;

static controls current_controls;

static topology_config current_topology;
static int seen_topology;

static server_config current_server;
static int seen_server;

static char *current_algorithm;
static char *current_secret;

static log_config current_logging;
static int current_category;
static int chan_type;
static int chan_level;
static u_int chan_flags;
static int chan_facility;
static char *chan_name;
static int chan_versions;
static u_long chan_max_size;

static log_channel lookup_channel(char *);
static void define_channel(char *, log_channel);
static char *canonical_name(char *);

int yyparse();
	
#line 103 "ns_parser.y"
typedef union {
	char *			cp;
	int			s_int;
	long			num;
	u_long			ul_int;
	u_int16_t		us_int;
	struct in_addr		ip_addr;
	ip_match_element	ime;
	ip_match_list		iml;
	rrset_order_list	rol;
	rrset_order_element	roe;
	struct dst_key *	keyi;
	enum axfr_format	axfr_fmt;
} YYSTYPE;
#line 130 "y.tab.c"
#define L_EOS 257
#define L_IPADDR 258
#define L_NUMBER 259
#define L_STRING 260
#define L_QSTRING 261
#define L_END_INCLUDE 262
#define T_INCLUDE 263
#define T_OPTIONS 264
#define T_DIRECTORY 265
#define T_PIDFILE 266
#define T_NAMED_XFER 267
#define T_DUMP_FILE 268
#define T_STATS_FILE 269
#define T_MEMSTATS_FILE 270
#define T_FAKE_IQUERY 271
#define T_RECURSION 272
#define T_FETCH_GLUE 273
#define T_QUERY_SOURCE 274
#define T_LISTEN_ON 275
#define T_PORT 276
#define T_ADDRESS 277
#define T_RRSET_ORDER 278
#define T_ORDER 279
#define T_NAME 280
#define T_CLASS 281
#define T_CONTROLS 282
#define T_INET 283
#define T_UNIX 284
#define T_PERM 285
#define T_OWNER 286
#define T_GROUP 287
#define T_ALLOW 288
#define T_DATASIZE 289
#define T_STACKSIZE 290
#define T_CORESIZE 291
#define T_DEFAULT 292
#define T_UNLIMITED 293
#define T_FILES 294
#define T_VERSION 295
#define T_HOSTSTATS 296
#define T_DEALLOC_ON_EXIT 297
#define T_TRANSFERS_IN 298
#define T_TRANSFERS_OUT 299
#define T_TRANSFERS_PER_NS 300
#define T_TRANSFER_FORMAT 301
#define T_MAX_TRANSFER_TIME_IN 302
#define T_SERIAL_QUERIES 303
#define T_ONE_ANSWER 304
#define T_MANY_ANSWERS 305
#define T_NOTIFY 306
#define T_AUTH_NXDOMAIN 307
#define T_MULTIPLE_CNAMES 308
#define T_USE_IXFR 309
#define T_MAINTAIN_IXFR_BASE 310
#define T_CLEAN_INTERVAL 311
#define T_INTERFACE_INTERVAL 312
#define T_STATS_INTERVAL 313
#define T_MAX_LOG_SIZE_IXFR 314
#define T_HEARTBEAT 315
#define T_USE_ID_POOL 316
#define T_MAX_NCACHE_TTL 317
#define T_HAS_OLD_CLIENTS 318
#define T_RFC2308_TYPE1 319
#define T_LAME_TTL 320
#define T_MIN_ROOTS 321
#define T_TREAT_CR_AS_SPACE 322
#define T_LOGGING 323
#define T_CATEGORY 324
#define T_CHANNEL 325
#define T_SEVERITY 326
#define T_DYNAMIC 327
#define T_FILE 328
#define T_VERSIONS 329
#define T_SIZE 330
#define T_SYSLOG 331
#define T_DEBUG 332
#define T_NULL_OUTPUT 333
#define T_PRINT_TIME 334
#define T_PRINT_CATEGORY 335
#define T_PRINT_SEVERITY 336
#define T_SORTLIST 337
#define T_TOPOLOGY 338
#define T_SERVER 339
#define T_LONG_AXFR 340
#define T_BOGUS 341
#define T_TRANSFERS 342
#define T_KEYS 343
#define T_SUPPORT_IXFR 344
#define T_ZONE 345
#define T_IN 346
#define T_CHAOS 347
#define T_HESIOD 348
#define T_TYPE 349
#define T_MASTER 350
#define T_SLAVE 351
#define T_STUB 352
#define T_RESPONSE 353
#define T_HINT 354
#define T_MASTERS 355
#define T_TRANSFER_SOURCE 356
#define T_PUBKEY 357
#define T_ALSO_NOTIFY 358
#define T_DIALUP 359
#define T_FILE_IXFR 360
#define T_IXFR_TMP 361
#define T_TRUSTED_KEYS 362
#define T_ACL 363
#define T_ALLOW_UPDATE 364
#define T_ALLOW_QUERY 365
#define T_ALLOW_TRANSFER 366
#define T_ALLOW_RECURSION 367
#define T_BLACKHOLE 368
#define T_SEC_KEY 369
#define T_ALGID 370
#define T_SECRET 371
#define T_CHECK_NAMES 372
#define T_WARN 373
#define T_FAIL 374
#define T_IGNORE 375
#define T_FORWARD 376
#define T_FORWARDERS 377
#define T_ONLY 378
#define T_FIRST 379
#define T_IF_NO_ANSWER 380
#define T_IF_NO_DOMAIN 381
#define T_YES 382
#define T_TRUE 383
#define T_NO 384
#define T_FALSE 385
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,   31,   31,   32,   32,   32,   32,   32,   32,   32,
   32,   32,   32,   32,   32,   33,   42,   34,   43,   43,
   44,   44,   44,   44,   44,   44,   44,   44,   44,   44,
   44,   44,   44,   44,   44,   44,   44,   44,   44,   44,
   44,   44,   44,   44,   46,   44,   44,   44,   44,   44,
   44,   44,   49,   44,   44,   44,   44,   44,   44,   44,
   44,   44,   44,   44,   44,   44,   44,   44,   44,   44,
   44,   44,   44,   35,   53,   53,   54,   54,   54,   54,
   15,   15,   12,   12,   13,   13,   14,   14,   16,    6,
    6,    5,    5,    4,    4,   55,   56,   48,   48,   48,
   48,    2,    2,    3,    3,   29,   29,   29,   29,   29,
   27,   27,   27,   28,   28,   28,   45,   45,   45,   45,
   51,   51,   51,   51,   26,   26,   26,   26,   52,   52,
   52,   47,   47,   57,   57,   58,   50,   50,   59,   59,
   60,   61,   36,   62,   62,   62,   64,   63,   66,   63,
   68,   68,   68,   68,   69,   69,   70,   71,   71,   71,
   71,   71,   72,   10,   10,   11,   11,   73,   74,   74,
   74,   74,   74,   74,   74,   67,   67,   67,    9,    9,
   75,   65,   65,   65,    8,    8,    8,    7,   76,   37,
   77,   77,   78,   78,   78,   78,   78,   78,   20,   20,
   18,   18,   18,   17,   17,   17,   17,   17,   19,   23,
   80,   79,   79,   79,   81,   41,   82,   82,   82,   24,
   25,   40,   84,   38,   83,   83,   21,   21,   22,   22,
   22,   22,   22,   85,   85,   86,   86,   86,   86,   86,
   86,   86,   86,   86,   86,   86,   89,   86,   86,   86,
   86,   86,   86,   86,   86,   86,   86,   87,   87,   92,
   91,   91,   93,   93,   94,   88,   88,   90,   90,   95,
   95,   96,   39,   97,   97,   98,   98,    1,   30,   30,
};
short yylen[] = {                                         2,
    1,    1,    2,    1,    2,    2,    2,    2,    2,    2,
    2,    2,    1,    2,    2,    3,    0,    5,    2,    3,
    0,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    3,
    2,    2,    5,    2,    0,    5,    2,    2,    4,    4,
    4,    4,    0,    5,    4,    4,    1,    1,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    4,
    2,    2,    1,    4,    2,    3,    0,    8,    8,    1,
    2,    3,    0,    2,    0,    2,    0,    2,    5,    1,
    1,    1,    1,    1,    1,    2,    2,    1,    1,    2,
    2,    0,    2,    0,    2,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    2,    2,    2,    2,    1,    1,    1,    1,    2,    2,
    2,    0,    1,    2,    3,    1,    0,    1,    2,    3,
    1,    0,    5,    2,    3,    1,    0,    6,    0,    6,
    1,    1,    2,    1,    2,    2,    2,    0,    1,    1,
    2,    2,    3,    1,    1,    0,    1,    2,    1,    1,
    1,    2,    2,    2,    2,    2,    3,    1,    1,    1,
    1,    2,    3,    1,    1,    1,    1,    1,    0,    6,
    2,    3,    2,    2,    2,    2,    4,    1,    2,    3,
    1,    2,    2,    1,    3,    3,    1,    3,    1,    1,
    1,    2,    3,    1,    0,    6,    2,    2,    1,    3,
    3,    5,    0,    5,    0,    3,    0,    1,    1,    1,
    1,    1,    1,    2,    3,    2,    2,    2,    2,    5,
    2,    2,    4,    4,    4,    2,    0,    5,    2,    2,
    2,    2,    5,    5,    4,    2,    1,    2,    3,    1,
    0,    1,    2,    3,    1,    1,    1,    0,    1,    2,
    3,    1,    4,    2,    3,    5,    5,    1,    1,    1,
};
short yydefred[] = {                                      0,
    0,   13,    0,   17,    0,  142,    0,    0,    0,    0,
  215,    0,    0,    2,    4,    0,    0,    0,    0,    0,
    0,    0,    0,   14,   15,    0,    0,    0,    0,  189,
    0,    0,  279,  280,    0,    0,    3,    5,    6,    7,
    8,    9,   10,   11,   12,   16,    0,   80,    0,    0,
    0,    0,    0,    0,  223,  228,    0,    0,    0,    0,
    0,   73,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   53,    0,    0,
    0,    0,    0,    0,    0,   45,    0,    0,   57,   58,
   92,   93,    0,    0,   74,    0,   75,  146,    0,    0,
    0,    0,    0,    0,    0,    0,  273,    0,  274,    0,
    0,    0,    0,    0,  201,    0,  207,    0,  209,    0,
   23,   25,   24,   28,   26,   27,  110,  106,  107,  108,
  109,   29,   30,   31,    0,    0,   47,    0,    0,    0,
    0,    0,  126,  127,  128,  121,  125,  122,  123,  124,
   22,   33,   34,  129,  130,  131,   90,   91,   59,   60,
   61,   32,   38,   39,   35,   36,   62,   63,   64,   65,
   68,   41,   66,   37,   42,   67,   72,   71,    0,    0,
   48,    0,   69,    0,    0,    0,    0,  111,  112,  113,
    0,  117,  118,  119,  120,   44,    0,   18,    0,   19,
    0,    0,   76,  186,  187,  147,  188,  185,  180,  149,
  179,  143,    0,  144,  198,    0,    0,    0,    0,    0,
    0,    0,    0,  224,    0,    0,  275,    0,    0,  203,
    0,  202,  199,  222,    0,  219,    0,    0,    0,    0,
    0,  278,   95,   94,   97,   96,  100,  101,  103,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  114,  115,  116,   40,    0,   20,    0,    0,    0,
    0,  145,  196,  193,  195,    0,  194,  190,    0,  191,
  257,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  247,
    0,    0,    0,    0,  205,  206,  208,  200,    0,    0,
  217,  218,  216,    0,   84,    0,    0,   70,    0,   81,
   52,   56,  141,    0,    0,    0,   49,   51,   50,   55,
  136,    0,    0,    0,    0,    0,    0,    0,  214,  211,
  210,    0,    0,  192,  249,  251,  252,  250,  237,  229,
  230,  232,  231,  233,  236,    0,    0,  241,    0,    0,
    0,  256,  238,  239,    0,    0,    0,  242,  266,  267,
  246,    0,  226,    0,  234,  276,  277,  220,  221,   43,
   86,    0,    0,   82,   54,    0,  139,   46,    0,  134,
    0,    0,  184,  181,    0,    0,  178,    0,    0,    0,
  171,    0,    0,    0,    0,  169,  170,    0,  197,    0,
  212,  105,    0,    0,    0,  265,    0,    0,    0,    0,
    0,    0,    0,  235,   88,    0,  140,  135,    0,    0,
  148,    0,  182,  154,    0,  151,  172,    0,  165,  167,
  168,  164,  173,  174,  175,  150,    0,  176,  213,  260,
    0,    0,    0,    0,  255,    0,  263,  243,  244,  245,
  272,    0,    0,    0,   89,   78,   79,  183,  153,    0,
    0,    0,    0,  163,  177,  240,    0,  258,  253,  254,
  264,  248,    0,  270,  155,  156,  157,  161,  162,  259,
  271,
};
short yydgoto[] = {                                      12,
  274,  171,  387,  275,  123,  189,  236,  237,  424,  470,
  471,  282,  347,  413,  283,  284,  145,  146,  147,  148,
   55,  385,  370,  269,  270,  176,  221,  295,  162,  149,
   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,
   23,   27,  117,  118,  226,  227,  362,  167,  212,  354,
  119,  120,   51,   52,  168,  169,  363,  364,  355,  356,
   29,  131,  132,  300,  425,  301,  435,  467,  502,  503,
  504,  436,  437,  438,  426,   54,  251,  252,  372,  373,
   36,  271,  254,  134,  331,  332,  481,  401,  402,  492,
  447,  482,  448,  449,  493,  494,   58,   59,
};
short yysindex[] = {                                    419,
 -172,    0, -236,    0,  -91,    0, -224, -211,  -71, -178,
    0,    0,  419,    0,    0, -166, -160, -158, -156, -154,
 -144, -139, -128,    0,    0, -126,  -49, -195,   10,    0,
 -178, -198,    0,    0,   12, -178,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  349,    0,   -7, -123,
 -112, -115, -238,   23,    0,    0, -189, -110, -105,   43,
   31,    0,  -98,  -96,  -94,  -85,  -76,  -73, -190, -190,
 -190,  -86, -106,   33,  -81,  -81,  -81,  -81,  -58, -190,
 -190,  -59,  -50,  -45, -121,  -34,  -32, -190, -190, -190,
 -190, -190,   51,   56,   63,   64,   66, -190,   68, -190,
 -190,   69,   71, -190,  123,  136,   -7,    0, -190,  212,
  219,  220,  222, -258, -182,    0,  168,   89,    0,    0,
    0,    0,   73,   62,    0,   93,    0,    0, -181, -216,
  -69,   94, -220,  230,   95,   96,    0,   99,    0,  312,
  313,  104,   43, -100,    0,  108,    0,  -29,    0, -196,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  -31,   -7,    0,  100,   92,  111,
  254,   98,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   43,   43,
    0,  257,    0,   43,   43,   43,   43,    0,    0,    0,
  -68,    0,    0,    0,    0,    0,  258,    0,  127,    0,
  111,  126,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  129,    0,    0, -121, -190,  130,  265, -190,
 -120,  133,  374,    0,  134,  135,    0,  137,  138,    0,
  -25,    0,    0,    0,  141,    0, -178, -178,   21,   32,
  275,    0,    0,    0,    0,    0,    0,    0,    0,   43,
 -178,   52, -108,  146,  -21,  -17,  147,  -11,    5,    9,
   14,    0,    0,    0,    0,  148,    0,  116,  121,  286,
  287,    0,    0,    0,    0, -151,    0,    0,  154,    0,
    0,  155, -190, -190,  157,  152,  -13,  143,   -7,   35,
  294, -190,  160,  161,  300,  302,  304,  -68,  -70,    0,
  236,  171,  169,  170,    0,    0,    0,    0,  172,  175,
    0,    0,    0,   18,    0, -178,  164,    0,  188,    0,
    0,    0,    0,  322,  147,  191,    0,    0,    0,    0,
    0,  324,  148,  193,  328,  194, -207,   -2,    0,    0,
    0,  -92,  195,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  111,  331,    0,  196,  197,
  202,    0,    0,    0,   43,   43,   43,    0,    0,    0,
    0,  338,    0,  215,    0,    0,    0,    0,    0,    0,
    0,  232,  216,    0,    0,  237,    0,    0,  239,    0,
   43,  186,    0,    0,  -41,  243,    0, -145,  240, -183,
    0, -190, -190, -190, -118,    0,    0,  245,    0,  246,
    0,    0,  249,  250,  251,    0,  379,  202,  255,   22,
   26,   30,  253,    0,    0,  248,    0,    0,   39,  256,
    0,  259,    0,    0,  260,    0,    0,  -16,    0,    0,
    0,    0,    0,    0,    0,    0,  261,    0,    0,    0,
 -101,  263,  252,  264,    0,  271,    0,    0,    0,    0,
    0,  389,  253,  272,    0,    0,    0,    0,    0, -218,
  -81,  187,  192,    0,    0,    0,  273,    0,    0,    0,
    0,    0,  274,    0,    0,    0,    0,    0,    0,    0,
    0,
};
short yyrindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  522,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  280,    0,    0,
 -117,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  282,    0,    0,    0,
  280,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  409,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  282,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  284,    0,    0,    0,    0,    0,  290,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  291,  292,    0,
    0, -222,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   41, -222,    0,    0,    0,  418,    0,    0,    0,
    0,    0,    0,    0,    0,  426,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  429,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  276,    0,    0,    0,
    0,    0,    0,    0,  428,    0,    0,    0,    0,    0,
    0,    0,  431,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  432,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  297,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  433,    0,    0,
    0,    0,  434,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  303,    0,    0,  305,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  436,    0,    0,    0,    0,    0,    0,    0,
    0,  306,  308,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,
};
short yygindex[] = {                                      0,
 -124,    0,    0,    0,  -93,  320,    0,    0,  437,    0,
    0,    0,    0,    0,    0,  285,  425,  -84,    0,  102,
    0,    0,    0,  301,  307,  -75,    0,  242,  -61,  -10,
    0,  559,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  456,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  523,  406,  410,    0,  214,    0,  224,
    0,    0,  449,    0,    0,    0,    0,    0,   78,   80,
    0,    0,    0,  149,  158,    0,    0,  335,    0,  217,
    0,    0,    0,    0,    0,  267,    0,    0,    0,    0,
    0,  106,    0,  140,    0,   97,    0,  541,
};
#define YYTABLESIZE 788
short yytable[] = {                                      35,
  178,  179,  180,  144,  308,  227,  476,  144,  163,  164,
  273,  144,  125,  211,  137,  144,  348,  128,  182,  183,
   56,  144,  143,  506,   26,   61,  192,  193,  194,  195,
  196,   28,  439,   30,  122,  245,  202,  144,  204,  205,
  515,  144,  208,   33,   34,  279,  144,  213,  423,   31,
  144,   32,   33,   34,  144,  242,   83,   83,  144,  266,
   48,   57,  144,  265,  177,  177,  177,  177,  157,  135,
  136,  144,  276,   47,  516,  144,   33,   34,   33,   34,
  246,   33,   34,  461,   24,  129,  130,   49,   50,   25,
   38,  218,  219,  143,  220,  264,   39,  143,   40,  337,
   41,  143,   42,  351,  369,  143,  298,  352,   33,   34,
  234,  143,   43,  357,   33,   34,  239,   44,  238,  241,
  247,  248,  249,  250,  235,  239,   83,  143,   45,  358,
   46,  143,   53,  359,   60,  245,  143,  124,  360,  227,
  143,  127,  410,   48,  143,  133,  488,  469,  143,   57,
  489,  139,  143,  150,  490,  172,  480,  140,  141,   33,
   34,  143,  151,  496,  152,  143,  153,   33,   34,  170,
   49,   50,  281,  267,  268,  154,  265,  173,   33,   34,
  246,  464,  187,  188,  155,  304,  465,  156,  307,  165,
  166,  158,  159,  160,  161,  222,  223,  224,  225,  184,
  265,  265,  181,  265,  265,  265,  265,  428,  185,  429,
  174,  175,  430,  186,  431,  432,  433,  434,   33,   34,
  247,  248,  249,  250,  190,  388,  191,  272,  140,  141,
   33,   34,  140,  141,   33,   34,  140,  141,   33,   34,
  140,  141,   33,   34,  261,  209,  140,  141,   33,   34,
  121,  376,  377,  427,  129,  130,  339,  340,  210,  265,
  392,  442,  140,  141,   33,   34,  140,  141,   33,   34,
  345,  140,  141,   33,   34,  140,  141,   33,   34,  140,
  141,   33,   34,  140,  141,   33,   34,  140,  141,   33,
   34,  239,  228,  389,  390,  371,  140,  141,   33,   34,
  140,  141,   33,   34,  292,  293,  294,  399,  400,  197,
  285,  286,  500,  501,  198,  288,  289,  290,  291,   85,
   85,  199,  200,  428,  201,  429,  203,  206,  430,  207,
  431,  432,  433,  434,  214,  411,  380,  381,  382,  142,
  383,  215,  216,  142,  217,  230,  232,  142,  231,  233,
  244,  142,  253,  255,  256,  257,  241,  142,  258,  259,
  403,  371,  384,  260,  263,  265,  265,  265,  166,  272,
  473,  474,  475,  142,  265,  165,  280,  142,  281,  287,
  296,  344,  142,  297,  299,  302,  142,  306,  305,  310,
  142,  268,  333,  334,  142,  335,  336,  338,  142,  343,
  346,  267,  350,  365,  353,  361,  366,  142,  367,  368,
  374,  142,  379,  375,  241,  378,  391,  466,  386,  472,
  393,  394,  395,   62,  396,  517,  397,  405,  408,  406,
  407,  409,   63,   64,   65,   66,   67,   68,   69,   70,
   71,   72,   73,  412,  414,   74,  415,  417,  418,  420,
  421,  441,  422,  443,  444,  445,   75,   76,   77,  446,
  453,   78,   79,   80,   81,   82,   83,   84,   85,   86,
   87,  454,  460,   88,   89,   90,   91,   92,   93,   94,
   95,   96,   97,   98,   99,  100,  101,  102,  103,  104,
  177,  311,  455,  457,  456,  458,  450,  451,  452,  463,
  468,  478,  479,  485,  105,  106,  480,  495,  483,  484,
  491,  487,  509,  512,  497,  498,  501,  505,  499,  508,
  500,    1,  459,  107,  510,  108,  109,  511,  514,  520,
  521,  102,  110,  111,  112,  113,   77,  312,   21,  114,
  225,  313,  137,  115,  116,  314,  204,   98,   99,  315,
  132,  104,  138,  166,   87,  133,  261,  262,  268,  152,
  269,  158,  159,  316,  160,  303,  240,  349,  262,  398,
  342,   37,  229,  126,  278,  341,  419,  277,  416,  243,
  519,  518,  462,  477,  317,  309,  507,  486,  440,  513,
  318,  319,  320,  321,  322,  323,  324,  404,  138,  325,
  326,  327,    0,    0,   62,    0,    0,  328,    0,    0,
    0,  329,  330,   63,   64,   65,   66,   67,   68,   69,
   70,   71,   72,   73,    0,    0,   74,    0,    0,  311,
    0,    0,    0,    0,    0,    0,    0,   75,   76,   77,
    0,    0,   78,   79,   80,   81,   82,   83,   84,   85,
   86,   87,    0,    0,   88,   89,   90,   91,   92,   93,
   94,   95,   96,   97,   98,   99,  100,  101,  102,  103,
  104,    0,    0,    0,    1,  312,    0,    0,    0,  313,
    2,    3,    4,  314,    0,  105,  106,  315,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    5,  316,    0,    0,  107,    0,  108,  109,    0,    0,
    0,    0,    0,  110,  111,  112,  113,    0,    0,    0,
  114,    0,  317,    0,  115,  116,    0,    0,  318,  319,
  320,  321,  322,  323,  324,    0,    0,  325,  326,  327,
    0,    6,    0,    0,    0,  328,    0,    0,    0,  329,
  330,    0,    0,    0,    0,    0,    0,    7,    0,    0,
    0,    0,    0,    8,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    9,   10,    0,    0,    0,    0,    0,   11,
};
short yycheck[] = {                                      10,
   76,   77,   78,   33,  125,  123,  125,   33,   70,   71,
   42,   33,  125,  107,  125,   33,  125,  256,   80,   81,
   31,   33,  123,  125,  261,   36,   88,   89,   90,   91,
   92,  123,  125,  258,   42,  256,   98,   33,  100,  101,
  259,   33,  104,  260,  261,  170,   33,  109,  256,  261,
   33,  123,  260,  261,   33,  125,  279,  280,   33,  256,
  256,  260,   33,  148,   75,   76,   77,   78,  259,  259,
  260,   33,  166,  123,  293,   33,  260,  261,  260,  261,
  301,  260,  261,  125,  257,  324,  325,  283,  284,  262,
  257,  350,  351,  123,  353,  125,  257,  123,  257,  125,
  257,  123,  257,  125,  256,  123,  231,  125,  260,  261,
  292,  123,  257,  125,  260,  261,  333,  257,  129,  130,
  341,  342,  343,  344,  306,  333,  349,  123,  257,  125,
  257,  123,  123,  125,  123,  256,  123,  261,  125,  257,
  123,  257,  125,  256,  123,  123,  125,  331,  123,  260,
  125,  257,  123,  123,  125,  123,  258,  258,  259,  260,
  261,  123,  261,  125,  261,  123,  261,  260,  261,  276,
  283,  284,  281,  370,  371,  261,  261,  259,  260,  261,
  301,  327,  304,  305,  261,  247,  332,  261,  250,  276,
  277,  382,  383,  384,  385,  378,  379,  380,  381,  259,
  285,  286,  261,  288,  289,  290,  291,  326,  259,  328,
  292,  293,  331,  259,  333,  334,  335,  336,  260,  261,
  341,  342,  343,  344,  259,  319,  259,  259,  258,  259,
  260,  261,  258,  259,  260,  261,  258,  259,  260,  261,
  258,  259,  260,  261,  143,  123,  258,  259,  260,  261,
  258,  313,  314,  256,  324,  325,  267,  268,  123,  344,
  322,  386,  258,  259,  260,  261,  258,  259,  260,  261,
  281,  258,  259,  260,  261,  258,  259,  260,  261,  258,
  259,  260,  261,  258,  259,  260,  261,  258,  259,  260,
  261,  333,  125,  259,  260,  306,  258,  259,  260,  261,
  258,  259,  260,  261,  373,  374,  375,  378,  379,  259,
  209,  210,  329,  330,  259,  214,  215,  216,  217,  279,
  280,  259,  259,  326,  259,  328,  259,  259,  331,  259,
  333,  334,  335,  336,  123,  346,  350,  351,  352,  369,
  354,  123,  123,  369,  123,  257,  285,  369,  276,  257,
  257,  369,  123,  259,  259,  257,  367,  369,   47,   47,
  125,  372,  376,  260,  257,  450,  451,  452,  277,  259,
  432,  433,  434,  369,  459,  276,  123,  369,  281,  123,
  123,  280,  369,  257,  259,  257,  369,  123,  259,  257,
  369,  371,  259,  259,  369,  259,  259,  257,  369,  125,
  349,  370,  257,  288,  258,  258,  286,  369,  123,  123,
  257,  369,  261,  259,  425,  259,  123,  428,  276,  430,
  261,  261,  123,  256,  123,  501,  123,  257,  257,  261,
  261,  257,  265,  266,  267,  268,  269,  270,  271,  272,
  273,  274,  275,  280,  257,  278,  125,  257,  125,  257,
  123,  257,  259,  123,  259,  259,  289,  290,  291,  258,
  123,  294,  295,  296,  297,  298,  299,  300,  301,  302,
  303,  257,  287,  306,  307,  308,  309,  310,  311,  312,
  313,  314,  315,  316,  317,  318,  319,  320,  321,  322,
  501,  256,  261,  257,  279,  257,  395,  396,  397,  257,
  261,  257,  257,  125,  337,  338,  258,  260,  259,  259,
  258,  257,  261,  125,  259,  257,  330,  257,  259,  257,
  329,    0,  421,  356,  261,  358,  359,  257,  257,  257,
  257,  123,  365,  366,  367,  368,  257,  302,  257,  372,
  257,  306,  125,  376,  377,  310,  257,  257,  257,  314,
  125,  123,  125,  257,  279,  125,  125,  125,  125,  257,
  125,  257,  257,  328,  257,  246,  130,  283,  144,  328,
  270,   13,  117,   51,  169,  269,  363,  168,  355,  131,
  503,  502,  425,  435,  349,  251,  481,  448,  372,  493,
  355,  356,  357,  358,  359,  360,  361,  331,   58,  364,
  365,  366,   -1,   -1,  256,   -1,   -1,  372,   -1,   -1,
   -1,  376,  377,  265,  266,  267,  268,  269,  270,  271,
  272,  273,  274,  275,   -1,   -1,  278,   -1,   -1,  256,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  289,  290,  291,
   -1,   -1,  294,  295,  296,  297,  298,  299,  300,  301,
  302,  303,   -1,   -1,  306,  307,  308,  309,  310,  311,
  312,  313,  314,  315,  316,  317,  318,  319,  320,  321,
  322,   -1,   -1,   -1,  256,  302,   -1,   -1,   -1,  306,
  262,  263,  264,  310,   -1,  337,  338,  314,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  282,  328,   -1,   -1,  356,   -1,  358,  359,   -1,   -1,
   -1,   -1,   -1,  365,  366,  367,  368,   -1,   -1,   -1,
  372,   -1,  349,   -1,  376,  377,   -1,   -1,  355,  356,
  357,  358,  359,  360,  361,   -1,   -1,  364,  365,  366,
   -1,  323,   -1,   -1,   -1,  372,   -1,   -1,   -1,  376,
  377,   -1,   -1,   -1,   -1,   -1,   -1,  339,   -1,   -1,
   -1,   -1,   -1,  345,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  362,  363,   -1,   -1,   -1,   -1,   -1,  369,
};
#define YYFINAL 12
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 385
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
"'!'",0,0,0,0,0,0,0,0,"'*'",0,0,0,0,"'/'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'{'",0,"'}'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"L_EOS",
"L_IPADDR","L_NUMBER","L_STRING","L_QSTRING","L_END_INCLUDE","T_INCLUDE",
"T_OPTIONS","T_DIRECTORY","T_PIDFILE","T_NAMED_XFER","T_DUMP_FILE",
"T_STATS_FILE","T_MEMSTATS_FILE","T_FAKE_IQUERY","T_RECURSION","T_FETCH_GLUE",
"T_QUERY_SOURCE","T_LISTEN_ON","T_PORT","T_ADDRESS","T_RRSET_ORDER","T_ORDER",
"T_NAME","T_CLASS","T_CONTROLS","T_INET","T_UNIX","T_PERM","T_OWNER","T_GROUP",
"T_ALLOW","T_DATASIZE","T_STACKSIZE","T_CORESIZE","T_DEFAULT","T_UNLIMITED",
"T_FILES","T_VERSION","T_HOSTSTATS","T_DEALLOC_ON_EXIT","T_TRANSFERS_IN",
"T_TRANSFERS_OUT","T_TRANSFERS_PER_NS","T_TRANSFER_FORMAT",
"T_MAX_TRANSFER_TIME_IN","T_SERIAL_QUERIES","T_ONE_ANSWER","T_MANY_ANSWERS",
"T_NOTIFY","T_AUTH_NXDOMAIN","T_MULTIPLE_CNAMES","T_USE_IXFR",
"T_MAINTAIN_IXFR_BASE","T_CLEAN_INTERVAL","T_INTERFACE_INTERVAL",
"T_STATS_INTERVAL","T_MAX_LOG_SIZE_IXFR","T_HEARTBEAT","T_USE_ID_POOL",
"T_MAX_NCACHE_TTL","T_HAS_OLD_CLIENTS","T_RFC2308_TYPE1","T_LAME_TTL",
"T_MIN_ROOTS","T_TREAT_CR_AS_SPACE","T_LOGGING","T_CATEGORY","T_CHANNEL",
"T_SEVERITY","T_DYNAMIC","T_FILE","T_VERSIONS","T_SIZE","T_SYSLOG","T_DEBUG",
"T_NULL_OUTPUT","T_PRINT_TIME","T_PRINT_CATEGORY","T_PRINT_SEVERITY",
"T_SORTLIST","T_TOPOLOGY","T_SERVER","T_LONG_AXFR","T_BOGUS","T_TRANSFERS",
"T_KEYS","T_SUPPORT_IXFR","T_ZONE","T_IN","T_CHAOS","T_HESIOD","T_TYPE",
"T_MASTER","T_SLAVE","T_STUB","T_RESPONSE","T_HINT","T_MASTERS",
"T_TRANSFER_SOURCE","T_PUBKEY","T_ALSO_NOTIFY","T_DIALUP","T_FILE_IXFR",
"T_IXFR_TMP","T_TRUSTED_KEYS","T_ACL","T_ALLOW_UPDATE","T_ALLOW_QUERY",
"T_ALLOW_TRANSFER","T_ALLOW_RECURSION","T_BLACKHOLE","T_SEC_KEY","T_ALGID",
"T_SECRET","T_CHECK_NAMES","T_WARN","T_FAIL","T_IGNORE","T_FORWARD",
"T_FORWARDERS","T_ONLY","T_FIRST","T_IF_NO_ANSWER","T_IF_NO_DOMAIN","T_YES",
"T_TRUE","T_NO","T_FALSE",
};
char *yyrule[] = {
"$accept : config_file",
"config_file : statement_list",
"statement_list : statement",
"statement_list : statement_list statement",
"statement : include_stmt",
"statement : options_stmt L_EOS",
"statement : controls_stmt L_EOS",
"statement : logging_stmt L_EOS",
"statement : server_stmt L_EOS",
"statement : zone_stmt L_EOS",
"statement : trusted_keys_stmt L_EOS",
"statement : acl_stmt L_EOS",
"statement : key_stmt L_EOS",
"statement : L_END_INCLUDE",
"statement : error L_EOS",
"statement : error L_END_INCLUDE",
"include_stmt : T_INCLUDE L_QSTRING L_EOS",
"$$1 :",
"options_stmt : T_OPTIONS $$1 '{' options '}'",
"options : option L_EOS",
"options : options option L_EOS",
"option :",
"option : T_VERSION L_QSTRING",
"option : T_DIRECTORY L_QSTRING",
"option : T_NAMED_XFER L_QSTRING",
"option : T_PIDFILE L_QSTRING",
"option : T_STATS_FILE L_QSTRING",
"option : T_MEMSTATS_FILE L_QSTRING",
"option : T_DUMP_FILE L_QSTRING",
"option : T_FAKE_IQUERY yea_or_nay",
"option : T_RECURSION yea_or_nay",
"option : T_FETCH_GLUE yea_or_nay",
"option : T_NOTIFY yea_or_nay",
"option : T_HOSTSTATS yea_or_nay",
"option : T_DEALLOC_ON_EXIT yea_or_nay",
"option : T_USE_IXFR yea_or_nay",
"option : T_MAINTAIN_IXFR_BASE yea_or_nay",
"option : T_HAS_OLD_CLIENTS yea_or_nay",
"option : T_AUTH_NXDOMAIN yea_or_nay",
"option : T_MULTIPLE_CNAMES yea_or_nay",
"option : T_CHECK_NAMES check_names_type check_names_opt",
"option : T_USE_ID_POOL yea_or_nay",
"option : T_RFC2308_TYPE1 yea_or_nay",
"option : T_LISTEN_ON maybe_port '{' address_match_list '}'",
"option : T_FORWARD forward_opt",
"$$2 :",
"option : T_FORWARDERS $$2 '{' opt_forwarders_list '}'",
"option : T_QUERY_SOURCE query_source",
"option : T_TRANSFER_SOURCE maybe_wild_addr",
"option : T_ALLOW_QUERY '{' address_match_list '}'",
"option : T_ALLOW_RECURSION '{' address_match_list '}'",
"option : T_ALLOW_TRANSFER '{' address_match_list '}'",
"option : T_SORTLIST '{' address_match_list '}'",
"$$3 :",
"option : T_ALSO_NOTIFY $$3 '{' opt_also_notify_list '}'",
"option : T_BLACKHOLE '{' address_match_list '}'",
"option : T_TOPOLOGY '{' address_match_list '}'",
"option : size_clause",
"option : transfer_clause",
"option : T_TRANSFER_FORMAT transfer_format",
"option : T_MAX_TRANSFER_TIME_IN L_NUMBER",
"option : T_SERIAL_QUERIES L_NUMBER",
"option : T_CLEAN_INTERVAL L_NUMBER",
"option : T_INTERFACE_INTERVAL L_NUMBER",
"option : T_STATS_INTERVAL L_NUMBER",
"option : T_MAX_LOG_SIZE_IXFR L_NUMBER",
"option : T_MAX_NCACHE_TTL L_NUMBER",
"option : T_LAME_TTL L_NUMBER",
"option : T_HEARTBEAT L_NUMBER",
"option : T_DIALUP yea_or_nay",
"option : T_RRSET_ORDER '{' rrset_ordering_list '}'",
"option : T_TREAT_CR_AS_SPACE yea_or_nay",
"option : T_MIN_ROOTS L_NUMBER",
"option : error",
"controls_stmt : T_CONTROLS '{' controls '}'",
"controls : control L_EOS",
"controls : controls control L_EOS",
"control :",
"control : T_INET maybe_wild_addr T_PORT in_port T_ALLOW '{' address_match_list '}'",
"control : T_UNIX L_QSTRING T_PERM L_NUMBER T_OWNER L_NUMBER T_GROUP L_NUMBER",
"control : error",
"rrset_ordering_list : rrset_ordering_element L_EOS",
"rrset_ordering_list : rrset_ordering_list rrset_ordering_element L_EOS",
"ordering_class :",
"ordering_class : T_CLASS any_string",
"ordering_type :",
"ordering_type : T_TYPE any_string",
"ordering_name :",
"ordering_name : T_NAME L_QSTRING",
"rrset_ordering_element : ordering_class ordering_type ordering_name T_ORDER L_STRING",
"transfer_format : T_ONE_ANSWER",
"transfer_format : T_MANY_ANSWERS",
"maybe_wild_addr : L_IPADDR",
"maybe_wild_addr : '*'",
"maybe_wild_port : in_port",
"maybe_wild_port : '*'",
"query_source_address : T_ADDRESS maybe_wild_addr",
"query_source_port : T_PORT maybe_wild_port",
"query_source : query_source_address",
"query_source : query_source_port",
"query_source : query_source_address query_source_port",
"query_source : query_source_port query_source_address",
"maybe_port :",
"maybe_port : T_PORT in_port",
"maybe_zero_port :",
"maybe_zero_port : T_PORT in_port",
"yea_or_nay : T_YES",
"yea_or_nay : T_TRUE",
"yea_or_nay : T_NO",
"yea_or_nay : T_FALSE",
"yea_or_nay : L_NUMBER",
"check_names_type : T_MASTER",
"check_names_type : T_SLAVE",
"check_names_type : T_RESPONSE",
"check_names_opt : T_WARN",
"check_names_opt : T_FAIL",
"check_names_opt : T_IGNORE",
"forward_opt : T_ONLY",
"forward_opt : T_FIRST",
"forward_opt : T_IF_NO_ANSWER",
"forward_opt : T_IF_NO_DOMAIN",
"size_clause : T_DATASIZE size_spec",
"size_clause : T_STACKSIZE size_spec",
"size_clause : T_CORESIZE size_spec",
"size_clause : T_FILES size_spec",
"size_spec : any_string",
"size_spec : L_NUMBER",
"size_spec : T_DEFAULT",
"size_spec : T_UNLIMITED",
"transfer_clause : T_TRANSFERS_IN L_NUMBER",
"transfer_clause : T_TRANSFERS_OUT L_NUMBER",
"transfer_clause : T_TRANSFERS_PER_NS L_NUMBER",
"opt_forwarders_list :",
"opt_forwarders_list : forwarders_in_addr_list",
"forwarders_in_addr_list : forwarders_in_addr L_EOS",
"forwarders_in_addr_list : forwarders_in_addr_list forwarders_in_addr L_EOS",
"forwarders_in_addr : L_IPADDR",
"opt_also_notify_list :",
"opt_also_notify_list : also_notify_in_addr_list",
"also_notify_in_addr_list : also_notify_in_addr L_EOS",
"also_notify_in_addr_list : also_notify_in_addr_list also_notify_in_addr L_EOS",
"also_notify_in_addr : L_IPADDR",
"$$4 :",
"logging_stmt : T_LOGGING $$4 '{' logging_opts_list '}'",
"logging_opts_list : logging_opt L_EOS",
"logging_opts_list : logging_opts_list logging_opt L_EOS",
"logging_opts_list : error",
"$$5 :",
"logging_opt : T_CATEGORY category $$5 '{' channel_list '}'",
"$$6 :",
"logging_opt : T_CHANNEL channel_name $$6 '{' channel_opt_list '}'",
"channel_severity : any_string",
"channel_severity : T_DEBUG",
"channel_severity : T_DEBUG L_NUMBER",
"channel_severity : T_DYNAMIC",
"version_modifier : T_VERSIONS L_NUMBER",
"version_modifier : T_VERSIONS T_UNLIMITED",
"size_modifier : T_SIZE size_spec",
"maybe_file_modifiers :",
"maybe_file_modifiers : version_modifier",
"maybe_file_modifiers : size_modifier",
"maybe_file_modifiers : version_modifier size_modifier",
"maybe_file_modifiers : size_modifier version_modifier",
"channel_file : T_FILE L_QSTRING maybe_file_modifiers",
"facility_name : any_string",
"facility_name : T_SYSLOG",
"maybe_syslog_facility :",
"maybe_syslog_facility : facility_name",
"channel_syslog : T_SYSLOG maybe_syslog_facility",
"channel_opt : channel_file",
"channel_opt : channel_syslog",
"channel_opt : T_NULL_OUTPUT",
"channel_opt : T_SEVERITY channel_severity",
"channel_opt : T_PRINT_TIME yea_or_nay",
"channel_opt : T_PRINT_CATEGORY yea_or_nay",
"channel_opt : T_PRINT_SEVERITY yea_or_nay",
"channel_opt_list : channel_opt L_EOS",
"channel_opt_list : channel_opt_list channel_opt L_EOS",
"channel_opt_list : error",
"channel_name : any_string",
"channel_name : T_NULL_OUTPUT",
"channel : channel_name",
"channel_list : channel L_EOS",
"channel_list : channel_list channel L_EOS",
"channel_list : error",
"category_name : any_string",
"category_name : T_DEFAULT",
"category_name : T_NOTIFY",
"category : category_name",
"$$7 :",
"server_stmt : T_SERVER L_IPADDR $$7 '{' server_info_list '}'",
"server_info_list : server_info L_EOS",
"server_info_list : server_info_list server_info L_EOS",
"server_info : T_BOGUS yea_or_nay",
"server_info : T_SUPPORT_IXFR yea_or_nay",
"server_info : T_TRANSFERS L_NUMBER",
"server_info : T_TRANSFER_FORMAT transfer_format",
"server_info : T_KEYS '{' key_list '}'",
"server_info : error",
"address_match_list : address_match_element L_EOS",
"address_match_list : address_match_list address_match_element L_EOS",
"address_match_element : address_match_simple",
"address_match_element : '!' address_match_simple",
"address_match_element : T_SEC_KEY L_STRING",
"address_match_simple : L_IPADDR",
"address_match_simple : L_IPADDR '/' L_NUMBER",
"address_match_simple : L_NUMBER '/' L_NUMBER",
"address_match_simple : address_name",
"address_match_simple : '{' address_match_list '}'",
"address_name : any_string",
"key_ref : any_string",
"key_list_element : key_ref",
"key_list : key_list_element L_EOS",
"key_list : key_list key_list_element L_EOS",
"key_list : error",
"$$8 :",
"key_stmt : T_SEC_KEY $$8 any_string '{' key_definition '}'",
"key_definition : algorithm_id secret",
"key_definition : secret algorithm_id",
"key_definition : error",
"algorithm_id : T_ALGID any_string L_EOS",
"secret : T_SECRET any_string L_EOS",
"acl_stmt : T_ACL any_string '{' address_match_list '}'",
"$$9 :",
"zone_stmt : T_ZONE L_QSTRING optional_class $$9 optional_zone_options_list",
"optional_zone_options_list :",
"optional_zone_options_list : '{' zone_option_list '}'",
"optional_class :",
"optional_class : any_string",
"zone_type : T_MASTER",
"zone_type : T_SLAVE",
"zone_type : T_HINT",
"zone_type : T_STUB",
"zone_type : T_FORWARD",
"zone_option_list : zone_option L_EOS",
"zone_option_list : zone_option_list zone_option L_EOS",
"zone_option : T_TYPE zone_type",
"zone_option : T_FILE L_QSTRING",
"zone_option : T_FILE_IXFR L_QSTRING",
"zone_option : T_IXFR_TMP L_QSTRING",
"zone_option : T_MASTERS maybe_zero_port '{' master_in_addr_list '}'",
"zone_option : T_TRANSFER_SOURCE maybe_wild_addr",
"zone_option : T_CHECK_NAMES check_names_opt",
"zone_option : T_ALLOW_UPDATE '{' address_match_list '}'",
"zone_option : T_ALLOW_QUERY '{' address_match_list '}'",
"zone_option : T_ALLOW_TRANSFER '{' address_match_list '}'",
"zone_option : T_FORWARD zone_forward_opt",
"$$10 :",
"zone_option : T_FORWARDERS $$10 '{' opt_zone_forwarders_list '}'",
"zone_option : T_MAX_TRANSFER_TIME_IN L_NUMBER",
"zone_option : T_MAX_LOG_SIZE_IXFR L_NUMBER",
"zone_option : T_NOTIFY yea_or_nay",
"zone_option : T_MAINTAIN_IXFR_BASE yea_or_nay",
"zone_option : T_PUBKEY L_NUMBER L_NUMBER L_NUMBER L_QSTRING",
"zone_option : T_PUBKEY L_STRING L_NUMBER L_NUMBER L_QSTRING",
"zone_option : T_ALSO_NOTIFY '{' opt_notify_in_addr_list '}'",
"zone_option : T_DIALUP yea_or_nay",
"zone_option : error",
"master_in_addr_list : master_in_addr L_EOS",
"master_in_addr_list : master_in_addr_list master_in_addr L_EOS",
"master_in_addr : L_IPADDR",
"opt_notify_in_addr_list :",
"opt_notify_in_addr_list : notify_in_addr_list",
"notify_in_addr_list : notify_in_addr L_EOS",
"notify_in_addr_list : notify_in_addr_list notify_in_addr L_EOS",
"notify_in_addr : L_IPADDR",
"zone_forward_opt : T_ONLY",
"zone_forward_opt : T_FIRST",
"opt_zone_forwarders_list :",
"opt_zone_forwarders_list : zone_forwarders_in_addr_list",
"zone_forwarders_in_addr_list : zone_forwarders_in_addr L_EOS",
"zone_forwarders_in_addr_list : zone_forwarders_in_addr_list zone_forwarders_in_addr L_EOS",
"zone_forwarders_in_addr : L_IPADDR",
"trusted_keys_stmt : T_TRUSTED_KEYS '{' trusted_keys_list '}'",
"trusted_keys_list : trusted_key L_EOS",
"trusted_keys_list : trusted_keys_list trusted_key L_EOS",
"trusted_key : L_STRING L_NUMBER L_NUMBER L_NUMBER L_QSTRING",
"trusted_key : L_STRING L_STRING L_NUMBER L_NUMBER L_QSTRING",
"in_port : L_NUMBER",
"any_string : L_STRING",
"any_string : L_QSTRING",
};
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH 10000
#endif
#endif
#define YYINITSTACKSIZE 200
int yydebug;
int yynerrs;
struct yystack {
    short *ssp;
    YYSTYPE *vsp;
    short *ss;
    YYSTYPE *vs;
    int stacksize;
    short *sslim;
};
int yychar; /* some people use this, so we copy it in & out */
int yyerrflag; /* must be global for yyerrok & YYRECOVERING */
YYSTYPE yylval;
#line 1776 "ns_parser.y"

static char *
canonical_name(char *name) {
	char canonical[MAXDNAME];
	
	if (strlen(name) >= MAXDNAME)
		return (NULL);
	strcpy(canonical, name);
	if (makename(canonical, ".", sizeof canonical) < 0)
		return (NULL);
	return (savestr(canonical, 0));
}

static void
init_acls() {
	ip_match_element ime;
	ip_match_list iml;
	struct in_addr address;

	/* Create the predefined ACLs */

	address.s_addr = 0U;

	/* ACL "any" */
	ime = new_ip_match_pattern(address, 0);
	iml = new_ip_match_list();
	add_to_ip_match_list(iml, ime);
	define_acl(savestr("any", 1), iml);

	/* ACL "none" */
	ime = new_ip_match_pattern(address, 0);
	ip_match_negate(ime);
	iml = new_ip_match_list();
	add_to_ip_match_list(iml, ime);
	define_acl(savestr("none", 1), iml);

	/* ACL "localhost" */
	ime = new_ip_match_localhost();
	iml = new_ip_match_list();
	add_to_ip_match_list(iml, ime);
	define_acl(savestr("localhost", 1), iml);

	/* ACL "localnets" */
	ime = new_ip_match_localnets();
	iml = new_ip_match_list();
	add_to_ip_match_list(iml, ime);
	define_acl(savestr("localnets", 1), iml);
}

static void
free_sym_value(int type, void *value) {
	ns_debug(ns_log_parser, 99, "free_sym_value: type %06x value %p",
		 type, value);
	type &= ~0xffff;
	switch (type) {
	case SYM_ACL:
		free_ip_match_list(value);
		break;
	case SYM_KEY:
		free_key_info(value);
		break;
	default:
		ns_panic(ns_log_parser, 1,
			 "unhandled case in free_sym_value()");
		/* NOTREACHED */
		break;
	}
}

static log_channel
lookup_channel(char *name) {
	symbol_value value;

	if (lookup_symbol(symtab, name, SYM_CHANNEL, &value))
		return ((log_channel)(value.pointer));
	return (NULL);
}

static void
define_channel(char *name, log_channel channel) {
	symbol_value value;

	value.pointer = channel;  
	define_symbol(symtab, name, SYM_CHANNEL, value, SYMBOL_FREE_KEY);
}

static void
define_builtin_channels() {
	define_channel(savestr("default_syslog", 1), syslog_channel);
	define_channel(savestr("default_debug", 1), debug_channel);
	define_channel(savestr("default_stderr", 1), stderr_channel);
	define_channel(savestr("null", 1), null_channel);
}

static void
parser_setup() {
	seen_options = 0;
	seen_topology = 0;
	symtab = new_symbol_table(SYMBOL_TABLE_SIZE, NULL);
	if (authtab != NULL)
		free_symbol_table(authtab);
	authtab = new_symbol_table(AUTH_TABLE_SIZE, free_sym_value);
	init_acls();
	define_builtin_channels();
	INIT_LIST(current_controls);
}

static void
parser_cleanup() {
	if (symtab != NULL)
		free_symbol_table(symtab);
	symtab = NULL;
	/*
	 * We don't clean up authtab here because the ip_match_lists are in
	 * use.
	 */
}

/*
 * Public Interface
 */

ip_match_list
lookup_acl(char *name) {
	symbol_value value;

	if (lookup_symbol(authtab, name, SYM_ACL, &value))
		return ((ip_match_list)(value.pointer));
	return (NULL);
}

void
define_acl(char *name, ip_match_list iml) {
	symbol_value value;

	INSIST(name != NULL);
	INSIST(iml != NULL);

	value.pointer = iml;
	define_symbol(authtab, name, SYM_ACL, value,
		      SYMBOL_FREE_KEY|SYMBOL_FREE_VALUE);
	ns_debug(ns_log_parser, 7, "acl %s", name);
	dprint_ip_match_list(ns_log_parser, iml, 2, "allow ", "deny ");
}

struct dst_key *
lookup_key(char *name) {
	symbol_value value;

	if (lookup_symbol(authtab, name, SYM_KEY, &value))
		return ((struct dst_key *)(value.pointer));
	return (NULL);
}

void
define_key(char *name, struct dst_key *dst_key) {
	symbol_value value;

	INSIST(name != NULL);
	INSIST(dst_key != NULL);

	value.pointer = dst_key;
	define_symbol(authtab, name, SYM_KEY, value, SYMBOL_FREE_VALUE);
	dprint_key_info(dst_key);
}

void
parse_configuration(const char *filename) {
	FILE *config_stream;

	config_stream = fopen(filename, "r");
	if (config_stream == NULL)
		ns_panic(ns_log_parser, 0, "can't open '%s'", filename);

	lexer_setup();
	parser_setup();
	lexer_begin_file(filename, config_stream);
	(void)yyparse();
	lexer_end_file();
	parser_cleanup();
}

void
parser_initialize(void) {
	lexer_initialize();
}

void
parser_shutdown(void) {
	if (authtab != NULL)
		free_symbol_table(authtab);
	lexer_shutdown();
}
#line 1216 "y.tab.c"
/* allocate initial stack */
#if defined(__STDC__) || defined(__cplusplus)
static int yyinitstack(struct yystack *sp)
#else
static int yyinitstack(sp)
    struct yystack *sp;
#endif
{
    int newsize;
    short *newss;
    YYSTYPE *newvs;

    newsize = YYINITSTACKSIZE;
    newss = (short *)malloc(newsize * sizeof *newss);
    newvs = (YYSTYPE *)malloc(newsize * sizeof *newvs);
    sp->ss = sp->ssp = newss;
    sp->vs = sp->vsp = newvs;
    if (newss == NULL || newvs == NULL) return -1;
    sp->stacksize = newsize;
    sp->sslim = newss + newsize - 1;
    return 0;
}

/* double stack size, up to YYMAXDEPTH */
#if defined(__STDC__) || defined(__cplusplus)
static int yygrowstack(struct yystack *sp)
#else
static int yygrowstack(sp)
    struct yystack *sp;
#endif
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = sp->stacksize) >= YYMAXDEPTH) return -1;
    if ((newsize *= 2) > YYMAXDEPTH) newsize = YYMAXDEPTH;
    i = sp->ssp - sp->ss;
    if ((newss = (short *)realloc(sp->ss, newsize * sizeof *newss)) == NULL)
        return -1;
    sp->ss = newss;
    sp->ssp = newss + i;
    if ((newvs = (YYSTYPE *)realloc(sp->vs, newsize * sizeof *newvs)) == NULL)
        return -1;
    sp->vs = newvs;
    sp->vsp = newvs + i;
    sp->stacksize = newsize;
    sp->sslim = newss + newsize - 1;
    return 0;
}

#define YYFREESTACK(sp) { free((sp)->ss); free((sp)->vs); }

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse()
{
    register int yym, yyn, yystate, yych;
    register YYSTYPE *yyvsp;
    YYSTYPE yyval;
    struct yystack yystk;
#if YYDEBUG
    register char *yys;
    extern char *getenv();

    if (yys = getenv("YYDEBUG"))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = yych = YYEMPTY;

    if (yyinitstack(&yystk)) goto yyoverflow;
    *yystk.ssp = yystate = 0;

yyloop:
    if (yyn = yydefred[yystate]) goto yyreduce;
    if (yych < 0)
    {
        if ((yych = YYLEX) < 0) yych = 0;
        yychar = yych;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yych <= YYMAXTOKEN) yys = yyname[yych];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yych, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yych) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yych)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yystk.ssp >= yystk.sslim && yygrowstack(&yystk))
            goto yyoverflow;
        *++yystk.ssp = yystate = yytable[yyn];
        *++yystk.vsp = yylval;
        yychar = yych = YYEMPTY;
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yych) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yych)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#ifdef lint
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#ifdef lint
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yystk.ssp]) &&
                    (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yystk.ssp, yytable[yyn]);
#endif
                if (yystk.ssp >= yystk.sslim && yygrowstack(&yystk))
                    goto yyoverflow;
                *++yystk.ssp = yystate = yytable[yyn];
                *++yystk.vsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yystk.ssp);
#endif
                if (yystk.ssp <= yystk.ss) goto yyabort;
                --yystk.ssp;
                --yystk.vsp;
            }
        }
    }
    else
    {
        if (yych == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yych <= YYMAXTOKEN) yys = yyname[yych];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yych, yys);
        }
#endif
        yychar = yych = YYEMPTY;
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyvsp = yystk.vsp; /* for speed in code under switch() */
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 1:
#line 241 "ns_parser.y"
{
		if (EMPTY(current_controls))
			ns_ctl_defaults(&current_controls);
		ns_ctl_install(&current_controls);
	}
break;
case 16:
#line 266 "ns_parser.y"
{ lexer_begin_file(yyvsp[-1].cp, NULL); }
break;
case 17:
#line 274 "ns_parser.y"
{
		if (seen_options)
			parser_error(0, "cannot redefine options");
		current_options = new_options();
	}
break;
case 18:
#line 280 "ns_parser.y"
{
		if (!seen_options)
			set_options(current_options, 0);
		else
			free_options(current_options);
		current_options = NULL;
		seen_options = 1;
	}
break;
case 22:
#line 296 "ns_parser.y"
{
		if (current_options->version != NULL)
			freestr(current_options->version);
		current_options->version = yyvsp[0].cp;
	}
break;
case 23:
#line 302 "ns_parser.y"
{
		if (current_options->directory != NULL)
			freestr(current_options->directory);
		current_options->directory = yyvsp[0].cp;
	}
break;
case 24:
#line 308 "ns_parser.y"
{
		if (current_options->named_xfer != NULL)
			freestr(current_options->named_xfer);
		current_options->named_xfer = yyvsp[0].cp;
	}
break;
case 25:
#line 314 "ns_parser.y"
{
		if (current_options->pid_filename != NULL)
			freestr(current_options->pid_filename);
		current_options->pid_filename = yyvsp[0].cp;
	}
break;
case 26:
#line 320 "ns_parser.y"
{
		if (current_options->stats_filename != NULL)
			freestr(current_options->stats_filename);
		current_options->stats_filename = yyvsp[0].cp;
	}
break;
case 27:
#line 326 "ns_parser.y"
{
		if (current_options->memstats_filename != NULL)
			freestr(current_options->memstats_filename);
		current_options->memstats_filename = yyvsp[0].cp;
	}
break;
case 28:
#line 332 "ns_parser.y"
{
		if (current_options->dump_filename != NULL)
			freestr(current_options->dump_filename);
		current_options->dump_filename = yyvsp[0].cp;
	}
break;
case 29:
#line 338 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_FAKE_IQUERY, yyvsp[0].num);
	}
break;
case 30:
#line 343 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_NORECURSE, !yyvsp[0].num);
	}
break;
case 31:
#line 348 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_NOFETCHGLUE, !yyvsp[0].num);
	}
break;
case 32:
#line 353 "ns_parser.y"
{
		set_global_boolean_option(current_options, 
			OPTION_NONOTIFY, !yyvsp[0].num);
	}
break;
case 33:
#line 358 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_HOSTSTATS, yyvsp[0].num);
	}
break;
case 34:
#line 363 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_DEALLOC_ON_EXIT, yyvsp[0].num);
	}
break;
case 35:
#line 368 "ns_parser.y"
{
		set_global_boolean_option(current_options, OPTION_USE_IXFR, yyvsp[0].num);
	}
break;
case 36:
#line 372 "ns_parser.y"
{
		set_global_boolean_option(current_options,
					  OPTION_MAINTAIN_IXFR_BASE, yyvsp[0].num);
	}
break;
case 37:
#line 377 "ns_parser.y"
{
		set_global_boolean_option(current_options,
					  OPTION_MAINTAIN_IXFR_BASE, yyvsp[0].num);
		set_global_boolean_option(current_options,
					  OPTION_NORFC2308_TYPE1, yyvsp[0].num);
		set_global_boolean_option(current_options,
					  OPTION_NONAUTH_NXDOMAIN, !yyvsp[0].num);
	}
break;
case 38:
#line 386 "ns_parser.y"
{
		set_global_boolean_option(current_options, OPTION_NONAUTH_NXDOMAIN,
				   !yyvsp[0].num);
	}
break;
case 39:
#line 391 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_MULTIPLE_CNAMES, yyvsp[0].num);
	}
break;
case 40:
#line 396 "ns_parser.y"
{
		current_options->check_names[yyvsp[-1].s_int] = (enum severity)yyvsp[0].s_int;
	}
break;
case 41:
#line 400 "ns_parser.y"
{
		set_global_boolean_option(current_options,
					  OPTION_USE_ID_POOL, yyvsp[0].num);
	}
break;
case 42:
#line 405 "ns_parser.y"
{
		set_global_boolean_option(current_options,
                        		  OPTION_NORFC2308_TYPE1, !yyvsp[0].num);
	}
break;
case 43:
#line 410 "ns_parser.y"
{
		char port_string[10];
		symbol_value value;

		(void)sprintf(port_string, "%u", yyvsp[-3].us_int);
		if (lookup_symbol(symtab, port_string, SYM_PORT, NULL))
			parser_error(0,
				     "cannot redefine listen-on for port %u",
				     ntohs(yyvsp[-3].us_int));
		else {
			add_listen_on(current_options, yyvsp[-3].us_int, yyvsp[-1].iml);
			value.pointer = NULL;
			define_symbol(symtab, savestr(port_string, 1),
				      SYM_PORT, value, SYMBOL_FREE_KEY);
		}

	}
break;
case 45:
#line 429 "ns_parser.y"
{
		if (current_options->fwdtab) {
			free_forwarders(current_options->fwdtab);
			current_options->fwdtab = NULL;
		}
	}
break;
case 48:
#line 438 "ns_parser.y"
{
		current_options->axfr_src = yyvsp[0].ip_addr;
	}
break;
case 49:
#line 442 "ns_parser.y"
{
		if (current_options->query_acl) {
			parser_warning(0,
			      "options allow-query acl already set; skipping");
			free_ip_match_list(yyvsp[-1].iml);
		} else 
			current_options->query_acl = yyvsp[-1].iml;
	}
break;
case 50:
#line 451 "ns_parser.y"
{
		if (current_options->recursion_acl) {
			parser_warning(0,
			      "options allow-recursion acl already set; skipping");
			free_ip_match_list(yyvsp[-1].iml);
		} else
			current_options->recursion_acl = yyvsp[-1].iml;
	}
break;
case 51:
#line 460 "ns_parser.y"
{
		if (current_options->transfer_acl) {
			parser_warning(0,
			   "options allow-transfer acl already set; skipping");
			free_ip_match_list(yyvsp[-1].iml);
		} else 
			current_options->transfer_acl = yyvsp[-1].iml;
	}
break;
case 52:
#line 469 "ns_parser.y"
{
		if (current_options->sortlist) {
			parser_warning(0,
			      "options sortlist already set; skipping");
			free_ip_match_list(yyvsp[-1].iml);
		} else
			current_options->sortlist = yyvsp[-1].iml;
	}
break;
case 53:
#line 478 "ns_parser.y"
{
		if (current_options->also_notify) {
			parser_warning(0,
			    "duplicate also-notify clause: overwriting");
			free_also_notify(current_options);
			current_options->also_notify = NULL;
		}
	}
break;
case 55:
#line 488 "ns_parser.y"
{
		if (current_options->blackhole_acl) {
			parser_warning(0,
			      "options blackhole already set; skipping");
			free_ip_match_list(yyvsp[-1].iml);
		} else
			current_options->blackhole_acl = yyvsp[-1].iml;
	}
break;
case 56:
#line 497 "ns_parser.y"
{
		if (current_options->topology) {
			parser_warning(0,
			      "options topology already set; skipping");
			free_ip_match_list(yyvsp[-1].iml);
		} else
			current_options->topology = yyvsp[-1].iml;
	}
break;
case 57:
#line 506 "ns_parser.y"
{
		/* To get around the $$ = $1 default rule. */
	}
break;
case 59:
#line 511 "ns_parser.y"
{
		current_options->transfer_format = yyvsp[0].axfr_fmt;
	}
break;
case 60:
#line 515 "ns_parser.y"
{
		current_options->max_transfer_time_in = yyvsp[0].num * 60;
	}
break;
case 61:
#line 519 "ns_parser.y"
{
		current_options->serial_queries = yyvsp[0].num;
	}
break;
case 62:
#line 523 "ns_parser.y"
{
		current_options->clean_interval = yyvsp[0].num * 60;
	}
break;
case 63:
#line 527 "ns_parser.y"
{
		current_options->interface_interval = yyvsp[0].num * 60;
	}
break;
case 64:
#line 531 "ns_parser.y"
{
		current_options->stats_interval = yyvsp[0].num * 60;
	}
break;
case 65:
#line 535 "ns_parser.y"
{
		current_options->max_log_size_ixfr = yyvsp[0].num;
	}
break;
case 66:
#line 539 "ns_parser.y"
{
		current_options->max_ncache_ttl = yyvsp[0].num;
	}
break;
case 67:
#line 543 "ns_parser.y"
{
		current_options->lame_ttl = yyvsp[0].num;
	}
break;
case 68:
#line 547 "ns_parser.y"
{
		current_options->heartbeat_interval = yyvsp[0].num * 60;
	}
break;
case 69:
#line 551 "ns_parser.y"
{
		set_global_boolean_option(current_options,
                                          OPTION_NODIALUP, !yyvsp[0].num);
	}
break;
case 70:
#line 556 "ns_parser.y"
{
		if (current_options->ordering)
			free_rrset_order_list(current_options->ordering);
		current_options->ordering = yyvsp[-1].rol;
	}
break;
case 71:
#line 562 "ns_parser.y"
{
		set_global_boolean_option(current_options,
					  OPTION_TREAT_CR_AS_SPACE, yyvsp[0].num);
	}
break;
case 72:
#line 567 "ns_parser.y"
{
		if (yyvsp[0].num >= 1)
			current_options->minroots = yyvsp[0].num;
	}
break;
case 78:
#line 587 "ns_parser.y"
{
		ns_ctl_add(&current_controls, ns_ctl_new_inet(yyvsp[-6].ip_addr, yyvsp[-4].us_int, yyvsp[-1].iml));
	}
break;
case 79:
#line 591 "ns_parser.y"
{
		ns_ctl_add(&current_controls, ns_ctl_new_unix(yyvsp[-6].cp, yyvsp[-4].num, yyvsp[-2].num, yyvsp[0].num));
	}
break;
case 81:
#line 598 "ns_parser.y"
{
		rrset_order_list rol;

		rol = new_rrset_order_list();
		if (yyvsp[-1].roe != NULL) {
			add_to_rrset_order_list(rol, yyvsp[-1].roe);
		}
		
		yyval.rol = rol;
	}
break;
case 82:
#line 609 "ns_parser.y"
{
		if (yyvsp[-1].roe != NULL) {
			add_to_rrset_order_list(yyvsp[-2].rol, yyvsp[-1].roe);
		}
		yyval.rol = yyvsp[-2].rol;
	}
break;
case 83:
#line 618 "ns_parser.y"
{
		yyval.s_int = C_ANY;
	}
break;
case 84:
#line 622 "ns_parser.y"
{
		symbol_value value;

		if (lookup_symbol(constants, yyvsp[0].cp, SYM_CLASS, &value))
			yyval.s_int = value.integer;
		else {
			parser_error(0, "unknown class '%s'; using ANY", yyvsp[0].cp);
			yyval.s_int = C_ANY;
		}
		freestr(yyvsp[0].cp);
	}
break;
case 85:
#line 636 "ns_parser.y"
{
		yyval.s_int = ns_t_any;
	}
break;
case 86:
#line 640 "ns_parser.y"
{
		int success;

		if (strcmp(yyvsp[0].cp, "*") == 0) {
			yyval.s_int = ns_t_any;
		} else {
			yyval.s_int = __sym_ston(__p_type_syms, yyvsp[0].cp, &success);
			if (success == 0) {
				yyval.s_int = ns_t_any;
				parser_error(0,
					     "unknown type '%s'; assuming ANY",
					     yyvsp[0].cp);
			}
		}
		freestr(yyvsp[0].cp);
	}
break;
case 87:
#line 658 "ns_parser.y"
{
		yyval.cp = savestr("*", 1);
	}
break;
case 88:
#line 662 "ns_parser.y"
{
		if (strcmp(".",yyvsp[0].cp) == 0 || strcmp("*.",yyvsp[0].cp) == 0) {
			yyval.cp = savestr("*", 1);
			freestr(yyvsp[0].cp);
		} else {
			yyval.cp = yyvsp[0].cp ;
		}
		/* XXX Should do any more name validation here? */
	}
break;
case 89:
#line 674 "ns_parser.y"
{
		enum ordering o;

		if (strlen(yyvsp[0].cp) == 0) {
			parser_error(0, "null order name");
			yyval.roe = NULL ;
		} else {
			o = lookup_ordering(yyvsp[0].cp);
			if (o == unknown_order) {
				o = (enum ordering)DEFAULT_ORDERING;
				parser_error(0,
					     "invalid order name '%s'; using %s",
					     yyvsp[0].cp, p_order(o));
			}
			
			freestr(yyvsp[0].cp);
			
			yyval.roe = new_rrset_order_element(yyvsp[-4].s_int, yyvsp[-3].s_int, yyvsp[-2].cp, o);
		}
	}
break;
case 90:
#line 697 "ns_parser.y"
{
		yyval.axfr_fmt = axfr_one_answer;
	}
break;
case 91:
#line 701 "ns_parser.y"
{
		yyval.axfr_fmt = axfr_many_answers;
	}
break;
case 92:
#line 706 "ns_parser.y"
{ yyval.ip_addr = yyvsp[0].ip_addr; }
break;
case 93:
#line 707 "ns_parser.y"
{ yyval.ip_addr.s_addr = htonl(INADDR_ANY); }
break;
case 94:
#line 710 "ns_parser.y"
{ yyval.us_int = yyvsp[0].us_int; }
break;
case 95:
#line 711 "ns_parser.y"
{ yyval.us_int = htons(0); }
break;
case 96:
#line 715 "ns_parser.y"
{
		current_options->query_source.sin_addr = yyvsp[0].ip_addr;
	}
break;
case 97:
#line 721 "ns_parser.y"
{
		current_options->query_source.sin_port = yyvsp[0].us_int;
	}
break;
case 102:
#line 732 "ns_parser.y"
{ yyval.us_int = htons(NS_DEFAULTPORT); }
break;
case 103:
#line 733 "ns_parser.y"
{ yyval.us_int = yyvsp[0].us_int; }
break;
case 104:
#line 736 "ns_parser.y"
{ yyval.us_int = htons(0); }
break;
case 105:
#line 737 "ns_parser.y"
{ yyval.us_int = yyvsp[0].us_int; }
break;
case 106:
#line 742 "ns_parser.y"
{ 
		yyval.num = 1;	
	}
break;
case 107:
#line 746 "ns_parser.y"
{ 
		yyval.num = 1;	
	}
break;
case 108:
#line 750 "ns_parser.y"
{ 
		yyval.num = 0;	
	}
break;
case 109:
#line 754 "ns_parser.y"
{ 
		yyval.num = 0;	
	}
break;
case 110:
#line 758 "ns_parser.y"
{ 
		if (yyvsp[0].num == 1 || yyvsp[0].num == 0) {
			yyval.num = yyvsp[0].num;
		} else {
			parser_warning(0,
				       "number should be 0 or 1; assuming 1");
			yyval.num = 1;
		}
	}
break;
case 111:
#line 770 "ns_parser.y"
{
		yyval.s_int = primary_trans;
	}
break;
case 112:
#line 774 "ns_parser.y"
{
		yyval.s_int = secondary_trans;
	}
break;
case 113:
#line 778 "ns_parser.y"
{
		yyval.s_int = response_trans;
	}
break;
case 114:
#line 784 "ns_parser.y"
{
		yyval.s_int = warn;
	}
break;
case 115:
#line 788 "ns_parser.y"
{
		yyval.s_int = fail;
	}
break;
case 116:
#line 792 "ns_parser.y"
{
		yyval.s_int = ignore;
	}
break;
case 117:
#line 798 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_FORWARD_ONLY, 1);
	}
break;
case 118:
#line 803 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_FORWARD_ONLY, 0);
	}
break;
case 119:
#line 808 "ns_parser.y"
{
		parser_warning(0, "forward if-no-answer is unimplemented");
	}
break;
case 120:
#line 812 "ns_parser.y"
{
		parser_warning(0, "forward if-no-domain is unimplemented");
	}
break;
case 121:
#line 818 "ns_parser.y"
{
		current_options->data_size = yyvsp[0].ul_int;
	}
break;
case 122:
#line 822 "ns_parser.y"
{
		current_options->stack_size = yyvsp[0].ul_int;
	}
break;
case 123:
#line 826 "ns_parser.y"
{
		current_options->core_size = yyvsp[0].ul_int;
	}
break;
case 124:
#line 830 "ns_parser.y"
{
		current_options->files = yyvsp[0].ul_int;
	}
break;
case 125:
#line 836 "ns_parser.y"
{
		u_long result;

		if (unit_to_ulong(yyvsp[0].cp, &result))
			yyval.ul_int = result;
		else {
			parser_error(0, "invalid unit string '%s'", yyvsp[0].cp);
			/* 0 means "use default" */
			yyval.ul_int = 0;
		}
		freestr(yyvsp[0].cp);
	}
break;
case 126:
#line 849 "ns_parser.y"
{	
		yyval.ul_int = (u_long)yyvsp[0].num;
	}
break;
case 127:
#line 853 "ns_parser.y"
{
		yyval.ul_int = 0;
	}
break;
case 128:
#line 857 "ns_parser.y"
{
		yyval.ul_int = ULONG_MAX;
	}
break;
case 129:
#line 863 "ns_parser.y"
{
		current_options->transfers_in = (u_long) yyvsp[0].num;
	}
break;
case 130:
#line 867 "ns_parser.y"
{
		current_options->transfers_out = (u_long) yyvsp[0].num;
	}
break;
case 131:
#line 871 "ns_parser.y"
{
		current_options->transfers_per_ns = (u_long) yyvsp[0].num;
	}
break;
case 134:
#line 881 "ns_parser.y"
{
		/* nothing */
	}
break;
case 135:
#line 885 "ns_parser.y"
{
		/* nothing */
	}
break;
case 136:
#line 891 "ns_parser.y"
{
	  	add_global_forwarder(current_options, yyvsp[0].ip_addr);
	}
break;
case 139:
#line 901 "ns_parser.y"
{
		/* nothing */
	}
break;
case 140:
#line 905 "ns_parser.y"
{
		/* nothing */
	}
break;
case 141:
#line 911 "ns_parser.y"
{
	  	add_global_also_notify(current_options, yyvsp[0].ip_addr);
	}
break;
case 142:
#line 921 "ns_parser.y"
{
		current_logging = begin_logging();
	}
break;
case 143:
#line 925 "ns_parser.y"
{
		end_logging(current_logging, 1);
		current_logging = NULL;
	}
break;
case 147:
#line 937 "ns_parser.y"
{
		current_category = yyvsp[0].s_int;
	}
break;
case 149:
#line 942 "ns_parser.y"
{
		chan_type = log_null;
		chan_flags = 0;
		chan_level = log_info;
	}
break;
case 150:
#line 948 "ns_parser.y"
{
		log_channel current_channel = NULL;

		if (lookup_channel(yyvsp[-4].cp) != NULL) {
			parser_error(0, "can't redefine channel '%s'", yyvsp[-4].cp);
			freestr(yyvsp[-4].cp);
		} else {
			switch (chan_type) {
			case log_file:
				current_channel =
					log_new_file_channel(chan_flags,
							     chan_level,
							     chan_name, NULL,
							     chan_versions,
							     chan_max_size);
				freestr(chan_name);
				chan_name = NULL;
				break;
			case log_syslog:
				current_channel =
					log_new_syslog_channel(chan_flags,
							       chan_level,
							       chan_facility);
				break;
			case log_null:
				current_channel = log_new_null_channel();
				break;
			default:
				ns_panic(ns_log_parser, 1,
					 "unknown channel type: %d",
					 chan_type);
			}
			if (current_channel == NULL)
				ns_panic(ns_log_parser, 0,
					 "couldn't create channel");
			define_channel(yyvsp[-4].cp, current_channel);
		}
	}
break;
case 151:
#line 989 "ns_parser.y"
{
		symbol_value value;

		if (lookup_symbol(constants, yyvsp[0].cp, SYM_LOGGING, &value)) {
			chan_level = value.integer;
		} else {
			parser_error(0, "unknown severity '%s'", yyvsp[0].cp);
			chan_level = log_debug(99);
		}
		freestr(yyvsp[0].cp);
	}
break;
case 152:
#line 1001 "ns_parser.y"
{
		chan_level = log_debug(1);
	}
break;
case 153:
#line 1005 "ns_parser.y"
{
		chan_level = yyvsp[0].num;
	}
break;
case 154:
#line 1009 "ns_parser.y"
{
		chan_level = 0;
		chan_flags |= LOG_USE_CONTEXT_LEVEL|LOG_REQUIRE_DEBUG;
	}
break;
case 155:
#line 1016 "ns_parser.y"
{
		chan_versions = yyvsp[0].num;
	}
break;
case 156:
#line 1020 "ns_parser.y"
{
		chan_versions = LOG_MAX_VERSIONS;
	}
break;
case 157:
#line 1026 "ns_parser.y"
{
		chan_max_size = yyvsp[0].ul_int;
	}
break;
case 158:
#line 1032 "ns_parser.y"
{
		chan_versions = 0;
		chan_max_size = ULONG_MAX;
	}
break;
case 159:
#line 1037 "ns_parser.y"
{
		chan_max_size = ULONG_MAX;
	}
break;
case 160:
#line 1041 "ns_parser.y"
{
		chan_versions = 0;
	}
break;
case 163:
#line 1049 "ns_parser.y"
{
		chan_flags |= LOG_CLOSE_STREAM;
		chan_type = log_file;
		chan_name = yyvsp[-1].cp;
	}
break;
case 164:
#line 1057 "ns_parser.y"
{ yyval.cp = yyvsp[0].cp; }
break;
case 165:
#line 1058 "ns_parser.y"
{ yyval.cp = savestr("syslog", 1); }
break;
case 166:
#line 1061 "ns_parser.y"
{ yyval.s_int = LOG_DAEMON; }
break;
case 167:
#line 1063 "ns_parser.y"
{
		symbol_value value;

		if (lookup_symbol(constants, yyvsp[0].cp, SYM_SYSLOG, &value)) {
			yyval.s_int = value.integer;
		} else {
			parser_error(0, "unknown facility '%s'", yyvsp[0].cp);
			yyval.s_int = LOG_DAEMON;
		}
		freestr(yyvsp[0].cp);
	}
break;
case 168:
#line 1077 "ns_parser.y"
{
		chan_type = log_syslog;
		chan_facility = yyvsp[0].s_int;
	}
break;
case 169:
#line 1083 "ns_parser.y"
{ /* nothing to do */ }
break;
case 170:
#line 1084 "ns_parser.y"
{ /* nothing to do */ }
break;
case 171:
#line 1086 "ns_parser.y"
{
		chan_type = log_null;
	}
break;
case 172:
#line 1089 "ns_parser.y"
{ /* nothing to do */ }
break;
case 173:
#line 1091 "ns_parser.y"
{
		if (yyvsp[0].num)
			chan_flags |= LOG_TIMESTAMP;
		else
			chan_flags &= ~LOG_TIMESTAMP;
	}
break;
case 174:
#line 1098 "ns_parser.y"
{
		if (yyvsp[0].num)
			chan_flags |= LOG_PRINT_CATEGORY;
		else
			chan_flags &= ~LOG_PRINT_CATEGORY;
	}
break;
case 175:
#line 1105 "ns_parser.y"
{
		if (yyvsp[0].num)
			chan_flags |= LOG_PRINT_LEVEL;
		else
			chan_flags &= ~LOG_PRINT_LEVEL;
	}
break;
case 180:
#line 1119 "ns_parser.y"
{ yyval.cp = savestr("null", 1); }
break;
case 181:
#line 1123 "ns_parser.y"
{
		log_channel channel;
		symbol_value value;

		if (current_category >= 0) {
			channel = lookup_channel(yyvsp[0].cp);
			if (channel != NULL) {
				add_log_channel(current_logging,
						current_category, channel);
			} else
				parser_error(0, "unknown channel '%s'", yyvsp[0].cp);
		}
		freestr(yyvsp[0].cp);
	}
break;
case 186:
#line 1145 "ns_parser.y"
{ yyval.cp = savestr("default", 1); }
break;
case 187:
#line 1146 "ns_parser.y"
{ yyval.cp = savestr("notify", 1); }
break;
case 188:
#line 1150 "ns_parser.y"
{
		symbol_value value;

		if (lookup_symbol(constants, yyvsp[0].cp, SYM_CATEGORY, &value))
			yyval.s_int = value.integer;
		else {
			parser_error(0, "invalid logging category '%s'",
				     yyvsp[0].cp);
			yyval.s_int = -1;
		}
		freestr(yyvsp[0].cp);
	}
break;
case 189:
#line 1169 "ns_parser.y"
{
		const char *ip_printable;
		symbol_value value;
		
		ip_printable = inet_ntoa(yyvsp[0].ip_addr);
		value.pointer = NULL;
		if (lookup_symbol(symtab, ip_printable, SYM_SERVER, NULL))
			seen_server = 1;
		else
			seen_server = 0;
		if (seen_server)
			parser_error(0, "cannot redefine server '%s'", 
				     ip_printable);
		else
			define_symbol(symtab, savestr(ip_printable, 1),
				      SYM_SERVER, value,
				      SYMBOL_FREE_KEY);
		current_server = begin_server(yyvsp[0].ip_addr);
	}
break;
case 190:
#line 1189 "ns_parser.y"
{
		end_server(current_server, !seen_server);
	}
break;
case 193:
#line 1199 "ns_parser.y"
{
		set_server_option(current_server, SERVER_INFO_BOGUS, yyvsp[0].num);
	}
break;
case 194:
#line 1203 "ns_parser.y"
{
		set_server_option(current_server, SERVER_INFO_SUPPORT_IXFR, yyvsp[0].num);
	}
break;
case 195:
#line 1207 "ns_parser.y"
{
		set_server_transfers(current_server, (int)yyvsp[0].num);
	}
break;
case 196:
#line 1211 "ns_parser.y"
{
		set_server_transfer_format(current_server, yyvsp[0].axfr_fmt);
	}
break;
case 199:
#line 1223 "ns_parser.y"
{
		ip_match_list iml;
		
		iml = new_ip_match_list();
		if (yyvsp[-1].ime != NULL)
			add_to_ip_match_list(iml, yyvsp[-1].ime);
		yyval.iml = iml;
	}
break;
case 200:
#line 1232 "ns_parser.y"
{
		if (yyvsp[-1].ime != NULL)
			add_to_ip_match_list(yyvsp[-2].iml, yyvsp[-1].ime);
		yyval.iml = yyvsp[-2].iml;
	}
break;
case 202:
#line 1241 "ns_parser.y"
{
		if (yyvsp[0].ime != NULL)
			ip_match_negate(yyvsp[0].ime);
		yyval.ime = yyvsp[0].ime;
	}
break;
case 203:
#line 1247 "ns_parser.y"
{
		char *key_name;
		struct dst_key *dst_key;

		key_name = canonical_name(yyvsp[0].cp);
		if (key_name == NULL) {
			parser_error(0, "can't make key name '%s' canonical",
				     yyvsp[0].cp);
			key_name = savestr("__bad_key__", 1);
		}
		dst_key = find_key(key_name, NULL);
		if (dst_key == NULL) {
			parser_error(0, "key \"%s\" not found", key_name);
			yyval.ime = NULL;
		}
		else
			yyval.ime = new_ip_match_key(dst_key);
	}
break;
case 204:
#line 1268 "ns_parser.y"
{
		yyval.ime = new_ip_match_pattern(yyvsp[0].ip_addr, 32);
	}
break;
case 205:
#line 1272 "ns_parser.y"
{
		if (yyvsp[0].num < 0 || yyvsp[0].num > 32) {
			parser_error(0, "mask bits out of range; skipping");
			yyval.ime = NULL;
		} else {
			yyval.ime = new_ip_match_pattern(yyvsp[-2].ip_addr, yyvsp[0].num);
			if (yyval.ime == NULL)
				parser_error(0, 
					   "address/mask mismatch; skipping");
		}
	}
break;
case 206:
#line 1284 "ns_parser.y"
{
		struct in_addr ia;

		if (yyvsp[-2].num > 255) {
			parser_error(0, "address out of range; skipping");
			yyval.ime = NULL;
		} else {
			if (yyvsp[0].num < 0 || yyvsp[0].num > 32) {
				parser_error(0,
					"mask bits out of range; skipping");
					yyval.ime = NULL;
			} else {
				ia.s_addr = htonl((yyvsp[-2].num & 0xff) << 24);
				yyval.ime = new_ip_match_pattern(ia, yyvsp[0].num);
				if (yyval.ime == NULL)
					parser_error(0, 
					   "address/mask mismatch; skipping");
			}
		}
	}
break;
case 208:
#line 1306 "ns_parser.y"
{
		char name[256];

		/*
		 * We want to be able to clean up this iml later so
		 * we give it a name and treat it like any other acl.
		 */
		sprintf(name, "__internal_%p", yyvsp[-1].iml);
		define_acl(savestr(name, 1), yyvsp[-1].iml);
  		yyval.ime = new_ip_match_indirect(yyvsp[-1].iml);
	}
break;
case 209:
#line 1320 "ns_parser.y"
{
		ip_match_list iml;

		iml = lookup_acl(yyvsp[0].cp);
		if (iml == NULL) {
			parser_error(0, "unknown ACL '%s'", yyvsp[0].cp);
			yyval.ime = NULL;
		} else
			yyval.ime = new_ip_match_indirect(iml);
		freestr(yyvsp[0].cp);
	}
break;
case 210:
#line 1338 "ns_parser.y"
{
		struct dst_key *dst_key;
		char *key_name;

		key_name = canonical_name(yyvsp[0].cp);
		if (key_name == NULL) {
			parser_error(0, "can't make key name '%s' canonical",
				     yyvsp[0].cp);
			yyval.keyi = NULL;
		} else {
			dst_key = lookup_key(key_name);
			if (dst_key == NULL) {
				parser_error(0, "unknown key '%s'", key_name);
				yyval.keyi = NULL;
			} else
				yyval.keyi = dst_key;
			freestr(key_name);
		}
		freestr(yyvsp[0].cp);
	}
break;
case 211:
#line 1361 "ns_parser.y"
{
		if (yyvsp[0].keyi == NULL)
			parser_error(0, "empty key not added to server list ");
		else
			add_server_key_info(current_server, yyvsp[0].keyi);
	}
break;
case 215:
#line 1375 "ns_parser.y"
{
		current_algorithm = NULL;
		current_secret = NULL;
	}
break;
case 216:
#line 1380 "ns_parser.y"
{
		struct dst_key *dst_key;
		char *key_name;

		key_name = canonical_name(yyvsp[-3].cp);
		if (key_name == NULL) {
			parser_error(0, "can't make key name '%s' canonical",
				     yyvsp[-3].cp);
		} else if (lookup_key(key_name) != NULL) {
			parser_error(0, "can't redefine key '%s'", key_name);
			freestr(key_name);
		} else {
			if (current_algorithm == NULL ||
			    current_secret == NULL)  {
				parser_error(0, "skipping bad key '%s'",
					     key_name);
				freestr(key_name);
			} else {
				dst_key = new_key_info(key_name,
						       current_algorithm,
						       current_secret);
				if (dst_key != NULL) {
					define_key(key_name, dst_key);
					if (secretkey_info == NULL)
						secretkey_info =
							new_key_info_list();
					add_to_key_info_list(secretkey_info,
							     dst_key);
				}
			}
		}
		freestr(yyvsp[-3].cp);
	}
break;
case 217:
#line 1416 "ns_parser.y"
{
		current_algorithm = yyvsp[-1].cp;
		current_secret = yyvsp[0].cp;
	}
break;
case 218:
#line 1421 "ns_parser.y"
{
		current_algorithm = yyvsp[0].cp;
		current_secret = yyvsp[-1].cp;
	}
break;
case 219:
#line 1426 "ns_parser.y"
{
		current_algorithm = NULL;
		current_secret = NULL;
	}
break;
case 220:
#line 1432 "ns_parser.y"
{ yyval.cp = yyvsp[-1].cp; }
break;
case 221:
#line 1435 "ns_parser.y"
{ yyval.cp = yyvsp[-1].cp; }
break;
case 222:
#line 1443 "ns_parser.y"
{
		if (lookup_acl(yyvsp[-3].cp) != NULL) {
			parser_error(0, "can't redefine ACL '%s'", yyvsp[-3].cp);
			freestr(yyvsp[-3].cp);
		} else
			define_acl(yyvsp[-3].cp, yyvsp[-1].iml);
	}
break;
case 223:
#line 1457 "ns_parser.y"
{
		int sym_type;
		symbol_value value;
		char *zone_name;

		if (!seen_options)
			parser_error(0,
             "no options statement before first zone; using previous/default");
		sym_type = SYM_ZONE | (yyvsp[0].num & 0xffff);
		value.pointer = NULL;
		zone_name = canonical_name(yyvsp[-1].cp);
		if (zone_name == NULL) {
			parser_error(0, "can't make zone name '%s' canonical",
				     yyvsp[-1].cp);
			should_install = 0;
			zone_name = savestr("__bad_zone__", 1);
		} else {
			if (lookup_symbol(symtab, zone_name, sym_type, NULL)) {
				should_install = 0;
				parser_error(0,
					"cannot redefine zone '%s' class %s",
					     *zone_name ? zone_name : ".",
					     p_class(yyvsp[0].num));
			} else {
				should_install = 1;
				define_symbol(symtab, savestr(zone_name, 1),
					      sym_type, value,
					      SYMBOL_FREE_KEY);
			}
		}
		freestr(yyvsp[-1].cp);
		current_zone = begin_zone(zone_name, yyvsp[0].num); 
	}
break;
case 224:
#line 1491 "ns_parser.y"
{
		end_zone(current_zone, should_install);
	}
break;
case 227:
#line 1501 "ns_parser.y"
{
		yyval.num = C_IN;
	}
break;
case 228:
#line 1505 "ns_parser.y"
{
		symbol_value value;

		if (lookup_symbol(constants, yyvsp[0].cp, SYM_CLASS, &value))
			yyval.num = value.integer;
		else {
			/* the zone validator will give the error */
			yyval.num = C_NONE;
		}
		freestr(yyvsp[0].cp);
	}
break;
case 229:
#line 1519 "ns_parser.y"
{
		yyval.s_int = Z_MASTER;
	}
break;
case 230:
#line 1523 "ns_parser.y"
{
		yyval.s_int = Z_SLAVE;
	}
break;
case 231:
#line 1527 "ns_parser.y"
{
		yyval.s_int = Z_HINT;
	}
break;
case 232:
#line 1531 "ns_parser.y"
{
		yyval.s_int = Z_STUB;
	}
break;
case 233:
#line 1535 "ns_parser.y"
{
		yyval.s_int = Z_FORWARD;
	}
break;
case 236:
#line 1545 "ns_parser.y"
{
		if (!set_zone_type(current_zone, yyvsp[0].s_int))
			parser_warning(0, "zone type already set; skipping");
	}
break;
case 237:
#line 1550 "ns_parser.y"
{
		if (!set_zone_filename(current_zone, yyvsp[0].cp))
			parser_warning(0,
				       "zone filename already set; skipping");
	}
break;
case 238:
#line 1556 "ns_parser.y"
{
                if (!set_zone_ixfr_file(current_zone, yyvsp[0].cp))
                        parser_warning(0,
                                       "zone ixfr data base already set; skipping");
    }
break;
case 239:
#line 1562 "ns_parser.y"
{
                if (!set_zone_ixfr_tmp(current_zone, yyvsp[0].cp))
                        parser_warning(0,
                                       "zone ixfr temp filename already set; skipping");
    }
break;
case 240:
#line 1568 "ns_parser.y"
{
		set_zone_master_port(current_zone, yyvsp[-3].us_int);
	}
break;
case 241:
#line 1572 "ns_parser.y"
{
		set_zone_transfer_source(current_zone, yyvsp[0].ip_addr);
	}
break;
case 242:
#line 1576 "ns_parser.y"
{
		if (!set_zone_checknames(current_zone, (enum severity)yyvsp[0].s_int))
			parser_warning(0,
	                              "zone checknames already set; skipping");
	}
break;
case 243:
#line 1582 "ns_parser.y"
{
		if (!set_zone_update_acl(current_zone, yyvsp[-1].iml))
			parser_warning(0,
				      "zone update acl already set; skipping");
	}
break;
case 244:
#line 1588 "ns_parser.y"
{
		if (!set_zone_query_acl(current_zone, yyvsp[-1].iml))
			parser_warning(0,
				      "zone query acl already set; skipping");
	}
break;
case 245:
#line 1594 "ns_parser.y"
{
		if (!set_zone_transfer_acl(current_zone, yyvsp[-1].iml))
			parser_warning(0,
				    "zone transfer acl already set; skipping");
	}
break;
case 247:
#line 1601 "ns_parser.y"
{
		struct zoneinfo *zp = current_zone.opaque;
		if (zp->z_fwdtab) {
                	free_forwarders(zp->z_fwdtab);
			zp->z_fwdtab = NULL;
		}

	}
break;
case 249:
#line 1611 "ns_parser.y"
{
		if (!set_zone_transfer_time_in(current_zone, yyvsp[0].num*60))
			parser_warning(0,
		       "zone max transfer time (in) already set; skipping");
	}
break;
case 250:
#line 1617 "ns_parser.y"
{
		set_zone_max_log_size_ixfr(current_zone, yyvsp[0].num);
        }
break;
case 251:
#line 1621 "ns_parser.y"
{
		set_zone_notify(current_zone, yyvsp[0].num);
	}
break;
case 252:
#line 1625 "ns_parser.y"
{
		set_zone_maintain_ixfr_base(current_zone, yyvsp[0].num);
	}
break;
case 253:
#line 1629 "ns_parser.y"
{
		/* flags proto alg key */
		set_zone_pubkey(current_zone, yyvsp[-3].num, yyvsp[-2].num, yyvsp[-1].num, yyvsp[0].cp);
	}
break;
case 254:
#line 1634 "ns_parser.y"
{
		/* flags proto alg key */
		char *endp;
		int flags = (int) strtol(yyvsp[-3].cp, &endp, 0);
		if (*endp != '\0')
			ns_panic(ns_log_parser, 1,
				 "Invalid flags string: %s", yyvsp[-3].cp);
		set_zone_pubkey(current_zone, flags, yyvsp[-2].num, yyvsp[-1].num, yyvsp[0].cp);

	}
break;
case 256:
#line 1646 "ns_parser.y"
{
		 set_zone_dialup(current_zone, yyvsp[0].num);
	}
break;
case 258:
#line 1653 "ns_parser.y"
{
		/* nothing */
	}
break;
case 259:
#line 1657 "ns_parser.y"
{
		/* nothing */
	}
break;
case 260:
#line 1663 "ns_parser.y"
{
	  	add_zone_master(current_zone, yyvsp[0].ip_addr);
	}
break;
case 263:
#line 1673 "ns_parser.y"
{
		/* nothing */
	}
break;
case 264:
#line 1677 "ns_parser.y"
{
		/* nothing */
	}
break;
case 265:
#line 1683 "ns_parser.y"
{
	  	add_zone_notify(current_zone, yyvsp[0].ip_addr);
	}
break;
case 266:
#line 1689 "ns_parser.y"
{
		set_zone_boolean_option(current_zone, OPTION_FORWARD_ONLY, 1);
	}
break;
case 267:
#line 1693 "ns_parser.y"
{
		set_zone_boolean_option(current_zone, OPTION_FORWARD_ONLY, 0);
	}
break;
case 268:
#line 1699 "ns_parser.y"
{
		set_zone_forward(current_zone);
	}
break;
case 270:
#line 1706 "ns_parser.y"
{
		/* nothing */
	}
break;
case 271:
#line 1710 "ns_parser.y"
{
		/* nothing */
	}
break;
case 272:
#line 1716 "ns_parser.y"
{
	  	add_zone_forwarder(current_zone, yyvsp[0].ip_addr);
	}
break;
case 273:
#line 1726 "ns_parser.y"
{
	}
break;
case 274:
#line 1730 "ns_parser.y"
{
		/* nothing */
	}
break;
case 275:
#line 1734 "ns_parser.y"
{
		/* nothing */
	}
break;
case 276:
#line 1739 "ns_parser.y"
{
		/* name flags proto alg key */
		set_trusted_key(yyvsp[-4].cp, yyvsp[-3].num, yyvsp[-2].num, yyvsp[-1].num, yyvsp[0].cp);
	}
break;
case 277:
#line 1744 "ns_parser.y"
{
		/* name flags proto alg key */
		char *endp;
		int flags = (int) strtol(yyvsp[-3].cp, &endp, 0);
		if (*endp != '\0')
			ns_panic(ns_log_parser, 1,
				 "Invalid flags string: %s", yyvsp[-3].cp);
		set_trusted_key(yyvsp[-4].cp, flags, yyvsp[-2].num, yyvsp[-1].num, yyvsp[0].cp);
	}
break;
case 278:
#line 1760 "ns_parser.y"
{
		if (yyvsp[0].num < 0 || yyvsp[0].num > 65535) {
		  	parser_warning(0, 
			  "invalid IP port number '%d'; setting port to 0",
			               yyvsp[0].num);
			yyvsp[0].num = 0;
		} else
			yyval.us_int = htons(yyvsp[0].num);
	}
break;
#line 3093 "y.tab.c"
    }
    yystk.ssp -= yym;
    yystate = *yystk.ssp;
    yystk.vsp -= yym;
    yym = yylhs[yyn];
    yych = yychar;
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yystk.ssp = YYFINAL;
        *++yystk.vsp = yyval;
        if (yych < 0)
        {
            if ((yych = YYLEX) < 0) yych = 0;
            yychar = yych;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yych <= YYMAXTOKEN) yys = yyname[yych];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yych, yys);
            }
#endif
        }
        if (yych == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yystk.ssp, yystate);
#endif
    if (yystk.ssp >= yystk.sslim && yygrowstack(&yystk))
        goto yyoverflow;
    *++yystk.ssp = yystate;
    *++yystk.vsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    YYFREESTACK(&yystk);
    return (1);
yyaccept:
    YYFREESTACK(&yystk);
    return (0);
}
