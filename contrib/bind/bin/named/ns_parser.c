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
static char rcsid[] = "$Id: ns_parser.y,v 8.11 1997/12/04 07:03:05 halley Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996, 1997 by Internet Software Consortium.
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

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>

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
static int seen_zone;

static options current_options;
static int seen_options;

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

#line 96 "ns_parser.y"
typedef union {
	char *			cp;
	int			s_int;
	long			num;
	u_long			ul_int;
	u_int16_t		us_int;
	struct in_addr		ip_addr;
	ip_match_element	ime;
	ip_match_list		iml;
	key_info		keyi;
	enum axfr_format	axfr_fmt;
} YYSTYPE;
#line 121 "y.tab.c"
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
#define T_DATASIZE 278
#define T_STACKSIZE 279
#define T_CORESIZE 280
#define T_DEFAULT 281
#define T_UNLIMITED 282
#define T_FILES 283
#define T_HOSTSTATS 284
#define T_DEALLOC_ON_EXIT 285
#define T_TRANSFERS_IN 286
#define T_TRANSFERS_OUT 287
#define T_TRANSFERS_PER_NS 288
#define T_TRANSFER_FORMAT 289
#define T_MAX_TRANSFER_TIME_IN 290
#define T_ONE_ANSWER 291
#define T_MANY_ANSWERS 292
#define T_NOTIFY 293
#define T_AUTH_NXDOMAIN 294
#define T_MULTIPLE_CNAMES 295
#define T_CLEAN_INTERVAL 296
#define T_INTERFACE_INTERVAL 297
#define T_STATS_INTERVAL 298
#define T_LOGGING 299
#define T_CATEGORY 300
#define T_CHANNEL 301
#define T_SEVERITY 302
#define T_DYNAMIC 303
#define T_FILE 304
#define T_VERSIONS 305
#define T_SIZE 306
#define T_SYSLOG 307
#define T_DEBUG 308
#define T_NULL_OUTPUT 309
#define T_PRINT_TIME 310
#define T_PRINT_CATEGORY 311
#define T_PRINT_SEVERITY 312
#define T_TOPOLOGY 313
#define T_SERVER 314
#define T_LONG_AXFR 315
#define T_BOGUS 316
#define T_TRANSFERS 317
#define T_KEYS 318
#define T_ZONE 319
#define T_IN 320
#define T_CHAOS 321
#define T_HESIOD 322
#define T_TYPE 323
#define T_MASTER 324
#define T_SLAVE 325
#define T_STUB 326
#define T_RESPONSE 327
#define T_HINT 328
#define T_MASTERS 329
#define T_TRANSFER_SOURCE 330
#define T_ALSO_NOTIFY 331
#define T_ACL 332
#define T_ALLOW_UPDATE 333
#define T_ALLOW_QUERY 334
#define T_ALLOW_TRANSFER 335
#define T_SEC_KEY 336
#define T_ALGID 337
#define T_SECRET 338
#define T_CHECK_NAMES 339
#define T_WARN 340
#define T_FAIL 341
#define T_IGNORE 342
#define T_FORWARD 343
#define T_FORWARDERS 344
#define T_ONLY 345
#define T_FIRST 346
#define T_IF_NO_ANSWER 347
#define T_IF_NO_DOMAIN 348
#define T_YES 349
#define T_TRUE 350
#define T_NO 351
#define T_FALSE 352
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,   25,   25,   26,   26,   26,   26,   26,   26,   26,
   26,   26,   26,   27,   34,   28,   35,   35,   36,   36,
   36,   36,   36,   36,   36,   36,   36,   36,   36,   36,
   36,   36,   36,   36,   36,   36,   38,   36,   36,   36,
   36,   36,   36,   36,   36,   36,   36,   36,   36,   36,
    5,    5,    4,    4,    3,    3,   43,   44,   40,   40,
   40,   40,    2,    2,   23,   23,   23,   23,   23,   21,
   21,   21,   22,   22,   22,   37,   37,   37,   37,   41,
   41,   41,   41,   20,   20,   20,   20,   42,   42,   42,
   39,   39,   45,   45,   46,   47,   29,   48,   48,   48,
   50,   49,   52,   49,   54,   54,   54,   54,   55,   55,
   56,   57,   57,   57,   57,   57,   58,    9,    9,   10,
   10,   59,   60,   60,   60,   60,   60,   60,   60,   53,
   53,   53,    8,    8,   61,   51,   51,   51,    7,    7,
    7,    6,   62,   30,   63,   63,   64,   64,   64,   64,
   64,   14,   14,   12,   12,   11,   11,   11,   11,   11,
   13,   17,   66,   65,   65,   65,   67,   33,   68,   68,
   68,   18,   19,   32,   70,   31,   69,   69,   15,   15,
   16,   16,   16,   16,   71,   71,   72,   72,   72,   72,
   72,   72,   72,   72,   72,   72,   72,   72,   73,   73,
   75,   74,   74,   76,   76,   77,    1,   24,   24,
};
short yylen[] = {                                         2,
    1,    1,    2,    1,    2,    2,    2,    2,    2,    2,
    1,    2,    2,    3,    0,    5,    2,    3,    0,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    3,    5,    2,    0,    5,    2,    4,
    4,    4,    1,    1,    2,    2,    2,    2,    2,    1,
    1,    1,    1,    1,    1,    1,    2,    2,    1,    1,
    2,    2,    0,    2,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    2,
    2,    2,    2,    1,    1,    1,    1,    2,    2,    2,
    0,    1,    2,    3,    1,    0,    5,    2,    3,    1,
    0,    6,    0,    6,    1,    1,    2,    1,    2,    2,
    2,    0,    1,    1,    2,    2,    3,    1,    1,    0,
    1,    2,    1,    1,    1,    2,    2,    2,    2,    2,
    3,    1,    1,    1,    1,    2,    3,    1,    1,    1,
    1,    1,    0,    6,    2,    3,    2,    2,    2,    4,
    1,    2,    3,    1,    2,    1,    3,    3,    1,    3,
    1,    1,    1,    2,    3,    1,    0,    6,    2,    2,
    1,    3,    3,    5,    0,    5,    0,    3,    0,    1,
    1,    1,    1,    1,    2,    3,    2,    2,    4,    2,
    2,    4,    4,    4,    2,    2,    4,    1,    2,    3,
    1,    0,    1,    2,    3,    1,    1,    1,    1,
};
short yydefred[] = {                                      0,
    0,   11,    0,   15,   96,    0,    0,    0,  167,    0,
    0,    2,    4,    0,    0,    0,    0,    0,    0,   12,
   13,    0,    0,    0,  143,    0,  208,  209,    0,    0,
    3,    5,    6,    7,    8,    9,   10,   14,    0,    0,
    0,  175,  180,    0,    0,   50,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   37,
    0,    0,   43,   44,  100,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  154,    0,  159,    0,  161,
    0,   20,   22,   21,   25,   23,   24,   69,   65,   66,
   67,   68,   26,   27,   28,    0,    0,   39,    0,    0,
    0,    0,   85,   86,   87,   80,   84,   81,   82,   83,
   30,   31,   88,   89,   90,   51,   52,   45,   46,   29,
   32,   33,   47,   48,   49,    0,    0,    0,   70,   71,
   72,    0,   76,   77,   78,   79,   36,    0,   16,    0,
   17,  140,  141,  101,  142,  139,  134,  103,  133,   97,
    0,   98,  151,    0,    0,    0,    0,    0,    0,    0,
  176,    0,    0,    0,  155,  152,  174,    0,  171,    0,
    0,    0,    0,    0,  207,   56,   55,   58,   53,   54,
   57,   61,   62,   64,    0,    0,    0,    0,   73,   74,
   75,   34,    0,   18,    0,    0,   99,  149,  147,  148,
    0,  144,    0,  145,  198,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  157,  158,
  160,  153,    0,    0,  169,  170,  168,    0,   42,   40,
   41,   95,    0,    0,    0,    0,    0,  166,  163,  162,
    0,    0,  146,  195,  196,  188,  181,  182,  184,  183,
  187,    0,  190,    0,    0,    0,    0,  191,  178,    0,
  185,  172,  173,   35,   38,    0,   93,  138,  135,    0,
    0,  132,    0,    0,    0,  125,    0,    0,    0,    0,
  123,  124,    0,  150,    0,  164,  201,    0,    0,  206,
    0,    0,    0,    0,    0,    0,  186,   94,  102,    0,
  136,  108,    0,  105,  126,    0,  119,  121,  122,  118,
  127,  128,  129,  104,    0,  130,  165,  189,    0,  199,
  197,    0,  204,  192,  193,  194,  137,  107,    0,    0,
    0,    0,  117,  131,  200,  205,  109,  110,  111,  115,
  116,
};
short yydgoto[] = {                                      10,
  197,  122,  198,  201,  138,  164,  165,  289,  328,  329,
   96,   97,   98,   99,   42,  271,  259,  192,  193,  126,
  152,  212,  113,  100,   11,   12,   13,   14,   15,   16,
   17,   18,   19,   23,   81,   82,  157,  158,  253,  118,
   83,   84,  119,  120,  254,  255,   24,   88,   89,  215,
  290,  216,  300,  325,  351,  352,  353,  301,  302,  303,
  291,   41,  178,  179,  261,  262,   30,  194,  181,   91,
  237,  238,  308,  311,  309,  312,  313,
};
short yysindex[] = {                                    122,
 -200,    0, -238,    0,    0, -227, -228, -188,    0,    0,
  122,    0,    0, -220, -176, -164, -158, -155, -149,    0,
    0, -147,  -88,   -4,    0, -188,    0,    0,    4, -188,
    0,    0,    0,    0,    0,    0,    0,    0,   79, -240,
   11,    0,    0,   16,   20,    0, -218,  -60,  -40,  -34,
  -24,  -14,  -53,  -53,  -53, -193,  -95, -190, -190, -190,
 -190,  -53,  -53,  -98,  -56,  -41, -162,  -31,  -53,  -53,
  -53,    6,   19,   22,  102,  161,  163, -204,  -57,    0,
 -120,   36,    0,    0,    0,  -73, -205, -115,   38, -241,
  177,  257,  258,   16,  -69,    0,   49,    0,  -29,    0,
 -226,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  -33,  -36,    0,   31,   32,
   51,  185,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   16,   16,   16,    0,    0,
    0, -277,    0,    0,    0,    0,    0,  188,    0,   55,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   57,    0,    0, -162,  -53,   56,  193, -118,   60,  104,
    0,   59,   63,  -25,    0,    0,    0,   69,    0, -188,
 -188,  -11,   -7,  203,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   16,  -20,  -16,   -9,    0,    0,
    0,    0,   73,    0,  209,  210,    0,    0,    0,    0,
 -143,    0,   77,    0,    0,   78,  -53,   75, -184,  215,
  -36,  216,  217,  218,  232, -277,  -10,   99,    0,    0,
    0,    0,  113,  114,    0,    0,    0,   -1,    0,    0,
    0,    0,  236,   73,  123, -214, -222,    0,    0,    0,
  -81,  124,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  121,    0,  125,   16,   16,   16,    0,    0,  130,
    0,    0,    0,    0,    0,  131,    0,    0,    0, -104,
  132,    0, -202,  129, -221,    0,  -53,  -53,  -53, -100,
    0,    0,  134,    0,  136,    0,    0,  -96,  138,    0,
  271,  125,  141,    3,    8,   12,    0,    0,    0,  142,
    0,    0,  143,    0,    0,  -89,    0,    0,    0,    0,
    0,    0,    0,    0,  144,    0,    0,    0,  146,    0,
    0,  147,    0,    0,    0,    0,    0,    0, -185, -190,
   76,  100,    0,    0,    0,    0,    0,    0,    0,    0,
    0,
};
short yyrindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  400,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  -85,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  149,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  284,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  149,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  155,  158,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  159,  160,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  294,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  295,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  299,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  168,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  301,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  171,    0,    0,  172,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  173,  174,    0,    0,    0,    0,    0,    0,    0,    0,
    0,
};
short yygindex[] = {                                      0,
  311,    0,    0,  211,  266,    0,    0,  357,    0,    0,
  350,   95,    0,  -80,    0,    0,    0,  253,  255,  -58,
    0,  212,  -43,   -8,    0,  438,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  369,    0,    0,    0,    0,
    0,    0,  331,  333,    0,  199,    0,    0,  367,    0,
    0,    0,    0,    0,  105,  108,    0,    0,    0,  156,
  170,    0,    0,  283,    0,  201,    0,    0,    0,    0,
    0,  226,    0,    0,  157,    0,  152,
};
#define YYTABLESIZE 465
short yytable[] = {                                      29,
  128,  129,  130,   95,  159,  200,  222,   95,  196,  170,
  114,  115,   95,  184,  173,   85,   95,   43,  131,  132,
  319,   45,   22,   95,  334,  140,  141,  142,  338,  189,
   25,   95,   26,  292,   39,   95,   32,  179,   27,   28,
   95,  288,  102,  304,   95,   27,   28,  174,   95,  127,
  127,  127,  127,   94,   27,   28,   20,   27,   28,   86,
   87,   21,  209,  210,  211,  206,  207,  208,  123,   27,
   28,   27,   28,  357,  175,  176,  177,  166,  169,  293,
   33,  294,  116,  117,  295,  327,  296,  297,  298,  299,
  124,  125,   34,   94,  167,  187,  358,   94,   35,  241,
  322,   36,   94,  167,  249,  323,   94,   37,  250,   38,
  190,  191,  258,   94,  279,  251,   27,   28,   40,  149,
  150,   94,  151,  284,  248,   94,   44,  344,  136,  137,
   94,  219,  345,   90,   94,   46,  346,  173,   94,  267,
  268,  269,  101,  270,   47,   48,   49,   50,   51,   52,
   53,   54,   55,   56,   57,   27,   28,   58,   59,   60,
  133,  307,   61,   62,   63,   64,   65,   66,   67,   68,
  174,  179,   69,   70,   71,   72,   73,   74,   27,   28,
  121,  243,  244,  265,   86,   87,   27,   28,   92,   93,
   27,   28,   75,  188,  314,  315,  316,  175,  176,  177,
  103,  293,  134,  294,  167,  108,  295,  162,  296,  297,
  298,  299,  260,   76,   77,  349,  350,  135,   78,  163,
  104,  199,   79,   80,  146,  195,  105,  139,   92,   93,
   27,   28,   92,   93,   27,   28,  106,   92,   93,   27,
   28,   92,   93,   27,   28,  225,  107,  169,   92,   93,
   27,   28,  260,  331,  332,  333,   92,   93,   27,   28,
   92,   93,   27,   28,  143,   92,   93,   27,   28,   92,
   93,   27,   28,   92,   93,   27,   28,  144,  188,  226,
  145,  169,  227,  147,  324,  148,  330,  153,  154,  155,
  156,  359,  161,  228,  172,  109,  110,  111,  112,  180,
  188,  188,  188,  182,  183,  186,  116,  205,  117,  195,
  213,  214,  229,  217,  220,  221,  224,  239,  230,  231,
  232,  240,  233,  234,  235,  242,  191,  247,  236,  190,
  252,  256,  257,  263,   46,  266,  264,  272,  274,  275,
  276,  127,  188,   47,   48,   49,   50,   51,   52,   53,
   54,   55,   56,   57,  277,  281,   58,   59,   60,  225,
  285,   61,   62,   63,   64,   65,   66,   67,   68,  282,
  283,   69,   70,   71,   72,   73,   74,    1,  307,  287,
  306,  350,  310,    2,    3,    4,  317,  318,  321,  326,
  336,   75,  337,  226,  340,  341,  227,  343,  347,    1,
  354,  348,  355,  356,  349,   19,   63,  228,  188,  188,
  188,  177,   76,   77,  156,   59,   60,   78,   91,   92,
    5,   79,   80,  202,  120,  203,  229,  106,  112,  113,
  114,  204,  230,  231,  232,    6,  233,  234,  235,  218,
    7,  273,  236,  168,  185,  246,  245,  278,   31,  160,
  203,  202,  286,    8,  171,  335,  361,    9,  360,  320,
  223,  305,  280,  342,  339,
};
short yycheck[] = {                                       8,
   59,   60,   61,   33,  125,   42,  125,   33,   42,  125,
   54,   55,   33,   94,  256,  256,   33,   26,   62,   63,
  125,   30,  261,   33,  125,   69,   70,   71,  125,  256,
  258,   33,  261,  256,  123,   33,  257,  123,  260,  261,
   33,  256,  261,  125,   33,  260,  261,  289,   33,   58,
   59,   60,   61,  123,  260,  261,  257,  260,  261,  300,
  301,  262,  340,  341,  342,  146,  147,  148,  259,  260,
  261,  260,  261,  259,  316,  317,  318,   86,   87,  302,
  257,  304,  276,  277,  307,  307,  309,  310,  311,  312,
  281,  282,  257,  123,  309,  125,  282,  123,  257,  125,
  303,  257,  123,  309,  125,  308,  123,  257,  125,  257,
  337,  338,  256,  123,  125,  125,  260,  261,  123,  324,
  325,  123,  327,  125,  205,  123,  123,  125,  291,  292,
  123,  175,  125,  123,  123,  256,  125,  256,  123,  324,
  325,  326,  123,  328,  265,  266,  267,  268,  269,  270,
  271,  272,  273,  274,  275,  260,  261,  278,  279,  280,
  259,  258,  283,  284,  285,  286,  287,  288,  289,  290,
  289,  257,  293,  294,  295,  296,  297,  298,  260,  261,
  276,  190,  191,  227,  300,  301,  260,  261,  258,  259,
  260,  261,  313,   99,  275,  276,  277,  316,  317,  318,
  261,  302,  259,  304,  309,  259,  307,  281,  309,  310,
  311,  312,  221,  334,  335,  305,  306,  259,  339,  293,
  261,  258,  343,  344,  123,  259,  261,  259,  258,  259,
  260,  261,  258,  259,  260,  261,  261,  258,  259,  260,
  261,  258,  259,  260,  261,  256,  261,  256,  258,  259,
  260,  261,  261,  297,  298,  299,  258,  259,  260,  261,
  258,  259,  260,  261,  259,  258,  259,  260,  261,  258,
  259,  260,  261,  258,  259,  260,  261,  259,  184,  290,
  259,  290,  293,  123,  293,  123,  295,  345,  346,  347,
  348,  350,  257,  304,  257,  349,  350,  351,  352,  123,
  206,  207,  208,   47,   47,  257,  276,  123,  277,  259,
  123,  257,  323,  257,  259,  123,  257,  259,  329,  330,
  331,  259,  333,  334,  335,  257,  338,  125,  339,  337,
  258,  123,  123,  257,  256,  261,  259,  123,  123,  123,
  123,  350,  248,  265,  266,  267,  268,  269,  270,  271,
  272,  273,  274,  275,  123,  257,  278,  279,  280,  256,
  125,  283,  284,  285,  286,  287,  288,  289,  290,  257,
  257,  293,  294,  295,  296,  297,  298,  256,  258,  257,
  257,  306,  258,  262,  263,  264,  257,  257,  257,  261,
  257,  313,  257,  290,  257,  125,  293,  257,  257,    0,
  257,  259,  257,  257,  305,  257,  123,  304,  314,  315,
  316,  257,  334,  335,  257,  257,  257,  339,  125,  125,
  299,  343,  344,  125,  257,  125,  323,  257,  257,  257,
  257,  121,  329,  330,  331,  314,  333,  334,  335,  174,
  319,  231,  339,   87,   95,  193,  192,  236,   11,   81,
  120,  119,  254,  332,   88,  300,  352,  336,  351,  290,
  178,  261,  237,  312,  308,
};
#define YYFINAL 10
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 352
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
"T_QUERY_SOURCE","T_LISTEN_ON","T_PORT","T_ADDRESS","T_DATASIZE","T_STACKSIZE",
"T_CORESIZE","T_DEFAULT","T_UNLIMITED","T_FILES","T_HOSTSTATS",
"T_DEALLOC_ON_EXIT","T_TRANSFERS_IN","T_TRANSFERS_OUT","T_TRANSFERS_PER_NS",
"T_TRANSFER_FORMAT","T_MAX_TRANSFER_TIME_IN","T_ONE_ANSWER","T_MANY_ANSWERS",
"T_NOTIFY","T_AUTH_NXDOMAIN","T_MULTIPLE_CNAMES","T_CLEAN_INTERVAL",
"T_INTERFACE_INTERVAL","T_STATS_INTERVAL","T_LOGGING","T_CATEGORY","T_CHANNEL",
"T_SEVERITY","T_DYNAMIC","T_FILE","T_VERSIONS","T_SIZE","T_SYSLOG","T_DEBUG",
"T_NULL_OUTPUT","T_PRINT_TIME","T_PRINT_CATEGORY","T_PRINT_SEVERITY",
"T_TOPOLOGY","T_SERVER","T_LONG_AXFR","T_BOGUS","T_TRANSFERS","T_KEYS","T_ZONE",
"T_IN","T_CHAOS","T_HESIOD","T_TYPE","T_MASTER","T_SLAVE","T_STUB","T_RESPONSE",
"T_HINT","T_MASTERS","T_TRANSFER_SOURCE","T_ALSO_NOTIFY","T_ACL",
"T_ALLOW_UPDATE","T_ALLOW_QUERY","T_ALLOW_TRANSFER","T_SEC_KEY","T_ALGID",
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
"statement : logging_stmt L_EOS",
"statement : server_stmt L_EOS",
"statement : zone_stmt L_EOS",
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
"option : T_AUTH_NXDOMAIN yea_or_nay",
"option : T_MULTIPLE_CNAMES yea_or_nay",
"option : T_CHECK_NAMES check_names_type check_names_opt",
"option : T_LISTEN_ON maybe_port '{' address_match_list '}'",
"option : T_FORWARD forward_opt",
"$$2 :",
"option : T_FORWARDERS $$2 '{' opt_forwarders_list '}'",
"option : T_QUERY_SOURCE query_source",
"option : T_ALLOW_QUERY '{' address_match_list '}'",
"option : T_ALLOW_TRANSFER '{' address_match_list '}'",
"option : T_TOPOLOGY '{' address_match_list '}'",
"option : size_clause",
"option : transfer_clause",
"option : T_TRANSFER_FORMAT transfer_format",
"option : T_MAX_TRANSFER_TIME_IN L_NUMBER",
"option : T_CLEAN_INTERVAL L_NUMBER",
"option : T_INTERFACE_INTERVAL L_NUMBER",
"option : T_STATS_INTERVAL L_NUMBER",
"option : error",
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
"$$3 :",
"logging_stmt : T_LOGGING $$3 '{' logging_opts_list '}'",
"logging_opts_list : logging_opt L_EOS",
"logging_opts_list : logging_opts_list logging_opt L_EOS",
"logging_opts_list : error",
"$$4 :",
"logging_opt : T_CATEGORY category $$4 '{' channel_list '}'",
"$$5 :",
"logging_opt : T_CHANNEL channel_name $$5 '{' channel_opt_list '}'",
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
"$$6 :",
"server_stmt : T_SERVER L_IPADDR $$6 '{' server_info_list '}'",
"server_info_list : server_info L_EOS",
"server_info_list : server_info_list server_info L_EOS",
"server_info : T_BOGUS yea_or_nay",
"server_info : T_TRANSFERS L_NUMBER",
"server_info : T_TRANSFER_FORMAT transfer_format",
"server_info : T_KEYS '{' key_list '}'",
"server_info : error",
"address_match_list : address_match_element L_EOS",
"address_match_list : address_match_list address_match_element L_EOS",
"address_match_element : address_match_simple",
"address_match_element : '!' address_match_simple",
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
"$$7 :",
"key_stmt : T_SEC_KEY $$7 any_string '{' key_definition '}'",
"key_definition : algorithm_id secret",
"key_definition : secret algorithm_id",
"key_definition : error",
"algorithm_id : T_ALGID any_string L_EOS",
"secret : T_SECRET any_string L_EOS",
"acl_stmt : T_ACL any_string '{' address_match_list '}'",
"$$8 :",
"zone_stmt : T_ZONE L_QSTRING optional_class $$8 optional_zone_options_list",
"optional_zone_options_list :",
"optional_zone_options_list : '{' zone_option_list '}'",
"optional_class :",
"optional_class : any_string",
"zone_type : T_MASTER",
"zone_type : T_SLAVE",
"zone_type : T_HINT",
"zone_type : T_STUB",
"zone_option_list : zone_option L_EOS",
"zone_option_list : zone_option_list zone_option L_EOS",
"zone_option : T_TYPE zone_type",
"zone_option : T_FILE L_QSTRING",
"zone_option : T_MASTERS '{' master_in_addr_list '}'",
"zone_option : T_TRANSFER_SOURCE maybe_wild_addr",
"zone_option : T_CHECK_NAMES check_names_opt",
"zone_option : T_ALLOW_UPDATE '{' address_match_list '}'",
"zone_option : T_ALLOW_QUERY '{' address_match_list '}'",
"zone_option : T_ALLOW_TRANSFER '{' address_match_list '}'",
"zone_option : T_MAX_TRANSFER_TIME_IN L_NUMBER",
"zone_option : T_NOTIFY yea_or_nay",
"zone_option : T_ALSO_NOTIFY '{' opt_notify_in_addr_list '}'",
"zone_option : error",
"master_in_addr_list : master_in_addr L_EOS",
"master_in_addr_list : master_in_addr_list master_in_addr L_EOS",
"master_in_addr : L_IPADDR",
"opt_notify_in_addr_list :",
"opt_notify_in_addr_list : notify_in_addr_list",
"notify_in_addr_list : notify_in_addr L_EOS",
"notify_in_addr_list : notify_in_addr_list notify_in_addr L_EOS",
"notify_in_addr : L_IPADDR",
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
#line 1282 "ns_parser.y"

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

key_info
lookup_key(char *name) {
	symbol_value value;

	if (lookup_symbol(authtab, name, SYM_KEY, &value))
		return ((key_info)(value.pointer));
	return (NULL);
}

void
define_key(char *name, key_info ki) {
	symbol_value value;

	INSIST(name != NULL);
	INSIST(ki != NULL);

	value.pointer = ki;
	define_symbol(authtab, name, SYM_KEY, value, SYMBOL_FREE_VALUE);
	dprint_key_info(ki);
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
#line 965 "y.tab.c"
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
#line 206 "ns_parser.y"
{
		/* nothing */
	}
break;
case 14:
#line 227 "ns_parser.y"
{ lexer_begin_file(yyvsp[-1].cp, NULL); }
break;
case 15:
#line 235 "ns_parser.y"
{
		if (seen_options)
			parser_error(0, "cannot redefine options");
		current_options = new_options();
	}
break;
case 16:
#line 241 "ns_parser.y"
{
		if (!seen_options)
			set_options(current_options, 0);
		else
			free_options(current_options);
		current_options = NULL;
		seen_options = 1;
	}
break;
case 20:
#line 257 "ns_parser.y"
{
		if (current_options->directory != NULL)
			freestr(current_options->directory);
		current_options->directory = yyvsp[0].cp;
	}
break;
case 21:
#line 263 "ns_parser.y"
{
		if (current_options->named_xfer != NULL)
			freestr(current_options->named_xfer);
		current_options->named_xfer = yyvsp[0].cp;
	}
break;
case 22:
#line 269 "ns_parser.y"
{
		if (current_options->pid_filename != NULL)
			freestr(current_options->pid_filename);
		current_options->pid_filename = yyvsp[0].cp;
	}
break;
case 23:
#line 275 "ns_parser.y"
{
		if (current_options->stats_filename != NULL)
			freestr(current_options->stats_filename);
		current_options->stats_filename = yyvsp[0].cp;
	}
break;
case 24:
#line 281 "ns_parser.y"
{
		if (current_options->memstats_filename != NULL)
			freestr(current_options->memstats_filename);
		current_options->memstats_filename = yyvsp[0].cp;
	}
break;
case 25:
#line 287 "ns_parser.y"
{
		if (current_options->dump_filename != NULL)
			freestr(current_options->dump_filename);
		current_options->dump_filename = yyvsp[0].cp;
	}
break;
case 26:
#line 293 "ns_parser.y"
{
		set_boolean_option(current_options, OPTION_FAKE_IQUERY, yyvsp[0].num);
	}
break;
case 27:
#line 297 "ns_parser.y"
{
		set_boolean_option(current_options, OPTION_NORECURSE, !yyvsp[0].num);
	}
break;
case 28:
#line 301 "ns_parser.y"
{
		set_boolean_option(current_options, OPTION_NOFETCHGLUE, !yyvsp[0].num);
	}
break;
case 29:
#line 305 "ns_parser.y"
{
		set_boolean_option(current_options, OPTION_NONOTIFY, !yyvsp[0].num);
	}
break;
case 30:
#line 309 "ns_parser.y"
{
		set_boolean_option(current_options, OPTION_HOSTSTATS, yyvsp[0].num);
	}
break;
case 31:
#line 313 "ns_parser.y"
{
		set_boolean_option(current_options, OPTION_DEALLOC_ON_EXIT,
				   yyvsp[0].num);
	}
break;
case 32:
#line 318 "ns_parser.y"
{
		set_boolean_option(current_options, OPTION_NONAUTH_NXDOMAIN,
				   !yyvsp[0].num);
	}
break;
case 33:
#line 323 "ns_parser.y"
{
		set_boolean_option(current_options, OPTION_MULTIPLE_CNAMES,
				   yyvsp[0].num);
	}
break;
case 34:
#line 328 "ns_parser.y"
{
		current_options->check_names[yyvsp[-1].s_int] = yyvsp[0].s_int;
	}
break;
case 35:
#line 332 "ns_parser.y"
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
case 37:
#line 351 "ns_parser.y"
{
		if (current_options->fwdtab) {
			free_forwarders(current_options->fwdtab);
			current_options->fwdtab = NULL;
		}
	}
break;
case 40:
#line 360 "ns_parser.y"
{
		if (current_options->query_acl)
			free_ip_match_list(current_options->query_acl);
		current_options->query_acl = yyvsp[-1].iml;
	}
break;
case 41:
#line 366 "ns_parser.y"
{
		if (current_options->transfer_acl)
			free_ip_match_list(current_options->transfer_acl);
		current_options->transfer_acl = yyvsp[-1].iml;
	}
break;
case 42:
#line 372 "ns_parser.y"
{
		if (current_options->topology)
			free_ip_match_list(current_options->topology);
		current_options->topology = yyvsp[-1].iml;
	}
break;
case 43:
#line 378 "ns_parser.y"
{
		/* To get around the $$ = $1 default rule. */
	}
break;
case 45:
#line 383 "ns_parser.y"
{
		current_options->transfer_format = yyvsp[0].axfr_fmt;
	}
break;
case 46:
#line 387 "ns_parser.y"
{
		current_options->max_transfer_time_in = yyvsp[0].num * 60;
	}
break;
case 47:
#line 391 "ns_parser.y"
{
		current_options->clean_interval = yyvsp[0].num * 60;
	}
break;
case 48:
#line 395 "ns_parser.y"
{
		current_options->interface_interval = yyvsp[0].num * 60;
	}
break;
case 49:
#line 399 "ns_parser.y"
{
		current_options->stats_interval = yyvsp[0].num * 60;
	}
break;
case 51:
#line 406 "ns_parser.y"
{
		yyval.axfr_fmt = axfr_one_answer;
	}
break;
case 52:
#line 410 "ns_parser.y"
{
		yyval.axfr_fmt = axfr_many_answers;
	}
break;
case 53:
#line 415 "ns_parser.y"
{ yyval.ip_addr = yyvsp[0].ip_addr; }
break;
case 54:
#line 416 "ns_parser.y"
{ yyval.ip_addr.s_addr = htonl(INADDR_ANY); }
break;
case 55:
#line 419 "ns_parser.y"
{ yyval.us_int = yyvsp[0].us_int; }
break;
case 56:
#line 420 "ns_parser.y"
{ yyval.us_int = htons(0); }
break;
case 57:
#line 424 "ns_parser.y"
{
		current_options->query_source.sin_addr = yyvsp[0].ip_addr;
	}
break;
case 58:
#line 430 "ns_parser.y"
{
		current_options->query_source.sin_port = yyvsp[0].us_int;
	}
break;
case 63:
#line 441 "ns_parser.y"
{ yyval.us_int = htons(NS_DEFAULTPORT); }
break;
case 64:
#line 442 "ns_parser.y"
{ yyval.us_int = yyvsp[0].us_int; }
break;
case 65:
#line 446 "ns_parser.y"
{ 
		yyval.num = 1;	
	}
break;
case 66:
#line 450 "ns_parser.y"
{ 
		yyval.num = 1;	
	}
break;
case 67:
#line 454 "ns_parser.y"
{ 
		yyval.num = 0;	
	}
break;
case 68:
#line 458 "ns_parser.y"
{ 
		yyval.num = 0;	
	}
break;
case 69:
#line 462 "ns_parser.y"
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
case 70:
#line 474 "ns_parser.y"
{
		yyval.s_int = primary_trans;
	}
break;
case 71:
#line 478 "ns_parser.y"
{
		yyval.s_int = secondary_trans;
	}
break;
case 72:
#line 482 "ns_parser.y"
{
		yyval.s_int = response_trans;
	}
break;
case 73:
#line 488 "ns_parser.y"
{
		yyval.s_int = warn;
	}
break;
case 74:
#line 492 "ns_parser.y"
{
		yyval.s_int = fail;
	}
break;
case 75:
#line 496 "ns_parser.y"
{
		yyval.s_int = ignore;
	}
break;
case 76:
#line 502 "ns_parser.y"
{
		set_boolean_option(current_options, OPTION_FORWARD_ONLY, 1);
	}
break;
case 77:
#line 506 "ns_parser.y"
{
		set_boolean_option(current_options, OPTION_FORWARD_ONLY, 0);
	}
break;
case 78:
#line 510 "ns_parser.y"
{
		parser_warning(0, "forward if-no-answer is unimplemented");
	}
break;
case 79:
#line 514 "ns_parser.y"
{
		parser_warning(0, "forward if-no-domain is unimplemented");
	}
break;
case 80:
#line 520 "ns_parser.y"
{
		current_options->data_size = yyvsp[0].ul_int;
	}
break;
case 81:
#line 524 "ns_parser.y"
{
		current_options->stack_size = yyvsp[0].ul_int;
	}
break;
case 82:
#line 528 "ns_parser.y"
{
		current_options->core_size = yyvsp[0].ul_int;
	}
break;
case 83:
#line 532 "ns_parser.y"
{
		current_options->files = yyvsp[0].ul_int;
	}
break;
case 84:
#line 538 "ns_parser.y"
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
case 85:
#line 551 "ns_parser.y"
{	
		yyval.ul_int = (u_long)yyvsp[0].num;
	}
break;
case 86:
#line 555 "ns_parser.y"
{
		yyval.ul_int = 0;
	}
break;
case 87:
#line 559 "ns_parser.y"
{
		yyval.ul_int = ULONG_MAX;
	}
break;
case 88:
#line 565 "ns_parser.y"
{
		current_options->transfers_in = (u_long) yyvsp[0].num;
	}
break;
case 89:
#line 569 "ns_parser.y"
{
		current_options->transfers_out = (u_long) yyvsp[0].num;
	}
break;
case 90:
#line 573 "ns_parser.y"
{
		current_options->transfers_per_ns = (u_long) yyvsp[0].num;
	}
break;
case 93:
#line 583 "ns_parser.y"
{
		/* nothing */
	}
break;
case 94:
#line 587 "ns_parser.y"
{
		/* nothing */
	}
break;
case 95:
#line 593 "ns_parser.y"
{
	  	add_forwarder(current_options, yyvsp[0].ip_addr);
	}
break;
case 96:
#line 603 "ns_parser.y"
{
		current_logging = begin_logging();
	}
break;
case 97:
#line 607 "ns_parser.y"
{
		end_logging(current_logging, 1);
		current_logging = NULL;
	}
break;
case 101:
#line 619 "ns_parser.y"
{
		current_category = yyvsp[0].s_int;
	}
break;
case 103:
#line 624 "ns_parser.y"
{
		chan_type = log_null;
		chan_flags = 0;
		chan_level = log_info;
	}
break;
case 104:
#line 630 "ns_parser.y"
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
case 105:
#line 671 "ns_parser.y"
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
case 106:
#line 683 "ns_parser.y"
{
		chan_level = log_debug(1);
	}
break;
case 107:
#line 687 "ns_parser.y"
{
		chan_level = yyvsp[0].num;
	}
break;
case 108:
#line 691 "ns_parser.y"
{
		chan_level = 0;
		chan_flags |= LOG_USE_CONTEXT_LEVEL|LOG_REQUIRE_DEBUG;
	}
break;
case 109:
#line 698 "ns_parser.y"
{
		chan_versions = yyvsp[0].num;
		chan_flags |= LOG_TRUNCATE;
	}
break;
case 110:
#line 703 "ns_parser.y"
{
		chan_versions = LOG_MAX_VERSIONS;
		chan_flags |= LOG_TRUNCATE;
	}
break;
case 111:
#line 710 "ns_parser.y"
{
		chan_max_size = yyvsp[0].ul_int;
	}
break;
case 112:
#line 716 "ns_parser.y"
{
		chan_versions = 0;
		chan_max_size = ULONG_MAX;
	}
break;
case 113:
#line 721 "ns_parser.y"
{
		chan_max_size = ULONG_MAX;
	}
break;
case 114:
#line 725 "ns_parser.y"
{
		chan_versions = 0;
	}
break;
case 117:
#line 733 "ns_parser.y"
{
		chan_flags |= LOG_CLOSE_STREAM;
		chan_type = log_file;
		chan_name = yyvsp[-1].cp;
	}
break;
case 118:
#line 741 "ns_parser.y"
{ yyval.cp = yyvsp[0].cp; }
break;
case 119:
#line 742 "ns_parser.y"
{ yyval.cp = savestr("syslog", 1); }
break;
case 120:
#line 745 "ns_parser.y"
{ yyval.s_int = LOG_DAEMON; }
break;
case 121:
#line 747 "ns_parser.y"
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
case 122:
#line 761 "ns_parser.y"
{
		chan_type = log_syslog;
		chan_facility = yyvsp[0].s_int;
	}
break;
case 123:
#line 767 "ns_parser.y"
{ /* nothing to do */ }
break;
case 124:
#line 768 "ns_parser.y"
{ /* nothing to do */ }
break;
case 125:
#line 770 "ns_parser.y"
{
		chan_type = log_null;
	}
break;
case 126:
#line 773 "ns_parser.y"
{ /* nothing to do */ }
break;
case 127:
#line 775 "ns_parser.y"
{
		if (yyvsp[0].num)
			chan_flags |= LOG_TIMESTAMP;
		else
			chan_flags &= ~LOG_TIMESTAMP;
	}
break;
case 128:
#line 782 "ns_parser.y"
{
		if (yyvsp[0].num)
			chan_flags |= LOG_PRINT_CATEGORY;
		else
			chan_flags &= ~LOG_PRINT_CATEGORY;
	}
break;
case 129:
#line 789 "ns_parser.y"
{
		if (yyvsp[0].num)
			chan_flags |= LOG_PRINT_LEVEL;
		else
			chan_flags &= ~LOG_PRINT_LEVEL;
	}
break;
case 134:
#line 803 "ns_parser.y"
{ yyval.cp = savestr("null", 1); }
break;
case 135:
#line 807 "ns_parser.y"
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
case 140:
#line 829 "ns_parser.y"
{ yyval.cp = savestr("default", 1); }
break;
case 141:
#line 830 "ns_parser.y"
{ yyval.cp = savestr("notify", 1); }
break;
case 142:
#line 834 "ns_parser.y"
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
case 143:
#line 853 "ns_parser.y"
{
		char *ip_printable;
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
case 144:
#line 873 "ns_parser.y"
{
		end_server(current_server, !seen_server);
	}
break;
case 147:
#line 883 "ns_parser.y"
{
		set_server_option(current_server, SERVER_INFO_BOGUS, yyvsp[0].num);
	}
break;
case 148:
#line 887 "ns_parser.y"
{
		set_server_transfers(current_server, (int)yyvsp[0].num);
	}
break;
case 149:
#line 891 "ns_parser.y"
{
		set_server_transfer_format(current_server, yyvsp[0].axfr_fmt);
	}
break;
case 152:
#line 903 "ns_parser.y"
{
		ip_match_list iml;
		
		iml = new_ip_match_list();
		if (yyvsp[-1].ime != NULL)
			add_to_ip_match_list(iml, yyvsp[-1].ime);
		yyval.iml = iml;
	}
break;
case 153:
#line 912 "ns_parser.y"
{
		if (yyvsp[-1].ime != NULL)
			add_to_ip_match_list(yyvsp[-2].iml, yyvsp[-1].ime);
		yyval.iml = yyvsp[-2].iml;
	}
break;
case 155:
#line 921 "ns_parser.y"
{
		if (yyvsp[0].ime != NULL)
			ip_match_negate(yyvsp[0].ime);
		yyval.ime = yyvsp[0].ime;
	}
break;
case 156:
#line 929 "ns_parser.y"
{
		yyval.ime = new_ip_match_pattern(yyvsp[0].ip_addr, 32);
	}
break;
case 157:
#line 933 "ns_parser.y"
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
case 158:
#line 945 "ns_parser.y"
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
case 160:
#line 967 "ns_parser.y"
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
case 161:
#line 981 "ns_parser.y"
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
case 162:
#line 999 "ns_parser.y"
{
		key_info ki;

		ki = lookup_key(yyvsp[0].cp);
		if (ki == NULL) {
			parser_error(0, "unknown key '%s'", yyvsp[0].cp);
			yyval.keyi = NULL;
		} else
			yyval.keyi = ki;
		freestr(yyvsp[0].cp);
	}
break;
case 163:
#line 1013 "ns_parser.y"
{
		if (yyvsp[0].keyi == NULL)
			parser_error(0, "empty key not added to server list ");
		else
			add_server_key_info(current_server, yyvsp[0].keyi);
	}
break;
case 167:
#line 1027 "ns_parser.y"
{
		current_algorithm = NULL;
		current_secret = NULL;
	}
break;
case 168:
#line 1032 "ns_parser.y"
{
		key_info ki;

		if (lookup_key(yyvsp[-3].cp) != NULL) {
			parser_error(0, "can't redefine key '%s'", yyvsp[-3].cp);
			freestr(yyvsp[-3].cp);
		} else {
			if (current_algorithm == NULL ||
			    current_secret == NULL)
				parser_error(0, "skipping bad key '%s'", yyvsp[-3].cp);
			else {
				ki = new_key_info(yyvsp[-3].cp, current_algorithm,
						  current_secret);
				define_key(yyvsp[-3].cp, ki);
			}
		}
	}
break;
case 169:
#line 1052 "ns_parser.y"
{
		current_algorithm = yyvsp[-1].cp;
		current_secret = yyvsp[0].cp;
	}
break;
case 170:
#line 1057 "ns_parser.y"
{
		current_algorithm = yyvsp[0].cp;
		current_secret = yyvsp[-1].cp;
	}
break;
case 171:
#line 1062 "ns_parser.y"
{
		current_algorithm = NULL;
		current_secret = NULL;
	}
break;
case 172:
#line 1068 "ns_parser.y"
{ yyval.cp = yyvsp[-1].cp; }
break;
case 173:
#line 1071 "ns_parser.y"
{ yyval.cp = yyvsp[-1].cp; }
break;
case 174:
#line 1079 "ns_parser.y"
{
		if (lookup_acl(yyvsp[-3].cp) != NULL) {
			parser_error(0, "can't redefine ACL '%s'", yyvsp[-3].cp);
			freestr(yyvsp[-3].cp);
		} else
			define_acl(yyvsp[-3].cp, yyvsp[-1].iml);
	}
break;
case 175:
#line 1093 "ns_parser.y"
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
			seen_zone = 1;
			zone_name = savestr("__bad_zone__", 1);
		} else {
			seen_zone = lookup_symbol(symtab, zone_name, sym_type,
						  NULL);
			if (seen_zone) {
				parser_error(0,
					"cannot redefine zone '%s' class %d",
					     zone_name, yyvsp[0].num);
			} else
				define_symbol(symtab, zone_name, sym_type,
					      value, 0);
		}
		freestr(yyvsp[-1].cp);
		current_zone = begin_zone(zone_name, yyvsp[0].num); 
	}
break;
case 176:
#line 1124 "ns_parser.y"
{ end_zone(current_zone, !seen_zone); }
break;
case 179:
#line 1132 "ns_parser.y"
{
		yyval.num = C_IN;
	}
break;
case 180:
#line 1136 "ns_parser.y"
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
case 181:
#line 1150 "ns_parser.y"
{
		yyval.s_int = Z_MASTER;
	}
break;
case 182:
#line 1154 "ns_parser.y"
{
		yyval.s_int = Z_SLAVE;
	}
break;
case 183:
#line 1158 "ns_parser.y"
{
		yyval.s_int = Z_HINT;
	}
break;
case 184:
#line 1162 "ns_parser.y"
{
		yyval.s_int = Z_STUB;
	}
break;
case 187:
#line 1172 "ns_parser.y"
{
		if (!set_zone_type(current_zone, yyvsp[0].s_int))
			parser_warning(0, "zone type already set; skipping");
	}
break;
case 188:
#line 1177 "ns_parser.y"
{
		if (!set_zone_filename(current_zone, yyvsp[0].cp))
			parser_warning(0,
				       "zone filename already set; skipping");
	}
break;
case 190:
#line 1184 "ns_parser.y"
{
		set_zone_transfer_source(current_zone, yyvsp[0].ip_addr);
	}
break;
case 191:
#line 1188 "ns_parser.y"
{
		if (!set_zone_checknames(current_zone, yyvsp[0].s_int))
			parser_warning(0,
	                              "zone checknames already set; skipping");
	}
break;
case 192:
#line 1194 "ns_parser.y"
{
		if (!set_zone_update_acl(current_zone, yyvsp[-1].iml))
			parser_warning(0,
				      "zone update acl already set; skipping");
	}
break;
case 193:
#line 1200 "ns_parser.y"
{
		if (!set_zone_query_acl(current_zone, yyvsp[-1].iml))
			parser_warning(0,
				      "zone query acl already set; skipping");
	}
break;
case 194:
#line 1206 "ns_parser.y"
{
		if (!set_zone_transfer_acl(current_zone, yyvsp[-1].iml))
			parser_warning(0,
				    "zone transfer acl already set; skipping");
	}
break;
case 195:
#line 1212 "ns_parser.y"
{
		if (!set_zone_transfer_time_in(current_zone, yyvsp[0].num*60))
			parser_warning(0,
		       "zone max transfer time (in) already set; skipping");
	}
break;
case 196:
#line 1218 "ns_parser.y"
{
		set_zone_notify(current_zone, yyvsp[0].num);
	}
break;
case 199:
#line 1226 "ns_parser.y"
{
		/* nothing */
	}
break;
case 200:
#line 1230 "ns_parser.y"
{
		/* nothing */
	}
break;
case 201:
#line 1236 "ns_parser.y"
{
	  	add_zone_master(current_zone, yyvsp[0].ip_addr);
	}
break;
case 204:
#line 1246 "ns_parser.y"
{
		/* nothing */
	}
break;
case 205:
#line 1250 "ns_parser.y"
{
		/* nothing */
	}
break;
case 206:
#line 1256 "ns_parser.y"
{
	  	add_zone_notify(current_zone, yyvsp[0].ip_addr);
	}
break;
case 207:
#line 1266 "ns_parser.y"
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
#line 2319 "y.tab.c"
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
