/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#ifndef	_LINUX_RADIX_TREE_H_
#define	_LINUX_RADIX_TREE_H_

#define	RADIX_TREE_MAP_SHIFT	6
#define	RADIX_TREE_MAP_SIZE	(1 << RADIX_TREE_MAP_SHIFT)
#define	RADIX_TREE_MAP_MASK	(RADIX_TREE_MAP_SIZE - 1)
#define	RADIX_TREE_MAX_HEIGHT						\
	    DIV_ROUND_UP((sizeof(long) * NBBY), RADIX_TREE_MAP_SHIFT)

struct radix_tree_node {
	void		*slots[RADIX_TREE_MAP_SIZE];
	int		count;
};

struct radix_tree_root {
	struct radix_tree_node	*rnode;
	gfp_t			gfp_mask;
	int			height;
};

#define	RADIX_TREE_INIT(mask)						\
	    { .rnode = NULL, .gfp_mask = mask, .height = 0 };
#define	INIT_RADIX_TREE(root, mask)					\
	    { (root)->rnode = NULL; (root)->gfp_mask = mask; (root)->height = 0; }
#define	RADIX_TREE(name, mask)						\
	    struct radix_tree_root name = RADIX_TREE_INIT(mask)

void	*radix_tree_lookup(struct radix_tree_root *, unsigned long);
void	*radix_tree_delete(struct radix_tree_root *, unsigned long);
int	radix_tree_insert(struct radix_tree_root *, unsigned long, void *);

#endif	/* _LINUX_RADIX_TREE_H_ */
