
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
 * Author: Julian Elischer <julian@whistle.com>
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
#define NG_VERSION 1
#define NGF_ORIG	0x0000		/* the msg is the original request */
#define NGF_RESP	0x0001		/* the message is a response */

/*
 * Here we describe the "generic" messages that all nodes inherently
 * understand. With the exception of NGM_TEXT_STATUS, these are handled
 * automatically by the base netgraph code.
 */

/* Generic message type cookie */
#define NGM_GENERIC_COOKIE 851672668

/* Generic messages defined for this type cookie */
#define	NGM_SHUTDOWN	0x0001 	/* no args */
#define NGM_MKPEER	0x0002
#define NGM_CONNECT	0x0003
#define NGM_NAME	0x0004
#define NGM_RMHOOK	0x0005
#define	NGM_NODEINFO	0x0006	/* get nodeinfo for the target */
#define	NGM_LISTHOOKS	0x0007	/* get nodeinfo for the target + hook info */
#define	NGM_LISTNAMES	0x0008	/* list all globally named nodes */
#define	NGM_LISTNODES	0x0009	/* list all nodes, named and unnamed */
#define	NGM_LISTTYPES	0x000a	/* list all installed node types */
#define	NGM_TEXT_STATUS	0x000b	/* (optional) returns human readable status */

/*
 * Args sections for generic NG commands. All strings are NUL-terminated.
 */
struct ngm_mkpeer {
	char	type[NG_TYPELEN + 1];			/* peer type */
	char	ourhook[NG_HOOKLEN + 1];		/* hook name */
	char	peerhook[NG_HOOKLEN + 1];		/* peer hook name */
};

struct ngm_connect {
	char	path[NG_PATHLEN + 1];			/* peer path */
	char	ourhook[NG_HOOKLEN + 1];		/* hook name */
	char	peerhook[NG_HOOKLEN + 1];		/* peer hook name */
};

struct ngm_name {
	char	name[NG_NODELEN + 1];			/* node name */
};

struct ngm_rmhook {
	char	ourhook[NG_HOOKLEN + 1];		/* hook name */
};

/* Structures used in response to NGM_NODEINFO and NGM_LISTHOOKS */
struct nodeinfo {
	char		name[NG_NODELEN + 1];	/* node name (if any) */
        char    	type[NG_TYPELEN + 1];   /* peer type */
	u_int32_t	id;			/* unique identifier */
	u_int32_t	hooks;			/* number of active hooks */
};

struct linkinfo {
	char		ourhook[NG_HOOKLEN + 1];	/* hook name */
	char		peerhook[NG_HOOKLEN + 1];	/* peer hook */
	struct nodeinfo	nodeinfo;
};

struct hooklist {
	struct nodeinfo nodeinfo;		/* node information */
	struct linkinfo link[0];		/* info about each hook */
};

/* Structure used for NGM_LISTNAMES/NGM_LISTNODES (not node specific) */
struct namelist {
	u_int32_t	numnames;
	struct nodeinfo	nodeinfo[0];
};

/* Structures used for NGM_LISTTYPES (not node specific) */
struct typeinfo {
	char		typename[NG_TYPELEN + 1];	/* name of type */
	u_int32_t	numnodes;			/* number alive */
};

struct typelist {
	u_int32_t	numtypes;
	struct typeinfo	typeinfo[0];
};

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

#ifdef KERNEL
/*
 * Allocate and initialize a netgraph message "msg" with "len"
 * extra bytes of argument. Sets "msg" to NULL if fails.
 * Does not initialize token.
 */
#define NG_MKMESSAGE(msg, cookie, cmdid, len, how)			\
	do {								\
	  MALLOC((msg), struct ng_mesg *, sizeof(struct ng_mesg)	\
	    + (len), M_NETGRAPH, (how));				\
	  if ((msg) == NULL)						\
	    break;							\
	  bzero((msg), sizeof(struct ng_mesg) + (len));			\
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
	    + (len), M_NETGRAPH, (how));				\
	  if ((rsp) == NULL)						\
	    break;							\
	  bzero((rsp), sizeof(struct ng_mesg) + (len));			\
	  (rsp)->header.version = NG_VERSION;				\
	  (rsp)->header.arglen = (len);					\
	  (rsp)->header.token = (msg)->header.token;			\
	  (rsp)->header.typecookie = (msg)->header.typecookie;		\
	  (rsp)->header.cmd = (msg)->header.cmd;			\
	  bcopy((msg)->header.cmdstr, (rsp)->header.cmdstr,		\
	    sizeof((rsp)->header.cmdstr));				\
	  (rsp)->header.flags |= NGF_RESP;				\
	} while (0)
#endif /* KERNEL */

#endif /* _NETGRAPH_NG_MESSAGE_H_ */

