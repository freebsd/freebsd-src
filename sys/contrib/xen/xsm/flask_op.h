/*
 *  This file contains the flask_op hypercall commands and definitions.
 *
 *  Author:  George Coker, <gscoker@alpha.ncsc.mil>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __FLASK_OP_H__
#define __FLASK_OP_H__

#include "../event_channel.h"

#define XEN_FLASK_INTERFACE_VERSION 1

struct xen_flask_load {
    XEN_GUEST_HANDLE(char) buffer;
    uint32_t size;
};
typedef struct xen_flask_load xen_flask_load_t;

struct xen_flask_setenforce {
    uint32_t enforcing;
};
typedef struct xen_flask_setenforce xen_flask_setenforce_t;

struct xen_flask_sid_context {
    /* IN/OUT: sid to convert to/from string */
    uint32_t sid;
    /* IN: size of the context buffer
     * OUT: actual size of the output context string
     */
    uint32_t size;
    XEN_GUEST_HANDLE(char) context;
};
typedef struct xen_flask_sid_context xen_flask_sid_context_t;

struct xen_flask_access {
    /* IN: access request */
    uint32_t ssid;
    uint32_t tsid;
    uint32_t tclass;
    uint32_t req;
    /* OUT: AVC data */
    uint32_t allowed;
    uint32_t audit_allow;
    uint32_t audit_deny;
    uint32_t seqno;
};
typedef struct xen_flask_access xen_flask_access_t;

struct xen_flask_transition {
    /* IN: transition SIDs and class */
    uint32_t ssid;
    uint32_t tsid;
    uint32_t tclass;
    /* OUT: new SID */
    uint32_t newsid;
};
typedef struct xen_flask_transition xen_flask_transition_t;

#if __XEN_INTERFACE_VERSION__ < 0x00040800
struct xen_flask_userlist {
    /* IN: starting SID for list */
    uint32_t start_sid;
    /* IN: size of user string and output buffer
     * OUT: number of SIDs returned */
    uint32_t size;
    union {
        /* IN: user to enumerate SIDs */
        XEN_GUEST_HANDLE(char) user;
        /* OUT: SID list */
        XEN_GUEST_HANDLE(uint32) sids;
    } u;
};
#endif

struct xen_flask_boolean {
    /* IN/OUT: numeric identifier for boolean [GET/SET]
     * If -1, name will be used and bool_id will be filled in. */
    uint32_t bool_id;
    /* OUT: current enforcing value of boolean [GET/SET] */
    uint8_t enforcing;
    /* OUT: pending value of boolean [GET/SET] */
    uint8_t pending;
    /* IN: new value of boolean [SET] */
    uint8_t new_value;
    /* IN: commit new value instead of only setting pending [SET] */
    uint8_t commit;
    /* IN: size of boolean name buffer [GET/SET]
     * OUT: actual size of name [GET only] */
    uint32_t size;
    /* IN: if bool_id is -1, used to find boolean [GET/SET]
     * OUT: textual name of boolean [GET only]
     */
    XEN_GUEST_HANDLE(char) name;
};
typedef struct xen_flask_boolean xen_flask_boolean_t;

struct xen_flask_setavc_threshold {
    /* IN */
    uint32_t threshold;
};
typedef struct xen_flask_setavc_threshold xen_flask_setavc_threshold_t;

struct xen_flask_hash_stats {
    /* OUT */
    uint32_t entries;
    uint32_t buckets_used;
    uint32_t buckets_total;
    uint32_t max_chain_len;
};
typedef struct xen_flask_hash_stats xen_flask_hash_stats_t;

struct xen_flask_cache_stats {
    /* IN */
    uint32_t cpu;
    /* OUT */
    uint32_t lookups;
    uint32_t hits;
    uint32_t misses;
    uint32_t allocations;
    uint32_t reclaims;
    uint32_t frees;
};
typedef struct xen_flask_cache_stats xen_flask_cache_stats_t;

struct xen_flask_ocontext {
    /* IN */
    uint32_t ocon;
    uint32_t sid;
    uint64_t low, high;
};
typedef struct xen_flask_ocontext xen_flask_ocontext_t;

struct xen_flask_peersid {
    /* IN */
    evtchn_port_t evtchn;
    /* OUT */
    uint32_t sid;
};
typedef struct xen_flask_peersid xen_flask_peersid_t;

struct xen_flask_relabel {
    /* IN */
    uint32_t domid;
    uint32_t sid;
};
typedef struct xen_flask_relabel xen_flask_relabel_t;

struct xen_flask_devicetree_label {
    /* IN */
    uint32_t sid;
    uint32_t length;
    XEN_GUEST_HANDLE(char) path;
};
typedef struct xen_flask_devicetree_label xen_flask_devicetree_label_t;

struct xen_flask_op {
    uint32_t cmd;
#define FLASK_LOAD              1
#define FLASK_GETENFORCE        2
#define FLASK_SETENFORCE        3
#define FLASK_CONTEXT_TO_SID    4
#define FLASK_SID_TO_CONTEXT    5
#define FLASK_ACCESS            6
#define FLASK_CREATE            7
#define FLASK_RELABEL           8
#define FLASK_USER              9  /* No longer implemented */
#define FLASK_POLICYVERS        10
#define FLASK_GETBOOL           11
#define FLASK_SETBOOL           12
#define FLASK_COMMITBOOLS       13
#define FLASK_MLS               14
#define FLASK_DISABLE           15 /* No longer implemented */
#define FLASK_GETAVC_THRESHOLD  16
#define FLASK_SETAVC_THRESHOLD  17
#define FLASK_AVC_HASHSTATS     18
#define FLASK_AVC_CACHESTATS    19
#define FLASK_MEMBER            20
#define FLASK_ADD_OCONTEXT      21
#define FLASK_DEL_OCONTEXT      22
#define FLASK_GET_PEER_SID      23
#define FLASK_RELABEL_DOMAIN    24
#define FLASK_DEVICETREE_LABEL  25
    uint32_t interface_version; /* XEN_FLASK_INTERFACE_VERSION */
    union {
        xen_flask_load_t load;
        xen_flask_setenforce_t enforce;
        /* FLASK_CONTEXT_TO_SID and FLASK_SID_TO_CONTEXT */
        xen_flask_sid_context_t sid_context;
        xen_flask_access_t access;
        /* FLASK_CREATE, FLASK_RELABEL, FLASK_MEMBER */
        xen_flask_transition_t transition;
#if __XEN_INTERFACE_VERSION__ < 0x00040800
        struct xen_flask_userlist userlist;
#endif
        /* FLASK_GETBOOL, FLASK_SETBOOL */
        xen_flask_boolean_t boolean;
        xen_flask_setavc_threshold_t setavc_threshold;
        xen_flask_hash_stats_t hash_stats;
        xen_flask_cache_stats_t cache_stats;
        /* FLASK_ADD_OCONTEXT, FLASK_DEL_OCONTEXT */
        xen_flask_ocontext_t ocontext;
        xen_flask_peersid_t peersid;
        xen_flask_relabel_t relabel;
        xen_flask_devicetree_label_t devicetree_label;
    } u;
};
typedef struct xen_flask_op xen_flask_op_t;
DEFINE_XEN_GUEST_HANDLE(xen_flask_op_t);

#endif
