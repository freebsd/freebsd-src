/**
 * Copyright (c) 2012
 *
 * Gregory Detal <gregory.detal@uclouvain.be>
 * Christoph Paasch <christoph.paasch@uclouvain.be>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* \summary: Multipath TCP (MPTCP) printer */

/* specification: RFC 6824 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"

#include "tcp.h"

#define MPTCP_SUB_CAPABLE       0x0
#define MPTCP_SUB_JOIN          0x1
#define MPTCP_SUB_DSS           0x2
#define MPTCP_SUB_ADD_ADDR      0x3
#define MPTCP_SUB_REMOVE_ADDR   0x4
#define MPTCP_SUB_PRIO          0x5
#define MPTCP_SUB_FAIL          0x6
#define MPTCP_SUB_FCLOSE        0x7

struct mptcp_option {
        nd_uint8_t     kind;
        nd_uint8_t     len;
        nd_uint8_t     sub_etc;        /* subtype upper 4 bits, other stuff lower 4 bits */
};

#define MPTCP_OPT_SUBTYPE(sub_etc)      (((sub_etc) >> 4) & 0xF)

struct mp_capable {
        nd_uint8_t     kind;
        nd_uint8_t     len;
        nd_uint8_t     sub_ver;
        nd_uint8_t     flags;
        nd_uint64_t    sender_key;
        nd_uint64_t    receiver_key;
};

#define MP_CAPABLE_OPT_VERSION(sub_ver) (((sub_ver) >> 0) & 0xF)
#define MP_CAPABLE_C                    0x80
#define MP_CAPABLE_S                    0x01

struct mp_join {
        nd_uint8_t     kind;
        nd_uint8_t     len;
        nd_uint8_t     sub_b;
        nd_uint8_t     addr_id;
        union {
                struct {
                        nd_uint32_t     token;
                        nd_uint32_t     nonce;
                } syn;
                struct {
                        nd_uint64_t     mac;
                        nd_uint32_t     nonce;
                } synack;
                struct {
                        nd_byte         mac[20];
                } ack;
        } u;
};

#define MP_JOIN_B                       0x01

struct mp_dss {
        nd_uint8_t     kind;
        nd_uint8_t     len;
        nd_uint8_t     sub;
        nd_uint8_t     flags;
};

#define MP_DSS_F                        0x10
#define MP_DSS_m                        0x08
#define MP_DSS_M                        0x04
#define MP_DSS_a                        0x02
#define MP_DSS_A                        0x01

static const struct tok mptcp_addr_subecho_bits[] = {
        { 0x6, "v0-ip6" },
        { 0x4, "v0-ip4" },
        { 0x1, "v1-echo" },
        { 0x0, "v1" },
        { 0, NULL }
};

struct mp_add_addr {
        nd_uint8_t     kind;
        nd_uint8_t     len;
        nd_uint8_t     sub_echo;
        nd_uint8_t     addr_id;
        union {
                struct {
                        nd_ipv4         addr;
                        nd_uint16_t     port;
                        nd_uint64_t     mac;
                } v4;
                struct {
                        nd_ipv4         addr;
                        nd_uint64_t     mac;
                } v4np;
                struct {
                        nd_ipv6         addr;
                        nd_uint16_t     port;
                        nd_uint64_t     mac;
                } v6;
                struct {
                        nd_ipv6         addr;
                        nd_uint64_t     mac;
                } v6np;
        } u;
};

struct mp_remove_addr {
        nd_uint8_t     kind;
        nd_uint8_t     len;
        nd_uint8_t     sub;
        /* list of addr_id */
        nd_uint8_t     addrs_id[1];
};

struct mp_fail {
        nd_uint8_t     kind;
        nd_uint8_t     len;
        nd_uint8_t     sub;
        nd_uint8_t     resv;
        nd_uint64_t    data_seq;
};

struct mp_close {
        nd_uint8_t     kind;
        nd_uint8_t     len;
        nd_uint8_t     sub;
        nd_uint8_t     rsv;
        nd_byte        key[8];
};

struct mp_prio {
        nd_uint8_t     kind;
        nd_uint8_t     len;
        nd_uint8_t     sub_b;
        nd_uint8_t     addr_id;
};

#define MP_PRIO_B                       0x01

static int
dummy_print(netdissect_options *ndo _U_,
            const u_char *opt _U_, u_int opt_len _U_, u_char flags _U_)
{
        return 1;
}

static int
mp_capable_print(netdissect_options *ndo,
                 const u_char *opt, u_int opt_len, u_char flags)
{
        const struct mp_capable *mpc = (const struct mp_capable *) opt;
        uint8_t version;

        if (!((opt_len == 12 || opt_len == 4) && flags & TH_SYN) &&
            !((opt_len == 20 || opt_len == 22) && (flags & (TH_SYN | TH_ACK)) ==
              TH_ACK))
                return 0;

        version = MP_CAPABLE_OPT_VERSION(GET_U_1(mpc->sub_ver));
        switch (version) {
                case 0: /* fall through */
                case 1:
                        ND_PRINT(" v%u", version);
                        break;
                default:
                        ND_PRINT(" Unknown Version (%u)", version);
                        return 1;
        }

        if (GET_U_1(mpc->flags) & MP_CAPABLE_C)
                ND_PRINT(" csum");
        if (opt_len == 12 || opt_len >= 20) {
                ND_PRINT(" {0x%" PRIx64, GET_BE_U_8(mpc->sender_key));
                if (opt_len >= 20)
                        ND_PRINT(",0x%" PRIx64, GET_BE_U_8(mpc->receiver_key));
                ND_PRINT("}");
        }
        return 1;
}

static int
mp_join_print(netdissect_options *ndo,
              const u_char *opt, u_int opt_len, u_char flags)
{
        const struct mp_join *mpj = (const struct mp_join *) opt;

        if (!(opt_len == 12 && (flags & TH_SYN)) &&
            !(opt_len == 16 && (flags & (TH_SYN | TH_ACK)) == (TH_SYN | TH_ACK)) &&
            !(opt_len == 24 && (flags & TH_ACK)))
                return 0;

        if (opt_len != 24) {
                if (GET_U_1(mpj->sub_b) & MP_JOIN_B)
                        ND_PRINT(" backup");
                ND_PRINT(" id %u", GET_U_1(mpj->addr_id));
        }

        switch (opt_len) {
        case 12: /* SYN */
                ND_PRINT(" token 0x%x" " nonce 0x%x",
                        GET_BE_U_4(mpj->u.syn.token),
                        GET_BE_U_4(mpj->u.syn.nonce));
                break;
        case 16: /* SYN/ACK */
                ND_PRINT(" hmac 0x%" PRIx64 " nonce 0x%x",
                        GET_BE_U_8(mpj->u.synack.mac),
                        GET_BE_U_4(mpj->u.synack.nonce));
                break;
        case 24: {/* ACK */
                size_t i;
                ND_PRINT(" hmac 0x");
                for (i = 0; i < sizeof(mpj->u.ack.mac); ++i)
                        ND_PRINT("%02x", mpj->u.ack.mac[i]);
        }
        default:
                break;
        }
        return 1;
}

static int
mp_dss_print(netdissect_options *ndo,
             const u_char *opt, u_int opt_len, u_char flags)
{
        const struct mp_dss *mdss = (const struct mp_dss *) opt;
        uint8_t mdss_flags;

        /* We need the flags, at a minimum. */
        if (opt_len < 4)
                return 0;

        if (flags & TH_SYN)
                return 0;

        mdss_flags = GET_U_1(mdss->flags);
        if (mdss_flags & MP_DSS_F)
                ND_PRINT(" fin");

        opt += 4;
        opt_len -= 4;
        if (mdss_flags & MP_DSS_A) {
                /* Ack present */
                ND_PRINT(" ack ");
                /*
                 * If the a flag is set, we have an 8-byte ack; if it's
                 * clear, we have a 4-byte ack.
                 */
                if (mdss_flags & MP_DSS_a) {
                        if (opt_len < 8)
                                return 0;
                        ND_PRINT("%" PRIu64, GET_BE_U_8(opt));
                        opt += 8;
                        opt_len -= 8;
                } else {
                        if (opt_len < 4)
                                return 0;
                        ND_PRINT("%u", GET_BE_U_4(opt));
                        opt += 4;
                        opt_len -= 4;
                }
        }

        if (mdss_flags & MP_DSS_M) {
                /*
                 * Data Sequence Number (DSN), Subflow Sequence Number (SSN),
                 * Data-Level Length present, and Checksum possibly present.
                 */
                ND_PRINT(" seq ");
                /*
                 * If the m flag is set, we have an 8-byte NDS; if it's clear,
                 * we have a 4-byte DSN.
                 */
                if (mdss_flags & MP_DSS_m) {
                        if (opt_len < 8)
                                return 0;
                        ND_PRINT("%" PRIu64, GET_BE_U_8(opt));
                        opt += 8;
                        opt_len -= 8;
                } else {
                        if (opt_len < 4)
                                return 0;
                        ND_PRINT("%u", GET_BE_U_4(opt));
                        opt += 4;
                        opt_len -= 4;
                }
                if (opt_len < 4)
                        return 0;
                ND_PRINT(" subseq %u", GET_BE_U_4(opt));
                opt += 4;
                opt_len -= 4;
                if (opt_len < 2)
                        return 0;
                ND_PRINT(" len %u", GET_BE_U_2(opt));
                opt += 2;
                opt_len -= 2;

                /*
                 * The Checksum is present only if negotiated.
                 * If there are at least 2 bytes left, process the next 2
                 * bytes as the Checksum.
                 */
                if (opt_len >= 2) {
                        ND_PRINT(" csum 0x%x", GET_BE_U_2(opt));
                        opt_len -= 2;
                }
        }
        if (opt_len != 0)
                return 0;
        return 1;
}

static int
add_addr_print(netdissect_options *ndo,
               const u_char *opt, u_int opt_len, u_char flags _U_)
{
        const struct mp_add_addr *add_addr = (const struct mp_add_addr *) opt;

        if (!(opt_len == 8 || opt_len == 10 || opt_len == 16 || opt_len == 18 ||
            opt_len == 20 || opt_len == 22 || opt_len == 28 || opt_len == 30))
                return 0;

        ND_PRINT(" %s",
                 tok2str(mptcp_addr_subecho_bits, "[bad version/echo]",
                         GET_U_1(add_addr->sub_echo) & 0xF));
        ND_PRINT(" id %u", GET_U_1(add_addr->addr_id));
        if (opt_len == 8 || opt_len == 10 || opt_len == 16 || opt_len == 18) {
                ND_PRINT(" %s", GET_IPADDR_STRING(add_addr->u.v4.addr));
                if (opt_len == 10 || opt_len == 18)
                        ND_PRINT(":%u", GET_BE_U_2(add_addr->u.v4.port));
                if (opt_len == 16)
                        ND_PRINT(" hmac 0x%" PRIx64, GET_BE_U_8(add_addr->u.v4np.mac));
                if (opt_len == 18)
                        ND_PRINT(" hmac 0x%" PRIx64, GET_BE_U_8(add_addr->u.v4.mac));
        }

        if (opt_len == 20 || opt_len == 22 || opt_len == 28 || opt_len == 30) {
                ND_PRINT(" %s", GET_IP6ADDR_STRING(add_addr->u.v6.addr));
                if (opt_len == 22 || opt_len == 30)
                        ND_PRINT(":%u", GET_BE_U_2(add_addr->u.v6.port));
                if (opt_len == 28)
                        ND_PRINT(" hmac 0x%" PRIx64, GET_BE_U_8(add_addr->u.v6np.mac));
                if (opt_len == 30)
                        ND_PRINT(" hmac 0x%" PRIx64, GET_BE_U_8(add_addr->u.v6.mac));
        }

        return 1;
}

static int
remove_addr_print(netdissect_options *ndo,
                  const u_char *opt, u_int opt_len, u_char flags _U_)
{
        const struct mp_remove_addr *remove_addr = (const struct mp_remove_addr *) opt;
        u_int i;

        if (opt_len < 4)
                return 0;

        opt_len -= 3;
        ND_PRINT(" id");
        for (i = 0; i < opt_len; i++)
                ND_PRINT(" %u", GET_U_1(remove_addr->addrs_id[i]));
        return 1;
}

static int
mp_prio_print(netdissect_options *ndo,
              const u_char *opt, u_int opt_len, u_char flags _U_)
{
        const struct mp_prio *mpp = (const struct mp_prio *) opt;

        if (opt_len != 3 && opt_len != 4)
                return 0;

        if (GET_U_1(mpp->sub_b) & MP_PRIO_B)
                ND_PRINT(" backup");
        else
                ND_PRINT(" non-backup");
        if (opt_len == 4)
                ND_PRINT(" id %u", GET_U_1(mpp->addr_id));

        return 1;
}

static int
mp_fail_print(netdissect_options *ndo,
              const u_char *opt, u_int opt_len, u_char flags _U_)
{
        if (opt_len != 12)
                return 0;

        ND_PRINT(" seq %" PRIu64, GET_BE_U_8(opt + 4));
        return 1;
}

static int
mp_fast_close_print(netdissect_options *ndo,
                    const u_char *opt, u_int opt_len, u_char flags _U_)
{
        if (opt_len != 12)
                return 0;

        ND_PRINT(" key 0x%" PRIx64, GET_BE_U_8(opt + 4));
        return 1;
}

static const struct {
        const char *name;
        int (*print)(netdissect_options *, const u_char *, u_int, u_char);
} mptcp_options[] = {
        { "capable",    mp_capable_print },
        { "join",       mp_join_print },
        { "dss",        mp_dss_print },
        { "add-addr",   add_addr_print },
        { "rem-addr",   remove_addr_print },
        { "prio",       mp_prio_print },
        { "fail",       mp_fail_print },
        { "fast-close", mp_fast_close_print },
        { "unknown",    dummy_print },
};

int
mptcp_print(netdissect_options *ndo,
            const u_char *cp, u_int len, u_char flags)
{
        const struct mptcp_option *opt;
        u_int subtype;

        ndo->ndo_protocol = "mptcp";
        if (len < 3)
                return 0;

        opt = (const struct mptcp_option *) cp;
        subtype = MPTCP_OPT_SUBTYPE(GET_U_1(opt->sub_etc));
        subtype = ND_MIN(subtype, MPTCP_SUB_FCLOSE + 1);

        ND_PRINT(" %u", len);

        ND_PRINT(" %s", mptcp_options[subtype].name);
        return mptcp_options[subtype].print(ndo, cp, len, flags);
}
