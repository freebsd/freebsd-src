
/*
 * ng_message.h
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
 * $Whistle: ng_message.h,v 1.12 1999/01/25 01:17:44 archie Exp $
 */

#ifndef _NETGRAPH_NG_MESSAGE_H_
#define _NETGRAPH_NG_MESSAGE_H_ 1

/* ASCII string size limits */
#define NG_TYPELEN	15	/* max type name len (16 with null) */
#define NG_HOOKLEN	15	/* max hook name len (16 with null) */
#define NG_NODELEN	15	/* max node name len (16 with null) */
#define NG_PATHLEN	511	/* max path len     (512 with null) */
#define NG_CMDSTRLEN	15	/* max command string (16 with null) */
#define NG_TEXTRESPONSE 1024	/* allow this length for a text response */

/* A netgraph message */
struct ng_mesg {
	struct	ng_msghdr {
		u_char		version;		/* must == NG_VERSION */
		u_char		spare;			/* pad to 2 bytes */
		u_int16_t	arglen;			/* length of data */
		u_int32_t	flags;			/* message status */
		u_int32_t	token;			/* match with reply */
		u_int32_t	typecookie;		/* node's type cookie */
		u_int32_t	cmd;			/* command identifier */
		u_char		cmdstr[NG_CMDSTRLEN+1];	/* cmd string + \0 */
	} header;
	char	data[0];		/* placeholder for actual data */
};

/* Keep this in sync with the above structure definition */
#define NG_GENERIC_NG_MESG_INFO(dtype)	{			\
	{							\
	  { "version",		&ng_parse_uint8_type	},	\
	  { "spare",		&ng_parse_uint8_type	},	\
	  { "arglen",		&ng_parse_uint16_type	},	\
	  { "flags",		&ng_parse_hint32_type	},	\
	  { "token",		&ng_parse_uint32_type	},	\
	  { "typecookie",	&ng_parse_uint32_type	},	\
	  { "cmd",		&ng_parse_uint32_type	},	\
	  { "cmdstr",		&ng_parse_cmdbuf_type	},	\
	  { "data",		(dtype)			},	\
	  { NULL },						\
	}							\
}

/* Negraph type binary compatibility field */
#define NG_VERSION	3

/* Flags field flags */
#define NGF_ORIG	0x0000		/* the msg is the original request */
#define NGF_RESP	0x0001		/* the message is a response */

/* Type of a unique node ID */
#define ng_ID_t unsigned int

/*
 * Here we describe the "generic" messages that all nodes inherently
 * understand. With the exception of NGM_TEXT_STATUS, these are handled
 * automatically by the base netgraph code.
 */

/* Generic message type cookie */
#define NGM_GENERIC_COOKIE	851672668

/* Generic messages defined for this type cookie */
#define	NGM_SHUTDOWN		1	/* shut down node */
#define NGM_MKPEER		2	/* create and attach a peer node */
#define NGM_CONNECT		3	/* connect two nodes */
#define NGM_NAME		4	/* give a node a name */
#define NGM_RMHOOK		5	/* break a connection btw. two nodes */
#define	NGM_NODEINFO		6	/* get nodeinfo for the target */
#define	NGM_LISTHOOKS		7	/* get list of hooks on node */
#define	NGM_LISTNAMES		8	/* list all globally named nodes */
#define	NGM_LISTNODES		9	/* list all nodes, named and unnamed */
#define	NGM_LISTTYPES		10	/* list all installed node types */
#define	NGM_TEXT_STATUS		11	/* (optional) get text status report */
#define	NGM_BINARY2ASCII	12	/* convert struct ng_mesg to ascii */
#define	NGM_ASCII2BINARY	13	/* convert ascii to struct ng_mesg */
#define	NGM_TEXT_CONFIG		14	/* (optional) get/set text config */

/* Structure used for NGM_MKPEER */
struct ngm_mkpeer {
	char	type[NG_TYPELEN + 1];			/* peer type */
	char	ourhook[NG_HOOKLEN + 1];		/* hook name */
	char	peerhook[NG_HOOKLEN + 1];		/* peer hook name */
};

/* Keep this in sync with the above structure definition */
#define NG_GENERIC_MKPEER_INFO()	{			\
	{							\
	  { "type",		&ng_parse_typebuf_type	},	\
	  { "ourhook",		&ng_parse_hookbuf_type	},	\
	  { "peerhook",		&ng_parse_hookbuf_type	},	\
	  { NULL },						\
	}							\
}

/* Structure used for NGM_CONNECT */
struct ngm_connect {
	char	path[NG_PATHLEN + 1];			/* peer path */
	char	ourhook[NG_HOOKLEN + 1];		/* hook name */
	char	peerhook[NG_HOOKLEN + 1];		/* peer hook name */
};

/* Keep this in sync with the above structure definition */
#define NG_GENERIC_CONNECT_INFO()	{			\
	{							\
	  { "path",		&ng_parse_pathbuf_type	},	\
	  { "ourhook",		&ng_parse_hookbuf_type	},	\
	  { "peerhook",		&ng_parse_hookbuf_type	},	\
	  { NULL },						\
	}							\
}

/* Structure used for NGM_NAME */
struct ngm_name {
	char	name[NG_NODELEN + 1];			/* node name */
};

/* Keep this in sync with the above structure definition */
#define NG_GENERIC_NAME_INFO()	{				\
	{							\
	  { "name",		&ng_parse_nodebuf_type	},	\
	  { NULL },						\
	}							\
}

/* Structure used for NGM_RMHOOK */
struct ngm_rmhook {
	char	ourhook[NG_HOOKLEN + 1];		/* hook name */
};

/* Keep this in sync with the above structure definition */
#define NG_GENERIC_RMHOOK_INFO()	{			\
	{							\
	  { "hook",		&ng_parse_hookbuf_type	},	\
	  { NULL },						\
	}							\
}

/* Structure used for NGM_NODEINFO */
struct nodeinfo {
	char		name[NG_NODELEN + 1];	/* node name (if any) */
        char    	type[NG_TYPELEN + 1];   /* peer type */
	ng_ID_t		id;			/* unique identifier */
	u_int32_t	hooks;			/* number of active hooks */
};

/* Keep this in sync with the above structure definition */
#define NG_GENERIC_NODEINFO_INFO()	{			\
	{							\
	  { "name",		&ng_parse_nodebuf_type	},	\
	  { "type",		&ng_parse_typebuf_type	},	\
	  { "id",		&ng_parse_hint32_type	},	\
	  { "hooks",		&ng_parse_uint32_type	},	\
	  { NULL },						\
	}							\
}

/* Structure used for NGM_LISTHOOKS */
struct linkinfo {
	char		ourhook[NG_HOOKLEN + 1];	/* hook name */
	char		peerhook[NG_HOOKLEN + 1];	/* peer hook */
	struct nodeinfo	nodeinfo;
};

/* Keep this in sync with the above structure definition */
#define NG_GENERIC_LINKINFO_INFO(nitype)	{		\
	{							\
	  { "ourhook",		&ng_parse_hookbuf_type	},	\
	  { "peerhook",		&ng_parse_hookbuf_type	},	\
	  { "nodeinfo",		(nitype)		},	\
	  { NULL },						\
	}							\
}

struct hooklist {
	struct nodeinfo nodeinfo;		/* node information */
	struct linkinfo link[0];		/* info about each hook */
};

/* Keep this in sync with the above structure definition */
#define NG_GENERIC_HOOKLIST_INFO(nitype,litype)	{		\
	{							\
	  { "nodeinfo",		(nitype)		},	\
	  { "linkinfo",		(litype)		},	\
	  { NULL },						\
	}							\
}

/* Structure used for NGM_LISTNAMES/NGM_LISTNODES */
struct namelist {
	u_int32_t	numnames;
	struct nodeinfo	nodeinfo[0];
};

/* Keep this in sync with the above structure definition */
#define NG_GENERIC_LISTNODES_INFO(niarraytype)	{		\
	{							\
	  { "numnames",		&ng_parse_uint32_type	},	\
	  { "nodeinfo",		(niarraytype)		},	\
	  { NULL },						\
	}							\
}

/* Structure used for NGM_LISTTYPES */
struct typeinfo {
	char		type_name[NG_TYPELEN + 1];	/* name of type */
	u_int32_t	numnodes;			/* number alive */
};

/* Keep this in sync with the above structure definition */
#define NG_GENERIC_TYPEINFO_INFO()		{		\
	{							\
	  { "typename",		&ng_parse_typebuf_type	},	\
	  { "numnodes",		&ng_parse_uint32_type	},	\
	  { NULL },						\
	}							\
}

struct typelist {
	u_int32_t	numtypes;
	struct typeinfo	typeinfo[0];
};

/* Keep this in sync with the above structure definition */
#define NG_GENERIC_TYPELIST_INFO(tiarraytype)	{		\
	{							\
	  { "numtypes",		&ng_parse_uint32_type	},	\
	  { "typeinfo",		(tiarraytype)		},	\
	  { NULL },						\
	}							\
}

/*
 * For netgraph nodes that are somehow associated with file descriptors
 * (e.g., a device that has a /dev entry and is also a netgraph node),
 * we define a generic ioctl for requesting the corresponding nodeinfo
 * structure and for assigning a name (if there isn't one already).
 *
 * For these to you need to also #include <sys/ioccom.h>.
 */

#define NGIOCGINFO	_IOR('N', 40, struct nodeinfo)	/* get node info */
#define NGIOCSETNAME	_IOW('N', 41, struct ngm_name)	/* set node name */

#ifdef _KERNEL
/*
 * Allocate and initialize a netgraph message "msg" with "len"
 * extra bytes of argument. Sets "msg" to NULL if fails.
 * Does not initialize token.
 */
#define NG_MKMESSAGE(msg, cookie, cmdid, len, how)			\
	do {								\
	  MALLOC((msg), struct ng_mesg *, sizeof(struct ng_mesg)	\
	    + (len), M_NETGRAPH, (how) | M_ZERO);			\
	  if ((msg) == NULL)						\
	    break;							\
	  (msg)->header.version = NG_VERSION;				\
	  (msg)->header.typecookie = (cookie);				\
	  (msg)->header.cmd = (cmdid);					\
	  (msg)->header.arglen = (len);					\
	  strncpy((msg)->header.cmdstr, #cmdid,				\
	    sizeof((msg)->header.cmdstr) - 1);				\
	} while (0)

/*
 * Allocate and initialize a response "rsp" to a message "msg"
 * with "len" extra bytes of argument. Sets "rsp" to NULL if fails.
 */
#define NG_MKRESPONSE(rsp, msg, len, how)				\
	do {								\
	  MALLOC((rsp), struct ng_mesg *, sizeof(struct ng_mesg)	\
	    + (len), M_NETGRAPH, (how) | M_ZERO);			\
	  if ((rsp) == NULL)						\
	    break;							\
	  (rsp)->header.version = NG_VERSION;				\
	  (rsp)->header.arglen = (len);					\
	  (rsp)->header.token = (msg)->header.token;			\
	  (rsp)->header.typecookie = (msg)->header.typecookie;		\
	  (rsp)->header.cmd = (msg)->header.cmd;			\
	  bcopy((msg)->header.cmdstr, (rsp)->header.cmdstr,		\
	    sizeof((rsp)->header.cmdstr));				\
	  (rsp)->header.flags |= NGF_RESP;				\
	} while (0)
#endif /* _KERNEL */

#endif /* _NETGRAPH_NG_MESSAGE_H_ */

