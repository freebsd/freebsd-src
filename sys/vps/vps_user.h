/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id: vps_user.h 153 2013-06-03 16:18:17Z klaus $ */

#ifndef _VPS_USER_H
#define _VPS_USER_H

#include <sys/cdefs.h>

#define DEFAULTLEN      0x40

/*
 * Possible vps status values
 */
#define VPS_ST_CREATING                  0x02
#define VPS_ST_RUNNING                   0x04
#define VPS_ST_SUSPENDED                 0x08
#define VPS_ST_SNAPSHOOTING              0x10
#define VPS_ST_RESTORING                 0x20
#define VPS_ST_DYING                     0x40
#define VPS_ST_DEAD                      0x80

/*
 * Various flags
 */
#define VPS_SUSPEND_RELINKFILES 	0x1000

#ifndef _VPS_USER_H__ONLY_FLAGS
/*
 * This structure is used for the LIST ioctl,
 * where only basic information about each vps
 * instance is needed.
 * Therefore this structure is small and can be filled
 * in quickly.
 */
struct vps_info {
        char name[MAXHOSTNAMELEN];
        char fsroot[MAXPATHLEN];

        int status;
        int nprocs;
        int nsocks;
        int nifaces;
	int restore_count;

	struct {
		unsigned long virt;
		unsigned long phys;
		unsigned long pctcpu;
	} acc;
};

/*
 * Extended info, used for the ''show'' command.
 */
struct vps_extinfo {
        char name[MAXHOSTNAMELEN];
        char fsroot[MAXPATHLEN];

        int status;
        int nprocs;
        int nsocks;
        int nifaces;
	int restore_count;
};

struct vps_getextinfo {
        char vps_name[MAXHOSTNAMELEN];
	void *data;
	size_t datalen;
};

/*
 * This is the extended structure for creating, modifying
 * or detailed viewing of a single vps instance.
 * It contains all parameters and information available.
 */
struct vps_param {
        char name[MAXHOSTNAMELEN];
        char fsroot[MAXPATHLEN];
};

struct vps_arg_ifmove {
        char vps_name[MAXHOSTNAMELEN];
        char if_name[IFNAMSIZ];
        char if_newname[IFNAMSIZ];
};

struct vps_arg_snapst {
        char vps_name[MAXHOSTNAMELEN];
        caddr_t database;
        size_t datalen;
        char errmsg[DEFAULTLEN];
	caddr_t msgbase;
	size_t msglen;
};

/* --- */

#define VPS_ARG_PRIV_ALLOW	0x2
#define VPS_ARG_PRIV_DENY	0x4
#define VPS_ARG_PRIV_NOSYS	0x8

#define VPS_ARG_ITEM_PRIV	0x2
#define VPS_ARG_ITEM_IP4	0x4
#define VPS_ARG_ITEM_IP6	0x8
#define VPS_ARG_ITEM_LIMIT	0x10

struct vps_arg_priv {
	u_int priv;
	u_int value;
};

struct vps_arg_ip4 {
	struct in_addr addr;
	struct in_addr mask;
};

struct vps_arg_ip6 {
	struct in6_addr addr;
	u_int8_t	plen;
};

struct vps_arg_limit {
	u_int resource;
	size_t cur;
	size_t soft;
	size_t hard;
	u_int16_t hits_soft;
	u_int16_t hits_hard;
};

struct vps_arg_item {
	u_int16_t type;
	u_int8_t revoke;
	u_int8_t _pad0;
	u_int16_t _pad1;
	u_int16_t _pad2;
	union {
		struct vps_arg_priv  priv;
		struct vps_arg_ip4   ip4;
		struct vps_arg_ip6   ip6;
		struct vps_arg_limit limit;
	} u;
};

struct vps_arg_get {
        char vps_name[MAXHOSTNAMELEN];
	void *data;
	size_t datalen;
};

struct vps_arg_set {
        char vps_name[MAXHOSTNAMELEN];
	void *data;
	size_t datalen;
        char errmsg[DEFAULTLEN];
};

struct vps_arg_flags {
	char vps_name[MAXHOSTNAMELEN];
	int flags;
};

struct vps_arg_getconsfd {
	char vps_name[MAXHOSTNAMELEN];
	int consfd;
};

/* --- */

/*
 * arg is the count of info structs which can be read (mmap).
 */
#define VPS_IOC_LIST     _IOR('v', 0x0101, int)

/*
 * see 'struct vps_getextinfo' definition.
 */
#define VPS_IOC_GETXINFO _IOWR('v', 0x0102, struct vps_getextinfo)

/*
 * arg is a pointer to a char string providing information
 * if an error occured.
 */
#define VPS_IOC_SET      _IOR('v', 0x0103, caddr_t *)

/* arg is a filled in vps_param struct. */
#define VPS_IOC_CREAT    _IOW('v', 0x0104, struct vps_param)

/* arg is name of vps instance. */
#define VPS_IOC_DESTR    _IOW('v', 0x0105, char[MAXHOSTNAMELEN])

/* arg is name of vps instance. */
#define VPS_IOC_SWITCH   _IOW('v', 0x0106, char[MAXHOSTNAMELEN])

/* arg is name of vps instance. */
#define VPS_IOC_SWITWT   _IOW('v', 0x0107, char[MAXHOSTNAMELEN])

/* arg is filled in vps_arg_ifmove struct. interface name
   is returned in ->if_name. */
#define VPS_IOC_IFMOVE   _IOWR('v', 0x0109, struct vps_arg_ifmove)

/* arg is name of vps instance. */
#define VPS_IOC_SUSPND   _IOW('v', 0x0110, struct vps_arg_flags)

/* arg is name of vps instance. */
#define VPS_IOC_RESUME   _IOW('v', 0x0111, struct vps_arg_flags)

/* arg is filled in vps_arg_snapst struct.
   length of data for mmaping is returned. */
#define VPS_IOC_SNAPST   _IOWR('v', 0x0112, struct vps_arg_snapst)

/* arg is filled in vps_arg_snapst struct.
   length of data for mmaping is given.
   name of vps instance is optional. */
#define VPS_IOC_RESTOR   _IOWR('v', 0x0113, struct vps_arg_snapst)

/* arg is size in bytes to allocate. */
#define VPS_IOC_ALLOC    _IOW('v', 0x0114, int)

/* arg is name of vps instance. */
#define VPS_IOC_ABORT    _IOW('v', 0x0115, struct vps_arg_flags)

/* arg is ignored. */
#define VPS_IOC_SNAPSTFIN     _IOW('v', 0x0119, void *)

#define VPS_IOC_ARGGET   _IOWR('v', 0x0120, struct vps_arg_get)
#define VPS_IOC_ARGSET   _IOWR('v', 0x0121, struct vps_arg_set)

/* vps_name is the mountpoint of the filesystem in question. */
#define VPS_IOC_FSCALCPATH    _IOWR('v', 0x0130, struct vps_arg_get)

/* vps_name is the vps which has the filesystem in question mounted
   as its root. */
#define VPS_IOC_FSCALC        _IOWR('v', 0x0131, struct vps_arg_get)

/* arg is name of vps instance. */
#define VPS_IOC_GETCONSFD     _IOWR('v', 0x0140, struct vps_arg_getconsfd)

#endif /* ! _VPS_USER_H__ONLY_FLAGS */

#endif /* _VPS_USER_H */

/* EOF */
