#ifndef lint
static char const yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif
#include <stdlib.h>
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYLEX yylex()
#define YYEMPTY -1
#define yyclearin (yychar=(YYEMPTY))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING (yyerrflag!=0)
#if defined(c_plusplus) || defined(__cplusplus)
#include <stdlib.h>
#else
extern char *getenv();
extern void *realloc();
#endif
static int yygrowstack();
#define YYPREFIX "yy"
#line 2 "ns_parser.y"
#if !defined(lint) && !defined(SABER)
static char rcsid[] = "$Id: ns_parser.y,v 8.79 2002/04/25 05:27:13 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996-2000 by Internet Software Consortium.
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
#include <sys/stat.h>
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
static symbol_table channeltab = NULL;

static zone_config current_zone;
static int should_install;

static options current_options;
static int seen_options;
static int logged_options_error;

static controls current_controls;

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
static void define_channel(const char *, log_channel);
static char *canonical_name(char *);

int yyparse();
	
#line 105 "ns_parser.y"
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
#line 139 "y.tab.c"
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
#define T_HITCOUNT 274
#define T_PREFERRED_GLUE 275
#define T_QUERY_SOURCE 276
#define T_LISTEN_ON 277
#define T_PORT 278
#define T_ADDRESS 279
#define T_RRSET_ORDER 280
#define T_ORDER 281
#define T_NAME 282
#define T_CLASS 283
#define T_CONTROLS 284
#define T_INET 285
#define T_UNIX 286
#define T_PERM 287
#define T_OWNER 288
#define T_GROUP 289
#define T_ALLOW 290
#define T_DATASIZE 291
#define T_STACKSIZE 292
#define T_CORESIZE 293
#define T_DEFAULT 294
#define T_UNLIMITED 295
#define T_FILES 296
#define T_VERSION 297
#define T_HOSTNAME 298
#define T_HOSTSTATS 299
#define T_HOSTSTATSMAX 300
#define T_DEALLOC_ON_EXIT 301
#define T_TRANSFERS_IN 302
#define T_TRANSFERS_OUT 303
#define T_TRANSFERS_PER_NS 304
#define T_TRANSFER_FORMAT 305
#define T_MAX_TRANSFER_TIME_IN 306
#define T_SERIAL_QUERIES 307
#define T_ONE_ANSWER 308
#define T_MANY_ANSWERS 309
#define T_NOTIFY 310
#define T_EXPLICIT 311
#define T_NOTIFY_INITIAL 312
#define T_AUTH_NXDOMAIN 313
#define T_MULTIPLE_CNAMES 314
#define T_USE_IXFR 315
#define T_MAINTAIN_IXFR_BASE 316
#define T_CLEAN_INTERVAL 317
#define T_INTERFACE_INTERVAL 318
#define T_STATS_INTERVAL 319
#define T_MAX_LOG_SIZE_IXFR 320
#define T_HEARTBEAT 321
#define T_USE_ID_POOL 322
#define T_MAX_NCACHE_TTL 323
#define T_HAS_OLD_CLIENTS 324
#define T_RFC2308_TYPE1 325
#define T_LAME_TTL 326
#define T_MIN_ROOTS 327
#define T_TREAT_CR_AS_SPACE 328
#define T_LOGGING 329
#define T_CATEGORY 330
#define T_CHANNEL 331
#define T_SEVERITY 332
#define T_DYNAMIC 333
#define T_FILE 334
#define T_VERSIONS 335
#define T_SIZE 336
#define T_SYSLOG 337
#define T_DEBUG 338
#define T_NULL_OUTPUT 339
#define T_PRINT_TIME 340
#define T_PRINT_CATEGORY 341
#define T_PRINT_SEVERITY 342
#define T_SORTLIST 343
#define T_TOPOLOGY 344
#define T_SERVER 345
#define T_LONG_AXFR 346
#define T_BOGUS 347
#define T_TRANSFERS 348
#define T_KEYS 349
#define T_SUPPORT_IXFR 350
#define T_ZONE 351
#define T_IN 352
#define T_CHAOS 353
#define T_HESIOD 354
#define T_TYPE 355
#define T_MASTER 356
#define T_SLAVE 357
#define T_STUB 358
#define T_RESPONSE 359
#define T_HINT 360
#define T_MASTERS 361
#define T_TRANSFER_SOURCE 362
#define T_PUBKEY 363
#define T_ALSO_NOTIFY 364
#define T_DIALUP 365
#define T_FILE_IXFR 366
#define T_IXFR_TMP 367
#define T_TRUSTED_KEYS 368
#define T_ACL 369
#define T_ALLOW_UPDATE 370
#define T_ALLOW_QUERY 371
#define T_ALLOW_TRANSFER 372
#define T_ALLOW_RECURSION 373
#define T_BLACKHOLE 374
#define T_SEC_KEY 375
#define T_ALGID 376
#define T_SECRET 377
#define T_CHECK_NAMES 378
#define T_WARN 379
#define T_FAIL 380
#define T_IGNORE 381
#define T_FORWARD 382
#define T_FORWARDERS 383
#define T_ONLY 384
#define T_FIRST 385
#define T_IF_NO_ANSWER 386
#define T_IF_NO_DOMAIN 387
#define T_YES 388
#define T_TRUE 389
#define T_NO 390
#define T_FALSE 391
#define YYERRCODE 256
const short yylhs[] = {                                        -1,
    0,   31,   31,   32,   32,   32,   32,   32,   32,   32,
   32,   32,   32,   32,   32,   33,   42,   34,   43,   43,
   44,   44,   44,   44,   44,   44,   44,   44,   44,   44,
   44,   44,   44,   44,   44,   44,   44,   44,   44,   44,
   44,   44,   44,   44,   44,   44,   44,   44,   44,   46,
   44,   44,   44,   44,   44,   44,   44,   49,   44,   44,
   44,   44,   44,   44,   44,   44,   44,   44,   44,   44,
   44,   44,   44,   44,   44,   44,   44,   44,   44,   35,
   53,   53,   54,   54,   54,   54,   54,   54,   15,   15,
   12,   12,   13,   13,   14,   14,   16,    6,    6,    5,
    5,    4,    4,   56,   57,   48,   48,   48,   48,    2,
    2,    3,    3,   29,   29,   29,   29,   29,   27,   27,
   27,   28,   28,   28,   45,   45,   45,   45,   51,   51,
   51,   51,   26,   26,   26,   26,   52,   52,   52,   47,
   47,   58,   58,   59,   50,   50,   60,   60,   61,   62,
   36,   63,   63,   63,   65,   64,   67,   64,   69,   69,
   69,   69,   70,   70,   71,   72,   72,   72,   72,   72,
   73,   10,   10,   11,   11,   74,   75,   75,   75,   75,
   75,   75,   75,   68,   68,   68,    9,    9,   76,   66,
   66,   66,    8,    8,    8,    7,   77,   37,   78,   78,
   79,   79,   79,   79,   79,   79,   20,   20,   18,   18,
   18,   17,   17,   17,   17,   17,   19,   23,   81,   80,
   80,   80,   82,   55,   55,   55,   83,   41,   84,   84,
   84,   24,   25,   40,   86,   38,   85,   85,   21,   21,
   22,   22,   22,   22,   22,   87,   87,   88,   88,   88,
   88,   88,   88,   88,   88,   88,   88,   88,   91,   88,
   88,   88,   88,   88,   88,   88,   88,   88,   88,   88,
   89,   89,   94,   94,   93,   93,   95,   95,   96,   90,
   90,   92,   92,   97,   97,   98,   39,   99,   99,  100,
  100,    1,   30,   30,
};
const short yylen[] = {                                         2,
    1,    1,    2,    1,    2,    2,    2,    2,    2,    2,
    2,    2,    1,    2,    2,    3,    0,    5,    2,    3,
    0,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    3,    2,    2,    5,    2,    0,
    5,    2,    2,    4,    4,    4,    4,    0,    5,    4,
    4,    1,    1,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    4,    2,    2,    1,    4,
    2,    3,    0,    8,   10,   12,    8,    1,    2,    3,
    0,    2,    0,    2,    0,    2,    5,    1,    1,    1,
    1,    1,    1,    2,    2,    1,    1,    2,    2,    0,
    2,    0,    2,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    2,    2,
    2,    2,    1,    1,    1,    1,    2,    2,    2,    0,
    1,    2,    3,    1,    0,    1,    2,    3,    1,    0,
    5,    2,    3,    1,    0,    6,    0,    6,    1,    1,
    2,    1,    2,    2,    2,    0,    1,    1,    2,    2,
    3,    1,    1,    0,    1,    2,    1,    1,    1,    2,
    2,    2,    2,    2,    3,    1,    1,    1,    1,    2,
    3,    1,    1,    1,    1,    1,    0,    6,    2,    3,
    2,    2,    2,    2,    4,    1,    2,    3,    1,    2,
    2,    1,    3,    3,    1,    3,    1,    1,    1,    2,
    3,    1,    1,    2,    3,    1,    0,    6,    2,    2,
    1,    3,    3,    5,    0,    5,    0,    3,    0,    1,
    1,    1,    1,    1,    1,    2,    3,    2,    2,    2,
    2,    5,    2,    2,    4,    4,    4,    2,    0,    5,
    2,    2,    2,    2,    2,    5,    5,    4,    2,    1,
    2,    3,    1,    3,    0,    1,    2,    3,    1,    1,
    1,    0,    1,    2,    3,    1,    4,    2,    3,    5,
    5,    1,    1,    1,
};
const short yydefred[] = {                                      0,
    0,   13,    0,   17,    0,  150,    0,    0,    0,    0,
  227,    0,    0,    2,    4,    0,    0,    0,    0,    0,
    0,    0,    0,   14,   15,    0,    0,    0,    0,  197,
    0,    0,  293,  294,    0,    0,    3,    5,    6,    7,
    8,    9,   10,   11,   12,   16,    0,   88,    0,    0,
    0,    0,    0,    0,  235,  240,    0,    0,    0,    0,
    0,   79,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   58,    0,    0,    0,    0,    0,    0,    0,
   50,    0,    0,   62,   63,  100,  101,    0,    0,   80,
    0,   81,  154,    0,    0,    0,    0,    0,    0,    0,
    0,  287,    0,  288,    0,    0,    0,    0,    0,  209,
    0,  215,    0,  217,    0,   24,   26,   25,   29,   27,
   28,  118,  114,  115,  116,  117,   31,   32,   33,   34,
   30,    0,    0,   52,    0,    0,    0,    0,    0,  134,
  135,  136,  129,  133,  130,  131,  132,   23,   22,   38,
   70,   39,  137,  138,  139,   98,   99,   64,   65,   66,
   35,   36,   37,   43,   44,   40,   41,   67,   68,   69,
   71,   74,   46,   72,   42,   47,   73,   78,   77,    0,
    0,   53,    0,   75,    0,    0,    0,    0,  119,  120,
  121,    0,  125,  126,  127,  128,   49,    0,   18,    0,
   19,    0,    0,    0,   82,  194,  195,  155,  196,  193,
  188,  157,  187,  151,    0,  152,  206,    0,    0,    0,
    0,    0,    0,    0,    0,  236,    0,    0,  289,    0,
    0,  211,    0,  210,  207,  234,    0,  231,    0,    0,
    0,    0,    0,  292,  103,  102,  105,  104,  108,  109,
  111,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  122,  123,  124,   45,    0,   20,    0,
    0,    0,    0,    0,  153,  204,  201,  203,    0,  202,
  198,    0,  199,  270,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  259,    0,    0,    0,    0,  213,  214,  216,
  208,    0,    0,  229,  230,  228,    0,   92,    0,    0,
   76,    0,   89,   57,   61,  149,    0,    0,    0,   54,
   56,   55,   60,  144,    0,    0,    0,    0,    0,    0,
    0,    0,  222,  219,  218,    0,    0,  200,  261,  263,
  264,  265,  262,  249,  241,  242,  244,  243,  245,  248,
    0,    0,  253,    0,    0,    0,  269,  250,  251,    0,
    0,    0,  254,  280,  281,  258,    0,  238,    0,  246,
  290,  291,  232,  233,   48,   94,    0,    0,   90,   59,
    0,  147,   51,    0,  142,    0,    0,    0,  192,  189,
    0,    0,  186,    0,    0,    0,  179,    0,    0,    0,
    0,  177,  178,    0,  205,    0,  220,  113,    0,    0,
    0,  279,    0,    0,    0,    0,    0,    0,    0,  247,
   96,    0,  148,  143,    0,    0,    0,  156,    0,  190,
  162,    0,  159,  180,    0,  173,  175,  176,  172,  181,
  182,  183,  158,    0,  184,  221,    0,    0,    0,    0,
    0,  268,    0,  277,  255,  256,  257,  286,    0,    0,
    0,   97,    0,    0,   87,  191,  161,    0,    0,    0,
    0,  171,  185,    0,  252,    0,  271,  266,  267,  278,
  260,    0,  284,    0,  226,  223,    0,    0,  163,  164,
  165,  169,  170,  274,  272,  285,    0,   85,    0,  224,
    0,  225,   86,
};
const short yydgoto[] = {                                      12,
  286,  178,  402,  287,  128,  198,  248,  249,  440,  487,
  488,  294,  360,  428,  295,  296,  150,  151,  152,  153,
   55,  400,  536,  281,  282,  183,  232,  307,  167,  154,
   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,
   23,   27,  122,  123,  237,  238,  375,  174,  223,  367,
  124,  125,   51,   52,  537,  175,  176,  376,  377,  368,
  369,   29,  136,  137,  313,  441,  314,  451,  484,  520,
  521,  522,  452,  453,  454,  442,   54,  263,  264,  386,
  387,  538,   36,  283,  266,  139,  344,  345,  498,  416,
  417,  509,  463,  499,  464,  465,  510,  511,   58,   59,
};
const short yysindex[] = {                                    148,
 -173,    0, -245,    0,  -82,    0, -202, -214,  -65, -180,
    0,    0,  148,    0,    0, -193, -183, -179, -170, -159,
 -154, -150, -143,    0,    0, -120,   18, -151,   20,    0,
 -180, -108,    0,    0,   41, -180,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  414,    0,  -25,  -93,
 -102,  -85, -218,   51,    0,    0, -139,  -90,  -71,   66,
   55,    0,  -47,  -42,  -34,   10,   33,   53, -123, -123,
 -123, -123,  -62,  -67,  -17,  196,   56,   56,   56,   56,
   73,   82, -123,   88, -123,   89,   99,  100,  -92,  102,
  103, -182, -123, -123, -123, -123, -123,  104,  105,  107,
   56,  118, -123,  119, -123, -123,  121,  122, -123,  259,
  260,  -25,    0, -123,  261,  265,  266,  268, -162,  -56,
    0,  295,  136,    0,    0,    0,    0, -181,  108,    0,
  139,    0,    0, -134, -200,  -72,  140, -217,  275,  141,
  143,    0,  142,    0,  356,  359,  149,   66,  -79,    0,
  150,    0,  -29,    0, -219,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  -31,  -25,    0,  130,  137,  159,  299,  145,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   66,
   66,    0,  300,    0,   66,   66,   66,   66,    0,    0,
    0, -177,    0,    0,    0,    0,    0,  301,    0,  169,
    0,  159,  307,  174,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  178,    0,    0,  -92, -123,  179,
  317, -123, -100,  187,  439,    0,  186,  188,    0,  190,
  191,    0,  -23,    0,    0,    0,  195,    0, -180, -180,
   80,   83,  335,    0,    0,    0,    0,    0,    0,    0,
    0,   66, -180,  106, -110,  205,  -19,  -15,  207,   -6,
   15,   19,   26,    0,    0,    0,    0,  208,    0,  173,
   66,  180,  344,  346,    0,    0,    0,    0, -145,    0,
    0,  221,    0,    0,  223,  -49, -123,   56,  222,   -3,
  206,  -25,   22,  362, -123,  225,  227,  366,  367,  371,
 -177, -128,    0,  109,  238,  235,  236,    0,    0,    0,
    0,  241,  243,    0,    0,    0,   30,    0, -180,  224,
    0,  248,    0,    0,    0,    0,  382,  207,  251,    0,
    0,    0,    0,    0,  386,  208,  255,  390,   38,  256,
 -211,  114,    0,    0,    0,  -70,  262,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  159,  395,    0,  263,  267,  270,    0,    0,    0,   66,
   66,   66,    0,    0,    0,    0,  397,    0,  264,    0,
    0,    0,    0,    0,    0,    0,  269,  244,    0,    0,
  272,    0,    0,  274,    0,   66,  175,  245,    0,    0,
 -113,  276,    0, -188,  271, -178,    0, -123, -123, -123,
 -119,    0,    0,  278,    0,  279,    0,    0,  280,  281,
  283,    0,  418,  270,  282,   42,   46,   52,  286,    0,
    0,  285,    0,    0,   62,  423,  288,    0,  291,    0,
    0,  290,    0,    0,  -43,    0,    0,    0,    0,    0,
    0,    0,    0,  293,    0,    0,  177, -112,  296,  294,
  312,    0,  297,    0,    0,    0,    0,    0,  431,  286,
  319,    0,  228, -138,    0,    0,    0, -205,   56,  242,
  239,    0,    0, -180,    0,  322,    0,    0,    0,    0,
    0,  323,    0,  458,    0,    0,  -68,  325,    0,    0,
    0,    0,    0,    0,    0,    0, -138,    0,  327,    0,
  -60,    0,    0,
};
const short yyrindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  585,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  332,    0,    0,
  -95,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  333,    0,    0,    0,
  332,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  480,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  333,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  347,    0,
    0,    0,    0,    0,  349,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  368,  369,    0,    0, -189,    0,
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
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   63, -189,    0,    0,    0,  499,    0,
    0,    0,    0,    0,    0,    0,    0,  502,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  505,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  348,
    0,    0,    0,    0,    0,    0,    0,  506,    0,    0,
    0,    0,    0,    0,    0,  507,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  508,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  373,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  509,    0,    0,    0,    0,  510,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  379,    0,    0,  380,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  383,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  516,
    0,    0,  385,    0,    0,    0,    0,    0,    0,  387,
  388,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,
};
const short yygindex[] = {                                      0,
 -141,    0,    0,    0, -107,  389,    0,    0,  511,    0,
    0,    0,    0,    0,    0,  353,  494,   35,    0,  147,
    0,    0, -300,  370,  372,  -77,    0,  308,  -63,  -10,
    0,  637,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  529,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  603,  111,  479,  481,    0,  287,    0,
  303,    0,    0,  525,    0,    0,    0,    0,    0,  144,
  152,    0,    0,    0,  211,  233,    0,    0,  401,    0,
  289, -475,    0,    0,    0,    0,    0,  352,    0,    0,
    0,    0,    0,  194,    0,  212,    0,  183,    0,  639,
};
#define YYTABLESIZE 822
const short yytable[] = {                                      35,
  185,  186,  187,  149,  222,  493,  168,  169,  170,  149,
  285,  478,  525,  149,  361,   26,  127,  149,  384,  190,
   56,  192,  130,  211,  321,   61,  149,  239,  202,  203,
  204,  205,  206,  207,  142,  291,  278,  133,  257,  213,
   28,  215,  216,  148,  439,  219,   31,  149,   33,   34,
  224,  149,  254,  539,  455,   30,  548,   32,  149,   33,
   34,  549,  149,   38,  553,  288,  184,  184,  184,  184,
  149,   33,   34,   39,  149,  549,  162,   40,  149,   33,
   34,   33,   34,   24,  149,  384,   41,  258,   25,  540,
  184,   91,   91,  148,  149,  276,  242,   42,  149,  148,
  310,  350,   43,  148,   48,  364,   44,  148,  243,  365,
  383,  134,  135,   45,   33,   34,  148,  535,  370,  140,
  141,   33,   34,  250,  253,   33,   34,  251,  201,  259,
  260,  261,  262,   49,   50,  162,   46,  148,  251,  371,
   47,  148,   53,  372,  481,  497,   33,   34,  148,  482,
  373,   57,  148,   48,  425,  257,  279,  280,  486,  246,
  148,  239,  437,   60,  148,   91,  505,  129,  148,   57,
  506,  132,  293,  138,  148,  247,  507,  155,  145,  146,
   33,   34,   49,   50,  148,  144,  513,  277,  148,   33,
   34,   33,   34,  229,  230,  317,  231,  171,  320,   33,
   34,  304,  305,  306,  258,  163,  164,  165,  166,  162,
  172,  173,  444,  156,  445,  196,  197,  446,  157,  447,
  448,  449,  450,  544,  403,  251,  158,  284,  145,  146,
   33,   34,  126,  418,  145,  146,   33,   34,  145,  146,
   33,   34,  145,  146,   33,   34,  259,  260,  261,  262,
  393,  145,  146,   33,   34,  414,  415,  134,  135,  458,
  177,  390,  391,  392,  163,  164,  165,  166,  352,  353,
  159,  407,  145,  146,   33,   34,  145,  146,   33,   34,
  404,  405,  358,  145,  146,   33,   34,  145,  146,   33,
   34,  518,  519,  160,  273,  145,  146,   33,   34,  145,
  146,   33,   34,  145,  146,   33,   34,  277,  385,  145,
  146,   33,   34,  161,  180,   33,   34,  184,  179,  145,
  146,   33,   34,  145,  146,   33,   34,  233,  234,  235,
  236,  277,  277,  188,  277,  277,  277,  277,  163,  164,
  165,  166,  189,   93,   93,  147,  191,  193,  426,  181,
  182,  147,  395,  396,  397,  147,  398,  194,  195,  147,
  199,  200,  208,  209,  324,  210,  297,  298,  147,  443,
  253,  300,  301,  302,  303,  385,  212,  214,  399,  217,
  218,  220,  221,  225,  490,  491,  492,  226,  227,  147,
  228,  277,  241,  147,  244,  245,  256,  265,  269,  267,
  147,  268,  270,    1,  147,  271,  275,  172,  272,    2,
    3,    4,  147,  277,  325,  173,  147,  284,  326,  239,
  147,  292,  299,  308,  327,  309,  147,  293,  328,  311,
  253,    5,  312,  483,  315,  489,  147,  318,  357,  319,
  147,  541,  329,  323,  346,  444,  347,  445,  348,  349,
  446,  351,  447,  448,  449,  450,  280,  379,  279,  356,
  359,  363,  378,  330,  366,  374,  381,  380,  382,  331,
  332,  333,  334,  335,  336,  337,    6,  388,  338,  339,
  340,  389,  394,  401,  406,  408,  341,  409,  410,  411,
  342,  343,    7,  412,  420,  421,  422,  423,    8,  424,
  277,  277,  277,  385,  429,  427,  430,  432,  184,  277,
  433,  435,  436,  385,  438,    9,   10,  459,  457,  469,
  470,  460,   11,  476,  472,  461,  385,  462,  473,  471,
  474,  485,  480,  477,  495,  496,  385,  497,  504,  500,
  385,  501,  502,  508,  512,  514,  515,  516,  517,  523,
   62,  524,  527,  530,  528,  531,  466,  467,  468,   63,
   64,   65,   66,   67,   68,   69,   70,   71,   72,   73,
   74,   75,  529,  518,   76,  533,  534,  519,  545,  546,
  547,  550,  475,  552,    1,   77,   78,   79,   83,   21,
   80,   81,   82,   83,   84,   85,   86,   87,   88,   89,
   90,   91,  110,  237,   92,  212,   93,   94,   95,   96,
   97,   98,   99,  100,  101,  102,  103,  104,  105,  106,
  107,  108,  109,  145,  106,  107,  140,  112,   95,  174,
  146,  141,  275,  276,  282,  160,  166,  110,  111,  273,
  283,   84,  274,  167,  168,  252,  316,  362,  413,   37,
  240,  355,  354,  131,  290,  289,  112,  551,  113,  114,
  255,  494,  434,  322,  543,  115,  116,  117,  118,   62,
  431,  542,  119,  479,  456,  503,  120,  121,   63,   64,
   65,   66,   67,   68,   69,   70,   71,   72,   73,   74,
   75,  526,  532,   76,  324,  419,  143,    0,    0,    0,
    0,    0,    0,    0,   77,   78,   79,    0,    0,   80,
   81,   82,   83,   84,   85,   86,   87,   88,   89,   90,
   91,    0,    0,   92,    0,   93,   94,   95,   96,   97,
   98,   99,  100,  101,  102,  103,  104,  105,  106,  107,
  108,  109,    0,    0,  325,    0,    0,    0,  326,    0,
    0,    0,    0,    0,  327,    0,  110,  111,  328,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  329,    0,    0,  112,    0,  113,  114,    0,
    0,    0,    0,    0,  115,  116,  117,  118,    0,    0,
    0,  119,    0,  330,    0,  120,  121,    0,    0,  331,
  332,  333,  334,  335,  336,  337,    0,    0,  338,  339,
  340,    0,    0,    0,    0,    0,  341,    0,    0,    0,
  342,  343,
};
const short yycheck[] = {                                      10,
   78,   79,   80,   33,  112,  125,   70,   71,   72,   33,
   42,  125,  125,   33,  125,  261,   42,   33,  319,   83,
   31,   85,  125,  101,  125,   36,   33,  123,   92,   93,
   94,   95,   96,   97,  125,  177,  256,  256,  256,  103,
  123,  105,  106,  123,  256,  109,  261,   33,  260,  261,
  114,   33,  125,  259,  125,  258,  125,  123,   33,  260,
  261,  537,   33,  257,  125,  173,   77,   78,   79,   80,
   33,  260,  261,  257,   33,  551,  259,  257,   33,  260,
  261,  260,  261,  257,   33,  386,  257,  305,  262,  295,
  101,  281,  282,  123,   33,  125,  278,  257,   33,  123,
  242,  125,  257,  123,  256,  125,  257,  123,  290,  125,
  256,  330,  331,  257,  260,  261,  123,  256,  125,  259,
  260,  260,  261,  134,  135,  260,  261,  339,  311,  347,
  348,  349,  350,  285,  286,  259,  257,  123,  339,  125,
  123,  123,  123,  125,  333,  258,  260,  261,  123,  338,
  125,  260,  123,  256,  125,  256,  376,  377,  337,  294,
  123,  257,  125,  123,  123,  355,  125,  261,  123,  260,
  125,  257,  283,  123,  123,  310,  125,  123,  258,  259,
  260,  261,  285,  286,  123,  257,  125,  153,  123,  260,
  261,  260,  261,  356,  357,  259,  359,  260,  262,  260,
  261,  379,  380,  381,  305,  388,  389,  390,  391,  259,
  278,  279,  332,  261,  334,  308,  309,  337,  261,  339,
  340,  341,  342,  524,  332,  339,  261,  259,  258,  259,
  260,  261,  258,  125,  258,  259,  260,  261,  258,  259,
  260,  261,  258,  259,  260,  261,  347,  348,  349,  350,
  328,  258,  259,  260,  261,  384,  385,  330,  331,  401,
  278,  311,  326,  327,  388,  389,  390,  391,  279,  280,
  261,  335,  258,  259,  260,  261,  258,  259,  260,  261,
  259,  260,  293,  258,  259,  260,  261,  258,  259,  260,
  261,  335,  336,  261,  148,  258,  259,  260,  261,  258,
  259,  260,  261,  258,  259,  260,  261,  273,  319,  258,
  259,  260,  261,  261,  259,  260,  261,  328,  123,  258,
  259,  260,  261,  258,  259,  260,  261,  384,  385,  386,
  387,  297,  298,  261,  300,  301,  302,  303,  388,  389,
  390,  391,  261,  281,  282,  375,  259,  259,  359,  294,
  295,  375,  356,  357,  358,  375,  360,  259,  259,  375,
  259,  259,  259,  259,  256,  259,  220,  221,  375,  256,
  381,  225,  226,  227,  228,  386,  259,  259,  382,  259,
  259,  123,  123,  123,  448,  449,  450,  123,  123,  375,
  123,  357,  257,  375,  287,  257,  257,  123,  257,  259,
  375,  259,   47,  256,  375,   47,  257,  278,  260,  262,
  263,  264,  375,  379,  306,  279,  375,  259,  310,  125,
  375,  123,  123,  123,  316,  257,  375,  283,  320,  123,
  441,  284,  259,  444,  257,  446,  375,  259,  292,  123,
  375,  519,  334,  257,  259,  332,  259,  334,  259,  259,
  337,  257,  339,  340,  341,  342,  377,  311,  376,  125,
  355,  257,  290,  355,  258,  258,  123,  288,  123,  361,
  362,  363,  364,  365,  366,  367,  329,  257,  370,  371,
  372,  259,  261,  278,  123,  261,  378,  261,  123,  123,
  382,  383,  345,  123,  257,  261,  261,  257,  351,  257,
  466,  467,  468,  514,  257,  282,  125,  257,  519,  475,
  125,  257,  123,  524,  259,  368,  369,  123,  257,  123,
  257,  259,  375,  349,  281,  259,  537,  258,  257,  261,
  257,  261,  257,  289,  257,  257,  547,  258,  257,  259,
  551,  259,  125,  258,  260,  123,  259,  257,  259,  257,
  256,  375,  257,  257,  261,  125,  410,  411,  412,  265,
  266,  267,  268,  269,  270,  271,  272,  273,  274,  275,
  276,  277,  261,  335,  280,  257,  349,  336,  257,  257,
  123,  257,  436,  257,    0,  291,  292,  293,  257,  257,
  296,  297,  298,  299,  300,  301,  302,  303,  304,  305,
  306,  307,  123,  257,  310,  257,  312,  313,  314,  315,
  316,  317,  318,  319,  320,  321,  322,  323,  324,  325,
  326,  327,  328,  125,  257,  257,  125,  123,  281,  257,
  125,  125,  125,  125,  125,  257,  257,  343,  344,  257,
  125,  257,  149,  257,  257,  135,  258,  295,  341,   13,
  122,  282,  281,   51,  176,  175,  362,  547,  364,  365,
  136,  451,  376,  263,  521,  371,  372,  373,  374,  256,
  368,  520,  378,  441,  386,  464,  382,  383,  265,  266,
  267,  268,  269,  270,  271,  272,  273,  274,  275,  276,
  277,  498,  510,  280,  256,  344,   58,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  291,  292,  293,   -1,   -1,  296,
  297,  298,  299,  300,  301,  302,  303,  304,  305,  306,
  307,   -1,   -1,  310,   -1,  312,  313,  314,  315,  316,
  317,  318,  319,  320,  321,  322,  323,  324,  325,  326,
  327,  328,   -1,   -1,  306,   -1,   -1,   -1,  310,   -1,
   -1,   -1,   -1,   -1,  316,   -1,  343,  344,  320,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  334,   -1,   -1,  362,   -1,  364,  365,   -1,
   -1,   -1,   -1,   -1,  371,  372,  373,  374,   -1,   -1,
   -1,  378,   -1,  355,   -1,  382,  383,   -1,   -1,  361,
  362,  363,  364,  365,  366,  367,   -1,   -1,  370,  371,
  372,   -1,   -1,   -1,   -1,   -1,  378,   -1,   -1,   -1,
  382,  383,
};
#define YYFINAL 12
#ifndef YYDEBUG
#define YYDEBUG 0
#elif YYDEBUG
#include <stdio.h>
#endif
#define YYMAXTOKEN 391
#if YYDEBUG
const char * const yyname[] = {
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
"T_HITCOUNT","T_PREFERRED_GLUE","T_QUERY_SOURCE","T_LISTEN_ON","T_PORT",
"T_ADDRESS","T_RRSET_ORDER","T_ORDER","T_NAME","T_CLASS","T_CONTROLS","T_INET",
"T_UNIX","T_PERM","T_OWNER","T_GROUP","T_ALLOW","T_DATASIZE","T_STACKSIZE",
"T_CORESIZE","T_DEFAULT","T_UNLIMITED","T_FILES","T_VERSION","T_HOSTNAME",
"T_HOSTSTATS","T_HOSTSTATSMAX","T_DEALLOC_ON_EXIT","T_TRANSFERS_IN",
"T_TRANSFERS_OUT","T_TRANSFERS_PER_NS","T_TRANSFER_FORMAT",
"T_MAX_TRANSFER_TIME_IN","T_SERIAL_QUERIES","T_ONE_ANSWER","T_MANY_ANSWERS",
"T_NOTIFY","T_EXPLICIT","T_NOTIFY_INITIAL","T_AUTH_NXDOMAIN",
"T_MULTIPLE_CNAMES","T_USE_IXFR","T_MAINTAIN_IXFR_BASE","T_CLEAN_INTERVAL",
"T_INTERFACE_INTERVAL","T_STATS_INTERVAL","T_MAX_LOG_SIZE_IXFR","T_HEARTBEAT",
"T_USE_ID_POOL","T_MAX_NCACHE_TTL","T_HAS_OLD_CLIENTS","T_RFC2308_TYPE1",
"T_LAME_TTL","T_MIN_ROOTS","T_TREAT_CR_AS_SPACE","T_LOGGING","T_CATEGORY",
"T_CHANNEL","T_SEVERITY","T_DYNAMIC","T_FILE","T_VERSIONS","T_SIZE","T_SYSLOG",
"T_DEBUG","T_NULL_OUTPUT","T_PRINT_TIME","T_PRINT_CATEGORY","T_PRINT_SEVERITY",
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
const char * const yyrule[] = {
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
"option : T_HOSTNAME L_QSTRING",
"option : T_VERSION L_QSTRING",
"option : T_DIRECTORY L_QSTRING",
"option : T_NAMED_XFER L_QSTRING",
"option : T_PIDFILE L_QSTRING",
"option : T_STATS_FILE L_QSTRING",
"option : T_MEMSTATS_FILE L_QSTRING",
"option : T_DUMP_FILE L_QSTRING",
"option : T_PREFERRED_GLUE L_STRING",
"option : T_FAKE_IQUERY yea_or_nay",
"option : T_RECURSION yea_or_nay",
"option : T_FETCH_GLUE yea_or_nay",
"option : T_HITCOUNT yea_or_nay",
"option : T_NOTIFY T_EXPLICIT",
"option : T_NOTIFY yea_or_nay",
"option : T_NOTIFY_INITIAL yea_or_nay",
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
"option : T_HOSTSTATSMAX L_NUMBER",
"option : T_MAX_LOG_SIZE_IXFR size_spec",
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
"control : T_INET maybe_wild_addr T_ALLOW '{' address_match_list '}' T_KEYS '{' dummy_key_list '}'",
"control : T_INET maybe_wild_addr T_PORT in_port T_ALLOW '{' address_match_list '}' T_KEYS '{' dummy_key_list '}'",
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
"dummy_key_list_element : key_ref",
"dummy_key_list : dummy_key_list_element L_EOS",
"dummy_key_list : dummy_key_list dummy_key_list_element L_EOS",
"dummy_key_list : error",
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
"zone_option : T_MAX_LOG_SIZE_IXFR size_spec",
"zone_option : T_NOTIFY T_EXPLICIT",
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
"master_in_addr : L_IPADDR T_SEC_KEY key_ref",
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
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short *yyss;
short *yysslim;
YYSTYPE *yyvs;
int yystacksize;
#line 1857 "ns_parser.y"

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
	define_acl("any", iml);

	/* ACL "none" */
	ime = new_ip_match_pattern(address, 0);
	ip_match_negate(ime);
	iml = new_ip_match_list();
	add_to_ip_match_list(iml, ime);
	define_acl("none", iml);

	/* ACL "localhost" */
	ime = new_ip_match_localhost();
	iml = new_ip_match_list();
	add_to_ip_match_list(iml, ime);
	define_acl("localhost", iml);

	/* ACL "localnets" */
	ime = new_ip_match_localnets();
	iml = new_ip_match_list();
	add_to_ip_match_list(iml, ime);
	define_acl("localnets", iml);
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
	case SYM_CHANNEL:
		INSIST(log_free_channel(value) == 0);
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

	if (lookup_symbol(channeltab, name, SYM_CHANNEL, &value))
		return ((log_channel)(value.pointer));
	return (NULL);
}

static void
define_channel(const char *name, log_channel channel) {
	symbol_value value;

	value.pointer = channel;  
	INSIST(log_inc_references(channel) == 0);
	define_symbol(channeltab, name, SYM_CHANNEL, value, SYMBOL_FREE_VALUE);
}

static void
define_builtin_channels() {
	define_channel("default_syslog", syslog_channel);
	define_channel("default_debug", debug_channel);
	define_channel("default_stderr", stderr_channel);
	define_channel("null", null_channel);
}

static void
parser_setup() {
	seen_options = 0;
	logged_options_error = 0;
	seen_topology = 0;
	symtab = new_symbol_table(SYMBOL_TABLE_SIZE, NULL);
	if (authtab != NULL)
		free_symbol_table(authtab);
	authtab = new_symbol_table(AUTH_TABLE_SIZE, free_sym_value);
	if (channeltab != NULL)
		free_symbol_table(channeltab);
	channeltab = new_symbol_table(AUTH_TABLE_SIZE, free_sym_value);
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
lookup_acl(const char *name) {
	symbol_value value;

	if (lookup_symbol(authtab, name, SYM_ACL, &value))
		return ((ip_match_list)(value.pointer));
	return (NULL);
}

void
define_acl(const char *name, ip_match_list iml) {
	symbol_value value;

	INSIST(name != NULL);
	INSIST(iml != NULL);

	value.pointer = iml;
	define_symbol(authtab, name, SYM_ACL, value, SYMBOL_FREE_VALUE);
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
define_key(const char *name, struct dst_key *dst_key) {
	symbol_value value;

	INSIST(name != NULL);
	INSIST(dst_key != NULL);

	value.pointer = dst_key;
	define_symbol(authtab, name, SYM_KEY, value, SYMBOL_FREE_VALUE);
	dprint_key_info(dst_key);
}

time_t
parse_configuration(const char *filename) {
	FILE *config_stream;
	struct stat sb;

	config_stream = fopen(filename, "r");
	if (config_stream == NULL)
		ns_panic(ns_log_parser, 0, "can't open '%s'", filename);
	if (fstat(fileno(config_stream), &sb) == -1)
		ns_panic(ns_log_parser, 0, "can't stat '%s'", filename);

	lexer_setup();
	parser_setup();
	lexer_begin_file(filename, config_stream);
	(void)yyparse();
	lexer_end_file();
	parser_cleanup();
	return (sb.st_mtime);
}

void
parser_initialize(void) {
	lexer_initialize();
}

void
parser_shutdown(void) {
	if (authtab != NULL)
		free_symbol_table(authtab);
	if (channeltab != NULL)
		free_symbol_table(channeltab);
	lexer_shutdown();
}
#line 1279 "y.tab.c"
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack()
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;
    i = yyssp - yyss;
    if ((newss = (short *)realloc(yyss, newsize * sizeof *newss)) == NULL)
        return -1;
    yyss = newss;
    yyssp = newss + i;
    if ((newvs = (YYSTYPE *)realloc(yyvs, newsize * sizeof *newvs)) == NULL)
        return -1;
    yyvs = newvs;
    yyvsp = newvs + i;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab

int
yyparse()
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register const char *yys;

    if ((yys = getenv("YYDEBUG")))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate])) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#if defined(lint) || defined(__GNUC__)
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#if defined(lint) || defined(__GNUC__)
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
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 1:
#line 246 "ns_parser.y"
{
		if (EMPTY(current_controls))
			ns_ctl_defaults(&current_controls);
		ns_ctl_install(&current_controls);
	}
break;
case 16:
#line 272 "ns_parser.y"
{
		lexer_begin_file(yyvsp[-1].cp, NULL);
		(void)freestr(yyvsp[-1].cp);
	}
break;
case 17:
#line 283 "ns_parser.y"
{
		if (seen_options)
			parser_error(0, "cannot redefine options");
		current_options = new_options();
	}
break;
case 18:
#line 289 "ns_parser.y"
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
#line 305 "ns_parser.y"
{
		if (current_options->hostname != NULL)
			(void)freestr(current_options->hostname);
		current_options->hostname = yyvsp[0].cp;
	}
break;
case 23:
#line 311 "ns_parser.y"
{
		if (current_options->version != NULL)
			(void)freestr(current_options->version);
		current_options->version = yyvsp[0].cp;
	}
break;
case 24:
#line 317 "ns_parser.y"
{
		if (current_options->directory != NULL)
			(void)freestr(current_options->directory);
		current_options->directory = yyvsp[0].cp;
	}
break;
case 25:
#line 323 "ns_parser.y"
{
		if (current_options->named_xfer != NULL)
			(void)freestr(current_options->named_xfer);
		current_options->named_xfer = yyvsp[0].cp;
	}
break;
case 26:
#line 329 "ns_parser.y"
{
		if (current_options->pid_filename != NULL)
			(void)freestr(current_options->pid_filename);
		current_options->pid_filename = yyvsp[0].cp;
	}
break;
case 27:
#line 335 "ns_parser.y"
{
		if (current_options->stats_filename != NULL)
			(void)freestr(current_options->stats_filename);
		current_options->stats_filename = yyvsp[0].cp;
	}
break;
case 28:
#line 341 "ns_parser.y"
{
		if (current_options->memstats_filename != NULL)
			(void)freestr(current_options->memstats_filename);
		current_options->memstats_filename = yyvsp[0].cp;
	}
break;
case 29:
#line 347 "ns_parser.y"
{
		if (current_options->dump_filename != NULL)
			(void)freestr(current_options->dump_filename);
		current_options->dump_filename = yyvsp[0].cp;
	}
break;
case 30:
#line 353 "ns_parser.y"
{
		current_options->preferred_glue =
			strcasecmp(yyvsp[0].cp, "aaaa") ? T_A : T_AAAA;
	}
break;
case 31:
#line 358 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_FAKE_IQUERY, yyvsp[0].num);
	}
break;
case 32:
#line 363 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_NORECURSE, !yyvsp[0].num);
	}
break;
case 33:
#line 368 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_NOFETCHGLUE, !yyvsp[0].num);
	}
break;
case 34:
#line 373 "ns_parser.y"
{
		set_global_boolean_option(current_options, 
			OPTION_HITCOUNT, yyvsp[0].num);
	}
break;
case 35:
#line 378 "ns_parser.y"
{
		current_options->notify = notify_explicit;
	}
break;
case 36:
#line 382 "ns_parser.y"
{
		if (yyvsp[0].num)
			current_options->notify = notify_yes;
		else
			current_options->notify = notify_no;
	}
break;
case 37:
#line 389 "ns_parser.y"
{
		if (initial_configuration && yyvsp[0].num)
			ns_notice(ns_log_default,
				  "suppressing initial notifies");
		set_global_boolean_option(current_options, 
			OPTION_SUPNOTIFY_INITIAL, yyvsp[0].num);
	}
break;
case 38:
#line 397 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_HOSTSTATS, yyvsp[0].num);
	}
break;
case 39:
#line 402 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_DEALLOC_ON_EXIT, yyvsp[0].num);
	}
break;
case 40:
#line 407 "ns_parser.y"
{
		set_global_boolean_option(current_options, OPTION_USE_IXFR, yyvsp[0].num);
	}
break;
case 41:
#line 411 "ns_parser.y"
{
		set_global_boolean_option(current_options,
					  OPTION_MAINTAIN_IXFR_BASE, yyvsp[0].num);
	}
break;
case 42:
#line 416 "ns_parser.y"
{
		set_global_boolean_option(current_options,
					  OPTION_NORFC2308_TYPE1, yyvsp[0].num);
		set_global_boolean_option(current_options,
					  OPTION_NONAUTH_NXDOMAIN, !yyvsp[0].num);
	}
break;
case 43:
#line 423 "ns_parser.y"
{
		set_global_boolean_option(current_options, OPTION_NONAUTH_NXDOMAIN,
				   !yyvsp[0].num);
	}
break;
case 44:
#line 428 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_MULTIPLE_CNAMES, yyvsp[0].num);
	}
break;
case 45:
#line 433 "ns_parser.y"
{
		current_options->check_names[yyvsp[-1].s_int] = (enum severity)yyvsp[0].s_int;
	}
break;
case 46:
#line 437 "ns_parser.y"
{
		set_global_boolean_option(current_options,
					  OPTION_USE_ID_POOL, yyvsp[0].num);
	}
break;
case 47:
#line 442 "ns_parser.y"
{
		set_global_boolean_option(current_options,
                        		  OPTION_NORFC2308_TYPE1, !yyvsp[0].num);
	}
break;
case 48:
#line 447 "ns_parser.y"
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
			define_symbol(symtab, port_string, SYM_PORT, value, 0);
		}

	}
break;
case 50:
#line 465 "ns_parser.y"
{
		if (current_options->fwdtab) {
			free_forwarders(current_options->fwdtab);
			current_options->fwdtab = NULL;
		}
	}
break;
case 53:
#line 474 "ns_parser.y"
{
		current_options->axfr_src = yyvsp[0].ip_addr;
	}
break;
case 54:
#line 478 "ns_parser.y"
{
		if (current_options->query_acl) {
			parser_warning(0,
			      "options allow-query acl already set; skipping");
			free_ip_match_list(yyvsp[-1].iml);
		} else 
			current_options->query_acl = yyvsp[-1].iml;
	}
break;
case 55:
#line 487 "ns_parser.y"
{
		if (current_options->recursion_acl) {
			parser_warning(0,
			      "options allow-recursion acl already set; skipping");
			free_ip_match_list(yyvsp[-1].iml);
		} else
			current_options->recursion_acl = yyvsp[-1].iml;
	}
break;
case 56:
#line 496 "ns_parser.y"
{
		if (current_options->transfer_acl) {
			parser_warning(0,
			   "options allow-transfer acl already set; skipping");
			free_ip_match_list(yyvsp[-1].iml);
		} else 
			current_options->transfer_acl = yyvsp[-1].iml;
	}
break;
case 57:
#line 505 "ns_parser.y"
{
		if (current_options->sortlist) {
			parser_warning(0,
			      "options sortlist already set; skipping");
			free_ip_match_list(yyvsp[-1].iml);
		} else
			current_options->sortlist = yyvsp[-1].iml;
	}
break;
case 58:
#line 514 "ns_parser.y"
{
		if (current_options->also_notify) {
			parser_warning(0,
			    "duplicate also-notify clause: overwriting");
			free_also_notify(current_options);
			current_options->also_notify = NULL;
		}
	}
break;
case 60:
#line 524 "ns_parser.y"
{
		if (current_options->blackhole_acl) {
			parser_warning(0,
			      "options blackhole already set; skipping");
			free_ip_match_list(yyvsp[-1].iml);
		} else
			current_options->blackhole_acl = yyvsp[-1].iml;
	}
break;
case 61:
#line 533 "ns_parser.y"
{
		if (current_options->topology) {
			parser_warning(0,
			      "options topology already set; skipping");
			free_ip_match_list(yyvsp[-1].iml);
		} else
			current_options->topology = yyvsp[-1].iml;
	}
break;
case 62:
#line 542 "ns_parser.y"
{
		/* To get around the $$ = $1 default rule. */
	}
break;
case 64:
#line 547 "ns_parser.y"
{
		current_options->transfer_format = yyvsp[0].axfr_fmt;
	}
break;
case 65:
#line 551 "ns_parser.y"
{
		current_options->max_transfer_time_in = yyvsp[0].num * 60;
	}
break;
case 66:
#line 555 "ns_parser.y"
{
		current_options->serial_queries = yyvsp[0].num;
	}
break;
case 67:
#line 559 "ns_parser.y"
{
		current_options->clean_interval = yyvsp[0].num * 60;
	}
break;
case 68:
#line 563 "ns_parser.y"
{
		current_options->interface_interval = yyvsp[0].num * 60;
	}
break;
case 69:
#line 567 "ns_parser.y"
{
		current_options->stats_interval = yyvsp[0].num * 60;
	}
break;
case 70:
#line 571 "ns_parser.y"
{
		current_options->max_host_stats = yyvsp[0].num;
	}
break;
case 71:
#line 575 "ns_parser.y"
{
		current_options->max_log_size_ixfr = yyvsp[0].ul_int;
	}
break;
case 72:
#line 579 "ns_parser.y"
{
		current_options->max_ncache_ttl = yyvsp[0].num;
	}
break;
case 73:
#line 583 "ns_parser.y"
{
		current_options->lame_ttl = yyvsp[0].num;
	}
break;
case 74:
#line 587 "ns_parser.y"
{
		current_options->heartbeat_interval = yyvsp[0].num * 60;
	}
break;
case 75:
#line 591 "ns_parser.y"
{
		set_global_boolean_option(current_options,
                                          OPTION_NODIALUP, !yyvsp[0].num);
	}
break;
case 76:
#line 596 "ns_parser.y"
{
		if (current_options->ordering)
			free_rrset_order_list(current_options->ordering);
		current_options->ordering = yyvsp[-1].rol;
	}
break;
case 77:
#line 602 "ns_parser.y"
{
		set_global_boolean_option(current_options,
					  OPTION_TREAT_CR_AS_SPACE, yyvsp[0].num);
	}
break;
case 78:
#line 607 "ns_parser.y"
{
		if (yyvsp[0].num >= 1)
			current_options->minroots = yyvsp[0].num;
	}
break;
case 84:
#line 627 "ns_parser.y"
{
		ns_ctl_add(&current_controls, ns_ctl_new_inet(yyvsp[-6].ip_addr, yyvsp[-4].us_int, yyvsp[-1].iml));
	}
break;
case 85:
#line 633 "ns_parser.y"
{
		parser_warning(0, "Ignoring BIND 9 inet control clause");
		free_ip_match_list(yyvsp[-5].iml);
	}
break;
case 86:
#line 640 "ns_parser.y"
{
		parser_warning(0, "Ignoring BIND 9 inet control clause");
		free_ip_match_list(yyvsp[-5].iml);
	}
break;
case 87:
#line 645 "ns_parser.y"
{
#ifndef NO_SOCKADDR_UN
		ns_ctl_add(&current_controls, ns_ctl_new_unix(yyvsp[-6].cp, yyvsp[-4].num, yyvsp[-2].num, yyvsp[0].num));
#endif
		freestr(yyvsp[-6].cp);
	}
break;
case 89:
#line 655 "ns_parser.y"
{
		rrset_order_list rol;

		rol = new_rrset_order_list();
		if (yyvsp[-1].roe != NULL) {
			add_to_rrset_order_list(rol, yyvsp[-1].roe);
		}
		
		yyval.rol = rol;
	}
break;
case 90:
#line 666 "ns_parser.y"
{
		if (yyvsp[-1].roe != NULL) {
			add_to_rrset_order_list(yyvsp[-2].rol, yyvsp[-1].roe);
		}
		yyval.rol = yyvsp[-2].rol;
	}
break;
case 91:
#line 675 "ns_parser.y"
{
		yyval.s_int = C_ANY;
	}
break;
case 92:
#line 679 "ns_parser.y"
{
		symbol_value value;

		if (lookup_symbol(constants, yyvsp[0].cp, SYM_CLASS, &value))
			yyval.s_int = value.integer;
		else {
			parser_error(0, "unknown class '%s'; using ANY", yyvsp[0].cp);
			yyval.s_int = C_ANY;
		}
		(void)freestr(yyvsp[0].cp);
	}
break;
case 93:
#line 693 "ns_parser.y"
{
		yyval.s_int = ns_t_any;
	}
break;
case 94:
#line 697 "ns_parser.y"
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
		(void)freestr(yyvsp[0].cp);
	}
break;
case 95:
#line 715 "ns_parser.y"
{
		yyval.cp = savestr("*", 1);
	}
break;
case 96:
#line 719 "ns_parser.y"
{
		if (strcmp(".",yyvsp[0].cp) == 0 || strcmp("*.",yyvsp[0].cp) == 0) {
			yyval.cp = savestr("*", 1);
			(void)freestr(yyvsp[0].cp);
		} else {
			yyval.cp = yyvsp[0].cp ;
		}
		/* XXX Should do any more name validation here? */
	}
break;
case 97:
#line 731 "ns_parser.y"
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
			
			(void)freestr(yyvsp[0].cp);
			
			yyval.roe = new_rrset_order_element(yyvsp[-4].s_int, yyvsp[-3].s_int, yyvsp[-2].cp, o);
		}
	}
break;
case 98:
#line 754 "ns_parser.y"
{
		yyval.axfr_fmt = axfr_one_answer;
	}
break;
case 99:
#line 758 "ns_parser.y"
{
		yyval.axfr_fmt = axfr_many_answers;
	}
break;
case 100:
#line 763 "ns_parser.y"
{ yyval.ip_addr = yyvsp[0].ip_addr; }
break;
case 101:
#line 764 "ns_parser.y"
{ yyval.ip_addr.s_addr = htonl(INADDR_ANY); }
break;
case 102:
#line 767 "ns_parser.y"
{ yyval.us_int = yyvsp[0].us_int; }
break;
case 103:
#line 768 "ns_parser.y"
{ yyval.us_int = htons(0); }
break;
case 104:
#line 772 "ns_parser.y"
{
		current_options->query_source.sin_addr = yyvsp[0].ip_addr;
	}
break;
case 105:
#line 778 "ns_parser.y"
{
		current_options->query_source.sin_port = yyvsp[0].us_int;
	}
break;
case 110:
#line 789 "ns_parser.y"
{ yyval.us_int = htons(NS_DEFAULTPORT); }
break;
case 111:
#line 790 "ns_parser.y"
{ yyval.us_int = yyvsp[0].us_int; }
break;
case 112:
#line 793 "ns_parser.y"
{ yyval.us_int = htons(0); }
break;
case 113:
#line 794 "ns_parser.y"
{ yyval.us_int = yyvsp[0].us_int; }
break;
case 114:
#line 799 "ns_parser.y"
{ 
		yyval.num = 1;	
	}
break;
case 115:
#line 803 "ns_parser.y"
{ 
		yyval.num = 1;	
	}
break;
case 116:
#line 807 "ns_parser.y"
{ 
		yyval.num = 0;	
	}
break;
case 117:
#line 811 "ns_parser.y"
{ 
		yyval.num = 0;	
	}
break;
case 118:
#line 815 "ns_parser.y"
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
case 119:
#line 827 "ns_parser.y"
{
		yyval.s_int = primary_trans;
	}
break;
case 120:
#line 831 "ns_parser.y"
{
		yyval.s_int = secondary_trans;
	}
break;
case 121:
#line 835 "ns_parser.y"
{
		yyval.s_int = response_trans;
	}
break;
case 122:
#line 841 "ns_parser.y"
{
		yyval.s_int = warn;
	}
break;
case 123:
#line 845 "ns_parser.y"
{
		yyval.s_int = fail;
	}
break;
case 124:
#line 849 "ns_parser.y"
{
		yyval.s_int = ignore;
	}
break;
case 125:
#line 855 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_FORWARD_ONLY, 1);
	}
break;
case 126:
#line 860 "ns_parser.y"
{
		set_global_boolean_option(current_options,
			OPTION_FORWARD_ONLY, 0);
	}
break;
case 127:
#line 865 "ns_parser.y"
{
		parser_warning(0, "forward if-no-answer is unimplemented");
	}
break;
case 128:
#line 869 "ns_parser.y"
{
		parser_warning(0, "forward if-no-domain is unimplemented");
	}
break;
case 129:
#line 875 "ns_parser.y"
{
		current_options->data_size = yyvsp[0].ul_int;
	}
break;
case 130:
#line 879 "ns_parser.y"
{
		current_options->stack_size = yyvsp[0].ul_int;
	}
break;
case 131:
#line 883 "ns_parser.y"
{
		current_options->core_size = yyvsp[0].ul_int;
	}
break;
case 132:
#line 887 "ns_parser.y"
{
		current_options->files = yyvsp[0].ul_int;
	}
break;
case 133:
#line 893 "ns_parser.y"
{
		u_long result;

		if (unit_to_ulong(yyvsp[0].cp, &result))
			yyval.ul_int = result;
		else {
			parser_error(0, "invalid unit string '%s'", yyvsp[0].cp);
			/* 0 means "use default" */
			yyval.ul_int = 0;
		}
		(void)freestr(yyvsp[0].cp);
	}
break;
case 134:
#line 906 "ns_parser.y"
{	
		yyval.ul_int = (u_long)yyvsp[0].num;
	}
break;
case 135:
#line 910 "ns_parser.y"
{
		yyval.ul_int = 0;
	}
break;
case 136:
#line 914 "ns_parser.y"
{
		yyval.ul_int = ULONG_MAX;
	}
break;
case 137:
#line 920 "ns_parser.y"
{
		current_options->transfers_in = (u_long) yyvsp[0].num;
	}
break;
case 138:
#line 924 "ns_parser.y"
{
		current_options->transfers_out = (u_long) yyvsp[0].num;
	}
break;
case 139:
#line 928 "ns_parser.y"
{
		current_options->transfers_per_ns = (u_long) yyvsp[0].num;
	}
break;
case 142:
#line 938 "ns_parser.y"
{
		/* nothing */
	}
break;
case 143:
#line 942 "ns_parser.y"
{
		/* nothing */
	}
break;
case 144:
#line 948 "ns_parser.y"
{
	  	add_global_forwarder(current_options, yyvsp[0].ip_addr);
	}
break;
case 147:
#line 958 "ns_parser.y"
{
		/* nothing */
	}
break;
case 148:
#line 962 "ns_parser.y"
{
		/* nothing */
	}
break;
case 149:
#line 968 "ns_parser.y"
{
	  	add_global_also_notify(current_options, yyvsp[0].ip_addr);
	}
break;
case 150:
#line 978 "ns_parser.y"
{
		current_logging = begin_logging();
	}
break;
case 151:
#line 982 "ns_parser.y"
{
		end_logging(current_logging, 1);
		current_logging = NULL;
	}
break;
case 155:
#line 994 "ns_parser.y"
{
		current_category = yyvsp[0].s_int;
	}
break;
case 157:
#line 999 "ns_parser.y"
{
		chan_type = log_null;
		chan_flags = 0;
		chan_level = log_info;
	}
break;
case 158:
#line 1005 "ns_parser.y"
{
		log_channel current_channel = NULL;

		if (lookup_channel(yyvsp[-4].cp) != NULL) {
			parser_error(0, "can't redefine channel '%s'", yyvsp[-4].cp);
		} else {
			switch (chan_type) {
			case log_file:
				current_channel =
					log_new_file_channel(chan_flags,
							     chan_level,
							     chan_name, NULL,
							     chan_versions,
							     chan_max_size);
				log_set_file_owner(current_channel,
						   user_id, group_id);
				chan_name = freestr(chan_name);
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
		(void)freestr(yyvsp[-4].cp);
	}
break;
case 159:
#line 1047 "ns_parser.y"
{
		symbol_value value;

		if (lookup_symbol(constants, yyvsp[0].cp, SYM_LOGGING, &value)) {
			chan_level = value.integer;
		} else {
			parser_error(0, "unknown severity '%s'", yyvsp[0].cp);
			chan_level = log_debug(99);
		}
		(void)freestr(yyvsp[0].cp);
	}
break;
case 160:
#line 1059 "ns_parser.y"
{
		chan_level = log_debug(1);
	}
break;
case 161:
#line 1063 "ns_parser.y"
{
		chan_level = yyvsp[0].num;
	}
break;
case 162:
#line 1067 "ns_parser.y"
{
		chan_level = 0;
		chan_flags |= LOG_USE_CONTEXT_LEVEL|LOG_REQUIRE_DEBUG;
	}
break;
case 163:
#line 1074 "ns_parser.y"
{
		chan_versions = yyvsp[0].num;
	}
break;
case 164:
#line 1078 "ns_parser.y"
{
		chan_versions = LOG_MAX_VERSIONS;
	}
break;
case 165:
#line 1084 "ns_parser.y"
{
		chan_max_size = yyvsp[0].ul_int;
	}
break;
case 166:
#line 1090 "ns_parser.y"
{
		chan_versions = 0;
		chan_max_size = ULONG_MAX;
	}
break;
case 167:
#line 1095 "ns_parser.y"
{
		chan_max_size = ULONG_MAX;
	}
break;
case 168:
#line 1099 "ns_parser.y"
{
		chan_versions = 0;
	}
break;
case 171:
#line 1107 "ns_parser.y"
{
		chan_flags |= LOG_CLOSE_STREAM;
		chan_type = log_file;
		chan_name = yyvsp[-1].cp;
	}
break;
case 172:
#line 1115 "ns_parser.y"
{ yyval.cp = yyvsp[0].cp; }
break;
case 173:
#line 1116 "ns_parser.y"
{ yyval.cp = savestr("syslog", 1); }
break;
case 174:
#line 1119 "ns_parser.y"
{ yyval.s_int = LOG_DAEMON; }
break;
case 175:
#line 1121 "ns_parser.y"
{
		symbol_value value;

		if (lookup_symbol(constants, yyvsp[0].cp, SYM_SYSLOG, &value)) {
			yyval.s_int = value.integer;
		} else {
			parser_error(0, "unknown facility '%s'", yyvsp[0].cp);
			yyval.s_int = LOG_DAEMON;
		}
		(void)freestr(yyvsp[0].cp);
	}
break;
case 176:
#line 1135 "ns_parser.y"
{
		chan_type = log_syslog;
		chan_facility = yyvsp[0].s_int;
	}
break;
case 177:
#line 1141 "ns_parser.y"
{ /* nothing to do */ }
break;
case 178:
#line 1142 "ns_parser.y"
{ /* nothing to do */ }
break;
case 179:
#line 1144 "ns_parser.y"
{
		chan_type = log_null;
	}
break;
case 180:
#line 1147 "ns_parser.y"
{ /* nothing to do */ }
break;
case 181:
#line 1149 "ns_parser.y"
{
		if (yyvsp[0].num)
			chan_flags |= LOG_TIMESTAMP;
		else
			chan_flags &= ~LOG_TIMESTAMP;
	}
break;
case 182:
#line 1156 "ns_parser.y"
{
		if (yyvsp[0].num)
			chan_flags |= LOG_PRINT_CATEGORY;
		else
			chan_flags &= ~LOG_PRINT_CATEGORY;
	}
break;
case 183:
#line 1163 "ns_parser.y"
{
		if (yyvsp[0].num)
			chan_flags |= LOG_PRINT_LEVEL;
		else
			chan_flags &= ~LOG_PRINT_LEVEL;
	}
break;
case 188:
#line 1177 "ns_parser.y"
{ yyval.cp = savestr("null", 1); }
break;
case 189:
#line 1181 "ns_parser.y"
{
		log_channel channel;

		if (current_category >= 0) {
			channel = lookup_channel(yyvsp[0].cp);
			if (channel != NULL) {
				add_log_channel(current_logging,
						current_category, channel);
			} else
				parser_error(0, "unknown channel '%s'", yyvsp[0].cp);
		}
		(void)freestr(yyvsp[0].cp);
	}
break;
case 194:
#line 1202 "ns_parser.y"
{ yyval.cp = savestr("default", 1); }
break;
case 195:
#line 1203 "ns_parser.y"
{ yyval.cp = savestr("notify", 1); }
break;
case 196:
#line 1207 "ns_parser.y"
{
		symbol_value value;

		if (lookup_symbol(constants, yyvsp[0].cp, SYM_CATEGORY, &value))
			yyval.s_int = value.integer;
		else {
			parser_error(0, "invalid logging category '%s'",
				     yyvsp[0].cp);
			yyval.s_int = -1;
		}
		(void)freestr(yyvsp[0].cp);
	}
break;
case 197:
#line 1226 "ns_parser.y"
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
			define_symbol(symtab, ip_printable, SYM_SERVER, value,
				      0);
		current_server = begin_server(yyvsp[0].ip_addr);
	}
break;
case 198:
#line 1245 "ns_parser.y"
{
		end_server(current_server, !seen_server);
	}
break;
case 201:
#line 1255 "ns_parser.y"
{
		set_server_option(current_server, SERVER_INFO_BOGUS, yyvsp[0].num);
	}
break;
case 202:
#line 1259 "ns_parser.y"
{
		set_server_option(current_server, SERVER_INFO_SUPPORT_IXFR, yyvsp[0].num);
	}
break;
case 203:
#line 1263 "ns_parser.y"
{
		set_server_transfers(current_server, (int)yyvsp[0].num);
	}
break;
case 204:
#line 1267 "ns_parser.y"
{
		set_server_transfer_format(current_server, yyvsp[0].axfr_fmt);
	}
break;
case 207:
#line 1279 "ns_parser.y"
{
		ip_match_list iml;
		
		iml = new_ip_match_list();
		if (yyvsp[-1].ime != NULL)
			add_to_ip_match_list(iml, yyvsp[-1].ime);
		yyval.iml = iml;
	}
break;
case 208:
#line 1288 "ns_parser.y"
{
		if (yyvsp[-1].ime != NULL)
			add_to_ip_match_list(yyvsp[-2].iml, yyvsp[-1].ime);
		yyval.iml = yyvsp[-2].iml;
	}
break;
case 210:
#line 1297 "ns_parser.y"
{
		if (yyvsp[0].ime != NULL)
			ip_match_negate(yyvsp[0].ime);
		yyval.ime = yyvsp[0].ime;
	}
break;
case 211:
#line 1303 "ns_parser.y"
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
	        (void)freestr(key_name);
		freestr(yyvsp[0].cp);
	}
break;
case 212:
#line 1326 "ns_parser.y"
{
		yyval.ime = new_ip_match_pattern(yyvsp[0].ip_addr, 32);
	}
break;
case 213:
#line 1330 "ns_parser.y"
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
case 214:
#line 1342 "ns_parser.y"
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
case 216:
#line 1364 "ns_parser.y"
{
		char name[256];

		/*
		 * We want to be able to clean up this iml later so
		 * we give it a name and treat it like any other acl.
		 */
		sprintf(name, "__internal_%p", yyvsp[-1].iml);
		define_acl(name, yyvsp[-1].iml);
  		yyval.ime = new_ip_match_indirect(yyvsp[-1].iml);
	}
break;
case 217:
#line 1378 "ns_parser.y"
{
		ip_match_list iml;

		iml = lookup_acl(yyvsp[0].cp);
		if (iml == NULL) {
			parser_error(0, "unknown ACL '%s'", yyvsp[0].cp);
			yyval.ime = NULL;
		} else
			yyval.ime = new_ip_match_indirect(iml);
		(void)freestr(yyvsp[0].cp);
	}
break;
case 218:
#line 1396 "ns_parser.y"
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
			key_name = freestr(key_name);
		}
		(void)freestr(yyvsp[0].cp);
	}
break;
case 219:
#line 1419 "ns_parser.y"
{
		if (yyvsp[0].keyi == NULL)
			parser_error(0, "empty key not added to server list ");
		else
			add_server_key_info(current_server, yyvsp[0].keyi);
	}
break;
case 227:
#line 1440 "ns_parser.y"
{
		current_algorithm = NULL;
		current_secret = NULL;
	}
break;
case 228:
#line 1445 "ns_parser.y"
{
		struct dst_key *dst_key;
		char *key_name;

		key_name = canonical_name(yyvsp[-3].cp);
		if (key_name == NULL) {
			parser_error(0, "can't make key name '%s' canonical",
				     yyvsp[-3].cp);
		} else if (lookup_key(key_name) != NULL) {
			parser_error(0, "can't redefine key '%s'", key_name);
		} else {
			if (current_algorithm == NULL ||
			    current_secret == NULL)  {
				parser_error(0, "skipping bad key '%s'",
					     key_name);
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
		if (key_name != NULL)
			key_name = freestr(key_name);
		if (current_algorithm != NULL)
			current_algorithm = freestr(current_algorithm);
		if (current_secret != NULL)
			current_secret = freestr(current_secret);
		(void)freestr(yyvsp[-3].cp);
	}
break;
case 229:
#line 1485 "ns_parser.y"
{
		current_algorithm = yyvsp[-1].cp;
		current_secret = yyvsp[0].cp;
	}
break;
case 230:
#line 1490 "ns_parser.y"
{
		current_algorithm = yyvsp[0].cp;
		current_secret = yyvsp[-1].cp;
	}
break;
case 231:
#line 1495 "ns_parser.y"
{
		current_algorithm = NULL;
		current_secret = NULL;
	}
break;
case 232:
#line 1501 "ns_parser.y"
{ yyval.cp = yyvsp[-1].cp; }
break;
case 233:
#line 1504 "ns_parser.y"
{ yyval.cp = yyvsp[-1].cp; }
break;
case 234:
#line 1512 "ns_parser.y"
{
		if (lookup_acl(yyvsp[-3].cp) != NULL) {
			parser_error(0, "can't redefine ACL '%s'", yyvsp[-3].cp);
		} else
			define_acl(yyvsp[-3].cp, yyvsp[-1].iml);
		(void)freestr(yyvsp[-3].cp);
	}
break;
case 235:
#line 1526 "ns_parser.y"
{
		int sym_type;
		symbol_value value;
		char *zone_name;

		if (!seen_options && !logged_options_error) {
			parser_error(0,
             "no options statement before first zone; using previous/default");
			logged_options_error = 1;
		}
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
				define_symbol(symtab, zone_name, sym_type,
					      value, 0);
			}
		}
		(void)freestr(yyvsp[-1].cp);
		current_zone = begin_zone(zone_name, yyvsp[0].num); 
	}
break;
case 236:
#line 1561 "ns_parser.y"
{
		end_zone(current_zone, should_install);
	}
break;
case 239:
#line 1571 "ns_parser.y"
{
		yyval.num = C_IN;
	}
break;
case 240:
#line 1575 "ns_parser.y"
{
		symbol_value value;

		if (lookup_symbol(constants, yyvsp[0].cp, SYM_CLASS, &value))
			yyval.num = value.integer;
		else {
			/* the zone validator will give the error */
			yyval.num = C_NONE;
		}
		(void)freestr(yyvsp[0].cp);
	}
break;
case 241:
#line 1589 "ns_parser.y"
{
		yyval.s_int = Z_MASTER;
	}
break;
case 242:
#line 1593 "ns_parser.y"
{
		yyval.s_int = Z_SLAVE;
	}
break;
case 243:
#line 1597 "ns_parser.y"
{
		yyval.s_int = Z_HINT;
	}
break;
case 244:
#line 1601 "ns_parser.y"
{
		yyval.s_int = Z_STUB;
	}
break;
case 245:
#line 1605 "ns_parser.y"
{
		yyval.s_int = Z_FORWARD;
	}
break;
case 248:
#line 1615 "ns_parser.y"
{
		if (!set_zone_type(current_zone, yyvsp[0].s_int))
			parser_warning(0, "zone type already set; skipping");
	}
break;
case 249:
#line 1620 "ns_parser.y"
{
		if (!set_zone_filename(current_zone, yyvsp[0].cp))
			parser_warning(0,
				       "zone filename already set; skipping");
	}
break;
case 250:
#line 1626 "ns_parser.y"
{
		if (!set_zone_ixfr_file(current_zone, yyvsp[0].cp))
			parser_warning(0,
				       "zone ixfr data base already set; skipping");
	}
break;
case 251:
#line 1632 "ns_parser.y"
{
		if (!set_zone_ixfr_tmp(current_zone, yyvsp[0].cp))
			parser_warning(0,
				       "zone ixfr temp filename already set; skipping");
	}
break;
case 252:
#line 1638 "ns_parser.y"
{
		set_zone_master_port(current_zone, yyvsp[-3].us_int);
	}
break;
case 253:
#line 1642 "ns_parser.y"
{
		set_zone_transfer_source(current_zone, yyvsp[0].ip_addr);
	}
break;
case 254:
#line 1646 "ns_parser.y"
{
		if (!set_zone_checknames(current_zone, (enum severity)yyvsp[0].s_int))
			parser_warning(0,
	                              "zone checknames already set; skipping");
	}
break;
case 255:
#line 1652 "ns_parser.y"
{
		if (!set_zone_update_acl(current_zone, yyvsp[-1].iml))
			parser_warning(0,
				      "zone update acl already set; skipping");
	}
break;
case 256:
#line 1658 "ns_parser.y"
{
		if (!set_zone_query_acl(current_zone, yyvsp[-1].iml))
			parser_warning(0,
				      "zone query acl already set; skipping");
	}
break;
case 257:
#line 1664 "ns_parser.y"
{
		if (!set_zone_transfer_acl(current_zone, yyvsp[-1].iml))
			parser_warning(0,
				    "zone transfer acl already set; skipping");
	}
break;
case 259:
#line 1671 "ns_parser.y"
{
		struct zoneinfo *zp = current_zone.opaque;
		if (zp->z_fwdtab) {
                	free_forwarders(zp->z_fwdtab);
			zp->z_fwdtab = NULL;
		}

	}
break;
case 261:
#line 1681 "ns_parser.y"
{
		if (!set_zone_transfer_time_in(current_zone, yyvsp[0].num*60))
			parser_warning(0,
		       "zone max transfer time (in) already set; skipping");
	}
break;
case 262:
#line 1687 "ns_parser.y"
{
		set_zone_max_log_size_ixfr(current_zone, yyvsp[0].ul_int);
        }
break;
case 263:
#line 1691 "ns_parser.y"
{
		set_zone_notify(current_zone, notify_explicit);
	}
break;
case 264:
#line 1695 "ns_parser.y"
{
		if (yyvsp[0].num)
			set_zone_notify(current_zone, notify_yes);
		else
			set_zone_notify(current_zone, notify_no);
	}
break;
case 265:
#line 1702 "ns_parser.y"
{
		set_zone_maintain_ixfr_base(current_zone, yyvsp[0].num);
	}
break;
case 266:
#line 1706 "ns_parser.y"
{
		/* flags proto alg key */
		set_zone_pubkey(current_zone, yyvsp[-3].num, yyvsp[-2].num, yyvsp[-1].num, yyvsp[0].cp);
	}
break;
case 267:
#line 1711 "ns_parser.y"
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
case 269:
#line 1723 "ns_parser.y"
{
		 set_zone_dialup(current_zone, yyvsp[0].num);
	}
break;
case 271:
#line 1730 "ns_parser.y"
{
		/* nothing */
	}
break;
case 272:
#line 1734 "ns_parser.y"
{
		/* nothing */
	}
break;
case 273:
#line 1740 "ns_parser.y"
{
	  	add_zone_master(current_zone, yyvsp[0].ip_addr, NULL);
	}
break;
case 274:
#line 1744 "ns_parser.y"
{
		add_zone_master(current_zone, yyvsp[-2].ip_addr, yyvsp[0].keyi);
	}
break;
case 277:
#line 1754 "ns_parser.y"
{
		/* nothing */
	}
break;
case 278:
#line 1758 "ns_parser.y"
{
		/* nothing */
	}
break;
case 279:
#line 1764 "ns_parser.y"
{
	  	add_zone_notify(current_zone, yyvsp[0].ip_addr);
	}
break;
case 280:
#line 1770 "ns_parser.y"
{
		set_zone_boolean_option(current_zone, OPTION_FORWARD_ONLY, 1);
	}
break;
case 281:
#line 1774 "ns_parser.y"
{
		set_zone_boolean_option(current_zone, OPTION_FORWARD_ONLY, 0);
	}
break;
case 282:
#line 1780 "ns_parser.y"
{
		set_zone_forward(current_zone);
	}
break;
case 284:
#line 1787 "ns_parser.y"
{
		/* nothing */
	}
break;
case 285:
#line 1791 "ns_parser.y"
{
		/* nothing */
	}
break;
case 286:
#line 1797 "ns_parser.y"
{
	  	add_zone_forwarder(current_zone, yyvsp[0].ip_addr);
	}
break;
case 287:
#line 1807 "ns_parser.y"
{
	}
break;
case 288:
#line 1811 "ns_parser.y"
{
		/* nothing */
	}
break;
case 289:
#line 1815 "ns_parser.y"
{
		/* nothing */
	}
break;
case 290:
#line 1820 "ns_parser.y"
{
		/* name flags proto alg key */
		set_trusted_key(yyvsp[-4].cp, yyvsp[-3].num, yyvsp[-2].num, yyvsp[-1].num, yyvsp[0].cp);
	}
break;
case 291:
#line 1825 "ns_parser.y"
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
case 292:
#line 1841 "ns_parser.y"
{
		if (yyvsp[0].num < 0 || yyvsp[0].num > 65535) {
		  	parser_warning(0, 
			  "invalid IP port number '%d'; setting port to 0",
			               (int)yyvsp[0].num);
			yyvsp[0].num = 0;
		} else
			yyval.us_int = htons(yyvsp[0].num);
	}
break;
#line 3214 "y.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
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
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
