/* $FreeBSD$ */
/* $Id: nfs4_dev.h,v 1.3 2003/11/05 14:58:59 rees Exp $ */

/*
 * copyright (c) 2003
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and redistribute
 * this software and such derivative works for any purpose, so long as the name
 * of the university of michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software without specific,
 * written prior authorization.  if the above copyright notice or any other
 * identification of the university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the university
 * of michigan as to its fitness for any purpose, and without warranty by the
 * university of michigan of any kind, either express or implied, including
 * without limitation the implied warranties of merchantability and fitness for
 * a particular purpose. the regents of the university of michigan shall not be
 * liable for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising out of or in
 * connection with the use of the software, even if it has been or is hereafter
 * advised of the possibility of such damages.
 */

#ifndef _NFS3_DEV_H_
#define _NFS4_DEV_H_

#include <sys/ioccom.h>


/* type of upcall */
#define NFS4DEV_TYPE_IDMAP 0
#define NFS4DEV_TYPE_GSS   1
#define NFS4DEV_MAX_TYPE   1

struct nfs4dev_msg {
	unsigned int msg_vers;
	unsigned int msg_type;
	unsigned int msg_xid;
	unsigned int msg_error;

	#define NFS4DEV_MSG_MAX_DATALEN 350 
	size_t msg_len;
	uint8_t msg_data[NFS4DEV_MSG_MAX_DATALEN];
};

#define NFS4DEV_VERSION  (0x3 << 16 | sizeof(struct nfs4dev_msg))

/* ioctl commands */
#define NFS4DEVIOCGET _IOR('A', 0x200, struct nfs4dev_msg)
#define NFS4DEVIOCPUT _IOW('A', 0x201, struct nfs4dev_msg)

#ifdef _KERNEL
int nfs4dev_call(uint32_t type, caddr_t req_data, size_t req_len, caddr_t rep_datap, size_t * rep_lenp);

void nfs4dev_purge(void);

void nfs4dev_init(void);
void nfs4dev_uninit(void);
#endif

#endif /* _NFS4_DEV_H_ */
