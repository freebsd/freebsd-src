
/*
 * ng_socket.c
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
 * Author: Julian Elischer <julian@freebsd.org>
 *
 * $FreeBSD$
 * $Whistle: ng_socket.c,v 1.28 1999/11/01 09:24:52 julian Exp $
 */

/*
 * Netgraph socket nodes
 *
 * There are two types of netgraph sockets, control and data.
 * Control sockets have a netgraph node, but data sockets are
 * parasitic on control sockets, and have no node of their own.
 */

#include <sys/param.h>
#include <sys/domain.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#ifdef NOTYET
#include <sys/vnode.h>
#endif
#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_socketvar.h>
#include <netgraph/ng_socket.h>

#ifdef NG_SEPARATE_MALLOC
MALLOC_DEFINE(M_NETGRAPH_PATH, "netgraph_path", "netgraph path info ");
MALLOC_DEFINE(M_NETGRAPH_SOCK, "netgraph_sock", "netgraph socket info ");
#else
#define M_NETGRAPH_PATH M_NETGRAPH
#define M_NETGRAPH_SOCK M_NETGRAPH
#endif

/*
 * It's Ascii-art time!
 *   +-------------+   +-------------+
 *   |socket  (ctl)|   |socket (data)|
 *   +-------------+   +-------------+
 *          ^                 ^
 *          |                 |
 *          v                 v
 *    +-----------+     +-----------+
 *    |pcb   (ctl)|     |pcb  (data)|
 *    +-----------+     +-----------+
 *          ^                 ^
 *          |                 |
 *          v                 v
 *      +--------------------------+
 *      |   Socket type private    |
 *      |       data               |
 *      +--------------------------+
 *                   ^
 *                   |
 *                   v
 *           +----------------+
 *           | struct ng_node |
 *           +----------------+
 */

/* Netgraph node methods */
static ng_constructor_t	ngs_constructor;
static ng_rcvmsg_t	ngs_rcvmsg;
static ng_shutdown_t	ngs_shutdown;
static ng_newhook_t	ngs_newhook;
static ng_connect_t	ngs_connect;
static ng_rcvdata_t	ngs_rcvdata;
static ng_disconnect_t	ngs_disconnect;

/* Internal methods */
static int	ng_attach_data(struct socket *so);
static int	ng_attach_cntl(struct socket *so);
static int	ng_attach_common(struct socket *so, int type);
static void	ng_detach_common(struct ngpcb *pcbp, int type);
/*static int	ng_internalize(struct mbuf *m, struct thread *p); */

static int	ng_connect_data(struct sockaddr *nam, struct ngpcb *pcbp);
static int	ng_bind(struct sockaddr *nam, struct ngpcb *pcbp);

static int	ngs_mod_event(module_t mod, int event, void *data);
static int	ship_msg(struct ngpcb *pcbp, struct ng_mesg *msg,
			struct sockaddr_ng *addr);

/* Netgraph type descriptor */
static struct ng_type typestruct = {
	NG_ABI_VERSION,
	NG_SOCKET_NODE_TYPE,
	ngs_mod_event,
	ngs_constructor,
	ngs_rcvmsg,
	ngs_shutdown,
	ngs_newhook,
	NULL,
	ngs_connect,
	ngs_rcvdata,
	ngs_disconnect,
	NULL
};
NETGRAPH_INIT(socket, &typestruct);

/* Buffer space */
static u_long ngpdg_sendspace = 20 * 1024;	/* really max datagram size */
static u_long ngpdg_recvspace = 20 * 1024;

/* List of all sockets */
static LIST_HEAD(, ngpcb) ngsocklist;

#define sotongpcb(so) ((struct ngpcb *)(so)->so_pcb)

/* If getting unexplained errors returned, set this to "Debugger("X"); */
#ifndef TRAP_ERROR
#define TRAP_ERROR
#endif

/***************************************************************
	Control sockets
***************************************************************/

static int
ngc_attach(struct socket *so, int proto, struct thread *td)
{
	struct ngpcb *const pcbp = sotongpcb(so);

	if (suser(td))
		return (EPERM);
	if (pcbp != NULL)
		return (EISCONN);
	return (ng_attach_cntl(so));
}

static int
ngc_detach(struct socket *so)
{
	struct ngpcb *const pcbp = sotongpcb(so);

	if (pcbp == NULL)
		return (EINVAL);
	ng_detach_common(pcbp, NG_CONTROL);
	return (0);
}

static int
ngc_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
	 struct mbuf *control, struct thread *td)
{
	struct ngpcb *const pcbp = sotongpcb(so);
	struct sockaddr_ng *const sap = (struct sockaddr_ng *) addr;
	struct ng_mesg *msg;
	struct mbuf *m0;
	char *path = NULL;
	int len, error = 0;

	if (pcbp == NULL) {
		error = EINVAL;
		goto release;
	}
#ifdef	NOTYET
	if (control && (error = ng_internalize(control, td))) {
		if (pcbp->sockdata == NULL) {
			error = ENOTCONN;
			goto release;
		}
	}
#else	/* NOTYET */
	if (control) {
		error = EINVAL;
		goto release;
	}
#endif	/* NOTYET */

	/* Require destination as there may be >= 1 hooks on this node */
	if (addr == NULL) {
		error = EDESTADDRREQ;
		goto release;
	}

	/* Allocate an expendable buffer for the path, chop off
	 * the sockaddr header, and make sure it's NUL terminated */
	len = sap->sg_len - 2;
	MALLOC(path, char *, len + 1, M_NETGRAPH_PATH, M_WAITOK);
	if (path == NULL) {
		error = ENOMEM;
		goto release;
	}
	bcopy(sap->sg_data, path, len);
	path[len] = '\0';

	/* Move the actual message out of mbufs into a linear buffer.
	 * Start by adding up the size of the data. (could use mh_len?) */
	for (len = 0, m0 = m; m0 != NULL; m0 = m0->m_next)
		len += m0->m_len;

	/* Move the data into a linear buffer as well. Messages are not
	 * delivered in mbufs. */
	MALLOC(msg, struct ng_mesg *, len + 1, M_NETGRAPH_MSG, M_WAITOK);
	if (msg == NULL) {
		error = ENOMEM;
		goto release;
	}
	m_copydata(m, 0, len, (char *)msg);

#ifdef TRACE_MESSAGES
	do {								
		item_p item;						
		if ((item = ng_package_msg(msg)) == NULL) {		
			(msg) = NULL;				
			(error) = ENOMEM;				
printf("err=%d\n",error);
			break;						
		}							
		if (((error) = ng_address_path((pcbp->sockdata->node), (item),		
					(path), (NULL))) == 0) {	
printf("[%x]:<---------[socket]: c=<%d>cmd=%x(%s) f=%x #%d (%s)\n",
item->el_dest->nd_ID,
msg->header.typecookie,
msg->header.cmd,
msg->header.cmdstr,
msg->header.flags,
msg->header.token,
item->el_dest->nd_type->name); 
			SAVE_LINE(item);			
			(error) = ng_snd_item((item), 0);		
		}							
else {
printf("errx=%d\n",error);
}
		(msg) = NULL;						
	} while (0);

#else
	/* The callee will free the msg when done. The path is our business. */
	NG_SEND_MSG_PATH(error, pcbp->sockdata->node, msg, path, 0);
#endif
release:
	if (path != NULL)
		FREE(path, M_NETGRAPH_PATH);
	if (control != NULL)
		m_freem(control);
	if (m != NULL)
		m_freem(m);
	return (error);
}

static int
ngc_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct ngpcb *const pcbp = sotongpcb(so);

	if (pcbp == 0)
		return (EINVAL);
	return (ng_bind(nam, pcbp));
}

static int
ngc_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
printf(" program tried to connect control socket to remote node\n ");
	/*
	 * At this time refuse to do this.. it used to
	 * do something but it was undocumented and not used.
	 */
	return (EINVAL);
}

/***************************************************************
	Data sockets
***************************************************************/

static int
ngd_attach(struct socket *so, int proto, struct thread *td)
{
	struct ngpcb *const pcbp = sotongpcb(so);

	if (pcbp != NULL)
		return (EISCONN);
	return (ng_attach_data(so));
}

static int
ngd_detach(struct socket *so)
{
	struct ngpcb *const pcbp = sotongpcb(so);

	if (pcbp == NULL)
		return (EINVAL);
	ng_detach_common(pcbp, NG_DATA);
	return (0);
}

static int
ngd_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
	 struct mbuf *control, struct thread *td)
{
	struct ngpcb *const pcbp = sotongpcb(so);
	struct sockaddr_ng *const sap = (struct sockaddr_ng *) addr;
	int     len, error;
	hook_p  hook = NULL;
	char	hookname[NG_HOOKLEN + 1];

	if ((pcbp == NULL) || (control != NULL)) {
		error = EINVAL;
		goto release;
	}
	if (pcbp->sockdata == NULL) {
		error = ENOTCONN;
		goto release;
	}
	/*
	 * If the user used any of these ways to not specify an address
	 * then handle specially.
	 */
	if ((sap == NULL)
	    || ((len = sap->sg_len - 2) <= 0)
	    || (*sap->sg_data == '\0')) {
		if (NG_NODE_NUMHOOKS(pcbp->sockdata->node) != 1) {
			error = EDESTADDRREQ;
			goto release;
		}
		/*
		 * if exactly one hook exists, just use it.
		 * Special case to allow write(2) to work on an ng_socket.
		 */
		hook = LIST_FIRST(&pcbp->sockdata->node->nd_hooks);
	} else {
		if (len > NG_HOOKLEN) {
			error = EINVAL;
			goto release;
		}

		/*
		 * chop off the sockaddr header, and make sure it's NUL
		 * terminated
		 */
		bcopy(sap->sg_data, hookname, len);
		hookname[len] = '\0';

		/* Find the correct hook from 'hookname' */
		LIST_FOREACH(hook, &pcbp->sockdata->node->nd_hooks, hk_hooks) {
			if (strcmp(hookname, NG_HOOK_NAME(hook)) == 0) {
				break;
			}
		}
		if (hook == NULL) {
			error = EHOSTUNREACH;
		}
	}

	/* Send data (OK if hook is NULL) */
	NG_SEND_DATA_ONLY(error, hook, m);	/* makes m NULL */

release:
	if (control != NULL)
		m_freem(control);
	if (m != NULL)
		m_freem(m);
	return (error);
}

static int
ngd_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct ngpcb *const pcbp = sotongpcb(so);

	if (pcbp == 0)
		return (EINVAL);
	return (ng_connect_data(nam, pcbp));
}

/*
 * Used for both data and control sockets
 */
static int
ng_setsockaddr(struct socket *so, struct sockaddr **addr)
{
	struct ngpcb *pcbp;
	struct sockaddr_ng *sg;
	int sg_len, namelen, s;

	/* Why isn't sg_data a `char[1]' ? :-( */
	sg_len = sizeof(struct sockaddr_ng) - sizeof(sg->sg_data) + 1;

	s = splnet();
	pcbp = sotongpcb(so);
	if ((pcbp == NULL) || (pcbp->sockdata == NULL)) {
		splx(s);
		return (EINVAL);
	}

	namelen = 0;		/* silence compiler ! */
	if ( NG_NODE_HAS_NAME(pcbp->sockdata->node))
		sg_len += namelen = strlen(NG_NODE_NAME(pcbp->sockdata->node));

	MALLOC(sg, struct sockaddr_ng *, sg_len, M_SONAME, M_WAITOK | M_ZERO);

	if (NG_NODE_HAS_NAME(pcbp->sockdata->node))
		bcopy(NG_NODE_NAME(pcbp->sockdata->node), sg->sg_data, namelen);
	splx(s);

	sg->sg_len = sg_len;
	sg->sg_family = AF_NETGRAPH;
	*addr = (struct sockaddr *)sg;

	return (0);
}

/*
 * Attach a socket to it's protocol specific partner.
 * For a control socket, actually create a netgraph node and attach
 * to it as well.
 */

static int
ng_attach_cntl(struct socket *so)
{
	struct ngsock *privdata;
	struct ngpcb *pcbp;
	int error;

	/* Setup protocol control block */
	if ((error = ng_attach_common(so, NG_CONTROL)) != 0)
		return (error);
	pcbp = sotongpcb(so);

	/* Allocate node private info */
	MALLOC(privdata, struct ngsock *,
	    sizeof(*privdata), M_NETGRAPH_SOCK, M_WAITOK | M_ZERO);
	if (privdata == NULL) {
		ng_detach_common(pcbp, NG_CONTROL);
		return (ENOMEM);
	}

	/* Make the generic node components */
	if ((error = ng_make_node_common(&typestruct, &privdata->node)) != 0) {
		FREE(privdata, M_NETGRAPH_SOCK);
		ng_detach_common(pcbp, NG_CONTROL);
		return (error);
	}
	NG_NODE_SET_PRIVATE(privdata->node, privdata);

	/* Link the pcb and the node private data */
	privdata->ctlsock = pcbp;
	pcbp->sockdata = privdata;
	privdata->refs++;
	return (0);
}

static int
ng_attach_data(struct socket *so)
{
	return(ng_attach_common(so, NG_DATA));
}

/*
 * Set up a socket protocol control block.
 * This code is shared between control and data sockets.
 */
static int
ng_attach_common(struct socket *so, int type)
{
	struct ngpcb *pcbp;
	int error;

	/* Standard socket setup stuff */
	error = soreserve(so, ngpdg_sendspace, ngpdg_recvspace);
	if (error)
		return (error);

	/* Allocate the pcb */
	MALLOC(pcbp, struct ngpcb *, sizeof(*pcbp), M_PCB, M_WAITOK | M_ZERO);
	if (pcbp == NULL)
		return (ENOMEM);
	pcbp->type = type;

	/* Link the pcb and the socket */
	so->so_pcb = (caddr_t) pcbp;
	pcbp->ng_socket = so;

	/* Add the socket to linked list */
	LIST_INSERT_HEAD(&ngsocklist, pcbp, socks);
	return (0);
}

/*
 * Disassociate the socket from it's protocol specific
 * partner. If it's attached to a node's private data structure,
 * then unlink from that too. If we were the last socket attached to it,
 * then shut down the entire node. Shared code for control and data sockets.
 */
static void
ng_detach_common(struct ngpcb *pcbp, int which)
{
	struct ngsock *priv;

	if (pcbp->sockdata) {
		priv = pcbp->sockdata;
		pcbp->sockdata = NULL;
		switch (which) {
		case NG_CONTROL:
			priv->ctlsock = NULL;
			break;
		case NG_DATA:
			priv->datasock = NULL;
			break;
		default:
			panic(__func__);
		}
		if ((--priv->refs == 0) && (priv->node != NULL))
			ng_rmnode_self(priv->node);
	}
	pcbp->ng_socket->so_pcb = NULL;
	pcbp->ng_socket = NULL;
	LIST_REMOVE(pcbp, socks);
	FREE(pcbp, M_PCB);
}

#ifdef NOTYET
/*
 * File descriptors can be passed into an AF_NETGRAPH socket.
 * Note, that file descriptors cannot be passed OUT.
 * Only character device descriptors are accepted.
 * Character devices are useful to connect a graph to a device,
 * which after all is the purpose of this whole system.
 */
static int
ng_internalize(struct mbuf *control, struct thread *td)
{
	const struct cmsghdr *cm = mtod(control, const struct cmsghdr *);
	struct file *fp;
	struct vnode *vn;
	int oldfds;
	int fd;

	if (cm->cmsg_type != SCM_RIGHTS || cm->cmsg_level != SOL_SOCKET ||
	    cm->cmsg_len != control->m_len) {
		TRAP_ERROR;
		return (EINVAL);
	}

	/* Check there is only one FD. XXX what would more than one signify? */
	oldfds = ((caddr_t)cm + cm->cmsg_len - (caddr_t)data) / sizeof (int);
	if (oldfds != 1) {
		TRAP_ERROR;
		return (EINVAL);
	}

	/* Check that the FD given is legit. and change it to a pointer to a
	 * struct file. */
	fd = CMSG_DATA(cm);
	if ((error = fget(td, fd, &fp)) != 0)
		return (error);

	/* Depending on what kind of resource it is, act differently. For
	 * devices, we treat it as a file. For an AF_NETGRAPH socket,
	 * shortcut straight to the node. */
	switch (fp->f_type) {
	case DTYPE_VNODE:
		vn = (struct vnode *) fp->f_data;
		if (vn && (vn->v_type == VCHR)) {
			/* for a VCHR, actually reference the FILE */
			fp->f_count++;
			/* XXX then what :) */
			/* how to pass on to other modules? */
		} else {
			fdrop(fp, td);
			TRAP_ERROR;
			return (EINVAL);
		}
		break;
	default:
		fdrop(fp, td);
		TRAP_ERROR;
		return (EINVAL);
	}
	fdrop(fp, td);
	return (0);
}
#endif	/* NOTYET */

/*
 * Connect the data socket to a named control socket node.
 */
static int
ng_connect_data(struct sockaddr *nam, struct ngpcb *pcbp)
{
	struct sockaddr_ng *sap;
	node_p farnode;
	struct ngsock *priv;
	int error;
	item_p item;

	/* If we are already connected, don't do it again */
	if (pcbp->sockdata != NULL)
		return (EISCONN);

	/* Find the target (victim) and check it doesn't already have a data
	 * socket. Also check it is a 'socket' type node.
	 * Use ng_package_data() and address_path() to do this.
	 */

	sap = (struct sockaddr_ng *) nam;
	/* The item will hold the node reference */
	item = ng_package_data(NULL, NULL);
	if (item == NULL) {
		return (ENOMEM);
	}
	if ((error = ng_address_path(NULL, item,  sap->sg_data, 0)))
		return (error); /* item is freed on failure */

	/*
	 * Extract node from item and free item. Remember we now have
	 * a reference on the node. The item holds it for us.
	 * when we free the item we release the reference.
	 */
	farnode = item->el_dest; /* shortcut */
	if (strcmp(farnode->nd_type->name, NG_SOCKET_NODE_TYPE) != 0) {
		NG_FREE_ITEM(item); /* drop the reference to the node */
		return (EINVAL);
	}
	priv = NG_NODE_PRIVATE(farnode);
	if (priv->datasock != NULL) {
		NG_FREE_ITEM(item);	/* drop the reference to the node */
		return (EADDRINUSE);
	}

	/*
	 * Link the PCB and the private data struct. and note the extra
	 * reference. Drop the extra reference on the node.
	 */
	priv->datasock = pcbp;
	pcbp->sockdata = priv;
	priv->refs++; /* XXX possible race if it's being freed */
	NG_FREE_ITEM(item);	/* drop the reference to the node */
	return (0);
}

/*
 * Binding a socket means giving the corresponding node a name
 */
static int
ng_bind(struct sockaddr *nam, struct ngpcb *pcbp)
{
	struct ngsock *const priv = pcbp->sockdata;
	struct sockaddr_ng *const sap = (struct sockaddr_ng *) nam;

	if (priv == NULL) {
		TRAP_ERROR;
		return (EINVAL);
	}
	if ((sap->sg_len < 4)
	||  (sap->sg_len > (NG_NODELEN + 3))
	||  (sap->sg_data[0] == '\0')
	||  (sap->sg_data[sap->sg_len - 3] != '\0')) {
		TRAP_ERROR;
		return (EINVAL);
	}
	return (ng_name_node(priv->node, sap->sg_data));
}

/*
 * Take a message and pass it up to the control socket associated
 * with the node.
 */
static int
ship_msg(struct ngpcb *pcbp, struct ng_mesg *msg, struct sockaddr_ng *addr)
{
	struct socket *const so = pcbp->ng_socket;
	struct mbuf *mdata;
	int msglen;

	/* Copy the message itself into an mbuf chain */
	msglen = sizeof(struct ng_mesg) + msg->header.arglen;
	mdata = m_devget((caddr_t) msg, msglen, 0, NULL, NULL);

	/* Here we free the message, as we are the end of the line.
	 * We need to do that regardless of whether we got mbufs. */
	NG_FREE_MSG(msg);

	if (mdata == NULL) {
		TRAP_ERROR;
		return (ENOBUFS);
	}

	/* Send it up to the socket */
	if (sbappendaddr(&so->so_rcv,
	    (struct sockaddr *) addr, mdata, NULL) == 0) {
		TRAP_ERROR;
		m_freem(mdata);
		return (ENOBUFS);
	}
	sorwakeup(so);
	return (0);
}

/*
 * You can only create new nodes from the socket end of things.
 */
static int
ngs_constructor(node_p nodep)
{
	return (EINVAL);
}

/*
 * We allow any hook to be connected to the node.
 * There is no per-hook private information though.
 */
static int
ngs_newhook(node_p node, hook_p hook, const char *name)
{
	NG_HOOK_SET_PRIVATE(hook, NG_NODE_PRIVATE(node));
	return (0);
}

/* 
 * if only one hook, allow read(2) and write(2) to work.
 */
static int
ngs_connect(hook_p hook)
{
	node_p node = NG_HOOK_NODE(hook);
	struct ngsock *priv = NG_NODE_PRIVATE(node);

	if ((priv->datasock)
	&&  (priv->datasock->ng_socket)) {
		if (NG_NODE_NUMHOOKS(node) == 1) {
			priv->datasock->ng_socket->so_state |= SS_ISCONNECTED;
		} else {
			priv->datasock->ng_socket->so_state &= ~SS_ISCONNECTED;
		}
	}
	return (0);
}

/*
 * Incoming messages get passed up to the control socket.
 * Unless they are for us specifically (socket_type)
 */
static int
ngs_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	struct ngsock *const priv = NG_NODE_PRIVATE(node);
	struct ngpcb *const pcbp = priv->ctlsock;
	struct sockaddr_ng *addr;
	int addrlen;
	int error = 0;
	struct	ng_mesg *msg;
	ng_ID_t	retaddr = NGI_RETADDR(item);
	char	retabuf[32];

	NGI_GET_MSG(item, msg);
	NG_FREE_ITEM(item); /* we have all we need */

	/* Only allow mesgs to be passed if we have the control socket.
	 * Data sockets can only support the generic messages. */
	if (pcbp == NULL) {
		TRAP_ERROR;
		return (EINVAL);
	}
#ifdef TRACE_MESSAGES
printf("[%x]:---------->[socket]: c=<%d>cmd=%x(%s) f=%x #%d\n",
retaddr,
msg->header.typecookie,
msg->header.cmd,
msg->header.cmdstr,
msg->header.flags,
msg->header.token);

#endif

	if (msg->header.typecookie == NGM_SOCKET_COOKIE) {
		switch (msg->header.cmd) {
		case NGM_SOCK_CMD_NOLINGER:
			priv->flags |= NGS_FLAG_NOLINGER;
			break;
		case NGM_SOCK_CMD_LINGER:
			priv->flags &= ~NGS_FLAG_NOLINGER;
			break;
		default:
			error = EINVAL;		/* unknown command */
		}
		/* Free the message and return */
		NG_FREE_MSG(msg);
		return(error);

	}
	/* Get the return address into a sockaddr */
	sprintf(retabuf,"[%x]:", retaddr);
	addrlen = strlen(retabuf);
	MALLOC(addr, struct sockaddr_ng *, addrlen + 4, M_NETGRAPH_PATH, M_NOWAIT);
	if (addr == NULL) {
		TRAP_ERROR;
		return (ENOMEM);
	}
	addr->sg_len = addrlen + 3;
	addr->sg_family = AF_NETGRAPH;
	bcopy(retabuf, addr->sg_data, addrlen);
	addr->sg_data[addrlen] = '\0';

	/* Send it up */
	error = ship_msg(pcbp, msg, addr);
	FREE(addr, M_NETGRAPH_PATH);
	return (error);
}

/*
 * Receive data on a hook
 */
static int
ngs_rcvdata(hook_p hook, item_p item)
{
	struct ngsock *const priv = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct ngpcb *const pcbp = priv->datasock;
	struct socket *so;
	struct sockaddr_ng *addr;
	char *addrbuf[NG_HOOKLEN + 1 + 4];
	int addrlen;
	struct mbuf *m;

	NGI_GET_M(item, m);
	NG_FREE_ITEM(item);
	/* If there is no data socket, black-hole it */
	if (pcbp == NULL) {
		NG_FREE_M(m);
		return (0);
	}
	so = pcbp->ng_socket;

	/* Get the return address into a sockaddr. */
	addrlen = strlen(NG_HOOK_NAME(hook));	/* <= NG_HOOKLEN */
	addr = (struct sockaddr_ng *) addrbuf;
	addr->sg_len = addrlen + 3;
	addr->sg_family = AF_NETGRAPH;
	bcopy(NG_HOOK_NAME(hook), addr->sg_data, addrlen);
	addr->sg_data[addrlen] = '\0';

	/* Try to tell the socket which hook it came in on */
	if (sbappendaddr(&so->so_rcv, (struct sockaddr *) addr, m, NULL) == 0) {
		m_freem(m);
		TRAP_ERROR;
		return (ENOBUFS);
	}
	sorwakeup(so);
	return (0);
}

/*
 * Hook disconnection
 *
 * For this type, removal of the last link destroys the node
 * if the NOLINGER flag is set.
 */
static int
ngs_disconnect(hook_p hook)
{
	node_p node = NG_HOOK_NODE(hook);
	struct ngsock *const priv = NG_NODE_PRIVATE(node);

	if ((priv->datasock)
	&&  (priv->datasock->ng_socket)) {
		if (NG_NODE_NUMHOOKS(node) == 1) {
			priv->datasock->ng_socket->so_state |= SS_ISCONNECTED;
		} else {
			priv->datasock->ng_socket->so_state &= ~SS_ISCONNECTED;
		}
	}

	if ((priv->flags & NGS_FLAG_NOLINGER )
	&& (NG_NODE_NUMHOOKS(node) == 0)
	&& (NG_NODE_IS_VALID(node))) {
		ng_rmnode_self(node);
	}
	return (0);
}

/*
 * Do local shutdown processing.
 * In this case, that involves making sure the socket
 * knows we should be shutting down.
 */
static int
ngs_shutdown(node_p node)
{
	struct ngsock *const priv = NG_NODE_PRIVATE(node);
	struct ngpcb *const dpcbp = priv->datasock;
	struct ngpcb *const pcbp = priv->ctlsock;

	if (dpcbp != NULL) {
		soisdisconnected(dpcbp->ng_socket);
		dpcbp->sockdata = NULL;
		priv->datasock = NULL;
		priv->refs--;
	}
	if (pcbp != NULL) {
		soisdisconnected(pcbp->ng_socket);
		pcbp->sockdata = NULL;
		priv->ctlsock = NULL;
		priv->refs--;
	}
	NG_NODE_SET_PRIVATE(node, NULL);
	NG_NODE_UNREF(node);
	FREE(priv, M_NETGRAPH_SOCK);
	return (0);
}

static	int
dummy_disconnect(struct socket *so)
{
	return (0);
}
/*
 * Control and data socket type descriptors
 */

static struct pr_usrreqs ngc_usrreqs = {
	NULL,			/* abort */
	pru_accept_notsupp,
	ngc_attach,
	ngc_bind,
	ngc_connect,
	pru_connect2_notsupp,
	pru_control_notsupp,
	ngc_detach,
	dummy_disconnect,	/* disconnect */
	pru_listen_notsupp,
	NULL,			/* setpeeraddr */
	pru_rcvd_notsupp,
	pru_rcvoob_notsupp,
	ngc_send,
	pru_sense_null,
	NULL,			/* shutdown */
	ng_setsockaddr,
	sosend,
	soreceive,
	sopoll
};

static struct pr_usrreqs ngd_usrreqs = {
	NULL,			/* abort */
	pru_accept_notsupp,
	ngd_attach,
	NULL,			/* bind */
	ngd_connect,
	pru_connect2_notsupp,
	pru_control_notsupp,
	ngd_detach,
	dummy_disconnect,	/* disconnect */
	pru_listen_notsupp,
	NULL,			/* setpeeraddr */
	pru_rcvd_notsupp,
	pru_rcvoob_notsupp,
	ngd_send,
	pru_sense_null,
	NULL,			/* shutdown */
	ng_setsockaddr,
	sosend,
	soreceive,
	sopoll
};

/*
 * Definitions of protocols supported in the NETGRAPH domain.
 */

extern struct domain ngdomain;		/* stop compiler warnings */

static struct protosw ngsw[] = {
	{
		SOCK_DGRAM,		/* protocol type */
		&ngdomain,		/* backpointer to domain */
		NG_CONTROL,
		PR_ATOMIC | PR_ADDR /* | PR_RIGHTS */,	/* flags */
		0, 0, 0, 0,		/* input, output, ctlinput, ctloutput */
		NULL,			/* ousrreq */
		0, 0, 0, 0,		/* init, fasttimeo, slowtimo, drain */
		&ngc_usrreqs,		/* usrreq table (above) */
		/*{NULL}*/		/* pffh (protocol filter head?) */
	},
	{
		SOCK_DGRAM,		/* protocol type */
		&ngdomain,		/* backpointer to domain */
		NG_DATA,
		PR_ATOMIC | PR_ADDR,	/* flags */
		0, 0, 0, 0,		/* input, output, ctlinput, ctloutput */
		NULL,			/* ousrreq() */
		0, 0, 0, 0,		/* init, fasttimeo, slowtimo, drain */
		&ngd_usrreqs,		/* usrreq table (above) */
		/*{NULL}*/		/* pffh (protocol filter head?) */
	}
};

struct domain ngdomain = {
	AF_NETGRAPH,
	"netgraph",
	NULL,					/* init() */
	NULL,					/* externalise() */
	NULL,					/* dispose() */
	ngsw,					/* protosw entry */
	&ngsw[sizeof(ngsw) / sizeof(ngsw[0])], 	/* Number of protosw entries */
	NULL,					/* next domain in list */
	NULL,					/* rtattach() */
	0,					/* arg to rtattach in bits */
	0					/* maxrtkey */
};

/*
 * Handle loading and unloading for this node type
 * This is to handle auxiliary linkages (e.g protocol domain addition).
 */
static int
ngs_mod_event(module_t mod, int event, void *data)
{
	int error = 0;

	switch (event) {
	case MOD_LOAD:
		/* Register protocol domain */
		net_add_domain(&ngdomain);
		break;
	case MOD_UNLOAD:
		/* Insure there are no open netgraph sockets */
		if (!LIST_EMPTY(&ngsocklist)) {
			error = EBUSY;
			break;
		}

#ifdef NOTYET
		if ((LIST_EMPTY(&ngsocklist)) && (typestruct.refs == 0)) {
		/* Unregister protocol domain XXX can't do this yet.. */
			if ((error = net_rm_domain(&ngdomain)) != 0)
				break;
		} else
#endif
			error = EBUSY;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

SYSCTL_INT(_net_graph, OID_AUTO, family, CTLFLAG_RD, 0, AF_NETGRAPH, "");
SYSCTL_NODE(_net_graph, OID_AUTO, data, CTLFLAG_RW, 0, "DATA");
SYSCTL_INT(_net_graph_data, OID_AUTO, proto, CTLFLAG_RD, 0, NG_DATA, "");
SYSCTL_NODE(_net_graph, OID_AUTO, control, CTLFLAG_RW, 0, "CONTROL");
SYSCTL_INT(_net_graph_control, OID_AUTO, proto, CTLFLAG_RD, 0, NG_CONTROL, "");

