/*
 * Copyright (c) 1998, Larry Lile
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define TOKEN_HDR_LEN       14
#define TOKEN_RING_MAC_LEN  6
#define ROUTING_SEGMENT_MAX 16
#define IS_SOURCE_ROUTED    (tp->ether_shost[0] & 0x80)
#define BROADCAST           ((ntohs(tp->rcf) & 0xE000) >> 13)
#define RIF_LENGTH          ((ntohs(tp->rcf) & 0x1f00) >> 8)
#define DIRECTION           ((ntohs(tp->rcf) & 0x0080) >> 7)
#define LARGEST_FRAME       ((ntohs(tp->rcf) & 0x0070) >> 4)
#define RING_NUMBER(x)      ((ntohs(tp->rseg[x]) & 0xfff0) >> 4)
#define BRIDGE_NUMBER(x)    ((ntohs(tp->rseg[x]) & 0x000f))
#define SEGMENT_COUNT       ((RIF_LENGTH - 2) / 2)

char *broadcast_indicator[] = { "Non-Broadcast", "Non-Broadcast", 
                                "Non-Broadcast", "Non-Broadcast",
                                "All-routes",    "All-routes",
                                "Single-route",  "Single-route"};

char *direction[] = { "Forward", "Backward"};

char *largest_frame[] = { "516", "1500", "2052", "4472", "8144",
                          "11407", "17800", ""};


struct token_header {
        u_char   ac;
        u_char   fc;
        u_char   ether_dhost[TOKEN_RING_MAC_LEN];
        u_char   ether_shost[TOKEN_RING_MAC_LEN];
        u_short  rcf;
        u_short  rseg[ROUTING_SEGMENT_MAX];
};
