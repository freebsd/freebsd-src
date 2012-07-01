/*-
 * Copyright (C) 2010 by Maxim Ignatenko <gelraen.ua@gmail.com>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <netgraph/ng_message.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_patch.h>
#include <netgraph/netgraph.h>

static ng_constructor_t	ng_patch_constructor;
static ng_rcvmsg_t	ng_patch_rcvmsg;
static ng_shutdown_t	ng_patch_shutdown;
static ng_newhook_t	ng_patch_newhook;
static ng_rcvdata_t	ng_patch_rcvdata;
static ng_disconnect_t	ng_patch_disconnect;

static int
ng_patch_config_getlen(const struct ng_parse_type *type,
    const u_char *start, const u_char *buf)
{
	const struct ng_patch_config *p;

	p = (const struct ng_patch_config *)(buf -
	    offsetof(struct ng_patch_config, ops));
	return (p->count);
}

static const struct ng_parse_struct_field ng_patch_op_type_fields[]
	= NG_PATCH_OP_TYPE_INFO;
static const struct ng_parse_type ng_patch_op_type = {
	&ng_parse_struct_type,
	&ng_patch_op_type_fields
};

static const struct ng_parse_array_info ng_patch_confarr_info = {
	&ng_patch_op_type,
	&ng_patch_config_getlen
};
static const struct ng_parse_type ng_patch_confarr_type = {
	&ng_parse_array_type,
	&ng_patch_confarr_info
};

static const struct ng_parse_struct_field ng_patch_config_type_fields[]
	= NG_PATCH_CONFIG_TYPE_INFO;
static const struct ng_parse_type ng_patch_config_type = {
	&ng_parse_struct_type,
	&ng_patch_config_type_fields
};

static const struct ng_parse_struct_field ng_patch_stats_fields[]
	= NG_PATCH_STATS_TYPE_INFO;
static const struct ng_parse_type ng_patch_stats_type = {
	&ng_parse_struct_type,
	&ng_patch_stats_fields
};

static const struct ng_cmdlist ng_patch_cmdlist[] = {
	{
		NGM_PATCH_COOKIE,
		NGM_PATCH_GETCONFIG,
		"getconfig",
		NULL,
		&ng_patch_config_type
	},
	{
		NGM_PATCH_COOKIE,
		NGM_PATCH_SETCONFIG,
		"setconfig",
		&ng_patch_config_type,
		NULL
	},
	{
		NGM_PATCH_COOKIE,
		NGM_PATCH_GET_STATS,
		"getstats",
		NULL,
		&ng_patch_stats_type
	},
	{
		NGM_PATCH_COOKIE,
		NGM_PATCH_CLR_STATS,
		"clrstats",
		NULL,
		NULL
	},
	{
		NGM_PATCH_COOKIE,
		NGM_PATCH_GETCLR_STATS,
		"getclrstats",
		NULL,
		&ng_patch_stats_type
	},
	{ 0 }
};

static struct ng_type typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		NG_PATCH_NODE_TYPE,
	.constructor =	ng_patch_constructor,
	.rcvmsg =	ng_patch_rcvmsg,
	.shutdown =	ng_patch_shutdown,
	.newhook =	ng_patch_newhook,
	.rcvdata =	ng_patch_rcvdata,
	.disconnect =	ng_patch_disconnect,
	.cmdlist =	ng_patch_cmdlist,
};
NETGRAPH_INIT(patch, &typestruct);

union patch_val {
	uint8_t		v1;
	uint16_t	v2;
	uint32_t	v4;
	uint64_t	v8;
};

struct ng_patch_priv {
	hook_p		in;
	hook_p		out;
	struct ng_patch_config *config;
	union patch_val *val;
	struct ng_patch_stats stats;
};
typedef struct ng_patch_priv *priv_p;

#define	NG_PATCH_CONF_SIZE(count)	(sizeof(struct ng_patch_config) + \
		(count) * sizeof(struct ng_patch_op))

static void do_patch(priv_p conf, struct mbuf *m);

static int
ng_patch_constructor(node_p node)
{
	priv_p privdata;

	privdata = malloc(sizeof(*privdata), M_NETGRAPH, M_WAITOK | M_ZERO);
	NG_NODE_SET_PRIVATE(node, privdata);
	privdata->in = NULL;
	privdata->out = NULL;
	privdata->config = NULL;
	return (0);
}

static int
ng_patch_newhook(node_p node, hook_p hook, const char *name)
{
	const priv_p privp = NG_NODE_PRIVATE(node);

	if (strncmp(name, NG_PATCH_HOOK_IN, strlen(NG_PATCH_HOOK_IN)) == 0) {
		privp->in = hook;
	} else if (strncmp(name, NG_PATCH_HOOK_OUT,
	    strlen(NG_PATCH_HOOK_OUT)) == 0) {
		privp->out = hook;
	} else
		return (EINVAL);
	return(0);
}

static int
ng_patch_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	const priv_p privp = NG_NODE_PRIVATE(node);
	struct ng_patch_config *conf, *newconf;
	union patch_val *newval;
	struct ng_mesg *msg;
	struct ng_mesg *resp;
	int i, clear, error;

	clear = error = 0;
	resp = NULL;
	NGI_GET_MSG(item, msg);
	switch (msg->header.typecookie) {
	case NGM_PATCH_COOKIE:
		switch (msg->header.cmd) {
		case NGM_PATCH_GETCONFIG:
			if (privp->config == NULL)
				break;
			NG_MKRESPONSE(resp, msg,
			    NG_PATCH_CONF_SIZE(privp->config->count),
			    M_WAITOK);
			bcopy(privp->config, resp->data,
			    NG_PATCH_CONF_SIZE(privp->config->count));
			break;
		case NGM_PATCH_SETCONFIG:
		    {
			if (msg->header.arglen <
			    sizeof(struct ng_patch_config)) {
				error = EINVAL;
				break;
			}

			conf = (struct ng_patch_config *)msg->data;
			if (msg->header.arglen <
			    NG_PATCH_CONF_SIZE(conf->count)) {
				error = EINVAL;
				break;
			}

			for(i = 0; i < conf->count; i++) {
				switch(conf->ops[i].length) {
				case 1:
				case 2:
				case 4:
				case 8:
					break;
				default:
					error = EINVAL;
					break;
				}
				if (error != 0)
					break;
			}

			conf->csum_flags &= CSUM_IP | CSUM_TCP | CSUM_UDP |
			    CSUM_SCTP;

			if (error == 0) {
				newconf = malloc(
				    NG_PATCH_CONF_SIZE(conf->count),
				    M_NETGRAPH, M_WAITOK);
				newval = malloc(conf->count *
				    sizeof(union patch_val), M_NETGRAPH,
				    M_WAITOK);
				for(i = 0; i < conf->count; i++) {
					switch (conf->ops[i].length) {
					case 1:
						newval[i].v1 =
						    conf->ops[i].value;
						break;
					case 2:
						newval[i].v2 =
						    conf->ops[i].value;
						break;
					case 4:
						newval[i].v4 =
						    conf->ops[i].value;
						break;
					case 8:
						newval[i].v8 =
						    conf->ops[i].value;
						break;
					}
				}
				bcopy(conf, newconf,
				    NG_PATCH_CONF_SIZE(conf->count));
				if (privp->val != NULL)
					free(privp->val, M_NETGRAPH);
				privp->val = newval;
				if (privp->config != NULL)
					free(privp->config, M_NETGRAPH);
				privp->config = newconf;
			}
			break;
		    }
		case NGM_PATCH_GETCLR_STATS:
			clear = 1;
			/* FALLTHROUGH */
		case NGM_PATCH_GET_STATS:
			NG_MKRESPONSE(resp, msg, sizeof(struct ng_patch_stats),
			    M_WAITOK);
			bcopy(&(privp->stats), resp->data,
			    sizeof(struct ng_patch_stats));
			if (clear == 0)
				break;
			/* else FALLTHROUGH */
		case NGM_PATCH_CLR_STATS:
			bzero(&(privp->stats), sizeof(struct ng_patch_stats));
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	default:
		error = EINVAL;
		break;
	}

	NG_RESPOND_MSG(error, node, item, resp);
	NG_FREE_MSG(msg);
	return(error);
}

static void
do_patch(priv_p privp, struct mbuf *m)
{
	struct ng_patch_config *conf;
	uint64_t buf;
	int i, patched;

	conf = privp->config;
	patched = 0;
	for(i = 0; i < conf->count; i++) {
		if (conf->ops[i].offset + conf->ops[i].length >
		    m->m_pkthdr.len)
			continue;

		/* for "=" operation we don't need to copy data from mbuf */
		if (conf->ops[i].mode != NG_PATCH_MODE_SET) {
			m_copydata(m, conf->ops[i].offset,
			    conf->ops[i].length, (caddr_t)&buf);
		}

		switch (conf->ops[i].length) {
		case 1:
			switch (conf->ops[i].mode) {
			case NG_PATCH_MODE_SET:
				*((uint8_t *)&buf) = privp->val[i].v1;
				break;
			case NG_PATCH_MODE_ADD:
				*((uint8_t *)&buf) += privp->val[i].v1;
				break;
			case NG_PATCH_MODE_SUB:
				*((uint8_t *)&buf) -= privp->val[i].v1;
				break;
			case NG_PATCH_MODE_MUL:
				*((uint8_t *)&buf) *= privp->val[i].v1;
				break;
			case NG_PATCH_MODE_DIV:
				*((uint8_t *)&buf) /= privp->val[i].v1;
				break;
			case NG_PATCH_MODE_NEG:
				*((int8_t *)&buf) = - *((int8_t *)&buf);
				break;
			case NG_PATCH_MODE_AND:
				*((uint8_t *)&buf) &= privp->val[i].v1;
				break;
			case NG_PATCH_MODE_OR:
				*((uint8_t *)&buf) |= privp->val[i].v1;
				break;
			case NG_PATCH_MODE_XOR:
				*((uint8_t *)&buf) ^= privp->val[i].v1;
				break;
			case NG_PATCH_MODE_SHL:
				*((uint8_t *)&buf) <<= privp->val[i].v1;
				break;
			case NG_PATCH_MODE_SHR:
				*((uint8_t *)&buf) >>= privp->val[i].v1;
				break;
			}
			break;
		case 2:
			*((int16_t *)&buf) =  ntohs(*((int16_t *)&buf));
			switch (conf->ops[i].mode) {
			case NG_PATCH_MODE_SET:
				*((uint16_t *)&buf) = privp->val[i].v2;
				break;
			case NG_PATCH_MODE_ADD:
				*((uint16_t *)&buf) += privp->val[i].v2;
				break;
			case NG_PATCH_MODE_SUB:
				*((uint16_t *)&buf) -= privp->val[i].v2;
				break;
			case NG_PATCH_MODE_MUL:
				*((uint16_t *)&buf) *= privp->val[i].v2;
				break;
			case NG_PATCH_MODE_DIV:
				*((uint16_t *)&buf) /= privp->val[i].v2;
				break;
			case NG_PATCH_MODE_NEG:
				*((int16_t *)&buf) = - *((int16_t *)&buf);
				break;
			case NG_PATCH_MODE_AND:
				*((uint16_t *)&buf) &= privp->val[i].v2;
				break;
			case NG_PATCH_MODE_OR:
				*((uint16_t *)&buf) |= privp->val[i].v2;
				break;
			case NG_PATCH_MODE_XOR:
				*((uint16_t *)&buf) ^= privp->val[i].v2;
				break;
			case NG_PATCH_MODE_SHL:
				*((uint16_t *)&buf) <<= privp->val[i].v2;
				break;
			case NG_PATCH_MODE_SHR:
				*((uint16_t *)&buf) >>= privp->val[i].v2;
				break;
			}
			*((int16_t *)&buf) =  htons(*((int16_t *)&buf));
			break;
		case 4:
			*((int32_t *)&buf) =  ntohl(*((int32_t *)&buf));
			switch (conf->ops[i].mode) {
			case NG_PATCH_MODE_SET:
				*((uint32_t *)&buf) = privp->val[i].v4;
				break;
			case NG_PATCH_MODE_ADD:
				*((uint32_t *)&buf) += privp->val[i].v4;
				break;
			case NG_PATCH_MODE_SUB:
				*((uint32_t *)&buf) -= privp->val[i].v4;
				break;
			case NG_PATCH_MODE_MUL:
				*((uint32_t *)&buf) *= privp->val[i].v4;
				break;
			case NG_PATCH_MODE_DIV:
				*((uint32_t *)&buf) /= privp->val[i].v4;
				break;
			case NG_PATCH_MODE_NEG:
				*((int32_t *)&buf) = - *((int32_t *)&buf);
				break;
			case NG_PATCH_MODE_AND:
				*((uint32_t *)&buf) &= privp->val[i].v4;
				break;
			case NG_PATCH_MODE_OR:
				*((uint32_t *)&buf) |= privp->val[i].v4;
				break;
			case NG_PATCH_MODE_XOR:
				*((uint32_t *)&buf) ^= privp->val[i].v4;
				break;
			case NG_PATCH_MODE_SHL:
				*((uint32_t *)&buf) <<= privp->val[i].v4;
				break;
			case NG_PATCH_MODE_SHR:
				*((uint32_t *)&buf) >>= privp->val[i].v4;
				break;
			}
			*((int32_t *)&buf) =  htonl(*((int32_t *)&buf));
			break;
		case 8:
			*((int64_t *)&buf) =  be64toh(*((int64_t *)&buf));
			switch (conf->ops[i].mode) {
			case NG_PATCH_MODE_SET:
				*((uint64_t *)&buf) = privp->val[i].v8;
				break;
			case NG_PATCH_MODE_ADD:
				*((uint64_t *)&buf) += privp->val[i].v8;
				break;
			case NG_PATCH_MODE_SUB:
				*((uint64_t *)&buf) -= privp->val[i].v8;
				break;
			case NG_PATCH_MODE_MUL:
				*((uint64_t *)&buf) *= privp->val[i].v8;
				break;
			case NG_PATCH_MODE_DIV:
				*((uint64_t *)&buf) /= privp->val[i].v8;
				break;
			case NG_PATCH_MODE_NEG:
				*((int64_t *)&buf) = - *((int64_t *)&buf);
				break;
			case NG_PATCH_MODE_AND:
				*((uint64_t *)&buf) &= privp->val[i].v8;
				break;
			case NG_PATCH_MODE_OR:
				*((uint64_t *)&buf) |= privp->val[i].v8;
				break;
			case NG_PATCH_MODE_XOR:
				*((uint64_t *)&buf) ^= privp->val[i].v8;
				break;
			case NG_PATCH_MODE_SHL:
				*((uint64_t *)&buf) <<= privp->val[i].v8;
				break;
			case NG_PATCH_MODE_SHR:
				*((uint64_t *)&buf) >>= privp->val[i].v8;
				break;
			}
			*((int64_t *)&buf) =  htobe64(*((int64_t *)&buf));
			break;
		}

		m_copyback(m, conf->ops[i].offset, conf->ops[i].length,
		    (caddr_t)&buf);
		patched = 1;
	}
	if (patched > 0)
		privp->stats.patched++;
}

static int
ng_patch_rcvdata(hook_p hook, item_p item)
{
	const priv_p priv = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct mbuf *m;
	hook_p target;
	int error;

	priv->stats.received++;
	NGI_GET_M(item, m);
	if (priv->config != NULL && hook == priv->in &&
	    (m->m_flags & M_PKTHDR) != 0) {
		m = m_unshare(m,M_NOWAIT);
		if (m == NULL) {
			priv->stats.dropped++;
			NG_FREE_ITEM(item);
			return (ENOMEM);
		}
		do_patch(priv, m);
		m->m_pkthdr.csum_flags |= priv->config->csum_flags;
	}

	target = NULL;
	if (hook == priv->in) {
		/* return frames on 'in' hook if 'out' not connected */
		if (priv->out != NULL)
			target = priv->out;
		else
			target = priv->in;
	}
	if (hook == priv->out && priv->in != NULL)
		target = priv->in;

	if (target == NULL) {
		priv->stats.dropped++;
		NG_FREE_ITEM(item);
		NG_FREE_M(m);
		return (0);
	}
	NG_FWD_NEW_DATA(error, item, target, m);
	return (error);
}

static int
ng_patch_shutdown(node_p node)
{
	const priv_p privdata = NG_NODE_PRIVATE(node);

	if (privdata->val != NULL)
		free(privdata->val, M_NETGRAPH);
	if (privdata->config != NULL)
		free(privdata->config, M_NETGRAPH);
	NG_NODE_SET_PRIVATE(node, NULL);
	NG_NODE_UNREF(node);
	free(privdata, M_NETGRAPH);
	return (0);
}

static int
ng_patch_disconnect(hook_p hook)
{
	priv_p priv;

	priv = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	if (hook == priv->in) {
		priv->in = NULL;
	}
	if (hook == priv->out) {
		priv->out = NULL;
	}
	if (NG_NODE_NUMHOOKS(NG_HOOK_NODE(hook)) == 0 &&
	    NG_NODE_IS_VALID(NG_HOOK_NODE(hook))) /* already shutting down? */
		ng_rmnode_self(NG_HOOK_NODE(hook));
	return (0);
}

