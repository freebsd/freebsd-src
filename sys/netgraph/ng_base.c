/*
 * ng_base.c
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
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/linker.h>
#include <sys/queue.h>
#include <sys/mbuf.h>
#include <sys/ctype.h>
#include <machine/limits.h>

#include <net/netisr.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>

MODULE_VERSION(netgraph, 1);

/* List of all active nodes */
static LIST_HEAD(, ng_node) ng_nodelist;
static struct mtx	ng_nodelist_mtx;

#ifdef	NETGRAPH_DEBUG

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
	NG_INVALID,
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
	&ng_deadhook,	/* Peer is self */
	&ng_deadnode,	/* attached to deadnode */
	{},		/* hooks list */
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
static struct mtx	ng_worklist_mtx;

/* List of installed types */
static LIST_HEAD(, ng_type) ng_typelist;
static struct mtx	ng_typelist_mtx;

/* Hash related definitions */
/* Don't nead to initialise them because it's a LIST */
#define ID_HASH_SIZE 32 /* most systems wont need even this many */
static LIST_HEAD(, ng_node) ng_ID_hash[ID_HASH_SIZE];
static struct mtx	ng_idhash_mtx;

/* Mutex that protects the free queue item list */
static volatile item_p		ngqfree;	/* free ones */
static struct mtx	ngq_mtx;

/* Internal functions */
static int	ng_add_hook(node_p node, const char *name, hook_p * hookp);
static int	ng_connect(hook_p hook1, hook_p hook2);
static void	ng_disconnect_hook(hook_p hook);
static int	ng_generic_msg(node_p here, item_p item, hook_p lasthook);
static ng_ID_t	ng_decodeidname(const char *name);
static int	ngb_mod_event(module_t mod, int event, void *data);
static void	ng_worklist_remove(node_p node);
static void	ngintr(void);
static int	ng_apply_item(node_p node, item_p item);
static void	ng_flush_input_queue(struct ng_queue * ngq);
static void	ng_setisr(node_p node);
static node_p	ng_ID2noderef(ng_ID_t ID);

/* imported , these used to be externally visible, some may go back */
int	ng_bypass(hook_p hook1, hook_p hook2);
int	ng_con_nodes(node_p node, const char *name, node_p node2,
	const char *name2);
void	ng_destroy_hook(hook_p hook);
node_p	ng_name2noderef(node_p node, const char *name);
int	ng_path2noderef(node_p here, const char *path,
	node_p *dest, hook_p *lasthook);
struct	ng_type *ng_findtype(const char *type);
int	ng_make_node(const char *type, node_p *nodepp);
int	ng_mkpeer(node_p node, const char *name, const char *name2, char *type);
int	ng_path_parse(char *addr, char **node, char **path, char **hook);
void	ng_rmnode(node_p node);
void	ng_unname(node_p node);


/* Our own netgraph malloc type */
MALLOC_DEFINE(M_NETGRAPH, "netgraph", "netgraph structures and ctrl messages");
MALLOC_DEFINE(M_NETGRAPH_HOOK, "netgraph_hook", "netgraph hook structures");
MALLOC_DEFINE(M_NETGRAPH_NODE, "netgraph_node", "netgraph node structures");
MALLOC_DEFINE(M_NETGRAPH_ITEM, "netgraph_item", "netgraph item structures");
MALLOC_DEFINE(M_NETGRAPH_META, "netgraph_meta", "netgraph name storage");
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
	mtx_enter(&ng_nodelist_mtx, MTX_DEF);
	hook = LIST_FIRST(&ng_freehooks);
	if (hook) {
		LIST_REMOVE(hook, hk_hooks);
		bcopy(&hook->hk_all, &temp, sizeof(temp));
		bzero(hook, sizeof(struct ng_hook));
		bcopy(&temp, &hook->hk_all, sizeof(temp));
		mtx_exit(&ng_nodelist_mtx, MTX_DEF);
		hook->hk_magic = HK_MAGIC;
	} else {
		mtx_exit(&ng_nodelist_mtx, MTX_DEF);
		_NG_ALLOC_HOOK(hook);
		if (hook) {
			hook->hk_magic = HK_MAGIC;
			mtx_enter(&ng_nodelist_mtx, MTX_DEF);
			SLIST_INSERT_HEAD(&ng_allhooks, hook, hk_all);
			mtx_exit(&ng_nodelist_mtx, MTX_DEF);
		}
	}
	return (hook);
}

static __inline node_p
ng_alloc_node(void)
{
	node_p node;
	SLIST_ENTRY(ng_node) temp;
	mtx_enter(&ng_nodelist_mtx, MTX_DEF);
	node = LIST_FIRST(&ng_freenodes);
	if (node) {
		LIST_REMOVE(node, nd_nodes);
		bcopy(&node->nd_all, &temp, sizeof(temp));
		bzero(node, sizeof(struct ng_node));
		bcopy(&temp, &node->nd_all, sizeof(temp));
		mtx_exit(&ng_nodelist_mtx, MTX_DEF);
		node->nd_magic = ND_MAGIC;
	} else {
		mtx_exit(&ng_nodelist_mtx, MTX_DEF);
		_NG_ALLOC_NODE(node);
		if (node) {
			node->nd_magic = ND_MAGIC;
			mtx_enter(&ng_nodelist_mtx, MTX_DEF);
			SLIST_INSERT_HEAD(&ng_allnodes, node, nd_all);
			mtx_exit(&ng_nodelist_mtx, MTX_DEF);
		}
	}
	return (node);
}

#define NG_ALLOC_HOOK(hook) do { (hook) = ng_alloc_hook(); } while (0)
#define NG_ALLOC_NODE(node) do { (node) = ng_alloc_node(); } while (0)


#define NG_FREE_HOOK(hook)						\
	do {								\
		mtx_enter(&ng_nodelist_mtx, MTX_DEF);			\
		LIST_INSERT_HEAD(&ng_freehooks, hook, hk_hooks);	\
		hook->hk_magic = 0;					\
		mtx_exit(&ng_nodelist_mtx, MTX_DEF);			\
	} while (0)

#define NG_FREE_NODE(node)						\
	do {								\
		mtx_enter(&ng_nodelist_mtx, MTX_DEF);			\
		LIST_INSERT_HEAD(&ng_freenodes, node, nd_nodes);	\
		node->nd_magic = 0;					\
		mtx_exit(&ng_nodelist_mtx, MTX_DEF);			\
	} while (0)

#else /* NETGRAPH_DEBUG */ /*----------------------------------------------*/

#define NG_ALLOC_HOOK(hook) _NG_ALLOC_HOOK(hook)
#define NG_ALLOC_NODE(node) _NG_ALLOC_NODE(node)

#define NG_FREE_HOOK(hook) do { FREE((hook), M_NETGRAPH_HOOK); } while (0)
#define NG_FREE_NODE(node) do { FREE((node), M_NETGRAPH_NODE); } while (0)

#endif /* NETGRAPH_DEBUG */ /*----------------------------------------------*/

/* Warning: Generally use NG_FREE_ITEM() instead */
#define NG_FREE_ITEM_REAL(item) do { FREE((item), M_NETGRAPH_ITEM); } while (0)


/* Set this to Debugger("X") to catch all errors as they occur */
#ifndef TRAP_ERROR
#define TRAP_ERROR
#endif

static	ng_ID_t nextID = 1;

#ifdef INVARIANTS
#define CHECK_DATA_MBUF(m)	do {					\
		struct mbuf *n;						\
		int total;						\
									\
		if (((m)->m_flags & M_PKTHDR) == 0)			\
			panic("%s: !PKTHDR", __FUNCTION__);		\
		for (total = 0, n = (m); n != NULL; n = n->m_next)	\
			total += n->m_len;				\
		if ((m)->m_pkthdr.len != total) {			\
			panic("%s: %d != %d",				\
			    __FUNCTION__, (m)->m_pkthdr.len, total);	\
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
static const struct ng_parse_struct_info				\
	ng_ ## lo ## _type_info = NG_GENERIC_ ## up ## _INFO args;	\
static const struct ng_parse_type ng_generic_ ## lo ## _type = {	\
	&ng_parse_struct_type,						\
	&ng_ ## lo ## _type_info					\
}

DEFINE_PARSE_STRUCT_TYPE(mkpeer, MKPEER, ());
DEFINE_PARSE_STRUCT_TYPE(connect, CONNECT, ());
DEFINE_PARSE_STRUCT_TYPE(name, NAME, ());
DEFINE_PARSE_STRUCT_TYPE(rmhook, RMHOOK, ());
DEFINE_PARSE_STRUCT_TYPE(nodeinfo, NODEINFO, ());
DEFINE_PARSE_STRUCT_TYPE(typeinfo, TYPEINFO, ());
DEFINE_PARSE_STRUCT_TYPE(linkinfo, LINKINFO, (&ng_generic_nodeinfo_type));

/* Get length of an array when the length is stored as a 32 bit
   value immediately preceeding the array -- as with struct namelist
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
		TRAP_ERROR;
		return (EINVAL);
	}

	/* Locate the node type */
	if ((type = ng_findtype(typename)) == NULL) {
		char filename[NG_TYPELEN + 4];
		linker_file_t lf;
		int error;

		/* Not found, try to load it as a loadable module */
		snprintf(filename, sizeof(filename), "ng_%s", typename);
		error = linker_load_file(filename, &lf);
		if (error != 0)
			return (error);
		lf->userrefs++;		/* pretend loaded by the syscall */

		/* Try again, as now the type should have linked itself in */
		if ((type = ng_findtype(typename)) == NULL)
			return (ENXIO);
	}

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
		TRAP_ERROR
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
		TRAP_ERROR;
		return (EINVAL);
	}

	/* Make a node and try attach it to the type */
	NG_ALLOC_NODE(node);
	if (node == NULL) {
		TRAP_ERROR;
		return (ENOMEM);
	}
	node->nd_type = type;
	NG_NODE_REF(node);				/* note reference */
	type->refs++;

	mtx_init(&node->nd_input_queue.q_mtx, "netgraph node mutex", 0);
	node->nd_input_queue.queue = NULL;
	node->nd_input_queue.last = &node->nd_input_queue.queue;
	node->nd_input_queue.q_flags = 0;
	node->nd_input_queue.q_node = node;

	/* Initialize hook list for new node */
	LIST_INIT(&node->nd_hooks);

	/* Link us into the node linked list */
	mtx_enter(&ng_nodelist_mtx, MTX_DEF);
	LIST_INSERT_HEAD(&ng_nodelist, node, nd_nodes);
	mtx_exit(&ng_nodelist_mtx, MTX_DEF);


	/* get an ID and put us in the hash chain */
	mtx_enter(&ng_idhash_mtx, MTX_DEF);
	for (;;) { /* wrap protection, even if silly */
		node_p node2 = NULL;
		node->nd_ID = nextID++; /* 137/second for 1 year before wrap */
		/* Is there a problem with the new number? */
		if ((node->nd_ID == 0)
		|| (node2 = ng_ID2noderef(node->nd_ID))) {
			if (node2) {
				NG_NODE_UNREF(node2);
				node2 = NULL;
			}
		} else {
			break;
		}
	}
	LIST_INSERT_HEAD(&ng_ID_hash[node->nd_ID % ID_HASH_SIZE],
							node, nd_idnodes);
	mtx_exit(&ng_idhash_mtx, MTX_DEF);

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
ng_rmnode(node_p node)
{
	hook_p hook;

	/* Check if it's already shutting down */
	if ((node->nd_flags & NG_CLOSING) != 0)
		return;

	/* Add an extra reference so it doesn't go away during this */
	NG_NODE_REF(node);

	/*
	 * Mark it invalid so any newcomers know not to try use it
	 * Also add our own mark so we can't recurse
	 * note that NG_INVALID does not do this as it's also set during
	 * creation
	 */
	node->nd_flags |= NG_INVALID|NG_CLOSING;

	/* Notify all remaining connected nodes to disconnect */
	while ((hook = LIST_FIRST(&node->nd_hooks)) != NULL)
		ng_destroy_hook(hook);

	/*
	 * Drain the input queue forceably.
	 * it has no hooks so what's it going to do, bleed on someone?
	 * Theoretically we came here from a queue entry that was added
	 * Just before the queue was closed, so it should be empty anyway.
	 */
	ng_flush_input_queue(&node->nd_input_queue);

	/*
	 * Take us off the work queue if we are there.
	 * We definatly have no work to be done.
	 */
	ng_worklist_remove(node);

	/* Ask the type if it has anything to do in this case */
	if (node->nd_type && node->nd_type->shutdown) {
		(*node->nd_type->shutdown)(node);
	} else {				/* do the default thing */
		NG_NODE_UNREF(node);
	}
	if (NG_NODE_IS_VALID(node)) {
		/*
		 * Well, blow me down if the node code hasn't declared
		 * that it doesn't want to die.
		 * Presumably it is a persistant node.
		 * XXX we need a way to tell the node
		 * "No, really.. the hardware's going away.. REALLY die"
		 * We need a way
		 */ 
		return;
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
 * Remove a reference to the node, possibly the last
 */
void
ng_unref_node(node_p node)
{
	int     v;
	do {
		v = node->nd_refs;
	} while (! atomic_cmpset_int(&node->nd_refs, v, v - 1));

	if (v == 1) { /* we were the last */

		mtx_enter(&ng_nodelist_mtx, MTX_DEF);
		node->nd_type->refs--; /* XXX maybe should get types lock? */
		LIST_REMOVE(node, nd_nodes);
		mtx_exit(&ng_nodelist_mtx, MTX_DEF);

		mtx_enter(&ng_idhash_mtx, MTX_DEF);
		LIST_REMOVE(node, nd_idnodes);
		mtx_exit(&ng_idhash_mtx, MTX_DEF);

		mtx_destroy(&node->nd_input_queue.q_mtx);
		NG_FREE_NODE(node);
	}
}

/************************************************************************
			Node ID handling
************************************************************************/
static node_p
ng_ID2noderef(ng_ID_t ID)
{
	node_p node;
	mtx_enter(&ng_idhash_mtx, MTX_DEF);
	LIST_FOREACH(node, &ng_ID_hash[ID % ID_HASH_SIZE], nd_idnodes) {
		if (NG_NODE_IS_VALID(node) && (NG_NODE_ID(node) == ID)) {
			break;
		}
	}
	if(node)
		NG_NODE_REF(node);
	mtx_exit(&ng_idhash_mtx, MTX_DEF);
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
	for (i = 0; i < NG_NODELEN + 1; i++) {
		if (name[i] == '\0' || name[i] == '.' || name[i] == ':')
			break;
	}
	if (i == 0 || name[i] != '\0') {
		TRAP_ERROR;
		return (EINVAL);
	}
	if (ng_decodeidname(name) != 0) { /* valid IDs not allowed here */
		TRAP_ERROR;
		return (EINVAL);
	}

	/* Check the name isn't already being used */
	if ((node2 = ng_name2noderef(node, name)) != NULL) {
		NG_NODE_UNREF(node2);
		TRAP_ERROR;
		return (EADDRINUSE);
	}

	/* copy it */
	strncpy(NG_NODE_NAME(node), name, NG_NODELEN);

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
	mtx_enter(&ng_nodelist_mtx, MTX_DEF);
	LIST_FOREACH(node, &ng_nodelist, nd_nodes) {
		if (NG_NODE_IS_VALID(node)
		&& NG_NODE_HAS_NAME(node)
		&& (strcmp(NG_NODE_NAME(node), name) == 0)) {
			break;
		}
	}
	if (node)
		NG_NODE_REF(node);
	mtx_exit(&ng_nodelist_mtx, MTX_DEF);
	return (node);
}

/*
 * Decode a ID name, eg. "[f03034de]". Returns 0 if the
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
 */
void
ng_unname(node_p node)
{
	bzero(NG_NODE_NAME(node), NG_NODELEN);
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
	int     v;
	do {
		v = hook->hk_refs;
	} while (! atomic_cmpset_int(&hook->hk_refs, v, v - 1));

	if (v == 1) { /* we were the last */
		if (NG_HOOK_NODE(hook)) {
			NG_NODE_UNREF((NG_HOOK_NODE(hook)));
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
		TRAP_ERROR;
		return (EINVAL);
	}
	if (ng_findhook(node, name) != NULL) {
		TRAP_ERROR;
		return (EEXIST);
	}

	/* Allocate the hook and link it up */
	NG_ALLOC_HOOK(hook);
	if (hook == NULL) {
		TRAP_ERROR;
		return (ENOMEM);
	}
	hook->hk_refs = 1;		/* add a reference for us to return */
	hook->hk_flags = HK_INVALID;
	hook->hk_peer = &ng_deadhook;	/* start off this way */
	hook->hk_node = node;
	NG_NODE_REF(node);		/* each hook counts as a reference */

	/* Set hook name */
	strncpy(NG_HOOK_NAME(hook), name, NG_HOOKLEN);

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
 * Connect a pair of hooks. Only used internally.
 * Assume whoever called us has the nodes locked
 * and is holding refs on the hooks.
 * XXX This may have to change to be SMP safe.
 */
static int
ng_connect(hook_p hook1, hook_p hook2)
{
	int     error;

	hook1->hk_peer = hook2;
	hook2->hk_peer = hook1;

	/* Each hook is referenced by the other */
	NG_HOOK_REF(hook1);
	NG_HOOK_REF(hook2);

	/* Give each node the opportunity to veto the pending connection */
	if (hook1->hk_node->nd_type->connect) {
		if ((error = (*hook1->hk_node->nd_type->connect) (hook1))) {
			ng_destroy_hook(hook1);	/* also zaps hook2 */
			return (error);
		}
	}
	if (hook2->hk_node->nd_type->connect) {
		if ((error = (*hook2->hk_node->nd_type->connect) (hook2))) {
			ng_destroy_hook(hook2);	/* also zaps hook1 */
			return (error);
		}
	}

	/* As a last act, allow the hooks to be used */
	hook1->hk_flags &= ~HK_INVALID;
	hook2->hk_flags &= ~HK_INVALID;
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
 */
void
ng_destroy_hook(hook_p hook)
{
	hook_p peer = NG_HOOK_PEER(hook);

	hook->hk_flags |= HK_INVALID;		/* as soon as possible */
	if (peer && (peer != &ng_deadhook)) {
		/*
		 * Set the peer to point to ng_deadhook
		 * from this moment on we are effectively independent it.
		 * send it an rmhook message of it's own.
		 */
		peer->hk_peer = &ng_deadhook;	/* They no longer know us */
		hook->hk_peer = &ng_deadhook;	/* Nor us, them */
		ng_rmhook_self(peer); 		/* Give it a surprise */
	}
	ng_disconnect_hook(hook);
	NG_HOOK_UNREF(hook);			/* account for peer link */
}

/*
 * Notify the node of the hook's demise. This may result in more actions
 * (e.g. shutdown) but we don't do that ourselves and don't know what
 * happens there. If there is no appropriate handler, then just remove it
 * and decrement the reference count. When it's freed it will decrement
 * the reference count of it's node which in turn might make something happen.
 */
static void
ng_disconnect_hook(hook_p hook)
{
	node_p node = NG_HOOK_NODE(hook);

	/*
	 * Remove the hook from the node's list to avoid possible recursion
	 * in case the disconnection results in node shutdown.
	 */
	LIST_REMOVE(hook, hk_hooks);
	node->nd_numhooks--;
	if (node->nd_type->disconnect) {
		/*
		 * The type handler may elect to destroy the peer so don't
		 * trust its existance after this point.
		 */
		(*node->nd_type->disconnect) (hook);
	}
	NG_HOOK_UNREF(hook);	/* Account for linkage to node */
}

/*
 * Take two hooks on a node and merge the connection so that the given node
 * is effectively bypassed.
 */
int
ng_bypass(hook_p hook1, hook_p hook2)
{
	if (hook1->hk_node != hook2->hk_node) {
		TRAP_ERROR
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
	|| (namelen > NG_TYPELEN)) {
		TRAP_ERROR;
		return (EINVAL);
	}

	/* Check for name collision */
	if (ng_findtype(tp->name) != NULL) {
		TRAP_ERROR;
		return (EEXIST);
	}

	tp->refs = 0;

	/* Link in new type */
	mtx_enter(&ng_typelist_mtx, MTX_DEF);
	LIST_INSERT_HEAD(&ng_typelist, tp, types);
	mtx_exit(&ng_typelist_mtx, MTX_DEF);
	return (0);
}

/*
 * Look for a type of the name given
 */
struct ng_type *
ng_findtype(const char *typename)
{
	struct ng_type *type;

	mtx_enter(&ng_typelist_mtx, MTX_DEF);
	LIST_FOREACH(type, &ng_typelist, types) {
		if (strcmp(type->name, typename) == 0)
			break;
	}
	mtx_exit(&ng_typelist_mtx, MTX_DEF);
	return (type);
}

/************************************************************************
			Composite routines
************************************************************************/

/*
 * Make a peer and connect. The order is arranged to minimise
 * the work needed to back out in case of error.
 * We assume that the local node is locked.
 * The new node probably doesn't need a lock until
 * it has a hook, but we should think about it a bit more.
 */
int
ng_mkpeer(node_p node, const char *name, const char *name2, char *type)
{
	node_p  node2;
	hook_p  hook;
	hook_p  hook2;
	int     error;

	if ((error = ng_add_hook(node, name, &hook))) /* gives us a ref */
		return (error);
	if ((error = ng_make_node(type, &node2))) {
		ng_destroy_hook(hook);
		NG_HOOK_UNREF(hook);
		return (error);
	}
	if ((error = ng_add_hook(node2, name2, &hook2))) {
		ng_rmnode(node2);
		ng_destroy_hook(hook);
		NG_HOOK_UNREF(hook);
		return (error);
	}

	/*
	 * Actually link the two hooks together.. on failure they are
	 * destroyed so we don't have to do that here.
	 */
	if ((error = ng_connect(hook, hook2))) 
		ng_rmnode(node2);
	/*
	 * drop the references we were holding on the two hooks.
	 */
	NG_HOOK_UNREF(hook);
	NG_HOOK_UNREF(hook2);
	return (error);
}

/*
 * Connect two nodes using the specified hooks
 * XXX need to think of a SMP safe way of doing this.
 */
int
ng_con_nodes(node_p node, const char *name, node_p node2, const char *name2)
{
	int     error;
	hook_p  hook;
	hook_p  hook2;

	if ((error = ng_add_hook(node, name, &hook)))  /* gives us a ref */
		return (error);
	if ((error = ng_add_hook(node2, name2, &hook2))) { /* we get a ref */
		ng_destroy_hook(hook);
		NG_HOOK_UNREF(hook);
		return (error);
	}
	error = ng_connect(hook, hook2);

	/*
	 * drop the references we were holding on the two hooks.
	 */
	NG_HOOK_UNREF(hook);
	NG_HOOK_UNREF(hook2);
	return (error);
}
/************************************************************************
		Utility routines to send self messages
************************************************************************/
/*
 * Static version of shutdown message. we don't want to need resources
 * to shut down (we may be doing it to release resources because we ran out.
 */
static struct	ng_mesg  ng_msg_shutdown = {
	{NG_VERSION,		/* u_char */
	0,			/* u_char spare */
	0,			/* u_int16_t arglen */
	NGF_STATIC,		/* u_int32_t flags */
	0,			/* u_int32_t token */
	NGM_GENERIC_COOKIE,	/* u_int32_t */
	NGM_SHUTDOWN,		/* u_int32_t */
	"shutdown"}		/* u_char[16] */
};
	
int
ng_rmnode_self(node_p here)
{
	item_p	item;
	struct	ng_mesg	*msg;

	/*
	 * Use the static version to avoid needing
	 * memory allocation to succeed.
	 * The message is never written to and always the same.
	 */
	msg = &ng_msg_shutdown;

	/*
	 * Try get a queue item to send it with.
	 * Hopefully since it has a reserve, we can get one.
	 * If we can't we are screwed anyhow.
	 * Increase the chances by flushing our queue first.
	 * We may free an item, (if we were the hog).
	 * Work in progress is allowed to complete.
	 * We also pretty much ensure that we come straight
	 * back in to do the shutdown. It may be a good idea
	 * to hold a reference actually to stop it from all
	 * going up in smoke.
	 */
/*	ng_flush_input_queue(&here->nd_input_queue); will mask problem  */
	item = ng_package_msg_self(here, NULL, msg);
	if (item == NULL) { /* it would have freed the msg except static */
		/* try again after flushing our queue */
		ng_flush_input_queue(&here->nd_input_queue);
		item = ng_package_msg_self(here, NULL, msg);
		if (item == NULL) {
			printf("failed to free node 0x%x\n", ng_node2ID(here));
			return (ENOMEM);
		}
	}
	return (ng_snd_item(item, 0));
}

#define	NG_INTERNAL_RMHOOK	0x123456
int
ng_rmhook_self(hook_p hook)
{
	item_p	item;
	struct	ng_mesg	*msg;
	node_p node = NG_HOOK_NODE(hook);

	NG_MKMESSAGE(msg, NGM_GENERIC_COOKIE, NG_INTERNAL_RMHOOK, 0, M_NOWAIT);
	/*
	 * Try get a queue item to send it with.
	 * Hopefully since it has a reserve, we can get one.
	 * If we can't we are screwed anyhow.
	 * Increase the chances by flushing our queue first.
	 * We may free an item, (if we were the hog).
	 * Work in progress is allowed to complete.
	 * We also pretty much ensure that we come straight
	 * back in to do the shutdown. It may be a good idea
	 * to hold a reference actually to stop it from all
	 * going up in smoke.
	 */
	item = ng_package_msg_self(node, hook, msg);
	if (item == NULL) {	/* couldn't allocate item. Freed msg */
		/* try again after flushing our queue */
		panic("Couldn't allocate item to remove hook");
	}
	return (ng_snd_item(item, 0));
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
	char   *node, *path, *hook;
	int     k;

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
	char    fullpath[NG_PATHLEN + 1];
	char   *nodename, *path, pbuf[2];
	node_p  node, oldnode;
	char   *cp;
	hook_p hook = NULL;

	/* Initialize */
	if (destp == NULL) {
		TRAP_ERROR
		return EINVAL;
	}
	*destp = NULL;

	/* Make a writable copy of address for ng_path_parse() */
	strncpy(fullpath, address, sizeof(fullpath) - 1);
	fullpath[sizeof(fullpath) - 1] = '\0';

	/* Parse out node and sequence of hooks */
	if (ng_path_parse(fullpath, &nodename, &path, NULL) < 0) {
		TRAP_ERROR;
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
			TRAP_ERROR
			return (ENOENT);
		}
	} else {
		if (here == NULL) {
			TRAP_ERROR
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
			TRAP_ERROR;
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
		TRAP_ERROR;
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

static __inline item_p ng_dequeue(struct ng_queue * ngq);
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
 * The ordering here is important! don't shuffle these. If you add
 * READ_PENDING to the word when it has READ_PENDING already set, you
 * generate a carry into the reader count, this you atomically add a reader,
 * and remove the pending reader count! Similarly for the pending writer
 * flag, adding WRITE_PENDING generates a carry and sets the WRITER_ACTIVE
 * flag, while clearing WRITE_PENDING. When 'SINGLE_THREAD_ONLY' is set, then
 * it is only permitted to do WRITER operations. Reader operations will
 * result in errors.
 * But that "hack" is unnecessary: "cpp" can do the math for us!
 */
/*-
 Safety Barrier--------+ (adjustable to suit taste) (not used yet)
                       |
                       V
+-------+-------+-------+-------+-------+-------+-------+-------+
| | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
|A|c|t|i|v|e| |R|e|a|d|e|r| |C|o|u|n|t| | | | | | | | | |R|A|W|S|
| | | | | | | | | | | | | | | | | | | | | | | | | | | | |P|W|P|T|
+-------+-------+-------+-------+-------+-------+-------+-------+
\_________________________ ____________________________/ | | | |
                          V                              | | | |
                [active reader count]                    | | | |
                                                         | | | |
        Read Pending ------------------------------------+ | | |
                                                           | | |
        Active Writer -------------------------------------+ | |
                                                             | |
        Write Pending ---------------------------------------+ |
                                                               |
        Single Threading Only ---------------------------------+
*/
#define	SINGLE_THREAD_ONLY 0x00000001	/* if set, even reads single thread */
#define WRITE_PENDING	0x00000002
#define	WRITER_ACTIVE	0x00000004
#define READ_PENDING	0x00000008
#define	READER_INCREMENT 0x00000010
#define	READER_MASK	0xfffffff0	/* Not valid if WRITER_ACTIVE is set */
#define SAFETY_BARRIER	0x00100000	/* 64K items queued should be enough */
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
ng_dequeue(struct ng_queue *ngq)
{
	item_p item;
	u_int		add_arg;
	/*
	 * If there is a writer, then the answer is "no". Everything else
	 * stops when there is a WRITER.
	 */
	if (ngq->q_flags & WRITER_ACTIVE) {
		return (NULL);
	}
	/* Now take a look at what's on the queue and what's running */
	if ((ngq->q_flags & ~(READER_MASK | SINGLE_THREAD_ONLY)) == READ_PENDING) {
		/*
		 * It was a reader and we have no write active. We don't care
		 * how many readers are already active. Adjust the count for
		 * the item we are about to dequeue. Adding READ_PENDING to
		 * the exisiting READ_PENDING clears it and generates a carry
		 * into the reader count.
		 */
		add_arg = READ_PENDING;
	} else if ((ngq->q_flags & ~SINGLE_THREAD_ONLY) == WRITE_PENDING) {
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
		 * the flag before this operation, the WRITE_PENDING flag
		 * will make it fail, and if it tests it later, the
		 * ACTIVE_WRITER flag will do the same. If it is SO slow that
		 * we have actually completed the operation, and neither flag
		 * is set (nor the READ_PENDING) by the time that it tests
		 * the flags, then it is actually ok for it to continue. If
		 * it completes and we've finished and the read pending is
		 * set it still fails.
		 *
		 * So we can just ignore it,  as long as we can ensure that the
		 * transition from WRITE_PENDING state to the WRITER_ACTIVE
		 * state is atomic.
		 *
		 * After failing, first it will be held back by the mutex, then
		 * when it can proceed, it will queue its request, then it
		 * would arrive at this function. Usually it will have to
		 * leave empty handed because the ACTIVE WRITER bit wil be
		 * set.
		 */
		/*
		 * Adjust the flags for the item we are about to dequeue.
		 * Adding WRITE_PENDING to the exisiting WRITE_PENDING clears
		 * it and generates a carry into the WRITER_ACTIVE flag, all
		 * atomically.
		 */
		add_arg = WRITE_PENDING;
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
		 * state, then it will be through in just a moment, (as soon
		 * as we release the mutex) and keep things moving.
		 */
		return (0);
	}

	/*
	 * Now we dequeue the request (whatever it may be) and correct the
	 * pending flags and the next and last pointers.
	 */
	item = ngq->queue;
	ngq->queue = item->el_next;
	if (ngq->last == &(item->el_next)) {
		/*
		 * that was the last entry in the queue so set the 'last
		 * pointer up correctly and make sure the pending flags are
		 * clear.
		 */
		ngq->last = &(ngq->queue);
		/*
		 * Whatever flag was set is cleared and the carry sets the
		 * correct new active state/count. So we don't need to change
		 * add_arg.
		 */
	} else {
		if ((ngq->queue->el_flags & NGQF_TYPE) == NGQF_READER) {
			/*
			 * If we had a READ_PENDING and have another one, we
			 * just want to add READ_PENDING twice (the same as
			 * adding READER_INCREMENT). If we had WRITE_PENDING,
			 * we want to add READ_PENDING + WRITE_PENDING to
			 * clear the old WRITE_PENDING, set ACTIVE_WRITER,
			 * and set READ_PENDING. Either way we just add
			 * READ_PENDING to whatever we already had.
			 */
			add_arg += READ_PENDING;
		} else {
			/*
			 * If we had a WRITE_PENDING and have another one, we
			 * just want to add WRITE_PENDING twice (the same as
			 * adding ACTIVE_WRITER). If we had READ_PENDING, we
			 * want to add READ_PENDING + WRITE_PENDING to clear
			 * the old READ_PENDING, increment the readers, and
			 * set WRITE_PENDING. Either way we just add
			 * WRITE_PENDING to whatever we already had.
			 */
			add_arg += WRITE_PENDING;
		}
	}
	atomic_add_long(&ngq->q_flags, add_arg);
	/*
	 * We have successfully cleared the old pending flag, set the new one
	 * if it is needed, and incremented the appropriate active field.
	 * (all in one atomic addition.. wow)
	 */
	return (item);
}

/*
 * Queue a packet to be picked up by someone else.
 * We really don't care who, but we can't or don't want to hang around
 * to process it ourselves. We are probably an interrupt routine..
 * 1 = writer, 0 = reader
 * We should set something to indicate NETISR requested
 * If it's the first item queued.
 */
#define NGQRW_R 0
#define NGQRW_W 1
static __inline void
ng_queue_rw(struct ng_queue * ngq, item_p  item, int rw)
{
	item->el_next = NULL;	/* maybe not needed */
	*ngq->last = item;
	/*
	 * If it was the first item in the queue then we need to
	 * set the last pointer and the type flags.
	 */
	if (ngq->last == &(ngq->queue)) {
		/*
		 * When called with constants for rw, the optimiser will
		 * remove the unneeded branch below.
		 */
		if (rw == NGQRW_W) {
			atomic_add_long(&ngq->q_flags, WRITE_PENDING);
		} else {
			atomic_add_long(&ngq->q_flags, READ_PENDING);
		}
	}
	ngq->last = &(item->el_next);
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

	/* ######### Hack alert ######### */
	atomic_add_long(&ngq->q_flags, READER_INCREMENT);
	if ((ngq->q_flags & (~READER_MASK)) == 0) {
		/* Successfully grabbed node */
		return (item);
	}
	/* undo the damage if we didn't succeed */
	atomic_subtract_long(&ngq->q_flags, READER_INCREMENT);

	/* ######### End Hack alert ######### */
	mtx_enter((&ngq->q_mtx), MTX_SPIN);
	/*
	 * Try again. Another processor (or interrupt for that matter) may
	 * have removed the last queued item that was stopping us from
	 * running, between the previous test, and the moment that we took
	 * the mutex. (Or maybe a writer completed.)
	 */
	if ((ngq->q_flags & (~READER_MASK)) == 0) {
		atomic_add_long(&ngq->q_flags, READER_INCREMENT);
		mtx_exit((&ngq->q_mtx), MTX_SPIN);
		return (item);
	}

	/*
	 * Quick check that we are doing things right.
	 */
	if (ngq->q_flags & SINGLE_THREAD_ONLY) {
		panic("Calling single treaded queue incorrectly");
	}

	/*
	 * and queue the request for later.
	 */
	item->el_flags |= NGQF_TYPE;
	ng_queue_rw(ngq, item, NGQRW_R);

	/*
	 * Ok, so that's the item successfully queued for later. So now we
	 * see if we can dequeue something to run instead.
	 */
	item = ng_dequeue(ngq);
	mtx_exit(&(ngq->q_mtx), MTX_SPIN);
	return (item);
}

static __inline item_p
ng_acquire_write(struct ng_queue *ngq, item_p item)
{
restart:
	mtx_enter(&(ngq->q_mtx), MTX_SPIN);
	/*
	 * If there are no readers, no writer, and no pending packets, then
	 * we can just go ahead. In all other situations we need to queue the
	 * request
	 */
	if ((ngq->q_flags & (~SINGLE_THREAD_ONLY)) == 0) {
		atomic_add_long(&ngq->q_flags, WRITER_ACTIVE);
		mtx_exit((&ngq->q_mtx), MTX_SPIN);
		if (ngq->q_flags & READER_MASK) {
			/* Collision with fast-track reader */
			atomic_add_long(&ngq->q_flags, -WRITER_ACTIVE);
			goto restart;
		}

		return (item);
	}

	/*
	 * and queue the request for later.
	 */
	item->el_flags &= ~NGQF_TYPE;
	ng_queue_rw(ngq, item, NGQRW_W);

	/*
	 * Ok, so that's the item successfully queued for later. So now we
	 * see if we can dequeue something to run instead.
	 */
	item = ng_dequeue(ngq);
	mtx_exit(&(ngq->q_mtx), MTX_SPIN);
	return (item);
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
	u_int		add_arg;
	mtx_enter(&ngq->q_mtx, MTX_SPIN);
	for (;;) {
		/* Now take a look at what's on the queue */
		if (ngq->q_flags & READ_PENDING) {
			add_arg = -READ_PENDING;
		} else if (ngq->q_flags & WRITE_PENDING) {
			add_arg = -WRITE_PENDING;
		} else {
			break;
		}

		item = ngq->queue;
		ngq->queue = item->el_next;
		if (ngq->last == &(item->el_next)) {
			ngq->last = &(ngq->queue);
		} else {
			if ((ngq->queue->el_flags & NGQF_TYPE) == NGQF_READER) {
				add_arg += READ_PENDING;
			} else {
				add_arg += WRITE_PENDING;
			}
		}
		atomic_add_long(&ngq->q_flags, add_arg);

		mtx_exit(&ngq->q_mtx, MTX_SPIN);
		NG_FREE_ITEM(item);
		mtx_enter(&ngq->q_mtx, MTX_SPIN);
	}
	mtx_exit(&ngq->q_mtx, MTX_SPIN);
}

/***********************************************************************
* Externally visible method for sending or queueing messages or data.
***********************************************************************/

/*
 * MACRO WILL DO THE JOB OF CALLING ng_package_msg IN CALLER
 * before we are called. The user code should have filled out the item
 * correctly by this stage:
 * Common:
 *    reference to destination node.
 *    Reference to destination rcv hook if relevant.
 * Data:
 *    pointer to mbuf
 *    pointer to metadata
 * Control_Message:
 *    pointer to msg.
 *    ID of original sender node. (return address)
 *
 * The nodes have several routines and macros to help with this task:
 * ng_package_msg()
 * ng_package_data() do much of the work.
 * ng_retarget_msg
 * ng_retarget_data
 */

int
ng_snd_item(item_p item, int queue)
{
	hook_p hook = item->el_hook;
	node_p dest = item->el_dest;
	int rw;
	int error = 0, ierror;
	item_p	oitem;
	struct ng_queue * ngq = &dest->nd_input_queue;

#ifdef	NETGRAPH_DEBUG
        _ngi_check(item, __FILE__, __LINE__);
#endif

	if (item == NULL) {
		TRAP_ERROR;
		return (EINVAL);	/* failed to get queue element */
	}
	if (dest == NULL) {
		NG_FREE_ITEM(item);
		TRAP_ERROR;
		return (EINVAL);	/* No address */
	}
	if ((item->el_flags & NGQF_D_M) == NGQF_DATA) {
		/*
		 * DATA MESSAGE
		 * Delivered to a node via a non-optional hook.
		 * Both should be present in the item even though
		 * the node is derivable from the hook.
		 * References are held on both by the item.
		 */
#ifdef	NETGRAPH_DEBUG
        _ngi_check(item, __FILE__, __LINE__);
#endif
		CHECK_DATA_MBUF(NGI_M(item));
		if (hook == NULL) {
			NG_FREE_ITEM(item);
			TRAP_ERROR;
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
		/* By default data is a reader in the locking scheme */
		item->el_flags |= NGQF_READER;
		rw = NGQRW_R;
	} else {
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
		/* Data messages count as writers unles explicitly exempted */
		if (NGI_MSG(item)->header.cmd & NGM_READONLY) {
			item->el_flags |= NGQF_READER;
			rw = NGQRW_R;
		} else {
			item->el_flags &= ~NGQF_TYPE;
			rw = NGQRW_W;
		}
	}
	/*
	 * If the node specifies single threading, force writer semantics
	 * Similarly the node may say one hook always produces writers.
	 * These are over-rides.
	 */
	if ((ngq->q_flags & SINGLE_THREAD_ONLY)
	|| (dest->nd_flags & NG_FORCE_WRITER)
	|| (hook && (hook->hk_flags & HK_FORCE_WRITER))) {
			rw = NGQRW_W;
			item->el_flags &= ~NGQF_TYPE;
	}
	if (queue) {
		/* Put it on the queue for that node*/
#ifdef	NETGRAPH_DEBUG
        _ngi_check(item, __FILE__, __LINE__);
#endif
		mtx_enter(&(ngq->q_mtx), MTX_SPIN);
		ng_queue_rw(ngq, item, rw);
		mtx_exit(&(ngq->q_mtx), MTX_SPIN);
		/*
		 * If there are active elements then we can rely on
		 * them. if not we should not rely on another packet
		 * coming here by another path,
		 * so it is best to put us in the netisr list.
		 */
		if ((ngq->q_flags & (READER_MASK|WRITER_ACTIVE)) == 0) {
			ng_setisr(ngq->q_node);
		}
		return (0);
	}
	/*
	 * Take a queue item and a node and see if we can apply the item to
	 * the node. We may end up getting a different item to apply instead.
	 * Will allow for a piggyback reply only in the case where
	 * there is no queueing.
	 */

	oitem = item;
	/*
	 * We already decided how we will be queueud or treated.
	 * Try get the appropriate operating permission.
	 */
 	if (rw == NGQRW_R) {
		item = ng_acquire_read(ngq, item);
	} else {
		item = ng_acquire_write(ngq, item);
	}

	/*
	 * May have come back with a different item.
	 * or maybe none at all. The one we started with will
	 * have been queued in thises cases.
	 */
	if (item == NULL) {
		return (0);
	}

#ifdef	NETGRAPH_DEBUG
        _ngi_check(item, __FILE__, __LINE__);
#endif
	ierror = ng_apply_item(dest, item); /* drops r/w lock when done */

	/* only return an error if it was our initial item.. (compat hack) */
	if (oitem == item) {
		error = ierror;
	}

	/*
	 * Now we've handled the packet we brought, (or a friend of it) let's
	 * look for any other packets that may have been queued up. We hold
	 * no locks, so if someone puts something in the queue after
	 * we check that it is empty, it is their problem
	 * to ensure it is processed. If we have the netisr thread cme in here
	 * while we still say we have stuff to do, we may get a boost
	 * in SMP systems. :-)
	 */
	for (;;) {
		/* quick hack to save all that mutex stuff */
		if ((ngq->q_flags & (WRITE_PENDING | READ_PENDING)) == 0) {
			if (dest->nd_flags & NG_WORKQ)
				ng_worklist_remove(dest);
			return (0);
		}
		/*
		 * dequeue acquires and adjusts the input_queue as it dequeues
		 * packets. It acquires the rw lock as needed.
		 */
		mtx_enter(&ngq->q_mtx, MTX_SPIN);
		item = ng_dequeue(ngq);
		mtx_exit(&ngq->q_mtx, MTX_SPIN);
		if (!item) {
			/*
			 * If we have no work to do
			 * then we certainly don't need to be
			 * on the worklist.
			 */
			if (dest->nd_flags & NG_WORKQ)
				ng_worklist_remove(dest);
			return (0);
		}
#ifdef	NETGRAPH_DEBUG
        _ngi_check(item, __FILE__, __LINE__);
#endif

		/*
		 * We have the appropriate lock, so run the item.
		 * When finished it will drop the lock accordingly
		 */

		ierror = ng_apply_item(dest, item);
		/*
		 * only return an error if it was our initial
		 * item.. (compat hack)
		 */
		if (oitem == item) {
			error = ierror;
		}
	}
	return (0);
}

/*
 * We have an item that was possibly queued somewhere.
 * It should contain all the information needed
 * to run it on the appropriate node/hook.
 */
static int
ng_apply_item(node_p node, item_p item)
{
	hook_p  hook;
	int was_reader = ((item->el_flags & NGQF_TYPE));
	int error = 0;
	ng_rcvdata_t *rcvdata;

	hook = item->el_hook;
	item->el_hook = NULL;	/* so NG_FREE_ITEM doesn't NG_HOOK_UNREF() */
	/* We already have the node.. assume responsibility */
	/* And the reference */
	/* node = item->el_dest; */
	item->el_dest = NULL;	/* same as for the hook above */
#ifdef	NETGRAPH_DEBUG
        _ngi_check(item, __FILE__, __LINE__);
#endif

	switch (item->el_flags & NGQF_D_M) {
	case NGQF_DATA:
		/*
		 * Check things are still ok as when we were queued.
		 */

		if ((hook == NULL)
		|| NG_HOOK_NOT_VALID(hook)
		|| NG_NODE_NOT_VALID(NG_HOOK_NODE(hook))
		|| ((rcvdata = NG_HOOK_NODE(hook)->nd_type->rcvdata) == NULL)) {
			error = EIO;
			NG_FREE_ITEM(item);
		} else {
			error = (*rcvdata)(hook, item);
		}
		break;
	case NGQF_MESG:

		if (hook) {
			item->el_hook = NULL;
			if (NG_HOOK_NOT_VALID(hook)) {
			/*
			 * If the hook has been zapped then we can't use it.
			 * Immediatly drop its reference.
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
			TRAP_ERROR;
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

			if ((msg->header.typecookie == NGM_GENERIC_COOKIE)
			&& ((msg->header.flags & NGF_RESP) == 0)) {
				error = ng_generic_msg(node, item, hook);
			} else {
				if ((node)->nd_type->rcvmsg != NULL) {
					error = (*(node)->nd_type->rcvmsg)((node),
						(item), (hook));
				} else {
					TRAP_ERROR;
					error = EINVAL; /* XXX */
					NG_FREE_ITEM(item);
				}
			}
			/* item is now invalid */
		}
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

	if (was_reader) {
		ng_leave_read(&node->nd_input_queue);
	} else {
		ng_leave_write(&node->nd_input_queue);
	}
	NG_NODE_UNREF(node);
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
		TRAP_ERROR
		error = EINVAL;
		goto out;
	}
	switch (msg->header.cmd) {
	case NGM_SHUTDOWN:
		ng_rmnode(here);
		break;
	case NGM_MKPEER:
	    {
		struct ngm_mkpeer *const mkp = (struct ngm_mkpeer *) msg->data;

		if (msg->header.arglen != sizeof(*mkp)) {
			TRAP_ERROR
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
			TRAP_ERROR
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
			TRAP_ERROR
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
			TRAP_ERROR
			error = EINVAL;
			break;
		}
		rmh->ourhook[sizeof(rmh->ourhook) - 1] = '\0';
		if ((hook = ng_findhook(here, rmh->ourhook)) != NULL)
			ng_destroy_hook(hook);
		break;
	    }
	case NG_INTERNAL_RMHOOK:
		ng_destroy_hook(lasthook);
		break;
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
			strncpy(ni->name, NG_NODE_NAME(here), NG_NODELEN);
		strncpy(ni->type, here->nd_type->name, NG_TYPELEN);
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
			strncpy(ni->name, NG_NODE_NAME(here), NG_NODELEN);
		strncpy(ni->type, here->nd_type->name, NG_TYPELEN);
		ni->id = ng_node2ID(here);

		/* Cycle through the linked list of hooks */
		ni->hooks = 0;
		LIST_FOREACH(hook, &here->nd_hooks, hk_hooks) {
			struct linkinfo *const link = &hl->link[ni->hooks];

			if (ni->hooks >= nhooks) {
				log(LOG_ERR, "%s: number of %s changed\n",
				    __FUNCTION__, "hooks");
				break;
			}
			if (NG_HOOK_NOT_VALID(hook))
				continue;
			strncpy(link->ourhook, NG_HOOK_NAME(hook), NG_HOOKLEN);
			strncpy(link->peerhook,
				NG_PEER_HOOK_NAME(hook), NG_HOOKLEN);
			if (NG_PEER_NODE_NAME(hook)[0] != '\0')
				strncpy(link->nodeinfo.name,
				    NG_PEER_NODE_NAME(hook), NG_NODELEN);
			strncpy(link->nodeinfo.type,
			   NG_PEER_NODE(hook)->nd_type->name, NG_TYPELEN);
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

		mtx_enter(&ng_nodelist_mtx, MTX_DEF);
		/* Count number of nodes */
		LIST_FOREACH(node, &ng_nodelist, nd_nodes) {
			if (NG_NODE_IS_VALID(node)
			&& (unnamed || NG_NODE_HAS_NAME(node))) {
				num++;
			}
		}
		mtx_exit(&ng_nodelist_mtx, MTX_DEF);

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
		mtx_enter(&ng_nodelist_mtx, MTX_DEF);
		LIST_FOREACH(node, &ng_nodelist, nd_nodes) {
			struct nodeinfo *const np = &nl->nodeinfo[nl->numnames];

			if (nl->numnames >= num) {
				log(LOG_ERR, "%s: number of %s changed\n",
				    __FUNCTION__, "nodes");
				break;
			}
			if (NG_NODE_NOT_VALID(node))
				continue;
			if (!unnamed && (! NG_NODE_HAS_NAME(node)))
				continue;
			if (NG_NODE_HAS_NAME(node))
				strncpy(np->name, NG_NODE_NAME(node), NG_NODELEN);
			strncpy(np->type, node->nd_type->name, NG_TYPELEN);
			np->id = ng_node2ID(node);
			np->hooks = node->nd_numhooks;
			nl->numnames++;
		}
		mtx_exit(&ng_nodelist_mtx, MTX_DEF);
		break;
	    }

	case NGM_LISTTYPES:
	    {
		struct typelist *tl;
		struct ng_type *type;
		int num = 0;

		mtx_enter(&ng_typelist_mtx, MTX_DEF);
		/* Count number of types */
		LIST_FOREACH(type, &ng_typelist, types) {
			num++;
		}
		mtx_exit(&ng_typelist_mtx, MTX_DEF);

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
		mtx_enter(&ng_typelist_mtx, MTX_DEF);
		LIST_FOREACH(type, &ng_typelist, types) {
			struct typeinfo *const tp = &tl->typeinfo[tl->numtypes];

			if (tl->numtypes >= num) {
				log(LOG_ERR, "%s: number of %s changed\n",
				    __FUNCTION__, "types");
				break;
			}
			strncpy(tp->type_name, type->name, NG_TYPELEN);
			tp->numnodes = type->refs;
			tl->numtypes++;
		}
		mtx_exit(&ng_typelist_mtx, MTX_DEF);
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
		if (msg->header.arglen < sizeof(struct ng_mesg)
		    || (msg->header.arglen - sizeof(struct ng_mesg)
		      < binary->header.arglen)) {
			TRAP_ERROR
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
		if ((msg->header.arglen < sizeof(*ascii) + 1)
		    || (ascii->header.arglen < 1)
		    || (msg->header.arglen
		      < sizeof(*ascii) + ascii->header.arglen)) {
			TRAP_ERROR
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
		TRAP_ERROR;
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
	if ( msg && ((msg->header.flags & NGF_STATIC) == 0))
		NG_FREE_MSG(msg);
	return (error);
}

/*
 * Copy a 'meta'.
 *
 * Returns new meta, or NULL if original meta is NULL or ENOMEM.
 */
meta_p
ng_copy_meta(meta_p meta)
{
	meta_p meta2;

	if (meta == NULL)
		return (NULL);
	MALLOC(meta2, meta_p, meta->used_len, M_NETGRAPH_META, M_NOWAIT);
	if (meta2 == NULL)
		return (NULL);
	meta2->allocated_len = meta->used_len;
	bcopy(meta, meta2, meta->used_len);
	return (meta2);
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
				mtx_enter(&ng_typelist_mtx, MTX_DEF);
				LIST_REMOVE(type, types);
				mtx_exit(&ng_typelist_mtx, MTX_DEF);
			}
		splx(s);
		break;

	case MOD_UNLOAD:
		s = splnet();
		if (type->refs != 0)		/* make sure no nodes exist! */
			error = EBUSY;
		else {
			if (type->mod_event != NULL) {	/* check with type */
				error = (*type->mod_event)(mod, event, data);
				if (error != 0) {	/* type refuses.. */
					splx(s);
					break;
				}
			}
			mtx_enter(&ng_typelist_mtx, MTX_DEF);
			LIST_REMOVE(type, types);
			mtx_exit(&ng_typelist_mtx, MTX_DEF);
		}
		splx(s);
		break;

	default:
		if (type->mod_event != NULL)
			error = (*type->mod_event)(mod, event, data);
		else
			error = 0;		/* XXX ? */
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
	int s, error = 0;

	switch (event) {
	case MOD_LOAD:
		/* Register line discipline */
		mtx_init(&ng_worklist_mtx, "netgraph worklist mutex", 0);
		mtx_init(&ng_typelist_mtx, "netgraph types mutex", 0);
		mtx_init(&ng_nodelist_mtx, "netgraph nodelist mutex", 0);
		mtx_init(&ng_idhash_mtx, "netgraph idhash mutex", 0);
		mtx_init(&ngq_mtx, "netgraph netisr mutex", 0);
		s = splimp();
		error = register_netisr(NETISR_NETGRAPH, ngintr);
		splx(s);
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
DECLARE_MODULE(netgraph, netgraph_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);

/************************************************************************
			Queue element get/free routines
************************************************************************/


static int			allocated;	/* number of items malloc'd */
static int			maxalloc = 128;	/* limit the damage of a leak */
static const int		ngqfreemax = 64;/* cache at most this many */
static const int		ngqfreelow = 4; /* try malloc if free < this */
static volatile int		ngqfreesize;	/* number of cached entries */
#ifdef	NETGRAPH_DEBUG
static TAILQ_HEAD(, ng_item) ng_itemlist = TAILQ_HEAD_INITIALIZER(ng_itemlist);
#endif
/*
 * Get a queue entry
 * This is usually called when a packet first enters netgraph.
 * By definition, this is usually from an interrupt, or from a user.
 * Users are not so important, but try be quick for the times that it's
 * an interrupt. Use atomic operations to cope with collisions
 * with interrupts and other processors. Assumes MALLOC is SMP safe.
 * XXX If reserve is low, we should try to get 2 from malloc as this
 * would indicate it often fails.
 */
static item_p
ng_getqblk(void)
{
	item_p item = NULL;

	/*
	 * Try get a cached queue block, or else allocate a new one
	 * If we are less than our reserve, try malloc. If malloc
	 * fails, then that's what the reserve is for...
	 * Don't completely trust ngqfreesize, as it is subject
	 * to races.. (it'll eventually catch up but may be out by one or two
	 * for brief moments(under SMP or interrupts).
	 * ngqfree is the final arbiter. We have our little reserve
	 * because we use M_NOWAIT for malloc. This just helps us
	 * avoid dropping packets while not increasing the time
	 * we take to service the interrupt (on average) (we hope).
	 */
	for (;;) {
		if ((ngqfreesize < ngqfreelow) || (ngqfree == NULL)) {
			if (allocated < maxalloc) {  /* don't leak forever */
				MALLOC(item, item_p ,
				    sizeof(*item), M_NETGRAPH_ITEM,
				    (M_NOWAIT | M_ZERO));
				if (item) {
#ifdef	NETGRAPH_DEBUG
					TAILQ_INSERT_TAIL(&ng_itemlist,
								item, all);
#endif	/* NETGRAPH_DEBUG */
					atomic_add_int(&allocated, 1);
					break;
				}
			}
		}

		/*
		 * We didn't or couldn't malloc.
		 * try get one from our cache.
		 * item must be NULL to get here.
		 */
		if ((item = ngqfree) != NULL) {
			/*
			 * Atomically try grab the first item
			 * and put it's successor in its place.
			 * If we fail, just try again.. someone else
			 * beat us to this one or freed one.
			 * Don't worry about races with ngqfreesize.
			 * Close enough is good enough..
			 */
			if (atomic_cmpset_ptr(&ngqfree, item, item->el_next)) {
				atomic_subtract_int(&ngqfreesize, 1);
				break;
			}
			item = NULL;
		} else {
			/* We really ran out */
			break;
		}
	}
	item->el_flags &= ~NGQF_FREE;
	return (item);
}

/*
 * Release a queue entry
 */
void
ng_free_item(item_p item)
{

	/*
	 * The item may hold resources on it's own. We need to free
	 * these before we can free the item. What they are depends upon
	 * what kind of item it is. it is important that nodes zero
	 * out pointers to resources that they remove from the item
	 * or we release them again here.
	 */
	if (item->el_flags & NGQF_FREE) {
		panic(" Freeing free queue item");
	}
	switch (item->el_flags & NGQF_D_M) {
	case NGQF_DATA:
		/* If we have an mbuf and metadata still attached.. */
		NG_FREE_M(_NGI_M(item));
		NG_FREE_META(_NGI_META(item));
		break;
	case NGQF_MESG:
		_NGI_RETADDR(item) = NULL;
		NG_FREE_MSG(_NGI_MSG(item));
		break;
	}
		/* If we still have a node or hook referenced... */
	if (item->el_dest) {
		NG_NODE_UNREF(item->el_dest);
		item->el_dest = NULL;
	}
	if (item->el_hook) {
		NG_HOOK_UNREF(item->el_hook);
		item->el_hook = NULL;
	}
	item->el_flags |= NGQF_FREE;

	/*
	 * We have freed any resources held by the item.
	 * now we can free the item itself.
	 */
	if (ngqfreesize < ngqfreemax) { /* don't worry about races */
		for (;;) {
			item->el_next = ngqfree;
			if (atomic_cmpset_ptr(&ngqfree, item->el_next, item)) {
				break;
			}
		}
		atomic_add_int(&ngqfreesize, 1);
	} else {
		/* This is the only place that should use this Macro */
#ifdef	NETGRAPH_DEBUG
		TAILQ_REMOVE(&ng_itemlist, item, all);
#endif	/* NETGRAPH_DEBUG */
		NG_FREE_ITEM_REAL(item);
		atomic_subtract_int(&allocated, 1);
	}
}

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
		ng_node2ID(node), node->nd_type->name,
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
	if (item->el_flags & NGQF_FREE) {
		printf(" Free item, freed at %s, line %d\n",
			item->lastfile, item->lastline);
	} else {
		printf(" ACTIVE item, last used at %s, line %d",
			item->lastfile, item->lastline);
		if ((item->el_flags & NGQF_D_M) == NGQF_MESG) {
			printf(" - retaddr[%d]:\n", _NGI_RETADDR(item));
		} else {
			printf(" - [data]\n");
		}
	}
	if (line) {
		printf(" problem discovered at file %s, line %d\n", file, line);
		if (item->el_dest) {
			printf("node %p ([%x])\n",
				item->el_dest, ng_node2ID(item->el_dest));
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
	SLIST_FOREACH(node, &ng_allnodes, nd_all) {
		printf("[%d] ", i++);
		dumpnode(node, NULL, 0);
	}
}

static void
ng_dumphooks(void)
{
	hook_p hook;
	int i = 1;
	SLIST_FOREACH(hook, &ng_allhooks, hk_all) {
		printf("[%d] ", i++);
		dumphook(hook, NULL, 0);
	}
}

static int
sysctl_debug_ng_dump_items(SYSCTL_HANDLER_ARGS)
{
	static int count;
	int error;
	int val;
	int i;

	val = allocated;
	i = 1;
	error = sysctl_handle_int(oidp, &val, sizeof(int), req);
	if(count++  & 1) { /* for some reason sysctl calls it twice */
		ng_dumpitems();
		ng_dumpnodes();
		ng_dumphooks();
	}
	return error;
}

SYSCTL_PROC(_debug, OID_AUTO, ng_dump_items, CTLTYPE_INT | CTLFLAG_RD,
    0, 0, sysctl_debug_ng_dump_items, "I", "Number of allocated items");
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
		mtx_enter(&ng_worklist_mtx, MTX_SPIN);
		node = TAILQ_FIRST(&ng_worklist);
		if (!node) {
			mtx_exit(&ng_worklist_mtx, MTX_SPIN);
			break;
		}
		TAILQ_REMOVE(&ng_worklist, node, nd_work);
		mtx_exit(&ng_worklist_mtx, MTX_SPIN);
		/*
		 * We have the node. We also take over the reference
		 * that the list had on it.
		 * Now process as much as you can, until it won't
		 * let you have another item off the queue.
		 * All this time, keep the reference
		 * that lets us be sure that the node still exists.
		 * Let the reference go at the last minute.
		 */
		for (;;) {
			mtx_enter(&node->nd_input_queue.q_mtx, MTX_SPIN);
			item = ng_dequeue(&node->nd_input_queue);
			if (item == NULL) {
				/*
				 * Say we are on the queue as long as
				 * we are processing it here.
				 * it probably wouldn't come here while we
				 * are processing anyhow.
				 */
				node->nd_flags &= ~NG_WORKQ;	
				mtx_exit(&node->nd_input_queue.q_mtx, MTX_SPIN);
				NG_NODE_UNREF(node);
				break; /* go look for another node */
			} else {
				mtx_exit(&node->nd_input_queue.q_mtx, MTX_SPIN);
#ifdef	NETGRAPH_DEBUG
        _ngi_check(item, __FILE__, __LINE__);
#endif
				ng_apply_item(node, item);
			}
		}
	}
}

static void
ng_worklist_remove(node_p node)
{
	mtx_enter(&ng_worklist_mtx, MTX_SPIN);
	if (node->nd_flags & NG_WORKQ) {
		TAILQ_REMOVE(&ng_worklist, node, nd_work);
		NG_NODE_UNREF(node);
	}
	node->nd_flags &= ~NG_WORKQ;
	mtx_exit(&ng_worklist_mtx, MTX_SPIN);
}

static void
ng_setisr(node_p node)
{
	mtx_enter(&ng_worklist_mtx, MTX_SPIN);
	if ((node->nd_flags & NG_WORKQ) == 0) {
		/*
		 * If we are not already on the work queue,
		 * then put us on.
		 */
		node->nd_flags |= NG_WORKQ;
		TAILQ_INSERT_TAIL(&ng_worklist, node, nd_work);
		NG_NODE_REF(node);
	}
	mtx_exit(&ng_worklist_mtx, MTX_SPIN);
	schednetisr(NETISR_NETGRAPH);
}


/***********************************************************************
* Externally useable functions to set up a queue item ready for sending
***********************************************************************/

#ifdef	NETGRAPH_DEBUG
#define	ITEM_DEBUG_CHECKS						\
	do {								\
		if (item->el_dest ) {					\
			printf("item already has node");		\
			Debugger("has node");				\
			NG_NODE_UNREF(item->el_dest);			\
			item->el_dest = NULL;				\
		}							\
		if (item->el_hook ) {					\
			printf("item already has hook");		\
			Debugger("has hook");				\
			NG_HOOK_UNREF(item->el_hook);			\
			item->el_hook = NULL;				\
		}							\
	} while (0)
#else
#define ITEM_DEBUG_CHECKS
#endif

/*
 * Put elements into the item.
 * Hook and node references will be removed when the item is dequeued.
 * (or equivalent)
 * (XXX) Unsafe because no reference held by peer on remote node.
 * remote node might go away in this timescale.
 * We know the hooks can't go away because that would require getting
 * a writer item on both nodes and we must have at least a  reader
 * here to eb able to do this.
 * Note that the hook loaded is the REMOTE hook.
 *
 * This is possibly in the critical path for new data.
 */
item_p
ng_package_data(struct mbuf *m, meta_p meta)
{
	item_p item;

	if ((item = ng_getqblk()) == NULL) {
		NG_FREE_M(m);
		NG_FREE_META(meta);
		return (NULL);
	}
	ITEM_DEBUG_CHECKS;
	item->el_flags = NGQF_DATA;
	item->el_next = NULL;
	NGI_M(item) = m;
	NGI_META(item) = meta;
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
ng_package_msg(struct ng_mesg *msg)
{
	item_p item;

	if ((item = ng_getqblk()) == NULL) {
		if ((msg->header.flags & NGF_STATIC) == 0) {
			NG_FREE_MSG(msg);
		}
		return (NULL);
	}
	ITEM_DEBUG_CHECKS;
	item->el_flags = NGQF_MESG;
	item->el_next = NULL;
	/*
	 * Set the current lasthook into the queue item
	 */
	NGI_MSG(item) = msg;
	NGI_RETADDR(item) = NULL;
	return (item);
}



#define SET_RETADDR							\
	do {	/* Data items don't have retaddrs */			\
		if ((item->el_flags & NGQF_D_M) == NGQF_MESG) {		\
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
		TRAP_ERROR
		return (EINVAL);
	}

	/*
	 * Transfer our interest to the other (peer) end.
	 */
	item->el_hook = NG_HOOK_PEER(hook);
	NG_HOOK_REF(item->el_hook); /* Don't let it go while on the queue */
	item->el_dest = NG_PEER_NODE(hook);
	NG_NODE_REF(item->el_dest); /* Nor this */
	SET_RETADDR;
	return (0);
}

int
ng_address_path(node_p here, item_p item, char *address, ng_ID_t retaddr) 
{
	node_p  dest = NULL;
	hook_p	hook = NULL;
	int     error;

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
	item->el_dest = dest;
	if (( item->el_hook = hook))
		NG_HOOK_REF(hook);	/* don't let it go while on the queue */
	SET_RETADDR;
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
		TRAP_ERROR
		return(EINVAL);
	}
	/* Fill out the contents */
	item->el_flags = NGQF_MESG;
	item->el_next = NULL;
	item->el_dest = dest;
	item->el_hook = NULL;
	SET_RETADDR;
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
	if ((item = ng_getqblk()) == NULL) {
		if ((msg->header.flags & NGF_STATIC) == 0) {
			NG_FREE_MSG(msg);
		}
		return (NULL);
	}

	/* Fill out the contents */
	item->el_flags = NGQF_MESG;
	item->el_next = NULL;
	item->el_dest = here;
	NG_NODE_REF(here);
	item->el_hook = hook;
	if (hook)
		NG_HOOK_REF(hook);
	NGI_MSG(item) = msg;
	NGI_RETADDR(item) = ng_node2ID(here);
	return (item);
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
	meta_p meta;
	struct ng_mesg *msg;
	ng_ID_t retaddr;
	int	error;

	NGI_GET_M(item, m);
	NGI_GET_META(item, meta);
	NGI_GET_MSG(item, msg);
	retaddr = NGI_RETADDR(item);
	NG_SEND_DATA(error, hook, m, meta);
	NG_SEND_DATA_ONLY(error, hook, m);
	NG_FWD_NEW_DATA(error, item, hook, m);
	NG_FWD_ITEM_HOOK(error, item, hook);
	NG_SEND_MSG_HOOK(error, node, msg, hook, retaddr);
	NG_SEND_MSG_ID(error, node, msg, retaddr, retaddr);
	NG_SEND_MSG_PATH(error, node, msg, ".:", retaddr);
	NG_QUEUE_MSG(error, node, msg, ".:", retaddr);
	NG_FWD_MSG_HOOK(error, node, item, hook, retaddr);
}
#endif /* TESTING */

