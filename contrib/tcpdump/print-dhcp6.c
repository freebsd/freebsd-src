/*
 * Copyright (C) 1998 and 1999 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* \summary: IPv6 DHCP printer */

/*
 * RFC3315: DHCPv6
 * supported DHCPv6 options:
 *  RFC3319: Session Initiation Protocol (SIP) Servers options,
 *  RFC3633: IPv6 Prefix options,
 *  RFC3646: DNS Configuration options,
 *  RFC3898: Network Information Service (NIS) Configuration options,
 *  RFC4075: Simple Network Time Protocol (SNTP) Configuration option,
 *  RFC4242: Information Refresh Time option,
 *  RFC4280: Broadcast and Multicast Control Servers options,
 *  RFC5908: Network Time Protocol (NTP) Server Option for DHCPv6
 *  RFC6334: Dual-Stack Lite option,
 */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

/* lease duration */
#define DHCP6_DURATION_INFINITE 0xffffffff

/* Error Values */
#define DH6ERR_FAILURE		16
#define DH6ERR_AUTHFAIL		17
#define DH6ERR_POORLYFORMED	18
#define DH6ERR_UNAVAIL		19
#define DH6ERR_OPTUNAVAIL	20

/* Message type */
#define DH6_SOLICIT	1
#define DH6_ADVERTISE	2
#define DH6_REQUEST	3
#define DH6_CONFIRM	4
#define DH6_RENEW	5
#define DH6_REBIND	6
#define DH6_REPLY	7
#define DH6_RELEASE	8
#define DH6_DECLINE	9
#define DH6_RECONFIGURE	10
#define DH6_INFORM_REQ	11
#define DH6_RELAY_FORW	12
#define DH6_RELAY_REPLY	13
#define DH6_LEASEQUERY	14
#define DH6_LQ_REPLY	15

static const struct tok dh6_msgtype_str[] = {
	{ DH6_SOLICIT,     "solicit"          },
	{ DH6_ADVERTISE,   "advertise"        },
	{ DH6_REQUEST,     "request"          },
	{ DH6_CONFIRM,     "confirm"          },
	{ DH6_RENEW,       "renew"            },
	{ DH6_REBIND,      "rebind"           },
	{ DH6_REPLY,       "reply"            },
	{ DH6_RELEASE,     "release"          },
	{ DH6_DECLINE,     "decline"          },
	{ DH6_RECONFIGURE, "reconfigure"      },
	{ DH6_INFORM_REQ,  "inf-req"          },
	{ DH6_RELAY_FORW,  "relay-fwd"        },
	{ DH6_RELAY_REPLY, "relay-reply"      },
	{ DH6_LEASEQUERY,  "leasequery"       },
	{ DH6_LQ_REPLY,    "leasequery-reply" },
	{ 0, NULL }
};

/* DHCP6 base packet format */
struct dhcp6 {
	union {
		nd_uint8_t msgtype;
		nd_uint32_t xid;
	} dh6_msgtypexid;
	/* options follow */
};
#define DH6_XIDMASK	0x00ffffff

/* DHCPv6 relay messages */
struct dhcp6_relay {
	nd_uint8_t dh6relay_msgtype;
	nd_uint8_t dh6relay_hcnt;
	nd_ipv6    dh6relay_linkaddr;	/* XXX: badly aligned */
	nd_ipv6    dh6relay_peeraddr;
	/* options follow */
};

/* options */
#define DH6OPT_CLIENTID	1
#define DH6OPT_SERVERID	2
#  define DUID_LLT  1 /* RFC8415 */
#  define DUID_EN   2 /* RFC8415 */
#  define DUID_LL   3 /* RFC8415 */
#  define DUID_UUID 4 /* RFC6355 */
#define DH6OPT_IA_NA 3
#define DH6OPT_IA_TA 4
#define DH6OPT_IA_ADDR 5
#define DH6OPT_ORO 6
#define DH6OPT_PREFERENCE 7
#  define DH6OPT_PREF_MAX 255
#define DH6OPT_ELAPSED_TIME 8
#define DH6OPT_RELAY_MSG 9
/*#define DH6OPT_SERVER_MSG 10 deprecated */
#define DH6OPT_AUTH 11
#  define DH6OPT_AUTHPROTO_DELAYED 2
#  define DH6OPT_AUTHPROTO_RECONFIG 3
#  define DH6OPT_AUTHALG_HMACMD5 1
#  define DH6OPT_AUTHRDM_MONOCOUNTER 0
#  define DH6OPT_AUTHRECONFIG_KEY 1
#  define DH6OPT_AUTHRECONFIG_HMACMD5 2
#define DH6OPT_UNICAST 12
#define DH6OPT_STATUS_CODE 13
#  define DH6OPT_STCODE_SUCCESS 0
#  define DH6OPT_STCODE_UNSPECFAIL 1
#  define DH6OPT_STCODE_NOADDRAVAIL 2
#  define DH6OPT_STCODE_NOBINDING 3
#  define DH6OPT_STCODE_NOTONLINK 4
#  define DH6OPT_STCODE_USEMULTICAST 5
#  define DH6OPT_STCODE_NOPREFIXAVAIL 6
#  define DH6OPT_STCODE_UNKNOWNQUERYTYPE 7
#  define DH6OPT_STCODE_MALFORMEDQUERY 8
#  define DH6OPT_STCODE_NOTCONFIGURED 9
#  define DH6OPT_STCODE_NOTALLOWED 10
#define DH6OPT_RAPID_COMMIT 14
#define DH6OPT_USER_CLASS 15
#define DH6OPT_VENDOR_CLASS 16
#define DH6OPT_VENDOR_OPTS 17
#define DH6OPT_INTERFACE_ID 18
#define DH6OPT_RECONF_MSG 19
#define DH6OPT_RECONF_ACCEPT 20
#define DH6OPT_SIP_SERVER_D 21
#define DH6OPT_SIP_SERVER_A 22
#define DH6OPT_DNS_SERVERS 23
#define DH6OPT_DOMAIN_LIST 24
#define DH6OPT_IA_PD 25
#define DH6OPT_IA_PD_PREFIX 26
#define DH6OPT_NIS_SERVERS 27
#define DH6OPT_NISP_SERVERS 28
#define DH6OPT_NIS_NAME 29
#define DH6OPT_NISP_NAME 30
#define DH6OPT_SNTP_SERVERS 31
#define DH6OPT_LIFETIME 32
#define DH6OPT_BCMCS_SERVER_D 33
#define DH6OPT_BCMCS_SERVER_A 34
#define DH6OPT_GEOCONF_CIVIC 36
#define DH6OPT_REMOTE_ID 37
#define DH6OPT_SUBSCRIBER_ID 38
#define DH6OPT_CLIENT_FQDN 39
#define DH6OPT_PANA_AGENT 40
#define DH6OPT_NEW_POSIX_TIMEZONE 41
#define DH6OPT_NEW_TZDB_TIMEZONE 42
#define DH6OPT_ERO 43
#define DH6OPT_LQ_QUERY 44
#define DH6OPT_CLIENT_DATA 45
#define DH6OPT_CLT_TIME 46
#define DH6OPT_LQ_RELAY_DATA 47
#define DH6OPT_LQ_CLIENT_LINK 48
#define DH6OPT_NTP_SERVER 56
#  define DH6OPT_NTP_SUBOPTION_SRV_ADDR 1
#  define DH6OPT_NTP_SUBOPTION_MC_ADDR 2
#  define DH6OPT_NTP_SUBOPTION_SRV_FQDN 3
#define DH6OPT_BOOTFILE_URL 59    /* RFC5970 */
#define DH6OPT_AFTR_NAME 64
#define DH6OPT_MUDURL 112
#define DH6OPT_SZTP_REDIRECT 136  /* RFC8572 */

static const struct tok dh6opt_str[] = {
	{ DH6OPT_CLIENTID,           "client-ID"            },
	{ DH6OPT_SERVERID,           "server-ID"            },
	{ DH6OPT_IA_NA,              "IA_NA"                },
	{ DH6OPT_IA_TA,              "IA_TA"                },
	{ DH6OPT_IA_ADDR,            "IA_ADDR"              },
	{ DH6OPT_ORO,                "option-request"       },
	{ DH6OPT_PREFERENCE,         "preference"           },
	{ DH6OPT_ELAPSED_TIME,       "elapsed-time"         },
	{ DH6OPT_RELAY_MSG,          "relay-message"        },
	{ DH6OPT_AUTH,               "authentication"       },
	{ DH6OPT_UNICAST,            "server-unicast"       },
	{ DH6OPT_STATUS_CODE,        "status-code"          },
	{ DH6OPT_RAPID_COMMIT,       "rapid-commit"         },
	{ DH6OPT_USER_CLASS,         "user-class"           },
	{ DH6OPT_VENDOR_CLASS,       "vendor-class"         },
	{ DH6OPT_VENDOR_OPTS,        "vendor-specific-info" },
	{ DH6OPT_INTERFACE_ID,       "interface-ID"         },
	{ DH6OPT_RECONF_MSG,         "reconfigure-message"  },
	{ DH6OPT_RECONF_ACCEPT,      "reconfigure-accept"   },
	{ DH6OPT_SIP_SERVER_D,       "SIP-servers-domain"   },
	{ DH6OPT_SIP_SERVER_A,       "SIP-servers-address"  },
	{ DH6OPT_DNS_SERVERS,        "DNS-server"           },
	{ DH6OPT_DOMAIN_LIST,        "DNS-search-list"      },
	{ DH6OPT_IA_PD,              "IA_PD"                },
	{ DH6OPT_IA_PD_PREFIX,       "IA_PD-prefix"         },
	{ DH6OPT_SNTP_SERVERS,       "SNTP-servers"         },
	{ DH6OPT_LIFETIME,           "lifetime"             },
	{ DH6OPT_NIS_SERVERS,        "NIS-server"           },
	{ DH6OPT_NISP_SERVERS,       "NIS+-server"          },
	{ DH6OPT_NIS_NAME,           "NIS-domain-name"      },
	{ DH6OPT_NISP_NAME,          "NIS+-domain-name"     },
	{ DH6OPT_BCMCS_SERVER_D,     "BCMCS-domain-name"    },
	{ DH6OPT_BCMCS_SERVER_A,     "BCMCS-server"         },
	{ DH6OPT_GEOCONF_CIVIC,      "Geoconf-Civic"        },
	{ DH6OPT_REMOTE_ID,          "Remote-ID"            },
	{ DH6OPT_SUBSCRIBER_ID,      "Subscriber-ID"        },
	{ DH6OPT_CLIENT_FQDN,        "Client-FQDN"          },
	{ DH6OPT_PANA_AGENT,         "PANA-agent"           },
	{ DH6OPT_NEW_POSIX_TIMEZONE, "POSIX-timezone"       },
	{ DH6OPT_NEW_TZDB_TIMEZONE,  "POSIX-tz-database"    },
	{ DH6OPT_ERO,                "Echo-request-option"  },
	{ DH6OPT_LQ_QUERY,           "Lease-query"          },
	{ DH6OPT_CLIENT_DATA,        "LQ-client-data"       },
	{ DH6OPT_CLT_TIME,           "Clt-time"             },
	{ DH6OPT_LQ_RELAY_DATA,      "LQ-relay-data"        },
	{ DH6OPT_LQ_CLIENT_LINK,     "LQ-client-link"       },
	{ DH6OPT_NTP_SERVER,         "NTP-server"           },
	{ DH6OPT_BOOTFILE_URL,       "Bootfile-URL"         },
	{ DH6OPT_AFTR_NAME,          "AFTR-Name"            },
	{ DH6OPT_MUDURL,             "MUD-URL"              },
	{ DH6OPT_SZTP_REDIRECT,      "SZTP-redirect"        },
	{ 0, NULL }
};

static const struct tok dh6opt_stcode_str[] = {
	{ DH6OPT_STCODE_SUCCESS,          "Success"          }, /* RFC3315 */
	{ DH6OPT_STCODE_UNSPECFAIL,       "UnspecFail"       }, /* RFC3315 */
	{ DH6OPT_STCODE_NOADDRAVAIL,      "NoAddrsAvail"     }, /* RFC3315 */
	{ DH6OPT_STCODE_NOBINDING,        "NoBinding"        }, /* RFC3315 */
	{ DH6OPT_STCODE_NOTONLINK,        "NotOnLink"        }, /* RFC3315 */
	{ DH6OPT_STCODE_USEMULTICAST,     "UseMulticast"     }, /* RFC3315 */
	{ DH6OPT_STCODE_NOPREFIXAVAIL,    "NoPrefixAvail"    }, /* RFC3633 */
	{ DH6OPT_STCODE_UNKNOWNQUERYTYPE, "UnknownQueryType" }, /* RFC5007 */
	{ DH6OPT_STCODE_MALFORMEDQUERY,   "MalformedQuery"   }, /* RFC5007 */
	{ DH6OPT_STCODE_NOTCONFIGURED,    "NotConfigured"    }, /* RFC5007 */
	{ DH6OPT_STCODE_NOTALLOWED,       "NotAllowed"       }, /* RFC5007 */
	{ 0, NULL }
};

struct dhcp6opt {
	nd_uint16_t dh6opt_type;
	nd_uint16_t dh6opt_len;
	/* type-dependent data follows */
};

static const char *
dhcp6stcode(const uint16_t code)
{
	return code > 255 ? "INVALID code" : tok2str(dh6opt_stcode_str, "code%u", code);
}

static void
dhcp6opt_print(netdissect_options *ndo,
               const u_char *cp, const u_char *ep)
{
	const struct dhcp6opt *dh6o;
	const u_char *tp;
	u_int i;
	uint16_t opttype;
	uint16_t optlen;
	uint8_t auth_proto;
	uint8_t auth_alg;
	uint8_t auth_rdm;
	u_int authinfolen, authrealmlen;
	u_int remain_len;  /* Length of remaining options */
	u_int label_len;   /* Label length */
	uint16_t subopt_code;
	uint16_t subopt_len;
	uint8_t dh6_reconf_type;
	uint8_t dh6_lq_query_type;
	u_int first_list_value;
	uint16_t remainder_len;

	if (cp == ep)
		return;
	while (cp < ep) {
		if (ep < cp + sizeof(*dh6o))
			goto trunc;
		dh6o = (const struct dhcp6opt *)cp;
		ND_TCHECK_SIZE(dh6o);
		optlen = GET_BE_U_2(dh6o->dh6opt_len);
		if (ep < cp + sizeof(*dh6o) + optlen)
			goto trunc;
		opttype = GET_BE_U_2(dh6o->dh6opt_type);
		ND_PRINT(" (%s", tok2str(dh6opt_str, "opt_%u", opttype));
		ND_TCHECK_LEN(cp + sizeof(*dh6o), optlen);
		switch (opttype) {
		case DH6OPT_CLIENTID:
		case DH6OPT_SERVERID:
			if (optlen < 2) {
				/*(*/
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			switch (GET_BE_U_2(tp)) {
			case DUID_LLT:
				if (optlen >= 2 + 6) {
					ND_PRINT(" hwaddr/time type %u time %u ",
					    GET_BE_U_2(tp + 2),
					    GET_BE_U_4(tp + 4));
					for (i = 8; i < optlen; i++)
						ND_PRINT("%02x",
							 GET_U_1(tp + i));
					/*(*/
					ND_PRINT(")");
				} else {
					/*(*/
					ND_PRINT(" ?)");
				}
				break;
			case DUID_EN:
				if (optlen >= 2 + 4) {
					ND_PRINT(" enterprise %u ", GET_BE_U_4(tp + 2));
					for (i = 2 + 4; i < optlen; i++)
						ND_PRINT("%02x",
							 GET_U_1(tp + i));
					/*(*/
					ND_PRINT(")");
				} else {
					/*(*/
					ND_PRINT(" ?)");
				}
				break;
			case DUID_LL:
				if (optlen >= 2 + 2) {
					ND_PRINT(" hwaddr type %u ",
					    GET_BE_U_2(tp + 2));
					for (i = 4; i < optlen; i++)
						ND_PRINT("%02x",
							 GET_U_1(tp + i));
					/*(*/
					ND_PRINT(")");
				} else {
					/*(*/
					ND_PRINT(" ?)");
				}
				break;
			case DUID_UUID:
				ND_PRINT(" uuid ");
				if (optlen == 2 + 16) {
					for (i = 2; i < optlen; i++)
						ND_PRINT("%02x",
							 GET_U_1(tp + i));
					/*(*/
					ND_PRINT(")");
				} else {
					/*(*/
					ND_PRINT(" ?)");
				}
				break;
			default:
				ND_PRINT(" type %u)", GET_BE_U_2(tp));
				break;
			}
			break;
		case DH6OPT_IA_ADDR:
			if (optlen < 24) {
				/*(*/
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			ND_PRINT(" %s", GET_IP6ADDR_STRING(tp));
			ND_PRINT(" pltime:%u vltime:%u",
			    GET_BE_U_4(tp + 16),
			    GET_BE_U_4(tp + 20));
			if (optlen > 24) {
				/* there are sub-options */
				dhcp6opt_print(ndo, tp + 24, tp + optlen);
			}
			ND_PRINT(")");
			break;
		case DH6OPT_ORO:
		case DH6OPT_ERO:
			if (optlen % 2) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			for (i = 0; i < optlen; i += 2) {
				ND_PRINT(" %s",
				    tok2str(dh6opt_str, "opt_%u", GET_BE_U_2(tp + i)));
			}
			ND_PRINT(")");
			break;
		case DH6OPT_PREFERENCE:
			if (optlen != 1) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			ND_PRINT(" %u)", GET_U_1(tp));
			break;
		case DH6OPT_ELAPSED_TIME:
			if (optlen != 2) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			ND_PRINT(" %u)", GET_BE_U_2(tp));
			break;
		case DH6OPT_RELAY_MSG:
		    {
			const u_char *snapend_save;

			ND_PRINT(" (");
			tp = (const u_char *)(dh6o + 1);
			/*
			 * Update the snapend to the end of the option before
			 * calling recursively dhcp6_print() for the nested
			 * packet. Other options may be present after the
			 * nested DHCPv6 packet. This prevents that, in
			 * dhcp6_print(), for the nested DHCPv6 packet, the
			 * remaining length < remaining caplen.
			 */
			snapend_save = ndo->ndo_snapend;
			ndo->ndo_snapend = ND_MIN(tp + optlen, ndo->ndo_snapend);
			dhcp6_print(ndo, tp, optlen);
			ndo->ndo_snapend = snapend_save;
			ND_PRINT(")");
			break;
		    }
		case DH6OPT_AUTH:
			if (optlen < 11) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			auth_proto = GET_U_1(tp);
			switch (auth_proto) {
			case DH6OPT_AUTHPROTO_DELAYED:
				ND_PRINT(" proto: delayed");
				break;
			case DH6OPT_AUTHPROTO_RECONFIG:
				ND_PRINT(" proto: reconfigure");
				break;
			default:
				ND_PRINT(" proto: %u", auth_proto);
				break;
			}
			tp++;
			auth_alg = GET_U_1(tp);
			switch (auth_alg) {
			case DH6OPT_AUTHALG_HMACMD5:
				/* XXX: may depend on the protocol */
				ND_PRINT(", alg: HMAC-MD5");
				break;
			default:
				ND_PRINT(", alg: %u", auth_alg);
				break;
			}
			tp++;
			auth_rdm = GET_U_1(tp);
			switch (auth_rdm) {
			case DH6OPT_AUTHRDM_MONOCOUNTER:
				ND_PRINT(", RDM: mono");
				break;
			default:
				ND_PRINT(", RDM: %u", auth_rdm);
				break;
			}
			tp++;
			ND_PRINT(", RD:");
			for (i = 0; i < 4; i++, tp += 2)
				ND_PRINT(" %04x", GET_BE_U_2(tp));

			/* protocol dependent part */
			authinfolen = optlen - 11;
			switch (auth_proto) {
			case DH6OPT_AUTHPROTO_DELAYED:
				if (authinfolen == 0)
					break;
				if (authinfolen < 20) {
					ND_PRINT(" ??");
					break;
				}
				authrealmlen = authinfolen - 20;
				if (authrealmlen > 0) {
					ND_PRINT(", realm: ");
				}
				for (i = 0; i < authrealmlen; i++, tp++)
					ND_PRINT("%02x", GET_U_1(tp));
				ND_PRINT(", key ID: %08x", GET_BE_U_4(tp));
				tp += 4;
				ND_PRINT(", HMAC-MD5:");
				for (i = 0; i < 4; i++, tp+= 4)
					ND_PRINT(" %08x", GET_BE_U_4(tp));
				break;
			case DH6OPT_AUTHPROTO_RECONFIG:
				if (authinfolen != 17) {
					ND_PRINT(" ??");
					break;
				}
				switch (GET_U_1(tp)) {
				case DH6OPT_AUTHRECONFIG_KEY:
					ND_PRINT(" reconfig-key");
					break;
				case DH6OPT_AUTHRECONFIG_HMACMD5:
					ND_PRINT(" type: HMAC-MD5");
					break;
				default:
					ND_PRINT(" type: ??");
					break;
				}
				tp++;
				ND_PRINT(" value:");
				for (i = 0; i < 4; i++, tp+= 4)
					ND_PRINT(" %08x", GET_BE_U_4(tp));
				break;
			default:
				ND_PRINT(" ??");
				break;
			}

			ND_PRINT(")");
			break;
		case DH6OPT_RAPID_COMMIT: /* nothing todo */
			ND_PRINT(")");
			break;
		case DH6OPT_INTERFACE_ID:
		case DH6OPT_SUBSCRIBER_ID:
			/*
			 * Since we cannot predict the encoding, print hex dump
			 * at most 10 characters.
			 */
			tp = (const u_char *)(dh6o + 1);
			ND_PRINT(" ");
			for (i = 0; i < optlen && i < 10; i++)
				ND_PRINT("%02x", GET_U_1(tp + i));
			ND_PRINT("...)");
			break;
		case DH6OPT_RECONF_MSG:
			if (optlen != 1) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			dh6_reconf_type = GET_U_1(tp);
			switch (dh6_reconf_type) {
			case DH6_RENEW:
				ND_PRINT(" for renew)");
				break;
			case DH6_INFORM_REQ:
				ND_PRINT(" for inf-req)");
				break;
			default:
				ND_PRINT(" for ?\?\?(%02x))", dh6_reconf_type);
				break;
			}
			break;
		case DH6OPT_RECONF_ACCEPT: /* nothing todo */
			ND_PRINT(")");
			break;
		case DH6OPT_SIP_SERVER_A:
		case DH6OPT_DNS_SERVERS:
		case DH6OPT_SNTP_SERVERS:
		case DH6OPT_NIS_SERVERS:
		case DH6OPT_NISP_SERVERS:
		case DH6OPT_BCMCS_SERVER_A:
		case DH6OPT_PANA_AGENT:
		case DH6OPT_LQ_CLIENT_LINK:
			if (optlen % 16) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			for (i = 0; i < optlen; i += 16)
				ND_PRINT(" %s", GET_IP6ADDR_STRING(tp + i));
			ND_PRINT(")");
			break;
		case DH6OPT_SIP_SERVER_D:
		case DH6OPT_DOMAIN_LIST:
			tp = (const u_char *)(dh6o + 1);
			while (tp < cp + sizeof(*dh6o) + optlen) {
				ND_PRINT(" ");
				if ((tp = fqdn_print(ndo, tp, cp + sizeof(*dh6o) + optlen)) == NULL)
					goto trunc;
			}
			ND_PRINT(")");
			break;
		case DH6OPT_STATUS_CODE:
			if (optlen < 2) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			ND_PRINT(" %s)", dhcp6stcode(GET_BE_U_2(tp)));
			break;
		case DH6OPT_IA_NA:
		case DH6OPT_IA_PD:
			if (optlen < 12) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			ND_PRINT(" IAID:%u T1:%u T2:%u",
			    GET_BE_U_4(tp),
			    GET_BE_U_4(tp + 4),
			    GET_BE_U_4(tp + 8));
			if (optlen > 12) {
				/* there are sub-options */
				dhcp6opt_print(ndo, tp + 12, tp + optlen);
			}
			ND_PRINT(")");
			break;
		case DH6OPT_IA_TA:
			if (optlen < 4) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			ND_PRINT(" IAID:%u", GET_BE_U_4(tp));
			if (optlen > 4) {
				/* there are sub-options */
				dhcp6opt_print(ndo, tp + 4, tp + optlen);
			}
			ND_PRINT(")");
			break;
		case DH6OPT_IA_PD_PREFIX:
			if (optlen < 25) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			ND_PRINT(" %s/%u", GET_IP6ADDR_STRING(tp + 9),
				 GET_U_1(tp + 8));
			ND_PRINT(" pltime:%u vltime:%u",
			    GET_BE_U_4(tp),
			    GET_BE_U_4(tp + 4));
			if (optlen > 25) {
				/* there are sub-options */
				dhcp6opt_print(ndo, tp + 25, tp + optlen);
			}
			ND_PRINT(")");
			break;
		case DH6OPT_LIFETIME:
		case DH6OPT_CLT_TIME:
			if (optlen != 4) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			ND_PRINT(" %u)", GET_BE_U_4(tp));
			break;
		case DH6OPT_REMOTE_ID:
			if (optlen < 4) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			ND_PRINT(" %u ", GET_BE_U_4(tp));
			/*
			 * Print hex dump first 10 characters.
			 */
			for (i = 4; i < optlen && i < 14; i++)
				ND_PRINT("%02x", GET_U_1(tp + i));
			ND_PRINT("...)");
			break;
		case DH6OPT_LQ_QUERY:
			if (optlen < 17) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			dh6_lq_query_type = GET_U_1(tp);
			switch (dh6_lq_query_type) {
			case 1:
				ND_PRINT(" by-address");
				break;
			case 2:
				ND_PRINT(" by-clientID");
				break;
			default:
				ND_PRINT(" type_%u", dh6_lq_query_type);
				break;
			}
			ND_PRINT(" %s", GET_IP6ADDR_STRING(tp + 1));
			if (optlen > 17) {
				/* there are query-options */
				dhcp6opt_print(ndo, tp + 17, tp + optlen);
			}
			ND_PRINT(")");
			break;
		case DH6OPT_CLIENT_DATA:
			tp = (const u_char *)(dh6o + 1);
			if (optlen > 0) {
				/* there are encapsulated options */
				dhcp6opt_print(ndo, tp, tp + optlen);
			}
			ND_PRINT(")");
			break;
		case DH6OPT_LQ_RELAY_DATA:
			if (optlen < 16) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			ND_PRINT(" %s ", GET_IP6ADDR_STRING(tp));
			/*
			 * Print hex dump first 10 characters.
			 */
			for (i = 16; i < optlen && i < 26; i++)
				ND_PRINT("%02x", GET_U_1(tp + i));
			ND_PRINT("...)");
			break;
		case DH6OPT_NTP_SERVER:
			if (optlen < 4) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			while (tp < cp + sizeof(*dh6o) + optlen - 4) {
				subopt_code = GET_BE_U_2(tp);
				tp += 2;
				subopt_len = GET_BE_U_2(tp);
				tp += 2;
				if (tp + subopt_len > cp + sizeof(*dh6o) + optlen)
					goto trunc;
				ND_PRINT(" subopt:%u", subopt_code);
				switch (subopt_code) {
				case DH6OPT_NTP_SUBOPTION_SRV_ADDR:
				case DH6OPT_NTP_SUBOPTION_MC_ADDR:
					if (subopt_len != 16) {
						ND_PRINT(" ?");
						break;
					}
					ND_PRINT(" %s", GET_IP6ADDR_STRING(tp));
					break;
				case DH6OPT_NTP_SUBOPTION_SRV_FQDN:
					ND_PRINT(" ");
					if (fqdn_print(ndo, tp, tp + subopt_len) == NULL)
						goto trunc;
					break;
				default:
					ND_PRINT(" ?");
					break;
				}
				tp += subopt_len;
			}
			ND_PRINT(")");
			break;
		case DH6OPT_AFTR_NAME:
			if (optlen < 3) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			remain_len = optlen;
			ND_PRINT(" ");
			/* Encoding is described in section 3.1 of RFC 1035 */
			while (remain_len && GET_U_1(tp)) {
				label_len = GET_U_1(tp);
				tp++;
				if (label_len < remain_len - 1) {
					nd_printjnp(ndo, tp, label_len);
					tp += label_len;
					remain_len -= (label_len + 1);
					if(GET_U_1(tp)) ND_PRINT(".");
				} else {
					ND_PRINT(" ?");
					break;
				}
			}
			ND_PRINT(")");
			break;
		case DH6OPT_NEW_POSIX_TIMEZONE: /* all three of these options */
		case DH6OPT_NEW_TZDB_TIMEZONE:	/* are encoded similarly */
		case DH6OPT_MUDURL:		/* although GMT might not work */
		        if (optlen < 5) {
				ND_PRINT(" ?)");
				break;
			}
			tp = (const u_char *)(dh6o + 1);
			ND_PRINT(" ");
			nd_printjnp(ndo, tp, optlen);
			ND_PRINT(")");
			break;

		case DH6OPT_BOOTFILE_URL:
			tp = (const u_char *)(dh6o + 1);
			ND_PRINT(" ");
			nd_printjn(ndo, tp, optlen);
			ND_PRINT(")");
			break;

		case DH6OPT_SZTP_REDIRECT:
		case DH6OPT_USER_CLASS:
			ND_PRINT(" ");
			tp = (const u_char *)(dh6o + 1);
			first_list_value = TRUE;
			remainder_len = optlen;
			while (remainder_len >= 2) {
				if (first_list_value == FALSE) {
					ND_PRINT(",");
				}
				first_list_value = FALSE;
				subopt_len = GET_BE_U_2(tp);
				if (subopt_len > remainder_len-2) {
					break;
				}
				tp += 2;
				nd_printjn(ndo, tp, subopt_len);
				tp += subopt_len;
				remainder_len -= (subopt_len+2);
			}
			if (remainder_len != 0 ) {
				ND_PRINT(" ?");
			}
			ND_PRINT(")");
			break;

		default:
			ND_PRINT(")");
			break;
		}

		cp += sizeof(*dh6o) + optlen;
	}
	return;

trunc:
	nd_print_trunc(ndo);
}

/*
 * Print dhcp6 packets
 */
void
dhcp6_print(netdissect_options *ndo,
            const u_char *cp, u_int length)
{
	const struct dhcp6 *dh6;
	const struct dhcp6_relay *dh6relay;
	uint8_t msgtype;
	const u_char *ep;
	const u_char *extp;
	const char *name;

	ndo->ndo_protocol = "dhcp6";
	ND_PRINT("dhcp6");

	ep = ndo->ndo_snapend;
	if (cp + length < ep)
		ep = cp + length;

	dh6 = (const struct dhcp6 *)cp;
	dh6relay = (const struct dhcp6_relay *)cp;
	ND_TCHECK_4(dh6->dh6_msgtypexid.xid);
	msgtype = GET_U_1(dh6->dh6_msgtypexid.msgtype);
	name = tok2str(dh6_msgtype_str, "msgtype-%u", msgtype);

	if (!ndo->ndo_vflag) {
		ND_PRINT(" %s", name);
		return;
	}

	/* XXX relay agent messages have to be handled differently */

	ND_PRINT(" %s (", name);	/*)*/
	if (msgtype != DH6_RELAY_FORW && msgtype != DH6_RELAY_REPLY) {
		ND_PRINT("xid=%x",
			 GET_BE_U_4(dh6->dh6_msgtypexid.xid) & DH6_XIDMASK);
		extp = (const u_char *)(dh6 + 1);
		dhcp6opt_print(ndo, extp, ep);
	} else {		/* relay messages */
		ND_PRINT("linkaddr=%s", GET_IP6ADDR_STRING(dh6relay->dh6relay_linkaddr));

		ND_PRINT(" peeraddr=%s", GET_IP6ADDR_STRING(dh6relay->dh6relay_peeraddr));

		dhcp6opt_print(ndo, (const u_char *)(dh6relay + 1), ep);
	}
	/*(*/
	ND_PRINT(")");
	return;

trunc:
	nd_print_trunc(ndo);
}
