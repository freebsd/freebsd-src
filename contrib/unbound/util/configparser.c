/* original parser id follows */
/* yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93" */
/* (use YYMAJOR/YYMINOR for ifdefs dependent on parser version) */

#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYPATCH 20140409

#define YYEMPTY        (-1)
#define yyclearin      (yychar = YYEMPTY)
#define yyerrok        (yyerrflag = 0)
#define YYRECOVERING() (yyerrflag != 0)
#define YYENOMEM       (-2)
#define YYEOF          0
#define YYPREFIX "yy"

#define YYPURE 0

#line 39 "util/configparser.y"
#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "util/configyyrename.h"
#include "util/config_file.h"
#include "util/net_help.h"

int ub_c_lex(void);
void ub_c_error(const char *message);

/* these need to be global, otherwise they cannot be used inside yacc */
extern struct config_parser_state* cfg_parser;

#if 0
#define OUTYY(s)  printf s /* used ONLY when debugging */
#else
#define OUTYY(s)
#endif

#line 64 "util/configparser.y"
#ifdef YYSTYPE
#undef  YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
#endif
#ifndef YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
typedef union {
	char*	str;
} YYSTYPE;
#endif /* !YYSTYPE_IS_DECLARED */
#line 57 "util/configparser.c"

/* compatibility with bison */
#ifdef YYPARSE_PARAM
/* compatibility with FreeBSD */
# ifdef YYPARSE_PARAM_TYPE
#  define YYPARSE_DECL() yyparse(YYPARSE_PARAM_TYPE YYPARSE_PARAM)
# else
#  define YYPARSE_DECL() yyparse(void *YYPARSE_PARAM)
# endif
#else
# define YYPARSE_DECL() yyparse(void)
#endif

/* Parameters sent to lex. */
#ifdef YYLEX_PARAM
# define YYLEX_DECL() yylex(void *YYLEX_PARAM)
# define YYLEX yylex(YYLEX_PARAM)
#else
# define YYLEX_DECL() yylex(void)
# define YYLEX yylex()
#endif

/* Parameters sent to yyerror. */
#ifndef YYERROR_DECL
#define YYERROR_DECL() yyerror(const char *s)
#endif
#ifndef YYERROR_CALL
#define YYERROR_CALL(msg) yyerror(msg)
#endif

extern int YYPARSE_DECL();

#define SPACE 257
#define LETTER 258
#define NEWLINE 259
#define COMMENT 260
#define COLON 261
#define ANY 262
#define ZONESTR 263
#define STRING_ARG 264
#define VAR_SERVER 265
#define VAR_VERBOSITY 266
#define VAR_NUM_THREADS 267
#define VAR_PORT 268
#define VAR_OUTGOING_RANGE 269
#define VAR_INTERFACE 270
#define VAR_DO_IP4 271
#define VAR_DO_IP6 272
#define VAR_DO_UDP 273
#define VAR_DO_TCP 274
#define VAR_CHROOT 275
#define VAR_USERNAME 276
#define VAR_DIRECTORY 277
#define VAR_LOGFILE 278
#define VAR_PIDFILE 279
#define VAR_MSG_CACHE_SIZE 280
#define VAR_MSG_CACHE_SLABS 281
#define VAR_NUM_QUERIES_PER_THREAD 282
#define VAR_RRSET_CACHE_SIZE 283
#define VAR_RRSET_CACHE_SLABS 284
#define VAR_OUTGOING_NUM_TCP 285
#define VAR_INFRA_HOST_TTL 286
#define VAR_INFRA_LAME_TTL 287
#define VAR_INFRA_CACHE_SLABS 288
#define VAR_INFRA_CACHE_NUMHOSTS 289
#define VAR_INFRA_CACHE_LAME_SIZE 290
#define VAR_NAME 291
#define VAR_STUB_ZONE 292
#define VAR_STUB_HOST 293
#define VAR_STUB_ADDR 294
#define VAR_TARGET_FETCH_POLICY 295
#define VAR_HARDEN_SHORT_BUFSIZE 296
#define VAR_HARDEN_LARGE_QUERIES 297
#define VAR_FORWARD_ZONE 298
#define VAR_FORWARD_HOST 299
#define VAR_FORWARD_ADDR 300
#define VAR_DO_NOT_QUERY_ADDRESS 301
#define VAR_HIDE_IDENTITY 302
#define VAR_HIDE_VERSION 303
#define VAR_IDENTITY 304
#define VAR_VERSION 305
#define VAR_HARDEN_GLUE 306
#define VAR_MODULE_CONF 307
#define VAR_TRUST_ANCHOR_FILE 308
#define VAR_TRUST_ANCHOR 309
#define VAR_VAL_OVERRIDE_DATE 310
#define VAR_BOGUS_TTL 311
#define VAR_VAL_CLEAN_ADDITIONAL 312
#define VAR_VAL_PERMISSIVE_MODE 313
#define VAR_INCOMING_NUM_TCP 314
#define VAR_MSG_BUFFER_SIZE 315
#define VAR_KEY_CACHE_SIZE 316
#define VAR_KEY_CACHE_SLABS 317
#define VAR_TRUSTED_KEYS_FILE 318
#define VAR_VAL_NSEC3_KEYSIZE_ITERATIONS 319
#define VAR_USE_SYSLOG 320
#define VAR_OUTGOING_INTERFACE 321
#define VAR_ROOT_HINTS 322
#define VAR_DO_NOT_QUERY_LOCALHOST 323
#define VAR_CACHE_MAX_TTL 324
#define VAR_HARDEN_DNSSEC_STRIPPED 325
#define VAR_ACCESS_CONTROL 326
#define VAR_LOCAL_ZONE 327
#define VAR_LOCAL_DATA 328
#define VAR_INTERFACE_AUTOMATIC 329
#define VAR_STATISTICS_INTERVAL 330
#define VAR_DO_DAEMONIZE 331
#define VAR_USE_CAPS_FOR_ID 332
#define VAR_STATISTICS_CUMULATIVE 333
#define VAR_OUTGOING_PORT_PERMIT 334
#define VAR_OUTGOING_PORT_AVOID 335
#define VAR_DLV_ANCHOR_FILE 336
#define VAR_DLV_ANCHOR 337
#define VAR_NEG_CACHE_SIZE 338
#define VAR_HARDEN_REFERRAL_PATH 339
#define VAR_PRIVATE_ADDRESS 340
#define VAR_PRIVATE_DOMAIN 341
#define VAR_REMOTE_CONTROL 342
#define VAR_CONTROL_ENABLE 343
#define VAR_CONTROL_INTERFACE 344
#define VAR_CONTROL_PORT 345
#define VAR_SERVER_KEY_FILE 346
#define VAR_SERVER_CERT_FILE 347
#define VAR_CONTROL_KEY_FILE 348
#define VAR_CONTROL_CERT_FILE 349
#define VAR_EXTENDED_STATISTICS 350
#define VAR_LOCAL_DATA_PTR 351
#define VAR_JOSTLE_TIMEOUT 352
#define VAR_STUB_PRIME 353
#define VAR_UNWANTED_REPLY_THRESHOLD 354
#define VAR_LOG_TIME_ASCII 355
#define VAR_DOMAIN_INSECURE 356
#define VAR_PYTHON 357
#define VAR_PYTHON_SCRIPT 358
#define VAR_VAL_SIG_SKEW_MIN 359
#define VAR_VAL_SIG_SKEW_MAX 360
#define VAR_CACHE_MIN_TTL 361
#define VAR_VAL_LOG_LEVEL 362
#define VAR_AUTO_TRUST_ANCHOR_FILE 363
#define VAR_KEEP_MISSING 364
#define VAR_ADD_HOLDDOWN 365
#define VAR_DEL_HOLDDOWN 366
#define VAR_SO_RCVBUF 367
#define VAR_EDNS_BUFFER_SIZE 368
#define VAR_PREFETCH 369
#define VAR_PREFETCH_KEY 370
#define VAR_SO_SNDBUF 371
#define VAR_SO_REUSEPORT 372
#define VAR_HARDEN_BELOW_NXDOMAIN 373
#define VAR_IGNORE_CD_FLAG 374
#define VAR_LOG_QUERIES 375
#define VAR_TCP_UPSTREAM 376
#define VAR_SSL_UPSTREAM 377
#define VAR_SSL_SERVICE_KEY 378
#define VAR_SSL_SERVICE_PEM 379
#define VAR_SSL_PORT 380
#define VAR_FORWARD_FIRST 381
#define VAR_STUB_FIRST 382
#define VAR_MINIMAL_RESPONSES 383
#define VAR_RRSET_ROUNDROBIN 384
#define VAR_MAX_UDP_SIZE 385
#define VAR_DELAY_CLOSE 386
#define VAR_UNBLOCK_LAN_ZONES 387
#define YYERRCODE 256
typedef short YYINT;
static const YYINT yylhs[] = {                           -1,
    0,    0,    1,    1,    1,    1,    1,    2,    3,    3,
   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
   12,   12,    4,    5,    5,  115,  115,  115,  115,  115,
    6,    7,    7,  121,  121,  121,  121,   13,   14,   70,
   73,   82,   15,   21,   61,   16,   74,   75,   32,   54,
   69,   17,   18,   19,   20,  104,  105,  106,  107,  108,
   71,   60,   86,  103,   22,   23,   24,   25,   26,   62,
   76,   77,   92,   48,   58,   49,   87,   42,   43,   44,
   45,   96,  100,  112,   97,   55,   27,   28,   29,   84,
  113,  114,   30,   31,   33,   34,   36,   37,   35,   38,
   39,   40,   46,   65,  101,   79,   72,   80,   81,   98,
   99,   85,   41,   63,   66,   47,   50,   88,   89,   64,
   90,   51,   52,   53,  102,   91,   59,   93,   94,   95,
   56,   57,   78,   67,   68,   83,  109,  110,  111,  116,
  117,  118,  120,  119,  122,  123,  124,  125,   10,   11,
   11,  126,  126,  126,  126,  126,  126,  126,  127,  129,
  128,  130,  131,  132,  133,    8,    9,    9,  134,  135,
};
static const YYINT yylen[] = {                            2,
    0,    2,    2,    2,    2,    2,    2,    1,    2,    0,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    2,    0,    1,    1,    1,    1,    1,
    1,    2,    0,    1,    1,    1,    1,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    3,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    3,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,    2,    2,    2,    2,    1,    2,
    0,    1,    1,    1,    1,    1,    1,    1,    2,    2,
    2,    2,    2,    2,    2,    1,    2,    0,    1,    2,
};
static const YYINT yydefred[] = {                         1,
    0,    8,  113,  121,  239,  256,    2,   10,  115,  123,
  258,  241,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    9,
   11,   12,   13,   14,   15,   16,   17,   18,   19,   20,
   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,
   31,   32,   33,   34,   35,   36,   37,   38,   39,   40,
   41,   42,   43,   44,   45,   46,   47,   48,   49,   50,
   51,   52,   53,   54,   55,   56,   57,   58,   59,   60,
   61,   62,   63,   64,   65,   66,   67,   68,   69,   70,
   71,   72,   73,   74,   75,   76,   77,   78,   79,   80,
   81,   82,   83,   84,   85,   86,   87,   88,   89,   90,
   91,   92,   93,   94,   95,   96,   97,   98,   99,  100,
  101,  102,  103,  104,  105,  106,  107,  108,  109,  110,
  111,  112,    0,    0,    0,    0,    0,  114,  116,  117,
  118,  119,  120,    0,    0,    0,    0,  122,  124,  125,
  126,  127,    0,  257,  259,    0,    0,    0,    0,    0,
    0,    0,  240,  242,  243,  244,  245,  246,  247,  248,
  129,  128,  133,  136,  134,  142,  143,  144,  145,  155,
  156,  157,  158,  159,  177,  178,  179,  183,  184,  139,
  185,  186,  189,  187,  188,  190,  191,  192,  203,  168,
  169,  170,  171,  193,  206,  164,  166,  207,  212,  213,
  214,  140,  176,  221,  222,  165,  217,  152,  135,  160,
  204,  210,  194,    0,    0,  225,  141,  130,  151,  197,
  131,  137,  138,  161,  162,  223,  196,  198,  199,  132,
  226,  180,  202,  153,  167,  208,  209,  211,  216,  163,
  220,  218,  219,  172,  175,  200,  201,  173,  174,  195,
  215,  154,  146,  147,  148,  149,  150,  227,  228,  229,
  181,  182,  230,  231,  232,  234,  233,  235,  236,  237,
  238,  260,  249,  251,  250,  252,  253,  254,  255,  205,
  224,
};
static const YYINT yydgoto[] = {                          1,
    7,    8,   13,    9,   14,   10,   15,   11,   16,   12,
   17,  120,  121,  122,  123,  124,  125,  126,  127,  128,
  129,  130,  131,  132,  133,  134,  135,  136,  137,  138,
  139,  140,  141,  142,  143,  144,  145,  146,  147,  148,
  149,  150,  151,  152,  153,  154,  155,  156,  157,  158,
  159,  160,  161,  162,  163,  164,  165,  166,  167,  168,
  169,  170,  171,  172,  173,  174,  175,  176,  177,  178,
  179,  180,  181,  182,  183,  184,  185,  186,  187,  188,
  189,  190,  191,  192,  193,  194,  195,  196,  197,  198,
  199,  200,  201,  202,  203,  204,  205,  206,  207,  208,
  209,  210,  211,  212,  213,  214,  215,  216,  217,  218,
  219,  220,  221,  222,  228,  229,  230,  231,  232,  233,
  238,  239,  240,  241,  242,  253,  254,  255,  256,  257,
  258,  259,  260,  244,  245,
};
static const YYINT yysindex[] = {                         0,
 -144,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0, -260, -209, -202, -358, -215, -233, -232, -231,
 -230, -226, -225, -224, -181, -178, -177, -176, -171, -142,
 -129, -128, -127, -126, -125, -124, -123, -122, -121, -119,
 -118, -117, -115, -114, -113, -112, -111, -109, -108, -107,
 -106, -105, -104, -103, -102, -101, -100,  -99,  -98,  -97,
  -96,  -95,  -94,  -93,  -92,  -90,  -89,  -88,  -87,  -86,
  -84,  -83,  -82,  -81,  -80,  -79,  -78,  -77,  -76,  -75,
  -74,  -73,  -72,  -71,  -70,  -69,  -68,  -67,  -65,  -64,
  -63,  -62,  -61,  -60,  -59,  -58,  -57,  -56,  -55,  -54,
  -53,  -52,  -50,  -49,  -48,  -47,  -46,  -45,  -44,  -43,
  -42,  -41,  -40,  -39,  -38,  -37,  -36,  -35,  -34,    0,
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
    0,    0,  -33,  -32,  -31,  -30,  -29,    0,    0,    0,
    0,    0,    0,  -28,  -27,  -26,  -25,    0,    0,    0,
    0,    0,  -24,    0,    0,  -23,  -22,  -21,  -20,  -19,
  -18,  -17,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  -16,  -15,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,
};
static const YYINT yyrindex[] = {                         0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    1,    2,    3,    4,    5,    0,    0,    0,
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
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,
};
static const YYINT yygindex[] = {                         0,
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
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,
};
#define YYTABLESIZE 362
static const YYINT yytable[] = {                        243,
    3,    4,    5,    6,    7,   18,   19,   20,   21,   22,
   23,   24,   25,   26,   27,   28,   29,   30,   31,   32,
   33,   34,   35,   36,   37,   38,   39,   40,   41,   42,
  261,  262,  263,  264,   43,   44,   45,  265,  266,  267,
   46,   47,   48,   49,   50,   51,   52,   53,   54,   55,
   56,   57,   58,   59,   60,   61,   62,   63,   64,   65,
   66,   67,   68,   69,   70,   71,   72,   73,   74,   75,
   76,   77,   78,   79,   80,   81,   82,   83,   84,   85,
   86,  223,  268,  224,  225,  269,  270,  271,  234,   87,
   88,   89,  272,   90,   91,   92,  235,  236,   93,   94,
   95,   96,   97,   98,   99,  100,  101,  102,  103,  104,
  105,  106,  107,  108,  109,  110,  111,  112,  113,  114,
    2,  273,  115,  116,  117,  118,  119,  246,  247,  248,
  249,  250,  251,  252,  274,  275,  276,  277,  278,  279,
  280,  281,  282,  226,  283,  284,  285,    3,  286,  287,
  288,  289,  290,    4,  291,  292,  293,  294,  295,  296,
  297,  298,  299,  300,  301,  302,  303,  304,  305,  306,
  307,  308,  227,  309,  310,  311,  312,  313,  237,  314,
  315,  316,  317,  318,  319,  320,  321,  322,  323,  324,
  325,  326,  327,  328,  329,  330,  331,    5,  332,  333,
  334,  335,  336,  337,  338,  339,  340,  341,  342,  343,
  344,  345,    6,  346,  347,  348,  349,  350,  351,  352,
  353,  354,  355,  356,  357,  358,  359,  360,  361,  362,
  363,  364,  365,  366,  367,  368,  369,  370,  371,  372,
  373,  374,  375,  376,  377,  378,  379,  380,  381,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    3,    4,    5,    6,    7,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    3,    4,    5,    6,    7,    0,    3,    4,
    5,    6,    7,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    3,    4,    5,    6,    7,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    3,    4,    5,
    6,    7,
};
static const YYINT yycheck[] = {                        358,
    0,    0,    0,    0,    0,  266,  267,  268,  269,  270,
  271,  272,  273,  274,  275,  276,  277,  278,  279,  280,
  281,  282,  283,  284,  285,  286,  287,  288,  289,  290,
  264,  264,  264,  264,  295,  296,  297,  264,  264,  264,
  301,  302,  303,  304,  305,  306,  307,  308,  309,  310,
  311,  312,  313,  314,  315,  316,  317,  318,  319,  320,
  321,  322,  323,  324,  325,  326,  327,  328,  329,  330,
  331,  332,  333,  334,  335,  336,  337,  338,  339,  340,
  341,  291,  264,  293,  294,  264,  264,  264,  291,  350,
  351,  352,  264,  354,  355,  356,  299,  300,  359,  360,
  361,  362,  363,  364,  365,  366,  367,  368,  369,  370,
  371,  372,  373,  374,  375,  376,  377,  378,  379,  380,
  265,  264,  383,  384,  385,  386,  387,  343,  344,  345,
  346,  347,  348,  349,  264,  264,  264,  264,  264,  264,
  264,  264,  264,  353,  264,  264,  264,  292,  264,  264,
  264,  264,  264,  298,  264,  264,  264,  264,  264,  264,
  264,  264,  264,  264,  264,  264,  264,  264,  264,  264,
  264,  264,  382,  264,  264,  264,  264,  264,  381,  264,
  264,  264,  264,  264,  264,  264,  264,  264,  264,  264,
  264,  264,  264,  264,  264,  264,  264,  342,  264,  264,
  264,  264,  264,  264,  264,  264,  264,  264,  264,  264,
  264,  264,  357,  264,  264,  264,  264,  264,  264,  264,
  264,  264,  264,  264,  264,  264,  264,  264,  264,  264,
  264,  264,  264,  264,  264,  264,  264,  264,  264,  264,
  264,  264,  264,  264,  264,  264,  264,  264,  264,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  265,  265,  265,  265,  265,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  292,  292,  292,  292,  292,   -1,  298,  298,
  298,  298,  298,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  342,  342,  342,  342,  342,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  357,  357,  357,
  357,  357,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 387
#define YYUNDFTOKEN 525
#define YYTRANSLATE(a) ((a) > YYMAXTOKEN ? YYUNDFTOKEN : (a))
#if YYDEBUG
static const char *const yyname[] = {

"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"SPACE","LETTER","NEWLINE",
"COMMENT","COLON","ANY","ZONESTR","STRING_ARG","VAR_SERVER","VAR_VERBOSITY",
"VAR_NUM_THREADS","VAR_PORT","VAR_OUTGOING_RANGE","VAR_INTERFACE","VAR_DO_IP4",
"VAR_DO_IP6","VAR_DO_UDP","VAR_DO_TCP","VAR_CHROOT","VAR_USERNAME",
"VAR_DIRECTORY","VAR_LOGFILE","VAR_PIDFILE","VAR_MSG_CACHE_SIZE",
"VAR_MSG_CACHE_SLABS","VAR_NUM_QUERIES_PER_THREAD","VAR_RRSET_CACHE_SIZE",
"VAR_RRSET_CACHE_SLABS","VAR_OUTGOING_NUM_TCP","VAR_INFRA_HOST_TTL",
"VAR_INFRA_LAME_TTL","VAR_INFRA_CACHE_SLABS","VAR_INFRA_CACHE_NUMHOSTS",
"VAR_INFRA_CACHE_LAME_SIZE","VAR_NAME","VAR_STUB_ZONE","VAR_STUB_HOST",
"VAR_STUB_ADDR","VAR_TARGET_FETCH_POLICY","VAR_HARDEN_SHORT_BUFSIZE",
"VAR_HARDEN_LARGE_QUERIES","VAR_FORWARD_ZONE","VAR_FORWARD_HOST",
"VAR_FORWARD_ADDR","VAR_DO_NOT_QUERY_ADDRESS","VAR_HIDE_IDENTITY",
"VAR_HIDE_VERSION","VAR_IDENTITY","VAR_VERSION","VAR_HARDEN_GLUE",
"VAR_MODULE_CONF","VAR_TRUST_ANCHOR_FILE","VAR_TRUST_ANCHOR",
"VAR_VAL_OVERRIDE_DATE","VAR_BOGUS_TTL","VAR_VAL_CLEAN_ADDITIONAL",
"VAR_VAL_PERMISSIVE_MODE","VAR_INCOMING_NUM_TCP","VAR_MSG_BUFFER_SIZE",
"VAR_KEY_CACHE_SIZE","VAR_KEY_CACHE_SLABS","VAR_TRUSTED_KEYS_FILE",
"VAR_VAL_NSEC3_KEYSIZE_ITERATIONS","VAR_USE_SYSLOG","VAR_OUTGOING_INTERFACE",
"VAR_ROOT_HINTS","VAR_DO_NOT_QUERY_LOCALHOST","VAR_CACHE_MAX_TTL",
"VAR_HARDEN_DNSSEC_STRIPPED","VAR_ACCESS_CONTROL","VAR_LOCAL_ZONE",
"VAR_LOCAL_DATA","VAR_INTERFACE_AUTOMATIC","VAR_STATISTICS_INTERVAL",
"VAR_DO_DAEMONIZE","VAR_USE_CAPS_FOR_ID","VAR_STATISTICS_CUMULATIVE",
"VAR_OUTGOING_PORT_PERMIT","VAR_OUTGOING_PORT_AVOID","VAR_DLV_ANCHOR_FILE",
"VAR_DLV_ANCHOR","VAR_NEG_CACHE_SIZE","VAR_HARDEN_REFERRAL_PATH",
"VAR_PRIVATE_ADDRESS","VAR_PRIVATE_DOMAIN","VAR_REMOTE_CONTROL",
"VAR_CONTROL_ENABLE","VAR_CONTROL_INTERFACE","VAR_CONTROL_PORT",
"VAR_SERVER_KEY_FILE","VAR_SERVER_CERT_FILE","VAR_CONTROL_KEY_FILE",
"VAR_CONTROL_CERT_FILE","VAR_EXTENDED_STATISTICS","VAR_LOCAL_DATA_PTR",
"VAR_JOSTLE_TIMEOUT","VAR_STUB_PRIME","VAR_UNWANTED_REPLY_THRESHOLD",
"VAR_LOG_TIME_ASCII","VAR_DOMAIN_INSECURE","VAR_PYTHON","VAR_PYTHON_SCRIPT",
"VAR_VAL_SIG_SKEW_MIN","VAR_VAL_SIG_SKEW_MAX","VAR_CACHE_MIN_TTL",
"VAR_VAL_LOG_LEVEL","VAR_AUTO_TRUST_ANCHOR_FILE","VAR_KEEP_MISSING",
"VAR_ADD_HOLDDOWN","VAR_DEL_HOLDDOWN","VAR_SO_RCVBUF","VAR_EDNS_BUFFER_SIZE",
"VAR_PREFETCH","VAR_PREFETCH_KEY","VAR_SO_SNDBUF","VAR_SO_REUSEPORT",
"VAR_HARDEN_BELOW_NXDOMAIN","VAR_IGNORE_CD_FLAG","VAR_LOG_QUERIES",
"VAR_TCP_UPSTREAM","VAR_SSL_UPSTREAM","VAR_SSL_SERVICE_KEY",
"VAR_SSL_SERVICE_PEM","VAR_SSL_PORT","VAR_FORWARD_FIRST","VAR_STUB_FIRST",
"VAR_MINIMAL_RESPONSES","VAR_RRSET_ROUNDROBIN","VAR_MAX_UDP_SIZE",
"VAR_DELAY_CLOSE","VAR_UNBLOCK_LAN_ZONES",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
"illegal-symbol",
};
static const char *const yyrule[] = {
"$accept : toplevelvars",
"toplevelvars :",
"toplevelvars : toplevelvars toplevelvar",
"toplevelvar : serverstart contents_server",
"toplevelvar : stubstart contents_stub",
"toplevelvar : forwardstart contents_forward",
"toplevelvar : pythonstart contents_py",
"toplevelvar : rcstart contents_rc",
"serverstart : VAR_SERVER",
"contents_server : contents_server content_server",
"contents_server :",
"content_server : server_num_threads",
"content_server : server_verbosity",
"content_server : server_port",
"content_server : server_outgoing_range",
"content_server : server_do_ip4",
"content_server : server_do_ip6",
"content_server : server_do_udp",
"content_server : server_do_tcp",
"content_server : server_interface",
"content_server : server_chroot",
"content_server : server_username",
"content_server : server_directory",
"content_server : server_logfile",
"content_server : server_pidfile",
"content_server : server_msg_cache_size",
"content_server : server_msg_cache_slabs",
"content_server : server_num_queries_per_thread",
"content_server : server_rrset_cache_size",
"content_server : server_rrset_cache_slabs",
"content_server : server_outgoing_num_tcp",
"content_server : server_infra_host_ttl",
"content_server : server_infra_lame_ttl",
"content_server : server_infra_cache_slabs",
"content_server : server_infra_cache_numhosts",
"content_server : server_infra_cache_lame_size",
"content_server : server_target_fetch_policy",
"content_server : server_harden_short_bufsize",
"content_server : server_harden_large_queries",
"content_server : server_do_not_query_address",
"content_server : server_hide_identity",
"content_server : server_hide_version",
"content_server : server_identity",
"content_server : server_version",
"content_server : server_harden_glue",
"content_server : server_module_conf",
"content_server : server_trust_anchor_file",
"content_server : server_trust_anchor",
"content_server : server_val_override_date",
"content_server : server_bogus_ttl",
"content_server : server_val_clean_additional",
"content_server : server_val_permissive_mode",
"content_server : server_incoming_num_tcp",
"content_server : server_msg_buffer_size",
"content_server : server_key_cache_size",
"content_server : server_key_cache_slabs",
"content_server : server_trusted_keys_file",
"content_server : server_val_nsec3_keysize_iterations",
"content_server : server_use_syslog",
"content_server : server_outgoing_interface",
"content_server : server_root_hints",
"content_server : server_do_not_query_localhost",
"content_server : server_cache_max_ttl",
"content_server : server_harden_dnssec_stripped",
"content_server : server_access_control",
"content_server : server_local_zone",
"content_server : server_local_data",
"content_server : server_interface_automatic",
"content_server : server_statistics_interval",
"content_server : server_do_daemonize",
"content_server : server_use_caps_for_id",
"content_server : server_statistics_cumulative",
"content_server : server_outgoing_port_permit",
"content_server : server_outgoing_port_avoid",
"content_server : server_dlv_anchor_file",
"content_server : server_dlv_anchor",
"content_server : server_neg_cache_size",
"content_server : server_harden_referral_path",
"content_server : server_private_address",
"content_server : server_private_domain",
"content_server : server_extended_statistics",
"content_server : server_local_data_ptr",
"content_server : server_jostle_timeout",
"content_server : server_unwanted_reply_threshold",
"content_server : server_log_time_ascii",
"content_server : server_domain_insecure",
"content_server : server_val_sig_skew_min",
"content_server : server_val_sig_skew_max",
"content_server : server_cache_min_ttl",
"content_server : server_val_log_level",
"content_server : server_auto_trust_anchor_file",
"content_server : server_add_holddown",
"content_server : server_del_holddown",
"content_server : server_keep_missing",
"content_server : server_so_rcvbuf",
"content_server : server_edns_buffer_size",
"content_server : server_prefetch",
"content_server : server_prefetch_key",
"content_server : server_so_sndbuf",
"content_server : server_harden_below_nxdomain",
"content_server : server_ignore_cd_flag",
"content_server : server_log_queries",
"content_server : server_tcp_upstream",
"content_server : server_ssl_upstream",
"content_server : server_ssl_service_key",
"content_server : server_ssl_service_pem",
"content_server : server_ssl_port",
"content_server : server_minimal_responses",
"content_server : server_rrset_roundrobin",
"content_server : server_max_udp_size",
"content_server : server_so_reuseport",
"content_server : server_delay_close",
"content_server : server_unblock_lan_zones",
"stubstart : VAR_STUB_ZONE",
"contents_stub : contents_stub content_stub",
"contents_stub :",
"content_stub : stub_name",
"content_stub : stub_host",
"content_stub : stub_addr",
"content_stub : stub_prime",
"content_stub : stub_first",
"forwardstart : VAR_FORWARD_ZONE",
"contents_forward : contents_forward content_forward",
"contents_forward :",
"content_forward : forward_name",
"content_forward : forward_host",
"content_forward : forward_addr",
"content_forward : forward_first",
"server_num_threads : VAR_NUM_THREADS STRING_ARG",
"server_verbosity : VAR_VERBOSITY STRING_ARG",
"server_statistics_interval : VAR_STATISTICS_INTERVAL STRING_ARG",
"server_statistics_cumulative : VAR_STATISTICS_CUMULATIVE STRING_ARG",
"server_extended_statistics : VAR_EXTENDED_STATISTICS STRING_ARG",
"server_port : VAR_PORT STRING_ARG",
"server_interface : VAR_INTERFACE STRING_ARG",
"server_outgoing_interface : VAR_OUTGOING_INTERFACE STRING_ARG",
"server_outgoing_range : VAR_OUTGOING_RANGE STRING_ARG",
"server_outgoing_port_permit : VAR_OUTGOING_PORT_PERMIT STRING_ARG",
"server_outgoing_port_avoid : VAR_OUTGOING_PORT_AVOID STRING_ARG",
"server_outgoing_num_tcp : VAR_OUTGOING_NUM_TCP STRING_ARG",
"server_incoming_num_tcp : VAR_INCOMING_NUM_TCP STRING_ARG",
"server_interface_automatic : VAR_INTERFACE_AUTOMATIC STRING_ARG",
"server_do_ip4 : VAR_DO_IP4 STRING_ARG",
"server_do_ip6 : VAR_DO_IP6 STRING_ARG",
"server_do_udp : VAR_DO_UDP STRING_ARG",
"server_do_tcp : VAR_DO_TCP STRING_ARG",
"server_tcp_upstream : VAR_TCP_UPSTREAM STRING_ARG",
"server_ssl_upstream : VAR_SSL_UPSTREAM STRING_ARG",
"server_ssl_service_key : VAR_SSL_SERVICE_KEY STRING_ARG",
"server_ssl_service_pem : VAR_SSL_SERVICE_PEM STRING_ARG",
"server_ssl_port : VAR_SSL_PORT STRING_ARG",
"server_do_daemonize : VAR_DO_DAEMONIZE STRING_ARG",
"server_use_syslog : VAR_USE_SYSLOG STRING_ARG",
"server_log_time_ascii : VAR_LOG_TIME_ASCII STRING_ARG",
"server_log_queries : VAR_LOG_QUERIES STRING_ARG",
"server_chroot : VAR_CHROOT STRING_ARG",
"server_username : VAR_USERNAME STRING_ARG",
"server_directory : VAR_DIRECTORY STRING_ARG",
"server_logfile : VAR_LOGFILE STRING_ARG",
"server_pidfile : VAR_PIDFILE STRING_ARG",
"server_root_hints : VAR_ROOT_HINTS STRING_ARG",
"server_dlv_anchor_file : VAR_DLV_ANCHOR_FILE STRING_ARG",
"server_dlv_anchor : VAR_DLV_ANCHOR STRING_ARG",
"server_auto_trust_anchor_file : VAR_AUTO_TRUST_ANCHOR_FILE STRING_ARG",
"server_trust_anchor_file : VAR_TRUST_ANCHOR_FILE STRING_ARG",
"server_trusted_keys_file : VAR_TRUSTED_KEYS_FILE STRING_ARG",
"server_trust_anchor : VAR_TRUST_ANCHOR STRING_ARG",
"server_domain_insecure : VAR_DOMAIN_INSECURE STRING_ARG",
"server_hide_identity : VAR_HIDE_IDENTITY STRING_ARG",
"server_hide_version : VAR_HIDE_VERSION STRING_ARG",
"server_identity : VAR_IDENTITY STRING_ARG",
"server_version : VAR_VERSION STRING_ARG",
"server_so_rcvbuf : VAR_SO_RCVBUF STRING_ARG",
"server_so_sndbuf : VAR_SO_SNDBUF STRING_ARG",
"server_so_reuseport : VAR_SO_REUSEPORT STRING_ARG",
"server_edns_buffer_size : VAR_EDNS_BUFFER_SIZE STRING_ARG",
"server_msg_buffer_size : VAR_MSG_BUFFER_SIZE STRING_ARG",
"server_msg_cache_size : VAR_MSG_CACHE_SIZE STRING_ARG",
"server_msg_cache_slabs : VAR_MSG_CACHE_SLABS STRING_ARG",
"server_num_queries_per_thread : VAR_NUM_QUERIES_PER_THREAD STRING_ARG",
"server_jostle_timeout : VAR_JOSTLE_TIMEOUT STRING_ARG",
"server_delay_close : VAR_DELAY_CLOSE STRING_ARG",
"server_unblock_lan_zones : VAR_UNBLOCK_LAN_ZONES STRING_ARG",
"server_rrset_cache_size : VAR_RRSET_CACHE_SIZE STRING_ARG",
"server_rrset_cache_slabs : VAR_RRSET_CACHE_SLABS STRING_ARG",
"server_infra_host_ttl : VAR_INFRA_HOST_TTL STRING_ARG",
"server_infra_lame_ttl : VAR_INFRA_LAME_TTL STRING_ARG",
"server_infra_cache_numhosts : VAR_INFRA_CACHE_NUMHOSTS STRING_ARG",
"server_infra_cache_lame_size : VAR_INFRA_CACHE_LAME_SIZE STRING_ARG",
"server_infra_cache_slabs : VAR_INFRA_CACHE_SLABS STRING_ARG",
"server_target_fetch_policy : VAR_TARGET_FETCH_POLICY STRING_ARG",
"server_harden_short_bufsize : VAR_HARDEN_SHORT_BUFSIZE STRING_ARG",
"server_harden_large_queries : VAR_HARDEN_LARGE_QUERIES STRING_ARG",
"server_harden_glue : VAR_HARDEN_GLUE STRING_ARG",
"server_harden_dnssec_stripped : VAR_HARDEN_DNSSEC_STRIPPED STRING_ARG",
"server_harden_below_nxdomain : VAR_HARDEN_BELOW_NXDOMAIN STRING_ARG",
"server_harden_referral_path : VAR_HARDEN_REFERRAL_PATH STRING_ARG",
"server_use_caps_for_id : VAR_USE_CAPS_FOR_ID STRING_ARG",
"server_private_address : VAR_PRIVATE_ADDRESS STRING_ARG",
"server_private_domain : VAR_PRIVATE_DOMAIN STRING_ARG",
"server_prefetch : VAR_PREFETCH STRING_ARG",
"server_prefetch_key : VAR_PREFETCH_KEY STRING_ARG",
"server_unwanted_reply_threshold : VAR_UNWANTED_REPLY_THRESHOLD STRING_ARG",
"server_do_not_query_address : VAR_DO_NOT_QUERY_ADDRESS STRING_ARG",
"server_do_not_query_localhost : VAR_DO_NOT_QUERY_LOCALHOST STRING_ARG",
"server_access_control : VAR_ACCESS_CONTROL STRING_ARG STRING_ARG",
"server_module_conf : VAR_MODULE_CONF STRING_ARG",
"server_val_override_date : VAR_VAL_OVERRIDE_DATE STRING_ARG",
"server_val_sig_skew_min : VAR_VAL_SIG_SKEW_MIN STRING_ARG",
"server_val_sig_skew_max : VAR_VAL_SIG_SKEW_MAX STRING_ARG",
"server_cache_max_ttl : VAR_CACHE_MAX_TTL STRING_ARG",
"server_cache_min_ttl : VAR_CACHE_MIN_TTL STRING_ARG",
"server_bogus_ttl : VAR_BOGUS_TTL STRING_ARG",
"server_val_clean_additional : VAR_VAL_CLEAN_ADDITIONAL STRING_ARG",
"server_val_permissive_mode : VAR_VAL_PERMISSIVE_MODE STRING_ARG",
"server_ignore_cd_flag : VAR_IGNORE_CD_FLAG STRING_ARG",
"server_val_log_level : VAR_VAL_LOG_LEVEL STRING_ARG",
"server_val_nsec3_keysize_iterations : VAR_VAL_NSEC3_KEYSIZE_ITERATIONS STRING_ARG",
"server_add_holddown : VAR_ADD_HOLDDOWN STRING_ARG",
"server_del_holddown : VAR_DEL_HOLDDOWN STRING_ARG",
"server_keep_missing : VAR_KEEP_MISSING STRING_ARG",
"server_key_cache_size : VAR_KEY_CACHE_SIZE STRING_ARG",
"server_key_cache_slabs : VAR_KEY_CACHE_SLABS STRING_ARG",
"server_neg_cache_size : VAR_NEG_CACHE_SIZE STRING_ARG",
"server_local_zone : VAR_LOCAL_ZONE STRING_ARG STRING_ARG",
"server_local_data : VAR_LOCAL_DATA STRING_ARG",
"server_local_data_ptr : VAR_LOCAL_DATA_PTR STRING_ARG",
"server_minimal_responses : VAR_MINIMAL_RESPONSES STRING_ARG",
"server_rrset_roundrobin : VAR_RRSET_ROUNDROBIN STRING_ARG",
"server_max_udp_size : VAR_MAX_UDP_SIZE STRING_ARG",
"stub_name : VAR_NAME STRING_ARG",
"stub_host : VAR_STUB_HOST STRING_ARG",
"stub_addr : VAR_STUB_ADDR STRING_ARG",
"stub_first : VAR_STUB_FIRST STRING_ARG",
"stub_prime : VAR_STUB_PRIME STRING_ARG",
"forward_name : VAR_NAME STRING_ARG",
"forward_host : VAR_FORWARD_HOST STRING_ARG",
"forward_addr : VAR_FORWARD_ADDR STRING_ARG",
"forward_first : VAR_FORWARD_FIRST STRING_ARG",
"rcstart : VAR_REMOTE_CONTROL",
"contents_rc : contents_rc content_rc",
"contents_rc :",
"content_rc : rc_control_enable",
"content_rc : rc_control_interface",
"content_rc : rc_control_port",
"content_rc : rc_server_key_file",
"content_rc : rc_server_cert_file",
"content_rc : rc_control_key_file",
"content_rc : rc_control_cert_file",
"rc_control_enable : VAR_CONTROL_ENABLE STRING_ARG",
"rc_control_port : VAR_CONTROL_PORT STRING_ARG",
"rc_control_interface : VAR_CONTROL_INTERFACE STRING_ARG",
"rc_server_key_file : VAR_SERVER_KEY_FILE STRING_ARG",
"rc_server_cert_file : VAR_SERVER_CERT_FILE STRING_ARG",
"rc_control_key_file : VAR_CONTROL_KEY_FILE STRING_ARG",
"rc_control_cert_file : VAR_CONTROL_CERT_FILE STRING_ARG",
"pythonstart : VAR_PYTHON",
"contents_py : contents_py content_py",
"contents_py :",
"content_py : py_script",
"py_script : VAR_PYTHON_SCRIPT STRING_ARG",

};
#endif

int      yydebug;
int      yynerrs;

int      yyerrflag;
int      yychar;
YYSTYPE  yyval;
YYSTYPE  yylval;

/* define the initial stack-sizes */
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH  YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH  10000
#endif
#endif

#define YYINITSTACKSIZE 200

typedef struct {
    unsigned stacksize;
    YYINT    *s_base;
    YYINT    *s_mark;
    YYINT    *s_last;
    YYSTYPE  *l_base;
    YYSTYPE  *l_mark;
} YYSTACKDATA;
/* variables for the parser stack */
static YYSTACKDATA yystack;
#line 1318 "util/configparser.y"

/* parse helper routines could be here */
#line 875 "util/configparser.c"

#if YYDEBUG
#include <stdio.h>		/* needed for printf */
#endif

#include <stdlib.h>	/* needed for malloc, etc */
#include <string.h>	/* needed for memset */

/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(YYSTACKDATA *data)
{
    int i;
    unsigned newsize;
    YYINT *newss;
    YYSTYPE *newvs;

    if ((newsize = data->stacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return YYENOMEM;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;

    i = (int) (data->s_mark - data->s_base);
    newss = (YYINT *)realloc(data->s_base, newsize * sizeof(*newss));
    if (newss == 0)
        return YYENOMEM;

    data->s_base = newss;
    data->s_mark = newss + i;

    newvs = (YYSTYPE *)realloc(data->l_base, newsize * sizeof(*newvs));
    if (newvs == 0)
        return YYENOMEM;

    data->l_base = newvs;
    data->l_mark = newvs + i;

    data->stacksize = newsize;
    data->s_last = data->s_base + newsize - 1;
    return 0;
}

#if YYPURE || defined(YY_NO_LEAKS)
static void yyfreestack(YYSTACKDATA *data)
{
    free(data->s_base);
    free(data->l_base);
    memset(data, 0, sizeof(*data));
}
#else
#define yyfreestack(data) /* nothing */
#endif

#define YYABORT  goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR  goto yyerrlab

int
YYPARSE_DECL()
{
    int yym, yyn, yystate;
#if YYDEBUG
    const char *yys;

    if ((yys = getenv("YYDEBUG")) != 0)
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = YYEMPTY;
    yystate = 0;

#if YYPURE
    memset(&yystack, 0, sizeof(yystack));
#endif

    if (yystack.s_base == NULL && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
    yystack.s_mark = yystack.s_base;
    yystack.l_mark = yystack.l_base;
    yystate = 0;
    *yystack.s_mark = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = YYLEX) < 0) yychar = YYEOF;
#if YYDEBUG
        if (yydebug)
        {
            yys = yyname[YYTRANSLATE(yychar)];
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
        if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM)
        {
            goto yyoverflow;
        }
        yystate = yytable[yyn];
        *++yystack.s_mark = yytable[yyn];
        *++yystack.l_mark = yylval;
        yychar = YYEMPTY;
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

    YYERROR_CALL("syntax error");

    goto yyerrlab;

yyerrlab:
    ++yynerrs;

yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yystack.s_mark]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yystack.s_mark, yytable[yyn]);
#endif
                if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM)
                {
                    goto yyoverflow;
                }
                yystate = yytable[yyn];
                *++yystack.s_mark = yytable[yyn];
                *++yystack.l_mark = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yystack.s_mark);
#endif
                if (yystack.s_mark <= yystack.s_base) goto yyabort;
                --yystack.s_mark;
                --yystack.l_mark;
            }
        }
    }
    else
    {
        if (yychar == YYEOF) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = yyname[YYTRANSLATE(yychar)];
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = YYEMPTY;
        goto yyloop;
    }

yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    if (yym)
        yyval = yystack.l_mark[1-yym];
    else
        memset(&yyval, 0, sizeof yyval);
    switch (yyn)
    {
case 8:
#line 119 "util/configparser.y"
	{ 
		OUTYY(("\nP(server:)\n")); 
	}
break;
case 113:
#line 169 "util/configparser.y"
	{
		struct config_stub* s;
		OUTYY(("\nP(stub_zone:)\n")); 
		s = (struct config_stub*)calloc(1, sizeof(struct config_stub));
		if(s) {
			s->next = cfg_parser->cfg->stubs;
			cfg_parser->cfg->stubs = s;
		} else 
			yyerror("out of memory");
	}
break;
case 121:
#line 185 "util/configparser.y"
	{
		struct config_stub* s;
		OUTYY(("\nP(forward_zone:)\n")); 
		s = (struct config_stub*)calloc(1, sizeof(struct config_stub));
		if(s) {
			s->next = cfg_parser->cfg->forwards;
			cfg_parser->cfg->forwards = s;
		} else 
			yyerror("out of memory");
	}
break;
case 128:
#line 201 "util/configparser.y"
	{ 
		OUTYY(("P(server_num_threads:%s)\n", yystack.l_mark[0].str)); 
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->num_threads = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 129:
#line 210 "util/configparser.y"
	{ 
		OUTYY(("P(server_verbosity:%s)\n", yystack.l_mark[0].str)); 
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->verbosity = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 130:
#line 219 "util/configparser.y"
	{ 
		OUTYY(("P(server_statistics_interval:%s)\n", yystack.l_mark[0].str)); 
		if(strcmp(yystack.l_mark[0].str, "") == 0 || strcmp(yystack.l_mark[0].str, "0") == 0)
			cfg_parser->cfg->stat_interval = 0;
		else if(atoi(yystack.l_mark[0].str) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->stat_interval = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 131:
#line 230 "util/configparser.y"
	{
		OUTYY(("P(server_statistics_cumulative:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_cumulative = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 132:
#line 239 "util/configparser.y"
	{
		OUTYY(("P(server_extended_statistics:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_extended = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 133:
#line 248 "util/configparser.y"
	{
		OUTYY(("P(server_port:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->port = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 134:
#line 257 "util/configparser.y"
	{
		OUTYY(("P(server_interface:%s)\n", yystack.l_mark[0].str));
		if(cfg_parser->cfg->num_ifs == 0)
			cfg_parser->cfg->ifs = calloc(1, sizeof(char*));
		else 	cfg_parser->cfg->ifs = realloc(cfg_parser->cfg->ifs,
				(cfg_parser->cfg->num_ifs+1)*sizeof(char*));
		if(!cfg_parser->cfg->ifs)
			yyerror("out of memory");
		else
			cfg_parser->cfg->ifs[cfg_parser->cfg->num_ifs++] = yystack.l_mark[0].str;
	}
break;
case 135:
#line 270 "util/configparser.y"
	{
		OUTYY(("P(server_outgoing_interface:%s)\n", yystack.l_mark[0].str));
		if(cfg_parser->cfg->num_out_ifs == 0)
			cfg_parser->cfg->out_ifs = calloc(1, sizeof(char*));
		else 	cfg_parser->cfg->out_ifs = realloc(
			cfg_parser->cfg->out_ifs, 
			(cfg_parser->cfg->num_out_ifs+1)*sizeof(char*));
		if(!cfg_parser->cfg->out_ifs)
			yyerror("out of memory");
		else
			cfg_parser->cfg->out_ifs[
				cfg_parser->cfg->num_out_ifs++] = yystack.l_mark[0].str;
	}
break;
case 136:
#line 285 "util/configparser.y"
	{
		OUTYY(("P(server_outgoing_range:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_num_ports = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 137:
#line 294 "util/configparser.y"
	{
		OUTYY(("P(server_outgoing_port_permit:%s)\n", yystack.l_mark[0].str));
		if(!cfg_mark_ports(yystack.l_mark[0].str, 1, 
			cfg_parser->cfg->outgoing_avail_ports, 65536))
			yyerror("port number or range (\"low-high\") expected");
		free(yystack.l_mark[0].str);
	}
break;
case 138:
#line 303 "util/configparser.y"
	{
		OUTYY(("P(server_outgoing_port_avoid:%s)\n", yystack.l_mark[0].str));
		if(!cfg_mark_ports(yystack.l_mark[0].str, 0, 
			cfg_parser->cfg->outgoing_avail_ports, 65536))
			yyerror("port number or range (\"low-high\") expected");
		free(yystack.l_mark[0].str);
	}
break;
case 139:
#line 312 "util/configparser.y"
	{
		OUTYY(("P(server_outgoing_num_tcp:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_num_tcp = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 140:
#line 321 "util/configparser.y"
	{
		OUTYY(("P(server_incoming_num_tcp:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->incoming_num_tcp = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 141:
#line 330 "util/configparser.y"
	{
		OUTYY(("P(server_interface_automatic:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->if_automatic = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 142:
#line 339 "util/configparser.y"
	{
		OUTYY(("P(server_do_ip4:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_ip4 = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 143:
#line 348 "util/configparser.y"
	{
		OUTYY(("P(server_do_ip6:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_ip6 = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 144:
#line 357 "util/configparser.y"
	{
		OUTYY(("P(server_do_udp:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_udp = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 145:
#line 366 "util/configparser.y"
	{
		OUTYY(("P(server_do_tcp:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_tcp = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 146:
#line 375 "util/configparser.y"
	{
		OUTYY(("P(server_tcp_upstream:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tcp_upstream = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 147:
#line 384 "util/configparser.y"
	{
		OUTYY(("P(server_ssl_upstream:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ssl_upstream = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 148:
#line 393 "util/configparser.y"
	{
		OUTYY(("P(server_ssl_service_key:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->ssl_service_key);
		cfg_parser->cfg->ssl_service_key = yystack.l_mark[0].str;
	}
break;
case 149:
#line 400 "util/configparser.y"
	{
		OUTYY(("P(server_ssl_service_pem:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->ssl_service_pem);
		cfg_parser->cfg->ssl_service_pem = yystack.l_mark[0].str;
	}
break;
case 150:
#line 407 "util/configparser.y"
	{
		OUTYY(("P(server_ssl_port:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->ssl_port = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 151:
#line 416 "util/configparser.y"
	{
		OUTYY(("P(server_do_daemonize:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_daemonize = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 152:
#line 425 "util/configparser.y"
	{
		OUTYY(("P(server_use_syslog:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->use_syslog = (strcmp(yystack.l_mark[0].str, "yes")==0);
#if !defined(HAVE_SYSLOG_H) && !defined(UB_ON_WINDOWS)
		if(strcmp(yystack.l_mark[0].str, "yes") == 0)
			yyerror("no syslog services are available. "
				"(reconfigure and compile to add)");
#endif
		free(yystack.l_mark[0].str);
	}
break;
case 153:
#line 439 "util/configparser.y"
	{
		OUTYY(("P(server_log_time_ascii:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_time_ascii = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 154:
#line 448 "util/configparser.y"
	{
		OUTYY(("P(server_log_queries:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_queries = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 155:
#line 457 "util/configparser.y"
	{
		OUTYY(("P(server_chroot:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->chrootdir);
		cfg_parser->cfg->chrootdir = yystack.l_mark[0].str;
	}
break;
case 156:
#line 464 "util/configparser.y"
	{
		OUTYY(("P(server_username:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->username);
		cfg_parser->cfg->username = yystack.l_mark[0].str;
	}
break;
case 157:
#line 471 "util/configparser.y"
	{
		OUTYY(("P(server_directory:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->directory);
		cfg_parser->cfg->directory = yystack.l_mark[0].str;
	}
break;
case 158:
#line 478 "util/configparser.y"
	{
		OUTYY(("P(server_logfile:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->logfile);
		cfg_parser->cfg->logfile = yystack.l_mark[0].str;
		cfg_parser->cfg->use_syslog = 0;
	}
break;
case 159:
#line 486 "util/configparser.y"
	{
		OUTYY(("P(server_pidfile:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->pidfile);
		cfg_parser->cfg->pidfile = yystack.l_mark[0].str;
	}
break;
case 160:
#line 493 "util/configparser.y"
	{
		OUTYY(("P(server_root_hints:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->root_hints, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 161:
#line 500 "util/configparser.y"
	{
		OUTYY(("P(server_dlv_anchor_file:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->dlv_anchor_file);
		cfg_parser->cfg->dlv_anchor_file = yystack.l_mark[0].str;
	}
break;
case 162:
#line 507 "util/configparser.y"
	{
		OUTYY(("P(server_dlv_anchor:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dlv_anchor_list, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 163:
#line 514 "util/configparser.y"
	{
		OUTYY(("P(server_auto_trust_anchor_file:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			auto_trust_anchor_file_list, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 164:
#line 522 "util/configparser.y"
	{
		OUTYY(("P(server_trust_anchor_file:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			trust_anchor_file_list, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 165:
#line 530 "util/configparser.y"
	{
		OUTYY(("P(server_trusted_keys_file:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			trusted_keys_file_list, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 166:
#line 538 "util/configparser.y"
	{
		OUTYY(("P(server_trust_anchor:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->trust_anchor_list, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 167:
#line 545 "util/configparser.y"
	{
		OUTYY(("P(server_domain_insecure:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->domain_insecure, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 168:
#line 552 "util/configparser.y"
	{
		OUTYY(("P(server_hide_identity:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_identity = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 169:
#line 561 "util/configparser.y"
	{
		OUTYY(("P(server_hide_version:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_version = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 170:
#line 570 "util/configparser.y"
	{
		OUTYY(("P(server_identity:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->identity);
		cfg_parser->cfg->identity = yystack.l_mark[0].str;
	}
break;
case 171:
#line 577 "util/configparser.y"
	{
		OUTYY(("P(server_version:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->version);
		cfg_parser->cfg->version = yystack.l_mark[0].str;
	}
break;
case 172:
#line 584 "util/configparser.y"
	{
		OUTYY(("P(server_so_rcvbuf:%s)\n", yystack.l_mark[0].str));
		if(!cfg_parse_memsize(yystack.l_mark[0].str, &cfg_parser->cfg->so_rcvbuf))
			yyerror("buffer size expected");
		free(yystack.l_mark[0].str);
	}
break;
case 173:
#line 592 "util/configparser.y"
	{
		OUTYY(("P(server_so_sndbuf:%s)\n", yystack.l_mark[0].str));
		if(!cfg_parse_memsize(yystack.l_mark[0].str, &cfg_parser->cfg->so_sndbuf))
			yyerror("buffer size expected");
		free(yystack.l_mark[0].str);
	}
break;
case 174:
#line 600 "util/configparser.y"
	{
        OUTYY(("P(server_so_reuseport:%s)\n", yystack.l_mark[0].str));
        if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
            yyerror("expected yes or no.");
        else cfg_parser->cfg->so_reuseport =
            (strcmp(yystack.l_mark[0].str, "yes")==0);
        free(yystack.l_mark[0].str);
    }
break;
case 175:
#line 610 "util/configparser.y"
	{
		OUTYY(("P(server_edns_buffer_size:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0)
			yyerror("number expected");
		else if (atoi(yystack.l_mark[0].str) < 12)
			yyerror("edns buffer size too small");
		else if (atoi(yystack.l_mark[0].str) > 65535)
			cfg_parser->cfg->edns_buffer_size = 65535;
		else cfg_parser->cfg->edns_buffer_size = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 176:
#line 623 "util/configparser.y"
	{
		OUTYY(("P(server_msg_buffer_size:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0)
			yyerror("number expected");
		else if (atoi(yystack.l_mark[0].str) < 4096)
			yyerror("message buffer size too small (use 4096)");
		else cfg_parser->cfg->msg_buffer_size = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 177:
#line 634 "util/configparser.y"
	{
		OUTYY(("P(server_msg_cache_size:%s)\n", yystack.l_mark[0].str));
		if(!cfg_parse_memsize(yystack.l_mark[0].str, &cfg_parser->cfg->msg_cache_size))
			yyerror("memory size expected");
		free(yystack.l_mark[0].str);
	}
break;
case 178:
#line 642 "util/configparser.y"
	{
		OUTYY(("P(server_msg_cache_slabs:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0)
			yyerror("number expected");
		else {
			cfg_parser->cfg->msg_cache_slabs = atoi(yystack.l_mark[0].str);
			if(!is_pow2(cfg_parser->cfg->msg_cache_slabs))
				yyerror("must be a power of 2");
		}
		free(yystack.l_mark[0].str);
	}
break;
case 179:
#line 655 "util/configparser.y"
	{
		OUTYY(("P(server_num_queries_per_thread:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->num_queries_per_thread = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 180:
#line 664 "util/configparser.y"
	{
		OUTYY(("P(server_jostle_timeout:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->jostle_time = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 181:
#line 673 "util/configparser.y"
	{
		OUTYY(("P(server_delay_close:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->delay_close = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 182:
#line 682 "util/configparser.y"
	{
		OUTYY(("P(server_unblock_lan_zones:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->unblock_lan_zones = 
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 183:
#line 692 "util/configparser.y"
	{
		OUTYY(("P(server_rrset_cache_size:%s)\n", yystack.l_mark[0].str));
		if(!cfg_parse_memsize(yystack.l_mark[0].str, &cfg_parser->cfg->rrset_cache_size))
			yyerror("memory size expected");
		free(yystack.l_mark[0].str);
	}
break;
case 184:
#line 700 "util/configparser.y"
	{
		OUTYY(("P(server_rrset_cache_slabs:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0)
			yyerror("number expected");
		else {
			cfg_parser->cfg->rrset_cache_slabs = atoi(yystack.l_mark[0].str);
			if(!is_pow2(cfg_parser->cfg->rrset_cache_slabs))
				yyerror("must be a power of 2");
		}
		free(yystack.l_mark[0].str);
	}
break;
case 185:
#line 713 "util/configparser.y"
	{
		OUTYY(("P(server_infra_host_ttl:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->host_ttl = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 186:
#line 722 "util/configparser.y"
	{
		OUTYY(("P(server_infra_lame_ttl:%s)\n", yystack.l_mark[0].str));
		verbose(VERB_DETAIL, "ignored infra-lame-ttl: %s (option "
			"removed, use infra-host-ttl)", yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 187:
#line 730 "util/configparser.y"
	{
		OUTYY(("P(server_infra_cache_numhosts:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_numhosts = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 188:
#line 739 "util/configparser.y"
	{
		OUTYY(("P(server_infra_cache_lame_size:%s)\n", yystack.l_mark[0].str));
		verbose(VERB_DETAIL, "ignored infra-cache-lame-size: %s "
			"(option removed, use infra-cache-numhosts)", yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 189:
#line 747 "util/configparser.y"
	{
		OUTYY(("P(server_infra_cache_slabs:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0)
			yyerror("number expected");
		else {
			cfg_parser->cfg->infra_cache_slabs = atoi(yystack.l_mark[0].str);
			if(!is_pow2(cfg_parser->cfg->infra_cache_slabs))
				yyerror("must be a power of 2");
		}
		free(yystack.l_mark[0].str);
	}
break;
case 190:
#line 760 "util/configparser.y"
	{
		OUTYY(("P(server_target_fetch_policy:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->target_fetch_policy);
		cfg_parser->cfg->target_fetch_policy = yystack.l_mark[0].str;
	}
break;
case 191:
#line 767 "util/configparser.y"
	{
		OUTYY(("P(server_harden_short_bufsize:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_short_bufsize = 
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 192:
#line 777 "util/configparser.y"
	{
		OUTYY(("P(server_harden_large_queries:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_large_queries = 
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 193:
#line 787 "util/configparser.y"
	{
		OUTYY(("P(server_harden_glue:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_glue = 
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 194:
#line 797 "util/configparser.y"
	{
		OUTYY(("P(server_harden_dnssec_stripped:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_dnssec_stripped = 
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 195:
#line 807 "util/configparser.y"
	{
		OUTYY(("P(server_harden_below_nxdomain:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_below_nxdomain = 
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 196:
#line 817 "util/configparser.y"
	{
		OUTYY(("P(server_harden_referral_path:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_referral_path = 
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 197:
#line 827 "util/configparser.y"
	{
		OUTYY(("P(server_use_caps_for_id:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->use_caps_bits_for_id = 
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 198:
#line 837 "util/configparser.y"
	{
		OUTYY(("P(server_private_address:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->private_address, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 199:
#line 844 "util/configparser.y"
	{
		OUTYY(("P(server_private_domain:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->private_domain, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 200:
#line 851 "util/configparser.y"
	{
		OUTYY(("P(server_prefetch:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefetch = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 201:
#line 860 "util/configparser.y"
	{
		OUTYY(("P(server_prefetch_key:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefetch_key = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 202:
#line 869 "util/configparser.y"
	{
		OUTYY(("P(server_unwanted_reply_threshold:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->unwanted_threshold = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 203:
#line 878 "util/configparser.y"
	{
		OUTYY(("P(server_do_not_query_address:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->donotqueryaddrs, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 204:
#line 885 "util/configparser.y"
	{
		OUTYY(("P(server_do_not_query_localhost:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->donotquery_localhost = 
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 205:
#line 895 "util/configparser.y"
	{
		OUTYY(("P(server_access_control:%s %s)\n", yystack.l_mark[-1].str, yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "deny")!=0 && strcmp(yystack.l_mark[0].str, "refuse")!=0 &&
			strcmp(yystack.l_mark[0].str, "deny_non_local")!=0 &&
			strcmp(yystack.l_mark[0].str, "refuse_non_local")!=0 &&
			strcmp(yystack.l_mark[0].str, "allow")!=0 && 
			strcmp(yystack.l_mark[0].str, "allow_snoop")!=0) {
			yyerror("expected deny, refuse, deny_non_local, "
				"refuse_non_local, allow or allow_snoop "
				"in access control action");
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->acls, yystack.l_mark[-1].str, yystack.l_mark[0].str))
				fatal_exit("out of memory adding acl");
		}
	}
break;
case 206:
#line 912 "util/configparser.y"
	{
		OUTYY(("P(server_module_conf:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->module_conf);
		cfg_parser->cfg->module_conf = yystack.l_mark[0].str;
	}
break;
case 207:
#line 919 "util/configparser.y"
	{
		OUTYY(("P(server_val_override_date:%s)\n", yystack.l_mark[0].str));
		if(strlen(yystack.l_mark[0].str) == 0 || strcmp(yystack.l_mark[0].str, "0") == 0) {
			cfg_parser->cfg->val_date_override = 0;
		} else if(strlen(yystack.l_mark[0].str) == 14) {
			cfg_parser->cfg->val_date_override = 
				cfg_convert_timeval(yystack.l_mark[0].str);
			if(!cfg_parser->cfg->val_date_override)
				yyerror("bad date/time specification");
		} else {
			if(atoi(yystack.l_mark[0].str) == 0)
				yyerror("number expected");
			cfg_parser->cfg->val_date_override = atoi(yystack.l_mark[0].str);
		}
		free(yystack.l_mark[0].str);
	}
break;
case 208:
#line 937 "util/configparser.y"
	{
		OUTYY(("P(server_val_sig_skew_min:%s)\n", yystack.l_mark[0].str));
		if(strlen(yystack.l_mark[0].str) == 0 || strcmp(yystack.l_mark[0].str, "0") == 0) {
			cfg_parser->cfg->val_sig_skew_min = 0;
		} else {
			cfg_parser->cfg->val_sig_skew_min = atoi(yystack.l_mark[0].str);
			if(!cfg_parser->cfg->val_sig_skew_min)
				yyerror("number expected");
		}
		free(yystack.l_mark[0].str);
	}
break;
case 209:
#line 950 "util/configparser.y"
	{
		OUTYY(("P(server_val_sig_skew_max:%s)\n", yystack.l_mark[0].str));
		if(strlen(yystack.l_mark[0].str) == 0 || strcmp(yystack.l_mark[0].str, "0") == 0) {
			cfg_parser->cfg->val_sig_skew_max = 0;
		} else {
			cfg_parser->cfg->val_sig_skew_max = atoi(yystack.l_mark[0].str);
			if(!cfg_parser->cfg->val_sig_skew_max)
				yyerror("number expected");
		}
		free(yystack.l_mark[0].str);
	}
break;
case 210:
#line 963 "util/configparser.y"
	{
		OUTYY(("P(server_cache_max_ttl:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_ttl = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 211:
#line 972 "util/configparser.y"
	{
		OUTYY(("P(server_cache_min_ttl:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->min_ttl = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 212:
#line 981 "util/configparser.y"
	{
		OUTYY(("P(server_bogus_ttl:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->bogus_ttl = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 213:
#line 990 "util/configparser.y"
	{
		OUTYY(("P(server_val_clean_additional:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->val_clean_additional = 
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 214:
#line 1000 "util/configparser.y"
	{
		OUTYY(("P(server_val_permissive_mode:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->val_permissive_mode = 
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 215:
#line 1010 "util/configparser.y"
	{
		OUTYY(("P(server_ignore_cd_flag:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ignore_cd = (strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 216:
#line 1019 "util/configparser.y"
	{
		OUTYY(("P(server_val_log_level:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->val_log_level = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 217:
#line 1028 "util/configparser.y"
	{
		OUTYY(("P(server_val_nsec3_keysize_iterations:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->val_nsec3_key_iterations);
		cfg_parser->cfg->val_nsec3_key_iterations = yystack.l_mark[0].str;
	}
break;
case 218:
#line 1035 "util/configparser.y"
	{
		OUTYY(("P(server_add_holddown:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->add_holddown = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 219:
#line 1044 "util/configparser.y"
	{
		OUTYY(("P(server_del_holddown:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->del_holddown = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 220:
#line 1053 "util/configparser.y"
	{
		OUTYY(("P(server_keep_missing:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0 && strcmp(yystack.l_mark[0].str, "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->keep_missing = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 221:
#line 1062 "util/configparser.y"
	{
		OUTYY(("P(server_key_cache_size:%s)\n", yystack.l_mark[0].str));
		if(!cfg_parse_memsize(yystack.l_mark[0].str, &cfg_parser->cfg->key_cache_size))
			yyerror("memory size expected");
		free(yystack.l_mark[0].str);
	}
break;
case 222:
#line 1070 "util/configparser.y"
	{
		OUTYY(("P(server_key_cache_slabs:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0)
			yyerror("number expected");
		else {
			cfg_parser->cfg->key_cache_slabs = atoi(yystack.l_mark[0].str);
			if(!is_pow2(cfg_parser->cfg->key_cache_slabs))
				yyerror("must be a power of 2");
		}
		free(yystack.l_mark[0].str);
	}
break;
case 223:
#line 1083 "util/configparser.y"
	{
		OUTYY(("P(server_neg_cache_size:%s)\n", yystack.l_mark[0].str));
		if(!cfg_parse_memsize(yystack.l_mark[0].str, &cfg_parser->cfg->neg_cache_size))
			yyerror("memory size expected");
		free(yystack.l_mark[0].str);
	}
break;
case 224:
#line 1091 "util/configparser.y"
	{
		OUTYY(("P(server_local_zone:%s %s)\n", yystack.l_mark[-1].str, yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "static")!=0 && strcmp(yystack.l_mark[0].str, "deny")!=0 &&
		   strcmp(yystack.l_mark[0].str, "refuse")!=0 && strcmp(yystack.l_mark[0].str, "redirect")!=0 &&
		   strcmp(yystack.l_mark[0].str, "transparent")!=0 && strcmp(yystack.l_mark[0].str, "nodefault")!=0
		   && strcmp(yystack.l_mark[0].str, "typetransparent")!=0)
			yyerror("local-zone type: expected static, deny, "
				"refuse, redirect, transparent, "
				"typetransparent or nodefault");
		else if(strcmp(yystack.l_mark[0].str, "nodefault")==0) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->
				local_zones_nodefault, yystack.l_mark[-1].str))
				fatal_exit("out of memory adding local-zone");
			free(yystack.l_mark[0].str);
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->local_zones, 
				yystack.l_mark[-1].str, yystack.l_mark[0].str))
				fatal_exit("out of memory adding local-zone");
		}
	}
break;
case 225:
#line 1113 "util/configparser.y"
	{
		OUTYY(("P(server_local_data:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->local_data, yystack.l_mark[0].str))
			fatal_exit("out of memory adding local-data");
	}
break;
case 226:
#line 1120 "util/configparser.y"
	{
		char* ptr;
		OUTYY(("P(server_local_data_ptr:%s)\n", yystack.l_mark[0].str));
		ptr = cfg_ptr_reverse(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
		if(ptr) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->
				local_data, ptr))
				fatal_exit("out of memory adding local-data");
		} else {
			yyerror("local-data-ptr could not be reversed");
		}
	}
break;
case 227:
#line 1135 "util/configparser.y"
	{
		OUTYY(("P(server_minimal_responses:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->minimal_responses =
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 228:
#line 1145 "util/configparser.y"
	{
		OUTYY(("P(server_rrset_roundrobin:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->rrset_roundrobin =
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 229:
#line 1155 "util/configparser.y"
	{
		OUTYY(("P(server_max_udp_size:%s)\n", yystack.l_mark[0].str));
		cfg_parser->cfg->max_udp_size = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 230:
#line 1162 "util/configparser.y"
	{
		OUTYY(("P(name:%s)\n", yystack.l_mark[0].str));
		if(cfg_parser->cfg->stubs->name)
			yyerror("stub name override, there must be one name "
				"for one stub-zone");
		free(cfg_parser->cfg->stubs->name);
		cfg_parser->cfg->stubs->name = yystack.l_mark[0].str;
	}
break;
case 231:
#line 1172 "util/configparser.y"
	{
		OUTYY(("P(stub-host:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->stubs->hosts, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 232:
#line 1179 "util/configparser.y"
	{
		OUTYY(("P(stub-addr:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->stubs->addrs, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 233:
#line 1186 "util/configparser.y"
	{
		OUTYY(("P(stub-first:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->isfirst=(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 234:
#line 1195 "util/configparser.y"
	{
		OUTYY(("P(stub-prime:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->isprime = 
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 235:
#line 1205 "util/configparser.y"
	{
		OUTYY(("P(name:%s)\n", yystack.l_mark[0].str));
		if(cfg_parser->cfg->forwards->name)
			yyerror("forward name override, there must be one "
				"name for one forward-zone");
		free(cfg_parser->cfg->forwards->name);
		cfg_parser->cfg->forwards->name = yystack.l_mark[0].str;
	}
break;
case 236:
#line 1215 "util/configparser.y"
	{
		OUTYY(("P(forward-host:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->forwards->hosts, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 237:
#line 1222 "util/configparser.y"
	{
		OUTYY(("P(forward-addr:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->forwards->addrs, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 238:
#line 1229 "util/configparser.y"
	{
		OUTYY(("P(forward-first:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->isfirst=(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 239:
#line 1238 "util/configparser.y"
	{ 
		OUTYY(("\nP(remote-control:)\n")); 
	}
break;
case 249:
#line 1249 "util/configparser.y"
	{
		OUTYY(("P(control_enable:%s)\n", yystack.l_mark[0].str));
		if(strcmp(yystack.l_mark[0].str, "yes") != 0 && strcmp(yystack.l_mark[0].str, "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->remote_control_enable = 
			(strcmp(yystack.l_mark[0].str, "yes")==0);
		free(yystack.l_mark[0].str);
	}
break;
case 250:
#line 1259 "util/configparser.y"
	{
		OUTYY(("P(control_port:%s)\n", yystack.l_mark[0].str));
		if(atoi(yystack.l_mark[0].str) == 0)
			yyerror("control port number expected");
		else cfg_parser->cfg->control_port = atoi(yystack.l_mark[0].str);
		free(yystack.l_mark[0].str);
	}
break;
case 251:
#line 1268 "util/configparser.y"
	{
		OUTYY(("P(control_interface:%s)\n", yystack.l_mark[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->control_ifs, yystack.l_mark[0].str))
			yyerror("out of memory");
	}
break;
case 252:
#line 1275 "util/configparser.y"
	{
		OUTYY(("P(rc_server_key_file:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->server_key_file);
		cfg_parser->cfg->server_key_file = yystack.l_mark[0].str;
	}
break;
case 253:
#line 1282 "util/configparser.y"
	{
		OUTYY(("P(rc_server_cert_file:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->server_cert_file);
		cfg_parser->cfg->server_cert_file = yystack.l_mark[0].str;
	}
break;
case 254:
#line 1289 "util/configparser.y"
	{
		OUTYY(("P(rc_control_key_file:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->control_key_file);
		cfg_parser->cfg->control_key_file = yystack.l_mark[0].str;
	}
break;
case 255:
#line 1296 "util/configparser.y"
	{
		OUTYY(("P(rc_control_cert_file:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->control_cert_file);
		cfg_parser->cfg->control_cert_file = yystack.l_mark[0].str;
	}
break;
case 256:
#line 1303 "util/configparser.y"
	{ 
		OUTYY(("\nP(python:)\n")); 
	}
break;
case 260:
#line 1312 "util/configparser.y"
	{
		OUTYY(("P(python-script:%s)\n", yystack.l_mark[0].str));
		free(cfg_parser->cfg->python_script);
		cfg_parser->cfg->python_script = yystack.l_mark[0].str;
	}
break;
#line 2338 "util/configparser.c"
    }
    yystack.s_mark -= yym;
    yystate = *yystack.s_mark;
    yystack.l_mark -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yystack.s_mark = YYFINAL;
        *++yystack.l_mark = yyval;
        if (yychar < 0)
        {
            if ((yychar = YYLEX) < 0) yychar = YYEOF;
#if YYDEBUG
            if (yydebug)
            {
                yys = yyname[YYTRANSLATE(yychar)];
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == YYEOF) goto yyaccept;
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
to state %d\n", YYPREFIX, *yystack.s_mark, yystate);
#endif
    if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM)
    {
        goto yyoverflow;
    }
    *++yystack.s_mark = (YYINT) yystate;
    *++yystack.l_mark = yyval;
    goto yyloop;

yyoverflow:
    YYERROR_CALL("yacc stack overflow");

yyabort:
    yyfreestack(&yystack);
    return (1);

yyaccept:
    yyfreestack(&yystack);
    return (0);
}
