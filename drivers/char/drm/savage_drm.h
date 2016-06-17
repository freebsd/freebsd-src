/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __SAVAGE_DRM_H__
#define __SAVAGE_DRM_H__

#ifndef __SAVAGE_SAREA_DEFINES__
#define __SAVAGE_SAREA_DEFINES__

#define DRM_SAVAGE_MEM_PAGE (1UL<<12)
#define DRM_SAVAGE_MEM_WORK 32
#define DRM_SAVAGE_MEM_LOCATION_PCI 1
#define DRM_SAVAGE_MEM_LOCATION_AGP 2
#define DRM_SAVAGE_DMA_AGP_SIZE (16*1024*1024)

typedef struct drm_savage_alloc_cont_mem
{
	size_t size; /*size of buffer*/
	unsigned long type; /*4k page or word*/
	unsigned long alignment;
	unsigned long location; /*agp or pci*/

	unsigned long phyaddress;
	unsigned long linear;
} drm_savage_alloc_cont_mem_t;

typedef struct drm_savage_get_physcis_address
{
	unsigned long v_address;
	unsigned long p_address;
} drm_savage_get_physcis_address_t;

/*ioctl number*/
#define DRM_IOCTL_SAVAGE_ALLOC_CONTINUOUS_MEM \
	DRM_IOWR(0x40,drm_savage_alloc_cont_mem_t)
#define DRM_IOCTL_SAVAGE_GET_PHYSICS_ADDRESS \
	DRM_IOWR(0x41, drm_savage_get_physcis_address_t)
#define DRM_IOCTL_SAVAGE_FREE_CONTINUOUS_MEM \
	DRM_IOWR(0x42, drm_savage_alloc_cont_mem_t)

#define SAVAGE_FRONT		0x1
#define SAVAGE_BACK		0x2
#define SAVAGE_DEPTH		0x4
#define SAVAGE_STENCIL		0x8

/* What needs to be changed for the current vertex dma buffer?
 */
#define SAVAGE_UPLOAD_CTX	0x1
#define SAVAGE_UPLOAD_TEX0	0x2
#define SAVAGE_UPLOAD_TEX1	0x4
#define SAVAGE_UPLOAD_PIPE	0x8  /* <- seems should be removed, Jiayo Hsu */
#define SAVAGE_UPLOAD_TEX0IMAGE	0x10 /* handled client-side */
#define SAVAGE_UPLOAD_TEX1IMAGE	0x20 /* handled client-side */
#define SAVAGE_UPLOAD_2D	0x40
#define SAVAGE_WAIT_AGE		0x80 /* handled client-side */
#define SAVAGE_UPLOAD_CLIPRECTS	0x100 /* handled client-side */
/*frank:add Buffer state 2001/11/15*/
#define SAVAGE_UPLOAD_BUFFERS 0x200
/* original marked off in MGA drivers , Jiayo Hsu Oct.23,2001 */

/* Keep these small for testing.
 */
#define SAVAGE_NR_SAREA_CLIPRECTS	8

/* 2 heaps (1 for card, 1 for agp), each divided into upto 128
 * regions, subject to a minimum region size of (1<<16) == 64k.
 *
 * Clients may subdivide regions internally, but when sharing between
 * clients, the region size is the minimum granularity.
 */

#define SAVAGE_CARD_HEAP		0
#define SAVAGE_AGP_HEAP			1
#define SAVAGE_NR_TEX_HEAPS		2
#define SAVAGE_NR_TEX_REGIONS		16   /* num. of global texture manage list element*/
#define SAVAGE_LOG_MIN_TEX_REGION_SIZE	16   /* each region 64K, Jiayo Hsu */

#endif /* __SAVAGE_SAREA_DEFINES__ */

/* drm_tex_region_t define in drm.h */

typedef drm_tex_region_t drm_savage_tex_region_t;

/* Setup registers for 2D, X server
 */
typedef struct {
	unsigned int pitch;
} drm_savage_server_regs_t;


typedef struct _drm_savage_sarea {
	/* The channel for communication of state information to the kernel
	 * on firing a vertex dma buffer.
	 */
	unsigned int setup[28];    /* 3D context registers */
   	drm_savage_server_regs_t server_state;
 
   	unsigned int dirty;
   	
   	unsigned int vertsize;   /* vertext  size */

	/* The current cliprects, or a subset thereof.
	 */
   	drm_clip_rect_t boxes[SAVAGE_NR_SAREA_CLIPRECTS];
   	unsigned int nbox;

	/* Information about the most recently used 3d drawable.  The
	 * client fills in the req_* fields, the server fills in the
	 * exported_ fields and puts the cliprects into boxes, above.
	 *
	 * The client clears the exported_drawable field before
	 * clobbering the boxes data.
	 */
        unsigned int req_drawable;	 /* the X drawable id */
	unsigned int req_draw_buffer;	 /* SAVAGE_FRONT or SAVAGE_BACK */

        unsigned int exported_drawable;
	unsigned int exported_index;
        unsigned int exported_stamp;
        unsigned int exported_buffers;
        unsigned int exported_nfront;
        unsigned int exported_nback;
	int exported_back_x, exported_front_x, exported_w;
	int exported_back_y, exported_front_y, exported_h;
   	drm_clip_rect_t exported_boxes[SAVAGE_NR_SAREA_CLIPRECTS];

	/* Counters for aging textures and for client-side throttling.
	 */
	unsigned int status[4];


	/* LRU lists for texture memory in agp space and on the card.
	 */
	drm_tex_region_t texList[SAVAGE_NR_TEX_HEAPS][SAVAGE_NR_TEX_REGIONS+1];
	unsigned int texAge[SAVAGE_NR_TEX_HEAPS];

	/* Mechanism to validate card state.
	 */
   	int ctxOwner;
	unsigned long shadow_status[64];/*too big?*/

	/*agp offset*/
	unsigned long agp_offset;
} drm_savage_sarea_t,*drm_savage_sarea_ptr;



typedef struct drm_savage_init {

   	unsigned long sarea_priv_offset;

	int chipset;
   	int sgram;

	unsigned int maccess;

   	unsigned int fb_cpp;
	unsigned int front_offset, front_pitch;
   	unsigned int back_offset, back_pitch;

   	unsigned int depth_cpp;
   	unsigned int depth_offset, depth_pitch;

   	unsigned int texture_offset[SAVAGE_NR_TEX_HEAPS];
   	unsigned int texture_size[SAVAGE_NR_TEX_HEAPS];

	unsigned long fb_offset;
	unsigned long mmio_offset;
	unsigned long status_offset;
} drm_savage_init_t;

typedef struct drm_savage_fullscreen {
	enum {
		SAVAGE_INIT_FULLSCREEN    = 0x01,
		SAVAGE_CLEANUP_FULLSCREEN = 0x02
	} func;
} drm_savage_fullscreen_t;

typedef struct drm_savage_clear {
	unsigned int flags;
	unsigned int clear_color;
	unsigned int clear_depth;
	unsigned int color_mask;
	unsigned int depth_mask;
} drm_savage_clear_t;

typedef struct drm_savage_vertex {
   	int idx;			/* buffer to queue */
	int used;			/* bytes in use */
	int discard;			/* client finished with buffer?  */
} drm_savage_vertex_t;

typedef struct drm_savage_indices {
   	int idx;			/* buffer to queue */
	unsigned int start;
	unsigned int end;
	int discard;			/* client finished with buffer?  */
} drm_savage_indices_t;

typedef struct drm_savage_iload {
	int idx;
	unsigned int dstorg;
	unsigned int length;
} drm_savage_iload_t;

typedef struct _drm_savage_blit {
	unsigned int planemask;
	unsigned int srcorg;
	unsigned int dstorg;
	int src_pitch, dst_pitch;
	int delta_sx, delta_sy;
	int delta_dx, delta_dy;
	int height, ydir;		/* flip image vertically */
	int source_pitch, dest_pitch;
} drm_savage_blit_t;

#endif
