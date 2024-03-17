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
  YYSYMBOL_VAR_RESPONSE_IP_TAG = 192,      /* VAR_RESPONSE_IP_TAG  */
  YYSYMBOL_VAR_RESPONSE_IP = 193,          /* VAR_RESPONSE_IP  */
  YYSYMBOL_VAR_RESPONSE_IP_DATA = 194,     /* VAR_RESPONSE_IP_DATA  */
  YYSYMBOL_VAR_HARDEN_ALGO_DOWNGRADE = 195, /* VAR_HARDEN_ALGO_DOWNGRADE  */
  YYSYMBOL_VAR_IP_TRANSPARENT = 196,       /* VAR_IP_TRANSPARENT  */
  YYSYMBOL_VAR_IP_DSCP = 197,              /* VAR_IP_DSCP  */
  YYSYMBOL_VAR_DISABLE_DNSSEC_LAME_CHECK = 198, /* VAR_DISABLE_DNSSEC_LAME_CHECK  */
  YYSYMBOL_VAR_IP_RATELIMIT = 199,         /* VAR_IP_RATELIMIT  */
  YYSYMBOL_VAR_IP_RATELIMIT_SLABS = 200,   /* VAR_IP_RATELIMIT_SLABS  */
  YYSYMBOL_VAR_IP_RATELIMIT_SIZE = 201,    /* VAR_IP_RATELIMIT_SIZE  */
  YYSYMBOL_VAR_RATELIMIT = 202,            /* VAR_RATELIMIT  */
  YYSYMBOL_VAR_RATELIMIT_SLABS = 203,      /* VAR_RATELIMIT_SLABS  */
  YYSYMBOL_VAR_RATELIMIT_SIZE = 204,       /* VAR_RATELIMIT_SIZE  */
  YYSYMBOL_VAR_OUTBOUND_MSG_RETRY = 205,   /* VAR_OUTBOUND_MSG_RETRY  */
  YYSYMBOL_VAR_MAX_SENT_COUNT = 206,       /* VAR_MAX_SENT_COUNT  */
  YYSYMBOL_VAR_MAX_QUERY_RESTARTS = 207,   /* VAR_MAX_QUERY_RESTARTS  */
  YYSYMBOL_VAR_RATELIMIT_FOR_DOMAIN = 208, /* VAR_RATELIMIT_FOR_DOMAIN  */
  YYSYMBOL_VAR_RATELIMIT_BELOW_DOMAIN = 209, /* VAR_RATELIMIT_BELOW_DOMAIN  */
  YYSYMBOL_VAR_IP_RATELIMIT_FACTOR = 210,  /* VAR_IP_RATELIMIT_FACTOR  */
  YYSYMBOL_VAR_RATELIMIT_FACTOR = 211,     /* VAR_RATELIMIT_FACTOR  */
  YYSYMBOL_VAR_IP_RATELIMIT_BACKOFF = 212, /* VAR_IP_RATELIMIT_BACKOFF  */
  YYSYMBOL_VAR_RATELIMIT_BACKOFF = 213,    /* VAR_RATELIMIT_BACKOFF  */
  YYSYMBOL_VAR_SEND_CLIENT_SUBNET = 214,   /* VAR_SEND_CLIENT_SUBNET  */
  YYSYMBOL_VAR_CLIENT_SUBNET_ZONE = 215,   /* VAR_CLIENT_SUBNET_ZONE  */
  YYSYMBOL_VAR_CLIENT_SUBNET_ALWAYS_FORWARD = 216, /* VAR_CLIENT_SUBNET_ALWAYS_FORWARD  */
  YYSYMBOL_VAR_CLIENT_SUBNET_OPCODE = 217, /* VAR_CLIENT_SUBNET_OPCODE  */
  YYSYMBOL_VAR_MAX_CLIENT_SUBNET_IPV4 = 218, /* VAR_MAX_CLIENT_SUBNET_IPV4  */
  YYSYMBOL_VAR_MAX_CLIENT_SUBNET_IPV6 = 219, /* VAR_MAX_CLIENT_SUBNET_IPV6  */
  YYSYMBOL_VAR_MIN_CLIENT_SUBNET_IPV4 = 220, /* VAR_MIN_CLIENT_SUBNET_IPV4  */
  YYSYMBOL_VAR_MIN_CLIENT_SUBNET_IPV6 = 221, /* VAR_MIN_CLIENT_SUBNET_IPV6  */
  YYSYMBOL_VAR_MAX_ECS_TREE_SIZE_IPV4 = 222, /* VAR_MAX_ECS_TREE_SIZE_IPV4  */
  YYSYMBOL_VAR_MAX_ECS_TREE_SIZE_IPV6 = 223, /* VAR_MAX_ECS_TREE_SIZE_IPV6  */
  YYSYMBOL_VAR_CAPS_WHITELIST = 224,       /* VAR_CAPS_WHITELIST  */
  YYSYMBOL_VAR_CACHE_MAX_NEGATIVE_TTL = 225, /* VAR_CACHE_MAX_NEGATIVE_TTL  */
  YYSYMBOL_VAR_PERMIT_SMALL_HOLDDOWN = 226, /* VAR_PERMIT_SMALL_HOLDDOWN  */
  YYSYMBOL_VAR_QNAME_MINIMISATION = 227,   /* VAR_QNAME_MINIMISATION  */
  YYSYMBOL_VAR_QNAME_MINIMISATION_STRICT = 228, /* VAR_QNAME_MINIMISATION_STRICT  */
  YYSYMBOL_VAR_IP_FREEBIND = 229,          /* VAR_IP_FREEBIND  */
  YYSYMBOL_VAR_DEFINE_TAG = 230,           /* VAR_DEFINE_TAG  */
  YYSYMBOL_VAR_LOCAL_ZONE_TAG = 231,       /* VAR_LOCAL_ZONE_TAG  */
  YYSYMBOL_VAR_ACCESS_CONTROL_TAG = 232,   /* VAR_ACCESS_CONTROL_TAG  */
  YYSYMBOL_VAR_LOCAL_ZONE_OVERRIDE = 233,  /* VAR_LOCAL_ZONE_OVERRIDE  */
  YYSYMBOL_VAR_ACCESS_CONTROL_TAG_ACTION = 234, /* VAR_ACCESS_CONTROL_TAG_ACTION  */
  YYSYMBOL_VAR_ACCESS_CONTROL_TAG_DATA = 235, /* VAR_ACCESS_CONTROL_TAG_DATA  */
  YYSYMBOL_VAR_VIEW = 236,                 /* VAR_VIEW  */
  YYSYMBOL_VAR_ACCESS_CONTROL_VIEW = 237,  /* VAR_ACCESS_CONTROL_VIEW  */
  YYSYMBOL_VAR_VIEW_FIRST = 238,           /* VAR_VIEW_FIRST  */
  YYSYMBOL_VAR_SERVE_EXPIRED = 239,        /* VAR_SERVE_EXPIRED  */
  YYSYMBOL_VAR_SERVE_EXPIRED_TTL = 240,    /* VAR_SERVE_EXPIRED_TTL  */
  YYSYMBOL_VAR_SERVE_EXPIRED_TTL_RESET = 241, /* VAR_SERVE_EXPIRED_TTL_RESET  */
  YYSYMBOL_VAR_SERVE_EXPIRED_REPLY_TTL = 242, /* VAR_SERVE_EXPIRED_REPLY_TTL  */
  YYSYMBOL_VAR_SERVE_EXPIRED_CLIENT_TIMEOUT = 243, /* VAR_SERVE_EXPIRED_CLIENT_TIMEOUT  */
  YYSYMBOL_VAR_EDE_SERVE_EXPIRED = 244,    /* VAR_EDE_SERVE_EXPIRED  */
  YYSYMBOL_VAR_SERVE_ORIGINAL_TTL = 245,   /* VAR_SERVE_ORIGINAL_TTL  */
  YYSYMBOL_VAR_FAKE_DSA = 246,             /* VAR_FAKE_DSA  */
  YYSYMBOL_VAR_FAKE_SHA1 = 247,            /* VAR_FAKE_SHA1  */
  YYSYMBOL_VAR_LOG_IDENTITY = 248,         /* VAR_LOG_IDENTITY  */
  YYSYMBOL_VAR_HIDE_TRUSTANCHOR = 249,     /* VAR_HIDE_TRUSTANCHOR  */
  YYSYMBOL_VAR_HIDE_HTTP_USER_AGENT = 250, /* VAR_HIDE_HTTP_USER_AGENT  */
  YYSYMBOL_VAR_HTTP_USER_AGENT = 251,      /* VAR_HTTP_USER_AGENT  */
  YYSYMBOL_VAR_TRUST_ANCHOR_SIGNALING = 252, /* VAR_TRUST_ANCHOR_SIGNALING  */
  YYSYMBOL_VAR_AGGRESSIVE_NSEC = 253,      /* VAR_AGGRESSIVE_NSEC  */
  YYSYMBOL_VAR_USE_SYSTEMD = 254,          /* VAR_USE_SYSTEMD  */
  YYSYMBOL_VAR_SHM_ENABLE = 255,           /* VAR_SHM_ENABLE  */
  YYSYMBOL_VAR_SHM_KEY = 256,              /* VAR_SHM_KEY  */
  YYSYMBOL_VAR_ROOT_KEY_SENTINEL = 257,    /* VAR_ROOT_KEY_SENTINEL  */
  YYSYMBOL_VAR_DNSCRYPT = 258,             /* VAR_DNSCRYPT  */
  YYSYMBOL_VAR_DNSCRYPT_ENABLE = 259,      /* VAR_DNSCRYPT_ENABLE  */
  YYSYMBOL_VAR_DNSCRYPT_PORT = 260,        /* VAR_DNSCRYPT_PORT  */
  YYSYMBOL_VAR_DNSCRYPT_PROVIDER = 261,    /* VAR_DNSCRYPT_PROVIDER  */
  YYSYMBOL_VAR_DNSCRYPT_SECRET_KEY = 262,  /* VAR_DNSCRYPT_SECRET_KEY  */
  YYSYMBOL_VAR_DNSCRYPT_PROVIDER_CERT = 263, /* VAR_DNSCRYPT_PROVIDER_CERT  */
  YYSYMBOL_VAR_DNSCRYPT_PROVIDER_CERT_ROTATED = 264, /* VAR_DNSCRYPT_PROVIDER_CERT_ROTATED  */
  YYSYMBOL_VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE = 265, /* VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE  */
  YYSYMBOL_VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS = 266, /* VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS  */
  YYSYMBOL_VAR_DNSCRYPT_NONCE_CACHE_SIZE = 267, /* VAR_DNSCRYPT_NONCE_CACHE_SIZE  */
  YYSYMBOL_VAR_DNSCRYPT_NONCE_CACHE_SLABS = 268, /* VAR_DNSCRYPT_NONCE_CACHE_SLABS  */
  YYSYMBOL_VAR_PAD_RESPONSES = 269,        /* VAR_PAD_RESPONSES  */
  YYSYMBOL_VAR_PAD_RESPONSES_BLOCK_SIZE = 270, /* VAR_PAD_RESPONSES_BLOCK_SIZE  */
  YYSYMBOL_VAR_PAD_QUERIES = 271,          /* VAR_PAD_QUERIES  */
  YYSYMBOL_VAR_PAD_QUERIES_BLOCK_SIZE = 272, /* VAR_PAD_QUERIES_BLOCK_SIZE  */
  YYSYMBOL_VAR_IPSECMOD_ENABLED = 273,     /* VAR_IPSECMOD_ENABLED  */
  YYSYMBOL_VAR_IPSECMOD_HOOK = 274,        /* VAR_IPSECMOD_HOOK  */
  YYSYMBOL_VAR_IPSECMOD_IGNORE_BOGUS = 275, /* VAR_IPSECMOD_IGNORE_BOGUS  */
  YYSYMBOL_VAR_IPSECMOD_MAX_TTL = 276,     /* VAR_IPSECMOD_MAX_TTL  */
  YYSYMBOL_VAR_IPSECMOD_WHITELIST = 277,   /* VAR_IPSECMOD_WHITELIST  */
  YYSYMBOL_VAR_IPSECMOD_STRICT = 278,      /* VAR_IPSECMOD_STRICT  */
  YYSYMBOL_VAR_CACHEDB = 279,              /* VAR_CACHEDB  */
  YYSYMBOL_VAR_CACHEDB_BACKEND = 280,      /* VAR_CACHEDB_BACKEND  */
  YYSYMBOL_VAR_CACHEDB_SECRETSEED = 281,   /* VAR_CACHEDB_SECRETSEED  */
  YYSYMBOL_VAR_CACHEDB_REDISHOST = 282,    /* VAR_CACHEDB_REDISHOST  */
  YYSYMBOL_VAR_CACHEDB_REDISPORT = 283,    /* VAR_CACHEDB_REDISPORT  */
  YYSYMBOL_VAR_CACHEDB_REDISTIMEOUT = 284, /* VAR_CACHEDB_REDISTIMEOUT  */
  YYSYMBOL_VAR_CACHEDB_REDISEXPIRERECORDS = 285, /* VAR_CACHEDB_REDISEXPIRERECORDS  */
  YYSYMBOL_VAR_CACHEDB_REDISPATH = 286,    /* VAR_CACHEDB_REDISPATH  */
  YYSYMBOL_VAR_CACHEDB_REDISPASSWORD = 287, /* VAR_CACHEDB_REDISPASSWORD  */
  YYSYMBOL_VAR_CACHEDB_REDISLOGICALDB = 288, /* VAR_CACHEDB_REDISLOGICALDB  */
  YYSYMBOL_VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM = 289, /* VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM  */
  YYSYMBOL_VAR_FOR_UPSTREAM = 290,         /* VAR_FOR_UPSTREAM  */
  YYSYMBOL_VAR_AUTH_ZONE = 291,            /* VAR_AUTH_ZONE  */
  YYSYMBOL_VAR_ZONEFILE = 292,             /* VAR_ZONEFILE  */
  YYSYMBOL_VAR_MASTER = 293,               /* VAR_MASTER  */
  YYSYMBOL_VAR_URL = 294,                  /* VAR_URL  */
  YYSYMBOL_VAR_FOR_DOWNSTREAM = 295,       /* VAR_FOR_DOWNSTREAM  */
  YYSYMBOL_VAR_FALLBACK_ENABLED = 296,     /* VAR_FALLBACK_ENABLED  */
  YYSYMBOL_VAR_TLS_ADDITIONAL_PORT = 297,  /* VAR_TLS_ADDITIONAL_PORT  */
  YYSYMBOL_VAR_LOW_RTT = 298,              /* VAR_LOW_RTT  */
  YYSYMBOL_VAR_LOW_RTT_PERMIL = 299,       /* VAR_LOW_RTT_PERMIL  */
  YYSYMBOL_VAR_FAST_SERVER_PERMIL = 300,   /* VAR_FAST_SERVER_PERMIL  */
  YYSYMBOL_VAR_FAST_SERVER_NUM = 301,      /* VAR_FAST_SERVER_NUM  */
  YYSYMBOL_VAR_ALLOW_NOTIFY = 302,         /* VAR_ALLOW_NOTIFY  */
  YYSYMBOL_VAR_TLS_WIN_CERT = 303,         /* VAR_TLS_WIN_CERT  */
  YYSYMBOL_VAR_TCP_CONNECTION_LIMIT = 304, /* VAR_TCP_CONNECTION_LIMIT  */
  YYSYMBOL_VAR_ANSWER_COOKIE = 305,        /* VAR_ANSWER_COOKIE  */
  YYSYMBOL_VAR_COOKIE_SECRET = 306,        /* VAR_COOKIE_SECRET  */
  YYSYMBOL_VAR_IP_RATELIMIT_COOKIE = 307,  /* VAR_IP_RATELIMIT_COOKIE  */
  YYSYMBOL_VAR_FORWARD_NO_CACHE = 308,     /* VAR_FORWARD_NO_CACHE  */
  YYSYMBOL_VAR_STUB_NO_CACHE = 309,        /* VAR_STUB_NO_CACHE  */
  YYSYMBOL_VAR_LOG_SERVFAIL = 310,         /* VAR_LOG_SERVFAIL  */
  YYSYMBOL_VAR_DENY_ANY = 311,             /* VAR_DENY_ANY  */
  YYSYMBOL_VAR_UNKNOWN_SERVER_TIME_LIMIT = 312, /* VAR_UNKNOWN_SERVER_TIME_LIMIT  */
  YYSYMBOL_VAR_LOG_TAG_QUERYREPLY = 313,   /* VAR_LOG_TAG_QUERYREPLY  */
  YYSYMBOL_VAR_STREAM_WAIT_SIZE = 314,     /* VAR_STREAM_WAIT_SIZE  */
  YYSYMBOL_VAR_TLS_CIPHERS = 315,          /* VAR_TLS_CIPHERS  */
  YYSYMBOL_VAR_TLS_CIPHERSUITES = 316,     /* VAR_TLS_CIPHERSUITES  */
  YYSYMBOL_VAR_TLS_USE_SNI = 317,          /* VAR_TLS_USE_SNI  */
  YYSYMBOL_VAR_IPSET = 318,                /* VAR_IPSET  */
  YYSYMBOL_VAR_IPSET_NAME_V4 = 319,        /* VAR_IPSET_NAME_V4  */
  YYSYMBOL_VAR_IPSET_NAME_V6 = 320,        /* VAR_IPSET_NAME_V6  */
  YYSYMBOL_VAR_TLS_SESSION_TICKET_KEYS = 321, /* VAR_TLS_SESSION_TICKET_KEYS  */
  YYSYMBOL_VAR_RPZ = 322,                  /* VAR_RPZ  */
  YYSYMBOL_VAR_TAGS = 323,                 /* VAR_TAGS  */
  YYSYMBOL_VAR_RPZ_ACTION_OVERRIDE = 324,  /* VAR_RPZ_ACTION_OVERRIDE  */
  YYSYMBOL_VAR_RPZ_CNAME_OVERRIDE = 325,   /* VAR_RPZ_CNAME_OVERRIDE  */
  YYSYMBOL_VAR_RPZ_LOG = 326,              /* VAR_RPZ_LOG  */
  YYSYMBOL_VAR_RPZ_LOG_NAME = 327,         /* VAR_RPZ_LOG_NAME  */
  YYSYMBOL_VAR_DYNLIB = 328,               /* VAR_DYNLIB  */
  YYSYMBOL_VAR_DYNLIB_FILE = 329,          /* VAR_DYNLIB_FILE  */
  YYSYMBOL_VAR_EDNS_CLIENT_STRING = 330,   /* VAR_EDNS_CLIENT_STRING  */
  YYSYMBOL_VAR_EDNS_CLIENT_STRING_OPCODE = 331, /* VAR_EDNS_CLIENT_STRING_OPCODE  */
  YYSYMBOL_VAR_NSID = 332,                 /* VAR_NSID  */
  YYSYMBOL_VAR_ZONEMD_PERMISSIVE_MODE = 333, /* VAR_ZONEMD_PERMISSIVE_MODE  */
  YYSYMBOL_VAR_ZONEMD_CHECK = 334,         /* VAR_ZONEMD_CHECK  */
  YYSYMBOL_VAR_ZONEMD_REJECT_ABSENCE = 335, /* VAR_ZONEMD_REJECT_ABSENCE  */
  YYSYMBOL_VAR_RPZ_SIGNAL_NXDOMAIN_RA = 336, /* VAR_RPZ_SIGNAL_NXDOMAIN_RA  */
  YYSYMBOL_VAR_INTERFACE_AUTOMATIC_PORTS = 337, /* VAR_INTERFACE_AUTOMATIC_PORTS  */
  YYSYMBOL_VAR_EDE = 338,                  /* VAR_EDE  */
  YYSYMBOL_VAR_INTERFACE_ACTION = 339,     /* VAR_INTERFACE_ACTION  */
  YYSYMBOL_VAR_INTERFACE_VIEW = 340,       /* VAR_INTERFACE_VIEW  */
  YYSYMBOL_VAR_INTERFACE_TAG = 341,        /* VAR_INTERFACE_TAG  */
  YYSYMBOL_VAR_INTERFACE_TAG_ACTION = 342, /* VAR_INTERFACE_TAG_ACTION  */
  YYSYMBOL_VAR_INTERFACE_TAG_DATA = 343,   /* VAR_INTERFACE_TAG_DATA  */
  YYSYMBOL_VAR_PROXY_PROTOCOL_PORT = 344,  /* VAR_PROXY_PROTOCOL_PORT  */
  YYSYMBOL_VAR_STATISTICS_INHIBIT_ZERO = 345, /* VAR_STATISTICS_INHIBIT_ZERO  */
  YYSYMBOL_VAR_HARDEN_UNKNOWN_ADDITIONAL = 346, /* VAR_HARDEN_UNKNOWN_ADDITIONAL  */
  YYSYMBOL_VAR_DISABLE_EDNS_DO = 347,      /* VAR_DISABLE_EDNS_DO  */
  YYSYMBOL_VAR_CACHEDB_NO_STORE = 348,     /* VAR_CACHEDB_NO_STORE  */
  YYSYMBOL_VAR_LOG_DESTADDR = 349,         /* VAR_LOG_DESTADDR  */
  YYSYMBOL_YYACCEPT = 350,                 /* $accept  */
  YYSYMBOL_toplevelvars = 351,             /* toplevelvars  */
  YYSYMBOL_toplevelvar = 352,              /* toplevelvar  */
  YYSYMBOL_force_toplevel = 353,           /* force_toplevel  */
  YYSYMBOL_serverstart = 354,              /* serverstart  */
  YYSYMBOL_contents_server = 355,          /* contents_server  */
  YYSYMBOL_content_server = 356,           /* content_server  */
  YYSYMBOL_stubstart = 357,                /* stubstart  */
  YYSYMBOL_contents_stub = 358,            /* contents_stub  */
  YYSYMBOL_content_stub = 359,             /* content_stub  */
  YYSYMBOL_forwardstart = 360,             /* forwardstart  */
  YYSYMBOL_contents_forward = 361,         /* contents_forward  */
  YYSYMBOL_content_forward = 362,          /* content_forward  */
  YYSYMBOL_viewstart = 363,                /* viewstart  */
  YYSYMBOL_contents_view = 364,            /* contents_view  */
  YYSYMBOL_content_view = 365,             /* content_view  */
  YYSYMBOL_authstart = 366,                /* authstart  */
  YYSYMBOL_contents_auth = 367,            /* contents_auth  */
  YYSYMBOL_content_auth = 368,             /* content_auth  */
  YYSYMBOL_rpz_tag = 369,                  /* rpz_tag  */
  YYSYMBOL_rpz_action_override = 370,      /* rpz_action_override  */
  YYSYMBOL_rpz_cname_override = 371,       /* rpz_cname_override  */
  YYSYMBOL_rpz_log = 372,                  /* rpz_log  */
  YYSYMBOL_rpz_log_name = 373,             /* rpz_log_name  */
  YYSYMBOL_rpz_signal_nxdomain_ra = 374,   /* rpz_signal_nxdomain_ra  */
  YYSYMBOL_rpzstart = 375,                 /* rpzstart  */
  YYSYMBOL_contents_rpz = 376,             /* contents_rpz  */
  YYSYMBOL_content_rpz = 377,              /* content_rpz  */
  YYSYMBOL_server_num_threads = 378,       /* server_num_threads  */
  YYSYMBOL_server_verbosity = 379,         /* server_verbosity  */
  YYSYMBOL_server_statistics_interval = 380, /* server_statistics_interval  */
  YYSYMBOL_server_statistics_cumulative = 381, /* server_statistics_cumulative  */
  YYSYMBOL_server_extended_statistics = 382, /* server_extended_statistics  */
  YYSYMBOL_server_statistics_inhibit_zero = 383, /* server_statistics_inhibit_zero  */
  YYSYMBOL_server_shm_enable = 384,        /* server_shm_enable  */
  YYSYMBOL_server_shm_key = 385,           /* server_shm_key  */
  YYSYMBOL_server_port = 386,              /* server_port  */
  YYSYMBOL_server_send_client_subnet = 387, /* server_send_client_subnet  */
  YYSYMBOL_server_client_subnet_zone = 388, /* server_client_subnet_zone  */
  YYSYMBOL_server_client_subnet_always_forward = 389, /* server_client_subnet_always_forward  */
  YYSYMBOL_server_client_subnet_opcode = 390, /* server_client_subnet_opcode  */
  YYSYMBOL_server_max_client_subnet_ipv4 = 391, /* server_max_client_subnet_ipv4  */
  YYSYMBOL_server_max_client_subnet_ipv6 = 392, /* server_max_client_subnet_ipv6  */
  YYSYMBOL_server_min_client_subnet_ipv4 = 393, /* server_min_client_subnet_ipv4  */
  YYSYMBOL_server_min_client_subnet_ipv6 = 394, /* server_min_client_subnet_ipv6  */
  YYSYMBOL_server_max_ecs_tree_size_ipv4 = 395, /* server_max_ecs_tree_size_ipv4  */
  YYSYMBOL_server_max_ecs_tree_size_ipv6 = 396, /* server_max_ecs_tree_size_ipv6  */
  YYSYMBOL_server_interface = 397,         /* server_interface  */
  YYSYMBOL_server_outgoing_interface = 398, /* server_outgoing_interface  */
  YYSYMBOL_server_outgoing_range = 399,    /* server_outgoing_range  */
  YYSYMBOL_server_outgoing_port_permit = 400, /* server_outgoing_port_permit  */
  YYSYMBOL_server_outgoing_port_avoid = 401, /* server_outgoing_port_avoid  */
  YYSYMBOL_server_outgoing_num_tcp = 402,  /* server_outgoing_num_tcp  */
  YYSYMBOL_server_incoming_num_tcp = 403,  /* server_incoming_num_tcp  */
  YYSYMBOL_server_interface_automatic = 404, /* server_interface_automatic  */
  YYSYMBOL_server_interface_automatic_ports = 405, /* server_interface_automatic_ports  */
  YYSYMBOL_server_do_ip4 = 406,            /* server_do_ip4  */
  YYSYMBOL_server_do_ip6 = 407,            /* server_do_ip6  */
  YYSYMBOL_server_do_nat64 = 408,          /* server_do_nat64  */
  YYSYMBOL_server_do_udp = 409,            /* server_do_udp  */
  YYSYMBOL_server_do_tcp = 410,            /* server_do_tcp  */
  YYSYMBOL_server_prefer_ip4 = 411,        /* server_prefer_ip4  */
  YYSYMBOL_server_prefer_ip6 = 412,        /* server_prefer_ip6  */
  YYSYMBOL_server_tcp_mss = 413,           /* server_tcp_mss  */
  YYSYMBOL_server_outgoing_tcp_mss = 414,  /* server_outgoing_tcp_mss  */
  YYSYMBOL_server_tcp_idle_timeout = 415,  /* server_tcp_idle_timeout  */
  YYSYMBOL_server_max_reuse_tcp_queries = 416, /* server_max_reuse_tcp_queries  */
  YYSYMBOL_server_tcp_reuse_timeout = 417, /* server_tcp_reuse_timeout  */
  YYSYMBOL_server_tcp_auth_query_timeout = 418, /* server_tcp_auth_query_timeout  */
  YYSYMBOL_server_tcp_keepalive = 419,     /* server_tcp_keepalive  */
  YYSYMBOL_server_tcp_keepalive_timeout = 420, /* server_tcp_keepalive_timeout  */
  YYSYMBOL_server_sock_queue_timeout = 421, /* server_sock_queue_timeout  */
  YYSYMBOL_server_tcp_upstream = 422,      /* server_tcp_upstream  */
  YYSYMBOL_server_udp_upstream_without_downstream = 423, /* server_udp_upstream_without_downstream  */
  YYSYMBOL_server_ssl_upstream = 424,      /* server_ssl_upstream  */
  YYSYMBOL_server_ssl_service_key = 425,   /* server_ssl_service_key  */
  YYSYMBOL_server_ssl_service_pem = 426,   /* server_ssl_service_pem  */
  YYSYMBOL_server_ssl_port = 427,          /* server_ssl_port  */
  YYSYMBOL_server_tls_cert_bundle = 428,   /* server_tls_cert_bundle  */
  YYSYMBOL_server_tls_win_cert = 429,      /* server_tls_win_cert  */
  YYSYMBOL_server_tls_additional_port = 430, /* server_tls_additional_port  */
  YYSYMBOL_server_tls_ciphers = 431,       /* server_tls_ciphers  */
  YYSYMBOL_server_tls_ciphersuites = 432,  /* server_tls_ciphersuites  */
  YYSYMBOL_server_tls_session_ticket_keys = 433, /* server_tls_session_ticket_keys  */
  YYSYMBOL_server_tls_use_sni = 434,       /* server_tls_use_sni  */
  YYSYMBOL_server_https_port = 435,        /* server_https_port  */
  YYSYMBOL_server_http_endpoint = 436,     /* server_http_endpoint  */
  YYSYMBOL_server_http_max_streams = 437,  /* server_http_max_streams  */
  YYSYMBOL_server_http_query_buffer_size = 438, /* server_http_query_buffer_size  */
  YYSYMBOL_server_http_response_buffer_size = 439, /* server_http_response_buffer_size  */
  YYSYMBOL_server_http_nodelay = 440,      /* server_http_nodelay  */
  YYSYMBOL_server_http_notls_downstream = 441, /* server_http_notls_downstream  */
  YYSYMBOL_server_use_systemd = 442,       /* server_use_systemd  */
  YYSYMBOL_server_do_daemonize = 443,      /* server_do_daemonize  */
  YYSYMBOL_server_use_syslog = 444,        /* server_use_syslog  */
  YYSYMBOL_server_log_time_ascii = 445,    /* server_log_time_ascii  */
  YYSYMBOL_server_log_queries = 446,       /* server_log_queries  */
  YYSYMBOL_server_log_replies = 447,       /* server_log_replies  */
  YYSYMBOL_server_log_tag_queryreply = 448, /* server_log_tag_queryreply  */
  YYSYMBOL_server_log_servfail = 449,      /* server_log_servfail  */
  YYSYMBOL_server_log_destaddr = 450,      /* server_log_destaddr  */
  YYSYMBOL_server_log_local_actions = 451, /* server_log_local_actions  */
  YYSYMBOL_server_chroot = 452,            /* server_chroot  */
  YYSYMBOL_server_username = 453,          /* server_username  */
  YYSYMBOL_server_directory = 454,         /* server_directory  */
  YYSYMBOL_server_logfile = 455,           /* server_logfile  */
  YYSYMBOL_server_pidfile = 456,           /* server_pidfile  */
  YYSYMBOL_server_root_hints = 457,        /* server_root_hints  */
  YYSYMBOL_server_dlv_anchor_file = 458,   /* server_dlv_anchor_file  */
  YYSYMBOL_server_dlv_anchor = 459,        /* server_dlv_anchor  */
  YYSYMBOL_server_auto_trust_anchor_file = 460, /* server_auto_trust_anchor_file  */
  YYSYMBOL_server_trust_anchor_file = 461, /* server_trust_anchor_file  */
  YYSYMBOL_server_trusted_keys_file = 462, /* server_trusted_keys_file  */
  YYSYMBOL_server_trust_anchor = 463,      /* server_trust_anchor  */
  YYSYMBOL_server_trust_anchor_signaling = 464, /* server_trust_anchor_signaling  */
  YYSYMBOL_server_root_key_sentinel = 465, /* server_root_key_sentinel  */
  YYSYMBOL_server_domain_insecure = 466,   /* server_domain_insecure  */
  YYSYMBOL_server_hide_identity = 467,     /* server_hide_identity  */
  YYSYMBOL_server_hide_version = 468,      /* server_hide_version  */
  YYSYMBOL_server_hide_trustanchor = 469,  /* server_hide_trustanchor  */
  YYSYMBOL_server_hide_http_user_agent = 470, /* server_hide_http_user_agent  */
  YYSYMBOL_server_identity = 471,          /* server_identity  */
  YYSYMBOL_server_version = 472,           /* server_version  */
  YYSYMBOL_server_http_user_agent = 473,   /* server_http_user_agent  */
  YYSYMBOL_server_nsid = 474,              /* server_nsid  */
  YYSYMBOL_server_so_rcvbuf = 475,         /* server_so_rcvbuf  */
  YYSYMBOL_server_so_sndbuf = 476,         /* server_so_sndbuf  */
  YYSYMBOL_server_so_reuseport = 477,      /* server_so_reuseport  */
  YYSYMBOL_server_ip_transparent = 478,    /* server_ip_transparent  */
  YYSYMBOL_server_ip_freebind = 479,       /* server_ip_freebind  */
  YYSYMBOL_server_ip_dscp = 480,           /* server_ip_dscp  */
  YYSYMBOL_server_stream_wait_size = 481,  /* server_stream_wait_size  */
  YYSYMBOL_server_edns_buffer_size = 482,  /* server_edns_buffer_size  */
  YYSYMBOL_server_msg_buffer_size = 483,   /* server_msg_buffer_size  */
  YYSYMBOL_server_msg_cache_size = 484,    /* server_msg_cache_size  */
  YYSYMBOL_server_msg_cache_slabs = 485,   /* server_msg_cache_slabs  */
  YYSYMBOL_server_num_queries_per_thread = 486, /* server_num_queries_per_thread  */
  YYSYMBOL_server_jostle_timeout = 487,    /* server_jostle_timeout  */
  YYSYMBOL_server_delay_close = 488,       /* server_delay_close  */
  YYSYMBOL_server_udp_connect = 489,       /* server_udp_connect  */
  YYSYMBOL_server_unblock_lan_zones = 490, /* server_unblock_lan_zones  */
  YYSYMBOL_server_insecure_lan_zones = 491, /* server_insecure_lan_zones  */
  YYSYMBOL_server_rrset_cache_size = 492,  /* server_rrset_cache_size  */
  YYSYMBOL_server_rrset_cache_slabs = 493, /* server_rrset_cache_slabs  */
  YYSYMBOL_server_infra_host_ttl = 494,    /* server_infra_host_ttl  */
  YYSYMBOL_server_infra_lame_ttl = 495,    /* server_infra_lame_ttl  */
  YYSYMBOL_server_infra_cache_numhosts = 496, /* server_infra_cache_numhosts  */
  YYSYMBOL_server_infra_cache_lame_size = 497, /* server_infra_cache_lame_size  */
  YYSYMBOL_server_infra_cache_slabs = 498, /* server_infra_cache_slabs  */
  YYSYMBOL_server_infra_cache_min_rtt = 499, /* server_infra_cache_min_rtt  */
  YYSYMBOL_server_infra_cache_max_rtt = 500, /* server_infra_cache_max_rtt  */
  YYSYMBOL_server_infra_keep_probing = 501, /* server_infra_keep_probing  */
  YYSYMBOL_server_target_fetch_policy = 502, /* server_target_fetch_policy  */
  YYSYMBOL_server_harden_short_bufsize = 503, /* server_harden_short_bufsize  */
  YYSYMBOL_server_harden_large_queries = 504, /* server_harden_large_queries  */
  YYSYMBOL_server_harden_glue = 505,       /* server_harden_glue  */
  YYSYMBOL_server_harden_dnssec_stripped = 506, /* server_harden_dnssec_stripped  */
  YYSYMBOL_server_harden_below_nxdomain = 507, /* server_harden_below_nxdomain  */
  YYSYMBOL_server_harden_referral_path = 508, /* server_harden_referral_path  */
  YYSYMBOL_server_harden_algo_downgrade = 509, /* server_harden_algo_downgrade  */
  YYSYMBOL_server_harden_unknown_additional = 510, /* server_harden_unknown_additional  */
  YYSYMBOL_server_use_caps_for_id = 511,   /* server_use_caps_for_id  */
  YYSYMBOL_server_caps_whitelist = 512,    /* server_caps_whitelist  */
  YYSYMBOL_server_private_address = 513,   /* server_private_address  */
  YYSYMBOL_server_private_domain = 514,    /* server_private_domain  */
  YYSYMBOL_server_prefetch = 515,          /* server_prefetch  */
  YYSYMBOL_server_prefetch_key = 516,      /* server_prefetch_key  */
  YYSYMBOL_server_deny_any = 517,          /* server_deny_any  */
  YYSYMBOL_server_unwanted_reply_threshold = 518, /* server_unwanted_reply_threshold  */
  YYSYMBOL_server_do_not_query_address = 519, /* server_do_not_query_address  */
  YYSYMBOL_server_do_not_query_localhost = 520, /* server_do_not_query_localhost  */
  YYSYMBOL_server_access_control = 521,    /* server_access_control  */
  YYSYMBOL_server_interface_action = 522,  /* server_interface_action  */
  YYSYMBOL_server_module_conf = 523,       /* server_module_conf  */
  YYSYMBOL_server_val_override_date = 524, /* server_val_override_date  */
  YYSYMBOL_server_val_sig_skew_min = 525,  /* server_val_sig_skew_min  */
  YYSYMBOL_server_val_sig_skew_max = 526,  /* server_val_sig_skew_max  */
  YYSYMBOL_server_val_max_restart = 527,   /* server_val_max_restart  */
  YYSYMBOL_server_cache_max_ttl = 528,     /* server_cache_max_ttl  */
  YYSYMBOL_server_cache_max_negative_ttl = 529, /* server_cache_max_negative_ttl  */
  YYSYMBOL_server_cache_min_ttl = 530,     /* server_cache_min_ttl  */
  YYSYMBOL_server_bogus_ttl = 531,         /* server_bogus_ttl  */
  YYSYMBOL_server_val_clean_additional = 532, /* server_val_clean_additional  */
  YYSYMBOL_server_val_permissive_mode = 533, /* server_val_permissive_mode  */
  YYSYMBOL_server_aggressive_nsec = 534,   /* server_aggressive_nsec  */
  YYSYMBOL_server_ignore_cd_flag = 535,    /* server_ignore_cd_flag  */
  YYSYMBOL_server_disable_edns_do = 536,   /* server_disable_edns_do  */
  YYSYMBOL_server_serve_expired = 537,     /* server_serve_expired  */
  YYSYMBOL_server_serve_expired_ttl = 538, /* server_serve_expired_ttl  */
  YYSYMBOL_server_serve_expired_ttl_reset = 539, /* server_serve_expired_ttl_reset  */
  YYSYMBOL_server_serve_expired_reply_ttl = 540, /* server_serve_expired_reply_ttl  */
  YYSYMBOL_server_serve_expired_client_timeout = 541, /* server_serve_expired_client_timeout  */
  YYSYMBOL_server_ede_serve_expired = 542, /* server_ede_serve_expired  */
  YYSYMBOL_server_serve_original_ttl = 543, /* server_serve_original_ttl  */
  YYSYMBOL_server_fake_dsa = 544,          /* server_fake_dsa  */
  YYSYMBOL_server_fake_sha1 = 545,         /* server_fake_sha1  */
  YYSYMBOL_server_val_log_level = 546,     /* server_val_log_level  */
  YYSYMBOL_server_val_nsec3_keysize_iterations = 547, /* server_val_nsec3_keysize_iterations  */
  YYSYMBOL_server_zonemd_permissive_mode = 548, /* server_zonemd_permissive_mode  */
  YYSYMBOL_server_add_holddown = 549,      /* server_add_holddown  */
  YYSYMBOL_server_del_holddown = 550,      /* server_del_holddown  */
  YYSYMBOL_server_keep_missing = 551,      /* server_keep_missing  */
  YYSYMBOL_server_permit_small_holddown = 552, /* server_permit_small_holddown  */
  YYSYMBOL_server_key_cache_size = 553,    /* server_key_cache_size  */
  YYSYMBOL_server_key_cache_slabs = 554,   /* server_key_cache_slabs  */
  YYSYMBOL_server_neg_cache_size = 555,    /* server_neg_cache_size  */
  YYSYMBOL_server_local_zone = 556,        /* server_local_zone  */
  YYSYMBOL_server_local_data = 557,        /* server_local_data  */
  YYSYMBOL_server_local_data_ptr = 558,    /* server_local_data_ptr  */
  YYSYMBOL_server_minimal_responses = 559, /* server_minimal_responses  */
  YYSYMBOL_server_rrset_roundrobin = 560,  /* server_rrset_roundrobin  */
  YYSYMBOL_server_unknown_server_time_limit = 561, /* server_unknown_server_time_limit  */
  YYSYMBOL_server_max_udp_size = 562,      /* server_max_udp_size  */
  YYSYMBOL_server_dns64_prefix = 563,      /* server_dns64_prefix  */
  YYSYMBOL_server_dns64_synthall = 564,    /* server_dns64_synthall  */
  YYSYMBOL_server_dns64_ignore_aaaa = 565, /* server_dns64_ignore_aaaa  */
  YYSYMBOL_server_nat64_prefix = 566,      /* server_nat64_prefix  */
  YYSYMBOL_server_define_tag = 567,        /* server_define_tag  */
  YYSYMBOL_server_local_zone_tag = 568,    /* server_local_zone_tag  */
  YYSYMBOL_server_access_control_tag = 569, /* server_access_control_tag  */
  YYSYMBOL_server_access_control_tag_action = 570, /* server_access_control_tag_action  */
  YYSYMBOL_server_access_control_tag_data = 571, /* server_access_control_tag_data  */
  YYSYMBOL_server_local_zone_override = 572, /* server_local_zone_override  */
  YYSYMBOL_server_access_control_view = 573, /* server_access_control_view  */
  YYSYMBOL_server_interface_tag = 574,     /* server_interface_tag  */
  YYSYMBOL_server_interface_tag_action = 575, /* server_interface_tag_action  */
  YYSYMBOL_server_interface_tag_data = 576, /* server_interface_tag_data  */
  YYSYMBOL_server_interface_view = 577,    /* server_interface_view  */
  YYSYMBOL_server_response_ip_tag = 578,   /* server_response_ip_tag  */
  YYSYMBOL_server_ip_ratelimit = 579,      /* server_ip_ratelimit  */
  YYSYMBOL_server_ip_ratelimit_cookie = 580, /* server_ip_ratelimit_cookie  */
  YYSYMBOL_server_ratelimit = 581,         /* server_ratelimit  */
  YYSYMBOL_server_ip_ratelimit_size = 582, /* server_ip_ratelimit_size  */
  YYSYMBOL_server_ratelimit_size = 583,    /* server_ratelimit_size  */
  YYSYMBOL_server_ip_ratelimit_slabs = 584, /* server_ip_ratelimit_slabs  */
  YYSYMBOL_server_ratelimit_slabs = 585,   /* server_ratelimit_slabs  */
  YYSYMBOL_server_ratelimit_for_domain = 586, /* server_ratelimit_for_domain  */
  YYSYMBOL_server_ratelimit_below_domain = 587, /* server_ratelimit_below_domain  */
  YYSYMBOL_server_ip_ratelimit_factor = 588, /* server_ip_ratelimit_factor  */
  YYSYMBOL_server_ratelimit_factor = 589,  /* server_ratelimit_factor  */
  YYSYMBOL_server_ip_ratelimit_backoff = 590, /* server_ip_ratelimit_backoff  */
  YYSYMBOL_server_ratelimit_backoff = 591, /* server_ratelimit_backoff  */
  YYSYMBOL_server_outbound_msg_retry = 592, /* server_outbound_msg_retry  */
  YYSYMBOL_server_max_sent_count = 593,    /* server_max_sent_count  */
  YYSYMBOL_server_max_query_restarts = 594, /* server_max_query_restarts  */
  YYSYMBOL_server_low_rtt = 595,           /* server_low_rtt  */
  YYSYMBOL_server_fast_server_num = 596,   /* server_fast_server_num  */
  YYSYMBOL_server_fast_server_permil = 597, /* server_fast_server_permil  */
  YYSYMBOL_server_qname_minimisation = 598, /* server_qname_minimisation  */
  YYSYMBOL_server_qname_minimisation_strict = 599, /* server_qname_minimisation_strict  */
  YYSYMBOL_server_pad_responses = 600,     /* server_pad_responses  */
  YYSYMBOL_server_pad_responses_block_size = 601, /* server_pad_responses_block_size  */
  YYSYMBOL_server_pad_queries = 602,       /* server_pad_queries  */
  YYSYMBOL_server_pad_queries_block_size = 603, /* server_pad_queries_block_size  */
  YYSYMBOL_server_ipsecmod_enabled = 604,  /* server_ipsecmod_enabled  */
  YYSYMBOL_server_ipsecmod_ignore_bogus = 605, /* server_ipsecmod_ignore_bogus  */
  YYSYMBOL_server_ipsecmod_hook = 606,     /* server_ipsecmod_hook  */
  YYSYMBOL_server_ipsecmod_max_ttl = 607,  /* server_ipsecmod_max_ttl  */
  YYSYMBOL_server_ipsecmod_whitelist = 608, /* server_ipsecmod_whitelist  */
  YYSYMBOL_server_ipsecmod_strict = 609,   /* server_ipsecmod_strict  */
  YYSYMBOL_server_edns_client_string = 610, /* server_edns_client_string  */
  YYSYMBOL_server_edns_client_string_opcode = 611, /* server_edns_client_string_opcode  */
  YYSYMBOL_server_ede = 612,               /* server_ede  */
  YYSYMBOL_server_proxy_protocol_port = 613, /* server_proxy_protocol_port  */
  YYSYMBOL_stub_name = 614,                /* stub_name  */
  YYSYMBOL_stub_host = 615,                /* stub_host  */
  YYSYMBOL_stub_addr = 616,                /* stub_addr  */
  YYSYMBOL_stub_first = 617,               /* stub_first  */
  YYSYMBOL_stub_no_cache = 618,            /* stub_no_cache  */
  YYSYMBOL_stub_ssl_upstream = 619,        /* stub_ssl_upstream  */
  YYSYMBOL_stub_tcp_upstream = 620,        /* stub_tcp_upstream  */
  YYSYMBOL_stub_prime = 621,               /* stub_prime  */
  YYSYMBOL_forward_name = 622,             /* forward_name  */
  YYSYMBOL_forward_host = 623,             /* forward_host  */
  YYSYMBOL_forward_addr = 624,             /* forward_addr  */
  YYSYMBOL_forward_first = 625,            /* forward_first  */
  YYSYMBOL_forward_no_cache = 626,         /* forward_no_cache  */
  YYSYMBOL_forward_ssl_upstream = 627,     /* forward_ssl_upstream  */
  YYSYMBOL_forward_tcp_upstream = 628,     /* forward_tcp_upstream  */
  YYSYMBOL_auth_name = 629,                /* auth_name  */
  YYSYMBOL_auth_zonefile = 630,            /* auth_zonefile  */
  YYSYMBOL_auth_master = 631,              /* auth_master  */
  YYSYMBOL_auth_url = 632,                 /* auth_url  */
  YYSYMBOL_auth_allow_notify = 633,        /* auth_allow_notify  */
  YYSYMBOL_auth_zonemd_check = 634,        /* auth_zonemd_check  */
  YYSYMBOL_auth_zonemd_reject_absence = 635, /* auth_zonemd_reject_absence  */
  YYSYMBOL_auth_for_downstream = 636,      /* auth_for_downstream  */
  YYSYMBOL_auth_for_upstream = 637,        /* auth_for_upstream  */
  YYSYMBOL_auth_fallback_enabled = 638,    /* auth_fallback_enabled  */
  YYSYMBOL_view_name = 639,                /* view_name  */
  YYSYMBOL_view_local_zone = 640,          /* view_local_zone  */
  YYSYMBOL_view_response_ip = 641,         /* view_response_ip  */
  YYSYMBOL_view_response_ip_data = 642,    /* view_response_ip_data  */
  YYSYMBOL_view_local_data = 643,          /* view_local_data  */
  YYSYMBOL_view_local_data_ptr = 644,      /* view_local_data_ptr  */
  YYSYMBOL_view_first = 645,               /* view_first  */
  YYSYMBOL_rcstart = 646,                  /* rcstart  */
  YYSYMBOL_contents_rc = 647,              /* contents_rc  */
  YYSYMBOL_content_rc = 648,               /* content_rc  */
  YYSYMBOL_rc_control_enable = 649,        /* rc_control_enable  */
  YYSYMBOL_rc_control_port = 650,          /* rc_control_port  */
  YYSYMBOL_rc_control_interface = 651,     /* rc_control_interface  */
  YYSYMBOL_rc_control_use_cert = 652,      /* rc_control_use_cert  */
  YYSYMBOL_rc_server_key_file = 653,       /* rc_server_key_file  */
  YYSYMBOL_rc_server_cert_file = 654,      /* rc_server_cert_file  */
  YYSYMBOL_rc_control_key_file = 655,      /* rc_control_key_file  */
  YYSYMBOL_rc_control_cert_file = 656,     /* rc_control_cert_file  */
  YYSYMBOL_dtstart = 657,                  /* dtstart  */
  YYSYMBOL_contents_dt = 658,              /* contents_dt  */
  YYSYMBOL_content_dt = 659,               /* content_dt  */
  YYSYMBOL_dt_dnstap_enable = 660,         /* dt_dnstap_enable  */
  YYSYMBOL_dt_dnstap_bidirectional = 661,  /* dt_dnstap_bidirectional  */
  YYSYMBOL_dt_dnstap_socket_path = 662,    /* dt_dnstap_socket_path  */
  YYSYMBOL_dt_dnstap_ip = 663,             /* dt_dnstap_ip  */
  YYSYMBOL_dt_dnstap_tls = 664,            /* dt_dnstap_tls  */
  YYSYMBOL_dt_dnstap_tls_server_name = 665, /* dt_dnstap_tls_server_name  */
  YYSYMBOL_dt_dnstap_tls_cert_bundle = 666, /* dt_dnstap_tls_cert_bundle  */
  YYSYMBOL_dt_dnstap_tls_client_key_file = 667, /* dt_dnstap_tls_client_key_file  */
  YYSYMBOL_dt_dnstap_tls_client_cert_file = 668, /* dt_dnstap_tls_client_cert_file  */
  YYSYMBOL_dt_dnstap_send_identity = 669,  /* dt_dnstap_send_identity  */
  YYSYMBOL_dt_dnstap_send_version = 670,   /* dt_dnstap_send_version  */
  YYSYMBOL_dt_dnstap_identity = 671,       /* dt_dnstap_identity  */
  YYSYMBOL_dt_dnstap_version = 672,        /* dt_dnstap_version  */
  YYSYMBOL_dt_dnstap_log_resolver_query_messages = 673, /* dt_dnstap_log_resolver_query_messages  */
  YYSYMBOL_dt_dnstap_log_resolver_response_messages = 674, /* dt_dnstap_log_resolver_response_messages  */
  YYSYMBOL_dt_dnstap_log_client_query_messages = 675, /* dt_dnstap_log_client_query_messages  */
  YYSYMBOL_dt_dnstap_log_client_response_messages = 676, /* dt_dnstap_log_client_response_messages  */
  YYSYMBOL_dt_dnstap_log_forwarder_query_messages = 677, /* dt_dnstap_log_forwarder_query_messages  */
  YYSYMBOL_dt_dnstap_log_forwarder_response_messages = 678, /* dt_dnstap_log_forwarder_response_messages  */
  YYSYMBOL_pythonstart = 679,              /* pythonstart  */
  YYSYMBOL_contents_py = 680,              /* contents_py  */
  YYSYMBOL_content_py = 681,               /* content_py  */
  YYSYMBOL_py_script = 682,                /* py_script  */
  YYSYMBOL_dynlibstart = 683,              /* dynlibstart  */
  YYSYMBOL_contents_dl = 684,              /* contents_dl  */
  YYSYMBOL_content_dl = 685,               /* content_dl  */
  YYSYMBOL_dl_file = 686,                  /* dl_file  */
  YYSYMBOL_server_disable_dnssec_lame_check = 687, /* server_disable_dnssec_lame_check  */
  YYSYMBOL_server_log_identity = 688,      /* server_log_identity  */
  YYSYMBOL_server_response_ip = 689,       /* server_response_ip  */
  YYSYMBOL_server_response_ip_data = 690,  /* server_response_ip_data  */
  YYSYMBOL_dnscstart = 691,                /* dnscstart  */
  YYSYMBOL_contents_dnsc = 692,            /* contents_dnsc  */
  YYSYMBOL_content_dnsc = 693,             /* content_dnsc  */
  YYSYMBOL_dnsc_dnscrypt_enable = 694,     /* dnsc_dnscrypt_enable  */
  YYSYMBOL_dnsc_dnscrypt_port = 695,       /* dnsc_dnscrypt_port  */
  YYSYMBOL_dnsc_dnscrypt_provider = 696,   /* dnsc_dnscrypt_provider  */
  YYSYMBOL_dnsc_dnscrypt_provider_cert = 697, /* dnsc_dnscrypt_provider_cert  */
  YYSYMBOL_dnsc_dnscrypt_provider_cert_rotated = 698, /* dnsc_dnscrypt_provider_cert_rotated  */
  YYSYMBOL_dnsc_dnscrypt_secret_key = 699, /* dnsc_dnscrypt_secret_key  */
  YYSYMBOL_dnsc_dnscrypt_shared_secret_cache_size = 700, /* dnsc_dnscrypt_shared_secret_cache_size  */
  YYSYMBOL_dnsc_dnscrypt_shared_secret_cache_slabs = 701, /* dnsc_dnscrypt_shared_secret_cache_slabs  */
  YYSYMBOL_dnsc_dnscrypt_nonce_cache_size = 702, /* dnsc_dnscrypt_nonce_cache_size  */
  YYSYMBOL_dnsc_dnscrypt_nonce_cache_slabs = 703, /* dnsc_dnscrypt_nonce_cache_slabs  */
  YYSYMBOL_cachedbstart = 704,             /* cachedbstart  */
  YYSYMBOL_contents_cachedb = 705,         /* contents_cachedb  */
  YYSYMBOL_content_cachedb = 706,          /* content_cachedb  */
  YYSYMBOL_cachedb_backend_name = 707,     /* cachedb_backend_name  */
  YYSYMBOL_cachedb_secret_seed = 708,      /* cachedb_secret_seed  */
  YYSYMBOL_cachedb_no_store = 709,         /* cachedb_no_store  */
  YYSYMBOL_redis_server_host = 710,        /* redis_server_host  */
  YYSYMBOL_redis_server_port = 711,        /* redis_server_port  */
  YYSYMBOL_redis_server_path = 712,        /* redis_server_path  */
  YYSYMBOL_redis_server_password = 713,    /* redis_server_password  */
  YYSYMBOL_redis_timeout = 714,            /* redis_timeout  */
  YYSYMBOL_redis_expire_records = 715,     /* redis_expire_records  */
  YYSYMBOL_redis_logical_db = 716,         /* redis_logical_db  */
  YYSYMBOL_server_tcp_connection_limit = 717, /* server_tcp_connection_limit  */
  YYSYMBOL_server_answer_cookie = 718,     /* server_answer_cookie  */
  YYSYMBOL_server_cookie_secret = 719,     /* server_cookie_secret  */
  YYSYMBOL_ipsetstart = 720,               /* ipsetstart  */
  YYSYMBOL_contents_ipset = 721,           /* contents_ipset  */
  YYSYMBOL_content_ipset = 722,            /* content_ipset  */
  YYSYMBOL_ipset_name_v4 = 723,            /* ipset_name_v4  */
  YYSYMBOL_ipset_name_v6 = 724             /* ipset_name_v6  */
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
#define YYLAST   745

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  350
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  375
/* YYNRULES -- Number of rules.  */
#define YYNRULES  727
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  1088

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   604


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
     345,   346,   347,   348,   349
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   206,   206,   206,   207,   207,   208,   208,   209,   209,
     209,   210,   210,   211,   211,   212,   212,   213,   215,   222,
     228,   229,   230,   230,   230,   231,   231,   232,   232,   232,
     233,   233,   233,   234,   234,   234,   235,   235,   236,   237,
     237,   237,   238,   238,   238,   239,   239,   240,   240,   241,
     241,   242,   242,   243,   243,   244,   244,   245,   245,   246,
     246,   247,   247,   247,   248,   248,   249,   249,   249,   250,
     250,   250,   251,   251,   252,   252,   253,   253,   254,   254,
     255,   255,   255,   256,   256,   257,   257,   258,   258,   258,
     259,   259,   260,   260,   261,   261,   262,   262,   262,   263,
     263,   264,   264,   265,   265,   266,   266,   267,   267,   268,
     268,   269,   269,   270,   270,   271,   271,   271,   272,   272,
     272,   273,   273,   273,   274,   274,   274,   274,   275,   276,
     276,   276,   277,   277,   277,   278,   278,   279,   279,   280,
     280,   280,   281,   281,   281,   282,   282,   283,   283,   283,
     284,   285,   285,   285,   286,   286,   286,   287,   287,   288,
     288,   289,   289,   290,   291,   291,   292,   292,   293,   293,
     294,   294,   295,   295,   296,   296,   297,   297,   298,   298,
     299,   299,   300,   300,   301,   301,   302,   302,   302,   303,
     303,   304,   304,   305,   305,   306,   306,   306,   307,   307,
     308,   309,   309,   310,   310,   311,   312,   312,   313,   313,
     314,   314,   314,   315,   315,   316,   316,   316,   317,   317,
     317,   318,   318,   319,   320,   320,   321,   321,   322,   322,
     323,   323,   324,   324,   324,   325,   325,   325,   326,   326,
     326,   327,   327,   328,   328,   329,   329,   330,   330,   330,
     331,   331,   332,   332,   333,   333,   334,   334,   335,   335,
     336,   336,   337,   337,   338,   340,   354,   355,   356,   356,
     356,   356,   356,   357,   357,   357,   359,   373,   374,   375,
     375,   375,   375,   376,   376,   376,   378,   394,   395,   396,
     396,   396,   396,   397,   397,   397,   399,   420,   421,   422,
     422,   422,   422,   423,   423,   423,   424,   424,   424,   427,
     446,   463,   471,   481,   488,   498,   517,   518,   519,   519,
     519,   519,   519,   520,   520,   520,   521,   521,   521,   521,
     523,   532,   541,   552,   561,   570,   579,   588,   599,   608,
     620,   634,   649,   660,   677,   694,   711,   728,   743,   758,
     771,   786,   795,   804,   813,   822,   831,   840,   847,   856,
     865,   874,   883,   892,   901,   910,   919,   928,   941,   952,
     963,   974,   983,   996,  1009,  1018,  1027,  1036,  1043,  1050,
    1059,  1066,  1075,  1083,  1090,  1097,  1105,  1114,  1122,  1138,
    1146,  1154,  1162,  1170,  1178,  1187,  1196,  1210,  1219,  1228,
    1237,  1246,  1255,  1264,  1273,  1280,  1287,  1313,  1321,  1328,
    1335,  1342,  1349,  1357,  1365,  1373,  1380,  1391,  1402,  1409,
    1418,  1427,  1436,  1445,  1452,  1459,  1466,  1482,  1490,  1498,
    1508,  1518,  1528,  1542,  1550,  1563,  1574,  1582,  1595,  1604,
    1613,  1622,  1631,  1641,  1651,  1659,  1672,  1681,  1689,  1698,
    1706,  1719,  1728,  1737,  1747,  1754,  1764,  1774,  1784,  1794,
    1804,  1814,  1824,  1834,  1844,  1851,  1858,  1865,  1874,  1883,
    1892,  1901,  1908,  1918,  1926,  1935,  1942,  1960,  1973,  1986,
    1999,  2008,  2017,  2026,  2035,  2045,  2055,  2066,  2075,  2084,
    2093,  2102,  2111,  2120,  2129,  2138,  2147,  2160,  2173,  2182,
    2189,  2198,  2207,  2216,  2225,  2235,  2243,  2256,  2264,  2320,
    2327,  2342,  2352,  2362,  2369,  2376,  2383,  2392,  2400,  2407,
    2421,  2442,  2463,  2475,  2487,  2499,  2508,  2529,  2541,  2553,
    2562,  2583,  2592,  2601,  2610,  2618,  2626,  2639,  2652,  2667,
    2682,  2691,  2700,  2710,  2720,  2729,  2738,  2747,  2753,  2762,
    2771,  2781,  2791,  2801,  2810,  2820,  2829,  2842,  2855,  2867,
    2881,  2893,  2907,  2916,  2927,  2936,  2943,  2953,  2960,  2967,
    2976,  2985,  2995,  3005,  3015,  3025,  3032,  3039,  3048,  3057,
    3067,  3077,  3087,  3094,  3101,  3108,  3116,  3126,  3136,  3146,
    3156,  3166,  3176,  3232,  3242,  3250,  3258,  3273,  3282,  3288,
    3289,  3290,  3290,  3290,  3291,  3291,  3291,  3292,  3292,  3294,
    3304,  3313,  3320,  3327,  3334,  3341,  3348,  3355,  3361,  3362,
    3363,  3363,  3363,  3364,  3364,  3364,  3365,  3366,  3366,  3367,
    3367,  3368,  3368,  3369,  3370,  3371,  3372,  3373,  3374,  3376,
    3385,  3395,  3402,  3409,  3418,  3425,  3432,  3439,  3446,  3455,
    3464,  3471,  3478,  3488,  3498,  3508,  3518,  3528,  3538,  3544,
    3545,  3546,  3548,  3555,  3561,  3562,  3563,  3565,  3572,  3582,
    3589,  3598,  3606,  3612,  3613,  3615,  3615,  3615,  3616,  3616,
    3617,  3618,  3619,  3620,  3621,  3623,  3632,  3641,  3648,  3657,
    3664,  3673,  3681,  3694,  3702,  3715,  3721,  3722,  3723,  3723,
    3724,  3724,  3724,  3725,  3725,  3725,  3726,  3726,  3728,  3740,
    3752,  3765,  3777,  3792,  3804,  3816,  3829,  3842,  3857,  3868,
    3877,  3893,  3899,  3900,  3901,  3901,  3903,  3918
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
  "VAR_CACHEDB_REDISEXPIRERECORDS", "VAR_CACHEDB_REDISPATH",
  "VAR_CACHEDB_REDISPASSWORD", "VAR_CACHEDB_REDISLOGICALDB",
  "VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM", "VAR_FOR_UPSTREAM",
  "VAR_AUTH_ZONE", "VAR_ZONEFILE", "VAR_MASTER", "VAR_URL",
  "VAR_FOR_DOWNSTREAM", "VAR_FALLBACK_ENABLED", "VAR_TLS_ADDITIONAL_PORT",
  "VAR_LOW_RTT", "VAR_LOW_RTT_PERMIL", "VAR_FAST_SERVER_PERMIL",
  "VAR_FAST_SERVER_NUM", "VAR_ALLOW_NOTIFY", "VAR_TLS_WIN_CERT",
  "VAR_TCP_CONNECTION_LIMIT", "VAR_ANSWER_COOKIE", "VAR_COOKIE_SECRET",
  "VAR_IP_RATELIMIT_COOKIE", "VAR_FORWARD_NO_CACHE", "VAR_STUB_NO_CACHE",
  "VAR_LOG_SERVFAIL", "VAR_DENY_ANY", "VAR_UNKNOWN_SERVER_TIME_LIMIT",
  "VAR_LOG_TAG_QUERYREPLY", "VAR_STREAM_WAIT_SIZE", "VAR_TLS_CIPHERS",
  "VAR_TLS_CIPHERSUITES", "VAR_TLS_USE_SNI", "VAR_IPSET",
  "VAR_IPSET_NAME_V4", "VAR_IPSET_NAME_V6", "VAR_TLS_SESSION_TICKET_KEYS",
  "VAR_RPZ", "VAR_TAGS", "VAR_RPZ_ACTION_OVERRIDE",
  "VAR_RPZ_CNAME_OVERRIDE", "VAR_RPZ_LOG", "VAR_RPZ_LOG_NAME",
  "VAR_DYNLIB", "VAR_DYNLIB_FILE", "VAR_EDNS_CLIENT_STRING",
  "VAR_EDNS_CLIENT_STRING_OPCODE", "VAR_NSID",
  "VAR_ZONEMD_PERMISSIVE_MODE", "VAR_ZONEMD_CHECK",
  "VAR_ZONEMD_REJECT_ABSENCE", "VAR_RPZ_SIGNAL_NXDOMAIN_RA",
  "VAR_INTERFACE_AUTOMATIC_PORTS", "VAR_EDE", "VAR_INTERFACE_ACTION",
  "VAR_INTERFACE_VIEW", "VAR_INTERFACE_TAG", "VAR_INTERFACE_TAG_ACTION",
  "VAR_INTERFACE_TAG_DATA", "VAR_PROXY_PROTOCOL_PORT",
  "VAR_STATISTICS_INHIBIT_ZERO", "VAR_HARDEN_UNKNOWN_ADDITIONAL",
  "VAR_DISABLE_EDNS_DO", "VAR_CACHEDB_NO_STORE", "VAR_LOG_DESTADDR",
  "$accept", "toplevelvars", "toplevelvar", "force_toplevel",
  "serverstart", "contents_server", "content_server", "stubstart",
  "contents_stub", "content_stub", "forwardstart", "contents_forward",
  "content_forward", "viewstart", "contents_view", "content_view",
  "authstart", "contents_auth", "content_auth", "rpz_tag",
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
  "server_https_port", "server_http_endpoint", "server_http_max_streams",
  "server_http_query_buffer_size", "server_http_response_buffer_size",
  "server_http_nodelay", "server_http_notls_downstream",
  "server_use_systemd", "server_do_daemonize", "server_use_syslog",
  "server_log_time_ascii", "server_log_queries", "server_log_replies",
  "server_log_tag_queryreply", "server_log_servfail",
  "server_log_destaddr", "server_log_local_actions", "server_chroot",
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
  "server_cache_min_ttl", "server_bogus_ttl",
  "server_val_clean_additional", "server_val_permissive_mode",
  "server_aggressive_nsec", "server_ignore_cd_flag",
  "server_disable_edns_do", "server_serve_expired",
  "server_serve_expired_ttl", "server_serve_expired_ttl_reset",
  "server_serve_expired_reply_ttl", "server_serve_expired_client_timeout",
  "server_ede_serve_expired", "server_serve_original_ttl",
  "server_fake_dsa", "server_fake_sha1", "server_val_log_level",
  "server_val_nsec3_keysize_iterations", "server_zonemd_permissive_mode",
  "server_add_holddown", "server_del_holddown", "server_keep_missing",
  "server_permit_small_holddown", "server_key_cache_size",
  "server_key_cache_slabs", "server_neg_cache_size", "server_local_zone",
  "server_local_data", "server_local_data_ptr", "server_minimal_responses",
  "server_rrset_roundrobin", "server_unknown_server_time_limit",
  "server_max_udp_size", "server_dns64_prefix", "server_dns64_synthall",
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
  "cachedb_backend_name", "cachedb_secret_seed", "cachedb_no_store",
  "redis_server_host", "redis_server_port", "redis_server_path",
  "redis_server_password", "redis_timeout", "redis_expire_records",
  "redis_logical_db", "server_tcp_connection_limit",
  "server_answer_cookie", "server_cookie_secret", "ipsetstart",
  "contents_ipset", "content_ipset", "ipset_name_v4", "ipset_name_v6", YY_NULLPTR
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
     585,   586,   587,   588,   589,   590,   591,   592,   593,   594,
     595,   596,   597,   598,   599,   600,   601,   602,   603,   604
};
#endif

#define YYPACT_NINF (-293)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -293,   267,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,   -13,   262,   122,   276,    56,    45,   293,   268,
     -81,  -292,  -100,   -35,  -285,    31,    32,    33,    75,    76,
      77,    78,    79,    80,    81,    83,    89,    94,   121,   123,
     125,   126,   134,   160,   161,   162,   163,   164,   165,   166,
     213,   215,   244,   245,   259,   260,   263,   264,   265,   270,
     271,   272,   273,   279,   285,   286,   295,   296,   297,   300,
     304,   306,   312,   325,   331,   332,   333,   334,   335,   343,
     344,   345,   346,   347,   351,   352,   353,   354,   356,   357,
     363,   365,   366,   367,   368,   369,   370,   372,   374,   375,
     377,   378,   379,   390,   391,   392,   393,   394,   395,   396,
     398,   399,   401,   402,   403,   404,   405,   406,   407,   408,
     410,   411,   412,   413,   414,   415,   416,   417,   418,   419,
     421,   422,   423,   424,   425,   426,   427,   428,   430,   450,
     451,   452,   453,   454,   455,   456,   457,   458,   461,   462,
     463,   464,   465,   466,   467,   468,   469,   470,   471,   472,
     473,   474,   475,   476,   477,   478,   479,   480,   481,   482,
     483,   484,   485,   486,   487,   488,   489,   490,   491,   492,
     494,   495,   496,   497,   498,   499,   500,   501,   502,   503,
     505,   506,   507,   508,   509,   510,   511,   512,   513,   514,
     516,   517,   518,   519,   520,   521,   522,   523,   524,   525,
     526,   527,   528,   529,   530,   531,   532,   533,   534,   535,
     537,   538,   539,   540,   541,   542,   543,   544,   545,   546,
     547,   549,   550,   551,   552,   553,   554,   555,   556,   557,
     558,   559,   560,   562,   563,   564,   565,   566,   567,   568,
     569,   570,   571,   572,   573,   574,   576,   577,   578,   580,
     581,   582,   583,   584,   586,   587,   588,   589,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,   590,   591,   592,   593,   594,   595,   596,   597,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,   598,
     599,   600,   601,   602,   603,   604,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,   605,   606,   607,   608,   609,   610,
     611,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,   612,
     613,   614,   615,   616,   617,   618,   619,   620,   621,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
     622,   623,   624,   625,   626,   627,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,   628,
     629,   630,   631,   632,   633,   634,   635,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,   636,   637,   638,   639,
     640,   641,   642,   643,   644,   645,   646,   647,   648,   649,
     650,   651,   652,   653,   654,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,   655,  -293,  -293,   656,  -293,
    -293,   657,   658,   659,   660,   661,   662,   663,   664,   665,
     666,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,   667,   668,   669,   670,   671,   672,   673,   674,
     675,   676,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,   677,   678,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
     679,   680,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,   681,   682,   683,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,   684,   685,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,   686,   687,   688,   689,   690,   691,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,   692,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,   693,  -293,  -293,  -293,  -293,
    -293,   694,   695,   696,   697,   698,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,   699,  -293,  -293,
     700,   701,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,   702,
     703,   704,  -293,  -293,  -293,  -293,  -293,  -293,   705,   706,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       2,     0,     1,    18,    19,   265,   276,   598,   658,   617,
     286,   672,   695,   296,   721,   315,   663,     3,    17,    21,
     267,   278,   288,   298,   317,   600,   619,   660,   665,   674,
     697,   723,     4,     5,     6,    10,    14,    15,     8,     9,
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
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    20,    22,
      23,    90,    93,   102,   261,   218,   219,    24,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   181,    39,    81,
      25,    94,    95,    50,    74,    89,   258,    26,    27,    28,
      31,    32,    29,    30,    33,    34,    35,   255,   256,   257,
      36,    37,    38,   126,   230,   127,   129,   130,   131,   232,
     237,   233,   244,   245,   246,   250,   132,   133,   134,   135,
     136,   137,   138,   214,    91,    80,   106,   124,   125,   242,
     239,   264,   128,    40,    41,    42,    43,    44,    82,    96,
      97,   113,    68,    78,    69,   222,   223,   107,    60,    61,
     221,    64,    62,    63,    65,   253,   117,   121,   142,   154,
     186,   157,   243,   118,    75,    45,    46,    47,   104,   143,
     144,   145,   146,    48,    49,    51,    52,    54,    55,    53,
     151,   152,   158,    56,    57,    58,    66,    85,   122,    99,
     153,   262,    92,   182,   100,   101,   119,   120,   240,   105,
      59,    83,    86,   195,    67,    70,   108,   109,   110,    84,
     183,   111,    71,    72,    73,   231,   123,   263,   205,   206,
     207,   208,   209,   210,   211,   212,   220,   112,    79,   254,
     114,   115,   116,   184,    76,    77,    98,    87,    88,   103,
     139,   140,   241,   141,   147,   148,   149,   150,   187,   188,
     190,   192,   193,   191,   194,   197,   198,   199,   196,   215,
     155,   249,   156,   161,   162,   159,   160,   163,   164,   166,
     165,   168,   167,   169,   170,   171,   234,   236,   235,   185,
     200,   201,   202,   203,   204,   224,   226,   225,   227,   228,
     229,   251,   252,   259,   260,   189,   213,   216,   217,   238,
     247,   248,     0,     0,     0,     0,     0,     0,     0,     0,
     266,   268,   269,   270,   272,   273,   274,   275,   271,     0,
       0,     0,     0,     0,     0,     0,   277,   279,   280,   281,
     282,   283,   284,   285,     0,     0,     0,     0,     0,     0,
       0,   287,   289,   290,   293,   294,   291,   295,   292,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   297,
     299,   300,   301,   302,   306,   307,   308,   303,   304,   305,
       0,     0,     0,     0,     0,     0,   320,   324,   325,   326,
     327,   328,   316,   318,   319,   321,   322,   323,   329,     0,
       0,     0,     0,     0,     0,     0,     0,   599,   601,   603,
     602,   608,   604,   605,   606,   607,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   618,   620,   622,   621,   623,
     624,   625,   626,   627,   628,   629,   630,   631,   632,   633,
     634,   635,   636,   637,   638,     0,   659,   661,     0,   664,
     666,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   673,   675,   676,   677,   679,   680,   678,   681,   682,
     683,   684,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   696,   698,   699,   706,   700,   701,   704,   705,
     702,   703,   707,     0,     0,   722,   724,   725,   331,   330,
     338,   351,   349,   363,   358,   359,   360,   364,   361,   362,
     365,   366,   367,   371,   372,   373,   404,   405,   406,   407,
     408,   436,   437,   438,   444,   445,   354,   446,   447,   450,
     448,   449,   454,   455,   456,   471,   419,   420,   423,   424,
     457,   475,   413,   415,   476,   483,   484,   485,   355,   435,
     505,   506,   414,   499,   396,   350,   409,   472,   480,   458,
       0,     0,   509,   356,   332,   395,   463,   333,   352,   353,
     410,   411,   507,   460,   465,   466,   369,   368,   334,   510,
     439,   470,   397,   418,   477,   478,   479,   482,   498,   412,
     503,   501,   502,   427,   434,   467,   468,   428,   429,   459,
     487,   398,   399,   403,   374,   376,   370,   377,   378,   379,
     380,   387,   388,   389,   390,   391,   392,   393,   511,   512,
     514,   440,   441,   442,   443,   451,   452,   453,   515,   516,
     517,   518,     0,     0,     0,   461,   430,   432,   668,   531,
     536,   534,   533,   537,   535,   544,   545,   546,     0,     0,
     540,   541,   542,   543,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   464,   481,   504,   550,   551,   431,
     519,     0,     0,     0,     0,     0,     0,   489,   490,   491,
     492,   493,   494,   495,   496,   497,   669,   421,   422,   425,
     416,   486,   394,   336,   337,   417,   552,   553,   554,   555,
     556,   558,   557,   559,   560,   561,   375,   382,   547,   549,
     548,   381,     0,   719,   720,   532,   401,   469,   513,   400,
     433,   383,   384,   386,   385,     0,   563,   426,   500,   357,
     564,     0,     0,     0,     0,     0,   565,   335,   462,   488,
     402,   566,   567,   568,   573,   571,   572,   569,   570,   574,
     575,   576,   577,   579,   580,   578,   591,     0,   595,   596,
       0,     0,   597,   581,   589,   582,   583,   584,   588,   590,
     585,   586,   587,   309,   310,   311,   312,   313,   314,   609,
     611,   610,   613,   614,   615,   616,   612,   639,   641,   642,
     643,   644,   645,   646,   647,   648,   649,   640,   650,   651,
     652,   653,   654,   655,   656,   657,   662,   667,   685,   686,
     687,   690,   688,   689,   691,   692,   693,   694,   708,   709,
     711,   712,   715,   716,   713,   714,   717,   710,   726,   727,
     473,   508,   530,   670,   671,   538,   539,   520,   521,     0,
       0,     0,   525,   718,   562,   474,   529,   526,     0,     0,
     592,   593,   594,   524,   522,   523,   527,   528
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,    95,
     235,   240,   249,   707,  -293,  -293,   708,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     1,    17,    18,    19,    32,   288,    20,    33,   540,
      21,    34,   556,    22,    35,   571,    23,    36,   589,   606,
     607,   608,   609,   610,   611,    24,    37,   612,   289,   290,
     291,   292,   293,   294,   295,   296,   297,   298,   299,   300,
     301,   302,   303,   304,   305,   306,   307,   308,   309,   310,
     311,   312,   313,   314,   315,   316,   317,   318,   319,   320,
     321,   322,   323,   324,   325,   326,   327,   328,   329,   330,
     331,   332,   333,   334,   335,   336,   337,   338,   339,   340,
     341,   342,   343,   344,   345,   346,   347,   348,   349,   350,
     351,   352,   353,   354,   355,   356,   357,   358,   359,   360,
     361,   362,   363,   364,   365,   366,   367,   368,   369,   370,
     371,   372,   373,   374,   375,   376,   377,   378,   379,   380,
     381,   382,   383,   384,   385,   386,   387,   388,   389,   390,
     391,   392,   393,   394,   395,   396,   397,   398,   399,   400,
     401,   402,   403,   404,   405,   406,   407,   408,   409,   410,
     411,   412,   413,   414,   415,   416,   417,   418,   419,   420,
     421,   422,   423,   424,   425,   426,   427,   428,   429,   430,
     431,   432,   433,   434,   435,   436,   437,   438,   439,   440,
     441,   442,   443,   444,   445,   446,   447,   448,   449,   450,
     451,   452,   453,   454,   455,   456,   457,   458,   459,   460,
     461,   462,   463,   464,   465,   466,   467,   468,   469,   470,
     471,   472,   473,   474,   475,   476,   477,   478,   479,   480,
     481,   482,   483,   484,   485,   486,   487,   488,   489,   490,
     491,   492,   493,   494,   495,   496,   497,   498,   499,   500,
     501,   502,   503,   504,   505,   506,   507,   508,   509,   510,
     511,   512,   513,   514,   515,   516,   517,   518,   519,   520,
     521,   522,   523,   524,   541,   542,   543,   544,   545,   546,
     547,   548,   557,   558,   559,   560,   561,   562,   563,   590,
     591,   592,   593,   594,   595,   596,   597,   598,   599,   572,
     573,   574,   575,   576,   577,   578,    25,    38,   627,   628,
     629,   630,   631,   632,   633,   634,   635,    26,    39,   655,
     656,   657,   658,   659,   660,   661,   662,   663,   664,   665,
     666,   667,   668,   669,   670,   671,   672,   673,   674,    27,
      40,   676,   677,    28,    41,   679,   680,   525,   526,   527,
     528,    29,    42,   691,   692,   693,   694,   695,   696,   697,
     698,   699,   700,   701,    30,    43,   712,   713,   714,   715,
     716,   717,   718,   719,   720,   721,   722,   529,   530,   531,
      31,    44,   725,   726,   727
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,   723,   724,   675,   678,    79,    80,
      81,   728,   729,   730,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   731,   732,   733,   734,   735,
     736,   737,   579,   738,   123,   124,   125,   126,   127,   739,
     128,   129,   130,   579,   740,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,   741,   613,   742,   157,   743,   744,   158,   159,   160,
     161,   162,   163,   164,   745,   165,   166,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   681,
     682,   683,   684,   685,   686,   687,   688,   689,   690,   549,
     746,   747,   748,   749,   750,   751,   752,   550,   551,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,   189,
     190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,   202,   203,   204,   205,   206,   207,   208,   209,
     210,   211,   212,   213,   214,   215,   216,   217,   218,   219,
     220,   221,   222,   753,   223,   754,   224,   225,   226,   227,
     228,   229,   230,   231,   232,   233,   234,   235,   236,   237,
     238,   239,   240,   241,   242,   702,   703,   704,   705,   706,
     707,   708,   709,   710,   755,   756,   243,   244,   245,   246,
     247,   248,   249,   250,   251,   252,   552,     2,   553,   757,
     758,   554,   614,   759,   760,   761,   253,   615,     3,     4,
     762,   763,   764,   765,   254,   255,   616,   256,   257,   766,
     258,   259,   260,   261,   262,   767,   768,   263,   264,   265,
     266,   267,   268,   269,   270,   769,   770,   771,   271,   532,
     772,   533,   534,   711,   773,     5,   774,   272,   273,   274,
     275,     6,   775,   564,   276,   277,   278,   279,   280,   281,
     282,   283,   284,   285,   286,   776,   287,   581,   582,   583,
     584,   777,   778,   779,   780,   781,   580,   586,   581,   582,
     583,   584,   585,   782,   783,   784,   785,   786,   586,   565,
     566,   787,   788,   789,   790,     7,   791,   792,   600,   601,
     602,   603,   604,   793,   535,   794,   795,   796,   797,   798,
     799,   605,   800,     8,   801,   802,   567,   803,   804,   805,
     587,   588,   619,   620,   621,   622,   623,   624,   625,   626,
     806,   807,   808,   809,   810,   811,   812,   536,   813,   814,
     537,   815,   816,   817,   818,   819,   820,   821,   822,   538,
     823,   824,   825,   826,   827,   828,   829,   830,   831,   832,
     555,   833,   834,   835,   836,   837,   838,   839,   840,     9,
     841,   636,   637,   638,   639,   640,   641,   642,   643,   644,
     645,   646,   647,   648,   649,   650,   651,   652,   653,   654,
     842,   843,   844,   845,   846,   847,   848,   849,   850,   568,
     569,   851,   852,   853,   854,   855,   856,   857,   858,   859,
     860,   861,   862,   863,   864,   865,   866,   867,   868,   869,
     870,   871,   872,   873,   874,   875,   876,   877,   878,   879,
     880,   881,   882,    10,   883,   884,   885,   886,   887,   888,
     889,   890,   891,   892,   570,   893,   894,   895,   896,   897,
     898,   899,   900,   901,   902,    11,   903,   904,   905,   906,
     907,   908,   909,   910,   911,   912,   913,   914,   915,   916,
     917,   918,   919,   920,   921,   922,    12,   923,   924,   925,
     926,   927,   928,   929,   930,   931,   932,   933,    13,   934,
     935,   936,   937,   938,   939,   940,   941,   942,   943,   944,
     945,   539,   946,   947,   948,   949,   950,   951,   952,   953,
     954,   955,   956,   957,   958,    14,   959,   960,   961,    15,
     962,   963,   964,   965,   966,    16,   967,   968,   969,   970,
     971,   972,   973,   974,   975,   976,   977,   978,   979,   980,
     981,   982,   983,   984,   985,   986,   987,   988,   989,   990,
     991,   992,   993,   994,   995,   996,   997,   998,   999,  1000,
    1001,  1002,  1003,  1004,  1005,  1006,  1007,  1008,  1009,  1010,
    1011,  1012,  1013,  1014,  1015,  1016,  1017,  1018,  1019,  1020,
    1021,  1022,  1023,  1024,  1025,  1026,  1027,  1028,  1029,  1030,
    1031,  1032,  1033,  1034,  1035,  1036,  1037,  1038,  1039,  1040,
    1041,  1042,  1043,  1044,  1045,  1046,  1047,  1048,  1049,  1050,
    1051,  1052,  1053,  1054,  1055,  1056,  1057,  1058,  1059,  1060,
    1061,  1062,  1063,  1064,  1065,  1066,  1067,  1068,  1069,  1070,
    1071,  1072,  1073,  1074,  1075,  1076,  1077,  1078,  1079,  1080,
    1081,  1082,  1083,  1084,  1085,  1086,  1087,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   617,   618
};

static const yytype_int16 yycheck[] =
{
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,   319,   320,   117,   329,    51,    52,
      53,    10,    10,    10,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    10,    10,    10,    10,    10,
      10,    10,    47,    10,   107,   108,   109,   110,   111,    10,
     113,   114,   115,    47,    10,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,    10,    37,    10,   147,    10,    10,   150,   151,   152,
     153,   154,   155,   156,    10,   158,   159,   160,   161,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,   259,
     260,   261,   262,   263,   264,   265,   266,   267,   268,    47,
      10,    10,    10,    10,    10,    10,    10,    55,    56,   192,
     193,   194,   195,   196,   197,   198,   199,   200,   201,   202,
     203,   204,   205,   206,   207,   208,   209,   210,   211,   212,
     213,   214,   215,   216,   217,   218,   219,   220,   221,   222,
     223,   224,   225,   226,   227,   228,   229,   230,   231,   232,
     233,   234,   235,    10,   237,    10,   239,   240,   241,   242,
     243,   244,   245,   246,   247,   248,   249,   250,   251,   252,
     253,   254,   255,   256,   257,   280,   281,   282,   283,   284,
     285,   286,   287,   288,    10,    10,   269,   270,   271,   272,
     273,   274,   275,   276,   277,   278,   144,     0,   146,    10,
      10,   149,    37,    10,    10,    10,   289,    37,    11,    12,
      10,    10,    10,    10,   297,   298,    37,   300,   301,    10,
     303,   304,   305,   306,   307,    10,    10,   310,   311,   312,
     313,   314,   315,   316,   317,    10,    10,    10,   321,    47,
      10,    49,    50,   348,    10,    48,    10,   330,   331,   332,
     333,    54,    10,    47,   337,   338,   339,   340,   341,   342,
     343,   344,   345,   346,   347,    10,   349,   292,   293,   294,
     295,    10,    10,    10,    10,    10,   290,   302,   292,   293,
     294,   295,   296,    10,    10,    10,    10,    10,   302,    83,
      84,    10,    10,    10,    10,    98,    10,    10,   323,   324,
     325,   326,   327,    10,   112,    10,    10,    10,    10,    10,
      10,   336,    10,   116,    10,    10,   110,    10,    10,    10,
     334,   335,    99,   100,   101,   102,   103,   104,   105,   106,
      10,    10,    10,    10,    10,    10,    10,   145,    10,    10,
     148,    10,    10,    10,    10,    10,    10,    10,    10,   157,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
     308,    10,    10,    10,    10,    10,    10,    10,    10,   172,
      10,   173,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,   189,   190,   191,
      10,    10,    10,    10,    10,    10,    10,    10,    10,   193,
     194,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,   236,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,   238,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,   258,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,    10,   279,    10,    10,    10,
      10,    10,    10,    10,    10,    10,    10,    10,   291,    10,
      10,    10,    10,    10,    10,    10,    10,    10,    10,    10,
      10,   309,    10,    10,    10,    10,    10,    10,    10,    10,
      10,    10,    10,    10,    10,   318,    10,    10,    10,   322,
      10,    10,    10,    10,    10,   328,    10,    10,    10,    10,
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
      10,    10,    10,    10,    10,    10,    10,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    37,    37
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_int16 yystos[] =
{
       0,   351,     0,    11,    12,    48,    54,    98,   116,   172,
     236,   258,   279,   291,   318,   322,   328,   352,   353,   354,
     357,   360,   363,   366,   375,   646,   657,   679,   683,   691,
     704,   720,   355,   358,   361,   364,   367,   376,   647,   658,
     680,   684,   692,   705,   721,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    51,
      52,    53,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,   107,   108,   109,   110,   111,   113,   114,
     115,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   147,   150,   151,
     152,   153,   154,   155,   156,   158,   159,   160,   161,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,   192,
     193,   194,   195,   196,   197,   198,   199,   200,   201,   202,
     203,   204,   205,   206,   207,   208,   209,   210,   211,   212,
     213,   214,   215,   216,   217,   218,   219,   220,   221,   222,
     223,   224,   225,   226,   227,   228,   229,   230,   231,   232,
     233,   234,   235,   237,   239,   240,   241,   242,   243,   244,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   269,   270,   271,   272,   273,   274,   275,
     276,   277,   278,   289,   297,   298,   300,   301,   303,   304,
     305,   306,   307,   310,   311,   312,   313,   314,   315,   316,
     317,   321,   330,   331,   332,   333,   337,   338,   339,   340,
     341,   342,   343,   344,   345,   346,   347,   349,   356,   378,
     379,   380,   381,   382,   383,   384,   385,   386,   387,   388,
     389,   390,   391,   392,   393,   394,   395,   396,   397,   398,
     399,   400,   401,   402,   403,   404,   405,   406,   407,   408,
     409,   410,   411,   412,   413,   414,   415,   416,   417,   418,
     419,   420,   421,   422,   423,   424,   425,   426,   427,   428,
     429,   430,   431,   432,   433,   434,   435,   436,   437,   438,
     439,   440,   441,   442,   443,   444,   445,   446,   447,   448,
     449,   450,   451,   452,   453,   454,   455,   456,   457,   458,
     459,   460,   461,   462,   463,   464,   465,   466,   467,   468,
     469,   470,   471,   472,   473,   474,   475,   476,   477,   478,
     479,   480,   481,   482,   483,   484,   485,   486,   487,   488,
     489,   490,   491,   492,   493,   494,   495,   496,   497,   498,
     499,   500,   501,   502,   503,   504,   505,   506,   507,   508,
     509,   510,   511,   512,   513,   514,   515,   516,   517,   518,
     519,   520,   521,   522,   523,   524,   525,   526,   527,   528,
     529,   530,   531,   532,   533,   534,   535,   536,   537,   538,
     539,   540,   541,   542,   543,   544,   545,   546,   547,   548,
     549,   550,   551,   552,   553,   554,   555,   556,   557,   558,
     559,   560,   561,   562,   563,   564,   565,   566,   567,   568,
     569,   570,   571,   572,   573,   574,   575,   576,   577,   578,
     579,   580,   581,   582,   583,   584,   585,   586,   587,   588,
     589,   590,   591,   592,   593,   594,   595,   596,   597,   598,
     599,   600,   601,   602,   603,   604,   605,   606,   607,   608,
     609,   610,   611,   612,   613,   687,   688,   689,   690,   717,
     718,   719,    47,    49,    50,   112,   145,   148,   157,   309,
     359,   614,   615,   616,   617,   618,   619,   620,   621,    47,
      55,    56,   144,   146,   149,   308,   362,   622,   623,   624,
     625,   626,   627,   628,    47,    83,    84,   110,   193,   194,
     238,   365,   639,   640,   641,   642,   643,   644,   645,    47,
     290,   292,   293,   294,   295,   296,   302,   334,   335,   368,
     629,   630,   631,   632,   633,   634,   635,   636,   637,   638,
     323,   324,   325,   326,   327,   336,   369,   370,   371,   372,
     373,   374,   377,   629,   630,   631,   632,   633,   636,    99,
     100,   101,   102,   103,   104,   105,   106,   648,   649,   650,
     651,   652,   653,   654,   655,   656,   173,   174,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,   185,   186,
     187,   188,   189,   190,   191,   659,   660,   661,   662,   663,
     664,   665,   666,   667,   668,   669,   670,   671,   672,   673,
     674,   675,   676,   677,   678,   117,   681,   682,   329,   685,
     686,   259,   260,   261,   262,   263,   264,   265,   266,   267,
     268,   693,   694,   695,   696,   697,   698,   699,   700,   701,
     702,   703,   280,   281,   282,   283,   284,   285,   286,   287,
     288,   348,   706,   707,   708,   709,   710,   711,   712,   713,
     714,   715,   716,   319,   320,   722,   723,   724,    10,    10,
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
      10,    10,    10,    10,    10,    10,    10,    10
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int16 yyr1[] =
{
       0,   350,   351,   351,   352,   352,   352,   352,   352,   352,
     352,   352,   352,   352,   352,   352,   352,   352,   353,   354,
     355,   355,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   356,   356,   356,   356,   356,
     356,   356,   356,   356,   356,   357,   358,   358,   359,   359,
     359,   359,   359,   359,   359,   359,   360,   361,   361,   362,
     362,   362,   362,   362,   362,   362,   363,   364,   364,   365,
     365,   365,   365,   365,   365,   365,   366,   367,   367,   368,
     368,   368,   368,   368,   368,   368,   368,   368,   368,   369,
     370,   371,   372,   373,   374,   375,   376,   376,   377,   377,
     377,   377,   377,   377,   377,   377,   377,   377,   377,   377,
     378,   379,   380,   381,   382,   383,   384,   385,   386,   387,
     388,   389,   390,   391,   392,   393,   394,   395,   396,   397,
     398,   399,   400,   401,   402,   403,   404,   405,   406,   407,
     408,   409,   410,   411,   412,   413,   414,   415,   416,   417,
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
     647,   648,   648,   648,   648,   648,   648,   648,   648,   649,
     650,   651,   652,   653,   654,   655,   656,   657,   658,   658,
     659,   659,   659,   659,   659,   659,   659,   659,   659,   659,
     659,   659,   659,   659,   659,   659,   659,   659,   659,   660,
     661,   662,   663,   664,   665,   666,   667,   668,   669,   670,
     671,   672,   673,   674,   675,   676,   677,   678,   679,   680,
     680,   681,   682,   683,   684,   684,   685,   686,   687,   688,
     689,   690,   691,   692,   692,   693,   693,   693,   693,   693,
     693,   693,   693,   693,   693,   694,   695,   696,   697,   698,
     699,   700,   701,   702,   703,   704,   705,   705,   706,   706,
     706,   706,   706,   706,   706,   706,   706,   706,   707,   708,
     709,   710,   711,   712,   713,   714,   715,   716,   717,   718,
     719,   720,   721,   721,   722,   722,   723,   724
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
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     0,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     0,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     0,     1,
       1,     1,     1,     1,     1,     1,     1,     2,     0,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     2,     2,     2,     2,     1,     2,     0,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
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
       2,     2,     2,     3,     3,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     3,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       3,     3,     4,     4,     4,     3,     3,     4,     4,     3,
       3,     2,     2,     2,     2,     2,     2,     2,     3,     3,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     3,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     3,     3,     3,     2,     2,     2,     1,     2,
       0,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     2,     2,     2,     2,     2,     2,     1,     2,     0,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     1,     2,
       0,     1,     2,     1,     2,     0,     1,     2,     2,     2,
       3,     3,     1,     2,     0,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     1,     2,     0,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     3,     2,
       2,     1,     2,     0,     1,     1,     2,     2
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
#line 216 "util/configparser.y"
        {
		OUTYY(("\nP(force-toplevel)\n"));
		cfg_parser->started_toplevel = 0;
	}
#line 2923 "util/configparser.c"
    break;

  case 19: /* serverstart: VAR_SERVER  */
#line 223 "util/configparser.y"
        {
		OUTYY(("\nP(server:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 2932 "util/configparser.c"
    break;

  case 265: /* stubstart: VAR_STUB_ZONE  */
#line 341 "util/configparser.y"
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
#line 2949 "util/configparser.c"
    break;

  case 276: /* forwardstart: VAR_FORWARD_ZONE  */
#line 360 "util/configparser.y"
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
#line 2966 "util/configparser.c"
    break;

  case 286: /* viewstart: VAR_VIEW  */
#line 379 "util/configparser.y"
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
#line 2985 "util/configparser.c"
    break;

  case 296: /* authstart: VAR_AUTH_ZONE  */
#line 400 "util/configparser.y"
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
#line 3009 "util/configparser.c"
    break;

  case 309: /* rpz_tag: VAR_TAGS STRING_ARG  */
#line 428 "util/configparser.y"
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
#line 3030 "util/configparser.c"
    break;

  case 310: /* rpz_action_override: VAR_RPZ_ACTION_OVERRIDE STRING_ARG  */
#line 447 "util/configparser.y"
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
#line 3049 "util/configparser.c"
    break;

  case 311: /* rpz_cname_override: VAR_RPZ_CNAME_OVERRIDE STRING_ARG  */
#line 464 "util/configparser.y"
        {
		OUTYY(("P(rpz_cname_override:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->auths->rpz_cname);
		cfg_parser->cfg->auths->rpz_cname = (yyvsp[0].str);
	}
#line 3059 "util/configparser.c"
    break;

  case 312: /* rpz_log: VAR_RPZ_LOG STRING_ARG  */
#line 472 "util/configparser.y"
        {
		OUTYY(("P(rpz_log:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->rpz_log = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3071 "util/configparser.c"
    break;

  case 313: /* rpz_log_name: VAR_RPZ_LOG_NAME STRING_ARG  */
#line 482 "util/configparser.y"
        {
		OUTYY(("P(rpz_log_name:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->auths->rpz_log_name);
		cfg_parser->cfg->auths->rpz_log_name = (yyvsp[0].str);
	}
#line 3081 "util/configparser.c"
    break;

  case 314: /* rpz_signal_nxdomain_ra: VAR_RPZ_SIGNAL_NXDOMAIN_RA STRING_ARG  */
#line 489 "util/configparser.y"
        {
		OUTYY(("P(rpz_signal_nxdomain_ra:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->rpz_signal_nxdomain_ra = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3093 "util/configparser.c"
    break;

  case 315: /* rpzstart: VAR_RPZ  */
#line 499 "util/configparser.y"
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
#line 3115 "util/configparser.c"
    break;

  case 330: /* server_num_threads: VAR_NUM_THREADS STRING_ARG  */
#line 524 "util/configparser.y"
        {
		OUTYY(("P(server_num_threads:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->num_threads = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3127 "util/configparser.c"
    break;

  case 331: /* server_verbosity: VAR_VERBOSITY STRING_ARG  */
#line 533 "util/configparser.y"
        {
		OUTYY(("P(server_verbosity:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->verbosity = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3139 "util/configparser.c"
    break;

  case 332: /* server_statistics_interval: VAR_STATISTICS_INTERVAL STRING_ARG  */
#line 542 "util/configparser.y"
        {
		OUTYY(("P(server_statistics_interval:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "") == 0 || strcmp((yyvsp[0].str), "0") == 0)
			cfg_parser->cfg->stat_interval = 0;
		else if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->stat_interval = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3153 "util/configparser.c"
    break;

  case 333: /* server_statistics_cumulative: VAR_STATISTICS_CUMULATIVE STRING_ARG  */
#line 553 "util/configparser.y"
        {
		OUTYY(("P(server_statistics_cumulative:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_cumulative = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3165 "util/configparser.c"
    break;

  case 334: /* server_extended_statistics: VAR_EXTENDED_STATISTICS STRING_ARG  */
#line 562 "util/configparser.y"
        {
		OUTYY(("P(server_extended_statistics:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_extended = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3177 "util/configparser.c"
    break;

  case 335: /* server_statistics_inhibit_zero: VAR_STATISTICS_INHIBIT_ZERO STRING_ARG  */
#line 571 "util/configparser.y"
        {
		OUTYY(("P(server_statistics_inhibit_zero:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stat_inhibit_zero = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3189 "util/configparser.c"
    break;

  case 336: /* server_shm_enable: VAR_SHM_ENABLE STRING_ARG  */
#line 580 "util/configparser.y"
        {
		OUTYY(("P(server_shm_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->shm_enable = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3201 "util/configparser.c"
    break;

  case 337: /* server_shm_key: VAR_SHM_KEY STRING_ARG  */
#line 589 "util/configparser.y"
        {
		OUTYY(("P(server_shm_key:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "") == 0 || strcmp((yyvsp[0].str), "0") == 0)
			cfg_parser->cfg->shm_key = 0;
		else if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->shm_key = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3215 "util/configparser.c"
    break;

  case 338: /* server_port: VAR_PORT STRING_ARG  */
#line 600 "util/configparser.y"
        {
		OUTYY(("P(server_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3227 "util/configparser.c"
    break;

  case 339: /* server_send_client_subnet: VAR_SEND_CLIENT_SUBNET STRING_ARG  */
#line 609 "util/configparser.y"
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
#line 3242 "util/configparser.c"
    break;

  case 340: /* server_client_subnet_zone: VAR_CLIENT_SUBNET_ZONE STRING_ARG  */
#line 621 "util/configparser.y"
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
#line 3258 "util/configparser.c"
    break;

  case 341: /* server_client_subnet_always_forward: VAR_CLIENT_SUBNET_ALWAYS_FORWARD STRING_ARG  */
#line 635 "util/configparser.y"
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
#line 3276 "util/configparser.c"
    break;

  case 342: /* server_client_subnet_opcode: VAR_CLIENT_SUBNET_OPCODE STRING_ARG  */
#line 650 "util/configparser.y"
        {
	#ifdef CLIENT_SUBNET
		OUTYY(("P(client_subnet_opcode:%s)\n", (yyvsp[0].str)));
		OUTYY(("P(Deprecated option, ignoring)\n"));
	#else
		OUTYY(("P(Compiled without edns subnet option, ignoring)\n"));
	#endif
		free((yyvsp[0].str));
	}
#line 3290 "util/configparser.c"
    break;

  case 343: /* server_max_client_subnet_ipv4: VAR_MAX_CLIENT_SUBNET_IPV4 STRING_ARG  */
#line 661 "util/configparser.y"
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
#line 3310 "util/configparser.c"
    break;

  case 344: /* server_max_client_subnet_ipv6: VAR_MAX_CLIENT_SUBNET_IPV6 STRING_ARG  */
#line 678 "util/configparser.y"
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
#line 3330 "util/configparser.c"
    break;

  case 345: /* server_min_client_subnet_ipv4: VAR_MIN_CLIENT_SUBNET_IPV4 STRING_ARG  */
#line 695 "util/configparser.y"
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
#line 3350 "util/configparser.c"
    break;

  case 346: /* server_min_client_subnet_ipv6: VAR_MIN_CLIENT_SUBNET_IPV6 STRING_ARG  */
#line 712 "util/configparser.y"
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
#line 3370 "util/configparser.c"
    break;

  case 347: /* server_max_ecs_tree_size_ipv4: VAR_MAX_ECS_TREE_SIZE_IPV4 STRING_ARG  */
#line 729 "util/configparser.y"
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
#line 3388 "util/configparser.c"
    break;

  case 348: /* server_max_ecs_tree_size_ipv6: VAR_MAX_ECS_TREE_SIZE_IPV6 STRING_ARG  */
#line 744 "util/configparser.y"
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
#line 3406 "util/configparser.c"
    break;

  case 349: /* server_interface: VAR_INTERFACE STRING_ARG  */
#line 759 "util/configparser.y"
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
#line 3422 "util/configparser.c"
    break;

  case 350: /* server_outgoing_interface: VAR_OUTGOING_INTERFACE STRING_ARG  */
#line 772 "util/configparser.y"
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
#line 3440 "util/configparser.c"
    break;

  case 351: /* server_outgoing_range: VAR_OUTGOING_RANGE STRING_ARG  */
#line 787 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_range:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_num_ports = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3452 "util/configparser.c"
    break;

  case 352: /* server_outgoing_port_permit: VAR_OUTGOING_PORT_PERMIT STRING_ARG  */
#line 796 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_port_permit:%s)\n", (yyvsp[0].str)));
		if(!cfg_mark_ports((yyvsp[0].str), 1,
			cfg_parser->cfg->outgoing_avail_ports, 65536))
			yyerror("port number or range (\"low-high\") expected");
		free((yyvsp[0].str));
	}
#line 3464 "util/configparser.c"
    break;

  case 353: /* server_outgoing_port_avoid: VAR_OUTGOING_PORT_AVOID STRING_ARG  */
#line 805 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_port_avoid:%s)\n", (yyvsp[0].str)));
		if(!cfg_mark_ports((yyvsp[0].str), 0,
			cfg_parser->cfg->outgoing_avail_ports, 65536))
			yyerror("port number or range (\"low-high\") expected");
		free((yyvsp[0].str));
	}
#line 3476 "util/configparser.c"
    break;

  case 354: /* server_outgoing_num_tcp: VAR_OUTGOING_NUM_TCP STRING_ARG  */
#line 814 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_num_tcp:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_num_tcp = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3488 "util/configparser.c"
    break;

  case 355: /* server_incoming_num_tcp: VAR_INCOMING_NUM_TCP STRING_ARG  */
#line 823 "util/configparser.y"
        {
		OUTYY(("P(server_incoming_num_tcp:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->incoming_num_tcp = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3500 "util/configparser.c"
    break;

  case 356: /* server_interface_automatic: VAR_INTERFACE_AUTOMATIC STRING_ARG  */
#line 832 "util/configparser.y"
        {
		OUTYY(("P(server_interface_automatic:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->if_automatic = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3512 "util/configparser.c"
    break;

  case 357: /* server_interface_automatic_ports: VAR_INTERFACE_AUTOMATIC_PORTS STRING_ARG  */
#line 841 "util/configparser.y"
        {
		OUTYY(("P(server_interface_automatic_ports:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->if_automatic_ports);
		cfg_parser->cfg->if_automatic_ports = (yyvsp[0].str);
	}
#line 3522 "util/configparser.c"
    break;

  case 358: /* server_do_ip4: VAR_DO_IP4 STRING_ARG  */
#line 848 "util/configparser.y"
        {
		OUTYY(("P(server_do_ip4:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_ip4 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3534 "util/configparser.c"
    break;

  case 359: /* server_do_ip6: VAR_DO_IP6 STRING_ARG  */
#line 857 "util/configparser.y"
        {
		OUTYY(("P(server_do_ip6:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_ip6 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3546 "util/configparser.c"
    break;

  case 360: /* server_do_nat64: VAR_DO_NAT64 STRING_ARG  */
#line 866 "util/configparser.y"
        {
		OUTYY(("P(server_do_nat64:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_nat64 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3558 "util/configparser.c"
    break;

  case 361: /* server_do_udp: VAR_DO_UDP STRING_ARG  */
#line 875 "util/configparser.y"
        {
		OUTYY(("P(server_do_udp:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_udp = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3570 "util/configparser.c"
    break;

  case 362: /* server_do_tcp: VAR_DO_TCP STRING_ARG  */
#line 884 "util/configparser.y"
        {
		OUTYY(("P(server_do_tcp:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_tcp = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3582 "util/configparser.c"
    break;

  case 363: /* server_prefer_ip4: VAR_PREFER_IP4 STRING_ARG  */
#line 893 "util/configparser.y"
        {
		OUTYY(("P(server_prefer_ip4:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefer_ip4 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3594 "util/configparser.c"
    break;

  case 364: /* server_prefer_ip6: VAR_PREFER_IP6 STRING_ARG  */
#line 902 "util/configparser.y"
        {
		OUTYY(("P(server_prefer_ip6:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefer_ip6 = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3606 "util/configparser.c"
    break;

  case 365: /* server_tcp_mss: VAR_TCP_MSS STRING_ARG  */
#line 911 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_mss:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
				yyerror("number expected");
		else cfg_parser->cfg->tcp_mss = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3618 "util/configparser.c"
    break;

  case 366: /* server_outgoing_tcp_mss: VAR_OUTGOING_TCP_MSS STRING_ARG  */
#line 920 "util/configparser.y"
        {
		OUTYY(("P(server_outgoing_tcp_mss:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outgoing_tcp_mss = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3630 "util/configparser.c"
    break;

  case 367: /* server_tcp_idle_timeout: VAR_TCP_IDLE_TIMEOUT STRING_ARG  */
#line 929 "util/configparser.y"
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
#line 3646 "util/configparser.c"
    break;

  case 368: /* server_max_reuse_tcp_queries: VAR_MAX_REUSE_TCP_QUERIES STRING_ARG  */
#line 942 "util/configparser.y"
        {
		OUTYY(("P(server_max_reuse_tcp_queries:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 1)
			cfg_parser->cfg->max_reuse_tcp_queries = 0;
		else cfg_parser->cfg->max_reuse_tcp_queries = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3660 "util/configparser.c"
    break;

  case 369: /* server_tcp_reuse_timeout: VAR_TCP_REUSE_TIMEOUT STRING_ARG  */
#line 953 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_reuse_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 1)
			cfg_parser->cfg->tcp_reuse_timeout = 0;
		else cfg_parser->cfg->tcp_reuse_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3674 "util/configparser.c"
    break;

  case 370: /* server_tcp_auth_query_timeout: VAR_TCP_AUTH_QUERY_TIMEOUT STRING_ARG  */
#line 964 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_auth_query_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 1)
			cfg_parser->cfg->tcp_auth_query_timeout = 0;
		else cfg_parser->cfg->tcp_auth_query_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3688 "util/configparser.c"
    break;

  case 371: /* server_tcp_keepalive: VAR_EDNS_TCP_KEEPALIVE STRING_ARG  */
#line 975 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_keepalive:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_tcp_keepalive = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3700 "util/configparser.c"
    break;

  case 372: /* server_tcp_keepalive_timeout: VAR_EDNS_TCP_KEEPALIVE_TIMEOUT STRING_ARG  */
#line 984 "util/configparser.y"
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
#line 3716 "util/configparser.c"
    break;

  case 373: /* server_sock_queue_timeout: VAR_SOCK_QUEUE_TIMEOUT STRING_ARG  */
#line 997 "util/configparser.y"
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
#line 3732 "util/configparser.c"
    break;

  case 374: /* server_tcp_upstream: VAR_TCP_UPSTREAM STRING_ARG  */
#line 1010 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tcp_upstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3744 "util/configparser.c"
    break;

  case 375: /* server_udp_upstream_without_downstream: VAR_UDP_UPSTREAM_WITHOUT_DOWNSTREAM STRING_ARG  */
#line 1019 "util/configparser.y"
        {
		OUTYY(("P(server_udp_upstream_without_downstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->udp_upstream_without_downstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3756 "util/configparser.c"
    break;

  case 376: /* server_ssl_upstream: VAR_SSL_UPSTREAM STRING_ARG  */
#line 1028 "util/configparser.y"
        {
		OUTYY(("P(server_ssl_upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ssl_upstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3768 "util/configparser.c"
    break;

  case 377: /* server_ssl_service_key: VAR_SSL_SERVICE_KEY STRING_ARG  */
#line 1037 "util/configparser.y"
        {
		OUTYY(("P(server_ssl_service_key:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->ssl_service_key);
		cfg_parser->cfg->ssl_service_key = (yyvsp[0].str);
	}
#line 3778 "util/configparser.c"
    break;

  case 378: /* server_ssl_service_pem: VAR_SSL_SERVICE_PEM STRING_ARG  */
#line 1044 "util/configparser.y"
        {
		OUTYY(("P(server_ssl_service_pem:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->ssl_service_pem);
		cfg_parser->cfg->ssl_service_pem = (yyvsp[0].str);
	}
#line 3788 "util/configparser.c"
    break;

  case 379: /* server_ssl_port: VAR_SSL_PORT STRING_ARG  */
#line 1051 "util/configparser.y"
        {
		OUTYY(("P(server_ssl_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->ssl_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3800 "util/configparser.c"
    break;

  case 380: /* server_tls_cert_bundle: VAR_TLS_CERT_BUNDLE STRING_ARG  */
#line 1060 "util/configparser.y"
        {
		OUTYY(("P(server_tls_cert_bundle:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->tls_cert_bundle);
		cfg_parser->cfg->tls_cert_bundle = (yyvsp[0].str);
	}
#line 3810 "util/configparser.c"
    break;

  case 381: /* server_tls_win_cert: VAR_TLS_WIN_CERT STRING_ARG  */
#line 1067 "util/configparser.y"
        {
		OUTYY(("P(server_tls_win_cert:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tls_win_cert = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3822 "util/configparser.c"
    break;

  case 382: /* server_tls_additional_port: VAR_TLS_ADDITIONAL_PORT STRING_ARG  */
#line 1076 "util/configparser.y"
        {
		OUTYY(("P(server_tls_additional_port:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->tls_additional_port,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3833 "util/configparser.c"
    break;

  case 383: /* server_tls_ciphers: VAR_TLS_CIPHERS STRING_ARG  */
#line 1084 "util/configparser.y"
        {
		OUTYY(("P(server_tls_ciphers:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->tls_ciphers);
		cfg_parser->cfg->tls_ciphers = (yyvsp[0].str);
	}
#line 3843 "util/configparser.c"
    break;

  case 384: /* server_tls_ciphersuites: VAR_TLS_CIPHERSUITES STRING_ARG  */
#line 1091 "util/configparser.y"
        {
		OUTYY(("P(server_tls_ciphersuites:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->tls_ciphersuites);
		cfg_parser->cfg->tls_ciphersuites = (yyvsp[0].str);
	}
#line 3853 "util/configparser.c"
    break;

  case 385: /* server_tls_session_ticket_keys: VAR_TLS_SESSION_TICKET_KEYS STRING_ARG  */
#line 1098 "util/configparser.y"
        {
		OUTYY(("P(server_tls_session_ticket_keys:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append(&cfg_parser->cfg->tls_session_ticket_keys,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 3864 "util/configparser.c"
    break;

  case 386: /* server_tls_use_sni: VAR_TLS_USE_SNI STRING_ARG  */
#line 1106 "util/configparser.y"
        {
		OUTYY(("P(server_tls_use_sni:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->tls_use_sni = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3876 "util/configparser.c"
    break;

  case 387: /* server_https_port: VAR_HTTPS_PORT STRING_ARG  */
#line 1115 "util/configparser.y"
        {
		OUTYY(("P(server_https_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->https_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3888 "util/configparser.c"
    break;

  case 388: /* server_http_endpoint: VAR_HTTP_ENDPOINT STRING_ARG  */
#line 1123 "util/configparser.y"
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
#line 3908 "util/configparser.c"
    break;

  case 389: /* server_http_max_streams: VAR_HTTP_MAX_STREAMS STRING_ARG  */
#line 1139 "util/configparser.y"
        {
		OUTYY(("P(server_http_max_streams:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->http_max_streams = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 3920 "util/configparser.c"
    break;

  case 390: /* server_http_query_buffer_size: VAR_HTTP_QUERY_BUFFER_SIZE STRING_ARG  */
#line 1147 "util/configparser.y"
        {
		OUTYY(("P(server_http_query_buffer_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str),
			&cfg_parser->cfg->http_query_buffer_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 3932 "util/configparser.c"
    break;

  case 391: /* server_http_response_buffer_size: VAR_HTTP_RESPONSE_BUFFER_SIZE STRING_ARG  */
#line 1155 "util/configparser.y"
        {
		OUTYY(("P(server_http_response_buffer_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str),
			&cfg_parser->cfg->http_response_buffer_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 3944 "util/configparser.c"
    break;

  case 392: /* server_http_nodelay: VAR_HTTP_NODELAY STRING_ARG  */
#line 1163 "util/configparser.y"
        {
		OUTYY(("P(server_http_nodelay:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->http_nodelay = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3956 "util/configparser.c"
    break;

  case 393: /* server_http_notls_downstream: VAR_HTTP_NOTLS_DOWNSTREAM STRING_ARG  */
#line 1171 "util/configparser.y"
        {
		OUTYY(("P(server_http_notls_downstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->http_notls_downstream = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3968 "util/configparser.c"
    break;

  case 394: /* server_use_systemd: VAR_USE_SYSTEMD STRING_ARG  */
#line 1179 "util/configparser.y"
        {
		OUTYY(("P(server_use_systemd:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->use_systemd = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3980 "util/configparser.c"
    break;

  case 395: /* server_do_daemonize: VAR_DO_DAEMONIZE STRING_ARG  */
#line 1188 "util/configparser.y"
        {
		OUTYY(("P(server_do_daemonize:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_daemonize = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 3992 "util/configparser.c"
    break;

  case 396: /* server_use_syslog: VAR_USE_SYSLOG STRING_ARG  */
#line 1197 "util/configparser.y"
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
#line 4009 "util/configparser.c"
    break;

  case 397: /* server_log_time_ascii: VAR_LOG_TIME_ASCII STRING_ARG  */
#line 1211 "util/configparser.y"
        {
		OUTYY(("P(server_log_time_ascii:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_time_ascii = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4021 "util/configparser.c"
    break;

  case 398: /* server_log_queries: VAR_LOG_QUERIES STRING_ARG  */
#line 1220 "util/configparser.y"
        {
		OUTYY(("P(server_log_queries:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_queries = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4033 "util/configparser.c"
    break;

  case 399: /* server_log_replies: VAR_LOG_REPLIES STRING_ARG  */
#line 1229 "util/configparser.y"
        {
		OUTYY(("P(server_log_replies:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_replies = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4045 "util/configparser.c"
    break;

  case 400: /* server_log_tag_queryreply: VAR_LOG_TAG_QUERYREPLY STRING_ARG  */
#line 1238 "util/configparser.y"
        {
		OUTYY(("P(server_log_tag_queryreply:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_tag_queryreply = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4057 "util/configparser.c"
    break;

  case 401: /* server_log_servfail: VAR_LOG_SERVFAIL STRING_ARG  */
#line 1247 "util/configparser.y"
        {
		OUTYY(("P(server_log_servfail:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_servfail = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4069 "util/configparser.c"
    break;

  case 402: /* server_log_destaddr: VAR_LOG_DESTADDR STRING_ARG  */
#line 1256 "util/configparser.y"
        {
		OUTYY(("P(server_log_destaddr:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_destaddr = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4081 "util/configparser.c"
    break;

  case 403: /* server_log_local_actions: VAR_LOG_LOCAL_ACTIONS STRING_ARG  */
#line 1265 "util/configparser.y"
        {
		OUTYY(("P(server_log_local_actions:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->log_local_actions = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4093 "util/configparser.c"
    break;

  case 404: /* server_chroot: VAR_CHROOT STRING_ARG  */
#line 1274 "util/configparser.y"
        {
		OUTYY(("P(server_chroot:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->chrootdir);
		cfg_parser->cfg->chrootdir = (yyvsp[0].str);
	}
#line 4103 "util/configparser.c"
    break;

  case 405: /* server_username: VAR_USERNAME STRING_ARG  */
#line 1281 "util/configparser.y"
        {
		OUTYY(("P(server_username:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->username);
		cfg_parser->cfg->username = (yyvsp[0].str);
	}
#line 4113 "util/configparser.c"
    break;

  case 406: /* server_directory: VAR_DIRECTORY STRING_ARG  */
#line 1288 "util/configparser.y"
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
#line 4142 "util/configparser.c"
    break;

  case 407: /* server_logfile: VAR_LOGFILE STRING_ARG  */
#line 1314 "util/configparser.y"
        {
		OUTYY(("P(server_logfile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->logfile);
		cfg_parser->cfg->logfile = (yyvsp[0].str);
		cfg_parser->cfg->use_syslog = 0;
	}
#line 4153 "util/configparser.c"
    break;

  case 408: /* server_pidfile: VAR_PIDFILE STRING_ARG  */
#line 1322 "util/configparser.y"
        {
		OUTYY(("P(server_pidfile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->pidfile);
		cfg_parser->cfg->pidfile = (yyvsp[0].str);
	}
#line 4163 "util/configparser.c"
    break;

  case 409: /* server_root_hints: VAR_ROOT_HINTS STRING_ARG  */
#line 1329 "util/configparser.y"
        {
		OUTYY(("P(server_root_hints:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->root_hints, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4173 "util/configparser.c"
    break;

  case 410: /* server_dlv_anchor_file: VAR_DLV_ANCHOR_FILE STRING_ARG  */
#line 1336 "util/configparser.y"
        {
		OUTYY(("P(server_dlv_anchor_file:%s)\n", (yyvsp[0].str)));
		log_warn("option dlv-anchor-file ignored: DLV is decommissioned");
		free((yyvsp[0].str));
	}
#line 4183 "util/configparser.c"
    break;

  case 411: /* server_dlv_anchor: VAR_DLV_ANCHOR STRING_ARG  */
#line 1343 "util/configparser.y"
        {
		OUTYY(("P(server_dlv_anchor:%s)\n", (yyvsp[0].str)));
		log_warn("option dlv-anchor ignored: DLV is decommissioned");
		free((yyvsp[0].str));
	}
#line 4193 "util/configparser.c"
    break;

  case 412: /* server_auto_trust_anchor_file: VAR_AUTO_TRUST_ANCHOR_FILE STRING_ARG  */
#line 1350 "util/configparser.y"
        {
		OUTYY(("P(server_auto_trust_anchor_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			auto_trust_anchor_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4204 "util/configparser.c"
    break;

  case 413: /* server_trust_anchor_file: VAR_TRUST_ANCHOR_FILE STRING_ARG  */
#line 1358 "util/configparser.y"
        {
		OUTYY(("P(server_trust_anchor_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			trust_anchor_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4215 "util/configparser.c"
    break;

  case 414: /* server_trusted_keys_file: VAR_TRUSTED_KEYS_FILE STRING_ARG  */
#line 1366 "util/configparser.y"
        {
		OUTYY(("P(server_trusted_keys_file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->
			trusted_keys_file_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4226 "util/configparser.c"
    break;

  case 415: /* server_trust_anchor: VAR_TRUST_ANCHOR STRING_ARG  */
#line 1374 "util/configparser.y"
        {
		OUTYY(("P(server_trust_anchor:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->trust_anchor_list, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4236 "util/configparser.c"
    break;

  case 416: /* server_trust_anchor_signaling: VAR_TRUST_ANCHOR_SIGNALING STRING_ARG  */
#line 1381 "util/configparser.y"
        {
		OUTYY(("P(server_trust_anchor_signaling:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->trust_anchor_signaling =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4250 "util/configparser.c"
    break;

  case 417: /* server_root_key_sentinel: VAR_ROOT_KEY_SENTINEL STRING_ARG  */
#line 1392 "util/configparser.y"
        {
		OUTYY(("P(server_root_key_sentinel:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->root_key_sentinel =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4264 "util/configparser.c"
    break;

  case 418: /* server_domain_insecure: VAR_DOMAIN_INSECURE STRING_ARG  */
#line 1403 "util/configparser.y"
        {
		OUTYY(("P(server_domain_insecure:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->domain_insecure, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4274 "util/configparser.c"
    break;

  case 419: /* server_hide_identity: VAR_HIDE_IDENTITY STRING_ARG  */
#line 1410 "util/configparser.y"
        {
		OUTYY(("P(server_hide_identity:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_identity = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4286 "util/configparser.c"
    break;

  case 420: /* server_hide_version: VAR_HIDE_VERSION STRING_ARG  */
#line 1419 "util/configparser.y"
        {
		OUTYY(("P(server_hide_version:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_version = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4298 "util/configparser.c"
    break;

  case 421: /* server_hide_trustanchor: VAR_HIDE_TRUSTANCHOR STRING_ARG  */
#line 1428 "util/configparser.y"
        {
		OUTYY(("P(server_hide_trustanchor:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_trustanchor = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4310 "util/configparser.c"
    break;

  case 422: /* server_hide_http_user_agent: VAR_HIDE_HTTP_USER_AGENT STRING_ARG  */
#line 1437 "util/configparser.y"
        {
		OUTYY(("P(server_hide_user_agent:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->hide_http_user_agent = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4322 "util/configparser.c"
    break;

  case 423: /* server_identity: VAR_IDENTITY STRING_ARG  */
#line 1446 "util/configparser.y"
        {
		OUTYY(("P(server_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->identity);
		cfg_parser->cfg->identity = (yyvsp[0].str);
	}
#line 4332 "util/configparser.c"
    break;

  case 424: /* server_version: VAR_VERSION STRING_ARG  */
#line 1453 "util/configparser.y"
        {
		OUTYY(("P(server_version:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->version);
		cfg_parser->cfg->version = (yyvsp[0].str);
	}
#line 4342 "util/configparser.c"
    break;

  case 425: /* server_http_user_agent: VAR_HTTP_USER_AGENT STRING_ARG  */
#line 1460 "util/configparser.y"
        {
		OUTYY(("P(server_http_user_agent:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->http_user_agent);
		cfg_parser->cfg->http_user_agent = (yyvsp[0].str);
	}
#line 4352 "util/configparser.c"
    break;

  case 426: /* server_nsid: VAR_NSID STRING_ARG  */
#line 1467 "util/configparser.y"
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
#line 4371 "util/configparser.c"
    break;

  case 427: /* server_so_rcvbuf: VAR_SO_RCVBUF STRING_ARG  */
#line 1483 "util/configparser.y"
        {
		OUTYY(("P(server_so_rcvbuf:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->so_rcvbuf))
			yyerror("buffer size expected");
		free((yyvsp[0].str));
	}
#line 4382 "util/configparser.c"
    break;

  case 428: /* server_so_sndbuf: VAR_SO_SNDBUF STRING_ARG  */
#line 1491 "util/configparser.y"
        {
		OUTYY(("P(server_so_sndbuf:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->so_sndbuf))
			yyerror("buffer size expected");
		free((yyvsp[0].str));
	}
#line 4393 "util/configparser.c"
    break;

  case 429: /* server_so_reuseport: VAR_SO_REUSEPORT STRING_ARG  */
#line 1499 "util/configparser.y"
        {
		OUTYY(("P(server_so_reuseport:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->so_reuseport =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4406 "util/configparser.c"
    break;

  case 430: /* server_ip_transparent: VAR_IP_TRANSPARENT STRING_ARG  */
#line 1509 "util/configparser.y"
        {
		OUTYY(("P(server_ip_transparent:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ip_transparent =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4419 "util/configparser.c"
    break;

  case 431: /* server_ip_freebind: VAR_IP_FREEBIND STRING_ARG  */
#line 1519 "util/configparser.y"
        {
		OUTYY(("P(server_ip_freebind:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ip_freebind =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4432 "util/configparser.c"
    break;

  case 432: /* server_ip_dscp: VAR_IP_DSCP STRING_ARG  */
#line 1529 "util/configparser.y"
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
#line 4449 "util/configparser.c"
    break;

  case 433: /* server_stream_wait_size: VAR_STREAM_WAIT_SIZE STRING_ARG  */
#line 1543 "util/configparser.y"
        {
		OUTYY(("P(server_stream_wait_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->stream_wait_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4460 "util/configparser.c"
    break;

  case 434: /* server_edns_buffer_size: VAR_EDNS_BUFFER_SIZE STRING_ARG  */
#line 1551 "util/configparser.y"
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
#line 4476 "util/configparser.c"
    break;

  case 435: /* server_msg_buffer_size: VAR_MSG_BUFFER_SIZE STRING_ARG  */
#line 1564 "util/configparser.y"
        {
		OUTYY(("P(server_msg_buffer_size:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else if (atoi((yyvsp[0].str)) < 4096)
			yyerror("message buffer size too small (use 4096)");
		else cfg_parser->cfg->msg_buffer_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4490 "util/configparser.c"
    break;

  case 436: /* server_msg_cache_size: VAR_MSG_CACHE_SIZE STRING_ARG  */
#line 1575 "util/configparser.y"
        {
		OUTYY(("P(server_msg_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->msg_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4501 "util/configparser.c"
    break;

  case 437: /* server_msg_cache_slabs: VAR_MSG_CACHE_SLABS STRING_ARG  */
#line 1583 "util/configparser.y"
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
#line 4517 "util/configparser.c"
    break;

  case 438: /* server_num_queries_per_thread: VAR_NUM_QUERIES_PER_THREAD STRING_ARG  */
#line 1596 "util/configparser.y"
        {
		OUTYY(("P(server_num_queries_per_thread:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->num_queries_per_thread = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4529 "util/configparser.c"
    break;

  case 439: /* server_jostle_timeout: VAR_JOSTLE_TIMEOUT STRING_ARG  */
#line 1605 "util/configparser.y"
        {
		OUTYY(("P(server_jostle_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->jostle_time = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4541 "util/configparser.c"
    break;

  case 440: /* server_delay_close: VAR_DELAY_CLOSE STRING_ARG  */
#line 1614 "util/configparser.y"
        {
		OUTYY(("P(server_delay_close:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->delay_close = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4553 "util/configparser.c"
    break;

  case 441: /* server_udp_connect: VAR_UDP_CONNECT STRING_ARG  */
#line 1623 "util/configparser.y"
        {
		OUTYY(("P(server_udp_connect:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->udp_connect = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4565 "util/configparser.c"
    break;

  case 442: /* server_unblock_lan_zones: VAR_UNBLOCK_LAN_ZONES STRING_ARG  */
#line 1632 "util/configparser.y"
        {
		OUTYY(("P(server_unblock_lan_zones:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->unblock_lan_zones =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4578 "util/configparser.c"
    break;

  case 443: /* server_insecure_lan_zones: VAR_INSECURE_LAN_ZONES STRING_ARG  */
#line 1642 "util/configparser.y"
        {
		OUTYY(("P(server_insecure_lan_zones:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->insecure_lan_zones =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4591 "util/configparser.c"
    break;

  case 444: /* server_rrset_cache_size: VAR_RRSET_CACHE_SIZE STRING_ARG  */
#line 1652 "util/configparser.y"
        {
		OUTYY(("P(server_rrset_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->rrset_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 4602 "util/configparser.c"
    break;

  case 445: /* server_rrset_cache_slabs: VAR_RRSET_CACHE_SLABS STRING_ARG  */
#line 1660 "util/configparser.y"
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
#line 4618 "util/configparser.c"
    break;

  case 446: /* server_infra_host_ttl: VAR_INFRA_HOST_TTL STRING_ARG  */
#line 1673 "util/configparser.y"
        {
		OUTYY(("P(server_infra_host_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->host_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4630 "util/configparser.c"
    break;

  case 447: /* server_infra_lame_ttl: VAR_INFRA_LAME_TTL STRING_ARG  */
#line 1682 "util/configparser.y"
        {
		OUTYY(("P(server_infra_lame_ttl:%s)\n", (yyvsp[0].str)));
		verbose(VERB_DETAIL, "ignored infra-lame-ttl: %s (option "
			"removed, use infra-host-ttl)", (yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4641 "util/configparser.c"
    break;

  case 448: /* server_infra_cache_numhosts: VAR_INFRA_CACHE_NUMHOSTS STRING_ARG  */
#line 1690 "util/configparser.y"
        {
		OUTYY(("P(server_infra_cache_numhosts:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_numhosts = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4653 "util/configparser.c"
    break;

  case 449: /* server_infra_cache_lame_size: VAR_INFRA_CACHE_LAME_SIZE STRING_ARG  */
#line 1699 "util/configparser.y"
        {
		OUTYY(("P(server_infra_cache_lame_size:%s)\n", (yyvsp[0].str)));
		verbose(VERB_DETAIL, "ignored infra-cache-lame-size: %s "
			"(option removed, use infra-cache-numhosts)", (yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4664 "util/configparser.c"
    break;

  case 450: /* server_infra_cache_slabs: VAR_INFRA_CACHE_SLABS STRING_ARG  */
#line 1707 "util/configparser.y"
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
#line 4680 "util/configparser.c"
    break;

  case 451: /* server_infra_cache_min_rtt: VAR_INFRA_CACHE_MIN_RTT STRING_ARG  */
#line 1720 "util/configparser.y"
        {
		OUTYY(("P(server_infra_cache_min_rtt:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_min_rtt = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4692 "util/configparser.c"
    break;

  case 452: /* server_infra_cache_max_rtt: VAR_INFRA_CACHE_MAX_RTT STRING_ARG  */
#line 1729 "util/configparser.y"
        {
		OUTYY(("P(server_infra_cache_max_rtt:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->infra_cache_max_rtt = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4704 "util/configparser.c"
    break;

  case 453: /* server_infra_keep_probing: VAR_INFRA_KEEP_PROBING STRING_ARG  */
#line 1738 "util/configparser.y"
        {
		OUTYY(("P(server_infra_keep_probing:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->infra_keep_probing =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4717 "util/configparser.c"
    break;

  case 454: /* server_target_fetch_policy: VAR_TARGET_FETCH_POLICY STRING_ARG  */
#line 1748 "util/configparser.y"
        {
		OUTYY(("P(server_target_fetch_policy:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->target_fetch_policy);
		cfg_parser->cfg->target_fetch_policy = (yyvsp[0].str);
	}
#line 4727 "util/configparser.c"
    break;

  case 455: /* server_harden_short_bufsize: VAR_HARDEN_SHORT_BUFSIZE STRING_ARG  */
#line 1755 "util/configparser.y"
        {
		OUTYY(("P(server_harden_short_bufsize:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_short_bufsize =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4740 "util/configparser.c"
    break;

  case 456: /* server_harden_large_queries: VAR_HARDEN_LARGE_QUERIES STRING_ARG  */
#line 1765 "util/configparser.y"
        {
		OUTYY(("P(server_harden_large_queries:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_large_queries =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4753 "util/configparser.c"
    break;

  case 457: /* server_harden_glue: VAR_HARDEN_GLUE STRING_ARG  */
#line 1775 "util/configparser.y"
        {
		OUTYY(("P(server_harden_glue:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_glue =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4766 "util/configparser.c"
    break;

  case 458: /* server_harden_dnssec_stripped: VAR_HARDEN_DNSSEC_STRIPPED STRING_ARG  */
#line 1785 "util/configparser.y"
        {
		OUTYY(("P(server_harden_dnssec_stripped:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_dnssec_stripped =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4779 "util/configparser.c"
    break;

  case 459: /* server_harden_below_nxdomain: VAR_HARDEN_BELOW_NXDOMAIN STRING_ARG  */
#line 1795 "util/configparser.y"
        {
		OUTYY(("P(server_harden_below_nxdomain:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_below_nxdomain =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4792 "util/configparser.c"
    break;

  case 460: /* server_harden_referral_path: VAR_HARDEN_REFERRAL_PATH STRING_ARG  */
#line 1805 "util/configparser.y"
        {
		OUTYY(("P(server_harden_referral_path:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_referral_path =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4805 "util/configparser.c"
    break;

  case 461: /* server_harden_algo_downgrade: VAR_HARDEN_ALGO_DOWNGRADE STRING_ARG  */
#line 1815 "util/configparser.y"
        {
		OUTYY(("P(server_harden_algo_downgrade:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_algo_downgrade =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4818 "util/configparser.c"
    break;

  case 462: /* server_harden_unknown_additional: VAR_HARDEN_UNKNOWN_ADDITIONAL STRING_ARG  */
#line 1825 "util/configparser.y"
        {
		OUTYY(("P(server_harden_unknown_additional:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->harden_unknown_additional =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4831 "util/configparser.c"
    break;

  case 463: /* server_use_caps_for_id: VAR_USE_CAPS_FOR_ID STRING_ARG  */
#line 1835 "util/configparser.y"
        {
		OUTYY(("P(server_use_caps_for_id:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->use_caps_bits_for_id =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4844 "util/configparser.c"
    break;

  case 464: /* server_caps_whitelist: VAR_CAPS_WHITELIST STRING_ARG  */
#line 1845 "util/configparser.y"
        {
		OUTYY(("P(server_caps_whitelist:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->caps_whitelist, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4854 "util/configparser.c"
    break;

  case 465: /* server_private_address: VAR_PRIVATE_ADDRESS STRING_ARG  */
#line 1852 "util/configparser.y"
        {
		OUTYY(("P(server_private_address:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->private_address, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4864 "util/configparser.c"
    break;

  case 466: /* server_private_domain: VAR_PRIVATE_DOMAIN STRING_ARG  */
#line 1859 "util/configparser.y"
        {
		OUTYY(("P(server_private_domain:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->private_domain, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4874 "util/configparser.c"
    break;

  case 467: /* server_prefetch: VAR_PREFETCH STRING_ARG  */
#line 1866 "util/configparser.y"
        {
		OUTYY(("P(server_prefetch:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefetch = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4886 "util/configparser.c"
    break;

  case 468: /* server_prefetch_key: VAR_PREFETCH_KEY STRING_ARG  */
#line 1875 "util/configparser.y"
        {
		OUTYY(("P(server_prefetch_key:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->prefetch_key = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4898 "util/configparser.c"
    break;

  case 469: /* server_deny_any: VAR_DENY_ANY STRING_ARG  */
#line 1884 "util/configparser.y"
        {
		OUTYY(("P(server_deny_any:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->deny_any = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4910 "util/configparser.c"
    break;

  case 470: /* server_unwanted_reply_threshold: VAR_UNWANTED_REPLY_THRESHOLD STRING_ARG  */
#line 1893 "util/configparser.y"
        {
		OUTYY(("P(server_unwanted_reply_threshold:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->unwanted_threshold = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 4922 "util/configparser.c"
    break;

  case 471: /* server_do_not_query_address: VAR_DO_NOT_QUERY_ADDRESS STRING_ARG  */
#line 1902 "util/configparser.y"
        {
		OUTYY(("P(server_do_not_query_address:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->donotqueryaddrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 4932 "util/configparser.c"
    break;

  case 472: /* server_do_not_query_localhost: VAR_DO_NOT_QUERY_LOCALHOST STRING_ARG  */
#line 1909 "util/configparser.y"
        {
		OUTYY(("P(server_do_not_query_localhost:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->donotquery_localhost =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 4945 "util/configparser.c"
    break;

  case 473: /* server_access_control: VAR_ACCESS_CONTROL STRING_ARG STRING_ARG  */
#line 1919 "util/configparser.y"
        {
		OUTYY(("P(server_access_control:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_acl_action((yyvsp[0].str));
		if(!cfg_str2list_insert(&cfg_parser->cfg->acls, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding acl");
	}
#line 4956 "util/configparser.c"
    break;

  case 474: /* server_interface_action: VAR_INTERFACE_ACTION STRING_ARG STRING_ARG  */
#line 1927 "util/configparser.y"
        {
		OUTYY(("P(server_interface_action:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_acl_action((yyvsp[0].str));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->interface_actions, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding acl");
	}
#line 4968 "util/configparser.c"
    break;

  case 475: /* server_module_conf: VAR_MODULE_CONF STRING_ARG  */
#line 1936 "util/configparser.y"
        {
		OUTYY(("P(server_module_conf:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->module_conf);
		cfg_parser->cfg->module_conf = (yyvsp[0].str);
	}
#line 4978 "util/configparser.c"
    break;

  case 476: /* server_val_override_date: VAR_VAL_OVERRIDE_DATE STRING_ARG  */
#line 1943 "util/configparser.y"
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
#line 4999 "util/configparser.c"
    break;

  case 477: /* server_val_sig_skew_min: VAR_VAL_SIG_SKEW_MIN STRING_ARG  */
#line 1961 "util/configparser.y"
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
#line 5015 "util/configparser.c"
    break;

  case 478: /* server_val_sig_skew_max: VAR_VAL_SIG_SKEW_MAX STRING_ARG  */
#line 1974 "util/configparser.y"
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
#line 5031 "util/configparser.c"
    break;

  case 479: /* server_val_max_restart: VAR_VAL_MAX_RESTART STRING_ARG  */
#line 1987 "util/configparser.y"
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
#line 5047 "util/configparser.c"
    break;

  case 480: /* server_cache_max_ttl: VAR_CACHE_MAX_TTL STRING_ARG  */
#line 2000 "util/configparser.y"
        {
		OUTYY(("P(server_cache_max_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5059 "util/configparser.c"
    break;

  case 481: /* server_cache_max_negative_ttl: VAR_CACHE_MAX_NEGATIVE_TTL STRING_ARG  */
#line 2009 "util/configparser.y"
        {
		OUTYY(("P(server_cache_max_negative_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_negative_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5071 "util/configparser.c"
    break;

  case 482: /* server_cache_min_ttl: VAR_CACHE_MIN_TTL STRING_ARG  */
#line 2018 "util/configparser.y"
        {
		OUTYY(("P(server_cache_min_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->min_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5083 "util/configparser.c"
    break;

  case 483: /* server_bogus_ttl: VAR_BOGUS_TTL STRING_ARG  */
#line 2027 "util/configparser.y"
        {
		OUTYY(("P(server_bogus_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->bogus_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5095 "util/configparser.c"
    break;

  case 484: /* server_val_clean_additional: VAR_VAL_CLEAN_ADDITIONAL STRING_ARG  */
#line 2036 "util/configparser.y"
        {
		OUTYY(("P(server_val_clean_additional:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->val_clean_additional =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5108 "util/configparser.c"
    break;

  case 485: /* server_val_permissive_mode: VAR_VAL_PERMISSIVE_MODE STRING_ARG  */
#line 2046 "util/configparser.y"
        {
		OUTYY(("P(server_val_permissive_mode:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->val_permissive_mode =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5121 "util/configparser.c"
    break;

  case 486: /* server_aggressive_nsec: VAR_AGGRESSIVE_NSEC STRING_ARG  */
#line 2056 "util/configparser.y"
        {
		OUTYY(("P(server_aggressive_nsec:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else
			cfg_parser->cfg->aggressive_nsec =
				(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5135 "util/configparser.c"
    break;

  case 487: /* server_ignore_cd_flag: VAR_IGNORE_CD_FLAG STRING_ARG  */
#line 2067 "util/configparser.y"
        {
		OUTYY(("P(server_ignore_cd_flag:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ignore_cd = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5147 "util/configparser.c"
    break;

  case 488: /* server_disable_edns_do: VAR_DISABLE_EDNS_DO STRING_ARG  */
#line 2076 "util/configparser.y"
        {
		OUTYY(("P(server_disable_edns_do:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->disable_edns_do = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5159 "util/configparser.c"
    break;

  case 489: /* server_serve_expired: VAR_SERVE_EXPIRED STRING_ARG  */
#line 2085 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->serve_expired = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5171 "util/configparser.c"
    break;

  case 490: /* server_serve_expired_ttl: VAR_SERVE_EXPIRED_TTL STRING_ARG  */
#line 2094 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->serve_expired_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5183 "util/configparser.c"
    break;

  case 491: /* server_serve_expired_ttl_reset: VAR_SERVE_EXPIRED_TTL_RESET STRING_ARG  */
#line 2103 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired_ttl_reset:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->serve_expired_ttl_reset = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5195 "util/configparser.c"
    break;

  case 492: /* server_serve_expired_reply_ttl: VAR_SERVE_EXPIRED_REPLY_TTL STRING_ARG  */
#line 2112 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired_reply_ttl:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->serve_expired_reply_ttl = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5207 "util/configparser.c"
    break;

  case 493: /* server_serve_expired_client_timeout: VAR_SERVE_EXPIRED_CLIENT_TIMEOUT STRING_ARG  */
#line 2121 "util/configparser.y"
        {
		OUTYY(("P(server_serve_expired_client_timeout:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->serve_expired_client_timeout = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5219 "util/configparser.c"
    break;

  case 494: /* server_ede_serve_expired: VAR_EDE_SERVE_EXPIRED STRING_ARG  */
#line 2130 "util/configparser.y"
        {
		OUTYY(("P(server_ede_serve_expired:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ede_serve_expired = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5231 "util/configparser.c"
    break;

  case 495: /* server_serve_original_ttl: VAR_SERVE_ORIGINAL_TTL STRING_ARG  */
#line 2139 "util/configparser.y"
        {
		OUTYY(("P(server_serve_original_ttl:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->serve_original_ttl = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5243 "util/configparser.c"
    break;

  case 496: /* server_fake_dsa: VAR_FAKE_DSA STRING_ARG  */
#line 2148 "util/configparser.y"
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
#line 5259 "util/configparser.c"
    break;

  case 497: /* server_fake_sha1: VAR_FAKE_SHA1 STRING_ARG  */
#line 2161 "util/configparser.y"
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
#line 5275 "util/configparser.c"
    break;

  case 498: /* server_val_log_level: VAR_VAL_LOG_LEVEL STRING_ARG  */
#line 2174 "util/configparser.y"
        {
		OUTYY(("P(server_val_log_level:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->val_log_level = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5287 "util/configparser.c"
    break;

  case 499: /* server_val_nsec3_keysize_iterations: VAR_VAL_NSEC3_KEYSIZE_ITERATIONS STRING_ARG  */
#line 2183 "util/configparser.y"
        {
		OUTYY(("P(server_val_nsec3_keysize_iterations:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->val_nsec3_key_iterations);
		cfg_parser->cfg->val_nsec3_key_iterations = (yyvsp[0].str);
	}
#line 5297 "util/configparser.c"
    break;

  case 500: /* server_zonemd_permissive_mode: VAR_ZONEMD_PERMISSIVE_MODE STRING_ARG  */
#line 2190 "util/configparser.y"
        {
		OUTYY(("P(server_zonemd_permissive_mode:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else	cfg_parser->cfg->zonemd_permissive_mode = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5309 "util/configparser.c"
    break;

  case 501: /* server_add_holddown: VAR_ADD_HOLDDOWN STRING_ARG  */
#line 2199 "util/configparser.y"
        {
		OUTYY(("P(server_add_holddown:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->add_holddown = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5321 "util/configparser.c"
    break;

  case 502: /* server_del_holddown: VAR_DEL_HOLDDOWN STRING_ARG  */
#line 2208 "util/configparser.y"
        {
		OUTYY(("P(server_del_holddown:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->del_holddown = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5333 "util/configparser.c"
    break;

  case 503: /* server_keep_missing: VAR_KEEP_MISSING STRING_ARG  */
#line 2217 "util/configparser.y"
        {
		OUTYY(("P(server_keep_missing:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->keep_missing = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5345 "util/configparser.c"
    break;

  case 504: /* server_permit_small_holddown: VAR_PERMIT_SMALL_HOLDDOWN STRING_ARG  */
#line 2226 "util/configparser.y"
        {
		OUTYY(("P(server_permit_small_holddown:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->permit_small_holddown =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5358 "util/configparser.c"
    break;

  case 505: /* server_key_cache_size: VAR_KEY_CACHE_SIZE STRING_ARG  */
#line 2236 "util/configparser.y"
        {
		OUTYY(("P(server_key_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->key_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 5369 "util/configparser.c"
    break;

  case 506: /* server_key_cache_slabs: VAR_KEY_CACHE_SLABS STRING_ARG  */
#line 2244 "util/configparser.y"
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
#line 5385 "util/configparser.c"
    break;

  case 507: /* server_neg_cache_size: VAR_NEG_CACHE_SIZE STRING_ARG  */
#line 2257 "util/configparser.y"
        {
		OUTYY(("P(server_neg_cache_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->neg_cache_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 5396 "util/configparser.c"
    break;

  case 508: /* server_local_zone: VAR_LOCAL_ZONE STRING_ARG STRING_ARG  */
#line 2265 "util/configparser.y"
        {
		OUTYY(("P(server_local_zone:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "static")!=0 && strcmp((yyvsp[0].str), "deny")!=0 &&
		   strcmp((yyvsp[0].str), "refuse")!=0 && strcmp((yyvsp[0].str), "redirect")!=0 &&
		   strcmp((yyvsp[0].str), "transparent")!=0 && strcmp((yyvsp[0].str), "nodefault")!=0
		   && strcmp((yyvsp[0].str), "typetransparent")!=0
		   && strcmp((yyvsp[0].str), "always_transparent")!=0
		   && strcmp((yyvsp[0].str), "block_a")!=0
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
				"inform_redirect, always_transparent, block_a,"
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
#line 5455 "util/configparser.c"
    break;

  case 509: /* server_local_data: VAR_LOCAL_DATA STRING_ARG  */
#line 2321 "util/configparser.y"
        {
		OUTYY(("P(server_local_data:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->local_data, (yyvsp[0].str)))
			fatal_exit("out of memory adding local-data");
	}
#line 5465 "util/configparser.c"
    break;

  case 510: /* server_local_data_ptr: VAR_LOCAL_DATA_PTR STRING_ARG  */
#line 2328 "util/configparser.y"
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
#line 5483 "util/configparser.c"
    break;

  case 511: /* server_minimal_responses: VAR_MINIMAL_RESPONSES STRING_ARG  */
#line 2343 "util/configparser.y"
        {
		OUTYY(("P(server_minimal_responses:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->minimal_responses =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5496 "util/configparser.c"
    break;

  case 512: /* server_rrset_roundrobin: VAR_RRSET_ROUNDROBIN STRING_ARG  */
#line 2353 "util/configparser.y"
        {
		OUTYY(("P(server_rrset_roundrobin:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->rrset_roundrobin =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5509 "util/configparser.c"
    break;

  case 513: /* server_unknown_server_time_limit: VAR_UNKNOWN_SERVER_TIME_LIMIT STRING_ARG  */
#line 2363 "util/configparser.y"
        {
		OUTYY(("P(server_unknown_server_time_limit:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->unknown_server_time_limit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5519 "util/configparser.c"
    break;

  case 514: /* server_max_udp_size: VAR_MAX_UDP_SIZE STRING_ARG  */
#line 2370 "util/configparser.y"
        {
		OUTYY(("P(server_max_udp_size:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->max_udp_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5529 "util/configparser.c"
    break;

  case 515: /* server_dns64_prefix: VAR_DNS64_PREFIX STRING_ARG  */
#line 2377 "util/configparser.y"
        {
		OUTYY(("P(dns64_prefix:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dns64_prefix);
		cfg_parser->cfg->dns64_prefix = (yyvsp[0].str);
	}
#line 5539 "util/configparser.c"
    break;

  case 516: /* server_dns64_synthall: VAR_DNS64_SYNTHALL STRING_ARG  */
#line 2384 "util/configparser.y"
        {
		OUTYY(("P(server_dns64_synthall:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dns64_synthall = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5551 "util/configparser.c"
    break;

  case 517: /* server_dns64_ignore_aaaa: VAR_DNS64_IGNORE_AAAA STRING_ARG  */
#line 2393 "util/configparser.y"
        {
		OUTYY(("P(dns64_ignore_aaaa:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dns64_ignore_aaaa,
			(yyvsp[0].str)))
			fatal_exit("out of memory adding dns64-ignore-aaaa");
	}
#line 5562 "util/configparser.c"
    break;

  case 518: /* server_nat64_prefix: VAR_NAT64_PREFIX STRING_ARG  */
#line 2401 "util/configparser.y"
        {
		OUTYY(("P(nat64_prefix:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->nat64_prefix);
		cfg_parser->cfg->nat64_prefix = (yyvsp[0].str);
	}
#line 5572 "util/configparser.c"
    break;

  case 519: /* server_define_tag: VAR_DEFINE_TAG STRING_ARG  */
#line 2408 "util/configparser.y"
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
#line 5589 "util/configparser.c"
    break;

  case 520: /* server_local_zone_tag: VAR_LOCAL_ZONE_TAG STRING_ARG STRING_ARG  */
#line 2422 "util/configparser.y"
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
#line 5613 "util/configparser.c"
    break;

  case 521: /* server_access_control_tag: VAR_ACCESS_CONTROL_TAG STRING_ARG STRING_ARG  */
#line 2443 "util/configparser.y"
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
#line 5637 "util/configparser.c"
    break;

  case 522: /* server_access_control_tag_action: VAR_ACCESS_CONTROL_TAG_ACTION STRING_ARG STRING_ARG STRING_ARG  */
#line 2464 "util/configparser.y"
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
#line 5652 "util/configparser.c"
    break;

  case 523: /* server_access_control_tag_data: VAR_ACCESS_CONTROL_TAG_DATA STRING_ARG STRING_ARG STRING_ARG  */
#line 2476 "util/configparser.y"
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
#line 5667 "util/configparser.c"
    break;

  case 524: /* server_local_zone_override: VAR_LOCAL_ZONE_OVERRIDE STRING_ARG STRING_ARG STRING_ARG  */
#line 2488 "util/configparser.y"
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
#line 5682 "util/configparser.c"
    break;

  case 525: /* server_access_control_view: VAR_ACCESS_CONTROL_VIEW STRING_ARG STRING_ARG  */
#line 2500 "util/configparser.y"
        {
		OUTYY(("P(server_access_control_view:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str2list_insert(&cfg_parser->cfg->acl_view,
			(yyvsp[-1].str), (yyvsp[0].str))) {
			yyerror("out of memory");
		}
	}
#line 5694 "util/configparser.c"
    break;

  case 526: /* server_interface_tag: VAR_INTERFACE_TAG STRING_ARG STRING_ARG  */
#line 2509 "util/configparser.y"
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
#line 5718 "util/configparser.c"
    break;

  case 527: /* server_interface_tag_action: VAR_INTERFACE_TAG_ACTION STRING_ARG STRING_ARG STRING_ARG  */
#line 2530 "util/configparser.y"
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
#line 5733 "util/configparser.c"
    break;

  case 528: /* server_interface_tag_data: VAR_INTERFACE_TAG_DATA STRING_ARG STRING_ARG STRING_ARG  */
#line 2542 "util/configparser.y"
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
#line 5748 "util/configparser.c"
    break;

  case 529: /* server_interface_view: VAR_INTERFACE_VIEW STRING_ARG STRING_ARG  */
#line 2554 "util/configparser.y"
        {
		OUTYY(("P(server_interface_view:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str2list_insert(&cfg_parser->cfg->interface_view,
			(yyvsp[-1].str), (yyvsp[0].str))) {
			yyerror("out of memory");
		}
	}
#line 5760 "util/configparser.c"
    break;

  case 530: /* server_response_ip_tag: VAR_RESPONSE_IP_TAG STRING_ARG STRING_ARG  */
#line 2563 "util/configparser.y"
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
#line 5784 "util/configparser.c"
    break;

  case 531: /* server_ip_ratelimit: VAR_IP_RATELIMIT STRING_ARG  */
#line 2584 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ip_ratelimit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5796 "util/configparser.c"
    break;

  case 532: /* server_ip_ratelimit_cookie: VAR_IP_RATELIMIT_COOKIE STRING_ARG  */
#line 2593 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit_cookie:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ip_ratelimit_cookie = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5808 "util/configparser.c"
    break;

  case 533: /* server_ratelimit: VAR_RATELIMIT STRING_ARG  */
#line 2602 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ratelimit = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5820 "util/configparser.c"
    break;

  case 534: /* server_ip_ratelimit_size: VAR_IP_RATELIMIT_SIZE STRING_ARG  */
#line 2611 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->ip_ratelimit_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 5831 "util/configparser.c"
    break;

  case 535: /* server_ratelimit_size: VAR_RATELIMIT_SIZE STRING_ARG  */
#line 2619 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit_size:%s)\n", (yyvsp[0].str)));
		if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->ratelimit_size))
			yyerror("memory size expected");
		free((yyvsp[0].str));
	}
#line 5842 "util/configparser.c"
    break;

  case 536: /* server_ip_ratelimit_slabs: VAR_IP_RATELIMIT_SLABS STRING_ARG  */
#line 2627 "util/configparser.y"
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
#line 5858 "util/configparser.c"
    break;

  case 537: /* server_ratelimit_slabs: VAR_RATELIMIT_SLABS STRING_ARG  */
#line 2640 "util/configparser.y"
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
#line 5874 "util/configparser.c"
    break;

  case 538: /* server_ratelimit_for_domain: VAR_RATELIMIT_FOR_DOMAIN STRING_ARG STRING_ARG  */
#line 2653 "util/configparser.y"
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
#line 5892 "util/configparser.c"
    break;

  case 539: /* server_ratelimit_below_domain: VAR_RATELIMIT_BELOW_DOMAIN STRING_ARG STRING_ARG  */
#line 2668 "util/configparser.y"
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
#line 5910 "util/configparser.c"
    break;

  case 540: /* server_ip_ratelimit_factor: VAR_IP_RATELIMIT_FACTOR STRING_ARG  */
#line 2683 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit_factor:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ip_ratelimit_factor = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5922 "util/configparser.c"
    break;

  case 541: /* server_ratelimit_factor: VAR_RATELIMIT_FACTOR STRING_ARG  */
#line 2692 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit_factor:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->ratelimit_factor = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5934 "util/configparser.c"
    break;

  case 542: /* server_ip_ratelimit_backoff: VAR_IP_RATELIMIT_BACKOFF STRING_ARG  */
#line 2701 "util/configparser.y"
        {
		OUTYY(("P(server_ip_ratelimit_backoff:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ip_ratelimit_backoff =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5947 "util/configparser.c"
    break;

  case 543: /* server_ratelimit_backoff: VAR_RATELIMIT_BACKOFF STRING_ARG  */
#line 2711 "util/configparser.y"
        {
		OUTYY(("P(server_ratelimit_backoff:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ratelimit_backoff =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 5960 "util/configparser.c"
    break;

  case 544: /* server_outbound_msg_retry: VAR_OUTBOUND_MSG_RETRY STRING_ARG  */
#line 2721 "util/configparser.y"
        {
		OUTYY(("P(server_outbound_msg_retry:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->outbound_msg_retry = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5972 "util/configparser.c"
    break;

  case 545: /* server_max_sent_count: VAR_MAX_SENT_COUNT STRING_ARG  */
#line 2730 "util/configparser.y"
        {
		OUTYY(("P(server_max_sent_count:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_sent_count = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5984 "util/configparser.c"
    break;

  case 546: /* server_max_query_restarts: VAR_MAX_QUERY_RESTARTS STRING_ARG  */
#line 2739 "util/configparser.y"
        {
		OUTYY(("P(server_max_query_restarts:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->max_query_restarts = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 5996 "util/configparser.c"
    break;

  case 547: /* server_low_rtt: VAR_LOW_RTT STRING_ARG  */
#line 2748 "util/configparser.y"
        {
		OUTYY(("P(low-rtt option is deprecated, use fast-server-num instead)\n"));
		free((yyvsp[0].str));
	}
#line 6005 "util/configparser.c"
    break;

  case 548: /* server_fast_server_num: VAR_FAST_SERVER_NUM STRING_ARG  */
#line 2754 "util/configparser.y"
        {
		OUTYY(("P(server_fast_server_num:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) <= 0)
			yyerror("number expected");
		else cfg_parser->cfg->fast_server_num = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6017 "util/configparser.c"
    break;

  case 549: /* server_fast_server_permil: VAR_FAST_SERVER_PERMIL STRING_ARG  */
#line 2763 "util/configparser.y"
        {
		OUTYY(("P(server_fast_server_permil:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("number expected");
		else cfg_parser->cfg->fast_server_permil = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6029 "util/configparser.c"
    break;

  case 550: /* server_qname_minimisation: VAR_QNAME_MINIMISATION STRING_ARG  */
#line 2772 "util/configparser.y"
        {
		OUTYY(("P(server_qname_minimisation:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->qname_minimisation =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6042 "util/configparser.c"
    break;

  case 551: /* server_qname_minimisation_strict: VAR_QNAME_MINIMISATION_STRICT STRING_ARG  */
#line 2782 "util/configparser.y"
        {
		OUTYY(("P(server_qname_minimisation_strict:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->qname_minimisation_strict =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6055 "util/configparser.c"
    break;

  case 552: /* server_pad_responses: VAR_PAD_RESPONSES STRING_ARG  */
#line 2792 "util/configparser.y"
        {
		OUTYY(("P(server_pad_responses:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->pad_responses =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6068 "util/configparser.c"
    break;

  case 553: /* server_pad_responses_block_size: VAR_PAD_RESPONSES_BLOCK_SIZE STRING_ARG  */
#line 2802 "util/configparser.y"
        {
		OUTYY(("P(server_pad_responses_block_size:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->pad_responses_block_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6080 "util/configparser.c"
    break;

  case 554: /* server_pad_queries: VAR_PAD_QUERIES STRING_ARG  */
#line 2811 "util/configparser.y"
        {
		OUTYY(("P(server_pad_queries:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->pad_queries =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6093 "util/configparser.c"
    break;

  case 555: /* server_pad_queries_block_size: VAR_PAD_QUERIES_BLOCK_SIZE STRING_ARG  */
#line 2821 "util/configparser.y"
        {
		OUTYY(("P(server_pad_queries_block_size:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("number expected");
		else cfg_parser->cfg->pad_queries_block_size = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6105 "util/configparser.c"
    break;

  case 556: /* server_ipsecmod_enabled: VAR_IPSECMOD_ENABLED STRING_ARG  */
#line 2830 "util/configparser.y"
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
#line 6121 "util/configparser.c"
    break;

  case 557: /* server_ipsecmod_ignore_bogus: VAR_IPSECMOD_IGNORE_BOGUS STRING_ARG  */
#line 2843 "util/configparser.y"
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
#line 6137 "util/configparser.c"
    break;

  case 558: /* server_ipsecmod_hook: VAR_IPSECMOD_HOOK STRING_ARG  */
#line 2856 "util/configparser.y"
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
#line 6152 "util/configparser.c"
    break;

  case 559: /* server_ipsecmod_max_ttl: VAR_IPSECMOD_MAX_TTL STRING_ARG  */
#line 2868 "util/configparser.y"
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
#line 6169 "util/configparser.c"
    break;

  case 560: /* server_ipsecmod_whitelist: VAR_IPSECMOD_WHITELIST STRING_ARG  */
#line 2882 "util/configparser.y"
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
#line 6184 "util/configparser.c"
    break;

  case 561: /* server_ipsecmod_strict: VAR_IPSECMOD_STRICT STRING_ARG  */
#line 2894 "util/configparser.y"
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
#line 6201 "util/configparser.c"
    break;

  case 562: /* server_edns_client_string: VAR_EDNS_CLIENT_STRING STRING_ARG STRING_ARG  */
#line 2908 "util/configparser.y"
        {
		OUTYY(("P(server_edns_client_string:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->edns_client_strings, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding "
				"edns-client-string");
	}
#line 6213 "util/configparser.c"
    break;

  case 563: /* server_edns_client_string_opcode: VAR_EDNS_CLIENT_STRING_OPCODE STRING_ARG  */
#line 2917 "util/configparser.y"
        {
		OUTYY(("P(edns_client_string_opcode:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0 && strcmp((yyvsp[0].str), "0") != 0)
			yyerror("option code expected");
		else if(atoi((yyvsp[0].str)) > 65535 || atoi((yyvsp[0].str)) < 0)
			yyerror("option code must be in interval [0, 65535]");
		else cfg_parser->cfg->edns_client_string_opcode = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6227 "util/configparser.c"
    break;

  case 564: /* server_ede: VAR_EDE STRING_ARG  */
#line 2928 "util/configparser.y"
        {
		OUTYY(("P(server_ede:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->ede = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6239 "util/configparser.c"
    break;

  case 565: /* server_proxy_protocol_port: VAR_PROXY_PROTOCOL_PORT STRING_ARG  */
#line 2937 "util/configparser.y"
        {
		OUTYY(("P(server_proxy_protocol_port:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->proxy_protocol_port, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6249 "util/configparser.c"
    break;

  case 566: /* stub_name: VAR_NAME STRING_ARG  */
#line 2944 "util/configparser.y"
        {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->stubs->name)
			yyerror("stub name override, there must be one name "
				"for one stub-zone");
		free(cfg_parser->cfg->stubs->name);
		cfg_parser->cfg->stubs->name = (yyvsp[0].str);
	}
#line 6262 "util/configparser.c"
    break;

  case 567: /* stub_host: VAR_STUB_HOST STRING_ARG  */
#line 2954 "util/configparser.y"
        {
		OUTYY(("P(stub-host:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->stubs->hosts, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6272 "util/configparser.c"
    break;

  case 568: /* stub_addr: VAR_STUB_ADDR STRING_ARG  */
#line 2961 "util/configparser.y"
        {
		OUTYY(("P(stub-addr:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->stubs->addrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6282 "util/configparser.c"
    break;

  case 569: /* stub_first: VAR_STUB_FIRST STRING_ARG  */
#line 2968 "util/configparser.y"
        {
		OUTYY(("P(stub-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6294 "util/configparser.c"
    break;

  case 570: /* stub_no_cache: VAR_STUB_NO_CACHE STRING_ARG  */
#line 2977 "util/configparser.y"
        {
		OUTYY(("P(stub-no-cache:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->no_cache=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6306 "util/configparser.c"
    break;

  case 571: /* stub_ssl_upstream: VAR_STUB_SSL_UPSTREAM STRING_ARG  */
#line 2986 "util/configparser.y"
        {
		OUTYY(("P(stub-ssl-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->ssl_upstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6319 "util/configparser.c"
    break;

  case 572: /* stub_tcp_upstream: VAR_STUB_TCP_UPSTREAM STRING_ARG  */
#line 2996 "util/configparser.y"
        {
                OUTYY(("P(stub-tcp-upstream:%s)\n", (yyvsp[0].str)));
                if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
                        yyerror("expected yes or no.");
                else cfg_parser->cfg->stubs->tcp_upstream =
                        (strcmp((yyvsp[0].str), "yes")==0);
                free((yyvsp[0].str));
        }
#line 6332 "util/configparser.c"
    break;

  case 573: /* stub_prime: VAR_STUB_PRIME STRING_ARG  */
#line 3006 "util/configparser.y"
        {
		OUTYY(("P(stub-prime:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->stubs->isprime =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6345 "util/configparser.c"
    break;

  case 574: /* forward_name: VAR_NAME STRING_ARG  */
#line 3016 "util/configparser.y"
        {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->forwards->name)
			yyerror("forward name override, there must be one "
				"name for one forward-zone");
		free(cfg_parser->cfg->forwards->name);
		cfg_parser->cfg->forwards->name = (yyvsp[0].str);
	}
#line 6358 "util/configparser.c"
    break;

  case 575: /* forward_host: VAR_FORWARD_HOST STRING_ARG  */
#line 3026 "util/configparser.y"
        {
		OUTYY(("P(forward-host:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->forwards->hosts, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6368 "util/configparser.c"
    break;

  case 576: /* forward_addr: VAR_FORWARD_ADDR STRING_ARG  */
#line 3033 "util/configparser.y"
        {
		OUTYY(("P(forward-addr:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->forwards->addrs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6378 "util/configparser.c"
    break;

  case 577: /* forward_first: VAR_FORWARD_FIRST STRING_ARG  */
#line 3040 "util/configparser.y"
        {
		OUTYY(("P(forward-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6390 "util/configparser.c"
    break;

  case 578: /* forward_no_cache: VAR_FORWARD_NO_CACHE STRING_ARG  */
#line 3049 "util/configparser.y"
        {
		OUTYY(("P(forward-no-cache:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->no_cache=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6402 "util/configparser.c"
    break;

  case 579: /* forward_ssl_upstream: VAR_FORWARD_SSL_UPSTREAM STRING_ARG  */
#line 3058 "util/configparser.y"
        {
		OUTYY(("P(forward-ssl-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->forwards->ssl_upstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6415 "util/configparser.c"
    break;

  case 580: /* forward_tcp_upstream: VAR_FORWARD_TCP_UPSTREAM STRING_ARG  */
#line 3068 "util/configparser.y"
        {
                OUTYY(("P(forward-tcp-upstream:%s)\n", (yyvsp[0].str)));
                if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
                        yyerror("expected yes or no.");
                else cfg_parser->cfg->forwards->tcp_upstream =
                        (strcmp((yyvsp[0].str), "yes")==0);
                free((yyvsp[0].str));
        }
#line 6428 "util/configparser.c"
    break;

  case 581: /* auth_name: VAR_NAME STRING_ARG  */
#line 3078 "util/configparser.y"
        {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->auths->name)
			yyerror("auth name override, there must be one name "
				"for one auth-zone");
		free(cfg_parser->cfg->auths->name);
		cfg_parser->cfg->auths->name = (yyvsp[0].str);
	}
#line 6441 "util/configparser.c"
    break;

  case 582: /* auth_zonefile: VAR_ZONEFILE STRING_ARG  */
#line 3088 "util/configparser.y"
        {
		OUTYY(("P(zonefile:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->auths->zonefile);
		cfg_parser->cfg->auths->zonefile = (yyvsp[0].str);
	}
#line 6451 "util/configparser.c"
    break;

  case 583: /* auth_master: VAR_MASTER STRING_ARG  */
#line 3095 "util/configparser.y"
        {
		OUTYY(("P(master:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->masters, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6461 "util/configparser.c"
    break;

  case 584: /* auth_url: VAR_URL STRING_ARG  */
#line 3102 "util/configparser.y"
        {
		OUTYY(("P(url:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->urls, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6471 "util/configparser.c"
    break;

  case 585: /* auth_allow_notify: VAR_ALLOW_NOTIFY STRING_ARG  */
#line 3109 "util/configparser.y"
        {
		OUTYY(("P(allow-notify:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->auths->allow_notify,
			(yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6482 "util/configparser.c"
    break;

  case 586: /* auth_zonemd_check: VAR_ZONEMD_CHECK STRING_ARG  */
#line 3117 "util/configparser.y"
        {
		OUTYY(("P(zonemd-check:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->zonemd_check =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6495 "util/configparser.c"
    break;

  case 587: /* auth_zonemd_reject_absence: VAR_ZONEMD_REJECT_ABSENCE STRING_ARG  */
#line 3127 "util/configparser.y"
        {
		OUTYY(("P(zonemd-reject-absence:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->zonemd_reject_absence =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6508 "util/configparser.c"
    break;

  case 588: /* auth_for_downstream: VAR_FOR_DOWNSTREAM STRING_ARG  */
#line 3137 "util/configparser.y"
        {
		OUTYY(("P(for-downstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->for_downstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6521 "util/configparser.c"
    break;

  case 589: /* auth_for_upstream: VAR_FOR_UPSTREAM STRING_ARG  */
#line 3147 "util/configparser.y"
        {
		OUTYY(("P(for-upstream:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->for_upstream =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6534 "util/configparser.c"
    break;

  case 590: /* auth_fallback_enabled: VAR_FALLBACK_ENABLED STRING_ARG  */
#line 3157 "util/configparser.y"
        {
		OUTYY(("P(fallback-enabled:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->auths->fallback_enabled =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6547 "util/configparser.c"
    break;

  case 591: /* view_name: VAR_NAME STRING_ARG  */
#line 3167 "util/configparser.y"
        {
		OUTYY(("P(name:%s)\n", (yyvsp[0].str)));
		if(cfg_parser->cfg->views->name)
			yyerror("view name override, there must be one "
				"name for one view");
		free(cfg_parser->cfg->views->name);
		cfg_parser->cfg->views->name = (yyvsp[0].str);
	}
#line 6560 "util/configparser.c"
    break;

  case 592: /* view_local_zone: VAR_LOCAL_ZONE STRING_ARG STRING_ARG  */
#line 3177 "util/configparser.y"
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
#line 6619 "util/configparser.c"
    break;

  case 593: /* view_response_ip: VAR_RESPONSE_IP STRING_ARG STRING_ARG  */
#line 3233 "util/configparser.y"
        {
		OUTYY(("P(view_response_ip:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_respip_action((yyvsp[0].str));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->views->respip_actions, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding per-view "
				"response-ip action");
	}
#line 6632 "util/configparser.c"
    break;

  case 594: /* view_response_ip_data: VAR_RESPONSE_IP_DATA STRING_ARG STRING_ARG  */
#line 3243 "util/configparser.y"
        {
		OUTYY(("P(view_response_ip_data:%s)\n", (yyvsp[-1].str)));
		if(!cfg_str2list_insert(
			&cfg_parser->cfg->views->respip_data, (yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding response-ip-data");
	}
#line 6643 "util/configparser.c"
    break;

  case 595: /* view_local_data: VAR_LOCAL_DATA STRING_ARG  */
#line 3251 "util/configparser.y"
        {
		OUTYY(("P(view_local_data:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->views->local_data, (yyvsp[0].str))) {
			fatal_exit("out of memory adding local-data");
		}
	}
#line 6654 "util/configparser.c"
    break;

  case 596: /* view_local_data_ptr: VAR_LOCAL_DATA_PTR STRING_ARG  */
#line 3259 "util/configparser.y"
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
#line 6672 "util/configparser.c"
    break;

  case 597: /* view_first: VAR_VIEW_FIRST STRING_ARG  */
#line 3274 "util/configparser.y"
        {
		OUTYY(("P(view-first:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->views->isfirst=(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6684 "util/configparser.c"
    break;

  case 598: /* rcstart: VAR_REMOTE_CONTROL  */
#line 3283 "util/configparser.y"
        {
		OUTYY(("\nP(remote-control:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 6693 "util/configparser.c"
    break;

  case 609: /* rc_control_enable: VAR_CONTROL_ENABLE STRING_ARG  */
#line 3295 "util/configparser.y"
        {
		OUTYY(("P(control_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->remote_control_enable =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6706 "util/configparser.c"
    break;

  case 610: /* rc_control_port: VAR_CONTROL_PORT STRING_ARG  */
#line 3305 "util/configparser.y"
        {
		OUTYY(("P(control_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("control port number expected");
		else cfg_parser->cfg->control_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 6718 "util/configparser.c"
    break;

  case 611: /* rc_control_interface: VAR_CONTROL_INTERFACE STRING_ARG  */
#line 3314 "util/configparser.y"
        {
		OUTYY(("P(control_interface:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append(&cfg_parser->cfg->control_ifs, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 6728 "util/configparser.c"
    break;

  case 612: /* rc_control_use_cert: VAR_CONTROL_USE_CERT STRING_ARG  */
#line 3321 "util/configparser.y"
        {
		OUTYY(("P(control_use_cert:%s)\n", (yyvsp[0].str)));
		cfg_parser->cfg->control_use_cert = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6738 "util/configparser.c"
    break;

  case 613: /* rc_server_key_file: VAR_SERVER_KEY_FILE STRING_ARG  */
#line 3328 "util/configparser.y"
        {
		OUTYY(("P(rc_server_key_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->server_key_file);
		cfg_parser->cfg->server_key_file = (yyvsp[0].str);
	}
#line 6748 "util/configparser.c"
    break;

  case 614: /* rc_server_cert_file: VAR_SERVER_CERT_FILE STRING_ARG  */
#line 3335 "util/configparser.y"
        {
		OUTYY(("P(rc_server_cert_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->server_cert_file);
		cfg_parser->cfg->server_cert_file = (yyvsp[0].str);
	}
#line 6758 "util/configparser.c"
    break;

  case 615: /* rc_control_key_file: VAR_CONTROL_KEY_FILE STRING_ARG  */
#line 3342 "util/configparser.y"
        {
		OUTYY(("P(rc_control_key_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->control_key_file);
		cfg_parser->cfg->control_key_file = (yyvsp[0].str);
	}
#line 6768 "util/configparser.c"
    break;

  case 616: /* rc_control_cert_file: VAR_CONTROL_CERT_FILE STRING_ARG  */
#line 3349 "util/configparser.y"
        {
		OUTYY(("P(rc_control_cert_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->control_cert_file);
		cfg_parser->cfg->control_cert_file = (yyvsp[0].str);
	}
#line 6778 "util/configparser.c"
    break;

  case 617: /* dtstart: VAR_DNSTAP  */
#line 3356 "util/configparser.y"
        {
		OUTYY(("\nP(dnstap:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 6787 "util/configparser.c"
    break;

  case 639: /* dt_dnstap_enable: VAR_DNSTAP_ENABLE STRING_ARG  */
#line 3377 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6799 "util/configparser.c"
    break;

  case 640: /* dt_dnstap_bidirectional: VAR_DNSTAP_BIDIRECTIONAL STRING_ARG  */
#line 3386 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_bidirectional:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_bidirectional =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6812 "util/configparser.c"
    break;

  case 641: /* dt_dnstap_socket_path: VAR_DNSTAP_SOCKET_PATH STRING_ARG  */
#line 3396 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_socket_path:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_socket_path);
		cfg_parser->cfg->dnstap_socket_path = (yyvsp[0].str);
	}
#line 6822 "util/configparser.c"
    break;

  case 642: /* dt_dnstap_ip: VAR_DNSTAP_IP STRING_ARG  */
#line 3403 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_ip:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_ip);
		cfg_parser->cfg->dnstap_ip = (yyvsp[0].str);
	}
#line 6832 "util/configparser.c"
    break;

  case 643: /* dt_dnstap_tls: VAR_DNSTAP_TLS STRING_ARG  */
#line 3410 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_tls = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6844 "util/configparser.c"
    break;

  case 644: /* dt_dnstap_tls_server_name: VAR_DNSTAP_TLS_SERVER_NAME STRING_ARG  */
#line 3419 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls_server_name:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_tls_server_name);
		cfg_parser->cfg->dnstap_tls_server_name = (yyvsp[0].str);
	}
#line 6854 "util/configparser.c"
    break;

  case 645: /* dt_dnstap_tls_cert_bundle: VAR_DNSTAP_TLS_CERT_BUNDLE STRING_ARG  */
#line 3426 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls_cert_bundle:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_tls_cert_bundle);
		cfg_parser->cfg->dnstap_tls_cert_bundle = (yyvsp[0].str);
	}
#line 6864 "util/configparser.c"
    break;

  case 646: /* dt_dnstap_tls_client_key_file: VAR_DNSTAP_TLS_CLIENT_KEY_FILE STRING_ARG  */
#line 3433 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls_client_key_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_tls_client_key_file);
		cfg_parser->cfg->dnstap_tls_client_key_file = (yyvsp[0].str);
	}
#line 6874 "util/configparser.c"
    break;

  case 647: /* dt_dnstap_tls_client_cert_file: VAR_DNSTAP_TLS_CLIENT_CERT_FILE STRING_ARG  */
#line 3440 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_tls_client_cert_file:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_tls_client_cert_file);
		cfg_parser->cfg->dnstap_tls_client_cert_file = (yyvsp[0].str);
	}
#line 6884 "util/configparser.c"
    break;

  case 648: /* dt_dnstap_send_identity: VAR_DNSTAP_SEND_IDENTITY STRING_ARG  */
#line 3447 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_send_identity:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_send_identity = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6896 "util/configparser.c"
    break;

  case 649: /* dt_dnstap_send_version: VAR_DNSTAP_SEND_VERSION STRING_ARG  */
#line 3456 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_send_version:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_send_version = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6908 "util/configparser.c"
    break;

  case 650: /* dt_dnstap_identity: VAR_DNSTAP_IDENTITY STRING_ARG  */
#line 3465 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_identity);
		cfg_parser->cfg->dnstap_identity = (yyvsp[0].str);
	}
#line 6918 "util/configparser.c"
    break;

  case 651: /* dt_dnstap_version: VAR_DNSTAP_VERSION STRING_ARG  */
#line 3472 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_version:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnstap_version);
		cfg_parser->cfg->dnstap_version = (yyvsp[0].str);
	}
#line 6928 "util/configparser.c"
    break;

  case 652: /* dt_dnstap_log_resolver_query_messages: VAR_DNSTAP_LOG_RESOLVER_QUERY_MESSAGES STRING_ARG  */
#line 3479 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_resolver_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_resolver_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6941 "util/configparser.c"
    break;

  case 653: /* dt_dnstap_log_resolver_response_messages: VAR_DNSTAP_LOG_RESOLVER_RESPONSE_MESSAGES STRING_ARG  */
#line 3489 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_resolver_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_resolver_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6954 "util/configparser.c"
    break;

  case 654: /* dt_dnstap_log_client_query_messages: VAR_DNSTAP_LOG_CLIENT_QUERY_MESSAGES STRING_ARG  */
#line 3499 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_client_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_client_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6967 "util/configparser.c"
    break;

  case 655: /* dt_dnstap_log_client_response_messages: VAR_DNSTAP_LOG_CLIENT_RESPONSE_MESSAGES STRING_ARG  */
#line 3509 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_client_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_client_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6980 "util/configparser.c"
    break;

  case 656: /* dt_dnstap_log_forwarder_query_messages: VAR_DNSTAP_LOG_FORWARDER_QUERY_MESSAGES STRING_ARG  */
#line 3519 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_forwarder_query_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_forwarder_query_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 6993 "util/configparser.c"
    break;

  case 657: /* dt_dnstap_log_forwarder_response_messages: VAR_DNSTAP_LOG_FORWARDER_RESPONSE_MESSAGES STRING_ARG  */
#line 3529 "util/configparser.y"
        {
		OUTYY(("P(dt_dnstap_log_forwarder_response_messages:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnstap_log_forwarder_response_messages =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7006 "util/configparser.c"
    break;

  case 658: /* pythonstart: VAR_PYTHON  */
#line 3539 "util/configparser.y"
        {
		OUTYY(("\nP(python:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 7015 "util/configparser.c"
    break;

  case 662: /* py_script: VAR_PYTHON_SCRIPT STRING_ARG  */
#line 3549 "util/configparser.y"
        {
		OUTYY(("P(python-script:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append_ex(&cfg_parser->cfg->python_script, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 7025 "util/configparser.c"
    break;

  case 663: /* dynlibstart: VAR_DYNLIB  */
#line 3556 "util/configparser.y"
        {
		OUTYY(("\nP(dynlib:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 7034 "util/configparser.c"
    break;

  case 667: /* dl_file: VAR_DYNLIB_FILE STRING_ARG  */
#line 3566 "util/configparser.y"
        {
		OUTYY(("P(dynlib-file:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_append_ex(&cfg_parser->cfg->dynlib_file, (yyvsp[0].str)))
			yyerror("out of memory");
	}
#line 7044 "util/configparser.c"
    break;

  case 668: /* server_disable_dnssec_lame_check: VAR_DISABLE_DNSSEC_LAME_CHECK STRING_ARG  */
#line 3573 "util/configparser.y"
        {
		OUTYY(("P(disable_dnssec_lame_check:%s)\n", (yyvsp[0].str)));
		if (strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->disable_dnssec_lame_check =
			(strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7057 "util/configparser.c"
    break;

  case 669: /* server_log_identity: VAR_LOG_IDENTITY STRING_ARG  */
#line 3583 "util/configparser.y"
        {
		OUTYY(("P(server_log_identity:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->log_identity);
		cfg_parser->cfg->log_identity = (yyvsp[0].str);
	}
#line 7067 "util/configparser.c"
    break;

  case 670: /* server_response_ip: VAR_RESPONSE_IP STRING_ARG STRING_ARG  */
#line 3590 "util/configparser.y"
        {
		OUTYY(("P(server_response_ip:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		validate_respip_action((yyvsp[0].str));
		if(!cfg_str2list_insert(&cfg_parser->cfg->respip_actions,
			(yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding response-ip");
	}
#line 7079 "util/configparser.c"
    break;

  case 671: /* server_response_ip_data: VAR_RESPONSE_IP_DATA STRING_ARG STRING_ARG  */
#line 3599 "util/configparser.y"
        {
		OUTYY(("P(server_response_ip_data:%s)\n", (yyvsp[-1].str)));
		if(!cfg_str2list_insert(&cfg_parser->cfg->respip_data,
			(yyvsp[-1].str), (yyvsp[0].str)))
			fatal_exit("out of memory adding response-ip-data");
	}
#line 7090 "util/configparser.c"
    break;

  case 672: /* dnscstart: VAR_DNSCRYPT  */
#line 3607 "util/configparser.y"
        {
		OUTYY(("\nP(dnscrypt:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 7099 "util/configparser.c"
    break;

  case 685: /* dnsc_dnscrypt_enable: VAR_DNSCRYPT_ENABLE STRING_ARG  */
#line 3624 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_enable:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->dnscrypt = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7111 "util/configparser.c"
    break;

  case 686: /* dnsc_dnscrypt_port: VAR_DNSCRYPT_PORT STRING_ARG  */
#line 3633 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_port:%s)\n", (yyvsp[0].str)));
		if(atoi((yyvsp[0].str)) == 0)
			yyerror("port number expected");
		else cfg_parser->cfg->dnscrypt_port = atoi((yyvsp[0].str));
		free((yyvsp[0].str));
	}
#line 7123 "util/configparser.c"
    break;

  case 687: /* dnsc_dnscrypt_provider: VAR_DNSCRYPT_PROVIDER STRING_ARG  */
#line 3642 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_provider:%s)\n", (yyvsp[0].str)));
		free(cfg_parser->cfg->dnscrypt_provider);
		cfg_parser->cfg->dnscrypt_provider = (yyvsp[0].str);
	}
#line 7133 "util/configparser.c"
    break;

  case 688: /* dnsc_dnscrypt_provider_cert: VAR_DNSCRYPT_PROVIDER_CERT STRING_ARG  */
#line 3649 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_provider_cert:%s)\n", (yyvsp[0].str)));
		if(cfg_strlist_find(cfg_parser->cfg->dnscrypt_provider_cert, (yyvsp[0].str)))
			log_warn("dnscrypt-provider-cert %s is a duplicate", (yyvsp[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_provider_cert, (yyvsp[0].str)))
			fatal_exit("out of memory adding dnscrypt-provider-cert");
	}
#line 7145 "util/configparser.c"
    break;

  case 689: /* dnsc_dnscrypt_provider_cert_rotated: VAR_DNSCRYPT_PROVIDER_CERT_ROTATED STRING_ARG  */
#line 3658 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_provider_cert_rotated:%s)\n", (yyvsp[0].str)));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_provider_cert_rotated, (yyvsp[0].str)))
			fatal_exit("out of memory adding dnscrypt-provider-cert-rotated");
	}
#line 7155 "util/configparser.c"
    break;

  case 690: /* dnsc_dnscrypt_secret_key: VAR_DNSCRYPT_SECRET_KEY STRING_ARG  */
#line 3665 "util/configparser.y"
        {
		OUTYY(("P(dnsc_dnscrypt_secret_key:%s)\n", (yyvsp[0].str)));
		if(cfg_strlist_find(cfg_parser->cfg->dnscrypt_secret_key, (yyvsp[0].str)))
			log_warn("dnscrypt-secret-key: %s is a duplicate", (yyvsp[0].str));
		if(!cfg_strlist_insert(&cfg_parser->cfg->dnscrypt_secret_key, (yyvsp[0].str)))
			fatal_exit("out of memory adding dnscrypt-secret-key");
	}
#line 7167 "util/configparser.c"
    break;

  case 691: /* dnsc_dnscrypt_shared_secret_cache_size: VAR_DNSCRYPT_SHARED_SECRET_CACHE_SIZE STRING_ARG  */
#line 3674 "util/configparser.y"
  {
	OUTYY(("P(dnscrypt_shared_secret_cache_size:%s)\n", (yyvsp[0].str)));
	if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->dnscrypt_shared_secret_cache_size))
		yyerror("memory size expected");
	free((yyvsp[0].str));
  }
#line 7178 "util/configparser.c"
    break;

  case 692: /* dnsc_dnscrypt_shared_secret_cache_slabs: VAR_DNSCRYPT_SHARED_SECRET_CACHE_SLABS STRING_ARG  */
#line 3682 "util/configparser.y"
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
#line 7194 "util/configparser.c"
    break;

  case 693: /* dnsc_dnscrypt_nonce_cache_size: VAR_DNSCRYPT_NONCE_CACHE_SIZE STRING_ARG  */
#line 3695 "util/configparser.y"
  {
	OUTYY(("P(dnscrypt_nonce_cache_size:%s)\n", (yyvsp[0].str)));
	if(!cfg_parse_memsize((yyvsp[0].str), &cfg_parser->cfg->dnscrypt_nonce_cache_size))
		yyerror("memory size expected");
	free((yyvsp[0].str));
  }
#line 7205 "util/configparser.c"
    break;

  case 694: /* dnsc_dnscrypt_nonce_cache_slabs: VAR_DNSCRYPT_NONCE_CACHE_SLABS STRING_ARG  */
#line 3703 "util/configparser.y"
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
#line 7221 "util/configparser.c"
    break;

  case 695: /* cachedbstart: VAR_CACHEDB  */
#line 3716 "util/configparser.y"
        {
		OUTYY(("\nP(cachedb:)\n"));
		cfg_parser->started_toplevel = 1;
	}
#line 7230 "util/configparser.c"
    break;

  case 708: /* cachedb_backend_name: VAR_CACHEDB_BACKEND STRING_ARG  */
#line 3729 "util/configparser.y"
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
#line 7245 "util/configparser.c"
    break;

  case 709: /* cachedb_secret_seed: VAR_CACHEDB_SECRETSEED STRING_ARG  */
#line 3741 "util/configparser.y"
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
#line 7260 "util/configparser.c"
    break;

  case 710: /* cachedb_no_store: VAR_CACHEDB_NO_STORE STRING_ARG  */
#line 3753 "util/configparser.y"
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
#line 7276 "util/configparser.c"
    break;

  case 711: /* redis_server_host: VAR_CACHEDB_REDISHOST STRING_ARG  */
#line 3766 "util/configparser.y"
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
#line 7291 "util/configparser.c"
    break;

  case 712: /* redis_server_port: VAR_CACHEDB_REDISPORT STRING_ARG  */
#line 3778 "util/configparser.y"
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
#line 7309 "util/configparser.c"
    break;

  case 713: /* redis_server_path: VAR_CACHEDB_REDISPATH STRING_ARG  */
#line 3793 "util/configparser.y"
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
#line 7324 "util/configparser.c"
    break;

  case 714: /* redis_server_password: VAR_CACHEDB_REDISPASSWORD STRING_ARG  */
#line 3805 "util/configparser.y"
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
#line 7339 "util/configparser.c"
    break;

  case 715: /* redis_timeout: VAR_CACHEDB_REDISTIMEOUT STRING_ARG  */
#line 3817 "util/configparser.y"
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
#line 7355 "util/configparser.c"
    break;

  case 716: /* redis_expire_records: VAR_CACHEDB_REDISEXPIRERECORDS STRING_ARG  */
#line 3830 "util/configparser.y"
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
#line 7371 "util/configparser.c"
    break;

  case 717: /* redis_logical_db: VAR_CACHEDB_REDISLOGICALDB STRING_ARG  */
#line 3843 "util/configparser.y"
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
#line 7389 "util/configparser.c"
    break;

  case 718: /* server_tcp_connection_limit: VAR_TCP_CONNECTION_LIMIT STRING_ARG STRING_ARG  */
#line 3858 "util/configparser.y"
        {
		OUTYY(("P(server_tcp_connection_limit:%s %s)\n", (yyvsp[-1].str), (yyvsp[0].str)));
		if (atoi((yyvsp[0].str)) < 0)
			yyerror("positive number expected");
		else {
			if(!cfg_str2list_insert(&cfg_parser->cfg->tcp_connection_limits, (yyvsp[-1].str), (yyvsp[0].str)))
				fatal_exit("out of memory adding tcp connection limit");
		}
	}
#line 7403 "util/configparser.c"
    break;

  case 719: /* server_answer_cookie: VAR_ANSWER_COOKIE STRING_ARG  */
#line 3869 "util/configparser.y"
        {
		OUTYY(("P(server_answer_cookie:%s)\n", (yyvsp[0].str)));
		if(strcmp((yyvsp[0].str), "yes") != 0 && strcmp((yyvsp[0].str), "no") != 0)
			yyerror("expected yes or no.");
		else cfg_parser->cfg->do_answer_cookie = (strcmp((yyvsp[0].str), "yes")==0);
		free((yyvsp[0].str));
	}
#line 7415 "util/configparser.c"
    break;

  case 720: /* server_cookie_secret: VAR_COOKIE_SECRET STRING_ARG  */
#line 3878 "util/configparser.y"
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
#line 7434 "util/configparser.c"
    break;

  case 721: /* ipsetstart: VAR_IPSET  */
#line 3894 "util/configparser.y"
                {
			OUTYY(("\nP(ipset:)\n"));
			cfg_parser->started_toplevel = 1;
		}
#line 7443 "util/configparser.c"
    break;

  case 726: /* ipset_name_v4: VAR_IPSET_NAME_V4 STRING_ARG  */
#line 3904 "util/configparser.y"
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
#line 7461 "util/configparser.c"
    break;

  case 727: /* ipset_name_v6: VAR_IPSET_NAME_V6 STRING_ARG  */
#line 3919 "util/configparser.y"
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
#line 7479 "util/configparser.c"
    break;


#line 7483 "util/configparser.c"

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

#line 3933 "util/configparser.y"


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
