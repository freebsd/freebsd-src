/*
 * Copyright (c) 2008 Mayur Shardul <mayur.shardul@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _VM_RADIX_H_
#define _VM_RADIX_H_

#include <sys/queue.h>

/* Default values of the tree parameters */
#define	VM_RADIX_WIDTH	5
#define	VM_RADIX_COUNT	(1 << VM_RADIX_WIDTH)
#define	VM_RADIX_MASK	(VM_RADIX_COUNT - 1)
#define	VM_RADIX_LIMIT	howmany((sizeof(vm_pindex_t) * NBBY), VM_RADIX_WIDTH)
#define	VM_RADIX_FLAGS	0x3	/* Flag bits stored in node pointers. */
#define	VM_RADIX_BLACK	0x1	/* Black node. (leaf only) */
#define	VM_RADIX_RED	0x2	/* Red node. (leaf only) */
#define	VM_RADIX_ANY	(VM_RADIX_RED | VM_RADIX_BLACK)
#define	VM_RADIX_EMPTY	0x1	/* Empty hint. (internal only) */
#define	VM_RADIX_HEIGHT	0xf	/* Bits of height in root */
#define	VM_RADIX_STACK	8	/* Nodes to store on stack. */

CTASSERT(VM_RADIX_HEIGHT >= VM_RADIX_LIMIT);

/* Calculates maximum value for a tree of height h. */
#define	VM_RADIX_MAX(h)							\
	    ((h) == VM_RADIX_LIMIT ? ((vm_pindex_t)-1) :		\
	    (((vm_pindex_t)1 << ((h) * VM_RADIX_WIDTH)) - 1))

struct vm_radix_node {
	void		*rn_child[VM_RADIX_COUNT];	/* child nodes. */
    	uint16_t	rn_count;			/* Valid children. */
};

/*
 * Radix tree root.  The height and pointer are set together to permit
 * coherent lookups while the root is modified.
 */
struct vm_radix {
	uintptr_t	rt_root;		/* root + height */
};

void	vm_radix_init(void);

/*
 * Functions which only work with black nodes. (object lock)
 */
int 	vm_radix_insert(struct vm_radix *, vm_pindex_t, void *);
void 	vm_radix_shrink(struct vm_radix *);

/*
 * Functions which work on specified colors. (object, vm_page_queue_free locks)
 */
void	*vm_radix_color(struct vm_radix *, vm_pindex_t, int);
void	*vm_radix_lookup(struct vm_radix *, vm_pindex_t, int);
int	vm_radix_lookupn(struct vm_radix *, vm_pindex_t, vm_pindex_t, int,
	    void **, int, vm_pindex_t *);
void	*vm_radix_lookup_le(struct vm_radix *, vm_pindex_t, int);
void	*vm_radix_remove(struct vm_radix *, vm_pindex_t, int);
void	vm_radix_foreach(struct vm_radix *, vm_pindex_t, vm_pindex_t, int,
	    void (*)(void *));

/*
 * Look up any entry at a position greater or equal to index.
 */
static inline void *
vm_radix_lookup_ge(struct vm_radix *rtree, vm_pindex_t index, int color)
{
        void *val;

        if (vm_radix_lookupn(rtree, index, 0, color, &val, 1, &index))
                return (val);
        return (NULL);
}

#endif /* !_VM_RADIX_H_ */
