
/*
 * ng_ksocket.c
 *
 * Copyright (c) 1996-1999 Whistle Communications, Inc.
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
 * WHISTLE COMMUNICATIONS DOES NOT WARRANT, GUARANTEE, OR MAKE ANY
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
 * $FreeBSD$
 * $Whistle: ng_ksocket.c,v 1.1 1999/11/16 20:04:40 archie Exp $
 */

/*
 * Kernel socket node type.  This node type is basically a kernel-mode
 * version of a socket... kindof like the reverse of the socket node type.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/uio.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_ksocket.h>

#include <netinet/in.h>
#include <netatalk/at.h>

/* Node private data */
struct ng_ksocket_private {
	hook_p		hook;
	struct socket	*so;
};
typedef struct ng_ksocket_private *priv_p;

/* Netgraph node methods */
static ng_constructor_t	ng_ksocket_constructor;
static ng_rcvmsg_t	ng_ksocket_rcvmsg;
static ng_shutdown_t	ng_ksocket_rmnode;
static ng_newhook_t	ng_ksocket_newhook;
static ng_rcvdata_t	ng_ksocket_rcvdata;
static ng_disconnect_t	ng_ksocket_disconnect;

/* Alias structure */
struct ng_ksocket_alias {
	const char	*name;
	const int	value;
	const int	family;
};

/* Helper functions */
static void	ng_ksocket_incoming(struct socket *so, void *arg, int waitflag);
static int	ng_ksocket_parse(const struct ng_ksocket_alias *aliases,
			const char *s, int family);

/* Node type descriptor */
static struct ng_type ng_ksocket_typestruct = {
	NG_VERSION,
	NG_KSOCKET_NODE_TYPE,
	NULL,
	ng_ksocket_constructor,
	ng_ksocket_rcvmsg,
	ng_ksocket_rmnode,
	ng_ksocket_newhook,
	NULL,
	NULL,
	ng_ksocket_rcvdata,
	ng_ksocket_rcvdata,
	ng_ksocket_disconnect
};
NETGRAPH_INIT(ksocket, &ng_ksocket_typestruct);

/* Protocol family aliases */
static const struct ng_ksocket_alias ng_ksocket_families[] = {
	{ "local",	PF_LOCAL	},
	{ "inet",	PF_INET		},
	{ "inet6",	PF_INET6	},
	{ "atalk",	PF_APPLETALK	},
	{ "ipx",	PF_IPX		},
	{ "atm",	PF_ATM		},
	{ NULL,		-1		},
};

/* Socket type aliases */
static const struct ng_ksocket_alias ng_ksocket_types[] = {
	{ "stream",	SOCK_STREAM	},
	{ "dgram",	SOCK_DGRAM	},
	{ "raw",	SOCK_RAW	},
	{ "rdm",	SOCK_RDM	},
	{ "seqpacket",	SOCK_SEQPACKET	},
	{ NULL,		-1		},
};

/* Protocol aliases */
static const struct ng_ksocket_alias ng_ksocket_protos[] = {
	{ "ip",		IPPROTO_IP,		PF_INET		},
	{ "raw",	IPPROTO_IP,		PF_INET		},
	{ "icmp",	IPPROTO_ICMP,		PF_INET		},
	{ "igmp",	IPPROTO_IGMP,		PF_INET		},
	{ "tcp",	IPPROTO_TCP,		PF_INET		},
	{ "udp",	IPPROTO_UDP,		PF_INET		},
	{ "gre",	IPPROTO_GRE,		PF_INET		},
	{ "esp",	IPPROTO_ESP,		PF_INET		},
	{ "ah",		IPPROTO_AH,		PF_INET		},
	{ "swipe",	IPPROTO_SWIPE,		PF_INET		},
	{ "encap",	IPPROTO_ENCAP,		PF_INET		},
	{ "divert",	IPPROTO_DIVERT,		PF_INET		},
	{ "ddp",	ATPROTO_DDP,		PF_APPLETALK	},
	{ "aarp",	ATPROTO_AARP,		PF_APPLETALK	},
	{ NULL,		-1					},
};

#define ERROUT(x)	do { error = (x); goto done; } while (0)

/************************************************************************
			NETGRAPH NODE STUFF
 ************************************************************************/

/*
 * Node type constructor
 */
static int
ng_ksocket_constructor(node_p *nodep)
{
	priv_p priv;
	int error;

	/* Allocate private structure */
	MALLOC(priv, priv_p, sizeof(*priv), M_NETGRAPH, M_WAITOK);
	if (priv == NULL)
		return (ENOMEM);
	bzero(priv, sizeof(*priv));

	/* Call generic node constructor */
	if ((error = ng_make_node_common(&ng_ksocket_typestruct, nodep))) {
		FREE(priv, M_NETGRAPH);
		return (error);
	}
	(*nodep)->private = priv;

	/* Done */
	return (0);
}

/*
 * Give our OK for a hook to be added. The hook name is of the
 * form "<family>:<type>:<proto>" where the three components may
 * be decimal numbers or else aliases from the above lists.
 *
 * Connecting a hook amounts to opening the socket.  Disconnecting
 * the hook closes the socket and destroys the node as well.
 */
static int
ng_ksocket_newhook(node_p node, hook_p hook, const char *name0)
{
	const priv_p priv = node->private;
	char *s1, *s2, name[NG_HOOKLEN+1];
	int family, type, protocol, error;
	struct proc *p = &proc0;		/* XXX help what to do here */

	/* Check if we're already connected */
	if (priv->hook != NULL)
		return (EISCONN);

	/* Extract family, type, and protocol from hook name */
	snprintf(name, sizeof(name), "%s", name0);
	s1 = name;
	if ((s2 = index(s1, '/')) == NULL)
		return (EINVAL);
	*s2++ = '\0';
	if ((family = ng_ksocket_parse(ng_ksocket_families, s1, 0)) == -1)
		return (EINVAL);
	s1 = s2;
	if ((s2 = index(s1, '/')) == NULL)
		return (EINVAL);
	*s2++ = '\0';
	if ((type = ng_ksocket_parse(ng_ksocket_types, s1, 0)) == -1)
		return (EINVAL);
	s1 = s2;
	if ((protocol = ng_ksocket_parse(ng_ksocket_protos, s1, family)) == -1)
		return (EINVAL);

	/* Create the socket */
	if ((error = socreate(family, &priv->so, type, protocol, p)) != 0)
		return (error);

	/* XXX call soreserve() ? */

	/* Add our hook for incoming data */
	priv->so->so_upcallarg = (caddr_t)node;
	priv->so->so_upcall = ng_ksocket_incoming;
	priv->so->so_rcv.sb_flags |= SB_UPCALL;

	/* OK */
	priv->hook = hook;
	return (0);
}

/*
 * Receive a control message
 */
static int
ng_ksocket_rcvmsg(node_p node, struct ng_mesg *msg,
	      const char *raddr, struct ng_mesg **rptr)
{
	const priv_p priv = node->private;
	struct ng_mesg *resp = NULL;
	struct proc *p = &proc0;
	int error = 0;

	switch (msg->header.typecookie) {
	case NGM_KSOCKET_COOKIE:
		switch (msg->header.cmd) {
		case NGM_KSOCKET_BIND:
		    {
			struct sockaddr *sa = (struct sockaddr *)msg->data;
			struct socket *const so = priv->so;

			/* Must have a connected hook first */
			if (priv->hook == NULL)
				ERROUT(ENETDOWN);

			/* Set and sanity check sockaddr length */
			if (msg->header.arglen > SOCK_MAXADDRLEN)
				ERROUT(ENAMETOOLONG);
			sa->sa_len = msg->header.arglen;
			error = sobind(so, sa, p);
			break;
		    }
		case NGM_KSOCKET_LISTEN:
		    {
			struct socket *const so = priv->so;
			int backlog;

			/* Must have a connected hook first */
			if (priv->hook == NULL)
				ERROUT(ENETDOWN);

			/* Get backlog argument */
			if (msg->header.arglen != sizeof(int))
				ERROUT(EINVAL);
			backlog = *((int *)msg->data);

			/* Do listen */
			if ((error = solisten(so, backlog, p)) != 0)
				break;

			/* Notify sender when we get a connection attempt */
				/* XXX implement me */
			break;
		    }

		case NGM_KSOCKET_ACCEPT:
		    {
			ERROUT(ENODEV);		/* XXX implement me */
			break;
		    }

		case NGM_KSOCKET_CONNECT:
		    {
			struct socket *const so = priv->so;
			struct sockaddr *sa = (struct sockaddr *)msg->data;

			/* Must have a connected hook first */
			if (priv->hook == NULL)
				ERROUT(ENETDOWN);

			/* Set and sanity check sockaddr length */
			if (msg->header.arglen > SOCK_MAXADDRLEN)
				ERROUT(ENAMETOOLONG);
			sa->sa_len = msg->header.arglen;

			/* Do connect */
			if ((so->so_state & SS_ISCONNECTING) != 0)
				ERROUT(EALREADY);
			if ((error = soconnect(so, sa, p)) != 0) {
				so->so_state &= ~SS_ISCONNECTING;
				ERROUT(error);
			}
			if ((so->so_state & SS_ISCONNECTING) != 0)
				/* Notify sender when we connect */
				/* XXX implement me */
				ERROUT(EINPROGRESS);
			break;
		    }

		case NGM_KSOCKET_GETNAME:
		    {
			ERROUT(ENODEV);		/* XXX implement me */
			break;
		    }

		case NGM_KSOCKET_GETPEERNAME:
		    {
			ERROUT(ENODEV);		/* XXX implement me */
			break;
		    }

		case NGM_KSOCKET_GETOPT:
		    {
			ERROUT(ENODEV);		/* XXX implement me */
			break;
		    }

		case NGM_KSOCKET_SETOPT:
		    {
			ERROUT(ENODEV);		/* XXX implement me */
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
 * Receive incoming data on our hook.  Send it out the socket.
 */
static int
ng_ksocket_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	const node_p node = hook->node;
	const priv_p priv = node->private;
	struct socket *const so = priv->so;
	struct proc *p = &proc0;
	int error;

	NG_FREE_META(meta);
	error = (*so->so_proto->pr_usrreqs->pru_sosend)(so, 0, 0, m, 0, 0, p);
	return (error);
}

/*
 * Destroy node
 */
static int
ng_ksocket_rmnode(node_p node)
{
	const priv_p priv = node->private;

	/* Close our socket (if any) */
	if (priv->so != NULL) {
		soclose(priv->so);
		priv->so = NULL;
	}

	/* Take down netgraph node */
	node->flags |= NG_INVALID;
	ng_cutlinks(node);
	ng_unname(node);
	bzero(priv, sizeof(*priv));
	FREE(priv, M_NETGRAPH);
	node->private = NULL;
	ng_unref(node);		/* let the node escape */
	return (0);
}

/*
 * Hook disconnection
 */
static int
ng_ksocket_disconnect(hook_p hook)
{
	KASSERT(hook->node->numhooks == 0,
	    ("%s: numhooks=%d?", __FUNCTION__, hook->node->numhooks));
	ng_rmnode(hook->node);
	return (0);
}

/************************************************************************
			HELPER STUFF
 ************************************************************************/

/*
 * When incoming data is appended to the socket, we get notified here.
 */
static void
ng_ksocket_incoming(struct socket *so, void *arg, int waitflag)
{
	const node_p node = arg;
	const priv_p priv = node->private;
	meta_p meta = NULL;
	struct sockaddr *nam;
	struct mbuf *m;
	struct uio auio;
	int s, flags, error;

	s = splnet();

	/* Sanity check */
	if ((node->flags & NG_INVALID) != 0) {
		splx(s);
		return;
	}
	KASSERT(so == priv->so, ("%s: wrong socket", __FUNCTION__));
	KASSERT(priv->hook != NULL, ("%s: no hook", __FUNCTION__));

	/* Read and forward available mbuf's */
	auio.uio_procp = NULL;
	auio.uio_resid = 1000000000;
	flags = MSG_DONTWAIT;
	do {
		if ((error = (*so->so_proto->pr_usrreqs->pru_soreceive)
		    (so, &nam, &auio, &m, (struct mbuf **)0, &flags)) == 0)
			NG_SEND_DATA(error, priv->hook, m, meta);
	} while (error == 0 && m != NULL);
	splx(s);
}

/*
 * Parse out either an integer value or an alias.
 */
static int
ng_ksocket_parse(const struct ng_ksocket_alias *aliases,
	const char *s, int family)
{
	int k, val;
	const char *eptr;

	/* Try aliases */
	for (k = 0; aliases[k].name != NULL; k++) {
		if (strcmp(s, aliases[k].name) == 0
		    && aliases[k].family == family)
			return aliases[k].value;
	}

	/* Try parsing as a number */
	val = (int)strtoul(s, &eptr, 10);
	if (val <= 0 || *eptr != '\0')
		return (-1);
	return (val);
}

