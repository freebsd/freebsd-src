
/*
 * ng_bpf.c
 *
 * Copyright (c) 1999 Whistle Communications, Inc.
 * All rights reserved.
 * 
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Whistle Communications;
 * provided, however, that:
 * 1. Any and all reproductions of the source or object code must include the
 *    copyright notice above and the following disclaimer of warranties; and
 * 2. No rights are granted, in any manner or form, to use Whistle
 *    Communications, Inc. trademarks, including the mark "WHISTLE
 *    COMMUNICATIONS" on advertising, endorsements, or otherwise except as
 *    such appears in the above copyright notice or in the software.
 * 
 * THIS SOFTWARE IS BEING PROVIDED BY WHISTLE COMMUNICATIONS "AS IS", AND
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, WHISTLE COMMUNICATIONS MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE,
 * INCLUDING WITHOUT LIMITATION, ANY AND ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 * WHISTLE COMMUNICATIONS DOES NOT WARRANT, GUARANBPF, OR MAKE ANY
 * REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE USE OF THIS
 * SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY OR OTHERWISE.
 * IN NO EVENT SHALL WHISTLE COMMUNICATIONS BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF WHISTLE COMMUNICATIONS IS ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * Author: Archie Cobbs <archie@whistle.com>
 *
 * $FreeBSD: src/sys/netgraph/ng_bpf.c,v 1.2.4.1 2000/08/22 18:52:16 archie Exp $
 * $Whistle: ng_bpf.c,v 1.3 1999/12/03 20:30:23 archie Exp $
 */

/*
 * BPF NETGRAPH NODE TYPE
 *
 * This node type accepts any number of hook connections.  With each hook
 * is associated a bpf(4) filter program, and two hook names (each possibly
 * the empty string).  Incoming packets are compared against the filter;
 * matching packets are delivered out the first named hook (or dropped if
 * the empty string), and non-matching packets are delivered out the second
 * named hook (or dropped if the empty string).
 *
 * Each hook also keeps statistics about how many packets have matched, etc.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>

#include <net/bpf.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_bpf.h>

#define OFFSETOF(s, e) ((char *)&((s *)0)->e - (char *)((s *)0))

#define ERROUT(x)	do { error = (x); goto done; } while (0)

/* Per hook private info */
struct ng_bpf_hookinfo {
	node_p			node;
	hook_p			hook;
	struct ng_bpf_hookprog	*prog;
	struct ng_bpf_hookstat	stats;
};
typedef struct ng_bpf_hookinfo *hinfo_p;

/* Netgraph methods */
static ng_constructor_t	ng_bpf_constructor;
static ng_rcvmsg_t	ng_bpf_rcvmsg;
static ng_shutdown_t	ng_bpf_rmnode;
static ng_newhook_t	ng_bpf_newhook;
static ng_rcvdata_t	ng_bpf_rcvdata;
static ng_disconnect_t	ng_bpf_disconnect;

/* Internal helper functions */
static int	ng_bpf_setprog(hook_p hook, const struct ng_bpf_hookprog *hp);

/* Parse type for one struct bfp_insn */
static const struct ng_parse_struct_info ng_bpf_insn_type_info = {
    {
	{ "code",	&ng_parse_hint16_type	},
	{ "jt",		&ng_parse_uint8_type	},
	{ "jf",		&ng_parse_uint8_type	},
	{ "k",		&ng_parse_uint32_type	},
	{ NULL }
    }
};
static const struct ng_parse_type ng_bpf_insn_type = {
	&ng_parse_struct_type,
	&ng_bpf_insn_type_info
};

/* Parse type for the field 'bpf_prog' in struct ng_bpf_hookprog */
static int
ng_bpf_hookprogary_getLength(const struct ng_parse_type *type,
	const u_char *start, const u_char *buf)
{
	const struct ng_bpf_hookprog *hp;

	hp = (const struct ng_bpf_hookprog *)
	    (buf - OFFSETOF(struct ng_bpf_hookprog, bpf_prog));
	return hp->bpf_prog_len;
}

static const struct ng_parse_array_info ng_bpf_hookprogary_info = {
	&ng_bpf_insn_type,
	&ng_bpf_hookprogary_getLength,
	NULL
};
static const struct ng_parse_type ng_bpf_hookprogary_type = {
	&ng_parse_array_type,
	&ng_bpf_hookprogary_info
};

/* Parse type for struct ng_bpf_hookprog */
static const struct ng_parse_struct_info ng_bpf_hookprog_type_info
	= NG_BPF_HOOKPROG_TYPE_INFO(&ng_bpf_hookprogary_type);
static const struct ng_parse_type ng_bpf_hookprog_type = {
	&ng_parse_struct_type,
	&ng_bpf_hookprog_type_info
};

/* Parse type for struct ng_bpf_hookstat */
static const struct ng_parse_struct_info
	ng_bpf_hookstat_type_info = NG_BPF_HOOKSTAT_TYPE_INFO;
static const struct ng_parse_type ng_bpf_hookstat_type = {
	&ng_parse_struct_type,
	&ng_bpf_hookstat_type_info
};

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_bpf_cmdlist[] = {
	{
	  NGM_BPF_COOKIE,
	  NGM_BPF_SET_PROGRAM,
	  "setprogram",
	  &ng_bpf_hookprog_type,
	  NULL
	},
	{
	  NGM_BPF_COOKIE,
	  NGM_BPF_GET_PROGRAM,
	  "getprogram",
	  &ng_parse_hookbuf_type,
	  &ng_bpf_hookprog_type
	},
	{
	  NGM_BPF_COOKIE,
	  NGM_BPF_GET_STATS,
	  "getstats",
	  &ng_parse_hookbuf_type,
	  &ng_bpf_hookstat_type
	},
	{
	  NGM_BPF_COOKIE,
	  NGM_BPF_CLR_STATS,
	  "clrstats",
	  &ng_parse_hookbuf_type,
	  NULL
	},
	{
	  NGM_BPF_COOKIE,
	  NGM_BPF_GETCLR_STATS,
	  "getclrstats",
	  &ng_parse_hookbuf_type,
	  &ng_bpf_hookstat_type
	},
	{ 0 }
};

/* Netgraph type descriptor */
static struct ng_type typestruct = {
	NG_VERSION,
	NG_BPF_NODE_TYPE,
	NULL,
	ng_bpf_constructor,
	ng_bpf_rcvmsg,
	ng_bpf_rmnode,
	ng_bpf_newhook,
	NULL,
	NULL,
	ng_bpf_rcvdata,
	ng_bpf_rcvdata,
	ng_bpf_disconnect,
	ng_bpf_cmdlist
};
NETGRAPH_INIT(bpf, &typestruct);

/* Default BPF program for a hook that matches nothing */
static const struct ng_bpf_hookprog ng_bpf_default_prog = {
	{ '\0' },		/* to be filled in at hook creation time */
	{ '\0' },
	{ '\0' },
	1,
	{ BPF_STMT(BPF_RET+BPF_K, 0) }
};

/*
 * Node constructor
 *
 * We don't keep any per-node private data
 */
static int
ng_bpf_constructor(node_p *nodep)
{
	int error = 0;

	if ((error = ng_make_node_common(&typestruct, nodep)))
		return (error);
	(*nodep)->private = NULL;
	return (0);
}

/*
 * Add a hook
 */
static int
ng_bpf_newhook(node_p node, hook_p hook, const char *name)
{
	hinfo_p hip;
	int error;

	/* Create hook private structure */
	MALLOC(hip, hinfo_p, sizeof(*hip), M_NETGRAPH, M_WAITOK);
	if (hip == NULL)
		return (ENOMEM);
	bzero(hip, sizeof(*hip));
	hip->hook = hook;
	hook->private = hip;
	hip->node = node;

	/* Attach the default BPF program */
	if ((error = ng_bpf_setprog(hook, &ng_bpf_default_prog)) != 0) {
		FREE(hip, M_NETGRAPH);
		hook->private = NULL;
		return (error);
	}

	/* Set hook name */
	strncpy(hip->prog->thisHook, name, sizeof(hip->prog->thisHook) - 1);
	hip->prog->thisHook[sizeof(hip->prog->thisHook) - 1] = '\0';
	return (0);
}

/*
 * Receive a control message
 */
static int
ng_bpf_rcvmsg(node_p node, struct ng_mesg *msg, const char *retaddr,
	   struct ng_mesg **rptr)
{
	struct ng_mesg *resp = NULL;
	int error = 0;

	switch (msg->header.typecookie) {
	case NGM_BPF_COOKIE:
		switch (msg->header.cmd) {
		case NGM_BPF_SET_PROGRAM:
		    {
			struct ng_bpf_hookprog *const
			    hp = (struct ng_bpf_hookprog *)msg->data;
			hook_p hook;

			/* Sanity check */
			if (msg->header.arglen < sizeof(*hp)
			    || msg->header.arglen
			      != NG_BPF_HOOKPROG_SIZE(hp->bpf_prog_len))
				ERROUT(EINVAL);

			/* Find hook */
			if ((hook = ng_findhook(node, hp->thisHook)) == NULL)
				ERROUT(ENOENT);

			/* Set new program */
			if ((error = ng_bpf_setprog(hook, hp)) != 0)
				ERROUT(error);
			break;
		    }

		case NGM_BPF_GET_PROGRAM:
		    {
			struct ng_bpf_hookprog *hp;
			hook_p hook;

			/* Sanity check */
			if (msg->header.arglen == 0)
				ERROUT(EINVAL);
			msg->data[msg->header.arglen - 1] = '\0';

			/* Find hook */
			if ((hook = ng_findhook(node, msg->data)) == NULL)
				ERROUT(ENOENT);

			/* Build response */
			hp = ((hinfo_p)hook->private)->prog;
			NG_MKRESPONSE(resp, msg,
			    NG_BPF_HOOKPROG_SIZE(hp->bpf_prog_len), M_NOWAIT);
			if (resp == NULL)
				ERROUT(ENOMEM);
			bcopy(hp, resp->data,
			   NG_BPF_HOOKPROG_SIZE(hp->bpf_prog_len));
			break;
		    }

		case NGM_BPF_GET_STATS:
		case NGM_BPF_CLR_STATS:
		case NGM_BPF_GETCLR_STATS:
		    {
			struct ng_bpf_hookstat *stats;
			hook_p hook;

			/* Sanity check */
			if (msg->header.arglen == 0)
				ERROUT(EINVAL);
			msg->data[msg->header.arglen - 1] = '\0';

			/* Find hook */
			if ((hook = ng_findhook(node, msg->data)) == NULL)
				ERROUT(ENOENT);
			stats = &((hinfo_p)hook->private)->stats;

			/* Build response (if desired) */
			if (msg->header.cmd != NGM_BPF_CLR_STATS) {
				NG_MKRESPONSE(resp,
				    msg, sizeof(*stats), M_NOWAIT);
				if (resp == NULL)
					ERROUT(ENOMEM);
				bcopy(stats, resp->data, sizeof(*stats));
			}

			/* Clear stats (if desired) */
			if (msg->header.cmd != NGM_BPF_GET_STATS)
				bzero(stats, sizeof(*stats));
			break;
		    }

		default:
			error = EINVAL;
			break;
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	if (rptr)
		*rptr = resp;
	else if (resp)
		FREE(resp, M_NETGRAPH);

done:
	FREE(msg, M_NETGRAPH);
	return (error);
}

/*
 * Receive data on a hook
 *
 * Apply the filter, and then drop or forward packet as appropriate.
 */
static int
ng_bpf_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	const hinfo_p hip = hook->private;
	int totlen = m->m_pkthdr.len;
	int needfree = 0, error = 0;
	u_char *data, buf[256];
	hinfo_p dhip;
	hook_p dest;
	u_int len;

	/* Update stats on incoming hook */
	hip->stats.recvFrames++;
	hip->stats.recvOctets += totlen;

	/* Need to put packet in contiguous memory for bpf */
	if (m->m_next != NULL) {
		if (totlen > sizeof(buf)) {
			MALLOC(data, u_char *, totlen, M_NETGRAPH, M_NOWAIT);
			if (data == NULL) {
				NG_FREE_DATA(m, meta);
				return (ENOMEM);
			}
			needfree = 1;
		} else
			data = buf;
		m_copydata(m, 0, totlen, (caddr_t)data);
	} else
		data = mtod(m, u_char *);

	/* Run packet through filter */
	len = bpf_filter(hip->prog->bpf_prog, data, totlen, totlen);
	if (needfree)
		FREE(data, M_NETGRAPH);

	/* See if we got a match and find destination hook */
	if (len > 0) {

		/* Update stats */
		hip->stats.recvMatchFrames++;
		hip->stats.recvMatchOctets += totlen;

		/* Truncate packet length if required by the filter */
		if (len < totlen) {
			m_adj(m, -(totlen - len));
			totlen -= len;
		}
		dest = ng_findhook(hip->node, hip->prog->ifMatch);
	} else
		dest = ng_findhook(hip->node, hip->prog->ifNotMatch);
	if (dest == NULL) {
		NG_FREE_DATA(m, meta);
		return (0);
	}

	/* Deliver frame out destination hook */
	dhip = (hinfo_p)dest->private;
	dhip->stats.xmitOctets += totlen;
	dhip->stats.xmitFrames++;
	NG_SEND_DATA(error, dest, m, meta);
	return (error);
}

/*
 * Shutdown processing
 */
static int
ng_bpf_rmnode(node_p node)
{
	node->flags |= NG_INVALID;
	ng_cutlinks(node);
	ng_unname(node);
	ng_unref(node);
	return (0);
}

/*
 * Hook disconnection
 */
static int
ng_bpf_disconnect(hook_p hook)
{
	const hinfo_p hip = hook->private;

	KASSERT(hip != NULL, ("%s: null info", __FUNCTION__));
	FREE(hip->prog, M_NETGRAPH);
	bzero(hip, sizeof(*hip));
	FREE(hip, M_NETGRAPH);
	hook->private = NULL;			/* for good measure */
	if (hook->node->numhooks == 0)
		ng_rmnode(hook->node);
	return (0);
}

/************************************************************************
			HELPER STUFF
 ************************************************************************/

/*
 * Set the BPF program associated with a hook
 */
static int
ng_bpf_setprog(hook_p hook, const struct ng_bpf_hookprog *hp0)
{
	const hinfo_p hip = hook->private;
	struct ng_bpf_hookprog *hp;
	int size;

	/* Check program for validity */
	if (!bpf_validate(hp0->bpf_prog, hp0->bpf_prog_len))
		return (EINVAL);

	/* Make a copy of the program */
	size = NG_BPF_HOOKPROG_SIZE(hp0->bpf_prog_len);
	MALLOC(hp, struct ng_bpf_hookprog *, size, M_NETGRAPH, M_WAITOK);
	if (hp == NULL)
		return (ENOMEM);
	bcopy(hp0, hp, size);

	/* Free previous program, if any, and assign new one */
	if (hip->prog != NULL)
		FREE(hip->prog, M_NETGRAPH);
	hip->prog = hp;
	return (0);
}

