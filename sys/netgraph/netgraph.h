
/*
 * netgraph.h
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
 * $Whistle: netgraph.h,v 1.29 1999/11/01 07:56:13 julian Exp $
 */

#ifndef _NETGRAPH_NETGRAPH_H_
#define _NETGRAPH_NETGRAPH_H_ 1

#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/module.h>

#ifndef _KERNEL
#error "This file should not be included in user level programs"
#endif

/*
 * Structure of a hook
 */
struct ng_hook {
	char   *name;		/* what this node knows this link as */
	void   *private;	/* node dependant ID for this hook */
	int	flags;		/* info about this hook/link */
	int	refs;		/* dont actually free this till 0 */
	struct	ng_hook *peer;	/* the other end of this link */
	struct	ng_node *node;	/* The node this hook is attached to */
	LIST_ENTRY(ng_hook) hooks;	/* linked list of all hooks on node */
};
typedef struct ng_hook *hook_p;

/* Flags for a hook */
#define HK_INVALID		0x0001	/* don't trust it! */

/*
 * Structure of a node
 */
struct ng_node {
	char   *name;		/* optional globally unique name */
	struct	ng_type *type;	/* the installed 'type' */
	int	flags;		/* see below for bit definitions */
	int	sleepers;	/* #procs sleeping on this node */
	int	refs;		/* number of references to this node */
	int	numhooks;	/* number of hooks */
	int	colour;		/* for graph colouring algorithms */
	void   *private;	/* node type dependant node ID */
	ng_ID_t		ID;	/* Unique per node */
	LIST_HEAD(hooks, ng_hook) hooks;	/* linked list of node hooks */
	LIST_ENTRY(ng_node)	  nodes;	/* linked list of all nodes */
	LIST_ENTRY(ng_node)	  idnodes;	/* ID hash collision list */
};
typedef struct ng_node *node_p;

/* Flags for a node */
#define NG_INVALID	0x001	/* free when all sleepers and refs go to 0 */
#define NG_BUSY		0x002	/* callers should sleep or wait */
#define NG_TOUCHED	0x004	/* to avoid cycles when 'flooding' */
#define NGF_TYPE1	0x10000000	/* reserved for type specific storage */
#define NGF_TYPE2	0x20000000	/* reserved for type specific storage */
#define NGF_TYPE3	0x40000000	/* reserved for type specific storage */
#define NGF_TYPE4	0x80000000	/* reserved for type specific storage */

/*
 * The structure that holds meta_data about a data packet (e.g. priority)
 * Nodes might add or subtract options as needed if there is room.
 * They might reallocate the struct to make more room if they need to.
 * Meta-data is still experimental.
 */
struct meta_field_header {
	u_long	cookie;		/* cookie for the field. Skip fields you don't
				 * know about (same cookie as in messgaes) */
	u_short type;		/* field ID */
	u_short len;		/* total len of this field including extra
				 * data */
	char	data[0];	/* data starts here */
};

/* To zero out an option 'in place' set it's cookie to this */
#define NGM_INVALID_COOKIE	865455152

/* This part of the metadata is always present if the pointer is non NULL */
struct ng_meta {
	char	priority;	/* -ve is less priority,  0 is default */
	char	discardability; /* higher is less valuable.. discard first */
	u_short allocated_len;	/* amount malloc'd */
	u_short used_len;	/* sum of all fields, options etc. */
	u_short flags;		/* see below.. generic flags */
	struct meta_field_header options[0];	/* add as (if) needed */
};
typedef struct ng_meta *meta_p;

/* Flags for meta-data */
#define NGMF_TEST	0x01	/* discard at the last moment before sending */
#define NGMF_TRACE	0x02	/* trace when handing this data to a node */

/* node method definitions */
typedef	int	ng_constructor_t(node_p *node);
typedef	int	ng_rcvmsg_t(node_p node, struct ng_mesg *msg,
			const char *retaddr, struct ng_mesg **resp,
			hook_p lasthook);
typedef	int	ng_shutdown_t(node_p node);
typedef	int	ng_newhook_t(node_p node, hook_p hook, const char *name);
typedef	hook_p	ng_findhook_t(node_p node, const char *name);
typedef	int	ng_connect_t(hook_p hook);
typedef	int	ng_rcvdata_t(hook_p hook, struct mbuf *m, meta_p meta,
			struct mbuf **ret_m, meta_p *ret_meta);
typedef	int	ng_disconnect_t(hook_p hook);

/*
 * Command list -- each node type specifies the command that it knows
 * how to convert between ASCII and binary using an array of these.
 * The last element in the array must be a terminator with cookie=0.
 */

struct ng_cmdlist {
	u_int32_t			cookie;		/* command typecookie */
	int				cmd;		/* command number */
	const char			*name;		/* command name */
	const struct ng_parse_type	*mesgType;	/* args if !NGF_RESP */
	const struct ng_parse_type	*respType;	/* args if NGF_RESP */
};

/*
 * Structure of a node type
 */
struct ng_type {

	u_int32_t	version; 	/* must equal NG_VERSION */
	const char	*name;		/* Unique type name */
	modeventhand_t	mod_event;	/* Module event handler (optional) */
	ng_constructor_t *constructor;	/* Node constructor */
	ng_rcvmsg_t	*rcvmsg;	/* control messages come here */
	ng_shutdown_t	*shutdown;	/* reset, and free resources */
	ng_newhook_t	*newhook;	/* first notification of new hook */
	ng_findhook_t	*findhook;	/* only if you have lots of hooks */
	ng_connect_t	*connect;	/* final notification of new hook */
	ng_rcvdata_t	*rcvdata;	/* date comes here */
	ng_rcvdata_t	*rcvdataq;	/* or here if being queued */
	ng_disconnect_t	*disconnect;	/* notify on disconnect */

	const struct	ng_cmdlist *cmdlist;	/* commands we can convert */

	/* R/W data private to the base netgraph code DON'T TOUCH! */
	LIST_ENTRY(ng_type) types;		/* linked list of all types */
	int		    refs;		/* number of instances */
};

/* Send data packet with meta-data */
#define NG_SEND_DATA(error, hook, m, a)					\
	do {								\
		(error) = ng_send_data((hook), (m), (a), NULL, NULL);	\
		(m) = NULL;						\
		(a) = NULL;						\
	} while (0)

/* Send  queued data packet with meta-data */
#define NG_SEND_DATAQ(error, hook, m, a)				\
	do {								\
		(error) = ng_send_dataq((hook), (m), (a), NULL, NULL);	\
		(m) = NULL;						\
		(a) = NULL;						\
	} while (0)

#define NG_SEND_DATA_RET(error, hook, m, a)				\
	do {								\
		struct mbuf *rm = NULL;					\
		meta_p ra = NULL;					\
		(error) = ng_send_data((hook), (m), (a), &rm, &ra);	\
		(m) = rm;						\
		(a) = ra;						\
	} while (0)

/* Free metadata */
#define NG_FREE_META(a)							\
	do {								\
		if ((a)) {						\
			FREE((a), M_NETGRAPH);				\
			a = NULL;					\
		}							\
	} while (0)

/* Free any data packet and/or meta-data */
#define NG_FREE_DATA(m, a)						\
	do {								\
		if ((m)) {						\
			m_freem((m));					\
			m = NULL;					\
		}							\
		NG_FREE_META((a));					\
	} while (0)

/*
 * Use the NETGRAPH_INIT() macro to link a node type into the
 * netgraph system. This works for types compiled into the kernel
 * as well as KLD modules. The first argument should be the type
 * name (eg, echo) and the second a pointer to the type struct.
 *
 * If a different link time is desired, e.g., a device driver that
 * needs to install its netgraph type before probing, use the
 * NETGRAPH_INIT_ORDERED() macro instead. Deivce drivers probably
 * want to use SI_SUB_DRIVERS instead of SI_SUB_PSEUDO.
 */

#define NETGRAPH_INIT_ORDERED(typename, typestructp, sub, order)	\
static moduledata_t ng_##typename##_mod = {				\
	"ng_" #typename,						\
	ng_mod_event,							\
	(typestructp)							\
};									\
DECLARE_MODULE(ng_##typename, ng_##typename##_mod, sub, order);		\
MODULE_DEPEND(ng_##typename, netgraph, 1, 1, 1)

#define NETGRAPH_INIT(tn, tp)						\
	NETGRAPH_INIT_ORDERED(tn, tp, SI_SUB_PSEUDO, SI_ORDER_ANY)

/* Special malloc() type for netgraph structs and ctrl messages */
MALLOC_DECLARE(M_NETGRAPH);

int	ng_bypass(hook_p hook1, hook_p hook2);
void	ng_cutlinks(node_p node);
int	ng_con_nodes(node_p node,
	     const char *name, node_p node2, const char *name2);
meta_p	ng_copy_meta(meta_p meta);
void	ng_destroy_hook(hook_p hook);
hook_p	ng_findhook(node_p node, const char *name);
node_p	ng_findname(node_p node, const char *name);
struct	ng_type *ng_findtype(const char *type);
int	ng_make_node(const char *type, node_p *nodepp);
int	ng_make_node_common(struct ng_type *typep, node_p *nodep);
int	ng_mkpeer(node_p node, const char *name, const char *name2, char *type);
int	ng_mod_event(module_t mod, int what, void *arg);
int	ng_name_node(node_p node, const char *name);
int	ng_newtype(struct ng_type *tp);
ng_ID_t ng_node2ID(node_p node);
int	ng_path2node(node_p here, const char *path, node_p *dest, char **rtnp,
			hook_p *lasthook);
int	ng_path_parse(char *addr, char **node, char **path, char **hook);
int	ng_queue_data(hook_p hook, struct mbuf *m, meta_p meta);
int	ng_queue_msg(node_p here, struct ng_mesg *msg, const char *address);
void	ng_release_node(node_p node);
void	ng_rmnode(node_p node);
int	ng_send_data(hook_p hook, struct mbuf *m, meta_p meta,
			struct mbuf **ret_m, meta_p *ret_meta);
int	ng_send_dataq(hook_p hook, struct mbuf *m, meta_p meta,
			struct mbuf **ret_m, meta_p *ret_meta);
int	ng_send_msg(node_p here, struct ng_mesg *msg,
	    const char *address, struct ng_mesg **resp);
void	ng_unname(node_p node);
void	ng_unref(node_p node);
int	ng_wait_node(node_p node, char *msg);

#endif /* _NETGRAPH_NETGRAPH_H_ */

