/*
 * Copyright (c) 2007-2011 Grégoire Henry, Juliusz Chroboczek
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

/* \summary: Babel Routing Protocol printer */
/* Specifications:
 *
 * RFC 6126
 * RFC 7298
 * RFC 7557
 * draft-ietf-babel-rfc6126bis-17
 * draft-ietf-babel-hmac-10
 * draft-ietf-babel-source-specific-0
 */

#include <config.h>

#include "netdissect-stdinc.h"

#include <stdio.h>
#include <string.h>

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

static void babel_print_v2(netdissect_options *, const u_char *cp, u_int length);

void
babel_print(netdissect_options *ndo,
            const u_char *cp, u_int length)
{
    ndo->ndo_protocol = "babel";
    ND_PRINT("babel");

    ND_TCHECK_4(cp);

    if(GET_U_1(cp) != 42) {
        ND_PRINT(" invalid header");
        return;
    } else {
        ND_PRINT(" %u", GET_U_1(cp + 1));
    }

    switch(GET_U_1(cp + 1)) {
    case 2:
        babel_print_v2(ndo, cp, length);
        break;
    default:
        ND_PRINT(" unknown version");
        break;
    }

    return;

 trunc:
    nd_print_trunc(ndo);
}

/* TLVs */
#define MESSAGE_PAD1 0
#define MESSAGE_PADN 1
#define MESSAGE_ACK_REQ 2
#define MESSAGE_ACK 3
#define MESSAGE_HELLO 4
#define MESSAGE_IHU 5
#define MESSAGE_ROUTER_ID 6
#define MESSAGE_NH 7
#define MESSAGE_UPDATE 8
#define MESSAGE_ROUTE_REQUEST 9
#define MESSAGE_SEQNO_REQUEST 10
#define MESSAGE_TSPC 11
#define MESSAGE_HMAC 12
#define MESSAGE_UPDATE_SRC_SPECIFIC 13 /* last appearance in draft-boutier-babel-source-specific-01 */
#define MESSAGE_REQUEST_SRC_SPECIFIC 14 /* idem */
#define MESSAGE_MH_REQUEST_SRC_SPECIFIC 15 /* idem */
#define MESSAGE_MAC 16
#define MESSAGE_PC 17
#define MESSAGE_CHALLENGE_REQUEST 18
#define MESSAGE_CHALLENGE_REPLY 19

/* sub-TLVs */
#define MESSAGE_SUB_PAD1 0
#define MESSAGE_SUB_PADN 1
#define MESSAGE_SUB_DIVERSITY 2
#define MESSAGE_SUB_TIMESTAMP 3

/* "Mandatory" bit in sub-TLV types */
#define MANDATORY_MASK 0x80

/* Flags for the Hello TLV */
#define UNICAST_MASK 0x8000

/* Diversity sub-TLV channel codes */
static const struct tok diversity_str[] = {
    { 0,   "reserved" },
    { 255, "all"      },
    { 0, NULL }
};

static const char *
format_id(netdissect_options *ndo, const u_char *id)
{
    static char buf[25];
    snprintf(buf, 25, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
             GET_U_1(id), GET_U_1(id + 1), GET_U_1(id + 2),
             GET_U_1(id + 3), GET_U_1(id + 4), GET_U_1(id + 5),
             GET_U_1(id + 6), GET_U_1(id + 7));
    buf[24] = '\0';
    return buf;
}

static const unsigned char v4prefix[16] =
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0 };

static const char *
format_prefix(netdissect_options *ndo, const u_char *prefix, unsigned char plen)
{
    static char buf[50];

    /*
     * prefix points to a buffer on the stack into which the prefix has
     * been placed, so we can't use GET_IPADDR_STRING() or
     * GET_IP6ADDR_STRING() on it.
     */
    if(plen >= 96 && memcmp(prefix, v4prefix, 12) == 0)
        snprintf(buf, 50, "%s/%u", ipaddr_string(ndo, prefix + 12), plen - 96);
    else
        snprintf(buf, 50, "%s/%u", ip6addr_string(ndo, prefix), plen);
    buf[49] = '\0';
    return buf;
}

static const char *
format_address(netdissect_options *ndo, const u_char *prefix)
{
    /*
     * prefix points to a buffer on the stack into which the prefix has
     * been placed, so we can't use GET_IPADDR_STRING() or
     * GET_IP6ADDR_STRING() on it.
     */
    if(memcmp(prefix, v4prefix, 12) == 0)
        return ipaddr_string(ndo, prefix + 12);
    else
        return ip6addr_string(ndo, prefix);
}

static const char *
format_interval(const uint16_t i)
{
    static char buf[sizeof("000.00s")];

    if (i == 0)
        return "0.0s (bogus)";
    snprintf(buf, sizeof(buf), "%u.%02us", i / 100, i % 100);
    return buf;
}

static const char *
format_interval_update(const uint16_t i)
{
    return i == 0xFFFF ? "infinity" : format_interval(i);
}

static const char *
format_timestamp(const uint32_t i)
{
    static char buf[sizeof("0000.000000s")];
    snprintf(buf, sizeof(buf), "%u.%06us", i / 1000000, i % 1000000);
    return buf;
}

/* Return number of octets consumed from the input buffer (not the prefix length
 * in bytes), or -1 for encoding error. */
static int
network_prefix(int ae, int plen, unsigned int omitted,
               const unsigned char *p, const unsigned char *dp,
               unsigned int len, unsigned char *p_r)
{
    unsigned pb;
    unsigned char prefix[16];
    int consumed = 0;

    if(plen >= 0)
        pb = (plen + 7) / 8;
    else if(ae == 1)
        pb = 4;
    else
        pb = 16;

    if(pb > 16)
        return -1;

    memset(prefix, 0, 16);

    switch(ae) {
    case 0: break;
    case 1:
        if(omitted > 4 || pb > 4 || (pb > omitted && len < pb - omitted))
            return -1;
        memcpy(prefix, v4prefix, 12);
        if(omitted) {
            if (dp == NULL) return -1;
            memcpy(prefix, dp, 12 + omitted);
        }
        if(pb > omitted) {
            memcpy(prefix + 12 + omitted, p, pb - omitted);
            consumed = pb - omitted;
        }
        break;
    case 2:
        if(omitted > 16 || (pb > omitted && len < pb - omitted))
            return -1;
        if(omitted) {
            if (dp == NULL) return -1;
            memcpy(prefix, dp, omitted);
        }
        if(pb > omitted) {
            memcpy(prefix + omitted, p, pb - omitted);
            consumed = pb - omitted;
        }
        break;
    case 3:
        if(pb > 8 && len < pb - 8) return -1;
        prefix[0] = 0xfe;
        prefix[1] = 0x80;
        if(pb > 8) {
            memcpy(prefix + 8, p, pb - 8);
            consumed = pb - 8;
        }
        break;
    default:
        return -1;
    }

    memcpy(p_r, prefix, 16);
    return consumed;
}

static int
network_address(int ae, const unsigned char *a, unsigned int len,
                unsigned char *a_r)
{
    return network_prefix(ae, -1, 0, a, NULL, len, a_r);
}

/*
 * Sub-TLVs consume the "extra data" of Babel TLVs (see Section 4.3 of RFC6126),
 * their encoding is similar to the encoding of TLVs, but the type namespace is
 * different:
 *
 * o Type 0 stands for Pad1 sub-TLV with the same encoding as the Pad1 TLV.
 * o Type 1 stands for PadN sub-TLV with the same encoding as the PadN TLV.
 * o Type 2 stands for Diversity sub-TLV, which propagates diversity routing
 *   data. Its body is a variable-length sequence of 8-bit unsigned integers,
 *   each representing per-hop number of interfering radio channel for the
 *   prefix. Channel 0 is invalid and must not be used in the sub-TLV, channel
 *   255 interferes with any other channel.
 * o Type 3 stands for Timestamp sub-TLV, used to compute RTT between
 *   neighbours. In the case of a Hello TLV, the body stores a 32-bits
 *   timestamp, while in the case of a IHU TLV, two 32-bits timestamps are
 *   stored.
 *
 * Sub-TLV types 0 and 1 are valid for any TLV type, whether sub-TLV type 2 is
 * only valid for TLV type 8 (Update). Note that within an Update TLV a missing
 * Diversity sub-TLV is not the same as a Diversity sub-TLV with an empty body.
 * The former would mean a lack of any claims about the interference, and the
 * latter would state that interference is definitely absent.
 * A type 3 sub-TLV is valid both for Hello and IHU TLVs, though the exact
 * semantic of the sub-TLV is different in each case.
 */
static void
subtlvs_print(netdissect_options *ndo,
              const u_char *cp, const u_char *ep, const uint8_t tlv_type)
{
    uint8_t subtype, sublen;
    const char *sep;
    uint32_t t1, t2;

    while (cp < ep) {
        subtype = GET_U_1(cp);
        cp++;
        if(subtype == MESSAGE_SUB_PAD1) {
            ND_PRINT(" sub-pad1");
            continue;
        }
        if ((MANDATORY_MASK & subtype) != 0)
            ND_PRINT(" (M)");
        if(cp == ep)
            goto invalid;
        sublen = GET_U_1(cp);
        cp++;
        if(cp + sublen > ep)
            goto invalid;

        switch(subtype) {
        case MESSAGE_SUB_PADN:
            ND_PRINT(" sub-padn");
            cp += sublen;
            break;
        case MESSAGE_SUB_DIVERSITY:
            ND_PRINT(" sub-diversity");
            if (sublen == 0) {
                ND_PRINT(" empty");
                break;
            }
            sep = " ";
            while (sublen) {
                ND_PRINT("%s%s", sep,
                         tok2str(diversity_str, "%u", GET_U_1(cp)));
                cp++;
                sep = "-";
                sublen--;
            }
            if(tlv_type != MESSAGE_UPDATE &&
               tlv_type != MESSAGE_UPDATE_SRC_SPECIFIC)
                ND_PRINT(" (bogus)");
            break;
        case MESSAGE_SUB_TIMESTAMP:
            ND_PRINT(" sub-timestamp");
            if(tlv_type == MESSAGE_HELLO) {
                if(sublen < 4)
                    goto invalid;
                t1 = GET_BE_U_4(cp);
                ND_PRINT(" %s", format_timestamp(t1));
            } else if(tlv_type == MESSAGE_IHU) {
                if(sublen < 8)
                    goto invalid;
                t1 = GET_BE_U_4(cp);
                ND_PRINT(" %s", format_timestamp(t1));
                t2 = GET_BE_U_4(cp + 4);
                ND_PRINT("|%s", format_timestamp(t2));
            } else
                ND_PRINT(" (bogus)");
            cp += sublen;
            break;
        default:
            ND_PRINT(" sub-unknown-0x%02x", subtype);
            cp += sublen;
        } /* switch */
    } /* while */
    return;

 invalid:
    nd_print_invalid(ndo);
}

#define ICHECK(i, l) \
	if ((i) + (l) > tlvs_length || (i) + (l) > packet_length_remaining) \
	    goto invalid;

static int
babel_print_v2_tlvs(netdissect_options *ndo,
                    const u_char *cp, u_int tlvs_length,
                    u_int packet_length_remaining)
{
    u_int i;
    u_char v4_prefix[16] =
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0 };
    u_char v6_prefix[16] = {0};

    i = 0;
    while(i < tlvs_length) {
        const u_char *message;
        uint8_t type;
        u_int len;

        message = cp + i;

        ICHECK(i, 1);
        if((type = GET_U_1(message)) == MESSAGE_PAD1) {
            ND_PRINT(ndo->ndo_vflag ? "\n\tPad 1" : " pad1");
            i += 1;
            continue;
        }

        ICHECK(i, 2);
        ND_TCHECK_2(message);
        len = GET_U_1(message + 1);

        ICHECK(i, 2 + len);
        ND_TCHECK_LEN(message, 2 + len);

        switch(type) {
        case MESSAGE_PADN: {
            if (!ndo->ndo_vflag)
                ND_PRINT(" padN");
            else
                ND_PRINT("\n\tPad %u", len + 2);
        }
            break;

        case MESSAGE_ACK_REQ: {
            u_short nonce, interval;
            if (!ndo->ndo_vflag)
                ND_PRINT(" ack-req");
            else {
                ND_PRINT("\n\tAcknowledgment Request ");
                if(len < 6) goto invalid;
                nonce = GET_BE_U_2(message + 4);
                interval = GET_BE_U_2(message + 6);
                ND_PRINT("%04x %s", nonce, format_interval(interval));
            }
        }
            break;

        case MESSAGE_ACK: {
            u_short nonce;
            if (!ndo->ndo_vflag)
                ND_PRINT(" ack");
            else {
                ND_PRINT("\n\tAcknowledgment ");
                if(len < 2) goto invalid;
                nonce = GET_BE_U_2(message + 2);
                ND_PRINT("%04x", nonce);
            }
        }
            break;

        case MESSAGE_HELLO:  {
            u_short seqno, interval, unicast;
            if (!ndo->ndo_vflag)
                ND_PRINT(" hello");
            else {
                ND_PRINT("\n\tHello ");
                if(len < 6) goto invalid;
                unicast = (GET_BE_U_2(message + 2) & UNICAST_MASK);
                seqno = GET_BE_U_2(message + 4);
                interval = GET_BE_U_2(message + 6);
                if(unicast)
                     ND_PRINT("(Unicast) ");
                ND_PRINT("seqno %u ", seqno);
                if(interval!=0)
                    ND_PRINT("interval %s", format_interval(interval));
                else
                    ND_PRINT("unscheduled");
                /* Extra data. */
                if(len > 6)
                    subtlvs_print(ndo, message + 8, message + 2 + len, type);
            }
        }
            break;

        case MESSAGE_IHU: {
            unsigned short rxcost, interval;
            if (!ndo->ndo_vflag)
                ND_PRINT(" ihu");
            else {
                u_char address[16];
                u_char ae;
                int rc;
                ND_PRINT("\n\tIHU ");
                if(len < 6) goto invalid;
                rxcost = GET_BE_U_2(message + 4);
                interval = GET_BE_U_2(message + 6);
                ae = GET_U_1(message + 2);
                rc = network_address(ae, message + 8,
                                     len - 6, address);
                if(rc < 0) { nd_print_trunc(ndo); break; }
                ND_PRINT("%s rxcost %u interval %s",
                       ae == 0 ? "any" : format_address(ndo, address),
                       rxcost, format_interval(interval));
                /* Extra data. */
                if((u_int)rc < len - 6)
                    subtlvs_print(ndo, message + 8 + rc, message + 2 + len,
                                  type);
            }
        }
            break;

        case MESSAGE_ROUTER_ID: {
            if (!ndo->ndo_vflag)
                ND_PRINT(" router-id");
            else {
                ND_PRINT("\n\tRouter Id");
                if(len < 10) goto invalid;
                ND_PRINT(" %s", format_id(ndo, message + 4));
            }
        }
            break;

        case MESSAGE_NH: {
            if (!ndo->ndo_vflag)
                ND_PRINT(" nh");
            else {
                int rc;
                u_char ae;
                u_char nh[16];
                ND_PRINT("\n\tNext Hop");
                if(len < 2) goto invalid;
                ae = GET_U_1(message + 2);
                rc = network_address(ae, message + 4,
                                     len - 2, nh);
                if(rc < 0) goto invalid;
                ND_PRINT(" %s", ae == 0 ? "invalid AE 0" : format_address(ndo, nh));
            }
        }
            break;

        case MESSAGE_UPDATE: {
            if (!ndo->ndo_vflag) {
                ND_PRINT(" update");
                if(len < 10)
                    goto invalid;
                else
                    ND_PRINT("%s%s%s",
                           (GET_U_1(message + 3) & 0x80) ? "/prefix": "",
                           (GET_U_1(message + 3) & 0x40) ? "/id" : "",
                           (GET_U_1(message + 3) & 0x3f) ? "/unknown" : "");
            } else {
                u_short interval, seqno, metric;
                u_char ae, plen;
                int rc;
                u_char prefix[16];
                ND_PRINT("\n\tUpdate");
                if(len < 10) goto invalid;
                ae = GET_U_1(message + 2);
                plen = GET_U_1(message + 4) + (GET_U_1(message + 2) == 1 ? 96 : 0);
                rc = network_prefix(ae,
                                    GET_U_1(message + 4),
                                    GET_U_1(message + 5),
                                    message + 12,
                                    GET_U_1(message + 2) == 1 ? v4_prefix : v6_prefix,
                                    len - 10, prefix);
                if(rc < 0) goto invalid;
                interval = GET_BE_U_2(message + 6);
                seqno = GET_BE_U_2(message + 8);
                metric = GET_BE_U_2(message + 10);
                ND_PRINT("%s%s%s %s metric %u seqno %u interval %s",
                       (GET_U_1(message + 3) & 0x80) ? "/prefix": "",
                       (GET_U_1(message + 3) & 0x40) ? "/id" : "",
                       (GET_U_1(message + 3) & 0x3f) ? "/unknown" : "",
                       ae == 0 ? "any" : format_prefix(ndo, prefix, plen),
                       metric, seqno, format_interval_update(interval));
                if(GET_U_1(message + 3) & 0x80) {
                    if(GET_U_1(message + 2) == 1)
                        memcpy(v4_prefix, prefix, 16);
                    else
                        memcpy(v6_prefix, prefix, 16);
                }
                /* extra data? */
                if((u_int)rc < len - 10)
                    subtlvs_print(ndo, message + 12 + rc, message + 2 + len, type);
            }
        }
            break;

        case MESSAGE_ROUTE_REQUEST: {
            if (!ndo->ndo_vflag)
                ND_PRINT(" route-request");
            else {
                int rc;
                u_char prefix[16], ae, plen;
                ND_PRINT("\n\tRoute Request ");
                if(len < 2) goto invalid;
                ae = GET_U_1(message + 2);
                plen = GET_U_1(message + 3) + (GET_U_1(message + 2) == 1 ? 96 : 0);
                rc = network_prefix(ae,
                                    GET_U_1(message + 3), 0,
                                    message + 4, NULL, len - 2, prefix);
                if(rc < 0) goto invalid;
                ND_PRINT("for %s",
                       ae == 0 ? "any" : format_prefix(ndo, prefix, plen));
            }
        }
            break;

        case MESSAGE_SEQNO_REQUEST : {
            if (!ndo->ndo_vflag)
                ND_PRINT(" seqno-request");
            else {
                int rc;
                u_short seqno;
                u_char prefix[16], ae, plen;
                ND_PRINT("\n\tSeqno Request ");
                if(len < 14) goto invalid;
                ae = GET_U_1(message + 2);
                seqno = GET_BE_U_2(message + 4);
                rc = network_prefix(ae,
                                    GET_U_1(message + 3), 0,
                                    message + 16, NULL, len - 14, prefix);
                if(rc < 0) goto invalid;
                plen = GET_U_1(message + 3) + (GET_U_1(message + 2) == 1 ? 96 : 0);
                ND_PRINT("(%u hops) for %s seqno %u id %s",
                       GET_U_1(message + 6),
                       ae == 0 ? "invalid AE 0" : format_prefix(ndo, prefix, plen),
                       seqno, format_id(ndo, message + 8));
            }
        }
            break;
        case MESSAGE_TSPC :
            if (!ndo->ndo_vflag)
                ND_PRINT(" tspc");
            else {
                ND_PRINT("\n\tTS/PC ");
                if(len < 6) goto invalid;
                ND_PRINT("timestamp %u packetcounter %u",
                          GET_BE_U_4(message + 4),
                          GET_BE_U_2(message + 2));
            }
            break;
        case MESSAGE_HMAC : {
            if (!ndo->ndo_vflag)
                ND_PRINT(" hmac");
            else {
                unsigned j;
                ND_PRINT("\n\tHMAC ");
                if(len < 18) goto invalid;
                ND_PRINT("key-id %u digest-%u ", GET_BE_U_2(message + 2),
                         len - 2);
                for (j = 0; j < len - 2; j++)
                    ND_PRINT("%02X", GET_U_1(message + j + 4));
            }
        }
            break;

        case MESSAGE_UPDATE_SRC_SPECIFIC : {
            if(!ndo->ndo_vflag) {
                ND_PRINT(" ss-update");
            } else {
                u_char prefix[16], src_prefix[16];
                u_short interval, seqno, metric;
                u_char ae, plen, src_plen, omitted;
                int rc;
                int parsed_len = 10;
                ND_PRINT("\n\tSS-Update");
                if(len < 10) goto invalid;
                ae = GET_U_1(message + 2);
                src_plen = GET_U_1(message + 3);
                plen = GET_U_1(message + 4);
                omitted = GET_U_1(message + 5);
                interval = GET_BE_U_2(message + 6);
                seqno = GET_BE_U_2(message + 8);
                metric = GET_BE_U_2(message + 10);
                rc = network_prefix(ae, plen, omitted, message + 2 + parsed_len,
                                    ae == 1 ? v4_prefix : v6_prefix,
                                    len - parsed_len, prefix);
                if(rc < 0) goto invalid;
                if(ae == 1)
                    plen += 96;
                parsed_len += rc;
                rc = network_prefix(ae, src_plen, 0, message + 2 + parsed_len,
                                    NULL, len - parsed_len, src_prefix);
                if(rc < 0) goto invalid;
                if(ae == 1)
                    src_plen += 96;
                parsed_len += rc;

                ND_PRINT(" %s from", format_prefix(ndo, prefix, plen));
                ND_PRINT(" %s metric %u seqno %u interval %s",
                          format_prefix(ndo, src_prefix, src_plen),
                          metric, seqno, format_interval_update(interval));
                /* extra data? */
                if((u_int)parsed_len < len)
                    subtlvs_print(ndo, message + 2 + parsed_len,
                                  message + 2 + len, type);
            }
        }
            break;

        case MESSAGE_REQUEST_SRC_SPECIFIC : {
            if(!ndo->ndo_vflag)
                ND_PRINT(" ss-request");
            else {
                int rc, parsed_len = 3;
                u_char ae, plen, src_plen, prefix[16], src_prefix[16];
                ND_PRINT("\n\tSS-Request ");
                if(len < 3) goto invalid;
                ae = GET_U_1(message + 2);
                plen = GET_U_1(message + 3);
                src_plen = GET_U_1(message + 4);
                rc = network_prefix(ae, plen, 0, message + 2 + parsed_len,
                                    NULL, len - parsed_len, prefix);
                if(rc < 0) goto invalid;
                if(ae == 1)
                    plen += 96;
                parsed_len += rc;
                rc = network_prefix(ae, src_plen, 0, message + 2 + parsed_len,
                                    NULL, len - parsed_len, src_prefix);
                if(rc < 0) goto invalid;
                if(ae == 1)
                    src_plen += 96;
                parsed_len += rc;
                if(ae == 0) {
                    ND_PRINT("for any");
                } else {
                    ND_PRINT("for (%s, ", format_prefix(ndo, prefix, plen));
                    ND_PRINT("%s)", format_prefix(ndo, src_prefix, src_plen));
                }
            }
        }
            break;

        case MESSAGE_MH_REQUEST_SRC_SPECIFIC : {
            if(!ndo->ndo_vflag)
                ND_PRINT(" ss-mh-request");
            else {
                int rc, parsed_len = 14;
                u_short seqno;
                u_char ae, plen, src_plen, prefix[16], src_prefix[16], hopc;
                const u_char *router_id = NULL;
                ND_PRINT("\n\tSS-MH-Request ");
                if(len < 14) goto invalid;
                ae = GET_U_1(message + 2);
                plen = GET_U_1(message + 3);
                seqno = GET_BE_U_2(message + 4);
                hopc = GET_U_1(message + 6);
                src_plen = GET_U_1(message + 7);
                router_id = message + 8;
                rc = network_prefix(ae, plen, 0, message + 2 + parsed_len,
                                    NULL, len - parsed_len, prefix);
                if(rc < 0) goto invalid;
                if(ae == 1)
                    plen += 96;
                parsed_len += rc;
                rc = network_prefix(ae, src_plen, 0, message + 2 + parsed_len,
                                    NULL, len - parsed_len, src_prefix);
                if(rc < 0) goto invalid;
                if(ae == 1)
                    src_plen += 96;
                ND_PRINT("(%u hops) for (%s, ",
                          hopc, format_prefix(ndo, prefix, plen));
                ND_PRINT("%s) seqno %u id %s",
                          format_prefix(ndo, src_prefix, src_plen),
                          seqno, format_id(ndo, router_id));
            }
        }
            break;

        case MESSAGE_MAC: {
            if (!ndo->ndo_vflag)
                ND_PRINT(" mac");
            else {
                ND_PRINT("\n\tMAC ");
                ND_PRINT("len %u", len);
            }
        }
            break;

        case MESSAGE_PC: {
            if (!ndo->ndo_vflag)
                ND_PRINT(" pc");
            else {
                ND_PRINT("\n\tPC");
                if(len < 4) goto invalid;
                ND_PRINT(" value %u",
                    GET_BE_U_4(message + 2));
                ND_PRINT(" index len %u", len-4);
            }
        }
            break;

        case MESSAGE_CHALLENGE_REQUEST: {
            if (!ndo->ndo_vflag)
                ND_PRINT(" challenge_request");
            else {
                ND_PRINT("\n\tChallenge Request");
                if(len > 192) goto invalid;
                ND_PRINT(" len %u", len);
            }
        }
            break;

        case MESSAGE_CHALLENGE_REPLY: {
            if (!ndo->ndo_vflag)
                ND_PRINT(" challenge_reply");
            else {
                ND_PRINT("\n\tChallenge Reply");
                if (len > 192) goto invalid;
                ND_PRINT(" len %u", len);
            }
        }
            break;

        default:
            if (!ndo->ndo_vflag)
                ND_PRINT(" unknown");
            else
                ND_PRINT("\n\tUnknown message type %u", type);
        }
        i += len + 2;
    }

    return 0; /* OK */

trunc:
    return -1; /* packet truncated by capture process */

invalid:
    return -2; /* packet is invalid */
}

static void
babel_print_v2(netdissect_options *ndo,
               const u_char *cp, u_int length)
{
    u_short bodylen;
    int ret;

    ND_TCHECK_4(cp);
    if (length < 4)
        goto invalid;
    bodylen = GET_BE_U_2(cp + 2);
    ND_PRINT(" (%u)", bodylen);
    length -= 4;
    cp += 4;

    /* Process the TLVs in the body */
    if (length < bodylen)
        goto invalid;
    ret = babel_print_v2_tlvs(ndo, cp, bodylen, length);
    if (ret == -1)
        goto trunc;
    if (ret == -2)
        goto invalid;
    length -= bodylen;
    cp += bodylen;

    /* If there's a trailer, process the TLVs in the trailer */
    if (length != 0) {
	if(ndo->ndo_vflag) ND_PRINT("\n\t----");
	else ND_PRINT(" |");
        ret = babel_print_v2_tlvs(ndo, cp, length, length);
        if (ret == -1)
            goto trunc;
        if (ret == -2)
            goto invalid;
    }
    return;

 trunc:
    nd_print_trunc(ndo);
    return;

 invalid:
    nd_print_invalid(ndo);
}
