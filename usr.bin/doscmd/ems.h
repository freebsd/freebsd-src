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
 * $FreeBSD: src/usr.bin/doscmd/ems.h,v 1.3 1999/08/28 01:00:13 peter Exp $
 */

#ifndef EMS_H
#define EMS_H

/* Header for ems.c, the EMS emulation */

/* Global definitions, some of them will be configurable in the future */

#define EMS_NUM_HANDLES		256		/* Includes OS handle 0 */
#define EMS_MAXSIZE		10240 		/* In kbytes */
#define EMS_MAX_PHYS		4		/* Frame is 64kB */
#define EMS_FRAME_ADDR		0xe0000
#define EMS_VERSION		0x40		/* Version 4.0 */
#define EMS_PAGESIZE		(16 *1024)	/* page size in bytes */
#define EMS_SAVEMAGIC		0xAFFE		/* magic number */

/* These are the LIM EMS 3.0 calls */
#define GET_MANAGER_STATUS      0x40    
#define GET_PAGE_FRAME_SEGMENT  0x41    
#define GET_PAGE_COUNTS         0x42    
#define GET_HANDLE_AND_ALLOCATE 0x43    
#define MAP_UNMAP               0x44    
#define DEALLOCATE_HANDLE       0x45    
#define GET_EMM_VERSION         0x46    
#define SAVE_PAGE_MAP           0x47    
#define RESTORE_PAGE_MAP        0x48    
#define RESERVED_1              0x49    
#define RESERVED_2              0x4a    
#define GET_HANDLE_COUNT        0x4b    
#define GET_PAGES_OWNED         0x4c    
#define GET_PAGES_FOR_ALL       0x4d    

/* LIM EMS 4.0 calls */
/* Global subfunctions for the LIM EMS 4.0 calls */
#define GET			0x0
#define SET			0x1
/* Modes for Map/Unmap and AlterandCall/AlterAndJump */
#define PHYS_ADDR		0x0
#define SEG_ADDR		0x0

/* Page map functions */
#define PAGE_MAP 	        0x4e
#define PAGE_MAP_PARTIAL	0x4f
/* Page map subfunctions */
#define GET_SET			0x2
#define GET_SIZE		0x3

#define MAP_UNMAP_MULTI_HANDLE  0x50

#define REALLOC_PAGES		0x51

#define HANDLE_ATTRIBUTES	0x52
/* Subfunctions */
#define HANDLE_CAPABILITY	0x2

#define HANDLE_NAME		0x53

#define HANDLE_DIRECTORY	0x54
#define HANDLE_SEARCH		0x1
#define GET_TOTAL_HANDLES	0x2

#define ALTER_PAGEMAP_JUMP	0x55
#define ALTER_PAGEMAP_CALL	0x56
/* Subfunction for call */
#define GET_STACK_SIZE		0x2

#define MOVE_MEMORY_REGION	0x57
/* Subfunctions */
#define	MOVE			0x0
#define EXCHANGE		0x1

#define GET_MAPPABLE_PHYS_ADDR	0x58
/* Subfunctions */
#define	GET_ARRAY		0x0
#define	GET_ARRAY_ENTRIES	0x1

#define GET_HW_CONFIGURATION	0x59
/* Subfunctions */
#define	GET_HW_ARRAY		0x0
#define	GET_RAW_PAGE_COUNT	0x1

#define ALLOCATE_PAGES		0x5a
/* Subfunctions */
#define	ALLOC_STANDARD		0x0
#define	ALLOC_RAW		0x1

#define ALTERNATE_MAP_REGISTER  0x5b
/* Subfunctions */
#define GET_SAVE_ARRAY_SIZE	0x2
#define ALLOCATE_REGISTER_SET	0x3
#define DEALLOCATE_REGISTER_SET 0x4
#define ALLOCATE_DMA		0x5
#define ENABLE_DMA		0x6
#define DISABLE_DMA		0x7
#define DEALLOCATE_DMA		0x8

#define PREPARE_WARMBOOT	0x5c

#define OS_FUNCTION_SET		0x5d
/* Subfunctions */
#define ENABLE			0x0
#define DISABLE			0x1
#define RETURN_KEY		0x2

/* End of call definitions */

/* EMS errors */

#define EMS_SUCCESS      	0x0
#define EMS_SW_MALFUNC    	0x80
#define EMS_HW_MALFUNC    	0x81
#define EMS_INV_HANDLE     	0x83
#define EMS_FUNC_NOSUP  	0x84
#define EMS_OUT_OF_HANDLES  	0x85
#define EMS_SAVED_MAP		0x86
#define EMS_OUT_OF_PHYS 	0x87
#define EMS_OUT_OF_LOG  	0x88
#define EMS_ZERO_PAGES  	0x89
#define EMS_LOGPAGE_TOOBIG 	0x8a
#define EMS_ILL_PHYS    	0x8b
#define EMS_NO_ROOM_TO_SAVE	0x8c
#define EMS_ALREADY_SAVED	0x8d
#define EMS_NO_SAVED_CONTEXT	0x8e
#define EMS_INVALID_SUB 	0x8f
#define EMS_INVALID_ATTR	0x90
#define EMS_FEAT_NOSUP  	0x91
#define EMS_MOVE_OVERLAP1  	0x92
#define EMS_MOVE_OVERFLOW	0x93
#define EMS_PAGEOFFSET		0x95
#define EMS_MOVE_OVERLAP2 	0x97
#define EMS_HNAME_NOT_FOUND	0xa0
#define EMS_NAME_EXISTS   	0xa1
#define EMS_SAVED_CONTEXT_BAD	0xa3
#define EMS_FUNCTION_DISABLED	0xa4

/*
 * EMS handles: The handle contains at its end an array of pointers to
 * its allocated pages. The array is of size npages. Handle structs are
 * malloced at runtime.
 * Page numbering: Every page is 16kB, always. The pages are numbered
 * from 0 to highest page, depending on total EMS memory. Every handle
 * has pages allocated and this pages too are numbered from 0 to highest
 * page allocated. This are *not* the same numbers, because there may be
 * holes in the allocation.
 * Page numbers are unsigned short, which will give us 65536 * 16 kB (1GB)
 * pages to handle at maximum. This should be enough for the next years.
 */ 

typedef struct {
    short handle[4];	     /* Handle for each mapping */
    u_char pos_mapped[4];    /* Boolean value, 1 if something is mapped */
    u_char pos_pagenum[4];   /* Page number currently mapped into position */
} EMS_mapping_context;


/* This union is for copying operations of the handle name only */
typedef union {
    u_char uc_hn[8];
    u_long ul_hn[2];
} Hname;

typedef struct {
    Hname   hname;
    u_long npages;
    /* The mapping context for save/restore page map */ 
    EMS_mapping_context *mcontext;    
    /* The pagenum here is the number in the system page array. The 
     * logical page number connected with this handle is the index into
     * this array.
     */
    u_short pagenum[0];		
    /* Will grow here, depending on allocation */
} EMS_handle;

/*
 * The connection between every page in the system and the handles is
 * maintained by an array of these structs. The array is indexed by the
 * page numbers.
 */

typedef struct {
    short handle;	/* The handle this page belongs to */
#define EMS_FREE    0
#define EMS_ALLOCED 1
#define EMS_MAPPED  2
    u_short status;	/* room for misc information */
} EMS_page;

/*
 * The combined pointer into EMS memory: offs is the offset into an EMS
 * page, page is the page index inside the region allocated to a handle.
 * This depends on EMS_PAGESIZE.
 * This is used for copy and move operations.
 */

typedef struct {
    u_long offs:14;
    u_long page:18;
} EMS_combi;

typedef union {
    u_long    ua_addr;		/* Conventional address pointer */
    EMS_combi ua_emsaddr;	/* EMS address pointer */
} EMS_addr;

#define EMS_OFFS(u) u.ua_emsaddr.offs
#define EMS_PAGE(u) u.ua_emsaddr.page
#define EMS_PTR(u) u.ua_addr

/*
 * EMS info structure, only used to pass information to and from
 * DOS 
 */

typedef struct {
    u_short handle __attribute__ ((packed));   /* handle */
    u_short npages __attribute__ ((packed));   /* pages allocated */
} EMShandlepage;

/*
 * EMS map/unmap multiple, only used to pass information to and from
 * DOS 
 */

typedef struct {
    u_short log __attribute__ ((packed));   /* logical page number */
    u_short phys __attribute__ ((packed));  /* physical page (position) or
					      segment address inside frame */
} EMSmapunmap;

/*
 * EMS handle directory, only used to pass information to and from
 * DOS 
 */

typedef struct {
   u_short log  __attribute__ ((packed));   /* logical page number */
   Hname   name __attribute__ ((packed));   /* Handle name */

} EMShandledir;

/*
 * Structure for get/set page map: This structure is used to save and
 * restore the page map from DOS memory. A program can get the mapping
 * context and later set (restore) it. To avoid errors we add a magic
 * number and a checksum.
 */

typedef struct {
   u_short magic;		/* Magic number */
   u_short checksum;		/* Checksum over entire structure */
   EMS_mapping_context ems_saved_context;
} EMScontext;

/*
 * EMS physical address array, only used to pass information to and from
 * DOS 
 */

typedef struct {
   u_short segm __attribute__ ((packed));  /* segment address inside frame */
   u_short phys __attribute__ ((packed));  /* physical page (position) */
} EMSaddrarray;

/*
 * EMS move memory call structure, only used to pass information to and from
 * DOS 
 */

typedef struct {
   u_long  length __attribute__ ((packed));      /* length of region */
#define EMS_MOVE_CONV 0
#define EMS_MOVE_EMS  1
   u_char  src_type __attribute__ ((packed));    /* source type (0,1) */
   u_short src_handle __attribute__ ((packed));  /* source handle */
   u_short src_offset __attribute__ ((packed));  /* source offset */
   u_short src_seg __attribute__ ((packed));     /* source type  */
   u_char  dst_type __attribute__ ((packed));    /* destination type (0,1) */
   u_short dst_handle __attribute__ ((packed));  /* destination handle */
   u_short dst_offset __attribute__ ((packed));  /* destination offset */
   u_short dst_seg __attribute__ ((packed));     /* destination type  */
} EMSmovemem;

#endif /* EMS_H */
