/*
 * AGPGART module version 0.99
 * Copyright (C) 1999 Jeff Hartmann
 * Copyright (C) 1999 Precision Insight, Inc.
 * Copyright (C) 1999 Xi Graphics, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JEFF HARTMANN, OR ANY OTHER CONTRIBUTORS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _AGP_H
#define _AGP_H 1

#define AGPIOC_BASE       'A'
#define AGPIOC_INFO       _IOR (AGPIOC_BASE, 0, agp_info*)
#define AGPIOC_ACQUIRE    _IO  (AGPIOC_BASE, 1)
#define AGPIOC_RELEASE    _IO  (AGPIOC_BASE, 2)
#define AGPIOC_SETUP      _IOW (AGPIOC_BASE, 3, agp_setup*)
#define AGPIOC_RESERVE    _IOW (AGPIOC_BASE, 4, agp_region*)
#define AGPIOC_PROTECT    _IOW (AGPIOC_BASE, 5, agp_region*)
#define AGPIOC_ALLOCATE   _IOWR(AGPIOC_BASE, 6, agp_allocate*)
#define AGPIOC_DEALLOCATE _IOW (AGPIOC_BASE, 7, int)
#define AGPIOC_BIND       _IOW (AGPIOC_BASE, 8, agp_bind*)
#define AGPIOC_UNBIND     _IOW (AGPIOC_BASE, 9, agp_unbind*)

#define AGP_DEVICE      "/dev/agpgart"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef __KERNEL__
#include <linux/types.h>
#include <asm/types.h>

typedef struct _agp_version {
	__u16 major;
	__u16 minor;
} agp_version;

typedef struct _agp_info {
	agp_version version;	/* version of the driver        */
	__u32 bridge_id;	/* bridge vendor/device         */
	__u32 agp_mode;		/* mode info of bridge          */
	off_t aper_base;	/* base of aperture             */
	size_t aper_size;	/* size of aperture             */
	size_t pg_total;	/* max pages (swap + system)    */
	size_t pg_system;	/* max pages (system)           */
	size_t pg_used;		/* current pages used           */
} agp_info;

typedef struct _agp_setup {
	__u32 agp_mode;		/* mode info of bridge          */
} agp_setup;

/*
 * The "prot" down below needs still a "sleep" flag somehow ...
 */
typedef struct _agp_segment {
	off_t pg_start;		/* starting page to populate    */
	size_t pg_count;	/* number of pages              */
	int prot;		/* prot flags for mmap          */
} agp_segment;

typedef struct _agp_region {
	pid_t pid;		/* pid of process               */
	size_t seg_count;	/* number of segments           */
	struct _agp_segment *seg_list;
} agp_region;

typedef struct _agp_allocate {
	int key;		/* tag of allocation            */
	size_t pg_count;	/* number of pages              */
	__u32 type;		/* 0 == normal, other devspec   */
   	__u32 physical;         /* device specific (some devices  
				 * need a phys address of the     
				 * actual page behind the gatt    
				 * table)                        */
} agp_allocate;

typedef struct _agp_bind {
	int key;		/* tag of allocation            */
	off_t pg_start;		/* starting page to populate    */
} agp_bind;

typedef struct _agp_unbind {
	int key;		/* tag of allocation            */
	__u32 priority;		/* priority for paging out      */
} agp_unbind;

#else				/* __KERNEL__ */

#define AGPGART_MINOR 175

#define AGP_UNLOCK()      	up(&(agp_fe.agp_mutex));
#define AGP_LOCK()    		down(&(agp_fe.agp_mutex));
#define AGP_LOCK_INIT() 	sema_init(&(agp_fe.agp_mutex), 1)

#ifndef _AGP_BACKEND_H
typedef struct _agp_version {
	u16 major;
	u16 minor;
} agp_version;

#endif

typedef struct _agp_info {
	agp_version version;	/* version of the driver        */
	u32 bridge_id;		/* bridge vendor/device         */
	u32 agp_mode;		/* mode info of bridge          */
	off_t aper_base;	/* base of aperture             */
	size_t aper_size;	/* size of aperture             */
	size_t pg_total;	/* max pages (swap + system)    */
	size_t pg_system;	/* max pages (system)           */
	size_t pg_used;		/* current pages used           */
} agp_info;

typedef struct _agp_setup {
	u32 agp_mode;		/* mode info of bridge          */
} agp_setup;

/*
 * The "prot" down below needs still a "sleep" flag somehow ...
 */
typedef struct _agp_segment {
	off_t pg_start;		/* starting page to populate    */
	size_t pg_count;	/* number of pages              */
	int prot;		/* prot flags for mmap          */
} agp_segment;

typedef struct _agp_segment_priv {
	off_t pg_start;
	size_t pg_count;
	pgprot_t prot;
} agp_segment_priv;

typedef struct _agp_region {
	pid_t pid;		/* pid of process               */
	size_t seg_count;	/* number of segments           */
	struct _agp_segment *seg_list;
} agp_region;

typedef struct _agp_allocate {
	int key;		/* tag of allocation            */
	size_t pg_count;	/* number of pages              */
	u32 type;		/* 0 == normal, other devspec   */
	u32 physical;           /* device specific (some devices  
				 * need a phys address of the     
				 * actual page behind the gatt    
				 * table)                        */
} agp_allocate;

typedef struct _agp_bind {
	int key;		/* tag of allocation            */
	off_t pg_start;		/* starting page to populate    */
} agp_bind;

typedef struct _agp_unbind {
	int key;		/* tag of allocation            */
	u32 priority;		/* priority for paging out      */
} agp_unbind;

typedef struct _agp_client {
	struct _agp_client *next;
	struct _agp_client *prev;
	pid_t pid;
	int num_segments;
	agp_segment_priv **segments;
} agp_client;

typedef struct _agp_controller {
	struct _agp_controller *next;
	struct _agp_controller *prev;
	pid_t pid;
	int num_clients;
	agp_memory *pool;
	agp_client *clients;
} agp_controller;

#define AGP_FF_ALLOW_CLIENT		0
#define AGP_FF_ALLOW_CONTROLLER 	1
#define AGP_FF_IS_CLIENT		2
#define AGP_FF_IS_CONTROLLER		3
#define AGP_FF_IS_VALID 		4

typedef struct _agp_file_private {
	struct _agp_file_private *next;
	struct _agp_file_private *prev;
	pid_t my_pid;
	long access_flags;	/* long req'd for set_bit --RR */
} agp_file_private;

struct agp_front_data {
	struct semaphore agp_mutex;
	agp_controller *current_controller;
	agp_controller *controllers;
	agp_file_private *file_priv_list;
	u8 used_by_controller;
	u8 backend_acquired;
};

#endif				/* __KERNEL__ */

#endif				/* _AGP_H */
