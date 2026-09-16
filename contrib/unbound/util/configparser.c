/* A Bison parser, made by GNU Bison 3.8.2.  */

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
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

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
#include "sldns/str2wire.h"

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


#line 102 "util/configparser.c"

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
  YYSYMBOL_VAR_DO_NAT64 = 21,              /* VAR_DO_NAT64  */
  YYSYMBOL_VAR_PREFER_IP6 = 22,            /* VAR_PREFER_IP6  */
  YYSYMBOL_VAR_DO_UDP = 23,                /* VAR_DO_UDP  */
  YYSYMBOL_VAR_DO_TCP = 24,                /* VAR_DO_TCP  */
  YYSYMBOL_VAR_TCP_MSS = 25,               /* VAR_TCP_MSS  */
  YYSYMBOL_VAR_OUTGOING_TCP_MSS = 26,      /* VAR_OUTGOING_TCP_MSS  */
  YYSYMBOL_VAR_TCP_IDLE_TIMEOUT = 27,      /* VAR_TCP_IDLE_TIMEOUT  */
  YYSYMBOL_VAR_EDNS_TCP_KEEPALIVE = 28,    /* VAR_EDNS_TCP_KEEPALIVE  */
  YYSYMBOL_VAR_EDNS_TCP_KEEPALIVE_TIMEOUT = 29, /* VAR_EDNS_TCP_KEEPALIVE_TIMEOUT  */
  YYSYMBOL_VAR_SOCK_QUEUE_TIMEOUT = 30,    /* VAR_SOCK_QUEUE_TIMEOUT  */
  YYSYMBOL_VAR_CHROOT = 31,                /* VAR_CHROOT  */
  YYSYMBOL_VAR_USERNAME = 32,              /* VAR_USERNAME  */
  YYSYMBOL_VAR_DIRECTORY = 33,             /* VAR_DIRECTORY  */
  YYSYMBOL_VAR_LOGFILE = 34,               /* VAR_LOGFILE  */
  YYSYMBOL_VAR_PIDFILE = 35,               /* VAR_PIDFILE  */
  YYSYMBOL_VAR_MSG_CACHE_SIZE = 36,        /* VAR_MSG_CACHE_SIZE  */
  YYSYMBOL_VAR_MSG_CACHE_SLABS = 37,       /* VAR_MSG_CACHE_SLABS  */
  YYSYMBOL_VAR_NUM_QUERIES_PER_THREAD = 38, /* VAR_NUM_QUERIES_PER_THREAD  */
  YYSYMBOL_VAR_RRSET_CACHE_SIZE = 39,      /* VAR_RRSET_CACHE_SIZE  */
  YYSYMBOL_VAR_RRSET_CACHE_SLABS = 40,     /* VAR_RRSET_CACHE_SLABS  */
  YYSYMBOL_VAR_OUTGOING_NUM_TCP = 41,      /* VAR_OUTGOING_NUM_TCP  */
  YYSYMBOL_VAR_INFRA_HOST_TTL = 42,        /* VAR_INFRA_HOST_TTL  */
  YYSYMBOL_VAR_INFRA_LAME_TTL = 43,        /* VAR_INFRA_LAME_TTL  */
  YYSYMBOL_VAR_INFRA_CACHE_SLABS = 44,     /* VAR_INFRA_CACHE_SLABS  */
  YYSYMBOL_VAR_INFRA_CACHE_NUMHOSTS = 45,  /* VAR_INFRA_CACHE_NUMHOSTS  */
  YYSYMBOL_VAR_INFRA_CACHE_LAME_SIZE = 46, /* VAR_INFRA_CACHE_LAME_SIZE  */
  YYSYMBOL_VAR_NAME = 47,                  /* VAR_NAME  */
  YYSYMBOL_VAR_STUB_ZONE = 48,             /* VAR_STUB_ZONE  */
  YYSYMBOL_VAR_STUB_HOST = 49,             /* VAR_STUB_HOST  */
  YYSYMBOL_VAR_STUB_ADDR = 50,             /* VAR_STUB_ADDR  */
  YYSYMBOL_VAR_TARGET_FETCH_POLICY = 51,   /* VAR_TARGET_FETCH_POLICY  */
  YYSYMBOL_VAR_HARDEN_SHORT_BUFSIZE = 52,  /* VAR_HARDEN_SHORT_BUFSIZE  */
  YYSYMBOL_VAR_HARDEN_LARGE_QUERIES = 53,  /* VAR_HARDEN_LARGE_QUERIES  */
  YYSYMBOL_VAR_FORWARD_ZONE = 54,          /* VAR_FORWARD_ZONE  */
  YYSYMBOL_VAR_FORWARD_HOST = 55,          /* VAR_FORWARD_HOST  */
  YYSYMBOL_VAR_FORWARD_ADDR = 56,          /* VAR_FORWARD_ADDR  */
  YYSYMBOL_VAR_DO_NOT_QUERY_ADDRESS = 57,  /* VAR_DO_NOT_QUERY_ADDRESS  */
  YYSYMBOL_VAR_HIDE_IDENTITY = 58,         /* VAR_HIDE_IDENTITY  */
  YYSYMBOL_VAR_HIDE_VERSION = 59,          /* VAR_HIDE_VERSION  */
  YYSYMBOL_VAR_IDENTITY = 60,              /* VAR_IDENTITY  */
  YYSYMBOL_VAR_VERSION = 61,               /* VAR_VERSION  */
  YYSYMBOL_VAR_HARDEN_GLUE = 62,           /* VAR_HARDEN_GLUE  */
  YYSYMBOL_VAR_MODULE_CONF = 63,           /* VAR_MODULE_CONF  */
  YYSYMBOL_VAR_TRUST_ANCHOR_FILE = 64,     /* VAR_TRUST_ANCHOR_FILE  */
  YYSYMBOL_VAR_TRUST_ANCHOR = 65,          /* VAR_TRUST_ANCHOR  */
  YYSYMBOL_VAR_VAL_OVERRIDE_DATE = 66,     /* VAR_VAL_OVERRIDE_DATE  */
  YYSYMBOL_VAR_BOGUS_TTL = 67,             /* VAR_BOGUS_TTL  */
  YYSYMBOL_VAR_VAL_CLEAN_ADDITIONAL = 68,  /* VAR_VAL_CLEAN_ADDITIONAL  */
  YYSYMBOL_VAR_VAL_PERMISSIVE_MODE = 69,   /* VAR_VAL_PERMISSIVE_MODE  */
  YYSYMBOL_VAR_INCOMING_NUM_TCP = 70,      /* VAR_INCOMING_NUM_TCP  */
  YYSYMBOL_VAR_MSG_BUFFER_SIZE = 71,       /* VAR_MSG_BUFFER_SIZE  */
  YYSYMBOL_VAR_KEY_CACHE_SIZE = 72,        /* VAR_KEY_CACHE_SIZE  */
  YYSYMBOL_VAR_KEY_CACHE_SLABS = 73,       /* VAR_KEY_CACHE_SLABS  */
  YYSYMBOL_VAR_TRUSTED_KEYS_FILE = 74,     /* VAR_TRUSTED_KEYS_FILE  */
  YYSYMBOL_VAR_VAL_NSEC3_KEYSIZE_ITERATIONS = 75, /* VAR_VAL_NSEC3_KEYSIZE_ITERATIONS  */
  YYSYMBOL_VAR_USE_SYSLOG = 76,            /* VAR_USE_SYSLOG  */
  YYSYMBOL_VAR_OUTGOING_INTERFACE = 77,    /* VAR_OUTGOING_INTERFACE  */
  YYSYMBOL_VAR_ROOT_HINTS = 78,            /* VAR_ROOT_HINTS  */
  YYSYMBOL_VAR_DO_NOT_QUERY_LOCALHOST = 79, /* VAR_DO_NOT_QUERY_LOCALHOST  */
  YYSYMBOL_VAR_CACHE_MAX_TTL = 80,         /* VAR_CACHE_MAX_TTL  */
  YYSYMBOL_VAR_HARDEN_DNSSEC_STRIPPED = 81, /* VAR_HARDEN_DNSSEC_STRIPPED  */
  YYSYMBOL_VAR_ACCESS_CONTROL = 82,        /* VAR_ACCESS_CONTROL  */
  YYSYMBOL_VAR_LOCAL_ZONE = 83,            /* VAR_LOCAL_ZONE  */
  YYSYMBOL_VAR_LOCAL_DATA = 84,            /* VAR_LOCAL_DATA  */
  YYSYMBOL_VAR_INTERFACE_AUTOMATIC = 85,   /* VAR_INTERFACE_AUTOMATIC  */
  YYSYMBOL_VAR_STATISTICS_INTERVAL = 86,   /* VAR_STATISTICS_INTERVAL  */
  YYSYMBOL_VAR_DO_DAEMONIZE = 87,          /* VAR_DO_DAEMONIZE  */
  YYSYMBOL_VAR_USE_CAPS_FOR_ID = 88,       /* VAR_USE_CAPS_FOR_ID  */
  YYSYMBOL_VAR_STATISTICS_CUMULATIVE = 89, /* VAR_STATISTICS_CUMULATIVE  */
  YYSYMBOL_VAR_OUTGOING_PORT_PERMIT = 90,  /* VAR_OUTGOING_PORT_PERMIT  */
  YYSYMBOL_VAR_OUTGOING_PORT_AVOID = 91,   /* VAR_OUTGOING_PORT_AVOID  */
  YYSYMBOL_VAR_DLV_ANCHOR_FILE = 92,       /* VAR_DLV_ANCHOR_FILE  */
  YYSYMBOL_VAR_DLV_ANCHOR = 93,            /* VAR_DLV_ANCHOR  */
  YYSYMBOL_VAR_NEG_CACHE_SIZE = 94,        /* VAR_NEG_CACHE_SIZE  */
  YYSYMBOL_VAR_HARDEN_REFERRAL_PATH = 95,  /* VAR_HARDEN_REFERRAL_PATH  */
  YYSYMBOL_VAR_PRIVATE_ADDRESS = 96,       /* VAR_PRIVATE_ADDRESS  */
  YYSYMBOL_VAR_PRIVATE_DOMAIN = 97,        /* VAR_PRIVATE_DOMAIN  */
  YYSYMBOL_VAR_REMOTE_CONTROL = 98,        /* VAR_REMOTE_CONTROL  */
  YYSYMBOL_VAR_CONTROL_ENABLE = 99,        /* VAR_CONTROL_ENABLE  */
  YYSYMBOL_VAR_CONTROL_INTERFACE = 100,    /* VAR_CONTROL_INTERFACE  */
  YYSYMBOL_VAR_CONTROL_PORT = 101,         /* VAR_CONTROL_PORT  */
  YYSYMBOL_VAR_SERVER_KEY_FILE = 102,      /* VAR_SERVER_KEY_FILE  */
  YYSYMBOL_VAR_SERVER_CERT_FILE = 103,     /* VAR_SERVER_CERT_FILE  */
  YYSYMBOL_VAR_CONTROL_KEY_FILE = 104,     /* VAR_CONTROL_KEY_FILE  */
  YYSYMBOL_VAR_CONTROL_CERT_FILE = 105,    /* VAR_CONTROL_CERT_FILE  */
  YYSYMBOL_VAR_CONTROL_USE_CERT = 106,     /* VAR_CONTROL_USE_CERT  */
  YYSYMBOL_VAR_TCP_REUSE_TIMEOUT = 107,    /* VAR_TCP_REUSE_TIMEOUT  */
  YYSYMBOL_VAR_MAX_REUSE_TCP_QUERIES = 108, /* VAR_MAX_REUSE_TCP_QUERIES  */
  YYSYMBOL_VAR_EXTENDED_STATISTICS = 109,  /* VAR_EXTENDED_STATISTICS  */
  YYSYMBOL_VAR_LOCAL_DATA_PTR = 110,       /* VAR_LOCAL_DATA_PTR  */
  YYSYMBOL_VAR_JOSTLE_TIMEOUT = 111,       /* VAR_JOSTLE_TIMEOUT  */
  YYSYMBOL_VAR_STUB_PRIME = 112,           /* VAR_STUB_PRIME  */
  YYSYMBOL_VAR_UNWANTED_REPLY_THRESHOLD = 113, /* VAR_UNWANTED_REPLY_THRESHOLD  */
  YYSYMBOL_VAR_LOG_TIME_ASCII = 114,       /* VAR_LOG_TIME_ASCII  */
  YYSYMBOL_VAR_DOMAIN_INSECURE = 115,      /* VAR_DOMAIN_INSECURE  */
  YYSYMBOL_VAR_PYTHON = 116,               /* VAR_PYTHON  */
  YYSYMBOL_VAR_PYTHON_SCRIPT = 117,        /* VAR_PYTHON_SCRIPT  */
  YYSYMBOL_VAR_VAL_SIG_SKEW_MIN = 118,     /* VAR_VAL_SIG_SKEW_MIN  */
  YYSYMBOL_VAR_VAL_SIG_SKEW_MAX = 119,     /* VAR_VAL_SIG_SKEW_MAX  */
  YYSYMBOL_VAR_VAL_MAX_RESTART = 120,      /* VAR_VAL_MAX_RESTART  */
  YYSYMBOL_VAR_CACHE_MIN_TTL = 121,        /* VAR_CACHE_MIN_TTL  */
  YYSYMBOL_VAR_VAL_LOG_LEVEL = 122,        /* VAR_VAL_LOG_LEVEL  */
  YYSYMBOL_VAR_AUTO_TRUST_ANCHOR_FILE = 123, /* VAR_AUTO_TRUST_ANCHOR_FILE  */
  YYSYMBOL_VAR_KEEP_MISSING = 124,         /* VAR_KEEP_MISSING  */
  YYSYMBOL_VAR_ADD_HOLDDOWN = 125,         /* VAR_ADD_HOLDDOWN  */
  YYSYMBOL_VAR_DEL_HOLDDOWN = 126,         /* VAR_DEL_HOLDDOWN  */
  YYSYMBOL_VAR_SO_RCVBUF = 127,            /* VAR_SO_RCVBUF  */
  YYSYMBOL_VAR_EDNS_BUFFER_SIZE = 128,     /* VAR_EDNS_BUFFER_SIZE  */
  YYSYMBOL_VAR_PREFETCH = 129,             /* VAR_PREFETCH  */
  YYSYMBOL_VAR_PREFETCH_KEY = 130,         /* VAR_PREFETCH_KEY  */
  YYSYMBOL_VAR_SO_SNDBUF = 131,            /* VAR_SO_SNDBUF  */
  YYSYMBOL_VAR_SO_REUSEPORT = 132,         /* VAR_SO_REUSEPORT  */
  YYSYMBOL_VAR_HARDEN_BELOW_NXDOMAIN = 133, /* VAR_HARDEN_BELOW_NXDOMAIN  */
  YYSYMBOL_VAR_IGNORE_CD_FLAG = 134,       /* VAR_IGNORE_CD_FLAG  */
  YYSYMBOL_VAR_LOG_QUERIES = 135,          /* VAR_LOG_QUERIES  */
  YYSYMBOL_VAR_LOG_REPLIES = 136,          /* VAR_LOG_REPLIES  */
  YYSYMBOL_VAR_LOG_LOCAL_ACTIONS = 137,    /* VAR_LOG_LOCAL_ACTIONS  */
  YYSYMBOL_VAR_TCP_UPSTREAM = 138,         /* VAR_TCP_UPSTREAM  */
  YYSYMBOL_VAR_SSL_UPSTREAM = 139,         /* VAR_SSL_UPSTREAM  */
  YYSYMBOL_VAR_TCP_AUTH_QUERY_TIMEOUT = 140, /* VAR_TCP_AUTH_QUERY_TIMEOUT  */
  YYSYMBOL_VAR_SSL_SERVICE_KEY = 141,      /* VAR_SSL_SERVICE_KEY  */
  YYSYMBOL_VAR_SSL_SERVICE_PEM = 142,      /* VAR_SSL_SERVICE_PEM  */
  YYSYMBOL_VAR_SSL_PORT = 143,             /* VAR_SSL_PORT  */
  YYSYMBOL_VAR_FORWARD_FIRST = 144,        /* VAR_FORWARD_FIRST  */
  YYSYMBOL_VAR_STUB_SSL_UPSTREAM = 145,    /* VAR_STUB_SSL_UPSTREAM  */
  YYSYMBOL_VAR_FORWARD_SSL_UPSTREAM = 146, /* VAR_FORWARD_SSL_UPSTREAM  */
  YYSYMBOL_VAR_TLS_CERT_BUNDLE = 147,      /* VAR_TLS_CERT_BUNDLE  */
  YYSYMBOL_VAR_STUB_TCP_UPSTREAM = 148,    /* VAR_STUB_TCP_UPSTREAM  */
  YYSYMBOL_VAR_FORWARD_TCP_UPSTREAM = 149, /* VAR_FORWARD_TCP_UPSTREAM  */
  YYSYMBOL_VAR_HTTPS_PORT = 150,           /* VAR_HTTPS_PORT  */
  YYSYMBOL_VAR_HTTP_ENDPOINT = 151,        /* VAR_HTTP_ENDPOINT  */
  YYSYMBOL_VAR_HTTP_MAX_STREAMS = 152,     /* VAR_HTTP_MAX_STREAMS  */
  YYSYMBOL_VAR_HTTP_QUERY_BUFFER_SIZE = 153, /* VAR_HTTP_QUERY_BUFFER_SIZE  */
  YYSYMBOL_VAR_HTTP_RESPONSE_BUFFER_SIZE = 154, /* VAR_HTTP_RESPONSE_BUFFER_SIZE  */
  YYSYMBOL_VAR_HTTP_NODELAY = 155,         /* VAR_HTTP_NODELAY  */
  YYSYMBOL_VAR_HTTP_NOTLS_DOWNSTREAM = 156, /* VAR_HTTP_NOTLS_DOWNSTREAM  */
  YYSYMBOL_VAR_STUB_FIRST = 157,           /* VAR_STUB_FIRST  */
  YYSYMBOL_VAR_MINIMAL_RESPONSES = 158,    /* VAR_MINIMAL_RESPONSES  */
  YYSYMBOL_VAR_RRSET_ROUNDROBIN = 159,     /* VAR_RRSET_ROUNDROBIN  */
  YYSYMBOL_VAR_MAX_UDP_SIZE = 160,         /* VAR_MAX_UDP_SIZE  */
  YYSYMBOL_VAR_DELAY_CLOSE = 161,          /* VAR_DELAY_CLOSE  */
  YYSYMBOL_VAR_UDP_CONNECT = 162,          /* VAR_UDP_CONNECT  */
  YYSYMBOL_VAR_UNBLOCK_LAN_ZONES = 163,    /* VAR_UNBLOCK_LAN_ZONES  */
  YYSYMBOL_VAR_INSECURE_LAN_ZONES = 164,   /* VAR_INSECURE_LAN_ZONES  */
  YYSYMBOL_VAR_INFRA_CACHE_MIN_RTT = 165,  /* VAR_INFRA_CACHE_MIN_RTT  */
  YYSYMBOL_VAR_INFRA_CACHE_MAX_RTT = 166,  /* VAR_INFRA_CACHE_MAX_RTT  */
  YYSYMBOL_VAR_INFRA_KEEP_PROBING = 167,   /* VAR_INFRA_KEEP_PROBING  */
  YYSYMBOL_VAR_DNS64_PREFIX = 168,         /* VAR_DNS64_PREFIX  */
  YYSYMBOL_VAR_DNS64_SYNTHALL = 169,       /* VAR_DNS64_SYNTHALL  */
  YYSYMBOL_VAR_DNS64_IGNORE_AAAA = 170,    /* VAR_DNS64_IGNORE_AAAA  */
  YYSYMBOL_VAR_NAT64_PREFIX = 171,         /* VAR_NAT64_PREFIX  */
  YYSYMBOL_VAR_DNSTAP = 172,               /* VAR_DNSTAP  */
  YYSYMBOL_VAR_DNSTAP_ENABLE = 173,        /* VAR_DNSTAP_ENABLE  */
  YYSYMBOL_VAR_DNSTAP_SOCKET_PATH = 174,   /* VAR_DNSTAP_SOCKET_PATH  */
  YYSYMBOL_VAR_DNSTAP_IP = 175,            /* VAR_DNSTAP_IP  */
  YYSYMBOL_VAR_DNSTAP_TLS = 176,           /* VAR_DNSTAP_TLS  */
  YYSYMBOL_VAR_DNSTAP_TLS_SERVER_NAME = 177, /* VAR_DNSTAP_TLS_SERVER_NAME  */
  YYSYMBOL_VAR_DNSTAP_TLS_CERT_BUNDLE = 178, /* VAR_DNSTAP_TLS_CERT_BUNDLE  */
  YYSYMBOL_VAR_DNSTAP_TLS_CLIENT_KEY_FILE = 179, /* VAR_DNSTAP_TLS_CLIENT_KEY_FILE  */
  YYSYMBOL_VAR_DNSTAP_TLS_CLIENT_CERT_FILE = 180, /* VAR_DNSTAP_TLS_CLIENT_CERT_FILE  */
  YYSYMBOL_VAR_DNSTAP_SEND_IDENTITY = 181, /* VAR_DNSTAP_SEND_IDENTITY  */
  YYSYMBOL_VAR_DNSTAP_SEND_VERSION = 182,  /* VAR_DNSTAP_SEND_VERSION  */
  YYSYMBOL_VAR_DNSTAP_BIDIRECTIONAL = 183, /* VAR_DNSTAP_BIDIRECTIONAL  */
  YYSYMBOL_VAR_DNSTAP_IDENTITY = 184,      /* VAR_DNSTAP_IDENTITY  */
  YYSYMBOL_VAR_DNSTAP_VERSION = 185,       /* VAR_DNSTAP_VERSION  */
  YYSYMBOL_VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES = 186, /* VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES  */
  YYSYMBOL_VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES = 187, /* VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES  */
  YYSYMBOL_VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES = 188, /* VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES  */
  YYSYMBOL_VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES = 189, /* VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES  */
  YYSYMBOL_VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES = 190, /* VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES  */
  YYSYMBOL_VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES = 191, /* VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES  */
  YYSYMBOL_VAR_DNSTAP_SAMPLE_RATE = 192,   /* VAR_DNSTAP_SAMPLE_RATE  */
  YYSYMBOL_VAR_RESPONSE_IP_TAG = 193,      /* VAR_RESPONSE_IP_TAG  */
  YYSYMBOL_VAR_RESPONSE_IP = 194,          /* VAR_RESPONSE_IP  */
  YYSYMBOL_VAR_RESPONSE_IP_DATA = 195,     /* VAR_RESPONSE_IP_DATA  */
  YYSYMBOL_VAR_HARDEN_ALGO_DOWNGRADE = 196, /* VAR_HARDEN_ALGO_DOWNGRADE  */
  YYSYMBOL_VAR_IP_TRANSPARENT = 197,       /* VAR_IP_TRANSPARENT  */
  YYSYMBOL_VAR_IP_DSCP = 198,              /* VAR_IP_DSCP  */
  YYSYMBOL_VAR_DISABLE_DNSSEC_LAME_CHECK = 199, /* VAR_DISABLE_DNSSEC_LAME_CHECK  */
  YYSYMBOL_VAR_IP_RATELIMIT = 200,         /* VAR_IP_RATELIMIT  */
  YYSYMBOL_VAR_IP_RATELIMIT_SLABS = 201,   /* VAR_IP_RATELIMIT_SLABS  */
  YYSYMBOL_VAR_IP_RATELIMIT_SIZE = 202,    /* VAR_IP_RATELIMIT_SIZE  */
  YYSYMBOL_VAR_RATELIMIT = 203,            /* VAR_RATELIMIT  */
  YYSYMBOL_VAR_RATELIMIT_SLABS = 204,      /* VAR_RATELIMIT_SLABS  */
  YYSYMBOL_VAR_RATELIMIT_SIZE = 205,       /* VAR_RATELIMIT_SIZE  */
  YYSYMBOL_VAR_OUTBOUND_MSG_RETRY = 206,   /* VAR_OUTBOUND_MSG_RETRY  */
  YYSYMBOL_VAR_MAX_SENT_COUNT = 207,       /* VAR_MAX_SENT_COUNT  */
  YYSYMBOL_VAR_MAX_QUERY_RESTARTS = 208,   /* VAR_MAX_QUERY_RESTARTS  */
  YYSYMBOL_VAR_RATELIMIT_FOR_DOMAIN = 209, /* VAR_RATELIMIT_FOR_DOMAIN  */
  YYSYMBOL_VAR_RATELIMIT_BELOW_DOMAIN = 210, /* VAR_RATELIMIT_BELOW_DOMAIN  */
  YYSYMBOL_VAR_IP_RATELIMIT_FACTOR = 211,  /* VAR_IP_RATELIMIT_FACTOR  */
  YYSYMBOL_VAR_RATELIMIT_FACTOR = 212,     /* VAR_RATELIMIT_FACTOR  */
  YYSYMBOL_VAR_IP_RATELIMIT_BACKOFF = 213, /* VAR_IP_RATELIMIT_BACKOFF  */
  YYSYMBOL_VAR_RATELIMIT_BACKOFF = 214,    /* VAR_RATELIMIT_BACKOFF  */
  YYSYMBOL_VAR_SEND_CLIENT_SUBNET = 215,   /* VAR_SEND_CLIENT_SUBNET  */
  YYSYMBOL_VAR_CLIENT_SUBNET_ZONE = 216,   /* VAR_CLIENT_SUBNET_ZONE  */
  YYSYMBOL_VAR_CLIENT_SUBNET_ALWAYS_FORWARD = 217, /* VAR_CLIENT_SUBNET_ALWAYS_FORWARD  */
  YYSYMBOL_VAR_CLIENT_SUBNET_OPCODE = 218, /* VAR_CLIENT_SUBNET_OPCODE  */
  YYSYMBOL_VAR_MAX_CLIENT_SUBNET_IPV4 = 219, /* VAR_MAX_CLIENT_SUBNET_IPV4  */
  YYSYMBOL_VAR_MAX_CLIENT_SUBNET_IPV6 = 220, /* VAR_MAX_CLIENT_SUBNET_IPV6  */
  YYSYMBOL_VAR_MIN_CLIENT_SUBNET_IPV4 = 221, /* VAR_MIN_CLIENT_SUBNET_IPV4  */
  YYSYMBOL_VAR_MIN_CLIENT_SUBNET_IPV6 = 222, /* VAR_MIN_CLIENT_SUBNET_IPV6  */
  YYSYMBOL_VAR_MAX_ECS_TREE_SIZE_IPV4 = 223, /* VAR_MAX_ECS_TREE_SIZE_IPV4  */
  YYSYMBOL_VAR_MAX_ECS_TREE_SIZE_IPV6 = 224, /* VAR_MAX_ECS_TREE_SIZE_IPV6  */
  YYSYMBOL_VAR_CAPS_WHITELIST = 225,       /* VAR_CAPS_WHITELIST  */
  YYSYMBOL_VAR_CACHE_MAX_NEGATIVE_TTL = 226, /* VAR_CACHE_MAX_NEGATIVE_TTL  */
  YYSYMBOL_VAR_PERMIT_SMALL_HOLDDOWN = 227, /* VAR_PERMIT_SMALL_HOLDDOWN  */
  YYSYMBOL_VAR_CACHE_MIN_NEGATIVE_TTL = 228, /* VAR_CACHE_MIN_NEGATIVE_TTL  */
  YYSYMBOL_VAR_QNAME_MINIMISATION = 229,   /* VAR_QNAME_MINIMISATION  */
  YYSYMBOL_VAR_QNAME_MINIMISATION_STRICT = 230, /* VAR_QNAME_MINIMISATION_STRICT  */
  YYSYMBOL_VAR_IP_FREEBIND = 231,          /* VAR_IP_FREEBIND  */
  YYSYMBOL_VAR_DEFINE_TAG = 232,           /* VAR_DEFINE_TAG  */
  YYSYMBOL_VAR_LOCAL_ZONE_TAG = 233,       /* VAR_LOCAL_ZONE_TAG  */
  YYSYMBOL_VAR_ACCESS_CONTROL_TAG = 234,   /* VAR_ACCESS_CONTROL_TAG  */
  YYSYMBOL_VAR_LOCAL_ZONE_OVERRIDE = 235,  /* VAR_LOCAL_ZONE_OVERRIDE  */
  YYSYMBOL_VAR_ACCESS_CONTROL_TAG_ACTION = 236, /* VAR_ACCESS_CONTROL_TAG_ACTION  */
  YYSYMBOL_VAR_ACCESS_CONTROL_TAG_DATA = 237, /* VAR_ACCESS_CONTROL_TAG_DATA  */
  YYSYMBOL_VAR_VIEW = 238,                 /* VAR_VIEW  */
  YYSYMBOL_VAR_ACCESS_CONTROL_VIEW = 239,  /* VAR_ACCESS_CONTROL_VIEW  */
  YYSYMBOL_VAR_VIEW_FIRST = 240,           /* VAR_VIEW_FIRST  */
  YYSYMBOL_VAR_SERVE_EXPIRED = 241,        /* VAR_SERVE_EXPIRED  */
  YYSYMBOL_VAR_SERVE_EXPIRED_TTL = 242,    /* VAR_SERVE_EXPIRED_TTL  */
  YYSYMBOL_VAR_SERVE_EXPIRED_TTL_RESET = 243, /* VAR_SERVE_EXPIRED_TTL_RESET  */
  YYSYMBOL_VAR_SERVE_EXPIRED_REPLY_TTL = 244, /* VAR_SERVE_EXPIRED_REPLY_TTL  */
  YYSYMBOL_VAR_SERVE_EXPIRED_CLIENT_TIMEOUT = 245, /* VAR_SERVE_EXPIRED_CLIENT_TIMEOUT  */
  YYSYMBOL_VAR_EDE_SERVE_EXPIRED = 246,    /* VAR_EDE_SERVE_EXPIRED  */
  YYSYMBOL_VAR_SERVE_ORIGINAL_TTL = 247,   /* VAR_SERVE_ORIGINAL_TTL  */
  YYSYMBOL_VAR_FAKE_DSA = 248,             /* VAR_FAKE_DSA  */
  YYSYMBOL_VAR_FAKE_SHA1 = 249,            /* VAR_FAKE_SHA1  */
  YYSYMBOL_VAR_LOG_IDENTITY = 250,         /* VAR_LOG_IDENTITY  */
  YYSYMBOL_VAR_HIDE_TRUSTANCHOR = 251,     /* VAR_HIDE_TRUSTANCHOR  */
  YYSYMBOL_VAR_HIDE_HTTP_USER_AGENT = 252, /* VAR_HIDE_HTTP_USER_AGENT  */
  YYSYMBOL_VAR_HTTP_USER_AGENT = 253,      /* VAR_HTTP_USER_AGENT  */
  YYSYMBOL_VAR_TRUST_ANCHOR_SIGNALING = 254, /* VAR_TRUST_ANCHOR_SIGNALING  */
  YYSYMBOL_VAR_AGGRESSIVE_NSEC = 255,      /* VAR_AGGRESSIVE_NSEC  */
  YYSYMBOL_VAR_USE_SYSTEMD = 256,          /* VAR_USE_SYSTEMD  */
  YYSYMBOL_VAR_SHM_ENABLE = 257,           /* VAR_SHM_ENABLE  */
  YYSYMBOL_VAR_SHM_KEY = 258,              /* VAR_SHM_KEY  */
  YYSYMBOL_VAR_ROOT_KEY_SENTINEL = 259,    /* VAR_ROOT_KEY_SENTINEL  */
  YYSYMBOL_VAR_DNSCRYPT = 260,             /* VAR_DNSCRYPT  */
  YYSYMBOL_VAR_DNSCRYPT_ENABLE = 261,      /* VAR_DNSCRYPT_ENABLE  */
  YYSYMBOL_VAR_DNSCRYPT_PORT = 262,        /* VAR_DNSCRYPT_PORT  */
  YYSYMBOL_VAR_DNSCRYPT_PROVIDER = 263,    /* VAR_DNSCRYPT_PROVIDER  */
  YYSYMBOL_VAR_DNSCRYPT_SECRET_KEY = 264,  /* VAR_DNSCRYPT_SECRET_KEY  */
  YYSYMBOL_VAR_DNSCRYPT_PROVIDER_CERT = 265, /* VAR_DNSCRYPT_PROVIDER_CERT  */
  YYSYMBOL_VAR_DNSCRYPT_PROVIDER_CERT_ROTATED = 266, /* VAR_DNSCRYPT_PROVIDER_CERT_ROTATED  */
  YYSYMBOL_VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE = 267, /* VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE  */
  YYSYMBOL_VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS = 268, /* VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS  */
  YYSYMBOL_VAR_DNSCRYPT_NONCE_CACHE_SIZE = 269, /* VAR_DNSCRYPT_NONCE_CACHE_SIZE  */
  YYSYMBOL_VAR_DNSCRYPT_NONCE_CACHE_SLABS = 270, /* VAR_DNSCRYPT_NONCE_CACHE_SLABS  */
  YYSYMBOL_VAR_PAD_RESPONSES = 271,        /* VAR_PAD_RESPONSES  */
  YYSYMBOL_VAR_PAD_RESPONSES_BLOCK_SIZE = 272, /* VAR_PAD_RESPONSES_BLOCK_SIZE  */
  YYSYMBOL_VAR_PAD_QUERIES = 273,          /* VAR_PAD_QUERIES  */
  YYSYMBOL_VAR_PAD_QUERIES_BLOCK_SIZE = 274, /* VAR_PAD_QUERIES_BLOCK_SIZE  */
  YYSYMBOL_VAR_IPSECMOD_ENABLED = 275,     /* VAR_IPSECMOD_ENABLED  */
  YYSYMBOL_VAR_IPSECMOD_HOOK = 276,        /* VAR_IPSECMOD_HOOK  */
  YYSYMBOL_VAR_IPSECMOD_IGNORE_BOGUS = 277, /* VAR_IPSECMOD_IGNORE_BOGUS  */
  YYSYMBOL_VAR_IPSECMOD_MAX_TTL = 278,     /* VAR_IPSECMOD_MAX_TTL  */
  YYSYMBOL_VAR_IPSECMOD_WHITELIST = 279,   /* VAR_IPSECMOD_WHITELIST  */
  YYSYMBOL_VAR_IPSECMOD_STRICT = 280,      /* VAR_IPSECMOD_STRICT  */
  YYSYMBOL_VAR_CACHEDB = 281,              /* VAR_CACHEDB  */
  YYSYMBOL_VAR_CACHEDB_BACKEND = 282,      /* VAR_CACHEDB_BACKEND  */
  YYSYMBOL_VAR_CACHEDB_SECRETSEED = 283,   /* VAR_CACHEDB_SECRETSEED  */
  YYSYMBOL_VAR_CACHEDB_REDISHOST = 284,    /* VAR_CACHEDB_REDISHOST  */
  YYSYMBOL_VAR_CACHEDB_REDISREPLICAHOST = 285, /* VAR_CACHEDB_REDISREPLICAHOST  */
  YYSYMBOL_VAR_CACHEDB_REDISPORT = 286,    /* VAR_CACHEDB_REDISPORT  */
  YYSYMBOL_VAR_CACHEDB_REDISREPLICAPORT = 287, /* VAR_CACHEDB_REDISREPLICAPORT  */
  YYSYMBOL_VAR_CACHEDB_REDISTIMEOUT = 288, /* VAR_CACHEDB_REDISTIMEOUT  */
  YYSYMBOL_VAR_CACHEDB_REDISREPLICATIMEOUT = 289, /* VAR_CACHEDB_REDISREPLICATIMEOUT  */
  YYSYMBOL_VAR_CACHEDB_REDISEXPIRERECORDS = 290, /* VAR_CACHEDB_REDISEXPIRERECORDS  */
  YYSYMBOL_VAR_CACHEDB_REDISPATH = 291,    /* VAR_CACHEDB_REDISPATH  */
  YYSYMBOL_VAR_CACHEDB_REDISREPLICAPATH = 292, /* VAR_CACHEDB_REDISREPLICAPATH  */
  YYSYMBOL_VAR_CACHEDB_REDISPASSWORD = 293, /* VAR_CACHEDB_REDISPASSWORD  */
  YYSYMBOL_VAR_CACHEDB_REDISREPLICAPASSWORD = 294, /* VAR_CACHEDB_REDISREPLICAPASSWORD  */
  YYSYMBOL_VAR_CACHEDB_REDISLOGICALDB = 295, /* VAR_CACHEDB_REDISLOGICALDB  */
  YYSYMBOL_VAR_CACHEDB_REDISREPLICALOGICALDB = 296, /* VAR_CACHEDB_REDISREPLICALOGICALDB  */
  YYSYMBOL_VAR_CACHEDB_REDISCOMMANDTIMEOUT = 297, /* VAR_CACHEDB_REDISCOMMANDTIMEOUT  */
  YYSYMBOL_VAR_CACHEDB_REDISREPLICACOMMANDTIMEOUT = 298, /* VAR_CACHEDB_REDISREPLICACOMMANDTIMEOUT  */
  YYSYMBOL_VAR_CACHEDB_REDISCONNECTTIMEOUT = 299, /* VAR_CACHEDB_REDISCONNECTTIMEOUT  */
  YYSYMBOL_VAR_CACHEDB_REDISREPLICACONNECTTIMEOUT = 300, /* VAR_CACHEDB_REDISREPLICACONNECTTIMEOUT  */
  YYSYMBOL_VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM = 301, /* VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM  */
  YYSYMBOL_VAR_FOR_UPSTREAM = 302,         /* VAR_FOR_UPSTREAM  */
  YYSYMBOL_VAR_AUTH_ZONE = 303,            /* VAR_AUTH_ZONE  */
  YYSYMBOL_VAR_ZONEFILE = 304,             /* VAR_ZONEFILE  */
  YYSYMBOL_VAR_MASTER = 305,               /* VAR_MASTER  */
  YYSYMBOL_VAR_URL = 306,                  /* VAR_URL  */
  YYSYMBOL_VAR_FOR_DOWNSTREAM = 307,       /* VAR_FOR_DOWNSTREAM  */
  YYSYMBOL_VAR_FALLBACK_ENABLED = 308,     /* VAR_FALLBACK_ENABLED  */
  YYSYMBOL_VAR_TLS_ADDITIONAL_PORT = 309,  /* VAR_TLS_ADDITIONAL_PORT  */
  YYSYMBOL_VAR_LOW_RTT = 310,              /* VAR_LOW_RTT  */
  YYSYMBOL_VAR_LOW_RTT_PERMIL = 311,       /* VAR_LOW_RTT_PERMIL  */
  YYSYMBOL_VAR_FAST_SERVER_PERMIL = 312,   /* VAR_FAST_SERVER_PERMIL  */
  YYSYMBOL_VAR_FAST_SERVER_NUM = 313,      /* VAR_FAST_SERVER_NUM  */
  YYSYMBOL_VAR_ALLOW_NOTIFY = 314,         /* VAR_ALLOW_NOTIFY  */
  YYSYMBOL_VAR_TLS_WIN_CERT = 315,         /* VAR_TLS_WIN_CERT  */
  YYSYMBOL_VAR_TCP_CONNECTION_LIMIT = 316, /* VAR_TCP_CONNECTION_LIMIT  */
  YYSYMBOL_VAR_ANSWER_COOKIE = 317,        /* VAR_ANSWER_COOKIE  */
  YYSYMBOL_VAR_COOKIE_SECRET = 318,        /* VAR_COOKIE_SECRET  */
  YYSYMBOL_VAR_IP_RATELIMIT_COOKIE = 319,  /* VAR_IP_RATELIMIT_COOKIE  */
  YYSYMBOL_VAR_FORWARD_NO_CACHE = 320,     /* VAR_FORWARD_NO_CACHE  */
  YYSYMBOL_VAR_STUB_NO_CACHE = 321,        /* VAR_STUB_NO_CACHE  */
  YYSYMBOL_VAR_LOG_SERVFAIL = 322,         /* VAR_LOG_SERVFAIL  */
  YYSYMBOL_VAR_DENY_ANY = 323,             /* VAR_DENY_ANY  */
  YYSYMBOL_VAR_UNKNOWN_SERVER_TIME_LIMIT = 324, /* VAR_UNKNOWN_SERVER_TIME_LIMIT  */
  YYSYMBOL_VAR_LOG_TAG_QUERYREPLY = 325,   /* VAR_LOG_TAG_QUERYREPLY  */
  YYSYMBOL_VAR_DISCARD_TIMEOUT = 326,      /* VAR_DISCARD_TIMEOUT  */
  YYSYMBOL_VAR_WAIT_LIMIT = 327,           /* VAR_WAIT_LIMIT  */
  YYSYMBOL_VAR_WAIT_LIMIT_COOKIE = 328,    /* VAR_WAIT_LIMIT_COOKIE  */
  YYSYMBOL_VAR_WAIT_LIMIT_NETBLOCK = 329,  /* VAR_WAIT_LIMIT_NETBLOCK  */
  YYSYMBOL_VAR_WAIT_LIMIT_COOKIE_NETBLOCK = 330, /* VAR_WAIT_LIMIT_COOKIE_NETBLOCK  */
  YYSYMBOL_VAR_STREAM_WAIT_SIZE = 331,     /* VAR_STREAM_WAIT_SIZE  */
  YYSYMBOL_VAR_TLS_CIPHERS = 332,          /* VAR_TLS_CIPHERS  */
  YYSYMBOL_VAR_TLS_CIPHERSUITES = 333,     /* VAR_TLS_CIPHERSUITES  */
  YYSYMBOL_VAR_TLS_USE_SNI = 334,          /* VAR_TLS_USE_SNI  */
  YYSYMBOL_VAR_TLS_PROTOCOLS = 335,        /* VAR_TLS_PROTOCOLS  */
  YYSYMBOL_VAR_IPSET = 336,                /* VAR_IPSET  */
  YYSYMBOL_VAR_IPSET_NAME_V4 = 337,        /* VAR_IPSET_NAME_V4  */
  YYSYMBOL_VAR_IPSET_NAME_V6 = 338,        /* VAR_IPSET_NAME_V6  */
  YYSYMBOL_VAR_TLS_SESSION_TICKET_KEYS = 339, /* VAR_TLS_SESSION_TICKET_KEYS  */
  YYSYMBOL_VAR_RPZ = 340,                  /* VAR_RPZ  */
  YYSYMBOL_VAR_TAGS = 341,                 /* VAR_TAGS  */
  YYSYMBOL_VAR_RPZ_ACTION_OVERRIDE = 342,  /* VAR_RPZ_ACTION_OVERRIDE  */
  YYSYMBOL_VAR_RPZ_CNAME_OVERRIDE = 343,   /* VAR_RPZ_CNAME_OVERRIDE  */
  YYSYMBOL_VAR_RPZ_LOG = 344,              /* VAR_RPZ_LOG  */
  YYSYMBOL_VAR_RPZ_LOG_NAME = 345,         /* VAR_RPZ_LOG_NAME  */
  YYSYMBOL_VAR_DYNLIB = 346,               /* VAR_DYNLIB  */
  YYSYMBOL_VAR_DYNLIB_FILE = 347,          /* VAR_DYNLIB_FILE  */
  YYSYMBOL_VAR_EDNS_CLIENT_STRING = 348,   /* VAR_EDNS_CLIENT_STRING  */
  YYSYMBOL_VAR_EDNS_CLIENT_STRING_OPCODE = 349, /* VAR_EDNS_CLIENT_STRING_OPCODE  */
  YYSYMBOL_VAR_NSID = 350,                 /* VAR_NSID  */
  YYSYMBOL_VAR_ZONEMD_PERMISSIVE_MODE = 351, /* VAR_ZONEMD_PERMISSIVE_MODE  */
  YYSYMBOL_VAR_ZONEMD_CHECK = 352,         /* VAR_ZONEMD_CHECK  */
  YYSYMBOL_VAR_ZONEMD_REJECT_ABSENCE = 353, /* VAR_ZONEMD_REJECT_ABSENCE  */
  YYSYMBOL_VAR_RPZ_SIGNAL_NXDOMAIN_RA = 354, /* VAR_RPZ_SIGNAL_NXDOMAIN_RA  */
  YYSYMBOL_VAR_INTERFACE_AUTOMATIC_PORTS = 355, /* VAR_INTERFACE_AUTOMATIC_PORTS  */
  YYSYMBOL_VAR_EDE = 356,                  /* VAR_EDE  */
  YYSYMBOL_VAR_DNS_ERROR_REPORTING = 357,  /* VAR_DNS_ERROR_REPORTING  */
  YYSYMBOL_VAR_INTERFACE_ACTION = 358,     /* VAR_INTERFACE_ACTION  */
  YYSYMBOL_VAR_INTERFACE_VIEW = 359,       /* VAR_INTERFACE_VIEW  */
  YYSYMBOL_VAR_INTERFACE_TAG = 360,        /* VAR_INTERFACE_TAG  */
  YYSYMBOL_VAR_INTERFACE_TAG_ACTION = 361, /* VAR_INTERFACE_TAG_ACTION  */
  YYSYMBOL_VAR_INTERFACE_TAG_DATA = 362,   /* VAR_INTERFACE_TAG_DATA  */
  YYSYMBOL_VAR_QUIC_PORT = 363,            /* VAR_QUIC_PORT  */
  YYSYMBOL_VAR_QUIC_SIZE = 364,            /* VAR_QUIC_SIZE  */
  YYSYMBOL_VAR_PROXY_PROTOCOL_PORT = 365,  /* VAR_PROXY_PROTOCOL_PORT  */
  YYSYMBOL_VAR_STATISTICS_INHIBIT_ZERO = 366, /* VAR_STATISTICS_INHIBIT_ZERO  */
  YYSYMBOL_VAR_HARDEN_UNKNOWN_ADDITIONAL = 367, /* VAR_HARDEN_UNKNOWN_ADDITIONAL  */
  YYSYMBOL_VAR_DISABLE_EDNS_DO = 368,      /* VAR_DISABLE_EDNS_DO  */
  YYSYMBOL_VAR_CACHEDB_NO_STORE = 369,     /* VAR_CACHEDB_NO_STORE  */
  YYSYMBOL_VAR_LOG_DESTADDR = 370,         /* VAR_LOG_DESTADDR  */
  YYSYMBOL_VAR_CACHEDB_CHECK_WHEN_SERVE_EXPIRED = 371, /* VAR_CACHEDB_CHECK_WHEN_SERVE_EXPIRED  */
  YYSYMBOL_VAR_COOKIE_SECRET_FILE = 372,   /* VAR_COOKIE_SECRET_FILE  */
  YYSYMBOL_VAR_ITER_SCRUB_NS = 373,        /* VAR_ITER_SCRUB_NS  */
  YYSYMBOL_VAR_ITER_SCRUB_CNAME = 374,     /* VAR_ITER_SCRUB_CNAME  */
  YYSYMBOL_VAR_ITER_SCRUB_RRSIG = 375,     /* VAR_ITER_SCRUB_RRSIG  */
  YYSYMBOL_VAR_MAX_TRANSFER_SIZE = 376,    /* VAR_MAX_TRANSFER_SIZE  */
  YYSYMBOL_VAR_MAX_TRANSFER_TIME = 377,    /* VAR_MAX_TRANSFER_TIME  */
  YYSYMBOL_VAR_MAX_GLOBAL_QUOTA = 378,     /* VAR_MAX_GLOBAL_QUOTA  */
  YYSYMBOL_VAR_HARDEN_UNVERIFIED_GLUE = 379, /* VAR_HARDEN_UNVERIFIED_GLUE  */
  YYSYMBOL_VAR_LOG_TIME_ISO = 380,         /* VAR_LOG_TIME_ISO  */
  YYSYMBOL_VAR_VAL_VALIDATION_ATTEMPTS = 381, /* VAR_VAL_VALIDATION_ATTEMPTS  */
  YYSYMBOL_VAR_VAL_HASH_ATTEMPTS = 382,    /* VAR_VAL_HASH_ATTEMPTS  */
  YYSYMBOL_VAR_ITER_SCRUB_PROMISCUOUS = 383, /* VAR_ITER_SCRUB_PROMISCUOUS  */
  YYSYMBOL_VAR_LOG_THREAD_ID = 384,        /* VAR_LOG_THREAD_ID  */
  YYSYMBOL_YYACCEPT = 385,                 /* $accept  */
  YYSYMBOL_toplevelvars = 386,             /* toplevelvars  */
  YYSYMBOL_toplevelvar = 387,              /* toplevelvar  */
  YYSYMBOL_force_toplevel = 388,           /* force_toplevel  */
  YYSYMBOL_serverstart = 389,              /* serverstart  */
  YYSYMBOL_contents_server = 390,          /* contents_server  */
  YYSYMBOL_content_server = 391,           /* content_server  */
  YYSYMBOL_stub_clause = 392,              /* stub_clause  */
  YYSYMBOL_stubstart = 393,                /* stubstart  */
  YYSYMBOL_contents_stub = 394,            /* contents_stub  */
  YYSYMBOL_content_stub = 395,             /* content_stub  */
  YYSYMBOL_forward_clause = 396,           /* forward_clause  */
  YYSYMBOL_forwardstart = 397,             /* forwardstart  */
  YYSYMBOL_contents_forward = 398,         /* contents_forward  */
  YYSYMBOL_content_forward = 399,          /* content_forward  */
  YYSYMBOL_view_clause = 400,              /* view_clause  */
  YYSYMBOL_viewstart = 401,                /* viewstart  */
  YYSYMBOL_contents_view = 402,            /* contents_view  */
  YYSYMBOL_content_view = 403,             /* content_view  */
  YYSYMBOL_authstart = 404,                /* authstart  */
  YYSYMBOL_contents_auth = 405,            /* contents_auth  */
  YYSYMBOL_content_auth = 406,             /* content_auth  */
  YYSYMBOL_rpz_tag = 407,                  /* rpz_tag  */
  YYSYMBOL_rpz_action_override = 408,      /* rpz_action_override  */
  YYSYMBOL_rpz_cname_override = 409,       /* rpz_cname_override  */
  YYSYMBOL_rpz_log = 410,                  /* rpz_log  */
  YYSYMBOL_rpz_log_name = 411,             /* rpz_log_name  */
  YYSYMBOL_rpz_signal_nxdomain_ra = 412,   /* rpz_signal_nxdomain_ra  */
  YYSYMBOL_rpzstart = 413,                 /* rpzstart  */
  YYSYMBOL_contents_rpz = 414,             /* contents_rpz  */
  YYSYMBOL_content_rpz = 415,              /* content_rpz  */
  YYSYMBOL_server_num_threads = 416,       /* server_num_threads  */
  YYSYMBOL_server_verbosity = 417,         /* server_verbosity  */
  YYSYMBOL_server_statistics_interval = 418, /* server_statistics_interval  */
  YYSYMBOL_server_statistics_cumulative = 419, /* server_statistics_cumulative  */
  YYSYMBOL_server_extended_statistics = 420, /* server_extended_statistics  */
  YYSYMBOL_server_statistics_inhibit_zero = 421, /* server_statistics_inhibit_zero  */
  YYSYMBOL_server_shm_enable = 422,        /* server_shm_enable  */
  YYSYMBOL_server_shm_key = 423,           /* server_shm_key  */
  YYSYMBOL_server_port = 424,              /* server_port  */
  YYSYMBOL_server_send_client_subnet = 425, /* server_send_client_subnet  */
  YYSYMBOL_server_client_subnet_zone = 426, /* server_client_subnet_zone  */
  YYSYMBOL_server_client_subnet_always_forward = 427, /* server_client_subnet_always_forward  */
  YYSYMBOL_server_client_subnet_opcode = 428, /* server_client_subnet_opcode  */
  YYSYMBOL_server_max_client_subnet_ipv4 = 429, /* server_max_client_subnet_ipv4  */
  YYSYMBOL_server_max_client_subnet_ipv6 = 430, /* server_max_client_subnet_ipv6  */
  YYSYMBOL_server_min_client_subnet_ipv4 = 431, /* server_min_client_subnet_ipv4  */
  YYSYMBOL_server_min_client_subnet_ipv6 = 432, /* server_min_client_subnet_ipv6  */
  YYSYMBOL_server_max_ecs_tree_size_ipv4 = 433, /* server_max_ecs_tree_size_ipv4  */
  YYSYMBOL_server_max_ecs_tree_size_ipv6 = 434, /* server_max_ecs_tree_size_ipv6  */
  YYSYMBOL_server_interface = 435,         /* server_interface  */
  YYSYMBOL_server_outgoing_interface = 436, /* server_outgoing_interface  */
  YYSYMBOL_server_outgoing_range = 437,    /* server_outgoing_range  */
  YYSYMBOL_server_outgoing_port_permit = 438, /* server_outgoing_port_permit  */
  YYSYMBOL_server_outgoing_port_avoid = 439, /* server_outgoing_port_avoid  */
  YYSYMBOL_server_outgoing_num_tcp = 440,  /* server_outgoing_num_tcp  */
  YYSYMBOL_server_incoming_num_tcp = 441,  /* server_incoming_num_tcp  */
  YYSYMBOL_server_interface_automatic = 442, /* server_interface_automatic  */
  YYSYMBOL_server_interface_automatic_ports = 443, /* server_interface_automatic_ports  */
  YYSYMBOL_server_do_ip4 = 444,            /* server_do_ip4  */
  YYSYMBOL_server_do_ip6 = 445,            /* server_do_ip6  */
  YYSYMBOL_server_do_nat64 = 446,          /* server_do_nat64  */
  YYSYMBOL_server_do_udp = 447,            /* server_do_udp  */
  YYSYMBOL_server_do_tcp = 448,            /* server_do_tcp  */
  YYSYMBOL_server_prefer_ip4 = 449,        /* server_prefer_ip4  */
  YYSYMBOL_server_prefer_ip6 = 450,        /* server_prefer_ip6  */
  YYSYMBOL_server_tcp_mss = 451,           /* server_tcp_mss  */
  YYSYMBOL_server_outgoing_tcp_mss = 452,  /* server_outgoing_tcp_mss  */
  YYSYMBOL_server_tcp_idle_timeout = 453,  /* server_tcp_idle_timeout  */
  YYSYMBOL_server_max_reuse_tcp_queries = 454, /* server_max_reuse_tcp_queries  */
  YYSYMBOL_server_tcp_reuse_timeout = 455, /* server_tcp_reuse_timeout  */
  YYSYMBOL_server_tcp_auth_query_timeout = 456, /* server_tcp_auth_query_timeout  */
  YYSYMBOL_server_tcp_keepalive = 457,     /* server_tcp_keepalive  */
  YYSYMBOL_server_tcp_keepalive_timeout = 458, /* server_tcp_keepalive_timeout  */
  YYSYMBOL_server_sock_queue_timeout = 459, /* server_sock_queue_timeout  */
  YYSYMBOL_server_tcp_upstream = 460,      /* server_tcp_upstream  */
  YYSYMBOL_server_udp_upstream_without_downstream = 461, /* server_udp_upstream_without_downstream  */
  YYSYMBOL_server_ssl_upstream = 462,      /* server_ssl_upstream  */
  YYSYMBOL_server_ssl_service_key = 463,   /* server_ssl_service_key  */
  YYSYMBOL_server_ssl_service_pem = 464,   /* server_ssl_service_pem  */
  YYSYMBOL_server_ssl_port = 465,          /* server_ssl_port  */
  YYSYMBOL_server_tls_cert_bundle = 466,   /* server_tls_cert_bundle  */
  YYSYMBOL_server_tls_win_cert = 467,      /* server_tls_win_cert  */
  YYSYMBOL_server_tls_additional_port = 468, /* server_tls_additional_port  */
  YYSYMBOL_server_tls_ciphers = 469,       /* server_tls_ciphers  */
  YYSYMBOL_server_tls_ciphersuites = 470,  /* server_tls_ciphersuites  */
  YYSYMBOL_server_tls_session_ticket_keys = 471, /* server_tls_session_ticket_keys  */
  YYSYMBOL_server_tls_use_sni = 472,       /* server_tls_use_sni  */
  YYSYMBOL_server_tls_protocols = 473,     /* server_tls_protocols  */
  YYSYMBOL_server_https_port = 474,        /* server_https_port  */
  YYSYMBOL_server_http_endpoint = 475,     /* server_http_endpoint  */
  YYSYMBOL_server_http_max_streams = 476,  /* server_http_max_streams  */
  YYSYMBOL_server_http_query_buffer_size = 477, /* server_http_query_buffer_size  */
  YYSYMBOL_server_http_response_buffer_size = 478, /* server_http_response_buffer_size  */
  YYSYMBOL_server_http_nodelay = 479,      /* server_http_nodelay  */
  YYSYMBOL_server_http_notls_downstream = 480, /* server_http_notls_downstream  */
  YYSYMBOL_server_quic_port = 481,         /* server_quic_port  */
  YYSYMBOL_server_quic_size = 482,         /* server_quic_size  */
  YYSYMBOL_server_use_systemd = 483,       /* server_use_systemd  */
  YYSYMBOL_server_do_daemonize = 484,      /* server_do_daemonize  */
  YYSYMBOL_server_use_syslog = 485,        /* server_use_syslog  */
  YYSYMBOL_server_log_time_ascii = 486,    /* server_log_time_ascii  */
  YYSYMBOL_server_log_time_iso = 487,      /* server_log_time_iso  */
  YYSYMBOL_server_log_queries = 488,       /* server_log_queries  */
  YYSYMBOL_server_log_replies = 489,       /* server_log_replies  */
  YYSYMBOL_server_log_tag_queryreply = 490, /* server_log_tag_queryreply  */
  YYSYMBOL_server_log_servfail = 491,      /* server_log_servfail  */
  YYSYMBOL_server_log_destaddr = 492,      /* server_log_destaddr  */
  YYSYMBOL_server_log_thread_id = 493,     /* server_log_thread_id  */
  YYSYMBOL_server_log_local_actions = 494, /* server_log_local_actions  */
  YYSYMBOL_server_chroot = 495,            /* server_chroot  */
  YYSYMBOL_server_username = 496,          /* server_username  */
  YYSYMBOL_server_directory = 497,         /* server_directory  */
  YYSYMBOL_server_logfile = 498,           /* server_logfile  */
  YYSYMBOL_server_pidfile = 499,           /* server_pidfile  */
  YYSYMBOL_server_root_hints = 500,        /* server_root_hints  */
  YYSYMBOL_server_dlv_anchor_file = 501,   /* server_dlv_anchor_file  */
  YYSYMBOL_server_dlv_anchor = 502,        /* server_dlv_anchor  */
  YYSYMBOL_server_auto_trust_anchor_file = 503, /* server_auto_trust_anchor_file  */
  YYSYMBOL_server_trust_anchor_file = 504, /* server_trust_anchor_file  */
  YYSYMBOL_server_trusted_keys_file = 505, /* server_trusted_keys_file  */
  YYSYMBOL_server_trust_anchor = 506,      /* server_trust_anchor  */
  YYSYMBOL_server_trust_anchor_signaling = 507, /* server_trust_anchor_signaling  */
  YYSYMBOL_server_root_key_sentinel = 508, /* server_root_key_sentinel  */
  YYSYMBOL_server_domain_insecure = 509,   /* server_domain_insecure  */
  YYSYMBOL_server_hide_identity = 510,     /* server_hide_identity  */
  YYSYMBOL_server_hide_version = 511,      /* server_hide_version  */
  YYSYMBOL_server_hide_trustanchor = 512,  /* server_hide_trustanchor  */
  YYSYMBOL_server_hide_http_user_agent = 513, /* server_hide_http_user_agent  */
  YYSYMBOL_server_identity = 514,          /* server_identity  */
  YYSYMBOL_server_version = 515,           /* server_version  */
  YYSYMBOL_server_http_user_agent = 516,   /* server_http_user_agent  */
  YYSYMBOL_server_nsid = 517,              /* server_nsid  */
  YYSYMBOL_server_so_rcvbuf = 518,         /* server_so_rcvbuf  */
  YYSYMBOL_server_so_sndbuf = 519,         /* server_so_sndbuf  */
  YYSYMBOL_server_so_reuseport = 520,      /* server_so_reuseport  */
  YYSYMBOL_server_ip_transparent = 521,    /* server_ip_transparent  */
  YYSYMBOL_server_ip_freebind = 522,       /* server_ip_freebind  */
  YYSYMBOL_server_ip_dscp = 523,           /* server_ip_dscp  */
  YYSYMBOL_server_stream_wait_size = 524,  /* server_stream_wait_size  */
  YYSYMBOL_server_edns_buffer_size = 525,  /* server_edns_buffer_size  */
  YYSYMBOL_server_msg_buffer_size = 526,   /* server_msg_buffer_size  */
  YYSYMBOL_server_msg_cache_size = 527,    /* server_msg_cache_size  */
  YYSYMBOL_server_msg_cache_slabs = 528,   /* server_msg_cache_slabs  */
  YYSYMBOL_server_num_queries_per_thread = 529, /* server_num_queries_per_thread  */
  YYSYMBOL_server_jostle_timeout = 530,    /* server_jostle_timeout  */
  YYSYMBOL_server_delay_close = 531,       /* server_delay_close  */
  YYSYMBOL_server_udp_connect = 532,       /* server_udp_connect  */
  YYSYMBOL_server_unblock_lan_zones = 533, /* server_unblock_lan_zones  */
  YYSYMBOL_server_insecure_lan_zones = 534, /* server_insecure_lan_zones  */
  YYSYMBOL_server_rrset_cache_size = 535,  /* server_rrset_cache_size  */
  YYSYMBOL_server_rrset_cache_slabs = 536, /* server_rrset_cache_slabs  */
  YYSYMBOL_server_infra_host_ttl = 537,    /* server_infra_host_ttl  */
  YYSYMBOL_server_infra_lame_ttl = 538,    /* server_infra_lame_ttl  */
  YYSYMBOL_server_infra_cache_numhosts = 539, /* server_infra_cache_numhosts  */
  YYSYMBOL_server_infra_cache_lame_size = 540, /* server_infra_cache_lame_size  */
  YYSYMBOL_server_infra_cache_slabs = 541, /* server_infra_cache_slabs  */
  YYSYMBOL_server_infra_cache_min_rtt = 542, /* server_infra_cache_min_rtt  */
  YYSYMBOL_server_infra_cache_max_rtt = 543, /* server_infra_cache_max_rtt  */
  YYSYMBOL_server_infra_keep_probing = 544, /* server_infra_keep_probing  */
  YYSYMBOL_server_target_fetch_policy = 545, /* server_target_fetch_policy  */
  YYSYMBOL_server_harden_short_bufsize = 546, /* server_harden_short_bufsize  */
  YYSYMBOL_server_harden_large_queries = 547, /* server_harden_large_queries  */
  YYSYMBOL_server_harden_glue = 548,       /* server_harden_glue  */
  YYSYMBOL_server_harden_unverified_glue = 549, /* server_harden_unverified_glue  */
  YYSYMBOL_server_harden_dnssec_stripped = 550, /* server_harden_dnssec_stripped  */
  YYSYMBOL_server_harden_below_nxdomain = 551, /* server_harden_below_nxdomain  */
  YYSYMBOL_server_harden_referral_path = 552, /* server_harden_referral_path  */
  YYSYMBOL_server_harden_algo_downgrade = 553, /* server_harden_algo_downgrade  */
  YYSYMBOL_server_harden_unknown_additional = 554, /* server_harden_unknown_additional  */
  YYSYMBOL_server_use_caps_for_id = 555,   /* server_use_caps_for_id  */
  YYSYMBOL_server_caps_whitelist = 556,    /* server_caps_whitelist  */
  YYSYMBOL_server_private_address = 557,   /* server_private_address  */
  YYSYMBOL_server_private_domain = 558,    /* server_private_domain  */
  YYSYMBOL_server_prefetch = 559,          /* server_prefetch  */
  YYSYMBOL_server_prefetch_key = 560,      /* server_prefetch_key  */
  YYSYMBOL_server_deny_any = 561,          /* server_deny_any  */
  YYSYMBOL_server_unwanted_reply_threshold = 562, /* server_unwanted_reply_threshold  */
  YYSYMBOL_server_do_not_query_address = 563, /* server_do_not_query_address  */
  YYSYMBOL_server_do_not_query_localhost = 564, /* server_do_not_query_localhost  */
  YYSYMBOL_server_access_control = 565,    /* server_access_control  */
  YYSYMBOL_server_interface_action = 566,  /* server_interface_action  */
  YYSYMBOL_server_module_conf = 567,       /* server_module_conf  */
  YYSYMBOL_server_val_override_date = 568, /* server_val_override_date  */
  YYSYMBOL_server_val_sig_skew_min = 569,  /* server_val_sig_skew_min  */
  YYSYMBOL_server_val_sig_skew_max = 570,  /* server_val_sig_skew_max  */
  YYSYMBOL_server_val_max_restart = 571,   /* server_val_max_restart  */
  YYSYMBOL_server_cache_max_ttl = 572,     /* server_cache_max_ttl  */
  YYSYMBOL_server_cache_max_negative_ttl = 573, /* server_cache_max_negative_ttl  */
  YYSYMBOL_server_cache_min_negative_ttl = 574, /* server_cache_min_negative_ttl  */
  YYSYMBOL_server_cache_min_ttl = 575,     /* server_cache_min_ttl  */
  YYSYMBOL_server_bogus_ttl = 576,         /* server_bogus_ttl  */
  YYSYMBOL_server_val_clean_additional = 577, /* server_val_clean_additional  */
  YYSYMBOL_server_val_permissive_mode = 578, /* server_val_permissive_mode  */
  YYSYMBOL_server_aggressive_nsec = 579,   /* server_aggressive_nsec  */
  YYSYMBOL_server_ignore_cd_flag = 580,    /* server_ignore_cd_flag  */
  YYSYMBOL_server_disable_edns_do = 581,   /* server_disable_edns_do  */
  YYSYMBOL_server_serve_expired = 582,     /* server_serve_expired  */
  YYSYMBOL_server_serve_expired_ttl = 583, /* server_serve_expired_ttl  */
  YYSYMBOL_server_serve_expired_ttl_reset = 584, /* server_serve_expired_ttl_reset  */
  YYSYMBOL_server_serve_expired_reply_ttl = 585, /* server_serve_expired_reply_ttl  */
  YYSYMBOL_server_serve_expired_client_timeout = 586, /* server_serve_expired_client_timeout  */
  YYSYMBOL_server_ede_serve_expired = 587, /* server_ede_serve_expired  */
  YYSYMBOL_server_serve_original_ttl = 588, /* server_serve_original_ttl  */
  YYSYMBOL_server_fake_dsa = 589,          /* server_fake_dsa  */
  YYSYMBOL_server_fake_sha1 = 590,         /* server_fake_sha1  */
  YYSYMBOL_server_val_log_level = 591,     /* server_val_log_level  */
  YYSYMBOL_server_val_nsec3_keysize_iterations = 592, /* server_val_nsec3_keysize_iterations  */
  YYSYMBOL_server_zonemd_permissive_mode = 593, /* server_zonemd_permissive_mode  */
  YYSYMBOL_server_add_holddown = 594,      /* server_add_holddown  */
  YYSYMBOL_server_del_holddown = 595,      /* server_del_holddown  */
  YYSYMBOL_server_keep_missing = 596,      /* server_keep_missing  */
  YYSYMBOL_server_permit_small_holddown = 597, /* server_permit_small_holddown  */
  YYSYMBOL_server_key_cache_size = 598,    /* server_key_cache_size  */
  YYSYMBOL_server_key_cache_slabs = 599,   /* server_key_cache_slabs  */
  YYSYMBOL_server_neg_cache_size = 600,    /* server_neg_cache_size  */
  YYSYMBOL_server_local_zone = 601,        /* server_local_zone  */
  YYSYMBOL_server_local_data = 602,        /* server_local_data  */
  YYSYMBOL_server_local_data_ptr = 603,    /* server_local_data_ptr  */
  YYSYMBOL_server_minimal_responses = 604, /* server_minimal_responses  */
  YYSYMBOL_server_rrset_roundrobin = 605,  /* server_rrset_roundrobin  */
  YYSYMBOL_server_unknown_server_time_limit = 606, /* server_unknown_server_time_limit  */
  YYSYMBOL_server_discard_timeout = 607,   /* server_discard_timeout  */
  YYSYMBOL_server_wait_limit = 608,        /* server_wait_limit  */
  YYSYMBOL_server_wait_limit_cookie = 609, /* server_wait_limit_cookie  */
  YYSYMBOL_server_wait_limit_netblock = 610, /* server_wait_limit_netblock  */
  YYSYMBOL_server_wait_limit_cookie_netblock = 611, /* server_wait_limit_cookie_netblock  */
  YYSYMBOL_server_max_udp_size = 612,      /* server_max_udp_size  */
  YYSYMBOL_server_dns64_prefix = 613,      /* server_dns64_prefix  */
  YYSYMBOL_server_dns64_synthall = 614,    /* server_dns64_synthall  */
  YYSYMBOL_server_dns64_ignore_aaaa = 615, /* server_dns64_ignore_aaaa  */
  YYSYMBOL_server_nat64_prefix = 616,      /* server_nat64_prefix  */
  YYSYMBOL_server_define_tag = 617,        /* server_define_tag  */
  YYSYMBOL_server_local_zone_tag = 618,    /* server_local_zone_tag  */
  YYSYMBOL_server_access_control_tag = 619, /* server_access_control_tag  */
  YYSYMBOL_server_access_control_tag_action = 620, /* server_access_control_tag_action  */
  YYSYMBOL_server_access_control_tag_data = 621, /* server_access_control_tag_data  */
  YYSYMBOL_server_local_zone_override = 622, /* server_local_zone_override  */
  YYSYMBOL_server_access_control_view = 623, /* server_access_control_view  */
  YYSYMBOL_server_interface_tag = 624,     /* server_interface_tag  */
  YYSYMBOL_server_interface_tag_action = 625, /* server_interface_tag_action  */
  YYSYMBOL_server_interface_tag_data = 626, /* server_interface_tag_data  */
  YYSYMBOL_server_interface_view = 627,    /* server_interface_view  */
  YYSYMBOL_server_response_ip_tag = 628,   /* server_response_ip_tag  */
  YYSYMBOL_server_ip_ratelimit = 629,      /* server_ip_ratelimit  */
  YYSYMBOL_server_ip_ratelimit_cookie = 630, /* server_ip_ratelimit_cookie  */
  YYSYMBOL_server_ratelimit = 631,         /* server_ratelimit  */
  YYSYMBOL_server_ip_ratelimit_size = 632, /* server_ip_ratelimit_size  */
  YYSYMBOL_server_ratelimit_size = 633,    /* server_ratelimit_size  */
  YYSYMBOL_server_ip_ratelimit_slabs = 634, /* server_ip_ratelimit_slabs  */
  YYSYMBOL_server_ratelimit_slabs = 635,   /* server_ratelimit_slabs  */
  YYSYMBOL_server_ratelimit_for_domain = 636, /* server_ratelimit_for_domain  */
  YYSYMBOL_server_ratelimit_below_domain = 637, /* server_ratelimit_below_domain  */
  YYSYMBOL_server_ip_ratelimit_factor = 638, /* server_ip_ratelimit_factor  */
  YYSYMBOL_server_ratelimit_factor = 639,  /* server_ratelimit_factor  */
  YYSYMBOL_server_ip_ratelimit_backoff = 640, /* server_ip_ratelimit_backoff  */
  YYSYMBOL_server_ratelimit_backoff = 641, /* server_ratelimit_backoff  */
  YYSYMBOL_server_outbound_msg_retry = 642, /* server_outbound_msg_retry  */
  YYSYMBOL_server_max_sent_count = 643,    /* server_max_sent_count  */
  YYSYMBOL_server_max_query_restarts = 644, /* server_max_query_restarts  */
  YYSYMBOL_server_low_rtt = 645,           /* server_low_rtt  */
  YYSYMBOL_server_fast_server_num = 646,   /* server_fast_server_num  */
  YYSYMBOL_server_fast_server_permil = 647, /* server_fast_server_permil  */
  YYSYMBOL_server_qname_minimisation = 648, /* server_qname_minimisation  */
  YYSYMBOL_server_qname_minimisation_strict = 649, /* server_qname_minimisation_strict  */
  YYSYMBOL_server_pad_responses = 650,     /* server_pad_responses  */
  YYSYMBOL_server_pad_responses_block_size = 651, /* server_pad_responses_block_size  */
  YYSYMBOL_server_pad_queries = 652,       /* server_pad_queries  */
  YYSYMBOL_server_pad_queries_block_size = 653, /* server_pad_queries_block_size  */
  YYSYMBOL_server_ipsecmod_enabled = 654,  /* server_ipsecmod_enabled  */
  YYSYMBOL_server_ipsecmod_ignore_bogus = 655, /* server_ipsecmod_ignore_bogus  */
  YYSYMBOL_server_ipsecmod_hook = 656,     /* server_ipsecmod_hook  */
  YYSYMBOL_server_ipsecmod_max_ttl = 657,  /* server_ipsecmod_max_ttl  */
  YYSYMBOL_server_ipsecmod_whitelist = 658, /* server_ipsecmod_whitelist  */
  YYSYMBOL_server_ipsecmod_strict = 659,   /* server_ipsecmod_strict  */
  YYSYMBOL_server_edns_client_string = 660, /* server_edns_client_string  */
  YYSYMBOL_server_edns_client_string_opcode = 661, /* server_edns_client_string_opcode  */
  YYSYMBOL_server_ede = 662,               /* server_ede  */
  YYSYMBOL_server_dns_error_reporting = 663, /* server_dns_error_reporting  */
  YYSYMBOL_server_proxy_protocol_port = 664, /* server_proxy_protocol_port  */
  YYSYMBOL_stub_name = 665,                /* stub_name  */
  YYSYMBOL_stub_host = 666,                /* stub_host  */
  YYSYMBOL_stub_addr = 667,                /* stub_addr  */
  YYSYMBOL_stub_first = 668,               /* stub_first  */
  YYSYMBOL_stub_no_cache = 669,            /* stub_no_cache  */
  YYSYMBOL_stub_ssl_upstream = 670,        /* stub_ssl_upstream  */
  YYSYMBOL_stub_tcp_upstream = 671,        /* stub_tcp_upstream  */
  YYSYMBOL_stub_prime = 672,               /* stub_prime  */
  YYSYMBOL_forward_name = 673,             /* forward_name  */
  YYSYMBOL_forward_host = 674,             /* forward_host  */
  YYSYMBOL_forward_addr = 675,             /* forward_addr  */
  YYSYMBOL_forward_first = 676,            /* forward_first  */
  YYSYMBOL_forward_no_cache = 677,         /* forward_no_cache  */
  YYSYMBOL_forward_ssl_upstream = 678,     /* forward_ssl_upstream  */
  YYSYMBOL_forward_tcp_upstream = 679,     /* forward_tcp_upstream  */
  YYSYMBOL_auth_name = 680,                /* auth_name  */
  YYSYMBOL_auth_zonefile = 681,            /* auth_zonefile  */
  YYSYMBOL_auth_master = 682,              /* auth_master  */
  YYSYMBOL_auth_url = 683,                 /* auth_url  */
  YYSYMBOL_auth_allow_notify = 684,        /* auth_allow_notify  */
  YYSYMBOL_auth_zonemd_check = 685,        /* auth_zonemd_check  */
  YYSYMBOL_auth_zonemd_reject_absence = 686, /* auth_zonemd_reject_absence  */
  YYSYMBOL_auth_for_downstream = 687,      /* auth_for_downstream  */
  YYSYMBOL_auth_for_upstream = 688,        /* auth_for_upstream  */
  YYSYMBOL_auth_fallback_enabled = 689,    /* auth_fallback_enabled  */
  YYSYMBOL_auth_max_transfer_size = 690,   /* auth_max_transfer_size  */
  YYSYMBOL_auth_max_transfer_time = 691,   /* auth_max_transfer_time  */
  YYSYMBOL_view_name = 692,                /* view_name  */
  YYSYMBOL_view_local_zone = 693,          /* view_local_zone  */
  YYSYMBOL_view_response_ip = 694,         /* view_response_ip  */
  YYSYMBOL_view_response_ip_data = 695,    /* view_response_ip_data  */
  YYSYMBOL_view_local_data = 696,          /* view_local_data  */
  YYSYMBOL_view_local_data_ptr = 697,      /* view_local_data_ptr  */
  YYSYMBOL_view_first = 698,               /* view_first  */
  YYSYMBOL_rcstart = 699,                  /* rcstart  */
  YYSYMBOL_contents_rc = 700,              /* contents_rc  */
  YYSYMBOL_content_rc = 701,               /* content_rc  */
  YYSYMBOL_rc_control_enable = 702,        /* rc_control_enable  */
  YYSYMBOL_rc_control_port = 703,          /* rc_control_port  */
  YYSYMBOL_rc_control_interface = 704,     /* rc_control_interface  */
  YYSYMBOL_rc_control_use_cert = 705,      /* rc_control_use_cert  */
  YYSYMBOL_rc_server_key_file = 706,       /* rc_server_key_file  */
  YYSYMBOL_rc_server_cert_file = 707,      /* rc_server_cert_file  */
  YYSYMBOL_rc_control_key_file = 708,      /* rc_control_key_file  */
  YYSYMBOL_rc_control_cert_file = 709,     /* rc_control_cert_file  */
  YYSYMBOL_dtstart = 710,                  /* dtstart  */
  YYSYMBOL_contents_dt = 711,              /* contents_dt  */
  YYSYMBOL_content_dt = 712,               /* content_dt  */
  YYSYMBOL_dt_dnstap_enable = 713,         /* dt_dnstap_enable  */
  YYSYMBOL_dt_dnstap_bidirectional = 714,  /* dt_dnstap_bidirectional  */
  YYSYMBOL_dt_dnstap_socket_path = 715,    /* dt_dnstap_socket_path  */
  YYSYMBOL_dt_dnstap_ip = 716,             /* dt_dnstap_ip  */
  YYSYMBOL_dt_dnstap_tls = 717,            /* dt_dnstap_tls  */
  YYSYMBOL_dt_dnstap_tls_server_name = 718, /* dt_dnstap_tls_server_name  */
  YYSYMBOL_dt_dnstap_tls_cert_bundle = 719, /* dt_dnstap_tls_cert_bundle  */
  YYSYMBOL_dt_dnstap_tls_client_key_file = 720, /* dt_dnstap_tls_client_key_file  */
  YYSYMBOL_dt_dnstap_tls_client_cert_file = 721, /* dt_dnstap_tls_client_cert_file  */
  YYSYMBOL_dt_dnstap_send_identity = 722,  /* dt_dnstap_send_identity  */
  YYSYMBOL_dt_dnstap_send_version = 723,   /* dt_dnstap_send_version  */
  YYSYMBOL_dt_dnstap_identity = 724,       /* dt_dnstap_identity  */
  YYSYMBOL_dt_dnstap_version = 725,        /* dt_dnstap_version  */
  YYSYMBOL_dt_dnstap_log_resolver_query_messages = 726, /* dt_dnstap_log_resolver_query_messages  */
  YYSYMBOL_dt_dnstap_log_resolver_response_messages = 727, /* dt_dnstap_log_resolver_response_messages  */
  YYSYMBOL_dt_dnstap_log_client_query_messages = 728, /* dt_dnstap_log_client_query_messages  */
  YYSYMBOL_dt_dnstap_log_client_response_messages = 729, /* dt_dnstap_log_client_response_messages  */
  YYSYMBOL_dt_dnstap_log_forwarder_query_messages = 730, /* dt_dnstap_log_forwarder_query_messages  */
  YYSYMBOL_dt_dnstap_log_forwarder_response_messages = 731, /* dt_dnstap_log_forwarder_response_messages  */
  YYSYMBOL_dt_dnstap_sample_rate = 732,    /* dt_dnstap_sample_rate  */
  YYSYMBOL_pythonstart = 733,              /* pythonstart  */
  YYSYMBOL_contents_py = 734,              /* contents_py  */
  YYSYMBOL_content_py = 735,               /* content_py  */
  YYSYMBOL_py_script = 736,                /* py_script  */
  YYSYMBOL_dynlibstart = 737,              /* dynlibstart  */
  YYSYMBOL_contents_dl = 738,              /* contents_dl  */
  YYSYMBOL_content_dl = 739,               /* content_dl  */
  YYSYMBOL_dl_file = 740,                  /* dl_file  */
  YYSYMBOL_server_disable_dnssec_lame_check = 741, /* server_disable_dnssec_lame_check  */
  YYSYMBOL_server_log_identity = 742,      /* server_log_identity  */
  YYSYMBOL_server_response_ip = 743,       /* server_response_ip  */
  YYSYMBOL_server_response_ip_data = 744,  /* server_response_ip_data  */
  YYSYMBOL_dnscstart = 745,                /* dnscstart  */
  YYSYMBOL_contents_dnsc = 746,            /* contents_dnsc  */
  YYSYMBOL_content_dnsc = 747,             /* content_dnsc  */
  YYSYMBOL_dnsc_dnscrypt_enable = 748,     /* dnsc_dnscrypt_enable  */
  YYSYMBOL_dnsc_dnscrypt_port = 749,       /* dnsc_dnscrypt_port  */
  YYSYMBOL_dnsc_dnscrypt_provider = 750,   /* dnsc_dnscrypt_provider  */
  YYSYMBOL_dnsc_dnscrypt_provider_cert = 751, /* dnsc_dnscrypt_provider_cert  */
  YYSYMBOL_dnsc_dnscrypt_provider_cert_rotated = 752, /* dnsc_dnscrypt_provider_cert_rotated  */
  YYSYMBOL_dnsc_dnscrypt_secret_key = 753, /* dnsc_dnscrypt_secret_key  */
  YYSYMBOL_dnsc_dnscrypt_shared_secret_cache_size = 754, /* dnsc_dnscrypt_shared_secret_cache_size  */
  YYSYMBOL_dnsc_dnscrypt_shared_secret_cache_slabs = 755, /* dnsc_dnscrypt_shared_secret_cache_slabs  */
  YYSYMBOL_dnsc_dnscrypt_nonce_cache_size = 756, /* dnsc_dnscrypt_nonce_cache_size  */
  YYSYMBOL_dnsc_dnscrypt_nonce_cache_slabs = 757, /* dnsc_dnscrypt_nonce_cache_slabs  */
  YYSYMBOL_cachedbstart = 758,             /* cachedbstart  */
  YYSYMBOL_contents_cachedb = 759,         /* contents_cachedb  */
  YYSYMBOL_content_cachedb = 760,          /* content_cachedb  */
  YYSYMBOL_cachedb_backend_name = 761,     /* cachedb_backend_name  */
  YYSYMBOL_cachedb_secret_seed = 762,      /* cachedb_secret_seed  */
  YYSYMBOL_cachedb_no_store = 763,         /* cachedb_no_store  */
  YYSYMBOL_cachedb_check_when_serve_expired = 764, /* cachedb_check_when_serve_expired  */
  YYSYMBOL_redis_server_host = 765,        /* redis_server_host  */
  YYSYMBOL_redis_replica_server_host = 766, /* redis_replica_server_host  */
  YYSYMBOL_redis_server_port = 767,        /* redis_server_port  */
  YYSYMBOL_redis_replica_server_port = 768, /* redis_replica_server_port  */
  YYSYMBOL_redis_server_path = 769,        /* redis_server_path  */
  YYSYMBOL_redis_replica_server_path = 770, /* redis_replica_server_path  */
  YYSYMBOL_redis_server_password = 771,    /* redis_server_password  */
  YYSYMBOL_redis_replica_server_password = 772, /* redis_replica_server_password  */
  YYSYMBOL_redis_timeout = 773,            /* redis_timeout  */
  YYSYMBOL_redis_replica_timeout = 774,    /* redis_replica_timeout  */
  YYSYMBOL_redis_command_timeout = 775,    /* redis_command_timeout  */
  YYSYMBOL_redis_replica_command_timeout = 776, /* redis_replica_command_timeout  */
  YYSYMBOL_redis_connect_timeout = 777,    /* redis_connect_timeout  */
  YYSYMBOL_redis_replica_connect_timeout = 778, /* redis_replica_connect_timeout  */
  YYSYMBOL_redis_expire_records = 779,     /* redis_expire_records  */
  YYSYMBOL_redis_logical_db = 780,         /* redis_logical_db  */
  YYSYMBOL_redis_replica_logical_db = 781, /* redis_replica_logical_db  */
  YYSYMBOL_server_tcp_connection_limit = 782, /* server_tcp_connection_limit  */
  YYSYMBOL_server_answer_cookie = 783,     /* server_answer_cookie  */
  YYSYMBOL_server_cookie_secret = 784,     /* server_cookie_secret  */
  YYSYMBOL_server_cookie_secret_file = 785, /* server_cookie_secret_file  */
  YYSYMBOL_server_iter_scrub_ns = 786,     /* server_iter_scrub_ns  */
  YYSYMBOL_server_iter_scrub_cname = 787,  /* server_iter_scrub_cname  */
  YYSYMBOL_server_iter_scrub_rrsig = 788,  /* server_iter_scrub_rrsig  */
  YYSYMBOL_server_max_global_quota = 789,  /* server_max_global_quota  */
  YYSYMBOL_server_iter_scrub_promiscuous = 790, /* server_iter_scrub_promiscuous  */
  YYSYMBOL_server_val_validation_attempts = 791, /* server_val_validation_attempts  */
  YYSYMBOL_server_val_hash_attempts = 792, /* server_val_hash_attempts  */
  YYSYMBOL_ipsetstart = 793,               /* ipsetstart  */
  YYSYMBOL_contents_ipset = 794,           /* contents_ipset  */
  YYSYMBOL_content_ipset = 795,            /* content_ipset  */
  YYSYMBOL_ipset_name_v4 = 796,            /* ipset_name_v4  */
  YYSYMBOL_ipset_name_v6 = 797             /* ipset_name_v6  */
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

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
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
#define YYLAST   825

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  385
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  413
/* YYNRULES -- Number of rules.  */
#define YYNRULES  802
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  1200

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   639


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
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381,   382,   383,   384
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   225,   225,   225,   226,   226,   227,   227,   228,   228,
     228,   229,   229,   230,   230,   231,   231,   232,   234,   241,
     247,   248,   249,   249,   249,   250,   250,   251,   251,   251,
     252,   252,   252,   253,   253,   253,   254,   254,   255,   256,
     256,   256,   257,   257,   257,   258,   258,   259,   259,   260,
     260,   261,   261,   262,   262,   263,   263,   264,   264,   265,
     265,   266,   266,   266,   267,   267,   268,   268,   268,   269,
     269,   269,   270,   270,   271,   271,   272,   272,   273,   273,
     274,   274,   274,   275,   275,   276,   276,   277,   277,   277,
     278,   278,   279,   279,   280,   280,   281,   281,   281,   282,
     282,   283,   283,   284,   284,   285,   285,   286,   286,   287,
     287,   288,   288,   289,   289,   290,   290,   290,   291,   291,
     291,   292,   292,   292,   293,   293,   293,   293,   294,   294,
     295,   295,   295,   296,   296,   296,   297,   297,   298,   298,
     299,   299,   299,   300,   300,   300,   301,   301,   302,   302,
     302,   303,   304,   304,   304,   305,   305,   305,   306,   306,
     307,   307,   308,   308,   309,   310,   310,   311,   311,   312,
     312,   313,   313,   314,   314,   315,   315,   316,   316,   317,
     317,   318,   318,   319,   319,   320,   321,   321,   322,   322,
     322,   323,   323,   324,   324,   325,   325,   326,   326,   326,
     327,   327,   328,   329,   329,   330,   330,   331,   332,   332,
     333,   333,   334,   334,   334,   335,   335,   336,   336,   336,
     337,   337,   337,   338,   338,   339,   340,   340,   341,   341,
     342,   342,   343,   343,   344,   344,   344,   345,   345,   345,
     346,   346,   346,   347,   347,   348,   348,   348,   349,   349,
     350,   350,   351,   351,   352,   352,   352,   353,   353,   353,
     354,   354,   355,   355,   356,   356,   357,   357,   358,   358,
     359,   360,   360,   361,   361,   362,   362,   363,   363,   363,
     364,   365,   365,   366,   366,   366,   368,   376,   390,   391,
     392,   392,   392,   392,   392,   393,   393,   393,   395,   403,
     417,   418,   419,   419,   419,   419,   420,   420,   420,   422,
     430,   444,   445,   446,   446,   446,   446,   447,   447,   447,
     449,   472,   473,   474,   474,   474,   474,   475,   475,   475,
     476,   476,   476,   477,   477,   480,   499,   516,   524,   534,
     541,   551,   572,   573,   574,   574,   574,   574,   574,   575,
     575,   575,   576,   576,   576,   576,   577,   577,   579,   588,
     597,   608,   617,   626,   635,   644,   655,   664,   676,   690,
     705,   716,   733,   750,   767,   784,   799,   814,   827,   842,
     851,   860,   869,   878,   887,   896,   903,   912,   921,   930,
     939,   948,   957,   966,   975,   984,   997,  1008,  1019,  1030,
    1039,  1052,  1065,  1074,  1083,  1092,  1099,  1106,  1115,  1122,
    1131,  1139,  1146,  1153,  1161,  1170,  1179,  1187,  1205,  1213,
    1221,  1229,  1237,  1245,  1261,  1268,  1277,  1286,  1300,  1309,
    1318,  1327,  1336,  1345,  1354,  1363,  1372,  1381,  1388,  1395,
    1421,  1429,  1436,  1443,  1450,  1457,  1465,  1473,  1481,  1488,
    1499,  1510,  1517,  1526,  1535,  1544,  1553,  1560,  1567,  1574,
    1590,  1598,  1606,  1616,  1626,  1636,  1650,  1658,  1671,  1682,
    1690,  1703,  1712,  1721,  1730,  1739,  1749,  1759,  1767,  1780,
    1789,  1797,  1806,  1814,  1827,  1836,  1845,  1855,  1862,  1872,
    1882,  1892,  1902,  1912,  1922,  1932,  1942,  1952,  1962,  1969,
    1976,  1983,  1992,  2001,  2010,  2019,  2026,  2036,  2044,  2053,
    2060,  2078,  2091,  2104,  2117,  2126,  2135,  2144,  2153,  2162,
    2172,  2182,  2193,  2202,  2211,  2220,  2229,  2238,  2247,  2256,
    2265,  2274,  2287,  2300,  2309,  2316,  2325,  2334,  2343,  2352,
    2362,  2370,  2383,  2391,  2455,  2462,  2477,  2487,  2497,  2504,
    2511,  2518,  2525,  2539,  2553,  2560,  2567,  2576,  2584,  2591,
    2605,  2626,  2647,  2659,  2671,  2683,  2692,  2713,  2725,  2737,
    2746,  2767,  2776,  2785,  2794,  2802,  2810,  2823,  2836,  2850,
    2864,  2873,  2882,  2892,  2902,  2911,  2920,  2929,  2935,  2944,
    2953,  2963,  2973,  2983,  2992,  3002,  3011,  3024,  3037,  3049,
    3063,  3075,  3089,  3097,  3108,  3117,  3126,  3133,  3143,  3150,
    3157,  3166,  3175,  3185,  3195,  3205,  3215,  3222,  3229,  3238,
    3247,  3257,  3267,  3277,  3284,  3291,  3298,  3306,  3316,  3326,
    3336,  3346,  3356,  3364,  3373,  3383,  3442,  3451,  3459,  3467,
    3482,  3491,  3497,  3498,  3499,  3499,  3499,  3500,  3500,  3500,
    3501,  3501,  3503,  3513,  3522,  3529,  3536,  3543,  3550,  3557,
    3564,  3570,  3571,  3572,  3572,  3572,  3573,  3573,  3573,  3574,
    3575,  3575,  3576,  3576,  3577,  3577,  3578,  3579,  3580,  3581,
    3582,  3583,  3584,  3586,  3595,  3605,  3612,  3619,  3628,  3635,
    3642,  3649,  3656,  3665,  3674,  3681,  3688,  3698,  3708,  3718,
    3728,  3738,  3748,  3759,  3765,  3766,  3767,  3769,  3776,  3782,
    3783,  3784,  3786,  3793,  3803,  3810,  3819,  3827,  3833,  3834,
    3836,  3836,  3836,  3837,  3837,  3838,  3839,  3840,  3841,  3842,
    3844,  3853,  3862,  3869,  3880,  3887,  3898,  3906,  3919,  3927,
    3940,  3946,  3947,  3948,  3948,  3949,  3949,  3950,  3950,  3951,
    3951,  3952,  3952,  3953,  3953,  3954,  3954,  3955,  3955,  3956,
    3956,  3957,  3957,  3958,  3960,  3972,  3984,  3997,  4010,  4022,
    4034,  4049,  4064,  4076,  4088,  4100,  4112,  4125,  4138,  4151,
    4164,  4177,  4190,  4203,  4218,  4233,  4244,  4253,  4269,  4276,
    4285,  4294,  4303,  4312,  4322,  4331,  4340,  4346,  4347,  4348,
    4348,  4350,  4365
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
  "VAR_DO_IP4", "VAR_DO_IP6", "VAR_DO_NAT64", "VAR_PREFER_IP6",
  "VAR_DO_UDP", "VAR_DO_TCP", "VAR_TCP_MSS", "VAR_OUTGOING_TCP_MSS",
  "VAR_TCP_IDLE_TIMEOUT", "VAR_EDNS_TCP_KEEPALIVE",
  "VAR_EDNS_TCP_KEEPALIVE_TIMEOUT", "VAR_SOCK_QUEUE_TIMEOUT", "VAR_CHROOT",
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
  "VAR_DNS64_IGNORE_AAAA", "VAR_NAT64_PREFIX", "VAR_DNSTAP",
  "VAR_DNSTAP_ENABLE", "VAR_DNSTAP_SOCKET_PATH", "VAR_DNSTAP_IP",
  "VAR_DNSTAP_TLS", "VAR_DNSTAP_TLS_SERVER_NAME",
  "VAR_DNSTAP_TLS_CERT_BUNDLE", "VAR_DNSTAP_TLS_CLIENT_KEY_FILE",
  "VAR_DNSTAP_TLS_CLIENT_CERT_FILE", "VAR_DNSTAP_SEND_IDENTITY",
  "VAR_DNSTAP_SEND_VERSION", "VAR_DNSTAP_BIDIRECTIONAL",
  "VAR_DNSTAP_IDENTITY", "VAR_DNSTAP_VERSION",
  "VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES",
  "VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES",
  "VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES",
  "VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES",
  "VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES",
  "VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES", "VAR_DNSTAP_SAMPLE_RATE",
  "VAR_RESPONSE_IP_TAG", "VAR_RESPONSE_IP", "VAR_RESPONSE_IP_DATA",
  "VAR_HARDEN_ALGO_DOWNGRADE", "VAR_IP_TRANSPARENT", "VAR_IP_DSCP",
  "VAR_DISABLE_DNSSEC_LAME_CHECK", "VAR_IP_RATELIMIT",
  "VAR_IP_RATELIMIT_SLABS", "VAR_IP_RATELIMIT_SIZE", "VAR_RATELIMIT",
  "VAR_RATELIMIT_SLABS", "VAR_RATELIMIT_SIZE", "VAR_OUTBOUND_MSG_RETRY",
  "VAR_MAX_SENT_COUNT", "VAR_MAX_QUERY_RESTARTS",
  "VAR_RATELIMIT_FOR_DOMAIN", "VAR_RATELIMIT_BELOW_DOMAIN",
  "VAR_IP_RATELIMIT_FACTOR", "VAR_RATELIMIT_FACTOR",
  "VAR_IP_RATELIMIT_BACKOFF", "VAR_RATELIMIT_BACKOFF",
  "VAR_SEND_CLIENT_SUBNET", "VAR_CLIENT_SUBNET_ZONE",
  "VAR_CLIENT_SUBNET_ALWAYS_FORWARD", "VAR_CLIENT_SUBNET_OPCODE",
  "VAR_MAX_CLIENT_SUBNET_IPV4", "VAR_MAX_CLIENT_SUBNET_IPV6",
  "VAR_MIN_CLIENT_SUBNET_IPV4", "VAR_MIN_CLIENT_SUBNET_IPV6",
  "VAR_MAX_ECS_TREE_SIZE_IPV4", "VAR_MAX_ECS_TREE_SIZE_IPV6",
  "VAR_CAPS_WHITELIST", "VAR_CACHE_MAX_NEGATIVE_TTL",
  "VAR_PERMIT_SMALL_HOLDDOWN", "VAR_CACHE_MIN_NEGATIVE_TTL",
  "VAR_QNAME_MINIMISATION", "VAR_QNAME_MINIMISATION_STRICT",
  "VAR_IP_FREEBIND", "VAR_DEFINE_TAG", "VAR_LOCAL_ZONE_TAG",
  "VAR_ACCESS_CONTROL_TAG", "VAR_LOCAL_ZONE_OVERRIDE",
  "VAR_ACCESS_CONTROL_TAG_ACTION", "VAR_ACCESS_CONTROL_TAG_DATA",
  "VAR_VIEW", "VAR_ACCESS_CONTROL_VIEW", "VAR_VIEW_FIRST",
  "VAR_SERVE_EXPIRED", "VAR_SERVE_EXPIRED_TTL",
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
  "VAR_CACHEDB_REDISREPLICAHOST", "VAR_CACHEDB_REDISPORT",
  "VAR_CACHEDB_REDISREPLICAPORT", "VAR_CACHEDB_REDISTIMEOUT",
  "VAR_CACHEDB_REDISREPLICATIMEOUT", "VAR_CACHEDB_REDISEXPIRERECORDS",
  "VAR_CACHEDB_REDISPATH", "VAR_CACHEDB_REDISREPLICAPATH",
  "VAR_CACHEDB_REDISPASSWORD", "VAR_CACHEDB_REDISREPLICAPASSWORD",
  "VAR_CACHEDB_REDISLOGICALDB", "VAR_CACHEDB_REDISREPLICALOGICALDB",
  "VAR_CACHEDB_REDISCOMMANDTIMEOUT",
  "VAR_CACHEDB_REDISREPLICACOMMANDTIMEOUT",
  "VAR_CACHEDB_REDISCONNECTTIMEOUT",
  "VAR_CACHEDB_REDISREPLICACONNECTTIMEOUT",
  "VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM", "VAR_FOR_UPSTREAM",
  "VAR_AUTH_ZONE", "VAR_ZONEFILE", "VAR_MASTER", "VAR_URL",
  "VAR_FOR_DOWNSTREAM", "VAR_FALLBACK_ENABLED", "VAR_TLS_ADDITIONAL_PORT",
  "VAR_LOW_RTT", "VAR_LOW_RTT_PERMIL", "VAR_FAST_SERVER_PERMIL",
  "VAR_FAST_SERVER_NUM", "VAR_ALLOW_NOTIFY", "VAR_TLS_WIN_CERT",
  "VAR_TCP_CONNECTION_LIMIT", "VAR_ANSWER_COOKIE", "VAR_COOKIE_SECRET",
  "VAR_IP_RATELIMIT_COOKIE", "VAR_FORWARD_NO_CACHE", "VAR_STUB_NO_CACHE",
  "VAR_LOG_SERVFAIL", "VAR_DENY_ANY", "VAR_UNKNOWN_SERVER_TIME_LIMIT",
  "VAR_LOG_TAG_QUERYREPLY", "VAR_DISCARD_TIMEOUT", "VAR_WAIT_LIMIT",
  "VAR_WAIT_LIMIT_COOKIE", "VAR_WAIT_LIMIT_NETBLOCK",
  "VAR_WAIT_LIMIT_COOKIE_NETBLOCK", "VAR_STREAM_WAIT_SIZE",
  "VAR_TLS_CIPHERS", "VAR_TLS_CIPHERSUITES", "VAR_TLS_USE_SNI",
  "VAR_TLS_PROTOCOLS", "VAR_IPSET", "VAR_IPSET_NAME_V4",
  "VAR_IPSET_NAME_V6", "VAR_TLS_SESSION_TICKET_KEYS", "VAR_RPZ",
  "VAR_TAGS", "VAR_RPZ_ACTION_OVERRIDE", "VAR_RPZ_CNAME_OVERRIDE",
  "VAR_RPZ_LOG", "VAR_RPZ_LOG_NAME", "VAR_DYNLIB", "VAR_DYNLIB_FILE",
  "VAR_EDNS_CLIENT_STRING", "VAR_EDNS_CLIENT_STRING_OPCODE", "VAR_NSID",
  "VAR_ZONEMD_PERMISSIVE_MODE", "VAR_ZONEMD_CHECK",
  "VAR_ZONEMD_REJECT_ABSENCE", "VAR_RPZ_SIGNAL_NXDOMAIN_RA",
  "VAR_INTERFACE_AUTOMATIC_PORTS", "VAR_EDE", "VAR_DNS_ERROR_REPORTING",
  "VAR_INTERFACE_ACTION", "VAR_INTERFACE_VIEW", "VAR_INTERFACE_TAG",
  "VAR_INTERFACE_TAG_ACTION", "VAR_INTERFACE_TAG_DATA", "VAR_QUIC_PORT",
  "VAR_QUIC_SIZE", "VAR_PROXY_PROTOCOL_PORT",
  "VAR_STATISTICS_INHIBIT_ZERO", "VAR_HARDEN_UNKNOWN_ADDITIONAL",
  "VAR_DISABLE_EDNS_DO", "VAR_CACHEDB_NO_STORE", "VAR_LOG_DESTADDR",
  "VAR_CACHEDB_CHECK_WHEN_SERVE_EXPIRED", "VAR_COOKIE_SECRET_FILE",
  "VAR_ITER_SCRUB_NS", "VAR_ITER_SCRUB_CNAME", "VAR_ITER_SCRUB_RRSIG",
  "VAR_MAX_TRANSFER_SIZE", "VAR_MAX_TRANSFER_TIME", "VAR_MAX_GLOBAL_QUOTA",
  "VAR_HARDEN_UNVERIFIED_GLUE", "VAR_LOG_TIME_ISO",
  "VAR_VAL_VALIDATION_ATTEMPTS", "VAR_VAL_HASH_ATTEMPTS",
  "VAR_ITER_SCRUB_PROMISCUOUS", "VAR_LOG_THREAD_ID", "$accept",
  "toplevelvars", "toplevelvar", "force_toplevel", "serverstart",
  "contents_server", "content_server", "stub_clause", "stubstart",
  "contents_stub", "content_stub", "forward_clause", "forwardstart",
  "contents_forward", "content_forward", "view_clause", "viewstart",
  "contents_view", "content_view", "authstart", "contents_auth",
  "content_auth", "rpz_tag", "rpz_action_override", "rpz_cname_override",
  "rpz_log", "rpz_log_name", "rpz_signal_nxdomain_ra", "rpzstart",
  "contents_rpz", "content_rpz", "server_num_threads", "server_verbosity",
  "server_statistics_interval", "server_statistics_cumulative",
  "server_extended_statistics", "server_statistics_inhibit_zero",
  "server_shm_enable", "server_shm_key", "server_port",
  "server_send_client_subnet", "server_client_subnet_zone",
  "server_client_subnet_always_forward", "server_client_subnet_opcode",
  "server_max_client_subnet_ipv4", "server_max_client_subnet_ipv6",
  "server_min_client_subnet_ipv4", "server_min_client_subnet_ipv6",
  "server_max_ecs_tree_size_ipv4", "server_max_ecs_tree_size_ipv6",
  "server_interface", "server_outgoing_interface", "server_outgoing_range",
  "server_outgoing_port_permit", "server_outgoing_port_avoid",
  "server_outgoing_num_tcp", "server_incoming_num_tcp",
  "server_interface_automatic", "server_interface_automatic_ports",
  "server_do_ip4", "server_do_ip6", "server_do_nat64", "server_do_udp",
  "server_do_tcp", "server_prefer_ip4", "server_prefer_ip6",
  "server_tcp_mss", "server_outgoing_tcp_mss", "server_tcp_idle_timeout",
  "server_max_reuse_tcp_queries", "server_tcp_reuse_timeout",
  "server_tcp_auth_query_timeout", "server_tcp_keepalive",
  "server_tcp_keepalive_timeout", "server_sock_queue_timeout",
  "server_tcp_upstream", "server_udp_upstream_without_downstream",
  "server_ssl_upstream", "server_ssl_service_key",
  "server_ssl_service_pem", "server_ssl_port", "server_tls_cert_bundle",
  "server_tls_win_cert", "server_tls_additional_port",
  "server_tls_ciphers", "server_tls_ciphersuites",
  "server_tls_session_ticket_keys", "server_tls_use_sni",
  "server_tls_protocols", "server_https_port", "server_http_endpoint",
  "server_http_max_streams", "server_http_query_buffer_size",
  "server_http_response_buffer_size", "server_http_nodelay",
  "server_http_notls_downstream", "server_quic_port", "server_quic_size",
  "server_use_systemd", "server_do_daemonize", "server_use_syslog",
  "server_log_time_ascii", "server_log_time_iso", "server_log_queries",
  "server_log_replies", "server_log_tag_queryreply", "server_log_servfail",
  "server_log_destaddr", "server_log_thread_id",
  "server_log_local_actions", "server_chroot", "server_username",
  "server_directory", "server_logfile", "server_pidfile",
  "server_root_hints", "server_dlv_anchor_file", "server_dlv_anchor",
  "server_auto_trust_anchor_file", "server_trust_anchor_file",
  "server_trusted_keys_file", "server_trust_anchor",
  "server_trust_anchor_signaling", "server_root_key_sentinel",
  "server_domain_insecure", "server_hide_identity", "server_hide_version",
  "server_hide_trustanchor", "server_hide_http_user_agent",
  "server_identity", "server_version", "server_http_user_agent",
  "server_nsid", "server_so_rcvbuf", "server_so_sndbuf",
  "server_so_reuseport", "server_ip_transparent", "server_ip_freebind",
  "server_ip_dscp", "server_stream_wait_size", "server_edns_buffer_size",
  "server_msg_buffer_size", "server_msg_cache_size",
  "server_msg_cache_slabs", "server_num_queries_per_thread",
  "server_jostle_timeout", "server_delay_close", "server_udp_connect",
  "server_unblock_lan_zones", "server_insecure_lan_zones",
  "server_rrset_cache_size", "server_rrset_cache_slabs",
  "server_infra_host_ttl", "server_infra_lame_ttl",
  "server_infra_cache_numhosts", "server_infra_cache_lame_size",
  "server_infra_cache_slabs", "server_infra_cache_min_rtt",
  "server_infra_cache_max_rtt", "server_infra_keep_probing",
  "server_target_fetch_policy", "server_harden_short_bufsize",
  "server_harden_large_queries", "server_harden_glue",
  "server_harden_unverified_glue", "server_harden_dnssec_stripped",
  "server_harden_below_nxdomain", "server_harden_referral_path",
  "server_harden_algo_downgrade", "server_harden_unknown_additional",
  "server_use_caps_for_id", "server_caps_whitelist",
  "server_private_address", "server_private_domain", "server_prefetch",
  "server_prefetch_key", "server_deny_any",
  "server_unwanted_reply_threshold", "server_do_not_query_address",
  "server_do_not_query_localhost", "server_access_control",
  "server_interface_action", "server_module_conf",
  "server_val_override_date", "server_val_sig_skew_min",
  "server_val_sig_skew_max", "server_val_max_restart",
  "server_cache_max_ttl", "server_cache_max_negative_ttl",
  "server_cache_min_negative_ttl", "server_cache_min_ttl",
  "server_bogus_ttl", "server_val_clean_additional",
  "server_val_permissive_mode", "server_aggressive_nsec",
  "server_ignore_cd_flag", "server_disable_edns_do",
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
  "server_discard_timeout", "server_wait_limit",
  "server_wait_limit_cookie", "server_wait_limit_netblock",
  "server_wait_limit_cookie_netblock", "server_max_udp_size",
  "server_dns64_prefix", "server_dns64_synthall",
  "server_dns64_ignore_aaaa", "server_nat64_prefix", "server_define_tag",
  "server_local_zone_tag", "server_access_control_tag",
  "server_access_control_tag_action", "server_access_control_tag_data",
  "server_local_zone_override", "server_access_control_view",
  "server_interface_tag", "server_interface_tag_action",
  "server_interface_tag_data", "server_interface_view",
  "server_response_ip_tag", "server_ip_ratelimit",
  "server_ip_ratelimit_cookie", "server_ratelimit",
  "server_ip_ratelimit_size", "server_ratelimit_size",
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
  "server_dns_error_reporting", "server_proxy_protocol_port", "stub_name",
  "stub_host", "stub_addr", "stub_first", "stub_no_cache",
  "stub_ssl_upstream", "stub_tcp_upstream", "stub_prime", "forward_name",
  "forward_host", "forward_addr", "forward_first", "forward_no_cache",
  "forward_ssl_upstream", "forward_tcp_upstream", "auth_name",
  "auth_zonefile", "auth_master", "auth_url", "auth_allow_notify",
  "auth_zonemd_check", "auth_zonemd_reject_absence", "auth_for_downstream",
  "auth_for_upstream", "auth_fallback_enabled", "auth_max_transfer_size",
  "auth_max_transfer_time", "view_name", "view_local_zone",
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
  "dt_dnstap_log_forwarder_response_messages", "dt_dnstap_sample_rate",
  "pythonstart", "contents_py", "content_py", "py_script", "dynlibstart",
  "contents_dl", "content_dl", "dl_file",
  "server_disable_dnssec_lame_check", "server_log_identity",
  "server_response_ip", "server_response_ip_data", "dnscstart",
  "contents_dnsc", "content_dnsc", "dnsc_dnscrypt_enable",
  "dnsc_dnscrypt_port", "dnsc_dnscrypt_provider",
  "dnsc_dnscrypt_provider_cert", "dnsc_dnscrypt_provider_cert_rotated",
  "dnsc_dnscrypt_secret_key", "dnsc_dnscrypt_shared_secret_cache_size",
  "dnsc_dnscrypt_shared_secret_cache_slabs",
  "dnsc_dnscrypt_nonce_cache_size", "dnsc_dnscrypt_nonce_cache_slabs",
  "cachedbstart", "contents_cachedb", "content_cachedb",
  "cachedb_backend_name", "cachedb_secret_seed", "cachedb_no_store",
  "cachedb_check_when_serve_expired", "redis_server_host",
  "redis_replica_server_host", "redis_server_port",
  "redis_replica_server_port", "redis_server_path",
  "redis_replica_server_path", "redis_server_password",
  "redis_replica_server_password", "redis_timeout",
  "redis_replica_timeout", "redis_command_timeout",
  "redis_replica_command_timeout", "redis_connect_timeout",
  "redis_replica_connect_timeout", "redis_expire_records",
  "redis_logical_db", "redis_replica_logical_db",
  "server_tcp_connection_limit", "server_answer_cookie",
  "server_cookie_secret", "server_cookie_secret_file",
  "server_iter_scrub_ns", "server_iter_scrub_cname",
  "server_iter_scrub_rrsig", "server_max_global_quota",
  "server_iter_scrub_promiscuous", "server_val_validation_attempts",
  "server_val_hash_attempts", "ipsetstart", "contents_ipset",
  "content_ipset", "ipset_name_v4", "ipset_name_v6", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-311)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -311,   275,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,   -13,   227,   234,   280,   114,
      88,   -14,   384,   -81,  -310,   -99,   210,  -303,    31,    32,
      33,    83,    89,    93,    94,   121,   122,   123,   126,   134,
     149,   150,   162,   163,   164,   165,   166,   167,   168,   169,
     215,   217,   237,   238,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   258,   259,   260,   261,   262,   263,   268,
     269,   270,   272,   273,   274,   281,   282,   283,   284,   285,
     288,   291,   297,   298,   314,   315,   318,   320,   321,   322,
     323,   324,   330,   331,   346,   348,   364,   366,   367,   369,
     371,   372,   375,   376,   377,   378,   379,   386,   387,   388,
     389,   390,   391,   393,   394,   395,   396,   397,   398,   399,
     400,   401,   402,   403,   404,   405,   407,   413,   414,   415,
     416,   417,   424,   425,   426,   427,   428,   429,   430,   431,
     433,   434,   435,   436,   438,   439,   440,   441,   442,   443,
     444,   445,   446,   447,   448,   449,   450,   451,   452,   453,
     458,   459,   460,   461,   462,   463,   466,   467,   468,   469,
     470,   471,   472,   473,   474,   475,   476,   477,   478,   479,
     501,   502,   504,   505,   506,   507,   508,   509,   511,   512,
     513,   514,   515,   516,   517,   518,   519,   520,   521,   522,
     523,   524,   526,   527,   528,   529,   530,   531,   532,   533,
     534,   535,   536,   537,   539,   540,   541,   542,   543,   545,
     567,   570,   572,   573,   574,   575,   576,   577,   578,   579,
     580,   581,   582,   583,   584,   585,   586,   587,   588,   589,
     590,   591,   592,   593,   594,   595,   596,   597,   598,   599,
     600,   602,   603,   604,   606,   607,   608,   609,   610,   612,
     613,   614,   615,   616,   617,   618,   619,   620,   621,   622,
     623,   624,   625,   626,   627,   628,   629,   630,   631,   632,
     633,   634,   635,   636,   637,   638,   639,   640,   641,   642,
     643,   644,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,   645,   646,   647,
     648,   649,   650,   651,   652,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,   653,   654,   655,   656,   657,   658,
     659,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,   660,
     661,   662,   663,   664,   665,   666,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,   667,   668,   669,   670,   671,   672,
     673,   674,   675,   676,   677,   678,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,   679,
     680,   681,   682,   683,   684,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
     685,   686,   687,   688,   689,   690,   691,   692,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,   693,   694,   695,
     696,   697,   698,   699,   700,   701,   702,   703,   704,   705,
     706,   707,   708,   709,   710,   711,   712,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,   713,  -311,
    -311,   714,  -311,  -311,   715,   716,   717,   718,   719,   720,
     721,   722,   723,   724,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,   725,   726,   727,   728,   729,
     730,   731,   732,   733,   734,   735,   736,   737,   738,   739,
     740,   741,   742,   743,   744,   745,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,   746,   747,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,   748,   749,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,   750,   751,   752,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,   753,   754,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,   755,   756,   757,
     758,   759,   760,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,   761,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,   762,
     763,  -311,  -311,  -311,  -311,  -311,  -311,   764,  -311,  -311,
    -311,  -311,  -311,  -311,   765,   766,   767,   768,   769,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,   770,  -311,  -311,   771,   772,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,   773,
     774,   775,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
     776,   777,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       2,     0,     1,    18,    19,   287,   299,   641,   703,   660,
     310,   717,   740,   320,   796,   341,   708,     3,    17,    21,
       5,   289,     6,   301,    10,   312,   322,   343,   643,   662,
     705,   710,   719,   742,   798,     4,   286,   298,   309,    14,
      15,     8,     9,     7,    16,    11,    12,    13,     0,     0,
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
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    20,    22,    23,    90,    93,   102,   272,   220,
     221,    24,   173,   174,   175,   176,   177,   178,   179,   180,
     181,   182,    39,    81,    25,    94,    95,    50,    74,    89,
     268,    26,    27,    28,    31,    32,    29,    30,    33,    34,
      35,   263,   264,   265,    36,    37,    38,   126,   232,   127,
     130,   131,   132,   234,   239,   235,   251,   252,   253,   257,
     259,   133,   134,   135,   136,   137,   138,   139,   266,   267,
     216,    91,    80,   106,   284,   124,   125,   244,   241,   275,
     129,   128,    40,    41,    42,    43,    44,    82,    96,    97,
     113,    68,    78,    69,   224,   225,   107,    60,    61,   223,
      64,    62,    63,    65,   261,   117,   121,   143,   155,   188,
     158,   250,   118,    75,    45,    46,    47,   104,   144,   145,
     146,   147,    48,    49,    51,    52,    54,    55,    53,   152,
     153,   159,    56,    57,    58,    66,   283,    85,   122,    99,
     154,   273,    92,   183,   100,   101,   119,   120,   242,   105,
      59,    83,    86,   197,    67,    70,   108,   109,   110,    84,
     184,   185,   111,    71,    72,    73,   233,   123,   274,   207,
     208,   209,   210,   211,   212,   213,   214,   222,   112,    79,
     262,   114,   115,   116,   186,    76,    77,    98,    87,    88,
     103,   140,   141,   243,   245,   246,   247,   248,   249,   142,
     148,   149,   150,   151,   189,   190,   192,   194,   195,   193,
     196,   199,   200,   201,   198,   217,   156,   256,   157,   162,
     163,   160,   161,   164,   165,   167,   166,   169,   168,   170,
     171,   172,   236,   238,   237,   187,   202,   203,   204,   205,
     206,   226,   228,   227,   229,   230,   231,   258,   260,   269,
     270,   271,   191,   215,   218,   219,   240,   254,   255,   276,
     277,   278,   282,   279,   285,   280,   281,     0,     0,     0,
       0,     0,     0,     0,     0,   288,   290,   291,   292,   294,
     295,   296,   297,   293,     0,     0,     0,     0,     0,     0,
       0,   300,   302,   303,   304,   305,   306,   307,   308,     0,
       0,     0,     0,     0,     0,     0,   311,   313,   314,   317,
     318,   315,   319,   316,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   321,   323,   324,   325,
     326,   330,   331,   332,   327,   328,   329,   333,   334,     0,
       0,     0,     0,     0,     0,   346,   350,   351,   352,   353,
     354,   342,   344,   345,   347,   348,   349,   355,   356,   357,
       0,     0,     0,     0,     0,     0,     0,     0,   642,   644,
     646,   645,   651,   647,   648,   649,   650,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   661,   663,   665,
     664,   666,   667,   668,   669,   670,   671,   672,   673,   674,
     675,   676,   677,   678,   679,   680,   681,   682,     0,   704,
     706,     0,   709,   711,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   718,   720,   721,   722,   724,   725,
     723,   726,   727,   728,   729,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   741,   743,   744,   761,
     763,   745,   746,   747,   748,   755,   756,   757,   758,   749,
     750,   751,   752,   753,   754,   762,   759,   760,     0,     0,
     797,   799,   800,   359,   358,   366,   379,   377,   391,   386,
     387,   388,   392,   389,   390,   393,   394,   395,   399,   400,
     401,   437,   438,   439,   440,   441,   469,   470,   471,   477,
     478,   382,   479,   480,   483,   481,   482,   487,   488,   489,
     505,   452,   453,   456,   457,   490,   509,   446,   448,   510,
     518,   519,   520,   383,   468,   540,   541,   447,   534,   427,
     378,   442,   506,   514,   492,     0,     0,   544,   384,   360,
     426,   497,   361,   380,   381,   443,   444,   542,   494,   499,
     500,   397,   396,   362,   545,   472,   504,   428,   451,   511,
     512,   513,   517,   533,   445,   538,   536,   537,   460,   467,
     501,   502,   461,   462,   493,   522,   430,   431,   436,   402,
     404,   398,   405,   406,   407,   408,   416,   417,   418,   419,
     420,   421,   422,   546,   547,   554,   473,   474,   475,   476,
     484,   485,   486,   555,   556,   557,   558,     0,     0,     0,
     495,   463,   465,   713,   571,   576,   574,   573,   577,   575,
     584,   585,   586,     0,     0,   580,   581,   582,   583,   367,
     368,   369,   370,   371,   372,   373,   374,   375,   376,   498,
     515,   539,   516,   590,   591,   464,   559,     0,     0,     0,
       0,     0,     0,   524,   525,   526,   527,   528,   529,   530,
     531,   532,   714,   454,   455,   458,   449,   521,   425,   364,
     365,   450,   592,   593,   594,   595,   596,   598,   597,   599,
     600,   601,   403,   410,   587,   589,   588,   409,     0,   786,
     787,   572,   433,   503,   548,   432,   549,   550,   551,     0,
       0,   466,   411,   412,   414,   415,   413,     0,   603,   459,
     535,   385,   604,   605,     0,     0,     0,     0,     0,   423,
     424,   606,   363,   496,   523,   434,   788,   789,   790,   791,
     792,   491,   429,   794,   795,   793,   435,   607,   608,   609,
     614,   612,   613,   610,   611,   615,   616,   617,   618,   620,
     621,   619,   634,     0,   638,   639,     0,     0,   640,   622,
     630,   623,   624,   625,   629,   631,   626,   627,   628,   632,
     633,   335,   336,   337,   338,   339,   340,   652,   654,   653,
     656,   657,   658,   659,   655,   683,   685,   686,   687,   688,
     689,   690,   691,   692,   693,   684,   694,   695,   696,   697,
     698,   699,   700,   701,   702,   707,   712,   730,   731,   732,
     735,   733,   734,   736,   737,   738,   739,   764,   765,   768,
     769,   770,   771,   776,   777,   782,   772,   773,   774,   775,
     783,   784,   778,   779,   780,   781,   766,   767,   801,   802,
     507,   543,   570,   715,   716,   578,   579,   560,   561,     0,
       0,     0,   565,   785,   552,   553,   602,   508,   569,   566,
       0,     0,   635,   636,   637,   564,   562,   563,   567,   568
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,   778,   779,   780,   781,   782,
    -311,  -311,   783,  -311,  -311,   784,   785,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,  -311,
    -311,  -311,  -311
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     1,    17,    18,    19,    35,   312,    20,    21,    36,
     585,    22,    23,    37,   601,    24,    25,    38,   616,    26,
      39,   636,   655,   656,   657,   658,   659,   660,    27,    40,
     661,   313,   314,   315,   316,   317,   318,   319,   320,   321,
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
     502,   503,   504,   505,   506,   507,   508,   509,   510,   511,
     512,   513,   514,   515,   516,   517,   518,   519,   520,   521,
     522,   523,   524,   525,   526,   527,   528,   529,   530,   531,
     532,   533,   534,   535,   536,   537,   538,   539,   540,   541,
     542,   543,   544,   545,   546,   547,   548,   549,   550,   551,
     552,   553,   554,   555,   556,   557,   558,   559,   560,   561,
     586,   587,   588,   589,   590,   591,   592,   593,   602,   603,
     604,   605,   606,   607,   608,   637,   638,   639,   640,   641,
     642,   643,   644,   645,   646,   647,   648,   617,   618,   619,
     620,   621,   622,   623,    28,    41,   678,   679,   680,   681,
     682,   683,   684,   685,   686,    29,    42,   707,   708,   709,
     710,   711,   712,   713,   714,   715,   716,   717,   718,   719,
     720,   721,   722,   723,   724,   725,   726,   727,    30,    43,
     729,   730,    31,    44,   732,   733,   562,   563,   564,   565,
      32,    45,   744,   745,   746,   747,   748,   749,   750,   751,
     752,   753,   754,    33,    46,   776,   777,   778,   779,   780,
     781,   782,   783,   784,   785,   786,   787,   788,   789,   790,
     791,   792,   793,   794,   795,   796,   797,   566,   567,   568,
     569,   570,   571,   572,   573,   574,   575,   576,    34,    47,
     800,   801,   802
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,   798,   799,   728,   731,    82,    83,
      84,   803,   804,   805,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   670,   671,   672,   673,   674,
     675,   676,   677,   806,   126,   127,   128,   129,   130,   807,
     131,   132,   133,   808,   809,   134,   135,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,   148,
     149,   150,   151,   152,   153,   154,   155,   156,   157,   158,
     159,   810,   811,   812,   160,   624,   813,   161,   162,   163,
     164,   165,   166,   167,   814,   168,   169,   170,   171,   172,
     173,   174,   175,   176,   177,   178,   179,   180,   181,   815,
     816,   624,   734,   735,   736,   737,   738,   739,   740,   741,
     742,   743,   817,   818,   819,   820,   821,   822,   823,   824,
     182,   183,   184,   185,   186,   187,   188,   189,   190,   191,
     192,   193,   194,   195,   196,   197,   198,   199,   200,   201,
     202,   203,   204,   205,   206,   207,   208,   209,   210,   211,
     212,   213,   214,   215,   216,   217,   218,   219,   220,   221,
     222,   223,   224,   225,   226,   825,   227,   826,   228,   229,
     230,   231,   232,   233,   234,   235,   236,   237,   238,   239,
     240,   241,   242,   243,   244,   245,   246,   827,   828,   829,
     830,   831,   832,   833,   834,   835,   836,   837,   247,   248,
     249,   250,   251,   252,   253,   254,   255,   256,   838,   839,
     840,   841,   842,   843,   577,     2,   578,   579,   844,   845,
     846,   594,   847,   848,   849,     0,     3,     4,   257,   595,
     596,   850,   851,   852,   853,   854,   258,   259,   855,   260,
     261,   856,   262,   263,   264,   265,   266,   857,   858,   267,
     268,   269,   270,   271,   272,   273,   274,   275,   276,   277,
     278,   279,   280,     5,   859,   860,   281,   609,   861,     6,
     862,   863,   864,   865,   866,   282,   283,   284,   285,   580,
     867,   868,   286,   287,   288,   289,   290,   291,   292,   293,
     294,   295,   296,   297,   298,   299,   869,   300,   870,   301,
     302,   303,   304,   610,   611,   305,   306,   307,   308,   309,
     310,   311,   581,     7,   871,   582,   872,   873,   597,   874,
     598,   875,   876,   599,   583,   877,   878,   879,   880,   881,
     612,     8,   626,   627,   628,   629,   882,   883,   884,   885,
     886,   887,   631,   888,   889,   890,   891,   892,   893,   894,
     895,   896,   897,   898,   899,   900,   625,   901,   626,   627,
     628,   629,   630,   902,   903,   904,   905,   906,   631,   649,
     650,   651,   652,   653,   907,   908,   909,   910,   911,   912,
     913,   914,   654,   915,   916,   917,   918,     9,   919,   920,
     921,   922,   923,   924,   925,   926,   927,   928,   929,   930,
     931,   932,   933,   934,   634,   635,   632,   633,   935,   936,
     937,   938,   939,   940,   613,   614,   941,   942,   943,   944,
     945,   946,   947,   948,   949,   950,   951,   952,   953,   954,
     634,   635,   755,   756,   757,   758,   759,   760,   761,   762,
     763,   764,   765,   766,   767,   768,   769,   770,   771,   772,
     773,   955,   956,    10,   957,   958,   959,   960,   961,   962,
     615,   963,   964,   965,   966,   967,   968,   969,   970,   971,
     972,   973,   974,   975,   976,    11,   977,   978,   979,   980,
     981,   982,   983,   984,   985,   986,   987,   988,   584,   989,
     990,   991,   992,   993,   600,   994,    12,   687,   688,   689,
     690,   691,   692,   693,   694,   695,   696,   697,   698,   699,
     700,   701,   702,   703,   704,   705,   706,   995,    13,   774,
     996,   775,   997,   998,   999,  1000,  1001,  1002,  1003,  1004,
    1005,  1006,  1007,  1008,  1009,  1010,  1011,  1012,  1013,  1014,
    1015,  1016,  1017,  1018,  1019,  1020,  1021,  1022,  1023,  1024,
    1025,    14,  1026,  1027,  1028,    15,  1029,  1030,  1031,  1032,
    1033,    16,  1034,  1035,  1036,  1037,  1038,  1039,  1040,  1041,
    1042,  1043,  1044,  1045,  1046,  1047,  1048,  1049,  1050,  1051,
    1052,  1053,  1054,  1055,  1056,  1057,  1058,  1059,  1060,  1061,
    1062,  1063,  1064,  1065,  1066,  1067,  1068,  1069,  1070,  1071,
    1072,  1073,  1074,  1075,  1076,  1077,  1078,  1079,  1080,  1081,
    1082,  1083,  1084,  1085,  1086,  1087,  1088,  1089,  1090,  1091,
    1092,  1093,  1094,  1095,  1096,  1097,  1098,  1099,  1100,  1101,
    1102,  1103,  1104,  1105,  1106,  1107,  1108,  1109,  1110,  1111,
    1112,  1113,  1114,  1115,  1116,  1117,  1118,  1119,  1120,  1121,
    1122,  1123,  1124,  1125,  1126,  1127,  1128,  1129,  1130,  1131,
    1132,  1133,  1134,  1135,  1136,  1137,  1138,  1139,  1140,  1141,
    1142,  1143,  1144,  1145,  1146,  1147,  1148,  1149,  1150,  1151,
    1152,  1153,  1154,  1155,  1156,  1157,  1158,  1159,  1160,  1161,
    1162,  1163,  1164,  1165,  1166,  1167,  1168,  1169,  1170,  1171,
    1172,  1173,  1174,  1175,  1176,  1177,  1178,  1179,  1180,  1181,
    1182,  1183,  1184,  1185,  1186,  1187,  1188,  1189,  1190,  1191,
    1192,  1193,  1194,  1195,  1196,  1197,  1198,  1199,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   662,   663,
     664,   665,   666,   667,   668,   669
};

static const yytype_int16 yycheck[] =
{
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,   337,   338,   117,   347,    51,    52,
      53,    10,    10,    10,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    99,   100,   101,   102,   103,
     104,   105,   106,    10,   107,   108,   109,   110,   111,    10,
     113,   114,   115,    10,    10,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,    10,    10,    10,   147,    47,    10,   150,   151,   152,
     153,   154,   155,   156,    10,   158,   159,   160,   161,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,    10,
      10,    47,   261,   262,   263,   264,   265,   266,   267,   268,
     269,   270,    10,    10,    10,    10,    10,    10,    10,    10,
     193,   194,   195,   196,   197,   198,   199,   200,   201,   202,
     203,   204,   205,   206,   207,   208,   209,   210,   211,   212,
     213,   214,   215,   216,   217,   218,   219,   220,   221,   222,
     223,   224,   225,   226,   227,   228,   229,   230,   231,   232,
     233,   234,   235,   236,   237,    10,   239,    10,   241,   242,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   255,   256,   257,   258,   259,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,   271,   272,
     273,   274,   275,   276,   277,   278,   279,   280,    10,    10,
      10,    10,    10,    10,    47,     0,    49,    50,    10,    10,
      10,    47,    10,    10,    10,    -1,    11,    12,   301,    55,
      56,    10,    10,    10,    10,    10,   309,   310,    10,   312,
     313,    10,   315,   316,   317,   318,   319,    10,    10,   322,
     323,   324,   325,   326,   327,   328,   329,   330,   331,   332,
     333,   334,   335,    48,    10,    10,   339,    47,    10,    54,
      10,    10,    10,    10,    10,   348,   349,   350,   351,   112,
      10,    10,   355,   356,   357,   358,   359,   360,   361,   362,
     363,   364,   365,   366,   367,   368,    10,   370,    10,   372,
     373,   374,   375,    83,    84,   378,   379,   380,   381,   382,
     383,   384,   145,    98,    10,   148,    10,    10,   144,    10,
     146,    10,    10,   149,   157,    10,    10,    10,    10,    10,
     110,   116,   304,   305,   306,   307,    10,    10,    10,    10,
      10,    10,   314,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,   302,    10,   304,   305,
     306,   307,   308,    10,    10,    10,    10,    10,   314,   341,
     342,   343,   344,   345,    10,    10,    10,    10,    10,    10,
      10,    10,   354,    10,    10,    10,    10,   172,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,   376,   377,   352,   353,    10,    10,
      10,    10,    10,    10,   194,   195,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
     376,   377,   282,   283,   284,   285,   286,   287,   288,   289,
     290,   291,   292,   293,   294,   295,   296,   297,   298,   299,
     300,    10,    10,   238,    10,    10,    10,    10,    10,    10,
     240,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,   260,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,   321,    10,
      10,    10,    10,    10,   320,    10,   281,   173,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,   189,   190,   191,   192,    10,   303,   369,
      10,   371,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,   336,    10,    10,    10,   340,    10,    10,    10,    10,
      10,   346,    10,    10,    10,    10,    10,    10,    10,    10,
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
      10,    10,    10,    10,    10,    10,    10,    10,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    40,    40,
      40,    40,    40,    40,    40,    40
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int16 yystos[] =
{
       0,   386,     0,    11,    12,    48,    54,    98,   116,   172,
     238,   260,   281,   303,   336,   340,   346,   387,   388,   389,
     392,   393,   396,   397,   400,   401,   404,   413,   699,   710,
     733,   737,   745,   758,   793,   390,   394,   398,   402,   405,
     414,   700,   711,   734,   738,   746,   759,   794,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    51,    52,    53,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,   107,   108,   109,   110,
     111,   113,   114,   115,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     147,   150,   151,   152,   153,   154,   155,   156,   158,   159,
     160,   161,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   193,   194,   195,   196,   197,   198,   199,   200,
     201,   202,   203,   204,   205,   206,   207,   208,   209,   210,
     211,   212,   213,   214,   215,   216,   217,   218,   219,   220,
     221,   222,   223,   224,   225,   226,   227,   228,   229,   230,
     231,   232,   233,   234,   235,   236,   237,   239,   241,   242,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   255,   256,   257,   258,   259,   271,   272,   273,
     274,   275,   276,   277,   278,   279,   280,   301,   309,   310,
     312,   313,   315,   316,   317,   318,   319,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   339,   348,   349,   350,   351,   355,   356,   357,   358,
     359,   360,   361,   362,   363,   364,   365,   366,   367,   368,
     370,   372,   373,   374,   375,   378,   379,   380,   381,   382,
     383,   384,   391,   416,   417,   418,   419,   420,   421,   422,
     423,   424,   425,   426,   427,   428,   429,   430,   431,   432,
     433,   434,   435,   436,   437,   438,   439,   440,   441,   442,
     443,   444,   445,   446,   447,   448,   449,   450,   451,   452,
     453,   454,   455,   456,   457,   458,   459,   460,   461,   462,
     463,   464,   465,   466,   467,   468,   469,   470,   471,   472,
     473,   474,   475,   476,   477,   478,   479,   480,   481,   482,
     483,   484,   485,   486,   487,   488,   489,   490,   491,   492,
     493,   494,   495,   496,   497,   498,   499,   500,   501,   502,
     503,   504,   505,   506,   507,   508,   509,   510,   511,   512,
     513,   514,   515,   516,   517,   518,   519,   520,   521,   522,
     523,   524,   525,   526,   527,   528,   529,   530,   531,   532,
     533,   534,   535,   536,   537,   538,   539,   540,   541,   542,
     543,   544,   545,   546,   547,   548,   549,   550,   551,   552,
     553,   554,   555,   556,   557,   558,   559,   560,   561,   562,
     563,   564,   565,   566,   567,   568,   569,   570,   571,   572,
     573,   574,   575,   576,   577,   578,   579,   580,   581,   582,
     583,   584,   585,   586,   587,   588,   589,   590,   591,   592,
     593,   594,   595,   596,   597,   598,   599,   600,   601,   602,
     603,   604,   605,   606,   607,   608,   609,   610,   611,   612,
     613,   614,   615,   616,   617,   618,   619,   620,   621,   622,
     623,   624,   625,   626,   627,   628,   629,   630,   631,   632,
     633,   634,   635,   636,   637,   638,   639,   640,   641,   642,
     643,   644,   645,   646,   647,   648,   649,   650,   651,   652,
     653,   654,   655,   656,   657,   658,   659,   660,   661,   662,
     663,   664,   741,   742,   743,   744,   782,   783,   784,   785,
     786,   787,   788,   789,   790,   791,   792,    47,    49,    50,
     112,   145,   148,   157,   321,   395,   665,   666,   667,   668,
     669,   670,   671,   672,    47,    55,    56,   144,   146,   149,
     320,   399,   673,   674,   675,   676,   677,   678,   679,    47,
      83,    84,   110,   194,   195,   240,   403,   692,   693,   694,
     695,   696,   697,   698,    47,   302,   304,   305,   306,   307,
     308,   314,   352,   353,   376,   377,   406,   680,   681,   682,
     683,   684,   685,   686,   687,   688,   689,   690,   691,   341,
     342,   343,   344,   345,   354,   407,   408,   409,   410,   411,
     412,   415,   680,   681,   682,   683,   684,   687,   690,   691,
      99,   100,   101,   102,   103,   104,   105,   106,   701,   702,
     703,   704,   705,   706,   707,   708,   709,   173,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,   189,   190,   191,   192,   712,   713,   714,
     715,   716,   717,   718,   719,   720,   721,   722,   723,   724,
     725,   726,   727,   728,   729,   730,   731,   732,   117,   735,
     736,   347,   739,   740,   261,   262,   263,   264,   265,   266,
     267,   268,   269,   270,   747,   748,   749,   750,   751,   752,
     753,   754,   755,   756,   757,   282,   283,   284,   285,   286,
     287,   288,   289,   290,   291,   292,   293,   294,   295,   296,
     297,   298,   299,   300,   369,   371,   760,   761,   762,   763,
     764,   765,   766,   767,   768,   769,   770,   771,   772,   773,
     774,   775,   776,   777,   778,   779,   780,   781,   337,   338,
     795,   796,   797,    10,    10,    10,    10,    10,    10,    10,
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
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int16 yyr1[] =
{
       0,   385,   386,   386,   387,   387,   387,   387,   387,   387,
     387,   387,   387,   387,   387,   387,   387,   387,   388,   389,
     390,   390,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   391,   391,   391,   391,
     391,   391,   391,   391,   391,   391,   392,   393,   394,   394,
     395,   395,   395,   395,   395,   395,   395,   395,   396,   397,
     398,   398,   399,   399,   399,   399,   399,   399,   399,   400,
     401,   402,   402,   403,   403,   403,   403,   403,   403,   403,
     404,   405,   405,   406,   406,   406,   406,   406,   406,   406,
     406,   406,   406,   406,   406,   407,   408,   409,   410,   411,
     412,   413,   414,   414,   415,   415,   415,   415,   415,   415,
     415,   415,   415,   415,   415,   415,   415,   415,   416,   417,
     418,   419,   420,   421,   422,   423,   424,   425,   426,   427,
     428,   429,   430,   431,   432,   433,   434,   435,   436,   437,
     438,   439,   440,   441,   442,   443,   444,   445,   446,   447,
     448,   449,   450,   451,   452,   453,   454,   455,   456,   457,
     458,   459,   460,   461,   462,   463,   464,   465,   466,   467,
     468,   469,   470,   471,   472,   473,   474,   475,   476,   477,
     478,   479,   480,   481,   482,   483,   484,   485,   486,   487,
     488,   489,   490,   491,   492,   493,   494,   495,   496,   497,
     498,   499,   500,   501,   502,   503,   504,   505,   506,   507,
     508,   509,   510,   511,   512,   513,   514,   515,   516,   517,
     518,   519,   520,   521,   522,   523,   524,   525,   526,   527,
     528,   529,   530,   531,   532,   533,   534,   535,   536,   537,
     538,   539,   540,   541,   542,   543,   544,   545,   546,   547,
     548,   549,   550,   551,   552,   553,   554,   555,   556,   557,
     558,   559,   560,   561,   562,   563,   564,   565,   566,   567,
     568,   569,   570,   571,   572,   573,   574,   575,   576,   577,
     578,   579,   580,   581,   582,   583,   584,   585,   586,   587,
     588,   589,   590,   591,   592,   593,   594,   595,   596,   597,
     598,   599,   600,   601,   602,   603,   604,   605,   606,   607,
     608,   609,   610,   611,   612,   613,   614,   615,   616,   617,
     618,   619,   620,   621,   622,   623,   624,   625,   626,   627,
     628,   629,   630,   631,   632,   633,   634,   635,   636,   637,
     638,   639,   640,   641,   642,   643,   644,   645,   646,   647,
     648,   649,   650,   651,   652,   653,   654,   655,   656,   657,
     658,   659,   660,   661,   662,   663,   664,   665,   666,   667,
     668,   669,   670,   671,   672,   673,   674,   675,   676,   677,
     678,   679,   680,   681,   682,   683,   684,   685,   686,   687,
     688,   689,   690,   691,   692,   693,   694,   695,   696,   697,
     698,   699,   700,   700,   701,   701,   701,   701,   701,   701,
     701,   701,   702,   703,   704,   705,   706,   707,   708,   709,
     710,   711,   711,   712,   712,   712,   712,   712,   712,   712,
     712,   712,   712,   712,   712,   712,   712,   712,   712,   712,
     712,   712,   712,   713,   714,   715,   716,   717,   718,   719,
     720,   721,   722,   723,   724,   725,   726,   727,   728,   729,
     730,   731,   732,   733,   734,   734,   735,   736,   737,   738,
     738,   739,   740,   741,   742,   743,   744,   745,   746,   746,
     747,   747,   747,   747,   747,   747,   747,   747,   747,   747,
     748,   749,   750,   751,   752,   753,   754,   755,   756,   757,
     758,   759,   759,   760,   760,   760,   760,   760,   760,   760,
     760,   760,   760,   760,   760,   760,   760,   760,   760,   760,
     760,   760,   760,   760,   761,   762,   763,   764,   765,   766,
     767,   768,   769,   770,   771,   772,   773,   774,   775,   776,
     777,   778,   779,   780,   781,   782,   783,   784,   785,   786,
     787,   788,   789,   790,   791,   792,   793,   794,   794,   795,
     795,   796,   797
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     2,     2,     1,     1,     2,     2,     2,
       1,     2,     2,     2,     2,     2,     2,     1,     1,     1,
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
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     1,     2,     0,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     1,
       2,     0,     1,     1,     1,     1,     1,     1,     1,     2,
       1,     2,     0,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     0,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     2,     2,     2,     2,
       2,     1,     2,     0,     1,     1,     1,     1,     1,     1,
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
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     3,     3,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     3,     2,     2,     2,     2,     2,     2,
       2,     2,     3,     3,     2,     2,     2,     2,     2,     2,
       3,     3,     4,     4,     4,     3,     3,     4,     4,     3,
       3,     2,     2,     2,     2,     2,     2,     2,     3,     3,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     3,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     3,     3,     3,     2,     2,
       2,     1,     2,     0,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     2,     2,     2,     2,     2,     2,     2,
       1,     2,     0,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     1,     2,     0,     1,     2,     1,     2,
       0,     1,     2,     2,     2,     3,     3,     1,     2,     0,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       1,     2,     0,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     3,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     0,     1,
       1,     2,     2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


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
    YYNOMEM;
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
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
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
#line 235 "util/configparser.y"
        {
		OUTYY(("\nP(force-toplevel)\n"));
		cfg_parser->started_toplevel = 0;
	}
#line 3077 "util/configparser.c"
    break;

  case 19: /* serverstart: VAR_SERVER  */
#line 242 "util/configparser.y"
        {
		OUTYY(("\nP(server:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 3086 "util/configparser.c"
    break;

  case 286: /* stub_clause: stubstart contents_stub  */
#line 369 "util/configparser.y"
        {
		/* stub end */
		if(cfg_parser->cfg->stubs &&
			!cfg_parser->cfg->stubs->name)
			yyerror("stub-zone without name");
	}
#line 3097 "util/configparser.c"
    break;

  case 287: /* stubstart: VAR_STUB_ZONE  */
#line 377 "util/configparser.y"
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
#line 3114 "util/configparser.c"
    break;

  case 298: /* forward_clause: forwardstart contents_forward  */
#line 396 "util/configparser.y"
        {
		/* forward end */
		if(cfg_parser->cfg->forwards &&
			!cfg_parser->cfg->forwards->name)
			yyerror("forward-zone without name");
	}
#line 3125 "util/configparser.c"
    break;

  case 299: /* forwardstart: VAR_FORWARD_ZONE  */
#line 404 "util/configparser.y"
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
#line 3142 "util/configparser.c"
    break;

  case 309: /* view_clause: viewstart contents_view  */
#line 423 "util/configparser.y"
        {
		/* view end */
		if(cfg_parser->cfg->views &&
			!cfg_parser->cfg->views->name)
			yyerror("view without name");
	}
#line 3153 "util/configparser.c"
    break;

  case 310: /* viewstart: VAR_VIEW  */
#line 431 "util/configparser.y"
        {
		struct config_view* s;
		OUTYY(("\nP(view:)\n"));
		cfg_parser->started_toplevel = 1;
		s = (struct config_view*)calloc(1, sizeof(struct config_view));
		if(s) {
			s->next = cfg_parser->cfg->views;
			cfg_parser->cfg->views = s;
		} else {
			yyerror("out of memory");
		}
	}
#line 3170 "util/configparser.c"
    break;

  case 320: /* authstart: VAR_AUTH_ZONE  */
#line 450 "util/configparser.y"
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
			s->max_transfer_size = 0;
			s->max_transfer_time = 0;
		} else {
			yyerror("out of memory");
		}
	}
#line 3196 "util/configparser.c"
    break;

  case 335: /* rpz_tag: VAR_TAGS STRING_ARG  */
#line 481 "util/configparser.y"
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
#line 3217 "util/configparser.c"
    break;

  case 336: /* rpz_action_override: VAR_RPZ_ACTION_OVERRIDE STRING_ARG  */
#line 500 "util/configparser.y"
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
#line 3236 "util/configparser.c"
    break;

  case 337: /* rpz_cname_override: VAR_RPZ_CNAME_OVERRIDE STRING_ARG  */
#line 517 "util/configparser.y"
        {
		OUTYY(("P(rpz_cname_override:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->auths->rpz_cname);
		cfg_parser->cfg->auths->rpz_cname = (yyvsp[0].str);
	}
#line 3246 "util/configparser.c"
    break;

  case 338: /* rpz_log: VAR_RPZ_LOG STRING_ARG  */
#line 525 "util/configparser.y"
        {
		OUTYY(("P(rpz_log:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->rpz_log = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3258 "util/configparser.c"
    break;

  case 339: /* rpz_log_name: VAR_RPZ_LOG_NAME STRING_ARG  */
#line 535 "util/configparser.y"
        {
		OUTYY(("P(rpz_log_name:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->auths->rpz_log_name);
		cfg_parser->cfg->auths->rpz_log_name = (yyvsp[0].str);
	}
#line 3268 "util/configparser.c"
    break;

  case 340: /* rpz_signal_nxdomain_ra: VAR_RPZ_SIGNAL_NXDOMAIN_RA STRING_ARG  */
#line 542 "util/configparser.y"
        {
		OUTYY(("P(rpz_signal_nxdomain_ra:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->rpz_signal_nxdomain_ra = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3280 "util/configparser.c"
    break;

  case 341: /* rpzstart: VAR_RPZ  */
#line 552 "util/configparser.y"
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
			s->max_transfer_size = 0;
			s->max_transfer_time = 0;
		} else {
			yyerror("out of memory");
		}
	}
#line 3304 "util/configparser.c"
    break;

  case 358: /* server_num_threads: VAR_NUM_THREADS STRING_ARG  */
#line 580 "util/configparser.y"
        {
		OUTYY(("P(server_num_threads:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->num_threads = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3316 "util/configparser.c"
    break;

  case 359: /* server_verbosity: VAR_VERBOSITY STRING_ARG  */
#line 589 "util/configparser.y"
        {
		OUTYY(("P(server_verbosity:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->verbosity = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3328 "util/configparser.c"
    break;

  case 360: /* server_statistics_interval: VAR_STATISTICS_INTERVAL STRING_ARG  */
#line 598 "util/configparser.y"
        {
		OUTYY(("P(server_statistics_interval:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "") == 0 || strcmp((yyvsp[0].str), "0") == 0)
			cfg_parser->cfg->stat_interval = 0;
		else if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->stat_interval = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3342 "util/configparser.c"
    break;

  case 361: /* server_statistics_cumulative: VAR_STATISTICS_CUMULATIVE STRING_ARG  */
#line 609 "util/configparser.y"
        {
		OUTYY(("P(server_statistics_cumulative:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_cumulative = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3354 "util/configparser.c"
    break;

  case 362: /* server_extended_statistics: VAR_EXTENDED_STATISTICS STRING_ARG  */
#line 618 "util/configparser.y"
        {
		OUTYY(("P(server_extended_statistics:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_extended = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3366 "util/configparser.c"
    break;

  case 363: /* server_statistics_inhibit_zero: VAR_STATISTICS_INHIBIT_ZERO STRING_ARG  */
#line 627 "util/configparser.y"
        {
		OUTYY(("P(server_statistics_inhibit_zero:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_inhibit_zero = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3378 "util/configparser.c"
    break;

  case 364: /* server_shm_enable: VAR_SHM_ENABLE STRING_ARG  */
#line 636 "util/configparser.y"
        {
		OUTYY(("P(server_shm_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->shm_enable = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3390 "util/configparser.c"
    break;

  case 365: /* server_shm_key: VAR_SHM_KEY STRING_ARG  */
#line 645 "util/configparser.y"
        {
		OUTYY(("P(server_shm_key:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "") == 0 || strcmp((yyvsp[0].str), "0") == 0)
			cfg_parser->cfg->shm_key = 0;
		else if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->shm_key = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3404 "util/configparser.c"
    break;

  case 366: /* server_port: VAR_PORT STRING_ARG  */
#line 656 "util/configparser.y"
        {
		OUTYY(("P(server_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3416 "util/configparser.c"
    break;

  case 367: /* server_send_client_subnet: VAR_SEND_CLIENT_SUBNET STRING_ARG  */
#line 665 "util/configparser.y"
        {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(server_send_client_subnet:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->client_subnet, (yyvsp[0].str)))
			yyerror("out of memory");
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 3431 "util/configparser.c"
    break;

  case 368: /* server_client_subnet_zone: VAR_CLIENT_SUBNET_ZONE STRING_ARG  */
#line 677 "util/configparser.y"
        {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(server_client_subnet_zone:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->client_subnet_zone,
			(yyvsp[0].str)))
			yyerror("out of memory");
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 3447 "util/configparser.c"
    break;

  case 369: /* server_client_subnet_always_forward: VAR_CLIENT_SUBNET_ALWAYS_FORWARD STRING_ARG  */
#line 691 "util/configparser.y"
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
#line 3465 "util/configparser.c"
    break;

  case 370: /* server_client_subnet_opcode: VAR_CLIENT_SUBNET_OPCODE STRING_ARG  */
#line 706 "util/configparser.y"
        {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(client_subnet_opcode:%s)\n", (yyvsp[0].str)));
		OUTYY(("P(Deprecated option, ignoring)\n"));
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 3479 "util/configparser.c"
    break;

  case 371: /* server_max_client_subnet_ipv4: VAR_MAX_CLIENT_SUBNET_IPV4 STRING_ARG  */
#line 717 "util/configparser.y"
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
#line 3499 "util/configparser.c"
    break;

  case 372: /* server_max_client_subnet_ipv6: VAR_MAX_CLIENT_SUBNET_IPV6 STRING_ARG  */
#line 734 "util/configparser.y"
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
#line 3519 "util/configparser.c"
    break;

  case 373: /* server_min_client_subnet_ipv4: VAR_MIN_CLIENT_SUBNET_IPV4 STRING_ARG  */
#line 751 "util/configparser.y"
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
#line 3539 "util/configparser.c"
    break;

  case 374: /* server_min_client_subnet_ipv6: VAR_MIN_CLIENT_SUBNET_IPV6 STRING_ARG  */
#line 768 "util/configparser.y"
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
#line 3559 "util/configparser.c"
    break;

  case 375: /* server_max_ecs_tree_size_ipv4: VAR_MAX_ECS_TREE_SIZE_IPV4 STRING_ARG  */
#line 785 "util/configparser.y"
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
#line 3577 "util/configparser.c"
    break;

  case 376: /* server_max_ecs_tree_size_ipv6: VAR_MAX_ECS_TREE_SIZE_IPV6 STRING_ARG  */
#line 800 "util/configparser.y"
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
#line 3595 "util/configparser.c"
    break;

  case 377: /* server_interface: VAR_INTERFACE STRING_ARG  */
#line 815 "util/configparser.y"
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
#line 3611 "util/configparser.c"
    break;

  case 378: /* server_outgoing_interface: VAR_OUTGOING_INTERFACE STRING_ARG  */
#line 828 "util/configparser.y"
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
#line 3629 "util/configparser.c"
    break;

  case 379: /* server_outgoing_range: VAR_OUTGOING_RANGE STRING_ARG  */
#line 843 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_range:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_num_ports = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3641 "util/configparser.c"
    break;

  case 380: /* server_outgoing_port_permit: VAR_OUTGOING_PORT_PERMIT STRING_ARG  */
#line 852 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_port_permit:%s)\n", (yyvsp[0].str)));
		if(!cfg_mark_ports((yyvsp[0].str), 1,
			cfg_parser->cfg->outgoing_avail_ports, 65536))
			yyerror("port number or range (\"low-high\") expected");
		free((yyvsp[0].str));
	}
#line 3653 "util/configparser.c"
    break;

  case 381: /* server_outgoing_port_avoid: VAR_OUTGOING_PORT_AVOID STRING_ARG  */
#line 861 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_port_avoid:%s)\n", (yyvsp[0].str)));
		if(!cfg_mark_ports((yyvsp[0].str), 0,
			cfg_parser->cfg->outgoing_avail_ports, 65536))
			yyerror("port number or range (\"low-high\") expected");
		free((yyvsp[0].str));
	}
#line 3665 "util/configparser.c"
    break;

  case 382: /* server_outgoing_num_tcp: VAR_OUTGOING_NUM_TCP STRING_ARG  */
#line 870 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_num_tcp:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_num_tcp = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3677 "util/configparser.c"
    break;

  case 383: /* server_incoming_num_tcp: VAR_INCOMING_NUM_TCP STRING_ARG  */
#line 879 "util/configparser.y"
        {
		OUTYY(("P(server_incoming_num_tcp:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->incoming_num_tcp = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3689 "util/configparser.c"
    break;

  case 384: /* server_interface_automatic: VAR_INTERFACE_AUTOMATIC STRING_ARG  */
#line 888 "util/configparser.y"
        {
		OUTYY(("P(server_interface_automatic:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->if_automatic = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3701 "util/configparser.c"
    break;

  case 385: /* server_interface_automatic_ports: VAR_INTERFACE_AUTOMATIC_PORTS STRING_ARG  */
#line 897 "util/configparser.y"
        {
		OUTYY(("P(server_interface_automatic_ports:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->if_automatic_ports);
		cfg_parser->cfg->if_automatic_ports = (yyvsp[0].str);
	}
#line 3711 "util/configparser.c"
    break;

  case 386: /* server_do_ip4: VAR_DO_IP4 STRING_ARG  */
#line 904 "util/configparser.y"
        {
		OUTYY(("P(server_do_ip4:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_ip4 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3723 "util/configparser.c"
    break;

  case 387: /* server_do_ip6: VAR_DO_IP6 STRING_ARG  */
#line 913 "util/configparser.y"
        {
		OUTYY(("P(server_do_ip6:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_ip6 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3735 "util/configparser.c"
    break;

  case 388: /* server_do_nat64: VAR_DO_NAT64 STRING_ARG  */
#line 922 "util/configparser.y"
        {
		OUTYY(("P(server_do_nat64:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_nat64 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3747 "util/configparser.c"
    break;

  case 389: /* server_do_udp: VAR_DO_UDP STRING_ARG  */
#line 931 "util/configparser.y"
        {
		OUTYY(("P(server_do_udp:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_udp = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3759 "util/configparser.c"
    break;

  case 390: /* server_do_tcp: VAR_DO_TCP STRING_ARG  */
#line 940 "util/configparser.y"
        {
		OUTYY(("P(server_do_tcp:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_tcp = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3771 "util/configparser.c"
    break;

  case 391: /* server_prefer_ip4: VAR_PREFER_IP4 STRING_ARG  */
#line 949 "util/configparser.y"
        {
		OUTYY(("P(server_prefer_ip4:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefer_ip4 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3783 "util/configparser.c"
    break;

  case 392: /* server_prefer_ip6: VAR_PREFER_IP6 STRING_ARG  */
#line 958 "util/configparser.y"
        {
		OUTYY(("P(server_prefer_ip6:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefer_ip6 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3795 "util/configparser.c"
    break;

  case 393: /* server_tcp_mss: VAR_TCP_MSS STRING_ARG  */
#line 967 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_mss:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->tcp_mss = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3807 "util/configparser.c"
    break;

  case 394: /* server_outgoing_tcp_mss: VAR_OUTGOING_TCP_MSS STRING_ARG  */
#line 976 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_tcp_mss:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_tcp_mss = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3819 "util/configparser.c"
    break;

  case 395: /* server_tcp_idle_timeout: VAR_TCP_IDLE_TIMEOUT STRING_ARG  */
#line 985 "util/configparser.y"
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
#line 3835 "util/configparser.c"
    break;

  case 396: /* server_max_reuse_tcp_queries: VAR_MAX_REUSE_TCP_QUERIES STRING_ARG  */
#line 998 "util/configparser.y"
        {
		OUTYY(("P(server_max_reuse_tcp_queries:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 1)
			cfg_parser->cfg->max_reuse_tcp_queries = 0;
		else cfg_parser->cfg->max_reuse_tcp_queries = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3849 "util/configparser.c"
    break;

  case 397: /* server_tcp_reuse_timeout: VAR_TCP_REUSE_TIMEOUT STRING_ARG  */
#line 1009 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_reuse_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 1)
			cfg_parser->cfg->tcp_reuse_timeout = 0;
		else cfg_parser->cfg->tcp_reuse_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3863 "util/configparser.c"
    break;

  case 398: /* server_tcp_auth_query_timeout: VAR_TCP_AUTH_QUERY_TIMEOUT STRING_ARG  */
#line 1020 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_auth_query_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 1)
			cfg_parser->cfg->tcp_auth_query_timeout = 0;
		else cfg_parser->cfg->tcp_auth_query_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3877 "util/configparser.c"
    break;

  case 399: /* server_tcp_keepalive: VAR_EDNS_TCP_KEEPALIVE STRING_ARG  */
#line 1031 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_keepalive:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_tcp_keepalive = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3889 "util/configparser.c"
    break;

  case 400: /* server_tcp_keepalive_timeout: VAR_EDNS_TCP_KEEPALIVE_TIMEOUT STRING_ARG  */
#line 1040 "util/configparser.y"
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
#line 3905 "util/configparser.c"
    break;

  case 401: /* server_sock_queue_timeout: VAR_SOCK_QUEUE_TIMEOUT STRING_ARG  */
#line 1053 "util/configparser.y"
        {
		OUTYY(("P(server_sock_queue_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) > 6553500)
			cfg_parser->cfg->sock_queue_timeout = 6553500;
		else if (atoi((yyvsp[0].str)) < 1)
			cfg_parser->cfg->sock_queue_timeout = 0;
		else cfg_parser->cfg->sock_queue_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3921 "util/configparser.c"
    break;

  case 402: /* server_tcp_upstream: VAR_TCP_UPSTREAM STRING_ARG  */
#line 1066 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tcp_upstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3933 "util/configparser.c"
    break;

  case 403: /* server_udp_upstream_without_downstream: VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM STRING_ARG  */
#line 1075 "util/configparser.y"
        {
		OUTYY(("P(server_udp_upstream_without_downstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->udp_upstream_without_downstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3945 "util/configparser.c"
    break;

  case 404: /* server_ssl_upstream: VAR_SSL_UPSTREAM STRING_ARG  */
#line 1084 "util/configparser.y"
        {
		OUTYY(("P(server_ssl_upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ssl_upstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3957 "util/configparser.c"
    break;

  case 405: /* server_ssl_service_key: VAR_SSL_SERVICE_KEY STRING_ARG  */
#line 1093 "util/configparser.y"
        {
		OUTYY(("P(server_ssl_service_key:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->ssl_service_key);
		cfg_parser->cfg->ssl_service_key = (yyvsp[0].str);
	}
#line 3967 "util/configparser.c"
    break;

  case 406: /* server_ssl_service_pem: VAR_SSL_SERVICE_PEM STRING_ARG  */
#line 1100 "util/configparser.y"
        {
		OUTYY(("P(server_ssl_service_pem:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->ssl_service_pem);
		cfg_parser->cfg->ssl_service_pem = (yyvsp[0].str);
	}
#line 3977 "util/configparser.c"
    break;

  case 407: /* server_ssl_port: VAR_SSL_PORT STRING_ARG  */
#line 1107 "util/configparser.y"
        {
		OUTYY(("P(server_ssl_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->ssl_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3989 "util/configparser.c"
    break;

  case 408: /* server_tls_cert_bundle: VAR_TLS_CERT_BUNDLE STRING_ARG  */
#line 1116 "util/configparser.y"
        {
		OUTYY(("P(server_tls_cert_bundle:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->tls_cert_bundle);
		cfg_parser->cfg->tls_cert_bundle = (yyvsp[0].str);
	}
#line 3999 "util/configparser.c"
    break;

  case 409: /* server_tls_win_cert: VAR_TLS_WIN_CERT STRING_ARG  */
#line 1123 "util/configparser.y"
        {
		OUTYY(("P(server_tls_win_cert:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tls_win_cert = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4011 "util/configparser.c"
    break;

  case 410: /* server_tls_additional_port: VAR_TLS_ADDITIONAL_PORT STRING_ARG  */
#line 1132 "util/configparser.y"
        {
		OUTYY(("P(server_tls_additional_port:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->tls_additional_port,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4022 "util/configparser.c"
    break;

  case 411: /* server_tls_ciphers: VAR_TLS_CIPHERS STRING_ARG  */
#line 1140 "util/configparser.y"
        {
		OUTYY(("P(server_tls_ciphers:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->tls_ciphers);
		cfg_parser->cfg->tls_ciphers = (yyvsp[0].str);
	}
#line 4032 "util/configparser.c"
    break;

  case 412: /* server_tls_ciphersuites: VAR_TLS_CIPHERSUITES STRING_ARG  */
#line 1147 "util/configparser.y"
        {
		OUTYY(("P(server_tls_ciphersuites:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->tls_ciphersuites);
		cfg_parser->cfg->tls_ciphersuites = (yyvsp[0].str);
	}
#line 4042 "util/configparser.c"
    break;

  case 413: /* server_tls_session_ticket_keys: VAR_TLS_SESSION_TICKET_KEYS STRING_ARG  */
#line 1154 "util/configparser.y"
        {
		OUTYY(("P(server_tls_session_ticket_keys:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append(&cfg_parser->cfg->tls_session_ticket_keys,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4053 "util/configparser.c"
    break;

  case 414: /* server_tls_use_sni: VAR_TLS_USE_SNI STRING_ARG  */
#line 1162 "util/configparser.y"
        {
		OUTYY(("P(server_tls_use_sni:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tls_use_sni = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4065 "util/configparser.c"
    break;

  case 415: /* server_tls_protocols: VAR_TLS_PROTOCOLS STRING_ARG  */
#line 1171 "util/configparser.y"
        {
		OUTYY(("P(server_tls_protocols:%s)\n", (yyvsp[0].str)));
		if(!cfg_tls_protocols_is_valid((yyvsp[0].str)))
			yyerror("tls-protocols: valid values are 'TLSv1.2' and 'TLSv1.3'.");
		free(cfg_parser->cfg->tls_protocols);
		cfg_parser->cfg->tls_protocols = (yyvsp[0].str);
	}
#line 4077 "util/configparser.c"
    break;

  case 416: /* server_https_port: VAR_HTTPS_PORT STRING_ARG  */
#line 1180 "util/configparser.y"
        {
		OUTYY(("P(server_https_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->https_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4089 "util/configparser.c"
    break;

  case 417: /* server_http_endpoint: VAR_HTTP_ENDPOINT STRING_ARG  */
#line 1188 "util/configparser.y"
        {
		OUTYY(("P(server_http_endpoint:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->http_endpoint);
		if((yyvsp[0].str) && (yyvsp[0].str)[0] != '/') {
			cfg_parser->cfg->http_endpoint = malloc(strlen((yyvsp[0].str))+2);
			if(cfg_parser->cfg->http_endpoint) {
				cfg_parser->cfg->http_endpoint[0] = '/';
				memmove(cfg_parser->cfg->http_endpoint+1, (yyvsp[0].str),
					strlen((yyvsp[0].str))+1);
			} else {
				yyerror("out of memory");
			}
			free((yyvsp[0].str));
		} else {
			cfg_parser->cfg->http_endpoint = (yyvsp[0].str);
		}
	}
#line 4111 "util/configparser.c"
    break;

  case 418: /* server_http_max_streams: VAR_HTTP_MAX_STREAMS STRING_ARG  */
#line 1206 "util/configparser.y"
        {
		OUTYY(("P(server_http_max_streams:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->http_max_streams = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4123 "util/configparser.c"
    break;

  case 419: /* server_http_query_buffer_size: VAR_HTTP_QUERY_BUFFER_SIZE STRING_ARG  */
#line 1214 "util/configparser.y"
        {
		OUTYY(("P(server_http_query_buffer_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str),
			&cfg_parser->cfg->http_query_buffer_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4135 "util/configparser.c"
    break;

  case 420: /* server_http_response_buffer_size: VAR_HTTP_RESPONSE_BUFFER_SIZE STRING_ARG  */
#line 1222 "util/configparser.y"
        {
		OUTYY(("P(server_http_response_buffer_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str),
			&cfg_parser->cfg->http_response_buffer_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4147 "util/configparser.c"
    break;

  case 421: /* server_http_nodelay: VAR_HTTP_NODELAY STRING_ARG  */
#line 1230 "util/configparser.y"
        {
		OUTYY(("P(server_http_nodelay:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->http_nodelay = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4159 "util/configparser.c"
    break;

  case 422: /* server_http_notls_downstream: VAR_HTTP_NOTLS_DOWNSTREAM STRING_ARG  */
#line 1238 "util/configparser.y"
        {
		OUTYY(("P(server_http_notls_downstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->http_notls_downstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4171 "util/configparser.c"
    break;

  case 423: /* server_quic_port: VAR_QUIC_PORT STRING_ARG  */
#line 1246 "util/configparser.y"
        {
		OUTYY(("P(server_quic_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str),"0")!=0)
			yyerror("port number expected");
		else {
			cfg_parser->cfg->quic_port = atoi((yyvsp[0].str));
#ifndef HAVE_NGTCP2
			if (cfg_parser->cfg->quic_port != 0)
				log_warn("%s:%d: Unbound is not compiled with "
					"ngtcp2. This is required to use DNS "
					"over QUIC.", cfg_parser->filename, cfg_parser->line);
#endif
		}
		free((yyvsp[0].str));
	}
#line 4191 "util/configparser.c"
    break;

  case 424: /* server_quic_size: VAR_QUIC_SIZE STRING_ARG  */
#line 1262 "util/configparser.y"
        {
		OUTYY(("P(server_quic_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->quic_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4202 "util/configparser.c"
    break;

  case 425: /* server_use_systemd: VAR_USE_SYSTEMD STRING_ARG  */
#line 1269 "util/configparser.y"
        {
		OUTYY(("P(server_use_systemd:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->use_systemd = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4214 "util/configparser.c"
    break;

  case 426: /* server_do_daemonize: VAR_DO_DAEMONIZE STRING_ARG  */
#line 1278 "util/configparser.y"
        {
		OUTYY(("P(server_do_daemonize:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_daemonize = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4226 "util/configparser.c"
    break;

  case 427: /* server_use_syslog: VAR_USE_SYSLOG STRING_ARG  */
#line 1287 "util/configparser.y"
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
#line 4243 "util/configparser.c"
    break;

  case 428: /* server_log_time_ascii: VAR_LOG_TIME_ASCII STRING_ARG  */
#line 1301 "util/configparser.y"
        {
		OUTYY(("P(server_log_time_ascii:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_time_ascii = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4255 "util/configparser.c"
    break;

  case 429: /* server_log_time_iso: VAR_LOG_TIME_ISO STRING_ARG  */
#line 1310 "util/configparser.y"
        {
		OUTYY(("P(server_log_time_iso:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_time_iso = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4267 "util/configparser.c"
    break;

  case 430: /* server_log_queries: VAR_LOG_QUERIES STRING_ARG  */
#line 1319 "util/configparser.y"
        {
		OUTYY(("P(server_log_queries:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_queries = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4279 "util/configparser.c"
    break;

  case 431: /* server_log_replies: VAR_LOG_REPLIES STRING_ARG  */
#line 1328 "util/configparser.y"
        {
		OUTYY(("P(server_log_replies:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_replies = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4291 "util/configparser.c"
    break;

  case 432: /* server_log_tag_queryreply: VAR_LOG_TAG_QUERYREPLY STRING_ARG  */
#line 1337 "util/configparser.y"
        {
		OUTYY(("P(server_log_tag_queryreply:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_tag_queryreply = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4303 "util/configparser.c"
    break;

  case 433: /* server_log_servfail: VAR_LOG_SERVFAIL STRING_ARG  */
#line 1346 "util/configparser.y"
        {
		OUTYY(("P(server_log_servfail:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_servfail = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4315 "util/configparser.c"
    break;

  case 434: /* server_log_destaddr: VAR_LOG_DESTADDR STRING_ARG  */
#line 1355 "util/configparser.y"
        {
		OUTYY(("P(server_log_destaddr:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_destaddr = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4327 "util/configparser.c"
    break;

  case 435: /* server_log_thread_id: VAR_LOG_THREAD_ID STRING_ARG  */
#line 1364 "util/configparser.y"
        {
		OUTYY(("P(server_log_thread_id:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_thread_id = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4339 "util/configparser.c"
    break;

  case 436: /* server_log_local_actions: VAR_LOG_LOCAL_ACTIONS STRING_ARG  */
#line 1373 "util/configparser.y"
        {
		OUTYY(("P(server_log_local_actions:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_local_actions = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4351 "util/configparser.c"
    break;

  case 437: /* server_chroot: VAR_CHROOT STRING_ARG  */
#line 1382 "util/configparser.y"
        {
		OUTYY(("P(server_chroot:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->chrootdir);
		cfg_parser->cfg->chrootdir = (yyvsp[0].str);
	}
#line 4361 "util/configparser.c"
    break;

  case 438: /* server_username: VAR_USERNAME STRING_ARG  */
#line 1389 "util/configparser.y"
        {
		OUTYY(("P(server_username:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->username);
		cfg_parser->cfg->username = (yyvsp[0].str);
	}
#line 4371 "util/configparser.c"
    break;

  case 439: /* server_directory: VAR_DIRECTORY STRING_ARG  */
#line 1396 "util/configparser.y"
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
#line 4400 "util/configparser.c"
    break;

  case 440: /* server_logfile: VAR_LOGFILE STRING_ARG  */
#line 1422 "util/configparser.y"
        {
		OUTYY(("P(server_logfile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->logfile);
		cfg_parser->cfg->logfile = (yyvsp[0].str);
		cfg_parser->cfg->use_syslog = 0;
	}
#line 4411 "util/configparser.c"
    break;

  case 441: /* server_pidfile: VAR_PIDFILE STRING_ARG  */
#line 1430 "util/configparser.y"
        {
		OUTYY(("P(server_pidfile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->pidfile);
		cfg_parser->cfg->pidfile = (yyvsp[0].str);
	}
#line 4421 "util/configparser.c"
    break;

  case 442: /* server_root_hints: VAR_ROOT_HINTS STRING_ARG  */
#line 1437 "util/configparser.y"
        {
		OUTYY(("P(server_root_hints:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->root_hints, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4431 "util/configparser.c"
    break;

  case 443: /* server_dlv_anchor_file: VAR_DLV_ANCHOR_FILE STRING_ARG  */
#line 1444 "util/configparser.y"
        {
		OUTYY(("P(server_dlv_anchor_file:%s)\n", (yyvsp[0].str)));
		log_warn("option dlv-anchor-file ignored: DLV is decommissioned");
		free((yyvsp[0].str));
	}
#line 4441 "util/configparser.c"
    break;

  case 444: /* server_dlv_anchor: VAR_DLV_ANCHOR STRING_ARG  */
#line 1451 "util/configparser.y"
        {
		OUTYY(("P(server_dlv_anchor:%s)\n", (yyvsp[0].str)));
		log_warn("option dlv-anchor ignored: DLV is decommissioned");
		free((yyvsp[0].str));
	}
#line 4451 "util/configparser.c"
    break;

  case 445: /* server_auto_trust_anchor_file: VAR_AUTO_TRUST_ANCHOR_FILE STRING_ARG  */
#line 1458 "util/configparser.y"
        {
		OUTYY(("P(server_auto_trust_anchor_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			auto_trust_anchor_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4462 "util/configparser.c"
    break;

  case 446: /* server_trust_anchor_file: VAR_TRUST_ANCHOR_FILE STRING_ARG  */
#line 1466 "util/configparser.y"
        {
		OUTYY(("P(server_trust_anchor_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			trust_anchor_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4473 "util/configparser.c"
    break;

  case 447: /* server_trusted_keys_file: VAR_TRUSTED_KEYS_FILE STRING_ARG  */
#line 1474 "util/configparser.y"
        {
		OUTYY(("P(server_trusted_keys_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			trusted_keys_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4484 "util/configparser.c"
    break;

  case 448: /* server_trust_anchor: VAR_TRUST_ANCHOR STRING_ARG  */
#line 1482 "util/configparser.y"
        {
		OUTYY(("P(server_trust_anchor:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->trust_anchor_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4494 "util/configparser.c"
    break;

  case 449: /* server_trust_anchor_signaling: VAR_TRUST_ANCHOR_SIGNALING STRING_ARG  */
#line 1489 "util/configparser.y"
        {
		OUTYY(("P(server_trust_anchor_signaling:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->trust_anchor_signaling =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4508 "util/configparser.c"
    break;

  case 450: /* server_root_key_sentinel: VAR_ROOT_KEY_SENTINEL STRING_ARG  */
#line 1500 "util/configparser.y"
        {
		OUTYY(("P(server_root_key_sentinel:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->root_key_sentinel =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4522 "util/configparser.c"
    break;

  case 451: /* server_domain_insecure: VAR_DOMAIN_INSECURE STRING_ARG  */
#line 1511 "util/configparser.y"
        {
		OUTYY(("P(server_domain_insecure:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->domain_insecure, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4532 "util/configparser.c"
    break;

  case 452: /* server_hide_identity: VAR_HIDE_IDENTITY STRING_ARG  */
#line 1518 "util/configparser.y"
        {
		OUTYY(("P(server_hide_identity:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_identity = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4544 "util/configparser.c"
    break;

  case 453: /* server_hide_version: VAR_HIDE_VERSION STRING_ARG  */
#line 1527 "util/configparser.y"
        {
		OUTYY(("P(server_hide_version:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_version = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4556 "util/configparser.c"
    break;

  case 454: /* server_hide_trustanchor: VAR_HIDE_TRUSTANCHOR STRING_ARG  */
#line 1536 "util/configparser.y"
        {
		OUTYY(("P(server_hide_trustanchor:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_trustanchor = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4568 "util/configparser.c"
    break;

  case 455: /* server_hide_http_user_agent: VAR_HIDE_HTTP_USER_AGENT STRING_ARG  */
#line 1545 "util/configparser.y"
        {
		OUTYY(("P(server_hide_user_agent:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_http_user_agent = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4580 "util/configparser.c"
    break;

  case 456: /* server_identity: VAR_IDENTITY STRING_ARG  */
#line 1554 "util/configparser.y"
        {
		OUTYY(("P(server_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->identity);
		cfg_parser->cfg->identity = (yyvsp[0].str);
	}
#line 4590 "util/configparser.c"
    break;

  case 457: /* server_version: VAR_VERSION STRING_ARG  */
#line 1561 "util/configparser.y"
        {
		OUTYY(("P(server_version:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->version);
		cfg_parser->cfg->version = (yyvsp[0].str);
	}
#line 4600 "util/configparser.c"
    break;

  case 458: /* server_http_user_agent: VAR_HTTP_USER_AGENT STRING_ARG  */
#line 1568 "util/configparser.y"
        {
		OUTYY(("P(server_http_user_agent:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->http_user_agent);
		cfg_parser->cfg->http_user_agent = (yyvsp[0].str);
	}
#line 4610 "util/configparser.c"
    break;

  case 459: /* server_nsid: VAR_NSID STRING_ARG  */
#line 1575 "util/configparser.y"
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
#line 4629 "util/configparser.c"
    break;

  case 460: /* server_so_rcvbuf: VAR_SO_RCVBUF STRING_ARG  */
#line 1591 "util/configparser.y"
        {
		OUTYY(("P(server_so_rcvbuf:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->so_rcvbuf))
			yyerror("buffer size expected");
		free((yyvsp[0].str));
	}
#line 4640 "util/configparser.c"
    break;

  case 461: /* server_so_sndbuf: VAR_SO_SNDBUF STRING_ARG  */
#line 1599 "util/configparser.y"
        {
		OUTYY(("P(server_so_sndbuf:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->so_sndbuf))
			yyerror("buffer size expected");
		free((yyvsp[0].str));
	}
#line 4651 "util/configparser.c"
    break;

  case 462: /* server_so_reuseport: VAR_SO_REUSEPORT STRING_ARG  */
#line 1607 "util/configparser.y"
        {
		OUTYY(("P(server_so_reuseport:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->so_reuseport =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4664 "util/configparser.c"
    break;

  case 463: /* server_ip_transparent: VAR_IP_TRANSPARENT STRING_ARG  */
#line 1617 "util/configparser.y"
        {
		OUTYY(("P(server_ip_transparent:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ip_transparent =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4677 "util/configparser.c"
    break;

  case 464: /* server_ip_freebind: VAR_IP_FREEBIND STRING_ARG  */
#line 1627 "util/configparser.y"
        {
		OUTYY(("P(server_ip_freebind:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ip_freebind =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4690 "util/configparser.c"
    break;

  case 465: /* server_ip_dscp: VAR_IP_DSCP STRING_ARG  */
#line 1637 "util/configparser.y"
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
#line 4707 "util/configparser.c"
    break;

  case 466: /* server_stream_wait_size: VAR_STREAM_WAIT_SIZE STRING_ARG  */
#line 1651 "util/configparser.y"
        {
		OUTYY(("P(server_stream_wait_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->stream_wait_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4718 "util/configparser.c"
    break;

  case 467: /* server_edns_buffer_size: VAR_EDNS_BUFFER_SIZE STRING_ARG  */
#line 1659 "util/configparser.y"
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
#line 4734 "util/configparser.c"
    break;

  case 468: /* server_msg_buffer_size: VAR_MSG_BUFFER_SIZE STRING_ARG  */
#line 1672 "util/configparser.y"
        {
		OUTYY(("P(server_msg_buffer_size:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 4096)
			yyerror("message buffer size too small (use 4096)");
		else cfg_parser->cfg->msg_buffer_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4748 "util/configparser.c"
    break;

  case 469: /* server_msg_cache_size: VAR_MSG_CACHE_SIZE STRING_ARG  */
#line 1683 "util/configparser.y"
        {
		OUTYY(("P(server_msg_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->msg_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4759 "util/configparser.c"
    break;

  case 470: /* server_msg_cache_slabs: VAR_MSG_CACHE_SLABS STRING_ARG  */
#line 1691 "util/configparser.y"
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
#line 4775 "util/configparser.c"
    break;

  case 471: /* server_num_queries_per_thread: VAR_NUM_QUERIES_PER_THREAD STRING_ARG  */
#line 1704 "util/configparser.y"
        {
		OUTYY(("P(server_num_queries_per_thread:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->num_queries_per_thread = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4787 "util/configparser.c"
    break;

  case 472: /* server_jostle_timeout: VAR_JOSTLE_TIMEOUT STRING_ARG  */
#line 1713 "util/configparser.y"
        {
		OUTYY(("P(server_jostle_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->jostle_time = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4799 "util/configparser.c"
    break;

  case 473: /* server_delay_close: VAR_DELAY_CLOSE STRING_ARG  */
#line 1722 "util/configparser.y"
        {
		OUTYY(("P(server_delay_close:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->delay_close = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4811 "util/configparser.c"
    break;

  case 474: /* server_udp_connect: VAR_UDP_CONNECT STRING_ARG  */
#line 1731 "util/configparser.y"
        {
		OUTYY(("P(server_udp_connect:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->udp_connect = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4823 "util/configparser.c"
    break;

  case 475: /* server_unblock_lan_zones: VAR_UNBLOCK_LAN_ZONES STRING_ARG  */
#line 1740 "util/configparser.y"
        {
		OUTYY(("P(server_unblock_lan_zones:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->unblock_lan_zones =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4836 "util/configparser.c"
    break;

  case 476: /* server_insecure_lan_zones: VAR_INSECURE_LAN_ZONES STRING_ARG  */
#line 1750 "util/configparser.y"
        {
		OUTYY(("P(server_insecure_lan_zones:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->insecure_lan_zones =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4849 "util/configparser.c"
    break;

  case 477: /* server_rrset_cache_size: VAR_RRSET_CACHE_SIZE STRING_ARG  */
#line 1760 "util/configparser.y"
        {
		OUTYY(("P(server_rrset_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->rrset_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4860 "util/configparser.c"
    break;

  case 478: /* server_rrset_cache_slabs: VAR_RRSET_CACHE_SLABS STRING_ARG  */
#line 1768 "util/configparser.y"
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
#line 4876 "util/configparser.c"
    break;

  case 479: /* server_infra_host_ttl: VAR_INFRA_HOST_TTL STRING_ARG  */
#line 1781 "util/configparser.y"
        {
		OUTYY(("P(server_infra_host_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->host_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4888 "util/configparser.c"
    break;

  case 480: /* server_infra_lame_ttl: VAR_INFRA_LAME_TTL STRING_ARG  */
#line 1790 "util/configparser.y"
        {
		OUTYY(("P(server_infra_lame_ttl:%s)\n", (yyvsp[0].str)));
		verbose(VERB_DETAIL, "ignored infra-lame-ttl: %s (option "
			"removed, use infra-host-ttl)", (yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4899 "util/configparser.c"
    break;

  case 481: /* server_infra_cache_numhosts: VAR_INFRA_CACHE_NUMHOSTS STRING_ARG  */
#line 1798 "util/configparser.y"
        {
		OUTYY(("P(server_infra_cache_numhosts:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_numhosts = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4911 "util/configparser.c"
    break;

  case 482: /* server_infra_cache_lame_size: VAR_INFRA_CACHE_LAME_SIZE STRING_ARG  */
#line 1807 "util/configparser.y"
        {
		OUTYY(("P(server_infra_cache_lame_size:%s)\n", (yyvsp[0].str)));
		verbose(VERB_DETAIL, "ignored infra-cache-lame-size: %s "
			"(option removed, use infra-cache-numhosts)", (yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4922 "util/configparser.c"
    break;

  case 483: /* server_infra_cache_slabs: VAR_INFRA_CACHE_SLABS STRING_ARG  */
#line 1815 "util/configparser.y"
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
#line 4938 "util/configparser.c"
    break;

  case 484: /* server_infra_cache_min_rtt: VAR_INFRA_CACHE_MIN_RTT STRING_ARG  */
#line 1828 "util/configparser.y"
        {
		OUTYY(("P(server_infra_cache_min_rtt:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_min_rtt = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4950 "util/configparser.c"
    break;

  case 485: /* server_infra_cache_max_rtt: VAR_INFRA_CACHE_MAX_RTT STRING_ARG  */
#line 1837 "util/configparser.y"
        {
		OUTYY(("P(server_infra_cache_max_rtt:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_max_rtt = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4962 "util/configparser.c"
    break;

  case 486: /* server_infra_keep_probing: VAR_INFRA_KEEP_PROBING STRING_ARG  */
#line 1846 "util/configparser.y"
        {
		OUTYY(("P(server_infra_keep_probing:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->infra_keep_probing =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4975 "util/configparser.c"
    break;

  case 487: /* server_target_fetch_policy: VAR_TARGET_FETCH_POLICY STRING_ARG  */
#line 1856 "util/configparser.y"
        {
		OUTYY(("P(server_target_fetch_policy:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->target_fetch_policy);
		cfg_parser->cfg->target_fetch_policy = (yyvsp[0].str);
	}
#line 4985 "util/configparser.c"
    break;

  case 488: /* server_harden_short_bufsize: VAR_HARDEN_SHORT_BUFSIZE STRING_ARG  */
#line 1863 "util/configparser.y"
        {
		OUTYY(("P(server_harden_short_bufsize:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_short_bufsize =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4998 "util/configparser.c"
    break;

  case 489: /* server_harden_large_queries: VAR_HARDEN_LARGE_QUERIES STRING_ARG  */
#line 1873 "util/configparser.y"
        {
		OUTYY(("P(server_harden_large_queries:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_large_queries =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5011 "util/configparser.c"
    break;

  case 490: /* server_harden_glue: VAR_HARDEN_GLUE STRING_ARG  */
#line 1883 "util/configparser.y"
        {
		OUTYY(("P(server_harden_glue:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_glue =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5024 "util/configparser.c"
    break;

  case 491: /* server_harden_unverified_glue: VAR_HARDEN_UNVERIFIED_GLUE STRING_ARG  */
#line 1893 "util/configparser.y"
       {
               OUTYY(("P(server_harden_unverified_glue:%s)\n", (yyvsp[0].str)));
               if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
                       yyerror("expected yes or no.");
               else cfg_parser->cfg->harden_unverified_glue =
                       (strcmp((yyvsp[0].str), "yes")==0);
               free((yyvsp[0].str));
       }
#line 5037 "util/configparser.c"
    break;

  case 492: /* server_harden_dnssec_stripped: VAR_HARDEN_DNSSEC_STRIPPED STRING_ARG  */
#line 1903 "util/configparser.y"
        {
		OUTYY(("P(server_harden_dnssec_stripped:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_dnssec_stripped =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5050 "util/configparser.c"
    break;

  case 493: /* server_harden_below_nxdomain: VAR_HARDEN_BELOW_NXDOMAIN STRING_ARG  */
#line 1913 "util/configparser.y"
        {
		OUTYY(("P(server_harden_below_nxdomain:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_below_nxdomain =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5063 "util/configparser.c"
    break;

  case 494: /* server_harden_referral_path: VAR_HARDEN_REFERRAL_PATH STRING_ARG  */
#line 1923 "util/configparser.y"
        {
		OUTYY(("P(server_harden_referral_path:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_referral_path =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5076 "util/configparser.c"
    break;

  case 495: /* server_harden_algo_downgrade: VAR_HARDEN_ALGO_DOWNGRADE STRING_ARG  */
#line 1933 "util/configparser.y"
        {
		OUTYY(("P(server_harden_algo_downgrade:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_algo_downgrade =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5089 "util/configparser.c"
    break;

  case 496: /* server_harden_unknown_additional: VAR_HARDEN_UNKNOWN_ADDITIONAL STRING_ARG  */
#line 1943 "util/configparser.y"
        {
		OUTYY(("P(server_harden_unknown_additional:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_unknown_additional =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5102 "util/configparser.c"
    break;

  case 497: /* server_use_caps_for_id: VAR_USE_CAPS_FOR_ID STRING_ARG  */
#line 1953 "util/configparser.y"
        {
		OUTYY(("P(server_use_caps_for_id:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->use_caps_bits_for_id =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5115 "util/configparser.c"
    break;

  case 498: /* server_caps_whitelist: VAR_CAPS_WHITELIST STRING_ARG  */
#line 1963 "util/configparser.y"
        {
		OUTYY(("P(server_caps_whitelist:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->caps_whitelist, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5125 "util/configparser.c"
    break;

  case 499: /* server_private_address: VAR_PRIVATE_ADDRESS STRING_ARG  */
#line 1970 "util/configparser.y"
        {
		OUTYY(("P(server_private_address:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->private_address, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5135 "util/configparser.c"
    break;

  case 500: /* server_private_domain: VAR_PRIVATE_DOMAIN STRING_ARG  */
#line 1977 "util/configparser.y"
        {
		OUTYY(("P(server_private_domain:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->private_domain, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5145 "util/configparser.c"
    break;

  case 501: /* server_prefetch: VAR_PREFETCH STRING_ARG  */
#line 1984 "util/configparser.y"
        {
		OUTYY(("P(server_prefetch:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefetch = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5157 "util/configparser.c"
    break;

  case 502: /* server_prefetch_key: VAR_PREFETCH_KEY STRING_ARG  */
#line 1993 "util/configparser.y"
        {
		OUTYY(("P(server_prefetch_key:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefetch_key = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5169 "util/configparser.c"
    break;

  case 503: /* server_deny_any: VAR_DENY_ANY STRING_ARG  */
#line 2002 "util/configparser.y"
        {
		OUTYY(("P(server_deny_any:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->deny_any = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5181 "util/configparser.c"
    break;

  case 504: /* server_unwanted_reply_threshold: VAR_UNWANTED_REPLY_THRESHOLD STRING_ARG  */
#line 2011 "util/configparser.y"
        {
		OUTYY(("P(server_unwanted_reply_threshold:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->unwanted_threshold = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5193 "util/configparser.c"
    break;

  case 505: /* server_do_not_query_address: VAR_DO_NOT_QUERY_ADDRESS STRING_ARG  */
#line 2020 "util/configparser.y"
        {
		OUTYY(("P(server_do_not_query_address:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->donotqueryaddrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5203 "util/configparser.c"
    break;

  case 506: /* server_do_not_query_localhost: VAR_DO_NOT_QUERY_LOCALHOST STRING_ARG  */
#line 2027 "util/configparser.y"
        {
		OUTYY(("P(server_do_not_query_localhost:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->donotquery_localhost =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5216 "util/configparser.c"
    break;

  case 507: /* server_access_control: VAR_ACCESS_CONTROL STRING_ARG STRING_ARG  */
#line 2037 "util/configparser.y"
        {
		OUTYY(("P(server_access_control:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_acl_action((yyvsp[0].str));
		if(!cfg_str2list_insert(&cfg_parser->cfg->acls, (yyvsp[-1].str), (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5227 "util/configparser.c"
    break;

  case 508: /* server_interface_action: VAR_INTERFACE_ACTION STRING_ARG STRING_ARG  */
#line 2045 "util/configparser.y"
        {
		OUTYY(("P(server_interface_action:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_acl_action((yyvsp[0].str));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->interface_actions, (yyvsp[-1].str), (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5239 "util/configparser.c"
    break;

  case 509: /* server_module_conf: VAR_MODULE_CONF STRING_ARG  */
#line 2054 "util/configparser.y"
        {
		OUTYY(("P(server_module_conf:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->module_conf);
		cfg_parser->cfg->module_conf = (yyvsp[0].str);
	}
#line 5249 "util/configparser.c"
    break;

  case 510: /* server_val_override_date: VAR_VAL_OVERRIDE_DATE STRING_ARG  */
#line 2061 "util/configparser.y"
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
#line 5270 "util/configparser.c"
    break;

  case 511: /* server_val_sig_skew_min: VAR_VAL_SIG_SKEW_MIN STRING_ARG  */
#line 2079 "util/configparser.y"
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
#line 5286 "util/configparser.c"
    break;

  case 512: /* server_val_sig_skew_max: VAR_VAL_SIG_SKEW_MAX STRING_ARG  */
#line 2092 "util/configparser.y"
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
#line 5302 "util/configparser.c"
    break;

  case 513: /* server_val_max_restart: VAR_VAL_MAX_RESTART STRING_ARG  */
#line 2105 "util/configparser.y"
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
#line 5318 "util/configparser.c"
    break;

  case 514: /* server_cache_max_ttl: VAR_CACHE_MAX_TTL STRING_ARG  */
#line 2118 "util/configparser.y"
        {
		OUTYY(("P(server_cache_max_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5330 "util/configparser.c"
    break;

  case 515: /* server_cache_max_negative_ttl: VAR_CACHE_MAX_NEGATIVE_TTL STRING_ARG  */
#line 2127 "util/configparser.y"
        {
		OUTYY(("P(server_cache_max_negative_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_negative_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5342 "util/configparser.c"
    break;

  case 516: /* server_cache_min_negative_ttl: VAR_CACHE_MIN_NEGATIVE_TTL STRING_ARG  */
#line 2136 "util/configparser.y"
        {
		OUTYY(("P(server_cache_min_negative_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->min_negative_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5354 "util/configparser.c"
    break;

  case 517: /* server_cache_min_ttl: VAR_CACHE_MIN_TTL STRING_ARG  */
#line 2145 "util/configparser.y"
        {
		OUTYY(("P(server_cache_min_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->min_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5366 "util/configparser.c"
    break;

  case 518: /* server_bogus_ttl: VAR_BOGUS_TTL STRING_ARG  */
#line 2154 "util/configparser.y"
        {
		OUTYY(("P(server_bogus_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->bogus_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5378 "util/configparser.c"
    break;

  case 519: /* server_val_clean_additional: VAR_VAL_CLEAN_ADDITIONAL STRING_ARG  */
#line 2163 "util/configparser.y"
        {
		OUTYY(("P(server_val_clean_additional:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->val_clean_additional =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5391 "util/configparser.c"
    break;

  case 520: /* server_val_permissive_mode: VAR_VAL_PERMISSIVE_MODE STRING_ARG  */
#line 2173 "util/configparser.y"
        {
		OUTYY(("P(server_val_permissive_mode:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->val_permissive_mode =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5404 "util/configparser.c"
    break;

  case 521: /* server_aggressive_nsec: VAR_AGGRESSIVE_NSEC STRING_ARG  */
#line 2183 "util/configparser.y"
        {
		OUTYY(("P(server_aggressive_nsec:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->aggressive_nsec =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5418 "util/configparser.c"
    break;

  case 522: /* server_ignore_cd_flag: VAR_IGNORE_CD_FLAG STRING_ARG  */
#line 2194 "util/configparser.y"
        {
		OUTYY(("P(server_ignore_cd_flag:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ignore_cd = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5430 "util/configparser.c"
    break;

  case 523: /* server_disable_edns_do: VAR_DISABLE_EDNS_DO STRING_ARG  */
#line 2203 "util/configparser.y"
        {
		OUTYY(("P(server_disable_edns_do:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->disable_edns_do = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5442 "util/configparser.c"
    break;

  case 524: /* server_serve_expired: VAR_SERVE_EXPIRED STRING_ARG  */
#line 2212 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->serve_expired = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5454 "util/configparser.c"
    break;

  case 525: /* server_serve_expired_ttl: VAR_SERVE_EXPIRED_TTL STRING_ARG  */
#line 2221 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->serve_expired_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5466 "util/configparser.c"
    break;

  case 526: /* server_serve_expired_ttl_reset: VAR_SERVE_EXPIRED_TTL_RESET STRING_ARG  */
#line 2230 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired_ttl_reset:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->serve_expired_ttl_reset = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5478 "util/configparser.c"
    break;

  case 527: /* server_serve_expired_reply_ttl: VAR_SERVE_EXPIRED_REPLY_TTL STRING_ARG  */
#line 2239 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired_reply_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->serve_expired_reply_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5490 "util/configparser.c"
    break;

  case 528: /* server_serve_expired_client_timeout: VAR_SERVE_EXPIRED_CLIENT_TIMEOUT STRING_ARG  */
#line 2248 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired_client_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->serve_expired_client_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5502 "util/configparser.c"
    break;

  case 529: /* server_ede_serve_expired: VAR_EDE_SERVE_EXPIRED STRING_ARG  */
#line 2257 "util/configparser.y"
        {
		OUTYY(("P(server_ede_serve_expired:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ede_serve_expired = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5514 "util/configparser.c"
    break;

  case 530: /* server_serve_original_ttl: VAR_SERVE_ORIGINAL_TTL STRING_ARG  */
#line 2266 "util/configparser.y"
        {
		OUTYY(("P(server_serve_original_ttl:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->serve_original_ttl = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5526 "util/configparser.c"
    break;

  case 531: /* server_fake_dsa: VAR_FAKE_DSA STRING_ARG  */
#line 2275 "util/configparser.y"
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
#line 5542 "util/configparser.c"
    break;

  case 532: /* server_fake_sha1: VAR_FAKE_SHA1 STRING_ARG  */
#line 2288 "util/configparser.y"
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
#line 5558 "util/configparser.c"
    break;

  case 533: /* server_val_log_level: VAR_VAL_LOG_LEVEL STRING_ARG  */
#line 2301 "util/configparser.y"
        {
		OUTYY(("P(server_val_log_level:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->val_log_level = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5570 "util/configparser.c"
    break;

  case 534: /* server_val_nsec3_keysize_iterations: VAR_VAL_NSEC3_KEYSIZE_ITERATIONS STRING_ARG  */
#line 2310 "util/configparser.y"
        {
		OUTYY(("P(server_val_nsec3_keysize_iterations:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->val_nsec3_key_iterations);
		cfg_parser->cfg->val_nsec3_key_iterations = (yyvsp[0].str);
	}
#line 5580 "util/configparser.c"
    break;

  case 535: /* server_zonemd_permissive_mode: VAR_ZONEMD_PERMISSIVE_MODE STRING_ARG  */
#line 2317 "util/configparser.y"
        {
		OUTYY(("P(server_zonemd_permissive_mode:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else	cfg_parser->cfg->zonemd_permissive_mode = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5592 "util/configparser.c"
    break;

  case 536: /* server_add_holddown: VAR_ADD_HOLDDOWN STRING_ARG  */
#line 2326 "util/configparser.y"
        {
		OUTYY(("P(server_add_holddown:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->add_holddown = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5604 "util/configparser.c"
    break;

  case 537: /* server_del_holddown: VAR_DEL_HOLDDOWN STRING_ARG  */
#line 2335 "util/configparser.y"
        {
		OUTYY(("P(server_del_holddown:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->del_holddown = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5616 "util/configparser.c"
    break;

  case 538: /* server_keep_missing: VAR_KEEP_MISSING STRING_ARG  */
#line 2344 "util/configparser.y"
        {
		OUTYY(("P(server_keep_missing:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->keep_missing = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5628 "util/configparser.c"
    break;

  case 539: /* server_permit_small_holddown: VAR_PERMIT_SMALL_HOLDDOWN STRING_ARG  */
#line 2353 "util/configparser.y"
        {
		OUTYY(("P(server_permit_small_holddown:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->permit_small_holddown =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5641 "util/configparser.c"
    break;

  case 540: /* server_key_cache_size: VAR_KEY_CACHE_SIZE STRING_ARG  */
#line 2363 "util/configparser.y"
        {
		OUTYY(("P(server_key_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->key_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 5652 "util/configparser.c"
    break;

  case 541: /* server_key_cache_slabs: VAR_KEY_CACHE_SLABS STRING_ARG  */
#line 2371 "util/configparser.y"
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
#line 5668 "util/configparser.c"
    break;

  case 542: /* server_neg_cache_size: VAR_NEG_CACHE_SIZE STRING_ARG  */
#line 2384 "util/configparser.y"
        {
		OUTYY(("P(server_neg_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->neg_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 5679 "util/configparser.c"
    break;

  case 543: /* server_local_zone: VAR_LOCAL_ZONE STRING_ARG STRING_ARG  */
#line 2392 "util/configparser.y"
        {
		OUTYY(("P(server_local_zone:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "static")!=0 && strcmp((yyvsp[0].str), "deny")!=0 &&
		   strcmp((yyvsp[0].str), "refuse")!=0 && strcmp((yyvsp[0].str), "redirect")!=0 &&
		   strcmp((yyvsp[0].str), "transparent")!=0 && strcmp((yyvsp[0].str), "nodefault")!=0
		   && strcmp((yyvsp[0].str), "typetransparent")!=0
		   && strcmp((yyvsp[0].str), "always_transparent")!=0
		   && strcmp((yyvsp[0].str), "block_a")!=0
		   && strcmp((yyvsp[0].str), "block_aaaa")!=0
		   && strcmp((yyvsp[0].str), "block_a_wdata")!=0
		   && strcmp((yyvsp[0].str), "block_aaaa_wdata")!=0
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
				"block_a, block_aaaa, "
				"block_a_wdata, block_aaaa_wdata, "
				"always_refuse, always_nxdomain, "
				"always_nodata, always_deny, always_null, "
				"noview, nodefault or ipset");
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		} else if(strcmp((yyvsp[0].str), "nodefault")==0) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->
				local_zones_nodefault, (yyvsp[-1].str)))
				yyerror("out of memory");
			free((yyvsp[0].str));
#ifdef USE_IPSET
		} else if(strcmp((yyvsp[0].str), "ipset")==0) {
			size_t len = strlen((yyvsp[-1].str));
			/* Make sure to add the trailing dot.
			 * These are str compared to domain names. */
			if((yyvsp[-1].str)[len-1] != '.') {
				char* prev = (yyvsp[-1].str);
				if(!((yyvsp[-1].str) = realloc((yyvsp[-1].str), len+2))) {
					yyerror("out of memory");
					free(prev);
				} else {
					(yyvsp[-1].str)[len] = '.';
					(yyvsp[-1].str)[len+1] = 0;
				}
			}
			if(!cfg_strlist_insert(&cfg_parser->cfg->
				local_zones_ipset, (yyvsp[-1].str)))
				yyerror("out of memory");
			free((yyvsp[0].str));
#endif
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->local_zones,
				(yyvsp[-1].str), (yyvsp[0].str)))
				yyerror("out of memory");
		}
	}
#line 5746 "util/configparser.c"
    break;

  case 544: /* server_local_data: VAR_LOCAL_DATA STRING_ARG  */
#line 2456 "util/configparser.y"
        {
		OUTYY(("P(server_local_data:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->local_data, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5756 "util/configparser.c"
    break;

  case 545: /* server_local_data_ptr: VAR_LOCAL_DATA_PTR STRING_ARG  */
#line 2463 "util/configparser.y"
        {
		char* ptr;
		OUTYY(("P(server_local_data_ptr:%s)\n", (yyvsp[0].str)));
		ptr = cfg_ptr_reverse((yyvsp[0].str));
		free((yyvsp[0].str));
		if(ptr) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->
				local_data, ptr))
				yyerror("out of memory");
		} else {
			yyerror("local-data-ptr could not be reversed");
		}
	}
#line 5774 "util/configparser.c"
    break;

  case 546: /* server_minimal_responses: VAR_MINIMAL_RESPONSES STRING_ARG  */
#line 2478 "util/configparser.y"
        {
		OUTYY(("P(server_minimal_responses:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->minimal_responses =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5787 "util/configparser.c"
    break;

  case 547: /* server_rrset_roundrobin: VAR_RRSET_ROUNDROBIN STRING_ARG  */
#line 2488 "util/configparser.y"
        {
		OUTYY(("P(server_rrset_roundrobin:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->rrset_roundrobin =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5800 "util/configparser.c"
    break;

  case 548: /* server_unknown_server_time_limit: VAR_UNKNOWN_SERVER_TIME_LIMIT STRING_ARG  */
#line 2498 "util/configparser.y"
        {
		OUTYY(("P(server_unknown_server_time_limit:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->unknown_server_time_limit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5810 "util/configparser.c"
    break;

  case 549: /* server_discard_timeout: VAR_DISCARD_TIMEOUT STRING_ARG  */
#line 2505 "util/configparser.y"
        {
		OUTYY(("P(server_discard_timeout:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->discard_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5820 "util/configparser.c"
    break;

  case 550: /* server_wait_limit: VAR_WAIT_LIMIT STRING_ARG  */
#line 2512 "util/configparser.y"
        {
		OUTYY(("P(server_wait_limit:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->wait_limit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5830 "util/configparser.c"
    break;

  case 551: /* server_wait_limit_cookie: VAR_WAIT_LIMIT_COOKIE STRING_ARG  */
#line 2519 "util/configparser.y"
        {
		OUTYY(("P(server_wait_limit_cookie:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->wait_limit_cookie = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5840 "util/configparser.c"
    break;

  case 552: /* server_wait_limit_netblock: VAR_WAIT_LIMIT_NETBLOCK STRING_ARG STRING_ARG  */
#line 2526 "util/configparser.y"
        {
		OUTYY(("P(server_wait_limit_netblock:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0) {
			yyerror("number expected");
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->
				wait_limit_netblock, (yyvsp[-1].str), (yyvsp[0].str)))
				yyerror("out of memory");
		}
	}
#line 5857 "util/configparser.c"
    break;

  case 553: /* server_wait_limit_cookie_netblock: VAR_WAIT_LIMIT_COOKIE_NETBLOCK STRING_ARG STRING_ARG  */
#line 2540 "util/configparser.y"
        {
		OUTYY(("P(server_wait_limit_cookie_netblock:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0) {
			yyerror("number expected");
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->
				wait_limit_cookie_netblock, (yyvsp[-1].str), (yyvsp[0].str)))
				yyerror("out of memory");
		}
	}
#line 5874 "util/configparser.c"
    break;

  case 554: /* server_max_udp_size: VAR_MAX_UDP_SIZE STRING_ARG  */
#line 2554 "util/configparser.y"
        {
		OUTYY(("P(server_max_udp_size:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->max_udp_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5884 "util/configparser.c"
    break;

  case 555: /* server_dns64_prefix: VAR_DNS64_PREFIX STRING_ARG  */
#line 2561 "util/configparser.y"
        {
		OUTYY(("P(dns64_prefix:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dns64_prefix);
		cfg_parser->cfg->dns64_prefix = (yyvsp[0].str);
	}
#line 5894 "util/configparser.c"
    break;

  case 556: /* server_dns64_synthall: VAR_DNS64_SYNTHALL STRING_ARG  */
#line 2568 "util/configparser.y"
        {
		OUTYY(("P(server_dns64_synthall:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dns64_synthall = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5906 "util/configparser.c"
    break;

  case 557: /* server_dns64_ignore_aaaa: VAR_DNS64_IGNORE_AAAA STRING_ARG  */
#line 2577 "util/configparser.y"
        {
		OUTYY(("P(dns64_ignore_aaaa:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dns64_ignore_aaaa,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 5917 "util/configparser.c"
    break;

  case 558: /* server_nat64_prefix: VAR_NAT64_PREFIX STRING_ARG  */
#line 2585 "util/configparser.y"
        {
		OUTYY(("P(nat64_prefix:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->nat64_prefix);
		cfg_parser->cfg->nat64_prefix = (yyvsp[0].str);
	}
#line 5927 "util/configparser.c"
    break;

  case 559: /* server_define_tag: VAR_DEFINE_TAG STRING_ARG  */
#line 2592 "util/configparser.y"
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
#line 5944 "util/configparser.c"
    break;

  case 560: /* server_local_zone_tag: VAR_LOCAL_ZONE_TAG STRING_ARG STRING_ARG  */
#line 2606 "util/configparser.y"
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
#line 5968 "util/configparser.c"
    break;

  case 561: /* server_access_control_tag: VAR_ACCESS_CONTROL_TAG STRING_ARG STRING_ARG  */
#line 2627 "util/configparser.y"
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
#line 5992 "util/configparser.c"
    break;

  case 562: /* server_access_control_tag_action: VAR_ACCESS_CONTROL_TAG_ACTION STRING_ARG STRING_ARG STRING_ARG  */
#line 2648 "util/configparser.y"
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
#line 6007 "util/configparser.c"
    break;

  case 563: /* server_access_control_tag_data: VAR_ACCESS_CONTROL_TAG_DATA STRING_ARG STRING_ARG STRING_ARG  */
#line 2660 "util/configparser.y"
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
#line 6022 "util/configparser.c"
    break;

  case 564: /* server_local_zone_override: VAR_LOCAL_ZONE_OVERRIDE STRING_ARG STRING_ARG STRING_ARG  */
#line 2672 "util/configparser.y"
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
#line 6037 "util/configparser.c"
    break;

  case 565: /* server_access_control_view: VAR_ACCESS_CONTROL_VIEW STRING_ARG STRING_ARG  */
#line 2684 "util/configparser.y"
        {
		OUTYY(("P(server_access_control_view:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str2list_insert(&cfg_parser->cfg->acl_view,
			(yyvsp[-1].str), (yyvsp[0].str))) {
			yyerror("out of memory");
		}
	}
#line 6049 "util/configparser.c"
    break;

  case 566: /* server_interface_tag: VAR_INTERFACE_TAG STRING_ARG STRING_ARG  */
#line 2693 "util/configparser.y"
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
#line 6073 "util/configparser.c"
    break;

  case 567: /* server_interface_tag_action: VAR_INTERFACE_TAG_ACTION STRING_ARG STRING_ARG STRING_ARG  */
#line 2714 "util/configparser.y"
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
#line 6088 "util/configparser.c"
    break;

  case 568: /* server_interface_tag_data: VAR_INTERFACE_TAG_DATA STRING_ARG STRING_ARG STRING_ARG  */
#line 2726 "util/configparser.y"
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
#line 6103 "util/configparser.c"
    break;

  case 569: /* server_interface_view: VAR_INTERFACE_VIEW STRING_ARG STRING_ARG  */
#line 2738 "util/configparser.y"
        {
		OUTYY(("P(server_interface_view:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str2list_insert(&cfg_parser->cfg->interface_view,
			(yyvsp[-1].str), (yyvsp[0].str))) {
			yyerror("out of memory");
		}
	}
#line 6115 "util/configparser.c"
    break;

  case 570: /* server_response_ip_tag: VAR_RESPONSE_IP_TAG STRING_ARG STRING_ARG  */
#line 2747 "util/configparser.y"
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
#line 6139 "util/configparser.c"
    break;

  case 571: /* server_ip_ratelimit: VAR_IP_RATELIMIT STRING_ARG  */
#line 2768 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ip_ratelimit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6151 "util/configparser.c"
    break;

  case 572: /* server_ip_ratelimit_cookie: VAR_IP_RATELIMIT_COOKIE STRING_ARG  */
#line 2777 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit_cookie:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ip_ratelimit_cookie = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6163 "util/configparser.c"
    break;

  case 573: /* server_ratelimit: VAR_RATELIMIT STRING_ARG  */
#line 2786 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ratelimit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6175 "util/configparser.c"
    break;

  case 574: /* server_ip_ratelimit_size: VAR_IP_RATELIMIT_SIZE STRING_ARG  */
#line 2795 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->ip_ratelimit_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 6186 "util/configparser.c"
    break;

  case 575: /* server_ratelimit_size: VAR_RATELIMIT_SIZE STRING_ARG  */
#line 2803 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->ratelimit_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 6197 "util/configparser.c"
    break;

  case 576: /* server_ip_ratelimit_slabs: VAR_IP_RATELIMIT_SLABS STRING_ARG  */
#line 2811 "util/configparser.y"
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
#line 6213 "util/configparser.c"
    break;

  case 577: /* server_ratelimit_slabs: VAR_RATELIMIT_SLABS STRING_ARG  */
#line 2824 "util/configparser.y"
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
#line 6229 "util/configparser.c"
    break;

  case 578: /* server_ratelimit_for_domain: VAR_RATELIMIT_FOR_DOMAIN STRING_ARG STRING_ARG  */
#line 2837 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit_for_domain:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0) {
			yyerror("number expected");
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->
				ratelimit_for_domain, (yyvsp[-1].str), (yyvsp[0].str)))
				yyerror("out of memory");
		}
	}
#line 6246 "util/configparser.c"
    break;

  case 579: /* server_ratelimit_below_domain: VAR_RATELIMIT_BELOW_DOMAIN STRING_ARG STRING_ARG  */
#line 2851 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit_below_domain:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0) {
			yyerror("number expected");
			free((yyvsp[-1].str));
			free((yyvsp[0].str));
		} else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->
				ratelimit_below_domain, (yyvsp[-1].str), (yyvsp[0].str)))
				yyerror("out of memory");
		}
	}
#line 6263 "util/configparser.c"
    break;

  case 580: /* server_ip_ratelimit_factor: VAR_IP_RATELIMIT_FACTOR STRING_ARG  */
#line 2865 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit_factor:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ip_ratelimit_factor = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6275 "util/configparser.c"
    break;

  case 581: /* server_ratelimit_factor: VAR_RATELIMIT_FACTOR STRING_ARG  */
#line 2874 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit_factor:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ratelimit_factor = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6287 "util/configparser.c"
    break;

  case 582: /* server_ip_ratelimit_backoff: VAR_IP_RATELIMIT_BACKOFF STRING_ARG  */
#line 2883 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit_backoff:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ip_ratelimit_backoff =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6300 "util/configparser.c"
    break;

  case 583: /* server_ratelimit_backoff: VAR_RATELIMIT_BACKOFF STRING_ARG  */
#line 2893 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit_backoff:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ratelimit_backoff =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6313 "util/configparser.c"
    break;

  case 584: /* server_outbound_msg_retry: VAR_OUTBOUND_MSG_RETRY STRING_ARG  */
#line 2903 "util/configparser.y"
        {
		OUTYY(("P(server_outbound_msg_retry:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outbound_msg_retry = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6325 "util/configparser.c"
    break;

  case 585: /* server_max_sent_count: VAR_MAX_SENT_COUNT STRING_ARG  */
#line 2912 "util/configparser.y"
        {
		OUTYY(("P(server_max_sent_count:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_sent_count = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6337 "util/configparser.c"
    break;

  case 586: /* server_max_query_restarts: VAR_MAX_QUERY_RESTARTS STRING_ARG  */
#line 2921 "util/configparser.y"
        {
		OUTYY(("P(server_max_query_restarts:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_query_restarts = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6349 "util/configparser.c"
    break;

  case 587: /* server_low_rtt: VAR_LOW_RTT STRING_ARG  */
#line 2930 "util/configparser.y"
        {
		OUTYY(("P(low-rtt option is deprecated, use fast-server-num instead)\n"));
		free((yyvsp[0].str));
	}
#line 6358 "util/configparser.c"
    break;

  case 588: /* server_fast_server_num: VAR_FAST_SERVER_NUM STRING_ARG  */
#line 2936 "util/configparser.y"
        {
		OUTYY(("P(server_fast_server_num:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) <= 0)
			yyerror("number expected");
		else cfg_parser->cfg->fast_server_num = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6370 "util/configparser.c"
    break;

  case 589: /* server_fast_server_permil: VAR_FAST_SERVER_PERMIL STRING_ARG  */
#line 2945 "util/configparser.y"
        {
		OUTYY(("P(server_fast_server_permil:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->fast_server_permil = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6382 "util/configparser.c"
    break;

  case 590: /* server_qname_minimisation: VAR_QNAME_MINIMISATION STRING_ARG  */
#line 2954 "util/configparser.y"
        {
		OUTYY(("P(server_qname_minimisation:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->qname_minimisation =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6395 "util/configparser.c"
    break;

  case 591: /* server_qname_minimisation_strict: VAR_QNAME_MINIMISATION_STRICT STRING_ARG  */
#line 2964 "util/configparser.y"
        {
		OUTYY(("P(server_qname_minimisation_strict:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->qname_minimisation_strict =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6408 "util/configparser.c"
    break;

  case 592: /* server_pad_responses: VAR_PAD_RESPONSES STRING_ARG  */
#line 2974 "util/configparser.y"
        {
		OUTYY(("P(server_pad_responses:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->pad_responses =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6421 "util/configparser.c"
    break;

  case 593: /* server_pad_responses_block_size: VAR_PAD_RESPONSES_BLOCK_SIZE STRING_ARG  */
#line 2984 "util/configparser.y"
        {
		OUTYY(("P(server_pad_responses_block_size:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->pad_responses_block_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6433 "util/configparser.c"
    break;

  case 594: /* server_pad_queries: VAR_PAD_QUERIES STRING_ARG  */
#line 2993 "util/configparser.y"
        {
		OUTYY(("P(server_pad_queries:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->pad_queries =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6446 "util/configparser.c"
    break;

  case 595: /* server_pad_queries_block_size: VAR_PAD_QUERIES_BLOCK_SIZE STRING_ARG  */
#line 3003 "util/configparser.y"
        {
		OUTYY(("P(server_pad_queries_block_size:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->pad_queries_block_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6458 "util/configparser.c"
    break;

  case 596: /* server_ipsecmod_enabled: VAR_IPSECMOD_ENABLED STRING_ARG  */
#line 3012 "util/configparser.y"
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
#line 6474 "util/configparser.c"
    break;

  case 597: /* server_ipsecmod_ignore_bogus: VAR_IPSECMOD_IGNORE_BOGUS STRING_ARG  */
#line 3025 "util/configparser.y"
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
#line 6490 "util/configparser.c"
    break;

  case 598: /* server_ipsecmod_hook: VAR_IPSECMOD_HOOK STRING_ARG  */
#line 3038 "util/configparser.y"
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
#line 6505 "util/configparser.c"
    break;

  case 599: /* server_ipsecmod_max_ttl: VAR_IPSECMOD_MAX_TTL STRING_ARG  */
#line 3050 "util/configparser.y"
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
#line 6522 "util/configparser.c"
    break;

  case 600: /* server_ipsecmod_whitelist: VAR_IPSECMOD_WHITELIST STRING_ARG  */
#line 3064 "util/configparser.y"
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
#line 6537 "util/configparser.c"
    break;

  case 601: /* server_ipsecmod_strict: VAR_IPSECMOD_STRICT STRING_ARG  */
#line 3076 "util/configparser.y"
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
#line 6554 "util/configparser.c"
    break;

  case 602: /* server_edns_client_string: VAR_EDNS_CLIENT_STRING STRING_ARG STRING_ARG  */
#line 3090 "util/configparser.y"
        {
		OUTYY(("P(server_edns_client_string:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->edns_client_strings, (yyvsp[-1].str), (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6565 "util/configparser.c"
    break;

  case 603: /* server_edns_client_string_opcode: VAR_EDNS_CLIENT_STRING_OPCODE STRING_ARG  */
#line 3098 "util/configparser.y"
        {
		OUTYY(("P(edns_client_string_opcode:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("option code expected");
		else if(atoi((yyvsp[0].str)) > 65535 || atoi((yyvsp[0].str)) < 0)
			yyerror("option code must be in interval [0, 65535]");
		else cfg_parser->cfg->edns_client_string_opcode = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6579 "util/configparser.c"
    break;

  case 604: /* server_ede: VAR_EDE STRING_ARG  */
#line 3109 "util/configparser.y"
        {
		OUTYY(("P(server_ede:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ede = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6591 "util/configparser.c"
    break;

  case 605: /* server_dns_error_reporting: VAR_DNS_ERROR_REPORTING STRING_ARG  */
#line 3118 "util/configparser.y"
        {
		OUTYY(("P(server_dns_error_reporting:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dns_error_reporting = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6603 "util/configparser.c"
    break;

  case 606: /* server_proxy_protocol_port: VAR_PROXY_PROTOCOL_PORT STRING_ARG  */
#line 3127 "util/configparser.y"
        {
		OUTYY(("P(server_proxy_protocol_port:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->proxy_protocol_port, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6613 "util/configparser.c"
    break;

  case 607: /* stub_name: VAR_NAME STRING_ARG  */
#line 3134 "util/configparser.y"
        {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->stubs->name)
			yyerror("stub name override, there must be one name "
				"for one stub-zone");
		free(cfg_parser->cfg->stubs->name);
		cfg_parser->cfg->stubs->name = (yyvsp[0].str);
	}
#line 6626 "util/configparser.c"
    break;

  case 608: /* stub_host: VAR_STUB_HOST STRING_ARG  */
#line 3144 "util/configparser.y"
        {
		OUTYY(("P(stub-host:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->stubs->hosts, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6636 "util/configparser.c"
    break;

  case 609: /* stub_addr: VAR_STUB_ADDR STRING_ARG  */
#line 3151 "util/configparser.y"
        {
		OUTYY(("P(stub-addr:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->stubs->addrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6646 "util/configparser.c"
    break;

  case 610: /* stub_first: VAR_STUB_FIRST STRING_ARG  */
#line 3158 "util/configparser.y"
        {
		OUTYY(("P(stub-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6658 "util/configparser.c"
    break;

  case 611: /* stub_no_cache: VAR_STUB_NO_CACHE STRING_ARG  */
#line 3167 "util/configparser.y"
        {
		OUTYY(("P(stub-no-cache:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->no_cache=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6670 "util/configparser.c"
    break;

  case 612: /* stub_ssl_upstream: VAR_STUB_SSL_UPSTREAM STRING_ARG  */
#line 3176 "util/configparser.y"
        {
		OUTYY(("P(stub-ssl-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->ssl_upstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6683 "util/configparser.c"
    break;

  case 613: /* stub_tcp_upstream: VAR_STUB_TCP_UPSTREAM STRING_ARG  */
#line 3186 "util/configparser.y"
        {
                OUTYY(("P(stub-tcp-upstream:%s)\n", (yyvsp[0].str)));
                if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
                        yyerror("expected yes or no.");
                else cfg_parser->cfg->stubs->tcp_upstream =
                        (strcmp((yyvsp[0].str), "yes")==0);
                free((yyvsp[0].str));
        }
#line 6696 "util/configparser.c"
    break;

  case 614: /* stub_prime: VAR_STUB_PRIME STRING_ARG  */
#line 3196 "util/configparser.y"
        {
		OUTYY(("P(stub-prime:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->isprime =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6709 "util/configparser.c"
    break;

  case 615: /* forward_name: VAR_NAME STRING_ARG  */
#line 3206 "util/configparser.y"
        {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->forwards->name)
			yyerror("forward name override, there must be one "
				"name for one forward-zone");
		free(cfg_parser->cfg->forwards->name);
		cfg_parser->cfg->forwards->name = (yyvsp[0].str);
	}
#line 6722 "util/configparser.c"
    break;

  case 616: /* forward_host: VAR_FORWARD_HOST STRING_ARG  */
#line 3216 "util/configparser.y"
        {
		OUTYY(("P(forward-host:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->forwards->hosts, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6732 "util/configparser.c"
    break;

  case 617: /* forward_addr: VAR_FORWARD_ADDR STRING_ARG  */
#line 3223 "util/configparser.y"
        {
		OUTYY(("P(forward-addr:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->forwards->addrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6742 "util/configparser.c"
    break;

  case 618: /* forward_first: VAR_FORWARD_FIRST STRING_ARG  */
#line 3230 "util/configparser.y"
        {
		OUTYY(("P(forward-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6754 "util/configparser.c"
    break;

  case 619: /* forward_no_cache: VAR_FORWARD_NO_CACHE STRING_ARG  */
#line 3239 "util/configparser.y"
        {
		OUTYY(("P(forward-no-cache:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->no_cache=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6766 "util/configparser.c"
    break;

  case 620: /* forward_ssl_upstream: VAR_FORWARD_SSL_UPSTREAM STRING_ARG  */
#line 3248 "util/configparser.y"
        {
		OUTYY(("P(forward-ssl-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->ssl_upstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6779 "util/configparser.c"
    break;

  case 621: /* forward_tcp_upstream: VAR_FORWARD_TCP_UPSTREAM STRING_ARG  */
#line 3258 "util/configparser.y"
        {
                OUTYY(("P(forward-tcp-upstream:%s)\n", (yyvsp[0].str)));
                if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
                        yyerror("expected yes or no.");
                else cfg_parser->cfg->forwards->tcp_upstream =
                        (strcmp((yyvsp[0].str), "yes")==0);
                free((yyvsp[0].str));
        }
#line 6792 "util/configparser.c"
    break;

  case 622: /* auth_name: VAR_NAME STRING_ARG  */
#line 3268 "util/configparser.y"
        {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->auths->name)
			yyerror("auth name override, there must be one name "
				"for one auth-zone");
		free(cfg_parser->cfg->auths->name);
		cfg_parser->cfg->auths->name = (yyvsp[0].str);
	}
#line 6805 "util/configparser.c"
    break;

  case 623: /* auth_zonefile: VAR_ZONEFILE STRING_ARG  */
#line 3278 "util/configparser.y"
        {
		OUTYY(("P(zonefile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->auths->zonefile);
		cfg_parser->cfg->auths->zonefile = (yyvsp[0].str);
	}
#line 6815 "util/configparser.c"
    break;

  case 624: /* auth_master: VAR_MASTER STRING_ARG  */
#line 3285 "util/configparser.y"
        {
		OUTYY(("P(master:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->masters, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6825 "util/configparser.c"
    break;

  case 625: /* auth_url: VAR_URL STRING_ARG  */
#line 3292 "util/configparser.y"
        {
		OUTYY(("P(url:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->urls, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6835 "util/configparser.c"
    break;

  case 626: /* auth_allow_notify: VAR_ALLOW_NOTIFY STRING_ARG  */
#line 3299 "util/configparser.y"
        {
		OUTYY(("P(allow-notify:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->allow_notify,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6846 "util/configparser.c"
    break;

  case 627: /* auth_zonemd_check: VAR_ZONEMD_CHECK STRING_ARG  */
#line 3307 "util/configparser.y"
        {
		OUTYY(("P(zonemd-check:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->zonemd_check =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6859 "util/configparser.c"
    break;

  case 628: /* auth_zonemd_reject_absence: VAR_ZONEMD_REJECT_ABSENCE STRING_ARG  */
#line 3317 "util/configparser.y"
        {
		OUTYY(("P(zonemd-reject-absence:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->zonemd_reject_absence =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6872 "util/configparser.c"
    break;

  case 629: /* auth_for_downstream: VAR_FOR_DOWNSTREAM STRING_ARG  */
#line 3327 "util/configparser.y"
        {
		OUTYY(("P(for-downstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->for_downstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6885 "util/configparser.c"
    break;

  case 630: /* auth_for_upstream: VAR_FOR_UPSTREAM STRING_ARG  */
#line 3337 "util/configparser.y"
        {
		OUTYY(("P(for-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->for_upstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6898 "util/configparser.c"
    break;

  case 631: /* auth_fallback_enabled: VAR_FALLBACK_ENABLED STRING_ARG  */
#line 3347 "util/configparser.y"
        {
		OUTYY(("P(fallback-enabled:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->fallback_enabled =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6911 "util/configparser.c"
    break;

  case 632: /* auth_max_transfer_size: VAR_MAX_TRANSFER_SIZE STRING_ARG  */
#line 3357 "util/configparser.y"
        {
		OUTYY(("P(max-transfer-size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->auths->max_transfer_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 6922 "util/configparser.c"
    break;

  case 633: /* auth_max_transfer_time: VAR_MAX_TRANSFER_TIME STRING_ARG  */
#line 3365 "util/configparser.y"
        {
		OUTYY(("P(max-transfer-time:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->auths->max_transfer_time = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6934 "util/configparser.c"
    break;

  case 634: /* view_name: VAR_NAME STRING_ARG  */
#line 3374 "util/configparser.y"
        {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->views->name)
			yyerror("view name override, there must be one "
				"name for one view");
		free(cfg_parser->cfg->views->name);
		cfg_parser->cfg->views->name = (yyvsp[0].str);
	}
#line 6947 "util/configparser.c"
    break;

  case 635: /* view_local_zone: VAR_LOCAL_ZONE STRING_ARG STRING_ARG  */
#line 3384 "util/configparser.y"
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
				yyerror("out of memory");
			free((yyvsp[0].str));
#ifdef USE_IPSET
		} else if(strcmp((yyvsp[0].str), "ipset")==0) {
			size_t len = strlen((yyvsp[-1].str));
			/* Make sure to add the trailing dot.
			 * These are str compared to domain names. */
			if((yyvsp[-1].str)[len-1] != '.') {
				char* prev = (yyvsp[-1].str);
				if(!((yyvsp[-1].str) = realloc((yyvsp[-1].str), len+2))) {
					yyerror("out of memory");
					free(prev);
				} else {
					(yyvsp[-1].str)[len] = '.';
					(yyvsp[-1].str)[len+1] = 0;
				}
			}
			if(!cfg_strlist_insert(&cfg_parser->cfg->views->
				local_zones_ipset, (yyvsp[-1].str)))
				yyerror("out of memory");
			free((yyvsp[0].str));
#endif
		} else {
			if(!cfg_str2list_insert(
				&cfg_parser->cfg->views->local_zones,
				(yyvsp[-1].str), (yyvsp[0].str)))
				yyerror("out of memory");
		}
	}
#line 7009 "util/configparser.c"
    break;

  case 636: /* view_response_ip: VAR_RESPONSE_IP STRING_ARG STRING_ARG  */
#line 3443 "util/configparser.y"
        {
		OUTYY(("P(view_response_ip:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_respip_action((yyvsp[0].str));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->views->respip_actions, (yyvsp[-1].str), (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 7021 "util/configparser.c"
    break;

  case 637: /* view_response_ip_data: VAR_RESPONSE_IP_DATA STRING_ARG STRING_ARG  */
#line 3452 "util/configparser.y"
        {
		OUTYY(("P(view_response_ip_data:%s)\n", (yyvsp[-1].str)));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->views->respip_data, (yyvsp[-1].str), (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 7032 "util/configparser.c"
    break;

  case 638: /* view_local_data: VAR_LOCAL_DATA STRING_ARG  */
#line 3460 "util/configparser.y"
        {
		OUTYY(("P(view_local_data:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->views->local_data, (yyvsp[0].str))) {
			yyerror("out of memory");
		}
	}
#line 7043 "util/configparser.c"
    break;

  case 639: /* view_local_data_ptr: VAR_LOCAL_DATA_PTR STRING_ARG  */
#line 3468 "util/configparser.y"
        {
		char* ptr;
		OUTYY(("P(view_local_data_ptr:%s)\n", (yyvsp[0].str)));
		ptr = cfg_ptr_reverse((yyvsp[0].str));
		free((yyvsp[0].str));
		if(ptr) {
			if(!cfg_strlist_insert(&cfg_parser->cfg->views->
				local_data, ptr))
				yyerror("out of memory");
		} else {
			yyerror("local-data-ptr could not be reversed");
		}
	}
#line 7061 "util/configparser.c"
    break;

  case 640: /* view_first: VAR_VIEW_FIRST STRING_ARG  */
#line 3483 "util/configparser.y"
        {
		OUTYY(("P(view-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->views->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7073 "util/configparser.c"
    break;

  case 641: /* rcstart: VAR_REMOTE_CONTROL  */
#line 3492 "util/configparser.y"
        {
		OUTYY(("\nP(remote-control:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 7082 "util/configparser.c"
    break;

  case 652: /* rc_control_enable: VAR_CONTROL_ENABLE STRING_ARG  */
#line 3504 "util/configparser.y"
        {
		OUTYY(("P(control_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->remote_control_enable =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7095 "util/configparser.c"
    break;

  case 653: /* rc_control_port: VAR_CONTROL_PORT STRING_ARG  */
#line 3514 "util/configparser.y"
        {
		OUTYY(("P(control_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("control port number expected");
		else cfg_parser->cfg->control_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 7107 "util/configparser.c"
    break;

  case 654: /* rc_control_interface: VAR_CONTROL_INTERFACE STRING_ARG  */
#line 3523 "util/configparser.y"
        {
		OUTYY(("P(control_interface:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append(&cfg_parser->cfg->control_ifs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 7117 "util/configparser.c"
    break;

  case 655: /* rc_control_use_cert: VAR_CONTROL_USE_CERT STRING_ARG  */
#line 3530 "util/configparser.y"
        {
		OUTYY(("P(control_use_cert:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->control_use_cert = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7127 "util/configparser.c"
    break;

  case 656: /* rc_server_key_file: VAR_SERVER_KEY_FILE STRING_ARG  */
#line 3537 "util/configparser.y"
        {
		OUTYY(("P(rc_server_key_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->server_key_file);
		cfg_parser->cfg->server_key_file = (yyvsp[0].str);
	}
#line 7137 "util/configparser.c"
    break;

  case 657: /* rc_server_cert_file: VAR_SERVER_CERT_FILE STRING_ARG  */
#line 3544 "util/configparser.y"
        {
		OUTYY(("P(rc_server_cert_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->server_cert_file);
		cfg_parser->cfg->server_cert_file = (yyvsp[0].str);
	}
#line 7147 "util/configparser.c"
    break;

  case 658: /* rc_control_key_file: VAR_CONTROL_KEY_FILE STRING_ARG  */
#line 3551 "util/configparser.y"
        {
		OUTYY(("P(rc_control_key_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->control_key_file);
		cfg_parser->cfg->control_key_file = (yyvsp[0].str);
	}
#line 7157 "util/configparser.c"
    break;

  case 659: /* rc_control_cert_file: VAR_CONTROL_CERT_FILE STRING_ARG  */
#line 3558 "util/configparser.y"
        {
		OUTYY(("P(rc_control_cert_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->control_cert_file);
		cfg_parser->cfg->control_cert_file = (yyvsp[0].str);
	}
#line 7167 "util/configparser.c"
    break;

  case 660: /* dtstart: VAR_DNSTAP  */
#line 3565 "util/configparser.y"
        {
		OUTYY(("\nP(dnstap:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 7176 "util/configparser.c"
    break;

  case 683: /* dt_dnstap_enable: VAR_DNSTAP_ENABLE STRING_ARG  */
#line 3587 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7188 "util/configparser.c"
    break;

  case 684: /* dt_dnstap_bidirectional: VAR_DNSTAP_BIDIRECTIONAL STRING_ARG  */
#line 3596 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_bidirectional:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_bidirectional =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7201 "util/configparser.c"
    break;

  case 685: /* dt_dnstap_socket_path: VAR_DNSTAP_SOCKET_PATH STRING_ARG  */
#line 3606 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_socket_path:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_socket_path);
		cfg_parser->cfg->dnstap_socket_path = (yyvsp[0].str);
	}
#line 7211 "util/configparser.c"
    break;

  case 686: /* dt_dnstap_ip: VAR_DNSTAP_IP STRING_ARG  */
#line 3613 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_ip:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_ip);
		cfg_parser->cfg->dnstap_ip = (yyvsp[0].str);
	}
#line 7221 "util/configparser.c"
    break;

  case 687: /* dt_dnstap_tls: VAR_DNSTAP_TLS STRING_ARG  */
#line 3620 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_tls = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7233 "util/configparser.c"
    break;

  case 688: /* dt_dnstap_tls_server_name: VAR_DNSTAP_TLS_SERVER_NAME STRING_ARG  */
#line 3629 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls_server_name:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_tls_server_name);
		cfg_parser->cfg->dnstap_tls_server_name = (yyvsp[0].str);
	}
#line 7243 "util/configparser.c"
    break;

  case 689: /* dt_dnstap_tls_cert_bundle: VAR_DNSTAP_TLS_CERT_BUNDLE STRING_ARG  */
#line 3636 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls_cert_bundle:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_tls_cert_bundle);
		cfg_parser->cfg->dnstap_tls_cert_bundle = (yyvsp[0].str);
	}
#line 7253 "util/configparser.c"
    break;

  case 690: /* dt_dnstap_tls_client_key_file: VAR_DNSTAP_TLS_CLIENT_KEY_FILE STRING_ARG  */
#line 3643 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls_client_key_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_tls_client_key_file);
		cfg_parser->cfg->dnstap_tls_client_key_file = (yyvsp[0].str);
	}
#line 7263 "util/configparser.c"
    break;

  case 691: /* dt_dnstap_tls_client_cert_file: VAR_DNSTAP_TLS_CLIENT_CERT_FILE STRING_ARG  */
#line 3650 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls_client_cert_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_tls_client_cert_file);
		cfg_parser->cfg->dnstap_tls_client_cert_file = (yyvsp[0].str);
	}
#line 7273 "util/configparser.c"
    break;

  case 692: /* dt_dnstap_send_identity: VAR_DNSTAP_SEND_IDENTITY STRING_ARG  */
#line 3657 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_send_identity:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_send_identity = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7285 "util/configparser.c"
    break;

  case 693: /* dt_dnstap_send_version: VAR_DNSTAP_SEND_VERSION STRING_ARG  */
#line 3666 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_send_version:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_send_version = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7297 "util/configparser.c"
    break;

  case 694: /* dt_dnstap_identity: VAR_DNSTAP_IDENTITY STRING_ARG  */
#line 3675 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_identity);
		cfg_parser->cfg->dnstap_identity = (yyvsp[0].str);
	}
#line 7307 "util/configparser.c"
    break;

  case 695: /* dt_dnstap_version: VAR_DNSTAP_VERSION STRING_ARG  */
#line 3682 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_version:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_version);
		cfg_parser->cfg->dnstap_version = (yyvsp[0].str);
	}
#line 7317 "util/configparser.c"
    break;

  case 696: /* dt_dnstap_log_resolver_query_messages: VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES STRING_ARG  */
#line 3689 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_resolver_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_resolver_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7330 "util/configparser.c"
    break;

  case 697: /* dt_dnstap_log_resolver_response_messages: VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES STRING_ARG  */
#line 3699 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_resolver_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_resolver_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7343 "util/configparser.c"
    break;

  case 698: /* dt_dnstap_log_client_query_messages: VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES STRING_ARG  */
#line 3709 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_client_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_client_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7356 "util/configparser.c"
    break;

  case 699: /* dt_dnstap_log_client_response_messages: VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES STRING_ARG  */
#line 3719 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_client_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_client_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7369 "util/configparser.c"
    break;

  case 700: /* dt_dnstap_log_forwarder_query_messages: VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES STRING_ARG  */
#line 3729 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_forwarder_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_forwarder_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7382 "util/configparser.c"
    break;

  case 701: /* dt_dnstap_log_forwarder_response_messages: VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES STRING_ARG  */
#line 3739 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_forwarder_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_forwarder_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7395 "util/configparser.c"
    break;

  case 702: /* dt_dnstap_sample_rate: VAR_DNSTAP_SAMPLE_RATE STRING_ARG  */
#line 3749 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_sample_rate:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if(atoi((yyvsp[0].str)) < 0)
			yyerror("dnstap sample rate too small");
		else	cfg_parser->cfg->dnstap_sample_rate = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 7409 "util/configparser.c"
    break;

  case 703: /* pythonstart: VAR_PYTHON  */
#line 3760 "util/configparser.y"
        {
		OUTYY(("\nP(python:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 7418 "util/configparser.c"
    break;

  case 707: /* py_script: VAR_PYTHON_SCRIPT STRING_ARG  */
#line 3770 "util/configparser.y"
        {
		OUTYY(("P(python-script:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append_ex(&cfg_parser->cfg->python_script, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 7428 "util/configparser.c"
    break;

  case 708: /* dynlibstart: VAR_DYNLIB  */
#line 3777 "util/configparser.y"
        {
		OUTYY(("\nP(dynlib:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 7437 "util/configparser.c"
    break;

  case 712: /* dl_file: VAR_DYNLIB_FILE STRING_ARG  */
#line 3787 "util/configparser.y"
        {
		OUTYY(("P(dynlib-file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append_ex(&cfg_parser->cfg->dynlib_file, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 7447 "util/configparser.c"
    break;

  case 713: /* server_disable_dnssec_lame_check: VAR_DISABLE_DNSSEC_LAME_CHECK STRING_ARG  */
#line 3794 "util/configparser.y"
        {
		OUTYY(("P(disable_dnssec_lame_check:%s)\n", (yyvsp[0].str)));
		if (strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->disable_dnssec_lame_check =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7460 "util/configparser.c"
    break;

  case 714: /* server_log_identity: VAR_LOG_IDENTITY STRING_ARG  */
#line 3804 "util/configparser.y"
        {
		OUTYY(("P(server_log_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->log_identity);
		cfg_parser->cfg->log_identity = (yyvsp[0].str);
	}
#line 7470 "util/configparser.c"
    break;

  case 715: /* server_response_ip: VAR_RESPONSE_IP STRING_ARG STRING_ARG  */
#line 3811 "util/configparser.y"
        {
		OUTYY(("P(server_response_ip:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_respip_action((yyvsp[0].str));
		if(!cfg_str2list_insert(&cfg_parser->cfg->respip_actions,
			(yyvsp[-1].str), (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 7482 "util/configparser.c"
    break;

  case 716: /* server_response_ip_data: VAR_RESPONSE_IP_DATA STRING_ARG STRING_ARG  */
#line 3820 "util/configparser.y"
        {
		OUTYY(("P(server_response_ip_data:%s)\n", (yyvsp[-1].str)));
		if(!cfg_str2list_insert(&cfg_parser->cfg->respip_data,
			(yyvsp[-1].str), (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 7493 "util/configparser.c"
    break;

  case 717: /* dnscstart: VAR_DNSCRYPT  */
#line 3828 "util/configparser.y"
        {
		OUTYY(("\nP(dnscrypt:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 7502 "util/configparser.c"
    break;

  case 730: /* dnsc_dnscrypt_enable: VAR_DNSCRYPT_ENABLE STRING_ARG  */
#line 3845 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnscrypt = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7514 "util/configparser.c"
    break;

  case 731: /* dnsc_dnscrypt_port: VAR_DNSCRYPT_PORT STRING_ARG  */
#line 3854 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->dnscrypt_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 7526 "util/configparser.c"
    break;

  case 732: /* dnsc_dnscrypt_provider: VAR_DNSCRYPT_PROVIDER STRING_ARG  */
#line 3863 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_provider:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnscrypt_provider);
		cfg_parser->cfg->dnscrypt_provider = (yyvsp[0].str);
	}
#line 7536 "util/configparser.c"
    break;

  case 733: /* dnsc_dnscrypt_provider_cert: VAR_DNSCRYPT_PROVIDER_CERT STRING_ARG  */
#line 3870 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_provider_cert:%s)\n", (yyvsp[0].str)));
		if(cfg_strlist_find(cfg_parser->cfg->dnscrypt_provider_cert, (yyvsp[0].str))) {
			log_warn("dnscrypt-provider-cert %s is a duplicate", (yyvsp[0].str));
			free((yyvsp[0].str));
		} else if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_provider_cert, (yyvsp[0].str))) {
			yyerror("out of memory");
		}
	}
#line 7550 "util/configparser.c"
    break;

  case 734: /* dnsc_dnscrypt_provider_cert_rotated: VAR_DNSCRYPT_PROVIDER_CERT_ROTATED STRING_ARG  */
#line 3881 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_provider_cert_rotated:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_provider_cert_rotated, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 7560 "util/configparser.c"
    break;

  case 735: /* dnsc_dnscrypt_secret_key: VAR_DNSCRYPT_SECRET_KEY STRING_ARG  */
#line 3888 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_secret_key:%s)\n", (yyvsp[0].str)));
		if(cfg_strlist_find(cfg_parser->cfg->dnscrypt_secret_key, (yyvsp[0].str))) {
			log_warn("dnscrypt-secret-key: %s is a duplicate", (yyvsp[0].str));
			free((yyvsp[0].str));
		} else if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_secret_key, (yyvsp[0].str))) {
			yyerror("out of memory");
		}
	}
#line 7574 "util/configparser.c"
    break;

  case 736: /* dnsc_dnscrypt_shared_secret_cache_size: VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE STRING_ARG  */
#line 3899 "util/configparser.y"
  {
	OUTYY(("P(dnscrypt_shared_secret_cache_size:%s)\n", (yyvsp[0].str)));
	if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->dnscrypt_shared_secret_cache_size))
		yyerror("memory size expected");
	free((yyvsp[0].str));
  }
#line 7585 "util/configparser.c"
    break;

  case 737: /* dnsc_dnscrypt_shared_secret_cache_slabs: VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS STRING_ARG  */
#line 3907 "util/configparser.y"
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
#line 7601 "util/configparser.c"
    break;

  case 738: /* dnsc_dnscrypt_nonce_cache_size: VAR_DNSCRYPT_NONCE_CACHE_SIZE STRING_ARG  */
#line 3920 "util/configparser.y"
  {
	OUTYY(("P(dnscrypt_nonce_cache_size:%s)\n", (yyvsp[0].str)));
	if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->dnscrypt_nonce_cache_size))
		yyerror("memory size expected");
	free((yyvsp[0].str));
  }
#line 7612 "util/configparser.c"
    break;

  case 739: /* dnsc_dnscrypt_nonce_cache_slabs: VAR_DNSCRYPT_NONCE_CACHE_SLABS STRING_ARG  */
#line 3928 "util/configparser.y"
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
#line 7628 "util/configparser.c"
    break;

  case 740: /* cachedbstart: VAR_CACHEDB  */
#line 3941 "util/configparser.y"
        {
		OUTYY(("\nP(cachedb:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 7637 "util/configparser.c"
    break;

  case 764: /* cachedb_backend_name: VAR_CACHEDB_BACKEND STRING_ARG  */
#line 3961 "util/configparser.y"
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
#line 7652 "util/configparser.c"
    break;

  case 765: /* cachedb_secret_seed: VAR_CACHEDB_SECRETSEED STRING_ARG  */
#line 3973 "util/configparser.y"
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
#line 7667 "util/configparser.c"
    break;

  case 766: /* cachedb_no_store: VAR_CACHEDB_NO_STORE STRING_ARG  */
#line 3985 "util/configparser.y"
        {
	#ifdef USE_CACHEDB
		OUTYY(("P(cachedb_no_store:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->cachedb_no_store = (strcmp((yyvsp[0].str), "yes")==0);
	#else
		OUTYY(("P(Compiled without cachedb, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 7683 "util/configparser.c"
    break;

  case 767: /* cachedb_check_when_serve_expired: VAR_CACHEDB_CHECK_WHEN_SERVE_EXPIRED STRING_ARG  */
#line 3998 "util/configparser.y"
        {
	#ifdef USE_CACHEDB
		OUTYY(("P(cachedb_check_when_serve_expired:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->cachedb_check_when_serve_expired = (strcmp((yyvsp[0].str), "yes")==0);
	#else
		OUTYY(("P(Compiled without cachedb, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 7699 "util/configparser.c"
    break;

  case 768: /* redis_server_host: VAR_CACHEDB_REDISHOST STRING_ARG  */
#line 4011 "util/configparser.y"
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
#line 7714 "util/configparser.c"
    break;

  case 769: /* redis_replica_server_host: VAR_CACHEDB_REDISREPLICAHOST STRING_ARG  */
#line 4023 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		OUTYY(("P(redis_replica_server_host:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->redis_replica_server_host);
		cfg_parser->cfg->redis_replica_server_host = (yyvsp[0].str);
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 7729 "util/configparser.c"
    break;

  case 770: /* redis_server_port: VAR_CACHEDB_REDISPORT STRING_ARG  */
#line 4035 "util/configparser.y"
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
#line 7747 "util/configparser.c"
    break;

  case 771: /* redis_replica_server_port: VAR_CACHEDB_REDISREPLICAPORT STRING_ARG  */
#line 4050 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		int port;
		OUTYY(("P(redis_replica_server_port:%s)\n", (yyvsp[0].str)));
		port = atoi((yyvsp[0].str));
		if(port == 0 || port < 0 || port > 65535)
			yyerror("valid redis server port number expected");
		else cfg_parser->cfg->redis_replica_server_port = port;
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 7765 "util/configparser.c"
    break;

  case 772: /* redis_server_path: VAR_CACHEDB_REDISPATH STRING_ARG  */
#line 4065 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		OUTYY(("P(redis_server_path:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->redis_server_path);
		cfg_parser->cfg->redis_server_path = (yyvsp[0].str);
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 7780 "util/configparser.c"
    break;

  case 773: /* redis_replica_server_path: VAR_CACHEDB_REDISREPLICAPATH STRING_ARG  */
#line 4077 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		OUTYY(("P(redis_replica_server_path:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->redis_replica_server_path);
		cfg_parser->cfg->redis_replica_server_path = (yyvsp[0].str);
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 7795 "util/configparser.c"
    break;

  case 774: /* redis_server_password: VAR_CACHEDB_REDISPASSWORD STRING_ARG  */
#line 4089 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		OUTYY(("P(redis_server_password:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->redis_server_password);
		cfg_parser->cfg->redis_server_password = (yyvsp[0].str);
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 7810 "util/configparser.c"
    break;

  case 775: /* redis_replica_server_password: VAR_CACHEDB_REDISREPLICAPASSWORD STRING_ARG  */
#line 4101 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		OUTYY(("P(redis_replica_server_password:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->redis_replica_server_password);
		cfg_parser->cfg->redis_replica_server_password = (yyvsp[0].str);
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
		free((yyvsp[0].str));
	#endif
	}
#line 7825 "util/configparser.c"
    break;

  case 776: /* redis_timeout: VAR_CACHEDB_REDISTIMEOUT STRING_ARG  */
#line 4113 "util/configparser.y"
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
#line 7841 "util/configparser.c"
    break;

  case 777: /* redis_replica_timeout: VAR_CACHEDB_REDISREPLICATIMEOUT STRING_ARG  */
#line 4126 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		OUTYY(("P(redis_replica_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("redis timeout value expected");
		else cfg_parser->cfg->redis_replica_timeout = atoi((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 7857 "util/configparser.c"
    break;

  case 778: /* redis_command_timeout: VAR_CACHEDB_REDISCOMMANDTIMEOUT STRING_ARG  */
#line 4139 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		OUTYY(("P(redis_command_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("redis command timeout value expected");
		else cfg_parser->cfg->redis_command_timeout = atoi((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 7873 "util/configparser.c"
    break;

  case 779: /* redis_replica_command_timeout: VAR_CACHEDB_REDISREPLICACOMMANDTIMEOUT STRING_ARG  */
#line 4152 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		OUTYY(("P(redis_replica_command_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("redis command timeout value expected");
		else cfg_parser->cfg->redis_replica_command_timeout = atoi((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 7889 "util/configparser.c"
    break;

  case 780: /* redis_connect_timeout: VAR_CACHEDB_REDISCONNECTTIMEOUT STRING_ARG  */
#line 4165 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		OUTYY(("P(redis_connect_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("redis connect timeout value expected");
		else cfg_parser->cfg->redis_connect_timeout = atoi((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 7905 "util/configparser.c"
    break;

  case 781: /* redis_replica_connect_timeout: VAR_CACHEDB_REDISREPLICACONNECTTIMEOUT STRING_ARG  */
#line 4178 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		OUTYY(("P(redis_replica_connect_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("redis connect timeout value expected");
		else cfg_parser->cfg->redis_replica_connect_timeout = atoi((yyvsp[0].str));
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 7921 "util/configparser.c"
    break;

  case 782: /* redis_expire_records: VAR_CACHEDB_REDISEXPIRERECORDS STRING_ARG  */
#line 4191 "util/configparser.y"
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
#line 7937 "util/configparser.c"
    break;

  case 783: /* redis_logical_db: VAR_CACHEDB_REDISLOGICALDB STRING_ARG  */
#line 4204 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		int db;
		OUTYY(("P(redis_logical_db:%s)\n", (yyvsp[0].str)));
		db = atoi((yyvsp[0].str));
		if((db == 0 && strcmp((yyvsp[0].str), "0") != 0) || db < 0)
			yyerror("valid redis logical database index expected");
		else cfg_parser->cfg->redis_logical_db = db;
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 7955 "util/configparser.c"
    break;

  case 784: /* redis_replica_logical_db: VAR_CACHEDB_REDISREPLICALOGICALDB STRING_ARG  */
#line 4219 "util/configparser.y"
        {
	#if defined(USE_CACHEDB) && defined(USE_REDIS)
		int db;
		OUTYY(("P(redis_replica_logical_db:%s)\n", (yyvsp[0].str)));
		db = atoi((yyvsp[0].str));
		if((db == 0 && strcmp((yyvsp[0].str), "0") != 0) || db < 0)
			yyerror("valid redis logical database index expected");
		else cfg_parser->cfg->redis_replica_logical_db = db;
	#else
		OUTYY(("P(Compiled without cachedb or redis, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 7973 "util/configparser.c"
    break;

  case 785: /* server_tcp_connection_limit: VAR_TCP_CONNECTION_LIMIT STRING_ARG STRING_ARG  */
#line 4234 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_connection_limit:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if (atoi((yyvsp[0].str)) < 0)
			yyerror("positive number expected");
		else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->tcp_connection_limits, (yyvsp[-1].str), (yyvsp[0].str)))
				yyerror("out of memory");
		}
	}
#line 7987 "util/configparser.c"
    break;

  case 786: /* server_answer_cookie: VAR_ANSWER_COOKIE STRING_ARG  */
#line 4245 "util/configparser.y"
        {
		OUTYY(("P(server_answer_cookie:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_answer_cookie = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7999 "util/configparser.c"
    break;

  case 787: /* server_cookie_secret: VAR_COOKIE_SECRET STRING_ARG  */
#line 4254 "util/configparser.y"
        {
		uint8_t secret[32];
		size_t secret_len = sizeof(secret);

		OUTYY(("P(server_cookie_secret:%s)\n", (yyvsp[0].str)));
		if(sldns_str2wire_hex_buf((yyvsp[0].str), secret, &secret_len)
		|| (secret_len != 16))
			yyerror("expected 128 bit hex string");
		else {
			cfg_parser->cfg->cookie_secret_len = secret_len;
			memcpy(cfg_parser->cfg->cookie_secret, secret, sizeof(secret));
		}
		free((yyvsp[0].str));
	}
#line 8018 "util/configparser.c"
    break;

  case 788: /* server_cookie_secret_file: VAR_COOKIE_SECRET_FILE STRING_ARG  */
#line 4270 "util/configparser.y"
        {
		OUTYY(("P(cookie_secret_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->cookie_secret_file);
		cfg_parser->cfg->cookie_secret_file = (yyvsp[0].str);
	}
#line 8028 "util/configparser.c"
    break;

  case 789: /* server_iter_scrub_ns: VAR_ITER_SCRUB_NS STRING_ARG  */
#line 4277 "util/configparser.y"
        {
		OUTYY(("P(server_iter_scrub_ns:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) < 1)
			yyerror("number >= 1 expected");
		else cfg_parser->cfg->iter_scrub_ns = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 8040 "util/configparser.c"
    break;

  case 790: /* server_iter_scrub_cname: VAR_ITER_SCRUB_CNAME STRING_ARG  */
#line 4286 "util/configparser.y"
        {
		OUTYY(("P(server_iter_scrub_cname:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->iter_scrub_cname = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 8052 "util/configparser.c"
    break;

  case 791: /* server_iter_scrub_rrsig: VAR_ITER_SCRUB_RRSIG STRING_ARG  */
#line 4295 "util/configparser.y"
        {
		OUTYY(("P(server_iter_scrub_rrsig:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->iter_scrub_rrsig = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 8064 "util/configparser.c"
    break;

  case 792: /* server_max_global_quota: VAR_MAX_GLOBAL_QUOTA STRING_ARG  */
#line 4304 "util/configparser.y"
        {
		OUTYY(("P(server_max_global_quota:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_global_quota = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 8076 "util/configparser.c"
    break;

  case 793: /* server_iter_scrub_promiscuous: VAR_ITER_SCRUB_PROMISCUOUS STRING_ARG  */
#line 4313 "util/configparser.y"
        {
		OUTYY(("P(server_iter_scrub_promiscuous:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->iter_scrub_promiscuous =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 8089 "util/configparser.c"
    break;

  case 794: /* server_val_validation_attempts: VAR_VAL_VALIDATION_ATTEMPTS STRING_ARG  */
#line 4323 "util/configparser.y"
        {
		OUTYY(("P(server_val_validation_attempts:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->val_validation_attempts = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 8101 "util/configparser.c"
    break;

  case 795: /* server_val_hash_attempts: VAR_VAL_HASH_ATTEMPTS STRING_ARG  */
#line 4332 "util/configparser.y"
        {
		OUTYY(("P(server_val_hash_attempts:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->val_hash_attempts = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 8113 "util/configparser.c"
    break;

  case 796: /* ipsetstart: VAR_IPSET  */
#line 4341 "util/configparser.y"
        {
		OUTYY(("\nP(ipset:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 8122 "util/configparser.c"
    break;

  case 801: /* ipset_name_v4: VAR_IPSET_NAME_V4 STRING_ARG  */
#line 4351 "util/configparser.y"
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
#line 8140 "util/configparser.c"
    break;

  case 802: /* ipset_name_v6: VAR_IPSET_NAME_V6 STRING_ARG  */
#line 4366 "util/configparser.y"
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
#line 8158 "util/configparser.c"
    break;


#line 8162 "util/configparser.c"

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
  ++yynerrs;

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
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
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

#line 4380 "util/configparser.y"


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
		strcmp(action, "allow_snoop")!=0 &&
		strcmp(action, "allow_cookie")!=0)
	{
		yyerror("expected deny, refuse, deny_non_local, "
			"refuse_non_local, allow, allow_setrd, "
			"allow_snoop or allow_cookie as access control action");
	}
}
