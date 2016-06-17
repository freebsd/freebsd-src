/* sis_mm.c -- Private header for Direct Rendering Manager -*- linux-c -*-
 * Created: Mon Jan  4 10:05:05 1999 by sclin@sis.com.tw
 *
 * Copyright 2000 Silicon Integrated Systems Corp, Inc., HsinChu, Taiwan.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 * 
 */

#include "sis.h"
#include <linux/sisfb.h>
#include "drmP.h"
#include "sis_drm.h"
#include "sis_drv.h"
#include "sis_ds.h"

#define MAX_CONTEXT 100
#define VIDEO_TYPE 0 
#define AGP_TYPE 1

typedef struct {
  int used;
  int context;
  set_t *sets[2]; /* 0 for video, 1 for AGP */
} sis_context_t;

static sis_context_t global_ppriv[MAX_CONTEXT];

static int add_alloc_set(int context, int type, unsigned int val)
{
  int i, retval = 0;
  
  for(i = 0; i < MAX_CONTEXT; i++)
    if(global_ppriv[i].used && global_ppriv[i].context == context){
      retval = setAdd(global_ppriv[i].sets[type], val);
      break;
    }
  return retval;
}

static int del_alloc_set(int context, int type, unsigned int val)
{  
  int i, retval = 0;
  for(i = 0; i < MAX_CONTEXT; i++)
    if(global_ppriv[i].used && global_ppriv[i].context == context){
      retval = setDel(global_ppriv[i].sets[type], val);
      break;
    }
  return retval;
}

/* fb management via fb device */ 
#if 1
int sis_fb_alloc(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
  drm_sis_mem_t fb;
  struct sis_memreq req;
  int retval = 0;
   
  if (copy_from_user(&fb, (drm_sis_mem_t *)arg, sizeof(fb)))
	  return -EFAULT;
  
  req.size = fb.size;
  sis_malloc(&req);
  if(req.offset){
    /* TODO */
    fb.offset = req.offset;
    fb.free = req.offset;
    if(!add_alloc_set(fb.context, VIDEO_TYPE, fb.free)){
      DRM_DEBUG("adding to allocation set fails\n");
      sis_free(req.offset);
      retval = -1;
    }
  }
  else{  
    fb.offset = 0;
    fb.size = 0;
    fb.free = 0;
  }
   
  if (copy_to_user((drm_sis_mem_t *)arg, &fb, sizeof(fb))) return -EFAULT;

  DRM_DEBUG("alloc fb, size = %d, offset = %ld\n", fb.size, req.offset);

  return retval;
}

int sis_fb_free(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
  drm_sis_mem_t fb;
  int retval = 0;
    
  if (copy_from_user(&fb, (drm_sis_mem_t *)arg, sizeof(fb)))
	  return -EFAULT;
  
  if(!fb.free){
    return -1;
  }

  sis_free(fb.free);
  if(!del_alloc_set(fb.context, VIDEO_TYPE, fb.free))
    retval = -1;

  DRM_DEBUG("free fb, offset = %ld\n", fb.free);
  
  return retval;
}

#else

int sis_fb_alloc(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
  return -1;
}

int sis_fb_free(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
  return 0;
}

#endif

/* agp memory management */ 
#if 1

static memHeap_t *AgpHeap = NULL;

int sisp_agp_init(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
  drm_sis_agp_t agp;
   
  if (copy_from_user(&agp, (drm_sis_agp_t *)arg, sizeof(agp)))
	  return -EFAULT;

  AgpHeap = mmInit(agp.offset, agp.size);

  DRM_DEBUG("offset = %u, size = %u", agp.offset, agp.size);
  
  return 0;
}

int sisp_agp_alloc(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
  drm_sis_mem_t agp;
  PMemBlock block;
  int retval = 0;
   
  if(!AgpHeap)
    return -1;
  
  if (copy_from_user(&agp, (drm_sis_mem_t *)arg, sizeof(agp)))
	  return -EFAULT;
  
  block = mmAllocMem(AgpHeap, agp.size, 0, 0);
  if(block){
    /* TODO */
    agp.offset = block->ofs;
    agp.free = (unsigned long)block;
    if(!add_alloc_set(agp.context, AGP_TYPE, agp.free)){
      DRM_DEBUG("adding to allocation set fails\n");
      mmFreeMem((PMemBlock)agp.free);
      retval = -1;
    }
  }
  else{  
    agp.offset = 0;
    agp.size = 0;
    agp.free = 0;
  }
   
  if (copy_to_user((drm_sis_mem_t *)arg, &agp, sizeof(agp))) return -EFAULT;

  DRM_DEBUG("alloc agp, size = %d, offset = %d\n", agp.size, agp.offset);

  return retval;
}

int sisp_agp_free(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
  drm_sis_mem_t agp;
  int retval = 0;

  if(!AgpHeap)
    return -1;
    
  if (copy_from_user(&agp, (drm_sis_mem_t *)arg, sizeof(agp)))
	  return -EFAULT;
  
  if(!agp.free){
    return -1;
  }

  mmFreeMem((PMemBlock)agp.free);
  if(!del_alloc_set(agp.context, AGP_TYPE, agp.free))
    retval = -1;

  DRM_DEBUG("free agp, free = %ld\n", agp.free);
  
  return retval;
}

#endif

int sis_init_context(int context)
{
	int i;
	
	for(i = 0; i < MAX_CONTEXT ; i++)
	  if(global_ppriv[i].used && (global_ppriv[i].context == context))
	    break;

	if(i >= MAX_CONTEXT){
	  for(i = 0; i < MAX_CONTEXT ; i++){
	    if(!global_ppriv[i].used){
	      global_ppriv[i].context = context;
	      global_ppriv[i].used = 1;
	      global_ppriv[i].sets[0] = setInit();
	      global_ppriv[i].sets[1] = setInit();
	      DRM_DEBUG("init allocation set, socket=%d, context = %d\n", 
	                 i, context);
	      break;
	    }	
	  }
	  if((i >= MAX_CONTEXT) || (global_ppriv[i].sets[0] == NULL) ||
	     (global_ppriv[i].sets[1] == NULL)){
	    return 0;
	  }
	}
	
	return 1;
}

int sis_final_context(int context)
{
	int i;

	for(i=0; i<MAX_CONTEXT; i++)
	  if(global_ppriv[i].used && (global_ppriv[i].context == context))
	    break;
          
	if(i < MAX_CONTEXT){
	  set_t *set;
	  unsigned int item;
	  int retval;
	  
  	  DRM_DEBUG("find socket %d, context = %d\n", i, context);

	  /* Video Memory */
	  set = global_ppriv[i].sets[0];
	  retval = setFirst(set, &item);
	  while(retval){
   	    DRM_DEBUG("free video memory 0x%x\n", item);
            sis_free(item);
	    retval = setNext(set, &item);
	  }
	  setDestroy(set);

	  /* AGP Memory */
	  set = global_ppriv[i].sets[1];
	  retval = setFirst(set, &item);
	  while(retval){
   	    DRM_DEBUG("free agp memory 0x%x\n", item);
	    mmFreeMem((PMemBlock)item);
	    retval = setNext(set, &item);
	  }
	  setDestroy(set);
	  
	  global_ppriv[i].used = 0;	  
        }

	/* turn-off auto-flip */
	/* TODO */
#if defined(SIS_STEREO)
	flip_final();
#endif
	
	return 1;
}
