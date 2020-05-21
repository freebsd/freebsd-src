/* A Bison parser, made by GNU Bison 3.4.1.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2019 Free Software Foundation,
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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 38 "util/configparser.y"

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

static void validate_respip_action(const char* action);

/* these need to be global, otherwise they cannot be used inside yacc */
extern struct config_parser_state* cfg_parser;

#if 0
#define OUTYY(s)  printf s /* used ONLY when debugging */
#else
#define OUTYY(s)
#endif


#line 99 "util/configparser.c"

# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
#ifndef YY_YY_UTIL_CONFIGPARSER_H_INCLUDED
# define YY_YY_UTIL_CONFIGPARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    SPACE = 258,
    LETTER = 259,
    NEWLINE = 260,
    COMMENT = 261,
    COLON = 262,
    ANY = 263,
    ZONESTR = 264,
    STRING_ARG = 265,
    VAR_SERVER = 266,
    VAR_VERBOSITY = 267,
    VAR_NUM_THREADS = 268,
    VAR_PORT = 269,
    VAR_OUTGOING_RANGE = 270,
    VAR_INTERFACE = 271,
    VAR_DO_IP4 = 272,
    VAR_DO_IP6 = 273,
    VAR_PREFER_IP6 = 274,
    VAR_DO_UDP = 275,
    VAR_DO_TCP = 276,
    VAR_TCP_MSS = 277,
    VAR_OUTGOING_TCP_MSS = 278,
    VAR_TCP_IDLE_TIMEOUT = 279,
    VAR_EDNS_TCP_KEEPALIVE = 280,
    VAR_EDNS_TCP_KEEPALIVE_TIMEOUT = 281,
    VAR_CHROOT = 282,
    VAR_USERNAME = 283,
    VAR_DIRECTORY = 284,
    VAR_LOGFILE = 285,
    VAR_PIDFILE = 286,
    VAR_MSG_CACHE_SIZE = 287,
    VAR_MSG_CACHE_SLABS = 288,
    VAR_NUM_QUERIES_PER_THREAD = 289,
    VAR_RRSET_CACHE_SIZE = 290,
    VAR_RRSET_CACHE_SLABS = 291,
    VAR_OUTGOING_NUM_TCP = 292,
    VAR_INFRA_HOST_TTL = 293,
    VAR_INFRA_LAME_TTL = 294,
    VAR_INFRA_CACHE_SLABS = 295,
    VAR_INFRA_CACHE_NUMHOSTS = 296,
    VAR_INFRA_CACHE_LAME_SIZE = 297,
    VAR_NAME = 298,
    VAR_STUB_ZONE = 299,
    VAR_STUB_HOST = 300,
    VAR_STUB_ADDR = 301,
    VAR_TARGET_FETCH_POLICY = 302,
    VAR_HARDEN_SHORT_BUFSIZE = 303,
    VAR_HARDEN_LARGE_QUERIES = 304,
    VAR_FORWARD_ZONE = 305,
    VAR_FORWARD_HOST = 306,
    VAR_FORWARD_ADDR = 307,
    VAR_DO_NOT_QUERY_ADDRESS = 308,
    VAR_HIDE_IDENTITY = 309,
    VAR_HIDE_VERSION = 310,
    VAR_IDENTITY = 311,
    VAR_VERSION = 312,
    VAR_HARDEN_GLUE = 313,
    VAR_MODULE_CONF = 314,
    VAR_TRUST_ANCHOR_FILE = 315,
    VAR_TRUST_ANCHOR = 316,
    VAR_VAL_OVERRIDE_DATE = 317,
    VAR_BOGUS_TTL = 318,
    VAR_VAL_CLEAN_ADDITIONAL = 319,
    VAR_VAL_PERMISSIVE_MODE = 320,
    VAR_INCOMING_NUM_TCP = 321,
    VAR_MSG_BUFFER_SIZE = 322,
    VAR_KEY_CACHE_SIZE = 323,
    VAR_KEY_CACHE_SLABS = 324,
    VAR_TRUSTED_KEYS_FILE = 325,
    VAR_VAL_NSEC3_KEYSIZE_ITERATIONS = 326,
    VAR_USE_SYSLOG = 327,
    VAR_OUTGOING_INTERFACE = 328,
    VAR_ROOT_HINTS = 329,
    VAR_DO_NOT_QUERY_LOCALHOST = 330,
    VAR_CACHE_MAX_TTL = 331,
    VAR_HARDEN_DNSSEC_STRIPPED = 332,
    VAR_ACCESS_CONTROL = 333,
    VAR_LOCAL_ZONE = 334,
    VAR_LOCAL_DATA = 335,
    VAR_INTERFACE_AUTOMATIC = 336,
    VAR_STATISTICS_INTERVAL = 337,
    VAR_DO_DAEMONIZE = 338,
    VAR_USE_CAPS_FOR_ID = 339,
    VAR_STATISTICS_CUMULATIVE = 340,
    VAR_OUTGOING_PORT_PERMIT = 341,
    VAR_OUTGOING_PORT_AVOID = 342,
    VAR_DLV_ANCHOR_FILE = 343,
    VAR_DLV_ANCHOR = 344,
    VAR_NEG_CACHE_SIZE = 345,
    VAR_HARDEN_REFERRAL_PATH = 346,
    VAR_PRIVATE_ADDRESS = 347,
    VAR_PRIVATE_DOMAIN = 348,
    VAR_REMOTE_CONTROL = 349,
    VAR_CONTROL_ENABLE = 350,
    VAR_CONTROL_INTERFACE = 351,
    VAR_CONTROL_PORT = 352,
    VAR_SERVER_KEY_FILE = 353,
    VAR_SERVER_CERT_FILE = 354,
    VAR_CONTROL_KEY_FILE = 355,
    VAR_CONTROL_CERT_FILE = 356,
    VAR_CONTROL_USE_CERT = 357,
    VAR_EXTENDED_STATISTICS = 358,
    VAR_LOCAL_DATA_PTR = 359,
    VAR_JOSTLE_TIMEOUT = 360,
    VAR_STUB_PRIME = 361,
    VAR_UNWANTED_REPLY_THRESHOLD = 362,
    VAR_LOG_TIME_ASCII = 363,
    VAR_DOMAIN_INSECURE = 364,
    VAR_PYTHON = 365,
    VAR_PYTHON_SCRIPT = 366,
    VAR_VAL_SIG_SKEW_MIN = 367,
    VAR_VAL_SIG_SKEW_MAX = 368,
    VAR_CACHE_MIN_TTL = 369,
    VAR_VAL_LOG_LEVEL = 370,
    VAR_AUTO_TRUST_ANCHOR_FILE = 371,
    VAR_KEEP_MISSING = 372,
    VAR_ADD_HOLDDOWN = 373,
    VAR_DEL_HOLDDOWN = 374,
    VAR_SO_RCVBUF = 375,
    VAR_EDNS_BUFFER_SIZE = 376,
    VAR_PREFETCH = 377,
    VAR_PREFETCH_KEY = 378,
    VAR_SO_SNDBUF = 379,
    VAR_SO_REUSEPORT = 380,
    VAR_HARDEN_BELOW_NXDOMAIN = 381,
    VAR_IGNORE_CD_FLAG = 382,
    VAR_LOG_QUERIES = 383,
    VAR_LOG_REPLIES = 384,
    VAR_LOG_LOCAL_ACTIONS = 385,
    VAR_TCP_UPSTREAM = 386,
    VAR_SSL_UPSTREAM = 387,
    VAR_SSL_SERVICE_KEY = 388,
    VAR_SSL_SERVICE_PEM = 389,
    VAR_SSL_PORT = 390,
    VAR_FORWARD_FIRST = 391,
    VAR_STUB_SSL_UPSTREAM = 392,
    VAR_FORWARD_SSL_UPSTREAM = 393,
    VAR_TLS_CERT_BUNDLE = 394,
    VAR_STUB_FIRST = 395,
    VAR_MINIMAL_RESPONSES = 396,
    VAR_RRSET_ROUNDROBIN = 397,
    VAR_MAX_UDP_SIZE = 398,
    VAR_DELAY_CLOSE = 399,
    VAR_UNBLOCK_LAN_ZONES = 400,
    VAR_INSECURE_LAN_ZONES = 401,
    VAR_INFRA_CACHE_MIN_RTT = 402,
    VAR_DNS64_PREFIX = 403,
    VAR_DNS64_SYNTHALL = 404,
    VAR_DNS64_IGNORE_AAAA = 405,
    VAR_DNSTAP = 406,
    VAR_DNSTAP_ENABLE = 407,
    VAR_DNSTAP_SOCKET_PATH = 408,
    VAR_DNSTAP_SEND_IDENTITY = 409,
    VAR_DNSTAP_SEND_VERSION = 410,
    VAR_DNSTAP_IDENTITY = 411,
    VAR_DNSTAP_VERSION = 412,
    VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES = 413,
    VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES = 414,
    VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES = 415,
    VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES = 416,
    VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES = 417,
    VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES = 418,
    VAR_RESPONSE_IP_TAG = 419,
    VAR_RESPONSE_IP = 420,
    VAR_RESPONSE_IP_DATA = 421,
    VAR_HARDEN_ALGO_DOWNGRADE = 422,
    VAR_IP_TRANSPARENT = 423,
    VAR_DISABLE_DNSSEC_LAME_CHECK = 424,
    VAR_IP_RATELIMIT = 425,
    VAR_IP_RATELIMIT_SLABS = 426,
    VAR_IP_RATELIMIT_SIZE = 427,
    VAR_RATELIMIT = 428,
    VAR_RATELIMIT_SLABS = 429,
    VAR_RATELIMIT_SIZE = 430,
    VAR_RATELIMIT_FOR_DOMAIN = 431,
    VAR_RATELIMIT_BELOW_DOMAIN = 432,
    VAR_IP_RATELIMIT_FACTOR = 433,
    VAR_RATELIMIT_FACTOR = 434,
    VAR_SEND_CLIENT_SUBNET = 435,
    VAR_CLIENT_SUBNET_ZONE = 436,
    VAR_CLIENT_SUBNET_ALWAYS_FORWARD = 437,
    VAR_CLIENT_SUBNET_OPCODE = 438,
    VAR_MAX_CLIENT_SUBNET_IPV4 = 439,
    VAR_MAX_CLIENT_SUBNET_IPV6 = 440,
    VAR_MIN_CLIENT_SUBNET_IPV4 = 441,
    VAR_MIN_CLIENT_SUBNET_IPV6 = 442,
    VAR_MAX_ECS_TREE_SIZE_IPV4 = 443,
    VAR_MAX_ECS_TREE_SIZE_IPV6 = 444,
    VAR_CAPS_WHITELIST = 445,
    VAR_CACHE_MAX_NEGATIVE_TTL = 446,
    VAR_PERMIT_SMALL_HOLDDOWN = 447,
    VAR_QNAME_MINIMISATION = 448,
    VAR_QNAME_MINIMISATION_STRICT = 449,
    VAR_IP_FREEBIND = 450,
    VAR_DEFINE_TAG = 451,
    VAR_LOCAL_ZONE_TAG = 452,
    VAR_ACCESS_CONTROL_TAG = 453,
    VAR_LOCAL_ZONE_OVERRIDE = 454,
    VAR_ACCESS_CONTROL_TAG_ACTION = 455,
    VAR_ACCESS_CONTROL_TAG_DATA = 456,
    VAR_VIEW = 457,
    VAR_ACCESS_CONTROL_VIEW = 458,
    VAR_VIEW_FIRST = 459,
    VAR_SERVE_EXPIRED = 460,
    VAR_SERVE_EXPIRED_TTL = 461,
    VAR_SERVE_EXPIRED_TTL_RESET = 462,
    VAR_SERVE_EXPIRED_REPLY_TTL = 463,
    VAR_SERVE_EXPIRED_CLIENT_TIMEOUT = 464,
    VAR_FAKE_DSA = 465,
    VAR_FAKE_SHA1 = 466,
    VAR_LOG_IDENTITY = 467,
    VAR_HIDE_TRUSTANCHOR = 468,
    VAR_TRUST_ANCHOR_SIGNALING = 469,
    VAR_AGGRESSIVE_NSEC = 470,
    VAR_USE_SYSTEMD = 471,
    VAR_SHM_ENABLE = 472,
    VAR_SHM_KEY = 473,
    VAR_ROOT_KEY_SENTINEL = 474,
    VAR_DNSCRYPT = 475,
    VAR_DNSCRYPT_ENABLE = 476,
    VAR_DNSCRYPT_PORT = 477,
    VAR_DNSCRYPT_PROVIDER = 478,
    VAR_DNSCRYPT_SECRET_KEY = 479,
    VAR_DNSCRYPT_PROVIDER_CERT = 480,
    VAR_DNSCRYPT_PROVIDER_CERT_ROTATED = 481,
    VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE = 482,
    VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS = 483,
    VAR_DNSCRYPT_NONCE_CACHE_SIZE = 484,
    VAR_DNSCRYPT_NONCE_CACHE_SLABS = 485,
    VAR_IPSECMOD_ENABLED = 486,
    VAR_IPSECMOD_HOOK = 487,
    VAR_IPSECMOD_IGNORE_BOGUS = 488,
    VAR_IPSECMOD_MAX_TTL = 489,
    VAR_IPSECMOD_WHITELIST = 490,
    VAR_IPSECMOD_STRICT = 491,
    VAR_CACHEDB = 492,
    VAR_CACHEDB_BACKEND = 493,
    VAR_CACHEDB_SECRETSEED = 494,
    VAR_CACHEDB_REDISHOST = 495,
    VAR_CACHEDB_REDISPORT = 496,
    VAR_CACHEDB_REDISTIMEOUT = 497,
    VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM = 498,
    VAR_FOR_UPSTREAM = 499,
    VAR_AUTH_ZONE = 500,
    VAR_ZONEFILE = 501,
    VAR_MASTER = 502,
    VAR_URL = 503,
    VAR_FOR_DOWNSTREAM = 504,
    VAR_FALLBACK_ENABLED = 505,
    VAR_TLS_ADDITIONAL_PORT = 506,
    VAR_LOW_RTT = 507,
    VAR_LOW_RTT_PERMIL = 508,
    VAR_FAST_SERVER_PERMIL = 509,
    VAR_FAST_SERVER_NUM = 510,
    VAR_ALLOW_NOTIFY = 511,
    VAR_TLS_WIN_CERT = 512,
    VAR_TCP_CONNECTION_LIMIT = 513,
    VAR_FORWARD_NO_CACHE = 514,
    VAR_STUB_NO_CACHE = 515,
    VAR_LOG_SERVFAIL = 516,
    VAR_DENY_ANY = 517,
    VAR_UNKNOWN_SERVER_TIME_LIMIT = 518,
    VAR_LOG_TAG_QUERYREPLY = 519,
    VAR_STREAM_WAIT_SIZE = 520,
    VAR_TLS_CIPHERS = 521,
    VAR_TLS_CIPHERSUITES = 522,
    VAR_IPSET = 523,
    VAR_IPSET_NAME_V4 = 524,
    VAR_IPSET_NAME_V6 = 525,
    VAR_TLS_SESSION_TICKET_KEYS = 526,
    VAR_RPZ = 527,
    VAR_TAGS = 528,
    VAR_RPZ_ACTION_OVERRIDE = 529,
    VAR_RPZ_CNAME_OVERRIDE = 530,
    VAR_RPZ_LOG = 531,
    VAR_RPZ_LOG_NAME = 532
  };
#endif
/* Tokens.  */
#define SPACE 258
#define LETTER 259
#define NEWLINE 260
#define COMMENT 261
#define COLON 262
#define ANY 263
#define ZONESTR 264
#define STRING_ARG 265
#define VAR_SERVER 266
#define VAR_VERBOSITY 267
#define VAR_NUM_THREADS 268
#define VAR_PORT 269
#define VAR_OUTGOING_RANGE 270
#define VAR_INTERFACE 271
#define VAR_DO_IP4 272
#define VAR_DO_IP6 273
#define VAR_PREFER_IP6 274
#define VAR_DO_UDP 275
#define VAR_DO_TCP 276
#define VAR_TCP_MSS 277
#define VAR_OUTGOING_TCP_MSS 278
#define VAR_TCP_IDLE_TIMEOUT 279
#define VAR_EDNS_TCP_KEEPALIVE 280
#define VAR_EDNS_TCP_KEEPALIVE_TIMEOUT 281
#define VAR_CHROOT 282
#define VAR_USERNAME 283
#define VAR_DIRECTORY 284
#define VAR_LOGFILE 285
#define VAR_PIDFILE 286
#define VAR_MSG_CACHE_SIZE 287
#define VAR_MSG_CACHE_SLABS 288
#define VAR_NUM_QUERIES_PER_THREAD 289
#define VAR_RRSET_CACHE_SIZE 290
#define VAR_RRSET_CACHE_SLABS 291
#define VAR_OUTGOING_NUM_TCP 292
#define VAR_INFRA_HOST_TTL 293
#define VAR_INFRA_LAME_TTL 294
#define VAR_INFRA_CACHE_SLABS 295
#define VAR_INFRA_CACHE_NUMHOSTS 296
#define VAR_INFRA_CACHE_LAME_SIZE 297
#define VAR_NAME 298
#define VAR_STUB_ZONE 299
#define VAR_STUB_HOST 300
#define VAR_STUB_ADDR 301
#define VAR_TARGET_FETCH_POLICY 302
#define VAR_HARDEN_SHORT_BUFSIZE 303
#define VAR_HARDEN_LARGE_QUERIES 304
#define VAR_FORWARD_ZONE 305
#define VAR_FORWARD_HOST 306
#define VAR_FORWARD_ADDR 307
#define VAR_DO_NOT_QUERY_ADDRESS 308
#define VAR_HIDE_IDENTITY 309
#define VAR_HIDE_VERSION 310
#define VAR_IDENTITY 311
#define VAR_VERSION 312
#define VAR_HARDEN_GLUE 313
#define VAR_MODULE_CONF 314
#define VAR_TRUST_ANCHOR_FILE 315
#define VAR_TRUST_ANCHOR 316
#define VAR_VAL_OVERRIDE_DATE 317
#define VAR_BOGUS_TTL 318
#define VAR_VAL_CLEAN_ADDITIONAL 319
#define VAR_VAL_PERMISSIVE_MODE 320
#define VAR_INCOMING_NUM_TCP 321
#define VAR_MSG_BUFFER_SIZE 322
#define VAR_KEY_CACHE_SIZE 323
#define VAR_KEY_CACHE_SLABS 324
#define VAR_TRUSTED_KEYS_FILE 325
#define VAR_VAL_NSEC3_KEYSIZE_ITERATIONS 326
#define VAR_USE_SYSLOG 327
#define VAR_OUTGOING_INTERFACE 328
#define VAR_ROOT_HINTS 329
#define VAR_DO_NOT_QUERY_LOCALHOST 330
#define VAR_CACHE_MAX_TTL 331
#define VAR_HARDEN_DNSSEC_STRIPPED 332
#define VAR_ACCESS_CONTROL 333
#define VAR_LOCAL_ZONE 334
#define VAR_LOCAL_DATA 335
#define VAR_INTERFACE_AUTOMATIC 336
#define VAR_STATISTICS_INTERVAL 337
#define VAR_DO_DAEMONIZE 338
#define VAR_USE_CAPS_FOR_ID 339
#define VAR_STATISTICS_CUMULATIVE 340
#define VAR_OUTGOING_PORT_PERMIT 341
#define VAR_OUTGOING_PORT_AVOID 342
#define VAR_DLV_ANCHOR_FILE 343
#define VAR_DLV_ANCHOR 344
#define VAR_NEG_CACHE_SIZE 345
#define VAR_HARDEN_REFERRAL_PATH 346
#define VAR_PRIVATE_ADDRESS 347
#define VAR_PRIVATE_DOMAIN 348
#define VAR_REMOTE_CONTROL 349
#define VAR_CONTROL_ENABLE 350
#define VAR_CONTROL_INTERFACE 351
#define VAR_CONTROL_PORT 352
#define VAR_SERVER_KEY_FILE 353
#define VAR_SERVER_CERT_FILE 354
#define VAR_CONTROL_KEY_FILE 355
#define VAR_CONTROL_CERT_FILE 356
#define VAR_CONTROL_USE_CERT 357
#define VAR_EXTENDED_STATISTICS 358
#define VAR_LOCAL_DATA_PTR 359
#define VAR_JOSTLE_TIMEOUT 360
#define VAR_STUB_PRIME 361
#define VAR_UNWANTED_REPLY_THRESHOLD 362
#define VAR_LOG_TIME_ASCII 363
#define VAR_DOMAIN_INSECURE 364
#define VAR_PYTHON 365
#define VAR_PYTHON_SCRIPT 366
#define VAR_VAL_SIG_SKEW_MIN 367
#define VAR_VAL_SIG_SKEW_MAX 368
#define VAR_CACHE_MIN_TTL 369
#define VAR_VAL_LOG_LEVEL 370
#define VAR_AUTO_TRUST_ANCHOR_FILE 371
#define VAR_KEEP_MISSING 372
#define VAR_ADD_HOLDDOWN 373
#define VAR_DEL_HOLDDOWN 374
#define VAR_SO_RCVBUF 375
#define VAR_EDNS_BUFFER_SIZE 376
#define VAR_PREFETCH 377
#define VAR_PREFETCH_KEY 378
#define VAR_SO_SNDBUF 379
#define VAR_SO_REUSEPORT 380
#define VAR_HARDEN_BELOW_NXDOMAIN 381
#define VAR_IGNORE_CD_FLAG 382
#define VAR_LOG_QUERIES 383
#define VAR_LOG_REPLIES 384
#define VAR_LOG_LOCAL_ACTIONS 385
#define VAR_TCP_UPSTREAM 386
#define VAR_SSL_UPSTREAM 387
#define VAR_SSL_SERVICE_KEY 388
#define VAR_SSL_SERVICE_PEM 389
#define VAR_SSL_PORT 390
#define VAR_FORWARD_FIRST 391
#define VAR_STUB_SSL_UPSTREAM 392
#define VAR_FORWARD_SSL_UPSTREAM 393
#define VAR_TLS_CERT_BUNDLE 394
#define VAR_STUB_FIRST 395
#define VAR_MINIMAL_RESPONSES 396
#define VAR_RRSET_ROUNDROBIN 397
#define VAR_MAX_UDP_SIZE 398
#define VAR_DELAY_CLOSE 399
#define VAR_UNBLOCK_LAN_ZONES 400
#define VAR_INSECURE_LAN_ZONES 401
#define VAR_INFRA_CACHE_MIN_RTT 402
#define VAR_DNS64_PREFIX 403
#define VAR_DNS64_SYNTHALL 404
#define VAR_DNS64_IGNORE_AAAA 405
#define VAR_DNSTAP 406
#define VAR_DNSTAP_ENABLE 407
#define VAR_DNSTAP_SOCKET_PATH 408
#define VAR_DNSTAP_SEND_IDENTITY 409
#define VAR_DNSTAP_SEND_VERSION 410
#define VAR_DNSTAP_IDENTITY 411
#define VAR_DNSTAP_VERSION 412
#define VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES 413
#define VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES 414
#define VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES 415
#define VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES 416
#define VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES 417
#define VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES 418
#define VAR_RESPONSE_IP_TAG 419
#define VAR_RESPONSE_IP 420
#define VAR_RESPONSE_IP_DATA 421
#define VAR_HARDEN_ALGO_DOWNGRADE 422
#define VAR_IP_TRANSPARENT 423
#define VAR_DISABLE_DNSSEC_LAME_CHECK 424
#define VAR_IP_RATELIMIT 425
#define VAR_IP_RATELIMIT_SLABS 426
#define VAR_IP_RATELIMIT_SIZE 427
#define VAR_RATELIMIT 428
#define VAR_RATELIMIT_SLABS 429
#define VAR_RATELIMIT_SIZE 430
#define VAR_RATELIMIT_FOR_DOMAIN 431
#define VAR_RATELIMIT_BELOW_DOMAIN 432
#define VAR_IP_RATELIMIT_FACTOR 433
#define VAR_RATELIMIT_FACTOR 434
#define VAR_SEND_CLIENT_SUBNET 435
#define VAR_CLIENT_SUBNET_ZONE 436
#define VAR_CLIENT_SUBNET_ALWAYS_FORWARD 437
#define VAR_CLIENT_SUBNET_OPCODE 438
#define VAR_MAX_CLIENT_SUBNET_IPV4 439
#define VAR_MAX_CLIENT_SUBNET_IPV6 440
#define VAR_MIN_CLIENT_SUBNET_IPV4 441
#define VAR_MIN_CLIENT_SUBNET_IPV6 442
#define VAR_MAX_ECS_TREE_SIZE_IPV4 443
#define VAR_MAX_ECS_TREE_SIZE_IPV6 444
#define VAR_CAPS_WHITELIST 445
#define VAR_CACHE_MAX_NEGATIVE_TTL 446
#define VAR_PERMIT_SMALL_HOLDDOWN 447
#define VAR_QNAME_MINIMISATION 448
#define VAR_QNAME_MINIMISATION_STRICT 449
#define VAR_IP_FREEBIND 450
#define VAR_DEFINE_TAG 451
#define VAR_LOCAL_ZONE_TAG 452
#define VAR_ACCESS_CONTROL_TAG 453
#define VAR_LOCAL_ZONE_OVERRIDE 454
#define VAR_ACCESS_CONTROL_TAG_ACTION 455
#define VAR_ACCESS_CONTROL_TAG_DATA 456
#define VAR_VIEW 457
#define VAR_ACCESS_CONTROL_VIEW 458
#define VAR_VIEW_FIRST 459
#define VAR_SERVE_EXPIRED 460
#define VAR_SERVE_EXPIRED_TTL 461
#define VAR_SERVE_EXPIRED_TTL_RESET 462
#define VAR_SERVE_EXPIRED_REPLY_TTL 463
#define VAR_SERVE_EXPIRED_CLIENT_TIMEOUT 464
#define VAR_FAKE_DSA 465
#define VAR_FAKE_SHA1 466
#define VAR_LOG_IDENTITY 467
#define VAR_HIDE_TRUSTANCHOR 468
#define VAR_TRUST_ANCHOR_SIGNALING 469
#define VAR_AGGRESSIVE_NSEC 470
#define VAR_USE_SYSTEMD 471
#define VAR_SHM_ENABLE 472
#define VAR_SHM_KEY 473
#define VAR_ROOT_KEY_SENTINEL 474
#define VAR_DNSCRYPT 475
#define VAR_DNSCRYPT_ENABLE 476
#define VAR_DNSCRYPT_PORT 477
#define VAR_DNSCRYPT_PROVIDER 478
#define VAR_DNSCRYPT_SECRET_KEY 479
#define VAR_DNSCRYPT_PROVIDER_CERT 480
#define VAR_DNSCRYPT_PROVIDER_CERT_ROTATED 481
#define VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE 482
#define VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS 483
#define VAR_DNSCRYPT_NONCE_CACHE_SIZE 484
#define VAR_DNSCRYPT_NONCE_CACHE_SLABS 485
#define VAR_IPSECMOD_ENABLED 486
#define VAR_IPSECMOD_HOOK 487
#define VAR_IPSECMOD_IGNORE_BOGUS 488
#define VAR_IPSECMOD_MAX_TTL 489
#define VAR_IPSECMOD_WHITELIST 490
#define VAR_IPSECMOD_STRICT 491
#define VAR_CACHEDB 492
#define VAR_CACHEDB_BACKEND 493
#define VAR_CACHEDB_SECRETSEED 494
#define VAR_CACHEDB_REDISHOST 495
#define VAR_CACHEDB_REDISPORT 496
#define VAR_CACHEDB_REDISTIMEOUT 497
#define VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM 498
#define VAR_FOR_UPSTREAM 499
#define VAR_AUTH_ZONE 500
#define VAR_ZONEFILE 501
#define VAR_MASTER 502
#define VAR_URL 503
#define VAR_FOR_DOWNSTREAM 504
#define VAR_FALLBACK_ENABLED 505
#define VAR_TLS_ADDITIONAL_PORT 506
#define VAR_LOW_RTT 507
#define VAR_LOW_RTT_PERMIL 508
#define VAR_FAST_SERVER_PERMIL 509
#define VAR_FAST_SERVER_NUM 510
#define VAR_ALLOW_NOTIFY 511
#define VAR_TLS_WIN_CERT 512
#define VAR_TCP_CONNECTION_LIMIT 513
#define VAR_FORWARD_NO_CACHE 514
#define VAR_STUB_NO_CACHE 515
#define VAR_LOG_SERVFAIL 516
#define VAR_DENY_ANY 517
#define VAR_UNKNOWN_SERVER_TIME_LIMIT 518
#define VAR_LOG_TAG_QUERYREPLY 519
#define VAR_STREAM_WAIT_SIZE 520
#define VAR_TLS_CIPHERS 521
#define VAR_TLS_CIPHERSUITES 522
#define VAR_IPSET 523
#define VAR_IPSET_NAME_V4 524
#define VAR_IPSET_NAME_V6 525
#define VAR_TLS_SESSION_TICKET_KEYS 526
#define VAR_RPZ 527
#define VAR_TAGS 528
#define VAR_RPZ_ACTION_OVERRIDE 529
#define VAR_RPZ_CNAME_OVERRIDE 530
#define VAR_RPZ_LOG 531
#define VAR_RPZ_LOG_NAME 532

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 66 "util/configparser.y"

	char*	str;

#line 700 "util/configparser.c"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_UTIL_CONFIGPARSER_H_INCLUDED  */



#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   590

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  278
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  301
/* YYNRULES -- Number of rules.  */
#define YYNRULES  580
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  863

#define YYUNDEFTOK  2
#define YYMAXUTOK   532

/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                                \
  ((unsigned) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_uint16 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   175,   175,   175,   176,   176,   177,   177,   178,   178,
     178,   179,   179,   180,   180,   181,   185,   190,   191,   192,
     192,   192,   193,   193,   194,   194,   195,   195,   196,   196,
     196,   197,   197,   198,   198,   198,   199,   199,   199,   200,
     200,   201,   201,   202,   202,   203,   203,   204,   204,   205,
     205,   206,   206,   207,   207,   208,   208,   208,   209,   209,
     209,   210,   210,   210,   211,   211,   212,   212,   213,   213,
     214,   214,   215,   215,   215,   216,   216,   217,   217,   218,
     218,   218,   219,   219,   220,   220,   221,   221,   222,   222,
     222,   223,   223,   224,   224,   225,   225,   226,   226,   227,
     227,   228,   228,   228,   229,   229,   230,   230,   230,   231,
     231,   231,   232,   232,   232,   233,   233,   233,   233,   234,
     235,   235,   235,   236,   236,   236,   237,   237,   238,   238,
     239,   239,   239,   240,   240,   241,   241,   241,   242,   242,
     243,   243,   244,   245,   245,   246,   246,   247,   247,   248,
     249,   249,   250,   250,   251,   251,   252,   252,   253,   253,
     254,   254,   254,   255,   255,   256,   256,   257,   257,   258,
     258,   259,   259,   260,   260,   261,   261,   261,   262,   262,
     262,   263,   263,   263,   264,   264,   265,   266,   266,   267,
     267,   268,   268,   269,   269,   270,   270,   270,   271,   271,
     271,   272,   272,   272,   273,   273,   274,   274,   275,   275,
     277,   289,   290,   291,   291,   291,   291,   291,   292,   292,
     294,   306,   307,   308,   308,   308,   308,   309,   309,   311,
     325,   326,   327,   327,   327,   327,   328,   328,   328,   330,
     347,   348,   349,   349,   349,   349,   350,   350,   350,   351,
     354,   373,   390,   398,   408,   416,   433,   434,   435,   435,
     435,   435,   435,   436,   436,   436,   437,   437,   439,   448,
     457,   468,   477,   486,   495,   506,   515,   527,   541,   556,
     567,   584,   601,   618,   635,   650,   665,   678,   693,   702,
     711,   720,   729,   738,   747,   756,   765,   774,   783,   792,
     801,   810,   823,   832,   845,   854,   863,   872,   879,   886,
     895,   902,   911,   919,   926,   933,   941,   950,   959,   973,
     982,   991,  1000,  1009,  1018,  1027,  1034,  1041,  1067,  1075,
    1082,  1089,  1096,  1103,  1111,  1119,  1127,  1134,  1145,  1156,
    1163,  1172,  1181,  1190,  1197,  1204,  1212,  1220,  1230,  1240,
    1250,  1258,  1271,  1282,  1290,  1303,  1312,  1321,  1330,  1340,
    1350,  1358,  1371,  1380,  1388,  1397,  1405,  1418,  1427,  1434,
    1444,  1454,  1464,  1474,  1484,  1494,  1504,  1514,  1521,  1528,
    1535,  1544,  1553,  1562,  1571,  1578,  1588,  1608,  1615,  1633,
    1646,  1659,  1668,  1677,  1686,  1695,  1705,  1715,  1726,  1735,
    1744,  1753,  1762,  1771,  1780,  1793,  1806,  1815,  1822,  1831,
    1840,  1849,  1858,  1866,  1879,  1887,  1928,  1935,  1950,  1960,
    1970,  1977,  1984,  1991,  2000,  2008,  2022,  2043,  2064,  2076,
    2088,  2100,  2109,  2130,  2140,  2149,  2157,  2165,  2178,  2191,
    2206,  2221,  2230,  2239,  2245,  2254,  2263,  2273,  2283,  2296,
    2309,  2321,  2335,  2347,  2361,  2371,  2378,  2385,  2394,  2403,
    2413,  2423,  2433,  2440,  2447,  2456,  2465,  2475,  2485,  2492,
    2499,  2506,  2514,  2524,  2534,  2544,  2554,  2593,  2603,  2611,
    2619,  2634,  2643,  2648,  2649,  2650,  2650,  2650,  2651,  2651,
    2651,  2652,  2652,  2654,  2664,  2673,  2680,  2687,  2694,  2701,
    2708,  2715,  2720,  2721,  2722,  2722,  2723,  2723,  2724,  2724,
    2725,  2726,  2727,  2728,  2729,  2730,  2732,  2741,  2748,  2757,
    2766,  2773,  2780,  2790,  2800,  2810,  2820,  2830,  2840,  2845,
    2846,  2847,  2849,  2855,  2865,  2872,  2881,  2889,  2894,  2895,
    2897,  2897,  2897,  2898,  2898,  2899,  2900,  2901,  2902,  2903,
    2905,  2915,  2924,  2931,  2940,  2947,  2956,  2964,  2977,  2985,
    2998,  3003,  3004,  3005,  3005,  3006,  3006,  3006,  3008,  3020,
    3032,  3044,  3059,  3072,  3083,  3088,  3089,  3090,  3090,  3092,
    3107
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "SPACE", "LETTER", "NEWLINE", "COMMENT",
  "COLON", "ANY", "ZONESTR", "STRING_ARG", "VAR_SERVER", "VAR_VERBOSITY",
  "VAR_NUM_THREADS", "VAR_PORT", "VAR_OUTGOING_RANGE", "VAR_INTERFACE",
  "VAR_DO_IP4", "VAR_DO_IP6", "VAR_PREFER_IP6", "VAR_DO_UDP", "VAR_DO_TCP",
  "VAR_TCP_MSS", "VAR_OUTGOING_TCP_MSS", "VAR_TCP_IDLE_TIMEOUT",
  "VAR_EDNS_TCP_KEEPALIVE", "VAR_EDNS_TCP_KEEPALIVE_TIMEOUT", "VAR_CHROOT",
  "VAR_USERNAME", "VAR_DIRECTORY", "VAR_LOGFILE", "VAR_PIDFILE",
  "VAR_MSG_CACHE_SIZE", "VAR_MSG_CACHE_SLABS",
  "VAR_NUM_QUERIES_PER_THREAD", "VAR_RRSET_CACHE_SIZE",
  "VAR_RRSET_CACHE_SLABS", "VAR_OUTGOING_NUM_TCP", "VAR_INFRA_HOST_TTL",
  "VAR_INFRA_LAME_TTL", "VAR_INFRA_CACHE_SLABS",
  "VAR_INFRA_CACHE_NUMHOSTS", "VAR_INFRA_CACHE_LAME_SIZE", "VAR_NAME",
  "VAR_STUB_ZONE", "VAR_STUB_HOST", "VAR_STUB_ADDR",
  "VAR_TARGET_FETCH_POLICY", "VAR_HARDEN_SHORT_BUFSIZE",
  "VAR_HARDEN_LARGE_QUERIES", "VAR_FORWARD_ZONE", "VAR_FORWARD_HOST",
  "VAR_FORWARD_ADDR", "VAR_DO_NOT_QUERY_ADDRESS", "VAR_HIDE_IDENTITY",
  "VAR_HIDE_VERSION", "VAR_IDENTITY", "VAR_VERSION", "VAR_HARDEN_GLUE",
  "VAR_MODULE_CONF", "VAR_TRUST_ANCHOR_FILE", "VAR_TRUST_ANCHOR",
  "VAR_VAL_OVERRIDE_DATE", "VAR_BOGUS_TTL", "VAR_VAL_CLEAN_ADDITIONAL",
  "VAR_VAL_PERMISSIVE_MODE", "VAR_INCOMING_NUM_TCP", "VAR_MSG_BUFFER_SIZE",
  "VAR_KEY_CACHE_SIZE", "VAR_KEY_CACHE_SLABS", "VAR_TRUSTED_KEYS_FILE",
  "VAR_VAL_NSEC3_KEYSIZE_ITERATIONS", "VAR_USE_SYSLOG",
  "VAR_OUTGOING_INTERFACE", "VAR_ROOT_HINTS", "VAR_DO_NOT_QUERY_LOCALHOST",
  "VAR_CACHE_MAX_TTL", "VAR_HARDEN_DNSSEC_STRIPPED", "VAR_ACCESS_CONTROL",
  "VAR_LOCAL_ZONE", "VAR_LOCAL_DATA", "VAR_INTERFACE_AUTOMATIC",
  "VAR_STATISTICS_INTERVAL", "VAR_DO_DAEMONIZE", "VAR_USE_CAPS_FOR_ID",
  "VAR_STATISTICS_CUMULATIVE", "VAR_OUTGOING_PORT_PERMIT",
  "VAR_OUTGOING_PORT_AVOID", "VAR_DLV_ANCHOR_FILE", "VAR_DLV_ANCHOR",
  "VAR_NEG_CACHE_SIZE", "VAR_HARDEN_REFERRAL_PATH", "VAR_PRIVATE_ADDRESS",
  "VAR_PRIVATE_DOMAIN", "VAR_REMOTE_CONTROL", "VAR_CONTROL_ENABLE",
  "VAR_CONTROL_INTERFACE", "VAR_CONTROL_PORT", "VAR_SERVER_KEY_FILE",
  "VAR_SERVER_CERT_FILE", "VAR_CONTROL_KEY_FILE", "VAR_CONTROL_CERT_FILE",
  "VAR_CONTROL_USE_CERT", "VAR_EXTENDED_STATISTICS", "VAR_LOCAL_DATA_PTR",
  "VAR_JOSTLE_TIMEOUT", "VAR_STUB_PRIME", "VAR_UNWANTED_REPLY_THRESHOLD",
  "VAR_LOG_TIME_ASCII", "VAR_DOMAIN_INSECURE", "VAR_PYTHON",
  "VAR_PYTHON_SCRIPT", "VAR_VAL_SIG_SKEW_MIN", "VAR_VAL_SIG_SKEW_MAX",
  "VAR_CACHE_MIN_TTL", "VAR_VAL_LOG_LEVEL", "VAR_AUTO_TRUST_ANCHOR_FILE",
  "VAR_KEEP_MISSING", "VAR_ADD_HOLDDOWN", "VAR_DEL_HOLDDOWN",
  "VAR_SO_RCVBUF", "VAR_EDNS_BUFFER_SIZE", "VAR_PREFETCH",
  "VAR_PREFETCH_KEY", "VAR_SO_SNDBUF", "VAR_SO_REUSEPORT",
  "VAR_HARDEN_BELOW_NXDOMAIN", "VAR_IGNORE_CD_FLAG", "VAR_LOG_QUERIES",
  "VAR_LOG_REPLIES", "VAR_LOG_LOCAL_ACTIONS", "VAR_TCP_UPSTREAM",
  "VAR_SSL_UPSTREAM", "VAR_SSL_SERVICE_KEY", "VAR_SSL_SERVICE_PEM",
  "VAR_SSL_PORT", "VAR_FORWARD_FIRST", "VAR_STUB_SSL_UPSTREAM",
  "VAR_FORWARD_SSL_UPSTREAM", "VAR_TLS_CERT_BUNDLE", "VAR_STUB_FIRST",
  "VAR_MINIMAL_RESPONSES", "VAR_RRSET_ROUNDROBIN", "VAR_MAX_UDP_SIZE",
  "VAR_DELAY_CLOSE", "VAR_UNBLOCK_LAN_ZONES", "VAR_INSECURE_LAN_ZONES",
  "VAR_INFRA_CACHE_MIN_RTT", "VAR_DNS64_PREFIX", "VAR_DNS64_SYNTHALL",
  "VAR_DNS64_IGNORE_AAAA", "VAR_DNSTAP", "VAR_DNSTAP_ENABLE",
  "VAR_DNSTAP_SOCKET_PATH", "VAR_DNSTAP_SEND_IDENTITY",
  "VAR_DNSTAP_SEND_VERSION", "VAR_DNSTAP_IDENTITY", "VAR_DNSTAP_VERSION",
  "VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES",
  "VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES",
  "VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES",
  "VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES",
  "VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES",
  "VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES", "VAR_RESPONSE_IP_TAG",
  "VAR_RESPONSE_IP", "VAR_RESPONSE_IP_DATA", "VAR_HARDEN_ALGO_DOWNGRADE",
  "VAR_IP_TRANSPARENT", "VAR_DISABLE_DNSSEC_LAME_CHECK",
  "VAR_IP_RATELIMIT", "VAR_IP_RATELIMIT_SLABS", "VAR_IP_RATELIMIT_SIZE",
  "VAR_RATELIMIT", "VAR_RATELIMIT_SLABS", "VAR_RATELIMIT_SIZE",
  "VAR_RATELIMIT_FOR_DOMAIN", "VAR_RATELIMIT_BELOW_DOMAIN",
  "VAR_IP_RATELIMIT_FACTOR", "VAR_RATELIMIT_FACTOR",
  "VAR_SEND_CLIENT_SUBNET", "VAR_CLIENT_SUBNET_ZONE",
  "VAR_CLIENT_SUBNET_ALWAYS_FORWARD", "VAR_CLIENT_SUBNET_OPCODE",
  "VAR_MAX_CLIENT_SUBNET_IPV4", "VAR_MAX_CLIENT_SUBNET_IPV6",
  "VAR_MIN_CLIENT_SUBNET_IPV4", "VAR_MIN_CLIENT_SUBNET_IPV6",
  "VAR_MAX_ECS_TREE_SIZE_IPV4", "VAR_MAX_ECS_TREE_SIZE_IPV6",
  "VAR_CAPS_WHITELIST", "VAR_CACHE_MAX_NEGATIVE_TTL",
  "VAR_PERMIT_SMALL_HOLDDOWN", "VAR_QNAME_MINIMISATION",
  "VAR_QNAME_MINIMISATION_STRICT", "VAR_IP_FREEBIND", "VAR_DEFINE_TAG",
  "VAR_LOCAL_ZONE_TAG", "VAR_ACCESS_CONTROL_TAG",
  "VAR_LOCAL_ZONE_OVERRIDE", "VAR_ACCESS_CONTROL_TAG_ACTION",
  "VAR_ACCESS_CONTROL_TAG_DATA", "VAR_VIEW", "VAR_ACCESS_CONTROL_VIEW",
  "VAR_VIEW_FIRST", "VAR_SERVE_EXPIRED", "VAR_SERVE_EXPIRED_TTL",
  "VAR_SERVE_EXPIRED_TTL_RESET", "VAR_SERVE_EXPIRED_REPLY_TTL",
  "VAR_SERVE_EXPIRED_CLIENT_TIMEOUT", "VAR_FAKE_DSA", "VAR_FAKE_SHA1",
  "VAR_LOG_IDENTITY", "VAR_HIDE_TRUSTANCHOR", "VAR_TRUST_ANCHOR_SIGNALING",
  "VAR_AGGRESSIVE_NSEC", "VAR_USE_SYSTEMD", "VAR_SHM_ENABLE",
  "VAR_SHM_KEY", "VAR_ROOT_KEY_SENTINEL", "VAR_DNSCRYPT",
  "VAR_DNSCRYPT_ENABLE", "VAR_DNSCRYPT_PORT", "VAR_DNSCRYPT_PROVIDER",
  "VAR_DNSCRYPT_SECRET_KEY", "VAR_DNSCRYPT_PROVIDER_CERT",
  "VAR_DNSCRYPT_PROVIDER_CERT_ROTATED",
  "VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE",
  "VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS",
  "VAR_DNSCRYPT_NONCE_CACHE_SIZE", "VAR_DNSCRYPT_NONCE_CACHE_SLABS",
  "VAR_IPSECMOD_ENABLED", "VAR_IPSECMOD_HOOK", "VAR_IPSECMOD_IGNORE_BOGUS",
  "VAR_IPSECMOD_MAX_TTL", "VAR_IPSECMOD_WHITELIST", "VAR_IPSECMOD_STRICT",
  "VAR_CACHEDB", "VAR_CACHEDB_BACKEND", "VAR_CACHEDB_SECRETSEED",
  "VAR_CACHEDB_REDISHOST", "VAR_CACHEDB_REDISPORT",
  "VAR_CACHEDB_REDISTIMEOUT", "VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM",
  "VAR_FOR_UPSTREAM", "VAR_AUTH_ZONE", "VAR_ZONEFILE", "VAR_MASTER",
  "VAR_URL", "VAR_FOR_DOWNSTREAM", "VAR_FALLBACK_ENABLED",
  "VAR_TLS_ADDITIONAL_PORT", "VAR_LOW_RTT", "VAR_LOW_RTT_PERMIL",
  "VAR_FAST_SERVER_PERMIL", "VAR_FAST_SERVER_NUM", "VAR_ALLOW_NOTIFY",
  "VAR_TLS_WIN_CERT", "VAR_TCP_CONNECTION_LIMIT", "VAR_FORWARD_NO_CACHE",
  "VAR_STUB_NO_CACHE", "VAR_LOG_SERVFAIL", "VAR_DENY_ANY",
  "VAR_UNKNOWN_SERVER_TIME_LIMIT", "VAR_LOG_TAG_QUERYREPLY",
  "VAR_STREAM_WAIT_SIZE", "VAR_TLS_CIPHERS", "VAR_TLS_CIPHERSUITES",
  "VAR_IPSET", "VAR_IPSET_NAME_V4", "VAR_IPSET_NAME_V6",
  "VAR_TLS_SESSION_TICKET_KEYS", "VAR_RPZ", "VAR_TAGS",
  "VAR_RPZ_ACTION_OVERRIDE", "VAR_RPZ_CNAME_OVERRIDE", "VAR_RPZ_LOG",
  "VAR_RPZ_LOG_NAME", "$accept", "toplevelvars", "toplevelvar",
  "serverstart", "contents_server", "content_server", "stubstart",
  "contents_stub", "content_stub", "forwardstart", "contents_forward",
  "content_forward", "viewstart", "contents_view", "content_view",
  "authstart", "contents_auth", "content_auth", "rpz_tag",
  "rpz_action_override", "rpz_cname_override", "rpz_log", "rpz_log_name",
  "rpzstart", "contents_rpz", "content_rpz", "server_num_threads",
  "server_verbosity", "server_statistics_interval",
  "server_statistics_cumulative", "server_extended_statistics",
  "server_shm_enable", "server_shm_key", "server_port",
  "server_send_client_subnet", "server_client_subnet_zone",
  "server_client_subnet_always_forward", "server_client_subnet_opcode",
  "server_max_client_subnet_ipv4", "server_max_client_subnet_ipv6",
  "server_min_client_subnet_ipv4", "server_min_client_subnet_ipv6",
  "server_max_ecs_tree_size_ipv4", "server_max_ecs_tree_size_ipv6",
  "server_interface", "server_outgoing_interface", "server_outgoing_range",
  "server_outgoing_port_permit", "server_outgoing_port_avoid",
  "server_outgoing_num_tcp", "server_incoming_num_tcp",
  "server_interface_automatic", "server_do_ip4", "server_do_ip6",
  "server_do_udp", "server_do_tcp", "server_prefer_ip6", "server_tcp_mss",
  "server_outgoing_tcp_mss", "server_tcp_idle_timeout",
  "server_tcp_keepalive", "server_tcp_keepalive_timeout",
  "server_tcp_upstream", "server_udp_upstream_without_downstream",
  "server_ssl_upstream", "server_ssl_service_key",
  "server_ssl_service_pem", "server_ssl_port", "server_tls_cert_bundle",
  "server_tls_win_cert", "server_tls_additional_port",
  "server_tls_ciphers", "server_tls_ciphersuites",
  "server_tls_session_ticket_keys", "server_use_systemd",
  "server_do_daemonize", "server_use_syslog", "server_log_time_ascii",
  "server_log_queries", "server_log_replies", "server_log_tag_queryreply",
  "server_log_servfail", "server_log_local_actions", "server_chroot",
  "server_username", "server_directory", "server_logfile",
  "server_pidfile", "server_root_hints", "server_dlv_anchor_file",
  "server_dlv_anchor", "server_auto_trust_anchor_file",
  "server_trust_anchor_file", "server_trusted_keys_file",
  "server_trust_anchor", "server_trust_anchor_signaling",
  "server_root_key_sentinel", "server_domain_insecure",
  "server_hide_identity", "server_hide_version", "server_hide_trustanchor",
  "server_identity", "server_version", "server_so_rcvbuf",
  "server_so_sndbuf", "server_so_reuseport", "server_ip_transparent",
  "server_ip_freebind", "server_stream_wait_size",
  "server_edns_buffer_size", "server_msg_buffer_size",
  "server_msg_cache_size", "server_msg_cache_slabs",
  "server_num_queries_per_thread", "server_jostle_timeout",
  "server_delay_close", "server_unblock_lan_zones",
  "server_insecure_lan_zones", "server_rrset_cache_size",
  "server_rrset_cache_slabs", "server_infra_host_ttl",
  "server_infra_lame_ttl", "server_infra_cache_numhosts",
  "server_infra_cache_lame_size", "server_infra_cache_slabs",
  "server_infra_cache_min_rtt", "server_target_fetch_policy",
  "server_harden_short_bufsize", "server_harden_large_queries",
  "server_harden_glue", "server_harden_dnssec_stripped",
  "server_harden_below_nxdomain", "server_harden_referral_path",
  "server_harden_algo_downgrade", "server_use_caps_for_id",
  "server_caps_whitelist", "server_private_address",
  "server_private_domain", "server_prefetch", "server_prefetch_key",
  "server_deny_any", "server_unwanted_reply_threshold",
  "server_do_not_query_address", "server_do_not_query_localhost",
  "server_access_control", "server_module_conf",
  "server_val_override_date", "server_val_sig_skew_min",
  "server_val_sig_skew_max", "server_cache_max_ttl",
  "server_cache_max_negative_ttl", "server_cache_min_ttl",
  "server_bogus_ttl", "server_val_clean_additional",
  "server_val_permissive_mode", "server_aggressive_nsec",
  "server_ignore_cd_flag", "server_serve_expired",
  "server_serve_expired_ttl", "server_serve_expired_ttl_reset",
  "server_serve_expired_reply_ttl", "server_serve_expired_client_timeout",
  "server_fake_dsa", "server_fake_sha1", "server_val_log_level",
  "server_val_nsec3_keysize_iterations", "server_add_holddown",
  "server_del_holddown", "server_keep_missing",
  "server_permit_small_holddown", "server_key_cache_size",
  "server_key_cache_slabs", "server_neg_cache_size", "server_local_zone",
  "server_local_data", "server_local_data_ptr", "server_minimal_responses",
  "server_rrset_roundrobin", "server_unknown_server_time_limit",
  "server_max_udp_size", "server_dns64_prefix", "server_dns64_synthall",
  "server_dns64_ignore_aaaa", "server_define_tag", "server_local_zone_tag",
  "server_access_control_tag", "server_access_control_tag_action",
  "server_access_control_tag_data", "server_local_zone_override",
  "server_access_control_view", "server_response_ip_tag",
  "server_ip_ratelimit", "server_ratelimit", "server_ip_ratelimit_size",
  "server_ratelimit_size", "server_ip_ratelimit_slabs",
  "server_ratelimit_slabs", "server_ratelimit_for_domain",
  "server_ratelimit_below_domain", "server_ip_ratelimit_factor",
  "server_ratelimit_factor", "server_low_rtt", "server_fast_server_num",
  "server_fast_server_permil", "server_qname_minimisation",
  "server_qname_minimisation_strict", "server_ipsecmod_enabled",
  "server_ipsecmod_ignore_bogus", "server_ipsecmod_hook",
  "server_ipsecmod_max_ttl", "server_ipsecmod_whitelist",
  "server_ipsecmod_strict", "stub_name", "stub_host", "stub_addr",
  "stub_first", "stub_no_cache", "stub_ssl_upstream", "stub_prime",
  "forward_name", "forward_host", "forward_addr", "forward_first",
  "forward_no_cache", "forward_ssl_upstream", "auth_name", "auth_zonefile",
  "auth_master", "auth_url", "auth_allow_notify", "auth_for_downstream",
  "auth_for_upstream", "auth_fallback_enabled", "view_name",
  "view_local_zone", "view_response_ip", "view_response_ip_data",
  "view_local_data", "view_local_data_ptr", "view_first", "rcstart",
  "contents_rc", "content_rc", "rc_control_enable", "rc_control_port",
  "rc_control_interface", "rc_control_use_cert", "rc_server_key_file",
  "rc_server_cert_file", "rc_control_key_file", "rc_control_cert_file",
  "dtstart", "contents_dt", "content_dt", "dt_dnstap_enable",
  "dt_dnstap_socket_path", "dt_dnstap_send_identity",
  "dt_dnstap_send_version", "dt_dnstap_identity", "dt_dnstap_version",
  "dt_dnstap_log_resolver_query_messages",
  "dt_dnstap_log_resolver_response_messages",
  "dt_dnstap_log_client_query_messages",
  "dt_dnstap_log_client_response_messages",
  "dt_dnstap_log_forwarder_query_messages",
  "dt_dnstap_log_forwarder_response_messages", "pythonstart",
  "contents_py", "content_py", "py_script",
  "server_disable_dnssec_lame_check", "server_log_identity",
  "server_response_ip", "server_response_ip_data", "dnscstart",
  "contents_dnsc", "content_dnsc", "dnsc_dnscrypt_enable",
  "dnsc_dnscrypt_port", "dnsc_dnscrypt_provider",
  "dnsc_dnscrypt_provider_cert", "dnsc_dnscrypt_provider_cert_rotated",
  "dnsc_dnscrypt_secret_key", "dnsc_dnscrypt_shared_secret_cache_size",
  "dnsc_dnscrypt_shared_secret_cache_slabs",
  "dnsc_dnscrypt_nonce_cache_size", "dnsc_dnscrypt_nonce_cache_slabs",
  "cachedbstart", "contents_cachedb", "content_cachedb",
  "cachedb_backend_name", "cachedb_secret_seed", "redis_server_host",
  "redis_server_port", "redis_timeout", "server_tcp_connection_limit",
  "ipsetstart", "contents_ipset", "content_ipset", "ipset_name_v4",
  "ipset_name_v6", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381,   382,   383,   384,
     385,   386,   387,   388,   389,   390,   391,   392,   393,   394,
     395,   396,   397,   398,   399,   400,   401,   402,   403,   404,
     405,   406,   407,   408,   409,   410,   411,   412,   413,   414,
     415,   416,   417,   418,   419,   420,   421,   422,   423,   424,
     425,   426,   427,   428,   429,   430,   431,   432,   433,   434,
     435,   436,   437,   438,   439,   440,   441,   442,   443,   444,
     445,   446,   447,   448,   449,   450,   451,   452,   453,   454,
     455,   456,   457,   458,   459,   460,   461,   462,   463,   464,
     465,   466,   467,   468,   469,   470,   471,   472,   473,   474,
     475,   476,   477,   478,   479,   480,   481,   482,   483,   484,
     485,   486,   487,   488,   489,   490,   491,   492,   493,   494,
     495,   496,   497,   498,   499,   500,   501,   502,   503,   504,
     505,   506,   507,   508,   509,   510,   511,   512,   513,   514,
     515,   516,   517,   518,   519,   520,   521,   522,   523,   524,
     525,   526,   527,   528,   529,   530,   531,   532
};
# endif

#define YYPACT_NINF -262

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-262)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -262,     0,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,   261,   -41,
     -36,   -40,   -20,   -42,   -29,  -128,  -105,  -170,  -221,  -261,
       2,     3,     4,    12,    26,    27,    28,    31,    32,    33,
      35,    36,    37,    38,    39,    51,    52,    53,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    85,    87,
      88,    91,    93,    94,    95,    96,    97,    98,    99,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,   189,   190,   191,   193,   197,   198,   199,
     200,   201,   202,   203,   205,   206,   207,   208,   211,   212,
     215,   228,   229,   230,   231,   232,   233,   234,   236,   237,
     238,   239,   240,   241,   242,   243,   244,   245,   246,   247,
     248,   249,   250,   251,   252,   253,   254,   255,   256,   257,
     259,   260,   294,   295,   296,   297,   301,   302,   303,   345,
     346,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,   347,   348,   349,   350,   351,   352,   353,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,   357,   361,
     362,   387,   388,   389,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,   391,   402,   403,   404,   405,   406,   407,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,   408,   409,   410,   411,
     412,   413,   414,   453,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,   455,   471,   472,   473,   474,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,   475,
     476,   477,   478,   479,   480,   481,   488,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,   489,   490,   491,   492,
     493,   495,   496,   497,   498,   499,   500,   501,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,   504,  -262,  -262,   507,   510,   511,   519,   520,   521,
     523,   524,   525,   526,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,   527,   528,   529,   530,   531,
    -262,  -262,  -262,  -262,  -262,  -262,   532,   533,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
     534,   535,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,   536,   537,   538,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,   539,
     540,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
     541,   542,   543,   544,   545,   546,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,   547,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,   548,  -262,  -262,   549,
     550,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,   551,   552,   553,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     1,    16,   210,   220,   482,   528,   501,   229,
     537,   560,   239,   574,   255,     3,    18,   212,   222,   231,
     241,   257,   484,   503,   530,   539,   562,   576,     4,     5,
       6,    10,    14,    15,     8,     9,     7,    11,    12,    13,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    17,    19,    20,    82,    85,    94,   181,   182,    21,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
      33,    73,    22,    86,    87,    44,    66,    81,    23,    24,
      26,    27,    25,    28,    29,    30,    31,    32,   117,   193,
     118,   120,   121,   122,   195,   200,   196,   207,   208,   209,
     177,    83,    72,    98,   115,   116,   205,   202,   119,    34,
      35,    36,    37,    38,    74,    88,    89,   104,    60,    70,
      61,   185,   186,    99,    54,    55,   184,    56,    57,   108,
     112,   126,   135,   160,   206,   109,    67,    39,    40,    41,
      96,   127,   128,   129,    42,    43,    45,    46,    48,    49,
      47,   133,    50,    51,    52,    58,    77,   113,    91,   134,
      84,   156,    92,    93,   110,   111,   203,    97,    53,    75,
      78,    59,    62,   100,   101,    76,   157,   102,    63,    64,
      65,   194,   114,   170,   171,   172,   173,   174,   175,   183,
     103,    71,   105,   106,   107,   158,    68,    69,    90,    79,
      80,    95,   123,   124,   204,   125,   130,   131,   132,   161,
     162,   164,   166,   167,   165,   168,   178,   136,   137,   140,
     141,   138,   139,   142,   143,   145,   144,   197,   199,   198,
     159,   169,   187,   189,   188,   190,   191,   192,   163,   176,
     179,   180,   201,     0,     0,     0,     0,     0,     0,     0,
     211,   213,   214,   215,   217,   218,   219,   216,     0,     0,
       0,     0,     0,     0,   221,   223,   224,   225,   226,   227,
     228,     0,     0,     0,     0,     0,     0,     0,   230,   232,
     233,   236,   237,   234,   238,   235,     0,     0,     0,     0,
       0,     0,     0,     0,   240,   242,   243,   244,   245,   249,
     246,   247,   248,     0,     0,     0,     0,     0,   260,   264,
     265,   266,   267,   256,   258,   259,   261,   262,   263,     0,
       0,     0,     0,     0,     0,     0,     0,   483,   485,   487,
     486,   492,   488,   489,   490,   491,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   502,   504,
     505,   506,   507,   508,   509,   510,   511,   512,   513,   514,
     515,     0,   529,   531,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   538,   540,   541,   542,   544,   545,
     543,   546,   547,   548,   549,     0,     0,     0,     0,     0,
     561,   563,   564,   565,   566,   567,     0,     0,   575,   577,
     578,   269,   268,   275,   288,   286,   294,   295,   298,   296,
     297,   299,   300,   301,   302,   303,   325,   326,   327,   328,
     329,   353,   354,   355,   360,   361,   291,   362,   363,   366,
     364,   365,   368,   369,   370,   384,   340,   341,   343,   344,
     371,   387,   334,   336,   388,   394,   395,   396,   292,   352,
     412,   413,   335,   407,   318,   287,   330,   385,   391,   372,
       0,     0,   416,   293,   270,   317,   376,   271,   289,   290,
     331,   332,   414,   374,   378,   379,   272,   417,   356,   383,
     319,   339,   389,   390,   393,   406,   333,   410,   408,   409,
     345,   351,   380,   381,   346,   347,   373,   398,   320,   321,
     324,   304,   306,   307,   308,   309,   310,   418,   419,   421,
     357,   358,   359,   367,   422,   423,   424,     0,     0,     0,
     375,   348,   533,   433,   437,   435,   434,   438,   436,     0,
       0,   441,   442,   276,   277,   278,   279,   280,   281,   282,
     283,   284,   285,   377,   392,   411,   446,   447,   349,   425,
       0,     0,     0,     0,     0,     0,   399,   400,   401,   402,
     403,   404,   405,   534,   342,   337,   397,   316,   273,   274,
     338,   448,   450,   449,   451,   452,   453,   305,   312,   443,
     445,   444,   311,     0,   323,   382,   420,   322,   350,   313,
     314,   315,   454,   455,   456,   460,   459,   457,   458,   461,
     462,   463,   464,   466,   465,   475,     0,   479,   480,     0,
       0,   481,   467,   473,   468,   469,   470,   472,   474,   471,
     250,   251,   252,   253,   254,   493,   495,   494,   497,   498,
     499,   500,   496,   516,   517,   518,   519,   520,   521,   522,
     523,   524,   525,   526,   527,   532,   550,   551,   552,   555,
     553,   554,   556,   557,   558,   559,   568,   569,   570,   571,
     572,   579,   580,   386,   415,   432,   535,   536,   439,   440,
     426,   427,     0,     0,     0,   431,   573,   476,   477,   478,
     430,   428,   429
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,   -23,   554,   555,   556,   557,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,  -262,
    -262
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,    15,    16,    28,   231,    17,    29,   430,    18,
      30,   444,    19,    31,   458,    20,    32,   474,   488,   489,
     490,   491,   492,    21,    33,   493,   232,   233,   234,   235,
     236,   237,   238,   239,   240,   241,   242,   243,   244,   245,
     246,   247,   248,   249,   250,   251,   252,   253,   254,   255,
     256,   257,   258,   259,   260,   261,   262,   263,   264,   265,
     266,   267,   268,   269,   270,   271,   272,   273,   274,   275,
     276,   277,   278,   279,   280,   281,   282,   283,   284,   285,
     286,   287,   288,   289,   290,   291,   292,   293,   294,   295,
     296,   297,   298,   299,   300,   301,   302,   303,   304,   305,
     306,   307,   308,   309,   310,   311,   312,   313,   314,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,   337,   338,   339,   340,   341,   342,   343,   344,   345,
     346,   347,   348,   349,   350,   351,   352,   353,   354,   355,
     356,   357,   358,   359,   360,   361,   362,   363,   364,   365,
     366,   367,   368,   369,   370,   371,   372,   373,   374,   375,
     376,   377,   378,   379,   380,   381,   382,   383,   384,   385,
     386,   387,   388,   389,   390,   391,   392,   393,   394,   395,
     396,   397,   398,   399,   400,   401,   402,   403,   404,   405,
     406,   407,   408,   409,   410,   411,   412,   413,   414,   415,
     416,   417,   431,   432,   433,   434,   435,   436,   437,   445,
     446,   447,   448,   449,   450,   475,   476,   477,   478,   479,
     480,   481,   482,   459,   460,   461,   462,   463,   464,   465,
      22,    34,   507,   508,   509,   510,   511,   512,   513,   514,
     515,    23,    35,   528,   529,   530,   531,   532,   533,   534,
     535,   536,   537,   538,   539,   540,    24,    36,   542,   543,
     418,   419,   420,   421,    25,    37,   554,   555,   556,   557,
     558,   559,   560,   561,   562,   563,   564,    26,    38,   570,
     571,   572,   573,   574,   575,   422,    27,    39,   578,   579,
     580
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint16 yytable[] =
{
       2,   466,   423,   451,   424,   425,   541,   438,   576,   577,
     494,     3,   581,   582,   583,   439,   440,   565,   566,   567,
     568,   569,   584,   466,   516,   517,   518,   519,   520,   521,
     522,   523,   524,   525,   526,   527,   585,   586,   587,   452,
     453,   588,   589,   590,     4,   591,   592,   593,   594,   595,
       5,   544,   545,   546,   547,   548,   549,   550,   551,   552,
     553,   596,   597,   598,   454,   426,   499,   500,   501,   502,
     503,   504,   505,   506,   599,   600,   601,   602,   603,   604,
     605,   606,   607,   608,   609,   610,   611,   612,   613,   614,
     615,   616,   617,   618,     6,   619,   427,   620,   621,   428,
     441,   622,   442,   623,   624,   625,   626,   627,   628,   629,
       7,   630,   631,   632,   633,   634,   635,   636,   637,   638,
     639,   640,   641,   642,   643,   455,   456,   644,   645,   646,
     647,   648,   649,   650,   651,   652,   653,   654,   655,   656,
     657,   658,   659,   660,   661,   662,   663,   664,   665,   666,
     667,     8,   668,   669,   670,   671,   672,   673,   674,   675,
     676,   677,   678,   679,   457,   680,   681,   682,   683,   684,
     685,   686,   687,   688,   689,   690,   691,   692,   693,   694,
     695,   696,   697,   698,   699,   700,   701,   702,   703,   704,
     705,   706,   707,   708,   709,   710,   711,   712,   713,   714,
     715,   716,     9,   717,   468,   469,   470,   718,   719,   720,
     721,   722,   723,   724,   473,   725,   726,   727,   728,   429,
      10,   729,   730,   443,   467,   731,   468,   469,   470,   471,
     472,   483,   484,   485,   486,   487,   473,    11,   732,   733,
     734,   735,   736,   737,   738,    12,   739,   740,   741,   742,
     743,   744,   745,   746,   747,   748,   749,   750,   751,   752,
     753,   754,   755,   756,   757,   758,   759,   760,    13,   761,
     762,     0,    14,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,   763,   764,   765,   766,    71,    72,
      73,   767,   768,   769,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   770,   771,   772,   773,   774,
     775,   776,   777,   778,   115,   116,   117,   779,   118,   119,
     120,   780,   781,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   782,   783,   784,
     145,   785,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   786,   787,   788,   789,   790,   791,   792,   793,
     794,   795,   796,   797,   798,   156,   157,   158,   159,   160,
     161,   162,   163,   164,   165,   166,   167,   168,   169,   170,
     171,   172,   173,   174,   175,   176,   177,   178,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   188,   189,   190,
     191,   192,   193,   799,   194,   800,   195,   196,   197,   198,
     199,   200,   201,   202,   203,   204,   205,   206,   207,   208,
     209,   801,   802,   803,   804,   805,   806,   807,   808,   809,
     810,   811,   210,   211,   212,   213,   214,   215,   812,   813,
     814,   815,   816,   817,   216,   818,   819,   820,   821,   822,
     823,   824,   217,   218,   825,   219,   220,   826,   221,   222,
     827,   828,   223,   224,   225,   226,   227,   228,   229,   829,
     830,   831,   230,   832,   833,   834,   835,   836,   837,   838,
     839,   840,   841,   842,   843,   844,   845,   846,   847,   848,
     849,   850,   851,   852,   853,   854,   855,   856,   857,   858,
     859,   860,   861,   862,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   495,   496,   497,
     498
};

static const yytype_int16 yycheck[] =
{
       0,    43,    43,    43,    45,    46,   111,    43,   269,   270,
      33,    11,    10,    10,    10,    51,    52,   238,   239,   240,
     241,   242,    10,    43,   152,   153,   154,   155,   156,   157,
     158,   159,   160,   161,   162,   163,    10,    10,    10,    79,
      80,    10,    10,    10,    44,    10,    10,    10,    10,    10,
      50,   221,   222,   223,   224,   225,   226,   227,   228,   229,
     230,    10,    10,    10,   104,   106,    95,    96,    97,    98,
      99,   100,   101,   102,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    94,    10,   137,    10,    10,   140,
     136,    10,   138,    10,    10,    10,    10,    10,    10,    10,
     110,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,   165,   166,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,   151,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,   204,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,   202,    10,   246,   247,   248,    10,    10,    10,
      10,    10,    10,    10,   256,    10,    10,    10,    10,   260,
     220,    10,    10,   259,   244,    10,   246,   247,   248,   249,
     250,   273,   274,   275,   276,   277,   256,   237,    10,    10,
      10,    10,    10,    10,    10,   245,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,   268,    10,
      10,    -1,   272,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    10,    10,    10,    10,    47,    48,
      49,    10,    10,    10,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    10,    10,    10,    10,    10,
      10,    10,    10,    10,   103,   104,   105,    10,   107,   108,
     109,    10,    10,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   134,   135,    10,    10,    10,
     139,    10,   141,   142,   143,   144,   145,   146,   147,   148,
     149,   150,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,   164,   165,   166,   167,   168,
     169,   170,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,   181,   182,   183,   184,   185,   186,   187,   188,
     189,   190,   191,   192,   193,   194,   195,   196,   197,   198,
     199,   200,   201,    10,   203,    10,   205,   206,   207,   208,
     209,   210,   211,   212,   213,   214,   215,   216,   217,   218,
     219,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,   231,   232,   233,   234,   235,   236,    10,    10,
      10,    10,    10,    10,   243,    10,    10,    10,    10,    10,
      10,    10,   251,   252,    10,   254,   255,    10,   257,   258,
      10,    10,   261,   262,   263,   264,   265,   266,   267,    10,
      10,    10,   271,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    33,    33,    33,
      33
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   279,     0,    11,    44,    50,    94,   110,   151,   202,
     220,   237,   245,   268,   272,   280,   281,   284,   287,   290,
     293,   301,   518,   529,   544,   552,   565,   574,   282,   285,
     288,   291,   294,   302,   519,   530,   545,   553,   566,   575,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    47,    48,    49,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,   103,   104,   105,   107,   108,
     109,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   139,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,   189,   190,   191,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,   203,   205,   206,   207,   208,   209,
     210,   211,   212,   213,   214,   215,   216,   217,   218,   219,
     231,   232,   233,   234,   235,   236,   243,   251,   252,   254,
     255,   257,   258,   261,   262,   263,   264,   265,   266,   267,
     271,   283,   304,   305,   306,   307,   308,   309,   310,   311,
     312,   313,   314,   315,   316,   317,   318,   319,   320,   321,
     322,   323,   324,   325,   326,   327,   328,   329,   330,   331,
     332,   333,   334,   335,   336,   337,   338,   339,   340,   341,
     342,   343,   344,   345,   346,   347,   348,   349,   350,   351,
     352,   353,   354,   355,   356,   357,   358,   359,   360,   361,
     362,   363,   364,   365,   366,   367,   368,   369,   370,   371,
     372,   373,   374,   375,   376,   377,   378,   379,   380,   381,
     382,   383,   384,   385,   386,   387,   388,   389,   390,   391,
     392,   393,   394,   395,   396,   397,   398,   399,   400,   401,
     402,   403,   404,   405,   406,   407,   408,   409,   410,   411,
     412,   413,   414,   415,   416,   417,   418,   419,   420,   421,
     422,   423,   424,   425,   426,   427,   428,   429,   430,   431,
     432,   433,   434,   435,   436,   437,   438,   439,   440,   441,
     442,   443,   444,   445,   446,   447,   448,   449,   450,   451,
     452,   453,   454,   455,   456,   457,   458,   459,   460,   461,
     462,   463,   464,   465,   466,   467,   468,   469,   470,   471,
     472,   473,   474,   475,   476,   477,   478,   479,   480,   481,
     482,   483,   484,   485,   486,   487,   488,   489,   548,   549,
     550,   551,   573,    43,    45,    46,   106,   137,   140,   260,
     286,   490,   491,   492,   493,   494,   495,   496,    43,    51,
      52,   136,   138,   259,   289,   497,   498,   499,   500,   501,
     502,    43,    79,    80,   104,   165,   166,   204,   292,   511,
     512,   513,   514,   515,   516,   517,    43,   244,   246,   247,
     248,   249,   250,   256,   295,   503,   504,   505,   506,   507,
     508,   509,   510,   273,   274,   275,   276,   277,   296,   297,
     298,   299,   300,   303,   503,   504,   505,   506,   507,    95,
      96,    97,    98,    99,   100,   101,   102,   520,   521,   522,
     523,   524,   525,   526,   527,   528,   152,   153,   154,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   531,   532,
     533,   534,   535,   536,   537,   538,   539,   540,   541,   542,
     543,   111,   546,   547,   221,   222,   223,   224,   225,   226,
     227,   228,   229,   230,   554,   555,   556,   557,   558,   559,
     560,   561,   562,   563,   564,   238,   239,   240,   241,   242,
     567,   568,   569,   570,   571,   572,   269,   270,   576,   577,
     578,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   278,   279,   279,   280,   280,   280,   280,   280,   280,
     280,   280,   280,   280,   280,   280,   281,   282,   282,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     283,   283,   283,   283,   283,   283,   283,   283,   283,   283,
     284,   285,   285,   286,   286,   286,   286,   286,   286,   286,
     287,   288,   288,   289,   289,   289,   289,   289,   289,   290,
     291,   291,   292,   292,   292,   292,   292,   292,   292,   293,
     294,   294,   295,   295,   295,   295,   295,   295,   295,   295,
     296,   297,   298,   299,   300,   301,   302,   302,   303,   303,
     303,   303,   303,   303,   303,   303,   303,   303,   304,   305,
     306,   307,   308,   309,   310,   311,   312,   313,   314,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,   337,   338,   339,   340,   341,   342,   343,   344,   345,
     346,   347,   348,   349,   350,   351,   352,   353,   354,   355,
     356,   357,   358,   359,   360,   361,   362,   363,   364,   365,
     366,   367,   368,   369,   370,   371,   372,   373,   374,   375,
     376,   377,   378,   379,   380,   381,   382,   383,   384,   385,
     386,   387,   388,   389,   390,   391,   392,   393,   394,   395,
     396,   397,   398,   399,   400,   401,   402,   403,   404,   405,
     406,   407,   408,   409,   410,   411,   412,   413,   414,   415,
     416,   417,   418,   419,   420,   421,   422,   423,   424,   425,
     426,   427,   428,   429,   430,   431,   432,   433,   434,   435,
     436,   437,   438,   439,   440,   441,   442,   443,   444,   445,
     446,   447,   448,   449,   450,   451,   452,   453,   454,   455,
     456,   457,   458,   459,   460,   461,   462,   463,   464,   465,
     466,   467,   468,   469,   470,   471,   472,   473,   474,   475,
     476,   477,   478,   479,   480,   481,   482,   483,   484,   485,
     486,   487,   488,   489,   490,   491,   492,   493,   494,   495,
     496,   497,   498,   499,   500,   501,   502,   503,   504,   505,
     506,   507,   508,   509,   510,   511,   512,   513,   514,   515,
     516,   517,   518,   519,   519,   520,   520,   520,   520,   520,
     520,   520,   520,   521,   522,   523,   524,   525,   526,   527,
     528,   529,   530,   530,   531,   531,   531,   531,   531,   531,
     531,   531,   531,   531,   531,   531,   532,   533,   534,   535,
     536,   537,   538,   539,   540,   541,   542,   543,   544,   545,
     545,   546,   547,   548,   549,   550,   551,   552,   553,   553,
     554,   554,   554,   554,   554,   554,   554,   554,   554,   554,
     555,   556,   557,   558,   559,   560,   561,   562,   563,   564,
     565,   566,   566,   567,   567,   567,   567,   567,   568,   569,
     570,   571,   572,   573,   574,   575,   575,   576,   576,   577,
     578
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     0,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     0,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     0,     1,     1,     1,     1,     1,     1,     1,
       2,     0,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     0,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     2,     2,     2,     2,     1,     2,     0,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     3,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     3,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     3,     3,     4,     4,
       4,     3,     3,     2,     2,     2,     2,     2,     2,     3,
       3,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     3,     3,     3,     2,
       2,     2,     1,     2,     0,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     2,     2,     2,     2,     2,     2,
       2,     1,     2,     0,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     1,     2,
       0,     1,     2,     2,     2,     3,     3,     1,     2,     0,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       1,     2,     0,     1,     1,     1,     1,     1,     2,     2,
       2,     2,     2,     3,     1,     2,     0,     1,     1,     2,
       2
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YYUSE (yyoutput);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyo, yytype, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &yyvsp[(yyi + 1) - (yynrhs)]
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return (YYSIZE_T) (yystpcpy (yyres, yystr) - yyres);
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
                    yysize = yysize1;
                  else
                    return 2;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
      yysize = yysize1;
    else
      return 2;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yynewstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  *yyssp = (yytype_int16) yystate;

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = (YYSIZE_T) (yyssp - yyss + 1);

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
# undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 16:
#line 186 "util/configparser.y"
    { 
		OUTYY(("\nP(server:)\n")); 
	}
#line 2670 "util/configparser.c"
    break;

  case 210:
#line 278 "util/configparser.y"
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
#line 2685 "util/configparser.c"
    break;

  case 220:
#line 295 "util/configparser.y"
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
#line 2700 "util/configparser.c"
    break;

  case 229:
#line 312 "util/configparser.y"
    {
		struct config_view* s;
		OUTYY(("\nP(view:)\n")); 
		s = (struct config_view*)calloc(1, sizeof(struct config_view));
		if(s) {
			s->next = cfg_parser->cfg->views;
			if(s->next && !s->next->name)
				yyerror("view without name");
			cfg_parser->cfg->views = s;
		} else 
			yyerror("out of memory");
	}
#line 2717 "util/configparser.c"
    break;

  case 239:
#line 331 "util/configparser.y"
    {
		struct config_auth* s;
		OUTYY(("\nP(auth_zone:)\n")); 
		s = (struct config_auth*)calloc(1, sizeof(struct config_auth));
		if(s) {
			s->next = cfg_parser->cfg->auths;
			cfg_parser->cfg->auths = s;
			/* defaults for auth zone */
			s->for_downstream = 1;
			s->for_upstream = 1;
			s->fallback_enabled = 0;
			s->isrpz = 0;
		} else 
			yyerror("out of memory");
	}
#line 2737 "util/configparser.c"
    break;

  case 250:
#line 355 "util/configparser.y"
    {
		uint8_t* bitlist;
		size_t len = 0;
		OUTYY(("P(server_local_zone_tag:%s)\n", (yyvsp[0].str)));
		bitlist = config_parse_taglist(cfg_parser->cfg, (yyvsp[0].str),
			&len);
		free((yyvsp[0].str));
		if(!bitlist) {
			yyerror("could not parse tags, (define-tag them first)");
		}
		if(bitlist) {
			cfg_parser->cfg->auths->rpz_taglist = bitlist;
			cfg_parser->cfg->auths->rpz_taglistlen = len;

		}
	}
#line 2758 "util/configparser.c"
    break;

  case 251:
#line 374 "util/configparser.y"
    {
		OUTYY(("P(rpz_action_override:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "nxdomain")!=0 && strcmp((yyvsp[0].str), "nodata")!=0 &&
		   strcmp((yyvsp[0].str), "passthru")!=0 && strcmp((yyvsp[0].str), "drop")!=0 &&
		   strcmp((yyvsp[0].str), "cname")!=0 && strcmp((yyvsp[0].str), "disabled")!=0) {
			yyerror("rpz-action-override action: expected nxdomain, "
				"nodata, passthru, drop, cname or disabled");
			free((yyvsp[0].str));
			cfg_parser->cfg->auths->rpz_action_override = NULL;
		}
		else {
			cfg_parser->cfg->auths->rpz_action_override = (yyvsp[0].str);
		}
	}
#line 2777 "util/configparser.c"
    break;

  case 252:
#line 391 "util/configparser.y"
    {
		OUTYY(("P(rpz_cname_override:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->auths->rpz_cname);
		cfg_parser->cfg->auths->rpz_cname = (yyvsp[0].str);
	}
#line 2787 "util/configparser.c"
    break;

  case 253:
#line 399 "util/configparser.y"
    {
		OUTYY(("P(rpz_log:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->rpz_log = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2799 "util/configparser.c"
    break;

  case 254:
#line 409 "util/configparser.y"
    {
		OUTYY(("P(rpz_log_name:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->auths->rpz_log_name);
		cfg_parser->cfg->auths->rpz_log_name = (yyvsp[0].str);
	}
#line 2809 "util/configparser.c"
    break;

  case 255:
#line 417 "util/configparser.y"
    {
		struct config_auth* s;
		OUTYY(("\nP(rpz:)\n")); 
		s = (struct config_auth*)calloc(1, sizeof(struct config_auth));
		if(s) {
			s->next = cfg_parser->cfg->auths;
			cfg_parser->cfg->auths = s;
			/* defaults for RPZ auth zone */
			s->for_downstream = 0;
			s->for_upstream = 0;
			s->fallback_enabled = 0;
			s->isrpz = 1;
		} else 
			yyerror("out of memory");
	}
#line 2829 "util/configparser.c"
    break;

  case 268:
#line 440 "util/configparser.y"
    { 
		OUTYY(("P(server_num_threads:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->num_threads = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2841 "util/configparser.c"
    break;

  case 269:
#line 449 "util/configparser.y"
    { 
		OUTYY(("P(server_verbosity:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->verbosity = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2853 "util/configparser.c"
    break;

  case 270:
#line 458 "util/configparser.y"
    { 
		OUTYY(("P(server_statistics_interval:%s)\n", (yyvsp[0].str))); 
		if(strcmp((yyvsp[0].str), "") == 0 || strcmp((yyvsp[0].str), "0") == 0)
			cfg_parser->cfg->stat_interval = 0;
		else if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->stat_interval = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2867 "util/configparser.c"
    break;

  case 271:
#line 469 "util/configparser.y"
    {
		OUTYY(("P(server_statistics_cumulative:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_cumulative = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2879 "util/configparser.c"
    break;

  case 272:
#line 478 "util/configparser.y"
    {
		OUTYY(("P(server_extended_statistics:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_extended = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2891 "util/configparser.c"
    break;

  case 273:
#line 487 "util/configparser.y"
    {
		OUTYY(("P(server_shm_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->shm_enable = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2903 "util/configparser.c"
    break;

  case 274:
#line 496 "util/configparser.y"
    { 
		OUTYY(("P(server_shm_key:%s)\n", (yyvsp[0].str))); 
		if(strcmp((yyvsp[0].str), "") == 0 || strcmp((yyvsp[0].str), "0") == 0)
			cfg_parser->cfg->shm_key = 0;
		else if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->shm_key = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2917 "util/configparser.c"
    break;

  case 275:
#line 507 "util/configparser.y"
    {
		OUTYY(("P(server_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2929 "util/configparser.c"
    break;

  case 276:
#line 516 "util/configparser.y"
    {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(server_send_client_subnet:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->client_subnet, (yyvsp[0].str)))
			fatal_exit("out of memory adding client-subnet");
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 2944 "util/configparser.c"
    break;

  case 277:
#line 528 "util/configparser.y"
    {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(server_client_subnet_zone:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->client_subnet_zone,
			(yyvsp[0].str)))
			fatal_exit("out of memory adding client-subnet-zone");
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 2960 "util/configparser.c"
    break;

  case 278:
#line 542 "util/configparser.y"
    {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(server_client_subnet_always_forward:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->client_subnet_always_forward =
				(strcmp((yyvsp[0].str), "yes")==0);
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 2978 "util/configparser.c"
    break;

  case 279:
#line 557 "util/configparser.y"
    {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(client_subnet_opcode:%s)\n", (yyvsp[0].str)));
		OUTYY(("P(Deprecated option, ignoring)\n"));
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 2992 "util/configparser.c"
    break;

  case 280:
#line 568 "util/configparser.y"
    {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(max_client_subnet_ipv4:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("IPv4 subnet length expected");
		else if (atoi((yyvsp[0].str)) > 32)
			cfg_parser->cfg->max_client_subnet_ipv4 = 32;
		else if (atoi((yyvsp[0].str)) < 0)
			cfg_parser->cfg->max_client_subnet_ipv4 = 0;
		else cfg_parser->cfg->max_client_subnet_ipv4 = (uint8_t)atoi((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 3012 "util/configparser.c"
    break;

  case 281:
#line 585 "util/configparser.y"
    {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(max_client_subnet_ipv6:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("Ipv6 subnet length expected");
		else if (atoi((yyvsp[0].str)) > 128)
			cfg_parser->cfg->max_client_subnet_ipv6 = 128;
		else if (atoi((yyvsp[0].str)) < 0)
			cfg_parser->cfg->max_client_subnet_ipv6 = 0;
		else cfg_parser->cfg->max_client_subnet_ipv6 = (uint8_t)atoi((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 3032 "util/configparser.c"
    break;

  case 282:
#line 602 "util/configparser.y"
    {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(min_client_subnet_ipv4:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("IPv4 subnet length expected");
		else if (atoi((yyvsp[0].str)) > 32)
			cfg_parser->cfg->min_client_subnet_ipv4 = 32;
		else if (atoi((yyvsp[0].str)) < 0)
			cfg_parser->cfg->min_client_subnet_ipv4 = 0;
		else cfg_parser->cfg->min_client_subnet_ipv4 = (uint8_t)atoi((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 3052 "util/configparser.c"
    break;

  case 283:
#line 619 "util/configparser.y"
    {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(min_client_subnet_ipv6:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("Ipv6 subnet length expected");
		else if (atoi((yyvsp[0].str)) > 128)
			cfg_parser->cfg->min_client_subnet_ipv6 = 128;
		else if (atoi((yyvsp[0].str)) < 0)
			cfg_parser->cfg->min_client_subnet_ipv6 = 0;
		else cfg_parser->cfg->min_client_subnet_ipv6 = (uint8_t)atoi((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 3072 "util/configparser.c"
    break;

  case 284:
#line 636 "util/configparser.y"
    {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(max_ecs_tree_size_ipv4:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("IPv4 ECS tree size expected");
		else if (atoi((yyvsp[0].str)) < 0)
			cfg_parser->cfg->max_ecs_tree_size_ipv4 = 0;
		else cfg_parser->cfg->max_ecs_tree_size_ipv4 = (uint32_t)atoi((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 3090 "util/configparser.c"
    break;

  case 285:
#line 651 "util/configparser.y"
    {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(max_ecs_tree_size_ipv6:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("IPv6 ECS tree size expected");
		else if (atoi((yyvsp[0].str)) < 0)
			cfg_parser->cfg->max_ecs_tree_size_ipv6 = 0;
		else cfg_parser->cfg->max_ecs_tree_size_ipv6 = (uint32_t)atoi((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 3108 "util/configparser.c"
    break;

  case 286:
#line 666 "util/configparser.y"
    {
		OUTYY(("P(server_interface:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->num_ifs == 0)
			cfg_parser->cfg->ifs = calloc(1, sizeof(char*));
		else 	cfg_parser->cfg->ifs = realloc(cfg_parser->cfg->ifs,
				(cfg_parser->cfg->num_ifs+1)*sizeof(char*));
		if(!cfg_parser->cfg->ifs)
			yyerror("out of memory");
		else
			cfg_parser->cfg->ifs[cfg_parser->cfg->num_ifs++] = (yyvsp[0].str);
	}
#line 3124 "util/configparser.c"
    break;

  case 287:
#line 679 "util/configparser.y"
    {
		OUTYY(("P(server_outgoing_interface:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->num_out_ifs == 0)
			cfg_parser->cfg->out_ifs = calloc(1, sizeof(char*));
		else 	cfg_parser->cfg->out_ifs = realloc(
			cfg_parser->cfg->out_ifs, 
			(cfg_parser->cfg->num_out_ifs+1)*sizeof(char*));
		if(!cfg_parser->cfg->out_ifs)
			yyerror("out of memory");
		else
			cfg_parser->cfg->out_ifs[
				cfg_parser->cfg->num_out_ifs++] = (yyvsp[0].str);
	}
#line 3142 "util/configparser.c"
    break;

  case 288:
#line 694 "util/configparser.y"
    {
		OUTYY(("P(server_outgoing_range:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_num_ports = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3154 "util/configparser.c"
    break;

  case 289:
#line 703 "util/configparser.y"
    {
		OUTYY(("P(server_outgoing_port_permit:%s)\n", (yyvsp[0].str)));
		if(!cfg_mark_ports((yyvsp[0].str), 1, 
			cfg_parser->cfg->outgoing_avail_ports, 65536))
			yyerror("port number or range (\"low-high\") expected");
		free((yyvsp[0].str));
	}
#line 3166 "util/configparser.c"
    break;

  case 290:
#line 712 "util/configparser.y"
    {
		OUTYY(("P(server_outgoing_port_avoid:%s)\n", (yyvsp[0].str)));
		if(!cfg_mark_ports((yyvsp[0].str), 0, 
			cfg_parser->cfg->outgoing_avail_ports, 65536))
			yyerror("port number or range (\"low-high\") expected");
		free((yyvsp[0].str));
	}
#line 3178 "util/configparser.c"
    break;

  case 291:
#line 721 "util/configparser.y"
    {
		OUTYY(("P(server_outgoing_num_tcp:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_num_tcp = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3190 "util/configparser.c"
    break;

  case 292:
#line 730 "util/configparser.y"
    {
		OUTYY(("P(server_incoming_num_tcp:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->incoming_num_tcp = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3202 "util/configparser.c"
    break;

  case 293:
#line 739 "util/configparser.y"
    {
		OUTYY(("P(server_interface_automatic:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->if_automatic = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3214 "util/configparser.c"
    break;

  case 294:
#line 748 "util/configparser.y"
    {
		OUTYY(("P(server_do_ip4:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_ip4 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3226 "util/configparser.c"
    break;

  case 295:
#line 757 "util/configparser.y"
    {
		OUTYY(("P(server_do_ip6:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_ip6 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3238 "util/configparser.c"
    break;

  case 296:
#line 766 "util/configparser.y"
    {
		OUTYY(("P(server_do_udp:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_udp = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3250 "util/configparser.c"
    break;

  case 297:
#line 775 "util/configparser.y"
    {
		OUTYY(("P(server_do_tcp:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_tcp = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3262 "util/configparser.c"
    break;

  case 298:
#line 784 "util/configparser.y"
    {
		OUTYY(("P(server_prefer_ip6:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefer_ip6 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3274 "util/configparser.c"
    break;

  case 299:
#line 793 "util/configparser.y"
    {
		OUTYY(("P(server_tcp_mss:%s)\n", (yyvsp[0].str)));
                if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
                        yyerror("number expected");
                else cfg_parser->cfg->tcp_mss = atoi((yyvsp[0].str));
                free((yyvsp[0].str));
	}
#line 3286 "util/configparser.c"
    break;

  case 300:
#line 802 "util/configparser.y"
    {
		OUTYY(("P(server_outgoing_tcp_mss:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_tcp_mss = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3298 "util/configparser.c"
    break;

  case 301:
#line 811 "util/configparser.y"
    {
		OUTYY(("P(server_tcp_idle_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) > 120000)
			cfg_parser->cfg->tcp_idle_timeout = 120000;
		else if (atoi((yyvsp[0].str)) < 1)
			cfg_parser->cfg->tcp_idle_timeout = 1;
		else cfg_parser->cfg->tcp_idle_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3314 "util/configparser.c"
    break;

  case 302:
#line 824 "util/configparser.y"
    {
		OUTYY(("P(server_tcp_keepalive:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_tcp_keepalive = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3326 "util/configparser.c"
    break;

  case 303:
#line 833 "util/configparser.y"
    {
		OUTYY(("P(server_tcp_keepalive_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) > 6553500)
			cfg_parser->cfg->tcp_keepalive_timeout = 6553500;
		else if (atoi((yyvsp[0].str)) < 1)
			cfg_parser->cfg->tcp_keepalive_timeout = 0;
		else cfg_parser->cfg->tcp_keepalive_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3342 "util/configparser.c"
    break;

  case 304:
#line 846 "util/configparser.y"
    {
		OUTYY(("P(server_tcp_upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tcp_upstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3354 "util/configparser.c"
    break;

  case 305:
#line 855 "util/configparser.y"
    {
		OUTYY(("P(server_udp_upstream_without_downstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->udp_upstream_without_downstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3366 "util/configparser.c"
    break;

  case 306:
#line 864 "util/configparser.y"
    {
		OUTYY(("P(server_ssl_upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ssl_upstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3378 "util/configparser.c"
    break;

  case 307:
#line 873 "util/configparser.y"
    {
		OUTYY(("P(server_ssl_service_key:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->ssl_service_key);
		cfg_parser->cfg->ssl_service_key = (yyvsp[0].str);
	}
#line 3388 "util/configparser.c"
    break;

  case 308:
#line 880 "util/configparser.y"
    {
		OUTYY(("P(server_ssl_service_pem:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->ssl_service_pem);
		cfg_parser->cfg->ssl_service_pem = (yyvsp[0].str);
	}
#line 3398 "util/configparser.c"
    break;

  case 309:
#line 887 "util/configparser.y"
    {
		OUTYY(("P(server_ssl_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->ssl_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3410 "util/configparser.c"
    break;

  case 310:
#line 896 "util/configparser.y"
    {
		OUTYY(("P(server_tls_cert_bundle:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->tls_cert_bundle);
		cfg_parser->cfg->tls_cert_bundle = (yyvsp[0].str);
	}
#line 3420 "util/configparser.c"
    break;

  case 311:
#line 903 "util/configparser.y"
    {
		OUTYY(("P(server_tls_win_cert:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tls_win_cert = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3432 "util/configparser.c"
    break;

  case 312:
#line 912 "util/configparser.y"
    {
		OUTYY(("P(server_tls_additional_port:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->tls_additional_port,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3443 "util/configparser.c"
    break;

  case 313:
#line 920 "util/configparser.y"
    {
		OUTYY(("P(server_tls_ciphers:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->tls_ciphers);
		cfg_parser->cfg->tls_ciphers = (yyvsp[0].str);
	}
#line 3453 "util/configparser.c"
    break;

  case 314:
#line 927 "util/configparser.y"
    {
		OUTYY(("P(server_tls_ciphersuites:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->tls_ciphersuites);
		cfg_parser->cfg->tls_ciphersuites = (yyvsp[0].str);
	}
#line 3463 "util/configparser.c"
    break;

  case 315:
#line 934 "util/configparser.y"
    {
		OUTYY(("P(server_tls_session_ticket_keys:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append(&cfg_parser->cfg->tls_session_ticket_keys,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3474 "util/configparser.c"
    break;

  case 316:
#line 942 "util/configparser.y"
    {
		OUTYY(("P(server_use_systemd:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->use_systemd = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3486 "util/configparser.c"
    break;

  case 317:
#line 951 "util/configparser.y"
    {
		OUTYY(("P(server_do_daemonize:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_daemonize = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3498 "util/configparser.c"
    break;

  case 318:
#line 960 "util/configparser.y"
    {
		OUTYY(("P(server_use_syslog:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->use_syslog = (strcmp((yyvsp[0].str), "yes")==0);
#if !defined(HAVE_SYSLOG_H) && !defined(UB_ON_WINDOWS)
		if(strcmp((yyvsp[0].str), "yes") == 0)
			yyerror("no syslog services are available. "
				"(reconfigure and compile to add)");
#endif
		free((yyvsp[0].str));
	}
#line 3515 "util/configparser.c"
    break;

  case 319:
#line 974 "util/configparser.y"
    {
		OUTYY(("P(server_log_time_ascii:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_time_ascii = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3527 "util/configparser.c"
    break;

  case 320:
#line 983 "util/configparser.y"
    {
		OUTYY(("P(server_log_queries:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_queries = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3539 "util/configparser.c"
    break;

  case 321:
#line 992 "util/configparser.y"
    {
  	OUTYY(("P(server_log_replies:%s)\n", (yyvsp[0].str)));
  	if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
  		yyerror("expected yes or no.");
  	else cfg_parser->cfg->log_replies = (strcmp((yyvsp[0].str), "yes")==0);
  	free((yyvsp[0].str));
  }
#line 3551 "util/configparser.c"
    break;

  case 322:
#line 1001 "util/configparser.y"
    {
  	OUTYY(("P(server_log_tag_queryreply:%s)\n", (yyvsp[0].str)));
  	if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
  		yyerror("expected yes or no.");
  	else cfg_parser->cfg->log_tag_queryreply = (strcmp((yyvsp[0].str), "yes")==0);
  	free((yyvsp[0].str));
  }
#line 3563 "util/configparser.c"
    break;

  case 323:
#line 1010 "util/configparser.y"
    {
		OUTYY(("P(server_log_servfail:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_servfail = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3575 "util/configparser.c"
    break;

  case 324:
#line 1019 "util/configparser.y"
    {
  	OUTYY(("P(server_log_local_actions:%s)\n", (yyvsp[0].str)));
  	if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
  		yyerror("expected yes or no.");
  	else cfg_parser->cfg->log_local_actions = (strcmp((yyvsp[0].str), "yes")==0);
  	free((yyvsp[0].str));
  }
#line 3587 "util/configparser.c"
    break;

  case 325:
#line 1028 "util/configparser.y"
    {
		OUTYY(("P(server_chroot:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->chrootdir);
		cfg_parser->cfg->chrootdir = (yyvsp[0].str);
	}
#line 3597 "util/configparser.c"
    break;

  case 326:
#line 1035 "util/configparser.y"
    {
		OUTYY(("P(server_username:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->username);
		cfg_parser->cfg->username = (yyvsp[0].str);
	}
#line 3607 "util/configparser.c"
    break;

  case 327:
#line 1042 "util/configparser.y"
    {
		OUTYY(("P(server_directory:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->directory);
		cfg_parser->cfg->directory = (yyvsp[0].str);
		/* change there right away for includes relative to this */
		if((yyvsp[0].str)[0]) {
			char* d;
#ifdef UB_ON_WINDOWS
			w_config_adjust_directory(cfg_parser->cfg);
#endif
			d = cfg_parser->cfg->directory;
			/* adjust directory if we have already chroot,
			 * like, we reread after sighup */
			if(cfg_parser->chroot && cfg_parser->chroot[0] &&
				strncmp(d, cfg_parser->chroot, strlen(
				cfg_parser->chroot)) == 0)
				d += strlen(cfg_parser->chroot);
			if(d[0]) {
			    if(chdir(d))
				log_err("cannot chdir to directory: %s (%s)",
					d, strerror(errno));
			}
		}
	}
#line 3636 "util/configparser.c"
    break;

  case 328:
#line 1068 "util/configparser.y"
    {
		OUTYY(("P(server_logfile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->logfile);
		cfg_parser->cfg->logfile = (yyvsp[0].str);
		cfg_parser->cfg->use_syslog = 0;
	}
#line 3647 "util/configparser.c"
    break;

  case 329:
#line 1076 "util/configparser.y"
    {
		OUTYY(("P(server_pidfile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->pidfile);
		cfg_parser->cfg->pidfile = (yyvsp[0].str);
	}
#line 3657 "util/configparser.c"
    break;

  case 330:
#line 1083 "util/configparser.y"
    {
		OUTYY(("P(server_root_hints:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->root_hints, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3667 "util/configparser.c"
    break;

  case 331:
#line 1090 "util/configparser.y"
    {
		OUTYY(("P(server_dlv_anchor_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dlv_anchor_file);
		cfg_parser->cfg->dlv_anchor_file = (yyvsp[0].str);
	}
#line 3677 "util/configparser.c"
    break;

  case 332:
#line 1097 "util/configparser.y"
    {
		OUTYY(("P(server_dlv_anchor:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dlv_anchor_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3687 "util/configparser.c"
    break;

  case 333:
#line 1104 "util/configparser.y"
    {
		OUTYY(("P(server_auto_trust_anchor_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			auto_trust_anchor_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3698 "util/configparser.c"
    break;

  case 334:
#line 1112 "util/configparser.y"
    {
		OUTYY(("P(server_trust_anchor_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			trust_anchor_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3709 "util/configparser.c"
    break;

  case 335:
#line 1120 "util/configparser.y"
    {
		OUTYY(("P(server_trusted_keys_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			trusted_keys_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3720 "util/configparser.c"
    break;

  case 336:
#line 1128 "util/configparser.y"
    {
		OUTYY(("P(server_trust_anchor:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->trust_anchor_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3730 "util/configparser.c"
    break;

  case 337:
#line 1135 "util/configparser.y"
    {
		OUTYY(("P(server_trust_anchor_signaling:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->trust_anchor_signaling =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3744 "util/configparser.c"
    break;

  case 338:
#line 1146 "util/configparser.y"
    {
		OUTYY(("P(server_root_key_sentinel:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->root_key_sentinel =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3758 "util/configparser.c"
    break;

  case 339:
#line 1157 "util/configparser.y"
    {
		OUTYY(("P(server_domain_insecure:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->domain_insecure, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3768 "util/configparser.c"
    break;

  case 340:
#line 1164 "util/configparser.y"
    {
		OUTYY(("P(server_hide_identity:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_identity = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3780 "util/configparser.c"
    break;

  case 341:
#line 1173 "util/configparser.y"
    {
		OUTYY(("P(server_hide_version:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_version = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3792 "util/configparser.c"
    break;

  case 342:
#line 1182 "util/configparser.y"
    {
		OUTYY(("P(server_hide_trustanchor:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_trustanchor = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3804 "util/configparser.c"
    break;

  case 343:
#line 1191 "util/configparser.y"
    {
		OUTYY(("P(server_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->identity);
		cfg_parser->cfg->identity = (yyvsp[0].str);
	}
#line 3814 "util/configparser.c"
    break;

  case 344:
#line 1198 "util/configparser.y"
    {
		OUTYY(("P(server_version:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->version);
		cfg_parser->cfg->version = (yyvsp[0].str);
	}
#line 3824 "util/configparser.c"
    break;

  case 345:
#line 1205 "util/configparser.y"
    {
		OUTYY(("P(server_so_rcvbuf:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->so_rcvbuf))
			yyerror("buffer size expected");
		free((yyvsp[0].str));
	}
#line 3835 "util/configparser.c"
    break;

  case 346:
#line 1213 "util/configparser.y"
    {
		OUTYY(("P(server_so_sndbuf:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->so_sndbuf))
			yyerror("buffer size expected");
		free((yyvsp[0].str));
	}
#line 3846 "util/configparser.c"
    break;

  case 347:
#line 1221 "util/configparser.y"
    {
        OUTYY(("P(server_so_reuseport:%s)\n", (yyvsp[0].str)));
        if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
            yyerror("expected yes or no.");
        else cfg_parser->cfg->so_reuseport =
            (strcmp((yyvsp[0].str), "yes")==0);
        free((yyvsp[0].str));
    }
#line 3859 "util/configparser.c"
    break;

  case 348:
#line 1231 "util/configparser.y"
    {
        OUTYY(("P(server_ip_transparent:%s)\n", (yyvsp[0].str)));
        if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
            yyerror("expected yes or no.");
        else cfg_parser->cfg->ip_transparent =
            (strcmp((yyvsp[0].str), "yes")==0);
        free((yyvsp[0].str));
    }
#line 3872 "util/configparser.c"
    break;

  case 349:
#line 1241 "util/configparser.y"
    {
        OUTYY(("P(server_ip_freebind:%s)\n", (yyvsp[0].str)));
        if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
            yyerror("expected yes or no.");
        else cfg_parser->cfg->ip_freebind =
            (strcmp((yyvsp[0].str), "yes")==0);
        free((yyvsp[0].str));
    }
#line 3885 "util/configparser.c"
    break;

  case 350:
#line 1251 "util/configparser.y"
    {
		OUTYY(("P(server_stream_wait_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->stream_wait_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 3896 "util/configparser.c"
    break;

  case 351:
#line 1259 "util/configparser.y"
    {
		OUTYY(("P(server_edns_buffer_size:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 12)
			yyerror("edns buffer size too small");
		else if (atoi((yyvsp[0].str)) > 65535)
			cfg_parser->cfg->edns_buffer_size = 65535;
		else cfg_parser->cfg->edns_buffer_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3912 "util/configparser.c"
    break;

  case 352:
#line 1272 "util/configparser.y"
    {
		OUTYY(("P(server_msg_buffer_size:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 4096)
			yyerror("message buffer size too small (use 4096)");
		else cfg_parser->cfg->msg_buffer_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3926 "util/configparser.c"
    break;

  case 353:
#line 1283 "util/configparser.y"
    {
		OUTYY(("P(server_msg_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->msg_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 3937 "util/configparser.c"
    break;

  case 354:
#line 1291 "util/configparser.y"
    {
		OUTYY(("P(server_msg_cache_slabs:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else {
			cfg_parser->cfg->msg_cache_slabs = atoi((yyvsp[0].str));
			if(!is_pow2(cfg_parser->cfg->msg_cache_slabs))
				yyerror("must be a power of 2");
		}
		free((yyvsp[0].str));
	}
#line 3953 "util/configparser.c"
    break;

  case 355:
#line 1304 "util/configparser.y"
    {
		OUTYY(("P(server_num_queries_per_thread:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->num_queries_per_thread = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3965 "util/configparser.c"
    break;

  case 356:
#line 1313 "util/configparser.y"
    {
		OUTYY(("P(server_jostle_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->jostle_time = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3977 "util/configparser.c"
    break;

  case 357:
#line 1322 "util/configparser.y"
    {
		OUTYY(("P(server_delay_close:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->delay_close = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3989 "util/configparser.c"
    break;

  case 358:
#line 1331 "util/configparser.y"
    {
		OUTYY(("P(server_unblock_lan_zones:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->unblock_lan_zones = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4002 "util/configparser.c"
    break;

  case 359:
#line 1341 "util/configparser.y"
    {
		OUTYY(("P(server_insecure_lan_zones:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->insecure_lan_zones = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4015 "util/configparser.c"
    break;

  case 360:
#line 1351 "util/configparser.y"
    {
		OUTYY(("P(server_rrset_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->rrset_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4026 "util/configparser.c"
    break;

  case 361:
#line 1359 "util/configparser.y"
    {
		OUTYY(("P(server_rrset_cache_slabs:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else {
			cfg_parser->cfg->rrset_cache_slabs = atoi((yyvsp[0].str));
			if(!is_pow2(cfg_parser->cfg->rrset_cache_slabs))
				yyerror("must be a power of 2");
		}
		free((yyvsp[0].str));
	}
#line 4042 "util/configparser.c"
    break;

  case 362:
#line 1372 "util/configparser.y"
    {
		OUTYY(("P(server_infra_host_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->host_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4054 "util/configparser.c"
    break;

  case 363:
#line 1381 "util/configparser.y"
    {
		OUTYY(("P(server_infra_lame_ttl:%s)\n", (yyvsp[0].str)));
		verbose(VERB_DETAIL, "ignored infra-lame-ttl: %s (option "
			"removed, use infra-host-ttl)", (yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4065 "util/configparser.c"
    break;

  case 364:
#line 1389 "util/configparser.y"
    {
		OUTYY(("P(server_infra_cache_numhosts:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_numhosts = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4077 "util/configparser.c"
    break;

  case 365:
#line 1398 "util/configparser.y"
    {
		OUTYY(("P(server_infra_cache_lame_size:%s)\n", (yyvsp[0].str)));
		verbose(VERB_DETAIL, "ignored infra-cache-lame-size: %s "
			"(option removed, use infra-cache-numhosts)", (yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4088 "util/configparser.c"
    break;

  case 366:
#line 1406 "util/configparser.y"
    {
		OUTYY(("P(server_infra_cache_slabs:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else {
			cfg_parser->cfg->infra_cache_slabs = atoi((yyvsp[0].str));
			if(!is_pow2(cfg_parser->cfg->infra_cache_slabs))
				yyerror("must be a power of 2");
		}
		free((yyvsp[0].str));
	}
#line 4104 "util/configparser.c"
    break;

  case 367:
#line 1419 "util/configparser.y"
    {
		OUTYY(("P(server_infra_cache_min_rtt:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_min_rtt = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4116 "util/configparser.c"
    break;

  case 368:
#line 1428 "util/configparser.y"
    {
		OUTYY(("P(server_target_fetch_policy:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->target_fetch_policy);
		cfg_parser->cfg->target_fetch_policy = (yyvsp[0].str);
	}
#line 4126 "util/configparser.c"
    break;

  case 369:
#line 1435 "util/configparser.y"
    {
		OUTYY(("P(server_harden_short_bufsize:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_short_bufsize = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4139 "util/configparser.c"
    break;

  case 370:
#line 1445 "util/configparser.y"
    {
		OUTYY(("P(server_harden_large_queries:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_large_queries = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4152 "util/configparser.c"
    break;

  case 371:
#line 1455 "util/configparser.y"
    {
		OUTYY(("P(server_harden_glue:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_glue = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4165 "util/configparser.c"
    break;

  case 372:
#line 1465 "util/configparser.y"
    {
		OUTYY(("P(server_harden_dnssec_stripped:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_dnssec_stripped = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4178 "util/configparser.c"
    break;

  case 373:
#line 1475 "util/configparser.y"
    {
		OUTYY(("P(server_harden_below_nxdomain:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_below_nxdomain = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4191 "util/configparser.c"
    break;

  case 374:
#line 1485 "util/configparser.y"
    {
		OUTYY(("P(server_harden_referral_path:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_referral_path = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4204 "util/configparser.c"
    break;

  case 375:
#line 1495 "util/configparser.y"
    {
		OUTYY(("P(server_harden_algo_downgrade:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_algo_downgrade = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4217 "util/configparser.c"
    break;

  case 376:
#line 1505 "util/configparser.y"
    {
		OUTYY(("P(server_use_caps_for_id:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->use_caps_bits_for_id = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4230 "util/configparser.c"
    break;

  case 377:
#line 1515 "util/configparser.y"
    {
		OUTYY(("P(server_caps_whitelist:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->caps_whitelist, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4240 "util/configparser.c"
    break;

  case 378:
#line 1522 "util/configparser.y"
    {
		OUTYY(("P(server_private_address:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->private_address, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4250 "util/configparser.c"
    break;

  case 379:
#line 1529 "util/configparser.y"
    {
		OUTYY(("P(server_private_domain:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->private_domain, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4260 "util/configparser.c"
    break;

  case 380:
#line 1536 "util/configparser.y"
    {
		OUTYY(("P(server_prefetch:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefetch = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4272 "util/configparser.c"
    break;

  case 381:
#line 1545 "util/configparser.y"
    {
		OUTYY(("P(server_prefetch_key:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefetch_key = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4284 "util/configparser.c"
    break;

  case 382:
#line 1554 "util/configparser.y"
    {
		OUTYY(("P(server_deny_any:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->deny_any = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4296 "util/configparser.c"
    break;

  case 383:
#line 1563 "util/configparser.y"
    {
		OUTYY(("P(server_unwanted_reply_threshold:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->unwanted_threshold = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4308 "util/configparser.c"
    break;

  case 384:
#line 1572 "util/configparser.y"
    {
		OUTYY(("P(server_do_not_query_address:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->donotqueryaddrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4318 "util/configparser.c"
    break;

  case 385:
#line 1579 "util/configparser.y"
    {
		OUTYY(("P(server_do_not_query_localhost:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->donotquery_localhost = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4331 "util/configparser.c"
    break;

  case 386:
#line 1589 "util/configparser.y"
    {
		OUTYY(("P(server_access_control:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "deny")!=0 && strcmp((yyvsp[0].str), "refuse")!=0 &&
			strcmp((yyvsp[0].str), "deny_non_local")!=0 &&
			strcmp((yyvsp[0].str), "refuse_non_local")!=0 &&
			strcmp((yyvsp[0].str), "allow_setrd")!=0 && 
			strcmp((yyvsp[0].str), "allow")!=0 && 
			strcmp((yyvsp[0].str), "allow_snoop")!=0) {
			yyerror("expected deny, refuse, deny_non_local, "
				"refuse_non_local, allow, allow_setrd or "
				"allow_snoop in access control action");
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->acls, (yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding acl");
		}
	}
#line 4354 "util/configparser.c"
    break;

  case 387:
#line 1609 "util/configparser.y"
    {
		OUTYY(("P(server_module_conf:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->module_conf);
		cfg_parser->cfg->module_conf = (yyvsp[0].str);
	}
#line 4364 "util/configparser.c"
    break;

  case 388:
#line 1616 "util/configparser.y"
    {
		OUTYY(("P(server_val_override_date:%s)\n", (yyvsp[0].str)));
		if(*(yyvsp[0].str) == '\0' || strcmp((yyvsp[0].str), "0") == 0) {
			cfg_parser->cfg->val_date_override = 0;
		} else if(strlen((yyvsp[0].str)) == 14) {
			cfg_parser->cfg->val_date_override = 
				cfg_convert_timeval((yyvsp[0].str));
			if(!cfg_parser->cfg->val_date_override)
				yyerror("bad date/time specification");
		} else {
			if(atoi((yyvsp[0].str)) == 0)
				yyerror("number expected");
			cfg_parser->cfg->val_date_override = atoi((yyvsp[0].str));
		}
		free((yyvsp[0].str));
	}
#line 4385 "util/configparser.c"
    break;

  case 389:
#line 1634 "util/configparser.y"
    {
		OUTYY(("P(server_val_sig_skew_min:%s)\n", (yyvsp[0].str)));
		if(*(yyvsp[0].str) == '\0' || strcmp((yyvsp[0].str), "0") == 0) {
			cfg_parser->cfg->val_sig_skew_min = 0;
		} else {
			cfg_parser->cfg->val_sig_skew_min = atoi((yyvsp[0].str));
			if(!cfg_parser->cfg->val_sig_skew_min)
				yyerror("number expected");
		}
		free((yyvsp[0].str));
	}
#line 4401 "util/configparser.c"
    break;

  case 390:
#line 1647 "util/configparser.y"
    {
		OUTYY(("P(server_val_sig_skew_max:%s)\n", (yyvsp[0].str)));
		if(*(yyvsp[0].str) == '\0' || strcmp((yyvsp[0].str), "0") == 0) {
			cfg_parser->cfg->val_sig_skew_max = 0;
		} else {
			cfg_parser->cfg->val_sig_skew_max = atoi((yyvsp[0].str));
			if(!cfg_parser->cfg->val_sig_skew_max)
				yyerror("number expected");
		}
		free((yyvsp[0].str));
	}
#line 4417 "util/configparser.c"
    break;

  case 391:
#line 1660 "util/configparser.y"
    {
		OUTYY(("P(server_cache_max_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4429 "util/configparser.c"
    break;

  case 392:
#line 1669 "util/configparser.y"
    {
		OUTYY(("P(server_cache_max_negative_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_negative_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4441 "util/configparser.c"
    break;

  case 393:
#line 1678 "util/configparser.y"
    {
		OUTYY(("P(server_cache_min_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->min_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4453 "util/configparser.c"
    break;

  case 394:
#line 1687 "util/configparser.y"
    {
		OUTYY(("P(server_bogus_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->bogus_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4465 "util/configparser.c"
    break;

  case 395:
#line 1696 "util/configparser.y"
    {
		OUTYY(("P(server_val_clean_additional:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->val_clean_additional = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4478 "util/configparser.c"
    break;

  case 396:
#line 1706 "util/configparser.y"
    {
		OUTYY(("P(server_val_permissive_mode:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->val_permissive_mode = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4491 "util/configparser.c"
    break;

  case 397:
#line 1716 "util/configparser.y"
    {
		OUTYY(("P(server_aggressive_nsec:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->aggressive_nsec =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4505 "util/configparser.c"
    break;

  case 398:
#line 1727 "util/configparser.y"
    {
		OUTYY(("P(server_ignore_cd_flag:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ignore_cd = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4517 "util/configparser.c"
    break;

  case 399:
#line 1736 "util/configparser.y"
    {
		OUTYY(("P(server_serve_expired:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->serve_expired = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4529 "util/configparser.c"
    break;

  case 400:
#line 1745 "util/configparser.y"
    {
		OUTYY(("P(server_serve_expired_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->serve_expired_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4541 "util/configparser.c"
    break;

  case 401:
#line 1754 "util/configparser.y"
    {
		OUTYY(("P(server_serve_expired_ttl_reset:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->serve_expired_ttl_reset = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4553 "util/configparser.c"
    break;

  case 402:
#line 1763 "util/configparser.y"
    {
		OUTYY(("P(server_serve_expired_reply_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->serve_expired_reply_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4565 "util/configparser.c"
    break;

  case 403:
#line 1772 "util/configparser.y"
    {
		OUTYY(("P(server_serve_expired_client_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->serve_expired_client_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4577 "util/configparser.c"
    break;

  case 404:
#line 1781 "util/configparser.y"
    {
		OUTYY(("P(server_fake_dsa:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
#if defined(HAVE_SSL) || defined(HAVE_NETTLE)
		else fake_dsa = (strcmp((yyvsp[0].str), "yes")==0);
		if(fake_dsa)
			log_warn("test option fake_dsa is enabled");
#endif
		free((yyvsp[0].str));
	}
#line 4593 "util/configparser.c"
    break;

  case 405:
#line 1794 "util/configparser.y"
    {
		OUTYY(("P(server_fake_sha1:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
#if defined(HAVE_SSL) || defined(HAVE_NETTLE)
		else fake_sha1 = (strcmp((yyvsp[0].str), "yes")==0);
		if(fake_sha1)
			log_warn("test option fake_sha1 is enabled");
#endif
		free((yyvsp[0].str));
	}
#line 4609 "util/configparser.c"
    break;

  case 406:
#line 1807 "util/configparser.y"
    {
		OUTYY(("P(server_val_log_level:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->val_log_level = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4621 "util/configparser.c"
    break;

  case 407:
#line 1816 "util/configparser.y"
    {
		OUTYY(("P(server_val_nsec3_keysize_iterations:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->val_nsec3_key_iterations);
		cfg_parser->cfg->val_nsec3_key_iterations = (yyvsp[0].str);
	}
#line 4631 "util/configparser.c"
    break;

  case 408:
#line 1823 "util/configparser.y"
    {
		OUTYY(("P(server_add_holddown:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->add_holddown = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4643 "util/configparser.c"
    break;

  case 409:
#line 1832 "util/configparser.y"
    {
		OUTYY(("P(server_del_holddown:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->del_holddown = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4655 "util/configparser.c"
    break;

  case 410:
#line 1841 "util/configparser.y"
    {
		OUTYY(("P(server_keep_missing:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->keep_missing = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4667 "util/configparser.c"
    break;

  case 411:
#line 1850 "util/configparser.y"
    {
		OUTYY(("P(server_permit_small_holddown:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->permit_small_holddown =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4680 "util/configparser.c"
    break;

  case 412:
#line 1859 "util/configparser.y"
    {
		OUTYY(("P(server_key_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->key_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4691 "util/configparser.c"
    break;

  case 413:
#line 1867 "util/configparser.y"
    {
		OUTYY(("P(server_key_cache_slabs:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else {
			cfg_parser->cfg->key_cache_slabs = atoi((yyvsp[0].str));
			if(!is_pow2(cfg_parser->cfg->key_cache_slabs))
				yyerror("must be a power of 2");
		}
		free((yyvsp[0].str));
	}
#line 4707 "util/configparser.c"
    break;

  case 414:
#line 1880 "util/configparser.y"
    {
		OUTYY(("P(server_neg_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->neg_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4718 "util/configparser.c"
    break;

  case 415:
#line 1888 "util/configparser.y"
    {
		OUTYY(("P(server_local_zone:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "static")!=0 && strcmp((yyvsp[0].str), "deny")!=0 &&
		   strcmp((yyvsp[0].str), "refuse")!=0 && strcmp((yyvsp[0].str), "redirect")!=0 &&
		   strcmp((yyvsp[0].str), "transparent")!=0 && strcmp((yyvsp[0].str), "nodefault")!=0
		   && strcmp((yyvsp[0].str), "typetransparent")!=0
		   && strcmp((yyvsp[0].str), "always_transparent")!=0
		   && strcmp((yyvsp[0].str), "always_refuse")!=0
		   && strcmp((yyvsp[0].str), "always_nxdomain")!=0
		   && strcmp((yyvsp[0].str), "noview")!=0
		   && strcmp((yyvsp[0].str), "inform")!=0 && strcmp((yyvsp[0].str), "inform_deny")!=0
		   && strcmp((yyvsp[0].str), "inform_redirect") != 0
			 && strcmp((yyvsp[0].str), "ipset") != 0) {
			yyerror("local-zone type: expected static, deny, "
				"refuse, redirect, transparent, "
				"typetransparent, inform, inform_deny, "
				"inform_redirect, always_transparent, "
				"always_refuse, always_nxdomain, noview "
				", nodefault or ipset");
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		} else if(strcmp((yyvsp[0].str), "nodefault")==0) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->
				local_zones_nodefault, (yyvsp[-1].str)))
				fatal_exit("out of memory adding local-zone");
			free((yyvsp[0].str));
#ifdef USE_IPSET
		} else if(strcmp((yyvsp[0].str), "ipset")==0) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->
				local_zones_ipset, (yyvsp[-1].str)))
				fatal_exit("out of memory adding local-zone");
			free((yyvsp[0].str));
#endif
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->local_zones, 
				(yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding local-zone");
		}
	}
#line 4762 "util/configparser.c"
    break;

  case 416:
#line 1929 "util/configparser.y"
    {
		OUTYY(("P(server_local_data:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->local_data, (yyvsp[0].str)))
			fatal_exit("out of memory adding local-data");
	}
#line 4772 "util/configparser.c"
    break;

  case 417:
#line 1936 "util/configparser.y"
    {
		char* ptr;
		OUTYY(("P(server_local_data_ptr:%s)\n", (yyvsp[0].str)));
		ptr = cfg_ptr_reverse((yyvsp[0].str));
		free((yyvsp[0].str));
		if(ptr) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->
				local_data, ptr))
				fatal_exit("out of memory adding local-data");
		} else {
			yyerror("local-data-ptr could not be reversed");
		}
	}
#line 4790 "util/configparser.c"
    break;

  case 418:
#line 1951 "util/configparser.y"
    {
		OUTYY(("P(server_minimal_responses:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->minimal_responses =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4803 "util/configparser.c"
    break;

  case 419:
#line 1961 "util/configparser.y"
    {
		OUTYY(("P(server_rrset_roundrobin:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->rrset_roundrobin =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4816 "util/configparser.c"
    break;

  case 420:
#line 1971 "util/configparser.y"
    {
		OUTYY(("P(server_unknown_server_time_limit:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->unknown_server_time_limit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4826 "util/configparser.c"
    break;

  case 421:
#line 1978 "util/configparser.y"
    {
		OUTYY(("P(server_max_udp_size:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->max_udp_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4836 "util/configparser.c"
    break;

  case 422:
#line 1985 "util/configparser.y"
    {
		OUTYY(("P(dns64_prefix:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dns64_prefix);
		cfg_parser->cfg->dns64_prefix = (yyvsp[0].str);
	}
#line 4846 "util/configparser.c"
    break;

  case 423:
#line 1992 "util/configparser.y"
    {
		OUTYY(("P(server_dns64_synthall:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dns64_synthall = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4858 "util/configparser.c"
    break;

  case 424:
#line 2001 "util/configparser.y"
    {
		OUTYY(("P(dns64_ignore_aaaa:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dns64_ignore_aaaa,
			(yyvsp[0].str)))
			fatal_exit("out of memory adding dns64-ignore-aaaa");
	}
#line 4869 "util/configparser.c"
    break;

  case 425:
#line 2009 "util/configparser.y"
    {
		char* p, *s = (yyvsp[0].str);
		OUTYY(("P(server_define_tag:%s)\n", (yyvsp[0].str)));
		while((p=strsep(&s, " \t\n")) != NULL) {
			if(*p) {
				if(!config_add_tag(cfg_parser->cfg, p))
					yyerror("could not define-tag, "
						"out of memory");
			}
		}
		free((yyvsp[0].str));
	}
#line 4886 "util/configparser.c"
    break;

  case 426:
#line 2023 "util/configparser.y"
    {
		size_t len = 0;
		uint8_t* bitlist = config_parse_taglist(cfg_parser->cfg, (yyvsp[0].str),
			&len);
		free((yyvsp[0].str));
		OUTYY(("P(server_local_zone_tag:%s)\n", (yyvsp[-1].str)));
		if(!bitlist) {
			yyerror("could not parse tags, (define-tag them first)");
			free((yyvsp[-1].str));
		}
		if(bitlist) {
			if(!cfg_strbytelist_insert(
				&cfg_parser->cfg->local_zone_tags,
				(yyvsp[-1].str), bitlist, len)) {
				yyerror("out of memory");
				free((yyvsp[-1].str));
			}
		}
	}
#line 4910 "util/configparser.c"
    break;

  case 427:
#line 2044 "util/configparser.y"
    {
		size_t len = 0;
		uint8_t* bitlist = config_parse_taglist(cfg_parser->cfg, (yyvsp[0].str),
			&len);
		free((yyvsp[0].str));
		OUTYY(("P(server_access_control_tag:%s)\n", (yyvsp[-1].str)));
		if(!bitlist) {
			yyerror("could not parse tags, (define-tag them first)");
			free((yyvsp[-1].str));
		}
		if(bitlist) {
			if(!cfg_strbytelist_insert(
				&cfg_parser->cfg->acl_tags,
				(yyvsp[-1].str), bitlist, len)) {
				yyerror("out of memory");
				free((yyvsp[-1].str));
			}
		}
	}
#line 4934 "util/configparser.c"
    break;

  case 428:
#line 2065 "util/configparser.y"
    {
		OUTYY(("P(server_access_control_tag_action:%s %s %s)\n", (yyvsp[-2].str), (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str3list_insert(&cfg_parser->cfg->acl_tag_actions,
			(yyvsp[-2].str), (yyvsp[-1].str), (yyvsp[0].str))) {
			yyerror("out of memory");
			free((yyvsp[-2].str));
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		}
	}
#line 4949 "util/configparser.c"
    break;

  case 429:
#line 2077 "util/configparser.y"
    {
		OUTYY(("P(server_access_control_tag_data:%s %s %s)\n", (yyvsp[-2].str), (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str3list_insert(&cfg_parser->cfg->acl_tag_datas,
			(yyvsp[-2].str), (yyvsp[-1].str), (yyvsp[0].str))) {
			yyerror("out of memory");
			free((yyvsp[-2].str));
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		}
	}
#line 4964 "util/configparser.c"
    break;

  case 430:
#line 2089 "util/configparser.y"
    {
		OUTYY(("P(server_local_zone_override:%s %s %s)\n", (yyvsp[-2].str), (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str3list_insert(&cfg_parser->cfg->local_zone_overrides,
			(yyvsp[-2].str), (yyvsp[-1].str), (yyvsp[0].str))) {
			yyerror("out of memory");
			free((yyvsp[-2].str));
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		}
	}
#line 4979 "util/configparser.c"
    break;

  case 431:
#line 2101 "util/configparser.y"
    {
		OUTYY(("P(server_access_control_view:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str2list_insert(&cfg_parser->cfg->acl_view,
			(yyvsp[-1].str), (yyvsp[0].str))) {
			yyerror("out of memory");
		}
	}
#line 4991 "util/configparser.c"
    break;

  case 432:
#line 2110 "util/configparser.y"
    {
		size_t len = 0;
		uint8_t* bitlist = config_parse_taglist(cfg_parser->cfg, (yyvsp[0].str),
			&len);
		free((yyvsp[0].str));
		OUTYY(("P(response_ip_tag:%s)\n", (yyvsp[-1].str)));
		if(!bitlist) {
			yyerror("could not parse tags, (define-tag them first)");
			free((yyvsp[-1].str));
		}
		if(bitlist) {
			if(!cfg_strbytelist_insert(
				&cfg_parser->cfg->respip_tags,
				(yyvsp[-1].str), bitlist, len)) {
				yyerror("out of memory");
				free((yyvsp[-1].str));
			}
		}
	}
#line 5015 "util/configparser.c"
    break;

  case 433:
#line 2131 "util/configparser.y"
    { 
		OUTYY(("P(server_ip_ratelimit:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ip_ratelimit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5027 "util/configparser.c"
    break;

  case 434:
#line 2141 "util/configparser.y"
    { 
		OUTYY(("P(server_ratelimit:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ratelimit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5039 "util/configparser.c"
    break;

  case 435:
#line 2150 "util/configparser.y"
    {
  	OUTYY(("P(server_ip_ratelimit_size:%s)\n", (yyvsp[0].str)));
  	if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->ip_ratelimit_size))
  		yyerror("memory size expected");
  	free((yyvsp[0].str));
  }
#line 5050 "util/configparser.c"
    break;

  case 436:
#line 2158 "util/configparser.y"
    {
		OUTYY(("P(server_ratelimit_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->ratelimit_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 5061 "util/configparser.c"
    break;

  case 437:
#line 2166 "util/configparser.y"
    {
  	OUTYY(("P(server_ip_ratelimit_slabs:%s)\n", (yyvsp[0].str)));
  	if(atoi((yyvsp[0].str)) == 0)
  		yyerror("number expected");
  	else {
  		cfg_parser->cfg->ip_ratelimit_slabs = atoi((yyvsp[0].str));
  		if(!is_pow2(cfg_parser->cfg->ip_ratelimit_slabs))
  			yyerror("must be a power of 2");
  	}
  	free((yyvsp[0].str));
  }
#line 5077 "util/configparser.c"
    break;

  case 438:
#line 2179 "util/configparser.y"
    {
		OUTYY(("P(server_ratelimit_slabs:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else {
			cfg_parser->cfg->ratelimit_slabs = atoi((yyvsp[0].str));
			if(!is_pow2(cfg_parser->cfg->ratelimit_slabs))
				yyerror("must be a power of 2");
		}
		free((yyvsp[0].str));
	}
#line 5093 "util/configparser.c"
    break;

  case 439:
#line 2192 "util/configparser.y"
    {
		OUTYY(("P(server_ratelimit_for_domain:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0) {
			yyerror("number expected");
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->
				ratelimit_for_domain, (yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding "
					"ratelimit-for-domain");
		}
	}
#line 5111 "util/configparser.c"
    break;

  case 440:
#line 2207 "util/configparser.y"
    {
		OUTYY(("P(server_ratelimit_below_domain:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0) {
			yyerror("number expected");
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->
				ratelimit_below_domain, (yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding "
					"ratelimit-below-domain");
		}
	}
#line 5129 "util/configparser.c"
    break;

  case 441:
#line 2222 "util/configparser.y"
    { 
  	OUTYY(("P(server_ip_ratelimit_factor:%s)\n", (yyvsp[0].str))); 
  	if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
  		yyerror("number expected");
  	else cfg_parser->cfg->ip_ratelimit_factor = atoi((yyvsp[0].str));
  	free((yyvsp[0].str));
	}
#line 5141 "util/configparser.c"
    break;

  case 442:
#line 2231 "util/configparser.y"
    { 
		OUTYY(("P(server_ratelimit_factor:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ratelimit_factor = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5153 "util/configparser.c"
    break;

  case 443:
#line 2240 "util/configparser.y"
    { 
		OUTYY(("P(low-rtt option is deprecated, use fast-server-num instead)\n"));
		free((yyvsp[0].str));
	}
#line 5162 "util/configparser.c"
    break;

  case 444:
#line 2246 "util/configparser.y"
    { 
		OUTYY(("P(server_fast_server_num:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) <= 0)
			yyerror("number expected");
		else cfg_parser->cfg->fast_server_num = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5174 "util/configparser.c"
    break;

  case 445:
#line 2255 "util/configparser.y"
    { 
		OUTYY(("P(server_fast_server_permil:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->fast_server_permil = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5186 "util/configparser.c"
    break;

  case 446:
#line 2264 "util/configparser.y"
    {
		OUTYY(("P(server_qname_minimisation:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->qname_minimisation = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5199 "util/configparser.c"
    break;

  case 447:
#line 2274 "util/configparser.y"
    {
		OUTYY(("P(server_qname_minimisation_strict:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->qname_minimisation_strict = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5212 "util/configparser.c"
    break;

  case 448:
#line 2284 "util/configparser.y"
    {
	#ifdef USE_IPSECMOD
		OUTYY(("P(server_ipsecmod_enabled:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ipsecmod_enabled = (strcmp((yyvsp[0].str), "yes")==0);
	#else
		OUTYY(("P(Compiled without IPsec module, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 5228 "util/configparser.c"
    break;

  case 449:
#line 2297 "util/configparser.y"
    {
	#ifdef USE_IPSECMOD
		OUTYY(("P(server_ipsecmod_ignore_bogus:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ipsecmod_ignore_bogus = (strcmp((yyvsp[0].str), "yes")==0);
	#else
		OUTYY(("P(Compiled without IPsec module, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 5244 "util/configparser.c"
    break;

  case 450:
#line 2310 "util/configparser.y"
    {
	#ifdef USE_IPSECMOD
		OUTYY(("P(server_ipsecmod_hook:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->ipsecmod_hook);
		cfg_parser->cfg->ipsecmod_hook = (yyvsp[0].str);
	#else
		OUTYY(("P(Compiled without IPsec module, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 5259 "util/configparser.c"
    break;

  case 451:
#line 2322 "util/configparser.y"
    {
	#ifdef USE_IPSECMOD
		OUTYY(("P(server_ipsecmod_max_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ipsecmod_max_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without IPsec module, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 5276 "util/configparser.c"
    break;

  case 452:
#line 2336 "util/configparser.y"
    {
	#ifdef USE_IPSECMOD
		OUTYY(("P(server_ipsecmod_whitelist:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->ipsecmod_whitelist, (yyvsp[0].str)))
			yyerror("out of memory");
	#else
		OUTYY(("P(Compiled without IPsec module, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 5291 "util/configparser.c"
    break;

  case 453:
#line 2348 "util/configparser.y"
    {
	#ifdef USE_IPSECMOD
		OUTYY(("P(server_ipsecmod_strict:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ipsecmod_strict = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without IPsec module, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 5308 "util/configparser.c"
    break;

  case 454:
#line 2362 "util/configparser.y"
    {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->stubs->name)
			yyerror("stub name override, there must be one name "
				"for one stub-zone");
		free(cfg_parser->cfg->stubs->name);
		cfg_parser->cfg->stubs->name = (yyvsp[0].str);
	}
#line 5321 "util/configparser.c"
    break;

  case 455:
#line 2372 "util/configparser.y"
    {
		OUTYY(("P(stub-host:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->stubs->hosts, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5331 "util/configparser.c"
    break;

  case 456:
#line 2379 "util/configparser.y"
    {
		OUTYY(("P(stub-addr:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->stubs->addrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5341 "util/configparser.c"
    break;

  case 457:
#line 2386 "util/configparser.y"
    {
		OUTYY(("P(stub-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5353 "util/configparser.c"
    break;

  case 458:
#line 2395 "util/configparser.y"
    {
		OUTYY(("P(stub-no-cache:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->no_cache=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5365 "util/configparser.c"
    break;

  case 459:
#line 2404 "util/configparser.y"
    {
		OUTYY(("P(stub-ssl-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->ssl_upstream = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5378 "util/configparser.c"
    break;

  case 460:
#line 2414 "util/configparser.y"
    {
		OUTYY(("P(stub-prime:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->isprime = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5391 "util/configparser.c"
    break;

  case 461:
#line 2424 "util/configparser.y"
    {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->forwards->name)
			yyerror("forward name override, there must be one "
				"name for one forward-zone");
		free(cfg_parser->cfg->forwards->name);
		cfg_parser->cfg->forwards->name = (yyvsp[0].str);
	}
#line 5404 "util/configparser.c"
    break;

  case 462:
#line 2434 "util/configparser.y"
    {
		OUTYY(("P(forward-host:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->forwards->hosts, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5414 "util/configparser.c"
    break;

  case 463:
#line 2441 "util/configparser.y"
    {
		OUTYY(("P(forward-addr:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->forwards->addrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5424 "util/configparser.c"
    break;

  case 464:
#line 2448 "util/configparser.y"
    {
		OUTYY(("P(forward-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5436 "util/configparser.c"
    break;

  case 465:
#line 2457 "util/configparser.y"
    {
		OUTYY(("P(forward-no-cache:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->no_cache=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5448 "util/configparser.c"
    break;

  case 466:
#line 2466 "util/configparser.y"
    {
		OUTYY(("P(forward-ssl-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->ssl_upstream = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5461 "util/configparser.c"
    break;

  case 467:
#line 2476 "util/configparser.y"
    {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->auths->name)
			yyerror("auth name override, there must be one name "
				"for one auth-zone");
		free(cfg_parser->cfg->auths->name);
		cfg_parser->cfg->auths->name = (yyvsp[0].str);
	}
#line 5474 "util/configparser.c"
    break;

  case 468:
#line 2486 "util/configparser.y"
    {
		OUTYY(("P(zonefile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->auths->zonefile);
		cfg_parser->cfg->auths->zonefile = (yyvsp[0].str);
	}
#line 5484 "util/configparser.c"
    break;

  case 469:
#line 2493 "util/configparser.y"
    {
		OUTYY(("P(master:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->masters, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5494 "util/configparser.c"
    break;

  case 470:
#line 2500 "util/configparser.y"
    {
		OUTYY(("P(url:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->urls, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5504 "util/configparser.c"
    break;

  case 471:
#line 2507 "util/configparser.y"
    {
		OUTYY(("P(allow-notify:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->allow_notify,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5515 "util/configparser.c"
    break;

  case 472:
#line 2515 "util/configparser.y"
    {
		OUTYY(("P(for-downstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->for_downstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5528 "util/configparser.c"
    break;

  case 473:
#line 2525 "util/configparser.y"
    {
		OUTYY(("P(for-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->for_upstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5541 "util/configparser.c"
    break;

  case 474:
#line 2535 "util/configparser.y"
    {
		OUTYY(("P(fallback-enabled:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->fallback_enabled =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5554 "util/configparser.c"
    break;

  case 475:
#line 2545 "util/configparser.y"
    {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->views->name)
			yyerror("view name override, there must be one "
				"name for one view");
		free(cfg_parser->cfg->views->name);
		cfg_parser->cfg->views->name = (yyvsp[0].str);
	}
#line 5567 "util/configparser.c"
    break;

  case 476:
#line 2555 "util/configparser.y"
    {
		OUTYY(("P(view_local_zone:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "static")!=0 && strcmp((yyvsp[0].str), "deny")!=0 &&
		   strcmp((yyvsp[0].str), "refuse")!=0 && strcmp((yyvsp[0].str), "redirect")!=0 &&
		   strcmp((yyvsp[0].str), "transparent")!=0 && strcmp((yyvsp[0].str), "nodefault")!=0
		   && strcmp((yyvsp[0].str), "typetransparent")!=0
		   && strcmp((yyvsp[0].str), "always_transparent")!=0
		   && strcmp((yyvsp[0].str), "always_refuse")!=0
		   && strcmp((yyvsp[0].str), "always_nxdomain")!=0
		   && strcmp((yyvsp[0].str), "noview")!=0
		   && strcmp((yyvsp[0].str), "inform")!=0 && strcmp((yyvsp[0].str), "inform_deny")!=0) {
			yyerror("local-zone type: expected static, deny, "
				"refuse, redirect, transparent, "
				"typetransparent, inform, inform_deny, "
				"always_transparent, always_refuse, "
				"always_nxdomain, noview or nodefault");
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		} else if(strcmp((yyvsp[0].str), "nodefault")==0) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->views->
				local_zones_nodefault, (yyvsp[-1].str)))
				fatal_exit("out of memory adding local-zone");
			free((yyvsp[0].str));
#ifdef USE_IPSET
		} else if(strcmp((yyvsp[0].str), "ipset")==0) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->views->
				local_zones_ipset, (yyvsp[-1].str)))
				fatal_exit("out of memory adding local-zone");
			free((yyvsp[0].str));
#endif
		} else {
			if(!cfg_str2list_insert(
				&cfg_parser->cfg->views->local_zones, 
				(yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding local-zone");
		}
	}
#line 5609 "util/configparser.c"
    break;

  case 477:
#line 2594 "util/configparser.y"
    {
		OUTYY(("P(view_response_ip:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_respip_action((yyvsp[0].str));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->views->respip_actions, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding per-view "
				"response-ip action");
	}
#line 5622 "util/configparser.c"
    break;

  case 478:
#line 2604 "util/configparser.y"
    {
		OUTYY(("P(view_response_ip_data:%s)\n", (yyvsp[-1].str)));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->views->respip_data, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding response-ip-data");
	}
#line 5633 "util/configparser.c"
    break;

  case 479:
#line 2612 "util/configparser.y"
    {
		OUTYY(("P(view_local_data:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->views->local_data, (yyvsp[0].str))) {
			fatal_exit("out of memory adding local-data");
		}
	}
#line 5644 "util/configparser.c"
    break;

  case 480:
#line 2620 "util/configparser.y"
    {
		char* ptr;
		OUTYY(("P(view_local_data_ptr:%s)\n", (yyvsp[0].str)));
		ptr = cfg_ptr_reverse((yyvsp[0].str));
		free((yyvsp[0].str));
		if(ptr) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->views->
				local_data, ptr))
				fatal_exit("out of memory adding local-data");
		} else {
			yyerror("local-data-ptr could not be reversed");
		}
	}
#line 5662 "util/configparser.c"
    break;

  case 481:
#line 2635 "util/configparser.y"
    {
		OUTYY(("P(view-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->views->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5674 "util/configparser.c"
    break;

  case 482:
#line 2644 "util/configparser.y"
    { 
		OUTYY(("\nP(remote-control:)\n")); 
	}
#line 5682 "util/configparser.c"
    break;

  case 493:
#line 2655 "util/configparser.y"
    {
		OUTYY(("P(control_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->remote_control_enable = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5695 "util/configparser.c"
    break;

  case 494:
#line 2665 "util/configparser.y"
    {
		OUTYY(("P(control_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("control port number expected");
		else cfg_parser->cfg->control_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5707 "util/configparser.c"
    break;

  case 495:
#line 2674 "util/configparser.y"
    {
		OUTYY(("P(control_interface:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append(&cfg_parser->cfg->control_ifs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5717 "util/configparser.c"
    break;

  case 496:
#line 2681 "util/configparser.y"
    {
		OUTYY(("P(control_use_cert:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->control_use_cert = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5727 "util/configparser.c"
    break;

  case 497:
#line 2688 "util/configparser.y"
    {
		OUTYY(("P(rc_server_key_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->server_key_file);
		cfg_parser->cfg->server_key_file = (yyvsp[0].str);
	}
#line 5737 "util/configparser.c"
    break;

  case 498:
#line 2695 "util/configparser.y"
    {
		OUTYY(("P(rc_server_cert_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->server_cert_file);
		cfg_parser->cfg->server_cert_file = (yyvsp[0].str);
	}
#line 5747 "util/configparser.c"
    break;

  case 499:
#line 2702 "util/configparser.y"
    {
		OUTYY(("P(rc_control_key_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->control_key_file);
		cfg_parser->cfg->control_key_file = (yyvsp[0].str);
	}
#line 5757 "util/configparser.c"
    break;

  case 500:
#line 2709 "util/configparser.y"
    {
		OUTYY(("P(rc_control_cert_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->control_cert_file);
		cfg_parser->cfg->control_cert_file = (yyvsp[0].str);
	}
#line 5767 "util/configparser.c"
    break;

  case 501:
#line 2716 "util/configparser.y"
    {
		OUTYY(("\nP(dnstap:)\n"));
	}
#line 5775 "util/configparser.c"
    break;

  case 516:
#line 2733 "util/configparser.y"
    {
		OUTYY(("P(dt_dnstap_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5787 "util/configparser.c"
    break;

  case 517:
#line 2742 "util/configparser.y"
    {
		OUTYY(("P(dt_dnstap_socket_path:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_socket_path);
		cfg_parser->cfg->dnstap_socket_path = (yyvsp[0].str);
	}
#line 5797 "util/configparser.c"
    break;

  case 518:
#line 2749 "util/configparser.y"
    {
		OUTYY(("P(dt_dnstap_send_identity:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_send_identity = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5809 "util/configparser.c"
    break;

  case 519:
#line 2758 "util/configparser.y"
    {
		OUTYY(("P(dt_dnstap_send_version:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_send_version = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5821 "util/configparser.c"
    break;

  case 520:
#line 2767 "util/configparser.y"
    {
		OUTYY(("P(dt_dnstap_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_identity);
		cfg_parser->cfg->dnstap_identity = (yyvsp[0].str);
	}
#line 5831 "util/configparser.c"
    break;

  case 521:
#line 2774 "util/configparser.y"
    {
		OUTYY(("P(dt_dnstap_version:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_version);
		cfg_parser->cfg->dnstap_version = (yyvsp[0].str);
	}
#line 5841 "util/configparser.c"
    break;

  case 522:
#line 2781 "util/configparser.y"
    {
		OUTYY(("P(dt_dnstap_log_resolver_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_resolver_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5854 "util/configparser.c"
    break;

  case 523:
#line 2791 "util/configparser.y"
    {
		OUTYY(("P(dt_dnstap_log_resolver_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_resolver_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5867 "util/configparser.c"
    break;

  case 524:
#line 2801 "util/configparser.y"
    {
		OUTYY(("P(dt_dnstap_log_client_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_client_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5880 "util/configparser.c"
    break;

  case 525:
#line 2811 "util/configparser.y"
    {
		OUTYY(("P(dt_dnstap_log_client_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_client_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5893 "util/configparser.c"
    break;

  case 526:
#line 2821 "util/configparser.y"
    {
		OUTYY(("P(dt_dnstap_log_forwarder_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_forwarder_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5906 "util/configparser.c"
    break;

  case 527:
#line 2831 "util/configparser.y"
    {
		OUTYY(("P(dt_dnstap_log_forwarder_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_forwarder_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5919 "util/configparser.c"
    break;

  case 528:
#line 2841 "util/configparser.y"
    { 
		OUTYY(("\nP(python:)\n")); 
	}
#line 5927 "util/configparser.c"
    break;

  case 532:
#line 2850 "util/configparser.y"
    {
		OUTYY(("P(python-script:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append_ex(&cfg_parser->cfg->python_script, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5937 "util/configparser.c"
    break;

  case 533:
#line 2856 "util/configparser.y"
    {
		OUTYY(("P(disable_dnssec_lame_check:%s)\n", (yyvsp[0].str)));
		if (strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->disable_dnssec_lame_check =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5950 "util/configparser.c"
    break;

  case 534:
#line 2866 "util/configparser.y"
    {
		OUTYY(("P(server_log_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->log_identity);
		cfg_parser->cfg->log_identity = (yyvsp[0].str);
	}
#line 5960 "util/configparser.c"
    break;

  case 535:
#line 2873 "util/configparser.y"
    {
		OUTYY(("P(server_response_ip:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_respip_action((yyvsp[0].str));
		if(!cfg_str2list_insert(&cfg_parser->cfg->respip_actions,
			(yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding response-ip");
	}
#line 5972 "util/configparser.c"
    break;

  case 536:
#line 2882 "util/configparser.y"
    {
		OUTYY(("P(server_response_ip_data:%s)\n", (yyvsp[-1].str)));
		if(!cfg_str2list_insert(&cfg_parser->cfg->respip_data,
			(yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding response-ip-data");
	}
#line 5983 "util/configparser.c"
    break;

  case 537:
#line 2890 "util/configparser.y"
    {
		OUTYY(("\nP(dnscrypt:)\n"));
	}
#line 5991 "util/configparser.c"
    break;

  case 550:
#line 2906 "util/configparser.y"
    {
		OUTYY(("P(dnsc_dnscrypt_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnscrypt = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6003 "util/configparser.c"
    break;

  case 551:
#line 2916 "util/configparser.y"
    {
		OUTYY(("P(dnsc_dnscrypt_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->dnscrypt_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6015 "util/configparser.c"
    break;

  case 552:
#line 2925 "util/configparser.y"
    {
		OUTYY(("P(dnsc_dnscrypt_provider:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnscrypt_provider);
		cfg_parser->cfg->dnscrypt_provider = (yyvsp[0].str);
	}
#line 6025 "util/configparser.c"
    break;

  case 553:
#line 2932 "util/configparser.y"
    {
		OUTYY(("P(dnsc_dnscrypt_provider_cert:%s)\n", (yyvsp[0].str)));
		if(cfg_strlist_find(cfg_parser->cfg->dnscrypt_provider_cert, (yyvsp[0].str)))
			log_warn("dnscrypt-provider-cert %s is a duplicate", (yyvsp[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_provider_cert, (yyvsp[0].str)))
			fatal_exit("out of memory adding dnscrypt-provider-cert");
	}
#line 6037 "util/configparser.c"
    break;

  case 554:
#line 2941 "util/configparser.y"
    {
		OUTYY(("P(dnsc_dnscrypt_provider_cert_rotated:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_provider_cert_rotated, (yyvsp[0].str)))
			fatal_exit("out of memory adding dnscrypt-provider-cert-rotated");
	}
#line 6047 "util/configparser.c"
    break;

  case 555:
#line 2948 "util/configparser.y"
    {
		OUTYY(("P(dnsc_dnscrypt_secret_key:%s)\n", (yyvsp[0].str)));
		if(cfg_strlist_find(cfg_parser->cfg->dnscrypt_secret_key, (yyvsp[0].str)))
			log_warn("dnscrypt-secret-key: %s is a duplicate", (yyvsp[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_secret_key, (yyvsp[0].str)))
			fatal_exit("out of memory adding dnscrypt-secret-key");
	}
#line 6059 "util/configparser.c"
    break;

  case 556:
#line 2957 "util/configparser.y"
    {
  	OUTYY(("P(dnscrypt_shared_secret_cache_size:%s)\n", (yyvsp[0].str)));
  	if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->dnscrypt_shared_secret_cache_size))
  		yyerror("memory size expected");
  	free((yyvsp[0].str));
  }
#line 6070 "util/configparser.c"
    break;

  case 557:
#line 2965 "util/configparser.y"
    {
  	OUTYY(("P(dnscrypt_shared_secret_cache_slabs:%s)\n", (yyvsp[0].str)));
  	if(atoi((yyvsp[0].str)) == 0)
  		yyerror("number expected");
  	else {
  		cfg_parser->cfg->dnscrypt_shared_secret_cache_slabs = atoi((yyvsp[0].str));
  		if(!is_pow2(cfg_parser->cfg->dnscrypt_shared_secret_cache_slabs))
  			yyerror("must be a power of 2");
  	}
  	free((yyvsp[0].str));
  }
#line 6086 "util/configparser.c"
    break;

  case 558:
#line 2978 "util/configparser.y"
    {
  	OUTYY(("P(dnscrypt_nonce_cache_size:%s)\n", (yyvsp[0].str)));
  	if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->dnscrypt_nonce_cache_size))
  		yyerror("memory size expected");
  	free((yyvsp[0].str));
  }
#line 6097 "util/configparser.c"
    break;

  case 559:
#line 2986 "util/configparser.y"
    {
  	OUTYY(("P(dnscrypt_nonce_cache_slabs:%s)\n", (yyvsp[0].str)));
  	if(atoi((yyvsp[0].str)) == 0)
  		yyerror("number expected");
  	else {
  		cfg_parser->cfg->dnscrypt_nonce_cache_slabs = atoi((yyvsp[0].str));
  		if(!is_pow2(cfg_parser->cfg->dnscrypt_nonce_cache_slabs))
  			yyerror("must be a power of 2");
  	}
  	free((yyvsp[0].str));
  }
#line 6113 "util/configparser.c"
    break;

  case 560:
#line 2999 "util/configparser.y"
    {
		OUTYY(("\nP(cachedb:)\n"));
	}
#line 6121 "util/configparser.c"
    break;

  case 568:
#line 3009 "util/configparser.y"
    {
	#ifdef USE_CACHEDB
		OUTYY(("P(backend:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->cachedb_backend);
		cfg_parser->cfg->cachedb_backend = (yyvsp[0].str);
	#else
		OUTYY(("P(Compiled without cachedb, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 6136 "util/configparser.c"
    break;

  case 569:
#line 3021 "util/configparser.y"
    {
	#ifdef USE_CACHEDB
		OUTYY(("P(secret-seed:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->cachedb_secret);
		cfg_parser->cfg->cachedb_secret = (yyvsp[0].str);
	#else
		OUTYY(("P(Compiled without cachedb, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 6151 "util/configparser.c"
    break;

  case 570:
#line 3033 "util/configparser.y"
    {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		OUTYY(("P(redis_server_host:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->redis_server_host);
		cfg_parser->cfg->redis_server_host = (yyvsp[0].str);
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 6166 "util/configparser.c"
    break;

  case 571:
#line 3045 "util/configparser.y"
    {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		int port;
		OUTYY(("P(redis_server_port:%s)\n", (yyvsp[0].str)));
		port = atoi((yyvsp[0].str));
		if(port == 0 || port < 0 || port > 65535)
			yyerror("valid redis server port number expected");
		else cfg_parser->cfg->redis_server_port = port;
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 6184 "util/configparser.c"
    break;

  case 572:
#line 3060 "util/configparser.y"
    {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		OUTYY(("P(redis_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("redis timeout value expected");
		else cfg_parser->cfg->redis_timeout = atoi((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 6200 "util/configparser.c"
    break;

  case 573:
#line 3073 "util/configparser.y"
    {
		OUTYY(("P(server_tcp_connection_limit:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if (atoi((yyvsp[0].str)) < 0)
			yyerror("positive number expected");
		else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->tcp_connection_limits, (yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding tcp connection limit");
		}
	}
#line 6214 "util/configparser.c"
    break;

  case 574:
#line 3084 "util/configparser.y"
    {
			OUTYY(("\nP(ipset:)\n"));
		}
#line 6222 "util/configparser.c"
    break;

  case 579:
#line 3093 "util/configparser.y"
    {
		#ifdef USE_IPSET
			OUTYY(("P(name-v4:%s)\n", (yyvsp[0].str)));
			if(cfg_parser->cfg->ipset_name_v4)
				yyerror("ipset name v4 override, there must be one "
					"name for ip v4");
			free(cfg_parser->cfg->ipset_name_v4);
			cfg_parser->cfg->ipset_name_v4 = (yyvsp[0].str);
		#else
			OUTYY(("P(Compiled without ipset, ignoring)\n"));
			free((yyvsp[0].str));
		#endif
		}
#line 6240 "util/configparser.c"
    break;

  case 580:
#line 3108 "util/configparser.y"
    {
		#ifdef USE_IPSET
			OUTYY(("P(name-v6:%s)\n", (yyvsp[0].str)));
			if(cfg_parser->cfg->ipset_name_v6)
				yyerror("ipset name v6 override, there must be one "
					"name for ip v6");
			free(cfg_parser->cfg->ipset_name_v6);
			cfg_parser->cfg->ipset_name_v6 = (yyvsp[0].str);
		#else
			OUTYY(("P(Compiled without ipset, ignoring)\n"));
			free((yyvsp[0].str));
		#endif
		}
#line 6258 "util/configparser.c"
    break;


#line 6262 "util/configparser.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;


#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif


/*-----------------------------------------------------.
| yyreturn -- parsing is finished, return the result.  |
`-----------------------------------------------------*/
yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 3122 "util/configparser.y"


/* parse helper routines could be here */
static void
validate_respip_action(const char* action)
{
	if(strcmp(action, "deny")!=0 &&
		strcmp(action, "redirect")!=0 &&
		strcmp(action, "inform")!=0 &&
		strcmp(action, "inform_deny")!=0 &&
		strcmp(action, "always_transparent")!=0 &&
		strcmp(action, "always_refuse")!=0 &&
		strcmp(action, "always_nxdomain")!=0)
	{
		yyerror("response-ip action: expected deny, redirect, "
			"inform, inform_deny, always_transparent, "
			"always_refuse or always_nxdomain");
	}
}


