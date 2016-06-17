/*======================================================================

    Resource management routines

    rsrc_mgr.c 1.79 2000/08/30 20:23:58

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU General Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.
    
======================================================================*/

#define __NO_VERSION__

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>
#include <pcmcia/cs.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>
#include "cs_internal.h"

/*====================================================================*/

/* Parameters that can be set with 'insmod' */

#define INT_MODULE_PARM(n, v) static int n = v; MODULE_PARM(n, "i")

INT_MODULE_PARM(probe_mem,	1);		/* memory probe? */
#ifdef CONFIG_ISA
INT_MODULE_PARM(probe_io,	1);		/* IO port probe? */
INT_MODULE_PARM(mem_limit,	0x10000);
#endif

/*======================================================================

    The resource_map_t structures are used to track what resources are
    available for allocation for PC Card devices.

======================================================================*/

typedef struct resource_map_t {
    u_long			base, num;
    struct resource_map_t	*next;
} resource_map_t;

/* Memory resource database */
static resource_map_t mem_db = { 0, 0, &mem_db };

/* IO port resource database */
static resource_map_t io_db = { 0, 0, &io_db };

#ifdef CONFIG_ISA

typedef struct irq_info_t {
    u_int			Attributes;
    int				time_share, dyn_share;
    struct socket_info_t	*Socket;
} irq_info_t;

/* Table of IRQ assignments */
static irq_info_t irq_table[NR_IRQS] = { { 0, 0, 0 }, /* etc */ };

#endif

/*======================================================================

    Linux resource management extensions

======================================================================*/

static struct resource *resource_parent(unsigned long b, unsigned long n,
					int flags, struct pci_dev *dev)
{
#ifdef CONFIG_PCI
	struct resource res, *pr;

	if (dev != NULL) {
		res.start = b;
		res.end = b + n - 1;
		res.flags = flags;
		pr = pci_find_parent_resource(dev, &res);
		if (pr)
			return pr;
	}
#endif /* CONFIG_PCI */
	if (flags & IORESOURCE_MEM)
		return &iomem_resource;
	return &ioport_resource;
}

static inline int check_io_resource(unsigned long b, unsigned long n,
				    struct pci_dev *dev)
{
	return check_resource(resource_parent(b, n, IORESOURCE_IO, dev), b, n);
}

static inline int check_mem_resource(unsigned long b, unsigned long n,
				     struct pci_dev *dev)
{
	return check_resource(resource_parent(b, n, IORESOURCE_MEM, dev), b, n);
}

static struct resource *make_resource(unsigned long b, unsigned long n,
				      int flags, char *name)
{
	struct resource *res = kmalloc(sizeof(*res), GFP_KERNEL);

	if (res) {
		memset(res, 0, sizeof(*res));
		res->name = name;
		res->start = b;
		res->end = b + n - 1;
		res->flags = flags | IORESOURCE_BUSY;
	}
	return res;
}

static int request_io_resource(unsigned long b, unsigned long n,
			       char *name, struct pci_dev *dev)
{
	struct resource *res = make_resource(b, n, IORESOURCE_IO, name);
	struct resource *pr = resource_parent(b, n, IORESOURCE_IO, dev);
	int err = -ENOMEM;

	if (res) {
		err = request_resource(pr, res);
		if (err)
			kfree(res);
	}
	return err;
}

static int request_mem_resource(unsigned long b, unsigned long n,
				char *name, struct pci_dev *dev)
{
	struct resource *res = make_resource(b, n, IORESOURCE_MEM, name);
	struct resource *pr = resource_parent(b, n, IORESOURCE_MEM, dev);
	int err = -ENOMEM;

	if (res) {
		err = request_resource(pr, res);
		if (err)
			kfree(res);
	}
	return err;
}

/*======================================================================

    These manage the internal databases of available resources.
    
======================================================================*/

static int add_interval(resource_map_t *map, u_long base, u_long num)
{
    resource_map_t *p, *q;

    for (p = map; ; p = p->next) {
	if ((p != map) && (p->base+p->num-1 >= base))
	    return -1;
	if ((p->next == map) || (p->next->base > base+num-1))
	    break;
    }
    q = kmalloc(sizeof(resource_map_t), GFP_KERNEL);
    if (!q) return CS_OUT_OF_RESOURCE;
    q->base = base; q->num = num;
    q->next = p->next; p->next = q;
    return CS_SUCCESS;
}

/*====================================================================*/

static int sub_interval(resource_map_t *map, u_long base, u_long num)
{
    resource_map_t *p, *q;

    for (p = map; ; p = q) {
	q = p->next;
	if (q == map)
	    break;
	if ((q->base+q->num > base) && (base+num > q->base)) {
	    if (q->base >= base) {
		if (q->base+q->num <= base+num) {
		    /* Delete whole block */
		    p->next = q->next;
		    kfree(q);
		    /* don't advance the pointer yet */
		    q = p;
		} else {
		    /* Cut off bit from the front */
		    q->num = q->base + q->num - base - num;
		    q->base = base + num;
		}
	    } else if (q->base+q->num <= base+num) {
		/* Cut off bit from the end */
		q->num = base - q->base;
	    } else {
		/* Split the block into two pieces */
		p = kmalloc(sizeof(resource_map_t), GFP_KERNEL);
		if (!p) return CS_OUT_OF_RESOURCE;
		p->base = base+num;
		p->num = q->base+q->num - p->base;
		q->num = base - q->base;
		p->next = q->next ; q->next = p;
	    }
	}
    }
    return CS_SUCCESS;
}

/*======================================================================

    These routines examine a region of IO or memory addresses to
    determine what ranges might be genuinely available.
    
======================================================================*/

#ifdef CONFIG_ISA
static void do_io_probe(ioaddr_t base, ioaddr_t num)
{
    
    ioaddr_t i, j, bad, any;
    u_char *b, hole, most;
    
    printk(KERN_INFO "cs: IO port probe 0x%04x-0x%04x:",
	   base, base+num-1);
    
    /* First, what does a floating port look like? */
    b = kmalloc(256, GFP_KERNEL);
    if (!b) {
            printk(KERN_ERR "do_io_probe: unable to kmalloc 256 bytes");
            return;
    }   
    memset(b, 0, 256);
    for (i = base, most = 0; i < base+num; i += 8) {
	if (check_io_resource(i, 8, NULL))
	    continue;
	hole = inb(i);
	for (j = 1; j < 8; j++)
	    if (inb(i+j) != hole) break;
	if ((j == 8) && (++b[hole] > b[most]))
	    most = hole;
	if (b[most] == 127) break;
    }
    kfree(b);

    bad = any = 0;
    for (i = base; i < base+num; i += 8) {
	if (check_io_resource(i, 8, NULL))
	    continue;
	for (j = 0; j < 8; j++)
	    if (inb(i+j) != most) break;
	if (j < 8) {
	    if (!any)
		printk(" excluding");
	    if (!bad)
		bad = any = i;
	} else {
	    if (bad) {
		sub_interval(&io_db, bad, i-bad);
		printk(" %#04x-%#04x", bad, i-1);
		bad = 0;
	    }
	}
    }
    if (bad) {
	if ((num > 16) && (bad == base) && (i == base+num)) {
	    printk(" nothing: probe failed.\n");
	    return;
	} else {
	    sub_interval(&io_db, bad, i-bad);
	    printk(" %#04x-%#04x", bad, i-1);
	}
    }
    
    printk(any ? "\n" : " clean.\n");
}
#endif

/*======================================================================

    The memory probe.  If the memory list includes a 64K-aligned block
    below 1MB, we probe in 64K chunks, and as soon as we accumulate at
    least mem_limit free space, we quit.
    
======================================================================*/

static int do_mem_probe(u_long base, u_long num,
			int (*is_valid)(u_long), int (*do_cksum)(u_long),
			socket_info_t *s)
{
    u_long i, j, bad, fail, step;

    printk(KERN_INFO "cs: memory probe 0x%06lx-0x%06lx:",
	   base, base+num-1);
    bad = fail = 0;
    step = (num < 0x20000) ? 0x2000 : ((num>>4) & ~0x1fff);
    for (i = j = base; i < base+num; i = j + step) {
	if (!fail) {	
	    for (j = i; j < base+num; j += step)
		if ((check_mem_resource(j, step, s->cap.cb_dev) == 0) &&
		    is_valid(j))
		    break;
	    fail = ((i == base) && (j == base+num));
	}
	if (fail) {
	    for (j = i; j < base+num; j += 2*step)
		if ((check_mem_resource(j, 2*step, s->cap.cb_dev) == 0) &&
		    do_cksum(j) && do_cksum(j+step))
		    break;
	}
	if (i != j) {
	    if (!bad) printk(" excluding");
	    printk(" %#05lx-%#05lx", i, j-1);
	    sub_interval(&mem_db, i, j-i);
	    bad += j-i;
	}
    }
    printk(bad ? "\n" : " clean.\n");
    return (num - bad);
}

#ifdef CONFIG_ISA

static u_long inv_probe(int (*is_valid)(u_long),
			int (*do_cksum)(u_long),
			resource_map_t *m, socket_info_t *s)
{
    u_long ok;
    if (m == &mem_db)
	return 0;
    ok = inv_probe(is_valid, do_cksum, m->next, s);
    if (ok) {
	if (m->base >= 0x100000)
	    sub_interval(&mem_db, m->base, m->num);
	return ok;
    }
    if (m->base < 0x100000)
	return 0;
    return do_mem_probe(m->base, m->num, is_valid, do_cksum, s);
}

void validate_mem(int (*is_valid)(u_long), int (*do_cksum)(u_long),
		  int force_low, socket_info_t *s)
{
    resource_map_t *m, mm;
    static u_char order[] = { 0xd0, 0xe0, 0xc0, 0xf0 };
    static int hi = 0, lo = 0;
    u_long b, i, ok = 0;
    
    if (!probe_mem) return;
    /* We do up to four passes through the list */
    if (!force_low) {
	if (hi++ || (inv_probe(is_valid, do_cksum, mem_db.next, s) > 0))
	    return;
	printk(KERN_NOTICE "cs: warning: no high memory space "
	       "available!\n");
    }
    if (lo++) return;
    for (m = mem_db.next; m != &mem_db; m = mm.next) {
	mm = *m;
	/* Only probe < 1 MB */
	if (mm.base >= 0x100000) continue;
	if ((mm.base | mm.num) & 0xffff) {
	    ok += do_mem_probe(mm.base, mm.num, is_valid, do_cksum, s);
	    continue;
	}
	/* Special probe for 64K-aligned block */
	for (i = 0; i < 4; i++) {
	    b = order[i] << 12;
	    if ((b >= mm.base) && (b+0x10000 <= mm.base+mm.num)) {
		if (ok >= mem_limit)
		    sub_interval(&mem_db, b, 0x10000);
		else
		    ok += do_mem_probe(b, 0x10000, is_valid, do_cksum, s);
	    }
	}
    }
}

#else /* CONFIG_ISA */

void validate_mem(int (*is_valid)(u_long), int (*do_cksum)(u_long),
		  int force_low, socket_info_t *s)
{
    resource_map_t *m, *n;
    static int done = 0;
    
    if (!probe_mem || done++)
	return;

    for (m = mem_db.next; m != &mem_db; m = n) {
	n = m->next;
	if (do_mem_probe(m->base, m->num, is_valid, do_cksum, s))
	    return;
    }
}

#endif /* CONFIG_ISA */

/*======================================================================

    These find ranges of I/O ports or memory addresses that are not
    currently allocated by other devices.

    The 'align' field should reflect the number of bits of address
    that need to be preserved from the initial value of *base.  It
    should be a power of two, greater than or equal to 'num'.  A value
    of 0 means that all bits of *base are significant.  *base should
    also be strictly less than 'align'.
    
======================================================================*/

int find_io_region(ioaddr_t *base, ioaddr_t num, ioaddr_t align,
		   char *name, socket_info_t *s)
{
    ioaddr_t try;
    resource_map_t *m;
    
    for (m = io_db.next; m != &io_db; m = m->next) {
	try = (m->base & ~(align-1)) + *base;
	for (try = (try >= m->base) ? try : try+align;
	     (try >= m->base) && (try+num <= m->base+m->num);
	     try += align) {
	    if (request_io_resource(try, num, name, s->cap.cb_dev) == 0) {
		*base = try;
		return 0;
	    }
	    if (!align) break;
	}
    }
    return -1;
}

int find_mem_region(u_long *base, u_long num, u_long align,
		    int force_low, char *name, socket_info_t *s)
{
    u_long try;
    resource_map_t *m;

    while (1) {
	for (m = mem_db.next; m != &mem_db; m = m->next) {
	    /* first pass >1MB, second pass <1MB */
	    if ((force_low != 0) ^ (m->base < 0x100000)) continue;
	    try = (m->base & ~(align-1)) + *base;
	    for (try = (try >= m->base) ? try : try+align;
		 (try >= m->base) && (try+num <= m->base+m->num);
		 try += align) {
		if (request_mem_resource(try, num, name, s->cap.cb_dev) == 0) {
		    *base = try;
		    return 0;
		}
		if (!align) break;
	    }
	}
	if (force_low) break;
	force_low++;
    }
    return -1;
}

/*======================================================================

    This checks to see if an interrupt is available, with support
    for interrupt sharing.  We don't support reserving interrupts
    yet.  If the interrupt is available, we allocate it.
    
======================================================================*/

#ifdef CONFIG_ISA

static void fake_irq(int i, void *d, struct pt_regs *r) { }
static inline int check_irq(int irq)
{
    if (request_irq(irq, fake_irq, 0, "bogus", NULL) != 0)
	return -1;
    free_irq(irq, NULL);
    return 0;
}

int try_irq(u_int Attributes, int irq, int specific)
{
    irq_info_t *info = &irq_table[irq];
    if (info->Attributes & RES_ALLOCATED) {
	switch (Attributes & IRQ_TYPE) {
	case IRQ_TYPE_EXCLUSIVE:
	    return CS_IN_USE;
	case IRQ_TYPE_TIME:
	    if ((info->Attributes & RES_IRQ_TYPE)
		!= RES_IRQ_TYPE_TIME)
		return CS_IN_USE;
	    if (Attributes & IRQ_FIRST_SHARED)
		return CS_BAD_ATTRIBUTE;
	    info->Attributes |= RES_IRQ_TYPE_TIME | RES_ALLOCATED;
	    info->time_share++;
	    break;
	case IRQ_TYPE_DYNAMIC_SHARING:
	    if ((info->Attributes & RES_IRQ_TYPE)
		!= RES_IRQ_TYPE_DYNAMIC)
		return CS_IN_USE;
	    if (Attributes & IRQ_FIRST_SHARED)
		return CS_BAD_ATTRIBUTE;
	    info->Attributes |= RES_IRQ_TYPE_DYNAMIC | RES_ALLOCATED;
	    info->dyn_share++;
	    break;
	}
    } else {
	if ((info->Attributes & RES_RESERVED) && !specific)
	    return CS_IN_USE;
	if (check_irq(irq) != 0)
	    return CS_IN_USE;
	switch (Attributes & IRQ_TYPE) {
	case IRQ_TYPE_EXCLUSIVE:
	    info->Attributes |= RES_ALLOCATED;
	    break;
	case IRQ_TYPE_TIME:
	    if (!(Attributes & IRQ_FIRST_SHARED))
		return CS_BAD_ATTRIBUTE;
	    info->Attributes |= RES_IRQ_TYPE_TIME | RES_ALLOCATED;
	    info->time_share = 1;
	    break;
	case IRQ_TYPE_DYNAMIC_SHARING:
	    if (!(Attributes & IRQ_FIRST_SHARED))
		return CS_BAD_ATTRIBUTE;
	    info->Attributes |= RES_IRQ_TYPE_DYNAMIC | RES_ALLOCATED;
	    info->dyn_share = 1;
	    break;
	}
    }
    return 0;
}

#endif

/*====================================================================*/

#ifdef CONFIG_ISA

void undo_irq(u_int Attributes, int irq)
{
    irq_info_t *info;

    info = &irq_table[irq];
    switch (Attributes & IRQ_TYPE) {
    case IRQ_TYPE_EXCLUSIVE:
	info->Attributes &= RES_RESERVED;
	break;
    case IRQ_TYPE_TIME:
	info->time_share--;
	if (info->time_share == 0)
	    info->Attributes &= RES_RESERVED;
	break;
    case IRQ_TYPE_DYNAMIC_SHARING:
	info->dyn_share--;
	if (info->dyn_share == 0)
	    info->Attributes &= RES_RESERVED;
	break;
    }
}

#endif

/*======================================================================

    The various adjust_* calls form the external interface to the
    resource database.
    
======================================================================*/

static int adjust_memory(adjust_t *adj)
{
    u_long base, num;
    int i, ret;

    base = adj->resource.memory.Base;
    num = adj->resource.memory.Size;
    if ((num == 0) || (base+num-1 < base))
	return CS_BAD_SIZE;

    ret = CS_SUCCESS;
    switch (adj->Action) {
    case ADD_MANAGED_RESOURCE:
	ret = add_interval(&mem_db, base, num);
	break;
    case REMOVE_MANAGED_RESOURCE:
	ret = sub_interval(&mem_db, base, num);
	if (ret == CS_SUCCESS) {
	    for (i = 0; i < sockets; i++) {
		release_cis_mem(socket_table[i]);
#ifdef CONFIG_CARDBUS
		cb_release_cis_mem(socket_table[i]);
#endif
	    }
	}
	break;
    default:
	ret = CS_UNSUPPORTED_FUNCTION;
    }
    
    return ret;
}

/*====================================================================*/

static int adjust_io(adjust_t *adj)
{
    int base, num;
    
    base = adj->resource.io.BasePort;
    num = adj->resource.io.NumPorts;
    if ((base < 0) || (base > 0xffff))
	return CS_BAD_BASE;
    if ((num <= 0) || (base+num > 0x10000) || (base+num <= base))
	return CS_BAD_SIZE;

    switch (adj->Action) {
    case ADD_MANAGED_RESOURCE:
	if (add_interval(&io_db, base, num) != 0)
	    return CS_IN_USE;
#ifdef CONFIG_ISA
	if (probe_io)
	    do_io_probe(base, num);
#endif
	break;
    case REMOVE_MANAGED_RESOURCE:
	sub_interval(&io_db, base, num);
	break;
    default:
	return CS_UNSUPPORTED_FUNCTION;
	break;
    }

    return CS_SUCCESS;
}

/*====================================================================*/

static int adjust_irq(adjust_t *adj)
{
#ifdef CONFIG_ISA
    int irq;
    irq_info_t *info;
    
    irq = adj->resource.irq.IRQ;
    if ((irq < 0) || (irq > 15))
	return CS_BAD_IRQ;
    info = &irq_table[irq];
    
    switch (adj->Action) {
    case ADD_MANAGED_RESOURCE:
	if (info->Attributes & RES_REMOVED)
	    info->Attributes &= ~(RES_REMOVED|RES_ALLOCATED);
	else
	    if (adj->Attributes & RES_ALLOCATED)
		return CS_IN_USE;
	if (adj->Attributes & RES_RESERVED)
	    info->Attributes |= RES_RESERVED;
	else
	    info->Attributes &= ~RES_RESERVED;
	break;
    case REMOVE_MANAGED_RESOURCE:
	if (info->Attributes & RES_REMOVED)
	    return 0;
	if (info->Attributes & RES_ALLOCATED)
	    return CS_IN_USE;
	info->Attributes |= RES_ALLOCATED|RES_REMOVED;
	info->Attributes &= ~RES_RESERVED;
	break;
    default:
	return CS_UNSUPPORTED_FUNCTION;
	break;
    }
#endif
    return CS_SUCCESS;
}

/*====================================================================*/

int pcmcia_adjust_resource_info(client_handle_t handle, adjust_t *adj)
{
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    
    switch (adj->Resource) {
    case RES_MEMORY_RANGE:
	return adjust_memory(adj);
	break;
    case RES_IO_RANGE:
	return adjust_io(adj);
	break;
    case RES_IRQ:
	return adjust_irq(adj);
	break;
    }
    return CS_UNSUPPORTED_FUNCTION;
}

/*====================================================================*/

void release_resource_db(void)
{
    resource_map_t *p, *q;
    
    for (p = mem_db.next; p != &mem_db; p = q) {
	q = p->next;
	kfree(p);
    }
    for (p = io_db.next; p != &io_db; p = q) {
	q = p->next;
	kfree(p);
    }
}
