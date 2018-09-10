/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 38 "util/configparser.y" /* yacc.c:339  */

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


#line 95 "util/configparser.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "configparser.h".  */
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
    VAR_CHROOT = 279,
    VAR_USERNAME = 280,
    VAR_DIRECTORY = 281,
    VAR_LOGFILE = 282,
    VAR_PIDFILE = 283,
    VAR_MSG_CACHE_SIZE = 284,
    VAR_MSG_CACHE_SLABS = 285,
    VAR_NUM_QUERIES_PER_THREAD = 286,
    VAR_RRSET_CACHE_SIZE = 287,
    VAR_RRSET_CACHE_SLABS = 288,
    VAR_OUTGOING_NUM_TCP = 289,
    VAR_INFRA_HOST_TTL = 290,
    VAR_INFRA_LAME_TTL = 291,
    VAR_INFRA_CACHE_SLABS = 292,
    VAR_INFRA_CACHE_NUMHOSTS = 293,
    VAR_INFRA_CACHE_LAME_SIZE = 294,
    VAR_NAME = 295,
    VAR_STUB_ZONE = 296,
    VAR_STUB_HOST = 297,
    VAR_STUB_ADDR = 298,
    VAR_TARGET_FETCH_POLICY = 299,
    VAR_HARDEN_SHORT_BUFSIZE = 300,
    VAR_HARDEN_LARGE_QUERIES = 301,
    VAR_FORWARD_ZONE = 302,
    VAR_FORWARD_HOST = 303,
    VAR_FORWARD_ADDR = 304,
    VAR_DO_NOT_QUERY_ADDRESS = 305,
    VAR_HIDE_IDENTITY = 306,
    VAR_HIDE_VERSION = 307,
    VAR_IDENTITY = 308,
    VAR_VERSION = 309,
    VAR_HARDEN_GLUE = 310,
    VAR_MODULE_CONF = 311,
    VAR_TRUST_ANCHOR_FILE = 312,
    VAR_TRUST_ANCHOR = 313,
    VAR_VAL_OVERRIDE_DATE = 314,
    VAR_BOGUS_TTL = 315,
    VAR_VAL_CLEAN_ADDITIONAL = 316,
    VAR_VAL_PERMISSIVE_MODE = 317,
    VAR_INCOMING_NUM_TCP = 318,
    VAR_MSG_BUFFER_SIZE = 319,
    VAR_KEY_CACHE_SIZE = 320,
    VAR_KEY_CACHE_SLABS = 321,
    VAR_TRUSTED_KEYS_FILE = 322,
    VAR_VAL_NSEC3_KEYSIZE_ITERATIONS = 323,
    VAR_USE_SYSLOG = 324,
    VAR_OUTGOING_INTERFACE = 325,
    VAR_ROOT_HINTS = 326,
    VAR_DO_NOT_QUERY_LOCALHOST = 327,
    VAR_CACHE_MAX_TTL = 328,
    VAR_HARDEN_DNSSEC_STRIPPED = 329,
    VAR_ACCESS_CONTROL = 330,
    VAR_LOCAL_ZONE = 331,
    VAR_LOCAL_DATA = 332,
    VAR_INTERFACE_AUTOMATIC = 333,
    VAR_STATISTICS_INTERVAL = 334,
    VAR_DO_DAEMONIZE = 335,
    VAR_USE_CAPS_FOR_ID = 336,
    VAR_STATISTICS_CUMULATIVE = 337,
    VAR_OUTGOING_PORT_PERMIT = 338,
    VAR_OUTGOING_PORT_AVOID = 339,
    VAR_DLV_ANCHOR_FILE = 340,
    VAR_DLV_ANCHOR = 341,
    VAR_NEG_CACHE_SIZE = 342,
    VAR_HARDEN_REFERRAL_PATH = 343,
    VAR_PRIVATE_ADDRESS = 344,
    VAR_PRIVATE_DOMAIN = 345,
    VAR_REMOTE_CONTROL = 346,
    VAR_CONTROL_ENABLE = 347,
    VAR_CONTROL_INTERFACE = 348,
    VAR_CONTROL_PORT = 349,
    VAR_SERVER_KEY_FILE = 350,
    VAR_SERVER_CERT_FILE = 351,
    VAR_CONTROL_KEY_FILE = 352,
    VAR_CONTROL_CERT_FILE = 353,
    VAR_CONTROL_USE_CERT = 354,
    VAR_EXTENDED_STATISTICS = 355,
    VAR_LOCAL_DATA_PTR = 356,
    VAR_JOSTLE_TIMEOUT = 357,
    VAR_STUB_PRIME = 358,
    VAR_UNWANTED_REPLY_THRESHOLD = 359,
    VAR_LOG_TIME_ASCII = 360,
    VAR_DOMAIN_INSECURE = 361,
    VAR_PYTHON = 362,
    VAR_PYTHON_SCRIPT = 363,
    VAR_VAL_SIG_SKEW_MIN = 364,
    VAR_VAL_SIG_SKEW_MAX = 365,
    VAR_CACHE_MIN_TTL = 366,
    VAR_VAL_LOG_LEVEL = 367,
    VAR_AUTO_TRUST_ANCHOR_FILE = 368,
    VAR_KEEP_MISSING = 369,
    VAR_ADD_HOLDDOWN = 370,
    VAR_DEL_HOLDDOWN = 371,
    VAR_SO_RCVBUF = 372,
    VAR_EDNS_BUFFER_SIZE = 373,
    VAR_PREFETCH = 374,
    VAR_PREFETCH_KEY = 375,
    VAR_SO_SNDBUF = 376,
    VAR_SO_REUSEPORT = 377,
    VAR_HARDEN_BELOW_NXDOMAIN = 378,
    VAR_IGNORE_CD_FLAG = 379,
    VAR_LOG_QUERIES = 380,
    VAR_LOG_REPLIES = 381,
    VAR_TCP_UPSTREAM = 382,
    VAR_SSL_UPSTREAM = 383,
    VAR_SSL_SERVICE_KEY = 384,
    VAR_SSL_SERVICE_PEM = 385,
    VAR_SSL_PORT = 386,
    VAR_FORWARD_FIRST = 387,
    VAR_STUB_SSL_UPSTREAM = 388,
    VAR_FORWARD_SSL_UPSTREAM = 389,
    VAR_TLS_CERT_BUNDLE = 390,
    VAR_STUB_FIRST = 391,
    VAR_MINIMAL_RESPONSES = 392,
    VAR_RRSET_ROUNDROBIN = 393,
    VAR_MAX_UDP_SIZE = 394,
    VAR_DELAY_CLOSE = 395,
    VAR_UNBLOCK_LAN_ZONES = 396,
    VAR_INSECURE_LAN_ZONES = 397,
    VAR_INFRA_CACHE_MIN_RTT = 398,
    VAR_DNS64_PREFIX = 399,
    VAR_DNS64_SYNTHALL = 400,
    VAR_DNSTAP = 401,
    VAR_DNSTAP_ENABLE = 402,
    VAR_DNSTAP_SOCKET_PATH = 403,
    VAR_DNSTAP_SEND_IDENTITY = 404,
    VAR_DNSTAP_SEND_VERSION = 405,
    VAR_DNSTAP_IDENTITY = 406,
    VAR_DNSTAP_VERSION = 407,
    VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES = 408,
    VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES = 409,
    VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES = 410,
    VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES = 411,
    VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES = 412,
    VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES = 413,
    VAR_RESPONSE_IP_TAG = 414,
    VAR_RESPONSE_IP = 415,
    VAR_RESPONSE_IP_DATA = 416,
    VAR_HARDEN_ALGO_DOWNGRADE = 417,
    VAR_IP_TRANSPARENT = 418,
    VAR_DISABLE_DNSSEC_LAME_CHECK = 419,
    VAR_IP_RATELIMIT = 420,
    VAR_IP_RATELIMIT_SLABS = 421,
    VAR_IP_RATELIMIT_SIZE = 422,
    VAR_RATELIMIT = 423,
    VAR_RATELIMIT_SLABS = 424,
    VAR_RATELIMIT_SIZE = 425,
    VAR_RATELIMIT_FOR_DOMAIN = 426,
    VAR_RATELIMIT_BELOW_DOMAIN = 427,
    VAR_IP_RATELIMIT_FACTOR = 428,
    VAR_RATELIMIT_FACTOR = 429,
    VAR_SEND_CLIENT_SUBNET = 430,
    VAR_CLIENT_SUBNET_ZONE = 431,
    VAR_CLIENT_SUBNET_ALWAYS_FORWARD = 432,
    VAR_CLIENT_SUBNET_OPCODE = 433,
    VAR_MAX_CLIENT_SUBNET_IPV4 = 434,
    VAR_MAX_CLIENT_SUBNET_IPV6 = 435,
    VAR_CAPS_WHITELIST = 436,
    VAR_CACHE_MAX_NEGATIVE_TTL = 437,
    VAR_PERMIT_SMALL_HOLDDOWN = 438,
    VAR_QNAME_MINIMISATION = 439,
    VAR_QNAME_MINIMISATION_STRICT = 440,
    VAR_IP_FREEBIND = 441,
    VAR_DEFINE_TAG = 442,
    VAR_LOCAL_ZONE_TAG = 443,
    VAR_ACCESS_CONTROL_TAG = 444,
    VAR_LOCAL_ZONE_OVERRIDE = 445,
    VAR_ACCESS_CONTROL_TAG_ACTION = 446,
    VAR_ACCESS_CONTROL_TAG_DATA = 447,
    VAR_VIEW = 448,
    VAR_ACCESS_CONTROL_VIEW = 449,
    VAR_VIEW_FIRST = 450,
    VAR_SERVE_EXPIRED = 451,
    VAR_FAKE_DSA = 452,
    VAR_FAKE_SHA1 = 453,
    VAR_LOG_IDENTITY = 454,
    VAR_HIDE_TRUSTANCHOR = 455,
    VAR_TRUST_ANCHOR_SIGNALING = 456,
    VAR_AGGRESSIVE_NSEC = 457,
    VAR_USE_SYSTEMD = 458,
    VAR_SHM_ENABLE = 459,
    VAR_SHM_KEY = 460,
    VAR_ROOT_KEY_SENTINEL = 461,
    VAR_DNSCRYPT = 462,
    VAR_DNSCRYPT_ENABLE = 463,
    VAR_DNSCRYPT_PORT = 464,
    VAR_DNSCRYPT_PROVIDER = 465,
    VAR_DNSCRYPT_SECRET_KEY = 466,
    VAR_DNSCRYPT_PROVIDER_CERT = 467,
    VAR_DNSCRYPT_PROVIDER_CERT_ROTATED = 468,
    VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE = 469,
    VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS = 470,
    VAR_DNSCRYPT_NONCE_CACHE_SIZE = 471,
    VAR_DNSCRYPT_NONCE_CACHE_SLABS = 472,
    VAR_IPSECMOD_ENABLED = 473,
    VAR_IPSECMOD_HOOK = 474,
    VAR_IPSECMOD_IGNORE_BOGUS = 475,
    VAR_IPSECMOD_MAX_TTL = 476,
    VAR_IPSECMOD_WHITELIST = 477,
    VAR_IPSECMOD_STRICT = 478,
    VAR_CACHEDB = 479,
    VAR_CACHEDB_BACKEND = 480,
    VAR_CACHEDB_SECRETSEED = 481,
    VAR_CACHEDB_REDISHOST = 482,
    VAR_CACHEDB_REDISPORT = 483,
    VAR_CACHEDB_REDISTIMEOUT = 484,
    VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM = 485,
    VAR_FOR_UPSTREAM = 486,
    VAR_AUTH_ZONE = 487,
    VAR_ZONEFILE = 488,
    VAR_MASTER = 489,
    VAR_URL = 490,
    VAR_FOR_DOWNSTREAM = 491,
    VAR_FALLBACK_ENABLED = 492,
    VAR_TLS_ADDITIONAL_PORT = 493,
    VAR_LOW_RTT = 494,
    VAR_LOW_RTT_PERMIL = 495,
    VAR_ALLOW_NOTIFY = 496,
    VAR_TLS_WIN_CERT = 497
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
#define VAR_CHROOT 279
#define VAR_USERNAME 280
#define VAR_DIRECTORY 281
#define VAR_LOGFILE 282
#define VAR_PIDFILE 283
#define VAR_MSG_CACHE_SIZE 284
#define VAR_MSG_CACHE_SLABS 285
#define VAR_NUM_QUERIES_PER_THREAD 286
#define VAR_RRSET_CACHE_SIZE 287
#define VAR_RRSET_CACHE_SLABS 288
#define VAR_OUTGOING_NUM_TCP 289
#define VAR_INFRA_HOST_TTL 290
#define VAR_INFRA_LAME_TTL 291
#define VAR_INFRA_CACHE_SLABS 292
#define VAR_INFRA_CACHE_NUMHOSTS 293
#define VAR_INFRA_CACHE_LAME_SIZE 294
#define VAR_NAME 295
#define VAR_STUB_ZONE 296
#define VAR_STUB_HOST 297
#define VAR_STUB_ADDR 298
#define VAR_TARGET_FETCH_POLICY 299
#define VAR_HARDEN_SHORT_BUFSIZE 300
#define VAR_HARDEN_LARGE_QUERIES 301
#define VAR_FORWARD_ZONE 302
#define VAR_FORWARD_HOST 303
#define VAR_FORWARD_ADDR 304
#define VAR_DO_NOT_QUERY_ADDRESS 305
#define VAR_HIDE_IDENTITY 306
#define VAR_HIDE_VERSION 307
#define VAR_IDENTITY 308
#define VAR_VERSION 309
#define VAR_HARDEN_GLUE 310
#define VAR_MODULE_CONF 311
#define VAR_TRUST_ANCHOR_FILE 312
#define VAR_TRUST_ANCHOR 313
#define VAR_VAL_OVERRIDE_DATE 314
#define VAR_BOGUS_TTL 315
#define VAR_VAL_CLEAN_ADDITIONAL 316
#define VAR_VAL_PERMISSIVE_MODE 317
#define VAR_INCOMING_NUM_TCP 318
#define VAR_MSG_BUFFER_SIZE 319
#define VAR_KEY_CACHE_SIZE 320
#define VAR_KEY_CACHE_SLABS 321
#define VAR_TRUSTED_KEYS_FILE 322
#define VAR_VAL_NSEC3_KEYSIZE_ITERATIONS 323
#define VAR_USE_SYSLOG 324
#define VAR_OUTGOING_INTERFACE 325
#define VAR_ROOT_HINTS 326
#define VAR_DO_NOT_QUERY_LOCALHOST 327
#define VAR_CACHE_MAX_TTL 328
#define VAR_HARDEN_DNSSEC_STRIPPED 329
#define VAR_ACCESS_CONTROL 330
#define VAR_LOCAL_ZONE 331
#define VAR_LOCAL_DATA 332
#define VAR_INTERFACE_AUTOMATIC 333
#define VAR_STATISTICS_INTERVAL 334
#define VAR_DO_DAEMONIZE 335
#define VAR_USE_CAPS_FOR_ID 336
#define VAR_STATISTICS_CUMULATIVE 337
#define VAR_OUTGOING_PORT_PERMIT 338
#define VAR_OUTGOING_PORT_AVOID 339
#define VAR_DLV_ANCHOR_FILE 340
#define VAR_DLV_ANCHOR 341
#define VAR_NEG_CACHE_SIZE 342
#define VAR_HARDEN_REFERRAL_PATH 343
#define VAR_PRIVATE_ADDRESS 344
#define VAR_PRIVATE_DOMAIN 345
#define VAR_REMOTE_CONTROL 346
#define VAR_CONTROL_ENABLE 347
#define VAR_CONTROL_INTERFACE 348
#define VAR_CONTROL_PORT 349
#define VAR_SERVER_KEY_FILE 350
#define VAR_SERVER_CERT_FILE 351
#define VAR_CONTROL_KEY_FILE 352
#define VAR_CONTROL_CERT_FILE 353
#define VAR_CONTROL_USE_CERT 354
#define VAR_EXTENDED_STATISTICS 355
#define VAR_LOCAL_DATA_PTR 356
#define VAR_JOSTLE_TIMEOUT 357
#define VAR_STUB_PRIME 358
#define VAR_UNWANTED_REPLY_THRESHOLD 359
#define VAR_LOG_TIME_ASCII 360
#define VAR_DOMAIN_INSECURE 361
#define VAR_PYTHON 362
#define VAR_PYTHON_SCRIPT 363
#define VAR_VAL_SIG_SKEW_MIN 364
#define VAR_VAL_SIG_SKEW_MAX 365
#define VAR_CACHE_MIN_TTL 366
#define VAR_VAL_LOG_LEVEL 367
#define VAR_AUTO_TRUST_ANCHOR_FILE 368
#define VAR_KEEP_MISSING 369
#define VAR_ADD_HOLDDOWN 370
#define VAR_DEL_HOLDDOWN 371
#define VAR_SO_RCVBUF 372
#define VAR_EDNS_BUFFER_SIZE 373
#define VAR_PREFETCH 374
#define VAR_PREFETCH_KEY 375
#define VAR_SO_SNDBUF 376
#define VAR_SO_REUSEPORT 377
#define VAR_HARDEN_BELOW_NXDOMAIN 378
#define VAR_IGNORE_CD_FLAG 379
#define VAR_LOG_QUERIES 380
#define VAR_LOG_REPLIES 381
#define VAR_TCP_UPSTREAM 382
#define VAR_SSL_UPSTREAM 383
#define VAR_SSL_SERVICE_KEY 384
#define VAR_SSL_SERVICE_PEM 385
#define VAR_SSL_PORT 386
#define VAR_FORWARD_FIRST 387
#define VAR_STUB_SSL_UPSTREAM 388
#define VAR_FORWARD_SSL_UPSTREAM 389
#define VAR_TLS_CERT_BUNDLE 390
#define VAR_STUB_FIRST 391
#define VAR_MINIMAL_RESPONSES 392
#define VAR_RRSET_ROUNDROBIN 393
#define VAR_MAX_UDP_SIZE 394
#define VAR_DELAY_CLOSE 395
#define VAR_UNBLOCK_LAN_ZONES 396
#define VAR_INSECURE_LAN_ZONES 397
#define VAR_INFRA_CACHE_MIN_RTT 398
#define VAR_DNS64_PREFIX 399
#define VAR_DNS64_SYNTHALL 400
#define VAR_DNSTAP 401
#define VAR_DNSTAP_ENABLE 402
#define VAR_DNSTAP_SOCKET_PATH 403
#define VAR_DNSTAP_SEND_IDENTITY 404
#define VAR_DNSTAP_SEND_VERSION 405
#define VAR_DNSTAP_IDENTITY 406
#define VAR_DNSTAP_VERSION 407
#define VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES 408
#define VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES 409
#define VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES 410
#define VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES 411
#define VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES 412
#define VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES 413
#define VAR_RESPONSE_IP_TAG 414
#define VAR_RESPONSE_IP 415
#define VAR_RESPONSE_IP_DATA 416
#define VAR_HARDEN_ALGO_DOWNGRADE 417
#define VAR_IP_TRANSPARENT 418
#define VAR_DISABLE_DNSSEC_LAME_CHECK 419
#define VAR_IP_RATELIMIT 420
#define VAR_IP_RATELIMIT_SLABS 421
#define VAR_IP_RATELIMIT_SIZE 422
#define VAR_RATELIMIT 423
#define VAR_RATELIMIT_SLABS 424
#define VAR_RATELIMIT_SIZE 425
#define VAR_RATELIMIT_FOR_DOMAIN 426
#define VAR_RATELIMIT_BELOW_DOMAIN 427
#define VAR_IP_RATELIMIT_FACTOR 428
#define VAR_RATELIMIT_FACTOR 429
#define VAR_SEND_CLIENT_SUBNET 430
#define VAR_CLIENT_SUBNET_ZONE 431
#define VAR_CLIENT_SUBNET_ALWAYS_FORWARD 432
#define VAR_CLIENT_SUBNET_OPCODE 433
#define VAR_MAX_CLIENT_SUBNET_IPV4 434
#define VAR_MAX_CLIENT_SUBNET_IPV6 435
#define VAR_CAPS_WHITELIST 436
#define VAR_CACHE_MAX_NEGATIVE_TTL 437
#define VAR_PERMIT_SMALL_HOLDDOWN 438
#define VAR_QNAME_MINIMISATION 439
#define VAR_QNAME_MINIMISATION_STRICT 440
#define VAR_IP_FREEBIND 441
#define VAR_DEFINE_TAG 442
#define VAR_LOCAL_ZONE_TAG 443
#define VAR_ACCESS_CONTROL_TAG 444
#define VAR_LOCAL_ZONE_OVERRIDE 445
#define VAR_ACCESS_CONTROL_TAG_ACTION 446
#define VAR_ACCESS_CONTROL_TAG_DATA 447
#define VAR_VIEW 448
#define VAR_ACCESS_CONTROL_VIEW 449
#define VAR_VIEW_FIRST 450
#define VAR_SERVE_EXPIRED 451
#define VAR_FAKE_DSA 452
#define VAR_FAKE_SHA1 453
#define VAR_LOG_IDENTITY 454
#define VAR_HIDE_TRUSTANCHOR 455
#define VAR_TRUST_ANCHOR_SIGNALING 456
#define VAR_AGGRESSIVE_NSEC 457
#define VAR_USE_SYSTEMD 458
#define VAR_SHM_ENABLE 459
#define VAR_SHM_KEY 460
#define VAR_ROOT_KEY_SENTINEL 461
#define VAR_DNSCRYPT 462
#define VAR_DNSCRYPT_ENABLE 463
#define VAR_DNSCRYPT_PORT 464
#define VAR_DNSCRYPT_PROVIDER 465
#define VAR_DNSCRYPT_SECRET_KEY 466
#define VAR_DNSCRYPT_PROVIDER_CERT 467
#define VAR_DNSCRYPT_PROVIDER_CERT_ROTATED 468
#define VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE 469
#define VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS 470
#define VAR_DNSCRYPT_NONCE_CACHE_SIZE 471
#define VAR_DNSCRYPT_NONCE_CACHE_SLABS 472
#define VAR_IPSECMOD_ENABLED 473
#define VAR_IPSECMOD_HOOK 474
#define VAR_IPSECMOD_IGNORE_BOGUS 475
#define VAR_IPSECMOD_MAX_TTL 476
#define VAR_IPSECMOD_WHITELIST 477
#define VAR_IPSECMOD_STRICT 478
#define VAR_CACHEDB 479
#define VAR_CACHEDB_BACKEND 480
#define VAR_CACHEDB_SECRETSEED 481
#define VAR_CACHEDB_REDISHOST 482
#define VAR_CACHEDB_REDISPORT 483
#define VAR_CACHEDB_REDISTIMEOUT 484
#define VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM 485
#define VAR_FOR_UPSTREAM 486
#define VAR_AUTH_ZONE 487
#define VAR_ZONEFILE 488
#define VAR_MASTER 489
#define VAR_URL 490
#define VAR_FOR_DOWNSTREAM 491
#define VAR_FALLBACK_ENABLED 492
#define VAR_TLS_ADDITIONAL_PORT 493
#define VAR_LOW_RTT 494
#define VAR_LOW_RTT_PERMIL 495
#define VAR_ALLOW_NOTIFY 496
#define VAR_TLS_WIN_CERT 497

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 66 "util/configparser.y" /* yacc.c:355  */

	char*	str;

#line 623 "util/configparser.c" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_UTIL_CONFIGPARSER_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 640 "util/configparser.c" /* yacc.c:358  */

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
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
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
#  define YYSIZE_T unsigned int
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

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
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
#define YYLAST   491

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  243
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  263
/* YYNRULES -- Number of rules.  */
#define YYNRULES  503
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  753

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   497

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
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
     235,   236,   237,   238,   239,   240,   241,   242
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   163,   163,   163,   164,   164,   165,   165,   166,   166,
     166,   167,   167,   168,   172,   177,   178,   179,   179,   179,
     180,   180,   181,   181,   182,   182,   183,   183,   184,   184,
     184,   185,   185,   185,   186,   186,   187,   187,   188,   188,
     189,   189,   190,   190,   191,   191,   192,   192,   193,   193,
     194,   194,   194,   195,   195,   195,   196,   196,   196,   197,
     197,   198,   198,   199,   199,   200,   200,   201,   201,   201,
     202,   202,   203,   203,   204,   204,   204,   205,   205,   206,
     206,   207,   207,   208,   208,   208,   209,   209,   210,   210,
     211,   211,   212,   212,   213,   213,   214,   214,   214,   215,
     215,   216,   216,   216,   217,   217,   217,   218,   218,   218,
     219,   219,   219,   219,   220,   220,   220,   221,   221,   221,
     222,   222,   223,   223,   224,   224,   225,   225,   226,   226,
     226,   227,   227,   228,   228,   229,   230,   230,   231,   231,
     232,   232,   233,   234,   234,   235,   235,   236,   236,   237,
     237,   237,   238,   238,   239,   239,   240,   240,   241,   241,
     242,   242,   242,   243,   243,   243,   244,   244,   244,   245,
     245,   246,   247,   247,   248,   248,   249,   249,   250,   250,
     251,   251,   251,   252,   252,   254,   266,   267,   268,   268,
     268,   268,   268,   269,   271,   283,   284,   285,   285,   285,
     285,   286,   288,   302,   303,   304,   304,   304,   304,   305,
     305,   305,   307,   323,   324,   325,   325,   325,   325,   326,
     326,   326,   327,   329,   338,   347,   358,   367,   376,   385,
     396,   405,   416,   429,   444,   455,   472,   489,   502,   517,
     526,   535,   544,   553,   562,   571,   580,   589,   598,   607,
     616,   625,   634,   643,   652,   661,   668,   675,   684,   691,
     700,   708,   717,   726,   740,   749,   758,   767,   774,   781,
     807,   815,   822,   829,   836,   843,   851,   859,   867,   874,
     885,   896,   903,   912,   921,   930,   937,   944,   952,   960,
     970,   980,   990,  1003,  1014,  1022,  1035,  1044,  1053,  1062,
    1072,  1082,  1090,  1103,  1112,  1120,  1129,  1137,  1150,  1159,
    1166,  1176,  1186,  1196,  1206,  1216,  1226,  1236,  1246,  1253,
    1260,  1267,  1276,  1285,  1294,  1301,  1311,  1329,  1336,  1354,
    1367,  1380,  1389,  1398,  1407,  1416,  1426,  1436,  1447,  1456,
    1465,  1478,  1491,  1500,  1507,  1516,  1525,  1534,  1543,  1551,
    1564,  1572,  1601,  1608,  1623,  1633,  1643,  1650,  1657,  1666,
    1680,  1699,  1718,  1730,  1742,  1754,  1765,  1784,  1794,  1803,
    1811,  1819,  1832,  1845,  1858,  1871,  1880,  1889,  1898,  1907,
    1917,  1927,  1940,  1953,  1964,  1977,  1988,  2001,  2011,  2018,
    2025,  2034,  2044,  2054,  2064,  2071,  2078,  2087,  2097,  2107,
    2114,  2121,  2128,  2136,  2146,  2156,  2166,  2176,  2206,  2216,
    2224,  2233,  2248,  2257,  2262,  2263,  2264,  2264,  2264,  2265,
    2265,  2265,  2266,  2266,  2268,  2278,  2287,  2294,  2301,  2308,
    2315,  2322,  2329,  2334,  2335,  2336,  2336,  2337,  2337,  2338,
    2338,  2339,  2340,  2341,  2342,  2343,  2344,  2346,  2354,  2361,
    2369,  2377,  2384,  2391,  2400,  2409,  2418,  2427,  2436,  2445,
    2450,  2451,  2452,  2454,  2460,  2470,  2477,  2486,  2494,  2500,
    2501,  2503,  2503,  2503,  2504,  2504,  2505,  2506,  2507,  2508,
    2509,  2511,  2521,  2531,  2538,  2547,  2554,  2563,  2571,  2584,
    2592,  2605,  2610,  2611,  2612,  2612,  2613,  2613,  2613,  2615,
    2629,  2644,  2656,  2671
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
  "VAR_TCP_MSS", "VAR_OUTGOING_TCP_MSS", "VAR_CHROOT", "VAR_USERNAME",
  "VAR_DIRECTORY", "VAR_LOGFILE", "VAR_PIDFILE", "VAR_MSG_CACHE_SIZE",
  "VAR_MSG_CACHE_SLABS", "VAR_NUM_QUERIES_PER_THREAD",
  "VAR_RRSET_CACHE_SIZE", "VAR_RRSET_CACHE_SLABS", "VAR_OUTGOING_NUM_TCP",
  "VAR_INFRA_HOST_TTL", "VAR_INFRA_LAME_TTL", "VAR_INFRA_CACHE_SLABS",
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
  "VAR_LOG_REPLIES", "VAR_TCP_UPSTREAM", "VAR_SSL_UPSTREAM",
  "VAR_SSL_SERVICE_KEY", "VAR_SSL_SERVICE_PEM", "VAR_SSL_PORT",
  "VAR_FORWARD_FIRST", "VAR_STUB_SSL_UPSTREAM", "VAR_FORWARD_SSL_UPSTREAM",
  "VAR_TLS_CERT_BUNDLE", "VAR_STUB_FIRST", "VAR_MINIMAL_RESPONSES",
  "VAR_RRSET_ROUNDROBIN", "VAR_MAX_UDP_SIZE", "VAR_DELAY_CLOSE",
  "VAR_UNBLOCK_LAN_ZONES", "VAR_INSECURE_LAN_ZONES",
  "VAR_INFRA_CACHE_MIN_RTT", "VAR_DNS64_PREFIX", "VAR_DNS64_SYNTHALL",
  "VAR_DNSTAP", "VAR_DNSTAP_ENABLE", "VAR_DNSTAP_SOCKET_PATH",
  "VAR_DNSTAP_SEND_IDENTITY", "VAR_DNSTAP_SEND_VERSION",
  "VAR_DNSTAP_IDENTITY", "VAR_DNSTAP_VERSION",
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
  "VAR_CAPS_WHITELIST", "VAR_CACHE_MAX_NEGATIVE_TTL",
  "VAR_PERMIT_SMALL_HOLDDOWN", "VAR_QNAME_MINIMISATION",
  "VAR_QNAME_MINIMISATION_STRICT", "VAR_IP_FREEBIND", "VAR_DEFINE_TAG",
  "VAR_LOCAL_ZONE_TAG", "VAR_ACCESS_CONTROL_TAG",
  "VAR_LOCAL_ZONE_OVERRIDE", "VAR_ACCESS_CONTROL_TAG_ACTION",
  "VAR_ACCESS_CONTROL_TAG_DATA", "VAR_VIEW", "VAR_ACCESS_CONTROL_VIEW",
  "VAR_VIEW_FIRST", "VAR_SERVE_EXPIRED", "VAR_FAKE_DSA", "VAR_FAKE_SHA1",
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
  "VAR_ALLOW_NOTIFY", "VAR_TLS_WIN_CERT", "$accept", "toplevelvars",
  "toplevelvar", "serverstart", "contents_server", "content_server",
  "stubstart", "contents_stub", "content_stub", "forwardstart",
  "contents_forward", "content_forward", "viewstart", "contents_view",
  "content_view", "authstart", "contents_auth", "content_auth",
  "server_num_threads", "server_verbosity", "server_statistics_interval",
  "server_statistics_cumulative", "server_extended_statistics",
  "server_shm_enable", "server_shm_key", "server_port",
  "server_send_client_subnet", "server_client_subnet_zone",
  "server_client_subnet_always_forward", "server_client_subnet_opcode",
  "server_max_client_subnet_ipv4", "server_max_client_subnet_ipv6",
  "server_interface", "server_outgoing_interface", "server_outgoing_range",
  "server_outgoing_port_permit", "server_outgoing_port_avoid",
  "server_outgoing_num_tcp", "server_incoming_num_tcp",
  "server_interface_automatic", "server_do_ip4", "server_do_ip6",
  "server_do_udp", "server_do_tcp", "server_prefer_ip6", "server_tcp_mss",
  "server_outgoing_tcp_mss", "server_tcp_upstream",
  "server_udp_upstream_without_downstream", "server_ssl_upstream",
  "server_ssl_service_key", "server_ssl_service_pem", "server_ssl_port",
  "server_tls_cert_bundle", "server_tls_win_cert",
  "server_tls_additional_port", "server_use_systemd",
  "server_do_daemonize", "server_use_syslog", "server_log_time_ascii",
  "server_log_queries", "server_log_replies", "server_chroot",
  "server_username", "server_directory", "server_logfile",
  "server_pidfile", "server_root_hints", "server_dlv_anchor_file",
  "server_dlv_anchor", "server_auto_trust_anchor_file",
  "server_trust_anchor_file", "server_trusted_keys_file",
  "server_trust_anchor", "server_trust_anchor_signaling",
  "server_root_key_sentinel", "server_domain_insecure",
  "server_hide_identity", "server_hide_version", "server_hide_trustanchor",
  "server_identity", "server_version", "server_so_rcvbuf",
  "server_so_sndbuf", "server_so_reuseport", "server_ip_transparent",
  "server_ip_freebind", "server_edns_buffer_size",
  "server_msg_buffer_size", "server_msg_cache_size",
  "server_msg_cache_slabs", "server_num_queries_per_thread",
  "server_jostle_timeout", "server_delay_close",
  "server_unblock_lan_zones", "server_insecure_lan_zones",
  "server_rrset_cache_size", "server_rrset_cache_slabs",
  "server_infra_host_ttl", "server_infra_lame_ttl",
  "server_infra_cache_numhosts", "server_infra_cache_lame_size",
  "server_infra_cache_slabs", "server_infra_cache_min_rtt",
  "server_target_fetch_policy", "server_harden_short_bufsize",
  "server_harden_large_queries", "server_harden_glue",
  "server_harden_dnssec_stripped", "server_harden_below_nxdomain",
  "server_harden_referral_path", "server_harden_algo_downgrade",
  "server_use_caps_for_id", "server_caps_whitelist",
  "server_private_address", "server_private_domain", "server_prefetch",
  "server_prefetch_key", "server_unwanted_reply_threshold",
  "server_do_not_query_address", "server_do_not_query_localhost",
  "server_access_control", "server_module_conf",
  "server_val_override_date", "server_val_sig_skew_min",
  "server_val_sig_skew_max", "server_cache_max_ttl",
  "server_cache_max_negative_ttl", "server_cache_min_ttl",
  "server_bogus_ttl", "server_val_clean_additional",
  "server_val_permissive_mode", "server_aggressive_nsec",
  "server_ignore_cd_flag", "server_serve_expired", "server_fake_dsa",
  "server_fake_sha1", "server_val_log_level",
  "server_val_nsec3_keysize_iterations", "server_add_holddown",
  "server_del_holddown", "server_keep_missing",
  "server_permit_small_holddown", "server_key_cache_size",
  "server_key_cache_slabs", "server_neg_cache_size", "server_local_zone",
  "server_local_data", "server_local_data_ptr", "server_minimal_responses",
  "server_rrset_roundrobin", "server_max_udp_size", "server_dns64_prefix",
  "server_dns64_synthall", "server_define_tag", "server_local_zone_tag",
  "server_access_control_tag", "server_access_control_tag_action",
  "server_access_control_tag_data", "server_local_zone_override",
  "server_access_control_view", "server_response_ip_tag",
  "server_ip_ratelimit", "server_ratelimit", "server_ip_ratelimit_size",
  "server_ratelimit_size", "server_ip_ratelimit_slabs",
  "server_ratelimit_slabs", "server_ratelimit_for_domain",
  "server_ratelimit_below_domain", "server_ip_ratelimit_factor",
  "server_ratelimit_factor", "server_low_rtt", "server_low_rtt_permil",
  "server_qname_minimisation", "server_qname_minimisation_strict",
  "server_ipsecmod_enabled", "server_ipsecmod_ignore_bogus",
  "server_ipsecmod_hook", "server_ipsecmod_max_ttl",
  "server_ipsecmod_whitelist", "server_ipsecmod_strict", "stub_name",
  "stub_host", "stub_addr", "stub_first", "stub_ssl_upstream",
  "stub_prime", "forward_name", "forward_host", "forward_addr",
  "forward_first", "forward_ssl_upstream", "auth_name", "auth_zonefile",
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
  "redis_server_port", "redis_timeout", YY_NULLPTR
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
     495,   496,   497
};
# endif

#define YYPACT_NINF -184

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-184)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -184,     0,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,   221,   -37,   -32,   -38,   -39,   -62,
    -129,  -104,  -160,  -183,    -3,    -1,     2,     3,     4,     5,
      30,    48,    49,    50,    51,    52,    54,    55,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    82,    83,    84,    85,    87,    88,    91,    93,
      94,    95,    96,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   148,
     149,   150,   151,   152,   153,   154,   155,   156,   157,   158,
     159,   160,   161,   162,   163,   164,   165,   166,   167,   168,
     169,   170,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,   181,   189,   190,   191,   193,   194,   195,   196,
     198,   199,   200,   201,   202,   203,   204,   205,   206,   207,
     208,   209,   210,   211,   212,   213,   215,   216,   217,   218,
     219,   220,   251,   252,   253,   254,   258,   259,   260,   302,
     303,   304,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,   305,   306,   307,   308,   309,   310,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,   314,   318,   319,   343,   344,  -184,
    -184,  -184,  -184,  -184,  -184,   345,   347,   357,   358,   359,
     360,   361,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
     362,   363,   364,   365,   366,   367,   368,   369,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,   404,   406,   418,
     419,   420,   421,   422,   423,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,   424,   425,   426,   427,   428,   435,
     436,   437,   438,   439,   440,   442,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,   443,
    -184,  -184,   444,   445,   446,   447,   448,   452,   454,   455,
     456,   457,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,   458,   459,   460,   461,   462,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
     463,   464,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,   465,   466,   467,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,   468,   469,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,   470,   471,   472,   473,   474,   475,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,   476,  -184,  -184,   477,   478,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,   479,   480,   481,  -184,  -184,  -184,  -184,
    -184,  -184,  -184
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     1,    14,   185,   194,   413,   459,   432,   202,
     468,   491,   212,     3,    16,   187,   196,   204,   214,   415,
     434,   461,   470,   493,     4,     5,     6,    10,    13,     8,
       9,     7,    11,    12,     0,     0,     0,     0,     0,     0,
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
       0,     0,    15,    17,    18,    77,    80,    89,   166,   167,
      19,   139,   140,   141,   142,   143,   144,    28,    68,    20,
      81,    82,    39,    61,    76,    21,    22,    24,    25,    23,
      26,    27,   112,   178,   113,   114,   115,   116,   180,   184,
     181,   162,    78,    67,    93,   110,   111,    29,    30,    31,
      32,    33,    69,    83,    84,    99,    55,    65,    56,   170,
     171,    94,    49,    50,   169,    51,    52,   103,   107,   120,
     128,   149,   104,    62,    34,    35,    36,    91,   121,   122,
     123,    37,    38,    40,    41,    43,    44,    42,   126,    45,
      46,    47,    53,    72,   108,    86,   127,    79,   145,    87,
      88,   105,   106,    92,    48,    70,    73,    54,    57,    95,
      96,    71,   146,    97,    58,    59,    60,   179,   109,   159,
     160,   168,    98,    66,   100,   101,   102,   147,    63,    64,
      85,    74,    75,    90,   117,   118,   119,   124,   125,   150,
     151,   153,   155,   156,   154,   157,   163,   129,   130,   133,
     134,   131,   132,   135,   136,   138,   137,   182,   183,   148,
     158,   172,   174,   173,   175,   176,   177,   152,   161,   164,
     165,     0,     0,     0,     0,     0,     0,   186,   188,   189,
     190,   192,   193,   191,     0,     0,     0,     0,     0,   195,
     197,   198,   199,   200,   201,     0,     0,     0,     0,     0,
       0,     0,   203,   205,   206,   209,   210,   207,   211,   208,
       0,     0,     0,     0,     0,     0,     0,     0,   213,   215,
     216,   217,   218,   222,   219,   220,   221,     0,     0,     0,
       0,     0,     0,     0,     0,   414,   416,   418,   417,   423,
     419,   420,   421,   422,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   433,   435,   436,   437,
     438,   439,   440,   441,   442,   443,   444,   445,   446,     0,
     460,   462,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   469,   471,   472,   473,   475,   476,   474,   477,
     478,   479,   480,     0,     0,     0,     0,     0,   492,   494,
     495,   496,   497,   498,   224,   223,   230,   239,   237,   245,
     246,   249,   247,   248,   250,   251,   267,   268,   269,   270,
     271,   294,   295,   296,   301,   302,   242,   303,   304,   307,
     305,   306,   309,   310,   311,   324,   282,   283,   285,   286,
     312,   327,   276,   278,   328,   334,   335,   336,   243,   293,
     348,   349,   277,   343,   263,   238,   272,   325,   331,   313,
       0,     0,   352,   244,   225,   262,   317,   226,   240,   241,
     273,   274,   350,   315,   319,   320,   227,   353,   297,   323,
     264,   281,   329,   330,   333,   342,   275,   346,   344,   345,
     287,   292,   321,   322,   288,   289,   314,   338,   265,   266,
     252,   254,   255,   256,   257,   258,   354,   355,   356,   298,
     299,   300,   308,   357,   358,     0,     0,     0,   316,   290,
     464,   367,   371,   369,   368,   372,   370,     0,     0,   375,
     376,   231,   232,   233,   234,   235,   236,   318,   332,   347,
     379,   380,   291,   359,     0,     0,     0,     0,     0,     0,
     339,   340,   341,   465,   284,   279,   337,   261,   228,   229,
     280,   381,   383,   382,   384,   385,   386,   253,   260,   377,
     378,   259,   387,   388,   389,   392,   391,   390,   393,   394,
     395,   396,   397,   406,     0,   410,   411,     0,     0,   412,
     398,   404,   399,   400,   401,   403,   405,   402,   424,   426,
     425,   428,   429,   430,   431,   427,   447,   448,   449,   450,
     451,   452,   453,   454,   455,   456,   457,   458,   463,   481,
     482,   483,   486,   484,   485,   487,   488,   489,   490,   499,
     500,   501,   502,   503,   326,   351,   366,   466,   467,   373,
     374,   360,   361,     0,     0,     0,   365,   407,   408,   409,
     364,   362,   363
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,  -184,
    -184,  -184,  -184
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,    13,    14,    24,   202,    15,    25,   377,    16,
      26,   389,    17,    27,   402,    18,    28,   418,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222,   223,   224,
     225,   226,   227,   228,   229,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   259,   260,   261,   262,   263,   264,
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
     365,   366,   378,   379,   380,   381,   382,   383,   390,   391,
     392,   393,   394,   419,   420,   421,   422,   423,   424,   425,
     426,   403,   404,   405,   406,   407,   408,   409,    19,    29,
     435,   436,   437,   438,   439,   440,   441,   442,   443,    20,
      30,   456,   457,   458,   459,   460,   461,   462,   463,   464,
     465,   466,   467,   468,    21,    31,   470,   471,   367,   368,
     369,   370,    22,    32,   482,   483,   484,   485,   486,   487,
     488,   489,   490,   491,   492,    23,    33,   498,   499,   500,
     501,   502,   503
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint16 yytable[] =
{
       2,   410,   395,   371,   469,   372,   373,   504,   384,   505,
       0,     3,   506,   507,   508,   509,   385,   386,   444,   445,
     446,   447,   448,   449,   450,   451,   452,   453,   454,   455,
     427,   428,   429,   430,   431,   432,   433,   434,   396,   397,
     510,     4,   493,   494,   495,   496,   497,     5,   472,   473,
     474,   475,   476,   477,   478,   479,   480,   481,   511,   512,
     513,   514,   515,   398,   516,   517,   374,   518,   519,   520,
     521,   522,   523,   524,   525,   526,   527,   528,   529,   530,
     531,   532,   533,   534,   535,   536,   537,   538,   539,   540,
     541,     6,   542,   543,   544,   545,   375,   546,   547,   376,
     387,   548,   388,   549,   550,   551,   552,     7,   553,   554,
     555,   556,   557,   558,   559,   560,   561,   562,   563,   564,
     565,   566,   399,   400,   567,   568,   569,   570,   571,   572,
     573,   574,   575,   576,   577,   578,   579,   580,   581,   582,
     583,   584,   585,   586,   587,   588,     8,   589,   590,   591,
     592,   593,   594,   595,   596,   597,   598,   401,   599,   600,
     601,   602,   603,   604,   605,   606,   607,   608,   609,   610,
     611,   612,   613,   614,   615,   616,   617,   618,   619,   620,
     621,   622,   623,   624,   625,   626,   627,   628,   629,   630,
     631,   632,   411,     9,   412,   413,   414,   415,   416,   633,
     634,   635,   417,   636,   637,   638,   639,    10,   640,   641,
     642,   643,   644,   645,   646,   647,   648,   649,   650,   651,
     652,   653,   654,   655,    11,   656,   657,   658,   659,   660,
     661,     0,    12,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,   662,   663,   664,   665,    62,    63,    64,   666,   667,
     668,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   669,   670,   671,   672,   673,   674,   675,   676,
     677,   106,   107,   108,   678,   109,   110,   111,   679,   680,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   681,   682,   683,   135,   684,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   685,   686,   687,
     688,   689,   690,   691,   692,   693,   694,   695,   696,   697,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   698,   179,   699,   180,   181,   182,
     183,   184,   185,   186,   187,   188,   189,   190,   700,   701,
     702,   703,   704,   705,   706,   707,   708,   709,   710,   191,
     192,   193,   194,   195,   196,   711,   712,   713,   714,   715,
     716,   197,   717,   718,   719,   720,   721,   722,   723,   198,
     199,   200,   724,   201,   725,   726,   727,   728,   729,   730,
     731,   732,   733,   734,   735,   736,   737,   738,   739,   740,
     741,   742,   743,   744,   745,   746,   747,   748,   749,   750,
     751,   752
};

static const yytype_int16 yycheck[] =
{
       0,    40,    40,    40,   108,    42,    43,    10,    40,    10,
      -1,    11,    10,    10,    10,    10,    48,    49,   147,   148,
     149,   150,   151,   152,   153,   154,   155,   156,   157,   158,
      92,    93,    94,    95,    96,    97,    98,    99,    76,    77,
      10,    41,   225,   226,   227,   228,   229,    47,   208,   209,
     210,   211,   212,   213,   214,   215,   216,   217,    10,    10,
      10,    10,    10,   101,    10,    10,   103,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    91,    10,    10,    10,    10,   133,    10,    10,   136,
     132,    10,   134,    10,    10,    10,    10,   107,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,   160,   161,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,   146,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,   195,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,   231,   193,   233,   234,   235,   236,   237,    10,
      10,    10,   241,    10,    10,    10,    10,   207,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,   224,    10,    10,    10,    10,    10,
      10,    -1,   232,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    10,    10,    10,    10,    44,    45,    46,    10,    10,
      10,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    10,    10,    10,    10,    10,    10,    10,    10,
      10,   100,   101,   102,    10,   104,   105,   106,    10,    10,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,    10,    10,    10,   135,    10,   137,   138,
     139,   140,   141,   142,   143,   144,   145,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
     159,   160,   161,   162,   163,   164,   165,   166,   167,   168,
     169,   170,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,   181,   182,   183,   184,   185,   186,   187,   188,
     189,   190,   191,   192,    10,   194,    10,   196,   197,   198,
     199,   200,   201,   202,   203,   204,   205,   206,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,   218,
     219,   220,   221,   222,   223,    10,    10,    10,    10,    10,
      10,   230,    10,    10,    10,    10,    10,    10,    10,   238,
     239,   240,    10,   242,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   244,     0,    11,    41,    47,    91,   107,   146,   193,
     207,   224,   232,   245,   246,   249,   252,   255,   258,   451,
     462,   477,   485,   498,   247,   250,   253,   256,   259,   452,
     463,   478,   486,   499,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    44,    45,    46,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,   100,   101,   102,   104,
     105,   106,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   135,   137,   138,   139,   140,
     141,   142,   143,   144,   145,   159,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,   189,   190,   191,   192,   194,
     196,   197,   198,   199,   200,   201,   202,   203,   204,   205,
     206,   218,   219,   220,   221,   222,   223,   230,   238,   239,
     240,   242,   248,   261,   262,   263,   264,   265,   266,   267,
     268,   269,   270,   271,   272,   273,   274,   275,   276,   277,
     278,   279,   280,   281,   282,   283,   284,   285,   286,   287,
     288,   289,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,   302,   303,   304,   305,   306,   307,
     308,   309,   310,   311,   312,   313,   314,   315,   316,   317,
     318,   319,   320,   321,   322,   323,   324,   325,   326,   327,
     328,   329,   330,   331,   332,   333,   334,   335,   336,   337,
     338,   339,   340,   341,   342,   343,   344,   345,   346,   347,
     348,   349,   350,   351,   352,   353,   354,   355,   356,   357,
     358,   359,   360,   361,   362,   363,   364,   365,   366,   367,
     368,   369,   370,   371,   372,   373,   374,   375,   376,   377,
     378,   379,   380,   381,   382,   383,   384,   385,   386,   387,
     388,   389,   390,   391,   392,   393,   394,   395,   396,   397,
     398,   399,   400,   401,   402,   403,   404,   405,   406,   407,
     408,   409,   410,   411,   412,   413,   414,   415,   416,   417,
     418,   419,   420,   421,   422,   423,   424,   481,   482,   483,
     484,    40,    42,    43,   103,   133,   136,   251,   425,   426,
     427,   428,   429,   430,    40,    48,    49,   132,   134,   254,
     431,   432,   433,   434,   435,    40,    76,    77,   101,   160,
     161,   195,   257,   444,   445,   446,   447,   448,   449,   450,
      40,   231,   233,   234,   235,   236,   237,   241,   260,   436,
     437,   438,   439,   440,   441,   442,   443,    92,    93,    94,
      95,    96,    97,    98,    99,   453,   454,   455,   456,   457,
     458,   459,   460,   461,   147,   148,   149,   150,   151,   152,
     153,   154,   155,   156,   157,   158,   464,   465,   466,   467,
     468,   469,   470,   471,   472,   473,   474,   475,   476,   108,
     479,   480,   208,   209,   210,   211,   212,   213,   214,   215,
     216,   217,   487,   488,   489,   490,   491,   492,   493,   494,
     495,   496,   497,   225,   226,   227,   228,   229,   500,   501,
     502,   503,   504,   505,    10,    10,    10,    10,    10,    10,
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
       0,   243,   244,   244,   245,   245,   245,   245,   245,   245,
     245,   245,   245,   245,   246,   247,   247,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   248,   248,   248,   249,   250,   250,   251,   251,
     251,   251,   251,   251,   252,   253,   253,   254,   254,   254,
     254,   254,   255,   256,   256,   257,   257,   257,   257,   257,
     257,   257,   258,   259,   259,   260,   260,   260,   260,   260,
     260,   260,   260,   261,   262,   263,   264,   265,   266,   267,
     268,   269,   270,   271,   272,   273,   274,   275,   276,   277,
     278,   279,   280,   281,   282,   283,   284,   285,   286,   287,
     288,   289,   290,   291,   292,   293,   294,   295,   296,   297,
     298,   299,   300,   301,   302,   303,   304,   305,   306,   307,
     308,   309,   310,   311,   312,   313,   314,   315,   316,   317,
     318,   319,   320,   321,   322,   323,   324,   325,   326,   327,
     328,   329,   330,   331,   332,   333,   334,   335,   336,   337,
     338,   339,   340,   341,   342,   343,   344,   345,   346,   347,
     348,   349,   350,   351,   352,   353,   354,   355,   356,   357,
     358,   359,   360,   361,   362,   363,   364,   365,   366,   367,
     368,   369,   370,   371,   372,   373,   374,   375,   376,   377,
     378,   379,   380,   381,   382,   383,   384,   385,   386,   387,
     388,   389,   390,   391,   392,   393,   394,   395,   396,   397,
     398,   399,   400,   401,   402,   403,   404,   405,   406,   407,
     408,   409,   410,   411,   412,   413,   414,   415,   416,   417,
     418,   419,   420,   421,   422,   423,   424,   425,   426,   427,
     428,   429,   430,   431,   432,   433,   434,   435,   436,   437,
     438,   439,   440,   441,   442,   443,   444,   445,   446,   447,
     448,   449,   450,   451,   452,   452,   453,   453,   453,   453,
     453,   453,   453,   453,   454,   455,   456,   457,   458,   459,
     460,   461,   462,   463,   463,   464,   464,   464,   464,   464,
     464,   464,   464,   464,   464,   464,   464,   465,   466,   467,
     468,   469,   470,   471,   472,   473,   474,   475,   476,   477,
     478,   478,   479,   480,   481,   482,   483,   484,   485,   486,
     486,   487,   487,   487,   487,   487,   487,   487,   487,   487,
     487,   488,   489,   490,   491,   492,   493,   494,   495,   496,
     497,   498,   499,   499,   500,   500,   500,   500,   500,   501,
     502,   503,   504,   505
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     1,     2,     0,     1,     1,     1,
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
       1,     1,     1,     1,     1,     1,     2,     0,     1,     1,
       1,     1,     1,     1,     1,     2,     0,     1,     1,     1,
       1,     1,     1,     2,     0,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     0,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     2,     2,     2,     2,     2,     2,
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
       2,     3,     2,     2,     2,     2,     2,     2,     2,     2,
       3,     3,     4,     4,     4,     3,     3,     2,     2,     2,
       2,     2,     2,     3,     3,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     3,     3,     3,
       2,     2,     2,     1,     2,     0,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     2,     2,     2,     2,     2,
       2,     2,     1,     2,     0,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     1,
       2,     0,     1,     2,     2,     2,     3,     3,     1,     2,
       0,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     1,     2,     0,     1,     1,     1,     1,     1,     2,
       2,     2,     2,     2
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
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


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
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
  unsigned long int yylno = yyrline[yyrule];
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
                       &(yyvsp[(yyi + 1) - (yynrhs)])
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
            /* Fall through.  */
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

  return yystpcpy (yyres, yystr) - yyres;
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
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
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
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
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
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
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
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
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
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

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
| yyreduce -- Do a reduction.  |
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
        case 14:
#line 173 "util/configparser.y" /* yacc.c:1646  */
    { 
		OUTYY(("\nP(server:)\n")); 
	}
#line 2473 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 185:
#line 255 "util/configparser.y" /* yacc.c:1646  */
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
#line 2488 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 194:
#line 272 "util/configparser.y" /* yacc.c:1646  */
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
#line 2503 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 202:
#line 289 "util/configparser.y" /* yacc.c:1646  */
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
#line 2520 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 212:
#line 308 "util/configparser.y" /* yacc.c:1646  */
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
		} else 
			yyerror("out of memory");
	}
#line 2539 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 223:
#line 330 "util/configparser.y" /* yacc.c:1646  */
    { 
		OUTYY(("P(server_num_threads:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->num_threads = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2551 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 224:
#line 339 "util/configparser.y" /* yacc.c:1646  */
    { 
		OUTYY(("P(server_verbosity:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->verbosity = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2563 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 225:
#line 348 "util/configparser.y" /* yacc.c:1646  */
    { 
		OUTYY(("P(server_statistics_interval:%s)\n", (yyvsp[0].str))); 
		if(strcmp((yyvsp[0].str), "") == 0 || strcmp((yyvsp[0].str), "0") == 0)
			cfg_parser->cfg->stat_interval = 0;
		else if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->stat_interval = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2577 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 226:
#line 359 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_statistics_cumulative:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_cumulative = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2589 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 227:
#line 368 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_extended_statistics:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_extended = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2601 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 228:
#line 377 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_shm_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->shm_enable = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2613 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 229:
#line 386 "util/configparser.y" /* yacc.c:1646  */
    { 
		OUTYY(("P(server_shm_key:%s)\n", (yyvsp[0].str))); 
		if(strcmp((yyvsp[0].str), "") == 0 || strcmp((yyvsp[0].str), "0") == 0)
			cfg_parser->cfg->shm_key = 0;
		else if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->shm_key = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2627 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 230:
#line 397 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2639 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 231:
#line 406 "util/configparser.y" /* yacc.c:1646  */
    {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(server_send_client_subnet:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->client_subnet, (yyvsp[0].str)))
			fatal_exit("out of memory adding client-subnet");
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
	}
#line 2653 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 232:
#line 417 "util/configparser.y" /* yacc.c:1646  */
    {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(server_client_subnet_zone:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->client_subnet_zone,
			(yyvsp[0].str)))
			fatal_exit("out of memory adding client-subnet-zone");
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
	}
#line 2668 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 233:
#line 430 "util/configparser.y" /* yacc.c:1646  */
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
#line 2686 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 234:
#line 445 "util/configparser.y" /* yacc.c:1646  */
    {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(client_subnet_opcode:%s)\n", (yyvsp[0].str)));
		OUTYY(("P(Deprecated option, ignoring)\n"));
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 2700 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 235:
#line 456 "util/configparser.y" /* yacc.c:1646  */
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
#line 2720 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 236:
#line 473 "util/configparser.y" /* yacc.c:1646  */
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
#line 2740 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 237:
#line 490 "util/configparser.y" /* yacc.c:1646  */
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
#line 2756 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 238:
#line 503 "util/configparser.y" /* yacc.c:1646  */
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
#line 2774 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 239:
#line 518 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_outgoing_range:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_num_ports = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2786 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 240:
#line 527 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_outgoing_port_permit:%s)\n", (yyvsp[0].str)));
		if(!cfg_mark_ports((yyvsp[0].str), 1, 
			cfg_parser->cfg->outgoing_avail_ports, 65536))
			yyerror("port number or range (\"low-high\") expected");
		free((yyvsp[0].str));
	}
#line 2798 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 241:
#line 536 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_outgoing_port_avoid:%s)\n", (yyvsp[0].str)));
		if(!cfg_mark_ports((yyvsp[0].str), 0, 
			cfg_parser->cfg->outgoing_avail_ports, 65536))
			yyerror("port number or range (\"low-high\") expected");
		free((yyvsp[0].str));
	}
#line 2810 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 242:
#line 545 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_outgoing_num_tcp:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_num_tcp = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2822 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 243:
#line 554 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_incoming_num_tcp:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->incoming_num_tcp = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2834 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 244:
#line 563 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_interface_automatic:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->if_automatic = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2846 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 245:
#line 572 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_do_ip4:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_ip4 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2858 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 246:
#line 581 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_do_ip6:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_ip6 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2870 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 247:
#line 590 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_do_udp:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_udp = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2882 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 248:
#line 599 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_do_tcp:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_tcp = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2894 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 249:
#line 608 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_prefer_ip6:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefer_ip6 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2906 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 250:
#line 617 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_tcp_mss:%s)\n", (yyvsp[0].str)));
                if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
                        yyerror("number expected");
                else cfg_parser->cfg->tcp_mss = atoi((yyvsp[0].str));
                free((yyvsp[0].str));
	}
#line 2918 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 251:
#line 626 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_outgoing_tcp_mss:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_tcp_mss = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2930 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 252:
#line 635 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_tcp_upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tcp_upstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2942 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 253:
#line 644 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_udp_upstream_without_downstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->udp_upstream_without_downstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2954 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 254:
#line 653 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_ssl_upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ssl_upstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 2966 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 255:
#line 662 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_ssl_service_key:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->ssl_service_key);
		cfg_parser->cfg->ssl_service_key = (yyvsp[0].str);
	}
#line 2976 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 256:
#line 669 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_ssl_service_pem:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->ssl_service_pem);
		cfg_parser->cfg->ssl_service_pem = (yyvsp[0].str);
	}
#line 2986 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 257:
#line 676 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_ssl_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->ssl_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 2998 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 258:
#line 685 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_tls_cert_bundle:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->tls_cert_bundle);
		cfg_parser->cfg->tls_cert_bundle = (yyvsp[0].str);
	}
#line 3008 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 259:
#line 692 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_tls_win_cert:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tls_win_cert = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3020 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 260:
#line 701 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_tls_additional_port:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->tls_additional_port,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3031 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 261:
#line 709 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_use_systemd:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->use_systemd = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3043 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 262:
#line 718 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_do_daemonize:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_daemonize = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3055 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 263:
#line 727 "util/configparser.y" /* yacc.c:1646  */
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
#line 3072 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 264:
#line 741 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_log_time_ascii:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_time_ascii = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3084 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 265:
#line 750 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_log_queries:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_queries = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3096 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 266:
#line 759 "util/configparser.y" /* yacc.c:1646  */
    {
  	OUTYY(("P(server_log_replies:%s)\n", (yyvsp[0].str)));
  	if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
  		yyerror("expected yes or no.");
  	else cfg_parser->cfg->log_replies = (strcmp((yyvsp[0].str), "yes")==0);
  	free((yyvsp[0].str));
  }
#line 3108 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 267:
#line 768 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_chroot:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->chrootdir);
		cfg_parser->cfg->chrootdir = (yyvsp[0].str);
	}
#line 3118 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 268:
#line 775 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_username:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->username);
		cfg_parser->cfg->username = (yyvsp[0].str);
	}
#line 3128 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 269:
#line 782 "util/configparser.y" /* yacc.c:1646  */
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
#line 3157 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 270:
#line 808 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_logfile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->logfile);
		cfg_parser->cfg->logfile = (yyvsp[0].str);
		cfg_parser->cfg->use_syslog = 0;
	}
#line 3168 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 271:
#line 816 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_pidfile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->pidfile);
		cfg_parser->cfg->pidfile = (yyvsp[0].str);
	}
#line 3178 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 272:
#line 823 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_root_hints:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->root_hints, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3188 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 273:
#line 830 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_dlv_anchor_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dlv_anchor_file);
		cfg_parser->cfg->dlv_anchor_file = (yyvsp[0].str);
	}
#line 3198 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 274:
#line 837 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_dlv_anchor:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dlv_anchor_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3208 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 275:
#line 844 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_auto_trust_anchor_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			auto_trust_anchor_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3219 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 276:
#line 852 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_trust_anchor_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			trust_anchor_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3230 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 277:
#line 860 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_trusted_keys_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			trusted_keys_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3241 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 278:
#line 868 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_trust_anchor:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->trust_anchor_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3251 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 279:
#line 875 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_trust_anchor_signaling:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->trust_anchor_signaling =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3265 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 280:
#line 886 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_root_key_sentinel:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->root_key_sentinel =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3279 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 281:
#line 897 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_domain_insecure:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->domain_insecure, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3289 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 282:
#line 904 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_hide_identity:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_identity = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3301 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 283:
#line 913 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_hide_version:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_version = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3313 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 284:
#line 922 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_hide_trustanchor:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_trustanchor = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3325 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 285:
#line 931 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->identity);
		cfg_parser->cfg->identity = (yyvsp[0].str);
	}
#line 3335 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 286:
#line 938 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_version:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->version);
		cfg_parser->cfg->version = (yyvsp[0].str);
	}
#line 3345 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 287:
#line 945 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_so_rcvbuf:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->so_rcvbuf))
			yyerror("buffer size expected");
		free((yyvsp[0].str));
	}
#line 3356 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 288:
#line 953 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_so_sndbuf:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->so_sndbuf))
			yyerror("buffer size expected");
		free((yyvsp[0].str));
	}
#line 3367 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 289:
#line 961 "util/configparser.y" /* yacc.c:1646  */
    {
        OUTYY(("P(server_so_reuseport:%s)\n", (yyvsp[0].str)));
        if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
            yyerror("expected yes or no.");
        else cfg_parser->cfg->so_reuseport =
            (strcmp((yyvsp[0].str), "yes")==0);
        free((yyvsp[0].str));
    }
#line 3380 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 290:
#line 971 "util/configparser.y" /* yacc.c:1646  */
    {
        OUTYY(("P(server_ip_transparent:%s)\n", (yyvsp[0].str)));
        if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
            yyerror("expected yes or no.");
        else cfg_parser->cfg->ip_transparent =
            (strcmp((yyvsp[0].str), "yes")==0);
        free((yyvsp[0].str));
    }
#line 3393 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 291:
#line 981 "util/configparser.y" /* yacc.c:1646  */
    {
        OUTYY(("P(server_ip_freebind:%s)\n", (yyvsp[0].str)));
        if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
            yyerror("expected yes or no.");
        else cfg_parser->cfg->ip_freebind =
            (strcmp((yyvsp[0].str), "yes")==0);
        free((yyvsp[0].str));
    }
#line 3406 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 292:
#line 991 "util/configparser.y" /* yacc.c:1646  */
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
#line 3422 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 293:
#line 1004 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_msg_buffer_size:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 4096)
			yyerror("message buffer size too small (use 4096)");
		else cfg_parser->cfg->msg_buffer_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3436 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 294:
#line 1015 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_msg_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->msg_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 3447 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 295:
#line 1023 "util/configparser.y" /* yacc.c:1646  */
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
#line 3463 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 296:
#line 1036 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_num_queries_per_thread:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->num_queries_per_thread = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3475 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 297:
#line 1045 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_jostle_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->jostle_time = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3487 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 298:
#line 1054 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_delay_close:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->delay_close = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3499 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 299:
#line 1063 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_unblock_lan_zones:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->unblock_lan_zones = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3512 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 300:
#line 1073 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_insecure_lan_zones:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->insecure_lan_zones = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3525 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 301:
#line 1083 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_rrset_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->rrset_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 3536 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 302:
#line 1091 "util/configparser.y" /* yacc.c:1646  */
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
#line 3552 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 303:
#line 1104 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_infra_host_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->host_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3564 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 304:
#line 1113 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_infra_lame_ttl:%s)\n", (yyvsp[0].str)));
		verbose(VERB_DETAIL, "ignored infra-lame-ttl: %s (option "
			"removed, use infra-host-ttl)", (yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3575 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 305:
#line 1121 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_infra_cache_numhosts:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_numhosts = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3587 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 306:
#line 1130 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_infra_cache_lame_size:%s)\n", (yyvsp[0].str)));
		verbose(VERB_DETAIL, "ignored infra-cache-lame-size: %s "
			"(option removed, use infra-cache-numhosts)", (yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3598 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 307:
#line 1138 "util/configparser.y" /* yacc.c:1646  */
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
#line 3614 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 308:
#line 1151 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_infra_cache_min_rtt:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_min_rtt = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3626 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 309:
#line 1160 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_target_fetch_policy:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->target_fetch_policy);
		cfg_parser->cfg->target_fetch_policy = (yyvsp[0].str);
	}
#line 3636 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 310:
#line 1167 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_harden_short_bufsize:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_short_bufsize = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3649 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 311:
#line 1177 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_harden_large_queries:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_large_queries = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3662 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 312:
#line 1187 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_harden_glue:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_glue = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3675 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 313:
#line 1197 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_harden_dnssec_stripped:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_dnssec_stripped = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3688 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 314:
#line 1207 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_harden_below_nxdomain:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_below_nxdomain = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3701 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 315:
#line 1217 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_harden_referral_path:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_referral_path = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3714 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 316:
#line 1227 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_harden_algo_downgrade:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_algo_downgrade = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3727 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 317:
#line 1237 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_use_caps_for_id:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->use_caps_bits_for_id = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3740 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 318:
#line 1247 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_caps_whitelist:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->caps_whitelist, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3750 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 319:
#line 1254 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_private_address:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->private_address, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3760 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 320:
#line 1261 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_private_domain:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->private_domain, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3770 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 321:
#line 1268 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_prefetch:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefetch = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3782 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 322:
#line 1277 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_prefetch_key:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefetch_key = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3794 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 323:
#line 1286 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_unwanted_reply_threshold:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->unwanted_threshold = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3806 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 324:
#line 1295 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_do_not_query_address:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->donotqueryaddrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3816 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 325:
#line 1302 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_do_not_query_localhost:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->donotquery_localhost = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3829 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 326:
#line 1312 "util/configparser.y" /* yacc.c:1646  */
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
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->acls, (yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding acl");
		}
	}
#line 3850 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 327:
#line 1330 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_module_conf:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->module_conf);
		cfg_parser->cfg->module_conf = (yyvsp[0].str);
	}
#line 3860 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 328:
#line 1337 "util/configparser.y" /* yacc.c:1646  */
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
#line 3881 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 329:
#line 1355 "util/configparser.y" /* yacc.c:1646  */
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
#line 3897 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 330:
#line 1368 "util/configparser.y" /* yacc.c:1646  */
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
#line 3913 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 331:
#line 1381 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_cache_max_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3925 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 332:
#line 1390 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_cache_max_negative_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_negative_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3937 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 333:
#line 1399 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_cache_min_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->min_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3949 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 334:
#line 1408 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_bogus_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->bogus_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3961 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 335:
#line 1417 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_val_clean_additional:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->val_clean_additional = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3974 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 336:
#line 1427 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_val_permissive_mode:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->val_permissive_mode = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3987 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 337:
#line 1437 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_aggressive_nsec:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->aggressive_nsec =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4001 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 338:
#line 1448 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_ignore_cd_flag:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ignore_cd = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4013 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 339:
#line 1457 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_serve_expired:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->serve_expired = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4025 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 340:
#line 1466 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_fake_dsa:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
#ifdef HAVE_SSL
		else fake_dsa = (strcmp((yyvsp[0].str), "yes")==0);
		if(fake_dsa)
			log_warn("test option fake_dsa is enabled");
#endif
		free((yyvsp[0].str));
	}
#line 4041 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 341:
#line 1479 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_fake_sha1:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
#ifdef HAVE_SSL
		else fake_sha1 = (strcmp((yyvsp[0].str), "yes")==0);
		if(fake_sha1)
			log_warn("test option fake_sha1 is enabled");
#endif
		free((yyvsp[0].str));
	}
#line 4057 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 342:
#line 1492 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_val_log_level:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->val_log_level = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4069 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 343:
#line 1501 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_val_nsec3_keysize_iterations:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->val_nsec3_key_iterations);
		cfg_parser->cfg->val_nsec3_key_iterations = (yyvsp[0].str);
	}
#line 4079 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 344:
#line 1508 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_add_holddown:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->add_holddown = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4091 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 345:
#line 1517 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_del_holddown:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->del_holddown = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4103 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 346:
#line 1526 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_keep_missing:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->keep_missing = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4115 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 347:
#line 1535 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_permit_small_holddown:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->permit_small_holddown =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4128 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 348:
#line 1544 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_key_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->key_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4139 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 349:
#line 1552 "util/configparser.y" /* yacc.c:1646  */
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
#line 4155 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 350:
#line 1565 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_neg_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->neg_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4166 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 351:
#line 1573 "util/configparser.y" /* yacc.c:1646  */
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
		   && strcmp((yyvsp[0].str), "inform")!=0 && strcmp((yyvsp[0].str), "inform_deny")!=0)
			yyerror("local-zone type: expected static, deny, "
				"refuse, redirect, transparent, "
				"typetransparent, inform, inform_deny, "
				"always_transparent, always_refuse, "
				"always_nxdomain, noview or nodefault");
		else if(strcmp((yyvsp[0].str), "nodefault")==0) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->
				local_zones_nodefault, (yyvsp[-1].str)))
				fatal_exit("out of memory adding local-zone");
			free((yyvsp[0].str));
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->local_zones, 
				(yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding local-zone");
		}
	}
#line 4198 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 352:
#line 1602 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_local_data:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->local_data, (yyvsp[0].str)))
			fatal_exit("out of memory adding local-data");
	}
#line 4208 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 353:
#line 1609 "util/configparser.y" /* yacc.c:1646  */
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
#line 4226 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 354:
#line 1624 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_minimal_responses:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->minimal_responses =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4239 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 355:
#line 1634 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_rrset_roundrobin:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->rrset_roundrobin =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4252 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 356:
#line 1644 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_max_udp_size:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->max_udp_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4262 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 357:
#line 1651 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dns64_prefix:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dns64_prefix);
		cfg_parser->cfg->dns64_prefix = (yyvsp[0].str);
	}
#line 4272 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 358:
#line 1658 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_dns64_synthall:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dns64_synthall = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4284 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 359:
#line 1667 "util/configparser.y" /* yacc.c:1646  */
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
#line 4301 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 360:
#line 1681 "util/configparser.y" /* yacc.c:1646  */
    {
		size_t len = 0;
		uint8_t* bitlist = config_parse_taglist(cfg_parser->cfg, (yyvsp[0].str),
			&len);
		free((yyvsp[0].str));
		OUTYY(("P(server_local_zone_tag:%s)\n", (yyvsp[-1].str)));
		if(!bitlist)
			yyerror("could not parse tags, (define-tag them first)");
		if(bitlist) {
			if(!cfg_strbytelist_insert(
				&cfg_parser->cfg->local_zone_tags,
				(yyvsp[-1].str), bitlist, len)) {
				yyerror("out of memory");
				free((yyvsp[-1].str));
			}
		}
	}
#line 4323 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 361:
#line 1700 "util/configparser.y" /* yacc.c:1646  */
    {
		size_t len = 0;
		uint8_t* bitlist = config_parse_taglist(cfg_parser->cfg, (yyvsp[0].str),
			&len);
		free((yyvsp[0].str));
		OUTYY(("P(server_access_control_tag:%s)\n", (yyvsp[-1].str)));
		if(!bitlist)
			yyerror("could not parse tags, (define-tag them first)");
		if(bitlist) {
			if(!cfg_strbytelist_insert(
				&cfg_parser->cfg->acl_tags,
				(yyvsp[-1].str), bitlist, len)) {
				yyerror("out of memory");
				free((yyvsp[-1].str));
			}
		}
	}
#line 4345 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 362:
#line 1719 "util/configparser.y" /* yacc.c:1646  */
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
#line 4360 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 363:
#line 1731 "util/configparser.y" /* yacc.c:1646  */
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
#line 4375 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 364:
#line 1743 "util/configparser.y" /* yacc.c:1646  */
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
#line 4390 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 365:
#line 1755 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_access_control_view:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str2list_insert(&cfg_parser->cfg->acl_view,
			(yyvsp[-1].str), (yyvsp[0].str))) {
			yyerror("out of memory");
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		}
	}
#line 4404 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 366:
#line 1766 "util/configparser.y" /* yacc.c:1646  */
    {
		size_t len = 0;
		uint8_t* bitlist = config_parse_taglist(cfg_parser->cfg, (yyvsp[0].str),
			&len);
		free((yyvsp[0].str));
		OUTYY(("P(response_ip_tag:%s)\n", (yyvsp[-1].str)));
		if(!bitlist)
			yyerror("could not parse tags, (define-tag them first)");
		if(bitlist) {
			if(!cfg_strbytelist_insert(
				&cfg_parser->cfg->respip_tags,
				(yyvsp[-1].str), bitlist, len)) {
				yyerror("out of memory");
				free((yyvsp[-1].str));
			}
		}
	}
#line 4426 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 367:
#line 1785 "util/configparser.y" /* yacc.c:1646  */
    { 
		OUTYY(("P(server_ip_ratelimit:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ip_ratelimit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4438 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 368:
#line 1795 "util/configparser.y" /* yacc.c:1646  */
    { 
		OUTYY(("P(server_ratelimit:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ratelimit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4450 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 369:
#line 1804 "util/configparser.y" /* yacc.c:1646  */
    {
  	OUTYY(("P(server_ip_ratelimit_size:%s)\n", (yyvsp[0].str)));
  	if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->ip_ratelimit_size))
  		yyerror("memory size expected");
  	free((yyvsp[0].str));
  }
#line 4461 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 370:
#line 1812 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_ratelimit_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->ratelimit_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4472 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 371:
#line 1820 "util/configparser.y" /* yacc.c:1646  */
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
#line 4488 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 372:
#line 1833 "util/configparser.y" /* yacc.c:1646  */
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
#line 4504 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 373:
#line 1846 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_ratelimit_for_domain:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0) {
			yyerror("number expected");
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->
				ratelimit_for_domain, (yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding "
					"ratelimit-for-domain");
		}
	}
#line 4520 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 374:
#line 1859 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_ratelimit_below_domain:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0) {
			yyerror("number expected");
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->
				ratelimit_below_domain, (yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding "
					"ratelimit-below-domain");
		}
	}
#line 4536 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 375:
#line 1872 "util/configparser.y" /* yacc.c:1646  */
    { 
  	OUTYY(("P(server_ip_ratelimit_factor:%s)\n", (yyvsp[0].str))); 
  	if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
  		yyerror("number expected");
  	else cfg_parser->cfg->ip_ratelimit_factor = atoi((yyvsp[0].str));
  	free((yyvsp[0].str));
	}
#line 4548 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 376:
#line 1881 "util/configparser.y" /* yacc.c:1646  */
    { 
		OUTYY(("P(server_ratelimit_factor:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ratelimit_factor = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4560 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 377:
#line 1890 "util/configparser.y" /* yacc.c:1646  */
    { 
		OUTYY(("P(server_low_rtt:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->low_rtt = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4572 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 378:
#line 1899 "util/configparser.y" /* yacc.c:1646  */
    { 
		OUTYY(("P(server_low_rtt_permil:%s)\n", (yyvsp[0].str))); 
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->low_rtt_permil = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4584 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 379:
#line 1908 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_qname_minimisation:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->qname_minimisation = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4597 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 380:
#line 1918 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_qname_minimisation_strict:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->qname_minimisation_strict = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4610 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 381:
#line 1928 "util/configparser.y" /* yacc.c:1646  */
    {
	#ifdef USE_IPSECMOD
		OUTYY(("P(server_ipsecmod_enabled:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ipsecmod_enabled = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without IPsec module, ignoring)\n"));
	#endif
	}
#line 4626 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 382:
#line 1941 "util/configparser.y" /* yacc.c:1646  */
    {
	#ifdef USE_IPSECMOD
		OUTYY(("P(server_ipsecmod_ignore_bogus:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ipsecmod_ignore_bogus = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without IPsec module, ignoring)\n"));
	#endif
	}
#line 4642 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 383:
#line 1954 "util/configparser.y" /* yacc.c:1646  */
    {
	#ifdef USE_IPSECMOD
		OUTYY(("P(server_ipsecmod_hook:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->ipsecmod_hook);
		cfg_parser->cfg->ipsecmod_hook = (yyvsp[0].str);
	#else
		OUTYY(("P(Compiled without IPsec module, ignoring)\n"));
	#endif
	}
#line 4656 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 384:
#line 1965 "util/configparser.y" /* yacc.c:1646  */
    {
	#ifdef USE_IPSECMOD
		OUTYY(("P(server_ipsecmod_max_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ipsecmod_max_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without IPsec module, ignoring)\n"));
	#endif
	}
#line 4672 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 385:
#line 1978 "util/configparser.y" /* yacc.c:1646  */
    {
	#ifdef USE_IPSECMOD
		OUTYY(("P(server_ipsecmod_whitelist:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->ipsecmod_whitelist, (yyvsp[0].str)))
			yyerror("out of memory");
	#else
		OUTYY(("P(Compiled without IPsec module, ignoring)\n"));
	#endif
	}
#line 4686 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 386:
#line 1989 "util/configparser.y" /* yacc.c:1646  */
    {
	#ifdef USE_IPSECMOD
		OUTYY(("P(server_ipsecmod_strict:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ipsecmod_strict = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without IPsec module, ignoring)\n"));
	#endif
	}
#line 4702 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 387:
#line 2002 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->stubs->name)
			yyerror("stub name override, there must be one name "
				"for one stub-zone");
		free(cfg_parser->cfg->stubs->name);
		cfg_parser->cfg->stubs->name = (yyvsp[0].str);
	}
#line 4715 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 388:
#line 2012 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(stub-host:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->stubs->hosts, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4725 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 389:
#line 2019 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(stub-addr:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->stubs->addrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4735 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 390:
#line 2026 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(stub-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4747 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 391:
#line 2035 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(stub-ssl-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->ssl_upstream = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4760 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 392:
#line 2045 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(stub-prime:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->isprime = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4773 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 393:
#line 2055 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->forwards->name)
			yyerror("forward name override, there must be one "
				"name for one forward-zone");
		free(cfg_parser->cfg->forwards->name);
		cfg_parser->cfg->forwards->name = (yyvsp[0].str);
	}
#line 4786 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 394:
#line 2065 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(forward-host:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->forwards->hosts, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4796 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 395:
#line 2072 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(forward-addr:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->forwards->addrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4806 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 396:
#line 2079 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(forward-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4818 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 397:
#line 2088 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(forward-ssl-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->ssl_upstream = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4831 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 398:
#line 2098 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->auths->name)
			yyerror("auth name override, there must be one name "
				"for one auth-zone");
		free(cfg_parser->cfg->auths->name);
		cfg_parser->cfg->auths->name = (yyvsp[0].str);
	}
#line 4844 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 399:
#line 2108 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(zonefile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->auths->zonefile);
		cfg_parser->cfg->auths->zonefile = (yyvsp[0].str);
	}
#line 4854 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 400:
#line 2115 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(master:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->masters, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4864 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 401:
#line 2122 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(url:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->urls, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4874 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 402:
#line 2129 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(allow-notify:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->allow_notify,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4885 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 403:
#line 2137 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(for-downstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->for_downstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4898 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 404:
#line 2147 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(for-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->for_upstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4911 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 405:
#line 2157 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(fallback-enabled:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->fallback_enabled =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4924 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 406:
#line 2167 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->views->name)
			yyerror("view name override, there must be one "
				"name for one view");
		free(cfg_parser->cfg->views->name);
		cfg_parser->cfg->views->name = (yyvsp[0].str);
	}
#line 4937 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 407:
#line 2177 "util/configparser.y" /* yacc.c:1646  */
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
		   && strcmp((yyvsp[0].str), "inform")!=0 && strcmp((yyvsp[0].str), "inform_deny")!=0)
			yyerror("local-zone type: expected static, deny, "
				"refuse, redirect, transparent, "
				"typetransparent, inform, inform_deny, "
				"always_transparent, always_refuse, "
				"always_nxdomain, noview or nodefault");
		else if(strcmp((yyvsp[0].str), "nodefault")==0) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->views->
				local_zones_nodefault, (yyvsp[-1].str)))
				fatal_exit("out of memory adding local-zone");
			free((yyvsp[0].str));
		} else {
			if(!cfg_str2list_insert(
				&cfg_parser->cfg->views->local_zones, 
				(yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding local-zone");
		}
	}
#line 4970 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 408:
#line 2207 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(view_response_ip:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_respip_action((yyvsp[0].str));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->views->respip_actions, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding per-view "
				"response-ip action");
	}
#line 4983 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 409:
#line 2217 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(view_response_ip_data:%s)\n", (yyvsp[-1].str)));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->views->respip_data, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding response-ip-data");
	}
#line 4994 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 410:
#line 2225 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(view_local_data:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->views->local_data, (yyvsp[0].str))) {
			fatal_exit("out of memory adding local-data");
			free((yyvsp[0].str));
		}
	}
#line 5006 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 411:
#line 2234 "util/configparser.y" /* yacc.c:1646  */
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
#line 5024 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 412:
#line 2249 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(view-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->views->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5036 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 413:
#line 2258 "util/configparser.y" /* yacc.c:1646  */
    { 
		OUTYY(("\nP(remote-control:)\n")); 
	}
#line 5044 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 424:
#line 2269 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(control_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->remote_control_enable = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5057 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 425:
#line 2279 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(control_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("control port number expected");
		else cfg_parser->cfg->control_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5069 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 426:
#line 2288 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(control_interface:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append(&cfg_parser->cfg->control_ifs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5079 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 427:
#line 2295 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(control_use_cert:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->control_use_cert = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5089 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 428:
#line 2302 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(rc_server_key_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->server_key_file);
		cfg_parser->cfg->server_key_file = (yyvsp[0].str);
	}
#line 5099 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 429:
#line 2309 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(rc_server_cert_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->server_cert_file);
		cfg_parser->cfg->server_cert_file = (yyvsp[0].str);
	}
#line 5109 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 430:
#line 2316 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(rc_control_key_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->control_key_file);
		cfg_parser->cfg->control_key_file = (yyvsp[0].str);
	}
#line 5119 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 431:
#line 2323 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(rc_control_cert_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->control_cert_file);
		cfg_parser->cfg->control_cert_file = (yyvsp[0].str);
	}
#line 5129 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 432:
#line 2330 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("\nP(dnstap:)\n"));
	}
#line 5137 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 447:
#line 2347 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dt_dnstap_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap = (strcmp((yyvsp[0].str), "yes")==0);
	}
#line 5148 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 448:
#line 2355 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dt_dnstap_socket_path:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_socket_path);
		cfg_parser->cfg->dnstap_socket_path = (yyvsp[0].str);
	}
#line 5158 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 449:
#line 2362 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dt_dnstap_send_identity:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_send_identity = (strcmp((yyvsp[0].str), "yes")==0);
	}
#line 5169 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 450:
#line 2370 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dt_dnstap_send_version:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_send_version = (strcmp((yyvsp[0].str), "yes")==0);
	}
#line 5180 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 451:
#line 2378 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dt_dnstap_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_identity);
		cfg_parser->cfg->dnstap_identity = (yyvsp[0].str);
	}
#line 5190 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 452:
#line 2385 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dt_dnstap_version:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_version);
		cfg_parser->cfg->dnstap_version = (yyvsp[0].str);
	}
#line 5200 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 453:
#line 2392 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dt_dnstap_log_resolver_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_resolver_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
	}
#line 5212 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 454:
#line 2401 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dt_dnstap_log_resolver_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_resolver_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
	}
#line 5224 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 455:
#line 2410 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dt_dnstap_log_client_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_client_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
	}
#line 5236 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 456:
#line 2419 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dt_dnstap_log_client_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_client_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
	}
#line 5248 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 457:
#line 2428 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dt_dnstap_log_forwarder_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_forwarder_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
	}
#line 5260 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 458:
#line 2437 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dt_dnstap_log_forwarder_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_forwarder_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
	}
#line 5272 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 459:
#line 2446 "util/configparser.y" /* yacc.c:1646  */
    { 
		OUTYY(("\nP(python:)\n")); 
	}
#line 5280 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 463:
#line 2455 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(python-script:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->python_script);
		cfg_parser->cfg->python_script = (yyvsp[0].str);
	}
#line 5290 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 464:
#line 2461 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(disable_dnssec_lame_check:%s)\n", (yyvsp[0].str)));
		if (strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->disable_dnssec_lame_check =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5303 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 465:
#line 2471 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_log_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->log_identity);
		cfg_parser->cfg->log_identity = (yyvsp[0].str);
	}
#line 5313 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 466:
#line 2478 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_response_ip:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_respip_action((yyvsp[0].str));
		if(!cfg_str2list_insert(&cfg_parser->cfg->respip_actions,
			(yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding response-ip");
	}
#line 5325 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 467:
#line 2487 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(server_response_ip_data:%s)\n", (yyvsp[-1].str)));
			if(!cfg_str2list_insert(&cfg_parser->cfg->respip_data,
				(yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding response-ip-data");
	}
#line 5336 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 468:
#line 2495 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("\nP(dnscrypt:)\n"));
		OUTYY(("\nP(dnscrypt:)\n"));
	}
#line 5345 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 481:
#line 2512 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dnsc_dnscrypt_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnscrypt = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5357 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 482:
#line 2522 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dnsc_dnscrypt_port:%s)\n", (yyvsp[0].str)));

		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->dnscrypt_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5370 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 483:
#line 2532 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dnsc_dnscrypt_provider:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnscrypt_provider);
		cfg_parser->cfg->dnscrypt_provider = (yyvsp[0].str);
	}
#line 5380 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 484:
#line 2539 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dnsc_dnscrypt_provider_cert:%s)\n", (yyvsp[0].str)));
		if(cfg_strlist_find(cfg_parser->cfg->dnscrypt_provider_cert, (yyvsp[0].str)))
			log_warn("dnscrypt-provider-cert %s is a duplicate", (yyvsp[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_provider_cert, (yyvsp[0].str)))
			fatal_exit("out of memory adding dnscrypt-provider-cert");
	}
#line 5392 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 485:
#line 2548 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dnsc_dnscrypt_provider_cert_rotated:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_provider_cert_rotated, (yyvsp[0].str)))
			fatal_exit("out of memory adding dnscrypt-provider-cert-rotated");
	}
#line 5402 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 486:
#line 2555 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("P(dnsc_dnscrypt_secret_key:%s)\n", (yyvsp[0].str)));
		if(cfg_strlist_find(cfg_parser->cfg->dnscrypt_secret_key, (yyvsp[0].str)))
			log_warn("dnscrypt-secret-key: %s is a duplicate", (yyvsp[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_secret_key, (yyvsp[0].str)))
			fatal_exit("out of memory adding dnscrypt-secret-key");
	}
#line 5414 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 487:
#line 2564 "util/configparser.y" /* yacc.c:1646  */
    {
  	OUTYY(("P(dnscrypt_shared_secret_cache_size:%s)\n", (yyvsp[0].str)));
  	if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->dnscrypt_shared_secret_cache_size))
  		yyerror("memory size expected");
  	free((yyvsp[0].str));
  }
#line 5425 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 488:
#line 2572 "util/configparser.y" /* yacc.c:1646  */
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
#line 5441 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 489:
#line 2585 "util/configparser.y" /* yacc.c:1646  */
    {
  	OUTYY(("P(dnscrypt_nonce_cache_size:%s)\n", (yyvsp[0].str)));
  	if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->dnscrypt_nonce_cache_size))
  		yyerror("memory size expected");
  	free((yyvsp[0].str));
  }
#line 5452 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 490:
#line 2593 "util/configparser.y" /* yacc.c:1646  */
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
#line 5468 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 491:
#line 2606 "util/configparser.y" /* yacc.c:1646  */
    {
		OUTYY(("\nP(cachedb:)\n"));
	}
#line 5476 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 499:
#line 2616 "util/configparser.y" /* yacc.c:1646  */
    {
	#ifdef USE_CACHEDB
		OUTYY(("P(backend:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->cachedb_backend)
			yyerror("cachedb backend override, there must be one "
				"backend");
		free(cfg_parser->cfg->cachedb_backend);
		cfg_parser->cfg->cachedb_backend = (yyvsp[0].str);
	#else
		OUTYY(("P(Compiled without cachedb, ignoring)\n"));
	#endif
	}
#line 5493 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 500:
#line 2630 "util/configparser.y" /* yacc.c:1646  */
    {
	#ifdef USE_CACHEDB
		OUTYY(("P(secret-seed:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->cachedb_secret)
			yyerror("cachedb secret-seed override, there must be "
				"only one secret");
		free(cfg_parser->cfg->cachedb_secret);
		cfg_parser->cfg->cachedb_secret = (yyvsp[0].str);
	#else
		OUTYY(("P(Compiled without cachedb, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 5511 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 501:
#line 2645 "util/configparser.y" /* yacc.c:1646  */
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
#line 5526 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 502:
#line 2657 "util/configparser.y" /* yacc.c:1646  */
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
#line 5544 "util/configparser.c" /* yacc.c:1646  */
    break;

  case 503:
#line 2672 "util/configparser.y" /* yacc.c:1646  */
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
#line 5560 "util/configparser.c" /* yacc.c:1646  */
    break;


#line 5564 "util/configparser.c" /* yacc.c:1646  */
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

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

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

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

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
#line 2684 "util/configparser.y" /* yacc.c:1906  */


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
