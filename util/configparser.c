/* A Bison parser, made by GNU Bison 3.7.6.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30706

/* Bison version string.  */
#define YYBISON_VERSION "3.7.6"

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
static void validate_acl_action(const char* action);

/* these need to be global, otherwise they cannot be used inside yacc */
extern struct config_parser_state* cfg_parser;

#if 0
#define OUTYY(s)  printf s /* used ONLY when debugging */
#else
#define OUTYY(s)
#endif


#line 101 "util/configparser.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
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

#include "configparser.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_SPACE = 3,                      /* SPACE  */
  YYSYMBOL_LETTER = 4,                     /* LETTER  */
  YYSYMBOL_NEWLINE = 5,                    /* NEWLINE  */
  YYSYMBOL_COMMENT = 6,                    /* COMMENT  */
  YYSYMBOL_COLON = 7,                      /* COLON  */
  YYSYMBOL_ANY = 8,                        /* ANY  */
  YYSYMBOL_ZONESTR = 9,                    /* ZONESTR  */
  YYSYMBOL_STRING_ARG = 10,                /* STRING_ARG  */
  YYSYMBOL_VAR_FORCE_TOPLEVEL = 11,        /* VAR_FORCE_TOPLEVEL  */
  YYSYMBOL_VAR_SERVER = 12,                /* VAR_SERVER  */
  YYSYMBOL_VAR_VERBOSITY = 13,             /* VAR_VERBOSITY  */
  YYSYMBOL_VAR_NUM_THREADS = 14,           /* VAR_NUM_THREADS  */
  YYSYMBOL_VAR_PORT = 15,                  /* VAR_PORT  */
  YYSYMBOL_VAR_OUTGOING_RANGE = 16,        /* VAR_OUTGOING_RANGE  */
  YYSYMBOL_VAR_INTERFACE = 17,             /* VAR_INTERFACE  */
  YYSYMBOL_VAR_PREFER_IP4 = 18,            /* VAR_PREFER_IP4  */
  YYSYMBOL_VAR_DO_IP4 = 19,                /* VAR_DO_IP4  */
  YYSYMBOL_VAR_DO_IP6 = 20,                /* VAR_DO_IP6  */
  YYSYMBOL_VAR_PREFER_IP6 = 21,            /* VAR_PREFER_IP6  */
  YYSYMBOL_VAR_DO_UDP = 22,                /* VAR_DO_UDP  */
  YYSYMBOL_VAR_DO_TCP = 23,                /* VAR_DO_TCP  */
  YYSYMBOL_VAR_TCP_MSS = 24,               /* VAR_TCP_MSS  */
  YYSYMBOL_VAR_OUTGOING_TCP_MSS = 25,      /* VAR_OUTGOING_TCP_MSS  */
  YYSYMBOL_VAR_TCP_IDLE_TIMEOUT = 26,      /* VAR_TCP_IDLE_TIMEOUT  */
  YYSYMBOL_VAR_EDNS_TCP_KEEPALIVE = 27,    /* VAR_EDNS_TCP_KEEPALIVE  */
  YYSYMBOL_VAR_EDNS_TCP_KEEPALIVE_TIMEOUT = 28, /* VAR_EDNS_TCP_KEEPALIVE_TIMEOUT  */
  YYSYMBOL_VAR_CHROOT = 29,                /* VAR_CHROOT  */
  YYSYMBOL_VAR_USERNAME = 30,              /* VAR_USERNAME  */
  YYSYMBOL_VAR_DIRECTORY = 31,             /* VAR_DIRECTORY  */
  YYSYMBOL_VAR_LOGFILE = 32,               /* VAR_LOGFILE  */
  YYSYMBOL_VAR_PIDFILE = 33,               /* VAR_PIDFILE  */
  YYSYMBOL_VAR_MSG_CACHE_SIZE = 34,        /* VAR_MSG_CACHE_SIZE  */
  YYSYMBOL_VAR_MSG_CACHE_SLABS = 35,       /* VAR_MSG_CACHE_SLABS  */
  YYSYMBOL_VAR_NUM_QUERIES_PER_THREAD = 36, /* VAR_NUM_QUERIES_PER_THREAD  */
  YYSYMBOL_VAR_RRSET_CACHE_SIZE = 37,      /* VAR_RRSET_CACHE_SIZE  */
  YYSYMBOL_VAR_RRSET_CACHE_SLABS = 38,     /* VAR_RRSET_CACHE_SLABS  */
  YYSYMBOL_VAR_OUTGOING_NUM_TCP = 39,      /* VAR_OUTGOING_NUM_TCP  */
  YYSYMBOL_VAR_INFRA_HOST_TTL = 40,        /* VAR_INFRA_HOST_TTL  */
  YYSYMBOL_VAR_INFRA_LAME_TTL = 41,        /* VAR_INFRA_LAME_TTL  */
  YYSYMBOL_VAR_INFRA_CACHE_SLABS = 42,     /* VAR_INFRA_CACHE_SLABS  */
  YYSYMBOL_VAR_INFRA_CACHE_NUMHOSTS = 43,  /* VAR_INFRA_CACHE_NUMHOSTS  */
  YYSYMBOL_VAR_INFRA_CACHE_LAME_SIZE = 44, /* VAR_INFRA_CACHE_LAME_SIZE  */
  YYSYMBOL_VAR_NAME = 45,                  /* VAR_NAME  */
  YYSYMBOL_VAR_STUB_ZONE = 46,             /* VAR_STUB_ZONE  */
  YYSYMBOL_VAR_STUB_HOST = 47,             /* VAR_STUB_HOST  */
  YYSYMBOL_VAR_STUB_ADDR = 48,             /* VAR_STUB_ADDR  */
  YYSYMBOL_VAR_TARGET_FETCH_POLICY = 49,   /* VAR_TARGET_FETCH_POLICY  */
  YYSYMBOL_VAR_HARDEN_SHORT_BUFSIZE = 50,  /* VAR_HARDEN_SHORT_BUFSIZE  */
  YYSYMBOL_VAR_HARDEN_LARGE_QUERIES = 51,  /* VAR_HARDEN_LARGE_QUERIES  */
  YYSYMBOL_VAR_FORWARD_ZONE = 52,          /* VAR_FORWARD_ZONE  */
  YYSYMBOL_VAR_FORWARD_HOST = 53,          /* VAR_FORWARD_HOST  */
  YYSYMBOL_VAR_FORWARD_ADDR = 54,          /* VAR_FORWARD_ADDR  */
  YYSYMBOL_VAR_DO_NOT_QUERY_ADDRESS = 55,  /* VAR_DO_NOT_QUERY_ADDRESS  */
  YYSYMBOL_VAR_HIDE_IDENTITY = 56,         /* VAR_HIDE_IDENTITY  */
  YYSYMBOL_VAR_HIDE_VERSION = 57,          /* VAR_HIDE_VERSION  */
  YYSYMBOL_VAR_IDENTITY = 58,              /* VAR_IDENTITY  */
  YYSYMBOL_VAR_VERSION = 59,               /* VAR_VERSION  */
  YYSYMBOL_VAR_HARDEN_GLUE = 60,           /* VAR_HARDEN_GLUE  */
  YYSYMBOL_VAR_MODULE_CONF = 61,           /* VAR_MODULE_CONF  */
  YYSYMBOL_VAR_TRUST_ANCHOR_FILE = 62,     /* VAR_TRUST_ANCHOR_FILE  */
  YYSYMBOL_VAR_TRUST_ANCHOR = 63,          /* VAR_TRUST_ANCHOR  */
  YYSYMBOL_VAR_VAL_OVERRIDE_DATE = 64,     /* VAR_VAL_OVERRIDE_DATE  */
  YYSYMBOL_VAR_BOGUS_TTL = 65,             /* VAR_BOGUS_TTL  */
  YYSYMBOL_VAR_VAL_CLEAN_ADDITIONAL = 66,  /* VAR_VAL_CLEAN_ADDITIONAL  */
  YYSYMBOL_VAR_VAL_PERMISSIVE_MODE = 67,   /* VAR_VAL_PERMISSIVE_MODE  */
  YYSYMBOL_VAR_INCOMING_NUM_TCP = 68,      /* VAR_INCOMING_NUM_TCP  */
  YYSYMBOL_VAR_MSG_BUFFER_SIZE = 69,       /* VAR_MSG_BUFFER_SIZE  */
  YYSYMBOL_VAR_KEY_CACHE_SIZE = 70,        /* VAR_KEY_CACHE_SIZE  */
  YYSYMBOL_VAR_KEY_CACHE_SLABS = 71,       /* VAR_KEY_CACHE_SLABS  */
  YYSYMBOL_VAR_TRUSTED_KEYS_FILE = 72,     /* VAR_TRUSTED_KEYS_FILE  */
  YYSYMBOL_VAR_VAL_NSEC3_KEYSIZE_ITERATIONS = 73, /* VAR_VAL_NSEC3_KEYSIZE_ITERATIONS  */
  YYSYMBOL_VAR_USE_SYSLOG = 74,            /* VAR_USE_SYSLOG  */
  YYSYMBOL_VAR_OUTGOING_INTERFACE = 75,    /* VAR_OUTGOING_INTERFACE  */
  YYSYMBOL_VAR_ROOT_HINTS = 76,            /* VAR_ROOT_HINTS  */
  YYSYMBOL_VAR_DO_NOT_QUERY_LOCALHOST = 77, /* VAR_DO_NOT_QUERY_LOCALHOST  */
  YYSYMBOL_VAR_CACHE_MAX_TTL = 78,         /* VAR_CACHE_MAX_TTL  */
  YYSYMBOL_VAR_HARDEN_DNSSEC_STRIPPED = 79, /* VAR_HARDEN_DNSSEC_STRIPPED  */
  YYSYMBOL_VAR_ACCESS_CONTROL = 80,        /* VAR_ACCESS_CONTROL  */
  YYSYMBOL_VAR_LOCAL_ZONE = 81,            /* VAR_LOCAL_ZONE  */
  YYSYMBOL_VAR_LOCAL_DATA = 82,            /* VAR_LOCAL_DATA  */
  YYSYMBOL_VAR_INTERFACE_AUTOMATIC = 83,   /* VAR_INTERFACE_AUTOMATIC  */
  YYSYMBOL_VAR_STATISTICS_INTERVAL = 84,   /* VAR_STATISTICS_INTERVAL  */
  YYSYMBOL_VAR_DO_DAEMONIZE = 85,          /* VAR_DO_DAEMONIZE  */
  YYSYMBOL_VAR_USE_CAPS_FOR_ID = 86,       /* VAR_USE_CAPS_FOR_ID  */
  YYSYMBOL_VAR_STATISTICS_CUMULATIVE = 87, /* VAR_STATISTICS_CUMULATIVE  */
  YYSYMBOL_VAR_OUTGOING_PORT_PERMIT = 88,  /* VAR_OUTGOING_PORT_PERMIT  */
  YYSYMBOL_VAR_OUTGOING_PORT_AVOID = 89,   /* VAR_OUTGOING_PORT_AVOID  */
  YYSYMBOL_VAR_DLV_ANCHOR_FILE = 90,       /* VAR_DLV_ANCHOR_FILE  */
  YYSYMBOL_VAR_DLV_ANCHOR = 91,            /* VAR_DLV_ANCHOR  */
  YYSYMBOL_VAR_NEG_CACHE_SIZE = 92,        /* VAR_NEG_CACHE_SIZE  */
  YYSYMBOL_VAR_HARDEN_REFERRAL_PATH = 93,  /* VAR_HARDEN_REFERRAL_PATH  */
  YYSYMBOL_VAR_PRIVATE_ADDRESS = 94,       /* VAR_PRIVATE_ADDRESS  */
  YYSYMBOL_VAR_PRIVATE_DOMAIN = 95,        /* VAR_PRIVATE_DOMAIN  */
  YYSYMBOL_VAR_REMOTE_CONTROL = 96,        /* VAR_REMOTE_CONTROL  */
  YYSYMBOL_VAR_CONTROL_ENABLE = 97,        /* VAR_CONTROL_ENABLE  */
  YYSYMBOL_VAR_CONTROL_INTERFACE = 98,     /* VAR_CONTROL_INTERFACE  */
  YYSYMBOL_VAR_CONTROL_PORT = 99,          /* VAR_CONTROL_PORT  */
  YYSYMBOL_VAR_SERVER_KEY_FILE = 100,      /* VAR_SERVER_KEY_FILE  */
  YYSYMBOL_VAR_SERVER_CERT_FILE = 101,     /* VAR_SERVER_CERT_FILE  */
  YYSYMBOL_VAR_CONTROL_KEY_FILE = 102,     /* VAR_CONTROL_KEY_FILE  */
  YYSYMBOL_VAR_CONTROL_CERT_FILE = 103,    /* VAR_CONTROL_CERT_FILE  */
  YYSYMBOL_VAR_CONTROL_USE_CERT = 104,     /* VAR_CONTROL_USE_CERT  */
  YYSYMBOL_VAR_TCP_REUSE_TIMEOUT = 105,    /* VAR_TCP_REUSE_TIMEOUT  */
  YYSYMBOL_VAR_MAX_REUSE_TCP_QUERIES = 106, /* VAR_MAX_REUSE_TCP_QUERIES  */
  YYSYMBOL_VAR_EXTENDED_STATISTICS = 107,  /* VAR_EXTENDED_STATISTICS  */
  YYSYMBOL_VAR_LOCAL_DATA_PTR = 108,       /* VAR_LOCAL_DATA_PTR  */
  YYSYMBOL_VAR_JOSTLE_TIMEOUT = 109,       /* VAR_JOSTLE_TIMEOUT  */
  YYSYMBOL_VAR_STUB_PRIME = 110,           /* VAR_STUB_PRIME  */
  YYSYMBOL_VAR_UNWANTED_REPLY_THRESHOLD = 111, /* VAR_UNWANTED_REPLY_THRESHOLD  */
  YYSYMBOL_VAR_LOG_TIME_ASCII = 112,       /* VAR_LOG_TIME_ASCII  */
  YYSYMBOL_VAR_DOMAIN_INSECURE = 113,      /* VAR_DOMAIN_INSECURE  */
  YYSYMBOL_VAR_PYTHON = 114,               /* VAR_PYTHON  */
  YYSYMBOL_VAR_PYTHON_SCRIPT = 115,        /* VAR_PYTHON_SCRIPT  */
  YYSYMBOL_VAR_VAL_SIG_SKEW_MIN = 116,     /* VAR_VAL_SIG_SKEW_MIN  */
  YYSYMBOL_VAR_VAL_SIG_SKEW_MAX = 117,     /* VAR_VAL_SIG_SKEW_MAX  */
  YYSYMBOL_VAR_VAL_MAX_RESTART = 118,      /* VAR_VAL_MAX_RESTART  */
  YYSYMBOL_VAR_CACHE_MIN_TTL = 119,        /* VAR_CACHE_MIN_TTL  */
  YYSYMBOL_VAR_VAL_LOG_LEVEL = 120,        /* VAR_VAL_LOG_LEVEL  */
  YYSYMBOL_VAR_AUTO_TRUST_ANCHOR_FILE = 121, /* VAR_AUTO_TRUST_ANCHOR_FILE  */
  YYSYMBOL_VAR_KEEP_MISSING = 122,         /* VAR_KEEP_MISSING  */
  YYSYMBOL_VAR_ADD_HOLDDOWN = 123,         /* VAR_ADD_HOLDDOWN  */
  YYSYMBOL_VAR_DEL_HOLDDOWN = 124,         /* VAR_DEL_HOLDDOWN  */
  YYSYMBOL_VAR_SO_RCVBUF = 125,            /* VAR_SO_RCVBUF  */
  YYSYMBOL_VAR_EDNS_BUFFER_SIZE = 126,     /* VAR_EDNS_BUFFER_SIZE  */
  YYSYMBOL_VAR_PREFETCH = 127,             /* VAR_PREFETCH  */
  YYSYMBOL_VAR_PREFETCH_KEY = 128,         /* VAR_PREFETCH_KEY  */
  YYSYMBOL_VAR_SO_SNDBUF = 129,            /* VAR_SO_SNDBUF  */
  YYSYMBOL_VAR_SO_REUSEPORT = 130,         /* VAR_SO_REUSEPORT  */
  YYSYMBOL_VAR_HARDEN_BELOW_NXDOMAIN = 131, /* VAR_HARDEN_BELOW_NXDOMAIN  */
  YYSYMBOL_VAR_IGNORE_CD_FLAG = 132,       /* VAR_IGNORE_CD_FLAG  */
  YYSYMBOL_VAR_LOG_QUERIES = 133,          /* VAR_LOG_QUERIES  */
  YYSYMBOL_VAR_LOG_REPLIES = 134,          /* VAR_LOG_REPLIES  */
  YYSYMBOL_VAR_LOG_LOCAL_ACTIONS = 135,    /* VAR_LOG_LOCAL_ACTIONS  */
  YYSYMBOL_VAR_TCP_UPSTREAM = 136,         /* VAR_TCP_UPSTREAM  */
  YYSYMBOL_VAR_SSL_UPSTREAM = 137,         /* VAR_SSL_UPSTREAM  */
  YYSYMBOL_VAR_TCP_AUTH_QUERY_TIMEOUT = 138, /* VAR_TCP_AUTH_QUERY_TIMEOUT  */
  YYSYMBOL_VAR_SSL_SERVICE_KEY = 139,      /* VAR_SSL_SERVICE_KEY  */
  YYSYMBOL_VAR_SSL_SERVICE_PEM = 140,      /* VAR_SSL_SERVICE_PEM  */
  YYSYMBOL_VAR_SSL_PORT = 141,             /* VAR_SSL_PORT  */
  YYSYMBOL_VAR_FORWARD_FIRST = 142,        /* VAR_FORWARD_FIRST  */
  YYSYMBOL_VAR_STUB_SSL_UPSTREAM = 143,    /* VAR_STUB_SSL_UPSTREAM  */
  YYSYMBOL_VAR_FORWARD_SSL_UPSTREAM = 144, /* VAR_FORWARD_SSL_UPSTREAM  */
  YYSYMBOL_VAR_TLS_CERT_BUNDLE = 145,      /* VAR_TLS_CERT_BUNDLE  */
  YYSYMBOL_VAR_STUB_TCP_UPSTREAM = 146,    /* VAR_STUB_TCP_UPSTREAM  */
  YYSYMBOL_VAR_FORWARD_TCP_UPSTREAM = 147, /* VAR_FORWARD_TCP_UPSTREAM  */
  YYSYMBOL_VAR_HTTPS_PORT = 148,           /* VAR_HTTPS_PORT  */
  YYSYMBOL_VAR_HTTP_ENDPOINT = 149,        /* VAR_HTTP_ENDPOINT  */
  YYSYMBOL_VAR_HTTP_MAX_STREAMS = 150,     /* VAR_HTTP_MAX_STREAMS  */
  YYSYMBOL_VAR_HTTP_QUERY_BUFFER_SIZE = 151, /* VAR_HTTP_QUERY_BUFFER_SIZE  */
  YYSYMBOL_VAR_HTTP_RESPONSE_BUFFER_SIZE = 152, /* VAR_HTTP_RESPONSE_BUFFER_SIZE  */
  YYSYMBOL_VAR_HTTP_NODELAY = 153,         /* VAR_HTTP_NODELAY  */
  YYSYMBOL_VAR_HTTP_NOTLS_DOWNSTREAM = 154, /* VAR_HTTP_NOTLS_DOWNSTREAM  */
  YYSYMBOL_VAR_STUB_FIRST = 155,           /* VAR_STUB_FIRST  */
  YYSYMBOL_VAR_MINIMAL_RESPONSES = 156,    /* VAR_MINIMAL_RESPONSES  */
  YYSYMBOL_VAR_RRSET_ROUNDROBIN = 157,     /* VAR_RRSET_ROUNDROBIN  */
  YYSYMBOL_VAR_MAX_UDP_SIZE = 158,         /* VAR_MAX_UDP_SIZE  */
  YYSYMBOL_VAR_DELAY_CLOSE = 159,          /* VAR_DELAY_CLOSE  */
  YYSYMBOL_VAR_UDP_CONNECT = 160,          /* VAR_UDP_CONNECT  */
  YYSYMBOL_VAR_UNBLOCK_LAN_ZONES = 161,    /* VAR_UNBLOCK_LAN_ZONES  */
  YYSYMBOL_VAR_INSECURE_LAN_ZONES = 162,   /* VAR_INSECURE_LAN_ZONES  */
  YYSYMBOL_VAR_INFRA_CACHE_MIN_RTT = 163,  /* VAR_INFRA_CACHE_MIN_RTT  */
  YYSYMBOL_VAR_INFRA_CACHE_MAX_RTT = 164,  /* VAR_INFRA_CACHE_MAX_RTT  */
  YYSYMBOL_VAR_INFRA_KEEP_PROBING = 165,   /* VAR_INFRA_KEEP_PROBING  */
  YYSYMBOL_VAR_DNS64_PREFIX = 166,         /* VAR_DNS64_PREFIX  */
  YYSYMBOL_VAR_DNS64_SYNTHALL = 167,       /* VAR_DNS64_SYNTHALL  */
  YYSYMBOL_VAR_DNS64_IGNORE_AAAA = 168,    /* VAR_DNS64_IGNORE_AAAA  */
  YYSYMBOL_VAR_DNSTAP = 169,               /* VAR_DNSTAP  */
  YYSYMBOL_VAR_DNSTAP_ENABLE = 170,        /* VAR_DNSTAP_ENABLE  */
  YYSYMBOL_VAR_DNSTAP_SOCKET_PATH = 171,   /* VAR_DNSTAP_SOCKET_PATH  */
  YYSYMBOL_VAR_DNSTAP_IP = 172,            /* VAR_DNSTAP_IP  */
  YYSYMBOL_VAR_DNSTAP_TLS = 173,           /* VAR_DNSTAP_TLS  */
  YYSYMBOL_VAR_DNSTAP_TLS_SERVER_NAME = 174, /* VAR_DNSTAP_TLS_SERVER_NAME  */
  YYSYMBOL_VAR_DNSTAP_TLS_CERT_BUNDLE = 175, /* VAR_DNSTAP_TLS_CERT_BUNDLE  */
  YYSYMBOL_VAR_DNSTAP_TLS_CLIENT_KEY_FILE = 176, /* VAR_DNSTAP_TLS_CLIENT_KEY_FILE  */
  YYSYMBOL_VAR_DNSTAP_TLS_CLIENT_CERT_FILE = 177, /* VAR_DNSTAP_TLS_CLIENT_CERT_FILE  */
  YYSYMBOL_VAR_DNSTAP_SEND_IDENTITY = 178, /* VAR_DNSTAP_SEND_IDENTITY  */
  YYSYMBOL_VAR_DNSTAP_SEND_VERSION = 179,  /* VAR_DNSTAP_SEND_VERSION  */
  YYSYMBOL_VAR_DNSTAP_BIDIRECTIONAL = 180, /* VAR_DNSTAP_BIDIRECTIONAL  */
  YYSYMBOL_VAR_DNSTAP_IDENTITY = 181,      /* VAR_DNSTAP_IDENTITY  */
  YYSYMBOL_VAR_DNSTAP_VERSION = 182,       /* VAR_DNSTAP_VERSION  */
  YYSYMBOL_VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES = 183, /* VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES  */
  YYSYMBOL_VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES = 184, /* VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES  */
  YYSYMBOL_VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES = 185, /* VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES  */
  YYSYMBOL_VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES = 186, /* VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES  */
  YYSYMBOL_VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES = 187, /* VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES  */
  YYSYMBOL_VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES = 188, /* VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES  */
  YYSYMBOL_VAR_RESPONSE_IP_TAG = 189,      /* VAR_RESPONSE_IP_TAG  */
  YYSYMBOL_VAR_RESPONSE_IP = 190,          /* VAR_RESPONSE_IP  */
  YYSYMBOL_VAR_RESPONSE_IP_DATA = 191,     /* VAR_RESPONSE_IP_DATA  */
  YYSYMBOL_VAR_HARDEN_ALGO_DOWNGRADE = 192, /* VAR_HARDEN_ALGO_DOWNGRADE  */
  YYSYMBOL_VAR_IP_TRANSPARENT = 193,       /* VAR_IP_TRANSPARENT  */
  YYSYMBOL_VAR_IP_DSCP = 194,              /* VAR_IP_DSCP  */
  YYSYMBOL_VAR_DISABLE_DNSSEC_LAME_CHECK = 195, /* VAR_DISABLE_DNSSEC_LAME_CHECK  */
  YYSYMBOL_VAR_IP_RATELIMIT = 196,         /* VAR_IP_RATELIMIT  */
  YYSYMBOL_VAR_IP_RATELIMIT_SLABS = 197,   /* VAR_IP_RATELIMIT_SLABS  */
  YYSYMBOL_VAR_IP_RATELIMIT_SIZE = 198,    /* VAR_IP_RATELIMIT_SIZE  */
  YYSYMBOL_VAR_RATELIMIT = 199,            /* VAR_RATELIMIT  */
  YYSYMBOL_VAR_RATELIMIT_SLABS = 200,      /* VAR_RATELIMIT_SLABS  */
  YYSYMBOL_VAR_RATELIMIT_SIZE = 201,       /* VAR_RATELIMIT_SIZE  */
  YYSYMBOL_VAR_OUTBOUND_MSG_RETRY = 202,   /* VAR_OUTBOUND_MSG_RETRY  */
  YYSYMBOL_VAR_MAX_SENT_COUNT = 203,       /* VAR_MAX_SENT_COUNT  */
  YYSYMBOL_VAR_MAX_QUERY_RESTARTS = 204,   /* VAR_MAX_QUERY_RESTARTS  */
  YYSYMBOL_VAR_RATELIMIT_FOR_DOMAIN = 205, /* VAR_RATELIMIT_FOR_DOMAIN  */
  YYSYMBOL_VAR_RATELIMIT_BELOW_DOMAIN = 206, /* VAR_RATELIMIT_BELOW_DOMAIN  */
  YYSYMBOL_VAR_IP_RATELIMIT_FACTOR = 207,  /* VAR_IP_RATELIMIT_FACTOR  */
  YYSYMBOL_VAR_RATELIMIT_FACTOR = 208,     /* VAR_RATELIMIT_FACTOR  */
  YYSYMBOL_VAR_IP_RATELIMIT_BACKOFF = 209, /* VAR_IP_RATELIMIT_BACKOFF  */
  YYSYMBOL_VAR_RATELIMIT_BACKOFF = 210,    /* VAR_RATELIMIT_BACKOFF  */
  YYSYMBOL_VAR_SEND_CLIENT_SUBNET = 211,   /* VAR_SEND_CLIENT_SUBNET  */
  YYSYMBOL_VAR_CLIENT_SUBNET_ZONE = 212,   /* VAR_CLIENT_SUBNET_ZONE  */
  YYSYMBOL_VAR_CLIENT_SUBNET_ALWAYS_FORWARD = 213, /* VAR_CLIENT_SUBNET_ALWAYS_FORWARD  */
  YYSYMBOL_VAR_CLIENT_SUBNET_OPCODE = 214, /* VAR_CLIENT_SUBNET_OPCODE  */
  YYSYMBOL_VAR_MAX_CLIENT_SUBNET_IPV4 = 215, /* VAR_MAX_CLIENT_SUBNET_IPV4  */
  YYSYMBOL_VAR_MAX_CLIENT_SUBNET_IPV6 = 216, /* VAR_MAX_CLIENT_SUBNET_IPV6  */
  YYSYMBOL_VAR_MIN_CLIENT_SUBNET_IPV4 = 217, /* VAR_MIN_CLIENT_SUBNET_IPV4  */
  YYSYMBOL_VAR_MIN_CLIENT_SUBNET_IPV6 = 218, /* VAR_MIN_CLIENT_SUBNET_IPV6  */
  YYSYMBOL_VAR_MAX_ECS_TREE_SIZE_IPV4 = 219, /* VAR_MAX_ECS_TREE_SIZE_IPV4  */
  YYSYMBOL_VAR_MAX_ECS_TREE_SIZE_IPV6 = 220, /* VAR_MAX_ECS_TREE_SIZE_IPV6  */
  YYSYMBOL_VAR_CAPS_WHITELIST = 221,       /* VAR_CAPS_WHITELIST  */
  YYSYMBOL_VAR_CACHE_MAX_NEGATIVE_TTL = 222, /* VAR_CACHE_MAX_NEGATIVE_TTL  */
  YYSYMBOL_VAR_PERMIT_SMALL_HOLDDOWN = 223, /* VAR_PERMIT_SMALL_HOLDDOWN  */
  YYSYMBOL_VAR_QNAME_MINIMISATION = 224,   /* VAR_QNAME_MINIMISATION  */
  YYSYMBOL_VAR_QNAME_MINIMISATION_STRICT = 225, /* VAR_QNAME_MINIMISATION_STRICT  */
  YYSYMBOL_VAR_IP_FREEBIND = 226,          /* VAR_IP_FREEBIND  */
  YYSYMBOL_VAR_DEFINE_TAG = 227,           /* VAR_DEFINE_TAG  */
  YYSYMBOL_VAR_LOCAL_ZONE_TAG = 228,       /* VAR_LOCAL_ZONE_TAG  */
  YYSYMBOL_VAR_ACCESS_CONTROL_TAG = 229,   /* VAR_ACCESS_CONTROL_TAG  */
  YYSYMBOL_VAR_LOCAL_ZONE_OVERRIDE = 230,  /* VAR_LOCAL_ZONE_OVERRIDE  */
  YYSYMBOL_VAR_ACCESS_CONTROL_TAG_ACTION = 231, /* VAR_ACCESS_CONTROL_TAG_ACTION  */
  YYSYMBOL_VAR_ACCESS_CONTROL_TAG_DATA = 232, /* VAR_ACCESS_CONTROL_TAG_DATA  */
  YYSYMBOL_VAR_VIEW = 233,                 /* VAR_VIEW  */
  YYSYMBOL_VAR_ACCESS_CONTROL_VIEW = 234,  /* VAR_ACCESS_CONTROL_VIEW  */
  YYSYMBOL_VAR_VIEW_FIRST = 235,           /* VAR_VIEW_FIRST  */
  YYSYMBOL_VAR_SERVE_EXPIRED = 236,        /* VAR_SERVE_EXPIRED  */
  YYSYMBOL_VAR_SERVE_EXPIRED_TTL = 237,    /* VAR_SERVE_EXPIRED_TTL  */
  YYSYMBOL_VAR_SERVE_EXPIRED_TTL_RESET = 238, /* VAR_SERVE_EXPIRED_TTL_RESET  */
  YYSYMBOL_VAR_SERVE_EXPIRED_REPLY_TTL = 239, /* VAR_SERVE_EXPIRED_REPLY_TTL  */
  YYSYMBOL_VAR_SERVE_EXPIRED_CLIENT_TIMEOUT = 240, /* VAR_SERVE_EXPIRED_CLIENT_TIMEOUT  */
  YYSYMBOL_VAR_EDE_SERVE_EXPIRED = 241,    /* VAR_EDE_SERVE_EXPIRED  */
  YYSYMBOL_VAR_SERVE_ORIGINAL_TTL = 242,   /* VAR_SERVE_ORIGINAL_TTL  */
  YYSYMBOL_VAR_FAKE_DSA = 243,             /* VAR_FAKE_DSA  */
  YYSYMBOL_VAR_FAKE_SHA1 = 244,            /* VAR_FAKE_SHA1  */
  YYSYMBOL_VAR_LOG_IDENTITY = 245,         /* VAR_LOG_IDENTITY  */
  YYSYMBOL_VAR_HIDE_TRUSTANCHOR = 246,     /* VAR_HIDE_TRUSTANCHOR  */
  YYSYMBOL_VAR_HIDE_HTTP_USER_AGENT = 247, /* VAR_HIDE_HTTP_USER_AGENT  */
  YYSYMBOL_VAR_HTTP_USER_AGENT = 248,      /* VAR_HTTP_USER_AGENT  */
  YYSYMBOL_VAR_TRUST_ANCHOR_SIGNALING = 249, /* VAR_TRUST_ANCHOR_SIGNALING  */
  YYSYMBOL_VAR_AGGRESSIVE_NSEC = 250,      /* VAR_AGGRESSIVE_NSEC  */
  YYSYMBOL_VAR_USE_SYSTEMD = 251,          /* VAR_USE_SYSTEMD  */
  YYSYMBOL_VAR_SHM_ENABLE = 252,           /* VAR_SHM_ENABLE  */
  YYSYMBOL_VAR_SHM_KEY = 253,              /* VAR_SHM_KEY  */
  YYSYMBOL_VAR_ROOT_KEY_SENTINEL = 254,    /* VAR_ROOT_KEY_SENTINEL  */
  YYSYMBOL_VAR_DNSCRYPT = 255,             /* VAR_DNSCRYPT  */
  YYSYMBOL_VAR_DNSCRYPT_ENABLE = 256,      /* VAR_DNSCRYPT_ENABLE  */
  YYSYMBOL_VAR_DNSCRYPT_PORT = 257,        /* VAR_DNSCRYPT_PORT  */
  YYSYMBOL_VAR_DNSCRYPT_PROVIDER = 258,    /* VAR_DNSCRYPT_PROVIDER  */
  YYSYMBOL_VAR_DNSCRYPT_SECRET_KEY = 259,  /* VAR_DNSCRYPT_SECRET_KEY  */
  YYSYMBOL_VAR_DNSCRYPT_PROVIDER_CERT = 260, /* VAR_DNSCRYPT_PROVIDER_CERT  */
  YYSYMBOL_VAR_DNSCRYPT_PROVIDER_CERT_ROTATED = 261, /* VAR_DNSCRYPT_PROVIDER_CERT_ROTATED  */
  YYSYMBOL_VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE = 262, /* VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE  */
  YYSYMBOL_VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS = 263, /* VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS  */
  YYSYMBOL_VAR_DNSCRYPT_NONCE_CACHE_SIZE = 264, /* VAR_DNSCRYPT_NONCE_CACHE_SIZE  */
  YYSYMBOL_VAR_DNSCRYPT_NONCE_CACHE_SLABS = 265, /* VAR_DNSCRYPT_NONCE_CACHE_SLABS  */
  YYSYMBOL_VAR_PAD_RESPONSES = 266,        /* VAR_PAD_RESPONSES  */
  YYSYMBOL_VAR_PAD_RESPONSES_BLOCK_SIZE = 267, /* VAR_PAD_RESPONSES_BLOCK_SIZE  */
  YYSYMBOL_VAR_PAD_QUERIES = 268,          /* VAR_PAD_QUERIES  */
  YYSYMBOL_VAR_PAD_QUERIES_BLOCK_SIZE = 269, /* VAR_PAD_QUERIES_BLOCK_SIZE  */
  YYSYMBOL_VAR_IPSECMOD_ENABLED = 270,     /* VAR_IPSECMOD_ENABLED  */
  YYSYMBOL_VAR_IPSECMOD_HOOK = 271,        /* VAR_IPSECMOD_HOOK  */
  YYSYMBOL_VAR_IPSECMOD_IGNORE_BOGUS = 272, /* VAR_IPSECMOD_IGNORE_BOGUS  */
  YYSYMBOL_VAR_IPSECMOD_MAX_TTL = 273,     /* VAR_IPSECMOD_MAX_TTL  */
  YYSYMBOL_VAR_IPSECMOD_WHITELIST = 274,   /* VAR_IPSECMOD_WHITELIST  */
  YYSYMBOL_VAR_IPSECMOD_STRICT = 275,      /* VAR_IPSECMOD_STRICT  */
  YYSYMBOL_VAR_CACHEDB = 276,              /* VAR_CACHEDB  */
  YYSYMBOL_VAR_CACHEDB_BACKEND = 277,      /* VAR_CACHEDB_BACKEND  */
  YYSYMBOL_VAR_CACHEDB_SECRETSEED = 278,   /* VAR_CACHEDB_SECRETSEED  */
  YYSYMBOL_VAR_CACHEDB_REDISHOST = 279,    /* VAR_CACHEDB_REDISHOST  */
  YYSYMBOL_VAR_CACHEDB_REDISPORT = 280,    /* VAR_CACHEDB_REDISPORT  */
  YYSYMBOL_VAR_CACHEDB_REDISTIMEOUT = 281, /* VAR_CACHEDB_REDISTIMEOUT  */
  YYSYMBOL_VAR_CACHEDB_REDISEXPIRERECORDS = 282, /* VAR_CACHEDB_REDISEXPIRERECORDS  */
  YYSYMBOL_VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM = 283, /* VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM  */
  YYSYMBOL_VAR_FOR_UPSTREAM = 284,         /* VAR_FOR_UPSTREAM  */
  YYSYMBOL_VAR_AUTH_ZONE = 285,            /* VAR_AUTH_ZONE  */
  YYSYMBOL_VAR_ZONEFILE = 286,             /* VAR_ZONEFILE  */
  YYSYMBOL_VAR_MASTER = 287,               /* VAR_MASTER  */
  YYSYMBOL_VAR_URL = 288,                  /* VAR_URL  */
  YYSYMBOL_VAR_FOR_DOWNSTREAM = 289,       /* VAR_FOR_DOWNSTREAM  */
  YYSYMBOL_VAR_FALLBACK_ENABLED = 290,     /* VAR_FALLBACK_ENABLED  */
  YYSYMBOL_VAR_TLS_ADDITIONAL_PORT = 291,  /* VAR_TLS_ADDITIONAL_PORT  */
  YYSYMBOL_VAR_LOW_RTT = 292,              /* VAR_LOW_RTT  */
  YYSYMBOL_VAR_LOW_RTT_PERMIL = 293,       /* VAR_LOW_RTT_PERMIL  */
  YYSYMBOL_VAR_FAST_SERVER_PERMIL = 294,   /* VAR_FAST_SERVER_PERMIL  */
  YYSYMBOL_VAR_FAST_SERVER_NUM = 295,      /* VAR_FAST_SERVER_NUM  */
  YYSYMBOL_VAR_ALLOW_NOTIFY = 296,         /* VAR_ALLOW_NOTIFY  */
  YYSYMBOL_VAR_TLS_WIN_CERT = 297,         /* VAR_TLS_WIN_CERT  */
  YYSYMBOL_VAR_TCP_CONNECTION_LIMIT = 298, /* VAR_TCP_CONNECTION_LIMIT  */
  YYSYMBOL_VAR_FORWARD_NO_CACHE = 299,     /* VAR_FORWARD_NO_CACHE  */
  YYSYMBOL_VAR_STUB_NO_CACHE = 300,        /* VAR_STUB_NO_CACHE  */
  YYSYMBOL_VAR_LOG_SERVFAIL = 301,         /* VAR_LOG_SERVFAIL  */
  YYSYMBOL_VAR_DENY_ANY = 302,             /* VAR_DENY_ANY  */
  YYSYMBOL_VAR_UNKNOWN_SERVER_TIME_LIMIT = 303, /* VAR_UNKNOWN_SERVER_TIME_LIMIT  */
  YYSYMBOL_VAR_LOG_TAG_QUERYREPLY = 304,   /* VAR_LOG_TAG_QUERYREPLY  */
  YYSYMBOL_VAR_STREAM_WAIT_SIZE = 305,     /* VAR_STREAM_WAIT_SIZE  */
  YYSYMBOL_VAR_TLS_CIPHERS = 306,          /* VAR_TLS_CIPHERS  */
  YYSYMBOL_VAR_TLS_CIPHERSUITES = 307,     /* VAR_TLS_CIPHERSUITES  */
  YYSYMBOL_VAR_TLS_USE_SNI = 308,          /* VAR_TLS_USE_SNI  */
  YYSYMBOL_VAR_IPSET = 309,                /* VAR_IPSET  */
  YYSYMBOL_VAR_IPSET_NAME_V4 = 310,        /* VAR_IPSET_NAME_V4  */
  YYSYMBOL_VAR_IPSET_NAME_V6 = 311,        /* VAR_IPSET_NAME_V6  */
  YYSYMBOL_VAR_TLS_SESSION_TICKET_KEYS = 312, /* VAR_TLS_SESSION_TICKET_KEYS  */
  YYSYMBOL_VAR_RPZ = 313,                  /* VAR_RPZ  */
  YYSYMBOL_VAR_TAGS = 314,                 /* VAR_TAGS  */
  YYSYMBOL_VAR_RPZ_ACTION_OVERRIDE = 315,  /* VAR_RPZ_ACTION_OVERRIDE  */
  YYSYMBOL_VAR_RPZ_CNAME_OVERRIDE = 316,   /* VAR_RPZ_CNAME_OVERRIDE  */
  YYSYMBOL_VAR_RPZ_LOG = 317,              /* VAR_RPZ_LOG  */
  YYSYMBOL_VAR_RPZ_LOG_NAME = 318,         /* VAR_RPZ_LOG_NAME  */
  YYSYMBOL_VAR_DYNLIB = 319,               /* VAR_DYNLIB  */
  YYSYMBOL_VAR_DYNLIB_FILE = 320,          /* VAR_DYNLIB_FILE  */
  YYSYMBOL_VAR_EDNS_CLIENT_STRING = 321,   /* VAR_EDNS_CLIENT_STRING  */
  YYSYMBOL_VAR_EDNS_CLIENT_STRING_OPCODE = 322, /* VAR_EDNS_CLIENT_STRING_OPCODE  */
  YYSYMBOL_VAR_NSID = 323,                 /* VAR_NSID  */
  YYSYMBOL_VAR_ZONEMD_PERMISSIVE_MODE = 324, /* VAR_ZONEMD_PERMISSIVE_MODE  */
  YYSYMBOL_VAR_ZONEMD_CHECK = 325,         /* VAR_ZONEMD_CHECK  */
  YYSYMBOL_VAR_ZONEMD_REJECT_ABSENCE = 326, /* VAR_ZONEMD_REJECT_ABSENCE  */
  YYSYMBOL_VAR_RPZ_SIGNAL_NXDOMAIN_RA = 327, /* VAR_RPZ_SIGNAL_NXDOMAIN_RA  */
  YYSYMBOL_VAR_INTERFACE_AUTOMATIC_PORTS = 328, /* VAR_INTERFACE_AUTOMATIC_PORTS  */
  YYSYMBOL_VAR_EDE = 329,                  /* VAR_EDE  */
  YYSYMBOL_VAR_INTERFACE_ACTION = 330,     /* VAR_INTERFACE_ACTION  */
  YYSYMBOL_VAR_INTERFACE_VIEW = 331,       /* VAR_INTERFACE_VIEW  */
  YYSYMBOL_VAR_INTERFACE_TAG = 332,        /* VAR_INTERFACE_TAG  */
  YYSYMBOL_VAR_INTERFACE_TAG_ACTION = 333, /* VAR_INTERFACE_TAG_ACTION  */
  YYSYMBOL_VAR_INTERFACE_TAG_DATA = 334,   /* VAR_INTERFACE_TAG_DATA  */
  YYSYMBOL_VAR_PROXY_PROTOCOL_PORT = 335,  /* VAR_PROXY_PROTOCOL_PORT  */
  YYSYMBOL_VAR_STATISTICS_INHIBIT_ZERO = 336, /* VAR_STATISTICS_INHIBIT_ZERO  */
  YYSYMBOL_YYACCEPT = 337,                 /* $accept  */
  YYSYMBOL_toplevelvars = 338,             /* toplevelvars  */
  YYSYMBOL_toplevelvar = 339,              /* toplevelvar  */
  YYSYMBOL_force_toplevel = 340,           /* force_toplevel  */
  YYSYMBOL_serverstart = 341,              /* serverstart  */
  YYSYMBOL_contents_server = 342,          /* contents_server  */
  YYSYMBOL_content_server = 343,           /* content_server  */
  YYSYMBOL_stubstart = 344,                /* stubstart  */
  YYSYMBOL_contents_stub = 345,            /* contents_stub  */
  YYSYMBOL_content_stub = 346,             /* content_stub  */
  YYSYMBOL_forwardstart = 347,             /* forwardstart  */
  YYSYMBOL_contents_forward = 348,         /* contents_forward  */
  YYSYMBOL_content_forward = 349,          /* content_forward  */
  YYSYMBOL_viewstart = 350,                /* viewstart  */
  YYSYMBOL_contents_view = 351,            /* contents_view  */
  YYSYMBOL_content_view = 352,             /* content_view  */
  YYSYMBOL_authstart = 353,                /* authstart  */
  YYSYMBOL_contents_auth = 354,            /* contents_auth  */
  YYSYMBOL_content_auth = 355,             /* content_auth  */
  YYSYMBOL_rpz_tag = 356,                  /* rpz_tag  */
  YYSYMBOL_rpz_action_override = 357,      /* rpz_action_override  */
  YYSYMBOL_rpz_cname_override = 358,       /* rpz_cname_override  */
  YYSYMBOL_rpz_log = 359,                  /* rpz_log  */
  YYSYMBOL_rpz_log_name = 360,             /* rpz_log_name  */
  YYSYMBOL_rpz_signal_nxdomain_ra = 361,   /* rpz_signal_nxdomain_ra  */
  YYSYMBOL_rpzstart = 362,                 /* rpzstart  */
  YYSYMBOL_contents_rpz = 363,             /* contents_rpz  */
  YYSYMBOL_content_rpz = 364,              /* content_rpz  */
  YYSYMBOL_server_num_threads = 365,       /* server_num_threads  */
  YYSYMBOL_server_verbosity = 366,         /* server_verbosity  */
  YYSYMBOL_server_statistics_interval = 367, /* server_statistics_interval  */
  YYSYMBOL_server_statistics_cumulative = 368, /* server_statistics_cumulative  */
  YYSYMBOL_server_extended_statistics = 369, /* server_extended_statistics  */
  YYSYMBOL_server_statistics_inhibit_zero = 370, /* server_statistics_inhibit_zero  */
  YYSYMBOL_server_shm_enable = 371,        /* server_shm_enable  */
  YYSYMBOL_server_shm_key = 372,           /* server_shm_key  */
  YYSYMBOL_server_port = 373,              /* server_port  */
  YYSYMBOL_server_send_client_subnet = 374, /* server_send_client_subnet  */
  YYSYMBOL_server_client_subnet_zone = 375, /* server_client_subnet_zone  */
  YYSYMBOL_server_client_subnet_always_forward = 376, /* server_client_subnet_always_forward  */
  YYSYMBOL_server_client_subnet_opcode = 377, /* server_client_subnet_opcode  */
  YYSYMBOL_server_max_client_subnet_ipv4 = 378, /* server_max_client_subnet_ipv4  */
  YYSYMBOL_server_max_client_subnet_ipv6 = 379, /* server_max_client_subnet_ipv6  */
  YYSYMBOL_server_min_client_subnet_ipv4 = 380, /* server_min_client_subnet_ipv4  */
  YYSYMBOL_server_min_client_subnet_ipv6 = 381, /* server_min_client_subnet_ipv6  */
  YYSYMBOL_server_max_ecs_tree_size_ipv4 = 382, /* server_max_ecs_tree_size_ipv4  */
  YYSYMBOL_server_max_ecs_tree_size_ipv6 = 383, /* server_max_ecs_tree_size_ipv6  */
  YYSYMBOL_server_interface = 384,         /* server_interface  */
  YYSYMBOL_server_outgoing_interface = 385, /* server_outgoing_interface  */
  YYSYMBOL_server_outgoing_range = 386,    /* server_outgoing_range  */
  YYSYMBOL_server_outgoing_port_permit = 387, /* server_outgoing_port_permit  */
  YYSYMBOL_server_outgoing_port_avoid = 388, /* server_outgoing_port_avoid  */
  YYSYMBOL_server_outgoing_num_tcp = 389,  /* server_outgoing_num_tcp  */
  YYSYMBOL_server_incoming_num_tcp = 390,  /* server_incoming_num_tcp  */
  YYSYMBOL_server_interface_automatic = 391, /* server_interface_automatic  */
  YYSYMBOL_server_interface_automatic_ports = 392, /* server_interface_automatic_ports  */
  YYSYMBOL_server_do_ip4 = 393,            /* server_do_ip4  */
  YYSYMBOL_server_do_ip6 = 394,            /* server_do_ip6  */
  YYSYMBOL_server_do_udp = 395,            /* server_do_udp  */
  YYSYMBOL_server_do_tcp = 396,            /* server_do_tcp  */
  YYSYMBOL_server_prefer_ip4 = 397,        /* server_prefer_ip4  */
  YYSYMBOL_server_prefer_ip6 = 398,        /* server_prefer_ip6  */
  YYSYMBOL_server_tcp_mss = 399,           /* server_tcp_mss  */
  YYSYMBOL_server_outgoing_tcp_mss = 400,  /* server_outgoing_tcp_mss  */
  YYSYMBOL_server_tcp_idle_timeout = 401,  /* server_tcp_idle_timeout  */
  YYSYMBOL_server_max_reuse_tcp_queries = 402, /* server_max_reuse_tcp_queries  */
  YYSYMBOL_server_tcp_reuse_timeout = 403, /* server_tcp_reuse_timeout  */
  YYSYMBOL_server_tcp_auth_query_timeout = 404, /* server_tcp_auth_query_timeout  */
  YYSYMBOL_server_tcp_keepalive = 405,     /* server_tcp_keepalive  */
  YYSYMBOL_server_tcp_keepalive_timeout = 406, /* server_tcp_keepalive_timeout  */
  YYSYMBOL_server_tcp_upstream = 407,      /* server_tcp_upstream  */
  YYSYMBOL_server_udp_upstream_without_downstream = 408, /* server_udp_upstream_without_downstream  */
  YYSYMBOL_server_ssl_upstream = 409,      /* server_ssl_upstream  */
  YYSYMBOL_server_ssl_service_key = 410,   /* server_ssl_service_key  */
  YYSYMBOL_server_ssl_service_pem = 411,   /* server_ssl_service_pem  */
  YYSYMBOL_server_ssl_port = 412,          /* server_ssl_port  */
  YYSYMBOL_server_tls_cert_bundle = 413,   /* server_tls_cert_bundle  */
  YYSYMBOL_server_tls_win_cert = 414,      /* server_tls_win_cert  */
  YYSYMBOL_server_tls_additional_port = 415, /* server_tls_additional_port  */
  YYSYMBOL_server_tls_ciphers = 416,       /* server_tls_ciphers  */
  YYSYMBOL_server_tls_ciphersuites = 417,  /* server_tls_ciphersuites  */
  YYSYMBOL_server_tls_session_ticket_keys = 418, /* server_tls_session_ticket_keys  */
  YYSYMBOL_server_tls_use_sni = 419,       /* server_tls_use_sni  */
  YYSYMBOL_server_https_port = 420,        /* server_https_port  */
  YYSYMBOL_server_http_endpoint = 421,     /* server_http_endpoint  */
  YYSYMBOL_server_http_max_streams = 422,  /* server_http_max_streams  */
  YYSYMBOL_server_http_query_buffer_size = 423, /* server_http_query_buffer_size  */
  YYSYMBOL_server_http_response_buffer_size = 424, /* server_http_response_buffer_size  */
  YYSYMBOL_server_http_nodelay = 425,      /* server_http_nodelay  */
  YYSYMBOL_server_http_notls_downstream = 426, /* server_http_notls_downstream  */
  YYSYMBOL_server_use_systemd = 427,       /* server_use_systemd  */
  YYSYMBOL_server_do_daemonize = 428,      /* server_do_daemonize  */
  YYSYMBOL_server_use_syslog = 429,        /* server_use_syslog  */
  YYSYMBOL_server_log_time_ascii = 430,    /* server_log_time_ascii  */
  YYSYMBOL_server_log_queries = 431,       /* server_log_queries  */
  YYSYMBOL_server_log_replies = 432,       /* server_log_replies  */
  YYSYMBOL_server_log_tag_queryreply = 433, /* server_log_tag_queryreply  */
  YYSYMBOL_server_log_servfail = 434,      /* server_log_servfail  */
  YYSYMBOL_server_log_local_actions = 435, /* server_log_local_actions  */
  YYSYMBOL_server_chroot = 436,            /* server_chroot  */
  YYSYMBOL_server_username = 437,          /* server_username  */
  YYSYMBOL_server_directory = 438,         /* server_directory  */
  YYSYMBOL_server_logfile = 439,           /* server_logfile  */
  YYSYMBOL_server_pidfile = 440,           /* server_pidfile  */
  YYSYMBOL_server_root_hints = 441,        /* server_root_hints  */
  YYSYMBOL_server_dlv_anchor_file = 442,   /* server_dlv_anchor_file  */
  YYSYMBOL_server_dlv_anchor = 443,        /* server_dlv_anchor  */
  YYSYMBOL_server_auto_trust_anchor_file = 444, /* server_auto_trust_anchor_file  */
  YYSYMBOL_server_trust_anchor_file = 445, /* server_trust_anchor_file  */
  YYSYMBOL_server_trusted_keys_file = 446, /* server_trusted_keys_file  */
  YYSYMBOL_server_trust_anchor = 447,      /* server_trust_anchor  */
  YYSYMBOL_server_trust_anchor_signaling = 448, /* server_trust_anchor_signaling  */
  YYSYMBOL_server_root_key_sentinel = 449, /* server_root_key_sentinel  */
  YYSYMBOL_server_domain_insecure = 450,   /* server_domain_insecure  */
  YYSYMBOL_server_hide_identity = 451,     /* server_hide_identity  */
  YYSYMBOL_server_hide_version = 452,      /* server_hide_version  */
  YYSYMBOL_server_hide_trustanchor = 453,  /* server_hide_trustanchor  */
  YYSYMBOL_server_hide_http_user_agent = 454, /* server_hide_http_user_agent  */
  YYSYMBOL_server_identity = 455,          /* server_identity  */
  YYSYMBOL_server_version = 456,           /* server_version  */
  YYSYMBOL_server_http_user_agent = 457,   /* server_http_user_agent  */
  YYSYMBOL_server_nsid = 458,              /* server_nsid  */
  YYSYMBOL_server_so_rcvbuf = 459,         /* server_so_rcvbuf  */
  YYSYMBOL_server_so_sndbuf = 460,         /* server_so_sndbuf  */
  YYSYMBOL_server_so_reuseport = 461,      /* server_so_reuseport  */
  YYSYMBOL_server_ip_transparent = 462,    /* server_ip_transparent  */
  YYSYMBOL_server_ip_freebind = 463,       /* server_ip_freebind  */
  YYSYMBOL_server_ip_dscp = 464,           /* server_ip_dscp  */
  YYSYMBOL_server_stream_wait_size = 465,  /* server_stream_wait_size  */
  YYSYMBOL_server_edns_buffer_size = 466,  /* server_edns_buffer_size  */
  YYSYMBOL_server_msg_buffer_size = 467,   /* server_msg_buffer_size  */
  YYSYMBOL_server_msg_cache_size = 468,    /* server_msg_cache_size  */
  YYSYMBOL_server_msg_cache_slabs = 469,   /* server_msg_cache_slabs  */
  YYSYMBOL_server_num_queries_per_thread = 470, /* server_num_queries_per_thread  */
  YYSYMBOL_server_jostle_timeout = 471,    /* server_jostle_timeout  */
  YYSYMBOL_server_delay_close = 472,       /* server_delay_close  */
  YYSYMBOL_server_udp_connect = 473,       /* server_udp_connect  */
  YYSYMBOL_server_unblock_lan_zones = 474, /* server_unblock_lan_zones  */
  YYSYMBOL_server_insecure_lan_zones = 475, /* server_insecure_lan_zones  */
  YYSYMBOL_server_rrset_cache_size = 476,  /* server_rrset_cache_size  */
  YYSYMBOL_server_rrset_cache_slabs = 477, /* server_rrset_cache_slabs  */
  YYSYMBOL_server_infra_host_ttl = 478,    /* server_infra_host_ttl  */
  YYSYMBOL_server_infra_lame_ttl = 479,    /* server_infra_lame_ttl  */
  YYSYMBOL_server_infra_cache_numhosts = 480, /* server_infra_cache_numhosts  */
  YYSYMBOL_server_infra_cache_lame_size = 481, /* server_infra_cache_lame_size  */
  YYSYMBOL_server_infra_cache_slabs = 482, /* server_infra_cache_slabs  */
  YYSYMBOL_server_infra_cache_min_rtt = 483, /* server_infra_cache_min_rtt  */
  YYSYMBOL_server_infra_cache_max_rtt = 484, /* server_infra_cache_max_rtt  */
  YYSYMBOL_server_infra_keep_probing = 485, /* server_infra_keep_probing  */
  YYSYMBOL_server_target_fetch_policy = 486, /* server_target_fetch_policy  */
  YYSYMBOL_server_harden_short_bufsize = 487, /* server_harden_short_bufsize  */
  YYSYMBOL_server_harden_large_queries = 488, /* server_harden_large_queries  */
  YYSYMBOL_server_harden_glue = 489,       /* server_harden_glue  */
  YYSYMBOL_server_harden_dnssec_stripped = 490, /* server_harden_dnssec_stripped  */
  YYSYMBOL_server_harden_below_nxdomain = 491, /* server_harden_below_nxdomain  */
  YYSYMBOL_server_harden_referral_path = 492, /* server_harden_referral_path  */
  YYSYMBOL_server_harden_algo_downgrade = 493, /* server_harden_algo_downgrade  */
  YYSYMBOL_server_use_caps_for_id = 494,   /* server_use_caps_for_id  */
  YYSYMBOL_server_caps_whitelist = 495,    /* server_caps_whitelist  */
  YYSYMBOL_server_private_address = 496,   /* server_private_address  */
  YYSYMBOL_server_private_domain = 497,    /* server_private_domain  */
  YYSYMBOL_server_prefetch = 498,          /* server_prefetch  */
  YYSYMBOL_server_prefetch_key = 499,      /* server_prefetch_key  */
  YYSYMBOL_server_deny_any = 500,          /* server_deny_any  */
  YYSYMBOL_server_unwanted_reply_threshold = 501, /* server_unwanted_reply_threshold  */
  YYSYMBOL_server_do_not_query_address = 502, /* server_do_not_query_address  */
  YYSYMBOL_server_do_not_query_localhost = 503, /* server_do_not_query_localhost  */
  YYSYMBOL_server_access_control = 504,    /* server_access_control  */
  YYSYMBOL_server_interface_action = 505,  /* server_interface_action  */
  YYSYMBOL_server_module_conf = 506,       /* server_module_conf  */
  YYSYMBOL_server_val_override_date = 507, /* server_val_override_date  */
  YYSYMBOL_server_val_sig_skew_min = 508,  /* server_val_sig_skew_min  */
  YYSYMBOL_server_val_sig_skew_max = 509,  /* server_val_sig_skew_max  */
  YYSYMBOL_server_val_max_restart = 510,   /* server_val_max_restart  */
  YYSYMBOL_server_cache_max_ttl = 511,     /* server_cache_max_ttl  */
  YYSYMBOL_server_cache_max_negative_ttl = 512, /* server_cache_max_negative_ttl  */
  YYSYMBOL_server_cache_min_ttl = 513,     /* server_cache_min_ttl  */
  YYSYMBOL_server_bogus_ttl = 514,         /* server_bogus_ttl  */
  YYSYMBOL_server_val_clean_additional = 515, /* server_val_clean_additional  */
  YYSYMBOL_server_val_permissive_mode = 516, /* server_val_permissive_mode  */
  YYSYMBOL_server_aggressive_nsec = 517,   /* server_aggressive_nsec  */
  YYSYMBOL_server_ignore_cd_flag = 518,    /* server_ignore_cd_flag  */
  YYSYMBOL_server_serve_expired = 519,     /* server_serve_expired  */
  YYSYMBOL_server_serve_expired_ttl = 520, /* server_serve_expired_ttl  */
  YYSYMBOL_server_serve_expired_ttl_reset = 521, /* server_serve_expired_ttl_reset  */
  YYSYMBOL_server_serve_expired_reply_ttl = 522, /* server_serve_expired_reply_ttl  */
  YYSYMBOL_server_serve_expired_client_timeout = 523, /* server_serve_expired_client_timeout  */
  YYSYMBOL_server_ede_serve_expired = 524, /* server_ede_serve_expired  */
  YYSYMBOL_server_serve_original_ttl = 525, /* server_serve_original_ttl  */
  YYSYMBOL_server_fake_dsa = 526,          /* server_fake_dsa  */
  YYSYMBOL_server_fake_sha1 = 527,         /* server_fake_sha1  */
  YYSYMBOL_server_val_log_level = 528,     /* server_val_log_level  */
  YYSYMBOL_server_val_nsec3_keysize_iterations = 529, /* server_val_nsec3_keysize_iterations  */
  YYSYMBOL_server_zonemd_permissive_mode = 530, /* server_zonemd_permissive_mode  */
  YYSYMBOL_server_add_holddown = 531,      /* server_add_holddown  */
  YYSYMBOL_server_del_holddown = 532,      /* server_del_holddown  */
  YYSYMBOL_server_keep_missing = 533,      /* server_keep_missing  */
  YYSYMBOL_server_permit_small_holddown = 534, /* server_permit_small_holddown  */
  YYSYMBOL_server_key_cache_size = 535,    /* server_key_cache_size  */
  YYSYMBOL_server_key_cache_slabs = 536,   /* server_key_cache_slabs  */
  YYSYMBOL_server_neg_cache_size = 537,    /* server_neg_cache_size  */
  YYSYMBOL_server_local_zone = 538,        /* server_local_zone  */
  YYSYMBOL_server_local_data = 539,        /* server_local_data  */
  YYSYMBOL_server_local_data_ptr = 540,    /* server_local_data_ptr  */
  YYSYMBOL_server_minimal_responses = 541, /* server_minimal_responses  */
  YYSYMBOL_server_rrset_roundrobin = 542,  /* server_rrset_roundrobin  */
  YYSYMBOL_server_unknown_server_time_limit = 543, /* server_unknown_server_time_limit  */
  YYSYMBOL_server_max_udp_size = 544,      /* server_max_udp_size  */
  YYSYMBOL_server_dns64_prefix = 545,      /* server_dns64_prefix  */
  YYSYMBOL_server_dns64_synthall = 546,    /* server_dns64_synthall  */
  YYSYMBOL_server_dns64_ignore_aaaa = 547, /* server_dns64_ignore_aaaa  */
  YYSYMBOL_server_define_tag = 548,        /* server_define_tag  */
  YYSYMBOL_server_local_zone_tag = 549,    /* server_local_zone_tag  */
  YYSYMBOL_server_access_control_tag = 550, /* server_access_control_tag  */
  YYSYMBOL_server_access_control_tag_action = 551, /* server_access_control_tag_action  */
  YYSYMBOL_server_access_control_tag_data = 552, /* server_access_control_tag_data  */
  YYSYMBOL_server_local_zone_override = 553, /* server_local_zone_override  */
  YYSYMBOL_server_access_control_view = 554, /* server_access_control_view  */
  YYSYMBOL_server_interface_tag = 555,     /* server_interface_tag  */
  YYSYMBOL_server_interface_tag_action = 556, /* server_interface_tag_action  */
  YYSYMBOL_server_interface_tag_data = 557, /* server_interface_tag_data  */
  YYSYMBOL_server_interface_view = 558,    /* server_interface_view  */
  YYSYMBOL_server_response_ip_tag = 559,   /* server_response_ip_tag  */
  YYSYMBOL_server_ip_ratelimit = 560,      /* server_ip_ratelimit  */
  YYSYMBOL_server_ratelimit = 561,         /* server_ratelimit  */
  YYSYMBOL_server_ip_ratelimit_size = 562, /* server_ip_ratelimit_size  */
  YYSYMBOL_server_ratelimit_size = 563,    /* server_ratelimit_size  */
  YYSYMBOL_server_ip_ratelimit_slabs = 564, /* server_ip_ratelimit_slabs  */
  YYSYMBOL_server_ratelimit_slabs = 565,   /* server_ratelimit_slabs  */
  YYSYMBOL_server_ratelimit_for_domain = 566, /* server_ratelimit_for_domain  */
  YYSYMBOL_server_ratelimit_below_domain = 567, /* server_ratelimit_below_domain  */
  YYSYMBOL_server_ip_ratelimit_factor = 568, /* server_ip_ratelimit_factor  */
  YYSYMBOL_server_ratelimit_factor = 569,  /* server_ratelimit_factor  */
  YYSYMBOL_server_ip_ratelimit_backoff = 570, /* server_ip_ratelimit_backoff  */
  YYSYMBOL_server_ratelimit_backoff = 571, /* server_ratelimit_backoff  */
  YYSYMBOL_server_outbound_msg_retry = 572, /* server_outbound_msg_retry  */
  YYSYMBOL_server_max_sent_count = 573,    /* server_max_sent_count  */
  YYSYMBOL_server_max_query_restarts = 574, /* server_max_query_restarts  */
  YYSYMBOL_server_low_rtt = 575,           /* server_low_rtt  */
  YYSYMBOL_server_fast_server_num = 576,   /* server_fast_server_num  */
  YYSYMBOL_server_fast_server_permil = 577, /* server_fast_server_permil  */
  YYSYMBOL_server_qname_minimisation = 578, /* server_qname_minimisation  */
  YYSYMBOL_server_qname_minimisation_strict = 579, /* server_qname_minimisation_strict  */
  YYSYMBOL_server_pad_responses = 580,     /* server_pad_responses  */
  YYSYMBOL_server_pad_responses_block_size = 581, /* server_pad_responses_block_size  */
  YYSYMBOL_server_pad_queries = 582,       /* server_pad_queries  */
  YYSYMBOL_server_pad_queries_block_size = 583, /* server_pad_queries_block_size  */
  YYSYMBOL_server_ipsecmod_enabled = 584,  /* server_ipsecmod_enabled  */
  YYSYMBOL_server_ipsecmod_ignore_bogus = 585, /* server_ipsecmod_ignore_bogus  */
  YYSYMBOL_server_ipsecmod_hook = 586,     /* server_ipsecmod_hook  */
  YYSYMBOL_server_ipsecmod_max_ttl = 587,  /* server_ipsecmod_max_ttl  */
  YYSYMBOL_server_ipsecmod_whitelist = 588, /* server_ipsecmod_whitelist  */
  YYSYMBOL_server_ipsecmod_strict = 589,   /* server_ipsecmod_strict  */
  YYSYMBOL_server_edns_client_string = 590, /* server_edns_client_string  */
  YYSYMBOL_server_edns_client_string_opcode = 591, /* server_edns_client_string_opcode  */
  YYSYMBOL_server_ede = 592,               /* server_ede  */
  YYSYMBOL_server_proxy_protocol_port = 593, /* server_proxy_protocol_port  */
  YYSYMBOL_stub_name = 594,                /* stub_name  */
  YYSYMBOL_stub_host = 595,                /* stub_host  */
  YYSYMBOL_stub_addr = 596,                /* stub_addr  */
  YYSYMBOL_stub_first = 597,               /* stub_first  */
  YYSYMBOL_stub_no_cache = 598,            /* stub_no_cache  */
  YYSYMBOL_stub_ssl_upstream = 599,        /* stub_ssl_upstream  */
  YYSYMBOL_stub_tcp_upstream = 600,        /* stub_tcp_upstream  */
  YYSYMBOL_stub_prime = 601,               /* stub_prime  */
  YYSYMBOL_forward_name = 602,             /* forward_name  */
  YYSYMBOL_forward_host = 603,             /* forward_host  */
  YYSYMBOL_forward_addr = 604,             /* forward_addr  */
  YYSYMBOL_forward_first = 605,            /* forward_first  */
  YYSYMBOL_forward_no_cache = 606,         /* forward_no_cache  */
  YYSYMBOL_forward_ssl_upstream = 607,     /* forward_ssl_upstream  */
  YYSYMBOL_forward_tcp_upstream = 608,     /* forward_tcp_upstream  */
  YYSYMBOL_auth_name = 609,                /* auth_name  */
  YYSYMBOL_auth_zonefile = 610,            /* auth_zonefile  */
  YYSYMBOL_auth_master = 611,              /* auth_master  */
  YYSYMBOL_auth_url = 612,                 /* auth_url  */
  YYSYMBOL_auth_allow_notify = 613,        /* auth_allow_notify  */
  YYSYMBOL_auth_zonemd_check = 614,        /* auth_zonemd_check  */
  YYSYMBOL_auth_zonemd_reject_absence = 615, /* auth_zonemd_reject_absence  */
  YYSYMBOL_auth_for_downstream = 616,      /* auth_for_downstream  */
  YYSYMBOL_auth_for_upstream = 617,        /* auth_for_upstream  */
  YYSYMBOL_auth_fallback_enabled = 618,    /* auth_fallback_enabled  */
  YYSYMBOL_view_name = 619,                /* view_name  */
  YYSYMBOL_view_local_zone = 620,          /* view_local_zone  */
  YYSYMBOL_view_response_ip = 621,         /* view_response_ip  */
  YYSYMBOL_view_response_ip_data = 622,    /* view_response_ip_data  */
  YYSYMBOL_view_local_data = 623,          /* view_local_data  */
  YYSYMBOL_view_local_data_ptr = 624,      /* view_local_data_ptr  */
  YYSYMBOL_view_first = 625,               /* view_first  */
  YYSYMBOL_rcstart = 626,                  /* rcstart  */
  YYSYMBOL_contents_rc = 627,              /* contents_rc  */
  YYSYMBOL_content_rc = 628,               /* content_rc  */
  YYSYMBOL_rc_control_enable = 629,        /* rc_control_enable  */
  YYSYMBOL_rc_control_port = 630,          /* rc_control_port  */
  YYSYMBOL_rc_control_interface = 631,     /* rc_control_interface  */
  YYSYMBOL_rc_control_use_cert = 632,      /* rc_control_use_cert  */
  YYSYMBOL_rc_server_key_file = 633,       /* rc_server_key_file  */
  YYSYMBOL_rc_server_cert_file = 634,      /* rc_server_cert_file  */
  YYSYMBOL_rc_control_key_file = 635,      /* rc_control_key_file  */
  YYSYMBOL_rc_control_cert_file = 636,     /* rc_control_cert_file  */
  YYSYMBOL_dtstart = 637,                  /* dtstart  */
  YYSYMBOL_contents_dt = 638,              /* contents_dt  */
  YYSYMBOL_content_dt = 639,               /* content_dt  */
  YYSYMBOL_dt_dnstap_enable = 640,         /* dt_dnstap_enable  */
  YYSYMBOL_dt_dnstap_bidirectional = 641,  /* dt_dnstap_bidirectional  */
  YYSYMBOL_dt_dnstap_socket_path = 642,    /* dt_dnstap_socket_path  */
  YYSYMBOL_dt_dnstap_ip = 643,             /* dt_dnstap_ip  */
  YYSYMBOL_dt_dnstap_tls = 644,            /* dt_dnstap_tls  */
  YYSYMBOL_dt_dnstap_tls_server_name = 645, /* dt_dnstap_tls_server_name  */
  YYSYMBOL_dt_dnstap_tls_cert_bundle = 646, /* dt_dnstap_tls_cert_bundle  */
  YYSYMBOL_dt_dnstap_tls_client_key_file = 647, /* dt_dnstap_tls_client_key_file  */
  YYSYMBOL_dt_dnstap_tls_client_cert_file = 648, /* dt_dnstap_tls_client_cert_file  */
  YYSYMBOL_dt_dnstap_send_identity = 649,  /* dt_dnstap_send_identity  */
  YYSYMBOL_dt_dnstap_send_version = 650,   /* dt_dnstap_send_version  */
  YYSYMBOL_dt_dnstap_identity = 651,       /* dt_dnstap_identity  */
  YYSYMBOL_dt_dnstap_version = 652,        /* dt_dnstap_version  */
  YYSYMBOL_dt_dnstap_log_resolver_query_messages = 653, /* dt_dnstap_log_resolver_query_messages  */
  YYSYMBOL_dt_dnstap_log_resolver_response_messages = 654, /* dt_dnstap_log_resolver_response_messages  */
  YYSYMBOL_dt_dnstap_log_client_query_messages = 655, /* dt_dnstap_log_client_query_messages  */
  YYSYMBOL_dt_dnstap_log_client_response_messages = 656, /* dt_dnstap_log_client_response_messages  */
  YYSYMBOL_dt_dnstap_log_forwarder_query_messages = 657, /* dt_dnstap_log_forwarder_query_messages  */
  YYSYMBOL_dt_dnstap_log_forwarder_response_messages = 658, /* dt_dnstap_log_forwarder_response_messages  */
  YYSYMBOL_pythonstart = 659,              /* pythonstart  */
  YYSYMBOL_contents_py = 660,              /* contents_py  */
  YYSYMBOL_content_py = 661,               /* content_py  */
  YYSYMBOL_py_script = 662,                /* py_script  */
  YYSYMBOL_dynlibstart = 663,              /* dynlibstart  */
  YYSYMBOL_contents_dl = 664,              /* contents_dl  */
  YYSYMBOL_content_dl = 665,               /* content_dl  */
  YYSYMBOL_dl_file = 666,                  /* dl_file  */
  YYSYMBOL_server_disable_dnssec_lame_check = 667, /* server_disable_dnssec_lame_check  */
  YYSYMBOL_server_log_identity = 668,      /* server_log_identity  */
  YYSYMBOL_server_response_ip = 669,       /* server_response_ip  */
  YYSYMBOL_server_response_ip_data = 670,  /* server_response_ip_data  */
  YYSYMBOL_dnscstart = 671,                /* dnscstart  */
  YYSYMBOL_contents_dnsc = 672,            /* contents_dnsc  */
  YYSYMBOL_content_dnsc = 673,             /* content_dnsc  */
  YYSYMBOL_dnsc_dnscrypt_enable = 674,     /* dnsc_dnscrypt_enable  */
  YYSYMBOL_dnsc_dnscrypt_port = 675,       /* dnsc_dnscrypt_port  */
  YYSYMBOL_dnsc_dnscrypt_provider = 676,   /* dnsc_dnscrypt_provider  */
  YYSYMBOL_dnsc_dnscrypt_provider_cert = 677, /* dnsc_dnscrypt_provider_cert  */
  YYSYMBOL_dnsc_dnscrypt_provider_cert_rotated = 678, /* dnsc_dnscrypt_provider_cert_rotated  */
  YYSYMBOL_dnsc_dnscrypt_secret_key = 679, /* dnsc_dnscrypt_secret_key  */
  YYSYMBOL_dnsc_dnscrypt_shared_secret_cache_size = 680, /* dnsc_dnscrypt_shared_secret_cache_size  */
  YYSYMBOL_dnsc_dnscrypt_shared_secret_cache_slabs = 681, /* dnsc_dnscrypt_shared_secret_cache_slabs  */
  YYSYMBOL_dnsc_dnscrypt_nonce_cache_size = 682, /* dnsc_dnscrypt_nonce_cache_size  */
  YYSYMBOL_dnsc_dnscrypt_nonce_cache_slabs = 683, /* dnsc_dnscrypt_nonce_cache_slabs  */
  YYSYMBOL_cachedbstart = 684,             /* cachedbstart  */
  YYSYMBOL_contents_cachedb = 685,         /* contents_cachedb  */
  YYSYMBOL_content_cachedb = 686,          /* content_cachedb  */
  YYSYMBOL_cachedb_backend_name = 687,     /* cachedb_backend_name  */
  YYSYMBOL_cachedb_secret_seed = 688,      /* cachedb_secret_seed  */
  YYSYMBOL_redis_server_host = 689,        /* redis_server_host  */
  YYSYMBOL_redis_server_port = 690,        /* redis_server_port  */
  YYSYMBOL_redis_timeout = 691,            /* redis_timeout  */
  YYSYMBOL_redis_expire_records = 692,     /* redis_expire_records  */
  YYSYMBOL_server_tcp_connection_limit = 693, /* server_tcp_connection_limit  */
  YYSYMBOL_ipsetstart = 694,               /* ipsetstart  */
  YYSYMBOL_contents_ipset = 695,           /* contents_ipset  */
  YYSYMBOL_content_ipset = 696,            /* content_ipset  */
  YYSYMBOL_ipset_name_v4 = 697,            /* ipset_name_v4  */
  YYSYMBOL_ipset_name_v6 = 698             /* ipset_name_v6  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

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


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
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

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

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
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
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
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
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
#define YYLAST   719

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  337
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  362
/* YYNRULES -- Number of rules.  */
#define YYNRULES  701
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  1049

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   591


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int16 yytranslate[] =
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
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   199,   199,   199,   200,   200,   201,   201,   202,   202,
     202,   203,   203,   204,   204,   205,   205,   206,   208,   215,
     221,   222,   223,   223,   223,   224,   224,   225,   225,   225,
     226,   226,   227,   227,   227,   228,   228,   229,   229,   229,
     230,   230,   230,   231,   231,   232,   232,   233,   233,   234,
     234,   235,   235,   236,   236,   237,   237,   238,   238,   239,
     239,   239,   240,   240,   241,   241,   241,   242,   242,   242,
     243,   243,   244,   244,   245,   245,   246,   246,   247,   247,
     247,   248,   248,   249,   249,   250,   250,   250,   251,   251,
     252,   252,   253,   253,   254,   254,   254,   255,   255,   256,
     256,   257,   257,   258,   258,   259,   259,   260,   260,   261,
     261,   262,   262,   263,   263,   263,   264,   264,   264,   265,
     265,   265,   266,   266,   266,   266,   267,   268,   268,   268,
     269,   269,   269,   270,   270,   271,   271,   272,   272,   272,
     273,   273,   273,   274,   274,   275,   275,   275,   276,   276,
     276,   277,   277,   277,   278,   278,   279,   279,   280,   280,
     281,   282,   282,   283,   283,   284,   284,   285,   285,   286,
     286,   287,   287,   288,   288,   289,   289,   290,   290,   291,
     291,   292,   292,   293,   293,   293,   294,   294,   295,   295,
     296,   296,   297,   297,   297,   298,   298,   299,   300,   300,
     301,   301,   302,   303,   303,   304,   304,   305,   305,   305,
     306,   306,   307,   307,   307,   308,   308,   308,   309,   309,
     310,   311,   311,   312,   312,   313,   313,   314,   314,   315,
     315,   315,   316,   316,   316,   317,   317,   317,   318,   318,
     319,   319,   320,   320,   321,   321,   322,   322,   323,   323,
     324,   324,   325,   325,   326,   326,   328,   342,   343,   344,
     344,   344,   344,   344,   345,   345,   345,   347,   361,   362,
     363,   363,   363,   363,   364,   364,   364,   366,   382,   383,
     384,   384,   384,   384,   385,   385,   385,   387,   408,   409,
     410,   410,   410,   410,   411,   411,   411,   412,   412,   412,
     415,   434,   451,   459,   469,   476,   486,   505,   506,   507,
     507,   507,   507,   507,   508,   508,   508,   509,   509,   509,
     509,   511,   520,   529,   540,   549,   558,   567,   576,   587,
     596,   608,   622,   637,   648,   665,   682,   699,   716,   731,
     746,   759,   774,   783,   792,   801,   810,   819,   828,   835,
     844,   853,   862,   871,   880,   889,   898,   907,   920,   931,
     942,   953,   962,   975,   984,   993,  1002,  1009,  1016,  1025,
    1032,  1041,  1049,  1056,  1063,  1071,  1080,  1088,  1104,  1112,
    1120,  1128,  1136,  1144,  1153,  1162,  1176,  1185,  1194,  1203,
    1212,  1221,  1230,  1237,  1244,  1270,  1278,  1285,  1292,  1299,
    1306,  1314,  1322,  1330,  1337,  1348,  1359,  1366,  1375,  1384,
    1393,  1402,  1409,  1416,  1423,  1439,  1447,  1455,  1465,  1475,
    1485,  1499,  1507,  1520,  1531,  1539,  1552,  1561,  1570,  1579,
    1588,  1598,  1608,  1616,  1629,  1638,  1646,  1655,  1663,  1676,
    1685,  1694,  1704,  1711,  1721,  1731,  1741,  1751,  1761,  1771,
    1781,  1791,  1798,  1805,  1812,  1821,  1830,  1839,  1848,  1855,
    1865,  1873,  1882,  1889,  1907,  1920,  1933,  1946,  1955,  1964,
    1973,  1982,  1992,  2002,  2013,  2022,  2031,  2040,  2049,  2058,
    2067,  2076,  2085,  2098,  2111,  2120,  2127,  2136,  2145,  2154,
    2163,  2172,  2180,  2193,  2201,  2256,  2263,  2278,  2288,  2298,
    2305,  2312,  2319,  2328,  2336,  2350,  2371,  2392,  2404,  2416,
    2428,  2437,  2458,  2470,  2482,  2491,  2512,  2521,  2530,  2538,
    2546,  2559,  2572,  2587,  2602,  2611,  2620,  2630,  2640,  2649,
    2658,  2667,  2673,  2682,  2691,  2701,  2711,  2721,  2730,  2740,
    2749,  2762,  2775,  2787,  2801,  2813,  2827,  2836,  2847,  2856,
    2863,  2873,  2880,  2887,  2896,  2905,  2915,  2925,  2935,  2945,
    2952,  2959,  2968,  2977,  2987,  2997,  3007,  3014,  3021,  3028,
    3036,  3046,  3056,  3066,  3076,  3086,  3096,  3152,  3162,  3170,
    3178,  3193,  3202,  3208,  3209,  3210,  3210,  3210,  3211,  3211,
    3211,  3212,  3212,  3214,  3224,  3233,  3240,  3247,  3254,  3261,
    3268,  3275,  3281,  3282,  3283,  3283,  3283,  3284,  3284,  3284,
    3285,  3286,  3286,  3287,  3287,  3288,  3288,  3289,  3290,  3291,
    3292,  3293,  3294,  3296,  3305,  3315,  3322,  3329,  3338,  3345,
    3352,  3359,  3366,  3375,  3384,  3391,  3398,  3408,  3418,  3428,
    3438,  3448,  3458,  3464,  3465,  3466,  3468,  3474,  3480,  3481,
    3482,  3484,  3490,  3500,  3507,  3516,  3524,  3530,  3531,  3533,
    3533,  3533,  3534,  3534,  3535,  3536,  3537,  3538,  3539,  3541,
    3551,  3560,  3567,  3576,  3583,  3592,  3600,  3613,  3621,  3634,
    3640,  3641,  3642,  3642,  3643,  3643,  3643,  3644,  3646,  3658,
    3670,  3682,  3697,  3710,  3723,  3734,  3740,  3741,  3742,  3742,
    3744,  3759
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "SPACE", "LETTER",
  "NEWLINE", "COMMENT", "COLON", "ANY", "ZONESTR", "STRING_ARG",
  "VAR_FORCE_TOPLEVEL", "VAR_SERVER", "VAR_VERBOSITY", "VAR_NUM_THREADS",
  "VAR_PORT", "VAR_OUTGOING_RANGE", "VAR_INTERFACE", "VAR_PREFER_IP4",
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
  "VAR_CONTROL_USE_CERT", "VAR_TCP_REUSE_TIMEOUT",
  "VAR_MAX_REUSE_TCP_QUERIES", "VAR_EXTENDED_STATISTICS",
  "VAR_LOCAL_DATA_PTR", "VAR_JOSTLE_TIMEOUT", "VAR_STUB_PRIME",
  "VAR_UNWANTED_REPLY_THRESHOLD", "VAR_LOG_TIME_ASCII",
  "VAR_DOMAIN_INSECURE", "VAR_PYTHON", "VAR_PYTHON_SCRIPT",
  "VAR_VAL_SIG_SKEW_MIN", "VAR_VAL_SIG_SKEW_MAX", "VAR_VAL_MAX_RESTART",
  "VAR_CACHE_MIN_TTL", "VAR_VAL_LOG_LEVEL", "VAR_AUTO_TRUST_ANCHOR_FILE",
  "VAR_KEEP_MISSING", "VAR_ADD_HOLDDOWN", "VAR_DEL_HOLDDOWN",
  "VAR_SO_RCVBUF", "VAR_EDNS_BUFFER_SIZE", "VAR_PREFETCH",
  "VAR_PREFETCH_KEY", "VAR_SO_SNDBUF", "VAR_SO_REUSEPORT",
  "VAR_HARDEN_BELOW_NXDOMAIN", "VAR_IGNORE_CD_FLAG", "VAR_LOG_QUERIES",
  "VAR_LOG_REPLIES", "VAR_LOG_LOCAL_ACTIONS", "VAR_TCP_UPSTREAM",
  "VAR_SSL_UPSTREAM", "VAR_TCP_AUTH_QUERY_TIMEOUT", "VAR_SSL_SERVICE_KEY",
  "VAR_SSL_SERVICE_PEM", "VAR_SSL_PORT", "VAR_FORWARD_FIRST",
  "VAR_STUB_SSL_UPSTREAM", "VAR_FORWARD_SSL_UPSTREAM",
  "VAR_TLS_CERT_BUNDLE", "VAR_STUB_TCP_UPSTREAM",
  "VAR_FORWARD_TCP_UPSTREAM", "VAR_HTTPS_PORT", "VAR_HTTP_ENDPOINT",
  "VAR_HTTP_MAX_STREAMS", "VAR_HTTP_QUERY_BUFFER_SIZE",
  "VAR_HTTP_RESPONSE_BUFFER_SIZE", "VAR_HTTP_NODELAY",
  "VAR_HTTP_NOTLS_DOWNSTREAM", "VAR_STUB_FIRST", "VAR_MINIMAL_RESPONSES",
  "VAR_RRSET_ROUNDROBIN", "VAR_MAX_UDP_SIZE", "VAR_DELAY_CLOSE",
  "VAR_UDP_CONNECT", "VAR_UNBLOCK_LAN_ZONES", "VAR_INSECURE_LAN_ZONES",
  "VAR_INFRA_CACHE_MIN_RTT", "VAR_INFRA_CACHE_MAX_RTT",
  "VAR_INFRA_KEEP_PROBING", "VAR_DNS64_PREFIX", "VAR_DNS64_SYNTHALL",
  "VAR_DNS64_IGNORE_AAAA", "VAR_DNSTAP", "VAR_DNSTAP_ENABLE",
  "VAR_DNSTAP_SOCKET_PATH", "VAR_DNSTAP_IP", "VAR_DNSTAP_TLS",
  "VAR_DNSTAP_TLS_SERVER_NAME", "VAR_DNSTAP_TLS_CERT_BUNDLE",
  "VAR_DNSTAP_TLS_CLIENT_KEY_FILE", "VAR_DNSTAP_TLS_CLIENT_CERT_FILE",
  "VAR_DNSTAP_SEND_IDENTITY", "VAR_DNSTAP_SEND_VERSION",
  "VAR_DNSTAP_BIDIRECTIONAL", "VAR_DNSTAP_IDENTITY", "VAR_DNSTAP_VERSION",
  "VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES",
  "VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES",
  "VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES",
  "VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES",
  "VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES",
  "VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES", "VAR_RESPONSE_IP_TAG",
  "VAR_RESPONSE_IP", "VAR_RESPONSE_IP_DATA", "VAR_HARDEN_ALGO_DOWNGRADE",
  "VAR_IP_TRANSPARENT", "VAR_IP_DSCP", "VAR_DISABLE_DNSSEC_LAME_CHECK",
  "VAR_IP_RATELIMIT", "VAR_IP_RATELIMIT_SLABS", "VAR_IP_RATELIMIT_SIZE",
  "VAR_RATELIMIT", "VAR_RATELIMIT_SLABS", "VAR_RATELIMIT_SIZE",
  "VAR_OUTBOUND_MSG_RETRY", "VAR_MAX_SENT_COUNT", "VAR_MAX_QUERY_RESTARTS",
  "VAR_RATELIMIT_FOR_DOMAIN", "VAR_RATELIMIT_BELOW_DOMAIN",
  "VAR_IP_RATELIMIT_FACTOR", "VAR_RATELIMIT_FACTOR",
  "VAR_IP_RATELIMIT_BACKOFF", "VAR_RATELIMIT_BACKOFF",
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
  "VAR_SERVE_EXPIRED_CLIENT_TIMEOUT", "VAR_EDE_SERVE_EXPIRED",
  "VAR_SERVE_ORIGINAL_TTL", "VAR_FAKE_DSA", "VAR_FAKE_SHA1",
  "VAR_LOG_IDENTITY", "VAR_HIDE_TRUSTANCHOR", "VAR_HIDE_HTTP_USER_AGENT",
  "VAR_HTTP_USER_AGENT", "VAR_TRUST_ANCHOR_SIGNALING",
  "VAR_AGGRESSIVE_NSEC", "VAR_USE_SYSTEMD", "VAR_SHM_ENABLE",
  "VAR_SHM_KEY", "VAR_ROOT_KEY_SENTINEL", "VAR_DNSCRYPT",
  "VAR_DNSCRYPT_ENABLE", "VAR_DNSCRYPT_PORT", "VAR_DNSCRYPT_PROVIDER",
  "VAR_DNSCRYPT_SECRET_KEY", "VAR_DNSCRYPT_PROVIDER_CERT",
  "VAR_DNSCRYPT_PROVIDER_CERT_ROTATED",
  "VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE",
  "VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS",
  "VAR_DNSCRYPT_NONCE_CACHE_SIZE", "VAR_DNSCRYPT_NONCE_CACHE_SLABS",
  "VAR_PAD_RESPONSES", "VAR_PAD_RESPONSES_BLOCK_SIZE", "VAR_PAD_QUERIES",
  "VAR_PAD_QUERIES_BLOCK_SIZE", "VAR_IPSECMOD_ENABLED",
  "VAR_IPSECMOD_HOOK", "VAR_IPSECMOD_IGNORE_BOGUS", "VAR_IPSECMOD_MAX_TTL",
  "VAR_IPSECMOD_WHITELIST", "VAR_IPSECMOD_STRICT", "VAR_CACHEDB",
  "VAR_CACHEDB_BACKEND", "VAR_CACHEDB_SECRETSEED", "VAR_CACHEDB_REDISHOST",
  "VAR_CACHEDB_REDISPORT", "VAR_CACHEDB_REDISTIMEOUT",
  "VAR_CACHEDB_REDISEXPIRERECORDS", "VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM",
  "VAR_FOR_UPSTREAM", "VAR_AUTH_ZONE", "VAR_ZONEFILE", "VAR_MASTER",
  "VAR_URL", "VAR_FOR_DOWNSTREAM", "VAR_FALLBACK_ENABLED",
  "VAR_TLS_ADDITIONAL_PORT", "VAR_LOW_RTT", "VAR_LOW_RTT_PERMIL",
  "VAR_FAST_SERVER_PERMIL", "VAR_FAST_SERVER_NUM", "VAR_ALLOW_NOTIFY",
  "VAR_TLS_WIN_CERT", "VAR_TCP_CONNECTION_LIMIT", "VAR_FORWARD_NO_CACHE",
  "VAR_STUB_NO_CACHE", "VAR_LOG_SERVFAIL", "VAR_DENY_ANY",
  "VAR_UNKNOWN_SERVER_TIME_LIMIT", "VAR_LOG_TAG_QUERYREPLY",
  "VAR_STREAM_WAIT_SIZE", "VAR_TLS_CIPHERS", "VAR_TLS_CIPHERSUITES",
  "VAR_TLS_USE_SNI", "VAR_IPSET", "VAR_IPSET_NAME_V4", "VAR_IPSET_NAME_V6",
  "VAR_TLS_SESSION_TICKET_KEYS", "VAR_RPZ", "VAR_TAGS",
  "VAR_RPZ_ACTION_OVERRIDE", "VAR_RPZ_CNAME_OVERRIDE", "VAR_RPZ_LOG",
  "VAR_RPZ_LOG_NAME", "VAR_DYNLIB", "VAR_DYNLIB_FILE",
  "VAR_EDNS_CLIENT_STRING", "VAR_EDNS_CLIENT_STRING_OPCODE", "VAR_NSID",
  "VAR_ZONEMD_PERMISSIVE_MODE", "VAR_ZONEMD_CHECK",
  "VAR_ZONEMD_REJECT_ABSENCE", "VAR_RPZ_SIGNAL_NXDOMAIN_RA",
  "VAR_INTERFACE_AUTOMATIC_PORTS", "VAR_EDE", "VAR_INTERFACE_ACTION",
  "VAR_INTERFACE_VIEW", "VAR_INTERFACE_TAG", "VAR_INTERFACE_TAG_ACTION",
  "VAR_INTERFACE_TAG_DATA", "VAR_PROXY_PROTOCOL_PORT",
  "VAR_STATISTICS_INHIBIT_ZERO", "$accept", "toplevelvars", "toplevelvar",
  "force_toplevel", "serverstart", "contents_server", "content_server",
  "stubstart", "contents_stub", "content_stub", "forwardstart",
  "contents_forward", "content_forward", "viewstart", "contents_view",
  "content_view", "authstart", "contents_auth", "content_auth", "rpz_tag",
  "rpz_action_override", "rpz_cname_override", "rpz_log", "rpz_log_name",
  "rpz_signal_nxdomain_ra", "rpzstart", "contents_rpz", "content_rpz",
  "server_num_threads", "server_verbosity", "server_statistics_interval",
  "server_statistics_cumulative", "server_extended_statistics",
  "server_statistics_inhibit_zero", "server_shm_enable", "server_shm_key",
  "server_port", "server_send_client_subnet", "server_client_subnet_zone",
  "server_client_subnet_always_forward", "server_client_subnet_opcode",
  "server_max_client_subnet_ipv4", "server_max_client_subnet_ipv6",
  "server_min_client_subnet_ipv4", "server_min_client_subnet_ipv6",
  "server_max_ecs_tree_size_ipv4", "server_max_ecs_tree_size_ipv6",
  "server_interface", "server_outgoing_interface", "server_outgoing_range",
  "server_outgoing_port_permit", "server_outgoing_port_avoid",
  "server_outgoing_num_tcp", "server_incoming_num_tcp",
  "server_interface_automatic", "server_interface_automatic_ports",
  "server_do_ip4", "server_do_ip6", "server_do_udp", "server_do_tcp",
  "server_prefer_ip4", "server_prefer_ip6", "server_tcp_mss",
  "server_outgoing_tcp_mss", "server_tcp_idle_timeout",
  "server_max_reuse_tcp_queries", "server_tcp_reuse_timeout",
  "server_tcp_auth_query_timeout", "server_tcp_keepalive",
  "server_tcp_keepalive_timeout", "server_tcp_upstream",
  "server_udp_upstream_without_downstream", "server_ssl_upstream",
  "server_ssl_service_key", "server_ssl_service_pem", "server_ssl_port",
  "server_tls_cert_bundle", "server_tls_win_cert",
  "server_tls_additional_port", "server_tls_ciphers",
  "server_tls_ciphersuites", "server_tls_session_ticket_keys",
  "server_tls_use_sni", "server_https_port", "server_http_endpoint",
  "server_http_max_streams", "server_http_query_buffer_size",
  "server_http_response_buffer_size", "server_http_nodelay",
  "server_http_notls_downstream", "server_use_systemd",
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
  "server_hide_http_user_agent", "server_identity", "server_version",
  "server_http_user_agent", "server_nsid", "server_so_rcvbuf",
  "server_so_sndbuf", "server_so_reuseport", "server_ip_transparent",
  "server_ip_freebind", "server_ip_dscp", "server_stream_wait_size",
  "server_edns_buffer_size", "server_msg_buffer_size",
  "server_msg_cache_size", "server_msg_cache_slabs",
  "server_num_queries_per_thread", "server_jostle_timeout",
  "server_delay_close", "server_udp_connect", "server_unblock_lan_zones",
  "server_insecure_lan_zones", "server_rrset_cache_size",
  "server_rrset_cache_slabs", "server_infra_host_ttl",
  "server_infra_lame_ttl", "server_infra_cache_numhosts",
  "server_infra_cache_lame_size", "server_infra_cache_slabs",
  "server_infra_cache_min_rtt", "server_infra_cache_max_rtt",
  "server_infra_keep_probing", "server_target_fetch_policy",
  "server_harden_short_bufsize", "server_harden_large_queries",
  "server_harden_glue", "server_harden_dnssec_stripped",
  "server_harden_below_nxdomain", "server_harden_referral_path",
  "server_harden_algo_downgrade", "server_use_caps_for_id",
  "server_caps_whitelist", "server_private_address",
  "server_private_domain", "server_prefetch", "server_prefetch_key",
  "server_deny_any", "server_unwanted_reply_threshold",
  "server_do_not_query_address", "server_do_not_query_localhost",
  "server_access_control", "server_interface_action", "server_module_conf",
  "server_val_override_date", "server_val_sig_skew_min",
  "server_val_sig_skew_max", "server_val_max_restart",
  "server_cache_max_ttl", "server_cache_max_negative_ttl",
  "server_cache_min_ttl", "server_bogus_ttl",
  "server_val_clean_additional", "server_val_permissive_mode",
  "server_aggressive_nsec", "server_ignore_cd_flag",
  "server_serve_expired", "server_serve_expired_ttl",
  "server_serve_expired_ttl_reset", "server_serve_expired_reply_ttl",
  "server_serve_expired_client_timeout", "server_ede_serve_expired",
  "server_serve_original_ttl", "server_fake_dsa", "server_fake_sha1",
  "server_val_log_level", "server_val_nsec3_keysize_iterations",
  "server_zonemd_permissive_mode", "server_add_holddown",
  "server_del_holddown", "server_keep_missing",
  "server_permit_small_holddown", "server_key_cache_size",
  "server_key_cache_slabs", "server_neg_cache_size", "server_local_zone",
  "server_local_data", "server_local_data_ptr", "server_minimal_responses",
  "server_rrset_roundrobin", "server_unknown_server_time_limit",
  "server_max_udp_size", "server_dns64_prefix", "server_dns64_synthall",
  "server_dns64_ignore_aaaa", "server_define_tag", "server_local_zone_tag",
  "server_access_control_tag", "server_access_control_tag_action",
  "server_access_control_tag_data", "server_local_zone_override",
  "server_access_control_view", "server_interface_tag",
  "server_interface_tag_action", "server_interface_tag_data",
  "server_interface_view", "server_response_ip_tag", "server_ip_ratelimit",
  "server_ratelimit", "server_ip_ratelimit_size", "server_ratelimit_size",
  "server_ip_ratelimit_slabs", "server_ratelimit_slabs",
  "server_ratelimit_for_domain", "server_ratelimit_below_domain",
  "server_ip_ratelimit_factor", "server_ratelimit_factor",
  "server_ip_ratelimit_backoff", "server_ratelimit_backoff",
  "server_outbound_msg_retry", "server_max_sent_count",
  "server_max_query_restarts", "server_low_rtt", "server_fast_server_num",
  "server_fast_server_permil", "server_qname_minimisation",
  "server_qname_minimisation_strict", "server_pad_responses",
  "server_pad_responses_block_size", "server_pad_queries",
  "server_pad_queries_block_size", "server_ipsecmod_enabled",
  "server_ipsecmod_ignore_bogus", "server_ipsecmod_hook",
  "server_ipsecmod_max_ttl", "server_ipsecmod_whitelist",
  "server_ipsecmod_strict", "server_edns_client_string",
  "server_edns_client_string_opcode", "server_ede",
  "server_proxy_protocol_port", "stub_name", "stub_host", "stub_addr",
  "stub_first", "stub_no_cache", "stub_ssl_upstream", "stub_tcp_upstream",
  "stub_prime", "forward_name", "forward_host", "forward_addr",
  "forward_first", "forward_no_cache", "forward_ssl_upstream",
  "forward_tcp_upstream", "auth_name", "auth_zonefile", "auth_master",
  "auth_url", "auth_allow_notify", "auth_zonemd_check",
  "auth_zonemd_reject_absence", "auth_for_downstream", "auth_for_upstream",
  "auth_fallback_enabled", "view_name", "view_local_zone",
  "view_response_ip", "view_response_ip_data", "view_local_data",
  "view_local_data_ptr", "view_first", "rcstart", "contents_rc",
  "content_rc", "rc_control_enable", "rc_control_port",
  "rc_control_interface", "rc_control_use_cert", "rc_server_key_file",
  "rc_server_cert_file", "rc_control_key_file", "rc_control_cert_file",
  "dtstart", "contents_dt", "content_dt", "dt_dnstap_enable",
  "dt_dnstap_bidirectional", "dt_dnstap_socket_path", "dt_dnstap_ip",
  "dt_dnstap_tls", "dt_dnstap_tls_server_name",
  "dt_dnstap_tls_cert_bundle", "dt_dnstap_tls_client_key_file",
  "dt_dnstap_tls_client_cert_file", "dt_dnstap_send_identity",
  "dt_dnstap_send_version", "dt_dnstap_identity", "dt_dnstap_version",
  "dt_dnstap_log_resolver_query_messages",
  "dt_dnstap_log_resolver_response_messages",
  "dt_dnstap_log_client_query_messages",
  "dt_dnstap_log_client_response_messages",
  "dt_dnstap_log_forwarder_query_messages",
  "dt_dnstap_log_forwarder_response_messages", "pythonstart",
  "contents_py", "content_py", "py_script", "dynlibstart", "contents_dl",
  "content_dl", "dl_file", "server_disable_dnssec_lame_check",
  "server_log_identity", "server_response_ip", "server_response_ip_data",
  "dnscstart", "contents_dnsc", "content_dnsc", "dnsc_dnscrypt_enable",
  "dnsc_dnscrypt_port", "dnsc_dnscrypt_provider",
  "dnsc_dnscrypt_provider_cert", "dnsc_dnscrypt_provider_cert_rotated",
  "dnsc_dnscrypt_secret_key", "dnsc_dnscrypt_shared_secret_cache_size",
  "dnsc_dnscrypt_shared_secret_cache_slabs",
  "dnsc_dnscrypt_nonce_cache_size", "dnsc_dnscrypt_nonce_cache_slabs",
  "cachedbstart", "contents_cachedb", "content_cachedb",
  "cachedb_backend_name", "cachedb_secret_seed", "redis_server_host",
  "redis_server_port", "redis_timeout", "redis_expire_records",
  "server_tcp_connection_limit", "ipsetstart", "contents_ipset",
  "content_ipset", "ipset_name_v4", "ipset_name_v6", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
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
     525,   526,   527,   528,   529,   530,   531,   532,   533,   534,
     535,   536,   537,   538,   539,   540,   541,   542,   543,   544,
     545,   546,   547,   548,   549,   550,   551,   552,   553,   554,
     555,   556,   557,   558,   559,   560,   561,   562,   563,   564,
     565,   566,   567,   568,   569,   570,   571,   572,   573,   574,
     575,   576,   577,   578,   579,   580,   581,   582,   583,   584,
     585,   586,   587,   588,   589,   590,   591
};
#endif

#define YYPACT_NINF (-286)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -286,   252,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,   -13,   203,   220,    52,    84,    38,   238,   211,
     -81,  -285,   -95,  -193,  -278,    29,    30,    31,    80,    81,
      91,    92,   120,   121,   132,   146,   147,   148,   149,   161,
     162,   163,   164,   165,   210,   212,   234,   235,   236,   237,
     239,   256,   257,   258,   259,   261,   262,   265,   266,   267,
     270,   273,   276,   286,   287,   290,   291,   292,   293,   295,
     296,   297,   302,   304,   318,   319,   320,   321,   322,   323,
     333,   334,   335,   337,   340,   341,   347,   349,   350,   351,
     353,   359,   365,   366,   367,   368,   369,   390,   391,   392,
     393,   394,   395,   396,   397,   398,   401,   402,   403,   404,
     405,   406,   407,   408,   409,   410,   412,   413,   414,   415,
     416,   417,   418,   419,   420,   421,   422,   423,   424,   425,
     426,   427,   428,   429,   430,   431,   432,   433,   434,   435,
     436,   437,   438,   439,   440,   441,   442,   443,   444,   445,
     446,   447,   448,   449,   450,   451,   452,   453,   454,   455,
     456,   457,   458,   459,   460,   461,   462,   463,   464,   465,
     466,   467,   468,   469,   470,   471,   472,   473,   474,   476,
     477,   478,   479,   480,   481,   482,   483,   484,   485,   486,
     487,   488,   489,   490,   491,   492,   494,   495,   496,   498,
     499,   500,   501,   502,   503,   504,   505,   506,   507,   508,
     510,   511,   512,   513,   514,   515,   516,   517,   519,   520,
     521,   522,   523,   524,   525,   526,   528,   529,   530,   531,
     532,   533,   534,   535,   536,   537,   538,   539,   540,   541,
     542,   543,   544,   545,   546,   547,   548,   549,   550,   552,
     553,   554,   556,   557,   558,   559,   560,   562,   563,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,   564,   565,   566,   567,   568,   569,
     570,   571,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,   572,   573,   574,   575,   576,   577,   578,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,   579,   580,   581,   582,
     583,   584,   585,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,   586,   587,   588,   589,   590,   591,   592,   593,   594,
     595,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,   596,   597,   598,   599,   600,   601,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,   602,   603,   604,   605,   606,   607,   608,   609,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,   610,   611,
     612,   613,   614,   615,   616,   617,   618,   619,   620,   621,
     622,   623,   624,   625,   626,   627,   628,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,   629,  -286,  -286,
     630,  -286,  -286,   631,   632,   633,   634,   635,   636,   637,
     638,   639,   640,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,   641,   642,   643,   644,   645,   646,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,   647,   648,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,   649,   650,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,   651,   652,   653,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,   654,
     655,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,   656,   657,   658,   659,   660,   661,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,   662,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,   663,  -286,  -286,  -286,  -286,  -286,   664,
     665,   666,   667,   668,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,   669,  -286,  -286,   670,   671,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
     672,   673,   674,  -286,  -286,  -286,  -286,  -286,  -286,   675,
     676,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       2,     0,     1,    18,    19,   256,   267,   582,   642,   601,
     277,   656,   679,   287,   695,   306,   647,     3,    17,    21,
     258,   269,   279,   289,   308,   584,   603,   644,   649,   658,
     681,   697,     4,     5,     6,    10,    14,    15,     8,     9,
       7,    16,    11,    12,    13,     0,     0,     0,     0,     0,
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
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    20,
      22,    23,    88,    91,   100,   255,   215,   216,    24,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,    37,
      79,    25,    92,    93,    48,    72,    87,   252,    26,    27,
      30,    31,    28,    29,    32,    33,    34,   249,   250,   251,
      35,    36,   124,   227,   125,   127,   128,   129,   229,   234,
     230,   241,   242,   243,   244,   130,   131,   132,   133,   134,
     135,   136,   211,    89,    78,   104,   122,   123,   239,   236,
     126,    38,    39,    40,    41,    42,    80,    94,    95,   111,
      66,    76,    67,   219,   220,   105,    58,    59,   218,    62,
      60,    61,    63,   247,   115,   119,   140,   151,   183,   154,
     240,   116,    73,    43,    44,    45,   102,   141,   142,   143,
     144,    46,    47,    49,    50,    52,    53,    51,   148,   149,
     155,    54,    55,    56,    64,    83,   120,    97,   150,    90,
     179,    98,    99,   117,   118,   237,   103,    57,    81,    84,
     192,    65,    68,   106,   107,   108,    82,   180,   109,    69,
      70,    71,   228,   121,   202,   203,   204,   205,   206,   207,
     208,   209,   217,   110,    77,   248,   112,   113,   114,   181,
      74,    75,    96,    85,    86,   101,   137,   138,   238,   139,
     145,   146,   147,   184,   185,   187,   189,   190,   188,   191,
     194,   195,   196,   193,   212,   152,   153,   158,   159,   156,
     157,   160,   161,   163,   162,   165,   164,   166,   167,   168,
     231,   233,   232,   182,   197,   198,   199,   200,   201,   221,
     223,   222,   224,   225,   226,   245,   246,   253,   254,   186,
     210,   213,   214,   235,     0,     0,     0,     0,     0,     0,
       0,     0,   257,   259,   260,   261,   263,   264,   265,   266,
     262,     0,     0,     0,     0,     0,     0,     0,   268,   270,
     271,   272,   273,   274,   275,   276,     0,     0,     0,     0,
       0,     0,     0,   278,   280,   281,   284,   285,   282,   286,
     283,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   288,   290,   291,   292,   293,   297,   298,   299,   294,
     295,   296,     0,     0,     0,     0,     0,     0,   311,   315,
     316,   317,   318,   319,   307,   309,   310,   312,   313,   314,
     320,     0,     0,     0,     0,     0,     0,     0,     0,   583,
     585,   587,   586,   592,   588,   589,   590,   591,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   602,   604,   606,
     605,   607,   608,   609,   610,   611,   612,   613,   614,   615,
     616,   617,   618,   619,   620,   621,   622,     0,   643,   645,
       0,   648,   650,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   657,   659,   660,   661,   663,   664,   662,
     665,   666,   667,   668,     0,     0,     0,     0,     0,     0,
     680,   682,   683,   684,   685,   686,   687,     0,     0,   696,
     698,   699,   322,   321,   329,   342,   340,   353,   349,   350,
     354,   351,   352,   355,   356,   357,   361,   362,   392,   393,
     394,   395,   396,   424,   425,   426,   432,   433,   345,   434,
     435,   438,   436,   437,   442,   443,   444,   458,   407,   408,
     411,   412,   445,   462,   401,   403,   463,   470,   471,   472,
     346,   423,   491,   492,   402,   485,   385,   341,   397,   459,
     467,   446,     0,     0,   495,   347,   323,   384,   450,   324,
     343,   344,   398,   399,   493,   448,   452,   453,   359,   358,
     325,   496,   427,   457,   386,   406,   464,   465,   466,   469,
     484,   400,   489,   487,   488,   415,   422,   454,   455,   416,
     417,   447,   474,   387,   388,   391,   363,   365,   360,   366,
     367,   368,   369,   376,   377,   378,   379,   380,   381,   382,
     497,   498,   500,   428,   429,   430,   431,   439,   440,   441,
     501,   502,   503,     0,     0,     0,   449,   418,   420,   652,
     516,   520,   518,   517,   521,   519,   528,   529,   530,     0,
       0,   524,   525,   526,   527,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   451,   468,   490,   534,   535,
     419,   504,     0,     0,     0,     0,     0,     0,   475,   476,
     477,   478,   479,   480,   481,   482,   483,   653,   409,   410,
     413,   404,   473,   383,   327,   328,   405,   536,   537,   538,
     539,   540,   542,   541,   543,   544,   545,   364,   371,   531,
     533,   532,   370,     0,   390,   456,   499,   389,   421,   372,
     373,   375,   374,     0,   547,   414,   486,   348,   548,     0,
       0,     0,     0,     0,   549,   326,   550,   551,   552,   557,
     555,   556,   553,   554,   558,   559,   560,   561,   563,   564,
     562,   575,     0,   579,   580,     0,     0,   581,   565,   573,
     566,   567,   568,   572,   574,   569,   570,   571,   300,   301,
     302,   303,   304,   305,   593,   595,   594,   597,   598,   599,
     600,   596,   623,   625,   626,   627,   628,   629,   630,   631,
     632,   633,   624,   634,   635,   636,   637,   638,   639,   640,
     641,   646,   651,   669,   670,   671,   674,   672,   673,   675,
     676,   677,   678,   688,   689,   690,   691,   692,   693,   700,
     701,   460,   494,   515,   654,   655,   522,   523,   505,   506,
       0,     0,     0,   510,   694,   546,   461,   514,   511,     0,
       0,   576,   577,   578,   509,   507,   508,   512,   513
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,   677,   678,   679,   680,   681,  -286,  -286,   682,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,  -286,
    -286,  -286
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     1,    17,    18,    19,    32,   279,    20,    33,   522,
      21,    34,   538,    22,    35,   553,    23,    36,   571,   588,
     589,   590,   591,   592,   593,    24,    37,   594,   280,   281,
     282,   283,   284,   285,   286,   287,   288,   289,   290,   291,
     292,   293,   294,   295,   296,   297,   298,   299,   300,   301,
     302,   303,   304,   305,   306,   307,   308,   309,   310,   311,
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
     482,   483,   484,   485,   486,   487,   488,   489,   490,   491,
     492,   493,   494,   495,   496,   497,   498,   499,   500,   501,
     502,   503,   504,   505,   506,   507,   508,   523,   524,   525,
     526,   527,   528,   529,   530,   539,   540,   541,   542,   543,
     544,   545,   572,   573,   574,   575,   576,   577,   578,   579,
     580,   581,   554,   555,   556,   557,   558,   559,   560,    25,
      38,   609,   610,   611,   612,   613,   614,   615,   616,   617,
      26,    39,   637,   638,   639,   640,   641,   642,   643,   644,
     645,   646,   647,   648,   649,   650,   651,   652,   653,   654,
     655,   656,    27,    40,   658,   659,    28,    41,   661,   662,
     509,   510,   511,   512,    29,    42,   673,   674,   675,   676,
     677,   678,   679,   680,   681,   682,   683,    30,    43,   690,
     691,   692,   693,   694,   695,   696,   513,    31,    44,   699,
     700,   701
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,   697,   698,   657,   660,    77,    78,    79,   702,
     703,   704,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   561,   684,   685,   686,   687,   688,   689,
     705,   706,   121,   122,   123,   124,   125,   546,   126,   127,
     128,   707,   708,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   561,
     709,   710,   155,   547,   548,   156,   157,   158,   159,   160,
     161,   162,   711,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   712,   713,   714,   715,
     549,   663,   664,   665,   666,   667,   668,   669,   670,   671,
     672,   716,   717,   718,   719,   720,   176,   177,   178,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,   189,
     190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,   202,   203,   204,   205,   206,   207,   208,   209,
     210,   211,   212,   213,   214,   215,   216,   217,   218,   219,
     721,   220,   722,   221,   222,   223,   224,   225,   226,   227,
     228,   229,   230,   231,   232,   233,   234,   235,   236,   237,
     238,   239,   550,   551,   723,   724,   725,   726,   514,   727,
     515,   516,     2,   240,   241,   242,   243,   244,   245,   246,
     247,   248,   249,     3,     4,   531,   728,   729,   730,   731,
     250,   732,   733,   532,   533,   734,   735,   736,   251,   252,
     737,   253,   254,   738,   255,   256,   739,   552,   257,   258,
     259,   260,   261,   262,   263,   264,   740,   741,     5,   265,
     742,   743,   744,   745,     6,   746,   747,   748,   266,   267,
     268,   269,   749,   517,   750,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   563,   564,   565,   566,   751,   752,
     753,   754,   755,   756,   568,   601,   602,   603,   604,   605,
     606,   607,   608,   757,   758,   759,   518,   760,     7,   519,
     761,   762,   582,   583,   584,   585,   586,   763,   520,   764,
     765,   766,   534,   767,   535,   587,     8,   536,   562,   768,
     563,   564,   565,   566,   567,   769,   770,   771,   772,   773,
     568,   618,   619,   620,   621,   622,   623,   624,   625,   626,
     627,   628,   629,   630,   631,   632,   633,   634,   635,   636,
     774,   775,   776,   777,   778,   779,   780,   781,   782,   569,
     570,   783,   784,   785,   786,   787,   788,   789,   790,   791,
     792,     9,   793,   794,   795,   796,   797,   798,   799,   800,
     801,   802,   803,   804,   805,   806,   807,   808,   809,   810,
     811,   812,   813,   814,   815,   816,   817,   818,   819,   820,
     821,   822,   823,   824,   825,   826,   827,   828,   829,   830,
     831,   832,   833,   834,   835,   836,   837,   838,   839,   840,
     841,   842,   843,   844,   845,   846,   847,   848,   849,   850,
     851,   852,   853,   854,   855,    10,   856,   857,   858,   859,
     860,   861,   862,   863,   864,   865,   866,   867,   868,   869,
     870,   871,   872,   521,   873,   874,   875,    11,   876,   877,
     878,   879,   880,   881,   882,   883,   884,   885,   886,   537,
     887,   888,   889,   890,   891,   892,   893,   894,    12,   895,
     896,   897,   898,   899,   900,   901,   902,    13,   903,   904,
     905,   906,   907,   908,   909,   910,   911,   912,   913,   914,
     915,   916,   917,   918,   919,   920,   921,   922,   923,   924,
     925,    14,   926,   927,   928,    15,   929,   930,   931,   932,
     933,    16,   934,   935,   936,   937,   938,   939,   940,   941,
     942,   943,   944,   945,   946,   947,   948,   949,   950,   951,
     952,   953,   954,   955,   956,   957,   958,   959,   960,   961,
     962,   963,   964,   965,   966,   967,   968,   969,   970,   971,
     972,   973,   974,   975,   976,   977,   978,   979,   980,   981,
     982,   983,   984,   985,   986,   987,   988,   989,   990,   991,
     992,   993,   994,   995,   996,   997,   998,   999,  1000,  1001,
    1002,  1003,  1004,  1005,  1006,  1007,  1008,  1009,  1010,  1011,
    1012,  1013,  1014,  1015,  1016,  1017,  1018,  1019,  1020,  1021,
    1022,  1023,  1024,  1025,  1026,  1027,  1028,  1029,  1030,  1031,
    1032,  1033,  1034,  1035,  1036,  1037,  1038,  1039,  1040,  1041,
    1042,  1043,  1044,  1045,  1046,  1047,  1048,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   595,   596,   597,   598,   599,   600
};

static const yytype_int16 yycheck[] =
{
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,   310,   311,   115,   320,    49,    50,    51,    10,
      10,    10,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    45,   277,   278,   279,   280,   281,   282,
      10,    10,   105,   106,   107,   108,   109,    45,   111,   112,
     113,    10,    10,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,    45,
      10,    10,   145,    81,    82,   148,   149,   150,   151,   152,
     153,   154,    10,   156,   157,   158,   159,   160,   161,   162,
     163,   164,   165,   166,   167,   168,    10,    10,    10,    10,
     108,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,    10,    10,    10,    10,    10,   189,   190,   191,   192,
     193,   194,   195,   196,   197,   198,   199,   200,   201,   202,
     203,   204,   205,   206,   207,   208,   209,   210,   211,   212,
     213,   214,   215,   216,   217,   218,   219,   220,   221,   222,
     223,   224,   225,   226,   227,   228,   229,   230,   231,   232,
      10,   234,    10,   236,   237,   238,   239,   240,   241,   242,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   190,   191,    10,    10,    10,    10,    45,    10,
      47,    48,     0,   266,   267,   268,   269,   270,   271,   272,
     273,   274,   275,    11,    12,    45,    10,    10,    10,    10,
     283,    10,    10,    53,    54,    10,    10,    10,   291,   292,
      10,   294,   295,    10,   297,   298,    10,   235,   301,   302,
     303,   304,   305,   306,   307,   308,    10,    10,    46,   312,
      10,    10,    10,    10,    52,    10,    10,    10,   321,   322,
     323,   324,    10,   110,    10,   328,   329,   330,   331,   332,
     333,   334,   335,   336,   286,   287,   288,   289,    10,    10,
      10,    10,    10,    10,   296,    97,    98,    99,   100,   101,
     102,   103,   104,    10,    10,    10,   143,    10,    96,   146,
      10,    10,   314,   315,   316,   317,   318,    10,   155,    10,
      10,    10,   142,    10,   144,   327,   114,   147,   284,    10,
     286,   287,   288,   289,   290,    10,    10,    10,    10,    10,
     296,   170,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,   181,   182,   183,   184,   185,   186,   187,   188,
      10,    10,    10,    10,    10,    10,    10,    10,    10,   325,
     326,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,   169,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,   233,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,   300,    10,    10,    10,   255,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,   299,
      10,    10,    10,    10,    10,    10,    10,    10,   276,    10,
      10,    10,    10,    10,    10,    10,    10,   285,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,   309,    10,    10,    10,   313,    10,    10,    10,    10,
      10,   319,    10,    10,    10,    10,    10,    10,    10,    10,
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
      10,    10,    10,    10,    10,    10,    10,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    37,    37,    37,    37,    37,    37
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_int16 yystos[] =
{
       0,   338,     0,    11,    12,    46,    52,    96,   114,   169,
     233,   255,   276,   285,   309,   313,   319,   339,   340,   341,
     344,   347,   350,   353,   362,   626,   637,   659,   663,   671,
     684,   694,   342,   345,   348,   351,   354,   363,   627,   638,
     660,   664,   672,   685,   695,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    49,    50,    51,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,   105,   106,   107,   108,   109,   111,   112,   113,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   145,   148,   149,   150,   151,
     152,   153,   154,   156,   157,   158,   159,   160,   161,   162,
     163,   164,   165,   166,   167,   168,   189,   190,   191,   192,
     193,   194,   195,   196,   197,   198,   199,   200,   201,   202,
     203,   204,   205,   206,   207,   208,   209,   210,   211,   212,
     213,   214,   215,   216,   217,   218,   219,   220,   221,   222,
     223,   224,   225,   226,   227,   228,   229,   230,   231,   232,
     234,   236,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     266,   267,   268,   269,   270,   271,   272,   273,   274,   275,
     283,   291,   292,   294,   295,   297,   298,   301,   302,   303,
     304,   305,   306,   307,   308,   312,   321,   322,   323,   324,
     328,   329,   330,   331,   332,   333,   334,   335,   336,   343,
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
     525,   526,   527,   528,   529,   530,   531,   532,   533,   534,
     535,   536,   537,   538,   539,   540,   541,   542,   543,   544,
     545,   546,   547,   548,   549,   550,   551,   552,   553,   554,
     555,   556,   557,   558,   559,   560,   561,   562,   563,   564,
     565,   566,   567,   568,   569,   570,   571,   572,   573,   574,
     575,   576,   577,   578,   579,   580,   581,   582,   583,   584,
     585,   586,   587,   588,   589,   590,   591,   592,   593,   667,
     668,   669,   670,   693,    45,    47,    48,   110,   143,   146,
     155,   300,   346,   594,   595,   596,   597,   598,   599,   600,
     601,    45,    53,    54,   142,   144,   147,   299,   349,   602,
     603,   604,   605,   606,   607,   608,    45,    81,    82,   108,
     190,   191,   235,   352,   619,   620,   621,   622,   623,   624,
     625,    45,   284,   286,   287,   288,   289,   290,   296,   325,
     326,   355,   609,   610,   611,   612,   613,   614,   615,   616,
     617,   618,   314,   315,   316,   317,   318,   327,   356,   357,
     358,   359,   360,   361,   364,   609,   610,   611,   612,   613,
     616,    97,    98,    99,   100,   101,   102,   103,   104,   628,
     629,   630,   631,   632,   633,   634,   635,   636,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,   639,   640,   641,
     642,   643,   644,   645,   646,   647,   648,   649,   650,   651,
     652,   653,   654,   655,   656,   657,   658,   115,   661,   662,
     320,   665,   666,   256,   257,   258,   259,   260,   261,   262,
     263,   264,   265,   673,   674,   675,   676,   677,   678,   679,
     680,   681,   682,   683,   277,   278,   279,   280,   281,   282,
     686,   687,   688,   689,   690,   691,   692,   310,   311,   696,
     697,   698,    10,    10,    10,    10,    10,    10,    10,    10,
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
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int16 yyr1[] =
{
       0,   337,   338,   338,   339,   339,   339,   339,   339,   339,
     339,   339,   339,   339,   339,   339,   339,   339,   340,   341,
     342,   342,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   343,   343,   343,   343,
     343,   343,   343,   343,   343,   343,   344,   345,   345,   346,
     346,   346,   346,   346,   346,   346,   346,   347,   348,   348,
     349,   349,   349,   349,   349,   349,   349,   350,   351,   351,
     352,   352,   352,   352,   352,   352,   352,   353,   354,   354,
     355,   355,   355,   355,   355,   355,   355,   355,   355,   355,
     356,   357,   358,   359,   360,   361,   362,   363,   363,   364,
     364,   364,   364,   364,   364,   364,   364,   364,   364,   364,
     364,   365,   366,   367,   368,   369,   370,   371,   372,   373,
     374,   375,   376,   377,   378,   379,   380,   381,   382,   383,
     384,   385,   386,   387,   388,   389,   390,   391,   392,   393,
     394,   395,   396,   397,   398,   399,   400,   401,   402,   403,
     404,   405,   406,   407,   408,   409,   410,   411,   412,   413,
     414,   415,   416,   417,   418,   419,   420,   421,   422,   423,
     424,   425,   426,   427,   428,   429,   430,   431,   432,   433,
     434,   435,   436,   437,   438,   439,   440,   441,   442,   443,
     444,   445,   446,   447,   448,   449,   450,   451,   452,   453,
     454,   455,   456,   457,   458,   459,   460,   461,   462,   463,
     464,   465,   466,   467,   468,   469,   470,   471,   472,   473,
     474,   475,   476,   477,   478,   479,   480,   481,   482,   483,
     484,   485,   486,   487,   488,   489,   490,   491,   492,   493,
     494,   495,   496,   497,   498,   499,   500,   501,   502,   503,
     504,   505,   506,   507,   508,   509,   510,   511,   512,   513,
     514,   515,   516,   517,   518,   519,   520,   521,   522,   523,
     524,   525,   526,   527,   528,   529,   530,   531,   532,   533,
     534,   535,   536,   537,   538,   539,   540,   541,   542,   543,
     544,   545,   546,   547,   548,   549,   550,   551,   552,   553,
     554,   555,   556,   557,   558,   559,   560,   561,   562,   563,
     564,   565,   566,   567,   568,   569,   570,   571,   572,   573,
     574,   575,   576,   577,   578,   579,   580,   581,   582,   583,
     584,   585,   586,   587,   588,   589,   590,   591,   592,   593,
     594,   595,   596,   597,   598,   599,   600,   601,   602,   603,
     604,   605,   606,   607,   608,   609,   610,   611,   612,   613,
     614,   615,   616,   617,   618,   619,   620,   621,   622,   623,
     624,   625,   626,   627,   627,   628,   628,   628,   628,   628,
     628,   628,   628,   629,   630,   631,   632,   633,   634,   635,
     636,   637,   638,   638,   639,   639,   639,   639,   639,   639,
     639,   639,   639,   639,   639,   639,   639,   639,   639,   639,
     639,   639,   639,   640,   641,   642,   643,   644,   645,   646,
     647,   648,   649,   650,   651,   652,   653,   654,   655,   656,
     657,   658,   659,   660,   660,   661,   662,   663,   664,   664,
     665,   666,   667,   668,   669,   670,   671,   672,   672,   673,
     673,   673,   673,   673,   673,   673,   673,   673,   673,   674,
     675,   676,   677,   678,   679,   680,   681,   682,   683,   684,
     685,   685,   686,   686,   686,   686,   686,   686,   687,   688,
     689,   690,   691,   692,   693,   694,   695,   695,   696,   696,
     697,   698
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     1,     1,     1,
       2,     0,     1,     1,     1,     1,     1,     1,     1,     1,
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
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     0,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     0,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     0,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     0,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     2,     2,     2,     2,     2,     1,     2,     0,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     2,     2,     2,     2,     2,     2,     2,     2,
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
       3,     3,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     3,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     3,     3,     4,     4,     4,
       3,     3,     4,     4,     3,     3,     2,     2,     2,     2,
       2,     2,     3,     3,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     3,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     3,     3,     3,     2,
       2,     2,     1,     2,     0,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     2,     2,     2,     2,     2,     2,
       2,     1,     2,     0,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     1,     2,     0,     1,     2,     1,     2,     0,
       1,     2,     2,     2,     3,     3,     1,     2,     0,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     1,
       2,     0,     1,     1,     1,     1,     1,     1,     2,     2,
       2,     2,     2,     2,     3,     1,     2,     0,     1,     1,
       2,     2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

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

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


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
# ifndef YY_LOCATION_PRINT
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yykind < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yykind], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
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
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
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
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
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






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
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
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

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
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
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
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

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

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
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
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
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
  case 18: /* force_toplevel: VAR_FORCE_TOPLEVEL  */
#line 209 "util/configparser.y"
        {
		OUTYY(("\nP(force-toplevel)\n"));
		cfg_parser->started_toplevel = 0;
	}
#line 2855 "util/configparser.c"
    break;

  case 19: /* serverstart: VAR_SERVER  */
#line 216 "util/configparser.y"
        {
		OUTYY(("\nP(server:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 2864 "util/configparser.c"
    break;

  case 256: /* stubstart: VAR_STUB_ZONE  */
#line 329 "util/configparser.y"
        {
		struct config_stub* s;
		OUTYY(("\nP(stub_zone:)\n"));
		cfg_parser->started_toplevel = 1;
		s = (struct config_stub*)calloc(1, sizeof(struct config_stub));
		if(s) {
			s->next = cfg_parser->cfg->stubs;
			cfg_parser->cfg->stubs = s;
		} else {
			yyerror("out of memory");
		}
	}
#line 2881 "util/configparser.c"
    break;

  case 267: /* forwardstart: VAR_FORWARD_ZONE  */
#line 348 "util/configparser.y"
        {
		struct config_stub* s;
		OUTYY(("\nP(forward_zone:)\n"));
		cfg_parser->started_toplevel = 1;
		s = (struct config_stub*)calloc(1, sizeof(struct config_stub));
		if(s) {
			s->next = cfg_parser->cfg->forwards;
			cfg_parser->cfg->forwards = s;
		} else {
			yyerror("out of memory");
		}
	}
#line 2898 "util/configparser.c"
    break;

  case 277: /* viewstart: VAR_VIEW  */
#line 367 "util/configparser.y"
        {
		struct config_view* s;
		OUTYY(("\nP(view:)\n"));
		cfg_parser->started_toplevel = 1;
		s = (struct config_view*)calloc(1, sizeof(struct config_view));
		if(s) {
			s->next = cfg_parser->cfg->views;
			if(s->next && !s->next->name)
				yyerror("view without name");
			cfg_parser->cfg->views = s;
		} else {
			yyerror("out of memory");
		}
	}
#line 2917 "util/configparser.c"
    break;

  case 287: /* authstart: VAR_AUTH_ZONE  */
#line 388 "util/configparser.y"
        {
		struct config_auth* s;
		OUTYY(("\nP(auth_zone:)\n"));
		cfg_parser->started_toplevel = 1;
		s = (struct config_auth*)calloc(1, sizeof(struct config_auth));
		if(s) {
			s->next = cfg_parser->cfg->auths;
			cfg_parser->cfg->auths = s;
			/* defaults for auth zone */
			s->for_downstream = 1;
			s->for_upstream = 1;
			s->fallback_enabled = 0;
			s->zonemd_check = 0;
			s->zonemd_reject_absence = 0;
			s->isrpz = 0;
		} else {
			yyerror("out of memory");
		}
	}
#line 2941 "util/configparser.c"
    break;

  case 300: /* rpz_tag: VAR_TAGS STRING_ARG  */
#line 416 "util/configparser.y"
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
#line 2962 "util/configparser.c"
    break;

  case 301: /* rpz_action_override: VAR_RPZ_ACTION_OVERRIDE STRING_ARG  */
#line 435 "util/configparser.y"
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
#line 2981 "util/configparser.c"
    break;

  case 302: /* rpz_cname_override: VAR_RPZ_CNAME_OVERRIDE STRING_ARG  */
#line 452 "util/configparser.y"
        {
		OUTYY(("P(rpz_cname_override:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->auths->rpz_cname);
		cfg_parser->cfg->auths->rpz_cname = (yyvsp[0].str);
	}
#line 2991 "util/configparser.c"
    break;

  case 303: /* rpz_log: VAR_RPZ_LOG STRING_ARG  */
#line 460 "util/configparser.y"
        {
		OUTYY(("P(rpz_log:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->rpz_log = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3003 "util/configparser.c"
    break;

  case 304: /* rpz_log_name: VAR_RPZ_LOG_NAME STRING_ARG  */
#line 470 "util/configparser.y"
        {
		OUTYY(("P(rpz_log_name:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->auths->rpz_log_name);
		cfg_parser->cfg->auths->rpz_log_name = (yyvsp[0].str);
	}
#line 3013 "util/configparser.c"
    break;

  case 305: /* rpz_signal_nxdomain_ra: VAR_RPZ_SIGNAL_NXDOMAIN_RA STRING_ARG  */
#line 477 "util/configparser.y"
        {
		OUTYY(("P(rpz_signal_nxdomain_ra:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->rpz_signal_nxdomain_ra = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3025 "util/configparser.c"
    break;

  case 306: /* rpzstart: VAR_RPZ  */
#line 487 "util/configparser.y"
        {
		struct config_auth* s;
		OUTYY(("\nP(rpz:)\n")); 
		cfg_parser->started_toplevel = 1;
		s = (struct config_auth*)calloc(1, sizeof(struct config_auth));
		if(s) {
			s->next = cfg_parser->cfg->auths;
			cfg_parser->cfg->auths = s;
			/* defaults for RPZ auth zone */
			s->for_downstream = 0;
			s->for_upstream = 0;
			s->fallback_enabled = 0;
			s->isrpz = 1;
		} else {
			yyerror("out of memory");
		}
	}
#line 3047 "util/configparser.c"
    break;

  case 321: /* server_num_threads: VAR_NUM_THREADS STRING_ARG  */
#line 512 "util/configparser.y"
        {
		OUTYY(("P(server_num_threads:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->num_threads = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3059 "util/configparser.c"
    break;

  case 322: /* server_verbosity: VAR_VERBOSITY STRING_ARG  */
#line 521 "util/configparser.y"
        {
		OUTYY(("P(server_verbosity:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->verbosity = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3071 "util/configparser.c"
    break;

  case 323: /* server_statistics_interval: VAR_STATISTICS_INTERVAL STRING_ARG  */
#line 530 "util/configparser.y"
        {
		OUTYY(("P(server_statistics_interval:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "") == 0 || strcmp((yyvsp[0].str), "0") == 0)
			cfg_parser->cfg->stat_interval = 0;
		else if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->stat_interval = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3085 "util/configparser.c"
    break;

  case 324: /* server_statistics_cumulative: VAR_STATISTICS_CUMULATIVE STRING_ARG  */
#line 541 "util/configparser.y"
        {
		OUTYY(("P(server_statistics_cumulative:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_cumulative = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3097 "util/configparser.c"
    break;

  case 325: /* server_extended_statistics: VAR_EXTENDED_STATISTICS STRING_ARG  */
#line 550 "util/configparser.y"
        {
		OUTYY(("P(server_extended_statistics:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_extended = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3109 "util/configparser.c"
    break;

  case 326: /* server_statistics_inhibit_zero: VAR_STATISTICS_INHIBIT_ZERO STRING_ARG  */
#line 559 "util/configparser.y"
        {
		OUTYY(("P(server_statistics_inhibit_zero:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_inhibit_zero = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3121 "util/configparser.c"
    break;

  case 327: /* server_shm_enable: VAR_SHM_ENABLE STRING_ARG  */
#line 568 "util/configparser.y"
        {
		OUTYY(("P(server_shm_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->shm_enable = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3133 "util/configparser.c"
    break;

  case 328: /* server_shm_key: VAR_SHM_KEY STRING_ARG  */
#line 577 "util/configparser.y"
        {
		OUTYY(("P(server_shm_key:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "") == 0 || strcmp((yyvsp[0].str), "0") == 0)
			cfg_parser->cfg->shm_key = 0;
		else if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->shm_key = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3147 "util/configparser.c"
    break;

  case 329: /* server_port: VAR_PORT STRING_ARG  */
#line 588 "util/configparser.y"
        {
		OUTYY(("P(server_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3159 "util/configparser.c"
    break;

  case 330: /* server_send_client_subnet: VAR_SEND_CLIENT_SUBNET STRING_ARG  */
#line 597 "util/configparser.y"
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
#line 3174 "util/configparser.c"
    break;

  case 331: /* server_client_subnet_zone: VAR_CLIENT_SUBNET_ZONE STRING_ARG  */
#line 609 "util/configparser.y"
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
#line 3190 "util/configparser.c"
    break;

  case 332: /* server_client_subnet_always_forward: VAR_CLIENT_SUBNET_ALWAYS_FORWARD STRING_ARG  */
#line 623 "util/configparser.y"
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
#line 3208 "util/configparser.c"
    break;

  case 333: /* server_client_subnet_opcode: VAR_CLIENT_SUBNET_OPCODE STRING_ARG  */
#line 638 "util/configparser.y"
        {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(client_subnet_opcode:%s)\n", (yyvsp[0].str)));
		OUTYY(("P(Deprecated option, ignoring)\n"));
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 3222 "util/configparser.c"
    break;

  case 334: /* server_max_client_subnet_ipv4: VAR_MAX_CLIENT_SUBNET_IPV4 STRING_ARG  */
#line 649 "util/configparser.y"
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
#line 3242 "util/configparser.c"
    break;

  case 335: /* server_max_client_subnet_ipv6: VAR_MAX_CLIENT_SUBNET_IPV6 STRING_ARG  */
#line 666 "util/configparser.y"
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
#line 3262 "util/configparser.c"
    break;

  case 336: /* server_min_client_subnet_ipv4: VAR_MIN_CLIENT_SUBNET_IPV4 STRING_ARG  */
#line 683 "util/configparser.y"
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
#line 3282 "util/configparser.c"
    break;

  case 337: /* server_min_client_subnet_ipv6: VAR_MIN_CLIENT_SUBNET_IPV6 STRING_ARG  */
#line 700 "util/configparser.y"
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
#line 3302 "util/configparser.c"
    break;

  case 338: /* server_max_ecs_tree_size_ipv4: VAR_MAX_ECS_TREE_SIZE_IPV4 STRING_ARG  */
#line 717 "util/configparser.y"
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
#line 3320 "util/configparser.c"
    break;

  case 339: /* server_max_ecs_tree_size_ipv6: VAR_MAX_ECS_TREE_SIZE_IPV6 STRING_ARG  */
#line 732 "util/configparser.y"
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
#line 3338 "util/configparser.c"
    break;

  case 340: /* server_interface: VAR_INTERFACE STRING_ARG  */
#line 747 "util/configparser.y"
        {
		OUTYY(("P(server_interface:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->num_ifs == 0)
			cfg_parser->cfg->ifs = calloc(1, sizeof(char*));
		else cfg_parser->cfg->ifs = realloc(cfg_parser->cfg->ifs,
				(cfg_parser->cfg->num_ifs+1)*sizeof(char*));
		if(!cfg_parser->cfg->ifs)
			yyerror("out of memory");
		else
			cfg_parser->cfg->ifs[cfg_parser->cfg->num_ifs++] = (yyvsp[0].str);
	}
#line 3354 "util/configparser.c"
    break;

  case 341: /* server_outgoing_interface: VAR_OUTGOING_INTERFACE STRING_ARG  */
#line 760 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_interface:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->num_out_ifs == 0)
			cfg_parser->cfg->out_ifs = calloc(1, sizeof(char*));
		else cfg_parser->cfg->out_ifs = realloc(
			cfg_parser->cfg->out_ifs,
			(cfg_parser->cfg->num_out_ifs+1)*sizeof(char*));
		if(!cfg_parser->cfg->out_ifs)
			yyerror("out of memory");
		else
			cfg_parser->cfg->out_ifs[
				cfg_parser->cfg->num_out_ifs++] = (yyvsp[0].str);
	}
#line 3372 "util/configparser.c"
    break;

  case 342: /* server_outgoing_range: VAR_OUTGOING_RANGE STRING_ARG  */
#line 775 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_range:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_num_ports = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3384 "util/configparser.c"
    break;

  case 343: /* server_outgoing_port_permit: VAR_OUTGOING_PORT_PERMIT STRING_ARG  */
#line 784 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_port_permit:%s)\n", (yyvsp[0].str)));
		if(!cfg_mark_ports((yyvsp[0].str), 1,
			cfg_parser->cfg->outgoing_avail_ports, 65536))
			yyerror("port number or range (\"low-high\") expected");
		free((yyvsp[0].str));
	}
#line 3396 "util/configparser.c"
    break;

  case 344: /* server_outgoing_port_avoid: VAR_OUTGOING_PORT_AVOID STRING_ARG  */
#line 793 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_port_avoid:%s)\n", (yyvsp[0].str)));
		if(!cfg_mark_ports((yyvsp[0].str), 0,
			cfg_parser->cfg->outgoing_avail_ports, 65536))
			yyerror("port number or range (\"low-high\") expected");
		free((yyvsp[0].str));
	}
#line 3408 "util/configparser.c"
    break;

  case 345: /* server_outgoing_num_tcp: VAR_OUTGOING_NUM_TCP STRING_ARG  */
#line 802 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_num_tcp:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_num_tcp = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3420 "util/configparser.c"
    break;

  case 346: /* server_incoming_num_tcp: VAR_INCOMING_NUM_TCP STRING_ARG  */
#line 811 "util/configparser.y"
        {
		OUTYY(("P(server_incoming_num_tcp:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->incoming_num_tcp = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3432 "util/configparser.c"
    break;

  case 347: /* server_interface_automatic: VAR_INTERFACE_AUTOMATIC STRING_ARG  */
#line 820 "util/configparser.y"
        {
		OUTYY(("P(server_interface_automatic:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->if_automatic = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3444 "util/configparser.c"
    break;

  case 348: /* server_interface_automatic_ports: VAR_INTERFACE_AUTOMATIC_PORTS STRING_ARG  */
#line 829 "util/configparser.y"
        {
		OUTYY(("P(server_interface_automatic_ports:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->if_automatic_ports);
		cfg_parser->cfg->if_automatic_ports = (yyvsp[0].str);
	}
#line 3454 "util/configparser.c"
    break;

  case 349: /* server_do_ip4: VAR_DO_IP4 STRING_ARG  */
#line 836 "util/configparser.y"
        {
		OUTYY(("P(server_do_ip4:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_ip4 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3466 "util/configparser.c"
    break;

  case 350: /* server_do_ip6: VAR_DO_IP6 STRING_ARG  */
#line 845 "util/configparser.y"
        {
		OUTYY(("P(server_do_ip6:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_ip6 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3478 "util/configparser.c"
    break;

  case 351: /* server_do_udp: VAR_DO_UDP STRING_ARG  */
#line 854 "util/configparser.y"
        {
		OUTYY(("P(server_do_udp:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_udp = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3490 "util/configparser.c"
    break;

  case 352: /* server_do_tcp: VAR_DO_TCP STRING_ARG  */
#line 863 "util/configparser.y"
        {
		OUTYY(("P(server_do_tcp:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_tcp = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3502 "util/configparser.c"
    break;

  case 353: /* server_prefer_ip4: VAR_PREFER_IP4 STRING_ARG  */
#line 872 "util/configparser.y"
        {
		OUTYY(("P(server_prefer_ip4:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefer_ip4 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3514 "util/configparser.c"
    break;

  case 354: /* server_prefer_ip6: VAR_PREFER_IP6 STRING_ARG  */
#line 881 "util/configparser.y"
        {
		OUTYY(("P(server_prefer_ip6:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefer_ip6 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3526 "util/configparser.c"
    break;

  case 355: /* server_tcp_mss: VAR_TCP_MSS STRING_ARG  */
#line 890 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_mss:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
				yyerror("number expected");
		else cfg_parser->cfg->tcp_mss = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3538 "util/configparser.c"
    break;

  case 356: /* server_outgoing_tcp_mss: VAR_OUTGOING_TCP_MSS STRING_ARG  */
#line 899 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_tcp_mss:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_tcp_mss = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3550 "util/configparser.c"
    break;

  case 357: /* server_tcp_idle_timeout: VAR_TCP_IDLE_TIMEOUT STRING_ARG  */
#line 908 "util/configparser.y"
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
#line 3566 "util/configparser.c"
    break;

  case 358: /* server_max_reuse_tcp_queries: VAR_MAX_REUSE_TCP_QUERIES STRING_ARG  */
#line 921 "util/configparser.y"
        {
		OUTYY(("P(server_max_reuse_tcp_queries:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 1)
			cfg_parser->cfg->max_reuse_tcp_queries = 0;
		else cfg_parser->cfg->max_reuse_tcp_queries = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3580 "util/configparser.c"
    break;

  case 359: /* server_tcp_reuse_timeout: VAR_TCP_REUSE_TIMEOUT STRING_ARG  */
#line 932 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_reuse_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 1)
			cfg_parser->cfg->tcp_reuse_timeout = 0;
		else cfg_parser->cfg->tcp_reuse_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3594 "util/configparser.c"
    break;

  case 360: /* server_tcp_auth_query_timeout: VAR_TCP_AUTH_QUERY_TIMEOUT STRING_ARG  */
#line 943 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_auth_query_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 1)
			cfg_parser->cfg->tcp_auth_query_timeout = 0;
		else cfg_parser->cfg->tcp_auth_query_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3608 "util/configparser.c"
    break;

  case 361: /* server_tcp_keepalive: VAR_EDNS_TCP_KEEPALIVE STRING_ARG  */
#line 954 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_keepalive:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_tcp_keepalive = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3620 "util/configparser.c"
    break;

  case 362: /* server_tcp_keepalive_timeout: VAR_EDNS_TCP_KEEPALIVE_TIMEOUT STRING_ARG  */
#line 963 "util/configparser.y"
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
#line 3636 "util/configparser.c"
    break;

  case 363: /* server_tcp_upstream: VAR_TCP_UPSTREAM STRING_ARG  */
#line 976 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tcp_upstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3648 "util/configparser.c"
    break;

  case 364: /* server_udp_upstream_without_downstream: VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM STRING_ARG  */
#line 985 "util/configparser.y"
        {
		OUTYY(("P(server_udp_upstream_without_downstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->udp_upstream_without_downstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3660 "util/configparser.c"
    break;

  case 365: /* server_ssl_upstream: VAR_SSL_UPSTREAM STRING_ARG  */
#line 994 "util/configparser.y"
        {
		OUTYY(("P(server_ssl_upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ssl_upstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3672 "util/configparser.c"
    break;

  case 366: /* server_ssl_service_key: VAR_SSL_SERVICE_KEY STRING_ARG  */
#line 1003 "util/configparser.y"
        {
		OUTYY(("P(server_ssl_service_key:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->ssl_service_key);
		cfg_parser->cfg->ssl_service_key = (yyvsp[0].str);
	}
#line 3682 "util/configparser.c"
    break;

  case 367: /* server_ssl_service_pem: VAR_SSL_SERVICE_PEM STRING_ARG  */
#line 1010 "util/configparser.y"
        {
		OUTYY(("P(server_ssl_service_pem:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->ssl_service_pem);
		cfg_parser->cfg->ssl_service_pem = (yyvsp[0].str);
	}
#line 3692 "util/configparser.c"
    break;

  case 368: /* server_ssl_port: VAR_SSL_PORT STRING_ARG  */
#line 1017 "util/configparser.y"
        {
		OUTYY(("P(server_ssl_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->ssl_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3704 "util/configparser.c"
    break;

  case 369: /* server_tls_cert_bundle: VAR_TLS_CERT_BUNDLE STRING_ARG  */
#line 1026 "util/configparser.y"
        {
		OUTYY(("P(server_tls_cert_bundle:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->tls_cert_bundle);
		cfg_parser->cfg->tls_cert_bundle = (yyvsp[0].str);
	}
#line 3714 "util/configparser.c"
    break;

  case 370: /* server_tls_win_cert: VAR_TLS_WIN_CERT STRING_ARG  */
#line 1033 "util/configparser.y"
        {
		OUTYY(("P(server_tls_win_cert:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tls_win_cert = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3726 "util/configparser.c"
    break;

  case 371: /* server_tls_additional_port: VAR_TLS_ADDITIONAL_PORT STRING_ARG  */
#line 1042 "util/configparser.y"
        {
		OUTYY(("P(server_tls_additional_port:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->tls_additional_port,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3737 "util/configparser.c"
    break;

  case 372: /* server_tls_ciphers: VAR_TLS_CIPHERS STRING_ARG  */
#line 1050 "util/configparser.y"
        {
		OUTYY(("P(server_tls_ciphers:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->tls_ciphers);
		cfg_parser->cfg->tls_ciphers = (yyvsp[0].str);
	}
#line 3747 "util/configparser.c"
    break;

  case 373: /* server_tls_ciphersuites: VAR_TLS_CIPHERSUITES STRING_ARG  */
#line 1057 "util/configparser.y"
        {
		OUTYY(("P(server_tls_ciphersuites:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->tls_ciphersuites);
		cfg_parser->cfg->tls_ciphersuites = (yyvsp[0].str);
	}
#line 3757 "util/configparser.c"
    break;

  case 374: /* server_tls_session_ticket_keys: VAR_TLS_SESSION_TICKET_KEYS STRING_ARG  */
#line 1064 "util/configparser.y"
        {
		OUTYY(("P(server_tls_session_ticket_keys:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append(&cfg_parser->cfg->tls_session_ticket_keys,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3768 "util/configparser.c"
    break;

  case 375: /* server_tls_use_sni: VAR_TLS_USE_SNI STRING_ARG  */
#line 1072 "util/configparser.y"
        {
		OUTYY(("P(server_tls_use_sni:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tls_use_sni = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3780 "util/configparser.c"
    break;

  case 376: /* server_https_port: VAR_HTTPS_PORT STRING_ARG  */
#line 1081 "util/configparser.y"
        {
		OUTYY(("P(server_https_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->https_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3792 "util/configparser.c"
    break;

  case 377: /* server_http_endpoint: VAR_HTTP_ENDPOINT STRING_ARG  */
#line 1089 "util/configparser.y"
        {
		OUTYY(("P(server_http_endpoint:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->http_endpoint);
		if((yyvsp[0].str) && (yyvsp[0].str)[0] != '/') {
			cfg_parser->cfg->http_endpoint = malloc(strlen((yyvsp[0].str))+2);
			if(!cfg_parser->cfg->http_endpoint)
				yyerror("out of memory");
			cfg_parser->cfg->http_endpoint[0] = '/';
			memmove(cfg_parser->cfg->http_endpoint+1, (yyvsp[0].str),
				strlen((yyvsp[0].str))+1);
			free((yyvsp[0].str));
		} else {
			cfg_parser->cfg->http_endpoint = (yyvsp[0].str);
		}
	}
#line 3812 "util/configparser.c"
    break;

  case 378: /* server_http_max_streams: VAR_HTTP_MAX_STREAMS STRING_ARG  */
#line 1105 "util/configparser.y"
        {
		OUTYY(("P(server_http_max_streams:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->http_max_streams = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3824 "util/configparser.c"
    break;

  case 379: /* server_http_query_buffer_size: VAR_HTTP_QUERY_BUFFER_SIZE STRING_ARG  */
#line 1113 "util/configparser.y"
        {
		OUTYY(("P(server_http_query_buffer_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str),
			&cfg_parser->cfg->http_query_buffer_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 3836 "util/configparser.c"
    break;

  case 380: /* server_http_response_buffer_size: VAR_HTTP_RESPONSE_BUFFER_SIZE STRING_ARG  */
#line 1121 "util/configparser.y"
        {
		OUTYY(("P(server_http_response_buffer_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str),
			&cfg_parser->cfg->http_response_buffer_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 3848 "util/configparser.c"
    break;

  case 381: /* server_http_nodelay: VAR_HTTP_NODELAY STRING_ARG  */
#line 1129 "util/configparser.y"
        {
		OUTYY(("P(server_http_nodelay:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->http_nodelay = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3860 "util/configparser.c"
    break;

  case 382: /* server_http_notls_downstream: VAR_HTTP_NOTLS_DOWNSTREAM STRING_ARG  */
#line 1137 "util/configparser.y"
        {
		OUTYY(("P(server_http_notls_downstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->http_notls_downstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3872 "util/configparser.c"
    break;

  case 383: /* server_use_systemd: VAR_USE_SYSTEMD STRING_ARG  */
#line 1145 "util/configparser.y"
        {
		OUTYY(("P(server_use_systemd:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->use_systemd = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3884 "util/configparser.c"
    break;

  case 384: /* server_do_daemonize: VAR_DO_DAEMONIZE STRING_ARG  */
#line 1154 "util/configparser.y"
        {
		OUTYY(("P(server_do_daemonize:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_daemonize = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3896 "util/configparser.c"
    break;

  case 385: /* server_use_syslog: VAR_USE_SYSLOG STRING_ARG  */
#line 1163 "util/configparser.y"
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
#line 3913 "util/configparser.c"
    break;

  case 386: /* server_log_time_ascii: VAR_LOG_TIME_ASCII STRING_ARG  */
#line 1177 "util/configparser.y"
        {
		OUTYY(("P(server_log_time_ascii:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_time_ascii = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3925 "util/configparser.c"
    break;

  case 387: /* server_log_queries: VAR_LOG_QUERIES STRING_ARG  */
#line 1186 "util/configparser.y"
        {
		OUTYY(("P(server_log_queries:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_queries = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3937 "util/configparser.c"
    break;

  case 388: /* server_log_replies: VAR_LOG_REPLIES STRING_ARG  */
#line 1195 "util/configparser.y"
        {
		OUTYY(("P(server_log_replies:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_replies = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3949 "util/configparser.c"
    break;

  case 389: /* server_log_tag_queryreply: VAR_LOG_TAG_QUERYREPLY STRING_ARG  */
#line 1204 "util/configparser.y"
        {
		OUTYY(("P(server_log_tag_queryreply:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_tag_queryreply = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3961 "util/configparser.c"
    break;

  case 390: /* server_log_servfail: VAR_LOG_SERVFAIL STRING_ARG  */
#line 1213 "util/configparser.y"
        {
		OUTYY(("P(server_log_servfail:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_servfail = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3973 "util/configparser.c"
    break;

  case 391: /* server_log_local_actions: VAR_LOG_LOCAL_ACTIONS STRING_ARG  */
#line 1222 "util/configparser.y"
        {
		OUTYY(("P(server_log_local_actions:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_local_actions = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3985 "util/configparser.c"
    break;

  case 392: /* server_chroot: VAR_CHROOT STRING_ARG  */
#line 1231 "util/configparser.y"
        {
		OUTYY(("P(server_chroot:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->chrootdir);
		cfg_parser->cfg->chrootdir = (yyvsp[0].str);
	}
#line 3995 "util/configparser.c"
    break;

  case 393: /* server_username: VAR_USERNAME STRING_ARG  */
#line 1238 "util/configparser.y"
        {
		OUTYY(("P(server_username:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->username);
		cfg_parser->cfg->username = (yyvsp[0].str);
	}
#line 4005 "util/configparser.c"
    break;

  case 394: /* server_directory: VAR_DIRECTORY STRING_ARG  */
#line 1245 "util/configparser.y"
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
#line 4034 "util/configparser.c"
    break;

  case 395: /* server_logfile: VAR_LOGFILE STRING_ARG  */
#line 1271 "util/configparser.y"
        {
		OUTYY(("P(server_logfile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->logfile);
		cfg_parser->cfg->logfile = (yyvsp[0].str);
		cfg_parser->cfg->use_syslog = 0;
	}
#line 4045 "util/configparser.c"
    break;

  case 396: /* server_pidfile: VAR_PIDFILE STRING_ARG  */
#line 1279 "util/configparser.y"
        {
		OUTYY(("P(server_pidfile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->pidfile);
		cfg_parser->cfg->pidfile = (yyvsp[0].str);
	}
#line 4055 "util/configparser.c"
    break;

  case 397: /* server_root_hints: VAR_ROOT_HINTS STRING_ARG  */
#line 1286 "util/configparser.y"
        {
		OUTYY(("P(server_root_hints:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->root_hints, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4065 "util/configparser.c"
    break;

  case 398: /* server_dlv_anchor_file: VAR_DLV_ANCHOR_FILE STRING_ARG  */
#line 1293 "util/configparser.y"
        {
		OUTYY(("P(server_dlv_anchor_file:%s)\n", (yyvsp[0].str)));
		log_warn("option dlv-anchor-file ignored: DLV is decommissioned");
		free((yyvsp[0].str));
	}
#line 4075 "util/configparser.c"
    break;

  case 399: /* server_dlv_anchor: VAR_DLV_ANCHOR STRING_ARG  */
#line 1300 "util/configparser.y"
        {
		OUTYY(("P(server_dlv_anchor:%s)\n", (yyvsp[0].str)));
		log_warn("option dlv-anchor ignored: DLV is decommissioned");
		free((yyvsp[0].str));
	}
#line 4085 "util/configparser.c"
    break;

  case 400: /* server_auto_trust_anchor_file: VAR_AUTO_TRUST_ANCHOR_FILE STRING_ARG  */
#line 1307 "util/configparser.y"
        {
		OUTYY(("P(server_auto_trust_anchor_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			auto_trust_anchor_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4096 "util/configparser.c"
    break;

  case 401: /* server_trust_anchor_file: VAR_TRUST_ANCHOR_FILE STRING_ARG  */
#line 1315 "util/configparser.y"
        {
		OUTYY(("P(server_trust_anchor_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			trust_anchor_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4107 "util/configparser.c"
    break;

  case 402: /* server_trusted_keys_file: VAR_TRUSTED_KEYS_FILE STRING_ARG  */
#line 1323 "util/configparser.y"
        {
		OUTYY(("P(server_trusted_keys_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			trusted_keys_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4118 "util/configparser.c"
    break;

  case 403: /* server_trust_anchor: VAR_TRUST_ANCHOR STRING_ARG  */
#line 1331 "util/configparser.y"
        {
		OUTYY(("P(server_trust_anchor:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->trust_anchor_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4128 "util/configparser.c"
    break;

  case 404: /* server_trust_anchor_signaling: VAR_TRUST_ANCHOR_SIGNALING STRING_ARG  */
#line 1338 "util/configparser.y"
        {
		OUTYY(("P(server_trust_anchor_signaling:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->trust_anchor_signaling =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4142 "util/configparser.c"
    break;

  case 405: /* server_root_key_sentinel: VAR_ROOT_KEY_SENTINEL STRING_ARG  */
#line 1349 "util/configparser.y"
        {
		OUTYY(("P(server_root_key_sentinel:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->root_key_sentinel =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4156 "util/configparser.c"
    break;

  case 406: /* server_domain_insecure: VAR_DOMAIN_INSECURE STRING_ARG  */
#line 1360 "util/configparser.y"
        {
		OUTYY(("P(server_domain_insecure:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->domain_insecure, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4166 "util/configparser.c"
    break;

  case 407: /* server_hide_identity: VAR_HIDE_IDENTITY STRING_ARG  */
#line 1367 "util/configparser.y"
        {
		OUTYY(("P(server_hide_identity:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_identity = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4178 "util/configparser.c"
    break;

  case 408: /* server_hide_version: VAR_HIDE_VERSION STRING_ARG  */
#line 1376 "util/configparser.y"
        {
		OUTYY(("P(server_hide_version:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_version = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4190 "util/configparser.c"
    break;

  case 409: /* server_hide_trustanchor: VAR_HIDE_TRUSTANCHOR STRING_ARG  */
#line 1385 "util/configparser.y"
        {
		OUTYY(("P(server_hide_trustanchor:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_trustanchor = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4202 "util/configparser.c"
    break;

  case 410: /* server_hide_http_user_agent: VAR_HIDE_HTTP_USER_AGENT STRING_ARG  */
#line 1394 "util/configparser.y"
        {
		OUTYY(("P(server_hide_user_agent:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_http_user_agent = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4214 "util/configparser.c"
    break;

  case 411: /* server_identity: VAR_IDENTITY STRING_ARG  */
#line 1403 "util/configparser.y"
        {
		OUTYY(("P(server_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->identity);
		cfg_parser->cfg->identity = (yyvsp[0].str);
	}
#line 4224 "util/configparser.c"
    break;

  case 412: /* server_version: VAR_VERSION STRING_ARG  */
#line 1410 "util/configparser.y"
        {
		OUTYY(("P(server_version:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->version);
		cfg_parser->cfg->version = (yyvsp[0].str);
	}
#line 4234 "util/configparser.c"
    break;

  case 413: /* server_http_user_agent: VAR_HTTP_USER_AGENT STRING_ARG  */
#line 1417 "util/configparser.y"
        {
		OUTYY(("P(server_http_user_agent:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->http_user_agent);
		cfg_parser->cfg->http_user_agent = (yyvsp[0].str);
	}
#line 4244 "util/configparser.c"
    break;

  case 414: /* server_nsid: VAR_NSID STRING_ARG  */
#line 1424 "util/configparser.y"
        {
		OUTYY(("P(server_nsid:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->nsid_cfg_str);
		cfg_parser->cfg->nsid_cfg_str = (yyvsp[0].str);
		free(cfg_parser->cfg->nsid);
		cfg_parser->cfg->nsid = NULL;
		cfg_parser->cfg->nsid_len = 0;
		if (*(yyvsp[0].str) == 0)
			; /* pass; empty string is not setting nsid */
		else if (!(cfg_parser->cfg->nsid = cfg_parse_nsid(
					(yyvsp[0].str), &cfg_parser->cfg->nsid_len)))
			yyerror("the NSID must be either a hex string or an "
			    "ascii character string prepended with ascii_.");
	}
#line 4263 "util/configparser.c"
    break;

  case 415: /* server_so_rcvbuf: VAR_SO_RCVBUF STRING_ARG  */
#line 1440 "util/configparser.y"
        {
		OUTYY(("P(server_so_rcvbuf:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->so_rcvbuf))
			yyerror("buffer size expected");
		free((yyvsp[0].str));
	}
#line 4274 "util/configparser.c"
    break;

  case 416: /* server_so_sndbuf: VAR_SO_SNDBUF STRING_ARG  */
#line 1448 "util/configparser.y"
        {
		OUTYY(("P(server_so_sndbuf:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->so_sndbuf))
			yyerror("buffer size expected");
		free((yyvsp[0].str));
	}
#line 4285 "util/configparser.c"
    break;

  case 417: /* server_so_reuseport: VAR_SO_REUSEPORT STRING_ARG  */
#line 1456 "util/configparser.y"
        {
		OUTYY(("P(server_so_reuseport:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->so_reuseport =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4298 "util/configparser.c"
    break;

  case 418: /* server_ip_transparent: VAR_IP_TRANSPARENT STRING_ARG  */
#line 1466 "util/configparser.y"
        {
		OUTYY(("P(server_ip_transparent:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ip_transparent =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4311 "util/configparser.c"
    break;

  case 419: /* server_ip_freebind: VAR_IP_FREEBIND STRING_ARG  */
#line 1476 "util/configparser.y"
        {
		OUTYY(("P(server_ip_freebind:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ip_freebind =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4324 "util/configparser.c"
    break;

  case 420: /* server_ip_dscp: VAR_IP_DSCP STRING_ARG  */
#line 1486 "util/configparser.y"
        {
		OUTYY(("P(server_ip_dscp:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) > 63)
			yyerror("value too large (max 63)");
		else if (atoi((yyvsp[0].str)) < 0)
			yyerror("value too small (min 0)");
		else
			cfg_parser->cfg->ip_dscp = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4341 "util/configparser.c"
    break;

  case 421: /* server_stream_wait_size: VAR_STREAM_WAIT_SIZE STRING_ARG  */
#line 1500 "util/configparser.y"
        {
		OUTYY(("P(server_stream_wait_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->stream_wait_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4352 "util/configparser.c"
    break;

  case 422: /* server_edns_buffer_size: VAR_EDNS_BUFFER_SIZE STRING_ARG  */
#line 1508 "util/configparser.y"
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
#line 4368 "util/configparser.c"
    break;

  case 423: /* server_msg_buffer_size: VAR_MSG_BUFFER_SIZE STRING_ARG  */
#line 1521 "util/configparser.y"
        {
		OUTYY(("P(server_msg_buffer_size:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 4096)
			yyerror("message buffer size too small (use 4096)");
		else cfg_parser->cfg->msg_buffer_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4382 "util/configparser.c"
    break;

  case 424: /* server_msg_cache_size: VAR_MSG_CACHE_SIZE STRING_ARG  */
#line 1532 "util/configparser.y"
        {
		OUTYY(("P(server_msg_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->msg_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4393 "util/configparser.c"
    break;

  case 425: /* server_msg_cache_slabs: VAR_MSG_CACHE_SLABS STRING_ARG  */
#line 1540 "util/configparser.y"
        {
		OUTYY(("P(server_msg_cache_slabs:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0) {
			yyerror("number expected");
		} else {
			cfg_parser->cfg->msg_cache_slabs = atoi((yyvsp[0].str));
			if(!is_pow2(cfg_parser->cfg->msg_cache_slabs))
				yyerror("must be a power of 2");
		}
		free((yyvsp[0].str));
	}
#line 4409 "util/configparser.c"
    break;

  case 426: /* server_num_queries_per_thread: VAR_NUM_QUERIES_PER_THREAD STRING_ARG  */
#line 1553 "util/configparser.y"
        {
		OUTYY(("P(server_num_queries_per_thread:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->num_queries_per_thread = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4421 "util/configparser.c"
    break;

  case 427: /* server_jostle_timeout: VAR_JOSTLE_TIMEOUT STRING_ARG  */
#line 1562 "util/configparser.y"
        {
		OUTYY(("P(server_jostle_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->jostle_time = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4433 "util/configparser.c"
    break;

  case 428: /* server_delay_close: VAR_DELAY_CLOSE STRING_ARG  */
#line 1571 "util/configparser.y"
        {
		OUTYY(("P(server_delay_close:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->delay_close = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4445 "util/configparser.c"
    break;

  case 429: /* server_udp_connect: VAR_UDP_CONNECT STRING_ARG  */
#line 1580 "util/configparser.y"
        {
		OUTYY(("P(server_udp_connect:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->udp_connect = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4457 "util/configparser.c"
    break;

  case 430: /* server_unblock_lan_zones: VAR_UNBLOCK_LAN_ZONES STRING_ARG  */
#line 1589 "util/configparser.y"
        {
		OUTYY(("P(server_unblock_lan_zones:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->unblock_lan_zones =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4470 "util/configparser.c"
    break;

  case 431: /* server_insecure_lan_zones: VAR_INSECURE_LAN_ZONES STRING_ARG  */
#line 1599 "util/configparser.y"
        {
		OUTYY(("P(server_insecure_lan_zones:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->insecure_lan_zones =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4483 "util/configparser.c"
    break;

  case 432: /* server_rrset_cache_size: VAR_RRSET_CACHE_SIZE STRING_ARG  */
#line 1609 "util/configparser.y"
        {
		OUTYY(("P(server_rrset_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->rrset_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4494 "util/configparser.c"
    break;

  case 433: /* server_rrset_cache_slabs: VAR_RRSET_CACHE_SLABS STRING_ARG  */
#line 1617 "util/configparser.y"
        {
		OUTYY(("P(server_rrset_cache_slabs:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0) {
			yyerror("number expected");
		} else {
			cfg_parser->cfg->rrset_cache_slabs = atoi((yyvsp[0].str));
			if(!is_pow2(cfg_parser->cfg->rrset_cache_slabs))
				yyerror("must be a power of 2");
		}
		free((yyvsp[0].str));
	}
#line 4510 "util/configparser.c"
    break;

  case 434: /* server_infra_host_ttl: VAR_INFRA_HOST_TTL STRING_ARG  */
#line 1630 "util/configparser.y"
        {
		OUTYY(("P(server_infra_host_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->host_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4522 "util/configparser.c"
    break;

  case 435: /* server_infra_lame_ttl: VAR_INFRA_LAME_TTL STRING_ARG  */
#line 1639 "util/configparser.y"
        {
		OUTYY(("P(server_infra_lame_ttl:%s)\n", (yyvsp[0].str)));
		verbose(VERB_DETAIL, "ignored infra-lame-ttl: %s (option "
			"removed, use infra-host-ttl)", (yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4533 "util/configparser.c"
    break;

  case 436: /* server_infra_cache_numhosts: VAR_INFRA_CACHE_NUMHOSTS STRING_ARG  */
#line 1647 "util/configparser.y"
        {
		OUTYY(("P(server_infra_cache_numhosts:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_numhosts = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4545 "util/configparser.c"
    break;

  case 437: /* server_infra_cache_lame_size: VAR_INFRA_CACHE_LAME_SIZE STRING_ARG  */
#line 1656 "util/configparser.y"
        {
		OUTYY(("P(server_infra_cache_lame_size:%s)\n", (yyvsp[0].str)));
		verbose(VERB_DETAIL, "ignored infra-cache-lame-size: %s "
			"(option removed, use infra-cache-numhosts)", (yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4556 "util/configparser.c"
    break;

  case 438: /* server_infra_cache_slabs: VAR_INFRA_CACHE_SLABS STRING_ARG  */
#line 1664 "util/configparser.y"
        {
		OUTYY(("P(server_infra_cache_slabs:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0) {
			yyerror("number expected");
		} else {
			cfg_parser->cfg->infra_cache_slabs = atoi((yyvsp[0].str));
			if(!is_pow2(cfg_parser->cfg->infra_cache_slabs))
				yyerror("must be a power of 2");
		}
		free((yyvsp[0].str));
	}
#line 4572 "util/configparser.c"
    break;

  case 439: /* server_infra_cache_min_rtt: VAR_INFRA_CACHE_MIN_RTT STRING_ARG  */
#line 1677 "util/configparser.y"
        {
		OUTYY(("P(server_infra_cache_min_rtt:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_min_rtt = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4584 "util/configparser.c"
    break;

  case 440: /* server_infra_cache_max_rtt: VAR_INFRA_CACHE_MAX_RTT STRING_ARG  */
#line 1686 "util/configparser.y"
        {
		OUTYY(("P(server_infra_cache_max_rtt:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_max_rtt = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4596 "util/configparser.c"
    break;

  case 441: /* server_infra_keep_probing: VAR_INFRA_KEEP_PROBING STRING_ARG  */
#line 1695 "util/configparser.y"
        {
		OUTYY(("P(server_infra_keep_probing:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->infra_keep_probing =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4609 "util/configparser.c"
    break;

  case 442: /* server_target_fetch_policy: VAR_TARGET_FETCH_POLICY STRING_ARG  */
#line 1705 "util/configparser.y"
        {
		OUTYY(("P(server_target_fetch_policy:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->target_fetch_policy);
		cfg_parser->cfg->target_fetch_policy = (yyvsp[0].str);
	}
#line 4619 "util/configparser.c"
    break;

  case 443: /* server_harden_short_bufsize: VAR_HARDEN_SHORT_BUFSIZE STRING_ARG  */
#line 1712 "util/configparser.y"
        {
		OUTYY(("P(server_harden_short_bufsize:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_short_bufsize =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4632 "util/configparser.c"
    break;

  case 444: /* server_harden_large_queries: VAR_HARDEN_LARGE_QUERIES STRING_ARG  */
#line 1722 "util/configparser.y"
        {
		OUTYY(("P(server_harden_large_queries:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_large_queries =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4645 "util/configparser.c"
    break;

  case 445: /* server_harden_glue: VAR_HARDEN_GLUE STRING_ARG  */
#line 1732 "util/configparser.y"
        {
		OUTYY(("P(server_harden_glue:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_glue =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4658 "util/configparser.c"
    break;

  case 446: /* server_harden_dnssec_stripped: VAR_HARDEN_DNSSEC_STRIPPED STRING_ARG  */
#line 1742 "util/configparser.y"
        {
		OUTYY(("P(server_harden_dnssec_stripped:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_dnssec_stripped =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4671 "util/configparser.c"
    break;

  case 447: /* server_harden_below_nxdomain: VAR_HARDEN_BELOW_NXDOMAIN STRING_ARG  */
#line 1752 "util/configparser.y"
        {
		OUTYY(("P(server_harden_below_nxdomain:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_below_nxdomain =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4684 "util/configparser.c"
    break;

  case 448: /* server_harden_referral_path: VAR_HARDEN_REFERRAL_PATH STRING_ARG  */
#line 1762 "util/configparser.y"
        {
		OUTYY(("P(server_harden_referral_path:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_referral_path =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4697 "util/configparser.c"
    break;

  case 449: /* server_harden_algo_downgrade: VAR_HARDEN_ALGO_DOWNGRADE STRING_ARG  */
#line 1772 "util/configparser.y"
        {
		OUTYY(("P(server_harden_algo_downgrade:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_algo_downgrade =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4710 "util/configparser.c"
    break;

  case 450: /* server_use_caps_for_id: VAR_USE_CAPS_FOR_ID STRING_ARG  */
#line 1782 "util/configparser.y"
        {
		OUTYY(("P(server_use_caps_for_id:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->use_caps_bits_for_id =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4723 "util/configparser.c"
    break;

  case 451: /* server_caps_whitelist: VAR_CAPS_WHITELIST STRING_ARG  */
#line 1792 "util/configparser.y"
        {
		OUTYY(("P(server_caps_whitelist:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->caps_whitelist, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4733 "util/configparser.c"
    break;

  case 452: /* server_private_address: VAR_PRIVATE_ADDRESS STRING_ARG  */
#line 1799 "util/configparser.y"
        {
		OUTYY(("P(server_private_address:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->private_address, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4743 "util/configparser.c"
    break;

  case 453: /* server_private_domain: VAR_PRIVATE_DOMAIN STRING_ARG  */
#line 1806 "util/configparser.y"
        {
		OUTYY(("P(server_private_domain:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->private_domain, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4753 "util/configparser.c"
    break;

  case 454: /* server_prefetch: VAR_PREFETCH STRING_ARG  */
#line 1813 "util/configparser.y"
        {
		OUTYY(("P(server_prefetch:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefetch = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4765 "util/configparser.c"
    break;

  case 455: /* server_prefetch_key: VAR_PREFETCH_KEY STRING_ARG  */
#line 1822 "util/configparser.y"
        {
		OUTYY(("P(server_prefetch_key:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefetch_key = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4777 "util/configparser.c"
    break;

  case 456: /* server_deny_any: VAR_DENY_ANY STRING_ARG  */
#line 1831 "util/configparser.y"
        {
		OUTYY(("P(server_deny_any:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->deny_any = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4789 "util/configparser.c"
    break;

  case 457: /* server_unwanted_reply_threshold: VAR_UNWANTED_REPLY_THRESHOLD STRING_ARG  */
#line 1840 "util/configparser.y"
        {
		OUTYY(("P(server_unwanted_reply_threshold:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->unwanted_threshold = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4801 "util/configparser.c"
    break;

  case 458: /* server_do_not_query_address: VAR_DO_NOT_QUERY_ADDRESS STRING_ARG  */
#line 1849 "util/configparser.y"
        {
		OUTYY(("P(server_do_not_query_address:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->donotqueryaddrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4811 "util/configparser.c"
    break;

  case 459: /* server_do_not_query_localhost: VAR_DO_NOT_QUERY_LOCALHOST STRING_ARG  */
#line 1856 "util/configparser.y"
        {
		OUTYY(("P(server_do_not_query_localhost:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->donotquery_localhost =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4824 "util/configparser.c"
    break;

  case 460: /* server_access_control: VAR_ACCESS_CONTROL STRING_ARG STRING_ARG  */
#line 1866 "util/configparser.y"
        {
		OUTYY(("P(server_access_control:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_acl_action((yyvsp[0].str));
		if(!cfg_str2list_insert(&cfg_parser->cfg->acls, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding acl");
	}
#line 4835 "util/configparser.c"
    break;

  case 461: /* server_interface_action: VAR_INTERFACE_ACTION STRING_ARG STRING_ARG  */
#line 1874 "util/configparser.y"
        {
		OUTYY(("P(server_interface_action:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_acl_action((yyvsp[0].str));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->interface_actions, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding acl");
	}
#line 4847 "util/configparser.c"
    break;

  case 462: /* server_module_conf: VAR_MODULE_CONF STRING_ARG  */
#line 1883 "util/configparser.y"
        {
		OUTYY(("P(server_module_conf:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->module_conf);
		cfg_parser->cfg->module_conf = (yyvsp[0].str);
	}
#line 4857 "util/configparser.c"
    break;

  case 463: /* server_val_override_date: VAR_VAL_OVERRIDE_DATE STRING_ARG  */
#line 1890 "util/configparser.y"
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
#line 4878 "util/configparser.c"
    break;

  case 464: /* server_val_sig_skew_min: VAR_VAL_SIG_SKEW_MIN STRING_ARG  */
#line 1908 "util/configparser.y"
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
#line 4894 "util/configparser.c"
    break;

  case 465: /* server_val_sig_skew_max: VAR_VAL_SIG_SKEW_MAX STRING_ARG  */
#line 1921 "util/configparser.y"
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
#line 4910 "util/configparser.c"
    break;

  case 466: /* server_val_max_restart: VAR_VAL_MAX_RESTART STRING_ARG  */
#line 1934 "util/configparser.y"
        {
		OUTYY(("P(server_val_max_restart:%s)\n", (yyvsp[0].str)));
		if(*(yyvsp[0].str) == '\0' || strcmp((yyvsp[0].str), "0") == 0) {
			cfg_parser->cfg->val_max_restart = 0;
		} else {
			cfg_parser->cfg->val_max_restart = atoi((yyvsp[0].str));
			if(!cfg_parser->cfg->val_max_restart)
				yyerror("number expected");
		}
		free((yyvsp[0].str));
	}
#line 4926 "util/configparser.c"
    break;

  case 467: /* server_cache_max_ttl: VAR_CACHE_MAX_TTL STRING_ARG  */
#line 1947 "util/configparser.y"
        {
		OUTYY(("P(server_cache_max_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4938 "util/configparser.c"
    break;

  case 468: /* server_cache_max_negative_ttl: VAR_CACHE_MAX_NEGATIVE_TTL STRING_ARG  */
#line 1956 "util/configparser.y"
        {
		OUTYY(("P(server_cache_max_negative_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_negative_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4950 "util/configparser.c"
    break;

  case 469: /* server_cache_min_ttl: VAR_CACHE_MIN_TTL STRING_ARG  */
#line 1965 "util/configparser.y"
        {
		OUTYY(("P(server_cache_min_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->min_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4962 "util/configparser.c"
    break;

  case 470: /* server_bogus_ttl: VAR_BOGUS_TTL STRING_ARG  */
#line 1974 "util/configparser.y"
        {
		OUTYY(("P(server_bogus_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->bogus_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4974 "util/configparser.c"
    break;

  case 471: /* server_val_clean_additional: VAR_VAL_CLEAN_ADDITIONAL STRING_ARG  */
#line 1983 "util/configparser.y"
        {
		OUTYY(("P(server_val_clean_additional:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->val_clean_additional =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4987 "util/configparser.c"
    break;

  case 472: /* server_val_permissive_mode: VAR_VAL_PERMISSIVE_MODE STRING_ARG  */
#line 1993 "util/configparser.y"
        {
		OUTYY(("P(server_val_permissive_mode:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->val_permissive_mode =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5000 "util/configparser.c"
    break;

  case 473: /* server_aggressive_nsec: VAR_AGGRESSIVE_NSEC STRING_ARG  */
#line 2003 "util/configparser.y"
        {
		OUTYY(("P(server_aggressive_nsec:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->aggressive_nsec =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5014 "util/configparser.c"
    break;

  case 474: /* server_ignore_cd_flag: VAR_IGNORE_CD_FLAG STRING_ARG  */
#line 2014 "util/configparser.y"
        {
		OUTYY(("P(server_ignore_cd_flag:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ignore_cd = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5026 "util/configparser.c"
    break;

  case 475: /* server_serve_expired: VAR_SERVE_EXPIRED STRING_ARG  */
#line 2023 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->serve_expired = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5038 "util/configparser.c"
    break;

  case 476: /* server_serve_expired_ttl: VAR_SERVE_EXPIRED_TTL STRING_ARG  */
#line 2032 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->serve_expired_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5050 "util/configparser.c"
    break;

  case 477: /* server_serve_expired_ttl_reset: VAR_SERVE_EXPIRED_TTL_RESET STRING_ARG  */
#line 2041 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired_ttl_reset:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->serve_expired_ttl_reset = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5062 "util/configparser.c"
    break;

  case 478: /* server_serve_expired_reply_ttl: VAR_SERVE_EXPIRED_REPLY_TTL STRING_ARG  */
#line 2050 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired_reply_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->serve_expired_reply_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5074 "util/configparser.c"
    break;

  case 479: /* server_serve_expired_client_timeout: VAR_SERVE_EXPIRED_CLIENT_TIMEOUT STRING_ARG  */
#line 2059 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired_client_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->serve_expired_client_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5086 "util/configparser.c"
    break;

  case 480: /* server_ede_serve_expired: VAR_EDE_SERVE_EXPIRED STRING_ARG  */
#line 2068 "util/configparser.y"
        {
		OUTYY(("P(server_ede_serve_expired:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ede_serve_expired = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5098 "util/configparser.c"
    break;

  case 481: /* server_serve_original_ttl: VAR_SERVE_ORIGINAL_TTL STRING_ARG  */
#line 2077 "util/configparser.y"
        {
		OUTYY(("P(server_serve_original_ttl:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->serve_original_ttl = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5110 "util/configparser.c"
    break;

  case 482: /* server_fake_dsa: VAR_FAKE_DSA STRING_ARG  */
#line 2086 "util/configparser.y"
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
#line 5126 "util/configparser.c"
    break;

  case 483: /* server_fake_sha1: VAR_FAKE_SHA1 STRING_ARG  */
#line 2099 "util/configparser.y"
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
#line 5142 "util/configparser.c"
    break;

  case 484: /* server_val_log_level: VAR_VAL_LOG_LEVEL STRING_ARG  */
#line 2112 "util/configparser.y"
        {
		OUTYY(("P(server_val_log_level:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->val_log_level = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5154 "util/configparser.c"
    break;

  case 485: /* server_val_nsec3_keysize_iterations: VAR_VAL_NSEC3_KEYSIZE_ITERATIONS STRING_ARG  */
#line 2121 "util/configparser.y"
        {
		OUTYY(("P(server_val_nsec3_keysize_iterations:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->val_nsec3_key_iterations);
		cfg_parser->cfg->val_nsec3_key_iterations = (yyvsp[0].str);
	}
#line 5164 "util/configparser.c"
    break;

  case 486: /* server_zonemd_permissive_mode: VAR_ZONEMD_PERMISSIVE_MODE STRING_ARG  */
#line 2128 "util/configparser.y"
        {
		OUTYY(("P(server_zonemd_permissive_mode:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else	cfg_parser->cfg->zonemd_permissive_mode = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5176 "util/configparser.c"
    break;

  case 487: /* server_add_holddown: VAR_ADD_HOLDDOWN STRING_ARG  */
#line 2137 "util/configparser.y"
        {
		OUTYY(("P(server_add_holddown:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->add_holddown = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5188 "util/configparser.c"
    break;

  case 488: /* server_del_holddown: VAR_DEL_HOLDDOWN STRING_ARG  */
#line 2146 "util/configparser.y"
        {
		OUTYY(("P(server_del_holddown:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->del_holddown = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5200 "util/configparser.c"
    break;

  case 489: /* server_keep_missing: VAR_KEEP_MISSING STRING_ARG  */
#line 2155 "util/configparser.y"
        {
		OUTYY(("P(server_keep_missing:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->keep_missing = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5212 "util/configparser.c"
    break;

  case 490: /* server_permit_small_holddown: VAR_PERMIT_SMALL_HOLDDOWN STRING_ARG  */
#line 2164 "util/configparser.y"
        {
		OUTYY(("P(server_permit_small_holddown:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->permit_small_holddown =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5225 "util/configparser.c"
    break;

  case 491: /* server_key_cache_size: VAR_KEY_CACHE_SIZE STRING_ARG  */
#line 2173 "util/configparser.y"
        {
		OUTYY(("P(server_key_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->key_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 5236 "util/configparser.c"
    break;

  case 492: /* server_key_cache_slabs: VAR_KEY_CACHE_SLABS STRING_ARG  */
#line 2181 "util/configparser.y"
        {
		OUTYY(("P(server_key_cache_slabs:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0) {
			yyerror("number expected");
		} else {
			cfg_parser->cfg->key_cache_slabs = atoi((yyvsp[0].str));
			if(!is_pow2(cfg_parser->cfg->key_cache_slabs))
				yyerror("must be a power of 2");
		}
		free((yyvsp[0].str));
	}
#line 5252 "util/configparser.c"
    break;

  case 493: /* server_neg_cache_size: VAR_NEG_CACHE_SIZE STRING_ARG  */
#line 2194 "util/configparser.y"
        {
		OUTYY(("P(server_neg_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->neg_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 5263 "util/configparser.c"
    break;

  case 494: /* server_local_zone: VAR_LOCAL_ZONE STRING_ARG STRING_ARG  */
#line 2202 "util/configparser.y"
        {
		OUTYY(("P(server_local_zone:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "static")!=0 && strcmp((yyvsp[0].str), "deny")!=0 &&
		   strcmp((yyvsp[0].str), "refuse")!=0 && strcmp((yyvsp[0].str), "redirect")!=0 &&
		   strcmp((yyvsp[0].str), "transparent")!=0 && strcmp((yyvsp[0].str), "nodefault")!=0
		   && strcmp((yyvsp[0].str), "typetransparent")!=0
		   && strcmp((yyvsp[0].str), "always_transparent")!=0
		   && strcmp((yyvsp[0].str), "always_refuse")!=0
		   && strcmp((yyvsp[0].str), "always_nxdomain")!=0
		   && strcmp((yyvsp[0].str), "always_nodata")!=0
		   && strcmp((yyvsp[0].str), "always_deny")!=0
		   && strcmp((yyvsp[0].str), "always_null")!=0
		   && strcmp((yyvsp[0].str), "noview")!=0
		   && strcmp((yyvsp[0].str), "inform")!=0 && strcmp((yyvsp[0].str), "inform_deny")!=0
		   && strcmp((yyvsp[0].str), "inform_redirect") != 0
		   && strcmp((yyvsp[0].str), "ipset") != 0) {
			yyerror("local-zone type: expected static, deny, "
				"refuse, redirect, transparent, "
				"typetransparent, inform, inform_deny, "
				"inform_redirect, always_transparent, "
				"always_refuse, always_nxdomain, "
				"always_nodata, always_deny, always_null, "
				"noview, nodefault or ipset");
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		} else if(strcmp((yyvsp[0].str), "nodefault")==0) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->
				local_zones_nodefault, (yyvsp[-1].str)))
				fatal_exit("out of memory adding local-zone");
			free((yyvsp[0].str));
#ifdef USE_IPSET
		} else if(strcmp((yyvsp[0].str), "ipset")==0) {
			size_t len = strlen((yyvsp[-1].str));
			/* Make sure to add the trailing dot.
			 * These are str compared to domain names. */
			if((yyvsp[-1].str)[len-1] != '.') {
				if(!((yyvsp[-1].str) = realloc((yyvsp[-1].str), len+2))) {
					fatal_exit("out of memory adding local-zone");
				}
				(yyvsp[-1].str)[len] = '.';
				(yyvsp[-1].str)[len+1] = 0;
			}
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
#line 5321 "util/configparser.c"
    break;

  case 495: /* server_local_data: VAR_LOCAL_DATA STRING_ARG  */
#line 2257 "util/configparser.y"
        {
		OUTYY(("P(server_local_data:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->local_data, (yyvsp[0].str)))
			fatal_exit("out of memory adding local-data");
	}
#line 5331 "util/configparser.c"
    break;

  case 496: /* server_local_data_ptr: VAR_LOCAL_DATA_PTR STRING_ARG  */
#line 2264 "util/configparser.y"
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
#line 5349 "util/configparser.c"
    break;

  case 497: /* server_minimal_responses: VAR_MINIMAL_RESPONSES STRING_ARG  */
#line 2279 "util/configparser.y"
        {
		OUTYY(("P(server_minimal_responses:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->minimal_responses =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5362 "util/configparser.c"
    break;

  case 498: /* server_rrset_roundrobin: VAR_RRSET_ROUNDROBIN STRING_ARG  */
#line 2289 "util/configparser.y"
        {
		OUTYY(("P(server_rrset_roundrobin:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->rrset_roundrobin =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5375 "util/configparser.c"
    break;

  case 499: /* server_unknown_server_time_limit: VAR_UNKNOWN_SERVER_TIME_LIMIT STRING_ARG  */
#line 2299 "util/configparser.y"
        {
		OUTYY(("P(server_unknown_server_time_limit:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->unknown_server_time_limit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5385 "util/configparser.c"
    break;

  case 500: /* server_max_udp_size: VAR_MAX_UDP_SIZE STRING_ARG  */
#line 2306 "util/configparser.y"
        {
		OUTYY(("P(server_max_udp_size:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->max_udp_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5395 "util/configparser.c"
    break;

  case 501: /* server_dns64_prefix: VAR_DNS64_PREFIX STRING_ARG  */
#line 2313 "util/configparser.y"
        {
		OUTYY(("P(dns64_prefix:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dns64_prefix);
		cfg_parser->cfg->dns64_prefix = (yyvsp[0].str);
	}
#line 5405 "util/configparser.c"
    break;

  case 502: /* server_dns64_synthall: VAR_DNS64_SYNTHALL STRING_ARG  */
#line 2320 "util/configparser.y"
        {
		OUTYY(("P(server_dns64_synthall:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dns64_synthall = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5417 "util/configparser.c"
    break;

  case 503: /* server_dns64_ignore_aaaa: VAR_DNS64_IGNORE_AAAA STRING_ARG  */
#line 2329 "util/configparser.y"
        {
		OUTYY(("P(dns64_ignore_aaaa:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dns64_ignore_aaaa,
			(yyvsp[0].str)))
			fatal_exit("out of memory adding dns64-ignore-aaaa");
	}
#line 5428 "util/configparser.c"
    break;

  case 504: /* server_define_tag: VAR_DEFINE_TAG STRING_ARG  */
#line 2337 "util/configparser.y"
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
#line 5445 "util/configparser.c"
    break;

  case 505: /* server_local_zone_tag: VAR_LOCAL_ZONE_TAG STRING_ARG STRING_ARG  */
#line 2351 "util/configparser.y"
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
#line 5469 "util/configparser.c"
    break;

  case 506: /* server_access_control_tag: VAR_ACCESS_CONTROL_TAG STRING_ARG STRING_ARG  */
#line 2372 "util/configparser.y"
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
#line 5493 "util/configparser.c"
    break;

  case 507: /* server_access_control_tag_action: VAR_ACCESS_CONTROL_TAG_ACTION STRING_ARG STRING_ARG STRING_ARG  */
#line 2393 "util/configparser.y"
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
#line 5508 "util/configparser.c"
    break;

  case 508: /* server_access_control_tag_data: VAR_ACCESS_CONTROL_TAG_DATA STRING_ARG STRING_ARG STRING_ARG  */
#line 2405 "util/configparser.y"
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
#line 5523 "util/configparser.c"
    break;

  case 509: /* server_local_zone_override: VAR_LOCAL_ZONE_OVERRIDE STRING_ARG STRING_ARG STRING_ARG  */
#line 2417 "util/configparser.y"
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
#line 5538 "util/configparser.c"
    break;

  case 510: /* server_access_control_view: VAR_ACCESS_CONTROL_VIEW STRING_ARG STRING_ARG  */
#line 2429 "util/configparser.y"
        {
		OUTYY(("P(server_access_control_view:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str2list_insert(&cfg_parser->cfg->acl_view,
			(yyvsp[-1].str), (yyvsp[0].str))) {
			yyerror("out of memory");
		}
	}
#line 5550 "util/configparser.c"
    break;

  case 511: /* server_interface_tag: VAR_INTERFACE_TAG STRING_ARG STRING_ARG  */
#line 2438 "util/configparser.y"
        {
		size_t len = 0;
		uint8_t* bitlist = config_parse_taglist(cfg_parser->cfg, (yyvsp[0].str),
			&len);
		free((yyvsp[0].str));
		OUTYY(("P(server_interface_tag:%s)\n", (yyvsp[-1].str)));
		if(!bitlist) {
			yyerror("could not parse tags, (define-tag them first)");
			free((yyvsp[-1].str));
		}
		if(bitlist) {
			if(!cfg_strbytelist_insert(
				&cfg_parser->cfg->interface_tags,
				(yyvsp[-1].str), bitlist, len)) {
				yyerror("out of memory");
				free((yyvsp[-1].str));
			}
		}
	}
#line 5574 "util/configparser.c"
    break;

  case 512: /* server_interface_tag_action: VAR_INTERFACE_TAG_ACTION STRING_ARG STRING_ARG STRING_ARG  */
#line 2459 "util/configparser.y"
        {
		OUTYY(("P(server_interface_tag_action:%s %s %s)\n", (yyvsp[-2].str), (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str3list_insert(&cfg_parser->cfg->interface_tag_actions,
			(yyvsp[-2].str), (yyvsp[-1].str), (yyvsp[0].str))) {
			yyerror("out of memory");
			free((yyvsp[-2].str));
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		}
	}
#line 5589 "util/configparser.c"
    break;

  case 513: /* server_interface_tag_data: VAR_INTERFACE_TAG_DATA STRING_ARG STRING_ARG STRING_ARG  */
#line 2471 "util/configparser.y"
        {
		OUTYY(("P(server_interface_tag_data:%s %s %s)\n", (yyvsp[-2].str), (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str3list_insert(&cfg_parser->cfg->interface_tag_datas,
			(yyvsp[-2].str), (yyvsp[-1].str), (yyvsp[0].str))) {
			yyerror("out of memory");
			free((yyvsp[-2].str));
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		}
	}
#line 5604 "util/configparser.c"
    break;

  case 514: /* server_interface_view: VAR_INTERFACE_VIEW STRING_ARG STRING_ARG  */
#line 2483 "util/configparser.y"
        {
		OUTYY(("P(server_interface_view:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str2list_insert(&cfg_parser->cfg->interface_view,
			(yyvsp[-1].str), (yyvsp[0].str))) {
			yyerror("out of memory");
		}
	}
#line 5616 "util/configparser.c"
    break;

  case 515: /* server_response_ip_tag: VAR_RESPONSE_IP_TAG STRING_ARG STRING_ARG  */
#line 2492 "util/configparser.y"
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
#line 5640 "util/configparser.c"
    break;

  case 516: /* server_ip_ratelimit: VAR_IP_RATELIMIT STRING_ARG  */
#line 2513 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ip_ratelimit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5652 "util/configparser.c"
    break;

  case 517: /* server_ratelimit: VAR_RATELIMIT STRING_ARG  */
#line 2522 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ratelimit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5664 "util/configparser.c"
    break;

  case 518: /* server_ip_ratelimit_size: VAR_IP_RATELIMIT_SIZE STRING_ARG  */
#line 2531 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->ip_ratelimit_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 5675 "util/configparser.c"
    break;

  case 519: /* server_ratelimit_size: VAR_RATELIMIT_SIZE STRING_ARG  */
#line 2539 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->ratelimit_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 5686 "util/configparser.c"
    break;

  case 520: /* server_ip_ratelimit_slabs: VAR_IP_RATELIMIT_SLABS STRING_ARG  */
#line 2547 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit_slabs:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0) {
			yyerror("number expected");
		} else {
			cfg_parser->cfg->ip_ratelimit_slabs = atoi((yyvsp[0].str));
			if(!is_pow2(cfg_parser->cfg->ip_ratelimit_slabs))
				yyerror("must be a power of 2");
		}
		free((yyvsp[0].str));
	}
#line 5702 "util/configparser.c"
    break;

  case 521: /* server_ratelimit_slabs: VAR_RATELIMIT_SLABS STRING_ARG  */
#line 2560 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit_slabs:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0) {
			yyerror("number expected");
		} else {
			cfg_parser->cfg->ratelimit_slabs = atoi((yyvsp[0].str));
			if(!is_pow2(cfg_parser->cfg->ratelimit_slabs))
				yyerror("must be a power of 2");
		}
		free((yyvsp[0].str));
	}
#line 5718 "util/configparser.c"
    break;

  case 522: /* server_ratelimit_for_domain: VAR_RATELIMIT_FOR_DOMAIN STRING_ARG STRING_ARG  */
#line 2573 "util/configparser.y"
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
#line 5736 "util/configparser.c"
    break;

  case 523: /* server_ratelimit_below_domain: VAR_RATELIMIT_BELOW_DOMAIN STRING_ARG STRING_ARG  */
#line 2588 "util/configparser.y"
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
#line 5754 "util/configparser.c"
    break;

  case 524: /* server_ip_ratelimit_factor: VAR_IP_RATELIMIT_FACTOR STRING_ARG  */
#line 2603 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit_factor:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ip_ratelimit_factor = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5766 "util/configparser.c"
    break;

  case 525: /* server_ratelimit_factor: VAR_RATELIMIT_FACTOR STRING_ARG  */
#line 2612 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit_factor:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ratelimit_factor = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5778 "util/configparser.c"
    break;

  case 526: /* server_ip_ratelimit_backoff: VAR_IP_RATELIMIT_BACKOFF STRING_ARG  */
#line 2621 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit_backoff:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ip_ratelimit_backoff =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5791 "util/configparser.c"
    break;

  case 527: /* server_ratelimit_backoff: VAR_RATELIMIT_BACKOFF STRING_ARG  */
#line 2631 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit_backoff:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ratelimit_backoff =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5804 "util/configparser.c"
    break;

  case 528: /* server_outbound_msg_retry: VAR_OUTBOUND_MSG_RETRY STRING_ARG  */
#line 2641 "util/configparser.y"
        {
		OUTYY(("P(server_outbound_msg_retry:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outbound_msg_retry = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5816 "util/configparser.c"
    break;

  case 529: /* server_max_sent_count: VAR_MAX_SENT_COUNT STRING_ARG  */
#line 2650 "util/configparser.y"
        {
		OUTYY(("P(server_max_sent_count:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_sent_count = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5828 "util/configparser.c"
    break;

  case 530: /* server_max_query_restarts: VAR_MAX_QUERY_RESTARTS STRING_ARG  */
#line 2659 "util/configparser.y"
        {
		OUTYY(("P(server_max_query_restarts:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_query_restarts = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5840 "util/configparser.c"
    break;

  case 531: /* server_low_rtt: VAR_LOW_RTT STRING_ARG  */
#line 2668 "util/configparser.y"
        {
		OUTYY(("P(low-rtt option is deprecated, use fast-server-num instead)\n"));
		free((yyvsp[0].str));
	}
#line 5849 "util/configparser.c"
    break;

  case 532: /* server_fast_server_num: VAR_FAST_SERVER_NUM STRING_ARG  */
#line 2674 "util/configparser.y"
        {
		OUTYY(("P(server_fast_server_num:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) <= 0)
			yyerror("number expected");
		else cfg_parser->cfg->fast_server_num = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5861 "util/configparser.c"
    break;

  case 533: /* server_fast_server_permil: VAR_FAST_SERVER_PERMIL STRING_ARG  */
#line 2683 "util/configparser.y"
        {
		OUTYY(("P(server_fast_server_permil:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->fast_server_permil = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5873 "util/configparser.c"
    break;

  case 534: /* server_qname_minimisation: VAR_QNAME_MINIMISATION STRING_ARG  */
#line 2692 "util/configparser.y"
        {
		OUTYY(("P(server_qname_minimisation:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->qname_minimisation =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5886 "util/configparser.c"
    break;

  case 535: /* server_qname_minimisation_strict: VAR_QNAME_MINIMISATION_STRICT STRING_ARG  */
#line 2702 "util/configparser.y"
        {
		OUTYY(("P(server_qname_minimisation_strict:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->qname_minimisation_strict =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5899 "util/configparser.c"
    break;

  case 536: /* server_pad_responses: VAR_PAD_RESPONSES STRING_ARG  */
#line 2712 "util/configparser.y"
        {
		OUTYY(("P(server_pad_responses:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->pad_responses = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5912 "util/configparser.c"
    break;

  case 537: /* server_pad_responses_block_size: VAR_PAD_RESPONSES_BLOCK_SIZE STRING_ARG  */
#line 2722 "util/configparser.y"
        {
		OUTYY(("P(server_pad_responses_block_size:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->pad_responses_block_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5924 "util/configparser.c"
    break;

  case 538: /* server_pad_queries: VAR_PAD_QUERIES STRING_ARG  */
#line 2731 "util/configparser.y"
        {
		OUTYY(("P(server_pad_queries:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->pad_queries = 
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5937 "util/configparser.c"
    break;

  case 539: /* server_pad_queries_block_size: VAR_PAD_QUERIES_BLOCK_SIZE STRING_ARG  */
#line 2741 "util/configparser.y"
        {
		OUTYY(("P(server_pad_queries_block_size:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->pad_queries_block_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5949 "util/configparser.c"
    break;

  case 540: /* server_ipsecmod_enabled: VAR_IPSECMOD_ENABLED STRING_ARG  */
#line 2750 "util/configparser.y"
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
#line 5965 "util/configparser.c"
    break;

  case 541: /* server_ipsecmod_ignore_bogus: VAR_IPSECMOD_IGNORE_BOGUS STRING_ARG  */
#line 2763 "util/configparser.y"
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
#line 5981 "util/configparser.c"
    break;

  case 542: /* server_ipsecmod_hook: VAR_IPSECMOD_HOOK STRING_ARG  */
#line 2776 "util/configparser.y"
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
#line 5996 "util/configparser.c"
    break;

  case 543: /* server_ipsecmod_max_ttl: VAR_IPSECMOD_MAX_TTL STRING_ARG  */
#line 2788 "util/configparser.y"
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
#line 6013 "util/configparser.c"
    break;

  case 544: /* server_ipsecmod_whitelist: VAR_IPSECMOD_WHITELIST STRING_ARG  */
#line 2802 "util/configparser.y"
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
#line 6028 "util/configparser.c"
    break;

  case 545: /* server_ipsecmod_strict: VAR_IPSECMOD_STRICT STRING_ARG  */
#line 2814 "util/configparser.y"
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
#line 6045 "util/configparser.c"
    break;

  case 546: /* server_edns_client_string: VAR_EDNS_CLIENT_STRING STRING_ARG STRING_ARG  */
#line 2828 "util/configparser.y"
        {
		OUTYY(("P(server_edns_client_string:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->edns_client_strings, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding "
				"edns-client-string");
	}
#line 6057 "util/configparser.c"
    break;

  case 547: /* server_edns_client_string_opcode: VAR_EDNS_CLIENT_STRING_OPCODE STRING_ARG  */
#line 2837 "util/configparser.y"
        {
		OUTYY(("P(edns_client_string_opcode:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("option code expected");
		else if(atoi((yyvsp[0].str)) > 65535 || atoi((yyvsp[0].str)) < 0)
			yyerror("option code must be in interval [0, 65535]");
		else cfg_parser->cfg->edns_client_string_opcode = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6071 "util/configparser.c"
    break;

  case 548: /* server_ede: VAR_EDE STRING_ARG  */
#line 2848 "util/configparser.y"
        {
		OUTYY(("P(server_ede:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ede = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6083 "util/configparser.c"
    break;

  case 549: /* server_proxy_protocol_port: VAR_PROXY_PROTOCOL_PORT STRING_ARG  */
#line 2857 "util/configparser.y"
        {
		OUTYY(("P(server_proxy_protocol_port:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->proxy_protocol_port, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6093 "util/configparser.c"
    break;

  case 550: /* stub_name: VAR_NAME STRING_ARG  */
#line 2864 "util/configparser.y"
        {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->stubs->name)
			yyerror("stub name override, there must be one name "
				"for one stub-zone");
		free(cfg_parser->cfg->stubs->name);
		cfg_parser->cfg->stubs->name = (yyvsp[0].str);
	}
#line 6106 "util/configparser.c"
    break;

  case 551: /* stub_host: VAR_STUB_HOST STRING_ARG  */
#line 2874 "util/configparser.y"
        {
		OUTYY(("P(stub-host:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->stubs->hosts, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6116 "util/configparser.c"
    break;

  case 552: /* stub_addr: VAR_STUB_ADDR STRING_ARG  */
#line 2881 "util/configparser.y"
        {
		OUTYY(("P(stub-addr:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->stubs->addrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6126 "util/configparser.c"
    break;

  case 553: /* stub_first: VAR_STUB_FIRST STRING_ARG  */
#line 2888 "util/configparser.y"
        {
		OUTYY(("P(stub-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6138 "util/configparser.c"
    break;

  case 554: /* stub_no_cache: VAR_STUB_NO_CACHE STRING_ARG  */
#line 2897 "util/configparser.y"
        {
		OUTYY(("P(stub-no-cache:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->no_cache=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6150 "util/configparser.c"
    break;

  case 555: /* stub_ssl_upstream: VAR_STUB_SSL_UPSTREAM STRING_ARG  */
#line 2906 "util/configparser.y"
        {
		OUTYY(("P(stub-ssl-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->ssl_upstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6163 "util/configparser.c"
    break;

  case 556: /* stub_tcp_upstream: VAR_STUB_TCP_UPSTREAM STRING_ARG  */
#line 2916 "util/configparser.y"
        {
                OUTYY(("P(stub-tcp-upstream:%s)\n", (yyvsp[0].str)));
                if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
                        yyerror("expected yes or no.");
                else cfg_parser->cfg->stubs->tcp_upstream =
                        (strcmp((yyvsp[0].str), "yes")==0);
                free((yyvsp[0].str));
        }
#line 6176 "util/configparser.c"
    break;

  case 557: /* stub_prime: VAR_STUB_PRIME STRING_ARG  */
#line 2926 "util/configparser.y"
        {
		OUTYY(("P(stub-prime:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->isprime =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6189 "util/configparser.c"
    break;

  case 558: /* forward_name: VAR_NAME STRING_ARG  */
#line 2936 "util/configparser.y"
        {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->forwards->name)
			yyerror("forward name override, there must be one "
				"name for one forward-zone");
		free(cfg_parser->cfg->forwards->name);
		cfg_parser->cfg->forwards->name = (yyvsp[0].str);
	}
#line 6202 "util/configparser.c"
    break;

  case 559: /* forward_host: VAR_FORWARD_HOST STRING_ARG  */
#line 2946 "util/configparser.y"
        {
		OUTYY(("P(forward-host:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->forwards->hosts, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6212 "util/configparser.c"
    break;

  case 560: /* forward_addr: VAR_FORWARD_ADDR STRING_ARG  */
#line 2953 "util/configparser.y"
        {
		OUTYY(("P(forward-addr:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->forwards->addrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6222 "util/configparser.c"
    break;

  case 561: /* forward_first: VAR_FORWARD_FIRST STRING_ARG  */
#line 2960 "util/configparser.y"
        {
		OUTYY(("P(forward-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6234 "util/configparser.c"
    break;

  case 562: /* forward_no_cache: VAR_FORWARD_NO_CACHE STRING_ARG  */
#line 2969 "util/configparser.y"
        {
		OUTYY(("P(forward-no-cache:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->no_cache=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6246 "util/configparser.c"
    break;

  case 563: /* forward_ssl_upstream: VAR_FORWARD_SSL_UPSTREAM STRING_ARG  */
#line 2978 "util/configparser.y"
        {
		OUTYY(("P(forward-ssl-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->ssl_upstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6259 "util/configparser.c"
    break;

  case 564: /* forward_tcp_upstream: VAR_FORWARD_TCP_UPSTREAM STRING_ARG  */
#line 2988 "util/configparser.y"
        {
                OUTYY(("P(forward-tcp-upstream:%s)\n", (yyvsp[0].str)));
                if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
                        yyerror("expected yes or no.");
                else cfg_parser->cfg->forwards->tcp_upstream =
                        (strcmp((yyvsp[0].str), "yes")==0);
                free((yyvsp[0].str));
        }
#line 6272 "util/configparser.c"
    break;

  case 565: /* auth_name: VAR_NAME STRING_ARG  */
#line 2998 "util/configparser.y"
        {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->auths->name)
			yyerror("auth name override, there must be one name "
				"for one auth-zone");
		free(cfg_parser->cfg->auths->name);
		cfg_parser->cfg->auths->name = (yyvsp[0].str);
	}
#line 6285 "util/configparser.c"
    break;

  case 566: /* auth_zonefile: VAR_ZONEFILE STRING_ARG  */
#line 3008 "util/configparser.y"
        {
		OUTYY(("P(zonefile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->auths->zonefile);
		cfg_parser->cfg->auths->zonefile = (yyvsp[0].str);
	}
#line 6295 "util/configparser.c"
    break;

  case 567: /* auth_master: VAR_MASTER STRING_ARG  */
#line 3015 "util/configparser.y"
        {
		OUTYY(("P(master:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->masters, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6305 "util/configparser.c"
    break;

  case 568: /* auth_url: VAR_URL STRING_ARG  */
#line 3022 "util/configparser.y"
        {
		OUTYY(("P(url:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->urls, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6315 "util/configparser.c"
    break;

  case 569: /* auth_allow_notify: VAR_ALLOW_NOTIFY STRING_ARG  */
#line 3029 "util/configparser.y"
        {
		OUTYY(("P(allow-notify:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->allow_notify,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6326 "util/configparser.c"
    break;

  case 570: /* auth_zonemd_check: VAR_ZONEMD_CHECK STRING_ARG  */
#line 3037 "util/configparser.y"
        {
		OUTYY(("P(zonemd-check:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->zonemd_check =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6339 "util/configparser.c"
    break;

  case 571: /* auth_zonemd_reject_absence: VAR_ZONEMD_REJECT_ABSENCE STRING_ARG  */
#line 3047 "util/configparser.y"
        {
		OUTYY(("P(zonemd-reject-absence:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->zonemd_reject_absence =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6352 "util/configparser.c"
    break;

  case 572: /* auth_for_downstream: VAR_FOR_DOWNSTREAM STRING_ARG  */
#line 3057 "util/configparser.y"
        {
		OUTYY(("P(for-downstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->for_downstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6365 "util/configparser.c"
    break;

  case 573: /* auth_for_upstream: VAR_FOR_UPSTREAM STRING_ARG  */
#line 3067 "util/configparser.y"
        {
		OUTYY(("P(for-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->for_upstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6378 "util/configparser.c"
    break;

  case 574: /* auth_fallback_enabled: VAR_FALLBACK_ENABLED STRING_ARG  */
#line 3077 "util/configparser.y"
        {
		OUTYY(("P(fallback-enabled:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->fallback_enabled =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6391 "util/configparser.c"
    break;

  case 575: /* view_name: VAR_NAME STRING_ARG  */
#line 3087 "util/configparser.y"
        {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->views->name)
			yyerror("view name override, there must be one "
				"name for one view");
		free(cfg_parser->cfg->views->name);
		cfg_parser->cfg->views->name = (yyvsp[0].str);
	}
#line 6404 "util/configparser.c"
    break;

  case 576: /* view_local_zone: VAR_LOCAL_ZONE STRING_ARG STRING_ARG  */
#line 3097 "util/configparser.y"
        {
		OUTYY(("P(view_local_zone:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "static")!=0 && strcmp((yyvsp[0].str), "deny")!=0 &&
		   strcmp((yyvsp[0].str), "refuse")!=0 && strcmp((yyvsp[0].str), "redirect")!=0 &&
		   strcmp((yyvsp[0].str), "transparent")!=0 && strcmp((yyvsp[0].str), "nodefault")!=0
		   && strcmp((yyvsp[0].str), "typetransparent")!=0
		   && strcmp((yyvsp[0].str), "always_transparent")!=0
		   && strcmp((yyvsp[0].str), "always_refuse")!=0
		   && strcmp((yyvsp[0].str), "always_nxdomain")!=0
		   && strcmp((yyvsp[0].str), "always_nodata")!=0
		   && strcmp((yyvsp[0].str), "always_deny")!=0
		   && strcmp((yyvsp[0].str), "always_null")!=0
		   && strcmp((yyvsp[0].str), "noview")!=0
		   && strcmp((yyvsp[0].str), "inform")!=0 && strcmp((yyvsp[0].str), "inform_deny")!=0
		   && strcmp((yyvsp[0].str), "inform_redirect") != 0
		   && strcmp((yyvsp[0].str), "ipset") != 0) {
			yyerror("local-zone type: expected static, deny, "
				"refuse, redirect, transparent, "
				"typetransparent, inform, inform_deny, "
				"inform_redirect, always_transparent, "
				"always_refuse, always_nxdomain, "
				"always_nodata, always_deny, always_null, "
				"noview, nodefault or ipset");
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		} else if(strcmp((yyvsp[0].str), "nodefault")==0) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->views->
				local_zones_nodefault, (yyvsp[-1].str)))
				fatal_exit("out of memory adding local-zone");
			free((yyvsp[0].str));
#ifdef USE_IPSET
		} else if(strcmp((yyvsp[0].str), "ipset")==0) {
			size_t len = strlen((yyvsp[-1].str));
			/* Make sure to add the trailing dot.
			 * These are str compared to domain names. */
			if((yyvsp[-1].str)[len-1] != '.') {
				if(!((yyvsp[-1].str) = realloc((yyvsp[-1].str), len+2))) {
					fatal_exit("out of memory adding local-zone");
				}
				(yyvsp[-1].str)[len] = '.';
				(yyvsp[-1].str)[len+1] = 0;
			}
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
#line 6463 "util/configparser.c"
    break;

  case 577: /* view_response_ip: VAR_RESPONSE_IP STRING_ARG STRING_ARG  */
#line 3153 "util/configparser.y"
        {
		OUTYY(("P(view_response_ip:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_respip_action((yyvsp[0].str));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->views->respip_actions, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding per-view "
				"response-ip action");
	}
#line 6476 "util/configparser.c"
    break;

  case 578: /* view_response_ip_data: VAR_RESPONSE_IP_DATA STRING_ARG STRING_ARG  */
#line 3163 "util/configparser.y"
        {
		OUTYY(("P(view_response_ip_data:%s)\n", (yyvsp[-1].str)));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->views->respip_data, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding response-ip-data");
	}
#line 6487 "util/configparser.c"
    break;

  case 579: /* view_local_data: VAR_LOCAL_DATA STRING_ARG  */
#line 3171 "util/configparser.y"
        {
		OUTYY(("P(view_local_data:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->views->local_data, (yyvsp[0].str))) {
			fatal_exit("out of memory adding local-data");
		}
	}
#line 6498 "util/configparser.c"
    break;

  case 580: /* view_local_data_ptr: VAR_LOCAL_DATA_PTR STRING_ARG  */
#line 3179 "util/configparser.y"
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
#line 6516 "util/configparser.c"
    break;

  case 581: /* view_first: VAR_VIEW_FIRST STRING_ARG  */
#line 3194 "util/configparser.y"
        {
		OUTYY(("P(view-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->views->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6528 "util/configparser.c"
    break;

  case 582: /* rcstart: VAR_REMOTE_CONTROL  */
#line 3203 "util/configparser.y"
        {
		OUTYY(("\nP(remote-control:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 6537 "util/configparser.c"
    break;

  case 593: /* rc_control_enable: VAR_CONTROL_ENABLE STRING_ARG  */
#line 3215 "util/configparser.y"
        {
		OUTYY(("P(control_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->remote_control_enable =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6550 "util/configparser.c"
    break;

  case 594: /* rc_control_port: VAR_CONTROL_PORT STRING_ARG  */
#line 3225 "util/configparser.y"
        {
		OUTYY(("P(control_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("control port number expected");
		else cfg_parser->cfg->control_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6562 "util/configparser.c"
    break;

  case 595: /* rc_control_interface: VAR_CONTROL_INTERFACE STRING_ARG  */
#line 3234 "util/configparser.y"
        {
		OUTYY(("P(control_interface:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append(&cfg_parser->cfg->control_ifs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6572 "util/configparser.c"
    break;

  case 596: /* rc_control_use_cert: VAR_CONTROL_USE_CERT STRING_ARG  */
#line 3241 "util/configparser.y"
        {
		OUTYY(("P(control_use_cert:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->control_use_cert = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6582 "util/configparser.c"
    break;

  case 597: /* rc_server_key_file: VAR_SERVER_KEY_FILE STRING_ARG  */
#line 3248 "util/configparser.y"
        {
		OUTYY(("P(rc_server_key_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->server_key_file);
		cfg_parser->cfg->server_key_file = (yyvsp[0].str);
	}
#line 6592 "util/configparser.c"
    break;

  case 598: /* rc_server_cert_file: VAR_SERVER_CERT_FILE STRING_ARG  */
#line 3255 "util/configparser.y"
        {
		OUTYY(("P(rc_server_cert_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->server_cert_file);
		cfg_parser->cfg->server_cert_file = (yyvsp[0].str);
	}
#line 6602 "util/configparser.c"
    break;

  case 599: /* rc_control_key_file: VAR_CONTROL_KEY_FILE STRING_ARG  */
#line 3262 "util/configparser.y"
        {
		OUTYY(("P(rc_control_key_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->control_key_file);
		cfg_parser->cfg->control_key_file = (yyvsp[0].str);
	}
#line 6612 "util/configparser.c"
    break;

  case 600: /* rc_control_cert_file: VAR_CONTROL_CERT_FILE STRING_ARG  */
#line 3269 "util/configparser.y"
        {
		OUTYY(("P(rc_control_cert_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->control_cert_file);
		cfg_parser->cfg->control_cert_file = (yyvsp[0].str);
	}
#line 6622 "util/configparser.c"
    break;

  case 601: /* dtstart: VAR_DNSTAP  */
#line 3276 "util/configparser.y"
        {
		OUTYY(("\nP(dnstap:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 6631 "util/configparser.c"
    break;

  case 623: /* dt_dnstap_enable: VAR_DNSTAP_ENABLE STRING_ARG  */
#line 3297 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6643 "util/configparser.c"
    break;

  case 624: /* dt_dnstap_bidirectional: VAR_DNSTAP_BIDIRECTIONAL STRING_ARG  */
#line 3306 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_bidirectional:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_bidirectional =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6656 "util/configparser.c"
    break;

  case 625: /* dt_dnstap_socket_path: VAR_DNSTAP_SOCKET_PATH STRING_ARG  */
#line 3316 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_socket_path:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_socket_path);
		cfg_parser->cfg->dnstap_socket_path = (yyvsp[0].str);
	}
#line 6666 "util/configparser.c"
    break;

  case 626: /* dt_dnstap_ip: VAR_DNSTAP_IP STRING_ARG  */
#line 3323 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_ip:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_ip);
		cfg_parser->cfg->dnstap_ip = (yyvsp[0].str);
	}
#line 6676 "util/configparser.c"
    break;

  case 627: /* dt_dnstap_tls: VAR_DNSTAP_TLS STRING_ARG  */
#line 3330 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_tls = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6688 "util/configparser.c"
    break;

  case 628: /* dt_dnstap_tls_server_name: VAR_DNSTAP_TLS_SERVER_NAME STRING_ARG  */
#line 3339 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls_server_name:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_tls_server_name);
		cfg_parser->cfg->dnstap_tls_server_name = (yyvsp[0].str);
	}
#line 6698 "util/configparser.c"
    break;

  case 629: /* dt_dnstap_tls_cert_bundle: VAR_DNSTAP_TLS_CERT_BUNDLE STRING_ARG  */
#line 3346 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls_cert_bundle:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_tls_cert_bundle);
		cfg_parser->cfg->dnstap_tls_cert_bundle = (yyvsp[0].str);
	}
#line 6708 "util/configparser.c"
    break;

  case 630: /* dt_dnstap_tls_client_key_file: VAR_DNSTAP_TLS_CLIENT_KEY_FILE STRING_ARG  */
#line 3353 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls_client_key_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_tls_client_key_file);
		cfg_parser->cfg->dnstap_tls_client_key_file = (yyvsp[0].str);
	}
#line 6718 "util/configparser.c"
    break;

  case 631: /* dt_dnstap_tls_client_cert_file: VAR_DNSTAP_TLS_CLIENT_CERT_FILE STRING_ARG  */
#line 3360 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls_client_cert_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_tls_client_cert_file);
		cfg_parser->cfg->dnstap_tls_client_cert_file = (yyvsp[0].str);
	}
#line 6728 "util/configparser.c"
    break;

  case 632: /* dt_dnstap_send_identity: VAR_DNSTAP_SEND_IDENTITY STRING_ARG  */
#line 3367 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_send_identity:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_send_identity = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6740 "util/configparser.c"
    break;

  case 633: /* dt_dnstap_send_version: VAR_DNSTAP_SEND_VERSION STRING_ARG  */
#line 3376 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_send_version:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_send_version = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6752 "util/configparser.c"
    break;

  case 634: /* dt_dnstap_identity: VAR_DNSTAP_IDENTITY STRING_ARG  */
#line 3385 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_identity);
		cfg_parser->cfg->dnstap_identity = (yyvsp[0].str);
	}
#line 6762 "util/configparser.c"
    break;

  case 635: /* dt_dnstap_version: VAR_DNSTAP_VERSION STRING_ARG  */
#line 3392 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_version:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_version);
		cfg_parser->cfg->dnstap_version = (yyvsp[0].str);
	}
#line 6772 "util/configparser.c"
    break;

  case 636: /* dt_dnstap_log_resolver_query_messages: VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES STRING_ARG  */
#line 3399 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_resolver_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_resolver_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6785 "util/configparser.c"
    break;

  case 637: /* dt_dnstap_log_resolver_response_messages: VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES STRING_ARG  */
#line 3409 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_resolver_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_resolver_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6798 "util/configparser.c"
    break;

  case 638: /* dt_dnstap_log_client_query_messages: VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES STRING_ARG  */
#line 3419 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_client_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_client_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6811 "util/configparser.c"
    break;

  case 639: /* dt_dnstap_log_client_response_messages: VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES STRING_ARG  */
#line 3429 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_client_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_client_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6824 "util/configparser.c"
    break;

  case 640: /* dt_dnstap_log_forwarder_query_messages: VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES STRING_ARG  */
#line 3439 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_forwarder_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_forwarder_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6837 "util/configparser.c"
    break;

  case 641: /* dt_dnstap_log_forwarder_response_messages: VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES STRING_ARG  */
#line 3449 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_forwarder_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_forwarder_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6850 "util/configparser.c"
    break;

  case 642: /* pythonstart: VAR_PYTHON  */
#line 3459 "util/configparser.y"
        {
		OUTYY(("\nP(python:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 6859 "util/configparser.c"
    break;

  case 646: /* py_script: VAR_PYTHON_SCRIPT STRING_ARG  */
#line 3469 "util/configparser.y"
        {
		OUTYY(("P(python-script:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append_ex(&cfg_parser->cfg->python_script, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6869 "util/configparser.c"
    break;

  case 647: /* dynlibstart: VAR_DYNLIB  */
#line 3475 "util/configparser.y"
        { 
		OUTYY(("\nP(dynlib:)\n")); 
		cfg_parser->started_toplevel = 1;
	}
#line 6878 "util/configparser.c"
    break;

  case 651: /* dl_file: VAR_DYNLIB_FILE STRING_ARG  */
#line 3485 "util/configparser.y"
        {
		OUTYY(("P(dynlib-file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append_ex(&cfg_parser->cfg->dynlib_file, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6888 "util/configparser.c"
    break;

  case 652: /* server_disable_dnssec_lame_check: VAR_DISABLE_DNSSEC_LAME_CHECK STRING_ARG  */
#line 3491 "util/configparser.y"
        {
		OUTYY(("P(disable_dnssec_lame_check:%s)\n", (yyvsp[0].str)));
		if (strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->disable_dnssec_lame_check =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6901 "util/configparser.c"
    break;

  case 653: /* server_log_identity: VAR_LOG_IDENTITY STRING_ARG  */
#line 3501 "util/configparser.y"
        {
		OUTYY(("P(server_log_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->log_identity);
		cfg_parser->cfg->log_identity = (yyvsp[0].str);
	}
#line 6911 "util/configparser.c"
    break;

  case 654: /* server_response_ip: VAR_RESPONSE_IP STRING_ARG STRING_ARG  */
#line 3508 "util/configparser.y"
        {
		OUTYY(("P(server_response_ip:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_respip_action((yyvsp[0].str));
		if(!cfg_str2list_insert(&cfg_parser->cfg->respip_actions,
			(yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding response-ip");
	}
#line 6923 "util/configparser.c"
    break;

  case 655: /* server_response_ip_data: VAR_RESPONSE_IP_DATA STRING_ARG STRING_ARG  */
#line 3517 "util/configparser.y"
        {
		OUTYY(("P(server_response_ip_data:%s)\n", (yyvsp[-1].str)));
		if(!cfg_str2list_insert(&cfg_parser->cfg->respip_data,
			(yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding response-ip-data");
	}
#line 6934 "util/configparser.c"
    break;

  case 656: /* dnscstart: VAR_DNSCRYPT  */
#line 3525 "util/configparser.y"
        {
		OUTYY(("\nP(dnscrypt:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 6943 "util/configparser.c"
    break;

  case 669: /* dnsc_dnscrypt_enable: VAR_DNSCRYPT_ENABLE STRING_ARG  */
#line 3542 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnscrypt = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6955 "util/configparser.c"
    break;

  case 670: /* dnsc_dnscrypt_port: VAR_DNSCRYPT_PORT STRING_ARG  */
#line 3552 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->dnscrypt_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6967 "util/configparser.c"
    break;

  case 671: /* dnsc_dnscrypt_provider: VAR_DNSCRYPT_PROVIDER STRING_ARG  */
#line 3561 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_provider:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnscrypt_provider);
		cfg_parser->cfg->dnscrypt_provider = (yyvsp[0].str);
	}
#line 6977 "util/configparser.c"
    break;

  case 672: /* dnsc_dnscrypt_provider_cert: VAR_DNSCRYPT_PROVIDER_CERT STRING_ARG  */
#line 3568 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_provider_cert:%s)\n", (yyvsp[0].str)));
		if(cfg_strlist_find(cfg_parser->cfg->dnscrypt_provider_cert, (yyvsp[0].str)))
			log_warn("dnscrypt-provider-cert %s is a duplicate", (yyvsp[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_provider_cert, (yyvsp[0].str)))
			fatal_exit("out of memory adding dnscrypt-provider-cert");
	}
#line 6989 "util/configparser.c"
    break;

  case 673: /* dnsc_dnscrypt_provider_cert_rotated: VAR_DNSCRYPT_PROVIDER_CERT_ROTATED STRING_ARG  */
#line 3577 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_provider_cert_rotated:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_provider_cert_rotated, (yyvsp[0].str)))
			fatal_exit("out of memory adding dnscrypt-provider-cert-rotated");
	}
#line 6999 "util/configparser.c"
    break;

  case 674: /* dnsc_dnscrypt_secret_key: VAR_DNSCRYPT_SECRET_KEY STRING_ARG  */
#line 3584 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_secret_key:%s)\n", (yyvsp[0].str)));
		if(cfg_strlist_find(cfg_parser->cfg->dnscrypt_secret_key, (yyvsp[0].str)))
			log_warn("dnscrypt-secret-key: %s is a duplicate", (yyvsp[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_secret_key, (yyvsp[0].str)))
			fatal_exit("out of memory adding dnscrypt-secret-key");
	}
#line 7011 "util/configparser.c"
    break;

  case 675: /* dnsc_dnscrypt_shared_secret_cache_size: VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE STRING_ARG  */
#line 3593 "util/configparser.y"
  {
	OUTYY(("P(dnscrypt_shared_secret_cache_size:%s)\n", (yyvsp[0].str)));
	if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->dnscrypt_shared_secret_cache_size))
		yyerror("memory size expected");
	free((yyvsp[0].str));
  }
#line 7022 "util/configparser.c"
    break;

  case 676: /* dnsc_dnscrypt_shared_secret_cache_slabs: VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS STRING_ARG  */
#line 3601 "util/configparser.y"
  {
	OUTYY(("P(dnscrypt_shared_secret_cache_slabs:%s)\n", (yyvsp[0].str)));
	if(atoi((yyvsp[0].str)) == 0) {
		yyerror("number expected");
	} else {
		cfg_parser->cfg->dnscrypt_shared_secret_cache_slabs = atoi((yyvsp[0].str));
		if(!is_pow2(cfg_parser->cfg->dnscrypt_shared_secret_cache_slabs))
			yyerror("must be a power of 2");
	}
	free((yyvsp[0].str));
  }
#line 7038 "util/configparser.c"
    break;

  case 677: /* dnsc_dnscrypt_nonce_cache_size: VAR_DNSCRYPT_NONCE_CACHE_SIZE STRING_ARG  */
#line 3614 "util/configparser.y"
  {
	OUTYY(("P(dnscrypt_nonce_cache_size:%s)\n", (yyvsp[0].str)));
	if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->dnscrypt_nonce_cache_size))
		yyerror("memory size expected");
	free((yyvsp[0].str));
  }
#line 7049 "util/configparser.c"
    break;

  case 678: /* dnsc_dnscrypt_nonce_cache_slabs: VAR_DNSCRYPT_NONCE_CACHE_SLABS STRING_ARG  */
#line 3622 "util/configparser.y"
  {
	OUTYY(("P(dnscrypt_nonce_cache_slabs:%s)\n", (yyvsp[0].str)));
	if(atoi((yyvsp[0].str)) == 0) {
		yyerror("number expected");
	} else {
		cfg_parser->cfg->dnscrypt_nonce_cache_slabs = atoi((yyvsp[0].str));
		if(!is_pow2(cfg_parser->cfg->dnscrypt_nonce_cache_slabs))
			yyerror("must be a power of 2");
	}
	free((yyvsp[0].str));
  }
#line 7065 "util/configparser.c"
    break;

  case 679: /* cachedbstart: VAR_CACHEDB  */
#line 3635 "util/configparser.y"
        {
		OUTYY(("\nP(cachedb:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 7074 "util/configparser.c"
    break;

  case 688: /* cachedb_backend_name: VAR_CACHEDB_BACKEND STRING_ARG  */
#line 3647 "util/configparser.y"
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
#line 7089 "util/configparser.c"
    break;

  case 689: /* cachedb_secret_seed: VAR_CACHEDB_SECRETSEED STRING_ARG  */
#line 3659 "util/configparser.y"
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
#line 7104 "util/configparser.c"
    break;

  case 690: /* redis_server_host: VAR_CACHEDB_REDISHOST STRING_ARG  */
#line 3671 "util/configparser.y"
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
#line 7119 "util/configparser.c"
    break;

  case 691: /* redis_server_port: VAR_CACHEDB_REDISPORT STRING_ARG  */
#line 3683 "util/configparser.y"
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
#line 7137 "util/configparser.c"
    break;

  case 692: /* redis_timeout: VAR_CACHEDB_REDISTIMEOUT STRING_ARG  */
#line 3698 "util/configparser.y"
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
#line 7153 "util/configparser.c"
    break;

  case 693: /* redis_expire_records: VAR_CACHEDB_REDISEXPIRERECORDS STRING_ARG  */
#line 3711 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		OUTYY(("P(redis_expire_records:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->redis_expire_records = (strcmp((yyvsp[0].str), "yes")==0);
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 7169 "util/configparser.c"
    break;

  case 694: /* server_tcp_connection_limit: VAR_TCP_CONNECTION_LIMIT STRING_ARG STRING_ARG  */
#line 3724 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_connection_limit:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if (atoi((yyvsp[0].str)) < 0)
			yyerror("positive number expected");
		else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->tcp_connection_limits, (yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding tcp connection limit");
		}
	}
#line 7183 "util/configparser.c"
    break;

  case 695: /* ipsetstart: VAR_IPSET  */
#line 3735 "util/configparser.y"
                {
			OUTYY(("\nP(ipset:)\n"));
			cfg_parser->started_toplevel = 1;
		}
#line 7192 "util/configparser.c"
    break;

  case 700: /* ipset_name_v4: VAR_IPSET_NAME_V4 STRING_ARG  */
#line 3745 "util/configparser.y"
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
#line 7210 "util/configparser.c"
    break;

  case 701: /* ipset_name_v6: VAR_IPSET_NAME_V6 STRING_ARG  */
#line 3760 "util/configparser.y"
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
#line 7228 "util/configparser.c"
    break;


#line 7232 "util/configparser.c"

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
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

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
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
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

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
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
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

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


#if !defined yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;
#endif


/*-------------------------------------------------------.
| yyreturn -- parsing is finished, clean up and return.  |
`-------------------------------------------------------*/
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
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 3774 "util/configparser.y"


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

static void
validate_acl_action(const char* action)
{
	if(strcmp(action, "deny")!=0 &&
		strcmp(action, "refuse")!=0 &&
		strcmp(action, "deny_non_local")!=0 &&
		strcmp(action, "refuse_non_local")!=0 &&
		strcmp(action, "allow_setrd")!=0 &&
		strcmp(action, "allow")!=0 &&
		strcmp(action, "allow_snoop")!=0)
	{
		yyerror("expected deny, refuse, deny_non_local, "
			"refuse_non_local, allow, allow_setrd or "
			"allow_snoop as access control action");
	}
}
