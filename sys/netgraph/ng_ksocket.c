
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
 * Author: Archie Cobbs <archie@freebsd.org>
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
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/ctype.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/uio.h>
#include <sys/un.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/ng_ksocket.h>

#include <netinet/in.h>
#include <netatalk/at.h>

#define OFFSETOF(s, e) ((char *)&((s *)0)->e - (char *)((s *)0))
#define SADATA_OFFSET	(OFFSETOF(struct sockaddr, sa_data))

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

/* Helper functions */
static void	ng_ksocket_incoming(struct socket *so, void *arg, int waitflag);
static int	ng_ksocket_parse(const struct ng_ksocket_alias *aliases,
			const char *s, int family);

/************************************************************************
			STRUCT SOCKADDR PARSE TYPE
 ************************************************************************/

/* Get the length of the data portion of a generic struct sockaddr */
static int
ng_parse_generic_sockdata_getLength(const struct ng_parse_type *type,
	const u_char *start, const u_char *buf)
{
	const struct sockaddr *sa;

	sa = (const struct sockaddr *)(buf - SADATA_OFFSET);
	return (sa->sa_len < SADATA_OFFSET) ? 0 : sa->sa_len - SADATA_OFFSET;
}

/* Type for the variable length data portion of a generic struct sockaddr */
static const struct ng_parse_type ng_ksocket_generic_sockdata_type = {
	&ng_parse_bytearray_type,
	&ng_parse_generic_sockdata_getLength
};

/* Type for a generic struct sockaddr */
static const struct ng_parse_struct_info ng_parse_generic_sockaddr_type_info = {
	{
	  { "len",	&ng_parse_uint8_type			},
	  { "family",	&ng_parse_uint8_type			},
	  { "data",	&ng_ksocket_generic_sockdata_type	},
	  { NULL }
	}
};
static const struct ng_parse_type ng_ksocket_generic_sockaddr_type = {
	&ng_parse_struct_type,
	&ng_parse_generic_sockaddr_type_info
};

/* Convert a struct sockaddr from ASCII to binary.  If its a protocol
   family that we specially handle, do that, otherwise defer to the
   generic parse type ng_ksocket_generic_sockaddr_type. */
static int
ng_ksocket_sockaddr_parse(const struct ng_parse_type *type,
	const char *s, int *off, const u_char *const start,
	u_char *const buf, int *buflen)
{
	struct sockaddr *const sa = (struct sockaddr *)buf;
	enum ng_parse_token tok;
	char fambuf[32];
	int family, len;
	char *t;

	/* If next token is a left curly brace, use generic parse type */
	if ((tok = ng_parse_get_token(s, off, &len)) == T_LBRACE) {
		return (*ng_ksocket_generic_sockaddr_type.supertype->parse)
		    (&ng_ksocket_generic_sockaddr_type,
		    s, off, start, buf, buflen);
	}

	/* Get socket address family followed by a slash */
	while (isspace(s[*off]))
		(*off)++;
	if ((t = index(s + *off, '/')) == NULL)
		return (EINVAL);
	if ((len = t - (s + *off)) > sizeof(fambuf) - 1)
		return (EINVAL);
	strncpy(fambuf, s + *off, len);
	fambuf[len] = '\0';
	*off += len + 1;
	if ((family = ng_ksocket_parse(ng_ksocket_families, fambuf, 0)) == -1)
		return (EINVAL);

	/* Set family */
	if (*buflen < SADATA_OFFSET)
		return (ERANGE);
	sa->sa_family = family;

	/* Set family-specific data and length */
	switch (sa->sa_family) {
	case PF_LOCAL:		/* Get pathname */
	    {
		const int pathoff = OFFSETOF(struct sockaddr_un, sun_path);
		struct sockaddr_un *const sun = (struct sockaddr_un *)sa;
		int toklen, pathlen;
		char *path;

		if ((path = ng_get_string_token(s, off, &toklen)) == NULL)
			return (EINVAL);
		pathlen = strlen(path);
		if (pathlen > SOCK_MAXADDRLEN) {
			FREE(path, M_NETGRAPH);
			return (E2BIG);
		}
		if (*buflen < pathoff + pathlen) {
			FREE(path, M_NETGRAPH);
			return (ERANGE);
		}
		*off += toklen;
		bcopy(path, sun->sun_path, pathlen);
		sun->sun_len = pathoff + pathlen;
		FREE(path, M_NETGRAPH);
		break;
	    }

	case PF_INET:		/* Get an IP address with optional port */
	    {
		struct sockaddr_in *const sin = (struct sockaddr_in *)sa;
		int i;

		/* Parse this: <ipaddress>[:port] */
		for (i = 0; i < 4; i++) {
			u_long val;
			char *eptr;

			val = strtoul(s + *off, &eptr, 10);
			if (val > 0xff || eptr == s + *off)
				return (EINVAL);
			*off += (eptr - (s + *off));
			((u_char *)&sin->sin_addr)[i] = (u_char)val;
			if (i < 3) {
				if (s[*off] != '.')
					return (EINVAL);
				(*off)++;
			} else if (s[*off] == ':') {
				(*off)++;
				val = strtoul(s + *off, &eptr, 10);
				if (val > 0xffff || eptr == s + *off)
					return (EINVAL);
				*off += (eptr - (s + *off));
				sin->sin_port = htons(val);
			} else
				sin->sin_port = 0;
		}
		bzero(&sin->sin_zero, sizeof(sin->sin_zero));
		sin->sin_len = sizeof(*sin);
		break;
	    }

#if 0
	case PF_APPLETALK:	/* XXX implement these someday */
	case PF_INET6:
	case PF_IPX:
#endif

	default:
		return (EINVAL);
	}

	/* Done */
	*buflen = sa->sa_len;
	return (0);
}

/* Convert a struct sockaddr from binary to ASCII */
static int
ng_ksocket_sockaddr_unparse(const struct ng_parse_type *type,
	const u_char *data, int *off, char *cbuf, int cbuflen)
{
	const struct sockaddr *sa = (const struct sockaddr *)(data + *off);
	int slen = 0;

	/* Output socket address, either in special or generic format */
	switch (sa->sa_family) {
	case PF_LOCAL:
	    {
		const int pathoff = OFFSETOF(struct sockaddr_un, sun_path);
		const struct sockaddr_un *sun = (const struct sockaddr_un *)sa;
		const int pathlen = sun->sun_len - pathoff;
		char pathbuf[SOCK_MAXADDRLEN + 1];
		char *pathtoken;

		bcopy(sun->sun_path, pathbuf, pathlen);
		pathbuf[pathlen] = '\0';
		if ((pathtoken = ng_encode_string(pathbuf)) == NULL)
			return (ENOMEM);
		slen += snprintf(cbuf, cbuflen, "local/%s", pathtoken);
		FREE(pathtoken, M_NETGRAPH);
		if (slen >= cbuflen)
			return (ERANGE);
		*off += sun->sun_len;
		return (0);
	    }

	case PF_INET:
	    {
		const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;

		slen += snprintf(cbuf, cbuflen, "inet/%d.%d.%d.%d",
		  ((const u_char *)&sin->sin_addr)[0],
		  ((const u_char *)&sin->sin_addr)[1],
		  ((const u_char *)&sin->sin_addr)[2],
		  ((const u_char *)&sin->sin_addr)[3]);
		if (sin->sin_port != 0) {
			slen += snprintf(cbuf + strlen(cbuf),
			    cbuflen - strlen(cbuf), ":%d",
			    (u_int)ntohs(sin->sin_port));
		}
		if (slen >= cbuflen)
			return (ERANGE);
		*off += sizeof(*sin);
		return(0);
	    }

#if 0
	case PF_APPLETALK:	/* XXX implement these someday */
	case PF_INET6:
	case PF_IPX:
#endif

	default:
		return (*ng_ksocket_generic_sockaddr_type.supertype->unparse)
		    (&ng_ksocket_generic_sockaddr_type,
		    data, off, cbuf, cbuflen);
	}
}

/* Parse type for struct sockaddr */
static const struct ng_parse_type ng_ksocket_sockaddr_type = {
	NULL,
	NULL,
	NULL,
	&ng_ksocket_sockaddr_parse,
	&ng_ksocket_sockaddr_unparse,
	NULL		/* no such thing as a default struct sockaddr */
};

/************************************************************************
		STRUCT NG_KSOCKET_SOCKOPT PARSE TYPE
 ************************************************************************/

/* Get length of the struct ng_ksocket_sockopt value field, which is the
   just the excess of the message argument portion over the length of
   the struct ng_ksocket_sockopt. */
static int
ng_parse_sockoptval_getLength(const struct ng_parse_type *type,
	const u_char *start, const u_char *buf)
{
	static const int offset = OFFSETOF(struct ng_ksocket_sockopt, value);
	const struct ng_ksocket_sockopt *sopt;
	const struct ng_mesg *msg;

	sopt = (const struct ng_ksocket_sockopt *)(buf - offset);
	msg = (const struct ng_mesg *)((const u_char *)sopt - sizeof(*msg));
	return msg->header.arglen - sizeof(*sopt);
}

/* Parse type for the option value part of a struct ng_ksocket_sockopt
   XXX Eventually, we should handle the different socket options specially.
   XXX This would avoid byte order problems, eg an integer value of 1 is
   XXX going to be "[1]" for little endian or "[3=1]" for big endian. */
static const struct ng_parse_type ng_ksocket_sockoptval_type = {
	&ng_parse_bytearray_type,
	&ng_parse_sockoptval_getLength
};

/* Parse type for struct ng_ksocket_sockopt */
static const struct ng_parse_struct_info ng_ksocket_sockopt_type_info
	= NG_KSOCKET_SOCKOPT_INFO(&ng_ksocket_sockoptval_type);
static const struct ng_parse_type ng_ksocket_sockopt_type = {
	&ng_parse_struct_type,
	&ng_ksocket_sockopt_type_info,
};

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_ksocket_cmds[] = {
	{
	  NGM_KSOCKET_COOKIE,
	  NGM_KSOCKET_BIND,
	  "bind",
	  &ng_ksocket_sockaddr_type,
	  NULL
	},
	{
	  NGM_KSOCKET_COOKIE,
	  NGM_KSOCKET_LISTEN,
	  "listen",
	  &ng_parse_int32_type,
	  NULL
	},
	{
	  NGM_KSOCKET_COOKIE,
	  NGM_KSOCKET_ACCEPT,
	  "accept",
	  NULL,
	  &ng_ksocket_sockaddr_type
	},
	{
	  NGM_KSOCKET_COOKIE,
	  NGM_KSOCKET_CONNECT,
	  "connect",
	  &ng_ksocket_sockaddr_type,
	  NULL
	},
	{
	  NGM_KSOCKET_COOKIE,
	  NGM_KSOCKET_GETNAME,
	  "getname",
	  NULL,
	  &ng_ksocket_sockaddr_type
	},
	{
	  NGM_KSOCKET_COOKIE,
	  NGM_KSOCKET_GETPEERNAME,
	  "getpeername",
	  NULL,
	  &ng_ksocket_sockaddr_type
	},
	{
	  NGM_KSOCKET_COOKIE,
	  NGM_KSOCKET_SETOPT,
	  "setopt",
	  &ng_ksocket_sockopt_type,
	  NULL
	},
	{
	  NGM_KSOCKET_COOKIE,
	  NGM_KSOCKET_GETOPT,
	  "getopt",
	  &ng_ksocket_sockopt_type,
	  &ng_ksocket_sockopt_type
	},
	{ 0 }
};

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
	ng_ksocket_disconnect,
	ng_ksocket_cmds
};
NETGRAPH_INIT(ksocket, &ng_ksocket_typestruct);

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
	MALLOC(priv, priv_p, sizeof(*priv), M_NETGRAPH, M_NOWAIT);
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
	struct proc *p = curproc ? curproc : &proc0;	/* XXX broken */
	const priv_p priv = node->private;
	char *s1, *s2, name[NG_HOOKLEN+1];
	int family, type, protocol, error;

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
	struct proc *p = curproc ? curproc : &proc0;	/* XXX broken */
	const priv_p priv = node->private;
	struct socket *const so = priv->so;
	struct ng_mesg *resp = NULL;
	int error = 0;

	switch (msg->header.typecookie) {
	case NGM_KSOCKET_COOKIE:
		switch (msg->header.cmd) {
		case NGM_KSOCKET_BIND:
		    {
			struct sockaddr *const sa
			    = (struct sockaddr *)msg->data;

			/* Sanity check */
			if (msg->header.arglen < SADATA_OFFSET
			    || msg->header.arglen < sa->sa_len)
				ERROUT(EINVAL);
			if (so == NULL)
				ERROUT(ENXIO);

			/* Bind */
			error = sobind(so, sa, p);
			break;
		    }
		case NGM_KSOCKET_LISTEN:
		    {
			/* Sanity check */
			if (msg->header.arglen != sizeof(int))
				ERROUT(EINVAL);
			if (so == NULL)
				ERROUT(ENXIO);

			/* Listen */
			if ((error = solisten(so, *((int *)msg->data), p)) != 0)
				break;

			/* Notify sender when we get a connection attempt */
				/* XXX implement me */
			error = ENODEV;
			break;
		    }

		case NGM_KSOCKET_ACCEPT:
		    {
			/* Sanity check */
			if (msg->header.arglen != 0)
				ERROUT(EINVAL);
			if (so == NULL)
				ERROUT(ENXIO);

			/* Accept on the socket in a non-blocking way */
			/* Create a new ksocket node for the new connection */
			/* Return a response with the peer's sockaddr and
			   the absolute name of the newly created node */
			   
				/* XXX implement me */

			error = ENODEV;
			break;
		    }

		case NGM_KSOCKET_CONNECT:
		    {
			struct sockaddr *const sa
			    = (struct sockaddr *)msg->data;

			/* Sanity check */
			if (msg->header.arglen < SADATA_OFFSET
			    || msg->header.arglen < sa->sa_len)
				ERROUT(EINVAL);
			if (so == NULL)
				ERROUT(ENXIO);

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
		case NGM_KSOCKET_GETPEERNAME:
		    {
			int (*func)(struct socket *so, struct sockaddr **nam);
			struct sockaddr *sa = NULL;
			int len;

			/* Sanity check */
			if (msg->header.arglen != 0)
				ERROUT(EINVAL);
			if (so == NULL)
				ERROUT(ENXIO);

			/* Get function */
			if (msg->header.cmd == NGM_KSOCKET_GETPEERNAME) {
				if ((so->so_state
				    & (SS_ISCONNECTED|SS_ISCONFIRMING)) == 0) 
					ERROUT(ENOTCONN);
				func = so->so_proto->pr_usrreqs->pru_peeraddr;
			} else
				func = so->so_proto->pr_usrreqs->pru_sockaddr;

			/* Get local or peer address */
			if ((error = (*func)(so, &sa)) != 0)
				goto bail;
			len = (sa == NULL) ? 0 : sa->sa_len;

			/* Send it back in a response */
			NG_MKRESPONSE(resp, msg, len, M_NOWAIT);
			if (resp == NULL) {
				error = ENOMEM;
				goto bail;
			}
			bcopy(sa, resp->data, len);

		bail:
			/* Cleanup */
			if (sa != NULL)
				FREE(sa, M_SONAME);
			break;
		    }

		case NGM_KSOCKET_GETOPT:
		    {
			struct ng_ksocket_sockopt *ksopt = 
			    (struct ng_ksocket_sockopt *)msg->data;
			struct sockopt sopt;

			/* Sanity check */
			if (msg->header.arglen != sizeof(*ksopt))
				ERROUT(EINVAL);
			if (so == NULL)
				ERROUT(ENXIO);

			/* Get response with room for option value */
			NG_MKRESPONSE(resp, msg, sizeof(*ksopt)
			    + NG_KSOCKET_MAX_OPTLEN, M_NOWAIT);
			if (resp == NULL)
				ERROUT(ENOMEM);

			/* Get socket option, and put value in the response */
			sopt.sopt_dir = SOPT_GET;
			sopt.sopt_level = ksopt->level;
			sopt.sopt_name = ksopt->name;
			sopt.sopt_p = p;
			sopt.sopt_valsize = NG_KSOCKET_MAX_OPTLEN;
			ksopt = (struct ng_ksocket_sockopt *)resp->data;
			sopt.sopt_val = ksopt->value;
			if ((error = sogetopt(so, &sopt)) != 0) {
				FREE(resp, M_NETGRAPH);
				break;
			}

			/* Set actual value length */
			resp->header.arglen = sizeof(*ksopt)
			    + sopt.sopt_valsize;
			break;
		    }

		case NGM_KSOCKET_SETOPT:
		    {
			struct ng_ksocket_sockopt *const ksopt = 
			    (struct ng_ksocket_sockopt *)msg->data;
			const int valsize = msg->header.arglen - sizeof(*ksopt);
			struct sockopt sopt;

			/* Sanity check */
			if (valsize < 0)
				ERROUT(EINVAL);
			if (so == NULL)
				ERROUT(ENXIO);

			/* Set socket option */
			sopt.sopt_dir = SOPT_SET;
			sopt.sopt_level = ksopt->level;
			sopt.sopt_name = ksopt->name;
			sopt.sopt_val = ksopt->value;
			sopt.sopt_valsize = valsize;
			sopt.sopt_p = p;
			error = sosetopt(so, &sopt);
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
	struct proc *p = curproc ? curproc : &proc0;	/* XXX broken */
	const node_p node = hook->node;
	const priv_p priv = node->private;
	struct socket *const so = priv->so;
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

		priv->so->so_upcall = NULL;
		priv->so->so_rcv.sb_flags &= ~SB_UPCALL;
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
		      (so, (struct sockaddr **)0, &auio, &m,
		      (struct mbuf **)0, &flags)) == 0
		    && m != NULL) {
			struct mbuf *n;

			/* Don't trust the various socket layers to get the
			   packet header and length correct (eg. kern/15175) */
			for (n = m, m->m_pkthdr.len = 0; n; n = n->m_next)
				m->m_pkthdr.len += n->m_len;
			NG_SEND_DATA(error, priv->hook, m, meta);
		}
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
	char *eptr;

	/* Try aliases */
	for (k = 0; aliases[k].name != NULL; k++) {
		if (strcmp(s, aliases[k].name) == 0
		    && aliases[k].family == family)
			return aliases[k].value;
	}

	/* Try parsing as a number */
	val = (int)strtoul(s, &eptr, 10);
	if (val < 0 || *eptr != '\0')
		return (-1);
	return (val);
}

