/*
 *  ncp.h
 *
 *  Copyright (C) 1995 by Volker Lendecke
 *  Modified for sparc by J.F. Chadima
 *  Modified for __constant_ntoh by Frank A. Vorstenbosch
 *
 */

#ifndef _LINUX_NCP_H
#define _LINUX_NCP_H

#include <linux/types.h>

#define NCP_PTYPE                (0x11)
#define NCP_PORT                 (0x0451)

#define NCP_ALLOC_SLOT_REQUEST   (0x1111)
#define NCP_REQUEST              (0x2222)
#define NCP_DEALLOC_SLOT_REQUEST (0x5555)

struct ncp_request_header {
	__u16 type __attribute__((packed));
	__u8 sequence __attribute__((packed));
	__u8 conn_low __attribute__((packed));
	__u8 task __attribute__((packed));
	__u8 conn_high __attribute__((packed));
	__u8 function __attribute__((packed));
	__u8 data[0] __attribute__((packed));
};

#define NCP_REPLY                (0x3333)
#define NCP_POSITIVE_ACK         (0x9999)

struct ncp_reply_header {
	__u16 type __attribute__((packed));
	__u8 sequence __attribute__((packed));
	__u8 conn_low __attribute__((packed));
	__u8 task __attribute__((packed));
	__u8 conn_high __attribute__((packed));
	__u8 completion_code __attribute__((packed));
	__u8 connection_state __attribute__((packed));
	__u8 data[0] __attribute__((packed));
};

#define NCP_VOLNAME_LEN (16)
#define NCP_NUMBER_OF_VOLUMES (256)
struct ncp_volume_info {
	__u32 total_blocks;
	__u32 free_blocks;
	__u32 purgeable_blocks;
	__u32 not_yet_purgeable_blocks;
	__u32 total_dir_entries;
	__u32 available_dir_entries;
	__u8 sectors_per_block;
	char volume_name[NCP_VOLNAME_LEN + 1];
};

#define AR_READ      (ntohs(0x0100))
#define AR_WRITE     (ntohs(0x0200))
#define AR_EXCLUSIVE (ntohs(0x2000))

#define NCP_FILE_ID_LEN 6

/* Defines for Name Spaces */
#define NW_NS_DOS     0
#define NW_NS_MAC     1
#define NW_NS_NFS     2
#define NW_NS_FTAM    3
#define NW_NS_OS2     4

/*  Defines for ReturnInformationMask */
#define RIM_NAME	      (ntohl(0x01000000L))
#define RIM_SPACE_ALLOCATED   (ntohl(0x02000000L))
#define RIM_ATTRIBUTES	      (ntohl(0x04000000L))
#define RIM_DATA_SIZE	      (ntohl(0x08000000L))
#define RIM_TOTAL_SIZE	      (ntohl(0x10000000L))
#define RIM_EXT_ATTR_INFO     (ntohl(0x20000000L))
#define RIM_ARCHIVE	      (ntohl(0x40000000L))
#define RIM_MODIFY	      (ntohl(0x80000000L))
#define RIM_CREATION	      (ntohl(0x00010000L))
#define RIM_OWNING_NAMESPACE  (ntohl(0x00020000L))
#define RIM_DIRECTORY	      (ntohl(0x00040000L))
#define RIM_RIGHTS	      (ntohl(0x00080000L))
#define RIM_ALL 	      (ntohl(0xFF0F0000L))
#define RIM_COMPRESSED_INFO   (ntohl(0x00000080L))

/* open/create modes */
#define OC_MODE_OPEN	  0x01
#define OC_MODE_TRUNCATE  0x02
#define OC_MODE_REPLACE   0x02
#define OC_MODE_CREATE	  0x08

/* open/create results */
#define OC_ACTION_NONE	   0x00
#define OC_ACTION_OPEN	   0x01
#define OC_ACTION_CREATE   0x02
#define OC_ACTION_TRUNCATE 0x04
#define OC_ACTION_REPLACE  0x04

/* access rights attributes */
#ifndef AR_READ_ONLY
#define AR_READ_ONLY	   0x0001
#define AR_WRITE_ONLY	   0x0002
#define AR_DENY_READ	   0x0004
#define AR_DENY_WRITE	   0x0008
#define AR_COMPATIBILITY   0x0010
#define AR_WRITE_THROUGH   0x0040
#define AR_OPEN_COMPRESSED 0x0100
#endif

struct nw_info_struct {
	__u32 spaceAlloc __attribute__((packed));
	__u32 attributes __attribute__((packed));
	__u16 flags __attribute__((packed));
	__u32 dataStreamSize __attribute__((packed));
	__u32 totalStreamSize __attribute__((packed));
	__u16 numberOfStreams __attribute__((packed));
	__u16 creationTime __attribute__((packed));
	__u16 creationDate __attribute__((packed));
	__u32 creatorID __attribute__((packed));
	__u16 modifyTime __attribute__((packed));
	__u16 modifyDate __attribute__((packed));
	__u32 modifierID __attribute__((packed));
	__u16 lastAccessDate __attribute__((packed));
	__u16 archiveTime __attribute__((packed));
	__u16 archiveDate __attribute__((packed));
	__u32 archiverID __attribute__((packed));
	__u16 inheritedRightsMask __attribute__((packed));
	__u32 dirEntNum __attribute__((packed));
	__u32 DosDirNum __attribute__((packed));
	__u32 volNumber __attribute__((packed));
	__u32 EADataSize __attribute__((packed));
	__u32 EAKeyCount __attribute__((packed));
	__u32 EAKeySize __attribute__((packed));
	__u32 NSCreator __attribute__((packed));
	__u8 nameLen __attribute__((packed));
	__u8 entryName[256] __attribute__((packed));
};

/* modify mask - use with MODIFY_DOS_INFO structure */
#define DM_ATTRIBUTES		  (ntohl(0x02000000L))
#define DM_CREATE_DATE		  (ntohl(0x04000000L))
#define DM_CREATE_TIME		  (ntohl(0x08000000L))
#define DM_CREATOR_ID		  (ntohl(0x10000000L))
#define DM_ARCHIVE_DATE 	  (ntohl(0x20000000L))
#define DM_ARCHIVE_TIME 	  (ntohl(0x40000000L))
#define DM_ARCHIVER_ID		  (ntohl(0x80000000L))
#define DM_MODIFY_DATE		  (ntohl(0x00010000L))
#define DM_MODIFY_TIME		  (ntohl(0x00020000L))
#define DM_MODIFIER_ID		  (ntohl(0x00040000L))
#define DM_LAST_ACCESS_DATE	  (ntohl(0x00080000L))
#define DM_INHERITED_RIGHTS_MASK  (ntohl(0x00100000L))
#define DM_MAXIMUM_SPACE	  (ntohl(0x00200000L))

struct nw_modify_dos_info {
	__u32 attributes __attribute__((packed));
	__u16 creationDate __attribute__((packed));
	__u16 creationTime __attribute__((packed));
	__u32 creatorID __attribute__((packed));
	__u16 modifyDate __attribute__((packed));
	__u16 modifyTime __attribute__((packed));
	__u32 modifierID __attribute__((packed));
	__u16 archiveDate __attribute__((packed));
	__u16 archiveTime __attribute__((packed));
	__u32 archiverID __attribute__((packed));
	__u16 lastAccessDate __attribute__((packed));
	__u16 inheritanceGrantMask __attribute__((packed));
	__u16 inheritanceRevokeMask __attribute__((packed));
	__u32 maximumSpace __attribute__((packed));
};

struct nw_search_sequence {
	__u8 volNumber __attribute__((packed));
	__u32 dirBase __attribute__((packed));
	__u32 sequence __attribute__((packed));
};

#endif				/* _LINUX_NCP_H */
