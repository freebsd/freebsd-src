/*
 * ng_base.c
 */

/*-
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
 * Authors: Julian Elischer <julian@freebsd.org>
 *          Archie Cobbs <archie@freebsd.org>
 *
 * $FreeBSD$
 * $Whistle: ng_base.c,v 1.39 1999/01/28 23:54:53 julian Exp $
 */

/*
 * This file implements the base netgraph code.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/errno.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <net/netisr.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>

MODULE_VERSION(netgraph, NG_ABI_VERSION);

/* List of all active nodes */
static LIST_HEAD(, ng_node) ng_nodelist;
static struct mtx	ng_nodelist_mtx;

/* Mutex to protect topology events. */
static struct mtx	ng_topo_mtx;

#ifdef	NETGRAPH_DEBUG
static struct mtx	ngq_mtx;	/* protects the queue item list */

static SLIST_HEAD(, ng_node) ng_allnodes;
static LIST_HEAD(, ng_node) ng_freenodes; /* in debug, we never free() them */
static SLIST_HEAD(, ng_hook) ng_allhooks;
static LIST_HEAD(, ng_hook) ng_freehooks; /* in debug, we never free() them */

static void ng_dumpitems(void);
static void ng_dumpnodes(void);
static void ng_dumphooks(void);

#endif	/* NETGRAPH_DEBUG */
/*
 * DEAD versions of the structures.
 * In order to avoid races, it is sometimes neccesary to point
 * at SOMETHING even though theoretically, the current entity is
 * INVALID. Use these to avoid these races.
 */
struct ng_type ng_deadtype = {
	NG_ABI_VERSION,
	"dead",
	NULL,	/* modevent */
	NULL,	/* constructor */
	NULL,	/* rcvmsg */
	NULL,	/* shutdown */
	NULL,	/* newhook */
	NULL,	/* findhook */
	NULL,	/* connect */
	NULL,	/* rcvdata */
	NULL,	/* disconnect */
	NULL, 	/* cmdlist */
};

struct ng_node ng_deadnode = {
	"dead",
	&ng_deadtype,	
	NGF_INVALID,
	1,	/* refs */
	0,	/* numhooks */
	NULL,	/* private */
	0,	/* ID */
	LIST_HEAD_INITIALIZER(ng_deadnode.hooks),
	{},	/* all_nodes list entry */
	{},	/* id hashtable list entry */
	{},	/* workqueue entry */
	{	0,
		{}, /* should never use! (should hang) */
		NULL,
		&ng_deadnode.nd_input_queue.queue,
		&ng_deadnode
	},
#ifdef	NETGRAPH_DEBUG
	ND_MAGIC,
	__FILE__,
	__LINE__,
	{NULL}
#endif	/* NETGRAPH_DEBUG */
};

struct ng_hook ng_deadhook = {
	"dead",
	NULL,		/* private */
	HK_INVALID | HK_DEAD,
	1,		/* refs always >= 1 */
	0,		/* undefined data link type */
	&ng_deadhook,	/* Peer is self */
	&ng_deadnode,	/* attached to deadnode */
	{},		/* hooks list */
	NULL,		/* override rcvmsg() */
	NULL,		/* override rcvdata() */
#ifdef	NETGRAPH_DEBUG
	HK_MAGIC,
	__FILE__,
	__LINE__,
	{NULL}
#endif	/* NETGRAPH_DEBUG */
};

/*
 * END DEAD STRUCTURES
 */
/* List nodes with unallocated work */
static TAILQ_HEAD(, ng_node) ng_worklist = TAILQ_HEAD_INITIALIZER(ng_worklist);
static struct mtx	ng_worklist_mtx;   /* MUST LOCK NODE FIRST */

/* List of installed types */
static LIST_HEAD(, ng_type) ng_typelist;
static struct mtx	ng_typelist_mtx;

/* Hash related definitions */
/* XXX Don't need to initialise them because it's a LIST */
#define NG_ID_HASH_SIZE 32 /* most systems wont need even this many */
static LIST_HEAD(, ng_node) ng_ID_hash[NG_ID_HASH_SIZE];
static struct mtx	ng_idhash_mtx;
/* Method to find a node.. used twice so do it here */
#define NG_IDHASH_FN(ID) ((ID) % (NG_ID_HASH_SIZE))
#define NG_IDHASH_FIND(ID, node)					\
	do { 								\
		mtx_assert(&ng_idhash_mtx, MA_OWNED);			\
		LIST_FOREACH(node, &ng_ID_hash[NG_IDHASH_FN(ID)],	\
						nd_idnodes) {		\
			if (NG_NODE_IS_VALID(node)			\
			&& (NG_NODE_ID(node) == ID)) {			\
				break;					\
			}						\
		}							\
	} while (0)


/* Internal functions */
static int	ng_add_hook(node_p node, const char *name, hook_p * hookp);
static int	ng_generic_msg(node_p here, item_p item, hook_p lasthook);
static ng_ID_t	ng_decodeidname(const char *name);
static int	ngb_mod_event(module_t mod, int event, void *data);
static void	ng_worklist_remove(node_p node);
static void	ngintr(void);
static int	ng_apply_item(node_p node, item_p item, int rw);
static void	ng_flush_input_queue(struct ng_queue * ngq);
static void	ng_setisr(node_p node);
static node_p	ng_ID2noderef(ng_ID_t ID);
static int	ng_con_nodes(node_p node, const char *name, node_p node2,
							const char *name2);
static void	ng_con_part2(node_p node, hook_p hook, void *arg1, int arg2);
static void	ng_con_part3(node_p node, hook_p hook, void *arg1, int arg2);
static int	ng_mkpeer(node_p node, const char *name,
						const char *name2, char *type);

/* Imported, these used to be externally visible, some may go back. */
void	ng_destroy_hook(hook_p hook);
node_p	ng_name2noderef(node_p node, const char *name);
int	ng_path2noderef(node_p here, const char *path,
	node_p *dest, hook_p *lasthook);
int	ng_make_node(const char *type, node_p *nodepp);
int	ng_path_parse(char *addr, char **node, char **path, char **hook);
void	ng_rmnode(node_p node, hook_p dummy1, void *dummy2, int dummy3);
void	ng_unname(node_p node);


/* Our own netgraph malloc type */
MALLOC_DEFINE(M_NETGRAPH, "netgraph", "netgraph structures and ctrl messages");
MALLOC_DEFINE(M_NETGRAPH_HOOK, "netgraph_hook", "netgraph hook structures");
MALLOC_DEFINE(M_NETGRAPH_NODE, "netgraph_node", "netgraph node structures");
MALLOC_DEFINE(M_NETGRAPH_ITEM, "netgraph_item", "netgraph item structures");
MALLOC_DEFINE(M_NETGRAPH_MSG, "netgraph_msg", "netgraph name storage");

/* Should not be visible outside this file */

#define _NG_ALLOC_HOOK(hook) \
	MALLOC(hook, hook_p, sizeof(*hook), M_NETGRAPH_HOOK, M_NOWAIT | M_ZERO)
#define _NG_ALLOC_NODE(node) \
	MALLOC(node, node_p, sizeof(*node), M_NETGRAPH_NODE, M_NOWAIT | M_ZERO)

#ifdef NETGRAPH_DEBUG /*----------------------------------------------*/
/*
 * In debug mode:
 * In an attempt to help track reference count screwups
 * we do not free objects back to the malloc system, but keep them
 * in a local cache where we can examine them and keep information safely
 * after they have been freed.
 * We use this scheme for nodes and hooks, and to some extent for items.
 */
static __inline hook_p
ng_alloc_hook(void)
{
	hook_p hook;
	SLIST_ENTRY(ng_hook) temp;
	mtx_lock(&ng_nodelist_mtx);
	hook = LIST_FIRST(&ng_freehooks);
	if (hook) {
		LIST_REMOVE(hook, hk_hooks);
		bcopy(&hook->hk_all, &temp, sizeof(temp));
		bzero(hook, sizeof(struct ng_hook));
		bcopy(&temp, &hook->hk_all, sizeof(temp));
		mtx_unlock(&ng_nodelist_mtx);
		hook->hk_magic = HK_MAGIC;
	} else {
		mtx_unlock(&ng_nodelist_mtx);
		_NG_ALLOC_HOOK(hook);
		if (hook) {
			hook->hk_magic = HK_MAGIC;
			mtx_lock(&ng_nodelist_mtx);
			SLIST_INSERT_HEAD(&ng_allhooks, hook, hk_all);
			mtx_unlock(&ng_nodelist_mtx);
		}
	}
	return (hook);
}

static __inline node_p
ng_alloc_node(void)
{
	node_p node;
	SLIST_ENTRY(ng_node) temp;
	mtx_lock(&ng_nodelist_mtx);
	node = LIST_FIRST(&ng_freenodes);
	if (node) {
		LIST_REMOVE(node, nd_nodes);
		bcopy(&node->nd_all, &temp, sizeof(temp));
		bzero(node, sizeof(struct ng_node));
		bcopy(&temp, &node->nd_all, sizeof(temp));
		mtx_unlock(&ng_nodelist_mtx);
		node->nd_magic = ND_MAGIC;
	} else {
		mtx_unlock(&ng_nodelist_mtx);
		_NG_ALLOC_NODE(node);
		if (node) {
			node->nd_magic = ND_MAGIC;
			mtx_lock(&ng_nodelist_mtx);
			SLIST_INSERT_HEAD(&ng_allnodes, node, nd_all);
			mtx_unlock(&ng_nodelist_mtx);
		}
	}
	return (node);
}

#define NG_ALLOC_HOOK(hook) do { (hook) = ng_alloc_hook(); } while (0)
#define NG_ALLOC_NODE(node) do { (node) = ng_alloc_node(); } while (0)


#define NG_FREE_HOOK(hook)						\
	do {								\
		mtx_lock(&ng_nodelist_mtx);			\
		LIST_INSERT_HEAD(&ng_freehooks, hook, hk_hooks);	\
		hook->hk_magic = 0;					\
		mtx_unlock(&ng_nodelist_mtx);			\
	} while (0)

#define NG_FREE_NODE(node)						\
	do {								\
		mtx_lock(&ng_nodelist_mtx);			\
		LIST_INSERT_HEAD(&ng_freenodes, node, nd_nodes);	\
		node->nd_magic = 0;					\
		mtx_unlock(&ng_nodelist_mtx);			\
	} while (0)

#else /* NETGRAPH_DEBUG */ /*----------------------------------------------*/

#define NG_ALLOC_HOOK(hook) _NG_ALLOC_HOOK(hook)
#define NG_ALLOC_NODE(node) _NG_ALLOC_NODE(node)

#define NG_FREE_HOOK(hook) do { FREE((hook), M_NETGRAPH_HOOK); } while (0)
#define NG_FREE_NODE(node) do { FREE((node), M_NETGRAPH_NODE); } while (0)

#endif /* NETGRAPH_DEBUG */ /*----------------------------------------------*/

/* Set this to kdb_enter("X") to catch all errors as they occur */
#ifndef TRAP_ERROR
#define TRAP_ERROR()
#endif

static	ng_ID_t nextID = 1;

#ifdef INVARIANTS
#define CHECK_DATA_MBUF(m)	do {					\
		struct mbuf *n;						\
		int total;						\
									\
		M_ASSERTPKTHDR(m);					\
		for (total = 0, n = (m); n != NULL; n = n->m_next) {	\
			total += n->m_len;				\
			if (n->m_nextpkt != NULL)			\
				panic("%s: m_nextpkt", __func__);	\
		}							\
									\
		if ((m)->m_pkthdr.len != total) {			\
			panic("%s: %d != %d",				\
			    __func__, (m)->m_pkthdr.len, total);	\
		}							\
	} while (0)
#else
#define CHECK_DATA_MBUF(m)
#endif


/************************************************************************
	Parse type definitions for generic messages
************************************************************************/

/* Handy structure parse type defining macro */
#define DEFINE_PARSE_STRUCT_TYPE(lo, up, args)				\
static const struct ng_parse_struct_field				\
	ng_ ## lo ## _type_fields[] = NG_GENERIC_ ## up ## _INFO args;	\
static const struct ng_parse_type ng_generic_ ## lo ## _type = {	\
	&ng_parse_struct_type,						\
	&ng_ ## lo ## _type_fields					\
}

DEFINE_PARSE_STRUCT_TYPE(mkpeer, MKPEER, ());
DEFINE_PARSE_STRUCT_TYPE(connect, CONNECT, ());
DEFINE_PARSE_STRUCT_TYPE(name, NAME, ());
DEFINE_PARSE_STRUCT_TYPE(rmhook, RMHOOK, ());
DEFINE_PARSE_STRUCT_TYPE(nodeinfo, NODEINFO, ());
DEFINE_PARSE_STRUCT_TYPE(typeinfo, TYPEINFO, ());
DEFINE_PARSE_STRUCT_TYPE(linkinfo, LINKINFO, (&ng_generic_nodeinfo_type));

/* Get length of an array when the length is stored as a 32 bit
   value immediately preceding the array -- as with struct namelist
   and struct typelist. */
static int
ng_generic_list_getLength(const struct ng_parse_type *type,
	const u_char *start, const u_char *buf)
{
	return *((const u_int32_t *)(buf - 4));
}

/* Get length of the array of struct linkinfo inside a struct hooklist */
static int
ng_generic_linkinfo_getLength(const struct ng_parse_type *type,
	const u_char *start, const u_char *buf)
{
	const struct hooklist *hl = (const struct hooklist *)start;

	return hl->nodeinfo.hooks;
}

/* Array type for a variable length array of struct namelist */
static const struct ng_parse_array_info ng_nodeinfoarray_type_info = {
	&ng_generic_nodeinfo_type,
	&ng_generic_list_getLength
};
static const struct ng_parse_type ng_generic_nodeinfoarray_type = {
	&ng_parse_array_type,
	&ng_nodeinfoarray_type_info
};

/* Array type for a variable length array of struct typelist */
static const struct ng_parse_array_info ng_typeinfoarray_type_info = {
	&ng_generic_typeinfo_type,
	&ng_generic_list_getLength
};
static const struct ng_parse_type ng_generic_typeinfoarray_type = {
	&ng_parse_array_type,
	&ng_typeinfoarray_type_info
};

/* Array type for array of struct linkinfo in struct hooklist */
static const struct ng_parse_array_info ng_generic_linkinfo_array_type_info = {
	&ng_generic_linkinfo_type,
	&ng_generic_linkinfo_getLength
};
static const struct ng_parse_type ng_generic_linkinfo_array_type = {
	&ng_parse_array_type,
	&ng_generic_linkinfo_array_type_info
};

DEFINE_PARSE_STRUCT_TYPE(typelist, TYPELIST, (&ng_generic_nodeinfoarray_type));
DEFINE_PARSE_STRUCT_TYPE(hooklist, HOOKLIST,
	(&ng_generic_nodeinfo_type, &ng_generic_linkinfo_array_type));
DEFINE_PARSE_STRUCT_TYPE(listnodes, LISTNODES,
	(&ng_generic_nodeinfoarray_type));

/* List of commands and how to convert arguments to/from ASCII */
static const struct ng_cmdlist ng_generic_cmds[] = {
	{
	  NGM_GENERIC_COOKIE,
	  NGM_SHUTDOWN,
	  "shutdown",
	  NULL,
	  NULL
	},
	{
	  NGM_GENERIC_COOKIE,
	  NGM_MKPEER,
	  "mkpeer",
	  &ng_generic_mkpeer_type,
	  NULL
	},
	{
	  NGM_GENERIC_COOKIE,
	  NGM_CONNECT,
	  "connect",
	  &ng_generic_connect_type,
	  NULL
	},
	{
	  NGM_GENERIC_COOKIE,
	  NGM_NAME,
	  "name",
	  &ng_generic_name_type,
	  NULL
	},
	{
	  NGM_GENERIC_COOKIE,
	  NGM_RMHOOK,
	  "rmhook",
	  &ng_generic_rmhook_type,
	  NULL
	},
	{
	  NGM_GENERIC_COOKIE,
	  NGM_NODEINFO,
	  "nodeinfo",
	  NULL,
	  &ng_generic_nodeinfo_type
	},
	{
	  NGM_GENERIC_COOKIE,
	  NGM_LISTHOOKS,
	  "listhooks",
	  NULL,
	  &ng_generic_hooklist_type
	},
	{
	  NGM_GENERIC_COOKIE,
	  NGM_LISTNAMES,
	  "listnames",
	  NULL,
	  &ng_generic_listnodes_type	/* same as NGM_LISTNODES */
	},
	{
	  NGM_GENERIC_COOKIE,
	  NGM_LISTNODES,
	  "listnodes",
	  NULL,
	  &ng_generic_listnodes_type
	},
	{
	  NGM_GENERIC_COOKIE,
	  NGM_LISTTYPES,
	  "listtypes",
	  NULL,
	  &ng_generic_typeinfo_type
	},
	{
	  NGM_GENERIC_COOKIE,
	  NGM_TEXT_CONFIG,
	  "textconfig",
	  NULL,
	  &ng_parse_string_type
	},
	{
	  NGM_GENERIC_COOKIE,
	  NGM_TEXT_STATUS,
	  "textstatus",
	  NULL,
	  &ng_parse_string_type
	},
	{
	  NGM_GENERIC_COOKIE,
	  NGM_ASCII2BINARY,
	  "ascii2binary",
	  &ng_parse_ng_mesg_type,
	  &ng_parse_ng_mesg_type
	},
	{
	  NGM_GENERIC_COOKIE,
	  NGM_BINARY2ASCII,
	  "binary2ascii",
	  &ng_parse_ng_mesg_type,
	  &ng_parse_ng_mesg_type
	},
	{ 0 }
};

/************************************************************************
			Node routines
************************************************************************/

/*
 * Instantiate a node of the requested type
 */
int
ng_make_node(const char *typename, node_p *nodepp)
{
	struct ng_type *type;
	int	error;

	/* Check that the type makes sense */
	if (typename == NULL) {
		TRAP_ERROR();
		return (EINVAL);
	}

	/* Locate the node type. If we fail we return. Do not try to load
	 * module.
	 */
	if ((type = ng_findtype(typename)) == NULL)
		return (ENXIO);

	/*
	 * If we have a constructor, then make the node and
	 * call the constructor to do type specific initialisation.
	 */
	if (type->constructor != NULL) {
		if ((error = ng_make_node_common(type, nodepp)) == 0) {
			if ((error = ((*type->constructor)(*nodepp)) != 0)) {
				NG_NODE_UNREF(*nodepp);
			}
		}
	} else {
		/*
		 * Node has no constructor. We cannot ask for one
		 * to be made. It must be brought into existance by
		 * some external agency. The external agency should
		 * call ng_make_node_common() directly to get the
		 * netgraph part initialised.
		 */
		TRAP_ERROR();
		error = EINVAL;
	}
	return (error);
}

/*
 * Generic node creation. Called by node initialisation for externally
 * instantiated nodes (e.g. hardware, sockets, etc ).
 * The returned node has a reference count of 1.
 */
int
ng_make_node_common(struct ng_type *type, node_p *nodepp)
{
	node_p node;

	/* Require the node type to have been already installed */
	if (ng_findtype(type->name) == NULL) {
		TRAP_ERROR();
		return (EINVAL);
	}

	/* Make a node and try attach it to the type */
	NG_ALLOC_NODE(node);
	if (node == NULL) {
		TRAP_ERROR();
		return (ENOMEM);
	}
	node->nd_type = type;
	NG_NODE_REF(node);				/* note reference */
	type->refs++;

	mtx_init(&node->nd_input_queue.q_mtx, "ng_node", NULL, MTX_SPIN);
	node->nd_input_queue.queue = NULL;
	node->nd_input_queue.last = &node->nd_input_queue.queue;
	node->nd_input_queue.q_flags = 0;
	node->nd_input_queue.q_node = node;

	/* Initialize hook list for new node */
	LIST_INIT(&node->nd_hooks);

	/* Link us into the node linked list */
	mtx_lock(&ng_nodelist_mtx);
	LIST_INSERT_HEAD(&ng_nodelist, node, nd_nodes);
	mtx_unlock(&ng_nodelist_mtx);


	/* get an ID and put us in the hash chain */
	mtx_lock(&ng_idhash_mtx);
	for (;;) { /* wrap protection, even if silly */
		node_p node2 = NULL;
		node->nd_ID = nextID++; /* 137/second for 1 year before wrap */

		/* Is there a problem with the new number? */
		NG_IDHASH_FIND(node->nd_ID, node2); /* already taken? */
		if ((node->nd_ID != 0) && (node2 == NULL)) {
			break;
		}
	}
	LIST_INSERT_HEAD(&ng_ID_hash[NG_IDHASH_FN(node->nd_ID)],
							node, nd_idnodes);
	mtx_unlock(&ng_idhash_mtx);

	/* Done */
	*nodepp = node;
	return (0);
}

/*
 * Forceably start the shutdown process on a node. Either call
 * it's shutdown method, or do the default shutdown if there is
 * no type-specific method.
 *
 * We can only be called form a shutdown message, so we know we have
 * a writer lock, and therefore exclusive access. It also means
 * that we should not be on the work queue, but we check anyhow.
 *
 * Persistent node types must have a type-specific method which
 * Allocates a new node in which case, this one is irretrievably going away,
 * or cleans up anything it needs, and just makes the node valid again,
 * in which case we allow the node to survive.
 *
 * XXX We need to think of how to tell a persistant node that we
 * REALLY need to go away because the hardware has gone or we
 * are rebooting.... etc.
 */
void
ng_rmnode(node_p node, hook_p dummy1, void *dummy2, int dummy3)
{
	hook_p hook;

	/* Check if it's already shutting down */
	if ((node->nd_flags & NGF_CLOSING) != 0)
		return;

	if (node == &ng_deadnode) {
		printf ("shutdown called on deadnode\n");
		return;
	}

	/* Add an extra reference so it doesn't go away during this */
	NG_NODE_REF(node);

	/*
	 * Mark it invalid so any newcomers know not to try use it
	 * Also add our own mark so we can't recurse
	 * note that NGF_INVALID does not do this as it's also set during
	 * creation
	 */
	node->nd_flags |= NGF_INVALID|NGF_CLOSING;

	/* If node has its pre-shutdown method, then call it first*/
	if (node->nd_type && node->nd_type->close)
		(*node->nd_type->close)(node);

	/* Notify all remaining connected nodes to disconnect */
	while ((hook = LIST_FIRST(&node->nd_hooks)) != NULL)
		ng_destroy_hook(hook);

	/*
	 * Drain the input queue forceably.
	 * it has no hooks so what's it going to do, bleed on someone?
	 * Theoretically we came here from a queue entry that was added
	 * Just before the queue was closed, so it should be empty anyway.
	 * Also removes us from worklist if needed.
	 */
	ng_flush_input_queue(&node->nd_input_queue);

	/* Ask the type if it has anything to do in this case */
	if (node->nd_type && node->nd_type->shutdown) {
		(*node->nd_type->shutdown)(node);
		if (NG_NODE_IS_VALID(node)) {
			/*
			 * Well, blow me down if the node code hasn't declared
			 * that it doesn't want to die.
			 * Presumably it is a persistant node.
			 * If we REALLY want it to go away,
			 *  e.g. hardware going away,
			 * Our caller should set NGF_REALLY_DIE in nd_flags.
			 */
			node->nd_flags &= ~(NGF_INVALID|NGF_CLOSING);
			NG_NODE_UNREF(node); /* Assume they still have theirs */
			return;
		}
	} else {				/* do the default thing */
		NG_NODE_UNREF(node);
	}

	ng_unname(node); /* basically a NOP these days */

	/*
	 * Remove extra reference, possibly the last
	 * Possible other holders of references may include
	 * timeout callouts, but theoretically the node's supposed to
	 * have cancelled them. Possibly hardware dependencies may
	 * force a driver to 'linger' with a reference.
	 */
	NG_NODE_UNREF(node);
}

/*
 * Remove a reference to the node, possibly the last.
 * deadnode always acts as it it were the last.
 */
int
ng_unref_node(node_p node)
{
	int v;

	if (node == &ng_deadnode) {
		return (0);
	}

	do {
		v = node->nd_refs - 1;
	} while (! atomic_cmpset_int(&node->nd_refs, v + 1, v));

	if (v == 0) { /* we were the last */

		mtx_lock(&ng_nodelist_mtx);
		node->nd_type->refs--; /* XXX maybe should get types lock? */
		LIST_REMOVE(node, nd_nodes);
		mtx_unlock(&ng_nodelist_mtx);

		mtx_lock(&ng_idhash_mtx);
		LIST_REMOVE(node, nd_idnodes);
		mtx_unlock(&ng_idhash_mtx);

		mtx_destroy(&node->nd_input_queue.q_mtx);
		NG_FREE_NODE(node);
	}
	return (v);
}

/************************************************************************
			Node ID handling
************************************************************************/
static node_p
ng_ID2noderef(ng_ID_t ID)
{
	node_p node;
	mtx_lock(&ng_idhash_mtx);
	NG_IDHASH_FIND(ID, node);
	if(node)
		NG_NODE_REF(node);
	mtx_unlock(&ng_idhash_mtx);
	return(node);
}

ng_ID_t
ng_node2ID(node_p node)
{
	return (node ? NG_NODE_ID(node) : 0);
}

/************************************************************************
			Node name handling
************************************************************************/

/*
 * Assign a node a name. Once assigned, the name cannot be changed.
 */
int
ng_name_node(node_p node, const char *name)
{
	int i;
	node_p node2;

	/* Check the name is valid */
	for (i = 0; i < NG_NODESIZ; i++) {
		if (name[i] == '\0' || name[i] == '.' || name[i] == ':')
			break;
	}
	if (i == 0 || name[i] != '\0') {
		TRAP_ERROR();
		return (EINVAL);
	}
	if (ng_decodeidname(name) != 0) { /* valid IDs not allowed here */
		TRAP_ERROR();
		return (EINVAL);
	}

	/* Check the name isn't already being used */
	if ((node2 = ng_name2noderef(node, name)) != NULL) {
		NG_NODE_UNREF(node2);
		TRAP_ERROR();
		return (EADDRINUSE);
	}

	/* copy it */
	strlcpy(NG_NODE_NAME(node), name, NG_NODESIZ);

	return (0);
}

/*
 * Find a node by absolute name. The name should NOT end with ':'
 * The name "." means "this node" and "[xxx]" means "the node
 * with ID (ie, at address) xxx".
 *
 * Returns the node if found, else NULL.
 * Eventually should add something faster than a sequential search.
 * Note it aquires a reference on the node so you can be sure it's still there.
 */
node_p
ng_name2noderef(node_p here, const char *name)
{
	node_p node;
	ng_ID_t temp;

	/* "." means "this node" */
	if (strcmp(name, ".") == 0) {
		NG_NODE_REF(here);
		return(here);
	}

	/* Check for name-by-ID */
	if ((temp = ng_decodeidname(name)) != 0) {
		return (ng_ID2noderef(temp));
	}

	/* Find node by name */
	mtx_lock(&ng_nodelist_mtx);
	LIST_FOREACH(node, &ng_nodelist, nd_nodes) {
		if (NG_NODE_IS_VALID(node)
		&& NG_NODE_HAS_NAME(node)
		&& (strcmp(NG_NODE_NAME(node), name) == 0)) {
			break;
		}
	}
	if (node)
		NG_NODE_REF(node);
	mtx_unlock(&ng_nodelist_mtx);
	return (node);
}

/*
 * Decode an ID name, eg. "[f03034de]". Returns 0 if the
 * string is not valid, otherwise returns the value.
 */
static ng_ID_t
ng_decodeidname(const char *name)
{
	const int len = strlen(name);
	char *eptr;
	u_long val;

	/* Check for proper length, brackets, no leading junk */
	if ((len < 3)
	|| (name[0] != '[')
	|| (name[len - 1] != ']')
	|| (!isxdigit(name[1]))) {
		return ((ng_ID_t)0);
	}

	/* Decode number */
	val = strtoul(name + 1, &eptr, 16);
	if ((eptr - name != len - 1)
	|| (val == ULONG_MAX)
	|| (val == 0)) {
		return ((ng_ID_t)0);
	}
	return (ng_ID_t)val;
}

/*
 * Remove a name from a node. This should only be called
 * when shutting down and removing the node.
 * IF we allow name changing this may be more resurected.
 */
void
ng_unname(node_p node)
{
}

/************************************************************************
			Hook routines
 Names are not optional. Hooks are always connected, except for a
 brief moment within these routines. On invalidation or during creation
 they are connected to the 'dead' hook.
************************************************************************/

/*
 * Remove a hook reference
 */
void
ng_unref_hook(hook_p hook)
{
	int v;

	if (hook == &ng_deadhook) {
		return;
	}
	do {
		v = hook->hk_refs;
	} while (! atomic_cmpset_int(&hook->hk_refs, v, v - 1));

	if (v == 1) { /* we were the last */
		if (_NG_HOOK_NODE(hook)) { /* it'll probably be ng_deadnode */
			_NG_NODE_UNREF((_NG_HOOK_NODE(hook)));
			hook->hk_node = NULL;
		}
		NG_FREE_HOOK(hook);
	}
}

/*
 * Add an unconnected hook to a node. Only used internally.
 * Assumes node is locked. (XXX not yet true )
 */
static int
ng_add_hook(node_p node, const char *name, hook_p *hookp)
{
	hook_p hook;
	int error = 0;

	/* Check that the given name is good */
	if (name == NULL) {
		TRAP_ERROR();
		return (EINVAL);
	}
	if (ng_findhook(node, name) != NULL) {
		TRAP_ERROR();
		return (EEXIST);
	}

	/* Allocate the hook and link it up */
	NG_ALLOC_HOOK(hook);
	if (hook == NULL) {
		TRAP_ERROR();
		return (ENOMEM);
	}
	hook->hk_refs = 1;		/* add a reference for us to return */
	hook->hk_flags = HK_INVALID;
	hook->hk_peer = &ng_deadhook;	/* start off this way */
	hook->hk_node = node;
	NG_NODE_REF(node);		/* each hook counts as a reference */

	/* Set hook name */
	strlcpy(NG_HOOK_NAME(hook), name, NG_HOOKSIZ);

	/*
	 * Check if the node type code has something to say about it
	 * If it fails, the unref of the hook will also unref the node.
	 */
	if (node->nd_type->newhook != NULL) {
		if ((error = (*node->nd_type->newhook)(node, hook, name))) {
			NG_HOOK_UNREF(hook);	/* this frees the hook */
			return (error);
		}
	}
	/*
	 * The 'type' agrees so far, so go ahead and link it in.
	 * We'll ask again later when we actually connect the hooks.
	 */
	LIST_INSERT_HEAD(&node->nd_hooks, hook, hk_hooks);
	node->nd_numhooks++;
	NG_HOOK_REF(hook);	/* one for the node */

	if (hookp)
		*hookp = hook;
	return (0);
}

/*
 * Find a hook
 *
 * Node types may supply their own optimized routines for finding
 * hooks.  If none is supplied, we just do a linear search.
 * XXX Possibly we should add a reference to the hook?
 */
hook_p
ng_findhook(node_p node, const char *name)
{
	hook_p hook;

	if (node->nd_type->findhook != NULL)
		return (*node->nd_type->findhook)(node, name);
	LIST_FOREACH(hook, &node->nd_hooks, hk_hooks) {
		if (NG_HOOK_IS_VALID(hook)
		&& (strcmp(NG_HOOK_NAME(hook), name) == 0))
			return (hook);
	}
	return (NULL);
}

/*
 * Destroy a hook
 *
 * As hooks are always attached, this really destroys two hooks.
 * The one given, and the one attached to it. Disconnect the hooks
 * from each other first. We reconnect the peer hook to the 'dead'
 * hook so that it can still exist after we depart. We then
 * send the peer its own destroy message. This ensures that we only
 * interact with the peer's structures when it is locked processing that
 * message. We hold a reference to the peer hook so we are guaranteed that
 * the peer hook and node are still going to exist until
 * we are finished there as the hook holds a ref on the node.
 * We run this same code again on the peer hook, but that time it is already
 * attached to the 'dead' hook.
 *
 * This routine is called at all stages of hook creation
 * on error detection and must be able to handle any such stage.
 */
void
ng_destroy_hook(hook_p hook)
{
	hook_p peer;
	node_p node;

	if (hook == &ng_deadhook) {	/* better safe than sorry */
		printf("ng_destroy_hook called on deadhook\n");
		return;
	}

	/*
	 * Protect divorce process with mutex, to avoid races on
	 * simultaneous disconnect.
	 */
	mtx_lock(&ng_topo_mtx);

	hook->hk_flags |= HK_INVALID;

	peer = NG_HOOK_PEER(hook);
	node = NG_HOOK_NODE(hook);

	if (peer && (peer != &ng_deadhook)) {
		/*
		 * Set the peer to point to ng_deadhook
		 * from this moment on we are effectively independent it.
		 * send it an rmhook message of it's own.
		 */
		peer->hk_peer = &ng_deadhook;	/* They no longer know us */
		hook->hk_peer = &ng_deadhook;	/* Nor us, them */
		if (NG_HOOK_NODE(peer) == &ng_deadnode) {
			/*
			 * If it's already divorced from a node,
			 * just free it.
			 */
			mtx_unlock(&ng_topo_mtx);
		} else {
			mtx_unlock(&ng_topo_mtx);
			ng_rmhook_self(peer); 	/* Send it a surprise */
		}
		NG_HOOK_UNREF(peer);		/* account for peer link */
		NG_HOOK_UNREF(hook);		/* account for peer link */
	} else
		mtx_unlock(&ng_topo_mtx);

	mtx_assert(&ng_topo_mtx, MA_NOTOWNED);

	/*
	 * Remove the hook from the node's list to avoid possible recursion
	 * in case the disconnection results in node shutdown.
	 */
	if (node == &ng_deadnode) { /* happens if called from ng_con_nodes() */
		return;
	}
	LIST_REMOVE(hook, hk_hooks);
	node->nd_numhooks--;
	if (node->nd_type->disconnect) {
		/*
		 * The type handler may elect to destroy the node so don't
		 * trust its existance after this point. (except
		 * that we still hold a reference on it. (which we
		 * inherrited from the hook we are destroying)
		 */
		(*node->nd_type->disconnect) (hook);
	}

	/*
	 * Note that because we will point to ng_deadnode, the original node
	 * is not decremented automatically so we do that manually.
	 */
	_NG_HOOK_NODE(hook) = &ng_deadnode;
	NG_NODE_UNREF(node);	/* We no longer point to it so adjust count */
	NG_HOOK_UNREF(hook);	/* Account for linkage (in list) to node */
}

/*
 * Take two hooks on a node and merge the connection so that the given node
 * is effectively bypassed.
 */
int
ng_bypass(hook_p hook1, hook_p hook2)
{
	if (hook1->hk_node != hook2->hk_node) {
		TRAP_ERROR();
		return (EINVAL);
	}
	hook1->hk_peer->hk_peer = hook2->hk_peer;
	hook2->hk_peer->hk_peer = hook1->hk_peer;

	hook1->hk_peer = &ng_deadhook;
	hook2->hk_peer = &ng_deadhook;

	/* XXX If we ever cache methods on hooks update them as well */
	ng_destroy_hook(hook1);
	ng_destroy_hook(hook2);
	return (0);
}

/*
 * Install a new netgraph type
 */
int
ng_newtype(struct ng_type *tp)
{
	const size_t namelen = strlen(tp->name);

	/* Check version and type name fields */
	if ((tp->version != NG_ABI_VERSION)
	|| (namelen == 0)
	|| (namelen >= NG_TYPESIZ)) {
		TRAP_ERROR();
		if (tp->version != NG_ABI_VERSION) {
			printf("Netgraph: Node type rejected. ABI mismatch. Suggest recompile\n");
		}
		return (EINVAL);
	}

	/* Check for name collision */
	if (ng_findtype(tp->name) != NULL) {
		TRAP_ERROR();
		return (EEXIST);
	}


	/* Link in new type */
	mtx_lock(&ng_typelist_mtx);
	LIST_INSERT_HEAD(&ng_typelist, tp, types);
	tp->refs = 1;	/* first ref is linked list */
	mtx_unlock(&ng_typelist_mtx);
	return (0);
}

/*
 * unlink a netgraph type
 * If no examples exist
 */
int
ng_rmtype(struct ng_type *tp)
{
	/* Check for name collision */
	if (tp->refs != 1) {
		TRAP_ERROR();
		return (EBUSY);
	}

	/* Unlink type */
	mtx_lock(&ng_typelist_mtx);
	LIST_REMOVE(tp, types);
	mtx_unlock(&ng_typelist_mtx);
	return (0);
}

/*
 * Look for a type of the name given
 */
struct ng_type *
ng_findtype(const char *typename)
{
	struct ng_type *type;

	mtx_lock(&ng_typelist_mtx);
	LIST_FOREACH(type, &ng_typelist, types) {
		if (strcmp(type->name, typename) == 0)
			break;
	}
	mtx_unlock(&ng_typelist_mtx);
	return (type);
}

/************************************************************************
			Composite routines
************************************************************************/
/*
 * Connect two nodes using the specified hooks, using queued functions.
 */
static void
ng_con_part3(node_p node, hook_p hook, void *arg1, int arg2)
{

	/*
	 * When we run, we know that the node 'node' is locked for us.
	 * Our caller has a reference on the hook.
	 * Our caller has a reference on the node.
	 * (In this case our caller is ng_apply_item() ).
	 * The peer hook has a reference on the hook.
	 * We are all set up except for the final call to the node, and
	 * the clearing of the INVALID flag.
	 */
	if (NG_HOOK_NODE(hook) == &ng_deadnode) {
		/*
		 * The node must have been freed again since we last visited
		 * here. ng_destry_hook() has this effect but nothing else does.
		 * We should just release our references and
		 * free anything we can think of.
		 * Since we know it's been destroyed, and it's our caller
		 * that holds the references, just return.
		 */
		return ;
	}
	if (hook->hk_node->nd_type->connect) {
		if ((*hook->hk_node->nd_type->connect) (hook)) {
			ng_destroy_hook(hook);	/* also zaps peer */
			printf("failed in ng_con_part3()\n");
			return ;
		}
	}
	/*
	 *  XXX this is wrong for SMP. Possibly we need
	 * to separate out 'create' and 'invalid' flags.
	 * should only set flags on hooks we have locked under our node.
	 */
	hook->hk_flags &= ~HK_INVALID;
	return ;
}

static void
ng_con_part2(node_p node, hook_p hook, void *arg1, int arg2)
{
	hook_p peer;

	/*
	 * When we run, we know that the node 'node' is locked for us.
	 * Our caller has a reference on the hook.
	 * Our caller has a reference on the node.
	 * (In this case our caller is ng_apply_item() ).
	 * The peer hook has a reference on the hook.
	 * our node pointer points to the 'dead' node.
	 * First check the hook name is unique.
	 * Should not happen because we checked before queueing this.
	 */
	if (ng_findhook(node, NG_HOOK_NAME(hook)) != NULL) {
		TRAP_ERROR();
		ng_destroy_hook(hook); /* should destroy peer too */
		printf("failed in ng_con_part2()\n");
		return ;
	}
	/*
	 * Check if the node type code has something to say about it
	 * If it fails, the unref of the hook will also unref the attached node,
	 * however since that node is 'ng_deadnode' this will do nothing.
	 * The peer hook will also be destroyed.
	 */
	if (node->nd_type->newhook != NULL) {
		if ((*node->nd_type->newhook)(node, hook, hook->hk_name)) {
			ng_destroy_hook(hook); /* should destroy peer too */
			printf("failed in ng_con_part2()\n");
			return ;
		}
	}

	/*
	 * The 'type' agrees so far, so go ahead and link it in.
	 * We'll ask again later when we actually connect the hooks.
	 */
	hook->hk_node = node;		/* just overwrite ng_deadnode */
	NG_NODE_REF(node);		/* each hook counts as a reference */
	LIST_INSERT_HEAD(&node->nd_hooks, hook, hk_hooks);
	node->nd_numhooks++;
	NG_HOOK_REF(hook);	/* one for the node */
	
	/*
	 * We now have a symetrical situation, where both hooks have been
	 * linked to their nodes, the newhook methods have been called
	 * And the references are all correct. The hooks are still marked
	 * as invalid, as we have not called the 'connect' methods
	 * yet.
	 * We can call the local one immediatly as we have the
	 * node locked, but we need to queue the remote one.
	 */
	if (hook->hk_node->nd_type->connect) {
		if ((*hook->hk_node->nd_type->connect) (hook)) {
			ng_destroy_hook(hook);	/* also zaps peer */
			printf("failed in ng_con_part2(A)\n");
			return ;
		}
	}

	/*
	 * Acquire topo mutex to avoid race with ng_destroy_hook().
	 */
	mtx_lock(&ng_topo_mtx);
	peer = hook->hk_peer;
	if (peer == &ng_deadhook) {
		mtx_unlock(&ng_topo_mtx);
		printf("failed in ng_con_part2(B)\n");
		ng_destroy_hook(hook);
		return ;
	}
	mtx_unlock(&ng_topo_mtx);

	if (ng_send_fn(peer->hk_node, peer, &ng_con_part3, arg1, arg2)) {
		printf("failed in ng_con_part2(C)\n");
		ng_destroy_hook(hook);	/* also zaps peer */
		return ;
	}
	hook->hk_flags &= ~HK_INVALID; /* need both to be able to work */
	return ;
}

/*
 * Connect this node with another node. We assume that this node is
 * currently locked, as we are only called from an NGM_CONNECT message.
 */
static int
ng_con_nodes(node_p node, const char *name, node_p node2, const char *name2)
{
	int	error;
	hook_p	hook;
	hook_p	hook2;

	if (ng_findhook(node2, name2) != NULL) {
		return(EEXIST);
	}
	if ((error = ng_add_hook(node, name, &hook)))  /* gives us a ref */
		return (error);
	/* Allocate the other hook and link it up */
	NG_ALLOC_HOOK(hook2);
	if (hook2 == NULL) {
		TRAP_ERROR();
		ng_destroy_hook(hook);	/* XXX check ref counts so far */
		NG_HOOK_UNREF(hook);	/* including our ref */
		return (ENOMEM);
	}
	hook2->hk_refs = 1;		/* start with a reference for us. */
	hook2->hk_flags = HK_INVALID;
	hook2->hk_peer = hook;		/* Link the two together */
	hook->hk_peer = hook2;	
	NG_HOOK_REF(hook);		/* Add a ref for the peer to each*/
	NG_HOOK_REF(hook2);
	hook2->hk_node = &ng_deadnode;
	strlcpy(NG_HOOK_NAME(hook2), name2, NG_HOOKSIZ);

	/*
	 * Queue the function above.
	 * Procesing continues in that function in the lock context of
	 * the other node.
	 */
	ng_send_fn(node2, hook2, &ng_con_part2, NULL, 0);

	NG_HOOK_UNREF(hook);		/* Let each hook go if it wants to */
	NG_HOOK_UNREF(hook2);
	return (0);
}

/*
 * Make a peer and connect.
 * We assume that the local node is locked.
 * The new node probably doesn't need a lock until
 * it has a hook, because it cannot really have any work until then,
 * but we should think about it a bit more.
 *
 * The problem may come if the other node also fires up
 * some hardware or a timer or some other source of activation,
 * also it may already get a command msg via it's ID.
 *
 * We could use the same method as ng_con_nodes() but we'd have
 * to add ability to remove the node when failing. (Not hard, just
 * make arg1 point to the node to remove).
 * Unless of course we just ignore failure to connect and leave
 * an unconnected node?
 */
static int
ng_mkpeer(node_p node, const char *name, const char *name2, char *type)
{
	node_p	node2;
	hook_p	hook1, hook2;
	int	error;

	if ((error = ng_make_node(type, &node2))) {
		return (error);
	}

	if ((error = ng_add_hook(node, name, &hook1))) { /* gives us a ref */
		ng_rmnode(node2, NULL, NULL, 0);
		return (error);
	}

	if ((error = ng_add_hook(node2, name2, &hook2))) {
		ng_rmnode(node2, NULL, NULL, 0);
		ng_destroy_hook(hook1);
		NG_HOOK_UNREF(hook1);
		return (error);
	}

	/*
	 * Actually link the two hooks together.
	 */
	hook1->hk_peer = hook2;
	hook2->hk_peer = hook1;

	/* Each hook is referenced by the other */
	NG_HOOK_REF(hook1);
	NG_HOOK_REF(hook2);

	/* Give each node the opportunity to veto the pending connection */
	if (hook1->hk_node->nd_type->connect) {
		error = (*hook1->hk_node->nd_type->connect) (hook1);
	}

	if ((error == 0) && hook2->hk_node->nd_type->connect) {
		error = (*hook2->hk_node->nd_type->connect) (hook2);

	}

	/*
	 * drop the references we were holding on the two hooks.
	 */
	if (error) {
		ng_destroy_hook(hook2);	/* also zaps hook1 */
		ng_rmnode(node2, NULL, NULL, 0);
	} else {
		/* As a last act, allow the hooks to be used */
		hook1->hk_flags &= ~HK_INVALID;
		hook2->hk_flags &= ~HK_INVALID;
	}
	NG_HOOK_UNREF(hook1);
	NG_HOOK_UNREF(hook2);
	return (error);
}

/************************************************************************
		Utility routines to send self messages
************************************************************************/
	
/* Shut this node down as soon as everyone is clear of it */
/* Should add arg "immediatly" to jump the queue */
int
ng_rmnode_self(node_p node)
{
	int		error;

	if (node == &ng_deadnode)
		return (0);
	node->nd_flags |= NGF_INVALID;
	if (node->nd_flags & NGF_CLOSING)
		return (0);

	error = ng_send_fn(node, NULL, &ng_rmnode, NULL, 0);
	return (error);
}

static void
ng_rmhook_part2(node_p node, hook_p hook, void *arg1, int arg2)
{
	ng_destroy_hook(hook);
	return ;
}

int
ng_rmhook_self(hook_p hook)
{
	int		error;
	node_p node = NG_HOOK_NODE(hook);

	if (node == &ng_deadnode)
		return (0);

	error = ng_send_fn(node, hook, &ng_rmhook_part2, NULL, 0);
	return (error);
}

/***********************************************************************
 * Parse and verify a string of the form:  <NODE:><PATH>
 *
 * Such a string can refer to a specific node or a specific hook
 * on a specific node, depending on how you look at it. In the
 * latter case, the PATH component must not end in a dot.
 *
 * Both <NODE:> and <PATH> are optional. The <PATH> is a string
 * of hook names separated by dots. This breaks out the original
 * string, setting *nodep to "NODE" (or NULL if none) and *pathp
 * to "PATH" (or NULL if degenerate). Also, *hookp will point to
 * the final hook component of <PATH>, if any, otherwise NULL.
 *
 * This returns -1 if the path is malformed. The char ** are optional.
 ***********************************************************************/
int
ng_path_parse(char *addr, char **nodep, char **pathp, char **hookp)
{
	char	*node, *path, *hook;
	int	k;

	/*
	 * Extract absolute NODE, if any
	 */
	for (path = addr; *path && *path != ':'; path++);
	if (*path) {
		node = addr;	/* Here's the NODE */
		*path++ = '\0';	/* Here's the PATH */

		/* Node name must not be empty */
		if (!*node)
			return -1;

		/* A name of "." is OK; otherwise '.' not allowed */
		if (strcmp(node, ".") != 0) {
			for (k = 0; node[k]; k++)
				if (node[k] == '.')
					return -1;
		}
	} else {
		node = NULL;	/* No absolute NODE */
		path = addr;	/* Here's the PATH */
	}

	/* Snoop for illegal characters in PATH */
	for (k = 0; path[k]; k++)
		if (path[k] == ':')
			return -1;

	/* Check for no repeated dots in PATH */
	for (k = 0; path[k]; k++)
		if (path[k] == '.' && path[k + 1] == '.')
			return -1;

	/* Remove extra (degenerate) dots from beginning or end of PATH */
	if (path[0] == '.')
		path++;
	if (*path && path[strlen(path) - 1] == '.')
		path[strlen(path) - 1] = 0;

	/* If PATH has a dot, then we're not talking about a hook */
	if (*path) {
		for (hook = path, k = 0; path[k]; k++)
			if (path[k] == '.') {
				hook = NULL;
				break;
			}
	} else
		path = hook = NULL;

	/* Done */
	if (nodep)
		*nodep = node;
	if (pathp)
		*pathp = path;
	if (hookp)
		*hookp = hook;
	return (0);
}

/*
 * Given a path, which may be absolute or relative, and a starting node,
 * return the destination node.
 */
int
ng_path2noderef(node_p here, const char *address,
				node_p *destp, hook_p *lasthook)
{
	char    fullpath[NG_PATHSIZ];
	char   *nodename, *path, pbuf[2];
	node_p  node, oldnode;
	char   *cp;
	hook_p hook = NULL;

	/* Initialize */
	if (destp == NULL) {
		TRAP_ERROR();
		return EINVAL;
	}
	*destp = NULL;

	/* Make a writable copy of address for ng_path_parse() */
	strncpy(fullpath, address, sizeof(fullpath) - 1);
	fullpath[sizeof(fullpath) - 1] = '\0';

	/* Parse out node and sequence of hooks */
	if (ng_path_parse(fullpath, &nodename, &path, NULL) < 0) {
		TRAP_ERROR();
		return EINVAL;
	}
	if (path == NULL) {
		pbuf[0] = '.';	/* Needs to be writable */
		pbuf[1] = '\0';
		path = pbuf;
	}

	/*
	 * For an absolute address, jump to the starting node.
	 * Note that this holds a reference on the node for us.
	 * Don't forget to drop the reference if we don't need it.
	 */
	if (nodename) {
		node = ng_name2noderef(here, nodename);
		if (node == NULL) {
			TRAP_ERROR();
			return (ENOENT);
		}
	} else {
		if (here == NULL) {
			TRAP_ERROR();
			return (EINVAL);
		}
		node = here;
		NG_NODE_REF(node);
	}

	/*
	 * Now follow the sequence of hooks
	 * XXX
	 * We actually cannot guarantee that the sequence
	 * is not being demolished as we crawl along it
	 * without extra-ordinary locking etc.
	 * So this is a bit dodgy to say the least.
	 * We can probably hold up some things by holding
	 * the nodelist mutex for the time of this
	 * crawl if we wanted.. At least that way we wouldn't have to
	 * worry about the nodes dissappearing, but the hooks would still
	 * be a problem.
	 */
	for (cp = path; node != NULL && *cp != '\0'; ) {
		char *segment;

		/*
		 * Break out the next path segment. Replace the dot we just
		 * found with a NUL; "cp" points to the next segment (or the
		 * NUL at the end).
		 */
		for (segment = cp; *cp != '\0'; cp++) {
			if (*cp == '.') {
				*cp++ = '\0';
				break;
			}
		}

		/* Empty segment */
		if (*segment == '\0')
			continue;

		/* We have a segment, so look for a hook by that name */
		hook = ng_findhook(node, segment);

		/* Can't get there from here... */
		if (hook == NULL
		    || NG_HOOK_PEER(hook) == NULL
		    || NG_HOOK_NOT_VALID(hook)
		    || NG_HOOK_NOT_VALID(NG_HOOK_PEER(hook))) {
			TRAP_ERROR();
			NG_NODE_UNREF(node);
#if 0
			printf("hooknotvalid %s %s %d %d %d %d ",
					path,
					segment,
					hook == NULL,
					NG_HOOK_PEER(hook) == NULL,
					NG_HOOK_NOT_VALID(hook),
					NG_HOOK_NOT_VALID(NG_HOOK_PEER(hook)));
#endif
			return (ENOENT);
		}

		/*
		 * Hop on over to the next node
		 * XXX
		 * Big race conditions here as hooks and nodes go away
		 * *** Idea.. store an ng_ID_t in each hook and use that
		 * instead of the direct hook in this crawl?
		 */
		oldnode = node;
		if ((node = NG_PEER_NODE(hook)))
			NG_NODE_REF(node);	/* XXX RACE */
		NG_NODE_UNREF(oldnode);	/* XXX another race */
		if (NG_NODE_NOT_VALID(node)) {
			NG_NODE_UNREF(node);	/* XXX more races */
			node = NULL;
		}
	}

	/* If node somehow missing, fail here (probably this is not needed) */
	if (node == NULL) {
		TRAP_ERROR();
		return (ENXIO);
	}

	/* Done */
	*destp = node;
	if (lasthook != NULL)
		*lasthook = (hook ? NG_HOOK_PEER(hook) : NULL);
	return (0);
}

/***************************************************************\
* Input queue handling.
* All activities are submitted to the node via the input queue
* which implements a multiple-reader/single-writer gate.
* Items which cannot be handled immeditly are queued.
*
* read-write queue locking inline functions			*
\***************************************************************/

static __inline item_p ng_dequeue(struct ng_queue * ngq, int *rw);
static __inline item_p ng_acquire_read(struct ng_queue * ngq,
					item_p  item);
static __inline item_p ng_acquire_write(struct ng_queue * ngq,
					item_p  item);
static __inline void	ng_leave_read(struct ng_queue * ngq);
static __inline void	ng_leave_write(struct ng_queue * ngq);
static __inline void	ng_queue_rw(struct ng_queue * ngq,
					item_p  item, int rw);

/*
 * Definition of the bits fields in the ng_queue flag word.
 * Defined here rather than in netgraph.h because no-one should fiddle
 * with them.
 *
 * The ordering here may be important! don't shuffle these.
 */
/*-
 Safety Barrier--------+ (adjustable to suit taste) (not used yet)
                       |
                       V
+-------+-------+-------+-------+-------+-------+-------+-------+
  | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
  | |A|c|t|i|v|e| |R|e|a|d|e|r| |C|o|u|n|t| | | | | | | | | |P|A|
  | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |O|W|
+-------+-------+-------+-------+-------+-------+-------+-------+
  \___________________________ ____________________________/ | |
                            V                                | |
                  [active reader count]                      | |
                                                             | |
            Operation Pending -------------------------------+ |
                                                               |
          Active Writer ---------------------------------------+


*/
#define WRITER_ACTIVE	0x00000001
#define OP_PENDING	0x00000002
#define READER_INCREMENT 0x00000004
#define READER_MASK	0xfffffffc	/* Not valid if WRITER_ACTIVE is set */
#define SAFETY_BARRIER	0x00100000	/* 128K items queued should be enough */

/* Defines of more elaborate states on the queue */
/* Mask of bits a new read cares about */
#define NGQ_RMASK	(WRITER_ACTIVE|OP_PENDING)

/* Mask of bits a new write cares about */
#define NGQ_WMASK	(NGQ_RMASK|READER_MASK)

/* Test to decide if there is something on the queue. */
#define QUEUE_ACTIVE(QP) ((QP)->q_flags & OP_PENDING)

/* How to decide what the next queued item is. */
#define HEAD_IS_READER(QP)  NGI_QUEUED_READER((QP)->queue)
#define HEAD_IS_WRITER(QP)  NGI_QUEUED_WRITER((QP)->queue) /* notused */

/* Read the status to decide if the next item on the queue can now run. */
#define QUEUED_READER_CAN_PROCEED(QP)			\
		(((QP)->q_flags & (NGQ_RMASK & ~OP_PENDING)) == 0)
#define QUEUED_WRITER_CAN_PROCEED(QP)			\
		(((QP)->q_flags & (NGQ_WMASK & ~OP_PENDING)) == 0)

/* Is there a chance of getting ANY work off the queue? */
#define NEXT_QUEUED_ITEM_CAN_PROCEED(QP)				\
	(QUEUE_ACTIVE(QP) && 						\
	((HEAD_IS_READER(QP)) ? QUEUED_READER_CAN_PROCEED(QP) :		\
				QUEUED_WRITER_CAN_PROCEED(QP)))


#define NGQRW_R 0
#define NGQRW_W 1

/*
 * Taking into account the current state of the queue and node, possibly take
 * the next entry off the queue and return it. Return NULL if there was
 * nothing we could return, either because there really was nothing there, or
 * because the node was in a state where it cannot yet process the next item
 * on the queue.
 *
 * This MUST MUST MUST be called with the mutex held.
 */
static __inline item_p
ng_dequeue(struct ng_queue *ngq, int *rw)
{
	item_p item;
	u_int		add_arg;

	mtx_assert(&ngq->q_mtx, MA_OWNED);
	/*
	 * If there is nothing queued, then just return.
	 * No point in continuing.
	 * XXXGL: assert this?
	 */
	if (!QUEUE_ACTIVE(ngq)) {
		CTR4(KTR_NET, "%20s: node [%x] (%p) queue empty; "
		    "queue flags 0x%lx", __func__,
		    ngq->q_node->nd_ID, ngq->q_node, ngq->q_flags);
		return (NULL);
	}

	/*
	 * From here, we can assume there is a head item.
	 * We need to find out what it is and if it can be dequeued, given
	 * the current state of the node.
	 */
	if (HEAD_IS_READER(ngq)) {
		if (!QUEUED_READER_CAN_PROCEED(ngq)) {
			/*
			 * It's a reader but we can't use it.
			 * We are stalled so make sure we don't
			 * get called again until something changes.
			 */
			ng_worklist_remove(ngq->q_node);
			CTR4(KTR_NET, "%20s: node [%x] (%p) queued reader "
			    "can't proceed; queue flags 0x%lx", __func__,
			    ngq->q_node->nd_ID, ngq->q_node, ngq->q_flags);
			return (NULL);
		}
		/*
		 * Head of queue is a reader and we have no write active.
		 * We don't care how many readers are already active.
		 * Add the correct increment for the reader count.
		 */
		add_arg = READER_INCREMENT;
		*rw = NGQRW_R;
	} else if (QUEUED_WRITER_CAN_PROCEED(ngq)) {
		/*
		 * There is a pending write, no readers and no active writer.
		 * This means we can go ahead with the pending writer. Note
		 * the fact that we now have a writer, ready for when we take
		 * it off the queue.
		 *
		 * We don't need to worry about a possible collision with the
		 * fasttrack reader.
		 *
		 * The fasttrack thread may take a long time to discover that we
		 * are running so we would have an inconsistent state in the
		 * flags for a while. Since we ignore the reader count
		 * entirely when the WRITER_ACTIVE flag is set, this should
		 * not matter (in fact it is defined that way). If it tests
		 * the flag before this operation, the OP_PENDING flag
		 * will make it fail, and if it tests it later, the
		 * WRITER_ACTIVE flag will do the same. If it is SO slow that
		 * we have actually completed the operation, and neither flag
		 * is set by the time that it tests the flags, then it is
		 * actually ok for it to continue. If it completes and we've
		 * finished and the read pending is set it still fails.
		 *
		 * So we can just ignore it,  as long as we can ensure that the
		 * transition from WRITE_PENDING state to the WRITER_ACTIVE
		 * state is atomic.
		 *
		 * After failing, first it will be held back by the mutex, then
		 * when it can proceed, it will queue its request, then it
		 * would arrive at this function. Usually it will have to
		 * leave empty handed because the ACTIVE WRITER bit will be
		 * set.
		 *
		 * Adjust the flags for the new active writer.
		 */
		add_arg = WRITER_ACTIVE;
		*rw = NGQRW_W;
		/*
		 * We want to write "active writer, no readers " Now go make
		 * it true. In fact there may be a number in the readers
		 * count but we know it is not true and will be fixed soon.
		 * We will fix the flags for the next pending entry in a
		 * moment.
		 */
	} else {
		/*
		 * We can't dequeue anything.. return and say so. Probably we
		 * have a write pending and the readers count is non zero. If
		 * we got here because a reader hit us just at the wrong
		 * moment with the fasttrack code, and put us in a strange
		 * state, then it will be coming through in just a moment,
		 * (just as soon as we release the mutex) and keep things
		 * moving.
		 * Make sure we remove ourselves from the work queue. It
		 * would be a waste of effort to do all this again.
		 */
		ng_worklist_remove(ngq->q_node);
		CTR4(KTR_NET, "%20s: node [%x] (%p) can't dequeue anything; "
		    "queue flags 0x%lx", __func__,
		    ngq->q_node->nd_ID, ngq->q_node, ngq->q_flags);
		return (NULL);
	}

	/*
	 * Now we dequeue the request (whatever it may be) and correct the
	 * pending flags and the next and last pointers.
	 */
	item = ngq->queue;
	ngq->queue = item->el_next;
	CTR6(KTR_NET, "%20s: node [%x] (%p) dequeued item %p with flags 0x%lx; "
	    "queue flags 0x%lx", __func__,
	    ngq->q_node->nd_ID,ngq->q_node, item, item->el_flags, ngq->q_flags);
	if (ngq->last == &(item->el_next)) {
		/*
		 * that was the last entry in the queue so set the 'last
		 * pointer up correctly and make sure the pending flag is
		 * clear.
		 */
		add_arg += -OP_PENDING;
		ngq->last = &(ngq->queue);
		/*
		 * Whatever flag was set will be cleared and
		 * the new acive field will be set by the add as well,
		 * so we don't need to change add_arg.
		 * But we know we don't need to be on the work list.
		 */
		atomic_add_long(&ngq->q_flags, add_arg);
		ng_worklist_remove(ngq->q_node);
	} else {
		/*
		 * Since there is still something on the queue
		 * we don't need to change the PENDING flag.
		 */
		atomic_add_long(&ngq->q_flags, add_arg);
		/*
		 * If we see more doable work, make sure we are
		 * on the work queue.
		 */
		if (NEXT_QUEUED_ITEM_CAN_PROCEED(ngq)) {
			ng_setisr(ngq->q_node);
		}
	}
	CTR6(KTR_NET, "%20s: node [%x] (%p) returning item %p as %s; "
	    "queue flags 0x%lx", __func__,
	    ngq->q_node->nd_ID, ngq->q_node, item, *rw ? "WRITER" : "READER" ,
	    ngq->q_flags);
	return (item);
}

/*
 * Queue a packet to be picked up by someone else.
 * We really don't care who, but we can't or don't want to hang around
 * to process it ourselves. We are probably an interrupt routine..
 * If the queue could be run, flag the netisr handler to start.
 */
static __inline void
ng_queue_rw(struct ng_queue * ngq, item_p  item, int rw)
{
	mtx_assert(&ngq->q_mtx, MA_OWNED);

	if (rw == NGQRW_W)
		NGI_SET_WRITER(item);
	else
		NGI_SET_READER(item);
	item->el_next = NULL;	/* maybe not needed */
	*ngq->last = item;
	CTR5(KTR_NET, "%20s: node [%x] (%p) queued item %p as %s", __func__,
	    ngq->q_node->nd_ID, ngq->q_node, item, rw ? "WRITER" : "READER" );
	/*
	 * If it was the first item in the queue then we need to
	 * set the last pointer and the type flags.
	 */
	if (ngq->last == &(ngq->queue)) {
		atomic_add_long(&ngq->q_flags, OP_PENDING);
		CTR3(KTR_NET, "%20s: node [%x] (%p) set OP_PENDING", __func__,
		    ngq->q_node->nd_ID, ngq->q_node);
	}

	ngq->last = &(item->el_next);
	/*
	 * We can take the worklist lock with the node locked
	 * BUT NOT THE REVERSE!
	 */
	if (NEXT_QUEUED_ITEM_CAN_PROCEED(ngq))
		ng_setisr(ngq->q_node);
}


/*
 * This function 'cheats' in that it first tries to 'grab' the use of the
 * node, without going through the mutex. We can do this becasue of the
 * semantics of the lock. The semantics include a clause that says that the
 * value of the readers count is invalid if the WRITER_ACTIVE flag is set. It
 * also says that the WRITER_ACTIVE flag cannot be set if the readers count
 * is not zero. Note that this talks about what is valid to SET the
 * WRITER_ACTIVE flag, because from the moment it is set, the value if the
 * reader count is immaterial, and not valid. The two 'pending' flags have a
 * similar effect, in that If they are orthogonal to the two active fields in
 * how they are set, but if either is set, the attempted 'grab' need to be
 * backed out because there is earlier work, and we maintain ordering in the
 * queue. The result of this is that the reader request can try obtain use of
 * the node with only a single atomic addition, and without any of the mutex
 * overhead. If this fails the operation degenerates to the same as for other
 * cases.
 *
 */
static __inline item_p
ng_acquire_read(struct ng_queue *ngq, item_p item)
{
	KASSERT(ngq != &ng_deadnode.nd_input_queue,
	    ("%s: working on deadnode", __func__));

	/* ######### Hack alert ######### */
	atomic_add_long(&ngq->q_flags, READER_INCREMENT);
	if ((ngq->q_flags & NGQ_RMASK) == 0) {
		/* Successfully grabbed node */
		CTR4(KTR_NET, "%20s: node [%x] (%p) fast acquired item %p",
		    __func__, ngq->q_node->nd_ID, ngq->q_node, item);
		return (item);
	}
	/* undo the damage if we didn't succeed */
	atomic_subtract_long(&ngq->q_flags, READER_INCREMENT);

	/* ######### End Hack alert ######### */
	mtx_lock_spin((&ngq->q_mtx));
	/*
	 * Try again. Another processor (or interrupt for that matter) may
	 * have removed the last queued item that was stopping us from
	 * running, between the previous test, and the moment that we took
	 * the mutex. (Or maybe a writer completed.)
	 * Even if another fast-track reader hits during this period
	 * we don't care as multiple readers is OK.
	 */
	if ((ngq->q_flags & NGQ_RMASK) == 0) {
		atomic_add_long(&ngq->q_flags, READER_INCREMENT);
		mtx_unlock_spin((&ngq->q_mtx));
		CTR4(KTR_NET, "%20s: node [%x] (%p) slow acquired item %p",
		    __func__, ngq->q_node->nd_ID, ngq->q_node, item);
		return (item);
	}

	/*
	 * and queue the request for later.
	 */
	ng_queue_rw(ngq, item, NGQRW_R);
	mtx_unlock_spin(&(ngq->q_mtx));

	return (NULL);
}

static __inline item_p
ng_acquire_write(struct ng_queue *ngq, item_p item)
{
	KASSERT(ngq != &ng_deadnode.nd_input_queue,
	    ("%s: working on deadnode", __func__));

restart:
	mtx_lock_spin(&(ngq->q_mtx));
	/*
	 * If there are no readers, no writer, and no pending packets, then
	 * we can just go ahead. In all other situations we need to queue the
	 * request
	 */
	if ((ngq->q_flags & NGQ_WMASK) == 0) {
		/* collision could happen *HERE* */
		atomic_add_long(&ngq->q_flags, WRITER_ACTIVE);
		mtx_unlock_spin((&ngq->q_mtx));
		if (ngq->q_flags & READER_MASK) {
			/* Collision with fast-track reader */
			atomic_subtract_long(&ngq->q_flags, WRITER_ACTIVE);
			goto restart;
		}
		CTR4(KTR_NET, "%20s: node [%x] (%p) acquired item %p",
		    __func__, ngq->q_node->nd_ID, ngq->q_node, item);
		return (item);
	}

	/*
	 * and queue the request for later.
	 */
	ng_queue_rw(ngq, item, NGQRW_W);
	mtx_unlock_spin(&(ngq->q_mtx));

	return (NULL);
}

static __inline void
ng_leave_read(struct ng_queue *ngq)
{
	atomic_subtract_long(&ngq->q_flags, READER_INCREMENT);
}

static __inline void
ng_leave_write(struct ng_queue *ngq)
{
	atomic_subtract_long(&ngq->q_flags, WRITER_ACTIVE);
}

static void
ng_flush_input_queue(struct ng_queue * ngq)
{
	item_p item;

	mtx_lock_spin(&ngq->q_mtx);
	while (ngq->queue) {
		item = ngq->queue;
		ngq->queue = item->el_next;
		if (ngq->last == &(item->el_next)) {
			ngq->last = &(ngq->queue);
			atomic_add_long(&ngq->q_flags, -OP_PENDING);
		}
		mtx_unlock_spin(&ngq->q_mtx);
		/* If the item is supplying a callback, call it with an error */
		if (item->apply != NULL) {
			(item->apply)(item->context, ENOENT);
			item->apply = NULL;
		}
		NG_FREE_ITEM(item);
		mtx_lock_spin(&ngq->q_mtx);
	}
	/*
	 * Take us off the work queue if we are there.
	 * We definatly have no work to be done.
	 */
	ng_worklist_remove(ngq->q_node);
	mtx_unlock_spin(&ngq->q_mtx);
}

/***********************************************************************
* Externally visible method for sending or queueing messages or data.
***********************************************************************/

/*
 * The module code should have filled out the item correctly by this stage:
 * Common:
 *    reference to destination node.
 *    Reference to destination rcv hook if relevant.
 * Data:
 *    pointer to mbuf
 * Control_Message:
 *    pointer to msg.
 *    ID of original sender node. (return address)
 * Function:
 *    Function pointer
 *    void * argument
 *    integer argument
 *
 * The nodes have several routines and macros to help with this task:
 */

int
ng_snd_item(item_p item, int flags)
{
	hook_p hook = NGI_HOOK(item);
	node_p node = NGI_NODE(item);
	int queue, rw;
	struct ng_queue * ngq = &node->nd_input_queue;
	int error = 0;

#ifdef	NETGRAPH_DEBUG
	_ngi_check(item, __FILE__, __LINE__);
#endif

	queue = (flags & NG_QUEUE) ? 1 : 0;

	if (item == NULL) {
		TRAP_ERROR();
		return (EINVAL);	/* failed to get queue element */
	}
	if (node == NULL) {
		NG_FREE_ITEM(item);
		TRAP_ERROR();
		return (EINVAL);	/* No address */
	}
	switch(item->el_flags & NGQF_TYPE) {
	case NGQF_DATA:
		/*
		 * DATA MESSAGE
		 * Delivered to a node via a non-optional hook.
		 * Both should be present in the item even though
		 * the node is derivable from the hook.
		 * References are held on both by the item.
		 */

		/* Protect nodes from sending NULL pointers
		 * to each other
		 */
		if (NGI_M(item) == NULL)
			return (EINVAL);

		CHECK_DATA_MBUF(NGI_M(item));
		if (hook == NULL) {
			NG_FREE_ITEM(item);
			TRAP_ERROR();
			return(EINVAL);
		}
		if ((NG_HOOK_NOT_VALID(hook))
		|| (NG_NODE_NOT_VALID(NG_HOOK_NODE(hook)))) {
			NG_FREE_ITEM(item);
			return (ENOTCONN);
		}
		if ((hook->hk_flags & HK_QUEUE)) {
			queue = 1;
		}
		break;
	case NGQF_MESG:
		/*
		 * CONTROL MESSAGE
		 * Delivered to a node.
		 * Hook is optional.
		 * References are held by the item on the node and
		 * the hook if it is present.
		 */
		if (hook && (hook->hk_flags & HK_QUEUE)) {
			queue = 1;
		}
		break;
	case NGQF_FN:
		break;
	default:
		NG_FREE_ITEM(item);
		TRAP_ERROR();
		return (EINVAL);
	}
	switch(item->el_flags & NGQF_RW) {
	case NGQF_READER:
		rw = NGQRW_R;
		break;
	case NGQF_WRITER:
		rw = NGQRW_W;
		break;
	default:
		panic("%s: invalid item flags %lx", __func__, item->el_flags);
	}

	/*
	 * If the node specifies single threading, force writer semantics.
	 * Similarly, the node may say one hook always produces writers.
	 * These are overrides.
	 */
	if ((node->nd_flags & NGF_FORCE_WRITER)
	    || (hook && (hook->hk_flags & HK_FORCE_WRITER)))
			rw = NGQRW_W;

	if (queue) {
		/* Put it on the queue for that node*/
#ifdef	NETGRAPH_DEBUG
		_ngi_check(item, __FILE__, __LINE__);
#endif
		mtx_lock_spin(&(ngq->q_mtx));
		ng_queue_rw(ngq, item, rw);
		mtx_unlock_spin(&(ngq->q_mtx));

		if (flags & NG_PROGRESS)
			return (EINPROGRESS);
		else
			return (0);
	}

	/*
	 * We already decided how we will be queueud or treated.
	 * Try get the appropriate operating permission.
	 */
 	if (rw == NGQRW_R)
		item = ng_acquire_read(ngq, item);
	else
		item = ng_acquire_write(ngq, item);


	if (item == NULL) {
		if (flags & NG_PROGRESS)
			return (EINPROGRESS);
		else
			return (0);
	}

#ifdef	NETGRAPH_DEBUG
	_ngi_check(item, __FILE__, __LINE__);
#endif

	NGI_GET_NODE(item, node); /* zaps stored node */

	error = ng_apply_item(node, item, rw); /* drops r/w lock when done */

	/*
	 * If the node goes away when we remove the reference,
	 * whatever we just did caused it.. whatever we do, DO NOT
	 * access the node again!
	 */
	if (NG_NODE_UNREF(node) == 0) {
		return (error);
	}

	mtx_lock_spin(&(ngq->q_mtx));
	if (NEXT_QUEUED_ITEM_CAN_PROCEED(ngq))
		ng_setisr(ngq->q_node);
	mtx_unlock_spin(&(ngq->q_mtx));

	return (error);
}

/*
 * We have an item that was possibly queued somewhere.
 * It should contain all the information needed
 * to run it on the appropriate node/hook.
 */
static int
ng_apply_item(node_p node, item_p item, int rw)
{
	hook_p  hook;
	int	error = 0;
	ng_rcvdata_t *rcvdata;
	ng_rcvmsg_t *rcvmsg;
	ng_apply_t *apply = NULL;
	void	*context = NULL;

	NGI_GET_HOOK(item, hook); /* clears stored hook */
#ifdef	NETGRAPH_DEBUG
	_ngi_check(item, __FILE__, __LINE__);
#endif

	/*
	 * If the item has an "apply" callback, store it.
	 * Clear item's callback immediately, to avoid an extra call if
	 * the item is reused by the destination node.
	 */
	if (item->apply != NULL) {
		apply = item->apply;
		context = item->context;
		item->apply = NULL;
	}

	switch (item->el_flags & NGQF_TYPE) {
	case NGQF_DATA:
		/*
		 * Check things are still ok as when we were queued.
		 */
		if ((hook == NULL)
		|| NG_HOOK_NOT_VALID(hook)
		|| NG_NODE_NOT_VALID(node) ) {
			error = EIO;
			NG_FREE_ITEM(item);
			break;
		}
		/*
		 * If no receive method, just silently drop it.
		 * Give preference to the hook over-ride method
		 */
		if ((!(rcvdata = hook->hk_rcvdata))
		&& (!(rcvdata = NG_HOOK_NODE(hook)->nd_type->rcvdata))) {
			error = 0;
			NG_FREE_ITEM(item);
			break;
		}
		error = (*rcvdata)(hook, item);
		break;
	case NGQF_MESG:
		if (hook) {
			if (NG_HOOK_NOT_VALID(hook)) {
				/*
				 * The hook has been zapped then we can't
				 * use it. Immediatly drop its reference.
				 * The message may not need it.
				 */
				NG_HOOK_UNREF(hook);
				hook = NULL;
			}
		}
		/*
		 * Similarly, if the node is a zombie there is
		 * nothing we can do with it, drop everything.
		 */
		if (NG_NODE_NOT_VALID(node)) {
			TRAP_ERROR();
			error = EINVAL;
			NG_FREE_ITEM(item);
		} else {
			/*
			 * Call the appropriate message handler for the object.
			 * It is up to the message handler to free the message.
			 * If it's a generic message, handle it generically,
			 * otherwise call the type's message handler
			 * (if it exists)
			 * XXX (race). Remember that a queued message may
			 * reference a node or hook that has just been
			 * invalidated. It will exist as the queue code
			 * is holding a reference, but..
			 */

			struct ng_mesg *msg = NGI_MSG(item);

			/*
			 * check if the generic handler owns it.
			 */
			if ((msg->header.typecookie == NGM_GENERIC_COOKIE)
			&& ((msg->header.flags & NGF_RESP) == 0)) {
				error = ng_generic_msg(node, item, hook);
				break;
			}
			/*
			 * Now see if there is a handler (hook or node specific)
			 * in the target node. If none, silently discard.
			 */
			if (((!hook) || (!(rcvmsg = hook->hk_rcvmsg)))
			&& (!(rcvmsg = node->nd_type->rcvmsg))) {
				TRAP_ERROR();
				error = 0;
				NG_FREE_ITEM(item);
				break;
			}
			error = (*rcvmsg)(node, item, hook);
		}
		break;
	case NGQF_FN:
		/*
		 *  We have to implicitly trust the hook,
		 * as some of these are used for system purposes
		 * where the hook is invalid. In the case of
		 * the shutdown message we allow it to hit
		 * even if the node is invalid.
		 */
		if ((NG_NODE_NOT_VALID(node))
		&& (NGI_FN(item) != &ng_rmnode)) {
			TRAP_ERROR();
			error = EINVAL;
			NG_FREE_ITEM(item);
			break;
		}
		(*NGI_FN(item))(node, hook, NGI_ARG1(item), NGI_ARG2(item));
		NG_FREE_ITEM(item);
		break;
		
	}
	/*
	 * We held references on some of the resources
	 * that we took from the item. Now that we have
	 * finished doing everything, drop those references.
	 */
	if (hook) {
		NG_HOOK_UNREF(hook);
	}

 	if (rw == NGQRW_R) {
		ng_leave_read(&node->nd_input_queue);
	} else {
		ng_leave_write(&node->nd_input_queue);
	}

	/* Apply callback. */
	if (apply != NULL)
		(*apply)(context, error);

	return (error);
}

/***********************************************************************
 * Implement the 'generic' control messages
 ***********************************************************************/
static int
ng_generic_msg(node_p here, item_p item, hook_p lasthook)
{
	int error = 0;
	struct ng_mesg *msg;
	struct ng_mesg *resp = NULL;

	NGI_GET_MSG(item, msg);
	if (msg->header.typecookie != NGM_GENERIC_COOKIE) {
		TRAP_ERROR();
		error = EINVAL;
		goto out;
	}
	switch (msg->header.cmd) {
	case NGM_SHUTDOWN:
		ng_rmnode(here, NULL, NULL, 0);
		break;
	case NGM_MKPEER:
	    {
		struct ngm_mkpeer *const mkp = (struct ngm_mkpeer *) msg->data;

		if (msg->header.arglen != sizeof(*mkp)) {
			TRAP_ERROR();
			error = EINVAL;
			break;
		}
		mkp->type[sizeof(mkp->type) - 1] = '\0';
		mkp->ourhook[sizeof(mkp->ourhook) - 1] = '\0';
		mkp->peerhook[sizeof(mkp->peerhook) - 1] = '\0';
		error = ng_mkpeer(here, mkp->ourhook, mkp->peerhook, mkp->type);
		break;
	    }
	case NGM_CONNECT:
	    {
		struct ngm_connect *const con =
			(struct ngm_connect *) msg->data;
		node_p node2;

		if (msg->header.arglen != sizeof(*con)) {
			TRAP_ERROR();
			error = EINVAL;
			break;
		}
		con->path[sizeof(con->path) - 1] = '\0';
		con->ourhook[sizeof(con->ourhook) - 1] = '\0';
		con->peerhook[sizeof(con->peerhook) - 1] = '\0';
		/* Don't forget we get a reference.. */
		error = ng_path2noderef(here, con->path, &node2, NULL);
		if (error)
			break;
		error = ng_con_nodes(here, con->ourhook, node2, con->peerhook);
		NG_NODE_UNREF(node2);
		break;
	    }
	case NGM_NAME:
	    {
		struct ngm_name *const nam = (struct ngm_name *) msg->data;

		if (msg->header.arglen != sizeof(*nam)) {
			TRAP_ERROR();
			error = EINVAL;
			break;
		}
		nam->name[sizeof(nam->name) - 1] = '\0';
		error = ng_name_node(here, nam->name);
		break;
	    }
	case NGM_RMHOOK:
	    {
		struct ngm_rmhook *const rmh = (struct ngm_rmhook *) msg->data;
		hook_p hook;

		if (msg->header.arglen != sizeof(*rmh)) {
			TRAP_ERROR();
			error = EINVAL;
			break;
		}
		rmh->ourhook[sizeof(rmh->ourhook) - 1] = '\0';
		if ((hook = ng_findhook(here, rmh->ourhook)) != NULL)
			ng_destroy_hook(hook);
		break;
	    }
	case NGM_NODEINFO:
	    {
		struct nodeinfo *ni;

		NG_MKRESPONSE(resp, msg, sizeof(*ni), M_NOWAIT);
		if (resp == NULL) {
			error = ENOMEM;
			break;
		}

		/* Fill in node info */
		ni = (struct nodeinfo *) resp->data;
		if (NG_NODE_HAS_NAME(here))
			strcpy(ni->name, NG_NODE_NAME(here));
		strcpy(ni->type, here->nd_type->name);
		ni->id = ng_node2ID(here);
		ni->hooks = here->nd_numhooks;
		break;
	    }
	case NGM_LISTHOOKS:
	    {
		const int nhooks = here->nd_numhooks;
		struct hooklist *hl;
		struct nodeinfo *ni;
		hook_p hook;

		/* Get response struct */
		NG_MKRESPONSE(resp, msg, sizeof(*hl)
		    + (nhooks * sizeof(struct linkinfo)), M_NOWAIT);
		if (resp == NULL) {
			error = ENOMEM;
			break;
		}
		hl = (struct hooklist *) resp->data;
		ni = &hl->nodeinfo;

		/* Fill in node info */
		if (NG_NODE_HAS_NAME(here))
			strcpy(ni->name, NG_NODE_NAME(here));
		strcpy(ni->type, here->nd_type->name);
		ni->id = ng_node2ID(here);

		/* Cycle through the linked list of hooks */
		ni->hooks = 0;
		LIST_FOREACH(hook, &here->nd_hooks, hk_hooks) {
			struct linkinfo *const link = &hl->link[ni->hooks];

			if (ni->hooks >= nhooks) {
				log(LOG_ERR, "%s: number of %s changed\n",
				    __func__, "hooks");
				break;
			}
			if (NG_HOOK_NOT_VALID(hook))
				continue;
			strcpy(link->ourhook, NG_HOOK_NAME(hook));
			strcpy(link->peerhook, NG_PEER_HOOK_NAME(hook));
			if (NG_PEER_NODE_NAME(hook)[0] != '\0')
				strcpy(link->nodeinfo.name,
				    NG_PEER_NODE_NAME(hook));
			strcpy(link->nodeinfo.type,
			   NG_PEER_NODE(hook)->nd_type->name);
			link->nodeinfo.id = ng_node2ID(NG_PEER_NODE(hook));
			link->nodeinfo.hooks = NG_PEER_NODE(hook)->nd_numhooks;
			ni->hooks++;
		}
		break;
	    }

	case NGM_LISTNAMES:
	case NGM_LISTNODES:
	    {
		const int unnamed = (msg->header.cmd == NGM_LISTNODES);
		struct namelist *nl;
		node_p node;
		int num = 0;

		mtx_lock(&ng_nodelist_mtx);
		/* Count number of nodes */
		LIST_FOREACH(node, &ng_nodelist, nd_nodes) {
			if (NG_NODE_IS_VALID(node)
			&& (unnamed || NG_NODE_HAS_NAME(node))) {
				num++;
			}
		}
		mtx_unlock(&ng_nodelist_mtx);

		/* Get response struct */
		NG_MKRESPONSE(resp, msg, sizeof(*nl)
		    + (num * sizeof(struct nodeinfo)), M_NOWAIT);
		if (resp == NULL) {
			error = ENOMEM;
			break;
		}
		nl = (struct namelist *) resp->data;

		/* Cycle through the linked list of nodes */
		nl->numnames = 0;
		mtx_lock(&ng_nodelist_mtx);
		LIST_FOREACH(node, &ng_nodelist, nd_nodes) {
			struct nodeinfo *const np = &nl->nodeinfo[nl->numnames];

			if (nl->numnames >= num) {
				log(LOG_ERR, "%s: number of %s changed\n",
				    __func__, "nodes");
				break;
			}
			if (NG_NODE_NOT_VALID(node))
				continue;
			if (!unnamed && (! NG_NODE_HAS_NAME(node)))
				continue;
			if (NG_NODE_HAS_NAME(node))
				strcpy(np->name, NG_NODE_NAME(node));
			strcpy(np->type, node->nd_type->name);
			np->id = ng_node2ID(node);
			np->hooks = node->nd_numhooks;
			nl->numnames++;
		}
		mtx_unlock(&ng_nodelist_mtx);
		break;
	    }

	case NGM_LISTTYPES:
	    {
		struct typelist *tl;
		struct ng_type *type;
		int num = 0;

		mtx_lock(&ng_typelist_mtx);
		/* Count number of types */
		LIST_FOREACH(type, &ng_typelist, types) {
			num++;
		}
		mtx_unlock(&ng_typelist_mtx);

		/* Get response struct */
		NG_MKRESPONSE(resp, msg, sizeof(*tl)
		    + (num * sizeof(struct typeinfo)), M_NOWAIT);
		if (resp == NULL) {
			error = ENOMEM;
			break;
		}
		tl = (struct typelist *) resp->data;

		/* Cycle through the linked list of types */
		tl->numtypes = 0;
		mtx_lock(&ng_typelist_mtx);
		LIST_FOREACH(type, &ng_typelist, types) {
			struct typeinfo *const tp = &tl->typeinfo[tl->numtypes];

			if (tl->numtypes >= num) {
				log(LOG_ERR, "%s: number of %s changed\n",
				    __func__, "types");
				break;
			}
			strcpy(tp->type_name, type->name);
			tp->numnodes = type->refs - 1; /* don't count list */
			tl->numtypes++;
		}
		mtx_unlock(&ng_typelist_mtx);
		break;
	    }

	case NGM_BINARY2ASCII:
	    {
		int bufSize = 20 * 1024;	/* XXX hard coded constant */
		const struct ng_parse_type *argstype;
		const struct ng_cmdlist *c;
		struct ng_mesg *binary, *ascii;

		/* Data area must contain a valid netgraph message */
		binary = (struct ng_mesg *)msg->data;
		if (msg->header.arglen < sizeof(struct ng_mesg) ||
		    (msg->header.arglen - sizeof(struct ng_mesg) <
		    binary->header.arglen)) {
			TRAP_ERROR();
			error = EINVAL;
			break;
		}

		/* Get a response message with lots of room */
		NG_MKRESPONSE(resp, msg, sizeof(*ascii) + bufSize, M_NOWAIT);
		if (resp == NULL) {
			error = ENOMEM;
			break;
		}
		ascii = (struct ng_mesg *)resp->data;

		/* Copy binary message header to response message payload */
		bcopy(binary, ascii, sizeof(*binary));

		/* Find command by matching typecookie and command number */
		for (c = here->nd_type->cmdlist;
		    c != NULL && c->name != NULL; c++) {
			if (binary->header.typecookie == c->cookie
			    && binary->header.cmd == c->cmd)
				break;
		}
		if (c == NULL || c->name == NULL) {
			for (c = ng_generic_cmds; c->name != NULL; c++) {
				if (binary->header.typecookie == c->cookie
				    && binary->header.cmd == c->cmd)
					break;
			}
			if (c->name == NULL) {
				NG_FREE_MSG(resp);
				error = ENOSYS;
				break;
			}
		}

		/* Convert command name to ASCII */
		snprintf(ascii->header.cmdstr, sizeof(ascii->header.cmdstr),
		    "%s", c->name);

		/* Convert command arguments to ASCII */
		argstype = (binary->header.flags & NGF_RESP) ?
		    c->respType : c->mesgType;
		if (argstype == NULL) {
			*ascii->data = '\0';
		} else {
			if ((error = ng_unparse(argstype,
			    (u_char *)binary->data,
			    ascii->data, bufSize)) != 0) {
				NG_FREE_MSG(resp);
				break;
			}
		}

		/* Return the result as struct ng_mesg plus ASCII string */
		bufSize = strlen(ascii->data) + 1;
		ascii->header.arglen = bufSize;
		resp->header.arglen = sizeof(*ascii) + bufSize;
		break;
	    }

	case NGM_ASCII2BINARY:
	    {
		int bufSize = 2000;	/* XXX hard coded constant */
		const struct ng_cmdlist *c;
		const struct ng_parse_type *argstype;
		struct ng_mesg *ascii, *binary;
		int off = 0;

		/* Data area must contain at least a struct ng_mesg + '\0' */
		ascii = (struct ng_mesg *)msg->data;
		if ((msg->header.arglen < sizeof(*ascii) + 1) ||
		    (ascii->header.arglen < 1) ||
		    (msg->header.arglen < sizeof(*ascii) +
		    ascii->header.arglen)) {
			TRAP_ERROR();
			error = EINVAL;
			break;
		}
		ascii->data[ascii->header.arglen - 1] = '\0';

		/* Get a response message with lots of room */
		NG_MKRESPONSE(resp, msg, sizeof(*binary) + bufSize, M_NOWAIT);
		if (resp == NULL) {
			error = ENOMEM;
			break;
		}
		binary = (struct ng_mesg *)resp->data;

		/* Copy ASCII message header to response message payload */
		bcopy(ascii, binary, sizeof(*ascii));

		/* Find command by matching ASCII command string */
		for (c = here->nd_type->cmdlist;
		    c != NULL && c->name != NULL; c++) {
			if (strcmp(ascii->header.cmdstr, c->name) == 0)
				break;
		}
		if (c == NULL || c->name == NULL) {
			for (c = ng_generic_cmds; c->name != NULL; c++) {
				if (strcmp(ascii->header.cmdstr, c->name) == 0)
					break;
			}
			if (c->name == NULL) {
				NG_FREE_MSG(resp);
				error = ENOSYS;
				break;
			}
		}

		/* Convert command name to binary */
		binary->header.cmd = c->cmd;
		binary->header.typecookie = c->cookie;

		/* Convert command arguments to binary */
		argstype = (binary->header.flags & NGF_RESP) ?
		    c->respType : c->mesgType;
		if (argstype == NULL) {
			bufSize = 0;
		} else {
			if ((error = ng_parse(argstype, ascii->data,
			    &off, (u_char *)binary->data, &bufSize)) != 0) {
				NG_FREE_MSG(resp);
				break;
			}
		}

		/* Return the result */
		binary->header.arglen = bufSize;
		resp->header.arglen = sizeof(*binary) + bufSize;
		break;
	    }

	case NGM_TEXT_CONFIG:
	case NGM_TEXT_STATUS:
		/*
		 * This one is tricky as it passes the command down to the
		 * actual node, even though it is a generic type command.
		 * This means we must assume that the item/msg is already freed
		 * when control passes back to us.
		 */
		if (here->nd_type->rcvmsg != NULL) {
			NGI_MSG(item) = msg; /* put it back as we found it */
			return((*here->nd_type->rcvmsg)(here, item, lasthook));
		}
		/* Fall through if rcvmsg not supported */
	default:
		TRAP_ERROR();
		error = EINVAL;
	}
	/*
	 * Sometimes a generic message may be statically allocated
	 * to avoid problems with allocating when in tight memeory situations.
	 * Don't free it if it is so.
	 * I break them appart here, because erros may cause a free if the item
	 * in which case we'd be doing it twice.
	 * they are kept together above, to simplify freeing.
	 */
out:
	NG_RESPOND_MSG(error, here, item, resp);
	if (msg)
		NG_FREE_MSG(msg);
	return (error);
}

/************************************************************************
			Queue element get/free routines
************************************************************************/

uma_zone_t			ng_qzone;
static int			maxalloc = 512;	/* limit the damage of a leak */

TUNABLE_INT("net.graph.maxalloc", &maxalloc);
SYSCTL_INT(_net_graph, OID_AUTO, maxalloc, CTLFLAG_RDTUN, &maxalloc,
    0, "Maximum number of queue items to allocate");

#ifdef	NETGRAPH_DEBUG
static TAILQ_HEAD(, ng_item) ng_itemlist = TAILQ_HEAD_INITIALIZER(ng_itemlist);
static int			allocated;	/* number of items malloc'd */
#endif

/*
 * Get a queue entry.
 * This is usually called when a packet first enters netgraph.
 * By definition, this is usually from an interrupt, or from a user.
 * Users are not so important, but try be quick for the times that it's
 * an interrupt.
 */
static __inline item_p
ng_getqblk(int flags)
{
	item_p item = NULL;
	int wait;

	wait = (flags & NG_WAITOK) ? M_WAITOK : M_NOWAIT;

	item = uma_zalloc(ng_qzone, wait | M_ZERO);

#ifdef	NETGRAPH_DEBUG
	if (item) {
			mtx_lock(&ngq_mtx);
			TAILQ_INSERT_TAIL(&ng_itemlist, item, all);
			allocated++;
			mtx_unlock(&ngq_mtx);
	}
#endif

	return (item);
}

/*
 * Release a queue entry
 */
void
ng_free_item(item_p item)
{
	KASSERT(item->apply == NULL, ("%s: leaking apply callback", __func__));

	/*
	 * The item may hold resources on it's own. We need to free
	 * these before we can free the item. What they are depends upon
	 * what kind of item it is. it is important that nodes zero
	 * out pointers to resources that they remove from the item
	 * or we release them again here.
	 */
	switch (item->el_flags & NGQF_TYPE) {
	case NGQF_DATA:
		/* If we have an mbuf still attached.. */
		NG_FREE_M(_NGI_M(item));
		break;
	case NGQF_MESG:
		_NGI_RETADDR(item) = 0;
		NG_FREE_MSG(_NGI_MSG(item));
		break;
	case NGQF_FN:
		/* nothing to free really, */
		_NGI_FN(item) = NULL;
		_NGI_ARG1(item) = NULL;
		_NGI_ARG2(item) = 0;
	case NGQF_UNDEF:
		break;
	}
	/* If we still have a node or hook referenced... */
	_NGI_CLR_NODE(item);
	_NGI_CLR_HOOK(item);

#ifdef	NETGRAPH_DEBUG
	mtx_lock(&ngq_mtx);
	TAILQ_REMOVE(&ng_itemlist, item, all);
	allocated--;
	mtx_unlock(&ngq_mtx);
#endif
	uma_zfree(ng_qzone, item);
}

/************************************************************************
			Module routines
************************************************************************/

/*
 * Handle the loading/unloading of a netgraph node type module
 */
int
ng_mod_event(module_t mod, int event, void *data)
{
	struct ng_type *const type = data;
	int s, error = 0;

	switch (event) {
	case MOD_LOAD:

		/* Register new netgraph node type */
		s = splnet();
		if ((error = ng_newtype(type)) != 0) {
			splx(s);
			break;
		}

		/* Call type specific code */
		if (type->mod_event != NULL)
			if ((error = (*type->mod_event)(mod, event, data))) {
				mtx_lock(&ng_typelist_mtx);
				type->refs--;	/* undo it */
				LIST_REMOVE(type, types);
				mtx_unlock(&ng_typelist_mtx);
			}
		splx(s);
		break;

	case MOD_UNLOAD:
		s = splnet();
		if (type->refs > 1) {		/* make sure no nodes exist! */
			error = EBUSY;
		} else {
			if (type->refs == 0) {
				/* failed load, nothing to undo */
				splx(s);
				break;
			}
			if (type->mod_event != NULL) {	/* check with type */
				error = (*type->mod_event)(mod, event, data);
				if (error != 0) {	/* type refuses.. */
					splx(s);
					break;
				}
			}
			mtx_lock(&ng_typelist_mtx);
			LIST_REMOVE(type, types);
			mtx_unlock(&ng_typelist_mtx);
		}
		splx(s);
		break;

	default:
		if (type->mod_event != NULL)
			error = (*type->mod_event)(mod, event, data);
		else
			error = EOPNOTSUPP;		/* XXX ? */
		break;
	}
	return (error);
}

/*
 * Handle loading and unloading for this code.
 * The only thing we need to link into is the NETISR strucure.
 */
static int
ngb_mod_event(module_t mod, int event, void *data)
{
	int error = 0;

	switch (event) {
	case MOD_LOAD:
		/* Initialize everything. */
		mtx_init(&ng_worklist_mtx, "ng_worklist", NULL, MTX_SPIN);
		mtx_init(&ng_typelist_mtx, "netgraph types mutex", NULL,
		    MTX_DEF);
		mtx_init(&ng_nodelist_mtx, "netgraph nodelist mutex", NULL,
		    MTX_DEF);
		mtx_init(&ng_idhash_mtx, "netgraph idhash mutex", NULL,
		    MTX_DEF);
		mtx_init(&ng_topo_mtx, "netgraph topology mutex", NULL,
		    MTX_DEF);
#ifdef	NETGRAPH_DEBUG
		mtx_init(&ngq_mtx, "netgraph item list mutex", NULL,
		    MTX_DEF);
#endif
		ng_qzone = uma_zcreate("NetGraph items", sizeof(struct ng_item),
		    NULL, NULL, NULL, NULL, UMA_ALIGN_CACHE, 0);
		uma_zone_set_max(ng_qzone, maxalloc);
		netisr_register(NETISR_NETGRAPH, (netisr_t *)ngintr, NULL,
		    NETISR_MPSAFE);
		break;
	case MOD_UNLOAD:
		/* You cant unload it because an interface may be using it.  */
		error = EBUSY;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

static moduledata_t netgraph_mod = {
	"netgraph",
	ngb_mod_event,
	(NULL)
};
DECLARE_MODULE(netgraph, netgraph_mod, SI_SUB_NETGRAPH, SI_ORDER_MIDDLE);
SYSCTL_NODE(_net, OID_AUTO, graph, CTLFLAG_RW, 0, "netgraph Family");
SYSCTL_INT(_net_graph, OID_AUTO, abi_version, CTLFLAG_RD, 0, NG_ABI_VERSION,"");
SYSCTL_INT(_net_graph, OID_AUTO, msg_version, CTLFLAG_RD, 0, NG_VERSION, "");

#ifdef	NETGRAPH_DEBUG
void
dumphook (hook_p hook, char *file, int line)
{
	printf("hook: name %s, %d refs, Last touched:\n",
		_NG_HOOK_NAME(hook), hook->hk_refs);
	printf("	Last active @ %s, line %d\n",
		hook->lastfile, hook->lastline);
	if (line) {
		printf(" problem discovered at file %s, line %d\n", file, line);
	}
}

void
dumpnode(node_p node, char *file, int line)
{
	printf("node: ID [%x]: type '%s', %d hooks, flags 0x%x, %d refs, %s:\n",
		_NG_NODE_ID(node), node->nd_type->name,
		node->nd_numhooks, node->nd_flags,
		node->nd_refs, node->nd_name);
	printf("	Last active @ %s, line %d\n",
		node->lastfile, node->lastline);
	if (line) {
		printf(" problem discovered at file %s, line %d\n", file, line);
	}
}

void
dumpitem(item_p item, char *file, int line)
{
	printf(" ACTIVE item, last used at %s, line %d",
		item->lastfile, item->lastline);
	switch(item->el_flags & NGQF_TYPE) {
	case NGQF_DATA:
		printf(" - [data]\n");
		break;
	case NGQF_MESG:
		printf(" - retaddr[%d]:\n", _NGI_RETADDR(item));
		break;
	case NGQF_FN:
		printf(" - fn@%p (%p, %p, %p, %d (%x))\n",
			item->body.fn.fn_fn,
			_NGI_NODE(item),
			_NGI_HOOK(item),
			item->body.fn.fn_arg1,
			item->body.fn.fn_arg2,
			item->body.fn.fn_arg2);
		break;
	case NGQF_UNDEF:
		printf(" - UNDEFINED!\n");
	}
	if (line) {
		printf(" problem discovered at file %s, line %d\n", file, line);
		if (_NGI_NODE(item)) {
			printf("node %p ([%x])\n",
				_NGI_NODE(item), ng_node2ID(_NGI_NODE(item)));
		}
	}
}

static void
ng_dumpitems(void)
{
	item_p item;
	int i = 1;
	TAILQ_FOREACH(item, &ng_itemlist, all) {
		printf("[%d] ", i++);
		dumpitem(item, NULL, 0);
	}
}

static void
ng_dumpnodes(void)
{
	node_p node;
	int i = 1;
	mtx_lock(&ng_nodelist_mtx);
	SLIST_FOREACH(node, &ng_allnodes, nd_all) {
		printf("[%d] ", i++);
		dumpnode(node, NULL, 0);
	}
	mtx_unlock(&ng_nodelist_mtx);
}

static void
ng_dumphooks(void)
{
	hook_p hook;
	int i = 1;
	mtx_lock(&ng_nodelist_mtx);
	SLIST_FOREACH(hook, &ng_allhooks, hk_all) {
		printf("[%d] ", i++);
		dumphook(hook, NULL, 0);
	}
	mtx_unlock(&ng_nodelist_mtx);
}

static int
sysctl_debug_ng_dump_items(SYSCTL_HANDLER_ARGS)
{
	int error;
	int val;
	int i;

	val = allocated;
	i = 1;
	error = sysctl_handle_int(oidp, &val, sizeof(int), req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val == 42) {
		ng_dumpitems();
		ng_dumpnodes();
		ng_dumphooks();
	}
	return (0);
}

SYSCTL_PROC(_debug, OID_AUTO, ng_dump_items, CTLTYPE_INT | CTLFLAG_RW,
    0, sizeof(int), sysctl_debug_ng_dump_items, "I", "Number of allocated items");
#endif	/* NETGRAPH_DEBUG */


/***********************************************************************
* Worklist routines
**********************************************************************/
/* NETISR thread enters here */
/*
 * Pick a node off the list of nodes with work,
 * try get an item to process off it.
 * If there are no more, remove the node from the list.
 */
static void
ngintr(void)
{
	item_p item;
	node_p  node = NULL;

	for (;;) {
		mtx_lock_spin(&ng_worklist_mtx);
		node = TAILQ_FIRST(&ng_worklist);
		if (!node) {
			mtx_unlock_spin(&ng_worklist_mtx);
			break;
		}
		node->nd_flags &= ~NGF_WORKQ;	
		TAILQ_REMOVE(&ng_worklist, node, nd_work);
		mtx_unlock_spin(&ng_worklist_mtx);
		CTR3(KTR_NET, "%20s: node [%x] (%p) taken off worklist",
		    __func__, node->nd_ID, node);
		/*
		 * We have the node. We also take over the reference
		 * that the list had on it.
		 * Now process as much as you can, until it won't
		 * let you have another item off the queue.
		 * All this time, keep the reference
		 * that lets us be sure that the node still exists.
		 * Let the reference go at the last minute.
		 * ng_dequeue will put us back on the worklist
		 * if there is more too do. This may be of use if there
		 * are Multiple Processors and multiple Net threads in the
		 * future.
		 */
		for (;;) {
			int rw;

			mtx_lock_spin(&node->nd_input_queue.q_mtx);
			item = ng_dequeue(&node->nd_input_queue, &rw);
			if (item == NULL) {
				mtx_unlock_spin(&node->nd_input_queue.q_mtx);
				break; /* go look for another node */
			} else {
				mtx_unlock_spin(&node->nd_input_queue.q_mtx);
				NGI_GET_NODE(item, node); /* zaps stored node */
				ng_apply_item(node, item, rw);
				NG_NODE_UNREF(node);
			}
		}
		NG_NODE_UNREF(node);
	}
}

static void
ng_worklist_remove(node_p node)
{
	mtx_assert(&node->nd_input_queue.q_mtx, MA_OWNED);

	mtx_lock_spin(&ng_worklist_mtx);
	if (node->nd_flags & NGF_WORKQ) {
		node->nd_flags &= ~NGF_WORKQ;
		TAILQ_REMOVE(&ng_worklist, node, nd_work);
		mtx_unlock_spin(&ng_worklist_mtx);
		NG_NODE_UNREF(node);
		CTR3(KTR_NET, "%20s: node [%x] (%p) removed from worklist",
		    __func__, node->nd_ID, node);
	} else {
		mtx_unlock_spin(&ng_worklist_mtx);
	}
}

/*
 * XXX
 * It's posible that a debugging NG_NODE_REF may need
 * to be outside the mutex zone
 */
static void
ng_setisr(node_p node)
{

	mtx_assert(&node->nd_input_queue.q_mtx, MA_OWNED);

	if ((node->nd_flags & NGF_WORKQ) == 0) {
		/*
		 * If we are not already on the work queue,
		 * then put us on.
		 */
		node->nd_flags |= NGF_WORKQ;
		mtx_lock_spin(&ng_worklist_mtx);
		TAILQ_INSERT_TAIL(&ng_worklist, node, nd_work);
		mtx_unlock_spin(&ng_worklist_mtx);
		NG_NODE_REF(node); /* XXX fafe in mutex? */
		CTR3(KTR_NET, "%20s: node [%x] (%p) put on worklist", __func__,
		    node->nd_ID, node);
	} else
		CTR3(KTR_NET, "%20s: node [%x] (%p) already on worklist",
		    __func__, node->nd_ID, node);
	schednetisr(NETISR_NETGRAPH);
}


/***********************************************************************
* Externally useable functions to set up a queue item ready for sending
***********************************************************************/

#ifdef	NETGRAPH_DEBUG
#define	ITEM_DEBUG_CHECKS						\
	do {								\
		if (NGI_NODE(item) ) {					\
			printf("item already has node");		\
			kdb_enter("has node");				\
			NGI_CLR_NODE(item);				\
		}							\
		if (NGI_HOOK(item) ) {					\
			printf("item already has hook");		\
			kdb_enter("has hook");				\
			NGI_CLR_HOOK(item);				\
		}							\
	} while (0)
#else
#define ITEM_DEBUG_CHECKS
#endif

/*
 * Put mbuf into the item.
 * Hook and node references will be removed when the item is dequeued.
 * (or equivalent)
 * (XXX) Unsafe because no reference held by peer on remote node.
 * remote node might go away in this timescale.
 * We know the hooks can't go away because that would require getting
 * a writer item on both nodes and we must have at least a  reader
 * here to be able to do this.
 * Note that the hook loaded is the REMOTE hook.
 *
 * This is possibly in the critical path for new data.
 */
item_p
ng_package_data(struct mbuf *m, int flags)
{
	item_p item;

	if ((item = ng_getqblk(flags)) == NULL) {
		NG_FREE_M(m);
		return (NULL);
	}
	ITEM_DEBUG_CHECKS;
	item->el_flags = NGQF_DATA | NGQF_READER;
	item->el_next = NULL;
	NGI_M(item) = m;
	return (item);
}

/*
 * Allocate a queue item and put items into it..
 * Evaluate the address as this will be needed to queue it and
 * to work out what some of the fields should be.
 * Hook and node references will be removed when the item is dequeued.
 * (or equivalent)
 */
item_p
ng_package_msg(struct ng_mesg *msg, int flags)
{
	item_p item;

	if ((item = ng_getqblk(flags)) == NULL) {
		NG_FREE_MSG(msg);
		return (NULL);
	}
	ITEM_DEBUG_CHECKS;
	/* Messages items count as writers unless explicitly exempted. */
	if (msg->header.cmd & NGM_READONLY)
		item->el_flags = NGQF_MESG | NGQF_READER;
	else
		item->el_flags = NGQF_MESG | NGQF_WRITER;
	item->el_next = NULL;
	/*
	 * Set the current lasthook into the queue item
	 */
	NGI_MSG(item) = msg;
	NGI_RETADDR(item) = 0;
	return (item);
}



#define SET_RETADDR(item, here, retaddr)				\
	do {	/* Data or fn items don't have retaddrs */		\
		if ((item->el_flags & NGQF_TYPE) == NGQF_MESG) {	\
			if (retaddr) {					\
				NGI_RETADDR(item) = retaddr;		\
			} else {					\
				/*					\
				 * The old return address should be ok.	\
				 * If there isn't one, use the address	\
				 * here.				\
				 */					\
				if (NGI_RETADDR(item) == 0) {		\
					NGI_RETADDR(item)		\
						= ng_node2ID(here);	\
				}					\
			}						\
		}							\
	} while (0)

int
ng_address_hook(node_p here, item_p item, hook_p hook, ng_ID_t retaddr)
{
	hook_p peer;
	node_p peernode;
	ITEM_DEBUG_CHECKS;
	/*
	 * Quick sanity check..
	 * Since a hook holds a reference on it's node, once we know
	 * that the peer is still connected (even if invalid,) we know
	 * that the peer node is present, though maybe invalid.
	 */
	if ((hook == NULL)
	|| NG_HOOK_NOT_VALID(hook)
	|| (NG_HOOK_PEER(hook) == NULL)
	|| NG_HOOK_NOT_VALID(NG_HOOK_PEER(hook))
	|| NG_NODE_NOT_VALID(NG_PEER_NODE(hook))) {
		NG_FREE_ITEM(item);
		TRAP_ERROR();
		return (ENETDOWN);
	}

	/*
	 * Transfer our interest to the other (peer) end.
	 */
	peer = NG_HOOK_PEER(hook);
	NG_HOOK_REF(peer);
	NGI_SET_HOOK(item, peer);
	peernode = NG_PEER_NODE(hook);
	NG_NODE_REF(peernode);
	NGI_SET_NODE(item, peernode);
	SET_RETADDR(item, here, retaddr);
	return (0);
}

int
ng_address_path(node_p here, item_p item, char *address, ng_ID_t retaddr)
{
	node_p	dest = NULL;
	hook_p	hook = NULL;
	int	error;

	ITEM_DEBUG_CHECKS;
	/*
	 * Note that ng_path2noderef increments the reference count
	 * on the node for us if it finds one. So we don't have to.
	 */
	error = ng_path2noderef(here, address, &dest, &hook);
	if (error) {
		NG_FREE_ITEM(item);
		return (error);
	}
	NGI_SET_NODE(item, dest);
	if ( hook) {
		NG_HOOK_REF(hook);	/* don't let it go while on the queue */
		NGI_SET_HOOK(item, hook);
	}
	SET_RETADDR(item, here, retaddr);
	return (0);
}

int
ng_address_ID(node_p here, item_p item, ng_ID_t ID, ng_ID_t retaddr)
{
	node_p dest;

	ITEM_DEBUG_CHECKS;
	/*
	 * Find the target node.
	 */
	dest = ng_ID2noderef(ID); /* GETS REFERENCE! */
	if (dest == NULL) {
		NG_FREE_ITEM(item);
		TRAP_ERROR();
		return(EINVAL);
	}
	/* Fill out the contents */
	NGI_SET_NODE(item, dest);
	NGI_CLR_HOOK(item);
	SET_RETADDR(item, here, retaddr);
	return (0);
}

/*
 * special case to send a message to self (e.g. destroy node)
 * Possibly indicate an arrival hook too.
 * Useful for removing that hook :-)
 */
item_p
ng_package_msg_self(node_p here, hook_p hook, struct ng_mesg *msg)
{
	item_p item;

	/*
	 * Find the target node.
	 * If there is a HOOK argument, then use that in preference
	 * to the address.
	 */
	if ((item = ng_getqblk(NG_NOFLAGS)) == NULL) {
		NG_FREE_MSG(msg);
		return (NULL);
	}

	/* Fill out the contents */
	item->el_flags = NGQF_MESG | NGQF_WRITER;
	item->el_next = NULL;
	NG_NODE_REF(here);
	NGI_SET_NODE(item, here);
	if (hook) {
		NG_HOOK_REF(hook);
		NGI_SET_HOOK(item, hook);
	}
	NGI_MSG(item) = msg;
	NGI_RETADDR(item) = ng_node2ID(here);
	return (item);
}

int
ng_send_fn1(node_p node, hook_p hook, ng_item_fn *fn, void * arg1, int arg2,
	int flags)
{
	item_p item;

	if ((item = ng_getqblk(flags)) == NULL) {
		return (ENOMEM);
	}
	item->el_flags = NGQF_FN | NGQF_WRITER;
	NG_NODE_REF(node); /* and one for the item */
	NGI_SET_NODE(item, node);
	if (hook) {
		NG_HOOK_REF(hook);
		NGI_SET_HOOK(item, hook);
	}
	NGI_FN(item) = fn;
	NGI_ARG1(item) = arg1;
	NGI_ARG2(item) = arg2;
	return(ng_snd_item(item, flags));
}

/*
 * Official timeout routines for Netgraph nodes.
 */
static void
ng_callout_trampoline(void *arg)
{
	item_p item = arg;

	ng_snd_item(item, 0);
}


int
ng_callout(struct callout *c, node_p node, hook_p hook, int ticks,
    ng_item_fn *fn, void * arg1, int arg2)
{
	item_p item, oitem;

	if ((item = ng_getqblk(NG_NOFLAGS)) == NULL)
		return (ENOMEM);

	item->el_flags = NGQF_FN | NGQF_WRITER;
	NG_NODE_REF(node);		/* and one for the item */
	NGI_SET_NODE(item, node);
	if (hook) {
		NG_HOOK_REF(hook);
		NGI_SET_HOOK(item, hook);
	}
	NGI_FN(item) = fn;
	NGI_ARG1(item) = arg1;
	NGI_ARG2(item) = arg2;
	oitem = c->c_arg;
	if (callout_reset(c, ticks, &ng_callout_trampoline, item) == 1 &&
	    oitem != NULL)
		NG_FREE_ITEM(oitem);
	return (0);
}

/* A special modified version of untimeout() */
int
ng_uncallout(struct callout *c, node_p node)
{
	item_p item;
	int rval;

	KASSERT(c != NULL, ("ng_uncallout: NULL callout"));
	KASSERT(node != NULL, ("ng_uncallout: NULL node"));

	rval = callout_stop(c);
	item = c->c_arg;
	/* Do an extra check */
	if ((rval > 0) && (c->c_func == &ng_callout_trampoline) &&
	    (NGI_NODE(item) == node)) {
		/*
		 * We successfully removed it from the queue before it ran
		 * So now we need to unreference everything that was
		 * given extra references. (NG_FREE_ITEM does this).
		 */
		NG_FREE_ITEM(item);
	}
	c->c_arg = NULL;

	return (rval);
}

/*
 * Set the address, if none given, give the node here.
 */
void
ng_replace_retaddr(node_p here, item_p item, ng_ID_t retaddr)
{
	if (retaddr) {
		NGI_RETADDR(item) = retaddr;
	} else {
		/*
		 * The old return address should be ok.
		 * If there isn't one, use the address here.
		 */
		NGI_RETADDR(item) = ng_node2ID(here);
	}
}

#define TESTING
#ifdef TESTING
/* just test all the macros */
void
ng_macro_test(item_p item);
void
ng_macro_test(item_p item)
{
	node_p node = NULL;
	hook_p hook = NULL;
	struct mbuf *m;
	struct ng_mesg *msg;
	ng_ID_t retaddr;
	int	error;

	NGI_GET_M(item, m);
	NGI_GET_MSG(item, msg);
	retaddr = NGI_RETADDR(item);
	NG_SEND_DATA(error, hook, m, NULL);
	NG_SEND_DATA_ONLY(error, hook, m);
	NG_FWD_NEW_DATA(error, item, hook, m);
	NG_FWD_ITEM_HOOK(error, item, hook);
	NG_SEND_MSG_HOOK(error, node, msg, hook, retaddr);
	NG_SEND_MSG_ID(error, node, msg, retaddr, retaddr);
	NG_SEND_MSG_PATH(error, node, msg, ".:", retaddr);
	NG_FWD_MSG_HOOK(error, node, item, hook, retaddr);
}
#endif /* TESTING */

