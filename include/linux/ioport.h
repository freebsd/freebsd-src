/*
 * ioport.h	Definitions of routines for detecting, reserving and
 *		allocating system resources.
 *
 * Authors:	Linus Torvalds
 */

#ifndef _LINUX_IOPORT_H
#define _LINUX_IOPORT_H

/*
 * Resources are tree-like, allowing
 * nesting etc..
 */
struct resource {
	const char *name;
	unsigned long start, end;
	unsigned long flags;
	struct resource *parent, *sibling, *child;
};

struct resource_list {
	struct resource_list *next;
	struct resource *res;
	struct pci_dev *dev;
};

/*
 * IO resources have these defined flags.
 */
#define IORESOURCE_BITS		0x000000ff	/* Bus-specific bits */

#define IORESOURCE_IO		0x00000100	/* Resource type */
#define IORESOURCE_MEM		0x00000200
#define IORESOURCE_IRQ		0x00000400
#define IORESOURCE_DMA		0x00000800

#define IORESOURCE_PREFETCH	0x00001000	/* No side effects */
#define IORESOURCE_READONLY	0x00002000
#define IORESOURCE_CACHEABLE	0x00004000
#define IORESOURCE_RANGELENGTH	0x00008000
#define IORESOURCE_SHADOWABLE	0x00010000
#define IORESOURCE_BUS_HAS_VGA	0x00080000

#define IORESOURCE_UNSET	0x20000000
#define IORESOURCE_AUTO		0x40000000
#define IORESOURCE_BUSY		0x80000000	/* Driver has marked this resource busy */

/* ISA PnP IRQ specific bits (IORESOURCE_BITS) */
#define IORESOURCE_IRQ_HIGHEDGE		(1<<0)
#define IORESOURCE_IRQ_LOWEDGE		(1<<1)
#define IORESOURCE_IRQ_HIGHLEVEL	(1<<2)
#define IORESOURCE_IRQ_LOWLEVEL		(1<<3)

/* ISA PnP DMA specific bits (IORESOURCE_BITS) */
#define IORESOURCE_DMA_TYPE_MASK	(3<<0)
#define IORESOURCE_DMA_8BIT		(0<<0)
#define IORESOURCE_DMA_8AND16BIT	(1<<0)
#define IORESOURCE_DMA_16BIT		(2<<0)

#define IORESOURCE_DMA_MASTER		(1<<2)
#define IORESOURCE_DMA_BYTE		(1<<3)
#define IORESOURCE_DMA_WORD		(1<<4)

#define IORESOURCE_DMA_SPEED_MASK	(3<<6)
#define IORESOURCE_DMA_COMPATIBLE	(0<<6)
#define IORESOURCE_DMA_TYPEA		(1<<6)
#define IORESOURCE_DMA_TYPEB		(2<<6)
#define IORESOURCE_DMA_TYPEF		(3<<6)

/* ISA PnP memory I/O specific bits (IORESOURCE_BITS) */
#define IORESOURCE_MEM_WRITEABLE	(1<<0)	/* dup: IORESOURCE_READONLY */
#define IORESOURCE_MEM_CACHEABLE	(1<<1)	/* dup: IORESOURCE_CACHEABLE */
#define IORESOURCE_MEM_RANGELENGTH	(1<<2)	/* dup: IORESOURCE_RANGELENGTH */
#define IORESOURCE_MEM_TYPE_MASK	(3<<3)
#define IORESOURCE_MEM_8BIT		(0<<3)
#define IORESOURCE_MEM_16BIT		(1<<3)
#define IORESOURCE_MEM_8AND16BIT	(2<<3)
#define IORESOURCE_MEM_SHADOWABLE	(1<<5)	/* dup: IORESOURCE_SHADOWABLE */
#define IORESOURCE_MEM_EXPANSIONROM	(1<<6)

/* PC/ISA/whatever - the normal PC address spaces: IO and memory */
extern struct resource ioport_resource;
extern struct resource iomem_resource;

extern int check_resource(struct resource *root, unsigned long, unsigned long);
extern int request_resource(struct resource *root, struct resource *new);
extern int release_resource(struct resource *new);
extern int allocate_resource(struct resource *root, struct resource *new,
			     unsigned long size,
			     unsigned long min, unsigned long max,
			     unsigned long align,
			     void (*alignf)(void *, struct resource *,
					    unsigned long, unsigned long),
			     void *alignf_data);

/* Convenience shorthand with allocation */
#define request_region(start,n,name)	__request_region(&ioport_resource, (start), (n), (name))
#define request_mem_region(start,n,name) __request_region(&iomem_resource, (start), (n), (name))

extern struct resource * __request_region(struct resource *, unsigned long start, unsigned long n, const char *name);

/* Compatibility cruft */
#define check_region(start,n)	__check_region(&ioport_resource, (start), (n))
#define release_region(start,n)	__release_region(&ioport_resource, (start), (n))
#define check_mem_region(start,n)	__check_region(&iomem_resource, (start), (n))
#define release_mem_region(start,n)	__release_region(&iomem_resource, (start), (n))

extern int __check_region(struct resource *, unsigned long, unsigned long);
extern void __release_region(struct resource *, unsigned long, unsigned long);

#define HAVE_AUTOIRQ
extern void autoirq_setup(int waittime);
extern int autoirq_report(int waittime);

#endif	/* _LINUX_IOPORT_H */
