
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
#include <sys/linker.h>
#include <sys/queue.h>
#include <sys/mbuf.h>
#include <sys/ctype.h>
#include <machine/limits.h>

#include <net/netisr.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>

/* List of all nodes */
static LIST_HEAD(, ng_node) nodelist;

/* List of installed types */
static LIST_HEAD(, ng_type) typelist;

/* Hash releted definitions */
#define ID_HASH_SIZE 32 /* most systems wont need even this many */
static LIST_HEAD(, ng_node) ID_hash[ID_HASH_SIZE];
/* Don't nead to initialise them because it's a LIST */

/* Internal functions */
static int	ng_add_hook(node_p node, const char *name, hook_p * hookp);
static int	ng_connect(hook_p hook1, hook_p hook2);
static void	ng_disconnect_hook(hook_p hook);
static int	ng_generic_msg(node_p here, struct ng_mesg *msg,
			const char *retaddr, struct ng_mesg ** resp);
static ng_ID_t	ng_decodeidname(const char *name);
static int	ngb_mod_event(module_t mod, int event, void *data);
static void	ngintr(void);

/* Our own netgraph malloc type */
MALLOC_DEFINE(M_NETGRAPH, "netgraph", "netgraph structures and ctrl messages");

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

	/* Check that the type makes sense */
	if (typename == NULL) {
		TRAP_ERROR;
		return (EINVAL);
	}

	/* Locate the node type */
	if ((type = ng_findtype(typename)) == NULL) {
		char *path, filename[NG_TYPELEN + 4];
		linker_file_t lf;
		int error;

		/* Not found, try to load it as a loadable module */
		snprintf(filename, sizeof(filename), "ng_%s.ko", typename);
		if ((path = linker_search_path(filename)) == NULL)
			return (ENXIO);
		error = linker_load_file(path, &lf);
		FREE(path, M_LINKER);
		if (error != 0)
			return (error);
		lf->userrefs++;		/* pretend loaded by the syscall */

		/* Try again, as now the type should have linked itself in */
		if ((type = ng_findtype(typename)) == NULL)
			return (ENXIO);
	}

	/* Call the constructor */
	if (type->constructor != NULL)
		return ((*type->constructor)(nodepp));
	else
		return (ng_make_node_common(type, nodepp));
}

/*
 * Generic node creation. Called by node constructors.
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
	MALLOC(node, node_p, sizeof(*node), M_NETGRAPH, M_NOWAIT);
	if (node == NULL) {
		TRAP_ERROR;
		return (ENOMEM);
	}
	bzero(node, sizeof(*node));
	node->type = type;
	node->refs++;				/* note reference */
	type->refs++;

	/* Link us into the node linked list */
	LIST_INSERT_HEAD(&nodelist, node, nodes);

	/* Initialize hook list for new node */
	LIST_INIT(&node->hooks);

	/* get an ID and put us in the hash chain */
	node->ID = nextID++; /* 137 per second for 1 year before wrap */
	LIST_INSERT_HEAD(&ID_hash[node->ID % ID_HASH_SIZE], node, idnodes);

	/* Done */
	*nodepp = node;
	return (0);
}

/*
 * Forceably start the shutdown process on a node. Either call
 * it's shutdown method, or do the default shutdown if there is
 * no type-specific method.
 *
 * Persistent nodes must have a type-specific method which
 * resets the NG_INVALID flag.
 */
void
ng_rmnode(node_p node)
{
	/* Check if it's already shutting down */
	if ((node->flags & NG_INVALID) != 0)
		return;

	/* Add an extra reference so it doesn't go away during this */
	node->refs++;

	/* Mark it invalid so any newcomers know not to try use it */
	node->flags |= NG_INVALID;

	/* Ask the type if it has anything to do in this case */
	if (node->type && node->type->shutdown)
		(*node->type->shutdown)(node);
	else {				/* do the default thing */
		ng_unname(node);
		ng_cutlinks(node);
		ng_unref(node);
	}

	/* Remove extra reference, possibly the last */
	ng_unref(node);
}

/*
 * Called by the destructor to remove any STANDARD external references
 */
void
ng_cutlinks(node_p node)
{
	hook_p  hook;

	/* Make sure that this is set to stop infinite loops */
	node->flags |= NG_INVALID;

	/* If we have sleepers, wake them up; they'll see NG_INVALID */
	if (node->sleepers)
		wakeup(node);

	/* Notify all remaining connected nodes to disconnect */
	while ((hook = LIST_FIRST(&node->hooks)) != NULL)
		ng_destroy_hook(hook);
}

/*
 * Remove a reference to the node, possibly the last
 */
void
ng_unref(node_p node)
{
	if (--node->refs <= 0) {
		node->type->refs--;
		LIST_REMOVE(node, nodes);
		LIST_REMOVE(node, idnodes);
		FREE(node, M_NETGRAPH);
	}
}

/*
 * Wait for a node to come ready. Returns a node with a reference count;
 * don't forget to drop it when we are done with it using ng_release_node().
 */
int
ng_wait_node(node_p node, char *msg)
{
	int s, error = 0;

	if (msg == NULL)
		msg = "netgraph";
	s = splnet();
	node->sleepers++;
	node->refs++;		/* the sleeping process counts as a reference */
	while ((node->flags & (NG_BUSY | NG_INVALID)) == NG_BUSY)
		error = tsleep(node, (PZERO + 1) | PCATCH, msg, 0);
	node->sleepers--;
	if (node->flags & NG_INVALID) {
		TRAP_ERROR;
		error = ENXIO;
	} else {
		KASSERT(node->refs > 1,
		    ("%s: refs=%d", __FUNCTION__, node->refs));
		node->flags |= NG_BUSY;
	}
	splx(s);

	/* Release the reference we had on it */
	if (error != 0)
		ng_unref(node);
	return error;
}

/*
 * Release a node acquired via ng_wait_node()
 */
void
ng_release_node(node_p node)
{
	/* Declare that we don't want it */
	node->flags &= ~NG_BUSY;

	/* If we have sleepers, then wake them up */
	if (node->sleepers)
		wakeup(node);

	/* We also have a reference.. drop it too */
	ng_unref(node);
}

/************************************************************************
			Node ID handling
************************************************************************/
static node_p
ng_ID2node(ng_ID_t ID)
{
	node_p np;
	LIST_FOREACH(np, &ID_hash[ID % ID_HASH_SIZE], idnodes) {
		if (np->ID == ID)
			break;
	}
	return(np);
}

ng_ID_t
ng_node2ID(node_p node)
{
	return (node->ID);
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

	/* Check the node isn't already named */
	if (node->name != NULL) {
		TRAP_ERROR;
		return (EISCONN);
	}

	/* Check the name isn't already being used */
	if (ng_findname(node, name) != NULL) {
		TRAP_ERROR;
		return (EADDRINUSE);
	}

	/* Allocate space and copy it */
	MALLOC(node->name, char *, strlen(name) + 1, M_NETGRAPH, M_NOWAIT);
	if (node->name == NULL) {
		TRAP_ERROR;
		return (ENOMEM);
	}
	strcpy(node->name, name);

	/* The name counts as a reference */
	node->refs++;
	return (0);
}

/*
 * Find a node by absolute name. The name should NOT end with ':'
 * The name "." means "this node" and "[xxx]" means "the node
 * with ID (ie, at address) xxx".
 *
 * Returns the node if found, else NULL.
 */
node_p
ng_findname(node_p this, const char *name)
{
	node_p node;
	ng_ID_t temp;

	/* "." means "this node" */
	if (strcmp(name, ".") == 0)
		return(this);

	/* Check for name-by-ID */
	if ((temp = ng_decodeidname(name)) != 0) {
		return (ng_ID2node(temp));
	}

	/* Find node by name */
	LIST_FOREACH(node, &nodelist, nodes) {
		if (node->name != NULL && strcmp(node->name, name) == 0)
			break;
	}
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
	if (len < 3 || name[0] != '[' || name[len - 1] != ']'
	    || !isxdigit(name[1]))
		return (0);

	/* Decode number */
	val = strtoul(name + 1, &eptr, 16);
	if (eptr - name != len - 1 || val == ULONG_MAX || val == 0)
		return ((ng_ID_t)0);
	return (ng_ID_t)val;
}

/*
 * Remove a name from a node. This should only be called
 * when shutting down and removing the node.
 */
void
ng_unname(node_p node)
{
	if (node->name) {
		FREE(node->name, M_NETGRAPH);
		node->name = NULL;
		ng_unref(node);
	}
}

/************************************************************************
			Hook routines

 Names are not optional. Hooks are always connected, except for a
 brief moment within these routines.

************************************************************************/

/*
 * Remove a hook reference
 */
static void
ng_unref_hook(hook_p hook)
{
	if (--hook->refs == 0)
		FREE(hook, M_NETGRAPH);
}

/*
 * Add an unconnected hook to a node. Only used internally.
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
	MALLOC(hook, hook_p, sizeof(*hook), M_NETGRAPH, M_NOWAIT);
	if (hook == NULL) {
		TRAP_ERROR;
		return (ENOMEM);
	}
	bzero(hook, sizeof(*hook));
	hook->refs = 1;
	hook->flags = HK_INVALID;
	hook->node = node;
	node->refs++;		/* each hook counts as a reference */

	/* Check if the node type code has something to say about it */
	if (node->type->newhook != NULL)
		if ((error = (*node->type->newhook)(node, hook, name)) != 0)
			goto fail;

	/*
	 * The 'type' agrees so far, so go ahead and link it in.
	 * We'll ask again later when we actually connect the hooks.
	 */
	LIST_INSERT_HEAD(&node->hooks, hook, hooks);
	node->numhooks++;

	/* Set hook name */
	MALLOC(hook->name, char *, strlen(name) + 1, M_NETGRAPH, M_NOWAIT);
	if (hook->name == NULL) {
		error = ENOMEM;
		LIST_REMOVE(hook, hooks);
		node->numhooks--;
fail:
		hook->node = NULL;
		ng_unref(node);
		ng_unref_hook(hook);	/* this frees the hook */
		return (error);
	}
	strcpy(hook->name, name);
	if (hookp)
		*hookp = hook;
	return (error);
}

/*
 * Connect a pair of hooks. Only used internally.
 */
static int
ng_connect(hook_p hook1, hook_p hook2)
{
	int     error;

	hook1->peer = hook2;
	hook2->peer = hook1;

	/* Give each node the opportunity to veto the impending connection */
	if (hook1->node->type->connect) {
		if ((error = (*hook1->node->type->connect) (hook1))) {
			ng_destroy_hook(hook1);	/* also zaps hook2 */
			return (error);
		}
	}
	if (hook2->node->type->connect) {
		if ((error = (*hook2->node->type->connect) (hook2))) {
			ng_destroy_hook(hook2);	/* also zaps hook1 */
			return (error);
		}
	}
	hook1->flags &= ~HK_INVALID;
	hook2->flags &= ~HK_INVALID;
	return (0);
}

/*
 * Find a hook
 *
 * Node types may supply their own optimized routines for finding
 * hooks.  If none is supplied, we just do a linear search.
 */
hook_p
ng_findhook(node_p node, const char *name)
{
	hook_p hook;

	if (node->type->findhook != NULL)
		return (*node->type->findhook)(node, name);
	LIST_FOREACH(hook, &node->hooks, hooks) {
		if (hook->name != NULL && strcmp(hook->name, name) == 0)
			return (hook);
	}
	return (NULL);
}

/*
 * Destroy a hook
 *
 * As hooks are always attached, this really destroys two hooks.
 * The one given, and the one attached to it. Disconnect the hooks
 * from each other first.
 */
void
ng_destroy_hook(hook_p hook)
{
	hook_p peer = hook->peer;

	hook->flags |= HK_INVALID;		/* as soon as possible */
	if (peer) {
		peer->flags |= HK_INVALID;	/* as soon as possible */
		hook->peer = NULL;
		peer->peer = NULL;
		ng_disconnect_hook(peer);
	}
	ng_disconnect_hook(hook);
}

/*
 * Notify the node of the hook's demise. This may result in more actions
 * (e.g. shutdown) but we don't do that ourselves and don't know what
 * happens there. If there is no appropriate handler, then just remove it
 * (and decrement the reference count of it's node which in turn might
 * make something happen).
 */
static void
ng_disconnect_hook(hook_p hook)
{
	node_p node = hook->node;

	/*
	 * Remove the hook from the node's list to avoid possible recursion
	 * in case the disconnection results in node shutdown.
	 */
	LIST_REMOVE(hook, hooks);
	node->numhooks--;
	if (node->type->disconnect) {
		/*
		 * The type handler may elect to destroy the peer so don't
		 * trust its existance after this point.
		 */
		(*node->type->disconnect) (hook);
	}
	ng_unref(node);		/* might be the last reference */
	if (hook->name)
		FREE(hook->name, M_NETGRAPH);
	hook->node = NULL;	/* may still be referenced elsewhere */
	ng_unref_hook(hook);
}

/*
 * Take two hooks on a node and merge the connection so that the given node
 * is effectively bypassed.
 */
int
ng_bypass(hook_p hook1, hook_p hook2)
{
	if (hook1->node != hook2->node)
		return (EINVAL);
	hook1->peer->peer = hook2->peer;
	hook2->peer->peer = hook1->peer;

	/* XXX If we ever cache methods on hooks update them as well */
	hook1->peer = NULL;
	hook2->peer = NULL;
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
	if (tp->version != NG_VERSION || namelen == 0 || namelen > NG_TYPELEN) {
		TRAP_ERROR;
		return (EINVAL);
	}

	/* Check for name collision */
	if (ng_findtype(tp->name) != NULL) {
		TRAP_ERROR;
		return (EEXIST);
	}

	/* Link in new type */
	LIST_INSERT_HEAD(&typelist, tp, types);
	tp->refs = 0;
	return (0);
}

/*
 * Look for a type of the name given
 */
struct ng_type *
ng_findtype(const char *typename)
{
	struct ng_type *type;

	LIST_FOREACH(type, &typelist, types) {
		if (strcmp(type->name, typename) == 0)
			break;
	}
	return (type);
}


/************************************************************************
			Composite routines
************************************************************************/

/*
 * Make a peer and connect. The order is arranged to minimise
 * the work needed to back out in case of error.
 */
int
ng_mkpeer(node_p node, const char *name, const char *name2, char *type)
{
	node_p  node2;
	hook_p  hook;
	hook_p  hook2;
	int     error;

	if ((error = ng_add_hook(node, name, &hook)))
		return (error);
	if ((error = ng_make_node(type, &node2))) {
		ng_destroy_hook(hook);
		return (error);
	}
	if ((error = ng_add_hook(node2, name2, &hook2))) {
		ng_rmnode(node2);
		ng_destroy_hook(hook);
		return (error);
	}

	/*
	 * Actually link the two hooks together.. on failure they are
	 * destroyed so we don't have to do that here.
	 */
	if ((error = ng_connect(hook, hook2)))
		ng_rmnode(node2);
	return (error);
}

/*
 * Connect two nodes using the specified hooks
 */
int
ng_con_nodes(node_p node, const char *name, node_p node2, const char *name2)
{
	int     error;
	hook_p  hook;
	hook_p  hook2;

	if ((error = ng_add_hook(node, name, &hook)))
		return (error);
	if ((error = ng_add_hook(node2, name2, &hook2))) {
		ng_destroy_hook(hook);
		return (error);
	}
	return (ng_connect(hook, hook2));
}

/*
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
 */

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
 * return the destination node. Compute the "return address" if desired.
 */
int
ng_path2node(node_p here, const char *address, node_p *destp, char **rtnp)
{
	const	node_p start = here;
	char    fullpath[NG_PATHLEN + 1];
	char   *nodename, *path, pbuf[2];
	node_p  node;
	char   *cp;

	/* Initialize */
	if (rtnp)
		*rtnp = NULL;
	if (destp == NULL)
		return EINVAL;
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

	/* For an absolute address, jump to the starting node */
	if (nodename) {
		node = ng_findname(here, nodename);
		if (node == NULL) {
			TRAP_ERROR;
			return (ENOENT);
		}
	} else
		node = here;

	/* Now follow the sequence of hooks */
	for (cp = path; node != NULL && *cp != '\0'; ) {
		hook_p hook;
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
		    || hook->peer == NULL
		    || (hook->flags & HK_INVALID) != 0) {
			TRAP_ERROR;
			return (ENOENT);
		}

		/* Hop on over to the next node */
		node = hook->peer->node;
	}

	/* If node somehow missing, fail here (probably this is not needed) */
	if (node == NULL) {
		TRAP_ERROR;
		return (ENXIO);
	}

	/* Now compute return address, i.e., the path to the sender */
	if (rtnp != NULL) {
		MALLOC(*rtnp, char *, NG_NODELEN + 2, M_NETGRAPH, M_NOWAIT);
		if (*rtnp == NULL) {
			TRAP_ERROR;
			return (ENOMEM);
		}
		if (start->name != NULL)
			sprintf(*rtnp, "%s:", start->name);
		else
			sprintf(*rtnp, "[%x]:", ng_node2ID(start));
	}

	/* Done */
	*destp = node;
	return (0);
}

/*
 * Call the appropriate message handler for the object.
 * It is up to the message handler to free the message.
 * If it's a generic message, handle it generically, otherwise
 * call the type's message handler (if it exists)
 */

#define CALL_MSG_HANDLER(error, node, msg, retaddr, resp)		\
do {									\
	if((msg)->header.typecookie == NGM_GENERIC_COOKIE) {		\
		(error) = ng_generic_msg((node), (msg),			\
				(retaddr), (resp));			\
	} else {							\
		if ((node)->type->rcvmsg != NULL) {			\
			(error) = (*(node)->type->rcvmsg)((node),	\
					(msg), (retaddr), (resp));	\
		} else {						\
			TRAP_ERROR;					\
			FREE((msg), M_NETGRAPH);			\
			(error) = EINVAL;				\
		}							\
	}								\
} while (0)


/*
 * Send a control message to a node
 */
int
ng_send_msg(node_p here, struct ng_mesg *msg, const char *address,
	    struct ng_mesg **rptr)
{
	node_p  dest = NULL;
	char   *retaddr = NULL;
	int     error;

	/* Find the target node */
	error = ng_path2node(here, address, &dest, &retaddr);
	if (error) {
		FREE(msg, M_NETGRAPH);
		return error;
	}

	/* Make sure the resp field is null before we start */
	if (rptr != NULL)
		*rptr = NULL;

	CALL_MSG_HANDLER(error, dest, msg, retaddr, rptr);

	/* Make sure that if there is a response, it has the RESP bit set */
	if ((error == 0) && rptr && *rptr)
		(*rptr)->header.flags |= NGF_RESP;

	/*
	 * If we had a return address it is up to us to free it. They should
	 * have taken a copy if they needed to make a delayed response.
	 */
	if (retaddr)
		FREE(retaddr, M_NETGRAPH);
	return (error);
}

/*
 * Implement the 'generic' control messages
 */
static int
ng_generic_msg(node_p here, struct ng_mesg *msg, const char *retaddr,
	       struct ng_mesg **resp)
{
	int error = 0;

	if (msg->header.typecookie != NGM_GENERIC_COOKIE) {
		TRAP_ERROR;
		FREE(msg, M_NETGRAPH);
		return (EINVAL);
	}
	switch (msg->header.cmd) {
	case NGM_SHUTDOWN:
		ng_rmnode(here);
		break;
	case NGM_MKPEER:
	    {
		struct ngm_mkpeer *const mkp = (struct ngm_mkpeer *) msg->data;

		if (msg->header.arglen != sizeof(*mkp)) {
			TRAP_ERROR;
			return (EINVAL);
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
			TRAP_ERROR;
			return (EINVAL);
		}
		con->path[sizeof(con->path) - 1] = '\0';
		con->ourhook[sizeof(con->ourhook) - 1] = '\0';
		con->peerhook[sizeof(con->peerhook) - 1] = '\0';
		error = ng_path2node(here, con->path, &node2, NULL);
		if (error)
			break;
		error = ng_con_nodes(here, con->ourhook, node2, con->peerhook);
		break;
	    }
	case NGM_NAME:
	    {
		struct ngm_name *const nam = (struct ngm_name *) msg->data;

		if (msg->header.arglen != sizeof(*nam)) {
			TRAP_ERROR;
			return (EINVAL);
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
			TRAP_ERROR;
			return (EINVAL);
		}
		rmh->ourhook[sizeof(rmh->ourhook) - 1] = '\0';
		if ((hook = ng_findhook(here, rmh->ourhook)) != NULL)
			ng_destroy_hook(hook);
		break;
	    }
	case NGM_NODEINFO:
	    {
		struct nodeinfo *ni;
		struct ng_mesg *rp;

		/* Get response struct */
		if (resp == NULL) {
			error = EINVAL;
			break;
		}
		NG_MKRESPONSE(rp, msg, sizeof(*ni), M_NOWAIT);
		if (rp == NULL) {
			error = ENOMEM;
			break;
		}

		/* Fill in node info */
		ni = (struct nodeinfo *) rp->data;
		if (here->name != NULL)
			strncpy(ni->name, here->name, NG_NODELEN);
		strncpy(ni->type, here->type->name, NG_TYPELEN);
		ni->id = ng_node2ID(here);
		ni->hooks = here->numhooks;
		*resp = rp;
		break;
	    }
	case NGM_LISTHOOKS:
	    {
		const int nhooks = here->numhooks;
		struct hooklist *hl;
		struct nodeinfo *ni;
		struct ng_mesg *rp;
		hook_p hook;

		/* Get response struct */
		if (resp == NULL) {
			error = EINVAL;
			break;
		}
		NG_MKRESPONSE(rp, msg, sizeof(*hl)
		    + (nhooks * sizeof(struct linkinfo)), M_NOWAIT);
		if (rp == NULL) {
			error = ENOMEM;
			break;
		}
		hl = (struct hooklist *) rp->data;
		ni = &hl->nodeinfo;

		/* Fill in node info */
		if (here->name)
			strncpy(ni->name, here->name, NG_NODELEN);
		strncpy(ni->type, here->type->name, NG_TYPELEN);
		ni->id = ng_node2ID(here);

		/* Cycle through the linked list of hooks */
		ni->hooks = 0;
		LIST_FOREACH(hook, &here->hooks, hooks) {
			struct linkinfo *const link = &hl->link[ni->hooks];

			if (ni->hooks >= nhooks) {
				log(LOG_ERR, "%s: number of %s changed\n",
				    __FUNCTION__, "hooks");
				break;
			}
			if ((hook->flags & HK_INVALID) != 0)
				continue;
			strncpy(link->ourhook, hook->name, NG_HOOKLEN);
			strncpy(link->peerhook, hook->peer->name, NG_HOOKLEN);
			if (hook->peer->node->name != NULL)
				strncpy(link->nodeinfo.name,
				    hook->peer->node->name, NG_NODELEN);
			strncpy(link->nodeinfo.type,
			   hook->peer->node->type->name, NG_TYPELEN);
			link->nodeinfo.id = ng_node2ID(hook->peer->node);
			link->nodeinfo.hooks = hook->peer->node->numhooks;
			ni->hooks++;
		}
		*resp = rp;
		break;
	    }

	case NGM_LISTNAMES:
	case NGM_LISTNODES:
	    {
		const int unnamed = (msg->header.cmd == NGM_LISTNODES);
		struct namelist *nl;
		struct ng_mesg *rp;
		node_p node;
		int num = 0;

		if (resp == NULL) {
			error = EINVAL;
			break;
		}

		/* Count number of nodes */
		LIST_FOREACH(node, &nodelist, nodes) {
			if (unnamed || node->name != NULL)
				num++;
		}

		/* Get response struct */
		if (resp == NULL) {
			error = EINVAL;
			break;
		}
		NG_MKRESPONSE(rp, msg, sizeof(*nl)
		    + (num * sizeof(struct nodeinfo)), M_NOWAIT);
		if (rp == NULL) {
			error = ENOMEM;
			break;
		}
		nl = (struct namelist *) rp->data;

		/* Cycle through the linked list of nodes */
		nl->numnames = 0;
		LIST_FOREACH(node, &nodelist, nodes) {
			struct nodeinfo *const np = &nl->nodeinfo[nl->numnames];

			if (nl->numnames >= num) {
				log(LOG_ERR, "%s: number of %s changed\n",
				    __FUNCTION__, "nodes");
				break;
			}
			if ((node->flags & NG_INVALID) != 0)
				continue;
			if (!unnamed && node->name == NULL)
				continue;
			if (node->name != NULL)
				strncpy(np->name, node->name, NG_NODELEN);
			strncpy(np->type, node->type->name, NG_TYPELEN);
			np->id = ng_node2ID(node);
			np->hooks = node->numhooks;
			nl->numnames++;
		}
		*resp = rp;
		break;
	    }

	case NGM_LISTTYPES:
	    {
		struct typelist *tl;
		struct ng_mesg *rp;
		struct ng_type *type;
		int num = 0;

		if (resp == NULL) {
			error = EINVAL;
			break;
		}

		/* Count number of types */
		LIST_FOREACH(type, &typelist, types)
			num++;

		/* Get response struct */
		if (resp == NULL) {
			error = EINVAL;
			break;
		}
		NG_MKRESPONSE(rp, msg, sizeof(*tl)
		    + (num * sizeof(struct typeinfo)), M_NOWAIT);
		if (rp == NULL) {
			error = ENOMEM;
			break;
		}
		tl = (struct typelist *) rp->data;

		/* Cycle through the linked list of types */
		tl->numtypes = 0;
		LIST_FOREACH(type, &typelist, types) {
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
		*resp = rp;
		break;
	    }

	case NGM_BINARY2ASCII:
	    {
		int bufSize = 20 * 1024;	/* XXX hard coded constant */
		const struct ng_parse_type *argstype;
		const struct ng_cmdlist *c;
		struct ng_mesg *rp, *binary, *ascii;

		/* Data area must contain a valid netgraph message */
		binary = (struct ng_mesg *)msg->data;
		if (msg->header.arglen < sizeof(struct ng_mesg)
		    || msg->header.arglen - sizeof(struct ng_mesg) 
		      < binary->header.arglen) {
			error = EINVAL;
			break;
		}

		/* Get a response message with lots of room */
		NG_MKRESPONSE(rp, msg, sizeof(*ascii) + bufSize, M_NOWAIT);
		if (rp == NULL) {
			error = ENOMEM;
			break;
		}
		ascii = (struct ng_mesg *)rp->data;

		/* Copy binary message header to response message payload */
		bcopy(binary, ascii, sizeof(*binary));

		/* Find command by matching typecookie and command number */
		for (c = here->type->cmdlist;
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
				FREE(rp, M_NETGRAPH);
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
		if (argstype == NULL)
			*ascii->data = '\0';
		else {
			if ((error = ng_unparse(argstype,
			    (u_char *)binary->data,
			    ascii->data, bufSize)) != 0) {
				FREE(rp, M_NETGRAPH);
				break;
			}
		}

		/* Return the result as struct ng_mesg plus ASCII string */
		bufSize = strlen(ascii->data) + 1;
		ascii->header.arglen = bufSize;
		rp->header.arglen = sizeof(*ascii) + bufSize;
		*resp = rp;
		break;
	    }

	case NGM_ASCII2BINARY:
	    {
		int bufSize = 2000;	/* XXX hard coded constant */
		const struct ng_cmdlist *c;
		const struct ng_parse_type *argstype;
		struct ng_mesg *rp, *ascii, *binary;
		int off = 0;

		/* Data area must contain at least a struct ng_mesg + '\0' */
		ascii = (struct ng_mesg *)msg->data;
		if (msg->header.arglen < sizeof(*ascii) + 1
		    || ascii->header.arglen < 1
		    || msg->header.arglen
		      < sizeof(*ascii) + ascii->header.arglen) {
			error = EINVAL;
			break;
		}
		ascii->data[ascii->header.arglen - 1] = '\0';

		/* Get a response message with lots of room */
		NG_MKRESPONSE(rp, msg, sizeof(*binary) + bufSize, M_NOWAIT);
		if (rp == NULL) {
			error = ENOMEM;
			break;
		}
		binary = (struct ng_mesg *)rp->data;

		/* Copy ASCII message header to response message payload */
		bcopy(ascii, binary, sizeof(*ascii));

		/* Find command by matching ASCII command string */
		for (c = here->type->cmdlist;
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
				FREE(rp, M_NETGRAPH);
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
		if (argstype == NULL)
			bufSize = 0;
		else {
			if ((error = ng_parse(argstype, ascii->data,
			    &off, (u_char *)binary->data, &bufSize)) != 0) {
				FREE(rp, M_NETGRAPH);
				break;
			}
		}

		/* Return the result */
		binary->header.arglen = bufSize;
		rp->header.arglen = sizeof(*binary) + bufSize;
		*resp = rp;
		break;
	    }

	case NGM_TEXT_STATUS:
		/*
		 * This one is tricky as it passes the command down to the
		 * actual node, even though it is a generic type command.
		 * This means we must assume that the msg is already freed
		 * when control passes back to us.
		 */
		if (resp == NULL) {
			error = EINVAL;
			break;
		}
		if (here->type->rcvmsg != NULL)
			return((*here->type->rcvmsg)(here, msg, retaddr, resp));
		/* Fall through if rcvmsg not supported */
	default:
		TRAP_ERROR;
		error = EINVAL;
	}
	FREE(msg, M_NETGRAPH);
	return (error);
}

/*
 * Send a data packet to a node. If the recipient has no
 * 'receive data' method, then silently discard the packet.
 */
int 
ng_send_data(hook_p hook, struct mbuf *m, meta_p meta)
{
	int (*rcvdata)(hook_p, struct mbuf *, meta_p);
	int error;

	CHECK_DATA_MBUF(m);
	if (hook && (hook->flags & HK_INVALID) == 0) {
		rcvdata = hook->peer->node->type->rcvdata;
		if (rcvdata != NULL)
			error = (*rcvdata)(hook->peer, m, meta);
		else {
			error = 0;
			NG_FREE_DATA(m, meta);
		}
	} else {
		TRAP_ERROR;
		error = ENOTCONN;
		NG_FREE_DATA(m, meta);
	}
	return (error);
}

/*
 * Send a queued data packet to a node. If the recipient has no
 * 'receive queued data' method, then try the 'receive data' method above.
 */
int 
ng_send_dataq(hook_p hook, struct mbuf *m, meta_p meta)
{
	int (*rcvdataq)(hook_p, struct mbuf *, meta_p);
	int error;

	CHECK_DATA_MBUF(m);
	if (hook && (hook->flags & HK_INVALID) == 0) {
		rcvdataq = hook->peer->node->type->rcvdataq;
		if (rcvdataq != NULL)
			error = (*rcvdataq)(hook->peer, m, meta);
		else {
			error = ng_send_data(hook, m, meta);
		}
	} else {
		TRAP_ERROR;
		error = ENOTCONN;
		NG_FREE_DATA(m, meta);
	}
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
	MALLOC(meta2, meta_p, meta->used_len, M_NETGRAPH, M_NOWAIT);
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
			if ((error = (*type->mod_event)(mod, event, data)) != 0)
				LIST_REMOVE(type, types);
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
			LIST_REMOVE(type, types);
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
			Queueing routines
************************************************************************/

/* The structure for queueing across ISR switches */
struct ng_queue_entry {
	u_long	flags;
	struct ng_queue_entry *next;
	union {
		struct {
			hook_p		da_hook;	/*  target hook */
			struct mbuf	*da_m;
			meta_p		da_meta;
		} data;
		struct {
			struct ng_mesg	*msg_msg;
			node_p		msg_node;
			void		*msg_retaddr;
		} msg;
	} body;
};
#define NGQF_DATA	0x01		/* the queue element is data */
#define NGQF_MESG	0x02		/* the queue element is a message */

static struct ng_queue_entry   *ngqbase;	/* items to be unqueued */
static struct ng_queue_entry   *ngqlast;	/* last item queued */
static const int		ngqroom = 64;	/* max items to queue */
static int			ngqsize;	/* number of items in queue */

static struct ng_queue_entry   *ngqfree;	/* free ones */
static const int		ngqfreemax = 16;/* cache at most this many */
static int			ngqfreesize;	/* number of cached entries */

/*
 * Get a queue entry
 */
static struct ng_queue_entry *
ng_getqblk(void)
{
	register struct ng_queue_entry *q;
	int s;

	/* Could be guarding against tty ints or whatever */
	s = splhigh();

	/* Try get a cached queue block, or else allocate a new one */
	if ((q = ngqfree) == NULL) {
		splx(s);
		if (ngqsize < ngqroom) {	/* don't worry about races */
			MALLOC(q, struct ng_queue_entry *,
			    sizeof(*q), M_NETGRAPH, M_NOWAIT);
		}
	} else {
		ngqfree = q->next;
		ngqfreesize--;
		splx(s);
	}
	return (q);
}

/*
 * Release a queue entry
 */
#define RETURN_QBLK(q)							\
do {									\
	int s;								\
	if (ngqfreesize < ngqfreemax) { /* don't worry about races */ 	\
		s = splhigh();						\
		(q)->next = ngqfree;					\
		ngqfree = (q);						\
		ngqfreesize++;						\
		splx(s);						\
	} else {							\
		FREE((q), M_NETGRAPH);					\
	}								\
} while (0)

/*
 * Running at a raised (but we don't know which) processor priority level,
 * put the data onto a queue to be picked up by another PPL (probably splnet)
 */
int
ng_queue_data(hook_p hook, struct mbuf *m, meta_p meta)
{
	struct ng_queue_entry *q;
	int s;

	if (hook == NULL) {
		NG_FREE_DATA(m, meta);
		return (0);
	}
	if ((q = ng_getqblk()) == NULL) {
		NG_FREE_DATA(m, meta);
		return (ENOBUFS);
	}

	/* Fill out the contents */
	q->flags = NGQF_DATA;
	q->next = NULL;
	q->body.data.da_hook = hook;
	q->body.data.da_m = m;
	q->body.data.da_meta = meta;
	hook->refs++;		/* don't let it go away while on the queue */

	/* Put it on the queue */
	s = splhigh();
	if (ngqbase) {
		ngqlast->next = q;
	} else {
		ngqbase = q;
	}
	ngqlast = q;
	ngqsize++;
	splx(s);

	/* Schedule software interrupt to handle it later */
	schednetisr(NETISR_NETGRAPH);
	return (0);
}

/*
 * Running at a raised (but we don't know which) processor priority level,
 * put the msg onto a queue to be picked up by another PPL (probably splnet)
 */
int
ng_queue_msg(node_p here, struct ng_mesg *msg, const char *address)
{
	register struct ng_queue_entry *q;
	int     s;
	node_p  dest = NULL;
	char   *retaddr = NULL;
	int     error;

	/* Find the target node. */
	error = ng_path2node(here, address, &dest, &retaddr);
	if (error) {
		FREE(msg, M_NETGRAPH);
		return (error);
	}
	if ((q = ng_getqblk()) == NULL) {
		FREE(msg, M_NETGRAPH);
		if (retaddr)
			FREE(retaddr, M_NETGRAPH);
		return (ENOBUFS);
	}

	/* Fill out the contents */
	q->flags = NGQF_MESG;
	q->next = NULL;
	q->body.msg.msg_node = dest;
	q->body.msg.msg_msg = msg;
	q->body.msg.msg_retaddr = retaddr;
	dest->refs++;		/* don't let it go away while on the queue */

	/* Put it on the queue */
	s = splhigh();
	if (ngqbase) {
		ngqlast->next = q;
	} else {
		ngqbase = q;
	}
	ngqlast = q;
	ngqsize++;
	splx(s);

	/* Schedule software interrupt to handle it later */
	schednetisr(NETISR_NETGRAPH);
	return (0);
}

/*
 * Pick an item off the queue, process it, and dispose of the queue entry.
 * Should be running at splnet.
 */
static void
ngintr(void)
{
	hook_p  hook;
	struct ng_queue_entry *ngq;
	struct mbuf *m;
	meta_p  meta;
	void   *retaddr;
	struct ng_mesg *msg;
	node_p  node;
	int     error = 0;
	int     s;

	while (1) {
		s = splhigh();
		if ((ngq = ngqbase)) {
			ngqbase = ngq->next;
			ngqsize--;
		}
		splx(s);
		if (ngq == NULL)
			return;
		switch (ngq->flags) {
		case NGQF_DATA:
			hook = ngq->body.data.da_hook;
			m = ngq->body.data.da_m;
			meta = ngq->body.data.da_meta;
			RETURN_QBLK(ngq);
			NG_SEND_DATAQ(error, hook, m, meta);
			ng_unref_hook(hook);
			break;
		case NGQF_MESG:
			node = ngq->body.msg.msg_node;
			msg = ngq->body.msg.msg_msg;
			retaddr = ngq->body.msg.msg_retaddr;
			RETURN_QBLK(ngq);
			if (node->flags & NG_INVALID) {
				FREE(msg, M_NETGRAPH);
			} else {
				CALL_MSG_HANDLER(error, node, msg,
						 retaddr, NULL);
			}
			ng_unref(node);
			if (retaddr)
				FREE(retaddr, M_NETGRAPH);
			break;
		default:
			RETURN_QBLK(ngq);
		}
	}
}


