/*
 * Copyright © 2010 Daniel Vetter
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/drm2/drmP.h>
#include <dev/drm2/drm.h>
#include <dev/drm2/i915/i915_drm.h>
#include <dev/drm2/i915/i915_drv.h>
#include <dev/drm2/i915/intel_drv.h>
#include <sys/sched.h>
#include <sys/sf_buf.h>

/* PPGTT support for Sandybdrige/Gen6 and later */
static void
i915_ppgtt_clear_range(struct i915_hw_ppgtt *ppgtt,
    unsigned first_entry, unsigned num_entries)
{
	uint32_t *pt_vaddr;
	uint32_t scratch_pte;
	struct sf_buf *sf;
	unsigned act_pd, first_pte, last_pte, i;

	act_pd = first_entry / I915_PPGTT_PT_ENTRIES;
	first_pte = first_entry % I915_PPGTT_PT_ENTRIES;

	scratch_pte = GEN6_PTE_ADDR_ENCODE(ppgtt->scratch_page_dma_addr);
	scratch_pte |= GEN6_PTE_VALID | GEN6_PTE_CACHE_LLC;

	while (num_entries) {
		last_pte = first_pte + num_entries;
		if (last_pte > I915_PPGTT_PT_ENTRIES)
			last_pte = I915_PPGTT_PT_ENTRIES;

		sched_pin();
		sf = sf_buf_alloc(ppgtt->pt_pages[act_pd], SFB_CPUPRIVATE);
		pt_vaddr = (uint32_t *)(uintptr_t)sf_buf_kva(sf);

		for (i = first_pte; i < last_pte; i++)
			pt_vaddr[i] = scratch_pte;

		sf_buf_free(sf);
		sched_unpin();

		num_entries -= last_pte - first_pte;
		first_pte = 0;
		act_pd++;
	}

}

int
i915_gem_init_aliasing_ppgtt(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv;
	struct i915_hw_ppgtt *ppgtt;
	u_int first_pd_entry_in_global_pt, i;

	dev_priv = dev->dev_private;

	/*
	 * ppgtt PDEs reside in the global gtt pagetable, which has 512*1024
	 * entries. For aliasing ppgtt support we just steal them at the end for
	 * now.
	 */
	first_pd_entry_in_global_pt = 512 * 1024 - I915_PPGTT_PD_ENTRIES;

	ppgtt = malloc(sizeof(*ppgtt), DRM_I915_GEM, M_WAITOK | M_ZERO);

	ppgtt->num_pd_entries = I915_PPGTT_PD_ENTRIES;
	ppgtt->pt_pages = malloc(sizeof(vm_page_t) * ppgtt->num_pd_entries,
	    DRM_I915_GEM, M_WAITOK | M_ZERO);

	for (i = 0; i < ppgtt->num_pd_entries; i++) {
		ppgtt->pt_pages[i] = vm_page_alloc(NULL, 0,
		    VM_ALLOC_NORMAL | VM_ALLOC_NOOBJ | VM_ALLOC_WIRED |
		    VM_ALLOC_ZERO);
		if (ppgtt->pt_pages[i] == NULL) {
			dev_priv->mm.aliasing_ppgtt = ppgtt;
			i915_gem_cleanup_aliasing_ppgtt(dev);
			return (-ENOMEM);
		}
	}

	ppgtt->scratch_page_dma_addr = dev_priv->mm.gtt.scratch_page_dma;

	i915_ppgtt_clear_range(ppgtt, 0, ppgtt->num_pd_entries *
	    I915_PPGTT_PT_ENTRIES);
	ppgtt->pd_offset = (first_pd_entry_in_global_pt) * sizeof(uint32_t);
	dev_priv->mm.aliasing_ppgtt = ppgtt;
	return (0);
}

static void
i915_ppgtt_insert_pages(struct i915_hw_ppgtt *ppgtt, unsigned first_entry,
    unsigned num_entries, vm_page_t *pages, uint32_t pte_flags)
{
	uint32_t *pt_vaddr, pte;
	struct sf_buf *sf;
	unsigned act_pd, first_pte;
	unsigned last_pte, i;
	vm_paddr_t page_addr;

	act_pd = first_entry / I915_PPGTT_PT_ENTRIES;
	first_pte = first_entry % I915_PPGTT_PT_ENTRIES;

	while (num_entries) {
		last_pte = first_pte + num_entries;
		if (last_pte > I915_PPGTT_PT_ENTRIES)
			last_pte = I915_PPGTT_PT_ENTRIES;

		sched_pin();
		sf = sf_buf_alloc(ppgtt->pt_pages[act_pd], SFB_CPUPRIVATE);
		pt_vaddr = (uint32_t *)(uintptr_t)sf_buf_kva(sf);

		for (i = first_pte; i < last_pte; i++) {
			page_addr = VM_PAGE_TO_PHYS(*pages);
			pte = GEN6_PTE_ADDR_ENCODE(page_addr);
			pt_vaddr[i] = pte | pte_flags;

			pages++;
		}

		sf_buf_free(sf);
		sched_unpin();

		num_entries -= last_pte - first_pte;
		first_pte = 0;
		act_pd++;
	}
}

void
i915_ppgtt_bind_object(struct i915_hw_ppgtt *ppgtt,
    struct drm_i915_gem_object *obj, enum i915_cache_level cache_level)
{
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;
	uint32_t pte_flags;

	dev = obj->base.dev;
	dev_priv = dev->dev_private;
	pte_flags = GEN6_PTE_VALID;

	switch (cache_level) {
	case I915_CACHE_LLC_MLC:
		pte_flags |= GEN6_PTE_CACHE_LLC_MLC;
		break;
	case I915_CACHE_LLC:
		pte_flags |= GEN6_PTE_CACHE_LLC;
		break;
	case I915_CACHE_NONE:
		pte_flags |= GEN6_PTE_UNCACHED;
		break;
	default:
		panic("cache mode");
	}

	i915_ppgtt_insert_pages(ppgtt, obj->gtt_space->start >> PAGE_SHIFT,
	    obj->base.size >> PAGE_SHIFT, obj->pages, pte_flags);
}

void i915_ppgtt_unbind_object(struct i915_hw_ppgtt *ppgtt,
			      struct drm_i915_gem_object *obj)
{
	i915_ppgtt_clear_range(ppgtt, obj->gtt_space->start >> PAGE_SHIFT,
	    obj->base.size >> PAGE_SHIFT);
}

void
i915_gem_cleanup_aliasing_ppgtt(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv;
	struct i915_hw_ppgtt *ppgtt;
	vm_page_t m;
	int i;

	dev_priv = dev->dev_private;
	ppgtt = dev_priv->mm.aliasing_ppgtt;
	if (ppgtt == NULL)
		return;
	dev_priv->mm.aliasing_ppgtt = NULL;

	for (i = 0; i < ppgtt->num_pd_entries; i++) {
		m = ppgtt->pt_pages[i];
		if (m != NULL) {
			vm_page_unwire(m, 0);
			vm_page_free(m);
		}
	}
	free(ppgtt->pt_pages, DRM_I915_GEM);
	free(ppgtt, DRM_I915_GEM);
}


static unsigned int
cache_level_to_agp_type(struct drm_device *dev, enum i915_cache_level
    cache_level)
{

	switch (cache_level) {
	case I915_CACHE_LLC_MLC:
		if (INTEL_INFO(dev)->gen >= 6)
			return (AGP_USER_CACHED_MEMORY_LLC_MLC);
		/*
		 * Older chipsets do not have this extra level of CPU
		 * cacheing, so fallthrough and request the PTE simply
		 * as cached.
		 */
	case I915_CACHE_LLC:
		return (AGP_USER_CACHED_MEMORY);

	default:
	case I915_CACHE_NONE:
		return (AGP_USER_MEMORY);
	}
}

static bool
do_idling(struct drm_i915_private *dev_priv)
{
	bool ret = dev_priv->mm.interruptible;

	if (dev_priv->mm.gtt.do_idle_maps) {
		dev_priv->mm.interruptible = false;
		if (i915_gpu_idle(dev_priv->dev, false)) {
			DRM_ERROR("Couldn't idle GPU\n");
			/* Wait a bit, in hopes it avoids the hang */
			DELAY(10);
		}
	}

	return ret;
}

static void
undo_idling(struct drm_i915_private *dev_priv, bool interruptible)
{

	if (dev_priv->mm.gtt.do_idle_maps)
		dev_priv->mm.interruptible = interruptible;
}

void
i915_gem_restore_gtt_mappings(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv;
	struct drm_i915_gem_object *obj;

	dev_priv = dev->dev_private;

	/* First fill our portion of the GTT with scratch pages */
	intel_gtt_clear_range(dev_priv->mm.gtt_start / PAGE_SIZE,
	    (dev_priv->mm.gtt_end - dev_priv->mm.gtt_start) / PAGE_SIZE);

	list_for_each_entry(obj, &dev_priv->mm.gtt_list, gtt_list) {
		i915_gem_clflush_object(obj);
		i915_gem_gtt_rebind_object(obj, obj->cache_level);
	}

	intel_gtt_chipset_flush();
}

int
i915_gem_gtt_bind_object(struct drm_i915_gem_object *obj)
{
	unsigned int agp_type;

	agp_type = cache_level_to_agp_type(obj->base.dev, obj->cache_level);
	intel_gtt_insert_pages(obj->gtt_space->start >> PAGE_SHIFT,
	    obj->base.size >> PAGE_SHIFT, obj->pages, agp_type);
	return (0);
}

void
i915_gem_gtt_rebind_object(struct drm_i915_gem_object *obj,
    enum i915_cache_level cache_level)
{
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;
	unsigned int agp_type;

	dev = obj->base.dev;
	dev_priv = dev->dev_private;
	agp_type = cache_level_to_agp_type(dev, cache_level);

	intel_gtt_insert_pages(obj->gtt_space->start >> PAGE_SHIFT,
	    obj->base.size >> PAGE_SHIFT, obj->pages, agp_type);
}

void
i915_gem_gtt_unbind_object(struct drm_i915_gem_object *obj)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	bool interruptible;

	dev = obj->base.dev;
	dev_priv = dev->dev_private;

	interruptible = do_idling(dev_priv);

	intel_gtt_clear_range(obj->gtt_space->start >> PAGE_SHIFT,
	    obj->base.size >> PAGE_SHIFT);

	undo_idling(dev_priv, interruptible);
}
