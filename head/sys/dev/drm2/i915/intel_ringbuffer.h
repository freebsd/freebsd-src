/*
 * $FreeBSD$
 */

#ifndef _INTEL_RINGBUFFER_H_
#define _INTEL_RINGBUFFER_H_

/*
 * Gen2 BSpec "1. Programming Environment" / 1.4.4.6 "Ring Buffer Use"
 * Gen3 BSpec "vol1c Memory Interface Functions" / 2.3.4.5 "Ring Buffer Use"
 * Gen4+ BSpec "vol1c Memory Interface and Command Stream" / 5.3.4.5 "Ring Buffer Use"
 *
 * "If the Ring Buffer Head Pointer and the Tail Pointer are on the same
 * cacheline, the Head Pointer must not be greater than the Tail
 * Pointer."
 */
#define I915_RING_FREE_SPACE 64

struct  intel_hw_status_page {
	u32		*page_addr;
	unsigned int	gfx_addr;
	struct		drm_i915_gem_object *obj;
};

#define I915_READ_TAIL(ring) I915_READ(RING_TAIL((ring)->mmio_base))
#define I915_WRITE_TAIL(ring, val) I915_WRITE(RING_TAIL((ring)->mmio_base), val)

#define I915_READ_START(ring) I915_READ(RING_START((ring)->mmio_base))
#define I915_WRITE_START(ring, val) I915_WRITE(RING_START((ring)->mmio_base), val)

#define I915_READ_HEAD(ring)  I915_READ(RING_HEAD((ring)->mmio_base))
#define I915_WRITE_HEAD(ring, val) I915_WRITE(RING_HEAD((ring)->mmio_base), val)

#define I915_READ_CTL(ring) I915_READ(RING_CTL((ring)->mmio_base))
#define I915_WRITE_CTL(ring, val) I915_WRITE(RING_CTL((ring)->mmio_base), val)

#define I915_READ_IMR(ring) I915_READ(RING_IMR((ring)->mmio_base))
#define I915_WRITE_IMR(ring, val) I915_WRITE(RING_IMR((ring)->mmio_base), val)

#define I915_READ_NOPID(ring) I915_READ(RING_NOPID((ring)->mmio_base))
#define I915_READ_SYNC_0(ring) I915_READ(RING_SYNC_0((ring)->mmio_base))
#define I915_READ_SYNC_1(ring) I915_READ(RING_SYNC_1((ring)->mmio_base))

struct  intel_ring_buffer {
	const char	*name;
	enum intel_ring_id {
		RCS = 0x0,
		VCS,
		BCS,
	} id;
#define I915_NUM_RINGS 3
	u32		mmio_base;
	void		*virtual_start;
	struct		drm_device *dev;
	struct		drm_i915_gem_object *obj;

	u32		head;
	u32		tail;
	int		space;
	int		size;
	int		effective_size;
	struct intel_hw_status_page status_page;

	/** We track the position of the requests in the ring buffer, and
	 * when each is retired we increment last_retired_head as the GPU
	 * must have finished processing the request and so we know we
	 * can advance the ringbuffer up to that position.
	 *
	 * last_retired_head is set to -1 after the value is consumed so
	 * we can detect new retirements.
	 */
	u32		last_retired_head;

	u32		irq_refcount;		/* protected by dev_priv->irq_lock */
	u32		irq_enable_mask;	/* bitmask to enable ring interrupt */
	u32		trace_irq_seqno;
	u32		sync_seqno[I915_NUM_RINGS-1];
	bool		(*irq_get)(struct intel_ring_buffer *ring);
	void		(*irq_put)(struct intel_ring_buffer *ring);

	int		(*init)(struct intel_ring_buffer *ring);

	void		(*write_tail)(struct intel_ring_buffer *ring,
				      uint32_t value);
	int		(*flush)(struct intel_ring_buffer *ring,
				  uint32_t	invalidate_domains,
				  uint32_t	flush_domains);
	int		(*add_request)(struct intel_ring_buffer *ring,
				       uint32_t *seqno);
	uint32_t	(*get_seqno)(struct intel_ring_buffer *ring);
	int		(*dispatch_execbuffer)(struct intel_ring_buffer *ring,
					       uint32_t offset, uint32_t length);
#define I915_DISPATCH_SECURE 0x1
#define I915_DISPATCH_PINNED 0x2
	void		(*cleanup)(struct intel_ring_buffer *ring);
	int		(*sync_to)(struct intel_ring_buffer *ring,
				   struct intel_ring_buffer *to,
				   u32 seqno);

	u32		semaphore_register[3]; /*our mbox written by others */
	u32		signal_mbox[2]; /* mboxes this ring signals to */
	/**
	 * List of objects currently involved in rendering from the
	 * ringbuffer.
	 *
	 * Includes buffers having the contents of their GPU caches
	 * flushed, not necessarily primitives.  last_rendering_seqno
	 * represents when the rendering involved will be completed.
	 *
	 * A reference is held on the buffer while on this list.
	 */
	struct list_head active_list;

	/**
	 * List of breadcrumbs associated with GPU requests currently
	 * outstanding.
	 */
	struct list_head request_list;

	/**
	 * List of objects currently pending a GPU write flush.
	 *
	 * All elements on this list will belong to either the
	 * active_list or flushing_list, last_rendering_seqno can
	 * be used to differentiate between the two elements.
	 */
	struct list_head gpu_write_list;

	/**
	 * Do we have some not yet emitted requests outstanding?
	 */
	u32 outstanding_lazy_request;

	/**
	 * Do an explicit TLB flush before MI_SET_CONTEXT
	 */
	bool itlb_before_ctx_switch;
	struct i915_hw_context *default_context;
	struct drm_i915_gem_object *last_context_obj;

	drm_local_map_t map;

	void *private;
};

static inline bool
intel_ring_initialized(struct intel_ring_buffer *ring)
{
	return ring->obj != NULL;
}

static inline unsigned
intel_ring_flag(struct intel_ring_buffer *ring)
{
	return 1 << ring->id;
}

static inline uint32_t
intel_ring_sync_index(struct intel_ring_buffer *ring,
		      struct intel_ring_buffer *other)
{
	int idx;

	/*
	 * cs -> 0 = vcs, 1 = bcs
	 * vcs -> 0 = bcs, 1 = cs,
	 * bcs -> 0 = cs, 1 = vcs.
	 */

	idx = (other - ring) - 1;
	if (idx < 0)
		idx += I915_NUM_RINGS;

	return idx;
}

static inline u32
intel_read_status_page(struct intel_ring_buffer *ring,
		       int reg)
{
	/* Ensure that the compiler doesn't optimize away the load. */
	__compiler_membar();
	return (atomic_load_acq_32(ring->status_page.page_addr + reg));
}

void intel_cleanup_ring_buffer(struct intel_ring_buffer *ring);

int intel_wait_ring_buffer(struct intel_ring_buffer *ring, int n);
static inline int intel_wait_ring_idle(struct intel_ring_buffer *ring)
{

	return (intel_wait_ring_buffer(ring, ring->size - 8));
}

int intel_ring_begin(struct intel_ring_buffer *ring, int n);

static inline void intel_ring_emit(struct intel_ring_buffer *ring,
				   u32 data)
{
	*(volatile uint32_t *)((char *)ring->virtual_start +
	    ring->tail) = data;
	ring->tail += 4;
}

void intel_ring_advance(struct intel_ring_buffer *ring);

uint32_t intel_ring_get_seqno(struct intel_ring_buffer *ring);

int intel_init_render_ring_buffer(struct drm_device *dev);
int intel_init_bsd_ring_buffer(struct drm_device *dev);
int intel_init_blt_ring_buffer(struct drm_device *dev);

u32 intel_ring_get_active_head(struct intel_ring_buffer *ring);
void intel_ring_setup_status_page(struct intel_ring_buffer *ring);

static inline u32 intel_ring_get_tail(struct intel_ring_buffer *ring)
{
	return ring->tail;
}

void i915_trace_irq_get(struct intel_ring_buffer *ring, uint32_t seqno);

/* DRI warts */
int intel_render_ring_init_dri(struct drm_device *dev, uint64_t start,
    uint32_t size);

#endif /* _INTEL_RINGBUFFER_H_ */
