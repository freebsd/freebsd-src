/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019-2021 IKS Service GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 * Author: Lutz Donnerhacke <lutz@donnerhacke.de>
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/ctype.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/counter.h>

#include <net/ethernet.h>

#include <netgraph/ng_message.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_vlan_rotate.h>
#include <netgraph/netgraph.h>

/*
 * This section contains the netgraph method declarations for the
 * sample node. These methods define the netgraph 'type'.
 */

static ng_constructor_t ng_vlanrotate_constructor;
static ng_rcvmsg_t ng_vlanrotate_rcvmsg;
static ng_shutdown_t ng_vlanrotate_shutdown;
static ng_newhook_t ng_vlanrotate_newhook;
static ng_rcvdata_t ng_vlanrotate_rcvdata;
static ng_disconnect_t ng_vlanrotate_disconnect;

/* Parse type for struct ng_vlanrotate_conf */
static const struct ng_parse_struct_field ng_vlanrotate_conf_fields[] = {
	{"rot", &ng_parse_int8_type},
	{"min", &ng_parse_uint8_type},
	{"max", &ng_parse_uint8_type},
	{NULL}
};
static const struct ng_parse_type ng_vlanrotate_conf_type = {
	&ng_parse_struct_type,
	&ng_vlanrotate_conf_fields
};

/* Parse type for struct ng_vlanrotate_stat */
static struct ng_parse_fixedarray_info ng_vlanrotate_stat_hist_info = {
	&ng_parse_uint64_type,
	NG_VLANROTATE_MAX_VLANS
};
static struct ng_parse_type ng_vlanrotate_stat_hist = {
	&ng_parse_fixedarray_type,
	&ng_vlanrotate_stat_hist_info
};
static const struct ng_parse_struct_field ng_vlanrotate_stat_fields[] = {
	{"drops", &ng_parse_uint64_type},
	{"excessive", &ng_parse_uint64_type},
	{"incomplete", &ng_parse_uint64_type},
	{"histogram", &ng_vlanrotate_stat_hist},
	{NULL}
};
static struct ng_parse_type ng_vlanrotate_stat_type = {
	&ng_parse_struct_type,
	&ng_vlanrotate_stat_fields
};


/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_vlanrotate_cmdlist[] = {
	{
		NGM_VLANROTATE_COOKIE,
		NGM_VLANROTATE_GET_CONF,
		"getconf",
		NULL,
		&ng_vlanrotate_conf_type,
	},
	{
		NGM_VLANROTATE_COOKIE,
		NGM_VLANROTATE_SET_CONF,
		"setconf",
		&ng_vlanrotate_conf_type,
		NULL
	},
	{
		NGM_VLANROTATE_COOKIE,
		NGM_VLANROTATE_GET_STAT,
		"getstat",
		NULL,
		&ng_vlanrotate_stat_type
	},
	{
		NGM_VLANROTATE_COOKIE,
		NGM_VLANROTATE_CLR_STAT,
		"clrstat",
		NULL,
		&ng_vlanrotate_stat_type
	},
	{
		NGM_VLANROTATE_COOKIE,
		NGM_VLANROTATE_GETCLR_STAT,
		"getclrstat",
		NULL,
		&ng_vlanrotate_stat_type
	},
	{0}
};

/* Netgraph node type descriptor */
static struct ng_type typestruct = {
	.version = NG_ABI_VERSION,
	.name = NG_VLANROTATE_NODE_TYPE,
	.constructor = ng_vlanrotate_constructor,
	.rcvmsg = ng_vlanrotate_rcvmsg,
	.shutdown = ng_vlanrotate_shutdown,
	.newhook = ng_vlanrotate_newhook,
	.rcvdata = ng_vlanrotate_rcvdata,
	.disconnect = ng_vlanrotate_disconnect,
	.cmdlist = ng_vlanrotate_cmdlist,
};
NETGRAPH_INIT(vlanrotate, &typestruct);

struct ng_vlanrotate_kernel_stats {
	counter_u64_t	drops, excessive, incomplete;
	counter_u64_t	histogram[NG_VLANROTATE_MAX_VLANS];
};

/* Information we store for each node */
struct vlanrotate {
	hook_p		original_hook;
	hook_p		ordered_hook;
	hook_p		excessive_hook;
	hook_p		incomplete_hook;
	struct ng_vlanrotate_conf conf;
	struct ng_vlanrotate_kernel_stats stats;
};
typedef struct vlanrotate *vlanrotate_p;

/*
 * Set up the private data structure.
 */
static int
ng_vlanrotate_constructor(node_p node)
{
	int i;

	vlanrotate_p vrp = malloc(sizeof(*vrp), M_NETGRAPH, M_WAITOK | M_ZERO);

	vrp->conf.max = NG_VLANROTATE_MAX_VLANS;

	vrp->stats.drops = counter_u64_alloc(M_WAITOK);
	vrp->stats.excessive = counter_u64_alloc(M_WAITOK);
	vrp->stats.incomplete = counter_u64_alloc(M_WAITOK);
	for (i = 0; i < NG_VLANROTATE_MAX_VLANS; i++)
		vrp->stats.histogram[i] = counter_u64_alloc(M_WAITOK);

	NG_NODE_SET_PRIVATE(node, vrp);
	return (0);
}

/*
 * Give our ok for a hook to be added.
 */
static int
ng_vlanrotate_newhook(node_p node, hook_p hook, const char *name)
{
	const vlanrotate_p vrp = NG_NODE_PRIVATE(node);
	hook_p *dst = NULL;

	if (strcmp(name, NG_VLANROTATE_HOOK_ORDERED) == 0) {
		dst = &vrp->ordered_hook;
	} else if (strcmp(name, NG_VLANROTATE_HOOK_ORIGINAL) == 0) {
		dst = &vrp->original_hook;
	} else if (strcmp(name, NG_VLANROTATE_HOOK_EXCESSIVE) == 0) {
		dst = &vrp->excessive_hook;
	} else if (strcmp(name, NG_VLANROTATE_HOOK_INCOMPLETE) == 0) {
		dst = &vrp->incomplete_hook;
	} else
		return (EINVAL);	/* not a hook we know about */

	if (*dst != NULL)
		return (EADDRINUSE);	/* don't override */

	*dst = hook;
	return (0);
}

/*
 * Get a netgraph control message.
 * A response is not required.
 */
static int
ng_vlanrotate_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	const vlanrotate_p vrp = NG_NODE_PRIVATE(node);
	struct ng_mesg *resp = NULL;
	struct ng_mesg *msg;
	struct ng_vlanrotate_conf *pcf;
	int error = 0;

	NGI_GET_MSG(item, msg);
	/* Deal with message according to cookie and command */
	switch (msg->header.typecookie) {
	case NGM_VLANROTATE_COOKIE:
		switch (msg->header.cmd) {
		case NGM_VLANROTATE_GET_CONF:
			NG_MKRESPONSE(resp, msg, sizeof(vrp->conf), M_NOWAIT);
			if (!resp) {
				error = ENOMEM;
				break;
			}
			*((struct ng_vlanrotate_conf *)resp->data) = vrp->conf;
			break;
		case NGM_VLANROTATE_SET_CONF:
			if (msg->header.arglen != sizeof(*pcf)) {
				error = EINVAL;
				break;
			}

			pcf = (struct ng_vlanrotate_conf *)msg->data;

			if (pcf->max == 0)	/* keep current value */
				pcf->max = vrp->conf.max;

			if ((pcf->max > NG_VLANROTATE_MAX_VLANS) ||
			    (pcf->min > pcf->max) ||
			    (abs(pcf->rot) >= pcf->max)) {
				error = EINVAL;
				break;
			}

			vrp->conf = *pcf;
			break;
		case NGM_VLANROTATE_GET_STAT:
		case NGM_VLANROTATE_GETCLR_STAT:
		{
			struct ng_vlanrotate_stat *p;
			int i;

			NG_MKRESPONSE(resp, msg, sizeof(*p), M_NOWAIT);
			if (!resp) {
				error = ENOMEM;
				break;
			}
			p = (struct ng_vlanrotate_stat *)resp->data;
			p->drops = counter_u64_fetch(vrp->stats.drops);
			p->excessive = counter_u64_fetch(vrp->stats.excessive);
			p->incomplete = counter_u64_fetch(vrp->stats.incomplete);
			for (i = 0; i < NG_VLANROTATE_MAX_VLANS; i++)
				p->histogram[i] = counter_u64_fetch(vrp->stats.histogram[i]);
			if (msg->header.cmd != NGM_VLANROTATE_GETCLR_STAT)
				break;
		}
		case NGM_VLANROTATE_CLR_STAT:
		{
			int i;

			counter_u64_zero(vrp->stats.drops);
			counter_u64_zero(vrp->stats.excessive);
			counter_u64_zero(vrp->stats.incomplete);
			for (i = 0; i < NG_VLANROTATE_MAX_VLANS; i++)
				counter_u64_zero(vrp->stats.histogram[i]);
			break;
		}
		default:
			error = EINVAL;	/* unknown command */
			break;
		}
		break;
	default:
		error = EINVAL;	/* unknown cookie type */
		break;
	}

	/* Take care of synchronous response, if any */
	NG_RESPOND_MSG(error, node, item, resp);
	/* Free the message and return */
	NG_FREE_MSG(msg);
	return (error);
}

/*
 * Receive data, and do rotate the vlans as desired.
 *
 * Rotating is quite complicated if the rotation offset and the number
 * of vlans are not relativly prime. In this case multiple slices need
 * to be rotated separately.
 *
 * Rotation can be additive or subtractive. Some examples:
 *  01234   5 vlans given
 *  -----
 *  34012  +2 rotate
 *  12340  +4 rotate
 *  12340  -1 rotate
 *
 * First some helper functions ...
 */

struct ether_vlan_stack_entry {
	uint16_t	proto;
	uint16_t	tag;
}		__packed;

struct ether_vlan_stack_header {
	uint8_t		dst[ETHER_ADDR_LEN];
	uint8_t		src[ETHER_ADDR_LEN];
	struct ether_vlan_stack_entry vlan_stack[1];
}		__packed;

static int
ng_vlanrotate_gcd(int a, int b)
{
	if (b == 0)
		return a;
	else
		return ng_vlanrotate_gcd(b, a % b);
}

static void
ng_vlanrotate_rotate(struct ether_vlan_stack_entry arr[], int d, int n)
{
	int		i, j, k;
	struct ether_vlan_stack_entry temp;

	/* for each commensurable slice */
	for (i = ng_vlanrotate_gcd(d, n); i-- > 0;) {
		/* rotate left aka downwards */
		temp = arr[i];
		j = i;

		while (1) {
			k = j + d;
			if (k >= n)
				k = k - n;
			if (k == i)
				break;
			arr[j] = arr[k];
			j = k;
		}

		arr[j] = temp;
	}
}

static int
ng_vlanrotate_rcvdata(hook_p hook, item_p item)
{
	const vlanrotate_p vrp = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct ether_vlan_stack_header *evsh;
	struct mbuf *m = NULL;
	hook_p	dst_hook;
	int8_t	rotate;
	int8_t	vlans = 0;
	int	error = ENOSYS;

	NGI_GET_M(item, m);

	if (hook == vrp->ordered_hook) {
		rotate = +vrp->conf.rot;
		dst_hook = vrp->original_hook;
	} else if (hook == vrp->original_hook) {
		rotate = -vrp->conf.rot;
		dst_hook = vrp->ordered_hook;
	} else {
		dst_hook = vrp->original_hook;
		goto send;	/* everything else goes out unmodified */
	}

	if (dst_hook == NULL) {
		error = ENETDOWN;
		goto fail;
	}

	/* count the vlans */
	for (vlans = 0; vlans <= NG_VLANROTATE_MAX_VLANS; vlans++) {
		size_t expected_len = sizeof(struct ether_vlan_stack_header)
		    + vlans * sizeof(struct ether_vlan_stack_entry);

		if (m->m_len < expected_len) {
			m = m_pullup(m, expected_len);
			if (m == NULL) {
				error = EINVAL;
				goto fail;
			}
		}

		evsh = mtod(m, struct ether_vlan_stack_header *);
		switch (ntohs(evsh->vlan_stack[vlans].proto)) {
		case ETHERTYPE_VLAN:
		case ETHERTYPE_QINQ:
		case ETHERTYPE_8021Q9100:
		case ETHERTYPE_8021Q9200:
		case ETHERTYPE_8021Q9300:
			break;
		default:
			goto out;
		}
	}
out:
	if ((vlans > vrp->conf.max) || (vlans >= NG_VLANROTATE_MAX_VLANS)) {
		counter_u64_add(vrp->stats.excessive, 1);
		dst_hook = vrp->excessive_hook;
		goto send;
	}

	if ((vlans < vrp->conf.min) || (vlans <= abs(rotate))) {
		counter_u64_add(vrp->stats.incomplete, 1);
		dst_hook = vrp->incomplete_hook;
		goto send;
	}
	counter_u64_add(vrp->stats.histogram[vlans], 1);

	/* rotating upwards always (using modular arithmetics) */
	if (rotate == 0) {
		/* nothing to do */
	} else if (rotate > 0) {
		ng_vlanrotate_rotate(evsh->vlan_stack, rotate, vlans);
	} else {
		ng_vlanrotate_rotate(evsh->vlan_stack, vlans + rotate, vlans);
	}

send:
	if (dst_hook == NULL)
		goto fail;
	NG_FWD_NEW_DATA(error, item, dst_hook, m);
	return 0;

fail:
	counter_u64_add(vrp->stats.drops, 1);
	if (m != NULL)
		m_freem(m);
	NG_FREE_ITEM(item);
	return (error);
}

/*
 * Do local shutdown processing..
 * All our links and the name have already been removed.
 */
static int
ng_vlanrotate_shutdown(node_p node)
{
	const		vlanrotate_p vrp = NG_NODE_PRIVATE(node);
	int i;

	NG_NODE_SET_PRIVATE(node, NULL);

	counter_u64_free(vrp->stats.drops);
	counter_u64_free(vrp->stats.excessive);
	counter_u64_free(vrp->stats.incomplete);
	for (i = 0; i < NG_VLANROTATE_MAX_VLANS; i++)
		counter_u64_free(vrp->stats.histogram[i]);

	free(vrp, M_NETGRAPH);

	NG_NODE_UNREF(node);
	return (0);
}

/*
 * Hook disconnection
 * For this type, removal of the last link destroys the node
 */
static int
ng_vlanrotate_disconnect(hook_p hook)
{
	const		vlanrotate_p vrp = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));

	if (vrp->original_hook == hook)
		vrp->original_hook = NULL;
	if (vrp->ordered_hook == hook)
		vrp->ordered_hook = NULL;
	if (vrp->excessive_hook == hook)
		vrp->excessive_hook = NULL;
	if (vrp->incomplete_hook == hook)
		vrp->incomplete_hook = NULL;

	/* during shutdown the node is invalid, don't shutdown twice */
	if ((NG_NODE_NUMHOOKS(NG_HOOK_NODE(hook)) == 0) &&
	    (NG_NODE_IS_VALID(NG_HOOK_NODE(hook))))
		ng_rmnode_self(NG_HOOK_NODE(hook));
	return (0);
}
