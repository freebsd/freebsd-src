/* usual BSD style copyright here */
/* Written by Julian Elischer (julian@dialix.oz.au)*/
/*
 * $Id: devfsext.h,v 1.3 1995/11/29 10:49:13 julian Exp $
 */

#ifndef _SYS_DEVFSECT_H_
#define _SYS_DEVFSECT_H_ 1
void *devfs_add_devsw(char *path,
		char *name,
		void *devsw,
		int minor,
		int chrblk,
		uid_t uid,
		gid_t gid,
		int perms)  ; 

/* deprecated.. don't use.. */
void *dev_add(char *path,
		char *name,
		void *funct,
		int minor,
		int chrblk,
		uid_t uid,
		gid_t gid,
		int perms)  ; 

void *dev_link(char *path,
		char *name,
		void *original); /* the result of a previous dev_link
					or dev_add operation */

#define DV_CHR 0
#define DV_BLK 1
#define DV_DEV 2

#endif /*_SYS_DEVFSECT_H_*/
