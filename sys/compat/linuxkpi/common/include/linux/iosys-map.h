/* Public domain. */

#ifndef _LINUXKPI_LINUX_IOSYS_MAP_H
#define _LINUXKPI_LINUX_IOSYS_MAP_H

#include <linux/io.h>
#include <linux/string.h>

struct iosys_map {
	union {
		void *vaddr_iomem;
		void *vaddr;
	};
	bool is_iomem;
#ifdef __OpenBSD__
	bus_space_handle_t bsh;
	bus_size_t size;
#endif
};

#define IOSYS_MAP_INIT_OFFSET(_ism_src_p, _off) ({			\
	struct iosys_map ism_dst = *(_ism_src_p);			\
	iosys_map_incr(&ism_dst, _off);					\
	ism_dst;							\
})

static inline void
iosys_map_incr(struct iosys_map *ism, size_t n)
{
	if (ism->is_iomem)
		ism->vaddr_iomem += n;
	else
		ism->vaddr += n;
}

static inline void
iosys_map_memcpy_to(struct iosys_map *ism, size_t off, const void *src,
    size_t len)
{
	if (ism->is_iomem)
		memcpy_toio(ism->vaddr_iomem + off, src, len);
	else
		memcpy(ism->vaddr + off, src, len);
}

static inline bool
iosys_map_is_null(const struct iosys_map *ism)
{
	if (ism->is_iomem)
		return (ism->vaddr_iomem == NULL);
	else
		return (ism->vaddr == NULL);
}

static inline bool
iosys_map_is_set(const struct iosys_map *ism)
{
	if (ism->is_iomem)
		return (ism->vaddr_iomem != NULL);
	else
		return (ism->vaddr != NULL);
}

static inline bool
iosys_map_is_equal(const struct iosys_map *ism_a,
    const struct iosys_map *ism_b)
{
	if (ism_a->is_iomem != ism_b->is_iomem)
		return (false);

	if (ism_a->is_iomem)
		return (ism_a->vaddr_iomem == ism_b->vaddr_iomem);
	else
		return (ism_a->vaddr == ism_b->vaddr);
}

static inline void
iosys_map_clear(struct iosys_map *ism)
{
	if (ism->is_iomem) {
		ism->vaddr_iomem = NULL;
		ism->is_iomem = false;
	} else {
		ism->vaddr = NULL;
	}
}

static inline void
iosys_map_set_vaddr_iomem(struct iosys_map *ism, void *addr)
{
	ism->vaddr_iomem = addr;
	ism->is_iomem = true;
}

static inline void
iosys_map_set_vaddr(struct iosys_map *ism, void *addr)
{
	ism->vaddr = addr;
	ism->is_iomem = false;
}

static inline void
iosys_map_memset(struct iosys_map *ism, size_t off, int value, size_t len)
{
	if (ism->is_iomem)
		memset_io(ism->vaddr_iomem + off, value, len);
	else
		memset(ism->vaddr + off, value, len);
}

#ifdef __LP64__
#define	_iosys_map_readq(_addr)			readq(_addr)
#define	_iosys_map_writeq(_val, _addr)		writeq(_val, _addr)
#else
#define	_iosys_map_readq(_addr) ({					\
	uint64_t val;							\
	memcpy_fromio(&val, _addr, sizeof(uint64_t));			\
	val;								\
})
#define	_iosys_map_writeq(_val, _addr)					\
	memcpy_toio(_addr, &(_val), sizeof(uint64_t))
#endif

#define	iosys_map_rd(_ism, _off, _type) ({				\
	_type val;							\
	if ((_ism)->is_iomem) {						\
		void *addr = (_ism)->vaddr_iomem + (_off);		\
		val = _Generic(val,					\
		    uint8_t : readb(addr),				\
		    uint16_t: readw(addr),				\
		    uint32_t: readl(addr),				\
		    uint64_t: _iosys_map_readq(addr));			\
	} else								\
		val = READ_ONCE(*(_type *)((_ism)->vaddr + (_off)));	\
	val;								\
})
#define	iosys_map_wr(_ism, _off, _type, _val) ({			\
	_type val = (_val);						\
	if ((_ism)->is_iomem) {						\
		void *addr = (_ism)->vaddr_iomem + (_off);		\
		_Generic(val,						\
		    uint8_t : writeb(val, addr),			\
		    uint16_t: writew(val, addr),			\
		    uint32_t: writel(val, addr),			\
		    uint64_t: _iosys_map_writeq(val, addr));		\
	} else								\
		WRITE_ONCE(*(_type *)((_ism)->vaddr + (_off)), val);	\
})

#define	iosys_map_rd_field(_ism, _off, _type, _field) ({		\
	_type *s;							\
	iosys_map_rd(_ism, (_off) + offsetof(_type, _field),		\
	    __typeof(s->_field));					\
})
#define	iosys_map_wr_field(_ism, _off, _type, _field, _val) ({		\
	_type *s;							\
	iosys_map_wr(_ism, (_off) + offsetof(_type, _field),		\
	    __typeof(s->_field), _val);					\
})

#endif
