/*-
 * Copyright (c) 1997 Helmut Wirth <hfwirth@ping.at>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, witout modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef XMS_H
#define XMS_H

#define XMS_VERSION   0x0300  /* version 3.00 */
#define XMS_REVISION  0x0100  /* driver revision 1.0 */

#define NUM_HANDLES     64	/* number of available handles */
#define FIRST_HANDLE    1	/* number of firts valid handle */
#define PARAGRAPH       16      /* bytes in a paragraph */
#define MAX_BLOCK_LOCKS 256	/* number of locks on a block */
#define DEFAULT_EMM_SIZE 512 * 1024    /* default EMM size */
 
/* Register AH codes for XMS functions */
#define  XMS_GET_VERSION                	0x00
#define  XMS_ALLOCATE_HIGH_MEMORY       	0x01
#define  XMS_FREE_HIGH_MEMORY           	0x02
#define  XMS_GLOBAL_ENABLE_A20          	0x03
#define  XMS_GLOBAL_DISABLE_A20         	0x04
#define  XMS_LOCAL_ENABLE_A20           	0x05
#define  XMS_LOCAL_DISABLE_A20          	0x06
#define  XMS_QUERY_A20                  	0x07
#define  XMS_QUERY_FREE_EXTENDED_MEMORY 	0x08
#define  XMS_ALLOCATE_EXTENDED_MEMORY   	0x09
#define  XMS_FREE_EXTENDED_MEMORY       	0x0a
#define  XMS_MOVE_EXTENDED_MEMORY_BLOCK 	0x0b
#define  XMS_LOCK_EXTENDED_MEMORY_BLOCK 	0x0c
#define  XMS_UNLOCK_EXTENDED_MEMORY_BLOCK 	0x0d
#define  XMS_GET_EMB_HANDLE_INFORMATION 	0x0e
#define  XMS_RESIZE_EXTENDED_MEMORY_BLOCK 	0x0f
#define  XMS_ALLOCATE_UMB               	0x10
#define  XMS_DEALLOCATE_UMB             	0x11
#define  XMS_REALLOCATE_UMB			0x12
/* New functions for values bigger than 65MB, not implented yet */
#define  XMS_QUERY_FREE_EXTENDED_MEMORY_LARGE	0x88
#define  XMS_ALLOCATE_EXTENDED_MEMORY_LARGE   	0x89
#define  XMS_FREE_EXTENDED_MEMORY_LARGE       	0x8a


/* XMS error return codes */
#define XMS_SUCCESS			0x0
#define XMS_NOT_IMPLEMENTED		0x80
#define XMS_VDISK			0x81   /* If vdisk.sys is present */
#define XMS_A20_ERROR			0x82
#define XMS_GENERAL_ERROR		0x8e
#define XMS_HMA_NOT_MANAGED		0x90
#define XMS_HMA_ALREADY_USED		0x91
#define XMS_HMA_NOT_ALLOCATED		0x93
#define XMS_A20_STILL_ENABLED		0x94
#define XMS_FULL			0xa0
#define XMS_OUT_OF_HANDLES		0xa1
#define XMS_INVALID_HANDLE		0xa2
#define XMS_INVALID_SOURCE_HANDLE	0xa3
#define XMS_INVALID_SOURCE_OFFSET	0xa4
#define XMS_INVALID_DESTINATION_HANDLE	0xa5
#define XMS_INVALID_DESTINATION_OFFSET	0xa6
#define XMS_INVALID_LENGTH		0xa7
#define XMS_BLOCK_NOT_LOCKED		0xaa
#define XMS_BLOCK_IS_LOCKED		0xab
#define XMS_BLOCK_LOCKCOUNT_OVERFLOW	0xac
#define XMS_REQUESTED_UMB_TOO_BIG	0xb0
#define XMS_NO_UMBS_AVAILABLE		0xb1
#define XMS_INVALID_UMB_SEGMENT		0xb2


/*
 * EMM structure for data exchange with DOS caller, hence the
 * packed format
 */

struct EMM {
   u_long  nbytes;
   u_short src_handle __attribute__ ((packed));
   u_long  src_offset __attribute__ ((packed));
   u_short dst_handle __attribute__ ((packed));
   u_long  dst_offset __attribute__ ((packed));
} ;

/*
 * XMS info structure, only used to pass information to and from
 * DOS 
 */

struct XMSinfo {
   u_char handle;				/* the handle */
   u_char num_locks __attribute__ ((packed));   /* number of locks */
   u_long size __attribute__ ((packed));	/* size of memory */
   u_long phys_addr __attribute__ ((packed));   /* "physical" address */
};

/*
 * Handle management inside the emulator for extendend memory pages, 
 * invisible to DOS
 */

typedef struct {
   u_long addr;		/* address inside emulator, from malloc() */
   u_long size;		/* size in bytes */
   u_char num_locks;	/* lock count for this handle */
} XMS_handle;

/*
 * Managment of UMB memory paragraphs (16 bytes). UMB blocks are
 * directly accessible by VM86 applications and lie between 0xd0000 and
 * 0xefff0 in VM86 memory space. 
 */

struct _UMB_block {
   u_long addr;			/* Start address of block */
   u_long size;			/* Size in bytes */
   struct _UMB_block *next;   
};

typedef struct _UMB_block UMB_block;

#endif /* XMS_H */
