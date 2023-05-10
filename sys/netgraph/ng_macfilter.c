/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002 Ericsson Research & Pekka Nikander
 * Copyright (c) 2020 Nick Hibma <n_hibma@FreeBSD.org>
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
 * $FreeBSD$
 */

/* 
 * MACFILTER NETGRAPH NODE TYPE
 *
 * This node type routes packets from the ether hook to either the default hook
 * if sender MAC address is not in the MAC table, or out over the specified
 * hook if it is.
 *
 * Other node types can then be used to apply specific processing to the
 * packets on each hook.
 *
 * If compiled with NG_MACFILTER_DEBUG the flow and resizing of the MAC table
 * are logged to the console.
 *
 * If compiled with NG_MACFILTER_DEBUG_RECVDATA every packet handled is logged
 * on the console.
 */

#include <sys/param.h>
#include <sys/ctype.h>
#include <sys/systm.h>

#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>

#include <sys/kernel.h>
#include <sys/malloc.h>

#include <sys/socket.h>
#include <net/ethernet.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>

#include "ng_macfilter.h"

#ifdef NG_SEPARATE_MALLOC
MALLOC_DEFINE(M_NETGRAPH_MACFILTER, "netgraph_macfilter", "netgraph macfilter node ");
#else
#define M_NETGRAPH_MACFILTER M_NETGRAPH
#endif

#define MACTABLE_BLOCKSIZE      128      /* block size for incrementing table */

#ifdef NG_MACFILTER_DEBUG
#define MACFILTER_DEBUG(fmt, ...)         printf("%s:%d: " fmt "\n", __FUNCTION__, __LINE__, __VA_ARGS__)
#else
#define MACFILTER_DEBUG(fmt, ...)
#endif
#define MAC_FMT                 "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_S_ARGS(v)           (v)[0], (v)[1], (v)[2], (v)[3], (v)[4], (v)[5]

/*
 * Parse type for struct ngm_macfilter_direct 
 */

static const struct ng_parse_struct_field macfilter_direct_fields[]
        = NGM_MACFILTER_DIRECT_FIELDS;
static const struct ng_parse_type ng_macfilter_direct_type = {
    &ng_parse_struct_type,
    &macfilter_direct_fields
};

/*
 * Parse type for struct ngm_macfilter_direct_hookid.
 */

static const struct ng_parse_struct_field macfilter_direct_ndx_fields[]
        = NGM_MACFILTER_DIRECT_NDX_FIELDS;
static const struct ng_parse_type ng_macfilter_direct_hookid_type = {
    &ng_parse_struct_type,
    &macfilter_direct_ndx_fields
};

/*
 * Parse types for struct ngm_macfilter_get_macs.
 */
static int
macfilter_get_macs_count(const struct ng_parse_type *type,
    const u_char *start, const u_char *buf)
{
	const struct ngm_macfilter_macs *const ngm_macs =
	    (const struct ngm_macfilter_macs *)(buf - OFFSETOF(struct ngm_macfilter_macs, macs));

	return ngm_macs->n;
}
static const struct ng_parse_struct_field ng_macfilter_mac_fields[]
        = NGM_MACFILTER_MAC_FIELDS;
static const struct ng_parse_type ng_macfilter_mac_type = {
    &ng_parse_struct_type,
    ng_macfilter_mac_fields,
};
static const struct ng_parse_array_info ng_macfilter_macs_array_info = {
    &ng_macfilter_mac_type,
    macfilter_get_macs_count
};
static const struct ng_parse_type ng_macfilter_macs_array_type = {
    &ng_parse_array_type,
    &ng_macfilter_macs_array_info
};
static const struct ng_parse_struct_field ng_macfilter_macs_fields[]
        = NGM_MACFILTER_MACS_FIELDS;
static const struct ng_parse_type ng_macfilter_macs_type = {
    &ng_parse_struct_type,
    &ng_macfilter_macs_fields
};

/*
 * Parse types for struct ngm_macfilter_get_hooks.
 */
static int
macfilter_get_upper_hook_count(const struct ng_parse_type *type,
    const u_char *start, const u_char *buf)
{
	const struct ngm_macfilter_hooks *const ngm_hooks =
	    (const struct ngm_macfilter_hooks *)(buf - OFFSETOF(struct ngm_macfilter_hooks, hooks));

        MACFILTER_DEBUG("buf %p, ngm_hooks %p, n %d", buf, ngm_hooks, ngm_hooks->n);

	return ngm_hooks->n;
}

static const struct ng_parse_struct_field ng_macfilter_hook_fields[]
        = NGM_MACFILTER_HOOK_FIELDS;
static const struct ng_parse_type ng_macfilter_hook_type = {
    &ng_parse_struct_type,
    ng_macfilter_hook_fields,
};
static const struct ng_parse_array_info ng_macfilter_hooks_array_info = {
    &ng_macfilter_hook_type,
    macfilter_get_upper_hook_count   
};
static const struct ng_parse_type ng_macfilter_hooks_array_type = {
    &ng_parse_array_type,
    &ng_macfilter_hooks_array_info
};
static const struct ng_parse_struct_field ng_macfilter_hooks_fields[]
        = NGM_MACFILTER_HOOKS_FIELDS;
static const struct ng_parse_type ng_macfilter_hooks_type = {
    &ng_parse_struct_type,
    &ng_macfilter_hooks_fields
};

/*
 * List of commands and how to convert arguments to/from ASCII 
 */
static const struct ng_cmdlist ng_macfilter_cmdlist[] = {
    {
	NGM_MACFILTER_COOKIE,
	NGM_MACFILTER_RESET,
	"reset",
	NULL,
	NULL
    },
    {
	NGM_MACFILTER_COOKIE,
	NGM_MACFILTER_DIRECT,
	"direct",
	&ng_macfilter_direct_type,
	NULL
    },
    {
	NGM_MACFILTER_COOKIE,
	NGM_MACFILTER_DIRECT_HOOKID,
	"directi",
	&ng_macfilter_direct_hookid_type,
	NULL
    },
    {
        NGM_MACFILTER_COOKIE,
        NGM_MACFILTER_GET_MACS,
        "getmacs",
        NULL,
        &ng_macfilter_macs_type
    },
    {
        NGM_MACFILTER_COOKIE,
        NGM_MACFILTER_GETCLR_MACS,
        "getclrmacs",
        NULL,
        &ng_macfilter_macs_type
    },
    {
        NGM_MACFILTER_COOKIE,
        NGM_MACFILTER_CLR_MACS,
        "clrmacs",
        NULL,
        NULL,
    },
    {
        NGM_MACFILTER_COOKIE,
        NGM_MACFILTER_GET_HOOKS,
        "gethooks",
        NULL,
        &ng_macfilter_hooks_type
    },
    { 0 }
};

/*
 * Netgraph node type descriptor 
 */
static ng_constructor_t	ng_macfilter_constructor;
static ng_rcvmsg_t	ng_macfilter_rcvmsg;
static ng_shutdown_t	ng_macfilter_shutdown;
static ng_newhook_t	ng_macfilter_newhook;
static ng_rcvdata_t	ng_macfilter_rcvdata;
static ng_disconnect_t	ng_macfilter_disconnect;

static struct ng_type typestruct = {
    .version =     NG_ABI_VERSION,
    .name =	   NG_MACFILTER_NODE_TYPE,
    .constructor = ng_macfilter_constructor,
    .rcvmsg =	   ng_macfilter_rcvmsg,
    .shutdown =	   ng_macfilter_shutdown,
    .newhook =	   ng_macfilter_newhook,
    .rcvdata =	   ng_macfilter_rcvdata,
    .disconnect =  ng_macfilter_disconnect,
    .cmdlist =	   ng_macfilter_cmdlist
};
NETGRAPH_INIT(macfilter, &typestruct);

/* 
 * Per MAC address info: the hook where to send to, the address
 * Note: We use the same struct as in the netgraph message, so we can bcopy the
 * array.
 */
typedef struct ngm_macfilter_mac *mf_mac_p;

/*
 * Node info
 */
typedef struct {
    hook_p     mf_ether_hook;	/* Ethernet hook */

    hook_p     *mf_upper;       /* Upper hooks */
    u_int      mf_upper_cnt;    /* Allocated # of upper slots */

    struct mtx mtx;             /* Mutex for MACs table */
    mf_mac_p   mf_macs;		/* MAC info: dynamically allocated */
    u_int      mf_mac_allocated;/* Allocated # of MAC slots */
    u_int      mf_mac_used;	/* Used # of MAC slots */
} *macfilter_p;

/*
 * Resize the MAC table to accommodate at least mfp->mf_mac_used + 1 entries.
 *
 * Note: mtx already held
 */
static int
macfilter_mactable_resize(macfilter_p mfp)
{
    int error = 0;

    int n = mfp->mf_mac_allocated;
    if (mfp->mf_mac_used < 2*MACTABLE_BLOCKSIZE-1)                              /* minimum size */
        n = 2*MACTABLE_BLOCKSIZE-1;
    else if (mfp->mf_mac_used + 2*MACTABLE_BLOCKSIZE < mfp->mf_mac_allocated)   /* reduce size */
        n = mfp->mf_mac_allocated - MACTABLE_BLOCKSIZE;
    else if (mfp->mf_mac_used == mfp->mf_mac_allocated)                         /* increase size */
        n = mfp->mf_mac_allocated + MACTABLE_BLOCKSIZE;

    if (n != mfp->mf_mac_allocated) {
        MACFILTER_DEBUG("used=%d allocated=%d->%d",
              mfp->mf_mac_used, mfp->mf_mac_allocated, n);
        
        mf_mac_p mfp_new = realloc(mfp->mf_macs,
                sizeof(mfp->mf_macs[0])*n,
                M_NETGRAPH, M_NOWAIT | M_ZERO);
        if (mfp_new == NULL) {
            error = -1;
        } else {
            mfp->mf_macs = mfp_new;
            mfp->mf_mac_allocated = n;
        }
    }

    return error;
}

/*
 * Resets the macfilter to pass all received packets
 * to the default hook.
 *
 * Note: mtx already held
 */
static void
macfilter_reset(macfilter_p mfp)
{
    mfp->mf_mac_used = 0;

    macfilter_mactable_resize(mfp);
}

/*
 * Resets the counts for each MAC address.
 *
 * Note: mtx already held
 */
static void
macfilter_reset_stats(macfilter_p mfp)
{
    int i;

    for (i = 0; i < mfp->mf_mac_used; i++) {
        mf_mac_p p = &mfp->mf_macs[i];
        p->packets_in = p->packets_out = 0;
        p->bytes_in = p->bytes_out = 0;
    }
}

/*
 * Count the number of matching macs routed to this hook.
 * 
 * Note: mtx already held
 */
static int
macfilter_mac_count(macfilter_p mfp, int hookid)
{
    int i;
    int cnt = 0;

    for (i = 0; i < mfp->mf_mac_used; i++)
        if (mfp->mf_macs[i].hookid == hookid)
            cnt++;

    return cnt;
}

/*
 * Find a MAC address in the mac table.
 *
 * Returns 0 on failure with *ri set to index before which to insert a new
 * element. Or returns 1 on success with *ri set to the index of the element
 * that matches. 
 * 
 * Note: mtx already held.
 */
static u_int
macfilter_find_mac(macfilter_p mfp, const u_char *ether, u_int *ri)
{
    mf_mac_p mf_macs = mfp->mf_macs;

    u_int base = 0;
    u_int range = mfp->mf_mac_used;
    while (range > 0) {
        u_int middle = base + (range >> 1);             /* middle */
	int d = bcmp(ether, mf_macs[middle].ether, ETHER_ADDR_LEN);
	if (d == 0) {   	                        /* match */
            *ri = middle;
            return 1;
	} else if (d > 0) {                             /* move right */
            range -= middle - base + 1;
            base = middle + 1;
	} else {                                        /* move left */
            range = middle - base;
        }
    }

    *ri = base;
    return 0;
}

/*
 * Change the upper hook for the given MAC address. If the hook id is zero (the
 * default hook), the MAC address is removed from the table. Otherwise it is
 * inserted to the table at a proper location, and the id of the hook is
 * marked.
 * 
 * Note: mtx already held.
 */
static int
macfilter_mactable_change(macfilter_p mfp, u_char *ether, int hookid)
{
    u_int i;
    int found = macfilter_find_mac(mfp, ether, &i);

    mf_mac_p mf_macs = mfp->mf_macs;

    MACFILTER_DEBUG("ether=" MAC_FMT " found=%d i=%d ether=" MAC_FMT " hookid=%d->%d used=%d allocated=%d",
          MAC_S_ARGS(ether), found, i, MAC_S_ARGS(mf_macs[i].ether),
          (found? mf_macs[i].hookid:NG_MACFILTER_HOOK_DEFAULT_ID), hookid,
          mfp->mf_mac_used, mfp->mf_mac_allocated);

    if (found) {
        if (hookid == NG_MACFILTER_HOOK_DEFAULT_ID) {   /* drop */
            /* Compress table */
            mfp->mf_mac_used--;
            size_t len = (mfp->mf_mac_used - i) * sizeof(mf_macs[0]);
            if (len > 0)
                bcopy(&mf_macs[i+1], &mf_macs[i], len);

            macfilter_mactable_resize(mfp);
        } else {                                        /* modify */
            mf_macs[i].hookid = hookid;
        }   
    } else {
        if (hookid == NG_MACFILTER_HOOK_DEFAULT_ID) {   /* not found */
            /* not present and not inserted */
            return 0;
        } else {                                        /* add */
            if (macfilter_mactable_resize(mfp) == -1) {
                return ENOMEM;
            } else {
                mf_macs = mfp->mf_macs;                 /* reassign; might have moved during resize */

                /* make room for new entry, unless appending */
                size_t len = (mfp->mf_mac_used - i) * sizeof(mf_macs[0]);
                if (len > 0)
                    bcopy(&mf_macs[i], &mf_macs[i+1], len);

                mf_macs[i].hookid = hookid;
                bcopy(ether, mf_macs[i].ether, ETHER_ADDR_LEN);

                mfp->mf_mac_used++;
            }
        }
    }

    return 0;
}

static int
macfilter_mactable_remove_by_hookid(macfilter_p mfp, int hookid)
{
    int i, j;

    for (i = 0, j = 0; i < mfp->mf_mac_used; i++) {
        if (mfp->mf_macs[i].hookid != hookid) {
            if (i != j)
                bcopy(&mfp->mf_macs[i], &mfp->mf_macs[j], sizeof(mfp->mf_macs[0]));
            j++;
        }
    }

    int removed = i - j;
    mfp->mf_mac_used = j;
    macfilter_mactable_resize(mfp);

    return removed;
}

static int
macfilter_find_hook(macfilter_p mfp, const char *hookname)
{
    int hookid;

    for (hookid = 0; hookid < mfp->mf_upper_cnt; hookid++) {
	if (mfp->mf_upper[hookid]) {
            if (strncmp(NG_HOOK_NAME(mfp->mf_upper[hookid]), 
		    hookname, NG_HOOKSIZ) == 0) {
                return hookid;
            }
        }
    }

    return 0;
}

static int
macfilter_direct(macfilter_p mfp, struct ngm_macfilter_direct *md)
{
    MACFILTER_DEBUG("ether=" MAC_FMT " hook=%s",
        MAC_S_ARGS(md->ether), md->hookname);

    int hookid = macfilter_find_hook(mfp, md->hookname);
    if (hookid < 0)
        return ENOENT;

    return macfilter_mactable_change(mfp, md->ether, hookid);
}

static int
macfilter_direct_hookid(macfilter_p mfp, struct ngm_macfilter_direct_hookid *mdi)
{
    MACFILTER_DEBUG("ether=" MAC_FMT " hookid=%d",
        MAC_S_ARGS(mdi->ether), mdi->hookid);

    if (mdi->hookid >= mfp->mf_upper_cnt)
	return EINVAL;
    else if (mfp->mf_upper[mdi->hookid] == NULL)
	return EINVAL;
    
    return macfilter_mactable_change(mfp, mdi->ether, mdi->hookid);
}

/*
 * Packet handling
 */

/*
 * Pass packets received from any upper hook to
 * a lower hook
 */
static int
macfilter_ether_output(hook_p hook, macfilter_p mfp, struct mbuf *m, hook_p *next_hook)
{
    struct ether_header *ether_header = mtod(m, struct ether_header *);
    u_char *ether = ether_header->ether_dhost;

    *next_hook = mfp->mf_ether_hook;

    mtx_lock(&mfp->mtx);

    u_int i;
    int found = macfilter_find_mac(mfp, ether, &i);
    if (found) {
        mf_mac_p mf_macs = mfp->mf_macs;

        mf_macs[i].packets_out++;
        if (m->m_len > ETHER_HDR_LEN)
            mf_macs[i].bytes_out += m->m_len - ETHER_HDR_LEN;
    
#ifdef NG_MACFILTER_DEBUG_RECVDATA
        MACFILTER_DEBUG("ether=" MAC_FMT " len=%db->%lldb: bytes: %s -> %s",
            MAC_S_ARGS(ether), m->m_len - ETHER_HDR_LEN, mf_macs[i].bytes_out,
            NG_HOOK_NAME(hook), NG_HOOK_NAME(*next_hook));
#endif
    } else {
#ifdef NG_MACFILTER_DEBUG_RECVDATA
        MACFILTER_DEBUG("ether=" MAC_FMT " len=%db->?b: bytes: %s->%s",
            MAC_S_ARGS(ether), m->m_len - ETHER_HDR_LEN,
            NG_HOOK_NAME(hook), NG_HOOK_NAME(*next_hook));
#endif
    }

    mtx_unlock(&mfp->mtx);

    return 0;
}

/*
 * Search for the right upper hook, based on the source ethernet 
 * address.  If not found, pass to the default upper hook.
 */
static int
macfilter_ether_input(hook_p hook, macfilter_p mfp, struct mbuf *m, hook_p *next_hook)
{
    struct ether_header *ether_header = mtod(m, struct ether_header *);
    u_char *ether = ether_header->ether_shost;
    int hookid = NG_MACFILTER_HOOK_DEFAULT_ID;

    mtx_lock(&mfp->mtx);

    u_int i;
    int found = macfilter_find_mac(mfp, ether, &i);
    if (found) {
        mf_mac_p mf_macs = mfp->mf_macs;

        mf_macs[i].packets_in++;
        if (m->m_len > ETHER_HDR_LEN)
            mf_macs[i].bytes_in += m->m_len - ETHER_HDR_LEN;

        hookid = mf_macs[i].hookid;
    
#ifdef NG_MACFILTER_DEBUG_RECVDATA
        MACFILTER_DEBUG("ether=" MAC_FMT " len=%db->%lldb: bytes: %s->%s",
            MAC_S_ARGS(ether), m->m_len - ETHER_HDR_LEN, mf_macs[i].bytes_in,
            NG_HOOK_NAME(hook), NG_HOOK_NAME(*next_hook));
#endif
    } else {
#ifdef NG_MACFILTER_DEBUG_RECVDATA
        MACFILTER_DEBUG("ether=" MAC_FMT " len=%db->?b: bytes: %s->%s",
            MAC_S_ARGS(ether), m->m_len - ETHER_HDR_LEN,
            NG_HOOK_NAME(hook), NG_HOOK_NAME(*next_hook));
#endif
    }

    if (hookid >= mfp->mf_upper_cnt)
        *next_hook = NULL;
    else
        *next_hook = mfp->mf_upper[hookid];

    mtx_unlock(&mfp->mtx);

    return 0;
}

/*
 * ======================================================================
 * Netgraph hooks
 * ======================================================================
 */

/*
 * See basic netgraph code for comments on the individual functions.
 */

static int
ng_macfilter_constructor(node_p node)
{
    macfilter_p mfp = malloc(sizeof(*mfp), M_NETGRAPH, M_NOWAIT | M_ZERO);
    if (mfp == NULL)
	return ENOMEM;

    int error = macfilter_mactable_resize(mfp);
    if (error)
        return error;

    NG_NODE_SET_PRIVATE(node, mfp);

    mtx_init(&mfp->mtx, "Macfilter table", NULL, MTX_DEF);

    return (0);
}

static int
ng_macfilter_newhook(node_p node, hook_p hook, const char *hookname)
{
    const macfilter_p mfp = NG_NODE_PRIVATE(node);

    MACFILTER_DEBUG("%s", hookname);

    if (strcmp(hookname, NG_MACFILTER_HOOK_ETHER) == 0) {
	mfp->mf_ether_hook = hook;
    } else {
        int hookid;
        if (strcmp(hookname, NG_MACFILTER_HOOK_DEFAULT) == 0) {
            hookid = NG_MACFILTER_HOOK_DEFAULT_ID;
        } else {
            for (hookid = 1; hookid < mfp->mf_upper_cnt; hookid++)
                if (mfp->mf_upper[hookid] == NULL)
                    break;
        }

        if (hookid >= mfp->mf_upper_cnt) {
            MACFILTER_DEBUG("upper cnt %d -> %d", mfp->mf_upper_cnt, hookid + 1);

            mfp->mf_upper_cnt = hookid + 1;
            mfp->mf_upper = realloc(mfp->mf_upper,
                    sizeof(mfp->mf_upper[0])*mfp->mf_upper_cnt,
                    M_NETGRAPH, M_NOWAIT | M_ZERO);
        }

        mfp->mf_upper[hookid] = hook;
    }

    return(0);
}

static int
ng_macfilter_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
    const macfilter_p mfp = NG_NODE_PRIVATE(node);
    struct ng_mesg *resp = NULL;
    struct ng_mesg *msg;
    int error = 0;
    struct ngm_macfilter_macs *ngm_macs;
    struct ngm_macfilter_hooks *ngm_hooks;
    struct ngm_macfilter_direct *md;
    struct ngm_macfilter_direct_hookid *mdi;
    int n = 0, i = 0;
    int hookid = 0;
    int resplen;

    NGI_GET_MSG(item, msg);

    mtx_lock(&mfp->mtx);

    switch (msg->header.typecookie) {
    case NGM_MACFILTER_COOKIE: 
	switch (msg->header.cmd) {

	case NGM_MACFILTER_RESET:
	    macfilter_reset(mfp);
	    break;

	case NGM_MACFILTER_DIRECT:
	    if (msg->header.arglen != sizeof(struct ngm_macfilter_direct)) {
		MACFILTER_DEBUG("direct: wrong type length (%d, expected %zu)",
                      msg->header.arglen, sizeof(struct ngm_macfilter_direct));
		error = EINVAL;
		break;
	    }
            md = (struct ngm_macfilter_direct *)msg->data;
	    error = macfilter_direct(mfp, md);
	    break;
	case NGM_MACFILTER_DIRECT_HOOKID:
	    if (msg->header.arglen != sizeof(struct ngm_macfilter_direct_hookid)) {
		MACFILTER_DEBUG("direct hookid: wrong type length (%d, expected %zu)",
                      msg->header.arglen, sizeof(struct ngm_macfilter_direct));
		error = EINVAL;
		break;
	    }
            mdi = (struct ngm_macfilter_direct_hookid *)msg->data;
	    error = macfilter_direct_hookid(mfp, mdi);
	    break;

        case NGM_MACFILTER_GET_MACS:
        case NGM_MACFILTER_GETCLR_MACS:
            n = mfp->mf_mac_used;
            resplen = sizeof(struct ngm_macfilter_macs) + n * sizeof(struct ngm_macfilter_mac);
            NG_MKRESPONSE(resp, msg, resplen, M_NOWAIT);
            if (resp == NULL) {
                error = ENOMEM;
                break;
            }
            ngm_macs = (struct ngm_macfilter_macs *)resp->data;
            ngm_macs->n = n;
            bcopy(mfp->mf_macs, &ngm_macs->macs[0], n * sizeof(struct ngm_macfilter_mac));

            if (msg->header.cmd == NGM_MACFILTER_GETCLR_MACS)
                macfilter_reset_stats(mfp);
            break;

        case NGM_MACFILTER_CLR_MACS:
            macfilter_reset_stats(mfp);
            break;

        case NGM_MACFILTER_GET_HOOKS:
            for (hookid = 0; hookid < mfp->mf_upper_cnt; hookid++)
                if (mfp->mf_upper[hookid] != NULL)
                    n++;
            resplen = sizeof(struct ngm_macfilter_hooks) + n * sizeof(struct ngm_macfilter_hook);
            NG_MKRESPONSE(resp, msg, resplen, M_NOWAIT | M_ZERO);
            if (resp == NULL) {
                error = ENOMEM;
                break;
            }

            ngm_hooks = (struct ngm_macfilter_hooks *)resp->data;
            ngm_hooks->n = n;
            for (hookid = 0; hookid < mfp->mf_upper_cnt; hookid++) {
                if (mfp->mf_upper[hookid] != NULL) {
                    struct ngm_macfilter_hook *ngm_hook = &ngm_hooks->hooks[i++];
                    strlcpy(ngm_hook->hookname,
                            NG_HOOK_NAME(mfp->mf_upper[hookid]),
                            NG_HOOKSIZ);
                    ngm_hook->hookid = hookid;
                    ngm_hook->maccnt = macfilter_mac_count(mfp, hookid);
                }
            }
            break;

	default:
	    error = EINVAL;		/* unknown command */
	    break;
	}
	break;

    default:
	error = EINVAL;			/* unknown cookie type */
	break;
    }

    mtx_unlock(&mfp->mtx);

    NG_RESPOND_MSG(error, node, item, resp);
    NG_FREE_MSG(msg);

    return error;
}

static int
ng_macfilter_rcvdata(hook_p hook, item_p item)
{
    const macfilter_p mfp = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
    int error;
    hook_p next_hook = NULL;
    struct mbuf *m;

    m = NGI_M(item);	/* 'item' still owns it. We are peeking */
    MACFILTER_DEBUG("%s", NG_HOOK_NAME(hook));

    if (hook == mfp->mf_ether_hook)
	error = macfilter_ether_input(hook, mfp, m, &next_hook);
    else if (mfp->mf_ether_hook != NULL)
	error = macfilter_ether_output(hook, mfp, m, &next_hook);

    if (next_hook == NULL) {
        NG_FREE_ITEM(item);
        return (0);
    }

    NG_FWD_ITEM_HOOK(error, item, next_hook);

    return error;
}

static int
ng_macfilter_disconnect(hook_p hook)
{
    const macfilter_p mfp = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));

    mtx_lock(&mfp->mtx);

    if (mfp->mf_ether_hook == hook) {
        mfp->mf_ether_hook = NULL;

        MACFILTER_DEBUG("%s", NG_HOOK_NAME(hook));
    } else {
        int hookid;

        for (hookid = 0; hookid < mfp->mf_upper_cnt; hookid++) {
            if (mfp->mf_upper[hookid] == hook) {
                mfp->mf_upper[hookid] = NULL;

#ifndef NG_MACFILTER_DEBUG
                macfilter_mactable_remove_by_hookid(mfp, hookid);
#else
                int cnt = macfilter_mactable_remove_by_hookid(mfp, hookid);

                MACFILTER_DEBUG("%s: removed %d MACs", NG_HOOK_NAME(hook), cnt);
#endif
                break;
            }
        }

        if (hookid == mfp->mf_upper_cnt - 1) {
            /* Reduce the size of the array when the last element was removed */
            for (--hookid; hookid >= 0 && mfp->mf_upper[hookid] == NULL; hookid--)
                ;

            MACFILTER_DEBUG("upper cnt %d -> %d", mfp->mf_upper_cnt, hookid + 1);
            mfp->mf_upper_cnt = hookid + 1;
            mfp->mf_upper = realloc(mfp->mf_upper,
                    sizeof(mfp->mf_upper[0])*mfp->mf_upper_cnt,
                    M_NETGRAPH, M_NOWAIT | M_ZERO);
        }
    }

    mtx_unlock(&mfp->mtx);

    if ((NG_NODE_NUMHOOKS(NG_HOOK_NODE(hook)) == 0)
	&& (NG_NODE_IS_VALID(NG_HOOK_NODE(hook)))) {
	ng_rmnode_self(NG_HOOK_NODE(hook));
    }

    return (0);
}

static int
ng_macfilter_shutdown(node_p node)
{
    const macfilter_p mfp = NG_NODE_PRIVATE(node);

    mtx_destroy(&mfp->mtx);
    free(mfp->mf_upper, M_NETGRAPH);
    free(mfp->mf_macs, M_NETGRAPH);
    free(mfp, M_NETGRAPH);

    NG_NODE_UNREF(node);

    return (0);
}
