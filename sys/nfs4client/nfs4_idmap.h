/* $FreeBSD$ */
/* $Id: nfs4_idmap.h,v 1.2 2003/11/05 14:58:59 rees Exp $ */

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

#ifndef __NFS4_IDMAP_H__
#define __NFS4_IDMAP_H__

#define IDMAP_TYPE_GID 1
#define IDMAP_TYPE_UID 2
#define IDMAP_MAX_TYPE 2

typedef union {
  uid_t uid;
  gid_t gid;
} ident_t;

#define IDMAP_MAXNAMELEN 249
struct idmap_msg {
	uint32_t id_type;
	ident_t id_id;
	size_t id_namelen; 
	char id_name[IDMAP_MAXNAMELEN + 1];
};


#ifdef _KERNEL
MALLOC_DECLARE(M_IDMAP);

void idmap_init(void);
void idmap_uninit(void);

int idmap_gid_to_name(gid_t id, char ** name, size_t * len);
int idmap_uid_to_name(uid_t id, char ** name, size_t * len);

int idmap_name_to_gid(char *, size_t len, gid_t *);
int idmap_name_to_uid(char *, size_t len, uid_t *);

#endif /* _KERNEL */


#endif /* __NFS4_GSS_H__ */
